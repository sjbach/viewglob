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

#ifndef PARAM_IO_H
#define PARAM_IO_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

#include <glib.h>

G_BEGIN_DECLS

/* Order must be maintained. */
enum parameter {
	P_NONE,

	/* The purpose of the connection. */
	P_PURPOSE,

	/* Used in vgseer initialization. */
	P_SHELL,
	P_PROC_ID,            /* P_PID conflicts with wait.h. */

	/* Volatile vgseer properties. */
	P_STATUS,
	P_PWD,
	P_CMD,
	P_MASK,
	P_DEVELOPING_MASK,

	/* For remote vgseers. */
	P_VGEXPAND_DATA,

	/* Directives to vgseer shells or the display. */
	P_ORDER,

	/* From the display. */
	P_KEY,
	P_FILE,

	/* Explanation for a previous parameter. */
	P_REASON,

	/* For when the connection gets closed (read = 0) */
	P_EOF,

	/* The number of parameter types. */
	P_COUNT,
};

gboolean get_param(int fd, enum parameter* param, gchar** value);
gboolean put_param(int fd, enum parameter param, gchar* value);

G_END_DECLS

#endif
