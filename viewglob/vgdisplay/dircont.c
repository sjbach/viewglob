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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#define WIDTH_BUFFER 5
#define DIR_NAME_SPACER 3
#define COUNT_SPACER 20
#define BASE_DIR_FONT_SIZE    +2
#define BASE_COUNT_FONT_SIZE  -1
//#define BASE_COUNT_FONT_SIZE  0

/* --- prototypes --- */
static void dircont_class_init(DirContClass* klass);
static void dircont_init(DirCont* dc);

static gboolean button_press_event(GtkWidget *widget,
		GdkEventButton *event, DirCont* dc);
static gboolean header_expose_event(GtkWidget* header, GdkEventExpose* event,
		DirCont* dc);
static gboolean scrolled_window_expose_event(GtkWidget* header,
		GdkEventExpose* event, DirCont* dc);
static void reset_count_layout(DirCont* dc); 
static GdkPixbuf* create_gradient(GdkColor* color1, GdkColor* color2,
		gint width, gint height, gboolean horz);


/* --- variables --- */
static gpointer parent_class = NULL;
static gint header_height = 0;

static PangoLayout* big_mask_layout = NULL;
static PangoLayout* small_mask_layout = NULL;
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
			count_tag_open->str, "<b>",
			"0123456789()",
			"</b>", count_tag_close->str, NULL);
	pango_layout_set_markup(layout, markup, -1);
	g_free(markup);
	pango_layout_get_pixel_size(layout, NULL, &height);
	header_height += height;
	g_object_unref(G_OBJECT(layout));
	gtk_widget_destroy(area);
//	header_height += 3; /* For spacing */

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
	dc->score = 0;
	dc->is_pwd = FALSE;
	dc->is_active = FALSE;

	/* The header is a drawing area. */
	dc->header = gtk_drawing_area_new();
	gtk_widget_set_size_request(dc->header, -1, header_height);
	gtk_widget_set_state(dc->header, GTK_STATE_ACTIVE);
	gtk_widget_show(dc->header);

	/* Create initial layouts. */
	dc->name_layout = gtk_widget_create_pango_layout(dc->header, NULL);
	dc->counts_layout = gtk_widget_create_pango_layout(dc->header, NULL);

	/* If the scrolled window is not in an event box, it's not possible to
	   paint along its whole height (don't know why), which looks crumby.
	   This widget serves no further function. */
	GtkWidget* paint_event_box = gtk_event_box_new();
	gtk_widget_show(paint_event_box);

	/* Setup the scrolled window (for the file box) */
	dc->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dc->scrolled_window),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	/* Create the file box. */
	dc->file_box = file_box_new();
	wrap_box_set_hspacing(WRAP_BOX(dc->file_box), 3);
	gtk_container_set_border_width(GTK_CONTAINER(dc->file_box), 2);
	wrap_box_set_line_justify(WRAP_BOX(dc->file_box), GTK_JUSTIFY_LEFT);
	gtk_widget_show(dc->file_box);

	g_signal_connect(G_OBJECT(dc->header), "expose-event",
			G_CALLBACK(header_expose_event), dc);
	g_signal_connect(G_OBJECT(dc->scrolled_window), "expose-event",
			G_CALLBACK(scrolled_window_expose_event), dc);
	g_signal_connect(G_OBJECT(dc->header), "button-press-event",
			G_CALLBACK(button_press_event), dc);

	/* Initialize the layout. */
	dircont_set_name(dc, "<unset>");
	dircont_set_counts(dc, "0", "0", "0");

	/* Pack 'em in. */
	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(dc->scrolled_window), dc->file_box);
