/* sphereeversion-corrugations --- Shows a sphere eversion, i.e., a
   smooth deformation (homotopy) that turns a sphere inside out.
   During the eversion, the deformed sphere is allowed to intersect
   itself transversally.  However, no creases or pinch points are
   allowed to occur. */

/* Copyright (c) 2021-2022 Carsten Steger <carsten@mirsanmir.org>. */

/*
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * REVISION HISTORY:
 * C. Steger - 22/02/25: Initial version
 */

/*
 * This program shows a sphere eversion, i.e., a smooth deformation
 * (homotopy) that turns a sphere inside out.  During the eversion,
 * the deformed sphere is allowed to intersect itself transversally.
 * However, no creases or pinch points are allowed to occur.
 *
 * The deformed sphere can be projected to the screen either
 * perspectively or orthographically.
 *
 * There are three display modes for the sphere: solid, transparent,
 * or random.  If random mode is selected, the mode is changed each
 * time an eversion has been completed.
 *
 * The appearance of the sphere can be as a solid object, as a set of
 * see-through bands, or random.  The bands can be parallel bands or
 * meridian bands, i.e., bands that run along the parallels (lines of
 * latitude) or bands that run along the meridians (lines of
 * longitude) of the sphere.  If random mode is selected, the
 * appearance is changed each time an eversion has been completed.
 *
 * The colors with with the sphere is drawn can be set to two-sided,
 * parallel, meridian, earth, or random.  In two-sided mode, the
 * sphere is drawn with gold on one side and purple on the other side.
 * In parallel mode, the sphere is displayed with colors that run from
 * blue to white to orange on one side of the surface and from magenta
 * to black to green on the other side.  The colors are aligned with
 * the parallels of the sphere in this mode.  In meridian mode, the
 * the sphere is displayed with colors that run from blue to white to
 * orange to black and back to blue on one side of the surface and
 * from magenta to white to green to black and back to magenta on the
 * other side.  In earth mode, the sphere is drawn with a texture of
 * earth by day on one side and with a texture of earth by night on
 * the other side.  Initially, the earth by day is on the outside and
 * the earth by night on the inside.  After the first eversion, the
 * earth by night will be on the outside.  All points of the earth on
 * the inside and outside are at the same positions on the sphere.
 * Since an eversion transforms the sphere into its inverse, the earth
 * by night will appear with all continents mirror reversed.  If
 * random mode is selected, the color scheme is changed each time an
 * eversion has been completed.
 *
 * By default, the sphere is rotated to a new viewing position each
 * time an eversion has been completed.  In addition, it is possible
 * to rotate the sphere while it is deforming.  The rotation speed for
 * each of the three coordinate axes around which the sphere rotates
 * can be chosen arbitrarily.  For best effects, however, it is
 * suggested to rotate only around the z axis while the sphere is
 * deforming.
 *
 * This program is inspired inspired by the video "Outside In" by the
 * Geometry Center (Bill Thurston, Silvio Levy, Delle Maxwell, Tamara
 * Munzner, Nathaniel Thurston, David Ben-Zvi, Matt Headrick, et al.),
 * 1994, and the accompanying booklet Silvio Levy: "Making Waves - A
 * Guide to the Ideas Behind Outside In", A K Peters, Wellesley, MA,
 * 1995.  Parts of the code in this program are based on the program
 * sphereEversion-0.4-src.zip (https://profs.etsmtl.ca/mmcguffin/eversion/)
 * by Michael J. McGuffin, which, in turn, is based on the program
 * evert.tar.Z (http://www.geom.uiuc.edu/docs/outreach/oi/software.html)
 * developed by Nathaniel Thurston at the Geometry Center.  The modified
 * code is used with permission.
 *
 * The sphere eversion method is described in detail in the video and
 * booklet mentioned in the previous paragraph. Briefly, the method
 * works as follows: Imagine the sphere cut into eight spherical lunes
 * (spherical biangles).  Now imagine each lune to be a belt.  The
 * ends of the belt (which correspond to the north and south poles of
 * the sphere) are pushed past each other.  This creates a loop in the
 * belt.  If the belt were straightened out, it would contain a 360
 * degree rotation.  This rotation can be removed by rotating each end
 * of the belt by 180 degrees.  Finally, the belt is pushed to the
 * opposite side of the sphere, which causes the side of the belt that
 * initially was inside the sphere to appear on the outside.
 *
 * The method described so far only works for a single lune (belt) and
 * not for the entire sphere.  To make it work for the entire sphere,
 * corrugations (i.e., waves) must be added to the sphere.  This
 * happens in the first phase of the eversion.  Then, the method
 * described above is applied to the eight lunes.  Finally, the
 * corrugations are removed to obtain the everted sphere.
 *
 * To see the eversion for a single lune, the option -lunes-1 can be
 * used.  Using this option, the eversion, as described above, is
 * easier to understand.  It is also possible to display two lunes
 * using -lunes-2 and four lunes using -lunes-4.  Using fewer than
 * eight lunes reduces the visual complexity of the eversion and may
 * help to understand the method.
 *
 * Furthermore, it is possible to display only one hemisphere using
 * the option -hemispheres-1.  This allows to see what is happening in
 * the center of the sphere during the eversion.  Note that the north
 * and south half of the sphere move in a symmetric fashion during the
 * eversion.  Hence, the eversion is actually composed of 16
 * semi-lunes (spherical triangles from the equator to the poles) that
 * all deform in the same manner.  By specifying -lunes-1
 * -hemispheres-1, the deformation of one semi-lune can be observed.
 *
 * Note that the options described above are only intended for
 * educational purposes.  They are not used if none of them are
 * explicitly specified.
 */


#ifdef STANDALONE
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */


#ifdef USE_GL

#include "sphereeversion.h"

#include <float.h>


/* Number of subdivisions of the surface */
#define NUM_STRIPS 8
#define NUM_U      128
#define NUM_V      64

/* Number of subdivisions per band */
#define NUMB_PAR (NUM_U/8)
#define NUMB_MER (NUM_V/2)


/* Compute a 4x4 rotation matrix from an xscreensaver unit quaternion. Note
   that xscreensaver has a different convention for unit quaternions than
   the one that is used in this hack. */
static void quat_to_rotmat(float p[4], float r[16])
{
  float al, be, de;
  float r00, r01, r02, r12, r22;
  float m[3][3];
  int   i, j;

  r00 = 1.0f-2.0f*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0f*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0f*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0f*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0f-2.0f*(p[1]*p[1]+p[0]*p[0]);

  al = atan2f(-r12,r22)*180.0f/M_PI_F;
  be = atan2f(r02,sqrtf(r00*r00+r01*r01))*180.0f/M_PI_F;
  de = atan2f(-r01,r00)*180.0f/M_PI_F;
  rotateall(al,be,de,m);
  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
      r[j*4+i] = m[i][j];
    r[3*4+i] = 0.0f;
    r[i*4+3] = 0.0f;
  }
  r[3*4+3] = 1.0f;
}


/* ------------------------------------------------------------------------- */


typedef struct {
  float f;
  float fu, fv;
} one_jet;


typedef struct {
  float f;
  float fu, fv;
  float fuu, fuv, fvv;
} two_jet;


typedef struct {
  one_jet x;
  one_jet y;
  one_jet z;
} one_jet_vec;


typedef struct {
  two_jet x;
  two_jet y;
  two_jet z;
} two_jet_vec;


typedef void surface_time_function(float u, float v, float t,
                                   int num_strips, one_jet_vec *val);


/* ------------------------------------------------------------------------- */


static inline void copy_one_jet(const one_jet *x, one_jet *res)
{
  res->f = x->f;
  res->fu = x->fu;
  res->fv = x->fv;
}


static inline void set_one_jet(float d, float du, float dv, one_jet *res)
{
  res->f = d;
  res->fu = du;
  res->fv = dv;
}


static inline void add_one_jets(const one_jet *x, const one_jet *y,
                                one_jet *res)
{
  res->f = x->f+y->f;
  res->fu = x->fu+y->fu;
  res->fv = x->fv+y->fv;
}


static inline void sub_one_jets(const one_jet *x, const one_jet *y,
                                one_jet *res)
{
  res->f = x->f-y->f;
  res->fu = x->fu-y->fu;
  res->fv = x->fv-y->fv;
}


static inline void mult_one_jets(const one_jet *x, const one_jet *y,
                                 one_jet *res)
{
  res->f = x->f*y->f;
  res->fu = x->f*y->fu+x->fu*y->f;
  res->fv = x->f*y->fv+x->fv*y->f;
}


static inline void add_one_jet_float(const one_jet *x, float d, one_jet *res)
{
  res->f = x->f+d;
  res->fu = x->fu;
  res->fv = x->fv;
}


static inline void mult_one_jet_float(const one_jet *x, float d, one_jet *res)
{
  res->f = d*x->f;
  res->fu = d*x->fu;
  res->fv = d*x->fv;
}


static inline void neg_one_jet(const one_jet *x, one_jet *res)
{
  res->f = -x->f;
  res->fu = -x->fu;
  res->fv = -x->fv;
}


static inline void fmod_one_jet(const one_jet *x, float d, one_jet *res)
{
  res->f = fmodf(x->f,d);
  if (res->f < 0.0f)
    res->f += d;
  res->fu = x->fu;
  res->fv = x->fv;
}


static inline void sin_one_jet(const one_jet *x, one_jet *res)
{
  float   s, c;
  one_jet t;

  mult_one_jet_float(x,2.0f*M_PI_F,&t);
  s = sinf(t.f);
  c = cosf(t.f);
  res->f = s;
  res->fu = c*t.fu;
  res->fv = c*t.fv;
}


static inline void cos_one_jet(const one_jet *x, one_jet *res)
{
  float   s, c;
  one_jet t;

  mult_one_jet_float(x,2.0f*M_PI_F,&t);
  s = cosf(t.f);
  c = -sinf(t.f);
  res->f = s;
  res->fu = c*t.fu;
  res->fv = c*t.fv;
}


