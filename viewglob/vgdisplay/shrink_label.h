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

#ifndef SHRINK_LABEL_H
#define SHRINK_LABEL_H

#include <gtk/gtklabel.h>

G_BEGIN_DECLS

/* --- type macros --- */
#define SHRINK_LABEL_TYPE		     (shrink_label_get_type())
#define SHRINK_LABEL(obj)	         (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHRINK_LABEL_TYPE, ShrinkLabel))
#define SHRINK_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SHRINK_LABEL_TYPE, ShrinkLabelClass))
#define IS_SHRINK_LABEL(obj)	     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHRINK_LABEL_TYPE))
#define IS_SHRINK_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SHRINK_LABEL_TYPE))
#define SHRINK_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SHRINK_LABEL_TYPE, ShrinkLabelClass))

/* --- typedefs --- */
typedef struct _ShrinkLabel ShrinkLabel;
typedef struct _ShrinkLabelClass ShrinkLabelClass;

/* --- ShrinkLabel --- */
struct _ShrinkLabel {
	GtkLabel label;

	gchar* long_text;
	gchar* short_text;
};

struct _ShrinkLabelClass {
	GtkLabelClass parent_class;
};


/* --- prototypes --- */
GType       shrink_label_get_type(void) G_GNUC_CONST;
GtkWidget*  shrink_label_new(void);
void        shrink_label_set_long_text(ShrinkLabel* sl, gchar* text);
void        shrink_label_set_short_text(ShrinkLabel* sl, gchar* text);

G_END_DECLS

#endif /* !SHRINK_LABEL_H */

