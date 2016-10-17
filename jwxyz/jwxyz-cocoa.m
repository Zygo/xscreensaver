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
   Xlib and that do Cocoa-ish or OpenGL-ish things that bear some resemblance
   to the things that Xlib might have done.

   This code is used by both the original jwxyz.m and the new jwxyz-gl.c.
 */

#import "jwxyzI.h"
#import "jwxyz-cocoa.h"

#include <stdarg.h>

#ifdef USE_IPHONE
# import <OpenGLES/ES1/gl.h>
# import <OpenGLES/ES1/glext.h>
# define NSOpenGLContext EAGLContext
#endif

/* OS X/iOS-specific JWXYZ implementation. */

void
jwxyz_logv (Bool error, const char *fmt, va_list args)
{
  vfprintf (stderr, fmt, args);
  fputc ('\n', stderr);
}

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


const XRectangle *
jwxyz_frame (Drawable d)
{
  return &d->frame;
}


unsigned int
jwxyz_drawable_depth (Drawable d)
{
  return (d->type == WINDOW
          ? visual_depth (NULL, NULL)
          : d->pixmap.depth);
}


void
jwxyz_get_pos (Window w, XPoint *xvpos, XPoint *xp)
{
# ifdef USE_IPHONE

  xvpos->x = 0;
  xvpos->y = 0;

  if (xp) {
    xp->x = w->window.last_mouse_x;
    xp->y = w->window.last_mouse_y;
  }

# else  // !USE_IPHONE

  NSWindow *nsw = [w->window.view window];

  // get bottom left of window on screen, from bottom left

# if (MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_6)
  NSRect rr1 = [w->window.view convertRect: NSMakeRect(0,0,0,0) toView:nil];
  NSRect rr2 = [nsw convertRectToScreen: rr1];
  NSPoint wpos = NSMakePoint (rr2.origin.x - rr1.origin.x,
                              rr2.origin.y - rr1.origin.y);
# else
  // deprecated as of 10.7
  NSPoint wpos = [nsw convertBaseToScreen: NSMakePoint(0,0)];
# endif


  // get bottom left of view on window, from bottom left
  NSPoint vpos;
  vpos.x = vpos.y = 0;
  vpos = [w->window.view convertPoint:vpos toView:[nsw contentView]];

  // get bottom left of view on screen, from bottom left
  vpos.x += wpos.x;
  vpos.y += wpos.y;

  // get top left of view on screen, from bottom left
  vpos.y += w->frame.height;

  // get top left of view on screen, from top left
  NSArray *screens = [NSScreen screens];
  NSScreen *screen = (screens && [screens count] > 0
                      ? [screens objectAtIndex:0]
                      : [NSScreen mainScreen]);
  NSRect srect = [screen frame];
  vpos.y = srect.size.height - vpos.y;

  xvpos->x = vpos.x;
  xvpos->y = vpos.y;

  if (xp) {
    // get the mouse position on window, from bottom left
    NSEvent *e = [NSApp currentEvent];
    NSPoint p = [e locationInWindow];

    // get mouse position on screen, from bottom left
    p.x += wpos.x;
    p.y += wpos.y;

    // get mouse position on screen, from top left
    p.y = srect.size.height - p.y;

    xp->x = (int) p.x;
    xp->y = (int) p.y;
  }

# endif // !USE_IPHONE
}


#ifdef USE_IPHONE

void
check_framebuffer_status (void)
{
  int err = glCheckFramebufferStatusOES (GL_FRAMEBUFFER_OES);
  switch (err) {
  case GL_FRAMEBUFFER_COMPLETE_OES:
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES:
    Assert (0, "framebuffer incomplete attachment");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES:
    Assert (0, "framebuffer incomplete missing attachment");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES:
    Assert (0, "framebuffer incomplete dimensions");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES:
    Assert (0, "framebuffer incomplete formats");
    break;
  case GL_FRAMEBUFFER_UNSUPPORTED_OES:
    Assert (0, "framebuffer unsupported");
    break;
/*
  case GL_FRAMEBUFFER_UNDEFINED:
    Assert (0, "framebuffer undefined");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
    Assert (0, "framebuffer incomplete draw buffer");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
    Assert (0, "framebuffer incomplete read buffer");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
    Assert (0, "framebuffer incomplete multisample");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
    Assert (0, "framebuffer incomplete layer targets");
    break;
 */
  default:
    NSCAssert (0, @"framebuffer incomplete, unknown error 0x%04X", err);
    break;
  }
}


