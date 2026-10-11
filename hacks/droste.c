/* xscreensaver, Copyright © 2023-2025 Jamie Zawinski <jwz@jwz.org>
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
#include "pow2.h"
#include "thread_util.h"
#include "xshm.h"
#include "doubletime.h"

#define DO_LOG_TABLES /* Dave's hairier and faster version */

#ifndef DO_LOG_TABLES
/* Complex numbers have been part of ANSI C since 1999, which is 24 years
   ago as of this writing, so I'm gonna guess that they are finally portable
   enough to use in XScreenSaver. */
# include <complex.h>
#endif

static const char *droste_defaults [] = {
  "*fpsSolid:			true",
  ".background:			Black",
  ".foreground:			#BEBEBE",
  "*delay:			20000",
  "*duration:			120",
  "*r1:				0.2",
  "*r2:				0.7",
  "*speed:			1.0",
  "*useSHM:                     True",
#ifdef HAVE_MOBILE
  "*ignoreRotation:             True",
  "*rotateImages:               True",
#endif
  THREAD_DEFAULTS
  0
};

static XrmOptionDescRec droste_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-duration",	".duration",		XrmoptionSepArg, 0 },
  { "-r1",		".r1",			XrmoptionSepArg, 0 },
  { "-r2",		".r2",			XrmoptionSepArg, 0 },
  { "-speed",		".speed",		XrmoptionSepArg, 0 },
  THREAD_OPTIONS
  { 0, 0, 0, 0 }
};


/* Zoom in a bit to hide the outermost edge of the spiral.  If it goes too
   large or too negative, we just get tiles, not spirals. */

#ifdef DO_LOG_TABLES
# define DEF_ZOOM log(0.3)
# define LOG2_BITS 9
# define ATAN_TABLE_EXTRA 4 /* Must be at least 1. */
#else
# define DEF_ZOOM    0.3
#endif

#define ZOOM_SCALE 66

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  Pixmap pixmap;
  XImage *in, *out;
  XShmSegmentInfo shminfo;
  GC gc;
  double start_time;
  async_load_state *img_loader;
  int delay, duration;
  double r1, r2;
  double zoom, speed;
  double angle;
  int angle_sign;
  XRectangle geom;

# ifdef DO_LOG_TABLES
  float log2_table[1 << LOG2_BITS], atan_table[768];
  float exp_table[768], sin_table[1 << 12];
# endif

  struct threadpool threadpool;

  /* Cached values shared by each thread when rendering one frame */
# ifdef DO_LOG_TABLES
  float orat;
  float i01i, i01i_scale;
  float clog_mul;
# else /* !DO_LOG_TABLES */
  double scale;
  double ixr, iyr;
  double oxr, oyr;
  double complex i0;
# endif /* !DO_LOG_TABLES */
};

struct thread {
  const struct state *st;
  int thread_id;

# ifdef DO_LOG_TABLES
  /* The following doesn't need to stick around between frames. */
  size_t den;

  unsigned zabs2;

  size_t div, mod;
  size_t r;
  size_t frac;

  float zr, zi;
  float zr_i0;
# endif /* DO_LOG_TABLES */
};


/* Called for each thread, once at startup. */
static int
droste_thread_create (void *t_raw,
                      struct threadpool *pool,
                      unsigned id)
{
  struct thread *t = (struct thread *) t_raw;
  t->st = GET_PARENT_OBJ (struct state, threadpool, pool);
  t->thread_id = id;
  return 0;
}

static void
droste_thread_destroy (void *t_raw)
{
}


