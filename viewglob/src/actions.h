/*
	Copyright (C) 2004 Stephen Bach
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

#ifndef ACTIONS_H
#define ACTIONS_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

BEGIN_C_DECLS

typedef enum _Action Action;
enum _Action {
	A_NOP,         /* Do nothing. */
	A_SEND_CMD,    /* Send the shell's current command line. */
	A_SEND_PWD,    /* Send the shell's current pwd. */
	A_SEND_LOST,   /* Tell the display we're lost for now. */
	A_SEND_UP,     /* Tell display to move up one line. */
	A_SEND_DOWN,   /* Tell display to move down one line. */
	A_SEND_PGUP,   /* Tell display to move up one page. */
	A_SEND_PGDOWN, /* Tell display to move down one page. */
	A_TOGGLE,      /* Enable/disable the display. */
	A_DISABLE,     /* Disable viewglob. */
	A_POP,         /* Pop off the top queued action. */
	A_DONE,        /* Nothing more to pop. */
	A_EXIT,        /* Shell closed -- finish execution. */
};

/* Prototypes */
Action action_queue(Action o);

END_C_DECLS

#endif /* !ACTIONS_H */
