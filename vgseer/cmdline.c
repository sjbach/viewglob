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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Initialize working command line and sequence buffer. */
void cmd_init(struct cmdline* cmd) {

	gchar* lc_all;
	gchar* lang;

	cmd->data = g_string_sized_new(256);
	cmd->pos = 0;
	cmd->rebuilding = FALSE;
	cmd->expect_newline = FALSE;

	cmd->pwd = NULL;
	cmd->mask = g_string_new(NULL);
	cmd->mask_final = g_string_new(NULL);

	/* Determine whether we're in a UTF-8 locale or not (hack).
	   cmd->pos is always treated as a byte offset, so some extra finagling
	   must be done with UTF-8. */
	cmd->is_utf8 = FALSE;
	lc_all = getenv("LC_ALL");
	lang = getenv("LANG");

	if (lc_all) {
		if (strstr(lc_all, "utf8")
				|| strstr(lc_all, "UTF8")
				|| strstr(lc_all, "UTF-8")
				|| strstr(lc_all, "utf-8")) {
			cmd->is_utf8 = TRUE;
		}
	}

	if (!cmd->is_utf8 && lang) {
		if (strstr(lang, "utf8")
				|| strstr(lang, "UTF8")
				|| strstr(lang, "UTF-8")
				|| strstr(lang, "utf-8")) {
			cmd->is_utf8 = TRUE;
		}
	}
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
		if (cmd->is_utf8) {
			gchar* previous = g_utf8_find_prev_char(
					cmd->data->str, cmd->data->str + cmd->pos);

			result = previous && g_unichar_isspace(g_utf8_get_char(previous));
		}
		else
			result = isspace( *(cmd->data->str + cmd->pos - 1) );
	}

	return result;
}


//FIXME test whitespace_to_left, whitespace_to_right functions
/* Determine whether there is whitespace to the right of the cursor
   (i.e. underneath the cursor). */
gboolean cmd_whitespace_to_right(struct cmdline* cmd) {
	gboolean result;

	if (cmd->is_utf8) {
		result = g_unichar_isspace(g_utf8_get_char(
					cmd->data->str + cmd->pos));
	}
	else
		result = isspace( *(cmd->data->str + cmd->pos) );

	return result;
}


/* Overwrite the char in the working command line at pos in command;
   realloc if necessary. */
