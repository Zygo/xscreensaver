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
   Xlib and that do Cocoa-ish things that bear some resemblance to the
   things that Xlib might have done.

   This is the original version of jwxyz for MacOS and iOS.
   The version used by Android is in jwxyz-gl.c and jwxyz-common.c.
   Both versions depend on jwxyz-cocoa.m.
 */

#ifdef JWXYZ_QUARTZ // entire file

#import <stdlib.h>
#import <stdint.h>
#import <wchar.h>

#ifdef USE_IPHONE
# import <UIKit/UIKit.h>
# import <UIKit/UIScreen.h>
# import <QuartzCore/QuartzCore.h>
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
# define colorWithDeviceRed colorWithRed

# define NSFontTraitMask      UIFontDescriptorSymbolicTraits
// The values for the flags for NSFontTraitMask and
// UIFontDescriptorSymbolicTraits match up, not that it really matters here.
# define NSBoldFontMask       UIFontDescriptorTraitBold
# define NSFixedPitchFontMask UIFontDescriptorTraitMonoSpace
# define NSItalicFontMask     UIFontDescriptorTraitItalic
#else
# import <Cocoa/Cocoa.h>
#endif

#import <CoreText/CTFont.h>
#import <CoreText/CTLine.h>
#import <CoreText/CTRun.h>

#import "jwxyzI.h"
#import "jwxyz-cocoa.h"
#import "jwxyz-timers.h"
#import "yarandom.h"
#import "utf8wc.h"
#import "xft.h"

# undef MAX
# undef MIN
# define MAX(a,b) ((a)>(b)?(a):(b))
# define MIN(a,b) ((a)<(b)?(a):(b))


struct jwxyz_Display {
  Window main_window;
  Screen *screen;
  int screen_count;
  struct jwxyz_sources_data *timers_data;

# ifndef USE_IPHONE
  CGDirectDisplayID cgdpy;  /* ...of the one and only Window, main_window.
                               This can change if the window is dragged to
                               a different screen. */
# endif

  CGColorSpaceRef colorspace;  /* Color space of this screen.  We tag all of
                                  our images with this to avoid translation
                                  when rendering. */

  unsigned long window_background;
};

struct jwxyz_Screen {
  Display *dpy;
  CGBitmapInfo bitmap_info;
  unsigned long black, white;
  Visual *visual;
  int screen_number;
};

struct jwxyz_GC {
  XGCValues gcv;
  unsigned int depth;
  CGImageRef clip_mask;  // CGImage copy of the Pixmap in gcv.clip_mask
};

struct jwxyz_Font {
  Display *dpy;
  char *ps_name;
  NSFont *nsfont;
  float size;   // points
  char *xa_font;

  // In X11, "Font" is just an ID, and "XFontStruct" contains the metrics.
  // But we need the metrics on both of them, so they go here.
  XFontStruct metrics;
};

struct jwxyz_XFontSet {
  XFontStruct *font;
};


// 24/32bpp -> 32bpp image conversion.
// Any of RGBA, BGRA, ABGR, or ARGB can be represented by a rotate of 0/8/16/24
// bits and an optional byte order swap.

// This type encodes such a conversion.
typedef unsigned convert_mode_t;

// It's rotate, then swap.
// A rotation here shifts bytes forward in memory. On x86/ARM, that's a left
// rotate, and on PowerPC, a rightward rotation.
static const convert_mode_t CONVERT_MODE_ROTATE_MASK = 0x3;
static const convert_mode_t CONVERT_MODE_SWAP = 0x4;


// Converts an array of pixels ('src') from one format to another, placing the
// result in 'dest', according to the pixel conversion mode 'mode'.
static void
convert_row (uint32_t *dest, const void *src, size_t count,
             convert_mode_t mode, size_t src_bpp)
{
  Assert (src_bpp == 24 || src_bpp == 32, "weird bpp");

  // This works OK iff src == dest or src and dest do not overlap.

  if (!mode) {
    if (src != dest)
      memcpy (dest, src, count * 4);
    return;
  }

  // This is correct, but not fast.
  convert_mode_t rot = (mode & CONVERT_MODE_ROTATE_MASK) * 8;
  convert_mode_t flip = mode & CONVERT_MODE_SWAP;

  src_bpp /= 8;

  uint32_t *dest_end = dest + count;
  while (dest != dest_end) {
    uint32_t x;

    if (src_bpp == 4)
      x = *(const uint32_t *)src;
    else { // src_bpp == 3
      const uint8_t *src8 = (const uint8_t *)src;
      // __LITTLE/BIG_ENDIAN__ are defined by the compiler.
# if defined __LITTLE_ENDIAN__
      x = src8[0] | (src8[1] << 8) | (src8[2] << 16) | 0xff000000;
# elif defined __BIG_ENDIAN__
      x = (src8[0] << 24) | (src8[1] << 16) | (src8[2] << 8) | 0xff;
# else
#  error "Can't determine system endianness."
# endif
    }

    src = (const uint8_t *)src + src_bpp;

    /* The naive (i.e. ubiquitous) portable implementation of bitwise rotation,
       for 32-bit integers, is:

       (x << rot) | (x >> (32 - rot))

       This works nearly everywhere. Compilers on x86 wil generally recognize
       the idiom and convert it to a ROL instruction. But there's a problem
       here: according to the C specification, bit shifts greater than or equal
       to the length of the integer are undefined. And if rot = 0:
       1. (x << 0) | (x >> (32 - 0))
       2. (x << 0) | (x >> 32)
       3. (x << 0) | (Undefined!)

       Still, when the compiler converts this to a ROL on x86, everything works
       as intended. But, there are two additional problems when Clang does
       compile-time constant expression evaluation with the (x >> 32)
       expression:
       1. Instead of evaluating it to something reasonable (either 0, like a
          human would intuitively expect, or x, like x86 would with SHR), Clang
          seems to pull a value out of nowhere, like -1, or some other random
          number.
       2. Clang's warning for this, -Wshift-count-overflow, only works when the
          shift count is a literal constant, as opposed to an arbitrary
          expression that is optimized down to a constant.
       Put together, this means that the assertions in jwxyz_make_display with
       convert_px break with the above naive rotation, but only for a release
       build.

       http://blog.regehr.org/archives/1063
       http://llvm.org/bugs/show_bug.cgi?id=17332
       As described in those links, there is a solution here: Masking the
       undefined shift with '& 31' as below makes the experesion well-defined
       again. And LLVM is set to pick up on this safe version of the idiom and
       use a rotation instruction on architectures (like x86) that support it,
       just like it does with the unsafe version.

       Too bad LLVM doesn't want to pick up on that particular optimization
       here. Oh well. At least this code usually isn't critical w.r.t.
       performance.
     */

# if defined __LITTLE_ENDIAN__
    x = (x << rot) | (x >> ((32 - rot) & 31));
# elif defined __BIG_ENDIAN__
    x = (x >> rot) | (x << ((32 - rot) & 31));
# endif

    if (flip)
      x = __builtin_bswap32(x); // LLVM/GCC built-in function.

    *dest = x;
    ++dest;
  }
}


// Converts a single pixel.
static uint32_t
convert_px (uint32_t px, convert_mode_t mode)
{
  convert_row (&px, &px, 1, mode, 32);
  return px;
}


// This returns the inverse conversion mode, such that:
// pixel
//   == convert_px(convert_px(pixel, mode), convert_mode_invert(mode))
//   == convert_px(convert_px(pixel, convert_mode_invert(mode)), mode)
static convert_mode_t
convert_mode_invert (convert_mode_t mode)
{
  // swap(0); rot(n) == rot(n); swap(0)
  // swap(1); rot(n) == rot(-n); swap(1)
  return mode & CONVERT_MODE_SWAP ? mode : CONVERT_MODE_ROTATE_MASK & -mode;
}


