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

#include "common.h"
#include "read-conf.h"

static enum opt_type string_to_opt(gchar* string);
static gchar* opt_to_string(enum opt_type opt);

static gchar* word_start(gchar* p);
static gchar* word_end(gchar* p);
static gchar* skip_line(gchar* p);


static gchar* contents = NULL;
static gchar* pos = NULL;


static gchar* opt_types[OT_COUNT] = {
	"host",
	"port",
	"shell-mode",
	"disable-star",
	"executable",
	"persistent",
	"sort-style",
	"dir-order",
	"font-size-modifier",
	"disable-icons",
	"error",   /* OT_ERROR */
	"done",    /* OT_DONE */
};


/* Attempt to open the given file and read in its contents. */
gboolean open_conf(gchar* filename) {
	g_return_val_if_fail(filename != NULL, FALSE);
	GError* err;

	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		return FALSE;

	if (!g_file_get_contents(filename, &contents, NULL, &err)) {
		g_warning("\"%s\" exists but could not be read: %s",
				filename, err->message);
		g_error_free(err);
		return FALSE;
	}

	pos = contents;
	
	return TRUE;
}


enum opt_type read_opt(gchar** value) {
	g_return_val_if_fail(contents != NULL, OT_ERROR);
	g_return_val_if_fail(pos != NULL, OT_ERROR);
	g_return_val_if_fail(value != NULL, OT_ERROR);

	enum opt_type opt;
	gchar* end;

	/* Find the first line which isn't a comment or blank. */
	do {
		pos = g_strchug(pos);
		if (*pos == '#') {
			if (!(pos = skip_line(pos)))
				return OT_DONE;
		}
		else
			break;
	} while (TRUE);

	if (*pos == '\0') {
		g_free(contents);
		contents = NULL;
		pos = NULL;
		return OT_DONE;
	}

	/* Delimit the string and convert to opt_type. */
	end = word_end(pos);
	*end = '\0';
	opt = string_to_opt(pos);

	if (opt == OT_ERROR)
		return OT_ERROR;

	/* Find the beginning of the argument (if there is one). */
	pos = *value = word_start(end + 1);

	/* Delimit the argument. */
	pos = word_end(pos);
	if (*pos != '\0') {
		*pos = '\0';
		pos++;
	}

	return opt;
}


static gchar* word_start(gchar* p) {
	g_return_val_if_fail(p != NULL, NULL);

	while (*p == ' ' || *p == '\t')
		p++;

	return p;
}


static gchar* word_end(gchar* p) {
	g_return_val_if_fail(p != NULL, NULL);

	while (!g_ascii_isspace(*p) && *p != '\0')
		p++;

	return p;
}


static gchar* skip_line(gchar* p) {
	g_return_val_if_fail(p != NULL, NULL);

	p = strstr(p, "\n");

	if (p)
		p++;

	return p;
}


static enum opt_type string_to_opt(gchar* string) {
	g_return_val_if_fail(string != NULL, OT_ERROR);

	enum opt_type opt = OT_ERROR;
	int i;

	for (i = 0; i < OT_COUNT; i++) {
		if (STREQ(string, opt_types[i])) {
			opt = i;
			break;
		}
	}

	return opt;
}


static gchar* opt_to_string(enum opt_type opt) {
	g_return_val_if_fail(opt < OT_COUNT, opt_types[OT_ERROR]);

	return opt_types[opt];
}

