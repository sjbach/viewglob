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
/* A file has two stages of being selected:
     YES:   the file has been explicitly named or expanded from a file glob.
	 MAYBE: the beginning of the file has been named (or expanded). */
#define SELECTION_COUNT 3
enum selection { S_YES, S_NO, S_MAYBE };

#define FILE_TYPE_COUNT 8
enum file_type {
	FT_REGULAR,
	FT_EXECUTABLE,
	FT_DIRECTORY,
	FT_BLOCKDEV,
	FT_CHARDEV,
	FT_FIFO,
	FT_SOCKET,
	FT_SYMLINK,
};

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
	char* name;
	enum selection selected;
	enum file_type type;
	File* next_file;
};

typedef struct _Directory Directory;
struct _Directory {
	char* name;
	dev_t dev_id;
	ino_t inode;
	int selected_count;
	int file_count;
	int hidden_count;
	File* file_list;
	Directory* next_dir;
};


END_C_DECLS

#endif /* !GLOB_EXPAND_H */
