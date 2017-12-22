/* imsmap, Copyright (c) 1992-2013 Juergen Nickelsen and Jamie Zawinski.
 * Derived from code by Markus Schirmer, TU Berlin.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Revision History:
 * 24-aug-92: jwz: hacked.
 * 17-May-97: jwz: hacked more.
 */

#include <stdio.h>
#include <math.h>

#include "screenhack.h"

#define NSTEPS 7
#define COUNT (1 << NSTEPS)
#define CELL(c, r) st->cell[((unsigned int)(c)) + ((unsigned int) (r)) * st->xmax]

#if defined(sun) && !__STDC__	/* sun cc doesn't know "signed char" */
#define signed /**/
#endif

struct state {
  Display *dpy;
  Window window;
  Colormap cmap;
  int ncolors;
  XColor *colors;
  Bool extra_krinkly_p;

  int delay, delay2;
  signed char *cell;
  int xmax, ymax;
  int iteration, iterations;

  int cx, xstep, ystep, xnextStep, ynextStep;

  unsigned int last_pixel, last_valid;
  int flip_x;
  int flip_xy;

  GC gc, gc2;
  XWindowAttributes xgwa;

  struct timeval then;
};


#define HEIGHT_TO_PIXEL(height)					\
	((height) < 0						\
	 ? (st->extra_krinkly_p					\
	    ? st->ncolors - 1 -  ((-(height)) % st->ncolors)	\
	    : 0)						\
	 : ((height) >= st->ncolors				\
	    ? (st->extra_krinkly_p				\
	       ? (height) % st->ncolors				\
	       : st->ncolors-1)					\
	    : (height)))


static unsigned int
set (struct state *st,
     unsigned int l,
     unsigned int c,
     unsigned int size,
     int height)
{
  int rang = 1 << (NSTEPS - size);
  height = height + (random () % rang) - rang / 2;
  height = HEIGHT_TO_PIXEL(height);
  CELL (l, c) = height;
  return st->colors[height].pixel;
}


static void
floyd_steinberg (struct state *st)
{
  int x, y, err;

  /* Instead of repeatedly calling XPutPixel(), we make an Image and then
     send its bits over all at once.  This consumes much less network
     bandwidth.  The image we create is Wx1 intead of WxH, so that we
     don't use enormous amounts of memory.
   */
  XImage *image =
    XCreateImage (st->dpy, st->xgwa.visual,
		  1, XYBitmap, 0,		/* depth, format, offset */
		  (char *) calloc ((st->xmax + 8) / 8, 1),	/* data */
		  st->xmax, 1, 8, 0);		/* w, h, pad, bpl */

  XSetForeground (st->dpy, st->gc, st->colors[0].pixel);
  XSetBackground (st->dpy, st->gc, st->colors[1].pixel);

  for (y = 0; y < st->ymax - 1; y++)
    {
      for (x = 0; x < st->xmax - 1; x++)
	{
	  if (CELL(x, y) < 0)
	    {
	      err = CELL (x, y);
	      XPutPixel (image, x, 0, 1);
	    }
	  else
	    {
	      err = CELL (x, y) - 1;
	      XPutPixel (image, x, 0, 0);
	    }
	  /* distribute error */
	  CELL (x,   y+1) += (int) (((float) err) * 3.0/8.0);
	  CELL (x+1, y)   += (int) (((float) err) * 3.0/8.0);
	  CELL (x+1, y+1) += (int) (((float) err) * 1.0/4.0);
	}
      XPutImage (st->dpy, st->window, st->gc, image, 0, 0, 0, y, st->xmax, 1);
    }
  XDestroyImage (image);
}


static void
draw (struct state *st,
      int x, int y, unsigned long pixel, int grid_size)
{
  if (st->flip_x)
    x = st->xmax - x;

  if (st->flip_xy)
    {
      int swap = x;
      x = y;
      y = swap;
    }

  if (! (st->last_valid && pixel == st->last_pixel))
    XSetForeground (st->dpy, st->gc, pixel);
  st->last_valid = 1, st->last_pixel = pixel;
  if (grid_size == 1)
    XDrawPoint (st->dpy, st->window, st->gc, x, y);
  else
    XFillRectangle (st->dpy, st->window, st->gc, x, y, grid_size, grid_size);
}


