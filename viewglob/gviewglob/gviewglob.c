/*
	Copyright (C) 2004 Stephen Bach
	This file is part of the viewglob package.

	viewglob is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	viewglob is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with viewglob; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <string.h>       /* For strcmp. */
#include <unistd.h>       /* For getopt. */
#include <stdio.h>        /* For BUFSIZ. */
#include "file_box.h"
#include "gviewglob.h"

/* Prototypes. */
static gboolean receive_data(GIOChannel* source, gchar* buff, gsize size, gsize* bytes_read);
static GString* read_string(const gchar* buff, gsize* start, gsize n, gchar delim, struct holdover* ho, gboolean* finished);

static void        set_icons(Exhibit* e);
static GdkPixbuf*  make_pixbuf_scaled(const guint8 icon_inline[], gint scale_height);

static gboolean  parse_args(int argc, char** argv);
static void      report_version(void);

static void process_cmd_data(const gchar* buff, gsize bytes, Exhibit* e);
static void process_glob_data(const gchar* buff, gsize bytes, Exhibit* e);

static void exhibit_unmark_all(Exhibit* e);
static void exhibit_cull(Exhibit* e);
static void exhibit_rearrange_and_show(Exhibit* e);

static FileSelection  map_selection_state(const GString* string);
static FileType       map_file_type(const GString* string);

static gint cmp_dlisting_same_name(gconstpointer a, gconstpointer b);
static gint cmp_dlisting_same_rank(gconstpointer a, gconstpointer b);

static gboolean  win_delete_event(GtkWidget*, GdkEvent*, gpointer);
static gboolean configure_event(GtkWidget* window, GdkEventConfigure* event, Exhibit* e);

struct viewable_preferences v;

#if DEBUG_ON
FILE* df;
#endif

/* Chooses a selection state based on the string's first char. */
static FileSelection map_selection_state(const GString* string) {
	switch ( *(string->str) ) {
		case '-':
			return FS_NO;
		case '~':
			return FS_MAYBE;
		case '*':
			return FS_YES;
		default:
			g_warning("Unexpected selection state \"%c\".", *(string->str));
			return FS_NO;
	}
}


/* Chooses a file type based on the string's first char. */
static FileType map_file_type(const GString* string) {
	switch ( *(string->str) ) {
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
			g_warning("Unexpected file type \"%c\".", *(string->str));
			return FT_REGULAR;
	}
}


static gint cmp_dlisting_same_name(gconstpointer a, gconstpointer b) {
	const DListing* aa = a;
	const GString* bb = b;

	return strcmp( aa->name->str, bb->str );
}


static gint cmp_dlisting_same_rank(gconstpointer a, gconstpointer b) {
	const DListing* aa = a;
	const gint* bb = b;

	if ( aa->rank == *bb )
		return 0;
	else if ( aa->rank > *bb )
		return 1;
	else
		return -1;
}


/* Try to get a string from the given buffer.  If delim is not seen, save the string for the
   next call (combining with whatever is already saved) in ho.  When delim is seen, return
   the completed string. */
static GString* read_string(const gchar* buff, gsize* start, gsize n, gchar delim, struct holdover* ho, gboolean* finished) {

	GString* string;
	gsize i;
	gboolean delim_reached = FALSE;

	for (i = *start; i < n; i++) {
		if ( *(buff + i) == delim ) {
			delim_reached = TRUE;
			break;
		}
	}
	*finished = delim_reached;

	if (delim_reached) {
		string = g_string_new_len(buff + *start, i - *start);
		if (ho->has_holdover) {
			g_string_prepend(string, ho->string->str);
			ho->has_holdover = FALSE;
		}
	}
	else {
		if (ho->has_holdover)
			g_string_append_len(ho->string, buff + *start, i - *start);
		else {
			if ( ! ho->has_holdover )   /* This will only be true once. */
				ho->string = g_string_new_len(buff + *start, i - *start);
			else {
				g_string_truncate(ho->string, 0);
				g_string_append_len(ho->string, buff + *start, i - *start);
			}

			ho->has_holdover = TRUE;
		}
	}

	*start = i;
	return string;
}



