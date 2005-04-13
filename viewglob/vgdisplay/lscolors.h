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

#ifndef LSCOLORS_H
#define LSCOLORS_H

#include "file-types.h"
#include <gtk/gtklabel.h>
#include <pango/pango.h>

/* Note: code depends on this specific ordering. */
enum term_color_code {
	TCC_NONE,
	TCC_BLACK,
	TCC_RED,
	TCC_GREEN,
	TCC_YELLOW,
	TCC_BLUE,
	TCC_MAGENTA,
	TCC_CYAN,
	TCC_WHITE,
};

/* Skipped attributes: blink, concealed. */
enum term_attribute_code {
	TAC_BOLD		= 1 << 0,
	TAC_UNDERSCORE	= 1 << 1,
	TAC_REVERSE		= 1 << 2,
};

typedef struct _TermTextAttr TermTextAttr;
struct _TermTextAttr {
	enum term_color_code fg;
	enum term_color_code bg;
	enum term_attribute_code attr;
	PangoAttrList* p_list;
};

void parse_ls_colors(gint size_modifier);
void label_set_attributes(gchar* name, FileType type, GtkLabel* label);
void set_color(enum term_color_code code, GdkColor* color);

#endif /* !LSCOLORS_H */

