/* Copyright (c) 2003 Levi Burton <donburton@sbcglobal.net>
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
#include "colors.h"

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

typedef struct _square {
  int x, y, w, h; 
  int color;
} square;

static void
randomize_square_colors(square *squares, int nsquares, int ncolors)
{
  int i;
  square *s = squares;
  for (i = 0; i < nsquares; i++) 
    s[i].color = random() % ncolors;
}


struct state {
  Display *dpy;
  Window window;

   int delay, subdivision, border, ncolors, twitch, dbuf;
    XWindowAttributes xgwa;
    GC gc; 
    XColor *colors;
    int sw, sh, gw, gh, nsquares;
    square *squares;
    Pixmap b, ba, bb;
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    XdbeBackBuffer backb;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
};

static void *
popsquares_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int x, y;
  double s1, v1, s2, v2 = 0;
  int h1, h2 = 0;
  /* Not sure how to use DBEClear */
  /* Bool dbeclear_p = get_boolean_resource(dpy, "useDBEClear", "Boolean"); */
  XColor fg, bg;
  XGCValues gcv;
  
  st->dpy = dpy;
  st->window = window;

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->subdivision = get_integer_resource(st->dpy, "subdivision", "Integer");
  st->border = get_integer_resource(st->dpy, "border", "Integer");
  st->ncolors = get_integer_resource(st->dpy, "ncolors", "Integer");
  st->twitch = get_boolean_resource(st->dpy, "twitch", "Boolean");
  st->dbuf = get_boolean_resource(st->dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  st->dbuf = False;
# endif

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  fg.pixel = get_pixel_resource (st->dpy, st->xgwa.colormap, "foreground", "Foreground");
  bg.pixel = get_pixel_resource (st->dpy, st->xgwa.colormap, "background", "Background");

  XQueryColor (st->dpy, st->xgwa.colormap, &fg);
  XQueryColor (st->dpy, st->xgwa.colormap, &bg);

  st->sw = st->xgwa.width / st->subdivision;
  st->sh = st->xgwa.height / st->subdivision;
  st->gw = st->sw ? st->xgwa.width / st->sw : 0;
  st->gh = st->sh ? st->xgwa.height / st->sh : 0;
  st->nsquares = st->gw * st->gh;
  if (st->nsquares < 1) st->nsquares = 1;
  if (st->ncolors < 1) st->ncolors = 1;

  gcv.foreground = fg.pixel;
  gcv.background = bg.pixel;
  st->gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);

  st->colors = (XColor *) calloc (st->ncolors, sizeof(XColor));
  st->squares = (square *) calloc (st->nsquares, sizeof(square));

  rgb_to_hsv (fg.red, fg.green, fg.blue, &h1, &s1, &v1);
  rgb_to_hsv (bg.red, bg.green, bg.blue, &h2, &s2, &v2);
  make_color_ramp (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                   h1, s1, v1,
                   h2, s2, v2,
                   st->colors, &st->ncolors,  /* would this be considered a value-result argument? */
                   True, True, False);
  if (st->ncolors < 2)
    {
      fprintf (stderr, "%s: insufficient colors!\n", progname);
      exit (1);
    }

  for (y = 0; y < st->gh; y++)
    for (x = 0; x < st->gw; x++) 
      {
        square *s = (square *) &st->squares[st->gw * y + x];
        s->w = st->sw;
        s->h = st->sh;
        s->x = x * st->sw;
        s->y = y * st->sh;
      }

  randomize_square_colors(st->squares, st->nsquares, st->ncolors);

  if (st->dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      st->b = xdbe_get_backbuffer (st->dpy, st->window, XdbeUndefined);
      st->backb = st->b;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
      if (!st->b)                      
        {
          st->ba = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height, st->xgwa.depth);
          st->bb = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height, st->xgwa.depth);
          st->b = st->ba;
        }
    }
  else 
    {
      st->b = st->window;
    }

  return st;
}

static unsigned long
popsquares_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int x, y;
  for (y = 0; y < st->gh; y++)
    for (x = 0; x < st->gw; x++) 
      {
        square *s = (square *) &st->squares[st->gw * y + x];
        XSetForeground (st->dpy, st->gc, st->colors[s->color].pixel);
        XFillRectangle (st->dpy, st->b, st->gc, s->x, s->y, 
                        st->border ? s->w - st->border : s->w, 
                        st->border ? s->h - st->border : s->h);
        s->color++;
        if (s->color == st->ncolors)
          {
            if (st->twitch && ((random() % 4) == 0))
              randomize_square_colors (st->squares, st->nsquares, st->ncolors);
            else
              s->color = random() % st->ncolors;
          }
      }
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->backb) 
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = st->window;
      info[0].swap_action = XdbeUndefined;
      XdbeSwapBuffers (st->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    if (st->dbuf)
      {
        XCopyArea (st->dpy, st->b, st->window, st->gc, 0, 0, 
                   st->xgwa.width, st->xgwa.height, 0, 0);
        st->b = (st->b == st->ba ? st->bb : st->ba);
      }

  return st->delay;
}


static void
popsquares_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  int x, y;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->sw = st->xgwa.width / st->subdivision;
  st->sh = st->xgwa.height / st->subdivision;
  st->gw = st->sw ? st->xgwa.width / st->sw : 0;
  st->gh = st->sh ? st->xgwa.height / st->sh : 0;
  st->nsquares = st->gw * st->gh;
  free (st->squares);
  if (st->nsquares < 1) st->nsquares = 1;
  st->squares = (square *) calloc (st->nsquares, sizeof(square));

  for (y = 0; y < st->gh; y++)
    for (x = 0; x < st->gw; x++) 
      {
        square *s = (square *) &st->squares[st->gw * y + x];
        s->w = st->sw;
        s->h = st->sh;
        s->x = x * st->sw;
        s->y = y * st->sh;
      }

  randomize_square_colors(st->squares, st->nsquares, st->ncolors);

  if (st->dbuf) {
    XFreePixmap (dpy, st->ba);
    XFreePixmap (dpy, st->bb);
    st->ba = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height, st->xgwa.depth);
    st->bb = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height, st->xgwa.depth);
    st->b = st->ba;
  }
}

static Bool
popsquares_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
popsquares_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *popsquares_defaults [] = {
  ".background: #0000FF",
  ".foreground: #00008B",
  "*delay: 25000",
  "*subdivision:  5",
  "*border: 1",
  "*ncolors: 128",
  "*twitch: False",
  "*doubleBuffer: False",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE: True",
  "*useDBEClear: True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec popsquares_options [] = {
  { "-fg", ".foreground", XrmoptionSepArg, 0},
  { "-bg", ".background", XrmoptionSepArg, 0},
  { "-delay",     ".delay", XrmoptionSepArg, 0 },
  { "-subdivision", ".subdivision", XrmoptionSepArg, 0 },
  { "-border", ".border", XrmoptionSepArg, 0},
  { "-ncolors",   ".ncolors", XrmoptionSepArg, 0 },
  { "-twitch",    ".twitch", XrmoptionNoArg, "True" },
  { "-no-twitch", ".twitch", XrmoptionNoArg, "False" },
  { "-db",        ".doubleBuffer", XrmoptionNoArg, "True" },
  { "-no-db",     ".doubleBuffer", XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("PopSquares", popsquares)