static gboolean win_delete_event(GtkWidget* widget, GdkEvent* event, gpointer data) {

	gtk_main_quit();
	return FALSE;
}


static gboolean get_glob_data(GIOChannel* source, GIOCondition condition, gpointer data) {
	static gchar buff[BUFSIZ];
	gsize bytes_read;

	if (receive_data(source, buff, sizeof buff, &bytes_read)) {
		process_glob_data(buff, bytes_read, (Exhibit*) data);
	}
	else
		gtk_main_quit();

	return TRUE;

}


static gboolean get_cmd_data(GIOChannel* source, GIOCondition condition, gpointer data) {
	static gchar buff[BUFSIZ];
	gsize bytes_read;

	if (receive_data(source, buff, sizeof buff, &bytes_read)) {
		process_cmd_data(buff, bytes_read, (Exhibit*) data);
	}
	else
		gtk_main_quit();

	return TRUE;

}


/* Attempt to read size bytes into buff from source. */
static gboolean receive_data(GIOChannel* source, gchar* buff, gsize size, gsize* bytes_read) {

	GError* error = NULL;
	gboolean in_loop = TRUE;
	gboolean data_read;

	DEBUG((df, "=="));

	while (in_loop) {
		switch ( g_io_channel_read_chars(source, buff, size, bytes_read, &error) ) {

			case (G_IO_STATUS_ERROR):
				g_printerr("gviewglob: %s\n", error->message);
				in_loop = data_read = FALSE;
				break;

			case (G_IO_STATUS_NORMAL):
				data_read = TRUE;
				in_loop = FALSE;
				break;

			case (G_IO_STATUS_EOF):
				DEBUG((df, "Shutdown\n"));
				g_io_channel_shutdown(source, FALSE, NULL);
				in_loop = data_read = FALSE;
				break;

			case (G_IO_STATUS_AGAIN):
				continue;

			default:
				g_warning("Unexpected result from g_io_channel_read_chars.");
				in_loop = data_read = FALSE;
				break;
		}
	}

	return data_read;
}


/* Finite state machine to interpret cmd data. */
static void process_cmd_data(const gchar* buff, gsize bytes, Exhibit* e) {
	static enum read_state rs = RS_DONE;  /* These variables are all static because they may need */
	static gboolean advance = FALSE;      /* to be preserved between calls to this function (in   */
	                                      /* the case that buff is not the whole input).          */

	static struct holdover ho = { NULL, FALSE };

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
			case RS_DONE:
				DEBUG((df, ":::"));
				rs = RS_CMD;
				break;

			case RS_CMD:
				string = read_string(buff, &pos, bytes, '\n', &ho, &completed);
				if (completed) {
					/* Set the cmdline text. */
					DEBUG((df, "cmd: %s\n", string->str));
					gtk_entry_set_text(GTK_ENTRY(e->cmdline), string->str);
					rs = RS_DONE;
					advance = TRUE;
					g_string_free(string, TRUE);
				}
				break;

			default:
				g_error("Unexpected read state in process_cmd_data.");
				break;
		}
	}

	DEBUG((df, "***out\n"));
}


