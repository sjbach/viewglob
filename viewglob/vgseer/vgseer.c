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

#include "config.h"

#include "common.h"

#include "tc_setraw.h"
#include "hardened_io.h"
#include "actions.h"
#include "connection.h"
#include "vgseer.h"

#include <stdio.h>
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

#if HAVE_TERMIOS_H
# include <termios.h>
#endif

#if GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#endif

#if DEBUG_ON
FILE* df;
#endif

// FIXME also add viewglob_enabled here?
struct user_state_info {
	gchar* cli;
	gchar* pwd;
	gboolean cli_changed;
	gboolean pwd_changed;
};

struct options {
	enum shell_type shell;
	gchar* executable;
	gchar* init_loc;
};

/* --- Prototypes --- */

/* Signal stuff */
static void     sigwinch_handler(gint signum);
static void     sigterm_handler(gint signum);
static gboolean handle_signals(void);
static void     handler(gint signum);
static void     unexpected_termination_handler(struct pty_child* shell_to_kill);
static size_t   strlen_safe(const gchar* string);

/* Program flow */
static gboolean main_loop(struct user_shell* u);
static gboolean io_activity(Connection** bufs, struct user_shell* u);
static gboolean scan_for_newline(const Connection* b);
static gboolean process_input(Connection* b, struct user_shell* u);

static gboolean is_key(Connection* b);
static gboolean is_filename(Connection* b);
static gboolean is_xid(Connection* b);
static gboolean convert_to_filename(enum process_level pl, gchar* holdover,
		Connection* src_b, Connection* dest_b);
static gboolean convert_to_key(Connection* src_b, Connection* dest_b);
static gboolean convert_to_xid(Connection* src_b, struct display* d);

static void send_command(struct display* d);
static void send_order(struct display* d, Action a);

static gboolean parse_args(gint argc, gchar** argv, struct options* opts);
static void report_version(void);

static gboolean send_term_size(gint shell_fd);
static void set_term_title(gint fd, gchar* title);
static void disable_viewglob(void);


/* This controls whether or not viewglob should actively do stuff.
   If the display is closed, viewglob will disable itself and try
   to just be a regular shell. */
gboolean viewglob_enabled = TRUE;

/* Controls whether there are any situations where the filenames
   received from the daemon should not be escaped. */
gboolean smart_insert = TRUE;


gboolean term_size_changed = FALSE;

