/*
**  Helpful definitions for porting xlock modes to xscreensaver.
**  by Charles Hannum, mycroft@ai.mit.edu
**
**  for xlock 2.3 and xscreensaver 1.2, 28AUG92
**
**
**  To use, just copy the appropriate file from xlock, add a target
**  for it in the Imakefile, and do the following:
**
**  1) If you include math.h, make sure it is before xlock.h.
**  2) Make sure the first thing you do in initfoo() is to call
**     XGetWindowAttributes.  This is what actually sets up the
**     colormap and whatnot.
**  3) Add an appropriate PROGRAM() line at the end of the .c file.
**     The information you need for this comes from xlock's file
**     resource.c.
**
**  That's about all there is to it.
**
**  As an added bonus, if you put an empty definition of PROGRAM() in
**  xlock's xlock.h, you can now use the code with either xlock or
**  xscreensaver.
**
**
**  If you make any improvements to this code, please send them to me!
**  It could certainly use some more work.
*/

#include "screenhack.h"

#define MAXSCREENS 1

static GC gc;
static unsigned long *pixels = 0, fg_pixel, bg_pixel;
static int npixels;
static Colormap cmap;

static int batchcount;
static unsigned int delay;
static double saturation;

typedef struct {
  GC gc;
  int npixels;
  u_long *pixels;
} perscreen;

static perscreen Scr[MAXSCREENS];
static Display *dsp;

static int screen = 0;

static void
My_XGetWindowAttributes (dpy, win, xgwa)
  Display *dpy;
  Window win;
  XWindowAttributes *xgwa;
{
  XGetWindowAttributes (dpy, win, xgwa);

  if (! pixels) {
    XGCValues gcv;
    XColor color;
    int n;
    int i, shift;

    cmap = xgwa->colormap;

    i = get_integer_resource ("ncolors", "Integer");
    if (i <= 2) i = 2, mono_p = True;
    shift = 360 / i;
    pixels = (unsigned long *) calloc (i, sizeof (unsigned long));
    fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
    bg_pixel = get_pixel_resource ("background", "Background", dpy, cmap);
    if (! mono_p) {
      for (npixels = 0; npixels < i; npixels++) {
        hsv_to_rgb ((360*npixels)/i, saturation, 1.0, &color.red, &color.green, &color.blue);
        if (! XAllocColor (dpy, cmap, &color))
          break;
        pixels[npixels] = color.pixel;
      }
    }
    n = get_integer_resource ("delay", "Usecs");
    if (n >= 0) delay = n;
    n = get_integer_resource ("count", "Integer");
    if (n > 0) batchcount = n;

    gcv.foreground = fg_pixel;
    gcv.background = bg_pixel;
    gc = XCreateGC (dpy, win, GCForeground|GCBackground, &gcv);

    XClearWindow (dpy, win);

    Scr[screen].gc = gc;
    Scr[screen].npixels = npixels;
    Scr[screen].pixels = pixels;
  }
}

#define XGetWindowAttributes(a,b,c) My_XGetWindowAttributes(a,b,c)

#undef BlackPixel
#define BlackPixel(a,b) bg_pixel
#undef WhitePixel
#define WhitePixel(a,b) fg_pixel
#define mono mono_p

#define seconds() time((time_t*)0)

char *defaults[] = {
  "*background:	black",
  "*foreground:	white",
  "*ncolors:	64",
  "*delay:	-1",
  0
};

XrmOptionDescRec options[] = {
  {"-count",	".count",	XrmoptionSepArg, 0},
  {"-ncolors",	".ncolors",	XrmoptionSepArg, 0},
  {"-delay",	".delay",	XrmoptionSepArg, 0},
};
int options_size = (sizeof (options) / sizeof (options[0]));

#define PROGRAM(Y,Z,D,B,S) \
char *progclass = Y;			\
					\
void screenhack (dpy, window)		\
  Display *dpy;				\
  Window window;			\
{					\
  batchcount = B;			\
  delay = D;				\
  saturation = S;			\
  dsp = dpy;				\
					\
  while (1) {				\
    init##Z (window);			\
    for (;;) {				\
      draw##Z (window);			\
      XSync (dpy, True);		\
      if (delay) usleep (delay);	\
    }					\
  }					\
}
