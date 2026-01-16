/* xscreensaver, Copyright (c) 2008-2015 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws repetitive patterns that should undo burned in LCD screens.
 * Concept shamelessly cloned from
 * http://toastycode.com/blog/2008/02/05/lcd-scrub/
 */

#include "screenhack.h"

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  enum { HORIZ_W, HORIZ_B, 
         VERT_W, VERT_B, 
         DIAG_W, DIAG_B, 
         WHITE, BLACK,
         RGB,
         RANDOM,
         END } mode;
  unsigned int enabled_mask;
  int count;
  GC fg, bg, bg2;
  int color_tick;
  int delay;
  int spread;
  int cycles;
  XImage *collisions;
  long ncollisions;
};


static void
pick_mode (struct state *st)
{
  st->count = 0;
  while (1)
    {
      if (++st->mode == END)
        st->mode = 0;
      if (st->enabled_mask & (1 << st->mode))
        break;
    }
}

static void *
lcdscrub_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  unsigned long fgp, bgp;

  st->dpy = dpy;
  st->window = window;
  st->delay  = get_integer_resource (st->dpy, "delay",  "Integer");
  st->spread = get_integer_resource (st->dpy, "spread", "Integer");
  st->cycles = get_integer_resource (st->dpy, "cycles", "Integer");

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  fgp = get_pixel_resource (st->dpy, st->xgwa.colormap, 
                            "foreground", "Foreground");
  bgp = get_pixel_resource (st->dpy, st->xgwa.colormap,
                            "background", "Background");

  gcv.foreground = bgp;
  gcv.background = fgp;
  st->bg  = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  st->bg2 = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  gcv.foreground = fgp;
  gcv.background = bgp;
  st->fg = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

#ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (st->dpy, st->fg,  False);
  jwxyz_XSetAntiAliasing (st->dpy, st->bg,  False);
  jwxyz_XSetAntiAliasing (st->dpy, st->bg2, False);
#endif

  st->enabled_mask = 0;
# define PREF(R,F) \
   if (get_boolean_resource (st->dpy, R, "Mode")) st->enabled_mask |= (1 << F)
  PREF("modeHW", HORIZ_W);
  PREF("modeHB", HORIZ_B);
  PREF("modeVW", VERT_W);
  PREF("modeVB", VERT_B);
  PREF("modeDW", DIAG_W);
  PREF("modeDB", DIAG_B);
  PREF("modeW",  WHITE);
  PREF("modeB",  BLACK);
  PREF("modeRGB", RGB);
  PREF("modeRandom", RANDOM);
# undef PREF
  if (! st->enabled_mask) 
    {
      fprintf (stderr, "%s: no modes enabled\n", progname);
      exit (1);
    }

  pick_mode (st);

  return st;
}


/* A test harness for visualizing different random number generators.
   This doesn't really belong in lcdscrub, but it was a convenient
   place to put it.
 */
#if 0								/* mwc1616 */

static unsigned long mwc1616_x = 1;
static unsigned long mwc1616_y = 2;

static void
mwc1616_srand (unsigned long seed)
{
  mwc1616_x = seed | 1;
  mwc1616_y = seed | 2;
}

static unsigned long
mwc1616 (void)
{
  mwc1616_x = 18000 * (mwc1616_x & 0xFFFF) + (mwc1616_x >> 16);
  mwc1616_y = 30903 * (mwc1616_y & 0xFFFF) + (mwc1616_y >> 16);
  return (mwc1616_x << 16) + (mwc1616_y & 0xFFFF);
}

# undef random
# undef srand
# define srand mwc1616_srand
# define random() ((unsigned int) (mwc1616() & (unsigned int) (~0)))


#elif 0						/* xorshift128plus */


static uint64_t xo_state0 = 1;
static uint64_t xo_state1 = 2;

static void
xorshift128plus_srand (unsigned long seed)
{
  xo_state0 = seed | 1;
  xo_state1 = seed | 2;
}

