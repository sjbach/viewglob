/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkWrapBox: Wrapping box widget
 * Copyright (C) 1999 Tim Janik
 *
 * WrapBox: Wrapping box widget
 * Copyright (C) 2004 Stephen Bach
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* The following is a heavily pillaged version of gtkwrapbox.c and
 * gtkvwrapbox.c from the GIMP 2.0.0 package.
 */

#include "wrap_box.h"


/* --- properties --- */
enum {
  PROP_0,
  PROP_JUSTIFY,
  PROP_HSPACING,
  PROP_VSPACING,
  PROP_LINE_JUSTIFY,
  PROP_CHILD_LIMIT,
  //PROP_OPTIMAL_WIDTH,
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_POSITION,
};


/* --- prototypes --- */
static void   wrap_box_class_init(WrapBoxClass *klass);
static void   wrap_box_init(WrapBox *wbox);
static void   wrap_box_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void   wrap_box_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void   wrap_box_set_child_property(GtkContainer *container, GtkWidget *child, guint property_id, const GValue *value, GParamSpec *pspec);
static void   wrap_box_get_child_property(GtkContainer *container, GtkWidget *child, guint property_id, GValue *value, GParamSpec *pspec);
static void   wrap_box_map(GtkWidget *widget);
static void   wrap_box_unmap(GtkWidget *widget);
static gint   wrap_box_expose(GtkWidget *widget, GdkEventExpose *event);
static void   wrap_box_add(GtkContainer *container, GtkWidget *widget);
static void   wrap_box_remove_simple(GtkContainer *container, GtkWidget *widget);

static void   wrap_box_forall(GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);
static GType  wrap_box_child_type(GtkContainer *container);

static void     wrap_box_size_request(GtkWidget *widget, GtkRequisition *requisition);
void            wrap_box_size_request_optimal(GtkWidget* widget, GtkRequisition* requisition, guint optimal_width);
static void     wrap_box_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static GSList*  reverse_list_col_children(WrapBox *wbox, WrapBoxChild **child_p, GtkAllocation *area, guint *max_child_width);

static guint get_n_visible_children(WrapBox* this);
static guint get_upper_bound_cols(WrapBox* this, guint optimal_width);


/* --- variables --- */
static gpointer parent_class = NULL;


/* --- functions --- */
GType wrap_box_get_type(void) {
	static GType wrap_box_type = 0;

	if (!wrap_box_type) {
		static const GTypeInfo wrap_box_info = {
			sizeof (WrapBoxClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) wrap_box_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (WrapBox),
			0,		/* n_preallocs */
			(GInstanceInitFunc) wrap_box_init,
		};
		wrap_box_type = g_type_register_static (GTK_TYPE_CONTAINER, "WrapBox", &wrap_box_info, 0);
	}

	return wrap_box_type;
}

static void wrap_box_class_init (WrapBoxClass *class) {
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;
	
	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	container_class = GTK_CONTAINER_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);
	
	object_class->set_property = wrap_box_set_property;
	object_class->get_property = wrap_box_get_property;
	
	widget_class->map = wrap_box_map;
	widget_class->unmap = wrap_box_unmap;
	widget_class->expose_event = wrap_box_expose;
	widget_class->size_request = wrap_box_size_request;
	widget_class->size_allocate = wrap_box_size_allocate;
	
	container_class->add = wrap_box_add;
	container_class->remove = wrap_box_remove_simple;
	container_class->forall = wrap_box_forall;
	container_class->child_type = wrap_box_child_type;
	container_class->set_child_property = wrap_box_set_child_property;
	container_class->get_child_property = wrap_box_get_child_property;

	/*class->rlist_line_children = NULL;*/

	g_object_class_install_property (object_class,
				   PROP_JUSTIFY,
				   g_param_spec_enum ("justify",
						      NULL,
						      NULL,
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_LEFT,
						      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
				   PROP_HSPACING,
				   g_param_spec_uint ("hspacing",
						      NULL,
						      NULL,
						      0,
						      G_MAXINT,
						      0,
						      G_PARAM_READWRITE));
		
	g_object_class_install_property (object_class,
				   PROP_VSPACING,
				   g_param_spec_uint ("vspacing",
						      NULL,
						      NULL,
						      0,
						      G_MAXINT,
						      0,
						      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
				   PROP_LINE_JUSTIFY,
				   g_param_spec_enum ("line_justify",
						      NULL,
						      NULL,
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_BOTTOM,
						      G_PARAM_READWRITE));

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


	g_object_class_install_property (object_class,
				   PROP_CHILD_LIMIT,
				   g_param_spec_uint ("max_children_per_line",
						      NULL,
						      NULL,
						      1,
						      32767,
						      32767,
						      G_PARAM_READWRITE));

	gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_POSITION,
					      g_param_spec_int ("position",
								NULL,
								NULL,
								-1, G_MAXINT, 0,
								G_PARAM_READWRITE));
}


