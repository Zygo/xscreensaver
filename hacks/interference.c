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
 * Last modified: Sun Aug 31 23:40:14 2003, 
 *              david slimp <rock808@DavidSlimp.com>
 * 		added -hue option to specify base color hue
 * Last modified: Wed May 15 00:04:43 2013,
 *              Dave Odell <dmo2118@gmail.com>
 *              Tuned performance; double-buffering is now off by default.
 *              Made animation speed independant of FPS.
 *              Added cleanup code, fixed a few glitches.
 *              Added gratuitous #ifdefs.
 */

#include <math.h>
#include <errno.h>

#include "screenhack.h"

#ifdef HAVE_STDINT_H
# include <stdint.h>
#else

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

#endif

/*
Tested on an Intel(R) Pentium(R) 4 CPU 3.00GHz (family 15, model 6, 2 cores),
1 GB PC2-4200, nouveau - Gallium 0.4 on NV44, X.Org version: 1.13.3. A very
modest system by current standards.

Does double-buffering make sense? (gridsize = 2)
USE_XIMAGE is off: Yes (-db: 4.1 FPS, -no-db: 2.9 FPS)
XPutImage in strips: No (-db: 35.9 FPS, -no-db: 38.7 FPS)
XPutImage, whole image: No (-db: 32.3 FPS, -no-db: 33.7 FPS)
MIT-SHM, whole image: Doesn't work anyway: (37.3 FPS)

If gridsize = 1, XPutImage is slow when the XImage is one line at a time.
XPutImage in strips: -db: 21.2 FPS, -no-db: 19.7 FPS
XPutimage, whole image: -db: 23.2 FPS, -no-db: 23.4 FPS
MIT-SHM: 26.0 FPS

So XPutImage in strips is very slightly faster when gridsize >= 2, but
quite a bit worse when gridsize = 1.
*/

/* I thought it would be faster this way, but it turns out not to be... -jwz */
/* It's a lot faster for me, though - D.O. */
#define USE_XIMAGE

/* i.e. make the XImage the size of the screen. This is much faster when
 * gridsize = 1. (And SHM is turned off.) */
#define USE_BIG_XIMAGE

/* Numbers are wave_table size, measured in unsigned integers.
 * FPS/radius = 50/radius = 800/radius = 1500/Big-O memory usage
 *
 * Use at most one of the following:
 * Both off = regular sqrt() - 13.5 FPS, 50/800/1500. */

/* #define USE_FAST_SQRT_HACKISH */ /* 17.8 FPS/2873/4921/5395/O(lg(radius)) */
#define USE_FAST_SQRT_BIGTABLE2 /* 26.1 FPS/156/2242/5386/O(radius^2) */

#ifndef USE_XIMAGE
# undef HAVE_XSHM_EXTENSION  /* only applicable when using XImages */
#endif /* USE_XIMAGE */

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */

static const char *interference_defaults [] = {
  ".background:  black",
  ".foreground:  white",
  "*count:       3",     /* number of waves */
  "*gridsize:    4",     /* pixel size, smaller values for better resolution */
  "*ncolors:     128",   /* number of colours used */
  "*hue:         0",     /* hue to use for base color (0-360) */
  "*speed:       30",    /* speed of wave origins moving around */
  "*delay:       30000", /* or something */
  "*color-shift: 60",    /* h in hsv space, smaller values for smaller
			  * color gradients */
  "*radius:      800",   /* wave extent */
  "*gray:        false", /* color or grayscale */
  "*mono:        false", /* monochrome, not very much fun */

  "*doubleBuffer: False", /* doubleBuffer slows things down for me - D.O. */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:      True", /* use double buffering extension */
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_XSHM_EXTENSION
  "*useSHM:      True", /* use shared memory extension */
#endif /*  HAVE_XSHM_EXTENSION */
#ifdef USE_IPHONE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec interference_options [] = {
  { "-count",       ".count",       XrmoptionSepArg, 0 }, 
  { "-ncolors",     ".ncolors",     XrmoptionSepArg, 0 }, 
  { "-gridsize",    ".gridsize",    XrmoptionSepArg, 0 }, 
  { "-hue",         ".hue",         XrmoptionSepArg, 0 },
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
  Bool use_shm, shm_can_draw;
  XShmSegmentInfo shm_info;
#endif /* HAVE_XSHM_EXTENSION */

  /*
   * Resources
   */
  int count;
  int grid_size;
  int colors;
  float hue;
  int speed;
  int delay;
  int shift;

  /*
   * Drawing-related entries
   */
  int w;
  int h;
  Colormap cmap;
  Screen *screen;
  XColor* pal;
#ifndef USE_XIMAGE
  GC* gcs;
#endif
  int radius; /* Not always the same as the X resource. */
  double last_frame;
#ifdef USE_XIMAGE
  uint32_t* row;
#endif

  /*
   * lookup tables
   */
  unsigned* wave_height;
    
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

#ifdef USE_FAST_SQRT_HACKISH
/* Based loosely on code from Wikipedia:
 * https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Approximations_that_depend_on_IEEE_representation
 */

/* FAST_SQRT_EXTRA_BITS = 3: Smallest useful value
 * = 5/6: A little bit of banding, wave_height table is on par with regular
 *        sqrt() code.
 * = 7: No apparent difference with original @ radius = 800.
 * = 8: One more just to be comfortable.
 */

# define FAST_SQRT_EXTRA_BITS 8

union int_float
{
  uint32_t i;
  float f;
};

static unsigned fast_log2(unsigned x)
{
  union int_float u;
  if(!x)
    return x;
  u.f = x;
  return ((u.i - 0x3f800000) >> (23 - FAST_SQRT_EXTRA_BITS)) + 1;
}

static float fast_inv_log2(unsigned x)
{
  union int_float u;
  if(!x)
    return 0.0f;
  u.i = ((x - 1) << (23 - FAST_SQRT_EXTRA_BITS)) + 0x3f800000;
  return u.f;
}

#endif

#ifdef USE_FAST_SQRT_BIGTABLE2

/* I eyeballed these figures. They could be improved. - D.O. */

# define FAST_SQRT_DISCARD_BITS1 4
/* = 5: Dot in center is almost invisible at radius = 800. */
/* = 4: Dot in center looks OK at radius = 50. */

/* 156/2740/9029 */
/* # define FAST_SQRT_DISCARD_BITS2 8 */
/* # define FAST_SQRT_CUTOFF 64 * 64 */

/* 156/2242/5386 */
# define FAST_SQRT_DISCARD_BITS2 9
# define FAST_SQRT_CUTOFF 128 * 128

/*
 * This is a little faster:
 * 44.5 FPS, 19/5000/17578
 *
 * # define FAST_SQRT_DISCARD_BITS1 7
 * # define FAST_SQRT_DISCARD_BITS2 7
 * # define FAST_SQRT_CUTOFF 0
 *
 * For radius = 800, FAST_SQRT_DISCARD_BITS2 =
 * = 9/10: Approximately the original table size, some banding near origins.
 * = 7: wave_height is 20 KB, and just fits inside a 32K L1 cache.
 * = 6: Nearly indistinguishable from original
 */

/*
  FAST_TABLE(x) is equivalent to, but slightly faster than:
  x < FAST_SQRT_CUTOFF ?
    (x >> FAST_SQRT_DISCARD_BITS1) :
    ((x - FAST_SQRT_CUTOFF) >> FAST_SQRT_DISCARD_BITS2) +
      (FAST_SQRT_CUTOFF >> FAST_SQRT_DISCARD_BITS1);
*/

#define FAST_TABLE(x) \
  ((x) < FAST_SQRT_CUTOFF ? \
    ((x) >> FAST_SQRT_DISCARD_BITS1) : \
    (((x) + \
      ((FAST_SQRT_CUTOFF << (FAST_SQRT_DISCARD_BITS2 - \
        FAST_SQRT_DISCARD_BITS1)) - FAST_SQRT_CUTOFF)) >> \
      FAST_SQRT_DISCARD_BITS2))

static double fast_inv_table(unsigned x)
{
  return x < (FAST_SQRT_CUTOFF >> FAST_SQRT_DISCARD_BITS1) ?
    (x << FAST_SQRT_DISCARD_BITS1) :
    ((x - (FAST_SQRT_CUTOFF >> FAST_SQRT_DISCARD_BITS1)) <<
      FAST_SQRT_DISCARD_BITS2) + FAST_SQRT_CUTOFF;
}

#endif

/* Also destroys c->row. */
static void destroy_image(Display* dpy, struct inter_context* c)
{
#ifdef USE_XIMAGE
  if(c->ximage) {
# ifdef HAVE_XSHM_EXTENSION
    if(c->use_shm) {
      destroy_xshm_image(dpy, c->ximage, &c->shm_info);
    } else
# endif
    {
      /* Also frees c->ximage->data, which isn't allocated by XCreateImage. */
      XDestroyImage(c->ximage);
    }
  }

  free(c->row);
#endif
}

static void inter_free(Display* dpy, struct inter_context* c)
{
#ifndef USE_XIMAGE
  unsigned i;
#endif

  if(c->pix_buf)
    XFreePixmap(dpy, c->pix_buf);

  if(c->copy_gc)
    XFreeGC(dpy, c->copy_gc);

  destroy_image(dpy, c);

  if(c->colors <= 2)
    free(c->pal);
  else
    free_colors(c->screen, c->cmap, c->pal, c->colors);

#ifndef USE_XIMAGE
  for(i = 0; i != c->colors; ++i)
    XFreeGC(dpy, c->gcs[i]);
  free(c->gcs);
#endif

  free(c->wave_height);
  free(c->source);
}

static void abort_no_mem(void)
{
  fprintf(stderr, "interference: %s\n", strerror(ENOMEM));
  exit(1);
}

static void check_no_mem(Display* dpy, struct inter_context* c, void* ptr)
{
  if(!ptr) {
    inter_free(dpy, c);
    abort_no_mem();
  }
}

/* On allocation error, c->row == NULL. */
static void create_image(
  Display* dpy, 
  struct inter_context* c, 
  const XWindowAttributes* xgwa)
{
#ifdef USE_XIMAGE
  c->row = malloc((c->w / c->grid_size) * sizeof(uint32_t));
  check_no_mem(dpy, c, c->row);

# ifdef HAVE_XSHM_EXTENSION
  /*
   * interference used to put one row at a time to the X server. This changes
   * today.
   *
   * XShmPutImage is asynchronous; the contents of the XImage must not be
   * modified until the server has placed the data on the screen. Waiting for
   * an XShmCompletionEvent after every line of pixels is a little nutty, so
   * shared-memory XImages will cover the entire screen, and it only has to be
   * sent once per frame.
   *
   * The non-SHM code, on the other hand is noticeably slower when
   * gridsize = 1 with one row at a time. If, on the other hand, gridsize >= 2,
   * there's a slight speed increase with one row at a time.
   *
   * This uses a lot more RAM than the single line approach. Users with only
   * 4 MB of RAM may wish to disable USE_BIG_XIMAGE and specify -no-shm on the
   * command line. Since this is 2013 and desktop computers are shipping with
   * 8 GB of RAM, I doubt that this will be a major issue. - D.O.
   */

  if (c->use_shm)
    {
      c->ximage = create_xshm_image(dpy, xgwa->visual, xgwa->depth,
                                    ZPixmap, 0, &c->shm_info,
                                    xgwa->width, xgwa->height);
      if (!c->ximage)
        c->use_shm = False;
      /* If create_xshm_image fails, it will not be attempted again. */

      c->shm_can_draw = True;
    }
# endif /* HAVE_XSHM_EXTENSION */

  if (!c->ximage)
    {
      c->ximage =
        XCreateImage(dpy, xgwa->visual,
                     xgwa->depth, ZPixmap, 0, 0, /* depth, fmt, offset, data */
                     xgwa->width,                /* width */
# ifdef USE_BIG_XIMAGE
                     xgwa->height,               /* height */
# else
                     c->grid_size,               /* height */
# endif
                     8, 0);                      /* pad, bpl */

      if(c->ximage)
        {
          c->ximage->data = (char *)
            calloc(c->ximage->height, c->ximage->bytes_per_line);

          if(!c->ximage->data)
            {
              free(c->ximage);
              c->ximage = NULL;
            }
        }
    }

  if(!c->ximage)
    {
      free(c->row);
      c->row = 0;
    }

  check_no_mem(dpy, c, c->row);
#endif /* USE_XIMAGE */
}

static void create_pix_buf(Display* dpy, Window win, struct inter_context *c,
                           const XWindowAttributes* xgwa)
{
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if(c->back_buf)
    return;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  c->pix_buf = XCreatePixmap(dpy, win, xgwa->width, xgwa->height, xgwa->depth);
}

static double float_time(void)
{
  struct timeval result;
  gettimeofday(
    &result
#ifdef GETTIMEOFDAY_TWO_ARGS
    , NULL
#endif
    );

  return result.tv_usec * 1.0e-6 + result.tv_sec;
}

static void inter_init(Display* dpy, Window win, struct inter_context* c) 
{
  XWindowAttributes xgwa;
  double H[3], S[3], V[3];
  int i;
  int mono;
  int gray;
  int radius;
  XGCValues val;
  Bool dbuf = get_boolean_resource (dpy, "doubleBuffer", "Boolean");

#ifndef USE_XIMAGE
  unsigned long valmask = 0;
#endif

# ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
  dbuf = False;
# endif

  memset (c, 0, sizeof(*c));

  c->dpy = dpy;
  c->win = win;

  c->delay = get_integer_resource(dpy, "delay", "Integer");

  XGetWindowAttributes(c->dpy, c->win, &xgwa);
  c->w = xgwa.width;
  c->h = xgwa.height;
  c->cmap = xgwa.colormap;
  c->screen = xgwa.screen;

#ifdef HAVE_XSHM_EXTENSION
  c->use_shm = get_boolean_resource(dpy, "useSHM", "Boolean");
#endif /*  HAVE_XSHM_EXTENSION */

  val.function = GXcopy;
  c->copy_gc = XCreateGC(c->dpy, TARGET(c), GCFunction, &val);

  c->count = get_integer_resource(dpy, "count", "Integer");
  if(c->count < 1)
    c->count = 1;
  c->grid_size = get_integer_resource(dpy, "gridsize", "Integer");
  if(c->grid_size < 1)
    c->grid_size = 1;
  mono = get_boolean_resource(dpy, "mono", "Boolean");
  if(!mono) {
    c->colors = get_integer_resource(dpy, "ncolors", "Integer");
    if(c->colors < 2)
      c->colors = 2;
  }
  c->hue = get_integer_resource(dpy, "hue", "Float");
  while (c->hue < 0 || c->hue >= 360)
    c->hue = frand(360.0);
  c->speed = get_integer_resource(dpy, "speed", "Integer");
  c->shift = get_float_resource(dpy, "color-shift", "Float");
  while(c->shift >= 360.0)
    c->shift -= 360.0;
  while(c->shift <= -360.0)
    c->shift += 360.0;
  radius = get_integer_resource(dpy, "radius", "Integer");;
  if(radius < 1)
    radius = 1;

  create_image(dpy, c, &xgwa);

  if(!mono) {
    c->pal = calloc(c->colors, sizeof(XColor));
    check_no_mem(dpy, c, c->pal);

    gray = get_boolean_resource(dpy, "gray", "Boolean");
    if(!gray) {
      H[0] = c->hue;
      H[1] = H[0] + c->shift < 360.0 ? H[0]+c->shift : H[0] + c->shift-360.0; 
      H[2] = H[1] + c->shift < 360.0 ? H[1]+c->shift : H[1] + c->shift-360.0; 
      S[0] = S[1] = S[2] = 1.0;
      V[0] = V[1] = V[2] = 1.0;
    } else {
      H[0] = H[1] = H[2] = 0.0;
      S[0] = S[1] = S[2] = 0.0;
      V[0] = 1.0; V[1] = 0.5; V[2] = 0.0;
    }

    make_color_loop(c->screen, xgwa.visual, c->cmap, 
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
    check_no_mem(dpy, c, c->pal);
    c->pal[0].pixel = BlackPixel(c->dpy, DefaultScreen(c->dpy));
    c->pal[1].pixel = WhitePixel(c->dpy, DefaultScreen(c->dpy));
  }

#ifdef HAVE_XSHM_EXTENSION
  if(c->use_shm)
    dbuf = False;
  /* Double-buffering doesn't work with MIT-SHM: XShmPutImage must draw to the
   * window. Otherwise, XShmCompletion events will have the XAnyEvent::window
   * field set to the back buffer, and XScreenSaver will ignore the event. */
#endif

  if (dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      c->back_buf = xdbe_get_backbuffer (c->dpy, c->win, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

      create_pix_buf(dpy, win, c, &xgwa);
    }

#ifndef USE_XIMAGE
  valmask = GCForeground;
  c->gcs = calloc(c->colors, sizeof(GC));
  check_no_mem(dpy, c, c->gcs);
  for(i = 0; i < c->colors; i++) {
    val.foreground = c->pal[i].pixel;    
    c->gcs[i] = XCreateGC(c->dpy, TARGET(c), valmask, &val);
  }
#endif

#if defined USE_FAST_SQRT_HACKISH
  c->radius = fast_log2(radius * radius);
#elif defined USE_FAST_SQRT_BIGTABLE2
  c->radius = radius * radius;
  c->radius = FAST_TABLE(c->radius);
#else
  c->radius = radius;
#endif

  c->wave_height = calloc(c->radius, sizeof(unsigned));
  check_no_mem(dpy, c, c->wave_height);

  for(i = 0; i < c->radius; i++) {
    float max, fi;
#if defined USE_FAST_SQRT_HACKISH
    fi = sqrt(fast_inv_log2(i));
#elif defined USE_FAST_SQRT_BIGTABLE2
    fi = sqrt(fast_inv_table(i));
#else
    fi = i;
#endif
    max = 
      ((float)c->colors) * 
      ((float)radius - fi) /
      ((float)radius);
    c->wave_height[i] = 
      (unsigned)
      ((max + max*cos(fi/50.0)) / 2.0);
  }

  c->source = calloc(c->count, sizeof(struct inter_source));
  check_no_mem(dpy, c, c->source);

  for(i = 0; i < c->count; i++) {
    c->source[i].x_theta = frand(2.0)*3.14159;
    c->source[i].y_theta = frand(2.0)*3.14159;
  }

  c->last_frame = float_time();
}

#define source_x(c, i) \
  (c->w/2 + ((int)(cos(c->source[i].x_theta)*((float)c->w/2.0))))
#define source_y(c, i) \
  (c->h/2 + ((int)(cos(c->source[i].y_theta)*((float)c->h/2.0))))

/*
 * This is rather suboptimal. Calculating the distance per-pixel is going to
 * be a lot slower than using now-ubiquitous SIMD CPU instructions to do four
 * or eight pixels at a time. Plus, this could be almost trivially
 * parallelized, what with all the multi-core hardware nowadays.
 */

static unsigned long do_inter(struct inter_context* c)
{
  int i, j, k;
  unsigned result;
  int dist;
  int g = c->grid_size;
  unsigned w_div_g = c->w/g;

  int dx, dy;
  int px, py;

#ifdef USE_XIMAGE
  unsigned img_y = 0;
  void *scanline = c->ximage->data;
#endif

  double now;
  float elapsed;

#if defined USE_XIMAGE && defined HAVE_XSHM_EXTENSION
  /* Wait a little while for the XServer to become ready if necessary. */
  if(c->use_shm && !c->shm_can_draw)
    return 2000;
#endif

  now = float_time();
  elapsed = (now - c->last_frame) * 10.0;

  c->last_frame = now;

  for(i = 0; i < c->count; i++) {
    c->source[i].x_theta += (elapsed*c->speed/1000.0);
    if(c->source[i].x_theta > 2.0*3.14159)
      c->source[i].x_theta -= 2.0*3.14159;
    c->source[i].y_theta += (elapsed*c->speed/1000.0);
    if(c->source[i].y_theta > 2.0*3.14159)
      c->source[i].y_theta -= 2.0*3.14159;
    c->source[i].x = source_x(c, i);
    c->source[i].y = source_y(c, i);
  }

  for(j = 0; j < c->h/g; j++) {
    for(i = 0; i < w_div_g; i++) {
      result = 0;
      px = i*g + g/2;
      py = j*g + g/2;
      for(k = 0; k < c->count; k++) {

        dx = px - c->source[k].x;
        dy = py - c->source[k].y;

        /*
         * Other possibilities for improving performance here:
         * 1. Using octagon-based distance estimation
         *    (Which causes giant octagons to appear.)
         * 2. Square root approximation by reinterpret-casting IEEE floats to
         *    integers.
         *    (Which causes angles to appear when two waves interfere.)
         */

/*      int_float u;
        u.f = dx*dx + dy*dy;
        u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
        dist = u.f; */

#if defined USE_FAST_SQRT_BIGTABLE2
        dist = dx*dx + dy*dy;
        dist = FAST_TABLE(dist);
#elif defined USE_FAST_SQRT_HACKISH
        dist = fast_log2(dx*dx + dy*dy);
#else
        dist = sqrt(dx*dx + dy*dy);
#endif

        result += (dist >= c->radius ? 0 : c->wave_height[dist]);
      }

      /* It's slightly faster to do a subtraction or two before calculating the
       * modulus. - D.O. */
      if(result >= c->colors)
      {
        result -= c->colors;
        if(result >= c->colors)
          result %= (unsigned)c->colors;
      }

#ifdef USE_XIMAGE
      c->row[i] = c->pal[result].pixel;
#else
      XFillRectangle(c->dpy, TARGET(c), c->gcs[result], g*i, g*j, g, g);
#endif /* USE_XIMAGE */
    }

#ifdef USE_XIMAGE
    /* Fill in these `gridsize' horizontal bits in the scanline */
    if(c->ximage->bits_per_pixel == 32)
    {
      uint32_t *ptr = (uint32_t *)scanline;
      for(i = 0; i < w_div_g; i++) {
        for(k = 0; k < g; k++)
          ptr[g*i+k] = c->row[i];
      }
    }
    else if(c->ximage->bits_per_pixel == 24)
    {
      uint8_t *ptr = (uint8_t *)scanline;
      for(i = 0; i < w_div_g; i++) {
        for(k = 0; k < g; k++) {
          uint32_t pixel = c->row[i];
          /* Might not work on big-endian. */
          ptr[0] = pixel;
          ptr[1] = (pixel & 0x0000ff00) >> 8;
          ptr[2] = (pixel & 0x00ff0000) >> 16;
          ptr += 3;
        }
      }
    }
    else if(c->ximage->bits_per_pixel == 16)
    {
      uint16_t *ptr = (uint16_t *)scanline;
      for(i = 0; i < w_div_g; i++) {
        for(k = 0; k < g; k++)
          ptr[g*i+k] = c->row[i];
      }
    }
    else if(c->ximage->bits_per_pixel == 8)
    {
      uint8_t *ptr = (uint8_t *)scanline;
      for(i = 0; i < w_div_g; i++) {
        for(k = 0; k < g; k++)
          ptr[g*i+k] = c->row[i];
      }
    }
    else
    {
      for(i = 0; i < w_div_g; i++) {
        for(k = 0; k < g; k++)
          XPutPixel(c->ximage, (g*i)+k, img_y, c->row[i]);
      }
    }

    /* Only the first scanline of the image has been filled in; clone that
       scanline to the rest of the `gridsize' lines in the ximage */
    for(k = 0; k < (g-1); k++)
      memcpy(c->ximage->data + (c->ximage->bytes_per_line * (img_y + k + 1)),
             c->ximage->data + (c->ximage->bytes_per_line * img_y),
             c->ximage->bytes_per_line);

# ifndef USE_BIG_XIMAGE
    /* Move the bits for this horizontal stripe to the server. */
#  ifdef HAVE_XSHM_EXTENSION
    if (!c->use_shm)
#  endif /*  HAVE_XSHM_EXTENSION */
      XPutImage(c->dpy, TARGET(c), c->copy_gc, c->ximage,
                0, 0, 0, g*j, c->ximage->width, c->ximage->height);
# endif

# if defined HAVE_XSHM_EXTENSION && !defined USE_BIG_XIMAGE
    if (c->use_shm)
# endif
    {
# if defined HAVE_XSHM_EXTENSION || defined USE_BIG_XIMAGE
      scanline = (char *)scanline + c->ximage->bytes_per_line * g;
      img_y += g;
# endif
    }

#endif /* USE_XIMAGE */
  }

#ifdef HAVE_XSHM_EXTENSION
  if (c->use_shm)
  {
    XShmPutImage(c->dpy, c->win, c->copy_gc, c->ximage,
                 0, 0, 0, 0, c->ximage->width, c->ximage->height,
                 True);
    c->shm_can_draw = False;
  }
#endif
#if defined HAVE_XSHM_EXTENSION && defined USE_BIG_XIMAGE
  else
#endif
#ifdef USE_BIG_XIMAGE
  {
    XPutImage(c->dpy, TARGET(c), c->copy_gc, c->ximage,
              0, 0, 0, 0, c->ximage->width, c->ximage->height);
  }
#endif

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

  return c->delay;
}

static void *
interference_init (Display *dpy, Window win)
{
  struct inter_context *c = (struct inter_context *) calloc (1, sizeof(*c));
  if(!c)
    abort_no_mem();
  inter_init(dpy, win, c);
  return c;
}

static unsigned long
interference_draw (Display *dpy, Window win, void *closure)
{
  struct inter_context *c = (struct inter_context *) closure;
  return do_inter(c);
}

static void
interference_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct inter_context *c = (struct inter_context *) closure;
  XWindowAttributes xgwa;
  Bool dbuf = (c->pix_buf
# ifdef HAVE_DOUBLE_BUFFER_EXTENSION
               || c->back_buf
# endif
               );

  c->w = w;
  c->h = h;

#ifdef USE_XIMAGE
  destroy_image(dpy, c);
  c->ximage = 0;
#endif

  if(c->pix_buf)
    XFreePixmap(dpy, c->pix_buf);
  c->pix_buf = None;

  XGetWindowAttributes(dpy, window, &xgwa);
  xgwa.width = w;
  xgwa.height = h;
  create_image(dpy, c, &xgwa);
  if(dbuf)
    create_pix_buf(dpy, window, c, &xgwa);
}

static Bool
interference_event (Display *dpy, Window window, void *closure, XEvent *event)
{
#if HAVE_XSHM_EXTENSION
  struct inter_context *c = (struct inter_context *) closure;

  if(c->use_shm && event->type == XShmGetEventBase(dpy) + ShmCompletion)
  {
    c->shm_can_draw = True;
    return True;
  }
#endif
  return False;
}

static void
interference_free (Display *dpy, Window window, void *closure)
{
  struct inter_context *c = (struct inter_context *) closure;
  inter_free(dpy, c);
}

XSCREENSAVER_MODULE ("Interference", interference)

