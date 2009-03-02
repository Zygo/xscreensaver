/* grab-ximage.c --- grab the screen to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001, 2003 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __GRAB_XIMAGE_H__
#define __GRAB_XIMAGE_H__

/* Returns an XImage structure containing an image of the desktop.
   (As a side-effect, that image *may* be painted onto the given Window.)
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.
 */
XImage * screen_to_ximage (Screen *screen, Window window);

#endif /* __GRAB_XIMAGE_H__ */
