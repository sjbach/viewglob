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

#include "sanitize.h"
#include "tc_setraw.h"
#include "actions.h"
#include "connection.h"
#include "child.h"
#include "sequences.h"
#include "shell.h"
#include "hardened-io.h"
#include "param-io.h"
#include "socket-connect.h"
#include "logging.h"
#include "fgetopt.h"
#include "conf-to-args.h"

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

#include <sys/ioctl.h>

#ifndef GWINSZ_IN_SYS_IOCTL
#  include <termios.h>
#endif


#define CONF_FILE         ".viewglob/vgseer.conf"

/* Structure for the state of the user's shell. */
struct user_state {
	struct cmdline cmd;

	struct child shell;
	struct child sandbox;
	enum shell_type type;

	gchar* vgexpand_opts;
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
	gchar* host;
	gchar* port;
	gboolean use_unix_socket;
	gchar* executable;
	gchar* init_loc;
};


/* Signal stuff. */
static gboolean handle_signals(void);
static RETSIGTYPE sigwinch_handler(gint signum);
static RETSIGTYPE handler(gint signum);
static void     clean_fail(struct child* new_lamb);
static gsize    strlen_safe(const gchar* string);

/* Setup. */
static void parse_args(gint argc, gchar** argv, struct options* opts);
static void clean_opts(struct options* opts);
static gboolean fork_shell(struct child* child, enum shell_type type,
		gboolean sandbox, gchar* init_loc);
static gboolean setup_zsh(gchar* init_loc);
static gboolean putenv_wrapped(gchar* string);
static void usage(void);

/* Program flow. */
static void     main_loop(struct user_state* u, gint vgd_fd);
static void     io_activity(struct user_state* u, Connection* shell_conn,
		Connection* term_conn, struct vgd_stuff* vgd);
static gboolean action_loop(struct user_state* u, struct vgd_stuff* vgd);
static void     send_status(enum shell_status ss, struct vgd_stuff* vgd);
static void     child_wait(struct user_state* u);
static void     process_shell(struct user_state* u, Connection* cnct);
static void     process_sandbox(struct user_state* u, struct vgd_stuff* vgd);
static void     process_terminal(struct user_state* u, Connection* cnct);
static void     process_vgd(struct user_state* u, struct vgd_stuff* vgd);
static gboolean scan_for_newline(const Connection* b);
static void     scan_sequences(Connection* b, struct user_state* u);

/* Communication with vgd. */
static void put_param_verify(gint fd, enum parameter param, gchar* value);
static void get_param_verify(gint fd, enum parameter* param, gchar** value,
		enum parameter expected_param, gchar* expected_value);
static gint connect_to_vgd(gchar* server, gchar* port,
		gboolean use_unix_socket, struct user_state* u);
static gboolean set_term_title(gint fd, gchar* title);
static gchar*   escape_filename(gchar* name, struct user_state* u,
		enum process_level pl, gchar* holdover);
static void    call_vgexpand(struct user_state* u, struct vgd_stuff* vgd);
static void put_param_wrapped(gint fd, enum parameter param, gchar* value);

static void report_version(void);

static void send_term_size(gint shell_fd);
static void disable_vgseer(struct vgd_stuff* vgd);

/* This controls whether or not vgseer should actively do stuff. */
gboolean vgseer_enabled = TRUE;

/* Set whenever SIGWINCH is received. */
gboolean term_size_changed = FALSE;


