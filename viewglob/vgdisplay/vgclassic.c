/*
	Copyright (C) 2004, 2005 Stephen Bach
	This file is part of the Viewglob package.

	Viewglob is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Viewglob is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Viewglob; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"

#include "common.h"
#include "file_box.h"
#include "dlisting.h"
#include "exhibit.h"
#include "param-io.h"
#include "vgclassic.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <string.h>       /* For strcmp. */
#include <unistd.h>       /* For getopt. */
#include <stdio.h>        /* For BUFSIZ. */


/* Prototypes. */
static gboolean receive_data(GIOChannel* source, GIOCondition condition,
		gpointer data);
static GString*  read_string(const gchar* buf, gsize* start, gsize n,
		gchar delim, struct holdover* ho, gboolean* finished);
static void      write_xwindow_id(GtkWidget* gtk_window);

static void        set_icons(void);

static gboolean  parse_args(int argc, char** argv);
static void      report_version(void);

static void  process_glob_data(const gchar* buf, gsize bytes, Exhibit* e);

static FileSelection  map_selection_state(const GString* string);
static FileType       map_file_type(const GString* string);

static gboolean  window_delete_event(GtkWidget* widget, GdkEvent* event,
		gpointer data);
static gboolean  window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, Exhibit* e);
static void      window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, Exhibit* e);
static gboolean  window_key_press_event(GtkWidget* window,
		GdkEventKey* event, gpointer data);

/* Globals. */
struct viewable_preferences v;


/* Chooses a selection state based on the string's first char. */
static FileSelection map_selection_state(const GString* string) {
	switch (*string->str) {
		case '-':
			return FS_NO;
		case '~':
			return FS_MAYBE;
		case '*':
			return FS_YES;
		default:
			g_warning("Unexpected selection state \"%c\".", *string->str);
			return FS_NO;
	}
}


/* Chooses a file type based on the string's first char. */
static FileType map_file_type(const GString* string) {
	switch (*string->str) {
		case 'r':
			return FT_REGULAR;
		case 'e':
			return FT_EXECUTABLE;
		case 'd':
			return FT_DIRECTORY;
		case 'y':
			return FT_SYMLINK;
		case 'b':
			return FT_BLOCKDEV;
		case 'c':
			return FT_CHARDEV;
		case 'f':
			return FT_FIFO;
		case 's':
			return FT_SOCKET;
		default:
			g_warning("Unexpected file type \"%c\".", *string->str);
			return FT_REGULAR;
	}
}


/* Try to get a string from the given buffer.  If delim is not seen, save the
   string for the next call (combining with whatever is already saved) in ho.
   When delim is seen, return the completed string. */
static GString* read_string(const gchar* buf, gsize* start, gsize n,
		gchar delim, struct holdover* ho, gboolean* finished) {

	GString* string = NULL;
	gsize i;
	gboolean delim_reached = FALSE;

	for (i = *start; i < n; i++) {
		if ( buf[i] == delim ) {
			delim_reached = TRUE;
			break;
		}
	}
	*finished = delim_reached;

	if (delim_reached) {
		string = g_string_new_len(buf + *start, i - *start);
		if (ho->has_holdover) {
			g_string_prepend(string, ho->string->str);
			ho->has_holdover = FALSE;
		}
	}
	else {
		if (ho->has_holdover)
			g_string_append_len(ho->string, buf + *start, i - *start);
		else {
			//FIXME \/ shouldn't it be !ho->string?
			if (!ho->has_holdover)   /* This will only be true once. */
				ho->string = g_string_new_len(buf + *start, i - *start);
			else {
				g_string_truncate(ho->string, 0);
				g_string_append_len(ho->string, buf + *start, i - *start);
			}

			ho->has_holdover = TRUE;
		}
	}

	*start = i;
	return string;
}


static gboolean window_delete_event(GtkWidget* widget, GdkEvent* event,
		gpointer data) {
	gtk_main_quit();
	return FALSE;
}


