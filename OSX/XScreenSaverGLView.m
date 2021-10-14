/* xscreensaver, Copyright © 2006-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a subclass of Apple's ScreenSaverView that knows how to run
   xscreensaver programs without X11 via the dark magic of the "jwxyz"
   library.  In xscreensaver terminology, this is the replacement for
   the "screenhack.c" module.
 */

#import "XScreenSaverGLView.h"
#import "XScreenSaverConfigSheet.h"
#import "jwxyz-cocoa.h"
#import "jwxyzI.h"
#import "screenhackI.h"
#import "xlockmoreI.h"

#ifdef HAVE_IPHONE
# include "jwzgles.h"
# import <OpenGLES/ES1/glext.h>
#else
# import <OpenGL/OpenGL.h>
#endif

/* used by the OpenGL screen savers
 */
extern GLXContext *init_GL (ModeInfo *);
extern void glXSwapBuffers (Display *, Window);
extern void glXMakeCurrent (Display *, Window, GLXContext);
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);


@implementation XScreenSaverGLView


/* With GL programs, drawing at full resolution isn't a problem.
 */
- (CGFloat) hackedContentScaleFactor:(BOOL)fonts_p
{
# ifdef HAVE_IPHONE
  return [self contentScaleFactor];
# else
  NSAssert (self.window, @"no window in view");
  if (! self.window) abort();
  return self.window.backingScaleFactor;
# endif
}

# ifdef HAVE_IPHONE

- (BOOL)ignoreRotation
{
  return FALSE;		// Allow xwindow and the glViewport to change shape
}

- (BOOL) suppressRotationAnimation
{
  return _suppressRotationAnimation;  // per-hack setting, default FALSE
}

- (BOOL) rotateTouches
{
  return TRUE;		// We need the XY axes swapped in our events
}


- (void) swapBuffers
{
#  ifdef JWXYZ_GL
  GLint gl_renderbuffer = xwindow->gl_renderbuffer;
#  endif // JWXYZ_GL
  glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_renderbuffer);
  [ogl_ctx presentRenderbuffer:GL_RENDERBUFFER_OES];
}
#endif // HAVE_IPHONE


- (void) animateOneFrame
{
# if defined HAVE_IPHONE && defined JWXYZ_QUARTZ
  UIGraphicsPushContext (backbuffer);
# endif

  [self render_x11];

# if defined HAVE_IPHONE && defined JWXYZ_QUARTZ
  UIGraphicsPopContext();
# endif
}


/* GL screenhacks don't display a backbuffer, so this is a stub. */
- (void) enableBackbuffer:(CGSize)new_backbuffer_size
{
}


/* GL screenhacks set their own viewport and matrices. */
- (void) setViewport
{
}


#ifdef HAVE_IPHONE

/* Keep the GL scene oriented into a portrait-mode View, regardless of
   what the physical device orientation is.
 */
- (void) reshape_x11
{
  [super reshape_x11];

  glMatrixMode(GL_PROJECTION);
  glRotatef (-current_device_rotation(), 0, 0, 1);
  glMatrixMode(GL_MODELVIEW);
}

- (void) render_x11
{
  BOOL was_initted_p = initted_p;
  [super render_x11];

  if (! was_initted_p && xdpy)
    _suppressRotationAnimation =
      get_boolean_resource (xdpy,
                            "suppressRotationAnimation",
                            "SuppressRotationAnimation");
}

#endif // HAVE_IPHONE



/* The backbuffer isn't actually used for GL programs, but it needs to
   be there for X11 calls to not error out.  However, nothing done with
   X11 calls will ever show up!  It all gets written into the backbuffer
   and discarded.  That's ok, though, because mostly it's just calls to
   XClearWindow and housekeeping stuff like that.  So we make a tiny one.
 */
- (void) createBackbuffer:(CGSize)new_size
{
#ifdef JWXYZ_QUARTZ
  NSAssert (! backbuffer_texture,
			@"backbuffer_texture shouldn't be used for GL hacks");

  if (! backbuffer) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    int w = 8;
    int h = 8;
    backbuffer = CGBitmapContextCreate (NULL, w, h,   // yup, only 8px x 8px.
                                        8, w*4, cs,
                                        (kCGBitmapByteOrder32Little |
                                         kCGImageAlphaNoneSkipLast));
    CGColorSpaceRelease (cs);
  }
#endif // JWXYZ_QUARTZ
}


/* Another stub for GL screenhacks. */
- (void) drawBackbuffer
{
}


/* Likewise. GL screenhacks control display with glXSwapBuffers(). */
- (void) flushBackbuffer
{
}


#ifndef HAVE_IPHONE

