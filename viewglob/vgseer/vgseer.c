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
#include "actions.h"
#include "connection.h"
#include "children.h"
#include "sequences.h"
#include "shell.h"
#include "hardened-io.h"
#include "param-io.h"

#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

/* Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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


/* Data structure for the user's shell. */
struct user_shell {
	struct cmdline cmd;
	struct pty_child proc;
	enum shell_type type;
};

/* Program argument options. */
struct options {
	enum shell_type shell;
	gchar* executable;
	gchar* init_loc;
	gchar* expand_params;
};


/* Signal stuff. */
static void     sigwinch_handler(gint signum);
static gboolean handle_signals(void);
static void     handler(gint signum);
static void     clean_fail(struct pty_child* new_lamb);
static gsize    strlen_safe(const gchar* string);

/* Program flow. */
static gboolean fork_shell(struct user_shell* u, gchar* init_loc);
static void     main_loop(struct user_shell* u, gint vgd_fd);
static void     io_activity(Connection** bufs, struct user_shell* u);
static gboolean scan_for_newline(const Connection* b);
static void     process_input(Connection* b, struct user_shell* u);

/* Communication with vgd. */
static gint connect_to_vgd(gchar* server, gint port, struct user_shell* u,
		gchar* expand_opts);
static void send_order(struct display* d, Action a);
static gboolean set_term_title(gint fd, gchar* title);

static void parse_args(gint argc, gchar** argv, struct options* opts);
static void report_version(void);

static gboolean send_term_size(gint shell_fd);
static void     disable_viewglob(void);


/* This controls whether or not vgseer should actively do stuff. */
gboolean vgseer_enabled = TRUE;

/* Set whenever SIGWINCH is received. */
gboolean term_size_changed = FALSE;


gint main(gint argc, gchar** argv) {

	/* Program options. */
	struct options opts = { ST_BASH, NULL, NULL, NULL };

	/* Almost everything revolves around this. */
	struct user_shell u;

	gint   vgd_fd;
	
	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	/* Initialize the shell and display structs. */
	u.proc.pid = -1;
	u.proc.fd = -1;
	args_init(&(u.proc.a));

#if DEBUG_ON
	df = fopen("/tmp/out1.txt", "w");
#endif

	/* Fill in the opts struct. */
	parse_args(argc, argv, &opts);
	u.proc.exec_name = opts.executable;
	u.type = opts.shell;

	/* Create the shell. */
	if (!fork_shell(&u, opts.init_loc))
		clean_fail(NULL);

	/* If we terminate unexpectedly, we must kill this child shell too. */
	clean_fail(&u.proc);

	/* Connect to vgd and negotiate setup. */
	vgd_fd = connect_to_vgd("127.0.0.1", 16108, &u, opts.expand_params);
	if (vgd_fd == -1)
		clean_fail(NULL);

	/* Setup signal handlers. */
	if (!handle_signals()) {
		g_critical("Could not set up signal handlers");
		clean_fail(NULL);
	}

	/* Send the terminal size to the user's shell. */
	if (!send_term_size(u.proc.fd)) {
		g_critical("Could not set terminal size");
		clean_fail(NULL);
	}

	if (!tc_setraw()) {
		g_critical("Could not set raw terminal mode: %s", g_strerror(errno));
		clean_fail(NULL);
	}

	/* Enter main_loop. */
	main_loop(&u, vgd_fd);

	/* Done -- Turn off terminal raw mode. */
	if (!tc_restore()) {
		g_warning("Could not restore terminal attributes: %s",
				g_strerror(errno));
	}

	gboolean ok = pty_child_terminate(&u.proc);
	g_print("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static gint connect_to_vgd(gchar* server, gint port, struct user_shell* u,
		gchar* expand_opts) {
	struct sockaddr_in sa;
	gint fd;
	gchar string[100];

	/* Convert the pid into a string. */
	if (snprintf(string, sizeof(string), "%ld", (long) getpid()) <= 0) {
		g_critical("Couldn't convert the pid to a string");
		return -1;
	}

	/* Setup the socket. */
	(void) memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	if (inet_aton(server, &sa.sin_addr) == 0) {
		g_critical("\"%s\" is an invalid address", server);
		return -1;
	}
	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		g_critical("Could not create socket: %s", g_strerror(errno));
		return -1;
	}

	/* Attempt to connect to vgd. */
	while (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
		if (errno == EINTR)
			continue;
		else {
			g_critical("Could not connect to vgd: %s", g_strerror(errno));
			return -1;
		}
	}

	/* Send over information. */
	enum parameter param;
	gchar* value = NULL;
	if (!put_param(fd, P_PURPOSE, "vgseer"))
		goto fail;
	if (!put_param(fd, P_LOCALITY, "local"))
		goto fail;
	if (!put_param(fd, P_SHELL, shell_type_to_string(u->type)))
		goto fail;
	if (!put_param(fd, P_PROC_ID, string))
		goto fail;
	if (!put_param(fd, P_VGEXPAND_OPTS, expand_opts))
		goto fail;

	/* Wait for acknowledgement. */
	if (!get_param(fd, &param, &value) || param != P_ORDER ||
			!STREQ(value, "set-title"))
		goto fail;

	/* Set the terminal title. */
	if (snprintf(string, sizeof(string), "vgseer%ld", (long) getpid()) <= 0) {
		g_critical("Couldn't convert the pid to a string");
		goto fail;
	}
	if (!set_term_title(STDOUT_FILENO, string)) {
		g_critical("Couldn't set the term title");
		goto fail;
	}

	/* Alert vgd, get acknowledgement, set title as something better. */
	if (!put_param(fd, P_STATUS, "title-set"))
		goto fail;
	if (!get_param(fd, &param, &value) || param != P_ORDER ||
			!STREQ(value, "continue"))
		goto fail;
	if (!set_term_title(STDOUT_FILENO, "viewglob"))
		g_warning("Couldn't set the term title");

	return fd;

fail:
	g_critical("Could not complete negotiation with vgd");
	(void) close(fd);
	return -1;
}


