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
#include "actions.h"
#include "connection.h"
#include "sequences.h"

#include <string.h>
#include <ctype.h>

typedef struct _Sequence Sequence;
struct _Sequence {
	gchar* name;	            /* For debugging. */
	gchar* seq;
	const gint length;
	gint pos;

	/* Determines whether a match should be attempted. */
	gboolean enabled;

	/* Function to run after successful match. */
	MatchEffect (*func)(Connection* b, struct cmdline* cmd);
};

/* Each process level has a set of sequences that it checks for.  This
   struct is for these sets. */
typedef struct _SeqGroup SeqGroup;
struct _SeqGroup {
	gint n;                 /* Number of sequences. */
	Sequence** seqs;
};


/* Sequence parsing functions. */
static gint    parse_digits(const gchar* string, size_t len);
static gchar*  parse_printables(const gchar* string, const gchar* sequence);

static void init_bash_seqs(void);
static void init_zsh_seqs(void);
static MatchStatus check_seq(gchar c, Sequence* sq);
static void analyze_effect(MatchEffect effect, Connection* b,
		struct cmdline* cmd);

/* Pattern completion actions. */
static MatchEffect seq_ps1_separator(Connection* b, struct cmdline* cmd);
static MatchEffect seq_rprompt_separator_start(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_rprompt_separator_end(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_zsh_completion_done(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_new_pwd(Connection* b, struct cmdline* cmd);

static MatchEffect seq_ctrl_g(Connection* b, struct cmdline* cmd);
static MatchEffect seq_viewglob_all(Connection* b, struct cmdline* cmd);
static gboolean arrow_key(Connection* b);

static MatchEffect seq_term_cmd_wrapped(Connection* b, struct cmdline* cmd);
static MatchEffect seq_term_backspace(Connection* b, struct cmdline* cmd);
static MatchEffect seq_term_cursor_forward(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_term_cursor_backward(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_term_cursor_up(Connection* b, struct cmdline* cmd);
static MatchEffect seq_term_erase_in_line(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_term_delete_chars(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_term_insert_blanks(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_term_bell(Connection* b, struct cmdline* cmd);
static MatchEffect seq_term_carriage_return(Connection* b,
		struct cmdline* cmd);
static MatchEffect seq_term_newline(Connection* b, struct cmdline* cmd);

/* The below characters don't appear in any of the sequences viewglob looks
   for, so we can use them as special characters. */
#define DIGIT_C      '\026'      /* Any sequence of digits (or none). */
#define DIGIT_S      "\026"
#define PRINTABLE_C  '\022'      /* Any sequence of printable characters */
#define PRINTABLE_S  "\022"      /* (or none) */
#define NOT_LF_CR_C  '\023'      /* Any single non-linefeed, non-carriage */
#define NOT_LF_CR_S  "\023"      /* return character. */
#define NOT_LF_C     '\024'      /* Any single non-linefeed character. */
#define NOT_LF_S     "\024"
#define ANY_C        '\030'      /* Any character. */
#define ANY_S        "\030"


/* Viewglob escape sequences.  Note: if these are changed, the
   init-viewglob.*rc files must be updated. */
#define STR_LEN_PAIR(s) s, sizeof (s) - 1
static Sequence PS1_SEPARATOR_SEQ = {
	"ps1 separator seq",
	STR_LEN_PAIR("\033[0;30m\033[0m\033[1;37m\033[0m"),
	0, FALSE, seq_ps1_separator,
};
static Sequence RPROMPT_SEPARATOR_START_SEQ = {
	"RPROMPT separator start",
	STR_LEN_PAIR("\033[0;34m\033[0m\033[0;31m\033[0m"),
	0, FALSE, seq_rprompt_separator_start,
};
static Sequence RPROMPT_SEPARATOR_END_SEQ = {
	"RPROMPT separator end",
	STR_LEN_PAIR("\033[0;34m\033[0m\033[0;31m\033[0m" "\033[" DIGIT_S "D"),
	0, FALSE, seq_rprompt_separator_end,
};
static Sequence NEW_PWD_SEQ = {
	"New pwd",
	STR_LEN_PAIR("\033P" PRINTABLE_S "\033\\"),
	0, FALSE, seq_new_pwd,
};


/* Enters into Viewglob mode. */
static Sequence CTRL_G_SEQ = {
	"Ctrl-G",
	STR_LEN_PAIR("\007"),
	0, FALSE, seq_ctrl_g,
};

/* We examine every key on an individual basis in Viewglob mode. */
static Sequence VIEWGLOB_ALL_SEQ = {
	"Viewglob seq",
	STR_LEN_PAIR(ANY_S),
	0, FALSE, seq_viewglob_all,
};


/* I've observed this always comes at the end of a list of tab completions
   in zsh. */
static Sequence ZSH_COMPLETION_DONE_SEQ = {
	"Zsh completion done",
	STR_LEN_PAIR("\033[0m\033[27m\033[24m\015\033[" DIGIT_S "C"),
	0, FALSE, seq_zsh_completion_done,
};

/* Terminal escape sequences that we have to watch for.
   There are many more, but in my observations only these are
   important for Viewglob's purposes. */
static Sequence TERM_CMD_WRAPPED_SEQ = {
	"Term cmd wrapped",
	STR_LEN_PAIR(" \015" NOT_LF_CR_S ""),
	0, FALSE, seq_term_cmd_wrapped,
};
static Sequence TERM_CARRIAGE_RETURN_SEQ = {
	"Term carriage return",
	STR_LEN_PAIR("\015" NOT_LF_S ""),
	0, FALSE, seq_term_carriage_return,
};
static Sequence TERM_NEWLINE_SEQ = {
	"Term newline",
	STR_LEN_PAIR("\015\n"),
	0, FALSE, seq_term_newline,
};
static Sequence TERM_BACKSPACE_SEQ  = {
	"Term backspace",
	STR_LEN_PAIR("\010"),
	0, FALSE, seq_term_backspace,
};
static Sequence TERM_CURSOR_FORWARD_SEQ = {
	"Term cursor forward",
	STR_LEN_PAIR("\033[" DIGIT_S "C"),
	0, FALSE, seq_term_cursor_forward,
};
static Sequence TERM_CURSOR_BACKWARD_SEQ = {
	"Term cursor backward",
	STR_LEN_PAIR("\033[" DIGIT_S "D"),
	0, FALSE, seq_term_cursor_backward,
};
static Sequence TERM_CURSOR_UP_SEQ  = {
	"Term cursor up",
	STR_LEN_PAIR("\033[" DIGIT_S "A"),
	0, FALSE, seq_term_cursor_up,
};
static Sequence TERM_ERASE_IN_LINE_SEQ = {
	"Term erase in line",
	STR_LEN_PAIR("\033[" DIGIT_S "K"),
	0, FALSE, seq_term_erase_in_line,
};
static Sequence TERM_DELETE_CHARS_SEQ = {
	"Term delete chars",
	STR_LEN_PAIR("\033[" DIGIT_S "P"),
	0, FALSE, seq_term_delete_chars,
};
static Sequence TERM_INSERT_BLANKS_SEQ = {
	"Term insert blanks",
	STR_LEN_PAIR("\033[" DIGIT_S "@"),
	0, FALSE, seq_term_insert_blanks,
};
static Sequence TERM_BELL_SEQ = {
	"Term bell",
	STR_LEN_PAIR("\007"),
	0, FALSE, seq_term_bell,
};


static SeqGroup* seq_groups;
static enum shell_type shell;


/* Grab the first set of digits from the string
   and convert into an integer. */
static gint parse_digits(const gchar* string, size_t len) {
	guint i;
	gboolean digits = FALSE;

	g_return_val_if_fail(string != NULL, 0);
	g_return_val_if_fail(len != 0, 0);

	for (i = 0; i < len; i++) {
		if (isdigit(string[i])) {
			digits = TRUE;
			break;
		}
	}

	if (digits)
		return atoi(string + i);
	else
		return 0;
}


/* Grab the first set of printable characters from
   the string and return. */
static gchar* parse_printables(const gchar* string, const gchar* sequence) {
	gchar* printables;
	gchar* p;

	gint pos, nbytes;
	gchar delimiter;

	g_return_val_if_fail(string != NULL, NULL);
	g_return_val_if_fail(sequence != NULL, NULL);

	/* Find the location of the printables in the sequence. */
	p = strchr(sequence, PRINTABLE_C);
	if (!p)
		return NULL;

	pos = p - sequence;
	delimiter = *(p + 1);

	for (nbytes = 0; *(string + pos + nbytes) != delimiter; nbytes++)
		;
	printables = g_malloc(nbytes + 1);

	/* Make a copy. */
	(void)strncpy(printables, string + pos, nbytes);
	*(printables + nbytes) = '\0';
	return printables;
}


/* Initialize all of the sequences.  Some of them are duplicated
   in different groups instead of referenced, which should be fixed. */
void init_seqs(enum shell_type shell) {

	seq_groups = g_new(SeqGroup, PL_COUNT);
	if (shell == ST_BASH)
		init_bash_seqs();
	else if (shell == ST_ZSH)
		init_zsh_seqs();
	else
		g_critical("Unexpected shell type");

	seq_groups[PL_TERMINAL].n = 1;
	seq_groups[PL_TERMINAL].seqs = g_new(Sequence*, 1);
	seq_groups[PL_TERMINAL].seqs[0] = &CTRL_G_SEQ;

	seq_groups[PL_VIEWGLOB].n = 1;
	seq_groups[PL_VIEWGLOB].seqs = g_new(Sequence*, 1);
	seq_groups[PL_VIEWGLOB].seqs[0] = &VIEWGLOB_ALL_SEQ;
}


static void init_bash_seqs(void) {

	shell = ST_BASH;

	seq_groups[PL_AT_PROMPT].n = 13;
	seq_groups[PL_AT_PROMPT].seqs = g_new(Sequence*, 13);

	seq_groups[PL_EXECUTING].n = 2;
	seq_groups[PL_EXECUTING].seqs = g_new(Sequence*, 2);

	/* PL_AT_PROMPT */
	seq_groups[PL_AT_PROMPT].seqs[0] = &PS1_SEPARATOR_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[1] = &NEW_PWD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[2] = &TERM_CMD_WRAPPED_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[3] = &TERM_CURSOR_FORWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[4] = &TERM_BACKSPACE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[5] = &TERM_ERASE_IN_LINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[6] = &TERM_DELETE_CHARS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[7] = &TERM_INSERT_BLANKS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[8] = &TERM_CURSOR_BACKWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[9] = &TERM_BELL_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[10] = &TERM_CURSOR_UP_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[11] = &TERM_CARRIAGE_RETURN_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[12] = &TERM_NEWLINE_SEQ;

	/* PL_EXECUTING */
	seq_groups[PL_EXECUTING].seqs[0] = &PS1_SEPARATOR_SEQ;
	seq_groups[PL_EXECUTING].seqs[1] = &NEW_PWD_SEQ;
}


static void init_zsh_seqs(void) {

	shell = ST_ZSH;

	seq_groups[PL_AT_PROMPT].n = 14;
	seq_groups[PL_AT_PROMPT].seqs = g_new(Sequence*, 14);

	seq_groups[PL_EXECUTING].n = 4;
	seq_groups[PL_EXECUTING].seqs = g_new(Sequence*, 4);

	seq_groups[PL_AT_RPROMPT].n = 1;
	seq_groups[PL_AT_RPROMPT].seqs = g_new(Sequence*, 1);

	/* PL_AT_PROMPT */
	seq_groups[PL_AT_PROMPT].seqs[0] = &PS1_SEPARATOR_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[1] = &RPROMPT_SEPARATOR_START_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[2] = &TERM_CMD_WRAPPED_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[3] = &TERM_CURSOR_FORWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[4] = &TERM_BACKSPACE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[5] = &TERM_ERASE_IN_LINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[6] = &TERM_DELETE_CHARS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[7] = &TERM_INSERT_BLANKS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[8] = &TERM_CURSOR_BACKWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[9] = &TERM_BELL_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[10] = &TERM_CURSOR_UP_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[11] = &TERM_CARRIAGE_RETURN_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[12] = &TERM_NEWLINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[13] = &NEW_PWD_SEQ;

	/* PL_EXECUTING */
	seq_groups[PL_EXECUTING].seqs[0] = &PS1_SEPARATOR_SEQ;
	seq_groups[PL_EXECUTING].seqs[1] = &RPROMPT_SEPARATOR_END_SEQ;
	seq_groups[PL_EXECUTING].seqs[2] = &NEW_PWD_SEQ;
	seq_groups[PL_EXECUTING].seqs[3] = &ZSH_COMPLETION_DONE_SEQ;

	/* PL_AT_RPROMPT */
	seq_groups[PL_AT_RPROMPT].seqs[0] = &RPROMPT_SEPARATOR_END_SEQ;
}


static void disable_seq(Sequence* sq) {
	sq->enabled = FALSE;
}


void enable_all_seqs(enum process_level pl) {
	gint i;
	for (i = 0; i < seq_groups[pl].n; i++)
		seq_groups[pl].seqs[i]->enabled = TRUE;
}


void check_seqs(Connection* b, struct cmdline* cmd) {
	gint i;
	MatchEffect effect;

	gchar c = b->buf[b->pos + b->seglen];
	SeqGroup seq_group = seq_groups[b->pl];

	b->status = MS_NO_MATCH;
	for (i = 0; i < seq_group.n; i++) {

		Sequence* seq = *(seq_group.seqs + i);

		b->status |= check_seq(c, seq);
		if (b->status & MS_MATCH) {

			/* Execute the pattern match function and analyze. */
			effect = (*(seq->func))(b, cmd);

			analyze_effect(effect, b, cmd);

			break;
		}
	}
}


/* Assumes that the sequences are intelligently chosen and are not
   malformed.  To elaborate:
   	- Do not have a special character delimiter after a special character.
	- Do not end a sequence with a special character.

   This function is poorly written, but it works.  */
static MatchStatus check_seq(gchar c, Sequence* sq) {

	MatchStatus result = MS_IN_PROGRESS;

	/* This sequence is disabled right now -- assume no match. */
	if (sq->enabled == FALSE)
		return MS_NO_MATCH;

	switch (sq->seq[sq->pos]) {
		case ANY_C:
			sq->pos++;
			break;

		case DIGIT_C:
			if (isdigit(c))
				return MS_IN_PROGRESS;
			else if (sq->seq[sq->pos + 1] == c) {
				/* Skip over special char and delimiter. */
				sq->pos += 2;
			}
			else {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			break;

		case PRINTABLE_C:
			if (isprint(c))
				return MS_IN_PROGRESS;
			else if (sq->seq[sq->pos + 1] == c) {
				/* Skip over special char and delimiter. */
				sq->pos += 2;
			}
			else {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			break;

		case NOT_LF_C:
			if (c == '\012') {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			else
				sq->pos++;
			break;

		case NOT_LF_CR_C:
			/* Linefeed or carriage return. */
			if (c == '\012' || c == '\015') {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			else
				sq->pos++;
			break;

		default:
			if (sq->seq[sq->pos] == c)
				sq->pos++;
			else {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			break;
	}

	if (sq->pos == sq->length) {
		sq->pos = 0;
		result = MS_MATCH;
	}
	return result;
}


/* React (or not) on the instance of a sequence match. */
static void analyze_effect(MatchEffect effect, Connection* b,
		struct cmdline* cmd) {

	switch (effect) {

		case ME_ERROR:
			/* Clear the command line to indicate we're out of sync,
			   and wait for the next PS1. */
			cmd_clear(cmd);
			b->pl = PL_EXECUTING;
			action_queue(A_SEND_LOST);
			break;

		case ME_NO_EFFECT:
			break;

		case ME_CMD_EXECUTED:
			b->pl = PL_EXECUTING;
			break;

		case ME_CMD_STARTED:
			if (cmd->rebuilding)
				cmd->rebuilding = FALSE;
			else
				cmd_clear(cmd);
			b->pl = PL_AT_PROMPT;
			action_queue(A_SEND_CMD);
			break;

		case ME_CMD_REBUILD:
			cmd->rebuilding = TRUE;
			b->pl = PL_EXECUTING;
			break;

		case ME_PWD_CHANGED:
			/* Send the new current directory. */
			action_queue(A_SEND_PWD);
			break;

		case ME_RPROMPT_STARTED:
			cmd->rebuilding = TRUE;
			b->pl = PL_AT_RPROMPT;
			break;

		default:
			g_return_if_reached();
			break;
	}
}


void clear_seqs(enum process_level pl) {
	gint i;
	for (i = 0; i < seq_groups[pl].n; i++)
		seq_groups[pl].seqs[i]->pos = 0;
	return;
}



static MatchEffect seq_ps1_separator(Connection* b,
		struct cmdline* cmd) {
	pass_segment(b);
	return ME_CMD_STARTED;
}


static MatchEffect seq_rprompt_separator_start(Connection* b,
		struct cmdline* cmd) {
	pass_segment(b);
	return ME_RPROMPT_STARTED;
}


static MatchEffect seq_rprompt_separator_end(Connection* b,
		struct cmdline* cmd) {
	pass_segment(b);
	return ME_CMD_STARTED;
}


static MatchEffect seq_new_pwd(Connection* b, struct cmdline* cmd) {
	if (cmd->pwd != NULL)
		g_free(cmd->pwd);

	cmd->pwd = parse_printables(b->buf + b->pos, NEW_PWD_SEQ.seq);

	eat_segment(b);
	return ME_PWD_CHANGED;
}


static MatchEffect seq_zsh_completion_done(Connection* b,
		struct cmdline* cmd) {
	cmd->rebuilding = TRUE;
	pass_segment(b);
	return ME_NO_EFFECT;
}


/* Add a carriage return to the present location in the command line,
   or if we're expecting a newline, command executed. */
static MatchEffect seq_term_cmd_wrapped(Connection* b, struct cmdline* cmd) {
	MatchEffect effect;

	if (cmd->expect_newline)
		effect = ME_CMD_EXECUTED;
	else if (!cmd_overwrite_char(cmd, '\015', FALSE))
		effect = ME_ERROR;
	else
		effect = ME_NO_EFFECT;

	/* Don't want to pass over the NOT_LF char. */
	b->seglen--;
	pass_segment(b);
	return effect;
}


/* Back up one character. */
static MatchEffect seq_term_backspace(Connection* b, struct cmdline* cmd) {
	MatchEffect effect;

	if (cmd->pos > 0) {
		cmd->pos--;
		effect = ME_NO_EFFECT;
	}
	else
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Move cursor forward from present location n times. */
static MatchEffect seq_term_cursor_forward(Connection* b,
		struct cmdline* cmd) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	n = parse_digits(b->buf + b->pos, b->seglen + 1);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (cmd->pos + n <= cmd->data->len)
		cmd->pos += n;
	else {
		if (shell == ST_ZSH) {
			if (cmd->pos + n == cmd->data->len + 1) {
				/* This is more likely to just be a space causing a
				   deletion of the RPROMPT in zsh. */
				cmd_overwrite_char(cmd, ' ', FALSE);
			}
			else {
				/* It's writing the RPROMPT. */
				cmd->rebuilding = TRUE;
				effect = ME_RPROMPT_STARTED;
			}
		}
		else /* ST_BASH */
			effect = ME_CMD_EXECUTED;
	}
	pass_segment(b);
	return effect;
}


/* Move cursor backward from present location n times. */
static MatchEffect seq_term_cursor_backward(Connection* b,
		struct cmdline* cmd) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	n = parse_digits(b->buf + b->pos, b->seglen + 1);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (cmd->pos - n >= 0)
		cmd->pos -= n;
	else
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Delete n chars from the command line. */
static MatchEffect seq_term_delete_chars(Connection* b,
		struct cmdline* cmd) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	n = parse_digits(b->buf + b->pos, b->seglen + 1);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (!cmd_del_chars(cmd, n))
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}

/* Insert n blanks at the current location on command line. */
static MatchEffect seq_term_insert_blanks(Connection* b,
		struct cmdline* cmd) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;

	n = parse_digits(b->buf + b->pos, b->seglen + 1);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (!cmd_insert_chars(cmd, ' ', n))
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Delete to the left or right of the cursor, or the whole line.
	0 = Clear to right
	1 = Clear to left
	2 = Clear all */
static MatchEffect seq_term_erase_in_line(Connection* b,
		struct cmdline* cmd) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	/* Default is 0. */
	n = parse_digits(b->buf + b->pos, b->seglen + 1);

	if (!cmd_wipe_in_line(cmd, n))
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Just ignore the damn bell. */
static MatchEffect seq_term_bell(Connection* b, struct cmdline* cmd) {
	pass_segment(b);
	return ME_NO_EFFECT;
}


/* Move cursor up from present location n times. */
static MatchEffect seq_term_cursor_up(Connection* b, struct cmdline* cmd) {
	MatchEffect effect = ME_NO_EFFECT;
	gint i, n;
	gint offset;
	gchar* last_cr_p;
	gchar* next_cr_p;
	gchar* pos;

	n = parse_digits(b->buf + b->pos, b->seglen + 1);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}

	last_cr_p = g_strrstr_len(cmd->data->str, cmd->pos, "\015");
	next_cr_p = g_strstr_len(cmd->data->str + cmd->pos,
			cmd->data->len - cmd->pos, "\015");
	if (last_cr_p == NULL && next_cr_p == NULL)
		goto out_of_prompt;

	/* First try to find the ^M at the beginning of the wanted line. */
	if (last_cr_p != NULL) {

		pos = cmd->data->str + cmd->pos;
		for (i = 0; i < n + 1 && pos != NULL; i++)
			pos = g_strrstr_len(cmd->data->str, pos - cmd->data->str, "\015");

		if (pos != NULL) {
			/* Cursor is offset chars from the beginning of the line. */
			offset = cmd->data->str + cmd->pos - last_cr_p;
			/* Position cursor on new line at same position. */
			cmd->pos = pos + offset - cmd->data->str;
			if (cmd->pos >= 0) {
				effect = ME_NO_EFFECT;
				goto done;
			}
			else
				goto out_of_prompt;
		}
	}

	/* That failed, so now try to find the ^M at the end of the wanted
	   line. */
	if (next_cr_p != NULL) {

		pos = cmd->data->str + cmd->pos;
		for (i = 0; i < n && pos != NULL; i++)
			pos = g_strrstr_len(cmd->data->str, pos - cmd->data->str, "\015");

		if (pos != NULL) {
			/* Cursor is offset chars from the end of the line. */
			offset = next_cr_p - cmd->data->str - cmd->pos;
			/* Position cursor on new line at same position. */
			cmd->pos = pos - offset - cmd->data->str;
			if (cmd->pos >= 0) {
				effect = ME_NO_EFFECT;
				goto done;
			}
			else
				goto out_of_prompt;
		}
	}

	/* No luck. */
	out_of_prompt:
	effect = ME_CMD_REBUILD;
	cmd->pos = 0;

	done:
	pass_segment(b);
	return effect;
}


/* Return cursor to beginning of this line, or if expecting
   newline, command executed. */
static MatchEffect seq_term_carriage_return(Connection* b,
		struct cmdline* cmd) {
	MatchEffect effect;
	gchar* p;

	if (cmd->expect_newline) {
		effect = ME_CMD_EXECUTED;
		pass_segment(b);
	}
	else {
		p = g_strrstr_len(cmd->data->str, cmd->pos, "\015");
		if (p == NULL) {
			cmd->pos = 0;
			effect = ME_CMD_REBUILD;
		}
		else {
			/* Go to the character just after the ^M. */
			cmd->pos = p - cmd->data->str + 1;
			effect = ME_NO_EFFECT;
		}

		/* We don't want to pop off the character following the ^M. */
		b->seglen--;
		pass_segment(b);
	}

	return effect;
}


/* What this function returns depends on whether the user has
   pressed any of return (enter), Ctrl-C, or Ctrl-D or not.
   If so, then a newline that takes us out of the command line
   is interpreted as a command execution.  If not, then it is
   a command line wrap. */
static MatchEffect seq_term_newline(Connection* b, struct cmdline* cmd) {
	gchar* p;
	MatchEffect effect;

	p = g_strstr_len(cmd->data->str + cmd->pos, cmd->data->len - cmd->pos,
			"\015");
	if (p == NULL) {
		if (cmd->expect_newline) {
			/* Command must have been executed. */
			effect = ME_CMD_EXECUTED;
		}
		else {
			/* Newline must just be a wrap. */
			cmd->pos = cmd->data->len;
			if (cmd_overwrite_char(cmd, '\015', FALSE))
				effect = ME_NO_EFFECT;
			else
				effect = ME_ERROR;
		}
	}
	else {
		cmd->pos = p - cmd->data->str + 1;
		effect = ME_NO_EFFECT;
	}
	pass_segment(b);
	return effect;
}


/* The Ctrl-G key was pressed. */
static MatchEffect seq_ctrl_g(Connection* b, struct cmdline* cmd) {
	eat_segment(b);
	b->pl = PL_VIEWGLOB;
	return ME_NO_EFFECT;
}


/* Viewglob key sequences.  There is quite a bit of repeated code here,
   but it gets the job done. */
static MatchEffect seq_viewglob_all(Connection* b, struct cmdline* cmd) {

	g_return_val_if_fail(b != NULL, ME_ERROR);
	g_return_val_if_fail(cmd != NULL, ME_ERROR);

	/* When in_navigation, we accept navigation keys without first seeing
	   a Ctrl-G. */
	static gboolean in_navigation = FALSE;

	/* When in_mask, we build up a developing mask until a control character
	   is seen. */
	static gboolean in_mask = FALSE;

	char c = *(b->buf + b->pos + b->seglen);

	if (in_mask) {
		if (isprint(c)) {
			cmd_mask_add(cmd, c);
		}
		else if (c == '\010' || c == '\177') {  /* Backspace & Del */
			cmd_mask_del(cmd);
		}
		else {
			if (c == '\015')   /* Only Enter will submit. */
				action_queue(A_MASK_FINAL);
			if (c != '\015')
				cmd_mask_clear(cmd);
			in_mask = FALSE;
			b->pl = PL_TERMINAL;
		}
		eat_segment(b);
	}
	else if (in_navigation) {
		switch (c) {
			case '\013':  /* Nav vi up */
			case '\020':  /* Nav emacs up */
				action_queue(A_SEND_UP);
				eat_segment(b);
				break;

			case '\012':  /* Nav vi down */
			case '\016':  /* Nav emacs down */
				action_queue(A_SEND_DOWN);
				eat_segment(b);
				break;

			case '\002':  /* Nav vi pgup */
			case '\025':  /* Nav emacs pgup */
				action_queue(A_SEND_PGUP);
				eat_segment(b);
				break;

			case '\006':  /* Nav vi pgdown */
			case '\004':  /* Nav emacs pgdown */
				action_queue(A_SEND_PGDOWN);
				eat_segment(b);
				break;

			case '\033': /* Arrow or pgdown/pgup keys */
				if (arrow_key(b))
					eat_segment(b);
				else {
					in_navigation = FALSE;
					pass_segment(b);
					b->pl = PL_TERMINAL;
				}
				break;

			case '\007': /* Ctrl-G */
				in_navigation = FALSE;
				eat_segment(b);
				break;

			default:
				in_navigation = FALSE;
				pass_segment(b);
				b->pl = PL_TERMINAL;
				break;
		}
	}
	else if (isprint(c) || c == '\010' || c == '\177') {
		in_mask = TRUE;
		cmd_mask_clear(cmd);
		if (c != '\010' && c != '\177') /* Backspace & Del */
			cmd_mask_add(cmd, c);
		eat_segment(b);
	}
	else {
		switch (c) {
			case '\013':  /* Nav vi up */
			case '\020':  /* Nav emacs up */
				in_navigation = TRUE;
				action_queue(A_SEND_UP);
				eat_segment(b);
				break;

			case '\012':  /* Nav vi down */
			case '\016':  /* Nav emacs down FIXME */
				in_navigation = TRUE;
				action_queue(A_SEND_DOWN);
				eat_segment(b);
				break;

			case '\002':  /* Nav vi pgup */
			case '\025':  /* Nav emacs pgup */
				in_navigation = TRUE;
				action_queue(A_SEND_PGUP);
				eat_segment(b);
				break;

			case '\006':  /* Nav vi pgdown */
			case '\004':  /* Nav emacs pgdown */
				in_navigation = TRUE;
				action_queue(A_SEND_PGDOWN);
				eat_segment(b);
				break;

			case '\033': /* Arrow or pgdown/pgup keys */
				if (arrow_key(b)) {
					in_navigation = TRUE;
				}
				else
					b->pl = PL_TERMINAL;
				eat_segment(b);
				break;


			case '\t':    /* Refocus */
				action_queue(A_REFOCUS);
				eat_segment(b);
				b->pl = PL_TERMINAL;
				break;

			case '\015':  /* Clear mask */
				cmd_mask_clear(cmd);
				action_queue(A_MASK_FINAL);
				eat_segment(b);
				b->pl = PL_TERMINAL;
				break;

			case '\000':  /* Toggle */
				action_queue(A_TOGGLE);
				eat_segment(b);
				b->pl = PL_TERMINAL;
				break;

			case '\021':  /* Disable */
				action_queue(A_DISABLE);
				eat_segment(b);
				b->pl = PL_TERMINAL;
				break;

			case '\007':  /* Ctrl-G */
				pass_segment(b);
				b->pl = PL_TERMINAL;
				break;

			default:
				eat_segment(b);
				b->pl = PL_TERMINAL;
		}
	}
	return ME_NO_EFFECT;
}


/* Peek into the buffer to see if we're looking at an arrow key, and if
   so extend the segment to cover the whole sequence.  Also add action
   associated with the navigation key. */
static gboolean arrow_key(Connection* b) {

	g_return_val_if_fail(b != NULL, FALSE);

	/* Navigation key sequences */
	const struct nav_seq {
		const gchar* seq;
		const Action action;
	} navs[] = {
		{ "\033[A", A_SEND_UP },      /* Up */
		{ "\033Oa", A_SEND_UP },      /* Ctrl-Up */
		{ "\033[B", A_SEND_DOWN },    /* Down */
		{ "\033Ob", A_SEND_DOWN },    /* Ctrl-Down */
		{ "\033[5~", A_SEND_PGUP },   /* PgUp */
		{ "\033[5^", A_SEND_PGUP },   /* Ctrl-PgUp */
		{ "\033[6~", A_SEND_PGDOWN }, /* PgDown */
		{ "\033[6^", A_SEND_PGDOWN }, /* Ctrl-PgDown */
	};

	gchar* start = b->buf + b->pos;
	gsize max_len = b->filled - b->pos;
	gsize seq_len;

	int i;
	for (i = 0; i < sizeof(navs)/sizeof(struct nav_seq); i++) {
		seq_len = strlen(navs[i].seq);
		if (seq_len <= max_len && STRNEQ(start, navs[i].seq, max_len)) {
			b->seglen += strlen(navs[i].seq) - 1;
			action_queue(navs[i].action);
			return TRUE;
		}
	}

	return FALSE;
}

