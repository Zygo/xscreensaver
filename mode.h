
#ifndef __XLOCK_MODE_H__
#define __XLOCK_MODE_H__

/*-
 * @(#)mode.h	4.00 97/01/01 xlockmore
 *
 * mode.h - mode management for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
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
	int         def_batchcount;
	int         def_cycles;
	int         def_size;
	float       def_saturation;
	char       *desc;	/* text description of mode */
	unsigned int flags;	/* state flags for this mode */
	void       *userdata;	/* for use by the mode */
} LockStruct;

#define LS_FLAG_INITED		1

typedef struct {
	Display    *display;	/* handle to X display */
	Screen     *screenptr;	/* ptr to screen info */
	int         screen;	/* number of current screen */
	int         real_screen;	/* for debugging */
	int         num_screens;	/* number screens locked */
	int         max_screens;	/* max # active screens */
	Window      window;	/* handle to current window */
	int         win_width;	/* width of current window */
	int         win_height;	/* height of current window */
	int         win_depth;	/* depth of current window */
	Visual     *visual;	/* visual of current window */
	Colormap    colormap;	/* default colormap of current window */
	unsigned long black_pixel;	/* pixel value for black */
	unsigned long white_pixel;	/* pixel value for white */
	unsigned int flags;	/* xlock window flags */
	float       delta3d;
} WindowInfo;

#define WI_FLAG_INFO_INITTED	0x001	/* private state flag */
#define WI_FLAG_ICONIC		0x002
#define WI_FLAG_MONO		0x004
#define WI_FLAG_INWINDOW	0x008
#define WI_FLAG_INROOT		0x010
#define WI_FLAG_NOLOCK	0x020
#define WI_FLAG_INSTALL		0x040
#define WI_FLAG_DEBUG		0x080
#define WI_FLAG_USE3D		0x100
#define WI_FLAG_VERBOSE		0x200
#define WI_FLAG_FULLRANDOM		0x400
#define WI_FLAG_WIREFRAME		0x800
#define WI_FLAG_JUST_INITTED	0x1000	/* private state flag */

typedef struct {
	long        pause;	/* output, set by mode */
	long        delay;	/* inputs, current settings */
	long        batchcount;
	long        cycles;
	long        size;
	float       saturation;
} RunInfo;

typedef struct ModeInfo_s {
	WindowInfo  windowinfo;
	perscreen  *screeninfo;
	RunInfo     runinfo;
	struct LockStruct_s *lockstruct;
} ModeInfo;

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
#define MI_SCREENPTR(mi)      ((mi)->windowinfo.screenptr)
#define MI_REAL_SCREEN(mi)	((mi)->windowinfo.real_screen)
#define MI_NUM_SCREENS(mi)	((mi)->windowinfo.num_screens)
#define MI_MAX_SCREENS(mi)	((mi)->windowinfo.max_screens)
#define MI_WINDOW(mi)		((mi)->windowinfo.window)
#define MI_WIN_WIDTH(mi)	((mi)->windowinfo.win_width)
#define MI_WIN_HEIGHT(mi)	((mi)->windowinfo.win_height)
#define MI_WIN_DEPTH(mi)	((mi)->windowinfo.win_depth)
#define MI_VISUAL(mi)	((mi)->windowinfo.visual)
#define MI_WIN_COLORMAP(mi)	((mi)->windowinfo.colormap)
#define MI_WIN_BLACK_PIXEL(mi)	((mi)->windowinfo.black_pixel)
#define MI_WIN_WHITE_PIXEL(mi)	((mi)->windowinfo.white_pixel)
#define MI_DELTA3D(mi)	((mi)->windowinfo.delta3d)
#define MI_WIN_FLAGS(mi)	((mi)->windowinfo.flags)
#define MI_WIN_SET_FLAG_STATE(mi,f,bool) ((mi)->windowinfo.flags = \
					(bool) ? (mi)->windowinfo.flags | f \
					: (mi)->windowinfo.flags & ~(f))
#define MI_WIN_FLAG_IS_SET(mi,f) ((mi)->windowinfo.flags & f)
#define MI_WIN_FLAG_NOT_SET(mi,f) ( ! MI_WIN_FLAG_IS_SET(mi,f))
#define MI_WIN_IS_ICONIC(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_ICONIC))
#define MI_WIN_IS_MONO(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_MONO))
#define MI_WIN_IS_INWINDOW(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_INWINDOW))
#define MI_WIN_IS_INROOT(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_INROOT))
#define MI_WIN_IS_NOLOCK(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_NOLOCK))
#define MI_WIN_IS_INSTALL(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_INSTALL))
#define MI_WIN_IS_DEBUG(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_DEBUG))
#define MI_WIN_IS_USE3D(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_USE3D))
#define MI_WIN_IS_VERBOSE(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_VERBOSE))
#define MI_WIN_IS_FULLRANDOM(mi) (MI_WIN_FLAG_IS_SET (mi, WI_FLAG_FULLRANDOM))
#define MI_WIN_IS_WIREFRAME(mi)	(MI_WIN_FLAG_IS_SET (mi, WI_FLAG_WIREFRAME))

