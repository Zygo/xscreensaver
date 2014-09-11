/* xscreensaver, Copyright (c) 1992-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Rotate a bitmap using using bitblts.
   The bitmap must be square, and must be a power of 2 in size.
   This was translated from SmallTalk code which appeared in the
   August 1981 issue of Byte magazine.

   The input bitmap may be non-square, it is padded and centered
   with the background color.  Another way would be to subdivide
   the bitmap into square components and rotate them independently
   (and preferably in parallel), but I don't think that would be as
   interesting looking.

   It's too bad almost nothing uses blitter hardware these days,
   or this might actually win.
 */

#include "screenhack.h"
#include "xpm-pixmap.h"
#include <stdio.h>

#include "images/som.xbm"

/* Implementing this using XCopyArea doesn't work with color images on OSX.
   This means that the Cocoa implementation of XCopyArea in jwxyz.m is 
   broken with the GXor, GXand, and/or the GXxor GC operations.  This
   probably means that (e.g.) "kCGBlendModeDarken" is not close enough 
   to being "GXand" to use for that.  (It works with monochrome images,
   just not color ones).

   So, on OSX, we implement the blitter by hand.  It is correct, but
   orders of magnitude slower.
 */
#ifndef HAVE_COCOA
# define USE_XCOPYAREA
#endif

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  int width, height, size;
  Bool scale_up;
  Pixmap self, temp, mask;
# ifdef USE_XCOPYAREA
  GC gc_set, gc_clear, gc_copy, gc_and, gc_or, gc_xor;
# endif
  GC gc;
  int delay, delay2;
  int duration;
  Pixmap bitmap;
  unsigned int fg, bg;

  int qwad; /* fuckin' C, man... who needs namespaces? */
  int first_time;
  int last_w, last_h;

  time_t start_time;
  Bool loaded_p;
  Bool load_ext_p;
  async_load_state *img_loader;
};

static void display (struct state *, Pixmap);
static void blitspin_init_2 (struct state *);

#define copy_to(from, xoff, yoff, to, op)				\
  bitblt (st, st->from, st->to, op, 0, 0,				\
	  st->size-(xoff), st->size-(yoff), (xoff), (yoff))

#define copy_from(to, xoff, yoff, from, op)				\
  bitblt (st, st->from, st->to, op, (xoff), (yoff),			\
	  st->size-(xoff), st->size-(yoff), 0, 0)


#ifdef USE_XCOPYAREA
# define bitblt(st, from, to, op, src_x, src_y, w, h, dst_x, dst_y)	\
         XCopyArea((st)->dpy, (from), (to), (st)->gc_##op,		\
		   (src_x), (src_y), (w), (h), (dst_x), (dst_y))
#else /* !USE_XCOPYAREA */

# define bitblt(st, from, to, op, src_x, src_y, w, h, dst_x, dst_y)	\
         do_bitblt((st)->dpy, (from), (to), st->gc, GX##op,		\
		   (src_x), (src_y), (w), (h), (dst_x), (dst_y))

static void
do_bitblt (Display *dpy, Drawable src, Drawable dst, GC gc, int op,
           int src_x, int src_y,
           unsigned int width, unsigned int height,
           int dst_x, int dst_y)
{
  if (op == GXclear)
    {
      XSetForeground (dpy, gc, 0xFF000000);  /* ARGB black for Cocoa */
      XFillRectangle (dpy, dst, gc, dst_x, dst_y, width, height);
    }
  else if (op == GXset)
    {
      XSetForeground (dpy, gc, ~0L);
      XFillRectangle (dpy, dst, gc, dst_x, dst_y, width, height);
    }
  else if (op == GXcopy)
    {
      XCopyArea (dpy, src, dst, gc, src_x, src_y, width, height, dst_x, dst_y);
    }
  else
    {
      XImage *srci = XGetImage (dpy, src, src_x, src_y, width, height,
                                ~0L, ZPixmap);
      XImage *dsti = XGetImage (dpy, dst, dst_x, dst_y, width, height,
                                ~0L, ZPixmap);
      unsigned long *out = (unsigned long *) dsti->data;
      unsigned long *in  = (unsigned long *) srci->data;
      unsigned long *end = (in + (height * srci->bytes_per_line
                                  / sizeof(unsigned long)));
      switch (op)
        {
        case GXor:  while (in < end) { *out++ |= *in++; } break;
        case GXand: while (in < end) { *out++ &= *in++; } break;
        case GXxor: while (in < end) { *out++ ^= *in++; } break;
        default: abort();
        }
      XPutImage (dpy, dst, gc, dsti, 0, 0, dst_x, dst_y, width, height);
      XDestroyImage (srci);
      XDestroyImage (dsti);
    }
}

