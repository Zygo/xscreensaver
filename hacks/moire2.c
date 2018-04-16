/* xscreensaver, Copyright (c) 1997-2013 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

struct state {
  Display *dpy;
  Window window;

  int ncolors;
  XColor *colors;
  int fg_pixel, bg_pixel;
  Pixmap p0, p1, p2, p3;
  GC copy_gc, erase_gc, window_gc;
  int width, height, size;
  int x1, x2, y1, y2, x3, y3;
  int dx1, dx2, dx3, dy1, dy2, dy3;
  int othickness, thickness;
  Bool do_three;
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer back_buf;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  Bool flip_a, flip_b;
  int pix;
  int delay, color_shift;

  int reset;
  int iterations, iteration;
};

static void
moire2_init_1 (struct state *st)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->othickness = get_integer_resource(st->dpy, "thickness", "Thickness");

  if (mono_p)
    st->ncolors = 2;
  else
    st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  if (st->ncolors < 2) st->ncolors = 2;
  if (st->ncolors <= 2) mono_p = True;

  if (mono_p)
    st->colors = 0;
  else
    st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));

  if (mono_p)
    ;
  else
    make_smooth_colormap (xgwa.screen, xgwa.visual, xgwa.colormap,
                          st->colors, &st->ncolors,
			  True, 0, True);

  st->bg_pixel = get_pixel_resource(st->dpy,
				xgwa.colormap, "background", "Background");
  st->fg_pixel = get_pixel_resource(st->dpy,
				xgwa.colormap, "foreground", "Foreground");

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  st->back_buf = xdbe_get_backbuffer (st->dpy, st->window, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
}


static void
reset_moire2 (struct state *st)
{
  GC gc;
  XWindowAttributes xgwa;
  XGCValues gcv;
  Bool xor;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->do_three = (0 == (random() % 3));

  st->width = xgwa.width;
  st->height = xgwa.height;
  st->size = st->width > st->height ? st->width : st->height;

  if (st->p0) XFreePixmap(st->dpy, st->p0);
  if (st->p1) XFreePixmap(st->dpy, st->p1);
  if (st->p2) XFreePixmap(st->dpy, st->p2);
  if (st->p3) XFreePixmap(st->dpy, st->p3);

  st->p0 = XCreatePixmap(st->dpy, st->window, st->width, st->height, 1);
  st->p1 = XCreatePixmap(st->dpy, st->window, st->width*2, st->height*2, 1);
  st->p2 = XCreatePixmap(st->dpy, st->window, st->width*2, st->height*2, 1);
  if (st->do_three)
    st->p3 = XCreatePixmap(st->dpy, st->window, st->width*2, st->height*2, 1);
  else
    st->p3 = 0;

  st->thickness = (st->othickness > 0 ? st->othickness : (1 + (random() % 4)));

  gcv.foreground = 0;
  gcv.line_width = (st->thickness == 1 ? 0 : st->thickness);
  gc = XCreateGC (st->dpy, st->p1, GCForeground|GCLineWidth, &gcv);

  XFillRectangle(st->dpy, st->p1, gc, 0, 0, st->width*2, st->height*2);
  XFillRectangle(st->dpy, st->p2, gc, 0, 0, st->width*2, st->height*2);
  if (st->do_three)
    XFillRectangle(st->dpy, st->p3, gc, 0, 0, st->width*2, st->height*2);

  XSetForeground(st->dpy, gc, 1);

  xor = (st->do_three || (st->thickness == 1) || (random() & 1));

  {
    int i, ii, maxx, maxy;

#define FROB(P) do { \
    maxx = (st->size*4); \
    maxy = (st->size*4); \
    if (0 == (random() % 5)) { \
	float f = 1.0 + frand(0.05); \
	if (random() & 1) maxx *= f; \
	else maxy *= f; \
      } \
    ii = (st->thickness + 1 + (xor ? 0 : 1) + (random() % (4 * st->thickness)));  \
    for (i = 0; i < (st->size*2); i += ii)  \
      XDrawArc(st->dpy, (P), gc, i-st->size, i-st->size, maxx-i-i, maxy-i-i, 0, 360*64); \
    if (0 == (random() % 5)) \
      { \
	XSetFunction(st->dpy, gc, GXxor); \
	XFillRectangle(st->dpy, (P), gc, 0, 0, st->width*2, st->height*2); \
	XSetFunction(st->dpy, gc, GXcopy); \
      } \
    } while(0)

    FROB(st->p1);
    FROB(st->p2);
    if (st->do_three)
      FROB(st->p3);
#undef FROB
  }

  XFreeGC(st->dpy, gc);

  if (st->copy_gc) XFreeGC(st->dpy, st->copy_gc);
  gcv.function = (xor ? GXxor : GXor);
  gcv.foreground = 1;
  gcv.background = 0;

  st->copy_gc = XCreateGC (st->dpy, st->p0, GCFunction|GCForeground|GCBackground, &gcv);

  gcv.foreground = 0;
  if (st->erase_gc) XFreeGC(st->dpy, st->erase_gc);
  st->erase_gc = XCreateGC (st->dpy, st->p0, GCForeground, &gcv);

  gcv.foreground = st->fg_pixel;
  gcv.background = st->bg_pixel;
  if (st->window_gc) XFreeGC(st->dpy, st->window_gc);
  st->window_gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);

#define FROB(N,DN,MAX) \
  N = (MAX/2) + (random() % MAX); \
  DN = ((1 + (random() % (7*st->thickness))) * ((random() & 1) ? 1 : -1))

  FROB(st->x1,st->dx1,st->width);
  FROB(st->x2,st->dx2,st->width);
  FROB(st->x3,st->dx3,st->width);
  FROB(st->y1,st->dy1,st->height);
  FROB(st->y2,st->dy2,st->height);
  FROB(st->y3,st->dy3,st->height);
#undef FROB
}



static void
moire2 (struct state *st)
{
#define FROB(N,DN,MAX) \
  N += DN; \
  if (N <= 0) N = 0, DN = -DN; \
  else if (N >= MAX) N = MAX, DN = -DN; \
  else if (0 == (random() % 100)) DN = -DN; \
  else if (0 == (random() % 50)) \
    DN += (DN <= -20 ? 1 : (DN >= 20 ? -1 : ((random() & 1) ? 1 : -1)))

  FROB(st->x1,st->dx1,st->width);
  FROB(st->x2,st->dx2,st->width);
  FROB(st->x3,st->dx3,st->width);
  FROB(st->y1,st->dy1,st->height);
  FROB(st->y2,st->dy2,st->height);
  FROB(st->y3,st->dy3,st->height);
#undef FROB

  XFillRectangle(st->dpy, st->p0, st->erase_gc, 0, 0, st->width, st->height);
  XCopyArea(st->dpy, st->p1, st->p0, st->copy_gc, st->x1, st->y1, st->width, st->height, 0, 0);
  XCopyArea(st->dpy, st->p2, st->p0, st->copy_gc, st->x2, st->y2, st->width, st->height, 0, 0);
  if (st->do_three)
    XCopyArea(st->dpy, st->p3, st->p0, st->copy_gc, st->x3, st->y3, st->width, st->height, 0, 0);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->back_buf)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = st->window;
      info[0].swap_action = XdbeUndefined;
      XCopyPlane (st->dpy, st->p0, st->back_buf, st->window_gc, 0, 0, st->width, st->height, 0, 0, 1L);
      XdbeSwapBuffers (st->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    XCopyPlane (st->dpy, st->p0, st->window, st->window_gc, 0, 0, st->width, st->height, 0, 0, 1L);

#if 0
  XCopyPlane(st->dpy, st->p1, st->window, st->window_gc, (st->width*2)/3, (st->height*2)/3,
	     st->width/2, st->height/2,
	     0, st->height/2, 1L);
  XCopyPlane(st->dpy, st->p2, st->window, st->window_gc, (st->width*2)/3, (st->height*2)/3,
	     st->width/2, st->height/2,
	     st->width/2, st->height/2, 1L);
#endif
}



static void *
moire2_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  st->reset = 1;

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->color_shift = get_integer_resource (st->dpy, "colorShift", "Integer");

  if (st->color_shift <= 0) st->color_shift = 1;
  moire2_init_1 (st);
  return st;
}

static unsigned long
moire2_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->reset)
    {
      st->reset = 0;

      st->iteration = 0;
      st->iterations = 30 + (random() % 70) + (random() % 70);
      reset_moire2 (st);

      st->flip_a = mono_p ? False : (random() & 1);
      st->flip_b = mono_p ? False : (random() & 1);

      if (st->flip_b)
        {
          XSetForeground(st->dpy, st->window_gc, st->bg_pixel);
          XSetBackground(st->dpy, st->window_gc, st->fg_pixel);
        }
      else
        {
          XSetForeground(st->dpy, st->window_gc, st->fg_pixel);
          XSetBackground(st->dpy, st->window_gc, st->bg_pixel);
        }
    }

  if (!mono_p)
    {
      st->pix++;
      st->pix = st->pix % st->ncolors;

      if (st->flip_a)
        XSetBackground(st->dpy, st->window_gc, st->colors[st->pix].pixel);
      else
        XSetForeground(st->dpy, st->window_gc, st->colors[st->pix].pixel);
    }


  moire2 (st);
  st->iteration++;
  if (st->iteration >= st->color_shift) 
    {
      st->iteration = 0;
      st->iterations--;
      if (st->iterations <= 0)
        st->reset = 1;
    }

  return st->delay;
}

static void
moire2_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->reset = 1;
}

static Bool
moire2_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
moire2_free (Display *dpy, Window window, void *closure)
{
}

static const char *moire2_defaults [] = {
  ".lowrez:		true",
  ".background:		black",
  ".foreground:		white",
  "*delay:		50000",
  "*thickness:		0",
  "*colors:		150",
  "*colorShift:		5",
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  /* Off by default, since it slows it down a lot, and the flicker isn't really
     all that bad without it... Or rather, it flickers just as badly with it.
     The XFree86 implementation of the XDBE extension totally blows!  There is
     just *no* excuse for the "swap buffers" operation to flicker like it does.
   */
  "*useDBE:      	False",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  0
};

static XrmOptionDescRec moire2_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".colors",	XrmoptionSepArg, 0 },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Moire2", moire2)