/* Attempt to read size bytes into buf from source. */
static gboolean receive_data(GIOChannel* source, GIOCondition condition,
		gpointer data) {

	enum parameter param;
	gchar* value;

	Exhibit* e = data;
	int fd = g_io_channel_unix_get_fd(source);
	
	if (get_param(fd, &param, &value)) {
		switch (param) {

			case P_ORDER:
				g_warning("P_ORDER: %s", value);
				exhibit_do_order(e, value);
				break;

			case P_WIN_ID:
				g_warning("P_WIN_ID: %s", value);
				break;

			case P_CMD:
				g_warning("P_CMD: %s", value);
				// TODO wrap into exhibit_blah() function */
				gchar* cmdline_utf8 = g_filename_to_utf8(value,
						strlen(value), NULL, NULL, NULL);
				gtk_entry_set_text(GTK_ENTRY(e->cmdline), cmdline_utf8);
				g_free(cmdline_utf8);
				break;

			case P_MASK:
				g_warning("P_MASK: %s", value);
				break;

			case P_DEVELOPING_MASK:
				g_warning("P_DEVELOPING_MASK: %s", value);
				break;

			case P_STATUS:
				g_warning("P_STATUS: %s", value);
				break;

			case P_PWD:
				//FIXME don't need to know pwd, do we?
				g_warning("P_PWD: %s", value);
				break;

			case P_VGEXPAND_DATA:
				g_warning("P_VGEXPAND_DATA: (lots)");
				process_glob_data(value, strlen(value), e);
				break;

			case P_EOF:
				g_warning("EOF");
				exit(EXIT_SUCCESS);
				/*break;*/

			default:
				g_critical("Unexpected parameter from vgd: %d = %s", param,
						value);
				exit(EXIT_FAILURE);
				/*break;*/
		}
	}
	else {
		g_critical("Could not receive data form vgd");
		exit(EXIT_FAILURE);
	}

	return TRUE;
}


/* Finite state machine to interpret glob data. */
/* TODO: instead of freeing string after each read, try to use it correctly. */
static void process_glob_data(const gchar* buf, gsize bytes, Exhibit* e) {

	/* These variables are all static because they may need
	   to be preserved between calls to this function (in
	   the case that buf is not the whole input).  */
	static enum glob_read_state rs = GRS_DONE;
	static gboolean advance = FALSE;

	static struct holdover ho = { NULL, FALSE };

	static GString* selected_count;
	static GString* total_count;
	static GString* hidden_count;

	static FileType type;
	static FileSelection selection;

	static gint dir_rank = 0;
	static DListing* dl;

	GString* string = NULL;
	gboolean completed = FALSE;

	gsize pos = 0;

	while (pos < bytes) {

		/* Skip the next character (the delimiter). */
		if (advance) {
			pos++;
			advance = FALSE;
			continue;
		}

		switch (rs) {
			case GRS_DONE:
				dir_rank = 0;
				exhibit_unmark_all(e);
				rs = GRS_SELECTED_COUNT;
				break;

			case GRS_SELECTED_COUNT:
				selected_count = read_string(buf, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					dir_rank++;
					rs = GRS_FILE_COUNT;
					advance = TRUE;
				}
				break;

			case GRS_FILE_COUNT:
				total_count = read_string(buf, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					rs = GRS_HIDDEN_COUNT;
					advance = TRUE;
				}
				break;

			case GRS_HIDDEN_COUNT:
				hidden_count = read_string(buf, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					rs = GRS_DIR_NAME;
					advance = TRUE;
				}
				break;

			/* Get dl, the DListing we're currently working on, from here. */
			case GRS_DIR_NAME:
				string = read_string(buf, &pos, bytes, '\n', &ho, &completed);
				if (completed) {

					dl = exhibit_add(e, string, dir_rank, selected_count, total_count, hidden_count);

					if (dir_rank == 1) {
						/* Put pwd into the window title. */
						char* title = g_strconcat("vg ", string->str, NULL);
						gtk_window_set_title(GTK_WINDOW(e->window), title);
						g_free(title);
					}

					g_string_free(selected_count, TRUE);
					g_string_free(total_count, TRUE);
					g_string_free(hidden_count, TRUE);

					advance = TRUE;
					rs = GRS_IN_LIMBO;
				}
				break;

			/* Either we'll read another FItem (or the first), a new DListing, or EOF (double \n). */
			case GRS_IN_LIMBO:
				completed = FALSE;
				if ( *(buf + pos) == '\t' ) {        /* Another FItem. */
					advance = TRUE;
					rs = GRS_FILE_STATE;
				}
				else if ( *(buf + pos) == '\n' ) {   /* End of glob-expand data. */
					advance = TRUE;
					rs = GRS_DONE;
				}
				else {                                /* Another DListing. */
					/* (No need to advance) */
					rs = GRS_SELECTED_COUNT;
				}

				break;


			/* Have to save file_state and file_type until we get file_name */
			case GRS_FILE_STATE:
				string = read_string(buf, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					selection = map_selection_state(string);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = GRS_FILE_TYPE;
				}
				break;

			case GRS_FILE_TYPE:
				string = read_string(buf, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					type = map_file_type(string);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = GRS_FILE_NAME;
				}
				break;

			case GRS_FILE_NAME:
				string = read_string(buf, &pos, bytes, '\n', &ho, &completed);
				if (completed) {
					file_box_add(FILE_BOX(dl->file_box), string, type, selection);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = GRS_IN_LIMBO;
				}
				break;

			default:
				g_error("Unexpected read state in process_glob_data.");
				break;
		}
	}

	/* We only display the glob data if we've read a set AND it's at the end
	   of the buffer.  Otherwise, the stuff we'd display would immediately be
	   overwritten by the stuff we're going to read in the next iteration
	   (which should happen immediately). */
	if (rs == GRS_DONE) {
		exhibit_cull(e);
		exhibit_rearrange_and_show(e);
	}
}


