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

#ifndef FILE_TYPES_H
#define FILE_TYPES_H

#include <glib.h>

G_BEGIN_DECLS


#define FILE_TYPES_NUM 8
typedef enum _FileType FileType;
enum _FileType {
	FT_REGULAR,
	FT_EXECUTABLE,
	FT_DIRECTORY,
	FT_BLOCKDEV,
	FT_CHARDEV,
	FT_FIFO,
	FT_SOCKET,
	FT_SYMLINK,
};


G_END_DECLS

#endif  /* !FILE_TYPES_H */

