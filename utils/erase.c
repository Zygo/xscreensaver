/* erase.c: Erase the screen in various more or less interesting ways.
 * Copyright (c) 1997-2001 Jamie Zawinski <jwz@jwz.org>
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

extern char *progname;

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))

#define LITTLE_NAP 5000   /* 1/200th sec */

typedef void (*Eraser) (Display *dpy, Window window, GC gc,
			int width, int height, int total_msecs);


static unsigned long
millitime (void)
{
  struct timeval tt;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tz;
  gettimeofday (&tt, &tz);
# else
  gettimeofday (&tt);
# endif
  return (tt.tv_sec * 1000) + (tt.tv_usec / 1000);
}


static void
random_lines (Display *dpy, Window window, GC gc,
	      int width, int height, int total_msecs)
{
  int granularity = 50;

  Bool horiz_p = (random() & 1);
  int max = (horiz_p ? height : width);
  int *lines = (int *) calloc(max, sizeof(*lines));
  int oi = -1;
  int i;
  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  for (i = 0; i < max; i++)
    lines[i] = i;

  for (i = 0; i < max; i++)
    {
      int t, r;
      t = lines[i];
      r = random() % max;
      lines[i] = lines[r];
      lines[r] = t;
    }

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      i /= granularity;

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int j;
          for (j = 0; j < granularity; j++)
            {
              int ii = i * granularity + j;
              if (horiz_p)
                XDrawLine (dpy, window, gc, 0, lines[ii], width, lines[ii]);
              else
                XDrawLine (dpy, window, gc, lines[ii], 0, lines[ii], height);
              hits++;
            }
          XSync (dpy, False);
        }

      oi = i;
      tick = millitime();
    }

  free(lines);
}


static void
venetian (Display *dpy, Window window, GC gc,
	  int width, int height, int total_msecs)
{
  Bool horiz_p = (random() & 1);
  Bool flip_p = (random() & 1);
  int max = (horiz_p ? height : width);
  int *lines = (int *) calloc(max, sizeof(*lines));
  int i, j;
  int oi = -1;

  unsigned long start_tick = millitime();
  unsigned long end_tick = (start_tick +
                            (1.5 * total_msecs));  /* this one needs more */
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  j = 0;
  for (i = 0; i < max*2; i++)
    {
      int line = ((i / 16) * 16) - ((i % 16) * 15);
      if (line >= 0 && line < max)
	lines[j++] = (flip_p ? max - line : line);
    }

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int k;
          for (k = oi; k <= i; k++)
            {
              if (horiz_p)
                XDrawLine(dpy,window, gc, 0, lines[k], width, lines[k]);
              else
                XDrawLine(dpy,window, gc, lines[k], 0, lines[k], height);
              hits++;
            }
          XSync (dpy, False);
        }

      oi = i;
      tick = millitime();
    }
  free(lines);
}


static void
triple_wipe (Display *dpy, Window window, GC gc,
	     int width, int height, int total_msecs)
{
  Bool flip_x = random() & 1;
  Bool flip_y = random() & 1;
  int max = width + (height / 2);
  int *lines = (int *)calloc(max, sizeof(int));
  int i;
  int oi = -1;

  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  for(i = 0; i < width/2; i++)
    lines[i] = i*2+height;
  for(i = 0; i < height/2; i++)
    lines[i+width/2] = i*2;
  for(i = 0; i < width/2; i++)
    lines[i+width/2+height/2] = width-i*2-(width%2?0:1)+height;

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);
      int x, y, x2, y2;

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int k;
          for (k = oi; k <= i; k++)
            {
              if (lines[k] < height)
                x = 0, y = lines[k], x2 = width, y2 = y;
              else
                x = lines[k]-height, y = 0, x2 = x, y2 = height;

              if (flip_x)
                x = width-x, x2 = width-x2;
              if (flip_y)
                y = height-y, y2 = height-y2;

              XDrawLine (dpy, window, gc, x, y, x2, y2);
              hits++;
            }
          XSync (dpy, False);
        }

      oi = i;
      tick = millitime();
    }
  free(lines);
}


