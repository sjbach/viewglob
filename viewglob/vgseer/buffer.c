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

#include "config.h"

#include "common.h"
#include "viewglob-error.h"
#include "buffer.h"

#include <string.h>


void prepend_holdover(Buffer* b) {

	size_t ho_len;

	if (!b->holdover) {
		viewglob_warning("b->holdover == NULL");
		return;
	}

	ho_len = strlen(b->holdover);

	/* If needed, grow the buffer. */
	if (b->filled + ho_len > b->size) {
		DEBUG((df, "reallocing %d bytes more\n", ho_len));
		b->buf = g_renew(char, b->buf, b->size + ho_len);
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
	/* b->n has not changed since the create_holdover(). */

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


void create_holdover(Buffer* b, gboolean write_later) {

	if (!b->n) {
		viewglob_warning("b->n == 0");
		return;
	}
	else if (b->holdover) {
		viewglob_warning("b->holdover != NULL");
		return;
	}

	/* Copy and null-terminate. */
	b->holdover = g_new(char, b->n);
	strncpy(b->holdover, b->buf + b->pos, (b->n - 1));
	*(b->holdover + (b->n - 1)) = '\0';

	DEBUG((df, "copied \"%s\"\n", b->holdover));
	DEBUG((df, "b->n: %d\n", b->n));

	if (write_later) {
		/* Writing of this segment is postponed until after the next buffer read. */
		DEBUG((df, "writing this holdover later.\n"));
		b->filled -= (b->n - 1);
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


void eat_segment(Buffer* b) {

	char* start;
	char* end;
	size_t length;

	if (b->n <= 0) {
		viewglob_warning("b->n <= 0");
		return;
	}

	start = b->buf + b->pos;
	end = b->buf + b->pos + b->n;
	length = b->filled - (b->pos + b->n);

	DEBUG((df, "moving %d bytes\n", length));
	memmove(start, end, length);

	DEBUG((df, "filed before: %d, filled after: %d\n", b->filled, b->filled - b->n));

	b->filled -= b->n;
	b->n = 1;
	/* b->pos is in the right spot because of the eat. */
}


void pass_segment(Buffer* b) {
	DEBUG((df, "passing %d bytes\n", b->n - 1));
	b->pos += b->n;
	b->n = 1;
}

