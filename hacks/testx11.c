/* testx11.c, Copyright (c) 2015 Dave Odell <dmo2118@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * This is a test for X11 drawing primitives for the non-X11 ports of
 * XScreenSaver. It shouldn't normally be installed with the other screenhacks.
 *
 * Almost no error checking in here. This is intentional.
 */

#include "screenhack.h"

#include <assert.h>
#include <errno.h>

#ifndef HAVE_COCOA
# define jwxyz_XSetAntiAliasing(dpy, gc, p)
#endif

#ifndef jwxyz_assert_display
# define jwxyz_assert_display(dpy)
#endif

#define countof(a) (sizeof(a) / sizeof(*(a)))

static const char *testx11_defaults [] = {
  ".background: #a020f0", /* purple */
  ".foreground: white",
  0
};

static XrmOptionDescRec testx11_options [] = {
  { 0, 0, 0, 0 }
};

#if defined HAVE_COCOA || defined HAVE_ANDROID
# define HAVE_JWXYZ 1
#endif

enum
{
  mode_welcome,
  mode_primitives,
  mode_images,
  mode_copy,

  mode_count
};

struct testx11 {
  Display *dpy;
  Window win;

  XWindowAttributes xgwa;

# ifndef HAVE_JWXYZ
  Pixmap backbuffer;
# endif

  unsigned mode;

  unsigned long rgb[3], salmon, magenta, gray50, dark_slate_gray1, cyan;

  Pixmap backdrop_tile, clip_mask_tile, backdrop_scratch;
  XColor backdrop_colors[64];
  int backdrop_ncolors;
  unsigned frame;

  GC copy_gc, black_gc, mono_gc, thick_line_gc, xor_gc, point_gc;
  Bool anti_alias_p;

  Pixmap mini_pix, pix64;
};


static void
abort_no_mem (void)
{
  fprintf(stderr, "%s: %s\n", progname, strerror(ENOMEM));
  abort();
}

static void
check_no_mem (void *ptr)
{
  if(!ptr)
    abort_no_mem();
}


static unsigned long
alloc_color (struct testx11 *st,
             unsigned short r, unsigned short g, unsigned short b)
{
  XColor color;
  color.red = r;
  color.green = g;
  color.blue = b;
  color.flags = DoRed | DoGreen | DoBlue;
  XAllocColor (st->dpy, st->xgwa.colormap, &color);
  return color.pixel;
}

static void
create_backbuffer(struct testx11 *st)
{
# ifndef HAVE_JWXYZ
  st->backbuffer = XCreatePixmap (st->dpy, st->win,
                                  st->xgwa.width, st->xgwa.height,
                                  st->xgwa.depth);
# endif
}


static void
copy_test (Display *dpy, Drawable src, Drawable dst, GC gc, int x, int y,
           unsigned long cells)
{
  XCopyArea(dpy, src, dst, gc, 0, 0, 3, 2, x, y);

  {
    XImage *image = XGetImage(dpy, src, 0, 0, 3, 2, cells, ZPixmap);
    XPutImage(dpy, dst, gc, image, 0, 0, x, y + 2, 3, 2);
    XDestroyImage(image);
  }
}

static void
test_pattern (struct testx11 *st, Drawable d)
{
  unsigned x;
  for (x = 0; x != 3; ++x) {
    XSetForeground(st->dpy, st->point_gc, st->rgb[x]);
    XDrawPoint(st->dpy, d, st->point_gc, x, 0);
    XSetForeground(st->dpy, st->point_gc, st->rgb[2 - x]);
    XFillRectangle(st->dpy, d, st->point_gc, x, 1, 1, 1);
  }

  copy_test (st->dpy, d, d, st->point_gc, 0, 2,
             st->rgb[0] | st->rgb[1] | st->rgb[2]);
}

static void
mini_rect(struct testx11 *st, Drawable t, int x, int y)
{
  XFillRectangle(st->dpy, t, st->copy_gc, x, y, 2, 2);
}

static Bool
toggle_antialiasing (struct testx11 *st)
{
  st->anti_alias_p ^= True;
  jwxyz_XSetAntiAliasing(st->dpy, st->copy_gc, st->anti_alias_p);
  return True;
}

static const unsigned tile_size = 16;
static const unsigned tile_count = 8;

