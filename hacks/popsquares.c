/* Copyright (c) 2003 Levi Burton <ldb@scoundrelz.net>
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

void
randomize_square_colors(square *squares, int nsquares, int ncolors)
{
  int i;
  square *s = squares;
  for (i = 0; i < nsquares; i++) 
    s[i].color = random() % ncolors;
}


char *progclass = "popsquares";

char *defaults [] = {
  ".background: blue",
  ".foreground: blue4",
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
  0
};

XrmOptionDescRec options [] = {
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

void 
screenhack (Display *dpy, Window window)
{
  int delay = get_integer_resource ("delay", "Integer");
  int subdivision = get_integer_resource("subdivision", "Integer");
  int border = get_integer_resource("border", "Integer");
  int ncolors = get_integer_resource("ncolors", "Integer");
  Bool twitch = get_boolean_resource("twitch", "Boolean");
  Bool dbuf = get_boolean_resource("doubleBuffer", "Boolean");
  int x, y;
  int sw, sh, gw, gh, nsquares = 0;
  double s1, v1, s2, v2 = 0;
  int h1, h2 = 0;
  /* Not sure how to use DBEClear */
  /* Bool dbeclear_p = get_boolean_resource("useDBEClear", "Boolean"); */
  XColor *colors = 0;
  XColor fg, bg;
  XGCValues gcv;
  GC gc; 
  square *squares;
  XWindowAttributes xgwa;
  Pixmap b=0, ba=0, bb=0;
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb = 0;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  
  XGetWindowAttributes (dpy, window, &xgwa);

  fg.pixel = get_pixel_resource ("foreground", "Foreground", dpy, xgwa.colormap);
  bg.pixel = get_pixel_resource ("background", "Background", dpy, xgwa.colormap);

  XQueryColor (dpy, xgwa.colormap, &fg);
  XQueryColor (dpy, xgwa.colormap, &bg);

  sw = xgwa.width / subdivision;
  sh = xgwa.height / subdivision;
  gw = xgwa.width / sw;
  gh = xgwa.height / sh;
  nsquares = gw * gh;

  gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

  colors = (XColor *) calloc (ncolors, sizeof(XColor));
  squares = (square *) calloc (nsquares, sizeof(square));

  rgb_to_hsv (fg.red, fg.green, fg.blue, &h1, &s1, &v1);
  rgb_to_hsv (bg.red, bg.green, bg.blue, &h2, &s2, &v2);
  make_color_ramp (dpy, xgwa.colormap,
                   h1, s1, v1,
                   h2, s2, v2,
                   colors, &ncolors,  /* would this be considered a value-result argument? */
                   True, True, False);
  if (ncolors < 2)
    {
      fprintf (stderr, "%s: insufficient colors!\n", progname);
      exit (1);
    }

  for (y = 0; y < gh; y++)
    for (x = 0; x < gw; x++) 
      {
        square *s = (square *) &squares[gw * y + x];
        s->w = sw;
        s->h = sh;
        s->x = x * sw;
        s->y = y * sh;
      }

  randomize_square_colors(squares, nsquares, ncolors);

  if (dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      b = xdbe_get_backbuffer (dpy, window, XdbeUndefined);
      backb = b;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
      if (!b)                      
        {
          ba = XCreatePixmap (dpy, window, xgwa.width, xgwa.height, xgwa.depth);
          bb = XCreatePixmap (dpy, window, xgwa.width, xgwa.height, xgwa.depth);
          b = ba;
        }
    }
  else 
    {
      b = window;
    }

  while (1) 
    {
      for (y = 0; y < gh; y++)
        for (x = 0; x < gw; x++) 
          {
            square *s = (square *) &squares[gw * y + x];
            XSetForeground (dpy, gc, colors[s->color].pixel);
            XFillRectangle (dpy, b, gc, s->x, s->y, 
                            border ? s->w - border : s->w, 
                            border ? s->h - border : s->h);
            s->color++;
            if (s->color == ncolors)
              {
                if (twitch && ((random() % 4) == 0))
                  randomize_square_colors (squares, nsquares, ncolors);
                else
                  s->color = random() % ncolors;
              }
          }
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      if (backb) 
        {
          XdbeSwapInfo info[1];
          info[0].swap_window = window;
          info[0].swap_action = XdbeUndefined;
          XdbeSwapBuffers (dpy, info, 1);
        }
      else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
        if (dbuf)
          {
            XCopyArea (dpy, b, window, gc, 0, 0, 
                       xgwa.width, xgwa.height, 0, 0);
            b = (b == ba ? bb : ba);
          }

      XSync (dpy, False);
      screenhack_handle_events (dpy);
      if (delay) 
        usleep (delay);
    }
}
