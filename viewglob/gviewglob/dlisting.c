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
#include "gviewglob.h"
#include "wrap_box.h"
#include "file_box.h"
#include <string.h>    /* For strcmp. */

#if DEBUG_ON
extern FILE* df;
#endif

extern struct viewable_preferences v;

static gint show_context_menu(GtkWidget *widget, GdkEvent *event);
static void show_hidden_files_activate_handler(GtkMenuItem* menu_item, DListing* dl);
static void show_all_files_activate_handler(GtkMenuItem* menu_item, DListing* dl);


static gint show_context_menu(GtkWidget *widget, GdkEvent *event) {
	GtkMenu* menu;
	GdkEventButton* event_button;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	menu = GTK_MENU(widget);

	if (event->type == GDK_BUTTON_PRESS) {
		event_button = (GdkEventButton*) event;
		if (event_button->button == 3) {
			gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event_button->button, event_button->time);
			return TRUE;
		}
	}

	return FALSE;
}


static void show_hidden_files_activate_handler(GtkMenuItem* menu_item, DListing* dl) { 

	DEBUG((df, "in show_hidden_blah: %s\n", dl->name->str));
	file_box_set_show_hidden_files(FILE_BOX(dl->file_box), TRUE);
	dlisting_reset_file_count_label(dl);
	gtk_widget_set_state(GTK_WIDGET(menu_item), GTK_STATE_INSENSITIVE);
}


static void show_all_files_activate_handler(GtkMenuItem* menu_item, DListing* dl) { 

	DEBUG((df, "in show_all_blah: %s\n", dl->name->str));
	file_box_set_file_display_limit(FILE_BOX(dl->file_box), 0);
	dlisting_reset_file_count_label(dl);
	gtk_widget_set_state(GTK_WIDGET(menu_item), GTK_STATE_INSENSITIVE);
}


DListing* dlisting_new(const GString* name, gint rank, const GString* selected_count, const GString* total_count, const GString* hidden_count, gint width) {
	DListing* new_dl;

	GtkWidget* vbox;
	GtkWidget* dir_heading_event_box;
	GtkWidget* dir_heading_vbox;
	GtkWidget* dir_heading_separator;
	GtkWidget* dir_label;
	GtkWidget* file_num_label;

	GtkWidget* menu;
	GtkWidget* menu_item_image;
	GtkWidget* image_menu_item;

	gchar* temp1;
	gchar* temp2;
	gchar* temp3;
	gsize length;

	/* Create the struct. */
	new_dl = g_new(DListing, 1);
	new_dl->name = g_string_new(name->str);
	new_dl->rank = rank;
	new_dl->old_rank = -1;
	new_dl->marked = TRUE;

	new_dl->selected_count = g_string_new(selected_count->str);
	new_dl->total_count = g_string_new(total_count->str);
	new_dl->hidden_count = g_string_new(hidden_count->str);

	new_dl->file_box = NULL;

	/* This is the vbox for this whole listing. */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);

	/* Event box for the directory header (so it can be a different color). */
	dir_heading_event_box = gtk_event_box_new();
	gtk_widget_set_state(dir_heading_event_box, GTK_STATE_ACTIVE);
	gtk_box_pack_start(GTK_BOX(vbox), dir_heading_event_box, FALSE, FALSE, 0);
	gtk_widget_show(dir_heading_event_box);

	/* Directory header vbox. */
	dir_heading_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_set_homogeneous(GTK_BOX(dir_heading_vbox), FALSE);
	gtk_container_add(GTK_CONTAINER(dir_heading_event_box), dir_heading_vbox);
	gtk_widget_show(dir_heading_vbox);

	/* Create black-line separator. */
	dir_heading_separator = gtk_hseparator_new();
	gtk_widget_modify_bg(dir_heading_separator, GTK_STATE_NORMAL, v.separator_color);
	gtk_box_pack_start(GTK_BOX(dir_heading_vbox), dir_heading_separator, FALSE, FALSE, 0);
	gtk_widget_show(dir_heading_separator);

	/* Create directory label and align it to the left. */
	/* This is involved, because a couple conversions need to be done. */
	dir_label = gtk_label_new(NULL);
	temp1 = g_filename_to_utf8(new_dl->name->str, new_dl->name->len, NULL, &length, NULL);
	temp2 = g_markup_escape_text(temp1, length);
	temp3 = g_strconcat("<b><big><big>", temp2, "</big></big></b>", NULL);
	gtk_label_set_markup(GTK_LABEL(dir_label), temp3);
	g_free(temp1);
	g_free(temp2);
	g_free(temp3);
	gtk_misc_set_alignment(GTK_MISC(dir_label), 0, 0);
	gtk_box_pack_start(GTK_BOX(dir_heading_vbox), dir_label, FALSE, FALSE, 0);
	gtk_widget_show(dir_label);

	/* Create file selection number label and align it to the left. */
	file_num_label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(file_num_label), 0, 0);
	gtk_box_pack_start(GTK_BOX(dir_heading_vbox), file_num_label, FALSE, FALSE, 0);
	gtk_widget_show(file_num_label);
	new_dl->count_label = file_num_label;

	/* Create the file box. */
	new_dl->file_box = file_box_new();
	/* wrap_box_set_optimal_width(WRAP_BOX(new_dl->file_box), width - 4); */
	file_box_set_optimal_width(FILE_BOX(new_dl->file_box), width);
	file_box_set_show_hidden_files(FILE_BOX(new_dl->file_box), v.show_hidden_files);
	file_box_set_file_display_limit(FILE_BOX(new_dl->file_box), v.file_display_limit);
	wrap_box_set_hspacing(WRAP_BOX(new_dl->file_box), 5);
	wrap_box_set_line_justify(WRAP_BOX(new_dl->file_box), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox), new_dl->file_box, FALSE, FALSE, 0);
	gtk_widget_show(new_dl->file_box);

	/* Must do this after creating the filebox. */
	dlisting_reset_file_count_label(new_dl);

	/* Setup the context menu. */
	menu = gtk_menu_new();

	image_menu_item = gtk_image_menu_item_new_with_mnemonic("Show _hidden files");
	menu_item_image = gtk_image_new_from_pixbuf(v.show_hidden_pixbuf);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(image_menu_item), menu_item_image);
	gtk_widget_show(menu_item_image);
	gtk_widget_show(image_menu_item);
	if (v.show_hidden_files) {
		/* Hidden files are always shown -- this item is redundant. */
		gtk_widget_set_state(image_menu_item, GTK_STATE_INSENSITIVE);
	}
	else
		g_signal_connect(image_menu_item, "activate", G_CALLBACK(show_hidden_files_activate_handler), new_dl);
	gtk_menu_append(GTK_MENU_SHELL(menu), image_menu_item);

	image_menu_item = gtk_image_menu_item_new_with_mnemonic("Show _all files");
	menu_item_image = gtk_image_new_from_pixbuf(v.show_all_pixbuf);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(image_menu_item), menu_item_image);
	gtk_widget_show(menu_item_image);
	gtk_widget_show(image_menu_item);
	if (v.file_display_limit == 0) {
		/* There's no file display limit -- this item is redundant. */
		gtk_widget_set_state(image_menu_item, GTK_STATE_INSENSITIVE);
	}
	else
		g_signal_connect(image_menu_item, "activate", G_CALLBACK(show_all_files_activate_handler), new_dl);
	gtk_menu_append(GTK_MENU_SHELL(menu), image_menu_item);

	gtk_widget_show(menu);
	g_signal_connect_swapped(vbox, "button_press_event", G_CALLBACK(show_context_menu), menu);

	new_dl->widget = vbox;
	new_dl->name_label = dir_label;
	new_dl->menu = menu;

	return new_dl;
}


