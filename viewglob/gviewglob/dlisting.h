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

#ifndef DLISTING_H
#define DLISTING_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _DListing DListing;
struct _DListing {
	GString* name;
	gint rank;
	gint old_rank;
	gboolean marked;
	GString* selected_count;
	GString* total_count;
	GString* hidden_count;

	GtkWidget* widget;
	GtkWidget* name_label;
	GtkWidget* count_label;

	GtkWidget* menu;

	GtkWidget* file_box;
};


DListing*  dlisting_new(const GString* name, gint rank, const GString* selected_count, const GString* total_count, const GString* hidden_count, gint width);
void       dlisting_mark(DListing* d, gint rank);
void       dlisting_update_file_counts(DListing* dl, const GString* selected_count, const GString* total_count, const GString* hidden_count);
void       dlisting_reset_file_count_label(DListing* dl);
gboolean   dlisting_is_new(const DListing* dl);
void       dlisting_free(DListing* dl);

G_END_DECLS

#endif /* !DLISTING_H */

