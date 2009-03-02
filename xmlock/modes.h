typedef struct LockStruct_s
{
	char *cmdline_arg;          /* mode name */
	/* Maybe other things should be added here from xlock? */
	/* Should read in XLock as well to set defaults */
	char *desc;                 /* text description of mode */
} LockStruct;
		
static LockStruct LockProcs[] =
{
{"ant","Shows Langton's and Turk's generalized ants"},
#ifdef USE_GL
{"atlantis","Shows moving sharks/whales/dolphin"},
#endif
{"ball","Shows bouncing balls"},
{"bat","Shows bouncing flying bats"},
{"blot","Shows Rorschach's ink blot test"},
{"bouboule","Shows Mimi's bouboule of moving stars"},
{"bounce","Shows bouncing footballs"},
{"braid","Shows random braids and knots"},
{"bubble","Shows popping bubbles"},
{"bubble3d","Richard Jones's GL bubbles"},
{"bug","Shows Palmiter's bug evolution and garden of Eden"},
#ifdef USE_GL
{"cage","Shows the Impossible Cage, an Escher-like GL scene"},
#endif
#if defined( USE_XPM ) || defined( USE_XPMINC )
{"cartoon","Shows bouncing cartoons"},
#endif
{"clock","Shows Packard's clock"},
{"coral","Shows a coral reef"},
{"crystal","Shows polygons in 2D plane groups"},
{"daisy","Shows a meadow of daisies"},
{"dclock","Shows a floating digital clock"},
{"deco","Shows art as ugly as sin"},
{"demon","Shows Griffeath's cellular automata"},
{"dilemma","Shows Lloyd's Prisoner's Dilemma simulation"},
{"discrete","Shows various discrete maps"},
{"drift","Shows cosmic drifting flame fractals"},
{"eyes","Shows eyes following a bouncing grelb"},
{"fadeplot","Shows a fading plot of sine squared"},
{"flag","Shows a waving flag of your operating system"},
{"flame","Shows cosmic flame fractals"},
{"flow","Shows dynamic strange attractors"},
{"forest","Shows binary trees of a fractal forest"},
{"galaxy","Shows crashing spiral galaxies"},
#ifdef USE_GL
{"gears","Shows GL's gears"},
#endif
{"goop","Shows goop from a lava lamp"},
{"grav","Shows orbiting planets"},
{"helix","Shows string art"},
{"hop","Shows real plane iterated fractals"},
{"hyper","Shows spinning n-dimensional hypercubes"},
{"ico","Shows a bouncing polyhedron"},
{"ifs","Shows a modified iterated function system"},
{"image","Shows randomly appearing logos"},
{"julia","Shows the Julia set"},
{"kaleid","Shows a kaleidoscope"},
{"kumppa","Shows kumppa"},
{"laser","Shows spinning lasers"},
{"life","Shows Conway's game of Life"},
{"life1d","Shows Wolfram's game of 1D Life"},
{"life3d","Shows Bays' game of 3D Life"},
{"lightning","Shows Keith's fractal lightning bolts"},
{"lisa","Shows animated lisajous loops"},
{"lissie","Shows lissajous worms"},
{"loop","Shows Langton's self-producing loops"},
{"mandelbrot","Shows mandelbrot sets"},
{"marquee","Shows messages"},
{"maze","Shows a random maze and a depth first search solution"},
#ifdef USE_GL
{"moebius","Shows Moebius Strip II, an Escher-like GL scene with ants"},
#endif
#ifdef USE_GL
{"morph3d","Shows GL morphing polyhedra"},
#endif
{"mountain","Shows Papo's mountain range"},
{"munch","Shows munching squares"},
{"nose","Shows a man with a big nose runs around spewing out messages"},
{"pacman","Shows Pacman(tm)"},
{"penrose","Shows Penrose's quasiperiodic tilings"},
{"petal","Shows various GCD Flowers"},
#ifdef USE_GL
{"pipes","Shows a selfbuilding pipe system"},
#endif
{"puzzle","Shows a puzzle being scrambled and then solved"},
{"pyro","Shows fireworks"},
{"qix","Shows spinning lines a la Qix(tm)"},
{"roll","Shows a rolling ball"},
{"rotor","Shows Tom's Roto-Rooter"},
#ifdef USE_GL
{"rubik","Shows an auto-solving Rubik's Cube"},
#endif
{"shape","Shows stippled rectangles, ellipses, and triangles"},
{"sierpinski","Shows Sierpinski's triangle"},
{"slip","Shows slipping blits"},
{"sphere","Shows a bunch of shaded spheres"},
{"spiral","Shows a helical locus of points"},
{"spline","Shows colorful moving splines"},
#ifdef USE_GL
{"sproingies","Shows Sproingies!  Nontoxic.  Safe for pets and small children"},
#endif
#ifdef USE_GL
{"stairs","Shows some Infinite Stairs, an Escher-like scene"},
#endif
{"star","Shows a star field with a twist"},
{"starfish","Shows starfish"},
{"strange","Shows strange attractors"},
#ifdef USE_GL
{"superquadrics","Shows 3D mathematical shapes"},
#endif
{"swarm","Shows a swarm of bees following a wasp"},
{"swirl","Shows animated swirling patterns"},
{"tetris","Shows tetris"},
{"thornbird","Shows an animated bird in a thorn bush fractal map"},
{"triangle","Shows a triangle mountain range"},
{"tube","Shows an animated tube"},
{"turtle","Shows turtle fractals"},
{"vines","Shows fractals"},
{"voters","Shows Dewdney's Voters"},
{"wator","Shows Dewdney's Water-Torus planet of fish and sharks"},
{"wire","Shows a random circuit with 2 electrons"},
{"world","Shows spinning Earths"},
{"worm","Shows wiggly worms"},
{"xjack","Shows Jack having one of those days"},
{"blank","Shows nothing but a black screen"},
 
#ifdef USE_BOMB
  {"bomb", "Shows a bomb and will autologout after a time"},
  {"random", "Shows a random mode (except blank and bomb)"}
  #else
  {"random", "Shows a random mode (except blank)"}
#endif
};


static int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