// This combines two conversions into one, such that:
// convert_px(convert_px(pixel, mode0), mode1)
//   == convert_px(pixel, convert_mode_merge(mode0, mode1))
static convert_mode_t
convert_mode_merge (convert_mode_t m0, convert_mode_t m1)
{
  // rot(r0); swap(s0); rot(r1); swap(s1)
  // rot(r0); rot(s0 ? -r1 : r1); swap(s0); swap(s1)
  // rot(r0 + (s0 ? -r1 : r1)); swap(s0 + s1)
  return
    ((m0 + (m0 & CONVERT_MODE_SWAP ? -m1 : m1)) & CONVERT_MODE_ROTATE_MASK) |
    ((m0 ^ m1) & CONVERT_MODE_SWAP);
}


// This returns a conversion mode that converts an arbitrary 32-bit format
// specified by bitmap_info to RGBA.
static convert_mode_t
convert_mode_to_rgba (CGBitmapInfo bitmap_info)
{
  // Former default: kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little
  // i.e. BGRA
  // red   = 0x00FF0000;
  // green = 0x0000FF00;
  // blue  = 0x000000FF;

  // RGBA: kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big

  CGImageAlphaInfo alpha_info =
    (CGImageAlphaInfo)(bitmap_info & kCGBitmapAlphaInfoMask);

  Assert (! (bitmap_info & kCGBitmapFloatComponents),
          "kCGBitmapFloatComponents unsupported");
  Assert (alpha_info != kCGImageAlphaOnly, "kCGImageAlphaOnly not supported");

  convert_mode_t rot = alpha_info == kCGImageAlphaFirst ||
                       alpha_info == kCGImageAlphaPremultipliedFirst ||
                       alpha_info == kCGImageAlphaNoneSkipFirst ?
                       3 : 0;

  CGBitmapInfo byte_order = bitmap_info & kCGBitmapByteOrderMask;

  Assert (byte_order == kCGBitmapByteOrder32Little ||
          byte_order == kCGBitmapByteOrder32Big,
          "byte order not supported");

  convert_mode_t swap = byte_order == kCGBitmapByteOrder32Little ?
                        CONVERT_MODE_SWAP : 0;
  if (swap)
    rot = CONVERT_MODE_ROTATE_MASK & -rot;
  return swap | rot;
}


union color_bytes
{
  uint32_t pixel;
  uint8_t bytes[4];
};


uint32_t
jwxyz_alloc_color (Display *dpy,
                   uint16_t r, uint16_t g, uint16_t b, uint16_t a)
{
  union color_bytes color;

  /* Instead of (int)(c / 256.0), another possibility is
     (int)(c * 255.0 / 65535.0 + 0.5). This can be calculated using only
     uint8_t integer_math(uint16_t c) {
       unsigned c0 = c + 128;
       return (c0 - (c0 >> 8)) >> 8;
     }
   */

  color.bytes[0] = r >> 8;
  color.bytes[1] = g >> 8;
  color.bytes[2] = b >> 8;
  color.bytes[3] = a >> 8;

  return
    convert_px (color.pixel,
      convert_mode_invert (convert_mode_to_rgba (dpy->screen->bitmap_info)));
}


void
jwxyz_query_color (Display *dpy, unsigned long pixel, uint8_t *rgba)
{
  union color_bytes color;
  color.pixel = convert_px ((uint32_t)pixel,
                            convert_mode_to_rgba (dpy->screen->bitmap_info));
  for (unsigned i = 0; i != 4; ++i)
    rgba[i] = color.bytes[i];
}


static void
query_color_float (Display *dpy, unsigned long pixel, float *rgba)
{
  uint8_t rgba8[4];
  jwxyz_query_color (dpy, pixel, rgba8);
  for (unsigned i = 0; i != 4; ++i)
    rgba[i] = rgba8[i] * (1.0f / 255.0f);
}


/* We keep a list of all of the Displays that have been created and not
   yet freed so that they can have sensible display numbers.  If three
   displays are created (0, 1, 2) and then #1 is closed, then the fourth
   display will be given the now-unused display number 1. (Everything in
   here assumes a 1:1 Display/Screen mapping.)

   The size of this array is the most number of live displays at one time.
   So if it's 20, then we'll blow up if the system has 19 monitors and also
   has System Preferences open (the small preview window).

   Note that xlockmore-style savers tend to allocate big structures, so
   setting this to 1000 will waste a few megabytes.  Also some of them assume
   that the number of screens never changes, so dynamically expanding this
   array won't work.
 */
# ifndef USE_IPHONE
static Display *jwxyz_live_displays[20] = { 0, };
# endif


Display *
jwxyz_make_display (Window w)
{
  CGContextRef cgc = w->cgc;

  Display *d = (Display *) calloc (1, sizeof(*d));
  d->screen = (Screen *) calloc (1, sizeof(Screen));
  d->screen->dpy = d;
  
  d->screen_count = 1;
  d->screen->screen_number = 0;
# ifndef USE_IPHONE
  {
    // Find the first empty slot in live_displays and plug us in.
    int size = sizeof(jwxyz_live_displays) / sizeof(*jwxyz_live_displays);
    int i;
    for (i = 0; i < size; i++) {
      if (! jwxyz_live_displays[i])
        break;
    }
    if (i >= size) abort();
    jwxyz_live_displays[i] = d;
    d->screen_count = size;
    d->screen->screen_number = i;
  }
# endif // !USE_IPHONE

  d->screen->bitmap_info = CGBitmapContextGetBitmapInfo (cgc);
  d->screen->black = jwxyz_alloc_color (d, 0x0000, 0x0000, 0x0000, 0xFFFF);
  d->screen->white = jwxyz_alloc_color (d, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF);

# if 0
  // Tests for the image conversion modes.
  {
    const uint32_t key = 0x04030201;
#  ifdef __LITTLE_ENDIAN__
    assert (convert_px (key, 0) == key);
    assert (convert_px (key, 1) == 0x03020104);
    assert (convert_px (key, 3) == 0x01040302);
    assert (convert_px (key, 4) == 0x01020304);
    assert (convert_px (key, 5) == 0x04010203);
#  endif
    for (unsigned i = 0; i != 8; ++i) {
      assert (convert_px(convert_px(key, i), convert_mode_invert(i)) == key);
      assert (convert_mode_invert(convert_mode_invert(i)) == i);
    }

    for (unsigned i = 0; i != 8; ++i) {
      for (unsigned j = 0; j != 8; ++j)
        assert (convert_px(convert_px(key, i), j) ==
                convert_px(key, convert_mode_merge(i, j)));
    }
  }
# endif

  Visual *v = (Visual *) calloc (1, sizeof(Visual));
  v->class      = TrueColor;
  v->red_mask   = jwxyz_alloc_color (d, 0xFFFF, 0x0000, 0x0000, 0x0000);
  v->green_mask = jwxyz_alloc_color (d, 0x0000, 0xFFFF, 0x0000, 0x0000);
  v->blue_mask  = jwxyz_alloc_color (d, 0x0000, 0x0000, 0xFFFF, 0x0000);
  CGBitmapInfo byte_order = d->screen->bitmap_info & kCGBitmapByteOrderMask;
  Assert ( ! (d->screen->bitmap_info & kCGBitmapFloatComponents) &&
          (byte_order == kCGBitmapByteOrder32Little ||
           byte_order == kCGBitmapByteOrder32Big),
          "invalid bits per channel");
  v->bits_per_rgb = 8;
  d->screen->visual = v;
  
  d->timers_data = jwxyz_sources_init (XtDisplayToApplicationContext (d));

  d->window_background = BlackPixel(d,0);

  d->main_window = w;

  Assert (cgc, "no CGContext");
  return d;
}

void
jwxyz_free_display (Display *dpy)
{
  jwxyz_sources_free (dpy->timers_data);
  
# ifndef USE_IPHONE
  {
    // Find us in live_displays and clear that slot.
    int size = ScreenCount(dpy);
    int i;
    for (i = 0; i < size; i++) {
      if (dpy == jwxyz_live_displays[i]) {
        jwxyz_live_displays[i] = 0;
        break;
      }
    }
    if (i >= size) abort();
  }
# endif // !USE_IPHONE

  free (dpy->screen->visual);
  free (dpy->screen);
  free (dpy);
}