int main(int argc, char** argv) {

	/* Program options. */
	struct options opts = { ST_BASH, NULL, NULL };

	/* Almost everything revolves around this. */
	struct user_shell u;

	gboolean ok = TRUE;

	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	/* Initialize the shell and display structs. */
	u.pwd = NULL;
	u.expect_newline = FALSE;
	u.proc.pid = -1;
	u.proc.fd = -1;
	args_init(&(u.proc.a));

#if DEBUG_ON
	df = fopen("/tmp/out1.txt", "w");
#endif

	/* This fills in the opts struct. */
	if (!parse_args(argc, argv, &opts)) {
		ok = FALSE;
		goto done;
	}
	u.proc.name = opts.executable;
	u.type = opts.shell;

	/* Get ready for the user shell child fork. */
	switch (u.type) {
		case ST_BASH:
			/* Bash is simple. */
			args_add(&(u.proc.a), "--init-file");
			args_add(&(u.proc.a), opts.init_loc);
			/* In my FreeBSD installation, unless bash is executed explicitly
			   as interactive, it causes issues when exiting the program.
			   Adding this flag doesn't hurt, so why not. */
			args_add(&(u.proc.a), "-i");
			break;

		case ST_ZSH: ; /* <-- Semicolon required for variable declaration */
			/* Zsh requires the init file be named ".zshrc", and its location
			   determined by the ZDOTDIR environment variable. */
			gchar* zdotdir = g_strconcat("ZDOTDIR=", opts.init_loc, NULL);
			if (putenv(zdotdir) != 0) {
				g_critical("Could not modify the environment: %s",
						g_strerror(errno));
				ok = FALSE;
				goto done;
			}
			g_free(zdotdir);
			break;
		default:
			g_critical("Unknown shell mode");
			ok = FALSE;
			goto done;
	}

	/* Setup a shell for the user. */
	if ( !pty_child_fork(&(u.proc), NEW_PTY_FD, NEW_PTY_FD, NEW_PTY_FD) ) {
		g_critical("Could not create user shell");
		ok = FALSE;
		goto done;
	}

	/* If we terminate unexpectedly, we must kill this child shell too. */
	unexpected_termination_handler(&u.proc);

	/* Setup signal handlers. */
	if ( !handle_signals() ) {
		g_critical("Could not set up signal handlers");
		ok = FALSE;
		goto done;
	}

	/* Send the terminal size to the user's shell. */
	if ( !send_term_size(u.proc.fd) ) {
		g_critical("Could not set terminal size");
		ok = FALSE;
		goto done;
	}

	if ( !tc_setraw() ) {
		g_critical("Could not set raw terminal mode");
		ok = FALSE;
		goto done;
	}

	/* Enter main_loop. */
	if ( !main_loop(&u) ) {
		g_critical("Problem during processing");
		ok = FALSE;
	}

	/* Done -- Turn off terminal raw mode. */
	if ( !tc_restore() )
		g_warning("Could not restore terminal attributes");

#if DEBUG_ON
	if (fclose(df) != 0)
		g_warning("Could not close debug file");
#endif

done:
	ok &= pty_child_terminate(&u.proc);
	g_print("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


/* Parse program arguments. */
static gboolean parse_args(gint argc, gchar** argv, struct options* opts) {
	gboolean in_loop = TRUE;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "c:e:i:mvVx:")) {
			case -1:
				in_loop = FALSE;
				break;
			case '?':
				g_critical("Unknown option specified");
				return FALSE;
				break;
			case 'c':
				/* Set the shell mode. */
				if (strcmp(optarg, "bash") == 0)
					opts->shell = ST_BASH;
				else if (strcmp(optarg, "zsh") == 0)
					opts->shell = ST_ZSH;
				else {
					g_critical("Unknown shell mode \"%s\"", optarg);
					return FALSE;
				}
				break;
			case 'e':
				/* Shell executable */
				g_free(opts->executable);
				opts->executable = g_strdup(optarg);
				break;
			case 'i':
				/* Shell initialization command */
				g_free(opts->init_loc);
				opts->init_loc = g_strdup(optarg);
				break;
			case 'm':
				smart_insert = FALSE;
				break;
			case 'v':
			case 'V':
				report_version();
				exit(EXIT_SUCCESS);
				break;
			case 'x':
				/* Expand command (glob-expand) */
				//g_free(opts->expand_command);
				//opts->expand_command = g_strdup(optarg);
				//FIXME
				break;
		}
	}

	if (!opts->executable) {
		g_critical("No shell executable specified");
		return FALSE;
	}
	else if (!opts->init_loc) {
		g_critical("No shell initialization command specified");
		return FALSE;
	}
	//else if (!opts->expand_command) {
	//	g_critical("No shell expansion command specified");
	//	return FALSE;

	return TRUE;
}

static void report_version(void) {
	printf("seer %s\n", VERSION);
	printf("Released %s\n", VG_RELEASE_DATE);
	return;
}


