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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "viewglob-error.h"
#include "children.h"
#include "ptutil.viewglob.h"
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


#if DEBUG_ON
extern FILE* df;
#endif

static bool create_fifo(char* name);
static bool waitpid_wrapped(pid_t pid);

static bool create_fifo(char* name) {
	int i;
	bool ok = true;

	/* Try five times to create the fifo, deleting if present. */
	for (i = 0; i < 5; i++) {
		/* Only read/writable by this user. */
		if (mkfifo(name, S_IRUSR | S_IWUSR) == -1) {
			if (errno == EEXIST) {
				viewglob_warning("Fifo already exists");
				if (unlink(name) == -1) {
					viewglob_error("Could not remove old file");
					ok = false;
					break;
				}
			}
			else {
				viewglob_error("Could not create fifo");
				viewglob_error(name);
				ok = false;
				break;
			}
		}
		else
			break;
	}
	return ok;
}


static bool waitpid_wrapped(pid_t pid) {
	bool result = true;

	switch (waitpid(pid, NULL, 0)) {
		case 0:
			/* pid has not exited. */
		case -1:
			/* Error. */
			result = false;
			break;
	}

	return result;
}


/* Initialize the argument array struct. */
void args_init(struct args* a) {
	a->argv = XMALLOC(char*, 1);
	*(a->argv) = NULL;
	a->arg_count = 1;
}


/* Add a new argument to this argument struct. */
void args_add(struct args* a, char* new_arg) {
	char* temp;

	if (new_arg) {
		temp = XMALLOC(char, strlen(new_arg) + 1);
		strcpy(temp, new_arg);
	}
	else
		temp = NULL;

	a->argv = XREALLOC(char*, a->argv, a->arg_count + 1);
	a->argv = realloc(a->argv, sizeof(char*) * (a->arg_count + 1));
	*(a->argv + a->arg_count) = temp;
	a->arg_count++;
}


