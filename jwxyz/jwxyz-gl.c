/* xscreensaver, Copyright (c) 1991-2018 Jamie Zawinski <jwz@jwz.org>
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

   This is the version of jwxyz for Android.  The version used by macOS
   and iOS is in jwxyz.m.
 */

/* Be advised, this is all very much a work in progress. */

/* There is probably no reason to ever implement indexed-color rendering here,
   even if many screenhacks still work with PseudoColor.
   - OpenGL ES 1.1 (Android, iOS) doesn't support indexed color.
   - macOS only provides indexed color via AGL (now deprecated), not
     NSOpenGLPixelFormat.
 */

/* TODO:
   - malloc error checking
   - Check max texture sizes for XGet/PutImage, XCopyArea.
   - Optional 5:5:5 16-bit color
*/

/* Techniques & notes:
   - iOS: OpenGL ES 2.0 isn't always available. Use OpenGL ES 1.1.
   - OS X: Drivers can go back to OpenGL 1.1 (GeForce 2 MX on 10.5.8).
   - Use stencil buffers (OpenGL 1.0+) for bitmap clipping masks.
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
   XCopyArea, XDrawString, XGetImage
   XCreatePixmap, XCreateGC, XCreateImage
   XPutPixel
   Check these, of course.
 */

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

#include "jwxyzI.h"
#include "jwxyz-timers.h"
#include "yarandom.h"
#include "utf8wc.h"
#include "xft.h"
#include "pow2.h"

#define countof(x) (sizeof((x))/sizeof((*x)))

union color_bytes {
  uint32_t pixel;
  uint8_t bytes[4];
};

// Use two textures: one for RGBA, one for luminance. Older Android doesn't
// seem to like it when textures change format.
enum {
  texture_rgba,
  texture_mono
};

struct jwxyz_Display {
  const struct jwxyz_vtbl *vtbl; // Must come first.

  Window main_window;
  GLenum pixel_format, pixel_type;
  Visual visual;
  struct jwxyz_sources_data *timers_data;

  Bool gl_texture_npot_p;
  /* Bool opengl_core_p */;
  GLenum gl_texture_target;
  
  GLuint textures[2]; // Also can work on the desktop.

  unsigned long window_background;

  int gc_function;
  Bool gc_alpha_allowed_p;
  int gc_clip_x_origin, gc_clip_y_origin;
  GLuint gc_clip_mask;

  // Alternately, there could be one queue per pixmap.
  size_t queue_size, queue_capacity;
  Drawable queue_drawable;
  GLint queue_mode;
  void *queue_vertex;
  uint32_t *queue_color;
  Bool queue_line_cap;
};

struct jwxyz_GC {
  XGCValues gcv;
  unsigned int depth;
  GLuint clip_mask;
  unsigned clip_mask_width, clip_mask_height;
};

struct jwxyz_linked_point {
    short x, y;
    linked_point *next;
};


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
  Assert (width, "no width");
  Assert (height, "no height");

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

#endif


static void
tex_parameters (Display *d, GLuint texture)
{
  // TODO: Check for (ARB|EXT|NV)_texture_rectangle. (All three are alike.)
  // Rectangle textures should be present on OS X with the following exceptions:
  // - Generic renderer on PowerPC OS X 10.4 and earlier
  // - ATI Rage 128
  glBindTexture (d->gl_texture_target, texture);
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
}

static void
tex_size (Display *dpy, unsigned *tex_w, unsigned *tex_h)
{
  if (!dpy->gl_texture_npot_p) {
    *tex_w = to_pow2(*tex_w);
    *tex_h = to_pow2(*tex_h);
  }
}

static void
tex_image (Display *dpy, GLenum internalformat,
           unsigned *tex_w, unsigned *tex_h, GLenum format, GLenum type,
           const void *data)
{
  unsigned w = *tex_w, h = *tex_h;
  tex_size (dpy, tex_w, tex_h);

  // TODO: Would using glTexSubImage2D exclusively be faster?
  if (*tex_w == w && *tex_h == h) {
    glTexImage2D (dpy->gl_texture_target, 0, internalformat, *tex_w, *tex_h,
                  0, format, type, data);
  } else {
    // TODO: Sampling the last row might be a problem if src_x != 0.
    glTexImage2D (dpy->gl_texture_target, 0, internalformat, *tex_w, *tex_h,
                  0, format, type, NULL);
    glTexSubImage2D (dpy->gl_texture_target, 0, 0, 0, w, h,
                     format, type, data);
  }
}


extern const struct jwxyz_vtbl gl_vtbl;

Display *
jwxyz_gl_make_display (Window w)
{
  Display *d = (Display *) calloc (1, sizeof(*d));
  d->vtbl = &gl_vtbl;

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
  d->pixel_format = GL_RGBA; /*
    gluCheckExtension ((const GLubyte *) "GL_APPLE_texture_format_BGRA8888",
                       extensions) ? GL_BGRA_EXT : GL_RGBA; */
  d->pixel_type = GL_UNSIGNED_BYTE;
  // See also OES_read_format.
# else  // !HAVE_JWZGLES
  if (gl_check_ver (&version, 1, 2) ||
      (gluCheckExtension ((const GLubyte *) "GL_EXT_bgra", extensions) &&
       gluCheckExtension ((const GLubyte *) "GL_APPLE_packed_pixels",
                          extensions))) {
    // APPLE_packed_pixels is only ever available on iOS, never Android.
    d->pixel_format = GL_BGRA_EXT;
    // Both Intel and PowerPC-era docs say to use GL_UNSIGNED_INT_8_8_8_8_REV.
    d->pixel_type = GL_UNSIGNED_INT_8_8_8_8_REV;
  } else {
    d->pixel_format = GL_RGBA;
    d->pixel_type = GL_UNSIGNED_BYTE;
  }
  // GL_ABGR_EXT/GL_UNSIGNED_BYTE is another possibilty that may have made more
  // sense on PowerPC.
# endif // !HAVE_JWZGLES

  // On really old systems, it would make sense to split textures
  // into subsections, to work around the maximum texture size.
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

  Visual *v = &d->visual;
  v->class      = TrueColor;
  if (d->pixel_format == GL_BGRA_EXT) {
    v->red_mask   = 0x00ff0000;
    v->green_mask = 0x0000ff00;
    v->blue_mask  = 0x000000ff;
    v->alpha_mask = 0xff000000;
  } else {
    Assert(d->pixel_format == GL_RGBA,
           "jwxyz_gl_make_display: Unknown pixel_format");
    unsigned long masks[4];
    for (unsigned i = 0; i != 4; ++i) {
      union color_bytes color;
      color.pixel = 0;
      color.bytes[i] = 0xff;
      masks[i] = color.pixel;
    }
    v->red_mask   = masks[0];
    v->green_mask = masks[1];
    v->blue_mask  = masks[2];
    v->alpha_mask = masks[3];
  }

  d->timers_data = jwxyz_sources_init (XtDisplayToApplicationContext (d));

  d->window_background = BlackPixel(d,0);

  d->main_window = w;

  {
    GLint max_texture_size, max_texture_units;
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv (GL_MAX_TEXTURE_UNITS, &max_texture_units);
    Log ("GL_MAX_TEXTURE_SIZE: %d, GL_MAX_TEXTURE_UNITS: %d\n",
         max_texture_size, max_texture_units);

    // OpenGL ES 1.1 and OpenGL 1.3 both promise at least 2 texture units:
    // OpenGL (R) ES Common/Common-Lite Profile Specification, Version 1.1.12 (Full Specification)
    // https://www.khronos.org/registry/OpenGL/specs/es/1.1/es_full_spec_1.1.pdf
    // * Table 6.22. Implementation Dependent Values
    // * D.2 Enhanced Texture Processing
    // (OpenGL 1.2 provides multitexturing as an ARB extension, and requires 1
    // texture unit only.)

    // ...but the glGet reference page contradicts this, and says there can be
    // just one.
    // https://www.khronos.org/registry/OpenGL-Refpages/es1.1/xhtml/glGet.xml
  }

  glGenTextures (countof (d->textures), d->textures);

  for (unsigned i = 0; i != countof (d->textures); i++) {
    tex_parameters (d, d->textures[i]);
  }

  d->gc_function = GXcopy;
  d->gc_alpha_allowed_p = False;
  d->gc_clip_mask = 0;

  jwxyz_assert_display(d);
  return d;
}