/* Main program loop. */
static gboolean main_loop(struct user_shell* u) {

	Action a = A_NOP;
	gboolean ok = TRUE;
	gboolean in_loop = TRUE;

	/* Setup the buffers. */
	Connection term_connection =  {
		"terminal",
		-1, -1,
		NULL, BUFSIZ, 0, 0, 0,
		PL_TERMINAL, MS_NO_MATCH,
		NULL, FALSE, 0 };

	Connection shell_connection = {
		"shell",
		-1, -1,
		NULL, BUFSIZ, 0, 0, 0,
		PL_EXECUTING, MS_NO_MATCH,
		NULL, FALSE, 0 };

	term_connection.buf = g_malloc(term_connection.size);
	shell_connection.buf = g_malloc(shell_connection.size);

	/* Terminal reads from stdin and writes to the shell.
	   Shell reads from shell and writes to stdout. */
	term_connection.fd_in = STDIN_FILENO;
	term_connection.fd_out = u->proc.fd;
	shell_connection.fd_in = u->proc.fd;
	shell_connection.fd_out = STDOUT_FILENO;

	Connection* connections[3];
	connections[0] = &term_connection;
	connections[1] = &shell_connection;
	connections[2] = NULL;

	/* Initialize working command line and sequence buffer. */
	if (!cmd_init(&u->cmd)) {
		g_critical("Could not allocate space for command line");
		return FALSE;
	}

	/* Initialize the sequences. */
	init_seqs(u->type);

	while (in_loop) {

		if ( !io_activity(connections, u) ) {
			ok = FALSE;
			break;
		}

		if (term_size_changed && !send_term_size(u->proc.fd)) {
			ok = FALSE;
			break;
		}

		DEBUG2((df, "cmd: %s\n", u->cmd.data->str));


		/* FIXME
		   - A_SEND_PWD has no use.
		   - rename action_queue() to action_stack() */
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
					//	send_command(disp);
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
					g_critical("Received unexpected action");
					ok = in_loop = FALSE;
					break;
			}
		}
	}

	cmd_free(&u->cmd);
	return ok;
}


/* Wait for input from the user's shell and the terminal.  If the
   terminal receives input, it's passed along pretty much untouched.  If
   the shell receives input, it's examined thoroughly with process_input. */
static gboolean io_activity(Connection** connections, struct user_shell* u) {

	fd_set fdset_read;
	gboolean data_converted;
	gint i;
	gint max_fd = -1;
	gboolean ok = TRUE;
	Connection* cnt;

	/* Check to see if the user shell has been closed.  This is necessary
	   because for some reason if the shell opens an external program such
       as gvim and then exits, it will sit and wait for the external
	   program to end (even if it's not attached to the terminal).  I
	   don't know why.  So here we force an exit if the user's shell
	   closes, and the external programs can stay open. */
	switch (waitpid(u->proc.pid, NULL, WNOHANG)) {
		case 0:
			break;
		case -1:
		default:
			DEBUG((df,"[user shell dead]"));
			u->proc.pid = -1;    /* So we don't try to kill it in cleanup. */
			action_queue(A_EXIT);
			goto done;
			break;
	}

	/* Iterate through the connections and setup the polling. */
	FD_ZERO(&fdset_read);
	for (i = 0; (cnt = connections[i]); i++) {
		FD_SET(cnt->fd_in, &fdset_read);
		max_fd = MAX(cnt->fd_in, max_fd);
	}

	/* Poll for output from the buffers. */
	if (!hardened_select(max_fd + 1, &fdset_read, NULL)) {
		g_critical("Problem while waiting for input: %s", g_strerror(errno));
		ok = FALSE;
		goto done;
	}

	/* Iterate through the connections and process those which are ready. */
	for (i = 0; (cnt = connections[i]); i++) {
		if (FD_ISSET(cnt->fd_in, &fdset_read)) {

			/* Read in from the connection. */
			if (!hardened_read(cnt->fd_in, cnt->buf, cnt->size, &cnt->filled)) {
				if (errno == EIO) {
					DEBUG((df, "~~2~~"));
					action_queue(A_EXIT);
					break;
				}
				else {
					g_critical("Read problem from %s: %s", cnt->name, g_strerror(errno));
					ok = FALSE;
					break;
				}
			}
			else if (cnt->filled == 0) {
				DEBUG((df, "~~1~~"));
				action_queue(A_EXIT);
				break;
			}

			#if DEBUG_ON
				size_t x;
				DEBUG((df, "read %d bytes from the %s:\n===============\n", cnt->filled, cnt->name));
				for (x = 0; x < cnt->filled; x++)
					DEBUG((df, "%c", cnt->buf[x]));
				DEBUG((df, "\n===============\n"));
			#endif

			/* Process the buffer. */
			if (viewglob_enabled) {
				ok = process_input(cnt, u);
				if (!ok)
					break;
			}

			/* Look for a newline.  If one is found, then a match of a
			   newline/carriage return in the shell's output (which extends
			   past the end of the command- line) will be interpreted as
			   command execution.  Otherwise, they'll be interpreted as a
			   command wrap.  This is a heuristic (I can't see a guaranteed
			   way to relate shell input to output); in my testing, it works
			   very well for a person typing at a shell (i.e. 1 char length
			   buffers), but less well when text is pasted in (i.e. multichar
			   length buffers).  Ideas for improvement are welcome. */
			if (viewglob_enabled && cnt->pl == PL_TERMINAL)
				u->expect_newline = scan_for_newline(cnt);


			/* Write out the full buffer. */
			if (cnt->filled && !hardened_write(cnt->fd_out, cnt->buf + cnt->skip, cnt->filled - cnt->skip)) {
				g_critical("Problem writing for %s: %s", cnt->name, g_strerror(errno));
				ok = FALSE;
				break;
			}
		}
	}

	done:
	return ok;
}


