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
#include "file_box.h"
#include "param-io.h"
#include "dircont.h"
#include <gtk/gtk.h>
#include <string.h>   /* For strcmp */

#define WIDTH_BUFFER 0
#define BASE_DIR_FONT_SIZE    +2
#define BASE_COUNT_FONT_SIZE  -1

/* --- prototypes --- */
static void dircont_class_init(DirContClass* klass);
static void dircont_init(DirCont* dc);

static void update_layout(DirCont* dc);

static gboolean button_press_event(GtkWidget *widget,
		GdkEventButton *event, DirCont* dc);
static gboolean header_expose_event(GtkWidget *area, GdkEventExpose *event,
		DirCont* dc);


/* --- variables --- */
static gpointer parent_class = NULL;
static GdkColor header_color;
static GdkColor separator_color;
static gint header_height = 0;

static GString* mask = NULL;
static GString* dir_tag_open = NULL;
static GString* dir_tag_close = NULL;
static GString* count_tag_open = NULL;
static GString* count_tag_close = NULL;

/* --- functions --- */
GType dircont_get_type(void) {
	static GType dircont_type = 0;

	if (!dircont_type) {
		static const GTypeInfo dircont_info = {
			sizeof (DirContClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) dircont_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (DirCont),
			0,		/* n_preallocs */
			(GInstanceInitFunc) dircont_init,
		};
		dircont_type = g_type_register_static (GTK_TYPE_VBOX, "DirCont",
				&dircont_info, 0);
	}

	return dircont_type;
}


static void dircont_class_init(DirContClass* class) {
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;


	object_class = G_OBJECT_CLASS(class);
	widget_class = GTK_WIDGET_CLASS(class);

	/* Figure out how high the header should be. */
	gint height;
	GtkWidget* area = gtk_drawing_area_new();
	PangoLayout* layout = gtk_widget_create_pango_layout(area, NULL);
	gchar* markup = g_strconcat(
			dir_tag_open->str, "<b>",
			"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZZ12345"
			"</b>", dir_tag_close->str, NULL);
	pango_layout_set_markup(layout, markup, -1);
	g_free(markup);
	pango_layout_get_pixel_size(layout, NULL, &height);
	header_height += height;
	markup = g_strconcat(
			count_tag_open->str,
			"0123456789()",
			count_tag_close->str, NULL);
	pango_layout_set_markup(layout, markup, -1);
	g_free(markup);
	pango_layout_get_pixel_size(layout, NULL, &height);
	header_height += height;
	g_object_unref(G_OBJECT(layout));
	gtk_widget_destroy(area);
//	header_height += 3; /* For spacing */

	//FIXME
	// Get separator and header colours
	if (!mask)
		mask = g_string_new(NULL);
	if (!dir_tag_open)
		dir_tag_open = g_string_new(NULL);
	if (!dir_tag_close)
		dir_tag_close = g_string_new(NULL);
	if (!count_tag_open)
		count_tag_open = g_string_new(NULL);
	if (!count_tag_close)
		count_tag_close = g_string_new(NULL);

	parent_class = g_type_class_peek_parent(class);
//	widget_class->size_request = dircont_size_request;
//	widget_class->size_allocate = dircont_size_allocate;
}


static void dircont_init(DirCont* dc) {
	GTK_WIDGET_SET_FLAGS(dc, GTK_NO_WINDOW);

	GtkBox* box = GTK_BOX(dc);
	gtk_box_set_homogeneous(box, FALSE);

	dc->name = g_string_new(NULL);
	dc->rank = dc->old_rank = -1;
	dc->marked = FALSE;
	dc->selected = g_string_new(NULL);
	dc->total = g_string_new(NULL);
	dc->hidden = g_string_new(NULL);
	dc->is_pwd = FALSE;
	dc->is_active = FALSE;

	/* The header is a drawing area. */
	dc->header = gtk_drawing_area_new();
	gtk_widget_set_size_request(dc->header, -1, header_height); //FIXME
	g_printerr("(%d)\n", header_height);
	gtk_widget_show(dc->header);
	g_signal_connect(G_OBJECT(dc->header), "expose-event",
			G_CALLBACK(header_expose_event), dc);
	g_signal_connect(G_OBJECT(dc->header), "button-press-event",
			G_CALLBACK(button_press_event), dc);

	/* Create initial layouts. */
	dc->name_layout = gtk_widget_create_pango_layout(dc->header, NULL);
	dc->counts_layout = gtk_widget_create_pango_layout(dc->header, NULL);

	/* Setup the scrolled window (for the file box) */
	dc->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dc->scrolled_window),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
