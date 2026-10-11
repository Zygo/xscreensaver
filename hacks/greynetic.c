/* xscreensaver, Copyright (c) 1992-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhack.h"

#ifndef HAVE_JWXYZ
# define DO_STIPPLE
#endif

#define NBITS 12

/* On some systems (notably MacOS X) these files are messed up.
 * They're tiny, so we might as well just inline them here.
 *
 * # include <X11/bitmaps/stipple>
 * # include <X11/bitmaps/cross_weave>
 * # include <X11/bitmaps/dimple1>
 * # include <X11/bitmaps/dimple3>
 * # include <X11/bitmaps/flipped_gray>
 * # include <X11/bitmaps/gray1>
 * # include <X11/bitmaps/gray3>
 * # include <X11/bitmaps/hlines2>
 * # include <X11/bitmaps/light_gray>
 * # include <X11/bitmaps/root_weave>
 * # include <X11/bitmaps/vlines2>
 * # include <X11/bitmaps/vlines3>
*/

#ifdef DO_STIPPLE
#define stipple_width  16
#define stipple_height 4
static unsigned char stipple_bits[] = {
  0x55, 0x55, 0xee, 0xee, 0x55, 0x55, 0xba, 0xbb};

#define cross_weave_width  16
#define cross_weave_height 16
static unsigned char cross_weave_bits[] = {
   0x55, 0x55, 0x88, 0x88, 0x55, 0x55, 0x22, 0x22, 0x55, 0x55, 0x88, 0x88,
   0x55, 0x55, 0x22, 0x22, 0x55, 0x55, 0x88, 0x88, 0x55, 0x55, 0x22, 0x22,
   0x55, 0x55, 0x88, 0x88, 0x55, 0x55, 0x22, 0x22};

#define dimple1_width 16
#define dimple1_height 16
static unsigned char dimple1_bits[] = {
   0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00,
   0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00,
   0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00};

#define dimple3_width 16
#define dimple3_height 16
static unsigned char dimple3_bits[] = {
   0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define flipped_gray_width  4
#define flipped_gray_height 2
static char flipped_gray_bits[] = { 0x07, 0x0d};
#define gray1_width  2
#define gray1_height 2
static char gray1_bits[] = { 0x01, 0x02};
#define gray3_width  4
#define gray3_height 4
static char gray3_bits[] = { 0x01, 0x00, 0x04, 0x00};
#define hlines2_width  1
#define hlines2_height 2
static char hlines2_bits[] = { 0x01, 0x00};
#define light_gray_width  4
#define light_gray_height 2
static char light_gray_bits[] = { 0x08, 0x02};
#define root_weave_width  4
#define root_weave_height 4
static char root_weave_bits[] = { 0x07, 0x0d, 0x0b, 0x0e};
#define vlines2_width  2
#define vlines2_height 1
static char vlines2_bits[] = { 0x01};
#define vlines3_width  3
#define vlines3_height 1
static char vlines3_bits[] = { 0x02};

#endif /* DO_STIPPLE */

struct state {
  Display *dpy;
  Window window;

  Pixmap pixmaps [NBITS];

  GC gc;
  int delay;
  unsigned long fg, bg, pixels [512];
  int npixels;
  int xlim, ylim;
  Bool grey_p;
  Colormap cmap;
};


static void *
greynetic_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
# ifdef DO_STIPPLE
  int i;
# endif /* DO_STIPPLE */
  XGCValues gcv;
  XWindowAttributes xgwa;
  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->xlim = xgwa.width;
  st->ylim = xgwa.height;
  st->cmap = xgwa.colormap;
  st->npixels = 0;
  st->grey_p = get_boolean_resource(st->dpy, "grey", "Boolean");
  gcv.foreground= st->fg= get_pixel_resource(st->dpy, st->cmap, "foreground","Foreground");
  gcv.background= st->bg= get_pixel_resource(st->dpy, st->cmap, "background","Background");

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;

# ifndef DO_STIPPLE
  st->gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
#  ifdef HAVE_JWXYZ /* allow non-opaque alpha components in pixel values */
  jwxyz_XSetAlphaAllowed (st->dpy, st->gc, True);
#  endif /* HAVE_JWXYZ */
# else /* DO_STIPPLE */
  gcv.fill_style= FillOpaqueStippled;
  st->gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground|GCFillStyle, &gcv);
  
  i = 0;
