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

#ifndef GLOB_EXPAND_H
#define GLOB_EXPAND_H 1

#if HAVE_CONFIG_H
#  include "config.h"
#endif

BEGIN_C_DECLS

struct _Directory {
	char* name;
	dev_t dev_id;
	ino_t inode;
	int selected_count;
	int file_count;
	int hidden_count;
	struct _File* file_list;
	struct _Directory* next_dir;
};

/* A file has two stages of being selected:
     YES:   the file has been explicitly named or expanded from a file glob.
	 MAYBE: the beginning of the file has been named (or expanded). */
enum selection { S_YES, S_NO, S_MAYBE };
enum file_type { FT_FILE, FT_DIR };

struct _File {
	char* name;
	enum selection selected;
	enum file_type type;
	struct _File* next_file;
};

typedef struct _Directory Directory;
typedef struct _File File;

static long get_max_path(const char*);
static char* vg_dirname(const char*);
static char* normalize_path(const char*);

static File* make_new_file(char* name, bool is_dir);
static Directory* make_new_dir(char* dir_name, dev_t dev_id, ino_t inode);
static bool mark_files(Directory* dir, char* file_name);
static bool have_dir(char* name, dev_t dev_id, ino_t inode, Directory** return_dir);

END_C_DECLS

#endif /* !GLOB_EXPAND_H */
