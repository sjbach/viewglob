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
#include <glib.h>

#include <sys/wait.h>
#ifndef WEXITSTATUS
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#  define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif


static gboolean create_fifo(gchar* name);
static gboolean waitpid_wrapped(pid_t pid);
static gboolean wait_for_data(gint fd);
 

/* Make five attempts at creating a fifo with the given name. */
static gboolean create_fifo(gchar* name) {
	gint i;
	gboolean ok = TRUE;

	for (i = 0; i < 5; i++) {

		/* Only read/writable by this user. */
		if (mkfifo(name, S_IRUSR | S_IWUSR) == -1) {
			if (errno == EEXIST) {
				g_warning("Fifo already exists");
				if (unlink(name) == -1) {
					g_warning("Could not remove old file");
					ok = FALSE;
					break;
				}
			}
			else {
				g_critical("Could not create fifo \"%s\"", name);
				ok = FALSE;
				break;
			}
		}
		else
			break;
	}
	return ok;
}


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
void args_init(struct args* a) {
	a->argv = g_new(gchar*, 1);
	*(a->argv) = NULL;
	a->arg_count = 1;
}


/* Add a new argument to this argument struct. */
void args_add(struct args* a, gchar* new_arg) {
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


/* Initialize communication fifos and the argument array for the display fork.
   Should only be called once, whereas display_fork() can be called multiple times. */
gboolean display_init(struct display* d) {
	pid_t pid;
	gchar* pid_str;
	gboolean ok = TRUE;

	d->a.argv[0] = d->name;
	d->xid = 0;              /* We'll get this from the display when it's started. */

	/* Get the current pid and turn it into a string. */
	pid_str = g_new(gchar, 10 + 1);    /* 10 for the length of the pid, 1 for \0. */
	pid = getpid();
	sprintf(pid_str, "%ld", (glong) pid);

	/* Create the glob fifo name. */
	d->glob_fifo_name = g_new(gchar, strlen("/tmp/viewglob") + strlen(pid_str) + strlen("-1") + 1);
	(void)strcpy(d->glob_fifo_name, "/tmp/viewglob");
	(void)strcat(d->glob_fifo_name, pid_str);
	(void)strcat(d->glob_fifo_name, "-1");

	/* Create the cmd fifo name. */
	d->cmd_fifo_name = g_new(gchar, strlen("/tmp/viewglob") + strlen(pid_str) + strlen("-2") + 1);
	(void)strcpy(d->cmd_fifo_name, "/tmp/viewglob");
	(void)strcat(d->cmd_fifo_name, pid_str);
	(void)strcat(d->cmd_fifo_name, "-2");

	/* And the feedback fifo name. */
	d->feedback_fifo_name = g_new(gchar, strlen("/tmp/viewglob") + strlen(pid_str) + strlen("-3") + 1);
	(void)strcpy(d->feedback_fifo_name, "/tmp/viewglob");
	(void)strcat(d->feedback_fifo_name, pid_str);
	(void)strcat(d->feedback_fifo_name, "-3");

	g_free(pid_str);

	/* Create the fifos. */
	if ( ! create_fifo(d->glob_fifo_name) )
		return FALSE;
	if ( ! create_fifo(d->cmd_fifo_name) )
		return FALSE;
	if ( ! create_fifo(d->feedback_fifo_name) )
		return FALSE;

	/* Add the fifo's to the arguments. */
	args_add(&(d->a), "-g");
	args_add(&(d->a), d->glob_fifo_name);
	args_add(&(d->a), "-c");
	args_add(&(d->a), d->cmd_fifo_name);
	args_add(&(d->a), "-f");
	args_add(&(d->a), d->feedback_fifo_name);

	/* Delimit the args with NULL. */
	args_add(&(d->a), NULL);

	return ok;
}



/* Open the display and set it up. */
gboolean display_fork(struct display* d) {
	gboolean ok = TRUE;

	switch (d->pid = fork()) {
		case -1:
			g_critical("Could not fork display: %s", g_strerror(errno));
			return FALSE;

		case 0:
			/* Open the display. */
			execvp(d->a.argv[0], d->a.argv);

			g_critical("Display exec failed: %s", g_strerror(errno));
			g_critical("If viewglob does not exit, you should do so manually");

			_exit(EXIT_FAILURE);
			break;
	}

	/* Open the fifos for writing in parent. */
	/* The sandbox shell will be outputting to the glob fifo, but seer opens it too
	   to make sure it doesn't get EOF'd by the sandbox shell accidentally. */
	if ( (d->glob_fifo_fd = open(d->glob_fifo_name, O_WRONLY)) == -1) {
		g_critical("Could not open glob fifo for writing: %s", g_strerror(errno));
		ok = FALSE;
	}
	if ( (d->cmd_fifo_fd = open(d->cmd_fifo_name, O_WRONLY)) == -1) {
		g_critical("Could not open cmd fifo for writing: %s", g_strerror(errno));
		ok = FALSE;
	}
	if ( (d->feedback_fifo_fd = open(d->feedback_fifo_name, O_RDONLY)) == -1) {
		g_critical("Could not open feedback fifo for reading: %s", g_strerror(errno));
		ok = FALSE;
	}


	return ok;
}


gboolean display_running(struct display* d) {
	return d->pid != -1;
}


/* Terminate display's process and close communication fifos.  Should be called for
   each forked display. */
gboolean display_terminate(struct display* d) {
	gboolean ok = TRUE;

	/* Terminate and wait the child's process. */
	if (d->pid != -1) {
		switch (kill(d->pid, SIGTERM)) {
			case 0:
				ok &= waitpid_wrapped(d->pid);
				d->pid = -1;
				d->xid = 0;
				break;
			case -1:
				if (errno == ESRCH)
					g_warning("Display already closed");
				else {
					g_critical("Could not close display: %s", g_strerror(errno));
					ok = FALSE;
				}
				break;
			default:
				g_return_val_if_reached(FALSE);
				/*break;*/
		}
	}

	/* Close the fifos, if open. */
	if (d->glob_fifo_fd != -1) {
		if (close(d->glob_fifo_fd) != -1)
			d->glob_fifo_fd = -1;
		else {
			g_critical("Could not close glob fifo: %s", g_strerror(errno));
			ok = FALSE;
		}
	}
	if (d->cmd_fifo_fd != -1) {
		if (close(d->cmd_fifo_fd) != -1)
			d->cmd_fifo_fd = -1;
		else {
			g_critical("Could not close cmd fifo: %s", g_strerror(errno));
			ok = FALSE;
		}
	}
	if (d->feedback_fifo_fd != -1) {
		if (close(d->feedback_fifo_fd) != -1)
			d->feedback_fifo_fd = -1;
		else {
			g_critical("Could not close feedback fifo: %s", g_strerror(errno));
			ok = FALSE;
		}
	}

	return ok;
}


/* Remove fifos and clear the display struct.  Should only be called
   when there will be no more display forks. */
gboolean display_cleanup(struct display* d) {
	gboolean ok = TRUE;

	/* Remove the fifos. */
	if (d->glob_fifo_name) {
		if ( unlink(d->glob_fifo_name) == -1 ) {
			if (errno != ENOENT)
				g_warning("Could not delete glob fifo \"%s\": %s", d->glob_fifo_name, g_strerror(errno));
		}
	}
	if (d->cmd_fifo_name) {
		if ( unlink(d->cmd_fifo_name) == -1 ) {
			if (errno != ENOENT)
				g_warning("Could not delete cmd fifo \"%s\": %s", d->cmd_fifo_name, g_strerror(errno));
		}
	}
	if (d->feedback_fifo_name) {
		if ( unlink(d->feedback_fifo_name) == -1 ) {
			if (errno != ENOENT)
				g_warning("Could not delete feedback fifo \"%s\": %s", d->feedback_fifo_name, g_strerror(errno));
		}
	}

	g_free(d->glob_fifo_name);
	g_free(d->cmd_fifo_name);
	g_free(d->feedback_fifo_name);

	d->glob_fifo_name = d->cmd_fifo_name = d->feedback_fifo_name = NULL;

	return ok;
}


/* Fork a new child with a pty. */
gboolean pty_child_fork(struct pty_child* c, gint new_stdin_fd, gint new_stdout_fd, gint new_stderr_fd) {

	gint pty_slave_fd = -1;
	gint pty_master_fd = -1;
	const gchar* pty_slave_name = NULL;

	gboolean ok = TRUE;

	c->a.argv[0] = c->exec_name;

	/* Delimit the args with NULL. */
	args_add(&(c->a), NULL);

	/* Setup a pty for the new shell. */
	/* get master (pty) */
	if ((pty_master_fd = rxvt_get_pty(&pty_slave_fd, &pty_slave_name)) < 0) {
		g_critical("Could not open master side of pty");
		c->pid = -1;
		c->fd = -1;
		return FALSE;
	}

	/* Turn on non-blocking -- this is used in rxvt, but I can't see why,
	   so I've disabled it. */
	/*fcntl(pty_master_fd, F_SETFL, O_NDELAY);*/

	/* This will be the interface with the new shell. */
	c->fd = pty_master_fd;

	switch ( c->pid = fork() ) {
		case -1:
			g_critical("Could not fork process: %s", g_strerror(errno));
			return FALSE;
			/*break;*/

		case 0:

			/* get slave (tty) */
			if (pty_slave_fd < 0) {
				if ((pty_slave_fd = rxvt_get_tty(pty_slave_name)) < 0) {
					(void) close(pty_master_fd);
					g_critical("Could not open slave tty \"%s\"", pty_slave_name);
					goto child_fail;
				}
			}
			if (rxvt_control_tty(pty_slave_fd, pty_slave_name) < 0) {
				g_critical("Could not obtain control of tty \"%s\"", pty_slave_name);
				goto child_fail;
			}

			/* A parameter of NEW_PTY_FD means to use the slave side of the new pty. */
			/* A parameter of CLOSE_FD means to just close that fd right out. */

			if (new_stdin_fd == NEW_PTY_FD)
				new_stdin_fd = pty_slave_fd;
			if (new_stdout_fd == NEW_PTY_FD)
				new_stdout_fd = pty_slave_fd;
			if (new_stderr_fd == NEW_PTY_FD)
				new_stderr_fd = pty_slave_fd;

			if (new_stdin_fd == CLOSE_FD)
				(void)close(STDIN_FILENO);
			else if ( dup2(new_stdin_fd, STDIN_FILENO) == -1 ) {
				g_critical("Could not replace stdin in child process: %s", g_strerror(errno));
				goto child_fail;
			}

			if (new_stdout_fd == CLOSE_FD)
				(void)close(STDOUT_FILENO);
			else if ( dup2(new_stdout_fd, STDOUT_FILENO) == -1 ) {
				g_critical("Could not replace stdout in child process: %s", g_strerror(errno));
				goto child_fail;
			}

			if (new_stderr_fd == CLOSE_FD)
				(void)close(STDERR_FILENO);
			else if ( dup2(new_stderr_fd, STDERR_FILENO) == -1 ) {
				g_critical("Could not replace stderr in child process: %s", g_strerror(errno));
				goto child_fail;
			}

			(void)close(pty_slave_fd);
			execvp(c->exec_name, c->a.argv);

			child_fail:
			g_critical("Exec failed: %s", g_strerror(errno));
			g_critical("If viewglob does not exit, you should do so manually");
			_exit(EXIT_FAILURE);

			break;
	}

	if (!wait_for_data(c->fd)) {
		g_critical("Did not receive go-ahead from child shell");
		ok = FALSE;
	}

	return ok;
}


gboolean pty_child_terminate(struct pty_child* c) {
	gboolean ok = TRUE;

	/* Close the pty to the sandbox shell (if valid). */
	if ( (c->fd != -1) && (close(c->fd) == -1) ) {
		g_critical("Could not close pty to child: %s", g_strerror(errno));
		ok = FALSE;
	}

	/* Terminate and wait the child's process. */
	if (c->pid != -1) {
		/* SIGHUP terminates bash (cleanly), SIGTERM won't. */
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

