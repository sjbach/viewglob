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
#include <string.h>       /* for strcmp */
#include <unistd.h>       /* For getopt */
#include "wrap_box.h"
#include "gviewglob.h"

struct viewable_preferences v;

#if DEBUG_ON
FILE* df;
#endif

/* Chooses a selection state based on the string's first char. */
static enum selection_state map_selection_state(const GString* string) {

	/* DEBUG((df, "state: %s\n", string->str)); */
	switch ( *(string->str) ) {
		case '-':
			return S_NO;
		case '~':
			return S_MAYBE;
		case '*':
			return S_YES;
	}
}


/* Chooses a file type based on the string's first char. */
static enum file_type map_file_type(const GString* string) {

	/* DEBUG((df, "type: %s\n", string->str)); */
	switch ( *(string->str) ) {
		case 'f':
			return T_FILE;
		case 'd':
			return T_DIR;
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

	if ( aa->rank == (*bb) )
		return 0;
	else if ( aa->rank > (*bb) )
		return 1;
	else
		return -1;
}


static gint cmp_fitem_same_name(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const GString* bb = b;

	return strcmp( aa->name->str, bb->str );
}


/* Sort by type (dir first), then by name (default Windows style). */
static gint cmp_fitem_ordering_type_alphabetical(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const FItem* bb = b;

	if (aa->type == T_DIR) {
		if (bb->type == T_DIR)
			return strcmp( aa->name->str, bb->name->str );
		else
			return -1;
	}
	else {
		if (bb->type == T_DIR)
			return 1;
		else
			return strcmp( aa->name->str, bb->name->str );
	}
}


/* Sort strictly by name (default ls style). */
static gint cmp_fitem_ordering_alphabetical(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const FItem* bb = b;

	return strcmp( aa->name->str, bb->name->str );
}


/* Try to get a string from the given buffer.  If delim is not seen, save the string for
   the next call (combining with whatever is already saved).  When delim is seen, return
   the completed string. */
static GString* read_string(const char* buff, gsize* start, gsize n, char delim, gboolean* finished) {
	static GString* holdover = NULL;
	static gboolean has_holdover = FALSE;

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
		if (has_holdover) {
			g_string_prepend(string, holdover->str);
			has_holdover = FALSE;
		}
	}
	else {
		if (has_holdover)
			g_string_append_len(holdover, buff + *start, i - *start);
		else {
			if ( ! holdover )   /* This will only be true once. */
				holdover = g_string_new_len(buff + *start, i - *start);
			else {
				g_string_truncate(holdover, 0);
				g_string_append_len(holdover, buff + *start, i - *start);
			}

			has_holdover = TRUE;
		}
	}

	*start = i;
	return string;
}



static gboolean win_delete_event(GtkWidget* widget, GdkEvent* event, gpointer data) {

	gtk_main_quit();
	return FALSE;
}


static gboolean receive_data(GIOChannel* source, GIOCondition condition, gpointer data) {
	static char buff[2048];   /* BUFSIZ? */
	gsize bytes_read;

	GError* error = NULL;
	gboolean in_loop = TRUE;

	DEBUG((df, "=="));

	while (in_loop) {
		switch ( g_io_channel_read_chars(source, buff, 2048, &bytes_read, &error) ) {

			case (G_IO_STATUS_ERROR):
				DEBUG((df, error->message));
				gtk_main_quit();
				in_loop = FALSE;
				break;

			case (G_IO_STATUS_NORMAL):
				process_data(buff, bytes_read, (Exhibit*) data);
				in_loop = FALSE;
				break;

			case (G_IO_STATUS_EOF):
				DEBUG((df, "Shutdown\n"));
				g_io_channel_shutdown(source, FALSE, NULL);
				gtk_main_quit();
				in_loop = FALSE;
				break;

			case (G_IO_STATUS_AGAIN):
				continue;

			default:
				DEBUG((df, "Unexpected result from g_io_channel_read_chars\n"));
				gtk_main_quit();
				in_loop = FALSE;
				break;
		}
	}

	return TRUE;
}