/* Call this after any modification to the bits on a Pixmap or Window.
   Most Pixmaps are used frequently as sources and infrequently as
   destinations, so it pays to cache the data as a CGImage as needed.
 */
void
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
jwxyz_window_resized (Display *dpy)
{
  Window w = dpy->main_window;

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
    // Auuugh!
    if (! dpy->cgdpy) {
      p.x = p.y = 0;
      CGGetDisplaysWithPoint (p, 1, &dpy->cgdpy, &n);
    }
    Assert (dpy->cgdpy, "unable to find CGDisplay");
  }
# endif // USE_IPHONE

/*
  {
    // Figure out this screen's colorspace, and use that for every CGImage.
    //
    CMProfileRef profile = 0;
    CMGetProfileByAVID ((CMDisplayIDType) dpy->cgdpy, &profile);
    Assert (profile, "unable to find colorspace profile");
    dpy->colorspace = CGColorSpaceCreateWithPlatformColorSpace (profile);
    Assert (dpy->colorspace, "unable to find colorspace");
  }
 */

  // WTF?  It's faster if we *do not* use the screen's colorspace!
  //
  dpy->colorspace = CGColorSpaceCreateDeviceRGB();

  invalidate_drawable_cache (w);
}


void
jwxyz_flush_context (Display *dpy)
{
  // CGContextSynchronize is another possibility.
  CGContextFlush(dpy->main_window->cgc);
}

jwxyz_sources_data *
display_sources_data (Display *dpy)
{
  return dpy->timers_data;
}


Window
XRootWindow (Display *dpy, int screen)
{
  return dpy ? dpy->main_window : 0;
}

Screen *
XDefaultScreenOfDisplay (Display *dpy)
{
  return dpy ? dpy->screen : 0;
}

Visual *
XDefaultVisualOfScreen (Screen *screen)
{
  return screen ? screen->visual : 0;
}

Display *
XDisplayOfScreen (Screen *s)
{
  return s ? s->dpy : 0;
}

int
XDisplayNumberOfScreen (Screen *s)
{
  return 0;
}

int
XScreenNumberOfScreen (Screen *s)
{
  return s? s->screen_number : 0;
}

int
jwxyz_ScreenCount (Display *dpy)
{
  return dpy ? dpy->screen_count : 0;
}

unsigned long
XBlackPixelOfScreen(Screen *screen)
{
  return screen->black;
}

unsigned long
XWhitePixelOfScreen(Screen *screen)
{
  return screen->white;
}

unsigned long
XCellsOfScreen(Screen *screen)
{
  Visual *v = screen->visual;
  return v->red_mask | v->green_mask | v->blue_mask;
}


