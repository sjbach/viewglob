/* `dir', `vdir' and `ls' directory listing programs for GNU.
   Copyright (C) 85, 88, 90, 91, 1995-2004 Free Software Foundation, Inc.

   lscolors.c -- Taken from ls.c and modified for viewglob's purposes.
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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Richard Stallman and David MacKenzie.  */

/* Color support by Peter Anvin <Peter.Anvin@linux.org> and Dennis
   Flaherty <dennisf@denix.elk.miles.com> based on original patches by
   Greg Lee <lee@uhunix.uhcc.hawaii.edu>.  */

#include "common.h"
#include "lscolors.h"
#include <string.h>     /* For strcpy(). */
#include <glib.h>

#define STREQ(a, b) (strcmp ((a), (b)) == 0)
#define LEN_STR_PAIR(s) sizeof (s) - 1, s


/* Null is a valid character in a color indicator (think about Epson
   printers, for example) so we have to use a length/buffer string
   type.  */
struct bin_str
  {
    size_t len;			/* Number of bytes */
    const char *string;		/* Pointer to the same */
  };

enum indicator_no
  {
    C_LEFT, C_RIGHT, C_END, C_NORM, C_FILE, C_DIR, C_LINK, C_FIFO, C_SOCK,
    C_BLK, C_CHR, C_MISSING, C_ORPHAN, C_EXEC, C_DOOR
  };

static const char *const indicator_name[]=
  {
    "lc", "rc", "ec", "no", "fi", "di", "ln", "pi", "so",
    "bd", "cd", "mi", "or", "ex", "do", NULL
  };

struct color_ext_type
  {
    struct bin_str ext;		/* The extension we're looking for */
    struct bin_str seq;		/* The sequence to output when we do */
    struct color_ext_type *next;	/* Next in list */
  };


/* Buffer for color sequences */
static char *color_buf;

static struct bin_str color_indicator[] =
  {
    { LEN_STR_PAIR ("\033[") },		/* lc: Left of color sequence */
    { LEN_STR_PAIR ("m") },		/* rc: Right of color sequence */
    { 0, NULL },			/* ec: End color (replaces lc+no+rc) */
    { LEN_STR_PAIR ("0") },		/* no: Normal */
    { LEN_STR_PAIR ("0") },		/* fi: File: default */
    { LEN_STR_PAIR ("01;34") },		/* di: Directory: bright blue */
    { LEN_STR_PAIR ("01;36") },		/* ln: Symlink: bright cyan */
    { LEN_STR_PAIR ("33") },		/* pi: Pipe: yellow/brown */
    { LEN_STR_PAIR ("01;35") },		/* so: Socket: bright magenta */
    { LEN_STR_PAIR ("01;33") },		/* bd: Block device: bright yellow */
    { LEN_STR_PAIR ("01;33") },		/* cd: Char device: bright yellow */
    { 0, NULL },			/* mi: Missing file: undefined */
    { 0, NULL },			/* or: Orphanned symlink: undefined */
    { LEN_STR_PAIR ("01;32") },		/* ex: Executable: bright green */
    { LEN_STR_PAIR ("01;35") }		/* do: Door: bright magenta */
  };


/* FIXME: comment  */
static struct color_ext_type *color_ext_list = NULL;

/* Nonzero means use colors to mark types.  Also define the different
   colors as well as the stuff for the LS_COLORS environment variable.
   The LS_COLORS variable is now in a termcap-like format.  */
static int print_with_color;

static gboolean
get_funky_string (char **dest, const char **src, gboolean equals_end,
		  size_t *output_count)
{
  int num;			/* For numerical codes */
  size_t count;			/* Something to count with */
  enum {
    ST_GND, ST_BACKSLASH, ST_OCTAL, ST_HEX, ST_CARET, ST_END, ST_ERROR
  } state;
  const char *p;
  char *q;

  p = *src;			/* We don't want to double-indirect */
  q = *dest;			/* the whole darn time.  */

  count = 0;			/* No characters counted in yet.  */
  num = 0;

  state = ST_GND;		/* Start in ground state.  */
  while (state < ST_END)
    {
      switch (state)
	{
	case ST_GND:		/* Ground state (no escapes) */
	  switch (*p)
	    {
	    case ':':
	    case '\0':
	      state = ST_END;	/* End of string */
	      break;
	    case '\\':
	      state = ST_BACKSLASH; /* Backslash scape sequence */
	      ++p;
	      break;
	    case '^':
	      state = ST_CARET; /* Caret escape */
	      ++p;
	      break;
	    case '=':
	      if (equals_end)
		{
		  state = ST_END; /* End */
		  break;
		}
	      /* else fall through */
	    default:
	      *(q++) = *(p++);
	      ++count;
	      break;
	    }
	  break;

	case ST_BACKSLASH:	/* Backslash escaped character */
	  switch (*p)
	    {
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	      state = ST_OCTAL;	/* Octal sequence */
	      num = *p - '0';
	      break;
	    case 'x':
	    case 'X':
	      state = ST_HEX;	/* Hex sequence */
	      num = 0;
	      break;
	    case 'a':		/* Bell */
	      num = 7;		/* Not all C compilers know what \a means */
	      break;
	    case 'b':		/* Backspace */
	      num = '\b';
	      break;
	    case 'e':		/* Escape */
	      num = 27;
	      break;
	    case 'f':		/* Form feed */
	      num = '\f';
	      break;
	    case 'n':		/* Newline */
	      num = '\n';
	      break;
	    case 'r':		/* Carriage return */
	      num = '\r';
	      break;
	    case 't':		/* Tab */
	      num = '\t';
	      break;
	    case 'v':		/* Vtab */
	      num = '\v';
	      break;
	    case '?':		/* Delete */
              num = 127;
	      break;
	    case '_':		/* Space */
	      num = ' ';
	      break;
	    case '\0':		/* End of string */
	      state = ST_ERROR;	/* Error! */
	      break;
	    default:		/* Escaped character like \ ^ : = */
	      num = *p;
	      break;
	    }
	  if (state == ST_BACKSLASH)
	    {
	      *(q++) = num;
	      ++count;
	      state = ST_GND;
	    }
	  ++p;
	  break;

	case ST_OCTAL:		/* Octal sequence */
	  if (*p < '0' || *p > '7')
	    {
	      *(q++) = num;
	      ++count;
	      state = ST_GND;
	    }
	  else
	    num = (num << 3) + (*(p++) - '0');
	  break;

	case ST_HEX:		/* Hex sequence */
	  switch (*p)
	    {
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
	      num = (num << 4) + (*(p++) - '0');
	      break;
	    case 'a':
	    case 'b':
	    case 'c':
	    case 'd':
	    case 'e':
	    case 'f':
	      num = (num << 4) + (*(p++) - 'a') + 10;
	      break;
	    case 'A':
	    case 'B':
	    case 'C':
	    case 'D':
	    case 'E':
	    case 'F':
	      num = (num << 4) + (*(p++) - 'A') + 10;
	      break;
	    default:
	      *(q++) = num;
	      ++count;
	      state = ST_GND;
	      break;
	    }
	  break;

	case ST_CARET:		/* Caret escape */
	  state = ST_GND;	/* Should be the next state... */
	  if (*p >= '@' && *p <= '~')
	    {
	      *(q++) = *(p++) & 037;
	      ++count;
	    }
	  else if (*p == '?')
	    {
	      *(q++) = 127;
	      ++count;
	    }
	  else
	    state = ST_ERROR;
	  break;

	default:
	  abort ();
	}
    }

  *dest = q;
  *src = p;
  *output_count = count;

  return state != ST_ERROR;
}

