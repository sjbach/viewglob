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
#include "actions.h"
#include "connection.h"
#include "sequences.h"

#include <string.h>
#include <ctype.h>

#define NUM_PROCESS_LEVELS 4

typedef struct _Sequence Sequence;
struct _Sequence {
	gchar* name;	            /* For debugging. */
	gchar* seq;
	gint length;
	gint pos;
	gboolean enabled;               /* Determines whether a match should be attempted. */
	MatchEffect (*func)(Connection* b, struct user_shell* u);  /* Function to run after successful match. */
};

/* Each process level (at prompt, executing, and eating) has a set of
   sequences that it checks for.  This struct is for these sets. */
typedef struct _SeqGroup SeqGroup;
struct _SeqGroup {
	gint n;                 /* Number of sequences. */
	Sequence* seqs;
};


/* Sequence parsing functions. */
static gint    parse_digits(const gchar* string, size_t len);
static gchar*  parse_printables(const gchar* string, const gchar* sequence);

static void init_bash_seqs(void);
static void init_zsh_seqs(void);
static MatchStatus check_seq(gchar c, Sequence* sq);
static void analyze_effect(MatchEffect effect, Connection* b, struct user_shell* u);

/* Pattern completion actions. */
static MatchEffect seq_ps1_separator(Connection* b, struct user_shell* u);
static MatchEffect seq_rprompt_separator_start(Connection* b, struct user_shell* u);
static MatchEffect seq_rprompt_separator_end(Connection* b, struct user_shell* u);
static MatchEffect seq_zsh_completion_done(Connection* b, struct user_shell* u);
static MatchEffect seq_new_pwd(Connection* b, struct user_shell* u);

