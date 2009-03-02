/* xscreensaver, Copyright (c) 1992, 1995, 1996
 *  Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This code derived from TI Explorer Lisp code by Joe Keane, Fritz Mueller,
 * and Jamie Zawinski.
 */

#include <math.h>
#include "screenhack.h"

static Display *dpy;
static Window window;
static GC color0, color1, color2, color3, color4, color5, color6, color7;
static GC black;

static int delay;

static int observer_z;
static int x_offset, y_offset;
static int unit_pixels;

struct point_state {
  int old_x, old_y;
  int new_x, new_y;
  Bool same_p;
};

static void
move_line (struct point_state *state0, struct point_state *state1, GC gc)
{
  if (state0->same_p && state1->same_p)
    return;
  if (mono_p)
    {
      XDrawLine (dpy, window, black,
		 state0->old_x, state0->old_y, state1->old_x, state1->old_y);
      XDrawLine (dpy, window, gc,
		 state0->new_x, state0->new_y, state1->new_x, state1->new_y);
    }
  else
    {
      XSegment segments [2];
      segments [0].x1 = state0->old_x; segments [0].y1 = state0->old_y;
      segments [0].x2 = state1->old_x; segments [0].y2 = state1->old_y;
      segments [1].x1 = state0->new_x; segments [1].y1 = state0->new_y;
      segments [1].x2 = state1->new_x; segments [1].y2 = state1->new_y;
      XDrawSegments (dpy, window, gc, segments, 2);
    }
}

