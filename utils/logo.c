/* xscreensaver, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This draws the XScreenSaver logo.
   Logo designed by Angela Goodman <rzr_grl@yahoo.com>

   The reason this C looks a lot like PostScript is that the logo was
   designed in Illustrator, and then I (jwz) translated the EPS to C
   by hand.
 */

#include "utils.h"
#include "resources.h"
#include "yarandom.h"
#include "spline.h"

extern const char *progname;

typedef struct {
  Display *dpy;
  Drawable drawable;
  GC gc;

  double x_scale;
  double y_scale;
  double translate_x;
  double translate_y;
  double current_x;
  double current_y;
  double path_x;
  double path_y;

  int y_origin;

  unsigned long logo_bg_pixel;
  unsigned long logo_fg_pixel;
  unsigned long monitor_fg_pixel;
  unsigned long monitor_bg_pixel;
  unsigned long flame1_fg_pixel;
  unsigned long flame1_bg_pixel;
  unsigned long flame2_fg_pixel;
  unsigned long flame2_bg_pixel;

  XPoint points[1024];
  int npoints;

} logo_state;


#undef UNDEF
#define UNDEF -65535

static void
reset (logo_state *state)
{
  state->x_scale = 1;
  state->y_scale = 1;
  state->translate_x = 0;
  state->translate_y = 0;
  state->current_x = UNDEF;
  state->current_y = UNDEF;
  state->path_x = UNDEF;
  state->path_y = UNDEF;
  state->npoints = 0;
}

static void
scale (logo_state *state, double x, double y)
{
  state->x_scale *= x;
  state->y_scale *= y;
}

static void
translate (logo_state *state, double x, double y)
{
  state->translate_x += x;
  state->translate_y += y;
}

static void
newpath (logo_state *state)
{
  state->current_x = UNDEF;
  state->current_y = UNDEF;
  state->path_x = UNDEF;
  state->path_y = UNDEF;

  state->npoints = 0;
}

static void
moveto (logo_state *state, double x, double y)
{
  x += state->translate_x;
  y += state->translate_y;
  if (state->path_x == UNDEF)
    {
      state->path_x = x;
      state->path_y = y;
    }
  state->current_x = x;
  state->current_y = y;
}

static void
lineto (logo_state *state, double x, double y)
{
  int x1 =                   (int) (state->x_scale * state->current_x);
  int y1 = state->y_origin - (int) (state->y_scale * state->current_y);
  int x2 =                   (int) (state->x_scale * (x + state->translate_x));
  int y2 = state->y_origin - (int) (state->y_scale * (y + state->translate_y));

  if (state->current_x == UNDEF) abort();

  if (state->npoints == 0 ||
      state->points[state->npoints-1].x != x1 ||
      state->points[state->npoints-1].y != y1)
    {
      state->points[state->npoints].x = x1;
      state->points[state->npoints].y = y1;
      state->npoints++;
    }
  state->points[state->npoints].x = x2;
  state->points[state->npoints].y = y2;
  state->npoints++;

  moveto(state, x, y);
}

static void
curveto (logo_state *state,
         double x1, double y1,
         double x2, double y2,
         double x3, double y3)
{
  spline s;
  double sx[4], sy[4];
  int i;

  if (state->current_x == UNDEF) abort();

  sx[0] =                    state->x_scale * state->current_x;
  sy[0] = state->y_origin - (state->y_scale * state->current_y);
  sx[1] =                    state->x_scale * (x1 + state->translate_x);
  sy[1] = state->y_origin - (state->y_scale * (y1 + state->translate_y));
  sx[2] =                    state->x_scale * (x2 + state->translate_x);
  sy[2] = state->y_origin - (state->y_scale * (y2 + state->translate_y));
  sx[3] =                    state->x_scale * (x3 + state->translate_x);
  sy[3] = state->y_origin - (state->y_scale * (y3 + state->translate_y));

  memset(&s, 0, sizeof(s));
  s.control_x = sx;
  s.control_y = sy;
  s.n_controls = 4;

  s.allocated_points = 50;  /* just the initial buffer size */
  s.points = (XPoint *) calloc (s.allocated_points, sizeof (*s.points));
  compute_spline(&s);

  for (i = 0; i < s.n_points; i++)
    {
      state->points[state->npoints].x = s.points[i].x;
      state->points[state->npoints].y = s.points[i].y;
      state->npoints++;
    }

  state->current_x = (state->points[state->npoints-1].x
                      / state->x_scale);
  state->current_y = ((state->y_origin - state->points[state->npoints-1].y)
                      / state->y_scale);
  free (s.points);
}


static void
closepath (logo_state *state)
{
  if (state->current_x != UNDEF)
    lineto (state,
            state->path_x - state->translate_x,
            state->path_y - state->translate_y);
}

static void
stroke (logo_state *state)
{
  XDrawLines (state->dpy, state->drawable, state->gc,
              state->points, state->npoints,
              CoordModeOrigin);
}

static void
fill (logo_state *state)
{
  XFillPolygon (state->dpy, state->drawable, state->gc,
                state->points, state->npoints,
                Complex, CoordModeOrigin);
}

static void
setlinewidth (logo_state *state, double w)
{
  XSetLineAttributes (state->dpy, state->gc,
                      (int) (w * state->x_scale),
                      LineSolid, CapRound, JoinRound);
}

