/* xscreensaver, Copyright (c) 2001-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __COLORBARS_H__
#define __COLORBARS_H__

/* Renders colorbars (plus an xscreensaver logo) onto the
   given Window or Pixmap.  Width and height may be zero
   (meaning "full size".)

   Colors will be allocated from the cmap, and never freed.
 */
extern void draw_colorbars (Screen *, Visual *, Drawable, Colormap,
                            int x, int y, int width, int height,
                            Pixmap logo, Pixmap logo_mask);

#endif /* __COLORBARS_H__ */
