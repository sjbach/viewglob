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
#include "file_types.h"
#include "lscolors.h"
#include <string.h>     /* For strcpy(). */
#include <glib.h>
#include <pango/pango.h>

#define STREQ(a, b) (strcmp ((a), (b)) == 0)

TermTextAttr type_ttas[FILE_TYPES_NUM];
//TermTextAttr* ext_ttas = NULL;

/* Null is a valid character in a color indicator (think about Epson
   printers, for example) so we have to use a length/buffer string
   type.  */
struct bin_str {
	size_t len;				/* Number of bytes */
	const char *string;		/* Pointer to the same */
	GString* gstr;			/* Progress. */
};

/* Reorganized these to correspond with FileType in file_types.h. */
static const char *const indicator_name[]= {
	"fi", "ex", "di", "bd", "cd", "pi", "so", "ln", "lc",
    "rc", "ec", "no", "mi", "or", "do", NULL
};

struct color_ext_type
  {
    struct bin_str ext;		/* The extension we're looking for. */
    struct bin_str seq;		/* The sequence to output when we do. */
	TermTextAttr tta;       /* The sequence in TermTextAttr form. */
    struct color_ext_type *next;	/* Next in list */
  };


static void create_termtextattrs(void);
static void create_pangoattrlists(void);
static void termtextattr_init(TermTextAttr* tta);
static void termtextattr_copy(TermTextAttr* dest, TermTextAttr* src);
static void termtextattr_check_reverse(TermTextAttr* tta);
static gboolean are_equal(TermTextAttr* a, TermTextAttr* b);
static TermTextAttr* scan_types_for_equivalency(TermTextAttr* tta);
static TermTextAttr* scan_exts_for_equivalency(TermTextAttr* tta);
static void parse_codes(struct bin_str* s, TermTextAttr* attr);
static PangoAttrList* create_pango_list(TermTextAttr* tta);


/* Buffer for color sequences */
static char *color_buf;

/* Reorganized to correspond with FileType in file_types.h. */
#define LEN_STR_PAIR(s) sizeof (s) - 1, s, NULL
#define COLOR_INDICATOR_SIZE 15
static struct bin_str color_indicator[] = {
	{ LEN_STR_PAIR ("0") },         /* fi: File: default */
	{ LEN_STR_PAIR ("01;32") },     /* ex: Executable: bright green */
	{ LEN_STR_PAIR ("01;34") },     /* di: Directory: bright blue */
	{ LEN_STR_PAIR ("01;33") },     /* bd: Block device: bright yellow */
	{ LEN_STR_PAIR ("01;33") },     /* cd: Char device: bright yellow */
	{ LEN_STR_PAIR ("33") },        /* pi: Pipe: yellow/brown */
	{ LEN_STR_PAIR ("01;35") },     /* so: Socket: bright magenta */
	{ LEN_STR_PAIR ("01;36") },     /* ln: Symlink: bright cyan */
	{ LEN_STR_PAIR ("\033[") },     /* lc: Left of color sequence */
	{ LEN_STR_PAIR ("m") },         /* rc: Right of color sequence */
	{ 0, NULL },                    /* ec: End color (replaces lc+no+rc) */
	{ LEN_STR_PAIR ("0") },         /* no: Normal */
	{ 0, NULL },                    /* mi: Missing file: undefined */
	{ 0, NULL },                    /* or: Orphanned symlink: undefined */
	{ LEN_STR_PAIR ("01;35") }      /* do: Door: bright magenta */
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
parse_ls_colors (void)
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
	      break;

	    case '\0':
	      state = 0;	/* Done! */
	      break;

	    default:	/* Assume it is file type label */
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
		g_printerr("gviewglob: Unrecognized LS_COLORS prefix: \"%s\".\n", label);
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
      g_free (color_buf);
      for (e = color_ext_list; e != NULL; /* empty */)
	{
	  e2 = e;
	  e = e->next;
	  g_free (e2);
	}
      print_with_color = 0;
    }

  /* Dealing with bin_str is very tiresome.  Convert
	 to useable GStrings. */
  int i;
  struct color_ext_type* iter;
  for (i = 0; i < COLOR_INDICATOR_SIZE; i++) {
	  color_indicator[i].gstr = g_string_new_len(
			  color_indicator[i].string,
			  color_indicator[i].len);
  }
  for (iter = color_ext_list; iter; iter = iter->next) {
	  iter->ext.gstr = g_string_new_len(iter->ext.string, iter->ext.len);
	  iter->seq.gstr = g_string_new_len(iter->seq.string, iter->seq.len);
  }

  create_termtextattrs();
  create_pangoattrlists();
}