/* Finite state machine to interpret glob data. */
/* TODO: instead of freeing string after each read, try to use it correctly. */
static void process_glob_data(const gchar* buff, gsize bytes, Exhibit* e) {
	static enum read_state rs = RS_DONE;  /* These variables are all static because they may need */
	static gboolean advance = FALSE;      /* to be preserved between calls to this function (in   */
	                                      /* the case that buff is not the whole input).          */

	static struct holdover ho = { NULL, FALSE };

	static GString* selected_count;
	static GString* total_count;
	static GString* hidden_count;

	static FileType type;
	static FileSelection selection;

	static gint dir_rank = 0;
	static DListing* dl;

	GString* string = NULL;
	GSList* search_result;
	gboolean completed = FALSE;

	gsize pos = 0;
	DEBUG((df, "&&"));

	while (pos < bytes) {

		/* Skip the next character (the delimiter). */
		if (advance) {
			pos++;
			advance = FALSE;
			continue;
		}

		switch (rs) {
			case RS_DONE:
				DEBUG((df, ":::"));
				dir_rank = 0;
				exhibit_unmark_all(e);
				rs = RS_SELECTED_COUNT;
				break;

			case RS_SELECTED_COUNT:
				selected_count = read_string(buff, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					dir_rank++;
					rs = RS_FILE_COUNT;
					advance = TRUE;
				}
				break;

			case RS_FILE_COUNT:
				total_count = read_string(buff, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					rs = RS_HIDDEN_COUNT;
					advance = TRUE;
				}
				break;

			case RS_HIDDEN_COUNT:
				hidden_count = read_string(buff, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					rs = RS_DIR_NAME;
					advance = TRUE;
				}
				break;

			/* Get dl, the DListing we're currently working on, from here. */
			case RS_DIR_NAME:
				string = read_string(buff, &pos, bytes, '\n', &ho, &completed);
				if (completed) {

					search_result = g_slist_find_custom(e->dl_slist, string, cmp_dlisting_same_name);
					if (search_result) {
						/* DEBUG((df, "<known_dir>")); */
						dl = search_result->data;
						dlisting_update_file_counts(dl, selected_count, total_count, hidden_count);
						dlisting_mark(dl, dir_rank);

						/* We'll be reading these next, at which point they'll be remarked. */
						file_box_begin_read(FILE_BOX(dl->file_box));
					}
					else {
						/* It's a new DListing. */
						/* DEBUG((df, "<new_dir>")); */
						dl = dlisting_new(string, dir_rank, selected_count, total_count, hidden_count, e->listings_box->allocation.width);
						e->dl_slist = g_slist_append(e->dl_slist, dl);
					}
					g_string_free(selected_count, TRUE);
					g_string_free(total_count, TRUE);
					g_string_free(hidden_count, TRUE);

					advance = TRUE;
					rs = RS_IN_LIMBO;
				}
				break;

			/* Either we'll read another FItem (or the first), a new DListing, or EOF (double \n). */
			case RS_IN_LIMBO:
				completed = FALSE;
				if ( *(buff + pos) == '\t' ) {        /* Another FItem. */
					advance = TRUE;
					rs = RS_FILE_STATE;
				}
				else if ( *(buff + pos) == '\n' ) {   /* End of glob-expand data. */
					advance = TRUE;
					DEBUG((df, "~~~"));
					rs = RS_DONE;
				}
				else {                                /* Another DListing. */
					/* (No need to advance) */
					rs = RS_SELECTED_COUNT;
				}

				break;


			/* Have to save file_state and file_type until we get file_name */
			case RS_FILE_STATE:
				string = read_string(buff, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					selection = map_selection_state(string);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = RS_FILE_TYPE;
				}
				break;

			case RS_FILE_TYPE:
				string = read_string(buff, &pos, bytes, ' ', &ho, &completed);
				if (completed) {
					type = map_file_type(string);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = RS_FILE_NAME;
				}
				break;

			case RS_FILE_NAME:
				string = read_string(buff, &pos, bytes, '\n', &ho, &completed);
				if (completed) {
					file_box_add(FILE_BOX(dl->file_box), string, type, selection);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = RS_IN_LIMBO;
				}
				break;

			default:
				g_error("Unexpected read state in process_glob_data.");
				break;
		}
	}

	DEBUG((df, "---out\n"));


	/* We only display the glob data if we've read a set AND it's at the end of the buffer.  Otherwise,
	   the stuff we'd display would immediately be overwritten by the stuff we're going to read in the
	   next iteration (which should happen immediately). */
	if (rs == RS_DONE) {
		exhibit_cull(e);
		exhibit_rearrange_and_show(e);
		gtk_widget_queue_resize(e->listings_box);  /* To make the scrollbars rescale. */
		DEBUG((df, "(^^)"));
	}

	DEBUG((df, "(**)"));
}


