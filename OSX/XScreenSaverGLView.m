/* xscreensaver, Copyright (c) 2006-2015 Jamie Zawinski <jwz@jwz.org>
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
#import "screenhackI.h"
#import "xlockmoreI.h"

#ifdef USE_IPHONE
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


- (NSOpenGLContext *) oglContext
{
  return ogl_ctx;
}


#ifdef USE_IPHONE
/* With GL programs, drawing at full resolution isn't a problem.
 */
- (CGFloat) hackedContentScaleFactor
{
  NSSize ssize = [[[UIScreen mainScreen] currentMode] size];
  NSSize bsize = [self bounds].size;

  // Ratio of screen size in pixels to view size in points.
  GLfloat s = ((ssize.width > ssize.height ? ssize.width : ssize.height) /
               (bsize.width > bsize.height ? bsize.width : bsize.height));
  return s;
}
#endif // USE_IPHONE


#ifdef USE_IPHONE
- (void) swapBuffers
{
  glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_renderbuffer);
  [ogl_ctx presentRenderbuffer:GL_RENDERBUFFER_OES];
}
#endif // USE_IPHONE


#ifdef USE_BACKBUFFER

- (void) animateOneFrame
{
# ifdef USE_IPHONE
  UIGraphicsPushContext (backbuffer);
# endif

  [self render_x11];

# ifdef USE_IPHONE
  UIGraphicsPopContext();
# endif
}


/* GL screenhacks don't display a backbuffer, so this is a stub. */
- (void) enableBackbuffer:(CGSize)new_backbuffer_size
{
}


/* The backbuffer isn't actually used for GL programs, but it needs to
   be there for X11 calls to not error out.  However, nothing done with
   X11 calls will ever show up!  It all gets written into the backbuffer
   and discarded.  That's ok, though, because mostly it's just calls to
   XClearWindow and housekeeping stuff like that.  So we make a tiny one.
 */
- (void) createBackbuffer:(CGSize)new_size
{
  NSAssert (! backbuffer_texture,
			@"backbuffer_texture shouldn't be used for GL hacks");

  backbuffer_size = new_size;

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
}


/* Another stub for GL screenhacks. */
- (void) drawBackbuffer
{
}


/* Likewise. GL screenhacks control display with glXSwapBuffers(). */
- (void) flushBackbufer
{
}


# endif // USE_BACKBUFFER


/* When changing the device orientation, leave the X11 Window and glViewport
   in portrait configuration.  OpenGL hacks examine current_device_rotation()
   within the scene as needed.
 */
- (BOOL)reshapeRotatedWindow
{
  return NO;
}


#ifndef USE_IPHONE

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
   */
  //  if (ms_p && [[view window] screen] != [[NSScreen screens] objectAtIndex:0])
  //    ms_p = 0;

  if (ms_p) {
    attrs[i++] = NSOpenGLPFASampleBuffers; attrs[i++] = 1;
    attrs[i++] = NSOpenGLPFASamples;       attrs[i++] = 6;
    // Don't really understand what this means:
    // attrs[i++] = NSOpenGLPFANoRecovery;
  }

  attrs[i] = 0;

  NSOpenGLPixelFormat *pixfmt = [[NSOpenGLPixelFormat alloc]
                                 initWithAttributes:attrs];

  if (ms_p && !pixfmt) {   // Retry without multisampling.
    i -= 2;
    attrs[i] = 0;
    pixfmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  }

  return [pixfmt autorelease];
}

#else // !USE_IPHONE

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

#endif // !USE_IPHONE


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
  Window win = mi->window;
  XScreenSaverGLView *view = (XScreenSaverGLView *) jwxyz_window_view (win);
  NSAssert1 ([view isKindOfClass:[XScreenSaverGLView class]],
             @"wrong view class: %@", view);

  // OpenGL initialization is in [XScreenSaverView startAnimation].

# ifdef USE_IPHONE
  jwzgles_reset ();
# endif // USE_IPHONE

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
  static int blort = -1;
  return (void *) &blort;
}


/* Copy the back buffer to the front buffer.
 */
void
glXSwapBuffers (Display *dpy, Window window)
{
  XScreenSaverGLView *view = (XScreenSaverGLView *) jwxyz_window_view (window);
  NSAssert1 ([view isKindOfClass:[XScreenSaverGLView class]],
             @"wrong view class: %@", view);
#ifndef USE_IPHONE
  NSOpenGLContext *ctx = [view oglContext];
  if (ctx) [ctx flushBuffer]; // despite name, this actually swaps
#else /* USE_IPHONE */
  [view swapBuffers];
#endif /* USE_IPHONE */
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
