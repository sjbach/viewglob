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

#ifndef SHELL_H
#define SHELL_H

#include "common.h"

G_BEGIN_DECLS

/* Supported shell types.
   Maintain order. */
enum shell_type {
	ST_BASH,
	ST_ZSH,
	ST_ERROR,
	ST_COUNT,
};

/* Maintain order. */
enum shell_status {
	SS_EXECUTING,
	SS_PROMPT,
	SS_LOST,
	SS_TITLE_SET,
	SS_ERROR,
	SS_COUNT,
};


gchar*            shell_type_to_string(enum shell_type shell);
enum shell_type   string_to_shell_type(gchar* string);
gchar*            shell_status_to_string(enum shell_status status);
enum shell_status string_to_shell_status(gchar* string);


G_END_DECLS

#endif /* !SHELL_H */

