/* xscreensaver, Copyright (c) 1991-2013 Jamie Zawinski <jwz@jwz.org>
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
#import <wchar.h>

#ifdef USE_IPHONE
# import <UIKit/UIKit.h>
# import <UIKit/UIScreen.h>
# import <QuartzCore/QuartzCore.h>
# import <CoreText/CTFont.h>
# define NSView  UIView
# define NSRect  CGRect
# define NSPoint CGPoint
# define NSSize  CGSize
# define NSColor UIColor
# define NSImage UIImage
# define NSEvent UIEvent
# define NSFont  UIFont
# define NSGlyph CGGlyph
# define NSWindow UIWindow
# define NSMakeSize   CGSizeMake
# define NSBezierPath UIBezierPath
#else
# import <Cocoa/Cocoa.h>
#endif

#import "jwxyz.h"
#import "jwxyz-timers.h"
#import "yarandom.h"

# define USE_BACKBUFFER  /* must be in sync with XScreenSaverView.h */

#undef  Assert
#define Assert(C,S) do { if (!(C)) jwxyz_abort ("%s",(S)); } while(0)

# undef MAX
# undef MIN
# define MAX(a,b) ((a)>(b)?(a):(b))
# define MIN(a,b) ((a)<(b)?(a):(b))


struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  CGContextRef cgc;
  CGImageRef cgi;
  CGRect frame;
  union {
    struct {
      NSView *view;
      unsigned long background;
      int last_mouse_x, last_mouse_y;
    } window;
    struct {
      int depth;
      void *cgc_buffer;		// the bits to which CGContextRef renders
    } pixmap;
  };
};

struct jwxyz_Display {
  Window main_window;
  Screen *screen;
  struct jwxyz_sources_data *timers_data;

# ifndef USE_IPHONE
  CGDirectDisplayID cgdpy;  /* ...of the one and only Window, main_window.
                               This can change if the window is dragged to
                               a different screen. */
# endif

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


/* Instead of calling abort(), throw a real exception, so that
   XScreenSaverView can catch it and display a dialog.
 */
void
jwxyz_abort (const char *fmt, ...)
{
  char s[10240];
  if (!fmt || !*fmt)
    strcpy (s, "abort");
  else
    {
      va_list args;
      va_start (args, fmt);
      vsprintf (s, fmt, args);
      va_end (args);
    }
  [[NSException exceptionWithName: NSInternalInconsistencyException
                reason: [NSString stringWithCString: s
                                  encoding:NSUTF8StringEncoding]
                userInfo: nil]
    raise];
  abort();  // not reached
}


Display *
jwxyz_make_display (void *nsview_arg, void *cgc_arg)
{
  CGContextRef cgc = (CGContextRef) cgc_arg;
  NSView *view = (NSView *) nsview_arg;
  Assert (view, "no view");
  if (!view) return 0;

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
  w->window.view = view;
  CFRetain (w->window.view);   // needed for garbage collection?
  w->window.background = BlackPixel(0,0);

  d->main_window = w;

# ifndef USE_IPHONE
  if (! cgc) {
    [view lockFocus];
    cgc = [[[view window] graphicsContext] graphicsPort];
    [view unlockFocus];
    w->cgc = cgc;
  }
# endif

  Assert (cgc, "no CGContext");
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
  Assert (w && w->type == WINDOW, "not a window");
  return w->window.view;
}


/* Call this after any modification to the bits on a Pixmap or Window.
   Most Pixmaps are used frequently as sources and infrequently as
   destinations, so it pays to cache the data as a CGImage as needed.
 */
static void
invalidate_drawable_cache (Drawable d)
{
  if (d && d->cgi) {
    CGImageRelease (d->cgi);
    d->cgi = 0;
  }
}


/* Call this when the View changes size or position.
 */
void
jwxyz_window_resized (Display *dpy, Window w, 
                      int new_x, int new_y, int new_width, int new_height,
                      void *cgc_arg)
{
  CGContextRef cgc = (CGContextRef) cgc_arg;
  Assert (w && w->type == WINDOW, "not a window");
  w->frame.origin.x    = new_x;
  w->frame.origin.y    = new_y;
  w->frame.size.width  = new_width;
  w->frame.size.height = new_height;

  if (cgc) w->cgc = cgc;
  Assert (w->cgc, "no CGContext");

# ifndef USE_IPHONE
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
# endif // USE_IPHONE

# ifndef USE_BACKBUFFER
  // Funny thing: As of OS X 10.9, if USE_BACKBUFFER is turned off,
  // then this one's faster.

  {
    // Figure out this screen's colorspace, and use that for every CGImage.
    //
    CMProfileRef profile = 0;
    CMGetProfileByAVID ((CMDisplayIDType) dpy->cgdpy, &profile);
    Assert (profile, "unable to find colorspace profile");
    dpy->colorspace = CGColorSpaceCreateWithPlatformColorSpace (profile);
    Assert (dpy->colorspace, "unable to find colorspace");
  }
# else  // USE_BACKBUFFER

  // WTF?  It's faster if we *do not* use the screen's colorspace!
  //
  dpy->colorspace = CGColorSpaceCreateDeviceRGB();
# endif // USE_BACKBUFFER

  invalidate_drawable_cache (w);
}


#ifdef USE_IPHONE
void
jwxyz_mouse_moved (Display *dpy, Window w, int x, int y)
{
  Assert (w && w->type == WINDOW, "not a window");
  w->window.last_mouse_x = x;
  w->window.last_mouse_y = y;
}
#endif // USE_IPHONE


void
jwxyz_flush_context (Display *dpy)
{
  // This is only used when USE_BACKBUFFER is off.
  CGContextFlush(dpy->main_window->cgc); // CGContextSynchronize is another possibility.
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
    default: Assert(0, "unknown gcv function"); break;
  }

