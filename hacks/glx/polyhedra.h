/* xscreensaver, Copyright (c) 2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __POLYHEDRA_H__
#define __POLYHEDRA_H__

typedef struct {
  double x, y, z;
} point;

typedef struct {
  int color;
  int npoints;
  int *points;   /* indexes into point list */
} face;

typedef struct {

  int number;
  char *wythoff;
  char *name;
  char *dual;
  char *config;
  char *group;
  char *class;

  int nfaces;
  int logical_faces;
  int logical_vertices;
  int nedges;
  int npoints;
  int density;
  int chi;

  point *points;
  face *faces;

} polyhedron;


extern int construct_polyhedra (polyhedron ***polyhedra_ret);
extern void free_polyhedron (polyhedron *p);

#endif /* __POLYHEDRA_H__ */
