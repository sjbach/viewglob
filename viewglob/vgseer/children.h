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

#ifndef CHILDREN_H
#define CHILDREN_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include <X11/Xlib.h>

G_BEGIN_DECLS


struct args {
	gchar** argv;
	gint arg_count;
};


struct pty_child {
	gchar* exec_name;
	struct args a;
	pid_t pid;
	gint fd;
};


struct display {
	gchar* name;
	struct args a;
	pid_t pid;
	Window xid;
	gchar* glob_fifo_name;
	gchar* cmd_fifo_name;
	gchar* feedback_fifo_name;
	gint glob_fifo_fd;
	gint cmd_fifo_fd;
	gint feedback_fifo_fd;
};

#define NEW_PTY_FD -99
#define CLOSE_FD -98
gboolean pty_child_fork(struct pty_child* c, gint new_stdin_fd, gint new_stdout_fd, gint new_stderr_fd);
gboolean pty_child_terminate(struct pty_child* c);

gboolean display_init(struct display* d);
gboolean display_fork(struct display* d);
gboolean display_running(struct display* d);
gboolean display_terminate(struct display* d);
gboolean display_cleanup(struct display* d);

void args_init(struct args* a);
void args_add(struct args* a, gchar* new_arg);


G_END_DECLS

#endif /* !CHILDREN_H */

