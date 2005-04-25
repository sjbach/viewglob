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

#include "common.h"
#include "tcp-connect.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>

int tcp_connect(const char *host, const char *serv) {
	int sockfd;

#ifdef HAVE_GETADDRINFO
	int				n;
	struct addrinfo	hints, *res, *ressave;

	(void) memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0) {
		g_critical("tcp_connect error for %s, %s: %s",
				 host, serv, gai_strerror(n));
		return -1;
	}
	ressave = res;

	do {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
			continue;	/* ignore this one */

		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break;		/* success */

		(void) close(sockfd);	/* ignore this one */
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL) {	/* errno set from final connect() */
		g_critical("tcp_connect error for %s, %s: %s", host, serv,
				g_strerror(errno));
		return -1;
	}

	freeaddrinfo(ressave);

#else
	struct sockaddr_in servaddr;
	struct in_addr** pptr;
	struct in_addr* inetaddrp[2];
	struct in_addr inetaddr;
	struct hostent* hp;

	int port = atoi(serv);

	if ((hp = gethostbyname(host)) == NULL) {
		if (inet_aton(host, &inetaddr) == 0) {
			g_critical("Could not convert hostname: %s",
					g_strerror(errno));
			return -1;
		}
		else {
			inetaddrp[0] = &inetaddr;
			inetaddrp[1] = NULL;
			pptr = inetaddrp;
		}
	}
	else
		pptr = (struct in_addr**) hp->h_addr_list;

	for ( ; *pptr != NULL; pptr++) {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			g_critical("Could not create socket: %s", g_strerror(errno));
			return -1;
		}

		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);
		memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));

		if (connect(sockfd, (struct sockaddr*) &servaddr,
					sizeof(servaddr)) == 0)
			break;   /* success */

		g_warning("Connection error: %s", g_strerror(errno));
		(void) close(sockfd);
	}
	
	if (*pptr == NULL) {
		g_critical("Unable to connect.");
		return -1;
	}
#endif

	return(sockfd);
}

