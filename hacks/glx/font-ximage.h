/* font-ximage.c --- renders text to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __FONT_XIMAGE_H__
# define __FONT_XIMAGE_H__

/* Returns an XImage structure containing the string rendered in the font.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.

   Foregroune and background are GL-style color specifiers: 4 floats from
   0.0-1.0.
 */
XImage *text_to_ximage (Screen *screen, Visual *visual,
                        const char *font,
                        const char *text_lines,
                        GLfloat *texture_fg,
                        GLfloat *texture_bg);


#endif /* __FONT_XIMAGE_H__ */
