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

#ifndef PTY_SHELL_H
#define PTY_SHELL_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

BEGIN_C_DECLS


struct args {
	char** argv;
	int arg_count;
};


struct pty_child {
	char* name;
	struct args a;
	pid_t pid;
	int fd;
	int transcript_fd;
};


struct display {
	char* name;
	struct args a;
	pid_t pid;
	char* glob_fifo_name;
	int glob_fifo_fd;
	char* cmd_fifo_name;
	int cmd_fifo_fd;
};

#define NEW_PTY_FD -99
bool pty_child_fork(struct pty_child* c, int new_stdin_fd, int new_stdout_fd, int new_stderr_fd);
bool pty_child_terminate(struct pty_child* c);

bool display_fork(struct display* d);
bool display_terminate(struct display* d);

void args_init(struct args* a);
void args_add(struct args* a, char* new_arg);

END_C_DECLS

#endif /* !PTY_SHELL_H */