static void dlisting_debug_print_marked(const GSList* dl_slist) {
	DListing* dl;
	DEBUG((df, "marked:\n"));
	while (dl_slist) {
		dl = dl_slist->data;
		if ( dl->marked == TRUE )
			DEBUG((df, " - %s\n", dl->name->str));
		dl_slist = g_slist_next(dl_slist);
	}
	DEBUG((df, "---\n"));
}


static void fitem_debug_print_names(const GSList* fi_slist) {
	FItem* fi;
	DEBUG((df, "\nNames:\n"));
	while (fi_slist) {
		fi = fi_slist->data;
		DEBUG((df, "- %s\n", fi->name->str));
		fi_slist = g_slist_next(fi_slist);
	}
	DEBUG((df, "---\n"));
}


static enum read_state get_data_type(const GString* s) {
	if (strcmp(s->str, "glob") == 0)
		return RS_GLOB;
	else if (strcmp(s->str, "cmd") == 0)
		return RS_CMD;
	else
		return RS_DONE;    /* This should not happen */
}


/* Finite state machine to interpret data.
   There is too much logic in this function. */
static void process_data(const char* buff, gsize bytes, Exhibit* e) {
	static enum read_state rs = RS_DONE;  /* These variables are all static because they may need */
	static gboolean advance = FALSE;      /* to be preserved between calls to this function (in   */
	                                      /* the case that buff is not the whole input).          */
	static GString* selected_count;
	static GString* total_count;
	static GString* hidden_count;

	static enum file_type type;
	static enum selection_state state;

	static gint dir_rank = 0;
	static DListing* dl;
	FItem* fi;

	GString* string = NULL;
	GSList* search_result;
	gboolean completed = FALSE;

	/* It's possible to receive the end of one set of glob data and the start of another in the
	   same buffer.  If that happens, there's no point in updating the screen with the first set,
	   as it'll immediately be superseded.  This variable is set when glob data is processed, and
	   unset when glob data is started.  At the end of this function it is checked against to see
	   if the widgets should be updated. */
	gboolean full_glob_data_processed = FALSE;

	gsize pos = 0;
	DEBUG((df, "&&"));

	DEBUG((df, "((("));
	for (pos = 0; pos < bytes; pos++) {
		DEBUG((df, "%c", buff[pos]));

	}
	pos = 0;
	DEBUG((df, ")))"));

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
				rs = RS_DATA_SOURCE;
				break;


			case RS_DATA_SOURCE:
				/* Determine type of data (glob or cmd). */
				string = read_string(buff, &pos, bytes, ':', &completed);
				if (completed) {
					DEBUG((df, "[[%s]]\n", string->str));
					rs = get_data_type(string);
					advance = TRUE;
					g_string_free(string, TRUE);
				}
				break;

			case RS_CMD:
				string = read_string(buff, &pos, bytes, '\n', &completed);
				if (completed) {
					/* Set the cmdline text. */
					DEBUG((df, "cmd: %s\n", string->str));
					gtk_entry_set_text(GTK_ENTRY(e->cmdline), string->str);
					rs = RS_DONE;
					advance = TRUE;
					g_string_free(string, TRUE);
				}
				break;

			case RS_GLOB:
				/* dlisting_debug_print_marked(e->dl_slist); */
				full_glob_data_processed = FALSE;
				dir_rank = 0;
				dlisting_unmark_all(e->dl_slist);
				rs = RS_SELECTED_COUNT;
				break;

			case RS_SELECTED_COUNT:
				selected_count = read_string(buff, &pos, bytes, ' ', &completed);
				if (completed) {
					dir_rank++;
					rs = RS_FILE_COUNT;
					advance = TRUE;
				}
				break;

			case RS_FILE_COUNT:
				total_count = read_string(buff, &pos, bytes, ' ', &completed);
				if (completed) {
					rs = RS_HIDDEN_COUNT;
					advance = TRUE;
				}
				break;

			case RS_HIDDEN_COUNT:
				hidden_count = read_string(buff, &pos, bytes, ' ', &completed);
				if (completed) {
					rs = RS_DIR_NAME;
					advance = TRUE;
				}
				break;

			/* Get dl, the DListing we're currently working on, from here. */
			case RS_DIR_NAME:
				string = read_string(buff, &pos, bytes, '\n', &completed);
				if (completed) {

					search_result = g_slist_find_custom(e->dl_slist, string, cmp_dlisting_same_name);
					if (search_result) {
						/* DEBUG((df, "<known_dir>")); */
						dl = search_result->data;
						dlisting_update_file_counts(dl, selected_count, total_count, hidden_count);
						dlisting_mark(dl, dir_rank);
						fitem_unmark_all(dl->fi_slist);  /* We'll be reading these next, at which point they'll be remarked. */
					}
					else {
						/* It's a new DListing. */
						/* DEBUG((df, "<new_dir>")); */
						//DEBUG((df, "(copt: %d)", e->optimal_width));
						dl = dlisting_new(string, dir_rank, selected_count, total_count, hidden_count, e->optimal_width);
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
					full_glob_data_processed = TRUE;
					rs = RS_DONE;
				}
				else {                                /* Another DListing. */
					/* (No need to advance) */
					rs = RS_SELECTED_COUNT;
				}

				break;


			/* Have to save file_state and file_type until we get file_name */
			case RS_FILE_STATE:
				string = read_string(buff, &pos, bytes, ' ', &completed);
				if (completed) {
					state = map_selection_state(string);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = RS_FILE_TYPE;
				}
				break;
				
			case RS_FILE_TYPE:
				string = read_string(buff, &pos, bytes, ' ', &completed);
				if (completed) {
					type = map_file_type(string);
					g_string_free(string, TRUE);

					advance = TRUE;
					rs = RS_FILE_NAME;
				}
				break;

			case RS_FILE_NAME:
				string = read_string(buff, &pos, bytes, '\n', &completed);
				if (completed) {

					search_result = g_slist_find_custom(dl->fi_slist, string, cmp_fitem_same_name);
					if (search_result) {
						/* We've seen this FItem before. */
						fi = search_result->data;
						fitem_mark(fi);
						if ( ! fitem_update_type_and_state(fi, type, state) ) {
							/* The type was modified, so this FItem must be resorted. */
							dl->fi_slist = g_slist_remove(dl->fi_slist, fi);
							dl->fi_slist = g_slist_insert_sorted(dl->fi_slist, fi, v.sort_function);
							dl->update_file_table = TRUE;
						}
					}
					else {
						//DEBUG((df, "here1: %s\n", string->str));
						/* It's a new FItem. */
						gboolean hidden = g_str_has_prefix(string->str, ".");    /* Check if it's a hidden file. */

						/* Big huge logic test. */
						if ( (v.file_display_limit == 0 || dl->n_v_fis < v.file_display_limit) &&          /* Check if we're under the limit. */
						     ((hidden && (v.show_hidden_files || dl->force_show_hidden)) || !hidden)) {   /* Check if we should display it. */
							/* We're good to go.  Build the widgets. */
						//DEBUG((df, "here2\n"));
							fi = fitem_new(string, state, type, TRUE);
							dl->n_v_fis++;
							dl->update_file_table = TRUE;
						}
						else {
						//DEBUG((df, "here3\n"));
							/* Don't build the widgets. */
							fi = fitem_new(string, state, type, FALSE);
						}
						dl->fi_slist = g_slist_insert_sorted(dl->fi_slist, fi, v.sort_function);
					}

					g_string_free(string, TRUE);

					advance = TRUE;
					rs = RS_IN_LIMBO;
				}
				break;
		}
	}

	DEBUG((df, "***out\n"));

	if (full_glob_data_processed) {
		delete_old_dlistings(e);
		rearrange_and_show(e);
		DEBUG((df, "^^"));
	}

	DEBUG((df, "**"));
}


