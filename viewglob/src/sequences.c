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
#include <string.h>
#include "seer.h"
#include "sequences.h"


typedef struct _Sequence Sequence;
struct _Sequence {
	char name[32];	            /* For debugging. */
	char* seq;
	int length;
	int pos;
	bool enabled;               /* Determines whether a match should be attempted. */
	MatchEffect (*func)(void);  /* Function to run after successful match. */
};

/* Each process level (at prompt, executing, and eating) has a set of
   sequences that it checks for.  This struct is for these sets. */
struct sequence_group {
	int n;                 /* Number of sequences. */
	Sequence* seqs;
};


/* Sequence parsing functions. */
static int    parse_digits(const char* string);
static char*  parse_printables(const char* string, const char* sequence);

static MatchStatus check_seq(char c, Sequence* sq);

/* Pattern completion actions. */
static MatchEffect seq_ps1_separator(void);
static MatchEffect seq_ps2_separator(void);
/* static MatchEffect seq_prompt_command(void); */
static MatchEffect seq_eat_start(void);
static MatchEffect seq_eat_end(void);
static MatchEffect seq_dummy(void);
static MatchEffect seq_term_cl_wrapped(void);
static MatchEffect seq_term_backspace(void);
static MatchEffect seq_term_cursor_forward(void);
static MatchEffect seq_term_cursor_backward(void);
static MatchEffect seq_term_cursor_up(void);
static MatchEffect seq_term_erase_in_line(void);
static MatchEffect seq_term_delete_chars(void);
static MatchEffect seq_term_insert_blanks(void);
static MatchEffect seq_term_bell(void);
static MatchEffect seq_term_carriage_return(void);
static MatchEffect seq_term_newline(void);


/* In sequences: 
     - The ENQ control character (005) stands for any sequence of digits (or none).
     - The ACK control character (006) stands for any sequence ofprintable characters (or none).
     - The EOT control character (004) stands for any non-linefeed character. */
#define DIGIT_C '\005'
#define PRINTABLE_C '\006'
#define DIGIT_S "\005"
#define PRINTABLE_S "\006"
#define NOT_LF_C '\004'
#define NOT_LF_S "\004"

/* viewglob escape sequences.
   Note: if these are changed, init-viewglob.sh must be updated. */
static char* const PS1_SEPARATOR_SEQ = "\033[0;30m\033[0m\033[1;37m\033[0m";
static char* const PS2_SEPARATOR_SEQ = "\033[0;34m\033[0m\033[0;31m\033[0m";
/* static char* const PROMPT_COMMAND_SEQ  = "\033P" PRINTABLE_S "\033\\"; */
static char* const EAT_START_SEQ  = "\033P";
static char* const EAT_END_SEQ = PRINTABLE_S "\033\\";

/* Terminal escape sequences that we have to watch for.
   There are many more, but in my observations only these are
   important for viewglob's purposes. */
static char* const TERM_CL_WRAPPED_SEQ = " \015";
static char* const TERM_BACKSPACE_SEQ  = "\010";
static char* const TERM_CURSOR_FORWARD_SEQ = "\033[" DIGIT_S "C";
static char* const TERM_CURSOR_BACKWARD_SEQ = "\033[" DIGIT_S "D";
static char* const TERM_CURSOR_UP_SEQ  = "\033[" DIGIT_S "A";
static char* const TERM_ERASE_IN_LINE_SEQ = "\033[" DIGIT_S "K";
static char* const TERM_DELETE_CHARS_SEQ = "\033[" DIGIT_S "P";
static char* const TERM_INSERT_BLANKS_SEQ = "\033[" DIGIT_S "@";
static char* const TERM_BELL_SEQ = "\007";
static char* const TERM_CARRIAGE_RETURN_SEQ = "\015" NOT_LF_S "";
static char* const TERM_NEWLINE_SEQ = "\015\n";


#if DEBUG_ON
extern FILE* df;
#endif

extern struct user_shell u;

/* Kludge variable. */
extern int chars_to_save;


static struct sequence_group* seq_groups;


/* Grab the first set of digits from the string
   and convert into an integer. */
static int parse_digits(const char* string) {
	int i;
	for (i = 0; string[i] != '\0'; i++)
		if (isdigit(string[i]))
			break;

	return atoi(string + i);
}


/* Grab the first set of printable characters from
   the string and return. */