void
set_color (Display *dpy, CGContextRef cgc, unsigned long argb,
           unsigned int depth, Bool alpha_allowed_p, Bool fill_p)
{
  jwxyz_validate_pixel (dpy, argb, depth, alpha_allowed_p);
  if (depth == 1) {
    if (fill_p)
      CGContextSetGrayFillColor   (cgc, (argb ? 1.0 : 0.0), 1.0);
    else
      CGContextSetGrayStrokeColor (cgc, (argb ? 1.0 : 0.0), 1.0);
  } else {
    float rgba[4];
    query_color_float (dpy, argb, rgba);
    if (fill_p)
      CGContextSetRGBFillColor   (cgc, rgba[0], rgba[1], rgba[2], rgba[3]);
    else
      CGContextSetRGBStrokeColor (cgc, rgba[0], rgba[1], rgba[2], rgba[3]);
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

  XRectangle wr = d->frame;
  CGRect to;
  to.origin.x    = wr.x + gc->gcv.clip_x_origin;
  to.origin.y    = wr.y + wr.height - gc->gcv.clip_y_origin
                    - p->frame.height;
  to.size.width  = p->frame.width;
  to.size.height = p->frame.height;

  CGContextClipToMask (d->cgc, to, gc->clip_mask);
}


/* Pushes a GC context; sets BlendMode and ClipMask.
 */
void
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


/* Pushes a GC context; sets BlendMode, ClipMask, Fill, and Stroke colors.
 */
void
push_color_gc (Display *dpy, Drawable d, GC gc, unsigned long color,
               Bool antialias_p, Bool fill_p)
{
  push_gc (d, gc);

  int depth = gc->depth;
  switch (gc->gcv.function) {
    case GXset:   color = (depth == 1 ? 1 : WhitePixel(dpy,0)); break;
    case GXclear: color = (depth == 1 ? 0 : BlackPixel(dpy,0)); break;
  }

  CGContextRef cgc = d->cgc;
  set_color (dpy, cgc, color, depth, gc->gcv.alpha_allowed_p, fill_p);
  CGContextSetShouldAntialias (cgc, antialias_p);
}


/* Pushes a GC context; sets Fill and Stroke colors to the foreground color.
 */
static void
push_fg_gc (Display *dpy, Drawable d, GC gc, Bool fill_p)
{
  push_color_gc (dpy, d, gc, gc->gcv.foreground, gc->gcv.antialias_p, fill_p);
}

static Bool
bitmap_context_p (Drawable d)
{
  return True;
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
  XRectangle wr = d->frame;

# ifdef XDRAWPOINTS_CGDATA

  if (bitmap_context_p (d))
  {
    CGContextRef cgc = d->cgc;
    void *data = CGBitmapContextGetData (cgc);
    size_t bpr = CGBitmapContextGetBytesPerRow (cgc);
    size_t w = CGBitmapContextGetWidth (cgc);
    size_t h = CGBitmapContextGetHeight (cgc);

    Assert (data, "no bitmap data in Drawable");

    unsigned long argb = gc->gcv.foreground;
    jwxyz_validate_pixel (dpy, argb, gc->depth, gc->gcv.alpha_allowed_p);
    if (gc->depth == 1)
      argb = (gc->gcv.foreground ? WhitePixel(dpy,0) : BlackPixel(dpy,0));

    CGFloat x0 = wr.x;
    CGFloat y0 = wr.y; // Y axis is refreshingly not flipped.

    // It's uglier, but faster, to hoist the conditional out of the loop.
    if (mode == CoordModePrevious) {
      CGFloat x = x0, y = y0;
      for (i = 0; i < count; i++, points++) {
        x += points->x;
        y += points->y;

        if (x >= 0 && x < w && y >= 0 && y < h) {
          unsigned int *p = (unsigned int *)
            ((char *) data + (size_t) y * bpr + (size_t) x * 4);
          *p = (unsigned int) argb;
        }
      }
    } else {
      for (i = 0; i < count; i++, points++) {
        CGFloat x = x0 + points->x;
        CGFloat y = y0 + points->y;

        if (x >= 0 && x < w && y >= 0 && y < h) {
          unsigned int *p = (unsigned int *)
            ((char *) data + (size_t) y * bpr + (size_t) x * 4);
          *p = (unsigned int) argb;
        }
      }
    }

  } else	/* d->type == WINDOW */

# endif /* XDRAWPOINTS_CGDATA */
  {
    push_fg_gc (dpy, d, gc, YES);

# ifdef XDRAWPOINTS_IMAGES

    unsigned long argb = gc->gcv.foreground;
    jwxyz_validate_pixel (dpy, argb, gc->depth, gc->gcv.alpha_allowed_p);
    if (gc->depth == 1)
      argb = (gc->gcv.foreground ? WhitePixel(dpy,0) : BlackPixel(dpy,0));

    CGDataProviderRef prov = CGDataProviderCreateWithData (NULL, &argb, 4,
                                                           NULL);
    CGImageRef cgi = CGImageCreate (1, 1,
                                    8, 32, 4,
                                    dpy->colorspace, 
                                    /* Host-ordered, since we're using the
                                       address of an int as the color data. */
                                    dpy->screen->bitmap_info,
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
        rect.origin.x = wr.x + points->x;
        rect.origin.y = wr.y + wr.height - points->y - 1;
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

    pop_gc (d, gc);
  }

  invalidate_drawable_cache (d);

  return 0;
}


CGPoint
map_point (Drawable d, int x, int y)
{
  const XRectangle *wr = &d->frame;
  CGPoint p;
  p.x = wr->x + x;
  p.y = wr->y + wr->height - y;
  return p;
}


static void
adjust_point_for_line (GC gc, CGPoint *p)
{
  // Here's the authoritative discussion on how X draws lines:
  // http://www.x.org/releases/current/doc/xproto/x11protocol.html#requests:CreateGC:line-width
  if (gc->gcv.line_width <= 1) {
    /* Thin lines are "drawn using an unspecified, device-dependent
       algorithm", but seriously though, Bresenham's algorithm. Bresenham's
       algorithm runs to and from pixel centers.

       There's a few screenhacks (Maze, at the very least) that set line_width
       to 1 when it probably should be set to 0, so it's line_width <= 1
       instead of < 1.
     */
    p->x += 0.5;
    p->y -= 0.5;
  } else {
    /* Thick lines OTOH run from the upper-left corners of pixels. This means
       that a horizontal thick line of width 1 straddles two scan lines.
       Aliasing requires one of these scan lines be chosen; the following
       nudges the point so that the right choice is made. */
    p->y -= 1e-3;
  }
}


static CGPoint
point_for_line (Drawable d, GC gc, int x, int y)
{
  CGPoint result = map_point (d, x, y);
  adjust_point_for_line (gc, &result);
  return result;
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
    else {
      if (!w)
        w = 1; // Actually show zero-length lines.
      return XFillRectangle (dpy, d, gc, x1, y1, w, w);
    }
  }
  
  CGPoint p = point_for_line (d, gc, x1, y1);

  push_fg_gc (dpy, d, gc, NO);

  CGContextRef cgc = d->cgc;
  set_line_mode (cgc, &gc->gcv);
  CGContextBeginPath (cgc);
  CGContextMoveToPoint (cgc, p.x, p.y);
  p = point_for_line(d, gc, x2, y2);
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
  CGPoint p;
  push_fg_gc (dpy, d, gc, NO);

  CGContextRef cgc = d->cgc;

  set_line_mode (cgc, &gc->gcv);
  
  // if the first and last points coincide, use closepath to get
  // the proper line-joining.
  BOOL closed_p = (points[0].x == points[count-1].x &&
                   points[0].y == points[count-1].y);
  if (closed_p) count--;
  
  p = point_for_line(d, gc, points->x, points->y);
  points++;
  CGContextBeginPath (cgc);
  CGContextMoveToPoint (cgc, p.x, p.y);
  for (i = 1; i < count; i++) {
    if (mode == CoordModePrevious) {
      p.x += points->x;
      p.y -= points->y;
    } else {
      p = point_for_line(d, gc, points->x, points->y);
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

  CGContextRef cgc = d->cgc;

  push_fg_gc (dpy, d, gc, NO);
  set_line_mode (cgc, &gc->gcv);
  CGContextBeginPath (cgc);
  for (i = 0; i < count; i++) {
    CGPoint p = point_for_line (d, gc, segments->x1, segments->y1);
    CGContextMoveToPoint (cgc, p.x, p.y);
    p = point_for_line (d, gc, segments->x2, segments->y2);
    CGContextAddLineToPoint (cgc, p.x, p.y);
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
  XRectangle wr = win->frame;
  return XClearArea (dpy, win, 0, 0, wr.width, wr.height, 0);
}

unsigned long
jwxyz_window_background (Display *dpy)
{
  return dpy->window_background;
}

int
XSetWindowBackground (Display *dpy, Window w, unsigned long pixel)
{
  Assert (w && w->type == WINDOW, "not a window");
  jwxyz_validate_pixel (dpy, pixel, 32, NO);
  dpy->window_background = pixel;
  return 0;
}

void
jwxyz_fill_rects (Display *dpy, Drawable d, GC gc,
                  const XRectangle *rectangles, unsigned long nrectangles,
                  unsigned long pixel)
{
  Assert (!gc || gc->depth == jwxyz_drawable_depth (d), "depth mismatch");

  CGContextRef cgc = d->cgc;

  Bool fast_fill_p =
    bitmap_context_p (d) &&
    (!gc || (gc->gcv.function == GXcopy &&
             !gc->gcv.alpha_allowed_p &&
             !gc->gcv.clip_mask));

  if (!fast_fill_p) {
    if (gc)
      push_color_gc (dpy, d, gc, pixel, gc->gcv.antialias_p, YES);
    else
      set_color (dpy, d->cgc, pixel, jwxyz_drawable_depth (d), NO, YES);
  }

  for (unsigned i = 0; i != nrectangles; ++i) {

    int x = rectangles[i].x;
    int y = rectangles[i].y;
    unsigned long width = rectangles[i].width;
    unsigned long height = rectangles[i].height;

    if (fast_fill_p) {
      long   // negative_int > unsigned_int == 1
        dw = CGBitmapContextGetWidth (cgc),
        dh = CGBitmapContextGetHeight (cgc);

      if (x >= dw || y >= dh)
        continue;

      if (x < 0) {
        width += x;
        x = 0;
      }

      if (y < 0) {
        height += y;
        y = 0;
      }

      if (width <= 0 || height <= 0)
        continue;

      unsigned long max_width = dw - x;
      if (width > max_width)
        width = max_width;
      unsigned long max_height = dh - y;
      if (height > max_height)
        height = max_height;

      if (jwxyz_drawable_depth (d) == 1)
        pixel = pixel ? WhitePixel(dpy, 0) : BlackPixel(dpy, 0);

      size_t dst_bytes_per_row = CGBitmapContextGetBytesPerRow (d->cgc);
      void *dst = seek_xy (CGBitmapContextGetData (d->cgc),
                           dst_bytes_per_row, x, y);

      Assert(sizeof(wchar_t) == 4, "somebody changed the ABI");
      while (height) {
        // Would be nice if Apple used SSE/NEON in wmemset. Maybe someday.
        wmemset (dst, (wchar_t) pixel, width);
        --height;
        dst = (char *) dst + dst_bytes_per_row;
      }

    } else {
      CGRect r;
      r.origin = map_point (d, x, y);
      r.origin.y -= height;
      r.size.width = width;
      r.size.height = height;
      CGContextFillRect (cgc, r);
    }
  }

  if (!fast_fill_p && gc)
    pop_gc (d, gc);
  invalidate_drawable_cache (d);
}


int
XClearArea (Display *dpy, Window win, int x, int y, int w, int h, Bool exp)
{
  Assert (win && win->type == WINDOW, "not a window");
  jwxyz_fill_rect (dpy, win, 0, x, y, w, h, dpy->window_background);
  return 0;
}


int
XFillPolygon (Display *dpy, Drawable d, GC gc, 
              XPoint *points, int npoints, int shape, int mode)
{
  XRectangle wr = d->frame;
  int i;
  push_fg_gc (dpy, d, gc, YES);
  CGContextRef cgc = d->cgc;
  CGContextBeginPath (cgc);
  float x = 0, y = 0;
  for (i = 0; i < npoints; i++) {
    if (i > 0 && mode == CoordModePrevious) {
      x += points[i].x;
      y -= points[i].y;
    } else {
      x = wr.x + points[i].x;
      y = wr.y + wr.height - points[i].y;
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

int
jwxyz_draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
                unsigned int width, unsigned int height,
                int angle1, int angle2, Bool fill_p)
{
  XRectangle wr = d->frame;
  CGRect bound;
  bound.origin.x = wr.x + x;
  bound.origin.y = wr.y + wr.height - y - (int)height;
  bound.size.width = width;
  bound.size.height = height;
  
  CGPoint ctr;
  ctr.x = bound.origin.x + bound.size.width /2;
  ctr.y = bound.origin.y + bound.size.height/2;
  
  float r1 = radians (angle1/64.0);
  float r2 = radians (angle2/64.0) + r1;
  BOOL clockwise = angle2 < 0;
  BOOL closed_p = (angle2 >= 360*64 || angle2 <= -360*64);
  
  push_fg_gc (dpy, d, gc, fill_p);

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


XGCValues *
jwxyz_gc_gcv (GC gc)
{
  return &gc->gcv;
}


unsigned int
jwxyz_gc_depth (GC gc)
{
  return gc->depth;
}


GC
XCreateGC (Display *dpy, Drawable d, unsigned long mask, XGCValues *xgcv)
{
  struct jwxyz_GC *gc = (struct jwxyz_GC *) calloc (1, sizeof(*gc));
  gc->depth = jwxyz_drawable_depth (d);

  jwxyz_gcv_defaults (dpy, &gc->gcv, gc->depth);
  XChangeGC (dpy, gc, mask, xgcv);
  return gc;
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
  while (length-- > 0)
    *out++ = table[*in++];
}


int
XPutImage (Display *dpy, Drawable d, GC gc, XImage *ximage,
           int src_x, int src_y, int dest_x, int dest_y,
           unsigned int w, unsigned int h)
{
  XRectangle wr = d->frame;

  Assert (gc, "no GC");
  Assert ((w < 65535), "improbably large width");
  Assert ((h < 65535), "improbably large height");
  Assert ((src_x  < 65535 && src_x  > -65535), "improbably large src_x");
  Assert ((src_y  < 65535 && src_y  > -65535), "improbably large src_y");
  Assert ((dest_x < 65535 && dest_x > -65535), "improbably large dest_x");
  Assert ((dest_y < 65535 && dest_y > -65535), "improbably large dest_y");

  // Clip width and height to the bounds of the Drawable
  //
  if (dest_x + (int)w > wr.width) {
    if (dest_x > wr.width)
      return 0;
    w = wr.width - dest_x;
  }
  if (dest_y + (int)h > wr.height) {
    if (dest_y > wr.height)
      return 0;
    h = wr.height - dest_y;
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

  if (jwxyz_dumb_drawing_mode(dpy, d, gc, dest_x, dest_y, w, h))
    return 0;

  int bpl = ximage->bytes_per_line;
  int bpp = ximage->bits_per_pixel;
  int bsize = bpl * h;
  char *data = ximage->data;

  CGRect r;
  r.origin.x = wr.x + dest_x;
  r.origin.y = wr.y + wr.height - dest_y - (int)h;
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
                                    dpy->screen->bitmap_info,
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
    push_fg_gc (dpy, d, gc, YES);

    CGContextFillRect (cgc, r);				// foreground color
    CGContextClipToMask (cgc, r, mask);
    set_color (dpy, cgc, gc->gcv.background, gc->depth, NO, YES);
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
  size_t depth, ibpp, ibpl;
  convert_mode_t mode;

  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  CGContextRef cgc = d->cgc;

  {
    depth = jwxyz_drawable_depth (d);
    mode = convert_mode_to_rgba (dpy->screen->bitmap_info);
    ibpp = CGBitmapContextGetBitsPerPixel (cgc);
    ibpl = CGBitmapContextGetBytesPerRow (cgc);
    data = CGBitmapContextGetData (cgc);
    Assert (data, "CGBitmapContextGetData failed");
  }
  
  // data points at (x,y) with ibpl rowstride.  ignore x,y from now on.
  data += (y * ibpl) + (x * (ibpp/8));
  
  format = (depth == 1 ? XYPixmap : ZPixmap);
  XImage *image = XCreateImage (dpy, 0, (unsigned int) depth,
                                format, 0, 0, width, height, 0, 0);
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
    const unsigned char *iline = data;
    unsigned char *oline = (unsigned char *) image->data;

    mode = convert_mode_merge (mode,
             convert_mode_invert (
               convert_mode_to_rgba (dpy->screen->bitmap_info)));

    for (yy = 0; yy < height; yy++) {

      convert_row ((uint32_t *)oline, iline, width, mode, ibpp);

      oline += obpl;
      iline += ibpl;
    }
  }

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

  XRectangle winr = d->frame;
  float rw = winr.width  / imgr.width;
  float rh = winr.height / imgr.height;
  float r = (rw < rh ? rw : rh);

  CGRect dst, dst2;
  dst.size.width  = imgr.width  * r;
  dst.size.height = imgr.height * r;
  dst.origin.x = (winr.width  - dst.size.width)  / 2;
  dst.origin.y = (winr.height - dst.size.height) / 2;

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
    jwxyz_fill_rect (dpy, d, 0, 0, 0, winr.width, winr.height,
                     jwxyz_drawable_depth (d) == 1 ? 0 : BlackPixel(dpy,0));
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
XCreatePixmap (Display *dpy, Drawable d,
               unsigned int width, unsigned int height, unsigned int depth)
{
  if (!dpy) abort();
  char *data = (char *) malloc (width * height * 4);
  if (! data) return 0;

  Pixmap p = (Pixmap) calloc (1, sizeof(*p));
  p->type = PIXMAP;
  p->frame.width       = width;
  p->frame.height      = height;
  p->pixmap.depth      = depth;
  p->pixmap.cgc_buffer = data;
  
  /* Quartz doesn't have a 1bpp image type.
     Used to use 8bpp gray images instead of 1bpp, but some Mac video cards
     don't support that!  So we always use 32bpp, regardless of depth. */

  p->cgc = CGBitmapContextCreate (data, width, height,
                                  8, /* bits per component */
                                  width * 4, /* bpl */
                                  dpy->colorspace,
                                  dpy->screen->bitmap_info);
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

  Pixmap p2 = 0;

  Window root;
  int x, y;
  unsigned int width, height, border_width, depth;
  if (XGetGeometry (dpy, p, &root,
                    &x, &y, &width, &height, &border_width, &depth)) {
    XGCValues gcv;
    gcv.function = GXcopy;
    GC gc = XCreateGC (dpy, p, GCFunction, &gcv);
    if (gc) {
      p2 = XCreatePixmap (dpy, p, width, height, depth);
      if (p2)
        XCopyArea (dpy, p, p2, gc, 0, 0, width, height, 0, 0);
      XFreeGC (dpy, gc);
    }
  }

  Assert (p2, "could not copy pixmap");

  return p2;
}


/* Font metric terminology, as used by X11:

   "lbearing" is the distance from the logical origin to the leftmost pixel.
   If a character's ink extends to the left of the origin, it is negative.

   "rbearing" is the distance from the logical origin to the rightmost pixel.

   "descent" is the distance from the logical origin to the bottommost pixel.
   For characters with descenders, it is positive.  For superscripts, it
   is negative.

   "ascent" is the distance from the logical origin to the topmost pixel.
   It is the number of pixels above the baseline.

   "width" is the distance from the logical origin to the position where
   the logical origin of the next character should be placed.

   If "rbearing" is greater than "width", then this character overlaps the
   following character.  If smaller, then there is trailing blank space.
 */
static void
utf8_metrics (Font fid, NSString *nsstr, XCharStruct *cs)
{
  // Returns the metrics of the multi-character, single-line UTF8 string.

  NSFont *nsfont = fid->nsfont;
  Drawable d = XRootWindow (fid->dpy, 0);

  CGContextRef cgc = d->cgc;
  NSDictionary *attr =
    [NSDictionary dictionaryWithObjectsAndKeys:
                    nsfont, NSFontAttributeName,
                  nil];
  NSAttributedString *astr = [[NSAttributedString alloc]
                               initWithString:nsstr
                                   attributes:attr];
  CTLineRef ctline = CTLineCreateWithAttributedString (
                       (__bridge CFAttributedStringRef) astr);
  CGContextSetTextPosition (cgc, 0, 0);
  CGContextSetShouldAntialias (cgc, True);  // #### Guess?

  memset (cs, 0, sizeof(*cs));

  // "CTRun represents set of consecutive glyphs sharing the same
  // attributes and direction".
  //
  // We also get multiple runs any time font subsitution happens:
  // E.g., if the current font is Verdana-Bold, a &larr; character
  // in the NSString will actually be rendered in LucidaGrande-Bold.
  //
  int count = 0;
  for (id runid in (NSArray *)CTLineGetGlyphRuns(ctline)) {
    CTRunRef run = (CTRunRef) runid;
    CFRange r = { 0, };
    CGRect bbox = CTRunGetImageBounds (run, cgc, r);
    CGFloat ascent, descent, leading;
    CGFloat advancement =
      CTRunGetTypographicBounds (run, r, &ascent, &descent, &leading);

# ifndef USE_IPHONE
    // Only necessary for when LCD smoothing is enabled, which iOS doesn't do.
    bbox.origin.x    -= 2.0/3.0;
    bbox.size.width  += 4.0/3.0;
    bbox.size.height += 1.0/2.0;
# endif

    // Create the metrics for this run:
    XCharStruct cc;
    cc.ascent   = ceil  (bbox.origin.y + bbox.size.height);
    cc.descent  = ceil (-bbox.origin.y);
    cc.lbearing = floor (bbox.origin.x);
    cc.rbearing = ceil  (bbox.origin.x + bbox.size.width);
    cc.width    = floor (advancement + 0.5);

    // Add those metrics into the cumulative metrics:
    if (count == 0)
      *cs = cc;
    else
      {
        cs->ascent   = MAX (cs->ascent,     cc.ascent);
        cs->descent  = MAX (cs->descent,    cc.descent);
        cs->lbearing = MIN (cs->lbearing,   cs->width + cc.lbearing);
        cs->rbearing = MAX (cs->rbearing,   cs->width + cc.rbearing);
        cs->width    = MAX (cs->width,      cs->width + cc.width);
      }

    // Why no y? What about vertical text?
    // XCharStruct doesn't encapsulate that but XGlyphInfo does.

    count++;
  }

  [astr release];
  CFRelease (ctline);
}



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
    Assert(0, "broken NSFont in fid");
    return;
  }

  int first = 32;
  int last = 255;

  XFontStruct *f = &fid->metrics;
  XCharStruct *min = &f->min_bounds;
  XCharStruct *max = &f->max_bounds;

  f->fid               = fid;
  f->min_char_or_byte2 = first;
  f->max_char_or_byte2 = last;
  f->default_char      = 'M';
  f->ascent            =  ceil ([fid->nsfont ascender]);
  f->descent           = -floor ([fid->nsfont descender]);

  min->width    = 32767;  // set to smaller values in the loop
  min->ascent   = 32767;
  min->descent  = 32767;
  min->lbearing = 32767;
  min->rbearing = 32767;

  f->per_char = (XCharStruct *) calloc (last-first+2, sizeof (XCharStruct));

  for (int i = first; i <= last; i++) {
    XCharStruct *cs = &f->per_char[i-first];

    char s2[2];
    s2[0] = i;
    s2[1] = 0;
    NSString *nsstr = [NSString stringWithCString:s2
                               encoding:NSISOLatin1StringEncoding];
    utf8_metrics (fid, nsstr, cs);

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

# if 0
    fprintf(stderr, " %3d %c: w=%3d lb=%3d rb=%3d as=%3d ds=%3d "
                    " bb=%5.1f x %5.1f @ %5.1f %5.1f  adv=%5.1f %5.1f\n",
            i, i, cs->width, cs->lbearing, cs->rbearing, 
            cs->ascent, cs->descent,
            bbox.size.width, bbox.size.height,
            bbox.origin.x, bbox.origin.y,
            advancement.width, advancement.height);
# endif // 0
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

  // build XFontProps
  f->n_properties = 1;
  f->properties = malloc (sizeof(*f->properties) * f->n_properties);
  f->properties[0].name = XA_FONT;
  Assert (sizeof (f->properties[0].card32) >= sizeof (char *),
          "atoms probably needs a real implementation");
  // If XInternAtom is ever implemented, use it here.
  f->properties[0].card32 = (char *)fid->xa_font;

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
  fid2->xa_font = strdup (fid->xa_font);
//  [fid2->nsfont retain];
  fid2->metrics.fid = fid2;

  return fid2;
}


static NSArray *
font_family_members (NSString *family_name)
{
# ifndef USE_IPHONE
  return [[NSFontManager sharedFontManager]
          availableMembersOfFontFamily:family_name];
# else
  return [UIFont fontNamesForFamilyName:family_name];
# endif
}


static NSString *
default_font_family (NSFontTraitMask require)
{
  return require & NSFixedPitchFontMask ? @"Courier" : @"Verdana";
}


static NSFont *
try_font (NSFontTraitMask traits, NSFontTraitMask mask,
          NSString *family_name, float size,
          char **name_ret)
{
  Assert (size > 0, "zero font size");

  NSArray *family_members = font_family_members (family_name);
  if (!family_members.count)
    family_members = font_family_members (default_font_family (traits));

# ifndef USE_IPHONE
  for (unsigned k = 0; k != family_members.count; ++k) {

    NSArray *member = [family_members objectAtIndex:k];
    NSFontTraitMask font_mask =
    [(NSNumber *)[member objectAtIndex:3] unsignedIntValue];

    if ((font_mask & mask) == traits) {

      NSString *name = [member objectAtIndex:0];
      NSFont *f = [NSFont fontWithName:name size:size];
      if (!f)
        break;

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
        break;
      }
      // NSLog(@"using \"%@\": %d", name, enc);

      // *name_ret = strdup ([name cStringUsingEncoding:NSUTF8StringEncoding]);
      *name_ret = strdup (name.UTF8String);
      return f;
    }
  }
# else // USE_IPHONE

  // This trick needs iOS 3.1, see "Using SDK-Based Development".
  Class has_font_descriptor = [UIFontDescriptor class];

  for (NSString *fn in family_members) {
# define MATCH(X) \
         ([fn rangeOfString:X options:NSCaseInsensitiveSearch].location \
         != NSNotFound)

    NSFontTraitMask font_mask;
    if (has_font_descriptor) {
      // This only works on iOS 7 and later.
      font_mask = [[UIFontDescriptor
                    fontDescriptorWithFontAttributes:
                    @{UIFontDescriptorNameAttribute:fn}]
                   symbolicTraits];
    } else {
      font_mask = 0;
      if (MATCH(@"Bold"))
        font_mask |= NSBoldFontMask;
      if (MATCH(@"Italic") || MATCH(@"Oblique"))
        font_mask |= NSItalicFontMask;
      if (MATCH(@"Courier"))
        font_mask |= NSFixedPitchFontMask;
    }

    if ((font_mask & mask) == traits) {

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
         break;

      *name_ret = strdup (fn.UTF8String);
      return [UIFont fontWithName:fn size:size];
    }
# undef MATCH
  }

# endif

  return NULL;
}


/* On Cocoa and iOS, fonts may be specified as "Georgia Bold 24" instead
   of XLFD strings; also they can be comma-separated strings with multiple
   font names.  First one that exists wins.
 */
static NSFont *
try_native_font (const char *name, float scale,
                 char **name_ret, float *size_ret, char **xa_font)
{
  if (!name) return 0;
  const char *spc = strrchr (name, ' ');
  if (!spc) return 0;

  NSFont *f = 0;
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

    size *= scale;

    name2[strlen(name2) - strlen(spc)] = 0;

    NSString *nsname = [NSString stringWithCString:name2
                                          encoding:NSUTF8StringEncoding];
    f = [NSFont fontWithName:nsname size:size];
    if (f) {
      *name_ret = strdup (name2);
      *size_ret = size;
      *xa_font = strdup (name); // Maybe this should be an XLFD?
      break;
    } else {
      NSLog(@"No native font: \"%@\" %.0f", nsname, size);
# if 0
      for (NSString *fam in [UIFont familyNames]) {
        NSLog(@"Family: %@", fam);
        for (NSString *f in [UIFont fontNamesForFamilyName:fam]) {
          NSLog(@"  Font: %@", f);
        }
      }
# endif
    }
  }

  free (otoken);
  return f;
}


/* Returns a random font in the given size and face.
 */
static NSFont *
random_font (NSFontTraitMask traits, NSFontTraitMask mask,
             float size, NSString **family_ret, char **name_ret)
{

# ifndef USE_IPHONE
  // Providing Unbold or Unitalic in the mask for availableFontNamesWithTraits
  // returns an empty list, at least on a system with default fonts only.
  NSArray *families = [[NSFontManager sharedFontManager]
                       availableFontFamilies];
  if (!families) return 0;
# else
  NSArray *families = [UIFont familyNames];

  // There are many dups in the families array -- uniquify it.
  {
    NSArray *sorted_families =
    [families sortedArrayUsingSelector:@selector(compare:)];
    NSMutableArray *new_families =
    [NSMutableArray arrayWithCapacity:sorted_families.count];

    NSString *prev_family = @"";
    for (NSString *family in sorted_families) {
      if ([family compare:prev_family])
        [new_families addObject:family];
      prev_family = family;
    }

    families = new_families;
  }
# endif // USE_IPHONE

  long n = [families count];
  if (n <= 0) return 0;

  int j;
  for (j = 0; j < n; j++) {
    int i = random() % n;
    NSString *family_name = [families objectAtIndex:i];

    NSFont *result = try_font (traits, mask, family_name, size, name_ret);
    if (result) {
      [*family_ret release];
      *family_ret = family_name;
      [*family_ret retain];
      return result;
    }
  }

  // None of the fonts support ASCII?
  return 0;
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


static NSFont *
try_xlfd_font (Display *dpy, const char *name, float scale,
               char **name_ret, float *size_ret, char **xa_font)
{
  NSFont *nsfont = 0;
  NSString *family_name = nil;
  NSFontTraitMask require = 0,
   // Default mask is for the built-in X11 font aliases.
   mask = NSFixedPitchFontMask | NSBoldFontMask | NSItalicFontMask;
  BOOL rand  = NO;
  float size = 0;
  char *ps_name = 0;

  const char *s = (name ? name : "");

  size_t L = strlen (s);
# define CMP(STR) (L == strlen(STR) && !strncasecmp (s, (STR), L))
# define UNSPEC   (L == 0 || L == 1 && *s == '*')
  if      (CMP ("6x10"))     size = 8,  require |= NSFixedPitchFontMask;
  else if (CMP ("6x10bold")) size = 8,  require |= NSFixedPitchFontMask | NSBoldFontMask;
  else if (CMP ("fixed"))    size = 12, require |= NSFixedPitchFontMask;
  else if (CMP ("9x15"))     size = 12, require |= NSFixedPitchFontMask;
  else if (CMP ("9x15bold")) size = 12, require |= NSFixedPitchFontMask | NSBoldFontMask;
  else if (CMP ("vga"))      size = 12, require |= NSFixedPitchFontMask;
  else if (CMP ("console"))  size = 12, require |= NSFixedPitchFontMask;
  else if (CMP ("gallant"))  size = 12, require |= NSFixedPitchFontMask;
  else {

    NSFontTraitMask forbid = 0;

    // Incorrect fields are ignored.

    if (*s == '-')
      ++s;
    const char *s2 = xlfd_field_end(s);

    // Foundry (ignore)

    L = xlfd_next (&s, &s2); // Family name
    // This used to substitute Georgia for Times. Now it doesn't.
    if (CMP ("random")) {
      rand = YES;
    } else if (CMP ("fixed")) {
      require |= NSFixedPitchFontMask;
      family_name = @"Courier";
    } else if (!UNSPEC) {
      family_name = [[[NSString alloc] initWithBytes:s
                                              length:L
                                            encoding:NSUTF8StringEncoding]
                     autorelease];
    }

    L = xlfd_next (&s, &s2); // Weight name
    if (CMP ("bold") || CMP ("demibold"))
      require |= NSBoldFontMask;
    else if (CMP ("medium") || CMP ("regular"))
      forbid |= NSBoldFontMask;

    L = xlfd_next (&s, &s2); // Slant
    if (CMP ("i") || CMP ("o"))
      require |= NSItalicFontMask;
    else if (CMP ("r"))
      forbid |= NSItalicFontMask;

    xlfd_next (&s, &s2); // Set width name (ignore)
    xlfd_next (&s, &s2); // Add style name (ignore)

    xlfd_next (&s, &s2); // Pixel size (ignore)

    xlfd_next (&s, &s2); // Point size
    char *s3;
    uintmax_t n = strtoumax(s, &s3, 10);
    if (s2 == s3)
      size = n / 10.0;

    xlfd_next (&s, &s2); // Resolution X (ignore)
    xlfd_next (&s, &s2); // Resolution Y (ignore)

    xlfd_next (&s, &s2); // Spacing
    if (CMP ("p"))
      forbid |= NSFixedPitchFontMask;
    else if (CMP ("m") || CMP ("c"))
      require |= NSFixedPitchFontMask;

    // Don't care about average_width or charset registry.

    mask = require | forbid;
  }
# undef CMP
# undef UNSPEC

  if (!family_name && !rand)
    family_name = default_font_family (require);

  if (size < 6 || size > 1000)
    size = 12;

  size *= scale;

  if (rand) {
    nsfont   = random_font (require, mask, size, &family_name, &ps_name);
    [family_name autorelease];
  }

  if (!nsfont)
    nsfont   = try_font (require, mask, family_name, size, &ps_name);

  // if that didn't work, turn off attibutes until it does
  // (e.g., there is no "Monaco-Bold".)
  //
  if (!nsfont && (mask & NSItalicFontMask)) {
    require &= ~NSItalicFontMask;
    mask &= ~NSItalicFontMask;
    nsfont = try_font (require, mask, family_name, size, &ps_name);
  }
  if (!nsfont && (mask & NSBoldFontMask)) {
    require &= ~NSBoldFontMask;
    mask &= ~NSBoldFontMask;
    nsfont = try_font (require, mask, family_name, size, &ps_name);
  }
  if (!nsfont && (mask & NSFixedPitchFontMask)) {
    require &= ~NSFixedPitchFontMask;
    mask &= ~NSFixedPitchFontMask;
    nsfont = try_font (require, mask, family_name, size, &ps_name);
  }

  if (nsfont) {
    unsigned dpi_d = XDisplayHeightMM (dpy,0) * 10 / 2;
    unsigned dpi = (254 * XDisplayHeight (dpy,0) + dpi_d) / (2 * dpi_d);
    *name_ret = ps_name;
    *size_ret = size;
    float actual_size = size / scale;
    asprintf(xa_font, "-*-%s-%s-%c-*-*-%u-%u-%u-%u-%c-0-iso10646-1",
             family_name.UTF8String,
             (require & NSBoldFontMask) ? "bold" : "medium",
             (require & NSItalicFontMask) ? 'o' : 'r',
             (unsigned)(dpi * actual_size / 72.27 + 0.5),
             (unsigned)(actual_size * 10 + 0.5), dpi, dpi,
             (require & NSFixedPitchFontMask) ? 'm' : 'p');
    return nsfont;
  } else {
    if (ps_name) free (ps_name);
    return 0;
  }
}


Font
XLoadFont (Display *dpy, const char *name)
{
  Font fid = (Font) calloc (1, sizeof(*fid));

  float scale = 1;

# ifdef USE_IPHONE
  /* Since iOS screens are physically smaller than desktop screens, scale up
     the fonts to make them more readable.

     Note that X11 apps on iOS also have the backbuffer sized in points
     instead of pixels, resulting in an effective X11 screen size of 768x1024
     or so, even if the display has significantly higher resolution.  That is
     unrelated to this hack, which is really about DPI.
   */
  scale = dpy->main_window->window.view.hackedContentScaleFactor;
  if (scale < 1) // iPad Pro magnifies the backbuffer by 3x, which makes text
    scale = 1;   // excessively blurry in BSOD.
# endif

  fid->dpy = dpy;
  fid->nsfont = try_native_font (name, scale, &fid->ps_name, &fid->size,
                                 &fid->xa_font);

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
    fid->nsfont = try_xlfd_font (dpy, name, scale, &fid->ps_name, &fid->size,
                                 &fid->xa_font);

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
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);
  gc->gcv.font = copy_font (fid);
  [gc->gcv.font->nsfont retain];
  CFRetain (gc->gcv.font->nsfont);   // needed for garbage collection?
  return 0;
}


XFontSet
XCreateFontSet (Display *dpy, char *name, 
                char ***missing_charset_list_return,
                int *missing_charset_count_return,
                char **def_string_return)
{
  char *name2 = strdup (name);
  char *s = strchr (name, ",");
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


const char *
jwxyz_nativeFontName (Font f, float *size)
{
  if (size) *size = f->size;
  return f->ps_name;
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


// Returns the verbose Unicode name of this character, like "agrave" or
// "daggerdouble".  Used by fontglide debugMetrics.
//
char *
jwxyz_unicode_character_name (Font fid, unsigned long uc)
{
  char *ret = 0;
  CTFontRef ctfont =
    CTFontCreateWithName ((CFStringRef) [fid->nsfont fontName],
                          [fid->nsfont pointSize],
                          NULL);
  Assert (ctfont, "no CTFontRef for UIFont");

  CGGlyph cgglyph;
  if (CTFontGetGlyphsForCharacters (ctfont, (UniChar *) &uc, &cgglyph, 1)) {
    CGFontRef cgfont = CTFontCopyGraphicsFont (ctfont, 0);
    NSString *name = (NSString *) CGFontCopyGlyphNameForGlyph(cgfont, cgglyph);
    ret = (name ? strdup ([name UTF8String]) : 0);
    CGFontRelease (cgfont);
    [name release];
  }

  CFRelease (ctfont);
  return ret;
}


// Given a UTF8 string, return an NSString.  Bogus UTF8 characters are ignored.
// We have to do this because stringWithCString returns NULL if there are
// any invalid characters at all.
//
static NSString *
sanitize_utf8 (const char *in, int in_len, Bool *latin1_pP)
{
  int out_len = in_len * 4;   // length of string might increase
  char *s2 = (char *) malloc (out_len);
  char *out = s2;
  const char *in_end  = in  + in_len;
  const char *out_end = out + out_len;
  Bool latin1_p = True;

  while (in < in_end)
    {
      unsigned long uc;
      long L1 = utf8_decode ((const unsigned char *) in, in_end - in, &uc);
      long L2 = utf8_encode (uc, out, out_end - out);
      in  += L1;
      out += L2;
      if (uc > 255) latin1_p = False;
    }
  *out = 0;
  NSString *nsstr =
    [NSString stringWithCString:s2 encoding:NSUTF8StringEncoding];
  free (s2);
  if (latin1_pP) *latin1_pP = latin1_p;
  return (nsstr ? nsstr : @"");
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

  NSString *nsstr = [[[NSString alloc] initWithBytes:s
                                              length:length
                                            encoding:NSISOLatin1StringEncoding]
                     autorelease];
  utf8_metrics (f->fid, nsstr, cs);
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
    NSString *nsstr = [NSString stringWithCString:utf8
                                encoding:NSUTF8StringEncoding];
    utf8_metrics (f->fid, nsstr, cs);
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
  Bool latin1_p;
  NSString *nsstr = sanitize_utf8 (str, len, &latin1_p);
  XCharStruct cs;

  utf8_metrics (set->font->fid, nsstr, &cs);

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


static int
draw_string (Display *dpy, Drawable d, GC gc, int x, int y,
             NSString *nsstr)
{
  if (! nsstr) return 1;

  XRectangle wr = d->frame;
  CGContextRef cgc = d->cgc;

  unsigned long argb = gc->gcv.foreground;
  if (gc->depth == 1) argb = (argb ? WhitePixel(dpy,0) : BlackPixel(dpy,0));
  float rgba[4];
  query_color_float (dpy, argb, rgba);
  NSColor *fg = [NSColor colorWithDeviceRed:rgba[0]
                                      green:rgba[1]
                                       blue:rgba[2]
                                      alpha:rgba[3]];

  if (!gc->gcv.font) {
    Assert (0, "no font");
    return 1;
  }

  /* This crashes on iOS 5.1 because NSForegroundColorAttributeName,
      NSFontAttributeName, and NSAttributedString are only present on iOS 6
      and later.  We could resurrect the Quartz code from v5.29 and do a
      runtime conditional on that, but that would be a pain in the ass.
      Probably time to just make iOS 6 a requirement.
   */

  NSDictionary *attr =
    [NSDictionary dictionaryWithObjectsAndKeys:
                    gc->gcv.font->nsfont, NSFontAttributeName,
                    fg, NSForegroundColorAttributeName,
                  nil];

  // Don't understand why we have to do both set_color and
  // NSForegroundColorAttributeName, but we do.
  //
  set_color (dpy, cgc, argb, 32, NO, YES);

  NSAttributedString *astr = [[NSAttributedString alloc]
                               initWithString:nsstr
                                   attributes:attr];
  CTLineRef dl = CTLineCreateWithAttributedString (
                   (__bridge CFAttributedStringRef) astr);

  // Not sure why this is necessary, but xoff is positive when the first
  // character on the line has a negative lbearing.  Without this, the
  // string is rendered with the first ink at 0 instead of at lbearing.
  // I have not seen xoff be negative, so I'm not sure if that can happen.
  //
  // Test case: "Combining Double Tilde" U+0360 (\315\240) followed by
  // a letter.
  //
  CGFloat xoff = CTLineGetOffsetForStringIndex (dl, 0, NULL);
  Assert (xoff >= 0, "unexpected CTLineOffset");
  x -= xoff;

  CGContextSetTextPosition (cgc,
                            wr.x + x,
                            wr.y + wr.height - y);
  CGContextSetShouldAntialias (cgc, gc->gcv.antialias_p);

  CTLineDraw (dl, cgc);
  CFRelease (dl);
  [astr release];

  invalidate_drawable_cache (d);
  return 0;
}


int
XDrawString (Display *dpy, Drawable d, GC gc, int x, int y,
             const char  *str, int len)
{
  char *s2 = (char *) malloc (len + 1);
  strncpy (s2, str, len);
  s2[len] = 0;
  NSString *nsstr = [NSString stringWithCString:s2
                                       encoding:NSISOLatin1StringEncoding];
  int ret = draw_string (dpy, d, gc, x, y, nsstr);
  free (s2);
  return ret;
}


int
XDrawString16 (Display *dpy, Drawable d, GC gc, int x, int y,
             const XChar2b *str, int len)
{
  char *s2 = XChar2b_to_utf8 (str, 0);   // already sanitized
  NSString *nsstr =
    [NSString stringWithCString:s2 encoding:NSUTF8StringEncoding];
  int ret = draw_string (dpy, d, gc, x, y, nsstr);
  free (s2);
  return ret;
}


void
Xutf8DrawString (Display *dpy, Drawable d, XFontSet set, GC gc,
                 int x, int y, const char *str, int len)
{
  char *s2 = (char *) malloc (len + 1);
  strncpy (s2, str, len);
  s2[len] = 0;
  NSString *nsstr = sanitize_utf8 (str, len, 0);
  draw_string (dpy, d, gc, x, y, nsstr);
  free (s2);
}


int
XDrawImageString (Display *dpy, Drawable d, GC gc, int x, int y,
                  const char *str, int len)
{
  int ascent, descent, dir;
  XCharStruct cs;
  XTextExtents (&gc->gcv.font->metrics, str, len,
                &dir, &ascent, &descent, &cs);
  jwxyz_fill_rect (dpy, d, gc,
                   x + MIN (0, cs.lbearing),
                   y - MAX (0, ascent),
                   MAX (MAX (0, cs.rbearing) -
                        MIN (0, cs.lbearing),
                        cs.width),
                   MAX (0, ascent) + MAX (0, descent),
                   gc->gcv.background);
  return XDrawString (dpy, d, gc, x, y, str, len);
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

#endif // JWXYZ_QUARTZ -- entire file
