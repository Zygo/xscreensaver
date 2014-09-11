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

/* Algorithm from a Mac program by Chris Tate, written in 1988 or so. */

/* 18-Sep-97: Johannes Keukelaar (johannes@nada.kth.se): Improved screen
 *            eraser.
 * 10-May-97: merged ellipse code by Dan Stromberg <strombrg@nis.acs.uci.edu>
 *            as found in xlockmore 4.03a10.
 * 1992:      jwz created.
 */

/* 25 April 2002: Matthew Strait <straitm@mathcs.carleton.edu> added
-subdelay option so the drawing process can be watched */

#include <math.h>
#include "screenhack.h"
#include "erase.h"

enum draw_state { HELIX, DRAW_HELIX, TRIG, DRAW_TRIG, LINGER, ERASE };

struct state {
  enum draw_state dstate;
  double sins [360];
  double coss [360];

  GC draw_gc;
  unsigned int default_fg_pixel;
  int sleep_time;
  int subdelay;
  eraser_state *eraser;

  int width, height;
  Colormap cmap;

  int x1, y1, x2, y2, angle, i;

  int radius1, radius2, d_angle, factor1, factor2, factor3, factor4;
  int d_angle_offset;
  int offset, dir, density;
};

static void *
helix_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  XGCValues gcv;
  XWindowAttributes xgwa;

  st->sleep_time = get_integer_resource(dpy, "delay", "Integer");
  st->subdelay = get_integer_resource(dpy, "subdelay", "Integer");

  XGetWindowAttributes (dpy, window, &xgwa);
  st->width = xgwa.width;
  st->height = xgwa.height;
  st->cmap = xgwa.colormap;
  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource (dpy, st->cmap, "foreground", "Foreground");
  st->draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource (dpy, st->cmap, "background", "Background");

  for (i = 0; i < 360; i++)
    {
      st->sins [i] = sin ((((double) i) / 180.0) * M_PI);
      st->coss [i] = cos ((((double) i) / 180.0) * M_PI);
    }

  st->dstate = (random() & 1) ? HELIX : TRIG;

  return st;
}

static int
gcd (int a, int b)
{
  while (b > 0)
    {
      int tmp;
      tmp = a % b;
      a = b;
      b = tmp;
    }
  return (a < 0 ? -a : a);
}

static void
helix (Display *dpy, Window window, struct state *st)
{
  int xmid = st->width / 2;
  int ymid = st->height / 2;
  int limit = 1 + (360 / gcd (360, st->d_angle));

  if (st->i == 0)
    {
      st->x1 = xmid;
      st->y1 = ymid + st->radius2;
      st->x2 = xmid;
      st->y2 = ymid + st->radius1;
      st->angle = 0;
    }
  
/*  for (st->i = 0; st->i < limit; st->i++)*/
    {
      int tmp;
#define pmod(x,y) (tmp=((x) % (y)), (tmp >= 0 ? tmp : (tmp + (y))))

      st->x1 = xmid + (((double) st->radius1) * st->sins [pmod ((st->angle * st->factor1), 360)]);
      st->y1 = ymid + (((double) st->radius2) * st->coss [pmod ((st->angle * st->factor2), 360)]);
      XDrawLine (dpy, window, st->draw_gc, st->x1, st->y1, st->x2, st->y2);
      st->x2 = xmid + (((double) st->radius2) * st->sins [pmod ((st->angle * st->factor3), 360)]);
      st->y2 = ymid + (((double) st->radius1) * st->coss [pmod ((st->angle * st->factor4), 360)]);
      XDrawLine (dpy, window, st->draw_gc, st->x1, st->y1, st->x2, st->y2);
      st->angle += st->d_angle;
    }
    st->i++;

    if (st->i >= limit)
      st->dstate = LINGER;
}

static void
trig (Display *dpy, Window window, struct state *st)
{
  int xmid = st->width / 2;
  int ymid = st->height / 2;

/*  while (st->d_angle >= -360 && st->d_angle <= 360)*/
    {
      int tmp;
      int angle = st->d_angle + st->d_angle_offset;
      st->x1 = (st->sins [pmod(angle * st->factor1, 360)] * xmid) + xmid;
      st->y1 = (st->coss [pmod(angle * st->factor1, 360)] * ymid) + ymid;
      st->x2 = (st->sins [pmod(angle * st->factor2 + st->offset, 360)] * xmid) + xmid;
      st->y2 = (st->coss [pmod(angle * st->factor2 + st->offset, 360)] * ymid) + ymid;
      XDrawLine(dpy, window, st->draw_gc, st->x1, st->y1, st->x2, st->y2);
      tmp = (int) 360 / (2 * st->density * st->factor1 * st->factor2);
      if (tmp == 0)	/* Do not want it getting stuck... */
	tmp = 1;	/* Would not need if floating point */
      st->d_angle += st->dir * tmp;
    }

  if (st->d_angle < -360 || st->d_angle > 360)
    st->dstate = LINGER;
}


