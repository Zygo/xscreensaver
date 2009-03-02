/*-
 * @(#)mode.h 4.00 97/01/01 xlockmore
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
 *    Extensive revision to define new data types for
 *    the new mode calling scheme.
 * 02-Jun-95: Extracted out of resource.c.
 *
 */

/*-
 * Declare external interface routines for supported screen savers.
 */

/* -------------------------------------------------------------------- */

#define USE_XPM
#define USE_GL
#define USE_UNSTABLE
#define USE_BOMB

typedef struct {
	int         dummy;
} ModeSpecOpt;

struct LockStruct_s;
struct ModeInfo_s;

typedef void (ModeHook) (struct ModeInfo_s *);
typedef void (HookProc) (struct LockStruct_s *, struct ModeInfo_s *);

typedef struct LockStruct_s {
	char       *cmdline_arg;	/* mode name */
#if 0
	char       *init_hook;	/* func to init a mode */
	char       *callback_hook;	/* func to run (tick) a mode */
	char       *release_hook;	/* func to shutdown a mode */
	char       *refresh_hook;	/* tells mode to repaint */
	char       *change_hook;	/* user wants mode to change */
	char       *unused_hook;	/* for future expansion */
	ModeSpecOpt *msopt;	/* this mode's def resources */
#endif
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
	char       *define;
} LockStruct;

