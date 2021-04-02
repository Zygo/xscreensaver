/* xscreensaver, Copyright (c) 1991-2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* JWXYZ Is Not Xlib.

   Pixmaps implemented in CPU RAM, for Android OpenGL hacks.
   Renders into an XImage, basically.

   See the comment at the top of jwxyz-common.c for an explanation of
   the division of labor between these various modules.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef JWXYZ_IMAGE /* entire file */

#include "jwxyzI.h"
#include "jwxyz.h"
#include "jwxyz-timers.h"
#include "pow2.h"

#include <wchar.h>


union color_bytes { // Hello, again.
  uint32_t pixel;
  uint8_t bytes[4];
};

struct jwxyz_Display {
  const struct jwxyz_vtbl *vtbl; // Must come first.

  Window main_window;
  Visual visual;
  struct jwxyz_sources_data *timers_data;

  unsigned long window_background;
};

struct jwxyz_GC {
  XGCValues gcv;
  unsigned int depth;
};


extern const struct jwxyz_vtbl image_vtbl;

Display *
jwxyz_image_make_display (Window w, const unsigned char *rgba_bytes)
{
  Display *d = (Display *) calloc (1, sizeof(*d));
  d->vtbl = &image_vtbl;

  Visual *v = &d->visual;
  v->class      = TrueColor;
  Assert (rgba_bytes[3] == 3, "alpha not last");
  unsigned long masks[4];
  for (unsigned i = 0; i != 4; ++i) {
    union color_bytes color;
    color.pixel = 0;
    color.bytes[rgba_bytes[i]] = 0xff;
    masks[i] = color.pixel;
  }
  v->red_mask   = masks[0];
  v->green_mask = masks[1];
  v->blue_mask  = masks[2];
  v->alpha_mask = masks[3];

  d->timers_data = jwxyz_sources_init (XtDisplayToApplicationContext (d));
  d->window_background = BlackPixel(d,0);
  d->main_window = w;

  return d;
}

void
jwxyz_image_free_display (Display *dpy)
{
  jwxyz_sources_free (dpy->timers_data);

  free (dpy);
}


static jwxyz_sources_data *
display_sources_data (Display *dpy)
{
  return dpy->timers_data;
}


static Window
root (Display *dpy)
{
  return dpy->main_window;
}

static Visual *
visual (Display *dpy)
{
  return &dpy->visual;
}


static void
next_point(short *v, XPoint p, int mode)
{
  switch (mode) {
    case CoordModeOrigin:
      v[0] = p.x;
      v[1] = p.y;
      break;
    case CoordModePrevious:
      v[0] += p.x;
      v[1] += p.y;
      break;
    default:
      Assert (False, "next_point: bad mode");
      break;
  }
}

#define SEEK_DRAWABLE(d, x, y) \
  SEEK_XY (jwxyz_image_data(d), jwxyz_image_pitch(d), x, y)

static int
DrawPoints (Display *dpy, Drawable d, GC gc,
            XPoint *points, int count, int mode)
{
  Assert (gc->gcv.function == GXcopy, "XDrawPoints: bad GC function");

  const XRectangle *frame = jwxyz_frame (d);
  short v[2] = {0, 0};
  for (unsigned i = 0; i < count; i++) {
    next_point(v, points[i], mode);
    if (v[0] >= 0 && v[0] < frame->width &&
        v[1] >= 0 && v[1] < frame->height)
      *SEEK_DRAWABLE(d, v[0], v[1]) = gc->gcv.foreground;
  }

  return 0;
}


static void
copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
           int src_x, int src_y, unsigned int width, unsigned int height,
           int dst_x, int dst_y)
{
  jwxyz_blit (jwxyz_image_data (src), jwxyz_image_pitch (src), src_x, src_y, 
              jwxyz_image_data (dst), jwxyz_image_pitch (dst), dst_x, dst_y, 
              width, height);
}


static void
draw_line (Drawable d, unsigned long pixel,
           short x0, short y0, short x1, short y1)
{
// TODO: Assert line_Width == 1, line_stipple == solid, etc.

  const XRectangle *frame = jwxyz_frame (d);
  if (x0 < 0 || x0 >= frame->width ||
      x1 < 0 || x1 >= frame->width ||
      y0 < 0 || y0 >= frame->height ||
      y1 < 0 || y1 >= frame->height) {
    Log ("draw_line: out of bounds");
    return;
  }

  int dx = abs(x1 - x0), dy = abs(y1 - y0);

  unsigned dmod0, dmod1;
  int dpx0, dpx1;
  if (dx > dy) {
    dmod0 = dy;
    dmod1 = dx;
    dpx0 = x1 > x0 ? 1 : -1;
    dpx1 = y1 > y0 ? frame->width : -frame->width;
  } else {
    dmod0 = dx;
    dmod1 = dy;
    dpx0 = y1 > y0 ? frame->width : -frame->width;
    dpx1 = x1 > x0 ? 1 : -1;
  }

  unsigned n = dmod1;
  unsigned mod = n;
  ++n;

  dmod0 <<= 1;
  dmod1 <<= 1;

  uint32_t *px = SEEK_DRAWABLE(d, x0, y0);

  for(; n; --n) {
    *px = pixel;

    mod += dmod0;
    if(mod > dmod1) {
      mod -= dmod1;
      px += dpx1;
    }

    px += dpx0;
  }
}

