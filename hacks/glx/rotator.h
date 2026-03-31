/* xscreensaver, Copyright (c) 1998-2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __ROTATOR_H__
#define __ROTATOR_H__

typedef struct rotator rotator;

/* Returns a rotator object, which encapsulates rotation and motion state.

   spin_[xyz]_speed indicates the relative speed of rotation.
   Specify 0 if you don't want any rotation around that axis.

   spin_accel specifies a scaling factor for the acceleration that is
   randomly applied to spin: if you want the speed to change faster,
   make this > 1.

   wander_speed indicates the relative speed through space.

   If randomize_initial_state_p is true, then the initial position and
   rotation will be randomized (even if the spin speeds are 0.)  If it
   is false, then all values will be initially zeroed.
 */
extern rotator *make_rotator (double spin_x_speed,
                              double spin_y_speed,
                              double spin_z_speed,
                              double spin_accel,
                              double wander_speed,
                              int randomize_initial_state_p);

/* Rotates one step, and returns the new rotation values.
   x, y, and z range from 0.0-1.0, the fraction through the circle
   (*not* radians or degrees!)
   If `update_p' is non-zero, then (maybe) rotate first.
 */
extern void get_rotation (rotator *rot,
                          double *x_ret, double *y_ret, double *z_ret,
                          int update_p);

/* Moves one step, and returns the new position values.
   x, y, and z range from 0.0-1.0, the fraction through space:
   scale those values as needed.
   If `update_p' is non-zero, then (maybe) move first.
 */
extern void get_position (rotator *rot,
                          double *x_ret, double *y_ret, double *z_ret,
                          int update_p);

/* Destroys and frees a `rotator' object. */
extern void free_rotator (rotator *r);

#endif /* __ROTATOR_H__ */
