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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "children.h"
#include "ptytty.h"
#include <stdio.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include <sys/wait.h>
#ifndef WEXITSTATUS
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#  define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif


static gboolean waitpid_wrapped(pid_t pid);
static gboolean wait_for_data(gint fd);
 

/* Wrapper to interpret both 0 and -1 waitpid() returns as errors. */
static gboolean waitpid_wrapped(pid_t pid) {
	gboolean result = TRUE;

	switch (waitpid(pid, NULL, 0)) {
		case 0:
			/* pid has not exited. */
		case -1:
			/* Error. */
			result = FALSE;
			break;
	}

	return result;
}


/* Initialize the argument array struct. */
void args_init(struct arguments* a) {
	a->argv = g_new(gchar*, 1);
	*(a->argv) = NULL;
	a->arg_count = 1;
}


/* Add a new argument to this argument struct. */
void args_add(struct arguments* a, gchar* new_arg) {
	gchar* temp;

	if (new_arg) {
		temp = g_new(gchar, strlen(new_arg) + 1);
		strcpy(temp, new_arg);
	}
	else
		temp = NULL;

	a->argv = g_renew(gchar*, a->argv, a->arg_count + 1);
	*(a->argv + a->arg_count) = temp;
	a->arg_count++;
}


void child_init(struct child* c) {
	c->exec_name = NULL;
	args_init(&c->args);
	c->pid = -1;
	c->fd_in = -1;
	c->fd_out = -1;
}


/* Fork a child with a read pipe and a write pipe. */
gboolean child_fork(struct child* c) {

	gint pfdout[2];
	gint pfdin[2];

	c->args.argv[0] = c->exec_name;

	/* Delimit the args with NULL. */
	args_add(&(c->args), NULL);

	if (pipe(pfdout) == -1 || pipe(pfdin) == -1) {
		g_critical("Could not create pipes: %s", g_strerror(errno));
		c->pid = -1;
		c->fd_in = -1;
		c->fd_out = -1;
		return FALSE;
	}

	switch (c->pid = fork()) {
		case -1:
			g_critical("Could not fork process: %s", g_strerror(errno));
			return FALSE;
			/*break;*/

		case 0:
			if (setsid() == -1)
				g_warning("child_fork(): setsid(): %s", g_strerror(errno));

			if (dup2(pfdout[0], STDIN_FILENO) == -1 ||
					dup2(pfdin[1], STDOUT_FILENO) == -1) {
				g_critical("Could not replace streams in child process: %s",
						g_strerror(errno));
				goto child_fail;
			}
			//(void) close(STDERR_FILENO);
			(void) close(pfdout[0]); // FIXME check errors
			(void) close(pfdout[1]);
			(void) close(pfdin[0]);
			(void) close(pfdin[1]);

			execvp(c->exec_name, c->args.argv);

			child_fail:
			g_critical("Exec failed: %s", g_strerror(errno));
			g_critical("If vgseer does not exit, you should do so manually");
			_exit(EXIT_FAILURE);
			break;
	}

	(void) close(pfdout[0]);
	(void) close(pfdin[1]);
	c->fd_out = pfdout[1];
	c->fd_in = pfdin[0];

	if (!wait_for_data(c->fd_out)) {
		g_critical("Did not receive go-ahead from child shell");
		return FALSE;
	}

	return TRUE;
}


gboolean child_running(struct child* c) {
	g_return_val_if_fail(c != NULL, FALSE);
	return c->pid != -1 && c->fd_in != -1 && c->fd_out != -1;
}


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


gboolean child_terminate(struct child* c) {
	gboolean ok = TRUE;

	/* Close the pty to the sandbox shell (if valid). */
	if (c->fd_in != -1)
		(void) close(c->fd_in);
	if (c->fd_out != -1)
		(void) close(c->fd_out);

	/* Terminate and wait the child's process. */
	if (c->pid != -1) {
		/* SIGHUP terminates bash (cleanly), SIGTERM won't. */
		// FIXME send SIGTERM and then SIGHUP?
		switch (kill(c->pid, SIGHUP)) {
			case 0:
				ok &= waitpid_wrapped(c->pid);
				c->pid = -1;
				break;
			case -1:
				if (errno == ESRCH)
					g_warning("Child already terminated");
				else {
					g_critical("Could not terminate child: %s",
							g_strerror(errno));
					ok = FALSE;
				}
				break;
			default:
				g_return_val_if_reached(FALSE);
				/*break;*/
		}
	}

	return ok;
}


static gboolean wait_for_data(gint fd) {
	fd_set fd_set_write;
	gboolean ok = TRUE;

	FD_ZERO(&fd_set_write);
	FD_SET(fd, &fd_set_write);
	if ( select(fd + 1, NULL, &fd_set_write, NULL, NULL) == -1 ) {
		g_critical("wait_for_data(): %s", g_strerror(errno));
		ok = FALSE;
	}

	return ok;
}

