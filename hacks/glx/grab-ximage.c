/* grab-ximage.c --- grab the screen to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001, 2003 Jamie Zawinski <jwz@jwz.org>
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
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>	/* only for GLfloat */

#include "grabscreen.h"
#include "visual.h"

extern char *progname;

#include <X11/Xutil.h>
#include <sys/time.h>

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


static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


static XImage *
screen_to_ximage_1 (Screen *screen, Window window, Pixmap pixmap)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  int win_width, win_height;
  int tex_width, tex_height;

  XGetWindowAttributes (dpy, window, &xgwa);
  win_width = xgwa.width;
  win_height = xgwa.height;

  /* GL texture sizes must be powers of two. */
  tex_width  = to_pow2(win_width);
  tex_height = to_pow2(win_height);

  /* Convert the server-side Pixmap to a client-side GL-ordered XImage.
   */
  {
    XImage *ximage1, *ximage2;
    XColor *colors = 0;

    ximage1 = XGetImage (dpy, pixmap, 0, 0, win_width, win_height, ~0L,
                         ZPixmap);
    XFreePixmap (dpy, pixmap);
    pixmap = 0;

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
      unsigned int crpos=0, cgpos=0, cbpos=0, capos=0; /* bitfield positions */
      unsigned int srpos=0, sgpos=0, sbpos=0;
      unsigned int srmsk=0, sgmsk=0, sbmsk=0;
      unsigned int srsiz=0, sgsiz=0, sbsiz=0;
      int i;

      unsigned char spread_map[3][256];

      if (colors == 0)  /* truecolor */
        {
          srmsk = ximage2->red_mask;
          sgmsk = ximage2->green_mask;
          sbmsk = ximage2->blue_mask;

          decode_mask (srmsk, &srpos, &srsiz);
          decode_mask (sgmsk, &sgpos, &sgsiz);
          decode_mask (sbmsk, &sbpos, &sbsiz);
        }

      /* Note that unlike X, which is endianness-agnostic (since any XImage
         can have its own specific bit ordering, with the server reversing
         things as necessary) OpenGL pretends everything is client-side, so
         we need to pack things in "RGBA" order on the client machine,
         regardless of its endianness.
       */
      if (bigendian())
        crpos = 24, cgpos = 16, cbpos =  8, capos =  0;
      else
        crpos =  0, cgpos =  8, cbpos = 16, capos = 24;

      if (colors == 0)  /* truecolor */
        {
          for (i = 0; i < 256; i++)
            {
              spread_map[0][i] = spread_bits (i, srsiz);
              spread_map[1][i] = spread_bits (i, sgsiz);
              spread_map[2][i] = spread_bits (i, sbsiz);
            }
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

    if (pixmap) XFreePixmap (dpy, pixmap);
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


/* Returns an XImage structure containing an image of the desktop.
   (As a side-effect, that image *may* be painted onto the given Window.)
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.
 */
XImage *
screen_to_ximage (Screen *screen, Window window, char **filename_return)
{
  Display *dpy = DisplayOfScreen (screen);
  Pixmap pixmap = 0;
  XWindowAttributes xgwa;

  XGetWindowAttributes (dpy, window, &xgwa);
  pixmap = XCreatePixmap (dpy, window, xgwa.width, xgwa.height, xgwa.depth);
  load_random_image (screen, window, pixmap, filename_return);

  return screen_to_ximage_1 (screen, window, pixmap);
}


typedef struct {
  void (*callback) (Screen *, Window, XImage *,
                    const char *name, void *closure, double cvt_time);
  void *closure;
  Pixmap pixmap;
} img_closure;


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
img_cb (Screen *screen, Window window, Drawable drawable,
        const char *name, void *closure)
{
  XImage *ximage;
  double cvt_time = double_time();
  img_closure *data = (img_closure *) closure;
  /* copy closure data to stack and free the original before running cb */
  img_closure dd = *data;
  memset (data, 0, sizeof (*data));
  free (data);
  data = 0;
  ximage = screen_to_ximage_1 (screen, window, dd.pixmap);
  dd.callback (screen, window, ximage, name, dd.closure, cvt_time);
}


/* Like the above, but loads the image in the background and runs the
   given callback once it has been loaded.
 */
#include <X11/Intrinsic.h>
extern XtAppContext app;

void
fork_screen_to_ximage (Screen *screen, Window window,
                       void (*callback) (Screen *, Window, XImage *,
                                         const char *name,
                                         void *closure,
                                         double cvt_time),
                       void *closure)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  img_closure *data = (img_closure *) calloc (1, sizeof(*data));
  data->callback = callback;
  data->closure  = closure;

  XGetWindowAttributes (dpy, window, &xgwa);
  data->pixmap = XCreatePixmap (dpy, window, xgwa.width, xgwa.height,
                                xgwa.depth);
  fork_load_random_image (screen, window, data->pixmap, img_cb, data);
}
