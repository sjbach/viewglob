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

/* Modified from main.c from the wmctrl package. */

#ifndef X11_STUFF_H
#define X11_STUFF_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include <X11/Xlib.h>

BEGIN_C_DECLS


int client_msg(Display *disp, Window win, char *msg,
	unsigned long data0, unsigned long data1, 
	unsigned long data2, unsigned long data3,
	unsigned long data4);

bool activate_window (Display* disp, Window win, bool switch_desktop);

char* get_property (Display* disp, Window win,
	Atom xa_prop_type, char* prop_name, unsigned long* size);

Window get_xid_from_title(Display* disp, char* title);


END_C_DECLS

#endif /* !X11_STUFF_H */

