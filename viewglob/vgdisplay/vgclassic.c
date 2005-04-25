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
#include "display-common.h"
#include "file_box.h"
#include "dlisting.h"
#include "exhibit.h"
#include "param-io.h"
#include "syslogging.h"

#include <gtk/gtk.h>
#include <string.h>       /* For strcmp. */
#include <unistd.h>       /* For getopt. */

/* Prototypes. */
static gboolean receive_data(GIOChannel* source, GIOCondition condition,
		gpointer data);
static void process_glob_data(gchar* buf, gsize bytes, Exhibit* e);

static gboolean window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, Exhibit* e);
static void window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, Exhibit* e);


/* Receive data from vgd. */
static gboolean receive_data(GIOChannel* source, GIOCondition condition,
		gpointer data) {

	enum parameter param;
	gchar* value;

	Exhibit* e = data;
	int fd = g_io_channel_unix_get_fd(source);
	
	if (get_param(fd, &param, &value)) {
		switch (param) {

			case P_ORDER:
				if (STREQ(value, "refocus"))
					refocus_wrapped(e->window, e->term_win->str);
				else
					exhibit_do_order(e, value);
				break;

			case P_CMD:
				exhibit_set_cmd(e, value);
				break;

			case P_WIN_ID:
				if (!STREQ(e->term_win->str, value)) {
					raise_wrapped(e->window, value);
					e->term_win = g_string_assign(e->term_win, value);
				}
				break;

			case P_MASK:
				//FIXME
				break;

			case P_DEVELOPING_MASK:
				//FIXME
				break;

			case P_STATUS:
				//FIXME
				break;

			case P_VGEXPAND_DATA:
				if (*value != '\0')
					process_glob_data(value, strlen(value), e);
				break;

			case P_EOF:
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
		g_critical("Could not receive data from vgd");
		exit(EXIT_FAILURE);
	}

	return TRUE;
}


/* Finite state machine to interpret glob data. */
static void process_glob_data(gchar* buf, gsize bytes, Exhibit* e) {

	enum glob_read_state rs;

	gchar* string = NULL;
	gchar* selected_count = NULL;
	gchar* total_count = NULL;
	gchar* hidden_count = NULL;

	static FileType type = FT_REGULAR;
	static FileSelection selection = FS_NO;

	gint dir_rank = -1;
	gint file_rank = -1;
	DListing* dl = NULL;

	gchar* p;

	p = buf;
	rs = GRS_DONE;
	while (p < buf + bytes) {

		switch (rs) {
			case GRS_DONE:
				dir_rank = -1;
				exhibit_unmark_all(e);
				rs = GRS_SELECTED_COUNT;
				break;

			case GRS_SELECTED_COUNT:
				selected_count = up_to_delimiter(&p, ' ');
				dir_rank++;
				rs = GRS_FILE_COUNT;
				break;

			case GRS_FILE_COUNT:
				total_count = up_to_delimiter(&p, ' ');
				rs = GRS_HIDDEN_COUNT;
				break;

			case GRS_HIDDEN_COUNT:
				hidden_count = up_to_delimiter(&p, ' ');
				rs = GRS_DIR_NAME;
				break;

			/* Get dl, the DListing we're currently working on, from here. */
			case GRS_DIR_NAME:
				string = up_to_delimiter(&p, '\n');
				dl = exhibit_add(e, string, dir_rank, selected_count,
						total_count, hidden_count);
				file_rank = -1;
				rs = GRS_IN_LIMBO;
				break;

			/* Either we'll read another FItem (or the first), a new DListing,
			   or EOF (double \n). */
			case GRS_IN_LIMBO:
				switch (*p) {
					case '\t':
						rs = GRS_FILE_STATE;
						p++;
						break;
					case '\n':
						rs = GRS_DONE;
						p++;
						break;
					default:
						rs = GRS_SELECTED_COUNT;
						break;
				}
				break;

			/* Have to save selection and type until we get the name */
			case GRS_FILE_STATE:
				string = up_to_delimiter(&p, ' ');
				selection = map_selection_state(*string);
				file_rank++;
				rs = GRS_FILE_TYPE;
				break;

			case GRS_FILE_TYPE:
				string = up_to_delimiter(&p, ' ');
				type = map_file_type(*string);
				rs = GRS_FILE_NAME;
				break;

			case GRS_FILE_NAME:
				string = up_to_delimiter(&p, '\n');
				file_box_add(FILE_BOX(dl->file_box), string, type, selection,
						file_rank);
				rs = GRS_IN_LIMBO;
				break;

			default:
				g_error("Unexpected read state in process_glob_data.");
				break;
		}
	}

	exhibit_cull(e);
	exhibit_rearrange_and_show(e);
}


/* Resize the DListings if necessary. */
static void window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, Exhibit* e) {
	GSList* iter;
	DListing* dl;

	if (e->width_change) {
		/* Cycle through the DListings and set the new optimal width.  This
		   will make them optimize themselves to the window's width. */
		for (iter = e->dls; iter; iter = g_slist_next(iter)) {
			dl = iter->data;
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


gint main(gint argc, gchar **argv) {

	gtk_init(&argc, &argv);

	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	/* Warning/error logging must go through syslog, otherwise it won't be
	   seen. */
	g_log_set_handler(NULL,
			G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_MESSAGE |
			G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, syslogging, NULL);
	openlog_wrapped(g_get_prgname());

	GtkWidget* vbox;
	GtkWidget* scrolled_window;

	struct prefs v;

	/* This is pretty central -- it gets passed around a lot. */
	Exhibit	e;
	e.dls = NULL;
	e.term_win = g_string_new(NULL);
	
	/* Option defaults. */
	prefs_init(&v);
	parse_args(argc, argv, &v);

	/* Set the label font sizes. */
	file_box_set_sizing(v.font_size_modifier, v.show_icons);
	dlisting_set_sizing(v.font_size_modifier);

	/* Create window. */
	e.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(e.window), 2);
	gtk_window_set_title(GTK_WINDOW(e.window), "vg");
	gtk_window_set_default_size(GTK_WINDOW(e.window), 340, 420);
	e.width_change = 0;
	set_icons();

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

	/* The sandbox glob command line. */
	e.cmdline = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(e.cmdline), FALSE);
	gtk_widget_set_sensitive(e.cmdline, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), e.cmdline, FALSE, FALSE, 0);
	gtk_widget_show(e.cmdline);

	/* The DListing separator looks better if it's filled instead of sunken,
	   so use the text color.  This probably isn't the best way to do this. */
	GtkStyle* style = gtk_widget_get_default_style();
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

	/* Setup a watch for glob input. */
	GIOChannel* stdin_ioc;
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

	/* Pass the window ID back to vgd. */
	write_xwindow_id(e.window);

	gtk_main();

	return EXIT_SUCCESS;
}