#define min(a,b) ((a)<(b)?(a):(b))

static void
random_helix (Display *dpy, Window window, struct state *st,
              XColor *color, Bool *got_color)
{
  int radius;
  double divisor;

  radius = min (st->width, st->height) / 2;

  st->i = 0;
  st->d_angle = 0;
  st->factor1 = 2;
  st->factor2 = 2;
  st->factor3 = 2;
  st->factor4 = 2;

  divisor = ((frand (3.0) + 1) * (((random() & 1) * 2) - 1));

  if ((random () & 1) == 0)
    {
      st->radius1 = radius;
      st->radius2 = radius / divisor;
    }
  else
    {
      st->radius2 = radius;
      st->radius1 = radius / divisor;
    }

  while (gcd (360, st->d_angle) >= 2)
    st->d_angle = random () % 360;

#define random_factor()				\
  (((random() % 7) ? ((random() & 1) + 1) : 3)	\
   * (((random() & 1) * 2) - 1))

  while (gcd (gcd (gcd (st->factor1, st->factor2), st->factor3), st->factor4) != 1)
    {
      st->factor1 = random_factor ();
      st->factor2 = random_factor ();
      st->factor3 = random_factor ();
      st->factor4 = random_factor ();
    }

  if (mono_p)
    XSetForeground (dpy, st->draw_gc, st->default_fg_pixel);
  else
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &color->red, &color->green, &color->blue);
      if ((*got_color = XAllocColor (dpy, st->cmap, color)))
	XSetForeground (dpy, st->draw_gc, color->pixel);
      else
	XSetForeground (dpy, st->draw_gc, st->default_fg_pixel);
    }

  XClearWindow (dpy, window);
}

static void
random_trig (Display *dpy, Window window, struct state *st,
             XColor *color, Bool *got_color)
{
  st->d_angle = 0;
  st->factor1 = (random() % 8) + 1;
  do {
    st->factor2 = (random() % 8) + 1;
  } while (st->factor1 == st->factor2);

  st->dir = (random() & 1) ? 1 : -1;
  st->d_angle_offset = random() % 360;
  st->offset = ((random() % ((360 / 4) - 1)) + 1) / 4;
  st->density = 1 << ((random() % 4) + 4);	/* Higher density, higher angles */

  if (mono_p)
    XSetForeground (dpy, st->draw_gc, st->default_fg_pixel);
  else
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &color->red, &color->green, &color->blue);
      if ((*got_color = XAllocColor (dpy, st->cmap, color)))
	XSetForeground (dpy, st->draw_gc, color->pixel);
      else
	XSetForeground (dpy, st->draw_gc, st->default_fg_pixel);
    }

  XClearWindow (dpy, window);
}


/* random_helix_or_trig */
static unsigned long
helix_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  Bool free_color = False;
  XColor color;
  int delay = st->subdelay;
  int erase_delay = 10000;
  int ii;

  if (st->eraser) {
    st->eraser = erase_window (dpy, window, st->eraser);
    if (st->eraser) 
      delay = erase_delay;
    goto END;
  }

  switch (st->dstate) 
    {
    case LINGER:
      delay = st->sleep_time * 1000000;
      st->dstate = ERASE;
      break;

    case ERASE:
      st->eraser = erase_window (dpy, window, st->eraser);
      delay = erase_delay;
      if (free_color) XFreeColors (dpy, st->cmap, &color.pixel, 1, 0);
      st->dstate = (random() & 1) ? HELIX : TRIG;
      break;

    case DRAW_HELIX:
      for (ii = 0; ii < 10; ii++) {
        helix (dpy, window, st);
        if (st->dstate != DRAW_HELIX)
          break;
      }
      break;

    case DRAW_TRIG:
      for (ii = 0; ii < 5; ii++) {
        trig (dpy, window, st);
        if (st->dstate != DRAW_TRIG)
          break;
      }
      break;

    case HELIX:
      random_helix (dpy, window, st, &color, &free_color);
      st->dstate = DRAW_HELIX;
      break;

    case TRIG:
      random_trig(dpy, window, st, &color, &free_color);
      st->dstate = DRAW_TRIG;
      break;

    default: 
      abort();
    }

 END:
  return delay;
}

static void
helix_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->width = w;
  st->height = h;
}

static Bool
helix_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
helix_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}



static const char *helix_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*fpsSolid:	true",
  "*delay:      5",
  "*subdelay:	20000",
#ifdef USE_IPHONE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec helix_options [] = {   
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { "-subdelay",        ".subdelay",            XrmoptionSepArg, 0 },
  { 0,			0,			0,		 0 },
};

XSCREENSAVER_MODULE ("Helix", helix)
