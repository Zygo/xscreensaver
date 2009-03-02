#ifndef __XLOCK_MODE_H__
#define __XLOCK_MODE_H__

#if !defined( lint ) && !defined( SABER )
/* #ident  "@(#)mode.h      4.14 99/06/17 xlockmore" */

#endif

/*-
 * mode.h - mode management for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 * xscreensaver code, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@tux.org>
 * 18-Mar-96: Ron Hitchens <ron@idiom.com>
 *		Extensive revision to define new data types for
 *		the new mode calling scheme.
 * 02-Jun-95: Extracted out of resource.c.
 *
 */

/*-
 * Declare external interface routines for supported screen savers.
 */

/* -------------------------------------------------------------------- */

#ifdef STANDALONE

/* xscreensaver compatibility layer for xlockmore modules. */

/*-
 * xscreensaver, Copyright (c) 1997, 1998 Jamie Zawinski <jwz@jwz.org>
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

/*-
 * Accessor macros for the ModeInfo structure
 */

#define MI_DISPLAY(MI)		((MI)->dpy)
#define MI_WINDOW(MI)		((MI)->window)
#define MI_NUM_SCREENS(MI)	(1)	/* Only manage one screen at a time; */
#define MI_SCREEN(MI)		(0)	/*  this might be fragile... */
#define MI_NPIXELS(MI)		((MI)->npixels)
#define MI_PIXEL(MI,N)		((MI)->pixels[(N)])
#define MI_VISUAL(MI)		((MI)->xgwa.visual)
#define MI_GC(MI)		((MI)->gc)
#define MI_PAUSE(MI)		((MI)->pause)
#define MI_LEFT_COLOR(MI)	((MI)->threed_left_color)
#define MI_RIGHT_COLOR(MI)	((MI)->threed_right_color)
#define MI_BOTH_COLOR(MI)	((MI)->threed_both_color)
#define MI_NONE_COLOR(MI)	((MI)->threed_none_color)
#define MI_DELTA3D(MI)		((MI)->threed_delta)
#define MI_CYCLES(MI)		((MI)->cycles)
#define MI_BATCHCOUNT(MI)	((MI)->count)
#define MI_SIZE(MI)		((MI)->size)
#define MI_BITMAP(MI)		((MI)->bitmap)
#define MI_WHITE_PIXEL(MI)	((MI)->white)
#define MI_BLACK_PIXEL(MI)	((MI)->black)
#define MI_WIDTH(MI)	((MI)->xgwa.width)
#define MI_HEIGHT(MI)	((MI)->xgwa.height)
#define MI_DEPTH(MI)	((MI)->xgwa.depth)
#define MI_COLORMAP(MI)	((MI)->xgwa.colormap)
#define MI_IS_FULLRANDOM(MI)((MI)->fullrandom)
#define MI_IS_DEBUG(MI)   ((MI)->verbose)
#define MI_IS_VERBOSE(MI)   ((MI)->verbose)
#define MI_IS_INSTALL(MI)   (True)
#define MI_IS_MONO(MI)	(mono_p)
#define MI_IS_INROOT(MI)	((MI)->root_p)
#define MI_IS_INWINDOW(MI)	(!(MI)->root_p)
#define MI_IS_ICONIC(MI)	(False)
#define MI_IS_WIREFRAME(MI)	((MI)->wireframe_p)
#define MI_IS_FPS(MI)	((MI)->fps_p)
#define MI_IS_USE3D(MI)	((MI)->threed)
#define MI_COUNT(MI)	((MI)->count)
#define MI_NCOLORS(MI)    ((MI)->ncolors)
#define MI_IS_DRAWN(MI)((MI)->is_drawn)


#define MI_CLEARWINDOWCOLORMAP(mi, gc, pixel) \
{ \
 XSetForeground(MI_DISPLAY(mi), gc, pixel); \
 XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc, \
   0, 0, (unsigned int) MI_WIDTH(mi), (unsigned int) MI_HEIGHT(mi)); \
}
#define MI_CLEARWINDOWCOLORMAPFAST(mi, gc, pixel) \
 MI_CLEARWINDOWCOLORMAP(mi, gc, pixel)
#define MI_CLEARWINDOWCOLOR(mi, pixel) \
 MI_CLEARWINDOWCOLORMAP(mi, MI_GC(mi), pixel)

/* #define MI_CLEARWINDOW(mi) XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi)) */
#define MI_CLEARWINDOW(mi) MI_CLEARWINDOWCOLOR(mi, MI_BLACK_PIXEL(mi))

#include "screenhack.h"

#ifdef __cplusplus
  extern "C" {
#endif
typedef struct ModeInfo {
	Display    *dpy;
	Window      window;
	Bool        root_p;
	int         npixels;
	unsigned long *pixels;
	XColor     *colors;
	Bool        writable_p;
	unsigned long white;
	unsigned long black;
	XWindowAttributes xgwa;
	GC          gc;
	long        pause;
	Bool        fullrandom;
	Bool        verbose;
	int         count;
	int         cycles;
	int         size;
	int         ncolors;
	Bool        threed;
	long        threed_left_color;
	long        threed_right_color;
	long        threed_both_color;
	long        threed_none_color;
	long        threed_delta;
	Bool        wireframe_p;
	char       *bitmap;
	Bool        is_drawn;
        Bool        fps_p;
} ModeInfo;

typedef enum {
	t_String, t_Float, t_Int, t_Bool
} xlockmore_type;

typedef struct {
	void       *var;
	char       *name;
	char       *classname;
	char       *def;
	xlockmore_type type;
} argtype;

typedef struct {
	char       *opt;
	char       *desc;
} OptionStruct;

typedef struct {
	int         numopts;
	XrmOptionDescRec *opts;
	int         numvarsdesc;
	argtype    *vars;
	OptionStruct *desc;
} ModeSpecOpt;

extern void xlockmore_screenhack(Display * dpy, Window window,
				 Bool want_writable_colors,
				 Bool want_uniform_colors,
				 Bool want_smooth_colors,
				 Bool want_bright_colors,
				 void        (*hack_init) (ModeInfo *),
				 void        (*hack_draw) (ModeInfo *),
				 void        (*hack_free) (ModeInfo *));
#ifdef USE_GL
extern Visual *get_gl_visual(Screen * screen, char *name, char *class);
#endif
#ifdef __cplusplus
  }
#endif