/* Create TermTextAttrs from the terminal colouring sequences. */
static void create_termtextattrs(void) {

	TermTextAttr normal;
	int i;

	/* Initialize "no", just in case it's not included
	   in LS_COLORS. */
	termtextattr_init(&normal);

	parse_codes(&color_indicator[11], &normal);  /* "no" */

	/* All file types inherit from "no". */
	for (i = 0; i < FILE_TYPES_NUM; i++) {
		termtextattr_copy(&type_ttas[i], &normal);
		parse_codes(&color_indicator[i], &type_ttas[i]);
	}

	/* Now do the extensions. */
	struct color_ext_type* iter;
	for (iter = color_ext_list; iter; iter = iter->next) {
		/* Extensions only apply to regular files, apparently. */
		termtextattr_copy(&iter->tta, &type_ttas[FT_REGULAR]);
		parse_codes(&iter->seq, &iter->tta);
		termtextattr_check_reverse(&iter->tta);
	}

	/* It's safe to reverse the file types now (if necessary). */
	for (i = 0; i < FILE_TYPES_NUM; i++)
		termtextattr_check_reverse(&type_ttas[i]);
}


/* Create PangoAttrLists from the TermTextAttrs. */
static void create_pangoattrlists(void) {
	TermTextAttr* match;
	int i;

	for (i = 0; i < FILE_TYPES_NUM; i++) {
		/* It's safe to reverse the file types now (if necessary). */
		termtextattr_check_reverse(&type_ttas[i]);
		match = scan_types_for_equivalency(&type_ttas[i]);
		if (match)
			type_ttas[i].p_list = match->p_list;
		else
			type_ttas[i].p_list = create_pango_list(&type_ttas[i]);
	}
	for (iter = color_ext_list; iter; iter = iter->next) {
		match = scan_exts_for_equivalency(&iter->tta);
		if (match)
			iter->tta.p_list = match->p_list;
		else
			iter->tta.p_list = create_pango_list(&iter->tta);
	}
}

/* If the given TermTextAttr has TAC_REVERSE, reverse its colors. */
static void termtextattr_check_reverse(TermTextAttr* tta) {

	if (tta->attr & TAC_REVERSE) {
		/* Switch foreground with background. */
		enum term_color_code temp;
		temp = tta->fg;
		tta->fg = tta->bg;
		tta->bg = temp;

		/* Remove TAC_REVERSE now that it's been applied. */
		tta->attr &= (~TAC_REVERSE);
	}
}


/* Parse the code sequences in s and convert to a TermTextAttr. */
static void parse_codes(struct bin_str* s, TermTextAttr* tta) {

	ptrdiff_t pos = 0;
	char* endptr = NULL;
	long code;

	while (pos < s->len) {
		code = strtol(s->gstr->str + pos, &endptr, 10);
		if (s->gstr->str + pos == endptr) {
			/* Did not convert any characters. */
			if (pos + 1 == s->len) {
				/* We're at the end of the string. */
				break;
			}
			else {
				/* It's just a separator or something.  Skip it. */
				pos++;
			}
		}
		else {
			/* Got a code -- let's see what it is. */
			pos = endptr - s->gstr->str;
			switch (code) {
				case 0:
					tta->attr = 0;
					break;
				case 1:
					tta->attr |= TAC_BOLD;
					break;
				case 4:
					tta->attr |= TAC_UNDERSCORE;
					break;
				case 7:
					tta->attr |= TAC_REVERSE;
					break;
				case 30:
					tta->fg = TCC_BLACK;
					break;
				case 31:
					tta->fg = TCC_RED;
					break;
				case 32:
					tta->fg = TCC_GREEN;
					break;
				case 33:
					tta->fg = TCC_YELLOW;
					break;
				case 34:
					tta->fg = TCC_BLUE;
					break;
				case 35:
					tta->fg = TCC_MAGENTA;
					break;
				case 36:
					tta->fg = TCC_CYAN;
					break;
				case 37:
					tta->fg = TCC_WHITE;
					break;
				case 40:
					tta->bg = TCC_BLACK;
					break;
				case 41:
					tta->bg = TCC_RED;
					break;
				case 42:
					tta->bg = TCC_GREEN;
					break;
				case 43:
					tta->bg = TCC_YELLOW;
					break;
				case 44:
					tta->bg = TCC_BLUE;
					break;
				case 45:
					tta->bg = TCC_MAGENTA;
					break;
				case 46:
					tta->bg = TCC_CYAN;
					break;
				case 47:
					tta->bg = TCC_WHITE;
					break;
			}
		}
	}
}


/* Initialize the given TermTextAttr. */
static void termtextattr_init(TermTextAttr* tta) {
	tta->fg = TCC_NONE;
	tta->bg = TCC_NONE;
	tta->attr = 0;
	tta->p_list = NULL;
}


/* Copy the attribute fields in src to the fields in dest. */
static void termtextattr_copy(TermTextAttr* dest, TermTextAttr* src) {
	dest->fg = src->fg;
	dest->bg = src->bg;
	dest->attr = src->attr;
	dest->p_list = NULL;     /* Don't copy this one. */
}