#define MI_PERSCREEN(mi)	((mi)->screeninfo)
#define MI_GC(mi)		((mi)->screeninfo->gc)
#define MI_NPIXELS(mi)		((mi)->screeninfo->npixels)
#define MI_CMAP(mi)		((mi)->screeninfo->cmap)
#define MI_PIXELS(mi)		((mi)->screeninfo->pixels)
#define MI_PIXEL(mi,n)		((mi)->screeninfo->pixels[n])

/* are these of interest to modes? */
#define MI_BG_COLOR(mi)		((mi)->screeninfo->bgcol)
#define MI_FG_COLOR(mi)		((mi)->screeninfo->fgcol)
#define MI_NONE_COLOR(mi)	((mi)->screeninfo->nonecol)	/* -install */
#define MI_RIGHT_COLOR(mi)	((mi)->screeninfo->rightcol)
#define MI_LEFT_COLOR(mi)	((mi)->screeninfo->leftcol)

#define MI_PAUSE(mi)		((mi)->runinfo.pause)
#define MI_DELAY(mi)		((mi)->runinfo.delay)
#define MI_BATCHCOUNT(mi)	((mi)->runinfo.batchcount)
#define MI_CYCLES(mi)		((mi)->runinfo.cycles)
#define MI_SIZE(mi)		((mi)->runinfo.size)
#define MI_SATURATION(mi)	((mi)->runinfo.saturation)

#define MI_LOCKSTRUCT(mi)	((mi)->lockstruct)
#define MI_DEFDELAY(mi)		((mi)->lockstruct->def_delay)
#define MI_DEFBATCHCOUNT(mi)	((mi)->lockstruct->def_batchcount)
#define MI_DEFCYCLES(mi)	((mi)->lockstruct->def_cycles)
#define MI_DEFSIZE(mi)	((mi)->lockstruct->def_size)
#define MI_DEFSATURATION(mi)	((mi)->lockstruct->def_saturation)

#define MI_NAME(mi)		((mi)->lockstruct->cmdline_arg)
#define MI_DESC(mi)		((mi)->lockstruct->desc)
#define MI_USERDATA(mi)		((mi)->lockstruct->userdata)

/* -------------------------------------------------------------------- */

extern HookProc call_init_hook;
extern HookProc call_callback_hook;
extern HookProc call_release_hook;
extern HookProc call_refresh_hook;
extern HookProc call_change_hook;

extern void set_default_mode(LockStruct *);
extern void release_last_mode(ModeInfo *);

/* -------------------------------------------------------------------- */

extern ModeHook init_ant;
extern ModeHook draw_ant;
extern ModeHook release_ant;
extern ModeHook refresh_ant;
extern ModeSpecOpt ant_opts;

extern ModeHook init_ball;
extern ModeHook draw_ball;
extern ModeHook release_ball;
extern ModeHook refresh_ball;
extern ModeSpecOpt ball_opts;

extern ModeHook init_bat;
extern ModeHook draw_bat;
extern ModeHook release_bat;
extern ModeHook refresh_bat;
extern ModeSpecOpt bat_opts;

extern ModeHook init_blot;
extern ModeHook draw_blot;
extern ModeHook release_blot;
extern ModeHook refresh_blot;
extern ModeSpecOpt blot_opts;

extern ModeHook init_bouboule;
extern ModeHook draw_bouboule;
extern ModeHook release_bouboule;
extern ModeHook refresh_bouboule;
extern ModeSpecOpt bouboule_opts;

extern ModeHook init_bounce;
extern ModeHook draw_bounce;
extern ModeHook release_bounce;
extern ModeHook refresh_bounce;
extern ModeSpecOpt bounce_opts;

extern ModeHook init_braid;
extern ModeHook draw_braid;
extern ModeHook release_braid;
extern ModeHook refresh_braid;
extern ModeSpecOpt braid_opts;

extern ModeHook init_bug;
extern ModeHook draw_bug;
extern ModeHook release_bug;
extern ModeHook refresh_bug;
extern ModeSpecOpt bug_opts;

