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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "io-all.h"

#include <unistd.h>
#include <errno.h>

#include <glib.h>


/* Write all length bytes of buf to fd, even if it requires several tries.
   Retry after signal interrupts. */
enum io_result write_all(gint fd, void* buf, gsize bytes) {

	g_return_val_if_fail(fd >= 0, FALSE);
	g_return_val_if_fail(buf != NULL, FALSE);

	enum io_result result = IOR_OK;
	ssize_t nwritten;

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


/* Read exactly the given number of bytes. */
enum io_result read_all(int fd, void* buf, gsize bytes) {

	g_return_val_if_fail(fd >= 0, FALSE);
	g_return_val_if_fail(buf != NULL, FALSE);

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


/* Wrapper for writev() to ensure everything is written. */
enum io_result writev_all(int fd, struct iovec* vec, int count) {

	g_return_val_if_fail(fd >= 0, FALSE);
	g_return_val_if_fail(vec != NULL, FALSE);
	g_return_val_if_fail(count >= 0, FALSE);

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