static inline void pow_one_jet(const one_jet *x, float n, one_jet *res)
{
  float x0, x1;

  x0 = powf(x->f,n);
  x1 = (x->f == 0.0f) ? 0.0f : n*x0/x->f;
  res->f = x0;
  res->fu = x1*x->fu;
  res->fv = x1*x->fv;
}


static inline void annihilate_one_jet(const one_jet *x, int index,
                                      one_jet *res)
{
  res->f = x->f;
  if (index == 0)
  {
    res->fu = 0.0f;
    res->fv = x->fv;
  }
  else
  {
    res->fu = x->fu;
    res->fv = 0.0f;
  }
}


static inline void interpolate_one_jets_float(const one_jet *v1,
                                              const one_jet *v2,
                                              const one_jet *weight,
                                              one_jet *res)
{
  one_jet neg_weight, one_minus_weight, v1w, v2w;

  neg_one_jet(weight,&neg_weight);
  add_one_jet_float(&neg_weight,1.0f,&one_minus_weight);
  mult_one_jets(v1,&one_minus_weight,&v1w);
  mult_one_jets(v2,weight,&v2w);
  add_one_jets(&v1w,&v2w,res);
}


/* ------------------------------------------------------------------------- */


static inline void copy_two_jet(const two_jet *x, two_jet *res)
{
  res->f = x->f;
  res->fu = x->fu;
  res->fv = x->fv;
  res->fuu = x->fuu;
  res->fuv = x->fuv;
  res->fvv = x->fvv;
}


static inline void set_two_jet(float d, float du, float dv, two_jet *res)
{
  res->f = d;
  res->fu = du;
  res->fv = dv;
  res->fuu = 0.0f;
  res->fuv = 0.0f;
  res->fvv = 0.0f;
}


static inline void two_jet_to_one_jet(const two_jet *x, one_jet *res)
{
  res->f = x->f;
  res->fu = x->fu;
  res->fv = x->fv;
}


static inline void add_two_jets(const two_jet *x, const two_jet *y,
                                two_jet *res)
{
  res->f = x->f+y->f;
  res->fu = x->fu+y->fu;
  res->fv = x->fv+y->fv;
  res->fuu = x->fuu+y->fuu;
  res->fuv = x->fuv+y->fuv;
  res->fvv = x->fvv+y->fvv;
}


static inline void sub_two_jets(const two_jet *x, const two_jet *y,
                                two_jet *res)
{
  res->f = x->f-y->f;
  res->fu = x->fu-y->fu;
  res->fv = x->fv-y->fv;
  res->fuu = x->fuu-y->fuu;
  res->fuv = x->fuv-y->fuv;
  res->fvv = x->fvv-y->fvv;
}


static inline void mult_two_jets(const two_jet *x, const two_jet *y,
                                 two_jet *res)
{
  res->f = x->f*y->f;
  res->fu = x->f*y->fu+x->fu*y->f;
  res->fv = x->f*y->fv+x->fv*y->f;
  res->fuu = x->f*y->fuu+2.0f*x->fu*y->fu+x->fuu*y->f;
  res->fuv = x->f*y->fuv+x->fu*y->fv+x->fv*y->fu+x->fuv*y->f;
  res->fvv = x->f*y->fvv+2.0f*x->fv*y->fv+x->fvv*y->f;
}


static inline void add_two_jet_float(const two_jet *x, float d, two_jet *res)
{
  res->f = x->f+d;
  res->fu = x->fu;
  res->fv = x->fv;
  res->fuu = x->fuu;
  res->fuv = x->fuv;
  res->fvv = x->fvv;
}


static inline void add_two_jet_float_inplace(float d, two_jet *res)
{
  res->f += d;
}


static inline void mult_two_jet_float(const two_jet *x, float d, two_jet *res)
{
  res->f = d*x->f;
  res->fu = d*x->fu;
  res->fv = d*x->fv;
  res->fuu = d*x->fuu;
  res->fuv = d*x->fuv;
  res->fvv = d*x->fvv;
}


static inline void neg_two_jet(const two_jet *x, two_jet *res)
{
  res->f = -x->f;
  res->fu = -x->fu;
  res->fv = -x->fv;
  res->fuu = -x->fuu;
  res->fuv = -x->fuv;
  res->fvv = -x->fvv;
}


static inline void fmod_two_jet(const two_jet *x, float d, two_jet *res)
{
  res->f = fmodf(x->f,d);
  if (res->f < 0.0f)
    res->f += d;
  res->fu = x->fu;
  res->fv = x->fv;
  res->fuu = x->fuu;
  res->fuv = x->fuv;
  res->fvv = x->fvv;
}


static inline void sin_two_jet(const two_jet *x, two_jet *res)
{
  float   s, c;
  two_jet t;

  mult_two_jet_float(x,2.0f*M_PI_F,&t);
  s = sinf(t.f);
  c = cosf(t.f);
  res->f = s;
  res->fu = c*t.fu;
  res->fv = c*t.fv;
  res->fuu = c*t.fuu-s*t.fu*t.fu;
  res->fuv = c*t.fuv-s*t.fu*t.fv;
  res->fvv = c*t.fvv-s*t.fv*t.fv;
}


static inline void cos_two_jet(const two_jet *x, two_jet *res)
{
  float   s, c;
  two_jet t;

  mult_two_jet_float(x,2.0f*M_PI_F,&t);
  s = cosf(t.f);
  c = -sinf(t.f);
  res->f = s;
  res->fu = c*t.fu;
  res->fv = c*t.fv;
  res->fuu = c*t.fuu-s*t.fu*t.fu;
  res->fuv = c*t.fuv-s*t.fu*t.fv;
  res->fvv = c*t.fvv-s*t.fv*t.fv;
}


static inline void pow_two_jet(const two_jet *x, float n, two_jet *res)
{
  float x0, x1, x2;

  x0 = powf(x->f,n);
  x1 = x->f == 0.0f ? 0.0f : n*x0/x->f;
  x2 = x->f == 0.0f ? 0.0f : (n-1.0f)*x1/x->f;

  res->f = x0;
  res->fu = x1*x->fu;
  res->fv = x1*x->fv;
  res->fuu = x1*x->fuu+x2*x->fu*x->fu;
  res->fuv = x1*x->fuv+x2*x->fu*x->fv;
  res->fvv = x1*x->fvv+x2*x->fv*x->fv;
}


static inline void differentiate_two_jet(const two_jet *x, int index,
                                         one_jet *res)
{
  if (index == 0)
  {
    res->f = x->fu;
    res->fu = x->fuu;
    res->fv = x->fuv;
  }
  else
  {
    res->f = x->fv;
    res->fu = x->fuv;
    res->fv = x->fvv;
  }
}


static inline void annihilate_two_jet(const two_jet *x, int index,
                                      two_jet *res)
{
  res->f = x->f;
  if (index == 0)
  {
    res->fu = 0.0f;
    res->fv = x->fv;
    res->fuu = 0.0f;
    res->fuv = 0.0f;
    res->fvv = x->fvv;
  }
  else
  {
    res->fu = x->fu;
    res->fv = 0.0f;
    res->fuu = x->fuu;
    res->fuv = 0.0f;
    res->fvv = 0.0f;
  }
}


/* ------------------------------------------------------------------------- */


static inline void add_one_jet_vecs(const one_jet_vec *v,
                                    const one_jet_vec *w,
                                    one_jet_vec *res)
{
  add_one_jets(&v->x,&w->x,&res->x);
  add_one_jets(&v->y,&w->y,&res->y);
  add_one_jets(&v->z,&w->z,&res->z);
}


static inline void mult_one_jet_vec_one_jet(const one_jet_vec *v,
                                            const one_jet *a,
                                            one_jet_vec *res)
{
  mult_one_jets(&v->x,a,&res->x);
  mult_one_jets(&v->y,a,&res->y);
  mult_one_jets(&v->z,a,&res->z);
}


static inline void annihilate_one_jet_vec(const one_jet_vec *v, int index,
                                          one_jet_vec *res)
{
  annihilate_one_jet(&v->x,index,&res->x);
  annihilate_one_jet(&v->y,index,&res->y);
  annihilate_one_jet(&v->z,index,&res->z);
}


static inline void cross_one_jet_vecs(const one_jet_vec *v,
                                      const one_jet_vec *w,
                                      one_jet_vec *res)
{
  one_jet vxwy, vxwz, vywx, vywz, vzwx, vzwy;

  mult_one_jets(&v->y,&w->z,&vywz);
  mult_one_jets(&v->z,&w->y,&vzwy);
  sub_one_jets(&vywz,&vzwy,&res->x);
  mult_one_jets(&v->z,&w->x,&vzwx);
  mult_one_jets(&v->x,&w->z,&vxwz);
  sub_one_jets(&vzwx,&vxwz,&res->y);
  mult_one_jets(&v->x,&w->y,&vxwy);
  mult_one_jets(&v->y,&w->x,&vywx);
  sub_one_jets(&vxwy,&vywx,&res->z);
}


static inline void dot_one_jet_vecs(const one_jet_vec *v, const one_jet_vec *w,
                                    one_jet *res)
{
  one_jet vxwx, vywy, vzwz, vxwxpvywy;

  mult_one_jets(&v->x,&w->x,&vxwx);
  mult_one_jets(&v->y,&w->y,&vywy);
  mult_one_jets(&v->z,&w->z,&vzwz);
  add_one_jets(&vxwx,&vywy,&vxwxpvywy);
  add_one_jets(&vxwxpvywy,&vzwz,res);
}


static inline void normalize_one_jet_vec(const one_jet_vec *v,
                                         one_jet_vec *res)
{
  one_jet a, sqrt_a;

  dot_one_jet_vecs(v,v,&a);
  if (a.f > 0.0f)
    pow_one_jet(&a,-0.5f,&sqrt_a);
  else
    set_one_jet(0.0f,0.0f,0.0f,&sqrt_a);
  mult_one_jet_vec_one_jet(v,&sqrt_a,res);
}