static void
quad_wipe (Display *dpy, Window window, GC gc,
	   int width, int height, int total_msecs)
{
  Bool flip_x = random() & 1;
  Bool flip_y = random() & 1;
  int max = width + height;
  int *lines = (int *)calloc(max, sizeof(int));
  int i;
  int oi = -1;

  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  for (i = 0; i < max/4; i++)
    {
      lines[i*4]   = i*2;
      lines[i*4+1] = height-i*2-(height%2?0:1);
      lines[i*4+2] = height+i*2;
      lines[i*4+3] = height+width-i*2-(width%2?0:1);
    }

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int k;
          for (k = oi; k <= i; k++)
            {
              int x, y, x2, y2;
              if (lines[k] < height)
                x = 0, y = lines[k], x2 = width, y2 = y;
              else
                x = lines[k]-height, y = 0, x2 = x, y2 = height;

              if (flip_x)
                x = width-x, x2 = width-x2;
              if (flip_y)
                y = height-y, y2 = height-y2;

              XDrawLine (dpy, window, gc, x, y, x2, y2);
              hits++;
            }
          XSync (dpy, False);
        }

      oi = i;
      tick = millitime();
    }

  free(lines);
}



static void
circle_wipe (Display *dpy, Window window, GC gc,
	     int width, int height, int total_msecs)
{
  int max = 360 * 64;
  int start = random() % max;
  int rad = (width > height ? width : height);
  int flip_p = random() & 1;
  int oth;

  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;

  int hits = 0;
  int nonhits = 0;

  oth = (flip_p ? max : 0);
  while (tick < end_tick)
    {
      int th = (max * (tick - start_tick)) / (end_tick - start_tick);
      if (flip_p)
        th = (360 * 64) - th;

      if (th == oth)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   (start+oth)%(360*64),
                   (th-oth));
          hits++;
          XSync (dpy, False);
        }

      oth = th;
      tick = millitime();
    }
}


static void
three_circle_wipe (Display *dpy, Window window, GC gc,
		   int width, int height, int total_msecs)
{
  int max = (360 * 64) / 6;
  int start = random() % max;
  int rad = (width > height ? width : height);
  int oth;

  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;

  int hits = 0;
  int nonhits = 0;

  oth = 0;
  while (tick < end_tick)
    {
      int th = (max * (tick - start_tick)) / (end_tick - start_tick);

      if (th == oth)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int off = 0;
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   (start+off+oth)%(360*64),
                   (th-oth));
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   ((start+off-oth))%(360*64),
                   -(th-oth));

          off += max + max;
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   (start+off+oth)%(360*64),
                   (th-oth));
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   ((start+off-oth))%(360*64),
                   -(th-oth));

          off += max + max;
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   (start+off+oth)%(360*64),
                   (th-oth));
          XFillArc(dpy, window, gc,
                   (width/2)-rad, (height/2)-rad, rad*2, rad*2,
                   ((start+off-oth))%(360*64),
                   -(th-oth));

          hits++;
          XSync (dpy, False);
        }

      oth = th;
      tick = millitime();
    }
}


static void
squaretate (Display *dpy, Window window, GC gc,
	    int width, int height, int total_msecs)
{
  int max = ((width > height ? width : width) * 2);
  int oi = -1;
  Bool flip = random() & 1;

  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

#define DRAW() \
      if (flip) { \
	points[0].x = width-points[0].x; \
	points[1].x = width-points[1].x; \
        points[2].x = width-points[2].x; } \
      XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin)

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          XPoint points [3];
          points[0].x = 0;
          points[0].y = 0;
          points[1].x = width;
          points[1].y = 0;
          points[2].x = 0;
          points[2].y = points[0].y + ((i * height) / max);
          DRAW();

          points[0].x = 0;
          points[0].y = 0;
          points[1].x = 0;
          points[1].y = height;
          points[2].x = ((i * width) / max);
          points[2].y = height;
          DRAW();

          points[0].x = width;
          points[0].y = height;
          points[1].x = 0;
          points[1].y = height;
          points[2].x = width;
          points[2].y = height - ((i * height) / max);
          DRAW();

          points[0].x = width;
          points[0].y = height;
          points[1].x = width;
          points[1].y = 0;
          points[2].x = width - ((i * width) / max);
          points[2].y = 0;
          DRAW();
          hits++;
          XSync (dpy, True);
        }

      oi = i;
      tick = millitime();
    }
#undef DRAW
}


