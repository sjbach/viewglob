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

/* Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "hardened-io.h"
#include "param-io.h"
#include "children.h"
#include "x11-stuff.h"
#include "shell.h"


struct state {
	GList*                clients;
	struct vgseer_client* active;
	struct child          display;
	Window                display_win;

	Display*              Xdisplay;
	gboolean              persistent;
	gint                  listen_fd;
};


struct vgseer_client {
	/* Static properties (set once) */
	pid_t             pid;
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


gint main(gint argc, gchar** argv) {

	struct state s;
	struct sockaddr_in sa;

	gint port = 16108;

	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	state_init(&s);

	/* Get a connection to the X display. */
	if ( (s.Xdisplay = XOpenDisplay(NULL)) == NULL) {
		g_critical("Could not connect to X server");
		exit(EXIT_FAILURE);
	}

	/* Setup listening socket. */
	(void) memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(port);
	if ( (s.listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		g_critical("Could not create socket: %s", g_strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (bind(s.listen_fd, (struct sockaddr*) &sa, sizeof(sa)) == -1) {
		g_critical("Could not bind socket: %s", g_strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (listen(s.listen_fd, SOMAXCONN) == -1) {
		g_critical("Could not listen on socket: %s", g_strerror(errno));
		exit(EXIT_FAILURE);
	}

	s.display.exec_name = "/home/steve/vg/vgdisplay/vgclassic";

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

			if (FD_ISSET(s->display.fd_in, &rset)) {
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
				if (child_running(&s->display))
					context_switch(s, v);
				break;
			}
		}
	}
}


static void context_switch(struct state* s, struct vgseer_client* v) {
	g_return_if_fail(s != NULL);
	g_return_if_fail(v != NULL);

	g_message("(disp) Context switch to client %d", v->fd);

	gint fd = s->display.fd_out;

	/* Send a bunch of data to the display. */
	if (	!put_param(fd, P_STATUS, shell_status_to_string(v->status)) ||
			!put_param(fd, P_CMD, v->cli->str) ||
			!put_param(fd, P_DEVELOPING_MASK, v->developing_mask->str) ||
			!put_param(fd, P_MASK, v->mask->str) ||
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
			g_message("(disp) P_FILE: %s", value);
			break;

		case P_KEY:
			g_message("(disp) P_KEY: %s", value);
			break;

		case P_WIN_ID:
			g_message("(disp) P_WIN_ID: %s", value);
			if ((s->display_win = strtoul(value, NULL, 10)) == ULONG_MAX) {
				g_warning("(disp) window ID is out of bounds: %s", value);
				s->display_win = 0;
			}
			else {
				g_message("(disp) Display has window id %lu", s->display_win);
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
				g_message("(%d) New status: %s", v->fd, value);
				update_display(s, v, param, value);
			}
			break;

		case P_PWD:
			v->pwd = g_string_assign(v->pwd, value);
			g_message("(%d) New pwd: %s", v->fd, v->pwd->str);
			break;

		case P_CMD:
			v->cli = g_string_assign(v->cli, value);
			g_message("(%d) New cli: %s", v->fd, v->cli->str);
			update_display(s, v, param, value);
			break;

		case P_MASK:
			/* Clear the developing mask, if any. */
			v->developing_mask = g_string_truncate(v->developing_mask, 0);
			v->mask = g_string_assign(v->mask, value);

			if (v->mask->len == 0)
				g_message("(%d) Mask cleared", v->fd);
			else
				g_message("(%d) New mask: %s", v->fd, v->mask->str);
			update_display(s, v, P_MASK, value);
			update_display(s, v, P_DEVELOPING_MASK, "");
			break;

		case P_DEVELOPING_MASK:
			v->developing_mask = g_string_assign(v->developing_mask, value);
			if (v->developing_mask->len == 0)
				g_message("(%d) Developing mask cleared", v->fd);
			else {
				g_message("(%d) New developing mask: %s", v->fd,
						v->developing_mask->str);
			}
			update_display(s, v, param, value);
			break;

		case P_ORDER:
			g_message("(%d) Received P_ORDER: %s", v->fd, value);
			if (STREQ(value, "refocus"))
				refocus(s->Xdisplay, v->win, s->display_win);
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
			g_message("(%d) Received vgexpand_data: (lots)", v->fd);
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

	if (	s->active == v &&
			child_running(&s->display) &&
			!put_param(s->display.fd_out, param, value)) {
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
			if (child_running(&s->display))
				context_switch(s, s->active);
		}
		else
			s->active = NULL;
	}
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

	g_message("(%d) Client is a vgseer", client_fd);

	v = g_new(struct vgseer_client, 1);
	vgseer_client_init(v);

	/* Pid */
	if (!get_param(client_fd, &param, &value) || param != P_PROC_ID)
		goto out_of_sync;
	switch (v->pid = strtol(value, NULL, 10)) {
		case LONG_MIN:
		case LONG_MAX:
			g_warning("(%d) Pid value is out of bounds: \"%s\"",
					client_fd, value);
			goto reject;
	}

	/* Tell vgseer client to set a title on its terminal. */
	if (!put_param(client_fd, P_ORDER, "set-title"))
		goto reject;
	if (!get_param(client_fd, &param, &value) || param != P_STATUS ||
			!STREQ(value, "title-set"))
		goto out_of_sync;

	/* Find the client's window. */
	gchar title[100];
	if (snprintf(title, sizeof(title), "vgseer%ld", (long) v->pid) <= 0) {
		g_critical("Couldn't convert the pid to a string");
		goto reject;
	}
	if ( (v->win = get_xid_from_title(s->Xdisplay, title)) == 0) {
		g_warning("(%d) Couldn't locate client's window", client_fd);
		goto reject;
	}
	g_message("(%d) Client has window id %lu", client_fd, v->win);
	if (!put_param(client_fd, P_ORDER, "continue"))
		goto reject;

	/* We've got a new client. */
	v->fd = client_fd;

	/* This is our only client, so it's active by default. */
	if (!s->clients)
		s->active = v;

	s->clients = g_list_prepend(s->clients, v);

	/* Startup the display if it's not around. */
	if (!child_running(&s->display) && !child_fork(&s->display)) {
		g_critical("Couldn't fork the display");
		die(s, EXIT_FAILURE);
	}

	return;

out_of_sync:
	g_warning("(%d) Client sent unexpected data", client_fd);
reject:
	g_warning("(%d) Client rejected", client_fd);
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


void state_init(struct state* s) {
	s->clients = NULL;
	s->persistent = FALSE;
	s->listen_fd = -1;

	child_init(&s->display);
	s->display_win = 0;

	s->Xdisplay = NULL;
	s->active = NULL;
}


void vgseer_client_init(struct vgseer_client* v) {

	v->pid = -1;
	v->win = 0;
	v->fd = -1;

	v->status = SS_LOST;
	v->cli = g_string_new(NULL);
	v->pwd = g_string_new(NULL);
	v->developing_mask = g_string_new(NULL);
	v->mask = g_string_new(NULL);
	v->expanded = g_string_new(NULL);
}

