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

#include "sanitize.h"
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

/* Structure for the state of the user's shell. */
struct user_state {
	struct cmdline cmd;
	struct child shell;
	struct child sandbox;
	enum shell_type type;
};

/* Structure for data relevant to communicating with vgd. */
struct vgd_stuff {
	int fd;
	Connection* shell_conn;
	Connection* term_conn;
	GString* expanded;
	gboolean vgexpand_called;
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
static void     clean_fail(struct child* new_lamb);
static gsize    strlen_safe(const gchar* string);

/* Setup. */
static gboolean fork_shell(struct child* child, enum shell_type type,
		gboolean sandbox, gchar* init_loc);
static gboolean setup_zsh(gchar* init_loc);
static gboolean putenv_wrapped(gchar* string);

/* Program flow. */
static void     main_loop(struct user_state* u, gint vgd_fd);
static void     io_activity(struct user_state* u, Connection* shell_conn,
		Connection* term_conn, struct vgd_stuff* vgd);
static gboolean action_loop(struct user_state* u, struct vgd_stuff* vgd);
static void     child_wait(struct user_state* u);
static void     process_shell(struct user_state* u, Connection* cnct);
static void     process_sandbox(struct user_state* u, struct vgd_stuff* vgd);
static void     process_terminal(struct user_state* u, Connection* cnct);
static void     process_vgd(struct user_state* u, struct vgd_stuff* vgd);
static gboolean scan_for_newline(const Connection* b);
static void     scan_sequences(Connection* b, struct user_state* u);

/* Communication with vgd. */
static gint     connect_to_vgd(gchar* server, gint port,
		struct user_state* u, gchar* expand_opts);
static gboolean set_term_title(gint fd, gchar* title);
static gchar*   escape_filename(gchar* name, struct user_state* u,
		enum process_level pl, gchar* holdover);
static void    call_vgexpand(struct user_state* u, struct vgd_stuff* vgd);
static void put_param_wrapped(gint fd, enum parameter param, gchar* value);

static void parse_args(gint argc, gchar** argv, struct options* opts);
static void report_version(void);

static void send_term_size(gint shell_fd);
static void disable_vgseer(struct vgd_stuff* vgd);

/* This controls whether or not vgseer should actively do stuff. */
gboolean vgseer_enabled = TRUE;

/* Set whenever SIGWINCH is received. */
gboolean term_size_changed = FALSE;


gint main(gint argc, gchar** argv) {

	/* Program options. */
	struct options opts = { ST_BASH, NULL, NULL, NULL };

	/* Almost everything revolves around this. */
	struct user_state u;

	gint vgd_fd;
	
	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	/* Initialize the shell and display structs. */
	child_init(&u.shell);
	child_init(&u.sandbox);

	/* Fill in the opts struct. */
	parse_args(argc, argv, &opts);
	u.shell.exec_name = u.sandbox.exec_name = opts.executable;
	u.type = opts.shell;

	/* Create the shells. */
	if (!fork_shell(&u.shell, u.type, FALSE, opts.init_loc))
		clean_fail(NULL);
	clean_fail(&u.shell);
	if (!fork_shell(&u.sandbox, u.type, TRUE, opts.init_loc))
		clean_fail(NULL);
	clean_fail(&u.sandbox);

	/* Connect to vgd and negotiate setup. */
	vgd_fd = connect_to_vgd("127.0.0.1", 16108, &u, opts.expand_params);
	if (vgd_fd == -1)
		clean_fail(NULL);

	/* Setup signal handlers. */
	if (!handle_signals()) {
		g_critical("Could not set up signal handlers");
		clean_fail(NULL);
	}

	send_term_size(u.shell.fd_out);
	if (!tc_setraw()) {
		g_critical("Could not set raw terminal mode: %s", g_strerror(errno));
		clean_fail(NULL);
	}

	/* Enter main_loop. */
	cmd_init(&u.cmd);
	init_seqs(u.type);
	main_loop(&u, vgd_fd);
	cmd_free(&u.cmd);

	/* Done -- Turn off terminal raw mode. */
	if (!tc_restore()) {
		g_warning("Could not restore terminal attributes: %s",
				g_strerror(errno));
	}

	gboolean ok;
	ok =  child_terminate(&u.shell);
	ok &= child_terminate(&u.sandbox);
	g_print("[Exiting viewglob]\n");
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static gint connect_to_vgd(gchar* server, gint port, struct user_state* u,
		gchar* expand_opts) {
	struct sockaddr_in sa;
	gint fd;
	gchar string[100];

	/* Convert the pid into a string. */
	if (snprintf(string, sizeof(string), "%ld", (glong) getpid()) <= 0) {
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
	if (!put_param(fd, P_PROC_ID, string))
		goto fail;

	/* Wait for acknowledgement. */
	if (!get_param(fd, &param, &value) || param != P_ORDER ||
			!STREQ(value, "set-title"))
		goto fail;

	/* Set the terminal title. */
	if (snprintf(string, sizeof(string), "vgseer%ld",
				(glong) getpid()) <= 0) {
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
	exit(EXIT_SUCCESS);
}


/* Main program loop. */
static void main_loop(struct user_state* u, gint vgd_fd) {

	g_return_if_fail(u != NULL);
	g_return_if_fail(vgd_fd >= 0);

	gchar common_buf[BUFSIZ];

	/* Terminal reads from stdin and writes to the shell. */
	Connection term_conn;
	connection_init(&term_conn, "terminal", STDIN_FILENO, u->shell.fd_out,
			common_buf, sizeof(common_buf), PL_TERMINAL);

	/* Reads from shell and writes to stdout. */
	Connection shell_conn;
	connection_init(&shell_conn, "shell", u->shell.fd_in, STDOUT_FILENO,
			common_buf, sizeof(common_buf), PL_EXECUTING);

	/* When dealing with vgd we have to access data from a bunch of
	   different places. */
	struct vgd_stuff vgd;
	vgd.fd = vgd_fd;
	vgd.term_conn = &term_conn;
	vgd.shell_conn = &shell_conn;
	vgd.expanded = g_string_sized_new(sizeof(common_buf));
	vgd.vgexpand_called = FALSE;

	gboolean in_loop = TRUE;
	while (in_loop) {

		io_activity(u, &shell_conn, &term_conn, &vgd);

		if (term_size_changed)
			send_term_size(u->shell.fd_out);

		child_wait(u);

		in_loop = action_loop(u, &vgd);
	}

	connection_free(&shell_conn);
	connection_free(&term_conn);
}


/* Act on all queued actions.  Most of these involve making calls to
   vgd. */
static gboolean action_loop(struct user_state* u, struct vgd_stuff* vgd) {

	Action a;
	enum parameter param = P_NONE;
	gchar* value = NULL;

	for (a = action_queue(A_DEQUEUE); a != A_DONE;
			a = action_queue(A_DEQUEUE)) {

		switch (a) {
			case A_EXIT:
				return FALSE;
				/*break;*/

			case A_DISABLE:
				disable_vgseer(vgd);
				break;

			case A_SEND_CMD:
				call_vgexpand(u, vgd);
				/* The parameters were already sent. */
				param = P_NONE;
				value = NULL;
				break;

			case A_SEND_PWD:
				param = P_PWD;
				value = u->cmd.pwd;
				break;

			case A_TOGGLE:
				param = P_ORDER;
				value = "toggle";
				break;

			case A_REFOCUS:
				param = P_ORDER;
				value = "refocus";
				break;

			case A_SEND_LOST:
				param = P_STATUS;
				value = "lost";
				break;

			case A_SEND_UP:
				param = P_ORDER;
				value = "up";
				break;

			case A_SEND_DOWN:
				param = P_ORDER;
				value = "down";
				break;

			case A_SEND_PGUP:
				param = P_ORDER;
				value = "pgup";
				break;

			case A_SEND_PGDOWN:
				param = P_ORDER;
				value = "pgdown";
				break;

			case A_NEW_MASK:
				param = P_DEVELOPING_MASK;
				value = u->cmd.mask->str;
				break;

			case A_DONE:
				break;

			default:
				g_return_val_if_reached(FALSE);
				break;
		}

		/* Write the parameter to vgd. */
		put_param_wrapped(vgd->fd, param, value);
		param = P_NONE;
		value = NULL;
	}

	return TRUE;
}


/* Check to see if the user shell has been closed.  This is necessary
   because for some reason if the shell opens an external program such as
   gvim and then exits, it will sit and wait for the external program to end
   (even if it's not attached to the terminal).  I don't know why.  So here
   we force an exit if the user's shell closes, and the external programs
   can stay open. */
static void child_wait(struct user_state* u) {

	switch (waitpid(u->shell.pid, NULL, WNOHANG)) {
		case 0:
			break;
		case -1:
		default:
			u->shell.pid = -1;    /* So we don't try to kill it in cleanup. */
			action_queue(A_EXIT);
			break;
	}
}


/* Wait for input and then do something about it. */
static void io_activity(struct user_state* u, Connection* shell_conn,
		Connection* term_conn, struct vgd_stuff* vgd) {

	g_return_if_fail(u != NULL);
	g_return_if_fail(shell_conn != NULL);
	g_return_if_fail(term_conn != NULL);
	g_return_if_fail(vgd != NULL);

	fd_set rset;
	gint max_fd = -1;

	/* Setup polling. */
	// TODO kill sandbox shell on disable.
	FD_ZERO(&rset);
	FD_SET(shell_conn->fd_in, &rset);
	FD_SET(term_conn->fd_in, &rset);
	max_fd = MAX(shell_conn->fd_in, term_conn->fd_in);
	if (vgseer_enabled) {
		FD_SET(u->sandbox.fd_in, &rset);
		FD_SET(vgd->fd, &rset);
		max_fd = MAX(MAX(max_fd, vgd->fd), u->sandbox.fd_in);
	}

	/* Wait for readable data. */
	//FIXME set a time limit to see what happens
	if (hardened_select(max_fd + 1, &rset, -1) == -1) {
		g_critical("Problem while waiting for input: %s", g_strerror(errno));
		clean_fail(NULL);
	}

	if (FD_ISSET(shell_conn->fd_in, &rset))
		process_shell(u, shell_conn);
	if (FD_ISSET(term_conn->fd_in, &rset))
		process_terminal(u, term_conn);
	if (FD_ISSET(vgd->fd, &rset))
		process_vgd(u, vgd);
	if (FD_ISSET(u->sandbox.fd_in, &rset))
		process_sandbox(u, vgd);
}


static void process_shell(struct user_state* u, Connection* cnct) {

	/* Prepend holdover from last shell read. */
	prepend_holdover(cnct);

	if (!connection_read(cnct))
		clean_fail(NULL);

	/* The scan commences whether vgseer_enabled is true or not, since
	   some viewglob sequences need to be removed or they interfere
	   with certain terminals. */
	scan_sequences(cnct, u);

	if (!connection_write(cnct))
		clean_fail(NULL);
}


static gssize sandbox_read(int fd, gchar* buf, gsize len) {
	gssize nread;

	switch (hardened_read(fd, buf, len, &nread)) {
		case IOR_OK:
			break;
		case IOR_ERROR:
			g_critical("Sandbox read error: %s", g_strerror(errno));
			clean_fail(NULL);
			/*break;*/
		case IOR_EOF:
			action_queue(A_EXIT);
			return -1;
			/*break;*/
		default:
			g_return_val_if_reached(-1);
	}

	return nread;
}


static void process_sandbox(struct user_state* u, struct vgd_stuff* vgd) {

	g_return_if_fail(u != NULL);
	g_return_if_fail(vgd != NULL);

	static gchar buf[BUFSIZ];
	gssize nread;

	if ((nread = sandbox_read(u->sandbox.fd_in, buf, sizeof(buf))) < 0)
		return;

//	FILE* temp;
//	temp = fopen("/tmp/sandbox.txt", "a");
//	fwrite(buf, 1, nread, temp);
//	fclose(temp);

	if (vgd->vgexpand_called) {
		gchar* start = NULL;
		gchar* end = NULL;
		gsize len;
		vgd->expanded = g_string_set_size(vgd->expanded, 0);

		/* Locate the start of the expand data */
		while (TRUE) {
			start = g_strstr_len(buf, nread, "\002");
			if (start)
				break;
			if ((nread = sandbox_read(u->sandbox.fd_in, buf, sizeof(buf))) < 0)
				return;
		}
		start++;
		len = buf + nread - start;

		/* Find the end of the data and copy everything along the way. */
		while (TRUE) {
			end = g_strstr_len(start, len, "\003");
			if (end)
				break;
			vgd->expanded = g_string_append_len(vgd->expanded, start, len);
			if ((nread = sandbox_read(u->sandbox.fd_in, buf, sizeof(buf))) < 0)
				return;
			start = buf;
			len = nread;
		}
		vgd->expanded = g_string_append_len(vgd->expanded, start,
				end - start);

		/* Now we have the whole output of vgexpand -- let's send it off. */
		put_param_wrapped(vgd->fd, P_VGEXPAND_DATA, vgd->expanded->str);

		vgd->vgexpand_called = FALSE;
	}
}


static void process_terminal(struct user_state* u, Connection* cnct) {

	/* Prepend holdover from last terminal read. */
	prepend_holdover(cnct);

	if (!connection_read(cnct))
		clean_fail(NULL);

	if (vgseer_enabled) {
		scan_sequences(cnct, u);

		/* Look for a newline.  If one is found, then a match of a
		   newline/carriage return in the shell's output (which extends
		   past the end of the command line) will be interpreted as
		   command execution.  Otherwise, they'll be interpreted as a
		   command wrap.  This is a heuristic (I can't see a guaranteed
		   way to relate shell input to output); in my testing, it works
		   very well for a person typing at a shell (i.e. 1 char length
		   buffers), but less well when text is pasted in (i.e. multichar
		   length buffers). */
		u->cmd.expect_newline = scan_for_newline(cnct);
	}

	if (!connection_write(cnct))
		clean_fail(NULL);
}


static void process_vgd(struct user_state* u, struct vgd_stuff* vgd) {

	g_return_if_fail(vgd != NULL);
	g_return_if_fail(u != NULL);

	enum parameter param;
	gchar* value;
	gsize len;

	if (!get_param(vgd->fd, &param, &value)) {
		g_critical("Out of sync with vgd");
		disable_vgseer(vgd);
		return;
	}

	switch (param) {

		case P_FILE:
			value = escape_filename(value, u, vgd->shell_conn->pl,
					vgd->term_conn->holdover);
			len = strlen(value);
			break;

		case P_KEY:
			value = g_strdup(value);
			if ( (len = strlen(value)) == 0) {
				/* Force a length of one for the case when a NUL character is
				   sent (Ctrl-<SPACE>). */
				len = 1;
			}
			break;

		case P_EOF:
			g_critical("vgd closed its connection");
		case P_STATUS:
			/* At this point we don't even need to check the value -- assumed
			   to be "dead" */
			disable_vgseer(vgd);
			return;
			/*break;*/

		default:
			g_print("(%d)", param);
			g_return_if_reached();
	}

	/* Now pretend this vgd data came from the terminal.  It's pretty safe to
	   assume the string will fit in the buffer, regardless of the size of
	   the holdover. */
	prepend_holdover(vgd->term_conn);
	memcpy(vgd->term_conn->buf + vgd->term_conn->filled, value, len);
	vgd->term_conn->filled += len;

	if (vgseer_enabled) {
		scan_sequences(vgd->term_conn, u);
		u->cmd.expect_newline = scan_for_newline(vgd->term_conn);
	}

	if (!connection_write(vgd->term_conn))
		clean_fail(NULL);

	g_free(value);
}


/* Scan through the newly read data for sequences. */
static void scan_sequences(Connection* b, struct user_state* u) {

	while (b->pos + b->seglen < b->filled) {

		cmd_del_trailing_CRs(&u->cmd);

		if (!IN_PROGRESS(b->status))
			enable_all_seqs(b->pl);

		check_seqs(b, &u->cmd);

		if (b->status & MS_MATCH)
			clear_seqs(b->pl);

		else if (b->status & MS_IN_PROGRESS)
			b->seglen++;

		else if (b->status & MS_NO_MATCH) {
			if (b->pl == PL_AT_PROMPT) {
				cmd_overwrite_char(&u->cmd, b->buf[b->pos], FALSE);
				action_queue(A_SEND_CMD);
			}
			b->pos++;
			b->seglen = 0;
		}
	}

	/* We might be in the middle of matching a sequence, but we're at the end
	   of the buffer.  If it's PL_AT_PROMPT, we've gotta write what we've got
	   since it goes straight to the user.  Otherwise, it's safe to make a
	   holdover to attach to the next buffer read. */
	if (IN_PROGRESS(b->status))
		create_holdover(b, b->pl != PL_AT_PROMPT);
}


/* Look for characters which can break a line. */
static gboolean scan_for_newline(const Connection* b) {
	gsize i;

	for (i = 0; i < b->filled; i++) {
//		g_print("%c (%d)\n", *(b->buf + i), *(b->buf + i));
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


/* Smart-escape (or not) the given filename.  This is a kludgy function --
   it requires peeking at the terminal's holdover, the shell's process
   level, and the command line's current state. */
static gchar* escape_filename(gchar* name, struct user_state* u,
		enum process_level pl, gchar* holdover) {
	gchar c;
	gchar* retval;

	GString* escaped = g_string_new(NULL);

	if (pl == PL_AT_PROMPT) {
		/* If there's no whitespace to the left, add a space at the
		   beginning. */
		if (!cmd_whitespace_to_left(&u->cmd, holdover))
			escaped = g_string_append_c(escaped, ' ');
	}

	/* Fill in the filename. */
	while ( (c = *name) != '\0') {
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
					escaped = g_string_append_c(escaped, '\\');
			default:
				escaped = g_string_append_c(escaped, c);
				break;
		}
		name++;
	}

	if (pl == PL_AT_PROMPT) {
		/* If there's no whitespace to the right, add a space at the end. */
		if (!cmd_whitespace_to_right(&u->cmd))
			escaped = g_string_append_c(escaped, ' ');
	}

	retval = escaped->str;
	g_string_free(escaped, FALSE);
	return retval;
}


/* Close connection with vgd and disable most functionality. */
static void disable_vgseer(struct vgd_stuff* vgd) {
	g_printerr("(viewglob disabled)");
	(void) close(vgd->fd);
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
static void send_term_size(gint shell_fd) {
	struct winsize size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
		g_critical("TIOCGWINSZ ioctl() call failed: %s", g_strerror(errno));
		clean_fail(NULL);
	}
	else if (ioctl(shell_fd, TIOCSWINSZ, &size) == -1) {
		g_critical("TIOCSWINSZ ioctl() call failed: %s", g_strerror(errno));
		clean_fail(NULL);
	}

	term_size_changed = FALSE;
}


/* Emits commands of the following form to the sandbox shell:
		cd "<pwd>" && vgexpand <opts> -m "<mask>" -- <cmd> ; cd / */
static void call_vgexpand(struct user_state* u, struct vgd_stuff* vgd) {

	gchar* cmd_sane;
	gchar* mask_sane;
	gchar* expand_command;

	cmd_sane = sanitize(u->cmd.data);

	mask_sane = sanitize(u->cmd.mask_final);
	/* A blank mask may as well be "*" */
	if (strlen(mask_sane) == 0) {
		g_free(mask_sane);
		mask_sane = g_strdup("*");
	}

	expand_command = g_strconcat("cd \"", u->cmd.pwd,
			"\" && vgexpand -m \"", mask_sane, "\" -- ", cmd_sane,
			" ; cd /\n", NULL);

	if (write_all(u->sandbox.fd_out, expand_command, strlen(expand_command))
			== IOR_ERROR)
		disable_vgseer(vgd);

	put_param_wrapped(vgd->fd, P_CMD, cmd_sane);
	/* TODO: only send mask if it's changed. */
	put_param_wrapped(vgd->fd, P_MASK, mask_sane);

	g_free(expand_command);
	g_free(mask_sane);
	g_free(cmd_sane);
	vgd->vgexpand_called = TRUE;
}


static gboolean fork_shell(struct child* child, enum shell_type type,
		gboolean sandbox, gchar* init_loc) {

	g_return_val_if_fail(child != NULL, FALSE);
	g_return_val_if_fail(init_loc != NULL, FALSE);

	/* Get ready for the child fork. */
	switch (type) {
		case ST_BASH:
			/* Bash is simple. */
			args_add(&child->args, "--init-file");
			args_add(&child->args, init_loc);
			/* In my FreeBSD installation, unless bash is executed explicitly
			   as interactive, it causes issues when exiting the program.
			   Adding this flag doesn't hurt, so why not. */
			args_add(&child->args, "-i");
			break;

		case ST_ZSH:
			if (!setup_zsh(init_loc))
				return FALSE;
			if (sandbox) {
				/* Disable the line editor.  This can be overridden, so we
				   also disable it in the rc file.  Disabling this twice
				   sometimes causes a pointless error message.  Annoying. */
				args_add(&child->args, "+Z");
			}
			/* Force interactive mode, otherwise zsh doesn't read its
			   initialization files in the sandbox shell. */
			args_add(&child->args, "-i");
			break;

		default:
			g_critical("Unknown shell mode");
			return FALSE;
			/*break;*/
	}

	if (sandbox) {
		if (!putenv_wrapped("VG_SANDBOX=yep") || !child_fork(child))
			return FALSE;
	}
	else {
		if (!putenv_wrapped("VG_SANDBOX=") ||
				!pty_child_fork(child, NEW_PTY_FD, NEW_PTY_FD, NEW_PTY_FD))
			return FALSE;
	}

	return TRUE;
}


/* Print error if it doesn't work out. */
static gboolean putenv_wrapped(gchar* string) {

	g_return_val_if_fail(string != NULL, FALSE);

	if (putenv(string) != 0) {
		g_critical("Could not modify the environment: %s",
				g_strerror(errno));
		return FALSE;
	}
	return TRUE;
}


/* Setup the environment for a favourable fork & exec into zsh. */
static gboolean setup_zsh(gchar* init_loc) {

	g_return_val_if_fail(init_loc != NULL, FALSE);
	g_return_val_if_fail(strlen(init_loc) > 0, FALSE);

	/* We must only do this once. */
	static gboolean visited = FALSE;

	if (!visited) {
		/* Zsh requires the init file be named ".zshrc", and its location
		   determined by the ZDOTDIR environment variable.  First check
		   to see if the user has already specified a ZDOTDIR. */
		gchar* zdotdir = getenv("ZDOTDIR");
		if (zdotdir) {
			/* Save it as VG_ZDOTDIR so we can specify our own. */
			zdotdir = g_strconcat("VG_ZDOTDIR=", zdotdir, NULL);
			if (!putenv_wrapped(zdotdir))
				return FALSE;
		}

		/* Use the location passed on the command line. */
		zdotdir = g_strconcat("ZDOTDIR=", init_loc, NULL);
		if (!putenv_wrapped(zdotdir))
			return FALSE;

		visited = TRUE;
	}

	return TRUE;
}


/* Fail if put_param() doesn't work out. */
static void put_param_wrapped(gint fd, enum parameter param, gchar* value) {
	g_return_if_fail(param >= 0 && param < P_COUNT);
	g_return_if_fail(fd >= 0);

	if (vgseer_enabled && param != P_NONE && value != NULL) {
		if (!put_param(fd, param, value)) {
			g_critical("Couldn't send parameter");
			clean_fail(NULL);
		}
	}
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


/* Exit with failure, but try to cleanup first. */
static void clean_fail(struct child* new_lamb) {
	static struct child* stored1 = NULL;
	static struct child* stored2 = NULL;

	if (new_lamb) {
		if (!stored1)
			stored1 = new_lamb;
		else if (!stored2)
			stored2 = new_lamb;
	}
	else {
		if (stored1)
			(void) child_terminate(stored1);
		if (stored2)
			(void) child_terminate(stored2);
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

