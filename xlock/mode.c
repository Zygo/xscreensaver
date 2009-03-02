#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)mode.c	4.02 97/04/01 xlockmore";

#endif
/*-
 * mode.c - Modes for xlock. 
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 01-Apr-97: Fixed memory leak in XStringListToTextProperty.
 * 23-Feb-96: Extensive revision to implement new mode hooks and stuff
 *		Ron Hitchens <ron@idiom.com>
 * 04-Sep-95: Moved over from mode.h (previously resource.h) with new
 *            "&*_opts" by Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 *
 */

#include "xlock.h"
#include <ctype.h>

/* -------------------------------------------------------------------- */

/*-
 * Mode options: If batchcount, cycles, or size options are set to 1 ...
 * they are probably not used by the mode.
 */

LockStruct  LockProcs[] =
{
	{"ant", init_ant, draw_ant, release_ant,
	 refresh_ant, init_ant, NULL, &ant_opts,
	 1000, -3, 40000, -7, 1.0,
	 "Shows Langton's and Turk's generalized ants", 0, NULL},
	{"ball", init_ball, draw_ball, release_ball,
	 refresh_ball, init_ball, NULL, &ball_opts,
	 10000, 10, 20, -100, 1.0,
	 "Shows bouncing balls", 0, NULL},
	{"bat", init_bat, draw_bat, release_bat,
	 refresh_bat, init_bat, NULL, &bat_opts,
	 100000, -8, 1, 0, 1.0,
	 "Shows bouncing flying bats", 0, NULL},
	{"blot", init_blot, draw_blot, release_blot,
	 refresh_blot, init_blot, NULL, &blot_opts,
	 200000, 6, 30, 1, 0.4,
	 "Shows Rorschach's ink blot test", 0, NULL},
	{"bouboule", init_bouboule, draw_bouboule, release_bouboule,
	 refresh_bouboule, init_bouboule, NULL, &bouboule_opts,
	 5000, 100, 1, 15, 1.0,
	 "Shows Mimi's bouboule of moving stars", 0, NULL},
	{"bounce", init_bounce, draw_bounce, release_bounce,
	 refresh_bounce, init_bounce, NULL, &bounce_opts,
	 10000, -10, 1, 0, 1.0,
	 "Shows bouncing footballs", 0, NULL},
	{"braid", init_braid, draw_braid, release_braid,
	 refresh_braid, init_braid, NULL, &braid_opts,
	 1000, 15, 100, 1, 1.0,
	 "Shows random braids and knots", 0, NULL},
	{"bug", init_bug, draw_bug, release_bug,
	 refresh_bug, init_bug, NULL, &bug_opts,
	 75000, 10, 32767, -4, 1.0,
	 "Shows Palmiter's bug evolution and garden of Eden", 0, NULL},
#if defined( USE_XPM ) || defined( USE_XPMINC )
	{"cartoon", init_cartoon, draw_cartoon, release_cartoon,
	 NULL, init_cartoon, NULL, &cartoon_opts,
	 10000, 1, 1, 1, 1.0,
	 "Shows bouncing cartoons", 0, NULL},
#endif
	{"clock", init_clock, draw_clock, release_clock,
	 refresh_clock, init_clock, NULL, &clock_opts,
	 100000, -16, 200, -200, 1.0,
	 "Shows Packard's clock", 0, NULL},
	{"crystal", init_crystal, draw_crystal, release_crystal,
	 refresh_crystal, init_crystal, NULL, &crystal_opts,
	 60000, -40, 200, -15, 1.0,
	 "Shows polygons in 2D plane groups", 0, NULL},
	{"daisy", init_daisy, draw_daisy, release_daisy,
	 refresh_daisy, init_daisy, NULL, &daisy_opts,
	 100000, 300, 350, 1, 1.0,
	 "Shows a meadow of daisies", 0, NULL},
	{"dclock", init_dclock, draw_dclock, release_dclock,
	 refresh_dclock, init_dclock, NULL, &dclock_opts,
	 10000, 1, 10000, 1, 0.2,
	 "Shows a floating digital clock", 0, NULL},
	{"demon", init_demon, draw_demon, release_demon,
	 refresh_demon, init_demon, NULL, &demon_opts,
	 50000, 0, 1000, -7, 1.0,
	 "Shows Griffeath's cellular automata", 0, NULL},
	{"drift", init_drift, draw_drift, release_drift,
	 refresh_drift, init_drift, NULL, &drift_opts,
	 10000, 30, 1, 1, 1.0,
	 "Shows cosmic drifting flame fractals", 0, NULL},
#ifdef USE_GL
	{"escher", init_escher, draw_escher, release_escher,
	 draw_escher, change_escher, NULL, &escher_opts,
	 1000, 0, 1, 1, 1.0,
	 "Shows some Escher like scenes", 0, NULL},
#endif
	{"eyes", init_eyes, draw_eyes, release_eyes,
	 refresh_eyes, init_eyes, NULL, &eyes_opts,
	 20000, -8, 5, 1, 1.0,
	 "Shows eyes following a bouncing grelb", 0, NULL},
	{"fadeplot", init_fadeplot, draw_fadeplot, release_fadeplot,
	 refresh_fadeplot, init_fadeplot, NULL, &fadeplot_opts,
	 30000, 10, 1500, 1, 0.6,
	 "Shows a fading plot of sine squared", 0, NULL},
	{"flag", init_flag, draw_flag, release_flag,
	 refresh_flag, init_flag, NULL, &flag_opts,
	 50000, 1, 1000, -7, 1.0,
	 "Shows a waving flag of your operating system", 0, NULL},
	{"flame", init_flame, draw_flame, release_flame,
	 refresh_flame, init_flame, NULL, &flame_opts,
	 750000, 20, 10000, 1, 1.0,
	 "Shows cosmic flame fractals", 0, NULL},
	{"forest", init_forest, draw_forest, release_forest,
	 refresh_forest, init_forest, NULL, &forest_opts,
	 400000, 100, 200, 1, 1.0,
	 "Shows binary trees of a fractal forest", 0, NULL},
	{"galaxy", init_galaxy, draw_galaxy, release_galaxy,
	 refresh_galaxy, init_galaxy, NULL, &galaxy_opts,
	 100, -5, 250, -3, 1.0,
	 "Shows crashing spiral galaxies", 0, NULL},
#ifdef USE_GL
	{"gears", init_gears, draw_gears, release_gears,
	 draw_gears, init_gears, NULL, &gears_opts,
	 1000, 1, 2, 1, 1.0,
	 "Shows GL's gears", 0, NULL},
#endif
	{"geometry", init_geometry, draw_geometry, release_geometry,
	 refresh_geometry, init_geometry, NULL, &geometry_opts,
	 10000, -10, 1, 1, 1.0,
	 "Shows morphing of a complete graph", 0, NULL},
	{"grav", init_grav, draw_grav, release_grav,
	 refresh_grav, init_grav, NULL, &grav_opts,
	 10000, -12, 1, 1, 1.0,
	 "Shows orbiting planets", 0, NULL},
	{"helix", init_helix, draw_helix, release_helix,
	 refresh_helix, init_helix, NULL, &helix_opts,
	 25000, 1, 100, 1, 1.0,
	 "Shows string art", 0, NULL},
	{"hop", init_hop, draw_hop, release_hop,
	 refresh_hop, init_hop, NULL, &hop_opts,
	 10000, 1000, 2500, 1, 1.0,
	 "Shows real plane iterated fractals", 0, NULL},
	{"hyper", init_hyper, draw_hyper, release_hyper,
	 refresh_hyper, init_hyper, NULL, &hyper_opts,
	 10000, 1, 300, 1, 1.0,
	 "Shows a spinning tesseract in 4D space", 0, NULL},
	{"ico", init_ico, draw_ico, release_ico,
	 refresh_ico, change_ico, NULL, &ico_opts,
	 100000, 0, 400, 0, 1.0,
	 "Shows a bouncing polyhedra", 0, NULL},
	{"ifs", init_ifs, draw_ifs, release_ifs,
	 init_ifs, init_ifs, NULL, &ifs_opts,
	 1000, 1, 1, 1, 1.0,
	 "Shows a modified iterated function system", 0, NULL},
	{"image", init_image, draw_image, release_image,
	 refresh_image, init_image, NULL, &image_opts,
	 2000000, -10, 1, 1, 1.0,
	 "Shows randomly appearing logos", 0, NULL},
	{"julia", init_julia, draw_julia, release_julia,
	 refresh_julia, init_julia, NULL, &julia_opts,
	 10000, 1000, 20, 1, 1.0,
	 "Shows the Julia set", 0, NULL},
	{"kaleid", init_kaleid, draw_kaleid, release_kaleid,
	 refresh_kaleid, init_kaleid, NULL, &kaleid_opts,
	 20000, 1, 700, 1, 1.0,
	 "Shows a kaleidoscope", 0, NULL},
	{"laser", init_laser, draw_laser, release_laser,
	 refresh_laser, init_laser, NULL, &laser_opts,
	 20000, -10, 200, 1, 1.0,
	 "Shows spinning lasers", 0, NULL},
	{"life", init_life, draw_life, release_life,
	 refresh_life, change_life, NULL, &life_opts,
	 750000, 40, 140, 0, 1.0,
	 "Shows Conway's game of Life", 0, NULL},
	{"life1d", init_life1d, draw_life1d, release_life1d,
	 refresh_life1d, init_life1d, NULL, &life1d_opts,
	 10000, 1, 10, 0, 1.0,
	 "Shows Wolfram's game of 1D Life", 0, NULL},
	{"life3d", init_life3d, draw_life3d, release_life3d,
	 refresh_life3d, change_life3d, NULL, &life3d_opts,
	 1000000, 35, 85, 1, 1.0,
	 "Shows Bays' game of 3D Life", 0, NULL},
	{"lightning", init_lightning, draw_lightning, release_lightning,
	 refresh_lightning, init_lightning, NULL, &lightning_opts,
	 10000, 1, 1, 1, 0.6,
	 "Shows Keith's fractal lightning bolts", 0, NULL},
	{"lisa", init_lisa, draw_lisa, release_lisa, refresh_lisa,
	 change_lisa, NULL, &lisa_opts,
	 25000, 1, 256, -1, 1.0,
	 "Shows animated lisajous loops", 0, NULL},
	{"lissie", init_lissie, draw_lissie, release_lissie,
	 refresh_lissie, init_lissie, NULL, &lissie_opts,
	 10000, -2, 2000, -200, 0.6,
	 "Shows lissajous worms", 0, NULL},
	{"loop", init_loop, draw_loop, release_loop,
	 refresh_loop, init_loop, NULL, &loop_opts,
	 100000, 1, 1600, -12, 1.0,
	 "Shows Langton's self-producing loops", 0, NULL},
	{"marquee", init_marquee, draw_marquee, release_marquee,
	 refresh_marquee, init_marquee, NULL, &marquee_opts,
	 100000, 1, 1, 1, 1.0,
	 "Shows messages", 0, NULL},
	{"maze", init_maze, draw_maze, release_maze,
	 refresh_maze, init_maze, NULL, &maze_opts,
	 1000, 1, 300, -40, 1.0,
	 "Shows a random maze and a depth first search solution", 0, NULL},
#ifdef USE_GL
	{"morph3d", init_morph3d, draw_morph3d, release_morph3d,
	 draw_morph3d, change_morph3d, NULL, &morph3d_opts,
	 1000, 0, 1, 1, 1.0,
	 "Shows GL morphing polyhedra", 0, NULL},
#endif
	{"mountain", init_mountain, draw_mountain, release_mountain,
	 refresh_mountain, init_mountain, NULL, &mountain_opts,
	 1000, 30, 100, 1, 1.0,
	 "Shows Papo's mountain range", 0, NULL},
	{"munch", init_munch, draw_munch, release_munch,
	 init_munch, init_munch, NULL, &munch_opts,
	 5000, 1, 7, 1, 1.0,
	 "Shows munching squares", 0, NULL},
	{"nose", init_nose, draw_nose, release_nose,
	 refresh_nose, init_nose, NULL, &nose_opts,
	 100000, 1, 1, 1, 1.0,
    "Shows a man with a big nose runs around spewing out messages", 0, NULL},
	{"pacman", init_pacman, draw_pacman, release_pacman,
	 refresh_pacman, init_pacman, NULL, &pacman_opts,
	 100000, 10, 1, 0, 1.0,
	 "Shows Pacman(tm)", 0, NULL},
	{"penrose", init_penrose, draw_penrose, release_penrose,
	 init_penrose, init_penrose, NULL, &penrose_opts,
	 10000, 1, 1, -40, 1.0,
	 "Shows Penrose's quasiperiodic tilings", 0, NULL},
	{"petal", init_petal, draw_petal, release_petal,
	 refresh_petal, init_petal, NULL, &petal_opts,
	 10000, -500, 400, 1, 1.0,
	 "Shows various GCD Flowers", 0, NULL},
#ifdef USE_GL
	{"pipes", init_pipes, draw_pipes, release_pipes,
#if defined( MESA ) && defined( SLOW )
	 draw_pipes,
#else
	 change_pipes,
#endif
	 change_pipes, NULL, &pipes_opts,
	 1000, 2, 5, 500, 1.0,
	 "Shows a selfbuilding pipe system", 0, NULL},
#endif
	{"puzzle", init_puzzle, draw_puzzle, release_puzzle,
	 init_puzzle, init_puzzle, NULL, &puzzle_opts,
	 10000, 250, 1, 1, 1.0,
	 "Shows a puzzle being scrambled and then solved", 0, NULL},
	{"pyro", init_pyro, draw_pyro, release_pyro,
	 refresh_pyro, init_pyro, NULL, &pyro_opts,
	 15000, 100, 1, -3, 1.0,
	 "Shows fireworks", 0, NULL},
	{"qix", init_qix, draw_qix, release_qix,
	 refresh_qix, init_qix, NULL, &qix_opts,
	 30000, 1, 64, 1, 1.0,
	 "Shows spinning lines a la Qix(tm)", 0, NULL},
	{"roll", init_roll, draw_roll, release_roll,
	 refresh_roll, init_roll, NULL, &roll_opts,
	 100000, 25, 1, -64, 0.6,
	 "Shows a rolling ball", 0, NULL},
	{"rotor", init_rotor, draw_rotor, release_rotor,
	 refresh_rotor, init_rotor, NULL, &rotor_opts,
	 10000, 4, 20, 1, 0.4,
	 "Shows Tom's Roto-Rooter", 0, NULL},
#ifdef USE_GL
	{"rubik", init_rubik, draw_rubik, release_rubik,
	 draw_rubik, change_rubik, NULL, &rubik_opts,
	 1000, -30, 5, -6, 1.0,
	 "Shows an auto-solving Rubik's Cube", 0, NULL},
#endif
	{"shape", init_shape, draw_shape, release_shape,
	 refresh_shape, init_shape, NULL, &shape_opts,
	 10000, 100, 256, 1, 1.0,
	 "Shows stippled rectangles, ellipses, and triangles", 0, NULL},
	{"sierpinski", init_sierpinski, draw_sierpinski, release_sierpinski,
	 refresh_sierpinski, init_sierpinski, NULL, &sierpinski_opts,
	 400000, 2000, 100, 1, 1.0,
	 "Shows Sierpinski's triangle", 0, NULL},
	{"slip", init_slip, draw_slip, release_slip,
	 init_slip, init_slip, NULL, &slip_opts,
	 50000, 35, 50, 1, 1.0,
	 "Shows slipping blits", 0, NULL},
	{"sphere", init_sphere, draw_sphere, release_sphere,
	 refresh_sphere, init_sphere, NULL, &sphere_opts,
	 10000, 1, 20, 0, 1.0,
	 "Shows a bunch of shaded spheres", 0, NULL},
	{"spiral", init_spiral, draw_spiral, release_spiral,
	 refresh_spiral, init_spiral, NULL, &spiral_opts,
	 5000, -40, 350, 1, 1.0,
	 "Shows helixes of dots", 0, NULL},
	{"spline", init_spline, draw_spline, release_spline,
	 refresh_spline, init_spline, NULL, &spline_opts,
	 30000, -6, 2048, 1, 0.4,
	 "Shows colorful moving splines", 0, NULL},
#ifdef USE_GL
	{"sproingies", init_sproingies, draw_sproingies, release_sproingies,
	 refresh_sproingies, init_sproingies, NULL, &sproingies_opts,
	 1000, 5, 0, 400, 1.0,
  "Shows Sproingies!  Nontoxic.  Safe for pets and small children", 0, NULL},
#endif
	{"star", init_star, draw_star, release_star,
	 refresh_star, init_star, NULL, &star_opts,
	 40000, 100, 1, 100, 0.2,
	 "Shows a star field with a twist", 0, NULL},
	{"strange", init_strange, draw_strange, release_strange,
	 init_strange, init_strange, NULL, &strange_opts,
	 1000, 1, 1, 1, 1.0,
	 "Shows strange attractors", 0, NULL},
#ifdef USE_GL
	{"superquadrics", init_superquadrics, draw_superquadrics, release_superquadrics,
	 refresh_superquadrics, init_superquadrics, NULL, &superquadrics_opts,
	 1000, 25, 40, 1, 1.0,
	 "Shows 3D mathematical shapes", 0, NULL},
#endif
	{"swarm", init_swarm, draw_swarm, release_swarm,
	 refresh_swarm, init_swarm, NULL, &swarm_opts,
	 10000, 100, 1, 1, 1.0,
	 "Shows a swarm of bees following a wasp", 0, NULL},
	{"swirl", init_swirl, draw_swirl, release_swirl,
	 refresh_swirl, init_swirl, NULL, &swirl_opts,
	 5000, 5, 1, 1, 1.0,
	 "Shows animated swirling patterns", 0, NULL},
	{"triangle", init_triangle, draw_triangle, release_triangle,
	 refresh_triangle, init_triangle, NULL, &triangle_opts,
	 10000, 1, 1, 1, 1.0,
	 "Shows a triangle mountain range", 0, NULL},
	{"tube", init_tube, draw_tube, release_tube,
	 NULL, init_tube, NULL, &tube_opts,
	 25000, -9, 20000, -200, 1.0,
	 "Shows an animated tube", 0, NULL},
	{"turtle", init_turtle, draw_turtle, release_turtle,
	 init_turtle, init_turtle, NULL, &turtle_opts,
	 1000000, 1, 20, 1, 1.0,
	 "Shows turtle fractals", 0, NULL},
	{"vines", init_vines, draw_vines, release_vines,
	 refresh_vines, init_vines, NULL, &vines_opts,
	 200000, 0, 1, 1, 1.0,
	 "Shows fractals", 0, NULL},
	{"voters", init_voters, draw_voters, release_voters,
	 refresh_voters, init_voters, NULL, &voters_opts,
	 1000, 0, 327670, 0, 1.0,
	 "Shows Dewdney's Voters", 0, NULL},
	{"wator", init_wator, draw_wator, release_wator,
	 refresh_wator, init_wator, NULL, &wator_opts,
	 750000, 1, 32767, 0, 1.0,
	 "Shows Dewdney's Water-Torus planet of fish and sharks", 0, NULL},
	{"wire", init_wire, draw_wire, release_wire,
	 refresh_wire, init_wire, NULL, &wire_opts,
	 500000, 1000, 150, -8, 1.0,
	 "Shows a random circuit with 2 electrons", 0, NULL},
	{"world", init_world, draw_world, release_world,
	 refresh_world, init_world, NULL, &world_opts,
	 100000, -16, 1, 1, 0.3,
	 "Shows spinning Earths", 0, NULL},
	{"worm", init_worm, draw_worm, release_worm,
	 refresh_worm, init_worm, NULL, &worm_opts,
	 17000, -20, 10, -3, 1.0,
	 "Shows wiggly worms", 0, NULL},

/* SPECIAL MODES */
	{"blank", init_blank, draw_blank, release_blank,
	 refresh_blank, init_blank, NULL, &blank_opts,
	 3000000, 1, 1, 1, 1.0,
	 "Shows nothing but a black screen", 0, NULL},
#ifdef USE_BOMB
	{"bomb", init_bomb, draw_bomb, release_bomb,
	 refresh_bomb, change_bomb, NULL, &bomb_opts,
	 100000, 10, 20, 1, 1.0,
	 "Shows a bomb and will autologout after a time", 0, NULL},
	{"random", init_random, draw_random, release_random,
	 refresh_random, change_random, NULL, &random_opts,
	 1, 1, 1, 1, 1.0,
	 "Shows a random mode from above except blank and bomb", 0, NULL},
#else
	{"random", init_random, draw_random, release_random,
	 refresh_random, change_random, NULL, &random_opts,
	 1, 1, 1, 1, 1.0,
	 "Shows a random mode from above except blank", 0, NULL},
#endif
};