/* Initialize the cached values shared by each thread. */
static void
droste_thread_frame_init (struct state *st)
{
  int iw = st->in->width;
  int ih = st->in->height;
  int ow = st->out->width;
  int oh = st->out->height;

  double iaspect = (double) iw / ih;
  double oaspect = (double) ow / oh;

# ifdef DO_LOG_TABLES
  double scale = log (st->r2 / st->r1);

  /* Fill output rect with input image with no black margins. */
  double irat = st->r1 * iw;

# else /* !DO_LOG_TABLES */
  /* Fill output rect with input image with no black margins. */
  if (iaspect > oaspect)
    {
      st->ixr = oaspect / iaspect;
      st->iyr = 1;
    }
  else
    {
      st->ixr = 1;
      st->iyr = iaspect / oaspect;
    }
# endif /* !DO_LOG_TABLES */

  /* Since the spiral cut implicitly zooms in a bunch, favoring the center
     of the image, let's zoom out a bit so that more of the image shows.
     At extreme radii and aspect ratios this might show the image edges. */
# ifdef DO_LOG_TABLES
  irat *= (iaspect == oaspect ? 0.7 / oaspect : 0.8);
# else /* !DO_LOG_TABLES */
  {
    double s = (iaspect == oaspect
                ? 0.7
                : 0.8 * (iaspect > oaspect ? iaspect : oaspect));
    st->ixr *= s;
    st->iyr *= s;
  }
# endif /* !DO_LOG_TABLES */

  /* Make the spiral have a 1:1 aspect ratio, instead of being a horizontal
     oval for landscape images and a vertical oval for portrait. */
# ifdef DO_LOG_TABLES
  st->orat = (log (0.5 / oh) + st->zoom) / scale;
# else /* !DO_LOG_TABLES */
  st->oxr = oaspect;
  st->oyr = 1;

  /* And now back that out again, so that the underlying image's aspect
     does not change as a result of that. */
  st->ixr /= st->oxr;
  st->iyr /= st->oyr;
# endif /* !DO_LOG_TABLES */

  /* Intermediate values used on every pixel that can be precomputed. */
# ifdef DO_LOG_TABLES
  st->angle = atan (scale / (2 * M_PI)) * st->angle_sign;
  st->i01i = -tan (st->angle);
  st->i01i_scale = (st->i01i / scale) * ((2 * M_PI) / countof(st->sin_table));
  st->i01i *= countof(st->sin_table) / (2 * M_PI);
  st->clog_mul = 0.5 * log(2) / scale;

  for (size_t i = 0; i != countof (st->exp_table); ++i)
    {
      double zr = (double) i / countof (st->exp_table);
      zr = (zr - 0.5) * scale;
      st->exp_table[i] = expf (zr) * irat;
    }
# else /* !DO_LOG_TABLES */
  st->scale = log (st->r2 / st->r1);
  st->angle = atan (st->scale / (2 * M_PI)) * st->angle_sign;
  st->i0 = cexp (0 + st->angle * I) * cos (st->angle);
# endif /* !DO_LOG_TABLES */
}


# ifdef DO_LOG_TABLES
static unsigned long
pixel (struct thread *t, float zi)
{
  const struct state *st = t->st;
  int iw = st->in->width;
  int ih = st->in->height;

  int ix, iy;

 /* C fmod:      x - y * trunc(x/y)  -- towards zero
    C remainder: x - y * floor(x/y)  -- towards -inf
    C remainder = GLSL mod
  */
#if 1
  /* Scale and rotate strips */
  float zr = t->zr - zi * st->i01i_scale;
  zi += t->zr_i0;

  /* Tile strips */
  zr -= floor(zr);

  /* Annulus to strip */
  float f = st->exp_table[(size_t) (zr * countof (st->exp_table))];
  ptrdiff_t angle = zi;
  const size_t sin_table_mod = (countof(st->sin_table) - 1);
  zr = f * st->sin_table[(angle + countof(st->sin_table) / 4) & sin_table_mod];
  zi = f * st->sin_table[angle & sin_table_mod];
#endif

  /* [ -0.5, 0.5 ] => [ 0, WH ] */
  ix = zr + iw * 0.5f;
  iy = zi + ih * 0.5f;

/* if (ix < 0 || iy < 0 || ix >= iw || iy >= ih) abort(); */
  return ((ix < 0 || iy < 0 || ix >= iw || iy >= ih)		    /* Clip */
          ? BlackPixelOfScreen (st->xgwa.screen)
          : XGetPixel (st->in, ix, iy));
}


static void
clog_init (struct thread *t, unsigned or, unsigned oi)
{
  or = 2 * or + 1;
  oi = 2 * oi + 1;

  t->zabs2 = or * or + oi * oi;

  size_t num = countof(t->st->atan_table) - ATAN_TABLE_EXTRA, start = oi;
  t->den = or;

  t->div = num / t->den;
  t->mod = num % t->den;
  size_t start_mod = start * t->mod;
  t->r = start * t->div + start_mod / t->den;
  t->frac = start_mod % t->den;

  t->div *= 2;
  t->mod *= 2;
  if (t->mod > t->den) {
    t->mod -= t->den;
    ++t->div;
  }
}