static void rearrange_and_show(Exhibit* e) {
	gint next_rank = 1;
	DListing* dl;
	GSList* search_result;

	/* DEBUG((df, "\nin order of rank:\n")); */
	while (search_result = g_slist_find_custom(e->dl_slist, &next_rank, cmp_dlisting_same_rank)) {
		dl = search_result->data;
		/* DEBUG((df, " - %s\n", dl->name->str)); */

		/* FIXME need mechanism to determine if not all files have been marked */
		/*if (dl->update_file_table)	{ */
			dlisting_file_table_update(dl);
		/*	dl->update_file_table = FALSE; */
		/*} */

		/* Ordering */
		if (dlisting_is_new(dl)) {
			/* DEBUG((df, "<packstart>")); */
			gtk_widget_hide(dl->listing_vbox);
			gtk_box_pack_start(GTK_BOX(e->display), dl->listing_vbox, FALSE, FALSE, 0);
			gtk_box_reorder_child(GTK_BOX(e->display), dl->listing_vbox, next_rank - 1);
			gtk_widget_show(dl->listing_vbox);
		}
		else if (dl->rank != dl->old_rank) {
				/* DEBUG((df, "<reorder>")); */
				gtk_box_reorder_child(GTK_BOX(e->display), dl->listing_vbox, next_rank - 1);
		}
		next_rank++;
		DEBUG((df, "~~"));
	}
	DEBUG((df, "@@"));

}


