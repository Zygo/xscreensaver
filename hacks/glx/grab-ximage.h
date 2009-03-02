/* grab-ximage.c --- grab the screen to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001, 2003, 2004 Jamie Zawinski <jwz@jwz.org>
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
XImage * screen_to_ximage (Screen *screen, Window window,
                           char **filename_return);

/* Like the above, but loads the image in the background and runs the
   given callback once it has been loaded.
 */
void fork_screen_to_ximage (Screen *screen, Window window,
                            void (*callback) (Screen *, Window, XImage *,
                                              const char *filename,
                                              void *closure,
                                              double cvt_time),
                            void *closure);

#endif /* __GRAB_XIMAGE_H__ */
