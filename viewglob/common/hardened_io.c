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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "vgseer.h"
#include "hardened_io.h"

#include <sys/stat.h>
#include <fcntl.h>


/* Attempt to open the given file with the given flags and mode.
   Emit warning if it doesn't work out. */
gint open_warning(gchar* file_name, gint flags, mode_t mode) {

	g_return_val_if_fail(file_name != NULL, -1);

	gint fd;

	if ((fd = open(file_name, flags, mode)) == -1)
		g_warning("Could not open file \"%s\": %s", file_name, g_strerror(errno));

	return fd;
}


/* Attempt to close the given file.  Emit warning on failure. */
void close_warning(gint fd, gchar* file_name) {
	if (fd != -1 && close(fd) == -1)
		g_warning("Could not close file \"%s\": %s", file_name, g_strerror(errno));
}


/* Retry read on EINTR. */
gboolean hardened_read(int fd, void* buf, size_t count, ssize_t* nread) {
	gboolean ok = TRUE;

	while (TRUE) {
		errno = 0;
		if ( (*nread = read(fd, buf, count)) == -1) {
			if (errno == EINTR)
				continue;
			else {
				ok = FALSE;
				break;
			}
		}
		else
			break;
	}

	return ok;
}


/* Write all length bytes of buff to fd, even if it requires several tries.
   Retry after signal interrupts. */
gboolean hardened_write(gint fd, gchar* buff, size_t length) {
	ssize_t nwritten;
	size_t offset = 0;

	while (length > 0) {
		errno = 0;
		if ( (nwritten = write(fd, buff + offset, length)) == -1 ) {
			if (errno == EINTR)
				nwritten = 0;
			else
				return FALSE;
		}
		length -= nwritten;
		offset += nwritten;
	}

	return TRUE;
}


/* If select is interrupted by a signal, try again. */
gboolean hardened_select(gint n, fd_set* readfds, fd_set* writefds) {

	gboolean ok = TRUE;

	while (TRUE) {
		if (select(n, readfds, writefds, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			else {
				ok = FALSE;
				break;
			}
		}
		else
			break;
	}
	return ok;
}

