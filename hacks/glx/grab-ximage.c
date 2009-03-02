/* grab-ximage.c --- grab the screen to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>	/* only for GLfloat */

#include "grabscreen.h"
#include "visual.h"

extern char *progname;

#include <X11/Xutil.h>

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

/* return the next larger power of 2. */
static int
to_pow2 (int i)
{
  static unsigned int pow2[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
                                 2048, 4096, 8192, 16384, 32768, 65536 };
  int j;
  for (j = 0; j < countof(pow2); j++)
    if (pow2[j] >= i) return pow2[j];
  abort();  /* too big! */
}


/* Given a bitmask, returns the position and width of the field.
 */
static void
decode_mask (unsigned int mask, unsigned int *pos_ret, unsigned int *size_ret)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1L << i))
      {
        int j = 0;
        *pos_ret = i;
        for (; i < 32; i++, j++)
          if (! (mask & (1L << i)))
            break;
        *size_ret = j;
        return;
      }
}


/* Given a value and a field-width, expands the field to fill out 8 bits.
 */
static unsigned char
spread_bits (unsigned char value, unsigned char width)
{
  switch (width)
    {
    case 8: return value;
    case 7: return (value << 1) | (value >> 6);
    case 6: return (value << 2) | (value >> 4);
    case 5: return (value << 3) | (value >> 2);
    case 4: return (value << 4) | (value);
    case 3: return (value << 5) | (value << 2) | (value >> 2);
    case 2: return (value << 6) | (value << 4) | (value);
    default: abort(); break;
    }
}


/* Returns an XImage structure containing an image of the desktop.
   (As a side-effect, that image will be painted onto the given Window.)
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.
 */
XImage *
screen_to_ximage (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  int win_width, win_height;
  int tex_width, tex_height;

  grab_screen_image (screen, window);

  XGetWindowAttributes (dpy, window, &xgwa);
  win_width = xgwa.width;
  win_height = xgwa.height;

  /* GL texture sizes must be powers of two. */
  tex_width  = to_pow2(win_width);
  tex_height = to_pow2(win_height);

  /* Convert the server-side Drawable to a client-side GL-ordered XImage.
   */
  {
    XImage *ximage1, *ximage2;
    XColor *colors = 0;

    ximage1 = XGetImage (dpy, window, 0, 0, win_width, win_height, ~0L,
                         ZPixmap);
    ximage2 = XCreateImage (dpy, xgwa.visual, 32, ZPixmap, 0, 0,
                            tex_width, tex_height, 32, 0);

    ximage2->data = (char *) calloc (tex_height, ximage2->bytes_per_line);

    {
      Screen *dscreen = DefaultScreenOfDisplay (dpy);
      Visual *dvisual = DefaultVisualOfScreen (dscreen);
      if (visual_class (dscreen, dvisual) == PseudoColor ||
          visual_class (dscreen, dvisual) == GrayScale)
        {
          Colormap cmap = DefaultColormapOfScreen(dscreen);
          int ncolors = visual_cells (dscreen, dvisual);
          int i;
          colors = (XColor *) calloc (sizeof (*colors), ncolors+1);
          for (i = 0; i < ncolors; i++)
            colors[i].pixel = i;
          XQueryColors (dpy, cmap, colors, ncolors);
        }
    }

    /* Translate the server-ordered image to a client-ordered image.
     */
    {
      int x, y;
      int crpos, cgpos, cbpos, capos;  /* bitfield positions */
      int srpos, sgpos, sbpos;
      int srmsk, sgmsk, sbmsk;
      int srsiz, sgsiz, sbsiz;
      int i;

      unsigned char spread_map[3][256];

      srmsk = ximage1->red_mask;
      sgmsk = ximage1->green_mask;
      sbmsk = ximage1->blue_mask;

      decode_mask (srmsk, &srpos, &srsiz);
      decode_mask (sgmsk, &sgpos, &sgsiz);
      decode_mask (sbmsk, &sbpos, &sbsiz);

      /* Note that unlike X, which is endianness-agnostic (since any XImage
         can have its own specific bit ordering, with the server reversing
         things as necessary) OpenGL pretends everything is client-side, so
         we need to pack things in "RGBA" order on the client machine,
         regardless of its endianness.
       */
      crpos =  0, cgpos =  8, cbpos = 16, capos = 24;

      for (i = 0; i < 256; i++)
        {
          spread_map[0][i] = spread_bits (i, srsiz);
          spread_map[1][i] = spread_bits (i, sgsiz);
          spread_map[2][i] = spread_bits (i, sbsiz);
        }

      for (y = 0; y < win_height; y++)
        {
          int y2 = (win_height-1-y); /* Texture maps are upside down. */
          for (x = 0; x < win_width; x++)
            {
              unsigned long sp = XGetPixel (ximage1, x, y2);
              unsigned char sr, sg, sb;
              unsigned long cp;

              if (colors)
                {
                  sr = colors[sp].red   & 0xFF;
                  sg = colors[sp].green & 0xFF;
                  sb = colors[sp].blue  & 0xFF;
                }
              else
                {
                  sr = (sp & srmsk) >> srpos;
                  sg = (sp & sgmsk) >> sgpos;
                  sb = (sp & sbmsk) >> sbpos;

                  sr = spread_map[0][sr];
                  sg = spread_map[1][sg];
                  sb = spread_map[2][sb];
                }

              cp = ((sr << crpos) |
                    (sg << cgpos) |
                    (sb << cbpos) |
                    (0xFF << capos));

              XPutPixel (ximage2, x, y, cp);
            }
        }
    }

    if (colors) free (colors);
    free (ximage1->data);
    ximage1->data = 0;
    XDestroyImage (ximage1);

#if 0
    fprintf(stderr, "%s: grabbed %dx%d window 0x%lx to %dx%d texture\n",
            progname, win_width, win_height, (unsigned long) window,
            tex_width, tex_height);
#endif

    return ximage2;
  }
}
