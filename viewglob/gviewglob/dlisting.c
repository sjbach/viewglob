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
#include "file_box.h"
#include "feedback.h"
#include "dlisting.h"
#include <gtk/gtk.h>
#include <string.h>   /* For strcmp */

#if DEBUG_ON
extern FILE* df;
#endif

#define WIDTH_BUFFER 0
#define BASE_DIR_FONT_SIZE    +2
#define BASE_COUNT_FONT_SIZE  -1

/* --- prototypes --- */
static void  dlisting_class_init(DListingClass* klass);
static void  dlisting_init(DListing* dl);
static void  dlisting_size_request(GtkWidget* widget, GtkRequisition* requisition);
static void  dlisting_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static void  dlisting_reset_file_count_label(DListing* dl);

static gboolean is_zero(const gchar* string);

static gboolean dlisting_button_press_event(GtkWidget *widget, GdkEventButton *event, DListing* dl);
static void show_hidden_files_activate_handler(GtkMenuItem* menu_item, DListing* dl);
static void show_all_files_activate_handler(GtkMenuItem* menu_item, DListing* dl);


/* --- variables --- */
static gpointer parent_class = NULL;
static GdkColor separator_color = { 0, 0, 0, 0 };
static GdkPixbuf* show_hidden_pixbuf = NULL;
static GdkPixbuf* show_all_pixbuf = NULL;
static gboolean show_hidden_files;
static gboolean show_all_files;

static gchar* directory_font_tags_open = NULL;
static gchar* directory_font_tags_close = NULL;
static gchar* file_count_font_tags_open = NULL;
static gchar* file_count_font_tags_close = NULL;

/* --- functions --- */
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
	widget_class->size_request = dlisting_size_request;
	widget_class->size_allocate = dlisting_size_allocate;
}


static void dlisting_init(DListing* dl) {
	GTK_WIDGET_SET_FLAGS(dl, GTK_NO_WINDOW);

	GtkBox* box = GTK_BOX(dl);
	GtkWidget* dir_heading_vbox;
	GtkWidget* dir_heading_separator;
	GtkWidget* left_spacer;

	GtkWidget* menu_item_image;
	GtkWidget* image_menu_item;

	dl->name = NULL;
	dl->old_rank = -1;
	dl->rank = -1;
	dl->marked = FALSE;

	dl->name = g_string_new("");
	dl->rank = -1;
	dl->old_rank = -1;

	dl->selected_count = g_string_new("0");
	dl->total_count = g_string_new("0");
	dl->hidden_count = g_string_new("0");

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

	/* Setup the context menu. */
	dl->menu = gtk_menu_new();

	/* Show Hidden. */
	image_menu_item = gtk_image_menu_item_new_with_mnemonic("Show _hidden files");
	if (show_hidden_pixbuf) {
		menu_item_image = gtk_image_new_from_pixbuf(show_hidden_pixbuf);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(image_menu_item), menu_item_image);
		gtk_widget_show(menu_item_image);
	}
	gtk_widget_show(image_menu_item);
	if (show_hidden_files) {
		/* Hidden files are always shown -- this item is redundant. */
		gtk_widget_set_state(image_menu_item, GTK_STATE_INSENSITIVE);
	}
	else
		g_signal_connect(image_menu_item, "activate", G_CALLBACK(show_hidden_files_activate_handler), dl);
	gtk_menu_append(GTK_MENU_SHELL(dl->menu), image_menu_item);

	/* Show All. */
	image_menu_item = gtk_image_menu_item_new_with_mnemonic("Show _all files");
	if (show_all_pixbuf) {
		menu_item_image = gtk_image_new_from_pixbuf(show_all_pixbuf);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(image_menu_item), menu_item_image);
		gtk_widget_show(menu_item_image);
	}
	gtk_widget_show(image_menu_item);
	if (show_all_files) {
		/* There's no file display limit -- this item is redundant. */
		gtk_widget_set_state(image_menu_item, GTK_STATE_INSENSITIVE);
	}
	else
		g_signal_connect(image_menu_item, "activate", G_CALLBACK(show_all_files_activate_handler), dl);
	gtk_menu_append(GTK_MENU_SHELL(dl->menu), image_menu_item);

	/* For right-click context menu and double left-click. */
	g_signal_connect(dl->heading_event_box, "button-press-event", G_CALLBACK(dlisting_button_press_event), dl);

	gtk_widget_show(dl->menu);

	/* Initialize the labels. */
	dlisting_set_name(dl, "<unset>");
	dlisting_reset_file_count_label(dl);
}


