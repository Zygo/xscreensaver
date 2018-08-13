/* ximage-loader.h --- converts XPM data to Pixmaps.
 * xscreensaver, Copyright (c) 1998-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _XIMAGE_LOADER_H_
#define _XIMAGE_LOADER_H_

extern Pixmap file_to_pixmap (Display *, Window, const char *filename,
                              int *width_ret, int *height_ret,
                              Pixmap *mask_ret);

extern Pixmap image_data_to_pixmap (Display *, Window, 
                                    const unsigned char *image_data,
                                    unsigned long data_size,
                                    int *width_ret, int *height_ret,
                                    Pixmap *mask_ret);

/* This XImage has RGBA data, which is what OpenGL code typically expects.
   Also it is upside down: the origin is at the bottom left of the image.
   X11 typically expects 0RGB as it has no notion of alpha, only 1-bit masks.
   With X11 code, you should probably use the _pixmap routines instead.
 */
extern XImage *image_data_to_ximage (Display *, Visual *,
                                     const unsigned char *image_data,
                                     unsigned long data_size);

extern XImage *file_to_ximage (Display *, Visual *, const char *filename);

#endif /* _XIMAGE_LOADER_H_ */