static uint64_t
xorshift128plus (void)
{
  register uint64_t s1 = xo_state0;
  register uint64_t s0 = xo_state1;
  xo_state0 = s0;
  s1 ^= s1 << 23;
  s1 ^= s1 >> 17;
  s1 ^= s0;
  s1 ^= s0 >> 26;
  xo_state1 = s1;
  return s1;
}

# undef random
# undef srand
# define srand xorshift128plus_srand
# define random() ((unsigned int) (xorshift128plus() & (unsigned int) (~0)))


#else								/* ya_random */
# undef srand
# define srand(n)

#endif								/* ya_random */



/* If you see patterns in this image, the PRNG sucks.
 */
static void
lcdscrub_random (struct state *st)
{
  unsigned long steps_per_frame = 3000000;
  unsigned long segments = 0x8000;  /* 2^15 */

  if (! st->collisions)
    {
      struct timeval tp;
# if GETTIMEOFDAY_TWO_ARGS
      gettimeofday (&tp, 0);
# else
      gettimeofday (&tp);
# endif
      srand ((unsigned int) (tp.tv_sec ^ tp.tv_usec));

      st->collisions = 
        XCreateImage (st->dpy, st->xgwa.visual, 1, XYPixmap,
                      0, NULL, segments, segments, 8, 0);
      if (! st->collisions) abort();
      st->collisions->data = (char *)
        calloc (segments, st->collisions->bytes_per_line);  /* 128 MB */
      if (! st->collisions->data) abort();
    }

  while (--steps_per_frame)
    {
      unsigned long x = random() & (segments-1);
      unsigned long y = random() & (segments-1);
      unsigned long p = XGetPixel (st->collisions, x, y) ? 0 : 1;
      XPutPixel (st->collisions, x, y, p);
      st->ncollisions += (p ? 1 : -1);
    }

  {
    int w, h;
    Pixmap p;
    GC gc;

    XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
    w = st->xgwa.width;
    h = st->xgwa.height;

    p = XCreatePixmap (st->dpy, st->window, w, h, 1);
    gc = XCreateGC (st->dpy, p, 0, 0);
    XSetBackground (st->dpy, gc, 0);
    XSetForeground (st->dpy, gc, 1);
    XPutImage (st->dpy, p, gc, st->collisions, 0, 0, 0, 0, w, h);
    XFreeGC (st->dpy, gc);

    gc = st->fg;
    XClearWindow (st->dpy, st->window);
    XSetClipMask (st->dpy, gc, p);
    XFillRectangle (st->dpy, st->window, gc, 0, 0, w, h);
    XFreePixmap (st->dpy, p);
  }

  /*
    fprintf(stderr, "%.2f\n", st->ncollisions / (float) (segments*segments));
  */
}


