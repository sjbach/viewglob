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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "seer.h"
#include "sequences.h"
#include "cmdline.h"

#if DEBUG_ON
extern FILE *df;
#endif

extern struct user_shell u;

/* Kludge variable. */
int chars_to_save = 0;

/* Initialize working command line and sequence buffer. */
bool cmd_init(void) {

	/* Initialize u.seqbuff */
	u.seqbuff.pos = 0;
	memset(u.seqbuff.string, '\0', SEMBUFF_STRING_SIZE);

	/* Initialize u.cmd */
	u.cmd.length = 0;
	u.cmd.pos = 0;
	return cmd_alloc();
}

/* Allocate or reallocate space for u.cmd.command. */
bool cmd_alloc(void) {
	static int step = 0;
	step++;

	/* Allocate */
	if (step == 1)
		u.cmd.command = XMALLOC(char, CMD_STEP_SIZE);
	else
		u.cmd.command = XREALLOC(char, u.cmd.command, step * CMD_STEP_SIZE);

	/* Set new memory to \0 */
	memset(u.cmd.command + (step - 1) * CMD_STEP_SIZE, '\0', CMD_STEP_SIZE * sizeof(char));
	return true;
}


/* Start over from scratch (usually after a command has been executed). */
bool cmd_clear(void) {
	memset(u.cmd.command, '\0', u.cmd.length);
	u.cmd.pos = 0;
	u.cmd.length = 0;
	return true;
}

bool seqbuff_clear(void) {
	return seqbuff_dequeue(u.seqbuff.pos, false);
}


bool seqbuff_enqueue(char c) {
	u.seqbuff.string[u.seqbuff.pos] = c;
	u.seqbuff.pos++;
	if (u.seqbuff.pos >= SEMBUFF_STRING_SIZE) {
		viewglob_error("End of u.seqbuff reached");
		return false;
	}
	return true;
}


/* Removes n chars from the beginning of seqbuff.
   If add_to_cmd is true, the removed chars are added to u.cmd
   at u.cmd.pos (overwriting, not inserting). */
bool seqbuff_dequeue(int n, bool add_to_cmd) {
	int i;

	if (n > u.seqbuff.pos)
		return false;

	if (add_to_cmd) {
		action_queue(A_SEND_CMD);
		for (i = 0; i < n; i++)
			if (!cmd_overwrite_char(u.seqbuff.string[i], true)) {
				viewglob_error("Could not overwrite char in u.cmd");
				return false;
			}
	}

	/* Overwrite the first n characters, and clear the duplicated characters
	   at the end. */
	n -= chars_to_save;   /* Kludge. */
	memmove(u.seqbuff.string, u.seqbuff.string + n, u.seqbuff.pos - n);
	memset(u.seqbuff.string + u.seqbuff.pos - n, '\0', n);
	u.seqbuff.pos -= n;
	chars_to_save = 0;
	return true;
}


/* Overwrite the char in the working command line at pos in command;
   realloc if necessary. */
bool cmd_overwrite_char(char c, bool preserve_cret) {

	while ( preserve_cret && *(u.cmd.command + u.cmd.pos) == '\015' ) {
		/* Preserve ^Ms. */
		u.cmd.pos++;
	}

	*(u.cmd.command + u.cmd.pos) = c;
	if (u.cmd.pos == u.cmd.length)
		u.cmd.length++;
	u.cmd.pos++;

	if (u.cmd.length % CMD_STEP_SIZE == 0) {
		if (!cmd_alloc()) {
			viewglob_error("Could not allocate space for cmd");
			return false;
		}
	}

	action_queue(A_SEND_CMD);
	return true;
}


/* Remove n chars from the working command line at u.cmd.pos. */
bool cmd_del_chars(int n) {
	memmove(u.cmd.command + u.cmd.pos, u.cmd.command + u.cmd.pos + n, u.cmd.length - (u.cmd.pos + n));
	memset(u.cmd.command + u.cmd.length - n, '\0', n);
	u.cmd.length -= n;

	action_queue(A_SEND_CMD);
	return true;
}


/* Trash everything. */
bool cmd_wipe_in_line(enum direction dir) {
	int cret_pos_l, cret_pos_r;

	switch (dir) {
		case D_RIGHT:	/* Clear to right (in this line) */
			cret_pos_r = find_next_cret(u.cmd.pos);
			if (cret_pos_r == -1) {
				/* Erase everything to the right -- no ^Ms to take into account. */
				memset(u.cmd.command + u.cmd.pos, '\0', u.cmd.length - u.cmd.pos);
				u.cmd.length = u.cmd.pos;
			}
			else {
				/* Erase to the right up to the first ^M. */
				cmd_del_chars(cret_pos_r - u.cmd.pos);

				/* If we were at pos 0, this is the new pos 0; delete the cret. */
				if (u.cmd.pos = 0)
					cmd_del_chars(1);
			}


			break;
		case D_LEFT:	/* Clear to left -- I've never seen this happen. */
			DEBUG((df, "D_LEFT seen in cmd_wipe_in_line\n"));
			break;
		case D_ALL:	/* Clear all (in this line). */

			/* Find the ^M to the right. */
			cret_pos_r = find_next_cret(u.cmd.pos);
			if (cret_pos_r == -1)
				cret_pos_r = u.cmd.length;
			
			/* Find the ^M to the left. */
			cret_pos_l = find_prev_cret(u.cmd.pos);
			if (cret_pos_l == -1)
				cret_pos_l = 0;

			/* Delete everything in-between. */
			u.cmd.pos = cret_pos_l;
			cmd_del_chars(cret_pos_r - cret_pos_l);

			break;
	}
	action_queue(A_SEND_CMD);
	return true;
}


bool cmd_backspace(int n) {
	int i;
	for (i = 0; i < n; i++) {
		if (u.cmd.pos > 0)
			u.cmd.pos--;
		else {
			viewglob_error("Backspaced out of command line");
			return false;
		}
	}
	return true;
}


bool cmd_insert_chars(char c, int n) {
	memmove(u.cmd.command + u.cmd.pos + n, u.cmd.command + u.cmd.pos, u.cmd.length - u.cmd.pos);
	memset(u.cmd.command + u.cmd.pos, c, n);
	u.cmd.length += n;

	action_queue(A_SEND_CMD);
	return true;
}


/* Remove trailing ^Ms.
   These have a tendency to collect after a lot of modifications in a command line.
   They're mostly harmless, and this is treating the symptom rather than the sickness,
   but it seems to work all right. */
void cmd_del_trailing_crets(void) {
	int temp;
	while (u.cmd.command[u.cmd.length - 1] == '\015' && u.cmd.pos < u.cmd.length - 1) {
		temp = u.cmd.pos;
		u.cmd.pos = u.cmd.length - 1;
		cmd_del_chars(1);
		u.cmd.pos = temp;
	}
}

