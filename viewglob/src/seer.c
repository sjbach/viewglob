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

#include "tc_setraw.viewglob.h"
#include "hardened_io.h"
#include "sanitize.h"
#include "seer.h"

#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

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

struct options opts;

struct user_shell u;	/* Almost everything revolves around this. */
struct glob_shell x;
struct display disp;

bool viewglob_enabled;  /* This controls whether or not viewglob should actively do stuff.
                           If the display is closed, viewglob will disable itself and try
                           to just be a regular shell. */

int main(int argc, char* argv[]) {

	int devnull_fd, fifo_fd;

	bool ok = true;
	viewglob_enabled = true;

	set_program_name(argv[0]);

	/* Initialize program options. */
	opts.executable = opts.display = opts.config_file = NULL;
	opts.shell_out_file = opts.term_out_file = NULL;
	opts.init_command = opts.expand_command = NULL;
	opts.display_argv = XMALLOC(char*, 1);
	*(opts.display_argv) = NULL;
	opts.display_args = 1;

	/* Initialize the shell and display structs. */
	u.s.pid = x.s.pid = disp.pid = -1;
	u.s.fd = x.s.fd = disp.fifo_fd = -1;
	u.pl = PL_EXECUTING;
	disp.fifo_name = x.out_file = x.glob_cmd = NULL;

	/* This fills in the opts struct. */
	parse_args(argc, argv);
	disp.name = opts.display;
	u.s.name = x.s.name = opts.executable;

	/* Open up the log files, if possible. */
	u.s.transcript_fd = open_warning(opts.shell_out_file, O_CREAT | O_WRONLY | O_TRUNC, PERM_FILE);
	if (u.s.transcript_fd == -1)
		XFREE(opts.shell_out_file);
	x.s.transcript_fd = open_warning(opts.term_out_file, O_CREAT |O_WRONLY | O_TRUNC, PERM_FILE);
	if (x.s.transcript_fd == -1)
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
	if ( ! pty_child_fork(&(x.s), NEW_PTY_FD, devnull_fd, devnull_fd) ) {
		viewglob_error("Could not create sandbox shell");
		ok = false;
		goto done;
	}

	/* Don't need /dev/null anymore. */
	close_warning(devnull_fd, "/dev/null");

	/* Open the display. */
	if ( ! display_fork(&disp, opts.display_argv) ) {
		viewglob_error("Could not create display");
		ok = false;
		goto done;
	}
	x.out_file = disp.fifo_name;

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

	/* Run the shell initialization command.  Right now this just adds the viewglob
	   delimiters to $PS1 and $PROMPT_COMMAND.  This must be done after starting
	   bash instead of changing the environment exported to the child, because the
	   variables usually are overwritten by sets in bashrc. */
	if ( ! hardened_write(u.s.fd, opts.init_command, strlen(opts.init_command)) ) {
		viewglob_error("Could not write shell initialization command");
		ok = false;
		goto restore_terminal;
	}

	/* Enter main_loop. */
	if ( ! main_loop(&disp) ) {
		viewglob_error("Problem during processing");
		ok = false;
	}

restore_terminal:
	/* Turn off terminal raw mode. */
	if ( ! tc_restore() )
		viewglob_warning("Could not restore terminal attributes");

#if DEBUG_ON
	if (fclose(df) != 0)
		viewglob_warning("Could not close debug file");
#endif

done:
	close_warning(x.s.transcript_fd, opts.term_out_file);
	close_warning(u.s.transcript_fd, opts.shell_out_file);
	ok &= display_terminate(&disp);
	ok &= pty_child_terminate(&(x.s));
	ok &= pty_child_terminate(&(u.s));
	printf("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void parse_args(int argc, char** argv) {
	bool in_loop = true;
	char* temp = NULL;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "bd:e:f:i:n:o:O:s:x:w")) {
			case -1:
				in_loop = false;
				break;
			case '?':
				viewglob_fatal("Unknown argument");
				break;

			case 'b':
				/* Disable icons in display. */
				add_display_arg("-b");
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
				XFREE(opts.init_command);
				opts.init_command = XMALLOC(char, strlen(optarg) + 1 + 1);
				strcpy(opts.init_command, optarg);
				strcat(opts.init_command, "\n");
				break;
			case 'n':
				/* Maximum number of files to display per directory (unless forced). */
				add_display_arg("-n");
				add_display_arg(optarg);
				break;
			case 'o':
				/* Duplicate shell output file */
				XFREE(opts.shell_out_file);
				opts.shell_out_file = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.shell_out_file, optarg);
				break;
			case 'O':
				/* Duplicate term output file */
				XFREE(opts.term_out_file);
				opts.term_out_file = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.term_out_file, optarg);
				break;
			case 's':
				/* Display sorting style. */
				add_display_arg("-s");
				add_display_arg(optarg);
				break;
			case 'w':
				/* Show hidden files by default in display. */
				add_display_arg("-w");
				break;
			case 'x':
				/* Expand command (glob-expand) */
				XFREE(opts.expand_command);
				opts.expand_command = XMALLOC(char, strlen(optarg) + 1);
				strcpy(opts.expand_command, optarg);
				break;
		}
	}

	/* Add the NULL argv delimiter. */
	add_display_arg(NULL);

	if (!opts.display)
		viewglob_fatal("No display program specified");
	else if (!opts.executable)
		viewglob_fatal("No shell executable specified");
	else if (!opts.init_command)
		viewglob_fatal("No shell initialization command specified");
	else if (!opts.expand_command)
		viewglob_fatal("No shell expansion command specified");

	return;
}