static void
setcolor (logo_state *state, unsigned long pixel)
{
  XSetForeground (state->dpy, state->gc, pixel);
}


static void
border (logo_state *state)
{
  setlinewidth(state, 4);

  newpath (state);
  moveto (state, 390.7588, 335.9102);
  lineto (state, 390.7588, 333.4834);
  lineto (state, 388.5283, 331.5156);
  lineto (state, 385.7754, 331.5156);
  lineto (state, 220.2090, 331.5156);
  lineto (state, 217.4570, 331.5156);
  lineto (state, 215.2256, 333.4834);
  lineto (state, 215.2256, 335.9102);
  lineto (state, 215.2256, 481.3916);
  lineto (state, 215.2256, 483.8184);
  lineto (state, 217.4570, 485.7856);
  lineto (state, 220.2090, 485.7856);
  lineto (state, 385.7754, 485.7856);
  lineto (state, 388.5283, 485.7856);
  lineto (state, 390.7588, 483.8184);
  lineto (state, 390.7588, 481.3916);
  lineto (state, 390.7588, 335.9102);
  closepath (state);

  setcolor (state, state->logo_bg_pixel);
  fill (state);

  setcolor (state, state->logo_fg_pixel);
  stroke (state);
}

static void
monitor (logo_state *state)
{
  setlinewidth (state, 0);

  setcolor (state, state->monitor_fg_pixel);

  newpath (state);
  moveto (state, 377.3408, 366.4893);
  lineto (state, 377.3408, 363.1758);
  lineto (state, 374.6543, 360.4893);
  lineto (state, 371.3408, 360.4893);
  lineto (state, 234.6743, 360.4893);
  lineto (state, 231.3608, 360.4893);
  lineto (state, 228.6743, 363.1758);
  lineto (state, 228.6743, 366.4893);
  lineto (state, 228.6743, 461.1563);
  lineto (state, 228.6743, 464.4697);
  lineto (state, 231.3608, 467.1563);
  lineto (state, 234.6743, 467.1563);
  lineto (state, 371.3408, 467.1563);
  lineto (state, 374.6543, 467.1563);
  lineto (state, 377.3408, 464.4697);
  lineto (state, 377.3408, 461.1563);
  lineto (state, 377.3408, 366.4893);
  closepath (state);
  fill (state);

  newpath (state);
  moveto (state, 325.7354, 369.5391);
  lineto (state, 322.2354, 375.0391);
  lineto (state, 318.2354, 351.5391);
  lineto (state, 342.2354, 344.5391);
  lineto (state, 265.4619, 344.5391);
  lineto (state, 289.4619, 351.5391);
  lineto (state, 285.4619, 375.0391);
  lineto (state, 281.9619, 369.5391);
  lineto (state, 325.7354, 369.5391);
  closepath (state);
  fill (state);

  newpath (state);
  moveto (state, 342.9453, 343.0273);
  lineto (state, 342.9453, 342.1924);
  lineto (state, 342.2539, 341.5156);
  lineto (state, 341.4043, 341.5156);
  lineto (state, 266.0039, 341.5156);
  lineto (state, 265.1523, 341.5156);
  lineto (state, 264.4639, 342.1924);
  lineto (state, 264.4639, 343.0273);
  lineto (state, 264.4639, 343.0273);
  lineto (state, 264.4639, 343.8623);
  lineto (state, 265.1523, 344.5391);
  lineto (state, 266.0039, 344.5391);
  lineto (state, 341.4043, 344.5391);
  lineto (state, 342.2539, 344.5391);
  lineto (state, 342.9453, 343.8623);
  lineto (state, 342.9453, 343.0273);
  lineto (state, 342.9453, 343.0273);
  closepath (state);
  stroke (state);
  fill (state);

  newpath (state);
  moveto (state, 360.3711, 382.1641);
  lineto (state, 360.3711, 378.8506);
  lineto (state, 357.6846, 376.1641);
  lineto (state, 354.3711, 376.1641);
  lineto (state, 253.0381, 376.1641);
  lineto (state, 249.7246, 376.1641);
  lineto (state, 247.0381, 378.8506);
  lineto (state, 247.0381, 382.1641);
  lineto (state, 247.0381, 444.1641);
  lineto (state, 247.0381, 447.4775);
  lineto (state, 249.7246, 450.1641);
  lineto (state, 253.0381, 450.1641);
  lineto (state, 354.3711, 450.1641);
  lineto (state, 357.6846, 450.1641);
  lineto (state, 360.3711, 447.4775);
  lineto (state, 360.3711, 444.1641);
  lineto (state, 360.3711, 382.1641);
  closepath (state);

  setcolor (state, state->monitor_bg_pixel);
  fill (state);
}


