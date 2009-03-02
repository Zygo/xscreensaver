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

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  int width, height, size;
  Bool scale_up;
  Pixmap self, temp, mask;
  GC SET, CLR, CPY, IOR, AND, XOR;
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
  async_load_state *img_loader;
};

static void display (struct state *, Pixmap);
static void blitspin_init_2 (struct state *);

#define copy_all_to(from, xoff, yoff, to, gc)			\
  XCopyArea (st->dpy, (from), (to), (gc), 0, 0,			\
	     st->size-(xoff), st->size-(yoff), (xoff), (yoff))

#define copy_all_from(to, xoff, yoff, from, gc)			\
  XCopyArea (st->dpy, (from), (to), (gc), (xoff), (yoff),	\
	     st->size-(xoff), st->size-(yoff), 0, 0)

static unsigned long
blitspin_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);

      if (!st->img_loader) { /* just finished */
        st->first_time = 0;
        st->loaded_p = False;
        st->qwad = -1;
        st->start_time = time ((time_t) 0);
      }

      return this_delay;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < time ((time_t) 0)) {
    /* Start it loading, but keep rotating the old image until it arrives. */
    st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                              st->bitmap, 0, 0);
  }

  if (! st->loaded_p) {
    blitspin_init_2 (st);
    st->loaded_p = True;
  }

  if (st->qwad == -1) 
    {
      XFillRectangle (st->dpy, st->mask, st->CLR, 0, 0, st->size, st->size);
      XFillRectangle (st->dpy, st->mask, st->SET, 0, 0, st->size>>1, st->size>>1);
      st->qwad = st->size>>1;
    }

  if (st->first_time)
    {
      st->first_time = 0;
      display (st, st->self);
      return st->delay2;
    }

  /* for (st->qwad = st->size>>1; st->qwad > 0; st->qwad>>=1) */

  copy_all_to  (st->mask, 0,        0,        st->temp, st->CPY);  /* 1 */
  copy_all_to  (st->mask, 0,        st->qwad, st->temp, st->IOR);  /* 2 */
  copy_all_to  (st->self, 0,        0,        st->temp, st->AND);  /* 3 */
  copy_all_to  (st->temp, 0,        0,        st->self, st->XOR);  /* 4 */
  copy_all_from(st->temp, st->qwad, 0,        st->self, st->XOR);  /* 5 */
  copy_all_from(st->self, st->qwad, 0,        st->self, st->IOR);  /* 6 */
  copy_all_to  (st->temp, st->qwad, 0,        st->self, st->XOR);  /* 7 */
  copy_all_to  (st->self, 0,        0,        st->temp, st->CPY);  /* 8 */
  copy_all_from(st->temp, st->qwad,           st->qwad, st->self,st->XOR);/*9*/
  copy_all_to  (st->mask, 0,        0,        st->temp, st->AND);  /* A */
  copy_all_to  (st->temp, 0,        0,        st->self, st->XOR);  /* B */
  copy_all_to  (st->temp, st->qwad, st->qwad, st->self, st->XOR);  /* C */
  copy_all_from(st->mask, st->qwad>>1,st->qwad>>1,st->mask,st->AND); /* D */
  copy_all_to  (st->mask, st->qwad, 0,        st->mask, st->IOR);  /* E */
  copy_all_to  (st->mask, 0,        st->qwad, st->mask, st->IOR);  /* F */
  display (st, st->self);

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
  /* sizeof(Dimension) == 2. */
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
# ifdef HAVE_COCOA
    /* I don't understand why it doesn't work with color images on OSX.
       I guess "kCGBlendModeDarken" is not close enough to being "GXand".
     */
    bitmap_name = "(builtin)";
# else
    bitmap_name = "(screen)";
# endif

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
      st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                            st->bitmap, 0, 0);
    }
  else
    {
      st->bitmap = xpm_file_to_pixmap (st->dpy, st->window, bitmap_name,
                                   &st->width, &st->height, 0);
      st->scale_up = True; /* probably? */
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

  st->self = XCreatePixmap (st->dpy, st->window, st->size, st->size, st->xgwa.depth);
  st->temp = XCreatePixmap (st->dpy, st->window, st->size, st->size, st->xgwa.depth);
  st->mask = XCreatePixmap (st->dpy, st->window, st->size, st->size, st->xgwa.depth);
  gcv.foreground = (st->xgwa.depth == 1 ? 1 : (~0));
  gcv.function=GXset;  st->SET = XCreateGC(st->dpy,st->self,GCFunction|GCForeground,&gcv);
  gcv.function=GXclear;st->CLR = XCreateGC(st->dpy,st->self,GCFunction|GCForeground,&gcv);
  gcv.function=GXcopy; st->CPY = XCreateGC(st->dpy,st->self,GCFunction|GCForeground,&gcv);
  gcv.function=GXor;   st->IOR = XCreateGC(st->dpy,st->self,GCFunction|GCForeground,&gcv);
  gcv.function=GXand;  st->AND = XCreateGC(st->dpy,st->self,GCFunction|GCForeground,&gcv);
  gcv.function=GXxor;  st->XOR = XCreateGC(st->dpy,st->self,GCFunction|GCForeground,&gcv);

  gcv.foreground = gcv.background = st->bg;
  st->gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);
  /* Clear st->self to the background color (not to 0, which st->CLR does.) */
  XFillRectangle (st->dpy, st->self, st->gc, 0, 0, st->size, st->size);
  XSetForeground (st->dpy, st->gc, st->fg);

#if 0
#ifdef HAVE_COCOA
  jwxyz_XSetAntiAliasing (st->dpy, st->gc,  False);
  jwxyz_XSetAntiAliasing (st->dpy, st->SET, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->CLR, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->CPY, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->IOR, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->AND, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->XOR, False);
#endif /* HAVE_COCOA */
#endif

  XCopyArea (st->dpy, st->bitmap, st->self, st->CPY, 0, 0, 
             st->width, st->height,
	     (st->size - st->width)  >> 1,
             (st->size - st->height) >> 1);
/*  XFreePixmap(st->dpy, st->bitmap);*/

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
  return False;
}

static void
blitspin_free (Display *dpy, Window window, void *closure)
{
}


static const char *blitspin_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*delay:	500000",
  "*delay2:	500000",
  "*duration:	120",
  "*bitmap:	(default)",
  "*geometry:	512x512",
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
