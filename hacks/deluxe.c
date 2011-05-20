/* xscreensaver, Copyright (c) 1999-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include "screenhack.h"
#include "alpha.h"

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#define countof(x) (sizeof(x)/sizeof(*(x)))
#define ABS(x) ((x)<0?-(x):(x))

struct state {
  Display *dpy;
  Window window;

  Bool transparent_p;
  int nplanes;
  unsigned long base_pixel, *plane_masks;

  int count;
  int delay;
  int ncolors;
  Bool dbuf;
  XColor *colors;
  GC erase_gc;
  struct throbber **throbbers;
  XWindowAttributes xgwa;
  Pixmap b, ba, bb;	/* double-buffer to reduce flicker */

# ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  Bool dbeclear_p;
  XdbeBackBuffer backb;
# endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
};

struct throbber {
  int x, y;
  int size;
  int max_size;
  int thickness;
  int speed;
  int fuse;
  GC gc;
  void (*draw) (struct state *, Drawable, struct throbber *);
};


static void
draw_star (struct state *st, Drawable w, struct throbber *t)
{
  XPoint points[11];
  int x = t->x;
  int y = t->y;
  int s = t->size / 0.383;  /* trial and error, I forget how to derive this */
  int s2 = t->size;
  double c = M_PI * 2;
  double o = -M_PI / 2;

  points[0].x = x + s  * cos(o + 0.0*c); points[0].y = y + s  * sin(o + 0.0*c);
  points[1].x = x + s2 * cos(o + 0.1*c); points[1].y = y + s2 * sin(o + 0.1*c);
  points[2].x = x + s  * cos(o + 0.2*c); points[2].y = y + s  * sin(o + 0.2*c);
  points[3].x = x + s2 * cos(o + 0.3*c); points[3].y = y + s2 * sin(o + 0.3*c);
  points[4].x = x + s  * cos(o + 0.4*c); points[4].y = y + s  * sin(o + 0.4*c);
  points[5].x = x + s2 * cos(o + 0.5*c); points[5].y = y + s2 * sin(o + 0.5*c);
  points[6].x = x + s  * cos(o + 0.6*c); points[6].y = y + s  * sin(o + 0.6*c);
  points[7].x = x + s2 * cos(o + 0.7*c); points[7].y = y + s2 * sin(o + 0.7*c);
  points[8].x = x + s  * cos(o + 0.8*c); points[8].y = y + s  * sin(o + 0.8*c);
  points[9].x = x + s2 * cos(o + 0.9*c); points[9].y = y + s2 * sin(o + 0.9*c);
  points[10] = points[0];

  XDrawLines (st->dpy, w, t->gc, points, countof(points), CoordModeOrigin);
}

static void
draw_circle (struct state *st, Drawable w, struct throbber *t)
{
  XDrawArc (st->dpy, w, t->gc,
            t->x - t->size / 2,
            t->y - t->size / 2,
            t->size, t->size,
            0, 360*64);
}

static void
draw_hlines (struct state *st, Drawable w, struct throbber *t)
{
  XDrawLine (st->dpy, w, t->gc, 0,
             t->y - t->size, t->max_size,
             t->y - t->size);
  XDrawLine (st->dpy, w, t->gc, 0,
             t->y + t->size, t->max_size,
             t->y + t->size);
}

static void
draw_vlines (struct state *st, Drawable w, struct throbber *t)
{
  XDrawLine (st->dpy, w, t->gc,
             t->x - t->size, 0,
             t->x - t->size, t->max_size);
  XDrawLine (st->dpy, w, t->gc,
             t->x + t->size, 0,
             t->x + t->size, t->max_size);
}

static void
draw_corners (struct state *st, Drawable w, struct throbber *t)
{
  int s = (t->size + t->thickness) / 2;
  XPoint points[3];

  if (t->y > s)
    {
      points[0].x = 0;        points[0].y = t->y - s;
      points[1].x = t->x - s; points[1].y = t->y - s;
      points[2].x = t->x - s; points[2].y = 0;
      XDrawLines (st->dpy, w, t->gc, points, countof(points), CoordModeOrigin);

      points[0].x = t->x + s;    points[0].y = 0;
      points[1].x = t->x + s;    points[1].y = t->y - s;
      points[2].x = t->max_size; points[2].y = t->y - s;
      XDrawLines (st->dpy, w, t->gc, points, countof(points), CoordModeOrigin);
    }

  if (t->x > s)
    {
      points[0].x = 0;        points[0].y = t->y + s;
      points[1].x = t->x - s; points[1].y = t->y + s;
      points[2].x = t->x - s; points[2].y = t->max_size;
      XDrawLines (st->dpy, w, t->gc, points, countof(points), CoordModeOrigin);

      points[0].x = t->x + s;    points[0].y = t->max_size;
      points[1].x = t->x + s;    points[1].y = t->y + s;
      points[2].x = t->max_size; points[2].y = t->y + s;
      XDrawLines (st->dpy, w, t->gc, points, countof(points), CoordModeOrigin);
    }
}


