/* xscreensaver, Copyright (c) 1997-2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Concept snarfed from Michael D. Bayne in
 * http://samskivert.com/internet/deep/1997/04/16/body.html
 */

#include "screenhack.h"
#include "xshm.h"

struct state {
  Display *dpy;
  Window window;
  XShmSegmentInfo shm_info;

  int delay;
  int offset;
  XColor *colors;
  int ncolors;
  GC gc;
  unsigned long fg_pixel;
  unsigned long bg_pixel;
  int depth;

  int draw_y, draw_xo, draw_yo;
  int draw_factor;
  XImage *draw_image;
  XWindowAttributes xgwa;
};

static void
moire_init_1 (struct state *st)
{
  int oncolors;
  int i;
  int fgh, bgh;
  double fgs, fgv, bgs, bgv;
  XWindowAttributes xgwa;
  XColor fgc, bgc;
  XGCValues gcv;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->offset = get_integer_resource (st->dpy, "offset", "Integer");
  if (st->offset < 2) st->offset = 2;

 MONO:
  if (st->colors)
    {
      for (i = 0; i < st->ncolors; i++)
	XFreeColors (st->dpy, xgwa.colormap, &st->colors[i].pixel, 1, 0);
      free(st->colors);
      st->colors = 0;
    }

  if (mono_p)
    {
      st->fg_pixel = WhitePixelOfScreen (DefaultScreenOfDisplay(st->dpy));
      st->bg_pixel = BlackPixelOfScreen (DefaultScreenOfDisplay(st->dpy));
    }
  else
    {
      st->fg_pixel = get_pixel_resource (st->dpy,
				     xgwa.colormap, "foreground", "Foreground");
      st->bg_pixel = get_pixel_resource (st->dpy,
				     xgwa.colormap, "background", "Background");
    }

  if (mono_p)
    {
      st->offset *= 20;   /* compensate for lack of shading */
      gcv.foreground = st->fg_pixel;
    }
  else
    {
      st->ncolors = get_integer_resource (st->dpy, "ncolors", "Integer");
      if (st->ncolors < 2) st->ncolors = 2;
      oncolors = st->ncolors;

      fgc.flags = bgc.flags = DoRed|DoGreen|DoBlue;
      if (get_boolean_resource(st->dpy, "random","Boolean"))
	{
	  fgc.red   = random() & 0xFFFF;
	  fgc.green = random() & 0xFFFF;
	  fgc.blue  = random() & 0xFFFF;
	  bgc.red   = random() & 0xFFFF;
	  bgc.green = random() & 0xFFFF;
	  bgc.blue  = random() & 0xFFFF;
	}
      else
	{
	  fgc.pixel = st->fg_pixel;
	  bgc.pixel = st->bg_pixel;
	  XQueryColor (st->dpy, xgwa.colormap, &fgc);
	  XQueryColor (st->dpy, xgwa.colormap, &bgc);
	}
      rgb_to_hsv (fgc.red, fgc.green, fgc.blue, &fgh, &fgs, &fgv);
      rgb_to_hsv (bgc.red, bgc.green, bgc.blue, &bgh, &bgs, &bgv);

      st->colors = (XColor *) malloc (sizeof (XColor) * (st->ncolors+2));
      memset(st->colors, 0, (sizeof (XColor) * (st->ncolors+2)));
      make_color_ramp (xgwa.screen, xgwa.visual, xgwa.colormap,
		       fgh, fgs, fgv, bgh, bgs, bgv,
		       st->colors, &st->ncolors,
		       True, True, False);
      if (st->ncolors != oncolors)
	fprintf(stderr, "%s: got %d of %d requested colors.\n",
		progname, st->ncolors, oncolors);

      if (st->ncolors <= 2)
	{
	  mono_p = True;
	  goto MONO;
	}

      gcv.foreground = st->colors[0].pixel;
    }
  if (st->gc) XFreeGC (st->dpy, st->gc);
  st->gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
}


static void *
moire_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  moire_init_1 (st);
  return st;
}


static unsigned long
moire_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XGCValues gcv;
  int chunk_size = 20, ii;

  if (st->draw_y == 0) 
    {
      moire_init_1 (st);

      XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

      st->draw_xo = (random() % st->xgwa.width)  - st->xgwa.width/2;
      st->draw_yo = (random() % st->xgwa.height) - st->xgwa.height/2;
      st->draw_factor = (random() % st->offset) + 1;

      st->depth = visual_depth(DefaultScreenOfDisplay(st->dpy), st->xgwa.visual);

      st->draw_image = create_xshm_image(st->dpy, st->xgwa.visual,
                                         st->depth, ZPixmap, &st->shm_info, /* depth, format, shm_info */
                                         st->xgwa.width, chunk_size);       /* w, h */
    }

  /* for (y = 0; y < st->xgwa.height; y++) */
  for (ii = 0; ii < chunk_size; ii++)
    {
      int x;
      for (x = 0; x < st->xgwa.width; x++)
	{
	  double xx = x + st->draw_xo;
	  double yy = st->draw_y + ii + st->draw_yo;
	  double i = ((xx * xx) + (yy * yy)) / (double) st->draw_factor;
	  if (mono_p)
	    gcv.foreground = ((((long) i) & 1) ? st->fg_pixel : st->bg_pixel);
	  else
	    gcv.foreground = st->colors[((long) i) % st->ncolors].pixel;
	  XPutPixel (st->draw_image, x, ii, gcv.foreground);
	}

      if (st->draw_y + ii >= st->xgwa.height)
        break;
    }

    put_xshm_image(st->dpy, st->window, st->gc, st->draw_image, 0, 0, 0, st->draw_y, st->xgwa.width, chunk_size, &st->shm_info);
    st->draw_y += chunk_size;

    if (st->draw_y >= st->xgwa.height)
      {
        st->draw_y = 0;

        destroy_xshm_image (st->dpy, st->draw_image, &st->shm_info);
        st->draw_image = 0;

        return st->delay * 1000000;
      }

  return st->delay * 10000;
}


static const char *moire_defaults [] = {
  ".background:		blue",
  ".foreground:		red",
  "*fpsSolid:		true",
  "*random:		true",
  "*delay:		5",
  "*ncolors:		64",
  "*offset:		50",
  "*useSHM:	      True",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec moire_options [] = {
  { "-random",		".random",	XrmoptionNoArg, "True" },
  { "-no-random",	".random",	XrmoptionNoArg, "False" },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-offset",		".offset",	XrmoptionSepArg, 0 },
  { "-shm",		".useSHM",	XrmoptionNoArg, "True" },
  { "-no-shm",		".useSHM",	XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

static void
moire_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
moire_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
moire_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->draw_image)
    destroy_xshm_image (dpy, st->draw_image, &st->shm_info);
  XFreeGC (dpy, st->gc);
  free (st->colors);
  free (st);
}

XSCREENSAVER_MODULE ("Moire", moire)
