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
#include "logging.h"


void logging(const gchar *log_domain, GLogLevelFlags level,
		const gchar *message, gpointer dummy) {

	gchar* level_str;

	if (level & G_LOG_LEVEL_CRITICAL)
		level_str = "CRITICAL";
	else if (level & G_LOG_LEVEL_WARNING)
		level_str = "Warning";
	else
		level_str = "FYI";

	g_printerr("%s: %s: %s\n", g_get_prgname(), level_str, message);
}

