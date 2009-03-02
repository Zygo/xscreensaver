/* erase.c: Erase the screen in various more or less interesting ways.
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 * Permission to use in any way granted. Provided "as is" without expressed
 * or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 * PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#include "utils.h"
#include "yarandom.h"
#include "usleep.h"
#include "resources.h"

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))

typedef void (*Eraser) (Display *dpy, Window window, GC gc,
			int width, int height, int delay, int granularity);


static void
random_lines (Display *dpy, Window window, GC gc,
	      int width, int height, int delay, int granularity)
{
  Bool horiz_p = (random() & 1);
  int max = (horiz_p ? height : width);
  int *lines = (int *) calloc(max, sizeof(*lines));
  int i;

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

  for (i = 0; i < max; i++)
    { 
      if (horiz_p)
	XDrawLine (dpy, window, gc, 0, lines[i], width, lines[i]);
      else
	XDrawLine (dpy, window, gc, lines[i], 0, lines[i], height);

      XSync (dpy, False);
      if (delay > 0 && ((i % granularity) == 0))
	usleep (delay * granularity);
    }
  free(lines);
}


static void
venetian (Display *dpy, Window window, GC gc,
	  int width, int height, int delay, int granularity)
{
  Bool horiz_p = (random() & 1);
  Bool flip_p = (random() & 1);
  int max = (horiz_p ? height : width);
  int *lines = (int *) calloc(max, sizeof(*lines));
  int i, j;

  granularity /= 6;

  j = 0;
  for (i = 0; i < max*2; i++)
    {
      int line = ((i / 16) * 16) - ((i % 16) * 15);
      if (line >= 0 && line < max)
	lines[j++] = (flip_p ? max - line : line);
    }

  for (i = 0; i < max; i++)
    { 
      if (horiz_p)
	XDrawLine (dpy, window, gc, 0, lines[i], width, lines[i]);
      else
	XDrawLine (dpy, window, gc, lines[i], 0, lines[i], height);

      XSync (dpy, False);
      if (delay > 0 && ((i % granularity) == 0))
	usleep (delay * granularity);
    }
  free(lines);
}


static void
triple_wipe (Display *dpy, Window window, GC gc,
	     int width, int height, int delay, int granularity)
{
  Bool flip_x = random() & 1;
  Bool flip_y = random() & 1;
  int max = width + (height / 2);
  int *lines = (int *)calloc(max, sizeof(int));
  int i;

  for(i = 0; i < width/2; i++)
    lines[i] = i*2+height;
  for(i = 0; i < height/2; i++)
    lines[i+width/2] = i*2;
  for(i = 0; i < width/2; i++)
    lines[i+width/2+height/2] = width-i*2-(width%2?0:1)+height;

  granularity /= 6;

  for (i = 0; i < max; i++)
    { 
      int x, y, x2, y2;
      if (lines[i] < height)
	x = 0, y = lines[i], x2 = width, y2 = y;
      else
	x = lines[i]-height, y = 0, x2 = x, y2 = height;

      if (flip_x)
	x = width-x, x2 = width-x2;
      if (flip_y)
	y = height-y, y2 = height-y2;

      XDrawLine (dpy, window, gc, x, y, x2, y2);
      XSync (dpy, False);
      if (delay > 0 && ((i % granularity) == 0))
	usleep (delay*granularity);
    }
  free(lines);
}


static void
quad_wipe (Display *dpy, Window window, GC gc,
	   int width, int height, int delay, int granularity)
{
  Bool flip_x = random() & 1;
  Bool flip_y = random() & 1;
  int max = width + height;
  int *lines = (int *)calloc(max, sizeof(int));
  int i;

  granularity /= 3;

  for (i = 0; i < max/4; i++)
    {
      lines[i*4]   = i*2;
      lines[i*4+1] = height-i*2-(height%2?0:1);
      lines[i*4+2] = height+i*2;
      lines[i*4+3] = height+width-i*2-(width%2?0:1);
    }

  for (i = 0; i < max; i++)
    { 
      int x, y, x2, y2;
      if (lines[i] < height)
	x = 0, y = lines[i], x2 = width, y2 = y;
      else
	x = lines[i]-height, y = 0, x2 = x, y2 = height;

      if (flip_x)
	x = width-x, x2 = width-x2;
      if (flip_y)
	y = height-y, y2 = height-y2;

      XDrawLine (dpy, window, gc, x, y, x2, y2);
      XSync (dpy, False);
      if (delay > 0 && ((i % granularity) == 0))
	usleep (delay*granularity);
    }
  free(lines);
}



static void
circle_wipe (Display *dpy, Window window, GC gc,
	     int width, int height, int delay, int granularity)
{
  int full = 360 * 64;
  int inc = full / 64;
  int start = random() % full;
  int rad = (width > height ? width : height);
  int i;
  if (random() & 1)
    inc = -inc;
  for (i = (inc > 0 ? 0 : full);
       (inc > 0 ? i < full : i > 0);
       i += inc)
    {
      XFillArc(dpy, window, gc,
	       (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (i+start) % full, inc);
      XFlush (dpy);
      usleep (delay*granularity);
    }
}


static void
three_circle_wipe (Display *dpy, Window window, GC gc,
		   int width, int height, int delay, int granularity)
{
  int i;
  int full = 360 * 64;
  int q = full / 6;
  int q2 = q * 2;
  int inc = full / 240;
  int start = random() % q;
  int rad = (width > height ? width : height);

  for (i = 0; i < q; i += inc)
    {
      XFillArc(dpy, window, gc, (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (start+i) % full, inc);
      XFillArc(dpy, window, gc, (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (start-i) % full, -inc);

      XFillArc(dpy, window, gc, (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (start+q2+i) % full, inc);
      XFillArc(dpy, window, gc, (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (start+q2-i) % full, -inc);

      XFillArc(dpy, window, gc, (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (start+q2+q2+i) % full, inc);
      XFillArc(dpy, window, gc, (width/2)-rad, (height/2)-rad, rad*2, rad*2,
	       (start+q2+q2-i) % full, -inc);

      XSync (dpy, False);
      usleep (delay*granularity);
    }
}


static void
squaretate (Display *dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
  int steps = (((width > height ? width : width) * 2) / granularity);
  int i;
  Bool flip = random() & 1;

#define DRAW() \
      if (flip) { \
	points[0].x = width-points[0].x; \
	points[1].x = width-points[1].x; \
        points[2].x = width-points[2].x; } \
      XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin)

  for (i = 0; i < steps; i++)
    {
      XPoint points [3];
      points[0].x = 0;
      points[0].y = 0;
      points[1].x = width;
      points[1].y = 0;
      points[2].x = 0;
      points[2].y = points[0].y + ((i * height) / steps);
      DRAW();

      points[0].x = 0;
      points[0].y = 0;
      points[1].x = 0;
      points[1].y = height;
      points[2].x = ((i * width) / steps);
      points[2].y = height;
      DRAW();

      points[0].x = width;
      points[0].y = height;
      points[1].x = 0;
      points[1].y = height;
      points[2].x = width;
      points[2].y = height - ((i * height) / steps);
      DRAW();

      points[0].x = width;
      points[0].y = height;
      points[1].x = width;
      points[1].y = 0;
      points[2].x = width - ((i * width) / steps);
      points[2].y = 0;
      DRAW();

      XSync (dpy, True);
      if (delay > 0)
	usleep (delay * granularity);
   }
#undef DRAW
}


/* from Frederick Roeber <roeber@netscape.com> */
static void
fizzle (Display *dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
  /* These dimensions must be prime numbers.  They should be roughly the
     square root of the width and height. */
# define BX 31
# define BY 31
# define SIZE (BX*BY)

  int array[SIZE];
  int i, j;
  XPoint *skews;
  int nx, ny;

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

  for( i = 0; i < SIZE; i++ ) {
    int x = array[i] % BX;
    int y = array[i] / BX;

    {
      int iy, cy;
      for( iy = 0, cy = 0; iy < height; iy += BY, cy++ ) {
        int ix, cx;
        for( ix = 0, cx = 0; ix < width; ix += BX, cx++ ) {
          int xx = ix + (SKEWX(cx, cy) + x*((cx%(BX-1))+1))%BX;
          int yy = iy + (SKEWY(cx, cy) + y*((cy%(BY-1))+1))%BY;
          XDrawPoint(dpy, window, gc, xx, yy);
        }
      }
    }

    if( (BX-1) == (i%BX) ) {
      XSync (dpy, False);
      usleep (delay*granularity);
    }
  }

# undef SKEWX
# undef SKEWY

  if( (XPoint *)0 != skews ) {
    free(skews);
  }

# undef BX
# undef BY
# undef SIZE
}


