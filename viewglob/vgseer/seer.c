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

#include "config.h"

#include "vgseer-common.h"
#include "viewglob-error.h"

#include "tc_setraw.h"
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


/* FIXME Maybe the daemon should sanitize the command line instead so someone can't write a malicious client */
// FIXME also add viewglob_enabled here?
struct user_state_info {
	gchar* cli;
	gchar* pwd;
	gboolean cli_changed;
	gboolean pwd_changed;
};

/* --- Prototypes --- */

/* Signal stuff */
static void     sigwinch_handler(gint signum);
static void     sigterm_handler(gint signum);
static gboolean handle_signals(void);
static void     handler(gint signum);
static size_t   strlen_safe(const gchar* string);

/* Program flow */
static gboolean main_loop(void);
static gboolean user_activity(void);
static gboolean write_to_shell(Buffer* b);
static gboolean scan_for_newline(const Buffer* b);
static gboolean process_input(Buffer* b);

static gboolean  is_key(Buffer* b);
static gboolean  is_filename(Buffer* b);
static gboolean  is_xid(Buffer* b);
static gboolean  convert_to_filename(enum process_level pl, gchar* holdover, Buffer* src_b, Buffer* dest_b);
static gboolean  convert_to_key(Buffer* src_b, Buffer* dest_b);
static gboolean  convert_to_xid(Buffer* src_b, struct display* d);

static void send_sane_cmd(struct display* d);
static void send_order(struct display* d, Action a);

static void parse_args(gint argc, gchar** argv);
static void report_version(void);

static void set_term_title(gint fd, gchar* title);
static void disable_viewglob(void);


/* --- Globals --- */
struct options opts;

/* Almost everything revolves around this. */
struct user_shell u;

/* This controls whether or not viewglob should actively do stuff.
   If the display is closed, viewglob will disable itself and try
   to just be a regular shell. */
gboolean viewglob_enabled;

