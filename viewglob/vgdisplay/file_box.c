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
#include "param-io.h"
#include "dlisting.h"
#include "param-io.h"
#include "file_box.h"
#include "wrap_box.h"
#include "lscolors.h"
#include <gtk/gtk.h>
#include <string.h>      /* For strcmp */

#define BASE_FONT_SIZE 0

/* --- properties --- */
enum {
  PROP_0,
  PROP_OPTIMAL_WIDTH,
};


/* --- prototypes --- */
static void  file_box_class_init(FileBoxClass* klass);
static void  file_box_init(FileBox* fbox);
/*static void  file_box_set_property(GObject* object, guint property_id,
  const GValue* value, GParamSpec* pspec);*/
/*static void  file_box_get_property(GObject* object, guint property_id,
  GValue* value, GParamSpec* pspec);*/

static void   file_box_size_request(GtkWidget* widget,
		GtkRequisition* requisition);
static guint  file_box_get_display_pos(FileBox* fbox, FItem* fitem);
static void   allow_size_requests(FileBox* fbox, gboolean allow);

static FItem*    fitem_new(const gchar* name, FileType type,
		FileSelection selection);
static void      fitem_build_widgets(FItem* fi);
static void      fitem_free(FItem* fi, gboolean destroy_widgets);
static int       fitem_update_type_selection_and_order(FItem* fi, FileType t,
		FileSelection s, FileBox* fbox, gint rank);

static gboolean size_request_kludge(GtkWidget* widget,
		GtkRequisition* allocation, gpointer user_data);
static gboolean fitem_button_press_event(GtkWidget* widget,
		GdkEventButton* event, FItem* fi);

static GtkStateType selection_to_state(FileSelection s);
static gint cmp_same_name(gconstpointer a, gconstpointer b);

static void initialize_icons(gchar* test_string);
static GdkPixbuf*  make_pixbuf_scaled(const guint8 icon_inline[],
		gint scale_height);


/* --- variables --- */
static gpointer parent_class = NULL;

static GdkPixbuf* file_type_icons[FT_COUNT] =
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/* --- functions --- */
GType file_box_get_type(void) {
	static GType file_box_type = 0;

	if (!file_box_type) {
		static const GTypeInfo file_box_info = {
			sizeof (FileBoxClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) file_box_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (FileBox),
			0,		/* n_preallocs */
			(GInstanceInitFunc) file_box_init,
		};
		file_box_type = g_type_register_static (WRAP_BOX_TYPE, "FileBox",
				&file_box_info, 0);
	}

	return file_box_type;
}



static void file_box_class_init(FileBoxClass* class) {
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS(class);
	widget_class = GTK_WIDGET_CLASS(class);

	parent_class = g_type_class_peek_parent(class);
	widget_class->size_request = file_box_size_request;

	/*
	g_object_class_install_property (object_class,
		PROP_OPTIMAL_WIDTH,
		g_param_spec_uint ("optimal_width",
			NULL,
			NULL,
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));
	*/
}


/* This function prevents the size-request signal from propagating if
   eat_size_requests == TRUE.  It would be better to make FItem a new class
   which reimplements gtk_widget_show and prevents the size request from
   happening in the first place. */
static gboolean size_request_kludge(GtkWidget* widget,
		GtkRequisition* requisition, gpointer user_data) {
	FileBox* fbox = FILE_BOX(widget);
	return fbox->eat_size_requests;
}


static void allow_size_requests(FileBox* fbox, gboolean allow) {

	if (fbox->eat_size_requests != allow)
		return;

	if (allow) {
		/* Now we do a size request. */
		fbox->eat_size_requests = FALSE;
		gtk_widget_queue_resize(GTK_WIDGET(fbox));
	}
	else
		fbox->eat_size_requests = TRUE;
}


static void file_box_init(FileBox *fbox) {
	GTK_WIDGET_SET_FLAGS (fbox, GTK_NO_WINDOW);

	fbox->optimal_width = 0;
	fbox->fis = NULL;
	fbox->eat_size_requests = FALSE;
	fbox->changed_fi = NULL;

	g_signal_connect(fbox, "size-request", G_CALLBACK(size_request_kludge),
			NULL);
}


GtkWidget* file_box_new (void) {
	return g_object_new (FILE_BOX_TYPE, NULL);
}


