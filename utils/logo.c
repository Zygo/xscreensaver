/* xscreensaver, Copyright © 2001-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* XScreenSaver Logo designed by Angela Goodman <rzr_grl@yahoo.com>
 */

/* If you are looking in here because you're trying to figure out how to
   change the logo that xscreensaver displays on the splash screen and
   password dialog, please don't.  The logo is xscreensaver's identity.
   You wouldn't alter the name or copyright notice on a program that
   you didn't write; please don't alter its logo either.
 */

#include "utils.h"
#include "resources.h"
#include "minixpm.h"

#include <stdio.h>

#include "images/logo-50.xpm"
#include "images/logo-180.xpm"
#include "images/logo-360.xpm"

/* Returns a pixmap of the xscreensaver logo.
 */
Pixmap
xscreensaver_logo (Screen *screen, Visual *visual,
                   Drawable drawable, Colormap cmap,
                   unsigned long background_color,
                   unsigned long **pixels_ret, int *npixels_ret,
                   Pixmap *mask_ret,
                   int size)
{
  Display *dpy = DisplayOfScreen (screen);
  int x, y;
  unsigned int w, h, bw;
  Window root;
  XImage *image;
  Pixmap p = 0;
  unsigned char *mask = 0;
  unsigned int depth;
  XGCValues gcv;
  GC gc;

  XGetGeometry (dpy, drawable, &root, &x, &y, &w, &h, &bw, &depth);

  image = minixpm_to_ximage (dpy, visual, cmap, depth, background_color,
                             (size == 0 ? logo_50_xpm  :
                              size == 1 ? logo_180_xpm : logo_360_xpm),
                             &w, &h, pixels_ret, npixels_ret,
                             (mask_ret ? &mask : 0));
  if (! image) return 0;

  p = XCreatePixmap (dpy, drawable, w, h, depth);
  gc = XCreateGC (dpy, p, 0, &gcv);
  XPutImage (dpy, p, gc, image, 0, 0, 0, 0, w, h);
  XDestroyImage (image);
  XFreeGC (dpy, gc);

  if (mask_ret && mask)
    {
      *mask_ret = (Pixmap)
        XCreatePixmapFromBitmapData (dpy, drawable, (char *) mask,
                                     w, h, 1, 0, 1);
      free (mask);
    }

  return p;
}
