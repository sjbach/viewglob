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
#include "wrap_box.h"
#include "dlisting.h"
#include <gtk/gtk.h>
#include <string.h>      /* For strcmp */

#if DEBUG_ON
extern FILE* df;
#endif

/* --- properties --- */
enum {
  PROP_0,
  PROP_OPTIMAL_WIDTH,
  PROP_SHOW_HIDDEN_FILES,
  PROP_FILE_DISPLAY_LIMIT,
};


/* --- prototypes --- */
static void  file_box_class_init(FileBoxClass* klass);
static void  file_box_init(FileBox* fbox);
/*static void  file_box_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec);*/
/*static void  file_box_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec);*/

static void   file_box_size_request(GtkWidget* widget, GtkRequisition* requisition);
static guint  file_box_get_display_pos(FileBox* fbox, FItem* fitem);
static void   file_box_allow_size_requests(FileBox* fbox, gboolean allow);

static FItem*    fitem_new(const gchar* name, FileType type, FileSelection selection);
static void      fitem_build_widgets(FItem* fi);
static void      fitem_free(FItem* fi, gboolean destroy_widgets);
static void      fitem_update_type_selection_and_order(FItem* fi, FileType t, FileSelection s, FileBox* fbox);
static gboolean  fitem_is_hidden(FItem* fi);
static void      fitem_determine_display_category(FItem* fi, FileBox* fbox);
static void      fitem_display(FItem* fi, FileBox* fbox);

static gboolean size_request_kludge(GtkWidget* widget, GtkRequisition* allocation, gpointer user_data);
static gboolean fitem_button_press_event(GtkWidget* widget, GdkEventButton* event, FItem* fi);

static gint cmp_same_name(gconstpointer a, gconstpointer b);
static gint cmp_ordering_ls(gconstpointer a, gconstpointer b);
static gint cmp_ordering_win(gconstpointer a, gconstpointer b);


/* --- variables --- */
static gpointer parent_class = NULL;
static GdkPixbuf* file_type_icons[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static GCompareFunc ordering_func = cmp_ordering_ls;
static GIOChannel* out_channel = NULL;


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
		file_box_type = g_type_register_static (WRAP_BOX_TYPE, "FileBox", &file_box_info, 0);
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

	g_object_class_install_property (object_class,
		PROP_SHOW_HIDDEN_FILES,
		g_param_spec_boolean("show_hidden_files",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
		PROP_FILE_DISPLAY_LIMIT,
		g_param_spec_uint ("file_display_limit",
			NULL,
			NULL,
			0,
			G_MAXINT,
			300,
			G_PARAM_READWRITE));
			*/
}


/* This function prevents the size-request signal from propagating if eat_size_requests == TRUE.
   It would be better to make FItem a new class which reimplements gtk_widget_show and prevents the
   size request from happening in the first place. */
static gboolean size_request_kludge(GtkWidget* widget, GtkRequisition* requisition, gpointer user_data) {
	FileBox* fbox = FILE_BOX(widget);
	return fbox->eat_size_requests;
}


static void file_box_allow_size_requests(FileBox* fbox, gboolean allow) {

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
	fbox->show_hidden_files = FALSE;
	fbox->n_files = 0;
	fbox->file_max = 32767;
	fbox->file_display_limit = DEFAULT_FILE_DISPLAY_LIMIT;
	fbox->fi_slist = NULL;
	fbox->eat_size_requests = FALSE;

	g_signal_connect(fbox, "size-request", G_CALLBACK(size_request_kludge), NULL);
}


GtkWidget* file_box_new (void) {
	return g_object_new (FILE_BOX_TYPE, NULL);
}


void file_box_destroy(FileBox* fbox) {

	g_slist_foreach(fbox->fi_slist, (GFunc) fitem_free, (gpointer) TRUE);
	g_slist_free(fbox->fi_slist);
	gtk_widget_destroy(GTK_WIDGET(fbox));
}



/*
static void file_box_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
	FileBox *fbox = FILE_BOX (object);
	
	switch (property_id) {
		case PROP_OPTIMAL_WIDTH:
			file_box_set_optimal_width(fbox, g_value_get_uint(value));
			break;
		case PROP_SHOW_HIDDEN_FILES:
			file_box_set_show_hidden_files(fbox, g_value_get_boolean(value));
			break;
		case PROP_FILE_DISPLAY_LIMIT:
			file_box_set_file_display_limit(fbox, g_value_get_uint(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}
*/



/*
static void file_box_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
	FileBox *fbox = FILE_BOX (object);
	GtkWidget *widget = GTK_WIDGET (object);

	switch (property_id) {
		case PROP_OPTIMAL_WIDTH:
			g_value_set_uint(value, fbox->optimal_width);
			break;
		case PROP_SHOW_HIDDEN_FILES:
			g_value_set_boolean(value, fbox->show_hidden_files);
			break;
		case PROP_FILE_DISPLAY_LIMIT:
			g_value_set_uint(value, fbox->file_display_limit);
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



void file_box_set_show_hidden_files(FileBox* fbox, gboolean show) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	if (fbox->show_hidden_files == show)
		return;

	GSList* fi_iter;
	FItem* fi;
	FileDisplayCategory fdc;

	fbox->show_hidden_files = show;
	if (show)
		fdc = FDC_REVEAL;
	else
		fdc = FDC_MASK;

	file_box_allow_size_requests(fbox, FALSE);

	/* Cycle through the hidden files and reveal or mask them. */
	for (fi_iter = fbox->fi_slist; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		if (fitem_is_hidden(fi)) {
			fi->disp_cat = fdc;
			fitem_display(fi, fbox);
		}
	}

	file_box_flush(fbox);
}



