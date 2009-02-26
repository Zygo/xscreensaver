/* imsmap, Copyright (c) 1992 Juergen Nickelsen <nickel@cs.tu-berlin.de>
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
#include <sys/time.h> /* for gettimeofday() */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "screenhack.h"

#define NSTEPS 7
#define COUNT (1 << NSTEPS)
#define CELL(c, r) cell[((unsigned int)(c)) + ((unsigned int) (r)) * xmax]

static enum imsmap_mode { MODE_H, MODE_S, MODE_V, MODE_RANDOM } mode;

static GC gc, gc2;
static XWindowAttributes xgwa;

#if defined(sun) && !__STDC__	/* sun cc doesn't know "signed char" */
#define signed /**/
#endif

static Colormap cmap;
static int ncolors;
static XColor *colors;
static Bool cycle_p;
static int cycle_direction;
static Bool extra_krinkly_p;

static int delay, cycle_delay;
static signed char *cell = NULL;
static int xmax, ymax;
static int iterations;

static void
init_map (Display *dpy, Window window)
{
  unsigned long fg_pixel = 0, bg_pixel = 0;
  int fg_h, bg_h;
  double fg_s, fg_v, bg_s, bg_v;

  enum imsmap_mode this_mode;
  static Bool rv_p;
    
  XGCValues gcv;

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;

  if (!ncolors)
    {
      char *mode_str = get_string_resource ("mode", "Mode");
      rv_p = get_boolean_resource ("reverseVideo", "ReverseVideo");
      cycle_p = get_boolean_resource ("cycle", "Cycle");
      ncolors = get_integer_resource ("ncolors", "Integer");
      delay = get_integer_resource ("delay", "Integer");
      cycle_delay = get_integer_resource ("cycleDelay", "Integer");
      iterations = get_integer_resource ("iterations", "Integer");
      if (iterations < 0) iterations = 0;
      else if (iterations > 7) iterations = 7;

      if (ncolors <= 2) ncolors = 0;
      if (ncolors == 0) mono_p = True;

      fg_pixel = get_pixel_resource ("background", "Background", dpy, cmap);
      bg_pixel = get_pixel_resource ("foreground", "Foreground", dpy, cmap);

      if (fg_pixel == bg_pixel)
	{
	  XColor black, white;
	  black.red = black.green = black.blue = 0;
	  white.red = white.green = white.blue = 0xFFFF;
	  black.flags = white.flags = DoRed|DoGreen|DoBlue;
	  XAllocColor(dpy, cmap, &black);
	  XAllocColor(dpy, cmap, &white);
	  if (bg_pixel == black.pixel)
	    fg_pixel = white.pixel;
	  else
	    fg_pixel = black.pixel;
	}

      if (mono_p) cycle_p = False;

      gcv.foreground = fg_pixel;
      gcv.background = bg_pixel;
      gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);
      gcv.foreground = bg_pixel;
      gc2 = XCreateGC (dpy, window, GCForeground, &gcv);

      if (!mode_str || !strcmp (mode_str, "random"))
	mode = MODE_RANDOM;
      else if (!strcmp (mode_str, "h") || !strcmp (mode_str, "hue"))
	mode = MODE_H;
      else if (!strcmp (mode_str, "s") || !strcmp (mode_str, "saturation"))
	mode = MODE_S;
      else if (!strcmp (mode_str, "v") || !strcmp (mode_str, "value"))
	mode = MODE_V;
      else
	{
	  fprintf (stderr,
	   "%s: mode must be hue, saturation, value, or random, not \"%s\"\n",
		   progname, mode_str);
	  mode = MODE_RANDOM;
	}
    }

  this_mode = mode;
  if (!mono_p && mode == MODE_RANDOM)
    switch (random () % 6) {
    case 0: this_mode = MODE_H; break;
    case 1: this_mode = MODE_S; break;
    case 2: this_mode = MODE_V; break;
    default: break;
    }

  if (mono_p)
    extra_krinkly_p = !(random() % 15);
  else
    extra_krinkly_p = !(random() % 5);

  if (!mono_p)
    {
      double distance, fg_H, bg_H, dh;

    RETRY:
      fg_h = random() % 360;
      fg_s = frand(1.0);
      fg_v = frand(1.0);

      bg_h = fg_h;
      bg_s = fg_s;
      bg_v = fg_v;

      switch (this_mode)
	{
	case MODE_H:
	  bg_h = random() % 360;
	  if (fg_v < 0.4)
	    goto RETRY;
	  distance = fg_h - bg_h;
	  if (distance < 0)
	    distance = -distance;
	  if (distance > 360)
	    distance = 180 - (distance - 180);
	  if (distance < 30)
	    goto RETRY;
	  break;

	case MODE_S:
	  bg_s = frand(1.0);
	  if (fg_v < 0.4)
	    goto RETRY;
	  distance = fg_s - bg_s;
	  if (distance < 0)
	    distance = -distance;
	  if (distance < 0.2)
	    goto RETRY;
	  break;

	case MODE_V:
	  bg_v = frand(1.0);
	  distance = fg_v - bg_v;
	  if (distance < 0)
	    distance = -distance;
	  if (distance < 0.4)
	    goto RETRY;
	  break;

	default:
	  bg_h = random() % 360;
	  bg_s = frand(1.0);
	  bg_v = frand(1.0);

	  fg_H = ((double) fg_h) / 360;
	  bg_H = ((double) bg_h) / 360;
	  dh = fg_H - bg_H;
	  if (dh < 0) dh = -dh;
	  if (dh > 0.5) dh = 0.5 - (dh - 0.5);
	  distance = sqrt ((dh * dh) +
			   ((fg_s - bg_s) * (fg_s - bg_s)) +
			   ((fg_v - bg_v) * (fg_v - bg_v)));
	  if (distance < 0.2)
	    goto RETRY;
	}

      cycle_p = True;
      if (colors)
	free_colors (dpy, cmap, colors, ncolors);
      else
	colors = (XColor *) malloc (ncolors * sizeof(*colors));

      cycle_direction = (random() & 1 ? 1 : -1);

    RETRY_NON_WRITABLE:
      {
	int n = ncolors;
	make_color_ramp (dpy, cmap,
			 fg_h, fg_s, fg_v,
			 bg_h, bg_s, bg_v,
			 colors, &n,
			 True, True, cycle_p);
	if (n == 0 && cycle_p)
	  {
	    cycle_p = False;
	    goto RETRY_NON_WRITABLE;
	  }
	ncolors = n;
      }

      if (ncolors <= 0)
	mono_p = 1;
    }

  if (mono_p)
    {
      static Bool done = False;
      static XColor c[50];
	  colors = c;
      cycle_p = False;
      ncolors = sizeof(c)/sizeof(*c);
      if (!done)
	{
	  int i;
	  done = True;
	  colors[0].pixel = fg_pixel;
	  for (i = 1; i < ncolors; i++)
	    colors[i].pixel = bg_pixel;
	}
    }

  XSetForeground (dpy, gc, colors[1].pixel);
  XFillRectangle (dpy, window, gc, 0, 0, xgwa.width, xgwa.height);
}