/* from Frederick Roeber <roeber@netscape.com> */
static void
fizzle (Display *dpy, Window window, GC gc,
        int width, int height, int total_msecs)
{
  /* These dimensions must be prime numbers.  They should be roughly the
     square root of the width and height. */
# define BX 41
# define BY 31
# define SIZE (BX*BY)

  int array[SIZE];
  int i, j;
  int oi = -1;
  XPoint *skews;
  XPoint points[250];
  int npoints = 0;
  int nx, ny;

  unsigned long start_tick = millitime();
  unsigned long end_tick = (start_tick +
                            (2.5 * total_msecs));  /* this one needs more */
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  /* Distribute the numbers [0,SIZE) randomly in the array */
  {
    int indices[SIZE];

    for( i = 0; i < SIZE; i++ ) {
      array[i] = -1;
      indices[i] = i;
    } 

    for( i = 0; i < SIZE; i++ ) {
      j = random()%(SIZE-i);
      array[indices[j]] = i;
      indices[j] = indices[SIZE-i-1];
    }
  }

  /* nx, ny are the number of cells across and down, rounded up */
  nx = width  / BX + (0 == (width %BX) ? 0 : 1);
  ny = height / BY + (0 == (height%BY) ? 0 : 1);
  skews = (XPoint *)malloc(sizeof(XPoint) * (nx*ny));
  if( (XPoint *)0 != skews ) {
    for( i = 0; i < nx; i++ ) {
      for( j = 0; j < ny; j++ ) {
        skews[j * nx + i].x = random()%BX;
        skews[j * nx + i].y = random()%BY;
      }
    }
  }

# define SKEWX(cx, cy) (((XPoint *)0 == skews)?0:skews[cy*nx + cx].x)
# define SKEWY(cx, cy) (((XPoint *)0 == skews)?0:skews[cy*nx + cx].y)

  while (tick < end_tick)
    {
      int i = (SIZE * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int j;
          for (j = oi; j < i; j++)
            {
              int x = array[j] % BX;
              int y = array[j] / BX;
              int iy, cy;
              for (iy = 0, cy = 0; iy < height; iy += BY, cy++)
                {
                  int ix, cx;
                  for( ix = 0, cx = 0; ix < width; ix += BX, cx++ ) {
                    int xx = ix + (SKEWX(cx, cy) + x*((cx%(BX-1))+1))%BX;
                    int yy = iy + (SKEWY(cx, cy) + y*((cy%(BY-1))+1))%BY;
                    if (xx < width && yy < height)
                      {
                        points[npoints].x = xx;
                        points[npoints].y = yy;
                        if (++npoints == countof(points))
                          {
                            XDrawPoints(dpy, window, gc, points, npoints,
                                        CoordModeOrigin);
                            XSync (dpy, False);
                            npoints = 0;
                          }
                      }
                  }
                }
            }
          hits++;
        }

      oi = i;
      tick = millitime();
    }

  if (npoints > 100)
    {
      XDrawPoints(dpy, window, gc, points, npoints, CoordModeOrigin);
      XSync (dpy, False);
      usleep (10000);
    }

# undef SKEWX
# undef SKEWY
# undef BX
# undef BY
# undef SIZE

  if (skews) free(skews);
}


/* from Rick Campbell <rick@campbellcentral.org> */
static void
spiral (Display *dpy, Window window, GC context,
        int width, int height, int total_msecs)
{
  int granularity = 1; /* #### */

  double pi2 = (M_PI + M_PI);
  int loop_count = 10;
  int angle_step = 1000 / 8;  /* disc granularity is 8 degrees */
  int max = pi2 * angle_step;
  double angle;
  int arc_limit;
  int arc_max_limit;
  int length_step;
  XPoint points [3];

  total_msecs *= 2.5;  /* this one needs more */

  angle = 0.0;
  arc_limit = 1;
  arc_max_limit = (ceil (sqrt ((width * width) + (height * height))) / 2.0);
  length_step = ((arc_max_limit + loop_count - 1) / loop_count);
  arc_max_limit += length_step;
  points [0].x = width / 2;
  points [0].y = height / 2;
  points [1].x = points [0].x + length_step;
  points [1].y = points [0].y;
  points [2].x = points [1].x;
  points [2].y = points [1].y;

  for (arc_limit = length_step;
       arc_limit < arc_max_limit;
       arc_limit += length_step)
    {
      int arc_length = length_step;
      int length_base = arc_limit;

      unsigned long start_tick = millitime();
      unsigned long end_tick = start_tick + (total_msecs /
                                             (arc_max_limit / length_step));
      unsigned long tick = start_tick;
      int hits = 0;
      int nonhits = 0;
      int i = 0;
      int oi = -1;

#if 0
      int max2 = max / granularity;
      while (i < max2)
#else
      while (tick < end_tick)
#endif
        {
          i = (max * (tick - start_tick)) / (end_tick - start_tick);
          if (i > max) i = max;

          i /= granularity;

          if (i == oi)
            {
              usleep (LITTLE_NAP);
              nonhits++;
            }
          else
            {
              int j, k;
#if 0
              for (k = oi; k <= i; k++)
#else
              k = i;
#endif
                {
                  for (j = 0; j < granularity; j++)
                    {
                      int ii = k * granularity + j;
                      angle = ii / (double) angle_step;
                      arc_length = length_base + ((length_step * angle) / pi2);
                      points [1].x = points [2].x;
                      points [1].y = points [2].y;
                      points [2].x = points [0].x +
                        (int)(cos(angle) * arc_length);
                      points [2].y = points [0].y +
                        (int)(sin(angle) * arc_length);
                      XFillPolygon (dpy, window, context, points, 3, Convex,
                                    CoordModeOrigin);
                      hits++;
                    }
                }
              XSync (dpy, False);
            }

          oi = i;
          tick = millitime();
        }
    }
}


