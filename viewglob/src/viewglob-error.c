/*
	error.c -- display formatted error diagnostics of varying severity
	Copyright (C) 2000 Gary V. Vaughan

	viewglob-error.c -- Modified for viewglob's purposes.
	Copyright (C) 2004 Stephen Bach

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
	Much of the following is taken from the book GNU Auotconf, Automake,
	and Libtool by Gary V. Vaughan, Ben Elliston, Tom Tromey, and Ian
	Lance Taylor.  The website is here:
		http://sources.redhat.com/autobook/
*/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "viewglob-error.h"
#include <string.h>

char *program_name = NULL;

static void error(int exit_status, const char *mode, const char *message) {
	fprintf (stderr, "%s: %s: %s.\n", program_name, mode, message);

	if (exit_status >= 0)
		exit(exit_status);
}

void viewglob_warning(const char *message) {
	error(-1, "warning", message);
}

void viewglob_error(const char *message) {
	error(-1, "ERROR", message);
}

void viewglob_fatal(const char *message) {
	error(EXIT_FAILURE, "FATAL", message);
}


void set_program_name(const char *path) {
	if (!program_name)
		program_name = basename(path);
}

/* Takes a sanitized path and returns the base (file) name. */
char* basename(const char* path) {
	char* base;
	int slash_pos;
	size_t path_length;

	path_length = strlen(path);

	/* Find the last / in the path. */
	slash_pos = find_prev(path, path_length - 1, '/');

	if (slash_pos == 0 && path_length == 1) {
		/* It's root. */
		base = XMALLOC(char, 2);
		(void)strcpy(base, "/");
	}
	else if (slash_pos == -1) {
		/* It's a relative path at pwd. */
		base = XMALLOC(char, path_length + 1);
		(void)strcpy(base, path);
	}
	else {
		base = XMALLOC(char, path_length - slash_pos);
		(void)strcpy(base, path + slash_pos + 1);
	}

	return base;
}


/* Return the position of the previous c from pos, or -1 if not found. */
int find_prev(const char* string, int pos, char c) {
	bool found = false;

	while (pos >= 0) {
		if ( *(string + pos) == c ) {
			found = true;
			break;
		}
		pos--;
	}

	if (found)
		return pos;
	else
		return -1;
}