void file_box_destroy(FileBox* fbox) {

	g_slist_foreach(fbox->fis, (GFunc) fitem_free, (gpointer) TRUE);
	g_slist_free(fbox->fis);
	gtk_widget_destroy(GTK_WIDGET(fbox));
}


/*
static void file_box_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec) {
	FileBox *fbox = FILE_BOX (object);
	
	switch (property_id) {
		case PROP_OPTIMAL_WIDTH:
			file_box_set_optimal_width(fbox, g_value_get_uint(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}
*/



/*
static void file_box_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec) {
	FileBox *fbox = FILE_BOX (object);
	GtkWidget *widget = GTK_WIDGET (object);

	switch (property_id) {
		case PROP_OPTIMAL_WIDTH:
			g_value_set_uint(value, fbox->optimal_width);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}
*/


void file_box_set_optimal_width(FileBox* fbox, guint optimal_width) {
	g_return_if_fail(IS_FILE_BOX(fbox));
	g_return_if_fail(optimal_width >= GTK_CONTAINER(fbox)->border_width * 2);

	if (fbox->optimal_width != optimal_width) {
		/*g_printerr("<new optimal_width: %d>", optimal_width);*/
		fbox->optimal_width = optimal_width;
		gtk_widget_queue_resize(GTK_WIDGET(fbox));
		/* FIXME */
	}
}


guint file_box_get_optimal_width(FileBox* fbox) {
	g_return_val_if_fail(IS_FILE_BOX(fbox), 0);
	return fbox->optimal_width;
}


void file_box_set_icon(FileType type, GdkPixbuf* icon) {
	file_type_icons[type] = icon;
}


/* Set the size of the FItem font (and the size of the file type icons). */
void file_box_set_sizing(gint modifier) {

	g_return_if_fail(modifier <= 10);   /* Constrain between -10 and 10. */
	g_return_if_fail(modifier >= -10);

	gint i;
	GString* test_string = g_string_new(
			"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123456789");

	gint font_size_modifier = BASE_FONT_SIZE + modifier;

	/* Create a test string. */
	if (font_size_modifier > 0) {
		for (i = 0; i < font_size_modifier; i++) {
			g_string_prepend(test_string, "<big>");
			g_string_append(test_string, "</big>");
		}
	}
	else if (font_size_modifier < 0) {
		for (i = font_size_modifier; i < 0; i++) {
			g_string_prepend(test_string, "<small>");
			g_string_append(test_string, "</small>");
		}
	}

	/* (Re)set the icons with the new sizing. */
	initialize_icons(test_string->str);

	/* Parse LS_COLORS. */
	parse_ls_colors(font_size_modifier);

	g_string_free(test_string, TRUE);
}


static void initialize_icons(gchar* test_string) {
#include "file_icons.h"

	GtkIconTheme* current_theme;
	GtkIconInfo* icon_info;

	guint icon_size;
	GtkWidget* test_label;
	PangoLayout* test_layout;

	int i;

	/* Free the old icons, if present. */
	for (i = 0; i < FT_COUNT; i++) {
		if (file_type_icons[i]) {
			g_object_unref(file_type_icons[i]);
			file_type_icons[i] = NULL;
		}
	}

	/* Figure out a good size for the file type icons. */
	test_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(test_label), test_string);
	test_layout = gtk_label_get_layout(GTK_LABEL(test_label));
	pango_layout_get_pixel_size(test_layout, NULL, &icon_size);
	gtk_widget_destroy(test_label);

	/* Try to get icons from the current theme. */
	current_theme = gtk_icon_theme_get_default();

	/* Regular file icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-regular",
			icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_REGULAR,
				gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_REGULAR,
				make_pixbuf_scaled(regular_inline,icon_size));
	}

	/* Executable icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme,
			"gnome-fs-executable", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_EXECUTABLE,
				gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_EXECUTABLE,
				make_pixbuf_scaled(executable_inline, icon_size));
	}

	/* Directory icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme,
			"gnome-fs-directory", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_DIRECTORY,
				gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_DIRECTORY,
				make_pixbuf_scaled(directory_inline, icon_size));
	}

	/* Block device icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-blockdev",
			icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_BLOCKDEV,
				gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_BLOCKDEV,
				make_pixbuf_scaled(blockdev_inline, icon_size));
	}

	/* Character device icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-chardev",
			icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_CHARDEV,
				gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_CHARDEV,
				make_pixbuf_scaled(chardev_inline, icon_size));
	}

	/* Fifo icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-fifo",
			icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_FIFO, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_FIFO, make_pixbuf_scaled(fifo_inline, icon_size));
	}

	/* Symlink icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-dev-symlink",
			icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_SYMLINK,
				gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_SYMLINK,
				make_pixbuf_scaled(symlink_inline, icon_size));
	}

	/* Socket icon */
	icon_info = gtk_icon_theme_lookup_icon(current_theme, "gnome-fs-socket",
			icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (icon_info) {
		/* Use whatever the user has. */
		file_box_set_icon(FT_SOCKET, gtk_icon_info_load_icon(icon_info, NULL));
		gtk_icon_info_free(icon_info);
	}
	else {
		/* Use this icon from old Gnome. */
		file_box_set_icon(FT_SOCKET,
				make_pixbuf_scaled(socket_inline, icon_size));
	}

}


