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

#include "config.h"

#include "common.h"
#include "viewglob-error.h"

#include "tc_setraw.viewglob.h"
#include "hardened_io.h"
#include "sanitize.h"
#include "actions.h"
#include "buffer.h"
#include "seer.h"

#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#  define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if DEBUG_ON
FILE* df;
#endif


/* Prototypes */
static bool main_loop(struct display* d);
static bool user_activity(void);
static bool scan_for_newline(const Buffer* b);
static bool process_input(Buffer* b);

static void send_sane_cmd(struct display* d);
static void send_lost(struct display* d);

static void parse_args(int argc, char** argv);
static void report_version(void);


/* Globals */
struct options opts;

struct user_shell u;    /* Almost everything revolves around this. */
struct glob_shell x;
struct display disp;

bool viewglob_enabled;  /* This controls whether or not viewglob should actively do stuff.
                           If the display is closed, viewglob will disable itself and try
                           to just be a regular shell. */

int main(int argc, char* argv[]) {

	int devnull_fd;
	bool ok = true;

	viewglob_enabled = true;
	set_program_name(argv[0]);

	/* Initialize program options. */
	opts.shell_type = ST_BASH;
	opts.executable = opts.display = NULL;
	opts.config_file = opts.shell_out_file = opts.term_out_file = NULL;
	opts.init_loc = opts.expand_command = NULL;
	opts.navigation = true;

	/* Initialize the shell and display structs. */
	u.s.pid = x.s.pid = disp.pid = -1;
	u.s.fd = x.s.fd = disp.glob_fifo_fd = -1;
	disp.glob_fifo_name = x.glob_cmd = NULL;

	/* And the argument arrays. */
	args_init(&(disp.a));
	args_init(&(u.s.a));
	args_init(&(x.s.a));

	/* This fills in the opts struct. */
	parse_args(argc, argv);
	disp.name = opts.display;
	u.s.name = x.s.name = opts.executable;

	/* Get ready for the user shell child fork. */
	if (opts.shell_type == ST_BASH) {
		/* Bash is simple. */
		args_add(&(u.s.a), "--init-file");
		args_add(&(u.s.a), opts.init_loc);
		args_add(&(x.s.a), "--init-file");
		args_add(&(x.s.a), opts.init_loc);
	}
	else if (opts.shell_type == ST_ZSH) {
		/* Zsh requires the init file be named ".zshrc", and its location determined
		   by the ZDOTDIR environment variable. */
		char* zdotdir = XMALLOC(char, strlen("ZDOTDIR=") + strlen(opts.init_loc) + 1);
		strcpy(zdotdir, "ZDOTDIR=");
		strcat(zdotdir, opts.init_loc);
		if (putenv(zdotdir) != 0) {
			viewglob_error("Could not modify the environment");
			ok = false;
			goto done;
		}
		args_add(&(x.s.a), "+Z");  /* Disable the line editor in the sandbox shell.  This can
		                              be overridden, so we also disable it in the rc file. */
	}
	else {
		viewglob_error("Unknown shell mode");
		ok = false;
		goto done;
	}

	/* Open up the log files, if possible. */
	u.shell_transcript_fd = open_warning(opts.shell_out_file, O_CREAT | O_WRONLY | O_TRUNC, PERM_FILE);
	if (u.shell_transcript_fd == -1)
		XFREE(opts.shell_out_file);
	u.term_transcript_fd = open_warning(opts.term_out_file, O_CREAT |O_WRONLY | O_TRUNC, PERM_FILE);
	if (u.term_transcript_fd == -1)
		XFREE(opts.term_out_file);

	/* Setup a shell for the user. */
	if ( ! pty_child_fork(&(u.s), NEW_PTY_FD, NEW_PTY_FD, NEW_PTY_FD) ) {
		viewglob_error("Could not create user shell");
		ok = false;
		goto done;
	}

	/* Open /dev/null for use in setting up the sandbox shell. */
	if ( (devnull_fd = open_warning("/dev/null", O_WRONLY, 0)) == -1) {
		viewglob_error("(Attempted on purpose)");
		ok = false;
		goto done;
	}

	/* Setup another shell to glob stuff in. */
	putenv("VG_SANDBOX=yep");
	if ( ! pty_child_fork(&(x.s), NEW_PTY_FD, devnull_fd, devnull_fd) ) {
		viewglob_error("Could not create sandbox shell");
		ok = false;
		goto done;
	}

	/* Don't need /dev/null anymore. */
	close_warning(devnull_fd, "/dev/null");

	/* Open the display. */
	if ( ! display_fork(&disp) ) {
		viewglob_error("Could not create display");
		ok = false;
		goto done;
	}

#if DEBUG_ON
	df = fopen("/tmp/out1.txt", "w");
#endif

	if ( ! handle_signals() ) {
		viewglob_error("Could not set up signal handlers");
		ok = false;
		goto done;
	}

	/* Send the terminal size to the user's shell. */
	if ( ! send_term_size(u.s.fd) ) {
		viewglob_error("Could not set terminal size");
		ok = false;
		goto done;
	}

	if ( ! tc_setraw() ) {
		viewglob_error("Could not set raw mode");
		ok = false;
		goto done;
	}

	/* Enter main_loop. */
	if ( ! main_loop(&disp) ) {
		viewglob_error("Problem during processing");
		ok = false;
	}

	/* Done -- Turn off terminal raw mode. */
	if ( ! tc_restore() )
		viewglob_warning("Could not restore terminal attributes");

#if DEBUG_ON
	if (fclose(df) != 0)
		viewglob_warning("Could not close debug file");
#endif

done:
	close_warning(u.term_transcript_fd, opts.term_out_file);
	close_warning(u.shell_transcript_fd, opts.shell_out_file);
	ok &= display_terminate(&disp);
	ok &= pty_child_terminate(&(x.s));
	ok &= pty_child_terminate(&(u.s));
	printf("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void parse_args(int argc, char** argv) {
	bool in_loop = true;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "bc:d:e:f:i:n:o:O:s:vVx:w")) {
			case -1:
				in_loop = false;
				break;
			case '?':
				viewglob_fatal("Unknown argument");
				break;

			case 'b':
				/* Disable icons in display. */
				args_add(&(disp.a), "-b");
				break;

			case 'c':
				/* Set the shell mode. */
				if (strcmp(optarg, "bash") == 0)
					opts.shell_type = ST_BASH;
				else if (strcmp(optarg, "zsh") == 0)
					opts.shell_type = ST_ZSH;
				else
					viewglob_fatal("Unknown shell mode");
				break;

			case 'd':
				/* Display program */
				XFREE(opts.display);
				opts.display = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.display, optarg);
				break;
			case 'e':
				/* Shell executable */
				XFREE(opts.executable);
				opts.executable = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.executable, optarg);
				break;
			case 'f':
				/* Configuration file (for display) */
				XFREE(opts.config_file);
				opts.config_file = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.config_file, optarg);
				break;
			case 'i':
				/* Shell initialization command */
				XFREE(opts.init_loc);
				opts.init_loc = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.init_loc, optarg);
				break;
			case 'n':
				/* Maximum number of files to display per directory (unless forced). */
				args_add(&(disp.a), "-n");
				args_add(&(disp.a), optarg);
				break;
			case 'o':
				/* Duplicate shell output file */
				if (optarg && strlen(optarg) > 0) {
					/* Do nothing if there's no argument. */
					XFREE(opts.shell_out_file);
					opts.shell_out_file = XMALLOC(char, strlen(optarg) + 1);
					strcpy(opts.shell_out_file, optarg);
				}
				break;
			case 'O':
				/* Duplicate term output file */
				if (optarg && strlen(optarg) > 0) {
					/* Do nothing if there's no argument. */
					XFREE(opts.term_out_file);
					opts.term_out_file = XMALLOC(char, strlen(optarg) + 1);
					strcpy(opts.term_out_file, optarg);
				}
				break;
			case 's':
				/* Display sorting style. */
				args_add(&(disp.a), "-s");
				args_add(&(disp.a), optarg);
				break;
			case 'v':
			case 'V':
				report_version();
				exit(EXIT_SUCCESS);
				break;
			case 'w':
				/* Show hidden files by default in display. */
				args_add(&(disp.a), "-w");
				break;
			case 'x':
				/* Expand command (glob-expand) */
				XFREE(opts.expand_command);
				opts.expand_command = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.expand_command, optarg);
				break;
		}
	}

	if (!opts.display)
		viewglob_fatal("No display program specified");
	else if (!opts.executable)
		viewglob_fatal("No shell executable specified");
	else if (!opts.init_loc)
		viewglob_fatal("No shell initialization command specified");
	else if (!opts.expand_command)
		viewglob_fatal("No shell expansion command specified");

	return;
}


