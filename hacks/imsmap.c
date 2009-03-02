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
 */

#include "screenhack.h"
#include <stdio.h>
#include <X11/Xutil.h>

#define NSTEPS 7
#define COUNT (1 << NSTEPS)
#define CELL(c, r) cell[((unsigned int)(c)) + ((unsigned int) (r)) * xmax]

static enum mode_t { MODE_H, MODE_S, MODE_V, MODE_RANDOM } mode;

static GC gc, gc2;
static Display *disp;
static Window wind;
static XWindowAttributes wattrs;

#if defined(sun) && !__STDC__	/* sun cc doesn't know "signed char" */
#define signed /**/
#endif

static unsigned long *pixels = 0, fg_pixel, bg_pixel;
static int npixels = 0;
static Colormap cmap;
static int timeout, cycle_delay;
static int cycle_p;
static signed char *cell = NULL;
static int xmax, ymax;
static int iterations;

static void
initwin (dsp, win)
     Display *dsp;
     Window win;
{
  int fg_h, bg_h;
  double fg_s, fg_v, bg_s, bg_v;

  enum mode_t this_mode;
  static Bool rv_p;
  static int ncolors = 0;
  int shift;
  double dshift;
    
  XGCValues gcv;

  XGetWindowAttributes (dsp, win, &wattrs);
  cmap = wattrs.colormap;

  if (!ncolors)
    {
      char *mode_str = get_string_resource ("mode", "Mode");
      rv_p = get_boolean_resource ("reverseVideo", "ReverseVideo");
      cycle_p = get_boolean_resource ("cycle", "Cycle");
      ncolors = get_integer_resource ("ncolors", "Integer");
      timeout = get_integer_resource ("timeout", "Integer");
      cycle_delay = get_integer_resource ("cycleDelay", "Integer");
      iterations = get_integer_resource ("iterations", "Integer");
      if (iterations < 0) iterations = 0;
      else if (iterations > 7) iterations = 7;
      pixels = (unsigned long *) calloc (ncolors, sizeof (unsigned int));
      fg_pixel = get_pixel_resource ("background", "Background", dsp, cmap);
      bg_pixel = get_pixel_resource ("foreground", "Foreground", dsp, cmap);

      if (mono_p && fg_pixel == bg_pixel)
	bg_pixel = !bg_pixel;

      if (mono_p) cycle_p = False;

      gcv.foreground = fg_pixel;
      gcv.background = bg_pixel;
      gc = XCreateGC (dsp, win, GCForeground|GCBackground, &gcv);
      gcv.foreground = bg_pixel;
      gc2 = XCreateGC (dsp, win, GCForeground, &gcv);

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
    else if (! mono_p)
      XFreeColors (dsp, cmap, pixels, npixels, 0);

  this_mode = mode;
  if (!mono_p && mode == MODE_RANDOM)
    switch (random () % 3) {
    case 0: this_mode = MODE_H; break;
    case 1: this_mode = MODE_S; break;
    case 2: this_mode = MODE_V; break;
    }

  if (mono_p)
    {
      npixels = ncolors;
      pixels [0] = fg_pixel;
      pixels [1] = bg_pixel;
    }
  else
    {
      XColor fg_color, bg_color;

      if (fg_pixel == bg_pixel)
	{
	HSV_AGAIN:
	  fg_h = random () % 360;
	  bg_h = random () % 360;
	  fg_s = frand (1.0);
	  bg_s = frand (1.0);
	V_AGAIN:
	  fg_v = frand (1.0);
	  bg_v = frand (1.0);
	  if ((fg_v - bg_v) > -0.4 && (fg_v - bg_v) < 0.4)
	    goto V_AGAIN;
	  hsv_to_rgb (fg_h, fg_s, fg_v, 
		      &fg_color.red, &fg_color.green, &fg_color.blue);
	  hsv_to_rgb (bg_h, bg_s, bg_v, 
		      &bg_color.red, &bg_color.green, &bg_color.blue);
	}
      else
	{
	  XQueryColor (dsp, cmap, &fg_color);
	  XQueryColor (dsp, cmap, &bg_color);
	  fg_color.pixel = fg_pixel;
	  bg_color.pixel = bg_pixel;
	}
      fg_color.flags = DoRed|DoGreen|DoBlue;
      bg_color.flags = DoRed|DoGreen|DoBlue;

      rgb_to_hsv (fg_color.red, fg_color.green, fg_color.blue,
		  &fg_h, &fg_s, &fg_v);
      rgb_to_hsv (bg_color.red, bg_color.green, bg_color.blue,
		  &bg_h, &bg_s, &bg_v);

      if (/*mode == MODE_RANDOM &&*/
	  ((this_mode == MODE_S && (fg_s-bg_s) > -0.3 && (fg_s-bg_s) < 0.3) ||
	   (this_mode == MODE_V && (fg_v-bg_v) > -0.3 && (fg_v-bg_v) < 0.3) ||
	   (this_mode == MODE_H && (fg_h-bg_h) > -30 && (fg_h-bg_h) < 30)))
	goto HSV_AGAIN;

      switch (this_mode) {
      case MODE_H:  shift = (bg_h - fg_h) / ncolors; break;
      case MODE_S: dshift = (bg_s - fg_s) / ncolors; break;
      case MODE_V: dshift = (bg_v - fg_v) / ncolors; break;
      default: abort ();
      }
      
      if (mode == MODE_RANDOM &&
	  ((this_mode == MODE_H)
	   ? ((shift > -2 && shift < 2) || fg_s < 0.3 || fg_v < 0.3)
	   : (dshift > -0.005 && dshift < 0.005)))
	goto HSV_AGAIN;

      if (mode == MODE_RANDOM && this_mode == MODE_S && fg_v < 0.5)
	goto V_AGAIN;

      for (npixels = 0; npixels < ncolors; npixels++)
	{
	  if (cycle_p)
	    {
	      unsigned long plane_masks;
	      /* allocate the writable color cells, one at a time. */
	      if (! XAllocColorCells (dsp, cmap, False, &plane_masks, 0,
				      &fg_color.pixel, 1))
		{
		  fprintf (stderr,
   "%s: couldn't allocate %s writable color cells.  Turning off -cycle.\n",
			   progname, (npixels ? "enough" : "any"));
		  cycle_p = 0;
		  goto NON_CYCLE;
		}
	      XStoreColor (dsp, cmap, &fg_color);
	    }
	  else
	    {
	    NON_CYCLE:
	      if (!XAllocColor (dsp, cmap, &fg_color))
		break;
	    }
	  pixels[npixels] = fg_color.pixel;
	
	  switch (this_mode)
	    {
	    case MODE_H: fg_h = (fg_h + shift) % 360; break;
	    case MODE_S: fg_s += dshift; break;
	    case MODE_V: fg_v += dshift; break;
	    default: abort ();
	    }
	  hsv_to_rgb (fg_h, fg_s, fg_v,
		      &fg_color.red, &fg_color.green, &fg_color.blue);
	}
    }
  XSetForeground (dsp, gc, pixels [0]);
  XFillRectangle (dsp, win, gc, 0, 0, wattrs.width, wattrs.height);
}


#define HEIGHT_TO_PIXEL(height) \
	(((int) (height)) < 0 ? 0 : \
	 ((int) (height)) >= npixels ? npixels - 3 : ((int) (height)))

static unsigned int
set (l, c, size, height)
     unsigned int l, c, size;
     int height;
{
  int rang = 1 << (NSTEPS - size);
  height = height + (random () % rang) - rang / 2;
  CELL (l, c) = height;

  return pixels [HEIGHT_TO_PIXEL (height)];
}

static void
floyd_steinberg ()
{
  int x, y, err;

  /* Instead of repeatedly calling XPutPixel(), we make an Image and then
     send its bits over all at once.  This consumes much less network
     bandwidth.  The image we create is Wx1 intead of WxH, so that we
     don't use enormous amounts of memory.
   */
  XImage *image =
    XCreateImage (disp, DefaultVisual(disp,DefaultScreen(disp)),
		  1, XYBitmap, 0,		/* depth, format, offset */
		  (char *) calloc ((xmax + 1) / 8, 1),	/* data */
		  xmax, 1, 8, 0);		/* w, h, pad, bpl */

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
      XPutImage (disp, wind, gc, image, 0, 0, 0, y, xmax, 1);
    }
  XDestroyImage (image);
}

