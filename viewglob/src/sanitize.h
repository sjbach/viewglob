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

#ifndef SANITIZE_H
#define SANITIZE_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

BEGIN_C_DECLS

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
	bool last_char_backslash;
	bool last_char_exclamation;
	bool last_char_dollar;
	bool skip_word;
	struct quote_list* ql;
	char* command;
	int pos;
};


char*  make_sane_cmd(char* full_command, int length);

static void   sane_add_char(struct sane_cmd* s, char c);
static void   sane_delete_current_word(struct sane_cmd* s);
static bool   sane_last_char(struct sane_cmd* s, char c);

static bool             in_quote(struct sane_cmd* s, enum quote_type qt);
static enum quote_type  ql_pop(struct sane_cmd* s);
static void             ql_push(struct sane_cmd* s, enum quote_type);

END_C_DECLS

#endif	/* !SANITIZE_H */
