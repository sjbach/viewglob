/*
	Copyright (C) 2004 Stephen Bach
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
#include "file_box.h"
#include "dlisting.h"
#include <gtk/gtk.h>
#include <string.h>   /* For strcmp */

#if DEBUG_ON
extern FILE* df;
#endif

/* --- prototypes --- */
static void  dlisting_class_init(DListingClass* klass);
static void  dlisting_init(DListing* dl);
static void  dlisting_reset_file_count_label(DListing* dl);

static gboolean is_zero(const gchar* string);

/* --- variables --- */
static gpointer parent_class = NULL;
static GdkColor separator_color = { 0, 0, 0, 0 };

/*--- functions --- */
GType dlisting_get_type(void) {
	static GType dlisting_type = 0;

	if (!dlisting_type) {
		static const GTypeInfo dlisting_info = {
			sizeof (DListingClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) dlisting_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (DListing),
			0,		/* n_preallocs */
			(GInstanceInitFunc) dlisting_init,
		};
		dlisting_type = g_type_register_static (GTK_TYPE_VBOX, "DListing", &dlisting_info, 0);
	}

	return dlisting_type;
}


static void dlisting_class_init(DListingClass* class) {
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;


	object_class = G_OBJECT_CLASS(class);
	widget_class = GTK_WIDGET_CLASS(class);

	parent_class = g_type_class_peek_parent(class);
	//widget_class->size_request = dlisting_size_request;
	//widget_class->size_allocate = dlisting_size_allocate;
}


static void dlisting_init(DListing* dl) {
	GTK_WIDGET_SET_FLAGS(dl, GTK_NO_WINDOW);

	//GtkWidget* vbox = GTK_VBOX(dl);
	GtkBox* box = GTK_BOX(dl);
	//GtkWidget* dir_heading_event_box;
	GtkWidget* dir_heading_vbox;
	GtkWidget* dir_heading_separator;
	GtkWidget* left_spacer;
	//GtkWidget* file_num_label;

	// FIXME
	//GtkWidget* menu;
	//GtkWidget* menu_item_image;
	//GtkWidget* image_menu_item;

	dl->name = NULL;
	dl->old_rank = -1;
	dl->rank = -1;
	dl->marked = FALSE;

	dl->selected_count = NULL;
	dl->total_count = NULL;
	dl->hidden_count = NULL;

	dl->name_label = NULL;
	dl->count_label = NULL;
	dl->menu = NULL;
	dl->file_box = NULL;

	//dl->widget = NULL;

	//dl->name = g_string_new("<unset>");
	dl->name = g_string_new("");
	dl->rank = -1;
	dl->old_rank = -1;
	dl->marked = TRUE;

	dl->selected_count = g_string_new("0");
	dl->total_count = g_string_new("0");
	dl->hidden_count = g_string_new("0");

	//gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
	gtk_box_set_homogeneous(box, FALSE);

	/* Event box for the directory header (so it can be a different colour). */
	dl->heading_event_box = gtk_event_box_new();
	gtk_widget_set_state(dl->heading_event_box, GTK_STATE_ACTIVE);
	gtk_box_pack_start(box, dl->heading_event_box, FALSE, FALSE, 0);
	gtk_widget_show(dl->heading_event_box);

	/* Directory header vbox. */
	dir_heading_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_set_homogeneous(GTK_BOX(dir_heading_vbox), FALSE);
	gtk_container_add(GTK_CONTAINER(dl->heading_event_box), dir_heading_vbox);
	gtk_widget_show(dir_heading_vbox);

	/* Create black-line separator. */
	dir_heading_separator = gtk_hseparator_new();
	//gtk_widget_modify_bg(dir_heading_separator, GTK_STATE_NORMAL, v.separator_color);
	gtk_widget_modify_bg(dir_heading_separator, GTK_STATE_NORMAL, &separator_color);
	gtk_box_pack_start(GTK_BOX(dir_heading_vbox), dir_heading_separator, FALSE, FALSE, 0);
	gtk_widget_show(dir_heading_separator);

	/* Create directory label and align it to the left. */
	dl->name_label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(dl->name_label), 0, 0);
	left_spacer = gtk_alignment_new(0, 0, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(left_spacer), 0, 0, 2, 0);
	gtk_container_add(GTK_CONTAINER(left_spacer), dl->name_label);
	gtk_box_pack_start(GTK_BOX(dir_heading_vbox), left_spacer, FALSE, FALSE, 0);
	gtk_widget_show(left_spacer);
	gtk_widget_show(dl->name_label);

	/* Create file selection number label and align it to the left. */
	dl->count_label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(dl->count_label), 0, 0);
	left_spacer = gtk_alignment_new(0, 0, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(left_spacer), 0, 0, 2, 0);
	gtk_container_add(GTK_CONTAINER(left_spacer), dl->count_label);
	gtk_box_pack_start(GTK_BOX(dir_heading_vbox), left_spacer, FALSE, FALSE, 0);
	gtk_widget_show(left_spacer);
	gtk_widget_show(dl->count_label);

	/* Create the file box. */
	dl->file_box = file_box_new();
	wrap_box_set_hspacing(WRAP_BOX(dl->file_box), 3);
	gtk_container_set_border_width(GTK_CONTAINER(dl->file_box), 2);
	wrap_box_set_line_justify(WRAP_BOX(dl->file_box), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(box, dl->file_box, FALSE, FALSE, 0);
	gtk_widget_show(dl->file_box);

	dlisting_set_name(dl, "<unset>");
	dlisting_reset_file_count_label(dl);

	/* TODO: context menu */
}


