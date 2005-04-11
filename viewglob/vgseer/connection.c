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
#include "connection.h"
#include "actions.h"
#include "hardened-io.h"

#include <string.h>


/* Initialize the given Connection. */
void connection_init(Connection* cnct, gchar* name, int fd_in, int fd_out,
		gchar* buf, gsize buflen, enum process_level pl) {

	g_return_if_fail(cnct != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(fd_in >= 0);
	g_return_if_fail(fd_out >= 0);
	g_return_if_fail(buf != NULL);

	cnct->name = name;
	cnct->fd_in = fd_in;
	cnct->fd_out = fd_out;
	cnct->buf = buf;
	cnct->size = buflen;
	cnct->filled = 0;
	cnct->pos = 0;
	cnct->seglen = 0;
	cnct->pl = pl;
	cnct->status = MS_NO_MATCH;
	cnct->holdover = NULL;
	cnct->ho_written = FALSE;
	cnct->skip = 0;

	/* The shell_status is of course only relevant to the user shell. */
	cnct->ss = SS_EXECUTING;
}


/* Free the read/write and holdover buffers. */
void connection_free(Connection* cnct) {

	g_return_if_fail(cnct != NULL);

	if (cnct->holdover) {
		g_free(cnct->holdover);
		cnct->holdover = NULL;
	}
}


/* Prepend the holdover from the last read onto the beginning
   of the current buffer. If there is no holdover, just initialize the
   offsets. */
void prepend_holdover(Connection* b) {

	g_return_if_fail(b != NULL);

	if (b->holdover) {
		size_t ho_len;
		ho_len = strlen(b->holdover);

		/* Copy over the holdover and then free it. */
		memcpy(b->buf, b->holdover, ho_len);
		g_free(b->holdover);
		b->holdover = NULL;

		b->filled = ho_len;
		b->pos = 0;
		/* b->seglen has not changed since the create_holdover(). */

		if (b->ho_written)
			b->skip = ho_len;
		else
			b->skip = 0;
	}
	else {
		b->filled = 0;
		b->pos = 0;
		b->seglen = 0;
		b->skip = 0;
	}
}


/* Cut the segment under investigation into a holdover, to be prepended to
   a later buffer. */
void create_holdover(Connection* b, gboolean write_later) {

	g_return_if_fail(b != NULL);
	g_return_if_fail(b->holdover == NULL);

	/* Copy as null-terminated string. */
	b->holdover = g_strndup(b->buf + b->pos, b->seglen);

	if (write_later) {
		/* Writing of this segment is postponed until after the next buffer
		   read. */
		b->filled -= b->seglen;
		b->ho_written = FALSE;
	}
	else
		b->ho_written = TRUE;
}


/* Remove the segment under investigation from the buffer. */
void eat_segment(Connection* b) {

	g_return_if_fail(b != NULL);

	char* start;
	char* end;
	size_t length;

	start = b->buf + b->pos;
	end = b->buf + b->pos + b->seglen + 1;
	length = b->filled - (b->pos + b->seglen + 1);

	memmove(start, end, length);

	b->filled -= b->seglen + 1;
	b->seglen = 0;
	/* b->pos is in the right spot because of the eat. */
}


/* Skip the current segment. */
void pass_segment(Connection* b) {

	g_return_if_fail(b != NULL);

	b->pos += b->seglen + 1;
	b->seglen = 0;
}


/* Read in from the connection. */
gboolean connection_read(Connection* cnct) {

	g_return_val_if_fail(cnct != NULL, FALSE);
	g_return_val_if_fail(cnct->filled < cnct->size, FALSE);

	gssize nread;
	gboolean ok = TRUE;

	switch (hardened_read(cnct->fd_in, cnct->buf + cnct->filled,
				cnct->size - cnct->filled, &nread)) {
		case IOR_OK:
			cnct->filled += nread;
			break;

		case IOR_ERROR:
			if (errno == EIO) {
				/* We often get an EIO here when the user shell exits.  For
				   now we pretend it's a gentle error.  FIXME, though, as
				   there's gotta be a side effect. */
				action_queue(A_EXIT);
			}
			else {
				g_critical("Read problem from %s: %s", cnct->name,
						g_strerror(errno));
				ok = FALSE;
			}
			break;

		case IOR_EOF:
			action_queue(A_EXIT);
			break;

		default:
			g_return_val_if_reached(FALSE);
	}

	return ok;
}


/* Write out the full (filled) buffer. */
gboolean connection_write(Connection* cnct) {

	g_return_val_if_fail(cnct != NULL, FALSE);

	gboolean ok = TRUE;

	if (cnct->filled) { 
		switch (write_all(cnct->fd_out, cnct->buf + cnct->skip,
					cnct->filled - cnct->skip)) {

			case IOR_OK:
				break;

			case IOR_ERROR:
				g_critical("Problem writing for %s: %s", cnct->name,
						g_strerror(errno));
				ok = FALSE;
				break;

			default:
				g_return_val_if_reached(FALSE);
		}
	}

	return ok;
}