void
jwxyz_gl_free_display (Display *dpy)
{
  Assert (dpy->vtbl == &gl_vtbl, "jwxyz-gl.c: bad vtable");

  /* TODO: Go over everything. */

  free (dpy->queue_vertex);
  free (dpy->queue_color);

  jwxyz_sources_free (dpy->timers_data);

  free (dpy);
}


/* Call this when the View changes size or position.
 */
void
jwxyz_window_resized (Display *dpy)
{
  Assert (dpy->vtbl == &gl_vtbl, "jwxyz-gl.c: bad vtable");

  const XRectangle *new_frame = jwxyz_frame (dpy->main_window);
  unsigned new_width = new_frame->width;
  unsigned new_height = new_frame->height;

  Assert (new_width, "jwxyz_window_resized: No width.");
  Assert (new_height, "jwxyz_window_resized: No height.");

/*if (cgc) w->cgc = cgc;
  Assert (w->cgc, "no CGContext"); */

  Log("resize: %d, %d\n", new_width, new_height);

  jwxyz_gl_flush (dpy);
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
}


static jwxyz_sources_data *
display_sources_data (Display *dpy)
{
  return dpy->timers_data;
}


static Window
root (Display *dpy)
{
  return dpy->main_window;
}

static Visual *
visual (Display *dpy)
{
  return &dpy->visual;
}


/* GC attributes by usage and OpenGL implementation:

   All drawing functions:
   function                                | glLogicOp w/ GL_COLOR_LOGIC_OP
   clip_x_origin, clip_y_origin, clip_mask | Multitexturing w/ GL_TEXTURE1

   Shape drawing functions:
   foreground, background                  | glColor*

   XDrawLines, XDrawRectangles, XDrawSegments:
   line_width, cap_style, join_style       | Lotsa vertices

   XFillPolygon:
   fill_rule                               | Multiple GL_TRIANGLE_FANs

   XDrawText:
   font                                    | Cocoa, then OpenGL display lists.

   alpha_allowed_p                         | GL_BLEND

   antialias_p                             | Well, there's options:
   * Multisampling would work, but that's something that would need to be set
     per-Pixmap, not per-GC.
   * GL_POINT, LINE, and POLYGON_SMOOTH are the old-school way of doing
     this, but POINT_SMOOTH is unnecessary, and POLYGON_SMOOTH is missing from
     GLES 1. All three are missing from GLES 2. Word on the street is that
     these are deprecated anyway.
   * Tiny textures with bilinear filtering to get the same effect as LINE_ and
     POLYGON_SMOOTH. A bit tricky.
   * Do nothing. Android hardware is very often high-DPI enough that
     anti-aliasing doesn't matter all that much.

   Nothing, really:
   subwindow_mode
 */

static void *
enqueue (Display *dpy, Drawable d, GC gc, int mode, size_t count,
         unsigned long pixel)
{
  if (dpy->queue_size &&
      (!gc || /* Could allow NULL GCs here... */
       dpy->gc_function != gc->gcv.function ||
       dpy->gc_alpha_allowed_p != gc->gcv.alpha_allowed_p ||
       dpy->gc_clip_mask != gc->clip_mask ||
       dpy->gc_clip_x_origin != gc->gcv.clip_x_origin ||
       dpy->gc_clip_y_origin != gc->gcv.clip_y_origin ||
       dpy->queue_mode != mode ||
       dpy->queue_drawable != d)) {
    jwxyz_gl_flush (dpy);
  }

  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  jwxyz_gl_set_gc (dpy, gc);

  if (mode == GL_TRIANGLE_STRIP)
    Assert (count, "empty triangle strip");
  // Use degenerate triangles to cut down on draw calls.
  Bool prepend2 = mode == GL_TRIANGLE_STRIP && dpy->queue_size;

  // ####: Primitive restarts should be used here when (if) they're available.
  if (prepend2)
    count += 2;

  // TODO: Use glColor when we can get away with it.
  size_t old_size = dpy->queue_size;
  dpy->queue_size += count;
  if (dpy->queue_size > dpy->queue_capacity) {
    dpy->queue_capacity = dpy->queue_size * 2;

    uint32_t *new_color = realloc (
      dpy->queue_color, sizeof(*dpy->queue_color) * dpy->queue_capacity);
    /* Allocate vertices as if they were always GLfloats. Otherwise, if
       queue_vertex is allocated to hold GLshorts, then things get switched
       to GLfloats, queue_vertex would be too small for the given capacity.
     */
    GLshort *new_vertex = realloc (
      dpy->queue_vertex, sizeof(GLfloat) * 2 * dpy->queue_capacity);

    if (!new_color || !new_vertex)
      return NULL;

    dpy->queue_color = new_color;
    dpy->queue_vertex = new_vertex;
  }

  dpy->queue_mode = mode;
  dpy->queue_drawable = d;

  union color_bytes color;

  // Like query_color, but for bytes.
  jwxyz_validate_pixel (dpy, pixel, jwxyz_drawable_depth (d),
                        gc ? gc->gcv.alpha_allowed_p : False);

  if (jwxyz_drawable_depth (d) == 1) {
    uint8_t b = pixel ? 0xff : 0;
    color.bytes[0] = b;
    color.bytes[1] = b;
    color.bytes[2] = b;
    color.bytes[3] = 0xff;
  } else {
    JWXYZ_QUERY_COLOR (dpy, pixel, 0xffull, color.bytes);
  }

  for (size_t i = 0; i != count; ++i) // TODO: wmemset when applicable.
    dpy->queue_color[i + old_size] = color.pixel;

  void *result = (char *)dpy->queue_vertex + old_size * 2 *
    (mode == GL_TRIANGLE_STRIP ? sizeof(GLfloat) : sizeof(GLshort));
  if (prepend2) {
    dpy->queue_color[old_size] = dpy->queue_color[old_size - 1];
    result = (GLfloat *)result + 4;
  }
  return result;
}