static void
make_clip_mask (struct testx11 *st)
{
  /* Activate this for extra Fun! */
# if 0
  /* This is kind of slow, but that's OK, because this only happens on int
     or during a resize.
   */
  unsigned w = st->xgwa.width, h = st->xgwa.height;
  unsigned x, y;
  Pixmap mask = XCreatePixmap (st->dpy, st->clip_mask_tile, w, h, 1);

  for (y = 0; y < h; y += 8) {
    for (x = 0; x < w; x += 8) {
      XCopyArea (st->dpy, st->clip_mask_tile, mask, st->mono_gc,
                 0, 0, 8, 8, x, y);
    }
  }

  XSetClipMask (st->dpy, st->xor_gc, mask);
  XFreePixmap (st->dpy, mask);
# endif
}


static void *
testx11_init (Display *dpy, Window win)
{
  static const char backdrop_pattern[] = {
    '\xff', '\x00', '\xfe', '\x01', '\xfc', '\x03', '\xf8', '\x07', /* Zig */
    '\xf0', '\x0f', '\xf8', '\x07', '\xfc', '\x03', '\xfe', '\x01', /* Zag */
    '\xff', '\x00', '\xfe', '\x01', '\xfc', '\x03', '\xf8', '\x07', /* etc. */
    '\xf0', '\x0f', '\xf8', '\x07', '\xfc', '\x03', '\xfe', '\x01',
    '\xff', '\x00', '\xfe', '\x01', '\xfc', '\x03', '\xf8', '\x07',
    '\xf0', '\x0f', '\xf8', '\x07', '\xfc', '\x03', '\xfe', '\x01',
    '\xff', '\x00', '\xfe', '\x01', '\xfc', '\x03', '\xf8', '\x07',
    '\xf0', '\x0f', '\xf8', '\x07', '\xfc', '\x03', '\xfe', '\x01',
  };

  static const char clip_bits[] = {
    '\x33', '\x33', '\xcc', '\xcc', '\x33', '\x33', '\xcc', '\xcc'
  };

  struct testx11 *st = (struct testx11 *) malloc (sizeof(*st));
  XGCValues gcv;

  check_no_mem (st);

  st->dpy = dpy;
  st->win = win;

  XGetWindowAttributes (dpy, win, &st->xgwa);

  create_backbuffer (st);

  st->rgb[0]           = alloc_color (st, 0xffff, 0x0000, 0x0000);
  st->rgb[1]           = alloc_color (st, 0x0000, 0xffff, 0x0000);
  st->rgb[2]           = alloc_color (st, 0x0000, 0x0000, 0xffff);
  st->salmon           = alloc_color (st, 0xffff, 0x7fff, 0x7fff);
  st->magenta          = alloc_color (st, 0xffff, 0x0000, 0xffff);
  st->gray50           = alloc_color (st, 0x8000, 0x8000, 0x8000);
  st->dark_slate_gray1 = alloc_color (st, 0x8000, 0xffff, 0xffff);
  st->cyan             = alloc_color (st, 0x0000, 0xffff, 0xffff);

  st->backdrop_tile = XCreatePixmapFromBitmapData (dpy, win,
                                                   (char *) backdrop_pattern,
                                                   tile_size, tile_size * 2,
                                                   1, 0, 1);

  st->clip_mask_tile = XCreatePixmapFromBitmapData (dpy, win,
                                                    (char *) clip_bits, 8, 8,
                                                    1, 0, 1);

  {
    unsigned s = tile_size * tile_count;
    st->backdrop_scratch = XCreatePixmap (dpy, win, s, s, st->xgwa.depth);
  }

  st->backdrop_ncolors = countof (st->backdrop_colors);
  make_color_loop (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                   180, 1, 0.5, 210, 1, 0.5, 240, 1, 0.5,
                   st->backdrop_colors, &st->backdrop_ncolors, True, NULL);

  st->frame = 0;

  st->mode = mode_welcome;

  gcv.function = GXcopy;
  gcv.foreground = st->cyan;
  gcv.background = st->magenta;
  gcv.line_width = 0;
  gcv.cap_style = CapRound;
  /* TODO: Real X11 uses fixed by default. */
  gcv.font = XLoadFont (dpy, "fixed");
  st->copy_gc = XCreateGC (dpy, win, GCFunction | GCForeground | GCBackground | GCLineWidth | GCCapStyle | GCFont, &gcv);

  gcv.foreground = BlackPixelOfScreen (st->xgwa.screen);
  st->black_gc = XCreateGC (dpy, win, GCForeground, &gcv);

  st->anti_alias_p = False;
  jwxyz_XSetAntiAliasing (dpy, st->copy_gc, False);

  gcv.foreground = alloc_color (st, 0x8000, 0x4000, 0xffff);
  gcv.line_width = 8;
  gcv.cap_style = CapProjecting;
  st->thick_line_gc = XCreateGC (dpy, win, GCForeground | GCLineWidth | GCCapStyle, &gcv);

  gcv.function = GXxor;
  st->xor_gc = XCreateGC (dpy, win, GCFunction, &gcv);

  st->point_gc = XCreateGC (dpy, win, 0, NULL);

  st->mono_gc = XCreateGC (dpy, st->clip_mask_tile, 0, &gcv);

  st->pix64 = XCreatePixmap(dpy, win, 64, 64, st->xgwa.depth);

  st->mini_pix = XCreatePixmap (dpy, win, 16, 16, st->xgwa.depth);

  {
    static const char text[] = "Welcome from testx11_init().";
    XClearWindow (dpy, win);
    XDrawString (dpy, win, st->copy_gc, 16, 16, text, countof (text) - 1);
  }

  make_clip_mask (st);

  jwxyz_assert_display (dpy);

  return st;
}