static void dlisting_size_request(GtkWidget* widget, GtkRequisition* requisition) {
	DListing* dl;
	GtkRequisition heading_req = { 0, 0 }, file_box_req = { 0, 0 };

	dl = DLISTING(widget);
	
	if (GTK_WIDGET_VISIBLE(GTK_WIDGET(dl->heading_event_box)))
		gtk_widget_size_request(GTK_WIDGET(dl->heading_event_box), &heading_req);
	if (GTK_WIDGET_VISIBLE(GTK_WIDGET(dl->file_box)))
		gtk_widget_size_request(GTK_WIDGET(dl->file_box), &file_box_req);

	requisition->width = heading_req.width + file_box_req.width + GTK_CONTAINER(dl)->border_width * 2;
	requisition->height = heading_req.height + file_box_req.height + GTK_CONTAINER(dl)->border_width * 2;

	requisition->width = MIN(requisition->width, dl->optimal_width - WIDTH_BUFFER);
	//g_printerr("(%d %d)", requisition->width, requisition->height);
}



static void dlisting_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
	DListing* dl;
	GtkRequisition child_requisition;
	GtkAllocation child_allocation;

	dl = DLISTING(widget);

	child_allocation.x = allocation->x + GTK_CONTAINER(dl)->border_width;
	child_allocation.width = MIN(child_allocation.width, dl->optimal_width - WIDTH_BUFFER);
	child_allocation.width = MAX(1, (gint) allocation->width - (gint) GTK_CONTAINER(dl)->border_width * 2);

	gtk_widget_get_child_requisition(GTK_WIDGET(dl->heading_event_box), &child_requisition);
	child_allocation.height = MIN(child_requisition.height, allocation->height);
	child_allocation.y = allocation->y + GTK_CONTAINER(dl)->border_width;

	gtk_widget_size_allocate (GTK_WIDGET(dl->heading_event_box), &child_allocation);

	child_allocation.y = child_allocation.y + child_allocation.height;
	child_allocation.height = MAX(1, allocation->height - child_allocation.height);

	gtk_widget_size_allocate (GTK_WIDGET(dl->file_box), &child_allocation);
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


void dlisting_set_show_hidden_pixbuf(GdkPixbuf* pixbuf) {
	show_hidden_pixbuf = pixbuf;
}


void dlisting_set_show_all_pixbuf(GdkPixbuf* pixbuf) {
	show_all_pixbuf = pixbuf;
}


void dlisting_set_show_hidden_files(gboolean show_hidden) {
	show_hidden_files = show_hidden;
}


void dlisting_set_show_all_files(gboolean show_all) {
	show_all_files = show_all;
}

/* Set the sizes of the directory heading font and the file count font. */
void dlisting_set_sizing(gint modifier) {

	gint dir_size, count_size;
	gchar* temp;

	gint i;

	g_return_if_fail(modifier <= 10);   /* Constrain between -10 and 10. */
	g_return_if_fail(modifier >= -10);

	g_free(directory_font_tags_open);
	g_free(directory_font_tags_close);
	g_free(file_count_font_tags_open);
	g_free(file_count_font_tags_close);

	directory_font_tags_open = g_strdup("");
	directory_font_tags_close = g_strdup("");
	file_count_font_tags_open = g_strdup("");
	file_count_font_tags_close = g_strdup("");

	dir_size = BASE_DIR_FONT_SIZE + modifier;
	count_size = BASE_COUNT_FONT_SIZE + modifier;

	/* Directory label sizing. */
	if (dir_size > 0) {
		for (i = 0; i < dir_size; i++) {
			temp = directory_font_tags_open;
			directory_font_tags_open = g_strconcat(
					directory_font_tags_open, "<big>", NULL);
			g_free (temp);

			temp = directory_font_tags_close;
			directory_font_tags_close = g_strconcat(
					directory_font_tags_close, "</big>", NULL);
			g_free (temp);
		}
	}
	else if (dir_size < 0) {
		for (i = 0; i > dir_size; i--) {
			temp = directory_font_tags_open;
			directory_font_tags_open = g_strconcat(
					directory_font_tags_open, "<small>", NULL);
			g_free (temp);

			temp = directory_font_tags_close;
			directory_font_tags_close = g_strconcat(
					directory_font_tags_close, "</small>", NULL);
			g_free (temp);
		}
	}

	/* File count label sizing. */
	if (count_size > 0) {
		for (i = 0; i < count_size; i++) {
			temp = file_count_font_tags_open;
			file_count_font_tags_open = g_strconcat(
					file_count_font_tags_open, "<big>", NULL);
			g_free (temp);

			temp = file_count_font_tags_close;
			file_count_font_tags_close = g_strconcat(
					file_count_font_tags_close, "</big>", NULL);
			g_free (temp);
		}
	}
	else if (count_size < 0) {
		for (i = 0; i > count_size; i--) {
			temp = file_count_font_tags_open;
			file_count_font_tags_open = g_strconcat(
					file_count_font_tags_open, "<small>", NULL);
			g_free (temp);

			temp = file_count_font_tags_close;
			file_count_font_tags_close = g_strconcat(
					file_count_font_tags_close, "</small>", NULL);
			g_free (temp);
		}
	}
}




