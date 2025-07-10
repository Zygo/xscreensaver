/* xscreensaver, Copyright © 2001-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for drawing NTSC colorbars.
   A couple of things use this.
 */

#include "utils.h"
#include "resources.h"
#include "colorbars.h"
#include "../hacks/ximage-loader.h"

#define LGBTQIA
#ifndef LGBTQIA /* SMPTE ECR-1-1978 */
static const char * const colors[7][18] = {
  { "#CCCCCC", "#FFFF00", "#00FFFF", "#00FF00",		/* tall bars */
    "#FF00FF", "#FF0000", "#0000FF", "#000000",
    0
  }, {
    "#000000", "#0000FF", "#FF0000", "#FF00FF",		/* short rev bars */
    "#00FF00", "#00FFFF", "#FFFF00", "#CCCCCC",
    0
  }, {
    "#000000", 0					/* blank */
  }, {
    "#FFFFFF", "#EEEEEE", "#DDDDDD", "#CCCCCC",		/* gray ramp */
    "#BBBBBB", "#AAAAAA", "#999999", "#888888",
    "#777777", "#666666", "#555555", "#444444",
    "#333333", "#222222", "#111111", "#000000"
  }, {
    "#000000", "#111111", "#222222", "#333333",		/* gray rev ramp */
    "#444444", "#555555", "#666666", "#777777",
    "#888888", "#999999", "#AAAAAA", "#BBBBBB",
    "#CCCCCC", "#DDDDDD", "#EEEEEE", "#FFFFFF"
  }, {
    "#000000", 0					/* blank */
  }, {
    "#FF00FF", "#FF00FF", "#FF00FF",			/* blacklevel row */
    "#FFFFFF", "#FFFFFF", "#FFFFFF",
    "#0000AD", "#0000AD", "#0000AD",
    "#131313", "#131313", "#131313",
    "#000000", "#131313", "#262626",			/* ramp */
    "#000000", "#000000", "#000000"
  }
};

#else /* LGBTQIA pride flag colorbars designed by
         Cable Contributes to Life <https://bsky.app/profile/cctl.me> --
         https://web.archive.org/web/20250701065803/https://bsky.app/profile/cctl.me/post/3lsthzde3h22g
         "The first set of columns use the colors of the gay pride flag,
         with the addition of the original SMPTE color on the final column.
         The second set of columns contains two pride flags: the pansexual
         flag horizontally, and the asexual flag flipped and horizontal.
         The final set of columns contains the three colors of the
         transgender flag and the two POC-inclusive colors, followed by
         the original bar gradient from the SMPTE ECR-1-1978 pattern."
       */

static const char * const colors[7][18] = {
  { "#E40303", "#FF8C00", "#FFED00", "#008026",		/* tall bars, pride */
    "#004CFF", "#732982", "#0000FF",
    0
  }, {
    "#FF218C", "#FFD800", "#21B1FF", "#800080",		/* short bars, pan a */
    "#FFFFFF", "#A3A3A3", "#000000",
    0
  }, {
    "#000000", 0					/* blank */
  }, {
    "#FFFFFF", "#EEEEEE", "#DDDDDD", "#CCCCCC",		/* gray ramp */
    "#BBBBBB", "#AAAAAA", "#999999", "#888888",
    "#777777", "#666666", "#555555", "#444444",
    "#333333", "#222222", "#111111", "#000000"
  }, {
    "#000000", "#111111", "#222222", "#333333",		/* gray rev ramp */
    "#444444", "#555555", "#666666", "#777777",
    "#888888", "#999999", "#AAAAAA", "#BBBBBB",
    "#CCCCCC", "#DDDDDD", "#EEEEEE", "#FFFFFF"
  }, {
    "#000000", 0					/* blank */
  }, {
    "#5BCEFA", "#5BCEFA", "#5BCEFA",			/* trans poc */
    "#F5A9B8", "#F5A9B8", "#F5A9B8",
    "#FFFFFF", "#FFFFFF", "#FFFFFF",
    "#613704", "#613704", "#613704",
    "#000000", "#131313", "#262626",			/* ramp */
    "#000000", "#000000", "#000000"
  }
};
#endif /* LGBTQIA */


static const int heights[7] = { 63, 10, 1, 5, 5, 1, 15 };   /* percentages */


void
draw_colorbars (Screen *screen, Visual *visual,
                Drawable drawable, Colormap cmap,
                int x, int y, int width, int height,
                Pixmap logo, Pixmap logo_mask)
{
  Display *dpy = DisplayOfScreen (screen);
  int ypct = 0;
  int j;
  XGCValues gcv;
  GC gc = XCreateGC (dpy, drawable, 0, &gcv);

  if (width <= 0 || height <= 0)
    {
      Window root;
      int xx, yy;
      unsigned int bw, d;
      XGetGeometry (dpy, drawable,
                    &root, &xx, &yy,
                    (unsigned int *) &width,
                    (unsigned int *) &height,
                    &bw, &d);
    }

  for (j = 0; j < sizeof(colors) / sizeof(*colors); j++)
    {
      int i, h, ncols;
      int x1 = 0;
      int y2;
      for (ncols = 0; ncols < sizeof(*colors) / sizeof(**colors); ncols++)
        if (!colors[j][ncols]) break;
      ypct += heights[j];
      y2 = height * ypct / 100;
      h = y2 - y; /* avoid roundoff fencepost */
      for (i = 0; i < ncols; i++)
        {
          XColor xcolor;
          const char *color = colors[j][i];
          int x2 = x + (width * (i+1) / ncols);
          int w = x2 - x1; /* avoid roundoff fencepost */
          if (! XParseColor (dpy, cmap, color, &xcolor))
            abort();
          xcolor.flags = DoRed|DoGreen|DoBlue;
          if (!XAllocColor (dpy, cmap, &xcolor))
            continue;
          XSetForeground (dpy, gc, xcolor.pixel);
          XFillRectangle (dpy, drawable, gc, x1, y, w, h);
          x1 = x2;
	}
      y = y2;
    }

  /* Add in the xscreensaver logo */
  if (logo)
    {
      Window r;
      int x, y;
      unsigned int logo_width, logo_height, bw, d;
      int x1, y1, w, h;
      XGetGeometry (dpy, logo, &r, &x, &y, &logo_width, &logo_height, &bw, &d);
      w = width;
      h = height * heights[0] / 100;
      x1 = x + (w - (int) logo_width) / 2;
      y1 = y + (h - (int) logo_height) / 2;
      if (logo_mask)
        {
          XSetClipMask (dpy, gc, logo_mask);
          XSetClipOrigin (dpy, gc, x1, y1);
        }
      XCopyArea (dpy, logo, drawable, gc, 0, 0,
                 logo_width, logo_height, x1, y1);
    }

  XFreeGC(dpy, gc);
}