static void
finish_triangle_strip (Display *dpy, GLfloat *enqueue_result)
{
  if (enqueue_result != dpy->queue_vertex) {
    enqueue_result[-4] = enqueue_result[-6];
    enqueue_result[-3] = enqueue_result[-5];
    enqueue_result[-2] = enqueue_result[0];
    enqueue_result[-1] = enqueue_result[1];
  }
}


static void
query_color (Display *dpy, unsigned long pixel, unsigned int depth,
             Bool alpha_allowed_p, GLfloat *rgba)
{
  jwxyz_validate_pixel (dpy, pixel, depth, alpha_allowed_p);

  if (depth == 1) {
    GLfloat f = pixel;
    rgba[0] = f;
    rgba[1] = f;
    rgba[2] = f;
    rgba[3] = 1;
  } else {
    JWXYZ_QUERY_COLOR (dpy, pixel, 1.0f, rgba);
  }
}


static void
set_color (Display *dpy, unsigned long pixel, unsigned int depth,
           Bool alpha_allowed_p)
{
  GLfloat rgba[4];
  query_color (dpy, pixel, depth, alpha_allowed_p, rgba);
  glColor4f (rgba[0], rgba[1], rgba[2], rgba[3]);
}

/* Pushes a GC context; sets Function and ClipMask. */
void
jwxyz_gl_set_gc (Display *dpy, GC gc)
{
  int function;
  Bool alpha_allowed_p;
  GLuint clip_mask;

  // GC is NULL for XClearArea and XClearWindow.
  if (gc) {
    function = gc->gcv.function;
    alpha_allowed_p = gc->gcv.alpha_allowed_p || gc->clip_mask;
    clip_mask = gc->clip_mask;
  } else {
    function = GXcopy;
    alpha_allowed_p = False;
    clip_mask = 0;
  }

  /* GL_COLOR_LOGIC_OP: OpenGL 1.1. */
  if (function != dpy->gc_function) {
    dpy->gc_function = function;
    if (function != GXcopy) {
      /* Fun fact: The glLogicOp opcode constants are the same as the X11 GX*
         function constants | GL_CLEAR.
       */
      glEnable (GL_COLOR_LOGIC_OP);
      glLogicOp (gc->gcv.function | GL_CLEAR);
    } else {
      glDisable (GL_COLOR_LOGIC_OP);
    }
  }

  /* Cocoa uses add/subtract/difference blending in place of logical ops.
     It looks nice, but implementing difference blending in OpenGL appears to
     require GL_KHR_blend_equation_advanced, and support for this is not
     widespread.
   */

  dpy->gc_alpha_allowed_p = alpha_allowed_p;
  if (alpha_allowed_p || clip_mask) {
    // TODO: Maybe move glBlendFunc to XCreatePixmap?
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_BLEND);
  } else {
    glDisable (GL_BLEND);
  }

  /* Texture units:
     GL_TEXTURE0: Texture for XPutImage/XCopyArea (if applicable)
     GL_TEXTURE1: Texture for clip masks (if applicable)
   */
  dpy->gc_clip_mask = clip_mask;

  glActiveTexture (GL_TEXTURE1);
  if (clip_mask) {
    glEnable (dpy->gl_texture_target);
    glBindTexture (dpy->gl_texture_target, gc->clip_mask);

    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
               alpha_allowed_p ? GL_MODULATE : GL_REPLACE);

    glMatrixMode (GL_TEXTURE);
    glLoadIdentity ();

    unsigned
      tex_w = gc->clip_mask_width + 2, tex_h = gc->clip_mask_height + 2;
    tex_size (dpy, &tex_w, &tex_h);

# ifndef HAVE_JWZGLES
    if (dpy->gl_texture_target == GL_TEXTURE_RECTANGLE_EXT)
    {
      glScalef (1, -1, 1);
    }
    else
# endif
    {
      glScalef (1.0f / tex_w, -1.0f / tex_h, 1);
    }

    glTranslatef (1 - gc->gcv.clip_x_origin,
                  1 - gc->gcv.clip_y_origin - (int)gc->clip_mask_height - 2,
                  0);
  } else {
    glDisable (dpy->gl_texture_target);
  }
  glActiveTexture (GL_TEXTURE0);
}


static void
set_color_gc (Display *dpy, Drawable d, GC gc, unsigned long color)
{
  jwxyz_gl_flush (dpy);
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  jwxyz_gl_set_gc (dpy, gc);

  unsigned int depth;

  if (gc) {
    depth = gc->depth;

    switch (gc->gcv.function) {
      case GXset:   color = (depth == 1 ? 1 : WhitePixel(dpy,0)); break;
      case GXclear: color = (depth == 1 ? 0 : BlackPixel(dpy,0)); break;
    }
  } else {
    depth = visual_depth (NULL, NULL);
  }

  set_color (dpy, color, depth, gc ? gc->gcv.alpha_allowed_p : False);
}

/* Pushes a GC context; sets color to the foreground color.
 */