//	gtk_container_set_resize_mode(GTK_CONTAINER(dc->file_box->parent),
//			GTK_RESIZE_PARENT);
	gtk_box_pack_start(box, dc->header, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(paint_event_box), dc->scrolled_window);
	gtk_box_pack_start(box, paint_event_box, TRUE, TRUE, 0);
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

	if (!STREQ(name, dc->name->str)) {

		dc->name = g_string_assign(dc->name, name);

		gchar* escaped;
		gchar* utf8;
		gchar* markup;
		gsize len;

		utf8 = g_filename_to_utf8(
				dc->name->str, dc->name->len, NULL, &len, NULL);
		escaped = g_markup_escape_text(utf8, len);
		markup = g_strconcat(
				dir_tag_open->str, "<b>",
				escaped,
				"</b>", dir_tag_close->str, NULL);

		pango_layout_set_markup(dc->name_layout, markup, -1);

		g_free(markup);
		g_free(escaped);
		g_free(utf8);
	}
}


void dircont_set_counts(DirCont* dc,
		const gchar* selected, const gchar* total, const gchar* hidden) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));
	g_return_if_fail(selected != NULL);
	g_return_if_fail(total != NULL);
	g_return_if_fail(hidden != NULL);

	gboolean changed = FALSE;

	/* Figure out if anything needs to be changed. */
	if ( (changed |= !STREQ(selected, dc->selected->str)) )
		dc->selected = g_string_assign(dc->selected, selected);
	if ( (changed |= !STREQ(total, dc->total->str)) ) {
		dc->is_restricted = STREQ(total, "0");
		dc->total = g_string_assign(dc->total, total);
	}
	if ( (changed |= !STREQ(hidden, dc->hidden->str)) )
		dc->hidden = g_string_assign(dc->hidden, hidden);

	if (changed)
		reset_count_layout(dc);
}


static void reset_count_layout(DirCont* dc) {
	g_return_if_fail(dc != NULL);

	gchar* markup;

	if (dc->is_restricted) {
		markup = g_strconcat(count_tag_open->str,
				"(Restricted)",
				count_tag_close->str, NULL);
	}
	else {
		markup = g_strconcat(count_tag_open->str, "<b>",
				dc->selected->str, " selected, ",
				dc->total->str, " total (",
				dc->hidden->str, " hidden)",
				"</b>", count_tag_close->str, NULL);
	}
	pango_layout_set_markup(dc->counts_layout, markup, -1);
	g_free(markup);
}


void dircont_repaint_header(DirCont* dc) {
	/* Make sure the header gets redrawn. */
	if (dc->header->window) {
		GdkRectangle rect = { 0, 0, 0, 0, };
		rect.width = dc->header->allocation.width;
		rect.height = dc->header->allocation.height;
		gdk_window_invalidate_rect(dc->header->window, &rect, FALSE);
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
		reset_count_layout(dc);
	}

	if (dc->is_active) {
		gtk_widget_set_state(dc->header, GTK_STATE_SELECTED);
		gtk_widget_show(dc->scrolled_window);
	}
	else {
		gtk_widget_hide(dc->scrolled_window);
		if (dc->is_restricted)
			gtk_widget_set_state(dc->header, GTK_STATE_INSENSITIVE);
		else
			gtk_widget_set_state(dc->header, GTK_STATE_ACTIVE);
	}
}

// TODO connect to scrolled window resize, use that as base for file_box resize


void dircont_set_pwd(DirCont* dc, gboolean setting) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));

	if (setting != dc->is_pwd)
		dc->is_pwd = setting;
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


/* Colour that thin line of space between the window and the scroll bar. */
static gboolean scrolled_window_expose_event(GtkWidget* scrollwin,
		GdkEventExpose *event, DirCont* dc) {

	if (dc->is_active && scrollwin->window) {
		/* Colour the whole area of the scrolled window, just to be sure. */
		GdkGC* light_gc =
			scrollwin->style->light_gc[GTK_WIDGET_STATE(dc->header)];
		gdk_draw_rectangle(
				scrollwin->window,
				light_gc,
				TRUE,
				0, 0,
				scrollwin->allocation.width,
				scrollwin->allocation.height);
	}

	return FALSE;
}