static inline void rotate_one_jet_vec_z_float(const one_jet_vec *v,
                                              const one_jet *angle,
                                              one_jet_vec *res)
{
  one_jet s, c, vxc, vxs, vyc, vys;

  sin_one_jet(angle,&s);
  cos_one_jet(angle,&c);
  mult_one_jets(&v->x,&c,&vxc);
  mult_one_jets(&v->y,&s,&vys);
  add_one_jets(&vxc,&vys,&res->x);
  mult_one_jets(&v->x,&s,&vxs);
  mult_one_jets(&v->y,&c,&vyc);
  sub_one_jets(&vyc,&vxs,&res->y);
  copy_one_jet(&v->z,&res->z);
}


/* ------------------------------------------------------------------------- */


static inline void two_jet_vec_to_one_jet_vec(const two_jet_vec *v,
                                              one_jet_vec *res)
{
  two_jet_to_one_jet(&v->x,&res->x);
  two_jet_to_one_jet(&v->y,&res->y);
  two_jet_to_one_jet(&v->z,&res->z);
}


static inline void add_two_jet_vecs(const two_jet_vec *v, const two_jet_vec *w,
                                    two_jet_vec *res)
{
  add_two_jets(&v->x,&w->x,&res->x);
  add_two_jets(&v->y,&w->y,&res->y);
  add_two_jets(&v->z,&w->z,&res->z);
}


static inline void mult_two_jet_vec_two_jet(const two_jet_vec *v,
                                            const two_jet *a,
                                            two_jet_vec *res)
{
  mult_two_jets(&v->x,a,&res->x);
  mult_two_jets(&v->y,a,&res->y);
  mult_two_jets(&v->z,a,&res->z);
}


static inline void mult_two_jet_vec_float(const two_jet_vec *v, float a,
                                          two_jet_vec *res)
{
  mult_two_jet_float(&v->x,a,&res->x);
  mult_two_jet_float(&v->y,a,&res->y);
  mult_two_jet_float(&v->z,a,&res->z);
}


static inline void annihilate_two_jet_vec(const two_jet_vec *v, int index,
                                          two_jet_vec *res)
{
  annihilate_two_jet(&v->x,index,&res->x);
  annihilate_two_jet(&v->y,index,&res->y);
  annihilate_two_jet(&v->z,index,&res->z);
}


static inline void differentiate_two_jet_vec(const two_jet_vec *x, int index,
                                             one_jet_vec *res)
{
  differentiate_two_jet(&x->x,index,&res->x);
  differentiate_two_jet(&x->y,index,&res->y);
  differentiate_two_jet(&x->z,index,&res->z);
}


static inline void rotate_two_jet_vec_z_float(const two_jet_vec *v,
                                              float angle, two_jet_vec *res)
{
  float   s, c;
  two_jet xs, xc, ys, yc;

  s = sinf(angle*2.0f*M_PI_F);
  c = cosf(angle*2.0f*M_PI_F);
  mult_two_jet_float(&v->x,c,&xc);
  mult_two_jet_float(&v->y,s,&ys);
  add_two_jets(&xc,&ys,&res->x);
  mult_two_jet_float(&v->x,-s,&xs);
  mult_two_jet_float(&v->y,c,&yc);
  add_two_jets(&xs,&yc,&res->y);
  copy_two_jet(&v->z,&res->z);
}


static inline void rotate_two_jet_vec_y_float(const two_jet_vec *v,
                                              float angle, two_jet_vec *res)
{
  float   s, c;
  two_jet xs, xc, zs, zc;

  s = sinf(angle*2.0f*M_PI_F);
  c = cosf(angle*2.0f*M_PI_F);
  mult_two_jet_float(&v->x,c,&xc);
  mult_two_jet_float(&v->z,-s,&zs);
  add_two_jets(&xc,&zs,&res->x);
  copy_two_jet(&v->y,&res->y);
  mult_two_jet_float(&v->x,s,&xs);
  mult_two_jet_float(&v->z,c,&zc);
  add_two_jets(&xs,&zc,&res->z);
}


static inline void interpolate_two_jet_vecs_two_jet(const two_jet_vec *v1,
                                                    const two_jet_vec *v2,
                                                    const two_jet *weight,
                                                    two_jet_vec *res)
{
  two_jet_vec v1w, v2w;
  two_jet     neg_weight, one_minus_weight;

  neg_two_jet(weight,&neg_weight);
  add_two_jet_float(&neg_weight,1.0f,&one_minus_weight);
  mult_two_jet_vec_two_jet(v1,&one_minus_weight,&v1w);
  mult_two_jet_vec_two_jet(v2,weight,&v2w);
  add_two_jet_vecs(&v1w,&v2w,res);
}


static inline void interpolate_two_jet_vecs_float(const two_jet_vec *v1,
                                                  const two_jet_vec *v2,
                                                  float weight,
                                                  two_jet_vec *res)
{
  two_jet_vec v1w, v2w;

  mult_two_jet_vec_float(v1,1.0f-weight,&v1w);
  mult_two_jet_vec_float(v2,weight,&v2w);
  add_two_jet_vecs(&v1w,&v2w,res);
}


/* ------------------------------------------------------------------------- */


static void figure_eight(const one_jet_vec *w, const one_jet_vec *h,
                         const one_jet_vec *bend, const one_jet *form,
                         const one_jet *v, one_jet_vec *res)
{
  one_jet_vec hh, w_sin_vv2, hh_interp, bend_heights_sqs;
  one_jet     height, height_neg, heights, heights_sq, heights_sqs;
  one_jet     vv, vv2, cos_vv, sin_vv2, cos_vv2, cos_vv2_m1;
  one_jet     cos_vv_m1, cos_vv_m1_xm2, interp;

  fmod_one_jet(v,1.0f,&vv);
  cos_one_jet(&vv,&cos_vv);
  mult_one_jet_float(&vv,2.0f,&vv2);
  sin_one_jet(&vv2,&sin_vv2);
  cos_one_jet(&vv2,&cos_vv2);
  add_one_jet_float(&cos_vv2,-1.0f,&cos_vv2_m1);
  neg_one_jet(&cos_vv2_m1,&height);
  if (vv.f > 0.25f && vv.f < 0.75f)
  {
    neg_one_jet(&height,&height_neg);
    add_one_jet_float(&height_neg,4.0f,&height);
  }
  mult_one_jet_float(&height,0.6f,&heights);
  mult_one_jets(&heights,&heights,&heights_sq);
  mult_one_jet_float(&heights_sq,1.0f/64.0f,&heights_sqs);
  mult_one_jet_vec_one_jet(bend,&heights_sqs,&bend_heights_sqs);
  add_one_jet_vecs(h,&bend_heights_sqs,&hh);
  add_one_jet_float(&cos_vv,-1.0f,&cos_vv_m1);
  mult_one_jet_float(&cos_vv_m1,-2.0f,&cos_vv_m1_xm2);
  interpolate_one_jets_float(&cos_vv_m1_xm2,&heights,form,&interp);
  mult_one_jet_vec_one_jet(w,&sin_vv2,&w_sin_vv2);
  mult_one_jet_vec_one_jet(&hh,&interp,&hh_interp);
  add_one_jet_vecs(&w_sin_vv2,&hh_interp,res);
}


static void add_figure_eight(two_jet_vec *p, two_jet *u, one_jet *v,
                             two_jet *form, two_jet *scale, int num_strips,
                             one_jet_vec *res)
{
  one_jet_vec dp, dpa, du, dv, po, fe, h, w, bend;
  one_jet_vec duxdv, duxdvn, hxdu, hxdun, dudsize, popfe;
  one_jet     fo, dsize, duu, sizeo, sizeos, vs, duuinv;
  two_jet_vec pa;
  two_jet     f, f2, ff, size;

  copy_two_jet(form,&f);
  mult_two_jets(&f,scale,&size);
  mult_two_jet_float(&f,2.0f,&f2);
  mult_two_jets(&f,&f,&ff);
  sub_two_jets(&f2,&ff,&f);
  two_jet_to_one_jet(&f,&fo);
  two_jet_to_one_jet(&size,&sizeo);
  differentiate_two_jet_vec(p,1,&dp);
  annihilate_one_jet_vec(&dp,1,&dv);
  annihilate_two_jet_vec(p,1,&pa);
  differentiate_two_jet_vec(&pa,0,&dpa);
  normalize_one_jet_vec(&dpa,&du);
  cross_one_jet_vecs(&du,&dv,&duxdv);
  normalize_one_jet_vec(&duxdv,&duxdvn);
  mult_one_jet_vec_one_jet(&duxdvn,&sizeo,&h);
  cross_one_jet_vecs(&h,&du,&hxdu);
  normalize_one_jet_vec(&hxdu,&hxdun);
  mult_one_jet_float(&sizeo,1.1f,&sizeos);
  mult_one_jet_vec_one_jet(&hxdun,&sizeos,&w);
  differentiate_two_jet(&size,0,&dsize);
  differentiate_two_jet(u,0,&duu);
  mult_one_jet_vec_one_jet(&du,&dsize,&dudsize);
  pow_one_jet(&duu,-1.0f,&duuinv);
  mult_one_jet_vec_one_jet(&dudsize,&duuinv,&bend);
  figure_eight(&w,&h,&bend,&fo,v,&fe);
  two_jet_vec_to_one_jet_vec(&pa,&po);
  mult_one_jet_float(v,1.0f/num_strips,&vs);
  add_one_jet_vecs(&po,&fe,&popfe);
  rotate_one_jet_vec_z_float(&popfe,&vs,res);
}


/* ------------------------------------------------------------------------- */