static void
set_fg_gc (Display *dpy, Drawable d, GC gc)
{
  set_color_gc (dpy, d, gc, gc->gcv.foreground);
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

static int
DrawPoints (Display *dpy, Drawable d, GC gc,
            XPoint *points, int count, int mode)
{
  short v[2] = {0, 0};

  // TODO: XPoints can be fed directly to OpenGL.
  GLshort *gl_points = enqueue (dpy, d, gc, GL_POINTS, count,
                                gc->gcv.foreground); // TODO: enqueue returns NULL.
  for (unsigned i = 0; i < count; i++) {
    next_point (v, points[i], mode);
    gl_points[2 * i] = v[0];
    gl_points[2 * i + 1] = v[1];
  }

  return 0;
}


static GLint
texture_internalformat (Display *dpy)
{
#ifdef HAVE_JWZGLES
  return dpy->pixel_format;
#else
  return GL_RGBA;
#endif
}

static GLenum
gl_pixel_type (const Display *dpy)
{
  return dpy->pixel_type;
}

static void
clear_texture (Display *dpy)
{
  glTexImage2D (dpy->gl_texture_target, 0, texture_internalformat(dpy), 0, 0,
                0, dpy->pixel_format, gl_pixel_type (dpy), NULL);
}


static void
vertex_pointer (Display *dpy, GLenum type, GLsizei stride,
                const void *pointer)
{
  glVertexPointer(2, type, stride, pointer);
  if (dpy->gc_clip_mask) {
    glClientActiveTexture (GL_TEXTURE1);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer (2, type, stride, pointer);
    glClientActiveTexture (GL_TEXTURE0);
  }
}


void
jwxyz_gl_flush (Display *dpy)
{
  Assert (dpy->vtbl == &gl_vtbl, "jwxyz-gl.c: bad vtable");

  if (!dpy->queue_size)
    return;

  // jwxyz_bind_drawable() and jwxyz_gl_set_gc() is called in enqueue().

  glEnableClientState (GL_COLOR_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);

  // TODO: Use glColor instead of glColorPointer if there's just one color.
  // TODO: Does OpenGL use both GL_COLOR_ARRAY and glColor at the same time?
  //       (Probably not.)
  glColor4f (1, 1, 1, 1);

  Bool shifted = dpy->queue_mode == GL_POINTS || dpy->queue_mode == GL_LINES;
  if (shifted) {
    glMatrixMode (GL_MODELVIEW);
    glTranslatef (0.5, 0.5, 0);
  }

  glColorPointer (4, GL_UNSIGNED_BYTE, 0, dpy->queue_color);
  vertex_pointer (dpy,
                  dpy->queue_mode == GL_TRIANGLE_STRIP ? GL_FLOAT : GL_SHORT,
                  0, dpy->queue_vertex);
  glDrawArrays (dpy->queue_mode, 0, dpy->queue_size);

  // TODO: This is right, right?
  if (dpy->queue_mode == GL_LINES && dpy->queue_line_cap) {
    Assert (!(dpy->queue_size % 2), "bad count for GL_LINES");
    glColorPointer (4, GL_UNSIGNED_BYTE, sizeof(GLubyte) * 8,
                    dpy->queue_color);
    vertex_pointer (dpy, GL_SHORT, sizeof(GLshort) * 4,
                    (GLshort *)dpy->queue_vertex + 2);
    glDrawArrays (GL_POINTS, 0, dpy->queue_size / 2);
  }

  if (shifted)
    glLoadIdentity ();

  glDisableClientState (GL_COLOR_ARRAY);
  glDisableClientState (GL_VERTEX_ARRAY);

  dpy->queue_size = 0;
}


void
jwxyz_gl_copy_area_read_tex_image (Display *dpy, unsigned src_height,
                                   int src_x, int src_y,
                                   unsigned int width, unsigned int height,
                                   int dst_x, int dst_y)
{
#  if defined HAVE_COCOA && !defined USE_IPHONE
  /* TODO: Does this help? */
  /* glFinish(); */
#  endif

  /* TODO: Fix TestX11 + mode_preserve with this one. */

  unsigned tex_w = width, tex_h = height;
  if (!dpy->gl_texture_npot_p) {
    tex_w = to_pow2(tex_w);
    tex_h = to_pow2(tex_h);
  }

  GLint internalformat = texture_internalformat(dpy);

  /* TODO: This probably shouldn't always be texture_rgba. */
  glBindTexture (dpy->gl_texture_target, dpy->textures[texture_rgba]);

  if (tex_w == width && tex_h == height) {
    glCopyTexImage2D (dpy->gl_texture_target, 0, internalformat,
                      src_x, src_height - src_y - height, width, height, 0);
  } else {
    glTexImage2D (dpy->gl_texture_target, 0, internalformat, tex_w, tex_h,
                  0, dpy->pixel_format, gl_pixel_type(dpy), NULL);
    glCopyTexSubImage2D (dpy->gl_texture_target, 0, 0, 0,
                         src_x, src_height - src_y - height, width, height);
  }
}

void
jwxyz_gl_copy_area_write_tex_image (Display *dpy, GC gc, int src_x, int src_y,
                                    unsigned int width, unsigned int height,
                                    int dst_x, int dst_y)
{
  jwxyz_gl_set_gc (dpy, gc);

  /* TODO: Copy-pasted from read_tex_image. */
  unsigned tex_w = width, tex_h = height;
  if (!dpy->gl_texture_npot_p) {
    tex_w = to_pow2(tex_w);
    tex_h = to_pow2(tex_h);
  }

  /* Must match what's in jwxyz_gl_copy_area_read_tex_image. */
  glBindTexture (dpy->gl_texture_target, dpy->textures[texture_rgba]);

  jwxyz_gl_draw_image (dpy, gc, dpy->gl_texture_target, tex_w, tex_h,
                       0, 0, gc->depth, width, height, dst_x, dst_y, False);

  clear_texture (dpy);
}


void
jwxyz_gl_draw_image (Display *dpy, GC gc, GLenum gl_texture_target,
                     unsigned int tex_w, unsigned int tex_h,
                     int src_x, int src_y, int src_depth,
                     unsigned int width, unsigned int height,
                     int dst_x, int dst_y, Bool flip_y)
{
  if (!gc || src_depth == gc->depth) {
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  } else {
    Assert (src_depth == 1 && gc->depth == 32,
            "jwxyz_gl_draw_image: bad depths");

    set_color (dpy, gc->gcv.background, gc->depth, gc->gcv.alpha_allowed_p);

    GLfloat rgba[4];
    query_color (dpy, gc->gcv.foreground, gc->depth, gc->gcv.alpha_allowed_p,
                 rgba);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, rgba);
  }

  glEnable (gl_texture_target);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);

  Assert (!glIsEnabled (GL_COLOR_ARRAY), "glIsEnabled (GL_COLOR_ARRAY)");
  Assert (!glIsEnabled (GL_NORMAL_ARRAY), "glIsEnabled (GL_NORMAL_ARRAY)");

  /* TODO: EXT_draw_texture or whatever it's called. */
  GLfloat vertices[4][2] = {
    {dst_x, dst_y},
    {dst_x, dst_y + height},
    {dst_x + width, dst_y + height},
    {dst_x + width, dst_y}
  };

  GLfloat
    tex_x0 = src_x, tex_y0 = src_y,
    tex_x1 = src_x + width, tex_y1 = src_y;

  if (flip_y)
    tex_y1 += height;
  else
    tex_y0 += height;

