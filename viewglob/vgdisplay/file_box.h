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

#ifndef FILE_BOX_H
#define FILE_BOX_H

#include <gtk/gtk.h>
#include "wrap_box.h"
#include "file-types.h"

G_BEGIN_DECLS

/* --- type macros --- */
#define FILE_BOX_TYPE            (file_box_get_type())
#define FILE_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST( (obj), FILE_BOX_TYPE, FileBox))
#define FILE_BOX_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST( (klass), FILE_BOX_TYPE, FileBoxClass))
#define IS_FILE_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE( (obj), FILE_BOX_TYPE))
#define IS_FILE_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass), FILE_BOX_TYPE))
#define FILE_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS( (obj), FILE_BOX_TYPE, FileBoxClass))

/* --- typedefs --- */
typedef struct _FileBox FileBox;
typedef struct _FileBoxClass FileBoxClass;
typedef struct _FItem FItem;

/* --- FileBox --- */
struct _FileBox {
	WrapBox   wbox;

	guint     optimal_width;
	gboolean  show_hidden_files;
	guint     file_display_limit;

	gboolean eat_size_requests;
	GSList*   fis;
};

struct _FileBoxClass {
	WrapBoxClass parent_class;
};

struct _FItem {
	GtkWidget*           widget;
	gchar*               name;
	FileType             type;
	FileSelection        selection;

	/* An FItem is "marked" if it's been seen after a begin_read. */
	gboolean             marked;
};


/* --- prototypes --- */
GType       file_box_get_type(void) G_GNUC_CONST;
GtkWidget*  file_box_new(void);
void        file_box_destroy(FileBox* fbox);
void        file_box_set_optimal_width(FileBox* fbox, guint optimal_width);
guint       file_box_get_optimal_width(FileBox* fbox);
int         file_box_add(FileBox* fbox, gchar* name, FileType type,
		FileSelection selection, gint rank);
void        file_box_begin_read(FileBox* fbox);
void        file_box_flush(FileBox* fbox);

void        file_box_set_icon(FileType type, GdkPixbuf* icon);
void        file_box_set_sizing(gint modifier);


G_END_DECLS

#endif  /* !FILE_BOX_H */

