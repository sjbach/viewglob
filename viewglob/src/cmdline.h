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

#ifndef CMDLINE_H
#define CMDLINE_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

BEGIN_C_DECLS

#define CMD_STEP_SIZE 512
#define SEMBUFF_STRING_SIZE 512

struct cmdline {
	char* command;
	int pos;
	int length;
	bool rebuilding;
};

struct sequence_buff {
	char string[SEMBUFF_STRING_SIZE];
	int pos;
	int length;
};

/* For cmd_wipe_in_line() -- maintain order of enumeration */
enum direction { D_RIGHT = 0, D_LEFT = 1, D_ALL = 2 };

bool  cmd_init(void);
bool  cmd_alloc(void);
bool  cmd_clear(void);
bool  seqbuff_clear(void);
bool  seqbuff_enqueue(char c);
bool  seqbuff_dequeue(int n, bool add_to_cmd);
bool seqbuff_advance(bool add_to_cmd);

bool  cmd_overwrite_char(char c, bool preserve_cret);
bool  cmd_insert_chars(char c, int n);
bool  cmd_del_chars(int n);
bool  cmd_wipe_in_line(enum direction dir);
bool  cmd_backspace(int n);
void  cmd_del_trailing_crets(void);

END_C_DECLS

#endif	/* !CMDLINE_H */
