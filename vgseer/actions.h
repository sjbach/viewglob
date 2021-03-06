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

#ifndef ACTIONS_H
#define ACTIONS_H

G_BEGIN_DECLS

typedef enum _Action Action;
enum _Action {
	A_NOP,           /* Do nothing. */
	A_SEND_CMD,      /* Send the shell's current command line. */
	A_SEND_PWD,      /* Send the shell's current pwd. */
	A_SEND_LOST,     /* Tell the display we're lost for now. */
	A_SEND_UP,       /* Tell display to move up one line. */
	A_SEND_DOWN,     /* Tell display to move down one line. */
	A_SEND_PGUP,     /* Tell display to move up one page. */
	A_SEND_PGDOWN,   /* Tell display to move down one page. */
	A_NEW_MASK,      /* A new mask as it's being typed. */
	A_TOGGLE,        /* Enable/disable the display. */
	A_REFOCUS,       /* Bring the focus back to the terminal and display. */
	A_DISABLE,       /* Disable viewglob. */
	A_DEQUEUE,       /* Dequeue and return the top queued action. */
	A_DONE,          /* Nothing more to pop. */
	A_EXIT,          /* Shell closed -- finish execution. */
};

/* Prototypes */
Action action_queue(Action o);

G_END_DECLS

#endif /* !ACTIONS_H */