static void wrap_box_init(WrapBox *wbox) {
	GTK_WIDGET_SET_FLAGS (wbox, GTK_NO_WINDOW);

	wbox->hspacing = 0;
	wbox->vspacing = 0;
	wbox->justify = GTK_JUSTIFY_LEFT;
	wbox->line_justify = GTK_JUSTIFY_BOTTOM;
	wbox->n_children = 0;
	wbox->children = NULL;
	wbox->child_limit = 32767;
	wbox->max_child_width = 0;
	wbox->max_child_height = 0;
	//wbox->optimal_width = 0;
}


GtkWidget* wrap_box_new (void) {
	return g_object_new (WRAP_BOX_TYPE, NULL);
}


static void wrap_box_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
	WrapBox *wbox = WRAP_BOX (object);
	
	switch (property_id) {
		case PROP_JUSTIFY:
			wrap_box_set_justify (wbox, g_value_get_enum (value));
			break;
		case PROP_LINE_JUSTIFY:
			wrap_box_set_line_justify (wbox, g_value_get_enum (value));
			break;
		case PROP_HSPACING:
			wrap_box_set_hspacing (wbox, g_value_get_uint (value));
			break;
		case PROP_VSPACING:
			wrap_box_set_vspacing (wbox, g_value_get_uint (value));
			break;
		case PROP_CHILD_LIMIT:
			if (wbox->child_limit != g_value_get_uint (value))
				gtk_widget_queue_resize (GTK_WIDGET (wbox));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void wrap_box_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
	WrapBox *wbox = WRAP_BOX (object);

	switch (property_id) {
		case PROP_JUSTIFY:
			g_value_set_enum (value, wbox->justify);
			break;
		case PROP_LINE_JUSTIFY:
			g_value_set_enum (value, wbox->line_justify);
			break;
		case PROP_HSPACING:
			g_value_set_uint (value, wbox->hspacing);
			break;
		case PROP_VSPACING:
			g_value_set_uint (value, wbox->vspacing);
			break;
		case PROP_CHILD_LIMIT:
			g_value_set_uint (value, wbox->child_limit);
			break;
		//case PROP_OPTIMAL_WIDTH:
		//	g_value_set_uint (value, wbox->optimal_width);
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void wrap_box_set_child_property (GtkContainer *container, GtkWidget *child, guint property_id, const GValue *value, GParamSpec *pspec) {
  WrapBox *wbox = WRAP_BOX (container);

  switch (property_id) {
		case CHILD_PROP_POSITION:
			wrap_box_reorder_child (wbox, child, g_value_get_int (value));
			break;
		default:
			GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
			break;
	}
}


static void wrap_box_get_child_property (GtkContainer *container, GtkWidget *child, guint property_id, GValue *value, GParamSpec *pspec) {
  WrapBox *wbox = WRAP_BOX (container);
  
  switch (property_id)
		{
			WrapBoxChild *child_info;
			guint i;
		case CHILD_PROP_POSITION:
			i = 0;
			for (child_info = wbox->children; child_info; child_info = child_info->next) {
				if (child_info->widget == child)
					break;
				i += 1;
			}
			g_value_set_int (value, child_info ? i : -1);
			break;
		default:
			GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
			break;
	}
}


static GType wrap_box_child_type(GtkContainer *container) {
	return GTK_TYPE_WIDGET;
}


void wrap_box_set_hspacing(WrapBox *wbox, guint hspacing) {
	g_return_if_fail (IS_WRAP_BOX (wbox));

	if (wbox->hspacing != hspacing) {
		wbox->hspacing = hspacing;
		gtk_widget_queue_resize (GTK_WIDGET (wbox));
	}
}

void wrap_box_set_vspacing(WrapBox *wbox, guint vspacing) {
	g_return_if_fail (IS_WRAP_BOX (wbox));
	
	if (wbox->vspacing != vspacing) {
		wbox->vspacing = vspacing;
		gtk_widget_queue_resize (GTK_WIDGET (wbox));
	}
}

void wrap_box_set_justify(WrapBox *wbox, GtkJustification justify) {
	g_return_if_fail (IS_WRAP_BOX (wbox));
	g_return_if_fail (justify <= GTK_JUSTIFY_FILL);

	if (wbox->justify != justify) {
		wbox->justify = justify;
		gtk_widget_queue_resize (GTK_WIDGET (wbox));
	}
}

void wrap_box_set_line_justify(WrapBox *wbox, GtkJustification line_justify) {
	g_return_if_fail (IS_WRAP_BOX (wbox));
	g_return_if_fail (line_justify <= GTK_JUSTIFY_FILL);

	if (wbox->line_justify != line_justify) {
		wbox->line_justify = line_justify;
		gtk_widget_queue_resize (GTK_WIDGET (wbox));
	}
}



#if 0
void wrap_box_set_optimal_width(WrapBox* wbox, guint optimal_width) {
	g_return_if_fail(IS_WRAP_BOX(wbox));
	g_return_if_fail(optimal_width >= GTK_CONTAINER(wbox)->border_width * 2);
	/*g_printerr("(dopt: %d)", optimal_width);*/

	/*g_printerr("border_width: %d", GTK_CONTAINER(wbox)->border_width * 2);*/

	optimal_width -= GTK_CONTAINER(wbox)->border_width * 2;

	if (wbox->optimal_width != optimal_width) {
		/*g_printerr("<new optimal_width: %d>\n", optimal_width);*/
		wbox->optimal_width = optimal_width;
		gtk_widget_queue_resize(GTK_WIDGET(wbox));
	}
}
#endif


void wrap_box_pack(WrapBox *wbox, GtkWidget *child, gboolean do_resize) {
	WrapBoxChild *child_info;

	g_return_if_fail (IS_WRAP_BOX (wbox));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (child->parent == NULL);

	child_info = g_new (WrapBoxChild, 1);
	child_info->widget = child;
	child_info->next = NULL;
	if (wbox->children) {
		WrapBoxChild *last = wbox->children;
		
		while (last->next)
			last = last->next;
		last->next = child_info;
	}
	else
		wbox->children = child_info;
	wbox->n_children++;

	gtk_widget_set_parent (child, GTK_WIDGET (wbox));

	if (GTK_WIDGET_REALIZED (wbox))
		gtk_widget_realize (child);

	if (GTK_WIDGET_VISIBLE (wbox) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (wbox))
			gtk_widget_map (child);

		if (do_resize)
			gtk_widget_queue_resize (child);
	}
}


/* Pack this widget at the given position. */
void wrap_box_pack_pos(WrapBox* wbox, GtkWidget* child, guint pos, gboolean do_resize) {
	WrapBoxChild* child_info;

	g_return_if_fail(IS_WRAP_BOX (wbox));
	g_return_if_fail(GTK_IS_WIDGET (child));
	g_return_if_fail(child->parent == NULL);

	child_info = g_new(WrapBoxChild, 1);
	child_info->widget = child;
	
	if (wbox->children) {
		WrapBoxChild* iter = wbox->children;
		WrapBoxChild* prev = NULL;
		while (pos && iter) {
			prev = iter;
			iter = iter->next;
			pos--;
		}
		if (prev)
			prev->next = child_info;
		else
			wbox->children = child_info;
		child_info->next = iter;
	}
	else {
		wbox->children = child_info;
		child_info->next = NULL;
	}
	wbox->n_children++;

	gtk_widget_set_parent(child, GTK_WIDGET(wbox));

	if (GTK_WIDGET_REALIZED(wbox))
		gtk_widget_realize(child);

	if (GTK_WIDGET_VISIBLE(wbox) && GTK_WIDGET_VISIBLE(child)) {
		if (GTK_WIDGET_MAPPED(wbox))
			gtk_widget_map(child);

		if (do_resize)
			gtk_widget_queue_resize (child);
	}
}


void wrap_box_reorder_child (WrapBox *wbox, GtkWidget *child, gint position) {
	WrapBoxChild *child_info, *prev = NULL;

	g_return_if_fail (IS_WRAP_BOX (wbox));
	g_return_if_fail (GTK_IS_WIDGET (child));

	for (child_info = wbox->children; child_info; prev = child_info, child_info = prev->next)
		if (child_info->widget == child)
			break;

	if (child_info && wbox->children->next) {
		WrapBoxChild *tmp;

		if (prev)
			prev->next = child_info->next;
		else
			wbox->children = child_info->next;

		prev = NULL;
		tmp = wbox->children;
		while (position && tmp->next) {
			position--;
			prev = tmp;
			tmp = prev->next;
		}

		if (position) {
			tmp->next = child_info;
			child_info->next = NULL;
		}
		else {
			child_info->next = tmp;
			if (prev)
				prev->next = child_info;
			else
				wbox->children = child_info;
		}

		if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (wbox))
			gtk_widget_queue_resize (child);
	}
}