static void exhibit_rearrange_and_show(Exhibit* e) {
	gint next_rank = 1;
	DListing* dl;
	GSList* search_result;

	while ( (search_result = g_slist_find_custom(e->dl_slist, &next_rank, cmp_dlisting_same_rank)) ) {
		dl = search_result->data;

		/* Commit the updates to the file box. */
		file_box_flush(FILE_BOX(dl->file_box));

		/* Ordering */
		if (dlisting_is_new(dl)) {
			gtk_box_pack_start(GTK_BOX(e->listings_box), dl->widget, FALSE, FALSE, 0);
			gtk_box_reorder_child(GTK_BOX(e->listings_box), dl->widget, next_rank - 1);
			gtk_widget_show(dl->widget);
		}
		else if (dl->rank != dl->old_rank)
			gtk_box_reorder_child(GTK_BOX(e->listings_box), dl->widget, next_rank - 1);

		next_rank++;
		DEBUG((df, "(~~)"));
	}
	DEBUG((df, "(@@)"));
}


/* Remove DListings and their widgets if they're no longer marked for showing. */
static void exhibit_cull(Exhibit* e) {
	GSList* iter;
	DListing* dl;

	iter = e->dl_slist;
	while (iter) {
		dl = iter->data;

		if (dl->marked)
			iter = g_slist_next(iter);
		else {
			/* Take no prisoners. */
			/* DEBUG((df, "removing %s\n", dl->name->str)); */
			GSList* tmp;
			tmp = iter;
			iter = g_slist_next(iter);
			e->dl_slist = g_slist_delete_link(e->dl_slist, tmp);
			dlisting_free(dl);
		}
	}
}


static void exhibit_unmark_all(Exhibit* e) {
	GSList* iter;
	DListing* dl;

	for (iter = e->dl_slist; iter; iter = g_slist_next(iter)) {
		dl = iter->data;
		dl->marked = FALSE;
	}
}


static void set_icons(Exhibit* e) {
#include "icons.h"

	GList* icons = NULL;

	GtkIconTheme* current_theme;
	GtkIconInfo* icon_info;

	guint icon_size;
	GtkWidget* test_label;
	PangoLayout* test_layout;

	/* Figure out a good size for the icons. */
	test_label = gtk_label_new("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890./{}[]|");
	test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
	pango_layout_get_pixel_size(test_layout, NULL, &icon_size);
	gtk_widget_destroy(test_label);

	if (icon_size < 6)
		icon_size = 12;

	/* Setup the application icons. */
	icons = g_list_append(icons, gdk_pixbuf_new_from_inline(-1, icon_16x16_inline, FALSE, NULL));
	icons = g_list_append(icons, gdk_pixbuf_new_from_inline(-1, icon_24x24_inline, FALSE, NULL));
	icons = g_list_append(icons, gdk_pixbuf_new_from_inline(-1, icon_32x32_inline, FALSE, NULL));
	icons = g_list_append(icons, gdk_pixbuf_new_from_inline(-1, icon_36x36_inline, FALSE, NULL));
	gtk_window_set_default_icon_list(icons);

	if (!v.show_icons)
		return;

	/* Try to get icons from the current theme. */
	current_theme = gtk_icon_theme_get_default();

	/* Regular file icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-regular", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_REGULAR, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_REGULAR, make_pixbuf_scaled(regular_inline, icon_size));
	}

	/* Executable icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-executable", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_EXECUTABLE, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_EXECUTABLE, make_pixbuf_scaled(executable_inline, icon_size));
	}

	/* Directory icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-directory", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_DIRECTORY, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_DIRECTORY, make_pixbuf_scaled(directory_inline, icon_size));
	}

	/* Block device icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-blockdev", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_BLOCKDEV, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_BLOCKDEV, make_pixbuf_scaled(blockdev_inline, icon_size));
	}

	/* Character device icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-chardev", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_CHARDEV, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_CHARDEV, make_pixbuf_scaled(chardev_inline, icon_size));
	}

	/* Fifo icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-fifo", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_FIFO, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_FIFO, make_pixbuf_scaled(fifo_inline, icon_size));
	}

	/* Symlink icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-dev-symlink", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_SYMLINK, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_SYMLINK, make_pixbuf_scaled(symlink_inline, icon_size));
	}

	/* Socket icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-socket", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_SOCKET, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_SOCKET, make_pixbuf_scaled(socket_inline, icon_size));
	}


	/* Context menu icons. */
	v.show_hidden_pixbuf = gdk_pixbuf_new_from_inline(-1, context_hidden_inline, FALSE, NULL);
	v.show_all_pixbuf = gdk_pixbuf_new_from_inline(-1, context_all_inline, FALSE, NULL);
}


