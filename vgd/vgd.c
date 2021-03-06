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


#include <string.h>
#include <stdio.h>

/* For open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "common.h"
#include "hardened-io.h"
#include "param-io.h"
#include "child.h"
#include "x11-stuff.h"
#include "shell.h"
#include "tcp-listen.h"
#include "logging.h"
#include "syslogging.h"
#include "fgetopt.h"
#include "conf-to-args.h"

#define DEFAULT_VGEXPAND_OPTS  "-d"
#define CONF_FILE              ".viewglob/vgd.conf"

#define X_FAILURE        3
#define SOCKET_FAILURE   2
#define GENERAL_FAILURE  1

struct state {
	GList*                clients;
	struct vgseer_client* current;
	gboolean              current_is_active;

	struct child          display;
	Window                display_win;

	Display*              Xdisplay;
	gboolean              persistent;
	gboolean              daemon;
	GString*              vgexpand_opts;

	gchar*                port;
	gchar*                unix_sock_name;
	gint                  port_fd;
	gint                  unix_fd;
};


struct vgseer_client {
	/* Static properties (set once) */
	Window            win;
	gint              fd;

	/* Dynamic properties (change often) */
	enum shell_status status;
	GString*          cli;
	GString*          pwd;
	GString*          developing_mask;
	GString*          mask;
	GString*          expanded;
};


void state_init(struct state* s);
void vgseer_client_init(struct vgseer_client* v);
static void poll_loop(struct state* s);
static void die(struct state* s, gint result);
static gint setup_polling(struct state* s, fd_set* set);
static void new_client(struct state* s, gint accept_fd);
static void new_ping_client(gint ping_fd);
static void new_vgseer_client(struct state* s, gint client_fd);
static void process_client(struct state* s, struct vgseer_client* v);
static void process_display(struct state* s);
static void drop_client(struct state* s, struct vgseer_client* v);
static void context_switch(struct state* s, struct vgseer_client* v);
static void check_active_window(struct state* s);
static void update_display(struct state* s, struct vgseer_client* v,
		enum parameter param, gchar* value);
static gint unix_listen(struct state* s);
static int daemonize(void);
static void parse_args(gint argc, gchar** argv, struct state* s);
static void report_version(void);
static void usage(void);


gint main(gint argc, gchar** argv) {

	struct state s;

	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	g_log_set_handler(NULL,
			G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_MESSAGE |
			G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, logging, NULL);

	state_init(&s);

	/* Get program execution options. */
	gint conf_argc;
	gchar** conf_argv;
	if (conf_to_args(&conf_argc, &conf_argv, CONF_FILE)) {
		parse_args(conf_argc, conf_argv, &s);
		g_strfreev(conf_argv);
	}
	parse_args(argc, argv, &s);

	/* Get a connection to the X display. */
	if ( (s.Xdisplay = XOpenDisplay(NULL)) == NULL) {
		g_critical("Could not connect to X server");
		exit(X_FAILURE);
	}

	/* Setup listening sockets. */
	if ( (s.port_fd = tcp_listen(NULL, s.port)) == -1)
		exit(SOCKET_FAILURE);
	if ( (s.unix_fd = unix_listen(&s)) == -1)
		exit(SOCKET_FAILURE);

	(void) chdir("/");

	/* Turn into a daemon. */
	if (s.daemon)
		daemonize();

	poll_loop(&s);

	if (s.unix_sock_name)
		(void) unlink(s.unix_sock_name);
	return EXIT_SUCCESS;
}


static gint unix_listen(struct state* s) {

	gint listenfd;

	gchar* name;
	gchar* home;
	gchar* vgdir;

	struct stat vgdir_stat;

	struct sockaddr_un sun;

	listenfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (listenfd < 0) {
		g_critical("Could not create unix socket: %s", g_strerror(errno));
		return -1;
	}

	if ((home = getenv("HOME")) == NULL) {
		g_critical("User does not have a home!");
		return -1;
	}

	/* Create the ~/.viewglob/ directory. */
	vgdir = g_strconcat(home, "/.viewglob", NULL);
	if (stat(vgdir, &vgdir_stat) == -1) {
		if (mkdir(vgdir, 0700) == -1) {
			g_critical("Could not create ~/.viewglob directory: %s",
					g_strerror(errno));
			return -1;
		}
	}
	else if (!S_ISDIR(vgdir_stat.st_mode)) {
		g_critical("~/.viewglob exists but is not a directory");
		return -1;
	}

	name = g_strconcat(vgdir, "/.", s->port, NULL);
	g_free(vgdir);

	if (strlen(name) + 1 > sizeof(sun.sun_path)) {
		g_critical("Path is too long for unix socket");
		return -1;
	}

	(void) unlink(name);
	(void) memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	strcpy(sun.sun_path, name);

	if (bind(listenfd, (struct sockaddr*) &sun, sizeof(sun)) == -1) {
		g_critical("Could not bind socket: %s", g_strerror(errno));
		return -1;
	}

	if (listen(listenfd, SOMAXCONN) == -1) {
		g_critical("Could not listen on socket: %s", g_strerror(errno));
		return -1;
	}

	s->unix_sock_name = name;
	return listenfd;
}