- (NSOpenGLPixelFormat *) getGLPixelFormat
{
  NSOpenGLPixelFormatAttribute attrs[40];
  int i = 0;
  attrs[i++] = NSOpenGLPFAColorSize; attrs[i++] = 24;
  attrs[i++] = NSOpenGLPFAAlphaSize; attrs[i++] = 8;
  attrs[i++] = NSOpenGLPFADepthSize; attrs[i++] = 24;

  if ([prefsReader getBooleanResource:"doubleBuffer"])
    attrs[i++] = NSOpenGLPFADoubleBuffer;

  Bool ms_p = [prefsReader getBooleanResource:"multiSample"];

  /* Sometimes, turning on multisampling kills performance.  At one point,
     I thought the answer was, "only run multisampling on one screen, and
     leave it turned off on other screens".  That's what this code does,
     but it turns out, that solution is insufficient.  I can't really tell
     what causes poor performance with multisampling, but it's not
     predictable.  Without changing the code, some times a given saver will
     perform fine with multisampling on, and other times it will perform
     very badly.  Without multisampling, they always perform fine.

     Update, 2021: In this modern world of Retina screens, I don't think
     there's any point in trying to get multisampling to work.  Modern systems
     all have retina / hiDPI screens, meaning that a logical pixel is already
     2 or 3.5 physical pixels, and the whole point of having displays like
     that is that software antialiasing isn't necessary any more because the
     individual pixels are small enough that human eyes can't see them as
     rectangles.  If you already have 0.16mm pixels, having 0.08mm "virtual"
     pixels is not perceptible.

     So multisample only makes sense on non-retina displays -- and those are
     the *old* video cards and GPUs, that are most likely to be resource
     limited and most likely to screw up in unpredictable ways!
   */
  //  if (ms_p && [[view window] screen] != [[NSScreen screens] objectAtIndex:0])
  //    ms_p = 0;

  if (ms_p) {
    attrs[i++] = NSOpenGLPFASampleBuffers; attrs[i++] = 1;
    attrs[i++] = NSOpenGLPFASamples;       attrs[i++] = 6;
    // Don't really understand what this means:
    // attrs[i++] = NSOpenGLPFANoRecovery;
  }

# pragma clang diagnostic push   // "NSOpenGLPFAWindow deprecated in 10.9"
# pragma clang diagnostic ignored "-Wdeprecated"
  attrs[i++] = NSOpenGLPFAWindow;
# pragma clang diagnostic pop

# ifdef JWXYZ_GL
  attrs[i++] = NSOpenGLPFAPixelBuffer;
# endif

  attrs[i] = 0;

  NSOpenGLPixelFormat *result = [[NSOpenGLPixelFormat alloc]
                                 initWithAttributes:attrs];

  if (ms_p && !result) {   // Retry without multisampling.
    i -= 2;
    attrs[i] = 0;
    result = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  }

  return [result autorelease];
}

#else // !HAVE_IPHONE

- (NSDictionary *)getGLProperties
{
  Bool dbuf_p = [prefsReader getBooleanResource:"doubleBuffer"];

  /* There seems to be no way to actually turn off double-buffering in
     EAGLContext (e.g., no way to draw to the front buffer directly)
     but if we turn on "retained backing" for non-buffering apps like
     "pipes", at least the back buffer isn't auto-cleared on them.
   */

  return [NSDictionary dictionaryWithObjectsAndKeys:
   kEAGLColorFormatRGBA8,             kEAGLDrawablePropertyColorFormat,
   [NSNumber numberWithBool:!dbuf_p], kEAGLDrawablePropertyRetainedBacking,
   nil];
}

- (void)addExtraRenderbuffers:(CGSize)size
{
  int w = size.width;
  int h = size.height;

  if (gl_depthbuffer)  glDeleteRenderbuffersOES (1, &gl_depthbuffer);

  glGenRenderbuffersOES (1, &gl_depthbuffer);
  // [EAGLContext renderbufferStorage:fromDrawable:] must be called before this.
  glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_depthbuffer);
  glRenderbufferStorageOES (GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES,
                            w, h);
  glFramebufferRenderbufferOES (GL_FRAMEBUFFER_OES,  GL_DEPTH_ATTACHMENT_OES,
                                GL_RENDERBUFFER_OES, gl_depthbuffer);
}

- (NSString *)getCAGravity
{
  return kCAGravityCenter;
}

- (void) startAnimation
{
  [super startAnimation];
  if (ogl_ctx) /* Almost always true. */
    _glesState = jwzgles_make_state ();
}

