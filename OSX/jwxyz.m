/* xscreensaver, Copyright (c) 1991-2009 Jamie Zawinski <jwz@jwz.org>
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
   Xlib and that do Cocoa-ish things that bear some resemblance to the
   things that Xlib might have done.
 */

#import <stdlib.h>
#import <stdint.h>
#import <Cocoa/Cocoa.h>
#import "jwxyz.h"
#import "jwxyz-timers.h"

#undef  Assert
#define Assert(C,S) do { \
  if (!(C)) { \
    NSLog(@"%s",S); \
    abort(); \
  }} while(0)

# undef MAX
# undef MIN
# define MAX(a,b) ((a)>(b)?(a):(b))
# define MIN(a,b) ((a)<(b)?(a):(b))


struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  CGContextRef cgc;
  CGRect frame;
  union {
    struct {
      NSView *view;
      unsigned long background;
    } window;
    struct {
      int depth;
    } pixmap;
  };
};

struct jwxyz_Display {
  Window main_window;
  Screen *screen;
  struct jwxyz_sources_data *timers_data;

  CGDirectDisplayID cgdpy;  /* ...of the one and only Window, main_window.
                               This can change if the window is dragged to
                               a different screen. */

  CGColorSpaceRef colorspace;  /* Color space of this screen.  We tag all of
                                  our images with this to avoid translation
                                  when rendering. */
};

struct jwxyz_Screen {
  Display *dpy;
  Visual *visual;
};

struct jwxyz_GC {
  XGCValues gcv;
  unsigned int depth;
  CGImageRef clip_mask;  // CGImage copy of the Pixmap in gcv.clip_mask
};

struct jwxyz_Font {
  char *ps_name;
  NSFont *nsfont;
  float size;   // points

  // In X11, "Font" is just an ID, and "XFontStruct" contains the metrics.
  // But we need the metrics on both of them, so they go here.
  XFontStruct metrics;
};


Display *
jwxyz_make_display (void *nsview_arg)
{
  NSView *view = (NSView *) nsview_arg;
  if (!view) abort();

  Display *d = (Display *) calloc (1, sizeof(*d));
  d->screen = (Screen *) calloc (1, sizeof(Screen));
  d->screen->dpy = d;
  
  Visual *v = (Visual *) calloc (1, sizeof(Visual));
  v->class      = TrueColor;
  v->red_mask   = 0x00FF0000;
  v->green_mask = 0x0000FF00;
  v->blue_mask  = 0x000000FF;
  v->bits_per_rgb = 8;
  d->screen->visual = v;
  
  d->timers_data = jwxyz_sources_init (XtDisplayToApplicationContext (d));


  Window w = (Window) calloc (1, sizeof(*w));
  w->type = WINDOW;
  // kludge! this needs to be set late, so we do it in XClearWindow!
  // w->cgc = [[[view window] graphicsContext] graphicsPort];
  w->cgc = 0;
  w->window.view = view;
  CFRetain (w->window.view);   // needed for garbage collection?
  w->window.background = BlackPixel(0,0);

  d->main_window = w;

  [view lockFocus];
  w->cgc = [[[view window] graphicsContext] graphicsPort];
  [view unlockFocus];

  jwxyz_window_resized (d, w);

  return d;
}

void
jwxyz_free_display (Display *dpy)
{
  jwxyz_XtRemoveInput_all (dpy);
  // #### jwxyz_XtRemoveTimeOut_all ();
  
  free (dpy->screen->visual);
  free (dpy->screen);
  free (dpy->main_window);
  free (dpy);
}


void *
jwxyz_window_view (Window w)
{
  Assert (w->type == WINDOW, "not a window");
  return w->window.view;
}

/* Call this when the View changes size or position.
 */
void
jwxyz_window_resized (Display *dpy, Window w)
{
  Assert (w->type == WINDOW, "not a window");
  NSRect r = [w->window.view frame];
  w->frame.origin.x    = r.origin.x;   // NSRect -> CGRect
  w->frame.origin.y    = r.origin.y;
  w->frame.size.width  = r.size.width;
  w->frame.size.height = r.size.height;

  // Figure out which screen the window is currently on.
  {
    int wx, wy;
    XTranslateCoordinates (dpy, w, NULL, 0, 0, &wx, &wy, NULL);
    CGPoint p;
    p.x = wx;
    p.y = wy;
    CGDisplayCount n;
    dpy->cgdpy = 0;
    CGGetDisplaysWithPoint (p, 1, &dpy->cgdpy, &n);
    Assert (dpy->cgdpy, "unable to find CGDisplay");
  }

#if 0
  {
    // Figure out this screen's colorspace, and use that for every CGImage.
    //
    CMProfileRef profile = 0;
    CMGetProfileByAVID ((CMDisplayIDType) dpy->cgdpy, &profile);
    Assert (profile, "unable to find colorspace profile");
    dpy->colorspace = CGColorSpaceCreateWithPlatformColorSpace (profile);
    Assert (dpy->colorspace, "unable to find colorspace");
  }
# else

  // WTF?  It's faster if we *do not* use the screen's colorspace!
  //
  dpy->colorspace = CGColorSpaceCreateDeviceRGB();
# endif
}


jwxyz_sources_data *
display_sources_data (Display *dpy)
{
  return dpy->timers_data;
}


Window
XRootWindow (Display *dpy, int screen)
{
  return dpy->main_window;
}

Screen *
XDefaultScreenOfDisplay (Display *dpy)
{
  return dpy->screen;
}

Visual *
XDefaultVisualOfScreen (Screen *screen)
{
  return screen->visual;
}

Display *
XDisplayOfScreen (Screen *s)
{
  return s->dpy;
}

int
XDisplayNumberOfScreen (Screen *s)
{
  return 0;
}

int
XScreenNumberOfScreen (Screen *s)
{
  return 0;
}

int
XDisplayWidth (Display *dpy, int screen)
{
  return (int) dpy->main_window->frame.size.width;
}

int
XDisplayHeight (Display *dpy, int screen)
{
  return (int) dpy->main_window->frame.size.height;
}


static void
validate_pixel (unsigned long pixel, unsigned int depth, BOOL alpha_allowed_p)
{
  if (depth == 1)
    Assert ((pixel == 0 || pixel == 1), "bogus mono pixel");
  else if (!alpha_allowed_p)
    Assert (((pixel & BlackPixel(0,0)) == BlackPixel(0,0)),
            "bogus color pixel");
}


static void
set_color (CGContextRef cgc, unsigned long argb, unsigned int depth,
           BOOL alpha_allowed_p, BOOL fill_p)
{
  validate_pixel (argb, depth, alpha_allowed_p);
  if (depth == 1) {
    if (fill_p)
      CGContextSetGrayFillColor   (cgc, (argb ? 1.0 : 0.0), 1.0);
    else
      CGContextSetGrayStrokeColor (cgc, (argb ? 1.0 : 0.0), 1.0);
  } else {
    float a = ((argb >> 24) & 0xFF) / 255.0;
    float r = ((argb >> 16) & 0xFF) / 255.0;
    float g = ((argb >>  8) & 0xFF) / 255.0;
    float b = ((argb      ) & 0xFF) / 255.0;
    if (fill_p)
      CGContextSetRGBFillColor   (cgc, r, g, b, a);
    else
      CGContextSetRGBStrokeColor (cgc, r, g, b, a);
  }
}

static void
set_line_mode (CGContextRef cgc, XGCValues *gcv)
{
  CGContextSetLineWidth (cgc, gcv->line_width ? gcv->line_width : 1);
  CGContextSetLineJoin  (cgc,
                         gcv->join_style == JoinMiter ? kCGLineJoinMiter :
                         gcv->join_style == JoinRound ? kCGLineJoinRound :
                         kCGLineJoinBevel);
  CGContextSetLineCap   (cgc, 
                         gcv->cap_style == CapNotLast ? kCGLineCapButt  :
                         gcv->cap_style == CapButt    ? kCGLineCapButt  :
                         gcv->cap_style == CapRound   ? kCGLineCapRound :
                         kCGLineCapSquare);
}

static void
set_clip_mask (Drawable d, GC gc)
{
  Assert (!!gc->gcv.clip_mask == !!gc->clip_mask, "GC clip mask mixup");

  Pixmap p = gc->gcv.clip_mask;
  if (!p) return;
  Assert (p->type == PIXMAP, "not a pixmap");

  CGRect wr = d->frame;
  CGRect to;
  to.origin.x    = wr.origin.x + gc->gcv.clip_x_origin;
  to.origin.y    = wr.origin.y + wr.size.height - gc->gcv.clip_y_origin
                    - p->frame.size.height;
  to.size.width  = p->frame.size.width;
  to.size.height = p->frame.size.height;

  CGContextClipToMask (d->cgc, to, gc->clip_mask);
}


/* Pushes a GC context; sets BlendMode and ClipMask.
 */
static void
push_gc (Drawable d, GC gc)
{
  CGContextRef cgc = d->cgc;
  CGContextSaveGState (cgc);

  switch (gc->gcv.function) {
    case GXset:
    case GXclear:
    case GXcopy:/*CGContextSetBlendMode (cgc, kCGBlendModeNormal);*/   break;
    case GXxor:   CGContextSetBlendMode (cgc, kCGBlendModeDifference); break;
    case GXor:    CGContextSetBlendMode (cgc, kCGBlendModeLighten);    break;
    case GXand:   CGContextSetBlendMode (cgc, kCGBlendModeDarken);     break;
    default: abort(); break;
  }

  if (gc->gcv.clip_mask)
    set_clip_mask (d, gc);
}

#define pop_gc(d,gc) CGContextRestoreGState ((d)->cgc)


/* Pushes a GC context; sets BlendMode, ClipMask, Fill, and Stroke colors.
 */
static void
push_color_gc (Drawable d, GC gc, unsigned long color, 
               BOOL antialias_p, Bool fill_p)
{
  push_gc (d, gc);

  int depth = gc->depth;
  switch (gc->gcv.function) {
    case GXset:   color = (depth == 1 ? 1 : WhitePixel(0,0)); break;
    case GXclear: color = (depth == 1 ? 0 : BlackPixel(0,0)); break;
  }

  CGContextRef cgc = d->cgc;

  set_color (cgc, color, depth, gc->gcv.alpha_allowed_p, fill_p);
  CGContextSetShouldAntialias (cgc, antialias_p);
}


