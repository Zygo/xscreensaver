/* Triangulate
   Efficient Triangulation Algorithm Suitable for Terrain Modelling
   or
   An Algorithm for Interpolating Irregularly-Spaced Data
   with Applications in Terrain Modelling

   Written by Paul Bourke
   Presented at Pan Pacific Computer Conference, Beijing, China.
   January 1989
   Abstract

   A discussion of a method that has been used with success in terrain
   modelling to estimate the height at any point on the land surface
   from irregularly distributed samples. The special requirements of
   terrain modelling are discussed as well as a detailed description
   of the algorithm and an example of its application.

   http://paulbourke.net/papers/triangulate/
   http://paulbourke.net/papers/triangulate/triangulate.c
 */

#ifndef __DELAUNAY_H__
#define __DELAUNAY_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

typedef struct {
   double x,y,z;
} XYZ;

typedef struct {
   int p1,p2,p3;
} ITRIANGLE;

/*
   Takes as input NV vertices in array pxyz
   Returned is a list of ntri triangular faces in the array v
   These triangles are arranged in a consistent clockwise order.
   The triangle array 'v' should be malloced to 3 * nv
   The vertex array pxyz must be big enough to hold 3 more points
   The vertex array must be sorted in increasing x values
 */
extern int delaunay (int nv, XYZ *pxyz, ITRIANGLE *v, int *ntri);

/* qsort(p,nv,sizeof(XYZ), delaunay_xyzcompare); */
extern int delaunay_xyzcompare (const void *v1, const void *v2);


#endif /* __DELAUNAY_H__ */