static GdkPixbuf* make_pixbuf_scaled(const guint8 icon_inline[],
		gint scale_size) {
	GdkPixbuf* temp;
	GdkPixbuf* result;

	temp = gdk_pixbuf_new_from_inline(-1, icon_inline, FALSE, NULL);
	result = gdk_pixbuf_scale_simple(temp, scale_size, scale_size,
			GDK_INTERP_BILINEAR);

	g_object_unref(temp);
	return result;
}


int file_box_add(FileBox* fbox, gchar* name, FileType type,
		FileSelection selection, gint rank) {
	g_return_val_if_fail(IS_FILE_BOX(fbox), 0);

	GSList* search_result;
	FItem* fi;
	gint points;

	/* Check if we've already got this FItem. */
	search_result = g_slist_find_custom(fbox->fis, name, cmp_same_name);
	if (search_result) {
		fi = search_result->data;
		points = fitem_update_type_selection_and_order(
				fi, type, selection, fbox, rank);
	}
	else {
		fi = fitem_new(name, type, selection);
		fbox->fis = g_slist_insert(fbox->fis, fi, rank);
		points = 2;
	}

	fi->marked = TRUE;

	if (!fi->widget) {
		/* Build widgets and pack it in. */
		fitem_build_widgets(fi);
		wrap_box_pack_pos(WRAP_BOX(fbox), fi->widget,
				file_box_get_display_pos(fbox, fi), FALSE);

		if (!fbox->changed_fi)
			fbox->changed_fi = fi;
	}

	return points;
}


/* Get the position of this fitem in the box. */
static guint file_box_get_display_pos(FileBox* fbox, FItem* fitem) {
	GSList* fi_iter;
	FItem* fi;
	guint pos = 0;

	for (fi_iter = fbox->fis; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		if (fi == fitem)
			break;
		else
			pos++;
	}

	return pos;
}


/* Unmark all the FItems.  This is called just before reading in a new bunch
   of data. */
void file_box_begin_read(FileBox* fbox) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* fi_iter;
	FItem* fi;

	for (fi_iter = fbox->fis; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		fi->marked = FALSE;
	}

	fbox->changed_fi = NULL;

	/* There will be no size requests of the file box until
	   file_box_flush(). */
	allow_size_requests(fbox, FALSE);
}


/* Delete all fitems which are not marked. */
void file_box_flush(FileBox* fbox) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* fi_iter;
	GSList* tmp;
	FItem* fi;

	fi_iter = fbox->fis;
	while (fi_iter) {
		fi = fi_iter->data;
		if (!fi->marked ) {
			/* Not marked -- no holds barred. */
			tmp = fi_iter;
			fi_iter = g_slist_next(fi_iter);
			fbox->fis = g_slist_delete_link(fbox->fis, tmp);
			fitem_free(fi, TRUE);
			continue;
		}
		else if (fi->widget)
			gtk_widget_show(fi->widget);

		fi_iter = g_slist_next(fi_iter);
	}

	/* Now we do a size request. */
	allow_size_requests(fbox, TRUE);
}


static void file_box_size_request(GtkWidget* widget,
		GtkRequisition* requisition) {
	FileBox* this = FILE_BOX(widget);
	if (!this->eat_size_requests) {
		wrap_box_size_request_optimal(widget, requisition,
				this->optimal_width);
	}
}


