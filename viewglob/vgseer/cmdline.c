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

#include "config.h"

#include "common.h"
#include "sequences.h"
#include "actions.h"
#include "cmdline.h"

#include <string.h>
#include <ctype.h>

/* Initialize working command line and sequence buffer. */
void cmd_init(struct cmdline* cmd) {
	cmd->data = g_string_sized_new(256);
	cmd->pos = 0;
	cmd->rebuilding = FALSE;
	cmd->expect_newline = FALSE;

	cmd->pwd = NULL;
	cmd->mask = g_string_new(NULL);
	cmd->mask_final = g_string_new(NULL);
}


void cmd_free(struct cmdline* cmd) {
	g_string_free(cmd->data, TRUE);
}


/* Start over from scratch (usually after a command has been executed). */
gboolean cmd_clear(struct cmdline* cmd) {

	cmd->data = g_string_truncate(cmd->data, 0);
	cmd->pos = 0;
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
		switch ( *(cmd->data->str + cmd->pos - 1) ) {
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


//FIXME test whitespace_to_left, whitespace_to_right functions
/* Determine whether there is whitespace to the right of the cursor
   (i.e. underneath the cursor). */
gboolean cmd_whitespace_to_right(struct cmdline* cmd) {
	gboolean result;

	switch ( *(cmd->data->str + cmd->pos) ) {
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
gboolean cmd_overwrite_char(struct cmdline* cmd, gchar c,
		gboolean preserve_CR) {

	while (preserve_CR && *(cmd->data->str + cmd->pos) == '\015') {
		/* Preserve ^Ms. */
		cmd->pos++;
	}

	if (cmd->pos == cmd->data->len)
		cmd->data = g_string_append_c(cmd->data, c);
	else
		*(cmd->data->str + cmd->pos) = c;
	cmd->pos++;

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Remove n chars from the working command line at cmd->pos. */
gboolean cmd_del_chars(struct cmdline* cmd, gint n) {

	g_return_val_if_fail(n >= 0, FALSE);

	if (cmd->pos + n > cmd->data->len) {
		/* We've failed to keep up. */
		action_queue(A_SEND_LOST);
		return FALSE;
	}

	cmd->data = g_string_erase(cmd->data, cmd->pos, n);

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Trash everything. */
gboolean cmd_wipe_in_line(struct cmdline* cmd, enum direction dir) {
	gchar* CR_l;
	gchar* CR_r;

	switch (dir) {

		case D_RIGHT:	/* Clear to right (in this line) */
			CR_r = g_strstr_len(
					cmd->data->str + cmd->pos,
					cmd->data->len - cmd->pos,
					"\015");
			if (CR_r == NULL) {
				/* Erase everything to the right -- no ^Ms to take into
				   account. */
				cmd->data = g_string_truncate(cmd->data, cmd->pos);
			}
			else {
				/* Erase to the right up to the first ^M. */
				cmd_del_chars(cmd, (CR_r - cmd->data->str) - cmd->pos);

				/* If we were at pos 0, this is the new pos 0; delete the
				   CR. */
				if (cmd->pos == 0)
					cmd_del_chars(cmd, 1);
			}
			break;

		case D_LEFT:	/* Clear to left -- I've never seen this happen. */
			g_return_val_if_reached(FALSE);
			break;

		case D_ALL:	/* Clear all (in this line). */

			/* Find the ^M to the right. */
			CR_r = g_strstr_len(
					cmd->data->str + cmd->pos,
					cmd->data->len - cmd->pos,
					"\015");
			if (CR_r == NULL)
				CR_r = cmd->data->str + cmd->data->len;
			
			/* Find the ^M to the left. */
			CR_l = g_strrstr_len(cmd->data->str, cmd->pos, "\015");
			if (CR_l == NULL)
				CR_l = cmd->data->str;

			/* Delete everything in-between. */
			cmd->pos = CR_l - cmd->data->str;
			cmd_del_chars(cmd, CR_r - CR_l);

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

	int i;
	for (i = 0; i < n; i++)
		cmd->data = g_string_insert_c(cmd->data, cmd->pos, c);

	action_queue(A_SEND_CMD);
	return TRUE;
}


//FIXME modify function to remove collected CRs? eg. abc^M^M^Mdef --> abc^Mdef
/* Remove trailing ^Ms.
   These have a tendency to collect after a lot of modifications in a command
   line.  They're mostly harmless, and this is treating the symptom rather
   than the sickness, but it seems to work all right. */
void cmd_del_trailing_CRs(struct cmdline* cmd) {
	gint temp;
	while (cmd->data->str[cmd->data->len - 1] == '\015' &&
			cmd->pos < cmd->data->len - 1) {
		temp = cmd->pos;
		cmd->pos = cmd->data->len - 1;
		cmd_del_chars(cmd, 1);
		cmd->pos = temp;
	}
}


void cmd_mask_add(struct cmdline* cmd, char c) {
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(isprint(c));

	if (cmd->mask->len < 50) {
		cmd->mask = g_string_append_c(cmd->mask, c);
		action_queue(A_NEW_MASK);
	}
}


void cmd_mask_clear(struct cmdline* cmd) {
	g_return_if_fail(cmd != NULL);

	cmd->mask = g_string_truncate(cmd->mask, 0);
	action_queue(A_NEW_MASK);
}


void cmd_mask_del(struct cmdline* cmd) {
	g_return_if_fail(cmd != NULL);

	if (cmd->mask->len > 0) {
		cmd->mask = g_string_truncate(cmd->mask, cmd->mask->len - 1);
		action_queue(A_NEW_MASK);
	}
}

