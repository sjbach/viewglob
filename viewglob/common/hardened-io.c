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
#include "hardened-io.h"

#include <sys/stat.h>
#include <fcntl.h>


/* Attempt to open the given file with the given flags and mode.
   Emit warning if it doesn't work out. */
gint open_warning(gchar* file_name, gint flags, mode_t mode) {

	g_return_val_if_fail(file_name != NULL, -1);

	gint fd;

	if ((fd = open(file_name, flags, mode)) == -1)
		g_warning("Could not open file \"%s\": %s", file_name,
				g_strerror(errno));

	return fd;
}


/* Attempt to close the given file.  Emit warning on failure. */
void close_warning(gint fd, gchar* file_name) {
	if (fd != -1 && close(fd) == -1)
		g_warning("Could not close file \"%s\": %s", file_name,
				g_strerror(errno));
}


/* Retry read on EINTR. */
enum io_result hardened_read(gint fd, void* buf, gsize count, gssize* nread) {

	g_return_val_if_fail(fd >= 0, IOR_ERROR);
	g_return_val_if_fail(buf != NULL, IOR_ERROR);

	gboolean result = IOR_OK;

	do {
		*nread = read(fd, buf, count);
	} while (*nread == -1 && errno == EINTR);

	if (*nread == -1)
		result = IOR_ERROR;
	else if (*nread == 0)
		result = IOR_EOF;

	return result;
}


/* Read exactly the given number of bytes. */
enum io_result read_all(gint fd, void* buf, gsize bytes) {

	g_return_val_if_fail(fd >= 0, IOR_ERROR);
	g_return_val_if_fail(buf != NULL, IOR_ERROR);

	gboolean result = IOR_OK;
	gssize nread = 0;

	gchar* pos = buf;
	while (bytes) {
		errno = 0;
		if ( (nread = read(fd, pos, bytes)) == -1) {
			if (errno == EINTR)
				nread = 0;
			else {
				result = IOR_ERROR;
				break;
			}
		}
		else if (nread == 0) {
			result = IOR_EOF;
			break;
		}
		bytes -= nread;
		pos += nread;
	}

	return result;
}


/* Write all length bytes of buf to fd, even if it requires several tries.
   Retry after signal interrupts. */
enum io_result write_all(gint fd, void* buf, gsize bytes) {

	g_return_val_if_fail(fd >= 0, IOR_ERROR);
	g_return_val_if_fail(buf != NULL, IOR_ERROR);

	enum io_result result = IOR_OK;
	gssize nwritten;

	gchar* pos = buf;
	while (bytes) {
		errno = 0;
		if ( (nwritten = write(fd, pos, bytes)) == -1 ) {
			if (errno == EINTR)
				nwritten = 0;
			else {
				result = IOR_ERROR;
				break;
			}
		}
		bytes -= nwritten;
		pos += nwritten;
	}

	return result;
}


/* Wrapper for writev() to ensure everything is written. */
enum io_result writev_all(gint fd, struct iovec* vec, gint count) {

	g_return_val_if_fail(fd >= 0, IOR_ERROR);
	g_return_val_if_fail(vec != NULL, IOR_ERROR);
	g_return_val_if_fail(count >= 0, IOR_ERROR);

	gssize nwritten;
	enum io_result result = IOR_OK;

	do
		nwritten = writev(fd, vec, count);
	while (nwritten == -1 && errno == EINTR);

	if (nwritten == -1)
		result = IOR_ERROR;
	else {
		gchar* ptr;
		while (count > 0) {
			if (nwritten >= vec->iov_len) {
				/* This buffer was completely written. */
				nwritten -= vec->iov_len;
				vec++;
				count--;
			}
			else {
				/* This buffer was partially written. */
				ptr = vec->iov_base;
				ptr += nwritten;
				vec->iov_base = ptr;
				vec->iov_len -= nwritten;
				break;
			}
		}

		if (count > 1)
			result = writev_all(fd, vec, count);
		else if (count == 1)
			result = write_all(fd, vec->iov_base, vec->iov_len);
	}

	return result;
}


/* If select is interrupted by a signal, try again. */
//int hardened_select(gint n, fd_set* readfds, struct timeval* timeout) {
int hardened_select(gint n, fd_set* readfds, long milliseconds) {

	g_return_val_if_fail(n > 0, -1);
	g_return_val_if_fail(readfds != NULL, -1);

	int result;
	struct timeval* t = NULL;

	/* Create a timestruct for the given number of milliseconds. */
	if (milliseconds >= 0) {
		t = g_new(struct timeval, 1);
		t->tv_sec = milliseconds / 1000;
		t->tv_usec = (milliseconds - (1000 * t->tv_sec)) * 1000;
	}

	do {
		result = select(n, readfds, NULL, NULL, t);
	} while (result == -1 && errno == EINTR);

	g_free(t);
	return result;
}