static void
flames_0a (logo_state *state)
{
  setlinewidth (state, 2);

  newpath (state);
  moveto (state, 268.1118, 375.1055);
  lineto (state, 278.0723, 371.9902);
  lineto (state, 285.0166, 362.1172);
  lineto (state, 307.6953, 356.7012);
  lineto (state, 328.5361, 364.6758);
  lineto (state, 339.9619, 383.5098);
  lineto (state, 345.8936, 389.6660);
  lineto (state, 343.8018, 403.9888);
  lineto (state, 340.0576, 432.1499);
  lineto (state, 332.6553, 443.0522);
  lineto (state, 319.5771, 453.9092);
  lineto (state, 308.0566, 485.2700);
  lineto (state, 318.7891, 505.4521);
  lineto (state, 325.9775, 515.9902);
  lineto (state, 288.2168, 468.6289);
  lineto (state, 304.2290, 442.3589);
  lineto (state, 318.9365, 416.1494);
  lineto (state, 313.5049, 417.6880);
  lineto (state, 311.5088, 418.7856);
  lineto (state, 306.4565, 424.1489);
  lineto (state, 308.7578, 428.8916);
  lineto (state, 297.6426, 440.9727);
  lineto (state, 282.4565, 457.4297);
  lineto (state, 280.1528, 461.7734);
  lineto (state, 296.6025, 495.0522);
  lineto (state, 301.0166, 499.0298);
  lineto (state, 277.2920, 523.6685);
  lineto (state, 275.7539, 530.4106);
  lineto (state, 277.2920, 523.6685);
  lineto (state, 293.3374, 492.3101);
  lineto (state, 277.8828, 480.4922);
  lineto (state, 265.1763, 462.8696);
  lineto (state, 265.9390, 450.7354);
  lineto (state, 264.7095, 445.1323);
  lineto (state, 268.3765, 438.5493);
  lineto (state, 275.4170, 425.7495);
  lineto (state, 278.5762, 413.9326);
  lineto (state, 284.3765, 406.5493);
  lineto (state, 284.7471, 391.6973);
  lineto (state, 275.8027, 404.2261);
  lineto (state, 273.8164, 414.5488);
  lineto (state, 252.3760, 431.5088);
  lineto (state, 257.4292, 460.0386);
  lineto (state, 269.9766, 491.3496);
  lineto (state, 240.8564, 433.4292);
  lineto (state, 244.6030, 410.4658);
  lineto (state, 256.2168, 392.4688);
  lineto (state, 270.3921, 380.3086);
  lineto (state, 268.1118, 375.1055);
  closepath (state);

  setcolor (state, state->flame1_bg_pixel);
  fill (state);

  setcolor (state, state->flame1_fg_pixel);
  stroke (state);
}


static void
flames_0b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 263.0464, 378.707);
  curveto (state, 271.0464, 370.0410, 280.3799, 358.7070, 303.0464, 360.0410);
  curveto (state, 325.7129, 361.3730, 339.7129, 382.0410, 341.7129, 392.7070);
  curveto (state, 343.7129, 403.3740, 337.7129, 432.7080, 327.0459, 444.0410);
  curveto (state, 316.3799, 455.3740, 304.3794, 488.0410, 313.7129, 504.0410);
  curveto (state, 323.0469, 520.0410, 283.7129, 470.7070, 299.7129, 443.3740);
  curveto (state, 315.7129, 416.0410, 310.0547, 417.6440, 306.7129, 420.7075);
  curveto (state, 302.7129, 424.3740, 305.1094, 429.3140, 293.3799, 442.0410);
  curveto (state, 277.7129, 459.0410, 275.3135, 463.5659, 292.3799, 494.0410);

  /* y = "2 copy curveto", e.g., curveto(x1, y1, x2, y2, x2, y2) */
  curveto (state, 297.0464, 502.3740, 272.3330, 528.0396, 272.3330, 528.0396);
  /* v = "currentpoint 6 2 roll curveto", e.g.,
     curveto (current_x, current_y, x1, x2, y1, y2) */
  curveto (state, 272.3330, 528.0396, 289.0469, 495.3745, 274.3799, 480.0410);
  curveto (state, 259.7129, 464.7075, 260.5073, 452.0679, 261.7129, 446.0410);
  curveto (state, 263.0464, 439.3740, 270.3799, 426.0410, 275.0464, 416.0410);
  curveto (state, 279.7129, 406.0410, 280.0986, 390.5684, 272.3799, 406.7075);
  curveto (state, 268.7129, 414.3740, 246.3794, 432.0405, 254.7129, 460.3740);
  curveto (state, 264.7129, 494.3740, 234.3799, 434.0410, 242.3799, 412.7075);
  curveto (state, 250.3799, 391.3730, 263.0464, 378.7070, 263.0464, 378.7070);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}


