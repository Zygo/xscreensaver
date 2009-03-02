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
	int         def_delay;	/* default delay for mode */
	int         def_batchcount;
	int         def_cycles;
	int         def_size;
	float       def_saturation;
	char       *desc;	/* text description of mode */
	unsigned int flags;	/* state flags for this mode */
	void       *userdata;	/* for use by the mode */
} LockStruct;

LockStruct  LockProcs[] =
{
	{"blot",
	 200000, 6, 30, 1, 0.4,
	 "Shows Rorschach's ink blot test", 0, NULL},
	{"bouboule",
	 5000, 100, 1, 15, 1.0,
	 "Shows Mimi's bouboule of moving stars", 0, NULL},
	{"bug",
	 75000, 10, 32767, -4, 1.0,
	 "Shows Palmiter's bug evolution and garden of Eden", 0, NULL},
	{"clock",
	 100000, -16, 200, -200, 1.0,
	 "Shows Packard's clock", 0, NULL},
	{"crystal",
	 60000, -40, 200, -15, 1.0,
	 "Shows polygons in 2D plane groups", 0, NULL},
	{"daisy",
	 100000, 300, 350, 1, 1.0,
	 "Shows a meadow of daisies", 0, NULL},
	{"dclock",
	 10000, 1, 10000, 1, 0.2,
	 "Shows a floating digital clock", 0, NULL},
	{"demon",
	 50000, 0, 1000, -7, 1.0,
	 "Shows Griffeath's cellular automata", 0, NULL},
	{"eyes",
	 20000, -8, 5, 1, 1.0,
	 "Shows eyes following a bouncing grelb", 0, NULL},
	{"flag",
	 50000, 1, 1000, -7, 1.0,
	 "Shows a flying flag of your operating system", 0, NULL},
	{"flame",
	 750000, 20, 10000, 1, 1.0,
	 "Shows cosmic flame fractals", 0, NULL},
	{"fract",
	 200000, 1, 1, 1, 1.0,
	 "Shows fractals", 0, NULL},
	{"grav",
	 10000, -12, 1, 1, 1.0,
	 "Shows orbiting planets", 0, NULL},
	{"helix",
	 25000, 1, 100, 1, 1.0,
	 "Shows string art", 0, NULL},
	{"hyper",
	 10000, 1, 300, 1, 1.0,
	 "Shows a spinning tesseract in 4D space", 0, NULL},
	{"ico",
	 100000, 0, 400, 0, 1.0,
	 "Shows a bouncing polyhedra", 0, NULL},
	{"image",
	 2000000, -10, 1, 1, 1.0,
	 "Shows randomly appearing logos", 0, NULL},
	{"kaleid",
	 20000, 1, 700, 1, 1.0,
	 "Shows a kaleidoscope", 0, NULL},
	{"laser",
	 20000, -10, 200, 1, 1.0,
	 "Shows spinning lasers", 0, NULL},
	{"life",
	 750000, 40, 140, 0, 1.0,
	 "Shows Conway's game of Life", 0, NULL},
	{"life1d",
	 10000, 1, 10, 0, 1.0,
	 "Shows Wolfram's game of 1D Life", 0, NULL},
	{"life3d",
	 1000000, 35, 85, 1, 1.0,
	 "Shows Bays' game of 3D Life", 0, NULL},
	{"lightning",
	 10000, 1, 1, 1, 0.6,
	 "Shows Keith's fractal lightning bolts", 0, NULL},
	{"lisa",
	 25000, 1, 256, -1, 1.0,
	 "Shows animated lisajous loops", 0, NULL},
	{"lissie",
	 10000, -2, 2000, -200, 0.6,
	 "Shows lissajous worms", 0, NULL},
	{"loop",
	 100000, 1, 1600, -12, 1.0,
	 "Shows Langton's self-producing loops", 0, NULL},
	{"marquee",
	 100000, 1, 1, 1, 1.0,
	 "Shows messages", 0, NULL},
	{"nose",
	 100000, 1, 1, 1, 1.0,
    "Shows a man with a big nose runs around spewing out messages", 0, NULL},
	{"penrose",
	 10000, 1, 1, -40, 1.0,
	 "Shows Penrose's quasiperiodic tilings", 0, NULL},
	{"petal",
	 10000, -500, 400, 1, 1.0,
	 "Shows various GCD Flowers", 0, NULL},
	{"puzzle",
	 10000, 250, 1, 1, 1.0,
	 "Shows a puzzle being scrambled and then solved", 0, NULL},
	{"pyro",
	 15000, 100, 1, -3, 1.0,
	 "Shows fireworks", 0, NULL},
	{"qix",
	 30000, 1, 64, 1, 1.0,
	 "Shows spinning lines a la Qix(tm)", 0, NULL},
	{"roll",
	 100000, 25, 1, -64, 1.0,
	 "Shows a rolling ball", 0, NULL},
	{"rotor",
	 10000, 4, 20, 1, 0.4,
	 "Shows Tom's Roto-Rooter", 0, NULL},
	{"shape",
	 10000, 100, 256, 1, 1.0,
	 "Shows stippled rectangles, ellipses, and triangles", 0, NULL},
	{"sierpinski",
	 400000, 2000, 100, 1, 1.0,
	 "Shows Sierpinski's triangle", 0, NULL},
	{"spline",
	 30000, -6, 2048, 1, 0.4,
	 "Shows colorful moving splines", 0, NULL},
	{"star",
	 40000, 100, 1, 100, 0.2,
	 "Shows a star field with a twist", 0, NULL},
	{"swarm",
	 10000, 100, 1, 1, 1.0,
	 "Shows a swarm of bees following a wasp", 0, NULL},
	{"triangle",
	 10000, 1, 1, 1, 1.0,
	 "Shows a triangle mountain range", 0, NULL},
	{"tube",
	 25000, 1, 20000, -200, 1.0,
	 "Shows an animated tube", 0, NULL},
	{"turtle",
	 1000000, 1, 20, 1, 1.0,
	 "Shows turtle fractals", 0, NULL},
	{"wire",
	 500000, 1000, 150, -8, 1.0,
	 "Shows a random circuit with 2 electrons", 0, NULL},
	{"world",
	 100000, -16, 1, 1, 0.3,
	 "Shows spinning Earths", 0, NULL},
	{"worm",
	 17000, -20, 10, -3, 1.0,
	 "Shows wiggly worms", 0, NULL},
/* NORMALLY HIGH CPU USAGE MODES (kind of subjective) */
	{"ant",
	 1000, -3, 40000, -7, 1.0,
	 "Shows Langton's and Turk's generalized ants", 0, NULL},
	{"ball",
	 10000, 10, 20, -100, 1.0,
	 "Shows bouncing balls", 0, NULL},
	{"bat",
	 100000, -8, 1, 0, 1.0,
	 "Shows bouncing flying bats", 0, NULL},
	{"bounce",
	 10000, -10, 1, 0, 1.0,
	 "Shows bouncing footballs", 0, NULL},
	{"braid",
	 1000, 15, 100, 1, 1.0,
	 "Shows random braids and knots", 0, NULL},
	{"drift",
	 10000, 30, 1, 1, 1.0,
	 "Shows cosmic drifting flame fractals", 0, NULL},
	{"forest",
	 400000, 100, 200, 1, 1.0,
	 "Shows binary trees of a fractal forest", 0, NULL},
	{"galaxy",
	 100, -5, 250, -3, 1.0,
	 "Shows crashing spiral galaxies", 0, NULL},
	{"hop",
	 10000, 1000, 2500, 1, 1.0,
	 "Shows real plane iterated fractals", 0, NULL},
	{"ifs",
	 1000, 1, 1, 1, 1.0,
	 "Shows a modified iterated function system", 0, NULL},
	{"julia",
	 10000, 1000, 20, 1, 1.0,
	 "Shows the Julia set", 0, NULL},
	{"maze",
	 1000, 1, 300, -40, 1.0,
	 "Shows a random maze and a depth first search solution", 0, NULL},
	{"mountain",
	 1000, 30, 100, 1, 1.0,
	 "Shows Papo's mountain range", 0, NULL},
	{"pacman",
	 100000, 10, 1, 0, 1.0,
	 "Shows Pacman(tm)", 0, NULL},
	{"slip",
	 50000, 35, 50, 1, 1.0,
	 "Shows slipping blits", 0, NULL},
	{"sphere",
	 10000, 1, 20, 0, 1.0,
	 "Shows a bunch of shaded spheres", 0, NULL},
	{"spiral",
	 5000, -40, 350, 1, 1.0,
	 "Shows helixes of dots", 0, NULL},
	{"strange",
	 1000, 1, 1, 1, 1.0,
	 "Shows strange attractors", 0, NULL},
	{"swirl",
	 5000, 5, 1, 1, 1.0,
	 "Shows animated swirling patterns", 0, NULL},
	{"voters",
	 1000, 0, 327670, 0, 1.0,
	 "Shows Dewdney's Voters", 0, NULL},
	{"wator",
	 750000, 1, 32767, 0, 1.0,
	 "Shows Dewdney's Water-Torus planet of fish and sharks", 0, NULL},
#if defined( USE_XPM ) || defined( USE_XPMINC )
	{"cartoon",
	 10000, 1, 1, 1, 1.0,
	 "Shows bouncing cartoons", 0, NULL},
#endif
#ifdef USE_GL
	{"atlantis",
	 100, 4, 250, 6000, 1.0,
	 "Shows moving sharks/whales/dolphin", 0, NULL},
	{"cage",
	 1000, 1, 1, 1, 1.0,
	 "Shows the Impossible Cage, an Escher-like GL scene", 0, NULL},
	{"gears",
	 1000, 1, 2, 1, 1.0,
	 "Shows GL's gears", 0, NULL},
	{"moebius",
	 1000, 1, 1, 1, 1.0,
       "Shows Moebius Strip II, an Escher-like GL scene with ants", 0, NULL},
	{"morph3d",
	 1000, 0, 1, 1, 1.0,
	 "Shows GL morphing polyhedra", 0, NULL},
	{"pipes",
	 1000, 2, 5, 500, 1.0,
	 "Shows a selfbuilding pipe system", 0, NULL},
	{"sproingies",
	 1000, 5, 0, 400, 1.0,
  "Shows Sproingies!  Nontoxic.  Safe for pets and small children", 0, NULL},
	{"stairs",
	 1000, 1, 1, 1, 1.0,
	 "Shows Infinite Stairs, an Escher-like GL scene", 0, NULL},
	{"superquadrics",
	 1000, 25, 40, 1, 1.0,
	 "Shows 3D mathematical shapes", 0, NULL},
#endif
/* NEW POSSIBLY, BUGGY MODES */
#ifdef USE_HACKERS
	{"fadeplot",
	 30000, 1500, 1, 1, 1.0,
	 "Shows fadeplot", 0, NULL},
#endif
/* SPECIAL MODES */
	{"blank",
	 3000000, 1, 1, 1, 1.0,
	 "Shows nothing but a black screen", 0, NULL},
#ifdef USE_BOMB
	{"bomb",
	 100000, 10, 20, 1, 1.0,
	 "Shows a bomb and will autologout after a time", 0, NULL},
	{"random",
	 1, 1, 1, 1, 1.0,
	 "Shows a random mode from above except blank and bomb", 0, NULL},
#else
	{"random",
	 1, 1, 1, 1, 1.0,
	 "Shows a random mode from above except blank", 0, NULL},
#endif
};

