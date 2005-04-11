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

#ifndef CONNECTION_H
#define CONNECTION_H

#include "shell.h"

G_BEGIN_DECLS

/* Status of a match attempt. */
typedef enum _MatchStatus MatchStatus;
enum _MatchStatus {
	MS_NO_MATCH     = 1 << 0,
	MS_IN_PROGRESS  = 1 << 1,
	MS_MATCH        = 1 << 2,
};


enum process_level {
	PL_TERMINAL,     /* For the terminal. */
	PL_AT_PROMPT,    /* When user is typing away. */
	PL_EXECUTING,    /* When a command is executing. */
	PL_AT_RPROMPT,   /* When the zsh RPROMPT is being printed. */
	PL_VIEWGLOB,     /* User typed Ctrl-G. */
	PL_COUNT,
};


typedef struct _Connection Connection;
struct _Connection {
	gchar* name;
	gint fd_in;
	gint fd_out;
	gchar* buf;             /* Read/write buffer. */
	gsize size;
	gsize filled;           /* The amount of the buffer that is actually
	                           filled. */
	gsize pos;              /* The offset of the segment being examined. */
	gsize seglen;           /* Length of the examined segment. */
	enum process_level pl;  /* Processing level of the buffer. */
	MatchStatus status;     /* Result of the last check_seqs() attempt. */
	gchar* holdover;        /* Segment leftover from the last read. */
	gboolean ho_written;    /* The holdover was written already (and thus
	                           should be skipped. */
	gsize skip;             /* Number of bytes to skip writing (already
	                           written). */
	enum shell_status ss;
};


void connection_init(Connection* cnct, gchar* name, gint fd_in, gint fd_out,
		gchar* buf, gsize buflen, enum process_level pl);
void connection_free(Connection* cnct);
void prepend_holdover(Connection* b);
void create_holdover(Connection* b, gboolean write_later);
void eat_segment(Connection* b);
void pass_segment(Connection* b);
gboolean connection_read(Connection* cnct);
gboolean connection_write(Connection* cnct);


G_END_DECLS

#endif /* !CONNECTION_H */