# ifndef HAVE_JWZGLES
  if (gl_texture_target != GL_TEXTURE_RECTANGLE_EXT)
# endif
  {
    GLfloat mx = 1.0f / tex_w, my = 1.0f / tex_h;
    tex_x0 *= mx;
    tex_y0 *= my;
    tex_x1 *= mx;
    tex_y1 *= my;
  }

  GLfloat tex_coords[4][2] = {
    {tex_x0, tex_y0},
    {tex_x0, tex_y1},
    {tex_x1, tex_y1},
    {tex_x1, tex_y0}
  };

  vertex_pointer (dpy, GL_FLOAT, 0, vertices);
  glTexCoordPointer (2, GL_FLOAT, 0, tex_coords);
  glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

//clear_texture();
  glDisable (gl_texture_target);
}

#if 0
void
jwxyz_gl_copy_area_read_pixels (Display *dpy, Drawable src, Drawable dst,
                                GC gc, int src_x, int src_y,
                                unsigned int width, unsigned int height,
                                int dst_x, int dst_y)
{
  XImage *img = XGetImage (dpy, src, src_x, src_y, width, height, ~0, ZPixmap);
  XPutImage (dpy, dst, gc, img, 0, 0, dst_x, dst_y, width, height);
  XDestroyImage (img);
}
#endif


static int
DrawLines (Display *dpy, Drawable d, GC gc, XPoint *points, int count,
           int mode)
{
  set_fg_gc (dpy, d, gc);

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
  vertex_pointer (dpy, GL_SHORT, 0, vertices);
  glDrawArrays (GL_LINE_STRIP, 0, count);
  
  free (vertices);

  if (gc->gcv.cap_style != CapNotLast) {
    // TODO: How does this look with multisampling?
    // TODO: Disable me for closed loops.
    vertex_pointer (dpy, GL_SHORT, 0, p);
    glDrawArrays (GL_POINTS, 0, 1);
  }

  glLoadIdentity ();
  
  return 0;
}


// Turn line segment into parallelogram based on line_width
//
// TODO: Fix epicycle hack with large thickness, and truchet line segment ends
//
static void
drawThickLine (Display *dpy, Drawable d, GC gc, int line_width,
               XSegment *segments)
{
    double dx, dy, di, m, angle;
    int sx1, sx2, sy1, sy2;

    sx1 = segments->x1;
    sy1 = segments->y1;
    sx2 = segments->x2;
    sy2 = segments->y2;

    dx = sx1 - sx2;
    dy = sy1 - sy2;
    di = sqrt(dx * dx + dy * dy);
    dx = dx / di;
    dy = dy / di;
    m = dy / dx;

    angle = atan(m); 

    float sn = sin(angle);
    float cs = cos(angle);
    float line_width_f = (float) line_width;

    float wsn = line_width_f * (sn/2);
    float csn = line_width_f * (cs/2);

    float x3 = sx1 - wsn;
    float y3 = sy1 + csn;
    float x4 = sx1 + wsn;
    float y4 = sy1 - csn;

    float x5 = sx2 - wsn;
    float y5 = sy2 + csn;
    float x6 = sx2 + wsn;
    float y6 = sy2 - csn;

    GLfloat *coords = enqueue (dpy, d, gc, GL_TRIANGLE_STRIP, 4,
                               gc->gcv.foreground);
    coords[0] = x3;
    coords[1] = y3;
    coords[2] = x4;
    coords[3] = y4;
    coords[4] = x5;
    coords[5] = y5;
    coords[6] = x6;
    coords[7] = y6;
    finish_triangle_strip (dpy, coords);
}


static int
DrawSegments (Display *dpy, Drawable d, GC gc, XSegment *segments, int count)
{
  /* TODO: Caps on thick lines. */
  /* Thin lines <= 1px are offset by +0.5; thick lines are not. */

  if (count == 1 && gc->gcv.line_width > 1) {
    drawThickLine (dpy, d, gc, gc->gcv.line_width, segments);
  }
  else {
    if (dpy->queue_line_cap != (gc->gcv.cap_style != CapNotLast))
      jwxyz_gl_flush (dpy);
    dpy->queue_line_cap = gc->gcv.cap_style != CapNotLast;

    // TODO: Static assert here.
    Assert (sizeof(XSegment) == sizeof(short) * 4 &&
            sizeof(GLshort) == sizeof(short) &&
            offsetof(XSegment, x1) == 0 &&
            offsetof(XSegment, x2) == 4,
            "XDrawSegments: Data alignment mix-up.");

    memcpy (enqueue(dpy, d, gc, GL_LINES, count * 2, gc->gcv.foreground),
            segments, count * sizeof(XSegment));
  }

  return 0;
}


static int
ClearWindow (Display *dpy, Window win)
{
  Assert (win == dpy->main_window, "not a window");

  jwxyz_gl_flush (dpy);
  jwxyz_bind_drawable (dpy, win, win);

  GLfloat color[4];
  JWXYZ_QUERY_COLOR (dpy, dpy->window_background, 1.0f, color);

  glClearColor (color[0], color[1], color[2], 1);
  glClear (GL_COLOR_BUFFER_BIT);
  return True;
}

static unsigned long *
window_background (Display *dpy)
{
  return &dpy->window_background;
}

static void
fill_rects (Display *dpy, Drawable d, GC gc,
            const XRectangle *rectangles, unsigned long nrectangles,
            unsigned long pixel)
{
  for (unsigned long i = 0; i != nrectangles; ++i) {
    const XRectangle *r = &rectangles[i];
    GLfloat *coords = enqueue (dpy, d, gc, GL_TRIANGLE_STRIP, 4, pixel);
    coords[0] = r->x;
    coords[1] = r->y;
    coords[2] = r->x;
    coords[3] = r->y + r->height;
    coords[4] = r->x + r->width;
    coords[5] = r->y;
    coords[6] = r->x + r->width;
    coords[7] = r->y + r->height;
    finish_triangle_strip (dpy, coords);
  }
}


