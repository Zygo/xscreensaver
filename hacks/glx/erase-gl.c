/* Copyright (c) 2017 Dave Odell <dmo2118@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * xlockmore.c has references to erase_window, but it never calls these when
 * running OpenGL hacks. Using this in place of regular utils/erase.c saves a
 * tiny bit of code/disk space with a native X11 build, where everything is
 * statically linked together.
 *
 * (Linux, amd64, CFLAGS='-O2 -g')
 * Before: -rwxr-xr-x 1 david david 545848 Aug  9 20:42 hilbert
 * After:  -rwxr-xr-x 1 david david 519344 Aug  9 20:41 hilbert
 *
 * (Linux, amd64, CFLAGS=-O2)
 * Before: -rwxr-xr-x 1 david david 150168 Aug  9 20:40 hilbert
 * After:  -rwxr-xr-x 1 david david 141256 Aug  9 20:39 hilbert
 */

#include "utils.h"
#include "erase.h"

void
eraser_free (eraser_state *st)
{
}


eraser_state *
erase_window (Display *dpy, Window window, eraser_state *st)
{
  return st;
}