//	gtk_widget_show(dc->scrolled_window);

	/* Create the file box. */
	dc->file_box = file_box_new();
	wrap_box_set_hspacing(WRAP_BOX(dc->file_box), 3);
	gtk_container_set_border_width(GTK_CONTAINER(dc->file_box), 2);
	wrap_box_set_line_justify(WRAP_BOX(dc->file_box), GTK_JUSTIFY_LEFT);
	gtk_widget_show(dc->file_box);

	/* Initialize the layout. */
	dircont_set_name(dc, "<unset>");
	dircont_set_selected(dc, "0");
	dircont_set_total(dc, "0");
	dircont_set_hidden(dc, "0");

	/* Pack 'em in. */
	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(dc->scrolled_window), dc->file_box);
	gtk_box_pack_start(box, dc->header, FALSE, FALSE, 0);
	gtk_box_pack_start(box, dc->scrolled_window, TRUE, TRUE, 0);
}


GtkWidget* dircont_new (void) {
	return g_object_new (DIRCONT_TYPE, NULL);
}


/* Set the sizes of the directory heading font and the file count font. */
void dircont_set_sizing(gint modifier) {

	g_return_if_fail(modifier <= 10);
	g_return_if_fail(modifier >= -10);

	gint dir_size, count_size;
	gint i;

	/* Instantiate these if need be. */
	if (!dir_tag_open)
		dir_tag_open = g_string_new(NULL);
	if (!dir_tag_close)
		dir_tag_close = g_string_new(NULL);
	if (!count_tag_open)
		count_tag_open = g_string_new(NULL);
	if (!count_tag_close)
		count_tag_close = g_string_new(NULL);

	dir_tag_open = g_string_truncate(dir_tag_open, 0);
	dir_tag_close = g_string_truncate(dir_tag_close, 0);
	count_tag_open = g_string_truncate(count_tag_open, 0);
	count_tag_close = g_string_truncate(count_tag_close, 0);

	dir_size = BASE_DIR_FONT_SIZE + modifier;
	count_size = BASE_COUNT_FONT_SIZE + modifier;

	/* Directory label sizing. */
	if (dir_size > 0) {
		for (i = 0; i < dir_size; i++) {
			dir_tag_open = g_string_append(dir_tag_open, "<big>");
			dir_tag_close = g_string_append(dir_tag_close, "</big>");
		}
	}
	else if (dir_size < 0) {
		for (i = 0; i > dir_size; i--) {
			dir_tag_open = g_string_append(dir_tag_open, "<small>");
			dir_tag_close = g_string_append(dir_tag_close, "</small>");
		}
	}

	/* File count label sizing. */
	if (count_size > 0) {
		for (i = 0; i < count_size; i++) {
			count_tag_open = g_string_append(count_tag_open, "<big>");
			count_tag_close = g_string_append(count_tag_close, "</big>");
		}
	}
	else if (count_size < 0) {
		for (i = 0; i > count_size; i--) {
			count_tag_open = g_string_append(count_tag_open, "<small>");
			count_tag_close = g_string_append(count_tag_close, "</small>");
		}
	}
}


void dircont_set_name(DirCont* dc, const gchar* name) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));
	g_return_if_fail(name != NULL);

	dc->name = g_string_assign(dc->name, name);
	update_layout(dc);
}


void dircont_set_selected(DirCont* dc, const gchar* selected) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));
	g_return_if_fail(selected != NULL);

	if (!STREQ(selected, dc->selected->str)) {
		dc->selected = g_string_assign(dc->selected, selected);
		update_layout(dc);
	}
}


void dircont_set_total(DirCont* dc, const gchar* total) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));
	g_return_if_fail(total != NULL);

	if (!STREQ(total, dc->total->str)) {
		dc->is_restricted = STREQ(total, "0");
		dc->total = g_string_assign(dc->total, total);
		update_layout(dc);
	}
}


void dircont_set_hidden(DirCont* dc, const gchar* hidden) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));
	g_return_if_fail(hidden != NULL);

	if (!STREQ(hidden, dc->hidden->str)) {
		dc->hidden = g_string_assign(dc->hidden, hidden);
		update_layout(dc);
	}
}


void dircont_mark(DirCont* dc, gint new_rank) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));

	dc->marked = TRUE;
	dc->old_rank = dc->rank;
	dc->rank = new_rank;
}


gboolean dircont_is_new(const DirCont* dc) {
	g_return_val_if_fail(dc != NULL, TRUE);
	g_return_val_if_fail(IS_DIRCONT(dc), TRUE);

	if (dc->old_rank >= 0)
		return FALSE;
	else
		return TRUE;
}


void dircont_set_active(DirCont* dc, gboolean setting) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));

	if (setting != dc->is_active) {
		dc->is_active = setting;
		update_layout(dc);
		if (dc->is_active)
			gtk_widget_show(dc->scrolled_window);
		else
			gtk_widget_hide(dc->scrolled_window);
	}
}

