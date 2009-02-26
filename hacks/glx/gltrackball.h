/* gltrackball, Copyright (c) 2002-2008 Jamie Zawinski <jwz@jwz.org>
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

#ifndef __GLTRACKBALL_H__
#define __GLTRACKBALL_H__

typedef struct trackball_state trackball_state;

/* Returns a trackball_state object, which encapsulates the stuff necessary
   to make dragging the mouse on the window of a GL program do the right thing.
 */
extern trackball_state *gltrackball_init (void);

/* Begin tracking the mouse: Call this when the mouse button goes down.
   x and y are the mouse position relative to the window.
   w and h are the size of the window.
 */
extern void gltrackball_start (trackball_state *, int x, int y, int w, int h);

/* Track the mouse: Call this each time the mouse moves with the button down.
   x and y are the new mouse position relative to the window.
   w and h are the size of the window.
 */
extern void gltrackball_track (trackball_state *, int x, int y, int w, int h);

/* Execute the rotations current encapsulated in the trackball_state:
   this does something analagous to glRotatef().
 */
extern void gltrackball_rotate (trackball_state *);

/* Call this when a mouse-wheel click is detected.
   Clicks act like horizontal or vertical drags.
   Percent is the length of the drag as a percentage of the screen size.
   Button is 'Button4' or 'Button5' (for the vertical wheel)
   or 'Button5' or 'Button6' (for the horizontal wheel).
   If `flip_p' is true, swap the horizontal and vertical axes.
 */
void gltrackball_mousewheel (trackball_state *ts,
                             int button, int percent, int flip_p);

/* Return the quaternion encapsulated by the trackball state.
 */
extern void gltrackball_get_quaternion (trackball_state *ts, float q[4]);

/* Reset the trackball to the default unrotated state.
 */
extern void gltrackball_reset (trackball_state *ts);

#endif /* __GLTRACKBALL_H__ */

