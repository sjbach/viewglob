/* error.h -- display formatted error diagnostics of varying severity
   Copyright (C) 2000 Gary V. Vaughan

   viewglob-error.h -- Modified for viewglob's purposes.
   Copyright (C) 2004, 2005 Stephen Bach
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
   Much of the following is taken from the book GNU Auotconf, Automake,
   and Libtool by Gary V. Vaughan, Ben Elliston, Tom Tromey, and Ian
   Lance Taylor.  The website is here:
       http://sources.redhat.com/autobook/
*/

#ifndef VIEWGLOB_ERROR_H
#define VIEWGLOB_ERROR_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "vgseer-common.h"

G_BEGIN_DECLS

void set_program_name (const char *argv0);

void viewglob_warning (const char *message);
void viewglob_error   (const char *message);
void viewglob_fatal   (const char *message);

int find_prev(const char* string, int pos, char c);
char* basename(const char* path);

G_END_DECLS

#endif /* !VIEWGLOB_ERROR_H */