gint main(int argc, char** argv) {

	gboolean ok = TRUE;

	/* Initialize program options. */
	opts.shell_type = ST_BASH;
	opts.executable = NULL;
	opts.init_loc = opts.expand_command = NULL;
	opts.smart_insert = TRUE;

	set_program_name(argv[0]);
	viewglob_enabled = TRUE;

	/* Initialize the shell and display structs. */
	u.s.pid = -1;
	u.s.fd = -1;
	args_init(&(u.s.a));

#if DEBUG_ON
	df = fopen("/tmp/out1.txt", "w");
#endif

	/* This fills in the opts struct. */
	parse_args(argc, argv);
	u.s.name = opts.executable;

	/* Get ready for the user shell child fork. */
	switch (opts.shell_type) {
		case ST_BASH:
			/* Bash is simple. */
			args_add(&(u.s.a), "--init-file");
			args_add(&(u.s.a), opts.init_loc);
			/* In my FreeBSD installation, unless bash is executed explicitly as
			   interactive, it causes issues when exiting the program.  Adding this flag
			   doesn't hurt, so why not. */
			args_add(&(u.s.a), "-i");
			break;

		case ST_ZSH: ; /* <-- Semicolon required for variable declaration */
			/* Zsh requires the init file be named ".zshrc", and its location determined
			   by the ZDOTDIR environment variable. */
			gchar* zdotdir = g_strconcat("ZDOTDIR=", opts.init_loc, NULL);
			if (putenv(zdotdir) != 0) {
				viewglob_error("Could not modify the environment");
				ok = FALSE;
				goto done;
			}
			g_free(zdotdir);
			break;
		default:
			viewglob_error("Unknown shell mode");
			ok = FALSE;
			goto done;
	}

	/* Setup a shell for the user. */
	if ( !pty_child_fork(&(u.s), NEW_PTY_FD, NEW_PTY_FD, NEW_PTY_FD) ) {
		viewglob_error("Could not create user shell");
		ok = FALSE;
		goto done;
	}

	if ( !handle_signals() ) {
		viewglob_error("Could not set up signal handlers");
		ok = FALSE;
		goto done;
	}

	/* Send the terminal size to the user's shell. */
	if ( !send_term_size(u.s.fd) ) {
		viewglob_error("Could not set terminal size");
		ok = FALSE;
		goto done;
	}

	if ( !tc_setraw() ) {
		viewglob_error("Could not set raw mode");
		ok = FALSE;
		goto done;
	}

	/* Enter main_loop. */
	if ( !main_loop() ) {
		viewglob_error("Problem during processing");
		ok = FALSE;
	}

	/* Done -- Turn off terminal raw mode. */
	if ( !tc_restore() )
		viewglob_warning("Could not restore terminal attributes");

#if DEBUG_ON
	if (fclose(df) != 0)
		viewglob_warning("Could not close debug file");
#endif

done:
	ok &= pty_child_terminate(&(u.s));
	printf("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void parse_args(gint argc, gchar** argv) {
	gboolean in_loop = TRUE;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "c:e:i:mvVx:")) {
			case -1:
				in_loop = FALSE;
				break;
			case '?':
				viewglob_fatal("Unknown argument");
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
			case 'e':
				/* Shell executable */
				g_free(opts.executable);
				opts.executable = g_new(gchar, strlen(optarg) + 1);
				strcpy(opts.executable, optarg);
				break;
			case 'i':
				/* Shell initialization command */
				g_free(opts.init_loc);
				opts.init_loc = g_new(gchar, strlen(optarg) + 1);
				strcpy(opts.init_loc, optarg);
				break;
			case 'm':
				opts.smart_insert = FALSE;
				break;
			case 'v':
			case 'V':
				report_version();
				exit(EXIT_SUCCESS);
				break;
			case 'x':
				/* Expand command (glob-expand) */
				g_free(opts.expand_command);
				opts.expand_command = g_new(gchar, strlen(optarg) + 1);
				strcpy(opts.expand_command, optarg);
				break;
		}
	}

	if (!opts.executable)
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
//static gboolean main_loop(struct display* disp) {
static gboolean main_loop(void) {

	Action a = A_NOP;
	gboolean ok = TRUE;
	gboolean in_loop = TRUE;

	/* Initialize working command line and sequence buffer. */
	if (!cmd_init()) {
		viewglob_error("Could not allocate space for command line");
		return FALSE;
	}

// FIXME move buffer initialization to here
// FIXME opts should be local to main()... shell_type could be a variable in u, which gets passe dot this function (and passed further from here)

	/* Initialize the sequences. */
	init_seqs(opts.shell_type);

	while (in_loop) {

		if ( !user_activity() ) {
			ok = FALSE;
			break;
		}

		/* FIXME
		   - A_SEND_PWD has no use. */
		for (a = action_queue(A_POP); in_loop && (a != A_DONE); a = action_queue(A_POP)) {
			switch (a) {
				case A_EXIT:
					DEBUG((df, "::d_exit::\n"));
					in_loop = FALSE;
					break;

				case A_DISABLE:
					DEBUG((df, "::disable::\n"));
					disable_viewglob();
					//if (display_running(disp))
					//	display_terminate(disp);
					break;

				case A_SEND_CMD:
					DEBUG((df, "::send cmd::\n"));
					//if (viewglob_enabled && display_running(disp))
					//	send_sane_cmd(disp);
					break;

				case A_SEND_PWD:
					/* Do nothing. */
					DEBUG((df, "::send pwd::\n"));
					break;

				case A_TOGGLE:
					/* Fork or terminate the display. */
					DEBUG((df, "::toggle::\n"));
					//if (viewglob_enabled) {
					//	if (display_running(disp))
					//		display_terminate(disp);
					//	else {
					//		display_fork(disp);
					//		action_queue(A_SEND_CMD);
					//	}
					//}
					break;

				case A_REFOCUS:
					DEBUG((df, "A_REFOCUS"));
					//if (viewglob_enabled && display_running(disp) && xDisp)
					//	refocus(xDisp, disp->xid, term_xid);
					break;

				case A_SEND_LOST:
				case A_SEND_UP:
				case A_SEND_DOWN:
				case A_SEND_PGUP:
				case A_SEND_PGDOWN:
					DEBUG((df, "::send order::\n"));
					//if (viewglob_enabled && display_running(disp))
					//	send_order(disp, a);
					//break;

				case A_DONE:
					DEBUG((df, "::d_done::\n"));
					break;

				default:
					viewglob_error("Received unexpected action");
					ok = in_loop = FALSE;
					break;
			}
		}
	}

	g_free(u.cmd.command);
	return ok;
}


/* Wait for input from the user's shell and the terminal.  If the
   terminal receives input, it's passed along pretty much untouched.  If
   the shell receives input, it's examined thoroughly with process_input. */
