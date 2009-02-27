/*
 * Copyright (c) 1987, 1988, 1989 Stanford University
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Stanford not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Stanford makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STANFORD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* This code came with the InterViews distribution, and was translated
   from C++ to C by Matthieu Devin <devin@lucid.com> some time in 1992.
 */

#include "utils.h"
#include "spline.h"

#define SMOOTHNESS 1.0

static void no_more_memory (void);
static void grow_spline_points (spline* s);
static void mid_point (double x0, double y0, double x1, double y1,
		       double *mx, double *my);
static int can_approx_with_line (double x0, double y0, double x2,
				 double y2, double x3, double y3);
static void add_line (spline* s, double x0, double y0, double x1, double y1);
static void add_bezier_arc (spline* s,
			    double x0, double y0, double x1, double y1,
			    double x2, double y2, double x3, double y3);
static void third_point (double x0, double y0, double x1, double y1,
			 double *tx, double *ty);
static void calc_section (spline* s, double cminus1x, double cminus1y,
			  double cx, double cy, double cplus1x, double cplus1y,
			  double cplus2x, double cplus2y);

static void
no_more_memory (void)
{
  fprintf (stderr, "No more memory\n");
  exit (1);
}

spline*
make_spline (u_int size)
{
  spline* s = (spline*)calloc (1, sizeof (spline));
  if (!s)
    no_more_memory ();
  s->n_controls = size;
  s->control_x = (double*)calloc (s->n_controls, sizeof (double));
  s->control_y = (double*)calloc (s->n_controls, sizeof (double));

  s->n_points = 0;
  s->allocated_points = s->n_controls;
  s->points = (XPoint*)calloc (s->allocated_points, sizeof (XPoint));

  if (!s->control_x || !s->control_y || !s->points)
    no_more_memory ();

  return s;
}

static void
grow_spline_points (spline *s)
{
  s->allocated_points *= 2;
  s->points =
    (XPoint*)realloc (s->points, s->allocated_points * sizeof (XPoint));

  if (!s->points)
    no_more_memory ();
}

static void 
mid_point (double x0, double y0,
	   double x1, double y1,
	   double *mx, double *my)
{
  *mx = (x0 + x1) / 2.0;
  *my = (y0 + y1) / 2.0;
}

static void 
third_point (double x0, double y0,
	     double x1, double y1,
	     double *tx, double *ty)
{
  *tx = (2 * x0 + x1) / 3.0;
  *ty = (2 * y0 + y1) / 3.0;
}

static int
can_approx_with_line (double x0, double y0,
		      double x2, double y2,
		      double x3, double y3)
{
  double triangle_area, side_squared, dx, dy;
  
  triangle_area = x0 * y2 - x2 * y0 + x2 * y3 - x3 * y2 + x3 * y0 - x0 * y3;
  /* actually 4 times the area. */
  triangle_area *= triangle_area;
  dx = x3 - x0;
  dy = y3 - y0;
  side_squared = dx * dx + dy * dy;
  return triangle_area <= SMOOTHNESS * side_squared;
}

static void
add_line (spline *s,
	  double x0, double y0,
	  double x1, double y1)
{
  if (s->n_points >= s->allocated_points)
    grow_spline_points (s);

  if (s->n_points == 0)
    {
      s->points [s->n_points].x = x0;
      s->points [s->n_points].y = y0;
      s->n_points += 1;
    }
  s->points [s->n_points].x = x1;
  s->points [s->n_points].y = y1;
  s->n_points += 1;
}

static void 
add_bezier_arc (spline *s,
		double x0, double y0,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
  double midx01, midx12, midx23, midlsegx, midrsegx, cx,
  midy01, midy12, midy23, midlsegy, midrsegy, cy;
    
  mid_point (x0, y0, x1, y1, &midx01, &midy01);
  mid_point (x1, y1, x2, y2, &midx12, &midy12);
  mid_point (x2, y2, x3, y3, &midx23, &midy23);
  mid_point (midx01, midy01, midx12, midy12, &midlsegx, &midlsegy);
  mid_point (midx12, midy12, midx23, midy23, &midrsegx, &midrsegy);
  mid_point (midlsegx, midlsegy, midrsegx, midrsegy, &cx, &cy);    

  if (can_approx_with_line (x0, y0, midlsegx, midlsegy, cx, cy))
    add_line (s, x0, y0, cx, cy);
  else if ((midx01 != x1) || (midy01 != y1) || (midlsegx != x2)
	   || (midlsegy != y2) || (cx != x3) || (cy != y3))
    add_bezier_arc (s, x0, y0, midx01, midy01, midlsegx, midlsegy, cx, cy);

  if (can_approx_with_line (cx, cy, midx23, midy23, x3, y3))
    add_line (s, cx, cy, x3, y3);
  else if ((cx != x0) || (cy != y0) || (midrsegx != x1) || (midrsegy != y1)
	   || (midx23 != x2) || (midy23 != y2))  
    add_bezier_arc (s, cx, cy, midrsegx, midrsegy, midx23, midy23, x3, y3);
}

