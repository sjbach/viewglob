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

/* Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "hardened-io.h"
#include "param-io.h"
#include "display.h"
#include "x11-stuff.h"
#include "shell.h"

struct sandbox {
	enum shell_type   shell;
	int               fd;
	pid_t             pid;
};


struct state {
	GList*         clients;
	struct sandbox bash;
	struct sandbox zsh;
	struct display d;
	gboolean       persistent;
	int            listen_fd;

	Display*       Xdisplay;
	Window         active_win;
};


struct vgseer_client {
	/* Static properties (set once) */
	gboolean          local;
	pid_t             pid;
	Window            win;
	enum shell_type   shell;
	gchar*            expand_opts;
	int               fd;

	/* Dynamic properties (change often) */
	enum shell_status status;
	gchar*            cli;
	gchar*            pwd;
};


void state_init(struct state* s);
void vgseer_client_init(struct vgseer_client* v);
static void poll_loop(struct state* s);
static void die(struct state* s, int result);
static gint setup_polling(struct state* s, fd_set* set);
static void new_client(struct state* s);
static void new_ping_client(int ping_fd);
static void new_vgseer_client(struct state* s, int client_fd);
static void process_client(struct state* s, struct vgseer_client* v);
static void drop_client(struct state* s, struct vgseer_client* v);

int main(int argc, char** argv) {

	struct state s;
	struct sockaddr_in sa;

	int port = 16108;

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

	poll_loop(&s);

	return 0;
}


static void poll_loop(struct state* s) {

	g_return_if_fail(s != NULL);

	GList* iter;
	struct vgseer_client* v;

	fd_set rset;
	int max_fd;
	int nready;

	while (TRUE) {

		max_fd = setup_polling(s, &rset);

		/* Wait for input for a half second. */
		nready = hardened_select(max_fd + 1, &rset, 500);
		if (nready == -1) {
			g_critical("Problem while waiting for data: %s", g_strerror(errno));
			die(s, EXIT_FAILURE);
		}
		else if (nready > 0) {

			if (FD_ISSET(s->listen_fd, &rset)) {
				new_client(s);
				nready--;
			}

			// - Check display fd

			if (nready) {

				for (iter = s->clients; iter; iter = g_list_next(iter)) {
					v = iter->data;
					if (FD_ISSET(v->fd, &rset)) {
						process_client(s, v);
						/* It's unsafe to process more than one client, as the list
						   may have changed. */
						break;
					}
				}

			}

		}
		
		// - Figure out active window, etc.


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
				//FIXME update display (no reglob) if active
			}
			break;

		case P_PWD:
			if (v->pwd == NULL || !STREQ(v->pwd, value)) {
				g_free(v->pwd);
				v->pwd = g_strdup(value);
				g_message("(%d) New pwd: %s", v->fd, v->pwd);
				//FIXME update display (no reglob) if active
			}
			break;

		case P_CMD:
			if (v->cli == NULL || !STREQ(v->cli, value)) {
				g_free(v->cli);
				v->cli = g_strdup(value);
				g_message("(%d) New cli: %s", v->fd, v->cli);
				//FIXME update display (with reglob) if active
			}
			break;

		case P_ORDER:
			g_message("(%d) Received P_ORDER", v->fd);
			break;

		case P_VGEXPAND_DATA:
			g_message("(%d) Received vgexpand_data", v->fd);
			break;

		case P_EOF:
			g_message("(%d) EOF from client", v->fd);
			drop_client(s, v);
			break;

		default:
			g_warning("(%d) Unexpected parameter: %d = %s", v->fd, param, value);
			drop_client(s, v);
			break;
	}
}


/* Disconnect the client and free its resources. */
static void drop_client(struct state* s, struct vgseer_client* v) {

	g_return_if_fail(s != NULL);
	g_return_if_fail(v != NULL);

	s->clients = g_list_remove(s->clients, v);

	(void) close(v->fd);
	g_message("(%d) Dropped client", v->fd);
	g_free(v->expand_opts);
	g_free(v);
}