static void
backdrop (struct testx11 *st, Drawable t)
{
  unsigned w = st->xgwa.width, h = st->xgwa.height;
  unsigned x, y;

  for (y = 0; y != tile_count; ++y) {
    const float s0 = 2 * M_PI / tile_count;
    float y_fac = sin ((y + st->frame / 16.0) * s0);
    for (x = 0; x != tile_count; ++x) {
      unsigned c = ((sin ((x + st->frame / 8.0) * s0) * y_fac) - 1) / 2 * st->backdrop_ncolors / 2;
      c = (c + st->frame) % st->backdrop_ncolors;
      XSetBackground (st->dpy, st->black_gc, st->backdrop_colors[c].pixel);
      XCopyPlane (st->dpy, st->backdrop_tile, st->backdrop_scratch, st->black_gc,
                  0, st->frame % tile_size, tile_size, tile_size, x * tile_size, y * tile_size, 1);
    }
  }

  /* XFillRectangle (st->dpy, t, st->black_gc, 0, 0, w, h); */

  for (y = 0; y < h; y += tile_count * tile_size) {
    for (x = 0; x < w; x += tile_count * tile_size) {
      XCopyArea (st->dpy, st->backdrop_scratch, t, st->copy_gc, 0, 0,
                 tile_count * tile_size, tile_count * tile_size, x, y);
    }
  }
}

static const unsigned button_pad = 16;
static const unsigned button_size = 64;