static struct throbber *
make_throbber (struct state *st, Drawable d, int w, int h, unsigned long pixel)
{
  XGCValues gcv;
  unsigned long flags;
  struct throbber *t = (struct throbber *) malloc (sizeof (*t));
  t->x = w / 2;
  t->y = h / 2;
  t->max_size = w;
  t->speed = get_integer_resource (st->dpy, "speed", "Speed");
  t->fuse = 1 + (random() % 4);
  t->thickness = get_integer_resource (st->dpy, "thickness", "Thickness");

  if (t->speed < 0) t->speed = -t->speed;
  t->speed += (((random() % t->speed) / 2) - (t->speed / 2));
  if (t->speed > 0) t->speed = -t->speed;

  flags = GCForeground;
# ifndef HAVE_COCOA
  if (st->transparent_p)
    {
      gcv.foreground = ~0L;
      gcv.plane_mask = st->base_pixel | st->plane_masks[random() % st->nplanes];
      flags |= GCPlaneMask;
    }
  else
# endif /* !HAVE_COCOA */
    {
      gcv.foreground = pixel;
    }

  gcv.line_width = t->thickness;
  gcv.cap_style = CapProjecting;
  gcv.join_style = JoinMiter;

  flags |= (GCLineWidth | GCCapStyle | GCJoinStyle);
  t->gc = XCreateGC (st->dpy, d, flags, &gcv);

# ifdef HAVE_COCOA
  if (st->transparent_p)
    {
      /* give a non-opaque alpha to the color */
      unsigned long pixel = gcv.foreground;
      unsigned long amask = BlackPixelOfScreen (st->xgwa.screen);
      unsigned long a = (0xCCCCCCCC & amask);
      pixel = (pixel & (~amask)) | a;

      jwxyz_XSetAlphaAllowed (st->dpy, t->gc, True);
      XSetForeground (st->dpy, t->gc, pixel);
    }
# endif /* HAVE_COCOA */

  switch (random() % 11) {
  case 0: case 1: case 2: case 3: t->draw = draw_star; break;
  case 4: case 5: case 6: case 7: t->draw = draw_circle; break;
  case 8: t->draw = draw_hlines; break;
  case 9: t->draw = draw_vlines; break;
  case 10: t->draw = draw_corners; break;
  default: abort(); break;
  }

  if (t->draw == draw_circle) 
    t->max_size *= 1.5;

  if (random() % 4)
    t->size = t->max_size;
  else
    t->size = t->thickness, t->speed = -t->speed;

  return t;
}

static int
throb (struct state *st, Drawable window, struct throbber *t)
{
  t->size += t->speed;
  if (t->size <= (t->thickness / 2))
    {
      t->speed = -t->speed;
      t->size += (t->speed * 2);
    }
  else if (t->size > t->max_size)
    {
      t->speed = -t->speed;
      t->size += (t->speed * 2);
      t->fuse--;
    }

  if (t->fuse <= 0)
    {
      XFreeGC (st->dpy, t->gc);
      memset (t, 0, sizeof(*t));
      free (t);
      return -1;
    }
  else
    {
      t->draw (st, window, t);
      return 0;
    }
}


static void *
deluxe_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  int i;
  st->dpy = dpy;
  st->window = window;
  st->count = get_integer_resource (st->dpy, "count", "Integer");
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->ncolors = get_integer_resource (st->dpy, "ncolors", "Integer");
  st->dbuf = get_boolean_resource (st->dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  st->dbeclear_p = get_boolean_resource (st->dpy, "useDBEClear", "Boolean");
#endif

# ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
  st->dbuf = False;
# endif

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->transparent_p = get_boolean_resource(st->dpy, "transparent", "Transparent");

  st->colors = (XColor *) calloc (sizeof(*st->colors), st->ncolors);

  if (get_boolean_resource(st->dpy, "mono", "Boolean"))
    {
    MONO:
      st->ncolors = 1;
      st->colors[0].pixel = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                           "foreground", "Foreground");
    }
