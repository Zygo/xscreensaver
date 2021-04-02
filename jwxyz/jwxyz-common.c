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

   macOS and iOS:

       jwxyz.m         -- Xlib in terms of Quartz / Core Graphics rendering,
                          Renders into a CGContextRef that points to a CGImage
                          in CPU RAM.
       jwxyz-cocoa.m   -- Cocoa glue: fonts and such.

   Android:

       jwxyz-android.c -- Java / JNI glue.  Renders into an FBO texture if
                          possible, otherwise an EGL PBuffer.

       jwxyz-image.c   -- Pixmaps implemented in CPU RAM, for OpenGL hacks.
                          Renders into an XImage, basically.

       jwxyz-gl.c      -- Pixmaps implemented in terms of OpenGL textures,
                          for X11 hacks (except kumppa, petri and slip).

   Shared code:

       jwxyz-common.c  -- Most of the Xlib implementation, used by all 3 OSes.
       jwzgles.c       -- OpenGL 1.3 implemented in terms of OpenGLES 1.1.


   Why two implemementations of Pixmaps for Android?

     - GL hacks don't tend to need much in the way of Xlib, and having a
       GL context to render Xlib alongside a GL context for rendering GL
       seemed like trouble.

     - X11 hacks render to a GL context because hardware acceleration tends
       to work well with Xlib geometric stuff.  Better framerates, lower
       power.

   Why not eliminate jwxyz-cocoa.m and use jwxyz-gl.c on macOS and iOS
   as well?

     - Yeah, maybe.

 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_JWXYZ /* whole file */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "jwxyzI.h"
#include "pow2.h"
#include "utf8wc.h"
#include "xft.h"

/* There's only one Window for a given jwxyz_Display. */
#define assert_window(dpy, w) \
  Assert (w == RootWindow (dpy, 0), "not a window")

#define VTBL JWXYZ_VTBL(dpy)

struct jwxyz_Font {
  Display *dpy;
  void *native_font;
  int refcount; // for deciding when to release the native font
  int ascent, descent;
  char *xa_font;

  // In X11, "Font" is just an ID, and "XFontStruct" contains the metrics.
  // But we need the metrics on both of them, so they go here.
  XFontStruct metrics;
};

struct jwxyz_XFontSet {
  XFontStruct *font;
};


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
static int
size_mm (Display *dpy, unsigned size)
{
  /* ((mm / inch) / (points / inch)) * dots / (dots / points) */
  return (25.4 / 72) * size / jwxyz_scale (XRootWindow (dpy,0)) + 0.5;
}

int
XDisplayWidthMM (Display *dpy, int screen)
{
  return size_mm (dpy, XDisplayWidth (dpy, screen));
}

int
XDisplayHeightMM (Display *dpy, int screen)
{
  return size_mm (dpy, XDisplayHeight (dpy, screen));
}

unsigned long
XBlackPixelOfScreen(Screen *screen)
{
  if (! screen) abort();
  return DefaultVisualOfScreen (screen)->alpha_mask;
}

unsigned long
XWhitePixelOfScreen(Screen *screen)
{
  Visual *v = DefaultVisualOfScreen (screen);
  return (v->red_mask | v->green_mask |v->blue_mask | v->alpha_mask);
}

