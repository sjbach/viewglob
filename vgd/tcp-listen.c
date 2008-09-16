/*
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>

#include "common.h"
#include "tcp-listen.h"


int tcp_listen(const char* host, const char* serv) {
	int listenfd;
	const int on = 1;

#ifdef HAVE_GETADDRINFO
	int				n;
	struct addrinfo	hints, *res, *ressave;

	(void) memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0) {
		g_critical("tcp_listen error for %s, %s: %s",
				host, serv, gai_strerror(n));
		return -1;
	}
	ressave = res;

	do {
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (listenfd < 0)   /* error, try next one. */
			continue;

		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
					&on, sizeof(on)) == -1) {
			g_critical("Could not set socket options: %s", g_strerror(errno));
			return -1;
		}

		if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break;			/* success */

		(void) close(listenfd);	/* bind error, close and try next one */
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL) {	/* errno from final socket() or bind() */
		g_critical("Could not setup a listening socket.\n"
				"The last error was: %s", g_strerror(errno));
		return -1;
	}

	if (listen(listenfd, SOMAXCONN) == -1) {
		g_critical("Could not listen on socket: %s", g_strerror(errno));
		return -1;
	}

	freeaddrinfo(ressave);

#else
	struct sockaddr_in sin;

	int port = atoi(serv);

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		g_critical("Could not create socket: %s", g_strerror(errno));
		return -1;
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
				&on, sizeof(on)) == -1) {
		g_critical("Could not set socket options: %s", g_strerror(errno));
		return -1;
	}

	(void) memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr*) &sin, sizeof(sin)) == -1) {
		g_critical("Could not bind socket: %s", g_strerror(errno));
		return -1;
	}

	if (listen(listenfd, SOMAXCONN) == -1) {
		g_critical("Could not listen on socket: %s", g_strerror(errno));
		return -1;
	}
#endif

	return listenfd;
}