/* Parse program arguments. */
static void parse_args(gint argc, gchar** argv, struct options* opts) {
	gboolean in_loop = TRUE;

	opterr = 0;
	while (in_loop) {
		switch (getopt(argc, argv, "c:e:i:vVx:")) {
			case -1:
				in_loop = FALSE;
				break;
			case '?':
				g_critical("Unknown option specified");
				clean_fail(NULL);
				break;
			case 'c':
				/* Set the shell mode. */
				if (strcmp(optarg, "bash") == 0)
					opts->shell = ST_BASH;
				else if (strcmp(optarg, "zsh") == 0)
					opts->shell = ST_ZSH;
				else {
					g_critical("Unknown shell mode \"%s\"", optarg);
					clean_fail(NULL);
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
			case 'v':
			case 'V':
				report_version();
				exit(EXIT_SUCCESS);
				break;
			case 'x':
				/* Vgexpand parameters */
				g_free(opts->expand_params);
				opts->expand_params = g_strdup(optarg);
				break;
		}
	}

	if (!opts->executable) {
		g_critical("No shell executable specified");
		clean_fail(NULL);
	}
	else if (!opts->init_loc) {
		g_critical("No shell initialization command specified");
		clean_fail(NULL);
	}
	else if (!opts->expand_params) {
		g_critical("No shell expansion parameters specified");
		clean_fail(NULL);
	}
}


static void report_version(void) {
	printf("seer %s\n", VERSION);
	printf("Released %s\n", VG_RELEASE_DATE);
	return;
}


/* Main program loop. */
static void main_loop(struct user_shell* u, gint vgd_fd) {

	g_return_if_fail(u != NULL);
	g_return_if_fail(vgd_fd >= 0);

	Action a;
	gboolean in_loop = TRUE;
	gchar common_buf[BUFSIZ];

	/* Terminal reads from stdin and writes to the shell. */
	Connection term_connection;
	connection_init(&term_connection, CT_TERMINAL, STDIN_FILENO, u->proc.fd,
			common_buf, sizeof(common_buf), PL_TERMINAL);

	/* Shell reads from shell and writes to stdout. */
	Connection shell_connection;
	connection_init(&shell_connection, CT_USER_SHELL, u->proc.fd, STDOUT_FILENO,
			common_buf, sizeof(common_buf), PL_EXECUTING);

	Connection* connections[3];
	connections[0] = &term_connection;
	connections[1] = &shell_connection;
	connections[2] = NULL;

	/* Initialize working command line and sequence buffer. */
	cmd_init(&u->cmd);

	/* Initialize the sequences. */
	init_seqs(u->type);

	while (in_loop) {

		io_activity(connections, u);

		if (term_size_changed && !send_term_size(u->proc.fd))
			clean_fail(NULL);

		DEBUG2((df, "cmd: %s\t\t(%s)\n", u->cmd.data->str, u->cmd.pwd));

		for (a = action_queue(A_DEQUEUE); in_loop && (a != A_DONE);
				a = action_queue(A_DEQUEUE)) {
			switch (a) {
				case A_EXIT:
					DEBUG((df, "::d_exit::\n"));
					in_loop = FALSE;
					break;

				case A_DISABLE:
					DEBUG((df, "::disable::\n"));
					disable_viewglob();
					break;

				case A_SEND_CMD:
					DEBUG((df, "::send cmd::\n"));
					if (vgseer_enabled) {
						if (!put_param(vgd_fd, P_CMD, u->cmd.data->str)) {
							g_critical("Couldn't send command line to vgd");
							clean_fail(NULL);
						}
					}
					break;

				case A_SEND_PWD:
					DEBUG((df, "::send pwd::\n"));
					if (vgseer_enabled) {
						if (!put_param(vgd_fd, P_PWD, u->cmd.pwd)) {
							g_critical("Couldn't send PWD to vgd");
							clean_fail(NULL);
						}
					}
					break;

				case A_TOGGLE:
					DEBUG((df, "::toggle::\n"));
					if (vgseer_enabled) {
						if (!put_param(vgd_fd, P_ORDER, "toggle")) {
							g_critical("Couldn't send toggle to vgd");
							clean_fail(NULL);
						}
					}
					break;

				case A_REFOCUS:
					DEBUG((df, "A_REFOCUS"));
					if (vgseer_enabled) {
						if (!put_param(vgd_fd, P_ORDER, "refocus")) {
							g_critical("Couldn't send refocus to vgd");
							clean_fail(NULL);
						}
					}
					break;

				case A_SEND_LOST:
				case A_SEND_UP:
				case A_SEND_DOWN:
				case A_SEND_PGUP:
				case A_SEND_PGDOWN:
					DEBUG((df, "::send order::\n"));
					//if (vgseer_enabled && display_running(disp))
					//	send_order(disp, a);
					break;

				case A_DONE:
					DEBUG((df, "::d_done::\n"));
					break;

				default:
					g_critical("Received unexpected action");
					clean_fail(NULL);
					break;
			}
		}
	}

	cmd_free(&u->cmd);
	connection_free(&shell_connection);
	connection_free(&term_connection);
}


/* Wait for input from the user's shell and the terminal.  If the
   terminal receives input, it's passed along pretty much untouched.  If
   the shell receives input, it's examined thoroughly with process_input. */
static void io_activity(Connection** connections, struct user_shell* u) {

	fd_set fdset_read;
	gboolean data_converted;
	gint i;
	gint max_fd = -1;
	Connection* cnt;
	gssize nread;

	/* Check to see if the user shell has been closed.  This is necessary
	   because for some reason if the shell opens an external program such
       as gvim and then exits, it will sit and wait for the external
	   program to end (even if it's not attached to the terminal).  I
	   don't know why.  So here we force an exit if the user's shell
	   closes, and the external programs can stay open. */
	//FIXME
	switch (waitpid(u->proc.pid, NULL, WNOHANG)) {
		case 0:
			break;
		case -1:
		default:
			u->proc.pid = -1;    /* So we don't try to kill it in cleanup. */
			action_queue(A_EXIT);
			return;
			/*break;*/
	}

	/* Iterate through the connections and setup the polling. */
	FD_ZERO(&fdset_read);
	for (i = 0; (cnt = connections[i]); i++) {
		FD_SET(cnt->fd_in, &fdset_read);
		max_fd = MAX(cnt->fd_in, max_fd);
	}

	/* Poll for output from the buffers. */
	if (hardened_select(max_fd + 1, &fdset_read, -1) == -1) {
		g_critical("Problem while waiting for input: %s", g_strerror(errno));
		clean_fail(NULL);
	}

	// FIXME if vgseer_disabled, should still eat bad segments (PS1).

	/* Iterate through the connections and process those which are ready. */
	for (i = 0; (cnt = connections[i]); i++) {
		if (FD_ISSET(cnt->fd_in, &fdset_read)) {

			/* Prepend holdover from last read on this connection. */
			prepend_holdover(cnt);

			/* Read in from the connection. */
			switch (hardened_read(cnt->fd_in, cnt->buf + cnt->filled, cnt->size,
						&nread)) {
				case IOR_OK:
					cnt->filled += nread;
					break;

				case IOR_ERROR:
					if (errno == EIO) {
						DEBUG((df, "~~1~~"));
						action_queue(A_EXIT);
					}
					else {
						g_critical("Read problem from %s: %s",
								connection_type_str(cnt->type), g_strerror(errno));
						clean_fail(NULL);
					}
					return;
					/*break;*/

				case IOR_EOF:
					DEBUG((df, "~~1~~"));
					action_queue(A_EXIT);
					return;
					/*break;*/
				default:
					g_return_if_reached();
					/*break;*/
			}

			/* Process the buffer. */
			if (vgseer_enabled)
				process_input(cnt, u);

			/* Look for a newline.  If one is found, then a match of a
			   newline/carriage return in the shell's output (which extends
			   past the end of the command- line) will be interpreted as
			   command execution.  Otherwise, they'll be interpreted as a
			   command wrap.  This is a heuristic (I can't see a guaranteed
			   way to relate shell input to output); in my testing, it works
			   very well for a person typing at a shell (i.e. 1 char length
			   buffers), but less well when text is pasted in (i.e. multichar
			   length buffers).  Ideas for improvement are welcome. */
			if (vgseer_enabled && cnt->pl == PL_TERMINAL)
				u->cmd.expect_newline = scan_for_newline(cnt);


			/* Write out the full buffer. */
			if (cnt->filled) { 
				switch (write_all(cnt->fd_out, cnt->buf + cnt->skip,
							cnt->filled - cnt->skip)) {
					case IOR_OK:
						break;
					case IOR_ERROR:
						g_critical("Problem writing for %s: %s",
								connection_type_str(cnt->type), g_strerror(errno));
						clean_fail(NULL);
						break;
					default:
						g_return_if_reached();
				}
			}
		}
	}
}


static void process_input(Connection* b, struct user_shell* u) {

	while (b->pos + b->seglen < b->filled) {

		DEBUG((df, "Pos at \'%c\' (%d of %d)\n", b->buf[b->pos], b->pos,
					b->filled - 1));

		cmd_del_trailing_CRs(&u->cmd);

		if (! IN_PROGRESS(b->status))
			enable_all_seqs(b->pl);

		#if DEBUG_ON
			gsize i;
			DEBUG((df, "process level: %d checking: --%c %d--\n(", b->pl,
						b->buf[b->pos + b->seglen], b->pos + b->seglen));
			for (i = 0; i < b->seglen + 1; i++)
				DEBUG((df, "%c", b->buf[b->pos + i]));
			DEBUG((df, ") |%d|\n", b->seglen + 1));
		#endif

		check_seqs(b, &u->cmd);

		if (b->status & MS_MATCH) {
			DEBUG((df, "*MS_MATCH*\n"));
			clear_seqs(b->pl);
		}
		else if (b->status & MS_IN_PROGRESS) {
			DEBUG((df, "*MS_IN_PROGRESS*\n"));
			b->seglen++;
		}
		else if (b->status & MS_NO_MATCH) {
			DEBUG((df, "*MS_NO_MATCH*\n"));
			if (b->pl == PL_AT_PROMPT) {
				cmd_overwrite_char(&u->cmd, b->buf[b->pos], FALSE);
				action_queue(A_SEND_CMD);
			}
			b->pos++;
			b->seglen = 0;
		}

		DEBUG((df, "<<<%s>>> {%d = %c}\n", u->cmd.data->str, u->cmd.pos,
					*(u->cmd.data->str + u->cmd.pos)));
		DEBUG((df, "length: %d\tpos: %d\tstrlen: %d\n\n", u->cmd.data->len,
					u->cmd.pos, strlen(u->cmd.data->str)));
	}

	/* We might be in the middle of matching a sequence, but we're at the end
	   of the buffer.  If it's PL_AT_PROMPT, we've gotta write what we've got
	   since it goes straight to the user.  Otherwise, it's safe to make a
	   holdover to attach to the next buffer read. */
	if (IN_PROGRESS(b->status)) {
		DEBUG((df, "seglen = %d, pos = %d, filled = %d\n", b->seglen,
					b->pos, b->filled));
		create_holdover(b, b->pl != PL_AT_PROMPT);
	}
}


/* Look for characters which can break a line. */
static gboolean scan_for_newline(const Connection* b) {
	gsize i;

	for (i = 0; i < b->filled; i++) {
		switch ( *(b->buf + i) ) {
			case '\n':     /* Newline. */
			case '\t':     /* Horizontal tab (for tab completion with
			                  multiple potential hits). */
			case '\003':   /* End of text -- Ctrl-C. */
			case '\004':   /* End of transmission -- Ctrl-D. */
			case '\015':   /* Carriage return -- this is the Enter key. */
			case '\017':   /* Shift in -- Ctrl-O (operate-and-get-next in
			                  bash readline). */
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
	gsize i, j;
	gchar c;

	i = strlen("file:");  /* Point to the first character after the ':'. */
	j = 0;

	if (pl == PL_AT_PROMPT) {
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
				if (pl == PL_AT_PROMPT)
					*(dest_b->buf + j++) = '\\';
			default:
				*(dest_b->buf + j++) = c;
				break;
		}
	}

	if (pl == PL_AT_PROMPT) {
		/* If there's no whitespace to the right, add a space at the end. */
		if (!cmd_whitespace_to_right(&u->cmd))
			*(dest_b->buf + j++) = ' ';
	}

	dest_b->filled = j;
	return TRUE;
}
#endif


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
	vgseer_enabled = FALSE;
}


/* Set the title of the current terminal window (hopefully). */
static gboolean set_term_title(gint fd, gchar* title) {

	gboolean ok = TRUE;
	gchar* full_title;

	/* These are escape sequences that most terminals use to delimit
	   the title. */
	full_title = g_strconcat("\033]0;", title, "\007", NULL);

	switch (write_all(fd, full_title, strlen(full_title))) {
		case IOR_OK:
			break;

		case IOR_ERROR:
			g_warning("Couldn't write term title");
			ok = FALSE;
			break;

		default:
			g_return_val_if_reached(FALSE);
			/*break;*/
	}

	g_free(full_title);
	return ok;
}


/* Send the window size to the given terminal. */
static gboolean send_term_size(gint shell_fd) {
	struct winsize size;

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


static gboolean fork_shell(struct user_shell* u, gchar* init_loc) {

	g_return_val_if_fail(u != NULL, FALSE);
	g_return_val_if_fail(init_loc != NULL, FALSE);

	gchar* zdotdir;
	gboolean ok = TRUE;

	/* Get ready for the user shell child fork. */
	switch (u->type) {
		case ST_BASH:
			/* Bash is simple. */
			args_add(&(u->proc.a), "--init-file");
			args_add(&(u->proc.a), init_loc);
			/* In my FreeBSD installation, unless bash is executed explicitly
			   as interactive, it causes issues when exiting the program.
			   Adding this flag doesn't hurt, so why not. */
			args_add(&(u->proc.a), "-i");
			break;

		case ST_ZSH:
			/* Zsh requires the init file be named ".zshrc", and its location
			   determined by the ZDOTDIR environment variable.  First check
			   to see if the user has already specified a ZDOTDIR. */
			zdotdir = getenv("ZDOTDIR");
			if (zdotdir) {
				/* Save it as VG_ZDOTDIR so we can specify our own. */
				zdotdir = g_strconcat("VG_ZDOTDIR=", zdotdir, NULL);
				if (putenv(zdotdir) != 0) {
					g_critical("Could not modify the environment: %s",
							g_strerror(errno));
					ok = FALSE;
					break;
				}
			}

			/* Use the location passed on the command line. */
			zdotdir = g_strconcat("ZDOTDIR=", init_loc, NULL);
			if (putenv(zdotdir) != 0) {
				g_critical("Could not modify the environment: %s",
						g_strerror(errno));
				ok = FALSE;
				break;
			}
			break;

		default:
			g_critical("Unknown shell mode");
			ok = FALSE;
			break;
	}

	/* Fork a shell for the user. */
	if (ok && !pty_child_fork(&(u->proc),
				NEW_PTY_FD, NEW_PTY_FD, NEW_PTY_FD)) {
		g_critical("Could not create user shell");
		ok = FALSE;
	}

	return ok;
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

	act.sa_handler = handler;
	if (sigaction(SIGTERM, &act, NULL) == -1)
		goto fail;
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


static void clean_fail(struct pty_child* new_lamb) {
	static struct pty_child* shell_stored;

	if (new_lamb)
		shell_stored = new_lamb;
	else {
		if (shell_stored)
			(void) pty_child_terminate(shell_stored);
		(void) tc_restore();
		printf("[Exiting Viewglob]\n");
		_exit(EXIT_FAILURE);
	}
}


/* Handler for the SIGWINCH signal. */
void sigwinch_handler(gint signum) {
	term_size_changed = TRUE;
}


static void handler(gint signum) {

	const gchar* string = g_strsignal(signum);

	if (signum != SIGTERM) {
		(void)write(STDERR_FILENO, string, strlen_safe(string));
		(void)write(STDERR_FILENO, "\n", 1);
	}

	clean_fail(NULL);
	_exit(EXIT_FAILURE);
}


static gsize strlen_safe(const gchar* string) {
	gsize n = 0;
	while (*string++ != '\0')
		n++;
	return n;
}

