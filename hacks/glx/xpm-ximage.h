/* xpm-ximage.h --- converts XPM data to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 1998, 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _XPM_TEXTURE_H_
#define _XPM_TEXTURE_H_

/* Returns an XImage structure containing the bits of the given XPM image.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.

   The Display and Visual arguments are used only for creating the XImage;
   no bits are pushed to the server.

   The Colormap argument is used just for parsing color names; no colors
   are allocated.
 */
extern XImage *xpm_to_ximage (Display *, Visual *, Colormap, char **xpm_data);
extern XImage *xpm_file_to_ximage (Display *, Visual *, Colormap,
                                   const char *filename);

#endif /* _XPM_TEXTURE_H_ */