#endif /* !USE_XCOPYAREA */



static unsigned long
blitspin_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay;
  int qwad;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);

      if (!st->img_loader) { /* just finished */
        st->first_time = 0;
        st->loaded_p = True;
        st->qwad = -1;
        st->start_time = time ((time_t) 0);
        blitspin_init_2 (st);
      }

      /* Rotate nothing if the very first image is not yet loaded */
      if (! st->loaded_p)
        return this_delay;
    }

  if (!st->img_loader &&
      st->load_ext_p &&
      st->start_time + st->duration < time ((time_t) 0)) {
    /* Start a new image loading, but keep rotating the old image 
       until the new one arrives. */
    st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                              st->bitmap, 0, 0);
  }

  if (st->qwad == -1) 
    {
      bitblt(st, st->mask, st->mask, clear,0,0, st->size,    st->size,    0,0);
      bitblt(st, st->mask, st->mask, set,  0,0, st->size>>1, st->size>>1, 0,0);
      st->qwad = st->size>>1;
    }

  if (st->first_time)
    {
      st->first_time = 0;
      display (st, st->self);
      return st->delay2;
    }

  /* for (st->qwad = st->size>>1; st->qwad > 0; st->qwad>>=1) */

  qwad = st->qwad;

  copy_to   (mask, 0,       0,       temp, copy);   /* 1 */
  copy_to   (mask, 0,       qwad,    temp, or);     /* 2 */
  copy_to   (self, 0,       0,       temp, and);    /* 3 */
  copy_to   (temp, 0,       0,       self, xor);    /* 4 */
  copy_from (temp, qwad,    0,       self, xor);    /* 5 */
  copy_from (self, qwad,    0,       self, or);     /* 6 */
  copy_to   (temp, qwad,    0,       self, xor);    /* 7 */
  copy_to   (self, 0,       0,       temp, copy);   /* 8 */
  copy_from (temp, qwad,    qwad,    self, xor);    /* 9 */
  copy_to   (mask, 0,       0,       temp, and);    /* A */
  copy_to   (temp, 0,       0,       self, xor);    /* B */
  copy_to   (temp, qwad,    qwad,    self, xor);    /* C */
  copy_from (mask, qwad>>1, qwad>>1, mask, and);    /* D */
  copy_to   (mask, qwad,    0,       mask, or);     /* E */
  copy_to   (mask, 0,       qwad,    mask, or);     /* F */
  display   (st, st->self);

  st->qwad >>= 1;
  if (st->qwad == 0)  /* done with this round */
    {
      st->qwad = -1;
      this_delay = st->delay2;
    }

  return this_delay;
}


static int 
to_pow2(struct state *st, int n, Bool up)
{
  int powers_of_2[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
			2048, 4096, 8192, 16384, 32768, 65536 };
  int i = 0;
  if (n > 65536) st->size = 65536;
  while (n >= powers_of_2[i]) i++;
  if (n == powers_of_2[i-1])
    return n;
  else
    return powers_of_2[up ? i : i-1];
}

static void *
blitspin_init (Display *d_arg, Window w_arg)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  char *bitmap_name;

  st->dpy = d_arg;
  st->window = w_arg;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->fg = get_pixel_resource (st->dpy, st->xgwa.colormap,
                               "foreground", "Foreground");
  st->bg = get_pixel_resource (st->dpy, st->xgwa.colormap,
                               "background", "Background");
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer");
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  if (st->delay < 0) st->delay = 0;
  if (st->delay2 < 0) st->delay2 = 0;
  if (st->duration < 1) st->duration = 1;

  st->start_time = time ((time_t) 0);

  bitmap_name = get_string_resource (st->dpy, "bitmap", "Bitmap");
  if (! bitmap_name || !*bitmap_name)
    bitmap_name = "(default)";

  if (!strcasecmp (bitmap_name, "(default)") ||
      !strcasecmp (bitmap_name, "default"))
    bitmap_name = "(screen)";

  if (!strcasecmp (bitmap_name, "(builtin)") ||
      !strcasecmp (bitmap_name, "builtin"))
    {
      st->width = som_width;
      st->height = som_height;
      st->bitmap = XCreatePixmapFromBitmapData (st->dpy, st->window,
                                                (char *) som_bits,
                                                st->width, st->height, 
                                                st->fg, st->bg, 
                                                st->xgwa.depth);
      st->scale_up = True; /* definitely. */
      st->loaded_p = True;
      blitspin_init_2 (st);
    }
  else if (!strcasecmp (bitmap_name, "(screen)") ||
           !strcasecmp (bitmap_name, "screen"))
    {
      st->bitmap = XCreatePixmap (st->dpy, st->window, 
                                  st->xgwa.width, st->xgwa.height,
                                  st->xgwa.depth);
      st->width = st->xgwa.width;
      st->height = st->xgwa.height;
      st->scale_up = True; /* maybe? */
      st->load_ext_p = True;
      st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                            st->bitmap, 0, 0);
    }
  else
    {
      st->bitmap = xpm_file_to_pixmap (st->dpy, st->window, bitmap_name,
                                   &st->width, &st->height, 0);
      st->scale_up = True; /* probably? */
      blitspin_init_2 (st);
    }

  return st;
}


