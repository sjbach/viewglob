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
#include "sanitize.h"


enum quote_type {
	QT_DUMMY,
	QT_SINGLE,
	QT_DOUBLE,
	QT_EXTGLOB_PAREN,
};


struct quote_list {
	enum quote_type qt;
	struct quote_list* next;
};


struct sane_cmd {
	gboolean last_char_backslash;
	gboolean last_char_exclamation;
	gboolean last_char_dollar;
	gboolean skip_word;
	struct quote_list* ql;
	GString* command;
};


static void       add_char(struct sane_cmd* s, gchar c);
static void       delete_current_word(struct sane_cmd* s);
static void       backspace(struct sane_cmd* s);
static gboolean   last_char(struct sane_cmd* s, gchar c);

static gboolean         in_quote(struct sane_cmd* s, enum quote_type qt);
static enum quote_type  ql_pop(struct sane_cmd* s);
static void             ql_push(struct sane_cmd* s, enum quote_type);


gchar* sanitize(GString* string) {
	struct sane_cmd s;
	gint i;
	gchar c;

	s.last_char_backslash = FALSE;
	s.last_char_exclamation = FALSE;
	s.last_char_dollar = FALSE;
	s.skip_word = FALSE;
	s.command = g_string_sized_new(string->len);
	s.ql = NULL;

	for (i = 0; i < string->len; i++) {
		c = *(string->str + i);

		if (s.last_char_exclamation) {
			/* Don't allow history expansion. */
			if ( (c != '(') && (c != ' ') && (c != '\t') && (c != '\n') ) {
				s.skip_word = TRUE;
				s.last_char_exclamation = FALSE;
				/* Remove the ! */
				backspace(&s);
				continue;
			}
		}
		else if (s.last_char_dollar) {
			/* Don't allow $ constructs (variables, command substitution,
			   etc. */
			if ( (c != ' ') && (c != '\t') && (c != '\n') ) {
				backspace(&s);
				s.last_char_dollar = FALSE;
				i = string->len;   /* Break out of loop. */
				continue;
			}
			else {
				/* A lone $ is acceptable. */
				s.last_char_dollar = FALSE;
			}
		}

		if (s.skip_word) {
			if ( (c == ' ') || (c == '\t') )
				s.skip_word = FALSE;
			else
				continue;
		}

		switch (c) {
			case ('\''):
				if (in_quote(&s, QT_SINGLE)) {
					ql_pop(&s);
					add_char(&s, c);
				}
				else if (in_quote(&s, QT_DOUBLE))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else {
					ql_push(&s, QT_SINGLE);
					add_char(&s, c);
				}
				break;

			case ('\"'):
				if (in_quote(&s, QT_SINGLE))
					add_char(&s, c);
				else if (in_quote(&s, QT_DOUBLE)) {
					ql_pop(&s);
					add_char(&s, c);
				}
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else {
					ql_push(&s, QT_DOUBLE);
					add_char(&s, c);
				}
				break;

			case ('\\'):
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else {
					s.last_char_backslash = TRUE;
					add_char(&s, c);
				}
				break;

			case ('$'):
				if (in_quote(&s, QT_SINGLE))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else {
					s.last_char_dollar = TRUE;
					add_char(&s, c);
				}

				break;


			case ('!'):      /* Gotta be careful about history expansion. */
				if (in_quote(&s, QT_SINGLE))
					add_char(&s, c);
				else if (in_quote(&s, QT_EXTGLOB_PAREN))
					/* No ! allowed in ?( ) constructs. */
					break;
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else {
					s.last_char_exclamation = TRUE;
					add_char(&s, c);
				}
				break;

			case (' '):
			case ('\t'):
				if (s.last_char_exclamation)
					s.last_char_exclamation = FALSE;
				else if (s.last_char_backslash)
					s.last_char_backslash = FALSE;
				else if (s.skip_word) {
					/* Only ' ' and \t can turn off skip_word. */
					s.skip_word = FALSE;
				}
				add_char(&s, c);
				break;

			/* Only allow ( in the *(blah), ?(blah), etc. forms, or when
			   quoted. */
			case ('('):
				if (in_quote(&s, QT_SINGLE))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else if (s.last_char_exclamation) {
					if (in_quote(&s, QT_DOUBLE)) {
						/* It sucks that ! is such a multiuse character.
						   There's no good way to deal here, so just give
						   up. */
						backspace(&s);
						s.last_char_exclamation = FALSE;
						i = string->len;
						break;
					}
					s.last_char_exclamation = FALSE;
					ql_push(&s, QT_EXTGLOB_PAREN);
					add_char(&s, c);
				}
				else if (in_quote(&s, QT_DOUBLE))
					add_char(&s, c);
				else if (last_char(&s, '?') || last_char(&s, '*') ||
				         last_char(&s, '+') || last_char(&s, '@')) {
					ql_push(&s, QT_EXTGLOB_PAREN);
					add_char(&s, c);
				}
				else
					i = string->len;    /* Break out of loop. */
				break;

			case (')'):
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else if (in_quote(&s, QT_EXTGLOB_PAREN)) {
					ql_pop(&s);
					add_char(&s, c);
				}
				/* Skip ) otherwise. */
				break;

			case ('`'):  /* Backtick */
				if (in_quote(&s, QT_SINGLE))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else
					i = string->len;    /* Break out of loop. */
				break;

			case (';'):     /* These are command finishers. */
			case ('&'):     /* Must be careful not to include one if not */
			case ('|'):     /* escaped, and to stop processing if so. */
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE) ||
						in_quote(&s, QT_EXTGLOB_PAREN))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else
					i = string->len;    /* Break out of loop. */
				break;

			case ('\015'):  /* Carriage return */
			case ('\n'):    /* Should never see this, but just in case. */
				break;      /* Skip 'em. */

			case ('>'):
			case ('<'):
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE) ||
						in_quote(&s, QT_EXTGLOB_PAREN))
					add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					add_char(&s, c);
				}
				else {    /* Break out of loop. */
					/* Note: this isn't entirely correct, as bash only
					   interprets a few characters as being part of the
					   redirection construct. */
					delete_current_word(&s);
					i = string->len;
				}
				break;

			default:
				if (s.last_char_backslash)
					s.last_char_backslash = FALSE;
				add_char(&s, c);
				break;
		}
	}

	if (s.last_char_backslash) {
		/* Can't have a trailing backslash. */
		backspace(&s);
		s.last_char_backslash = FALSE;
	}

	/* Close unclosed quotes. */
	enum quote_type qt;
	while ( (qt = ql_pop(&s)) != QT_DUMMY ) {

		if (s.last_char_exclamation) {
			/* This exclamation could be interpreted as special because of
			   the following quote characters. */
			backspace(&s);
			s.last_char_exclamation = FALSE;
		}
		
		switch (qt) {
			case QT_SINGLE:
				c = '\'';
				break;
			case QT_DOUBLE:
				c = '\"';
				break;
			case QT_EXTGLOB_PAREN:
				c = ')';
				break;
			default:
				/* Should never get here, but just in case. */
				c = ' ';
				break;
		}
		add_char(&s, c);
	}
	
	gchar* retval = s.command->str;
	g_string_free(s.command, FALSE);
	return retval;
}