#else /* STANDALONE */
#ifdef __cplusplus
  extern "C" {
#endif
struct LockStruct_s;
struct ModeInfo_s;

typedef void (ModeHook) (struct ModeInfo_s *);
typedef void (HookProc) (struct LockStruct_s *, struct ModeInfo_s *);

typedef struct LockStruct_s {
	char       *cmdline_arg;	/* mode name */
	ModeHook   *init_hook;	/* func to init a mode */
	ModeHook   *callback_hook;	/* func to run (tick) a mode */
	ModeHook   *release_hook;	/* func to shutdown a mode */
	ModeHook   *refresh_hook;	/* tells mode to repaint */
	ModeHook   *change_hook;	/* user wants mode to change */
	ModeHook   *unused_hook;	/* for future expansion */
	ModeSpecOpt *msopt;	/* this mode's def resources */
	int         def_delay;	/* default delay for mode */
	int         def_count;
	int         def_cycles;
	int         def_size;
	int         def_ncolors;
	float       def_saturation;
	char       *def_bitmap;
	char       *desc;	/* text description of mode */
	unsigned int flags;	/* state flags for this mode */
	void       *userdata;	/* for use by the mode */
} LockStruct;

#define LS_FLAG_INITED		1

typedef struct {
	Pixmap      pixmap;
#ifdef ORIGINAL_XPM_PATCH
	/* Not recommended */
	Pixmap      bitmap;
#else
	int         width, height, graphics_format;
#endif
} mailboxInfo;

typedef struct {
	mailboxInfo mail;
	mailboxInfo nomail;
	GC          mbgc;
} mboxInfo;

typedef struct {
	Visual     *visual;
	int         visualclass;	/* visual class name of current window */
	unsigned int depth;
	unsigned long red_mask, green_mask, blue_mask;	/* masks of current window */
	int         colormap_size;	/* colormap of current window */
	Colormap    colormap;	/* current colormap */
	Window      window;	/* window used to cover screen */
	Window      icon;	/* window used during password typein */
	Window      root;	/* convenience pointer to the root window */
#ifdef USE_BUTTON_LOGOUT
	Window      button;
#endif
	GC          gc;		/* graphics context for animation */
	GC          textgc;	/* graphics context used for text rendering */
	GC          plantextgc;	/* graphics context for plan message rendering */
	XPoint      iconpos;	/* location of top left edge of icon */
	XPoint      planpos;	/* location of top left edge of message */
	int         npixels;	/* number of valid entries in pixels */
	unsigned long *pixels;	/* pixel values in the colormap */
	unsigned long black_pixel, white_pixel;		/* black and white pixel values */
	unsigned long bg_pixel, fg_pixel;	/* background and foreground pixel values */
	unsigned long right_pixel, left_pixel;	/* 3D color pixel values */
	unsigned long none_pixel, both_pixel;
	XWindowChanges fullsizeconfigure;
	mboxInfo    mb;
	Pixmap	    root_pixmap;
} ScreenInfo;

typedef struct {
	Display    *display;	/* handle to X display */
	Screen     *screenptr;	/* ptr to screen info */
	int         screen;	/* number of current screen */
	int         real_screen;	/* for debugging */
	int         num_screens;	/* number screens locked */
	int         max_screens;	/* max # active screens */
	Window      window;	/* handle to current window */
	int         width;	/* width of current window */
	int         height;	/* height of current window */
	unsigned int flags;	/* xlock window flags */
	float       delta3d;
	Bool        is_drawn;	/*Indicates that enough is drawn for special *
				 *  erase                                    */
} WindowInfo;
#ifdef __cplusplus
  }
#endif

#define WI_FLAG_INFO_INITTED	0x001	/* private state flag */
#define WI_FLAG_ICONIC		0x002
#define WI_FLAG_MONO		0x004
#define WI_FLAG_INWINDOW	0x008
#define WI_FLAG_INROOT		0x010
#define WI_FLAG_NOLOCK		0x020
#define WI_FLAG_INSTALL		0x040
#define WI_FLAG_DEBUG		0x080
#define WI_FLAG_USE3D		0x100
#define WI_FLAG_VERBOSE		0x200
#define WI_FLAG_FULLRANDOM	0x400
#define WI_FLAG_WIREFRAME	0x800
#define WI_FLAG_FPS		0x1000
#define WI_FLAG_JUST_INITTED	0x2000	/* private state flag */

#ifdef __cplusplus
  extern "C" {
#endif
typedef struct {
	long        pause;	/* output, set by mode */
	long        delay;	/* inputs, current settings */
	int         count;
	int         cycles;
	int         size;
	int         ncolors;
	float       saturation;
	char       *bitmap;
} RunInfo;

typedef struct ModeInfo_s {
	WindowInfo  windowinfo;
	ScreenInfo *screeninfo;
	RunInfo     runinfo;
	struct LockStruct_s *lockstruct;
} ModeInfo;
#ifdef __cplusplus
  }
#endif

/* -------------------------------------------------------------------- */

/*-
 * These are the public interfaces that a mode should use to obtain
 * information about the display and other environmental parameters.
 * Everything hangs off a ModeInfo pointer.  A mode should NOT cache
 * a ModeInfo pointer, the struct it points to is volatile.  The mode
 * can safely make a copy of the data it points to, however.  But it
 * is recommended the mode make use of the passed-in pointer and pass
 * it along to functions it calls.
 * Use these macros, don't look at the fields directly.  The insides
 * of the ModeInfo struct are certain to change in the future.
 */

#define MODE_IS_INITED(ls)	((ls)->flags & LS_FLAG_INITED)
#define MODE_NOT_INITED(ls)	( ! MODE_IS_INITED(ls))

#define MI_DISPLAY(mi)		((mi)->windowinfo.display)
#define MI_SCREEN(mi)		((mi)->windowinfo.screen)
#define MI_SCREENPTR(mi)      	((mi)->windowinfo.screenptr)
#define MI_REAL_SCREEN(mi)	((mi)->windowinfo.real_screen)
#define MI_NUM_SCREENS(mi)	((mi)->windowinfo.num_screens)
#define MI_MAX_SCREENS(mi)	((mi)->windowinfo.max_screens)
#define MI_WINDOW(mi)		((mi)->windowinfo.window)
#define MI_WIDTH(mi)		((mi)->windowinfo.width)
#define MI_HEIGHT(mi)		((mi)->windowinfo.height)
#define MI_DELTA3D(mi)		((mi)->windowinfo.delta3d)
#define MI_FLAGS(mi)		((mi)->windowinfo.flags)
#define MI_IS_DRAWN(mi)		((mi)->windowinfo.is_drawn)
#define MI_SET_FLAG_STATE(mi,f,bool) ((mi)->windowinfo.flags = \
					(bool) ? (mi)->windowinfo.flags | f \
					: (mi)->windowinfo.flags & ~(f))
#define MI_FLAG_IS_SET(mi,f) 	((mi)->windowinfo.flags & f)
#define MI_FLAG_NOT_SET(mi,f) 	( ! MI_FLAG_IS_SET(mi,f))
#define MI_IS_ICONIC(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_ICONIC))
#define MI_IS_MONO(mi)		(MI_FLAG_IS_SET (mi, WI_FLAG_MONO))
#define MI_IS_INWINDOW(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_INWINDOW))
#define MI_IS_INROOT(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_INROOT))
#define MI_IS_NOLOCK(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_NOLOCK))
#define MI_IS_INSTALL(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_INSTALL))
#define MI_IS_DEBUG(mi)		(MI_FLAG_IS_SET (mi, WI_FLAG_DEBUG))
#define MI_IS_USE3D(mi)		(MI_FLAG_IS_SET (mi, WI_FLAG_USE3D))
#define MI_IS_VERBOSE(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_VERBOSE))
#define MI_IS_FULLRANDOM(mi) 	(MI_FLAG_IS_SET (mi, WI_FLAG_FULLRANDOM))
#define MI_IS_WIREFRAME(mi)	(MI_FLAG_IS_SET (mi, WI_FLAG_WIREFRAME))
#define MI_IS_FPS(mi)		(MI_FLAG_IS_SET (mi, WI_FLAG_FPS))

