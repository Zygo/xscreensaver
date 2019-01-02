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

#define PI_2000 6284


struct state {
  Display *dpy;
  Window window;

   struct projectile *projectiles, *free_projectiles;
   struct projectile **sorted_projectiles;

   GC draw_gc, erase_gc;
   unsigned int default_fg_pixel;
   Colormap cmap;

   int how_many, frequency, scatter, delay;

   int sin_cache[PI_2000];
   int cos_cache[PI_2000];

   int draw_xlim, draw_ylim, real_draw_xlim, real_draw_ylim;

   unsigned long last_pixel;
};



/* Slightly whacked, for better explosions
 */

static void
cache(struct state *st)
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
      st->cos_cache[i]=(int) (cos(((double)i)/1000.0)*dA*2500.0);
      st->sin_cache[i]=(int) (sin(((double)i)/1000.0)*dA*2500.0);
    }
}


static struct projectile *
get_projectile (struct state *st)
{
  struct projectile *p;
  if (st->free_projectiles)
    {
      p = st->free_projectiles;
      st->free_projectiles = p->next_free;
      p->next_free = 0;
      p->dead = False;
      return p;
    }
  else
    return 0;
}

static void
free_projectile (struct state *st, struct projectile *p)
{
  p->next_free = st->free_projectiles;
  st->free_projectiles = p;
  p->dead = True;
}

static void
launch (struct state *st, 
        int xlim, int ylim, int g)
{
  struct projectile *p = get_projectile (st);
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

  /* cope with small windows -- those constants assume big windows. */
  {
    int dd = 1000000 / ylim;
    if (dd > 1)
      p->fuse /= dd;
  }

  if (! mono_p)
    {
      hsv_to_rgb (random () % 360, 1.0, 1.0,
		  &p->color.red, &p->color.green, &p->color.blue);
      p->color.flags = DoRed | DoGreen | DoBlue;
      if (!XAllocColor (st->dpy, st->cmap, &p->color))
	{
	  p->color.pixel = WhitePixel (st->dpy, DefaultScreen (st->dpy));
	  p->color.red = p->color.green = p->color.blue = 0xFFFF;
	}
    }
}

static struct projectile *
shrapnel (struct state *st, struct projectile *parent)
{
  struct projectile *p = get_projectile (st);
  int v;
  if (! p) return 0;
  p->x = parent->x;
  p->y = parent->y;
  v=random () % PI_2000;
  p->dx =(st->sin_cache[v]) + parent->dx;
  p->dy =(st->cos_cache[v]) + parent->dy;
  p->decay = (random () % 50) - 60;
  p->size = (parent->size * 2) / 3;
  p->fuse = 0;
  p->primary = False;

  p->color = parent->color;
  if (! mono_p)
    XAllocColor (st->dpy, st->cmap, &p->color); /* dup the lock */
  
  return p;
}

static void *
pyro_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  XGCValues gcv;
  XWindowAttributes xgwa;
  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->last_pixel = ~0;
  st->cmap = xgwa.colormap;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->how_many = get_integer_resource (st->dpy, "count", "Integer");
  st->frequency = get_integer_resource (st->dpy, "frequency", "Integer");
  st->scatter = get_integer_resource (st->dpy, "scatter", "Integer");
  if (st->how_many <= 0) st->how_many = 100;
  if (st->frequency <= 0) st->frequency = 30;
  if (st->scatter <= 0) st->scatter = 20;
  st->projectiles = 0;
  st->free_projectiles = 0;
  st->projectiles = (struct projectile *)
    calloc (st->how_many, sizeof (*st->projectiles));
  st->sorted_projectiles = (struct projectile **)
    calloc (st->how_many, sizeof (*st->sorted_projectiles));
  for (i = 0; i < st->how_many; i++)
    free_projectile (st, &st->projectiles [i]);
  for (i = 0; i < st->how_many; i++)
    st->sorted_projectiles[i] = &st->projectiles[i];
  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");
  st->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource (st->dpy, st->cmap, "background", "Background");
  st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  XClearWindow (st->dpy, st->window);
  cache(st);  

  return st;
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
sort_by_pixel (struct state *st, int length)
{
  qsort ((void *) st->sorted_projectiles,
         length,
         sizeof(*st->sorted_projectiles),
         projectile_pixel_sorter);
}


static unsigned long
pyro_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XWindowAttributes xgwa;
  int g = 100;
  int resort = 0;
  int i;
  
  for (i = 0; i < st->how_many; i++)
    {
      struct projectile *p = st->sorted_projectiles [i];
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
	    XDrawPoint (st->dpy, st->window, st->erase_gc, old_x, old_y);
          else
            XFillRectangle (st->dpy, st->window, st->erase_gc, old_x, old_y,
                            old_size, old_size);
        }

      if ((p->primary ? (p->fuse > 0) : (p->size > 0)) &&
	  x < st->real_draw_xlim &&
	  y < st->real_draw_ylim &&
	  x > 0 &&
	  y > 0)
	{
          if (size > 0)
            {
              unsigned long pixel;

              if (mono_p || p->primary)
                pixel = st->default_fg_pixel;
              else
                pixel = p->color.pixel;

              if (pixel != st->last_pixel)
                {
                  st->last_pixel = pixel;
                  XSetForeground (st->dpy, st->draw_gc, pixel);
                }

              if (size == 1)
                XDrawPoint (st->dpy, st->window, st->draw_gc, x, y);
              else if (size < 4)
                XFillRectangle (st->dpy, st->window, st->draw_gc, x, y, size, size);
              else
                XFillArc (st->dpy, st->window, st->draw_gc, x, y, size, size, 0, 360*64);
            }
        }
      else
	{
	  free_projectile (st, p);
	  if (! mono_p)
	    if (p->color.pixel != WhitePixel (st->dpy, DefaultScreen (st->dpy)))
	      XFreeColors (st->dpy, st->cmap, &p->color.pixel, 1, 0);
	}

      if (p->primary && p->fuse <= 0)
	{
	  int j = (random () % st->scatter) + (st->scatter/2);
	  while (j--)
	    shrapnel (st, p);
          resort = 1;
	}
    }

  if ((random () % st->frequency) == 0)
    {
      XGetWindowAttributes (st->dpy, st->window, &xgwa);
      st->real_draw_xlim = xgwa.width;
      st->real_draw_ylim = xgwa.height;
      st->draw_xlim = st->real_draw_xlim * 1000;
      st->draw_ylim = st->real_draw_ylim * 1000;
      launch (st, st->draw_xlim, st->draw_ylim, g);
      resort = 1;
    }

  /* being sorted lets us avoid changing the GC's foreground color as often. */
  if (resort)
    sort_by_pixel (st, st->how_many);

  return st->delay;
}

static void
pyro_reshape (Display *dpy, Window window, void *closure, 
              unsigned int w, unsigned int h)
{
}

static Bool
pyro_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
pyro_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->draw_gc);
  XFreeGC (dpy, st->erase_gc);
  free (st->projectiles);
  free (st->sorted_projectiles);
  free (st);
}



static const char *pyro_defaults [] = {
  ".lowrez:     true",
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*count:	600",
  "*delay:	10000",
  "*frequency:	30",
  "*scatter:	100",
  0
};

static XrmOptionDescRec pyro_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-frequency",	".frequency",	XrmoptionSepArg, 0 },
  { "-scatter",		".scatter",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Pyro", pyro)