extern ModeHook init_crystal;
extern ModeHook draw_crystal;
extern ModeHook release_crystal;
extern ModeHook refresh_crystal;
extern ModeSpecOpt crystal_opts;

extern ModeHook init_clock;
extern ModeHook draw_clock;
extern ModeHook release_clock;
extern ModeHook refresh_clock;
extern ModeSpecOpt clock_opts;

extern ModeHook init_daisy;
extern ModeHook draw_daisy;
extern ModeHook release_daisy;
extern ModeHook refresh_daisy;
extern ModeSpecOpt daisy_opts;

extern ModeHook init_dclock;
extern ModeHook draw_dclock;
extern ModeHook release_dclock;
extern ModeHook refresh_dclock;
extern ModeSpecOpt dclock_opts;

extern ModeHook init_demon;
extern ModeHook draw_demon;
extern ModeHook release_demon;
extern ModeHook refresh_demon;
extern ModeSpecOpt demon_opts;

extern ModeHook init_drift;
extern ModeHook draw_drift;
extern ModeHook release_drift;
extern ModeHook refresh_drift;
extern ModeSpecOpt drift_opts;

extern ModeHook init_eyes;
extern ModeHook draw_eyes;
extern ModeHook release_eyes;
extern ModeHook refresh_eyes;
extern ModeSpecOpt eyes_opts;

extern ModeHook init_flag;
extern ModeHook draw_flag;
extern ModeHook release_flag;
extern ModeHook refresh_flag;
extern ModeSpecOpt flag_opts;

extern ModeHook init_flame;
extern ModeHook draw_flame;
extern ModeHook release_flame;
extern ModeHook refresh_flame;
extern ModeSpecOpt flame_opts;

extern ModeHook init_forest;
extern ModeHook draw_forest;
extern ModeHook release_forest;
extern ModeHook refresh_forest;
extern ModeHook refresh_forest;
extern ModeSpecOpt forest_opts;

extern ModeHook init_fract;
extern ModeHook draw_fract;
extern ModeHook release_fract;
extern ModeHook refresh_fract;
extern ModeSpecOpt fract_opts;

extern ModeHook init_galaxy;
extern ModeHook draw_galaxy;
extern ModeHook release_galaxy;
extern ModeHook refresh_galaxy;
extern ModeSpecOpt galaxy_opts;

extern ModeHook init_geometry;
extern ModeHook draw_geometry;
extern ModeHook release_geometry;
extern ModeHook refresh_geometry;
extern ModeSpecOpt geometry_opts;

extern ModeHook init_grav;
extern ModeHook draw_grav;
extern ModeHook release_grav;
extern ModeHook refresh_grav;
extern ModeSpecOpt grav_opts;

extern ModeHook init_helix;
extern ModeHook draw_helix;
extern ModeHook release_helix;
extern ModeHook refresh_helix;
extern ModeSpecOpt helix_opts;

extern ModeHook init_hop;
extern ModeHook draw_hop;
extern ModeHook release_hop;
extern ModeHook refresh_hop;
extern ModeSpecOpt hop_opts;

extern ModeHook init_hyper;
extern ModeHook draw_hyper;
extern ModeHook release_hyper;
extern ModeHook refresh_hyper;
extern ModeSpecOpt hyper_opts;

extern ModeHook init_ico;
extern ModeHook draw_ico;
extern ModeHook release_ico;
extern ModeHook refresh_ico;
extern ModeHook change_ico;
extern ModeSpecOpt ico_opts;

extern ModeHook init_ifs;
extern ModeHook draw_ifs;
extern ModeHook release_ifs;
extern ModeSpecOpt ifs_opts;

extern ModeHook init_image;
extern ModeHook draw_image;
extern ModeHook release_image;
extern ModeHook refresh_image;
extern ModeSpecOpt image_opts;

extern ModeHook init_julia;
extern ModeHook draw_julia;
extern ModeHook release_julia;
extern ModeHook refresh_julia;
extern ModeSpecOpt julia_opts;

extern ModeHook init_kaleid;
extern ModeHook draw_kaleid;
extern ModeHook release_kaleid;
extern ModeHook refresh_kaleid;
extern ModeSpecOpt kaleid_opts;

extern ModeHook init_laser;
extern ModeHook draw_laser;
extern ModeHook release_laser;
extern ModeHook refresh_laser;
extern ModeSpecOpt laser_opts;

extern ModeHook init_life;
extern ModeHook draw_life;
extern ModeHook release_life;
extern ModeHook refresh_life;
extern ModeHook change_life;
extern ModeSpecOpt life_opts;

