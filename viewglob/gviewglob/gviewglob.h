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
#define GVIEWGLOB_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

struct viewable_preferences {
	/* Options */
	gboolean show_icons;
	gboolean show_hidden_files;
	guint file_display_limit;

	/* Input Fifos */
	gchar* glob_fifo;
	gchar* cmd_fifo;
};


/* Used in the read_string function. */
struct holdover {
	GString* string;
	gboolean has_holdover;
};

enum glob_read_state {
	GRS_DONE,
	GRS_CMD,
	GRS_SELECTED_COUNT, 
	GRS_FILE_COUNT,
	GRS_HIDDEN_COUNT,
	GRS_DIR_NAME,
	GRS_IN_LIMBO,     /* Either input ends or another file or dir follows. */
	GRS_FILE_STATE,
	GRS_FILE_TYPE,
	GRS_FILE_NAME,
};

enum cmd_read_state {
	CRS_DONE,
	CRS_CMD,
	CRS_ORDER,
};

G_END_DECLS

#endif /* !GVIEWGLOB_H */
