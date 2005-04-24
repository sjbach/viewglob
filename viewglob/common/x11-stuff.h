/*
	Copyright (C) 2003 Tomas Styblo
	From wmctrl:
	A command line tool to interact with an EWMH/NetWM compatible X Window
	Manager.

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

#ifndef X11_STUFF_H
#define X11_STUFF_H

#include "common.h"
#include <X11/Xlib.h>

G_BEGIN_DECLS


void refocus(Display* disp, Window w1, Window w2);
Window get_xid_from_title(Display* disp, char* title);
Window get_active_window(Display* disp);
void focus_window(Display* disp, Window win, gulong desktop);
void raise_window(Display* disp, Window win1, Window win2,
		gboolean switch_desktop);

gint window_to_desktop (Display *disp, Window win, gint desktop);
gulong* get_desktop(Display* disp, Window win);
gulong* current_desktop(Display* disp);

gboolean is_visible(Display* disp, Window win);
gchar* win_to_str(Window win);
Window str_to_win(gchar* string);


G_END_DECLS

#endif /* !X11_STUFF_H */