static int
DrawLines (Display *dpy, Drawable d, GC gc, XPoint *points, int count,
           int mode)
{
  short v[2] = {0, 0}, v_prev[2] = {0, 0};
  unsigned long pixel = gc->gcv.foreground;
  for (unsigned i = 0; i != count; ++i) {
    next_point(v, points[i], mode);
    if (i)
      draw_line (d, pixel, v_prev[0], v_prev[1], v[0], v[1]);
    v_prev[0] = v[0];
    v_prev[1] = v[1];
  }
  return 0;
}


static int
DrawSegments (Display *dpy, Drawable d, GC gc, XSegment *segments, int count)
{
  unsigned long pixel = gc->gcv.foreground;
  for (unsigned i = 0; i != count; ++i) {
    XSegment *seg = &segments[i];
    draw_line (d, pixel, seg->x1, seg->y1, seg->x2, seg->y2);
  }
  return 0;
}


static int
ClearWindow (Display *dpy, Window win)
{
  Assert (win == dpy->main_window, "not a window");
  const XRectangle *wr = jwxyz_frame (win);
  return XClearArea (dpy, win, 0, 0, wr->width, wr->height, 0);
}

static unsigned long *
window_background (Display *dpy)
{
  return &dpy->window_background;
}

static void
fill_rects (Display *dpy, Drawable d, GC gc,
            const XRectangle *rectangles, unsigned long nrectangles,
            unsigned long pixel)
{
  Assert (!gc || gc->gcv.function == GXcopy, "XDrawPoints: bad GC function");

  const XRectangle *frame = jwxyz_frame (d);
  void *image_data = jwxyz_image_data (d);
  ptrdiff_t image_pitch = jwxyz_image_pitch (d);

  for (unsigned i = 0; i != nrectangles; ++i) {
    const XRectangle *rect = &rectangles[i];
    unsigned x0 = rect->x >= 0 ? rect->x : 0, y0 = rect->y >= 0 ? rect->y : 0;
    int x1 = rect->x + rect->width, y1 = rect->y + rect->height;
    if (y1 > frame->height)
      y1 = frame->height;
    if (x1 > frame->width)
      x1 = frame->width;
    unsigned x_size = x1 - x0, y_size = y1 - y0;
    void *dst = SEEK_XY (image_data, image_pitch, x0, y0);
    while (y_size) {
# if __SIZEOF_WCHAR_T__ == 4
      wmemset (dst, (wchar_t) pixel, x_size);
# else
      for(size_t i = 0; i != x_size; ++i)
        ((uint32_t *)dst)[i] = pixel;
# endif
      --y_size;
      dst = (char *) dst + image_pitch;
    }
  }
}


static int
FillPolygon (Display *dpy, Drawable d, GC gc,
             XPoint *points, int npoints, int shape, int mode)
{
  Log ("XFillPolygon: not implemented");
  return 0;
}

static int
draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
                unsigned int width, unsigned int height,
                int angle1, int angle2, Bool fill_p)
{
  Log ("jwxyz_draw_arc: not implemented");
  return 0;
}


static XGCValues *
gc_gcv (GC gc)
{
  return &gc->gcv;
}


static unsigned int
gc_depth (GC gc)
{
  return gc->depth;
}


static GC
CreateGC (Display *dpy, Drawable d, unsigned long mask, XGCValues *xgcv)
{
  struct jwxyz_GC *gc = (struct jwxyz_GC *) calloc (1, sizeof(*gc));
  gc->depth = jwxyz_drawable_depth (d);

  jwxyz_gcv_defaults (dpy, &gc->gcv, gc->depth);
  XChangeGC (dpy, gc, mask, xgcv);
  return gc;
}


static int
FreeGC (Display *dpy, GC gc)
{
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);

  free (gc);
  return 0;
}


