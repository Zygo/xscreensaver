/* erase.c: Erase the screen in various more or less interesting ways.
 * Copyright (c) 1997-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Portions (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>:
 *   Permission to use in any way granted. Provided "as is" without expressed
 *   or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 *   PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#include "utils.h"
#include "yarandom.h"
#include "usleep.h"
#include "resources.h"
#include "erase.h"
#include <sys/time.h> /* for gettimeofday() */

extern char *progname;

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))

typedef void (*Eraser) (eraser_state *);

struct eraser_state {
  Display *dpy;
  Window window;
  GC fg_gc, bg_gc;
  int width, height;
  Eraser fn;

  double start_time, stop_time;
  double ratio, prev_ratio;

  /* data for random_lines, venetian, random_squares */
  Bool horiz_p;
  Bool flip_p;
  int nlines, *lines;

  /* data for triple_wipe, quad_wipe */
  Bool flip_x, flip_y;

  /* data for circle_wipe, three_circle_wipe */
  int start;

  /* data for random_squares */
  int cols;

  /* data for fizzle */
  unsigned short *fizzle_rnd;

};


static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
random_lines (eraser_state *st)
{
  int i;

  if (! st->lines)	/* first time */
    {
      st->horiz_p = (random() & 1);
      st->nlines = (st->horiz_p ? st->height : st->width);
      st->lines = (int *) calloc (st->nlines, sizeof(*st->lines));

      for (i = 0; i < st->nlines; i++)  /* every line */
        st->lines[i] = i;

      for (i = 0; i < st->nlines; i++)  /* shuffle */
        {
          int t, r;
          t = st->lines[i];
          r = random() % st->nlines;
          st->lines[i] = st->lines[r];
          st->lines[r] = t;
        }
    }

  for (i = st->nlines * st->prev_ratio;
       i < st->nlines * st->ratio;
       i++)
    {
      if (st->horiz_p)
        XDrawLine (st->dpy, st->window, st->bg_gc, 
                   0, st->lines[i], st->width, st->lines[i]);
      else
        XDrawLine (st->dpy, st->window, st->bg_gc, 
                   st->lines[i], 0, st->lines[i], st->height);
    }

  if (st->ratio >= 1.0)
    {
      free (st->lines);
      st->lines = 0;
    }
}


static void
venetian (eraser_state *st)
{
  int i;
  if (st->ratio == 0.0)
    {
      int j = 0;
      st->horiz_p = (random() & 1);
      st->flip_p = (random() & 1);
      st->nlines = (st->horiz_p ? st->height : st->width);
      st->lines = (int *) calloc (st->nlines, sizeof(*st->lines));

      for (i = 0; i < st->nlines * 2; i++)
        {
          int line = ((i / 16) * 16) - ((i % 16) * 15);
          if (line >= 0 && line < st->nlines)
            st->lines[j++] = (st->flip_p ? st->nlines - line : line);
        }
    }

  
  for (i = st->nlines * st->prev_ratio;
       i < st->nlines * st->ratio;
       i++)
    {
      if (st->horiz_p)
        XDrawLine (st->dpy, st->window, st->bg_gc, 
                   0, st->lines[i], st->width, st->lines[i]);
      else
        XDrawLine (st->dpy, st->window, st->bg_gc, 
                   st->lines[i], 0, st->lines[i], st->height);
    }

  if (st->ratio >= 1.0)
    {
      free (st->lines);
      st->lines = 0;
    }
}


static void
triple_wipe (eraser_state *st)
{
  int i;
  if (st->ratio == 0.0)
    {
      st->flip_x = random() & 1;
      st->flip_y = random() & 1;
      st->nlines = st->width + (st->height / 2);
      st->lines = (int *) calloc (st->nlines, sizeof(*st->lines));

      for (i = 0; i < st->width / 2; i++)
        st->lines[i] = i * 2 + st->height;
      for (i = 0; i < st->height / 2; i++)
        st->lines[i + st->width / 2] = i*2;
      for (i = 0; i < st->width / 2; i++)
        st->lines[i + st->width / 2 + st->height / 2] = 
          st->width - i * 2 - (st->width % 2 ? 0 : 1) + st->height;
    }

  for (i = st->nlines * st->prev_ratio;
       i < st->nlines * st->ratio;
       i++)
    {
      int x, y, x2, y2;

      if (st->lines[i] < st->height)
        x = 0, y = st->lines[i], x2 = st->width, y2 = y;
      else
        x = st->lines[i]-st->height, y = 0, x2 = x, y2 = st->height;

      if (st->flip_x)
        x = st->width - x, x2 = st->width - x2;
      if (st->flip_y)
        y = st->height - y, y2 = st->height - y2;

      XDrawLine (st->dpy, st->window, st->bg_gc, x, y, x2, y2);
    }

  if (st->ratio >= 1.0)
    {
      free (st->lines);
      st->lines = 0;
    }
}