/*------------------------------------*/

typedef struct struct_option_s {
	char       *cmdarg;
	char       *label;
	char       *desc;
} struct_option;

/* Description of the boolean option */
struct_option BoolOpt[] =
{
      {"mono", "mono", "The mono option causes xlock to display monochrome"},
	{"nolock", "nolock", "The nolock option causes xlock to only draw the patterns and not lock the display"},
	{"remote", "remote", "The remote option causes xlock to not stop you from locking remote X11 server"},
	{"allowroot", "allowroot", "The allowroot option allow the root password to unlock the server"},
	{"enablesaver", "enablesaver", "I don't know !! TODO"},
	{"allowaccess", "allowaccess", "I don't know !! TODO"},
	{"grabmouse", "grabmouse", "I don't know !! TODO"},
	{"echokeys", "echokeys", "I don't know !! TODO"},
	{"usefirst", "usefirst", "I don't know !! TODO"},
	{"verbose", "verbose", "verbose launch"}
};

/* Description of the general option */
struct_option generalOpt[] =
{
	{"username", "username", "text string to use for Name prompt"},
	{"password", "password", "text string to use for Password prompt"},
	{"info", "info", "text string to use for instruction"},
	{"validate", "validate", "text string to use for validating password message"},
	{"invalidate", "invalidate", "text string to use for invalid password message"},
	{"message", "message", "message to say"}
};

/* Description of the font and color option */
struct_option fntcolorOpt[] =
{
	{"bg", "background", "background color to use for password prompt"},
	{"fg", "foreground", "foreground color to use for password prompt"},
	{"font", "font", "font to use for password prompt"},
	{"msgfont", "msgfont", "font to use for message"}
};