static void set_icons(void) {
#include "app_icons.h"

	GList* icons = NULL;

	/* Setup the application icons. */
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_16x16_inline, FALSE, NULL));
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_24x24_inline, FALSE, NULL));
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_32x32_inline, FALSE, NULL));
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_36x36_inline, FALSE, NULL));
	gtk_window_set_default_icon_list(icons);
}



/* Resize the DListings if necessary. */
static void window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, Exhibit* e) {
	GSList* dl_iter;
	DListing* dl;

	if (e->width_change) {
		/* Cycle through the DListings and set the new optimal width.  This
		   will make them optimize themselves to the window's width. */
		for (dl_iter = e->dl_slist; dl_iter; dl_iter = g_slist_next(dl_iter)) {
			dl = dl_iter->data;
			dlisting_set_optimal_width(dl,
					((gint)dl->optimal_width) + e->width_change);
		}
		e->width_change = 0;
	}
}


/* Track the width of the toplevel window. */
static gboolean window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, Exhibit* e) {
	e->width_change += event->width - window->allocation.width;
	return FALSE;
}


/* A key has been pressed -- write it to the terminal. */
static gboolean window_key_press_event(GtkWidget* window, GdkEventKey* event,
		gpointer data) {

	gsize  bytes_written;
	gchar* temp1;
	gchar* temp2;

	gboolean result;   //FIXME default value?

	switch (event->keyval) {
		case GDK_Home:
		case GDK_Left:
		case GDK_Up:
		case GDK_Right:
		case GDK_Down:
		case GDK_Page_Up:
		case GDK_Page_Down:
		case GDK_End:
			/* These keys we let the display interpret. */
			result = FALSE;
			break;

		default:
			/* The rest are passed to the terminal. */
			temp1 = g_malloc(2);
			*temp1 = event->keyval;
			*(temp1 + 1) = '\0';

			/* Convert out of utf8. */
			temp2 = g_locale_from_utf8(temp1, -1, NULL, &bytes_written, NULL);

			if (temp2) {
				if (event->state & GDK_CONTROL_MASK) {
					/* Control is being held.  Determine if it's a control
					   key and convert. */
					if (event->keyval >= 'a' && event->keyval <= 'z')
						*temp2 -= 96;
					else if (event->keyval >= '[' && event->keyval <= '_')
						*temp2 -= 64;
					else if (event->keyval == '@')
						*temp2 = '\0';
				}

				if (!put_param(STDOUT_FILENO, P_KEY, temp2)) {
					g_critical("Couldn't write key to stdout");
					exit(EXIT_FAILURE);
				}

				g_free(temp2);
			}
			else
				result = FALSE;

			g_free(temp1);
			break;
	}

	return result;
}