static void parse_args(gint argc, gchar** argv, struct state* s) {
	g_return_if_fail(argv != NULL);
	g_return_if_fail(s != NULL);

	gboolean in_loop = TRUE;

	struct option long_options[] = {
		{ "port", 1, NULL, 'p' },
		{ "display", 1, NULL, 'd' },
		{ "persistent", 2, NULL, 'P' },
		{ "daemon", 2, NULL, 'D' },
		{ "sort-style", 1, NULL, 's' },
		{ "dir-order", 1, NULL, 'r' },
		{ "font-size-modifier", 1, NULL, 'z' },
		{ "black", 1, NULL, '1' },
		{ "red", 1, NULL, '2' },
		{ "green", 1, NULL, '3' },
		{ "yellow", 1, NULL, '4' },
		{ "blue", 1, NULL, '5' },
		{ "magenta", 1, NULL, '6' },
		{ "cyan", 1, NULL, '7' },
		{ "white", 1, NULL, '8' },
		{ "file-icons", 2, NULL, 'i' },
		{ "jump-resize", 2, NULL, 'j' },
		{ "help", 0, NULL, 'H' },
		{ "version", 0, NULL, 'V' },
		{ 0, 0, 0, 0},
	};

	optind = 0;
	while (in_loop) {
		switch (fgetopt_long(argc, argv,
					"p:d:D::P::s:r:z:i::j::HV", long_options, NULL)) {
			case -1:
				in_loop = FALSE;
				break;

			/* Port */
			case 'p':
				g_free(s->port);
				s->port = g_strdup(optarg);
				break;

			/* Display */
			case 'd':
				/* vgmini and vgclassic can be accepted without providing a
				   path. */
				g_free(s->display.exec_name);
				if (STREQ(optarg, "vgmini") || STREQ(optarg, "vgclassic")) {
					s->display.exec_name = g_strconcat(
							VG_LIB_DIR, "/", optarg, NULL);
				}
				else
					s->display.exec_name = g_strdup(optarg);
				break;

			/* Persistence */
			case 'P':
				if (!optarg || STREQ(optarg, "on"))
					s->persistent = TRUE;
				else if (STREQ(optarg, "off"))
					s->persistent = FALSE;
				break;

			/* Daemon */
			case 'D':
				if (!optarg || STREQ(optarg, "on"))
					s->daemon = TRUE;
				else if (STREQ(optarg, "off"))
					s->daemon = FALSE;
				break;

			/* Sort style */
			case 's':
				if (STREQ(optarg, "ls"))
					optarg = "-l";
				else if (STREQ(optarg, "windows") || STREQ(optarg, "win"))
					optarg = "-w";
				else
					optarg = "";
				s->vgexpand_opts = g_string_append(
						s->vgexpand_opts, optarg);
				break;

			/* Dir order */
			case 'r':
				if (STREQ(optarg, "descending"))
					optarg = "-d";
				else if (STREQ(optarg, "ascending"))
					optarg = "-a";
				else if (STREQ(optarg, "ascending-pwd-first"))
					optarg = "-p";
				else
					optarg = "";
				s->vgexpand_opts = g_string_append(
						s->vgexpand_opts, optarg);
				break;

			/* Font size modifier */
			case 'z':
				args_add(&s->display.args, "-z");
				args_add(&s->display.args, optarg);
				break;

			/* Colours */
			case '1':
				args_add(&s->display.args, "--black");
				args_add(&s->display.args, optarg);
				break;
			case '2':
				args_add(&s->display.args, "--red");
				args_add(&s->display.args, optarg);
				break;
			case '3':
				args_add(&s->display.args, "--green");
				args_add(&s->display.args, optarg);
				break;
			case '4':
				args_add(&s->display.args, "--yellow");
				args_add(&s->display.args, optarg);
				break;
			case '5':
				args_add(&s->display.args, "--blue");
				args_add(&s->display.args, optarg);
				break;
			case '6':
				args_add(&s->display.args, "--magenta");
				args_add(&s->display.args, optarg);
				break;
			case '7':
				args_add(&s->display.args, "--cyan");
				args_add(&s->display.args, optarg);
				break;
			case '8':
				args_add(&s->display.args, "--white");
				args_add(&s->display.args, optarg);
				break;

			/* File type icons */
			case 'i':
				args_add(&s->display.args, "--file-icons");
				if (optarg)
					args_add(&s->display.args, optarg);
				break;

			/* Enable or disable jump-resize */
			case 'j':
				args_add(&s->display.args, "--jump-resize");
				if (optarg)
					args_add(&s->display.args, optarg);
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
				exit(GENERAL_FAILURE);
				break;

			case '?':
			default:
				g_critical("Unknown option provided");
				exit(GENERAL_FAILURE);
				break;
		}
	}
}


