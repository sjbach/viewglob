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

/* The following is a heavily pillaged version of gtkwrapbox.h and
 * gtkvwrapbox.h from the GIMP 2.0.0 package.
 */

#ifndef WRAP_BOX_H
#define WRAP_BOX_H

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

/* --- type macros --- */
#define WRAP_BOX_TYPE		       (wrap_box_get_type ())
#define WRAP_BOX(obj)	           (G_TYPE_CHECK_INSTANCE_CAST ((obj), WRAP_BOX_TYPE, WrapBox))
#define WRAP_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WRAP_BOX_TYPE, WrapBoxClass))
#define IS_WRAP_BOX(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WRAP_BOX_TYPE))
#define IS_WRAP_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WRAP_BOX_TYPE))
#define WRAP_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WRAP_BOX_TYPE, WrapBoxClass))

/* --- typedefs --- */
typedef struct _WrapBox WrapBox;
typedef struct _WrapBoxClass WrapBoxClass;
typedef struct _WrapBoxChild WrapBoxChild;

/* --- WrapBox --- */
struct _WrapBox {
	GtkContainer   container;

	guint          max_child_width;
	guint          max_child_height;
	guint          optimal_width;

	guint          justify : 4;
	guint          line_justify : 4;
	guint8         hspacing;
	guint8         vspacing;
	guint16        n_children;
	WrapBoxChild*  children;
	guint          child_limit;
};

struct _WrapBoxClass {
	GtkContainerClass parent_class;

};

struct _WrapBoxChild {
	GtkWidget*  widget;

	WrapBoxChild* next;
};

#define GTK_JUSTIFY_TOP  GTK_JUSTIFY_LEFT
#define GTK_JUSTIFY_BOTTOM GTK_JUSTIFY_RIGHT

/* --- prototypes --- */
GType      wrap_box_get_type(void) G_GNUC_CONST;
GtkWidget* wrap_box_new(void);
void       wrap_box_set_hspacing(WrapBox *wbox, guint hspacing);
void       wrap_box_set_vspacing(WrapBox *wbox, guint vspacing);
void       wrap_box_set_justify(WrapBox *wbox, GtkJustification justify);
void       wrap_box_set_line_justify(WrapBox *wbox, GtkJustification line_justify);
void       wrap_box_set_optimal_width(WrapBox* wbox, guint optimal_width);
void       wrap_box_pack(WrapBox *wbox, GtkWidget *child);
void       wrap_box_reorder_child(WrapBox *wbox, GtkWidget *child, gint position);

G_END_DECLS

#endif /* !WRAP_BOX_H */