/* Remove DListings and their widgets if they're no longer marked for showing. */
static void delete_old_dlistings(Exhibit* e) {
	GSList* list_iter;
	DListing* dl;

	list_iter = e->dl_slist;
	while (list_iter) {
		dl = list_iter->data;

		if (dl->marked)
			list_iter = g_slist_next(list_iter);
		else {
			/* Take no prisoners. */
			/* DEBUG((df, "removing %s\n", dl->name->str)); */
			GSList* tmp;
			tmp = list_iter;
			list_iter = g_slist_next(list_iter);
			e->dl_slist = g_slist_delete_link(e->dl_slist, tmp);
			dlisting_free(dl);
		}
	}
}


#define ICON_SIZE 12
static void set_icons(Exhibit* e) {
#include "icons.h"

	GList* icons = NULL;

	GtkIconTheme* current_theme;
	GtkIconInfo* icon_info;

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

	/* Directory icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-directory", ICON_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		/* (I should be checking for an error here) */
		v.dir_pixbuf = gtk_icon_info_load_icon(icon_info, NULL);
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		v.dir_pixbuf = make_pixbuf_scaled(directory_inline);
	}

	/* Regular file icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-regular", ICON_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		/* (I should be checking for an error here) */
		v.file_pixbuf = gtk_icon_info_load_icon(icon_info, NULL);
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		v.file_pixbuf = make_pixbuf_scaled(regular_inline);
	}

	/* Context menu icons. */
	v.show_hidden_pixbuf = gdk_pixbuf_new_from_inline(-1, hidden_inline, FALSE, NULL);
	v.show_all_pixbuf = gdk_pixbuf_new_from_inline(-1, all_inline, FALSE, NULL);
}


static GdkPixbuf* make_pixbuf_scaled(const guint8 icon_inline[]) {
		gint width, height;
		GdkPixbuf* temp;
		GdkPixbuf* result;

		temp = gdk_pixbuf_new_from_inline(-1, icon_inline, FALSE, NULL);
		width = gdk_pixbuf_get_width(temp);
		height = gdk_pixbuf_get_height(temp);
		result = gdk_pixbuf_scale_simple(temp, ICON_SIZE, height * ICON_SIZE / width, GDK_INTERP_BILINEAR);

		g_free(temp);
		return result;
}


/* The main listings widget has been resized; remember the new size so the WrapBoxes can grow/shrink gracefully. */
static void listing_resize_event(GtkWidget* display_vbox, GtkAllocation* allocation, Exhibit* e) {
	GSList* dl_iter;
	DListing* dl;

	DEBUG((df, "size-allocate-event (%d,%d)\n", allocation->width, allocation->height));
	e->optimal_width = allocation->width;
	for (dl_iter = e->dl_slist; dl_iter; dl_iter = g_slist_next(dl_iter)) {
		dl = dl_iter->data;
		if (dl->file_table)
			wrap_box_set_optimal_width(WRAP_BOX(dl->file_table), allocation->width - 4);
	}

}


