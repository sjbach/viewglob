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

#ifndef VGCLASSIC_H
#define VGCLASSIC_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS


struct prefs {
	/* Options. */
	gboolean show_icons;
	gint font_size_modifier;
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

G_END_DECLS

#endif /* !VGCLASSIC_H */

