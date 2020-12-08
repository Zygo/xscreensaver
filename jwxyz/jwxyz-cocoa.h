/* xscreensaver, Copyright (c) 1991-2015 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __JWXYZ_COCOA_H__
#define __JWXYZ_COCOA_H__

#import "XScreenSaverView.h"

#ifdef HAVE_IPHONE
# import <UIKit/UIKit.h>
# define NSView           UIView
# define NSOpenGLContext  EAGLContext
#endif

#ifdef JWXYZ_QUARTZ

struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  CGContextRef cgc;
  CGImageRef cgi;
  XRectangle frame;
  union {
    struct {
      XScreenSaverView *view;
      int last_mouse_x, last_mouse_y;
    } window;
    struct {
      int depth;
      void *cgc_buffer;		// the bits to which CGContextRef renders
    } pixmap;
  };
};

#elif defined JWXYZ_GL

struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  /* OS X: Contexts are unique for each pixmap, 'cause life is hectic. (OS X
           appears to dislike it when you attach different pbuffers to the
           same context one after the other, apparently.) The Window has this
           CFRetained because of garbage collection. For both Pixmaps and
           Windows, CFRelease this when done.
     iOS:  ogl_ctx here is set to either XScreenSaverView.ogl_ctx or
           XRootWindow()->window.ogl_ctx_pixmap. No garbage collection antics
           here, so no need to CFRetain anything. Plus, if a screenhack leaks
           a Pixmap (and they do that all the time), ogl_ctx_pixmap will also
           get leaked if a Pixmap CFRetains this.
   */
  NSOpenGLContext *ogl_ctx;      // OpenGL rendering context (OS X)
# ifdef HAVE_IPHONE
  // TODO: Also on OS X as extensions.
  GLuint gl_framebuffer, gl_renderbuffer;
# endif // HAVE_IPHONE
  CGImageRef cgi;
  XRectangle frame;
  union {
    struct {
      XScreenSaverView *view;
      int last_mouse_x, last_mouse_y;
      struct jwxyz_Drawable *current_drawable;
# ifndef HAVE_IPHONE
      NSOpenGLPixelFormat *pixfmt;
      GLint virtual_screen;
# else // HAVE_IPHONE
      NSOpenGLContext *ogl_ctx_pixmap;
# endif
    } window;
    struct {
      int depth;
# ifndef HAVE_IPHONE
      NSOpenGLPixelBuffer *gl_pbuffer;
      // GLuint blit_texture; // TODO: For blitting from Pbuffers
# endif
    } pixmap;
  };
};

#endif // JWXYZ_GL

extern NSString *nsstring_from(const char *str, size_t len, int utf8_p);

#ifdef HAVE_IPHONE
extern void create_framebuffer (GLuint *gl_framebuffer,
                                GLuint *gl_renderbuffer);
extern void check_framebuffer_status (void);
#endif // HAVE_IPHONE

#define jwxyz_window_view(w) ((w)->window.view)

#endif
