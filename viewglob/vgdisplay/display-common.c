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

#include "x11-stuff.h"
#include "display-common.h"
#include "param-io.h"
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>


static gboolean get_win_geometry(Display* Xdisplay, Window win, gint* x,
		gint* y, guint* w, guint* h);
static void get_decorations(GdkWindow* gdk_win, gint* left, gint* right,
		gint* top, gint* bottom);


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


#define WIDTH_VERT  240
#define HEIGHT_HORZ 150
gboolean resize_jump(GtkWidget* gtk_window, gchar* term_win_str) {
	g_return_val_if_fail(gtk_window != NULL, FALSE);
	g_return_val_if_fail(term_win_str != NULL, FALSE);

	if (!gtk_window->window)
		return FALSE;

	GdkWindow* gdk_win = gtk_window->window;

	Window term_win;
	Window me_win;

	if ((term_win = strtoul(term_win_str, NULL, 10)) == ULONG_MAX) {
		g_warning("Window ID out of bounds: %s", term_win_str);
		return FALSE;
	}
	me_win = GDK_WINDOW_XID(gdk_win);

	Display* Xdisplay = GDK_DRAWABLE_XDISPLAY(gdk_win);

	gint left, right, top, bottom;
	get_decorations(gdk_win, &left, &right, &top, &bottom);

	gint term_x, term_y;
	guint term_w, term_h;

	gint me_x, me_y;
	guint me_w, me_h;

	/* Get the dimensions of the desktop. */
	gint screen_width = gdk_screen_width();
	gint screen_height = gdk_screen_height();

	/* Get the window geometries. */
	if (!get_win_geometry(Xdisplay, term_win, &term_x, &term_y, &term_w,
				&term_h) ||
			!get_win_geometry(Xdisplay, me_win, &me_x, &me_y, &me_w, &me_h))
		return FALSE;

	/* Apply assumed decoration sizes. */
	term_x -= left;
	term_y -= top;
	term_w += left + right;
	term_h += top + bottom;
	me_x -= left;
	me_y -= top;
	me_w += left + right;
	me_h += top + bottom;

	gboolean moved = FALSE;

	if (term_x >= 0 && term_y >= 0) {

		/* Left */
		if ( (term_x - (gint)WIDTH_VERT >= 0) &&
				(term_y + (gint)term_h <= screen_height) ) {
			me_x = term_x - (gint)WIDTH_VERT;
			me_y = term_y;
			me_w = WIDTH_VERT - left - right;
			me_h = term_h - top - bottom;
			moved = TRUE;
		}
		/* Right */
		else if ( (term_x + (gint)term_w + (gint)WIDTH_VERT <= screen_width) &&
				(term_y + (gint)term_h <= screen_height) ) {
			me_x = term_x + (gint)term_w;
			me_y = term_y;
			me_w = WIDTH_VERT - left - right;
			me_h = term_h - top - bottom;
			moved = TRUE;
		}
		/* Bottom */
		else if ( (term_x + (gint)term_w <= screen_width) &&
				(term_y + (gint)term_h + (gint)HEIGHT_HORZ <= screen_height) ) {
			me_x = term_x;
			me_y = term_y + (gint)term_h;
			me_w = term_w - left - right;
			me_h = HEIGHT_HORZ - top - bottom;
			moved = TRUE;
		}
		/* Top */
		else if ( (term_x + (gint)term_w <= screen_width) &&
				(term_y - (gint)HEIGHT_HORZ >= 0) ) {
			me_x = term_x;
			me_y = term_y - (gint)HEIGHT_HORZ;
			me_w = term_w - left - right;
			me_h = HEIGHT_HORZ - top - bottom;
			moved = TRUE;
		}
	}

#if 0
	/* Moving without resizing: */
	/* First try aligning to the left. */
	if ( (term_x - (gint)me_w >= 0) &&
			(term_y + (gint)me_h <= screen_height) ) {
		XMoveWindow(Xdisplay, me_win, term_x - me_w, term_y);
		moved = TRUE;
	}
	/* Next aligning to the right. */
	else if ( (term_x + (gint)term_w + (gint)me_w <= screen_width) &&
			(term_y + (gint)me_h <= screen_height)) {
		XMoveWindow(Xdisplay, me_win, term_x + term_w, term_y);
		moved = TRUE;
	}
	/* Next try aligning to bottom. */
	else if ( (term_x + (gint)me_w <= screen_width) &&
			(term_y + (gint)term_h + (gint)me_h <= screen_height)) {
		XMoveWindow(Xdisplay, me_win, term_x, term_y + term_h);
		moved = TRUE;
	}
#endif

	if (moved) {
		gdk_window_move_resize(gdk_win, me_x, me_y, me_w, me_h);
		focus_window(Xdisplay, me_win, FALSE);
		get_win_geometry(Xdisplay, me_win, &term_x, &term_y, &term_w,
				&term_h);
	}
	return TRUE;
}


/* Figure out the size of the window decorations (making assumptions) */
static void get_decorations(GdkWindow* gdk_win, gint* left, gint* right,
		gint* top, gint* bottom) {

	gint outside_x, outside_y;
	gint inside_x, inside_y;
	gdk_window_get_root_origin(gdk_win, &inside_x, &inside_y);
	gdk_window_get_origin(gdk_win, &outside_x, &outside_y);

	GdkRectangle frame;
	gdk_window_get_frame_extents(gdk_win, &frame);

	gint width, height;
	gdk_drawable_get_size(GDK_DRAWABLE(gdk_win), &width, &height);

	*left = outside_x - inside_x;
	*right = frame.width - width - *left;
	*top = outside_y - inside_y;
	*bottom = frame.height - height - *top;
}


static gboolean get_win_geometry(Display* Xdisplay, Window win, gint* x,
		gint* y, guint* w, guint* h) {

	Window root_win;
	Window garbage;

	guint border, depth;

	switch (XGetGeometry(Xdisplay, win, &root_win, x, y, w, h, &border,
				&depth)) {
		case BadDrawable:
		case BadWindow:
			g_warning("Error while getting terminal window attributes");
			return FALSE;
		default:
			break;
	}

	if (!XTranslateCoordinates(Xdisplay, win, root_win, 0, 0, x, y,
				&garbage)) {
		g_warning("Couldn't translate coordinates");
		return FALSE;
	}

	return TRUE;
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


/* End the program. */
gboolean window_delete_event(GtkWidget* widget, GdkEvent* event,
		gpointer data) {
	gtk_main_quit();
	return FALSE;
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

