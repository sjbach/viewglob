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
#include "shell.h"

#include <string.h>

static gchar* shell_types[ST_COUNT] = {
	"bash",
	"zsh",
	"error",
};

static gchar* shell_statuses[SS_COUNT] = {
	"executing",
	"prompt",
	"lost",
	"title-set",
	"error",
};


gchar* shell_type_to_string(enum shell_type type) {
	g_return_val_if_fail(type >= 0 && type < ST_COUNT,
			shell_types[ST_ERROR]);
	return shell_types[type];
}


gchar* shell_status_to_string(enum shell_status status) {
	g_return_val_if_fail(status >= 0 && status < SS_COUNT,
			shell_statuses[SS_ERROR]);
	return shell_statuses[status];
}


enum shell_type string_to_shell_type(gchar* string) {
	g_return_val_if_fail(string != NULL, ST_ERROR);

	enum shell_type type = ST_ERROR;

	int i;
	for (i = 0; i < ST_COUNT; i++) {
		if (STREQ(string, shell_types[i])) {
			type = i;
			break;
		}
	}

	return type;
}


enum shell_status string_to_shell_status(gchar* string) {
	g_return_val_if_fail(string != NULL, SS_ERROR);

	enum shell_status status = SS_ERROR;

	int i;
	for (i = 0; i < SS_COUNT; i++) {
		if (STREQ(string, shell_statuses[i])) {
			status = i;
			break;
		}
	}

	return status;
}

