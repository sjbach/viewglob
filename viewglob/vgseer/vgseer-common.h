/* 
   common.h -- Process this file with configure to produce common.h
   Copyright (C) 2000 Gary V. Vaughan

   common.h -- Modified for viewglob's purposes.
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

#ifndef COMMON_H
#define COMMON_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define DEBUG_ON 0

#if HAVE_STDBOOL_H
#  include <stdbool.h>
#else
#  if ! HAVE__BOOL
#    ifdef __cplusplus
typedef bool _Bool;
#    else
typedef unsigned char _Bool;
#    endif
#  endif
#  define bool _Bool
#  define false 0
#  define true 1
#  define __bool_true_false_are_defined 1
#endif


#if HAVE_ERRNO_H
#  include <errno.h>
#endif /*HAVE_ERRNO_H*/
#ifndef errno
extern int errno;
#endif

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */


#ifndef EXIT_SUCCESS
#  define EXIT_SUCCESS  0
#  define EXIT_FAILURE  1
#endif

#define PERM_FILE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/*
#define XCALLOC(type, num)                                  \
        ((type *) xcalloc ((num), sizeof(type)))
*/
#define XMALLOC(type, num)                                  \
        ((type *) xmalloc ((num) * sizeof(type)))
#define XREALLOC(type, p, num)                              \
        ((type *) xrealloc ((p), (num) * sizeof(type)))
#define XFREE(stale)                            do {        \
        if (stale) { free (stale);  stale = 0; }            \
                                                } while (0)

#if DEBUG_ON
#define DEBUG(blah)	do {					\
						fprintf blah;	\
						fflush(df);			\
					} while (0)
#else
#define DEBUG(blah)	do { } while(0)
#endif

#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

/* extern void *xcalloc    (size_t num, size_t size); */
extern void *xmalloc    (size_t num);
extern void *xrealloc   (void *p, size_t num);

#endif /* !COMMON_H */
