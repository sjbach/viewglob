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
static bool write_to_shell(Buffer* b);
static bool scan_for_newline(const Buffer* b);
static bool process_input(Buffer* b);

static bool  is_key(Buffer* b);
static bool  is_filename(Buffer* b);
static bool  convert_to_filename(enum process_level pl, char* holdover, Buffer* src_b, Buffer* dest_b);
static bool  convert_to_key(Buffer* src_b, Buffer* dest_b);

static void send_sane_cmd(struct display* d);
static void send_order(struct display* d, Action a);

static void parse_args(int argc, char** argv);
static void report_version(void);

static void disable_viewglob(void);


/* Globals */
struct options opts;

struct user_shell u;    /* Almost everything revolves around this. */
struct glob_shell x;
struct display disp;

bool viewglob_enabled;  /* This controls whether or not viewglob should actively do stuff.
                           If the display is closed, viewglob will disable itself and try
                           to just be a regular shell. */

int main(int argc, char* argv[]) {

	bool ok = true;

	viewglob_enabled = true;
	set_program_name(argv[0]);

	/* Initialize program options. */
	opts.shell_type = ST_BASH;
	opts.executable = opts.display = NULL;
	opts.config_file = opts.shell_out_file = opts.term_out_file = NULL;
	opts.init_loc = opts.expand_command = NULL;
	opts.smart_insert = true;

	/* Initialize the shell and display structs. */
	u.s.pid = x.s.pid = disp.pid = -1;
	u.s.fd = x.s.fd = -1;
	disp.glob_fifo_fd = disp.cmd_fifo_fd = disp.feedback_fifo_fd = -1;
	disp.glob_fifo_name = x.glob_cmd = NULL;

#if DEBUG_ON
	df = fopen("/tmp/out1.txt", "w");
#endif

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
		/* In my FreeBSD installation, unless bash is executed explicitly as
		   interactive, it causes issues when exiting the program.  Adding this flag
		   doesn't hurt, so why not. */
		args_add(&(u.s.a), "-i");
		args_add(&(x.s.a), "-i");
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

	/* Setup another shell to glob stuff in. */
	putenv("VG_SANDBOX=yep");
	if ( ! pty_child_fork(&(x.s), NEW_PTY_FD, CLOSE_FD, CLOSE_FD) ) {
		viewglob_error("Could not create sandbox shell");
		ok = false;
		goto done;
	}

	/* Open the display. */
	if ( (!display_init(&disp)) || (!display_fork(&disp)) ) {
		viewglob_error("Could not create display");
		ok = false;
		goto done;
	}

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
	ok &= display_cleanup(&disp);
	ok &= pty_child_terminate(&(x.s));
	ok &= pty_child_terminate(&(u.s));
	printf("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void parse_args(int argc, char** argv) {
	bool in_loop = true;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "bc:d:e:f:i:mn:o:O:s:vVx:w")) {
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
			case 'm':
				opts.smart_insert = false;
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

	Action a = A_NOP;
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
		for (a = action_queue(A_POP); in_loop && (a != A_DONE); a = action_queue(A_POP)) {
			switch (a) {
				case A_EXIT:
					DEBUG((df, "::d_exit::\n"));
					in_loop = false;
					break;

				case A_DISABLE:
					DEBUG((df, "::disable::\n"));
					disable_viewglob();
					if (display_running(disp))
						display_terminate(disp);
					break;

				case A_SEND_CMD:
					DEBUG((df, "::send cmd::\n"));
					if (viewglob_enabled && display_running(disp))
						send_sane_cmd(disp);
					break;

				case A_SEND_PWD:
					/* Do nothing. */
					DEBUG((df, "::send pwd::\n"));
					break;

				case A_TOGGLE:
					/* Fork or terminate the display. */
					DEBUG((df, "::toggle::\n"));
					if (viewglob_enabled) {
						if (display_running(disp))
							display_terminate(disp);
						else {
							display_fork(disp);
							action_queue(A_SEND_CMD);
						}
					}
					break;

				case A_SEND_LOST:
				case A_SEND_UP:
				case A_SEND_DOWN:
				case A_SEND_PGUP:
				case A_SEND_PGDOWN:
					DEBUG((df, "::send order::\n"));
					if (viewglob_enabled && display_running(disp))
						send_order(disp, a);
					break;

				case A_DONE:
					DEBUG((df, "::d_done::\n"));
					break;

				default:
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

	static Buffer term_b =  { NULL, BUFSIZ, 0, 0, 0, PL_TERMINAL,  MS_NO_MATCH, NULL };
	static Buffer shell_b = { NULL, BUFSIZ, 0, 0, 0, PL_EXECUTING, MS_NO_MATCH, NULL };
	static Buffer sandbox_b = { NULL, BUFSIZ, 0, 0, 0, PL_EXECUTING, MS_NO_MATCH, NULL };
	static Buffer display_b = { NULL, BUFSIZ, 0, 0, 0, PL_TERMINAL, MS_NO_MATCH, NULL };

	fd_set fdset_read;
	int max_fd;
	bool data_converted;
	bool ok = true;

	/* This only happens once. */
	if (!term_b.buf && !shell_b.buf) {
		term_b.buf = XMALLOC(char, term_b.size);
		shell_b.buf = XMALLOC(char, shell_b.size);
		sandbox_b.buf = XMALLOC(char, sandbox_b.size);
		display_b.buf = XMALLOC(char, display_b.size);
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

	/* Setup the polling. */
	FD_ZERO(&fdset_read);
	FD_SET(STDIN_FILENO, &fdset_read);    /* Terminal. */
	FD_SET(u.s.fd, &fdset_read);          /* User shell. */
	FD_SET(x.s.fd, &fdset_read);          /* Sandbox shell. */
	if (display_running(&disp)) {
		FD_SET(disp.feedback_fifo_fd, &fdset_read);    /* Display feedback. */
		max_fd = MAX(MAX(MAX(STDIN_FILENO, x.s.fd), u.s.fd), disp.feedback_fifo_fd);
	}
	else
		max_fd = MAX(MAX(STDIN_FILENO, u.s.fd), x.s.fd);

	/* Poll for output from the shell, terminal, and maybe the display. */
	if (!hardened_select(max_fd + 1, &fdset_read, NULL)) {
		viewglob_error("Problem while waiting for input");
		ok = false;
		goto done;
	}

	/* If data has been written by the display... */
	if (display_running(&disp)) {
		if (FD_ISSET(disp.feedback_fifo_fd, &fdset_read)) {
			if (!hardened_read(disp.feedback_fifo_fd, display_b.buf, display_b.size, &display_b.filled)) {
				if (errno == EIO)
					goto done;
				else {
					viewglob_error("Read problem from display");
					ok = false;
					goto done;
				}
			}
			else if (display_b.filled == 0) {
				/* The display was closed ("toggled"). */
				DEBUG((df, "~~0~~"));
				action_queue(A_TOGGLE);
				goto done;
			}
			else {
				#if DEBUG_ON
					size_t x;
					DEBUG((df, "read %d bytes from display:\n===============\n", display_b.filled));
					for (x = 0; x < display_b.filled; x++)
						DEBUG((df, "%c", display_b.buf[x]));
					DEBUG((df, "\n===============\n"));
				#endif

				/* Transform the display's output to something the shell can use. */
				if (is_filename(&display_b))
					data_converted = convert_to_filename(shell_b.pl, shell_b.holdover, &display_b, &term_b);
				else if (is_key(&display_b))
					data_converted = convert_to_key(&display_b, &term_b);
				else
					data_converted = false;

				if (data_converted) {
					/* Pretend this data came from the terminal. */
					ok = write_to_shell(&term_b);
					if (!ok)
						goto done;
				}
			}
		}
	}

	/* If data has been written by the terminal... */
	if (FD_ISSET(STDIN_FILENO, &fdset_read)) {

		/* Read in from terminal. */
		if (!hardened_read(STDIN_FILENO, term_b.buf, term_b.size, &term_b.filled)) {
			if (errno == EIO)
				goto done;
			else {
				viewglob_error("Read problem from terminal");
				ok = false;
				goto done;
			}
		}
		else if (term_b.filled == 0) {
			DEBUG((df, "~~1~~"));
			action_queue(A_EXIT);
			goto done;
		}

		ok = write_to_shell(&term_b);
		if (!ok)
			goto done;
	}

	/* If data has been written by the user's shell... */
	if (FD_ISSET(u.s.fd, &fdset_read)) {

		/* Read in from the shell. */
		if (!hardened_read(u.s.fd, shell_b.buf, shell_b.size, &shell_b.filled)) {
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
		else if (shell_b.filled == 0) {
			DEBUG((df, "~~3~~"));
			action_queue(A_EXIT);
			goto done;
		}

		#if DEBUG_ON
			size_t x;
			DEBUG((df, "read %d bytes from the shell:\n===============\n", shell_b.filled));
			for (x = 0; x < shell_b.filled; x++)
				DEBUG((df, "%c", shell_b.buf[x]));
			DEBUG((df, "\n===============\n"));
		#endif

		/* Process the buffer. */
		if (viewglob_enabled) {
			ok = process_input(&shell_b);
			if (!ok) goto done;
		}

		/* Write out the full buffer. */
		if (shell_b.filled && !hardened_write(STDOUT_FILENO, shell_b.buf + shell_b.skip, shell_b.filled - shell_b.skip)) {
			viewglob_error("Problem writing to stdout");
			ok = false;
			goto done;
		}
		if (u.shell_transcript_fd != -1 && !hardened_write(u.shell_transcript_fd, shell_b.buf + shell_b.skip, shell_b.filled - shell_b.skip))
			viewglob_warning("Could not write shell transcript");
	}

	/* If data has been written by the sandbox shell...
	   This shouldn't happen, but it does.  The shell seems to echo data if it's given input while
	   a command is running.  I can't figure out how to disable that, so we've got to periodically
	   read the data so that the pipe doesn't fill up and lock the program in send_*(). */
	if (FD_ISSET(x.s.fd, &fdset_read)) {
		if (!hardened_read(x.s.fd, sandbox_b.buf, sandbox_b.size, &sandbox_b.filled)) {
			viewglob_error("Read problem from sandbox");
			viewglob_error(strerror(errno));
		}
		/* We just read this data to clear the pipe -- we don't actually use it. */
	}

	done:
	return ok;
}


/* Look for characters which can break a line. */
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
			default:
				break;
		}
	}

	return false;
}


/* Convert the data in src_b into an escaped filename (maybe) and copy it into dest_b. */
static bool convert_to_filename(enum process_level pl, char* holdover, Buffer* src_b, Buffer* dest_b) {
	size_t i, j;
	char c;

	i = strlen("file:");  /* Point to the first character after the ':'. */
	j = 0;

	if (pl == PL_AT_PROMPT && opts.smart_insert) {
		/* If there's no whitespace to the left, add a space at the beginning. */
		if (!cmd_whitespace_to_left(holdover))
			*(dest_b->buf + j++) = ' ';
	}

	/* Fill in the filename. */
	for (; i < src_b->filled && (c = *(src_b->buf + i)) != '\0'; i++) {
		switch(c) {
			/* Shell special characters. */
			case '*':
			case '?':
			case '$':
			case '|':
			case '&':
			case ';':
			case '(':
			case ')':
			case '<':
			case '>':
			case ' ':
			case '\t':
			case '\n':
			case '[':
			case ']':
			case '#':
			case '\'':
			case '\"':
			case '`':
			case ',':
			case ':':
			case '{':
			case '}':
			case '~':
			case '\\':
			case '!':
				if (pl == PL_AT_PROMPT || !opts.smart_insert)
					*(dest_b->buf + j++) = '\\';
			default:
				*(dest_b->buf + j++) = c;
				break;
		}
	}

	if (pl == PL_AT_PROMPT && opts.smart_insert) {
		/* If there's no whitespace to the right, add a space at the end. */
		if (!cmd_whitespace_to_right())
			*(dest_b->buf + j++) = ' ';
	}

	dest_b->filled = j;
	return true;
}


/* Convert the data in src_b into a key and copy it into dest_b. */
static bool convert_to_key(Buffer* src_b, Buffer* dest_b) {
	*(dest_b->buf) = *(src_b->buf + strlen("key:"));
	dest_b->filled = 1;

	DEBUG((df, "(the key is: %c)\n", *(dest_b->buf)));
	return true;
}


static bool write_to_shell(Buffer* b) {
	bool ok = true;

	#if DEBUG_ON
		size_t x;
		DEBUG((df, "read %d bytes from the terminal:\n===============\n", b->filled));
		for (x = 0; x < b->filled; x++)
			DEBUG((df, "%c", b->buf[x]));
		DEBUG((df, "\n===============\n"));
	#endif

	/* Process the buffer. */
	if (viewglob_enabled) {
		ok = process_input(b);
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
		u.expect_newline = scan_for_newline(b);

	/* Write it out. */
	if (!hardened_write(u.s.fd, b->buf + b->skip, b->filled - b->skip)) {
		viewglob_error("Problem writing to shell");
		ok = false;
		goto done;
	}
	if (u.term_transcript_fd != -1 && !hardened_write(u.term_transcript_fd, b->buf + b->skip, b->filled - b->skip))
		viewglob_warning("Could not write term transcript");

	done:
	return ok;
}


static bool process_input(Buffer* b) {

	if (b->holdover)
		prepend_holdover(b);
	else {
		b->pos = 0;
		b->n = 1;
		b->skip = 0;
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
				cmd_overwrite_char(b->buf[b->pos], false);
				action_queue(A_SEND_CMD);
			}
			b->pos++;
			b->n = 1;
		}

		DEBUG((df, "<<<%s>>> {%d = %c}\n", u.cmd.command, u.cmd.pos, *(u.cmd.command + u.cmd.pos)));
		DEBUG((df, "length: %d\tpos: %d\tstrlen: %d\n\n", u.cmd.length, u.cmd.pos, strlen(u.cmd.command)));
	}

	if (IN_PROGRESS(b->status)) {
		DEBUG((df, "n = %d, pos = %d, filled = %d\n", b->n, b->pos, b->filled));
		create_holdover(b, b->pl != PL_AT_PROMPT);
	}

	return true;
}


