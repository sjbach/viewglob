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
#include <gdk/gdkkeysyms.h>

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


/* A key has been pressed -- write it to stdout. */
gboolean window_key_press_event(GtkWidget* window, GdkEventKey* event,
		gpointer data) {

	gsize  bytes_written;
	gchar* temp1;
	gchar* temp2;

	gboolean result = TRUE;

	switch (event->keyval) {
		case GDK_Home:
		case GDK_Left:
		case GDK_Up:
		case GDK_Right:
		case GDK_Down:
		case GDK_Page_Up:
		case GDK_Page_Down:
		case GDK_End:
			/* These keys we let the display interpret. */
			result = FALSE;
			break;

		default:
			/* The rest are passed to the terminal. */
			temp1 = g_malloc(2);
			*temp1 = event->keyval;
			*(temp1 + 1) = '\0';

			/* Convert out of utf8. */
			temp2 = g_locale_from_utf8(temp1, -1, NULL, &bytes_written, NULL);

			if (temp2) {
				if (event->state & GDK_CONTROL_MASK) {
					/* Control is being held.  Determine if it's a control
					   key and convert. */
					if (event->keyval >= 'a' && event->keyval <= 'z')
						*temp2 -= 96;
					else if (event->keyval >= '[' && event->keyval <= '_')
						*temp2 -= 64;
					else if (event->keyval == '@' || event->keyval == ' ')
						/* This will go out empty, but vgseer is smart enough
						   to interpret a value length of 0 as a NUL
						   character. */
						*temp2 = '\0';
				}

				if (!put_param(STDOUT_FILENO, P_KEY, temp2)) {
					g_critical("Couldn't write key to stdout");
					exit(EXIT_FAILURE);
				}

				g_free(temp2);
			}
			else
				result = FALSE;

			g_free(temp1);
			break;
	}

	return result;
}

