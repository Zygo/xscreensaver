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

/* I implemented this subset of libXPM here because I don't want the
   xscreensaver daemon to depend on libXPM for two reasons: first,
   because I want the logo to show up even if libXPM is not installed
   on the system; and second, I don't want to have to security-audit
   libXPM.  The fewer libraries that are linked into the xscreensaver
   daemon, the more likely to be secure it is.

   Also, the Cocoa port uses this code since libXPM isn't available
   by default on MacOS.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else  /* !HAVE_COCOA - real Xlib */
# include <X11/Xlib.h>
# include <X11/Xutil.h>
#endif /* !HAVE_COCOA */

#include "minixpm.h"

extern const char *progname;

static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}

static const char hex[128] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
                              0, 10,11,12,13,14,15,0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 10,11,12,13,14,15,0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

XImage *
minixpm_to_ximage (Display *dpy, Visual *visual, Colormap colormap, int depth,
                   unsigned long transparent_color,
                   const char * const * data,
                   int *width_ret, int *height_ret,
                   unsigned long **pixels_ret, int *npixels_ret,
                   unsigned char **mask_ret)
{
  int w, w8, h, ncolors, nbytes;
  char c;
  int x, y, i, pixel_count;
  struct {
    char byte;
    int cr; int cg; int cb;
    int mr; int mg; int mb;
  } cmap[256];
  unsigned char rmap[256];

  unsigned long *pixels;
  XImage *ximage = 0;

  if (4 != sscanf ((const char *) *data,
                   "%d %d %d %d %c", &w, &h, &ncolors, &nbytes, &c)) {
    fprintf (stderr, "%s: unparsable XPM header\n", progname);
    abort();
  }

  if (ncolors < 1 || ncolors > 255) {
    fprintf (stderr, "%s: XPM: ncolors is %d\n", progname, ncolors);
    abort();
  }
  if (nbytes != 1) {
    fprintf (stderr, "%s: %d-byte XPM files not supported\n",
             progname, nbytes);
    abort();
  }
  data++;

  for (i = 0; i < ncolors; i++)
    {
      const char *line = *data;
      cmap[i].byte = *line++;
      while (*line)
        {
          int r, g, b;
          char which;
          while (*line == ' ' || *line == '\t')
            line++;
          which = *line;
          if (!which) continue;  /* whitespace at end of line */
          line++;
          if (which != 'c' && which != 'm') {
            fprintf (stderr, "%s: unknown XPM pixel type '%c' in \"%s\"\n",
                     progname, which, *data);
            abort();
          }
          while (*line == ' ' || *line == '\t')
            line++;
          if (!strncasecmp(line, "None", 4))
            {
              r = g = b = -1;
              line += 4;
            }
          else
            {
              if (*line != '#') {
                fprintf (stderr, "%s: unparsable XPM color spec: \"%s\"\n",
                         progname, line);
                abort();
              }
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
  pixel_count = 0;
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
              color.red   = (cmap[i].mr << 8) | cmap[i].mr;
              color.green = (cmap[i].mg << 8) | cmap[i].mg;
              color.blue  = (cmap[i].mb << 8) | cmap[i].mb;
              if (!XAllocColor (dpy, colormap, &color)) {
                fprintf (stderr, "%s: unable to allocate XPM color\n",
                         progname);
                abort();
              }
            }
          pixels[pixel_count] = color.pixel;
          rmap[(int) cmap[i].byte] = pixel_count;
          pixel_count++;
        }
    }

  ximage = XCreateImage (dpy, visual, depth,
                         (depth == 1 ? XYBitmap : ZPixmap),
                         0, 0, w, h, 8, 0);
  if (! ximage) return 0;

  ximage->bitmap_bit_order =
    ximage->byte_order =
    (bigendian() ? MSBFirst : LSBFirst);

  ximage->data = (char *) calloc (ximage->height, ximage->bytes_per_line);
  if (!ximage->data) {
    XDestroyImage (ximage);
    return 0;
  }

  w8 = (w + 7) / 8;
  if (mask_ret)
    {
      int s = (w8 * h) + 1;
      *mask_ret = (unsigned char *) malloc (s);
      if (!*mask_ret)
        mask_ret = 0;
      else
        memset (*mask_ret, 255, s);
    }

  for (y = 0; y < h; y++)
    {
      const char *line = *data++;
      for (x = 0; x < w; x++)
        {
          int p = rmap[(int) *line];
          line++;
          XPutPixel (ximage, x, y, (p == 255 ? transparent_color : pixels[p]));

          if (p == 255 && mask_ret)
            (*mask_ret)[(y * w8) + (x >> 3)] &= (~(1 << (x & 7)));
        }
    }

  *width_ret = w;
  *height_ret = h;
  *pixels_ret = pixels;
  *npixels_ret = pixel_count;
  return ximage;
}
