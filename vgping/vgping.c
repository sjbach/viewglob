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

#include "param-io.h"
#include "hardened-io.h"
#include "socket-connect.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

gint main(gint argc, char** argv) {

	gint fd;
	gchar* host = "localhost";
	gchar* port = "16108";

	enum parameter param;
	gchar* value;

	/* Emit no output. */
	(void) close(STDIN_FILENO);
	(void) close(STDOUT_FILENO);
	(void) close(STDERR_FILENO);
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	if (argc >= 3) {
		host = argv[1];
		port = argv[2];
	}

	/* If the "port"' contains '.', it's assumed to be a unix socket name
	   rather than a port. */
	if (strchr(port, '.'))
		fd = unix_connect(port + 1);
	else
		fd = tcp_connect(host, port);
	
	if (fd == -1)
		return EXIT_FAILURE;

	if (!put_param(fd, P_PURPOSE, "vgping"))
		return EXIT_FAILURE;

	if (!get_param(fd, &param, &value) || param != P_STATUS ||
			!STREQ(value, "yo"))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

