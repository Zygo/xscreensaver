/* quaternion, Copyright (c) 2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Utilities for quaternions.
 */

#ifndef __QUATERNION_H__
#define __QUATERNION_H__

typedef struct { double x, y, z; } vec3;
typedef struct { double x, y, z, w; } quat;

extern double vec3_length (const vec3);
extern vec3 vec3_scale (const vec3, double);
extern vec3 vec3_normalize (const vec3);
extern vec3 vec3_add (const vec3, const vec3);
extern vec3 vec3_sub (const vec3, const vec3);
extern vec3 vec3_cross (const vec3, const vec3);
extern double vec3_dot (const vec3, const vec3);

extern quat quat_normalize (const quat);
extern quat quat_add (const quat, const quat);
extern quat quat_mult (const quat, const quat);
extern quat quat_conjugate (const quat);
extern double quat_dot (const quat, const quat);
extern double quat_angle (const quat, const quat);
extern vec3 quat_euler_angles (const quat);
extern quat axis_to_quat (const vec3, double phi);
extern void quat_to_rotmatrix (const quat, double m[16]);
extern quat quat_slerp (double t, const quat, const quat);

#endif /* __QUATERNION_H__ */
