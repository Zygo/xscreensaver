/* xscreensaver, Copyright (c) 1992, 1995, 1996, 1998, 2000
 *  Jamie Zawinski <jwz@jwz.org>
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

#define POINT_COUNT 16
#define LINE_COUNT 32

#define ANGLE_SCALE 0.001

struct line_info
{
  char li_ip;
  char li_iq;
  char li_color;
  char li_pad;
};

struct point_state
{
  short old_x, old_y;
  short new_x, new_y;
};

struct hyper_state
{
  char hs_stop;
  char hs_icon;
  char hs_resize;
  char hs_redraw;
  Display *hs_display;
  Window hs_window;
  float hs_two_observer_z;
  float hs_offset_x;
  float hs_offset_y;
  float hs_unit_scale;
  int hs_delay;
  GC hs_color_gcs[8];
#if 0
  double hs_angle_xy;
  double hs_angle_xz;
  double hs_angle_yz;
  double hs_angle_xw;
  double hs_angle_yw;
  double hs_angle_zw;
#endif
  double hs_cos_xy, hs_sin_xy;
  double hs_cos_xz, hs_sin_xz;
  double hs_cos_yz, hs_sin_yz;
  double hs_cos_xw, hs_sin_xw;
  double hs_cos_yw, hs_sin_yw;
  double hs_cos_zw, hs_sin_zw;
  double hs_ref_ax, hs_ref_ay, hs_ref_az, hs_ref_aw;
  double hs_ref_bx, hs_ref_by, hs_ref_bz, hs_ref_bw;
  double hs_ref_cx, hs_ref_cy, hs_ref_cz, hs_ref_cw;
  double hs_ref_dx, hs_ref_dy, hs_ref_dz, hs_ref_dw;
  struct point_state hs_points[POINT_COUNT];
};

static const struct line_info line_table[LINE_COUNT];

static void init (struct hyper_state *hs);
static void hyper (struct hyper_state *hs);
static void check_events (struct hyper_state *hs);
static void set_sizes (struct hyper_state *hs, int width, int height);

static struct hyper_state hyper_state;


char *progclass = "Hypercube";

char *defaults[] =
{
  "*observer-z:	3",
  "*delay: 10000",
  "*xy: 3",
  "*xz:	5",
  "*yw:	10",
  "*yz:	0",
  "*xw:	0",
  "*zw:	0",
  ".background:	black",
  ".foreground:	white",
  "*color0:	magenta",
  "*color3:	#FF0093",
  "*color1:	yellow",
  "*color2:	#FF9300",
  "*color4:	green",
  "*color7:	#00FFD0",
  "*color5:	#8080FF",
  "*color6:	#00D0FF",

  0
};

XrmOptionDescRec options [] =
{
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
  struct hyper_state *hs;

  hs = &hyper_state;
  hs->hs_display = d;
  hs->hs_window = w;

  init (hs);

  hyper (hs);
}


static void
init (struct hyper_state *hs)
{
  Display *dpy;
  Window win;
  XGCValues gcv;
  Colormap cmap;
  unsigned long bg_pixel;
  int delay;
  float observer_z;

  dpy = hs->hs_display;
  win = hs->hs_window;

  observer_z = get_float_resource ("observer-z", "Float");
  if (observer_z < 1.125)
    observer_z = 1.125;
  /* hs->hs_observer_z = observer_z; */
  hs->hs_two_observer_z = 2.0 * observer_z;

  {
    int root;
    XWindowAttributes wa;
    int width;
    int height;

    root = get_boolean_resource("root", "Boolean");
    XGetWindowAttributes (dpy, win, &wa);
    XSelectInput(dpy, win, root ? ExposureMask :
		 wa.your_event_mask | ExposureMask | ButtonPressMask);
    
    width = wa.width;
    height = wa.height;
    cmap = wa.colormap;
    set_sizes (hs, width, height);
  }

  delay = get_integer_resource ("delay", "Integer");
  hs->hs_delay = delay;

  bg_pixel = get_pixel_resource ("background", "Background", dpy, cmap);

  if (mono_p)
    {
      GC black_gc;
      unsigned long fg_pixel;
      GC white_gc;

      gcv.function = GXcopy;
      gcv.foreground = bg_pixel;
      black_gc = XCreateGC (dpy, win, GCForeground|GCFunction, &gcv);
      fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
      gcv.foreground = fg_pixel ^ bg_pixel;
      white_gc = XCreateGC (dpy, win, GCForeground|GCFunction, &gcv);
      hs->hs_color_gcs[0] = black_gc;
      hs->hs_color_gcs[1] = white_gc;
    }
  else
    {
      int col;

      gcv.function = GXxor;
      for (col = 0; col < 8; col++)
	{
	  char buffer[16];
	  unsigned long fg_pixel;
	  GC color_gc;

	  sprintf (buffer, "color%d", col);
	  fg_pixel = get_pixel_resource (buffer, "Foreground", dpy, cmap);
	  gcv.foreground = fg_pixel ^ bg_pixel;
	  color_gc = XCreateGC (dpy, win, GCForeground|GCFunction, &gcv);
	  hs->hs_color_gcs[col] = color_gc;
	}
    }

  hs->hs_ref_ax = 1.0, hs->hs_ref_ay = 0.0, hs->hs_ref_az = 0.0, hs->hs_ref_aw = 0.0;
  hs->hs_ref_bx = 0.0, hs->hs_ref_by = 1.0, hs->hs_ref_bz = 0.0, hs->hs_ref_bw = 0.0;
  hs->hs_ref_cx = 0.0, hs->hs_ref_cy = 0.0, hs->hs_ref_cz = 1.0, hs->hs_ref_cw = 0.0;
  hs->hs_ref_dx = 0.0, hs->hs_ref_dy = 0.0, hs->hs_ref_dz = 0.0, hs->hs_ref_dw = 1.0;

  {
  double xy;
  double xz;
  double yz;
  double xw;
  double yw;
  double zw;
  double cos_xy, sin_xy;
  double cos_xz, sin_xz;
  double cos_yz, sin_yz;
  double cos_xw, sin_xw;
  double cos_yw, sin_yw;
  double cos_zw, sin_zw;

  xy = get_float_resource ("xy", "Float") * ANGLE_SCALE;
  xz = get_float_resource ("xz", "Float") * ANGLE_SCALE;
  yz = get_float_resource ("yz", "Float") * ANGLE_SCALE;
  xw = get_float_resource ("xw", "Float") * ANGLE_SCALE;
  yw = get_float_resource ("yw", "Float") * ANGLE_SCALE;
  zw = get_float_resource ("zw", "Float") * ANGLE_SCALE;

  cos_xy = cos (xy), sin_xy = sin (xy);
  hs->hs_cos_xy = cos_xy, hs->hs_sin_xy = sin_xy;
  cos_xz = cos (xz), sin_xz = sin (xz);
  hs->hs_cos_xz = cos_xz, hs->hs_sin_xz = sin_xz;
  cos_yz = cos (yz), sin_yz = sin (yz);
  hs->hs_cos_yz = cos_yz, hs->hs_sin_yz = sin_yz;
  cos_xw = cos (xw), sin_xw = sin (xw);
  hs->hs_cos_xw = cos_xw, hs->hs_sin_xw = sin_xw;
  cos_yw = cos (yw), sin_yw = sin (yw);
  hs->hs_cos_yw = cos_yw, hs->hs_sin_yw = sin_yw;
  cos_zw = cos (zw), sin_zw = sin (zw);
  hs->hs_cos_zw = cos_zw, hs->hs_sin_zw = sin_zw;
  }
}