# define BITS(n,w,h) \
  st->pixmaps [i++] = \
    XCreatePixmapFromBitmapData (st->dpy, st->window, (char *) n, w, h, 1, 0, 1)

  BITS (stipple_bits, stipple_width, stipple_height);
  BITS (cross_weave_bits, cross_weave_width, cross_weave_height);
  BITS (dimple1_bits, dimple1_width, dimple1_height);
  BITS (dimple3_bits, dimple3_width, dimple3_height);
  BITS (flipped_gray_bits, flipped_gray_width, flipped_gray_height);
  BITS (gray1_bits, gray1_width, gray1_height);
  BITS (gray3_bits, gray3_width, gray3_height);
  BITS (hlines2_bits, hlines2_width, hlines2_height);
  BITS (light_gray_bits, light_gray_width, light_gray_height);
  BITS (root_weave_bits, root_weave_width, root_weave_height);
  BITS (vlines2_bits, vlines2_width, vlines2_height);
  BITS (vlines3_bits, vlines3_width, vlines3_height);
# endif /* DO_STIPPLE */
  return st;
}

static unsigned long
greynetic_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int x, y, w=0, h=0, i;
  XGCValues gcv;

  for (i = 0; i < 10; i++) /* minimize area, but don't try too hard */
    {
      w = 50 + random () % (st->xlim - 50);
      h = 50 + random () % (st->ylim - 50);
      if (w + h < st->xlim && w + h < st->ylim)
	break;
    }
  x = random () % (st->xlim - w);
  y = random () % (st->ylim - h);
# ifdef DO_STIPPLE
  gcv.stipple = st->pixmaps [random () % NBITS];
# endif /* !DO_STIPPLE */
  if (mono_p)
    {
    MONO:
      if (random () & 1)
	gcv.foreground = st->fg, gcv.background = st->bg;
      else
	gcv.foreground = st->bg, gcv.background = st->fg;
    }
  else
    {
      XColor fgc, bgc;
      if (st->npixels == sizeof (st->pixels) / sizeof (unsigned long))
	goto REUSE;
      fgc.flags = bgc.flags = DoRed|DoGreen|DoBlue;
      fgc.red = random ();
      fgc.green = random ();
      fgc.blue = random ();
# ifdef DO_STIPPLE
      bgc.red = random ();
      bgc.green = random ();
      bgc.blue = random ();
# endif /* DO_STIPPLE */

      if (st->grey_p)
        {
          fgc.green = fgc.blue = fgc.red;
          bgc.green = bgc.blue = bgc.red;
        }

      if (! XAllocColor (st->dpy, st->cmap, &fgc))
	goto REUSE;
      st->pixels [st->npixels++] = fgc.pixel;
      gcv.foreground = fgc.pixel;
# ifdef DO_STIPPLE
      if (! XAllocColor (st->dpy, st->cmap, &bgc))
	goto REUSE;
      st->pixels [st->npixels++] = bgc.pixel;
      gcv.background = bgc.pixel;
# endif /* DO_STIPPLE */
      goto DONE;
    REUSE:
      if (st->npixels <= 0)
	{
	  mono_p = 1;
	  goto MONO;
	}
      gcv.foreground = st->pixels [random () % st->npixels];
# ifdef DO_STIPPLE
      gcv.background = st->pixels [random () % st->npixels];
# endif /* DO_STIPPLE */
    DONE:
      ;

# ifdef HAVE_JWXYZ
      {
        /* give a non-opaque alpha to the color */
        unsigned long pixel = gcv.foreground;
        unsigned long amask = BlackPixel (dpy,0);
        unsigned long a = (random() & amask);
        pixel = (pixel & (~amask)) | a;
        gcv.foreground = pixel;
      }
# endif /* !HAVE_JWXYZ */
    }
# ifndef DO_STIPPLE
  XChangeGC (st->dpy, st->gc, GCForeground, &gcv);
# else  /* DO_STIPPLE */
  XChangeGC (st->dpy, st->gc, GCStipple|GCForeground|GCBackground, &gcv);
# endif /* DO_STIPPLE */
  XFillRectangle (st->dpy, st->window, st->gc, x, y, w, h);
  return st->delay;
}


static const char *greynetic_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*delay:	10000",
  "*grey:	false",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec greynetic_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-grey",		".grey",	XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};

static void
greynetic_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xlim = w;
  st->ylim = h;
}

static Bool
greynetic_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
greynetic_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (st->dpy, st->gc);
  free (st);
}

XSCREENSAVER_MODULE ("Greynetic", greynetic)

