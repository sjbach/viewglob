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

#ifndef FILE_BOX_H
#define FILE_BOX_H

#include <gtk/gtk.h>
#include "wrap_box.h"

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
typedef enum _FileBoxOrdering FileBoxOrdering;
typedef struct _FItem FItem;
typedef enum _FileType FileType;
typedef enum _FileSelection FileSelection;
typedef enum _FileDisplayCategory FileDisplayCategory;

/* --- enumerations --- */
enum _FileBoxOrdering {
	FBO_LS,
	FBO_WIN,
};

enum _FileType {
	FT_REGULAR,
	FT_EXECUTABLE,
	FT_DIRECTORY,
	FT_BLOCKDEV,
	FT_CHARDEV,
	FT_FIFO,
	FT_SOCKET,
	FT_SYMLINK,
};

enum _FileSelection {
	FS_YES = GTK_STATE_SELECTED,
	FS_NO = GTK_STATE_NORMAL,
	FS_MAYBE = GTK_STATE_ACTIVE,
};

enum _FileDisplayCategory {
	FDC_INDETERMINATE,
	FDC_REVEAL,        /* Display regularly. */
	FDC_MASK,          /* Display only if selected (peek). */
};

/* --- FileBox --- */
struct _FileBox {
	WrapBox   wbox;

	guint     optimal_width;
	gboolean  show_hidden_files;
	guint     file_display_limit;

	guint n_files;
	guint n_displayed_files;
	guint file_max;

	GSList*   fi_slist;
};

struct _FileBoxClass {
	WrapBoxClass parent_class;
};

struct _FItem {
	GtkWidget*           widget;
	GString*             name;
	FileType             type;
	FileSelection        selection;

	FileDisplayCategory  disp_cat;
	gboolean             marked;      /* An FItem is "marked" if it's been seen after a begin_read. */
};


/* --- prototypes --- */
GType       file_box_get_type(void) G_GNUC_CONST;
GtkWidget*  file_box_new(void);
void        file_box_destroy(FileBox* fbox);
void        file_box_set_optimal_width(FileBox* fbox, guint optimal_width);
void        file_box_set_show_hidden_files(FileBox* fbox, gboolean show);
void        file_box_set_file_display_limit(FileBox* fbox, guint limit);
void        file_box_set_ordering(FileBoxOrdering fbo);
void        file_box_set_icon(FileType type, GdkPixbuf* icon);
gboolean    file_box_get_show_hidden_files(FileBox* fbox);
guint       file_box_get_file_display_limit(FileBox* fbox);
void        file_box_add(FileBox* fbox, GString* name, FileType type, FileSelection selection);
void        file_box_begin_read(FileBox* fbox);
void        file_box_flush(FileBox* fbox);


G_END_DECLS

#endif  /* !FILE_BOX_H */