static void
flames_1a (logo_state *state)
{
  setlinewidth (state, 2);

  newpath (state);
  moveto (state, 268.1118, 375.1055);
  lineto (state, 278.0723, 371.9902);
  lineto (state, 285.0166, 362.1172);
  lineto (state, 307.6953, 356.7012);
  lineto (state, 328.5361, 364.6758);
  lineto (state, 339.9619, 383.5098);
  lineto (state, 345.8936, 389.666);
  lineto (state, 343.8018, 403.9888);
  lineto (state, 340.0576, 432.1499);
  lineto (state, 332.6553, 443.0522);
  lineto (state, 319.5771, 453.9092);
  lineto (state, 308.0566, 485.27);
  lineto (state, 318.7891, 505.4521);
  lineto (state, 325.9775, 515.9902);
  lineto (state, 288.2168, 468.6289);
  lineto (state, 304.229, 442.3589);
  lineto (state, 318.9365, 416.1494);
  lineto (state, 313.5049, 417.688);
  lineto (state, 311.5088, 418.7856);
  lineto (state, 306.4565, 424.1489);
  lineto (state, 308.7578, 428.8916);
  lineto (state, 297.6426, 440.9727);
  lineto (state, 282.4565, 457.4297);
  lineto (state, 280.1528, 461.7734);
  lineto (state, 296.6025, 495.0522);
  lineto (state, 301.0166, 499.0298);
  lineto (state, 277.292, 523.6685);
  lineto (state, 275.7539, 530.4106);
  lineto (state, 277.292, 523.6685);
  lineto (state, 293.3374, 492.3101);
  lineto (state, 277.8828, 480.4922);
  lineto (state, 265.1763, 462.8696);
  lineto (state, 265.939, 450.7354);
  lineto (state, 264.7095, 445.1323);
  lineto (state, 268.3765, 438.5493);
  lineto (state, 275.417, 425.7495);
  lineto (state, 278.5762, 413.9326);
  lineto (state, 284.3765, 406.5493);
  lineto (state, 284.7471, 391.6973);
  lineto (state, 275.8027, 404.2261);
  lineto (state, 273.8164, 414.5488);
  lineto (state, 252.376, 431.5088);
  lineto (state, 257.4292, 460.0386);
  lineto (state, 269.9766, 491.3496);
  lineto (state, 240.8564, 433.4292);
  lineto (state, 244.603, 410.4658);
  lineto (state, 256.2168, 392.4688);
  lineto (state, 270.3921, 380.3086);
  lineto (state, 268.1118, 375.1055);
  closepath (state);

  setcolor (state, state->flame1_bg_pixel);
  fill (state);

  setcolor (state, state->flame1_fg_pixel);
  stroke (state);
}

static void
flames_1b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 271.0469, 382.041);
  curveto (state, 279.0469, 373.375, 288.3804, 362.041, 311.0469, 363.375);
  curveto (state, 333.7129, 364.707, 347.7129, 385.375, 349.7129, 396.0405);
  curveto (state, 351.7129, 406.707, 347.3818, 437.8853, 335.0469, 447.374);
  curveto (state, 313.0, 464.3335, 312.3789, 491.374, 321.7129, 507.374);
  curveto (state, 331.0469, 523.374, 291.7134, 474.04, 307.7129, 446.707);
  curveto (state, 323.7129, 419.374, 318.0547, 420.9771, 314.7129, 424.0405);
  curveto (state, 310.7129, 427.707, 313.1094, 432.647, 301.3804, 445.374);
  curveto (state, 285.7134, 462.374, 283.314, 466.8989, 300.3804, 497.374);
  curveto (state, 305.0474, 505.707, 277.667, 518.9995, 277.667, 518.9995);
  curveto (state, 277.667, 518.9995, 297.0474, 498.7075, 282.3804, 483.374);
  curveto (state, 267.7134, 468.0405, 268.5078, 455.4009, 269.7134, 449.374);
  curveto (state, 271.0469, 442.707, 278.3804, 429.374, 283.0469, 419.374);
  curveto (state, 287.7134, 409.374, 288.0991, 393.9023, 280.3804, 410.0405);
  curveto (state, 276.7134, 417.707, 254.3799, 435.3735, 262.7134, 463.707);
  curveto (state, 272.7134, 497.707, 246.3335, 428.3335, 254.3335, 407.0);
  curveto (state, 262.3335, 385.666, 271.0469, 382.041, 271.0469, 382.041);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}


static void
flames_2a (logo_state *state)
{
  setlinewidth (state, 2);

  newpath (state);
  moveto (state, 268.1118, 375.1055);
  lineto (state, 278.0723, 371.9902);
  lineto (state, 285.0166, 362.1172);
  lineto (state, 307.6953, 356.7012);
  lineto (state, 328.5361, 364.6758);
  lineto (state, 335.0684, 383.8438);
  lineto (state, 341.0, 390.0);
  lineto (state, 338.9082, 404.3228);
  lineto (state, 340.0576, 432.1499);
  lineto (state, 332.6553, 443.0522);
  lineto (state, 319.5771, 453.9092);
  lineto (state, 312.6006, 479.4844);
  lineto (state, 323.333, 499.6665);
  lineto (state, 330.5215, 510.2046);
  lineto (state, 288.2168, 468.6289);
  lineto (state, 304.229, 442.3589);
  lineto (state, 318.9365, 416.1494);
  lineto (state, 313.5049, 417.688);
  lineto (state, 311.5088, 418.7856);
  lineto (state, 306.4565, 424.1489);
  lineto (state, 308.7578, 428.8916);
  lineto (state, 297.6426, 440.9727);
  lineto (state, 282.4565, 457.4297);
  lineto (state, 280.1528, 461.7734);
  lineto (state, 296.6025, 495.0522);
  lineto (state, 301.0166, 499.0298);
  lineto (state, 277.292, 523.6685);
  lineto (state, 275.7539, 530.4106);
  lineto (state, 277.292, 523.6685);
  lineto (state, 293.3374, 492.3101);
  lineto (state, 277.8828, 480.4922);
  lineto (state, 265.1763, 462.8696);
  lineto (state, 265.939, 450.7354);
  lineto (state, 264.7095, 445.1323);
  lineto (state, 268.3765, 438.5493);
  lineto (state, 275.417, 425.7495);
  lineto (state, 278.5762, 413.9326);
  lineto (state, 284.3765, 406.5493);
  lineto (state, 284.7471, 391.6973);
  lineto (state, 275.8027, 404.2261);
  lineto (state, 273.8164, 414.5488);
  lineto (state, 251.8291, 429.9214);
  lineto (state, 255.3203, 464.5493);
  lineto (state, 254.7437, 462.4072);
  lineto (state, 247.2534, 427.2969);
  lineto (state, 251.0, 404.3335);
  lineto (state, 262.6138, 386.3359);
  lineto (state, 270.3921, 380.3086);
  lineto (state, 268.1118, 375.1055);
  closepath (state);

  setcolor (state, state->flame1_bg_pixel);
  fill (state);

  setcolor (state, state->flame1_fg_pixel);
  stroke (state);
}