static int
FillPolygon (Display *dpy, Drawable d, GC gc,
             XPoint *points, int npoints, int shape, int mode)
{
  set_fg_gc(dpy, d, gc);
  
  // TODO: Re-implement the GLU tesselation functions.

  /* Complex: Pedal, and for some reason Attraction, Mountain, Qix, SpeedMine, Starfish
   * Nonconvex: Goop, Pacman, Rocks, Speedmine
   *
   * We currently do Nonconvex with the simple-to-implement ear clipping
   * algorithm, but in the future we can replace that with an algorithm
   * with slower big-O growth
   *
   */
  
  
  // TODO: Feed vertices straight to OpenGL for CoordModeOrigin.

  if (shape == Convex) {

    GLshort *vertices = malloc(npoints * sizeof(GLshort) * 2); // TODO: Oh look, another unchecked malloc.
    short v[2] = {0, 0};
  
    for (unsigned i = 0; i < npoints; i++) {
      next_point(v, points[i], mode);
      vertices[2 * i] = v[0];
      vertices[2 * i + 1] = v[1];
    }

    glEnableClientState (GL_VERTEX_ARRAY);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    vertex_pointer (dpy, GL_SHORT, 0, vertices);
    glDrawArrays (GL_TRIANGLE_FAN, 0, npoints);

    free(vertices);

  } else if (shape == Nonconvex) {

    // TODO: assert that x,y of first and last point match, as that is assumed

    linked_point *root;
    root = (linked_point *) malloc( sizeof(linked_point) );
    set_points_list(points,npoints,root);
    traverse_points_list(dpy, root);

  } else {
    Assert((shape == Convex || shape == Nonconvex),
           "XFillPolygon: (TODO) Unimplemented shape");
  }

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

/* TODO: Fill in arcs with line width > 1 */
static int
draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
          unsigned int width, unsigned int height,
          int angle1, int angle2, Bool fill_p)
{
    int gglw = gc->gcv.line_width;

    if (fill_p || gglw <= 1) {
        draw_arc_gl (dpy, d, gc, x, y, width, height, angle1, angle2, fill_p);
    }
    else {
        int w1, w2, h1, h2, gglwh;
        w1 = width + gglw;
        h1 = height + gglw;
        h2 = height - gglw;
        w2 = width - gglw;
        gglwh = gglw / 2;
        int x1 = x - gglwh;
        int x2 = x + gglwh;
        int y1 = y - gglwh;
        int y2 = y + gglwh;
        //draw_arc_gl (dpy, d, gc, x, y, width, height, angle1, angle2, fill_p);
        draw_arc_gl (dpy, d, gc, x1, y1, w1, h1, angle1, angle2, fill_p);
        draw_arc_gl (dpy, d, gc, x2, y2, w2, h2, angle1, angle2, fill_p);
    }
    return 0;
}


int
draw_arc_gl (Display *dpy, Drawable d, GC gc, int x, int y,
          unsigned int width, unsigned int height,
          int angle1, int angle2, Bool fill_p)
{
  set_fg_gc(dpy, d, gc);

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
  
  vertex_pointer (dpy, GL_FLOAT, 0, data);
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


static XGCValues *
gc_gcv (GC gc)
{
  return &gc->gcv;
}


static unsigned int
gc_depth (GC gc)
{
  return gc->depth;
}


static GC
CreateGC (Display *dpy, Drawable d, unsigned long mask, XGCValues *xgcv)
{
  struct jwxyz_GC *gc = (struct jwxyz_GC *) calloc (1, sizeof(*gc));
  gc->depth = jwxyz_drawable_depth (d);

  jwxyz_gcv_defaults (dpy, &gc->gcv, gc->depth);
  XChangeGC (dpy, gc, mask, xgcv);
  return gc;
}


static void
free_clip_mask (Display *dpy, GC gc)
{
  if (gc->gcv.clip_mask) {
    if (dpy->gc_clip_mask == gc->clip_mask) {
      jwxyz_gl_flush (dpy);
      dpy->gc_clip_mask = 0;
    }
    glDeleteTextures (1, &gc->clip_mask);
  }
}


static int
FreeGC (Display *dpy, GC gc)
{
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);

  free_clip_mask (dpy, gc);
  free (gc);
  return 0;
}