int         numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

/* -------------------------------------------------------------------- */

static LockStruct *last_initted_mode = NULL;
static LockStruct *default_mode = NULL;

/* -------------------------------------------------------------------- */

void
set_default_mode(LockStruct * ls)
{
	default_mode = ls;
}

/* -------------------------------------------------------------------- */

static void
set_window_title(ModeInfo * mi)
{
	XTextProperty prop;
	char        buf[512];
	char       *ptr = buf;
	unsigned int status;

	(void) sprintf(buf, "%s: %s", MI_NAME(mi), MI_DESC(mi));
	status = XStringListToTextProperty(&ptr, 1, &prop);
	if (status != 0) {
		XSetWMName(MI_DISPLAY(mi), MI_WINDOW(mi), &prop);
		XFree((caddr_t) prop.value);
	}
}

/* -------------------------------------------------------------------- */

/*-
 *    This hook is called prior to calling the init hook of a
 *      different mode.  It is to inform the mode that it is losing
 *      control, and should therefore release any dynamically created
 *      resources.
 */

void
call_release_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		return;
	}
	MI_LOCKSTRUCT(mi) = ls;
	if (ls->release_hook != NULL) {
		ls->release_hook(mi);
	}
	ls->flags &= ~(LS_FLAG_INITED);

	last_initted_mode = NULL;
}

