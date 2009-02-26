/* xscreensaver, Copyright (c) 1993, 1995, 1996, 1997
 *  Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* I wanted to lay down new circles with TV:ALU-ADD instead of TV:ALU-XOR,
   but X doesn't support arithmetic combinations of pixmaps!!  What losers.
   I suppose I could crank out the 2's compliment math by hand, but that's
   a real drag...

   This would probably look good with shapes other than circles as well.

 */

#include "screenhack.h"
#include <stdio.h>

struct circle {
  int x, y, radius;
  int increment;
  int dx, dy;
};

static enum color_mode {
  seuss_mode, ramp_mode, random_mode
} cmode;


static struct circle *circles;
static int count, global_count;
static Pixmap pixmap, buffer;
static int width, height, global_inc;
static int delay, delay2, cycle_delay;
static unsigned long fg_pixel, bg_pixel;
static GC draw_gc, erase_gc, copy_gc, merge_gc;
static Bool anim_p;
static Colormap cmap;

static int ncolors;
static XColor *colors;
static Bool cycle_p;
static int fg_index;
static int bg_index;


#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

static void
init_circles_1 (Display *dpy, Window window)
{
  int i;
  count = (global_count ? global_count
	   : (3 + (random () % max (1, (min (width, height) / 50)))
	        + (random () % max (1, (min (width, height) / 50)))));
  circles = (struct circle *) malloc (count * sizeof (struct circle));
  for (i = 0; i < count; i++)
    {
      circles [i].x = 10 + random () % (width - 20);
      circles [i].y = 10 + random () % (height - 20);
      if (global_inc)
      circles [i].increment = global_inc;
      else
	{ /* prefer smaller increments to larger ones */
	  int j = 8;
	  int inc = ((random()%j) + (random()%j) + (random()%j)) - ((j*3)/2);
	  if (inc < 0) inc = -inc + 3;
	  circles [i].increment = inc + 3;
	}
      circles [i].radius = random () % circles [i].increment;
      circles [i].dx = ((random () % 3) - 1) * (1 + random () % 5);
      circles [i].dy = ((random () % 3) - 1) * (1 + random () % 5);
    }
}

static void
init_circles (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *mode_str = 0;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  global_count = get_integer_resource ("count", "Integer");
  if (global_count < 0) global_count = 0;
  global_inc = get_integer_resource ("increment", "Integer");
  if (global_inc < 0) global_inc = 0;
  anim_p = get_boolean_resource ("animate", "Boolean");
  delay = get_integer_resource ("delay", "Integer");
  delay2 = get_integer_resource ("delay2", "Integer") * 1000000;
  cycle_delay = get_integer_resource ("cycleDelay", "Integer");
  mode_str = get_string_resource ("colorMode", "ColorMode");
  if (! mode_str) cmode = random_mode;
  else if (!strcmp (mode_str, "seuss"))  cmode = seuss_mode;
  else if (!strcmp (mode_str, "ramp"))   cmode = ramp_mode;
  else if (!strcmp (mode_str, "random")) cmode = random_mode;
  else {
    fprintf (stderr,
	     "%s: colorMode must be seuss, ramp, or random, not \"%s\"\n",
	     progname, mode_str);
    exit (1);
  }

  if (mono_p) cmode = seuss_mode;
  if (cmode == random_mode)
    cmode = ((random()&3) == 1) ? ramp_mode : seuss_mode;

  if (cmode == ramp_mode)
    anim_p = False;    /* This combo doesn't work right... */

  ncolors = get_integer_resource ("colors", "Colors");
  if (ncolors < 2) ncolors = 2;
  if (ncolors <= 2) mono_p = True;

  if (mono_p)
    colors = 0;
  else
    colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));

  cycle_p = mono_p ? False : get_boolean_resource ("cycle", "Cycle");


  if (mono_p)
    ;
  else if (random() % (cmode == seuss_mode ? 2 : 10))
    make_uniform_colormap (dpy, xgwa.visual, cmap, colors, &ncolors,
			   True, &cycle_p, True);
  else
    make_smooth_colormap (dpy, xgwa.visual, cmap, colors, &ncolors,
			  True, &cycle_p, True);

  if (ncolors <= 2) mono_p = True;
  if (mono_p) cycle_p = False;
  if (mono_p) cmode = seuss_mode;

  if (mono_p)
    {
      fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
      bg_pixel = get_pixel_resource ("background", "Background", dpy, cmap);
    }
  else
    {
      fg_index = 0;
      bg_index = ncolors / 4;
      if (fg_index == bg_index) bg_index++;
      fg_pixel = colors[fg_index].pixel;
      bg_pixel = colors[bg_index].pixel;
    }

  width = max (50, xgwa.width);
  height = max (50, xgwa.height);

