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

#ifndef SANITIZE_H
#define SANITIZE_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "vgseer-common.h"

G_BEGIN_DECLS

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
	char* command;
	int pos;
};


char*  make_sane_cmd(char* full_command, int length);

G_END_DECLS

#endif	/* !SANITIZE_H */
