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
#include "child.h"
#include "hardened-io.h"
#include "conf-to-args.h"

gboolean conf_to_args(int* argc, char*** argv, gchar* file) {
	g_return_val_if_fail(argc != NULL, FALSE);
	g_return_val_if_fail(argv != NULL, FALSE);
	g_return_val_if_fail(file != NULL, FALSE);

	struct child conf_to_args;

	GString* arg_str = NULL;

	gchar buf[1024];
	gint nread;

	gchar* home = getenv("HOME");
	if (!home) {
		g_warning("User has no home!");
		return FALSE;
	}

	/* Call conf-to-args.sh with the configuration file as an argument. */
	child_init(&conf_to_args);
	conf_to_args.exec_name = VG_LIB_DIR "/conf-to-args.sh";
	args_add(&conf_to_args.args,
			g_strconcat(home, "/", file, NULL));
	if (!child_fork(&conf_to_args))
		return FALSE;

	arg_str = g_string_new(NULL);

	/* Get all the output from conf-to-args.sh into one buffer. */
	gboolean in_loop = TRUE;
	while (in_loop) {
		switch (hardened_read(conf_to_args.fd_in, buf, sizeof buf, &nread)) {
			case IOR_OK:
				arg_str = g_string_append_len(arg_str, buf, nread);
				break;
			case IOR_ERROR:
				g_warning("Error reading from configuration file: %s",
						g_strerror(errno));
				goto fail;
				break;
			case IOR_EOF:
				in_loop = FALSE;
				break;
		}
	}

	/* It's probably already dead, but just to be sure. */
	(void) child_terminate(&conf_to_args);

	if (arg_str->len == 0) {
		g_string_free(arg_str, TRUE);
		return FALSE;
	}

	/* Remove a trailing newline. */
	if (arg_str->str[arg_str->len - 1] == '\n')
		arg_str = g_string_truncate(arg_str, arg_str->len - 1);

	/* Setup the argument array. */
	*argv = g_strsplit(arg_str->str, " ", -1);
	gint i;
	for (i = 0; *(*argv + i) != NULL; i++)
		;
	*argc = i;

	/*for (i = 0; i < *argc; i++)
		g_printerr("%s\n", *(*argv + i));*/

	g_string_free(arg_str, TRUE);
	return TRUE;

fail:
	// TODO free child args
	g_string_free(arg_str, TRUE);
	(void) child_terminate(&conf_to_args);
	return FALSE;
}

