/* xscreensaver, Copyright (c) 1997, 1998, 2001 Jamie Zawinski <jwz@jwz.org>
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
 * http://www.go2net.com/internet/deep/1997/04/16/body.html
 */

#include "screenhack.h"
#include <X11/Xutil.h>
#include <stdio.h>

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
static Bool use_shm;
static XShmSegmentInfo shm_info;
#endif /* HAVE_XSHM_EXTENSION */

static int offset = 0;
static XColor *colors = 0;
static int ncolors = 0;
static GC gc = 0;
static unsigned long fg_pixel = 0;
static unsigned long bg_pixel = 0;

static void
init_moire (Display *dpy, Window window)
{
  int oncolors;
  int i;
  int fgh, bgh;
  double fgs, fgv, bgs, bgv;
  XWindowAttributes xgwa;
  XColor fgc, bgc;
  XGCValues gcv;
  XGetWindowAttributes (dpy, window, &xgwa);

  offset = get_integer_resource ("offset", "Integer");
  if (offset < 2) offset = 2;

#ifdef HAVE_XSHM_EXTENSION
  use_shm = get_boolean_resource("useSHM", "Boolean");
#endif /*  HAVE_XSHM_EXTENSION */

 MONO:
  if (colors)
    {
      for (i = 0; i < ncolors; i++)
	XFreeColors (dpy, xgwa.colormap, &colors[i].pixel, 1, 0);
      free(colors);
      colors = 0;
    }

  if (mono_p)
    {
      fg_pixel = WhitePixelOfScreen (DefaultScreenOfDisplay(dpy));
      bg_pixel = BlackPixelOfScreen (DefaultScreenOfDisplay(dpy));
    }
  else
    {
      fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy,
				     xgwa.colormap);
      bg_pixel = get_pixel_resource ("background", "Background", dpy,
				     xgwa.colormap);
    }

  if (mono_p)
    {
      offset *= 20;   /* compensate for lack of shading */
      gcv.foreground = fg_pixel;
    }
  else
    {
      ncolors = get_integer_resource ("ncolors", "Integer");
      if (ncolors < 2) ncolors = 2;
      oncolors = ncolors;

      fgc.flags = bgc.flags = DoRed|DoGreen|DoBlue;
      if (get_boolean_resource("random","Boolean"))
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
	  fgc.pixel = fg_pixel;
	  bgc.pixel = bg_pixel;
	  XQueryColor (dpy, xgwa.colormap, &fgc);
	  XQueryColor (dpy, xgwa.colormap, &bgc);
	}
      rgb_to_hsv (fgc.red, fgc.green, fgc.blue, &fgh, &fgs, &fgv);
      rgb_to_hsv (bgc.red, bgc.green, bgc.blue, &bgh, &bgs, &bgv);

      colors = (XColor *) malloc (sizeof (XColor) * (ncolors+2));
      memset(colors, 0, (sizeof (XColor) * (ncolors+2)));
      make_color_ramp (dpy, xgwa.colormap,
		       fgh, fgs, fgv, bgh, bgs, bgv,
		       colors, &ncolors,
		       True, True, False);
      if (ncolors != oncolors)
	fprintf(stderr, "%s: got %d of %d requested colors.\n",
		progname, ncolors, oncolors);

      if (ncolors <= 2)
	{
	  mono_p = True;
	  goto MONO;
	}

      gcv.foreground = colors[0].pixel;
    }
  gc = XCreateGC (dpy, window, GCForeground, &gcv);
}


static void
moire (Display *dpy, Window window, int offset, XColor *colors, int ncolors)
{
  long x, y, xo, yo;
  int factor = (random() % offset) + 1;
  XGCValues gcv;
  XWindowAttributes xgwa;
  XImage *image;
  int depth;
  XGetWindowAttributes (dpy, window, &xgwa);

  xo = (random() % xgwa.width)  - xgwa.width/2;
  yo = (random() % xgwa.height) - xgwa.height/2;

  depth = visual_depth(DefaultScreenOfDisplay(dpy), xgwa.visual);

  image = 0;
# ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    {
      image = create_xshm_image(dpy, xgwa.visual, depth, ZPixmap, 0,
				&shm_info, xgwa.width, 1);
      if (!image)
	use_shm = False;
    }
# endif /* HAVE_XSHM_EXTENSION */

  if (!image)
    {
      image = XCreateImage (dpy, xgwa.visual,
			    depth, ZPixmap, 0,	    /* depth, format, offset */
			    0, xgwa.width, 1, 8, 0); /* data, w, h, pad, bpl */
      image->data = (char *) calloc(image->height, image->bytes_per_line);
    }

  for (y = 0; y < xgwa.height; y++)
    {
      for (x = 0; x < xgwa.width; x++)
	{
	  double xx = x + xo;
	  double yy = y + yo;
	  double i = ((xx * xx) + (yy * yy)) / (double) factor;
	  if (mono_p)
	    gcv.foreground = ((((long) i) & 1) ? fg_pixel : bg_pixel);
	  else
	    gcv.foreground = colors[((long) i) % ncolors].pixel;
	  XPutPixel (image, x, 0, gcv.foreground);
	}

# ifdef HAVE_XSHM_EXTENSION
      if (use_shm)
	XShmPutImage(dpy, window, gc, image, 0, 0, 0, y, xgwa.width, 1, False);
      else
# endif /*  HAVE_XSHM_EXTENSION */
	XPutImage (dpy, window, gc, image, 0, 0, 0, y, xgwa.width, 1);

      XSync(dpy, False);
    }

# ifdef HAVE_XSHM_EXTENSION
  if (!use_shm)
# endif /*  HAVE_XSHM_EXTENSION */
    if (image->data)
      {
	free(image->data);
	image->data = 0;
      }

# ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    destroy_xshm_image (dpy, image, &shm_info);
  else
# endif /*  HAVE_XSHM_EXTENSION */
    XDestroyImage (image);
}


char *progclass = "Moire";

char *defaults [] = {
  ".background:		blue",
  ".foreground:		red",
  "*random:		true",
  "*delay:		5",
  "*ncolors:		64",
  "*offset:		50",
#ifdef HAVE_XSHM_EXTENSION
  "*useSHM:	      True",
#endif /*  HAVE_XSHM_EXTENSION */
  0
};

XrmOptionDescRec options [] = {
  { "-random",		".random",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-offset",		".offset",	XrmoptionSepArg, 0 },
#ifdef HAVE_XSHM_EXTENSION
  { "-shm",		".useSHM",	XrmoptionNoArg, "True" },
  { "-no-shm",		".useSHM",	XrmoptionNoArg, "False" },
#endif /*  HAVE_XSHM_EXTENSION */
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  int delay = get_integer_resource ("delay", "Integer");
  while (1)
    {
      init_moire (dpy, window);
      moire (dpy, window, offset, colors, ncolors);
      XSync (dpy, False);
      screenhack_handle_events (dpy);
      if (delay)
	sleep(delay);
    }
}
