/* glxfonts, Copyright (c) 2001-2014 Jamie Zawinski <jwz@jwz.org>
 * Loads X11 fonts for use with OpenGL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Loads X11 fonts for use with OpenGL.
 */

#ifndef __GLXFONTS_H__
#define __GLXFONTS_H__

#include "texfont.h"

/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().

   If width and height are 0, then instead the text is placed
   into the 3D scene at the origin, billboarded to face the
   viewer.
 */
void print_gl_string (Display *dpy,
                      texture_font_data *font,
                      int window_width, int window_height,
                      GLfloat x, GLfloat y,
                      const char *string,
                      Bool clear_background_p);

#endif /* __GLXFONTS_H__ */