static void
quad_wipe (eraser_state *st)
{
  int i;
  if (st->ratio == 0.0)
    {
      st->flip_x = random() & 1;
      st->flip_y = random() & 1;
      st->nlines = st->width + st->height;
      st->lines = (int *) calloc (st->nlines, sizeof(*st->lines));

      for (i = 0; i < st->nlines/4; i++)
        {
          st->lines[i*4]   = i*2;
          st->lines[i*4+1] = st->height - i*2 - (st->height % 2 ? 0 : 1);
          st->lines[i*4+2] = st->height + i*2;
          st->lines[i*4+3] = st->height + st->width - i*2
            - (st->width % 2 ? 0 : 1);
        }
    }

  for (i = st->nlines * st->prev_ratio;
       i < st->nlines * st->ratio;
       i++)
    {
      int x, y, x2, y2;
      if (st->lines[i] < st->height)
        x = 0, y = st->lines[i], x2 = st->width, y2 = y;
      else
        x = st->lines[i] - st->height, y = 0, x2 = x, y2 = st->height;

      if (st->flip_x)
        x = st->width-x, x2 = st->width-x2;
      if (st->flip_y)
        y = st->height-y, y2 = st->height-y2;

      XDrawLine (st->dpy, st->window, st->bg_gc, x, y, x2, y2);
    }

  if (st->ratio >= 1.0)
    {
      free (st->lines);
      st->lines = 0;
    }
}


static void
circle_wipe (eraser_state *st)
{
  int rad = (st->width > st->height ? st->width : st->height);
  int max = 360 * 64;
  int th, oth;

  if (st->ratio == 0.0)
    {
      st->flip_p = random() & 1;
      st->start = random() % max;
    }

  th  = max * st->ratio;
  oth = max * st->prev_ratio;
  if (st->flip_p)
    {
      th  = max - th;
      oth = max - oth;
    }
  XFillArc (st->dpy, st->window, st->bg_gc,
            (st->width  / 2) - rad,
            (st->height / 2) - rad, 
            rad*2, rad*2,
            (st->start + oth) % max,
            th-oth);
}


static void
three_circle_wipe (eraser_state *st)
{
  int rad = (st->width > st->height ? st->width : st->height);
  int max = 360 * 64;
  int th, oth;
  int i;

  if (st->ratio == 0.0)
    st->start = random() % max;

  th  = max/6 * st->ratio;
  oth = max/6 * st->prev_ratio;

  for (i = 0; i < 3; i++)
    {
      int off = i * max / 3;
      XFillArc (st->dpy, st->window, st->bg_gc,
                (st->width  / 2) - rad,
                (st->height / 2) - rad, 
                rad*2, rad*2,
                (st->start + off + oth) % max,
                th-oth);
      XFillArc (st->dpy, st->window, st->bg_gc,
                (st->width  / 2) - rad,
                (st->height / 2) - rad, 
                rad*2, rad*2,
                (st->start + off - oth) % max,
                oth-th);
    }
}


static void
squaretate (eraser_state *st)
{
  int max = ((st->width > st->height ? st->width : st->height) * 2);
  XPoint points [3];
  int i = max * st->ratio;

  if (st->ratio == 0.0)
    st->flip_p = random() & 1;
    
# define DRAW()						\
  if (st->flip_p) {					\
    points[0].x = st->width - points[0].x;		\
    points[1].x = st->width - points[1].x;		\
    points[2].x = st->width - points[2].x;		\
  }							\
  XFillPolygon (st->dpy, st->window, st->bg_gc,		\
		points, 3, Convex, CoordModeOrigin)

  points[0].x = 0;
  points[0].y = 0;
  points[1].x = st->width;
  points[1].y = 0;
  points[2].x = 0;
  points[2].y = points[0].y + ((i * st->height) / max);
  DRAW();

  points[0].x = 0;
  points[0].y = 0;
  points[1].x = 0;
  points[1].y = st->height;
  points[2].x = ((i * st->width) / max);
  points[2].y = st->height;
  DRAW();

  points[0].x = st->width;
  points[0].y = st->height;
  points[1].x = 0;
  points[1].y = st->height;
  points[2].x = st->width;
  points[2].y = st->height - ((i * st->height) / max);
  DRAW();

  points[0].x = st->width;
  points[0].y = st->height;
  points[1].x = st->width;
  points[1].y = 0;
  points[2].x = st->width - ((i * st->width) / max);
  points[2].y = 0;
  DRAW();
# undef DRAW
}


