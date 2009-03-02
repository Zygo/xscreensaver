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

extern char *progname;

#include <X11/Xutil.h>

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}

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

    ximage1 = XGetImage (dpy, window, 0, 0, win_width, win_height, ~0L,
                         ZPixmap);
    ximage2 = XCreateImage (dpy, xgwa.visual, 32, ZPixmap, 0, 0,
                            tex_width, tex_height, 32, 0);

    ximage2->data = (char *) calloc (tex_height, ximage2->bytes_per_line);

    /* Translate the server-ordered image to a client-ordered image.
     */
    {
      int x, y;
      int crpos, cgpos, cbpos, capos;  /* bitfield positions */
      int srpos, sgpos, sbpos;
      int srmsk, sgmsk, sbmsk;
      int sdepth = ximage1->depth;

      srmsk = ximage1->red_mask;
      sgmsk = ximage1->green_mask;
      sbmsk = ximage1->blue_mask;
      if (sdepth == 32 || sdepth == 24)
        srpos = 16, sgpos = 8, sbpos = 0;
      else /* 15 or 16 */
        srpos = 10, sgpos = 5, sbpos = 0;

      /* Note that unlike X, which is endianness-agnostic (since any XImage
         can have its own specific bit ordering, with the server reversing
         things as necessary) OpenGL pretends everything is client-side, so
         we need to pack things in the right order for the client machine.
       */
      if (bigendian())
        crpos = 24, cgpos = 16, cbpos =  8, capos =  0;
      else
        crpos =  0, cgpos =  8, cbpos = 16, capos = 24;

      for (y = 0; y < win_height; y++)
        {
          int y2 = (win_height-1-y); /* Texture maps are upside down. */
          for (x = 0; x < win_width; x++)
            {
              unsigned long sp = XGetPixel (ximage1, x, y2);
              unsigned char sr = (sp & srmsk) >> srpos;
              unsigned char sg = (sp & sgmsk) >> sgpos;
              unsigned char sb = (sp & sbmsk) >> sbpos;
              unsigned long cp;

              if (sdepth < 24)   /* spread 5 bits to 8 */
                {
                  sr = (sr << 3) | (sr >> 2);
                  sg = (sg << 3) | (sg >> 2);
                  sb = (sb << 3) | (sb >> 2);
                }
              cp = ((sr << crpos) |
                    (sg << cgpos) |
                    (sb << cbpos) |
                    (0xFF << capos));
              XPutPixel (ximage2, x, y, cp);
            }
        }
    }

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
