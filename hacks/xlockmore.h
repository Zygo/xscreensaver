/* xlockmore.h --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997-2003 Jamie Zawinski <jwz@jwz.org>
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

#if !defined(PROGCLASS) || !defined(HACK_INIT) || !defined(HACK_DRAW)
ERROR!  Define PROGCLASS, HACK_INIT, and HACK_DRAW before including xlockmore.h
#endif

#include "config.h"

#ifndef __STDC__
ERROR!  Sorry, xlockmore.h requires ANSI C (gcc, for example.)
  /* (The ansi dependency is that we use string concatenation,
     and cpp-based stringification of tokens.) */
#endif

#include <stdio.h>
#include <math.h>
#include "xlockmoreI.h"

#ifdef USE_GL
# include <GL/glx.h>
  extern GLXContext *init_GL (ModeInfo *);
  extern void clear_gl_error (void);
  extern void check_gl_error (const char *type);

  extern void do_fps (ModeInfo *);
  extern GLfloat fps_1 (ModeInfo *);
  extern void    fps_2 (ModeInfo *);

# define FreeAllGL(dpy) /* */
#endif /* !USE_GL */

/* Accessor macros for the ModeInfo structure
 */

#define MI_DISPLAY(MI)		((MI)->dpy)
#define MI_WINDOW(MI)		((MI)->window)
#define MI_NUM_SCREENS(MI)	(1)	/* Only manage one screen at a time; */
#define MI_SCREEN(MI)		(0)	/*  this might be fragile... */
#define MI_WIN_WHITE_PIXEL(MI)	((MI)->white)
#define MI_WIN_BLACK_PIXEL(MI)	((MI)->black)
#define MI_NPIXELS(MI)		((MI)->npixels)
#define MI_PIXEL(MI,N)		((MI)->pixels[(N)])
#define MI_WIN_WIDTH(MI)	((MI)->xgwa.width)
#define MI_WIN_HEIGHT(MI)	((MI)->xgwa.height)
#define MI_WIN_DEPTH(MI)	((MI)->xgwa.depth)
#define MI_WIN_COLORMAP(MI)	((MI)->xgwa.colormap)
#define MI_VISUAL(MI)		((MI)->xgwa.visual)
#define MI_GC(MI)		((MI)->gc)
#define MI_PAUSE(MI)		((MI)->pause)
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

#define MI_CLEARWINDOW(mi) XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi))

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

/* The globals that screenhack.c expects (initialized by xlockmore.c).
 */
char *defaults[100];
XrmOptionDescRec options[100];

/* Prototypes for the actual drawing routines...
 */
extern void HACK_INIT(ModeInfo *);
extern void HACK_DRAW(ModeInfo *);

#ifdef HACK_FREE
  extern void HACK_FREE(ModeInfo *);
#else
# define HACK_FREE 0
#endif

#ifdef HACK_RESHAPE
  extern void HACK_RESHAPE(ModeInfo *, int width, int height);
#else
# define HACK_RESHAPE 0
#endif

#ifdef HACK_HANDLE_EVENT
  extern Bool HACK_HANDLE_EVENT(ModeInfo *, XEvent *e);
#else
# define HACK_HANDLE_EVENT 0
#endif


/* Emit code for the entrypoint used by screenhack.c, and pass control
   down into xlockmore.c with the appropriate parameters.
 */

char *progclass = PROGCLASS;

void screenhack (Display *dpy, Window window)
{
  xlockmore_screenhack (dpy, window,

#ifdef WRITABLE_COLORS
			True,
#else
			False,
#endif

#ifdef UNIFORM_COLORS
			True,
#else
			False,
#endif

#ifdef SMOOTH_COLORS
			True,
#else
			False,
#endif

#ifdef BRIGHT_COLORS
			True,
#else
			False,
#endif

#ifdef EVENT_MASK
			EVENT_MASK,
#else
			0,
#endif

			HACK_INIT,
			HACK_DRAW,
			HACK_RESHAPE,
			HACK_HANDLE_EVENT,
			HACK_FREE);
}


const char *app_defaults = DEFAULTS ;
