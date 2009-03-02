/* xpm-pixmap.h --- converts XPM data to Pixmaps.
 * xscreensaver, Copyright (c) 1998-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _XPM_PIXMAP_H_
#define _XPM_PIXMAP_H_

/* Returns a Pixmap structure containing the bits of the given XPM image.
 */
extern Pixmap xpm_data_to_pixmap (Display *, Window, 
                                  /*const*/ char * const *xpm_data,
                                  int *width_ret, int *height_ret,
                                  Pixmap *mask_ret);
extern Pixmap xpm_file_to_pixmap (Display *, Window, const char *filename,
                                  int *width_ret, int *height_ret,
                                  Pixmap *mask_ret);

#endif /* _XPM_PIXMAP_H_ */
