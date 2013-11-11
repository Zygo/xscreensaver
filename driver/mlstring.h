/* mlstring.h --- Multi-line strings for use with Xlib
 *
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
#ifndef MLSTRING_H
#define MLSTRING_H

#include <X11/Intrinsic.h>

/* mlstring means multi-line string */

struct mlstr_line;

typedef struct mlstring mlstring;
struct mlstring {
  struct mlstr_line *lines; /* linked list */
  Dimension overall_height;
  Dimension overall_width;
  /* XXX: Perhaps it is simpler to keep a reference to the XFontStruct */
  int font_ascent;
  int font_height;
  Font font_id;
};

struct mlstr_line {
  char *line;
  Dimension line_width;
  struct mlstr_line *next_line;
};

mlstring *
mlstring_new(const char *str, XFontStruct *font, Dimension wrap_width);

/* Does not have to be called manually */
void
mlstring_wrap(mlstring *mstr, XFontStruct *font, Dimension width);

void
mlstring_free(mlstring *str);

void
mlstring_draw(Display *dpy, Drawable dialog, GC gc, mlstring *string, int x, int y);

#endif
/* vim:ts=8:sw=2:noet
 */