/* Add a new argument to the display's argv. */
static void add_display_arg(char* new_arg) {
	char* temp;

	if (new_arg) {
		temp = XMALLOC(char, strlen(new_arg));
		strcpy(temp, new_arg);
	}
	else
		temp = NULL;

	opts.display_argv = XREALLOC(char*, opts.display_argv, opts.display_args + 1);
	*(opts.display_argv + opts.display_args) = temp;
	opts.display_args++;
}


/* Main program loop. */
static bool main_loop(struct display* disp) {

	enum action d;
	bool ok = true;
	bool in_loop = true;

	char* sane_cmd;

	/* Initialize working command line and sequence buffer. */
	if (!cmd_init()) {
		return -1;
	}

	/* Initialize the sequences. */
	init_seqs();

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
				DEBUG((df, "::d_exit::"));
				in_loop = false;
				break;
			}
			else if (d == A_DONE) {
				DEBUG((df, "::d_done::"));
				break;
			}
			else if (d == A_SEND_CMD) {
				DEBUG((df, "::send cmd::"));
				if (viewglob_enabled)
					send_sane_cmd(disp);
			}
			else if (d == A_SEND_PWD) {
				/* Do nothing. */
				DEBUG((df, "::send pwd::"));
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
	static char buff[BUFSIZ];
	ssize_t nread;
	fd_set fdset_read;

	bool ok = true;

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
		if (hardened_read(STDIN_FILENO, buff, sizeof buff, &nread) == false) {
			if (errno == EIO)
				goto done;
			else {
				viewglob_error("Read problem from terminal");
				ok = false;
				goto done;
			}
		}
		else if (nread == 0) {
			DEBUG((df, "~~1~~"));
			action_queue(A_EXIT);
			goto done;
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
			u.expect_newline = scan_for_newline(buff, nread);

		/* Write it out. */
		if (!hardened_write(u.s.fd, buff, nread)) {
			viewglob_error("Problem writing to shell");
			ok = false;
			goto done;
		}
		if (x.s.transcript_fd != -1 && !hardened_write(x.s.transcript_fd, buff, nread))
			viewglob_warning("Could not write term transcript");

	}

	/* If data has been written by the shell... */
	if (FD_ISSET(u.s.fd, &fdset_read)) {

		/* Read in from the shell. */
		if (hardened_read(u.s.fd, buff, sizeof buff, &nread) == false) {
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
		else if (nread == 0) {
			DEBUG((df, "~~3~~"));
			action_queue(A_EXIT);
			goto done;
		}

		/* Process the buffer. */
		if (viewglob_enabled) {
			ok = process_input(buff, &nread);
			if (!ok) goto done;
		}

		/* Write out the full buffer. */
		if (nread && !hardened_write(STDOUT_FILENO, buff, nread)) {
			viewglob_error("Problem writing to stdout");
			ok = false;
			goto done;
		}
		if (u.s.transcript_fd != -1 && !hardened_write(u.s.transcript_fd, buff, nread))
			viewglob_warning("Could not write shell transcript");
	}

	done:
	return ok;
}


/* Look for newlines, Ctrl-C, Ctrl-D, Ctrl-O and carriage returns */
static bool scan_for_newline(const char* buff, size_t n) {
	size_t i;

	for (i = 0; i < n; i++) {
		if ( *(buff + i) == '\n' || *(buff + i) == '\003'
				|| *(buff + i) == '\004' || *(buff + i) == '\015'
				|| *(buff + i) == '\017' ) {
			return true;
		}
	}

	return false;
}


/* Process the buffer read in from the shell, character by character. */
static bool process_input(char* buff, size_t* n) {

	bool is_done;
	size_t i;

	for (i = 0; i < *n; i++) {

		if (u.pl == PL_EATING) {
			/* We're reading something we don't want put to the terminal. */
			if (! eat(buff, n, &i) )
				return false;
			continue;
		}

		/* Add this character to the queue. */
		if (!seqbuff_enqueue(buff[i]))
			return false;

		DEBUG((df, "[%c] {{%s}}\n", buff[i], u.seqbuff.string));

		is_done = false;
		while (!is_done) {

			/* Remove trailing ^Ms. */
			cmd_del_trailing_crets();

			is_done = match_loop(u.pl);

			DEBUG((df, "<<<%s>>> {%d = %c}\n", u.cmd.command, u.cmd.pos, *(u.cmd.command + u.cmd.pos)));
			DEBUG((df, "length: %d\tpos: %d\tstrlen: %d\n\n", u.cmd.length, u.cmd.pos, strlen(u.cmd.command)));
		}
	}
	return true;
}


/* This is a modified version of match_loop below.  I really should wrap them into the same function (TODO)
   - Is there a possibility that part of END_EAT_SEQ will be cut off if it comes at the end of buff? */
static bool eat(char* buff, size_t* n, size_t* start) {
	static MatchStatus status = MS_NO_MATCH;
	size_t i;
	MatchEffect effect = ME_NO_EFFECT;

	for (i = *start; i < *n; i++) {

		if (!seqbuff_enqueue(buff[i]))
			return false;

		if (status != MS_IN_PROGRESS)
			enable_all_seqs(PL_EATING);

		DEBUG((df, "eating: %c? <<%s>>\n", buff[i], u.seqbuff.string));
		status = check_seqs(PL_EATING, buff[i], &effect);

		if (status & MS_MATCH) {
			/* Matched -- eat away the bad stuff. */
			clear_seqs(PL_EATING);
			memmove(buff + *start, buff + (i - 1), *n - (i - 1));
			*n -= (i - 1) - *start;
			*start = i;
			u.pl = PL_EXECUTING;
			seqbuff_dequeue(u.seqbuff.pos, false);

			return true;
		}

	}

	DEBUG((df, "no match yet.\n"));
	/* We didn't reach the end.  Truncate the data and wait for next iteration. */
	*n -= *n - *start;
	*start = *n;
	return true;
}


/* Attempt to match each sequence against u.seqbuff. */
static bool match_loop(enum process_level pl) {
	static MatchStatus status = MS_NO_MATCH;    /* Remember status of the last match_loop run. */
	MatchEffect effect = ME_NO_EFFECT;
	bool is_done = false;
	int i;

	while (true) {

		if (u.seqbuff.pos > 0) {
			if (status != MS_IN_PROGRESS) {
				/* We're not waiting on a specific match, so let's check all sequences. */
				enable_all_seqs(pl);
			}

			DEBUG((df, "process level: %d checking: %c\n", pl, u.seqbuff.string[u.seqbuff.pos - 1]));

			status = check_seqs(pl, u.seqbuff.string[u.seqbuff.pos - 1], &effect);
		}
		else {
			DEBUG((df, "...done trying to match\n"));
			is_done = true;
			break;
		}

		if (status == MS_NO_MATCH) {
			/* Pop off the oldest character, adding it to u.cmd if the user is at the
			   command line, and start over. */
			seqbuff_dequeue(1, u.pl == PL_AT_PROMPT);
		}
		else if (status == MS_IN_PROGRESS) {
			/* There are potential matches, but no more characters to check against. */
			is_done = true;
			break;
		}
		else if (status & MS_MATCH) {
			/* Pop off the matched sequence, and start over with any remaining characters.
			   Do not copy the matched sequence to u.cmd. */
			clear_seqs(pl);
			analyze_effect(effect);
			seqbuff_dequeue(u.seqbuff.pos, false);
			break;
		}
	}

	return is_done;
}


/* React (or not) on the instance of a sequence match. */
/* FIXME probably should return a bool for graceful crash. */
static void analyze_effect(MatchEffect effect) {
	static bool rebuilding = false;     /* This variable does nothing. */

	switch (effect) {

		case ME_ERROR:
			DEBUG((df, "**ERROR**\n"));
			break;

		case ME_NO_EFFECT:
			DEBUG((df, "**NO_EFFECT**\n"));
			break;

		case ME_CMD_EXECUTED:
			DEBUG((df, "**CMD_EXECUTED**\n"));
			cmd_clear();
			u.pl = PL_EXECUTING;
			break;

		case ME_CMD_STARTED:
			DEBUG((df, "**CMD_STARTED**\n"));
			if (rebuilding)
				rebuilding = false;
			else
				cmd_wipe_in_line(D_ALL);
			u.pl = PL_AT_PROMPT;
			break;

		case ME_CMD_REBUILD:
			DEBUG((df, "**CMD_REBUILD**\n"));
			rebuilding = true;
			u.pl = PL_EXECUTING;
			break;

		case ME_PWD_CHANGED:
			/* Send the new current directory. */
			action_queue(A_SEND_PWD);
			break;

		case ME_EAT_STARTED:
			u.pl = PL_EATING;
			break;

		case ME_DUMMY:
			DEBUG((df, "**DUMMY**\n"));
			break;

		default:
			/* Error -- this shouldn't happen unless I've screwed up */
			viewglob_fatal("Received unexpected match result");
			break;
		}
}

/* This function writes out stuff that looks like this:
   cmd:<command>
   cd <u.pwd> && { builtin echo -n "glob:" && <glob-command> <sane_cmd> ;} >> <fifo> ; cd /
*/
static void send_sane_cmd(struct display* d) {

	char* sane_cmd;
	char* sane_cmd_delimited;

	sane_cmd = make_sane_cmd(u.cmd.command, u.cmd.length);

	/* sane_cmd_delimited gets sent directly to the display. */
	sane_cmd_delimited = XMALLOC(char, strlen("cmd:") + strlen(sane_cmd) + strlen("\n") + 1);
	(void)strcpy(sane_cmd_delimited, "cmd:");
	(void)strcat(sane_cmd_delimited, sane_cmd);
	(void)strcat(sane_cmd_delimited, "\n");

	DEBUG((df, "\n^^^%s^^^\n", sane_cmd_delimited));
	if ( ! hardened_write(d->fifo_fd, sane_cmd_delimited, strlen(sane_cmd_delimited)) ) {
		viewglob_enabled = false;
		goto done;
	}

	XFREE(x.glob_cmd);
	x.glob_cmd = XMALLOC(char,
		strlen("cd ") + strlen(u.pwd) + strlen(" && { builtin echo -n \"glob:\" && ") +
		strlen(opts.expand_command) + strlen(" ") + strlen(sane_cmd) +
		strlen(" ;} >> ") + strlen(d->fifo_name) + strlen(" ; cd /\n") + 1);

	strcpy(x.glob_cmd, "cd ");
	strcat(x.glob_cmd, u.pwd);
	strcat(x.glob_cmd, " && { builtin echo -n \"glob:\" && ");
	strcat(x.glob_cmd, opts.expand_command);
	strcat(x.glob_cmd, " ");
	strcat(x.glob_cmd, sane_cmd);
	strcat(x.glob_cmd, " ;} >> ");
	strcat(x.glob_cmd, d->fifo_name);
	strcat(x.glob_cmd, " ; cd /\n");

	DEBUG((df, "\n[[[%s]]]\n", x.glob_cmd));
	if ( ! hardened_write(x.s.fd, x.glob_cmd, strlen(x.glob_cmd)) )
		viewglob_enabled = false;

done:
	XFREE(sane_cmd_delimited);
	XFREE(sane_cmd);
}


/* This is kind-of a queue.  If o is A_SEND_CMD, A_SEND_PWD, or A_EXIT,
   the action is queued.  If o is A_POP, then the correct queued action
   is dequeued.  Note that this doesn't follow first in, first out,
   since A_EXIT is a much more important action, so it gets dequeued
   first.  Also, a new A_SEND_PWD invalidates a previous A_SEND_CMD. */
enum action action_queue(enum action o) {

	static bool send_cmd = false;
	static bool send_pwd = false;
	static bool do_exit = false;

	switch (o) {

		case (A_SEND_CMD):
			send_cmd = true;
			break;

		case (A_SEND_PWD):
			send_cmd = false;
			send_pwd = true;
			break;

		case (A_POP):
			if (do_exit)
				return A_EXIT;
			else if (send_pwd) {
				send_pwd = false;
				return A_SEND_PWD;
			}
			else if (send_cmd) {
				send_cmd = false;
				return A_SEND_CMD;
			}
			else
				return A_DONE;
			break;

		case (A_EXIT):
			do_exit = true;
			break;

		default:
			break;
	}
	
	return A_NOP;
}


void sigwinch_handler(int signum) {
	/* Don't need to do much here. */
	u.term_size_changed = true;
	return;
}


void sigterm_handler(int signum) {
	close_warning(x.s.transcript_fd, opts.term_out_file);
	close_warning(u.s.transcript_fd, opts.shell_out_file);
	(void) display_terminate(&disp);
	(void) pty_child_terminate(&(x.s));
	(void) pty_child_terminate(&(u.s));
	(void) tc_restore();
	printf("[Exiting viewglob]\n");
	_exit(EXIT_FAILURE);
}

