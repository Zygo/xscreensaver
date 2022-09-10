/* sphere, Copyright (c) 2001-2014 Jamie Zawinski <jwz@jwz.org>
 * Utility function to create a unit sphere in GL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __SPHERE_H__
#define __SPHERE_H__

/* Creates a diameter 1 sphere at 0, 0, 0.
   stacks = number of north/south divisions (latitude)
   slices = number of clockwise/counterclockwise divisions (longitude)
   Returns number of polygons used.
 */
extern int unit_sphere (int stacks, int slices, int wire_p);
extern int unit_dome (int stacks, int slices, int wire_p);

#endif /* __SPHERE_H__ */