GtkWidget* dlisting_new (void) {
	return g_object_new (DLISTING_TYPE, NULL);
}


void dlisting_set_separator_color(GdkColor color) {
	separator_color.pixel = color.pixel;
	separator_color.red = color.red;
	separator_color.green = color.green;
	separator_color.blue = color.blue;
}


static void dlisting_reset_file_count_label(DListing* dl) {
	gchar* temp_string;

	glong n_displayed;
	guint display_limit;

	display_limit = file_box_get_file_display_limit( FILE_BOX(dl->file_box) );

	if (file_box_get_show_hidden_files(FILE_BOX(dl->file_box)))
		n_displayed = atol(dl->total_count->str);
	else
		n_displayed = atol(dl->total_count->str) - atol(dl->hidden_count->str);

	if (is_zero(dl->total_count->str))
		temp_string = g_strdup("<small>(Restricted)</small>");
	else if (display_limit == 0 || n_displayed <= display_limit) {
		temp_string = g_strconcat("<small>",
				dl->selected_count->str, " selected, ",
				dl->total_count->str, " total (",
				dl->hidden_count->str, " hidden)</small>", NULL);
	}
	else {
			temp_string = g_strconcat("<small>",
					dl->selected_count->str, " selected, <b>",
					dl->total_count->str, "</b> total (",
					dl->hidden_count->str, " hidden) [Results truncated]</small>", NULL);
	}

	gtk_label_set_markup(GTK_LABEL(dl->count_label), temp_string);
	g_free(temp_string);
}


/* Must first convert new_name to utf8, then escape it, then add markup. */
void dlisting_set_name(DListing* dl, const gchar* new_name) {
	gchar* temp1;
	gchar* temp2;
	gchar* temp3;
	gsize length;

	g_return_if_fail(IS_DLISTING(dl));
	g_return_if_fail(new_name != NULL);

	g_string_assign(dl->name, new_name);

	temp1 = g_filename_to_utf8(new_name, strlen(new_name), NULL, &length, NULL);
	temp2 = g_markup_escape_text(temp1, length);
	temp3 = g_strconcat("<b><big><big>", temp2, "</big></big></b>", NULL);
	gtk_label_set_markup(GTK_LABEL(dl->name_label), temp3);

	g_free(temp1);
	g_free(temp2);
	g_free(temp3);
}


void dlisting_set_file_counts(DListing* dl, const gchar* selected, const gchar* total, const gchar* hidden) {

	gboolean change = FALSE;

	g_return_if_fail(IS_DLISTING(dl));
	g_return_if_fail(selected != NULL);
	g_return_if_fail(total != NULL);
	g_return_if_fail(hidden != NULL);

	/* Color the heading event box. */
	if (is_zero(total))
		gtk_widget_set_state(dl->heading_event_box, GTK_STATE_SELECTED);
	else
		gtk_widget_set_state(dl->heading_event_box, GTK_STATE_ACTIVE);


	/* Compare selected count. */
	if (strcmp(dl->selected_count->str, selected) != 0) {
		g_string_assign(dl->selected_count, selected);
		change = TRUE;
	}

	/* Compare total file count. */
	if (strcmp(dl->total_count->str, total) != 0) {
		g_string_assign(dl->total_count, total);
		change = TRUE;
	}

	/* Compare hidden count. */
	if (strcmp(dl->hidden_count->str, hidden) != 0) {
		g_string_assign(dl->hidden_count, hidden);
		change = TRUE;
	}

	if (change)
		dlisting_reset_file_count_label(dl);
}


void dlisting_set_optimal_width(DListing* dl, gint width) {

	g_return_if_fail(IS_DLISTING(dl));
	g_return_if_fail(width >= 0);

	dl->optimal_width = width;
	file_box_set_optimal_width(FILE_BOX(dl->file_box), dl->optimal_width);
	// FIXME
}


void dlisting_mark(DListing* dl, gint new_rank) {
	g_return_if_fail(IS_DLISTING(dl));

	dl->marked = TRUE;
	dl->old_rank = dl->rank;
	dl->rank = new_rank;
}


gboolean dlisting_is_new(const DListing* dl) {
	g_return_val_if_fail(IS_DLISTING(dl), TRUE);

	if (dl->old_rank > 0)
		return FALSE;
	else
		return TRUE;
}


/* Remove all memory associated with this DListing. */
void dlisting_free(DListing* dl) {

	GtkWidget* widget;
	
	g_return_if_fail(IS_DLISTING(dl));
	
	widget = GTK_WIDGET(dl);
	gtk_widget_hide(widget);

	file_box_destroy(FILE_BOX(dl->file_box));   /* This also gets the FItems associated with the file box. */

	//FIXME
	//gtk_widget_destroy(dl->menu);    /* Delete the menu.  This should get the menu items too. */

	g_string_free(dl->name, TRUE);
	g_string_free(dl->selected_count, TRUE);
	g_string_free(dl->total_count, TRUE);
	g_string_free(dl->hidden_count, TRUE);

	gtk_widget_destroy(widget);                 /* Should take care of everything else. */
}


static gboolean is_zero(const gchar* string) {
	if (strcmp(string, "0") == 0)
		return TRUE;
	else
		return FALSE;
}