static void
flames_2b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 271.0469, 382.041);
  curveto (state, 279.0469, 373.375, 288.3804, 362.041, 311.0469, 363.375);
  curveto (state, 333.7129, 364.707, 336.333, 386.334, 338.333, 397.0);
  curveto (state, 340.333, 407.6665, 347.3818, 437.8853, 335.0469, 447.374);
  curveto (state, 313.0, 464.3335, 312.3789, 491.374, 321.7129, 507.374);
  curveto (state, 331.0469, 523.374, 291.7134, 474.04, 307.7129, 446.707);
  curveto (state, 323.7129, 419.374, 318.0547, 420.9771, 314.7129, 424.0405);
  curveto (state, 310.7129, 427.707, 313.1094, 432.647, 301.3804, 445.374);
  curveto (state, 285.7134, 462.374, 283.314, 466.8989, 300.3804, 497.374);
  curveto (state, 305.0474, 505.707, 277.667, 518.9995, 277.667, 518.9995);
  curveto (state, 277.667, 518.9995, 297.0474, 498.7075, 282.3804, 483.374);
  curveto (state, 267.7134, 468.0405, 268.5078, 455.4009, 269.7134, 449.374);
  curveto (state, 271.0469, 442.707, 278.3804, 429.374, 283.0469, 419.374);
  curveto (state, 287.7134, 409.374, 288.0991, 393.9023, 280.3804, 410.0405);
  curveto (state, 276.7134, 417.707, 254.3799, 435.3735, 262.7134, 463.707);
  curveto (state, 272.7134, 497.707, 246.3335, 428.3335, 254.3335, 407.0);
  curveto (state, 262.3335, 385.666, 271.0469, 382.041, 271.0469, 382.041);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}


static void
flames_3a (logo_state *state)
{
  setlinewidth (state, 2);

  newpath (state);
  moveto (state, 268.1118, 375.1055);
  lineto (state, 278.0723, 371.9902);
  lineto (state, 285.0166, 362.1172);
  lineto (state, 307.6953, 356.7012);
  lineto (state, 328.5361, 364.6758);
  lineto (state, 339.9619, 383.5098);
  lineto (state, 345.8936, 389.666);
  lineto (state, 343.8018, 403.9888);
  lineto (state, 334.4023, 428.0977);
  lineto (state, 327.0, 439.0);
  lineto (state, 309.667, 469.6665);
  lineto (state, 308.2676, 473.4844);
  lineto (state, 319.0, 493.6665);
  lineto (state, 307.667, 482.0);
  lineto (state, 288.2168, 468.6289);
  lineto (state, 304.229, 442.3589);
  lineto (state, 318.9365, 416.1494);
  lineto (state, 313.5049, 417.688);
  lineto (state, 311.5088, 418.7856);
  lineto (state, 306.4565, 424.1489);
  lineto (state, 305.4487, 426.2524);
  lineto (state, 294.3335, 438.3335);
  lineto (state, 279.1475, 454.7905);
  lineto (state, 280.1528, 461.7734);
  lineto (state, 296.6025, 495.0522);
  lineto (state, 301.0166, 499.0298);
  lineto (state, 299.5381, 511.5913);
  lineto (state, 298.0, 518.3335);
  lineto (state, 299.5381, 511.5913);
  lineto (state, 293.3374, 492.3101);
  lineto (state, 277.8828, 480.4922);
  lineto (state, 265.1763, 462.8696);
  lineto (state, 265.939, 450.7354);
  lineto (state, 264.7095, 445.1323);
  lineto (state, 268.3765, 438.5493);
  lineto (state, 275.417, 425.7495);
  lineto (state, 278.5762, 413.9326);
  lineto (state, 284.3765, 406.5493);
  lineto (state, 284.7471, 391.6973);
  lineto (state, 275.8027, 404.2261);
  lineto (state, 273.8164, 414.5488);
  lineto (state, 252.376, 431.5088);
  lineto (state, 257.4292, 460.0386);
  lineto (state, 269.9766, 491.3496);
  lineto (state, 240.8564, 433.4292);
  lineto (state, 244.603, 410.4658);
  lineto (state, 256.2168, 392.4688);
  lineto (state, 270.3921, 380.3086);
  lineto (state, 268.1118, 375.1055);
  closepath (state);

  setcolor (state, state->flame1_bg_pixel);
  fill (state);

  setcolor (state, state->flame1_fg_pixel);
  stroke (state);
}

