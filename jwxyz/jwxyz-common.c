/* xscreensaver, Copyright (c) 1991-2016 Jamie Zawinski <jwz@jwz.org>
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

   But it's a bunch of function definitions that bear some resemblance to
   Xlib and that do Cocoa-ish or OpenGL-ish things that bear some resemblance
   to the things that Xlib might have done.

   This is the version of jwxyz for Android.  The version used by MacOS
   and iOS is in jwxyz.m.
 */

#include "config.h"

#ifdef HAVE_JWXYZ /* whole file */

#include "jwxyzI.h"

/* There's only one Window for a given jwxyz_Display. */
#define assert_window(dpy, w) \
  Assert (w == RootWindow (dpy, 0), "not a window")


void
Log (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  Logv (fmt, args);
  va_end (args);
}


int
XDisplayWidth (Display *dpy, int screen)
{
  return jwxyz_frame (XRootWindow (dpy, 0))->width;
}

int
XDisplayHeight (Display *dpy, int screen)
{
  return jwxyz_frame (XRootWindow (dpy, 0))->height;
}


/* XLFDs use dots per inch, but Xlib uses millimeters. Go figure. */
static const unsigned dpi = 75;

int
XDisplayWidthMM (Display *dpy, int screen)
{
  const unsigned denom = dpi * 10 / 2;
  return (254 * XDisplayWidth (dpy, screen) + denom) / (2 * denom);
}

int
XDisplayHeightMM (Display *dpy, int screen)
{
  const unsigned denom = dpi * 10 / 2;
  return (254 * XDisplayHeight (dpy, screen) + denom) / (2 * denom);
}


void
jwxyz_validate_pixel (Display *dpy, unsigned long pixel, unsigned int depth,
                      Bool alpha_allowed_p)
{
  Assert (depth == 1 || depth == visual_depth(NULL, NULL),
          "invalid depth: %d", depth);

  if (depth == 1)
    Assert ((pixel == 0 || pixel == 1), "bogus mono  pixel: 0x%08X", pixel);
  else if (!alpha_allowed_p)
    Assert (((pixel & BlackPixel(dpy,0)) == BlackPixel(dpy,0)),
            "bogus color pixel: 0x%08X", pixel);
}


int
XDrawPoint (Display *dpy, Drawable d, GC gc, int x, int y)
{
  XPoint p;
  p.x = x;
  p.y = y;
  return XDrawPoints (dpy, d, gc, &p, 1, CoordModeOrigin);
}


Bool
jwxyz_dumb_drawing_mode(Display *dpy, Drawable d, GC gc,
                        int x, int y, unsigned width, unsigned height)
{
  XGCValues *gcv = jwxyz_gc_gcv (gc);

  if (gcv->function == GXset || gcv->function == GXclear) {
    // "set" and "clear" are dumb drawing modes that ignore the source
    // bits and just draw solid rectangles.
    unsigned depth = jwxyz_gc_depth (gc);
    jwxyz_fill_rect (dpy, d, 0, x, y, width, height,
                     (gcv->function == GXset
                      ? (depth == 1 ? 1 : WhitePixel(dpy,0))
                      : (depth == 1 ? 0 : BlackPixel(dpy,0))));
    return True;
  }

  return False;
}