static void wrap_box_map (GtkWidget *widget) {
	WrapBox *wbox = WRAP_BOX (widget);
	WrapBoxChild *child;

	GTK_WIDGET_SET_FLAGS (wbox, GTK_MAPPED);

	for (child = wbox->children; child; child = child->next)
		if (GTK_WIDGET_VISIBLE (child->widget) && !GTK_WIDGET_MAPPED (child->widget))
			gtk_widget_map (child->widget);
}

static void wrap_box_unmap (GtkWidget *widget) {
	WrapBox *wbox = WRAP_BOX (widget);
	WrapBoxChild *child;

	GTK_WIDGET_UNSET_FLAGS (wbox, GTK_MAPPED);

	for (child = wbox->children; child; child = child->next)
		if (GTK_WIDGET_VISIBLE (child->widget) && GTK_WIDGET_MAPPED (child->widget))
			gtk_widget_unmap (child->widget);
}

static gint wrap_box_expose (GtkWidget *widget, GdkEventExpose *event) {
	return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}

static void wrap_box_add (GtkContainer *container, GtkWidget *widget) {
	wrap_box_pack (WRAP_BOX (container), widget, TRUE);
}

static void wrap_box_remove_simple(GtkContainer *container, GtkWidget *widget) {
	wrap_box_remove(container, widget, FALSE);
}