static void
draw (x, y, pixel, grid_size)	/* not called in mono mode */
     int x, y, grid_size;
     unsigned long pixel;
{
  static unsigned int last_pixel, last_valid = 0;
  if (! (last_valid && pixel == last_pixel))
    XSetForeground (disp, gc, pixel);
  last_valid = 1, last_pixel = pixel;
  if (grid_size == 1)
    XDrawPoint (disp, wind, gc, x, y);
  else
    XFillRectangle (disp, wind, gc, x, y, grid_size, grid_size);
}


static void 
drawmap ()
{
  unsigned int x, y, i, step, nextStep, x1, x2, y1, y2;
  unsigned int pixel, qpixels [4];

  xmax = wattrs.width;
  ymax = wattrs.height;

  cell = (signed char *) calloc (xmax * ymax, 1);
  if (cell == NULL)
    exit (1);

  CELL (0, 0) = 0;
  step = COUNT;
  for (i = 0; i < iterations; i++)
    {
      nextStep = step / 2;
      for (x = 0; x < xmax; x += step)
	{
	  x1 = x + nextStep;
	  if (x1 >= xmax)
	    x1 = 0;
	  x2 = x + step;
	  if (x2 >= xmax)
	    x2 = 0;
	  for (y = 0; y < ymax; y += step)
	    {
	      y1 = y + nextStep;
	      if (y1 >= ymax)
		y1 = 0;
	      y2 = y + step;
	      if (y2 >= ymax)
		y2 = 0;

	      qpixels [0] = pixels [HEIGHT_TO_PIXEL (CELL (x, y))];
	      qpixels [1] = pixels [HEIGHT_TO_PIXEL (CELL (x, y2))];
	      qpixels [2] = pixels [HEIGHT_TO_PIXEL (CELL (x2, y))];
	      qpixels [3] = pixels [HEIGHT_TO_PIXEL (CELL (x2, y2))];

	      pixel = set (x, y1, i,
			   ((int) CELL (x, y) + (int) CELL (x, y2) + 1) / 2);
	      if (! mono_p &&
		  (pixel != qpixels[0] || pixel != qpixels[1] || 
		   pixel != qpixels[2] || pixel != qpixels[3]))
		draw (x, y1, pixel, nextStep);

	      pixel = set (x1, y, i,
			   ((int) CELL (x, y) + (int) CELL (x2, y) + 1) / 2);
	      if (! mono_p &&
		  (pixel != qpixels[0] || pixel != qpixels[1] || 
		   pixel != qpixels[2] || pixel != qpixels[3]))
		draw (x1, y, pixel, nextStep);

	      pixel = set (x1, y1, i,
			   ((int) CELL (x, y) + (int) CELL (x, y2) +
			    (int) CELL (x2, y) + (int) CELL (x2, y2) + 2)
			   / 4);
	      if (! mono_p &&
		  (pixel != qpixels[0] || pixel != qpixels[1] || 
		   pixel != qpixels[2] || pixel != qpixels[3]))
		draw (x1, y1, pixel, nextStep);
	    }
	}
      step = nextStep;
      if (!mono_p)
	XSync (disp, True);
    }
  if (mono_p)
    /* in mono-mode, we do all the drawing at the end */
    floyd_steinberg ();
  
  free (cell);
  XSync (disp, True);
}