static void arc(const two_jet *u, float xsize, float ysize, float zsize,
                two_jet_vec *res)
{
  /* sin(two_jet(0.0f,0.0f,1.0f)): */
  static const two_jet sin_v = {
    0.0f, 0.0f, 2.0f*M_PI_F, 0.0f, 0.0f, 0.0f
  };
  /* cos(two_jet(0.0f,0.0f,1.0f)): */
  static const two_jet cos_v = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -4.0f*M_PI_F*M_PI_F
  };
  two_jet uq, sin_uq, cos_uq, sin_v_xsize, cos_v_ysize;

  mult_two_jet_float(u,0.25f,&uq);
  sin_two_jet(&uq,&sin_uq);
  cos_two_jet(&uq,&cos_uq);
  mult_two_jet_float(&sin_v,xsize,&sin_v_xsize);
  mult_two_jets(&sin_uq,&sin_v_xsize,&res->x);
  mult_two_jet_float(&cos_v,ysize,&cos_v_ysize);
  mult_two_jets(&sin_uq,&cos_v_ysize,&res->y);
  mult_two_jet_float(&cos_uq,zsize,&res->z);
}


static inline void param1(two_jet *x, two_jet *p)
{
  float   offset;
  two_jet xm, xm_sq, xm_sq_neg, xm_2;
  two_jet xm_sq_neg_xm_2, xm_sq_xm_2;

  offset = 0.0f;
  fmod_two_jet(x,4.0f,&xm);
  if (xm.f > 2.0f)
  {
    add_two_jet_float_inplace(-2.0f,&xm);
    offset = 2.0f;
  }
  pow_two_jet(&xm,2.0f,&xm_sq);
  if (xm.f <= 1.0f)
  {
    neg_two_jet(&xm_sq,&xm_sq_neg);
    mult_two_jet_float(&xm,2.0f,&xm_2);
    add_two_jets(&xm_sq_neg,&xm_2,&xm_sq_neg_xm_2);
    add_two_jet_float(&xm_sq_neg_xm_2,offset,p);
  }
  else
  {
    mult_two_jet_float(&xm,-2.0f,&xm_2);
    add_two_jets(&xm_sq,&xm_2,&xm_sq_xm_2);
    add_two_jet_float(&xm_sq_xm_2,offset+2.0f,p);
  }
}


static inline void param2(two_jet *x, two_jet *p)
{
  float   offset;
  two_jet xm, xm_sq, xm_sq_neg, xm_4, xm_sq_neg_xm_4;

  offset = 0.0f;
  fmod_two_jet(x,4.0f,&xm);
  if (xm.f > 2.0f)
  {
    add_two_jet_float_inplace(-2.0f,&xm);
    offset = 2.0f;
  }
  pow_two_jet(&xm,2.0f,&xm_sq);
  if (xm.f <= 1.0f)
  {
    add_two_jet_float(&xm_sq,offset,p);
  }
  else
  {
    neg_two_jet(&xm_sq,&xm_sq_neg);
    mult_two_jet_float(&xm,4.0f,&xm_4);
    add_two_jets(&xm_sq_neg,&xm_4,&xm_sq_neg_xm_4);
    add_two_jet_float(&xm_sq_neg_xm_4,offset-2.0f,p);
  }
}


static inline void u_interp(two_jet *x, two_jet *res)
{
  two_jet xm, xm_neg, xm_sq, xm_cu;
  two_jet xm_sq_3, xm_cu_m2;

  fmod_two_jet(x,2.0f,&xm);
  if (xm.f > 1.0f)
  {
    neg_two_jet(&xm,&xm_neg);
    add_two_jet_float(&xm_neg,2.0f,&xm);
  }
  pow_two_jet(&xm,2.0f,&xm_sq);
  pow_two_jet(&xm,3.0f,&xm_cu);
  mult_two_jet_float(&xm_sq,3.0f,&xm_sq_3);
  mult_two_jet_float(&xm_cu,-2.0f,&xm_cu_m2);
  add_two_jets(&xm_sq_3,&xm_cu_m2,res);
}


static inline void ff_interp(two_jet *x, two_jet *res)
{
  static const float ffpow = 3.0f;
  two_jet xm, xm_neg, xms, xm_ffm1, xm_ff;
  two_jet xm_ffm1_ff, xm_ff_1mff;

  fmod_two_jet(x,2.0f,&xm);
  if (xm.f > 1.0f)
  {
    neg_two_jet(&xm,&xm_neg);
    add_two_jet_float(&xm_neg,2.0f,&xm);
  }
  mult_two_jet_float(&xm,1.06f,&xms);
  add_two_jet_float(&xms,-0.05f,&xm);
  if (xm.f < 0.0f)
  {
    set_two_jet(0.0f,0.0f,0.0f,res);
  }
  else if (xm.f > 1.0f)
  {
    set_two_jet(1.0f,0.0f,0.0f,res);
  }
  else
  {
    pow_two_jet(&xm,ffpow-1.0f,&xm_ffm1);
    pow_two_jet(&xm,ffpow,&xm_ff);
    mult_two_jet_float(&xm_ffm1,ffpow,&xm_ffm1_ff);
    mult_two_jet_float(&xm_ff,1.0f-ffpow,&xm_ff_1mff);
    add_two_jets(&xm_ffm1_ff,&xm_ff_1mff,res);
  }
}


static inline void fs_interp(two_jet *x, two_jet *res)
{
  static const float fspow = 3.0f;
  two_jet xm, xm_neg, xm_fsm1, xm_fs;
  two_jet xm_fsm1_fs, xm_fs_1mfs;
  two_jet xm_fsm1_fs_xm_fs_1mfs;

  fmod_two_jet(x,2.0f,&xm);
  if (xm.f > 1.0f)
  {
    neg_two_jet(&xm,&xm_neg);
    add_two_jet_float(&xm_neg,2.0f,&xm);
  }
  pow_two_jet(&xm,fspow-1.0f,&xm_fsm1);
  pow_two_jet(&xm,fspow,&xm_fs);
  mult_two_jet_float(&xm_fsm1,fspow,&xm_fsm1_fs);
  mult_two_jet_float(&xm_fs,1.0f-fspow,&xm_fs_1mfs);
  add_two_jets(&xm_fsm1_fs,&xm_fs_1mfs,&xm_fsm1_fs_xm_fs_1mfs);
  mult_two_jet_float(&xm_fsm1_fs_xm_fs_1mfs,-0.2f,res);
}


/* ------------------------------------------------------------------------- */


static inline void stage1(two_jet *u, two_jet_vec *s1)
{
  arc(u,1.0f,1.0f,1.0f,s1);
}


static inline void stage2(two_jet *u, two_jet_vec *s2)
{
  two_jet     p1u, p2u, ui;
  two_jet_vec a1, a2;

  param1(u,&p1u);
  param2(u,&p2u);
  arc(&p1u,0.9f,0.9f,-1.0f,&a1);
  arc(&p2u,1.0f,1.0f,0.5f,&a2);
  u_interp(u,&ui);
  interpolate_two_jet_vecs_two_jet(&a1,&a2,&ui,s2);
}


static inline void stage3(two_jet *u, two_jet_vec *s3)
{
  two_jet     p1u, p2u, ui;
  two_jet_vec a1, a2;

  param1(u,&p1u);
  param2(u,&p2u);
  arc(&p1u,-0.9f,-0.9f,-1.0f,&a1);
  arc(&p2u,-1.0f,1.0f,-0.5f,&a2);
  u_interp(u,&ui);
  interpolate_two_jet_vecs_two_jet(&a1,&a2,&ui,s3);
}


static inline void stage4(two_jet *u, two_jet_vec *s4)
{
  arc(u,-1.0f,-1.0f,-1.0f,s4);
}


static inline void scene12(two_jet *u, float t, two_jet_vec *s12)
{
  two_jet_vec s1, s2;

  stage1(u,&s1);
  stage2(u,&s2);
  interpolate_two_jet_vecs_float(&s1,&s2,t,s12);
}


static inline void scene23(two_jet *u, float t, two_jet_vec *s23)
{
  two_jet     p1u, p2u, ui;
  two_jet_vec a1, a2, a1r, a2r;
  float       tt;

  t *= 0.5f;
  tt = (u->f <= 1.0f) ? t : -t;
  param1(u,&p1u);
  param2(u,&p2u);
  arc(&p1u,0.9f,0.9f,-1.0f,&a1);
  arc(&p2u,1.0f,1.0f,0.5f,&a2);
  rotate_two_jet_vec_z_float(&a1,tt,&a1r);
  rotate_two_jet_vec_y_float(&a2,t,&a2r);
  u_interp(u,&ui);
  interpolate_two_jet_vecs_two_jet(&a1r,&a2r,&ui,s23);
}


static inline void scene34(two_jet *u, float t, two_jet_vec *s34)
{
  two_jet_vec s3, s4;

  stage3(u,&s3);
  stage4(u,&s4);
  interpolate_two_jet_vecs_float(&s3,&s4,t,s34);
}


static inline void corrugate(float u, float v, float t, int num_strips,
                             one_jet_vec *val)
{
  two_jet_vec s1;
  two_jet     uj, form, scale, ui;
  one_jet     vj;

  set_two_jet(u,1.0f,0.0f,&uj);
  set_one_jet(v,0.0f,1.0f,&vj);
  stage1(&uj,&s1);
  ff_interp(&uj,&ui);
  mult_two_jet_float(&ui,t,&form);
  fs_interp(&uj,&scale);
  add_figure_eight(&s1,&uj,&vj,&form,&scale,num_strips,val);
}


static inline void push_through(float u, float v, float t, int num_strips,
                                one_jet_vec *val)
{
  two_jet_vec s12;
  two_jet     uj, form, scale;
  one_jet     vj;

  set_two_jet(u,1.0f,0.0f,&uj);
  set_one_jet(v,0.0f,1.0f,&vj);
  scene12(&uj,t,&s12);
  ff_interp(&uj,&form);
  fs_interp(&uj,&scale);
  add_figure_eight(&s12,&uj,&vj,&form,&scale,num_strips,val);
}


static inline void twist(float u, float v, float t, int num_strips,
                         one_jet_vec *val)
{
  two_jet_vec s23;
  two_jet     uj, form, scale;
  one_jet     vj;

  set_two_jet(u,1.0f,0.0f,&uj);
  set_one_jet(v,0.0f,1.0f,&vj);
  scene23(&uj,t,&s23);
  ff_interp(&uj,&form);
  fs_interp(&uj,&scale);
  add_figure_eight(&s23,&uj,&vj,&form,&scale,num_strips,val);
}