static void
hyper (struct hyper_state *hs)
{
  int roted;

  roted = 0;

  for (;;)
    {
      int icon;
      int resize;
      char moved[POINT_COUNT];
      int redraw;
      int stop;
      int delay;

      check_events (hs);

      icon = hs->hs_icon;
      resize = hs->hs_resize;
      if (icon || !(roted | resize))
	goto skip1;

      {
	float observer_z;
	float unit_scale;
	float offset_x;
	float offset_y;
	double az, bz, cz, dz;
	double sum_z;
	double ax, bx, cx, dx;
	double sum_x;
	double ay, by, cy, dy;
	double sum_y;
	struct point_state *ps;
	int old_x;
	int old_y;
	double mul;
	double xf;
	double yf;
	int new_x;
	int new_y;
	int mov;


#define compute(as,bs,cs,ds,i) \
  az = hs->hs_ref_az; bz = hs->hs_ref_bz; cz = hs->hs_ref_cz; dz = hs->hs_ref_dz; \
  ax = hs->hs_ref_ax; bx = hs->hs_ref_bx; cx = hs->hs_ref_cx; dx = hs->hs_ref_dx; \
  ay = hs->hs_ref_ay; by = hs->hs_ref_by; cy = hs->hs_ref_cy; dy = hs->hs_ref_dy; \
  sum_z = as az bs bz cs cz ds dz; \
  observer_z = hs->hs_two_observer_z; \
  unit_scale = hs->hs_unit_scale; \
  sum_x = as ax bs bx cs cx ds dx; \
  sum_y = as ay bs by cs cy ds dy; \
  ps = &hs->hs_points[i]; \
  mul = unit_scale / (observer_z - sum_z); \
  offset_x = hs->hs_offset_x; \
  offset_y = hs->hs_offset_y; \
  old_x = ps->new_x; \
  old_y = ps->new_y; \
  xf = sum_x * mul + offset_x; \
  yf = sum_y * mul + offset_y; \
  new_x = (int)rint(xf); \
  new_y = (int)rint(yf); \
  ps->old_x = old_x; \
  ps->old_y = old_y; \
  ps->new_x = new_x; \
  ps->new_y = new_y; \
  mov = old_x != new_x || old_y != new_y; \
  moved[i] = mov;

	compute (-, -, -, -, 0);
	compute (-, -, -, +, 1);
	compute (-, -, +, -, 2);
	compute (-, -, +, +, 3);
	compute (-, +, -, -, 4);
	compute (-, +, -, +, 5);
	compute (-, +, +, -, 6);
	compute (-, +, +, +, 7);
	compute (+, -, -, -, 8);
	compute (+, -, -, +, 9);
	compute (+, -, +, -, 10);
	compute (+, -, +, +, 11);
	compute (+, +, -, -, 12);
	compute (+, +, -, +, 13);
	compute (+, +, +, -, 14);
	compute (+, +, +, +, 15);
      }

    skip1:
      icon = hs->hs_icon;
      redraw = hs->hs_redraw;
      if (icon || !(roted | redraw))
	goto skip2;

      {
	int lc;
	const struct line_info *lip;
	int mono;
	Display *dpy;
	Window win;

	lc = LINE_COUNT;
	lip = &line_table[0];
	mono = mono_p;
	dpy = hs->hs_display;
	win = hs->hs_window;

	while (--lc >= 0)
	  {
	    int ip;
	    int iq;
	    int col;
	    struct point_state *sp;
	    struct point_state *sq;
	    int mov_p;
	    int mov_q;
	    GC erase_gc;
	    GC draw_gc;
	    int p_x;
	    int p_y;
	    int q_x;
	    int q_y;

	    ip = lip->li_ip;
	    iq = lip->li_iq;
	    col = lip->li_color;
	    lip++;
	    mov_p = moved[ip];
	    mov_q = moved[iq];
	    if (!(redraw | mov_p | mov_q))
	      continue;

	    sp = &hs->hs_points[ip];
	    sq = &hs->hs_points[iq];

	    if (mono)
	      {
		erase_gc = hs->hs_color_gcs[0];
		draw_gc = hs->hs_color_gcs[1];
	      }
	    else
	      {
		GC color_gc;

		color_gc = hs->hs_color_gcs[col];
		erase_gc = color_gc;
		draw_gc = color_gc;
	      }

	    if (!redraw)
	      {
		p_x = sp->old_x;
		p_y = sp->old_y;
		q_x = sq->old_x;
		q_y = sq->old_y;
		XDrawLine (dpy, win, erase_gc, p_x, p_y, q_x, q_y);
	      }

	    p_x = sp->new_x;
	    p_y = sp->new_y;
	    q_x = sq->new_x;
	    q_y = sq->new_y;
	    XDrawLine (dpy, win, draw_gc, p_x, p_y, q_x, q_y);
	  }

        XFlush (dpy);
      }

    skip2:
      stop = hs->hs_stop;
      roted = 0;
      if (stop)
	goto skip3;

      roted = 1;

      {
	double cos_a;
	double sin_a;
	double old_u;
	double old_v;
	double new_u;
	double new_v;

 /* If you get error messages about the following forms, and you think you're
    using an ANSI C conforming compiler, then you're mistaken.  Possibly you're
    mixing an ANSI compiler with a non-ANSI preprocessor, or vice versa.
    Regardless, your system is broken; it's not a bug in this program.
  */
#if defined(__STDC__) || defined(__ANSI_CPP__)

#define rotate(name,dim0,dim1) \
  old_u = hs->hs_ref_##name##dim0; \
  old_v = hs->hs_ref_##name##dim1; \
  new_u = old_u * cos_a + old_v * sin_a; \
  new_v = old_v * cos_a - old_u * sin_a; \
  hs->hs_ref_##name##dim0 = new_u; \
  hs->hs_ref_##name##dim1 = new_v;

#define rotates(dim0,dim1) \
  if (hs->hs_sin_##dim0##dim1 != 0) { \
    cos_a = hs->hs_cos_##dim0##dim1; \
    sin_a = hs->hs_sin_##dim0##dim1; \
    rotate(a,dim0,dim1); \
    rotate(b,dim0,dim1); \
    rotate(c,dim0,dim1); \
    rotate(d,dim0,dim1); \
  }

#else /* !__STDC__, courtesy of Andreas Luik <luik@isa.de> */

#define rotate(name,dim0,dim1) \
  old_u = hs->hs_ref_/**/name/**/dim0; \
  old_v = hs->hs_ref_/**/name/**/dim1; \
  new_u = old_u * cos_a + old_v * sin_a; \
  new_v = old_v * cos_a - old_u * sin_a; \
  hs->hs_ref_/**/name/**/dim0 = new_u; \
  hs->hs_ref_/**/name/**/dim1 = new_v;

#define rotates(dim0,dim1) \
  if (hs->hs_sin_/**/dim0/**/dim1 != 0) { \
    cos_a = hs->hs_cos_/**/dim0/**/dim1; \
    sin_a = hs->hs_sin_/**/dim0/**/dim1; \
    rotate(a,dim0,dim1); \
    rotate(b,dim0,dim1); \
    rotate(c,dim0,dim1); \
    rotate(d,dim0,dim1); \
  }

#endif /* !__STDC__ */

	rotates (x,y);
	rotates (x,z);
	rotates (y,z);
	rotates (x,w);
	rotates (y,w);
	rotates (z,w);
      }

    skip3:
      /* stop = hs->hs_stop; */
      delay = hs->hs_delay;
      if (stop && delay < 10000)
	delay = 10000;
      if (delay > 0)
	usleep (delay);
    }
}