static void
fizzle (eraser_state *st)
{
  const double overshoot = 1.0625;

  unsigned int x, y, i;
  const unsigned int size = 256;
  unsigned short *rnd;
  XPoint *points;
  unsigned int npoints =
    (unsigned int)(size * size * st->ratio * overshoot) -
    (unsigned int)(size * size * st->prev_ratio * overshoot);

  if (st->ratio >= 1.0)
    {
      free (st->fizzle_rnd);
      st->fizzle_rnd = NULL;
      return;
    }

  if (! st->fizzle_rnd)
    {
      unsigned int chunks =
        ((st->width + size - 1) / size) * ((st->height + size - 1) / size);
      unsigned int i;

      st->fizzle_rnd =
        (unsigned short *) malloc (sizeof(unsigned short) * chunks);

      if (! st->fizzle_rnd)
        return;

      for (i = 0; i != chunks; i++)
        st->fizzle_rnd[i] = NRAND(0x10000) | 1; /* Seed can't be 0. */
    }

  points = (XPoint *) malloc ((npoints + 1) * sizeof(*points));
  if (! points) return;

  rnd = st->fizzle_rnd;

  for (y = 0; y < st->height; y += 256)
    {
      for (x = 0; x < st->width; x += 256)
        {
          unsigned int need0 = 0;
          unsigned short r = *rnd;
          for (i = 0; i != npoints; i++)
            {
              points[i].x = r % size + x;
              points[i].y = (r >> 8) % size + y;

              /* Xorshift. This has a period of 2^16, which exactly matches
                 the number of pixels in each 256x256 chunk.

                 Other shift constants are possible, but it's hard to say
                 which constants are best: a 2^16 period PRNG will never score
                 well on BigCrush.
               */
              r = (r ^ (r <<  3)) & 0xffff;
              r =  r ^ (r >>  5);
              r = (r ^ (r << 11)) & 0xffff;
              need0 |= (r == 0x8080); /* Can be anything, really. */
            }

          if (need0)
            {
              points[npoints].x = x;
              points[npoints].y = y;
            }

          XDrawPoints (st->dpy, st->window, st->bg_gc,
                       points, npoints + need0, CoordModeOrigin);
          *rnd = r;
          rnd++;
        }
    }
  free (points);
}


static void
spiral (eraser_state *st)
{
  int max_radius = (st->width > st->height ? st->width : st->height) * 0.7;
  int loops = 10;
  float max_th = M_PI * 2 * loops;
  int i;
  int steps = 360 * loops / 4;
  float off;

  if (st->ratio == 0.0)
    {
      st->flip_p = random() & 1;
      st->start = random() % 360;
    }

  off = st->start * M_PI / 180;

  for (i = steps * st->prev_ratio;
       i < steps * st->ratio;
       i++)
    {
      float th1 = i     * max_th / steps;
      float th2 = (i+1) * max_th / steps;
      int   r1  = i     * max_radius / steps;
      int   r2  = (i+1) * max_radius / steps;
      XPoint points[3];

      if (st->flip_p)
        {
          th1 = max_th - th1;
          th2 = max_th - th2;
        }

      points[0].x = st->width  / 2;
      points[0].y = st->height / 2;
      points[1].x = points[0].x + r1 * cos (off + th1);
      points[1].y = points[0].y + r1 * sin (off + th1);
      points[2].x = points[0].x + r2 * cos (off + th2);
      points[2].y = points[0].y + r2 * sin (off + th2);
/*  XFillRectangle(st->dpy, st->window, st->fg_gc,0,0,st->width, st->height);*/
      XFillPolygon (st->dpy, st->window, st->bg_gc,
                    points, 3, Convex, CoordModeOrigin);
    }
}