static GdkPixbuf* make_pixbuf_scaled(const guint8 icon_inline[], gint scale_size) {
		gint width, height;
		GdkPixbuf* temp;
		GdkPixbuf* result;

		temp = gdk_pixbuf_new_from_inline(-1, icon_inline, FALSE, NULL);
		width = gdk_pixbuf_get_width(temp);
		height = gdk_pixbuf_get_height(temp);
		result = gdk_pixbuf_scale_simple(temp, scale_size, scale_size, GDK_INTERP_BILINEAR);

		g_free(temp);
		return result;
}


static gboolean configure_event(GtkWidget* window, GdkEventConfigure* event, Exhibit* e) {
	GSList* dl_iter;
	DListing* dl;

	/*g_print("<configure-event (%d --> %d)>", window->allocation.width, event->width);*/
	if (event->width != window->allocation.width) {
		for (dl_iter = e->dl_slist; dl_iter; dl_iter = g_slist_next(dl_iter)) {
			dl = dl_iter->data;
			if (dl->file_box) {
				file_box_set_optimal_width(FILE_BOX(dl->file_box), 
						file_box_get_optimal_width(FILE_BOX(dl->file_box)) + event->width - window->allocation.width);
			}
		}
	}

	return FALSE;
}


static gboolean parse_args(int argc, char** argv) {
	gboolean in_loop = TRUE;
	gint max;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "bc:g:n:s:vVw")) {
			case -1:
				DEBUG((df, "done\n"));
				in_loop = FALSE;
				break;
			case '?':
				DEBUG((df, "Unknown argument\n"));
				gtk_main_quit();
				break;
			case 'b':
				/* No icons. */
				v.show_icons = FALSE;
				DEBUG((df, "icons\n"));
				break;
			case 'c':
				g_free(v.cmd_fifo);
				v.cmd_fifo = g_strdup(optarg);
				break;
			case 'g':
				g_free(v.glob_fifo);
				v.glob_fifo = g_strdup(optarg);
				break;
			case 'n':
				/* Maximum files to display. */
				max = atoi(optarg);
				DEBUG((df, "max: %ld\n", max));
				if (max < 0)
					v.file_display_limit = DEFAULT_FILE_DISPLAY_LIMIT;
				else
					v.file_display_limit = max;
				break;
			case 's':
				/* File sorting style. */
				if (strcmp(optarg, "ls") == 0)
					file_box_set_ordering(FBO_LS);
				else if (strcmp(optarg, "win") == 0 || strcmp(optarg, "windows") == 0)
					file_box_set_ordering(FBO_WIN);
				break;
			case 'v':
			case 'V':
				report_version();
				return FALSE;
				break;
			case 'w':
				/* Show hidden files by default. */
				v.show_hidden_files = TRUE;
				break;
		}
	}
	return TRUE;
}


static void report_version(void) {
	g_print("gviewglob %s\n", GVIEWGLOB_VERSION);
	g_print("Released %s\n", GVIEWGLOB_RELEASE_DATE);
	return;
}


