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

#include "vgseer-common.h"
#include "sanitize.h"

/*#define DEBUG(blah)	do { } while(0)*/

#if DEBUG_ON
extern FILE* df;
#endif

static void   sane_add_char(struct sane_cmd* s, char c);
static void   sane_delete_current_word(struct sane_cmd* s);
static gboolean   sane_last_char(struct sane_cmd* s, char c);

static gboolean             in_quote(struct sane_cmd* s, enum quote_type qt);
static enum quote_type  ql_pop(struct sane_cmd* s);
static void             ql_push(struct sane_cmd* s, enum quote_type);


char* make_sane_cmd(char* full_command, int length) {
	struct sane_cmd s;
	int i;
	char c;

	DEBUG((df,"<full: %s>\n", full_command));

	s.last_char_backslash = FALSE;
	s.last_char_exclamation = FALSE;
	s.last_char_dollar = FALSE;
	s.skip_word = FALSE;
	s.command = g_new(char, length + 5 + 1);    /* The 5 is a just-in-case buffer. */
	s.pos = 0;
	s.ql = NULL;

	for (i = 0; i < length; i++) {
		DEBUG((df, "."));
		c = *(full_command + i);

		if (s.last_char_exclamation) {
			/* Don't allow history expansion. */
			if ( (c != '(') && (c != ' ') && (c != '\t') && (c != '\n') ) {
				s.skip_word = TRUE;
				s.last_char_exclamation = FALSE;
				s.pos--;      /* Remove the ! */
				continue;
			}
		}
		else if (s.last_char_dollar) {
			/* Don't allow $ constructs (variables, command substitution, etc. */
			if ( (c != ' ') && (c != '\t') && (c != '\n') ) {
				s.pos--;
				s.last_char_dollar = FALSE;
				i = length;   /* Break out of loop. */
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
					sane_add_char(&s, c);
				}
				else if (in_quote(&s, QT_DOUBLE))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else {
					ql_push(&s, QT_SINGLE);
					sane_add_char(&s, c);
				}
				break;

			case ('\"'):
				if (in_quote(&s, QT_SINGLE))
					sane_add_char(&s, c);
				else if (in_quote(&s, QT_DOUBLE)) {
					ql_pop(&s);
					sane_add_char(&s, c);
				}
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else {
					ql_push(&s, QT_DOUBLE);
					sane_add_char(&s, c);
				}
				break;

			case ('\\'):
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else {
					s.last_char_backslash = TRUE;
					sane_add_char(&s, c);
				}
				break;

			case ('$'):
				if (in_quote(&s, QT_SINGLE))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else {
					s.last_char_dollar = TRUE;
					sane_add_char(&s, c);
				}

				break;


			case ('!'):      /* Gotta be careful about history expansion. */
				if (in_quote(&s, QT_SINGLE))
					sane_add_char(&s, c);
				else if (in_quote(&s, QT_EXTGLOB_PAREN))
					/* No ! allowed in ?( ) constructs. */
					break;
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else {
					s.last_char_exclamation = TRUE;
					sane_add_char(&s, c);
				}
				break;

			case (' '):
			case ('\t'):
				if (s.last_char_exclamation)
					s.last_char_exclamation = FALSE;
				else if (s.last_char_backslash)
					s.last_char_backslash = FALSE;
				else if (s.skip_word) {         /* Only ' ' and \t can turn off skip_word. */
					s.skip_word = FALSE;
				}
				sane_add_char(&s, c);
				break;

			/* Only allow ( in the *(blah), ?(blah), etc. forms, or when quoted. */
			case ('('):
				DEBUG((df, "<("));
				if (in_quote(&s, QT_SINGLE))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					DEBUG((df, "!"));
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else if (s.last_char_exclamation) {
					DEBUG((df, "?"));
					if (in_quote(&s, QT_DOUBLE)) {
						/* It sucks that ! is such a multiuse character. There's no good way to deal
						   here, so just give up. */
						s.pos--;
						s.last_char_exclamation = FALSE;
						i = length;
						break;
					}
					s.last_char_exclamation = FALSE;
					ql_push(&s, QT_EXTGLOB_PAREN);
					sane_add_char(&s, c);
				}
				else if (in_quote(&s, QT_DOUBLE))
					sane_add_char(&s, c);
				else if (sane_last_char(&s, '?') || sane_last_char(&s, '*') ||
				         sane_last_char(&s, '+') || sane_last_char(&s, '@')) {
					DEBUG((df, "-"));
					ql_push(&s, QT_EXTGLOB_PAREN);
					sane_add_char(&s, c);
				}
				else
					i = length;    /* Break out of loop. */
				DEBUG((df, ">"));
				break;

			case (')'):
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else if (in_quote(&s, QT_EXTGLOB_PAREN)) {
					ql_pop(&s);
					sane_add_char(&s, c);
				}
				/* Skip ) otherwise. */
				break;

			case ('`'):  /* Backtick */
				if (in_quote(&s, QT_SINGLE))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else
					i = length;    /* Break out of loop. */
				break;

			case (';'):     /* These are command finishers. */
			case ('&'):     /* Must be careful not to include one if not escaped, */
			case ('|'):     /* and to stop processing if so. */
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE) || in_quote(&s, QT_EXTGLOB_PAREN))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else
					i = length;    /* Break out of loop. */
				break;

			case ('\015'):  /* Carriage return */
			case ('\n'):    /* Should never see this, but just in case. */
				break;      /* Skip 'em. */

			case ('>'):
			case ('<'):
				if (in_quote(&s, QT_SINGLE) || in_quote(&s, QT_DOUBLE) || in_quote(&s, QT_EXTGLOB_PAREN))
					sane_add_char(&s, c);
				else if (s.last_char_backslash) {
					s.last_char_backslash = FALSE;
					sane_add_char(&s, c);
				}
				else {    /* Break out of loop. */
					sane_delete_current_word(&s); /* Note: this isn't entirely correct, as bash only interprets a few
					                                 characters as being part of the redirection construct. */
					i = length;
				}
				break;

			default:
				if (s.last_char_backslash)
					s.last_char_backslash = FALSE;
				sane_add_char(&s, c);
				break;
		}
	}

	if (s.last_char_backslash) {
		/* Can't have a trailing backslash. */
		s.pos--;
		s.last_char_backslash = FALSE;
	}

	/* Close unclosed quotes. */
	enum quote_type qt;
	while ( (qt = ql_pop(&s)) != QT_DUMMY ) {

		if (s.last_char_exclamation) {
			/* This exclamation could be interpreted as special because of the following quote characters. */
			s.pos--;
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
		sane_add_char(&s, c);
	}

	sane_add_char(&s, '\0');
	return s.command;
}


static gboolean sane_last_char(struct sane_cmd* s, char c) {
	if (s->pos > 0) {
		if ( *(s->command + s->pos - 1) == c )
			return TRUE;
	}
	
	return FALSE;
}


static void sane_delete_current_word(struct sane_cmd* s) {
	int i;
	char c;

	for (i = s->pos; i >= 0; i--) {
		c = *(s->command + i);
		if (c == ' ' || c == '\t' || c == '\n')
			break;
		else {
			*(s->command + i) = '\0';
			if (s->pos > 0)
				s->pos--;
		}
	}
}


static void sane_add_char(struct sane_cmd* s, char c) {
	DEBUG((df, "\n- Adding %c to %d", c, s->pos));
	*(s->command + s->pos) = c;
	s->pos++;
	return;
}


static enum quote_type  ql_pop(struct sane_cmd* s) {
	enum quote_type popped;
	if (s->ql) {
		struct quote_list* tmp = s->ql->next;
		popped = s->ql->qt;
		g_free(s->ql);
		s->ql = tmp;
	}
	else
		popped = QT_DUMMY;

	DEBUG((df, "(popping %d)", popped));
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