static void
random_squares (eraser_state *st)
{
  int i, size, rows;

  if (st->ratio == 0.0)
    {
      st->cols = 10 + random() % 30;
      size = st->width / st->cols;
      rows = (size ? (st->height / size) : 0) + 1;
      st->nlines = st->cols * rows;
      st->lines = (int *) calloc (st->nlines, sizeof(*st->lines));

      for (i = 0; i < st->nlines; i++)  /* every square */
        st->lines[i] = i;

      for (i = 0; i < st->nlines; i++)  /* shuffle */
        {
          int t, r;
          t = st->lines[i];
          r = random() % st->nlines;
          st->lines[i] = st->lines[r];
          st->lines[r] = t;
        }
    }

  size = st->width / st->cols;
  rows = (size ? (st->height / size) : 0) + 1;

  for (i = st->nlines * st->prev_ratio;
       i < st->nlines * st->ratio;
       i++)
    {
      int x = st->lines[i] % st->cols;
      int y = st->lines[i] / st->cols;
      XFillRectangle (st->dpy, st->window, st->bg_gc,
                      st->width  * x / st->cols,
                      st->height * y / rows,
                      size+1, size+1);
    }

  if (st->ratio >= 1.0)
    {
      free (st->lines);
      st->lines = 0;
    }
}


/* I first saw something like this, albeit in reverse, in an early Tetris
   implementation for the Mac.
    -- Torbjörn Andersson <torbjorn@dev.eurotime.se>
 */
static void
slide_lines (eraser_state *st)
{
  int max = st->width * 1.1;
  int nlines = 40;
  int h = st->height / nlines;
  int y, step;
  int tick = 0;

  if (h < 10)
    h = 10;

  step = (max * st->ratio) - (max * st->prev_ratio);
  if (step <= 0)
    step = 1;

  for (y = 0; y < st->height; y += h)
    {
      if (st->width <= step)
        ;
      else if (tick & 1)
        {
          XCopyArea (st->dpy, st->window, st->window, st->fg_gc,
                     0, y, st->width-step, h, step, y);
          XFillRectangle (st->dpy, st->window, st->bg_gc, 
                          0, y, step, h);
        }
      else
        {
          XCopyArea (st->dpy, st->window, st->window, st->fg_gc,
                     step, y, st->width-step, h, 0, y);
          XFillRectangle (st->dpy, st->window, st->bg_gc, 
                          st->width-step, y, step, h);
        }

      tick++;
    }
}


/* from Frederick Roeber <roeber@xigo.com> */
static void
losira (eraser_state *st)
{
  double mode1 = 0.55;
  double mode2 = mode1 + 0.30;
  double mode3 = 1.0;
  int radius = 10;

  if (st->ratio < mode1)		/* squeeze from the sides */
    {
      double ratio = st->ratio / mode1;
      double prev_ratio = st->prev_ratio / mode1;
      int max = st->width / 2;
      int step = (max * ratio) - (max * prev_ratio);

      if (step <= 0)
        step = 1;

      /* pull from left */
      XCopyArea (st->dpy, st->window, st->window, st->fg_gc,
                 0, 0, max - step, st->height, step, 0);
      XFillRectangle (st->dpy, st->window, st->bg_gc, 
                      0, 0, max * ratio, st->height);

      /* pull from right */
      XCopyArea (st->dpy, st->window, st->window, st->fg_gc,
                 max+step, 0, max - step, st->height, max, 0);
      XFillRectangle (st->dpy, st->window, st->bg_gc, 
                      max + max*(1-ratio), 0, max, st->height);

      /* expand white from center */
      XFillRectangle (st->dpy, st->window, st->fg_gc,
                      max - (radius * ratio), 0,
                      radius * ratio * 2, st->height);
    }
  else if (st->ratio < mode2)		/* squeeze from the top/bottom */
    {
      double ratio = (st->ratio - mode1) / (mode2 - mode1);
      int max = st->height / 2;

      /* fill middle */
      XFillRectangle (st->dpy, st->window, st->fg_gc,
                      st->width/2 - radius,
                      max * ratio,
                      radius*2, st->height * (1 - ratio));

      /* fill left and right */
      XFillRectangle (st->dpy, st->window, st->bg_gc, 
                      0, 0, st->width/2 - radius, st->height);
      XFillRectangle (st->dpy, st->window, st->bg_gc, 
                      st->width/2 + radius, 0, st->width/2, st->height);

      /* fill top and bottom */
      XFillRectangle (st->dpy, st->window, st->bg_gc,
                      0, 0, st->width, max * ratio);
      XFillRectangle (st->dpy, st->window, st->bg_gc,
                      0, st->height - (max * ratio),
                      st->width, max);

      /* cap top */
      XFillArc (st->dpy, st->window, st->fg_gc,
                st->width/2 - radius,
                max * ratio - radius,
                radius*2, radius*2,
                0, 180*64);

      /* cap bottom */
      XFillArc (st->dpy, st->window, st->fg_gc,
                st->width/2 - radius,
                st->height - (max * ratio + radius),
                radius*2, radius*2,
                180*64, 180*64);
    }
  else if (st->ratio < mode3)		/* starburst */
    {
      double ratio = (st->ratio - mode2) / (mode3 - mode2);
      double r2 = ratio * radius * 4;
      XArc arc[9];
      int i;
      int angle = 360*64/countof(arc);

      for (i = 0; i < countof(arc); i++)
        {
          double th;
          arc[i].angle1 = angle * i;
          arc[i].angle2 = angle;
          arc[i].width  = radius*2 * (1 + ratio);
          arc[i].height = radius*2 * (1 + ratio);
          arc[i].x = st->width  / 2 - radius;
          arc[i].y = st->height / 2 - radius;

          th = ((arc[i].angle1 + (arc[i].angle2 / 2)) / 64.0 / 180 * M_PI);

          arc[i].x += r2 * cos (th);
          arc[i].y -= r2 * sin (th);
        }

      XFillRectangle (st->dpy, st->window, st->bg_gc, 
                      0, 0, st->width, st->height);
      XFillArcs (st->dpy, st->window, st->fg_gc, arc, countof(arc));
    }
}