static void
calc_section (spline *s,
	      double cminus1x, double cminus1y,
	      double cx, double cy,
	      double cplus1x, double cplus1y,
	      double cplus2x, double cplus2y)
{
  double p0x, p1x, p2x, p3x, tempx,
  p0y, p1y, p2y, p3y, tempy;
  
  third_point (cx, cy, cplus1x, cplus1y, &p1x, &p1y);
  third_point (cplus1x, cplus1y, cx, cy, &p2x, &p2y);
  third_point (cx, cy, cminus1x, cminus1y, &tempx, &tempy);
  mid_point (tempx, tempy, p1x, p1y, &p0x, &p0y);
  third_point (cplus1x, cplus1y, cplus2x, cplus2y, &tempx, &tempy);
  mid_point (tempx, tempy, p2x, p2y, &p3x, &p3y);
  add_bezier_arc (s, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y);
}

void
compute_spline (spline *s)
{
  int i;
  s->n_points = 0;
  
  if (s->n_controls < 3)
    return;

  calc_section (s, s->control_x [0], s->control_y [0], s->control_x [0],
		s->control_y [0], s->control_x [0], s->control_y [0],
		s->control_x [1], s->control_y [1]);
  calc_section (s, s->control_x [0], s->control_y [0], s->control_x [0],
		s->control_y [0], s->control_x [1], s->control_y [1],
		s->control_x [2], s->control_y [2]);

  for (i = 1; i < s->n_controls - 2; i++)
    calc_section (s, s->control_x [i - 1], s->control_y [i - 1],
		  s->control_x [i], s->control_y [i],
		  s->control_x [i + 1], s->control_y [i + 1],
		  s->control_x [i + 2], s->control_y [i + 2]);
  
  calc_section (s, s->control_x [i - 1], s->control_y [i - 1],
		s->control_x [i], s->control_y [i],
		s->control_x [i + 1], s->control_y [i + 1],
		s->control_x [i + 1], s->control_y [i + 1]);
  calc_section (s, s->control_x [i], s->control_y [i],
		s->control_x [i + 1], s->control_y [i + 1],
		s->control_x [i + 1], s->control_y [i + 1],
		s->control_x [i + 1], s->control_y [i + 1]);
}

void 
compute_closed_spline (spline *s)
{
  int i;
  s->n_points = 0;

  if (s->n_controls < 3) 
    return;

  calc_section (s,
		s->control_x [s->n_controls - 1],
		s->control_y [s->n_controls - 1],
		s->control_x [0], s->control_y [0],
		s->control_x [1], s->control_y [1],
		s->control_x [2], s->control_y [2]);

  for (i = 1; i < s->n_controls - 2; i++)
    calc_section (s, s->control_x [i - 1], s->control_y [i - 1],
		  s->control_x [i], s->control_y [i],
		  s->control_x [i + 1], s->control_y [i + 1],
		  s->control_x [i + 2], s->control_y [i + 2]);
      
  calc_section (s, s->control_x [i - 1], s->control_y [i - 1],
		s->control_x [i], s->control_y [i],
		s->control_x [i + 1], s->control_y [i + 1],
		s->control_x [0], s->control_y [0]);
  calc_section (s, s->control_x [i], s->control_y [i],
		s->control_x [i + 1], s->control_y [i + 1],
		s->control_x [0], s->control_y [0],
		s->control_x [1], s->control_y [1]);
}

void
just_fill_spline (spline *s)
{
  int i;

  while (s->allocated_points < s->n_controls + 1)
    grow_spline_points (s);
    
  for (i = 0; i < s->n_controls; i++)
    {
      s->points [i].x = s->control_x [i];
      s->points [i].y = s->control_y [i];
    }
  s->points [s->n_controls].x = s->control_x [0];
  s->points [s->n_controls].y = s->control_y [0];
  s->n_points = s->n_controls + 1;
}

void
append_spline_points (spline *s1, spline *s2)
{
  int i;
  while (s1->allocated_points < s1->n_points + s2->n_points)
    grow_spline_points (s1);
  for (i = s1->n_points; i < s1->n_points + s2->n_points; i++)
    {
      s1->points [i].x = s2->points [i - s1->n_points].x;
      s1->points [i].y = s2->points [i - s1->n_points].y;
    }
  s1->n_points = s1->n_points + s2->n_points;
}

void
spline_bounding_box (spline *s, XRectangle *rectangle_out)
{
  int min_x;
  int max_x;
  int min_y;
  int max_y;
  int i;

  if (s->n_points == 0)
    {
      rectangle_out->x = 0;
      rectangle_out->y = 0;
      rectangle_out->width = 0;
      rectangle_out->height = 0;
    }

  min_x = s->points [0].x;
  max_x = min_x;
  min_y = s->points [0].y;
  max_y = min_y;

  for (i = 1; i < s->n_points; i++)
    {
      if (s->points [i].x < min_x)
	min_x = s->points [i].x;
      if (s->points [i].x > max_x)
	max_x = s->points [i].x;
      if (s->points [i].y < min_y)
	min_y = s->points [i].y;
      if (s->points [i].y > max_y)
	max_y = s->points [i].y;
    }
  rectangle_out->x = min_x;
  rectangle_out->y = min_y;
  rectangle_out->width = max_x - min_x;
  rectangle_out->height = max_y - min_y;
}
