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

#ifndef _SPLINE_H_
#define _SPLINE_H_

#ifdef VMS
# ifndef __DECC
   typedef unsigned int u_int;
# else
#  if __DECC_VER < 50200000
    typedef unsigned int u_int;
#  endif
# endif
#endif

typedef struct _spline
{
  /* input */
  u_int		n_controls;
  double*	control_x;
  double*	control_y;

  /* output */
  u_int		n_points;
  XPoint*	points;
  u_int		allocated_points;
} spline;

spline* make_spline (u_int size);
void compute_spline (spline* s);
void compute_closed_spline (spline* s);
void just_fill_spline (spline* s);
void append_spline_points (spline* s1, spline* s2);
void spline_bounding_box (spline* s, XRectangle* rectangle_out);

#endif /* _SPLINE_H_ */
