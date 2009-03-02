/* xscreensaver, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
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

/* I basically implemented libXPM here, because I don't want the
   xscreensaver daemon to depend on libXPM for two reasons: first,
   because I want the logo to show up even if libXPM is not installed
   on the system; and second, I don't want to have to security-audit
   libXPM.  The fewer libraries that are linked into the xscreensaver
   daemon, the more likely to be secure it is.
 */

/* If you are looking in here because you're trying to figure out how to
   change the logo that xscreensaver displays on the splash screen and
   password dialog, please don't.  The logo is xscreensaver's identity.
   You wouldn't alter the name or copyright notice on a program that
   you didn't write; please don't alter its logo either.
 */

#include "utils.h"
#include "resources.h"

#include <stdio.h>
#include <X11/Xutil.h>

#include "images/logo-50.xpm"
#include "images/logo-180.xpm"

static const char hex[128] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
                              0, 10,11,12,13,14,15,0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 10,11,12,13,14,15,0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static XImage *
parse_xpm_data (Display *dpy, Visual *visual, Colormap colormap, int depth,
                unsigned long transparent_color,
                unsigned const char * const * data,
                int *width_ret, int *height_ret,
                unsigned long **pixels_ret, int *npixels_ret,
                unsigned char **mask_ret)
{
  int w, w8, h, ncolors, nbytes;
  char c;
  int i;
  struct {
    char byte;
    int cr; int cg; int cb;
    int mr; int mg; int mb;
  } cmap[256];
  unsigned char rmap[256];

  unsigned long *pixels;
  XImage *ximage = 0;

  if (4 != sscanf ((const char *) *data,
                   "%d %d %d %d %c", &w, &h, &ncolors, &nbytes, &c))
    abort();
  if (ncolors < 1 || ncolors > 255)
    abort();
  if (nbytes != 1)
    abort();
  data++;

  w8 = (w + 8) / 8;

  if (mask_ret)
    {
      int s = (w8 * h) + 1;
      *mask_ret = (unsigned char *) malloc (s);
      if (!*mask_ret)
        mask_ret = 0;
      else
        memset (*mask_ret, 255, s);
    }

  for (i = 0; i < ncolors; i++)
    {
      const unsigned char *line = *data;
      cmap[i].byte = *line++;
      while (*line)
        {
          int r, g, b;
          char which;
          while (*line == ' ' || *line == '\t')
            line++;
          which = *line++;
          if (which != 'c' && which != 'm')
            abort();
          while (*line == ' ' || *line == '\t')
            line++;
          if (!strncasecmp(line, "None", 4))
            {
              r = g = b = -1;
              line += 4;
            }
          else
            {
              if (*line == '#')
                line++;
              r = (hex[(int) line[0]] << 4) | hex[(int) line[1]]; line += 2;
              g = (hex[(int) line[0]] << 4) | hex[(int) line[1]]; line += 2;
              b = (hex[(int) line[0]] << 4) | hex[(int) line[1]]; line += 2;
            }

          if (which == 'c')
            {
              cmap[i].cr = r;
              cmap[i].cg = g;
              cmap[i].cb = b;
            }
          else
            {
              cmap[i].mr = r;
              cmap[i].mg = g;
              cmap[i].mb = b;
            }
        }

      data++;
    }

  if (depth == 1) transparent_color = 1;

  pixels = (unsigned long *) calloc (ncolors+1, sizeof(*pixels));
  for (i = 0; i < ncolors; i++)
    {
      if (cmap[i].cr == -1) /* transparent */
        {
          rmap[(int) cmap[i].byte] = 255;
        }
      else
        {
          XColor color;
          color.flags = DoRed|DoGreen|DoBlue;
          color.red   = (cmap[i].cr << 8) | cmap[i].cr;
          color.green = (cmap[i].cg << 8) | cmap[i].cg;
          color.blue  = (cmap[i].cb << 8) | cmap[i].cb;
          if (depth == 1 ||
              !XAllocColor (dpy, colormap, &color))
            {
              color.pixel = (cmap[i].mr ? 1 : 0);
            }
          pixels[i] = color.pixel;
          rmap[(int) cmap[i].byte] = i;
        }
    }

  ximage = XCreateImage (dpy, visual, depth,
                         (depth == 1 ? XYBitmap : ZPixmap),
                         0, 0, w, h, 8, 0);
  if (ximage)
    {
      int x, y;
      ximage->data = (char *)
        calloc ( ximage->height, ximage->bytes_per_line);
      for (y = 0; y < h; y++)
        {
          const unsigned char *line = *data++;
          for (x = 0; x < w; x++)
            {
              int p = rmap[*line++];
              XPutPixel (ximage, x, y,
                         (p == 255 ? transparent_color : pixels[p]));

              if (p == 255 && mask_ret)
                (*mask_ret)[(y * w8) + (x >> 3)] &= (~(1 << (x % 8)));
            }
        }
    }
  
  *width_ret = w;
  *height_ret = h;
  *pixels_ret = pixels;
  *npixels_ret = ncolors;
  return ximage;
}



/* Draws the logo centered in the given Drawable (presumably a Pixmap.)
   next_frame_p means randomize the flame shape.
 */
Pixmap
xscreensaver_logo (Display *dpy, Window window, Colormap cmap,
                   unsigned long background_color,
                   unsigned long **pixels_ret, int *npixels_ret,
                   Pixmap *mask_ret,
                   Bool big_p)
{
  int npixels, iw, ih;
  unsigned long *pixels;
  XImage *image;
  Pixmap p = 0;
  XWindowAttributes xgwa;
  unsigned char *mask = 0;

  XGetWindowAttributes (dpy, window, &xgwa);

  image = parse_xpm_data (dpy, xgwa.visual, xgwa.colormap, xgwa.depth,
                          background_color,
                          (big_p ? logo_180_xpm : logo_50_xpm),
                          &iw, &ih, &pixels, &npixels,
                          (mask_ret ? &mask : 0));
  if (image)
    {
      XGCValues gcv;
      GC gc;
      p = XCreatePixmap (dpy, window, iw, ih, xgwa.depth);
      gc = XCreateGC (dpy, p, 0, &gcv);
      XPutImage (dpy, p, gc, image, 0, 0, 0, 0, iw, ih);
      XDestroyImage (image);
      XFreeGC (dpy, gc);

      if (mask_ret && mask)
        {
          *mask_ret = (Pixmap)
            XCreatePixmapFromBitmapData (dpy, window, (char *) mask,
                                         iw, ih, 1L, 0L, 1);
          free (mask);
        }
    }
  return p;
}
