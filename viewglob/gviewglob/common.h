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

#ifndef COMMON_H
#define COMMON_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#define DEBUG_ON 1

#if DEBUG_ON
#  include <stdio.h>
#endif

#include <stdlib.h>        /* For atol */

#ifndef EXIT_SUCCESS
#  define EXIT_SUCCESS  0
#  define EXIT_FAILURE  1
#endif

#if DEBUG_ON
#define DEBUG(blah)	do {					\
						fprintf blah;	\
						fflush(df);			\
					} while (0)
#else
#define DEBUG(blah)	do { } while(0)
#endif

#endif /* !COMMON_H */