void
create_framebuffer (GLuint *gl_framebuffer, GLuint *gl_renderbuffer)
{
  glGenFramebuffersOES  (1, gl_framebuffer);
  glBindFramebufferOES  (GL_FRAMEBUFFER_OES,  *gl_framebuffer);

  glGenRenderbuffersOES (1, gl_renderbuffer);
  glBindRenderbufferOES (GL_RENDERBUFFER_OES, *gl_renderbuffer);
}

#endif // USE_IPHONE


#if defined JWXYZ_QUARTZ

/* Pushes a GC context; sets Fill and Stroke colors to the background color.
 */
static void
push_bg_gc (Display *dpy, Drawable d, GC gc, Bool fill_p)
{
  XGCValues *gcv = jwxyz_gc_gcv (gc);
  push_color_gc (dpy, d, gc, gcv->background, gcv->antialias_p, fill_p);
}


void
jwxyz_copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
                 int src_x, int src_y,
                 unsigned int width, unsigned int height,
                 int dst_x, int dst_y)
{
  XRectangle src_frame = src->frame, dst_frame = dst->frame;
  XGCValues *gcv = jwxyz_gc_gcv (gc);

  BOOL mask_p = src->type == PIXMAP && src->pixmap.depth == 1;


  /* If we're copying from a bitmap to a bitmap, and there's nothing funny
     going on with clipping masks or depths or anything, optimize it by
     just doing a memcpy instead of going through a CGI.
   */
  if (gcv->function == GXcopy &&
      !gcv->clip_mask &&
      jwxyz_drawable_depth (src) == jwxyz_drawable_depth (dst)) {

    Assert(!src_frame.x &&
           !src_frame.y &&
           !dst_frame.x &&
           !dst_frame.y,
           "unexpected non-zero origin");

    ptrdiff_t src_pitch = CGBitmapContextGetBytesPerRow(src->cgc);
    ptrdiff_t dst_pitch = CGBitmapContextGetBytesPerRow(dst->cgc);
    char *src_data = seek_xy (CGBitmapContextGetData (src->cgc), src_pitch,
                              src_x, src_y);
    char *dst_data = seek_xy (CGBitmapContextGetData (dst->cgc), dst_pitch,
                              dst_x, dst_y);

    size_t bytes = width * 4;

    if (src == dst && dst_y > src_y) {
      // Copy upwards if the areas might overlap.
      src_data += src_pitch * (height - 1);
      dst_data += dst_pitch * (height - 1);
      src_pitch = -src_pitch;
      dst_pitch = -dst_pitch;
    }

    while (height) {
      // memcpy is an alias for memmove on OS X.
      memmove(dst_data, src_data, bytes);
      src_data += src_pitch;
      dst_data += dst_pitch;
      --height;
    }
  } else {
    CGRect
    src_rect = CGRectMake(src_x, src_y, width, height),
    dst_rect = CGRectMake(dst_x, dst_y, width, height);

    src_rect.origin = map_point (src, src_rect.origin.x,
                                 src_rect.origin.y + src_rect.size.height);
    dst_rect.origin = map_point (dst, dst_rect.origin.x,
                                 dst_rect.origin.y + src_rect.size.height);

    NSObject *releaseme = 0;
    CGImageRef cgi;
    BOOL free_cgi_p = NO;

    // We must first copy the bits to an intermediary CGImage object, then
    // copy that to the destination drawable's CGContext.
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
    if (src_rect.origin.x    != src_frame.x   ||
        src_rect.origin.y    != src_frame.y   ||
        src_rect.size.width  != src_frame.width ||
        src_rect.size.height != src_frame.height) {
      // #### I don't understand why this is needed...
      src_rect.origin.y = (src_frame.height -
                           src_rect.size.height - src_rect.origin.y);
      // This does not copy image data, so it should be fast.
      cgi = CGImageCreateWithImageInRect (cgi, src_rect);
      free_cgi_p = YES;
    }

    CGContextRef cgc = dst->cgc;

    if (mask_p) {		// src depth == 1

      push_bg_gc (dpy, dst, gc, YES);

      // fill the destination rectangle with solid background...
      CGContextFillRect (cgc, dst_rect);

      Assert (cgc, "no CGC with 1-bit XCopyArea");

      // then fill in a solid rectangle of the fg color, using the image as an
      // alpha mask.  (the image has only values of BlackPixel or WhitePixel.)
      set_color (dpy, cgc, gcv->foreground, jwxyz_gc_depth (gc),
                 gcv->alpha_allowed_p, YES);
      CGContextClipToMask (cgc, dst_rect, cgi);
      CGContextFillRect (cgc, dst_rect);

      pop_gc (dst, gc);

    } else {		// src depth > 1

      push_gc (dst, gc);

      // copy the CGImage onto the destination CGContext
      //Assert(CGImageGetColorSpace(cgi) == dpy->colorspace, "bad colorspace");
      CGContextDrawImage (cgc, dst_rect, cgi);

      pop_gc (dst, gc);
    }

    if (free_cgi_p) CGImageRelease (cgi);
    
    if (releaseme) [releaseme release];
  }
  
  invalidate_drawable_cache (dst);
}