#ifndef HAVE_COCOA
  else if (st->transparent_p)
    {
      st->nplanes = get_integer_resource (st->dpy, "planes", "Planes");
      if (st->nplanes <= 0)
        st->nplanes = (random() % (st->xgwa.depth-2)) + 2;

      allocate_alpha_colors (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                             &st->nplanes, True, &st->plane_masks,
			     &st->base_pixel);
      if (st->nplanes <= 1)
	{
# if 0
	  fprintf (stderr,
         "%s: couldn't allocate any color planes; turning transparency off.\n",
		   progname);
# endif
          st->transparent_p = False;
	  goto COLOR;
	}
    }
#endif /* !HAVE_COCOA */
  else
    {
#ifndef HAVE_COCOA
    COLOR:
#endif
      make_random_colormap (st->dpy, st->xgwa.visual, st->xgwa.colormap,
                            st->colors, &st->ncolors, True, True, 0, True);
      if (st->ncolors < 2)
        goto MONO;
    }

  if (st->dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      if (st->dbeclear_p)
        st->b = xdbe_get_backbuffer (st->dpy, st->window, XdbeBackground);
      else
        st->b = xdbe_get_backbuffer (st->dpy, st->window, XdbeUndefined);
      st->backb = st->b;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

      if (!st->b)
        {
          st->ba = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height,st->xgwa.depth);
          st->bb = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height,st->xgwa.depth);
          st->b = st->ba;
        }
    }
  else
    {
      st->b = st->window;
    }

  st->throbbers = (struct throbber **) calloc (st->count, sizeof(struct throbber *));
  for (i = 0; i < st->count; i++)
    st->throbbers[i] = make_throbber (st, st->b, st->xgwa.width, st->xgwa.height,
                                  st->colors[random() % st->ncolors].pixel);

  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "background", "Background");
  st->erase_gc = XCreateGC (st->dpy, st->b, GCForeground, &gcv);

  if (st->ba) XFillRectangle (st->dpy, st->ba, st->erase_gc, 0, 0, st->xgwa.width, st->xgwa.height);
  if (st->bb) XFillRectangle (st->dpy, st->bb, st->erase_gc, 0, 0, st->xgwa.width, st->xgwa.height);

  return st;
}

static unsigned long
deluxe_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (!st->dbeclear_p || !st->backb)
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    XFillRectangle (st->dpy, st->b, st->erase_gc, 0, 0, st->xgwa.width, st->xgwa.height);

  for (i = 0; i < st->count; i++)
    if (throb (st, st->b, st->throbbers[i]) < 0)
      st->throbbers[i] = make_throbber (st, st->b, st->xgwa.width, st->xgwa.height,
                                    st->colors[random() % st->ncolors].pixel);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->backb)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = st->window;
      info[0].swap_action = (st->dbeclear_p ? XdbeBackground : XdbeUndefined);
      XdbeSwapBuffers (st->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    if (st->dbuf)
      {
        XCopyArea (st->dpy, st->b, st->window, st->erase_gc, 0, 0,
                   st->xgwa.width, st->xgwa.height, 0, 0);
        st->b = (st->b == st->ba ? st->bb : st->ba);
      }

  return st->delay;
}

static void
deluxe_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  if (! st->dbuf) {   /* #### more complicated if we have a back buffer... */
    int i;
    XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
    XClearWindow (dpy, window);
    for (i = 0; i < st->count; i++)
      if (st->throbbers[i])
        st->throbbers[i]->fuse = 0;
  }
}

static Bool
deluxe_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
deluxe_free (Display *dpy, Window window, void *closure)
{
}


static const char *deluxe_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*delay:		10000",
  "*count:		5",
  "*thickness:		50",
  "*speed:		15",
  "*ncolors:		20",
  "*transparent:	True",
  "*doubleBuffer:	True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
  "*useDBEClear:	True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};

static XrmOptionDescRec deluxe_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-speed",		".speed",	XrmoptionSepArg, 0 },
  { "-transparent",	".transparent",	 XrmoptionNoArg,  "True"  },
  { "-no-transparent",	".transparent",	 XrmoptionNoArg,  "False" },
  { "-opaque",		".transparent",	 XrmoptionNoArg,  "False" },
  { "-no-opaque",	".transparent",	 XrmoptionNoArg,  "True"  },
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True"  },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Deluxe", deluxe)
