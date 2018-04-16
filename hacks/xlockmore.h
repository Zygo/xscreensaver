/* xlockmore.h --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997-2017 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * The definitions in this file make it possible to compile an xlockmore
 * module into a standalone program, and thus use it with xscreensaver.
 * By Jamie Zawinski <jwz@jwz.org> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef __STDC__
ERROR!  Sorry, xlockmore.h requires ANSI C (gcc, for example.)
  /* (The ansi dependency is that we use string concatenation,
     and cpp-based stringification of tokens.) */
#endif

#include "screenhackI.h"
#include "xlockmoreI.h"

# define ENTRYPOINT static

/* Accessor macros for the ModeInfo structure
 */
#define MI_DISPLAY(MI)		((MI)->dpy)
#define MI_WINDOW(MI)		((MI)->window)
#define MI_NUM_SCREENS(MI)	XLOCKMORE_NUM_SCREENS
#define MI_SCREEN(MI)		((MI)->screen_number)
#define MI_SCREENPTR(MI)	((MI)->xgwa.screen)
#define MI_WIN_WHITE_PIXEL(MI)	((MI)->white)
#define MI_WIN_BLACK_PIXEL(MI)	((MI)->black)
#define MI_NPIXELS(MI)		((MI)->npixels)
#define MI_PIXEL(MI,N)		((MI)->pixels[(N)])
#define MI_WIN_WIDTH(MI)	((MI)->xgwa.width)
#define MI_WIN_HEIGHT(MI)	((MI)->xgwa.height)
#define MI_WIN_DEPTH(MI)	((MI)->xgwa.depth)
#define MI_DEPTH(MI)		((MI)->xgwa.depth)
#define MI_WIN_COLORMAP(MI)	((MI)->xgwa.colormap)
#define MI_VISUAL(MI)		((MI)->xgwa.visual)
#define MI_GC(MI)		((MI)->gc)
#define MI_PAUSE(MI)		((MI)->pause)
#define MI_DELAY(MI)		((MI)->pause)
#define MI_WIN_IS_FULLRANDOM(MI)((MI)->fullrandom)
#define MI_WIN_IS_VERBOSE(MI)   (False)
#define MI_WIN_IS_INSTALL(MI)   (True)
#define MI_WIN_IS_MONO(MI)	(mono_p)
#define MI_WIN_IS_INROOT(MI)	((MI)->root_p)
#define MI_WIN_IS_INWINDOW(MI)	(!(MI)->root_p)
#define MI_WIN_IS_ICONIC(MI)	(False)
#define MI_WIN_IS_WIREFRAME(MI)	((MI)->wireframe_p)
#define MI_WIN_IS_USE3D(MI)	((MI)->threed)
#define MI_LEFT_COLOR(MI)	((MI)->threed_left_color)
#define MI_RIGHT_COLOR(MI)	((MI)->threed_right_color)
#define MI_BOTH_COLOR(MI)	((MI)->threed_both_color)
#define MI_NONE_COLOR(MI)	((MI)->threed_none_color)
#define MI_DELTA3D(MI)		((MI)->threed_delta)
#define MI_CYCLES(MI)		((MI)->cycles)
#define MI_BATCHCOUNT(MI)	((MI)->batchcount)
#define MI_SIZE(MI)		((MI)->size)
#define MI_IS_DRAWN(MI)		((MI)->is_drawn)
#define MI_IS_FPS(MI)		((MI)->fps_p)
#define MI_NCOLORS(MI)		((MI)->npixels)
#define MI_NAME(MI)		(progname)

#define MI_COLORMAP(MI)	        (MI_WIN_COLORMAP((MI)))
#define MI_WIDTH(MI)		(MI_WIN_WIDTH((MI)))
#define MI_HEIGHT(MI)		(MI_WIN_HEIGHT((MI)))
#define MI_IS_ICONIC(MI)	(MI_WIN_IS_ICONIC((MI)))
#define MI_IS_WIREFRAME(MI)	(MI_WIN_IS_WIREFRAME((MI)))
#define MI_IS_MONO(MI)		(MI_WIN_IS_MONO((MI)))
#define MI_COUNT(MI)		(MI_BATCHCOUNT((MI)))
#define MI_BLACK_PIXEL(MI)	(MI_WIN_BLACK_PIXEL(MI))
#define MI_WHITE_PIXEL(MI)	(MI_WIN_WHITE_PIXEL(MI))
#define MI_IS_FULLRANDOM(MI)	(MI_WIN_IS_FULLRANDOM(MI))
#define MI_IS_VERBOSE(MI)	(MI_WIN_IS_VERBOSE(MI))
#define MI_IS_INSTALL(MI)	(MI_WIN_IS_INSTALL(MI))
#define MI_IS_DEBUG(MI)		(False)
#define MI_IS_MOUSE(MI)		(False)

/* Under xlockmore, MI_CLEARWINDOW runs immediately, and for animated clears
   it delays execution while the animation runs. This doesn't work on
   XScreenSaver, which has mandatory double-buffering on macOS/iOS/Android.

   Tricky: As a result, MI_CLEARWINDOW doesn't clear the window until after
   init_##() or draw_##() finishes.
 */
#ifdef USE_GL
# define MI_CLEARWINDOW(mi) XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi))
#else
# define MI_CLEARWINDOW(mi) ((mi)->needs_clear = True)
#endif

/* MI_INIT and MI_ABORT are XScreenSaver extensions. These exist primarily for
   the sake of ports to macOS, iOS, and Android, all of which need to restart
   individual screenhacks repeatedly in the same process. This requires
   reusing MI_SCREEN() numbers; previously many xlockmore API hacks did not
   support this the way they were supposed to.
 */