#ifdef DEBUG
  width/=2; height/=2;
#endif

  pixmap = XCreatePixmap (dpy, window, width, height, 1);
  if (cmode == seuss_mode)
    buffer = XCreatePixmap (dpy, window, width, height, 1);
  else
    buffer = 0;

  gcv.foreground = 1;
  gcv.background = 0;
  draw_gc = XCreateGC (dpy, pixmap, GCForeground | GCBackground, &gcv);
  gcv.foreground = 0;
  erase_gc = XCreateGC (dpy, pixmap, GCForeground, &gcv);
  gcv.foreground = fg_pixel;
  gcv.background = bg_pixel;
  copy_gc = XCreateGC (dpy, window, GCForeground | GCBackground, &gcv);

  if (cmode == seuss_mode)
    {
      gcv.foreground = 1;
      gcv.background = 0;
      gcv.function = GXxor;
      merge_gc = XCreateGC (dpy, pixmap,
			    GCForeground | GCBackground | GCFunction, &gcv);
    }
  else
    {
      gcv.foreground = fg_pixel;
      gcv.background = bg_pixel;
      gcv.function = GXcopy;
      merge_gc = XCreateGC (dpy, window,
			    GCForeground | GCBackground | GCFunction, &gcv);
    }

  init_circles_1 (dpy, window);
  XClearWindow (dpy, window);
  if (buffer) XFillRectangle (dpy, buffer, erase_gc, 0, 0, width, height);
}

static void
run_circles (Display *dpy, Window window)
{
  int i;
  static int iterations = 0;
  static int oiterations = 0;
  static Bool first_time_p = True;
  Bool done = False;
  Bool inhibit_sleep = False;
  XFillRectangle (dpy, pixmap, erase_gc, 0, 0, width, height);
  for (i = 0; i < count; i++)
    {
      int radius = circles [i].radius;
      int inc = circles [i].increment;

      if (! (iterations & 1))  /* never stop on an odd number of iterations */
	;
      else if (radius == 0)	/* eschew inf */
	;
      else if (radius < 0)	/* stop when the circles are points */
	done = True;
      else			/* stop when the circles fill the window */
	{
	  /* Probably there's a simpler way to ask the musical question,
	     "is this square completely enclosed by this circle," but I've
	     forgotten too much trig to know it...  (That's not really the
	     right question anyway, but the right question is too hard.) */
	  double x1 = ((double) (-circles [i].x)) / ((double) radius);
	  double y1 = ((double) (-circles [i].y)) / ((double) radius);
	  double x2 = ((double) (width - circles [i].x)) / ((double) radius);
	  double y2 = ((double) (height - circles [i].y)) / ((double) radius);
	  x1 *= x1; x2 *= x2; y1 *= y1; y2 *= y2;
	  if ((x1 + y1) < 1 && (x2 + y2) < 1 && (x1 + y2) < 1 && (x2 + y1) < 1)
	    done = True;
	}

      if (radius > 0 &&
	  (cmode == seuss_mode ||	/* drawing all circles, or */
	   circles [0].increment < 0))	/* on the way back in */
	{
	  XFillArc (dpy,
		    (cmode == seuss_mode ? pixmap : window),
		    (cmode == seuss_mode ? draw_gc : merge_gc),
		    circles [i].x - radius, circles [i].y - radius,
		    radius * 2, radius * 2, 0, 360*64);
	}
      circles [i].radius += inc;
    }

  if (cycle_p && cmode != seuss_mode)
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
	  rotate_colors (dpy, cmap, colors, ncolors, 1);
	  then = now;
	}
    }

  if (anim_p && !first_time_p)
    inhibit_sleep = !done;

  if (done)
    {
      if (anim_p)
	{
	  first_time_p = False;
	  for (i = 0; i < count; i++)
	    {
	      circles [i].x += circles [i].dx;
	      circles [i].y += circles [i].dy;
	      circles [i].radius %= circles [i].increment;
	      if (circles [i].x < 0 || circles [i].x >= width)
		{
		  circles [i].dx = -circles [i].dx;
		  circles [i].x += (2 * circles [i].dx);
		}
	      if (circles [i].y < 0 || circles [i].y >= height)
		{
		  circles [i].dy = -circles [i].dy;
		  circles [i].y += (2 * circles [i].dy);
		}
	    }
	}
      else if (circles [0].increment < 0)
	{
	  /* We've zoomed out and the screen is blank -- re-pick the
	     center points, and shift the colors.
	   */
	  free (circles);
	  init_circles_1 (dpy, window);
	  if (! mono_p)
	    {
	      fg_index = (fg_index + 1) % ncolors;
	      bg_index = (fg_index + (ncolors/2)) % ncolors;
	      XSetForeground (dpy, copy_gc, colors [fg_index].pixel);
	      XSetBackground (dpy, copy_gc, colors [bg_index].pixel);
	    }
	}
#if 1
      /* Sometimes go out from the inside instead of the outside */
      else if ((random () % 10) == 0)
	{
# if 0
	  if (! mono_p)
	    {
	      unsigned long swap = fg_index;
	      fg_index = bg_index;
	      bg_index = swap;
	      XSetForeground (dpy, copy_gc, colors [fg_index].pixel);
	      XSetBackground (dpy, copy_gc, colors [bg_index].pixel);
	    }
# endif
	  iterations = 0; /* ick */
	  for (i = 0; i < count; i++)
	    circles [i].radius %= circles [i].increment;
	}
#endif
      else
	{
	  oiterations = iterations;
	  for (i = 0; i < count; i++)
	    {
	      circles [i].increment = -circles [i].increment;
	      circles [i].radius += (2 * circles [i].increment);
	    }
	}
    }

  if (buffer)
    XCopyPlane (dpy, pixmap, buffer, merge_gc, 0, 0, width, height, 0, 0, 1);
  else if (cmode != seuss_mode)
    {

      if (!mono_p)
	{
	  fg_index++;
	  bg_index++;
	  if (fg_index >= ncolors) fg_index = 0;
	  if (bg_index >= ncolors) bg_index = 0;
	  XSetForeground (dpy, merge_gc, colors [fg_index].pixel);
	}

      if (circles [0].increment >= 0)
	inhibit_sleep = True;
      else if (done && cmode == seuss_mode)
	XFillRectangle (dpy, window, merge_gc, 0, 0, width, height);
    }
  else
    XCopyPlane (dpy, pixmap, window, merge_gc, 0, 0, width, height, 0, 0, 1);

  /* buffer is only used in seuss-mode or anim-mode */
  if (buffer && (anim_p
		 ? (done || (first_time_p && (iterations & 1)))
		 : (iterations & 1)))
    {
      XCopyPlane (dpy, buffer, window, copy_gc, 0, 0, width, height, 0, 0, 1);
      XSync (dpy, True);
      if (anim_p && done)
	XFillRectangle (dpy, buffer, erase_gc, 0, 0, width, height);
    }