#define HEIGHT_TO_PIXEL(height)				\
	((height) < 0					\
	 ? (extra_krinkly_p				\
	    ? ncolors - ((-(height)) % ncolors)		\
	    : 0)					\
	 : ((height) >= ncolors				\
	    ? (extra_krinkly_p				\
	       ? (height) % ncolors			\
	       : ncolors-1)				\
	    : (height)))


static unsigned int
set (unsigned int l,
     unsigned int c,
     unsigned int size,
     int height)
{
  int rang = 1 << (NSTEPS - size);
  height = height + (random () % rang) - rang / 2;
  height = HEIGHT_TO_PIXEL(height);
  CELL (l, c) = height;
  return colors[height].pixel;
}

static void
floyd_steinberg (Display *dpy, Window window)
{
  int x, y, err;

  /* Instead of repeatedly calling XPutPixel(), we make an Image and then
     send its bits over all at once.  This consumes much less network
     bandwidth.  The image we create is Wx1 intead of WxH, so that we
     don't use enormous amounts of memory.
   */
  XImage *image =
    XCreateImage (dpy, xgwa.visual,
		  1, XYBitmap, 0,		/* depth, format, offset */
		  (char *) calloc ((xmax + 8) / 8, 1),	/* data */
		  xmax, 1, 8, 0);		/* w, h, pad, bpl */

  XSetForeground (dpy, gc, colors[0].pixel);
  XSetBackground (dpy, gc, colors[1].pixel);

  for (y = 0; y < ymax - 1; y++)
    {
      for (x = 0; x < xmax - 1; x++)
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
      XPutImage (dpy, window, gc, image, 0, 0, 0, y, xmax, 1);
    }
  XDestroyImage (image);
}

static void
draw (Display *dpy, Window window,
      int x, int y, unsigned long pixel, int grid_size)
{
  static unsigned int last_pixel, last_valid = 0;
  if (! (last_valid && pixel == last_pixel))
    XSetForeground (dpy, gc, pixel);
  last_valid = 1, last_pixel = pixel;
  if (grid_size == 1)
    XDrawPoint (dpy, window, gc, x, y);
  else
    XFillRectangle (dpy, window, gc, x, y, grid_size, grid_size);
}


