static char *LockProcs[] =
{
  "ant",
  "ball",
  "bat",
  "blot",
  "bouboule",
  "bounce",
  "braid",
  "bug",
#if defined( USE_XPM ) || defined( USE_XPMINC )
  "cartoon",
#endif
  "crystal",
  "clock",
  "daisy",
  "dclock",
  "demon",
  "drift",
#ifdef USE_GL
  "escher",
#endif
  "eyes",
  "fadeplot",
  "flag",
  "flame",
  "forest",
  "galaxy",
#ifdef USE_GL
  "gears",
#endif
  "geometry",
  "grav",
  "helix",
  "hop",
  "hyper",
  "ico",
  "ifs",
  "image",
  "julia",
  "kaleid",
  "laser",
  "life",
  "life1d",
  "life3d",
  "lightning",
  "lisa",
  "lissie",
  "loop",
  "marquee",
  "maze",
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
  "sphere",
  "spiral",
  "spline",
#ifdef USE_GL
  "sproingies",
#endif
  "star",
  "strange",
#ifdef USE_GL
  "superquadrics",
#endif
  "swarm",
  "swirl",
  "triangle",
  "tube",
  "turtle",
  "vines",
  "voters",
  "wator",
  "wire",
  "world",
  "worm",
  "blank",
#ifdef USE_BOMB
  "bomb",
  "random",
#else
  "random",
#endif
  NULL
};

/* static int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]); */
