/* gltrackball, Copyright (c) 2002-2012 Jamie Zawinski <jwz@jwz.org>
 * GL-flavored wrapper for trackball.c
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_COCOA
# include <GL/gl.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "trackball.h"
#include "gltrackball.h"

extern double current_device_rotation (void);  /* Bah, it's in fps.h */

struct trackball_state {
  int x, y;
  GLfloat q[4];
};

/* Returns a trackball_state object, which encapsulates the stuff necessary
   to make dragging the mouse on the window of a GL program do the right thing.
 */
trackball_state *
gltrackball_init (void)
{
  trackball_state *ts = (trackball_state *) calloc (1, sizeof (*ts));
  if (!ts) return 0;
  trackball (ts->q, 0, 0, 0, 0);
  return ts;
}

/* Reset the trackball to the default unrotated state.
 */
void
gltrackball_reset (trackball_state *ts)
{
  memset (ts, 0, sizeof(*ts));
  trackball (ts->q, 0, 0, 0, 0);
}


/* Begin tracking the mouse: Call this when the mouse button goes down.
   x and y are the mouse position relative to the window.
   w and h are the size of the window.
 */
void
gltrackball_start (trackball_state *ts, int x, int y, int w, int h)
{
  ts->x = x;
  ts->y = y;
}

/* Track the mouse: Call this each time the mouse moves with the button down.
   x and y are the new mouse position relative to the window.
   w and h are the size of the window.
 */
void
gltrackball_track (trackball_state *ts, int x, int y, int w, int h)
{
  float q2[4];
  trackball (q2,
             (2.0 * ts->x - w) / w,
             (h - 2.0 * ts->y) / h,
             (2.0 * x - w) / w,
             (h - 2.0 * y) / h);
  ts->x = x;
  ts->y = y;
  add_quats (q2, ts->q, ts->q);
}

/* Execute the rotations current encapsulated in the trackball_state:
   this does something analagous to glRotatef().
 */
void
gltrackball_rotate (trackball_state *ts)
{
  GLfloat m[4][4];
  build_rotmatrix (m, ts->q);
  glMultMatrixf (&m[0][0]);
}


# define Button4 4  /* X11/Xlib.h */
# define Button5 5
# define Button6 6
# define Button7 7

/* Call this when a mouse-wheel click is detected.
   Clicks act like horizontal or vertical drags.
   Percent is the length of the drag as a percentage of the screen size.
   Button is 'Button4' or 'Button5' (for the vertical wheel)
   or 'Button5' or 'Button6' (for the horizontal wheel).
   If `flip_p' is true, swap the horizontal and vertical axes.
 */
void
gltrackball_mousewheel (trackball_state *ts,
                        int button, int percent, int flip_p)
{
  int up_p;
  double move;
  int horizontal_p;

#ifdef HAVE_COCOA
  flip_p = 0;      /* MacOS has already handled this. */
#endif

  switch (button) {
  case Button4: up_p = 1; horizontal_p = 0; break;
  case Button5: up_p = 0; horizontal_p = 0; break;
  case Button6: up_p = 1; horizontal_p = 1; break;
  case Button7: up_p = 0; horizontal_p = 1; break;
  default: abort(); break;
  }

  if (flip_p)
    {
      horizontal_p = !horizontal_p;
      up_p = !up_p;
    }

  move = (up_p
          ? 1.0 - (percent / 100.0)
          : 1.0 + (percent / 100.0));

  gltrackball_start (ts, 50, 50, 100, 100);
  if (horizontal_p)
    gltrackball_track (ts, 50*move, 50, 100, 100);
  else
    gltrackball_track (ts, 50, 50*move, 100, 100);
}

void
gltrackball_get_quaternion (trackball_state *ts, float q[4])
{
  int i;
  for (i=0; i<4; i++)
    q[i] = ts->q[i];
}
