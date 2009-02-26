/* xscreensaver, Copyright (c) 1992, 1994, 1996, 1998, 2001
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

#include <math.h>
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
static struct projectile **sorted_projectiles;


/* Slightly whacked, for better explosions
 */
#define PI_2000 6284

static int sin_cache[PI_2000];
static int cos_cache[PI_2000];

static void
cache(void)
{               /*needs to be run once. Could easily be */
  int i;        /*reimplemented to run and cache at compile-time,*/
  double dA;    /*saving on init_pyro time */
  for (i=0; i<PI_2000; i++)
    {
      dA=sin(((double) (random() % (PI_2000/2)))/1000.0);
      /*Emulation of spherical distribution*/
      dA+=asin(frand(1.0))/M_PI_2*0.1;
      /*Approximating the integration of the binominal, for
        well-distributed randomness*/
      cos_cache[i]=(int) (cos(((double)i)/1000.0)*dA*2500.0);
      sin_cache[i]=(int) (sin(((double)i)/1000.0)*dA*2500.0);
    }
}


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
  int v;
  if (! p) return 0;
  p->x = parent->x;
  p->y = parent->y;
  v=random () % PI_2000;
  p->dx =(sin_cache[v]) + parent->dx;
  p->dy =(cos_cache[v]) + parent->dx;
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
    calloc (how_many, sizeof (*projectiles));
  sorted_projectiles = (struct projectile **)
    calloc (how_many, sizeof (*sorted_projectiles));
  for (i = 0; i < how_many; i++)
    free_projectile (&projectiles [i]);
  for (i = 0; i < how_many; i++)
    sorted_projectiles[i] = &projectiles[i];
  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  XClearWindow (dpy, window);
  cache();  
  return cmap;
}


static int
projectile_pixel_sorter (const void *a, const void *b)
{
  struct projectile *pa = *(struct projectile **) a;
  struct projectile *pb = *(struct projectile **) b;
  if (pa->color.pixel == pb->color.pixel) return 0;
  else if (pa->color.pixel < pb->color.pixel) return -1;
  else return 1;
}

static void
sort_by_pixel (int length)
{
  qsort ((void *) sorted_projectiles,
         length,
         sizeof(*sorted_projectiles),
         projectile_pixel_sorter);
}


static void
pyro (Display *dpy, Window window, Colormap cmap)
{
  XWindowAttributes xgwa;
  static int xlim, ylim, real_xlim, real_ylim;
  int g = 100;
  int resort = 0;
  int i;
  
  for (i = 0; i < how_many; i++)
    {
      struct projectile *p = sorted_projectiles [i];
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
      if (old_size > 0)
        {
          if (old_size == 1)
	    XDrawPoint (dpy, window, erase_gc, old_x, old_y);
          else
            XFillRectangle (dpy, window, erase_gc, old_x, old_y,
                            old_size, old_size);
        }

      if ((p->primary ? (p->fuse > 0) : (p->size > 0)) &&
	  x < real_xlim &&
	  y < real_ylim &&
	  x > 0 &&
	  y > 0)
	{
          if (size > 0)
            {
              static unsigned long last_pixel = ~0;
              unsigned long pixel;

              if (mono_p || p->primary)
                pixel = default_fg_pixel;
              else
                pixel = p->color.pixel;

              if (pixel != last_pixel)
                {
                  last_pixel = pixel;
                  XSetForeground (dpy, draw_gc, pixel);
                }

              if (size == 1)
                XDrawPoint (dpy, window, draw_gc, x, y);
              else if (size < 4)
                XFillRectangle (dpy, window, draw_gc, x, y, size, size);
              else
                XFillArc (dpy, window, draw_gc, x, y, size, size, 0, 360*64);
            }
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
          resort = 1;
	}
    }

  if ((random () % frequency) == 0)
    {
      XGetWindowAttributes (dpy, window, &xgwa);
      real_xlim = xgwa.width;
      real_ylim = xgwa.height;
      xlim = real_xlim * 1000;
      ylim = real_ylim * 1000;
      launch (xlim, ylim, g, dpy, cmap);
      resort = 1;
    }

  /* being sorted lets us avoid changing the GC's foreground color as often. */
  if (resort)
    sort_by_pixel (how_many);
}


char *progclass = "Pyro";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*count:	600",
  "*delay:	5000",
  "*frequency:	30",
  "*scatter:	100",
  "*geometry:	800x500",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-frequency",	".frequency",	XrmoptionSepArg, 0 },
  { "-scatter",		".scatter",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  Colormap cmap = init_pyro (dpy, window);
  int delay = get_integer_resource ("delay", "Integer");

  while (1)
    {
      pyro (dpy, window, cmap);

      XSync (dpy, False);
      screenhack_handle_events (dpy);
      usleep (delay);
    }
}