#undef MAX
#undef MIN
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

/* from David Bagley <bagleyd@tux.org> */
static void
random_squares(Display * dpy, Window window, GC gc,
               int width, int height, int total_msecs)
{
  int granularity = 20;

  int randsize = MAX(1, MIN(width, height) / (16 + (random() % 32)));
  int max = (height / randsize + 1) * (width / randsize + 1);
  int *squares = (int *) calloc(max, sizeof (*squares));
  int i;
  int columns = width / randsize + 1;  /* Add an extra for roundoff */

  int oi = -1;
  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  for (i = 0; i < max; i++)
    squares[i] = i;

  for (i = 0; i < max; i++)
    {
      int t, r;
      t = squares[i];
      r = random() % max;
      squares[i] = squares[r];
      squares[r] = t;
    }

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      i /= granularity;

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int j;
          for (j = 0; j < granularity; j++)
            {
              int ii = i * granularity + j;

              XFillRectangle(dpy, window, gc,
                             (squares[ii] % columns) * randsize,
                             (squares[ii] / columns) * randsize,
                             randsize, randsize);
              hits++;
            }
        }
      XSync (dpy, False);

      oi = i;
      tick = millitime();
    }
  free(squares);
}

/* I first saw something like this, albeit in reverse, in an early Tetris
   implementation for the Mac.
    -- Torbjörn Andersson <torbjorn@dev.eurotime.se>
 */
static void
slide_lines (Display *dpy, Window window, GC gc,
             int width, int height, int total_msecs)
{
  int max = width;
  int dy = MAX (10, height/40);

  int oi = 0;
  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + total_msecs;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int y;
          int tick = 0;
          int from1 = oi;
          int to1 = i;
          int w = width-to1;
          int from2 = width - oi - w;
          int to2 = width - i - w;

          for (y = 0; y < height; y += dy)
            {
              if (++tick & 1)
                {
                  XCopyArea (dpy, window, window, gc, from1, y, w, dy, to1, y);
                  XFillRectangle (dpy, window, gc, from1, y, to1-from1, dy);
                }
              else
                {
                  XCopyArea (dpy, window, window, gc, from2, y, w, dy, to2, y);
                  XFillRectangle (dpy, window, gc, from2+w, y, to2-from2, dy);
                }
            }

          hits++;
          XSync (dpy, False);
        }

      oi = i;
      tick = millitime();
    }
}


