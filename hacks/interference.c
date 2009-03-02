/* interference.c --- colored fields via decaying sinusoidal waves.
 * An entry for the RHAD Labs Screensaver Contest.
 *
 * Author: Hannu Mallat <hmallat@cs.hut.fi>
 *
 * Copyright (C) 1998 Hannu Mallat.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * decaying sinusoidal waves, which extend spherically from their
 * respective origins, move around the plane. a sort of interference
 * between them is calculated and the resulting twodimensional wave
 * height map is plotted in a grid, using softly changing colours.
 *
 * not physically (or in any sense) accurate, but fun to look at for 
 * a while. you may tune the speed/resolution/interestingness tradeoff 
 * with X resources, see below.
 *
 * Created      : Wed Apr 22 09:30:30 1998, hmallat
 * Last modified: Wed Apr 22 09:30:30 1998, hmallat
 *
 * TODO:
 *
 *    This really needs to be sped up.
 *
 *    I've tried making it use XPutPixel/XPutImage instead of XFillRectangle,
 *    but that doesn't seem to help (it's the same speed at gridsize=1, and
 *    it actually makes it slower at larger sizes.)
 *
 *    I played around with shared memory, but clearly I still don't know how
 *    to use the XSHM extension properly, because it doesn't work yet.
 *
 *    Hannu had put in code to use the double-buffering extension, but that
 *    code didn't work for me on Irix.  I don't think it would help anyway,
 *    since it's not the swapping of frames that is the bottleneck (or a source
 *    of visible flicker.)
 *
 *     -- jwz, 4-Jun-98
 */

#include <math.h>

#include "screenhack.h"

# include <X11/Xutil.h>

/* I thought it would be faster this way, but it turns out not to be... -jwz */
#undef USE_XIMAGE

#ifndef USE_XIMAGE
# undef HAVE_XSHM_EXTENSION  /* only applicable when using XImages */
#endif /* USE_XIMAGE */


#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */

char *progclass="Interference";

char *defaults [] = {
  ".background:  black",
  ".foreground:  white",
  "*count:       3",     /* number of waves */
  "*gridsize:    4",     /* pixel size, smaller values for better resolution */
  "*ncolors:     128",   /* number of colours used */
  "*speed:       30",    /* speed of wave origins moving around */
  "*delay:       30000", /* or something */
  "*color-shift: 60",    /* h in hsv space, smaller values for smaller
			  * color gradients */
  "*radius:      800",   /* wave extent */
  "*gray:        false", /* color or grayscale */
  "*mono:        false", /* monochrome, not very much fun */

  "*doubleBuffer: True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:      True", /* use double buffering extension */
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_XSHM_EXTENSION
  "*useSHM:      True", /* use shared memory extension */
#endif /*  HAVE_XSHM_EXTENSION */
  0
};

XrmOptionDescRec options [] = {
  { "-count",       ".count",       XrmoptionSepArg, 0 }, 
  { "-ncolors",     ".ncolors",     XrmoptionSepArg, 0 }, 
  { "-gridsize",    ".gridsize",    XrmoptionSepArg, 0 }, 
  { "-speed",       ".speed",       XrmoptionSepArg, 0 },
  { "-delay",       ".delay",       XrmoptionSepArg, 0 },
  { "-color-shift", ".color-shift", XrmoptionSepArg, 0 },
  { "-radius",      ".radius",      XrmoptionSepArg, 0 },
  { "-gray",        ".gray",        XrmoptionNoArg,  "True" },
  { "-mono",        ".mono",        XrmoptionNoArg,  "True" },
  { "-db",	    ".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",	    ".doubleBuffer", XrmoptionNoArg,  "False" },
#ifdef HAVE_XSHM_EXTENSION
  { "-shm",	".useSHM",	XrmoptionNoArg, "True" },
  { "-no-shm",	".useSHM",	XrmoptionNoArg, "False" },
#endif /*  HAVE_XSHM_EXTENSION */
  { 0, 0, 0, 0 }
};

int options_size = (sizeof (options) / sizeof (XrmOptionDescRec));

struct inter_source {
  int x; 
  int y;
  double x_theta;
  double y_theta;
};

struct inter_context {
  /*
   * Display-related entries 
   */
  Display* dpy;
  Window   win;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer back_buf;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  Pixmap   pix_buf;

  GC       copy_gc;
#ifdef USE_XIMAGE
  XImage  *ximage;
#endif /* USE_XIMAGE */

#ifdef HAVE_XSHM_EXTENSION
  Bool use_shm;
  XShmSegmentInfo shm_info;
#endif /* HAVE_XSHM_EXTENSION */

  /*
   * Resources
   */
  int count;
  int grid_size;
  int colors;
  int speed;
  int delay;
  int shift;
  int radius;

  /*
   * Drawing-related entries
   */
  int w;
  int h;
  Colormap cmap;
  XColor* pal;
  GC* gcs;