void file_box_set_file_display_limit(FileBox* fbox, guint limit) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	if (fbox->file_display_limit == limit)
		return;

	GSList* fi_iter;
	FItem* fi;

	fbox->file_display_limit = limit;

	file_box_allow_size_requests(fbox, FALSE);

	/* Cycle through REVEAL fitems and re-display them up to the new limit.
	   (fitems over that limit will be truncated). */
	for (fi_iter = fbox->fi_slist; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		if (fi->disp_cat == FDC_REVEAL)
			fitem_display(fi, fbox);
	}

	file_box_flush(fbox);
}



void file_box_set_ordering(FileBoxOrdering fbo) {
	if (fbo == FBO_LS)
		ordering_func = cmp_ordering_ls;
	else if (fbo == FBO_WIN)
		ordering_func = cmp_ordering_win;
}


guint file_box_get_optimal_width(FileBox* fbox) {
	g_return_val_if_fail(IS_FILE_BOX(fbox), 0);
	return fbox->optimal_width;
}



void file_box_set_icon(FileType type, GdkPixbuf* icon) {
	file_type_icons[type] = icon;
}


void file_box_set_out_channel(GIOChannel* channel) {
	g_return_if_fail(channel != NULL);
	out_channel = channel;
}


gboolean file_box_get_show_hidden_files(FileBox* fbox) {
	g_return_val_if_fail(IS_FILE_BOX(fbox), FALSE);
	return fbox->show_hidden_files;
}


guint file_box_get_file_display_limit(FileBox* fbox) {
	g_return_val_if_fail(IS_FILE_BOX(fbox), 0);
	return fbox->file_display_limit;
}



void file_box_add(FileBox* fbox, GString* name, FileType type, FileSelection selection) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* search_result;
	FItem* fi;

	/* Check if we've already got this FItem. */
	search_result = g_slist_find_custom(fbox->fi_slist, name->str, cmp_same_name);
	if (search_result) {
		fi = search_result->data;
		fitem_update_type_selection_and_order(fi, type, selection, fbox);
	}
	else {
		fi = fitem_new(name->str, type, selection);
		fbox->fi_slist = g_slist_insert_sorted(fbox->fi_slist, fi, ordering_func);
	}

	fi->marked = TRUE;
	fitem_determine_display_category(fi, fbox);
	fitem_display(fi, fbox);
}



/* Display (or not) the given FItem.
   This involves creating and destroying widgets. */
