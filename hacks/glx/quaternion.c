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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <math.h>
#include "quaternion.h"

#ifndef  FLT_EPSILON
# define FLT_EPSILON 1e-15f
#endif



double
vec3_length (const vec3 v)
{
  return sqrt (v.x*v.x + v.y*v.y + v.z*v.z);
}

vec3
vec3_scale (const vec3 v, double s)
{
  vec3 o;
  o.x = v.x * s;
  o.y = v.y * s;
  o.z = v.z * s;
  return o;
}

vec3
vec3_normalize (const vec3 v)
{
  return vec3_scale (v, 1 / vec3_length (v));
}

vec3
vec3_add (const vec3 v1, const vec3 v2)
{
  vec3 o;
  o.x = v1.x + v2.x;
  o.y = v1.y + v2.y;
  o.z = v1.z + v2.z;
  return o;
}

vec3
vec3_sub (const vec3 v1, const vec3 v2)
{
  vec3 o;
  o.x = v1.x - v2.x;
  o.y = v1.y - v2.y;
  o.z = v1.z - v2.z;
  return o;
}

vec3
vec3_cross (const vec3 v1, const vec3 v2)
{
  vec3 o;
  o.x = (v1.y * v2.z) - (v1.z * v2.y);
  o.y = (v1.z * v2.x) - (v1.x * v2.z);
  o.z = (v1.x * v2.y) - (v1.y * v2.x);
  return o;
}

double
vec3_dot (const vec3 v1, const vec3 v2)
{
  return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}


