
#include "common.h"
#include "file_box.h"
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
static void  file_box_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec);
static void  file_box_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec);

static void  file_box_size_request(GtkWidget* widget, GtkRequisition* requisition);
static void  file_box_size_allocate(GtkWidget* widget, GtkAllocation* allocation);

static gint  file_box_get_display_pos(FileBox* fbox, FItem* fitem);

static FItem* fitem_new(const GString* name, FileType type, FileSelection selection, gboolean build_widgets);
static void fitem_rebuild_widgets(FItem* fi);
static void fitem_free(FItem* fi, gboolean destroy_widgets);
static gboolean fitem_update_type_and_selection(FItem* fi, FileType t, FileSelection s);

static gint cmp_same_name(gconstpointer a, gconstpointer b);
static gint cmp_ordering_ls(gconstpointer a, gconstpointer b);
static gint cmp_ordering_win(gconstpointer a, gconstpointer b);


/* --- variables --- */
static gpointer parent_class = NULL;
static GdkPixbuf* file_type_icons[2] = { NULL, NULL };
static GCompareFunc ordering_func = cmp_ordering_ls;


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
		//file_box_type = g_type_register_static (GTK_TYPE_CONTAINER, "FileBox", &file_box_info, 0);
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


static void file_box_init(FileBox *fbox) {
	GTK_WIDGET_SET_FLAGS (fbox, GTK_NO_WINDOW);

	fbox->optimal_width = 0;
	fbox->show_hidden_files = FALSE;
	fbox->n_files = 0;
	fbox->file_max = 32767;
	fbox->file_display_limit = 300;
	//fbox->sort_func = cmp_ordering_ls;
	fbox->fitems = NULL;
}


GtkWidget* file_box_new (void) {
	return g_object_new (FILE_BOX_TYPE, NULL);
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

	optimal_width -= GTK_CONTAINER(fbox)->border_width * 2;

	if (fbox->optimal_width != optimal_width) {
		g_printerr("<new optimal_width: %d>\n", optimal_width);
		fbox->optimal_width = optimal_width;
		gtk_widget_queue_resize(GTK_WIDGET(fbox));
		/* FIXME */
	}
}


void file_box_set_show_hidden_files(FileBox* fbox, gboolean show) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	if (fbox->show_hidden_files != show) {
		g_printerr("<new show_hidden_files>\n");
		fbox->show_hidden_files = show;
		/* FIXME */
	}
}


void file_box_set_file_display_limit(FileBox* fbox, guint limit) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	if (fbox->file_display_limit != limit) {
		g_printerr("<new display_limit: %d>\n", limit);
		fbox->file_display_limit = limit;
		/* FIXME */
	}
}


void file_box_set_ordering(FileBoxOrdering fbo) {
	if (fbo == FBO_LS)
		ordering_func = cmp_ordering_ls;
	else if (fbo == FBO_WIN)
		ordering_func = cmp_ordering_win;
}


void file_box_set_icon(FileType type, GdkPixbuf* icon) {
	file_type_icons[type] = icon;
}


void file_box_add(FileBox* fbox, GString* name, FileType type, FileSelection selection) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* search_result;
	FItem* fi;
	
	DEBUG((df, "."));

	/* Locate the fitem. */
	search_result = g_slist_find_custom(fbox->fitems, name, cmp_same_name);

	/* TODO: if found, compare type and selection */
	if (search_result) {
		fi = search_result->data;

		if (fitem_update_type_and_selection(fi, type, selection)) {
			/* Reposition this FItem, which has changed type. */
			fbox->fitems = g_slist_remove(fbox->fitems, fi);
			fbox->fitems = g_slist_insert_sorted(fbox->fitems, fi, ordering_func);   /* TODO sort_func should be global */
			wrap_box_pack(WRAP_BOX(fbox), fi->widget);
			wrap_box_reorder_child(WRAP_BOX(fbox), fi->widget, file_box_get_display_pos(fbox, fi));
		}

		fi->marked = TRUE;
	}
	else {
		/* TODO: Must check to see if starts with ".", etc. */
		if (fbox->n_displayed_files < fbox->file_display_limit) {
			/* Put it in the correct place in the box. */
			fi = fitem_new(name, type, selection, TRUE);
			fbox->fitems = g_slist_insert_sorted(fbox->fitems, fi, ordering_func);
			wrap_box_pack(WRAP_BOX(fbox), fi->widget);
			wrap_box_reorder_child(WRAP_BOX(fbox), fi->widget, file_box_get_display_pos(fbox, fi));
			fi->displayed = TRUE;
			fbox->n_displayed_files++;
		}
		else {
			fi = fitem_new(name, type, selection, FALSE);
			fbox->fitems = g_slist_insert_sorted(fbox->fitems, fi, ordering_func);
		}
	}
}


