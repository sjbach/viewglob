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
#include "vgseer.h"
#include "sequences.h"
#include "actions.h"
#include "cmdline.h"

#include <string.h>

extern struct user_shell u;

static gboolean cmd_alloc(struct cmdline* cmd);

/* Initialize working command line and sequence buffer. */
gboolean cmd_init(struct cmdline* cmd) {

	/* Initialize u.cmd */
	cmd->length = 0;
	cmd->pos = 0;
	return cmd_alloc(cmd);
}

/* Allocate or reallocate space for u.cmd.command. */
static gboolean cmd_alloc(struct cmdline* cmd) {
	static gint step = 0;
	step++;

	/* Allocate */
	if (step == 1)
		cmd->command = g_new(gchar, CMD_STEP_SIZE);
	else
		cmd->command = g_renew(gchar, cmd->command, step * CMD_STEP_SIZE);

	/* Set new memory to \0 */
	memset(cmd->command + (step - 1) * CMD_STEP_SIZE, '\0', CMD_STEP_SIZE);
	return TRUE;
}


/* Start over from scratch (usually after a command has been executed). */
gboolean cmd_clear(struct cmdline* cmd) {
	memset(cmd->command, '\0', cmd->length);
	cmd->pos = 0;
	cmd->length = 0;
	return TRUE;
}


/* Determine whether there is whitespace to the left of the cursor. */
gboolean cmd_whitespace_to_left(struct cmdline* cmd, gchar* holdover) {
	gboolean result;

	if ( (cmd->pos == 0) ||
	     /* Kludge: if the shell buffer has a holdover, which consists
			only of a space, it's reasonable to assume it won't
			complete a sequence and will end up being added to the
			command line. */
	     (holdover && strlen(holdover) == 1 && *(holdover) == ' ') )
		result = TRUE;
	else {
		switch ( *(cmd->command + cmd->pos - 1) ) {
			case ' ':
			case '\n':
			case '\t':
				result = TRUE;
				break;
			default:
				result = FALSE;
				break;
		}
	}

	return result;
}


/* Determine whether there is whitespace to the right of the cursor
   (i.e. underneath the cursor). */
gboolean cmd_whitespace_to_right(struct cmdline* cmd) {
	gboolean result;

	switch ( *(cmd->command + cmd->pos) ) {
		case ' ':
		case '\n':
		case '\t':
			result = TRUE;
			break;
		default:
			result = FALSE;
			break;
	}

	return result;
}


/* Overwrite the char in the working command line at pos in command;
   realloc if necessary. */
gboolean cmd_overwrite_char(struct cmdline* cmd, gchar c, gboolean preserve_cret) {

	while ( preserve_cret && *(cmd->command + cmd->pos) == '\015' ) {
		/* Preserve ^Ms. */
		cmd->pos++;
	}

	DEBUG((df, "overwriting \'%c\'\n", c));

	*(cmd->command + cmd->pos) = c;
	if (cmd->pos == cmd->length)
		cmd->length++;
	cmd->pos++;

	if (cmd->length % CMD_STEP_SIZE == 0) {
		if (!cmd_alloc(cmd)) {
			g_critical("Could not allocate memory for cmd");
			return FALSE;
		}
	}

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Remove n chars from the working command line at u.cmd.pos. */
gboolean cmd_del_chars(struct cmdline* cmd, gint n) {

	if (cmd->pos + n > cmd->length) {
		/* We've failed to keep up. */
		action_queue(A_SEND_LOST);
		return FALSE;
	}

	memmove(cmd->command + cmd->pos, cmd->command + cmd->pos + n, cmd->length - (cmd->pos + n));
	memset(cmd->command + cmd->length - n, '\0', n);
	cmd->length -= n;

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Trash everything. */
gboolean cmd_wipe_in_line(struct cmdline* cmd, enum direction dir) {
	gint cret_pos_l, cret_pos_r;

	switch (dir) {
		case D_RIGHT:	/* Clear to right (in this line) */
			DEBUG((df, "(right)\n"));
			cret_pos_r = find_next_cret(cmd->command, cmd->length, cmd->pos);
			if (cret_pos_r == -1) {
				/* Erase everything to the right -- no ^Ms to take into account. */
				memset(cmd->command + cmd->pos, '\0', cmd->length - cmd->pos);
				cmd->length = cmd->pos;
			}
			else {
				/* Erase to the right up to the first ^M. */
				cmd_del_chars(cmd, cret_pos_r - cmd->pos);

				/* If we were at pos 0, this is the new pos 0; delete the cret. */
				if (!cmd->pos)
					cmd_del_chars(cmd, 1);
			}
			break;
		case D_LEFT:	/* Clear to left -- I've never seen this happen. */
			DEBUG((df, "D_LEFT seen in cmd_wipe_in_line\n"));
			break;
		case D_ALL:	/* Clear all (in this line). */
			DEBUG((df, "(all)\n"));

			/* Find the ^M to the right. */
			cret_pos_r = find_next_cret(cmd->command, cmd->length, cmd->pos);
			if (cret_pos_r == -1)
				cret_pos_r = cmd->length;
			
			/* Find the ^M to the left. */
			cret_pos_l = find_prev_cret(cmd->command, cmd->pos);
			if (cret_pos_l == -1)
				cret_pos_l = 0;

			/* Delete everything in-between. */
			cmd->pos = cret_pos_l;
			cmd_del_chars(cmd, cret_pos_r - cret_pos_l);

			break;
		default:
			g_return_val_if_reached(FALSE);
			break;
	}
	action_queue(A_SEND_CMD);
	return TRUE;
}


gboolean cmd_insert_chars(struct cmdline* cmd, gchar c, gint n) {

	if (n < 0) {
		g_warning("<0 in cmd_insert_chars");
		action_queue(A_SEND_LOST);
		return FALSE;
	}

	memmove(cmd->command + cmd->pos + n, cmd->command + cmd->pos, cmd->length - cmd->pos);
	memset(cmd->command + cmd->pos, c, n);
	cmd->length += n;

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Remove trailing ^Ms.
   These have a tendency to collect after a lot of modifications in a command
   line.  They're mostly harmless, and this is treating the symptom rather
   than the sickness, but it seems to work all right. */
void cmd_del_trailing_crets(struct cmdline* cmd) {
	gint temp;
	while (cmd->command[cmd->length - 1] == '\015' && cmd->pos < cmd->length - 1) {
		temp = cmd->pos;
		cmd->pos = cmd->length - 1;
		cmd_del_chars(cmd, 1);
		cmd->pos = temp;
	}
}