#undef MAX
#undef MIN
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

/* from David Bagley <bagleyd@tux.org> */
static void
random_squares(Display * dpy, Window window, GC gc,
               int width, int height, int delay, int granularity)
{
  int randsize = MAX(1, MIN(width, height) / (16 + (random() % 32)));
  int max = (height / randsize + 1) * (width / randsize + 1);
  int *squares = (int *) calloc(max, sizeof (*squares));
  int i;
  int columns = width / randsize + 1;  /* Add an extra for roundoff */

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

  for (i = 0; i < max; i++)
    {
      XFillRectangle(dpy, window, gc,
		     (squares[i] % columns) * randsize,
		     (squares[i] / columns) * randsize,
		     randsize, randsize);

      XSync(dpy, False);
      if (delay > 0 && ((i % granularity) == 0))
      usleep(delay * granularity);
    }
  free(squares);
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
  random_squares,
};


void
erase_window(Display *dpy, Window window, GC gc,
	     int width, int height, int mode, int delay)
{
  int granularity = 25;

  if (mode < 0 || mode >= countof(erasers))
    mode = random() % countof(erasers);
  (*(erasers[mode])) (dpy, window, gc, width, height, delay, granularity);
  XClearWindow (dpy, window);
  XSync(dpy, False);
}


void
erase_full_window(Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGCValues gcv;
  GC erase_gc;
  XColor black;
  int erase_speed, erase_mode;
  char *s;

  s = get_string_resource("eraseSpeed", "Integer");
  if (s && *s)
    erase_speed = get_integer_resource("eraseSpeed", "Integer");
  else
    erase_speed = 400;
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
		erase_mode, erase_speed);
  XFreeColors(dpy, xgwa.colormap, &black.pixel, 1, 0);
  XFreeGC(dpy, erase_gc);
}



#if 0
#include "screenhack.h"

char *progclass = "Erase";
char *defaults [] = {
  0
};

XrmOptionDescRec options [] = {{0}};
int options_size = 0;

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  int delay = 500000;
  XGCValues gcv;
  GC gc;
  XColor white;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  white.flags = DoRed|DoGreen|DoBlue;
  white.red = white.green = white.blue = 0xFFFF;
  XAllocColor(dpy, xgwa.colormap, &white);
  gcv.foreground = white.pixel;
  gc = XCreateGC (dpy, window, GCForeground, &gcv);

  while (1)
    {
      XFillRectangle(dpy, window, gc, 0, 0, 1280, 1024);
      XSync (dpy, False);
      usleep (delay);
      erase_full_window(dpy, window);
      XSync (dpy, False);
      usleep (delay);

    }
}

#endif