static void
hyper (double xy, double xz, double yz, double xw, double yw, double zw)
{
  double cos_xy = cos (xy), sin_xy = sin (xy);
  double cos_xz = cos (xz), sin_xz = sin (xz);
  double cos_yz = cos (yz), sin_yz = sin (yz);
  double cos_xw = cos (xw), sin_xw = sin (xw);
  double cos_yw = cos (yw), sin_yw = sin (yw);
  double cos_zw = cos (zw), sin_zw = sin (zw);

  double ax = 1.0, ay = 0.0, az = 0.0, aw = 0.0;
  double bx = 0.0, by = 1.0, bz = 0.0, bw = 0.0;
  double cx = 0.0, cy = 0.0, cz = 1.0, cw = 0.0;
  double dx = 0.0, dy = 0.0, dz = 0.0, dw = 1.0;

  double _tmp0_, _tmp1_;

  struct point_state points [16];
  memset (points, 0, sizeof (points));

#define mmmm (&points[0])
#define mmmp (&points[1])
#define mmpm (&points[2])
#define mmpp (&points[3])
#define mpmm (&points[4])
#define mpmp (&points[5])
#define mppm (&points[6])
#define mppp (&points[7])
#define pmmm (&points[8])
#define pmmp (&points[9])
#define pmpm (&points[10])
#define pmpp (&points[11])
#define ppmm (&points[12])
#define ppmp (&points[13])
#define pppm (&points[14])
#define pppp (&points[15])

  while (1)
    {
      double temp_mult;

#define compute(a,b,c,d,point_state) \
      temp_mult = (unit_pixels / (((a*az) + (b*bz) + (c*cz) + (d*dz) +	  \
				   (a*aw) + (b*bw) + (c*cw) + (d*dw))	  \
				  - observer_z));			  \
  point_state->old_x = point_state->new_x;				  \
  point_state->old_y = point_state->new_y;				  \
  point_state->new_x = ((((a*ax) + (b*bx) + (c*cx) + (d*dx)) * temp_mult) \
			+ x_offset);					  \
  point_state->new_y = ((((a*ay) + (b*by) + (c*cy) + (d*dy)) * temp_mult) \
			+ y_offset);					  \
  point_state->same_p = (point_state->old_x == point_state->new_x &&	  \
			 point_state->old_y == point_state->new_y);

      compute (-1, -1, -1, -1, mmmm);
      compute (-1, -1, -1,  1, mmmp);
      compute (-1, -1,  1, -1, mmpm);
      compute (-1, -1,  1,  1, mmpp);
      compute (-1,  1, -1, -1, mpmm);
      compute (-1,  1, -1,  1, mpmp);
      compute (-1,  1,  1, -1, mppm);
      compute (-1,  1,  1,  1, mppp);
      compute ( 1, -1, -1, -1, pmmm);
      compute ( 1, -1, -1,  1, pmmp);
      compute ( 1, -1,  1, -1, pmpm);
      compute ( 1, -1,  1,  1, pmpp);
      compute ( 1,  1, -1, -1, ppmm);
      compute ( 1,  1, -1,  1, ppmp);
      compute ( 1,  1,  1, -1, pppm);
      compute ( 1,  1,  1,  1, pppp);

      move_line (mmmm, mmmp, color0);
      move_line (mmmm, mmpm, color0);
      move_line (mmpm, mmpp, color0);
      move_line (mmmp, mmpp, color0);
      
      move_line (pmmm, pmmp, color1);
      move_line (pmmm, pmpm, color1);
      move_line (pmpm, pmpp, color1);
      move_line (pmmp, pmpp, color1);
      
      move_line (mpmm, mpmp, color2);
      move_line (mpmm, mppm, color2);
      move_line (mppm, mppp, color2);
      move_line (mpmp, mppp, color2);
      
      move_line (mmpp, mppp, color3);
      move_line (mmpp, pmpp, color3);
      move_line (pmpp, pppp, color3);
      move_line (mppp, pppp, color3);
      
      move_line (mmmm, mpmm, color4);
      move_line (mmmm, pmmm, color4);
      move_line (mpmm, ppmm, color4);
      move_line (pmmm, ppmm, color4);
      
      move_line (mmmp, mpmp, color5);
      move_line (mmmp, pmmp, color5);
      move_line (pmmp, ppmp, color5);
      move_line (mpmp, ppmp, color5);
      
      move_line (mmpm, mppm, color6);
      move_line (mmpm, pmpm, color6);
      move_line (pmpm, pppm, color6);
      move_line (mppm, pppm, color6);
      
      move_line (ppmm, ppmp, color7);
      move_line (ppmm, pppm, color7);
      move_line (pppm, pppp, color7);
      move_line (ppmp, pppp, color7);

 /* If you get error messages about the following forms, and you think you're
    using an ANSI C conforming compiler, then you're mistaken.  Possibly you're
    mixing an ANSI compiler with a non-ANSI preprocessor, or vice versa.
    Regardless, your system is broken; it's not a bug in this program.
  */
#if defined(__STDC__) || defined(__ANSI_CPP__)
# define rotate(name,dim0,dim1,cos,sin) \
      _tmp0_ = ((name##dim0 * cos) + (name##dim1 * sin)); \
      _tmp1_ = ((name##dim1 * cos) - (name##dim0 * sin)); \
      name##dim0 = _tmp0_; \
      name##dim1 = _tmp1_;

# define rotates(dim0,dim1) \
      if (sin_##dim0##dim1 != 0) {				   \
        rotate(a, dim0, dim1, cos_##dim0##dim1, sin_##dim0##dim1); \
        rotate(b, dim0, dim1, cos_##dim0##dim1, sin_##dim0##dim1); \
        rotate(c, dim0, dim1, cos_##dim0##dim1, sin_##dim0##dim1); \
        rotate(d, dim0, dim1, cos_##dim0##dim1, sin_##dim0##dim1); \
      }

#else /* !__STDC__, courtesy of Andreas Luik <luik@isa.de> */
# define rotate(name,dim0,dim1,cos,sin) \
      _tmp0_ = ((name/**/dim0 * cos) + (name/**/dim1 * sin)); \
      _tmp1_ = ((name/**/dim1 * cos) - (name/**/dim0 * sin)); \
      name/**/dim0 = _tmp0_; \
      name/**/dim1 = _tmp1_;

# define rotates(dim0,dim1) \
      if (sin_/**/dim0/**/dim1 != 0) {				       \
        rotate(a,dim0,dim1,cos_/**/dim0/**/dim1,sin_/**/dim0/**/dim1); \
        rotate(b,dim0,dim1,cos_/**/dim0/**/dim1,sin_/**/dim0/**/dim1); \
        rotate(c,dim0,dim1,cos_/**/dim0/**/dim1,sin_/**/dim0/**/dim1); \
        rotate(d,dim0,dim1,cos_/**/dim0/**/dim1,sin_/**/dim0/**/dim1); \
      }
#endif /* !__STDC__ */

      rotates (x,y);
      rotates (x,z);
      rotates (y,z);
      rotates (x,w);
      rotates (y,w);
      rotates (z,w);

      XSync (dpy, True);
      if (delay) usleep (delay);
    }
}


char *progclass = "Hypercube";

char *defaults [] = {
  "Hypercube.background:	black",		/* to placate SGI */
  "Hypercube.foreground:	white",
  "*color0:	red",
  "*color1:	orange",
  "*color2:	yellow",
  "*color3:	white",
  "*color4:	green",
  "*color5:	cyan",
  "*color6:	dodgerblue",
  "*color7:	magenta",

  "*xw:		0.000",
  "*xy:		0.010",
  "*xz:		0.005",
  "*yw:		0.010",
  "*yz:		0.000",
  "*zw:		0.000",

  "*observer-z:	5",
  "*delay:	100000",
  0
};

XrmOptionDescRec options [] = {
  { "-color0",		".color0",	XrmoptionSepArg, 0 },
  { "-color1",		".color1",	XrmoptionSepArg, 0 },
  { "-color2",		".color2",	XrmoptionSepArg, 0 },
  { "-color3",		".color3",	XrmoptionSepArg, 0 },
  { "-color4",		".color4",	XrmoptionSepArg, 0 },
  { "-color5",		".color5",	XrmoptionSepArg, 0 },
  { "-color6",		".color6",	XrmoptionSepArg, 0 },
  { "-color7",		".color7",	XrmoptionSepArg, 0 },

  { "-xw",		".xw",		XrmoptionSepArg, 0 },
  { "-xy",		".xy",		XrmoptionSepArg, 0 },
  { "-xz",		".xz",		XrmoptionSepArg, 0 },
  { "-yw",		".yw",		XrmoptionSepArg, 0 },
  { "-yz",		".yz",		XrmoptionSepArg, 0 },
  { "-zw",		".zw",		XrmoptionSepArg, 0 },

  { "-observer-z",	".observer-z",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *d, Window w)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;
  double xy, xz, yz, xw, yw, zw;
  unsigned long bg;

  dpy = d;
  window = w;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;

  x_offset = xgwa.width / 2;
  y_offset = xgwa.height / 2;
  unit_pixels = xgwa.width < xgwa.height ? xgwa.width : xgwa.height;

  xy = get_float_resource ("xy", "Float");
  xz = get_float_resource ("xz", "Float");
  yz = get_float_resource ("yz", "Float");
  xw = get_float_resource ("xw", "Float");
  yw = get_float_resource ("yw", "Float");
  zw = get_float_resource ("zw", "Float");

  observer_z = get_integer_resource ("observer-z", "Integer");

  delay = get_integer_resource ("delay", "Integer");

  bg = get_pixel_resource ("background", "Background", dpy, cmap);

  if (mono_p)
    {
      gcv.function = GXcopy;
      gcv.foreground = bg;
      black = XCreateGC (dpy, window, GCForeground|GCFunction, &gcv);
      gcv.foreground = get_pixel_resource ("foreground", "Foreground",
					   dpy, cmap);
      color0 = color1 = color2 = color3 = color4 = color5 = color6 = color7 =
	XCreateGC (dpy, window, GCForeground|GCFunction, &gcv);
    }
  else
    {
      black = 0;
      gcv.function = GXxor;
#define make_gc(color,name) \
	gcv.foreground = bg ^ get_pixel_resource ((name), "Foreground", \
						  dpy, cmap);		\
	color = XCreateGC (dpy, window, GCForeground|GCFunction, &gcv)

      make_gc (color0,"color0");
      make_gc (color1,"color1");
      make_gc (color2,"color2");
      make_gc (color3,"color3");
      make_gc (color4,"color4");
      make_gc (color5,"color5");
      make_gc (color6,"color6");
      make_gc (color7,"color7");
    }

  hyper (xy, xz, yz, xw, yw, zw);
}