gint main(gint argc, gchar** argv) {

	/* Program options. */
	struct options opts;

	/* Almost everything revolves around this. */
	struct user_state u;

	gint vgd_fd;
	
	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	/* Set up the log handler. */
	g_log_set_handler(NULL,
			G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_MESSAGE |
			G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, logging, NULL);

	/* Initialize the options. */
	(void) putenv_wrapped("VG_ASTERISK=yep");
	opts.shell = ST_BASH;
	opts.host = g_strdup("localhost");
	opts.port = g_strdup("16108");
	opts.use_unix_socket = TRUE;
	opts.executable = NULL;
	opts.init_loc = NULL;

	/* Fill in the opts struct. */
	gint conf_argc;
	gchar** conf_argv;
	if (conf_to_args(&conf_argc, &conf_argv, CONF_FILE)) {
		parse_args(conf_argc, conf_argv, &opts);
		g_strfreev(conf_argv);
	}
	parse_args(argc, argv, &opts);

	clean_opts(&opts);

	if (getenv("VG_VIEWGLOB_ACTIVE")) {
		g_message("Viewglob is already active in this shell.");
		clean_fail(NULL);
	}

	/* Initialize the shell and display structs. */
	child_init(&u.shell);
	child_init(&u.sandbox);

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
	vgd_fd = connect_to_vgd(opts.host, opts.port, opts.use_unix_socket, &u);
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

	(void) chdir("/");

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
	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


static gint connect_to_vgd(gchar* server, gchar* port,
		gboolean use_unix_socket, struct user_state* u) {
	gint fd;
	gchar term_title[100];

	/* Create the temporary title to use so that vgd can locate the
	   terminal. */
	if (snprintf(term_title, sizeof(term_title), "vgseer%ld",
				(glong) getpid()) <= 0) {
		g_critical("Couldn't convert the pid to a string");
		return -1;
	}

	/* Attempt to connect to vgd. */
	if (use_unix_socket)
		fd = unix_connect(port);
	else
		fd = tcp_connect(server, port);

	if (fd == -1)
		return -1;

	enum parameter param;
	gchar* value = NULL;

	/* Send over information. */
	put_param_verify(fd, P_PURPOSE, "vgseer");
	put_param_verify(fd, P_VERSION, VERSION);
	put_param_verify(fd, P_TERM_TITLE, term_title);

	/* Wait for acknowledgement. */
	get_param_verify(fd, &param, &value, P_STATUS, NULL);
	if (STREQ(value, "ERROR")) {
		/* Print reason for error and exit. */
		get_param_verify(fd, &param, &value, P_REASON, NULL);
		g_critical(value);
		clean_fail(NULL);
	}
	else if (STREQ(value, "WARNING")) {
		/* Print warning but continue. */
		get_param_verify(fd, &param, &value, P_REASON, NULL);
		g_warning(value);
	}
	else if (!STREQ(value, "OK")) {
		g_critical("Unknown value for P_STATUS: %s", value);
		clean_fail(NULL);
	}

	/* Wait for vgd to tell us to set our title. */
	get_param_verify(fd, &param, &value, P_ORDER, "set-title");

	/* Set the terminal title. */
	if (!set_term_title(STDOUT_FILENO, term_title)) {
		g_critical("Couldn't set the term title");
		clean_fail(NULL);
	}

	/* Alert vgd that we've set the title. */
	put_param_verify(fd, P_STATUS, "title-set");

	/* Next receive the vgexpand execution options. */
	get_param_verify(fd, &param, &value, P_VGEXPAND_OPTS, NULL);

	u->vgexpand_opts = g_strdup(value);

	/* It's safe to change the title to something else now. */
	if (!set_term_title(STDOUT_FILENO, "viewglob"))
		g_warning("Couldn't fix the term title");

	return fd;
}


/* Ensure that the parameter is transferred successfully, and fail if not. */
static void put_param_verify(gint fd, enum parameter param, gchar* value) {
	if (!put_param(fd, param, value)) {
		g_critical("Could not send parameter: %s = %s",
				param_to_string(param), value);
		clean_fail(NULL);
	}
}


/* Ensure the data we get is what we expect to receive, and fail if not. */
static void get_param_verify(gint fd, enum parameter* param, gchar** value,
		enum parameter expected_param, gchar* expected_value) {

	if (!get_param(fd, param, value)) {
		/* An error has already been reported. */
		g_critical("Expected: %s = \"%s\"",
				param_to_string(expected_param),
				value ? *value : "(unknown)");
		clean_fail(NULL);
	}

	/* Next verify we got the expected parameter. */
	if (*param != expected_param) {
		g_critical("Received %s (\"%s\") instead of %s",
				param_to_string(*param), *value,
				param_to_string(expected_param));
		clean_fail(NULL);
	}

	/* Now check the value, if necessary. */
	if (expected_value != NULL && !STREQ(expected_value, *value)) {
		g_critical("Expected %s = \"%s\", not %s = \"%s\".",
				param_to_string(expected_param), expected_value,
				param_to_string(*param), *value);
		clean_fail(NULL);
	}
}


/* Parse program arguments. */
static void parse_args(gint argc, gchar** argv, struct options* opts) {
	gboolean in_loop = TRUE;

	struct option long_options[] = {
		{ "host", 1, NULL, 'h' },
		{ "port", 1, NULL, 'p' },
		{ "shell-mode", 1, NULL, 'c' },
		{ "shell-star", 2, NULL, 't' },
		{ "executable", 1, NULL, 'e' },
		{ "unix-socket", 2, NULL, 'u' },
		{ "help", 0, NULL, 'H' },
		{ "version", 0, NULL, 'V' },
		{ 0, 0, 0, 0},
	};

	optind = 0;
	while (in_loop) {
		switch (fgetopt_long(argc, argv,
					"h:p:c:t::e:u::vVH", long_options, NULL)) {
			case -1:
				in_loop = FALSE;
				break;

			case 'h':
				g_free(opts->host);
				opts->host = g_strdup(optarg);
				opts->use_unix_socket = FALSE;
				break;

			case 'p':
				g_free(opts->port);
				opts->port = g_strdup(optarg);
				break;

			case 'c':
				opts->shell = string_to_shell_type(optarg);
				break;

			case 't':
				if (!optarg || STREQ(optarg, "on"))
					putenv_wrapped("VG_ASTERISK=yep");
				else if (STREQ(optarg, "off"))
					putenv_wrapped("VG_ASTERISK=");
				break;

			case 'e':
				g_free(opts->executable);
				opts->executable = g_strdup(optarg);
				break;

			case 'u':
				if (!optarg || STREQ(optarg, "on"))
					opts->use_unix_socket = TRUE;
				else if (STREQ(optarg, "off"))
					opts->use_unix_socket = FALSE;
				break;

			case 'H':
				usage();
				break;

			case 'v':
			case 'V':
				report_version();
				break;

			case ':':
				g_critical("Option missing argument");
				clean_fail(NULL);
				break;

			case '?':
			default:
				g_critical("Unknown option specified");
				clean_fail(NULL);
				break;
		}
	}
}


static void clean_opts(struct options* opts) {

	if (opts->shell == ST_BASH) {
		if (!opts->executable)
			opts->executable = g_strdup("bash");
		opts->init_loc = g_strconcat(
				VG_LIB_DIR, "/init-viewglob.bashrc", NULL);
	}
	else if (opts->shell == ST_ZSH) {
		if (!opts->executable)
			opts->executable = g_strdup("zsh");
		opts->init_loc = g_strdup(VG_LIB_DIR);
	}
	else {
		g_critical("Invalid shell mode specified");
		clean_fail(NULL);
	}
}


static void report_version(void) {
	printf("%s %s\n", g_get_prgname(), VERSION);
	printf("Released %s\n", VG_RELEASE_DATE);
	exit(EXIT_SUCCESS);
}


static void usage(void) {
	g_print(
#		include "vgseer-usage.h"
	);
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
		if (in_loop)
			send_status(shell_conn.ss, &vgd);
	}

	connection_free(&shell_conn);
	connection_free(&term_conn);
}