static unsigned long
testx11_draw (Display *dpy, Window win, void *st_raw)
{
  struct testx11 *st = (struct testx11 *) st_raw;
  unsigned w = st->xgwa.width, h = st->xgwa.height;
# ifdef HAVE_JWXYZ
  Drawable t = win;
# else
  Drawable t =
    st->mode == mode_welcome ||
    st->mode == mode_images ||
    st->mode == mode_copy ? win : st->backbuffer;
# endif
  unsigned i;

  assert (dpy == st->dpy);
  assert (win == st->win);

  jwxyz_assert_display (dpy);

  switch (st->mode)
  {
  case mode_primitives:
    backdrop (st, t);

    XDrawPoint (dpy, t, st->thick_line_gc, 0, 0);
    XDrawPoint (dpy, t, st->thick_line_gc, 0, 1);
    XDrawPoint (dpy, t, st->thick_line_gc, 1, 0);
    XDrawPoint (dpy, t, st->thick_line_gc, 1, 1);

    mini_rect (st, t, 2, 0);
    mini_rect (st, t, 0, 2);
    mini_rect (st, t, 4, 2);
    mini_rect (st, t, 2, 4);

    mini_rect (st, t, 30, -2);
    mini_rect (st, t, 32, 0);

    mini_rect (st, t, 30, h - 2);
    mini_rect (st, t, 32, h);

    mini_rect (st, t, -2, 30);
    mini_rect (st, t, 0, 32);
    mini_rect (st, t, w - 2, 30);
    mini_rect (st, t, w, 32);

    mini_rect (st, t, w / 2 + 4, h / 2 + 4);

    XFillArc (dpy, t, st->copy_gc, 16, 16, 256, 256, 45 * 64, -135 * 64);
    /* XCopyArea(dpy, t, t, st->copy_gc, 48, 48, 128, 128, 64, 64); */

    /* Center */
    XDrawPoint (dpy, t, st->copy_gc, w / 2, h / 2);

    /* Top/bottom */
    XDrawPoint (dpy, t, st->copy_gc, w / 2, 0);
    XDrawPoint (dpy, t, st->copy_gc, w / 2, h - 1);

    XDrawPoint (dpy, t, st->copy_gc, 16, -1);
    XDrawPoint (dpy, t, st->copy_gc, 17, 0);

    XDrawPoint (dpy, t, st->copy_gc, 15, h - 2);
    XDrawPoint (dpy, t, st->copy_gc, 16, h - 1);
    XDrawPoint (dpy, t, st->copy_gc, 17, h);

    {
      XPoint *points = malloc (sizeof(XPoint) * h);
      XSegment *lines = malloc (sizeof(XSegment) * h);
      XPoint *points2 = malloc (sizeof(XPoint) * h);
      for(i = 0; i != h; ++i)
      {
        points[i].x = 8 + (i & 1);
        points[i].y = i;

        lines[i].x1 = 48 + (i & 1) * 4;
        lines[i].y1 = i;
        lines[i].x2 = 50;
        lines[i].y2 = i;

        points2[i].x = 24 + (i & 1);
        points2[i].y = i;
        /* XFillRectangle(st->dpy, t, st->copy_gc, 24 + (i & 1), i, 1, 1); */
      }

      XDrawPoints (dpy, t, st->copy_gc, points, h, CoordModeOrigin);
      XDrawSegments (dpy, t, st->copy_gc, lines, h);
      XDrawPoints (dpy, t, st->copy_gc, points2, h, CoordModeOrigin);
      free (lines);
      free (points);
      free (points2);
    }

    /* Left/right */
    XDrawPoint (dpy, t, st->copy_gc, -1, 16);
    XDrawPoint (dpy, t, st->copy_gc, 0, 17);
    XDrawPoint (dpy, t, st->copy_gc, w - 1, 16);
    XDrawPoint (dpy, t, st->copy_gc, w, 17);

    {
      XPoint *points = malloc(sizeof(XPoint) * w);
      XSegment *lines = malloc(sizeof(XSegment) * w);
      XPoint *points2 = malloc(sizeof(XPoint) * w);
      for(i = 0; i != w; ++i)
      {
        points[i].x = i;
        points[i].y = 8 + (i & 1);
        lines[i].x1 = i;
        lines[i].y1 = 48 + (i & 1) * 4;
        lines[i].x2 = i;
        lines[i].y2 = 50;

        points2[i].x = i;
        points2[i].y = 24 + (i & 1);
        /* XFillRectangle(dpy, t, st->copy_gc, i, 24 + (i & 1), 1, 1); */
      }

      XDrawPoints (dpy, t, st->copy_gc, points, w, CoordModeOrigin);
      XDrawSegments (dpy, t, st->copy_gc, lines, w);
      {
        /* Thick purple lines */
        XSegment seg[2] =
        {
          {31, 31, 31, 31}, /* TODO: This should not be rotated in Cocoa. */
          {42, 31, 42, 42}
        };
        XDrawSegments (dpy, t, st->thick_line_gc, seg, 2);

        XDrawLine (dpy, t, st->thick_line_gc, 58, 43, 64, 43);
        XDrawLine (dpy, t, st->thick_line_gc, 73, 43, 80, 43);
      }

      XDrawPoints (dpy, t, st->copy_gc, points2, w, CoordModeOrigin);
      free (points);
      free (points2);
      free (lines);
    }

    XDrawLine (dpy, t, st->copy_gc, 54, 11, 72, 22);

    {
      XPoint vertices[] = {{5, 5}, {5, 8}, {8, 8}, {8, 5}};
      XFillPolygon (dpy, t, st->copy_gc, vertices, 4, Convex, CoordModeOrigin);
    }

    {
      XDrawRectangle (dpy, t, st->copy_gc, 11, 11, 11, 11);
      XFillRectangle (dpy, t, st->copy_gc, 13, 13, 8, 8);
    }

    /* Several ~16 pixel boxes in a row. */

    /* Box 0 */
    {
      XDrawRectangle (dpy, t, st->copy_gc, 54, 54, 16, 16);
      XDrawRectangle (dpy, t, st->copy_gc, 55, 55, 14, 14);

      XDrawPoint (dpy, t, st->thick_line_gc, 56, 56);
      XDrawPoint (dpy, t, st->copy_gc, 57, 56);
    }

    /* Box 1 */
    XCopyArea (dpy, t, t, st->copy_gc, 55, 55, 15, 15, 72, 55);

    /* Box 2 */
    {
      XImage *image = XGetImage(st->dpy, t, 55, 55, 15, 15, 0xffffff, ZPixmap);
      XPutImage (dpy, t, st->copy_gc, image, 0, 0, 88, 55, 15, 15);
      XDestroyImage(image);
    }

    /* Box 3 */

    {
      XCopyArea (dpy, t, st->mini_pix, st->copy_gc, 104, 55, 16, 16, 0, 0);
      /* XCopyArea (dpy, t, st->mini_pix, st->copy_gc, 105, 56, 14, 14, 1, 1);
         XCopyArea (dpy, t, st->mini_pix, st->copy_gc, 1, 1, 14, 14, 1, 1);
       */

      /* This point gets hidden. */
      XDrawPoint (dpy, t, st->copy_gc, 104 + 8, 55 + 8);

      XDrawPoint (dpy, st->mini_pix, st->copy_gc, 0, 0);
      XDrawPoint (dpy, st->mini_pix, st->copy_gc, 15, 15);
      XDrawRectangle (dpy, st->mini_pix, st->copy_gc, 1, 1, 13, 13);
      XCopyArea (dpy, st->mini_pix, t, st->copy_gc, 0, 0, 16, 16, 104, 55);
    }

    {
      XDrawLine (dpy, t, st->copy_gc, 11, 28, 22, 28);
      XDrawLine (dpy, t, st->copy_gc, 12, 27, 12, 46);
      XDrawLine (dpy, t, st->copy_gc, 14, 30, 14, 30);
    }

    XDrawArc (dpy, t, st->copy_gc, 27, 11, 19, 11, 0, 360 * 64);

    XDrawRectangle (dpy, t, st->copy_gc, 54, 73, 64, 64);
    XFillArc (dpy, t, st->copy_gc, 56, 75, 60, 60, 0, 360 * 64);
    /* XDrawArc (dpy, t, st->thick_line_gc, 56, 75, 60, 60, 0, 360 * 64); */

    XClearArea (dpy, win, 121, 55, 16, 16, False);
    break;

  case mode_images:
    backdrop (st, t);

    /* if(w >= 9 && h >= 10) */
    {
      Screen *screen = st->xgwa.screen;
      Visual *visual = st->xgwa.visual;
      Pixmap pixmap = XCreatePixmap (dpy, t, 3, 10,
                                     visual_depth (screen, visual));
      unsigned long cells = cells = st->rgb[0] | st->rgb[1] | st->rgb[2];

      {
        XSetForeground (dpy, st->point_gc, st->salmon);
        XDrawPoint(dpy, t, st->point_gc, 0, h - 1);
      }

      test_pattern (st, t);
      test_pattern (st, pixmap);
      /* Here's a good spot to verify that the pixmap contains the right colors
         at the top.
       */
      copy_test (dpy, t, pixmap, st->copy_gc, 0, 6, cells);

      XCopyArea (dpy, pixmap, t, st->copy_gc, 0, 0, 3, 10, 3, 0);
      {
        XImage *image = XGetImage (dpy, pixmap, 0, 0, 3, 10, cells, ZPixmap);
        XPutImage (dpy, t, st->copy_gc, image, 0, 0, 6, 0, 3, 10);
        XDestroyImage (image);
      }

      XFreePixmap (dpy, pixmap);
      XSync (dpy, False);
    }
    break;

  case mode_copy:
    backdrop (st, win);

    /* X.org isn't making a whole lot of sense here. */

    Bool use_copy = (st->frame / 20) & 1;

    {
      GC gc = use_copy ? st->copy_gc : st->xor_gc;

      XSetWindowBackground (dpy, t, st->magenta);
      XCopyArea (dpy, t, t, gc, -2, -2, 40, 40, 20, 20);

      if (!use_copy)
        XCopyArea (st->dpy, t, t, gc, -20, h - 20, 40, 40, 20, h - 60);
      XCopyArea (dpy, t, t, gc, w - 38, h - 38, 40, 40, w - 60, h - 60);
      XCopyArea (dpy, t, t, gc, w - 20, -20, 40, 40, w - 60, 20);

      XSetWindowBackground (dpy, t, st->gray50);
      XCopyArea (st->dpy, t, t, gc, -20, 64, 40, 40, 20, 64);

      XSetWindowBackground (dpy, t, st->dark_slate_gray1);
      XCopyArea (st->dpy, t, t, gc, -20, 112, 40, 40, 20, 112);
    }

    if (use_copy)
    {
      GC gc = st->copy_gc;
      XCopyArea (st->dpy, t, st->pix64, gc, 0, h - 64, 64, 64, 0, 0);

      XSetForeground (st->dpy, st->xor_gc, st->rgb[1]);
      XSetBackground (st->dpy, st->xor_gc, st->cyan);

      /* XCopyArea (st->dpy, st->pix64, st->pix64, gc, 32, 32, 64, 64, 0, 0);
         XCopyArea (st->dpy, st->pix64, t, gc, 0, 0, 64, 64, 4, h - 68);
       */
      XCopyArea (st->dpy, st->pix64, t, gc, 32, 32, 128, 64, 0, h - 64);
    }

    break;
  }

  for (i = 1; i != mode_count; ++i) {
    unsigned i0 = i - 1;
    char str[32];
    XRectangle button_dims;
    button_dims.x = i0 * (button_pad + button_size) + button_pad;
    button_dims.y = h - button_pad - button_size;
    button_dims.width = button_size;
    button_dims.height = button_size;
    if (!st->mode)
      XFillRectangles (dpy, t, st->black_gc, &button_dims, 1);
    XDrawRectangle (dpy, t, st->copy_gc, button_dims.x, button_dims.y,
                    button_dims.width, button_dims.height);

    XDrawString (dpy, t, st->copy_gc,
                 button_dims.x + button_size / 2 - 3,
                 h - button_pad - button_size / 2 + 13 / 2,
                 str, sprintf(str, "%u", i));
  }

  if (t != win)
    XCopyArea (dpy, t, win, st->copy_gc, 0, 0, w, h, 0, 0);

  ++st->frame;
  return 1000000 / 20;
}