static void
clog_z (struct thread *t, int oi)
{
# if 1
  /* Tile strips to ordinary space */
  int log_zabs2 = i_log2_fast(t->zabs2);
  int shift = LOG2_BITS - log_zabs2;
  t->zr = (
    log_zabs2 +
    t->st->log2_table[
      (shift >= 0 ? t->zabs2 << shift : t->zabs2 >> -shift) -
        countof (t->st->log2_table)]) * t->st->clog_mul + t->st->orat;

  t->zabs2 += 8 * (oi + 1);

  t->zi = t->st->atan_table[t->r];

  t->r += t->div;
  t->frac += t->mod;
  if (t->frac >= t->den) {
    t->frac -= t->den;
    ++t->r;
  }

  t->zr_i0 = t->zr * t->st->i01i;
  t->zr += 0.5;
# endif
}
# endif /* DO_LOG_TABLES */


/* Called in each thread, for doing one slice of the image. 
 */
static void
droste_thread_run (void *t_raw)
{
  struct thread *t = (struct thread *) t_raw;
  const struct state *st = t->st;
# ifdef DO_LOG_TABLES
  int cx = st->out->width / 2;
  int cy = st->out->height / 2;

  const size_t N = countof(st->sin_table) / 4;

  int or;
  int ormax = cx < cy ? cx : cy;
  for (or = 0; or != ormax; or++)
    {
      /* Some CPU cache contention at the very center. */
      unsigned or1 = or + 1;
      unsigned oi = or1 * t->thread_id / st->threadpool.count;
      unsigned oimax = or1 * (t->thread_id + 1) / st->threadpool.count;

      clog_init (t, or, oi);

      for (; oi != oimax; oi++)
        {
          clog_z (t, oi);

          XPutPixel (st->out, cx + or,     cy - oi - 1,
                     pixel (t, -t->zi - 0 * N));
          XPutPixel (st->out, cx + oi,     cy - or - 1,
                     pixel (t,  t->zi - 1 * N));
          XPutPixel (st->out, cx - oi - 1, cy - or - 1,
                     pixel (t, -t->zi - 1 * N));
          XPutPixel (st->out, cx - or - 1, cy - oi - 1,
                     pixel (t,  t->zi - 2 * N));
          XPutPixel (st->out, cx - or - 1, cy + oi,
                     pixel (t, -t->zi + 2 * N));
          XPutPixel (st->out, cx - oi - 1, cy + or,
                     pixel (t,  t->zi + 1 * N));
          XPutPixel (st->out, cx + oi,     cy + or,
                     pixel (t, -t->zi + 1 * N));
          XPutPixel (st->out, cx + or,     cy + oi,
                     pixel (t,  t->zi + 0 * N));
        }
    }

  if (cx > cy)
    {
      unsigned oi0 = cy * t->thread_id / st->threadpool.count;
      unsigned oi1 = cy * (t->thread_id + 1) / st->threadpool.count;

      for (or = ormax; or != cx; or++)
        {
          clog_init (t, or, oi0);
          unsigned oi;
          for (oi = oi0; oi != oi1; oi++)
            {
              clog_z (t, oi);

              XPutPixel (st->out, cx + or,     cy - oi - 1,
                         pixel (t, -t->zi - 0 * N));
              XPutPixel (st->out, cx - or - 1, cy - oi - 1,
                         pixel (t,  t->zi - 2 * N));
              XPutPixel (st->out, cx - or - 1, cy + oi,
                         pixel (t, -t->zi + 2 * N));
              XPutPixel (st->out, cx + or,     cy + oi,
                         pixel (t,  t->zi + 0 * N));
            }
        }
    }
  else
    {
      unsigned oi0 = cx * t->thread_id / st->threadpool.count;
      unsigned oi1 = cx * (t->thread_id + 1) / st->threadpool.count;

      for (or = ormax; or != cy; or++)
        {
          clog_init (t, or, oi0);
          unsigned oi;
          for (oi = oi0; oi != oi1; oi++)
            {
              clog_z (t, oi);

              XPutPixel (st->out, cx + oi,     cy - or - 1,
                         pixel (t,  t->zi - 1 * N));
              XPutPixel (st->out, cx - oi - 1, cy - or - 1,
                         pixel (t, -t->zi - 1 * N));
              XPutPixel (st->out, cx - oi - 1, cy + or,
                         pixel (t,  t->zi + 1 * N));
              XPutPixel (st->out, cx + oi,     cy + or,
                         pixel (t, -t->zi + 1 * N));
            }
        }
    }

# else /* !DO_LOG_TABLES */

  unsigned long black = BlackPixelOfScreen (st->xgwa.screen);
  int iw = st->in->width;
  int ih = st->in->height;
  int ow = st->out->width;
  int oh = st->out->height;
  double zoom = st->zoom;
  double ixr = st->ixr;
  double iyr = st->iyr;
  double oxr = st->oxr;
  double oyr = st->oyr;
  double complex i0 = st->i0;
  double scale = st->scale;
  double r1 = st->r1;

  int step = oh / st->threadpool.count;
  int ox, oy = t->thread_id * step;
  int oy2 = oy + step;
  if (oy2 > oh) oy2 = oh;

  for (; oy < oy2; oy++)
    for (ox = 0; ox < ow; ox++)
      {
        double complex z = (((((double) ox / ow) - 0.5) * oxr) +
                            ((((double) oy / oh) - 0.5) * oyr) * I);
        int ix, iy;
        unsigned long p;

       /* C fmod:      x - y * trunc(x/y)  -- towards zero
          C remainder: x - y * floor(x/y)  -- towards -inf
          C remainder = GLSL mod
        */
#if 1
        z *= zoom;
        z = clog (z);			   /* Tile strips to ordinary space */
        z = z / i0;				 /* Scale and rotate strips */
        z = remainder (creal(z), scale) + cimag(z) * I;	     /* Tile strips */
        z = cexp (z) * r1;				/* Annulus to strip */
#endif

        ix = iw * (creal(z) * ixr + 0.5);     /* [ -0.5, 0.5 ] => [ 0, WH ] */
        iy = ih * (cimag(z) * iyr + 0.5);

     /* if (ix < 0 || iy < 0 || ix >= iw || iy >= ih) abort(); */
        p = ((ix < 0 || iy < 0 || ix >= iw || iy >= ih)		    /* Clip */
             ? black
             : XGetPixel (st->in, ix, iy));
        XPutPixel (st->out, ox, oy, p);
      }
# endif /* !DO_LOG_TABLES */
}