static Eraser erasers[] = {
  random_lines,
  venetian,
  triple_wipe,
  quad_wipe,
  circle_wipe,
  three_circle_wipe,
  squaretate,
  fizzle,
  spiral,
  random_squares,
  slide_lines,
  losira,
};


static eraser_state *
eraser_init (Display *dpy, Window window)
{
  eraser_state *st = (eraser_state *) calloc (1, sizeof(*st));
  XWindowAttributes xgwa;
  XGCValues gcv;
  unsigned long fg, bg;
  double duration;
  int which;
  char *s;

  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (dpy, window, &xgwa);
  st->width = xgwa.width;
  st->height = xgwa.height;

  bg = get_pixel_resource (dpy, xgwa.colormap, "background", "Background");
  fg = get_pixel_resource (dpy, xgwa.colormap, "foreground", "Foreground");

  gcv.foreground = fg;
  gcv.background = bg;
  st->fg_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);
  gcv.foreground = bg;
  gcv.background = fg;
  st->bg_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

# ifdef HAVE_COCOA
  /* Pretty much all of these leave turds if AA is on. */
  jwxyz_XSetAntiAliasing (st->dpy, st->fg_gc, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->bg_gc, False);
# endif

  s = get_string_resource (dpy, "eraseMode", "Integer");
  if (!s || !*s)
    which = -1;
  else
    which = get_integer_resource(dpy, "eraseMode", "Integer");
  if (s) free (s);

  if (which < 0 || which >= countof(erasers))
    which = random() % countof(erasers);
  st->fn = erasers[which];

  duration = get_float_resource (dpy, "eraseSeconds", "Float");
  if (duration < 0.1 || duration > 10)
    duration = 1;

  st->start_time = double_time();
  st->stop_time = st->start_time + duration;

  XSync (st->dpy, False);

  return st;
}


void
eraser_free (eraser_state *st)
{
  XClearWindow (st->dpy, st->window); /* Final draw is black-on-black. */
  st->ratio = 1.0;
  st->fn (st); /* Free any memory. May also draw, but that doesn't matter. */
  XFreeGC (st->dpy, st->fg_gc);
  XFreeGC (st->dpy, st->bg_gc);
  free (st);
}

eraser_state *
erase_window (Display *dpy, Window window, eraser_state *st)
{
  double now, duration;
  Bool first_p = False;
  if (! st)
    {
      first_p = True;
      st = eraser_init (dpy, window);
    }

  now = (first_p ? st->start_time : double_time());
  duration = st->stop_time - st->start_time;

  st->prev_ratio = st->ratio;
  st->ratio = (now - st->start_time) / duration;

  if (st->ratio < 1.0)
    {
      st->fn (st);
      XSync (st->dpy, False);
    }
  else
    {
      eraser_free (st);
      st = 0;
    }
  return st;
}
