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


#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "viewglob-error.h"
#include "x11-stuff.h"
#define MAX_PROPERTY_VALUE_LEN 4096

#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>


#if DEBUG_ON
extern FILE* df;
#endif

static Window *get_client_list (Display *disp, unsigned long *size);
static char* get_property (Display* disp, Window win, Atom xa_prop_type, char* prop_name, unsigned long* size);
static char* get_window_title (Display* disp, Window win);
static bool client_msg(Display* disp, Window win, char* msg,
		unsigned long data0, unsigned long data1, 
		unsigned long data2, unsigned long data3,
		unsigned long data4);


bool activate_window (Display* disp, Window win, bool switch_desktop) {
	unsigned long* desktop;

	/* desktop ID */
	if ((desktop = (unsigned long*)get_property(disp, win,
			XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
		if ((desktop = (unsigned long*)get_property(disp, win,
				XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
			DEBUG((df, "Cannot find desktop ID of the window.\n"));
		}
	}

	if (switch_desktop && desktop) {
		if (client_msg(disp, DefaultRootWindow(disp), 
					"_NET_CURRENT_DESKTOP", 
					*desktop, 0, 0, 0, 0) != EXIT_SUCCESS) {
			DEBUG((df, "Cannot switch desktop.\n"));
		}
		XFREE(desktop);
	}

	client_msg(disp, win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
	XMapRaised(disp, win);

	return true;
}


static bool client_msg(Display* disp, Window win, char* msg,
		unsigned long data0, unsigned long data1, 
		unsigned long data2, unsigned long data3,
		unsigned long data4) {
	XEvent event;
	long mask = SubstructureRedirectMask | SubstructureNotifyMask;

	event.xclient.type = ClientMessage;
	event.xclient.serial = 0;
	event.xclient.send_event = True;
	event.xclient.message_type = XInternAtom(disp, msg, False);
	event.xclient.window = win;
	event.xclient.format = 32;
	event.xclient.data.l[0] = data0;
	event.xclient.data.l[1] = data1;
	event.xclient.data.l[2] = data2;
	event.xclient.data.l[3] = data3;
	event.xclient.data.l[4] = data4;
	
	if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event))
		return true;
	else {
		viewglob_warning("Cannot send %s event");
		viewglob_warning(msg);
		return false;
	}
}


static char* get_property (Display* disp, Window win,
		Atom xa_prop_type, char* prop_name, unsigned long* size) {
	Atom xa_prop_name;
	Atom xa_ret_type;
	int ret_format;
	unsigned long ret_nitems;
	unsigned long ret_bytes_after;
	unsigned long tmp_size;
	unsigned char* ret_prop;
	char* ret;
	
	xa_prop_name = XInternAtom(disp, prop_name, False);
	
	if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
			xa_prop_type, &xa_ret_type, &ret_format,
			&ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
		DEBUG((df, "Cannot get %s property.\n", prop_name));
		return NULL;
	}
  
	if (xa_ret_type != xa_prop_type) {
		DEBUG((df, "Invalid type of %s property.\n", prop_name));
		XFree(ret_prop);
		return NULL;
	}

	/* null terminate the result to make string handling easier */
	tmp_size = (ret_format / 8) * ret_nitems;
	ret = XMALLOC(char, tmp_size + 1);
	memcpy(ret, ret_prop, tmp_size);
	ret[tmp_size] = '\0';

	if (size)
		*size = tmp_size;
	
	XFree(ret_prop);
	return ret;
}


Window get_xid_from_title(Display* disp, char* title) {
	Window* client_list;
	unsigned long client_list_size;
	int i, j;

	if (!disp || !title)
		return 0;

	/* Wait for at most 3 seconds. */
	for (i = 0; i < 30; i++) {

		if ( (client_list = get_client_list(disp, &client_list_size)) == NULL)
			return 0;

		for (j = 0; j < client_list_size / 4; j++) {
			char* window_title = get_window_title(disp, client_list[j]);
			if (window_title && strcmp(title, window_title) == 0)
				return client_list[j];
		}

		/* Check again in .1 seconds. */
		usleep(100000);
	}

	return 0;
}


static Window* get_client_list (Display* disp, unsigned long* size) {
	Window* client_list;

	if ((client_list = (Window*)get_property(disp, DefaultRootWindow(disp), 
					XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
		if ((client_list = (Window*)get_property(disp, DefaultRootWindow(disp), 
						XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
			viewglob_warning("Cannot get client list properties. \n"
				  "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
				  "\n");
			return NULL;
		}
	}

	return client_list;
}


static char* get_window_title (Display *disp, Window win) {
	char* title_utf8;
	char* wm_name;
	char* net_wm_name;

	wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
	net_wm_name = get_property(disp, win, 
			XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

	if (net_wm_name)
		title_utf8 = strdup(net_wm_name);
	else if (wm_name)
		title_utf8 = strdup(wm_name);
	else
		title_utf8 = NULL;

	XFREE(wm_name);
	XFREE(net_wm_name);
	
	return title_utf8;
}