static void
flames_3b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 263.0464, 378.707);
  curveto (state, 271.0464, 370.041, 280.3799, 358.707, 303.0464, 360.041);
  curveto (state, 325.7129, 361.373, 340.4473, 381.9297, 341.7129, 392.707);
  curveto (state, 343.0, 403.6665, 331.667, 428.3335, 321.0, 439.6665);
  curveto (state, 310.334, 450.9995, 308.584, 461.5942, 310.667, 480.0);
  curveto (state, 312.667, 497.6665, 299.9536, 463.626, 304.6665, 438.3335);
  lineto (state, 305.3335, 432.3335);
  curveto (state, 309.0, 419.3335, 316.667, 408.3335, 288.0, 436.6665);
  curveto (state, 271.5576, 452.9175, 275.3135, 463.5659, 292.3799, 494.041);
  curveto (state, 297.0464, 502.374, 272.333, 528.0396, 272.333, 528.0396);
  curveto (state, 272.333, 528.0396, 289.0469, 495.3745, 274.3799, 480.041);
  curveto (state, 259.7129, 464.7075, 260.5073, 452.0679, 261.7129, 446.041);
  curveto (state, 263.0464, 439.374, 270.3799, 426.041, 275.0464, 416.041);
  curveto (state, 279.7129, 406.041, 283.3696, 392.5908, 272.3799, 406.7075);
  curveto (state, 268.0, 412.3335, 246.3794, 432.0405, 254.7129, 460.374);
  curveto (state, 264.7129, 494.374, 240.6665, 435.0, 248.6665, 413.6665);
  curveto (state, 256.6665, 392.332, 263.0464, 378.707, 263.0464, 378.707);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}


static void
flames_4a (logo_state *state)
{
  flames_3a (state);
}

static void
flames_4b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 263.0464, 378.707);
  curveto (state, 271.0464, 370.041, 280.3799, 358.707, 303.0464, 360.041);
  curveto (state, 325.7129, 361.373, 340.4473, 381.9297, 341.7129, 392.707);
  curveto (state, 343.0, 403.6665, 331.667, 428.3335, 321.0, 439.6665);
  curveto (state, 310.334, 450.9995, 306.5693, 463.2358, 319.667, 476.3335);
  curveto (state, 320.667, 477.3335, 299.9536, 463.626, 304.6665, 438.3335);
  lineto (state, 305.3335, 432.3335);
  curveto (state, 309.0, 419.3335, 316.667, 408.3335, 288.0, 436.6665);
  curveto (state, 271.5576, 452.9175, 275.3135, 463.5659, 292.3799, 494.041);
  curveto (state, 297.0464, 502.374, 272.333, 528.0396, 272.333, 528.0396);
  curveto (state, 272.333, 528.0396, 289.0469, 495.3745, 274.3799, 480.041);
  curveto (state, 259.7129, 464.7075, 260.5073, 452.0679, 261.7129, 446.041);
  curveto (state, 263.0464, 439.374, 270.3799, 426.041, 275.0464, 416.041);
  curveto (state, 279.7129, 406.041, 313.667, 376.333, 272.3799, 406.7075);
  curveto (state, 265.9966, 411.4038, 251.333, 417.0, 259.6665, 445.3335);
  curveto (state, 269.6665, 479.3335, 247.6665, 417.0, 248.6665, 413.6665);
  curveto (state, 255.2134, 391.8418, 263.0464, 378.707, 263.0464, 378.707);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}


static void flames_6a (logo_state *);
static void flames_6b (logo_state *);

static void
flames_5a (logo_state *state)
{
  unsigned long a = state->flame1_bg_pixel;
  unsigned long b = state->flame2_bg_pixel;
  state->flame1_bg_pixel = b;
  state->flame2_bg_pixel = a;
  flames_6a (state);
  state->flame1_bg_pixel = a;
  state->flame2_bg_pixel = b;
}

static void
flames_5b (logo_state *state)
{
  unsigned long a = state->flame1_bg_pixel;
  unsigned long b = state->flame2_bg_pixel;
  state->flame1_bg_pixel = b;
  state->flame2_bg_pixel = a;
  flames_6b (state);
  state->flame1_bg_pixel = a;
  state->flame2_bg_pixel = b;
}


static void
flames_6a (logo_state *state)
{
  flames_3a (state);
}

static void
flames_6b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 263.0464, 378.707);
  curveto (state, 271.0464, 370.041, 280.3799, 358.707, 303.0464, 360.041);
  curveto (state, 325.7129, 361.373, 340.4473, 381.9297, 341.7129, 392.707);
  curveto (state, 343.0, 403.6665, 331.667, 428.3335, 321.0, 439.6665);
  curveto (state, 310.334, 450.9995, 306.5693, 463.2358, 319.667, 476.3335);
  curveto (state, 320.667, 477.3335, 299.9536, 463.626, 304.6665, 438.3335);
  lineto (state, 305.3335, 432.3335);
  curveto (state, 309.0, 419.3335, 316.667, 408.3335, 288.0, 436.6665);
  curveto (state, 271.5576, 452.9175, 275.3135, 463.5659, 292.3799, 494.041);
  curveto (state, 297.0464, 502.374, 272.333, 528.0396, 272.333, 528.0396);
  curveto (state, 272.333, 528.0396, 289.0469, 495.3745, 274.3799, 480.041);
  curveto (state, 259.7129, 464.7075, 260.5073, 452.0679, 261.7129, 446.041);
  curveto (state, 263.0464, 439.374, 270.3799, 426.041, 275.0464, 416.041);
  curveto (state, 279.7129, 406.041, 313.667, 376.333, 272.3799, 406.7075);
  curveto (state, 265.9966, 411.4038, 251.333, 417.0, 259.6665, 445.3335);
  curveto (state, 269.6665, 479.3335, 247.6665, 417.0, 248.6665, 413.6665);
  curveto (state, 255.2134, 391.8418, 263.0464, 378.707, 263.0464, 378.707);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}