static char* parse_printables(const char* string, const char* sequence) {
	char* printables;
	char* p;

	int pos, nbytes;
	char delimiter;

	/* Find the location of the printables in the sequence. */
	p = strchr(sequence, PRINTABLE_C);
	if (!p)
		return NULL;

	pos = p - sequence;
	delimiter = *(p + 1);

	for (nbytes = 0; *(string + pos + nbytes) != delimiter; nbytes++) ;
	printables = XMALLOC(char, nbytes + 1);

	/* Make a copy. */
	(void)strncpy(printables, string + pos, nbytes);
	*(printables + nbytes) = '\0';
	return printables;
}


int find_prev_cret(int pos) {
	int i;
	bool found = false;

	/* Find the first ^M to the left of pos. */
	for (i = pos - 1; i >= 0; i--)
		if (u.cmd.command[i] == '\015') {
			found = true;
			break;
		}

	if (found)
		return i;
	else
		return -1;
}


int find_next_cret(int pos) {
	char* cr;

	/* Find the first ^M to the right of pos. */
	cr = memchr(u.cmd.command + pos, '\015', u.cmd.length - pos);

	if (cr != NULL)
		return cr - u.cmd.command;
	else
		return -1;
}


/* Initialize all of the sequences.  Some of them are duplicated
   in different groups instead of referenced, which should be fixed. */