#define MI_SCREENINFO(mi)	((mi)->screeninfo)
#define MI_DEPTH(mi)		((mi)->screeninfo->depth)
#define MI_VISUAL(mi)		((mi)->screeninfo->visual)
#define MI_VISUALCLASS(mi)	((mi)->screeninfo->visualclass)
#define MI_COLORMAP_SIZE(mi)	((mi)->screeninfo->colormap_size)
#define MI_RED_MASK(mi)		((mi)->screeninfo->red_mask)
#define MI_GREEN_MASK(mi)	((mi)->screeninfo->green_mask)
#define MI_BLUE_MASK(mi)	((mi)->screeninfo->blue_mask)
#define MI_COLORMAP(mi)		((mi)->screeninfo->colormap)
#define MI_GC(mi)		((mi)->screeninfo->gc)
#define MI_NPIXELS(mi)		((mi)->screeninfo->npixels)
#define MI_PIXELS(mi)		((mi)->screeninfo->pixels)
#define MI_PIXEL(mi,n)		((mi)->screeninfo->pixels[n])
#define MI_BLACK_PIXEL(mi)	((mi)->screeninfo->black_pixel)
#define MI_WHITE_PIXEL(mi)	((mi)->screeninfo->white_pixel)
#define MI_BG_PIXEL(mi)		((mi)->screeninfo->bg_pixel)
#define MI_FG_PIXEL(mi)		((mi)->screeninfo->fg_pixel)
#define MI_NONE_COLOR(mi)	((mi)->screeninfo->none_pixel)	/* -install */
#define MI_RIGHT_COLOR(mi)	((mi)->screeninfo->right_pixel)
#define MI_LEFT_COLOR(mi)	((mi)->screeninfo->left_pixel)
#define MI_ROOT_PIXMAP(mi)	((mi)->screeninfo->root_pixmap)

#define MI_DELAY(mi)		((mi)->runinfo.delay)
#define MI_COUNT(mi)		((mi)->runinfo.count)
#define MI_CYCLES(mi)		((mi)->runinfo.cycles)
#define MI_SIZE(mi)		((mi)->runinfo.size)
#define MI_NCOLORS(mi)		((mi)->runinfo.ncolors)
#define MI_SATURATION(mi)	((mi)->runinfo.saturation)
#define MI_BITMAP(mi)		((mi)->runinfo.bitmap)
#define MI_PAUSE(MI)		((mi)->runinfo.pause)

#define MI_LOCKSTRUCT(mi)	((mi)->lockstruct)
#define MI_DEFDELAY(mi)		((mi)->lockstruct->def_delay)
#define MI_DEFCOUNT(mi)		((mi)->lockstruct->def_count)
#define MI_DEFCYCLES(mi)	((mi)->lockstruct->def_cycles)
#define MI_DEFSIZE(mi)		((mi)->lockstruct->def_size)
#define MI_DEFNCOLORS(mi)	((mi)->lockstruct->def_ncolors)
#define MI_DEFSATURATION(mi)	((mi)->lockstruct->def_saturation)
#define MI_DEFBITMAP(mi)	((mi)->lockstruct->def_bitmap)

#define MI_NAME(mi)		((mi)->lockstruct->cmdline_arg)
#define MI_DESC(mi)		((mi)->lockstruct->desc)
#define MI_USERDATA(mi)		((mi)->lockstruct->userdata)

#ifdef SOLARIS2
/*-
 * It was acting weird with the first rectangle drawn
 * on a Ultra2 with TrueColor Solaris2.x in deco mode.
 * Not sure if other modes are affected... now handled in deco mode.
 * If it does happen it is probably not as noticable to worry about.
 */
#endif

#ifdef __cplusplus
  extern "C" {
#endif
extern void modeDescription(ModeInfo * mi);
extern void erase_full_window(ModeInfo * mi, GC erase_gc, unsigned long pixel);
#ifdef __cplusplus
  }
#endif

#define MI_CLEARWINDOWCOLORMAP(mi, gc, pixel) \
{ \
 erase_full_window( mi , gc , pixel ); \
}
#define MI_CLEARWINDOWCOLORMAPFAST(mi, gc, pixel) \
{ \
 XSetForeground(MI_DISPLAY(mi), gc, pixel); \
 if ((MI_WIDTH(mi) >= 2) || (MI_HEIGHT(mi) >= 2)) { \
 XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc, 1, 1, \
  (unsigned int) MI_WIDTH(mi) - 2, (unsigned int) MI_HEIGHT(mi) - 2); \
 XDrawRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc, 0, 0, \
  (unsigned int) MI_WIDTH(mi) - 1, (unsigned int) MI_HEIGHT(mi) - 1); \
 } else \
 XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc, 0, 0, \
  (unsigned int) MI_WIDTH(mi), (unsigned int) MI_HEIGHT(mi)); \
}
#define MI_CLEARWINDOWCOLOR(mi, pixel) \
 MI_CLEARWINDOWCOLORMAP(mi, MI_GC(mi), pixel)
#if 1
/*-
 * XClearWindow with GL or MONO does not always work.
 */
#define MI_CLEARWINDOW(mi) MI_CLEARWINDOWCOLOR(mi, MI_BLACK_PIXEL(mi))
#else
#define MI_CLEARWINDOW(mi) XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi))
#endif

/* -------------------------------------------------------------------- */

