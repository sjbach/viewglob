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

#include <string.h>

static gchar* connection_types[CT_COUNT] = {
	"user shell",
	"sandbox shell",
	"terminal",
	"vgd",
};

/* Initialize the given Connection. */
void connection_init(Connection* cnct, enum connection_type type, int fd_in,
		int fd_out, gchar* buf, gsize buflen, enum process_level pl) {

	g_return_if_fail(cnct != NULL);
	g_return_if_fail(type >= 0 && type < CT_COUNT);
	g_return_if_fail(fd_in >= 0);
	g_return_if_fail(fd_out >= 0);
	g_return_if_fail(buf != NULL);

	cnct->type = type;
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
}


/* Free the read/write and holdover buffers. */
void connection_free(Connection* cnct) {

	g_return_if_fail(cnct != NULL);

	if (cnct->holdover) {
		g_free(cnct->holdover);
		cnct->holdover = NULL;
	}
}


gchar* connection_type_str(enum connection_type type) {

	g_return_val_if_fail(type >= 0 && type < CT_COUNT, "(error)");

	return connection_types[type];
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


/* Cut the segment under investigation into a holdover, to be attached to
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

