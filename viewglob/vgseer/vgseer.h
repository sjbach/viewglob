/*
	Copyright (C) 2004, 2005 Stephen Bach
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

#ifndef SEER_H
#define SEER_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "sequences.h"
#include "cmdline.h"
#include "children.h"

G_BEGIN_DECLS


/* Data structure for the user's shell. */
struct user_shell {
	gchar* pwd;
	struct cmdline cmd;
	struct pty_child s;

	gboolean term_size_changed;
	gboolean expect_newline;
};


G_END_DECLS

#endif /* !SEER_H */