static void report_version(void) {
	printf("seer %s\n", VERSION);
	printf("Released %s\n", VG_RELEASE_DATE);
	return;
}


/* Main program loop. */
static bool main_loop(struct display* disp) {

	Action d;
	bool ok = true;
	bool in_loop = true;

	/* Initialize working command line and sequence buffer. */
	if (!cmd_init()) {
		return -1;
	}

	/* Initialize the sequences. */
	init_seqs(opts.shell_type);

	while (in_loop) {

		if ( ! user_activity() ) {
			ok = false;
			break;
		}

		/* FIXME
		   - A_SEND_PWD has no use. */
		while (true) {
			d = action_queue(A_POP);
			if (d == A_EXIT) {
				DEBUG((df, "::d_exit::\n"));
				in_loop = false;
				break;
			}
			else if (d == A_DONE) {
				DEBUG((df, "::d_done::\n"));
				break;
			}
			else if (d == A_SEND_CMD) {
				DEBUG((df, "::send cmd::\n"));
				if (viewglob_enabled)
					send_sane_cmd(disp);
			}
			else if (d == A_SEND_PWD) {
				/* Do nothing. */
				DEBUG((df, "::send pwd::\n"));
			}
			else if (d == A_SEND_LOST) {
				/* blah */
				if (viewglob_enabled)
					send_lost(disp);
			}
			else {
				viewglob_error("Received unexpected action");
				ok = in_loop = false;
				break;
			}
		}
	}

	XFREE(u.cmd.command);
	return ok;
}