#ifdef __cplusplus
  extern "C" {
#endif
extern HookProc call_init_hook;
extern HookProc call_callback_hook;
extern HookProc call_release_hook;
extern HookProc call_refresh_hook;
extern HookProc call_change_hook;

extern void set_default_mode(LockStruct *);
extern void release_last_mode(ModeInfo *);

/* -------------------------------------------------------------------- */

/* if you just want the blank mode... how sad! */
#ifndef BLANK_ONLY
#ifndef NICE_ONLY
/* comment out following defines to remove modes */
#ifdef USE_GL
#define MODE_biof
#define MODE_cage
#define MODE_fire
#define MODE_gears
#define MODE_glplanet
#define MODE_lament
#define MODE_moebius
#define MODE_molecule
#define MODE_morph3d
#define MODE_noof
#define MODE_rubik
#define MODE_sierpinski3d
#define MODE_sballs
#define MODE_sproingies
#define MODE_stairs
#define MODE_superquadrics
#ifdef USE_UNSTABLE
#define MODE_skewb /* under development */
#endif
#ifdef USE_RETIRED
#define MODE_cartoon /* retired mode */
#define MODE_kaleid2 /* retired mode */
#endif
#define MODE_atlantis
#define MODE_atunnels
#define MODE_bubble3d
#define MODE_pipes
#define MODE_sproingies
#ifdef HAVE_CXX
#define MODE_invert
#if defined( HAVE_TTF ) && defined( HAVE_GLTT )
#define MODE_text3d
#endif
#if defined( HAVE_FREETYPE ) && defined( HAVE_FTGL )
#define MODE_text3d2
#endif
#endif	/* HAVE_CXX */
#endif	/* USE_GL */
#ifdef HAVE_CXX
#define MODE_solitare
#endif
#ifdef USE_UNSTABLE
#define MODE_run /* VMS problems as well as -debug problems */
#ifdef UNDER_DEVELOPMENT
#if 0
#ifdef HAVE_CXX
#define MODE_billiards
#endif
#endif
#endif	/* UNDER_DEVELOPMENT */
#endif	/* USE_UNSTABLE */
#define MODE_ant
#define MODE_ant3d
#define MODE_apollonian
#define MODE_ball
#define MODE_bat
#define MODE_blot
#define MODE_bouboule
#define MODE_bounce
#define MODE_braid
#define MODE_bubble
#define MODE_bug
#define MODE_clock
#define MODE_coral
#define MODE_crystal
#define MODE_daisy
#define MODE_dclock
/*-
 * Comment out dclock sub-modes to disable them from random selection.
 * Useful for disabling y2k and millennium after those countdown dates
 * have already gone by.
 */
/* #define MODE_dclock_y2k */
/* #define MODE_dclock_millennium */
#define MODE_decay
#define MODE_deco
#define MODE_demon
#define MODE_dilemma
#define MODE_discrete
#define MODE_dragon
#define MODE_drift
#define MODE_euler2d
#define MODE_eyes
#define MODE_fadeplot
#define MODE_fiberlamp
#define MODE_flag
#define MODE_flame
#define MODE_flow
#define MODE_forest
#define MODE_galaxy
#define MODE_goop
#define MODE_grav
#define MODE_helix
#define MODE_hop
#define MODE_hyper
#define MODE_ico
#define MODE_ifs
#define MODE_image
#define MODE_juggle
#define MODE_julia
#define MODE_kaleid
#define MODE_kumppa
#define MODE_laser
#define MODE_life
#define MODE_life1d
#define MODE_life3d
#define MODE_lightning
#define MODE_lisa
#define MODE_lissie
#define MODE_loop
#define MODE_lyapunov
#define MODE_mandelbrot
#define MODE_marquee
#define MODE_matrix
#define MODE_maze
#define MODE_mountain
#define MODE_munch
#define MODE_nose
#define MODE_pacman
#define MODE_penrose
#define MODE_petal
#define MODE_petri
#define MODE_polyominoes
#define MODE_puzzle
#define MODE_pyro
#define MODE_qix
#define MODE_roll
#define MODE_rotor
#define MODE_scooter /* should be combined with star some day */
#define MODE_shape
#define MODE_sierpinski
#define MODE_slip
#define MODE_space /* should be combined with star some day */
#define MODE_sphere
#define MODE_spiral
#define MODE_spline
#define MODE_star
#define MODE_starfish
#define MODE_strange
#define MODE_swarm
#define MODE_swirl
#define MODE_t3d
#define MODE_tetris
#define MODE_thornbird
#define MODE_tik_tak
#define MODE_toneclock
#define MODE_triangle
#define MODE_tube
#define MODE_turtle
#define MODE_vines
#define MODE_voters
#define MODE_wator
#define MODE_wire
#define MODE_world
#define MODE_worm
#define MODE_xcl
#define MODE_xjack

#ifdef USE_BOMB
#define MODE_bomb
#endif

#else /* NICE_ONLY */
#ifdef HAVE_CXX
#define MODE_solitare
#endif
#define MODE_apollonian
#define MODE_blot
#define MODE_bouboule
#define MODE_bug
#define MODE_clock
#define MODE_daisy
#define MODE_dclock
#define MODE_decay
#define MODE_deco
#define MODE_demon
#define MODE_dilemma
#define MODE_dragon
#define MODE_eyes
#define MODE_fadeplot
#define MODE_flame
#define MODE_forest
#define MODE_grav
#define MODE_hop
#define MODE_hyper
#define MODE_ico
#define MODE_image
#define MODE_kaleid
#define MODE_life
#define MODE_life1d
#define MODE_life3d
#define MODE_lightning
#define MODE_lisa
#define MODE_lissie
#define MODE_loop
#define MODE_marquee
#define MODE_munch
#define MODE_nose
#define MODE_pacman
#define MODE_penrose
#define MODE_petal
#define MODE_puzzle
#define MODE_pyro
#define MODE_qix
#define MODE_roll
#define MODE_space /* should be combined with star some day */
#define MODE_sphere
#define MODE_spiral
#define MODE_spline
#define MODE_star
#define MODE_swarm
#define MODE_tetris
#define MODE_triangle
#define MODE_tube
#define MODE_turtle
#define MODE_vines
#define MODE_wator
#define MODE_wire
#define MODE_world
#define MODE_worm
#define MODE_xcl
#define MODE_xjack

#ifdef USE_BOMB
#define MODE_bomb
#endif

#endif /* NICE_ONLY */
#endif /* BLANK_ONLY */

#ifndef USE_MODULES

#ifdef MODE_ant
extern ModeHook init_ant;
extern ModeHook draw_ant;
extern ModeHook release_ant;
extern ModeHook refresh_ant;
extern ModeSpecOpt ant_opts;
#endif

#ifdef MODE_ant3d
extern ModeHook init_ant3d;
extern ModeHook draw_ant3d;
extern ModeHook release_ant3d;
extern ModeHook refresh_ant3d;
extern ModeSpecOpt ant3d_opts;
#endif

#ifdef MODE_apollonian
extern ModeHook init_apollonian;
extern ModeHook draw_apollonian;
extern ModeHook release_apollonian;
extern ModeHook refresh_apollonian;
extern ModeSpecOpt apollonian_opts;
#endif

#ifdef MODE_atlantis
extern ModeHook init_atlantis;
extern ModeHook draw_atlantis;
extern ModeHook release_atlantis;
extern ModeHook refresh_atlantis;
extern ModeHook change_atlantis;
extern ModeSpecOpt atlantis_opts;
#endif

#ifdef MODE_atunnels
extern ModeHook init_atunnels;
extern ModeHook draw_atunnels;
extern ModeHook release_atunnels;
extern ModeHook refresh_atunnels;
extern ModeHook change_atunnels;
extern ModeSpecOpt atunnels_opts;
#endif

#ifdef MODE_ball
extern ModeHook init_ball;
extern ModeHook draw_ball;
extern ModeHook release_ball;
extern ModeHook refresh_ball;
extern ModeSpecOpt ball_opts;
#endif

#ifdef MODE_bat
extern ModeHook init_bat;
extern ModeHook draw_bat;
extern ModeHook release_bat;
extern ModeHook refresh_bat;
extern ModeSpecOpt bat_opts;
#endif

#ifdef MODE_biof
extern ModeHook init_biof;
extern ModeHook draw_biof;
extern ModeHook release_biof;
extern ModeHook refresh_biof;
extern ModeHook change_fire;
extern ModeSpecOpt biof_opts;
#endif

#ifdef MODE_billiards
extern ModeHook init_billiards;
extern ModeHook draw_billiards;
extern ModeHook release_billiards;
extern ModeHook refresh_billiards;
extern ModeSpecOpt billiards_opts;
#endif

#ifdef MODE_blot
extern ModeHook init_blot;
extern ModeHook draw_blot;
extern ModeHook release_blot;
extern ModeHook refresh_blot;
extern ModeSpecOpt blot_opts;
#endif

#ifdef MODE_bouboule
extern ModeHook init_bouboule;
extern ModeHook draw_bouboule;
extern ModeHook release_bouboule;
extern ModeHook refresh_bouboule;
extern ModeSpecOpt bouboule_opts;
#endif

#ifdef MODE_bounce
extern ModeHook init_bounce;
extern ModeHook draw_bounce;
extern ModeHook release_bounce;
extern ModeHook refresh_bounce;
extern ModeSpecOpt bounce_opts;
#endif

#ifdef MODE_braid
extern ModeHook init_braid;
extern ModeHook draw_braid;
extern ModeHook release_braid;
extern ModeHook refresh_braid;
extern ModeSpecOpt braid_opts;
#endif

#ifdef MODE_bubble
extern ModeHook init_bubble;
extern ModeHook draw_bubble;
extern ModeHook release_bubble;
extern ModeHook refresh_bubble;
extern ModeSpecOpt bubble_opts;
#endif

#ifdef MODE_bubble3d
extern ModeHook init_bubble3d;
extern ModeHook draw_bubble3d;
extern ModeHook release_bubble3d;
extern ModeHook change_bubble3d;
extern ModeSpecOpt bubble3d_opts;
#endif

#ifdef MODE_bug
extern ModeHook init_bug;
extern ModeHook draw_bug;
extern ModeHook release_bug;
extern ModeHook refresh_bug;
extern ModeSpecOpt bug_opts;
#endif

#ifdef MODE_cage
extern ModeHook init_cage;
extern ModeHook draw_cage;
extern ModeHook release_cage;
extern ModeHook change_cage;
extern ModeSpecOpt cage_opts;
#endif

#ifdef MODE_cartoon
extern ModeHook init_cartoon;
extern ModeHook draw_cartoon;
extern ModeHook release_cartoon;
extern ModeHook refresh_cartoon;
extern ModeSpecOpt cartoon_opts;
#endif

#ifdef MODE_clock
extern ModeHook init_clock;
extern ModeHook draw_clock;
extern ModeHook release_clock;
extern ModeHook refresh_clock;
extern ModeSpecOpt clock_opts;
#endif

#ifdef MODE_coral
extern ModeHook init_coral;
extern ModeHook draw_coral;
extern ModeHook release_coral;
extern ModeHook refresh_coral;
extern ModeSpecOpt coral_opts;
#endif

#ifdef MODE_crystal
extern ModeHook init_crystal;
extern ModeHook draw_crystal;
extern ModeHook release_crystal;
extern ModeHook refresh_crystal;
extern ModeSpecOpt crystal_opts;
#endif

#ifdef MODE_daisy
extern ModeHook init_daisy;
extern ModeHook draw_daisy;
extern ModeHook release_daisy;
extern ModeHook refresh_daisy;
extern ModeSpecOpt daisy_opts;
#endif

#ifdef MODE_dclock
extern ModeHook init_dclock;
extern ModeHook draw_dclock;
extern ModeHook release_dclock;
extern ModeHook refresh_dclock;
extern ModeSpecOpt dclock_opts;
#endif

#ifdef MODE_decay
extern ModeHook init_decay;
extern ModeHook draw_decay;
extern ModeHook release_decay;
extern ModeHook refresh_decay;
extern ModeSpecOpt decay_opts;
#endif

#ifdef MODE_deco
extern ModeHook init_deco;
extern ModeHook draw_deco;
extern ModeHook release_deco;
extern ModeHook refresh_deco;
extern ModeSpecOpt deco_opts;
#endif

#ifdef MODE_demon
extern ModeHook init_demon;
extern ModeHook draw_demon;
extern ModeHook release_demon;
extern ModeHook refresh_demon;
extern ModeSpecOpt demon_opts;
#endif

#ifdef MODE_dilemma
extern ModeHook init_dilemma;
extern ModeHook draw_dilemma;
extern ModeHook release_dilemma;
extern ModeHook refresh_dilemma;
extern ModeSpecOpt dilemma_opts;
#endif

#ifdef MODE_discrete
extern ModeHook init_discrete;
extern ModeHook draw_discrete;
extern ModeHook release_discrete;
extern ModeHook refresh_discrete;
extern ModeHook change_discrete;
extern ModeSpecOpt discrete_opts;
#endif

#ifdef MODE_dragon
extern ModeHook init_dragon;
extern ModeHook draw_dragon;
extern ModeHook release_dragon;
extern ModeHook refresh_dragon;
extern ModeSpecOpt dragon_opts;
#endif

#ifdef MODE_drift
extern ModeHook init_drift;
extern ModeHook draw_drift;
extern ModeHook release_drift;
extern ModeHook refresh_drift;
extern ModeSpecOpt drift_opts;
#endif

#ifdef MODE_euler2d
extern ModeHook init_euler2d;
extern ModeHook draw_euler2d;
extern ModeHook release_euler2d;
extern ModeHook refresh_euler2d;
extern ModeSpecOpt euler2d_opts;
#endif

#ifdef MODE_eyes
extern ModeHook init_eyes;
extern ModeHook draw_eyes;
extern ModeHook release_eyes;
extern ModeHook refresh_eyes;
extern ModeSpecOpt eyes_opts;
#endif

#ifdef MODE_fadeplot
extern ModeHook init_fadeplot;
extern ModeHook draw_fadeplot;
extern ModeHook release_fadeplot;
extern ModeHook refresh_fadeplot;
extern ModeSpecOpt fadeplot_opts;
#endif

#ifdef MODE_fire
extern ModeHook init_fire;
extern ModeHook draw_fire;
extern ModeHook release_fire;
extern ModeHook refresh_fire;
extern ModeHook change_fire;
extern ModeSpecOpt fire_opts;
#endif

#ifdef MODE_fiberlamp
extern ModeHook init_fiberlamp;
extern ModeHook draw_fiberlamp;
extern ModeHook release_fiberlamp;
extern ModeHook change_fiberlamp;
extern ModeSpecOpt fiberlamp_opts;
#endif

#ifdef MODE_flag
extern ModeHook init_flag;
extern ModeHook draw_flag;
extern ModeHook release_flag;
extern ModeHook refresh_flag;
extern ModeSpecOpt flag_opts;
#endif

#ifdef MODE_flame
extern ModeHook init_flame;
extern ModeHook draw_flame;
extern ModeHook release_flame;
extern ModeHook refresh_flame;
extern ModeSpecOpt flame_opts;
#endif

#ifdef MODE_flow
extern ModeHook init_flow;
extern ModeHook draw_flow;
extern ModeHook release_flow;
extern ModeHook refresh_flow;
extern ModeHook change_flow;
extern ModeSpecOpt flow_opts;
#endif

#ifdef MODE_forest
extern ModeHook init_forest;
extern ModeHook draw_forest;
extern ModeHook release_forest;
extern ModeHook refresh_forest;
extern ModeSpecOpt forest_opts;
#endif

#ifdef MODE_galaxy
extern ModeHook init_galaxy;
extern ModeHook draw_galaxy;
extern ModeHook release_galaxy;
extern ModeHook refresh_galaxy;
extern ModeSpecOpt galaxy_opts;
#endif

#ifdef MODE_gears
extern ModeHook init_gears;
extern ModeHook draw_gears;
extern ModeHook release_gears;
extern ModeSpecOpt gears_opts;
#endif

#ifdef MODE_glplanet
extern ModeHook init_glplanet;
extern ModeHook draw_glplanet;
extern ModeHook release_glplanet;
extern ModeSpecOpt glplanet_opts;
#endif

#ifdef MODE_goop
extern ModeHook init_goop;
extern ModeHook draw_goop;
extern ModeHook release_goop;
extern ModeSpecOpt goop_opts;
#endif

#ifdef MODE_grav
extern ModeHook init_grav;
extern ModeHook draw_grav;
extern ModeHook release_grav;
extern ModeHook refresh_grav;
extern ModeSpecOpt grav_opts;
#endif

#ifdef MODE_helix
extern ModeHook init_helix;
extern ModeHook draw_helix;
extern ModeHook release_helix;
extern ModeHook refresh_helix;
extern ModeSpecOpt helix_opts;
#endif

#ifdef MODE_hop
extern ModeHook init_hop;
extern ModeHook draw_hop;
extern ModeHook release_hop;
extern ModeHook refresh_hop;
extern ModeSpecOpt hop_opts;
#endif

#ifdef MODE_hyper
extern ModeHook init_hyper;
extern ModeHook draw_hyper;
extern ModeHook release_hyper;
extern ModeHook refresh_hyper;
extern ModeHook change_hyper;
extern ModeSpecOpt hyper_opts;
#endif

#ifdef MODE_ico
extern ModeHook init_ico;
extern ModeHook draw_ico;
extern ModeHook release_ico;
extern ModeHook refresh_ico;
extern ModeHook change_ico;
extern ModeSpecOpt ico_opts;
#endif

#ifdef MODE_ifs
extern ModeHook init_ifs;
extern ModeHook draw_ifs;
extern ModeHook release_ifs;
extern ModeSpecOpt ifs_opts;
#endif

#ifdef MODE_image
extern ModeHook init_image;
extern ModeHook draw_image;
extern ModeHook release_image;
extern ModeHook refresh_image;
extern ModeSpecOpt image_opts;
#endif

#ifdef MODE_invert
extern ModeHook init_invert;
extern ModeHook draw_invert;
extern ModeHook release_invert;
extern ModeSpecOpt invert_opts;
#endif


#ifdef MODE_juggle
extern ModeHook init_juggle;
extern ModeHook draw_juggle;
extern ModeHook release_juggle;
extern ModeHook change_juggle;
extern ModeSpecOpt juggle_opts;
#endif

#ifdef MODE_julia
extern ModeHook init_julia;
extern ModeHook draw_julia;
extern ModeHook release_julia;
extern ModeHook refresh_julia;
extern ModeSpecOpt julia_opts;
#endif

#ifdef MODE_kaleid2
extern ModeHook init_kaleid2;
extern ModeHook draw_kaleid2;
extern ModeHook release_kaleid2;
extern ModeHook refresh_kaleid2;
extern ModeSpecOpt kaleid2_opts;
#endif

#ifdef MODE_kaleid
extern ModeHook init_kaleid;
extern ModeHook draw_kaleid;
extern ModeHook release_kaleid;
extern ModeHook refresh_kaleid;
extern ModeSpecOpt kaleid_opts;
#endif

#ifdef MODE_kumppa
extern ModeHook init_kumppa;
extern ModeHook draw_kumppa;
extern ModeHook release_kumppa;
extern ModeSpecOpt kumppa_opts;
#endif

#ifdef MODE_lament
extern ModeHook init_lament;
extern ModeHook draw_lament;
extern ModeHook release_lament;
extern ModeHook change_lament;
extern ModeSpecOpt lament_opts;
#endif

#ifdef MODE_laser
extern ModeHook init_laser;
extern ModeHook draw_laser;
extern ModeHook release_laser;
extern ModeHook refresh_laser;
extern ModeSpecOpt laser_opts;
#endif

#ifdef MODE_life
extern ModeHook init_life;
extern ModeHook draw_life;
extern ModeHook release_life;
extern ModeHook refresh_life;
extern ModeHook change_life;
extern ModeSpecOpt life_opts;
#endif

#ifdef MODE_life1d
extern ModeHook init_life1d;
extern ModeHook draw_life1d;
extern ModeHook release_life1d;
extern ModeHook refresh_life1d;
extern ModeSpecOpt life1d_opts;
#endif

#ifdef MODE_life3d
extern ModeHook init_life3d;
extern ModeHook draw_life3d;
extern ModeHook release_life3d;
extern ModeHook refresh_life3d;
extern ModeHook change_life3d;
extern ModeSpecOpt life3d_opts;
#endif

#ifdef MODE_lightning
extern ModeHook init_lightning;
extern ModeHook draw_lightning;
extern ModeHook release_lightning;
extern ModeHook refresh_lightning;
extern ModeSpecOpt lightning_opts;
#endif

#ifdef MODE_lisa
extern ModeHook init_lisa;
extern ModeHook draw_lisa;
extern ModeHook release_lisa;
extern ModeHook refresh_lisa;
extern ModeHook change_lisa;
extern ModeSpecOpt lisa_opts;
#endif

#ifdef MODE_lissie
extern ModeHook init_lissie;
extern ModeHook draw_lissie;
extern ModeHook release_lissie;
extern ModeHook refresh_lissie;
extern ModeSpecOpt lissie_opts;
#endif

#ifdef MODE_loop
extern ModeHook init_loop;
extern ModeHook draw_loop;
extern ModeHook release_loop;
extern ModeHook refresh_loop;
extern ModeSpecOpt loop_opts;
#endif

#ifdef MODE_lyapunov
extern ModeHook init_lyapunov;
extern ModeHook draw_lyapunov;
extern ModeHook release_lyapunov;
extern ModeHook refresh_lyapunov;
extern ModeSpecOpt lyapunov_opts;
#endif

#ifdef MODE_mandelbrot
extern ModeHook init_mandelbrot;
extern ModeHook draw_mandelbrot;
extern ModeHook release_mandelbrot;
extern ModeHook refresh_mandelbrot;
extern ModeSpecOpt mandelbrot_opts;
#endif

#ifdef MODE_marquee
extern ModeHook init_marquee;
extern ModeHook draw_marquee;
extern ModeHook release_marquee;
extern ModeSpecOpt marquee_opts;
#endif

#ifdef MODE_matrix
extern ModeHook init_matrix;
extern ModeHook draw_matrix;
extern ModeHook release_matrix;
extern ModeHook refresh_matrix;
extern ModeHook change_matrix;
extern ModeSpecOpt matrix_opts;
#endif

#ifdef MODE_maze
extern ModeHook init_maze;
extern ModeHook draw_maze;
extern ModeHook release_maze;
extern ModeHook refresh_maze;
extern ModeSpecOpt maze_opts;
#endif

#ifdef MODE_moebius
extern ModeHook init_moebius;
extern ModeHook draw_moebius;
extern ModeHook release_moebius;
extern ModeHook change_moebius;
extern ModeSpecOpt moebius_opts;
#endif

#ifdef MODE_molecule
extern ModeHook init_molecule;
extern ModeHook draw_molecule;
extern ModeHook release_molecule;
extern ModeSpecOpt molecule_opts;
#endif

#ifdef MODE_morph3d
extern ModeHook init_morph3d;
extern ModeHook draw_morph3d;
extern ModeHook release_morph3d;
extern ModeHook change_morph3d;
extern ModeSpecOpt morph3d_opts;
#endif

#ifdef MODE_mountain
extern ModeHook init_mountain;
extern ModeHook draw_mountain;
extern ModeHook release_mountain;
extern ModeHook refresh_mountain;
extern ModeSpecOpt mountain_opts;
#endif

#ifdef MODE_munch
extern ModeHook init_munch;
extern ModeHook draw_munch;
extern ModeHook release_munch;
extern ModeSpecOpt munch_opts;
#endif

#ifdef MODE_noof
extern ModeHook init_noof;
extern ModeHook draw_noof;
extern ModeHook release_noof;
extern ModeSpecOpt noof_opts;
#endif

#ifdef MODE_nose
extern ModeHook init_nose;
extern ModeHook draw_nose;
extern ModeHook release_nose;
extern ModeHook refresh_nose;
extern ModeSpecOpt nose_opts;
#endif

#ifdef MODE_pacman
extern ModeHook init_pacman;
extern ModeHook draw_pacman;
extern ModeHook release_pacman;
extern ModeHook refresh_pacman;
extern ModeHook change_pacman;
extern ModeSpecOpt pacman_opts;
#endif

#ifdef MODE_penrose
extern ModeHook init_penrose;
extern ModeHook draw_penrose;
extern ModeHook release_penrose;

#if 0
extern ModeHook refresh_penrose;	/* Needed */
#endif
extern ModeSpecOpt penrose_opts;
#endif

#ifdef MODE_petal
extern ModeHook init_petal;
extern ModeHook draw_petal;
extern ModeHook release_petal;
extern ModeHook refresh_petal;
extern ModeSpecOpt petal_opts;
#endif

#ifdef MODE_petri
extern ModeHook init_petri;
extern ModeHook draw_petri;
extern ModeHook release_petri;
extern ModeHook refresh_petri;
extern ModeSpecOpt petri_opts;
#endif

#ifdef MODE_pipes
extern ModeHook init_pipes;
extern ModeHook draw_pipes;
extern ModeHook release_pipes;
extern ModeHook refresh_pipes;
extern ModeHook change_pipes;
extern ModeSpecOpt pipes_opts;
#endif

#ifdef MODE_polyominoes
extern ModeHook init_polyominoes;
extern ModeHook draw_polyominoes;
extern ModeHook release_polyominoes;
extern ModeHook refresh_polyominoes;
extern ModeSpecOpt polyominoes_opts;
#endif

#ifdef MODE_puzzle
extern ModeHook init_puzzle;
extern ModeHook draw_puzzle;
extern ModeHook release_puzzle;

#if 0
extern ModeHook refresh_puzzle;	/* Needed */
#endif
extern ModeSpecOpt puzzle_opts;
#endif

#ifdef MODE_pyro
extern ModeHook init_pyro;
extern ModeHook draw_pyro;
extern ModeHook release_pyro;
extern ModeHook refresh_pyro;
extern ModeSpecOpt pyro_opts;
#endif

#ifdef MODE_qix
extern ModeHook init_qix;
extern ModeHook draw_qix;
extern ModeHook release_qix;
extern ModeHook refresh_qix;
extern ModeSpecOpt qix_opts;
#endif

#ifdef MODE_roll
extern ModeHook init_roll;
extern ModeHook draw_roll;
extern ModeHook release_roll;
extern ModeHook refresh_roll;
extern ModeSpecOpt roll_opts;
#endif

#ifdef MODE_rotor
extern ModeHook init_rotor;
extern ModeHook draw_rotor;
extern ModeHook release_rotor;
extern ModeHook refresh_rotor;
extern ModeSpecOpt rotor_opts;
#endif

#ifdef MODE_rubik
extern ModeHook init_rubik;
extern ModeHook draw_rubik;
extern ModeHook release_rubik;
extern ModeHook change_rubik;
extern ModeSpecOpt rubik_opts;
#endif

#ifdef MODE_sballs
extern ModeHook init_sballs;
extern ModeHook draw_sballs;
extern ModeHook release_sballs;
extern ModeHook refresh_sballs;
extern ModeHook change_sballs;
extern ModeSpecOpt sballs_opts;
#endif

#ifdef MODE_scooter
extern ModeHook init_scooter;
extern ModeHook draw_scooter;
extern ModeHook release_scooter;
extern ModeHook refresh_scooter;
extern ModeHook change_scooter;
extern ModeSpecOpt scooter_opts;
#endif

#ifdef MODE_shape
extern ModeHook init_shape;
extern ModeHook draw_shape;
extern ModeHook release_shape;
extern ModeHook refresh_shape;
extern ModeSpecOpt shape_opts;
#endif

#ifdef MODE_sierpinski
extern ModeHook init_sierpinski;
extern ModeHook draw_sierpinski;
extern ModeHook release_sierpinski;
extern ModeHook refresh_sierpinski;
extern ModeSpecOpt sierpinski_opts;
#endif

#ifdef MODE_sierpinski3d
extern ModeHook init_gasket;
extern ModeHook draw_gasket;
extern ModeHook release_gasket;
extern ModeSpecOpt gasket_opts;
#endif

#ifdef MODE_skewb
extern ModeHook init_skewb;
extern ModeHook draw_skewb;
extern ModeHook release_skewb;
extern ModeHook change_skewb;
extern ModeSpecOpt skewb_opts;
#endif

#ifdef MODE_slip
extern ModeHook init_slip;
extern ModeHook draw_slip;
extern ModeHook release_slip;

#if 0
extern ModeHook refresh_slip;	/* Probably not practical */
#endif
extern ModeSpecOpt slip_opts;
#endif

#ifdef MODE_solitare
extern ModeHook init_solitare;
extern ModeHook draw_solitare;
extern ModeHook release_solitare;
extern ModeHook refresh_solitare;
extern ModeSpecOpt solitare_opts;
#endif

#ifdef MODE_space
extern ModeHook init_space;
extern ModeHook draw_space;
extern ModeHook release_space;
extern ModeHook refresh_space;
extern ModeSpecOpt space_opts;
#endif

#ifdef MODE_sphere
extern ModeHook init_sphere;
extern ModeHook draw_sphere;
extern ModeHook release_sphere;
extern ModeHook refresh_sphere;
extern ModeSpecOpt sphere_opts;
#endif

#ifdef MODE_spiral
extern ModeHook init_spiral;
extern ModeHook draw_spiral;
extern ModeHook release_spiral;
extern ModeHook refresh_spiral;
extern ModeSpecOpt spiral_opts;
#endif

#ifdef MODE_spline
extern ModeHook init_spline;
extern ModeHook draw_spline;
extern ModeHook release_spline;
extern ModeHook refresh_spline;
extern ModeSpecOpt spline_opts;
#endif

#ifdef MODE_sproingies
extern ModeHook init_sproingies;
extern ModeHook draw_sproingies;
extern ModeHook release_sproingies;
extern ModeHook refresh_sproingies;
extern ModeSpecOpt sproingies_opts;
#endif

#ifdef MODE_stairs
extern ModeHook init_stairs;
extern ModeHook draw_stairs;
extern ModeHook release_stairs;
extern ModeHook refresh_stairs;
extern ModeHook change_stairs;
extern ModeSpecOpt stairs_opts;
#endif

#ifdef MODE_star
extern ModeHook init_star;
extern ModeHook draw_star;
extern ModeHook release_star;
extern ModeHook refresh_star;
extern ModeSpecOpt star_opts;
#endif

#ifdef MODE_starfish
extern ModeHook init_starfish;
extern ModeHook draw_starfish;
extern ModeHook release_starfish;
extern ModeSpecOpt starfish_opts;
#endif

#ifdef MODE_strange
extern ModeHook init_strange;
extern ModeHook draw_strange;
extern ModeHook release_strange;
extern ModeSpecOpt strange_opts;
#endif

#ifdef MODE_superquadrics
extern ModeHook init_superquadrics;
extern ModeHook draw_superquadrics;
extern ModeHook release_superquadrics;
extern ModeHook refresh_superquadrics;
extern ModeSpecOpt superquadrics_opts;
#endif

#ifdef MODE_swarm
extern ModeHook init_swarm;
extern ModeHook draw_swarm;
extern ModeHook release_swarm;
extern ModeHook refresh_swarm;
extern ModeSpecOpt swarm_opts;
#endif

#ifdef MODE_swirl
extern ModeHook init_swirl;
extern ModeHook draw_swirl;
extern ModeHook release_swirl;
extern ModeHook refresh_swirl;
extern ModeSpecOpt swirl_opts;
#endif

#ifdef MODE_t3d
extern ModeHook init_t3d;
extern ModeHook draw_t3d;
extern ModeHook release_t3d;
extern ModeHook refresh_t3d;
extern ModeSpecOpt t3d_opts;
#endif

#ifdef MODE_tetris
extern ModeHook init_tetris;
extern ModeHook draw_tetris;
extern ModeHook release_tetris;
extern ModeHook refresh_tetris;
extern ModeHook change_tetris;
extern ModeSpecOpt tetris_opts;
#endif

#ifdef MODE_text3d
extern ModeHook init_text3d;
extern ModeHook draw_text3d;
extern ModeHook release_text3d;
extern ModeHook refresh_text3d;
extern ModeHook change_text3d;
extern ModeSpecOpt text3d_opts;
#endif

#ifdef MODE_text3d2
extern ModeHook init_text3d2;
extern ModeHook draw_text3d2;
extern ModeHook release_text3d2;
extern ModeHook refresh_text3d2;
extern ModeHook change_text3d2;
extern ModeSpecOpt text3d2_opts;
#endif

#ifdef MODE_thornbird
extern ModeHook init_thornbird;
extern ModeHook draw_thornbird;
extern ModeHook release_thornbird;
extern ModeHook refresh_thornbird;
extern ModeHook change_thornbird;
extern ModeSpecOpt thornbird_opts;
#endif

#ifdef MODE_tik_tak
extern ModeHook init_tik_tak;
extern ModeHook draw_tik_tak;
extern ModeHook release_tik_tak;
extern ModeHook refresh_tik_tak;
extern ModeSpecOpt tik_tak_opts;
#endif

#ifdef MODE_toneclock
extern ModeHook init_toneclock;
extern ModeHook draw_toneclock;
extern ModeHook release_toneclock;
extern ModeHook refresh_toneclock;
extern ModeSpecOpt toneclock_opts;
#endif

#ifdef MODE_triangle
extern ModeHook init_triangle;
extern ModeHook draw_triangle;
extern ModeHook release_triangle;
extern ModeHook refresh_triangle;
extern ModeSpecOpt triangle_opts;
#endif

#ifdef MODE_tube
extern ModeHook init_tube;
extern ModeHook draw_tube;
extern ModeHook release_tube;
extern ModeHook refresh_tube;
extern ModeSpecOpt tube_opts;
#endif

#ifdef MODE_turtle
extern ModeHook init_turtle;
extern ModeHook draw_turtle;
extern ModeHook release_turtle;
extern ModeHook refresh_turtle;
extern ModeSpecOpt turtle_opts;
#endif

#ifdef MODE_vines
extern ModeHook init_vines;
extern ModeHook draw_vines;
extern ModeHook release_vines;
extern ModeHook refresh_vines;
extern ModeSpecOpt vines_opts;
#endif

#ifdef MODE_voters
extern ModeHook init_voters;
extern ModeHook draw_voters;
extern ModeHook release_voters;
extern ModeHook refresh_voters;
extern ModeSpecOpt voters_opts;
#endif

#ifdef MODE_wator
extern ModeHook init_wator;
extern ModeHook draw_wator;
extern ModeHook release_wator;
extern ModeHook refresh_wator;
extern ModeSpecOpt wator_opts;
#endif

#ifdef MODE_wire
extern ModeHook init_wire;
extern ModeHook draw_wire;
extern ModeHook release_wire;
extern ModeHook refresh_wire;
extern ModeSpecOpt wire_opts;
#endif

#ifdef MODE_world
extern ModeHook init_world;
extern ModeHook draw_world;
extern ModeHook release_world;
extern ModeHook refresh_world;
extern ModeSpecOpt world_opts;
#endif

#ifdef MODE_worm
extern ModeHook init_worm;
extern ModeHook draw_worm;
extern ModeHook release_worm;
extern ModeHook refresh_worm;
extern ModeSpecOpt worm_opts;
#endif

#ifdef MODE_xcl
extern ModeHook init_xcl;
extern ModeHook draw_xcl;
extern ModeHook release_xcl;
extern ModeSpecOpt xcl_opts;
#endif

#ifdef MODE_xjack
extern ModeHook init_xjack;
extern ModeHook draw_xjack;
extern ModeHook release_xjack;
extern ModeSpecOpt xjack_opts;
#endif

extern ModeHook init_blank;
extern ModeHook draw_blank;
extern ModeHook release_blank;
extern ModeHook refresh_blank;
extern ModeSpecOpt blank_opts;

#ifdef MODE_run
extern ModeHook init_run;
extern ModeHook draw_run;
extern ModeHook release_run;
extern ModeHook refresh_run;
extern ModeSpecOpt run_opts;
#endif

#ifdef MODE_bomb
extern ModeHook init_bomb;
extern ModeHook draw_bomb;
extern ModeHook release_bomb;
extern ModeHook refresh_bomb;
extern ModeHook change_bomb;
extern ModeSpecOpt bomb_opts;
#endif

extern ModeHook init_random;
extern ModeHook draw_random;
extern ModeHook release_random;
extern ModeHook refresh_random;
extern ModeHook change_random;
extern ModeSpecOpt random_opts;

extern LockStruct LockProcs[];

#else /* #ifndef USE_MODULES */

extern void LoadModules(char *);
extern void UnloadModules();

extern LockStruct *LockProcs;
extern void **LoadedModules;	/* save handles on loaded modules for closing */

#endif

extern int  numprocs;

#ifdef __cplusplus
  }
#endif

#endif /* STANDALONE */
/* -------------------------------------------------------------------- */
#endif /* __XLOCK_MODE_H__ */
