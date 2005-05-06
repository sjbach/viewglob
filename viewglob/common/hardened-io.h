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

#ifndef HARDENED_IO_H
#define HARDENED_IO_H

#include "common.h"

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

#include <sys/uio.h>

G_BEGIN_DECLS

enum io_result { IOR_OK, IOR_ERROR, IOR_EOF };


gint open_warning(char* file_name, gint flags, mode_t mode);
void close_warning(gint fd, char* file_name);

enum io_result write_all(gint fd, void* buf, gsize bytes);
enum io_result read_all(int fd, void* buf, gsize bytes);
enum io_result writev_all(int fd, struct iovec* vec, int count);

enum io_result hardened_read(gint fd, void* buf, size_t count, ssize_t* nread);
int            hardened_select(gint fd, fd_set* readfds, long milliseconds);

G_END_DECLS

#endif /* !HARDENED_IO_H */
