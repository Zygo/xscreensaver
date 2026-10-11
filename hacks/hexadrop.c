/* xscreensaver, Copyright (c) 1999-2019 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws a grid of hexagons or other shapes and drops them out.
 * Created 8-Jul-2013.
 */

#include <math.h>
#include "screenhack.h"

#define ABS(x) ((x)<0?-(x):(x))

/* Avoid rounding errors by using a larger fixed-point grid.
   Without this, we got little pointy errors at some corners. */
#define SCALE 10

typedef struct {
  int sides;
  int cx, cy;
  double th, radius, i, speed;
  int colors[2];
  Bool initted_p;
} cell;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;

  int ncells, cells_size, gw, gh;
  cell *cells;

  int delay;
  double speed;
  int sides;
  Bool lockstep_p;
  Bool uniform_p;
  Bool initted_p;

  int ncolors;
  XColor *colors;
  GC gc;

} state;


static void
make_cells (state *st)
{
  int grid_size = get_integer_resource (st->dpy, "size", "Size");
  cell *cells2;
  int size, r, gw, gh, x, y, i;
  double th = 0;

  if (grid_size < 5) grid_size = 5;

  size = ((st->xgwa.width > st->xgwa.height
           ? st->xgwa.width : st->xgwa.height)
          / grid_size);
  gw = st->xgwa.width  / size;
  gh = st->xgwa.height / size;

  switch (st->sides) {
  case 8:
    r  = size * 0.75;
    th = M_PI / st->sides;
    gw *= 1.25;
    gh *= 1.25;
    break;
  case 6:
    r  = size / sqrt(3);
    th = M_PI / st->sides;
    gh *= 1.2;
    break;
  case 3:
    size *= 2;
    r  = size / sqrt(3);
    th = M_PI / st->sides / 2;
    break;
  case 4:
    size /= 2;
    r  = size * sqrt (2);
    th = M_PI / st->sides;
    break;
  default:
    abort();
    break;
  }

  gw += 3;	/* leave a few extra columns off screen just in case */
  gh += 3;

  st->ncells = gw * gh;

  if (st->initted_p && !st->cells) abort();
  if (!st->initted_p && st->cells) abort();

  cells2 = (cell *) calloc (st->ncells, sizeof(*cells2));
  if (! cells2) abort();

  if (st->cells)
    {
      for (y = 0; y < (st->gh < gh ? st->gh : gh); y++)
        for (x = 0; x < (st->gw < gw ? st->gw : gw); x++)
          cells2[y * gw + x] = st->cells [y * st->gw + x];
      free (st->cells);
      st->cells = 0;
    }

  st->cells = cells2;
  st->gw = gw;
  st->gh = gh;

  i = 0;
  for (y = 0; y < gh; y++)
    for (x = 0; x < gw; x++)
      {
        cell *c = &st->cells[i];
        c->sides = st->sides;
        c->radius = SCALE * r;
        c->th = th;

        switch (st->sides) {
        case 8:
          if (x & 1)
            {
              c->cx = SCALE * x * size;
              c->radius /= 2;
              c->th = M_PI / 4;
              c->sides = 4;
              c->radius *= 1.1;
            }
          else
            {
              c->cx = SCALE * x * size;
              c->radius *= 1.02;
              c->radius--;
            }

          if (y & 1)
            c->cx -= SCALE * size;

          c->cy = SCALE * y * size;

         break;
        case 6:
          c->cx = SCALE * x * size;
          c->cy = SCALE * y * size * sqrt(3)/2;
          if (y & 1)
            c->cx -= SCALE * size * 0.5;
          break;
        case 4:
          c->cx = SCALE * x * size * 2;
          c->cy = SCALE * y * size * 2;
          break;
        case 3:
          c->cx = SCALE * x * size * 0.5;
          c->cy = SCALE * y * size * sqrt(3)/2;
          if ((x & 1) ^ (y & 1))
            {
              c->th = th + M_PI;
              c->cy -= SCALE * r * 0.5;
            }
          break;
        default:
          abort();
        }

        if (! c->initted_p)
          {
            c->speed = st->speed * (st->uniform_p ? 1 : (0.1 + frand(0.9)));
            c->i = st->lockstep_p ? 0 : random() % r;
            c->colors[0] = (st->lockstep_p ? 0 : random() % st->ncolors);
            c->colors[1] = 0;
            c->initted_p = True;
          }

        c->radius += SCALE;  /* Avoid single-pixel erase rounding errors */

        if (c->i > c->radius) c->i = c->radius;
        if (c->colors[0] >= st->ncolors) c->colors[0] = st->ncolors-1;
        if (c->colors[1] >= st->ncolors) c->colors[1] = st->ncolors-1;

        i++;
      }

  st->initted_p = True;
}


static void
draw_cell (state *st, cell *c)
{
  XPoint points[20];
  int i, j;
  for (j = 0; j <= 1; j++)
    {
      int r = (j == 0 ? c->radius : c->i);
      for (i = 0; i < c->sides; i++)
        {
          double th = i * M_PI * 2 / c->sides;
          points[i].x = (c->cx + r * cos (th + c->th) + 0.5) / SCALE;
          points[i].y = (c->cy + r * sin (th + c->th) + 0.5) / SCALE;
        }
      XSetForeground (st->dpy, st->gc, st->colors[c->colors[j]].pixel);
      XFillPolygon (st->dpy, st->window, st->gc, points, c->sides,
                    Convex, CoordModeOrigin);
    }

  c->i -= SCALE * c->speed;
  if (c->i < 0)
    {
      c->i = c->radius;
      c->colors[1] = c->colors[0];
      if (c != &st->cells[0])
        c->colors[0] = st->cells[0].colors[0];
      else
        c->colors[0] = random() % st->ncolors;
    }
}