void dlisting_mark(DListing* d, gint new_rank) {
	d->marked = TRUE;
	d->old_rank = d->rank;
	d->rank = new_rank;
}


/* Compare and update (if necessary) the file counts for the given DListing based on the new data. */
void dlisting_update_file_counts(DListing* dl, const GString* selected_count, const GString* total_count, const GString* hidden_count) {

	gboolean change = FALSE;

	/* Compare selected count. */
	if (strcmp(dl->selected_count->str, selected_count->str) != 0) {
		g_string_assign(dl->selected_count, selected_count->str);
		change = TRUE;
	}

	/* Compare total file count. */
	if (strcmp(dl->total_count->str, total_count->str) != 0) {
		g_string_assign(dl->total_count, total_count->str);
		change = TRUE;
	}

	/* Compare hidden count. */
	if (strcmp(dl->hidden_count->str, hidden_count->str) != 0) {
		g_string_assign(dl->hidden_count, hidden_count->str);
		change = TRUE;
	}

	if (change)
		dlisting_reset_file_count_label(dl);
}


void dlisting_reset_file_count_label(DListing* dl) {
	gchar* temp_string;

	glong n_displayed;
	guint display_limit;

	display_limit = file_box_get_file_display_limit( FILE_BOX(dl->file_box) );

	if (file_box_get_show_hidden_files(FILE_BOX(dl->file_box)))
		n_displayed = atol(dl->total_count->str);
	else
		n_displayed = atol(dl->total_count->str) - atol(dl->hidden_count->str);

	if (display_limit == 0 || n_displayed <= display_limit) {
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


gboolean dlisting_is_new(const DListing* dl) {
	if (dl->old_rank > 0)
		return FALSE;
	else
		return TRUE;
}


/* Remove all memory associated with this DListing. */
void dlisting_free(DListing* dl) {

	gtk_widget_hide(dl->widget);

	file_box_destroy(FILE_BOX(dl->file_box));   /* This also gets the FItems associated with the file box. */
	gtk_widget_destroy(dl->widget);             /* Should take care of everything else. */


	gtk_widget_destroy(dl->menu);    /* Delete the menu.  This should get the menu items too. */

	g_string_free(dl->name, TRUE);
	g_string_free(dl->selected_count, TRUE);
	g_string_free(dl->total_count, TRUE);
	g_string_free(dl->hidden_count, TRUE);

	g_free(dl);
}