static bool is_filename(Buffer* b) {
	bool result;

	if ( *(b->buf) != 'f' || b->filled < 7 )
		result = false;
	else 
		result = !strncmp("file:", b->buf, 5);

	return result;
}


static bool is_key(Buffer* b) {
	bool result;

	if (*(b->buf) != 'k' || b->filled < 5)
		result = false;
	else
		result = !strncmp("key:", b->buf, 4);

	return result;
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

	DEBUG((df, "writing %s", sane_cmd_delimited));
	DEBUG((df, "[[[%s]]]\n", x.glob_cmd));

	/* Write the sanitized command line to the cmd_fifo, then the glob command to the sandbox
	   shell (which sends it to the display). */
	if ( (! hardened_write(d->cmd_fifo_fd, sane_cmd_delimited, strlen(sane_cmd_delimited))) ||
	     (! hardened_write(x.s.fd, x.glob_cmd, strlen(x.glob_cmd))))
		disable_viewglob();

	DEBUG((df, "done writing\n"));

	XFREE(sane_cmd_delimited);
	XFREE(sane_cmd);
}


static void send_order(struct display* d, Action a) {
	char* order;

	switch (a) {
		case A_SEND_LOST:
			order = "order:lost\n";
			break;
		case A_SEND_UP:
			order = "order:up\n";
			break;
		case A_SEND_DOWN:
			order = "order:down\n";
			break;
		case A_SEND_PGUP:
			order = "order:pgup\n";
			break;
		case A_SEND_PGDOWN:
			order = "order:pgdown\n";
			break;
		default:
			viewglob_warning("Unexpection action in send_order().");
			return;
	}

	if (!hardened_write(d->cmd_fifo_fd, order, strlen(order)))
		disable_viewglob();
}


static void disable_viewglob(void) {
	fprintf(stderr, "(viewglob disabled)");
	viewglob_enabled = false;
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
	(void) display_cleanup(&disp);
	(void) pty_child_terminate(&(x.s));
	(void) pty_child_terminate(&(u.s));
	(void) tc_restore();
	printf("[Exiting viewglob]\n");
	_exit(EXIT_FAILURE);
}