static gboolean user_activity(void) {

	static Buffer term_b =  { NULL, BUFSIZ, 0, 0, 0, PL_TERMINAL,  MS_NO_MATCH, NULL, FALSE, 0 };
	static Buffer shell_b = { NULL, BUFSIZ, 0, 0, 0, PL_EXECUTING, MS_NO_MATCH, NULL, FALSE, 0 };

	fd_set fdset_read;
	gint max_fd;
	gboolean data_converted;
	gboolean ok = TRUE;

	/* This only happens once. */
	if (!term_b.buf && !shell_b.buf) {
		term_b.buf = g_new(gchar, term_b.size);
		shell_b.buf = g_new(gchar, shell_b.size);
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
	max_fd = MAX(STDIN_FILENO, u.s.fd);

	/* Poll for output from the shell, terminal, and daemon. */
	if (!hardened_select(max_fd + 1, &fdset_read, NULL)) {
		viewglob_error("Problem while waiting for input");
		ok = FALSE;
		goto done;
	}

	/* If data has been written by the terminal... */
	if (FD_ISSET(STDIN_FILENO, &fdset_read)) {

		/* Read in from terminal. */
		if (!hardened_read(STDIN_FILENO, term_b.buf, term_b.size, &term_b.filled)) {
			if (errno == EIO)
				//FIXME?  why no ok = FALSE?
				goto done;
			else {
				viewglob_error("Read problem from terminal");
				ok = FALSE;
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
				ok = FALSE;
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
			ok = FALSE;
			goto done;
		}
	}

	done:
	return ok;
}

/*FIXME
  - add in and out fd variables to Buffer
  */

static gboolean write_to_shell(Buffer* b) {
	gboolean ok = TRUE;

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
		ok = FALSE;
		goto done;
	}

	done:
	return ok;
}


static gboolean process_input(Buffer* b) {

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
				cmd_overwrite_char(b->buf[b->pos], FALSE);
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

	return TRUE;
}



/* Look for characters which can break a line. */
static gboolean scan_for_newline(const Buffer* b) {
	size_t i;

	for (i = 0; i < b->filled; i++) {
		switch ( *(b->buf + i) ) {
			case '\n':     /* Newline. */
			case '\t':     /* Horizontal tab (for tab completion with multiple potential hits). */
			case '\003':   /* End of text -- Ctrl-C. */
			case '\004':   /* End of transmission -- Ctrl-D. */
			case '\015':   /* Carriage return -- this is the Enter key. */
			case '\017':   /* Shift in -- Ctrl-O (operate-and-get-next in bash readline). */
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
}


/* Convert the data in src_b into an escaped filename (maybe) and copy it into dest_b. */
//FIXME have the escaping be decided later (so we don't have to pass in pl)
static gboolean convert_to_filename(enum process_level pl, gchar* holdover, Buffer* src_b, Buffer* dest_b) {
	size_t i, j;
	gchar c;

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
	return TRUE;
}


/* Convert the data in src_b into a key and copy it into dest_b. */
static gboolean convert_to_key(Buffer* src_b, Buffer* dest_b) {
	*(dest_b->buf) = *(src_b->buf + strlen("key:"));
	dest_b->filled = 1;

	DEBUG((df, "(the key is: %c)\n", *(dest_b->buf)));
	return TRUE;
}


static gboolean convert_to_xid(Buffer* src_b, struct display* d) {
	gchar* xid_string;

	xid_string = src_b->buf + strlen("xid:");
	d->xid = strtoul(xid_string, NULL, 10);
	return TRUE;
}


static gboolean is_filename(Buffer* b) {
	gboolean result;

	if ( *(b->buf) != 'f' || b->filled < 7 )
		result = FALSE;
	else 
		result = !strncmp("file:", b->buf, 5);

	return result;
}


static gboolean is_key(Buffer* b) {
	gboolean result;

	if (*(b->buf) != 'k' || b->filled < 5)
		result = FALSE;
	else
		result = !strncmp("key:", b->buf, 4);

	return result;
}


static gboolean is_xid(Buffer* b) {
	gboolean result;

	if (*(b->buf) != 'x' || b->filled < 5)
		result = FALSE;
	else
		result = !strncmp("xid:", b->buf, 4);

	return result;
}

/* This function writes out stuff that looks like this:
   cmd:<sane_command>
   cd "<u.pwd>" && <glob-command> <sane_cmd> >> <glob fifo> ; cd / */
static void send_sane_cmd(struct display* d) {

	gchar* sane_cmd;
	gchar* sane_cmd_delimited;

	sane_cmd = make_sane_cmd(u.cmd.command, u.cmd.length);

	/* sane_cmd_delimited gets sent directly to the display. */
	sane_cmd_delimited = g_new(gchar, strlen("cmd:") + strlen(sane_cmd) + strlen("\n") + 1);
	(void)strcpy(sane_cmd_delimited, "cmd:");
	(void)strcat(sane_cmd_delimited, sane_cmd);
	(void)strcat(sane_cmd_delimited, "\n");

	DEBUG((df, "writing %s", sane_cmd_delimited));

	g_free(sane_cmd_delimited);
	g_free(sane_cmd);
}


static void send_order(struct display* d, Action a) {
	gchar* order;

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

//	if (!hardened_write(d->cmd_fifo_fd, order, strlen(order)))
//		disable_viewglob();
}


static void disable_viewglob(void) {
	fprintf(stderr, "(viewglob disabled)");
	viewglob_enabled = FALSE;
}


/* Set the title of the current terminal window (hopefully). */
static void set_term_title(gint fd, gchar* title) {

	gchar* full_title;

	/* These are escape sequences that most terminals use to delimit the title. */
	full_title = g_new(gchar, strlen("\033]0;") + strlen(title) + strlen("\007"));
	strcpy(full_title, "\033]0;");
	strcat(full_title, title);
	strcat(full_title, "\007");

	hardened_write(fd, full_title, strlen(full_title));
	g_free(full_title);
}


void sigwinch_handler(gint signum) {
	/* Don't need to do much here. */
	u.term_size_changed = TRUE;
	return;
}



/* Modified from code written by Marc J. Rockind and copyrighted as
   described in COPYING2. */
static gboolean handle_signals(void) {
	sigset_t set;
	struct sigaction act;

	if (sigfillset(&set) == -1)
		return FALSE;
	if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)
		return FALSE;
	memset(&act, 0, sizeof(act));
	if (sigfillset(&act.sa_mask) == -1)
		return FALSE;
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGINT, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGQUIT, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGPIPE, &act, NULL) == -1)
		return FALSE;
	act.sa_handler = sigterm_handler;
	if (sigaction(SIGTERM, &act, NULL) == -1)
		return FALSE;
	act.sa_handler = handler;
	if (sigaction(SIGBUS, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGFPE, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGILL, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGSEGV, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGSYS, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGXCPU, &act, NULL) == -1)
		return FALSE;
	if (sigaction(SIGXFSZ, &act, NULL) == -1)
		return FALSE;
	act.sa_handler = sigwinch_handler;
	if (sigaction(SIGWINCH, &act, NULL) == -1)
		return FALSE;
	if (sigemptyset(&set) == -1)
		return FALSE;
	if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)
		return FALSE;
	return TRUE;
}