gboolean cmd_overwrite_char(struct cmdline* cmd, gchar c,
		gboolean preserve_CR) {


	if (preserve_CR) {
		/* Preserve ^Ms. */
			while (*(cmd->data->str + cmd->pos) == '\015')
				cmd->pos++;
	}

	if (cmd->is_utf8) {
		/* We examine each byte individually, but UTF-8 characters can be
		   multibyte.  So conserve a string of characters until a valid
		   UTF-8 char is created. */
		static GString* utf8_char = NULL;
		if (!utf8_char)
			utf8_char = g_string_new(NULL);

		utf8_char = g_string_append_c(utf8_char, c);

		if (g_utf8_validate(utf8_char->str, utf8_char->len, NULL)) {
			/* The UTF-8 character has been completed and it's time to put it
			   onto the command line. */

			if (cmd->pos == cmd->data->len) {
				/* We're at the end - just append the "character". */
				cmd->data = g_string_append(cmd->data, utf8_char->str);
			}
			else {
				/* We need to overwrite data.  Since the characters might not
				   be the same length, erase the old one first and then insert
				   the new one. */
				gchar* next = g_utf8_next_char(cmd->data->str + cmd->pos);
				gsize erase_len = next - (cmd->data->str + cmd->pos);
				cmd->data = g_string_erase(cmd->data, cmd->pos, erase_len);
				cmd->data = g_string_insert(cmd->data, cmd->pos,
						utf8_char->str);
			}

			cmd->pos += utf8_char->len;
			g_string_truncate(utf8_char, 0);
		}

	}
	else {
		if (cmd->pos == cmd->data->len)
			cmd->data = g_string_append_c(cmd->data, c);
		else
			*(cmd->data->str + cmd->pos) = c;
		cmd->pos++;
	}

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Remove n chars from the working command line at cmd->pos. */
gboolean cmd_del_chars(struct cmdline* cmd, gint n) {

	g_return_val_if_fail(n >= 0, FALSE);


	if (cmd->is_utf8) {
		/* Erase n UTF-8 characters. */
		int i;
		for (i = 0; i < n; i++) {
			if ( *(cmd->data->str + cmd->pos) == '\0' ) {
				/* We've failed to keep up. */
				action_queue(A_SEND_LOST);
				return FALSE;
			}
			gchar* next = g_utf8_next_char(cmd->data->str + cmd->pos);
			gsize erase_len = next - (cmd->data->str + cmd->pos);
			cmd->data = g_string_erase(cmd->data, cmd->pos, erase_len);
		}
	}
	else {
		if (cmd->pos + n > cmd->data->len) {
			/* We've failed to keep up. */
			action_queue(A_SEND_LOST);
			return FALSE;
		}

		cmd->data = g_string_erase(cmd->data, cmd->pos, n);
	}

	action_queue(A_SEND_CMD);
	return TRUE;
}


/* Trash everything. */
gboolean cmd_wipe_in_line(struct cmdline* cmd, enum direction dir) {
	gchar* CR_l;
	gchar* CR_r;
	int chars;

	switch (dir) {

		case D_RIGHT:	/* Clear to right (in this line) */

			/* Find the ^M to the right. */
			if (cmd->is_utf8) {
				CR_r = g_utf8_strchr(
						cmd->data->str + cmd->pos,
						cmd->data->len - cmd->pos,
						g_utf8_get_char("\015"));
			}
			else
				CR_r = strchr(cmd->data->str + cmd->pos, '\015');

			if (!CR_r) {
				/* Erase everything to the right -- no ^Ms to take into
				   account. */
				cmd->data = g_string_truncate(cmd->data, cmd->pos);
			}
			else {
				/* Erase to the right up to the first ^M. */
				if (cmd->is_utf8) {
					chars = g_utf8_pointer_to_offset(
							cmd->data->str + cmd->pos, CR_r);
				}
				else
					chars = (CR_r - cmd->data->str) - cmd->pos;

				cmd_del_chars(cmd, chars);

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
			if (cmd->is_utf8) {
				CR_r = g_utf8_strchr(
						cmd->data->str + cmd->pos,
						cmd->data->len - cmd->pos,
						g_utf8_get_char("\015"));
			}
			else
				CR_r = strchr(cmd->data->str + cmd->pos, '\015');

			if (!CR_r)
				CR_r = cmd->data->str + cmd->data->len;
			
			/* Find the ^M to the left. */
			if (cmd->is_utf8) {
				CR_l = g_utf8_strrchr(cmd->data->str, cmd->pos,
						g_utf8_get_char("\015"));
			}
			else
				CR_l = g_strrstr_len(cmd->data->str, cmd->pos, "\015");

			if (!CR_l)
				CR_l = cmd->data->str;

			/* Delete everything in-between. */
			cmd->pos = CR_l - cmd->data->str;

			if (cmd->is_utf8)
				chars = g_utf8_pointer_to_offset(CR_l, CR_r);
			else 
				chars = CR_r - CR_l;

			cmd_del_chars(cmd, chars);

			break;

		default:
			g_return_val_if_reached(FALSE);
			break;
	}

	action_queue(A_SEND_CMD);
	return TRUE;
}


gboolean cmd_insert_chars(struct cmdline* cmd, gchar c, gint n) {
	/* Right now this function is only used to insert spaces, so it's safe
	   for UTF-8. */

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
	/* This should be safe for UTF-8 as well. */
	gint temp;
	while (cmd->data->len &&
			cmd->data->str[cmd->data->len - 1] == '\015' &&
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


/* Try to move the cursor forward n characters.  Does not modify command line
   if unsuccessful. */
gboolean cmd_forward(struct cmdline* cmd, gint n, gboolean do_it) {
	g_return_val_if_fail(n >= 0, FALSE);

	if (cmd->is_utf8) {
		gint i;
		gint new_pos = cmd->pos;
		for (i = 0; i < n; i++) {
			if ( *(cmd->data->str + new_pos) == '\0'
					|| new_pos > cmd->data->len) {
				return FALSE;
			}
			gchar* next = g_utf8_next_char(cmd->data->str + new_pos);
			new_pos = next - cmd->data->str;
		}
		if (do_it)
			cmd->pos = new_pos;
	}
	else {
		/* Just need to shift by n. */
		if (cmd->pos + n <= cmd->data->len) {
			if (do_it)
				cmd->pos += n;
		}
		else
			return FALSE;
	}

	return TRUE;
}


/* Try to move the cursor backward n characters.  Does not modify command line
   if unsuccessful. */
gboolean cmd_backward(struct cmdline* cmd, gint n, gboolean do_it) {
	g_return_val_if_fail(n >= 0, FALSE);

	if (cmd->is_utf8) {
		gint i;
		gint new_pos = cmd->pos;
		for (i = 0; i < n; i++) {
			gchar* prev = g_utf8_find_prev_char(cmd->data->str,
					cmd->data->str + new_pos);
			if (!prev)
				return FALSE;

			new_pos = prev - cmd->data->str;
		}
		if (do_it)
			cmd->pos = new_pos;
	}
	else {
		/* Just need to shift by n. */
		if (cmd->pos - n >= 0) {
			if (do_it)
				cmd->pos -= n;
		}
		else
			return FALSE;
	}

	return TRUE;
}

