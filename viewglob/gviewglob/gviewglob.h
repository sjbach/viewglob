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

#ifndef GVIEWGLOB_H
#define GVIEWGOB_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "file_box.h"
#include "dlisting.h"

#define GVIEWGLOB_VERSION "0.8.2"
#define GVIEWGLOB_RELEASE_DATE "August 23, 2004"

G_BEGIN_DECLS

struct viewable_preferences {
	GdkPixbuf* show_hidden_pixbuf;
	GdkPixbuf* show_all_pixbuf;

	GdkColor* separator_color;

	/* Options */
	gboolean show_icons;
	gboolean show_hidden_files;
	guint file_display_limit;

	/* Input Fifos */
	gchar* glob_fifo;
	gchar* cmd_fifo;
};


typedef struct _Exhibit Exhibit;
struct _Exhibit {
	GSList* dl_slist;    /* This is for DListing structs. */
	GtkWidget* listings_box;  /* This is the vbox holding the dir/file listings. */

	GtkWidget* cmdline;           /* The entry holding the cmdline. */

	gint optimal_width;  /* Expected width of the listings vbox. */
};


/* Used in the read_string function. */
struct holdover {
	GString* string;
	gboolean has_holdover;
};


enum read_state {
	RS_DONE,
	RS_CMD,
	RS_SELECTED_COUNT, 
	RS_FILE_COUNT,
	RS_HIDDEN_COUNT,
	RS_DIR_NAME,
	RS_IN_LIMBO,     /* Either Input ends or another file follows. */
	RS_FILE_STATE,
	RS_FILE_TYPE,
	RS_FILE_NAME,
};

G_END_DECLS

#endif /* !GVIEWGLOB_H */
