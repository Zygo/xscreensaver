/* glxfonts, Copyright (c) 2001-2012 Jamie Zawinski <jwz@jwz.org>
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

#ifndef HAVE_GLBITMAP
# include "texfont.h"
#endif /* !HAVE_GLBITMAP */

#ifdef HAVE_GLBITMAP
/* This is basically the same as glXUseXFont().
   We have our own version of it for portability.
 */
extern void xscreensaver_glXUseXFont (Display *dpy, Font font, 
                                      int first, int count, int listbase);
#endif /* HAVE_GLBITMAP */

/* Loads the font named by the X resource "res".
   Returns an XFontStruct.
   Also converts the font to a set of GL lists and returns the first list.
*/
extern void load_font (Display *, char *resource,
                       XFontStruct **font_ret,
                       GLuint *dlist_ret);

/* Bounding box of the string in pixels.
 */
extern int string_width (XFontStruct *f, const char *c, int *height_ret);

/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any text inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_font(). */
void print_gl_string (Display *dpy,
# ifdef HAVE_GLBITMAP
                      XFontStruct *font,
                      GLuint font_dlist,
# else  /* !HAVE_GLBITMAP */
                      texture_font_data *font_data,
# endif /* !HAVE_GLBITMAP */
                      int window_width, int window_height,
                      GLfloat x, GLfloat y,
                      const char *string,
                      Bool clear_background_p);

#endif /* __GLXFONTS_H__ */