static int
PutImage (Display *dpy, Drawable d, GC gc, XImage *ximage,
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

  jwxyz_gl_flush (dpy);
  jwxyz_bind_drawable (dpy, dpy->main_window, d);
  jwxyz_gl_set_gc (dpy, gc);

  int bpl = ximage->bytes_per_line;
  int bpp = ximage->bits_per_pixel;

  char *tex_data;
  unsigned src_w;
  GLint tex_internalformat;
  GLenum tex_format, tex_type;
  unsigned tex_index;

  if (bpp == 32) {
    tex_data = ximage->data + src_y * bpl + (src_x * 4);

    jwxyz_assert_display(dpy);
    
    /* There probably won't be any hacks that do this, but... */
    Assert (!(bpl % 4), "XPutImage: bytes_per_line not divisible by four.");
    
    tex_internalformat = texture_internalformat(dpy);
    tex_format = dpy->pixel_format;
    tex_type = gl_pixel_type(dpy);
    tex_index = texture_rgba;

    /* GL_UNPACK_ROW_LENGTH is not allowed to be negative. (sigh) */
# ifndef HAVE_JWZGLES
    src_w = w;
    glPixelStorei (GL_UNPACK_ROW_LENGTH, src_w);
# else
    src_w = bpl / 4;
# endif

    // glPixelStorei (GL_UNPACK_ALIGNMENT, 4); // Probably unnecessary.
  } else {
    Assert (bpp == 1, "expected 1 or 32 bpp");
    Assert ((src_x % 8) == 0,
            "XPutImage with non-byte-aligned 1bpp X offset not implemented");

    const char *src_data = ximage->data + src_y * bpl + (src_x / 8);
    unsigned w8 = (w + 7) / 8;

    src_w = w8 * 8;

    tex_data = malloc(src_w * h);

#if 0
    {
      char s[10240];
      int x, y, o;
      Log("#PI ---------- %d %d %08lx %08lx",
          jwxyz_drawable_depth(d), ximage->depth,
          (unsigned long)d, (unsigned long)ximage);
      for (y = 0; y < ximage->height; y++) {
        o = 0;
        for (x = 0; x < ximage->width; x++) {
          unsigned long b = XGetPixel(ximage, x, y);
          s[o++] = (   (b & 0xFF000000)
                    ? ((b & 0x00FFFFFF) ? '#' : '-')
                    : ((b & 0x00FFFFFF) ? '+' : ' '));
        }
        s[o] = 0;
        Log("#PI [%s]",s);
      }
      Log("# PI ----------");
    }
#endif
    uint32_t *data_out = (uint32_t *)tex_data;
    for(unsigned y = h; y; --y) {
      for(unsigned x = 0; x != w8; ++x) {
        // TODO: Does big endian work here?
        uint8_t byte = src_data[x];
        uint32_t word = byte;
        word = (word & 0x3) | ((word & 0xc) << 14);
        word = (word & 0x00010001) | ((word & 0x00020002) << 7);
        data_out[x << 1] = (word << 8) - word;

        word = byte >> 4;
        word = (word & 0x3) | ((word & 0xc) << 14);
        word = (word & 0x00010001) | ((word & 0x00020002) << 7);
        data_out[(x << 1) | 1] = (word << 8) - word;
      }
      src_data += bpl;
      data_out += src_w / 4;
    }
#if 0
    {
      char s[10240];
      int x, y, o;
      Log("#P2 ----------");
      for (y = 0; y < ximage->height; y++) {
        o = 0;
        for (x = 0; x < ximage->width; x++) {
          unsigned long b = ((uint8_t *)tex_data)[y * w + x];
          s[o++] = (   (b & 0xFF000000)
                    ? ((b & 0x00FFFFFF) ? '#' : '-')
                    : ((b & 0x00FFFFFF) ? '+' : ' '));
        }
        s[o] = 0;
        Log("#P2 [%s]",s);
      }
      Log("# P2 ----------");
    }
#endif

    tex_internalformat = GL_LUMINANCE;
    tex_format = GL_LUMINANCE;
    tex_type = GL_UNSIGNED_BYTE;
    tex_index = texture_mono;

    // glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  }

# if 1 // defined HAVE_JWZGLES
  // Regular OpenGL uses GL_TEXTURE_RECTANGLE_EXT in place of GL_TEXTURE_2D.
  // TODO: Make use of OES_draw_texture.

  unsigned tex_w = src_w, tex_h = h;
  glBindTexture (dpy->gl_texture_target, dpy->textures[tex_index]);

  // A fun project: reimplement xshm.c by means of a PBO using
  // GL_MAP_UNSYNCHRONIZED_BIT.

  tex_image (dpy, tex_internalformat, &tex_w, &tex_h, tex_format, tex_type,
             tex_data);

  if (bpp == 1)
    free(tex_data);

  jwxyz_gl_draw_image (dpy, gc, dpy->gl_texture_target, tex_w, tex_h,
                       0, 0, bpp, w, h, dest_x, dest_y, True);
# else
  glRasterPos2i (dest_x, dest_y);
  glPixelZoom (1, -1);
  jwxyz_assert_display (dpy);
  glDrawPixels (w, h, dpy->pixel_format, gl_pixel_type(dpy), data);
# endif

  jwxyz_assert_gl ();

  return 0;
}

/* At the moment only XGetImage and get_xshm_image use XGetSubImage. */
/* #### Twang calls XGetImage on the window intending to get a
   buffer full of black.  This is returning a buffer full of white
   instead of black for some reason. */
static XImage *
GetSubImage (Display *dpy, Drawable d, int x, int y,
             unsigned int width, unsigned int height,
             unsigned long plane_mask, int format,
             XImage *dest_image, int dest_x, int dest_y)
{
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  jwxyz_gl_flush (dpy);
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
                  dpy->pixel_format, gl_pixel_type(dpy), dest_data);

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

    uint32_t *rgba_image = malloc(width * height * 4);
    Assert (rgba_image, "not enough memory");

    // Must be GL_RGBA; GL_RED isn't available.
    glReadPixels (x, jwxyz_frame (d)->height - (y + height), width, height,
                  GL_RGBA, GL_UNSIGNED_BYTE, rgba_image);

    Assert (!(dest_x % 8), "XGetSubImage: dest_x not byte-aligned");
    uint8_t *top =
      (uint8_t *)dest_image->data + dest_image->bytes_per_line * dest_y
      + dest_x / 8;
#if 0
    {
      char s[10240];
      Log("#GI ---------- %d %d  %d x %d %08lx", 
          jwxyz_drawable_depth(d), dest_image->depth,
          width, height,
          (unsigned long) d);
      for (int y = 0; y < height; y++) {
        int x;
        for (x = 0; x < width; x++) {
          unsigned long b = rgba_image[(height-y-1) * width + x];
          s[x] = (   (b & 0xFF000000)
                  ? ((b & 0x00FFFFFF) ? '#' : '-')
                  : ((b & 0x00FFFFFF) ? '+' : ' '));
        }
        s[x] = 0;
        Log("#GI [%s]",s);
      }
      Log("#GI ----------");
    }
#endif
    const uint32_t *bottom = rgba_image + width * (height - 1);
    for (unsigned y = height; y; --y) {
      memset (top, 0, width / 8);
      for (unsigned x = 0; x != width; ++x) {
        if (bottom[x] & 0x80)
          top[x >> 3] |= (1 << (x & 7));
      }
      top += dest_image->bytes_per_line;
      bottom -= width;
    }

    free (rgba_image);
  }

  return dest_image;
}


