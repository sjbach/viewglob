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

#include "display-common.h"
#include "param-io.h"
#include <gdk/gdkx.h>

/* Set the application icons. */
void set_icons(void) {
#include "app_icons.h"

	GList* icons = NULL;

	/* Setup the application icons. */
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_16x16_inline, FALSE, NULL));
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_24x24_inline, FALSE, NULL));
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_32x32_inline, FALSE, NULL));
	icons = g_list_append(icons,
			gdk_pixbuf_new_from_inline(-1, icon_36x36_inline, FALSE, NULL));
	gtk_window_set_default_icon_list(icons);
}


/* Send the id of the given window's X window to seer. */
void write_xwindow_id(GtkWidget* gtk_window) {

	GdkWindow* gdk_window = gtk_window->window;
	GString* xwindow_string = g_string_new(NULL);

	if (gdk_window)
		g_string_printf(xwindow_string, "%lu", GDK_WINDOW_XID(gdk_window));
	else {
		g_warning("Couldn't find an id for the display's window.");
		xwindow_string = g_string_assign(xwindow_string, "0");
	}

	if (!put_param(STDOUT_FILENO, P_WIN_ID, xwindow_string->str)) {
		g_critical("Couldn't write window ID to stdout");
		exit(EXIT_FAILURE);
	}

	g_string_free(xwindow_string, TRUE);
}


gchar* up_to_delimiter(gchar** ptr, char c) {
	gchar* start = *ptr;
	while (**ptr != c)
		(*ptr)++;

	**ptr = '\0';
	(*ptr)++;
	return start;
}


/* Chooses a selection state based on the string's first char. */
FileSelection map_selection_state(gchar c) {
	switch (c) {
		case '-':
			return FS_NO;
		case '~':
			return FS_MAYBE;
		case '*':
			return FS_YES;
		default:
			g_warning("Unexpected selection state \"%c\".", c);
			return FS_NO;
	}
}


/* Chooses a file type based on the string's first char. */
FileType map_file_type(gchar c) {
	switch (c) {
		case 'r':
			return FT_REGULAR;
		case 'e':
			return FT_EXECUTABLE;
		case 'd':
			return FT_DIRECTORY;
		case 'y':
			return FT_SYMLINK;
		case 'b':
			return FT_BLOCKDEV;
		case 'c':
			return FT_CHARDEV;
		case 'f':
			return FT_FIFO;
		case 's':
			return FT_SOCKET;
		default:
			g_warning("Unexpected file type \"%c\".", c);
			return FT_REGULAR;
	}
}

