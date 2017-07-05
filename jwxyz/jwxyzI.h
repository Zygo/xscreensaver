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

#ifndef __JWXYZ_I_H__
#define __JWXYZ_I_H__

#include <inttypes.h>

#include "jwxyz.h"

#define Assert(C, ...) do { if (!(C)) jwxyz_abort (__VA_ARGS__); } while(0)

#undef MAX
#undef MIN
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define JWXYZ_FONT_FAMILY       0 /* i.e. -[NSFont familyName] via XLFD */
#define JWXYZ_FONT_FACE         1 /*      -[NSFont fontName]   via native */
#define JWXYZ_FONT_RANDOM       2

#define JWXYZ_STYLE_BOLD        1
#define JWXYZ_STYLE_ITALIC      2
#define JWXYZ_STYLE_MONOSPACE   4

/* jwxyz.m, jwxyz-gl.c */
extern Display *jwxyz_make_display (Window w);
extern void jwxyz_free_display (Display *);
extern void jwxyz_window_resized (Display *);
extern uint32_t jwxyz_alloc_color (Display *dpy, uint16_t r, uint16_t g,
                                   uint16_t b, uint16_t a);
extern void jwxyz_query_color (Display *dpy, unsigned long pixel,
                               uint8_t *rgba);
extern unsigned long jwxyz_window_background (Display *);
extern int jwxyz_draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
                           unsigned int width, unsigned int height,
                           int angle1, int angle2, Bool fill_p);
extern void jwxyz_fill_rects (Display *dpy, Drawable d, GC gc,
                              const XRectangle *rectangles,
                              unsigned long nrects,
                              unsigned long pixel);
extern XGCValues *jwxyz_gc_gcv (GC gc);
extern unsigned int jwxyz_gc_depth (GC gc);
extern int jwxyz_draw_string (Display *dpy, Drawable d, GC gc, int x, int y,
                              const char *str, size_t len, Bool utf8);

/* jwxyz-cocoa.m, jwxyz-android.c */
extern const XRectangle *jwxyz_frame (Drawable d); /* XGetGeometry sux. */
extern unsigned int jwxyz_drawable_depth (Drawable d);

/* 'scale' is pixels per point, with 72 points per inch. This matches the
   meaning of -[UIView contentScaleFactor] on iOS.
 */
extern float jwxyz_scale (Window main_window);

extern const char *jwxyz_default_font_family (int require);
extern void *jwxyz_load_native_font (Window main_window,
                                     int traits_jwxyz, int mask_jwxyz,
                                     const char *font_name_ptr,
                                     size_t font_name_length,
                                     int font_name_type, float size,
                                     char **family_name_ret,
                                     int *ascent_ret, int *descent_ret);
extern void jwxyz_release_native_font (Display *, void *native_font);
extern void jwxyz_render_text (Display *, void *native_font,
                               const char *str, size_t len, int utf8_p,
                               XCharStruct *cs_ret, char **pixmap_ret);
extern void jwxyz_get_pos (Window w, XPoint *vpos, XPoint *p);
extern void jwxyz_copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
                             int src_x, int src_y,
                             unsigned int width, unsigned int height,
                             int dst_x, int dst_y);
#ifndef current_device_rotation
extern double current_device_rotation (void);
extern Bool ignore_rotation_p (Display *);
#endif

/* jwxyz-common.c */
extern void jwxyz_validate_pixel (Display *dpy, unsigned long pixel,
                                  unsigned int depth, Bool alpha_allowed_p);
extern Bool jwxyz_dumb_drawing_mode(Display *dpy, Drawable d, GC gc,
                                    int x, int y,
                                    unsigned width, unsigned height);
extern void jwxyz_fill_rect (Display *, Drawable, GC,
                             int x, int y,
                             unsigned int width, unsigned int height,
                             unsigned long pixel);
extern void jwxyz_gcv_defaults (Display *dpy, XGCValues *gcv, int depth);
extern void *jwxyz_native_font (Font f);

# if defined JWXYZ_QUARTZ

#  include <CoreGraphics/CGGeometry.h>
#  include <CoreGraphics/CGContext.h>

extern void jwxyz_flush_context (Display *);

#  define jwxyz_assert_display(dpy)
#  define jwxyz_assert_drawable(main_window, d)
#  define jwxyz_assert_gl()

/* jwxyz-cocoa.m needs these from jwxyz.m for XCopyArea. */
extern void invalidate_drawable_cache (Drawable d);
extern void set_color (Display *dpy, CGContextRef cgc, unsigned long argb,
                       unsigned int depth, Bool alpha_allowed_p, Bool fill_p);
extern void push_gc (Drawable d, GC gc);
extern void push_color_gc (Display *dpy, Drawable d, GC gc,
                           unsigned long color,
                           Bool antialias_p, Bool fill_p);
extern CGPoint map_point (Drawable d, int x, int y);

#define seek_xy(dst, dst_pitch, x, y) \
  ((void *)((char *)dst + dst_pitch * y + x * 4))

#define pop_gc(d,gc) CGContextRestoreGState (d->cgc)

# elif defined JWXYZ_GL

#  if defined(USE_IPHONE)
#   include <OpenGLES/ES1/gl.h>
#  elif defined(HAVE_COCOA)
#   include <OpenGL/gl.h>
#  elif defined(HAVE_ANDROID)
#   include <GLES/gl.h>
#  else
#   include <GL/gl.h>
#  endif

/* utils/jwxyz-gl.c */
extern void jwxyz_prepare_context (Display *dpy);
extern void jwxyz_set_matrices (Display *dpy, unsigned width, unsigned height,
                                Bool screen_p);

/* Only works if both drawables share OpenGL objects. OpenGL context sharing
   works on OS X, iOS, and some (but not all!) EGL implementations. This would
   also work with FBOs and one context for everything. Surprisingly slow and
   unreliable.
 */
extern void jwxyz_gl_copy_area_read_tex_image (Display *dpy,
                                               unsigned src_height,
                                               int src_x, int src_y,
                                               unsigned int width,
                                               unsigned int height,
                                               int dst_x, int dst_y);
extern void jwxyz_gl_copy_area_write_tex_image (Display *dpy, GC gc,
                                                int src_x, int src_y,
                                                unsigned int width,
                                                unsigned int height,
                                                int dst_x, int dst_y);
extern void jwxyz_gl_draw_image (GLenum target,
                                 unsigned int tex_w, unsigned int tex_h,
                                 int src_x, int src_y,
                                 unsigned int width, unsigned int height,
                                 int dst_x, int dst_y);

/* glReadPixels followed by glTexImage2D. This is terrible, so only use this
   if nothing else works.
 */
extern void jwxyz_gl_copy_area_read_pixels (Display *dpy, Drawable src,
                                            Drawable dst, GC gc,
                                            int src_x, int src_y,
                                            unsigned int width,
                                            unsigned int height,
                                            int dst_x, int dst_y);

/* Platform-specific */
extern void jwxyz_bind_drawable (Display *dpy, Window w, Drawable d);
extern void jwxyz_assert_display (Display *);
extern void jwxyz_assert_drawable (Window main_window, Drawable d);
extern void jwxyz_assert_gl (void);

# endif /* JWXYZ_GL */

#endif
