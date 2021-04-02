/* xlockmore.h --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * See xlockmore.h and xlockmore.c.
 */

#ifndef __XLOCKMORE_INTERNAL_H__
#define __XLOCKMORE_INTERNAL_H__

#include <time.h>

#include "screenhackI.h"
#include "erase.h"


typedef struct ModeInfo ModeInfo;

/* Keep slots for these pointers in ModeInfo even if this header is
   included in a file that is not being compiled in GL-mode. so that
   sizeof(ModeInfo) doesn't vary.
 */
#if defined(HAVE_GL) && !defined(USE_GL)
# ifdef HAVE_JWZGLES
   typedef struct jwzgles_state jwzgles_state;
# endif
# ifdef HAVE_EGL
   typedef struct egl_data egl_data;
# elif !defined(HAVE_COCOA) && !defined(HAVE_ANDROID)
  typedef void *GLXContext;
# endif
#endif /* HAVE_GL && !USE_GL */

#ifdef HAVE_EGL
  typedef struct egl_data *GLXContext;
  typedef Drawable GLXDrawable;
#endif /* !HAVE_EGL */

#if defined(HAVE_EGL) && defined(USE_GL)
  typedef struct egl_data {
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;  /* Unused */
    EGLConfig  egl_config;   /* Unused */
  } egl_data;

  extern Bool glXMakeCurrent (Display *, GLXDrawable, GLXContext);
  extern void glXSwapBuffers (Display *, GLXDrawable);
#endif /* HAVE_EGL && USE_GL */


#ifdef USE_GL
  extern GLXContext *init_GL (ModeInfo *);
  extern void xlockmore_reset_gl_state(void);
  extern void clear_gl_error (void);
  extern void check_gl_error (const char *type);

  extern Visual *xlockmore_pick_gl_visual (Screen *);
  extern Bool xlockmore_validate_gl_visual (Screen *, const char *, Visual *);
#endif /* USE_GL */


/* These are only used in GL mode, but I don't understand why XCode
   isn't seeing the prototypes for them in glx/fps-gl.c... */
extern void do_fps (ModeInfo *);
extern void xlockmore_gl_compute_fps (Display *, Window, fps_state *, void *);
extern void xlockmore_gl_draw_fps (ModeInfo *);
extern void xlockmore_gl_free_fps (fps_state *);
# define do_fps xlockmore_gl_draw_fps


extern void xlockmore_setup (struct xscreensaver_function_table *, void *);
extern void xlockmore_do_fps (Display *, Window, fps_state *, void *);
extern void xlockmore_mi_init (ModeInfo *, size_t, void **);
extern Bool xlockmore_no_events (ModeInfo *, XEvent *);

/* The xlockmore RNG API is implemented in utils/yarandom.h. */


struct ModeInfo {
  struct xlockmore_function_table *xlmft;
  Display *dpy;
  Window window;
  Bool root_p;
  int screen_number;
  int npixels;
  unsigned long *pixels;
  XColor *colors;
  Bool writable_p;
  unsigned long white;
  unsigned long black;
  XWindowAttributes xgwa;
  GC gc;
  long pause;
  Bool fullrandom;
  long cycles;
  long batchcount;
  long size;
  Bool threed;
  long threed_left_color;
  long threed_right_color;
  long threed_both_color;
  long threed_none_color;
  long threed_delta;
  Bool wireframe_p;
  Bool is_drawn;
  eraser_state *eraser;
  Bool needs_clear;

  /* Used only by OpenGL programs, since FPS is tricky there. */
  fps_state *fpst;
  Bool fps_p;
  unsigned long polygon_count;  /* These values are for -fps display only */
  double recursion_depth;

# ifdef HAVE_GL
#  ifndef HAVE_JWXYZ
    GLXContext glx_context;  /* or egl_data */
#  endif
#  ifdef HAVE_JWZGLES
    jwzgles_state *jwzgles_state;
#  endif
# endif /* HAVE_GL */
};

typedef enum {  t_String, t_Float, t_Int, t_Bool } xlockmore_type;

typedef struct {
  void *var;
  char *name;
  char *classname;
  char *def;
  xlockmore_type type;
} argtype;

typedef struct {
  char *opt;
  char *desc;
} OptionStruct;

typedef struct {
  int numopts;
  XrmOptionDescRec *opts;
  int numvarsdesc;
  argtype *vars;
  OptionStruct *desc;
} ModeSpecOpt;

struct xlockmore_function_table {
  const char *progclass;
  const char *defaults;
  Bool want_writable_colors;
  enum { color_scheme_default, color_scheme_uniform, 
         color_scheme_smooth, color_scheme_bright }
    desired_color_scheme;
  void (*hack_init) (ModeInfo *);
  void (*hack_draw) (ModeInfo *);
  void (*hack_reshape) (ModeInfo *, int, int);
  void (*hack_release) (ModeInfo *);
  void (*hack_free) (ModeInfo *);
  Bool (*hack_handle_events) (ModeInfo *, XEvent *);
  ModeSpecOpt *opts;

# ifdef HAVE_JWZGLES    /* set in xlock-gl-utils.c */
  void (*jwzgles_make_current) (jwzgles_state *);
  void (*jwzgles_free) (void);
# endif /* HAVE_JWZGLES */

  void **state_array;
  unsigned long live_displays, got_init;
};

#ifdef HAVE_JWXYZ
# define XLOCKMORE_NUM_SCREENS	\
  (sizeof(((struct xlockmore_function_table *)0)->live_displays) * 8)
#else
# define XLOCKMORE_NUM_SCREENS	2 /* For DEBUG_PAIR. */
#endif

#endif /* __XLOCKMORE_INTERNAL_H__ */