static void
check_events (struct hyper_state *hs)
{
  Display *dpy;
  int count;
  int ic;
  int resize;
  Window win;
  int redraw;

  dpy = hs->hs_display;
  count = XEventsQueued (dpy, QueuedAfterReading);
  ic = count;
  hs->hs_resize = 0;
  hs->hs_redraw = 0;

  while (--ic >= 0)
    {
      XEvent	e;

      XNextEvent (dpy, &e);

      switch (e.type)
	{
	case Expose:
	  hs->hs_icon = 0;
	  hs->hs_redraw = 1;
	  break;

	case ConfigureNotify:
	  hs->hs_icon = 0;
	  hs->hs_resize = 1;
	  hs->hs_redraw = 1;
	  break;

	case ButtonPress:
	  switch (e.xbutton.button)
	    {
	    case 2:
	      hs->hs_stop = !hs->hs_stop;
	      break;
	    default:
	      break;
	    }
	  break;

	case UnmapNotify:
	  hs->hs_icon = 1;
	  hs->hs_redraw = 0;
	  break;

	default:
	  screenhack_handle_event(dpy, &e);
	  break;
	}
    }

  resize = hs->hs_resize;
  win = hs->hs_window;
  if (resize)
    {
      XWindowAttributes wa;
      int width;
      int height;

      XGetWindowAttributes (dpy, win, &wa);
      width = wa.width;
      height = wa.height;
      set_sizes (&hyper_state, width, height);
    }

  redraw = hs->hs_redraw;
  if (redraw)
    XClearWindow (dpy, win);
}


