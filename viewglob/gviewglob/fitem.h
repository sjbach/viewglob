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

#ifndef FITEM_H
#define FITEM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum selection_state {
	S_YES = GTK_STATE_SELECTED,
	S_NO = GTK_STATE_NORMAL,
	S_MAYBE = GTK_STATE_ACTIVE,
};

enum file_type {
	T_FILE,
	T_DIR,
};

typedef struct _FItem FItem;
struct _FItem {
	GString* name;
	enum file_type type;
	enum selection_state selection;
	gboolean marked;
	gboolean is_new;

	GtkWidget* widget;
};


FItem*    fitem_new(const GString* name, enum selection_state state, enum file_type type, gboolean build_widgets);
void fitem_rebuild_widgets(FItem* fi);
void      fitem_unmark_all(GSList* fi_slist);
void      fitem_mark(FItem* fi);
gboolean  fitem_update_type_and_state(FItem* fi, enum file_type t, enum selection_state s);
void      fitem_free(FItem* fi, gboolean destroy_widgets);

G_END_DECLS

#endif /* !FITEM_H */
