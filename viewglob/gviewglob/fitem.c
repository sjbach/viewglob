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
#include "fitem.h"

#if DEBUG_ON
extern FILE* df;
#endif

/* For the icons and highlight colors. */
extern struct viewable_preferences v;


FItem* fitem_new(const GString* name, enum selection_state state, enum file_type type, gboolean build_widgets) {
	FItem* new_fitem;

	new_fitem = g_new(FItem, 1);
	new_fitem->name = g_string_new(name->str);
	new_fitem->type = type;
	new_fitem->selection = state;
	new_fitem->marked = TRUE;
	new_fitem->is_new = TRUE;
	new_fitem->widget = NULL;

	/* Create the widgets. */
	if (build_widgets)
		fitem_rebuild_widgets(new_fitem);

	return new_fitem;
}


void fitem_rebuild_widgets(FItem* fi) {

	GtkWidget* icon_image;
	GtkWidget* label;
	GtkWidget* hbox;
	GtkWidget* eventbox;

	gchar* temp1;
	gchar* temp2;
	gsize  length;

	/* Event Box (to show selection) */
	eventbox = gtk_event_box_new();
	gtk_widget_set_state(eventbox, fi->selection);

	/* HBox */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(eventbox), hbox);

	/* Icon */
	if (v.show_icons) {
		if (fi->type == T_FILE)
			icon_image = gtk_image_new_from_pixbuf(v.file_pixbuf);
		else
			icon_image = gtk_image_new_from_pixbuf(v.dir_pixbuf);
		gtk_widget_show(icon_image);
		gtk_box_pack_start(GTK_BOX(hbox), icon_image, FALSE, FALSE, 0);
	}

	/* Label -- must convert the text to utf8, and then escape it. */
	label = gtk_label_new(NULL);
	temp1 = g_filename_to_utf8(fi->name->str, fi->name->len, NULL, &length, NULL);
	temp2 = g_markup_escape_text(temp1, length);
	gtk_label_set_markup(GTK_LABEL(label), temp2);
	g_free(temp1);
	g_free(temp2);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	/* Remove the old widgets if present. */
	if (fi->widget) {
		gtk_widget_destroy(fi->widget);
		fi->is_new = TRUE;
	}

	fi->widget = eventbox;
}


/* Returns true if the update doesn't require external changes, false otherwise. */
gboolean fitem_update_type_and_state(FItem* fi, enum file_type t, enum selection_state s) {

	/* TODO if selected, always show; if becomes unselected, hide if hidden file. */
	if (fi->selection != s) {
		fi->selection = s;
		if (fi->widget)
			gtk_widget_set_state(fi->widget, fi->selection);
	}

	if (fi->type != t) {
		DEBUG((df, "%s type changed.\n", fi->name->str));
		fi->type = t;

		/* We'll have to make new widgets for this particular FItem... */
		/* (But only if it already has widgets). */
		if (fi->widget) {
			fitem_rebuild_widgets(fi);
			return FALSE;
		}
		else
			return TRUE;
	}
	else
		return TRUE;
}


/* Remove all memory associated with this FItem. */
void fitem_free(FItem* fi, gboolean destroy_widgets) {
	if (!fi)
		return;

	DEBUG((df, "destroying: %s (%d)\n", fi->name->str, destroy_widgets));

	if (fi->name)
		g_string_free(fi->name, TRUE);
	if (destroy_widgets && fi->widget)
		gtk_widget_destroy(fi->widget);   /* This will grab all the stuff inside, too. */

	g_free(fi);
	return;
}


void fitem_unmark_all(GSList* fi_slist) {
	FItem* fi;

	while (fi_slist) {
		fi = fi_slist->data;
		/*DEBUG((df, "unmarking %s\n", fi->name->str));*/
		fi->marked = FALSE;
		fi_slist = g_slist_next(fi_slist);
	}
}


void fitem_mark(FItem* fi) {
	/*DEBUG((df, "marking %s\n", fi->name->str));*/
	fi->marked = TRUE;
}