void
release_last_mode(ModeInfo * mi)
{
	if (last_initted_mode == NULL) {
		return;
	}
	call_release_hook(last_initted_mode, mi);

	last_initted_mode = NULL;
}

/* -------------------------------------------------------------------- */

/*-
 *    Call the init hook for a mode.  If this mode is not the same
 *      as the last one, call the release proc for the previous mode
 *      so that it will surrender its dynamic resources.
 *      A mode's init hook may be called multiple times, without
 *      intervening release calls.
 */

void
call_init_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	if (ls != last_initted_mode) {
		call_release_hook(last_initted_mode, mi);
	}
	MI_LOCKSTRUCT(mi) = ls;
	set_window_title(mi);

	ls->init_hook(mi);

	ls->flags |= LS_FLAG_INITED;
	MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_JUST_INITTED, True);

	last_initted_mode = ls;
}

/* -------------------------------------------------------------------- */

/*-
 *    Call the callback hook for a mode.  This hook is called repeatedly,
 *      at (approximately) constant time intervals.  The time between calls
 *      is controlled by the -delay command line option, which is mapped
 *      to the variable named delay.
 */

void
call_callback_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	MI_LOCKSTRUCT(mi) = ls;

	ls->callback_hook(mi);

	MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_JUST_INITTED, False);
}