  if (gc->gcv.clip_mask)
    set_clip_mask (d, gc);
}

#define pop_gc(d,gc) CGContextRestoreGState (d->cgc)


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
   call to CGContextFillRects() with a list of 1x1 rectangles!

   I still wouldn't call it *fast*, however...
 */
#define XDRAWPOINTS_IMAGES

/* Update, 2012: Kurt Revis <krevis@snoize.com> points out that diddling
   the bitmap data directly is faster.  This only works on Pixmaps, though,
   not Windows.  (Fortunately, on iOS, the Window is really a Pixmap.)
 */
#define XDRAWPOINTS_CGDATA

int
XDrawPoints (Display *dpy, Drawable d, GC gc, 
             XPoint *points, int count, int mode)
{
  int i;
  CGRect wr = d->frame;

  push_fg_gc (d, gc, YES);

# ifdef XDRAWPOINTS_CGDATA

#  ifdef USE_BACKBUFFER
  if (1)  // Because of the backbuffer, all iPhone Windows work like Pixmaps.
#  else
  if (d->type == PIXMAP)
#  endif
  {
    CGContextRef cgc = d->cgc;
    void *data = CGBitmapContextGetData (cgc);
    size_t bpr = CGBitmapContextGetBytesPerRow (cgc);
    size_t w = CGBitmapContextGetWidth (cgc);
    size_t h = CGBitmapContextGetHeight (cgc);

    Assert (data, "no bitmap data in Drawable");

    unsigned int argb = gc->gcv.foreground;
    validate_pixel (argb, gc->depth, gc->gcv.alpha_allowed_p);
    if (gc->depth == 1)
      argb = (gc->gcv.foreground ? WhitePixel(0,0) : BlackPixel(0,0));

    CGFloat x0 = wr.origin.x;
    CGFloat y0 = wr.origin.y; // Y axis is refreshingly not flipped.

    // It's uglier, but faster, to hoist the conditional out of the loop.
    if (mode == CoordModePrevious) {
      CGFloat x = x0, y = y0;
      for (i = 0; i < count; i++, points++) {
        x += points->x;
        y += points->y;

        if (x >= 0 && x < w && y >= 0 && y < h) {
          unsigned int *p = (unsigned int *)
            ((char *) data + (size_t) y * bpr + (size_t) x * 4);
          *p = argb;
        }
      }
    } else {
      for (i = 0; i < count; i++, points++) {
        CGFloat x = x0 + points->x;
        CGFloat y = y0 + points->y;

        if (x >= 0 && x < w && y >= 0 && y < h) {
          unsigned int *p = (unsigned int *)
            ((char *) data + (size_t) y * bpr + (size_t) x * 4);
          *p = argb;
        }
      }
    }

  } else	/* d->type == WINDOW */

# endif /* XDRAWPOINTS_CGDATA */
  {

# ifdef XDRAWPOINTS_IMAGES

    unsigned int argb = gc->gcv.foreground;
    validate_pixel (argb, gc->depth, gc->gcv.alpha_allowed_p);
    if (gc->depth == 1)
      argb = (gc->gcv.foreground ? WhitePixel(0,0) : BlackPixel(0,0));

    CGDataProviderRef prov = CGDataProviderCreateWithData (NULL, &argb, 4,
                                                           NULL);
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

      //Assert(CGImageGetColorSpace (cgi) == dpy->colorspace,"bad colorspace");
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
  }

  pop_gc (d, gc);
  invalidate_drawable_cache (d);

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

static Bool
bitmap_context_p (Drawable d)
{
# ifdef USE_BACKBUFFER
  return True;
# else
  // Because of the backbuffer, all iPhone Windows work like Pixmaps.
  return d->type == PIXMAP;
# endif
}

static void
fill_rect_memset (void *dst, size_t dst_pitch, uint32_t fill_data,
                  size_t fill_width, size_t fill_height)
{
  Assert(sizeof(wchar_t) == 4, "somebody changed the ABI");
  while (fill_height) {
    // Would be nice if Apple used SSE/NEON in wmemset. Maybe someday.
    wmemset (dst, fill_data, fill_width);
    --fill_height;
    dst = (char *) dst + dst_pitch;
  }
}

static void *
seek_xy (void *dst, size_t dst_pitch, unsigned x, unsigned y)
{
  return (char *)dst + dst_pitch * y + x * 4;
}

static unsigned int
drawable_depth (Drawable d)
{
  return (d->type == WINDOW
          ? visual_depth (NULL, NULL)
          : d->pixmap.depth);
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

  if (gc->gcv.function == GXset ||
      gc->gcv.function == GXclear) {
    // "set" and "clear" are dumb drawing modes that ignore the source
    // bits and just draw solid rectangles.
    set_color (dst->cgc,
               (gc->gcv.function == GXset
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

  // Not actually the original dst_rect, just the one before it's clipped to
  // the src_frame.
  CGRect orig_dst_rect = dst_rect;

  CLIP (src, dst, x, width);
  CLIP (src, dst, y, height);
# undef CLIP

  if (orig_dst_rect.size.width <= 0 || orig_dst_rect.size.height <= 0)
    return 0;

  // Sort-of-special case where no pixels can be grabbed from the source,
  // and the whole destination is filled with the background color.
  if (src_rect.size.width < 0 || src_rect.size.height < 0) {
    src_rect.size.width  = 0;
    src_rect.size.height = 0;
  }
  
  NSObject *releaseme = 0;
  CGImageRef cgi;
  BOOL mask_p = NO;
  BOOL free_cgi_p = NO;


  /* If we're copying from a bitmap to a bitmap, and there's nothing funny
     going on with clipping masks or depths or anything, optimize it by
     just doing a memcpy instead of going through a CGI.
   */
  if (bitmap_context_p (src)) {

    if (bitmap_context_p (dst) &&
        gc->gcv.function == GXcopy &&
        !gc->gcv.clip_mask &&
        drawable_depth (src) == drawable_depth (dst)) {

      Assert(!(int)src_frame.origin.x &&
             !(int)src_frame.origin.y &&
             !(int)dst_frame.origin.x &&
             !(int)dst_frame.origin.y,
             "unexpected non-zero origin");
      
      char *src_data = CGBitmapContextGetData(src->cgc);
      char *dst_data = CGBitmapContextGetData(dst->cgc);
      size_t src_pitch = CGBitmapContextGetBytesPerRow(src->cgc);
      size_t dst_pitch = CGBitmapContextGetBytesPerRow(dst->cgc);
      
      // Int to float and back again. It's not very safe, but it seems to work.
      int src_x0 = src_rect.origin.x;
      int dst_x0 = dst_rect.origin.x;
      
      // Flip the Y-axis a second time.
      int src_y0 = (src_frame.origin.y + src_frame.size.height -
                    src_rect.size.height - src_rect.origin.y);
      int dst_y0 = (dst_frame.origin.y + dst_frame.size.height -
                    dst_rect.size.height - dst_rect.origin.y);
      
      unsigned width0  = (int) src_rect.size.width;
      unsigned height0 = (int) src_rect.size.height;
      
      Assert((int)src_rect.size.width  == (int)dst_rect.size.width ||
             (int)src_rect.size.height == (int)dst_rect.size.height,
             "size mismatch");
      {
        char *src_data0 = seek_xy(src_data, src_pitch, src_x0, src_y0);
        char *dst_data0 = seek_xy(dst_data, dst_pitch, dst_x0, dst_y0);
        size_t src_pitch0 = src_pitch;
        size_t dst_pitch0 = dst_pitch;
        size_t bytes = width0 * 4;

        if (src == dst && dst_y0 > src_y0) {
          // Copy upwards if the areas might overlap.
          src_data0 += src_pitch0 * (height0 - 1);
          dst_data0 += dst_pitch0 * (height0 - 1);
          src_pitch0 = -src_pitch0;
          dst_pitch0 = -dst_pitch0;
        }
      
        size_t lines0 = height0;
        while (lines0) {
          // memcpy is an alias for memmove on OS X.
          memmove(dst_data0, src_data0, bytes);
          src_data0 += src_pitch0;
          dst_data0 += dst_pitch0;
          --lines0;
        }
      }

      if (clipped) {
        int orig_dst_x = orig_dst_rect.origin.x;
        int orig_dst_y = (dst_frame.origin.y + dst_frame.size.height -
                          orig_dst_rect.origin.y - orig_dst_rect.size.height);
        int orig_width  = orig_dst_rect.size.width;
        int orig_height = orig_dst_rect.size.height;

        Assert (orig_dst_x >= 0 &&
                orig_dst_x + orig_width  <= (int) dst_frame.size.width &&
                orig_dst_y >= 0 &&
                orig_dst_y + orig_height <= (int) dst_frame.size.height,
                "wrong dimensions");

        if (orig_dst_y < dst_y0) {
          fill_rect_memset (seek_xy (dst_data, dst_pitch,
                                     orig_dst_x, orig_dst_y), dst_pitch,
                            gc->gcv.background, orig_width,
                            dst_y0 - orig_dst_y);
        }

        if (orig_dst_y + orig_height > dst_y0 + height0) {
          fill_rect_memset (seek_xy (dst_data, dst_pitch, orig_dst_x,
                                     dst_y0 + height0),
                            dst_pitch,
                            gc->gcv.background, orig_width,
                            orig_dst_y + orig_height - dst_y0 - height0);
        }

        if (orig_dst_x < dst_x0) {
          fill_rect_memset (seek_xy (dst_data, dst_pitch, orig_dst_x, dst_y0),
                            dst_pitch, gc->gcv.background,
                            dst_x0 - orig_dst_x, height0);
        }

        if (dst_x0 + width0 < orig_dst_x + orig_width) {
          fill_rect_memset (seek_xy (dst_data, dst_pitch, dst_x0 + width0,
                                     dst_y0),
                            dst_pitch, gc->gcv.background,
                            orig_dst_x + orig_width - dst_x0 - width0,
                            height0);
        }
      }

      invalidate_drawable_cache (dst);
      return 0;
    }


    // If we are copying from a Pixmap to a Pixmap or Window, we must first
    // copy the bits to an intermediary CGImage object, then copy that to the
    // destination drawable's CGContext.
    //
    // (It doesn't seem to be possible to use NSCopyBits() to optimize the
    // case of copying from a Pixmap back to itself, but I don't think that
    // happens very often anyway.)
    //
    // First we get a CGImage out of the pixmap CGContext -- it's the whole
    // pixmap, but it presumably shares the data pointer instead of copying
    // it.  We then cache that CGImage it inside the Pixmap object.  Note:
    // invalidate_drawable_cache() must be called to discard this any time a
    // modification is made to the pixmap, or we'll end up re-using old bits.
    //
    if (!src->cgi)
      src->cgi = CGBitmapContextCreateImage (src->cgc);
    cgi = src->cgi;

    // if doing a sub-rect, trim it down.
    if (src_rect.origin.x    != src_frame.origin.x   ||
        src_rect.origin.y    != src_frame.origin.y   ||
        src_rect.size.width  != src_frame.size.width ||
        src_rect.size.height != src_frame.size.height) {
      // #### I don't understand why this is needed...
      src_rect.origin.y = (src_frame.size.height -
                           src_rect.size.height - src_rect.origin.y);
      // This does not copy image data, so it should be fast.
      cgi = CGImageCreateWithImageInRect (cgi, src_rect);
      free_cgi_p = YES;
    }

  if (src->type == PIXMAP && src->pixmap.depth == 1)
      mask_p = YES;

# ifndef USE_BACKBUFFER
  } else { /* (src->type == WINDOW) */
    
    NSRect nsfrom;    // NSRect != CGRect on 10.4
    nsfrom.origin.x    = src_rect.origin.x;
    nsfrom.origin.y    = src_rect.origin.y;
    nsfrom.size.width  = src_rect.size.width;
    nsfrom.size.height = src_rect.size.height;

    if (src == dst) {

      // If we are copying from a window to itself, we can use NSCopyBits()
      // without first copying the rectangle to an intermediary CGImage.
      // This is ~28% faster (but I *expected* it to be twice as fast...)
      // (kumppa, bsod, decayscreen, memscroller, slidescreen, slip, xjack)
      //
      cgi = 0;

    } else {

      // If we are copying from a Window to a Pixmap, we must first copy
      // the bits to an intermediary CGImage object, then copy that to the
      // Pixmap's CGContext.
      //
      NSBitmapImageRep *bm = [[NSBitmapImageRep alloc]
                               initWithFocusedViewRect:nsfrom];
      unsigned char *data = [bm bitmapData];
      int bps = [bm bitsPerSample];
      int bpp = [bm bitsPerPixel];
      int bpl = [bm bytesPerRow];
      releaseme = bm;

      // create a CGImage from those bits.
      // (As of 10.5, we could just do cgi = [bm CGImage] (it is autoreleased)
      // but that method didn't exist in 10.4.)

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
      free_cgi_p = YES;
      //Assert(CGImageGetColorSpace(cgi) == dpy->colorspace,"bad colorspace");
      CGDataProviderRelease (prov);
    }

# endif // !USE_BACKBUFFER
  }

  CGContextRef cgc = dst->cgc;

  if (mask_p) {		// src depth == 1

    push_bg_gc (dst, gc, YES);

    // fill the destination rectangle with solid background...
    CGContextFillRect (cgc, orig_dst_rect);

    Assert (cgc, "no CGC with 1-bit XCopyArea");

    // then fill in a solid rectangle of the fg color, using the image as an
    // alpha mask.  (the image has only values of BlackPixel or WhitePixel.)
    set_color (cgc, gc->gcv.foreground, gc->depth, 
               gc->gcv.alpha_allowed_p, YES);
    CGContextClipToMask (cgc, dst_rect, cgi);
    CGContextFillRect (cgc, dst_rect);

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
      set_color (cgc, gc->gcv.background, gc->depth, 
                 gc->gcv.alpha_allowed_p, YES);
      CGContextFillRect (cgc, orig_dst_rect);
    }

    if (cgi) {
      // copy the CGImage onto the destination CGContext
      //Assert(CGImageGetColorSpace(cgi) == dpy->colorspace, "bad colorspace");
      CGContextDrawImage (cgc, dst_rect, cgi);
    } else {
      // No cgi means src == dst, and both are Windows.

# ifdef USE_BACKBUFFER
      Assert (0, "NSCopyBits unimplemented"); // shouldn't be reached anyway
      return 0;
# else // !USE_BACKBUFFER
      NSRect nsfrom;
      nsfrom.origin.x    = src_rect.origin.x;    // NSRect != CGRect on 10.4
      nsfrom.origin.y    = src_rect.origin.y;
      nsfrom.size.width  = src_rect.size.width;
      nsfrom.size.height = src_rect.size.height;
      NSPoint nsto;
      nsto.x             = dst_rect.origin.x;
      nsto.y             = dst_rect.origin.y;
      NSCopyBits (0, nsfrom, nsto);
# endif // !USE_BACKBUFFER
    }

    pop_gc (dst, gc);
  }

  if (free_cgi_p) CGImageRelease (cgi);

  if (releaseme) [releaseme release];
  invalidate_drawable_cache (dst);
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

  CGContextRef cgc = d->cgc;
  set_line_mode (cgc, &gc->gcv);
  CGContextBeginPath (cgc);
  CGContextMoveToPoint (cgc, p.x, p.y);
  p.x = wr.origin.x + x2;
  p.y = wr.origin.y + wr.size.height - y2;
  CGContextAddLineToPoint (cgc, p.x, p.y);
  CGContextStrokePath (cgc);
  pop_gc (d, gc);
  invalidate_drawable_cache (d);
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

  CGContextRef cgc = d->cgc;

  set_line_mode (cgc, &gc->gcv);
  
  // if the first and last points coincide, use closepath to get
  // the proper line-joining.
  BOOL closed_p = (points[0].x == points[count-1].x &&
                   points[0].y == points[count-1].y);
  if (closed_p) count--;
  
  p.x = wr.origin.x + points->x;
  p.y = wr.origin.y + wr.size.height - points->y;
  points++;
  CGContextBeginPath (cgc);
  CGContextMoveToPoint (cgc, p.x, p.y);
  for (i = 1; i < count; i++) {
    if (mode == CoordModePrevious) {
      p.x += points->x;
      p.y -= points->y;
    } else {
      p.x = wr.origin.x + points->x;
      p.y = wr.origin.y + wr.size.height - points->y;
    }
    CGContextAddLineToPoint (cgc, p.x, p.y);
    points++;
  }
  if (closed_p) CGContextClosePath (cgc);
  CGContextStrokePath (cgc);
  pop_gc (d, gc);
  invalidate_drawable_cache (d);
  return 0;
}


int
XDrawSegments (Display *dpy, Drawable d, GC gc, XSegment *segments, int count)
{
  int i;
  CGRect wr = d->frame;

  CGContextRef cgc = d->cgc;

  push_fg_gc (d, gc, NO);
  set_line_mode (cgc, &gc->gcv);
  CGContextBeginPath (cgc);
  for (i = 0; i < count; i++) {
    CGContextMoveToPoint    (cgc, 
                             wr.origin.x + segments->x1,
                             wr.origin.y + wr.size.height - segments->y1);
    CGContextAddLineToPoint (cgc,
                             wr.origin.x + segments->x2,
                             wr.origin.y + wr.size.height - segments->y2);
    segments++;
  }
  CGContextStrokePath (cgc);
  pop_gc (d, gc);
  invalidate_drawable_cache (d);
  return 0;
}


int
XClearWindow (Display *dpy, Window win)
{
  Assert (win && win->type == WINDOW, "not a window");
  CGRect wr = win->frame;
  return XClearArea (dpy, win, 0, 0, wr.size.width, wr.size.height, 0);
}

int
XSetWindowBackground (Display *dpy, Window w, unsigned long pixel)
{
  Assert (w && w->type == WINDOW, "not a window");
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

  CGContextRef cgc = d->cgc;
  if (fill_p)
    CGContextFillRect (cgc, r);
  else {
    if (gc)
      set_line_mode (cgc, &gc->gcv);
    CGContextStrokeRect (cgc, r);
  }

  if (gc)
    pop_gc (d, gc);
  invalidate_drawable_cache (d);
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
  CGContextRef cgc = d->cgc;
  push_fg_gc (d, gc, YES);
  for (i = 0; i < n; i++) {
    CGRect r;
    r.origin.x = wr.origin.x + rects->x;
    r.origin.y = wr.origin.y + wr.size.height - rects->y - rects->height;
    r.size.width = rects->width;
    r.size.height = rects->height;
    CGContextFillRect (cgc, r);
    rects++;
  }
  pop_gc (d, gc);
  invalidate_drawable_cache (d);
  return 0;
}


int
XClearArea (Display *dpy, Window win, int x, int y, int w, int h, Bool exp)
{
  Assert (win && win->type == WINDOW, "not a window");
  CGContextRef cgc = win->cgc;
  set_color (cgc, win->window.background, 32, NO, YES);
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
  CGContextRef cgc = d->cgc;
  CGContextBeginPath (cgc);
  float x = 0, y = 0;
  for (i = 0; i < npoints; i++) {
    if (i > 0 && mode == CoordModePrevious) {
      x += points[i].x;
      y -= points[i].y;
    } else {
      x = wr.origin.x + points[i].x;
      y = wr.origin.y + wr.size.height - points[i].y;
    }
        
    if (i == 0)
      CGContextMoveToPoint (cgc, x, y);
    else
      CGContextAddLineToPoint (cgc, x, y);
  }
  CGContextClosePath (cgc);
  if (gc->gcv.fill_rule == EvenOddRule)
    CGContextEOFillPath (cgc);
  else
    CGContextFillPath (cgc);
  pop_gc (d, gc);
  invalidate_drawable_cache (d);
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

  CGContextRef cgc = d->cgc;
  CGContextBeginPath (cgc);
  
  CGContextSaveGState(cgc);
  CGContextTranslateCTM (cgc, ctr.x, ctr.y);
  CGContextScaleCTM (cgc, width/2.0, height/2.0);
  if (fill_p)
    CGContextMoveToPoint (cgc, 0, 0);

  CGContextAddArc (cgc, 0.0, 0.0, 1, r1, r2, clockwise);
  CGContextRestoreGState (cgc);  // restore before stroke, for line width

  if (closed_p)
    CGContextClosePath (cgc); // for proper line joining
  
  if (fill_p) {
    CGContextFillPath (cgc);
  } else {
    set_line_mode (cgc, &gc->gcv);
    CGContextStrokePath (cgc);
  }

  pop_gc (d, gc);
  invalidate_drawable_cache (d);
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
  if (! mask) return;
  Assert (gc && from, "no gc");
  if (!gc || !from) return;

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
  Assert (w && w->type == WINDOW, "not a window");
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

  Assert (gc, "no GC");
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

  CGContextRef cgc = d->cgc;

  if (gc->gcv.function == GXset ||
      gc->gcv.function == GXclear) {
    // "set" and "clear" are dumb drawing modes that ignore the source
    // bits and just draw solid rectangles.
    set_color (cgc, (gc->gcv.function == GXset
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
    CGContextDrawImage (cgc, r, cgi);
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

    CGContextFillRect (cgc, r);				// foreground color
    CGContextClipToMask (cgc, r, mask);
    set_color (cgc, gc->gcv.background, gc->depth, NO, YES);
    CGContextFillRect (cgc, r);				// background color
    pop_gc (d, gc);

    free (flipped);
    CGDataProviderRelease (prov);
    CGImageRelease (mask);
  }

  invalidate_drawable_cache (d);

  return 0;
}


XImage *
XGetImage (Display *dpy, Drawable d, int x, int y,
           unsigned int width, unsigned int height,
           unsigned long plane_mask, int format)
{
  const unsigned char *data = 0;
  int depth, ibpp, ibpl;
  enum { RGBA, ARGB, BGRA } src_format; // As bytes.
# ifndef USE_BACKBUFFER
  NSBitmapImageRep *bm = 0;
# endif
  
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  CGContextRef cgc = d->cgc;

#ifndef USE_BACKBUFFER
  // Because of the backbuffer, all iPhone Windows work like Pixmaps.
  if (d->type == PIXMAP)
# endif
  {
    depth = (d->type == PIXMAP
             ? d->pixmap.depth
             : 32);
    // We create pixmaps and iPhone backbuffers with kCGImageAlphaNoneSkipFirst.
    src_format = BGRA; // #### Should this be ARGB on PPC?
    ibpp = CGBitmapContextGetBitsPerPixel (cgc);
    ibpl = CGBitmapContextGetBytesPerRow (cgc);
    data = CGBitmapContextGetData (cgc);
    Assert (data, "CGBitmapContextGetData failed");

# ifndef USE_BACKBUFFER
  } else { /* (d->type == WINDOW) */

    // get the bits (desired sub-rectangle) out of the NSView
    NSRect nsfrom;
    nsfrom.origin.x = x;
//  nsfrom.origin.y = y;
    nsfrom.origin.y = d->frame.size.height - height - y;
    nsfrom.size.width = width;
    nsfrom.size.height = height;
    bm = [[NSBitmapImageRep alloc] initWithFocusedViewRect:nsfrom];
    depth = 32;
    src_format = ([bm bitmapFormat] & NSAlphaFirstBitmapFormat) ? ARGB : RGBA;
    ibpp = [bm bitsPerPixel];
    ibpl = [bm bytesPerRow];
    data = [bm bitmapData];
    Assert (data, "NSBitmapImageRep initWithFocusedViewRect failed");
# endif // !USE_BACKBUFFER
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

      switch (src_format) {
      case ARGB:
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
        break;
      case RGBA:
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
        break;
      case BGRA:
        for (xx = 0; xx < width; xx++) {
          unsigned char b = *iline2++;
          unsigned char g = *iline2++;
          unsigned char r = *iline2++;
          unsigned char a = (ibpp == 32 ? (*iline2++) : 0xFF);
          uint32_t pixel = ((a << 24) |
                            (r << 16) |
                            (g <<  8) |
                            (b <<  0));
          *((uint32_t *) oline2) = pixel;
          oline2 += 4;
        }
        break;
      default:
        abort();
        break;
      }

      oline += obpl;
      iline += ibpl;
    }
  }

# ifndef USE_BACKBUFFER
  if (bm) [bm release];
# endif

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
# ifndef USE_IPHONE
  CGImageSourceRef cgsrc;
# endif // USE_IPHONE
  NSSize imgr;

  CGContextRef cgc = d->cgc;

  if (nsimg_p) {

    NSImage *nsimg = (NSImage *) img_arg;
    imgr = [nsimg size];

# ifndef USE_IPHONE
    // convert the NSImage to a CGImage via the toll-free-bridging 
    // of NSData and CFData...
    //
    NSData *nsdata = [NSBitmapImageRep
                       TIFFRepresentationOfImageRepsInArray:
                         [nsimg representations]];
    CFDataRef cfdata = (CFDataRef) nsdata;
    cgsrc = CGImageSourceCreateWithData (cfdata, NULL);
    cgi = CGImageSourceCreateImageAtIndex (cgsrc, 0, NULL);
# else  // USE_IPHONE
    cgi = nsimg.CGImage;
# endif // USE_IPHONE

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
    set_color (cgc, BlackPixel(dpy,0), 32, NO, YES);
    draw_rect (dpy, d, 0, 0, 0, winr.size.width, winr.size.height, NO, YES);
  }

  CGAffineTransform trans = 
    exif_rotate (exif_rotation, rot_p ? dst2.size : dst.size);

  CGContextSaveGState (cgc);
  CGContextConcatCTM (cgc, 
                      CGAffineTransformMakeTranslation (dst.origin.x,
                                                        dst.origin.y));
  CGContextConcatCTM (cgc, trans);
  //Assert (CGImageGetColorSpace (cgi) == dpy->colorspace, "bad colorspace");
  CGContextDrawImage (cgc, dst2, cgi);
  CGContextRestoreGState (cgc);

# ifndef USE_IPHONE
  if (nsimg_p) {
    CFRelease (cgsrc);
    CGImageRelease (cgi);
  }
# endif // USE_IPHONE

  if (geom_ret) {
    geom_ret->x = dst.origin.x;
    geom_ret->y = dst.origin.y;
    geom_ret->width  = dst.size.width;
    geom_ret->height = dst.size.height;
  }

  invalidate_drawable_cache (d);
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
  p->pixmap.cgc_buffer = data;
  
  /* Quartz doesn't have a 1bpp image type.
     Used to use 8bpp gray images instead of 1bpp, but some Mac video cards
     don't support that!  So we always use 32bpp, regardless of depth. */

  p->cgc = CGBitmapContextCreate (data, width, height,
                                  8, /* bits per component */
                                  width * 4, /* bpl */
                                  dpy->colorspace,
                                  // Without this, it returns 0...
                                  (kCGImageAlphaNoneSkipFirst |
                                   kCGBitmapByteOrder32Host)
                                  );
  Assert (p->cgc, "could not create CGBitmapContext");
  return p;
}


int
XFreePixmap (Display *d, Pixmap p)
{
  Assert (p && p->type == PIXMAP, "not a pixmap");
  invalidate_drawable_cache (p);
  CGContextRelease (p->cgc);
  if (p->pixmap.cgc_buffer)
    free (p->pixmap.cgc_buffer);
  free (p);
  return 0;
}


static Pixmap
copy_pixmap (Display *dpy, Pixmap p)
{
  if (!p) return 0;
  Assert (p->type == PIXMAP, "not a pixmap");

  int width  = p->frame.size.width;
  int height = p->frame.size.height;
  char *data = (char *) malloc (width * height * 4);
  if (! data) return 0;

  memcpy (data, p->pixmap.cgc_buffer, width * height * 4);

  Pixmap p2 = (Pixmap) malloc (sizeof (*p2));
  *p2 = *p;
  p2->cgi = 0;
  p2->pixmap.cgc_buffer = data;
  p2->cgc = CGBitmapContextCreate (data, width, height,
                                   8, /* bits per component */
                                   width * 4, /* bpl */
                                   dpy->colorspace,
                                   // Without this, it returns 0...
                                   (kCGImageAlphaNoneSkipFirst |
                                    kCGBitmapByteOrder32Host)
                                   );
  Assert (p2->cgc, "could not create CGBitmapContext");

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
    Assert (0, "no NSFont in fid");
    return;
  }
  if (![fid->nsfont fontName]) {
    Assert(0, @"broken NSFont in fid");
    return;
  }

  int first = 32;
  int last = 255;

  XFontStruct *f = &fid->metrics;
  XCharStruct *min = &f->min_bounds;
  XCharStruct *max = &f->max_bounds;

#define CEIL(F)  ((F) < 0 ? floor(F) : ceil(F))

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

# ifndef USE_IPHONE
  NSBezierPath *bpath = [NSBezierPath bezierPath];
# else  // USE_IPHONE
  CTFontRef ctfont =
    CTFontCreateWithName ((CFStringRef) [fid->nsfont fontName],
                          [fid->nsfont pointSize],
                          NULL);
  Assert (ctfont, @"no CTFontRef for UIFont");
# endif // USE_IPHONE

  for (i = first; i <= last; i++) {
    unsigned char str[2];
    str[0] = i;
    str[1] = 0;

    NSString *nsstr = [NSString stringWithCString:(char *) str
                                         encoding:NSISOLatin1StringEncoding];
    NSPoint advancement = { 0, };
    NSRect bbox = {{ 0, }, };

# ifndef USE_IPHONE

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
    advancement.x = advancement.y = 0;
    [bpath removeAllPoints];
    [bpath moveToPoint:advancement];
    [bpath appendBezierPathWithGlyph:glyph inFont:fid->nsfont];
    advancement = [bpath currentPoint];
    bbox = [bpath bounds];

# else  // USE_IPHONE

    /* There is no way to get "lbearing", "rbearing" or "descent" out of
       NSFont.  'sizeWithFont' gives us "width" and "height" only.
       Likewise, 'drawAtPoint' (to an offscreen CGContext) gives us the
       width of the character and the ascent of the font.

       Maybe we could use CGFontGetGlyphBBoxes() and avoid linking in
       the CoreText library, but there's no non-CoreText way to turn a
       unichar into a CGGlyph.
     */
    UniChar uchar = [nsstr characterAtIndex: 0];
    CGGlyph cgglyph = 0;

    if (CTFontGetGlyphsForCharacters (ctfont, &uchar, &cgglyph, 1))
      {
        bbox = CTFontGetBoundingRectsForGlyphs (ctfont,
                                                kCTFontDefaultOrientation,
                                                &cgglyph, NULL, 1);
        CGSize adv = { 0, };
        CTFontGetAdvancesForGlyphs (ctfont, kCTFontDefaultOrientation,
                                    &cgglyph, &adv, 1);
        advancement.x = adv.width;
        advancement.y = adv.height;

        /* A bug that existed was that the GL FPS display was truncating
           characters slightly: commas looked like periods.

           At one point, I believed the bounding box was being rounded
           wrong and we needed to add padding to it here.

           I think what was actually going on was, I was computing rbearing
           wrong.  Also there was an off-by-one error in texfont.c, displaying
           too little of the bitmap.

           Adding arbitrarily large padding to the bbox is fine in fontglide
           and FPS display, but screws up BSOD. Increasing bbox width makes
           inverted text print too wide; decreasing origin makes characters
           clip.

           I think that all 3 states are correct now with the new lbearing
           computation plus the texfont fix.
         */
#  if 0
        double kludge = 2;
        bbox.origin.x    -= kludge;
        bbox.origin.y    -= kludge;
        bbox.size.width  += kludge;
        bbox.size.height += kludge;
#  endif
      }
# endif // USE_IPHONE

    /* Now that we know the advancement and bounding box, we can compute
       the lbearing and rbearing.
     */
    XCharStruct *cs = &f->per_char[i-first];

    cs->ascent   = CEIL (bbox.origin.y) + CEIL (bbox.size.height);
    cs->descent  = CEIL(-bbox.origin.y);
    cs->lbearing = floor (bbox.origin.x);
//  cs->rbearing = CEIL (bbox.origin.x) + CEIL (bbox.size.width);
    cs->rbearing = CEIL (bbox.origin.x + bbox.size.width) - cs->lbearing;
    cs->width    = CEIL (advancement.x);

//  Assert (cs->rbearing - cs->lbearing == CEIL(bbox.size.width), 
//          "bbox w wrong");
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
                    " bb=%5.1f x %5.1f @ %5.1f %5.1f  adv=%5.1f %5.1f\n",
            i, i, cs->width, cs->lbearing, cs->rbearing, 
            cs->ascent, cs->descent,
            bbox.size.width, bbox.size.height,
            bbox.origin.x, bbox.origin.y,
            advancement.x, advancement.y);
#endif
  }

# ifdef USE_IPHONE
  CFRelease (ctfont);
# endif
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
try_native_font (const char *name, float scale,
                 char **name_ret, float *size_ret)
{
  if (!name) return 0;
  const char *spc = strrchr (name, ' ');
  if (!spc) return 0;
  int dsize = 0;
  if (1 != sscanf (spc, " %d ", &dsize)) return 0;
  float size = dsize;

  if (size <= 4) return 0;

  size *= scale;

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
# ifndef USE_IPHONE
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

# else  // USE_IPHONE

  NSMutableArray *fonts = [NSMutableArray arrayWithCapacity:100];
  NSArray *families = [UIFont familyNames];
  NSMutableDictionary *famdict = [NSMutableDictionary 
                                   dictionaryWithCapacity:100];
  NSObject *y = [NSNumber numberWithBool:YES];
  for (NSString *name in families) {
    // There are many dups in the families array -- uniquify it.
    [famdict setValue:y forKey:name];
  }

  for (NSString *name in famdict) {
    for (NSString *fn in [UIFont fontNamesForFamilyName:name]) {

# define MATCH(X) \
         ([fn rangeOfString:X options:NSCaseInsensitiveSearch].location \
         != NSNotFound)

      BOOL bb = MATCH(@"Bold");
      BOOL ii = MATCH(@"Italic") || MATCH(@"Oblique");

      if (!bold != !bb) continue;
      if (!ital != !ii) continue;

      /* Check if it can do ASCII.  No good way to accomplish this!
         These are fonts present in iPhone Simulator as of June 2012
         that don't include ASCII.
       */
      if (MATCH(@"AppleGothic") ||	// Korean
          MATCH(@"Dingbats") ||		// Dingbats
          MATCH(@"Emoji") ||		// Emoticons
          MATCH(@"Geeza") ||		// Arabic
          MATCH(@"Hebrew") ||		// Hebrew
          MATCH(@"HiraKaku") ||		// Japanese
          MATCH(@"HiraMin") ||		// Japanese
          MATCH(@"Kailasa") ||		// Tibetan
          MATCH(@"Ornaments") ||	// Dingbats
          MATCH(@"STHeiti")		// Chinese
         )
        continue;

      [fonts addObject:fn];
# undef MATCH
    }
  }

  if (! [fonts count]) return 0;	// Nothing suitable?

  int i = random() % [fonts count];
  NSString *name = [fonts objectAtIndex:i];
  UIFont *ff = [UIFont fontWithName:name size:size];
  *name_ret = strdup ([name cStringUsingEncoding:NSUTF8StringEncoding]);

  return ff;

# endif // USE_IPHONE
}


static NSFont *
try_xlfd_font (const char *name, float scale,
               char **name_ret, float *size_ret)
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

  size *= scale;

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

  float scale = 1;

# ifdef USE_IPHONE
  // Scale up fonts on Retina displays.
  scale = dpy->main_window->window.view.contentScaleFactor;
# endif

  fid->nsfont = try_native_font (name, scale, &fid->ps_name, &fid->size);

  if (!fid->nsfont && name &&
      strchr (name, ' ') &&
      !strchr (name, '*')) {
    // If name contains a space but no stars, it is a native font spec --
    // return NULL so that we know it really didn't exist.  Else, it is an
    //  XLFD font, so keep trying.
    XUnloadFont (dpy, fid);
    return 0;
  }

  if (! fid->nsfont)
    fid->nsfont = try_xlfd_font (name, scale, &fid->ps_name, &fid->size);

  // We should never return NULL for XLFD fonts.
  if (!fid->nsfont) {
    Assert (0, "no font");
    return 0;
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
  if (!fid) return 0;
  return XQueryFont (dpy, fid);
}

int
XUnloadFont (Display *dpy, Font fid)
{
  if (fid->ps_name)
    free (fid->ps_name);
  if (fid->metrics.per_char)
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
set_font (Display *dpy, CGContextRef cgc, GC gc)
{
  Font font = gc->gcv.font;
  if (! font) {
    font = XLoadFont (dpy, 0);
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

  CGContextRef cgc = d->cgc;
  push_fg_gc (d, gc, YES);
  set_font (dpy, cgc, gc);

  CGContextSetTextDrawingMode (cgc, kCGTextFill);
  if (gc->gcv.antialias_p)
    CGContextSetShouldAntialias (cgc, YES);
  CGContextShowTextAtPoint (cgc,
                            wr.origin.x + x,
                            wr.origin.y + wr.size.height - y,
                            str, len);
  pop_gc (d, gc);

# else /* !0 */

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

# endif  /* 0 */

  invalidate_drawable_cache (d);
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

  gc->gcv.clip_mask = copy_pixmap (dpy, m);
  if (gc->gcv.clip_mask)
    gc->clip_mask =
      CGBitmapContextCreateImage (gc->gcv.clip_mask->cgc);
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
  Assert (w && w->type == WINDOW, "not a window");

# ifdef USE_IPHONE
  int x = w->window.last_mouse_x;
  int y = w->window.last_mouse_y;
  if (root_x_ret) *root_x_ret = x;
  if (root_y_ret) *root_y_ret = y;
  if (win_x_ret)  *win_x_ret  = x;
  if (win_y_ret)  *win_y_ret  = y;

# else  // !USE_IPHONE

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
#ifdef USE_IPHONE
  double s = w->window.view.contentScaleFactor;
#else
  int s = 1;
#endif
  NSRect srect = [screen frame];
  vpos.y = (s * srect.size.height) - vpos.y;
  
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
# endif // !USE_IPHONE
  
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
  Assert (w && w->type == WINDOW, "not a window");

# ifdef USE_IPHONE

  NSPoint p;
  p.x = src_x;
  p.y = src_y;

# else  // !USE_IPHONE

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
# ifdef USE_IPHONE
  double s = w->window.view.contentScaleFactor;
# else
  int s = 1;
# endif
  NSRect srect = [screen frame];
  vpos.y = (s * srect.size.height) - vpos.y;
  
  // point starts out relative to top left of view
  NSPoint p;
  p.x = src_x;
  p.y = src_y;
  
  // get point relative to top left of screen
  p.x += vpos.x;
  p.y += vpos.y;
# endif // !USE_IPHONE

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