/* from Frederick Roeber <roeber@xigo.com> */
static void
losira (Display * dpy, Window window, GC gc,
        int width, int height, int total_msecs)
{
  XGCValues gcv;
  XWindowAttributes wa;
  XColor white;
  GC white_gc; 
  XArc arc[2][8];
  double xx[8], yy[8], dx[8], dy[8];

  int i;
  int oi = 0;

  int max = width/2;
  int max_off = MAX(1, max / 12);

  int msecs1 = (0.55 * total_msecs);
  int msecs2 = (0.30 * total_msecs);
  int msecs3 = (0.15 * total_msecs);

  unsigned long start_tick = millitime();
  unsigned long end_tick = start_tick + msecs1;
  unsigned long tick = start_tick;
  int hits = 0;
  int nonhits = 0;

  XGetWindowAttributes(dpy, window, &wa);
  white.flags = DoRed|DoGreen|DoBlue;
  white.red = white.green = white.blue = 65535;
  XAllocColor(dpy, wa.colormap, &white);
  gcv.foreground = white.pixel;
  white_gc = XCreateGC(dpy, window, GCForeground, &gcv);

  /* Squeeze in from the sides */
  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int off = (max_off * (tick - start_tick)) / (end_tick - start_tick);

          int from1 = oi;
          int to1 = i;
          int w = max - to1 - off/2 + 1;
          int from2 = max+(to1-from1)+off/2;
          int to2 = max+off/2;

          if (w < 0)
            break;

          XCopyArea (dpy, window, window, gc, from1, 0, w, height, to1, 0);
          XCopyArea (dpy, window, window, gc, from2, 0, w, height, to2, 0);
          XFillRectangle (dpy, window, gc, from1, 0, (to1-from1), height);
          XFillRectangle (dpy, window, gc, to2+w, 0, from2+w,     height);
          XFillRectangle (dpy, window, white_gc, max-off/2, 0, off, height);
          hits++;
          XSync(dpy, False);
        }

      oi = i;
      tick = millitime();
    }


  XFillRectangle(dpy, window, white_gc, max-max_off/2, 0, max_off, height);

  /* Cap the top and bottom of the line */
  XFillRectangle(dpy, window, gc, max-max_off/2, 0, max_off, max_off/2);
  XFillRectangle(dpy, window, gc, max-max_off/2, height-max_off/2,
                 max_off, max_off/2);
  XFillArc(dpy, window, white_gc, max-max_off/2-1, 0,
           max_off-1, max_off-1, 0, 180*64);
  XFillArc(dpy, window, white_gc, max-max_off/2-1, height-max_off,
           max_off-1, max_off-1,
           180*64, 360*64);

  XFillRectangle(dpy, window, gc, 0,               0, max-max_off/2, height);
  XFillRectangle(dpy, window, gc, max+max_off/2-1, 0, max-max_off/2, height);
  XSync(dpy, False);

  /* Collapse vertically */
  start_tick = millitime();
  end_tick = start_tick + msecs2;
  tick = start_tick;

  max = height/2;
  oi = 0;
  while (tick < end_tick)
    {
      int i = (max * (tick - start_tick)) / (end_tick - start_tick);
      int x = (width-max_off)/2;
      int w = max_off;

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int off = (max_off * (tick - start_tick)) / (end_tick - start_tick);

          int from1 = oi;
          int to1 = i;
          int h = max - to1 - off/2;
          int from2 = max+(to1-from1)+off/2;
          int to2 = max+off/2;

          if (h < max_off/2)
            break;

          XCopyArea (dpy, window, window, gc, x, from1, w, h, x, to1);
          XCopyArea (dpy, window, window, gc, x, from2, w, h, x, to2);
          XFillRectangle(dpy, window, gc, x, from1, w, (to1 - from1));
          XFillRectangle(dpy, window, gc, x, to2+h, w, (to2 - from2));
          hits++;
          XSync(dpy, False);
        }

      oi = i;
      tick = millitime();
    }

  /* "This is Sci-Fi" */
  for( i = 0; i < 8; i++ ) {
    arc[0][i].width = arc[0][i].height = max_off;
    arc[1][i].width = arc[1][i].height = max_off;
    arc[0][i].x = arc[1][i].x = width/2;
    arc[0][i].y = arc[1][i].y = height/2;
    xx[i] = (double)(width/2)  - max_off/2;
    yy[i] = (double)(height/2) - max_off/2;
  }

  arc[0][0].angle1 = arc[1][0].angle1 =   0*64; arc[0][0].angle2 = arc[1][0].angle2 =  45*64;
  arc[0][1].angle1 = arc[1][1].angle1 =  45*64; arc[0][1].angle2 = arc[1][1].angle2 =  45*64;
  arc[0][2].angle1 = arc[1][2].angle1 =  90*64; arc[0][2].angle2 = arc[1][2].angle2 =  45*64;
  arc[0][3].angle1 = arc[1][3].angle1 = 135*64; arc[0][3].angle2 = arc[1][3].angle2 =  45*64;
  arc[0][4].angle1 = arc[1][4].angle1 = 180*64; arc[0][4].angle2 = arc[1][4].angle2 =  45*64;
  arc[0][5].angle1 = arc[1][5].angle1 = 225*64; arc[0][5].angle2 = arc[1][5].angle2 =  45*64;
  arc[0][6].angle1 = arc[1][6].angle1 = 270*64; arc[0][6].angle2 = arc[1][6].angle2 =  45*64;
  arc[0][7].angle1 = arc[1][7].angle1 = 315*64; arc[0][7].angle2 = arc[1][7].angle2 =  45*64;
  
  for( i = 0; i < 8; i++ ) {
    dx[i] =  cos((i*45 + 22.5)/360 * 2*M_PI);
    dy[i] = -sin((i*45 + 22.5)/360 * 2*M_PI);
  }

  gcv.line_width = 3;  
  XChangeGC(dpy, gc, GCLineWidth, &gcv);

  XClearWindow (dpy, window);
  XFillArc(dpy, window, white_gc,
           width/2-max_off/2-1, height/2-max_off/2-1,
           max_off-1, max_off-1,
           0, 360*64);
  XDrawLine(dpy, window, gc, 0, height/2-1, width, height/2-1);
  XDrawLine(dpy, window, gc, width/2-1, 0, width/2-1, height);
  XDrawLine(dpy, window, gc, width/2-1-max_off, height/2-1-max_off,
            width/2+max_off, height/2+max_off);
  XDrawLine(dpy, window, gc, width/2+max_off, height/2-1-max_off,
            width/2-1-max_off, height/2+max_off);

  XSync(dpy, False);


  /* Fan out */
  start_tick = millitime();
  end_tick = start_tick + msecs3;
  tick = start_tick;
  oi = 0;
  while (tick < end_tick)
    {
      int i = (max_off * (tick - start_tick)) / (end_tick - start_tick);

      if (i == oi)
        {
          usleep (LITTLE_NAP);
          nonhits++;
        }
      else
        {
          int j;
          for (j = 0; j < 8; j++)
            {
              xx[j] += 2*dx[j];
              yy[j] += 2*dy[j];
              arc[(i+1)%2][j].x = xx[j];
              arc[(i+1)%2][j].y = yy[j];
            }

          XFillRectangle (dpy, window, gc,
                          (width-max_off*5)/2, (height-max_off*5)/2,
                          max_off*5, max_off*5);
          XFillArcs(dpy, window, white_gc, arc[(i+1)%2], 8);
          XSync(dpy, False);
          hits++;
        }

      oi = i;
      tick = millitime();
    }
  
  XSync (dpy, False);

  /*XFreeColors(dpy, wa.colormap, &white.pixel, 1, 0);*/
  XFreeGC(dpy, white_gc);
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