extern ModeHook init_life1d;
extern ModeHook draw_life1d;
extern ModeHook release_life1d;
extern ModeHook refresh_life1d;
extern ModeSpecOpt life1d_opts;

extern ModeHook init_life3d;
extern ModeHook draw_life3d;
extern ModeHook release_life3d;
extern ModeHook refresh_life3d;
extern ModeHook change_life3d;
extern ModeSpecOpt life3d_opts;

extern ModeHook init_lightning;
extern ModeHook draw_lightning;
extern ModeHook release_lightning;
extern ModeHook refresh_lightning;
extern ModeSpecOpt lightning_opts;

extern ModeHook init_lisa;
extern ModeHook draw_lisa;
extern ModeHook release_lisa;
extern ModeHook refresh_lisa;
extern ModeHook change_lisa;
extern ModeSpecOpt lisa_opts;

extern ModeHook init_lissie;
extern ModeHook draw_lissie;
extern ModeHook release_lissie;
extern ModeHook refresh_lissie;
extern ModeSpecOpt lissie_opts;

extern ModeHook init_loop;
extern ModeHook draw_loop;
extern ModeHook release_loop;
extern ModeHook refresh_loop;
extern ModeSpecOpt loop_opts;

extern ModeHook init_marquee;
extern ModeHook draw_marquee;
extern ModeHook release_marquee;
extern ModeHook refresh_marquee;
extern ModeSpecOpt marquee_opts;

extern ModeHook init_maze;
extern ModeHook draw_maze;
extern ModeHook release_maze;
extern ModeHook refresh_maze;
extern ModeSpecOpt maze_opts;

extern ModeHook init_mountain;
extern ModeHook draw_mountain;
extern ModeHook release_mountain;
extern ModeHook refresh_mountain;
extern ModeSpecOpt mountain_opts;

extern ModeHook init_nose;
extern ModeHook draw_nose;
extern ModeHook release_nose;
extern ModeHook refresh_nose;
extern ModeSpecOpt nose_opts;

extern ModeHook init_pacman;
extern ModeHook draw_pacman;
extern ModeHook release_pacman;
extern ModeHook refresh_pacman;
extern ModeSpecOpt pacman_opts;

extern ModeHook init_penrose;
extern ModeHook draw_penrose;
extern ModeHook release_penrose;

#if 0
extern ModeHook refresh_penrose;	/* Needed */

#endif
extern ModeSpecOpt penrose_opts;

extern ModeHook init_petal;
extern ModeHook draw_petal;
extern ModeHook release_petal;
extern ModeHook refresh_petal;
extern ModeSpecOpt petal_opts;

extern ModeHook init_puzzle;
extern ModeHook draw_puzzle;
extern ModeHook release_puzzle;

#if 0
extern ModeHook refresh_puzzle;	/* Needed */

#endif
extern ModeSpecOpt puzzle_opts;

extern ModeHook init_pyro;
extern ModeHook draw_pyro;
extern ModeHook release_pyro;
extern ModeHook refresh_pyro;
extern ModeSpecOpt pyro_opts;

extern ModeHook init_qix;
extern ModeHook draw_qix;
extern ModeHook release_qix;
extern ModeHook refresh_qix;
extern ModeSpecOpt qix_opts;

extern ModeHook init_roll;
extern ModeHook draw_roll;
extern ModeHook release_roll;
extern ModeHook refresh_roll;
extern ModeSpecOpt roll_opts;

extern ModeHook init_rotor;
extern ModeHook draw_rotor;
extern ModeHook release_rotor;
extern ModeHook refresh_rotor;
extern ModeSpecOpt rotor_opts;

extern ModeHook init_shape;
extern ModeHook draw_shape;
extern ModeHook release_shape;
extern ModeHook refresh_shape;
extern ModeSpecOpt shape_opts;

extern ModeHook init_sierpinski;
extern ModeHook draw_sierpinski;
extern ModeHook release_sierpinski;
extern ModeHook refresh_sierpinski;
extern ModeSpecOpt sierpinski_opts;

extern ModeHook init_slip;
extern ModeHook draw_slip;
extern ModeHook release_slip;

#if 0
extern ModeHook refresh_slip;	/* Probably not practical */

#endif
extern ModeSpecOpt slip_opts;

extern ModeHook init_sphere;
extern ModeHook draw_sphere;
extern ModeHook release_sphere;
extern ModeHook refresh_sphere;
extern ModeSpecOpt sphere_opts;

