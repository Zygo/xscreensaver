#ifndef _SPLINE_H_
#define _SPLINE_H_

#include <X11/Xlib.h>

#if __STDC__
# define P(x)x
#else
# define P(x)()
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

spline* make_spline P((u_int size));
void compute_spline P((spline* s));
void compute_closed_spline P((spline* s));
void just_fill_spline P((spline* s));
void append_spline_points P((spline* s1, spline* s2));
void spline_bounding_box P((spline* s, XRectangle* rectangle_out));

#undef P
#endif /* _SPLINE_H_ */