static void usage(void) {
	g_print(
#		include "vgd-usage.h"
	);
	exit(EXIT_SUCCESS);
}


static void report_version(void) {
	printf("%s %s\n", g_get_prgname(), VERSION);
	printf("Released %s\n", VG_RELEASE_DATE);
	exit(EXIT_SUCCESS);
}


static void poll_loop(struct state* s) {
	g_return_if_fail(s != NULL);

	GList* iter;
	struct vgseer_client* v;

	fd_set rset;
	gint max_fd;
	gint nready;

	while (TRUE) {

		max_fd = setup_polling(s, &rset);

		/* Wait for input for a half second. */
		nready = hardened_select(max_fd + 1, &rset, 500);
		if (nready == -1) {
			g_critical("Problem while waiting for data: %s",
					g_strerror(errno));
			die(s, GENERAL_FAILURE);
		}
		else if (nready > 0) {

			if (FD_ISSET(s->port_fd, &rset)) {
				new_client(s, s->port_fd);
				nready--;
			}

			if (nready && FD_ISSET(s->unix_fd, &rset)) {
				new_client(s, s->unix_fd);
				nready--;
			}

			if (nready && child_running(&s->display) && 
					FD_ISSET(s->display.fd_in, &rset)) {
				process_display(s);
				nready--;
			}

			if (nready) {

				for (iter = s->clients; iter; iter = g_list_next(iter)) {
					v = iter->data;
					if (FD_ISSET(v->fd, &rset)) {
						process_client(s, v);
						/* It's unsafe to process more than one client, as
						   the list may have changed. */
						break;
					}
				}
			}
		}
		
		if (s->clients)
			check_active_window(s);
	}
}


static void check_active_window(struct state* s) {
	g_return_if_fail(s != NULL);

	Window new_active_win = get_active_window(s->Xdisplay);
	
	/* If the currently active window has changed and is one of
	   our vgseer client terminals, make a context switch. */
	if (new_active_win == s->current->win) {
		if (!s->current_is_active) {
			/* Reraise the display. */
			s->current_is_active = TRUE;
			update_display(s, s->current, P_WIN_ID,
					win_to_str(s->current->win));
		}
	}
	else {
		s->current_is_active = FALSE;
		GList* iter;
		struct vgseer_client* v;
		for (iter = s->clients; iter; iter = g_list_next(iter)) {
			v = iter->data;
			if (new_active_win == v->win) {
				s->current = v;
				s->current_is_active = TRUE;
				context_switch(s, v);
				break;
			}
		}
	}
}


static void context_switch(struct state* s, struct vgseer_client* v) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(v != NULL);

	if (!child_running(&s->display))
		return;

	gint fd = s->display.fd_out;

	/* Send a bunch of data to the display. */
	if (	!put_param(fd, P_STATUS, shell_status_to_string(v->status)) ||
			!put_param(fd, P_CMD, v->cli->str) ||
			!put_param(fd, P_DEVELOPING_MASK, v->developing_mask->str) ||
			!put_param(fd, P_MASK, v->mask->str) ||
			!put_param(fd, P_WIN_ID, win_to_str(v->win)) ||
			!put_param(fd, P_VGEXPAND_DATA, v->expanded->str)) {
		g_critical("(disp) Couldn't make context switch");
		// FIXME restart display, try again
	}
}