void wrap_box_remove (GtkContainer *container, GtkWidget *widget, gboolean do_resize) {
	WrapBox *wbox = WRAP_BOX (container);
	WrapBoxChild *child, *last = NULL;

	child = wbox->children;
	while (child) {
		if (child->widget == widget) {
			gboolean was_visible;

			was_visible = GTK_WIDGET_VISIBLE (widget);
			gtk_widget_unparent (widget);

			if (last)
				last->next = child->next;
			else
				wbox->children = child->next;
			g_free (child);
			wbox->n_children--;

			if (was_visible && do_resize)
				gtk_widget_queue_resize (GTK_WIDGET (container));

			break;
		}

	last = child;
	child = last->next;
	}
}

static void wrap_box_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data) {
	WrapBox *wbox = WRAP_BOX (container);
	WrapBoxChild *child;

	child = wbox->children;
	while (child) {
		GtkWidget *widget = child->widget;

		child = child->next;

		callback (widget, callback_data);
	}
}


/* --- */

static inline void get_child_requisition (WrapBox *wbox, GtkWidget *child, GtkRequisition *child_requisition) {
		gtk_widget_get_child_requisition (child, child_requisition);
}


/* Get an upper bound on the number of possible columns. */
static guint get_upper_bound_cols(WrapBox* this, guint optimal_width) {
	WrapBoxChild* child;
	GtkRequisition child_req;

	guint width = 0;
	guint cols = 0;

	for (child = this->children; child; child = child->next) {
		if (GTK_WIDGET_VISIBLE(child->widget)) {

			if (cols)
				width += this->hspacing;

			gtk_widget_size_request(child->widget, &child_req);
			width += child_req.width;

			if (width <= optimal_width)
				cols++;
		}
	}

	return cols;
}


