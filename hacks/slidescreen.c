/* xscreensaver, Copyright (c) 1992, 1993, 1994 Jamie Zawinski <jwz@mcom.com>
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

static int grid_size;
static int pix_inc;
static int hole_x, hole_y;
static int bitmap_w, bitmap_h;
static int xoff, yoff;
static int grid_w, grid_h;
static int delay, delay2;
static GC gc;

static void
init_slide (dpy, window)
     Display *dpy;
     Window window;
{
  int i;
  XGCValues gcv;
  XWindowAttributes xgwa;
  int border;
  int root_p;
  unsigned long fg;
  Pixmap pixmap;
  Drawable d;
  Colormap cmap;
  Visual *visual;

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  visual = xgwa.visual;

  copy_default_colormap_contents (dpy, cmap, visual);

  delay = get_integer_resource ("delay", "Integer");
  delay2 = get_integer_resource ("delay2", "Integer");
  grid_size = get_integer_resource ("gridSize", "Integer");
  pix_inc = get_integer_resource ("pixelIncrement", "Integer");
  border = get_integer_resource ("internalBorderWidth", "InternalBorderWidth");
  fg = get_pixel_resource ("background", "Background", dpy,
			   /* Pixels always come out of default map. */
			   XDefaultColormapOfScreen (xgwa.screen));
  root_p = get_boolean_resource ("root", "Boolean");

  if (delay < 0) delay = 0;
  if (delay2 < 0) delay2 = 0;
  if (pix_inc < 1) pix_inc = 1;
  if (grid_size < 1) grid_size = 1;

  gcv.foreground = fg;
  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gc = XCreateGC (dpy, window, GCForeground |GCFunction | GCSubwindowMode,
		  &gcv);

  pixmap = grab_screen_image (dpy, window, root_p);

  XGetWindowAttributes (dpy, window, &xgwa);
  bitmap_w = xgwa.width;
  bitmap_h = xgwa.height;

  grid_w = bitmap_w / grid_size;
  grid_h = bitmap_h / grid_size;
  hole_x = random () % grid_w;
  hole_y = random () % grid_h;
  xoff = (bitmap_w - (grid_w * grid_size)) / 2;
  yoff = (bitmap_h - (grid_h * grid_size)) / 2;

  d = (pixmap ? pixmap : window);

  if (border)
    {
      int i;
      for (i = 0; i <= bitmap_w; i += grid_size)
	XFillRectangle (dpy, d, gc, xoff+i-border/2, yoff, border, bitmap_h);
      for (i = 0; i <= bitmap_h; i += grid_size)
	XFillRectangle (dpy, d, gc, xoff, yoff+i-border/2, bitmap_w, border);
    }

  if (xoff)
    {
      XFillRectangle (dpy, d, gc, 0, 0, xoff, bitmap_h);
      XFillRectangle (dpy, d, gc, bitmap_w - xoff, 0, xoff, bitmap_h);
    }
  if (yoff)
    {
      XFillRectangle (dpy, d, gc, 0, 0, bitmap_w, yoff);
      XFillRectangle (dpy, d, gc, 0, bitmap_h - yoff, bitmap_w, yoff);
    }

  if (pixmap) XClearWindow (dpy, window);
  XSync (dpy, True);
  if (delay2) usleep (delay2 * 2);
 for (i = 0; i < grid_size; i += pix_inc)
   {
     XPoint points [3];
     points[0].x = xoff + grid_size * hole_x;
     points[0].y = yoff + grid_size * hole_y;
     points[1].x = points[0].x + grid_size;
     points[1].y = points[0].y;
     points[2].x = points[0].x;
     points[2].y = points[0].y + i;
     XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin);

     points[1].x = points[0].x;
     points[1].y = points[0].y + grid_size;
     points[2].x = points[0].x + i;
     points[2].y = points[0].y + grid_size;
     XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin);

     points[0].x = points[1].x + grid_size;
     points[0].y = points[1].y;
     points[2].x = points[0].x;
     points[2].y = points[0].y - i;
     XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin);

     points[1].x = points[0].x;
     points[1].y = points[0].y - grid_size;
     points[2].x = points[1].x - i;
     points[2].y = points[1].y;
     XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin);

     XSync (dpy, True);
     if (delay) usleep (delay);
   }

  XFillRectangle (dpy, window, gc,
		  xoff + grid_size * hole_x,
		  yoff + grid_size * hole_y,
		  grid_size, grid_size);
}

