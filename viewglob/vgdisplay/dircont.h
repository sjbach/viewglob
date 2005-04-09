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

#ifndef DIRCONT_H
#define DIRCONT_H

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

/* --- type macros --- */
#define DIRCONT_TYPE \
	(dircont_get_type())
#define DIRCONT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), DIRCONT_TYPE, DirCont))
#define DIRCONT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), DIRCONT_TYPE, DirContClass))
#define IS_DIRCONT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), DIRCONT_TYPE))
#define IS_DIRCONT_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), DIRCONT_TYPE))
#define DIRCONT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), DIRCONT_TYPE, DirContClass))

/* -- enums -- */
typedef enum _DirContNav DirContNav;

enum _DirContNav {
	DCN_PGUP,
	DCN_PGDOWN,
};

/* --- typedefs --- */
typedef struct _DirCont DirCont;
typedef struct _DirContClass DirContClass;

/* --- DirCont --- */
struct _DirCont {
	GtkVBox vbox;
	guint optimal_width;

	GString* name;
	gint rank;
	gint old_rank;
	gboolean marked;

	GString* selected;
	GString* total;
	GString* hidden;

	GtkWidget* header;
	GtkWidget* paint_event_box;
	GtkWidget* scrolled_window;
	GtkWidget* file_box;

	gboolean is_pwd;
	gboolean is_restricted;
	gboolean is_active;
	gboolean is_highlighted;

	gint score;

	PangoLayout* name_layout;
	PangoLayout* counts_layout;
};

struct _DirContClass {
	GtkVBoxClass parent_class;
};

/* --- prototypes --- */
GType dircont_get_type(void) G_GNUC_CONST;
GtkWidget* dircont_new(void);
void dircont_destroy(DirCont* dc);
void dircont_set_optimal_width(DirCont* dc, gint width);
void dircont_set_mask_string(DirCont* dc, const gchar* mask_str);
void dircont_set_name(DirCont* dc, const gchar* name);
void dircont_set_counts(DirCont* dc,
		const gchar* selected, const gchar* total, const gchar* hidden);
void dircont_repaint_header(DirCont* dc);
void dircont_mark(DirCont* d, gint rank);
void dircont_set_active(DirCont* dc, gboolean setting);
void dircont_set_pwd(DirCont* dc, gboolean setting);
gboolean dircont_is_new(const DirCont* dc);
void dircont_free(DirCont* dc);
void dircont_set_sizing(gint modifier);
void dircont_nav(DirCont* dc, DirContNav nav);

G_END_DECLS

#endif /* !DIRCONT_H */