void init_seqs(void) {

	seq_groups = XMALLOC(struct sequence_group, NUM_PROCESS_LEVELS);

	seq_groups[PL_AT_PROMPT].n = 14;
	seq_groups[PL_AT_PROMPT].seqs = XMALLOC(Sequence, 14);

	seq_groups[PL_EXECUTING].n = 3;
	seq_groups[PL_EXECUTING].seqs = XMALLOC(Sequence, 3);

	seq_groups[PL_EATING].n = 1;
	seq_groups[PL_EATING].seqs = XMALLOC(Sequence, 1);

	int i;
	for (i = 0; i < seq_groups[PL_AT_PROMPT].n; i++) {
		seq_groups[PL_AT_PROMPT].seqs[i].pos = 0;
		seq_groups[PL_AT_PROMPT].seqs[i].enabled = false;
	}
	for (i = 0; i < seq_groups[PL_EXECUTING].n; i++) {
		seq_groups[PL_EXECUTING].seqs[i].pos = 0;
		seq_groups[PL_EXECUTING].seqs[i].enabled = false;
	}
	for (i = 0; i < seq_groups[PL_EATING].n; i++) {
		seq_groups[PL_EATING].seqs[i].pos = 0;
		seq_groups[PL_EATING].seqs[i].enabled = false;
	}

	/* PL_AT_PROMPT */
	strcpy(seq_groups[PL_AT_PROMPT].seqs[0].name, "PS1 separator");
	seq_groups[PL_AT_PROMPT].seqs[0].seq = PS1_SEPARATOR_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[0].length = strlen(PS1_SEPARATOR_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[0].func = seq_ps1_separator;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[1].name, "PS2 separator");
	seq_groups[PL_AT_PROMPT].seqs[1].seq = PS2_SEPARATOR_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[1].length = strlen(PS2_SEPARATOR_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[1].func = seq_ps2_separator;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[2].name, "Term CL wrapped");
	seq_groups[PL_AT_PROMPT].seqs[2].seq = TERM_CL_WRAPPED_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[2].length = strlen(TERM_CL_WRAPPED_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[2].func = seq_term_cl_wrapped;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[3].name, "Term cursor forward");
	seq_groups[PL_AT_PROMPT].seqs[3].seq = TERM_CURSOR_FORWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[3].length = strlen(TERM_CURSOR_FORWARD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[3].func = seq_term_cursor_forward;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[4].name, "Term backspace");
	seq_groups[PL_AT_PROMPT].seqs[4].seq = TERM_BACKSPACE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[4].length = strlen(TERM_BACKSPACE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[4].func = seq_term_backspace;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[5].name, "Term erase in line");
	seq_groups[PL_AT_PROMPT].seqs[5].seq = TERM_ERASE_IN_LINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[5].length = strlen(TERM_ERASE_IN_LINE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[5].func = seq_term_erase_in_line;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[6].name, "Term delete chars");
	seq_groups[PL_AT_PROMPT].seqs[6].seq = TERM_DELETE_CHARS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[6].length = strlen(TERM_DELETE_CHARS_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[6].func = seq_term_delete_chars;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[7].name, "Term insert blanks");
	seq_groups[PL_AT_PROMPT].seqs[7].seq = TERM_INSERT_BLANKS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[7].length = strlen(TERM_INSERT_BLANKS_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[7].func = seq_term_insert_blanks;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[8].name, "Term cursor backward");
	seq_groups[PL_AT_PROMPT].seqs[8].seq = TERM_CURSOR_BACKWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[8].length = strlen(TERM_CURSOR_BACKWARD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[8].func = seq_term_cursor_backward;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[9].name, "Term bell");
	seq_groups[PL_AT_PROMPT].seqs[9].seq = TERM_BELL_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[9].length = strlen(TERM_BELL_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[9].func = seq_term_bell;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[10].name, "Cursor up");
	seq_groups[PL_AT_PROMPT].seqs[10].seq = TERM_CURSOR_UP_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[10].length = strlen(TERM_CURSOR_UP_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[10].func = seq_term_cursor_up;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[11].name, "Carriage return");
	seq_groups[PL_AT_PROMPT].seqs[11].seq = TERM_CARRIAGE_RETURN_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[11].length = strlen(TERM_CARRIAGE_RETURN_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[11].func = seq_term_carriage_return;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[12].name, "Newline");
	seq_groups[PL_AT_PROMPT].seqs[12].seq = TERM_NEWLINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[12].length = strlen(TERM_NEWLINE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[12].func = seq_term_newline;

	strcpy(seq_groups[PL_AT_PROMPT].seqs[13].name, "Eat start");
	seq_groups[PL_AT_PROMPT].seqs[13].seq = EAT_START_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[13].length = strlen(EAT_START_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[13].func = seq_eat_start;


	/* PL_EXECUTING */
	strcpy(seq_groups[PL_EXECUTING].seqs[0].name, "PS1 separator");
	seq_groups[PL_EXECUTING].seqs[0].seq = PS1_SEPARATOR_SEQ;
	seq_groups[PL_EXECUTING].seqs[0].length = strlen(PS1_SEPARATOR_SEQ);
	seq_groups[PL_EXECUTING].seqs[0].func = seq_ps1_separator;

	strcpy(seq_groups[PL_EXECUTING].seqs[1].name, "PS2 separator");
	seq_groups[PL_EXECUTING].seqs[1].seq = PS2_SEPARATOR_SEQ;
	seq_groups[PL_EXECUTING].seqs[1].length = strlen(PS2_SEPARATOR_SEQ);
	seq_groups[PL_EXECUTING].seqs[1].func = seq_ps2_separator;

	strcpy(seq_groups[PL_EXECUTING].seqs[2].name, "Eat start");
	seq_groups[PL_EXECUTING].seqs[2].seq = EAT_START_SEQ;
	seq_groups[PL_EXECUTING].seqs[2].length = strlen(EAT_START_SEQ);
	seq_groups[PL_EXECUTING].seqs[2].func = seq_eat_start;


	/* PL_EATING */
	strcpy(seq_groups[PL_EATING].seqs[0].name, "Eat end");
	seq_groups[PL_EATING].seqs[0].seq = EAT_END_SEQ;
	seq_groups[PL_EATING].seqs[0].length = strlen(EAT_END_SEQ);
	seq_groups[PL_EATING].seqs[0].func = seq_eat_end;

	for (i = 0; i < seq_groups[PL_AT_PROMPT].n; i++)
		DEBUG((df,"%s (%d)\n", seq_groups[PL_AT_PROMPT].seqs[i].name, seq_groups[PL_AT_PROMPT].seqs[i].length));
}


static void disable_seq(Sequence* sq) {
	DEBUG((df, " (disabled)\n"));
	sq->enabled = false;
}


void enable_all_seqs(enum process_level pl) {
	int i;
	DEBUG((df, "(all enabled)\n"));
	for (i = 0; i < seq_groups[pl].n; i++)
		seq_groups[pl].seqs[i].enabled = true;
}



MatchStatus check_seqs(enum process_level pl, char c, MatchEffect* effect) {
	int i;
	MatchStatus status = MS_NO_MATCH;

	for (i = 0; i < seq_groups[pl].n; i++) {
		status |= check_seq(c, seq_groups[pl].seqs + i);
		if (status & MS_MATCH) {
			DEBUG((df, "Matched seq \"%s\" (%d)\n", seq_groups[pl].seqs[i].name, i));
			*effect = (*(seq_groups[pl].seqs[i].func))();		/* Execute the pattern match function. */
			break;
		}
	}

	return status;
}


/* Assumes that the chosen sequences are intelligently chosen
   and are not malformed.  To elaborate:
   	- Do not have a special character delimiter after a special character.
	- Do not end a sequence with a special character.

   This function is poorly written, but it works.  */
static MatchStatus check_seq(char c, Sequence* sq) {

	MatchStatus result = MS_IN_PROGRESS;

	/* This sequence is disabled right now -- assume no match. */
	if (sq->enabled == false) {
		DEBUG((df, "\t~~~"));
		return MS_NO_MATCH;
	}

	DEBUG((df, "\t<%s> %d %d\n", sq->seq, sq->pos, sq->length));

	switch (sq->seq[sq->pos]) {
		case DIGIT_C:
			if (isdigit(c))
				return MS_IN_PROGRESS;
			else if (sq->seq[sq->pos + 1] == c)		/* Skip over special char and delimiter. */
				sq->pos += 2;
			else {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			break;

		case PRINTABLE_C:
			if (isprint(c))
				return MS_IN_PROGRESS;
			else if (sq->seq[sq->pos + 1] == c)		/* Skip over special char and delimiter. */
				sq->pos += 2;
			else {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			break;

		case NOT_LF_C:
			if (c == '\n') {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			else {
				sq->pos++;
			}
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


void clear_seqs(enum process_level pl) {
	int i;
	for (i = 0; i < seq_groups[pl].n; i++)
		seq_groups[pl].seqs[i].pos = 0;
	return;
}



static MatchEffect seq_ps1_separator(void) {
	return ME_CMD_STARTED;
}


static MatchEffect seq_eat_start(void) {
	return ME_EAT_STARTED;
}


static MatchEffect seq_eat_end(void) {
	if (u.pwd != NULL)
		XFREE(u.pwd);

	/* Get the new current directory. */
	u.pwd = parse_printables(u.seqbuff.string, EAT_END_SEQ);

	return ME_PWD_CHANGED;
}


/* For now matching the PS2 separator means nothing; viewglob will
   not process multiline commands. */
static MatchEffect seq_ps2_separator(void) {
	return ME_CMD_EXECUTED;
}


/*
static MatchEffect seq_prompt_command(void) {

	if (u.pwd != NULL)
		XFREE(u.pwd);

	/ * Get the new current directory. * /
	u.pwd = parse_printables(u.seqbuff.string, PROMPT_COMMAND_SEQ);
	action_queue(A_SEND_PWD);

	return ME_PWD_CHANGED;
}
*/


/* Add a carriage return to the present location in the command line,
   or if we're expecting a newline, command executed. */
static MatchEffect seq_term_cl_wrapped(void) {

	if (u.expect_newline)
		return ME_CMD_EXECUTED;

	if (!cmd_overwrite_char('\015', false))
		return ME_ERROR;
	else
		return ME_NO_EFFECT;
}


/* Back up one character. */
static MatchEffect seq_term_backspace(void) {
	if (u.cmd.pos > 0)
		u.cmd.pos--;
	else
		return ME_ERROR;
	return ME_NO_EFFECT;
}


/* Move cursor forward from present location n times. */
static MatchEffect seq_term_cursor_forward(void) {
	int n;
	
	n = parse_digits(u.seqbuff.string);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (u.cmd.pos + n <= u.cmd.length)
		u.cmd.pos += n;
	else
		return ME_ERROR;
	return ME_NO_EFFECT;
}


/* Move cursor backward from present location n times. */
static MatchEffect seq_term_cursor_backward(void) {
	int n;
	
	n = parse_digits(u.seqbuff.string);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (u.cmd.pos - n >= 0)
		u.cmd.pos -= n;
	else
		return ME_ERROR;
	return ME_NO_EFFECT;
}


static MatchEffect seq_dummy(void) {
	DEBUG((df, "in seq_dummy\n"));
	return ME_DUMMY;
}


/* Delete n chars from the command line. */
static MatchEffect seq_term_delete_chars(void) {
	int n;
	
	n = parse_digits(u.seqbuff.string);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (cmd_del_chars(n))
		return ME_NO_EFFECT;
	else
		return ME_ERROR;
}

/* Insert n blanks at the current location on command line. */
static MatchEffect seq_term_insert_blanks(void) {
	int n;

	n = parse_digits(u.seqbuff.string);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (cmd_insert_chars(' ', n))
		return ME_NO_EFFECT;
	else
		return ME_ERROR;
}


/* Delete to the left or right of the cursor, or the whole line.
	0 = Clear to right
	1 = Clear to left
	2 = Clear all */
static MatchEffect seq_term_erase_in_line(void) {
	int n;
	
	/* Default is 0. */
	n = parse_digits(u.seqbuff.string);

	if (cmd_wipe_in_line(n))
		return ME_NO_EFFECT;
	else
		return ME_ERROR;
}


/* Just ignore the damn bell. */
static MatchEffect seq_term_bell(void) {
	return ME_NO_EFFECT;
}


/* Move cursor up from present location n times. */
static MatchEffect seq_term_cursor_up(void) {
	int i, n;
	int last_cret, next_cret, offset;
	int pos;

	n = parse_digits(u.seqbuff.string);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}

	last_cret = find_prev_cret(u.cmd.pos);
	next_cret = find_next_cret(u.cmd.pos);
	if (last_cret == -1 && next_cret == -1) {
		goto out_of_prompt;
	}

	/* First try to find the ^M at the beginning of the wanted line. */
	if (last_cret != -1) {
		DEBUG((df, "trying first\n"));

		pos = u.cmd.pos;
		for (i = 0; i < n + 1 && pos != -1; i++)
			pos = find_prev_cret(pos);

		if (pos != -1) {
			offset = u.cmd.pos - last_cret;    /* Cursor is offset chars from the beginning of the line. */
			u.cmd.pos = pos + offset;          /* Position cursor on new line at same position. */
			if (u.cmd.pos >= 0)
				return ME_NO_EFFECT;
			else
				goto out_of_prompt;
		}
	}

	/* That failed, so now try to find the ^M at the end of the wanted line. */
	if (next_cret != -1) {
		DEBUG((df, "trying second\n"));

		pos = u.cmd.pos;
		for (i = 0; i < n && pos != -1; i++)
			pos = find_prev_cret(pos);

		if (pos != -1) {
			offset = next_cret - u.cmd.pos;
			u.cmd.pos = pos - offset;
			if (u.cmd.pos >= 0)
				return ME_NO_EFFECT;
			else
				goto out_of_prompt;
		}
	}


	/* No luck. */
	out_of_prompt:
	u.cmd.pos = 0;
	return ME_CMD_REBUILD;
}


/* Return cursor to beginning of this line, or if expecting
   newline, command executed. */
static MatchEffect seq_term_carriage_return(void) {
	int p;

	if (u.expect_newline)
		return ME_CMD_EXECUTED;

	/* We don't want to pop off the character following the ^M. */
	chars_to_save = 1;	/* Kludge. */

	p = find_prev_cret(u.cmd.pos);
	if (p == -1) {
			u.cmd.pos = 0;
			return ME_CMD_REBUILD;
	}
	else {
		/* Go to the character just after the ^M. */
		u.cmd.pos = p + 1;
		return ME_NO_EFFECT;
	}
}


/* What this function returns depends on whether the user has
   pressed any of return (enter), Ctrl-C, or Ctrl-D or not.
   If so, then a newline that takes us out of the command-line
   is interpreted as a command execution.  If not, then it is
   a command-line wrap. */
static MatchEffect seq_term_newline(void) {
	int p;
	MatchEffect effect;

	DEBUG((df, "expect_newline: %d", u.expect_newline));

	p = find_next_cret(u.cmd.pos);
	if (p == -1) {
		if (u.expect_newline) {
			/* Command must have been executed. */
			effect = ME_CMD_EXECUTED;
		}
		else {
			/* Newline must just be a wrap. */
			u.cmd.pos = u.cmd.length;
			if (cmd_overwrite_char('\015', false))
				effect = ME_NO_EFFECT;
			else
				effect = ME_ERROR;
		}
	}
	else {
		u.cmd.pos = p + 1;
		effect = ME_NO_EFFECT;
	}
	return effect;
}
