/*
	Copyright (C) 2004 Stephen Bach
	This file is part of the viewglob package.

	viewglob is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	viewglob is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with viewglob; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"

#include "common.h"
#include <glib.h>

GIOChannel* out_channel = NULL;

void feedback_set_channel(GIOChannel* channel) {
	g_return_if_fail(channel != NULL);
	out_channel = channel;
}


gboolean feedback_write_string(gchar* string, gsize bytes) {

	GError* error = NULL;
	gsize bytes_written;
	gboolean in_loop = TRUE;
	gboolean result = TRUE;

	g_return_val_if_fail(out_channel != NULL, FALSE);

	while (in_loop) {
		switch (g_io_channel_write_chars(out_channel, string, bytes, &bytes_written, &error)) {

			case (G_IO_STATUS_ERROR):
				g_printerr("gviewglob: %s\n", error->message);
				in_loop = result = FALSE;
				break;

			case (G_IO_STATUS_NORMAL):
				in_loop = FALSE;
				break;

			case (G_IO_STATUS_AGAIN):
				break;

			default:
				g_warning("Unexpected result from g_io_channel_write_chars.");
				in_loop = result = FALSE;
				break;
		}
	}

	return result;
}