/* -------------------------------------------------------------------- */

/*-
 *    Window damage has occurred.  If the mode has been initted and
 *      supplied a refresh proc, call that.  Otherwise call its init
 *      hook again.
 */

#define JUST_INITTED(mi)	(MI_WIN_FLAG_IS_SET(mi, WI_FLAG_JUST_INITTED))

void
call_refresh_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	MI_LOCKSTRUCT(mi) = ls;

	if (ls->refresh_hook == NULL) {
		/*
		 * No refresh hook supplied.  If the mode has been
		 * initialized, and the callback has been called at least
		 * once, then call the init hook to do the refresh.
		 * Note that two flags are examined here.  The first
		 * indicates if the mode has ever had its init hook called,
		 * the second is a per-screen flag which indicates
		 * if the draw (callback) hook has been called since the
		 * init hook was called for that screen.
		 * This second test is a hack.  A mode should gracefully
		 * deal with its init hook being called twice in a row.
		 * Once all the modes have been updated, this hack should
		 * be removed.
		 */
		if (MODE_NOT_INITED(ls) || JUST_INITTED(mi)) {
			return;
		}
		call_init_hook(ls, mi);
	} else {
		ls->refresh_hook(mi);
	}
}

/* -------------------------------------------------------------------- */

/*-
 *    The user has requested a change, by pressing the middle mouse
 *      button.  Let the mode know about it.
 */

void
call_change_hook(LockStruct * ls, ModeInfo * mi)
{
	if (ls == NULL) {
		if (default_mode == NULL) {
			return;
		} else {
			ls = default_mode;
		}
	}
	MI_LOCKSTRUCT(mi) = ls;

	if (ls->change_hook != NULL) {
		ls->change_hook(mi);
	}
}

/* -------------------------------------------------------------------- */