/* Paint the header in various ways, depending on the state of the dc. */
static gboolean header_expose_event(GtkWidget* header, GdkEventExpose* event,
		DirCont* dc) {

	static GdkPixbuf* active_grad = NULL;
	static GdkPixbuf* pwd_grad = NULL;

	GdkGC* fg_gc = header->style->fg_gc[GTK_WIDGET_STATE(header)];
	GdkGC* bg_gc = header->style->bg_gc[GTK_WIDGET_STATE(header)];
	GdkGC* light_gc = header->style->dark_gc[GTK_WIDGET_STATE(header)];

	gint name_width, name_height;
	pango_layout_get_pixel_size(dc->name_layout, &name_width, &name_height);

	if (dc->is_active) {

		if (active_grad == NULL ||
				gdk_pixbuf_get_width(active_grad) !=
				header->allocation.width) {
			active_grad = create_gradient(
					&header->style->dark[GTK_STATE_SELECTED],
					&header->style->light[GTK_STATE_SELECTED],
					header->allocation.width,
					header->allocation.height, FALSE);
		}

		gdk_draw_pixbuf(
				header->window,
				bg_gc,
				active_grad,
				0, 0, 0, 0,
				header->allocation.width, header->allocation.height,
				GDK_RGB_DITHER_NONE, 0, 0);
	}
	else if (dc->is_pwd) {

		if (pwd_grad == NULL || 
				gdk_pixbuf_get_width(pwd_grad) !=
				header->allocation.width) {
			pwd_grad = create_gradient(
					&header->style->dark[GTK_STATE_ACTIVE],
					&header->style->light[GTK_STATE_ACTIVE],
					header->allocation.width,
					header->allocation.height, FALSE);
		}

		gdk_draw_pixbuf(
				header->window,
				bg_gc,
				pwd_grad,
				0, 0, 0, 0,
				header->allocation.width, header->allocation.height,
				GDK_RGB_DITHER_NONE, 0, 0);

	}
	else {
		gdk_draw_rectangle(
				header->window,
				bg_gc,
				TRUE,
				0, 0,
				header->allocation.width, name_height);
	}

	/* Draw separator. */
	if (!dc->is_active && dc->rank != 0) {
		gdk_draw_rectangle(
				header->window,
				fg_gc,
				TRUE,
				0, 0,
				header->allocation.width, 1);
	}


	/* Draw name */
	gdk_draw_layout(
			header->window,
			fg_gc,
			DIR_NAME_SPACER, 0,
			dc->name_layout);

	/* Draw file counts */
	gdk_draw_layout(
			header->window,
			fg_gc,
			COUNT_SPACER, name_height,
			dc->counts_layout);


	/* If this is the active dc, show the mask as well. */
	if (dc->is_active) {
		gint mask_width;
		pango_layout_get_pixel_size(big_mask_layout, &mask_width, NULL);

		if (3*DIR_NAME_SPACER + name_width + mask_width <
				header->allocation.width && big_mask_layout) {
			/* Use the big mask layout. */
			gdk_draw_layout(
					header->window,
					fg_gc,
					header->allocation.width - mask_width - DIR_NAME_SPACER,
					2,
					big_mask_layout);
		}
		else if (small_mask_layout) {
			/* Use the small mask layout. */

			pango_layout_get_pixel_size(small_mask_layout, &mask_width, NULL);
			gdk_draw_layout(
					header->window,
					fg_gc,
					header->allocation.width - mask_width - COUNT_SPACER,
					name_height,
					small_mask_layout);
		}
	}


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
void dircont_set_mask_string(DirCont* dc, const gchar* mask_str) {

	/* Initialize the layouts if necessary. */
	if (!big_mask_layout || !small_mask_layout) {
		GtkWidget* area = gtk_drawing_area_new();
		big_mask_layout = gtk_widget_create_pango_layout(area, NULL);
		small_mask_layout = gtk_widget_create_pango_layout(area, NULL);
		gtk_widget_destroy(area);
	}

	/* Big mask layout (when directory name is short) */
	gchar* markup = g_strconcat(
			dir_tag_open->str, "<small><b>",
			mask_str,
			"</b></small>", dir_tag_close->str, NULL);
	pango_layout_set_markup(big_mask_layout, markup, -1);
	g_free(markup);


	/* Small mask layout (when directory name is long) */
	markup = g_strconcat(
			count_tag_open->str, "<b>",
			"Mask: ", mask_str,
			"</b>", count_tag_close->str, NULL);
	pango_layout_set_markup(small_mask_layout, markup, -1);
	g_free(markup);
}


/* Navigate the scrolled window for the given dc. */
void dircont_nav(DirCont* dc, DirContNav nav) {
	g_return_if_fail(dc != NULL);
	g_return_if_fail(IS_DIRCONT(dc));

	gdouble upper, lower, current, step_inc, page_inc, change;

	GtkAdjustment* vadj= gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(dc->scrolled_window));

	current = gtk_adjustment_get_value(vadj);
	page_inc = vadj->page_increment;
	step_inc = vadj->step_increment;
	lower = vadj->lower;
	change = 0;

	/* Otherwise we scroll down into a page of black. */
	upper = vadj->upper - page_inc - step_inc;

	switch (nav) {
		case DCN_PGUP:
			change = -page_inc;
			break;
		case DCN_PGDOWN:
			change = +page_inc;
			break;
		default:
			g_return_if_reached();
	}

	/* Set the value. */
	if (change)
		gtk_adjustment_set_value(vadj, CLAMP(current + change, lower, upper));
}


