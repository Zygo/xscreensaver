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
   Xlib and that do OpenGL-ish things that bear some resemblance to the
   things that Xlib might have done.

   This is the version of jwxyz for Android.  The version used by MacOS
   and iOS is in jwxyz.m.
 */

/* Be advised, this is all very much a work in progress.

   TODO: The following should be implemented before OpenGL can be considered
   practical here:
   - Above all, pick the smallest not-yet working hack that utilizes the
     needed functionality.
     - Half-ass the drawing functions.
   - [OK] What Interference needs
   - Fast Pixmaps
   - Whatever clipping is used in XScreenSaver (shape and/or bitmap clipping)
   - Delayed context creation to support anti-aliasing/multisampling
   - Everything these hacks need:
     - FuzzyFlakes (needs wide lines)
     - Greynetic
     - [OK] Deluxe
   - [OK] Get DangerBall going.
   - [OK] iOS.
     - [Interference, so far...] And fast, too.
   - And text really needs to work for the FPS display. */

/* Also, Take note that OS X can actually run with 256 colors. */

/* TODO:
   - malloc error checking
   - Check max texture sizes for XGet/PutImage, XCopyArea.
   - Optional 5:5:5 16-bit color
*/

/* Techniques & notes:
   - iOS: OpenGL ES 2.0 isn't always available. Use OpenGL ES 1.1.
   - OS X: Drivers can go back to OpenGL 1.1 (GeForce 2 MX on 10.5.8).
   - Use stencil buffers (OpenGL 1.0+) for bitmap clipping masks.
   - glLogicOp is an actual thing that should work for GCs.
   - Pixmaps can be any of the following, depending on GL implementation.
     - This requires offscreen rendering. Fortunately, this is always
       available.
       - OS X: Drawable objects, including: pixel buffers and offscreen
         memory.
         - Offscreen buffers w/ CGLSetOffScreen (10.0+)
         - http://lists.apple.com/archives/mac-opengl/2002/Jun/msg00087.html
           provides a very ugly solution for hardware-accelerated offscreen
           rendering with CGLSetParameter(*, kCGLCPSurfaceTexture, *) on OS X
           10.1+. Apple docs say it's actually for OS X 10.3+, instead.
         - Pixel buffers w/ CGLSetPBuffer (10.3+, now deprecated)
           - Requires APPLE_pixel_buffer.
             - Available in software on x86 only.
             - Always available on hardware.
           - Need to blit? Use OpenGL pixel buffers. (GL_PIXEL_PACK_BUFFER)
         - Framebuffer objects w/ GL_(ARB|EXT)_framebuffer_object
           - TODO: Can EXT_framebuffers be different sizes?
           - Preferred on 10.7+
       - iOS: Use OES_framebuffer_object, it's always present.
 */

/* OpenGL hacks call a number of X11 functions, including
 * XCopyArea, XDrawString, XGetImage
 * XCreatePixmap, XCreateGC, XCreateImage
 * XPutPixel
 * Check these, of course. */

#ifdef JWXYZ_GL /* entire file */

#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#ifdef HAVE_COCOA
# ifdef USE_IPHONE
#  import <QuartzCore/QuartzCore.h>
#  include <OpenGLES/ES1/gl.h>
#  include <OpenGLES/ES1/glext.h>

#  define NSView  UIView
#  define NSRect  CGRect
#  define NSPoint CGPoint
#  define NSSize  CGSize
#  define NSColor UIColor
#  define NSImage UIImage
#  define NSEvent UIEvent
#  define NSFont  UIFont
#  define NSGlyph CGGlyph
#  define NSWindow UIWindow
#  define NSMakeSize   CGSizeMake
#  define NSBezierPath UIBezierPath
#  define colorWithDeviceRed colorWithRed

#  define NSFontTraitMask      UIFontDescriptorSymbolicTraits
// The values for the flags for NSFontTraitMask and
// UIFontDescriptorSymbolicTraits match up, not that it really matters here.
#  define NSBoldFontMask       UIFontDescriptorTraitBold
#  define NSFixedPitchFontMask UIFontDescriptorTraitMonoSpace
#  define NSItalicFontMask     UIFontDescriptorTraitItalic

#  define NSOpenGLContext EAGLContext

# else
#  include <OpenGL/glu.h>
# endif
#else
/* TODO: Does this work on iOS? */
# ifndef HAVE_JWZGLES
#  include <gl/glu.h>
# else
#  include <GLES/gl.h>
#  include <GLES/glext.h>
# endif
#endif

#ifdef HAVE_JWZGLES
# include "jwzglesI.h"
#endif

#ifdef HAVE_ANDROID
# include <android/log.h>
#endif

#include "jwxyzI.h"
#include "jwxyz-timers.h"
#include "yarandom.h"
#include "utf8wc.h"
#include "xft.h"

#if defined HAVE_COCOA
# include <CoreGraphics/CGGeometry.h>
#else


#ifdef HAVE_ANDROID
 extern void Log(const char *fmt, ...);
#else
static void
Log (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
}
#endif

struct CGPoint {
    float x;
    float y;
};
typedef struct CGPoint CGPoint;

struct CGSize {
    float width;
    float height;
};
typedef struct CGSize CGSize;

struct CGRect {
    CGPoint origin;
    CGSize size;
};
typedef struct CGRect CGRect;

#endif

# undef MAX
# undef MIN
# define MAX(a,b) ((a)>(b)?(a):(b))
# define MIN(a,b) ((a)<(b)?(a):(b))

union color_bytes
{
  /* On 64-bit systems, high bits of the 32-bit pixel are available as scratch
     space. I doubt if any screen savers need it, but just in case... */
  unsigned long pixel;
  uint8_t bytes[4];
};

struct jwxyz_Display {
  Window main_window;
  Screen *screen;
  int screen_count;
  struct jwxyz_sources_data *timers_data;

  Bool gl_texture_npot_p;
  /* Bool opengl_core_p */;
  GLenum gl_texture_target;
  
// #if defined USE_IPHONE
  GLuint rect_texture; // Also can work on the desktop.
// #endif

  unsigned long window_background;
};

struct jwxyz_Screen {
  Display *dpy;
  GLenum pixel_format, pixel_type;
  unsigned long black, white;
  Visual *visual;
  int screen_number;
};

struct jwxyz_GC {
  XGCValues gcv;
  unsigned int depth;
  // CGImageRef clip_mask;  // CGImage copy of the Pixmap in gcv.clip_mask
};

struct jwxyz_Font {
  Display *dpy;
  char *ps_name;
  void *native_font;
  int refcount; // for deciding when to release the native font
  float size;   // points
  int ascent, descent;
  char *xa_font;

  // In X11, "Font" is just an ID, and "XFontStruct" contains the metrics.
  // But we need the metrics on both of them, so they go here.
  XFontStruct metrics;
};

struct jwxyz_XFontSet {
  XFontStruct *font;
};


/* XGetImage in CoreGraphics JWXYZ has to deal with funky pixel formats
   necessitating fast & flexible pixel conversion. OpenGL does image format
   conversion itself, so alloc_color and query_color are mercifully simple.
 */
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

  if (dpy->screen->pixel_format == GL_BGRA_EXT) {
    color.pixel = color.bytes[2] |
                  (color.bytes[1] << 8) |
                  (color.bytes[0] << 16) |
                  (color.bytes[3] << 24);
  } else {
    Assert(dpy->screen->pixel_format == GL_RGBA,
           "jwxyz_alloc_color: Unknown pixel_format");
  }

  return (uint32_t)color.pixel;
}