static void sigterm_handler(gint signum) {
	(void) pty_child_terminate(&(u.s));
	(void) tc_restore();
	printf("[Exiting viewglob]\n");
	_exit(EXIT_FAILURE);
}


/* Modified from code written by Marc J. Rockind and copyrighted as
   described in COPYING2. */
static void handler(int signum) {
	int i;
	struct {
		int signum;
		char* msg;
	} sigmsg[] = {
		{ SIGTERM, "Termination signal" },
		{ SIGBUS, "Access to undefined portion of a memory object" },
		{ SIGFPE, "Erroneous arithmetic operation" },
		{ SIGILL, "Illegal instruction" },
		{ SIGSEGV, "Invalid memory reference" },
		{ SIGSYS, "Bad system call" },
		{ SIGXCPU, "CPU-time limit exceeded" },
		{ SIGXFSZ, "File-size limit exceeded" },
		{ 0, NULL }
	};

	/* clean_up(); */
	for (i = 0; sigmsg[i].signum > 0; i++) {
		if (sigmsg[i].signum == signum) {
			(void)write(STDERR_FILENO, sigmsg[i].msg, strlen_safe(sigmsg[i].msg));
			(void)write(STDERR_FILENO, "\n", 1);
			break;
		}
	}
	_exit(EXIT_FAILURE);
}


/* Modified from code written by Marc J. Rockind and copyrighted as
   described in COPYING2. */
static size_t strlen_safe(const char* string) {
	size_t n = 0;
	while (*string++ != '\0')
		n++;
	return n;
}