static void
blitspin_init_2 (struct state *st)
{
  XGCValues gcv;

  /* make it square */
  st->size = (st->width < st->height) ? st->height : st->width;
  st->size = to_pow2(st, st->size, st->scale_up); /* round up to power of 2 */
  {						/* don't exceed screen size */
    int s = XScreenNumberOfScreen(st->xgwa.screen);
    int w = to_pow2(st, XDisplayWidth(st->dpy, s), False);
    int h = to_pow2(st, XDisplayHeight(st->dpy, s), False);
    if (st->size > w) st->size = w;
    if (st->size > h) st->size = h;
  }

  if (st->self) XFreePixmap (st->dpy, st->self);
  if (st->temp) XFreePixmap (st->dpy, st->temp);
  if (st->mask) XFreePixmap (st->dpy, st->mask);

  st->self = XCreatePixmap (st->dpy, st->window, st->size, st->size, 
                            st->xgwa.depth);
  st->temp = XCreatePixmap (st->dpy, st->window, st->size, st->size, 
                            st->xgwa.depth);
  st->mask = XCreatePixmap (st->dpy, st->window, st->size, st->size, 
                            st->xgwa.depth);
  gcv.foreground = (st->xgwa.depth == 1 ? 1 : (~0));

# ifdef USE_XCOPYAREA
#  define make_gc(op) \
    gcv.function=GX##op; \
    if (st->gc_##op) XFreeGC (st->dpy, st->gc_##op); \
    st->gc_##op = XCreateGC (st->dpy, st->self, GCFunction|GCForeground, &gcv)
  make_gc(set);
  make_gc(clear);
  make_gc(copy);
  make_gc(and);
  make_gc(or);
  make_gc(xor);
# endif /* USE_XCOPYAREA */

  gcv.foreground = gcv.background = st->bg;
  if (st->gc) XFreeGC (st->dpy, st->gc);
  st->gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);
  /* Clear st->self to the background color (not to 0, which 'clear' does.) */
  XFillRectangle (st->dpy, st->self, st->gc, 0, 0, st->size, st->size);
  XSetForeground (st->dpy, st->gc, st->fg);

  XCopyArea (st->dpy, st->bitmap, st->self, st->gc, 0, 0, 
             st->width, st->height,
	     (st->size - st->width)  >> 1,
             (st->size - st->height) >> 1);

  st->qwad = -1;
  st->first_time = 1;
}

static void
display (struct state *st, Pixmap pixmap)
{
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  if (st->xgwa.width != st->last_w || 
      st->xgwa.height != st->last_h)
    {
      XClearWindow (st->dpy, st->window);
      st->last_w = st->xgwa.width;
      st->last_h = st->xgwa.height;
    }
  if (st->xgwa.depth != 1)
    XCopyArea (st->dpy, pixmap, st->window, st->gc, 0, 0, st->size, st->size,
	       (st->xgwa.width - st->size) >> 1,
               (st->xgwa.height - st->size) >> 1);
  else
    XCopyPlane (st->dpy, pixmap, st->window, st->gc, 0, 0, st->size, st->size,
		(st->xgwa.width - st->size) >> 1,
                (st->xgwa.height - st->size) >> 1,
                1);
/*
  XDrawRectangle (st->dpy, st->window, st->gc,
		  ((st->xgwa.width - st->size) >> 1) - 1,
                  ((st->xgwa.height - st->size) >> 1) - 1,
		  st->size+2, st->size+2);
*/
}

static void
blitspin_reshape (Display *dpy, Window window, void *closure, 
                  unsigned int w, unsigned int h)
{
}

static Bool
blitspin_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;
      return True;
    }
  return False;
}

static void
blitspin_free (Display *dpy, Window window, void *closure)
{
}


static const char *blitspin_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  ".fpsSolid:	true",
  "*delay:	500000",
  "*delay2:	500000",
  "*duration:	120",
  "*bitmap:	(default)",
  "*geometry:	1080x1080",
#ifdef USE_IPHONE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec blitspin_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-duration",	".duration",	XrmoptionSepArg, 0 },
  { "-bitmap",		".bitmap",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("BlitSpin", blitspin)