static MatchEffect seq_nav_up(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_down(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_pgup(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_pgdown(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_ctrl_g(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_toggle(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_refocus(Connection* b, struct user_shell* u);
static MatchEffect seq_nav_disable(Connection* b, struct user_shell* u);
/*static MatchEffect seq_nav_catch_all(Connection* b, struct user_shell* u);*/

static MatchEffect seq_term_cmd_wrapped(Connection* b, struct user_shell* u);
static MatchEffect seq_term_backspace(Connection* b, struct user_shell* u);
static MatchEffect seq_term_cursor_forward(Connection* b, struct user_shell* u);
static MatchEffect seq_term_cursor_backward(Connection* b, struct user_shell* u);
static MatchEffect seq_term_cursor_up(Connection* b, struct user_shell* u);
static MatchEffect seq_term_erase_in_line(Connection* b, struct user_shell* u);
static MatchEffect seq_term_delete_chars(Connection* b, struct user_shell* u);
static MatchEffect seq_term_insert_blanks(Connection* b, struct user_shell* u);
static MatchEffect seq_term_bell(Connection* b, struct user_shell* u);
static MatchEffect seq_term_carriage_return(Connection* b, struct user_shell* u);
static MatchEffect seq_term_newline(Connection* b, struct user_shell* u);

/* The below characters don't appear in any of the sequences viewglob looks for, so we
   can use them as special characters. */
#define DIGIT_C '\021'            /* Any sequence of digits (or none). */
#define DIGIT_S "\021"
#define PRINTABLE_C '\022'        /* Any sequence of printable characters (or none). */
#define PRINTABLE_S "\022"
#define NOT_LF_CR_C '\023'        /* Any single non-linefeed, non-carriage return character. */
#define NOT_LF_CR_S "\023"
#define NOT_LF_C '\024'           /* Any single non-linefeed character. */
#define NOT_LF_S "\024"
/*#define ANY_C '\024'*/          /* Any single character. */
/*#define ANY_S "\024"*/

/* viewglob escape sequences.
   Note: if these are changed, the init-viewglob.*rc files must be updated. */
static gchar* const PS1_SEPARATOR_SEQ = "\033[0;30m\033[0m\033[1;37m\033[0m";
static gchar* const RPROMPT_SEPARATOR_START_SEQ = "\033[0;34m\033[0m\033[0;31m\033[0m";
static gchar* const RPROMPT_SEPARATOR_END_SEQ = "\033[0;34m\033[0m\033[0;31m\033[0m" "\033[" DIGIT_S "D";
static gchar* const NEW_PWD_SEQ = "\033P" PRINTABLE_S "\033\\";

/* viewglob navigation sequences. */
static gchar* const NAV_VI_UP_SEQ = "\007k";
static gchar* const NAV_C_VI_UP_SEQ = "\007\013";
static gchar* const NAV_VI_DOWN_SEQ = "\007j";
static gchar* const NAV_C_VI_DOWN_SEQ = "\007\012";
static gchar* const NAV_VI_PGUP_SEQ = "\007b";
static gchar* const NAV_C_VI_PGUP_SEQ = "\007\002";
static gchar* const NAV_VI_PGDOWN_SEQ = "\007f";
static gchar* const NAV_C_VI_PGDOWN_SEQ = "\007\006";
static gchar* const NAV_EMACS_UP_SEQ = "\007p";
static gchar* const NAV_C_EMACS_UP_SEQ = "\007\020";
static gchar* const NAV_EMACS_DOWN_SEQ = "\007n";
static gchar* const NAV_C_EMACS_DOWN_SEQ = "\007\020";
static gchar* const NAV_EMACS_PGUP_SEQ = "\007u";
static gchar* const NAV_C_EMACS_PGUP_SEQ = "\007\025";
static gchar* const NAV_EMACS_PGDOWN_SEQ = "\007d";
static gchar* const NAV_C_EMACS_PGDOWN_SEQ = "\007\004";
static gchar* const NAV_KEYBOARD_UP_SEQ = "\007\033[A";
static gchar* const NAV_C_KEYBOARD_UP_SEQ = "\007\033Oa";
static gchar* const NAV_KEYBOARD_DOWN_SEQ = "\007\033[B";
static gchar* const NAV_C_KEYBOARD_DOWN_SEQ = "\007\033Ob";
static gchar* const NAV_KEYBOARD_PGUP_SEQ = "\007\033[5~";
static gchar* const NAV_C_KEYBOARD_PGUP_SEQ = "\007\033[5^";
static gchar* const NAV_KEYBOARD_PGDOWN_SEQ = "\007\033[6~";
static gchar* const NAV_C_KEYBOARD_PGDOWN_SEQ = "\007\033[6^";
static gchar* const NAV_CTRL_G_SEQ = "\007\007";
static gchar* const NAV_TOGGLE_SEQ = "\007 ";
static gchar* const NAV_C_TOGGLE_SEQ = "\007\000";
static gchar* const NAV_REFOCUS_SEQ = "\007\t";
static gchar* const NAV_REFOCUS_ALT_SEQ = "\007\015";
static gchar* const NAV_DISABLE_SEQ = "\007q";
/*static gchar* const NAV_CATCH_ALL_SEQ = "\007" ANY_S "";*/

/* I've observed this always comes at the end of a list of tab completions in zsh. */
static gchar* const ZSH_COMPLETION_DONE_SEQ = "\033[0m\033[27m\033[24m\015\033[" DIGIT_S "C";

/* Terminal escape sequences that we have to watch for.
   There are many more, but in my observations only these are
   important for viewglob's purposes. */
static gchar* const TERM_CMD_WRAPPED_SEQ = " \015" NOT_LF_CR_S "";
static gchar* const TERM_CARRIAGE_RETURN_SEQ = "\015" NOT_LF_S "";
static gchar* const TERM_NEWLINE_SEQ = "\015\n";
static gchar* const TERM_BACKSPACE_SEQ  = "\010";
static gchar* const TERM_CURSOR_FORWARD_SEQ = "\033[" DIGIT_S "C";
static gchar* const TERM_CURSOR_BACKWARD_SEQ = "\033[" DIGIT_S "D";
static gchar* const TERM_CURSOR_UP_SEQ  = "\033[" DIGIT_S "A";
static gchar* const TERM_ERASE_IN_LINE_SEQ = "\033[" DIGIT_S "K";
static gchar* const TERM_DELETE_CHARS_SEQ = "\033[" DIGIT_S "P";
static gchar* const TERM_INSERT_BLANKS_SEQ = "\033[" DIGIT_S "@";
static gchar* const TERM_BELL_SEQ = "\007";


static SeqGroup* seq_groups;
static enum shell_type shell;


/* Grab the first set of digits from the string
   and convert into an integer. */
static gint parse_digits(const gchar* string, size_t len) {
	guint i;
	gboolean digits = FALSE;
	for (i = 0; string[i] != '\0' && i < len && !digits; i++) {
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

	/* Find the location of the printables in the sequence. */
	p = strchr(sequence, PRINTABLE_C);
	if (!p)
		return NULL;

	pos = p - sequence;
	delimiter = *(p + 1);

	for (nbytes = 0; *(string + pos + nbytes) != delimiter; nbytes++) ;
	printables = g_new(gchar, nbytes + 1);

	/* Make a copy. */
	(void)strncpy(printables, string + pos, nbytes);
	*(printables + nbytes) = '\0';
	return printables;
}

//FIXME remove find_prev_cret() and find_next_cret() and replace with g_strstrblah() etc.?

gint find_prev_cret(gchar* string, gint pos) {
	gint i;
	gboolean found = FALSE;

	/* Find the first ^M to the left of pos. */
	for (i = pos - 1; i >= 0; i--)
		if (string[i] == '\015') {
			found = TRUE;
			break;
		}

	if (found)
		return i;
	else
		return -1;
}


gint find_next_cret(gchar* string, gint length, gint pos) {
	gchar* cr;

	/* Find the first ^M to the right of pos. */
	cr = memchr(string + pos, '\015', length - pos);

	if (cr != NULL)
		return cr - string;
	else
		return -1;
}


/* Initialize all of the sequences.  Some of them are duplicated
   in different groups instead of referenced, which should be fixed. */
void init_seqs(enum shell_type shell) {
	gint i;
	seq_groups = g_new(SeqGroup, NUM_PROCESS_LEVELS);

	if (shell == ST_BASH)
		init_bash_seqs();
	else if (shell == ST_ZSH)
		init_zsh_seqs();
	else
		g_critical("Unexpected shell type");

	seq_groups[PL_TERMINAL].n = 30;
	seq_groups[PL_TERMINAL].seqs = g_new(Sequence, 30);
	for (i = 0; i < seq_groups[PL_TERMINAL].n; i++) {
		seq_groups[PL_TERMINAL].seqs[i].pos = 0;
		seq_groups[PL_TERMINAL].seqs[i].enabled = FALSE;
	}

	seq_groups[PL_TERMINAL].seqs[0].name = "Nav vi up";
	seq_groups[PL_TERMINAL].seqs[0].seq = NAV_VI_UP_SEQ;
	seq_groups[PL_TERMINAL].seqs[0].length = strlen(NAV_VI_UP_SEQ);
	seq_groups[PL_TERMINAL].seqs[0].func = seq_nav_up;

	seq_groups[PL_TERMINAL].seqs[1].name = "Nav vi down";
	seq_groups[PL_TERMINAL].seqs[1].seq = NAV_VI_DOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[1].length = strlen(NAV_VI_DOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[1].func = seq_nav_down;

	seq_groups[PL_TERMINAL].seqs[2].name = "Nav vi pgup";
	seq_groups[PL_TERMINAL].seqs[2].seq = NAV_VI_PGUP_SEQ;
	seq_groups[PL_TERMINAL].seqs[2].length = strlen(NAV_VI_PGUP_SEQ);
	seq_groups[PL_TERMINAL].seqs[2].func = seq_nav_pgup;

	seq_groups[PL_TERMINAL].seqs[3].name = "Nav vi pgdown";
	seq_groups[PL_TERMINAL].seqs[3].seq = NAV_VI_PGDOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[3].length = strlen(NAV_VI_PGDOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[3].func = seq_nav_pgdown;

	seq_groups[PL_TERMINAL].seqs[4].name = "Nav emacs up";
	seq_groups[PL_TERMINAL].seqs[4].seq = NAV_EMACS_UP_SEQ;
	seq_groups[PL_TERMINAL].seqs[4].length = strlen(NAV_EMACS_UP_SEQ);
	seq_groups[PL_TERMINAL].seqs[4].func = seq_nav_up;

	seq_groups[PL_TERMINAL].seqs[5].name = "Nav emacs down";
	seq_groups[PL_TERMINAL].seqs[5].seq = NAV_EMACS_DOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[5].length = strlen(NAV_EMACS_DOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[5].func = seq_nav_down;

	seq_groups[PL_TERMINAL].seqs[6].name = "Nav emacs pgup";
	seq_groups[PL_TERMINAL].seqs[6].seq = NAV_EMACS_PGUP_SEQ;
	seq_groups[PL_TERMINAL].seqs[6].length = strlen(NAV_EMACS_PGUP_SEQ);
	seq_groups[PL_TERMINAL].seqs[6].func = seq_nav_pgup;

	seq_groups[PL_TERMINAL].seqs[7].name = "Nav emacs pgdown";
	seq_groups[PL_TERMINAL].seqs[7].seq = NAV_EMACS_PGDOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[7].length = strlen(NAV_EMACS_PGDOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[7].func = seq_nav_pgdown;

	seq_groups[PL_TERMINAL].seqs[8].name = "Nav keyboard up";
	seq_groups[PL_TERMINAL].seqs[8].seq = NAV_KEYBOARD_UP_SEQ;
	seq_groups[PL_TERMINAL].seqs[8].length = strlen(NAV_KEYBOARD_UP_SEQ);
	seq_groups[PL_TERMINAL].seqs[8].func = seq_nav_up;

	seq_groups[PL_TERMINAL].seqs[9].name = "Nav keyboard down";
	seq_groups[PL_TERMINAL].seqs[9].seq = NAV_KEYBOARD_DOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[9].length = strlen(NAV_KEYBOARD_DOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[9].func = seq_nav_down;

	seq_groups[PL_TERMINAL].seqs[10].name = "Nav keyboard pgup";
	seq_groups[PL_TERMINAL].seqs[10].seq = NAV_KEYBOARD_PGUP_SEQ;
	seq_groups[PL_TERMINAL].seqs[10].length = strlen(NAV_KEYBOARD_PGUP_SEQ);
	seq_groups[PL_TERMINAL].seqs[10].func = seq_nav_pgup;

	seq_groups[PL_TERMINAL].seqs[11].name = "Nav keyboard pgdown";
	seq_groups[PL_TERMINAL].seqs[11].seq = NAV_KEYBOARD_PGDOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[11].length = strlen(NAV_KEYBOARD_PGDOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[11].func = seq_nav_pgdown;

	seq_groups[PL_TERMINAL].seqs[12].name = "Nav ctrl g";
	seq_groups[PL_TERMINAL].seqs[12].seq = NAV_CTRL_G_SEQ;
	seq_groups[PL_TERMINAL].seqs[12].length = strlen(NAV_CTRL_G_SEQ);
	seq_groups[PL_TERMINAL].seqs[12].func = seq_nav_ctrl_g;

	seq_groups[PL_TERMINAL].seqs[13].name = "Nav C vi up";
	seq_groups[PL_TERMINAL].seqs[13].seq = NAV_C_VI_UP_SEQ;
	seq_groups[PL_TERMINAL].seqs[13].length = strlen(NAV_C_VI_UP_SEQ);
	seq_groups[PL_TERMINAL].seqs[13].func = seq_nav_up;

	seq_groups[PL_TERMINAL].seqs[14].name = "Nav C vi down";
	seq_groups[PL_TERMINAL].seqs[14].seq = NAV_C_VI_DOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[14].length = strlen(NAV_C_VI_DOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[14].func = seq_nav_down;

	seq_groups[PL_TERMINAL].seqs[15].name = "Nav C vi pgup";
	seq_groups[PL_TERMINAL].seqs[15].seq = NAV_C_VI_PGUP_SEQ;
	seq_groups[PL_TERMINAL].seqs[15].length = strlen(NAV_C_VI_PGUP_SEQ);
	seq_groups[PL_TERMINAL].seqs[15].func = seq_nav_pgup;

	seq_groups[PL_TERMINAL].seqs[16].name = "Nav C vi pgdown";
	seq_groups[PL_TERMINAL].seqs[16].seq = NAV_C_VI_PGDOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[16].length = strlen(NAV_C_VI_PGDOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[16].func = seq_nav_pgdown;

	seq_groups[PL_TERMINAL].seqs[17].name = "Nav C emacs up";
	seq_groups[PL_TERMINAL].seqs[17].seq = NAV_C_EMACS_UP_SEQ;
	seq_groups[PL_TERMINAL].seqs[17].length = strlen(NAV_C_EMACS_UP_SEQ);
	seq_groups[PL_TERMINAL].seqs[17].func = seq_nav_up;

	seq_groups[PL_TERMINAL].seqs[18].name = "Nav C emacs down";
	seq_groups[PL_TERMINAL].seqs[18].seq = NAV_C_EMACS_DOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[18].length = strlen(NAV_C_EMACS_DOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[18].func = seq_nav_down;

	seq_groups[PL_TERMINAL].seqs[19].name = "Nav C emacs pgup";
	seq_groups[PL_TERMINAL].seqs[19].seq = NAV_C_EMACS_PGUP_SEQ;
	seq_groups[PL_TERMINAL].seqs[19].length = strlen(NAV_C_EMACS_PGUP_SEQ);
	seq_groups[PL_TERMINAL].seqs[19].func = seq_nav_pgup;

	seq_groups[PL_TERMINAL].seqs[20].name = "Nav C emacs pgdown";
	seq_groups[PL_TERMINAL].seqs[20].seq = NAV_C_EMACS_PGDOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[20].length = strlen(NAV_C_EMACS_PGDOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[20].func = seq_nav_pgdown;

	seq_groups[PL_TERMINAL].seqs[21].name = "Nav C keyboard up";
	seq_groups[PL_TERMINAL].seqs[21].seq = NAV_C_KEYBOARD_UP_SEQ;
	seq_groups[PL_TERMINAL].seqs[21].length = strlen(NAV_C_KEYBOARD_UP_SEQ);
	seq_groups[PL_TERMINAL].seqs[21].func = seq_nav_up;

	seq_groups[PL_TERMINAL].seqs[22].name = "Nav C keyboard down";
	seq_groups[PL_TERMINAL].seqs[22].seq = NAV_C_KEYBOARD_DOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[22].length = strlen(NAV_C_KEYBOARD_DOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[22].func = seq_nav_down;

	seq_groups[PL_TERMINAL].seqs[23].name = "Nav C keyboard pgup";
	seq_groups[PL_TERMINAL].seqs[23].seq = NAV_C_KEYBOARD_PGUP_SEQ;
	seq_groups[PL_TERMINAL].seqs[23].length = strlen(NAV_C_KEYBOARD_PGUP_SEQ);
	seq_groups[PL_TERMINAL].seqs[23].func = seq_nav_pgup;

	seq_groups[PL_TERMINAL].seqs[24].name = "Nav C keyboard pgdown";
	seq_groups[PL_TERMINAL].seqs[24].seq = NAV_C_KEYBOARD_PGDOWN_SEQ;
	seq_groups[PL_TERMINAL].seqs[24].length = strlen(NAV_C_KEYBOARD_PGDOWN_SEQ);
	seq_groups[PL_TERMINAL].seqs[24].func = seq_nav_pgdown;

	seq_groups[PL_TERMINAL].seqs[25].name = "Nav toggle";
	seq_groups[PL_TERMINAL].seqs[25].seq = NAV_TOGGLE_SEQ;
	seq_groups[PL_TERMINAL].seqs[25].length = strlen(NAV_TOGGLE_SEQ);
	seq_groups[PL_TERMINAL].seqs[25].func = seq_nav_toggle;

	/* Need to add 1 to the strlen here because we're matching on NUL. */
	seq_groups[PL_TERMINAL].seqs[26].name = "Nav C toggle";
	seq_groups[PL_TERMINAL].seqs[26].seq = NAV_C_TOGGLE_SEQ;
	seq_groups[PL_TERMINAL].seqs[26].length = strlen(NAV_C_TOGGLE_SEQ) + 1;
	seq_groups[PL_TERMINAL].seqs[26].func = seq_nav_toggle;

	seq_groups[PL_TERMINAL].seqs[27].name = "Nav disable";
	seq_groups[PL_TERMINAL].seqs[27].seq = NAV_DISABLE_SEQ;
	seq_groups[PL_TERMINAL].seqs[27].length = strlen(NAV_DISABLE_SEQ);
	seq_groups[PL_TERMINAL].seqs[27].func = seq_nav_disable;

	seq_groups[PL_TERMINAL].seqs[28].name = "Nav refocus";
	seq_groups[PL_TERMINAL].seqs[28].seq = NAV_REFOCUS_SEQ;
	seq_groups[PL_TERMINAL].seqs[28].length = strlen(NAV_REFOCUS_SEQ);
	seq_groups[PL_TERMINAL].seqs[28].func = seq_nav_refocus;

	seq_groups[PL_TERMINAL].seqs[29].name = "Nav refocus alt";
	seq_groups[PL_TERMINAL].seqs[29].seq = NAV_REFOCUS_ALT_SEQ;
	seq_groups[PL_TERMINAL].seqs[29].length = strlen(NAV_REFOCUS_ALT_SEQ);
	seq_groups[PL_TERMINAL].seqs[29].func = seq_nav_refocus;

	/*
	seq_groups[PL_TERMINAL].seqs[13].name = "Nav catch all";
	seq_groups[PL_TERMINAL].seqs[13].seq = NAV_CATCH_ALL_SEQ;
	seq_groups[PL_TERMINAL].seqs[13].length = strlen(NAV_CATCH_ALL_SEQ);
	seq_groups[PL_TERMINAL].seqs[13].func = seq_nav_catch_all;
	*/

#if DEBUG_ON
	for (i = 0; i < seq_groups[PL_AT_PROMPT].n; i++)
		DEBUG((df,"%s (%d)\n", seq_groups[PL_AT_PROMPT].seqs[i].name, seq_groups[PL_AT_PROMPT].seqs[i].length));
#endif
}


static void init_bash_seqs(void) {

	shell = ST_BASH;

	seq_groups[PL_AT_PROMPT].n = 13;
	seq_groups[PL_AT_PROMPT].seqs = g_new(Sequence, 13);

	seq_groups[PL_EXECUTING].n = 2;
	seq_groups[PL_EXECUTING].seqs = g_new(Sequence, 2);

	gint i;
	for (i = 0; i < seq_groups[PL_AT_PROMPT].n; i++) {
		seq_groups[PL_AT_PROMPT].seqs[i].pos = 0;
		seq_groups[PL_AT_PROMPT].seqs[i].enabled = FALSE;
	}
	for (i = 0; i < seq_groups[PL_EXECUTING].n; i++) {
		seq_groups[PL_EXECUTING].seqs[i].pos = 0;
		seq_groups[PL_EXECUTING].seqs[i].enabled = FALSE;
	}

	/* PL_AT_PROMPT */
	seq_groups[PL_AT_PROMPT].seqs[0].name = "PS1 separator";
	seq_groups[PL_AT_PROMPT].seqs[0].seq = PS1_SEPARATOR_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[0].length = strlen(PS1_SEPARATOR_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[0].func = seq_ps1_separator;

	seq_groups[PL_AT_PROMPT].seqs[1].name = "New pwd";
	seq_groups[PL_AT_PROMPT].seqs[1].seq = NEW_PWD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[1].length = strlen(NEW_PWD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[1].func = seq_new_pwd;

	seq_groups[PL_AT_PROMPT].seqs[2].name = "Term cmd wrapped";
	seq_groups[PL_AT_PROMPT].seqs[2].seq = TERM_CMD_WRAPPED_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[2].length = strlen(TERM_CMD_WRAPPED_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[2].func = seq_term_cmd_wrapped;

	seq_groups[PL_AT_PROMPT].seqs[3].name = "Term cursor forward";
	seq_groups[PL_AT_PROMPT].seqs[3].seq = TERM_CURSOR_FORWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[3].length = strlen(TERM_CURSOR_FORWARD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[3].func = seq_term_cursor_forward;

	seq_groups[PL_AT_PROMPT].seqs[4].name = "Term backspace";
	seq_groups[PL_AT_PROMPT].seqs[4].seq = TERM_BACKSPACE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[4].length = strlen(TERM_BACKSPACE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[4].func = seq_term_backspace;

	seq_groups[PL_AT_PROMPT].seqs[5].name = "Term erase in line";
	seq_groups[PL_AT_PROMPT].seqs[5].seq = TERM_ERASE_IN_LINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[5].length = strlen(TERM_ERASE_IN_LINE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[5].func = seq_term_erase_in_line;

	seq_groups[PL_AT_PROMPT].seqs[6].name = "Term delete chars";
	seq_groups[PL_AT_PROMPT].seqs[6].seq = TERM_DELETE_CHARS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[6].length = strlen(TERM_DELETE_CHARS_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[6].func = seq_term_delete_chars;

	seq_groups[PL_AT_PROMPT].seqs[7].name = "Term insert blanks";
	seq_groups[PL_AT_PROMPT].seqs[7].seq = TERM_INSERT_BLANKS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[7].length = strlen(TERM_INSERT_BLANKS_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[7].func = seq_term_insert_blanks;

	seq_groups[PL_AT_PROMPT].seqs[8].name = "Term cursor backward";
	seq_groups[PL_AT_PROMPT].seqs[8].seq = TERM_CURSOR_BACKWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[8].length = strlen(TERM_CURSOR_BACKWARD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[8].func = seq_term_cursor_backward;

	seq_groups[PL_AT_PROMPT].seqs[9].name = "Term bell";
	seq_groups[PL_AT_PROMPT].seqs[9].seq = TERM_BELL_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[9].length = strlen(TERM_BELL_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[9].func = seq_term_bell;

	seq_groups[PL_AT_PROMPT].seqs[10].name = "Term cursor up";
	seq_groups[PL_AT_PROMPT].seqs[10].seq = TERM_CURSOR_UP_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[10].length = strlen(TERM_CURSOR_UP_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[10].func = seq_term_cursor_up;

	seq_groups[PL_AT_PROMPT].seqs[11].name = "Term carriage return";
	seq_groups[PL_AT_PROMPT].seqs[11].seq = TERM_CARRIAGE_RETURN_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[11].length = strlen(TERM_CARRIAGE_RETURN_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[11].func = seq_term_carriage_return;

	seq_groups[PL_AT_PROMPT].seqs[12].name = "Term newline";
	seq_groups[PL_AT_PROMPT].seqs[12].seq = TERM_NEWLINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[12].length = strlen(TERM_NEWLINE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[12].func = seq_term_newline;

	/* PL_EXECUTING */
	seq_groups[PL_EXECUTING].seqs[0].name = "PS1 separator";
	seq_groups[PL_EXECUTING].seqs[0].seq = PS1_SEPARATOR_SEQ;
	seq_groups[PL_EXECUTING].seqs[0].length = strlen(PS1_SEPARATOR_SEQ);
	seq_groups[PL_EXECUTING].seqs[0].func = seq_ps1_separator;

	seq_groups[PL_EXECUTING].seqs[1].name = "New pwd";
	seq_groups[PL_EXECUTING].seqs[1].seq = NEW_PWD_SEQ;
	seq_groups[PL_EXECUTING].seqs[1].length = strlen(NEW_PWD_SEQ);
	seq_groups[PL_EXECUTING].seqs[1].func = seq_new_pwd;
}


static void init_zsh_seqs(void) {

	shell = ST_ZSH;

	seq_groups[PL_AT_PROMPT].n = 14;
	seq_groups[PL_AT_PROMPT].seqs = g_new(Sequence, 14);

	seq_groups[PL_EXECUTING].n = 4;
	seq_groups[PL_EXECUTING].seqs = g_new(Sequence, 4);

	seq_groups[PL_AT_RPROMPT].n = 1;
	seq_groups[PL_AT_RPROMPT].seqs = g_new(Sequence, 1);

	gint i;
	for (i = 0; i < seq_groups[PL_AT_PROMPT].n; i++) {
		seq_groups[PL_AT_PROMPT].seqs[i].pos = 0;
		seq_groups[PL_AT_PROMPT].seqs[i].enabled = FALSE;
	}
	for (i = 0; i < seq_groups[PL_EXECUTING].n; i++) {
		seq_groups[PL_EXECUTING].seqs[i].pos = 0;
		seq_groups[PL_EXECUTING].seqs[i].enabled = FALSE;
	}
	for (i = 0; i < seq_groups[PL_AT_RPROMPT].n; i++) {
		seq_groups[PL_AT_RPROMPT].seqs[i].pos = 0;
		seq_groups[PL_AT_RPROMPT].seqs[i].enabled = FALSE;
	}

	/* PL_AT_PROMPT */
	seq_groups[PL_AT_PROMPT].seqs[0].name = "PS1 separator";
	seq_groups[PL_AT_PROMPT].seqs[0].seq = PS1_SEPARATOR_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[0].length = strlen(PS1_SEPARATOR_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[0].func = seq_ps1_separator;

	seq_groups[PL_AT_PROMPT].seqs[1].name = "RPROMPT separator start";
	seq_groups[PL_AT_PROMPT].seqs[1].seq = RPROMPT_SEPARATOR_START_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[1].length = strlen(RPROMPT_SEPARATOR_START_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[1].func = seq_rprompt_separator_start;

	seq_groups[PL_AT_PROMPT].seqs[2].name = "Term cmd wrapped";
	seq_groups[PL_AT_PROMPT].seqs[2].seq = TERM_CMD_WRAPPED_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[2].length = strlen(TERM_CMD_WRAPPED_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[2].func = seq_term_cmd_wrapped;

	seq_groups[PL_AT_PROMPT].seqs[3].name = "Term cursor forward";
	seq_groups[PL_AT_PROMPT].seqs[3].seq = TERM_CURSOR_FORWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[3].length = strlen(TERM_CURSOR_FORWARD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[3].func = seq_term_cursor_forward;

	seq_groups[PL_AT_PROMPT].seqs[4].name = "Term backspace";
	seq_groups[PL_AT_PROMPT].seqs[4].seq = TERM_BACKSPACE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[4].length = strlen(TERM_BACKSPACE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[4].func = seq_term_backspace;

	seq_groups[PL_AT_PROMPT].seqs[5].name = "Term erase in line";
	seq_groups[PL_AT_PROMPT].seqs[5].seq = TERM_ERASE_IN_LINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[5].length = strlen(TERM_ERASE_IN_LINE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[5].func = seq_term_erase_in_line;

	seq_groups[PL_AT_PROMPT].seqs[6].name = "Term delete chars";
	seq_groups[PL_AT_PROMPT].seqs[6].seq = TERM_DELETE_CHARS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[6].length = strlen(TERM_DELETE_CHARS_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[6].func = seq_term_delete_chars;

	seq_groups[PL_AT_PROMPT].seqs[7].name = "Term insert blanks";
	seq_groups[PL_AT_PROMPT].seqs[7].seq = TERM_INSERT_BLANKS_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[7].length = strlen(TERM_INSERT_BLANKS_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[7].func = seq_term_insert_blanks;

	seq_groups[PL_AT_PROMPT].seqs[8].name = "Term cursor backward";
	seq_groups[PL_AT_PROMPT].seqs[8].seq = TERM_CURSOR_BACKWARD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[8].length = strlen(TERM_CURSOR_BACKWARD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[8].func = seq_term_cursor_backward;

	seq_groups[PL_AT_PROMPT].seqs[9].name = "Term bell";
	seq_groups[PL_AT_PROMPT].seqs[9].seq = TERM_BELL_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[9].length = strlen(TERM_BELL_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[9].func = seq_term_bell;

	seq_groups[PL_AT_PROMPT].seqs[10].name = "Cursor up";
	seq_groups[PL_AT_PROMPT].seqs[10].seq = TERM_CURSOR_UP_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[10].length = strlen(TERM_CURSOR_UP_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[10].func = seq_term_cursor_up;

	seq_groups[PL_AT_PROMPT].seqs[11].name = "Carriage return";
	seq_groups[PL_AT_PROMPT].seqs[11].seq = TERM_CARRIAGE_RETURN_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[11].length = strlen(TERM_CARRIAGE_RETURN_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[11].func = seq_term_carriage_return;

	seq_groups[PL_AT_PROMPT].seqs[12].name = "Newline";
	seq_groups[PL_AT_PROMPT].seqs[12].seq = TERM_NEWLINE_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[12].length = strlen(TERM_NEWLINE_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[12].func = seq_term_newline;

	seq_groups[PL_AT_PROMPT].seqs[13].name = "New pwd";
	seq_groups[PL_AT_PROMPT].seqs[13].seq = NEW_PWD_SEQ;
	seq_groups[PL_AT_PROMPT].seqs[13].length = strlen(NEW_PWD_SEQ);
	seq_groups[PL_AT_PROMPT].seqs[13].func = seq_new_pwd;

	/* PL_EXECUTING */
	seq_groups[PL_EXECUTING].seqs[0].name = "PS1 separator";
	seq_groups[PL_EXECUTING].seqs[0].seq = PS1_SEPARATOR_SEQ;
	seq_groups[PL_EXECUTING].seqs[0].length = strlen(PS1_SEPARATOR_SEQ);
	seq_groups[PL_EXECUTING].seqs[0].func = seq_ps1_separator;

	seq_groups[PL_EXECUTING].seqs[1].name = "RPROMPT separator end";
	seq_groups[PL_EXECUTING].seqs[1].seq = RPROMPT_SEPARATOR_END_SEQ;
	seq_groups[PL_EXECUTING].seqs[1].length = strlen(RPROMPT_SEPARATOR_END_SEQ);
	seq_groups[PL_EXECUTING].seqs[1].func = seq_rprompt_separator_end;

	seq_groups[PL_EXECUTING].seqs[2].name = "New pwd";
	seq_groups[PL_EXECUTING].seqs[2].seq = NEW_PWD_SEQ;
	seq_groups[PL_EXECUTING].seqs[2].length = strlen(NEW_PWD_SEQ);
	seq_groups[PL_EXECUTING].seqs[2].func = seq_new_pwd;

	seq_groups[PL_EXECUTING].seqs[3].name = "Zsh completion done";
	seq_groups[PL_EXECUTING].seqs[3].seq = ZSH_COMPLETION_DONE_SEQ;
	seq_groups[PL_EXECUTING].seqs[3].length = strlen(ZSH_COMPLETION_DONE_SEQ);
	seq_groups[PL_EXECUTING].seqs[3].func = seq_zsh_completion_done;

	/* PL_AT_RPROMPT */
	seq_groups[PL_AT_RPROMPT].seqs[0].name = "RPROMPT separator end";
	seq_groups[PL_AT_RPROMPT].seqs[0].seq = RPROMPT_SEPARATOR_END_SEQ;
	seq_groups[PL_AT_RPROMPT].seqs[0].length = strlen(RPROMPT_SEPARATOR_END_SEQ);
	seq_groups[PL_AT_RPROMPT].seqs[0].func = seq_rprompt_separator_end;
}


static void disable_seq(Sequence* sq) {
	DEBUG((df, " (disabled)\n"));
	sq->enabled = FALSE;
}


void enable_all_seqs(enum process_level pl) {
	gint i;
	DEBUG((df, "(all enabled)\n"));
	for (i = 0; i < seq_groups[pl].n; i++)
		seq_groups[pl].seqs[i].enabled = TRUE;
}



void check_seqs(Connection* b, struct user_shell* u) {
	gint i;
	MatchEffect effect;

	gchar c = b->buf[b->pos + (b->n - 1)];
	SeqGroup seq_group = seq_groups[b->pl];

	b->status = MS_NO_MATCH;
	for (i = 0; i < seq_group.n; i++) {

		Sequence* seq = seq_group.seqs + i;

		b->status |= check_seq(c, seq);
		if (b->status & MS_MATCH) {

			DEBUG((df, "Matched seq \"%s\" (%d)\n", seq->name, i));

			/* Execute the pattern match function and analyze. */
			effect = (*(seq->func))(b, u);
			//FIXME return error value from analyze effect!
			analyze_effect(effect, b, u);

			break;
		}
	}
}


/* Assumes that the chosen sequences are intelligently chosen
   and are not malformed.  To elaborate:
   	- Do not have a special character delimiter after a special character.
	- Do not end a sequence with a special character.

   This function is poorly written, but it works.  */
static MatchStatus check_seq(gchar c, Sequence* sq) {

	MatchStatus result = MS_IN_PROGRESS;

	/* This sequence is disabled right now -- assume no match. */
	if (sq->enabled == FALSE) {
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
			if (c == '\012') {
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			else
				sq->pos++;
			break;

		case NOT_LF_CR_C:
			if (c == '\012' || c == '\015') {   /* Linefeed or carriage return. */
				sq->pos = 0;
				disable_seq(sq);
				return MS_NO_MATCH;
			}
			else
				sq->pos++;
			break;

#if 0
		case ANY_C:
			/* Anything goes. */
			sq->pos++;
			break;
#endif

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
/* FIXME probably should return a gboolean for graceful crash. */
static void analyze_effect(MatchEffect effect, Connection* b, struct user_shell* u) {

	switch (effect) {

		case ME_ERROR:
			DEBUG((df, "**ERROR**\n"));
			/* Clear the command line to indicate we're out of sync,
			   and wait for the next PS1. */
			cmd_clear(&u->cmd);
			b->pl = PL_EXECUTING;
			action_queue(A_SEND_LOST);
			break;

		case ME_NO_EFFECT:
			DEBUG((df, "**NO_EFFECT**\n"));
			break;

		case ME_CMD_EXECUTED:
			DEBUG((df, "**CMD_EXECUTED**\n"));
			b->pl = PL_EXECUTING;
			break;

		case ME_CMD_STARTED:
			DEBUG((df, "**CMD_STARTED**\n"));
			if (u->cmd.rebuilding)
				u->cmd.rebuilding = FALSE;
			else
				cmd_clear(&u->cmd);
			b->pl = PL_AT_PROMPT;
			action_queue(A_SEND_CMD);
			break;

		case ME_CMD_REBUILD:
			DEBUG((df, "**CMD_REBUILD**\n"));
			u->cmd.rebuilding = TRUE;
			b->pl = PL_EXECUTING;
			break;

		case ME_PWD_CHANGED:
			/* Send the new current directory. */
			DEBUG((df, "**PWD_CHANGED**\n"));
			action_queue(A_SEND_PWD);
			break;

		case ME_RPROMPT_STARTED:
			DEBUG((df, "**RPROMPT_STARTED**\n"));
			u->cmd.rebuilding = TRUE;
			b->pl = PL_AT_RPROMPT;
			break;

		default:
			/* Error -- this shouldn't happen unless I've screwed up */
			g_return_if_reached();
			break;
	}
}


void clear_seqs(enum process_level pl) {
	gint i;
	for (i = 0; i < seq_groups[pl].n; i++)
		seq_groups[pl].seqs[i].pos = 0;
	return;
}



static MatchEffect seq_ps1_separator(Connection* b, struct user_shell* u) {
	pass_segment(b);
	return ME_CMD_STARTED;
}


static MatchEffect seq_rprompt_separator_start(Connection* b, struct user_shell*  u) {
	pass_segment(b);
	return ME_RPROMPT_STARTED;
}


static MatchEffect seq_rprompt_separator_end(Connection* b, struct user_shell*  u) {
	pass_segment(b);
	return ME_CMD_STARTED;
}


static MatchEffect seq_new_pwd(Connection* b, struct user_shell*  u) {
	if (u->pwd != NULL)
		g_free(u->pwd);

	u->pwd = parse_printables(b->buf + b->pos, NEW_PWD_SEQ);
	DEBUG((df, "new pwd: %s\n", u->pwd));
	eat_segment(b);
	return ME_PWD_CHANGED;

}


static MatchEffect seq_zsh_completion_done(Connection* b, struct user_shell*  u) {
	u->cmd.rebuilding = TRUE;
	pass_segment(b);
	return ME_NO_EFFECT;
}


/* Add a carriage return to the present location in the command line,
   or if we're expecting a newline, command executed. */
static MatchEffect seq_term_cmd_wrapped(Connection* b, struct user_shell*  u) {
	MatchEffect effect;

	if (u->expect_newline)
		effect = ME_CMD_EXECUTED;
	else if (!cmd_overwrite_char(&u->cmd, '\015', FALSE))
		effect = ME_ERROR;
	else
		effect = ME_NO_EFFECT;

	/* Don't want to pass over the NOT_LF char. */
	b->n--;
	pass_segment(b);
	return effect;
}


/* Back up one character. */
static MatchEffect seq_term_backspace(Connection* b, struct user_shell*  u) {
	MatchEffect effect;

	if (u->cmd.pos > 0) {
		u->cmd.pos--;
		effect = ME_NO_EFFECT;
	}
	else
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Move cursor forward from present location n times. */
static MatchEffect seq_term_cursor_forward(Connection* b, struct user_shell* u) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	n = parse_digits(b->buf + b->pos, b->n);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (u->cmd.pos + n <= u->cmd.data->len)
		u->cmd.pos += n;
	else {
		if (shell == ST_ZSH) {
			if (u->cmd.pos + n == u->cmd.data->len + 1) {
				/* This is more likely to just be a space causing a deletion of the RPROMPT in zsh. */
				cmd_overwrite_char(&u->cmd, ' ', FALSE);
			}
			else {
				/* It's writing the RPROMPT. */
				u->cmd.rebuilding = TRUE;
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
static MatchEffect seq_term_cursor_backward(Connection* b, struct user_shell*  u) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	n = parse_digits(b->buf + b->pos, b->n);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (u->cmd.pos - n >= 0)
		u->cmd.pos -= n;
	else
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Delete n chars from the command line. */
static MatchEffect seq_term_delete_chars(Connection* b, struct user_shell*  u) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	n = parse_digits(b->buf + b->pos, b->n);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (!cmd_del_chars(&u->cmd, n))
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}

/* Insert n blanks at the current location on command line. */
static MatchEffect seq_term_insert_blanks(Connection* b, struct user_shell*  u) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;

	n = parse_digits(b->buf + b->pos, b->n);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}
	if (!cmd_insert_chars(&u->cmd, ' ', n))
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Delete to the left or right of the cursor, or the whole line.
	0 = Clear to right
	1 = Clear to left
	2 = Clear all */
static MatchEffect seq_term_erase_in_line(Connection* b, struct user_shell*  u) {
	MatchEffect effect = ME_NO_EFFECT;
	gint n;
	
	/* Default is 0. */
	n = parse_digits(b->buf + b->pos, b->n);

	if (!cmd_wipe_in_line(&u->cmd, n))
		effect = ME_ERROR;

	pass_segment(b);
	return effect;
}


/* Just ignore the damn bell. */
static MatchEffect seq_term_bell(Connection* b, struct user_shell*  u) {
	pass_segment(b);
	return ME_NO_EFFECT;
}


/* Move cursor up from present location n times. */
static MatchEffect seq_term_cursor_up(Connection* b, struct user_shell*  u) {
	MatchEffect effect = ME_NO_EFFECT;
	gint i, n;
	gint offset;
	gchar* last_cr_p;
	gchar* next_cr_p;
	gchar* pos;

	n = parse_digits(b->buf + b->pos, b->n);
	if (n == 0) {
		/* Default is 1. */
		n = 1;
	}

	last_cr_p = g_strrstr_len(u->cmd.data->str, u->cmd.pos, "\015");
	next_cr_p = g_strstr_len(u->cmd.data->str + u->cmd.pos, u->cmd.data->len - u->cmd.pos, "\015");
	if (last_cr_p == NULL && next_cr_p == NULL)
		goto out_of_prompt;

	/* First try to find the ^M at the beginning of the wanted line. */
	if (last_cr_p != NULL) {
		DEBUG((df, "trying first\n"));

		pos = u->cmd.data->str + u->cmd.pos;
		for (i = 0; i < n + 1 && pos != NULL; i++)
			pos = g_strrstr_len(u->cmd.data->str, pos - u->cmd.data->str, "\015");

		if (pos != NULL) {
			/* Cursor is offset chars from the beginning of the line. */
			offset = u->cmd.data->str + u->cmd.pos - last_cr_p;
			/* Position cursor on new line at same position. */
			u->cmd.pos = pos + offset - u->cmd.data->str;
			if (u->cmd.pos >= 0) {
				effect = ME_NO_EFFECT;
				goto done;
			}
			else
				goto out_of_prompt;
		}
	}

	/* That failed, so now try to find the ^M at the end of the wanted line. */
	if (next_cr_p != NULL) {
		DEBUG((df, "trying second\n"));

		pos = u->cmd.data->str + u->cmd.pos;
		for (i = 0; i < n && pos != NULL; i++)
			pos = g_strrstr_len(u->cmd.data->str, pos - u->cmd.data->str, "\015");

		if (pos != NULL) {
			offset = next_cr_p - u->cmd.data->str - u->cmd.pos;
			u->cmd.pos = pos - offset - u->cmd.data->str;
			if (u->cmd.pos >= 0) {
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
	u->cmd.pos = 0;

	done:
	pass_segment(b);
	return effect;
}


/* Return cursor to beginning of this line, or if expecting
   newline, command executed. */
static MatchEffect seq_term_carriage_return(Connection* b, struct user_shell*  u) {
	MatchEffect effect;
	gchar* p;

	if (u->expect_newline) {
		effect = ME_CMD_EXECUTED;
		pass_segment(b);
	}
	else {
		p = g_strrstr_len(u->cmd.data->str, u->cmd.pos, "\015");
		if (p == NULL) {
			u->cmd.pos = 0;
			effect = ME_CMD_REBUILD;
		}
		else {
			/* Go to the character just after the ^M. */
			u->cmd.pos = p - u->cmd.data->str + 1;
			effect = ME_NO_EFFECT;
		}

		/* We don't want to pop off the character following the ^M. */
		b->n--;
		pass_segment(b);
	}

	return effect;
}


/* What this function returns depends on whether the user has
   pressed any of return (enter), Ctrl-C, or Ctrl-D or not.
   If so, then a newline that takes us out of the command line
   is interpreted as a command execution.  If not, then it is
   a command line wrap. */
static MatchEffect seq_term_newline(Connection* b, struct user_shell*  u) {
	gchar* p;
	MatchEffect effect;

	DEBUG((df, "expect_newline: %d", u->expect_newline));

	p = g_strstr_len(u->cmd.data->str + u->cmd.pos, u->cmd.data->len - u->cmd.pos, "\015");
	if (p == NULL) {
		if (u->expect_newline) {
			/* Command must have been executed. */
			effect = ME_CMD_EXECUTED;
		}
		else {
			/* Newline must just be a wrap. */
			u->cmd.pos = u->cmd.data->len;
			if (cmd_overwrite_char(&u->cmd, '\015', FALSE))
				effect = ME_NO_EFFECT;
			else
				effect = ME_ERROR;
		}
	}
	else {
		u->cmd.pos = p - u->cmd.data->str + 1;
		effect = ME_NO_EFFECT;
	}
	pass_segment(b);
	return effect;
}


static MatchEffect seq_nav_up(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_up!\n"));
	eat_segment(b);
	action_queue(A_SEND_UP);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_down(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_down!\n"));
	eat_segment(b);
	action_queue(A_SEND_DOWN);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_pgup(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_pgup!\n"));
	eat_segment(b);
	action_queue(A_SEND_PGUP);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_pgdown(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_pgdown!\n"));
	eat_segment(b);
	action_queue(A_SEND_PGDOWN);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_ctrl_g(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_ctrlg!\n"));
	b->n--;
	b->pos++;
	eat_segment(b);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_toggle(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_toggle!\n"));
	eat_segment(b);
	action_queue(A_TOGGLE);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_refocus(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_refocus!\n"));
	eat_segment(b);
	action_queue(A_REFOCUS);
	return ME_NO_EFFECT;
}


static MatchEffect seq_nav_disable(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_disable!\n"));
	eat_segment(b);
	action_queue(A_DISABLE);
	return ME_NO_EFFECT;
}


/*
static MatchEffect seq_nav_catch_all(Connection* b, struct user_shell*  u) {
	DEBUG((df, "seq_nav_catch_all!\n"));
	eat_segment(b);
	return ME_NO_EFFECT;
}
*/

