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

/* Modified from main.c in the wmctrl package. */


#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "x11-stuff.h"
#define MAX_PROPERTY_VALUE_LEN 4096

#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib.h>

static Window *get_client_list (Display *disp, gulong *size);
static gchar* get_property (Display* disp, Window win, Atom xa_prop_type,
		gchar* prop_name, gulong* size);
static gchar* get_window_title (Display* disp, Window win);
static gboolean client_msg(Display* disp, Window win, gchar* msg,
		gulong data0, gulong data1, gulong data2, gulong data3, gulong data4);


void refocus(Display* disp, Window w1, Window w2) {

	g_return_if_fail(disp != NULL);

	Window active_window;
	gint revert_to_return;

	XGetInputFocus(disp, &active_window, &revert_to_return);

	/* Refocus the window which isn't focused.  Or, if neither
	   are focused (?), focus both. */
	if (active_window == w1 && w2 != 0)
		focus_window(disp, w2, FALSE);
	else if (active_window == w2 && w1 != 0)
		focus_window(disp, w1, FALSE);
	else if (w1 != 0 && w2 != 0) {
		focus_window(disp, w2, FALSE);
		focus_window(disp, w1, FALSE);
	}
}


Window get_active_window(Display* disp) {
	
	g_return_val_if_fail(disp != NULL, 0);
	
	Window active_window;
	gint revert_to_return;

	XGetInputFocus(disp, &active_window, &revert_to_return);
	
	return active_window;
}


void focus_window(Display* disp, Window win, gboolean switch_desktop) {
	gulong* desktop;
	/* desktop ID */
	if ((desktop = (gulong*)get_property(disp, win,
			XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
		if ((desktop = (gulong*)get_property(disp, win,
				XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
			g_warning("Cannot find desktop ID of the window");
		}
	}

	if (switch_desktop && desktop) {
		if (client_msg(disp, DefaultRootWindow(disp), 
					"_NET_CURRENT_DESKTOP", 
					*desktop, 0, 0, 0, 0) != EXIT_SUCCESS) {
			g_warning("Cannot switch desktop");
		}
		g_free(desktop);
	}

	/*client_msg(disp, win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);*/
	client_msg(disp, win, "_NET_ACTIVE_WINDOW", 2, time(NULL), 0, 0, 0);
	XMapRaised(disp, win);
}


static gboolean client_msg(Display* disp, Window win, gchar* msg,
		gulong data0, gulong data1, 
		gulong data2, gulong data3,
		gulong data4) {
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
		return TRUE;
	else {
		g_warning("Cannot send %s event", msg);
		return FALSE;
	}
}


static gchar* get_property (Display* disp, Window win,
		Atom xa_prop_type, gchar* prop_name, gulong* size) {
	Atom xa_prop_name;
	Atom xa_ret_type;
	gint ret_format;
	gulong ret_nitems;
	gulong ret_bytes_after;
	gulong tmp_size;
	guchar* ret_prop;
	gchar* ret;
	
	xa_prop_name = XInternAtom(disp, prop_name, False);
	
	if (XGetWindowProperty(disp, win, xa_prop_name, 0,
				MAX_PROPERTY_VALUE_LEN / 4, False,
				xa_prop_type, &xa_ret_type, &ret_format,
				&ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
		g_warning("Cannot get %s property", prop_name);
		return NULL;
	}
  
	if (xa_ret_type != xa_prop_type) {
		g_warning("Invalid type of %s property", prop_name);
		XFree(ret_prop);
		return NULL;
	}

	/* null terminate the result to make string handling easier */
	tmp_size = (ret_format / 8) * ret_nitems;
	ret = g_malloc(tmp_size + 1);
	memcpy(ret, ret_prop, tmp_size);
	ret[tmp_size] = '\0';

	if (size)
		*size = tmp_size;
	
	XFree(ret_prop);
	return ret;
}


Window get_xid_from_title(Display* disp, gchar* title) {
	Window* client_list;
	gulong client_list_size;
	gint i, j;

	if (!disp || !title)
		return 0;

	/* Wait for at most 3 seconds. */
	for (i = 0; i < 30; i++) {

		if ( (client_list = get_client_list(disp, &client_list_size)) == NULL)
			return 0;

		for (j = 0; j < client_list_size / 4; j++) {
			gchar* window_title = get_window_title(disp, client_list[j]);
			/* title can appear anywhere in the window's title. */
			if (window_title && strstr(window_title, title))
				return client_list[j];
		}

		/* Check again in .1 seconds. */
		usleep(100000);
	}

	return 0;
}


static Window* get_client_list (Display* disp, gulong* size) {
	Window* client_list;

	if ((client_list = (Window*)get_property(disp, DefaultRootWindow(disp), 
					XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
		if ((client_list = (Window*)get_property(disp, DefaultRootWindow(disp), 
						XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
			g_warning("Cannot get client list properties. \n"
				  "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
				  "\n");
			return NULL;
		}
	}

	return client_list;
}


static gchar* get_window_title (Display *disp, Window win) {
	gchar* title_utf8;
	gchar* wm_name;
	gchar* net_wm_name;

	wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
	net_wm_name = get_property(disp, win, 
			XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

	if (net_wm_name)
		title_utf8 = g_strdup(net_wm_name);
	else if (wm_name)
		title_utf8 = g_strdup(wm_name);
	else
		title_utf8 = NULL;

	g_free(wm_name);
	g_free(net_wm_name);
	
	return title_utf8;
}