static void
testx11_reshape (Display *dpy, Window window, void *st_raw,
                 unsigned int w, unsigned int h)
{
  struct testx11 *st = (struct testx11 *)st_raw;
  st->xgwa.width = w;
  st->xgwa.height = h;
# ifndef HAVE_JWXYZ
  XFreePixmap (st->dpy, st->backbuffer);
# endif
  create_backbuffer (st);
  make_clip_mask (st);
}

static Bool
testx11_event (Display *dpy, Window window, void *st_raw, XEvent *event)
{
  struct testx11 *st = (struct testx11 *) st_raw;

  Bool handled = False;

  switch (event->xany.type)
  {
  case KeyPress:
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ')
        handled = toggle_antialiasing (st);

      if (c >= '0' && c <= '9' && c < '0' + mode_count) {
        st->mode = c - '0';
        handled = True;
      }
    }
    break;

  case ButtonPress:
    if (event->xbutton.y >= st->xgwa.height - button_pad * 2 - button_size) {
      int i = (event->xbutton.x - button_pad / 2) / (button_pad + button_size) + 1;
      if (i && i < mode_count) {
        st->mode = i;
        handled = True;
      }
    }

    if (!handled)
      handled = toggle_antialiasing (st);
    break;
  }

  return handled;
}

static void
testx11_free (Display *dpy, Window window, void *st_raw)
{
  /* Omitted for the sake of brevity. */
}

XSCREENSAVER_MODULE_2 ("TestX11", testx11, testx11)
