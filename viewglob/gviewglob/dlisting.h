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

#ifndef DLISTING_H
#define DLISTING_H

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

/* --- type macros --- */
#define DLISTING_TYPE		     (dlisting_get_type())
#define DLISTING(obj)	         (G_TYPE_CHECK_INSTANCE_CAST ((obj), DLISTING_TYPE, DListing))
#define DLISTING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DLISTING_TYPE, DListingClass))
#define IS_DLISTING(obj)	     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DLISTING_TYPE))
#define IS_DLISTING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DLISTING_TYPE))
#define DLISTING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), DLISTING_TYPE, DListingClass))

/* --- typedefs --- */
typedef struct _DListing DListing;
typedef struct _DListingClass DListingClass;

/* --- DListing --- */
struct _DListing {
	GtkVBox   vbox;

	guint optimal_width;

	GString*  name;
	gint      rank;
	gint      old_rank;
	gboolean  marked;

	GString*  selected_count;
	GString*  total_count;
	GString*  hidden_count;

	GtkWidget* heading_event_box;
	GtkWidget* name_label;
	GtkWidget* count_label;
	GtkWidget* menu;
	GtkWidget* file_box;
};

struct _DListingClass {
	GtkVBoxClass parent_class;
};


/* --- prototypes --- */
GType       dlisting_get_type(void) G_GNUC_CONST;
GtkWidget*  dlisting_new(void);
void        dlisting_destroy(DListing* dl);
void        dlisting_set_name(DListing* dl, const gchar* name);
void        dlisting_set_file_counts(DListing* dl, const gchar* selected, const gchar* total, const gchar* hidden);
void        dlisting_set_optimal_width(DListing* dl, gint width);
void        dlisting_mark(DListing* d, gint rank);
gboolean    dlisting_is_new(const DListing* dl);
void        dlisting_free(DListing* dl);

void        dlisting_set_separator_color(GdkColor color);
void        dlisting_set_show_hidden_pixbuf(GdkPixbuf* pixbuf);
void        dlisting_set_show_all_pixbuf(GdkPixbuf* pixbuf);
void        dlisting_set_show_hidden_files(gboolean show_hidden);
void        dlisting_set_show_all_files(gboolean show_all);
void        dlisting_set_sizing(gint modifier);

G_END_DECLS

#endif /* !DLISTING_H */
