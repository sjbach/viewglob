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

#ifndef CIRCULAR_H
#define CIRCULAR_H

/* This file only exists because of the circular dependencies between
   sequences.h and buffer.h. */

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "vgseer-common.h"

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
};


typedef struct _Buffer Buffer;
struct _Buffer {
//	char* name;
//	int fd_in;
//	int fd_out;
	char* buf;
	size_t size;            /* malloc'd size. */
	size_t filled;          /* The amount of the buffer that is actually filled. */
	size_t pos;             /* The offset of the segment being examined. */
	size_t n;               /* Length of the examined segment. */
	enum process_level pl;
	MatchStatus status;     /* Result of the last check_seqs() attempt. */
	char* holdover;         /* Segment leftover from the last read. */
	gboolean ho_written;        /* The holdover was written already (and thus should be skipped. */
	size_t skip;            /* Number of bytes to skip writing (already written). */
};


G_END_DECLS

#endif /* !CIRCULAR_H */