static inline void un_push(float u, float v, float t, int num_strips,
                           one_jet_vec *val)
{
  two_jet_vec s34;
  two_jet     uj, form, scale;
  one_jet     vj;

  set_two_jet(u,1.0f,0.0f,&uj);
  set_one_jet(v,0.0f,1.0f,&vj);
  scene34(&uj,t,&s34);
  ff_interp(&uj,&form);
  fs_interp(&uj,&scale);
  add_figure_eight(&s34,&uj,&vj,&form,&scale,num_strips,val);
}


static inline void un_corrugate(float u, float v, float t, int num_strips,
                                one_jet_vec *val)
{
  two_jet_vec s4;
  two_jet     uj, form, scale, ui;
  one_jet     vj;

  set_two_jet(u,1.0f,0.0f,&uj);
  set_one_jet(v,0.0f,1.0f,&vj);
  stage4(&uj,&s4);
  ff_interp(&uj,&ui);
  mult_two_jet_float(&ui,1.0f-t,&form);
  fs_interp(&uj,&scale);
  add_figure_eight(&s4,&uj,&vj,&form,&scale,num_strips,val);
}


/* ------------------------------------------------------------------------- */


static inline void gen_point_and_normal(one_jet_vec *p, float *point,
                                        float *normal)
{
  float x, y, z, nx, ny, nz, s;

  x = p->x.f;
  y = p->y.f;
  z = p->z.f;
  nx = p->y.fu*p->z.fv-p->z.fu*p->y.fv;
  ny = p->z.fu*p->x.fv-p->x.fu*p->z.fv;
  nz = p->x.fu*p->y.fv-p->y.fu*p->x.fv;
  s = nx*nx+ny*ny+nz*nz;
  if (s > 0.0f)
    s = sqrtf(1.0f/s);
  else
    s = 0.0f;

  point[0] = x;
  point[1] = y;
  point[2] = z;
  normal[0] = -nx*s;
  normal[1] = -ny*s;
  normal[2] = -nz*s;
}


static inline void gen_surface(surface_time_function *func, float umin,
                               float umax, int ucount, float vmin,
                               float vmax, int vcount, float t,
                               float *points, float *normals, int num_strips)
{
  one_jet_vec val;
  int         j, k, l;
  float       u, v, delta_u, delta_v;
  float       speedv;

  if (ucount <= 0 || vcount <= 0)
    return;

  delta_u = (umax-umin)/ucount;
  delta_v = (vmax-vmin)/vcount;

  for (j=0; j<=ucount; j++)
  {
    u = umin+j*delta_u;
    (*func)(u,0.0f,t,num_strips,&val);
    speedv = sqrtf(val.x.fv*val.x.fv+val.y.fv*val.y.fv+val.z.fv*val.z.fv);
    if (speedv == 0.0f)
    {
      /* Perturb a bit, hoping to avoid degeneracy */
      u += u < 1.0f ? FLT_EPSILON : -FLT_EPSILON;
    }
    for (k=0; k<=vcount; k++)
    {
      l = 3*(j*(vcount+1)+k);
      v = vmin+k*delta_v;
      (*func)(u,v,t,num_strips,&val);
      gen_point_and_normal(&val,&points[l],&normals[l]);
    }
  }
}


static void generate_geometry(float *points, float *normals, float time,
                              int num_strips, float u_min, int u_count,
                              float u_max, float v_min, int v_count,
                              float v_max)
{
  /* Start of corrugation */
  static const float corr_start   = 0.00f;
  /* Start of push (poles are pushed through each other) */
  static const float push_start   = 0.10f;
  /* Start of twist (poles rotate in opposite directions) */
  static const float twist_start  = 0.23f;
  /* Start of unpush (poles held fixed while corrugations pushed through
     center) */
  static const float unpush_start = 0.60f;
  /* Start of uncorrugation */
  static const float uncorr_start = 0.93f;

  if (time >= uncorr_start)
    gen_surface(un_corrugate,u_min,u_max,u_count,v_min,v_max,v_count,
                (time-uncorr_start)/(1.0f-uncorr_start),
                points,normals,num_strips);
  else if (time >= unpush_start)
    gen_surface(un_push,u_min,u_max,u_count,v_min,v_max,v_count,
                (time-unpush_start)/(uncorr_start-unpush_start),
                points,normals,num_strips);
  else if (time >= twist_start)
    gen_surface(twist,u_min,u_max,u_count,v_min,v_max,v_count,
                (time-twist_start)/(unpush_start-twist_start),
                points,normals,num_strips);
  else if (time >= push_start)
    gen_surface(push_through,u_min,u_max,u_count,v_min,v_max,v_count,
                (time-push_start)/(twist_start-push_start),
                points,normals,num_strips);
  else if (time >= corr_start)
    gen_surface(corrugate,u_min,u_max,u_count,v_min,v_max,v_count,
                (time-corr_start)/(push_start-corr_start),
                points,normals,num_strips);
}


/* ------------------------------------------------------------------------- */


/* Set up the surface colors for the main pass (i.e., index 0). */
static void setup_surface_colors(ModeInfo *mi)
{
  int   hemisphere, strip, j, k, l;
  float phi, theta, angle_strip;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  static const float matc[3][3] = {
    {  0.577350269f, -0.577350269f,  0.577350269f },
    {  0.211324865f,  0.788675135f,  0.577350269f },
    { -0.788675135f, -0.211324865f,  0.577350269f }
  };

  angle_strip = 2.0f*M_PI_F/NUM_STRIPS;

  if (se->colors[0] != COLORS_TWOSIDED && se->colors[0] != COLORS_EARTH)
  {
    for (hemisphere=0; hemisphere<2; hemisphere++)
    {
      for (strip=0; strip<NUM_STRIPS; strip++)
      {
        for (j=0; j<=NUM_U; j++)
        {
          if ((hemisphere & 1) == 0)
            theta = 0.5f*M_PI_F*(NUM_U-j)/NUM_U;
          else
            theta = -0.5f*M_PI_F*(NUM_U-j)/NUM_U;
          for (k=0; k<=NUM_V; k++)
          {
            if ((hemisphere & 1) == 0)
              phi = angle_strip*k/NUM_V;
            else
              phi = angle_strip*(NUM_V-k)/NUM_V;
            phi += strip*angle_strip;
            l = ((hemisphere*NUM_STRIPS+strip)*(NUM_U+1)+j)*(NUM_V+1)+k;
            if (se->colors[0] == COLORS_PARALLEL)
              color(se,(2.0f*theta+3.0f*M_PI_F/4.0f)*(2.0f/3.0f),matc,
                    &se->colf[0][4*l],&se->colb[0][4*l]);
            else
              color(se,phi,matc,&se->colf[0][4*l],&se->colb[0][4*l]);
          }
        }
      }
    }
  }
#ifdef VERTEXATTRIBARRAY_WORKAROUND
  else /* se->colors[0] == COLORS_TWOSIDED || se->colors[0] == COLORS_EARTH */
  {
    /* For some strange reason, the color buffer must be initialized
       and used on macOS. Otherwise two-sided lighting will not
       work. */
    int m;
    for (hemisphere=0; hemisphere<2; hemisphere++)
    {
      for (strip=0; strip<NUM_STRIPS; strip++)
      {
        for (j=0; j<=NUM_U; j++)
        {
          for (k=0; k<=NUM_V; k++)
          {
            l = ((hemisphere*NUM_STRIPS+strip)*(NUM_U+1)+j)*(NUM_V+1)+k;
            for (m=0; m<4; m++)
            {
              se->colf[0][4*l+m] = 1.0f;
              se->colb[0][4*l+m] = 1.0f;
            }
          }
        }
      }
    }
  }
#endif /* VERTEXATTRIBARRAY_WORKAROUND */

#ifdef HAVE_GLSL
  if (se->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorf_buffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 4*2*NUM_STRIPS*(NUM_U+1)*(NUM_V+1)*sizeof(GLfloat),
                 se->colf[0],GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorb_buffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 4*2*NUM_STRIPS*(NUM_U+1)*(NUM_V+1)*sizeof(GLfloat),
                 se->colb[0],GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
  }
#endif /* HAVE_GLSL */
}


#ifdef HAVE_GLSL

/* Set up the texture coordinates. */
static void setup_tex_coords(ModeInfo *mi)
{
  int   hemisphere, strip, j, k, l;
  float phi, theta, phi_scale, theta_scale, angle_strip;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  angle_strip = 2.0f*M_PI_F/NUM_STRIPS;
  phi_scale = 0.5f/M_PI_F;
  theta_scale = 1.0f/M_PI_F;

  for (hemisphere=0; hemisphere<2; hemisphere++)
  {
    for (strip=0; strip<NUM_STRIPS; strip++)
    {
      for (j=0; j<=NUM_U; j++)
      {
        if ((hemisphere & 1) == 0)
          theta = 0.5f*M_PI_F*(NUM_U-j)/NUM_U;
        else
          theta = -0.5f*M_PI_F*(NUM_U-j)/NUM_U;
        for (k=0; k<=NUM_V; k++)
        {
          if ((hemisphere & 1) == 0)
            phi = angle_strip*k/NUM_V;
          else
            phi = angle_strip*(NUM_V-k)/NUM_V;
          phi += strip*angle_strip;
          l = ((hemisphere*NUM_STRIPS+strip)*(NUM_U+1)+j)*(NUM_V+1)+k;
          se->tex[2*l+0] = 1.0f-(phi_scale*phi+0.5f);
          se->tex[2*l+1] = theta_scale*theta+0.5f;
        }
      }
    }
  }

  if (se->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_tex_coord_buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 4*2*NUM_STRIPS*(NUM_U+1)*(NUM_V+1)*sizeof(GLfloat),
                 se->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
  }
}

#endif /* HAVE_GLSL */


