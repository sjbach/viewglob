/* xmalloc.c -- memory management with out of memory reporting
   Copyright (C) 2000 Gary V. Vaughan

   xmalloc.c -- Modified for viewglob's purposes.
   Copyright (C) 2004 Stephen Bach
  
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

/* The following is taken from the book GNU Autoconf, Automake, and
   Libtool by Gary V. Vaughan, Ben Elliston, Tom Tromey, and Ian
   Lance Taylor (with modification).  The website is here:
       http://sources.redhat.com/autobook/ */

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "viewglob-error.h"

void *
xmalloc (size_t num) {
  void *new = malloc (num);
  if (!new)
    viewglob_fatal ("Memory exhausted");
  return new;
}

void *
xrealloc (void *p, size_t num) {
  void *new;

  if (!p)
    return xmalloc (num);

  new = realloc (p, num);
  if (!new)
    viewglob_fatal ("Memory exhausted");

  return new;
}

/*	Don't need this.
void *
xcalloc (size_t num, size_t size) {
  void *new = xmalloc (num * size);
  bzero (new, num * size);
  return new;
}
*/