static void process_display(struct state* s) {
	g_return_if_fail(s != NULL);

	enum parameter param;
	gchar* value;

	/* Try to recover from display read errors instead of just dying. */
	if (!get_param(s->display.fd_in, &param, &value)) {
		(void) child_terminate(&s->display);
		if (!child_fork(&s->display)) {
			g_critical("The display had issues and I couldn't restart it");
			die(s, GENERAL_FAILURE);
		}
		else {
			s->display_win = 0;
			return;
		}
	}

	switch (param) {

		case P_FILE:
			/* Pass the file on to the client. */
			break;

		case P_KEY:
			/* Pass the key on to the client. */
			break;

		case P_WIN_ID:
			/* Store the new window id. */
			if ((s->display_win = strtoul(value, NULL, 10)) == ULONG_MAX) {
				g_warning("(disp) window ID is out of bounds: %s", value);
				s->display_win = 0;
			}
			param = P_NONE;
			break;

		case P_EOF:
			g_message("(disp) EOF from display");
			(void) child_terminate(&s->display);
			param = P_NONE;
			break;

		default:
			g_warning("(disp) Unexpected parameter: %d = %s", param, value);
			// TODO: restart display (wrap into function)
			param = P_NONE;
			break;
	}

	if (param != P_NONE) {
		/* Pass the message right along. */
		if (!put_param(s->current->fd, param, value)) {
			g_warning("Couldn't pass message to current client");
			drop_client(s, s->current);
		}
	}
}


static void process_client(struct state* s, struct vgseer_client* v) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(v != NULL);

	enum parameter param;
	gchar* value;

	if (!get_param(v->fd, &param, &value)) {
		drop_client(s, v);
		return;
	}

	enum shell_status new_status;

	switch (param) {

		case P_STATUS:
			if ( (new_status = string_to_shell_status(value)) == SS_ERROR) {
				g_warning("(%d) Invalid shell status from client: %s",
						v->fd, value);
				drop_client(s, v);
			}

			if (new_status != v->status) {
				v->status = new_status;
				update_display(s, v, param, value);
			}
			break;

		case P_PWD:
			// FIXME PWD required?
			v->pwd = g_string_assign(v->pwd, value);
			break;

		case P_CMD:
			v->cli = g_string_assign(v->cli, value);
			update_display(s, v, param, value);
			break;

		case P_MASK:
			/* Clear the developing mask, if any. */
			v->developing_mask = g_string_truncate(v->developing_mask, 0);
			v->mask = g_string_assign(v->mask, value);

			update_display(s, v, P_DEVELOPING_MASK, "");
			update_display(s, v, P_MASK, value);
			break;

		case P_DEVELOPING_MASK:
			v->developing_mask = g_string_assign(v->developing_mask, value);

			update_display(s, v, param, value);
			break;

		case P_ORDER:
			if (STREQ(value, "refocus")) {
				/* If vgd doesn't recognize that the current terminal has
				   changed, you can force it to take notice by refocusing. */
				if (s->current != v) {
					s->current = v;
					context_switch(s, v);
				}
				else
					update_display(s, v, param, value);
			}
			else if (STREQ(value, "toggle")) {
				if (child_running(&s->display))
					child_terminate(&s->display);
				else {
					if (!child_fork(&s->display)) {
						g_critical("Couldn't fork the display");
						die(s, GENERAL_FAILURE);
					}
					context_switch(s, s->current);
				}
			}
			else {
				/* The rest of the orders go to the display. */
				update_display(s, v, param, value);
			}
			break;

		case P_VGEXPAND_DATA:
			v->expanded = g_string_assign(v->expanded, value);
			update_display(s, v, param, value);
			break;

		case P_EOF:
			g_message("(%d) EOF from client", v->fd);
			drop_client(s, v);
			break;

		default:
			g_warning("(%d) Unexpected parameter: %d = %s", v->fd, param,
					value);
			drop_client(s, v);
			break;
	}
}


static void update_display(struct state* s, struct vgseer_client* v,
		enum parameter param, gchar* value) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(v != NULL);
	g_return_if_fail(value != NULL);

	if (s->current != v || !child_running(&s->display))
		return;

	if (!put_param(s->display.fd_out, param, value)) {
		g_critical("Couldn't send parameter to display");
		//FIXME restart display
	}
}