/* Pushes a GC context; sets Fill and Stroke colors to the foreground color.
 */
static void
push_fg_gc (Drawable d, GC gc, Bool fill_p)
{
  push_color_gc (d, gc, gc->gcv.foreground, gc->gcv.antialias_p, fill_p);
}

/* Pushes a GC context; sets Fill and Stroke colors to the background color.
 */
static void
push_bg_gc (Drawable d, GC gc, Bool fill_p)
{
  push_color_gc (d, gc, gc->gcv.background, gc->gcv.antialias_p, fill_p);
}



/* You've got to be fucking kidding me!

   It is *way* faster to draw points by creating and drawing a 1x1 CGImage
   with repeated calls to CGContextDrawImage than it is to make a single
   call to CGContextFillRects()!

   I still wouldn't call it *fast*, however...
 */
#define XDRAWPOINTS_IMAGES

int
XDrawPoints (Display *dpy, Drawable d, GC gc, 
             XPoint *points, int count, int mode)
{
  int i;
  CGRect wr = d->frame;

  push_fg_gc (d, gc, YES);

# ifdef XDRAWPOINTS_IMAGES

  unsigned int argb = gc->gcv.foreground;
  validate_pixel (argb, gc->depth, gc->gcv.alpha_allowed_p);
  if (gc->depth == 1)
    argb = (gc->gcv.foreground ? WhitePixel(0,0) : BlackPixel(0,0));

  CGDataProviderRef prov = CGDataProviderCreateWithData (NULL, &argb, 4, NULL);
  CGImageRef cgi = CGImageCreate (1, 1,
                                  8, 32, 4,
                                  dpy->colorspace, 
                                  /* Host-ordered, since we're using the
                                     address of an int as the color data. */
                                  (kCGImageAlphaNoneSkipFirst | 
                                   kCGBitmapByteOrder32Host),
                                  prov, 
                                  NULL,  /* decode[] */
                                  NO, /* interpolate */
                                  kCGRenderingIntentDefault);
  CGDataProviderRelease (prov);

  CGContextRef cgc = d->cgc;
  CGRect rect;
  rect.size.width = rect.size.height = 1;
  for (i = 0; i < count; i++) {
    if (i > 0 && mode == CoordModePrevious) {
      rect.origin.x += points->x;
      rect.origin.x -= points->y;
    } else {
      rect.origin.x = wr.origin.x + points->x;
      rect.origin.y = wr.origin.y + wr.size.height - points->y - 1;
    }

    //Assert (CGImageGetColorSpace (cgi) == dpy->colorspace, "bad colorspace");
    CGContextDrawImage (cgc, rect, cgi);
    points++;
  }

  CGImageRelease (cgi);

# else /* ! XDRAWPOINTS_IMAGES */

  CGRect *rects = (CGRect *) malloc (count * sizeof(CGRect));
  CGRect *r = rects;
  
  for (i = 0; i < count; i++) {
    r->size.width = r->size.height = 1;
    if (i > 0 && mode == CoordModePrevious) {
      r->origin.x = r[-1].origin.x + points->x;
      r->origin.y = r[-1].origin.x - points->y;
    } else {
      r->origin.x = wr.origin.x + points->x;
      r->origin.y = wr.origin.y + wr.size.height - points->y;
    }
    points++;
    r++;
  }

  CGContextFillRects (d->cgc, rects, count);
  free (rects);

# endif /* ! XDRAWPOINTS_IMAGES */

  pop_gc (d, gc);

  return 0;
}


int
XDrawPoint (Display *dpy, Drawable d, GC gc, int x, int y)
{
  XPoint p;
  p.x = x;
  p.y = y;
  return XDrawPoints (dpy, d, gc, &p, 1, CoordModeOrigin);
}


static void draw_rect (Display *, Drawable, GC, 
                       int x, int y, unsigned int width, unsigned int height, 
                       BOOL foreground_p, BOOL fill_p);

