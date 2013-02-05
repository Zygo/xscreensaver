/* xscreensaver, Copyright (c) 2001-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __MINIXPM_H__
#define __MINIXPM_H__


/* A dead simple XPM parser that knows how to make XImage structures.
   Only handles single-byte color XPMs.
 */

extern XImage * minixpm_to_ximage (Display *, Visual *, Colormap, int depth,
                                   unsigned long transparent_color,
                                   const char * const * data,
                                   int *width_ret, int *height_ret,
                                   unsigned long **pixels_ret, 
                                   int *npixels_ret,
                                   unsigned char **mask_ret);

#endif /* __MINIXPM_H__ */