/* Wait for input from the user's shell and the terminal.  If the
   terminal receives input, it's passed along pretty much untouched.  If
   the shell receives input, it's examined thoroughly with process_input. */
static bool user_activity(void) {

	static Buffer termb =  { NULL, BUFSIZ, 0, 0, 0, PL_TERMINAL,  MS_NO_MATCH, NULL };
	static Buffer shellb = { NULL, BUFSIZ, 0, 0, 0, PL_EXECUTING, MS_NO_MATCH, NULL };

	fd_set fdset_read;
	bool ok = true;

	/* This only happens once. */
	if (!termb.buf && !shellb.buf) {
		termb.buf = XMALLOC(char, termb.size);
		shellb.buf = XMALLOC(char, shellb.size);
	}

	/* Check to see if the user shell has been closed.  This is necessary
	   because for some reason if the shell opens an external program such
       as gvim and then exits, it will sit and wait for the external
	   program to end (even if it's not attached to the terminal).  I
	   don't know why.  So here we force an exit if the user's shell
	   closes, and the external programs can stay open. */
	switch (waitpid(u.s.pid, NULL, WNOHANG)) {
		case 0:
			break;
		case -1:
		default:
			DEBUG((df,"[user shell dead]"));
			u.s.pid = -1;    /* So we don't try to kill it in cleanup. */
			action_queue(A_EXIT);
			goto done;
			break;
	}

	/* Poll for output from the shell and from the terminal */
	FD_ZERO(&fdset_read);
	FD_SET(STDIN_FILENO, &fdset_read);    /* Terminal. */
	FD_SET(u.s.fd, &fdset_read);          /* Shell. */
	if (!hardened_select(u.s.fd + 1, &fdset_read, NULL)) {
		viewglob_error("Problem while waiting for input");
		ok = false;
		goto done;
	}

	/* If data has been written by the terminal... */
	if (FD_ISSET(STDIN_FILENO, &fdset_read)) {

		/* Read in from terminal. */
		if (!hardened_read(STDIN_FILENO, termb.buf, termb.size, &termb.filled)) {
			if (errno == EIO)
				goto done;
			else {
				viewglob_error("Read problem from terminal");
				ok = false;
				goto done;
			}
		}
		else if (termb.filled == 0) {
			DEBUG((df, "~~1~~"));
			action_queue(A_EXIT);
			goto done;
		}

		#if DEBUG_ON
			size_t x;
			DEBUG((df, "read %d bytes from the terminal:\n===============\n", termb.filled));
			for (x = 0; x < termb.filled; x++)
				DEBUG((df, "%c", termb.buf[x]));
			DEBUG((df, "\n===============\n"));
		#endif

		/* Process the buffer. */
		if (viewglob_enabled) {
			ok = process_input(&termb);
			if (!ok) goto done;
		}

		/* Look for a newline.  If one is found, then a match of a newline/carriage
		   return in the shell's output (which extends past the end of the command-
		   line) will be interpreted as command execution.  Otherwise, they'll be
		   interpreted as a command wrap.  This is a heuristic (I can't see a
		   guaranteed way to relate shell input to output); in my testing, it works
		   very well for a person typing at a shell (i.e. 1 char length buffers),
		   but less well when text is pasted in (i.e. multichar length buffers).
		   Ideas for improvement are welcome. */
		if (viewglob_enabled)
			u.expect_newline = scan_for_newline(&termb);

		/* Write it out. */
		if (!hardened_write(u.s.fd, termb.buf, termb.filled)) {
			viewglob_error("Problem writing to shell");
			ok = false;
			goto done;
		}
		if (u.term_transcript_fd != -1 && !hardened_write(u.term_transcript_fd, termb.buf, termb.filled))
			viewglob_warning("Could not write term transcript");

	}

	/* If data has been written by the shell... */
	if (FD_ISSET(u.s.fd, &fdset_read)) {

		/* Read in from the shell. */
		if (!hardened_read(u.s.fd, shellb.buf, shellb.size, &shellb.filled)) {
			if (errno == EIO) {
				DEBUG((df, "~~2~~"));
				action_queue(A_EXIT);
				goto done;
			}
			else {
				viewglob_error("Read problem from shell");
				ok = false;
				goto done;
			}
		}
		else if (shellb.filled == 0) {
			DEBUG((df, "~~3~~"));
			action_queue(A_EXIT);
			goto done;
		}

		#if DEBUG_ON
			size_t x;
			DEBUG((df, "read %d bytes from the shell:\n===============\n", shellb.filled));
			for (x = 0; x < shellb.filled; x++)
				DEBUG((df, "%c", shellb.buf[x]));
			DEBUG((df, "\n===============\n"));
		#endif

		/* Process the buffer. */
		if (viewglob_enabled) {
			ok = process_input(&shellb);
			if (!ok) goto done;
		}

		/* Write out the full buffer. */
		if (shellb.filled && !hardened_write(STDOUT_FILENO, shellb.buf, shellb.filled)) {
			viewglob_error("Problem writing to stdout");
			ok = false;
			goto done;
		}
		if (u.shell_transcript_fd != -1 && !hardened_write(u.shell_transcript_fd, shellb.buf, shellb.filled))
			viewglob_warning("Could not write shell transcript");
	}

	done:
	return ok;
}