LockStruct  LockProcs[] =
{
	{"ant",
	 1000, -3, 40000, -7, 64, 1.0, "",
	 "Shows Langton's and Turk's generalized ants", 0, NULL, NULL},
#ifdef USE_GL
	{"atlantis",
	 1000, 4, 100, 6000, 64, 1.0, "",
	 "Shows moving sharks/whales/dolphin", 0, NULL, "#ifdef USE_GL"},
#endif
	{"ball",
	 10000, 10, 20, -100, 64, 1.0, "",
	 "Shows bouncing balls", 0, NULL, NULL},
	{"bat",
	 100000, -8, 1, 0, 64, 1.0, "",
	 "Shows bouncing flying bats", 0, NULL, NULL},
	{"blot",
	 200000, 6, 30, 1, 64, 0.3, "",
	 "Shows Rorschach's ink blot test", 0, NULL, NULL},
	{"bouboule",
	 10000, 100, 1, 15, 64, 1.0, "",
	 "Shows Mimi's bouboule of moving stars", 0, NULL, NULL},
	{"bounce",
	 5000, -10, 1, 0, 64, 1.0, "",
	 "Shows bouncing footballs", 0, NULL, NULL},
	{"braid",
	 1000, 15, 100, -7, 64, 1.0, "",
	 "Shows random braids and knots", 0, NULL, NULL},
	{"bubble",
	 100000, 25, 1, 100, 64, 0.6, "",
	 "Shows popping bubbles", 0, NULL, NULL},
#ifdef USE_GL
	{"bubble3d",
	 1000, 1, 2, 1, 64, 1.0, "",
	 "Richard Jones's GL bubbles", 0, NULL, NULL},
#endif
	{"bug",
	 75000, 10, 32767, -4, 64, 1.0, "",
	 "Shows Palmiter's bug evolution and garden of Eden", 0, NULL, NULL},
#ifdef USE_GL
	{"cage",
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows the Impossible Cage, an Escher-like GL scene", 0, NULL, "#ifdef USE_GL"},
#endif
#if defined( USE_XPM ) || defined( USE_XPMINC )
	{"cartoon",
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows bouncing cartoons", 0, NULL, "#if defined( USE_XPM ) || defined( USE_XPMINC )"},
#endif
	{"clock",
	 100000, -16, 200, -200, 64, 1.0, "",
	 "Shows Packard's clock", 0, NULL, NULL},
	{"coral",
	 60000, -3, 1, 35, 64, 0.6, "",
	 "Shows a coral reef", 0, NULL, NULL},
	{"crystal",
	 60000, -500, 200, -15, 64, 1.0, "",
	 "Shows polygons in 2D plane groups", 0, NULL, NULL},
	{"daisy",
	 100000, 300, 350, 1, 64, 1.0, "",
	 "Shows a meadow of daisies", 0, NULL, NULL},
	{"dclock",
	 10000, 1, 10000, 1, 64, 0.3, "",
	 "Shows a floating digital clock", 0, NULL, NULL},
	{"deco",
	 1000000, -30, 2, -10, 64, 0.6, "",
	 "Shows art as ugly as sin", 0, NULL, NULL},
	{"demon",
	 50000, 0, 1000, -7, 64, 1.0, "",
	 "Shows Griffeath's cellular automata", 0, NULL, NULL},
	{"dilemma",
	 200000, -2, 1000, 0, 64, 1.0, "",
	 "Shows Lloyd's Prisoner's Dilemma simulation", 0, NULL, NULL},
	{"discrete",
	 1000, 4096, 2500, 1, 64, 1.0, "",
	 "Shows various discrete maps", 0, NULL, NULL},
	{"drift",
	 10000, 30, 1, 1, 64, 1.0, "",
	 "Shows cosmic drifting flame fractals", 0, NULL, NULL},
	{"eyes",
	 20000, -8, 5, 1, 64, 1.0, "",
	 "Shows eyes following a bouncing grelb", 0, NULL, NULL},
	{"fadeplot",
	 30000, 10, 1500, 1, 64, 0.6, "",
	 "Shows a fading plot of sine squared", 0, NULL, NULL},
	{"flag",
	 50000, 1, 1000, -7, 64, 1.0, "",
	 "Shows a waving flag of your operating system", 0, NULL, NULL},
	{"flame",
	 750000, 20, 10000, 1, 64, 1.0, "",
	 "Shows cosmic flame fractals", 0, NULL, NULL},
	{"flow",
	 1000, 1024, 3000, 1, 64, 1.0, "",
	 "Shows dynamic strange attractors", 0, NULL, NULL},
	{"forest",
	 400000, 100, 200, 1, 64, 1.0, "",
	 "Shows binary trees of a fractal forest", 0, NULL, NULL},
	{"galaxy",
	 100, -5, 250, -3, 64, 1.0, "",
	 "Shows crashing spiral galaxies", 0, NULL, NULL},
#ifdef USE_GL
	{"gears",
	 1000, 1, 2, 1, 64, 1.0, "",
	 "Shows GL's gears", 0, NULL, "#ifdef USE_GL"},
#endif
	{"goop",
	 10000, -12, 1, 1, 64, 1.0, "",
	 "Shows goop from a lava lamp", 0, NULL, NULL},
	{"grav",
	 10000, -12, 1, 1, 64, 1.0, "",
	 "Shows orbiting planets", 0, NULL, NULL},
	{"helix",
	 25000, 1, 100, 1, 64, 1.0, "",
	 "Shows string art", 0, NULL, NULL},
	{"hop",
	 10000, 1000, 2500, 1, 64, 1.0, "",
	 "Shows real plane iterated fractals", 0, NULL, NULL},
	{"hyper",
	 100000, -6, 300, 1, 64, 1.0, "",
	 "Shows spinning n-dimensional hypercubes", 0, NULL, NULL},
	{"ico",
	 100000, 0, 400, 0, 64, 1.0, "",
	 "Shows a bouncing polyhedron", 0, NULL, NULL},
	{"ifs",
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows a modified iterated function system", 0, NULL, NULL},
	{"image",
	 2000000, -10, 1, 1, 64, 1.0, "",
	 "Shows randomly appearing logos", 0, NULL, NULL},
	{"julia",
	 10000, 1000, 20, 1, 64, 1.0, "",
	 "Shows the Julia set", 0, NULL, NULL},
	{"kaleid",
	 80000, 4, 40, -9, 64, 0.6, "",
	 "Shows a kaleidoscope", 0, NULL, NULL},
	{"kumppa",
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows kumppa", 0, NULL, NULL},
	{"laser",
	 20000, -10, 200, 1, 64, 1.0, "",
	 "Shows spinning lasers", 0, NULL, NULL},
	{"life",
	 750000, 40, 140, 0, 64, 1.0, "",
	 "Shows Conway's game of Life", 0, NULL, NULL},
	{"life1d",
	 10000, 1, 10, 0, 64, 1.0, "",
	 "Shows Wolfram's game of 1D Life", 0, NULL, NULL},
	{"life3d",
	 1000000, 35, 85, 1, 64, 1.0, "",
	 "Shows Bays' game of 3D Life", 0, NULL, NULL},
	{"lightning",
	 10000, 1, 1, 1, 64, 0.6, "",
	 "Shows Keith's fractal lightning bolts", 0, NULL, NULL},
	{"lisa",
	 25000, 1, 256, -1, 64, 1.0, "",
	 "Shows animated lisajous loops", 0, NULL, NULL},
	{"lissie",
	 10000, 1, 2000, -200, 64, 0.6, "",
	 "Shows lissajous worms", 0, NULL, NULL},
	{"loop",
	 100000, 1, 1600, -12, 64, 1.0, "",
	 "Shows Langton's self-producing loops", 0, NULL, NULL},
	{"mandelbrot",
	 25000, -8, 20000, 1, 64, 1.0, "",
	 "Shows mandelbrot sets", 0, NULL, NULL},
	{"marquee",
	 100000, 1, 1, 1, 64, 1.0, "",
	 "Shows messages", 0, NULL, NULL},
	{"maze",
	 1000, 1, 3000, -40, 64, 1.0, "",
     "Shows a random maze and a depth first search solution", 0, NULL, NULL},
#ifdef USE_GL
	{"moebius",
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows Moebius Strip II, an Escher-like GL scene with ants", 0, NULL, "#ifdef USE_GL"},
	{"morph3d",
	 1000, 0, 1, 1, 64, 1.0, "",
	 "Shows GL morphing polyhedra", 0, NULL, "#ifdef USE_GL"},
#endif
	{"mountain",
	 1000, 30, 4000, 1, 64, 1.0, "",
	 "Shows Papo's mountain range", 0, NULL, NULL},
	{"munch",
	 5000, 1, 7, 1, 64, 1.0, "",
	 "Shows munching squares", 0, NULL, NULL},
	{"nose",
	 100000, 1, 1, 1, 64, 1.0, "",
	 "Shows a man with a big nose runs around spewing out messages", 0, NULL, NULL},
	{"pacman",
	 100000, 10, 1, 0, 64, 1.0, "",
	 "Shows Pacman(tm)", 0, NULL, NULL},
	{"penrose",
	 10000, 1, 1, -40, 64, 1.0, "",
	 "Shows Penrose's quasiperiodic tilings", 0, NULL, NULL},
	{"petal",
	 10000, -500, 400, 1, 64, 1.0, "",
	 "Shows various GCD Flowers", 0, NULL, NULL},
#ifdef USE_GL
	{"pipes",
	 1000, 2, 5, 500, 64, 1.0, "",
	 "Shows a selfbuilding pipe system", 0, NULL, "#ifdef USE_GL"},
#endif
	{"puzzle",
	 10000, 250, 1, 1, 64, 1.0, "",
	 "Shows a puzzle being scrambled and then solved", 0, NULL, NULL},
	{"pyro",
	 15000, 100, 1, -3, 64, 1.0, "",
	 "Shows fireworks", 0, NULL, NULL},
	{"qix",
	 30000, -5, 32, 1, 64, 1.0, "",
	 "Shows spinning lines a la Qix(tm)", 0, NULL, NULL},
	{"roll",
	 100000, 25, 1, -64, 64, 0.6, "",
	 "Shows a rolling ball", 0, NULL, NULL},
	{"rotor",
	 10000, 4, 20, -6, 64, 0.3, "",
	 "Shows Tom's Roto-Rooter", 0, NULL, NULL},
#ifdef USE_GL
	{"rubik",
	 10000, -30, 5, -6, 64, 1.0, "",
	 "Shows an auto-solving Rubik's Cube", 0, NULL, "#ifdef USE_GL"},
#endif
	{"shape",
	 10000, 100, 256, 1, 64, 1.0, "",
	 "Shows stippled rectangles, ellipses, and triangles", 0, NULL, NULL},
	{"sierpinski",
	 400000, 2000, 100, 1, 64, 1.0, "",
	 "Shows Sierpinski's triangle", 0, NULL, NULL},
	{"slip",
	 50000, 35, 50, 1, 64, 1.0, "",
	 "Shows slipping blits", 0, NULL, NULL},
	{"sphere",
	 5000, 1, 20, 0, 64, 1.0, "",
	 "Shows a bunch of shaded spheres", 0, NULL, NULL},
	{"spiral",
	 5000, -40, 350, 1, 64, 1.0, "",
	 "Shows a helical locus of points", 0, NULL, NULL},
	{"spline",
	 30000, -6, 2048, 1, 64, 0.3, "",
	 "Shows colorful moving splines", 0, NULL, NULL},
#ifdef USE_GL
	{"sproingies",
	 1000, 5, 0, 400, 64, 1.0, "",
	 "Shows Sproingies!  Nontoxic.  Safe for pets and small children", 0, NULL, "#ifdef USE_GL"},
	{"stairs",
	 200000, 0, 1, 1, 64, 1.0, "",
"Shows some Infinite Stairs, an Escher-like scene", 0, NULL, "#ifdef USE_GL"},
#endif
	{"star",
	 75000, 100, 1, 100, 64, 0.3, "",
	 "Shows a star field with a twist", 0, NULL, NULL},
	{"starfish",
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows starfish", 0, NULL, NULL},
	{"strange",
	 1000, 1, 1, 1, 64, 1.0, "",
	 "Shows strange attractors", 0, NULL, NULL},
#ifdef USE_GL
	{"superquadrics",
	 1000, 25, 40, 1, 64, 1.0, "",
	 "Shows 3D mathematical shapes", 0, NULL, "#ifdef USE_GL"},
#endif
	{"swarm",
	 15000, 100, 1, 1, 64, 1.0, "",
	 "Shows a swarm of bees following a wasp", 0, NULL, NULL},
	{"swirl",
	 5000, 5, 1, 1, 64, 1.0, "",
	 "Shows animated swirling patterns", 0, NULL, NULL},
	{"tetris",
   50000, 1, 1, 0, 64, 1.0, "",
	 "Shows tetris", 0, NULL, NULL},
	{"thornbird",
	 1000, 800, 16, 1, 64, 1.0, "",
	 "Shows an animated bird in a thorn bush fractal map", 0, NULL, NULL},
	{"triangle",
	 10000, 1, 1, 1, 64, 1.0, "",
	 "Shows a triangle mountain range", 0, NULL, NULL},
	{"tube",
	 25000, -9, 20000, -200, 64, 1.0, "",
	 "Shows an animated tube", 0, NULL, NULL},
	{"turtle",
	 1000000, 1, 20, 1, 64, 1.0, "",
	 "Shows turtle fractals", 0, NULL, NULL},
	{"vines",
	 200000, 0, 1, 1, 64, 1.0, "",
	 "Shows fractals", 0, NULL, NULL},
	{"voters",
	 1000, 0, 327670, 0, 64, 1.0, "",
	 "Shows Dewdney's Voters", 0, NULL, NULL},
	{"wator",
	 750000, 1, 32767, 0, 64, 1.0, "",
     "Shows Dewdney's Water-Torus planet of fish and sharks", 0, NULL, NULL},
	{"wire",
	 500000, 1000, 150, -8, 64, 1.0, "",
	 "Shows a random circuit with 2 electrons", 0, NULL, NULL},
	{"world",
	 100000, -16, 1, 1, 64, 0.3, "",
	 "Shows spinning Earths", 0, NULL, NULL},
	{"worm",
	 17000, -20, 10, -3, 64, 1.0, "",
	 "Shows wiggly worms", 0, NULL, NULL},
	{"xjack",
	 50000, 1, 1, 1, 64, 1.0, "",
	 "Shows Jack having one of those days", 0, NULL, NULL},

/* SPECIAL MODES */
	{"blank",
	 3000000, 1, 1, 1, 64, 1.0, "",
	 "Shows nothing but a black screen", 0, NULL, NULL}

};
