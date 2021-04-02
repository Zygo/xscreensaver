/* xscreensaver, Copyright (c) 2007-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * cwaves -- languid sinusoidal colors.
 */

#include "screenhack.h"
#include <stdio.h>
#include "ximage-loader.h"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef struct {
  double scale;
  double offset;
  double delta;
} wave;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC gc;
  int delay;
  int scale;
  int ncolors;
  XColor *colors;

  int nwaves;
  wave *waves;
  int debug_p;

} state;


static void *
cwaves_init (Display *dpy, Window window)
{
  int i;
  XGCValues gcv;
  state *st = (state *) calloc (1, sizeof (*st));

  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->debug_p = get_boolean_resource (dpy, "debug", "Boolean");
  /* Xft uses 'scale' */
  st->scale = get_integer_resource (dpy, "waveScale", "Integer");
  if (st->scale <= 0) st->scale = 1;
  st->ncolors = get_integer_resource (dpy, "ncolors", "Integer");
  if (st->ncolors < 4) st->ncolors = 4;
  st->colors = (XColor *) malloc (sizeof(*st->colors) * (st->ncolors+1));
  make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                        st->colors, &st->ncolors,
                        True, 0, False);

  st->gc = XCreateGC (st->dpy, st->window, 0, &gcv);
  st->delay = get_integer_resource (dpy, "delay", "Integer");

  st->nwaves  = get_integer_resource (dpy, "nwaves", "Integer");
  st->waves  = (wave *) calloc (st->nwaves,  sizeof(*st->waves));

  for (i = 0; i < st->nwaves; i++)
    {
      st->waves[i].scale  = frand(0.03) + 0.005;
      st->waves[i].offset = frand(M_PI);
      st->waves[i].delta  = (BELLRAND(2)-1) / 15.0;
    }

  return st;
}


static unsigned long
cwaves_draw (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;
  int i, x;

  for (i = 0; i < st->nwaves; i++)
    st->waves[i].offset += st->waves[i].delta;

  for (x = 0; x < st->xgwa.width; x += st->scale)
    {
      double v = 0;
      int j;
      for (i = 0; i < st->nwaves; i++)
        v += cos ((x * st->waves[i].scale) - st->waves[i].offset);
      v /= st->nwaves;

      j = st->ncolors * (v/2 + 0.5);
      if (j < 0 || j >= st->ncolors) abort();
      XSetForeground (st->dpy, st->gc, st->colors[j].pixel);
      XFillRectangle (st->dpy, st->window, st->gc, 
                      x, 0, st->scale, st->xgwa.height);
    }

  if (st->debug_p)
    {
      int wh = (st->xgwa.height / (st->nwaves + 1)) * 0.9;
      int i;
      XSetLineAttributes (st->dpy, st->gc, 2, LineSolid, CapRound, JoinRound);
      XSetForeground (st->dpy, st->gc, BlackPixelOfScreen (st->xgwa.screen));
      for (i = 0; i < st->nwaves; i++)
        {
          int y = st->xgwa.height * i / (st->nwaves + 1);
          int ox = -1, oy = -1;

          for (x = 0; x < st->xgwa.width; x += st->scale)
            {
              int yy;
              double v = 0;
              v = cos ((x * st->waves[i].scale) - st->waves[i].offset);
              v /= 2;

              yy = y + wh/2 + (wh * v);
              if (ox == -1)
                ox = x, oy = yy;
              XDrawLine (st->dpy, st->window, st->gc, ox, oy, x, yy);
              ox = x;
              oy = yy;
            }
        }

      {
        int y = st->xgwa.height * i / (st->nwaves + 1);
        int ox = -1, oy = -1;

        for (x = 0; x < st->xgwa.width; x += st->scale)
          {
            int yy;
            double v = 0;
            for (i = 0; i < st->nwaves; i++)
              v += cos ((x * st->waves[i].scale) - st->waves[i].offset);
            v /= st->nwaves;
            v /= 2;

            yy = y + wh/2 + (wh * v);
            if (ox == -1)
              ox = x, oy = yy;
            XDrawLine (st->dpy, st->window, st->gc, ox, oy, x, yy);
            ox = x;
            oy = yy;
          }
      }
    }

  return st->delay;
}


static void
cwaves_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  state *st = (state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
}

static Bool
cwaves_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  state *st = (state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual,
                            st->xgwa.colormap,
                            st->colors, &st->ncolors,
                            True, 0, False);
      return True;
    }
  return False;
}

static void
cwaves_free (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;
  XFreeGC (dpy, st->gc);
  free (st->colors);
  free (st->waves);
  free (st);
}


static const char *cwaves_defaults [] = {
  ".background:		   black",
  ".foreground:		   white",
  "*ncolors:		   600",
  "*nwaves:		   15",
  "*waveScale:		   2",
  "*debug:		   False",
  "*delay:		   20000",
#ifdef HAVE_MOBILE
  "*ignoreRotation:        True",
#endif
  0
};

static XrmOptionDescRec cwaves_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-waves",		".nwaves",		XrmoptionSepArg, 0 },
  { "-colors",		".ncolors",		XrmoptionSepArg, 0 },
  { "-scale",		".waveScale",		XrmoptionSepArg, 0 },
  { "-debug",		".debug",		XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("CWaves", cwaves)