static void
slide1 (dpy, window)
     Display *dpy;
     Window window;
{
  /* this code is a total kludge, but who cares, it works... */
 int i, x, y, ix, iy, dx, dy, dir, w, h, size, inc;
 static int last = -1;
 do {
   dir = random () % 4;
   switch (dir)
     {
     case 0: dx = 0,  dy = 1;  break;
     case 1: dx = -1, dy = 0;  break;
     case 2: dx = 0,  dy = -1; break;
     case 3: dx = 1,  dy = 0;  break;
     default: abort ();
     }
 } while (dir == last ||
	  hole_x + dx < 0 || hole_x + dx >= grid_w ||
	  hole_y + dy < 0 || hole_y + dy >= grid_h);
 if (grid_w > 1 && grid_h > 1)
   last = (dir == 0 ? 2 : dir == 2 ? 0 : dir == 1 ? 3 : 1);

 switch (dir)
   {
   case 0: size = 1 + (random()%(grid_h - hole_y - 1)); h = size; w = 1; break;
   case 1: size = 1 + (random()%hole_x); 	        w = size; h = 1; break;
   case 2: size = 1 + (random()%hole_y);	        h = size; w = 1; break;
   case 3: size = 1 + (random()%(grid_w - hole_x - 1)); w = size; h = 1; break;
   default: abort ();
   }

 if (dx == -1) hole_x -= (size - 1);
 else if (dy == -1) hole_y -= (size - 1);

 ix = x = xoff + (hole_x + dx) * grid_size;
 iy = y = yoff + (hole_y + dy) * grid_size;
 inc = pix_inc;
 for (i = 0; i < grid_size; i += inc)
   {
     if (inc + i > grid_size)
       inc = grid_size - i;
     XCopyArea (dpy, window, window, gc, x, y, grid_size * w, grid_size * h,
		x - dx * inc, y - dy * inc);
     x -= dx * inc;
     y -= dy * inc;
     switch (dir)
       {
       case 0: XFillRectangle (dpy, window, gc,
			       ix, y + grid_size * h, grid_size * w, iy - y);
	 break;
       case 1: XFillRectangle (dpy, window, gc, ix, iy, x - ix, grid_size * h);
	 break;
       case 2: XFillRectangle (dpy, window, gc, ix, iy, grid_size * w, y - iy);
	 break;
       case 3: XFillRectangle (dpy, window, gc,
			       x + grid_size * w, iy, ix - x, grid_size * h);
	 break;
       }

     XSync (dpy, True);
     if (delay) usleep (delay);
   }
 switch (dir)
   {
   case 0: hole_y += size; break;
   case 1: hole_x--; break;
   case 2: hole_y--; break;
   case 3: hole_x += size; break;
   }
}


char *progclass = "SlidePuzzle";

char *defaults [] = {
  "SlidePuzzle.mappedWhenManaged:false",
  "SlidePuzzle.dontClearWindow:	 true",
  "*background:			black",
  "*gridSize:			70",
  "*pixelIncrement:		10",
  "*internalBorderWidth:	1",
  "*delay:			50000",
  "*delay2:			1000000",
  0
};

XrmOptionDescRec options [] = {
  { "-grid-size",	".gridSize",		XrmoptionSepArg, 0 },
  { "-ibw",		".internalBorderWidth",	XrmoptionSepArg, 0 },
  { "-increment",	".pixelIncrement",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",		XrmoptionSepArg, 0 },
};

int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_slide (dpy, window);
  while (1)
    {
      slide1 (dpy, window);
      if (delay2) usleep (delay2);
    }
}