static void
flames_6c (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state, 293.7134, 370.5859);
  curveto (state, 299.6665, 366.333, 303.9854, 362.6934, 317.7656, 359.4023);
  curveto (state, 331.0, 361.667, 337.3711, 375.6924, 340.9766, 379.4326);
  curveto (state, 339.7051, 388.1357, 333.9941, 402.7852, 329.4961, 409.4097);
  curveto (state, 318.9629, 428.0435, 310.1455, 425.4028, 316.667, 437.6665);
  curveto (state, 309.7803, 430.5771, 305.9297, 427.4131, 315.6602, 411.4507);
  curveto (state, 324.5957, 395.5244, 321.2949, 396.46, 320.082, 397.127);
  curveto (state, 317.0137, 400.3857, 316.4004, 401.6636, 309.6465, 409.0044);
  curveto (state, 300.4189, 419.0044, 299.2461, 416.7344, 309.2402, 436.9556);
  curveto (state, 310.2539, 440.3984, 315.3184, 446.2725, 314.1016, 457.4121);
  curveto (state, 315.0371, 453.3154, 309.041, 441.8022, 299.6504, 434.6216);
  curveto (state, 291.9297, 423.9136, 292.3931, 416.54, 291.646, 413.1357);
  curveto (state, 293.874, 409.1357, 298.1523, 401.3579, 300.0718, 394.1777);
  curveto (state, 303.5962, 389.6914, 289.9995, 396.4463, 289.9995, 396.4463);
  curveto (state, 285.3413, 401.5103, 269.5962, 401.9976, 272.6665, 419.3335);
  curveto (state, 269.3335, 402.0, 277.1523, 406.0244, 279.4287, 392.0713);
  curveto (state, 286.4858, 381.1357, 291.6665, 375.0, 293.7134, 370.5859);
  closepath (state);

  setcolor (state, state->flame1_bg_pixel);
  fill (state);

  setcolor (state, state->flame1_fg_pixel);
  stroke (state);
}


static void
flames_7a (logo_state *state)
{
  setlinewidth (state, 2);

  newpath (state);
  moveto (state, 268.1118, 375.1055);
  lineto (state, 278.0723, 371.9902);
  lineto (state, 285.0166, 362.1172);
  lineto (state, 307.6953, 356.7012);
  lineto (state, 328.5361, 364.6758);
  lineto (state, 339.9619, 383.5098);
  lineto (state, 345.8936, 389.666);
  lineto (state, 343.8018, 403.9888);
  lineto (state, 340.0576, 432.1499);
  lineto (state, 332.6553, 443.0522);
  lineto (state, 319.5771, 453.9092);
  lineto (state, 308.0566, 485.27);
  lineto (state, 318.7891, 505.4521);
  lineto (state, 325.9775, 515.9902);
  lineto (state, 288.2168, 468.6289);
  lineto (state, 304.229, 442.3589);
  lineto (state, 318.9365, 416.1494);
  lineto (state, 313.5049, 417.688);
  lineto (state, 311.5088, 418.7856);
  lineto (state, 306.4565, 424.1489);
  lineto (state, 308.7578, 428.8916);
  lineto (state, 297.6426, 440.9727);
  lineto (state, 282.4565, 457.4297);
  lineto (state, 280.1528, 461.7734);
  lineto (state, 296.6025, 495.0522);
  lineto (state, 301.0166, 499.0298);
  lineto (state, 277.292, 523.6685);
  lineto (state, 275.7539, 530.4106);
  lineto (state, 277.292, 523.6685);
  lineto (state, 293.3374, 492.3101);
  lineto (state, 277.8828, 480.4922);
  lineto (state, 265.1763, 462.8696);
  lineto (state, 265.939, 450.7354);
  lineto (state, 264.7095, 445.1323);
  lineto (state, 268.3765, 438.5493);
  lineto (state, 275.417, 425.7495);
  lineto (state, 278.5762, 413.9326);
  lineto (state, 284.3765, 406.5493);
  lineto (state, 284.7471, 391.6973);
  lineto (state, 275.8027, 404.2261);
  lineto (state, 273.8164, 414.5488);
  lineto (state, 252.376, 431.5088);
  lineto (state, 257.4292, 460.0386);
  lineto (state, 269.9766, 491.3496);
  lineto (state, 240.8564, 433.4292);
  lineto (state, 244.603, 410.4658);
  lineto (state, 256.2168, 392.4688);
  lineto (state, 270.3921, 380.3086);
  lineto (state, 268.1118, 375.1055);
  closepath (state);

  setcolor (state, state->flame1_bg_pixel);
  fill (state);

  setcolor (state, state->flame1_fg_pixel);
  stroke (state);
}