bool display_init(struct display* d) {
	pid_t pid;
	char* pid_str;
	bool ok = true;

	d->a.argv[0] = d->name;

	/* Get the current pid and turn it into a string. */
	pid_str = XMALLOC(char, 10 + 1);    /* 10 for the length of the pid, 1 for \0. */
	pid = getpid();
	sprintf(pid_str, "%ld", (long int) pid);

	/* Create the glob fifo name. */
	d->glob_fifo_name = XMALLOC(char, strlen("/tmp/viewglob") + strlen(pid_str) + strlen("-1") + 1);
	(void)strcpy(d->glob_fifo_name, "/tmp/viewglob");
	(void)strcat(d->glob_fifo_name, pid_str);
	(void)strcat(d->glob_fifo_name, "-1");

	/* Create the cmd fifo name. */
	d->cmd_fifo_name = XMALLOC(char, strlen("/tmp/viewglob") + strlen(pid_str) + strlen("-2") + 1);
	(void)strcpy(d->cmd_fifo_name, "/tmp/viewglob");
	(void)strcat(d->cmd_fifo_name, pid_str);
	(void)strcat(d->cmd_fifo_name, "-2");

	/* And the feedback fifo name. */
	d->feedback_fifo_name = XMALLOC(char, strlen("/tmp/viewglob") + strlen(pid_str) + strlen("-3") + 1);
	(void)strcpy(d->feedback_fifo_name, "/tmp/viewglob");
	(void)strcat(d->feedback_fifo_name, pid_str);
	(void)strcat(d->feedback_fifo_name, "-3");

	XFREE(pid_str);

	/* Create the fifos. */
	if ( ! create_fifo(d->glob_fifo_name) )
		return false;
	if ( ! create_fifo(d->cmd_fifo_name) )
		return false;
	if ( ! create_fifo(d->feedback_fifo_name) )
		return false;

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
bool display_fork(struct display* d) {
	bool ok = true;

	switch (d->pid = fork()) {
		case -1:
			viewglob_error("Could not fork display");
			return false;

		case 0:
			/* Open the display. */
			execvp(d->a.argv[0], d->a.argv);

			viewglob_error("Display exec failed");
			viewglob_error("If viewglob does not exit, you should do so manually");
			_exit(EXIT_FAILURE);

			break;
	}

	/* Open the fifos for writing in parent. */
	/* The sandbox shell will be outputting to the glob fifo, but seer opens it too
	   to make sure it doesn't get EOF'd by the sandbox shell accidentally. */
	if ( (d->glob_fifo_fd = open(d->glob_fifo_name, O_WRONLY)) == -1) {
		viewglob_error("Could not open glob fifo for writing");
		ok = false;
	}
	if ( (d->cmd_fifo_fd = open(d->cmd_fifo_name, O_WRONLY)) == -1) {
		viewglob_error("Could not open cmd fifo for writing");
		ok = false;
	}
	if ( (d->feedback_fifo_fd = open(d->feedback_fifo_name, O_RDONLY)) == -1) {
		viewglob_error("Could not open feedback fifo for reading");
		ok = false;
	}


	return ok;
}


bool display_running(struct display* d) {
	return d->pid != -1;
}


bool display_terminate(struct display* d) {
	bool ok = true;

	/* Close the fifos, if open. */
	if (d->glob_fifo_fd != -1) {
		if (close(d->glob_fifo_fd) != -1)
			d->glob_fifo_fd = -1;
		else {
			viewglob_error("Could not close glob fifo");
			ok = false;
		}
	}
	if (d->cmd_fifo_fd != -1) {
		if (close(d->cmd_fifo_fd) != -1)
			d->cmd_fifo_fd = -1;
		else {
			viewglob_error("Could not close cmd fifo");
			ok = false;
		}
	}
	if (d->feedback_fifo_fd != -1) {
		if (close(d->feedback_fifo_fd) != -1)
			d->feedback_fifo_fd = -1;
		else {
			viewglob_error("Could not close feedback fifo");
			ok = false;
		}
	}


	/* Terminate and wait the child's process. */
	if (d->pid != -1) {
		switch (kill(d->pid, SIGTERM)) {
			case ESRCH:
				viewglob_warning("Display already closed");
			case 0:
				ok &= waitpid_wrapped(d->pid);
				d->pid = -1;
				break;
			default:
				viewglob_error("Could not close display");
				ok = false;
				break;
		}
	}

	return ok;
}


bool display_cleanup(struct display* d) {
	bool ok = true;

	/* Remove the fifos. */
	if (d->glob_fifo_name) {
		if ( unlink(d->glob_fifo_name) == -1 ) {
			if (errno != ENOENT) {
				viewglob_warning("Could not delete glob fifo");
				viewglob_warning(d->glob_fifo_name);
			}
		}
	}
	if (d->cmd_fifo_name) {
		if ( unlink(d->cmd_fifo_name) == -1 ) {
			if (errno != ENOENT) {
				viewglob_warning("Could not delete cmd fifo");
				viewglob_warning(d->cmd_fifo_name);
			}
		}
	}

	XFREE(d->glob_fifo_name);
	XFREE(d->cmd_fifo_name);

	return ok;
}


/* Fork a new child with a pty. */
bool pty_child_fork(struct pty_child* c, int new_stdin_fd, int new_stdout_fd, int new_stderr_fd) {

	PTINFO* p;
	bool ok = true;

	c->a.argv[0] = c->name;

	/* Delimit the args with NULL. */
	args_add(&(c->a), NULL);

	/* Setup a pty for the new shell. */
	p = pt_open_master();
	if (p == NULL) {
		viewglob_error("Could not open master side of pty");
		c->pid = -1;
		c->fd = -1;
		return false;
	}

	/* This will be the interface with the new shell. */
	c->fd = PT_GET_MASTER_FD(p);

	switch ( c->pid = fork() ) {
		case -1:
			viewglob_error("Could not fork process");
			return false;
			/*break;*/

		case 0:
			if ( ! pt_open_slave(p) ) {
				viewglob_error("Could not open slave side of pty");
				goto child_fail;
			}

			/* A parameter of NEW_PTY_FD means to use the slave side of the new pty. */
			if (new_stdin_fd == NEW_PTY_FD)
				new_stdin_fd = PT_GET_SLAVE_FD(p);
			if (new_stdout_fd == NEW_PTY_FD)
				new_stdout_fd = PT_GET_SLAVE_FD(p);
			if (new_stderr_fd == NEW_PTY_FD)
				new_stderr_fd = PT_GET_SLAVE_FD(p);

			if ( dup2(new_stdin_fd, STDIN_FILENO) == -1 ) {
				viewglob_error("Could not replace stdin in child process");
				goto child_fail;
			}

			if ( dup2(new_stdout_fd, STDOUT_FILENO) == -1 ) {
				viewglob_error("Could not replace stdout in child process");
				goto child_fail;
			}

			if ( dup2(new_stderr_fd, STDERR_FILENO) == -1 ) {
				viewglob_error("Could not replace stderr in child process");
				goto child_fail;
			}

			(void)close(PT_GET_SLAVE_FD(p));
			execvp(c->name, c->a.argv);

			child_fail:
			viewglob_error("Exec failed");
			viewglob_error("If viewglob does not exit, you should do so manually");
			_exit(EXIT_FAILURE);

			break;
	}

	/* Wait for user shell to set itself up. */
	if ( ! pt_wait_master(p) ) {
		viewglob_error("Did not receive go-ahead from child");
		ok = false;
	}

	return ok;
}


bool pty_child_terminate(struct pty_child* c) {
	bool ok = true;

	/* Close the pty to the sandbox shell (if valid). */
	if ( (c->fd != -1) && (close(c->fd) == -1) ) {
		viewglob_error("Could not close pty to child");
		ok = false;
	}
	
	/* Terminate and wait the child's process. */
	if (c->pid != -1) {
		switch (kill(c->pid, SIGHUP)) {    /* SIGHUP terminates bash, but SIGTERM won't. */
			case ESRCH:
				viewglob_warning("Child already terminated");
			case 0:
				ok &= waitpid_wrapped(c->pid);
				c->pid = -1;
				break;
			default:
				viewglob_error("Could not terminate child");
				ok = false;
				break;
		}
	}

	return ok;
}