static void dlisting_reset_file_count_label(DListing* dl) {
	gchar* temp_string;
	gchar* markup;

	glong n_displayed;
	guint display_limit;

	display_limit = file_box_get_file_display_limit( FILE_BOX(dl->file_box) );

	if (file_box_get_show_hidden_files(FILE_BOX(dl->file_box)))
		n_displayed = atol(dl->total_count->str);
	else
		n_displayed = atol(dl->total_count->str) - atol(dl->hidden_count->str);

	if (is_zero(dl->total_count->str))
		temp_string = g_strdup("(Restricted)");
	else if (display_limit == 0 || n_displayed <= display_limit) {
		temp_string = g_strconcat(
				dl->selected_count->str, " selected, ",
				dl->total_count->str, " total (",
				dl->hidden_count->str, " hidden)", NULL);
	}
	else {
		temp_string = g_strconcat(
				dl->selected_count->str, " selected, <b>",
				dl->total_count->str, "</b> total (",
				dl->hidden_count->str, " hidden) [Results truncated]", NULL);
	}

	if (file_count_font_tags_open && file_count_font_tags_close) {
		markup = g_strconcat(
				file_count_font_tags_open,
				temp_string,
				file_count_font_tags_close, NULL);
		g_free(temp_string);
	}
	else
		markup = temp_string;

	gtk_label_set_markup(GTK_LABEL(dl->count_label), markup);
	g_free(markup);
}


/* Must first convert new_name to utf8, then escape it, then add markup. */
void dlisting_set_name(DListing* dl, const gchar* new_name) {
	gchar* utf8;
	gchar* escaped;
	gchar* markup;
	gsize length;

	g_return_if_fail(IS_DLISTING(dl));
	g_return_if_fail(new_name != NULL);

	g_string_assign(dl->name, new_name);

	utf8 = g_filename_to_utf8(new_name, strlen(new_name), NULL, &length, NULL);
	escaped = g_markup_escape_text(utf8, length);

	/* Add markup. */
	if (directory_font_tags_open && directory_font_tags_close) {
		markup = g_strconcat(
				"<b>", directory_font_tags_open,
				escaped, directory_font_tags_close,
				"</b>", NULL);
	}
	else
		markup = g_strconcat("<b>", escaped, "</b>", NULL);

	gtk_label_set_markup(GTK_LABEL(dl->name_label), markup);

	g_free(utf8);
	g_free(escaped);
	g_free(markup);
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
	file_box_set_optimal_width(FILE_BOX(dl->file_box), dl->optimal_width - WIDTH_BUFFER);

	gtk_widget_queue_resize(GTK_WIDGET(dl->heading_event_box));
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

	gtk_widget_destroy(dl->menu);    /* Delete the menu.  This should get the menu items too. */

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


static gboolean dlisting_button_press_event(GtkWidget *widget, GdkEventButton *event, DListing* dl) {

	GString* string;

	g_return_val_if_fail(dl != NULL, FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		/* Right-click -- popup the context menu. */
		gtk_menu_popup (GTK_MENU(dl->menu), NULL, NULL, NULL, NULL, event->button, event->time);
	}
	else if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		/* Double left-click -- write out the directory name. */
		string = g_string_new("file:");
		string = g_string_append(string, dl->name->str);

		/* Append a trailing '/' if need be. */
		if ( *(string->str + string->len - 1) != '/')
			string = g_string_append(string, "/");

		feedback_write_string(string->str, string->len + 1);

		g_string_free(string, TRUE);
	}

	return FALSE;
}


static void show_hidden_files_activate_handler(GtkMenuItem* menu_item, DListing* dl) { 
	GtkWidget* widget;

	g_return_if_fail(GTK_IS_MENU_ITEM(menu_item));
	g_return_if_fail(IS_DLISTING(dl));
	widget = GTK_WIDGET(dl);

	file_box_set_show_hidden_files(FILE_BOX(dl->file_box), TRUE);
	dlisting_reset_file_count_label(dl);
	gtk_widget_set_state(GTK_WIDGET(menu_item), GTK_STATE_INSENSITIVE);

	if (widget->parent) {
		/* Update scrollbar. */
		gtk_widget_queue_resize(widget->parent);
	}
}


static void show_all_files_activate_handler(GtkMenuItem* menu_item, DListing* dl) { 
	GtkWidget* widget;

	g_return_if_fail(GTK_IS_MENU_ITEM(menu_item));
	g_return_if_fail(IS_DLISTING(dl));
	widget = GTK_WIDGET(dl);

	file_box_set_file_display_limit(FILE_BOX(dl->file_box), 0);
	dlisting_reset_file_count_label(dl);
	gtk_widget_set_state(GTK_WIDGET(menu_item), GTK_STATE_INSENSITIVE);

	if (widget->parent) {
		/* Update scrollbar. */
		gtk_widget_queue_resize(widget->parent);
	}
}


