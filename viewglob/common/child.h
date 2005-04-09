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

#ifndef CHILD_H
#define CHILD_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

G_BEGIN_DECLS


struct arguments {
	gchar** argv;
	gint arg_count;
};


struct child {
	gchar* exec_name;
	struct arguments args;
	pid_t pid;
	gint fd_in;
	gint fd_out;
};


#define NEW_PTY_FD -99
#define CLOSE_FD   -98
void     child_init(struct child* c);
gboolean child_fork(struct child* c);
gboolean pty_child_fork(struct child* c, gint new_stdin_fd, gint new_stdout_fd, gint new_stderr_fd);
gboolean child_running(struct child* c);
gboolean child_terminate(struct child* c);

void args_init(struct arguments* a);
void args_add(struct arguments* a, gchar* new_arg);

gboolean wait_for_data(gint fd);

G_END_DECLS

#endif /* !CHILD_H */