static void fitem_display(FItem* fi, FileBox* fbox) {
	switch (fi->disp_cat) {

		case FDC_REVEAL:
			/* We display a REVEAL'd FItem if we're under the limit or if it's selected. */
			if ( (fbox->file_display_limit == 0) || (fbox->n_displayed_files < fbox->file_display_limit) ) {
				if ( ! fi->widget ) {
					/* Build widgets and pack it in. */
					fitem_build_widgets(fi);
					wrap_box_pack_pos(WRAP_BOX(fbox), fi->widget, file_box_get_display_pos(fbox, fi), FALSE);

				}
				fbox->n_displayed_files++;
			}
			else if (fi->selection == FS_YES) {
				if ( ! fi->widget ) {
					/* It's not under the limit, but it's been selected.
					   Doesn't have any widgets (if it does it's already displayed), so we make them. */
					fitem_build_widgets(fi);
					wrap_box_pack_pos(WRAP_BOX(fbox), fi->widget, file_box_get_display_pos(fbox, fi), FALSE);
				}
			}
			else if (fi->widget) {
				/* Remove the widgets since this is over the limit and not selected. */
				gtk_widget_destroy(fi->widget);
				fi->widget = NULL;
			}
			break;

		case FDC_MASK:
			if (fi->selection == FS_YES) {
				/* Selected -- we display it even though it's masked. */
				if (! fi->widget )  {
					/* We're peeking at this FItem, but it doesn't have any widgets yet. */
					fitem_build_widgets(fi);
					wrap_box_pack_pos(WRAP_BOX(fbox), fi->widget, file_box_get_display_pos(fbox, fi), FALSE);
				}

			}
			else if (fi->widget) {
				/* Not selected.  Destroy the FItem's widgets if present. */
				gtk_widget_destroy(fi->widget);
				fi->widget = NULL;
			}
			break;

		default:
			g_error("Unexpected display category.");
			break;
	}
}



/* Get the position of this fitem in the box. */
static guint file_box_get_display_pos(FileBox* fbox, FItem* fitem) {
	GSList* fi_iter;
	FItem* fi;
	guint pos = 0;

	for (fi_iter = fbox->fi_slist; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		if (fi == fitem)
			break;
		else if ( (fi->disp_cat == FDC_REVEAL || fi->selection == FS_YES) && fi->widget )
			pos++;
	}

	return pos;
}



/* Unmark all the FItems.  This is called just before reading in a new bunch of data. */
void file_box_begin_read(FileBox* fbox) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* fi_iter;
	FItem* fi;

	for (fi_iter = fbox->fi_slist; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		fi->marked = FALSE;
	}

	fbox->n_displayed_files = 0;   /* This isn't really true, but n_displayed_files needs to be
									  redefined during processing. */

	/* There will be no size requests of the file box until file_box_flush(). */
	file_box_allow_size_requests(fbox, FALSE);
}



/* Delete all fitems which are not marked. */
void file_box_flush(FileBox* fbox) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* fi_iter;
	GSList* tmp;
	FItem* fi;

	fi_iter = fbox->fi_slist;
	while (fi_iter) {
		fi = fi_iter->data;
		if ( ! fi->marked ) {
			/* Not marked -- no holds barred. */
			if (fi->disp_cat == FDC_REVEAL && fi->widget)
				fbox->n_displayed_files--;
			tmp = fi_iter;
			fi_iter = g_slist_next(fi_iter);
			fbox->fi_slist = g_slist_delete_link(fbox->fi_slist, tmp);
			fitem_free(fi, TRUE);
			continue;
		}
		else if (fi->widget)
			gtk_widget_show(fi->widget);

		fi_iter = g_slist_next(fi_iter);
	}

	/* Now we do a size request. */
	file_box_allow_size_requests(fbox, TRUE);
}



static void file_box_size_request(GtkWidget* widget, GtkRequisition* requisition) {
	FileBox* this = FILE_BOX(widget);
	if (!this->eat_size_requests) {
		//g_printerr(".");
		wrap_box_size_request_optimal(widget, requisition, this->optimal_width);
	}
}



static FItem* fitem_new(const gchar* name, FileType type, FileSelection selection) {
	FItem* new_fitem;

	new_fitem = g_new(FItem, 1);
	new_fitem->name = g_strdup(name);
	new_fitem->type = type;
	new_fitem->selection = selection;
	new_fitem->disp_cat = FDC_INDETERMINATE;
	new_fitem->widget = NULL;

	return new_fitem;
}



static void fitem_build_widgets(FItem* fi) {

	GtkWidget* label;
	GtkWidget* hbox;
	GtkWidget* eventbox;

	gchar* temp;

	/* Event Box (to show selection) */
	eventbox = gtk_event_box_new();
	gtk_widget_set_state(eventbox, fi->selection);

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
	temp = g_filename_to_utf8(fi->name, strlen(fi->name), NULL, NULL, NULL);
	label = gtk_label_new(temp);
	g_free(temp);
	gtk_misc_set_padding(GTK_MISC(label), 1, 0);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	/* Connect to button click on the eventbox. */
	g_signal_connect(eventbox, "button-press-event", G_CALLBACK(fitem_button_press_event), fi);

	fi->widget = eventbox;
	/*gtk_widget_show(eventbox);*/
}


