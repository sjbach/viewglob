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


#ifndef IO_ALL_H
#define IO_ALL_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include <sys/uio.h>
#include <glib.h>

G_BEGIN_DECLS

enum io_result { IOR_OK, IOR_ERROR, IOR_EOF };

enum io_result write_all(gint fd, void* buf, gsize bytes);
enum io_result read_all(int fd, void* buf, gsize bytes);
enum io_result writev_all(int fd, struct iovec* vec, int count);

G_END_DECLS

#endif