/* Get the position of this fitem in the box. */
static gint file_box_get_display_pos(FileBox* fbox, FItem* fitem) {
	GSList* fi_iter;
	FItem* fi;
	gint pos = 0;

	for (fi_iter = fbox->fitems; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		if (fi == fitem)
			break;
		else if (fi->displayed)
			pos++;
	}

	return pos;
}


/* Unmark all the FItems.  This is called just before reading in a new bunch of data. */
void file_box_unmark_all(FileBox* fbox) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* fi_iter;
	FItem* fi;

	for (fi_iter = fbox->fitems; fi_iter; fi_iter = g_slist_next(fi_iter)) {
		fi = fi_iter->data;
		fi->marked = FALSE;
	}
}


/* Delete all fitems which are not marked. */
void file_box_cull(FileBox* fbox) {
	g_return_if_fail(IS_FILE_BOX(fbox));

	GSList* fi_iter;
	GSList* tmp;
	FItem* fi;

	fi_iter = fbox->fitems;
	while (fi_iter) {
		fi = fi_iter->data;
		if ( ! fi->marked ) {
			/* Not marked -- no holds barred. */
			if (fi->displayed)
				fbox->n_displayed_files--;
			tmp = fi_iter;
			fi_iter = g_slist_next(fi_iter);
			fbox->fitems = g_slist_delete_link(fbox->fitems, tmp);
			fitem_free(fi, TRUE);
			continue;
		}
		fi_iter = g_slist_next(fi_iter);
	}
}


static void file_box_size_request(GtkWidget* widget, GtkRequisition* requisition) {
	FileBox* this = FILE_BOX(widget);

	wrap_box_size_request_optimal(WRAP_BOX(widget), requisition, this->optimal_width);
}


static FItem* fitem_new(const GString* name, FileType type, FileSelection selection, gboolean build_widgets) {
	FItem* new_fitem;

	new_fitem = g_new(FItem, 1);
	new_fitem->name = g_string_new(name->str);
	new_fitem->type = type;
	new_fitem->selection = selection;
	new_fitem->marked = TRUE;
	new_fitem->displayed = FALSE;
	new_fitem->widget = NULL;

	/* Create the widgets. */
	if (build_widgets)
		fitem_rebuild_widgets(new_fitem);

	return new_fitem;
}


static void fitem_rebuild_widgets(FItem* fi) {

	GtkWidget* label;
	GtkWidget* hbox;
	GtkWidget* eventbox;

	gchar* temp1;
	gchar* temp2;
	gsize  length;

	/* Event Box (to show selection) */
	eventbox = gtk_event_box_new();
	g_printerr("selection: %d\n", fi->selection);
	gtk_widget_set_state(eventbox, fi->selection);

	/* HBox */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(eventbox), hbox);

	/* Icon */
	if (file_type_icons[fi->type]) {
		GtkWidget* icon_image;
		icon_image = gtk_image_new_from_pixbuf(file_type_icons[fi->type]);
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
		fi->displayed = FALSE;
	}

	fi->widget = eventbox;
	gtk_widget_show(eventbox);
}


/* Remove all memory associated with this FItem. */
static void fitem_free(FItem* fi, gboolean destroy_widgets) {
	if (!fi)
		return;

	DEBUG((df, "destroying: %s (%d)\n", fi->name->str, destroy_widgets));

	if (fi->name)
		g_string_free(fi->name, TRUE);
	if (destroy_widgets && fi->widget)
		gtk_widget_destroy(fi->widget);   /* This will grab all the stuff inside, too. */

	g_free(fi);
}


/* Returns true if the update requires external changes, false otherwise. */
static gboolean fitem_update_type_and_selection(FItem* fi, FileType t, FileSelection s) {

	gboolean widgets_changed = FALSE;

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
		if (fi->displayed && fi->widget) {
			fitem_rebuild_widgets(fi);
			widgets_changed = TRUE;
		}
	}

	return widgets_changed;
}


static gint cmp_same_name(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const GString* bb = b;

	return strcmp( aa->name->str, bb->str );
}


/* Sort by type (dir first), then by name (default Windows style). */
static gint cmp_ordering_win(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const FItem* bb = b;

	if (aa->type == FT_DIR) {
		if (bb->type == FT_DIR)
			return strcmp( aa->name->str, bb->name->str );
		else
			return -1;
	}
	else {
		if (bb->type == FT_DIR)
			return 1;
		else
			return strcmp( aa->name->str, bb->name->str );
	}
}


/* Sort strictly by name (default ls style). */
static gint cmp_ordering_ls(gconstpointer a, gconstpointer b) {
	const FItem* aa = a;
	const FItem* bb = b;

	return strcmp( aa->name->str, bb->name->str );
}