/* Accept a new client. */
static void new_client(struct state* s) {

	g_return_if_fail(s != NULL);

	int new_fd;
	struct sockaddr_in sa;
	socklen_t sa_len;

	enum parameter param;
	gchar* value;

	/* Accept the new client. */
	again:
	sa_len = sizeof(sa);
	if ( (new_fd = accept(s->listen_fd, (struct sockaddr*) &sa, &sa_len)) == -1) {
		if (errno == EINTR)
			goto again;
		else {
			g_warning("Error while accepting new client: %s", g_strerror(errno));
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


static void new_ping_client(int ping_fd) {

	g_return_if_fail(ping_fd >= 0);

	g_message("(%d) Client is pinging", ping_fd);
	if (!put_param(ping_fd, P_STATUS, "yo"))
		g_warning("(%d) Couldn't ping back", ping_fd);

	(void) close(ping_fd);
}


static void new_vgseer_client(struct state* s, int client_fd) {

	g_return_if_fail(s != NULL);
	g_return_if_fail(client_fd >= 0);

	enum parameter param;
	gchar* value;
	struct vgseer_client* v;

	g_message("(%d) Client is a vgseer", client_fd);

	v = g_new(struct vgseer_client, 1);
	vgseer_client_init(v);

	/* Locality */
	if (!get_param(client_fd, &param, &value) || param != P_LOCALITY)
		goto out_of_sync;
	if (STREQ(value, "local"))
		v->local = TRUE;
	else if (STREQ(value, "remote"))
		v->local = FALSE;
	else {
		g_warning("(%d) Unexpected locality: \"%s\"", client_fd, value);
		goto reject;
	}

	/* Shell */
	if (!get_param(client_fd, &param, &value) || param != P_SHELL)
		goto out_of_sync;
	if (STREQ(value, "bash"))
		v->shell = ST_BASH;
	else if (STREQ(value, "zsh"))
		v->shell = ST_ZSH;
	else {
		g_warning("(%d) Unexpected shell: \"%s\"", client_fd, value);
		goto reject;
	}

	/* Pid */
	if (!get_param(client_fd, &param, &value) || param != P_PROC_ID)
		goto out_of_sync;
	switch (v->pid = strtol(value, NULL, 10)) {
		case LONG_MIN:
		case LONG_MAX:
			g_warning("(%d) Pid value is out of bounds: \"%s\"", client_fd, value);
			goto reject;
	}

	/* Vgexpand Opts */
	// FIXME make sure vgexpand opts are safe
	if (!get_param(client_fd, &param, &value) || param != P_VGEXPAND_OPTS)
		goto out_of_sync;
	v->expand_opts = g_strdup(value);

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
	s->clients = g_list_prepend(s->clients, v);
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
	if (display_running(&s->d)) {
		FD_SET(s->d.fd, set);
		max_fd = MAX(max_fd, s->d.fd);
	}

	return max_fd;
}


static void die(struct state* s, int result) {
	// - Tell vgseer clients to disable viewglob functionality
	// - Kill display
	exit(result);
}


void state_init(struct state* s) {
	s->clients = NULL;
	s->bash.shell = ST_BASH;
	s->bash.pid = -1;
	s->bash.fd = -1;
	s->zsh.shell = ST_ZSH;
	s->zsh.pid = -1;
	s->zsh.fd = -1;
	s->d.pid = -1;
	s->d.fd = -1;
	s->persistent = FALSE;
	s->listen_fd = -1;

	s->Xdisplay = NULL;
	s->active_win = 0;
}


void vgseer_client_init(struct vgseer_client* v) {

	v->local = TRUE;
	v->pid = -1;
	v->win = 0;
	v->shell = ST_BASH;
	v->expand_opts = NULL;
	v->fd = -1;

	v->status = SS_LOST;
	v->cli = NULL;
	v->pwd = NULL;
}

