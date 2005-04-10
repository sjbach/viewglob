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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>

#include "common.h"
#include "syslogging.h"


void openlog_wrapped(const gchar* ident) {
	openlog(ident, LOG_PID, LOG_USER);
}


void syslogging(const gchar* log_domain, GLogLevelFlags log_level,
		const gchar* message, gpointer dummy) {

	int priority;

	if (log_level & G_LOG_LEVEL_CRITICAL)
		priority = LOG_CRIT;
	else if (log_level & G_LOG_LEVEL_WARNING)
		priority = LOG_WARNING;
//	else if (log_level & G_LOG_LEVEL_MESSAGE)
	else
		priority = LOG_NOTICE;

	syslog(priority, message);
}


