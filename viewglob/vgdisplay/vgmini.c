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
#include "dircont.h"
#include "file_box.h"
#include "param-io.h"

#include <string.h>
#include <gtk/gtk.h>


struct vgmini {
	GtkWidget* window;
	gint width_change;

	GtkWidget* cmdline;

	GSList* dcs;
	DirCont* active;

	GtkWidget* vbox;

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
static void do_order(struct vgmini* vg, const gchar* order);
static void update_dc(struct vgmini* vg, DirCont* dc, gboolean setting);
static void activate_dc(struct vgmini* vg, gboolean next);
static gboolean window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, struct vgmini* vg);
static void window_allocate_event(GtkWidget* window,
		GtkAllocation* allocation, struct vgmini* vg);
gint default_width(struct vgmini* vg);


gint default_width(struct vgmini* vg) {

	if (vg->dcs) {
		/* Use the size of an existing file_box. */
		GSList* iter;
		for (iter = vg->dcs; iter; iter = g_slist_next(iter)) {
			DirCont* dc = vg->dcs->data;
			if (GTK_WIDGET_VISIBLE(dc->file_box))
				return dc->file_box->allocation.width;
		}
	}

	/* Didn't work, so just use the size of the main vbox. */
	return vg->vbox->allocation.width;
}


gint main(gint argc, char** argv) {

	gtk_init(&argc, &argv);

	struct vgmini vg;
	vg.cmdline = gtk_drawing_area_new();
	vg.dcs = NULL;
	vg.active = NULL;
	vg.width_change = 0;

	/* Toplevel window. */
	vg.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(vg.window), 0);
	gtk_window_set_title(GTK_WINDOW(vg.window), "vg");
	gtk_window_set_default_size(GTK_WINDOW(vg.window), 260, 316);
	set_icons();

	/* VBox for the DirConts. */
	vg.vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vg.vbox);

	/* vgmini keeps sizes a little smaller than vgclassic. */
	file_box_set_sizing(-1);
	dircont_set_sizing(-1);

	gtk_container_add(GTK_CONTAINER(vg.window), vg.vbox);

	g_signal_connect(G_OBJECT(vg.window), "configure-event",
			G_CALLBACK(window_configure_event), &vg);
	g_signal_connect(G_OBJECT(vg.window), "size-allocate",
			G_CALLBACK(window_allocate_event), &vg);
	g_signal_connect(G_OBJECT(vg.window), "delete_event",
			G_CALLBACK(window_delete_event), NULL);
	g_signal_connect(G_OBJECT(vg.window), "key-press-event",
			G_CALLBACK(window_key_press_event), NULL);
//	g_signal_connect(G_OBJECT(area), "expose-event",
//			G_CALLBACK(area_expose_event), NULL);

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
				do_order(vg, value);
				break;

			case P_CMD:
//				exhibit_set_cmd(vg, value);
				break;

			case P_WIN_ID:
				/* This display doesn't use the window id. */
				break;

			case P_MASK:
				dircont_set_mask_string(vg->active, value);
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

	gchar* string;
	gchar* selected;
	gchar* total;
	gchar* hidden;

	static FileType type;
	static FileSelection selection;

	gint dir_rank;
	gint file_rank;
	DirCont* dc;

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
		/* Set optimal width as the width of the listings vbox. */
		if (vg->active) {
			dircont_set_optimal_width(dc,
					vg->active->file_box->allocation.width);
		}
		else {
			dircont_set_optimal_width(dc, 240); //FIXME
		}
//		dircont_set_optimal_width(dc, vg->listings_box->allocation.width);
//		dircont_set_optimal_width(dc, 240); //FIXME
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
	dircont_repaint_header(dc);
}


static void do_order(struct vgmini* vg, const gchar* order) {
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

	DirCont* new_active;
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
//		g_printerr("(width change: %d)", vg->width_change);
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



#if 0
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


/* Track the width of the toplevel window. */
static gboolean window_configure_event(GtkWidget* window,
		GdkEventConfigure* event, struct vgmini* vg) {
	vg->width_change += event->width - window->allocation.width;
	return FALSE;
}
#endif





/*
Window:
	GtkDrawingArea
		- For command line
		- Hides when window not active
	GtkVBox
		Dlistings
	GtkStatusBar
		- For current mask
Developing Mask draws on top of the header of the first DL.

DListing equivalent:
	Vbox
		GtkDrawingArea for full header
			- Expose event
			- Highlight event
			- Lose highlight event
		ScrolledWindow <-- hide when not active
			FileBox

   */