static int
SetClipMask (Display *dpy, GC gc, Pixmap m)
{
  Assert (m != dpy->main_window, "not a pixmap");

  free_clip_mask (dpy, gc);

  if (!m) {
    gc->clip_mask = 0;
    return 0;
  }

  Assert (jwxyz_drawable_depth (m) == 1, "wrong depth for clip mask");

  const XRectangle *frame = jwxyz_frame (m);
  gc->clip_mask_width = frame->width;
  gc->clip_mask_height = frame->height;

  /* Apparently GL_ALPHA isn't a valid format for a texture used in an FBO:

    (86) Are any one- or two- component formats color-renderable?

            Presently none of the one- or two- component texture formats
            defined in unextended OpenGL is color-renderable.  The R
            and RG float formats defined by the NV_float_buffer
            extension are color-renderable.

            Although an early draft of the FBO specification permitted
            rendering into alpha, luminance, and intensity formats, this
            this capability was pulled when it was realized that it was
            under-specified exactly how rendering into these formats
            would work.  (specifically, how R/G/B/A map to I/L/A)

     <https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_framebuffer_object.txt>

     ...Therefore, 1-bit drawables get to have wasted color channels.
     Currently, R=G=B=grey, and the alpha channel is unused.
   */

  /* There doesn't seem to be any way to move what's on one of the color
     channels to the alpha channel in GLES 1.1 without leaving the GPU.
   */

  /* GLES 1.1 only supports GL_CLAMP_TO_EDGE or GL_REPEAT for
     GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T, so to prevent drawing outside
     the clip mask pixmap, there's two options:
     1. Use glScissor.
     2. Add a black border.

     Option #2 is in use here.

     The following converts in-place an RGBA image to alpha-only image with a
     1px black border.
   */

  // Prefix/suffix: 1 row+1 pixel, rounded to nearest int32.
  size_t pad0 = frame->width + 3, pad = (pad0 + 3) & ~3;
  uint8_t *data = malloc(2 * pad + frame->width * frame->height * 4);

  uint8_t *rgba = data + pad;
  uint8_t *alpha = rgba;
  uint8_t *alpha0 = alpha - pad0;
  memset (alpha0, 0, pad0); // Top row.

  jwxyz_gl_flush (dpy);
  jwxyz_bind_drawable (dpy, dpy->main_window, m);
  glReadPixels (0, 0, frame->width, frame->height, GL_RGBA, GL_UNSIGNED_BYTE,
                rgba);

  for (unsigned y = 0; y != frame->height; ++y) {
    for (unsigned x = 0; x != frame->width; ++x)
      alpha[x] = rgba[x * 4];

    rgba += frame->width * 4;

    alpha += frame->width;
    alpha[0] = 0;
    alpha[1] = 0;
    alpha += 2;
  }

  alpha -= 2;
  memset (alpha, 0, pad0); // Bottom row.

  glGenTextures (1, &gc->clip_mask);
  tex_parameters (dpy, gc->clip_mask);

  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

  unsigned tex_w = frame->width + 2, tex_h = frame->height + 2;
  tex_image (dpy, GL_ALPHA, &tex_w, &tex_h, GL_ALPHA, GL_UNSIGNED_BYTE,
             alpha0);

  free(data);

  return 0;
}

static int
SetClipOrigin (Display *dpy, GC gc, int x, int y)
{
  gc->gcv.clip_x_origin = x;
  gc->gcv.clip_y_origin = y;
  return 0;
}

void set_points_list(XPoint *points, int npoints, linked_point *root)
{
    linked_point *current;  

    current = root;
    for (int i = 0; i < npoints - 2 ; i++) {
        current->x = points[i].x;
        current->y = points[i].y;
        current->next = (linked_point *) malloc(sizeof(linked_point)); 
        current = current->next;
    }
    current->x = points[npoints-2].x;
    current->y = points[npoints-2].y;
    current->next = root;
}


double compute_edge_length(linked_point * a, linked_point * b)
{

    int xdiff, ydiff, xsq, ysq, added;
    double xy_add, edge_length;

    xdiff = a->x - b->x;
    ydiff = a->y - b->y;
    xsq = xdiff * xdiff;
    ysq = ydiff * ydiff;
    added = xsq + ysq;
    xy_add = (double) added;
    edge_length = sqrt(xy_add);
    return edge_length;
}

double get_angle(double a, double b, double c)
{
    double cos_a, i_cos_a;
    cos_a = (((b * b) + (c * c)) - (a * a)) / (double) (2.0 * b * c);
    i_cos_a = acos(cos_a);
    return i_cos_a;
}


Bool is_same_slope(linked_point * a)
{

    int abx, bcx, aby, bcy, aa, bb;
    linked_point *b;
    linked_point *c;

    b = a->next;
    c = b->next;

    // test if slopes are indefinite for both line segments
    if (a->x == b->x) {
        return b->x == c->x;
    } else if (b->x == c->x) {
        return False;   // false, as ax/bx is not indefinite
    }

    abx = a->x - b->x;
    bcx = b->x - c->x;
    aby = a->y - b->y;
    bcy = b->y - c->y;
    aa = abx * bcy;
    bb = bcx * aby;

    return aa == bb;
}

void draw_three_vertices(Display *dpy, linked_point * a, Bool triangle)
{

    linked_point *b;
    linked_point *c;
    GLenum drawType;

    b = a->next;
    c = b->next;

    GLfloat vertices[3][2] = {
        {a->x, a->y},
        {b->x, b->y},
        {c->x, c->y}
    };

    if (triangle) {
        drawType = GL_TRIANGLES;
    } else {
        drawType = GL_LINES;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    vertex_pointer(dpy, GL_FLOAT, 0, vertices);
    glDrawArrays(drawType, 0, 3);

    free(b);  // cut midpoint off from remaining polygon vertex list
    a->next = c;
}


Bool is_an_ear(linked_point * a)
{
    double edge_ab, edge_bc, edge_ac;
    double angle_a, angle_b, angle_c;
    double my_pi;
    linked_point *b, *c;

    b = a->next;
    c = b->next;
    my_pi = (double) M_PI;

    edge_ab = compute_edge_length(a, b);
    edge_bc = compute_edge_length(b, c);
    edge_ac = compute_edge_length(a, c);
    angle_a = get_angle(edge_bc, edge_ab, edge_ac);
    angle_b = get_angle(edge_ac, edge_ab, edge_bc);
    angle_c = get_angle(edge_ab, edge_ac, edge_bc);

    return angle_a < my_pi && angle_b < my_pi && angle_c < my_pi;
}


Bool is_three_point_loop(linked_point * head)
{
    return head->x == head->next->next->next->x
        && head->y == head->next->next->next->y;
}


void traverse_points_list(Display *dpy, linked_point * root)
{
    linked_point *head;
    head = root;

    while (!is_three_point_loop(head)) {
        if (is_an_ear(head)) {
            draw_three_vertices(dpy, head, True);
        } else if (is_same_slope(head)) {
            draw_three_vertices(dpy, head, False);
        } else {
            head = head->next;
        }
    }

    // handle final three vertices in polygon
    if (is_an_ear(head)) {
        draw_three_vertices(dpy, head, True);
    } else if (is_same_slope(head)) {
        draw_three_vertices(dpy, head, False);
    } else {
        free(head->next->next);
        free(head->next);
        free(head);
        Assert (False, "traverse_points_list: unknown configuration");
    }

    free(head->next);
    free(head);
}


const struct jwxyz_vtbl gl_vtbl = {
  root,
  visual,
  display_sources_data,

  window_background,
  draw_arc,
  fill_rects,
  gc_gcv,
  gc_depth,
  jwxyz_draw_string,

  jwxyz_gl_copy_area,

  DrawPoints,
  DrawSegments,
  CreateGC,
  FreeGC,
  ClearWindow,
  SetClipMask,
  SetClipOrigin,
  FillPolygon,
  DrawLines,
  PutImage,
  GetSubImage
};

#endif /* JWXYZ_GL -- entire file */
