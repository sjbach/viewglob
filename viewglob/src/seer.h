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

#ifndef SEER_H
#define SEER_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "sequences.h"
#include "cmdline.h"
#include "children.h"

BEGIN_C_DECLS

struct options {
	enum shell_type shell_type;
	char* executable;
	char* display;
	char* config_file;
	char* shell_out_file;
	char* term_out_file;
	char* init_loc;
	char* expand_command;
};


/* Data structure for the user's shell. */
struct user_shell {
	char* pwd;
	struct cmdline cmd;
	struct sequence_buff seqbuff;
	struct pty_child s;

	int shell_transcript_fd;
	int term_transcript_fd;

	enum process_level pl;
	bool term_size_changed;
	bool expect_newline;
};


/* Data structure for seer's sandbox shell. */
struct glob_shell {
	char* glob_cmd;
	struct pty_child s;
};


enum action {
	A_NOP,      /* Do nothing. */
	A_SEND_CMD, /* Send the shell's current command line. */
	A_SEND_PWD, /* Send the shell's current pwd. */
	A_POP,      /* Pop off the top queued action. */
	A_DONE,     /* Nothing more to pop. */
	A_EXIT,     /* Shell closed -- finish execution. */
};


/* Prototypes */
enum action action_queue(enum action);

void sigwinch_handler(int signum);
void sigterm_handler(int signum);
bool handle_signals(void);
void handler(int signum);
size_t strlen_safe(const char* string);

END_C_DECLS

#endif /* !SEER_H */
