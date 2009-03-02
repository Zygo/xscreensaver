/* xscreensaver, Copyright (c) 2006, 2007 Jamie Zawinski <jwz@jwz.org>
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

- (void) dealloc
{
  if (agl_ctx)
    aglDestroyContext (agl_ctx);
  [super dealloc];
}


- (void)stopAnimation
{
  [super stopAnimation];
  
  // Without this, the GL frame stays on screen when switching tabs
  // in System Preferences.
  //
  if (agl_ctx) {
    aglSetCurrentContext (NULL);
    aglDestroyContext (agl_ctx);
    agl_ctx = 0;
  }
}


// #### maybe this could/should just be on 'lockFocus' instead?
- (void) prepareContext
{
  if (agl_ctx)
    if (!aglSetCurrentContext (agl_ctx)) {
      check_gl_error ("aglSetCurrentContext");
      abort();
    }
}
      
- (void) resizeContext
{
  if (! agl_ctx) 
    return;

  /* Constrain the AGL context to the rectangle of this view
     (not of our parent window).
   */
  GLint aglrect[4];
  NSRect rect = [[[self window] contentView] convertRect:[self frame]
                                                fromView:self];
  aglrect[0] = rect.origin.x;
  aglrect[1] = rect.origin.y;
  aglrect[2] = rect.size.width;
  aglrect[3] = rect.size.height;
  aglEnable (agl_ctx, AGL_BUFFER_RECT);
  aglSetInteger (agl_ctx, AGL_BUFFER_RECT, aglrect);
  aglUpdateContext (agl_ctx);
}


- (void)drawRect:(NSRect)rect
{
  if (! agl_ctx)
    [super drawRect:rect];
}


- (AGLContext) aglContext
{
  return agl_ctx;
}

- (void) setAglContext: (AGLContext) ctx
{
  if (agl_ctx)
    if (! aglDestroyContext (agl_ctx)) {
      check_gl_error("aglDestroyContext");
      abort();
    }
  agl_ctx = ctx;
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
  
  GLint agl_attrs[] = {
    AGL_RGBA,
    AGL_DOUBLEBUFFER,
    AGL_DEPTH_SIZE, 16,
    0 };
  AGLPixelFormat aglpixf = aglChoosePixelFormat (NULL, 0, agl_attrs);
  AGLContext ctx = aglCreateContext (aglpixf, NULL);
  aglDestroyPixelFormat (aglpixf);
  if (! ctx) {
    check_gl_error("aglCreateContext");
    abort();
  }
  
  if (! aglSetDrawable (ctx, GetWindowPort ([[view window] windowRef]))) {
    check_gl_error("aglSetDrawable");
    abort();
  }

  if (! aglSetCurrentContext (ctx)) {
    check_gl_error("aglSetCurrentContext");
    abort();
  }

  [view setAglContext:ctx];

  // Sync refreshes to the vertical blanking interval
  GLint r = 1;
  aglSetInteger (ctx, AGL_SWAP_INTERVAL, &r);

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
  AGLContext ctx = [view aglContext];
  if (ctx) aglSwapBuffers (ctx);
}

/* Does nothing. 
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
  while (aglGetError() != AGL_NO_ERROR)
    ;
}

static void
check_agl_error (const char *type)
{
  char buf[100];
  GLenum i;
  const char *e;
  switch ((i = aglGetError())) {
    case AGL_NO_ERROR: return;
    case AGL_BAD_ATTRIBUTE:        e = "bad attribute";	break;
    case AGL_BAD_PROPERTY:         e = "bad propery";	break;
    case AGL_BAD_PIXELFMT:         e = "bad pixelfmt";	break;
    case AGL_BAD_RENDINFO:         e = "bad rendinfo";	break;
    case AGL_BAD_CONTEXT:          e = "bad context";	break;
    case AGL_BAD_DRAWABLE:         e = "bad drawable";	break;
    case AGL_BAD_GDEV:             e = "bad gdev";	break;
    case AGL_BAD_STATE:            e = "bad state";	break;
    case AGL_BAD_VALUE:            e = "bad value";	break;
    case AGL_BAD_MATCH:            e = "bad match";	break;
    case AGL_BAD_ENUM:             e = "bad enum";	break;
    case AGL_BAD_OFFSCREEN:        e = "bad offscreen";	break;
    case AGL_BAD_FULLSCREEN:       e = "bad fullscreen";break;
    case AGL_BAD_WINDOW:           e = "bad window";	break;
    case AGL_BAD_POINTER:          e = "bad pointer";	break;
    case AGL_BAD_MODULE:           e = "bad module";	break;
    case AGL_BAD_ALLOC:            e = "bad alloc";	break;
    case AGL_BAD_CONNECTION:       e = "bad connection";break;
    default:
      e = buf; sprintf (buf, "unknown AGL error %d", (int) i); break;
  }
  NSLog (@"%s AGL error: %s", type, e);
  exit (1);
}


/* report a GL error. */
void
check_gl_error (const char *type)
{
  check_agl_error (type);
  
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
