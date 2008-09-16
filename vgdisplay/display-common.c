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

#include "common.h"
#include "string.h"
#include "x11-stuff.h"
#include "display-common.h"
#include "param-io.h"
#include "lscolors.h"

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "fgetopt.h"

static void report_version(void);


void prefs_init(struct prefs* v) {
	v->show_icons = TRUE;
	v->jump_resize = TRUE;
	v->font_size_modifier = 0;
}


void parse_args(int argc, char** argv, struct prefs* v) {
	gboolean in_loop = TRUE;

	struct option long_options[] = {
		{ "font-size-modifier", 1, NULL, 'z' },
		{ "black", 1, NULL, '1' },
		{ "red", 1, NULL, '2' },
		{ "green", 1, NULL, '3' },
		{ "yellow", 1, NULL, '4' },
		{ "blue", 1, NULL, '5' },
		{ "magenta", 1, NULL, '6' },
		{ "cyan", 1, NULL, '7' },
		{ "white", 1, NULL, '8' },
		{ "jump-resize", 2, NULL, 'j' },
		{ "file-icons", 2, NULL, 'i' },
		{ "version", 0, NULL, 'V' },
	};

	GdkColor color_temp;

	optind = 0;
	while (in_loop) {
		switch (fgetopt_long(argc, argv, "j::z:i::vV", long_options, NULL)) {

			case -1:
				in_loop = FALSE;
				break;

			/* Font size modifier */
			case 'z':
				v->font_size_modifier = CLAMP(atoi(optarg), -10, 10);
				break;

			/* Enable or disable icons */
			case 'i':
				if (!optarg || STREQ(optarg, "on"))
					v->show_icons = TRUE;
				else if (STREQ(optarg, "off"))
					v->show_icons = FALSE;
				break;

			/* Enable or disable jump-resize */
			case 'j':
				if (!optarg || STREQ(optarg, "on"))
					v->jump_resize = TRUE;
				else if (STREQ(optarg, "off"))
					v->jump_resize = FALSE;
				break;

			/* Colours */
			case '1':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_BLACK, &color_temp);
				break;
			case '2':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_RED, &color_temp);
				break;
			case '3':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_GREEN, &color_temp);
				break;
			case '4':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_YELLOW, &color_temp);
				break;
			case '5':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_BLUE, &color_temp);
				break;
			case '6':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_MAGENTA, &color_temp);
				break;
			case '7':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_CYAN, &color_temp);
				break;
			case '8':
				if (gdk_color_parse(optarg, &color_temp))
					set_color(TCC_WHITE, &color_temp);
				break;

			case 'v':
			case 'V':
				report_version();
				break;
	
			case ':':
				g_warning("Option missing argument");
				/*exit(EXIT_FAILURE);*/
				break;

			case '?':
			default:
				g_warning("Unknown option provided");
				/*exit(EXIT_FAILURE);*/
				break;
		}
	}
}


static void report_version(void) {
	g_print("%s %s\n", g_get_prgname(), VERSION);
	g_print("Released %s\n", VG_RELEASE_DATE);
	exit(EXIT_SUCCESS);
}


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


void refocus_wrapped(GtkWidget* display_win_gtk, gchar* term_win_str) {
	g_return_if_fail(display_win_gtk != NULL);
	g_return_if_fail(term_win_str != NULL);
	g_return_if_fail(display_win_gtk->window != NULL);

	Display* Xdisplay;
	Window display_win, term_win;

	Xdisplay = GDK_DRAWABLE_XDISPLAY(display_win_gtk->window);
	display_win = GDK_WINDOW_XID(display_win_gtk->window);
	term_win = str_to_win(term_win_str);

	if (display_win == 0 || term_win == 0)
		return;

	refocus(Xdisplay, display_win, term_win);
	static int i = 0; i++;
	g_warning("refocus %d", i);
}


void raise_wrapped(GtkWidget* display_win_gtk, gchar* term_win_str) {

	Display* Xdisplay;
	Window display_win, term_win;

	Xdisplay = GDK_DRAWABLE_XDISPLAY(display_win_gtk->window);
	display_win = GDK_WINDOW_XID(display_win_gtk->window);
	term_win = str_to_win(term_win_str);

	if (display_win == 0 || term_win == 0)
		return;

	gint term_desktop = get_desktop(Xdisplay, term_win);
	gint disp_desktop = get_desktop(Xdisplay, display_win);

	if (term_desktop != -1 && disp_desktop != -1
			&& term_desktop != disp_desktop) {
		window_to_desktop(Xdisplay, display_win, term_desktop);
	}

	static int i = 0; i++;
	g_warning("raise %d", i);
	/*raise_window(Xdisplay, display_win, term_win, TRUE);*/
	//XMapRaised(Xdisplay, display_win);
	XRaiseWindow(Xdisplay, display_win);
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