/* Draw the sphere eversion using OpenGL's fixed functionality. */
static int outside_in_ff(ModeInfo *mi)
{
  static const GLfloat light_ambient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  static const GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 0.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_gold[] = { 1.0f, 0.843f, 0.0f, 1.0f };
  static const GLfloat mat_diff_purple[] = { 0.5f, 0.0f, 0.5f, 1.0f };
  static const GLfloat mat_diff_gold_trans[] = { 1.0f, 0.843f, 0.0f, 0.7f };
  static const GLfloat mat_diff_purple_trans[] = { 0.5f, 0.0f, 0.5f, 0.7f };
  static const GLfloat mat_diff_front[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  int i, j, k, l, m, hemisphere, strip;
  float angle;
  float *cf, *cb;
  float qu[4], qr[16];
  int polys = 0;

  generate_geometry(se->sp,se->sn,se->time,NUM_STRIPS,0.0f,NUM_U,1.0f,
                    0.0f,NUM_V,1.0f);

  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (se->projection == DISP_PERSPECTIVE)
  {
    gluPerspective(60.0f,se->aspect,-se->offset3d[2]-2.0f,
                   -se->offset3d[2]+2.0f);
  }
  else
  {
    if (se->aspect >= 1.0f)
      glOrtho(-1.8f*se->aspect,1.8f*se->aspect,-1.8f,1.8f,
              -se->offset3d[2]-2.0f,-se->offset3d[2]+2.0f);
    else
      glOrtho(-1.8f,1.8f,-1.8f/se->aspect,1.8f/se->aspect,
              -se->offset3d[2]-2.0f,-se->offset3d[2]+2.0f);
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glShadeModel(GL_SMOOTH);
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
  glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
  glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
  glLightfv(GL_LIGHT0,GL_POSITION,light_position);
  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
  glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);
  if (se->display_mode[0] == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else
  {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }

  if (se->colors[0] == COLORS_TWOSIDED || se->colors[0] == COLORS_EARTH)
  {
    glColor3fv(mat_diff_front);
    if (se->display_mode[0] == DISP_SURFACE)
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_gold);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_purple);
    }
    else
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_gold_trans);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_purple_trans);
    }
  }

  gltrackball_get_quaternion(se->trackball,qu);
  quat_to_rotmat(qu,qr);
  glTranslatef(0.0f,0.0f,-3.2f);
  glMultMatrixf(qr);
  glRotatef(se->alpha,1.0f,0.0f,0.0f);
  glRotatef(se->beta,0.0f,1.0f,0.0f);
  glRotatef(se->delta,0.0f,0.0f,1.0f);

  for (hemisphere=0; hemisphere<se->num_hemispheres; hemisphere++)
  {
    glPushMatrix();
    glRotatef(hemisphere*180.0f,0.0f,1.0f,0.0f);
    for (strip=0; strip<NUM_STRIPS; strip+=se->strip_step)
    {
      angle = (hemisphere == 0 ? -strip : strip+1)*360.0f/NUM_STRIPS;
      glPushMatrix();
      glRotatef(angle,0.0f,0.0f,1.0f);

      if (se->appearance[0] == APPEARANCE_SOLID)
      {
        for (k=0; k<NUM_V; k++)
        {
          glBegin(GL_TRIANGLE_STRIP);
          for (j=0; j<=NUM_U; j++)
          {
            for (i=0; i<=1; i++)
            {
              l = j*(NUM_V+1)+k+i;
              if (se->colors[0] != COLORS_TWOSIDED &&
                  se->colors[0] != COLORS_EARTH)
              {
                m = (hemisphere*NUM_STRIPS+strip)*(NUM_U+1)*(NUM_V+1)+l;
                cf = &se->colf[0][4*m];
                cb = &se->colb[0][4*m];
                glColor3fv(cf);
                glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
                glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
              }
              glNormal3fv(&se->sn[3*l]);
              glVertex3fv(&se->sp[3*l]);
            }
          }
          glEnd();
        }
      }
      else if (se->appearance[0] == APPEARANCE_MERIDIAN_BANDS)
      {
        for (k=0; k<NUM_V; k++)
        {
          if (se->appearance[0] == APPEARANCE_MERIDIAN_BANDS &&
              ((k & (NUMB_MER-1)) >= NUMB_MER/4) &&
              ((k & (NUMB_MER-1)) < 3*NUMB_MER/4))
            continue;
          glBegin(GL_TRIANGLE_STRIP);
          for (j=0; j<=NUM_U; j++)
          {
            for (i=0; i<=1; i++)
            {
              l = j*(NUM_V+1)+k+i;
              if (se->colors[0] != COLORS_TWOSIDED &&
                  se->colors[0] != COLORS_EARTH)
              {
                m = (hemisphere*NUM_STRIPS+strip)*(NUM_U+1)*(NUM_V+1)+l;
                cf = &se->colf[0][4*m];
                cb = &se->colb[0][4*m];
                glColor3fv(cf);
                glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
                glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
              }
              glNormal3fv(&se->sn[3*l]);
              glVertex3fv(&se->sp[3*l]);
            }
          }
          glEnd();
        }
      }
      else if (se->appearance[0] == APPEARANCE_PARALLEL_BANDS)
      {
        for (j=0; j<NUM_U; j++)
        {
          if (((j & (NUMB_PAR-1)) >= NUMB_PAR/4) &&
              ((j & (NUMB_PAR-1)) < 3*NUMB_PAR/4))
            continue;
          glBegin(GL_TRIANGLE_STRIP);
          for (k=NUM_V; k>=0; k--)
          {
            for (i=0; i<=1; i++)
            {
              l = (j+i)*(NUM_V+1)+k;
              if (se->colors[0] != COLORS_TWOSIDED &&
                  se->colors[0] != COLORS_EARTH)
              {
                m = (hemisphere*NUM_STRIPS+strip)*(NUM_U+1)*(NUM_V+1)+l;
                cf = &se->colf[0][4*m];
                cb = &se->colb[0][4*m];
                glColor3fv(cf);
                glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
                glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
              }
              glNormal3fv(&se->sn[3*l]);
              glVertex3fv(&se->sp[3*l]);
            }
          }
          glEnd();
        }
      }
      glPopMatrix();
    }
    glPopMatrix();
  }

  if (se->appearance[0] == APPEARANCE_SOLID)
    polys = 2*se->num_hemispheres*8/se->strip_step*(NUM_U+1)*NUM_V;
  else if (se->appearance[0] == APPEARANCE_MERIDIAN_BANDS)
    polys = se->num_hemispheres*8/se->strip_step*(NUM_U+1)*NUM_V;
  else if (se->appearance[0] == APPEARANCE_PARALLEL_BANDS)
    polys = se->num_hemispheres*8/se->strip_step*NUM_U*(NUM_V+1);

  return polys;
}


#ifdef HAVE_GLSL