- (void) stopAnimation
{
  [super stopAnimation];
#ifdef HAVE_IPHONE
  if (_glesState) {
    [EAGLContext setCurrentContext:ogl_ctx];
    jwzgles_make_current (_glesState);
    jwzgles_free_state ();
  }
#endif
}

- (void) prepareContext
{
  [super prepareContext];
  jwzgles_make_current (_glesState);
}

#endif // !HAVE_IPHONE


- (void)dealloc {
  // ogl_ctx
  // gl_framebuffer
  // gl_renderbuffer
  // gl_depthbuffer
  [super dealloc];
}

@end


/* Utility functions...
 */


// redefine NSAssert, etc. here since they don't work when not inside
// an ObjC method.

#undef NSAssert
#undef NSAssert1
#undef NSAssert2
#define NSASS(S) \
  jwxyz_abort ("%s", [(S) cStringUsingEncoding:NSUTF8StringEncoding])
#define NSAssert(CC,S)      do { if (!(CC)) { NSASS((S)); }} while(0)
#define NSAssert1(CC,S,A)   do { if (!(CC)) { \
  NSASS(([NSString stringWithFormat: S, A])); }} while(0)
#define NSAssert2(CC,S,A,B) do { if (!(CC)) { \
  NSASS(([NSString stringWithFormat: S, A, B])); }} while(0)


/* Called by OpenGL savers using the XLockmore API.
 */
GLXContext *
init_GL (ModeInfo *mi)
{
#if 0
  Window win = mi->window;
  XScreenSaverGLView *view = (XScreenSaverGLView *) jwxyz_window_view (win);
  NSAssert1 ([view isKindOfClass:[XScreenSaverGLView class]],
             @"wrong view class: %@", view);
#endif

  // OpenGL initialization is in [XScreenSaverView startAnimation].

  // I don't know why this is necessary, but it beats randomly having some
  // textures be upside down.
  //
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Caller expects a pointer to an opaque struct...  which it dereferences.
  // Don't ask me, it's historical...
  static GLXContext blort = (GLXContext) -1;
  return &blort;
}


/* Copy the back buffer to the front buffer.
 */
void
glXSwapBuffers (Display *dpy, Window window)
{
  // This all is very much like what's in -[XScreenSaverView flushBackbuffer].
#ifdef JWXYZ_GL
  jwxyz_bind_drawable (window, window);
#endif // JWXYZ_GL

  XScreenSaverGLView *view = (XScreenSaverGLView *) jwxyz_window_view (window);
#if 0
  NSAssert1 ([view isKindOfClass:[XScreenSaverGLView class]],
             @"wrong view class: %@", view);
#endif
#ifndef HAVE_IPHONE
  NSOpenGLContext *ctx = [view oglContext];
  if (ctx) [ctx flushBuffer]; // despite name, this actually swaps
#else /* HAVE_IPHONE */
  [view swapBuffers];
#endif /* HAVE_IPHONE */
}

/* Does nothing - prepareContext already did the work.
 */
void
glXMakeCurrent (Display *dpy, Window window, GLXContext context)
{
}


/* clear away any lingering error codes */
void
clear_gl_error (void)
{
  while (glGetError() != GL_NO_ERROR)
    ;
}


#if defined GL_INVALID_FRAMEBUFFER_OPERATION_OES && \
  !defined GL_INVALID_FRAMEBUFFER_OPERATION
# define GL_INVALID_FRAMEBUFFER_OPERATION GL_INVALID_FRAMEBUFFER_OPERATION_OES
#endif


/* report a GL error. */
void
check_gl_error (const char *type)
{
  char buf[100];
  GLenum i;
  const char *e;
  switch ((i = glGetError())) {
    case GL_NO_ERROR: return;
    case GL_INVALID_ENUM:          e = "invalid enum";      break;
    case GL_INVALID_VALUE:         e = "invalid value";     break;
    case GL_INVALID_OPERATION:     e = "invalid operation"; break;
    case GL_STACK_OVERFLOW:        e = "stack overflow";    break;
    case GL_STACK_UNDERFLOW:       e = "stack underflow";   break;
    case GL_OUT_OF_MEMORY:         e = "out of memory";     break;
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      e = "invalid framebuffer operation";
      break;
#endif
#ifdef GL_TABLE_TOO_LARGE_EXT
    case GL_TABLE_TOO_LARGE_EXT:   e = "table too large";   break;
#endif
#ifdef GL_TEXTURE_TOO_LARGE_EXT
    case GL_TEXTURE_TOO_LARGE_EXT: e = "texture too large"; break;
#endif
    default:
      e = buf; sprintf (buf, "unknown GL error %d", (int) i); break;
  }
  NSAssert2 (0, @"%s GL error: %s", type, e);
}