int
XCopyArea (Display *dpy, Drawable src, Drawable dst, GC gc, 
           int src_x, int src_y, 
           unsigned int width, unsigned int height, 
           int dst_x, int dst_y)
{
  Assert (gc, "no GC");
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((src_x  < 65535 && src_x  > -65535), "improbably large src_x");
  Assert ((src_y  < 65535 && src_y  > -65535), "improbably large src_y");
  Assert ((dst_x  < 65535 && dst_x  > -65535), "improbably large dst_x");
  Assert ((dst_y  < 65535 && dst_y  > -65535), "improbably large dst_y");

  if (width == 0 || height == 0)
    return 0;

  if (jwxyz_dumb_drawing_mode (dpy, dst, gc, dst_x, dst_y, width, height))
    return 0;

  XRectangle src_frame, dst_frame; // Sizes and origins of the two drawables
  Bool clipped = False;            // Whether we did any clipping of the rects.

  src_frame = *jwxyz_frame (src);
  dst_frame = *jwxyz_frame (dst);
  
  // Initialize src_rect...
  //
  src_x += src_frame.x;
  src_y += src_frame.y;
  if (src_y < -65535) Assert(0, "src.origin.y went nuts");

  // Initialize dst_rect...
  //
  dst_x += dst_frame.x;
  dst_y += dst_frame.y;
  if (dst_y < -65535) Assert(0, "dst.origin.y went nuts");

  // Use signed width and height for this...
  int width0 = width, height0 = height;

  // Clip rects to frames...
  //

# define CLIP(THIS,THAT,VAL,SIZE) do { \
  int off = THIS##_##VAL; \
  if (off < 0) { \
    clipped = True; \
    SIZE##0      += off; \
    THIS##_##VAL -= off; \
    THAT##_##VAL -= off; \
  } \
  off = (( THIS##_##VAL +  SIZE##0) - \
         (THIS##_frame.VAL + THIS##_frame.SIZE)); \
  if (off > 0) { \
    clipped = True; \
    SIZE##0 -= off; \
  }} while(0)

  CLIP (dst, src, x, width);
  CLIP (dst, src, y, height);

  // Not actually the original dst_rect, just the one before it's clipped to
  // the src_frame.
  int orig_dst_x = dst_x;
  int orig_dst_y = dst_y;
  int orig_width  = width0;
  int orig_height = height0;

  if (width0 <= 0 || height0 <= 0)
    return 0;

  CLIP (src, dst, x, width);
  CLIP (src, dst, y, height);
# undef CLIP

  // Sort-of-special case where no pixels can be grabbed from the source,
  // and the whole destination is filled with the background color.
  if (width0 <= 0 || height0 <= 0) {
    width0  = 0;
    height0 = 0;
  } else {
    jwxyz_copy_area (dpy, src, dst, gc,
                     src_x, src_y, width0, height0, dst_x, dst_y);
  }

  // If either the src or dst rects did not lie within their drawables, then
  // we have adjusted both the src and dst rects to account for the clipping;
  // that means we need to clear to the background, so that clipped bits end
  // up in the bg color instead of simply not being copied.
  //
  // This has to happen after the copy, because if it happens before, the
  // cleared area will get grabbed if it overlaps with the source rectangle.
  //
  if (clipped && dst == XRootWindow (dpy,0)) {
    int dst_x0 = dst_x;
    int dst_y0 = dst_y;

    Assert (orig_dst_x >= 0 &&
            orig_dst_x + orig_width  <= dst_frame.width &&
            orig_dst_y >= 0 &&
            orig_dst_y + orig_height <= dst_frame.height,
            "wrong dimensions");

    XRectangle rects[4];
    XRectangle *rects_end = rects;

    if (orig_dst_y < dst_y0) {
      rects_end->x = orig_dst_x;
      rects_end->y = orig_dst_y;
      rects_end->width = orig_width;
      rects_end->height = dst_y0 - orig_dst_y;
      ++rects_end;
    }

    if (orig_dst_y + orig_height > dst_y0 + height0) {
      rects_end->x = orig_dst_x;
      rects_end->y = dst_y0 + height0;
      rects_end->width = orig_width;
      rects_end->height = orig_dst_y + orig_height - dst_y0 - height0;
      ++rects_end;
    }

    if (orig_dst_x < dst_x0) {
      rects_end->x = orig_dst_x;
      rects_end->y = dst_y0;
      rects_end->width = dst_x0 - orig_dst_x;
      rects_end->height = height0;
      ++rects_end;
    }

    if (dst_x0 + width0 < orig_dst_x + orig_width) {
      rects_end->x = dst_x0 + width0;
      rects_end->y = dst_y0;
      rects_end->width = orig_dst_x + orig_width - dst_x0 - width0;
      rects_end->height = height0;
      ++rects_end;
    }

    XGCValues *gcv = jwxyz_gc_gcv (gc);
    int old_function = gcv->function;
    gcv->function = GXcopy;
    jwxyz_fill_rects (dpy, dst, gc, rects, rects_end - rects,
                      jwxyz_window_background (dpy));
    gcv->function = old_function;
  }

  return 0;
}


int
XCopyPlane (Display *dpy, Drawable src, Drawable dest, GC gc,
            int src_x, int src_y,
            unsigned width, int height,
            int dest_x, int dest_y, unsigned long plane)
{
  Assert ((jwxyz_gc_depth (gc) == 1 || plane == 1), "hairy plane mask!");
  
  // This isn't right: XCopyPlane() is supposed to map 1/0 to fg/bg,
  // not to white/black.
  return XCopyArea (dpy, src, dest, gc,
                    src_x, src_y, width, height, dest_x, dest_y);
}


void
jwxyz_fill_rect (Display *dpy, Drawable d, GC gc,
                 int x, int y, unsigned int width, unsigned int height,
                 unsigned long pixel)
{
  XRectangle r = {x, y, width, height};
  jwxyz_fill_rects (dpy, d, gc, &r, 1, pixel);
}

int
XFillRectangle (Display *dpy, Drawable d, GC gc, int x, int y, 
                unsigned int width, unsigned int height)
{
  jwxyz_fill_rect (dpy, d, gc, x, y, width, height,
                   jwxyz_gc_gcv (gc)->foreground);
  return 0;
}

int
XDrawRectangle (Display *dpy, Drawable d, GC gc, int x, int y, 
                unsigned int width, unsigned int height)
{
  XPoint points[5] = {
    {x, y},
    {x, y + height},
    {x + width, y + height},
    {x + width, y},
    {x, y}
  };

  XDrawLines(dpy, d, gc, points, 5, CoordModeOrigin);
  return 0;
}

int
XFillRectangles (Display *dpy, Drawable d, GC gc, XRectangle *rects, int n)
{
  jwxyz_fill_rects (dpy, d, gc, rects, n, jwxyz_gc_gcv (gc)->foreground);
  return 0;
}


int
XDrawArc (Display *dpy, Drawable d, GC gc, int x, int y,
          unsigned int width, unsigned int height, int angle1, int angle2)
{
  return jwxyz_draw_arc (dpy, d, gc, x, y, width, height, angle1, angle2,
                         False);
}

int
XFillArc (Display *dpy, Drawable d, GC gc, int x, int y,
          unsigned int width, unsigned int height, int angle1, int angle2)
{
  return jwxyz_draw_arc (dpy, d, gc, x, y, width, height, angle1, angle2,
                         True);
}

int
XDrawArcs (Display *dpy, Drawable d, GC gc, XArc *arcs, int narcs)
{
  int i;
  for (i = 0; i < narcs; i++)
    jwxyz_draw_arc (dpy, d, gc,
                    arcs[i].x, arcs[i].y,
                    arcs[i].width, arcs[i].height,
                    arcs[i].angle1, arcs[i].angle2,
                    False);
  return 0;
}

int
XFillArcs (Display *dpy, Drawable d, GC gc, XArc *arcs, int narcs)
{
  int i;
  for (i = 0; i < narcs; i++)
    jwxyz_draw_arc (dpy, d, gc,
                    arcs[i].x, arcs[i].y,
                    arcs[i].width, arcs[i].height,
                    arcs[i].angle1, arcs[i].angle2,
                    True);
  return 0;
}

void
jwxyz_gcv_defaults (Display *dpy, XGCValues *gcv, int depth)
{
  memset (gcv, 0, sizeof(*gcv));
  gcv->function   = GXcopy;
  gcv->foreground = (depth == 1 ? 1 : WhitePixel(dpy,0));
  gcv->background = (depth == 1 ? 0 : BlackPixel(dpy,0));
  gcv->line_width = 1;
  gcv->cap_style  = CapButt;
  gcv->join_style = JoinMiter;
  gcv->fill_rule  = EvenOddRule;

  gcv->alpha_allowed_p = False;
  gcv->antialias_p     = True;
}


int
XChangeGC (Display *dpy, GC gc, unsigned long mask, XGCValues *from)
{
  if (! mask) return 0;
  Assert (gc && from, "no gc");
  if (!gc || !from) return 0;

  XGCValues *to = jwxyz_gc_gcv (gc);
  unsigned depth = jwxyz_gc_depth (gc);

  if (mask & GCFunction)        to->function            = from->function;
  if (mask & GCForeground)      to->foreground          = from->foreground;
  if (mask & GCBackground)      to->background          = from->background;
  if (mask & GCLineWidth)       to->line_width          = from->line_width;
  if (mask & GCCapStyle)        to->cap_style           = from->cap_style;
  if (mask & GCJoinStyle)       to->join_style          = from->join_style;
  if (mask & GCFillRule)        to->fill_rule           = from->fill_rule;
  if (mask & GCClipXOrigin)     to->clip_x_origin       = from->clip_x_origin;
  if (mask & GCClipYOrigin)     to->clip_y_origin       = from->clip_y_origin;
  if (mask & GCSubwindowMode)   to->subwindow_mode      = from->subwindow_mode;

  if (mask & GCClipMask)	XSetClipMask (0, gc, from->clip_mask);
  if (mask & GCFont)		XSetFont (0, gc, from->font);

  if (mask & GCForeground)
    jwxyz_validate_pixel (dpy, from->foreground, depth, to->alpha_allowed_p);
  if (mask & GCBackground)
    jwxyz_validate_pixel (dpy, from->background, depth, to->alpha_allowed_p);

  Assert ((! (mask & (GCLineStyle |
                      GCPlaneMask |
                      GCFillStyle |
                      GCTile |
                      GCStipple |
                      GCTileStipXOrigin |
                      GCTileStipYOrigin |
                      GCGraphicsExposures |
                      GCDashOffset |
                      GCDashList |
                      GCArcMode))),
          "unimplemented gcvalues mask");

  return 0;
}


Status
XGetWindowAttributes (Display *dpy, Window w, XWindowAttributes *xgwa)
{
  assert_window(dpy, w);
  memset (xgwa, 0, sizeof(*xgwa));
  const XRectangle *frame = jwxyz_frame (w);
  xgwa->x      = frame->x;
  xgwa->y      = frame->y;
  xgwa->width  = frame->width;
  xgwa->height = frame->height;
  xgwa->depth  = visual_depth (NULL, NULL);
  xgwa->screen = DefaultScreenOfDisplay (dpy);
  xgwa->visual = XDefaultVisualOfScreen (xgwa->screen);
  return 0;
}

Status
XGetGeometry (Display *dpy, Drawable d, Window *root_ret,
              int *x_ret, int *y_ret,
              unsigned int *w_ret, unsigned int *h_ret,
              unsigned int *bw_ret, unsigned int *d_ret)
{
  const XRectangle *frame = jwxyz_frame (d);
  *x_ret    = frame->x;
  *y_ret    = frame->y;
  *w_ret    = frame->width;
  *h_ret    = frame->height;
  *d_ret    = jwxyz_drawable_depth (d);
  *root_ret = RootWindow (dpy, 0);
  *bw_ret   = 0;
  return True;
}


Status
XAllocColor (Display *dpy, Colormap cmap, XColor *color)
{
  color->pixel = jwxyz_alloc_color (dpy,
                                    color->red,
                                    color->green,
                                    color->blue,
                                    0xFFFF);
  return 1;
}

Status
XAllocColorCells (Display *dpy, Colormap cmap, Bool contig,
                  unsigned long *pmret, unsigned int npl,
                  unsigned long *pxret, unsigned int npx)
{
  return 0;
}

int
XStoreColors (Display *dpy, Colormap cmap, XColor *colors, int n)
{
  Assert(0, "XStoreColors called");
  return 0;
}

int
XStoreColor (Display *dpy, Colormap cmap, XColor *c)
{
  Assert(0, "XStoreColor called");
  return 0;
}

int
XFreeColors (Display *dpy, Colormap cmap, unsigned long *px, int npixels,
             unsigned long planes)
{
  return 0;
}

Status
XParseColor (Display *dpy, Colormap cmap, const char *spec, XColor *ret)
{
  unsigned char r=0, g=0, b=0;
  if (*spec == '#' && strlen(spec) == 7) {
    static unsigned const char hex[] = {   // yeah yeah, shoot me.
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
      0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const unsigned char *uspec = (const unsigned char *)spec;
    r = (hex[uspec[1]] << 4) | hex[uspec[2]];
    g = (hex[uspec[3]] << 4) | hex[uspec[4]];
    b = (hex[uspec[5]] << 4) | hex[uspec[6]];
  } else if (!strcasecmp(spec,"black")) {
    //  r = g = b = 0;
  } else if (!strcasecmp(spec,"white")) {
    r = g = b = 255;
  } else if (!strcasecmp(spec,"red")) {
    r = 255;
  } else if (!strcasecmp(spec,"green")) {
    g = 255;
  } else if (!strcasecmp(spec,"blue")) {
    b = 255;
  } else if (!strcasecmp(spec,"cyan")) {
    g = b = 255;
  } else if (!strcasecmp(spec,"magenta")) {
    r = b = 255;
  } else if (!strcasecmp(spec,"yellow")) {
    r = g = 255;
  } else {
    return 0;
  }

  ret->red   = (r << 8) | r;
  ret->green = (g << 8) | g;
  ret->blue  = (b << 8) | b;
  ret->flags = DoRed|DoGreen|DoBlue;
  return 1;
}

Status
XAllocNamedColor (Display *dpy, Colormap cmap, char *name,
                  XColor *screen_ret, XColor *exact_ret)
{
  if (! XParseColor (dpy, cmap, name, screen_ret))
    return False;
  *exact_ret = *screen_ret;
  return XAllocColor (dpy, cmap, screen_ret);
}

int
XQueryColor (Display *dpy, Colormap cmap, XColor *color)
{
  jwxyz_validate_pixel (dpy, color->pixel, visual_depth (NULL, NULL), False);
  uint8_t rgba[4];
  jwxyz_query_color (dpy, color->pixel, rgba);
  color->red   = (rgba[0] << 8) | rgba[0];
  color->green = (rgba[1] << 8) | rgba[1];
  color->blue  = (rgba[2] << 8) | rgba[2];
  color->flags = DoRed|DoGreen|DoBlue;
  return 0;
}

int
XQueryColors (Display *dpy, Colormap cmap, XColor *c, int n)
{
  int i;
  for (i = 0; i < n; i++)
    XQueryColor (dpy, cmap, &c[i]);
  return 0;
}


static unsigned long
ximage_getpixel_1 (XImage *ximage, int x, int y)
{
  return ((ximage->data [y * ximage->bytes_per_line + (x>>3)] >> (x & 7)) & 1);
}

static int
ximage_putpixel_1 (XImage *ximage, int x, int y, unsigned long pixel)
{
  if (pixel)
    ximage->data [y * ximage->bytes_per_line + (x>>3)] |=  (1 << (x & 7));
  else
    ximage->data [y * ximage->bytes_per_line + (x>>3)] &= ~(1 << (x & 7));

  return 0;
}

static unsigned long
ximage_getpixel_32 (XImage *ximage, int x, int y)
{
  return ((unsigned long)
          *((uint32_t *) ximage->data +
            (y * (ximage->bytes_per_line >> 2)) +
            x));
}

static int
ximage_putpixel_32 (XImage *ximage, int x, int y, unsigned long pixel)
{
  *((uint32_t *) ximage->data +
    (y * (ximage->bytes_per_line >> 2)) +
    x) = (uint32_t) pixel;
  return 0;
}


Status
XInitImage (XImage *ximage)
{
  if (!ximage->bytes_per_line)
    ximage->bytes_per_line = (ximage->depth == 1
                              ? (ximage->width + 7) / 8
                              : ximage->width * 4);

  if (ximage->depth == 1) {
    ximage->f.put_pixel = ximage_putpixel_1;
    ximage->f.get_pixel = ximage_getpixel_1;
  } else if (ximage->depth == 32 || ximage->depth == 24) {
    ximage->f.put_pixel = ximage_putpixel_32;
    ximage->f.get_pixel = ximage_getpixel_32;
  } else {
    Assert (0, "unknown depth");
  }
  return 1;
}


XImage *
XCreateImage (Display *dpy, Visual *visual, unsigned int depth,
              int format, int offset, char *data,
              unsigned int width, unsigned int height,
              int bitmap_pad, int bytes_per_line)
{
  XImage *ximage = (XImage *) calloc (1, sizeof(*ximage));
  ximage->width = width;
  ximage->height = height;
  ximage->format = format;
  ximage->data = data;
  ximage->bitmap_unit = 8;
  ximage->byte_order = LSBFirst;
  ximage->bitmap_bit_order = ximage->byte_order;
  ximage->bitmap_pad = bitmap_pad;
  ximage->depth = depth;
  Visual *v = DefaultVisualOfScreen (DefaultScreenOfDisplay (dpy));
  ximage->red_mask   = (depth == 1 ? 0 : v->red_mask);
  ximage->green_mask = (depth == 1 ? 0 : v->green_mask);
  ximage->blue_mask  = (depth == 1 ? 0 : v->blue_mask);
  ximage->bits_per_pixel = (depth == 1 ? 1 : visual_depth (NULL, NULL));
  ximage->bytes_per_line = bytes_per_line;

  XInitImage (ximage);
  return ximage;
}

XImage *
XSubImage (XImage *from, int x, int y, unsigned int w, unsigned int h)
{
  XImage *to = (XImage *) malloc (sizeof(*to));
  memcpy (to, from, sizeof(*from));
  to->width = w;
  to->height = h;
  to->bytes_per_line = 0;
  XInitImage (to);

  to->data = (char *) malloc (h * to->bytes_per_line);

  if (x >= from->width)
    w = 0;
  else if (x+w > from->width)
    w = from->width - x;

  if (y >= from->height)
    h = 0;
  else if (y+h > from->height)
    h = from->height - y;

  int tx, ty;
  for (ty = 0; ty < h; ty++)
    for (tx = 0; tx < w; tx++)
      XPutPixel (to, tx, ty, XGetPixel (from, x+tx, y+ty));
  return to;
}


XPixmapFormatValues *
XListPixmapFormats (Display *dpy, int *n_ret)
{
  XPixmapFormatValues *ret = calloc (2, sizeof(*ret));
  ret[0].depth = visual_depth (NULL, NULL);
  ret[0].bits_per_pixel = 32;
  ret[0].scanline_pad = 8;
  ret[1].depth = 1;
  ret[1].bits_per_pixel = 1;
  ret[1].scanline_pad = 8;
  *n_ret = 2;
  return ret;
}


unsigned long
XGetPixel (XImage *ximage, int x, int y)
{
  return ximage->f.get_pixel (ximage, x, y);
}


int
XPutPixel (XImage *ximage, int x, int y, unsigned long pixel)
{
  return ximage->f.put_pixel (ximage, x, y, pixel);
}

int
XDestroyImage (XImage *ximage)
{
  if (ximage->data) free (ximage->data);
  free (ximage);
  return 0;
}


Pixmap
XCreatePixmapFromBitmapData (Display *dpy, Drawable drawable,
                             const char *data,
                             unsigned int w, unsigned int h,
                             unsigned long fg, unsigned int bg,
                             unsigned int depth)
{
  Pixmap p = XCreatePixmap (dpy, drawable, w, h, depth);
  XImage *image = XCreateImage (dpy, 0, 1, XYPixmap, 0,
                                (char *) data, w, h, 0, 0);
  XGCValues gcv;
  gcv.foreground = fg;
  gcv.background = bg;
  GC gc = XCreateGC (dpy, p, GCForeground|GCBackground, &gcv);
  XPutImage (dpy, p, gc, image, 0, 0, 0, 0, w, h);
  XFreeGC (dpy, gc);
  image->data = 0;
  XDestroyImage (image);
  return p;
}


char *
XGetAtomName (Display *dpy, Atom atom)
{
  if (atom == XA_FONT)
    return strdup ("FONT");

  // Note that atoms (that aren't predefined) are just char *.
  return strdup ((char *) atom);
}


int
XSetForeground (Display *dpy, GC gc, unsigned long fg)
{
  XGCValues *gcv = jwxyz_gc_gcv (gc);
  jwxyz_validate_pixel (dpy, fg, jwxyz_gc_depth (gc), gcv->alpha_allowed_p);
  gcv->foreground = fg;
  return 0;
}


int
XSetBackground (Display *dpy, GC gc, unsigned long bg)
{
  XGCValues *gcv = jwxyz_gc_gcv (gc);
  jwxyz_validate_pixel (dpy, bg, jwxyz_gc_depth (gc), gcv->alpha_allowed_p);
  gcv->background = bg;
  return 0;
}

int
jwxyz_XSetAlphaAllowed (Display *dpy, GC gc, Bool allowed)
{
  jwxyz_gc_gcv (gc)->alpha_allowed_p = allowed;
  return 0;
}

int
jwxyz_XSetAntiAliasing (Display *dpy, GC gc, Bool antialias_p)
{
  jwxyz_gc_gcv (gc)->antialias_p = antialias_p;
  return 0;
}


int
XSetLineAttributes (Display *dpy, GC gc, unsigned int line_width,
                    int line_style, int cap_style, int join_style)
{
  XGCValues *gcv = jwxyz_gc_gcv (gc);
  gcv->line_width = line_width;
  Assert (line_style == LineSolid, "only LineSolid implemented");
//  gc->gcv.line_style = line_style;
  gcv->cap_style = cap_style;
  gcv->join_style = join_style;
  return 0;
}

int
XSetGraphicsExposures (Display *dpy, GC gc, Bool which)
{
  return 0;
}

int
XSetFunction (Display *dpy, GC gc, int which)
{
  jwxyz_gc_gcv (gc)->function = which;
  return 0;
}

int
XSetSubwindowMode (Display *dpy, GC gc, int which)
{
  jwxyz_gc_gcv (gc)->subwindow_mode = which;
  return 0;
}


Bool
XQueryPointer (Display *dpy, Window w, Window *root_ret, Window *child_ret,
               int *root_x_ret, int *root_y_ret,
               int *win_x_ret, int *win_y_ret, unsigned int *mask_ret)
{
  assert_window (dpy, w);

  XPoint vpos, p;
  jwxyz_get_pos (w, &vpos, &p);

  if (root_x_ret) *root_x_ret = p.x;
  if (root_y_ret) *root_y_ret = p.y;
  if (win_x_ret)  *win_x_ret  = p.x - vpos.x;
  if (win_y_ret)  *win_y_ret  = p.y - vpos.y;
  if (mask_ret)   *mask_ret   = 0;  // #### poll the keyboard modifiers?
  if (root_ret)   *root_ret   = 0;
  if (child_ret)  *child_ret  = 0;
  return True;
}

Bool
XTranslateCoordinates (Display *dpy, Window w, Window dest_w,
                       int src_x, int src_y,
                       int *dest_x_ret, int *dest_y_ret,
                       Window *child_ret)
{
  assert_window (dpy, w);

  XPoint vpos, p;
  jwxyz_get_pos (w, &vpos, NULL);

  // point starts out relative to top left of view
  p.x = src_x;
  p.y = src_y;

  // get point relative to top left of screen
  p.x += vpos.x;
  p.y += vpos.y;

  *dest_x_ret = p.x;
  *dest_y_ret = p.y;
  if (child_ret)
    *child_ret = w;
  return True;
}


KeySym
XKeycodeToKeysym (Display *dpy, KeyCode code, int index)
{
  return code;
}

int
XLookupString (XKeyEvent *e, char *buf, int size, KeySym *k_ret,
               XComposeStatus *xc)
{
  KeySym ks = XKeycodeToKeysym (0, e->keycode, 0);
  char c = 0;
  // Do not put non-ASCII KeySyms like XK_Shift_L and XK_Page_Up in the string.
  if ((unsigned int) ks <= 255)
    c = (char) ks;

  // Put control characters in the string.  Not meta.
  if (e->state & ControlMask) {
    if (c >= 'a' && c <= 'z')    // Upcase control.
      c -= 'a'-'A';
    if (c >= '@' && c <= '_')    // Shift to control page.
      c -= '@';
    if (c == ' ')		 // C-SPC is NULL.
      c = 0;
  }

  if (k_ret) *k_ret = ks;
  if (size > 0) buf[0] = c;
  if (size > 1) buf[1] = 0;
  return (size > 0 ? 1 : 0);
}


int
XFlush (Display *dpy)
{
  // Just let the event loop take care of this on its own schedule.
  return 0;
}

int
XSync (Display *dpy, Bool flush)
{
  return XFlush (dpy);
}


// declared in utils/visual.h
int
has_writable_cells (Screen *s, Visual *v)
{
  return 0;
}

int
visual_depth (Screen *s, Visual *v)
{
  return 32;
}

int
visual_cells (Screen *s, Visual *v)
{
  return (int)(v->red_mask | v->green_mask | v->blue_mask);
}

int
visual_class (Screen *s, Visual *v)
{
  return TrueColor;
}

int
get_bits_per_pixel (Display *dpy, int depth)
{
  Assert (depth == 32 || depth == 1, "unexpected depth");
  return depth;
}

int
screen_number (Screen *screen)
{
  Display *dpy = DisplayOfScreen (screen);
  int i;
  for (i = 0; i < ScreenCount (dpy); i++)
    if (ScreenOfDisplay (dpy, i) == screen)
      return i;
  abort ();
  return 0;
}

// declared in utils/grabclient.h
Bool
use_subwindow_mode_p (Screen *screen, Window window)
{
  return False;
}

#endif /* HAVE_JWXYZ */
