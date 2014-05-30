/* xscreensaver, Copyright (c) 2006-2012 Jamie Zawinski <jwz@jwz.org>
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

#import "XScreenSaverView.h"

#ifdef USE_IPHONE
# import <OpenGLES/EAGL.h>
# import <OpenGLES/ES1/gl.h>
# import <OpenGLES/ES1/glext.h>
# import <QuartzCore/QuartzCore.h>
# define NSOpenGLContext EAGLContext
#else
# import <AppKit/NSOpenGL.h>
#endif

@interface XScreenSaverGLView : XScreenSaverView
{
  NSOpenGLContext *ogl_ctx;      // OpenGL rendering context

# ifdef USE_IPHONE
  GLuint gl_framebuffer, gl_renderbuffer, gl_depthbuffer;
# endif /* USE_IPHONE */
}

@end