void
parse_ls_color (void)
{
  const char *p;		/* Pointer to character being parsed */
  char *buf;			/* color_buf buffer pointer */
  int state;			/* State of parser */
  int ind_no;			/* Indicator number */
  char label[3];		/* Indicator label */
  struct color_ext_type *ext;	/* Extension we are working on */

  if ((p = getenv ("LS_COLORS")) == NULL || *p == '\0')
    return;

  ext = NULL;
  strcpy (label, "??");

  /* This is an overly conservative estimate, but any possible
     LS_COLORS string will *not* generate a color_buf longer than
     itself, so it is a safe way of allocating a buffer in
     advance.  */
  buf = color_buf = g_strdup (p);

  state = 1;
  while (state > 0)
    {
      switch (state)
	{
	case 1:		/* First label character */
	  switch (*p)
	    {
	    case ':':
	      ++p;
	      break;

	    case '*':
	      /* Allocate new extension block and add to head of
		 linked list (this way a later definition will
		 override an earlier one, which can be useful for
		 having terminal-specific defs override global).  */

	      ext = g_new(struct color_ext_type, 1);
	      ext->next = color_ext_list;
	      color_ext_list = ext;

	      ++p;
	      ext->ext.string = buf;

	      state = (get_funky_string (&buf, &p, TRUE, &ext->ext.len)
		       ? 4 : -1);
	      g_printerr("$");
	      break;

	    case '\0':
	      state = 0;	/* Done! */
	      break;

	    default:	/* Assume it is file type label */
		g_printerr("!");
	      label[0] = *(p++);
	      state = 2;
	      break;
	    }
	  break;

	case 2:		/* Second label character */
	  if (*p)
	    {
	      label[1] = *(p++);
	      state = 3;
	    }
	  else
	    state = -1;	/* Error */
	  break;

	case 3:		/* Equal sign after indicator label */
	  state = -1;	/* Assume failure... */
	  if (*(p++) == '=')/* It *should* be... */
	    {
	      for (ind_no = 0; indicator_name[ind_no] != NULL; ++ind_no)
		{
		  if (STREQ (label, indicator_name[ind_no]))
		    {
		      color_indicator[ind_no].string = buf;
		      state = (get_funky_string (&buf, &p, FALSE,
						 &color_indicator[ind_no].len)
			       ? 1 : -1);
		      break;
		    }
		}
	      if (state == -1)
		g_printerr("gviewglob: Unrecognized prefix: \"%s\".\n", label);
		//error (0, 0, _("unrecognized prefix: %s"), quotearg (label));
	    }
	 break;

	case 4:		/* Equal sign after *.ext */
	  if (*(p++) == '=')
	    {
	      ext->seq.string = buf;
	      state = (get_funky_string (&buf, &p, FALSE, &ext->seq.len)
		       ? 1 : -1);
	    }
	  else
	    state = -1;
	  break;
	}
    }

  if (state < 0)
    {
      struct color_ext_type *e;
      struct color_ext_type *e2;

      g_printerr("gviewglob: Unparsable value for LS_COLORS environment variable.\n");
      //error (0, 0,
//	     _("unparsable value for LS_COLORS environment variable"));
      g_free (color_buf);
      for (e = color_ext_list; e != NULL; /* empty */)
	{
	  e2 = e;
	  e = e->next;
	  g_free (e2);
	}
      print_with_color = 0;
    }

  /*
  if (color_indicator[C_LINK].len == 6
      && !strncmp (color_indicator[C_LINK].string, "target", 6))
    color_symlink_as_referent = 1;
  */
}


