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

#include "config.h"

#include "common.h"
#include "viewglob-error.h"
#include "children.h"
#include "actions.h"

struct action_list {
	Action a;
	struct action_list* next;
};

static void    al_push(Action a);
static Action  al_pop(void);


#if DEBUG_ON
extern FILE* df;
#endif


static struct action_list* al = NULL;


/* This is kind-of a queue.  If o is A_SEND_CMD, A_SEND_PWD, or A_EXIT,
   the action is queued.  If o is A_POP, then the correct queued action
   is dequeued.  Note that this doesn't follow first in, first out,
   since A_EXIT is a much more important action, so it gets dequeued
   first.  Also, a new A_SEND_PWD invalidates a previous A_SEND_CMD.
   A_SEND_CMD invalidates A_SEND_LOST, and A_SEND_LOST invalidates
   A_SEND_CMD.  However, the A_SEND_UP, A_SEND_DOWN, A_SEND_PGUP, and
   A_SEND_PGDOWN actions are queued and dequeued as in a real queue. */
Action action_queue(Action o) {

	Action result = A_NOP;

	/* Don't need a queue for these. */
	static bool send_lost = false;
	static bool send_cmd = false;
	static bool send_pwd = false;
	static bool do_exit = false;

	switch (o) {

		case (A_SEND_CMD):
			send_lost = false;
			send_cmd = true;
			break;

		case (A_SEND_PWD):
			send_cmd = false;
			send_pwd = true;
			break;

		case (A_SEND_LOST):
			send_cmd = false;
			send_lost = true;
			break;

		case (A_SEND_UP):
		case (A_SEND_DOWN):
		case (A_SEND_PGUP):
		case (A_SEND_PGDOWN):
			al_push(o);
			break;

		case (A_EXIT):
			do_exit = true;
			break;

		case (A_POP):
			if (do_exit)
				result = A_EXIT;
			else if (send_lost) {
				send_lost = false;
				result = A_SEND_LOST;
			}
			else if (send_pwd) {
				send_pwd = false;
				result = A_SEND_PWD;
			}
			else if (send_cmd) {
				send_cmd = false;
				result = A_SEND_CMD;
			}
			else
				result = al_pop();
			break;

		default:
			break;
	}

	return result;
}


static void al_push(Action a) {
	struct action_list* new_al;
	struct action_list* iter;

	new_al = XMALLOC(struct action_list, 1);
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


static Action al_pop(void) {
	Action a;

	if (al) {
		struct action_list* tmp;
		a = al->a;
		tmp = al;
		al = al->next;
		XFREE(tmp);
	}
	else
		a = A_DONE;

	return a;
}

