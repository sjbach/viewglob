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

#ifndef BUFFER_H
#define BUFFER_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "circular.h"
#include "sequences.h"

BEGIN_C_DECLS

void prepend_holdover(Buffer* b);
void create_holdover(Buffer* b, bool write_later);
void eat_segment(Buffer* b);
void pass_segment(Buffer* b);

END_C_DECLS

#endif /* !BUFFER_H */
