/*
 * (c) Copyright 1993, 1994, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
/*
 * Trackball code:
 *
 * Implementation of a virtual trackball.
 * Implemented by Gavin Bell, lots of ideas from Thant Tessman and
 *   the August '88 issue of Siggraph's "Computer Graphics," pp. 121-129.
 *
 * Vector manip code:
 *
 * Original code from:
 * David M. Ciemiewicz, Mark Grossman, Henry Moreton, and Paul Haeberli
 *
 * Much mucking with by:
 * Gavin Bell
 *
 * 26-Feb-2026: jwz: converted from arrays to 'typedef quat'.
 */

#include <math.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "trackball.h"

#ifndef  M_SQRT2
# define M_SQRT2   1.41421356237309504880  /* sqrt(2) */
#endif

#ifndef  M_SQRT1_2
# define M_SQRT1_2 0.70710678118654752440  /* 1/sqrt(2) */
#endif


/*
 * This size should really be based on the distance from the center of
 * rotation to the point on the object underneath the mouse.  That
 * point would then track the mouse as closely as possible.  This is a
 * simple example, though, so that is left as an Exercise for the
 * Programmer.
 */
#define TRACKBALLSIZE  (0.8)

/* Project an x,y pair onto a sphere of radius r OR a hyperbolic sheet
 * if we are away from the center of the sphere.
 */
static double
tb_project_to_sphere (double r, double x, double y)
{
  double d, t, z;

  d = sqrt(x*x + y*y);

  if (d < r * M_SQRT1_2) {	/* Inside sphere */
    z = sqrt(r*r - d*d);
  } else {			/* On hyperbola */
    t = r / M_SQRT2;
    z = t*t / d;
  }
  return z;
}

/* Ok, simulate a track-ball.  Project the points onto the virtual
 * trackball, then figure out the axis of rotation, which is the cross
 * product of P1 P2 and O P1 (O is the center of the ball, 0,0,0)
 * Note:  This is a deformed trackball-- is a trackball in the center,
 * but is deformed into a hyperbolic sheet of rotation away from the
 * center.  This particular function was chosen after trying out
 * several variations.
 *
 * It is assumed that the arguments to this routine are in the range
 * (-1.0 ... 1.0)
 */
quat
trackball (double p1x, double p1y, double p2x, double p2y)
{
  vec3 a;      /* Axis of rotation */
  double phi;  /* how much to rotate about axis */
  vec3 p1, p2, d;
  double t;

  if (p1x == p2x && p1y == p2y) {
    quat o = { 0, 0, 0, 1 };
    return o;
  }

  /*
   * First, figure out z-coordinates for projection of P1 and P2 to
   * deformed sphere
   */
  p1.x = p1x;
  p1.y = p1y;
  p1.z = tb_project_to_sphere (TRACKBALLSIZE, p1x, p1y);
  p2.x = p2x;
  p2.y = p2y;
  p2.z = tb_project_to_sphere (TRACKBALLSIZE, p2x, p2y);

  a = vec3_cross (p2, p1);

  /* Figure out how much to rotate around that axis. */
  d = vec3_sub (p1, p2);
  t = vec3_length(d) / (2.0*TRACKBALLSIZE);

  /* Avoid problems with out-of-control values... */
  if (t > 1.0)  t = 1.0;
  if (t < -1.0) t = -1.0;
  phi = 2.0 * asin(t);

  return axis_to_quat (a, phi);
}
