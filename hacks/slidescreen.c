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

#include "screenhack.h"

enum { DOWN = 0, LEFT, UP, RIGHT };
enum { VERTICAL, HORIZONTAL };

struct state {
  Display *dpy;
  Window window;

  int grid_size;
  int pix_inc;
  int hole_x, hole_y;
  int bitmap_w, bitmap_h;
  int xoff, yoff;
  int grid_w, grid_h;
  int delay, delay2;
  int duration;
  GC gc;
  unsigned long fg, bg;
  int max_width, max_height;
  int early_i;

  int draw_rnd, draw_i;
  int draw_x, draw_y, draw_ix, draw_iy, draw_dx, draw_dy;
  int draw_dir, draw_w, draw_h, draw_size, draw_inc;
  int draw_last;
  int draw_initted;

  time_t start_time;
  async_load_state *img_loader;
};


static void *
slidescreen_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XWindowAttributes xgwa;
  XGCValues gcv;
  long gcflags;

  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                            st->window, 0, 0);
  st->start_time = time ((time_t) 0);

  st->max_width = xgwa.width;
  st->max_height = xgwa.height;

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer");
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  st->grid_size = get_integer_resource (st->dpy, "gridSize", "Integer");
  st->pix_inc = get_integer_resource (st->dpy, "pixelIncrement", "Integer");

  /* Don't let the grid be smaller than 5x5 */
  while (st->grid_size > xgwa.width / 5)
    st->grid_size /= 2;
  while (st->grid_size > xgwa.height / 5)
    st->grid_size /= 2;

  if (st->delay < 0) st->delay = 0;
  if (st->delay2 < 0) st->delay2 = 0;
  if (st->duration < 1) st->duration = 1;
  if (st->pix_inc < 1) st->pix_inc = 1;
  if (st->grid_size < 1) st->grid_size = 1;


  {
    XColor fgc, bgc;
    char *fgs = get_string_resource(st->dpy, "background", "Background");
    char *bgs = get_string_resource(st->dpy, "foreground", "Foreground");
    Bool fg_ok, bg_ok;
    if (!XParseColor (st->dpy, xgwa.colormap, fgs, &fgc))
      XParseColor (st->dpy, xgwa.colormap, "black", &bgc);
    if (!XParseColor (st->dpy, xgwa.colormap, bgs, &bgc))
      XParseColor (st->dpy, xgwa.colormap, "gray", &fgc);

    fg_ok = XAllocColor (st->dpy, xgwa.colormap, &fgc);
    bg_ok = XAllocColor (st->dpy, xgwa.colormap, &bgc);

    /* If we weren't able to allocate the two colors we want from the
       colormap (which is likely if the screen has been grabbed on an
       8-bit SGI visual -- don't ask) then just go through the map
       and find the closest color to the ones we wanted, and use those
       pixels without actually allocating them.
     */
    if (fg_ok)
      st->fg = fgc.pixel;
    else
      st->fg = 0;

    if (bg_ok)
      st->bg = bgc.pixel;
    else
      st->bg = 1;

#ifndef HAVE_COCOA
    if (!fg_ok || bg_ok)
      {
        int i;
	unsigned long fgd = ~0;
	unsigned long bgd = ~0;
	int max = visual_cells (xgwa.screen, xgwa.visual);
	XColor *all = (XColor *) calloc(sizeof (*all), max);
	for (i = 0; i < max; i++)
	  {
	    all[i].flags = DoRed|DoGreen|DoBlue;
	    all[i].pixel = i;
	  }
	XQueryColors (st->dpy, xgwa.colormap, all, max);
	for(i = 0; i < max; i++)
	  {
	    long rd, gd, bd;
	    unsigned long dd;
	    if (!fg_ok)
	      {
		rd = (all[i].red   >> 8) - (fgc.red   >> 8);
		gd = (all[i].green >> 8) - (fgc.green >> 8);
		bd = (all[i].blue  >> 8) - (fgc.blue  >> 8);
		if (rd < 0) rd = -rd;
		if (gd < 0) gd = -gd;
		if (bd < 0) bd = -bd;
		dd = (rd << 1) + (gd << 2) + bd;
		if (dd < fgd)
		  {
		    fgd = dd;
		    st->fg = all[i].pixel;
		    if (dd == 0)
		      fg_ok = True;
		  }
	      }

	    if (!bg_ok)
	      {
		rd = (all[i].red   >> 8) - (bgc.red   >> 8);
		gd = (all[i].green >> 8) - (bgc.green >> 8);
		bd = (all[i].blue  >> 8) - (bgc.blue  >> 8);
		if (rd < 0) rd = -rd;
		if (gd < 0) gd = -gd;
		if (bd < 0) bd = -bd;
		dd = (rd << 1) + (gd << 2) + bd;
		if (dd < bgd)
		  {
		    bgd = dd;
		    st->bg = all[i].pixel;
		    if (dd == 0)
		      bg_ok = True;
		  }
	      }

	    if (fg_ok && bg_ok)
	      break;
	  }
	XFree(all);
      }
#endif /* !HAVE_COCOA */
  }

  gcv.foreground = st->fg;
  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gcflags = GCForeground |GCFunction;
  if (use_subwindow_mode_p(xgwa.screen, st->window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
  st->gc = XCreateGC (st->dpy, st->window, gcflags, &gcv);

  return st;
}

static void
draw_grid (struct state *st)
{
  int i;
  Drawable d;
  int border;
  XWindowAttributes xgwa;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  border = get_integer_resource (st->dpy, "internalBorderWidth",
                                 "InternalBorderWidth");

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->bitmap_w = xgwa.width;
  st->bitmap_h = xgwa.height;

  st->grid_w = st->bitmap_w / st->grid_size;
  st->grid_h = st->bitmap_h / st->grid_size;
  st->hole_x = random () % st->grid_w;
  st->hole_y = random () % st->grid_h;
  st->xoff = (st->bitmap_w - (st->grid_w * st->grid_size)) / 2;
  st->yoff = (st->bitmap_h - (st->grid_h * st->grid_size)) / 2;

  d = st->window;

  st->early_i = -10;
  st->draw_last = -1;

  if (border)
    {
      int half = border/2;
      int half2 = (border & 1 ? half+1 : half);
      XSetForeground(st->dpy, st->gc, st->bg);
      for (i = 0; i < st->bitmap_w; i += st->grid_size)
	{
	  int j;
	  for (j = 0; j < st->bitmap_h; j += st->grid_size)
	    XDrawRectangle (st->dpy, d, st->gc,
			    st->xoff+i+half2, st->yoff+j+half2,
			    st->grid_size-border-1, st->grid_size-border-1);
	}

      XSetForeground(st->dpy, st->gc, st->fg);
      for (i = 0; i <= st->bitmap_w; i += st->grid_size)
	XFillRectangle (st->dpy, d, st->gc, st->xoff+i-half, st->yoff, border, st->bitmap_h);
      for (i = 0; i <= st->bitmap_h; i += st->grid_size)
	XFillRectangle (st->dpy, d, st->gc, st->xoff, st->yoff+i-half, st->bitmap_w, border);
    }

  if (st->xoff)
    {
      XFillRectangle (st->dpy, d, st->gc, 0, 0, st->xoff, st->bitmap_h);
      XFillRectangle (st->dpy, d, st->gc, st->bitmap_w - st->xoff, 0, st->xoff, st->bitmap_h);
    }
  if (st->yoff)
    {
      XFillRectangle (st->dpy, d, st->gc, 0, 0, st->bitmap_w, st->yoff);
      XFillRectangle (st->dpy, d, st->gc, 0, st->bitmap_h - st->yoff, st->bitmap_w, st->yoff);
    }
}


static int
slidescreen_draw_early (struct state *st)
{
  while (st->early_i < 0)
    {
      st->early_i++;
      return 1;
    }

  /* for (early_i = 0; early_i < grid_size; early_i += pix_inc) */
   {
     XPoint points [3];
     points[0].x = st->xoff + st->grid_size * st->hole_x;
     points[0].y = st->yoff + st->grid_size * st->hole_y;
     points[1].x = points[0].x + st->grid_size;
     points[1].y = points[0].y;
     points[2].x = points[0].x;
     points[2].y = points[0].y + st->early_i;
     XFillPolygon (st->dpy, st->window, st->gc, points, 3, Convex, CoordModeOrigin);

     points[1].x = points[0].x;
     points[1].y = points[0].y + st->grid_size;
     points[2].x = points[0].x + st->early_i;
     points[2].y = points[0].y + st->grid_size;
     XFillPolygon (st->dpy, st->window, st->gc, points, 3, Convex, CoordModeOrigin);

     points[0].x = points[1].x + st->grid_size;
     points[0].y = points[1].y;
     points[2].x = points[0].x;
     points[2].y = points[0].y - st->early_i;
     XFillPolygon (st->dpy, st->window, st->gc, points, 3, Convex, CoordModeOrigin);

     points[1].x = points[0].x;
     points[1].y = points[0].y - st->grid_size;
     points[2].x = points[1].x - st->early_i;
     points[2].y = points[1].y;
     XFillPolygon (st->dpy, st->window, st->gc, points, 3, Convex, CoordModeOrigin);
   }

   st->early_i += st->pix_inc;
   if (st->early_i < st->grid_size)
     return 1;

   XFillRectangle (st->dpy, st->window, st->gc,
                   st->xoff + st->grid_size * st->hole_x,
                   st->yoff + st->grid_size * st->hole_y,
                   st->grid_size, st->grid_size);
   return 0;
}


static unsigned long
slidescreen_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay;

  /* this code is a total kludge, but who cares, it works... */

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
        st->start_time = time ((time_t) 0);
        draw_grid (st);
      }
      return st->delay;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < time ((time_t) 0)) {
    XWindowAttributes xgwa;
    XGetWindowAttributes(st->dpy, st->window, &xgwa);
    st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                              st->window, 0, 0);
    st->start_time = time ((time_t) 0);
    st->draw_initted = 0;
    return st->delay;
  }

  if (! st->draw_initted)
    {
      if (!slidescreen_draw_early (st))
        {
          st->draw_initted = 1;
          return st->delay2;
        }
      else
        return st->delay;
    }

 if (st->draw_i == 0)
   {
     if (st->draw_last == -1) st->draw_last = random () % 2;

     /* alternate between horizontal and vertical slides */
     /* note that draw_dir specifies the direction the _hole_ moves, not the tiles */
     if (st->draw_last == VERTICAL) {
       if (((st->grid_w > 1) ? st->draw_rnd = random () % (st->grid_w - 1) : 0)
	   < st->hole_x) {
         st->draw_dx = -1; st->draw_dir = LEFT;  st->hole_x -= st->draw_rnd;
       } else {
         st->draw_dx =  1; st->draw_dir = RIGHT; st->draw_rnd -= st->hole_x;
       }
       st->draw_dy = 0; st->draw_w = st->draw_size = st->draw_rnd + 1; st->draw_h = 1;
       st->draw_last = HORIZONTAL;
     } else {
       if (((st->grid_h > 1) ? st->draw_rnd = random () % (st->grid_h - 1) : 0)
	   < st->hole_y) {
         st->draw_dy = -1; st->draw_dir = UP;    st->hole_y -= st->draw_rnd;
       } else {
         st->draw_dy =  1; st->draw_dir = DOWN;  st->draw_rnd -= st->hole_y;
       }
       st->draw_dx = 0; st->draw_h = st->draw_size = st->draw_rnd + 1; st->draw_w = 1;
       st->draw_last = VERTICAL;
     }
 
     st->draw_ix = st->draw_x = st->xoff + (st->hole_x + st->draw_dx) * st->grid_size;
     st->draw_iy = st->draw_y = st->yoff + (st->hole_y + st->draw_dy) * st->grid_size;
     st->draw_inc = st->pix_inc;

   }

 /* for (draw_i = 0; draw_i < grid_size; draw_i += draw_inc) */
   {
     int fx, fy, tox, toy;
     if (st->draw_inc + st->draw_i > st->grid_size)
       st->draw_inc = st->grid_size - st->draw_i;
     tox = st->draw_x - st->draw_dx * st->draw_inc;
     toy = st->draw_y - st->draw_dy * st->draw_inc;

     fx = (st->draw_x < 0 ? 0 : st->draw_x > st->max_width  ? st->max_width  : st->draw_x);
     fy = (st->draw_y < 0 ? 0 : st->draw_y > st->max_height ? st->max_height : st->draw_y);
     tox = (tox < 0 ? 0 : tox > st->max_width  ? st->max_width  : tox);
     toy = (toy < 0 ? 0 : toy > st->max_height ? st->max_height : toy);

     XCopyArea (st->dpy, st->window, st->window, st->gc,
		fx, fy,
		st->grid_size * st->draw_w, st->grid_size * st->draw_h,
		tox, toy);

     st->draw_x -= st->draw_dx * st->draw_inc;
     st->draw_y -= st->draw_dy * st->draw_inc;
     switch (st->draw_dir)
       {
       case DOWN: XFillRectangle (st->dpy, st->window, st->gc,
			       st->draw_ix, st->draw_y + st->grid_size * st->draw_h, st->grid_size * st->draw_w, st->draw_iy - st->draw_y);
	 break;
       case LEFT: XFillRectangle (st->dpy, st->window, st->gc, st->draw_ix, st->draw_iy, st->draw_x - st->draw_ix, st->grid_size * st->draw_h);
	 break;
       case UP: XFillRectangle (st->dpy, st->window, st->gc, st->draw_ix, st->draw_iy, st->grid_size * st->draw_w, st->draw_y - st->draw_iy);
	 break;
       case RIGHT: XFillRectangle (st->dpy, st->window, st->gc,
			       st->draw_x + st->grid_size * st->draw_w, st->draw_iy, st->draw_ix - st->draw_x, st->grid_size * st->draw_h);
	 break;
       }
   }

   st->draw_i += st->draw_inc;
   if (st->draw_i >= st->grid_size)
     {
       st->draw_i = 0;

       switch (st->draw_dir)
         {
         case DOWN: st->hole_y += st->draw_size; break;
         case LEFT: st->hole_x--; break;
         case UP: st->hole_y--; break;
         case RIGHT: st->hole_x += st->draw_size; break;
         }

       this_delay = st->delay2;
     }

   return this_delay;
}

static void
slidescreen_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
slidescreen_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
slidescreen_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->gc);
  free (st);
}



static const char *slidescreen_defaults [] = {
  "*dontClearRoot:		True",
  "*fpsSolid:			true",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  ".background:			Black",
  ".foreground:			#BEBEBE",
  "*gridSize:			70",
  "*pixelIncrement:		10",
  "*internalBorderWidth:	4",
  "*delay:			50000",
  "*delay2:			1000000",
  "*duration:			120",
  0
};

static XrmOptionDescRec slidescreen_options [] = {
  { "-grid-size",	".gridSize",		XrmoptionSepArg, 0 },
  { "-ibw",		".internalBorderWidth",	XrmoptionSepArg, 0 },
  { "-increment",	".pixelIncrement",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",		XrmoptionSepArg, 0 },
  {"-duration",		".duration",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("SlideScreen", slidescreen)