static gboolean last_char(struct sane_cmd* s, gchar c) {
	if ( (s->command->len > 0) &&
			(*(s->command->str + s->command->len - 1) == c) )
		return TRUE;
	else
		return FALSE;
}


static void delete_current_word(struct sane_cmd* s) {

	gchar c = *(s->command->str + s->command->len);
	while (c != ' ' && c != '\t' && c != '\n' && s->command->len > 0) {
		backspace(s);
		c = *(s->command->str + s->command->len);
	}
}


static void add_char(struct sane_cmd* s, gchar c) {
	s->command = g_string_append_c(s->command, c);
}


static enum quote_type ql_pop(struct sane_cmd* s) {
	enum quote_type popped;
	if (s->ql) {
		struct quote_list* tmp = s->ql->next;
		popped = s->ql->qt;
		g_free(s->ql);
		s->ql = tmp;
	}
	else
		popped = QT_DUMMY;

	return popped;
}


static void ql_push(struct sane_cmd* s, enum quote_type new_qt) {
	struct quote_list* new_ql;

	new_ql = g_new(struct quote_list, 1);
	new_ql->qt = new_qt;
	new_ql->next = s->ql;
	s->ql = new_ql;
}


static gboolean in_quote(struct sane_cmd* s, enum quote_type qt) {
	if (s->ql) {
		if (s->ql->qt == qt)
			return TRUE;
		else
			return FALSE;
	}
	else
		return FALSE;
}


static void backspace(struct sane_cmd* s) {
	g_return_if_fail(s != NULL);

	if (s->command->len > 0)
		s->command = g_string_truncate(s->command, s->command->len - 1);
}