static void *
droste_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  long gcflags;

  st->dpy = dpy;
  st->window = window;
  st->start_time = 0;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  if (st->delay < 0) st->delay = 0;
  if (st->duration < 1) st->duration = 1;

  st->r1 = get_float_resource (st->dpy, "r1", "Radius");
  st->r2 = get_float_resource (st->dpy, "r2", "Radius");
  if (st->r1 < 0) st->r1 = 0;
  if (st->r1 > 1) st->r1 = 1;
  if (st->r2 < 0) st->r2 = 0;
  if (st->r2 > 1) st->r2 = 1;

  st->zoom = DEF_ZOOM;
  st->speed = get_float_resource (st->dpy, "speed", "Ratio");
  if (st->speed > ZOOM_SCALE-1) st->speed = ZOOM_SCALE-1;

  st->angle_sign = (random() & 1) ? 1 : -1;

  gcv.function = GXcopy;
  gcflags = GCFunction;
  st->gc = XCreateGC (st->dpy, st->window, gcflags, &gcv);

# ifdef DO_LOG_TABLES
  for(size_t i = 0; i != countof (st->log2_table); ++i)
    st->log2_table[i] = log2(1 + i * (1.0 / countof (st->log2_table)));
  for (size_t i = 0; i != countof(st->atan_table); ++i)
    {
      st->atan_table[i] =
        atan((double) i / (countof(st->atan_table) - ATAN_TABLE_EXTRA)) *
          (countof(st->sin_table) / (2 * M_PI));
    }
  for (size_t i = 0; i != countof(st->sin_table); ++i)
    st->sin_table[i] = sin(i * (2 * M_PI / countof(st->sin_table)));
# endif /* DO_LOG_TABLES */

  /* Create the (idle) threads. */
  {
    static const struct threadpool_class cls = {
      sizeof(struct thread),
      droste_thread_create,
      droste_thread_destroy
    };
    threadpool_create (&st->threadpool, &cls, dpy, hardware_concurrency (dpy));
  }

  return st;
}


