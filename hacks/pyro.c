/* xscreensaver, Copyright (c) 1992, 1994, 1996
 *  Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Draw some fireworks.  Inspired from TI Explorer Lisp code by 
   John S. Pezaris <pz@hx.lcs.mit.edu>
 */

#include "screenhack.h"

struct projectile {
  int x, y;	/* position */
  int dx, dy;	/* velocity */
  int decay;
  int size;
  int fuse;
  Bool primary;
  Bool dead;
  XColor color;
  struct projectile *next_free;
};

static struct projectile *projectiles, *free_projectiles;

static struct projectile *
get_projectile (void)
{
  struct projectile *p;
  if (free_projectiles)
    {
      p = free_projectiles;
      free_projectiles = p->next_free;
      p->next_free = 0;
      p->dead = False;
      return p;
    }
  else
    return 0;
}

static void
free_projectile (struct projectile *p)
{
  p->next_free = free_projectiles;
  free_projectiles = p;
  p->dead = True;
}

static void
launch (int xlim, int ylim, int g,
	Display *dpy, Colormap cmap)
{
  struct projectile *p = get_projectile ();
  int x, dx, xxx;
  if (! p) return;

  do {
    x = (random () % xlim);
    dx = 30000 - (random () % 60000);
    xxx = x + (dx * 200);
  } while (xxx <= 0 || xxx >= xlim);

  p->x = x;
  p->y = ylim;
  p->dx = dx;
  p->size = 8000;
  p->decay = 0;
  p->dy = (random () % 4000) - 13000;
  p->fuse = ((((random () % 500) + 500) * abs (p->dy / g)) / 1000);
  p->primary = True;

  if (! mono_p)
    {
      hsv_to_rgb (random () % 360, 1.0, 1.0,
		  &p->color.red, &p->color.green, &p->color.blue);
      p->color.flags = DoRed | DoGreen | DoBlue;
      if (!XAllocColor (dpy, cmap, &p->color))
	{
	  p->color.pixel = WhitePixel (dpy, DefaultScreen (dpy));
	  p->color.red = p->color.green = p->color.blue = 0xFFFF;
	}
    }
}

static struct projectile *
shrapnel (struct projectile *parent, Display *dpy, Colormap cmap)
{
  struct projectile *p = get_projectile ();
  if (! p) return 0;
  p->x = parent->x;
  p->y = parent->y;
  p->dx = (random () % 5000) - 2500 + parent->dx;
  p->dy = (random () % 5000) - 2500 + parent->dy;
  p->decay = (random () % 50) - 60;
  p->size = (parent->size * 2) / 3;
  p->fuse = 0;
  p->primary = False;

  p->color = parent->color;
  if (! mono_p)
    XAllocColor (dpy, cmap, &p->color); /* dup the lock */
  
  return p;
}

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;

static int how_many, frequency, scatter;

static Colormap
init_pyro (Display *dpy, Window window)
{
  int i;
  Colormap cmap;
  XGCValues gcv;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  how_many = get_integer_resource ("count", "Integer");
  frequency = get_integer_resource ("frequency", "Integer");
  scatter = get_integer_resource ("scatter", "Integer");
  if (how_many <= 0) how_many = 100;
  if (frequency <= 0) frequency = 30;
  if (scatter <= 0) scatter = 20;
  projectiles = 0;
  free_projectiles = 0;
  projectiles = (struct projectile *)
    calloc (how_many, sizeof (struct projectile));
  for (i = 0; i < how_many; i++)
    free_projectile (&projectiles [i]);
  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  XClearWindow (dpy, window);
  return cmap;
}

static void
pyro (Display *dpy, Window window, Colormap cmap)
{
  XWindowAttributes xgwa;
  static int xlim, ylim, real_xlim, real_ylim;
  int g = 100;
  int i;

  if ((random () % frequency) == 0)
    {
      XGetWindowAttributes (dpy, window, &xgwa);
      real_xlim = xgwa.width;
      real_ylim = xgwa.height;
      xlim = real_xlim * 1000;
      ylim = real_ylim * 1000;
      launch (xlim, ylim, g, dpy, cmap);
    }

  XSync (dpy, True);
  usleep (10000);

  for (i = 0; i < how_many; i++)
    {
      struct projectile *p = &projectiles [i];
      int old_x, old_y, old_size;
      int size, x, y;
      if (p->dead) continue;
      old_x = p->x >> 10;
      old_y = p->y >> 10;
      old_size = p->size >> 10;
      size = (p->size += p->decay) >> 10;
      x = (p->x += p->dx) >> 10;
      y = (p->y += p->dy) >> 10;
      p->dy += (p->size >> 6);
      if (p->primary) p->fuse--;

      /* erase old one */
      XFillRectangle (dpy, window, erase_gc, old_x, old_y,
		      old_size, old_size);

      if ((p->primary ? (p->fuse > 0) : (p->size > 0)) &&
	  x < real_xlim &&
	  y < real_ylim &&
	  x > 0 &&
	  y > 0)
	{
	  if (mono_p || p->primary)
	    XSetForeground (dpy, draw_gc, default_fg_pixel);
	  else
	    XSetForeground (dpy, draw_gc, p->color.pixel);

	  if /*(p->primary)*/ (size > 2)
	    XFillArc (dpy, window, draw_gc, x, y, size, size, 0, 360*64);
	  else
	    XFillRectangle (dpy, window, draw_gc, x, y, size, size);
	}
      else
	{
	  free_projectile (p);
	  if (! mono_p)
	    if (p->color.pixel != WhitePixel (dpy, DefaultScreen (dpy)))
	      XFreeColors (dpy, cmap, &p->color.pixel, 1, 0);
	}

      if (p->primary && p->fuse <= 0)
	{
	  int j = (random () % scatter) + (scatter/2);
	  while (j--)
	    shrapnel (p, dpy, cmap);
	}
    }
}


char *progclass = "Pyro";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*count:	100",
  "*frequency:	30",
  "*scatter:	20",
  "*geometry:	800x500",
  0
};

XrmOptionDescRec options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-frequency",	".frequency",	XrmoptionSepArg, 0 },
  { "-scatter",		".scatter",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  Colormap cmap = init_pyro (dpy, window);
  while (1)
    pyro (dpy, window, cmap);
}
