/*
	Copyright (C) 2004, 2005 Stephen Bach
	This file is part of the viewglob package.

	viewglob is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	viewglob is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with viewglob; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "common.h"
#include "shrink_label.h"
#include <gtk/gtk.h>

#if DEBUG_ON
extern FILE* df;
#endif

/* --- prototypes --- */
static void  shrink_label_class_init(ShrinkLabelClass* klass);
static void  shrink_label_init(ShrinkLabel* sl);
static void  shrink_label_size_request(GtkWidget* widget, GtkRequisition* requisition);
static void shrink_label_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static void  shrink_label_choose_text(ShrinkLabel* sl, GtkAllocation a);

/* --- functions --- */
GType shrink_label_get_type(void) {
	static GType shrink_label_type = 0;

	if (!shrink_label_type) {
		static const GTypeInfo shrink_label_info = {
			sizeof (ShrinkLabelClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) shrink_label_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (ShrinkLabel),
			0,		/* n_preallocs */
			(GInstanceInitFunc) shrink_label_init,
		};
		shrink_label_type = g_type_register_static (GTK_TYPE_LABEL, "ShrinkLabel", &shrink_label_info, 0);
	}

	return shrink_label_type;
}


static void shrink_label_class_init(ShrinkLabelClass* class) {
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;


	object_class = G_OBJECT_CLASS(class);
	widget_class = GTK_WIDGET_CLASS(class);

	//parent_class = g_type_class_peek_parent(class);
	widget_class->size_request = shrink_label_size_request;
	widget_class->size_allocate = shrink_label_size_allocate;
}


static void shrink_label_init(ShrinkLabel* sl) {
	GTK_WIDGET_SET_FLAGS(sl, GTK_NO_WINDOW);
	sl->long_text = NULL;
	sl->short_text = NULL;
}


GtkWidget* shrink_label_new (void) {
	return g_object_new (SHRINK_LABEL_TYPE, NULL);
}


static void shrink_label_size_request(GtkWidget* widget, GtkRequisition* requisition) {

	ShrinkLabel* sl;

	GtkWidget* test_label;
	PangoLayout* test_layout;
	
	//gint width, height;
	
	g_return_if_fail(IS_SHRINK_LABEL(widget));
	g_return_if_fail(requisition != NULL);

	sl = SHRINK_LABEL(widget);

	if (!sl->long_text) {
		requisition->height = 0;
		requisition->width = 0;
	}
	else {
		test_label = gtk_label_new(sl->long_text);
		test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
		pango_layout_get_pixel_size(test_layout, &(requisition->width), &(requisition->height));
		gtk_widget_destroy(test_label);
	}

	g_print("(requesting: %d %d)", requisition->width, requisition->height);
}


static void shrink_label_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {

	g_print("(got: %d %d)", allocation->width, allocation->height);
	shrink_label_choose_text(SHRINK_LABEL(widget), *allocation);

}


void shrink_label_set_long_text(ShrinkLabel* sl, gchar* text) {

	g_return_if_fail(IS_SHRINK_LABEL(sl));

	if (sl->long_text)
		g_free(sl->long_text);
	sl->long_text = g_strdup(text);
	shrink_label_choose_text(sl, GTK_WIDGET(sl)->allocation);
}


void shrink_label_set_short_text(ShrinkLabel* sl, gchar* text) {

	g_return_if_fail(IS_SHRINK_LABEL(sl));

	if (sl->short_text)
		g_free(sl->short_text);
	sl->short_text = g_strdup(text);
	shrink_label_choose_text(sl, GTK_WIDGET(sl)->allocation);
}


static void shrink_label_choose_text(ShrinkLabel* sl, GtkAllocation a) {
	GtkWidget* test_label;
	PangoLayout* test_layout;
	guint width, height;
	gchar* shrink_text;

	g_return_if_fail(sl->long_text != NULL);

	/* Try the long text first. */
	shrink_text = sl->long_text;
	test_label = gtk_label_new(sl->long_text);
	test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
	pango_layout_get_pixel_size(test_layout, &width, &height);

	if (width <= a.width) {
		gtk_label_set_text(GTK_LABEL(sl), shrink_text);
		gtk_widget_destroy(test_label);
		return;
	}
	else if (sl->short_text) {
		/* Try the short text if present. */
		shrink_text = sl->short_text;
		gtk_label_set_text(GTK_LABEL(test_label), sl->short_text);
		test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
		pango_layout_get_pixel_size(test_layout, &width, &height);

		if (width <= a.width) {
			gtk_label_set_text(GTK_LABEL(sl), shrink_text);
			gtk_widget_destroy(test_label);
			return;
		}
	}

	/* Convert the last three characters to dots. */
	GString* text = g_string_new(shrink_text);
	g_string_truncate(text, MAX(text->len - 3, 0));
	g_string_append(text, "...");

	gtk_label_set_text(GTK_LABEL(test_label), text->str);
	test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
	pango_layout_get_pixel_size(test_layout, &width, &height);

	/* Strip off the characters, one by one, until the size is right. */
	while (width > a.width && text->len > 3) {
		g_string_truncate(text, MAX(text->len - 4, 0));
		g_string_append(text, "...");
		gtk_label_set_text(GTK_LABEL(test_label), text->str);
		test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
		pango_layout_get_pixel_size(test_layout, &width, &height);
	}

	if (text->len <= 3)
		g_warning("<= 3 in shrink_label_choose_text");

	gtk_label_set_text(GTK_LABEL(sl), text->str);
	g_string_free(text, TRUE);
	gtk_widget_destroy(test_label);
}