static void
erase_window (Display *dpy, Window window, GC gc,
              int width, int height, int mode, int total_msecs)
{
  Bool verbose_p = False;
  unsigned long start = millitime();

  if (mode < 0 || mode >= countof(erasers))
    mode = random() % countof(erasers);

  (*(erasers[mode])) (dpy, window, gc, width, height, total_msecs);

  if (verbose_p)
    fprintf(stderr, "%s: eraser %d time: %4.2f sec\n",
            progname, mode, (millitime() - start) / 1000.0);

  XClearWindow (dpy, window);
  XSync(dpy, False);
  usleep (333333);  /* 1/3 sec */
}


void
erase_full_window(Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGCValues gcv;
  GC erase_gc;
  XColor black;
  int erase_msecs, erase_mode;
  char *s;

  s = get_string_resource("eraseSeconds", "Integer");
  if (s && *s)
    erase_msecs = 1000 * get_float_resource("eraseSeconds", "Float");
  else
    erase_msecs = 1000;

  if (erase_msecs < 10 || erase_msecs > 10000)
    erase_msecs = 1000;

  if (s) free(s);

  s = get_string_resource("eraseMode", "Integer");
  if (s && *s)
    erase_mode = get_integer_resource("eraseMode", "Integer");
  else
    erase_mode = -1;
  if (s) free(s);

  XGetWindowAttributes (dpy, window, &xgwa);
  black.flags = DoRed|DoGreen|DoBlue;
  black.red = black.green = black.blue = 0;
  XAllocColor(dpy, xgwa.colormap, &black);
  gcv.foreground = black.pixel;
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  erase_window (dpy, window, erase_gc, xgwa.width, xgwa.height,
		erase_mode, erase_msecs);
  XFreeColors(dpy, xgwa.colormap, &black.pixel, 1, 0);
  XFreeGC(dpy, erase_gc);
}
