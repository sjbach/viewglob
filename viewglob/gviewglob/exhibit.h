/*
	Copyright (C) 2004, 2005 Stephen Bach
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

#ifndef EXHIBIT_H
#define EXHIBIT_H

#include "dlisting.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS


typedef struct _Exhibit Exhibit;
struct _Exhibit {
	GtkWidget* window;
	gboolean iconified;
	GSList* dl_slist;           /* This is for DListing structs. */
	GtkWidget* listings_box;    /* This is the vbox holding the dir/file listings. */
	GtkAdjustment* vadjustment; /* Vertical scrollbar. */
	gint width_change;          /* Change in window width that needs to be applied to the DListings. */

	GtkWidget* cmdline;         /* The entry holding the cmdline. */
};


DListing*  exhibit_add(Exhibit* e, GString* name, gint rank, GString* selected_count, GString* total_count, GString* hidden_count);
void       exhibit_unmark_all(Exhibit* e);
void       exhibit_cull(Exhibit* e);
void       exhibit_rearrange_and_show(Exhibit* e);
void       exhibit_do_order(Exhibit* e, GString* order);

G_END_DECLS

#endif /* !EXHIBIT_H */