/* Scan through the type_ttas array for a matching tta. */
static TermTextAttr* scan_types_for_equivalency(TermTextAttr* tta) {
	int i;
	TermTextAttr* retval = NULL;

	for (i = 0; i < FILE_TYPES_NUM; i++) {
		if (tta == &type_ttas[i]) {
			/* We've reached ourselves, so we're done. */
			break;
		}
		if (are_equal(tta, &type_ttas[i])) {
			retval = &type_ttas[i];
			break;
		}
	}
	return retval;
}


/* Scan throught the color_ext_list list for a matching tta. */
static TermTextAttr* scan_exts_for_equivalency(TermTextAttr* tta) {
	struct color_ext_type* iter;
	TermTextAttr* retval = NULL;

	for (iter = color_ext_list; iter; iter = iter->next) {
		if (&iter->tta == tta) {
			/* We've reached ourselves, so we're done. */
			break;
		}
		else if (are_equal(tta, &iter->tta)) {
			retval = &iter->tta;
			break;
		}
	}
	return retval;
}


/* Compares the given TermTextAttrs for equivalency. */
static gboolean are_equal(TermTextAttr* a, TermTextAttr* b) {
	gboolean is_same = TRUE;
	is_same &= a->fg == b->fg;
	is_same &= a->bg == b->bg;
	is_same &= a->attr == b->attr;

	return is_same;
}


/* Convert the given TermTextAttr into a PangoAttrList. */
static PangoAttrList* create_pango_list(TermTextAttr* tta) {

	PangoAttribute* p_attr;
	PangoAttrList* p_list = pango_attr_list_new();
	gboolean list_set = FALSE;

	static struct color_mapping {
		guint16 r;
		guint16 g;
		guint16 b;
	} map[] = {
		{ 0x0000, 0x0000, 0x0000 },	/* TCC_NONE (not used) */
		{ 0x0000, 0x0000, 0x0000 },	/* TCC_BLACK */
		{ 0x9e88, 0x1888, 0x2888 },	/* TCC_RED */
		{ 0xae88, 0xce88, 0x9188 }, /* TCC_GREEN */
		{ 0xff88, 0xf788, 0x9688 }, /* TCC_YELLOW */
		{ 0x4188, 0x8688, 0xbe88 }, /* TCC_BLUE */
		{ 0x9688, 0x3c88, 0x5988 }, /* TCC_MAGENTA */
		{ 0x7188, 0xbe88, 0xbe88 }, /* TCC_CYAN */
		{ 0xffff, 0xffff, 0xffff }, /* TCC_WHITE */
	};

	/* Foreground colour */
	if (tta->fg > TCC_NONE && tta->fg <= TCC_WHITE) {
		p_attr = pango_attr_foreground_new(
				map[tta->fg].r,
				map[tta->fg].g,
				map[tta->fg].b);
		p_attr->start_index = 0;
		p_attr->end_index = G_MAXINT;
		pango_attr_list_insert(p_list, p_attr);
		list_set = TRUE;
	}

	/* Background colour */
	if (tta->bg > TCC_NONE && tta->bg <= TCC_WHITE) {
		p_attr = pango_attr_background_new(
				map[tta->bg].r,
				map[tta->bg].g,
				map[tta->bg].b);
		p_attr->start_index = 0;
		p_attr->end_index = G_MAXINT;
		pango_attr_list_insert(p_list, p_attr);
		list_set = TRUE;
	}


	/* Bold */
	if (tta->attr & TAC_BOLD) {
		p_attr = pango_attr_weight_new(PANGO_WEIGHT_HEAVY);
		p_attr->start_index = 0;
		p_attr->end_index = G_MAXINT;
		pango_attr_list_insert(p_list, p_attr);
		list_set = TRUE;
	}

	/* Underscore */
	if (tta->attr & TAC_UNDERSCORE) {
		p_attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
		p_attr->start_index = 0;
		p_attr->end_index = G_MAXINT;
		pango_attr_list_insert(p_list, p_attr);
		list_set = TRUE;
	}

	if (list_set) {
		/* Probably don't need to do this, but it doesn't hurt since
		   the attribute lists are valid through the life of the program. */
		pango_attr_list_ref(p_list);
	}
	else {
		pango_attr_list_unref(p_list);
		p_list = NULL;
	}

	return p_list;
}


/* Get a PangoAttrList for this label, based on its name and type.  */
void label_set_attributes(gchar* name, FileType type, GtkLabel* label) {

	PangoAttrList* p_list = type_ttas[type].p_list;
	if (type == FT_REGULAR) {
		struct color_ext_type* iter;
		for (iter = color_ext_list; iter; iter = iter->next) {
			if (g_str_has_suffix(name, iter->ext.gstr->str)) {
				p_list = iter->tta.p_list;
				break;
		}
	}


	}
	if (p_list)
		gtk_label_set_attributes(label, p_list);
}