static guint get_n_visible_children(WrapBox* this) {
	WrapBoxChild* child;
	guint16 n_visible_children = 0;

	for (child = this->children; child; child = child->next) {
		if (GTK_WIDGET_VISIBLE(child->widget))
			n_visible_children++;
	}

	return n_visible_children;
}


/* This is the smart size request; we try to maximize the width of the file box. */
void wrap_box_size_request_optimal(GtkWidget* widget, GtkRequisition* requisition, guint optimal_width) {
	WrapBox* this = WRAP_BOX(widget);
	WrapBoxChild* child;
	GtkRequisition child_req;

	guint rows, row;
	guint col_width;
	guint child_width;
	guint total_width;

	guint col, cols;
	guint16 n_visible_children;

	cols = get_upper_bound_cols(this, optimal_width);
	n_visible_children = get_n_visible_children(this);

	/* Get "max" child height. */
	child = this->children;
	if (child) {
		gtk_widget_size_request(child->widget, &child_req);
		this->max_child_height = child_req.height;
	}

	start:
	if (cols && n_visible_children) {

		total_width = 0;
		col_width = 0;
		rows = n_visible_children / cols + CLAMP(n_visible_children % cols, 0, 1);
		row = 1;
		col = 1;

		/*g_printerr("__");*/
		for (child = this->children; child; child = child->next) {
			if (GTK_WIDGET_VISIBLE(child->widget)) {
				gtk_widget_size_request(child->widget, &child_req);

				child_width = child_req.width;
				/*g_printerr("{%d}", child_width);*/

				if (col > 1)
					child_width += this->hspacing;

				col_width = MAX(col_width, child_width);
				if (total_width + col_width > optimal_width) {
					/* Too many columns.  Start over with one less. */
					cols--;
					goto start;
				}
				else if (row < rows)
					row++;
				else {
					/*g_printerr("<%d>", col_width);*/

					/* Add the column. */
					total_width += col_width;

					row = 1;
					col++;
					col_width = 0;
				}
			}
		}
		if (col_width) {
			/*g_printerr("<%d>", col_width);*/
			/* This column wasn't completed. */
			total_width += col_width;
		}
		requisition->width = total_width + GTK_CONTAINER(this)->border_width * 2;
		requisition->height = (rows - 1) * this->vspacing + rows * this->max_child_height + GTK_CONTAINER(this)->border_width * 2;
	}
	else {
		/* Make the best of the situation. */
		/*g_printerr("{no go}");*/
		requisition->width = optimal_width + GTK_CONTAINER(this)->border_width * 2;
		requisition->height = n_visible_children * this->vspacing + n_visible_children * this->max_child_height + GTK_CONTAINER(this)->border_width * 2;
	}

	/*g_printerr("\ncols: %d, rows: %d\n", cols, rows);*/
	/*g_printerr("req: width: %d, height: %d\n", requisition->width, requisition->height);*/
}


/* This is the dumb size request -- we have no knowledge of a good width, so we just go for
   one column. */
static void wrap_box_size_request(GtkWidget* widget, GtkRequisition* requisition) {
	WrapBox* this = WRAP_BOX(widget);
	WrapBoxChild* child;
	GtkRequisition child_req;

	guint width = 0, height = 0;

	for (child = this->children; child; child = child->next) {
		if (GTK_WIDGET_VISIBLE(child->widget)) {
			gtk_widget_size_request(child->widget, &child_req);
			width = MAX(width, child_req.width);
			height += child_req.height;
		}
	}

	requisition->width = width + GTK_CONTAINER(this)->border_width * 2;
	requisition->height = height + GTK_CONTAINER(this)->border_width * 2;
}


