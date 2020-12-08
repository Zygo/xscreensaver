/* gltrackball, Copyright (c) 2002-2017 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_COCOA
# include "jwxyz.h"
#elif defined(HAVE_ANDROID)
# include "jwxyz.h"
# include <GLES/gl.h>
#else  /* real X11 */
# include <X11/X.h>
# include <X11/Xlib.h>
# include <GL/gl.h>
#endif /* !HAVE_COCOA */

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

# define Button4 4  /* WTF */
# define Button5 5
# define Button6 6
# define Button7 7

#include "trackball.h"
#include "gltrackball.h"

#if defined(HAVE_IPHONE) || defined(HAVE_ANDROID)
  /* Surely this should be defined somewhere more centrally... */
# define HAVE_MOBILE
#endif

/* Bah, copied from ../fps.h */
#ifdef HAVE_MOBILE
  extern double current_device_rotation (void);
#else
# define current_device_rotation() (0)
#endif


struct trackball_state {
  int ow, oh;
  double x, y;
  double dx, dy, ddx, ddy;
  GLfloat q[4];
  int button_down_p;
  int ignore_device_rotation_p;
};

/* Returns a trackball_state object, which encapsulates the stuff necessary
   to make dragging the mouse on the window of a GL program do the right thing.
 */
trackball_state *
gltrackball_init (int ignore_device_rotation_p)
{
  trackball_state *ts = (trackball_state *) calloc (1, sizeof (*ts));
  if (!ts) return 0;
  ts->ignore_device_rotation_p = ignore_device_rotation_p;
  trackball (ts->q, 0, 0, 0, 0);
  return ts;
}

void
gltrackball_free (trackball_state *ts)
{
  free (ts);
}


/* Device rotation interacts very strangely with mouse positions.
   I'm not entirely sure this is the right fix.
 */
static void
adjust_for_device_rotation (trackball_state *ts,
                            double *x, double *y, double *w, double *h)
{
  int rot = (int) current_device_rotation();
  int swap;

  if (ts->ignore_device_rotation_p) return;

  while (rot <= -180) rot += 360;
  while (rot >   180) rot -= 360;

  if (rot > 135 || rot < -135)		/* 180 */
    {
      *x = *w - *x;
      *y = *h - *y;
    }
  else if (rot > 45)			/* 90 */
    {
      swap = *x; *x = *y; *y = swap;
      swap = *w; *w = *h; *h = swap;
      *x = *w - *x;
    }
  else if (rot < -45)			/* 270 */
    {
      swap = *x; *x = *y; *y = swap;
      swap = *w; *w = *h; *h = swap;
      *y = *h - *y;
    }
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
  ts->button_down_p = 1;
  ts->dx = ts->ddx = 0;
  ts->dy = ts->ddy = 0;
}

/* Stop tracking the mouse: Call this when the mouse button goes up.
 */
void
gltrackball_stop (trackball_state *ts)
{
  ts->button_down_p = 0;
}

static void
gltrackball_track_1 (trackball_state *ts,
                     double x, double y,
                     int w, int h,
                     int ignore_device_rotation_p)
{
  double X = x;
  double Y = y;
  double W = w, W2 = w;
  double H = h, H2 = h;
  float q2[4];
  double ox = ts->x;
  double oy = ts->y;

  ts->x = x;
  ts->y = y;

  if (! ignore_device_rotation_p)
    {
      adjust_for_device_rotation (ts, &ox, &oy, &W,  &H);
      adjust_for_device_rotation (ts, &X,  &Y,  &W2, &H2);
    }
  trackball (q2,
             (2 * ox - W) / W,
             (H - 2 * oy) / H,
             (2 * X - W)  / W,
             (H - 2 * Y)  / H);

  add_quats (q2, ts->q, ts->q);
}


/* Track the mouse: Call this each time the mouse moves with the button down.
   x and y are the new mouse position relative to the window.
   w and h are the size of the window.
 */
void
gltrackball_track (trackball_state *ts, int x, int y, int w, int h)
{
  double dampen = 0.01;  /* This keeps it going for about 3 sec */
  ts->dx = x - ts->x;
  ts->dy = y - ts->y;
  ts->ddx = ts->dx * dampen;
  ts->ddy = ts->dy * dampen;
  ts->ow = w;
  ts->oh = h;
  gltrackball_track_1 (ts, x, y, w, h, False);
}


static void
gltrackball_dampen (double *n, double *dn)
{
  int pos = (*n > 0);
  *n -= *dn;
  if (pos != (*n > 0))
    *n = *dn = 0;
}


/* Reset the trackball to the default unrotated state,
   plus an optional initial rotation.
 */
void
gltrackball_reset (trackball_state *ts, float x, float y)
{
  int bd = ts->button_down_p;
  int ig = ts->ignore_device_rotation_p;
  memset (ts, 0, sizeof(*ts));
  ts->button_down_p = bd;
  ts->ignore_device_rotation_p = ig;
  trackball (ts->q, 0, 0, x, y);
}


/* Execute the rotations current encapsulated in the trackball_state:
   this does something analogous to glRotatef().
 */
void
gltrackball_rotate (trackball_state *ts)
{
  GLfloat m[4][4];
  if (!ts->button_down_p &&
      (ts->ddx != 0 ||
       ts->ddy != 0))
    {
      /* Apply inertia: keep moving in the same direction as the last move. */
      gltrackball_track_1 (ts, 
                           ts->x + ts->dx,
                           ts->y + ts->dy,
                           ts->ow, ts->oh,
                           False);

      /* Dampen inertia: gradually stop spinning. */
      gltrackball_dampen (&ts->dx, &ts->ddx);
      gltrackball_dampen (&ts->dy, &ts->ddy);
    }

  build_rotmatrix (m, ts->q);
  glMultMatrixf (&m[0][0]);
}


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
  int horizontal_p;
  int mx, my, move, scale;

#ifdef HAVE_JWXYZ
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

  scale = mx = my = 1000;
  move = (up_p
          ? floor (scale * (1.0 - (percent / 100.0)))
          : ceil  (scale * (1.0 + (percent / 100.0))));
  if (horizontal_p) mx = move;
  else              my = move;
  gltrackball_start (ts, scale, scale, scale*2, scale*2);
  gltrackball_track (ts, mx, my, scale*2, scale*2);
}

void
gltrackball_get_quaternion (trackball_state *ts, float q[4])
{
  int i;
  for (i=0; i<4; i++)
    q[i] = ts->q[i];
}


/* A utility function for event-handler functions:
   Handles the various motion and click events related to trackballs.
   Returns True if the event was handled.
 */
Bool
gltrackball_event_handler (XEvent *event,
                           trackball_state *ts,
                           int window_width, int window_height,
                           Bool *button_down_p)
{
  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      *button_down_p = True;
      gltrackball_start (ts,
                         event->xbutton.x, event->xbutton.y,
                         window_width, window_height);
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      *button_down_p = False;
      gltrackball_stop (ts);
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (ts, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           *button_down_p)
    {
      gltrackball_track (ts,
                         event->xmotion.x, event->xmotion.y,
                         window_width, window_height);
      return True;
    }

  return False;
}