// Converts an array of pixels ('src') from one format to another, placing the
// result in 'dest', according to the pixel conversion mode 'mode'.
void
jwxyz_query_color (Display *dpy, unsigned long pixel, uint8_t *rgba)
{
  union color_bytes color;

  if(dpy->screen->pixel_format == GL_RGBA)
  {
    color.pixel = pixel;
    for (unsigned i = 0; i != 4; ++i)
      rgba[i] = color.bytes[i];
    return;
  }

  Assert (dpy->screen->pixel_format == GL_BGRA_EXT,
          "jwxyz_query_color: Unknown pixel format");
  /* TODO: Cross-check with XAllocColor. */
  rgba[0] = (pixel >> 16) & 0xFF;
  rgba[1] = (pixel >>  8) & 0xFF;
  rgba[2] = (pixel >>  0) & 0xFF;
  rgba[3] = (pixel >> 24) & 0xFF;
}


void
jwxyz_assert_display(Display *dpy)
{
  if(!dpy)
    return;
  jwxyz_assert_gl ();
  jwxyz_assert_drawable (dpy->main_window, dpy->main_window);
}


void
jwxyz_set_matrices (Display *dpy, unsigned width, unsigned height,
                    Bool window_p)
{
  /* TODO: Check registration pattern from Interference with rectangles instead of points. */

  // The projection matrix is always set as follows. The modelview matrix is
  // usually identity, but for points and thin lines, it's translated by 0.5.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  
# if defined(USE_IPHONE) || defined(HAVE_ANDROID)

  if (window_p && ignore_rotation_p(dpy)) {
    int o = (int) current_device_rotation();
    glRotatef (-o, 0, 0, 1);
  }

  // glPointSize(1); // This is the default.
  
#ifdef HAVE_JWZGLES
  glOrthof /* TODO: Is glOrthox worth it? Signs point to no. */
#else
  glOrtho
#endif
    (0, width, height, 0, -1, 1);

  glMatrixMode(GL_MODELVIEW);
# endif // HAVE_MOBILE
}

#ifndef HAVE_JWZGLES

struct gl_version
{
  // iOS always uses OpenGL ES 1.1.
  unsigned major;
  unsigned minor;
};

static GLboolean
gl_check_ver (const struct gl_version *caps,
              unsigned gl_major,
              unsigned gl_minor)
{
  return caps->major > gl_major ||
           (caps->major == gl_major && caps->minor >= gl_minor);
}

/*
static GLboolean gl_check_ext(const struct gl_caps *caps,
                              unsigned gl_major,
                              unsigned gl_minor,
                              const char *extension)
{
  return
    gl_check_ver(caps, gl_major, gl_minor) ||
    gluCheckExtension(extension, caps->extensions);
}
*/

#endif


// NSOpenGLContext *jwxyz_debug_context;

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

# ifndef HAVE_JWZGLES
  struct gl_version version;

  {
    const GLubyte *version_str = glGetString (GL_VERSION);

    /* iPhone is always OpenGL ES 1.1. */
    if (sscanf ((const char *) version_str, "%u.%u",
                &version.major, &version.minor) < 2)
    {
      version.major = 1;
      version.minor = 1;
    }
  }
# endif // !HAVE_JWZGLES

  const GLubyte *extensions = glGetString (GL_EXTENSIONS);

  /* See:
     - Apple TN2080: Understanding and Detecting OpenGL Functionality.
     - OpenGL Programming Guide for the Mac - Best Practices for Working with
       Texture Data - Optimal Data Formats and Types
   */

  // If a video adapter suports BGRA textures, then that's probably as fast as
  // you're gonna get for getting a texture onto the screen.
# ifdef HAVE_JWZGLES
  /* TODO: Make BGRA work on iOS. As it is, it breaks XPutImage. (glTexImage2D, AFAIK) */
  d->screen->pixel_format = GL_RGBA; /*
    gluCheckExtension ((const GLubyte *) "GL_APPLE_texture_format_BGRA8888",
                       extensions) ? GL_BGRA_EXT : GL_RGBA; */
  d->screen->pixel_type = GL_UNSIGNED_BYTE;
  // See also OES_read_format.
# else  // !HAVE_JWZGLES
  if (gl_check_ver (&version, 1, 2) ||
      (gluCheckExtension ((const GLubyte *) "GL_EXT_bgra", extensions) &&
       gluCheckExtension ((const GLubyte *) "GL_APPLE_packed_pixels",
                          extensions))) {
    d->screen->pixel_format = GL_BGRA_EXT;
    // Both Intel and PowerPC-era docs say to use GL_UNSIGNED_INT_8_8_8_8_REV.
    d->screen->pixel_type = GL_UNSIGNED_INT_8_8_8_8_REV;
  } else {
    d->screen->pixel_format = GL_RGBA;
    d->screen->pixel_type = GL_UNSIGNED_BYTE;
  }
  // GL_ABGR_EXT/GL_UNSIGNED_BYTE is another possibilty that may have made more
  // sense on PowerPC.
# endif // !HAVE_JWZGLES

  // On really old systems, it would make sense to split the texture
  // into subsections.
# ifndef HAVE_JWZGLES
  d->gl_texture_npot_p = gluCheckExtension ((const GLubyte *)
                                            "GL_ARB_texture_rectangle",
                                            extensions);
  d->gl_texture_target = d->gl_texture_npot_p ?
                         GL_TEXTURE_RECTANGLE_EXT :
                         GL_TEXTURE_2D;
# else
  d->gl_texture_npot_p = jwzgles_gluCheckExtension
      ((const GLubyte *) "GL_APPLE_texture_2D_limited_npot", extensions) ||
    jwzgles_gluCheckExtension
      ((const GLubyte *) "GL_OES_texture_npot", extensions) ||
    jwzgles_gluCheckExtension // From PixelFlinger 1.4
      ((const GLubyte *) "GL_ARB_texture_non_power_of_two", extensions);

  d->gl_texture_target = GL_TEXTURE_2D;