/* Draw the sphere eversion using OpenGL's programmable functionality. */
static int outside_in_pf(ModeInfo *mi)
{
  static const GLfloat light_model_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_ambient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  static const GLfloat light_ambient_earth[]  = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 0.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_gold[] = { 1.0f, 0.843f, 0.0f, 1.0f };
  static const GLfloat mat_diff_purple[] = { 0.5f, 0.0f, 0.5f, 1.0f };
  static const GLfloat mat_diff_gold_trans[] = { 1.0f, 0.843f, 0.0f, 0.7f };
  static const GLfloat mat_diff_purple_trans[] = { 0.5f, 0.0f, 0.5f, 0.7f };
  static const GLfloat mat_diff_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_white_trans[] = { 1.0f, 1.0f, 1.0f, 0.7f };
  static const GLuint blend_indices[6] = { 0, 1, 2, 2, 3, 0 };
  static const GLfloat blend_p[4][2] =
    { { -1.0, -1.0 }, { 1.0, -1.0 }, { 1.0, 1.0 }, { -1.0, 1.0 } };
  static const GLfloat blend_t[4][2] =
    { { 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 }, { 0.0, 1.0 } };
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  int i, j, k, l, hemisphere, strip, pass, num_passes, ni;
  float angle, t;
  float qu[4], qr[16];
  GLfloat light_direction[3], half_vector[3], len;
  GLfloat p_mat[16], mv_mat[16], mv_mat1[16], mv_mat2[16];
  GLint draw_buf, read_buf;
  GLsizeiptr index_offset;
  int polys = 0;

  if (!se->use_shaders)
    return 0;

  if (!se->buffers_initialized)
  {
    /* The indices only need to be computed once. */
    /* Compute the solid indices. */
    ni = 0;
    se->num_solid_strips = 0;
    se->num_solid_triangles = 0;
    for (j=0; j<NUM_U; j++)
    {
      for (k=NUM_V; k>=0; k--)
      {
        for (i=0; i<=1; i++)
        {
          l = (j+i)*(NUM_V+1)+k;
          se->solid_indices[ni++] = l;
        }
      }
      se->num_solid_strips++;
    }
    se->num_solid_triangles = 2*(NUM_V+1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->solid_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->solid_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    /* Compute the parallel indices. */
    ni = 0;
    se->num_parallel_strips = 0;
    se->num_parallel_triangles = 0;
    for (j=0; j<NUM_U; j++)
    {
      if (((j & (NUMB_PAR-1)) >= NUMB_PAR/4) &&
          ((j & (NUMB_PAR-1)) < 3*NUMB_PAR/4))
        continue;
      for (k=NUM_V; k>=0; k--)
      {
        for (i=0; i<=1; i++)
        {
          l = (j+i)*(NUM_V+1)+k;
          se->parallel_indices[ni++] = l;
        }
      }
      se->num_parallel_strips++;
    }
    se->num_parallel_triangles = 2*(NUM_V+1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->parallel_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->parallel_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    /* Compute the meridian indices. */
    ni = 0;
    se->num_meridian_strips = 0;
    se->num_meridian_triangles = 0;
    for (k=0; k<NUM_V; k++)
    {
      if (((k & (NUMB_MER-1)) >= NUMB_MER/4) &&
          ((k & (NUMB_MER-1)) < 3*NUMB_MER/4))
        continue;
      for (j=0; j<=NUM_U; j++)
      {
        for (i=0; i<=1; i++)
        {
          l = j*(NUM_V+1)+k+i;
          se->meridian_indices[ni++] = l;
        }
      }
      se->num_meridian_strips++;
    }
    se->num_meridian_triangles = 2*(NUM_U+1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->meridian_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->meridian_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    se->buffers_initialized = GL_TRUE;
  }

  generate_geometry(se->sp,se->sn,se->time,NUM_STRIPS,0.0f,NUM_U,1.0f,
                    0.0f,NUM_V,1.0f);

  glBindBuffer(GL_ARRAY_BUFFER,se->vertex_pos_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*(NUM_U+1)*(NUM_V+1)*sizeof(GLfloat),
               se->sp,GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,se->vertex_normal_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*(NUM_U+1)*(NUM_V+1)*sizeof(GLfloat),
               se->sn,GL_STREAM_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  glsl_Identity(p_mat);
  if (se->projection == DISP_PERSPECTIVE)
  {
    glsl_Perspective(p_mat,60.0f,se->aspect,-se->offset3d[2]-2.0f,
                     -se->offset3d[2]+2.0f);
  }
  else
  {
    if (se->aspect >= 1.0f)
      glsl_Orthographic(p_mat,-1.8f*se->aspect,1.8f*se->aspect,-1.8f,1.8f,
                        -se->offset3d[2]-2.0f,-se->offset3d[2]+2.0f);
    else
      glsl_Orthographic(p_mat,-1.8f,1.8f,-1.8f/se->aspect,1.8f/se->aspect,
                        -se->offset3d[2]-2.0f,-se->offset3d[2]+2.0f);
  }

  gltrackball_get_quaternion(se->trackball,qu);
  quat_to_rotmat(qu,qr);
  glsl_Identity(mv_mat);
  glsl_Translate(mv_mat,0.0f,0.0f,-3.2f);
  glsl_MultMatrix(mv_mat,qr);
  glsl_Rotate(mv_mat,se->alpha,1.0f,0.0f,0.0f);
  glsl_Rotate(mv_mat,se->beta,0.0f,1.0f,0.0f);
  glsl_Rotate(mv_mat,se->delta,0.0f,0.0f,1.0f);

  len = sqrtf(light_position[0]*light_position[0]+
              light_position[1]*light_position[1]+
              light_position[2]*light_position[2]);
  light_direction[0] = light_position[0]/len;
  light_direction[1] = light_position[1]/len;
  light_direction[2] = light_position[2]/len;
  half_vector[0] = light_direction[0];
  half_vector[1] = light_direction[1];
  half_vector[2] = light_direction[2]+1.0f;
  len = sqrtf(half_vector[0]*half_vector[0]+
              half_vector[1]*half_vector[1]+
              half_vector[2]*half_vector[2]);
  half_vector[0] /= len;
  half_vector[1] /= len;
  half_vector[2] /= len;

  num_passes = se->anim_state == ANIM_TURN ? 2 : 1;

  for (pass=0; pass<num_passes; pass++)
  {
    glUseProgram(se->poly_shader_program);

    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glUniformMatrix4fv(se->poly_proj_index,1,GL_FALSE,p_mat);

    glUniform4fv(se->poly_front_ambient_index,1,mat_diff_white);
    glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_white);
    glUniform4fv(se->poly_back_ambient_index,1,mat_diff_white);
    glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_white);
    glVertexAttrib4f(se->poly_colorf_index,1.0f,1.0f,1.0f,1.0f);
    glVertexAttrib4f(se->poly_colorb_index,1.0f,1.0f,1.0f,1.0f);

    glUniform4fv(se->poly_front_ambient_index,1,mat_diff_white);
    glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_white);
    glUniform4fv(se->poly_back_ambient_index,1,mat_diff_white);
    glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_white);
    glVertexAttrib4f(se->poly_colorf_index,1.0f,1.0f,1.0f,1.0f);
    glVertexAttrib4f(se->poly_colorb_index,1.0f,1.0f,1.0f,1.0f);

    glUniform4fv(se->poly_glbl_ambient_index,1,light_model_ambient);
    if (se->colors[pass] != COLORS_EARTH)
      glUniform4fv(se->poly_lt_ambient_index,1,light_ambient);
    else
      glUniform4fv(se->poly_lt_ambient_index,1,light_ambient_earth);
    glUniform4fv(se->poly_lt_diffuse_index,1,light_diffuse);
    glUniform4fv(se->poly_lt_specular_index,1,light_specular);
    glUniform3fv(se->poly_lt_direction_index,1,light_direction);
    glUniform3fv(se->poly_lt_halfvect_index,1,half_vector);
    glUniform4fv(se->poly_specular_index,1,mat_specular);
    glUniform1f(se->poly_shininess_index,50.0f);

    if (se->display_mode[pass] == DISP_SURFACE)
    {
      glDisable(GL_CULL_FACE);
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
    }
    else
    {
      glDisable(GL_CULL_FACE);
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    }

    if (se->colors[pass] == COLORS_TWOSIDED)
    {
      if (se->display_mode[pass] == DISP_SURFACE)
      {
        glUniform4fv(se->poly_front_ambient_index,1,mat_diff_gold);
        glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_gold);
        glUniform4fv(se->poly_back_ambient_index,1,mat_diff_purple);
        glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_purple);
      }
      else
      {
        glUniform4fv(se->poly_front_ambient_index,1,mat_diff_gold_trans);
        glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_gold_trans);
        glUniform4fv(se->poly_back_ambient_index,1,mat_diff_purple_trans);
        glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_purple_trans);
      }
    }
    else if (se->colors[pass] == COLORS_EARTH)
    {
      if (se->display_mode[pass] == DISP_TRANSPARENT)
      {
        glUniform4fv(se->poly_front_ambient_index,1,mat_diff_white_trans);
        glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_white_trans);
        glUniform4fv(se->poly_back_ambient_index,1,mat_diff_white_trans);
        glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_white_trans);
      }
      else
      {
        glUniform4fv(se->poly_front_ambient_index,1,mat_diff_white);
        glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_white);
        glUniform4fv(se->poly_back_ambient_index,1,mat_diff_white);
        glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_white);
      }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,se->tex_names[0]);
    glUniform1i(se->poly_tex_samp_f_index,0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,se->tex_names[1]);
    glUniform1i(se->poly_tex_samp_b_index,1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,se->tex_names[2]);
    glUniform1i(se->poly_tex_samp_w_index,2);
    glUniform1i(se->poly_bool_textures_index,se->colors[pass] == COLORS_EARTH);

    glEnableVertexAttribArray(se->poly_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_pos_buffer);
    glVertexAttribPointer(se->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(se->poly_normal_index);
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_normal_buffer);
    glVertexAttribPointer(se->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

    for (hemisphere=0; hemisphere<se->num_hemispheres; hemisphere++)
    {
      glsl_CopyMatrix(mv_mat1,mv_mat);
      glsl_Rotate(mv_mat1,hemisphere*180.0f,0.0f,1.0f,0.0f);
      for (strip=0; strip<NUM_STRIPS; strip+=se->strip_step)
      {
        angle = (hemisphere == 0 ? -strip : strip+1)*360.0f/NUM_STRIPS;
        glsl_CopyMatrix(mv_mat2,mv_mat1);
        glsl_Rotate(mv_mat2,angle,0.0f,0.0f,1.0f);
        glUniformMatrix4fv(se->poly_mv_index,1,GL_FALSE,mv_mat2);

        index_offset = (2*(hemisphere*NUM_STRIPS+strip)*(NUM_U+1)*(NUM_V+1)*
                        sizeof(GLfloat));

        glEnableVertexAttribArray(se->poly_vertex_tex_index);
        glBindBuffer(GL_ARRAY_BUFFER,se->vertex_tex_coord_buffer);
        glVertexAttribPointer(se->poly_vertex_tex_index,2,GL_FLOAT,GL_FALSE,0,
                              (const GLvoid *)index_offset);

        if (se->colors[pass] != COLORS_TWOSIDED &&
            se->colors[pass] != COLORS_EARTH)
        {
          index_offset = (4*(hemisphere*NUM_STRIPS+strip)*(NUM_U+1)*(NUM_V+1)*
                          sizeof(GLfloat));

          glEnableVertexAttribArray(se->poly_colorf_index);
          glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorf_buffer[pass]);
          glVertexAttribPointer(se->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,
                                (const GLvoid *)index_offset);

          glEnableVertexAttribArray(se->poly_colorb_index);
          glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorb_buffer[pass]);
          glVertexAttribPointer(se->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,
                                (const GLvoid *)index_offset);
        }
#ifdef VERTEXATTRIBARRAY_WORKAROUND
        else /* se->colors[0] == COLORS_TWOSIDED ||
                se->colors[pass] == COLORS_EARTH */
        {
          index_offset = (4*(hemisphere*NUM_STRIPS+strip)*(NUM_U+1)*(NUM_V+1)*
                          sizeof(GLfloat));

          glEnableVertexAttribArray(se->poly_colorf_index);
          glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorf_buffer[pass]);
          glVertexAttribPointer(se->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,
                                (const GLvoid *)index_offset);

          glEnableVertexAttribArray(se->poly_colorb_index);
          glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorb_buffer[pass]);
          glVertexAttribPointer(se->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,
                                (const GLvoid *)index_offset);
        }
