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

#include "common.h"
#include "child.h"
#include "ptytty.h"

#include <fcntl.h>


/* Fork a new child with a pty. */
gboolean pty_child_fork(struct child* c, gint new_stdin_fd,
		gint new_stdout_fd, gint new_stderr_fd) {

	gint pty_slave_fd = -1;
	gint pty_master_fd = -1;
	const gchar* pty_slave_name = NULL;

	c->args.argv[0] = c->exec_name;

	/* Delimit the args with NULL. */
	args_add(&(c->args), NULL);

	/* Setup a pty for the new shell. */
	/* Get master (pty) */
	if ((pty_master_fd = rxvt_get_pty(&pty_slave_fd, &pty_slave_name)) < 0) {
		g_critical("Could not open master side of pty");
		c->pid = -1;
		c->fd_in = -1;
		c->fd_out = -1;
		return FALSE;
	}

	/* Turn on non-blocking -- probably not necessary since we're in raw
	   terminal mode. */
	fcntl(pty_master_fd, F_SETFL, O_NDELAY);

	/* This will be the interface with the new shell. */
	c->fd_in = pty_master_fd;
	c->fd_out = pty_master_fd;

	switch (c->pid = fork()) {
		case -1:
			g_critical("Could not fork process: %s", g_strerror(errno));
			return FALSE;
			/*break;*/

		case 0:
			if (setsid() == -1) {
				g_critical("pty_child_fork(): setsid(): %s",
						g_strerror(errno));
				goto child_fail;
			}

			/* Get slave (tty) */
			if (pty_slave_fd < 0) {
				if ((pty_slave_fd = rxvt_get_tty(pty_slave_name)) < 0) {
					(void) close(pty_master_fd);
					g_critical("Could not open slave tty \"%s\"",
							pty_slave_name);
					goto child_fail;
				}
			}
			if (rxvt_control_tty(pty_slave_fd, pty_slave_name) < 0) {
				g_critical("Could not obtain control of tty \"%s\"",
						pty_slave_name);
				goto child_fail;
			}

			/* A parameter of NEW_PTY_FD means to use the slave side of the
			   new pty.  A parameter of CLOSE_FD means to just close that fd
			   right out. */
			if (new_stdin_fd == NEW_PTY_FD)
				new_stdin_fd = pty_slave_fd;
			if (new_stdout_fd == NEW_PTY_FD)
				new_stdout_fd = pty_slave_fd;
			if (new_stderr_fd == NEW_PTY_FD)
				new_stderr_fd = pty_slave_fd;

			if (new_stdin_fd == CLOSE_FD)
				(void)close(STDIN_FILENO);
			else if (dup2(new_stdin_fd, STDIN_FILENO) == -1) {
				g_critical("Could not replace stdin in child process: %s",
						g_strerror(errno));
				goto child_fail;
			}

			if (new_stdout_fd == CLOSE_FD)
				(void)close(STDOUT_FILENO);
			else if (dup2(new_stdout_fd, STDOUT_FILENO) == -1) {
				g_critical("Could not replace stdout in child process: %s",
						g_strerror(errno));
				goto child_fail;
			}

			if (new_stderr_fd == CLOSE_FD)
				(void)close(STDERR_FILENO);
			else if (dup2(new_stderr_fd, STDERR_FILENO) == -1) {
				g_critical("Could not replace stderr in child process: %s",
						g_strerror(errno));
				goto child_fail;
			}

			(void)close(pty_slave_fd);
			execvp(c->exec_name, c->args.argv);

			child_fail:
			g_critical("Exec failed: %s", g_strerror(errno));
			g_critical("If vgseer does not exit, you should do so manually");
			_exit(EXIT_FAILURE);
			break;
	}

	if (!wait_for_data(c->fd_out)) {
		g_critical("Did not receive go-ahead from child shell");
		return FALSE;
	}

	return TRUE;
}