quat
quat_normalize (const quat q)
{
  quat o;
  double s = sqrt (q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
  o.x = q.x / s;
  o.y = q.y / s;
  o.z = q.z / s;
  o.w = q.w / s;
  return o;
}

quat
quat_add (const quat q1, const quat q2)
{
  quat o;
  o.x = q1.x + q2.x;
  o.y = q1.y + q2.y;
  o.z = q1.z + q2.z;
  o.w = q1.w + q2.w;
  return o;
}


quat
quat_mult (const quat q1, const quat q2)
{
  quat out;
# if 1
  out.x = q1.w * q2.x + q1.x * q2.w - q1.y * q2.z - q1.z * q2.y;
  out.y = q1.w * q2.y + q1.x * q2.z + q1.y * q2.w - q1.z * q2.x;
  out.z = q1.w * q2.z - q1.x * q2.y + q1.y * q2.x - q1.z * q2.w;
  out.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
# else
  vec3 t1, t2, t3;

  t1.x = q1.x; t1.y = q1.y; t1.z = q1.z;  /* vector part of quats */
  t2.x = q2.x; t2.y = q2.y; t2.z = q2.z;

  t3 = vec3_add (vec3_cross (t2, t1),
                 vec3_add (vec3_scale (t1, q2.w),
                           vec3_scale (t2, q1.w)));
  out.x = t3.x;
  out.y = t3.y;
  out.z = t3.z;
  out.w = q1.w * q2.w - vec3_dot (t1, t2);
# endif
  return out;
}


quat
quat_conjugate (const quat q)
{
  quat out;
  out.x = -q.x;
  out.y = -q.y;
  out.z = -q.z;
  out.w =  q.w;
  return out;
}


double
quat_dot (const quat q, const quat r)
{
  return (q.x * r.x + q.y * r.y + q.z * r.z + q.w * r.w);
}


double
quat_angle (const quat a, const quat b)
{
  return 2 * acos (quat_dot (quat_normalize (a), quat_normalize (b)));
}


vec3
quat_euler_angles (const quat q)
{
  vec3 o;
  o.x = atan2 (    2 * ((q.w * q.x) + (q.y * q.z)),
               1 - 2 * ((q.x * q.x) + (q.y * q.y)));
  o.y = -M_PI/2 + 2 * atan2 (sqrt (1 + 2 * (q.w * q.y - q.x * q.z)),
                             sqrt (1 - 2 * (q.w * q.y - q.x * q.z)));
  o.z = atan2 (    2 * (q.w * q.z + q.x * q.y),
               1 - 2 * (q.y * q.y + q.z * q.z));
  return o;
}


quat
axis_to_quat (const vec3 a, double phi)
{
  quat q;
  vec3 b = vec3_scale (vec3_normalize (a), sin (phi/2));
  q.x = b.x;
  q.y = b.y;
  q.z = b.z;
  q.w = cos (phi/2);
  return q;
}

void
quat_to_rotmatrix (const quat q, double m[16])
{
  m[0] = 1 - 2 * (q.y * q.y + q.z * q.z);
  m[1] = 2 * (q.x * q.y - q.z * q.w);
  m[2] = 2 * (q.z * q.x + q.y * q.w);
  m[3] = 0;

  m[4] = 2 * (q.x * q.y + q.z * q.w);
  m[5] = 1 - 2 * (q.z * q.z + q.x * q.x);
  m[6] = 2 * (q.y * q.z - q.x * q.w);
  m[7] = 0;

  m[8]  = 2 * (q.z * q.x - q.y * q.w);
  m[9]  = 2 * (q.y * q.z + q.x * q.w);
  m[10] = 1 - 2 * (q.y * q.y + q.x * q.x);
  m[11] = 0;

  m[12] = 0;
  m[13] = 0;
  m[14] = 0;
  m[15] = 1;
}


/* Spherical linear interpolation */
quat
quat_slerp (double t, const quat a, const quat b)
{
#if 1
  quat out;
  double alpha, beta;
  double cos_t = (a.x * b.x +
                  a.y * b.y +
                  a.z * b.z +
                  a.w * b.w);

  if (1 - cos_t < FLT_EPSILON)
    {
      alpha = t;
      beta  = 1-t;
    }
  else
    {
      double theta = acos (cos_t);
      double sin_t = sin (theta);
      alpha = sin (t * theta) / sin_t;
      beta  = sin (theta - t * theta) / sin_t;
    }
  out.x = beta * a.x + alpha * b.x;
  out.y = beta * a.y + alpha * b.y;
  out.z = beta * a.z + alpha * b.z;
  out.w = beta * a.w + alpha * b.w;
  return out;

#elif 1

  /* ##### this one seems to behave the same as the above */

  quat out;
  double alpha, beta;
  double cos_t = (a.x * b.x +
                  a.y * b.y +
                  a.z * b.z +
                  a.w * b.w);
  quat bb = b;

  if (cos_t < 0)		/* #### different */
    {
      cos_t = -cos_t;
      bb.x = -bb.x;
      bb.y = -bb.y;
      bb.z = -bb.z;
      bb.w = -bb.w;
    }

  if (1 - cos_t < FLT_EPSILON)
    {
      alpha = t;
      beta  = 1-t;
    }
  else
    {
      double theta = acos (cos_t);
      double sin_t = sin (theta);
      alpha = sin (t * theta) / sin_t;
      beta  = sin (theta - t * theta) / sin_t;
    }

  out.x = beta * a.x + alpha * bb.x;
  out.y = beta * a.y + alpha * bb.y;
  out.z = beta * a.z + alpha * bb.z;
  out.w = beta * a.w + alpha * bb.w;
  return out;

#else

  /* ##### this one is different but seems worse */

  quat out;

  double cos_t = (a.x * b.x +
                  a.y * b.y +
                  a.z * b.z +
                  a.w * b.w);
  if (fabs(cos_t) >= 1.0)
    {
      return a;
    }

  double theta = acos(cos_t);
  double sinHalfTheta = sqrt(1 - cos_t * cos_t);

  if (fabs(sinHalfTheta) < 0.001)
    {
      out.w = (a.w * 0.5 + b.w * 0.5);
      out.x = (a.x * 0.5 + b.x * 0.5);
      out.y = (a.y * 0.5 + b.y * 0.5);
      out.z = (a.z * 0.5 + b.z * 0.5);
      return out;
    }

  double ratioA = sin((1 - t) * theta) / sinHalfTheta;
  double ratioB = sin(t * theta) / sinHalfTheta;

  out.w = (a.w * ratioA + b.w * ratioB);
  out.x = (a.x * ratioA + b.x * ratioB);
  out.y = (a.y * ratioA + b.y * ratioB);
  out.z = (a.z * ratioA + b.z * ratioB);
  return out;

#endif
}
