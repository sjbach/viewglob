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
#include "x11-stuff.h"
#include <gdk/gdkx.h>
#include <X11/Xlib.h>


static gboolean get_win_geometry(Display* Xdisplay, Window win, gint* x,
		gint* y, guint* w, guint* h);
static void get_decorations(GdkWindow* gdk_win, gint* left, gint* right,
		gint* top, gint* bottom);


#define WIDTH_VERT  240
#define HEIGHT_HORZ 150
gboolean jump_and_resize(GtkWidget* gtk_window, gchar* term_win_str) {
	g_return_val_if_fail(gtk_window != NULL, FALSE);
	g_return_val_if_fail(term_win_str != NULL, FALSE);

	if (!gtk_window->window)
		return FALSE;

	GdkWindow* gdk_win = gtk_window->window;

	Window term_win = str_to_win(term_win_str);
	Window me_win = GDK_WINDOW_XID(gdk_win);
	Display* Xdisplay = GDK_DRAWABLE_XDISPLAY(gdk_win);

	/*if (term_win == 0 || !is_visible(Xdisplay, me_win))*/
	if (term_win == 0)
		return FALSE;
	
	gint left, right, top, bottom;
	get_decorations(gdk_win, &left, &right, &top, &bottom);

	gint term_x, term_y, term_w, term_h;

	gint old_x, old_y, old_w, old_h;
	gint me_x, me_y, me_w, me_h;

	/* Get the dimensions of the desktop. */
	gint screen_width = gdk_screen_width();
	gint screen_height = gdk_screen_height();

	/* Get the window geometries. */
	if (!get_win_geometry(Xdisplay, term_win, &term_x, &term_y, &term_w,
				&term_h) ||
			!get_win_geometry(Xdisplay, me_win,
				&old_x, &old_y, &old_w, &old_h))
		return FALSE;

	/* Apply assumed decoration sizes. */
	term_x -= left;
	term_y -= top;
	term_w += left + right;
	term_h += top + bottom;
	me_x = old_x - left;
	me_y = old_y - top;
	me_w = old_w + left + right;
	me_h = old_h + top + bottom;

	gboolean move = FALSE;

	if (term_x >= 0 && term_y >= 0) {

		/* Left */
		if ( (term_x - (gint)WIDTH_VERT >= 0) &&
				(term_y + (gint)term_h <= screen_height) ) {
			me_x = term_x - (gint)WIDTH_VERT;
			me_y = term_y;
			me_w = WIDTH_VERT - left - right;
			me_h = term_h - top - bottom;
			move = TRUE;
		}
		/* Right */
		else if ( (term_x + (gint)term_w + (gint)WIDTH_VERT <= screen_width) &&
				(term_y + (gint)term_h <= screen_height) ) {
			me_x = term_x + (gint)term_w;
			me_y = term_y;
			me_w = WIDTH_VERT - left - right;
			me_h = term_h - top - bottom;
			move = TRUE;
		}
		/* Bottom */
		else if ( (term_x + (gint)term_w <= screen_width) &&
				(term_y + (gint)term_h + (gint)HEIGHT_HORZ <= screen_height) ) {
			me_x = term_x;
			me_y = term_y + (gint)term_h;
			me_w = term_w - left - right;
			me_h = HEIGHT_HORZ - top - bottom;
			move = TRUE;
		}
		/* Top */
		else if ( (term_x + (gint)term_w <= screen_width) &&
				(term_y - (gint)HEIGHT_HORZ >= 0) ) {
			me_x = term_x;
			me_y = term_y - (gint)HEIGHT_HORZ;
			me_w = term_w - left - right;
			me_h = HEIGHT_HORZ - top - bottom;
			move = TRUE;
		}
	}

	gulong* term_desktop = get_desktop(Xdisplay, term_win);
	gulong* me_desktop = get_desktop(Xdisplay, me_win);

	/* Determine if the new position is different from the old position. */
	gboolean changed = me_x + left != old_x
		|| me_y + top != old_y || me_w != old_w || me_h != old_h
		|| (term_desktop != NULL && me_desktop != NULL
			&& *me_desktop != *term_desktop);

#if 0
	/* Moving without resizing: */
	/* First try aligning to the left. */
	if ( (term_x - (gint)me_w >= 0) &&
			(term_y + (gint)me_h <= screen_height) ) {
		XMoveWindow(Xdisplay, me_win, term_x - me_w, term_y);
		move = TRUE;
	}
	/* Next aligning to the right. */
	else if ( (term_x + (gint)term_w + (gint)me_w <= screen_width) &&
			(term_y + (gint)me_h <= screen_height)) {
		XMoveWindow(Xdisplay, me_win, term_x + term_w, term_y);
		move = TRUE;
	}
	/* Next try aligning to bottom. */
	else if ( (term_x + (gint)me_w <= screen_width) &&
			(term_y + (gint)term_h + (gint)me_h <= screen_height)) {
		XMoveWindow(Xdisplay, me_win, term_x, term_y + term_h);
		move = TRUE;
	}
#endif

	if (move && changed)
		gdk_window_move_resize(gdk_win, me_x, me_y, me_w, me_h);

	g_free(term_desktop);
	g_free(me_desktop);

	return move && changed;
}


/* Figure out the size of the window decorations (making assumptions) */
static void get_decorations(GdkWindow* gdk_win, gint* left, gint* right,
		gint* top, gint* bottom) {

	if (!gdk_window_is_visible(gdk_win))
		return;

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