/* MI_INIT implements the following pattern, as seen in various forms at the
   beginning of init_##():

   if(!state_array) {
     state_array = (state_t *)calloc(MI_NUM_SCREENS(mi), sizeof(state_t));
     if(!state_array) {
       fprintf(stderr, "%s: out of memory\n", progname);
       return;
     }
   }
   hack_free_state(mi);
   memset(&state_array[MI_SCREEN(mi)], 0, sizeof(*state_array));

   MI_INIT also assumes ownership of the state_array over to xlockmore.c,
   which frees it after all screens (or "screens") are closed.
 */

#define MI_INIT(mi, state_array) \
  xlockmore_mi_init ((mi), sizeof(*(state_array)), (void **)&(state_array))

/* Use MI_ABORT if an init_## or draw_## hook needs to shut everything down.
   This replaces explicit calls to release_## hooks when things go wrong from
   inside an xlockmore API screenhack.

   At some point this may do something other than just abort().
 */
#define MI_ABORT(mi)		abort();

#define FreeAllGL(dpy)		/* */

/* Some other utility macros.
 */
#define SINF(n)			((float)sin((double)(n)))
#define COSF(n)			((float)cos((double)(n)))
#define FABSF(n)		((float)fabs((double)(n)))

#undef MAX
#undef MIN
#undef ABS
#define MAX(a,b)((a)>(b)?(a):(b))
#define MIN(a,b)((a)<(b)?(a):(b))
#define ABS(a)((a)<0 ? -(a) : (a))

/* Maximum possible number of colors (*not* default number of colors.) */
#define NUMCOLORS 256


/* In an Xlib world, we define two global symbols here:
   a struct in `MODULENAME_xscreensaver_function_table',
   and a pointer to that in `xscreensaver_function_table'.

   In a Cocoa or Android world, we only define the prefixed symbol;
   the un-prefixed symbol does not exist.
 */
#ifdef HAVE_MOBILE
# define XSCREENSAVER_LINK(NAME)
#else
# define XSCREENSAVER_LINK(NAME) \
   struct xscreensaver_function_table *xscreensaver_function_table = &NAME;
#endif


# if !defined(USE_GL) || defined(HAVE_COCOA) || defined(HAVE_ANDROID)
#  define xlockmore_pick_gl_visual 0
#  define xlockmore_validate_gl_visual 0
# endif  /* !USE_GL || HAVE_COCOA || HAVE_ANDROID */

# ifdef USE_GL
#  define XLOCKMORE_FPS xlockmore_gl_compute_fps
# else
#  define XLOCKMORE_FPS xlockmore_do_fps
# endif

# ifdef HAVE_JWXYZ
#  ifdef USE_GL
#   define XLOCKMORE_VISUAL GL_VISUAL
#  else
#   define XLOCKMORE_VISUAL DEFAULT_VISUAL
#  endif
# else /* !HAVE_JWXYZ */
#  define XLOCKMORE_VISUAL \
     xlockmore_pick_gl_visual, xlockmore_validate_gl_visual
# endif /* !HAVE_JWXYZ */

#ifdef WRITABLE_COLORS
# undef WRITABLE_COLORS
# define WRITABLE_COLORS 1
#else
# define WRITABLE_COLORS 0
#endif

#if defined(UNIFORM_COLORS)
# define XLOCKMORE_COLOR_SCHEME color_scheme_uniform
#elif defined(SMOOTH_COLORS)
# define XLOCKMORE_COLOR_SCHEME color_scheme_smooth
#elif defined(BRIGHT_COLORS)
# define XLOCKMORE_COLOR_SCHEME color_scheme_bright
#else
# define XLOCKMORE_COLOR_SCHEME color_scheme_default
#endif

/* This is the macro that links this program in with the rest of
   xscreensaver.  For example:

     XSCREENSAVER_MODULE   ("Atlantis", atlantis)
     XSCREENSAVER_MODULE_2 ("GLMatrix", glmatrix, matrix)

   CLASS:   a string, the class name for resources.
   NAME:    a token, the name of the executable.  Should be the same
            as CLASS, but downcased.
   PREFIX:  the symbol used in the function names, e.g., `draw_atlantis'.

   NAME and PREFIX are usually the same.  If they are not, use
   XSCREENSAVER_MODULE_2() instead of XSCREENSAVER_MODULE().
 */
#define XSCREENSAVER_MODULE_2(CLASS,NAME,PREFIX)			\
									\
  static struct xlockmore_function_table				\
	 NAME ## _xlockmore_function_table = {			\
	   CLASS,							\
	   DEFAULTS,							\
	   WRITABLE_COLORS,						\
	   XLOCKMORE_COLOR_SCHEME,					\
	   init_    ## PREFIX,						\
	   draw_    ## PREFIX,						\
	   reshape_ ## PREFIX,						\
	   release_ ## PREFIX,						\
	   free_    ## PREFIX,						\
	   PREFIX   ## _handle_event,					\
	   & PREFIX ## _opts						\
  };									\
									\
  struct xscreensaver_function_table					\
	 NAME ## _xscreensaver_function_table = {			\
	   0, 0, 0,							\
	   xlockmore_setup,						\
	   & NAME ## _xlockmore_function_table,				\
	   0, 0, 0, 0, 0,						\
	   XLOCKMORE_FPS,						\
	   XLOCKMORE_VISUAL };						\
									\
  XSCREENSAVER_LINK (NAME ## _xscreensaver_function_table)

#define XSCREENSAVER_MODULE(CLASS,PREFIX)				\
      XSCREENSAVER_MODULE_2(CLASS,PREFIX,PREFIX)
