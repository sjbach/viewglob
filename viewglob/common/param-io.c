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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "param-io.h"
#include "hardened-io.h"

#include <netinet/in.h>
#include <string.h>

/* On my busiest machine, the vgexpand output for /usr/bin, /usr/include, and
   /usr/lib all at once is 85K.  So 100K should be a good max for now.
   Probably in the future it would be better to have a growing buffer. */
#define BUFFER_SIZE 102400

/* Order must correspond to enum parameter type. */
static gchar* params[P_COUNT] = {
	"none",
	"purpose",
	"version",
	"pid",
	"status",
	"pwd",
	"cmd",
	"mask",
	"developing-mask",
	"vgexpand-data",
	"order",
	"key",
	"file",
	"win-id",
	"reason",
	"eof",      /* This one shouldn't be received as a string. */
};


static enum parameter string_to_param(gchar* string);
static gchar*         param_to_string(enum parameter param);


gboolean get_param(int fd, enum parameter* param, gchar** value) {

	g_return_val_if_fail(fd >= 0, FALSE);
	g_return_val_if_fail(param != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	static gchar buf[BUFFER_SIZE];
	guint32 bytes;

	/* Find out how many bytes we're going to need to read. */
	switch (read_all(fd, &bytes, sizeof(bytes))) {
		case IOR_OK:
			bytes = ntohl(bytes);
			break;
		case IOR_EOF:
			goto eof_reached;
			/*break;*/
		case IOR_ERROR:
			g_warning("Error while reading data length: %s",
					g_strerror(errno));
			return FALSE;
			/*break;*/
		default:
			g_return_val_if_reached(FALSE);
			/*break;*/
	}

	/* Get the whole parameter/value pair. */
	switch (read_all(fd, buf, bytes)) {
		case IOR_OK:
			break;
		case IOR_EOF:
			goto eof_reached;
			/*break;*/
		case IOR_ERROR:
			g_warning("Error while reading data: %s", g_strerror(errno));
			return FALSE;
			/*break;*/
		default:
			g_return_val_if_reached(FALSE);
			/*break;*/
	}

	/* Now parse the data. */
	gchar* start;
	gchar* end;
	gchar* p = NULL;
	gchar* v = NULL;

	/* First the parameter name. */
	start = buf;
	end = g_strstr_len(buf, bytes, ":");
	if (!end)
		goto fail;
	*end = '\0';
	p = start;
	bytes -= end - start + 1;

	/* Now the value. */
	start = end + 1;
	end = g_strstr_len(start, bytes, "\027\027");
	if (!end)
		goto fail;
	*end = '\0';
	v = start;
	bytes -= end - start + 2;

	*param = string_to_param(p);
	*value = v;
	return TRUE;

	eof_reached:
	*param = P_EOF;
	*value = "EOF received";
	return TRUE;

	fail:
	g_warning("Data in incorrect format");
	return FALSE;
}


gboolean put_param(int fd, enum parameter param, gchar* value) {

	g_return_val_if_fail(fd >= 0, FALSE);
	g_return_val_if_fail(param < P_COUNT, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	gchar* string;
	guint32 bytes;
	gsize len;

	/* Make the string. */
	string = g_strconcat(param_to_string(param), ":", value, "\027\027", NULL);
	len = strlen(string);

	/* Make sure the length is okay, and convert to network format. */
	if (len > BUFFER_SIZE) {
		g_warning("String length is greater than %u", BUFFER_SIZE);
		return FALSE;
	}
	bytes = htonl((guint32)len);

	/* Use writev() to try to avoid Nagle effect */
	struct iovec iov[2];
	iov[0].iov_base = &bytes;
	iov[0].iov_len = sizeof(bytes);
	iov[1].iov_base = string;
	iov[1].iov_len = len;
	if (writev_all(fd, iov, 2) != IOR_OK) {
		g_warning("Could not write pair: %s", g_strerror(errno));
		return FALSE;
	}

	return TRUE;
}


static enum parameter string_to_param(gchar* string) {

	g_return_val_if_fail(string != NULL, P_NONE);

	enum parameter param = P_NONE;
	int i;

	for (i = 0; i < P_COUNT; i++) {
		if (STREQ(string, params[i])) {
			param = i;
			break;
		}
	}

	return param;
}


static gchar* param_to_string(enum parameter param) {

	g_return_val_if_fail(param < P_COUNT, params[P_NONE]);

	return params[param];
}