static void
init_map (struct state *st)
{
  XGCValues gcv;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->cmap = st->xgwa.colormap;

  st->flip_x  = (random() % 2);
  st->flip_xy = (random() % 2);

  if (mono_p)
    st->flip_xy = 0;
  else if (st->colors)
    free_colors (st->xgwa.screen, st->cmap, st->colors, st->ncolors);
  st->colors = 0;

  st->ncolors = get_integer_resource (st->dpy, "ncolors", "Integer");
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer");
  st->iterations = get_integer_resource (st->dpy, "iterations", "Integer");
  if (st->iterations < 0) st->iterations = 0;
  else if (st->iterations > 7) st->iterations = 7;

  if (st->ncolors <= 2) st->ncolors = 0;
  if (st->ncolors == 0) mono_p = True;
  if (st->ncolors > 255) st->ncolors = 255;  /* too many look bad */

  if (!st->gc)  st->gc  = XCreateGC (st->dpy, st->window, 0, &gcv);
  if (!st->gc2) st->gc2 = XCreateGC (st->dpy, st->window, 0, &gcv);

  if (mono_p)
    st->extra_krinkly_p = !(random() % 15);
  else
    st->extra_krinkly_p = !(random() % 5);

  if (!mono_p)
    {
      if (st->ncolors < 1) st->ncolors = 1;
      st->colors = (XColor *) malloc (st->ncolors * sizeof(*st->colors));

      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->cmap,
                            st->colors, &st->ncolors,
                            True, 0, False);
      if (st->ncolors <= 2)
	mono_p = 1;
    }

  if (mono_p)
    {
      int i;
      unsigned long fg_pixel = 
        get_pixel_resource (st->dpy, st->xgwa.colormap, 
                            "foreground", "Foreground");
      unsigned long bg_pixel = 
        get_pixel_resource (st->dpy, st->xgwa.colormap, 
                            "background", "Background");
      if (!st->colors)
        {
          st->ncolors = 50;
          st->colors = (XColor *) calloc (st->ncolors, sizeof(*st->colors));
        }
      st->colors[0].pixel = fg_pixel;
      for (i = 1; i < st->ncolors; i++)
        st->colors[i].pixel = bg_pixel;
    }

  XSetForeground (st->dpy, st->gc, st->colors[1].pixel);
  XFillRectangle (st->dpy, st->window, st->gc, 0, 0, 
                  st->xgwa.width, st->xgwa.height);

  if (st->flip_xy)
    {
      st->xmax = st->xgwa.height;
      st->ymax = st->xgwa.width;
    }
  else
    {
      st->xmax = st->xgwa.width;
      st->ymax = st->xgwa.height;
    }

  if (st->cell) free (st->cell);
  st->cell = (signed char *) calloc (st->xmax * st->ymax, 1);

  CELL (0, 0) = 0;
  st->xstep = COUNT;
  st->ystep = COUNT;

  st->iteration = 0;
  st->cx = 0;
}


static void *
imsmap_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  init_map (st);
  return st;
}


static unsigned long
imsmap_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay2;
  int i;

  /* do this many lines at a time without pausing */
  int col_chunk = st->iteration * 2 + 1;

  if (st->iteration > st->iterations)
    init_map (st);

  if (st->cx == 0)
    {
      st->xnextStep = st->xstep / 2;
      st->ynextStep = st->ystep / 2;
    }

  for (i = 0; i < col_chunk; i++)
    {
      int x1, x2, y1, y2;
      int y;
      int x = st->cx;

      x1 = x + st->xnextStep;
      if (x1 < 0)
        x1 = st->xmax-1;
      else if (x1 >= st->xmax)
        x1 = 0;

      x2 = x + st->xstep;
      if (x2 < 0)
        x2 = st->xmax-1;
      else if (x2 >= st->xmax)
        x2 = 0;

      for (y = 0; y < st->ymax; y += st->ystep)
        {
          unsigned int pixel, qpixels [4];

          y1 = y + st->ynextStep;
          if (y1 < 0)
            y1 = st->ymax-1;
          else if (y1 >= st->ymax)
            y1 = 0;

          y2 = y + st->ystep;
          if (y2 < 0)
            y2 = st->ymax-1;
          else if (y2 >= st->ymax)
            y2 = 0;

          qpixels [0] = st->colors [HEIGHT_TO_PIXEL (CELL (x, y))].pixel;
          qpixels [1] = st->colors [HEIGHT_TO_PIXEL (CELL (x, y2))].pixel;
          qpixels [2] = st->colors [HEIGHT_TO_PIXEL (CELL (x2, y))].pixel;
          qpixels [3] = st->colors [HEIGHT_TO_PIXEL (CELL (x2, y2))].pixel;

          pixel = set (st, x, y1, st->iteration,
                       ((int) CELL (x, y) + (int) CELL (x, y2) + 1) / 2);

          if (! mono_p &&
              (pixel != qpixels[0] || pixel != qpixels[1] || 
               pixel != qpixels[2] || pixel != qpixels[3]))
            draw (st, x, y1, pixel, st->ynextStep);

          pixel = set (st, x1, y, st->iteration,
                       ((int) CELL (x, y) + (int) CELL (x2, y) + 1) / 2);
          if (! mono_p &&
              (pixel != qpixels[0] || pixel != qpixels[1] || 
               pixel != qpixels[2] || pixel != qpixels[3]))
            draw (st, x1, y, pixel, st->ynextStep);

          pixel = set (st, x1, y1, st->iteration,
                       ((int) CELL (x, y) + (int) CELL (x, y2) +
                        (int) CELL (x2, y) + (int) CELL (x2, y2) + 2)
                       / 4);
          if (! mono_p &&
              (pixel != qpixels[0] || pixel != qpixels[1] || 
               pixel != qpixels[2] || pixel != qpixels[3]))
            draw (st, x1, y1, pixel, st->ynextStep);
        }

      st->cx += st->xstep;
      if (st->cx >= st->xmax)
        break;
    }

  if (st->cx >= st->xmax)
    {
      st->cx = 0;
      st->xstep = st->xnextStep;
      st->ystep = st->ynextStep;

      st->iteration++;

      if (st->iteration > st->iterations)
        this_delay = st->delay * 1000000;

      if (mono_p)
        floyd_steinberg (st);  /* in mono, do all drawing at the end */
    }

  return this_delay;
}


static void
imsmap_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  init_map (st);
}


static Bool
imsmap_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      init_map (st);
      return True;
    }

  return False;
}


static void
imsmap_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->colors) free (st->colors);
  if (st->cell) free (st->cell);
  free (st);
}


static const char *imsmap_defaults [] = {
  ".background:	#000066",
  ".foreground:	#FF00FF",
  "*fpsSolid:	true",
  "*mode:	random",
  "*ncolors:	50",
  "*iterations:	7",
  "*delay:	5",
  "*delay2:	20000",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec imsmap_options [] = {
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("IMSMap", imsmap)