static void send_status(enum shell_status ss, struct vgd_stuff* vgd) {
	static enum shell_status ss_prev = -1;

	if (ss != ss_prev) {
		put_param_wrapped(vgd->fd, P_STATUS, shell_status_to_string(ss));
		ss_prev = ss;
	}
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
				vgd->shell_conn->ss = SS_LOST;
				param = P_NONE;
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

//	FILE* temp_file = fopen("/tmp/out1.txt", "a");
//	if (fwrite(cnct->buf, 1, cnct->filled, temp_file) != cnct->filled)
//		g_warning("bad");
//	fclose(temp_file);

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
	   holdover to attach to the next buffer read.  Special case: if the only
	   character in the segment thus far is a space, we can assume that the
	   user has simply typed a space (it's not the first character of a command
	   wrap). */
	if (IN_PROGRESS(b->status)) {
		if (b->pl == PL_AT_PROMPT
				&& b->seglen == 1 && b->buf[b->pos] == ' ') {
			disable_all_seqs(PL_AT_PROMPT);
			b->status = MS_NO_MATCH;
			cmd_overwrite_char(&u->cmd, b->buf[b->pos], FALSE);
			action_queue(A_SEND_CMD);
		}
		else 
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
			/*case '~':     Don't escape $HOME */
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

	static GString* mask_prev = NULL;
	if (!mask_prev)
		mask_prev = g_string_new(NULL);

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

	expand_command = g_strconcat("cd \'", u->cmd.pwd,
			"\' && vgexpand -m \'", mask_sane, "\' ", u->vgexpand_opts,
			" -- ", cmd_sane, " ; cd /\n", NULL);

	if (write_all(u->sandbox.fd_out, expand_command,
				strlen(expand_command)) == IOR_ERROR)
		disable_vgseer(vgd);

	vgd->vgexpand_called = TRUE;

	/* Send the command line. */
	put_param_wrapped(vgd->fd, P_CMD, cmd_sane);
	
	/* Only send the mask if it's changed. */
	/*if (!STREQ(mask_prev->str, mask_sane)) {*/
		put_param_wrapped(vgd->fd, P_MASK, mask_sane);
	/*	mask_prev = g_string_assign(mask_prev, mask_sane);*/
	/*}*/

	g_free(expand_command);
	g_free(mask_sane);
	g_free(cmd_sane);
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
		_exit(EXIT_FAILURE);
	}
}


/* Handler for the SIGWINCH signal. */
static RETSIGTYPE sigwinch_handler(gint signum) {
	term_size_changed = TRUE;
}


static RETSIGTYPE handler(gint signum) {

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