/* Disconnect the client and free its resources. */
static void drop_client(struct state* s, struct vgseer_client* v) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(v != NULL);

	s->clients = g_list_remove(s->clients, v);

	(void) close(v->fd);
	g_message("(%d) Dropped client", v->fd);
	g_string_free(v->cli, TRUE);
	g_string_free(v->pwd, TRUE);
	g_string_free(v->developing_mask, TRUE);
	g_string_free(v->mask, TRUE);
	g_string_free(v->expanded, TRUE);
	g_free(v);

	/* Kill the display if all the clients are gone. */
	if (child_running(&s->display) && !s->clients) {
		(void) child_terminate(&s->display);
		g_message("(disp) Killed display");
	}

	/* If this was the current client, switch to another one. */
	if (s->current == v) {
		if (s->clients) {
			s->current = s->clients->data;
			context_switch(s, s->current);
		}
		else
			s->current = NULL;
	}

	/* If we're not running in persistent mode, and this was the last client,
	   exit. */
	if (!s->persistent && !s->clients)
		die(s, EXIT_SUCCESS);
}


/* Accept a new client. */
static void new_client(struct state* s, gint accept_fd) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(accept_fd >= 0);

	gint new_fd;
	struct sockaddr_in sa;
	socklen_t sa_len;

	enum parameter param;
	gchar* value;

	/* Accept the new client. */
	again:
	sa_len = sizeof(sa);
	if ( (new_fd = accept(accept_fd, (struct sockaddr*) &sa,
					&sa_len)) == -1) {
		if (errno == EINTR)
			goto again;
		else {
			g_warning("Error while accepting new client: %s",
					g_strerror(errno));
			return;
		}
	}

	g_message("(%d) New client accepted", new_fd);

	// TODO add time limits to get_param
	/* Receive client's purpose. */
	if (get_param(new_fd, &param, &value) && param == P_PURPOSE) {
		if (STREQ(value, "vgping"))
			new_ping_client(new_fd);
		else if (STREQ(value, "vgseer"))
			new_vgseer_client(s, new_fd);
		else {
			g_warning("(%d) Unexpected purpose: \"%s\"", new_fd, value);
			(void) close(new_fd);
		}
	}
	else {
		g_warning("(%d) Did not receive purpose from client", new_fd);
		(void) close(new_fd);
	}
}


static void new_ping_client(gint ping_fd) {
	g_return_if_fail(ping_fd >= 0);

	g_message("(%d) Client is pinging", ping_fd);
	if (!put_param(ping_fd, P_STATUS, "yo"))
		g_warning("(%d) Couldn't ping back", ping_fd);

	(void) close(ping_fd);
}


static void new_vgseer_client(struct state* s, gint client_fd) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(client_fd >= 0);

	enum parameter param;
	gchar* value;

	struct vgseer_client* v;
	gchar* term_title = NULL;

	g_message("(%d) Client is a vgseer", client_fd);

	v = g_new(struct vgseer_client, 1);
	vgseer_client_init(v);

	/* Version */
	if (!get_param(client_fd, &param, &value) || param != P_VERSION)
		goto out_of_sync;
	// TODO: check_version()
	if (STREQ(VERSION, value)) {
		value = "OK";
		if (!put_param(client_fd, P_STATUS, value))
			goto reject;
	}
	else {
		/* Versions differ */
		gchar* warning = g_strconcat("vgd is v", VERSION,
				", vgseer is v", value, NULL);
		value = "WARNING";
		if (!put_param(client_fd, P_STATUS, value))
			goto reject;
		if (!put_param(client_fd, P_REASON, warning))
			goto reject;
		g_free(warning);
	}

	/* Title */
	if (!get_param(client_fd, &param, &value) || param != P_TERM_TITLE)
		goto out_of_sync;
	term_title = g_strdup(value);
	
	/* Tell vgseer client to set a title on its terminal. */
	if (!put_param(client_fd, P_ORDER, "set-title"))
		goto reject;
	if (!get_param(client_fd, &param, &value) || param != P_STATUS ||
			!STREQ(value, "title-set"))
		goto out_of_sync;

	/* Find the client's terminal window. */
	if ( (v->win = get_xid_from_title(s->Xdisplay, term_title)) == 0)
		g_warning("(%d) Couldn't locate client's window", client_fd);

	/* Finally, send over the vgexpand options. */
	if (!put_param(client_fd, P_VGEXPAND_OPTS, s->vgexpand_opts->str))
		goto reject;

	/* We've got a new client. */
	v->fd = client_fd;

	/* This is our only client, so it's current by default. */
	if (!s->clients)
		s->current = v;

	s->clients = g_list_prepend(s->clients, v);

	/* Startup the display if it's not around. */
	if (!child_running(&s->display)) {
		if (!child_fork(&s->display)) {
			g_critical("Couldn't fork the display");
			die(s, GENERAL_FAILURE);
		}
		/* Send the window ID, just in case this is the current window.
		   Otherwise the window ID is only sent on a context switch. */
		update_display(s, v, P_WIN_ID, win_to_str(v->win));
	}

	return;