/* Look for characters which can break a line. */
//static bool scan_for_newline(const char* buff, size_t n) {
static bool scan_for_newline(const Buffer* b) {
	size_t i;

	for (i = 0; i < b->filled; i++) {
		switch ( *(b->buf + i) ) {
			case '\n':     /* Newline. */
			case '\t':     /* Horizontal tab (for tab completion with multiple potential hits). */
			case '\003':   /* End of text -- Ctrl-C. */
			case '\004':   /* End of transmission -- Ctrl-D. */
			case '\015':   /* Carriage return -- this is the Enter key. */
			case '\017':   /* Shift in -- Ctrl-O (operate-and-get-next in bash readline). */
				return true;
//			case '!':
//				action_queue(A_SEND_LOST);
				break;
			default:
				break;
		}
	}

	return false;
}


static bool process_input(Buffer* b) {

	if (b->holdover)
		prepend_holdover(b);
	else {
		b->pos = 0;
		b->n = 1;
	}

	while (b->pos + (b->n - 1) < b->filled) {

		DEBUG((df, "Pos at \'%c\' (%d of %d)\n", b->buf[b->pos], b->pos, b->filled - 1));

		cmd_del_trailing_crets();

		if (! IN_PROGRESS(b->status))
			enable_all_seqs(b->pl);

		#if DEBUG_ON
			size_t i;
			DEBUG((df, "process level: %d checking: --%c %d--\n(", b->pl, b->buf[b->pos + (b->n - 1)], b->pos + (b->n - 1)));
			for (i = 0; i < b->n; i++)
				DEBUG((df, "%c", b->buf[b->pos + i]));
			DEBUG((df, ") |%d|\n", b->n));
		#endif

		check_seqs(b);

		if (b->status & MS_MATCH) {
			DEBUG((df, "*MS_MATCH*\n"));
			clear_seqs(b->pl);
		}
		else if (b->status & MS_IN_PROGRESS) {
			DEBUG((df, "*MS_IN_PROGRESS*\n"));
			b->n++;
		}
		else if (b->status & MS_NO_MATCH) {
			DEBUG((df, "*MS_NO_MATCH*\n"));
			if (b->pl == PL_AT_PROMPT) {
				cmd_overwrite_char(b->buf[b->pos], true);
				action_queue(A_SEND_CMD);
			}
			b->pos++;
			b->n = 1;
		}

		DEBUG((df, "<<<%s>>> {%d = %c}\n", u.cmd.command, u.cmd.pos, *(u.cmd.command + u.cmd.pos)));
		DEBUG((df, "length: %d\tpos: %d\tstrlen: %d\n\n", u.cmd.length, u.cmd.pos, strlen(u.cmd.command)));
	}

	if (IN_PROGRESS(b->status)) {
		if (b->pl == PL_AT_PROMPT) {
			/* We're at the prompt, so we can't make a holdover for the next read because the user's
			   actions may depend on this output.  This is an unfortunate kludge. */
			DEBUG((df, "pre-emptive write of segment (because of PL_AT_PROMPT)\n"));
			for (; b->pos < b->filled; b->pos++)
				cmd_overwrite_char(b->buf[b->pos], true);
			action_queue(A_SEND_CMD);
		}
		else {
			/* We'll store the sequence thus far as holdover. */
			DEBUG((df, "n = %d, pos = %d, filled = %d\n", b->n, b->pos, b->filled));
			create_holdover(b);
		}
	}

	return true;
}