#elif defined JWXYZ_GL

/* Warning! The JWXYZ_GL code here is experimental and provisional and not at
   all ready for prime time. Please be careful.
 */

void
jwxyz_copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
                 int src_x, int src_y,
                 unsigned int width, unsigned int height,
                 int dst_x, int dst_y)
{
  /* TODO:
   - glCopyPixels if src == dst
   - Pixel buffer copying
   - APPLE_framebuffer_multisample has ResolveMultisampleFramebufferAPPLE,
     which is like a blit.
   */

  /* Strange and ugly flickering when going the glCopyTexImage2D route on
     OS X. (Early 2009 Mac mini, OS X 10.10)
   */
# ifdef USE_IPHONE
  jwxyz_gl_copy_area_copy_tex_image (dpy, src, dst, gc, src_x, src_y,
                                     width, height, dst_x, dst_y);
# else // !USE_IPHONE
  jwxyz_gl_copy_area_read_pixels (dpy, src, dst, gc,
                                  src_x, src_y, width, height, dst_x, dst_y);
# endif // !USE_IPHONE
  jwxyz_assert_gl ();
}


void
jwxyz_assert_gl ()
{
  // This is like check_gl_error, except this happens for debug builds only.
#ifndef __OPTIMIZE__
  if([NSOpenGLContext currentContext])
  {
    // glFinish here drops FPS into the toilet. It might need to be on if
    // something goes wrong.
    // glFinish();
    GLenum error = glGetError();
    Assert (!error, "jwxyz_assert_gl: OpenGL error");
  }
#endif // !__OPTIMIZE__
}

void
jwxyz_assert_drawable (Window main_window, Drawable d)
{
#if !defined USE_IPHONE && !defined __OPTIMIZE__
  XScreenSaverView *view = main_window->window.view;
  NSOpenGLContext *ogl_ctx = [view oglContext];

  if (d->type == WINDOW) {
    Assert([ogl_ctx view] == view,
           "jwxyz_assert_display: ogl_ctx view not set!");
  }

  @try {
    /* Assert([d->ctx isKindOfClass:[NSOpenGLContext class]], "Not a context."); */
    Class c = [ogl_ctx class];
    Assert([c isSubclassOfClass:[NSOpenGLContext class]], "Not a context.");
    // [d->ctx makeCurrentContext];
  }
  @catch (NSException *exception) {
    perror([[exception reason] UTF8String]);
    abort();
  }
#endif // !USE_IPHONE && !__OPTIMIZE__
}


void
jwxyz_bind_drawable (Window main_window, Drawable d)
{
  /* Windows and Pixmaps need to use different contexts with OpenGL
     screenhacks, because an OpenGL screenhack sets state in an arbitrary
     fashion, but jwxyz-gl.c has its own ideas w.r.t. OpenGL state.

     On iOS, all pixmaps can use the same context with different FBOs. Which
     is nice.
   */

  /* OpenGL screenhacks in general won't be drawing on the Window, but they
     can and will draw on a Pixmap -- but an OpenGL call following an Xlib
     call won't be able to fix the fact that it's drawing offscreen.
   */

  /* EXT_direct_state_access might be appropriate, but it's apparently not
     available on Mac OS X.
   */

  // jwxyz_assert_display (dpy);
  jwxyz_assert_drawable (main_window, main_window);
  jwxyz_assert_gl ();
  jwxyz_assert_drawable (main_window, d);

#if defined USE_IPHONE && !defined __OPTIMIZE__
  Drawable current_drawable = main_window->window.current_drawable;
  Assert (!current_drawable
            || current_drawable->ogl_ctx == [EAGLContext currentContext],
          "bind_drawable: Wrong context.");
  if (current_drawable) {
    GLint framebuffer;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING_OES, &framebuffer);
    Assert (framebuffer == current_drawable->gl_framebuffer,
            "bind_drawable: Wrong framebuffer.");
  }