#endif /* VERTEXATTRIBARRAY_WORKAROUND */

        if (se->appearance[pass] == APPEARANCE_SOLID)
        {
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->solid_indices_buffer);
          for (i=0; i<se->num_solid_strips; i++)
          {
            index_offset = se->num_solid_triangles*i*sizeof(GLuint);
            glDrawElements(GL_TRIANGLE_STRIP,se->num_solid_triangles,
                           GL_UNSIGNED_INT,(const GLvoid *)index_offset);
          }
        }
        else if (se->appearance[pass] == APPEARANCE_PARALLEL_BANDS)
        {
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->parallel_indices_buffer);
          for (i=0; i<se->num_parallel_strips; i++)
          {
            index_offset = se->num_parallel_triangles*i*sizeof(GLuint);
            glDrawElements(GL_TRIANGLE_STRIP,se->num_parallel_triangles,
                           GL_UNSIGNED_INT,(const GLvoid *)index_offset);
          }
        }
        else if (se->appearance[pass] == APPEARANCE_MERIDIAN_BANDS)
        {
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->meridian_indices_buffer);
          for (i=0; i<se->num_meridian_strips; i++)
          {
            index_offset = se->num_meridian_triangles*i*sizeof(GLuint);
            glDrawElements(GL_TRIANGLE_STRIP,se->num_meridian_triangles,
                           GL_UNSIGNED_INT,(const GLvoid *)index_offset);
          }
        }
      }
    }

    if (se->appearance[pass] == APPEARANCE_SOLID)
      polys += 2*se->num_hemispheres*8/se->strip_step*(NUM_U+1)*NUM_V;
    else if (se->appearance[pass] == APPEARANCE_MERIDIAN_BANDS)
      polys += se->num_hemispheres*8/se->strip_step*(NUM_U+1)*NUM_V;
    else if (se->appearance[pass] == APPEARANCE_PARALLEL_BANDS)
      polys += se->num_hemispheres*8/se->strip_step*NUM_U*(NUM_V+1);

    glDisableVertexAttribArray(se->poly_pos_index);
    glDisableVertexAttribArray(se->poly_normal_index);
    if (se->colors[pass] != COLORS_TWOSIDED &&
        se->colors[pass] != COLORS_EARTH)
    {
      glDisableVertexAttribArray(se->poly_colorf_index);
      glDisableVertexAttribArray(se->poly_colorb_index);
    }
#ifdef VERTEXATTRIBARRAY_WORKAROUND
    else /* !colored */
    {
      glDisableVertexAttribArray(se->poly_colorf_index);
      glDisableVertexAttribArray(se->poly_colorb_index);
    }
#endif /* VERTEXATTRIBARRAY_WORKAROUND */
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    glUseProgram(0);

    if (num_passes == 2)
    {
      /* Copy the rendered image to a texture. */
      glGetIntegerv(GL_DRAW_BUFFER0,&draw_buf);
      glGetIntegerv(GL_READ_BUFFER,&read_buf);
      glReadBuffer(draw_buf);
      glBindTexture(GL_TEXTURE_2D,se->color_textures[pass]);
      glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,se->WindW,se->WindH);
      glReadBuffer(read_buf);
    }
  }

  if (num_passes == 2)
  {
    t = (float)se->turn_step/(float)se->num_turn;
    /* Apply an easing function to t. */
    t = (3.0-2.0*t)*t*t;

    glUseProgram(se->blend_shader_program);

    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glUniform1f(se->blend_t_index,t);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,se->color_textures[0]);
    glUniform1i(se->blend_sampler0_index,0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,se->color_textures[1]);
    glUniform1i(se->blend_sampler1_index,1);

    glEnableVertexAttribArray(se->blend_vertex_p_index);
    glVertexAttribPointer(se->blend_vertex_p_index,2,GL_FLOAT,GL_FALSE,
                          2*sizeof(GLfloat),blend_p);

    glEnableVertexAttribArray(se->blend_vertex_t_index);
    glVertexAttribPointer(se->blend_vertex_t_index,2,GL_FLOAT,GL_FALSE,
                          2*sizeof(GLfloat),blend_t);

    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,blend_indices);

    glActiveTexture(GL_TEXTURE0);

    glUseProgram(0);
  }

  return polys;
}

#endif /* HAVE_GLSL */



void init_sphereeversion_corrugations(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (deform_speed == 0.0f)
    deform_speed = 10.0f;

  se->alpha = frand(120.0)-60.0;
  se->beta = frand(120.0)-60.0;
  se->delta = frand(360.0);

  se->anim_state = ANIM_DEFORM;
  se->time = 0.0;
  se->defdir = 1;
  se->turn_step = 0;
  se->num_turn = 0;

  se->offset3d[0] = 0.0f;
  se->offset3d[1] = 0.0f;
  se->offset3d[2] = -3.2f;

  se->sp = calloc(3*(NUM_U+1)*(NUM_V+1),sizeof(float));
  se->sn = calloc(3*(NUM_U+1)*(NUM_V+1),sizeof(float));
  se->colf[0] = calloc(4*2*NUM_STRIPS*(NUM_U+1)*(NUM_U+1),sizeof(float));
  se->colf[1] = calloc(4*2*NUM_STRIPS*(NUM_U+1)*(NUM_U+1),sizeof(float));
  se->colb[0] = calloc(4*2*NUM_STRIPS*(NUM_U+1)*(NUM_U+1),sizeof(float));
  se->colb[1] = calloc(4*2*NUM_STRIPS*(NUM_U+1)*(NUM_U+1),sizeof(float));

#ifdef HAVE_GLSL
  se->solid_indices = calloc(2*NUM_U*(NUM_V+1),sizeof(float));
  se->parallel_indices = calloc(NUM_U*(NUM_V+1),sizeof(float));
  se->meridian_indices = calloc((NUM_U+1)*NUM_V,sizeof(float));
  se->line_indices = NULL;

  init_glsl(mi);
#endif /* HAVE_GLSL */

  setup_surface_colors(mi);
#ifdef HAVE_GLSL
  se->tex = calloc(4*2*NUM_STRIPS*(NUM_U+1)*(NUM_V+1),sizeof(float));
  gen_textures(mi);
  setup_tex_coords(mi);
#endif /* HAVE_GLSL */
}


void display_sphereeversion_corrugations(ModeInfo *mi)
{
  float alpha, beta, delta, dot, a, t, q[4];
  float *colt;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
#ifdef HAVE_GLSL
  GLuint vertex_colort_buffer;
#endif /* HAVE_GLSL */

  if (!se->button_pressed)
  {
    if (se->anim_state == ANIM_DEFORM)
    {
      se->time += se->defdir*deform_speed*0.0001;
      if (se->time < 0.0)
      {
        se->time = 0.0;
        se->defdir = -se->defdir;
        se->anim_state = ANIM_TURN;
      }
      if (se->time > 1.0)
      {
        se->time = 1.0;
        se->defdir = -se->defdir;
        se->anim_state = ANIM_TURN;
      }
      if (se->anim_state == ANIM_TURN)
      {
        /* Convert the current rotation angles to a quaternion. */
        angles_to_quat(se->alpha,se->beta,se->delta,se->qs);
        /* Determine random target rotation angles and  convert them to the
           target quaternion. */
        alpha = frand(120.0)-60.0;
        beta = frand(120.0)-60.0;
        delta = frand(360.0);
        angles_to_quat(alpha,beta,delta,se->qe);
        /* Compute the angle between qs and qe. */
        dot = (se->qs[0]*se->qe[0]+se->qs[1]*se->qe[1]+
               se->qs[2]*se->qe[2]+se->qs[3]*se->qe[3]);
        if (dot < 0.0f)
        {
          se->qe[0] = -se->qe[0];
          se->qe[1] = -se->qe[1];
          se->qe[2] = -se->qe[2];
          se->qe[3] = -se->qe[3];
          dot = -dot;
        }
        a = 180.0f/M_PI_F*acosf(dot);
        se->num_turn = ceilf(a/TURN_STEP);

        /* Change the parameters randomly after one full eversion when a
           turn to the new orientation starts. */
        /* Copy the current display modes to the values of the second pass. */
        se->display_mode[1] = se->display_mode[0];
        se->appearance[1] = se->appearance[0];
        se->colors[1] = se->colors[0];
        /* Swap the front and back color buffers. */
        colt = se->colf[1];
        se->colf[1] = se->colf[0];
        se->colf[0] = colt;
        colt = se->colb[1];
        se->colb[1] = se->colb[0];
        se->colb[0] = colt;
#ifdef HAVE_GLSL
        /* Swap the OpenGL front and back color buffers. */
        if (se->use_shaders)
        {
          vertex_colort_buffer = se->vertex_colorf_buffer[1];
          se->vertex_colorf_buffer[1] = se->vertex_colorf_buffer[0];
          se->vertex_colorf_buffer[0] = vertex_colort_buffer;
          vertex_colort_buffer = se->vertex_colorb_buffer[1];
          se->vertex_colorb_buffer[1] = se->vertex_colorb_buffer[0];
          se->vertex_colorb_buffer[0] = vertex_colort_buffer;
        }
#endif /* HAVE_GLSL */
        /* Randomly select new display modes for the main pass. */
        if (se->random_display_mode)
          se->display_mode[0] = random() % NUM_DISPLAY_MODES;
        if (se->random_appearance)
          se->appearance[0] = random() % NUM_APPEARANCES;
        if (se->random_colors)
          se->colors[0] = random() % NUM_COLORS;
        setup_surface_colors(mi);
      }
    }
    else /* se->anim_state == ANIM_TURN */
    {
      t = (float)se->turn_step/(float)se->num_turn;
      /* Apply an easing function to t. */
      t = (3.0-2.0*t)*t*t;
      quat_slerp(t,se->qs,se->qe,q);
      quat_to_angles(q,&se->alpha,&se->beta,&se->delta);
      se->turn_step++;
      if (se->turn_step > se->num_turn)
      {
        se->turn_step = 0;
        se->anim_state = ANIM_DEFORM;
      }
    }

    if (se->anim_state == ANIM_DEFORM)
    {
      se->alpha += speed_x*se->speed_scale;
      if (se->alpha >= 360.0f)
        se->alpha -= 360.0f;
      se->beta += speed_y*se->speed_scale;
      if (se->beta >= 360.0f)
        se->beta -= 360.0f;
      se->delta += speed_z*se->speed_scale;
      if (se->delta >= 360.0f)
        se->delta -= 360.0f;
    }
  }

  gltrackball_rotate(se->trackball);
#ifdef HAVE_GLSL
  if (se->use_shaders)
    mi->polygon_count = outside_in_pf(mi);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = outside_in_ff(mi);
}


#endif /* USE_GL */