out_of_sync:
	g_warning("(%d) Client sent unexpected data", client_fd);
reject:
	g_warning("(%d) Client rejected", client_fd);
	g_free(term_title);
	g_free(v);
	(void) close(client_fd);
}


/* Set the fd_set for all clients, the display, and the listen fd. */
static gint setup_polling(struct state* s, fd_set* set) {
	g_return_val_if_fail(s != NULL, 0);
	g_return_val_if_fail(set != NULL, 0);

	gint max_fd;
	GList* iter;
	struct vgseer_client* v;

	/* Setup polling for the accept sockets. */
	FD_ZERO(set);
	FD_SET(s->port_fd, set);
	FD_SET(s->unix_fd, set);
	max_fd = MAX(s->port_fd, s->unix_fd);

	/* Setup polling for each vgseer client. */
	for (iter = s->clients; iter; iter = g_list_next(iter)) {
		v = iter->data;
		g_return_val_if_fail(v->fd >= 0, 0);
		FD_SET(v->fd, set);
		max_fd = MAX(max_fd, v->fd);
	}

	/* And poll the display. */
	if (child_running(&s->display)) {
		FD_SET(s->display.fd_in, set);
		max_fd = MAX(max_fd, s->display.fd_in);
	}

	return max_fd;
}


/* Tell vgseer clients to disable, kill the display, and exit. */
static void die(struct state* s, gint result) {

	GList* iter;
	struct vgseer_client* v;

	/* Drop all the clients. */
	for (iter = s->clients; iter; iter = s->clients) {
		v = iter->data;
		(void) put_param(v->fd, P_STATUS, "dead");
		(void) drop_client(s, v);
	}

	/* Kill display. */
	if (child_running(&s->display))
		(void) child_terminate(&s->display);

	if (s->unix_sock_name)
		(void) unlink(s->unix_sock_name);
	exit(result);
}


/* This function taken from UNIX Network Programming, Volume I 3rd Ed.
   by W. Richard Stevens, Bill Fenner, and Andrew M. Rudoff. */
static gboolean daemonize(void) {

	pid_t pid;

	if ((pid = fork()) < 0)
		return FALSE; 
	else if (pid)
		_exit(EXIT_SUCCESS);  /* Parent terminates. */

	/* Child 1 continues... */

	if (setsid() < 0)
		return FALSE;

//	if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
//		return FALSE;

	if ((pid = fork()) < 0)
		return FALSE;
	else if (pid)
		_exit(EXIT_SUCCESS);  /* Child 1 terminates. */

	/* Child 2 continues... */

	/* Redirect stdin, stdout, and stderr to /dev/null. */
	(void) close(STDIN_FILENO);
	(void) close(STDOUT_FILENO);
	(void) close(STDERR_FILENO);
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	/* Use syslog for warnings and errors now that we're a daemon. */
	g_log_set_handler(NULL,
			G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_MESSAGE |
			G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, syslogging, NULL);
	openlog_wrapped(g_get_prgname());

	return TRUE;
}


void state_init(struct state* s) {
	s->clients = NULL;
	s->persistent = FALSE;
	s->daemon = TRUE;

	child_init(&s->display);
	s->display.exec_name = g_strdup(VG_LIB_DIR "/vgmini");
	s->display_win = 0;

	s->vgexpand_opts = g_string_new(DEFAULT_VGEXPAND_OPTS);
	s->Xdisplay = NULL;
	s->current = NULL;
	s->current_is_active = FALSE;

	s->port = g_strdup("16108");
	s->port_fd = -1;

	s->unix_sock_name = NULL;
	s->unix_fd = -1;
}


void vgseer_client_init(struct vgseer_client* v) {

	v->win = 0;
	v->fd = -1;

	v->status = SS_LOST;
	v->cli = g_string_new(NULL);
	v->pwd = g_string_new(NULL);
	v->developing_mask = g_string_new(NULL);
	v->mask = g_string_new(NULL);
	v->expanded = g_string_new(NULL);
}