static GSList* reverse_list_col_children (WrapBox *wbox, WrapBoxChild **child_p, GtkAllocation *area, guint *max_child_width) {
	GSList *slist = NULL;
	guint height = 0, col_height = area->height;
	WrapBoxChild *child = *child_p;

	*max_child_width = 0;

	/* Get first visible child. */
	while (child && !GTK_WIDGET_VISIBLE (child->widget)) {
		*child_p = child->next;
		child = *child_p;
	}

	if (child) {
		GtkRequisition child_requisition;
		guint n = 1;

		get_child_requisition (wbox, child->widget, &child_requisition);
		height += child_requisition.height;
		*max_child_width = MAX (*max_child_width, child_requisition.width);
		slist = g_slist_prepend (slist, child);
		*child_p = child->next;
		child = *child_p;

		while (child && n < wbox->child_limit) {
			if (GTK_WIDGET_VISIBLE (child->widget)) {
				get_child_requisition (wbox, child->widget, &child_requisition);
				if (height + wbox->vspacing + child_requisition.height > col_height)
					break;
				height += wbox->vspacing + child_requisition.height;
				*max_child_width = MAX (*max_child_width, child_requisition.width);
				slist = g_slist_prepend (slist, child);
				n++;
			}
			*child_p = child->next;
			child = *child_p;
		}
	}

	slist = g_slist_reverse (slist);
	return slist;
}


static void layout_col(WrapBox *wbox, GtkAllocation *area, GSList *children, guint children_per_line) {
	GSList *slist;
	guint n_children = 0, n_expand_children = 0, have_expand_children = 0;
	gint total_height = 0;
	gfloat y, height, extra;
	GtkAllocation child_allocation;

	for (slist = children; slist; slist = slist->next) {
		WrapBoxChild *child = slist->data;
		GtkRequisition child_requisition;

		n_children++;

		get_child_requisition (wbox, child->widget, &child_requisition);
		total_height += child_requisition.height;
	}

	height = MAX (1, area->height - (n_children - 1) * wbox->vspacing);
	if (height > total_height)
		extra = height - total_height;
	else
		extra = 0;
	have_expand_children = n_expand_children && extra;

	y = area->y;
	if (have_expand_children && wbox->justify != GTK_JUSTIFY_FILL) {
		height = extra;
		extra /= ((gdouble) n_expand_children);
	}
	else {
		if (wbox->justify == GTK_JUSTIFY_FILL) {
			height = extra;
			have_expand_children = TRUE;
			n_expand_children = n_children;
			extra /= ((gdouble) n_expand_children);
		}
		else if (wbox->justify == GTK_JUSTIFY_CENTER) {
			y += extra / 2;
			height = 0;
			extra = 0;
		}
		else if (wbox->justify == GTK_JUSTIFY_LEFT) {
			height = 0;
			extra = 0;
		}
		else if (wbox->justify == GTK_JUSTIFY_RIGHT) {
			y += extra;
			height = 0;
			extra = 0;
		}
	}

	n_children = 0;
	for (slist = children; slist; slist = slist->next) {
		WrapBoxChild *child = slist->data;

		child_allocation.y = y;
		child_allocation.x = area->x;
		GtkRequisition child_requisition;

		get_child_requisition (wbox, child->widget, &child_requisition);

		if (child_requisition.width >= area->width)
			child_allocation.width = area->width;
		else {
			child_allocation.width = child_requisition.width;
			if (wbox->line_justify == GTK_JUSTIFY_FILL)
				child_allocation.width = area->width;
			else if (wbox->line_justify == GTK_JUSTIFY_CENTER)
				child_allocation.x += (area->width - child_requisition.width) / 2;
			else if (wbox->line_justify == GTK_JUSTIFY_BOTTOM)
				child_allocation.x += area->width - child_requisition.width;
		}

		if (have_expand_children) {
			child_allocation.height = child_requisition.height;
			if (wbox->justify == GTK_JUSTIFY_FILL) {
				guint space;

				n_expand_children--;
				space = extra * n_expand_children;
				space = height - space;
				height -= space;
				child_allocation.y += space / 2;
				y += space;
			}
		}
		else {
			/* g_printerr ("child_allocation.y %d += %d * %f ", child_allocation.y, n_children, extra); */
			child_allocation.y += n_children * extra;
			/* g_printerr ("= %d\n", child_allocation.y); */
			child_allocation.height = MIN (child_requisition.height, area->height - child_allocation.y + area->y);
		}

		y += child_allocation.height + wbox->vspacing;
		gtk_widget_size_allocate (child->widget, &child_allocation);
		n_children++;
	}
}