# endif

  d->screen->black = jwxyz_alloc_color (d, 0x0000, 0x0000, 0x0000, 0xFFFF);
  d->screen->white = jwxyz_alloc_color (d, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF);

  Visual *v = (Visual *) calloc (1, sizeof(Visual));
  v->class      = TrueColor;
  v->red_mask   = jwxyz_alloc_color (d, 0xFFFF, 0x0000, 0x0000, 0x0000);
  v->green_mask = jwxyz_alloc_color (d, 0x0000, 0xFFFF, 0x0000, 0x0000);
  v->blue_mask  = jwxyz_alloc_color (d, 0x0000, 0x0000, 0xFFFF, 0x0000);
  v->bits_per_rgb = 8;
  d->screen->visual = v;

  d->timers_data = jwxyz_sources_init (XtDisplayToApplicationContext (d));

  d->window_background = BlackPixel(d,0);

  d->main_window = w;
  {
    fputs((char *)glGetString(GL_VENDOR), stderr);
    fputc(' ', stderr);
    fputs((char *)glGetString(GL_RENDERER), stderr);
    fputc(' ', stderr);
    fputs((char *)glGetString(GL_VERSION), stderr);
    fputc('\n', stderr);
//  puts(caps.extensions);
    GLint max_texture_size;
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
    printf ("GL_MAX_TEXTURE_SIZE: %d\n", max_texture_size);
  }
 
  // In case a GL hack wants to use X11 to draw offscreen, the rect_texture is available.
  Assert (d->main_window == w, "Uh-oh.");
  glGenTextures (1, &d->rect_texture);
  // TODO: Check for (ARB|EXT|NV)_texture_rectangle. (All three are alike.)
  // Rectangle textures should be present on OS X with the following exceptions:
  // - Generic renderer on PowerPC OS X 10.4 and earlier
  // - ATI Rage 128
  glBindTexture (d->gl_texture_target, d->rect_texture);
  // TODO: This is all somewhere else. Refactor.
  glTexParameteri (d->gl_texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (d->gl_texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // This might be redundant for rectangular textures.
# ifndef HAVE_JWZGLES
  const GLint wrap = GL_CLAMP;
# else  // HAVE_JWZGLES
  const GLint wrap = GL_CLAMP_TO_EDGE;
# endif // HAVE_JWZGLES

  // In OpenGL, CLAMP_TO_EDGE is OpenGL 1.2 or GL_SGIS_texture_edge_clamp.
  // This is always present with OpenGL ES.
  glTexParameteri (d->gl_texture_target, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri (d->gl_texture_target, GL_TEXTURE_WRAP_T, wrap);

  jwxyz_assert_display(d);
  return d;
}

void
jwxyz_free_display (Display *dpy)
{
  /* TODO: Go over everything. */

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
static void
invalidate_drawable_cache (Drawable d)
{
  /* TODO: Kill this outright. jwxyz_bind_drawable handles any potential 
     invalidation.
   */

  /*
  if (d && d->cgi) {
    abort();
    CGImageRelease (d->cgi);
    d->cgi = 0;
  }
 */
}


/* Call this when the View changes size or position.
 */
void
jwxyz_window_resized (Display *dpy)
{
  const XRectangle *new_frame = jwxyz_frame (dpy->main_window);
  unsigned new_width = new_frame->width;
  unsigned new_height = new_frame->height;

  Assert (new_width, "jwxyz_window_resized: No width.");
  Assert (new_height, "jwxyz_window_resized: No height.");

/*if (cgc) w->cgc = cgc;
  Assert (w->cgc, "no CGContext"); */

  Log("resize: %d, %d\n", new_width, new_height);

  jwxyz_bind_drawable (dpy, dpy->main_window, dpy->main_window);

  // TODO: What does the iPhone need?
  
  // iOS only: If the main_window is not the current_drawable, then set_matrices
  // was already called in bind_drawable.
  jwxyz_set_matrices (dpy, new_width, new_height, True);

/*
  GLint draw_buffer;
  glGetIntegerv (GL_DRAW_BUFFER, &draw_buffer);
  
  glDrawBuffer (GL_FRONT);
  glClearColor (1, 0, 1, 0);
  glClear (GL_COLOR_BUFFER_BIT);
  glDrawBuffer (GL_BACK);
  glClearColor (0, 1, 1, 0);
  glClear (GL_COLOR_BUFFER_BIT);
  
  glDrawBuffer (draw_buffer); */
  
  // Stylish and attractive purple!
  // glClearColor (1, 0, 1, 0.5);
  // glClear (GL_COLOR_BUFFER_BIT);
  
  invalidate_drawable_cache (dpy->main_window);
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


/* GC attributes by usage and OpenGL implementation:
 *
 * All drawing functions:
 * function                                | glLogicOp w/ GL_COLOR_LOGIC_OP
 * clip_x_origin, clip_y_origin, clip_mask | Stencil mask
 *
 * Shape drawing functions:
 * foreground, background                  | glColor*
 *
 * XDrawLines, XDrawRectangles, XDrawSegments:
 * line_width, cap_style, join_style       | Lotsa vertices
 *
 * XFillPolygon:
 * fill_rule                               | Multiple GL_TRIANGLE_FANs
 *
 * XDrawText:
 * font                                    | Cocoa, then OpenGL display lists.
 *
 * alpha_allowed_p                         | TODO
 * antialias_p                             | TODO
 *
 * Nothing, really:
 * subwindow_mode
*/

static void
set_clip_mask (GC gc)
{
  Assert (!gc->gcv.clip_mask, "set_gc: TODO");
}

static void
set_function (int function)
{
  Assert (function == GXcopy, "set_gc: (TODO) Stubbed gcv function");
  
  /* TODO: The GL_COLOR_LOGIC_OP extension is exactly what is needed here. (OpenGL 1.1)
   Fun fact: The glLogicOp opcode constants are the same as the X11 GX* function constants | GL_CLEAR.
   */
  
#if 0
  switch (gc->gcv.function) {
    case GXset:
    case GXclear:
    case GXcopy:/*CGContextSetBlendMode (cgc, kCGBlendModeNormal);*/   break;
    case GXxor:   CGContextSetBlendMode (cgc, kCGBlendModeDifference); break;
    case GXor:    CGContextSetBlendMode (cgc, kCGBlendModeLighten);    break;
    case GXand:   CGContextSetBlendMode (cgc, kCGBlendModeDarken);     break;
    default: Assert(0, "unknown gcv function"); break;
  }
#endif
}


static void
set_color (Display *dpy, unsigned long pixel, unsigned int depth,
           Bool alpha_allowed_p)
{
  jwxyz_validate_pixel (dpy, pixel, depth, alpha_allowed_p);

  if (depth == 1) {
    GLfloat f = pixel;
    glColor4f (f, f, f, 1);
  } else {
    /* TODO: alpha_allowed_p */
    uint8_t rgba[4];
    jwxyz_query_color (dpy, pixel, rgba);
#ifdef HAVE_JWZGLES
    glColor4f (rgba[0] / 255.0f, rgba[1] / 255.0f,
               rgba[2] / 255.0f, rgba[3] / 255.0f);
#else
    glColor4ubv (rgba);
#endif
  }
}

/* Pushes a GC context; sets Function, ClipMask, and color.
 */
static void
set_color_gc (Display *dpy, GC gc, unsigned long color)
{
  // GC is NULL for XClearArea and XClearWindow.
  unsigned int depth;
  int function;
  if (gc) {
    function = gc->gcv.function;
    depth = gc->depth;
    set_clip_mask (gc);
  } else {
    function = GXcopy;
    depth = visual_depth (NULL, NULL);
    // TODO: Set null clip mask here.
  }

  set_function (function);

  switch (function) {
    case GXset:   color = (depth == 1 ? 1 : WhitePixel(dpy,0)); break;
    case GXclear: color = (depth == 1 ? 0 : BlackPixel(dpy,0)); break;
  }

  set_color (dpy, color, depth, gc ? gc->gcv.alpha_allowed_p : False);
  
  /* TODO: Antialiasing. */
  /* CGContextSetShouldAntialias (cgc, antialias_p); */
}

/* Pushes a GC context; sets color to the foreground color.
 */
static void
set_fg_gc (Display *dpy, GC gc)
{
  set_color_gc (dpy, gc, gc->gcv.foreground);
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

int
XDrawPoints (Display *dpy, Drawable d, GC gc, 
             XPoint *points, int count, int mode)
{
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  set_fg_gc (dpy, gc);

/*
  
  glBegin(GL_POINTS);
  for (unsigned i = 0; i < count; i++) {
    next_point(v, points[i], mode);
    glVertex2f(v[0] + 0.5f, v[1] + 0.5f);
  }
  glEnd();
 */

  short v[2] = {0, 0};

  // TODO: XPoints can be fed directly to OpenGL.
  GLshort *gl_points = malloc (count * 2 * sizeof(GLshort)); // TODO: malloc returns NULL.
  for (unsigned i = 0; i < count; i++) {
    next_point (v, points[i], mode);
    gl_points[2 * i] = v[0];
    gl_points[2 * i + 1] = v[1];
  }
  
  glMatrixMode (GL_MODELVIEW);
  glTranslatef (0.5, 0.5, 0);
  
  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  glVertexPointer (2, GL_SHORT, 0, gl_points);
  glDrawArrays (GL_POINTS, 0, count);
  
  free (gl_points);
  
  glLoadIdentity ();
  
  return 0;
}


static GLint
texture_internalformat(Display *dpy)
{
#ifdef HAVE_JWZGLES
  return dpy->screen->pixel_format;
#else
  return GL_RGBA;
#endif
}

static GLenum gl_pixel_type(const Display *dpy)
{
  return dpy->screen->pixel_type;
}

static void
clear_texture (Display *dpy)
{
  glTexImage2D (dpy->gl_texture_target, 0, texture_internalformat(dpy), 0, 0,
                0, dpy->screen->pixel_format, gl_pixel_type (dpy), NULL);
}

static void set_white (void)
{
#ifdef HAVE_JWZGLES
  glColor4f (1, 1, 1, 1);
#else
  glColor3ub (0xff, 0xff, 0xff);
#endif
}


static GLsizei to_pow2 (size_t x);


void
jwxyz_gl_copy_area_copy_tex_image (Display *dpy, Drawable src, Drawable dst,
                                   GC gc, int src_x, int src_y,
                                   unsigned int width, unsigned int height,
                                   int dst_x, int dst_y)
{
  const XRectangle *src_frame = jwxyz_frame (src);

  Assert(gc->gcv.function == GXcopy, "XCopyArea: Unknown function");

  jwxyz_bind_drawable (dpy, dpy->main_window, src);

#  if defined HAVE_COCOA && !defined USE_IPHONE
  /* TODO: Does this help? */
  /* glFinish(); */
#  endif

  unsigned tex_w = width, tex_h = height;
  if (!dpy->gl_texture_npot_p) {
    tex_w = to_pow2(tex_w);
    tex_h = to_pow2(tex_h);
  }

  GLint internalformat = texture_internalformat(dpy);

  glBindTexture (dpy->gl_texture_target, dpy->rect_texture);

  if (tex_w == width && tex_h == height) {
    glCopyTexImage2D (dpy->gl_texture_target, 0, internalformat,
                      src_x, src_frame->height - src_y - height,
                      width, height, 0);
  } else {
    glTexImage2D (dpy->gl_texture_target, 0, internalformat, tex_w, tex_h,
                  0, dpy->screen->pixel_format, gl_pixel_type(dpy), NULL);
    glCopyTexSubImage2D (dpy->gl_texture_target, 0, 0, 0,
                         src_x, src_frame->height - src_y - height,
                         width, height);
  }

  jwxyz_bind_drawable (dpy, dpy->main_window, dst);
  set_white ();
  glBindTexture (dpy->gl_texture_target, dpy->rect_texture);
  glEnable (dpy->gl_texture_target);

  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
    
  /* TODO: Copied from XPutImage. Refactor. */
  /* TODO: EXT_draw_texture or whatever it's called. */
  GLfloat vertices[4][2] =
  {
    {dst_x, dst_y},
    {dst_x, dst_y + height},
    {dst_x + width, dst_y + height},
    {dst_x + width, dst_y}
  };
  
#ifdef HAVE_JWZGLES
  static const GLshort tex_coords[4][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
#else
  GLshort tex_coords[4][2] = {{0, height}, {0, 0}, {width, 0}, {width, height}};
#endif

  glVertexPointer (2, GL_FLOAT, 0, vertices);
  glTexCoordPointer (2, GL_SHORT, 0, tex_coords);
  glDrawArrays (GL_TRIANGLE_FAN, 0, 4);
  
  clear_texture (dpy);
  
  glDisable (dpy->gl_texture_target);
}

void
jwxyz_gl_copy_area_read_pixels (Display *dpy, Drawable src, Drawable dst,
                                GC gc, int src_x, int src_y,
                                unsigned int width, unsigned int height,
                                int dst_x, int dst_y)
{
# if 1
  XImage *img = XGetImage (dpy, src, src_x, src_y, width, height, ~0, ZPixmap);
  XPutImage (dpy, dst, gc, img, 0, 0, dst_x, dst_y, width, height);
  XDestroyImage (img);
# endif

# if 0
  /* Something may or may not be broken in here. (shrug) */
  bind_drawable(dpy, src);

  /* Error checking would be nice. */
  void *pixels = malloc (src_rect.size.width * 4 * src_rect.size.height);

  glPixelStorei (GL_PACK_ROW_LENGTH, 0);
  glPixelStorei (GL_PACK_ALIGNMENT, 4);

  glReadPixels (src_rect.origin.x, dst_frame.size.height - (src_rect.origin.y + src_rect.size.height),
                src_rect.size.width, src_rect.size.height,
                GL_RGBA, GL_UNSIGNED_BYTE, // TODO: Pick better formats.
                pixels);

  bind_drawable (dpy, dst);

  glPixelZoom (1.0f, 1.0f);

  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

  glRasterPos2i (dst_rect.origin.x, dst_rect.origin.y + dst_rect.size.height);
  glDrawPixels (dst_rect.size.width, dst_rect.size.height,
                GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  free(pixels);
# endif
}


#if 0
// TODO: Make sure offset works in super-sampled mode.
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
#endif


int
XDrawLine (Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2)
{
  // TODO: XDrawLine == XDrawSegments(nlines == 1), also in jwxyz.m
  XSegment segment;
  segment.x1 = x1;
  segment.y1 = y1;
  segment.x2 = x2;
  segment.y2 = y2;
  XDrawSegments (dpy, d, gc, &segment, 1);

  // when drawing a zero-length line, obey line-width and cap-style.
/* if (x1 == x2 && y1 == y2) {
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
  invalidate_drawable_cache (d); */
  return 0;
}

int
XDrawLines (Display *dpy, Drawable d, GC gc, XPoint *points, int count,
            int mode)
{
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  set_fg_gc (dpy, gc);

  /* TODO: Thick lines
   * Zero-length line segments
   * Paths with zero length total (Remember line width, cap style.)
   * Closed loops
   */
  
  if (!count)
    return 0;

  GLshort *vertices = malloc(2 * sizeof(GLshort) * count); // TODO malloc NULL sigh
  
  glMatrixMode (GL_MODELVIEW);
  glTranslatef (0.5f, 0.5f, 0);
  
  short p[2] = {0, 0};
  for (unsigned i = 0; i < count; i++) {
    next_point (p, points[i], mode);
    vertices[2 * i] = p[0];
    vertices[2 * i + 1] = p[1];
  }
  
  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  glVertexPointer (2, GL_SHORT, 0, vertices);
  glDrawArrays (GL_LINE_STRIP, 0, count);
  
  free (vertices);

  if (gc->gcv.cap_style != CapNotLast) {
    // TODO: How does this look with multisampling?
    // TODO: Disable me for closed loops.
    glVertexPointer (2, GL_SHORT, 0, p);
    glDrawArrays (GL_POINTS, 0, 1);
  }

  glLoadIdentity ();
  
  return 0;
}


int
XDrawSegments (Display *dpy, Drawable d, GC gc, XSegment *segments, int count)
{
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  set_fg_gc (dpy, gc);
  
  /* TODO: Thick lines. */
  /* Thin lines <= 1px are offset by +0.5; thick lines are not. */
  
  glMatrixMode (GL_MODELVIEW);
  glTranslatef (0.5, 0.5, 0);

  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  
  Assert (sizeof(XSegment) == sizeof(short) * 4, "XDrawSegments: Data alignment mix-up."); // TODO: Static assert here.
  Assert (sizeof(GLshort) == sizeof(short), "XDrawSegments: Data alignment mix-up."); // TODO: Static assert here.
  Assert (offsetof(XSegment, x1) == 0, "XDrawSegments: Data alignment mix-up.");
  Assert (offsetof(XSegment, x2) == 4, "XDrawSegments: Data alignment mix-up.");
  glVertexPointer (2, GL_SHORT, 0, segments);
  glDrawArrays (GL_LINES, 0, count * 2);
  
  if (gc->gcv.cap_style != CapNotLast) {
    glVertexPointer (2, GL_SHORT, sizeof(GLshort) * 4, (const GLshort *)segments + 2);
    glDrawArrays (GL_POINTS, 0, count);
  }
  
  glLoadIdentity ();
  
/* CGRect wr = d->frame;
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
  invalidate_drawable_cache (d); */
  return 0;
}


int
XClearWindow (Display *dpy, Window win)
{
  Assert (win == dpy->main_window, "not a window");
  const XRectangle *wr = jwxyz_frame (win);
  /* TODO: Use glClear if there's no background pixmap. */
  return XClearArea (dpy, win, 0, 0, wr->width, wr->height, 0);
}

unsigned long
jwxyz_window_background (Display *dpy)
{
  return dpy->window_background;
}

int
XSetWindowBackground (Display *dpy, Window w, unsigned long pixel)
{
  Assert (w == dpy->main_window, "not a window");
  jwxyz_validate_pixel (dpy, pixel, visual_depth (NULL, NULL), False);
  dpy->window_background = pixel;
  return 0;
}

void
jwxyz_fill_rects (Display *dpy, Drawable d, GC gc,
                  const XRectangle *rectangles, unsigned long nrectangles,
                  unsigned long pixel)
{
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  set_color_gc (dpy, gc, pixel);
/*
  glBegin(GL_QUADS);
  for (unsigned i = 0; i != nrectangles; ++i) {
    const XRectangle *r = &rectangles[i];
    glVertex2i(r->x, r->y);
    glVertex2i(r->x, r->y + r->height);
    glVertex2i(r->x + r->width, r->y + r->height);
    glVertex2i(r->x + r->width, r->y);
  }
  glEnd(); */
  
  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    
  for (unsigned long i = 0; i != nrectangles; ++i)
  {
    const XRectangle *r = &rectangles[i];
    
    GLfloat coords[4][2] =
    {
      {r->x, r->y},
      {r->x, r->y + r->height},
      {r->x + r->width, r->y + r->height},
      {r->x + r->width, r->y}
    };
    
    // TODO: Several rects at once. Maybe even tune for XScreenSaver workloads.
    glVertexPointer (2, GL_FLOAT, 0, coords);
    jwxyz_assert_gl ();
    glDrawArrays (GL_TRIANGLE_FAN, 0, 4);
    jwxyz_assert_gl ();
  }
}


int
XClearArea (Display *dpy, Window win, int x, int y, int w, int h, Bool exp)
{
  Assert(win == dpy->main_window, "XClearArea: not a window");
  Assert(!exp, "XClearArea: exposures unsupported");

  jwxyz_fill_rect (dpy, win, 0, x, y, w, h, dpy->window_background);
  return 0;
}


int
XFillPolygon (Display *dpy, Drawable d, GC gc, 
              XPoint *points, int npoints, int shape, int mode)
{
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  set_fg_gc(dpy, gc);
  
  // TODO: Re-implement the GLU tesselation functions.

  /* Complex: Pedal, and for some reason Attraction, Mountain, Qix, SpeedMine, Starfish
   * Nonconvex: Goop, Pacman, Rocks, Speedmine
   */
  
  Assert(shape == Convex, "XFillPolygon: (TODO) Unimplemented shape");
  
  // TODO: Feed vertices straight to OpenGL for CoordModeOrigin.
  GLshort *vertices = malloc(npoints * sizeof(GLshort) * 2); // TODO: Oh look, another unchecked malloc.
  short v[2] = {0, 0};
  
  for (unsigned i = 0; i < npoints; i++) {
    next_point(v, points[i], mode);
    vertices[2 * i] = v[0];
    vertices[2 * i + 1] = v[1];
  }

  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  
  glVertexPointer (2, GL_SHORT, 0, vertices);
  glDrawArrays (GL_TRIANGLE_FAN, 0, npoints);
  
  free(vertices);

  /*
  CGRect wr = d->frame;
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
  */
  return 0;
}

#define radians(DEG) ((DEG) * M_PI / 180.0)
#define degrees(RAD) ((RAD) * 180.0 / M_PI)

static void
arc_xy(GLfloat *p, double cx, double cy, double w2, double h2, double theta)
{
  p[0] = cos(theta) * w2 + cx;
  p[1] = -sin(theta) * h2 + cy;
}

static unsigned
mod_neg(int a, unsigned b)
{
  /* Normal modulus is implementation defined for negative numbers. This is 
   * well-defined such that the repeating pattern for a >= 0 is maintained for 
   * a < 0. */
  return a < 0 ? (b - 1) - (-(a + 1) % b) : a % b;
}

int
jwxyz_draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
                unsigned int width, unsigned int height,
                int angle1, int angle2, Bool fill_p)
{
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  set_fg_gc(dpy, gc);

  /* Let's say the number of line segments needed to make a convincing circle is
     4*sqrt(radius). (But these arcs aren't necessarily circular arcs...) */

  double w2 = width * 0.5f, h2 = height * 0.5f;
  double a, b; /* Semi-major/minor axes. */
  if(w2 > h2) {
    a = w2;
    b = h2;
  } else {
    a = h2;
    b = w2;
  }
  
  const double two_pi = 2 * M_PI;

  double amb = a - b, apb = a + b;
  double h = (amb * amb) / (apb * apb);
  // TODO: Math cleanup.
  double C_approx = M_PI * apb * (1 + 3 * h / (10 + sqrtf(4 - 3 * h)));
  double segments_f = 4 * sqrtf(C_approx / (2 * M_PI));

  // TODO: Explain how drawing works what with the points of overlapping arcs
  // matching up.
 
#if 1
  unsigned segments_360 = segments_f;
  
  /* TODO: angle2 == 0. This is a tilted square with CapSquare. */
  /* TODO: color, thick lines, CapNotLast for thin lines */
  /* TODO: Transformations. */

  double segment_angle = two_pi / segments_360;

  const unsigned deg64 = 360 * 64;
  const double rad_from_deg64 = two_pi / deg64;
  
  if (angle2 < 0) {
    angle1 += angle2;
    angle2 = -angle2;
  }

  angle1 = mod_neg(angle1, deg64); // TODO: Is this OK? Consider negative numbers.

  if (angle2 > deg64)
    angle2 = deg64; // TODO: Handle circles special.
  
  double
    angle1_f = angle1 * rad_from_deg64,
    angle2_f = angle2 * rad_from_deg64;
  
  if (angle2_f > two_pi) // TODO: Move this up.
    angle2_f = two_pi;
  
  double segment1_angle_part = fmodf(angle1_f, segment_angle);
  
  unsigned segment1 = ((angle1_f - segment1_angle_part) / segment_angle) + 1.5;

  double angle_2r = angle2_f - segment1_angle_part;
  unsigned segments = angle_2r / segment_angle;
  
  GLfloat cx = x + w2, cy = y + h2;

  GLfloat *data = malloc((segments + 3) * sizeof(GLfloat) * 2); // TODO: Check result.
  
  GLfloat *data_ptr = data;
  if (fill_p) {
    data_ptr[0] = cx;
    data_ptr[1] = cy;
    data_ptr += 2;
  }
  
  arc_xy (data_ptr, cx, cy, w2, h2, angle1_f);
  data_ptr += 2;
  
  for (unsigned s = 0; s != segments; ++s) {
    // TODO: Make sure values of theta for the following arc_xy call are between
    // angle1_f and angle1_f + angle2_f.
    arc_xy (data_ptr, cx, cy, w2, h2, (segment1 + s) * segment_angle);
    data_ptr += 2;
  }
  
  arc_xy (data_ptr, cx, cy, w2, h2, angle1_f + angle2_f);
  data_ptr += 2;

  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
  
  glVertexPointer (2, GL_FLOAT, 0, data);
  glDrawArrays (fill_p ? GL_TRIANGLE_FAN : GL_LINE_STRIP,
                0,
                (GLsizei)((data_ptr - data) / 2));

  free(data);
  
#endif
  
#if 0
  unsigned segments = segments_f * (fabs(angle2) / (360 * 64));
 
  glBegin (fill_p ? GL_TRIANGLE_FAN : GL_LINE_STRIP);
  
  if (fill_p /* && gc->gcv.arc_mode == ArcPieSlice */)
    glVertex2f (cx, cy);
  
  /* TODO: This should fix the middle points of the arc so that the starting and ending points are OK. */
  
  float to_radians = 2 * M_PI / (360 * 64);
  float theta = angle1 * to_radians, d_theta = angle2 * to_radians / segments;
  
  for (unsigned s = 0; s != segments + 1; ++s) /* TODO: This is the right number of segments, yes? */
  {
    glVertex2f(cos(theta) * w2 + cx, -sin(theta) * h2 + cy);
    theta += d_theta;
  }
  
  glEnd ();
#endif
  
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

  // TODO
/*
  Assert (!!gc->gcv.clip_mask == !!gc->clip_mask, "GC clip mask mixup");

  if (gc->gcv.clip_mask) {
    XFreePixmap (dpy, gc->gcv.clip_mask);
    CGImageRelease (gc->clip_mask);
  }
*/
  free (gc);
  return 0;
}


/*
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
*/

// Copied and pasted from OSX/XScreenSaverView.m
static GLsizei
to_pow2 (size_t x)
{
  if (x <= 1)
    return 1;

  size_t mask = (size_t)-1;
  unsigned bits = sizeof(x) * CHAR_BIT;
  unsigned log2 = bits;

  --x;
  while (bits) {
    if (!(x & mask)) {
      log2 -= bits;
      x <<= bits;
    }

    bits >>= 1;
    mask <<= bits;
  }

  return 1 << log2;
}


int
XPutImage (Display *dpy, Drawable d, GC gc, XImage *ximage,
           int src_x, int src_y, int dest_x, int dest_y,
           unsigned int w, unsigned int h)
{
  jwxyz_assert_display (dpy);
 
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

  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  int bpl = ximage->bytes_per_line;
  int bpp = ximage->bits_per_pixel;
  /* int bsize = bpl * h; */
  char *data = ximage->data;

/*
  CGRect r;
  r.origin.x = wr->x + dest_x;
  r.origin.y = wr->y + wr->height - dest_y - h;
  r.size.width = w;
  r.size.height = h;
*/

  Assert (gc->gcv.function == GXcopy, "XPutImage: (TODO) GC function not supported");
  Assert (!gc->gcv.clip_mask, "XPutImage: (TODO) GC clip mask not supported");
  
  if (bpp == 32) {

    /* Take advantage of the fact that it's ok for (bpl != w * bpp)
       to create a CGImage from a sub-rectagle of the XImage.
     */
    data += (src_y * bpl) + (src_x * 4);

    jwxyz_assert_display(dpy);
    
    /* There probably won't be any hacks that do this, but... */
    Assert (!(bpl % 4), "XPutImage: bytes_per_line not divisible by four.");
    
    unsigned src_w = bpl / 4;

    /* GL_UNPACK_ROW_LENGTH is not allowed to be negative. (sigh) */
# ifndef HAVE_JWZGLES
    glPixelStorei (GL_UNPACK_ROW_LENGTH, src_w);
    src_w = w;
# endif

    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

# if 1 // defined HAVE_JWZGLES
    // Regular OpenGL uses GL_TEXTURE_RECTANGLE_EXT in place of GL_TEXTURE_2D.
    // TODO: Make use of OES_draw_texture.
    // TODO: Coords might be wrong; things might be upside-down or backwards
    //       or whatever.

    unsigned tex_w = src_w, tex_h = h;
    if (!dpy->gl_texture_npot_p) {
      tex_w = to_pow2(tex_w);
      tex_h = to_pow2(tex_h);
    }

    GLint internalformat = texture_internalformat(dpy);

    glBindTexture (dpy->gl_texture_target, dpy->rect_texture);

    if (tex_w == src_w && tex_h == h) {
      glTexImage2D (dpy->gl_texture_target, 0, internalformat, tex_w, tex_h,
                    0, dpy->screen->pixel_format, gl_pixel_type(dpy), data);
    } else {
      // TODO: Sampling the last row might be a problem if src_x != 0.
      glTexImage2D (dpy->gl_texture_target, 0, internalformat, tex_w, tex_h,
                    0, dpy->screen->pixel_format, gl_pixel_type(dpy), NULL);
      glTexSubImage2D (dpy->gl_texture_target, 0, 0, 0, src_w, h,
                       dpy->screen->pixel_format, gl_pixel_type(dpy), data);
    }
    
    set_white ();
    // glEnable (dpy->gl_texture_target);
    // glColor4f (0.5, 0, 1, 1);
    glEnable (dpy->gl_texture_target);
    glEnableClientState (GL_VERTEX_ARRAY);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    // TODO: Why are these ever turned on in the first place?
    glDisableClientState (GL_COLOR_ARRAY);
    glDisableClientState (GL_NORMAL_ARRAY);
    // glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    GLfloat vertices[4][2] =
    {
      {dest_x, dest_y},
      {dest_x, dest_y + h},
      {dest_x + w, dest_y + h},
      {dest_x + w, dest_y}
    };

    GLfloat texcoord_w, texcoord_h;
#  ifndef HAVE_JWZGLES
    if (dpy->gl_texture_target == GL_TEXTURE_RECTANGLE_EXT) {
      texcoord_w = w;
      texcoord_h = h;
    } else
#  endif /* HAVE_JWZGLES */
    {
      texcoord_w = (double)w / tex_w;
      texcoord_h = (double)h / tex_h;
    }

    GLfloat tex_coords[4][2];
    tex_coords[0][0] = 0;
    tex_coords[0][1] = 0;
    tex_coords[1][0] = 0;
    tex_coords[1][1] = texcoord_h;
    tex_coords[2][0] = texcoord_w;
    tex_coords[2][1] = texcoord_h;
    tex_coords[3][0] = texcoord_w;
    tex_coords[3][1] = 0;

    glVertexPointer (2, GL_FLOAT, 0, vertices);
    glTexCoordPointer (2, GL_FLOAT, 0, tex_coords);

    // Respect the alpha channel in the XImage
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

    glDisable (GL_BLEND);

//  clear_texture();
    glDisable (dpy->gl_texture_target);
# else
    glRasterPos2i (dest_x, dest_y);
    glPixelZoom (1, -1);
    jwxyz_assert_display (dpy);
    glDrawPixels (w, h, dpy->screen->pixel_format, gl_pixel_type(dpy), data);
# endif
  } else {   // (bpp == 1)

    // Assert(FALSE, "XPutImage: TODO");
    // Check out ximage_(get|put)pixel_1
    
#if 0
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
                                         GL_FALSE); /* interpolate */
    push_fg_gc (dpy, d, gc, GL_TRUE);

    CGContextFillRect (cgc, r);				// foreground color
    CGContextClipToMask (cgc, r, mask);
    set_color (dpy, cgc, gc->gcv.background, gc->depth, GL_FALSE, GL_TRUE);
    CGContextFillRect (cgc, r);				// background color
    pop_gc (d, gc);

    free (flipped);
    CGDataProviderRelease (prov);
    CGImageRelease (mask);
#endif
  }
 
  jwxyz_assert_gl ();
  invalidate_drawable_cache (d);

  return 0;
}

/* At the moment nothing actually uses XGetSubImage. */
/* #### Actually lots of things call XGetImage, which calls XGetSubImage.
   E.g., Twang calls XGetImage on the window intending to get a
   buffer full of black.  This is returning a buffer full of white
   instead of black for some reason. */
static XImage *
XGetSubImage (Display *dpy, Drawable d, int x, int y,
              unsigned int width, unsigned int height,
              unsigned long plane_mask, int format,
              XImage *dest_image, int dest_x, int dest_y)
{
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  
  // TODO: What if this reads off the edge? What is supposed to happen?

  {
    // In case the caller tries to write off the edge.
    int
      max_width = dest_image->width - dest_x,
      max_height = dest_image->height - dest_y;

    if (width > max_width) {
      width = max_width;
    }
    
    if (height > max_height) {
      height = max_height;
    }
  }
  
  Assert (jwxyz_drawable_depth (d) == dest_image->depth, "XGetSubImage: depth mismatch");
  
  if (dest_image->depth == visual_depth (NULL, NULL)) {
    Assert (!(dest_image->bytes_per_line % 4), "XGetSubImage: bytes_per_line not divisible by 4");
    unsigned pixels_per_line = dest_image->bytes_per_line / 4;
#ifdef HAVE_JWZGLES
    Assert (pixels_per_line == width, "XGetSubImage: (TODO) pixels_per_line != width");
#else
    glPixelStorei (GL_PACK_ROW_LENGTH, pixels_per_line);
#endif
    glPixelStorei (GL_PACK_ALIGNMENT, 4);
    
    uint32_t *dest_data = (uint32_t *)dest_image->data + pixels_per_line * dest_y + dest_x;
    
    glReadPixels (x, jwxyz_frame (d)->height - (y + height), width, height,
                  dpy->screen->pixel_format, gl_pixel_type(dpy), dest_data);

    /* Flip this upside down. :( */
    uint32_t *top = dest_data;
    uint32_t *bottom = dest_data + pixels_per_line * (height - 1);
    for (unsigned y = height / 2; y; --y) {
      for (unsigned x = 0; x != width; ++x) {
        uint32_t px = top[x];
        top[x] = bottom[x];
        bottom[x] = px;
      }
      top += pixels_per_line;
      bottom -= pixels_per_line;
    }
  } else {

    /* TODO: Actually get pixels. */

    Assert (!(dest_x % 8), "XGetSubImage: dest_x not byte-aligned");
    uint8_t *dest_data =
      (uint8_t *)dest_image->data + dest_image->bytes_per_line * dest_y
      + dest_x / 8;
    for (unsigned y = height / 2; y; --y) {
      memset (dest_data, y & 1 ? 0x55 : 0xAA, width / 8);
      dest_data += dest_image->bytes_per_line;
    }
  }

  return dest_image;
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

/* Returns a transformation matrix to do rotation as per the provided
   EXIF "Orientation" value.
 */
/*
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
*/

void
jwxyz_draw_NSImage_or_CGImage (Display *dpy, Drawable d, 
                                Bool nsimg_p, void *img_arg,
                               XRectangle *geom_ret, int exif_rotation)
{
  Assert (False, "jwxyz_draw_NSImage_or_CGImage: TODO stub");
#if 0
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
    jwxyz_fill_rect (dpy, d, 0, 0, 0, winr.size.width, winr.size.height,
                     drawable_depth (d) == 1 ? 0 : BlackPixel(dpy,0));
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
#endif
}

#ifndef HAVE_JWZGLES

/*
static void
create_rectangle_texture (GLuint *texture)
{
  glGenTextures(1, texture);
  glBindTexture(GL_TEXTURE_RECTANGLE_EXT, *texture);
  glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
*/

#endif


#if 0
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
#endif


// This is XQueryFont, but for the XFontStruct embedded in 'Font'
//
static void
query_font (Font fid)
{
  if (!fid || !fid->native_font) {
    Assert (0, "no native font in fid");
    return;
  }

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
    jwxyz_render_text (dpy, native_font, &s, 1, GL_FALSE, cs, 0);

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
    Log (" %3d %c: w=%3d lb=%3d rb=%3d as=%3d ds=%3d\n",
         i, i, cs->width, cs->lbearing, cs->rbearing, 
         cs->ascent, cs->descent);
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
  f->properties[0].card32 = (unsigned long)(char *)fid->xa_font;

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


#if 0


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

  for (NSString *fn in family_members) {
# define MATCH(X) \
         ([fn rangeOfString:X options:NSCaseInsensitiveSearch].location \
         != NSNotFound)

    // The magic invocation for getting font names is
    // [[UIFontDescriptor
    //   fontDescriptorWithFontAttributes:@{UIFontDescriptorNameAttribute: name}]
    //  symbolicTraits]
    // ...but this only works on iOS 7 and later.
    NSFontTraitMask font_mask = 0;
    if (MATCH(@"Bold"))
      font_mask |= NSBoldFontMask;
    if (MATCH(@"Italic") || MATCH(@"Oblique"))
      font_mask |= NSItalicFontMask;

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
  char *name2;

  while ((name2 = strtok (token, ","))) {
    token = 0;

    while (*name2 == ' ' || *name2 == '\t' || *name2 == '\n')
      name2++;

    spc = strrchr (name2, ' ');
    if (!spc) continue;

    int dsize = 0;
    if (1 != sscanf (spc, " %d ", &dsize))
      continue;
    float size = dsize;

    if (size <= 4) continue;

    size *= scale;

    name2[strlen(name2) - strlen(spc)] = 0;

    NSString *nsname = [NSString stringWithCString:name2
                                          encoding:NSUTF8StringEncoding];
    f = [NSFont fontWithName:nsname size:size];
    if (f) {
      *name_ret = name2;
      *size_ret = size;
      *xa_font = strdup (name); // Maybe this should be an XLFD?
      break;
    } else {
      NSLog(@"No native font: \"%@\" %.0f", nsname, size);
    }
  }

  free (token);
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

    NSString *prev_family = nil;
    for (NSString *family in sorted_families) {
      if ([family compare:prev_family])
        [new_families addObject:family];
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


// Fonts need this. XDisplayHeightMM and friends should probably be consistent
// with this as well if they're ever implemented.
static const unsigned dpi = 75;


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
try_xlfd_font (const char *name, float scale,
               char **name_ret, float *size_ret, char **xa_font)
{
  NSFont *nsfont = 0;
  NSString *family_name = nil;
  NSFontTraitMask require = 0, forbid = 0;
  GLboolean rand  = GL_FALSE;
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

    // Incorrect fields are ignored.

    if (*s == '-')
      ++s;
    const char *s2 = xlfd_field_end(s);

    // Foundry (ignore)

    L = xlfd_next (&s, &s2); // Family name
    // This used to substitute Georgia for Times. Now it doesn't.
    if (CMP ("random")) {
      rand = GL_TRUE;
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
  }
# undef CMP
# undef UNSPEC

  if (!family_name && !rand)
    family_name = default_font_family (require);

  if (size < 6 || size > 1000)
    size = 12;

  size *= scale;

  NSFontTraitMask mask = require | forbid;

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
    return 0;
  }
}

#endif

Font
XLoadFont (Display *dpy, const char *name)
{
  Font fid = (Font) calloc (1, sizeof(*fid));
  fid->refcount = 1;
  fid->dpy = dpy;

  // (TODO) float scale = 1;

# ifdef USE_IPHONE
  /* Since iOS screens are physically smaller than desktop screens, scale up
     the fonts to make them more readable.

     Note that X11 apps on iOS also have the backbuffer sized in points
     instead of pixels, resulting in an effective X11 screen size of 768x1024
     or so, even if the display has significantly higher resolution.  That is
     unrelated to this hack, which is really about DPI.
   */
  /* scale = 2; */
# endif

  fid->dpy = dpy;
  fid->native_font = jwxyz_load_native_font (dpy, name,
                                             &fid->ps_name, &fid->size,
                                             &fid->ascent, &fid->descent);
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
  Font font2 = copy_font (fid);
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);
  gc->gcv.font = font2;
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
  /* TODO Fonts
  char *ret = 0;
  CTFontRef ctfont =
    CTFontCreateWithName ((CFStringRef) [fid->nsfont fontName],
                          [fid->nsfont pointSize],
                          NULL);
  Assert (ctfont, @"no CTFontRef for UIFont");

  CGGlyph cgglyph;
  if (CTFontGetGlyphsForCharacters (ctfont, (UniChar *) &uc, &cgglyph, 1)) {
    NSString *name = (NSString *)
      CGFontCopyGlyphNameForGlyph (CTFontCopyGraphicsFont (ctfont, 0),
                                   cgglyph);
    ret = (name ? strdup ([name UTF8String]) : 0);
  }

  CFRelease (ctfont);
  return ret;
   */
  return NULL;
}


// Given a UTF8 string, return an NSString.  Bogus UTF8 characters are ignored.
// We have to do this because stringWithCString returns NULL if there are
// any invalid characters at all.
//
/* TODO
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
*/

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
  jwxyz_render_text (dpy, ff->native_font, s, length, GL_FALSE, cs, 0);
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
                       GL_TRUE, cs, 0);
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
#if 0
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
#endif
  abort();
}


static int
draw_string (Display *dpy, Drawable d, GC gc, int x, int y,
             const char *str, size_t len, GLboolean utf8)
{
  Font ff = gc->gcv.font;
  XCharStruct cs;

  char *data = 0;
  jwxyz_render_text (dpy, ff->native_font, str, len, utf8, &cs, &data);
  int w = cs.rbearing - cs.lbearing;
  int h = cs.ascent + cs.descent;

  if (w < 0 || h < 0) abort();
  if (w == 0 || h == 0) {
    if (data) free(data);
    return 0;
  }

  XImage *img = XCreateImage (dpy, dpy->screen->visual, 32,
                              ZPixmap, 0, data, w, h, 0, 0);

  /* The image of text is a 32-bit image, in white.
     Take the red channel for intensity and use that as alpha.
     replace RGB with the GC's foreground color.
     This expects that XPutImage respects alpha and only writes
     the bits that are not masked out.
     This also assumes that XPutImage expects ARGB.
   */
  {
    char *s = data;
    char *end = s + (w * h * 4);
    uint8_t rgba[4];
    jwxyz_query_color (dpy, gc->gcv.foreground, rgba);
    while (s < end) {

      s[3] = s[1];
      s[0] = rgba[0];
      s[1] = rgba[1];
      s[2] = rgba[2];
      s += 4;
    }
  }

  XPutImage (dpy, d, gc, img, 0, 0, 
             x + cs.lbearing,
             y - cs.ascent,
             w, h);
  XDestroyImage (img);

  return 0;
}

int
XDrawString (Display *dpy, Drawable d, GC gc, int x, int y,
             const char  *str, int len)
{
  return draw_string (dpy, d, gc, x, y, str, len, GL_FALSE);
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
  ret = draw_string (dpy, d, gc, x, y, s2, strlen(s2), GL_TRUE);
  free (s2);
  return ret;
}


void
Xutf8DrawString (Display *dpy, Drawable d, XFontSet set, GC gc,
                 int x, int y, const char *str, int len)
{
  draw_string (dpy, d, gc, x, y, str, len, GL_TRUE);
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
//####  abort();
/*
  TODO

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
*/
  
  return 0;
}

int
XSetClipOrigin (Display *dpy, GC gc, int x, int y)
{
  gc->gcv.clip_x_origin = x;
  gc->gcv.clip_y_origin = y;
  return 0;
}

#endif /* JWXYZ_GL -- entire file */