static gboolean fitem_button_press_event(GtkWidget* widget, GdkEventButton* event, FItem* fi) {

	GError* error = NULL;
	gsize bytes_written;
	gboolean in_loop = TRUE;
	GString* data;

	/* Write out the FItem's name opon a double click. */
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		if (out_channel) {

			data = g_string_new("file:");

			/* If shift is held, write out the full path. */
			if ( (event->state & GDK_SHIFT_MASK) && fi->widget->parent && fi->widget->parent->parent) {
				data = g_string_append(data, DLISTING(fi->widget->parent->parent)->name->str);

				/* If the parent dir is / (root), it already has a '/' on the end. */
				if ( *(data->str + strlen(data->str) - 1) != '/')
					data = g_string_append(data, "/");
			}

			data = g_string_append(data, fi->name);

			/* Write out the file name. */
			while (in_loop) {
				switch (g_io_channel_write_chars(out_channel, data->str, strlen(data->str) + 1,  &bytes_written, &error)) {
					case (G_IO_STATUS_ERROR):
						g_printerr("gviewglob: %s\n", error->message);
						in_loop = FALSE;
						break;

					case (G_IO_STATUS_NORMAL):
						in_loop = FALSE;
						break;

					case (G_IO_STATUS_AGAIN):
						break;

					default:
						g_warning("Unexpected result from g_io_channel_write_chars.");
						in_loop = FALSE;
						break;
				}
			}

			g_string_free(data, TRUE);
		}

	}

	return TRUE;
}


/* Remove all memory associated with this FItem. */
static void fitem_free(FItem* fi, gboolean destroy_widgets) {
	if (!fi)
		return;

	/*DEBUG((df, "destroying: %s (%d)\n", fi->name, destroy_widgets));*/

	g_free(fi->name);
	if (destroy_widgets && fi->widget)
		gtk_widget_destroy(fi->widget);   /* This will grab all the stuff inside, too. */

	g_free(fi);
}



/* Determine the FileDisplayCategory of this fitem. */
static void fitem_determine_display_category(FItem* fi, FileBox* fbox) {
	if (fitem_is_hidden(fi)) {
		if (fbox->show_hidden_files)
			fi->disp_cat = FDC_REVEAL;
		else
			fi->disp_cat = FDC_MASK;
	}
	else
		fi->disp_cat = FDC_REVEAL;
}



static void fitem_update_type_selection_and_order(FItem* fi, FileType t, FileSelection s, FileBox* fbox) {

	/* File type. */
	if (fi->type != t) {
		DEBUG((df, "%s type changed.\n", fi->name));
		fi->type = t;

		/* Remove the widgets for this fitem (if it has any).
		   They'll be remade later if need be. */
		if (fi->widget) {
			gtk_widget_destroy(fi->widget);
			fi->widget = NULL;
		}

		/* Reposition this FItem. */
		fbox->fi_slist = g_slist_remove(fbox->fi_slist, fi);
		fbox->fi_slist = g_slist_insert_sorted(fbox->fi_slist, fi, ordering_func);
	}

	/* File selection state. */
	if (fi->selection != s) {
		fi->selection = s;
		if (fi->widget)
			gtk_widget_set_state(fi->widget, fi->selection);
	}
}



static gboolean fitem_is_hidden(FItem* fi) {
	return *(fi->name) == '.';
}



static gint cmp_same_name(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const gchar* bb = b;

	return strcmp(aa->name, bb);
}



/* Sort by type (dir first), then by name (default Windows style). */
static gint cmp_ordering_win(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const FItem* bb = b;

	if (aa->type == FT_DIRECTORY) {
		if (bb->type == FT_DIRECTORY)
			return strcmp( aa->name, bb->name);
		else
			return -1;
	}
	else {
		if (bb->type == FT_DIRECTORY)
			return 1;
		else
			return strcmp( aa->name, bb->name);
	}
}



/* Sort strictly by name (default ls style). */
static gint cmp_ordering_ls(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const FItem* bb = b;

	return strcmp( aa->name, bb->name);
}