/* A column of children. */
typedef struct _Line Line;
struct _Line {
	GSList  *children;
	guint16  width;
	Line     *next;
};


static void layout_cols (WrapBox *wbox, GtkAllocation *area) {
	WrapBoxChild *next_child;
	guint min_width;
	GSList *slist;
	Line *line_list = NULL;
	guint total_width = 0, n_lines = 0;
	gfloat shrink_width;
	guint children_per_line;

	next_child = wbox->children;

	slist = reverse_list_col_children(wbox, &next_child, area, &min_width);
	/*slist = g_slist_reverse (slist);*/

	children_per_line = g_slist_length (slist);
	while (slist) {
		Line *line = g_new (Line, 1);
		/*g_printerr("[_%d_]", min_width);*/

		line->children = slist;
		line->width = min_width;
		total_width += min_width;
		line->next = line_list;
		line_list = line;
		n_lines++;

		slist = reverse_list_col_children(wbox, &next_child, area, &min_width);
		/*slist = g_slist_reverse (slist);*/
	}

	if (total_width > area->width) {
		/*g_printerr("<:::t: %d a: %d>", total_width, area->width);*/
		shrink_width = total_width - area->width;
	}
	else
		shrink_width = 0;

	if (1) { /* reverse lines and shrink */
		Line *prev = NULL, *last = NULL;
		gfloat n_shrink_lines = n_lines;

		while (line_list) {
			Line *tmp = line_list->next;

			if (shrink_width) {
				Line *line = line_list;
				guint shrink_fract = shrink_width / n_shrink_lines + 0.5;

				if (line->width > shrink_fract) {
					shrink_width -= shrink_fract;
					line->width -= shrink_fract;
				}
				else {
					shrink_width -= line->width - 1;
					line->width = 1;
				}
			}
			n_shrink_lines--;

			last = line_list;
			line_list->next = prev;
			prev = line_list;
			line_list = tmp;
		}
		line_list = last;
	}

	if (n_lines) {
		Line *line;
		gfloat x;

		x = area->x;
		line = line_list;
		while (line) {
			GtkAllocation col_allocation;
			Line *next_line = line->next;

			col_allocation.y = area->y;
			col_allocation.height = area->height;
			col_allocation.width = line->width;

			col_allocation.x = x;
			/*g_printerr("[x: %d]", col_allocation.x);*/

			x += col_allocation.width + wbox->hspacing;
			layout_col (wbox, &col_allocation, line->children, children_per_line);

			g_slist_free (line->children);
			g_free (line);
			line = next_line;
		}
	}
}


static void wrap_box_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
	WrapBox *wbox = WRAP_BOX (widget);
	GtkAllocation area;
	gint border = GTK_CONTAINER (wbox)->border_width; /*<h2v-skip>*/

	widget->allocation = *allocation;
	area.y = allocation->y + border;
	area.x = allocation->x + border;
	area.height = MAX (1, (gint) allocation->height - border * 2);
	area.width = MAX (1, (gint) allocation->width - border * 2);

	/*<h2v-off>*/
	/*g_printerr ("got: width %d, height %d\n\n", allocation->width, allocation->height);*/
	/*<h2v-on>*/

	layout_cols (wbox, &area);
}


