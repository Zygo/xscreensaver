static char *LockProcs[] =
{
"ant",
#ifdef USE_GL
"atlantis",
#endif
"ball",
"bat",
"blot",
"bouboule",
"bounce",
"braid",
"bubble",
#if defined( USE_GL ) && defined( HAVE_CXX )
"bubble3d",
#endif
"bug",
#ifdef USE_GL
"cage",
#endif
#if defined( USE_XPM ) || defined( USE_XPMINC )
"cartoon",
#endif
"clock",
"coral",
"crystal",
"daisy",
"dclock",
"decay",
"deco",
"demon",
"dilemma",
"discrete",
"drift",
"eyes",
"fadeplot",
"flag",
"flame",
"flow",
"forest",
"galaxy",
#ifdef USE_GL
"gears",
#endif
"goop",
"grav",
"helix",
"hop",
"hyper",
"ico",
"ifs",
"image",
#if defined( USE_GL ) && defined( HAVE_CXX )
"invert",
#endif
"juggle",
"julia",
"kaleid",
"kumppa",
#if defined( USE_GL ) && ( defined( USE_XPM ) || defined( USE_XPMINC ))
"lament",
#endif
"laser",
"life",
"life1d",
"life3d",
"lightning",
"lisa",
"lissie",
"loop",
"lyapunov",
"mandelbrot",
"marquee",
"matrix",
"maze",
#ifdef USE_GL
"moebius",
#endif
#ifdef USE_GL
"morph3d",
#endif
"mountain",
"munch",
"nose",
"pacman",
"penrose",
"petal",
#ifdef USE_GL
"pipes",
#endif
"puzzle",
"pyro",
"qix",
"roll",
"rotor",
#ifdef USE_GL
"rubik",
#endif
"shape",
"sierpinski",
"slip",
#ifdef HAVE_CXX
"solitare",
#endif
#ifdef USE_UNSTABLE
"space",
#endif
"sphere",
"spiral",
"spline",
#ifdef USE_GL
"sproingies",
#endif
#ifdef USE_GL
"stairs",
#endif
"star",
"starfish",
"strange",
#ifdef USE_GL
"superquadrics",
#endif
"swarm",
"swirl",
"t3d",
"tetris",
"thornbird",
"tik_tak",
"triangle",
"tube",
"turtle",
"vines",
"voters",
"wator",
"wire",
"world",
"worm",
"xjack",
"blank",
#ifdef USE_BOMB
"bomb",
"random"
#else
"random"
#endif
};

#define numprocs (sizeof (LockProcs) / sizeof (LockProcs[0]))