static gboolean process_input(Connection* b, struct user_shell* u) {

	if (b->holdover)
		prepend_holdover(b);
	else {
		b->pos = 0;
		b->n = 1;
		b->skip = 0;
	}

	while (b->pos + (b->n - 1) < b->filled) {

		DEBUG((df, "Pos at \'%c\' (%d of %d)\n", b->buf[b->pos], b->pos, b->filled - 1));

		cmd_del_trailing_crets(&u->cmd);

		if (! IN_PROGRESS(b->status))
			enable_all_seqs(b->pl);

		#if DEBUG_ON
			size_t i;
			DEBUG((df, "process level: %d checking: --%c %d--\n(", b->pl, b->buf[b->pos + (b->n - 1)], b->pos + (b->n - 1)));
			for (i = 0; i < b->n; i++)
				DEBUG((df, "%c", b->buf[b->pos + i]));
			DEBUG((df, ") |%d|\n", b->n));
		#endif

		//FIXME: return error value here!
		check_seqs(b, u);

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
				cmd_overwrite_char(&u->cmd, b->buf[b->pos], FALSE);
				action_queue(A_SEND_CMD);
			}
			b->pos++;
			b->n = 1;
		}

		DEBUG((df, "<<<%s>>> {%d = %c}\n", u->cmd.data->str, u->cmd.pos, *(u->cmd.data->str + u->cmd.pos)));
		DEBUG((df, "length: %d\tpos: %d\tstrlen: %d\n\n", u->cmd.data->len, u->cmd.pos, strlen(u->cmd.data->str)));
	}

	if (IN_PROGRESS(b->status)) {
		DEBUG((df, "n = %d, pos = %d, filled = %d\n", b->n, b->pos, b->filled));
		create_holdover(b, b->pl != PL_AT_PROMPT);
	}

	return TRUE;
}


/* Look for characters which can break a line. */
static gboolean scan_for_newline(const Connection* b) {
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


/* Convert the data in src_b into an escaped filename (maybe) and copy it into
   dest_b. */
//FIXME have the escaping be decided later (so we don't have to pass in pl or u)
#if 0
static gboolean convert_to_filename(enum process_level pl, gchar* holdover, Connection* src_b, Connection* dest_b) {
	size_t i, j;
	gchar c;

	i = strlen("file:");  /* Point to the first character after the ':'. */
	j = 0;

	if (pl == PL_AT_PROMPT && smart_insert) {
		/* If there's no whitespace to the left, add a space at the
		   beginning. */
		if (!cmd_whitespace_to_left(&u->cmd, holdover))
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
				if (pl == PL_AT_PROMPT || !smart_insert)
					*(dest_b->buf + j++) = '\\';
			default:
				*(dest_b->buf + j++) = c;
				break;
		}
	}

	if (pl == PL_AT_PROMPT && smart_insert) {
		/* If there's no whitespace to the right, add a space at the end. */
		if (!cmd_whitespace_to_right(&u->cmd))
			*(dest_b->buf + j++) = ' ';
	}

	dest_b->filled = j;
	return TRUE;
}
#endif


/* Convert the data in src_b into a key and copy it into dest_b. */
static gboolean convert_to_key(Connection* src_b, Connection* dest_b) {
	*(dest_b->buf) = *(src_b->buf + strlen("key:"));
	dest_b->filled = 1;

	DEBUG((df, "(the key is: %c)\n", *(dest_b->buf)));
	return TRUE;
}