static void
flames_7b (logo_state *state)
{
  setlinewidth (state, 0.8);

  newpath (state);
  moveto (state,  262.0464, 374.7109);
  curveto (state, 270.0464, 366.9424, 279.3799, 356.7822, 302.0469, 357.9785);
  curveto (state, 324.7129, 359.1719, 338.7129, 377.6992, 340.7129, 387.2607);
  curveto (state, 342.7129, 396.8228, 336.7129, 423.1182, 326.0459, 433.2773);
  curveto (state, 315.3799, 443.4365, 303.3789, 472.7197, 312.7129, 487.0625);
  curveto (state, 322.0469, 501.4053, 282.7129, 457.1812, 298.7129, 432.6797);
  curveto (state, 314.7129, 408.1777, 309.0547, 409.6147, 305.7129, 412.3608);
  curveto (state, 301.7129, 415.6475, 304.1094, 420.0757, 292.3799, 431.4844);
  curveto (state, 276.7129, 446.7236, 274.3135, 450.7798, 291.3799, 478.0981);
  curveto (state, 296.0464, 485.5684, 271.333, 508.5752, 271.333, 508.5752);
  curveto (state, 271.333, 508.5752, 288.0469, 479.2935, 273.3799, 465.5483);
  curveto (state, 258.7129, 451.8032, 259.5073, 440.4727, 260.7129, 435.0703);
  curveto (state, 262.0464, 429.0938, 269.3799, 417.1416, 274.0464, 408.1777);
  curveto (state, 278.7129, 399.2134, 279.0986, 385.3438, 271.3799, 399.811);
  curveto (state, 267.7129, 406.6831, 245.3794, 422.52, 253.7129, 447.9185);
  curveto (state, 263.7129, 478.397, 233.3799, 424.313, 241.3799, 405.1895);
  curveto (state, 249.3799, 386.0645, 262.0464, 374.7109, 262.0464, 374.7109);
  closepath (state);

  setcolor (state, state->flame2_bg_pixel);
  fill (state);

  setcolor (state, state->flame2_fg_pixel);
  stroke (state);
}


static unsigned long
alloccolor (Display *dpy, Colormap cmap, char *s)
{
  XColor color;
  if (!XParseColor (dpy, cmap, s, &color))
    {
      fprintf (stderr, "%s: can't parse color %s\n", progname, s);
      return -1;
    }
  if (! XAllocColor (dpy, cmap, &color))
    {
      fprintf (stderr, "%s: couldn't allocate color %s\n", progname, s);
      return -1;
    }
  return color.pixel;
}


/* Draws the logo centered in the given Drawable (presumably a Pixmap.)
   next_frame_p means randomize the flame shape.
 */
void
xscreensaver_logo (Display *dpy, Drawable dest, Colormap cmap,
                   Bool next_frame_p)
{
  XGCValues gcv;
  logo_state S;
  logo_state *state = &S;
  int mono_p;

  unsigned int w, h, depth;
  unsigned long bg;

  state->dpy = dpy;
  state->drawable = dest;
  state->gc = XCreateGC (dpy, dest, 0, &gcv);

  {
    Window root;
    int x, y;
    unsigned int bw;
    XGetGeometry (dpy, dest, &root, &x, &y, &w, &h, &bw, &depth);
    mono_p = (depth == 1);
    state->y_origin = h;
  }

  if (mono_p)
    {
      unsigned long white = 1;
      unsigned long black = 0;
      bg = black;
      state->logo_bg_pixel    = white;
      state->logo_fg_pixel    = black;
      state->monitor_bg_pixel = white;
      state->monitor_fg_pixel = black;
      state->flame1_bg_pixel  = white;
      state->flame1_fg_pixel  = black;
      state->flame2_bg_pixel  = white;
      state->flame2_fg_pixel  = black;
    }
  else
    {
      bg = get_pixel_resource ("background", "Background", dpy, cmap);
      state->logo_bg_pixel    = alloccolor (dpy, cmap, "#FFFFFF");
      state->logo_fg_pixel    = alloccolor (dpy, cmap, "#000000");
/*      state->monitor_bg_pixel = alloccolor (dpy, cmap, "#00AA00");*/
      state->monitor_bg_pixel = alloccolor (dpy, cmap, "#FFFFFF");
      state->monitor_fg_pixel = alloccolor (dpy, cmap, "#000000");
      state->flame1_bg_pixel  = alloccolor (dpy, cmap, "#FFA500");
      state->flame1_fg_pixel  = alloccolor (dpy, cmap, "#000000");
      state->flame2_bg_pixel  = alloccolor (dpy, cmap, "#FF0000");
      state->flame2_fg_pixel  = alloccolor (dpy, cmap, "#000000");
    }

  setcolor (state, bg);
  XFillRectangle (dpy, dest, state->gc, 0, 0, w, h);

  reset (state);
  scale (state, w / 220.0, w / 220.0);
  translate (state, -193, -315);

  border (state);
  monitor (state);

  if (!next_frame_p)
    {
      flames_0a (state);
      flames_0b (state);
    }
  else
    {
      static int tick = 0;
      static int tick2 = 0;
      if (++tick2 > 3) tick2 = 0;
      if (tick2 == 0)
        if (++tick > 7) tick = 0;
      switch (tick) {
      case 0: flames_0a (state); flames_0b (state); break;
      case 1: flames_1a (state); flames_1b (state); break;
      case 2: flames_2a (state); flames_2b (state); break;
      case 3: flames_3a (state); flames_3b (state); break;
      case 4: flames_4a (state); flames_4b (state); break;
      case 5: flames_5a (state); flames_5b (state); break;
      case 6: flames_6a (state); flames_6b (state); flames_6c (state); break;
      case 7: flames_7a (state); flames_7b (state); break;
      default: abort(); break;
      }
    }

  XFreeGC (dpy, state->gc);
}