static unsigned long
droste_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->img_loader)		/* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0,
                                                &st->geom);
      if (! st->img_loader)	/* just finished */
        {
          st->start_time = double_time();

          if (st->in)
            XDestroyImage (st->in);
          if (st->out)
            destroy_xshm_image (st->dpy, st->out, &st->shminfo);

          st->in = XGetImage (st->dpy, st->pixmap,
                              st->geom.x, st->geom.y,
                              st->geom.width, st->geom.height,
                              ~0L, ZPixmap);

          st->out = create_xshm_image (st->dpy, st->xgwa.visual,
                                       st->xgwa.depth, ZPixmap,
                                       &st->shminfo,
                                       (st->xgwa.width + 1) & ~1,
                                       (st->xgwa.height + 1) & ~1);

          st->angle_sign = (random() & 1) ? 1 : -1;
          st->zoom = DEF_ZOOM;
        }
      return st->delay;
    }

  if (!st->pixmap ||
      (!st->img_loader &&
       st->start_time + st->duration < double_time()))
    {
      int w, h;
      double s;
      XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
      if (st->pixmap)
        XFreePixmap (st->dpy, st->pixmap);

      /* Let's load an image somewhat larger than the window so that when
         we see the deep zoom at the outer edges, it is less pixellated. */
      s = 2.5;
      w = s * st->xgwa.width;
      h = s * st->xgwa.height;
      st->pixmap = XCreatePixmap (st->dpy, st->window, w, h, st->xgwa.depth);
      st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                                st->pixmap, 0, &st->geom);
      st->start_time = double_time();
      return st->delay;
    }

# ifdef DO_LOG_TABLES
  st->zoom += log1p ((-1.0 / ZOOM_SCALE) * st->speed);
# else /* !DO_LOG_TABLES */
  st->zoom *= 1 - ((1.0 / ZOOM_SCALE) * st->speed);

  if (st->zoom >=  HUGE_VAL ||
      st->zoom <= -HUGE_VAL ||
      (st->zoom <=  4.94e-324 &&	/* Approx. smallest magnitude double */
       st->zoom >= -4.94e-324))
    st->zoom = DEF_ZOOM;		/* Reset is our only option */
# endif /* !DO_LOG_TABLES */

  droste_thread_frame_init (st);
  threadpool_run (&st->threadpool, droste_thread_run);
  threadpool_wait (&st->threadpool);
  put_xshm_image (st->dpy, st->window, st->gc, st->out,
                  0, 0,
                  (st->xgwa.width  - st->out->width) / 2,
                  (st->xgwa.height - st->out->height) / 2,
                  st->out->width, st->out->height,
                  &st->shminfo);

  return st->delay;
}


static void
droste_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  /* Load a new image shortly after resizing stops, to avoid starting
     a zillion image loaders as the resize events flood in. */
  if (st->start_time > 0)
    st->start_time = double_time() - st->duration + 0.25;
}

static Bool
droste_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      switch (keysym) {
      case XK_Up:    st->r1 += 0.005; goto P1;		/* R1 */
      case XK_Down:  st->r1 -= 0.005; goto P1;
      case XK_Right: st->r2 += 0.005; goto P1;		/* R2 */
      case XK_Left:  st->r2 -= 0.005; goto P1;
      default: break;
      }
      switch (c) {
      case '-': case '_': st->speed -= 0.1; goto P2;	/* Zoom */
      case '=': case '+': st->speed += 0.1; goto P2;
      case '<': case ',': st->speed -= 0.1; goto P2;
      case '>': case '.': st->speed += 0.1; goto P2;
      default: break;
      }
    }

  if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;
      return True;
    }

  return False;

 P1:
  fprintf (stderr, "--r1 %.3f --r2 %.3f\n", st->r1, st->r2);
  return True;
 P2:
  fprintf (stderr, "--speed %.3f\n", st->speed);
  return True;
}

static void
droste_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->pixmap)
    XFreePixmap (st->dpy, st->pixmap);
  if (st->in)
    XDestroyImage (st->in);
  if (st->out)
    destroy_xshm_image (st->dpy, st->out, &st->shminfo);
  if (st->threadpool.count)
    threadpool_destroy (&st->threadpool);
  XFreeGC (dpy, st->gc);
  free (st);
}

XSCREENSAVER_MODULE ("Droste", droste)
