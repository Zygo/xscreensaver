/* xlockmore.h --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997-2012 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */


typedef struct ModeInfo ModeInfo;

#ifdef USE_GL

/* I'm told that the Sun version of OpenGL needs to have the constant
   SUN_OGL_NO_VERTEX_MACROS defined in order for morph3d to compile
   (the number of arguments to the glNormal3f macro changes...)
   Verified with gcc 2.7.2.2 and Sun cc 4.2 with OpenGL 1.1.1 dev 4
   on Solaris 2.5.1.
 */
# ifndef HAVE_MESA_GL
#  if defined(__sun) && defined(__SVR4)	/* Solaris */
#   define SUN_OGL_NO_VERTEX_MACROS 1
#  endif /* Solaris */
# endif /* !HAVE_MESA_GL */

# ifdef HAVE_COCOA
#  ifndef USE_IPHONE
#   include <OpenGL/gl.h>
#   include <OpenGL/glu.h>
#  endif
# else
#  include <GL/gl.h>
#  include <GL/glu.h>
#  include <GL/glx.h>
# endif

# ifdef HAVE_JWZGLES
#  include "jwzgles.h"
# endif /* HAVE_JWZGLES */


  extern GLXContext *init_GL (ModeInfo *);
  extern void xlockmore_reset_gl_state(void);
  extern void clear_gl_error (void);
  extern void check_gl_error (const char *type);

  extern Visual *xlockmore_pick_gl_visual (Screen *);
  extern Bool xlockmore_validate_gl_visual (Screen *, const char *, Visual *);

#endif /* !USE_GL */

/* These are only used in GL mode, but I don't understand why XCode
   isn't seeing the prototypes for them in glx/fps-gl.c... */
extern void do_fps (ModeInfo *);
extern void xlockmore_gl_compute_fps (Display *, Window, fps_state *, void *);
extern void xlockmore_gl_draw_fps (ModeInfo *);
# define do_fps xlockmore_gl_draw_fps


extern void xlockmore_setup (struct xscreensaver_function_table *, void *);
extern void xlockmore_do_fps (Display *, Window, fps_state *, void *);


/* Compatibility with the xlockmore RNG API
   (note that the xlockmore hacks never expect negative numbers.)
 */
#define LRAND()			((long) (random() & 0x7fffffff))
#define NRAND(n)		((int) (LRAND() % (n)))
#define MAXRAND			(2147483648.0) /* unsigned 1<<31 as a float */
#define SRAND(n)		/* already seeded by screenhack.c */


struct ModeInfo {
  struct xlockmore_function_table *xlmft;
  Display *dpy;
  Window window;
  Bool root_p;
  int num_screens;
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

  /* Used only by OpenGL programs, since FPS is tricky there. */
  fps_state *fpst;
  Bool fps_p;
  unsigned long polygon_count;  /* These values are for -fps display only */
  double recursion_depth;

#ifdef HAVE_XSHM_EXTENSION
  Bool use_shm;
  XShmSegmentInfo shm_info;
#endif
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
  void (*hack_refresh) (ModeInfo *);
  void (*hack_free) (ModeInfo *);
  Bool (*hack_handle_events) (ModeInfo *, XEvent *);
  ModeSpecOpt *opts;
};

#endif /* __XLOCKMORE_INTERNAL_H__ */