static FItem* fitem_new(const gchar* name, FileType type,
		FileSelection selection) {
	FItem* new_fitem;

	new_fitem = g_new(FItem, 1);
	new_fitem->name = g_strdup(name);
	new_fitem->type = type;
	new_fitem->selection = selection;
	new_fitem->widget = NULL;

	return new_fitem;
}


static GtkStateType selection_to_state(FileSelection s) {
	switch (s) {
		case FS_YES:
			return GTK_STATE_SELECTED;
			break;
		case FS_MAYBE:
			return GTK_STATE_ACTIVE;
			break;
		case FS_NO:
		default:
			return GTK_STATE_NORMAL;
			break;
	}
}


static void fitem_build_widgets(FItem* fi) {

	GtkWidget* label;
	GtkWidget* hbox;
	GtkWidget* eventbox;

	gchar* label_text;
	gsize  length;

	/* Event Box (to show selection) */
	eventbox = gtk_event_box_new();
	gtk_widget_set_state(eventbox, selection_to_state(fi->selection));

	/* HBox */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(eventbox), hbox);

	/* Icon */
	if (file_type_icons[fi->type]) {
		GtkWidget* icon_image;
		icon_image = gtk_image_new_from_pixbuf(file_type_icons[fi->type]);
		gtk_misc_set_padding(GTK_MISC(icon_image), 1, 0);
		gtk_widget_show(icon_image);
		gtk_box_pack_start(GTK_BOX(hbox), icon_image, FALSE, FALSE, 0);
	}

	/* Label -- must convert the text to utf8. */
	label_text = g_filename_to_utf8(fi->name, strlen(fi->name), NULL,
			&length, NULL);
	label = gtk_label_new(label_text);
	g_free(label_text);

	/* LS_COLORS. */
	label_set_attributes(fi->name, fi->type, GTK_LABEL(label));

	gtk_misc_set_padding(GTK_MISC(label), 1, 0);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	/* Connect to button click on the eventbox. */
	g_signal_connect(eventbox, "button-press-event",
			G_CALLBACK(fitem_button_press_event), fi);

	fi->widget = eventbox;
	/*gtk_widget_show(eventbox);*/
}


static gboolean fitem_button_press_event(GtkWidget* widget,
		GdkEventButton* event, FItem* fi) {

	g_return_val_if_fail(event != NULL, FALSE);
	g_return_val_if_fail(fi != NULL, FALSE);

	/* Write out the FItem's name upon a double click. */
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {

			GString* string = g_string_new(NULL);

			string = g_string_append(string, fi->name);

			/* Trailing '/' on directories. */
			if (fi->type == FT_DIRECTORY)
				string = g_string_append(string, "/");

			/* Write out the file name. */
			if (!put_param(STDOUT_FILENO, P_FILE, string->str))
				g_warning("Could not write filename to stdout");

			g_string_free(string, TRUE);
	}

	return FALSE;
}


/* Remove all memory associated with this FItem. */
static void fitem_free(FItem* fi, gboolean destroy_widgets) {
	if (!fi)
		return;

	g_free(fi->name);
	if (destroy_widgets && fi->widget) {
		/* This will grab all the stuff inside, too. */
		gtk_widget_destroy(fi->widget);
	}
	g_free(fi);
}


static int fitem_update_type_selection_and_order(FItem* fi, FileType t,
		FileSelection s, FileBox* fbox, gint rank) {

	gint points = 0;

	/* File type. */
	if (fi->type != t) {
		fi->type = t;

		/* Remove the widgets for this fitem (if it has any).
		   They'll be remade later if need be. */
		if (fi->widget) {
			gtk_widget_destroy(fi->widget);
			fi->widget = NULL;
		}

		/* Reposition this FItem. */
		fbox->fis = g_slist_remove(fbox->fis, fi);
		fbox->fis = g_slist_insert(fbox->fis, fi, rank);

		points = 2;
	}

	/* File selection state. */
	if (fi->selection != s) {
		fi->selection = s;
		if (fi->widget) {
			gtk_widget_set_state(fi->widget,
					selection_to_state(fi->selection));
		}

		points++;
	}

	if (points && !fbox->changed_fi)
		fbox->changed_fi = fi;

	return points;
}


static gint cmp_same_name(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const gchar* bb = b;

	return strcmp(aa->name, bb);
}