static unsigned long
lcdscrub_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int count = st->count % st->spread;
  int i;
  GC fg = (st->mode & 1 ? st->fg : st->bg);
  GC bg = (st->mode & 1 ? st->bg : st->fg);

  switch (st->mode) {
  case HORIZ_W:
  case HORIZ_B:
    XFillRectangle (st->dpy, st->window, bg, 0, 0,
                    st->xgwa.width, st->xgwa.height);
    for (i = count; i < st->xgwa.height; i += st->spread)
      XDrawLine (st->dpy, st->window, fg, 0, i, st->xgwa.width, i);
    break;
  case VERT_W:
  case VERT_B:
    XFillRectangle (st->dpy, st->window, bg, 0, 0,
                    st->xgwa.width, st->xgwa.height);
    for (i = count; i < st->xgwa.width; i += st->spread)
      XDrawLine (st->dpy, st->window, fg, i, 0, i, st->xgwa.height);
    break;
  case DIAG_W:
  case DIAG_B:
    XFillRectangle (st->dpy, st->window, bg, 0, 0,
                    st->xgwa.width, st->xgwa.height);
    for (i = count; i < st->xgwa.width; i += st->spread)
      XDrawLine (st->dpy, st->window, fg, i, 0, 
                 i + st->xgwa.width, st->xgwa.width);
    for (i = -count; i < st->xgwa.height; i += st->spread)
      XDrawLine (st->dpy, st->window, fg, 0, i,
                 st->xgwa.height, i + st->xgwa.height);
    break;
  case RGB:
    {
      int scale = 10 * 8; /* 8 sec */
      static const unsigned short colors[][3] = {
        { 0xFFFF, 0x0000, 0x0000 },
        { 0x0000, 0xFFFF, 0x0000 },
        { 0x0000, 0x0000, 0xFFFF },
        { 0xFFFF, 0xFFFF, 0x0000 },
        { 0xFFFF, 0x0000, 0xFFFF },
        { 0x0000, 0xFFFF, 0xFFFF },
        { 0xFFFF, 0xFFFF, 0xFFFF },
        { 0x0000, 0x0000, 0x0000 },
      };
      static unsigned long last = 0;
      XColor xc;
      bg = st->bg2;
      xc.red   = colors[st->color_tick / scale][0];
      xc.green = colors[st->color_tick / scale][1];
      xc.blue  = colors[st->color_tick / scale][2];
      if (last) XFreeColors (st->dpy, st->xgwa.colormap, &last, 1, 0);
      XAllocColor (st->dpy, st->xgwa.colormap, &xc);
      last = xc.pixel;
      XSetForeground (st->dpy, bg, xc.pixel);
      st->color_tick = (st->color_tick + 1) % (countof(colors) * scale);
      /* fall through */
    }
  case WHITE:
  case BLACK:
    XFillRectangle (st->dpy, st->window, bg, 0, 0,
                    st->xgwa.width, st->xgwa.height);
    break;
  case RANDOM:
    lcdscrub_random (st);
    break;
  default: 
    abort(); 
    break;
  }

  st->count++;

  if (st->count > st->spread * st->cycles)
    pick_mode (st);

  return st->delay;
}

static void
lcdscrub_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
lcdscrub_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
lcdscrub_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->fg);
  XFreeGC (dpy, st->bg);
  XFreeGC (dpy, st->bg2);
  if (st->collisions)
    {
      free (st->collisions->data);
      st->collisions->data = 0;
      XDestroyImage (st->collisions);
    }
  free (st);
}


static const char *lcdscrub_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*fpsSolid:		True",
  "*delay:		100000",
  "*spread:		8",
  "*cycles:		60",
  "*modeHW:		True",
  "*modeHB:		True",
  "*modeVW:		True",
  "*modeVB:		True",
  "*modeDW:		True",
  "*modeDB:		True",
  "*modeW:		True",
  "*modeB:		True",
  "*modeRGB:		True",
  "*modeRandom:		False",
  0
};

static XrmOptionDescRec lcdscrub_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-spread",		".spread",	XrmoptionSepArg, 0 },
  { "-cycles",		".cycles",	XrmoptionSepArg, 0 },
  { "-no-hw",		".modeHW",	XrmoptionNoArg, "False" },
  { "-no-hb",		".modeHB",	XrmoptionNoArg, "False" },
  { "-no-vw",		".modeVW",	XrmoptionNoArg, "False" },
  { "-no-vb",		".modeVB",	XrmoptionNoArg, "False" },
  { "-no-dw",		".modeDW",	XrmoptionNoArg, "False" },
  { "-no-db",		".modeDB",	XrmoptionNoArg, "False" },
  { "-no-w",		".modeW",	XrmoptionNoArg, "False" },
  { "-no-b",		".modeB",	XrmoptionNoArg, "False" },
  { "-no-rgb",		".modeRGB",	XrmoptionNoArg, "False" },
  { "-random",		".modeRandom",	XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("LCDscrub", lcdscrub)