static void
set_sizes (struct hyper_state *hs, int width, int height)
{
  double observer_z;
  int min_dim;
  double var;
  double offset_x;
  double offset_y;
  double unit_scale;

  observer_z = 0.5 * hs->hs_two_observer_z;
  min_dim = width < height ? width : height;
  var = sqrt(observer_z * observer_z - 1.0);
  offset_x = 0.5 * (double)(width - 1);
  offset_y = 0.5 * (double)(height - 1);
  unit_scale = 0.4 * min_dim * var;
  hs->hs_offset_x = (float)offset_x;
  hs->hs_offset_y = (float)offset_y;
  hs->hs_unit_scale = (float)unit_scale;
}


/* data */

static const struct line_info line_table[LINE_COUNT] =
{
    { 0, 1, 0, },
    { 0, 2, 0, },
    { 1, 3, 0, },
    { 2, 3, 0, },
    { 4, 5, 1, },
    { 4, 6, 1, },
    { 5, 7, 1, },
    { 6, 7, 1, },
    { 0, 4, 4, },
    { 0, 8, 4, },
    { 4, 12, 4, },
    { 8, 12, 4, },
    { 1, 5, 5, },
    { 1, 9, 5, },
    { 5, 13, 5, },
    { 9, 13, 5, },
    { 2, 6, 6, },
    { 2, 10, 6, },
    { 6, 14, 6, },
    { 10, 14, 6, },
    { 3, 7, 7, },
    { 3, 11, 7, },
    { 7, 15, 7, },
    { 11, 15, 7, },
    { 8, 9, 2, },
    { 8, 10, 2, },
    { 9, 11, 2, },
    { 10, 11, 2, },
    { 12, 13, 3, },
    { 12, 14, 3, },
    { 13, 15, 3, },
    { 14, 15, 3, },
};