/* This function writes out stuff that looks like this:
   cmd:<sane_command>
   cd "<u.pwd>" && <glob-command> <sane_cmd> >> <glob fifo> ; cd / */
static void send_sane_cmd(struct display* d) {

	char* sane_cmd;
	char* sane_cmd_delimited;

	sane_cmd = make_sane_cmd(u.cmd.command, u.cmd.length);

	/* sane_cmd_delimited gets sent directly to the display. */
	sane_cmd_delimited = XMALLOC(char, strlen("cmd:") + strlen(sane_cmd) + strlen("\n") + 1);
	(void)strcpy(sane_cmd_delimited, "cmd:");
	(void)strcat(sane_cmd_delimited, sane_cmd);
	(void)strcat(sane_cmd_delimited, "\n");

	/* TODO don't send the command if it's reasonably unchanged. */
	XFREE(x.glob_cmd);
	x.glob_cmd = XMALLOC(char,
		strlen("cd \"") + strlen(u.pwd) + strlen("\" && ") +
		strlen(opts.expand_command) + strlen(" ") + strlen(sane_cmd) +
		strlen(" >> ") + strlen(d->glob_fifo_name) + strlen(" ; cd /\n") + 1);

	strcpy(x.glob_cmd, "cd \"");
	strcat(x.glob_cmd, u.pwd);
	strcat(x.glob_cmd, "\" && ");
	strcat(x.glob_cmd, opts.expand_command);
	strcat(x.glob_cmd, " ");
	strcat(x.glob_cmd, sane_cmd);
	strcat(x.glob_cmd, " >> ");
	strcat(x.glob_cmd, d->glob_fifo_name);
	strcat(x.glob_cmd, " ; cd /\n");

	DEBUG((df, "\n^^^%s^^^\n", sane_cmd_delimited));
	DEBUG((df, "\n[[[%s]]]\n", x.glob_cmd));

	/* Write the sanitized command line to the cmd_fifo, then the glob command to the sandbox
	   shell (which sends it to the display). */
	if ( (! hardened_write(d->cmd_fifo_fd, sane_cmd_delimited, strlen(sane_cmd_delimited))) ||
	     (! hardened_write(x.s.fd, x.glob_cmd, strlen(x.glob_cmd)))) {
		fprintf(stderr, "(viewglob disabled)");
		viewglob_enabled = false;
	}

	XFREE(sane_cmd_delimited);
	XFREE(sane_cmd);
}


static void send_lost(struct display* d) {
	if (!hardened_write(d->cmd_fifo_fd, "order:lost\n", strlen("order:lost\n"))) {
		fprintf(stderr, "(viewglob disabled)");
		viewglob_enabled = false;
	}
}


void sigwinch_handler(int signum) {
	/* Don't need to do much here. */
	u.term_size_changed = true;
	return;
}


void sigterm_handler(int signum) {
	close_warning(u.term_transcript_fd, opts.term_out_file);
	close_warning(u.shell_transcript_fd, opts.shell_out_file);
	(void) display_terminate(&disp);
	(void) pty_child_terminate(&(x.s));
	(void) pty_child_terminate(&(u.s));
	(void) tc_restore();
	printf("[Exiting viewglob]\n");
	_exit(EXIT_FAILURE);
}