// TODO connect to scrolled window resize, use that as base for file_box resize


void dircont_set_pwd(DirCont* dc, gboolean setting) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));

	if (setting != dc->is_pwd) {
		dc->is_pwd = setting;
		update_layout(dc);
	}
}


/* Remove all memory associated with this DirCont. */
void dircont_free(DirCont* dc) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));

	GtkWidget* widget;
	
	
	widget = GTK_WIDGET(dc);
	gtk_widget_hide(widget);

	/* This also gets the FItems associated with the file box. */
	file_box_destroy(FILE_BOX(dc->file_box));

	g_string_free(dc->name, TRUE);
	g_string_free(dc->selected, TRUE);
	g_string_free(dc->total, TRUE);
	g_string_free(dc->hidden, TRUE);

	/* Should take care of everything else. */
	gtk_widget_destroy(widget);
}


static void update_layout(DirCont* dc) {

	gchar* escaped;
	gchar* utf8;
	gchar* markup;
	gsize len;

	utf8 = g_filename_to_utf8(dc->name->str, dc->name->len, NULL, &len, NULL);
	escaped = g_markup_escape_text(utf8, len);
	markup = g_strconcat(
			dir_tag_open->str, "<b>",
			escaped,
			"</b>", dir_tag_close->str, NULL);

	pango_layout_set_markup(dc->name_layout, markup, -1);
	g_free(markup);
	g_free(escaped);
	g_free(utf8);

	if (dc->is_active) {
		markup = g_strconcat(count_tag_open->str,
				dc->selected->str, " selected, ",
				dc->total->str, " total (",
				dc->hidden->str, " hidden)          Mask: ", mask->str,
				count_tag_close->str, NULL);
	}
	else {
		markup = g_strconcat(count_tag_open->str,
				dc->selected->str, " selected, ",
				dc->total->str, " total (",
				dc->hidden->str, " hidden)",
				count_tag_close->str, NULL);
	}
	pango_layout_set_markup(dc->counts_layout, markup, -1);
	g_free(markup);

	/* Now make sure the header gets redrawn. */
	GdkRectangle rect = { 0, 0, 0, 0, };
	rect.width = dc->header->allocation.width;
	rect.height = dc->header->allocation.height;
	gdk_window_invalidate_rect(dc->header->window, &rect, FALSE);


//	if (dc->is_pwd)
//	if (dc->is_active)

}


static gboolean header_expose_event(GtkWidget *header, GdkEventExpose *event,
		DirCont* dc) {

	GdkGC* fg_gc = header->style->fg_gc[GTK_WIDGET_STATE(header)];
//	GdkGC* bg_gc = header->style->bg_gc[GTK_WIDGET_STATE(header)];
	GdkGC* bg_gc = header->style->bg_gc[GTK_STATE_ACTIVE];

	gint name_height;
	pango_layout_get_pixel_size(dc->name_layout, NULL, &name_height);

	/* Draw background of name */
	gdk_draw_rectangle(
			header->window,
			bg_gc,
			TRUE,
			0, 0,
			header->allocation.width, name_height);

	/* Draw name */
	gdk_draw_layout(
			header->window,
			fg_gc,
			3, 0,
			dc->name_layout);

	/* Draw file counts (and maybe mask text) */
	gdk_draw_layout(
			header->window,
			fg_gc,
			20, name_height,
			dc->counts_layout);

	return TRUE;
}


static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
		DirCont* dc) {
	g_return_val_if_fail(dc != NULL, FALSE);
	g_return_val_if_fail(IS_DIRCONT(dc), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	//FIXME single click activates

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		/* Double left-click -- write out the directory name. */

		GString* string;
		string = g_string_new(dc->name->str);

		/* Append a trailing '/' if need be. */
		if ( *(string->str + string->len - 1) != '/')
			string = g_string_append(string, "/");

		if (!put_param(STDOUT_FILENO, P_FILE, string->str))
			g_warning("Could not write filename to stdout");

		g_string_free(string, TRUE);
	}

	return FALSE;
}


void dircont_set_optimal_width(DirCont* dc, gint width) {

	g_return_if_fail(IS_DIRCONT(dc));
	g_return_if_fail(width >= 0);

	dc->optimal_width = width;
	file_box_set_optimal_width(FILE_BOX(dc->file_box),
			dc->optimal_width - WIDTH_BUFFER);

//	gtk_widget_queue_resize(GTK_WIDGET(dc->heading_event_box));
}


/* Set the new mask text and update the given DirCont (which is probably the
   active DirCont). */
void dircont_set_mask_string(DirCont* dc, gchar* mask_str) {

	mask = g_string_assign(mask, mask_str);

	update_layout(dc);
}



