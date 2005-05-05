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

#include "common.h"
#include "display-common.h"
#include "jump-resize.h"
#include "dircont.h"
#include "file_box.h"
#include "param-io.h"
#include "syslogging.h"

#include <string.h>
#include <gtk/gtk.h>


struct vgmini {
	GtkWidget* window;
	gint width_change;

	GtkWidget* cmdline;

	GSList* dcs;
	DirCont* active;

	GtkWidget* vbox;

	GString* term_win;
	gboolean jump_resize;
};


static gint cmp_dircont_same_name(gconstpointer a, gconstpointer b);
static gint cmp_dircont_same_rank(gconstpointer a, gconstpointer b);

static gboolean receive_data(GIOChannel* source, GIOCondition condition,
		gpointer data);
static void process_glob_data(gchar* buf, gsize bytes, struct vgmini* vg);

static DirCont* add_dircont(struct vgmini* vg, gchar* name, gint rank,
		gchar* selected, gchar* total, gchar* hidden);
static void unmark_all_dirconts(struct vgmini* vg);
static void cull_dcs(struct vgmini* vg);
static void rearrange_and_show(struct vgmini* vg);
static void update_dc(struct vgmini* vg, DirCont* dc, gboolean setting);
static void activate_dc(struct vgmini* vg, gboolean next);

static void do_nav(struct vgmini* vg, const gchar* order);
static void set_cmd(struct vgmini* vg, const gchar* string);

static gboolean window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, struct vgmini* vg);
static void window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, struct vgmini* vg);
gboolean window_focus_event(GtkWidget* widget, GdkEventFocus* event,
		struct vgmini* vg);
gboolean button_release_event(GtkWidget* header, GdkEventButton* event,
		struct vgmini* vg);


gint main(gint argc, char** argv) {

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

	struct prefs prfs;
	prefs_init(&prfs);
	parse_args(argc, argv, &prfs);

	struct vgmini vg;
	vg.dcs = NULL;
	vg.active = NULL;
	vg.width_change = 0;
	vg.term_win = g_string_new(NULL);
	vg.jump_resize = prfs.jump_resize;

	/* vgmini keeps sizes a little smaller than vgclassic. */
	file_box_set_sizing(prfs.font_size_modifier - 1, prfs.show_icons);
	dircont_set_sizing(prfs.font_size_modifier - 1);

	/* Toplevel window. */
	vg.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(vg.window), 0);
	gtk_window_set_title(GTK_WINDOW(vg.window), "vg");
	gtk_window_set_default_size(GTK_WINDOW(vg.window), 240, 316);
	set_icons();

	/* VBox for the DirConts. */
	vg.vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vg.vbox);

	/* Read-only Viewglob command line.  This only appears when the display
	   is the active window. */
	vg.cmdline = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(vg.cmdline), FALSE);
	gtk_widget_set_sensitive(vg.cmdline, FALSE);
	gtk_widget_hide(vg.cmdline);

	gtk_container_add(GTK_CONTAINER(vg.window), vg.vbox);
	gtk_box_pack_end(GTK_BOX(vg.vbox), vg.cmdline, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(vg.window), "configure-event",
			G_CALLBACK(window_configure_event), &vg);
	g_signal_connect(G_OBJECT(vg.window), "size-allocate",
			G_CALLBACK(window_allocate_event), &vg);
	g_signal_connect(G_OBJECT(vg.window), "delete_event",
			G_CALLBACK(window_delete_event), NULL);
	g_signal_connect(G_OBJECT(vg.window), "key-press-event",
			G_CALLBACK(window_key_press_event), NULL);

	g_signal_connect(G_OBJECT(vg.window), "focus-in-event",
			G_CALLBACK(window_focus_event), &vg);
	g_signal_connect(G_OBJECT(vg.window), "focus-out-event",
			G_CALLBACK(window_focus_event), &vg);

	GIOChannel* stdin_ioc;
	if ( (stdin_ioc = g_io_channel_unix_new(STDIN_FILENO)) == NULL) {
		g_critical("Couldn't create IOChannel from stdin");
		exit(EXIT_FAILURE);
	}
	g_io_channel_set_encoding(stdin_ioc, NULL, NULL);
	g_io_channel_set_flags(stdin_ioc, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(stdin_ioc, G_IO_IN, receive_data, &vg);

	gtk_widget_show(vg.window);

	/* Pass the window ID back to vgd. */
	write_xwindow_id(vg.window);

	gtk_main();

	return EXIT_SUCCESS;
}


