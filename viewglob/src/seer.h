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
	char* executable;
	char* display;
	char** display_argv;
	int    display_args;
	char* config_file;
	char* shell_out_file;
	char* term_out_file;
	char* init_command;
	char* expand_command;
};


/* Data structure for the shell that users use. */
struct user_shell {
	char* pwd;
	struct cmdline cmd;
	struct sequence_buff seqbuff;
	struct pty_child s;

	enum process_level pl;
	bool term_size_changed;
	bool expect_newline;
};


/* Data structure for seer's sandbox shell. */
struct glob_shell {
	char* out_file;
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
static bool main_loop(struct display* d);
static bool user_activity(void);
static bool scan_for_newline(const char* buff, size_t n);
static bool process_input(char* buff, size_t* n);
static bool match_loop(enum process_level pl);
static bool eat(char* buff, size_t* n, size_t* start);
static void analyze_effect(MatchEffect effect);

static void send_sane_cmd(struct display* d);

static void parse_args(int argc, char** argv);
static void add_display_arg(char* new_arg);

enum action action_queue(enum action);

void sigwinch_handler(int signum);
bool handle_signals(void);
void handler(int signum);
size_t strlen_safe(const char* string);

END_C_DECLS

#endif /* !SEER_H */
