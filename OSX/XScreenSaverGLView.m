/* xscreensaver, Copyright (c) 2006-2009 Jamie Zawinski <jwz@jwz.org>
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

#import <OpenGL/OpenGL.h>

/* used by the OpenGL screen savers
 */
extern GLXContext *init_GL (ModeInfo *);
extern void glXSwapBuffers (Display *, Window);
extern void glXMakeCurrent (Display *, Window, GLXContext);
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);


@implementation XScreenSaverGLView

#if 0
- (void) dealloc
{
  /* #### Do we have to destroy ogl_ctx? */
  [super dealloc];
}
#endif


- (void)stopAnimation
{
  [super stopAnimation];
  
  // Without this, the GL frame stays on screen when switching tabs
  // in System Preferences.
  //
  [NSOpenGLContext clearCurrentContext];
}


// #### maybe this could/should just be on 'lockFocus' instead?
- (void) prepareContext
{
  if (ogl_ctx) {
    [ogl_ctx makeCurrentContext];
//    check_gl_error ("makeCurrentContext");
    [ogl_ctx update];
  }
}


- (void) resizeContext
{
  if (ogl_ctx) 
    [ogl_ctx setView:self];
}


- (void)drawRect:(NSRect)rect
{
  if (! ogl_ctx)
    [super drawRect:rect];
}


- (NSOpenGLContext *) oglContext
{
  return ogl_ctx;
}


- (void) setOglContext: (NSOpenGLContext *) ctx
{
  ogl_ctx = ctx;
  [self resizeContext];
}

@end


/* Utility functions...
 */


/* Called by OpenGL savers using the XLockmore API.
 */
GLXContext *
init_GL (ModeInfo *mi)
{
  Window win = mi->window;
  XScreenSaverGLView *view = (XScreenSaverGLView *) jwxyz_window_view (win);
  NSOpenGLContext *ctx = [view oglContext];

  if (!ctx) {

    NSOpenGLPixelFormatAttribute attrs[20];
    int i = 0;
    attrs[i++] = NSOpenGLPFAColorSize; attrs[i++] = 24;
    attrs[i++] = NSOpenGLPFAAlphaSize; attrs[i++] = 8;
    attrs[i++] = NSOpenGLPFADepthSize; attrs[i++] = 16;

    if (get_boolean_resource (mi->dpy, "doubleBuffer", "DoubleBuffer"))
      attrs[i++] = NSOpenGLPFADoubleBuffer;

    attrs[i] = 0;

    NSOpenGLPixelFormat *pixfmt = [[NSOpenGLPixelFormat alloc] 
                                    initWithAttributes:attrs];

    ctx = [[NSOpenGLContext alloc] 
            initWithFormat:pixfmt
              shareContext:nil];
  }

  // Sync refreshes to the vertical blanking interval
  GLint r = 1;
  [ctx setValues:&r forParameter:NSOpenGLCPSwapInterval];
  check_gl_error ("NSOpenGLCPSwapInterval");

  [ctx makeCurrentContext];
  check_gl_error ("makeCurrentContext");

  [view setOglContext:ctx];

  // Clear frame buffer ASAP, else there are bits left over from other apps.
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//  glFinish ();
//  glXSwapBuffers (mi->dpy, mi->window);


  // Enable multi-threading, if possible.  This runs most OpenGL commands
  // and GPU management on a second CPU.
  {
#   ifndef  kCGLCEMPEngine
#    define kCGLCEMPEngine 313  // Added in MacOS 10.4.8 + XCode 2.4.
#   endif
    CGLContextObj cctx = CGLGetCurrentContext();
    CGLError err = CGLEnable (cctx, kCGLCEMPEngine);
    if (err != kCGLNoError) {
      NSLog (@"enabling multi-threaded OpenGL failed: %d", err);
    }
  }

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
  NSOpenGLContext *ctx = [view oglContext];
  if (ctx) [ctx flushBuffer]; // despite name, this actually swaps
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
#ifdef GL_TABLE_TOO_LARGE_EXT
    case GL_TABLE_TOO_LARGE_EXT:   e = "table too large";   break;
#endif
#ifdef GL_TEXTURE_TOO_LARGE_EXT
    case GL_TEXTURE_TOO_LARGE_EXT: e = "texture too large"; break;
#endif
    default:
      e = buf; sprintf (buf, "unknown GL error %d", (int) i); break;
  }
  NSLog (@"%s GL error: %s", type, e);
  exit (1);
}
