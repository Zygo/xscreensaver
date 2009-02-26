/*
 * (c) 2007, Quest Software, Inc. All rights reserved.
 *
 * This file is part of XScreenSaver,
 * Copyright (c) 1993-2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <stdlib.h>
#include <ctype.h>

#include <X11/Xlib.h>

#include "mlstring.h"

#define LINE_SPACING 1.2

static mlstring *
mlstring_allocate(const char *msg);

static void
mlstring_calculate(mlstring *str, XFontStruct *font);

mlstring*
mlstring_new(const char *msg, XFontStruct *font, Dimension wrap_width)
{
  mlstring *newstr;

  if (!(newstr = mlstring_allocate(msg)))
    return NULL;

  newstr->font_id = font->fid;

  mlstring_wrap(newstr, font, wrap_width);

  return newstr;
}

mlstring *
mlstring_allocate(const char *msg)
{
  const char *s;
  mlstring *ml;
  struct mlstr_line *cur, *prev = NULL;
  size_t linelength;
  int the_end = 0;

  if (!msg)
    return NULL;

  ml = calloc(1, sizeof(mlstring));

  if (!ml)
    return NULL;

  for (s = msg; !the_end; msg = ++s)
    {
      /* New string struct */
      cur = calloc(1, sizeof(struct mlstr_line));
      if (!cur)
	goto fail;

      if (!ml->lines)
	ml->lines = cur;

      /* Find the \n or end of string */
      while (*s != '\n')
	{
	  if (*s == '\0')
	    {
	      the_end = 1;
	      break;
	    }

	  ++s;
	}

      linelength = s - msg;

      /* Duplicate the string */
      cur->line = malloc(linelength + 1);
      if (!cur->line)
	goto fail;

      strncpy(cur->line, msg, linelength);
      cur->line[linelength] = '\0';

      if (prev)
	prev->next_line = cur;
      prev = cur;
    }

  return ml;

fail:

  if (ml)
    mlstring_free(ml);

  return NULL;
}


/*
 * Frees an mlstring.
 * This function does not have any unit tests.
 */
void
mlstring_free(mlstring *str) {
  struct mlstr_line *cur, *next;

  for (cur = str->lines; cur; cur = next) {
    next = cur->next_line;
    free(cur->line);
    free(cur);
  }

  free(str);
}


void
mlstring_wrap(mlstring *mstring, XFontStruct *font, Dimension width)
{
  short char_width = font->max_bounds.width;
  int line_length, wrap_at;
  struct mlstr_line *mstr, *newml;

  /* An alternative implementation of this function would be to keep trying
   * XTextWidth() on space-delimited substrings until the longest one less
   * than 'width' is found, however there shouldn't be much difference
   * between that, and this implementation.
   */

  for (mstr = mstring->lines; mstr; mstr = mstr->next_line)
    {
      if (XTextWidth(font, mstr->line, strlen(mstr->line)) > width)
        {
	  /* Wrap it */
	  line_length = width / char_width;
	  if (line_length == 0)
	    line_length = 1;
	  
	  /* First try to soft wrap by finding a space */
	  for (wrap_at = line_length; wrap_at >= 0 && !isspace(mstr->line[wrap_at]); --wrap_at);
	  
	  if (wrap_at == -1) /* No space found, hard wrap */
	    wrap_at = line_length;

	  newml = calloc(1, sizeof(*newml));
	  if (!newml) /* OOM, don't bother trying to wrap */
	    break;

	  if (NULL == (newml->line = strdup(mstr->line + wrap_at)))
	    {
	      /* OOM, jump ship */
	      free(newml);
	      break;
	    }
	
	  /* Terminate the existing string at its end */
	  mstr->line[wrap_at] = '\0';

	  newml->next_line = mstr->next_line;
	  mstr->next_line = newml;
	}
    }

  mlstring_calculate(mstring, font);
}

#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/*
 * Calculates the overall extents (width + height of the multi-line string).
 * This function is called as part of mlstring_new().
 * It does not have any unit testing.
 */
void
mlstring_calculate(mlstring *str, XFontStruct *font) {
  struct mlstr_line *line;

  str->font_height = font->ascent + font->descent;
  str->overall_height = 0;
  str->overall_width = 0;

  /* XXX: Should there be some baseline calculations to help XDrawString later on? */
  str->font_ascent = font->ascent;

  for (line = str->lines; line; line = line->next_line)
    {
      line->line_width = XTextWidth(font, line->line, strlen(line->line));
      str->overall_width = MAX(str->overall_width, line->line_width);
      /* Don't add line spacing for the first line */
      str->overall_height += (font->ascent + font->descent) *
			     (line == str->lines ? 1 : LINE_SPACING);
    }
}

void
mlstring_draw(Display *dpy, Drawable dialog, GC gc, mlstring *string, int x, int y) {
  struct mlstr_line *line;

  if (!string)
    return;
  
  y += string->font_ascent;

  XSetFont(dpy, gc, string->font_id);

  for (line = string->lines; line; line = line->next_line)
    {
      XDrawString(dpy, dialog, gc, x, y, line->line, strlen(line->line));
      y += string->font_height * LINE_SPACING;
    }
}

/* vim:ts=8:sw=2:noet
 */
