/* glxfonts, Copyright (c) 2001-2004 Jamie Zawinski <jwz@jwz.org>
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
 * Compute normal vectors for arbitrary triangles.
 */

#ifndef __GLXFONTS_H__
#define __GLXFONTS_H__

/* Loads the font named by the X resource "res".
   Returns an XFontStruct.
   Also converts the font to a set of GL lists and returns the first list.
*/
extern void load_font (Display *, char *resource,
                       XFontStruct **font_ret,
                       GLuint *dlist_ret);

/* Width of the string in pixels. */
extern int string_width (XFontStruct *f, const char *c);

/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any text inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_font(). */
void print_gl_string (Display *dpy,
                      XFontStruct *font,
                      GLuint font_dlist,
                      int window_width, int window_height,
                      GLfloat x, GLfloat y,
                      const char *string);

#endif /* __GLXFONTS_H__ */