/* Receive data from vgd. */
static gboolean receive_data(GIOChannel* source, GIOCondition condition,
		gpointer data) {

	enum parameter param;
	gchar* value;

	struct vgmini* vg = data;
	int fd = g_io_channel_unix_get_fd(source);
	
	if (get_param(fd, &param, &value)) {
		switch (param) {

			case P_ORDER:
				if (STREQ(value, "refocus")) {
					if (vg->jump_resize) {
						if (jump_and_resize(vg->window, vg->term_win->str))
							raise_wrapped(vg->window, vg->term_win->str);
						else
							refocus_wrapped(vg->window, vg->term_win->str);
					}
					else
						refocus_wrapped(vg->window, vg->term_win->str);
				}
				else
					do_nav(vg, value);
				break;

			case P_CMD:
				set_cmd(vg, value);
				break;

			case P_WIN_ID:
				if (!STREQ(vg->term_win->str, value)) {

					vg->term_win = g_string_assign(vg->term_win, value);

					if (vg->jump_resize) {
						if (jump_and_resize(vg->window, vg->term_win->str))
							raise_wrapped(vg->window, vg->term_win->str);
						else
							refocus_wrapped(vg->window, vg->term_win->str);
					}
					else
						raise_wrapped(vg->window, value);
					
				}
				break;

			case P_MASK:
				dircont_set_mask_string(vg->active, value);
				break;

			case P_DEVELOPING_MASK:
				dircont_set_dev_mask_string(vg->active, value);
				break;

			case P_STATUS:
				//FIXME
				break;

			case P_VGEXPAND_DATA:
				if (*value != '\0')
					process_glob_data(value, strlen(value), vg);
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
static void process_glob_data(gchar* buf, gsize bytes, struct vgmini* vg) {

	enum glob_read_state rs;

	gchar* string = NULL;
	gchar* selected = NULL;
	gchar* total = NULL;
	gchar* hidden = NULL;

	static FileType type = FT_REGULAR;
	static FileSelection selection = FS_NO;

	gint dir_rank = -1;
	gint file_rank = -1;
	DirCont* dc = NULL;

	gchar* p;

	p = buf;
	rs = GRS_DONE;
	while (p < buf + bytes) {

		switch (rs) {
			case GRS_DONE:
				dir_rank = -1;
				unmark_all_dirconts(vg);
				rs = GRS_SELECTED_COUNT;
				break;

			case GRS_SELECTED_COUNT:
				selected = up_to_delimiter(&p, ' ');
				dir_rank++;
				rs = GRS_FILE_COUNT;
				break;

			case GRS_FILE_COUNT:
				total = up_to_delimiter(&p, ' ');
				rs = GRS_HIDDEN_COUNT;
				break;

			case GRS_HIDDEN_COUNT:
				hidden = up_to_delimiter(&p, ' ');
				rs = GRS_DIR_NAME;
				break;

			/* Get dc, the DirCont we're currently working on, from here. */
			case GRS_DIR_NAME:
				string = up_to_delimiter(&p, '\n');
				dc = add_dircont(vg, string, dir_rank, selected,
						total, hidden);
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
				dc->score += file_box_add(FILE_BOX(dc->file_box),
						string, type, selection, file_rank);
				rs = GRS_IN_LIMBO;
				break;

			default:
				g_error("Unexpected read state in process_glob_data.");
				break;
		}
	}

	cull_dcs(vg);
	rearrange_and_show(vg);
}

gboolean button_release_event(GtkWidget* header, GdkEventButton* event,
		struct vgmini* vg) {

	GSList* iter;
	DirCont* dc;
	gboolean found = FALSE;

	/* Find the dc with this header. */
	if (vg->dcs) {
		for (iter = vg->dcs; iter; iter = g_slist_next(iter)) {
			dc = iter->data;
			if (dc->header == header) {
				found = TRUE;
				break;
			}
		}
	}

	/* Activate it, and deactivate the old active dc. */
	if (found && vg->active != dc) {
		update_dc(vg, dc, TRUE);
		if (vg->active)
			update_dc(vg, vg->active, FALSE);
		vg->active = dc;
	}

	return FALSE;
}

static DirCont* add_dircont(struct vgmini* vg, gchar* name, gint rank,
		gchar* selected, gchar* total, gchar* hidden) {

	DirCont* dc;
	GSList* search_result;

	gboolean is_pwd = *name == PWD_CHAR;

	/* If this directory is PWD, set it as the title of the window. */
	if (is_pwd) {
		name++;
		gchar* new_title = g_strconcat("vg ", name, NULL);
		gtk_window_set_title(GTK_WINDOW(vg->window), new_title);
		g_free(new_title);
	}

	search_result = g_slist_find_custom(vg->dcs, name,
			cmp_dircont_same_name);

	if (search_result) {

		/* It's a known DirCont. */
		dc = search_result->data;
		dircont_set_counts(dc, selected, total, hidden);
		dircont_set_pwd(dc, is_pwd);
		dircont_mark(dc, rank);

		/* We'll be reading these next, at which point they'll be remarked. */
		file_box_begin_read(FILE_BOX(dc->file_box));
	}
	else {

		/* It's a new DirCont. */
		dc = DIRCONT(dircont_new());
		dircont_set_name(dc, name);
		dircont_set_counts(dc, selected, total, hidden);
		dircont_set_pwd(dc, is_pwd);

		/* Clicking the header activates it.  This must be done on this level
		   rather than in dircont.c because the other dcs must be
		   deactivated. */
		g_signal_connect(G_OBJECT(dc->header), "button-release-event",
				G_CALLBACK(button_release_event), vg);

		/* Set optimal width as the width of the listings vbox. */
		if (vg->active) {
			dircont_set_optimal_width(dc,
					vg->active->file_box->allocation.width);
		}
		else
			dircont_set_optimal_width(dc, 240); //FIXME
		dircont_mark(dc, rank);
		dc->score += 100;
		vg->dcs = g_slist_append(vg->dcs, dc);
	}

	return dc;
}


/* Remove DirConts and their widgets if they're no longer marked for
   showing. */
static void cull_dcs(struct vgmini* vg) {
	GSList* iter;
	DirCont* dc;

	iter = vg->dcs;
	while (iter) {
		dc = iter->data;

		if (dc->marked)
			iter = g_slist_next(iter);
		else {
			/* Take no prisoners. */
			GSList* tmp;
			tmp = iter;
			iter = g_slist_next(iter);
			vg->dcs = g_slist_delete_link(vg->dcs, tmp);
			dircont_free(dc);
			if (vg->active == dc)
				vg->active = NULL;
		}
	}
}


static void unmark_all_dirconts(struct vgmini* vg) {
	GSList* iter;
	DirCont* dc;

	for (iter = vg->dcs; iter; iter = g_slist_next(iter)) {
		dc = iter->data;
		dc->marked = FALSE;
		dc->score = 0;
	}
}


static void rearrange_and_show(struct vgmini* vg) {
	gint next_rank = 0;
	DirCont* dc;
	GSList* iter;

	DirCont* highest = vg->dcs->data;
	while ( (iter = g_slist_find_custom(vg->dcs, GINT_TO_POINTER(next_rank),
					cmp_dircont_same_rank)) ) {
		dc = iter->data;

		/* Commit the updates to the file box. */
		file_box_flush(FILE_BOX(dc->file_box));

		/* Ordering */
		if (dircont_is_new(dc)) {
			gtk_box_pack_start(GTK_BOX(vg->vbox), GTK_WIDGET(dc),
					FALSE, FALSE, 0);
			gtk_box_reorder_child(GTK_BOX(vg->vbox), GTK_WIDGET(dc),
					next_rank);
			gtk_widget_show(GTK_WIDGET(dc));
		}
		else {
			if (dc->rank != dc->old_rank) {
				gtk_box_reorder_child(GTK_BOX(vg->vbox), GTK_WIDGET(dc),
						next_rank);
			}
		}

		/* Restricted directories can't be active. */
		if (dc->is_restricted)
			dc->score = -1;

		if (dc->score >= highest->score)
			highest = dc;

		next_rank++;
	}

	/* Always use the previous active dc if there's a tie. */
	if (!vg->active || highest->score > vg->active->score)
		vg->active = highest;

	if (vg->dcs) {
		for (iter = vg->dcs; iter; iter = g_slist_next(iter)) {
			dc = iter->data;
			update_dc(vg, dc, dc == vg->active);
		}
	}
}


static void update_dc(struct vgmini* vg, DirCont* dc, gboolean setting) {
	dircont_set_active(dc, setting);
	gtk_box_set_child_packing(
			GTK_BOX(vg->vbox),
			GTK_WIDGET(dc),
			setting, setting, 0, GTK_PACK_START);
	dircont_scroll_to_changed(dc);
	dircont_repaint_header(dc);
}


static void do_nav(struct vgmini* vg, const gchar* order) {
	if (STREQ(order, "pgup"))
		dircont_nav(vg->active, DCN_PGUP);
	else if (STREQ(order, "pgdown"))
		dircont_nav(vg->active, DCN_PGDOWN);
	else if (STREQ(order, "up"))
		activate_dc(vg, FALSE);
	else if (STREQ(order, "down"))
		activate_dc(vg, TRUE);
	else {
		g_warning("Unexpected order: \"%s\"", order);
		return;
	}
}


/* Activate the next or previous DirCont (and deactivate the current one). */
static void activate_dc(struct vgmini* vg, gboolean next) {

	if (!vg->active)
		return;

	DirCont* new_active = NULL;
	GSList* search;

	gint rank = vg->active->rank;
	gboolean found = FALSE;

	while ( (rank = (next ? rank + 1 : rank - 1)) >= 0) {

		search = g_slist_find_custom(vg->dcs, GINT_TO_POINTER(rank),
				cmp_dircont_same_rank);

		if (search) {
			new_active = search->data;
			if (new_active->is_restricted)
				continue;
			else {
				found = TRUE;
				break;
			}
		}
		else {
			/* There're no more to look at. */
			return;
		}
	}

	/* Activate the found dc. */
	if (found) {
		update_dc(vg, new_active, TRUE);
		update_dc(vg, vg->active, FALSE);
		vg->active = new_active;
	}
}


/* Set the given string as the command line (after converting to utf8). */
static void set_cmd(struct vgmini* vg, const gchar* string) {
	gchar* cmdline_utf8 = g_filename_to_utf8(string, -1, NULL, NULL, NULL);
	gtk_entry_set_text(GTK_ENTRY(vg->cmdline), cmdline_utf8);
	g_free(cmdline_utf8);
}


/* Track the width of the toplevel window. */
static gboolean window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, struct vgmini* vg) {
	vg->width_change += event->width - window->allocation.width;
	return FALSE;
}


/* Resize the DirConts if necessary. */
static void window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, struct vgmini* vg) {
	GSList* iter;
	DirCont* dc;

	if (vg->width_change) {
		/* Cycle through the DirConts and set the new optimal width.  This
		   will make them optimize themselves to the window's width. */
		for (iter = vg->dcs; iter; iter = g_slist_next(iter)) {
			dc = iter->data;
			dircont_set_optimal_width(dc,
					((gint)dc->optimal_width) + vg->width_change);
		}
		vg->width_change = 0;
	}
}


/* Hide or show the command line. */
gboolean window_focus_event (GtkWidget* widget, GdkEventFocus* event,
		struct vgmini* vg) {

	if (event->in)
		gtk_widget_show(vg->cmdline);
	else
		gtk_widget_hide(vg->cmdline);

	return FALSE;
}


static gint cmp_dircont_same_name(gconstpointer a, gconstpointer b) {
	const DirCont* aa = a;
	const gchar* bb = b;

	return strcmp(aa->name->str, bb);
}

static gint cmp_dircont_same_rank(gconstpointer a, gconstpointer b) {
	const DirCont* aa = a;
	const gint bb = GPOINTER_TO_INT(b);

	if (aa->rank == bb)
		return 0;
	else if (aa->rank > bb)
		return 1;
	else
		return -1;
}