/* Send the id of the display's X window to seer. */
static void write_xwindow_id(GtkWidget* gtk_window) {

	GdkWindow* gdk_window = gtk_window->window;
	GString* xwindow_string = g_string_new(NULL);

	if (gdk_window)
		g_string_printf(xwindow_string, "%lu", GDK_WINDOW_XID(gdk_window));
	else {
		g_warning("Couldn't find an id for the display's window.");
		xwindow_string = g_string_assign(xwindow_string, "0");
	}

	if (!put_param(STDOUT_FILENO, P_WIN_ID, xwindow_string->str)) {
		g_critical("Couldn't write window ID to stdout");
		exit(EXIT_FAILURE);
	}

	g_string_free(xwindow_string, TRUE);
}


static gboolean parse_args(int argc, char** argv) {
	gboolean in_loop = TRUE;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "bf:vVz:")) {
			case -1:
				in_loop = FALSE;
				break;
			case '?':
				gtk_main_quit();
				break;
			case 'b':
				/* No icons. */
				v.show_icons = FALSE;
				break;
			case 'v':
			case 'V':
				report_version();
				return FALSE;
				break;
			case 'z':
				v.font_size_modifier = CLAMP(atoi(optarg), -10, 10);
				break;
		}
	}
	return TRUE;
}


static void report_version(void) {
	g_print("vgclassic %s\n", VERSION);
	g_print("Released %s\n", VG_RELEASE_DATE);
	return;
}


int main(int argc, char *argv[]) {

	GtkWidget* vbox;
	GtkWidget* scrolled_window;

	GtkStyle* style;

	/* This is pretty central -- it gets passed around a lot. */
	Exhibit	e;
	e.dl_slist = NULL;
	
	gtk_init(&argc, &argv);

	/* Option defaults. */
	v.show_icons = TRUE;
	v.font_size_modifier = 0;
	if (! parse_args(argc, argv) )
		return 0;

	/* Set the label font sizes. */
	file_box_set_sizing(v.font_size_modifier);
	dlisting_set_sizing(v.font_size_modifier);

	/* Create vgclassic window. */
	e.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(e.window), 5);
	gtk_window_set_title(GTK_WINDOW(e.window), "vg");
	gtk_window_set_default_size(GTK_WINDOW(e.window), 340, 420);
	e.width_change = 0;

	/* VBox for the scrolled window and the command-line widget. */
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
	gtk_container_add(GTK_CONTAINER(e.window), vbox);
	gtk_widget_show(vbox);

	/* ScrolledWindow for the layout. */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show(scrolled_window);
	e.vadjustment = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(scrolled_window));

	/* Command line text widget. */
	e.cmdline = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(e.cmdline), FALSE);
	gtk_widget_set_sensitive(e.cmdline, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), e.cmdline, FALSE, FALSE, 0);
	gtk_widget_show(e.cmdline);

	/* The DListing separator looks better if it's filled instead of sunken,
	   so use the text color.  This probably isn't the best way to do this. */
    style = gtk_widget_get_default_style();
	dlisting_set_separator_color(style->fg[GTK_STATE_NORMAL]);

	/* Setup the listings display. */
	e.listings_box = gtk_vbox_new(FALSE, 5);
	gtk_box_set_homogeneous(GTK_BOX(e.listings_box), FALSE);
	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(scrolled_window), e.listings_box);
	gtk_widget_show(e.listings_box);

	g_signal_connect(G_OBJECT(e.window), "configure-event",
			G_CALLBACK(window_configure_event), &e);
	g_signal_connect(G_OBJECT(e.window), "size-allocate",
			G_CALLBACK(window_allocate_event), &e);
	g_signal_connect(G_OBJECT(e.window), "delete_event",
			G_CALLBACK(window_delete_event), NULL);
	g_signal_connect(G_OBJECT(e.window), "key-press-event",
			G_CALLBACK(window_key_press_event), NULL);

	set_icons();

	GIOChannel* stdin_ioc;

	/* Setup a watch for glob input. */
	if ( (stdin_ioc = g_io_channel_unix_new(STDIN_FILENO)) == NULL) {
		g_critical("Couldn't create IOChannel from stdin");
		exit(EXIT_FAILURE);
	}
	g_io_channel_set_encoding(stdin_ioc, NULL, NULL);
	g_io_channel_set_flags(stdin_ioc, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(stdin_ioc, G_IO_IN, receive_data, &e);

	/*gdk_window_set_debug_updates(TRUE);*/

	/* And we're off... */
	gtk_widget_show(e.window);

	/* Pass the window ID back to seer. */
	write_xwindow_id(e.window);

	gtk_main();

	return EXIT_SUCCESS;
}