extern ModeHook init_spiral;
extern ModeHook draw_spiral;
extern ModeHook release_spiral;
extern ModeHook refresh_spiral;
extern ModeSpecOpt spiral_opts;

extern ModeHook init_spline;
extern ModeHook draw_spline;
extern ModeHook release_spline;
extern ModeHook refresh_spline;
extern ModeSpecOpt spline_opts;

extern ModeHook init_star;
extern ModeHook draw_star;
extern ModeHook release_star;
extern ModeHook refresh_star;
extern ModeSpecOpt star_opts;

extern ModeHook init_strange;
extern ModeHook draw_strange;
extern ModeHook release_strange;
extern ModeSpecOpt strange_opts;

extern ModeHook init_swarm;
extern ModeHook draw_swarm;
extern ModeHook release_swarm;
extern ModeHook refresh_swarm;
extern ModeSpecOpt swarm_opts;

extern ModeHook init_swirl;
extern ModeHook draw_swirl;
extern ModeHook release_swirl;
extern ModeHook refresh_swirl;
extern ModeSpecOpt swirl_opts;

extern ModeHook init_triangle;
extern ModeHook draw_triangle;
extern ModeHook release_triangle;
extern ModeHook refresh_triangle;
extern ModeSpecOpt triangle_opts;

extern ModeHook init_tube;
extern ModeHook draw_tube;
extern ModeHook release_tube;
extern ModeHook refresh_tube;
extern ModeSpecOpt tube_opts;

extern ModeHook init_turtle;
extern ModeHook draw_turtle;
extern ModeHook release_turtle;
extern ModeHook refresh_turtle;
extern ModeSpecOpt turtle_opts;

extern ModeHook init_voters;
extern ModeHook draw_voters;
extern ModeHook release_voters;
extern ModeHook refresh_voters;
extern ModeSpecOpt voters_opts;

extern ModeHook init_wator;
extern ModeHook draw_wator;
extern ModeHook release_wator;
extern ModeHook refresh_wator;
extern ModeSpecOpt wator_opts;

extern ModeHook init_wire;
extern ModeHook draw_wire;
extern ModeHook release_wire;
extern ModeHook refresh_wire;
extern ModeSpecOpt wire_opts;

extern ModeHook init_world;
extern ModeHook draw_world;
extern ModeHook release_world;
extern ModeHook refresh_world;
extern ModeSpecOpt world_opts;

extern ModeHook init_worm;
extern ModeHook draw_worm;
extern ModeHook release_worm;
extern ModeHook refresh_worm;
extern ModeSpecOpt worm_opts;

#if defined( USE_XPM ) || defined( USE_XPMINC )
extern ModeHook init_cartoon;
extern ModeHook draw_cartoon;
extern ModeHook release_cartoon;
extern ModeSpecOpt cartoon_opts;

#endif

#ifdef USE_GL
extern ModeHook init_escher;
extern ModeHook draw_escher;
extern ModeHook release_escher;
extern ModeHook change_escher;
extern ModeSpecOpt escher_opts;

extern ModeHook init_gears;
extern ModeHook draw_gears;
extern ModeHook release_gears;
extern ModeSpecOpt gears_opts;

extern ModeHook init_morph3d;
extern ModeHook draw_morph3d;
extern ModeHook release_morph3d;
extern ModeHook change_morph3d;
extern ModeSpecOpt morph3d_opts;

extern ModeHook init_pipes;
extern ModeHook draw_pipes;
extern ModeHook release_pipes;
extern ModeHook refresh_pipes;
extern ModeHook change_pipes;
extern ModeSpecOpt pipes_opts;

extern ModeHook init_sproingies;
extern ModeHook draw_sproingies;
extern ModeHook release_sproingies;
extern ModeHook refresh_sproingies;
extern ModeSpecOpt sproingies_opts;

extern ModeHook init_superquadrics;
extern ModeHook draw_superquadrics;
extern ModeHook release_superquadrics;
extern ModeHook refresh_superquadrics;
extern ModeSpecOpt superquadrics_opts;

#endif

#ifdef USE_HACKERS
extern ModeHook init_fadeplot;
extern ModeHook draw_fadeplot;
extern ModeHook release_fadeplot;
extern ModeHook refresh_fadeplot;
extern ModeSpecOpt fadeplot_opts;

#endif

extern ModeHook init_blank;
extern ModeHook draw_blank;
extern ModeHook release_blank;
extern ModeHook refresh_blank;
extern ModeSpecOpt blank_opts;

#ifdef USE_BOMB
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
extern int  numprocs;

/* -------------------------------------------------------------------- */

#endif /* __XLOCK_MODE_H__ */