/* Grabbed this from xfce4.  It may have been written by any of the following
   people:
	   Brian Tarricone <bjt23@cornell.edu>
	   Jasper Huijsmans <huysmans@users.sourceforge.net>
	   Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de> */
static GdkPixbuf*  create_gradient(GdkColor* color1, GdkColor* color2,
		gint width, gint height, gboolean horz) {

	g_return_val_if_fail(color1 != NULL && color2 != NULL, NULL);
	g_return_val_if_fail(width > 0 && height > 0, NULL);

	GdkPixbuf *pix;
	gint i, j;
	GdkPixdata pixdata;
	guint8 rgb[3];
    GError *err = NULL;

	pixdata.magic = GDK_PIXBUF_MAGIC_NUMBER;
	pixdata.length = GDK_PIXDATA_HEADER_LENGTH + (width * height * 3);
	pixdata.pixdata_type = GDK_PIXDATA_COLOR_TYPE_RGB |
		GDK_PIXDATA_SAMPLE_WIDTH_8 | GDK_PIXDATA_ENCODING_RAW;
	pixdata.rowstride = width * 3;
	pixdata.width = width;
	pixdata.height = height;
	pixdata.pixel_data = g_malloc(width * height * 3);

	if(horz) {
		for(i = 0; i < width; i++) {
			rgb[0] = (color1->red +
					(i * (color2->red - color1->red) / width)) >> 8;
			rgb[1] = (color1->green +
					(i * (color2->green - color1->green) / width)) >> 8;
			rgb[2] = (color1->blue +
					(i * (color2->blue - color1->blue) / width)) >> 8;
			memcpy(pixdata.pixel_data + (i * 3), rgb, 3);
		}
		
		for(i = 1; i < height; i++) {
			memcpy(pixdata.pixel_data + (i * pixdata.rowstride),
					pixdata.pixel_data, pixdata.rowstride);
		}
	}
	else {
		for(i = 0; i < height; i++) {
			rgb[0] = (color1->red +
					(i * (color2->red - color1->red) / height)) >> 8;
			rgb[1] = (color1->green
					+ (i * (color2->green - color1->green) / height)) >> 8;
			rgb[2] = (color1->blue
					+ (i * (color2->blue - color1->blue) / height)) >> 8;
			for(j = 0; j < width; j++) {
				memcpy(pixdata.pixel_data + (i * pixdata.rowstride) + (j * 3),
						rgb, 3);
			}
		}
	}
	
	pix = gdk_pixbuf_from_pixdata(&pixdata, TRUE, &err);
	if (!pix) {
		g_warning("Unable to create color gradient: %s", err->message);
		g_error_free(err);
	}

	return pix;
}