int
XCopyArea (Display *dpy, Drawable src, Drawable dst, GC gc, 
           int src_x, int src_y, 
           unsigned int width, unsigned int height, 
           int dst_x, int dst_y)
{
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((src_x  < 65535 && src_x  > -65535), "improbably large src_x");
  Assert ((src_y  < 65535 && src_y  > -65535), "improbably large src_y");
  Assert ((dst_x  < 65535 && dst_x  > -65535), "improbably large dst_x");
  Assert ((dst_y  < 65535 && dst_y  > -65535), "improbably large dst_y");

  if (width == 0 || height == 0)
    return 0;

  if (gc && (gc->gcv.function == GXset ||
             gc->gcv.function == GXclear)) {
    // "set" and "clear" are dumb drawing modes that ignore the source
    // bits and just draw solid rectangles.
    set_color (dst->cgc, (gc->gcv.function == GXset
                          ? (gc->depth == 1 ? 1 : WhitePixel(0,0))
                          : (gc->depth == 1 ? 0 : BlackPixel(0,0))),
               gc->depth, gc->gcv.alpha_allowed_p, YES);
    draw_rect (dpy, dst, 0, dst_x, dst_y, width, height, YES, YES);
    return 0;
  }

  CGRect src_frame, dst_frame;   // Sizes and origins of the two drawables
  CGRect src_rect,  dst_rect;    // The two rects to draw, clipped to the
                                 //  bounds of their drawables.
  BOOL clipped = NO;             // Whether we did any clipping of the rects.

  src_frame = src->frame;
  dst_frame = dst->frame;
  
  // Initialize src_rect...
  //
  src_rect.origin.x    = src_frame.origin.x + src_x;
  src_rect.origin.y    = src_frame.origin.y + src_frame.size.height
                          - height - src_y;
  if (src_rect.origin.y < -65535) Assert(0, "src.origin.y went nuts");
  src_rect.size.width  = width;
  src_rect.size.height = height;
  
  // Initialize dst_rect...
  //
  dst_rect.origin.x    = dst_frame.origin.x + dst_x;
  dst_rect.origin.y    = dst_frame.origin.y + dst_frame.size.height
                          - height - dst_y;
  if (dst_rect.origin.y < -65535) Assert(0, "dst.origin.y went nuts");
  dst_rect.size.width  = width;
  dst_rect.size.height = height;
  
  // Clip rects to frames...
  //
//  CGRect orig_src_rect = src_rect;
  CGRect orig_dst_rect = dst_rect;

# define CLIP(THIS,THAT,VAL,SIZE) do { \
  float off = THIS##_rect.origin.VAL; \
  if (off < 0) { \
    clipped = YES; \
    THIS##_rect.size.SIZE  += off; \
    THAT##_rect.size.SIZE  += off; \
    THIS##_rect.origin.VAL -= off; \
    THAT##_rect.origin.VAL -= off; \
  } \
  off = (( THIS##_rect.origin.VAL +  THIS##_rect.size.SIZE) - \
         (THIS##_frame.origin.VAL + THIS##_frame.size.SIZE)); \
  if (off > 0) { \
    clipped = YES; \
    THIS##_rect.size.SIZE  -= off; \
    THAT##_rect.size.SIZE  -= off; \
  }} while(0)

  CLIP (dst, src, x, width);
  CLIP (dst, src, y, height);
  CLIP (src, dst, x, width);
  CLIP (src, dst, y, height);
# undef CLIP

#if 0
  Assert (src_rect.size.width  == dst_rect.size.width, "width out of sync");
  Assert (src_rect.size.height == dst_rect.size.height, "height out of sync");
  Assert (src_rect.origin.x >= 0 && src_rect.origin.y >= 0, "clip failed src_x");
  Assert (dst_rect.origin.x >= 0 && dst_rect.origin.y >= 0, "clip failed dst_x");
  Assert (src_rect.origin.y >= 0 && src_rect.origin.y >= 0, "clip failed src_y");
  Assert (dst_rect.origin.y >= 0 && dst_rect.origin.y >= 0, "clip failed dst_y");
  Assert (src_rect.origin.x  + src_rect.size.width <=
          src_frame.origin.x + src_frame.size.width, "clip failed src_width");
#endif
  
  if (src_rect.size.width <= 0 || src_rect.size.height <= 0)
    return 0;
  
  NSObject *releaseme = 0;
  CGImageRef cgi;
  BOOL mask_p = NO;

  if (src->type == PIXMAP) {

    // get a CGImage out of the pixmap CGContext -- it's the whole pixmap,
    // but it presumably shares the data pointer instead of copying it.
    cgi = CGBitmapContextCreateImage (src->cgc);

    // if doing a sub-rect, trim it down.
    if (src_rect.origin.x    != src_frame.origin.x   ||
        src_rect.origin.y    != src_frame.origin.y   ||
        src_rect.size.width  != src_frame.size.width ||
        src_rect.size.height != src_frame.size.height) {
      // #### I don't understand why this is needed...
      src_rect.origin.y = (src_frame.size.height -
                           src_rect.size.height - src_rect.origin.y);
      // This does not copy image data, so it should be fast.
      CGImageRef cgi2 = CGImageCreateWithImageInRect (cgi, src_rect);
      CGImageRelease (cgi);
      cgi = cgi2;
    }

    if (src->pixmap.depth == 1)
      mask_p = YES;

  } else { /* (src->type == WINDOW) */
    
    NSRect nsfrom;
    nsfrom.origin.x    = src_rect.origin.x;
    nsfrom.origin.y    = src_rect.origin.y;
    nsfrom.size.width  = src_rect.size.width;
    nsfrom.size.height = src_rect.size.height;

#if 1
    // get the bits (desired sub-rectangle) out of the NSView via Cocoa.
    //
    NSBitmapImageRep *bm = [NSBitmapImageRep alloc];
    [bm initWithFocusedViewRect:nsfrom];
    unsigned char *data = [bm bitmapData];
    int bps = [bm bitsPerSample];
    int bpp = [bm bitsPerPixel];
    int bpl = [bm bytesPerRow];
    releaseme = bm;
#endif

#if 0
    // QuickDraw way (doesn't work, need NSQuickDrawView)
    PixMapHandle pix = GetPortPixMap([src->window.view qdPort]);
    char **data = GetPortPixMap (pix);
    int bps = 8;
    int bpp = 32;
    int bpl = GetPixRowBytes (pix) & 0x3FFF;
#endif

#if 0
    // get the bits (desired sub-rectangle) out of the raw frame buffer.
    // (This renders wrong, and appears to be even slower anyway.)
    //
    int window_x, window_y;
    XTranslateCoordinates (dpy, src, NULL, 0, 0, &window_x, &window_y, NULL);
    window_x += nsfrom.origin.x;
    window_y += (dst->frame.size.height
                 - (nsfrom.origin.y + nsfrom.size.height));

    unsigned char *data = (unsigned char *) 
      CGDisplayAddressForPosition (dpy->cgdpy, window_x, window_y);
    int bps = CGDisplayBitsPerSample (dpy->cgdpy);
    int bpp = CGDisplayBitsPerPixel (dpy->cgdpy);
    int bpl = CGDisplayBytesPerRow (dpy->cgdpy);

#endif

    // create a CGImage from those bits

    CGDataProviderRef prov =
      CGDataProviderCreateWithData (NULL, data, bpl * nsfrom.size.height,
                                    NULL);
    cgi = CGImageCreate (src_rect.size.width, src_rect.size.height,
                         bps, bpp, bpl,
                         dpy->colorspace, 
                         /* Use whatever default bit ordering we got from
                            initWithFocusedViewRect.  I would have assumed
                            that it was (kCGImageAlphaNoneSkipFirst |
                            kCGBitmapByteOrder32Host), but on Intel,
                            it's not!
                          */
                         0,
                         prov, 
                         NULL,  /* decode[] */
                         NO, /* interpolate */
                         kCGRenderingIntentDefault);
    //Assert (CGImageGetColorSpace (cgi) == dpy->colorspace, "bad colorspace");
    CGDataProviderRelease (prov);
  }

  if (mask_p) {		// src depth == 1

    push_bg_gc (dst, gc, YES);

    // fill the destination rectangle with solid background...
    CGContextFillRect (dst->cgc, orig_dst_rect);

    // then fill in a solid rectangle of the fg color, using the image as an
    // alpha mask.  (the image has only values of BlackPixel or WhitePixel.)
    set_color (dst->cgc, gc->gcv.foreground, gc->depth, 
               gc->gcv.alpha_allowed_p, YES);
    CGContextClipToMask (dst->cgc, dst_rect, cgi);
    CGContextFillRect (dst->cgc, dst_rect);

    pop_gc (dst, gc);

  } else {		// src depth > 1

    push_gc (dst, gc);

    // If either the src or dst rects did not lie within their drawables,
    // then we have adjusted both the src and dst rects to account for 
    // the clipping; that means we need to first clear to the background,
    // so that clipped bits end up in the bg color instead of simply not
    // being copied.
    //
    if (clipped) {
      set_color (dst->cgc, gc->gcv.background, gc->depth, 
                 gc->gcv.alpha_allowed_p, YES);
      CGContextFillRect (dst->cgc, orig_dst_rect);
    }

    // copy the CGImage onto the destination CGContext
    //Assert (CGImageGetColorSpace (cgi) == dpy->colorspace, "bad colorspace");
    CGContextDrawImage (dst->cgc, dst_rect, cgi);

    pop_gc (dst, gc);
  }
  
  CGImageRelease (cgi);
  if (releaseme) [releaseme release];
  return 0;
}


int
XCopyPlane (Display *dpy, Drawable src, Drawable dest, GC gc,
            int src_x, int src_y,
            unsigned width, int height,
            int dest_x, int dest_y, unsigned long plane)
{
  Assert ((gc->depth == 1 || plane == 1), "hairy plane mask!");
  
  // This isn't right: XCopyPlane() is supposed to map 1/0 to fg/bg,
  // not to white/black.
  return XCopyArea (dpy, src, dest, gc,
                    src_x, src_y, width, height, dest_x, dest_y);
}


int
XDrawLine (Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2)
{
  // when drawing a zero-length line, obey line-width and cap-style.
  if (x1 == x2 && y1 == y2) {
    int w = gc->gcv.line_width;
    x1 -= w/2;
    y1 -= w/2;
    if (gc->gcv.line_width > 1 && gc->gcv.cap_style == CapRound)
      return XFillArc (dpy, d, gc, x1, y1, w, w, 0, 360*64);
    else
      return XFillRectangle (dpy, d, gc, x1, y1, w, w);
  }
  
  CGRect wr = d->frame;
  NSPoint p;
  p.x = wr.origin.x + x1;
  p.y = wr.origin.y + wr.size.height - y1;

  push_fg_gc (d, gc, NO);

  set_line_mode (d->cgc, &gc->gcv);
  CGContextBeginPath (d->cgc);
  CGContextMoveToPoint (d->cgc, p.x, p.y);
  p.x = wr.origin.x + x2;
  p.y = wr.origin.y + wr.size.height - y2;
  CGContextAddLineToPoint (d->cgc, p.x, p.y);
  CGContextStrokePath (d->cgc);
  pop_gc (d, gc);
  return 0;
}

int
XDrawLines (Display *dpy, Drawable d, GC gc, XPoint *points, int count,
            int mode)
{
  int i;
  NSPoint p;
  CGRect wr = d->frame;
  push_fg_gc (d, gc, NO);
  set_line_mode (d->cgc, &gc->gcv);
  
  // if the first and last points coincide, use closepath to get
  // the proper line-joining.
  BOOL closed_p = (points[0].x == points[count-1].x &&
                   points[0].y == points[count-1].y);
  if (closed_p) count--;
  
  p.x = wr.origin.x + points->x;
  p.y = wr.origin.y + wr.size.height - points->y;
  points++;
  CGContextBeginPath (d->cgc);
  CGContextMoveToPoint (d->cgc, p.x, p.y);
  for (i = 1; i < count; i++) {
    if (mode == CoordModePrevious) {
      p.x += points->x;
      p.y -= points->y;
    } else {
      p.x = wr.origin.x + points->x;
      p.y = wr.origin.y + wr.size.height - points->y;
    }
    CGContextAddLineToPoint (d->cgc, p.x, p.y);
    points++;
  }
  if (closed_p) CGContextClosePath (d->cgc);
  CGContextStrokePath (d->cgc);
  pop_gc (d, gc);
  return 0;
}


int
XDrawSegments (Display *dpy, Drawable d, GC gc, XSegment *segments, int count)
{
  int i;
  CGRect wr = d->frame;

  push_fg_gc (d, gc, NO);
  set_line_mode (d->cgc, &gc->gcv);
  CGContextBeginPath (d->cgc);
  for (i = 0; i < count; i++) {
    CGContextMoveToPoint    (d->cgc, 
                             wr.origin.x + segments->x1,
                             wr.origin.y + wr.size.height - segments->y1);
    CGContextAddLineToPoint (d->cgc,
                             wr.origin.x + segments->x2,
                             wr.origin.y + wr.size.height - segments->y2);
    segments++;
  }
  CGContextStrokePath (d->cgc);
  pop_gc (d, gc);
  return 0;
}


int
XClearWindow (Display *dpy, Window win)
{
  Assert (win->type == WINDOW, "not a window");
  CGRect wr = win->frame;
  return XClearArea (dpy, win, 0, 0, wr.size.width, wr.size.height, 0);
}

int
XSetWindowBackground (Display *dpy, Window w, unsigned long pixel)
{
  Assert (w->type == WINDOW, "not a window");
  validate_pixel (pixel, 32, NO);
  w->window.background = pixel;
  return 0;
}

static void
draw_rect (Display *dpy, Drawable d, GC gc, 
           int x, int y, unsigned int width, unsigned int height, 
           BOOL foreground_p, BOOL fill_p)
{
  CGRect wr = d->frame;
  CGRect r;
  r.origin.x = wr.origin.x + x;
  r.origin.y = wr.origin.y + wr.size.height - y - height;
  r.size.width = width;
  r.size.height = height;

  if (gc) {
    if (foreground_p)
      push_fg_gc (d, gc, fill_p);
    else
      push_bg_gc (d, gc, fill_p);
  }

  if (fill_p)
    CGContextFillRect (d->cgc, r);
  else {
    if (gc)
      set_line_mode (d->cgc, &gc->gcv);
    CGContextStrokeRect (d->cgc, r);
  }

  if (gc)
    pop_gc (d, gc);
}


int
XFillRectangle (Display *dpy, Drawable d, GC gc, int x, int y, 
                unsigned int width, unsigned int height)
{
  draw_rect (dpy, d, gc, x, y, width, height, YES, YES);
  return 0;
}

int
XDrawRectangle (Display *dpy, Drawable d, GC gc, int x, int y, 
                unsigned int width, unsigned int height)
{
  draw_rect (dpy, d, gc, x, y, width, height, YES, NO);
  return 0;
}

int
XFillRectangles (Display *dpy, Drawable d, GC gc, XRectangle *rects, int n)
{
  CGRect wr = d->frame;
  int i;
  push_fg_gc (d, gc, YES);
  for (i = 0; i < n; i++) {
    CGRect r;
    r.origin.x = wr.origin.x + rects->x;
    r.origin.y = wr.origin.y + wr.size.height - rects->y - rects->height;
    r.size.width = rects->width;
    r.size.height = rects->height;
    CGContextFillRect (d->cgc, r);
    rects++;
  }
  pop_gc (d, gc);
  return 0;
}


int
XClearArea (Display *dpy, Window win, int x, int y, int w, int h, Bool exp)
{
  Assert (win->type == WINDOW, "not a window");
  set_color (win->cgc, win->window.background, 32, NO, YES);
  draw_rect (dpy, win, 0, x, y, w, h, NO, YES);
  return 0;
}


int
XFillPolygon (Display *dpy, Drawable d, GC gc, 
              XPoint *points, int npoints, int shape, int mode)
{
  CGRect wr = d->frame;
  int i;
  push_fg_gc (d, gc, YES);
  CGContextBeginPath (d->cgc);
  for (i = 0; i < npoints; i++) {
    float x, y;
    if (i > 0 && mode == CoordModePrevious) {
      x += points[i].x;
      y -= points[i].y;
    } else {
      x = wr.origin.x + points[i].x;
      y = wr.origin.y + wr.size.height - points[i].y;
    }
        
    if (i == 0)
      CGContextMoveToPoint (d->cgc, x, y);
    else
      CGContextAddLineToPoint (d->cgc, x, y);
  }
  CGContextClosePath (d->cgc);
  if (gc->gcv.fill_rule == EvenOddRule)
    CGContextEOFillPath (d->cgc);
  else
    CGContextFillPath (d->cgc);
  pop_gc (d, gc);
  return 0;
}

#define radians(DEG) ((DEG) * M_PI / 180.0)
#define degrees(RAD) ((RAD) * 180.0 / M_PI)

static int
draw_arc (Display *dpy, Drawable d, GC gc, int x, int y, 
          unsigned int width, unsigned int height, int angle1, int angle2,
          BOOL fill_p)
{
  CGRect wr = d->frame;
  CGRect bound;
  bound.origin.x = wr.origin.x + x;
  bound.origin.y = wr.origin.y + wr.size.height - y - height;
  bound.size.width = width;
  bound.size.height = height;
  
  CGPoint ctr;
  ctr.x = bound.origin.x + bound.size.width /2;
  ctr.y = bound.origin.y + bound.size.height/2;
  
  float r1 = radians (angle1/64.0);
  float r2 = radians (angle2/64.0) + r1;
  BOOL clockwise = angle2 < 0;
  BOOL closed_p = (angle2 >= 360*64 || angle2 <= -360*64);
  
  push_fg_gc (d, gc, fill_p);

  CGContextBeginPath (d->cgc);
  
  CGContextSaveGState(d->cgc);
  CGContextTranslateCTM (d->cgc, ctr.x, ctr.y);
  CGContextScaleCTM (d->cgc, width/2.0, height/2.0);
  if (fill_p)
    CGContextMoveToPoint (d->cgc, 0, 0);

  CGContextAddArc (d->cgc, 0.0, 0.0, 1, r1, r2, clockwise);
  CGContextRestoreGState (d->cgc);  // restore before stroke, for line width

  if (closed_p)
    CGContextClosePath (d->cgc); // for proper line joining
  
  if (fill_p) {
    CGContextFillPath (d->cgc);
  } else {
    set_line_mode (d->cgc, &gc->gcv);
    CGContextStrokePath (d->cgc);
  }

  pop_gc (d, gc);
  return 0;
}

int
XDrawArc (Display *dpy, Drawable d, GC gc, int x, int y, 
          unsigned int width, unsigned int height, int angle1, int angle2)
{
  return draw_arc (dpy, d, gc, x, y, width, height, angle1, angle2, NO);
}

int
XFillArc (Display *dpy, Drawable d, GC gc, int x, int y, 
          unsigned int width, unsigned int height, int angle1, int angle2)
{
  return draw_arc (dpy, d, gc, x, y, width, height, angle1, angle2, YES);
}

int
XDrawArcs (Display *dpy, Drawable d, GC gc, XArc *arcs, int narcs)
{
  int i;
  for (i = 0; i < narcs; i++)
    draw_arc (dpy, d, gc, 
              arcs[i].x, arcs[i].y, 
              arcs[i].width, arcs[i].height, 
              arcs[i].angle1, arcs[i].angle2,
              NO);
  return 0;
}

int
XFillArcs (Display *dpy, Drawable d, GC gc, XArc *arcs, int narcs)
{
  int i;
  for (i = 0; i < narcs; i++)
    draw_arc (dpy, d, gc, 
              arcs[i].x, arcs[i].y, 
              arcs[i].width, arcs[i].height, 
              arcs[i].angle1, arcs[i].angle2,
              YES);
  return 0;
}


static void
gcv_defaults (XGCValues *gcv, int depth)
{
  memset (gcv, 0, sizeof(*gcv));
  gcv->function   = GXcopy;
  gcv->foreground = (depth == 1 ? 1 : WhitePixel(0,0));
  gcv->background = (depth == 1 ? 0 : BlackPixel(0,0));
  gcv->line_width = 1;
  gcv->cap_style  = CapNotLast;
  gcv->join_style = JoinMiter;
  gcv->fill_rule  = EvenOddRule;

  gcv->alpha_allowed_p = NO;
  gcv->antialias_p     = YES;
}

static void
set_gcv (GC gc, XGCValues *from, unsigned long mask)
{
  if (mask & GCFunction)	gc->gcv.function	= from->function;
  if (mask & GCForeground)	gc->gcv.foreground	= from->foreground;
  if (mask & GCBackground)	gc->gcv.background	= from->background;
  if (mask & GCLineWidth)	gc->gcv.line_width	= from->line_width;
  if (mask & GCCapStyle)	gc->gcv.cap_style	= from->cap_style;
  if (mask & GCJoinStyle)	gc->gcv.join_style	= from->join_style;
  if (mask & GCFillRule)	gc->gcv.fill_rule	= from->fill_rule;
  if (mask & GCClipXOrigin)	gc->gcv.clip_x_origin	= from->clip_x_origin;
  if (mask & GCClipYOrigin)	gc->gcv.clip_y_origin	= from->clip_y_origin;
  if (mask & GCSubwindowMode)	gc->gcv.subwindow_mode	= from->subwindow_mode;
  
  if (mask & GCClipMask)	XSetClipMask (0, gc, from->clip_mask);
  if (mask & GCFont)		XSetFont (0, gc, from->font);

  if (mask & GCForeground) validate_pixel (from->foreground, gc->depth,
                                           gc->gcv.alpha_allowed_p);
  if (mask & GCBackground) validate_pixel (from->background, gc->depth,
                                           gc->gcv.alpha_allowed_p);
    
  if (mask & GCLineStyle)	abort();
  if (mask & GCPlaneMask)	abort();
  if (mask & GCFillStyle)	abort();
  if (mask & GCTile)		abort();
  if (mask & GCStipple)		abort();
  if (mask & GCTileStipXOrigin)	abort();
  if (mask & GCTileStipYOrigin)	abort();
  if (mask & GCGraphicsExposures) abort();
  if (mask & GCDashOffset)	abort();
  if (mask & GCDashList)	abort();
  if (mask & GCArcMode)		abort();
}


GC
XCreateGC (Display *dpy, Drawable d, unsigned long mask, XGCValues *xgcv)
{
  struct jwxyz_GC *gc = (struct jwxyz_GC *) calloc (1, sizeof(*gc));
  if (d->type == WINDOW) {
    gc->depth = 32;
  } else { /* (d->type == PIXMAP) */
    gc->depth = d->pixmap.depth;
  }

  gcv_defaults (&gc->gcv, gc->depth);
  set_gcv (gc, xgcv, mask);
  return gc;
}

int
XChangeGC (Display *dpy, GC gc, unsigned long mask, XGCValues *gcv)
{
  set_gcv (gc, gcv, mask);
  return 0;
}


int
XFreeGC (Display *dpy, GC gc)
{
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);

  Assert (!!gc->gcv.clip_mask == !!gc->clip_mask, "GC clip mask mixup");

  if (gc->gcv.clip_mask) {
    XFreePixmap (dpy, gc->gcv.clip_mask);
    CGImageRelease (gc->clip_mask);
  }
  free (gc);
  return 0;
}


Status
XGetWindowAttributes (Display *dpy, Window w, XWindowAttributes *xgwa)
{
  Assert (w->type == WINDOW, "not a window");
  memset (xgwa, 0, sizeof(*xgwa));
  xgwa->x      = w->frame.origin.x;
  xgwa->y      = w->frame.origin.y;
  xgwa->width  = w->frame.size.width;
  xgwa->height = w->frame.size.height;
  xgwa->depth  = 32;
  xgwa->screen = dpy->screen;
  xgwa->visual = dpy->screen->visual;
  return 0;
}

Status
XGetGeometry (Display *dpy, Drawable d, Window *root_ret,
              int *x_ret, int *y_ret, 
              unsigned int *w_ret, unsigned int *h_ret,
              unsigned int *bw_ret, unsigned int *d_ret)
{
  *x_ret    = d->frame.origin.x;
  *y_ret    = d->frame.origin.y;
  *w_ret    = d->frame.size.width;
  *h_ret    = d->frame.size.height;
  *d_ret    = (d->type == WINDOW ? 32 : d->pixmap.depth);
  *root_ret = RootWindow (dpy, 0);
  *bw_ret   = 0;
  return True;
}


Status
XAllocColor (Display *dpy, Colormap cmap, XColor *color)
{
  // store 32 bit ARGB in the pixel field.
  // (The uint32_t is so that 0xFF000000 doesn't become 0xFFFFFFFFFF000000)
  color->pixel = (uint32_t)
                 ((                       0xFF  << 24) |
                  (((color->red   >> 8) & 0xFF) << 16) |
                  (((color->green >> 8) & 0xFF) <<  8) |
                  (((color->blue  >> 8) & 0xFF)      ));
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
    r = (hex[spec[1]] << 4) | hex[spec[2]];
    g = (hex[spec[3]] << 4) | hex[spec[4]];
    b = (hex[spec[5]] << 4) | hex[spec[6]];
  } else if (!strcasecmp(spec,"black")) {
    r = g = b = 0;
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
  validate_pixel (color->pixel, 32, NO);
  unsigned char r = ((color->pixel >> 16) & 0xFF);
  unsigned char g = ((color->pixel >>  8) & 0xFF);
  unsigned char b = ((color->pixel      ) & 0xFF);
  color->red   = (r << 8) | r;
  color->green = (g << 8) | g;
  color->blue  = (b << 8) | b;
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
    abort();
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
  ximage->byte_order = MSBFirst;
  ximage->bitmap_bit_order = ximage->byte_order;
  ximage->bitmap_pad = bitmap_pad;
  ximage->depth = depth;
  ximage->red_mask   = (depth == 1 ? 0 : 0x00FF0000);
  ximage->green_mask = (depth == 1 ? 0 : 0x0000FF00);
  ximage->blue_mask  = (depth == 1 ? 0 : 0x000000FF);
  ximage->bits_per_pixel = (depth == 1 ? 1 : 32);
  ximage->bytes_per_line = bytes_per_line;

  XInitImage (ximage);
  return ximage;
}

XImage *
XSubImage (XImage *from, int x, int y, unsigned int w, unsigned int h)
{
  XImage *to = XCreateImage (0, 0, from->depth, from->format, 0, 0,
                             w, h, from->bitmap_pad, 0);
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
  ret[0].depth = 32;
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


static void
flipbits (unsigned const char *in, unsigned char *out, int length)
{
  static const unsigned char table[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 
    0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 
    0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 
    0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA, 
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 
    0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 
    0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1, 
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 
    0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 
    0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD, 
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 
    0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 
    0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
  };
  while (--length > 0)
    *out++ = table[*in++];
}


int
XPutImage (Display *dpy, Drawable d, GC gc, XImage *ximage,
           int src_x, int src_y, int dest_x, int dest_y,
           unsigned int w, unsigned int h)
{
  CGRect wr = d->frame;

  Assert ((w < 65535), "improbably large width");
  Assert ((h < 65535), "improbably large height");
  Assert ((src_x  < 65535 && src_x  > -65535), "improbably large src_x");
  Assert ((src_y  < 65535 && src_y  > -65535), "improbably large src_y");
  Assert ((dest_x < 65535 && dest_x > -65535), "improbably large dest_x");
  Assert ((dest_y < 65535 && dest_y > -65535), "improbably large dest_y");

  // Clip width and height to the bounds of the Drawable
  //
  if (dest_x + w > wr.size.width) {
    if (dest_x > wr.size.width)
      return 0;
    w = wr.size.width - dest_x;
  }
  if (dest_y + h > wr.size.height) {
    if (dest_y > wr.size.height)
      return 0;
    h = wr.size.height - dest_y;
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

  if (gc && (gc->gcv.function == GXset ||
             gc->gcv.function == GXclear)) {
    // "set" and "clear" are dumb drawing modes that ignore the source
    // bits and just draw solid rectangles.
    set_color (d->cgc, (gc->gcv.function == GXset
                        ? (gc->depth == 1 ? 1 : WhitePixel(0,0))
                        : (gc->depth == 1 ? 0 : BlackPixel(0,0))),
               gc->depth, gc->gcv.alpha_allowed_p, YES);
    draw_rect (dpy, d, 0, dest_x, dest_y, w, h, YES, YES);
    return 0;
  }

  int bpl = ximage->bytes_per_line;
  int bpp = ximage->bits_per_pixel;
  int bsize = bpl * h;
  char *data = ximage->data;

  CGRect r;
  r.origin.x = wr.origin.x + dest_x;
  r.origin.y = wr.origin.y + wr.size.height - dest_y - h;
  r.size.width = w;
  r.size.height = h;

  if (bpp == 32) {

    /* Take advantage of the fact that it's ok for (bpl != w * bpp)
       to create a CGImage from a sub-rectagle of the XImage.
     */
    data += (src_y * bpl) + (src_x * 4);
    CGDataProviderRef prov = 
      CGDataProviderCreateWithData (NULL, data, bsize, NULL);

    CGImageRef cgi = CGImageCreate (w, h,
                                    bpp/4, bpp, bpl,
                                    dpy->colorspace, 
                                    /* Need this for XPMs to have the right
                                       colors, e.g. the logo in "maze". */
                                    (kCGImageAlphaNoneSkipFirst |
                                     kCGBitmapByteOrder32Host),
                                    prov, 
                                    NULL,  /* decode[] */
                                    NO, /* interpolate */
                                    kCGRenderingIntentDefault);
    CGDataProviderRelease (prov);
    //Assert (CGImageGetColorSpace (cgi) == dpy->colorspace, "bad colorspace");
    CGContextDrawImage (d->cgc, r, cgi);
    CGImageRelease (cgi);

  } else {   // (bpp == 1)

    /* To draw a 1bpp image, we use it as a mask and fill two rectangles.

       #### However, the bit order within a byte in a 1bpp XImage is
            the wrong way around from what Quartz expects, so first we
            have to copy the data to reverse it.  Shit!  Maybe it
            would be worthwhile to go through the hacks and #ifdef
            each one that diddles 1bpp XImage->data directly...
     */
    Assert ((src_x % 8) == 0,
            "XPutImage with non-byte-aligned 1bpp X offset not implemented");

    data += (src_y * bpl) + (src_x / 8);   // move to x,y within the data
    unsigned char *flipped = (unsigned char *) malloc (bsize);

    flipbits ((unsigned char *) data, flipped, bsize);

    CGDataProviderRef prov = 
      CGDataProviderCreateWithData (NULL, flipped, bsize, NULL);
    CGImageRef mask = CGImageMaskCreate (w, h, 
                                         1, bpp, bpl,
                                         prov,
                                         NULL,  /* decode[] */
                                         NO); /* interpolate */
    push_fg_gc (d, gc, YES);

    CGContextFillRect (d->cgc, r);			// foreground color
    CGContextClipToMask (d->cgc, r, mask);
    set_color (d->cgc, gc->gcv.background, gc->depth, NO, YES);
    CGContextFillRect (d->cgc, r);			// background color
    pop_gc (d, gc);

    free (flipped);
    CGDataProviderRelease (prov);
    CGImageRelease (mask);
  }

  return 0;
}


XImage *
XGetImage (Display *dpy, Drawable d, int x, int y,
           unsigned int width, unsigned int height,
           unsigned long plane_mask, int format)
{
  const unsigned char *data = 0;
  int depth, ibpp, ibpl, alpha_first_p;
  NSBitmapImageRep *bm = 0;
  
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  if (d->type == PIXMAP) {
    depth = d->pixmap.depth;
    alpha_first_p = 1; // we created it with kCGImageAlphaNoneSkipFirst.
    ibpp = CGBitmapContextGetBitsPerPixel (d->cgc);
    ibpl = CGBitmapContextGetBytesPerRow (d->cgc);
    data = CGBitmapContextGetData (d->cgc);
    Assert (data, "CGBitmapContextGetData failed");
  } else {
    // get the bits (desired sub-rectangle) out of the NSView
    bm = [NSBitmapImageRep alloc];
    NSRect nsfrom;
    nsfrom.origin.x = x;
    nsfrom.origin.y = y;
    nsfrom.size.width = width;
    nsfrom.size.height = height;
    [bm initWithFocusedViewRect:nsfrom];
    depth = 32;
    alpha_first_p = ([bm bitmapFormat] & NSAlphaFirstBitmapFormat);
    ibpp = [bm bitsPerPixel];
    ibpl = [bm bytesPerRow];
    data = [bm bitmapData];
    Assert (data, "NSBitmapImageRep initWithFocusedViewRect failed");
  }
  
  // data points at (x,y) with ibpl rowstride.  ignore x,y from now on.
  data += (y * ibpl) + (x * (ibpp/8));
  
  format = (depth == 1 ? XYPixmap : ZPixmap);
  XImage *image = XCreateImage (dpy, 0, depth, format, 0, 0, width, height,
                                0, 0);
  image->data = (char *) malloc (height * image->bytes_per_line);
  
  int obpl = image->bytes_per_line;
  
  /* both PPC and Intel use word-ordered ARGB frame buffers, which
     means that on Intel it is BGRA when viewed by bytes (And BGR
     when using 24bpp packing).

     BUT! Intel-64 stores alpha at the other end! 32bit=RGBA, 64bit=ARGB.
     The NSAlphaFirstBitmapFormat bit in bitmapFormat seems to be the
     indicator of this latest kink.
   */
  int xx, yy;
  if (depth == 1) {
    const unsigned char *iline = data;
    for (yy = 0; yy < height; yy++) {

      const unsigned char *iline2 = iline;
      for (xx = 0; xx < width; xx++) {

        iline2++;                     // ignore R  or  A  or  A  or  B
        iline2++;                     // ignore G  or  B  or  R  or  G
        unsigned char r = *iline2++;  // use    B  or  G  or  G  or  R
        if (ibpp == 32) iline2++;     // ignore A  or  R  or  B  or  A

        XPutPixel (image, xx, yy, (r ? 1 : 0));
      }
      iline += ibpl;
    }
  } else {
    Assert (ibpp == 24 || ibpp == 32, "weird obpp");
    const unsigned char *iline = data;
    unsigned char *oline = (unsigned char *) image->data;
    for (yy = 0; yy < height; yy++) {

      const unsigned char *iline2 = iline;
      unsigned char *oline2 = oline;

      if (alpha_first_p)			// ARGB
        for (xx = 0; xx < width; xx++) {
          unsigned char a = (ibpp == 32 ? (*iline2++) : 0xFF);
          unsigned char r = *iline2++;
          unsigned char g = *iline2++;
          unsigned char b = *iline2++;
          uint32_t pixel = ((a << 24) |
                            (r << 16) |
                            (g <<  8) |
                            (b <<  0));
          *((uint32_t *) oline2) = pixel;
          oline2 += 4;
        }
      else					// RGBA
        for (xx = 0; xx < width; xx++) {
          unsigned char r = *iline2++;
          unsigned char g = *iline2++;
          unsigned char b = *iline2++;
          unsigned char a = (ibpp == 32 ? (*iline2++) : 0xFF);
          uint32_t pixel = ((a << 24) |
                            (r << 16) |
                            (g <<  8) |
                            (b <<  0));
          *((uint32_t *) oline2) = pixel;
          oline2 += 4;
        }

      oline += obpl;
      iline += ibpl;
    }
  }

  if (bm) [bm release];

  return image;
}


/* Returns a transformation matrix to do rotation as per the provided
   EXIF "Orientation" value.
 */
static CGAffineTransform
exif_rotate (int rot, CGSize rect)
{
  CGAffineTransform trans = CGAffineTransformIdentity;
  switch (rot) {
  case 2:		// flip horizontal
    trans = CGAffineTransformMakeTranslation (rect.width, 0);
    trans = CGAffineTransformScale (trans, -1, 1);
    break;

  case 3:		// rotate 180
    trans = CGAffineTransformMakeTranslation (rect.width, rect.height);
    trans = CGAffineTransformRotate (trans, M_PI);
    break;

  case 4:		// flip vertical
    trans = CGAffineTransformMakeTranslation (0, rect.height);
    trans = CGAffineTransformScale (trans, 1, -1);
    break;

  case 5:		// transpose (UL-to-LR axis)
    trans = CGAffineTransformMakeTranslation (rect.height, rect.width);
    trans = CGAffineTransformScale (trans, -1, 1);
    trans = CGAffineTransformRotate (trans, 3 * M_PI / 2);
    break;

  case 6:		// rotate 90
    trans = CGAffineTransformMakeTranslation (0, rect.width);
    trans = CGAffineTransformRotate (trans, 3 * M_PI / 2);
    break;

  case 7:		// transverse (UR-to-LL axis)
    trans = CGAffineTransformMakeScale (-1, 1);
    trans = CGAffineTransformRotate (trans, M_PI / 2);
    break;

  case 8:		// rotate 270
    trans = CGAffineTransformMakeTranslation (rect.height, 0);
    trans = CGAffineTransformRotate (trans, M_PI / 2);
    break;

  default: 
    break;
  }

  return trans;
}


void
jwxyz_draw_NSImage_or_CGImage (Display *dpy, Drawable d, 
                                Bool nsimg_p, void *img_arg,
                               XRectangle *geom_ret, int exif_rotation)
{
  CGImageRef cgi;
  CGImageSourceRef cgsrc;
  NSSize imgr;

  if (nsimg_p) {

    NSImage *nsimg = (NSImage *) img_arg;
    imgr = [nsimg size];

    // convert the NSImage to a CGImage via the toll-free-bridging 
    // of NSData and CFData...
    //
    NSData *nsdata = [NSBitmapImageRep
                       TIFFRepresentationOfImageRepsInArray:
                         [nsimg representations]];
    CFDataRef cfdata = (CFDataRef) nsdata;
    cgsrc = CGImageSourceCreateWithData (cfdata, NULL);
    cgi = CGImageSourceCreateImageAtIndex (cgsrc, 0, NULL);

  } else {
    cgi = (CGImageRef) img_arg;
    imgr.width  = CGImageGetWidth (cgi);
    imgr.height = CGImageGetHeight (cgi);
  }

  Bool rot_p = (exif_rotation >= 5);

  if (rot_p)
    imgr = NSMakeSize (imgr.height, imgr.width);

  CGRect winr = d->frame;
  float rw = winr.size.width  / imgr.width;
  float rh = winr.size.height / imgr.height;
  float r = (rw < rh ? rw : rh);

  CGRect dst, dst2;
  dst.size.width  = imgr.width  * r;
  dst.size.height = imgr.height * r;
  dst.origin.x = (winr.size.width  - dst.size.width)  / 2;
  dst.origin.y = (winr.size.height - dst.size.height) / 2;

  dst2.origin.x = dst2.origin.y = 0;
  if (rot_p) {
    dst2.size.width = dst.size.height; 
    dst2.size.height = dst.size.width;
  } else {
    dst2.size = dst.size;
  }

  // Clear the part not covered by the image to background or black.
  //
  if (d->type == WINDOW)
    XClearWindow (dpy, d);
  else {
    set_color (d->cgc, BlackPixel(dpy,0), 32, NO, YES);
    draw_rect (dpy, d, 0, 0, 0, winr.size.width, winr.size.height, NO, YES);
  }

  CGAffineTransform trans = 
    exif_rotate (exif_rotation, rot_p ? dst2.size : dst.size);

  CGContextSaveGState (d->cgc);
  CGContextConcatCTM (d->cgc, 
                      CGAffineTransformMakeTranslation (dst.origin.x,
                                                        dst.origin.y));
  CGContextConcatCTM (d->cgc, trans);
  //Assert (CGImageGetColorSpace (cgi) == dpy->colorspace, "bad colorspace");
  CGContextDrawImage (d->cgc, dst2, cgi);
  CGContextRestoreGState (d->cgc);

  if (nsimg_p) {
    CFRelease (cgsrc);
    CGImageRelease (cgi);
  }

  if (geom_ret) {
    geom_ret->x = dst.origin.x;
    geom_ret->y = dst.origin.y;
    geom_ret->width  = dst.size.width;
    geom_ret->height = dst.size.height;
  }
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

Pixmap
XCreatePixmap (Display *dpy, Drawable d,
               unsigned int width, unsigned int height, unsigned int depth)
{
  char *data = (char *) malloc (width * height * 4);
  if (! data) return 0;

  Pixmap p = (Pixmap) calloc (1, sizeof(*p));
  p->type = PIXMAP;
  p->frame.size.width  = width;
  p->frame.size.height = height;
  p->pixmap.depth      = depth;
  
  /* Quartz doesn't have a 1bpp image type.
     We used to use 8bpp gray images instead of 1bpp, but some Mac video
     don't support that!  So we always use 32bpp, regardless of depth. */

  p->cgc = CGBitmapContextCreate (data, width, height,
                                  8, /* bits per component */
                                  width * 4, /* bpl */
                                  dpy->colorspace,
                                  // Without this, it returns 0...
                                  kCGImageAlphaNoneSkipFirst
                                  );
  Assert (p->cgc, "could not create CGBitmapContext");
  return p;
}


int
XFreePixmap (Display *d, Pixmap p)
{
  Assert (p->type == PIXMAP, "not a pixmap");
  CGContextRelease (p->cgc);
  free (p);
  return 0;
}


static Pixmap
copy_pixmap (Pixmap p)
{
  if (!p) return 0;
  Assert (p->type == PIXMAP, "not a pixmap");
  Pixmap p2 = (Pixmap) malloc (sizeof (*p2));
  *p2 = *p;
  CGContextRetain (p2->cgc);   // #### is this ok? need to copy it instead?
  return p2;
}


/* Font metric terminology, as used by X11:

   "lbearing" is the distance from the logical origin to the leftmost pixel.
   If a character's ink extends to the left of the origin, it is negative.

   "rbearing" is the distance from the logical origin to the rightmost pixel.

   "descent" is the distance from the logical origin to the bottommost pixel.
   For characters with descenders, it is negative.

   "ascent" is the distance from the logical origin to the topmost pixel.
   It is the number of pixels above the baseline.

   "width" is the distance from the logical origin to the position where
   the logical origin of the next character should be placed.

   If "rbearing" is greater than "width", then this character overlaps the
   following character.  If smaller, then there is trailing blank space.
 */


// This is XQueryFont, but for the XFontStruct embedded in 'Font'
//
static void
query_font (Font fid)
{
  if (!fid || !fid->nsfont) {
    NSLog(@"no NSFont in fid");
    abort();
  }
  if (![fid->nsfont fontName]) {
    NSLog(@"broken NSFont in fid");
    abort();
  }

  int first = 32;
  int last = 255;

  XFontStruct *f = &fid->metrics;
  XCharStruct *min = &f->min_bounds;
  XCharStruct *max = &f->max_bounds;

#define CEIL(F) ((F) < 0 ? floor(F) : ceil(F))

  f->fid               = fid;
  f->min_char_or_byte2 = first;
  f->max_char_or_byte2 = last;
  f->default_char      = 'M';
  f->ascent            =  CEIL ([fid->nsfont ascender]);
  f->descent           = -CEIL ([fid->nsfont descender]);

  min->width    = 255;  // set to smaller values in the loop
  min->ascent   = 255;
  min->descent  = 255;
  min->lbearing = 255;
  min->rbearing = 255;

  f->per_char = (XCharStruct *) calloc (last-first+2, sizeof (XCharStruct));
  int i;

  NSBezierPath *bpath = [NSBezierPath bezierPath];

  for (i = first; i <= last; i++) {
    unsigned char str[2];
    str[0] = i;
    str[1] = 0;

    NSString *nsstr = [NSString stringWithCString:(char *) str
                                         encoding:NSISOLatin1StringEncoding];

    /* I can't believe we have to go through this bullshit just to
       convert a 'char' to an NSGlyph!!

       You might think that we could do
          NSGlyph glyph = [fid->nsfont glyphWithName:nsstr];
       but that doesn't work; my guess is that glyphWithName expects
       full Unicrud names like "LATIN CAPITAL LETTER A WITH ACUTE".
     */
    NSGlyph glyph;
    {
      NSTextStorage *ts = [[NSTextStorage alloc] initWithString:nsstr];
      [ts setFont:fid->nsfont];
      NSLayoutManager *lm = [[NSLayoutManager alloc] init];

      /* Without this, the layout manager ends up on a queue somewhere and is
         referenced again after we return to the command loop.  Since we don't
         use this layout manager again, by that time it may have been garbage
         collected, and we crash.  Setting this seems to cause `lm' to no 
         longer be referenced once we exit this block. */
      [lm setBackgroundLayoutEnabled:NO];

      NSTextContainer *tc = [[NSTextContainer alloc] init];
      [lm addTextContainer:tc];
      [tc release];	// lm retains tc
      [ts addLayoutManager:lm];
      [lm release];	// ts retains lm
      glyph = [lm glyphAtIndex:0];
      [ts release];
    }

    /* Compute the bounding box and advancement by converting the glyph
       to a bezier path.  There appears to be *no other way* to find out
       the bounding box of a character: [NSFont boundingRectForGlyph] and
       [NSString sizeWithAttributes] both return an advancement-sized
       rectangle, not a rectangle completely enclosing the glyph's ink.
     */
    NSPoint advancement;
    NSRect bbox;
    advancement.x = advancement.y = 0;
    [bpath removeAllPoints];
    [bpath moveToPoint:advancement];
    [bpath appendBezierPathWithGlyph:glyph inFont:fid->nsfont];
    advancement = [bpath currentPoint];
    bbox = [bpath bounds];

    /* Now that we know the advancement and bounding box, we can compute
       the lbearing and rbearing.
     */
    XCharStruct *cs = &f->per_char[i-first];

    cs->ascent   = CEIL (bbox.origin.y) + CEIL (bbox.size.height);
    cs->descent  = CEIL(-bbox.origin.y);
    cs->lbearing = CEIL (bbox.origin.x);
    cs->rbearing = CEIL (bbox.origin.x) + CEIL (bbox.size.width);
    cs->width    = CEIL (advancement.x);

    Assert (cs->rbearing - cs->lbearing == CEIL(bbox.size.width), 
            "bbox w wrong");
    Assert (cs->ascent   + cs->descent  == CEIL(bbox.size.height),
            "bbox h wrong");

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

# undef CEIL

#if 0
    fprintf(stderr, " %3d %c: w=%3d lb=%3d rb=%3d as=%3d ds=%3d "
                    " bb=%3d x %3d @ %3d %3d  adv=%3d %3d\n",
            i, i, cs->width, cs->lbearing, cs->rbearing, 
            cs->ascent, cs->descent,
            (int) bbox.size.width, (int) bbox.size.height,
            (int) bbox.origin.x, (int) bbox.origin.y,
            (int) advancement.x, (int) advancement.y);
#endif
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

  // copy XCharStruct array
  int size = f->max_char_or_byte2 - f->min_char_or_byte2;
  f->per_char = (XCharStruct *) calloc (size + 2, sizeof (XCharStruct));
  memcpy (f->per_char, fid->metrics.per_char,
          size * sizeof (XCharStruct));

  return f;
}


static Font
copy_font (Font fid)
{
  // copy 'Font' struct
  Font fid2 = (Font) malloc (sizeof(*fid2));
  *fid2 = *fid;

  // copy XCharStruct array
  int size = fid->metrics.max_char_or_byte2 - fid->metrics.min_char_or_byte2;
  fid2->metrics.per_char = (XCharStruct *) 
    malloc ((size + 2) * sizeof (XCharStruct));
  memcpy (fid2->metrics.per_char, fid->metrics.per_char, 
          size * sizeof (XCharStruct));

  // copy the other pointers
  fid2->ps_name = strdup (fid->ps_name);
//  [fid2->nsfont retain];
  fid2->metrics.fid = fid2;

  return fid2;
}


static NSFont *
try_font (BOOL fixed, BOOL bold, BOOL ital, BOOL serif, float size,
          char **name_ret)
{
  Assert (size > 0, "zero font size");
  const char *name;

  if (fixed) {
    // 
    // "Monaco" only exists in plain.
    // "LucidaSansTypewriterStd" gets an AGL bad value error.
    // 
    if (bold && ital) name = "Courier-BoldOblique";
    else if (bold)    name = "Courier-Bold";
    else if (ital)    name = "Courier-Oblique";
    else              name = "Courier";

  } else if (serif) {
    // 
    // "Georgia" looks better than "Times".
    // 
    if (bold && ital) name = "Georgia-BoldItalic";
    else if (bold)    name = "Georgia-Bold";
    else if (ital)    name = "Georgia-Italic";
    else              name = "Georgia";

  } else {
    // 
    // "Geneva" only exists in plain.
    // "LucidaSansStd-BoldItalic" gets an AGL bad value error.
    // "Verdana" renders smoother than "Helvetica" for some reason.
    // 
    if (bold && ital) name = "Verdana-BoldItalic";
    else if (bold)    name = "Verdana-Bold";
    else if (ital)    name = "Verdana-Italic";
    else              name = "Verdana";
  }

  NSString *nsname = [NSString stringWithCString:name
                                        encoding:NSUTF8StringEncoding];
  NSFont *f = [NSFont fontWithName:nsname size:size];
  if (f)
    *name_ret = strdup(name);
  return f;
}

static NSFont *
try_native_font (const char *name, char **name_ret, float *size_ret)
{
  if (!name) return 0;
  const char *spc = strrchr (name, ' ');
  if (!spc) return 0;
  int size = 0;
  if (1 != sscanf (spc, " %d ", &size)) return 0;
  if (size <= 4) return 0;

  char *name2 = strdup (name);
  name2[strlen(name2) - strlen(spc)] = 0;
  NSString *nsname = [NSString stringWithCString:name2
                                        encoding:NSUTF8StringEncoding];
  NSFont *f = [NSFont fontWithName:nsname size:size];
  if (f) {
    *name_ret = name2;
    *size_ret = size;
    return f;
  } else {
    free (name2);
    return 0;
  }
}


/* Returns a random font in the given size and face.
 */
static NSFont *
random_font (BOOL bold, BOOL ital, float size, char **name_ret)
{
  NSFontTraitMask mask = ((bold ? NSBoldFontMask   : NSUnboldFontMask) |
                          (ital ? NSItalicFontMask : NSUnitalicFontMask));
  NSArray *fonts = [[NSFontManager sharedFontManager]
                     availableFontNamesWithTraits:mask];
  if (!fonts) return 0;

  int n = [fonts count];
  if (n <= 0) return 0;

  int j;
  for (j = 0; j < n; j++) {
    int i = random() % n;
    NSString *name = [fonts objectAtIndex:i];
    NSFont *f = [NSFont fontWithName:name size:size];
    if (!f) continue;

    /* Don't use this font if it (probably) doesn't include ASCII characters.
     */
    NSStringEncoding enc = [f mostCompatibleStringEncoding];
    if (! (enc == NSUTF8StringEncoding ||
           enc == NSISOLatin1StringEncoding ||
           enc == NSNonLossyASCIIStringEncoding ||
           enc == NSISOLatin2StringEncoding ||
           enc == NSUnicodeStringEncoding ||
           enc == NSWindowsCP1250StringEncoding ||
           enc == NSWindowsCP1252StringEncoding ||
           enc == NSMacOSRomanStringEncoding)) {
      // NSLog(@"skipping \"%@\": encoding = %d", name, enc);
      continue;
    }
    // NSLog(@"using \"%@\": %d", name, enc);

    *name_ret = strdup ([name cStringUsingEncoding:NSUTF8StringEncoding]);
    return f;
  }

  // None of the fonts support ASCII?
  return 0;
}


static NSFont *
try_xlfd_font (const char *name, char **name_ret, float *size_ret)
{
  NSFont *nsfont = 0;
  BOOL bold  = NO;
  BOOL ital  = NO;
  BOOL fixed = NO;
  BOOL serif = NO;
  BOOL rand  = NO;
  float size = 0;
  char *ps_name = 0;

  const char *s = (name ? name : "");
  while (*s) {
    while (*s && (*s == '*' || *s == '-'))
      s++;
    const char *s2 = s;
    while (*s2 && (*s2 != '*' && *s2 != '-'))
      s2++;
    
    int L = s2-s;
    if (s == s2)
      ;
# define CMP(STR) (L == strlen(STR) && !strncasecmp (s, (STR), L))
    else if (CMP ("random"))   rand  = YES;
    else if (CMP ("bold"))     bold  = YES;
    else if (CMP ("i"))        ital  = YES;
    else if (CMP ("o"))        ital  = YES;
    else if (CMP ("courier"))  fixed = YES;
    else if (CMP ("fixed"))    fixed = YES;
    else if (CMP ("m"))        fixed = YES;
    else if (CMP ("times"))    serif = YES;
    else if (CMP ("6x10"))     fixed = YES, size = 8;
    else if (CMP ("6x10bold")) fixed = YES, size = 8,  bold = YES;
    else if (CMP ("9x15"))     fixed = YES, size = 12;
    else if (CMP ("9x15bold")) fixed = YES, size = 12, bold = YES;
    else if (CMP ("vga"))      fixed = YES, size = 12;
    else if (CMP ("console"))  fixed = YES, size = 12;
    else if (CMP ("gallant"))  fixed = YES, size = 12;
# undef CMP
    else if (size == 0) {
      int n = 0;
      if (1 == sscanf (s, " %d ", &n))
        size = n / 10.0;
    }

    s = s2;
  }

  if (size < 6 || size > 1000)
    size = 12;

  if (rand)
    nsfont   = random_font (bold, ital, size, &ps_name);

  if (!nsfont)
    nsfont   = try_font (fixed, bold, ital, serif, size, &ps_name);

  // if that didn't work, turn off attibutes until it does
  // (e.g., there is no "Monaco-Bold".)
  //
  if (!nsfont && serif) {
    serif = NO;
    nsfont = try_font (fixed, bold, ital, serif, size, &ps_name);
  }
  if (!nsfont && ital) {
    ital = NO;
    nsfont = try_font (fixed, bold, ital, serif, size, &ps_name);
  }
  if (!nsfont && bold) {
    bold = NO;
    nsfont = try_font (fixed, bold, ital, serif, size, &ps_name);
  }
  if (!nsfont && fixed) {
    fixed = NO;
    nsfont = try_font (fixed, bold, ital, serif, size, &ps_name);
  }

  if (nsfont) {
    *name_ret = ps_name;
    *size_ret = size;
    return nsfont;
  } else {
    return 0;
  }
}


Font
XLoadFont (Display *dpy, const char *name)
{
  Font fid = (Font) calloc (1, sizeof(*fid));

  fid->nsfont = try_native_font (name, &fid->ps_name, &fid->size);
  if (! fid->nsfont)
    fid->nsfont = try_xlfd_font (name, &fid->ps_name, &fid->size);
  if (!fid->nsfont) {
    NSLog(@"no NSFont for \"%s\"", name);
    abort();
  }
  CFRetain (fid->nsfont);   // needed for garbage collection?

  //NSLog(@"parsed \"%s\" to %s %.1f", name, fid->ps_name, fid->size);

  query_font (fid);

  return fid;
}


XFontStruct *
XLoadQueryFont (Display *dpy, const char *name)
{
  Font fid = XLoadFont (dpy, name);
  return XQueryFont (dpy, fid);
}

int
XUnloadFont (Display *dpy, Font fid)
{
  free (fid->ps_name);
  free (fid->metrics.per_char);

  // #### DAMMIT!  I can't tell what's going wrong here, but I keep getting
  //      crashes in [NSFont ascender] <- query_font, and it seems to go away
  //      if I never release the nsfont.  So, fuck it, we'll just leak fonts.
  //      They're probably not very big...
  //
  //  [fid->nsfont release];
  //  CFRelease (fid->nsfont);

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
      if (info[i].per_char)
        free (info[i].per_char);
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
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);
  gc->gcv.font = copy_font (fid);
  [gc->gcv.font->nsfont retain];
  CFRetain (gc->gcv.font->nsfont);   // needed for garbage collection?
  return 0;
}

int
XTextExtents (XFontStruct *f, const char *s, int length,
              int *dir_ret, int *ascent_ret, int *descent_ret,
              XCharStruct *cs)
{
  memset (cs, 0, sizeof(*cs));
  int i;
  for (i = 0; i < length; i++) {
    unsigned char c = (unsigned char) s[i];
    if (c < f->min_char_or_byte2 || c > f->max_char_or_byte2)
      c = f->default_char;
    const XCharStruct *cc = &f->per_char[c - f->min_char_or_byte2];
    if (i == 0) {
      *cs = *cc;
    } else {
      cs->ascent   = MAX (cs->ascent,   cc->ascent);
      cs->descent  = MAX (cs->descent,  cc->descent);
      cs->lbearing = MIN (cs->lbearing, cs->width + cc->lbearing);
      cs->rbearing = MAX (cs->rbearing, cs->width + cc->rbearing);
      cs->width   += cc->width;
    }
  }
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


static void
set_font (CGContextRef cgc, GC gc)
{
  Font font = gc->gcv.font;
  if (! font) {
    font = XLoadFont (0, 0);
    gc->gcv.font = font;
    [gc->gcv.font->nsfont retain];
    CFRetain (gc->gcv.font->nsfont);   // needed for garbage collection?
  }
  CGContextSelectFont (cgc, font->ps_name, font->size, kCGEncodingMacRoman);
  CGContextSetTextMatrix (cgc, CGAffineTransformIdentity);
}


static int
draw_string (Display *dpy, Drawable d, GC gc, int x, int y,
             const char  *str, int len, BOOL clear_background_p)
{
  if (clear_background_p) {
    int ascent, descent, dir;
    XCharStruct cs;
    XTextExtents (&gc->gcv.font->metrics, str, len,
                  &dir, &ascent, &descent, &cs);
    draw_rect (dpy, d, gc,
               x + MIN (0, cs.lbearing),
               y - MAX (0, ascent),
               MAX (MAX (0, cs.rbearing) -
                    MIN (0, cs.lbearing),
                    cs.width),
               MAX (0, ascent) + MAX (0, descent),
               NO, YES);
  }

  CGRect wr = d->frame;

# if 1
  /* The Quartz way is probably faster, but doesn't draw Latin1 properly.
     But the Cocoa way only works on NSView, not on CGContextRef (pixmaps)!
   */

  push_fg_gc (d, gc, YES);
  set_font (d->cgc, gc);

  CGContextSetTextDrawingMode (d->cgc, kCGTextFill);
  if (gc->gcv.antialias_p)
    CGContextSetShouldAntialias (d->cgc, YES);
  CGContextShowTextAtPoint (d->cgc,
                            wr.origin.x + x,
                            wr.origin.y + wr.size.height - y,
                            str, len);
  pop_gc (d, gc);

#else /* !0 */

  /* The Cocoa way...
   */

  unsigned long argb = gc->gcv.foreground;
  if (gc->depth == 1) argb = (argb ? WhitePixel(dpy,0) : BlackPixel(dpy,0));
  float a = ((argb >> 24) & 0xFF) / 255.0;
  float r = ((argb >> 16) & 0xFF) / 255.0;
  float g = ((argb >>  8) & 0xFF) / 255.0;
  float b = ((argb      ) & 0xFF) / 255.0;
  NSColor *fg = [NSColor colorWithDeviceRed:r green:g blue:b alpha:a];
  NSDictionary *attr =
    [NSDictionary dictionaryWithObjectsAndKeys:
                    gc->gcv.font->nsfont, NSFontAttributeName,
                    fg, NSForegroundColorAttributeName,
                  nil];
  char *s2 = (char *) malloc (len + 1);
  strncpy (s2, str, len);
  s2[len] = 0;
  NSString *nsstr = [NSString stringWithCString:s2
                                         encoding:NSISOLatin1StringEncoding];
  free (s2);
  NSPoint pos;
  pos.x = wr.origin.x + x;
  pos.y = wr.origin.y + wr.size.height - y - gc->gcv.font->metrics.descent;
  [nsstr drawAtPoint:pos withAttributes:attr];

#endif  /* 0 */

  return 0;
}


int
XDrawString (Display *dpy, Drawable d, GC gc, int x, int y,
             const char  *str, int len)
{
  return draw_string (dpy, d, gc, x, y, str, len, NO);
}

int
XDrawImageString (Display *dpy, Drawable d, GC gc, int x, int y,
                  const char *str, int len)
{
  return draw_string (dpy, d, gc, x, y, str, len, YES);
}


int
XSetForeground (Display *dpy, GC gc, unsigned long fg)
{
  validate_pixel (fg, gc->depth, gc->gcv.alpha_allowed_p);
  gc->gcv.foreground = fg;
  return 0;
}


int
XSetBackground (Display *dpy, GC gc, unsigned long bg)
{
  validate_pixel (bg, gc->depth, gc->gcv.alpha_allowed_p);
  gc->gcv.background = bg;
  return 0;
}

int
jwxyz_XSetAlphaAllowed (Display *dpy, GC gc, Bool allowed)
{
  gc->gcv.alpha_allowed_p = allowed;
  return 0;
}

int
jwxyz_XSetAntiAliasing (Display *dpy, GC gc, Bool antialias_p)
{
  gc->gcv.antialias_p = antialias_p;
  return 0;
}


int
XSetLineAttributes (Display *dpy, GC gc, unsigned int line_width,
                    int line_style, int cap_style, int join_style)
{
  gc->gcv.line_width = line_width;
  Assert (line_style == LineSolid, "only LineSolid implemented");
//  gc->gcv.line_style = line_style;
  gc->gcv.cap_style = cap_style;
  gc->gcv.join_style = join_style;
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
  gc->gcv.function = which;
  return 0;
}

int
XSetSubwindowMode (Display *dpy, GC gc, int which)
{
  gc->gcv.subwindow_mode = which;
  return 0;
}

int
XSetClipMask (Display *dpy, GC gc, Pixmap m)
{
  Assert (!!gc->gcv.clip_mask == !!gc->clip_mask, "GC clip mask mixup");

  if (gc->gcv.clip_mask) {
    XFreePixmap (dpy, gc->gcv.clip_mask);
    CGImageRelease (gc->clip_mask);
  }

  gc->gcv.clip_mask = copy_pixmap (m);
  if (gc->gcv.clip_mask)
    gc->clip_mask = CGBitmapContextCreateImage (gc->gcv.clip_mask->cgc);
  else
    gc->clip_mask = 0;

  return 0;
}

int
XSetClipOrigin (Display *dpy, GC gc, int x, int y)
{
  gc->gcv.clip_x_origin = x;
  gc->gcv.clip_y_origin = y;
  return 0;
}


Bool
XQueryPointer (Display *dpy, Window w, Window *root_ret, Window *child_ret,
               int *root_x_ret, int *root_y_ret, 
               int *win_x_ret, int *win_y_ret, unsigned int *mask_ret)
{
  Assert (w->type == WINDOW, "not a window");
  NSWindow *nsw = [w->window.view window];
  NSPoint wpos;
  // get bottom left of window on screen, from bottom left
  wpos.x = wpos.y = 0;
  wpos = [nsw convertBaseToScreen:wpos];
  
  NSPoint vpos;
  // get bottom left of view on window, from bottom left
  vpos.x = vpos.y = 0;
  vpos = [w->window.view convertPoint:vpos toView:[nsw contentView]];

  // get bottom left of view on screen, from bottom left
  vpos.x += wpos.x;
  vpos.y += wpos.y;
  
  // get top left of view on screen, from bottom left
  vpos.y += w->frame.size.height;
  
  // get top left of view on screen, from top left
  NSArray *screens = [NSScreen screens];
  NSScreen *screen = (screens && [screens count] > 0
                      ? [screens objectAtIndex:0]
                      : [NSScreen mainScreen]);
  NSRect srect = [screen frame];
  vpos.y = srect.size.height - vpos.y;
  
  // get the mouse position on window, from bottom left
  NSEvent *e = [NSApp currentEvent];
  NSPoint p = [e locationInWindow];
  
  // get mouse position on screen, from bottom left
  p.x += wpos.x;
  p.y += wpos.y;
  
  // get mouse position on screen, from top left
  p.y = srect.size.height - p.y;

  if (root_x_ret) *root_x_ret = (int) p.x;
  if (root_y_ret) *root_y_ret = (int) p.y;
  if (win_x_ret)  *win_x_ret  = (int) (p.x - vpos.x);
  if (win_y_ret)  *win_y_ret  = (int) (p.y - vpos.y);
  
  if (mask_ret)   *mask_ret   = 0;  // ####
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
  Assert (w->type == WINDOW, "not a window");
  NSWindow *nsw = [w->window.view window];
  NSPoint wpos;
  // get bottom left of window on screen, from bottom left
  wpos.x = wpos.y = 0;
  wpos = [nsw convertBaseToScreen:wpos];
  
  NSPoint vpos;
  // get bottom left of view on window, from bottom left
  vpos.x = vpos.y = 0;
  vpos = [w->window.view convertPoint:vpos toView:[nsw contentView]];

  // get bottom left of view on screen, from bottom left
  vpos.x += wpos.x;
  vpos.y += wpos.y;
  
  // get top left of view on screen, from bottom left
  vpos.y += w->frame.size.height;
  
  // get top left of view on screen, from top left
  NSArray *screens = [NSScreen screens];
  NSScreen *screen = (screens && [screens count] > 0
                      ? [screens objectAtIndex:0]
                      : [NSScreen mainScreen]);
  NSRect srect = [screen frame];
  vpos.y = srect.size.height - vpos.y;
  
  // point starts out relative to top left of view
  NSPoint p;
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
  char c = (char) ks;	  // could be smarter about modifiers here...
  if (k_ret) *k_ret = ks;
  if (size > 0) buf[0] = c;
  if (size > 1) buf[1] = 0;
  return 0;
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
  return 0xFFFFFF;
}

int
visual_class (Screen *s, Visual *v)
{
  return TrueColor;
}

// declared in utils/grabclient.h
Bool
use_subwindow_mode_p (Screen *screen, Window window)
{
  return False;
}
