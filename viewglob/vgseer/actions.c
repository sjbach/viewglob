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
#include "actions.h"

struct action_list {
	Action a;
	struct action_list* next;
}* al = NULL;


/* Prototypes. */
static void    al_enqueue(Action a);
static Action  al_dequeue(void);


/* This is kind-of a queue.  If o is A_SEND_CMD, A_SEND_PWD, or A_EXIT,
   the action is queued.  If o is A_DEQUEUE, then the correct queued action
   is dequeued.  Note that this doesn't follow first in, first out,
   since A_EXIT is a much more important action, so it gets dequeued
   first.  Also, a new A_SEND_PWD invalidates a previous A_SEND_CMD.
   A_SEND_CMD invalidates A_SEND_LOST, and A_SEND_LOST invalidates
   A_SEND_CMD.  However, the A_SEND_UP, A_SEND_DOWN, A_SEND_PGUP, and
   A_SEND_PGDOWN actions are queued and dequeued as in a real queue. */
Action action_queue(Action o) {

	Action result = A_NOP;

	/* Don't need a queue for these. */
	static gboolean send_lost = FALSE;
	static gboolean send_cmd = FALSE;
	static gboolean send_pwd = FALSE;
	static gboolean disable = FALSE;
	static gboolean do_exit = FALSE;

	static gboolean new_mask = FALSE;

	//FIXME replace with glib queue

	switch (o) {

		case A_SEND_CMD:
			send_lost = FALSE;
			new_mask = FALSE;
			send_cmd = TRUE;
			break;

		case A_SEND_PWD:
			send_cmd = FALSE;
			send_pwd = TRUE;
			break;

		case A_SEND_LOST:
			send_cmd = FALSE;
			send_lost = TRUE;
			break;

		case A_NEW_MASK:
			new_mask = TRUE;
			break;

		case A_SEND_UP:
		case A_SEND_DOWN:
		case A_SEND_PGUP:
		case A_SEND_PGDOWN:
		case A_TOGGLE:
		case A_REFOCUS:
			al_enqueue(o);
			break;

		case A_DISABLE:
			disable = TRUE;
			break;

		case A_EXIT:
			do_exit = TRUE;
			break;

		case A_DEQUEUE:
			if (do_exit)
				result = A_EXIT;
			else if (disable) {
				disable = FALSE;
				result = A_DISABLE;
			}
			else if (send_lost) {
				send_lost = FALSE;
				result = A_SEND_LOST;
			}
			else if (send_pwd) {
				send_pwd = FALSE;
				result = A_SEND_PWD;
			}
			else if (send_cmd) {
				send_cmd = FALSE;
				result = A_SEND_CMD;
			}
			else if (new_mask) {
				new_mask = FALSE;
				result = A_NEW_MASK;
			}
			else
				result = al_dequeue();
			break;

		default:
			break;
	}

	return result;
}


static void al_enqueue(Action a) {
	struct action_list* new_al;
	struct action_list* iter;

	new_al = g_new(struct action_list, 1);
	new_al->a = a;
	new_al->next = NULL;

	if (al) {
		/* Add it to the end. */
		iter = al;
		while (iter->next)
			iter = iter->next;
		iter->next = new_al;
	}
	else
		al = new_al;
}


static Action al_dequeue(void) {
	Action a;

	if (al) {
		struct action_list* tmp;
		a = al->a;
		tmp = al;
		al = al->next;
		g_free(tmp);
	}
	else
		a = A_DONE;

	return a;
}

