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

#ifndef READ_CONF_H
#define READ_CONF_H

#include "common.h"

G_BEGIN_DECLS

enum opt_type {
	OT_HOST,
	OT_PORT,
	OT_SHELL_MODE,
	OT_DISABLE_STAR,
	OT_EXECUTABLE,
	OT_PERSISTENT,
	OT_SORT_STYLE,
	OT_DIR_ORDER,
	OT_FONT_SIZE_MODIFIER,
	OT_DISABLE_ICONS,
	OT_ERROR,
	OT_DONE,
	OT_COUNT,
};



gboolean open_conf(gchar* file);
enum opt_type read_opt(gchar** value);

G_END_DECLS

#endif

