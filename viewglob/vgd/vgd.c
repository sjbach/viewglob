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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <glib.h>

#include "common.h"
#include "hardened-io.h"
#include "param-io.h"
#include "display.h"
#include "x11-stuff.h"


/* Supported shell types. */
//FIXME
enum shell_type {
	ST_BASH,
	ST_ZSH,
};

enum shell_status {
	SS_EXECUTING,
	SS_PROMPT,
	SS_LOST,
	SS_TITLE_SET,
};


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
	Window         active_win;
	gboolean       persistent;
	int            listen_fd;
};


struct vgseer_client {
	/* Static properties (set once) */
	gboolean          local;
	pid_t             pid;
	Window            win;
	enum shell_type   shell;
	gchar*            expand_opts;

	/* Dynamic properties (change often) */
	enum shell_status status;
	gchar*            cli;
	gchar*            pwd;
};


void state_init(struct state* s);
static void data_loop(struct state* s);

int main(int argc, char** argv) {

	struct state s;
	struct sockaddr_in sa;

	int port = 16108;

	/* Set the program name. */
	gchar* basename = g_path_get_basename(argv[0]);
	g_set_prgname(basename);
	g_free(basename);

	state_init(&s);

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

	data_loop(&s);

	return 0;
}


static void data_loop(struct state* s) {

	GList* iter;
	struct vgseer_client v;

	fd_set rset;
	int max_fd;

	while (TRUE) {

		/* Setup polling for the accept socket. */
		FD_ZERO(&rset);
		FD_SET(s->listen_fd, &rset);
		max_fd = s->listen_fd;

		/* Setup polling for each vgseer client. */
		for (iter = s->clients; iter; iter = g_list_next(iter)) {
			v = iter->data;
			g_return_if_fail(v->fd >= 0);
			FD_SET(v->fd, &rset);
			max_fd = MAX(max_fd, v->fd);
		}

		/* And poll the display. */
		if (display_running(&s->d)) {
			FD_SET(s->d.fd, &rset);
			max_fd = MAX(max_fd, s->d.fd);
		}




	}
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
	s->active_win = 0;
	s->persistent = FALSE;
	s->listen_fd = -1;
}