static void
cycle (dpy)
     Display *dpy;
{
  XColor *colors = (XColor *) malloc (npixels * sizeof (XColor));
  time_t stop;
  int i;
  for (i = 0; i < npixels; i++)
    colors [i].pixel = pixels [i];
  XQueryColors (dpy, cmap, colors, npixels);
  stop = (time_t) ((time ((time_t) 0)) + timeout);
  while (stop >= (time_t) time ((time_t) 0))
    {
      unsigned long scratch = colors [npixels-1].pixel;
      for (i = npixels-1; i > 0; i--)
	colors [i].pixel = colors [i-1].pixel;
      colors [0].pixel = scratch;
      XStoreColors (dpy, cmap, colors, npixels);
      XSync (dpy, True);
      if (cycle_delay) usleep (cycle_delay);
    }
  XSync (dpy, True);
  free (colors);
}


char *progclass = "Imsmap";

char *defaults [] = {
  "*background:	black",
  "*foreground:	black",
  "*mode:	random",
  "*ncolors:	50",
  "*iterations:	7",
  "*timeout:	10",
  "*cycleDelay:	100000",
  "*cycle:	true",
  0
};

XrmOptionDescRec options [] = {
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-timeout",		".timeout",	XrmoptionSepArg, 0 },
  { "-cycle-delay",	".cycleDelay",	XrmoptionSepArg, 0 },
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionNoArg, "True"  },
  { "-no-cycle",	".cycle",	XrmoptionNoArg, "False" }
};
int options_size = (sizeof (options) / sizeof (options[0]));


void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
    disp = dpy;
    wind = window;
    while (1)
      {
	initwin (dpy, window);
	drawmap ();
	if (timeout)
	  {
	    if (cycle_p)
	      cycle (dpy);
	    else
	      sleep (timeout);
	  }
      }
}