#ifdef DEBUG
  XCopyPlane (dpy, pixmap, window, copy_gc, 0,0,width,height,width,height, 1);
  if (buffer)
    XCopyPlane (dpy, buffer, window, copy_gc, 0,0,width,height,0,height, 1);
  XSync (dpy, True);
#endif

  if (done)
    iterations = 0;
  else
    iterations++;

  if (delay && !inhibit_sleep)
    {
      static Bool really_first_p = True;
      int direction = 1;
      int d = delay;
      if (done && cycle_p && cmode != seuss_mode && !really_first_p)
	{
	  d = delay2;
	  if (! (random() % 10))
	    direction = -1;
	}
      if (done)
	really_first_p = False;

      XSync(dpy, False);

      if (cycle_p && cycle_delay)
	{
	  int i = 0;
	  while (i < d)
	    {
	      rotate_colors (dpy, cmap, colors, ncolors, direction);
	      usleep(cycle_delay);
	      i += cycle_delay;
	    }
	}
      else
	usleep (d);
    }
}


char *progclass = "Halo";

char *defaults [] = {
  "*background:		black",
  "*foreground:		white",
  "*colorMode:		random",
  "*colors:		100",
  "*cycle:		true",
  "*count:		0",
  "*delay:		100000",
  "*delay2:		20",
  "*cycleDelay:		100000",
  0
};

XrmOptionDescRec options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-cycle-delay",	".cycleDelay",	XrmoptionSepArg, 0 },
  { "-animate",		".animate",	XrmoptionNoArg, "True" },
  { "-mode",		".colorMode",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionNoArg, "True" },
  { "-no-cycle",	".cycle",	XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  init_circles (dpy, window);
  while (1)
    run_circles (dpy, window);
}
