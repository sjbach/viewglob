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
#include "x11-stuff.h"
#define MAX_PROPERTY_VALUE_LEN 4096

#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>


#if DEBUG_ON
extern FILE* df;
#endif

//static int action_window_str (Display* disp, char mode);
static Window *get_client_list (Display *disp, unsigned long *size);
char* get_property (Display *disp, Window win, Atom xa_prop_type, char* prop_name, unsigned long *size);
static char *get_window_title (Display *disp, Window win);

#if 0
Window get_terminal_xid(Display* disp, pid_t pid) {
	char* title;
	int i;
	Window xid = 0;

	if(!disp)
		return 0;

	title = XMALLOC(char, 100);
	sprintf(title, "viewglob%d", pid);

	/* Wait for at most 3 seconds. */
	for (i = 0; i < 30; i++) {
		xid = get_xid_from_title(disp, title);
		if (xid)
			break;
		else
			usleep(100000);
	}

	return xid;
}
#endif

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


int client_msg(Display *disp, Window win, char *msg,
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
    
    if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
        return EXIT_SUCCESS;
    }
    else {
        fprintf(stderr, "Cannot send %s event.\n", msg);
        return EXIT_FAILURE;
    }
}


char* get_property (Display* disp, Window win,
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
        //p_verbose("Cannot get %s property.\n", prop_name);
        return NULL;
    }
  
    if (xa_ret_type != xa_prop_type) {
        //p_verbose("Invalid type of %s property.\n", prop_name);
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

	if (!disp)
		return 0;

	/* Wait for at most 3 seconds. */
	for (i = 0; i < 30; i++) {

		client_list = get_client_list(disp, &client_list_size);

		for (j = 0; j < client_list_size / 4; j++) {
			char* window_title = get_window_title(disp, client_list[j]);
			if (strcmp(title, window_title) == 0)
				return client_list[j];
		}

		/* Check again in .1 seconds. */
		usleep(100000);
	}

	return 0;
}

#if 0
static int action_window_str (Display *disp, char mode) {
    Window activate = 0;
    Window *client_list;
    unsigned long client_list_size;
    int i;
    
    if (strcmp(SELECT_WINDOW_MAGIC, options.param_window) == 0) {
        activate = Select_Window(disp);
        if (activate) {
            return action_window(disp, activate, mode);
        }
        else {
            return EXIT_FAILURE;
        }
    }
    else {
        if ((client_list = get_client_list(disp, &client_list_size)) == NULL) {
            return EXIT_FAILURE; 
        }
        
        for (i = 0; i < client_list_size / 4; i++) {
            char *title_utf8 = get_window_title(disp, client_list[i]); /* UTF8 */
            char *title_utf8_cf = NULL;
            if (title_utf8) {
                char *match;
                char *match_cf;
                if (envir_utf8) {
                    match = g_strdup(options.param_window);
                    match_cf = g_utf8_casefold(options.param_window, -1);
                }
                else {
                    if (! (match = g_locale_to_utf8(options.param_window, -1, NULL, NULL, NULL))) {
                        match = g_strdup(options.param_window);
                    }
                    match_cf = g_utf8_casefold(match, -1);
                }
                
                if (!match || !match_cf) {
                    continue;
                }

                title_utf8_cf = g_utf8_casefold(title_utf8, -1);

                if ((options.full_window_title_match && strcmp(title_utf8, match) == 0) ||
                        (!options.full_window_title_match && strstr(title_utf8_cf, match_cf))) {
                    activate = client_list[i];
                    g_free(match);
                    g_free(match_cf);
                    g_free(title_utf8);
                    g_free(title_utf8_cf);
                    break;
                }
                g_free(match);
                g_free(match_cf);
                g_free(title_utf8);
                g_free(title_utf8_cf);
            }
        }
        g_free(client_list);

        if (activate) {
            return action_window(disp, activate, mode);
        }
        else {
            return EXIT_FAILURE;
        }
    }
}
#endif

static Window *get_client_list (Display *disp, unsigned long *size) {
    Window *client_list;

    if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp), 
                    XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
        if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp), 
                        XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
            fputs("Cannot get client list properties. \n"
                  "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
                  "\n", stderr);
            return NULL;
        }
    }

    return client_list;
}


static char* get_window_title (Display *disp, Window win) {/*{{{*/
    char *title_utf8;
    char *wm_name;
    char *net_wm_name;

    wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
    net_wm_name = get_property(disp, win, 
            XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

    if (net_wm_name) {
        //title_utf8 = g_strdup(net_wm_name);
        title_utf8 = strdup(net_wm_name);
    }
    else {
        if (wm_name) {
            //title_utf8 = g_locale_to_utf8(wm_name, -1, NULL, NULL, NULL);
			title_utf8 = strdup(wm_name);
        }
        else {
            title_utf8 = NULL;
        }
    }

    XFREE(wm_name);
    XFREE(net_wm_name);
    
    return title_utf8;
}/*}}}*/


