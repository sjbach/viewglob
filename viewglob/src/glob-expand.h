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

#define GLOB_EXPAND_VERSION "0.8.1"
#define GLOB_EXPAND_RELEASE_DATE "August 11, 2004"

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

struct _File {
	char* name;
	enum selection selected;
	enum file_type type;
	struct _File* next_file;
};

typedef struct _Directory Directory;
typedef struct _File File;

END_C_DECLS

#endif /* !GLOB_EXPAND_H */
