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

#ifndef HARDENED_IO_H
#define HARDENED_IO_H 1

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

G_BEGIN_DECLS

gint open_warning(char* file_name, gint flags, mode_t mode);
void close_warning(gint fd, char* file_name);

gboolean hardened_write(gint, char*, size_t length);
gboolean hardened_read(gint fd, void* buf, size_t count, ssize_t* nread);
gboolean hardened_select(gint fd, fd_set* readfds, fd_set* writefds);

gboolean send_term_size(gint shell_fd);

G_END_DECLS

#endif /* !HARDENED_IO_H */