int main(int argc, char *argv[]) {

	GtkWidget* window;
	GtkWidget* vbox;
	GtkWidget* scrolled_window;

	GtkWidget* command_line_entry;

	Exhibit	e;        /* This is pretty central -- it gets passed around a lot. */
	e.dl_slist = NULL;
	
	GIOChannel* glob_channel;
	GIOChannel* cmd_channel;

	gtk_init (&argc, &argv);

#if DEBUG_ON
	df = fopen("/tmp/debug.txt", "w");
#endif

	/* Option defaults. */
	v.show_icons = TRUE;
	v.show_hidden_files = FALSE;
	v.file_display_limit = DEFAULT_FILE_DISPLAY_LIMIT;
	v.glob_fifo = v.cmd_fifo = NULL;
	if (! parse_args(argc, argv) )
		return 0;

	/* Create gviewglob window. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 5);
	gtk_window_set_title(GTK_WINDOW(window), (gchar *) "gviewglob");
	gtk_window_set_default_size(GTK_WINDOW(window), 340, 420);
	g_signal_connect(G_OBJECT (window), "delete_event", G_CALLBACK(win_delete_event), NULL);

	/* VBox for the scrolled window and the command-line widget. */
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	/* Scrollbar for the file/directory display vbox. */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show(scrolled_window);

	/* Command line text widget. */
	command_line_entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(command_line_entry), FALSE);
	gtk_widget_set_sensitive(command_line_entry, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), command_line_entry, FALSE, FALSE, 0);
	gtk_widget_show(command_line_entry);
	e.cmdline = command_line_entry;

	/* The DListing separator looks better if it's filled instead of sunken, so use the text color.
	   This probably isn't the best way to do this. */
	GtkStyle* style = gtk_widget_get_default_style();
	v.separator_color = &style->fg[GTK_STATE_NORMAL];

	/* Setup the listings display. */
	e.listings_box = gtk_vbox_new(FALSE, 5);
	gtk_box_set_homogeneous(GTK_BOX(e.listings_box), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), e.listings_box);
	gtk_container_set_resize_mode(GTK_CONTAINER(e.listings_box), GTK_RESIZE_IMMEDIATE);
	/* g_object_set(e.listings_box, "width-request", 150, NULL); */
	gtk_widget_show(e.listings_box);

	/* Keep tabs on the size of this widget. */
	/* g_signal_connect(G_OBJECT(e.listings_box), "size-allocate", G_CALLBACK(listing_resize_event), &e); */
	g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(configure_event), &e);

	set_icons(&e);

	/* Setup watch for glob input. */
	if (v.glob_fifo)
		glob_channel = g_io_channel_new_file(v.glob_fifo, "r", NULL);
	else {
		/* Use stdin if a fifo wasn't provided. */
		glob_channel = g_io_channel_unix_new(0);
	}
	if (!glob_channel) {
		g_error("Could not open glob channel.");
		return 1;
	}
	g_io_channel_set_encoding(glob_channel, NULL, NULL);
	g_io_channel_set_flags(glob_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(glob_channel, G_IO_IN, get_glob_data, &e);

	/* Setup watch for cmd input (only if a fifo name was passed as an argument). */
	if (v.cmd_fifo) {
		cmd_channel = g_io_channel_new_file(v.cmd_fifo, "r", NULL);
		if (!cmd_channel) {
			g_error("Could not open cmd channel.");
			return 2;
		}
		g_io_channel_set_encoding(cmd_channel, NULL, NULL);
		g_io_channel_set_flags(cmd_channel, G_IO_FLAG_NONBLOCK, NULL);
		g_io_add_watch(cmd_channel, G_IO_IN, get_cmd_data, &e);
	}

	/*gdk_window_set_debug_updates(TRUE);*/

	/* And we're off... */
	gtk_widget_show(window);

	gtk_main();

#if DEBUG_ON
	(void)fclose(df);
#endif

	return 0;
}

