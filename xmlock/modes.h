typedef struct LockStruct_s
{
	char *cmdline_arg;          /* mode name */
	/* Maybe other things should be added here from xlock? */
	/* Should read in XLock as well to set defaults */
	char *desc;                 /* text description of mode */
} LockStruct;

static LockStruct LockProcs[] =
{
{(char *) "ant", (char *) "Shows Langton's and Turk's generalized ants"},
{(char *) "ant3d", (char *) "Shows 3D ants"},
{(char *) "apollonian", (char *) "Shows Apollonian Circles"},
#ifdef USE_GL
{(char *) "atlantis", (char *) "Shows moving sharks/whales/dolphin"},
#endif
#ifdef USE_GL
{(char *) "atunnels", (char *) "Shows an OpenGL advanced tunnel screensaver"},
#endif
{(char *) "ball", (char *) "Shows bouncing balls"},
{(char *) "bat", (char *) "Shows bouncing flying bats"},
#ifdef USE_GL
{(char *) "biof", (char *) "Shows 3D bioform"},
#endif
{(char *) "blot", (char *) "Shows Rorschach's ink blot test"},
{(char *) "bouboule", (char *) "Shows Mimi's bouboule of moving stars"},
{(char *) "bounce", (char *) "Shows bouncing footballs"},
{(char *) "braid", (char *) "Shows random braids and knots"},
{(char *) "bubble", (char *) "Shows popping bubbles"},
#if defined( USE_GL ) && defined( HAVE_CXX )
{(char *) "bubble3d", (char *) "Richard Jones's GL bubbles"},
#endif
{(char *) "bug", (char *) "Shows Palmiter's bug evolution and garden of Eden"},
#ifdef USE_GL
{(char *) "cage", (char *) "Shows the Impossible Cage, an Escher-like GL scene"},
#endif
{(char *) "clock", (char *) "Shows Packard's clock"},
{(char *) "coral", (char *) "Shows a coral reef"},
{(char *) "crystal", (char *) "Shows polygons in 2D plane groups"},
{(char *) "daisy", (char *) "Shows a meadow of daisies"},
{(char *) "dclock", (char *) "Shows a floating digital clock or message"},
{(char *) "decay", (char *) "Shows a decaying screen"},
{(char *) "deco", (char *) "Shows art as ugly as sin"},
{(char *) "demon", (char *) "Shows Griffeath's cellular automata"},
{(char *) "dilemma", (char *) "Shows Lloyd's Prisoner's Dilemma simulation"},
{(char *) "discrete", (char *) "Shows various discrete maps"},
{(char *) "dragon", (char *) "Shows Deventer's Hexagonal Dragons Maze"},
{(char *) "drift", (char *) "Shows cosmic drifting flame fractals"},
{(char *) "euler2d", (char *) "Shows a simulation of 2D incompressible inviscid fluid"},
{(char *) "eyes", (char *) "Shows eyes following a bouncing grelb"},
{(char *) "fadeplot", (char *) "Shows a fading plot of sine squared"},
{(char *) "fiberlamp", (char *) "Shows a Fiber Optic Lamp"},
#ifdef USE_GL
{(char *) "fire", (char *) "Shows a 3D fire-like image"},
#endif
{(char *) "flag", (char *) "Shows a waving flag image"},
{(char *) "flame", (char *) "Shows cosmic flame fractals"},
{(char *) "flow", (char *) "Shows dynamic strange attractors"},
{(char *) "forest", (char *) "Shows binary trees of a fractal forest"},
{(char *) "galaxy", (char *) "Shows crashing spiral galaxies"},
#ifdef USE_GL
{(char *) "gears", (char *) "Shows GL's gears"},
#endif
#ifdef USE_GL
{(char *) "glplanet", (char *) "Animates texture mapped sphere (planet)"},
#endif
{(char *) "goop", (char *) "Shows goop from a lava lamp"},
{(char *) "grav", (char *) "Shows orbiting planets"},
{(char *) "helix", (char *) "Shows string art"},
{(char *) "hop", (char *) "Shows real plane iterated fractals"},
{(char *) "hyper", (char *) "Shows spinning n-dimensional hypercubes"},
{(char *) "ico", (char *) "Shows a bouncing polyhedron"},
{(char *) "ifs", (char *) "Shows a modified iterated function system"},
{(char *) "image", (char *) "Shows randomly appearing logos"},
#if defined( USE_GL ) && defined( HAVE_CXX )
{(char *) "invert", (char *) "Shows a sphere inverted without wrinkles"},
#endif
{(char *) "juggle", (char *) "Shows a Juggler, juggling"},
{(char *) "julia", (char *) "Shows the Julia set"},
{(char *) "kaleid", (char *) "Shows a kaleidoscope"},
{(char *) "kumppa", (char *) "Shows kumppa"},
#ifdef USE_GL
{(char *) "lament", (char *) "Shows Lemarchand's Box"},
#endif
{(char *) "laser", (char *) "Shows spinning lasers"},
{(char *) "life", (char *) "Shows Conway's game of Life"},
{(char *) "life1d", (char *) "Shows Wolfram's game of 1D Life"},
{(char *) "life3d", (char *) "Shows Bays' game of 3D Life"},
{(char *) "lightning", (char *) "Shows Keith's fractal lightning bolts"},
{(char *) "lisa", (char *) "Shows animated lisajous loops"},
{(char *) "lissie", (char *) "Shows lissajous worms"},
{(char *) "loop", (char *) "Shows Langton's self-producing loops"},
{(char *) "lyapunov", (char *) "Shows lyapunov space"},
{(char *) "mandelbrot", (char *) "Shows mandelbrot sets"},
{(char *) "marquee", (char *) "Shows messages"},
{(char *) "matrix", (char *) "Shows the matrix"},
{(char *) "maze", (char *) "Shows a random maze and a depth first search solution"},
#ifdef USE_GL
{(char *) "moebius", (char *) "Shows Moebius Strip II, an Escher-like GL scene with ants"},
#endif
#ifdef USE_GL
{(char *) "molecule", (char *) "Draws molecules"},
#endif
#ifdef USE_GL
{(char *) "morph3d", (char *) "Shows GL morphing polyhedra"},
#endif
{(char *) "mountain", (char *) "Shows Papo's mountain range"},
{(char *) "munch", (char *) "Shows munching squares"},
#ifdef USE_GL
{(char *) "noof", (char *) "Shows SGI Diatoms"},
#endif
{(char *) "nose", (char *) "Shows a man with a big nose runs around spewing out messages"},
{(char *) "pacman", (char *) "Shows Pacman(tm)"},
{(char *) "penrose", (char *) "Shows Penrose's quasiperiodic tilings"},
{(char *) "petal", (char *) "Shows various GCD Flowers"},
{(char *) "petri", (char *) "Shows a mold simulation in a petri dish"},
#ifdef USE_GL
{(char *) "pipes", (char *) "Shows a selfbuilding pipe system"},
#endif
{(char *) "polyominoes", (char *) "Shows attempts to place polyominoes into a rectangle"},
{(char *) "puzzle", (char *) "Shows a puzzle being scrambled and then solved"},
{(char *) "pyro", (char *) "Shows fireworks"},
{(char *) "qix", (char *) "Shows spinning lines a la Qix(tm)"},
{(char *) "roll", (char *) "Shows a rolling ball"},
{(char *) "rotor", (char *) "Shows Tom's Roto-Rooter"},
#ifdef USE_GL
{(char *) "rubik", (char *) "Shows an auto-solving Rubik's Cube"},
#endif
#ifdef USE_GL
{(char *) "sballs", (char *) "Balls spinning like crazy in GL"},
#endif
{(char *) "scooter", (char *) "Shows a journey through space tunnel and stars"},
{(char *) "shape", (char *) "Shows stippled rectangles, ellipses, and triangles"},
{(char *) "sierpinski", (char *) "Shows Sierpinski's triangle"},
#ifdef USE_GL
{(char *) "sierpinski3d", (char *) "Shows GL's Sierpinski gasket"},
#endif
#if defined(USE_GL) && defined( USE_UNSTABLE )
{(char *) "skewb", (char *) "Shows an auto-solving Skewb"},
#endif
{(char *) "slip", (char *) "Shows slipping blits"},
#ifdef HAVE_CXX
{(char *) "solitare", (char *) "Shows Klondike's game of solitare"},
#endif
#ifdef USE_UNSTABLE
{(char *) "space", (char *) "Shows a journey into deep space"},
#endif
{(char *) "sphere", (char *) "Shows a bunch of shaded spheres"},
{(char *) "spiral", (char *) "Shows a helical locus of points"},
{(char *) "spline", (char *) "Shows colorful moving splines"},
#ifdef USE_GL
{(char *) "sproingies", (char *) "Shows Sproingies!  Nontoxic.  Safe for pets and small children"},
#endif
#ifdef USE_GL
{(char *) "stairs", (char *) "Shows some Infinite Stairs, an Escher-like scene"},
#endif
{(char *) "star", (char *) "Shows a star field with a twist"},
{(char *) "starfish", (char *) "Shows starfish"},
{(char *) "strange", (char *) "Shows strange attractors"},
#ifdef USE_GL
{(char *) "superquadrics", (char *) "Shows 3D mathematical shapes"},
#endif
{(char *) "swarm", (char *) "Shows a swarm of bees following a wasp"},
{(char *) "swirl", (char *) "Shows animated swirling patterns"},
{(char *) "t3d", (char *) "Shows a Flying Balls Clock Demo"},
{(char *) "tetris", (char *) "Shows an autoplaying tetris game"},
#if defined(USE_GL) && defined(HAVE_CXX) && defined( HAVE_TTF ) && defined( HAVE_GLTT )
{(char *) "text3d", (char *) "Shows 3D text"},
#endif
#if defined(USE_GL) && defined(HAVE_CXX) && defined( HAVE_FREETYPE ) && defined( HAVE_FTGL )
{(char *) "text3d2", (char *) "Shows 3D text"},
#endif
{(char *) "thornbird", (char *) "Shows an animated bird in a thorn bush fractal map"},
{(char *) "tik_tak", (char *) "Shows rotating polygons"},
{(char *) "toneclock", (char *) "Shows Peter Schat's toneclock"},
{(char *) "triangle", (char *) "Shows a triangle mountain range"},
{(char *) "tube", (char *) "Shows an animated tube"},
{(char *) "turtle", (char *) "Shows turtle fractals"},
{(char *) "vines", (char *) "Shows fractals"},
{(char *) "voters", (char *) "Shows Dewdney's Voters"},
{(char *) "wator", (char *) "Shows Dewdney's Water-Torus planet of fish and sharks"},
{(char *) "wire", (char *) "Shows a random circuit with 2 electrons"},
{(char *) "world", (char *) "Shows spinning Earths"},
{(char *) "worm", (char *) "Shows wiggly worms"},
{(char *) "xcl", (char *) "Shows a control line combat model race"},
{(char *) "xjack", (char *) "Shows Jack having one of those days"},
{(char *) "blank", (char *) "Shows nothing but a black screen"},
#ifdef USE_BOMB
{(char *) "bomb", (char *) "Shows a bomb and will autologout after a time"},
{(char *) "random", (char *) "Shows a random mode (except blank and bomb)"}
#else
{(char *) "random", (char *) "Shows a random mode (except blank)"}
#endif
};

#define numprocs (sizeof(LockProcs) /sizeof(LockProcs[0]))
