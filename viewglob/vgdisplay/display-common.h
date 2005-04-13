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


#ifndef DISPLAY_COMMON_H
#define DISPLAY_COMMON_H


#include "config.h"

#include "common.h"
#include "file-types.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

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


struct prefs {
	/* Options. */
	gboolean show_icons;
	gint font_size_modifier;
};


void set_icons(void);
void write_xwindow_id(GtkWidget* gtk_window);
gboolean resize_jump(GtkWidget* gtk_window, gchar* term_win_str);
gchar* up_to_delimiter(gchar** ptr, char c);
FileSelection map_selection_state(gchar c);
FileType map_file_type(gchar c);
gboolean window_key_press_event(GtkWidget* window, GdkEventKey* event,
		gpointer data);
gboolean window_delete_event(GtkWidget* widget, GdkEvent* event,
		gpointer data);
void prefs_init(struct prefs* v);
void parse_args(int argc, char** argv, struct prefs* v);

G_END_DECLS

#endif