static gboolean convert_to_xid(Connection* src_b, struct display* d) {
	gchar* xid_string;

	xid_string = src_b->buf + strlen("xid:");
	d->xid = strtoul(xid_string, NULL, 10);
	return TRUE;
}


static gboolean is_filename(Connection* b) {
	gboolean result;

	if ( *(b->buf) != 'f' || b->filled < 7 )
		result = FALSE;
	else 
		result = !strncmp("file:", b->buf, 5);

	return result;
}


static gboolean is_key(Connection* b) {
	gboolean result;

	if (*(b->buf) != 'k' || b->filled < 5)
		result = FALSE;
	else
		result = !strncmp("key:", b->buf, 4);

	return result;
}


static gboolean is_xid(Connection* b) {
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
static void send_command(struct display* d) {

	//FIXME
	DEBUG((df, "writing %s", u.cmd.data->str));
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
			g_return_if_reached();
			return;
	}

//	if (!hardened_write(d->cmd_fifo_fd, order, strlen(order)))
//		disable_viewglob();
}


static void disable_viewglob(void) {
	g_printerr("(viewglob disabled)");
	viewglob_enabled = FALSE;
}


/* Set the title of the current terminal window (hopefully). */
static void set_term_title(gint fd, gchar* title) {

	gchar* full_title;

	/* These are escape sequences that most terminals use to delimit the title. */
	full_title = g_strconcat("\033]0;", title, "\007", NULL);

	hardened_write(fd, full_title, strlen(full_title));
	g_free(full_title);
}


/* Handler for the SIGWINCH signal. */
void sigwinch_handler(gint signum) {
	term_size_changed = TRUE;
}


/* Send the window size to the given terminal. */
static gboolean send_term_size(gint shell_fd) {
	struct winsize size;
	DEBUG((df, "in send_term_size\n"));
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
		g_critical("TIOCGWINSZ ioctl() call failed: %s", g_strerror(errno));
		return FALSE;
	}
	else if (ioctl(shell_fd, TIOCSWINSZ, &size) == -1) {
		g_critical("TIOCSWINSZ ioctl() call failed: %s", g_strerror(errno));
		return FALSE;
	}

	term_size_changed = FALSE;
	return TRUE;
}



/* Modified from code written by Marc J. Rockind and copyrighted as
   described in COPYING2. */
static gboolean handle_signals(void) {
	sigset_t set;
	struct sigaction act;

	if (sigfillset(&set) == -1)
		goto fail;
	if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)
		goto fail;
	memset(&act, 0, sizeof(act));
	if (sigfillset(&act.sa_mask) == -1)
		goto fail;

	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGINT, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGQUIT, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGPIPE, &act, NULL) == -1)
		goto fail;

	act.sa_handler = sigterm_handler;
	if (sigaction(SIGTERM, &act, NULL) == -1)
		goto fail;

	act.sa_handler = handler;
	if (sigaction(SIGBUS, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGFPE, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGILL, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGSEGV, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGSYS, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGXCPU, &act, NULL) == -1)
		goto fail;
	if (sigaction(SIGXFSZ, &act, NULL) == -1)
		goto fail;

	act.sa_handler = sigwinch_handler;
	if (sigaction(SIGWINCH, &act, NULL) == -1)
		goto fail;

	if (sigemptyset(&set) == -1)
		goto fail;
	if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)
		goto fail;

	return TRUE;

fail:
	g_critical("Could not handle signals: %s", g_strerror(errno));
	return FALSE;
}


static void sigterm_handler(gint signum) {
	unexpected_termination_handler(NULL);
}


static void unexpected_termination_handler(struct pty_child* shell_to_kill) {
	static struct pty_child* shell;

	if (shell_to_kill)
		shell = shell_to_kill;
	else if (shell) {
		(void) pty_child_terminate(shell);
		(void) tc_restore();
		printf("[Exiting Viewglob]\n");
		_exit(EXIT_FAILURE);
	}
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

