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
#include "file_box.h"
#include "vgclassic.h"
#include "exhibit.h"
#include <string.h>    /* For strcmp */

/* Prototypes. */
static gint cmp_dlisting_same_name(gconstpointer a, gconstpointer b);
static gint cmp_dlisting_same_rank(gconstpointer a, gconstpointer b);

static gint cmp_dlisting_same_name(gconstpointer a, gconstpointer b) {
	const DListing* aa = a;
	const gchar* bb = b;

	return strcmp(aa->name->str, bb);
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


DListing* exhibit_add(Exhibit* e, gchar* name, gint rank,
		gchar* selected_count, gchar* total_count, gchar* hidden_count) {

	DListing* dl;
	GSList* search_result;

	search_result = g_slist_find_custom(e->dl_slist, name,
			cmp_dlisting_same_name);

	if (search_result) {

		/* It's a known DListing. */
		dl = search_result->data;
		dlisting_set_file_counts(dl, selected_count, total_count,
				hidden_count);
		dlisting_mark(dl, rank);

		/* We'll be reading these next, at which point they'll be remarked. */
		file_box_begin_read(FILE_BOX(dl->file_box));
	}
	else {

		/* It's a new DListing. */
		dl = DLISTING(dlisting_new());
		dlisting_set_name(dl, name);
		dlisting_set_file_counts(dl, selected_count, total_count,
				hidden_count);
		/* Set optimal width as the width of the listings vbox. */
		dlisting_set_optimal_width(dl, e->listings_box->allocation.width);
		dlisting_mark(dl, rank);
		e->dl_slist = g_slist_append(e->dl_slist, dl);
	}

	return dl;
}


void exhibit_rearrange_and_show(Exhibit* e) {
	gint next_rank = 1;
	DListing* dl;
	GSList* search_result;

	while ( (search_result = g_slist_find_custom(e->dl_slist, &next_rank,
					cmp_dlisting_same_rank)) ) {
		dl = search_result->data;

		/* Commit the updates to the file box. */
		file_box_flush(FILE_BOX(dl->file_box));

		/* Ordering */
		if (dlisting_is_new(dl)) {
			gtk_box_pack_start(GTK_BOX(e->listings_box), GTK_WIDGET(dl),
					FALSE, FALSE, 0);
			gtk_box_reorder_child(GTK_BOX(e->listings_box), GTK_WIDGET(dl),
					next_rank - 1);
			gtk_widget_show(GTK_WIDGET(dl));
		}
		else if (dl->rank != dl->old_rank) {
			gtk_box_reorder_child(GTK_BOX(e->listings_box), GTK_WIDGET(dl),
					next_rank - 1);
		}

		next_rank++;
	}

	/* To make the scrollbars rescale. */
	gtk_widget_queue_resize(e->listings_box);
}



/* Remove DListings and their widgets if they're no longer marked for
   showing. */
void exhibit_cull(Exhibit* e) {
	GSList* iter;
	DListing* dl;

	iter = e->dl_slist;
	while (iter) {
		dl = iter->data;

		if (dl->marked)
			iter = g_slist_next(iter);
		else {
			/* Take no prisoners. */
			GSList* tmp;
			tmp = iter;
			iter = g_slist_next(iter);
			e->dl_slist = g_slist_delete_link(e->dl_slist, tmp);
			dlisting_free(dl);
		}
	}
}


void exhibit_unmark_all(Exhibit* e) {
	GSList* iter;
	DListing* dl;

	for (iter = e->dl_slist; iter; iter = g_slist_next(iter)) {
		dl = iter->data;
		dl->marked = FALSE;
	}
}


void exhibit_do_order(Exhibit* e, gchar* order) {

	gdouble upper, lower, current, step_increment, page_increment, change;

	change = 0;
	current = gtk_adjustment_get_value(e->vadjustment);
	step_increment = e->vadjustment->step_increment;
	page_increment = e->vadjustment->page_increment;
	lower = e->vadjustment->lower;

	/* Otherwise we scroll down into a page of black. */
	upper = e->vadjustment->upper - page_increment - step_increment;

	if (STREQ(order, "up"))
		change = -step_increment;
	else if (STREQ(order, "down"))
		change = +step_increment;
	else if (STREQ(order, "pgup"))
		change = -page_increment;
	else if (STREQ(order, "pgdown"))
		change = +page_increment;
	else {
		g_warning("Unexpected order in process_cmd_data.");
		return;
	}

	if (change) {
		gtk_adjustment_set_value(e->vadjustment,
				CLAMP(current + change, lower, upper));
		gtk_adjustment_value_changed(e->vadjustment);
	}
}

