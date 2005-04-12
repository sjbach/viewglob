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

#define DEFAULT_VGEXPAND_OPTS "-d"

struct state {
	GList*                clients;
	struct vgseer_client* active;
	struct child          display;
	Window                display_win;

	Display*              Xdisplay;
	gboolean              persistent;
	GString*              vgexpand_opts;
	gint                  listen_fd;
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
gchar* win_to_str(Window win);
static void poll_loop(struct state* s);
static void die(struct state* s, gint result);
static gint setup_polling(struct state* s, fd_set* set);
static void new_client(struct state* s);
static void new_ping_client(gint ping_fd);
static void new_vgseer_client(struct state* s, gint client_fd);
static void process_client(struct state* s, struct vgseer_client* v);
static void process_display(struct state* s);
static void drop_client(struct state* s, struct vgseer_client* v);
static void context_switch(struct state* s, struct vgseer_client* v);
static void check_active_window(struct state* s);
static void update_display(struct state* s, struct vgseer_client* v,
		enum parameter param, gchar* value);
static int daemonize(void);


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
//	s.display.exec_name = VG_LIB_DIR "/vgclassic";
	s.display.exec_name = VG_LIB_DIR "/vgmini";

	/* Get a connection to the X display. */
	if ( (s.Xdisplay = XOpenDisplay(NULL)) == NULL) {
		g_critical("Could not connect to X server");
		exit(EXIT_FAILURE);
	}

	/* Setup listening socket. */
	if ( (s.listen_fd = tcp_listen("localhost", "16108")) == -1)
		exit(EXIT_FAILURE);

	/* Turn into a daemon. */
	daemonize();

	/* Use syslog for warnings and errors now that we're a daemon */
	g_log_set_handler(NULL,
			G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_MESSAGE |
			G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, syslogging, NULL);
	openlog_wrapped(g_get_prgname());

	poll_loop(&s);

	return EXIT_SUCCESS;
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
			die(s, EXIT_FAILURE);
		}
		else if (nready > 0) {

			if (FD_ISSET(s->listen_fd, &rset)) {
				new_client(s);
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
	if (new_active_win != s->active->win) {
		GList* iter;
		struct vgseer_client* v;
		for (iter = s->clients; iter; iter = g_list_next(iter)) {
			v = iter->data;
			if (new_active_win == v->win) {
				s->active = v;
				context_switch(s, v);
				break;
			}
		}
	}
}


/* Converts win to a string (statically allocated memory) */
gchar* win_to_str(Window win) {
	g_return_val_if_fail(win != 0, "0");

	static GString* win_str = NULL;

	if (!win_str)
		win_str = g_string_new(NULL);

	g_string_printf(win_str, "%lu", win);
	return win_str->str;
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
			die(s, EXIT_FAILURE);
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
		if (!put_param(s->active->fd, param, value)) {
			g_warning("Couldn't pass message to active client");
			drop_client(s, s->active);
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
				/* If vgd doesn't recognize that the active terminal has
				   changed, you can force it to take notice by refocusing. */
				if (s->active != v) {
					s->active = v;
					context_switch(s, v);
				}
				if (child_running(&s->display))
					refocus(s->Xdisplay, v->win, s->display_win);
			}
			else if (STREQ(value, "toggle")) {
				if (child_running(&s->display))
					child_terminate(&s->display);
				else {
					if (!child_fork(&s->display)) {
						g_critical("Couldn't fork the display");
						die(s, EXIT_FAILURE);
					}
					context_switch(s, s->active);
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

	if (s->active != v || !child_running(&s->display))
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

	/* If this was the active client, switch to another one. */
	if (s->active == v) {
		if (s->clients) {
			s->active = s->clients->data;
			context_switch(s, s->active);
		}
		else
			s->active = NULL;
	}

	/* If we're not running in persistent mode, and this was the last client,
	   exit. */
	if (!s->persistent && !s->clients)
		die(s, EXIT_SUCCESS);
}


/* Accept a new client. */
static void new_client(struct state* s) {

	g_return_if_fail(s != NULL);

	gint new_fd;
	struct sockaddr_in sa;
	socklen_t sa_len;

	enum parameter param;
	gchar* value;

	/* Accept the new client. */
	again:
	sa_len = sizeof(sa);
	if ( (new_fd = accept(s->listen_fd, (struct sockaddr*) &sa,
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
		if (STREQ(value, "ping"))
			new_ping_client(new_fd);
		else if (STREQ(value, "vgseer"))
			new_vgseer_client(s, new_fd);
		else
			g_warning("(%d) Unexpected purpose: \"%s\"", new_fd, value);
	}
	else
		g_warning("(%d) Did not receive purpose from client", new_fd);
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
	if ( (v->win = get_xid_from_title(s->Xdisplay, term_title)) == 0) {
		g_warning("(%d) Couldn't locate client's window", client_fd);
		goto reject;
	}

	/* Finally, send over the vgexpand options. */
	if (!put_param(client_fd, P_VGEXPAND_OPTS, s->vgexpand_opts->str))
		goto reject;

	/* We've got a new client. */
	v->fd = client_fd;

	/* This is our only client, so it's active by default. */
	if (!s->clients)
		s->active = v;

	s->clients = g_list_prepend(s->clients, v);

	/* Startup the display if it's not around. */
	if (!child_running(&s->display)) {
		if (!child_fork(&s->display)) {
			g_critical("Couldn't fork the display");
			die(s, EXIT_FAILURE);
		}
		/* Send the window ID, just in case this is the active window.
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


	/* Setup polling for the accept socket. */
	FD_ZERO(set);
	FD_SET(s->listen_fd, set);
	max_fd = s->listen_fd;

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

	(void) chdir("/");   /* Change working directory. */

	/* Redirect stdin, stdout, and stderr to /dev/null. */
	(void) close(STDIN_FILENO);
	(void) close(STDOUT_FILENO);
	(void) close(STDERR_FILENO);
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	return TRUE;
}


void state_init(struct state* s) {
	s->clients = NULL;
	s->persistent = FALSE;
	s->listen_fd = -1;

	child_init(&s->display);
	s->display_win = 0;

	s->vgexpand_opts = g_string_new(DEFAULT_VGEXPAND_OPTS);
	s->Xdisplay = NULL;
	s->active = NULL;
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

