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

#ifndef VGEXPAND_H
#define VGEXPAND_H

#include "common.h"
#include "file-types.h"

G_BEGIN_DECLS


/* Order in which to list the directories:
	SO_DESCENDING: listed in order of appearance on the command line.
	SO_ASCENDING: listed in reverse order.
	SO_ASCENDING_PWD_FIRST: listed in reverse order with pwd first. */
enum sort_order {
	SO_DESCENDING,
	SO_ASCENDING,
	SO_ASCENDING_PWD_FIRST,
};

typedef struct _File File;
struct _File {
	gchar* name;
	FileSelection selected;
	FileType type;
	gboolean shown;
};

typedef struct _Directory Directory;
struct _Directory {
	gchar* name;
	dev_t dev_id;
	ino_t inode;
	gint selected_count;
	gint file_count;
	gint hidden_count;
	GTree* files;
	gboolean is_pwd;
	Directory* next_dir;
	gchar* lookup;       /* Piggyback filename for mark_traverse. */
	gsize  lookup_len;
};

struct mask {
	gchar* pattern;
	gboolean dirs_only;
};


G_END_DECLS

#endif /* !VGEXPAND_H */

