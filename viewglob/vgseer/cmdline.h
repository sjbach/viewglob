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

#ifndef CMDLINE_H
#define CMDLINE_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"

G_BEGIN_DECLS

#define CMD_STEP_SIZE 512

struct cmdline {
	gchar* command;
	gint pos;
	gint length;
	gboolean rebuilding;
};

/* For cmd_wipe_in_line() -- maintain order of enumeration */
enum direction { D_RIGHT = 0, D_LEFT = 1, D_ALL = 2 };

gboolean  cmd_init(void);
gboolean  cmd_alloc(void);
gboolean  cmd_clear(void);

void      cmd_enqueue_overwrite(gchar c, gboolean preserve_cret);
void      cmd_dequeue_overwrite(void);
gboolean  cmd_has_queue(void);
gboolean  cmd_write_queue(void);
void      cmd_clear_queue(void);

gboolean  cmd_whitespace_to_left(gchar* holdover);
gboolean  cmd_whitespace_to_right(void);

gboolean  cmd_overwrite_char(gchar c, gboolean preserve_cret);
gboolean  cmd_insert_chars(gchar c, gint n);
gboolean  cmd_del_chars(gint n);
gboolean  cmd_wipe_in_line(enum direction dir);
gboolean  cmd_backspace(gint n);
void      cmd_del_trailing_crets(void);

G_END_DECLS

#endif	/* !CMDLINE_H */