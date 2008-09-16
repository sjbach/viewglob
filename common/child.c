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
#include "hardened-io.h"
#include "child.h"
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>

#include <sys/wait.h>
#ifndef WEXITSTATUS
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#  define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif


static gboolean waitpid_wrapped(pid_t pid);
 

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

	if (new_arg)
		temp = g_strdup(new_arg);
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

			/* Close off stderr. */
			(void) close(STDERR_FILENO);
			(void) open ("/dev/null", O_RDWR);

			(void) close(pfdout[0]); // FIXME check errors
			(void) close(pfdout[1]);
			(void) close(pfdin[0]);
			(void) close(pfdin[1]);

			execvp(c->exec_name, c->args.argv);

			child_fail:
			g_critical("Exec failed: %s", g_strerror(errno));
			g_critical("If the program does not exit, you should "
					"do so manually");
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


gboolean child_terminate(struct child* c) {
	gboolean ok = TRUE;

	/* Close the pty to the sandbox shell (if valid). */
	if (c->fd_in != -1) {
		(void) close(c->fd_in);
		c->fd_in = -1;
	}
	if (c->fd_out != -1) {
		(void) close(c->fd_out);
		c->fd_out = -1;
	}

	/* Terminate and wait the child's process. */
	if (c->pid != -1) {
		/* SIGHUP terminates bash (cleanly), SIGTERM won't. */
		// FIXME send SIGTERM and then SIGHUP?
		switch (kill(c->pid, SIGHUP)) {
			case 0:
				ok &= waitpid_wrapped(c->pid);
				break;
			case -1:
				if (errno == ESRCH)
					ok &= waitpid_wrapped(c->pid);
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
		c->pid = -1;
	}

	return ok;
}


gboolean wait_for_data(gint fd) {
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