  /*
   * lookup tables
   */
  int* wave_height;
    
  /*
   * Interference sources
   */
  struct inter_source* source;
};

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# define TARGET(c) ((c)->back_buf ? (c)->back_buf : \
                    (c)->pix_buf ? (c)->pix_buf : (c)->win)
#else  /* HAVE_DOUBLE_BUFFER_EXTENSION */
# define TARGET(c) ((c)->pix_buf ? (c)->pix_buf : (c)->win)
#endif /* !HAVE_DOUBLE_BUFFER_EXTENSION */

void inter_init(Display* dpy, Window win, struct inter_context* c) 
{
  XWindowAttributes xgwa;
  double H[3], S[3], V[3];
  int i;
  int mono;
  int gray;
  XGCValues val;
  unsigned long valmask = 0;
  Bool dbuf = get_boolean_resource ("doubleBuffer", "Boolean");

  memset (c, 0, sizeof(*c));

  c->dpy = dpy;
  c->win = win;

  XGetWindowAttributes(c->dpy, c->win, &xgwa);
  c->w = xgwa.width;
  c->h = xgwa.height;
  c->cmap = xgwa.colormap;

#ifdef HAVE_XSHM_EXTENSION
  c->use_shm = get_boolean_resource("useSHM", "Boolean");
#endif /*  HAVE_XSHM_EXTENSION */

  if (dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      c->back_buf = xdbe_get_backbuffer (c->dpy, c->win, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      if (!c->back_buf)
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
        c->pix_buf = XCreatePixmap (dpy, win, xgwa.width, xgwa.height,
                                    xgwa.depth);
    }

  val.function = GXcopy;
  c->copy_gc = XCreateGC(c->dpy, TARGET(c), GCFunction, &val);

  c->count = get_integer_resource("count", "Integer");
  if(c->count < 1)
    c->count = 1;
  c->grid_size = get_integer_resource("gridsize", "Integer");
  if(c->grid_size < 1)
    c->grid_size = 1;
  mono = get_boolean_resource("mono", "Boolean");
  if(!mono) {
    c->colors = get_integer_resource("ncolors", "Integer");
    if(c->colors < 2)
      c->colors = 2;
  }
  c->speed = get_integer_resource("speed", "Integer");
  c->shift = get_float_resource("color-shift", "Float");
  while(c->shift >= 360.0)
    c->shift -= 360.0;
  while(c->shift <= -360.0)
    c->shift += 360.0;
  c->radius = get_integer_resource("radius", "Integer");;
  if(c->radius < 1)
    c->radius = 1;

#ifdef USE_XIMAGE

  c->ximage = 0;

# ifdef HAVE_XSHM_EXTENSION
  if (c->use_shm)
    {
      c->ximage = create_xshm_image(dpy, xgwa.visual, xgwa.depth,
				    ZPixmap, 0, &c->shm_info,
				    xgwa.width, c->grid_size);
      if (!c->ximage)
	c->use_shm = False;
    }
# endif /* HAVE_XSHM_EXTENSION */

  if (!c->ximage)
    {
      c->ximage =
	XCreateImage (dpy, xgwa.visual,
		      xgwa.depth, ZPixmap, 0, 0, /* depth, fmt, offset, data */
		      xgwa.width, c->grid_size,	 /* width, height */
		      8, 0);			 /* pad, bpl */
      c->ximage->data = (unsigned char *)
	calloc(c->ximage->height, c->ximage->bytes_per_line);
    }
#endif /* USE_XIMAGE */

  if(!mono) {
    c->pal = calloc(c->colors, sizeof(XColor));

    gray = get_boolean_resource("gray", "Boolean");
    if(!gray) {
      H[0] = frand(360.0); 
      H[1] = H[0] + c->shift < 360.0 ? H[0]+c->shift : H[0] + c->shift-360.0; 
      H[2] = H[1] + c->shift < 360.0 ? H[1]+c->shift : H[1] + c->shift-360.0; 
      S[0] = S[1] = S[2] = 1.0;
      V[0] = V[1] = V[2] = 1.0;
    } else {
      H[0] = H[1] = H[2] = 0.0;
      S[0] = S[1] = S[2] = 0.0;
      V[0] = 1.0; V[1] = 0.5; V[2] = 0.0;
    }

    make_color_loop(c->dpy, c->cmap, 
		    H[0], S[0], V[0], 
		    H[1], S[1], V[1], 
		    H[2], S[2], V[2], 
		    c->pal, &(c->colors), 
		    True, False);
    if(c->colors < 2) { /* color allocation failure */
      mono = 1;
      free(c->pal);
    }
  }

  if(mono) { /* DON'T else this with the previous if! */
    c->colors = 2;
    c->pal = calloc(2, sizeof(XColor));
    c->pal[0].pixel = BlackPixel(c->dpy, DefaultScreen(c->dpy));
    c->pal[1].pixel = WhitePixel(c->dpy, DefaultScreen(c->dpy));
  }    

  valmask = GCForeground;
  c->gcs = calloc(c->colors, sizeof(GC));
  for(i = 0; i < c->colors; i++) {
    val.foreground = c->pal[i].pixel;    
    c->gcs[i] = XCreateGC(c->dpy, TARGET(c), valmask, &val);
  }

  c->wave_height = calloc(c->radius, sizeof(int));
  for(i = 0; i < c->radius; i++) {
    float max = 
      ((float)c->colors) * 
      ((float)c->radius - (float)i) /
      ((float)c->radius);
    c->wave_height[i] = 
      (int)
      ((max + max*cos((double)i/50.0)) / 2.0);
  }

  c->source = calloc(c->count, sizeof(struct inter_source));
  for(i = 0; i < c->count; i++) {
    c->source[i].x_theta = frand(2.0)*3.14159;
    c->source[i].y_theta = frand(2.0)*3.14159;
  }

}

#define source_x(c, i) \
  (c->w/2 + ((int)(cos(c->source[i].x_theta)*((float)c->w/2.0))))
#define source_y(c, i) \
  (c->h/2 + ((int)(cos(c->source[i].y_theta)*((float)c->h/2.0))))

/*
 * this is rather suboptimal. the sqrt() doesn't seem to be a big
 * performance hit, but all those separate XFillRectangle()'s are.
 * hell, it's just a quick hack anyway -- if someone wishes to tune
 * it, go ahead! 
 */

void do_inter(struct inter_context* c) 
{
  int i, j, k;
  int result;
  int dist;
  int g;

  int dx, dy;

  for(i = 0; i < c->count; i++) {
    c->source[i].x_theta += (c->speed/1000.0);
    if(c->source[i].x_theta > 2.0*3.14159)
      c->source[i].x_theta -= 2.0*3.14159;
    c->source[i].y_theta += (c->speed/1000.0);
    if(c->source[i].y_theta > 2.0*3.14159)
      c->source[i].y_theta -= 2.0*3.14159;
    c->source[i].x = source_x(c, i);
    c->source[i].y = source_y(c, i);
  }

  g = c->grid_size;

  for(j = 0; j < c->h/g; j++) {
    for(i = 0; i < c->w/g; i++) {
      result = 0;
      for(k = 0; k < c->count; k++) {
	dx = i*g + g/2 - c->source[k].x;
	dy = j*g + g/2 - c->source[k].y;
	dist = sqrt(dx*dx + dy*dy); /* what's the performance penalty here? */
	result += (dist > c->radius ? 0 : c->wave_height[dist]);
      }
      result %= c->colors;

#ifdef USE_XIMAGE
      /* Fill in these `gridsize' horizontal bits in the scanline */
      for(k = 0; k < g; k++)
	XPutPixel(c->ximage, (g*i)+k, 0, c->pal[result].pixel);

#else  /* !USE_XIMAGE */
      XFillRectangle(c->dpy, TARGET(c), c->gcs[result], g*i, g*j, g, g); 
#endif /* !USE_XIMAGE */
    }

#ifdef USE_XIMAGE

    /* Only the first scanline of the image has been filled in; clone that
       scanline to the rest of the `gridsize' lines in the ximage */
    for(k = 0; k < (g-1); k++)
      memcpy(c->ximage->data + (c->ximage->bytes_per_line * (k + 1)),
	     c->ximage->data + (c->ximage->bytes_per_line * k),
	     c->ximage->bytes_per_line);

    /* Move the bits for this horizontal stripe to the server. */
# ifdef HAVE_XSHM_EXTENSION
    if (c->use_shm)
      XShmPutImage(c->dpy, TARGET(c), c->copy_gc, c->ximage,
		   0, 0, 0, g*j, c->ximage->width, c->ximage->height,
		   False);
    else
# endif /*  HAVE_XSHM_EXTENSION */
      XPutImage(c->dpy, TARGET(c), c->copy_gc, c->ximage,
		0, 0, 0, g*j, c->ximage->width, c->ximage->height);

#endif /* USE_XIMAGE */
  }

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (c->back_buf)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = c->win;
      info[0].swap_action = XdbeUndefined;
      XdbeSwapBuffers(c->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    if (c->pix_buf)
      {
        XCopyArea (c->dpy, c->pix_buf, c->win, c->copy_gc,
                   0, 0, c->w, c->h, 0, 0);
      }

  XSync(c->dpy, False);
}

void screenhack(Display *dpy, Window win) 
{
  struct inter_context c;
  int delay;

  delay = get_integer_resource("delay", "Integer");

  inter_init(dpy, win, &c);
  while(1) {
    do_inter(&c); 
    screenhack_handle_events (dpy);
    if(delay) usleep(delay);
  }
}