static gboolean configure_event(GtkWidget* window, GdkEventConfigure* event, Exhibit* e) {
	static gint last_width = -1;
	GSList* dl_iter;
	DListing* dl;

	if (last_width == -1)
		last_width = event->width;

	DEBUG((df, "configure-event (%d,%d) (%d)\n", event->width, event->height, event->width - last_width));
	e->optimal_width = event->width - 34;
	if (event->width != last_width) {
		for (dl_iter = e->dl_slist; dl_iter; dl_iter = g_slist_next(dl_iter)) {
			dl = dl_iter->data;
			if (dl->file_table)
				wrap_box_set_optimal_width(WRAP_BOX(dl->file_table), event->width - 34);
		}
		last_width = event->width;
	}

	return FALSE;
}


static void parse_args(int argc, char** argv) {
	gboolean in_loop = TRUE;
	glong d_temp;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "bn:s:w")) {
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
			case 'n':
				/* Maximum files to display. */
				d_temp = atol(optarg);
				DEBUG((df, "d_temp: %ld\n", d_temp));
				if (d_temp < 0)
					v.file_display_limit = 300;
				else
					v.file_display_limit = d_temp;
				break;
			case 's':
				/* File sorting style. */
				if (strcmp(optarg, "ls") == 0)
					v.sort_function = cmp_fitem_ordering_alphabetical;
				else if (strcmp(optarg, "win") == 0 || strcmp(optarg, "windows") == 0)
					v.sort_function = cmp_fitem_ordering_type_alphabetical;
				break;
			case 'w':
				/* Show hidden files by default. */
				v.show_hidden_files = TRUE;
				break;
		}
	}
	return;
}


int main(int argc, char *argv[]) {

	GtkWidget* window;
	GtkWidget* vbox;
	GtkWidget* scrolled_window;

	GtkWidget* command_line_entry;

	Exhibit	e;        /* This is pretty central -- it gets passed around a lot. */
	e.dl_slist = NULL;
	e.optimal_width = 340 - 34;
	
	GIOChannel* read_channel;

	gtk_init (&argc, &argv);

#if DEBUG_ON
	df = fopen("/tmp/debug.txt", "w");
#endif

	/* Option defaults. */
	v.show_icons = TRUE;
	v.show_hidden_files = FALSE;
	v.file_display_limit = 300;
	v.sort_function = cmp_fitem_ordering_alphabetical;
	parse_args(argc, argv);

	/* Create gviewglob window. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 5);
	gtk_window_set_title(GTK_WINDOW(window), (gchar *) "gviewglob");
	gtk_window_set_default_size(GTK_WINDOW(window), 340, 420);
	g_signal_connect(G_OBJECT (window), "delete_event", G_CALLBACK(win_delete_event), NULL);

	/* VBox for the scrolled window and the command line text widget. */
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
	gtk_widget_set_state(command_line_entry, GTK_STATE_INSENSITIVE);
	gtk_box_pack_start(GTK_BOX(vbox), command_line_entry, FALSE, FALSE, 0);
	gtk_widget_show(command_line_entry);
	e.cmdline = command_line_entry;

	/* The DListing separator looks better if it's filled instead of sunken, so use the text color.
	   This probably isn't the best way to do this. */
	GtkStyle* style = gtk_widget_get_default_style();
	v.separator_color = &style->fg[GTK_STATE_NORMAL];

	/* Setup the listings display. */
	e.display = gtk_vbox_new(FALSE, 4);
	gtk_box_set_homogeneous(GTK_BOX(e.display), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), e.display);
	gtk_container_set_resize_mode(GTK_CONTAINER(e.display), GTK_RESIZE_IMMEDIATE);
	/* g_object_set(e.display, "width-request", 150, NULL); */
	gtk_widget_show(e.display);

	/* Keep tabs on the size of this widget. */
	/* g_signal_connect(G_OBJECT(e.display), "size-allocate", G_CALLBACK(listing_resize_event), &e); */
	g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(configure_event), &e);

	set_icons(&e);

	/* Wait for input. */
	read_channel = g_io_channel_unix_new(0);
	g_io_channel_set_encoding(read_channel, NULL, NULL);
	g_io_channel_set_flags(read_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(read_channel, G_IO_IN, receive_data, &e);

	/* And we're off... */
	gtk_widget_show(window);
	gtk_main();

#if DEBUG_ON
	(void)fclose(df);
#endif

	return 0;
}