#endif

  if (main_window->window.current_drawable != d) {
    main_window->window.current_drawable = d;

    /* Doing this repeatedly is probably not OK performance-wise. Probably. */
#ifndef USE_IPHONE
    [d->ogl_ctx makeCurrentContext];
#else
    [EAGLContext setCurrentContext:d->ogl_ctx];
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, d->gl_framebuffer);
    if (d->type == PIXMAP) {
      glViewport (0, 0, d->frame.width, d->frame.height);
      jwxyz_set_matrices (d->frame.width, d->frame.height);
    }
#endif
  }

  jwxyz_assert_gl ();
}


Pixmap
XCreatePixmap (Display *dpy, Drawable d,
               unsigned int width, unsigned int height, unsigned int depth)
{
  Pixmap p = (Pixmap) calloc (1, sizeof(*p));
  p->type = PIXMAP;
  p->frame.width  = width;
  p->frame.height = height;
  p->pixmap.depth = depth;

  Assert (depth == 1 || depth == 32, "XCreatePixmap: depth must be 32");

  /* TODO: If Pixel buffers are not supported, do something about it. */
  Window w = XRootWindow (dpy, 0);

# ifndef USE_IPHONE

  p->ogl_ctx = [[NSOpenGLContext alloc]
                initWithFormat:w->window.pixfmt
                shareContext:w->ogl_ctx];
  CFRetain (p->ogl_ctx);

  [p->ogl_ctx makeCurrentContext]; // This is indeed necessary.

  p->pixmap.gl_pbuffer = [[NSOpenGLPixelBuffer alloc]
                          /* TODO: Only if there are rectangluar textures. */
                          initWithTextureTarget:GL_TEXTURE_RECTANGLE_EXT
                          /* TODO: Make sure GL_RGBA isn't better. */
                          textureInternalFormat:GL_RGB
                          textureMaxMipMapLevel:0
                          pixelsWide:width
                          pixelsHigh:height];
  CFRetain (p->pixmap.gl_pbuffer);

  [p->ogl_ctx setPixelBuffer:p->pixmap.gl_pbuffer
                 cubeMapFace:0
                 mipMapLevel:0
        currentVirtualScreen:w->window.virtual_screen];

# else // USE_IPHONE

  p->ogl_ctx = w->window.ogl_ctx_pixmap;

  [EAGLContext setCurrentContext:p->ogl_ctx];
  create_framebuffer (&p->gl_framebuffer, &p->gl_renderbuffer);

  glRenderbufferStorageOES (GL_RENDERBUFFER_OES, GL_RGBA8_OES, width, height);
  glFramebufferRenderbufferOES (GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
                                GL_RENDERBUFFER_OES, p->gl_renderbuffer);

  check_framebuffer_status ();

  glBindFramebufferOES(GL_FRAMEBUFFER_OES, p->gl_framebuffer);

# endif // USE_IPHONE

  w->window.current_drawable = p;
  glViewport (0, 0, width, height);
  jwxyz_set_matrices (width, height);

# ifndef __OPTIMIZE__
  glClearColor (frand(1), frand(1), frand(1), 0);
  glClear (GL_COLOR_BUFFER_BIT);
# endif

  return p;
}

int
XFreePixmap (Display *d, Pixmap p)
{
  Assert (p && p->type == PIXMAP, "not a pixmap");

  Window w = RootWindow (d, 0);

# ifndef USE_IPHONE
  CFRelease (p->ogl_ctx);
  [p->ogl_ctx release];

  CFRelease (p->pixmap.gl_pbuffer);
  [p->pixmap.gl_pbuffer release];
# else  // USE_IPHONE
  glDeleteRenderbuffersOES (1, &p->gl_renderbuffer);
  glDeleteFramebuffersOES (1, &p->gl_framebuffer);
# endif // USE_IPHONE

  if (w->window.current_drawable == p) {
    w->window.current_drawable = NULL;
  }

  free (p);
  return 0;
}

#endif // JWXYZ_GL