static void 
draw_map (Display *dpy, Window window)
{
  int xstep, ystep, xnextStep, ynextStep;
  int x, y, i, x1, x2, y1, y2;
  unsigned int pixel, qpixels [4];

  int backwards = random() & 1;

  xmax = xgwa.width;
  ymax = xgwa.height;

  cell = (signed char *) calloc (xmax * ymax, 1);
  if (cell == NULL)
    exit (1);

  CELL (0, 0) = 0;
  xstep = (backwards ? -COUNT : COUNT);
  ystep = COUNT;
  for (i = 0; i < iterations; i++)
    {
      xnextStep = xstep / 2;
      ynextStep = ystep / 2;
      for (x = (backwards ? xmax-1 : 0);
	   (backwards ? x >= 0 : x < xmax);
	   x += xstep)
	{
	  x1 = x + xnextStep;
	  if (x1 < 0)
	    x1 = xmax-1;
	  else if (x1 >= xmax)
	    x1 = 0;

	  x2 = x + xstep;
	  if (x2 < 0)
	    x2 = xmax-1;
	  else if (x2 >= xmax)
	    x2 = 0;

	  for (y = 0; y < ymax; y += ystep)
	    {
	      y1 = y + ynextStep;
	      if (y1 < 0)
		y1 = ymax-1;
	      else if (y1 >= ymax)
		y1 = 0;

	      y2 = y + ystep;
	      if (y2 < 0)
		y2 = ymax-1;
	      else if (y2 >= ymax)
		y2 = 0;

	      qpixels [0] = colors [HEIGHT_TO_PIXEL (CELL (x, y))].pixel;
	      qpixels [1] = colors [HEIGHT_TO_PIXEL (CELL (x, y2))].pixel;
	      qpixels [2] = colors [HEIGHT_TO_PIXEL (CELL (x2, y))].pixel;
	      qpixels [3] = colors [HEIGHT_TO_PIXEL (CELL (x2, y2))].pixel;

	      pixel = set (x, y1, i,
			   ((int) CELL (x, y) + (int) CELL (x, y2) + 1) / 2);
	      if (! mono_p &&
		  (pixel != qpixels[0] || pixel != qpixels[1] || 
		   pixel != qpixels[2] || pixel != qpixels[3]))
		draw (dpy, window, x, y1, pixel, ynextStep);

	      pixel = set (x1, y, i,
			   ((int) CELL (x, y) + (int) CELL (x2, y) + 1) / 2);
	      if (! mono_p &&
		  (pixel != qpixels[0] || pixel != qpixels[1] || 
		   pixel != qpixels[2] || pixel != qpixels[3]))
		draw (dpy, window, x1, y, pixel, ynextStep);

	      pixel = set (x1, y1, i,
			   ((int) CELL (x, y) + (int) CELL (x, y2) +
			    (int) CELL (x2, y) + (int) CELL (x2, y2) + 2)
			   / 4);
	      if (! mono_p &&
		  (pixel != qpixels[0] || pixel != qpixels[1] || 
		   pixel != qpixels[2] || pixel != qpixels[3]))
		draw (dpy, window, x1, y1, pixel, ynextStep);


	      if (cycle_p)
		{
		  struct timeval now;
		  static struct timeval then = { 0, };
		  unsigned long diff;
#ifdef GETTIMEOFDAY_TWO_ARGS
		  struct timezone tzp;
		  gettimeofday(&now, &tzp);
#else
		  gettimeofday(&now);
#endif
		  diff = (((now.tv_sec - then.tv_sec)  * 1000000) +
			  (now.tv_usec - then.tv_usec));
		  if (diff > cycle_delay)
		    {
		      rotate_colors (dpy, cmap, colors, ncolors,
				     cycle_direction);
		      then = now;
		    }
		}
	    }
	}
      xstep = xnextStep;
      ystep = ynextStep;
      if (!mono_p)
	XSync (dpy, False);
      screenhack_handle_events (dpy);
    }
  if (mono_p)
    /* in mono-mode, we do all the drawing at the end */
    floyd_steinberg (dpy, window);
  
  free (cell);
  XSync (dpy, False);
}


char *progclass = "Imsmap";

char *defaults [] = {
  ".background:	black",
  ".foreground:	black",
  "*mode:	random",
  "*ncolors:	50",
  "*iterations:	7",
  "*delay:	10",
  "*cycleDelay:	100000",
  "*cycle:	true",
  0
};

XrmOptionDescRec options [] = {
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-cycle-delay",	".cycleDelay",	XrmoptionSepArg, 0 },
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionNoArg, "True"  },
  { "-no-cycle",	".cycle",	XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
    while (1)
      {
	init_map (dpy, window);
	draw_map (dpy, window);
	if (delay)
	  {
	    if (cycle_p)
	      {
		time_t start = time((time_t) 0);
		while (start + delay > time((time_t) 0))
		  {
		    rotate_colors (dpy, cmap, colors, ncolors,
				   cycle_direction);
		    if (cycle_delay) usleep(cycle_delay);
                    screenhack_handle_events (dpy);
		  }
	      }
	    else
              {
                screenhack_handle_events (dpy);
                sleep (delay);
              }
	  }
      }
}