static int
PutImage (Display *dpy, Drawable d, GC gc, XImage *ximage,
          int src_x, int src_y, int dest_x, int dest_y,
          unsigned int w, unsigned int h)
{
  const XRectangle *wr = jwxyz_frame (d);

  Assert (gc, "no GC");
  Assert ((w < 65535), "improbably large width");
  Assert ((h < 65535), "improbably large height");
  Assert ((src_x  < 65535 && src_x  > -65535), "improbably large src_x");
  Assert ((src_y  < 65535 && src_y  > -65535), "improbably large src_y");
  Assert ((dest_x < 65535 && dest_x > -65535), "improbably large dest_x");
  Assert ((dest_y < 65535 && dest_y > -65535), "improbably large dest_y");

  // Clip width and height to the bounds of the Drawable
  //
  if (dest_x + w > wr->width) {
    if (dest_x > wr->width)
      return 0;
    w = wr->width - dest_x;
  }
  if (dest_y + h > wr->height) {
    if (dest_y > wr->height)
      return 0;
    h = wr->height - dest_y;
  }
  if (w <= 0 || h <= 0)
    return 0;

  // Clip width and height to the bounds of the XImage
  //
  if (src_x + w > ximage->width) {
    if (src_x > ximage->width)
      return 0;
    w = ximage->width - src_x;
  }
  if (src_y + h > ximage->height) {
    if (src_y > ximage->height)
      return 0;
    h = ximage->height - src_y;
  }
  if (w <= 0 || h <= 0)
    return 0;

  /* Assert (d->win */

  if (jwxyz_dumb_drawing_mode(dpy, d, gc, dest_x, dest_y, w, h))
    return 0;

  XGCValues *gcv = gc_gcv (gc);

  Assert (gcv->function == GXcopy, "XPutImage: bad GC function");
  Assert (!ximage->xoffset, "XPutImage: bad xoffset");

  ptrdiff_t
    src_pitch = ximage->bytes_per_line,
    dst_pitch = jwxyz_image_pitch (d);

  const void *src_ptr = SEEK_XY (ximage->data, src_pitch, src_x, src_y);
  void *dst_ptr = SEEK_XY (jwxyz_image_data (d), dst_pitch, dest_x, dest_y);

  if (gcv->alpha_allowed_p) {
    Assert (ximage->depth == 32, "XPutImage: depth != 32");
    Assert (ximage->format == ZPixmap, "XPutImage: bad format");
    Assert (ximage->bits_per_pixel == 32, "XPutImage: bad bits_per_pixel");

    const uint8_t *src_row = src_ptr;
    uint8_t *dst_row = dst_ptr;

    /* Slight loss of precision here: color values may end up being one less
       than what they should be.
     */
    while (h) {
      for (unsigned x = 0; x != w; ++x) {
        // Pixmaps don't contain alpha. (Yay.)
        const uint8_t *src = src_row + x * 4;
        uint8_t *dst = dst_row + x * 4;

        // ####: This is pretty SIMD friendly.
        // Protip: Align dst (load + store), let src be unaligned (load only)
        uint16_t alpha = src[3], alpha1 = 0xff - src[3];
        dst[0] = (src[0] * alpha + dst[0] * alpha1) >> 8;
        dst[1] = (src[1] * alpha + dst[1] * alpha1) >> 8;
        dst[2] = (src[2] * alpha + dst[2] * alpha1) >> 8;
      }

      src_row += src_pitch;
      dst_row += dst_pitch;
      --h;
    }
  } else {
    Assert (ximage->depth == 1 || ximage->depth == 32,
            "XPutImage: depth != 1 && depth != 32");

    if (ximage->depth == 32) {
      Assert (ximage->format == ZPixmap, "XPutImage: bad format");
      Assert (ximage->bits_per_pixel == 32, "XPutImage: bad bits_per_pixel");
      jwxyz_blit (ximage->data, ximage->bytes_per_line, src_x, src_y,
                  jwxyz_image_data (d), jwxyz_image_pitch (d), dest_x, dest_y,
                  w, h);
    } else {
      Log ("XPutImage: depth == 1");
    }
  }

  return 0;
}

static XImage *
GetSubImage (Display *dpy, Drawable d, int x, int y,
             unsigned int width, unsigned int height,
             unsigned long plane_mask, int format,
             XImage *dest_image, int dest_x, int dest_y)
{
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  Assert (dest_image->depth == 32 && jwxyz_drawable_depth (d) == 32,
          "XGetSubImage: bad depth");
  Assert (format == ZPixmap, "XGetSubImage: bad format");

  jwxyz_blit (jwxyz_image_data (d), jwxyz_image_pitch (d), x, y,
              dest_image->data, dest_image->bytes_per_line, dest_x, dest_y,
              width, height);

  return dest_image;
}


static int
SetClipMask (Display *dpy, GC gc, Pixmap m)
{
  Log ("TODO: No clip masks yet"); // Slip/colorbars.c needs this.
  return 0;
}

static int
SetClipOrigin (Display *dpy, GC gc, int x, int y)
{
  gc->gcv.clip_x_origin = x;
  gc->gcv.clip_y_origin = y;
  return 0;
}


const struct jwxyz_vtbl image_vtbl = {
  root,
  visual,
  display_sources_data,

  window_background,
  draw_arc,
  fill_rects,
  gc_gcv,
  gc_depth,
  jwxyz_draw_string,

  copy_area,

  DrawPoints,
  DrawSegments,
  CreateGC,
  FreeGC,
  ClearWindow,
  SetClipMask,
  SetClipOrigin,
  FillPolygon,
  DrawLines,
  PutImage,
  GetSubImage
};

#endif /* JWXYZ_IMAGE -- entire file */
