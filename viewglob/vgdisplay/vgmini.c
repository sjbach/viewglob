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
static void cull_dirconts(struct vgmini* vg);
static void rearrange_and_show(struct vgmini* vg);
//static gboolean window_delete_event(GtkWidget* widget, GdkEvent* event,
//		gpointer data);
//static gboolean window_configure_event(GtkWidget* window,
//		GdkEventConfigure* event, struct vgmini* vg);
//static void window_allocate_event(GtkWidget* window,
//		GtkAllocation* allocation, struct vgmini* vg);
//static gboolean window_key_press_event(GtkWidget* window,
//		GdkEventKey* event, gpointer data);
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

	/*
	GtkWidget* wdc = dircont_new();
	DirCont* dc = DIRCONT(wdc);
	dircont_set_name(dc, "Name3");
	gtk_widget_show(wdc);
	gtk_widget_hide(DIRCONT(wdc)->file_box->parent->parent);
	gtk_box_pack_start(GTK_BOX(vg.vbox), wdc, FALSE, FALSE, 0);
	wdc = dircont_new();
	dircont_set_name(dc, "Name");
	dircont_set_selected(dc, "selected");
	dircont_set_total(dc, "total");
	dircont_set_hidden(dc, "hidden");
	file_box_begin_read(FILE_BOX(dc->file_box));
	file_box_add(FILE_BOX(dc->file_box), "name", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name1", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name2", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name3", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name4", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name5", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name6", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name7", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name8", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name9", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name10", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name11", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name12", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name13", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name14", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name15", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name16", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name17", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "name18", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "blahblahblah", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "afileisme", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "alhkdjk12", FT_REGULAR, FS_NO, 0);
	file_box_add(FILE_BOX(dc->file_box), "121120ksa", FT_REGULAR, FS_NO, 0);
	file_box_flush(FILE_BOX(dc->file_box));
	dircont_set_optimal_width(dc, 240);
	gtk_widget_show(wdc);
	gtk_box_pack_start(GTK_BOX(vg.vbox), wdc, TRUE, TRUE, 0);
	wdc = dircont_new();
	dircont_set_name(dc, "Name2");
	gtk_widget_show(wdc);
	gtk_widget_hide(DIRCONT(wdc)->file_box->parent->parent);
	gtk_box_pack_start(GTK_BOX(vg.vbox), wdc, FALSE, FALSE, 0);
	*/




	gtk_container_add(GTK_CONTAINER(vg.window), vg.vbox);

	/*
	g_signal_connect(G_OBJECT(vg.window), "configure-event",
			G_CALLBACK(window_configure_event), &vg);
	g_signal_connect(G_OBJECT(vg.window), "size-allocate",
			G_CALLBACK(window_allocate_event), &vg);
			*/
//	g_signal_connect(G_OBJECT(vg.window), "delete_event",
//			G_CALLBACK(window_delete_event), NULL);
//	g_signal_connect(G_OBJECT(vg.window), "key-press-event",
//			G_CALLBACK(window_key_press_event), NULL);
//	g_signal_connect(G_OBJECT(area), "expose-event",
//			G_CALLBACK(area_expose_event), NULL);


//	vg.dcs = g_slist_prepend(vg.dcs, dc);

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
//				exhibit_do_order(vg, value);
				break;

			case P_CMD:
//				exhibit_set_cmd(vg, value);
				break;

			case P_WIN_ID:
				/* This display doesn't use the window id. */
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
				file_box_add(FILE_BOX(dc->file_box), string, type, selection,
						file_rank);
				rs = GRS_IN_LIMBO;
				break;

			default:
				g_error("Unexpected read state in process_glob_data.");
				break;
		}
	}

	cull_dirconts(vg);
	rearrange_and_show(vg);
}


static DirCont* add_dircont(struct vgmini* vg, gchar* name, gint rank,
		gchar* selected, gchar* total, gchar* hidden) {

	DirCont* dc;
	GSList* search_result;

	/* If this directory is PWD, set it as the title of the window. */
	if (*name == PWD_CHAR) {
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
		dircont_set_selected(dc, selected);
		dircont_set_total(dc, total);
		dircont_set_hidden(dc, hidden);
		dircont_mark(dc, rank);

		/* We'll be reading these next, at which point they'll be remarked. */
		file_box_begin_read(FILE_BOX(dc->file_box));
	}
	else {

		/* It's a new DirCont. */
		dc = DIRCONT(dircont_new());
		dircont_set_name(dc, name);
		dircont_set_selected(dc, selected);
		dircont_set_total(dc, total);
		dircont_set_hidden(dc, hidden);
		/* Set optimal width as the width of the listings vbox. */
//		dircont_set_optimal_width(dc, vg->listings_box->allocation.width);
		dircont_set_optimal_width(dc, 240); //FIXME
		dircont_mark(dc, rank);
		vg->dcs = g_slist_append(vg->dcs, dc);
	}

	return dc;
}


/* Remove DirConts and their widgets if they're no longer marked for
   showing. */
static void cull_dirconts(struct vgmini* vg) {
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
	}
}


static void rearrange_and_show(struct vgmini* vg) {
	gint next_rank = 0;
	DirCont* dc;
	GSList* iter;

	DirCont* prev_active = vg->active;
	while ( (iter = g_slist_find_custom(vg->dcs, &next_rank,
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
			vg->active = dc;
		}
		else {
			if (dc->rank != dc->old_rank) {
				gtk_box_reorder_child(GTK_BOX(vg->vbox), GTK_WIDGET(dc),
						next_rank);
			}
			if (prev_active == NULL)
				prev_active = dc;
		}

		next_rank++;
	}

	if (!vg->active)
		vg->active = prev_active;

	for (iter = vg->dcs; iter; iter = g_slist_next(iter)) {
		dc = iter->data;
		gboolean setting = (dc == vg->active);
		dircont_set_active(dc, setting);
		gtk_box_set_child_packing(
				GTK_BOX(vg->vbox),
				GTK_WIDGET(dc),
				setting, setting, 0, GTK_PACK_START);
	}

	/* To make the scrollbars rescale. */
//	gtk_widget_queue_resize(vg->vbox);
}





static gint cmp_dircont_same_name(gconstpointer a, gconstpointer b) {
	const DirCont* aa = a;
	const gchar* bb = b;

	return strcmp(aa->name->str, bb);
}

static gint cmp_dircont_same_rank(gconstpointer a, gconstpointer b) {
	const DirCont* aa = a;
	const gint* bb = b;

	if ( aa->rank == *bb )
		return 0;
	else if ( aa->rank > *bb )
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

