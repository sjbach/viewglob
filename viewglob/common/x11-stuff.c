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


#include "common.h"
#include "x11-stuff.h"
#define MAX_PROPERTY_VALUE_LEN 4096

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <string.h>
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

	if (w1 == 0 || w2 == 0)
		return;

	Window active_window;
	gint dummy;
	gulong* w1_desktop;
	gulong* w2_desktop;

	w1_desktop = get_desktop(disp, w1);
	w2_desktop = get_desktop(disp, w2);

	XGetInputFocus(disp, &active_window, &dummy);

	/* Refocus the window which isn't focused.  Or, if neither
	   are focused (?), focus both. */
	if (active_window == w1)
		focus_window(disp, w2, *w1_desktop);
	else if (active_window == w2)
		focus_window(disp, w1, *w2_desktop);
	else {
		focus_window(disp, w1, *w2_desktop);
		focus_window(disp, w2, *w2_desktop);
	}

	g_free(w1_desktop);
	g_free(w2_desktop);
}


Window get_active_window(Display* disp) {
	g_return_val_if_fail(disp != NULL, 0);
	
	/*
	Window active_window;
	gint dummy;

	XGetInputFocus(disp, &active_window, &dummy);
	
	return active_window;
	*/

	gchar *prop;
	gulong size;
	Window ret = (Window)0;

	prop = get_property(disp, DefaultRootWindow(disp), XA_WINDOW, 
		"_NET_ACTIVE_WINDOW", &size);
	if (prop) {
		ret = *((Window*)prop);
		g_free(prop);
	}
	return(ret);
}


void focus_window(Display* disp, Window win, gulong desktop) {

	if (window_to_desktop(disp, win, desktop)) {
		 /* 100 ms - make sure the WM has enough time to move the window,
			before we activate it */
		usleep(100000);
	}

	client_msg(disp, win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
	XMapRaised(disp, win);
}


/* Check to see if the given window is visible. */
gboolean is_visible(Display* disp, Window win) {

	if (win == 0)
		return FALSE;

	XWindowAttributes xwa;
	if (!XGetWindowAttributes(disp, win, &xwa)) {
		g_warning("Could not get X window attributes");
		return FALSE;
	}
	else if (xwa.map_state != IsViewable)
		return FALSE;

	return TRUE;
}


gulong* get_desktop(Display* disp, Window win) {

	gulong* desktop = NULL;

	/* desktop ID */
	if ((desktop = (gulong*)get_property(disp, win,
			XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
		desktop = (gulong*)get_property(disp, win,
				XA_CARDINAL, "_WIN_WORKSPACE", NULL);
	}

	return desktop;
}


gulong* current_desktop(Display* disp) {
    gulong *cur_desktop = NULL;

	Window root = DefaultRootWindow(disp);

	if (!(cur_desktop = (gulong*)get_property(disp, root,
			XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
		if (!(cur_desktop = (gulong*)get_property(disp, root,
				XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
			g_warning("Cannot get current desktop properties. "
					"(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)");
		}
	}

	return cur_desktop;
}


gint window_to_desktop (Display *disp, Window win, gint desktop) {
    gulong *cur_desktop = NULL;
    Window root = DefaultRootWindow(disp);
   
    if (desktop == -1) {
        if (! (cur_desktop = (gulong *)get_property(disp, root,
                XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
            if (! (cur_desktop = (gulong *)get_property(disp, root,
                    XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
                g_warning("Cannot get current desktop properties. "
                      "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)");
                return EXIT_FAILURE;
            }
        }
        desktop = *cur_desktop;
    }
    g_free(cur_desktop);

    return client_msg(disp, win, "_NET_WM_DESKTOP", (gulong)desktop,
            0, 0, 0, 0);
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

		for (j = 0; j < client_list_size / sizeof(Window); j++) {
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


static gchar* get_window_title(Display *disp, Window win) {
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


/* Converts win to a string (statically allocated memory) */
gchar* win_to_str(Window win) {

	static GString* win_str = NULL;

	if (!win_str)
		win_str = g_string_new(NULL);

	g_string_printf(win_str, "%lu", win);
	return win_str->str;
}


Window str_to_win(gchar* string) {
	g_return_val_if_fail(string != NULL, 0);

	Window win;

	win = strtoul(string, NULL, 10);

	if (win == ULONG_MAX) {
		g_warning("str_to_win(): conversion out of bounds: %s", string);
		win = 0;
	}

	return win;
}

