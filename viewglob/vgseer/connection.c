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


/* Initialize the given Connection. */
void connection_init(Connection* cnct, char* name, int fd_in, int fd_out,
		size_t buflen, enum process_level pl) {

	g_return_if_fail(cnct != NULL);

	cnct->name = name;
	cnct->fd_in = fd_in;
	cnct->fd_out = fd_out;
	cnct->buf = g_malloc(buflen);
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

	if (cnct->buf) {
		g_free(cnct->buf);
		cnct->buf = NULL;
	}

	if (cnct->holdover) {
		g_free(cnct->holdover);
		cnct->holdover = NULL;
	}
}


/* Prepend the holdover from the last read onto the beginning
   of the current buffer. */
void prepend_holdover(Connection* b) {

	size_t ho_len;

	g_return_if_fail(b != NULL);
	g_return_if_fail(b->holdover != NULL);

	ho_len = strlen(b->holdover);

	/* If needed, grow the buffer. */
	if (b->filled + ho_len > b->size) {
		DEBUG((df, "reallocing %d bytes more\n", ho_len));
		b->buf = g_realloc(b->buf, b->size + ho_len);
		b->size += ho_len;
	}

	/* Make room at the beginning of the buffer. */
	memmove(b->buf + ho_len, b->buf, b->filled);

	DEBUG((df, "prepending \"%s\"\n", b->holdover));

	/* Copy over the holdover and then free it. */
	memcpy(b->buf, b->holdover, ho_len);
	g_free(b->holdover);
	b->holdover = NULL;

	b->filled += ho_len;
	b->pos = 0;
	/* b->seglen has not changed since the create_holdover(). */

	if (b->ho_written) {
		DEBUG((df, "skipping write of %d chars (first char: \'%c\'\n", ho_len, *(b->buf + ho_len)));
		b->skip = ho_len;
	}
	else
		b->skip = 0;

	#if DEBUG_ON
		size_t x;
		DEBUG((df, "===============\n"));
		for (x = 0; x < b->filled; x++)
			DEBUG((df, "%c", b->buf[x]));
		DEBUG((df, "\n===============\n"));
	#endif
}


/* Cut the segment under investigation into a holdover, to be attached to
   a later buffer. */
void create_holdover(Connection* b, gboolean write_later) {

	g_return_if_fail(b != NULL);
	g_return_if_fail(b->holdover == NULL);

	/* Copy as null-terminated string. */
	b->holdover = g_strndup(b->buf + b->pos, b->seglen);

	DEBUG((df, "copied \"%s\"\n", b->holdover));
	DEBUG((df, "b->seglen: %d\n", b->seglen));

	if (write_later) {
		/* Writing of this segment is postponed until after the next buffer read. */
		DEBUG((df, "writing this holdover later.\n"));
		b->filled -= b->seglen;
		b->ho_written = FALSE;
	}
	else {
		DEBUG((df, "writing this holdover now.\n"));
		b->ho_written = TRUE;
	}

	#if DEBUG_ON
		size_t x;
		DEBUG((df, "===============\n"));
		for (x = 0; x < b->filled; x++)
			DEBUG((df, "%c", b->buf[x]));
		DEBUG((df, "\n===============\n"));
	#endif
}


/* Remove the segment under investigation from the buffer. */
void eat_segment(Connection* b) {

	char* start;
	char* end;
	size_t length;

	g_return_if_fail(b != NULL);

	start = b->buf + b->pos;
	end = b->buf + b->pos + b->seglen + 1;
	length = b->filled - (b->pos + b->seglen + 1);

	DEBUG((df, "moving %d bytes\n", length));
	memmove(start, end, length);

	DEBUG((df, "filed before: %d, filled after: %d\n", b->filled, b->filled - b->seglen + 1));

	b->filled -= b->seglen + 1;
	b->seglen = 0;
	/* b->pos is in the right spot because of the eat. */
}


/* Skip the current segment. */
void pass_segment(Connection* b) {

	g_return_if_fail(b != NULL);

	DEBUG((df, "passing %d bytes\n", b->seglen));
	b->pos += b->seglen + 1;
	b->seglen = 0;
}