static void
hexadrop_init_1 (Display *dpy, Window window, state *st)
{
  XGCValues gcv;
  char *s1, *s2;

  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->ncolors = get_integer_resource (st->dpy, "ncolors", "Integer");
  st->speed = get_float_resource (st->dpy, "speed", "Speed");
  if (st->speed < 0) st->speed = 0;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  if (st->ncolors < 2) st->ncolors = 2;

  st->colors = (XColor *) calloc (sizeof(*st->colors), st->ncolors);

  if (st->ncolors < 10)
    make_random_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                          st->colors, &st->ncolors, False, True, 0, True);
  else
    make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                          st->colors, &st->ncolors, True, 0, True);
  XSetWindowBackground (dpy, window, st->colors[0].pixel);

  s1 = get_string_resource (st->dpy, "uniform", "Uniform");
  s2 = get_string_resource (st->dpy, "lockstep", "Lockstep");

  if ((!s1 || !*s1 || !strcasecmp(s1, "maybe")) &&
      (!s2 || !*s2 || !strcasecmp(s2, "maybe")))
    {
      /* When being random, don't do both. */
      st->uniform_p = random() & 1;
      st->lockstep_p = st->uniform_p ? 0 : random() & 1;
    }
  else 
    {
      if (!s1 || !*s1 || !strcasecmp(s1, "maybe"))
        st->uniform_p = random() & 1;
      else
        st->uniform_p = get_boolean_resource (st->dpy, "uniform", "Uniform");

      if (!s2 || !*s2 || !strcasecmp(s2, "maybe"))
        st->lockstep_p = random() & 1;
      else
        st->lockstep_p = get_boolean_resource (st->dpy, "lockstep","Lockstep");
    }
  if (s1) free (s1);
  if (s2) free (s2);

  st->sides = get_integer_resource (st->dpy, "sides", "Sides");
  if (! (st->sides == 0 || st->sides == 3 || st->sides == 4 || 
         st->sides == 6 || st->sides == 8))
    {
      printf ("%s: invalid number of sides: %d\n", progname, st->sides);
      st->sides = 0;
    }

  if (! st->sides)
    {
      static int defs[] = { 3, 3, 3,
                            4,
                            6, 6, 6, 6,
                            8, 8, 8 };
      st->sides = defs[random() % countof(defs)];
    }

  make_cells (st);
  gcv.foreground = st->colors[0].pixel;
  st->gc = XCreateGC (dpy, window, GCForeground, &gcv);
}


static void *
hexadrop_init (Display *dpy, Window window)
{
  state *st = (state *) calloc (1, sizeof(*st));
  hexadrop_init_1 (dpy, window, st);
  return st;
}



static unsigned long
hexadrop_draw (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;
  int i;

  for (i = 0; i < st->ncells; i++)
    draw_cell (st, &st->cells[i]);

  return st->delay;
}


static void
hexadrop_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  state *st = (state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  make_cells (st);
}


static void
hexadrop_free_1 (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;
  if (st->colors)
    {
      free_colors (st->xgwa.screen, st->xgwa.colormap, st->colors, st->ncolors);
      free (st->colors);
      st->colors = 0;
    }
  if (st->cells) 
    {
      free (st->cells);
      st->cells = 0;
    }
  if (st->gc)
    {
      XFreeGC (st->dpy, st->gc);
      st->gc = 0;
    }

  memset (st, 0, sizeof(*st));
}


static void
hexadrop_free (Display *dpy, Window window, void *closure)
{
  hexadrop_free_1 (dpy, window, closure);
  free (closure);
}


static Bool
hexadrop_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  state *st = (state *) closure;

  if (screenhack_event_helper (dpy, window, event))
    {
      if (random() % 5)		/* Change everything */
        {
          hexadrop_free_1 (st->dpy, st->window, st);
          hexadrop_init_1 (dpy, window, st);
        }
      else			/* Change colors only */
        {
          /* Save the old geometry */
          cell *c = st->cells;
          int n = st->ncells;
          int s = st->sides;
          int i;

          /* Protect it from being freed */
          st->cells = 0;
          hexadrop_free_1 (st->dpy, st->window, st);
          hexadrop_init_1 (dpy, window, st);

          /* Reset the old cells */
          for (i = 0; i < n; i++)
            c[i].initted_p = False;

          /* Re-init, then put them back. */
          free (st->cells);
          st->cells = c;
          st->ncells = n;
          st->sides = s;
        }

      return True;
    }

  return False;
}


static const char *hexadrop_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*fpsSolid:		true",
  "*delay:		30000",
  "*sides:		0",
  "*size:		15",
  "*speed:		1.0",
  "*ncolors:		128",
  "*uniform:		Maybe",
  "*lockstep:		Maybe",
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec hexadrop_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-sides",		".sides",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, 0 },
  { "-speed",		".speed",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-uniform-speed",	".uniform",	XrmoptionNoArg, "True"  },
  { "-no-uniform-speed",".uniform",	XrmoptionNoArg, "False" },
  { "-nonuniform-speed",".uniform",	XrmoptionNoArg, "False" },
  { "-lockstep",	".lockstep",	XrmoptionNoArg, "True"  },
  { "-no-lockstep",	".lockstep",	XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Hexadrop", hexadrop)