unsigned long
XCellsOfScreen(Screen *screen)
{
  Visual *v = DefaultVisualOfScreen (screen);
  return (v->red_mask | v->green_mask |v->blue_mask);
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
  XGCValues *gcv = VTBL->gc_gcv (gc);

  if (gcv->function == GXset || gcv->function == GXclear) {
    // "set" and "clear" are dumb drawing modes that ignore the source
    // bits and just draw solid rectangles.
    unsigned depth = VTBL->gc_depth (gc);
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
    VTBL->copy_area (dpy, src, dst, gc,
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

    XGCValues *gcv = VTBL->gc_gcv (gc);
    int old_function = gcv->function;
    gcv->function = GXcopy;
    VTBL->fill_rects (dpy, dst, gc, rects, rects_end - rects,
                      *VTBL->window_background (dpy));
    gcv->function = old_function;
  }

  return 0;
}


void
jwxyz_blit (const void *src_data, ptrdiff_t src_pitch,
            unsigned src_x, unsigned src_y,
            void *dst_data, ptrdiff_t dst_pitch,
            unsigned dst_x, unsigned dst_y,
            unsigned width, unsigned height)
{
  Bool same = src_data == dst_data;
  src_data = SEEK_XY (src_data, src_pitch, src_x, src_y);
  dst_data = SEEK_XY (dst_data, dst_pitch, dst_x, dst_y);

  size_t bytes = width * 4;

  if (same && dst_y > src_y) {
    // Copy upwards if the areas might overlap.
    src_data += src_pitch * (height - 1);
    dst_data += dst_pitch * (height - 1);
    src_pitch = -src_pitch;
    dst_pitch = -dst_pitch;
  }

  while (height) {
    // memcpy is an alias for memmove on macOS.
    memmove (dst_data, src_data, bytes);
    src_data += src_pitch;
    dst_data += dst_pitch;
    --height;
  }
}


int
XCopyPlane (Display *dpy, Drawable src, Drawable dest, GC gc,
            int src_x, int src_y,
            unsigned width, int height,
            int dest_x, int dest_y, unsigned long plane)
{
  Assert ((VTBL->gc_depth (gc) == 1 || plane == 1), "hairy plane mask!");
  
  // This isn't right: XCopyPlane() is supposed to map 1/0 to fg/bg,
  // not to white/black.
  return XCopyArea (dpy, src, dest, gc,
                    src_x, src_y, width, height, dest_x, dest_y);
}


int
XDrawLine (Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2)
{
  XSegment segment;
  segment.x1 = x1;
  segment.y1 = y1;
  segment.x2 = x2;
  segment.y2 = y2;
  XDrawSegments (dpy, d, gc, &segment, 1);
  return 0;
}


int
XSetWindowBackground (Display *dpy, Window w, unsigned long pixel)
{
  Assert (w == XRootWindow (dpy,0), "not a window");
  jwxyz_validate_pixel (dpy, pixel, visual_depth (NULL, NULL), False);
  *VTBL->window_background (dpy) = pixel;
  return 0;
}

void
jwxyz_fill_rect (Display *dpy, Drawable d, GC gc,
                 int x, int y, unsigned int width, unsigned int height,
                 unsigned long pixel)
{
  XRectangle r = {x, y, width, height};
  VTBL->fill_rects (dpy, d, gc, &r, 1, pixel);
}

int
XFillRectangle (Display *dpy, Drawable d, GC gc, int x, int y, 
                unsigned int width, unsigned int height)
{
  jwxyz_fill_rect (dpy, d, gc, x, y, width, height,
                   VTBL->gc_gcv (gc)->foreground);
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
  VTBL->fill_rects (dpy, d, gc, rects, n, VTBL->gc_gcv (gc)->foreground);
  return 0;
}


int
XClearArea (Display *dpy, Window win, int x, int y, int w, int h, Bool exp)
{
  Assert(win == XRootWindow(dpy,0), "XClearArea: not a window");
  Assert(!exp, "XClearArea: exposures unsupported");
  jwxyz_fill_rect (dpy, win, 0, x, y, w, h, *VTBL->window_background (dpy));
  return 0;
}


int
XDrawArc (Display *dpy, Drawable d, GC gc, int x, int y,
          unsigned int width, unsigned int height, int angle1, int angle2)
{
  return VTBL->draw_arc (dpy, d, gc, x, y, width, height, angle1, angle2,
                         False);
}

int
XFillArc (Display *dpy, Drawable d, GC gc, int x, int y,
          unsigned int width, unsigned int height, int angle1, int angle2)
{
  return VTBL->draw_arc (dpy, d, gc, x, y, width, height, angle1, angle2,
                         True);
}

int
XDrawArcs (Display *dpy, Drawable d, GC gc, XArc *arcs, int narcs)
{
  int i;
  for (i = 0; i < narcs; i++)
    VTBL->draw_arc (dpy, d, gc,
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
    VTBL->draw_arc (dpy, d, gc,
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

  XGCValues *to = VTBL->gc_gcv (gc);
  unsigned depth = VTBL->gc_depth (gc);

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

  if (mask & GCClipMask)	XSetClipMask (dpy, gc, from->clip_mask);
  if (mask & GCFont)		XSetFont (dpy, gc, from->font);

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
  Visual *v = DefaultVisualOfScreen (DefaultScreenOfDisplay (dpy));
  color->pixel =
    (((color->red   << 16) >> (31 - i_log2(v->red_mask)))   & v->red_mask)   |
    (((color->green << 16) >> (31 - i_log2(v->green_mask))) & v->green_mask) |
    (((color->blue  << 16) >> (31 - i_log2(v->blue_mask)))  & v->blue_mask)  |
    v->alpha_mask;
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
  uint16_t rgba[4];
  JWXYZ_QUERY_COLOR (dpy, color->pixel, 0xffffull, rgba);
  color->red   = rgba[0];
  color->green = rgba[1];
  color->blue  = rgba[2];
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
ximage_getpixel_8 (XImage *ximage, int x, int y)
{
  return ((unsigned long)
          *((uint8_t *) ximage->data +
            (y * ximage->bytes_per_line) +
            x));
}

static int
ximage_putpixel_8 (XImage *ximage, int x, int y, unsigned long pixel)
{
  *((uint8_t *) ximage->data +
    (y * ximage->bytes_per_line) +
    x) = (uint8_t) pixel;
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
    ximage->bytes_per_line = (ximage->depth == 1 ? (ximage->width + 7) / 8 :
                              ximage->depth == 8 ? ximage->width :
                              ximage->width * 4);

  if (ximage->depth == 1) {
    ximage->f.put_pixel = ximage_putpixel_1;
    ximage->f.get_pixel = ximage_getpixel_1;
  } else if (ximage->depth == 32 || ximage->depth == 24) {
    ximage->f.put_pixel = ximage_putpixel_32;
    ximage->f.get_pixel = ximage_getpixel_32;
  } else if (ximage->depth == 8) {
    ximage->f.put_pixel = ximage_putpixel_8;
    ximage->f.get_pixel = ximage_getpixel_8;
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


XImage *
XGetImage (Display *dpy, Drawable d, int x, int y,
           unsigned int width, unsigned int height,
           unsigned long plane_mask, int format)
{
  unsigned depth = jwxyz_drawable_depth (d);
  XImage *image = XCreateImage (dpy, 0, depth, format, 0, 0, width, height,
                                0, 0);
  image->data = (char *) malloc (height * image->bytes_per_line);

  return XGetSubImage (dpy, d, x, y, width, height, plane_mask, format,
                       image, 0, 0);
}


Pixmap
XCreatePixmapFromBitmapData (Display *dpy, Drawable drawable,
                             const char *data,
                             unsigned int w, unsigned int h,
                             unsigned long fg, unsigned long bg,
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


// This is XQueryFont, but for the XFontStruct embedded in 'Font'
//
static void
query_font (Font fid)
{
  Assert (fid && fid->native_font, "no native font in fid");

  int first = 32;
  int last = 255;

  Display *dpy = fid->dpy;
  void *native_font = fid->native_font;

  XFontStruct *f = &fid->metrics;
  XCharStruct *min = &f->min_bounds;
  XCharStruct *max = &f->max_bounds;

  f->fid               = fid;
  f->min_char_or_byte2 = first;
  f->max_char_or_byte2 = last;
  f->default_char      = 'M';
  f->ascent            = fid->ascent;
  f->descent           = fid->descent;

  min->width    = 32767;  // set to smaller values in the loop
  min->ascent   = 32767;
  min->descent  = 32767;
  min->lbearing = 32767;
  min->rbearing = 32767;

  f->per_char = (XCharStruct *) calloc (last-first+2, sizeof (XCharStruct));

  for (int i = first; i <= last; i++) {
    XCharStruct *cs = &f->per_char[i-first];
    char s = (char) i;
    jwxyz_render_text (dpy, native_font, &s, 1, False, False, cs, 0);

    max->width    = MAX (max->width,    cs->width);
    max->ascent   = MAX (max->ascent,   cs->ascent);
    max->descent  = MAX (max->descent,  cs->descent);
    max->lbearing = MAX (max->lbearing, cs->lbearing);
    max->rbearing = MAX (max->rbearing, cs->rbearing);

    min->width    = MIN (min->width,    cs->width);
    min->ascent   = MIN (min->ascent,   cs->ascent);
    min->descent  = MIN (min->descent,  cs->descent);
    min->lbearing = MIN (min->lbearing, cs->lbearing);
    min->rbearing = MIN (min->rbearing, cs->rbearing);
/*
    Log (" %3d %c: w=%3d lb=%3d rb=%3d as=%3d ds=%3d "
         " bb=%5.1f x %5.1f @ %5.1f %5.1f  adv=%5.1f %5.1f\n"
         i, i, cs->width, cs->lbearing, cs->rbearing,
         cs->ascent, cs->descent,
         bbox.size.width, bbox.size.height,
         bbox.origin.x, bbox.origin.y,
         advancement.width, advancement.height);
 */
  }
}


// Since 'Font' includes the metrics, this just makes a copy of that.
//
XFontStruct *
XQueryFont (Display *dpy, Font fid)
{
  // copy XFontStruct
  XFontStruct *f = (XFontStruct *) calloc (1, sizeof(*f));
  *f = fid->metrics;
  f->fid = fid;

  // build XFontProps
  f->n_properties = 1;
  f->properties = malloc (sizeof(*f->properties) * f->n_properties);
  f->properties[0].name = XA_FONT;
  Assert (sizeof (f->properties[0].card32) >= sizeof (char *),
          "atoms probably needs a real implementation");
  // If XInternAtom is ever implemented, use it here.
  f->properties[0].card32 = (unsigned long)fid->xa_font;

  // copy XCharStruct array
  int size = (f->max_char_or_byte2 - f->min_char_or_byte2) + 1;
  f->per_char = (XCharStruct *) calloc (size + 2, sizeof (XCharStruct));

  memcpy (f->per_char, fid->metrics.per_char,
          size * sizeof (XCharStruct));

  return f;
}


static Font
copy_font (Font fid)
{
  fid->refcount++;
  return fid;
}


/* On Cocoa and iOS, fonts may be specified as "Georgia Bold 24" instead
   of XLFD strings; also they can be comma-separated strings with multiple
   font names.  First one that exists wins.
 */
static void
try_native_font (Display *dpy, const char *name, Font fid)
{
  if (!name) return;
  const char *spc = strrchr (name, ' ');
  if (!spc) return;

  char *token = strdup (name);
  char *otoken = token;
  char *name2;
  char *lasts;

  while ((name2 = strtok_r (token, ",", &lasts))) {
    token = 0;

    while (*name2 == ' ' || *name2 == '\t' || *name2 == '\n')
      name2++;

    spc = strrchr (name2, ' ');
    if (!spc) continue;

    int dsize = 0;
    if (1 != sscanf (spc, " %d ", &dsize))
      continue;
    float size = dsize;

    if (size < 4) continue;

    name2[strlen(name2) - strlen(spc)] = 0;

    fid->native_font = jwxyz_load_native_font(XRootWindow(dpy,0), 0, 0, name2,
                                              strlen(name2) - strlen(spc),
                                              JWXYZ_FONT_FACE, size, NULL,
                                              &fid->ascent, &fid->descent);
    if (fid->native_font) {
      fid->xa_font = strdup (name); // Maybe this should be an XLFD?
      break;
    } else {
      // To list fonts:
      //  po [UIFont familyNames]
      //  po [UIFont fontNamesForFamilyName:@"Arial"]
      Log("No native font: \"%s\" %.0f", name2, size);
    }
  }

  free (otoken);
}


static const char *
xlfd_field_end (const char *s)
{
  const char *s2 = strchr(s, '-');
  if (!s2)
    s2 = s + strlen(s);
  return s2;
}


static size_t
xlfd_next (const char **s, const char **s2)
{
  if (!**s2) {
    *s = *s2;
  } else {
    Assert (**s2 == '-', "xlfd parse error");
    *s = *s2 + 1;
    *s2 = xlfd_field_end (*s);
  }

  return *s2 - *s;
}


static void
try_xlfd_font (Display *dpy, const char *name, Font fid)
{
  const char *family_name = NULL; /* Not NULL-terminated. */
  size_t family_name_size = 0;
  int require = 0,
   // Default mask is for the built-in X11 font aliases.
   mask = JWXYZ_STYLE_MONOSPACE | JWXYZ_STYLE_BOLD | JWXYZ_STYLE_ITALIC;
  Bool rand  = False;
  float size = 12; /* In points (1/72 in.) */

  const char *s = (name ? name : "");

  size_t L = strlen (s);
# define CMP(STR) (L == strlen(STR) && !strncasecmp (s, (STR), L))
# define UNSPEC   (L == 0 || (L == 1 && *s == '*'))
  if      (CMP ("6x10"))     size = 8,  require |= JWXYZ_STYLE_MONOSPACE;
  else if (CMP ("6x10bold")) size = 8,  require |= JWXYZ_STYLE_MONOSPACE | JWXYZ_STYLE_BOLD;
  else if (CMP ("fixed"))    size = 12, require |= JWXYZ_STYLE_MONOSPACE;
  else if (CMP ("9x15"))     size = 12, require |= JWXYZ_STYLE_MONOSPACE;
  else if (CMP ("9x15bold")) size = 12, require |= JWXYZ_STYLE_MONOSPACE | JWXYZ_STYLE_BOLD;
  else if (CMP ("vga"))      size = 12, require |= JWXYZ_STYLE_MONOSPACE;
  else if (CMP ("console"))  size = 12, require |= JWXYZ_STYLE_MONOSPACE;
  else if (CMP ("gallant"))  size = 12, require |= JWXYZ_STYLE_MONOSPACE;
  else {

    int forbid = 0;

    // Incorrect fields are ignored.

    if (*s == '-')
      ++s;
    const char *s2 = xlfd_field_end(s);

    // Foundry (ignore)

    L = xlfd_next (&s, &s2); // Family name
    // This used to substitute Georgia for Times. Now it doesn't.
    if (CMP ("random")) {
      rand = True;
    } else if (CMP ("fixed") || CMP ("monospace")) {
      require |= JWXYZ_STYLE_MONOSPACE;
      family_name = "Courier";
      family_name_size = strlen(family_name);
    } else if (!UNSPEC) {
      family_name = s;
      family_name_size = L;
    }

    L = xlfd_next (&s, &s2); // Weight name
    if (CMP ("bold") || CMP ("demibold"))
      require |= JWXYZ_STYLE_BOLD;
    else if (CMP ("medium") || CMP ("regular"))
      forbid |= JWXYZ_STYLE_BOLD;

    L = xlfd_next (&s, &s2); // Slant
    if (CMP ("i") || CMP ("o"))
      require |= JWXYZ_STYLE_ITALIC;
    else if (CMP ("r"))
      forbid |= JWXYZ_STYLE_ITALIC;

    xlfd_next (&s, &s2); // Set width name (ignore)
    xlfd_next (&s, &s2); // Add style name (ignore)

    L = xlfd_next (&s, &s2); // Pixel size
    char *s3;
    uintmax_t pxsize = strtoumax(s, &s3, 10);
    if (UNSPEC || s2 != s3)
      pxsize = UINTMAX_MAX; // i.e. it's invalid.

    L = xlfd_next (&s, &s2); // Point size
    uintmax_t ptsize = strtoumax(s, &s3, 10);
    if (UNSPEC || s2 != s3)
      ptsize = UINTMAX_MAX;

    xlfd_next (&s, &s2); // Resolution X (ignore)
    xlfd_next (&s, &s2); // Resolution Y (ignore)

    L = xlfd_next (&s, &s2); // Spacing
    if (CMP ("p"))
      forbid |= JWXYZ_STYLE_MONOSPACE;
    else if (CMP ("m") || CMP ("c"))
      require |= JWXYZ_STYLE_MONOSPACE;

    xlfd_next (&s, &s2); // Average width (ignore)

    // -*-courier-bold-r-*-*-14-*-*-*-*-*-*-*         14 px
    // -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*        14 pt
    // -*-courier-bold-r-*-*-140-*                    14 pt, via wildcard
    // -*-courier-bold-r-*-140-*                      14 pt, not handled
    // -*-courier-bold-r-*-*-14-180-*-*-*-*-*-*       error

    L = xlfd_next (&s, &s2); // Charset registry
    if (ptsize != UINTMAX_MAX) {
      // It was in the ptsize field, so that's definitely what it is.
      size = ptsize / 10.0;
    } else if (pxsize != UINTMAX_MAX) {
      size = pxsize;
      // If it's a fully qualified XLFD, then this really is the pxsize.
      // Otherwise, this is probably point size with a multi-field wildcard.
      if (L == 0)
        size /= 10.0;
    }

    mask = require | forbid;
  }
# undef CMP
# undef UNSPEC

  if (!family_name && !rand) {
    family_name = jwxyz_default_font_family (require);
    family_name_size = strlen (family_name);
  }

  if (size < 6 || size > 1000)
    size = 12;

  char *family_name_ptr = NULL;
  fid->native_font = jwxyz_load_native_font (XRootWindow(dpy,0),
                                             require, mask,
                                             family_name, family_name_size,
                                             rand ? JWXYZ_FONT_RANDOM : JWXYZ_FONT_FAMILY,
                                             size, &family_name_ptr,
                                             &fid->ascent, &fid->descent);

  if (fid->native_font) {
    unsigned dpi_d = XDisplayHeightMM (dpy,0) * 10 / 2;
    unsigned dpi = (254 * XDisplayHeight (dpy,0) + dpi_d) / (2 * dpi_d);
    asprintf(&fid->xa_font, "-*-%s-%s-%c-*-*-%u-%u-%u-%u-%c-0-iso10646-1",
             family_name_ptr,
             (require & JWXYZ_STYLE_BOLD) ? "bold" : "medium",
             (require & JWXYZ_STYLE_ITALIC) ? 'o' : 'r',
             (unsigned)(dpi * size / 72.27 + 0.5),
             (unsigned)(size * 10 + 0.5), dpi, dpi,
             (require & JWXYZ_STYLE_MONOSPACE) ? 'm' : 'p');
  }

  free (family_name_ptr);
}


Font
XLoadFont (Display *dpy, const char *name)
{
  Font fid = (Font) calloc (1, sizeof(*fid));

  fid->refcount = 1;
  fid->dpy = dpy;
  try_native_font (dpy, name, fid);

  if (!fid->native_font && name &&
      strchr (name, ' ') &&
      !strchr (name, '*')) {
    // If name contains a space but no stars, it is a native font spec --
    // return NULL so that we know it really didn't exist.  Else, it is an
    //  XLFD font, so keep trying.
    free (fid);
    return 0;
  }

  if (! fid->native_font)
    try_xlfd_font (dpy, name, fid);

  if (!fid->native_font) {
    free (fid);
    return 0;
  }

  query_font (fid);

  return fid;
}


XFontStruct *
XLoadQueryFont (Display *dpy, const char *name)
{
  Font fid = XLoadFont (dpy, name);
  if (!fid) return 0;
  return XQueryFont (dpy, fid);
}

int
XUnloadFont (Display *dpy, Font fid)
{
  if (--fid->refcount < 0) abort();
  if (fid->refcount > 0) return 0;

  if (fid->native_font)
    jwxyz_release_native_font (fid->dpy, fid->native_font);

  if (fid->metrics.per_char)
    free (fid->metrics.per_char);

  free (fid);
  return 0;
}

int
XFreeFontInfo (char **names, XFontStruct *info, int n)
{
  int i;
  if (names) {
    for (i = 0; i < n; i++)
      if (names[i]) free (names[i]);
    free (names);
  }
  if (info) {
    for (i = 0; i < n; i++)
      if (info[i].per_char) {
        free (info[i].per_char);
        free (info[i].properties);
      }
    free (info);
  }
  return 0;
}

int
XFreeFont (Display *dpy, XFontStruct *f)
{
  Font fid = f->fid;
  XFreeFontInfo (0, f, 1);
  XUnloadFont (dpy, fid);
  return 0;
}


int
XSetFont (Display *dpy, GC gc, Font fid)
{
  XGCValues *gcv = VTBL->gc_gcv(gc);
  Font font2 = copy_font (fid);
  if (gcv->font)
    XUnloadFont (dpy, gcv->font);
  gcv->font = font2;
  return 0;
}


XFontSet
XCreateFontSet (Display *dpy, char *name,
                char ***missing_charset_list_return,
                int *missing_charset_count_return,
                char **def_string_return)
{
  char *name2 = strdup (name);
  char *s = strchr (name, ',');
  if (s) *s = 0;
  XFontSet set = 0;
  XFontStruct *f = XLoadQueryFont (dpy, name2);
  if (f)
  {
    set = (XFontSet) calloc (1, sizeof(*set));
    set->font = f;
  }
  free (name2);
  if (missing_charset_list_return)  *missing_charset_list_return = 0;
  if (missing_charset_count_return) *missing_charset_count_return = 0;
  if (def_string_return) *def_string_return = 0;
  return set;
}


void
XFreeFontSet (Display *dpy, XFontSet set)
{
  XFreeFont (dpy, set->font);
  free (set);
}


void
XFreeStringList (char **list)
{
  int i;
  if (!list) return;
  for (i = 0; list[i]; i++)
    XFree (list[i]);
  XFree (list);
}


int
XTextExtents (XFontStruct *f, const char *s, int length,
              int *dir_ret, int *ascent_ret, int *descent_ret,
              XCharStruct *cs)
{
  // Unfortunately, adding XCharStructs together to get the extents for a
  // string doesn't work: Cocoa uses non-integral character advancements, but
  // XCharStruct.width is an integer. Plus that doesn't take into account
  // kerning pairs, alternate glyphs, and fun stuff like the word "Zapfino" in
  // Zapfino.

  Font ff = f->fid;
  Display *dpy = ff->dpy;
  jwxyz_render_text (dpy, ff->native_font, s, length, False, False, cs, 0);
  *dir_ret = 0;
  *ascent_ret  = f->ascent;
  *descent_ret = f->descent;
  return 0;
}

int
XTextWidth (XFontStruct *f, const char *s, int length)
{
  int ascent, descent, dir;
  XCharStruct cs;
  XTextExtents (f, s, length, &dir, &ascent, &descent, &cs);
  return cs.width;
}


int
XTextExtents16 (XFontStruct *f, const XChar2b *s, int length,
                int *dir_ret, int *ascent_ret, int *descent_ret,
                XCharStruct *cs)
{
  // Bool latin1_p = True;
  int i, utf8_len = 0;
  char *utf8 = XChar2b_to_utf8 (s, &utf8_len);   // already sanitized

  for (i = 0; i < length; i++)
    if (s[i].byte1 > 0) {
      // latin1_p = False;
      break;
    }

  {
    Font ff = f->fid;
    Display *dpy = ff->dpy;
    jwxyz_render_text (dpy, ff->native_font, utf8, strlen(utf8),
                       True, False, cs, 0);
  }

  *dir_ret = 0;
  *ascent_ret  = f->ascent;
  *descent_ret = f->descent;
  free (utf8);
  return 0;
}


/* "Returns the distance in pixels in the primary draw direction from
 the drawing origin to the origin of the next character to be drawn."

 "overall_ink_return is set to the bbox of the string's character ink."

 "The overall_ink_return for a nondescending, horizontally drawn Latin
 character is conventionally entirely above the baseline; that is,
 overall_ink_return.height <= -overall_ink_return.y."

 [So this means that y is the top of the ink, and height grows down:
 For above-the-baseline characters, y is negative.]

 "The overall_ink_return for a nonkerned character is entirely at, and to
 the right of, the origin; that is, overall_ink_return.x >= 0."

 [So this means that x is the left of the ink, and width grows right.
 For left-of-the-origin characters, x is negative.]

 "A character consisting of a single pixel at the origin would set
 overall_ink_return fields y = 0, x = 0, width = 1, and height = 1."
 */
int
Xutf8TextExtents (XFontSet set, const char *str, int len,
                  XRectangle *overall_ink_return,
                  XRectangle *overall_logical_return)
{
  XCharStruct cs;
  Font f = set->font->fid;

  jwxyz_render_text (f->dpy, f->native_font, str, len, True, False, &cs,
                     NULL);

  /* "The overall_logical_return is the bounding box that provides minimum
   spacing to other graphical features for the string. Other graphical
   features, for example, a border surrounding the text, should not
   intersect this rectangle."

   So I think that means they're the same?  Or maybe "ink" is the bounding
   box, and "logical" is the advancement?  But then why is the return value
   the advancement?
   */
  if (overall_ink_return)
    XCharStruct_to_XmbRectangle (cs, *overall_ink_return);
  if (overall_logical_return)
    XCharStruct_to_XmbRectangle (cs, *overall_logical_return);

  return cs.width;
}


int
jwxyz_draw_string (Display *dpy, Drawable d, GC gc, int x, int y,
                   const char *str, size_t len, int utf8_p)
{
  const XGCValues *gcv = VTBL->gc_gcv (gc);
  Font ff = gcv->font;
  XCharStruct cs;

  char *data = 0;
  jwxyz_render_text (dpy, jwxyz_native_font (ff), str, len, utf8_p,
                     gcv->antialias_p, &cs, &data);
  int w = cs.rbearing - cs.lbearing;
  int h = cs.ascent + cs.descent;

  if (w < 0 || h < 0) abort();
  if (w == 0 || h == 0) {
    if (data) free(data);
    return 0;
  }

  XImage *img = XCreateImage (dpy, VTBL->visual (dpy), 32,
                              ZPixmap, 0, data, w, h, 0, 0);

  /* The image of text is a 32-bit image, in white.
     Take the green channel for intensity and use that as alpha.
     replace RGB with the GC's foreground color.
     This expects that XPutImage respects alpha and only writes
     the bits that are not masked out.
   */
  {
# define ROTL(x, rot) (((x) << ((rot) & 31)) | ((x) >> (32 - ((rot) & 31))))

    Visual *v = DefaultVisualOfScreen (DefaultScreenOfDisplay(dpy));
    unsigned shift = (i_log2 (v->alpha_mask) - i_log2 (v->green_mask)) & 31;
    uint32_t mask = ROTL(v->green_mask, shift) & v->alpha_mask,
             color = gcv->foreground & ~v->alpha_mask;
    uint32_t *s = (uint32_t *)data;
    uint32_t *end = s + (w * h);
    while (s < end) {

      *s = (ROTL(*s, shift) & mask) | color;
      ++s;
    }
  }

  {
    Bool old_alpha = gcv->alpha_allowed_p;
    jwxyz_XSetAlphaAllowed (dpy, gc, True);
    XPutImage (dpy, d, gc, img, 0, 0,
               x + cs.lbearing,
               y - cs.ascent,
               w, h);
    jwxyz_XSetAlphaAllowed (dpy, gc, old_alpha);
    XDestroyImage (img);
  }

  return 0;
}


int
XDrawString (Display *dpy, Drawable d, GC gc, int x, int y,
             const char  *str, int len)
{
  return VTBL->draw_string (dpy, d, gc, x, y, str, len, False);
}


int
XDrawString16 (Display *dpy, Drawable d, GC gc, int x, int y,
               const XChar2b *str, int len)
{
  XChar2b *b2 = malloc ((len + 1) * sizeof(*b2));
  char *s2;
  int ret;
  memcpy (b2, str, len * sizeof(*b2));
  b2[len].byte1 = b2[len].byte2 = 0;
  s2 = XChar2b_to_utf8 (b2, 0);
  free (b2);
  ret = VTBL->draw_string (dpy, d, gc, x, y, s2, strlen(s2), True);
  free (s2);
  return ret;
}


void
Xutf8DrawString (Display *dpy, Drawable d, XFontSet set, GC gc,
                 int x, int y, const char *str, int len)
{
  VTBL->draw_string (dpy, d, gc, x, y, str, len, True);
}


int
XDrawImageString (Display *dpy, Drawable d, GC gc, int x, int y,
                  const char *str, int len)
{
  int ascent, descent, dir;
  XCharStruct cs;
  XTextExtents (&VTBL->gc_gcv (gc)->font->metrics, str, len,
                &dir, &ascent, &descent, &cs);
  jwxyz_fill_rect (dpy, d, gc,
                   x + MIN (0, cs.lbearing),
                   y - MAX (0, ascent),

                   /* The +1 here is almost certainly wrong, but BSOD
                      requires it; and only BSOD, fluidballs, juggle
                      and grabclient call XDrawImageString... */
                   MAX (MAX (0, cs.rbearing) -
                        MIN (0, cs.lbearing),
                        cs.width) + 1,
                   MAX (0, ascent) + MAX (0, descent),
                   VTBL->gc_gcv(gc)->background);
  return XDrawString (dpy, d, gc, x, y, str, len);
}


void *
jwxyz_native_font (Font f)
{
  return f->native_font;
}


int
XSetForeground (Display *dpy, GC gc, unsigned long fg)
{
  XGCValues *gcv = VTBL->gc_gcv (gc);
  jwxyz_validate_pixel (dpy, fg, VTBL->gc_depth (gc), gcv->alpha_allowed_p);
  gcv->foreground = fg;
  return 0;
}


int
XSetBackground (Display *dpy, GC gc, unsigned long bg)
{
  XGCValues *gcv = VTBL->gc_gcv (gc);
  jwxyz_validate_pixel (dpy, bg, VTBL->gc_depth (gc), gcv->alpha_allowed_p);
  gcv->background = bg;
  return 0;
}

int
jwxyz_XSetAlphaAllowed (Display *dpy, GC gc, Bool allowed)
{
  VTBL->gc_gcv (gc)->alpha_allowed_p = allowed;
  return 0;
}

int
jwxyz_XSetAntiAliasing (Display *dpy, GC gc, Bool antialias_p)
{
  VTBL->gc_gcv (gc)->antialias_p = antialias_p;
  return 0;
}


int
XSetLineAttributes (Display *dpy, GC gc, unsigned int line_width,
                    int line_style, int cap_style, int join_style)
{
  XGCValues *gcv = VTBL->gc_gcv (gc);
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
  VTBL->gc_gcv (gc)->function = which;
  return 0;
}

int
XSetSubwindowMode (Display *dpy, GC gc, int which)
{
  VTBL->gc_gcv (gc)->subwindow_mode = which;
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

void
visual_rgb_masks (Screen *s, Visual *v, unsigned long *red_mask,
                  unsigned long *green_mask, unsigned long *blue_mask)
{
  *red_mask = v->red_mask;
  *green_mask = v->green_mask;
  *blue_mask = v->blue_mask;
}

int
visual_pixmap_depth (Screen *s, Visual *v)
{
  return 32;
}

int
screen_number (Screen *screen)
{
  return 0;
}

// declared in utils/grabclient.h
Bool
use_subwindow_mode_p (Screen *screen, Window window)
{
  return False;
}

#endif /* HAVE_JWXYZ */
