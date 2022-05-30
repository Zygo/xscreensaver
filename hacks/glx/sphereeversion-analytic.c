/* sphereeversion-analytic --- Shows a sphere eversion, i.e., a smooth
   deformation (homotopy) that turns a sphere inside out.  During the
   eversion, the deformed sphere is allowed to intersect itself
   transversally.  However, no creases or pinch points are allowed to
   occur. */

/* Copyright (c) 2020-2022 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 22/02/25: Moved the analytic sphere eversion code to this file
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
 * It is also possible to display a graticule (i.e., a coordinate grid
 * consisting of parallel and meridian lines) on top of the surface.
 * The graticule mode can be set to on, off, or random.  If random
 * mode is selected, the graticule mode is changed each time an
 * eversion has been completed.
 *
 * It is possible to define a surface order of the sphere eversion as
 * random or as a value between 2 and 5.  This determines the the
 * complexity of the deformation.  For higher surface orders, some
 * z-fighting might occur around the central stage of the eversion,
 * which might lead to some irregular flickering of the displayed
 * surface if it is displayed as a solid object.  For odd surface
 * orders, z-fighting will occur very close to the central stage of
 * the eversion since the deformed sphere is a doubly covered Boy
 * surface (for surface order 3) or a doubly covered generalized Boy
 * surface (for surface order 5) in this case.  If you find this
 * distracting, you should set the surface order to 2.  If a random
 * surface order is selected, the surface order is changed each time
 * an eversion has been completed.
 *
 * The colors with with the sphere is drawn can be set to two-sided,
 * parallel, meridian, or random.  In two-sided mode, the sphere is
 * drawn with red on one side and green on the other side.  In
 * parallel mode, the sphere is displayed with colors that run from
 * red to cyan on one side of the surface and from green to violet on
 * the other side.  The colors are aligned with the parallels of the
 * sphere in this mode.  In meridian mode, the the sphere is displayed
 * with colors that run from red to white to cyan to black and back to
 * red on one side of the surface and from green to white to violet to
 * black and back to green on the other side.  The colors are aligned
 * with the meridians of the sphere in this mode.  In earth mode, the
 * sphere is drawn with a texture of earth by day on one side and with
 * a texture of earth by night on the other side.  Initially, the
 * earth by day is on the outside and the earth by night on the
 * inside.  After the first eversion, the earth by night will be on
 * the outside.  All points of the earth on the inside and outside are
 * at the same positions on the sphere.  Since an eversion transforms
 * the sphere into its inverse, the earth by night will appear with
 * all continents mirror reversed.  If random mode is selected, the
 * color scheme is changed each time an eversion has been completed.
 *
 * By default, the sphere is rotated to a new viewing position each
 * time an eversion has been completed.  In addition, it is possible
 * to rotate the sphere while it is deforming.  The rotation speed for
 * each of the three coordinate axes around which the sphere rotates
 * can be chosen arbitrarily.  For best effects, however, it is
 * suggested to rotate only around the z axis while the sphere is
 * deforming.
 *
 * This program is inspired by the following paper: Adam Bednorz,
 * Witold Bednorz: "Analytic sphere eversion using ruled surfaces",
 * Differential Geometry and its Applications 64:59-79, 2019.
 */


#ifdef STANDALONE
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */


#ifdef USE_GL

#include "sphereeversion.h"

#include <float.h>


/* Shape parameters for the Bednorz sphere eversion. */
#define BEDNORZ_OMEGA 2.0
#define BEDNORZ_Q (2.0/3.0)
#define BEDNORZ_ETA_MIN (3.0/4.0)
#define BEDNORZ_BETA_MAX 0.1
#define BEDNORZ_ALPHA 1.0
#define BEDNORZ_EPS2 0.001
#define BEDNORZ_EPS3 0.002
#define BEDNORZ_EPS4 0.001
#define BEDNORZ_EPS5 0.002

#define BEDNORZ_TAU1 (1.0/BEDNORZ_Q)
#define BEDNORZ_TAU2 2.5
#define BEDNORZ_TAU3 4.5
#define BEDNORZ_TAU4 6.0

#define BEDNORZ_TAU_MIN (-BEDNORZ_TAU4)
#define BEDNORZ_TAU_MAX (BEDNORZ_TAU4)

/* Number of subdivisions of the surface */
#define NUMTH 256
#define NUMPH 256

/* Number of subdivisions between grid lines */
#define NUMGRID 32

/* Number of subdivisions per band */
#define NUMBDIR  16
#define NUMBDIST 16

typedef struct {
  int n;
  double kappa;
  double omega;
  double t;
  double p;
  double q;
  double xi;
  double eta;
  double alpha;
  double beta;
  double gamma;
  double lambda;
  double eps;
} bednorz_shape_par;



/* Compute a 3D rotation matrix from an xscreensaver unit quaternion. Note
   that xscreensaver has a different convention for unit quaternions than
   the one that is used in this hack. */
static void quat_to_rotmat(float p[4], float m[3][3])
{
  float al, be, de;
  float r00, r01, r02, r12, r22;

  r00 = 1.0f-2.0f*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0f*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0f*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0f*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0f-2.0f*(p[1]*p[1]+p[0]*p[0]);

  al = atan2f(-r12,r22)*180.0f/M_PI_F;
  be = atan2f(r02,sqrtf(r00*r00+r01*r01))*180.0f/M_PI_F;
  de = atan2f(-r01,r00)*180.0f/M_PI_F;
  rotateall(al,be,de,m);
}


/* Compute the cross product between the vectors a and b. */
static inline void cross(float a[3], float b[3], float c[3])
{
  c[0] = a[1]*b[2]-a[2]*b[1];
  c[1] = a[2]*b[0]-a[0]*b[2];
  c[2] = a[0]*b[1]-a[1]*b[0];
}


/* Compute x^n for integers 0 <= n <= 11 efficiently. */
static inline double ipow(double x, int n)
{
  double x2, x4, x8;

  switch (n)
  {
    case 0:
      return 1.0;
    case 1:
      return x;
    case 2:
      x2 = x*x;
      return x2;
    case 3:
      x2 = x*x;
      return x2*x;
    case 4:
      x2 = x*x;
      x4 = x2*x2;
      return x4;
    case 5:
      x2 = x*x;
      x4 = x2*x2;
      return x4*x;
    case 6:
      x2 = x*x;
      x4 = x2*x2;
      return x4*x2;
    case 7:
      x2 = x*x;
      x4 = x2*x2;
      return x4*x2*x;
    case 8:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8;
    case 9:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8*x;
    case 10:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8*x2;
    case 11:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8*x2*x;
    default:
      return pow(x,n);
  }
}


/* Compute the Bednorz shape parameter kappa based on the eversion order n. */
static inline double bednorz_get_kappa(int n)
{
  return (n-1.0)/(2.0*n);
}


/* Compute the Bednorz shape parameter t based on the deformation
   parameter tau. */
static inline double bednorz_get_t(double tau)
{
  return (tau >= BEDNORZ_TAU1 ?
          BEDNORZ_TAU1 :
          (tau <= -BEDNORZ_TAU1 ?
           -BEDNORZ_TAU1 :
           tau));
}


/* Compute the Bednorz shape parameter q based on the deformation
   parameter tau. */
static inline double bednorz_get_q(double tau)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU1 ?
          0.0 :
          (abs_tau < BEDNORZ_TAU2 ?
           BEDNORZ_Q*(abs_tau-BEDNORZ_TAU1)/(BEDNORZ_TAU2-BEDNORZ_TAU1) :
           BEDNORZ_Q));
}


/* Compute the Bednorz shape parameter p based on the deformation
   parameter tau. */
static inline double bednorz_get_p(double tau)
{
  return 1.0-fabs(bednorz_get_q(tau)*bednorz_get_t(tau));
}


/* Compute the Bednorz shape parameter xi based on the deformation
   parameter tau. */
static inline double bednorz_get_xi(double tau)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU2 ?
          1.0 :
          (abs_tau < BEDNORZ_TAU3 ?
           (BEDNORZ_TAU3-abs_tau)/(BEDNORZ_TAU3-BEDNORZ_TAU2) :
           0.0));
}


/* Compute the Bednorz shape parameter eta based on the deformation
   parameter tau and the shape parameter eta_min. */
static inline double bednorz_get_eta(double tau, double eta_min)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU2 ?
          eta_min :
          (abs_tau < BEDNORZ_TAU3 ?
           (eta_min+(1.0-eta_min)*
            (abs_tau-BEDNORZ_TAU2)/(BEDNORZ_TAU3-BEDNORZ_TAU2)) :
           1.0));
}


/* Compute the Bednorz shape parameter alpha based on the deformation
   parameter tau. */
static inline double bednorz_get_alpha(double tau)
{
  double xi;

  xi = bednorz_get_xi(tau);
  return BEDNORZ_ALPHA*ipow(xi,2);
}


/* Compute the Bednorz shape parameter beta based on the deformation
   parameter tau and the shape parameter beta_max. */
static inline double bednorz_get_beta(double tau, double beta_max)
{
  double xi;

  xi = bednorz_get_xi(tau);
  return ipow(1.0-xi,2)+beta_max*ipow(xi,3);
}


/* Compute the Bednorz shape parameter gamma based on the shape
   parameters alpha and beta. */
static inline double bednorz_get_gamma(double alpha, double beta)
{
  return 2.0*sqrt(alpha*beta);
}


/* Compute the Bednorz shape parameter lambda based on the deformation
   parameter tau. */
static inline double bednorz_get_lambda(double tau)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU3 ?
          1.0 :
          (abs_tau < BEDNORZ_TAU4 ?
           (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3) :
           0.0));
}


/* Compute the Bednorz shape parameter eps based on the deformation
   parameter tau and the eversion order n.  This is an extension to the
   original approach that prevents z fighting to some extent in certain
   stages of the eversion. */
static inline double bednorz_get_eps(double tau, int n)
{
  double sgn_tau, abs_tau;

  sgn_tau = (tau < 0.0 ? -1.0 : (tau > 0.0 ? 1.0 : 0.0));
  abs_tau = fabs(tau);
  switch (n)
  {
    case 2:
      return (abs_tau < BEDNORZ_TAU1 ?
              0.0 :
              (abs_tau < BEDNORZ_TAU2 ?
               (BEDNORZ_EPS2*sgn_tau*
                (abs_tau-BEDNORZ_TAU1)/(BEDNORZ_TAU2-BEDNORZ_TAU1)) :
               (abs_tau < BEDNORZ_TAU3 ?
                BEDNORZ_EPS2*sgn_tau :
                (abs_tau < BEDNORZ_TAU4 ?
                 (BEDNORZ_EPS2*sgn_tau*
                  (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                 0.0))));
    case 3:
      return (abs_tau < BEDNORZ_TAU1 ?
              BEDNORZ_EPS3*sgn_tau*abs_tau/BEDNORZ_TAU1 :
              (abs_tau < BEDNORZ_TAU3 ?
               BEDNORZ_EPS3*sgn_tau :
               (abs_tau < BEDNORZ_TAU4 ?
                (BEDNORZ_EPS3*sgn_tau*
                 (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                0.0)));
    case 4:
      return (abs_tau < BEDNORZ_TAU1 ?
              BEDNORZ_EPS4*sgn_tau*abs_tau/BEDNORZ_TAU1 :
              (abs_tau < BEDNORZ_TAU3 ?
               BEDNORZ_EPS4*sgn_tau :
               (abs_tau < BEDNORZ_TAU4 ?
                (BEDNORZ_EPS4*sgn_tau*
                 (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                0.0)));
    case 5:
      return (abs_tau < BEDNORZ_TAU1 ?
              BEDNORZ_EPS5*sgn_tau*abs_tau/BEDNORZ_TAU1 :
              (abs_tau < BEDNORZ_TAU3 ?
               BEDNORZ_EPS5*sgn_tau :
               (abs_tau < BEDNORZ_TAU4 ?
                (BEDNORZ_EPS5*sgn_tau*
                 (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                0.0)));
    default:
      return 0.0;
  }
}


/* Compute the equations for a point x and its partial derivatives dxdph and
   dxdth in the Bednorz sphere eversion based on the sphere parameters phi
   (longitude) and theta (latitude) and the shape parameters bsp.  This
   corresponds to equations (4), (12), and (15) in the paper. */
static inline void bednorz_get_p0(double phi, double theta,
                                  bednorz_shape_par *bsp, double x[3],
                                  double dxdph[3], double dxdth[3])
{
  int n;
  double kappa, omega, t, p, q, eta, lambda;
  double ct, st, cp, sp, cnm1p, snm1p, cnp, snp;
  double ctn, ictn, ictnp1, ct2, st2, ct2n, ict2n, ict2np1, ton;
  double oml, omlplctn, pe1pk, tat2k, nst2pct2, ost, snpmqt;
  double nm1p, ostictn, oictnp1, nst2pct2oictnp1, tcp, tsp;
  double lost, tomlplctn, lostcp, lostsp, tomlplctncp, tomlplctnsp;
  double tomlplctncpmlostsp, tomlplctnspplostcp, ntctnst, oct2;
  double omlpe1pktat2k, nst2;

  n = bsp->n;
  kappa = bsp->kappa;
  omega = bsp->omega;
  t = bsp->t;
  p = bsp->p;
  q = bsp->q;
  eta = bsp->eta;
  lambda = bsp->lambda;

  ct = cos(theta);
  st = sin(theta);
  cp = cos(phi);
  sp = sin(phi);
  cnp = cos(n*phi);
  snp = sin(n*phi);
  ctn = ipow(ct,n);
  ictn = 1.0/ctn;
  ictnp1 = ictn/ct;
  ct2 = ct*ct;
  st2 = st*st;
  ton = t/n;
  snpmqt = snp-q*t;
  ost = omega*st;

  if (lambda >= 1.0)
  {
    cnm1p = cos((n-1)*phi);
    snm1p = sin((n-1)*phi);
    nst2pct2 = n*st2+ct2;
    nm1p = (n-1)*p;
    tcp = t*cp;
    tsp = t*sp;
    ostictn = ost*ictn;
    oictnp1 = omega*ictnp1;
    nst2pct2oictnp1 = nst2pct2*oictnp1;
    x[0] = p*snm1p-sp*ostictn+tcp;
    x[1] = p*cnm1p+cp*ostictn+tsp;
    x[2] = snpmqt*ostictn-ton*cnp;
    dxdph[0] = nm1p*cnm1p-cp*ostictn-tsp;
    dxdph[1] = -nm1p*snm1p-sp*ostictn+tcp;
    dxdph[2] = n*cnp*ostictn+t*snp;
    dxdth[0] = -sp*nst2pct2oictnp1;
    dxdth[1] = cp*nst2pct2oictnp1;
    dxdth[2] = snpmqt*nst2pct2oictnp1;
    /* The same formulas written out in full glory:
       x[0] = (t*cos(phi)+
               p*sin((n-1)*phi)-
               (omega*sin(theta)/ipow(cos(theta),n))*sin(phi));
       x[1] = (t*sin(phi)+
               p*cos((n-1)*phi)+
               (omega*sin(theta)/ipow(cos(theta),n))*cos(phi));
       x[2] = ((omega*sin(theta)/ipow(cos(theta),n))*sin(n*phi)-
               (t/n)*cos(n*phi)-
               omega*q*t*sin(theta)/ipow(cos(theta),n));
       dxdph[0] = (-t*sin(phi)+
                   (n-1)*p*cos((n-1)*phi)-
                   (omega*sin(theta)/ipow(cos(theta),n))*cos(phi));
       dxdph[1] = (t*cos(phi)-
                   (n-1)*p*sin((n-1)*phi)-
                   (omega*sin(theta)/ipow(cos(theta),n))*sin(phi));
       dxdph[2] = ((n*omega*sin(theta)/ipow(cos(theta),n))*cos(n*phi)+
                   t*sin(n*phi));
       dxdth[0] = -((omega*sin(phi)*(n*ipow(sin(theta),2)+ipow(cos(theta),2)))/
                    ipow(cos(theta),n+1));
       dxdth[1] = ((omega*cos(phi)*(n*ipow(sin(theta),2)+ipow(cos(theta),2)))/
                   ipow(cos(theta),n+1));
       dxdth[2] = ((omega*sin(n*phi)*(n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),n+1))-
                   (omega*q*t*(n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),n+1)));
     */
  }
  else
  {
    ct2n = ipow(ct,2*n);
    ict2n = 1.0/ct2n;
    ict2np1 = ict2n/ct;
    oml = 1.0-lambda;
    omlplctn = oml+lambda*ctn;
    pe1pk = pow(eta,1.0+kappa);
    tat2k = t*pow(fabs(t),2.0*kappa);
    lost = lambda*ost;
    lostcp = lost*cp;
    lostsp = lost*sp;
    tomlplctn = t*omlplctn;
    tomlplctncp = tomlplctn*cp;
    tomlplctnsp = tomlplctn*sp;
    tomlplctncpmlostsp = tomlplctncp-lostsp;
    tomlplctnspplostcp = tomlplctnsp+lostcp;
    ntctnst = n*t*ctn*st;
    oct2 = omega*ct2;
    omlpe1pktat2k = oml*pe1pk*tat2k;
    nst2 = n*st2;
    x[0] = tomlplctncpmlostsp*ictn;
    x[1] = tomlplctnspplostcp*ictn;
    x[2] = lambda*(ost*snpmqt*ictn-ton*cnp)-omlpe1pktat2k*st*ict2n;
    dxdph[0] = -tomlplctnspplostcp*ictn;
    dxdph[1] = tomlplctncpmlostsp*ictn;
    dxdph[2] = lambda*(omega*n*st*cnp*ictn+t*snp);
    dxdth[0] = (n*tomlplctncpmlostsp*st-lambda*(ntctnst*cp+oct2*sp))*ictnp1;
    dxdth[1] = (n*tomlplctnspplostcp*st-lambda*(ntctnst*sp-oct2*cp))*ictnp1;
    dxdth[2] = (lambda*omega*snpmqt*(nst2+ct2)*ictnp1-
                omlpe1pktat2k*(2.0*nst2+ct2)*ict2np1);
    /* The same formulas written out in full glory:
       x[0] = ((t*(1.0-lambda+lambda*ipow(cos(theta),n))*cos(phi)-
                lambda*omega*sin(theta)*sin(phi))/
               ipow(cos(theta),n));
       x[1] = ((t*(1.0-lambda+lambda*ipow(cos(theta),n))*sin(phi)+
                lambda*omega*sin(theta)*cos(phi))/
               ipow(cos(theta),n));
       x[2] = (lambda*(omega*sin(theta)*(sin(n*phi)-q*t)/ipow(cos(theta),n)-
                       (t/n)*cos(n*phi))-
               (1.0-lambda)*pow(eta,1.0+kappa)*
               t*pow(fabs(t),2.0*kappa)*sin(theta)/ipow(cos(theta),2*n));
       dxdph[0] = ((-t*(1.0-lambda+lambda*ipow(cos(theta),n))*sin(phi)-
                    lambda*omega*sin(theta)*cos(phi))/
                   ipow(cos(theta),n));
       dxdph[1] = ((t*(1.0-lambda+lambda*ipow(cos(theta),n))*cos(phi)-
                    lambda*omega*sin(theta)*sin(phi))/
                   ipow(cos(theta),n));
       dxdph[2] = (lambda*(omega*n*sin(theta)*(cos(n*phi))/ipow(cos(theta),n)+
                           t*sin(n*phi)));
       dxdth[0] = ((n*(t*(1.0-lambda+lambda*ipow(cos(theta),n))*cos(phi)-
                       lambda*omega*sin(theta)*sin(phi))*sin(theta)/
                    ipow(cos(theta),n+1))-
                   (lambda*(n*t*ipow(cos(theta),n)*sin(theta)*cos(phi)+
                            omega*ipow(cos(theta),2)*sin(phi))/
                    ipow(cos(theta),n+1)));
       dxdth[1] = ((n*(t*(1.0-lambda+lambda*ipow(cos(theta),n))*sin(phi)+
                       lambda*omega*sin(theta)*cos(phi))*sin(theta)/
                    ipow(cos(theta),n+1))-
                   (lambda*(n*t*ipow(cos(theta),n)*sin(theta)*sin(phi)-
                            omega*ipow(cos(theta),2)*cos(phi))/
                    ipow(cos(theta),n+1)));
       dxdth[2] = ((lambda*omega*(sin(n*phi)-q*t)*
                    (n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),n+1))-
                   ((1.0-lambda)*pow(eta,1.0+kappa)*t*pow(fabs(t),2.0*kappa)*
                    (2.0*n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),2*n+1)));
     */
  }
}


/* Compute the equations for a point y and its partial derivatives dydph and
   dydth in the Bednorz sphere eversion based on the sphere parameters phi
   (longitude) and theta (latitude) and the shape parameters bsp.  This
   corresponds to equation (7) in the paper. */
static inline void bednorz_get_p1(double phi, double theta,
                                  bednorz_shape_par *bsp, double y[3],
                                  double dydph[3], double dydth[3])
{
  double kappa, xi, eta;
  double x[3], dxdph[3], dxdth[3];
  double x0, x1, x2, x02, x12, x02px12, ex02px12, xipex02px12;
  double ixipex02px122, ixipex02px12k, ixipex02px12kp1;
  double x0dx0dphpx1dx1dph, x0dx0dthpx1dx1dth;
  double tex0dx0dphpx1dx1dph, tex0dx0dthpx1dx1dth;
  double ktex0dx0dphpx1dx1dph, ktex0dx0dthpx1dx1dth;

  kappa = bsp->kappa;
  xi = bsp->xi;
  eta = bsp->eta;

  bednorz_get_p0(phi,theta,bsp,x,dxdph,dxdth);

  x0 = x[0];
  x1 = x[1];
  x2 = x[2];
  x02 = x0*x0;
  x12 = x1*x1;
  x02px12 = x02+x12;
  ex02px12 = eta*x02px12;
  xipex02px12 = xi+ex02px12;
  ixipex02px122 = 1.0/(xipex02px12*xipex02px12);
  ixipex02px12k = 1.0/pow(xipex02px12,kappa);
  ixipex02px12kp1 = ixipex02px12k/xipex02px12;
  x0dx0dphpx1dx1dph = x0*dxdph[0]+x1*dxdph[1];
  x0dx0dthpx1dx1dth = x0*dxdth[0]+x1*dxdth[1];
  tex0dx0dphpx1dx1dph = 2.0*eta*x0dx0dphpx1dx1dph;
  tex0dx0dthpx1dx1dth = 2.0*eta*x0dx0dthpx1dx1dth;
  ktex0dx0dphpx1dx1dph = kappa*tex0dx0dphpx1dx1dph;
  ktex0dx0dthpx1dx1dth = kappa*tex0dx0dthpx1dx1dth;

  y[0] = x0*ixipex02px12k;
  y[1] = x1*ixipex02px12k;
  y[2] = x2/xipex02px12;
  dydph[0] = (dxdph[0]*xipex02px12-ktex0dx0dphpx1dx1dph*x0)*ixipex02px12kp1;
  dydph[1] = (dxdph[1]*xipex02px12-ktex0dx0dphpx1dx1dph*x1)*ixipex02px12kp1;
  dydph[2] = (dxdph[2]*xipex02px12-tex0dx0dphpx1dx1dph*x2)*ixipex02px122;
  dydth[0] = (dxdth[0]*xipex02px12-ktex0dx0dthpx1dx1dth*x0)*ixipex02px12kp1;
  dydth[1] = (dxdth[1]*xipex02px12-ktex0dx0dthpx1dx1dth*x1)*ixipex02px12kp1;
  dydth[2] = (dxdth[2]*xipex02px12-tex0dx0dthpx1dx1dth*x2)*ixipex02px122;

  /* The same formulas written out in full glory:
     y[0] = x[0]/pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa);
     y[1] = x[1]/pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa);
     y[2] = x[2]/(xi+eta*(ipow(x[0],2)+ipow(x[1],2)));
     dydph[0] = ((dxdph[0]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[0]*(x[0]*dxdph[0]+x[1]*dxdph[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydph[1] = ((dxdph[1]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[1]*(x[0]*dxdph[0]+x[1]*dxdph[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydph[2] = ((dxdph[2]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*x[2]*(x[0]*dxdph[0]+x[1]*dxdph[1]))/
                 ipow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),2));
     dydth[0] = ((dxdth[0]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[0]*(x[0]*dxdth[0]+x[1]*dxdth[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydth[1] = ((dxdth[1]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[1]*(x[0]*dxdth[0]+x[1]*dxdth[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydth[2] = ((dxdth[2]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*x[2]*(x[0]*dxdth[0]+x[1]*dxdth[1]))/
                 ipow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),2));
  */
}


/* Compute the equations for a point z and its partial derivatives dzdph and
   dzdth in the Bednorz sphere eversion based on the sphere parameters phi
   (longitude) and theta (latitude) and the shape parameters bsp.  This
   corresponds to equations (8) and (9) in the paper. */
static void inline bednorz_get_p2(double phi, double theta,
                                  bednorz_shape_par *bsp, double z[3],
                                  double dzdph[3], double dzdth[3])
{
  double alpha, beta, gamma;
  double y[3], dydph[3], dydth[3];
  double y0, y1, y2, y02, y12, y02py12, egy2, apby02py12, amby02py12;
  double iapby02py12, iapby02py122, igapby02py12, igapby02py122;
  double ambogapb, egy2apby02py12, egy2amby02py12, tbegy2;
  double y0dy0dphpy1dy1dph, y0dy0dthpy1dy1dth;
  double iy02py12, iy02py122, ty0dy0dphpy1dy1dph, ty0dy0dthpy1dy1dth;
  double tbegy2y0dy0dphpy1dy1dph, tbegy2y0dy0dthpy1dy1dth;
  double gigapby02py122, gegy2amby02py12;

  alpha = bsp->alpha;
  beta = bsp->beta;
  gamma = bsp->gamma;

  bednorz_get_p1(phi,theta,bsp,y,dydph,dydth);

  y0 = y[0];
  y1 = y[1];
  y2 = y[2];
  y02 = y0*y0;
  y12 = y1*y1;
  y02py12 = y02+y12;
  y0dy0dphpy1dy1dph = y0*dydph[0]+y1*dydph[1];
  y0dy0dthpy1dy1dth = y0*dydth[0]+y1*dydth[1];

  if (alpha > 0.0)
  {
    /* For the north and south poles, the equations in bednorz_get_p0
       and bednorz_get_p1 become singular.  Therefore, we include a special
       treatment here.  Furthermore, we compute the surface normal vector
       based on the cross product of the partial derivative vectors in
       bednorz_point_normal.  For the north and south poles, the partial
       derivative vectors are linearly dependent and thus don't yield a
       useful normal vector.  Therefore, we have to include a special
       treatment for the two poles. */
    if (fabs(theta-M_PI/2.0) <= 1.0e-4)
    {
      z[0] = 0.0;
      z[1] = 0.0;
      z[2] = -sqrt(alpha/beta)/(alpha+beta);
      dzdph[0] = 1.0;
      dzdph[1] = 0.0;
      dzdph[2] = 0.0;
      dzdth[0] = 0.0;
      dzdth[1] = 1.0;
      dzdth[2] = 0.0;
    }
    else if (fabs(theta+M_PI/2.0) <= 1.0e-4)
    {
      z[0] = 0.0;
      z[1] = 0.0;
      z[2] = -sqrt(alpha/beta)/(alpha+beta);
      dzdph[0] = 1.0;
      dzdph[1] = 0.0;
      dzdph[2] = 0.0;
      dzdth[0] = 0.0;
      dzdth[1] = -1.0;
      dzdth[2] = 0.0;
    }
    else
    {
      egy2 = exp(gamma*y2);
      apby02py12 = alpha+beta*y02py12;
      amby02py12 = alpha-beta*y02py12;
      iapby02py12 = 1.0/apby02py12;
      iapby02py122 = iapby02py12*iapby02py12;
      igapby02py12 = iapby02py12/gamma;
      igapby02py122 = igapby02py12*igapby02py12;
      ambogapb = (alpha-beta)/(gamma*(alpha+beta));
      egy2apby02py12 = egy2*apby02py12;
      egy2amby02py12 = egy2*amby02py12;
      tbegy2 = 2.0*beta*egy2;
      tbegy2y0dy0dphpy1dy1dph = tbegy2*y0dy0dphpy1dy1dph;
      tbegy2y0dy0dthpy1dy1dth = tbegy2*y0dy0dthpy1dy1dth;
      gigapby02py122 = gamma*igapby02py122;
      gegy2amby02py12 = gamma*egy2amby02py12;

      z[0] = y0*egy2*iapby02py12;
      z[1] = y1*egy2*iapby02py12;
      z[2] = egy2amby02py12*igapby02py12-ambogapb;
      dzdph[0] = ((y0*gamma*dydph[2]+dydph[0])*egy2apby02py12-
                  y0*tbegy2y0dy0dphpy1dy1dph)*iapby02py122;
      dzdph[1] = ((y1*gamma*dydph[2]+dydph[1])*egy2apby02py12-
                  y1*tbegy2y0dy0dphpy1dy1dph)*iapby02py122;
      dzdph[2] = (((gegy2amby02py12*dydph[2]-tbegy2y0dy0dphpy1dy1dph)*
                   apby02py12-tbegy2y0dy0dphpy1dy1dph*amby02py12)*
                  gigapby02py122);
      dzdth[0] = ((y0*gamma*dydth[2]+dydth[0])*egy2apby02py12-
                  y0*tbegy2y0dy0dthpy1dy1dth)*iapby02py122;
      dzdth[1] = ((y1*gamma*dydth[2]+dydth[1])*egy2apby02py12-
                  y1*tbegy2y0dy0dthpy1dy1dth)*iapby02py122;
      dzdth[2] = (((gegy2amby02py12*dydth[2]-tbegy2y0dy0dthpy1dy1dth)*
                   apby02py12-tbegy2y0dy0dthpy1dy1dth*amby02py12)*
                  gigapby02py122);
    }

    /* The same formulas written out in full glory:
       z[0] = (y[0]*exp(gamma*y[2]))/(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)));
       z[1] = (y[1]*exp(gamma*y[2]))/(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)));
       z[2] = ((exp(gamma*y[2])*(alpha-beta*(ipow(y[0],2)+ipow(y[1],2))))/
               (gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
               ((alpha-beta)/(gamma*(alpha+beta))));
       dzdph[0] = (((exp(gamma*y[2])*(gamma*y[0]*dydph[2]+dydph[0])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[0]*
                     (y[0]*dydph[0]+y[1]*dydph[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdph[1] = (((exp(gamma*y[2])*(gamma*y[1]*dydph[2]+dydph[1])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[1]*
                     (y[0]*dydph[0]+y[1]*dydph[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdph[2] = (((gamma*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*dydph[2]-
                     2.0*beta*exp(gamma*y[2])*(y[0]*dydph[0]+y[1]*dydph[1]))*
                    (gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*gamma*beta*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*
                     (y[0]*dydph[0]+y[1]*dydph[1])))/
                   ipow(gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))),2));
       dzdth[0] = (((exp(gamma*y[2])*(gamma*y[0]*dydth[2]+dydth[0])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[0]*
                     (y[0]*dydth[0]+y[1]*dydth[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdth[1] = (((exp(gamma*y[2])*(gamma*y[1]*dydth[2]+dydth[1])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[1]*
                     (y[0]*dydth[0]+y[1]*dydth[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdth[2] = (((gamma*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*dydth[2]-
                     2.0*beta*exp(gamma*y[2])*(y[0]*dydth[0]+y[1]*dydth[1]))*
                    (gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*gamma*beta*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*
                     (y[0]*dydth[0]+y[1]*dydth[1])))/
                   ipow(gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))),2));
     */
  }
  else
  {
    iy02py12 = 1.0/y02py12;
    iy02py122 = iy02py12*iy02py12;
    ty0dy0dphpy1dy1dph = 2.0*y0dy0dphpy1dy1dph;
    ty0dy0dthpy1dy1dth = 2.0*y0dy0dthpy1dy1dth;

    z[0] = y0*iy02py12;
    z[1] = y1*iy02py12;
    z[2] = -y2;

    /* We compute the surface normal vector based on the cross product
       of the partial derivative vectors in bednorz_point_normal.  For the
       north and south poles, the partial derivative vectors are linearly
       dependent and thus don't yield a useful normal vector.  Therefore,
       we have to include a special treatment for the two poles. */
    if (fabs(theta-M_PI/2.0) <= 1.0e-4)
    {
      dzdph[0] = 1.0;
      dzdph[1] = 0.0;
      dzdph[2] = 0.0;
      dzdth[0] = 0.0;
      dzdth[1] = 1.0;
      dzdth[2] = 0.0;
    }
    else if (fabs(theta+M_PI/2.0) <= 1.0e-4)
    {
      dzdph[0] = 1.0;
      dzdph[1] = 0.0;
      dzdph[2] = 0.0;
      dzdth[0] = 0.0;
      dzdth[1] = -1.0;
      dzdth[2] = 0.0;
    }
    else
    {
      dzdph[0] = (dydph[0]*y02py12-y0*ty0dy0dphpy1dy1dph)*iy02py122;
      dzdph[1] = (dydph[1]*y02py12-y1*ty0dy0dphpy1dy1dph)*iy02py122;
      dzdph[2] = -dydph[2];
      dzdth[0] = (dydth[0]*y02py12-y0*ty0dy0dthpy1dy1dth)*iy02py122;
      dzdth[1] = (dydth[1]*y02py12-y1*ty0dy0dthpy1dy1dth)*iy02py122;
      dzdth[2] = -dydth[2];
    }

    /* The same formulas written out in full glory:
       z[0] = y[0]/(ipow(y[0],2)+ipow(y[1],2));
       z[1] = y[1]/(ipow(y[0],2)+ipow(y[1],2));
       z[2] = -y[2];
       dzdph[0] = ((dydph[0]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[0]*(y[0]*dydph[0]+y[1]*dydph[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdph[1] = ((dydph[1]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[1]*(y[0]*dydph[0]+y[1]*dydph[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdph[2] = -dydph[2];
       dzdth[0] = ((dydth[0]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[0]*(y[0]*dydth[0]+y[1]*dydth[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdth[1] = ((dydth[1]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[1]*(y[0]*dydth[0]+y[1]*dydth[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdth[2] = -dydth[2];
     */
  }
}


/* Compute a point p and its surface normal n in the Bednorz sphere
   eversion based on the sphere parameters phi (longitude) and theta
   (latitude) and the shape parameters bsp. */
static void inline bednorz_point_normal(double phi, double theta,
                                        bednorz_shape_par *bsp,
                                        float p[3], float n[3])
{
  double z[3], dzdph[3], dzdth[3];
  float a[3], b[3], t;
  float lambda, eps, oz;
  int i;

  bednorz_get_p2(phi,theta,bsp,z,dzdph,dzdth);

  for (i=0; i<3; i++)
  {
    p[i] = z[i];
    a[i] = dzdph[i];
    b[i] = dzdth[i];
  }

  /* In the original version of the Bednorz sphere eversion, the regions
     around the north and south poles of the sphere are deformed to points
     that lie very close to each other.  This leads to a significant amount
     of z fighting, especially for higher eversion orders, which is visually
     unpleasant.  Therefore, we modify the shape of the deformed sphere very
     slightly to avoid or at least ameliorate the z fighting. */
  lambda = bsp->lambda;
  eps = bsp->eps;
  if (lambda == 1.0)
    oz = eps*sin(theta);
  else
    oz = 0.0f;
  p[2] += oz;

  cross(a,b,n);
  t = 1.0f/sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
  n[0] *= t;
  n[1] *= t;
  n[2] *= t;
}


/* Set up the surface colors for the main pass (i.e., index 0). */
static void setup_surface_colors(ModeInfo *mi, float phi_min, float phi_max,
                                 float theta_min, float theta_max,
                                 int num_phi, int num_theta)
{
  int i, j, o;
  float phi, theta, phi_range, theta_range;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  static const float matc[3][3] = {
    {  0.577350269f, -0.577350269f,  0.577350269f },
    {  0.211324865f,  0.788675135f,  0.577350269f },
    { -0.788675135f, -0.211324865f,  0.577350269f }
  };

  if (se->colors[0] != COLORS_TWOSIDED && se->colors[0] != COLORS_EARTH)
  {
    phi_range = phi_max-phi_min;
    theta_range = theta_max-theta_min;
    for (j=0; j<=num_phi; j++)
    {
      phi = phi_range*j/num_phi+phi_min;
      for (i=0; i<=num_theta; i++)
      {
        o = j*(num_theta+1)+i;
        theta = theta_range*i/num_theta+theta_min;
        if (se->colors[0] == COLORS_PARALLEL)
          color(se,(2.0f*theta+3.0f*M_PI_F/4.0f)*(2.0f/3.0f),matc,
                &se->colf[0][4*o],&se->colb[0][4*o]);
        else /* se->colors[0] == COLORS_MERIDIAN */
          color(se,phi,matc,&se->colf[0][4*o],&se->colb[0][4*o]);
      }
    }
  }
#ifdef VERTEXATTRIBARRAY_WORKAROUND
  else /* se->colors[0] == COLORS_TWOSIDED || se->colors[0] == COLORS_EARTH*/
  {
    /* For some strange reason, the color buffer must be initialized
       and used on macOS. Otherwise two-sided lighting will not
       work. */
    int k;
    for (j=0; j<=num_phi; j++)
    {
      for (i=0; i<=num_theta; i++)
      {
        o = j*(num_theta+1)+i;
        for (k=0; k<4; k++)
        {
          se->colf[0][4*o+k] = 1.0f;
          se->colb[0][4*o+k] = 1.0f;
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
                 4*(num_phi+1)*(num_theta+1)*sizeof(GLfloat),
                 se->colf[0],GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorb_buffer[0]);
    glBufferData(GL_ARRAY_BUFFER,
                 4*(num_phi+1)*(num_theta+1)*sizeof(GLfloat),
                 se->colb[0],GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER,0);
  }
#endif /* HAVE_GLSL */
}


#ifdef HAVE_GLSL

/* Set up the texture coordinates. */
static void setup_tex_coords(ModeInfo *mi, float phi_min, float phi_max,
                             float theta_min, float theta_max,
                             int num_phi, int num_theta)
{
  int i, j, o;
  float phi, theta, phi_range, theta_range, phi_scale, theta_scale;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  phi_range = phi_max-phi_min;
  theta_range = theta_max-theta_min;
  phi_scale = 0.5f/M_PI_F;
  theta_scale = 1.0f/M_PI_F;
  for (j=0; j<=num_phi; j++)
  {
    phi = phi_range*j/num_phi+phi_min;
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      theta = theta_range*i/num_theta+theta_min;
      se->tex[2*o+0] = phi_scale*phi+0.5f;
      se->tex[2*o+1] = theta_scale*theta+0.5f;
    }
  }

  if (se->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_tex_coord_buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 2*(num_phi+1)*(num_theta+1)*sizeof(GLfloat),
                 se->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
  }
}

#endif /* HAVE_GLSL */


/* Draw the Bednorz sphere eversion using OpenGL's fixed
   functionality. */
static int bednorz_sphere_eversion_ff(ModeInfo *mi, float phi_min,
                                      float phi_max, float theta_min,
                                      float theta_max, int num_phi,
                                      int num_theta, int numb_dist,
                                      int numb_dir, int num_grid)
{
  static const GLfloat light_ambient[]        = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_diffuse[]        = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[]       = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_position[]       = { 1.0, 1.0, 1.0, 0.0 };
  static const GLfloat mat_specular[]         = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat mat_diff_front[]       = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_back[]        = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_trans_front[] = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_back[]  = { 0.0, 1.0, 0.0, 0.7 };
  float p[3], n[3], mat[3][3];
  int i, j, k, l, m, o;
  int numb_dist_mask, numb_dist_min, numb_dist_max;
  int numb_dir_mask, numb_dir_min, numb_dir_max;
  float phi, theta, phi_range, theta_range;
  float *xx, *xn, *cf, *cb;
  float a, b, c, d, e, tau, tau_min, tau_max, r, s;
  float x, y, z, zmin, zmax, rmax, scale, offset_z;
  bednorz_shape_par bsp;
  float qu[4], r1[3][3], r2[3][3];
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  int polys;

  tau = se->tau;
  /* Apply easing functions to the different ranges of tau. */
  if (fabsf(tau) <= BEDNORZ_TAU4)
  {
    if (fabsf(tau) <= BEDNORZ_TAU1)
    {
      tau_min = 0.0f;
      tau_max = BEDNORZ_TAU1;
    }
    else
    if (fabsf(tau) <= BEDNORZ_TAU2)
    {
      tau_min = BEDNORZ_TAU1;
      tau_max = BEDNORZ_TAU2;
    }
    else if (fabsf(tau) <= BEDNORZ_TAU3)
    {
      tau_min = BEDNORZ_TAU2;
      tau_max = BEDNORZ_TAU3;
    }
    else /* fabsf(tau) <= BEDNORZ_TAU4 */
    {
      tau_min = BEDNORZ_TAU3;
      tau_max = BEDNORZ_TAU4;
    }
    e = 1.0f/(tau_min*tau_min-2.0f*tau_min*tau_max+tau_max*tau_max);
    a = -2.0f*e;
    b = 3.0f*(tau_min+tau_max)*e;
    c = -6.0f*tau_min*tau_max*e;
    d = tau_min*tau_max*(tau_min+tau_max)*e;
    if (tau >= 0.0f)
      tau = ((a*tau+b)*tau+c)*tau+d;
    else
      tau = ((a*tau-b)*tau+c)*tau-d;
  }
  /* Set up the shape parameters. */
  bsp.n = se->g;
  bsp.kappa = bednorz_get_kappa(bsp.n);
  bsp.omega = BEDNORZ_OMEGA;
  bsp.t = bednorz_get_t(tau);
  bsp.p = bednorz_get_p(tau);
  bsp.q = bednorz_get_q(tau);
  bsp.xi = bednorz_get_xi(tau);
  bsp.eta = bednorz_get_eta(tau,se->eta_min);
  bsp.alpha = bednorz_get_alpha(tau);
  bsp.beta = bednorz_get_beta(tau,se->beta_max);
  bsp.gamma = bednorz_get_gamma(bsp.alpha,bsp.beta);
  bsp.lambda = bednorz_get_lambda(tau);
  bsp.eps = bednorz_get_eps(tau,bsp.n);

  /* Compute the surface points and normals. */
  phi_range = phi_max-phi_min;
  theta_range = theta_max-theta_min;
  for (j=0; j<=num_phi; j++)
  {
    phi = phi_range*j/num_phi+phi_min;
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      theta = theta_range*i/num_theta+theta_min;
      bednorz_point_normal(phi,theta,&bsp,&se->sp[3*o],&se->sn[3*o]);
    }
  }

  /* Compute the z offset. */
  zmin = FLT_MAX;
  zmax = -FLT_MAX;
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      z = se->sp[3*o+2];
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
    }
  }
  offset_z = -0.5f*(zmin+zmax);

  /* Shift the surface in the z direction and compute the scale. */
  rmax = -FLT_MAX;
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      se->sp[3*o+2] += offset_z;
      x = se->sp[3*o];
      y = se->sp[3*o+1];
      z = se->sp[3*o+2];
      r = x*x+y*y+z*z;
      if (r > rmax)
        rmax = r;
    }
  }
  scale = 0.75f/sqrtf(rmax);

  /* Scale the surface. */
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      se->sp[3*o] *= scale;
      se->sp[3*o+1] *= scale;
      se->sp[3*o+2] *= scale;
    }
  }

  /* Compute the rotation that rotates the surface in 3D, including the
     trackball rotations. */
  rotateall(se->alpha,se->beta,se->delta,r1);

  gltrackball_get_quaternion(se->trackball,qu);
  quat_to_rotmat(qu,r2);

  mult_rotmat(r2,r1,mat);

  numb_dist_mask = numb_dist-1;
  numb_dist_min = numb_dist/4;
  numb_dist_max = 3*numb_dist/4;
  numb_dir_mask = numb_dir-1;
  numb_dir_min = numb_dir/4;
  numb_dir_max = 3*numb_dir/4;

  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (se->projection == DISP_PERSPECTIVE)
  {
      gluPerspective(60.0,se->aspect,0.1,10.0);
  }
  else
  {
    if (se->aspect >= 1.0)
      glOrtho(-se->aspect,se->aspect,-1.0,1.0,0.1,10.0);
    else
      glOrtho(-1.0,1.0,-1.0/se->aspect,1.0/se->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (se->display_mode[0] == DISP_SURFACE)
  {
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
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
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else /* se->display_mode[0] == DISP_TRANSPARENT */
  {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
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
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }

  if (se->colors[0] == COLORS_TWOSIDED || se->colors[0] == COLORS_EARTH)
  {
    glColor3fv(mat_diff_front);
    if (se->display_mode[0] == DISP_TRANSPARENT)
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_front);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_back);
    }
    else
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_front);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_back);
    }
  }
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f,1.0f);

  if (se->appearance[0] == APPEARANCE_PARALLEL_BANDS)
  {
    for (i=0; i<num_theta; i++)
    {
      if (((i & numb_dist_mask) >= numb_dist_min) &&
          ((i & numb_dist_mask) < numb_dist_max))
        continue;
      glBegin(GL_TRIANGLE_STRIP);
      for (j=num_phi; j>=0; j--)
      {
        for (k=0; k<=1; k++)
        {
          l = i+k;
          m = j;
          o = m*(num_theta+1)+l;
          phi = phi_range*m/num_phi+phi_min;
          theta = theta_range*l/num_theta+theta_min;
          xx = &se->sp[3*o];
          xn = &se->sn[3*o];
          for (l=0; l<3; l++)
          {
            r = 0.0f;
            s = 0.0f;
            for (m=0; m<3; m++)
            {
              r += mat[l][m]*xx[m];
              s += mat[l][m]*xn[m];
            }
            p[l] = r+se->offset3d[l];
            n[l] = s;
          }
          if (se->colors[0] != COLORS_TWOSIDED &&
              se->colors[0] != COLORS_EARTH)
          {
            cf = &se->colf[0][4*o];
            cb = &se->colb[0][4*o];
            glColor3fv(cf);
            glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
            glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
          }
          glNormal3fv(n);
          glVertex3fv(p);
        }
      }
      glEnd();
    }
    polys = num_theta*(num_phi+1);
  }
  else /* se->appearance[0] != APPEARANCE_PARALLEL_BANDS */
  {
    for (j=0; j<num_phi; j++)
    {
      if (se->appearance[0] == APPEARANCE_MERIDIAN_BANDS &&
          ((j & numb_dir_mask) >= numb_dir_min) &&
          ((j & numb_dir_mask) < numb_dir_max))
        continue;
      glBegin(GL_TRIANGLE_STRIP);
      for (i=0; i<=num_theta; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = j+k;
          o = m*(num_theta+1)+l;
          phi = phi_range*m/num_phi+phi_min;
          theta = theta_range*l/num_theta+theta_min;
          xx = &se->sp[3*o];
          xn = &se->sn[3*o];
          for (l=0; l<3; l++)
          {
            r = 0.0f;
            s = 0.0f;
            for (m=0; m<3; m++)
            {
              r += mat[l][m]*xx[m];
              s += mat[l][m]*xn[m];
            }
            p[l] = r+se->offset3d[l];
            n[l] = s;
          }
          if (se->colors[0] != COLORS_TWOSIDED &&
              se->colors[0] != COLORS_EARTH)
          {
            cf = &se->colf[0][4*o];
            cb = &se->colb[0][4*o];
            glColor3fv(cf);
            glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
            glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
          }
          glNormal3fv(n);
          glVertex3fv(p);
        }
      }
      glEnd();
    }
    polys = 2*num_phi*(num_theta+1);
    if (se->appearance[0] == APPEARANCE_MERIDIAN_BANDS)
      polys /= 2;
  }

  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0.0f,0.0f);
  if (se->graticule[0])
  {
    glColor4f(1.0f,1.0f,1.0f,1.0f);
    glLineWidth(2.0f);
    glEnable(GL_LINE_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glEnable(GL_BLEND);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    /* Draw meridians. */
    for (j=0; j<num_phi; j+=num_grid)
    {
      glBegin(GL_LINE_STRIP);
      for (i=0; i<=num_theta; i++)
      {
        o = j*(num_theta+1)+i;
        phi = phi_range*j/num_phi+phi_min;
        theta = theta_range*i/num_theta+theta_min;
        xx = &se->sp[3*o];
        for (l=0; l<3; l++)
        {
          r = 0.0f;
          for (m=0; m<3; m++)
            r += mat[l][m]*xx[m];
          p[l] = r+se->offset3d[l];
        }
        glVertex3fv(p);
      }
      glEnd();
    }
    /* Draw parallels. */
    for (i=num_grid; i<=num_theta-num_grid; i+=num_grid)
    {
      glBegin(GL_LINE_LOOP);
      for (j=num_phi-1; j>=0; j--)
      {
        o = j*(num_theta+1)+i;
        phi = phi_range*j/num_phi+phi_min;
        theta = theta_range*i/num_theta+theta_min;
        xx = &se->sp[3*o];
        for (l=0; l<3; l++)
        {
          r = 0.0f;
          for (m=0; m<3; m++)
            r += mat[l][m]*xx[m];
          p[l] = r+se->offset3d[l];
        }
        glVertex3fv(p);
      }
      glEnd();
    }
    glLineWidth(1.0f);
    glPolygonOffset(0.0f,0.0f);
    glDisable(GL_LINE_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    if (se->display_mode[0] != DISP_TRANSPARENT)
      glDisable(GL_BLEND);
  }

  return polys;
}


#ifdef HAVE_GLSL

/* Draw the Bednorz sphere eversion using OpenGL's programmable
   functionality. */
static int bednorz_sphere_eversion_pf(ModeInfo *mi, float phi_min,
                                      float phi_max, float theta_min,
                                      float theta_max, int num_phi,
                                      int num_theta, int numb_dist,
                                      int numb_dir, int num_grid)
{
  static const GLfloat light_model_ambient[]  = { 0.2, 0.2, 0.2, 1.0 };
  static const GLfloat light_ambient[]        = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_ambient_earth[]  = { 0.3, 0.3, 0.3, 1.0 };
  static const GLfloat light_diffuse[]        = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[]       = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_position[]       = { 1.0, 1.0, 1.0, 0.0 };
  static const GLfloat mat_specular[]         = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat mat_diff_front[]       = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_back[]        = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_trans_front[] = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_back[]  = { 0.0, 1.0, 0.0, 0.7 };
  static const GLfloat mat_diff_white[]       = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat mat_diff_white_trans[] = { 1.0, 1.0, 1.0, 0.7 };
  static const GLuint blend_indices[6] = { 0, 1, 2, 2, 3, 0 };
  static const GLfloat blend_p[4][2] =
    { { -1.0, -1.0 }, { 1.0, -1.0 }, { 1.0, 1.0 }, { -1.0, 1.0 } };
  static const GLfloat blend_t[4][2] =
    { { 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 }, { 0.0, 1.0 } };
  GLfloat light_direction[3], half_vector[3], len;
  GLfloat p_mat[16], mv_mat[16], rot_mat[16];
  float mat[3][3];
  int i, j, k, l, m, o, pass, num_passes, ni;
  int numb_dist_mask, numb_dist_min, numb_dist_max;
  int numb_dir_mask, numb_dir_min, numb_dir_max;
  float phi, theta, phi_range, theta_range;
  float a, b, c, d, e, tau, tau_min, tau_max, r, t;
  float x, y, z, zmin, zmax, rmax, scale, offset_z;
  bednorz_shape_par bsp;
  float qu[4], r1[3][3], r2[3][3];
  GLint draw_buf, read_buf;
  GLsizeiptr index_offset;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  int polys = 0;

  if (!se->use_shaders)
    return 0;

  numb_dist_mask = numb_dist-1;
  numb_dist_min = numb_dist/4;
  numb_dist_max = 3*numb_dist/4;
  numb_dir_mask = numb_dir-1;
  numb_dir_min = numb_dir/4;
  numb_dir_max = 3*numb_dir/4;

  if (!se->buffers_initialized)
  {
    /* The indices only need to be computed once. */
    /* Compute the solid indices. */
    ni = 0;
    se->num_solid_strips = 0;
    se->num_solid_triangles = 0;
    for (j=0; j<num_phi; j++)
    {
      for (i=0; i<=num_theta; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = j+k;
          o = m*(num_theta+1)+l;
          se->solid_indices[ni++] = o;
        }
      }
      se->num_solid_strips++;
    }
    se->num_solid_triangles = 2*(num_theta+1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->solid_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->solid_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    /* Compute the meridian indices. */
    ni = 0;
    se->num_meridian_strips = 0;
    se->num_meridian_triangles = 0;
    for (j=0; j<num_phi; j++)
    {
      if (((j & numb_dir_mask) >= numb_dir_min) &&
          ((j & numb_dir_mask) < numb_dir_max))
        continue;
      for (i=0; i<=num_theta; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = j+k;
          o = m*(num_theta+1)+l;
          se->meridian_indices[ni++] = o;
        }
      }
      se->num_meridian_strips++;
    }
    se->num_meridian_triangles = 2*(num_theta+1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->meridian_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->meridian_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    /* Compute the parallel indices. */
    ni = 0;
    se->num_parallel_strips = 0;
    se->num_parallel_triangles = 0;
    for (i=0; i<num_theta; i++)
    {
      if (((i & numb_dist_mask) >= numb_dist_min) &&
          ((i & numb_dist_mask) < numb_dist_max))
        continue;
      for (j=num_phi; j>=0; j--)
      {
        for (k=0; k<=1; k++)
        {
          l = i+k;
          m = j;
          o = m*(num_theta+1)+l;
          se->parallel_indices[ni++] = o;
        }
      }
      se->num_parallel_strips++;
    }
    se->num_parallel_triangles = 2*(num_phi+1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->parallel_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->parallel_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    /* Compute the line indices. */
    ni = 0;
    for (j=0; j<num_phi; j+=num_grid)
    {
      for (i=0; i<num_theta; i++)
      {
        se->line_indices[ni++] = j*(num_theta+1)+i;
        se->line_indices[ni++] = j*(num_theta+1)+i+1;
      }
    }
    for (i=num_grid; i<=num_theta-num_grid; i+=num_grid)
    {
      for (j=num_phi; j>0; j--)
      {
        o = j*(num_theta+1)+i;
        se->line_indices[ni++] = j*(num_theta+1)+i;
        se->line_indices[ni++] = (j-1)*(num_theta+1)+i;
      }
    }
    se->num_lines = ni;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->line_indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ni*sizeof(GLuint),
                 se->line_indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    se->buffers_initialized = True;
  }

  tau = se->tau;
  /* Apply easing functions to the different ranges of tau. */
  if (fabsf(tau) <= BEDNORZ_TAU4)
  {
    if (fabsf(tau) <= BEDNORZ_TAU1)
    {
      tau_min = 0.0f;
      tau_max = BEDNORZ_TAU1;
    }
    else
    if (fabsf(tau) <= BEDNORZ_TAU2)
    {
      tau_min = BEDNORZ_TAU1;
      tau_max = BEDNORZ_TAU2;
    }
    else if (fabsf(tau) <= BEDNORZ_TAU3)
    {
      tau_min = BEDNORZ_TAU2;
      tau_max = BEDNORZ_TAU3;
    }
    else /* fabsf(tau) <= BEDNORZ_TAU4 */
    {
      tau_min = BEDNORZ_TAU3;
      tau_max = BEDNORZ_TAU4;
    }
    e = 1.0f/(tau_min*tau_min-2.0f*tau_min*tau_max+tau_max*tau_max);
    a = -2.0f*e;
    b = 3.0f*(tau_min+tau_max)*e;
    c = -6.0f*tau_min*tau_max*e;
    d = tau_min*tau_max*(tau_min+tau_max)*e;
    if (tau >= 0.0f)
      tau = ((a*tau+b)*tau+c)*tau+d;
    else
      tau = ((a*tau-b)*tau+c)*tau-d;
  }
  /* Set up the shape parameters. */
  bsp.n = se->g;
  bsp.kappa = bednorz_get_kappa(bsp.n);
  bsp.omega = BEDNORZ_OMEGA;
  bsp.t = bednorz_get_t(tau);
  bsp.p = bednorz_get_p(tau);
  bsp.q = bednorz_get_q(tau);
  bsp.xi = bednorz_get_xi(tau);
  bsp.eta = bednorz_get_eta(tau,se->eta_min);
  bsp.alpha = bednorz_get_alpha(tau);
  bsp.beta = bednorz_get_beta(tau,se->beta_max);
  bsp.gamma = bednorz_get_gamma(bsp.alpha,bsp.beta);
  bsp.lambda = bednorz_get_lambda(tau);
  bsp.eps = bednorz_get_eps(tau,bsp.n);

  /* Compute the surface points and normals. */
  phi_range = phi_max-phi_min;
  theta_range = theta_max-theta_min;
  for (j=0; j<=num_phi; j++)
  {
    phi = phi_range*j/num_phi+phi_min;
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      theta = theta_range*i/num_theta+theta_min;
      bednorz_point_normal(phi,theta,&bsp,&se->sp[3*o],&se->sn[3*o]);
    }
  }

  /* Compute the z offset. */
  zmin = FLT_MAX;
  zmax = -FLT_MAX;
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      z = se->sp[3*o+2];
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
    }
  }
  offset_z = -0.5f*(zmin+zmax);

  /* Shift the surface in the z direction and compute the scale. */
  rmax = -FLT_MAX;
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      se->sp[3*o+2] += offset_z;
      x = se->sp[3*o];
      y = se->sp[3*o+1];
      z = se->sp[3*o+2];
      r = x*x+y*y+z*z;
      if (r > rmax)
        rmax = r;
    }
  }
  scale = 0.75f/sqrtf(rmax);

  glBindBuffer(GL_ARRAY_BUFFER,se->vertex_pos_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*(num_phi+1)*(num_theta+1)*sizeof(GLfloat),
               se->sp,GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,se->vertex_normal_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*(num_phi+1)*(num_theta+1)*sizeof(GLfloat),
               se->sn,GL_STREAM_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  /* Compute the rotation that rotates the surface in 3D, including the
     trackball rotations. */
  rotateall(se->alpha,se->beta,se->delta,r1);

  gltrackball_get_quaternion(se->trackball,qu);
  quat_to_rotmat(qu,r2);

  mult_rotmat(r2,r1,mat);

  glsl_Identity(p_mat);
  if (se->projection == DISP_PERSPECTIVE)
  {
    glsl_Perspective(p_mat,60.0f,se->aspect,0.1f,10.0f);
  }
  else
  {
    if (se->aspect >= 1.0)
      glsl_Orthographic(p_mat,-se->aspect,se->aspect,-1.0f,1.0f,
                        0.1f,10.0f);
    else
      glsl_Orthographic(p_mat,-1.0f,1.0f,-1.0f/se->aspect,1.0f/se->aspect,
                        0.1f,10.0f);
  }
  glsl_Identity(rot_mat);
  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      rot_mat[GLSL__LINCOOR(i,j,4)] = mat[i][j];
  glsl_Identity(mv_mat);
  glsl_Translate(mv_mat,se->offset3d[0],se->offset3d[1],se->offset3d[2]);
  glsl_Scale(mv_mat,scale,scale,scale);
  glsl_MultMatrix(mv_mat,rot_mat);

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

    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glUniformMatrix4fv(se->poly_mv_index,1,GL_FALSE,mv_mat);
    glUniformMatrix4fv(se->poly_proj_index,1,GL_FALSE,p_mat);

    glUniform4fv(se->poly_front_ambient_index,1,mat_diff_white);
    glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_white);
    glUniform4fv(se->poly_back_ambient_index,1,mat_diff_white);
    glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_white);
    glVertexAttrib4f(se->poly_colorf_index,1.0f,1.0f,1.0f,1.0f);
    glVertexAttrib4f(se->poly_colorb_index,1.0f,1.0f,1.0f,1.0f);

    if (se->display_mode[pass] == DISP_SURFACE)
    {
      glDisable(GL_CULL_FACE);
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
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
    }
    else /* se->display_mode[pass] == DISP_TRANSPARENT */
    {
      glDisable(GL_CULL_FACE);
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA,GL_ONE);
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
    }

    if (se->colors[pass] == COLORS_TWOSIDED)
    {
      if (se->display_mode[pass] == DISP_TRANSPARENT)
      {
        glUniform4fv(se->poly_front_ambient_index,1,mat_diff_trans_front);
        glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_trans_front);
        glUniform4fv(se->poly_back_ambient_index,1,mat_diff_trans_back);
        glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_trans_back);
      }
      else
      {
        glUniform4fv(se->poly_front_ambient_index,1,mat_diff_front);
        glUniform4fv(se->poly_front_diffuse_index,1,mat_diff_front);
        glUniform4fv(se->poly_back_ambient_index,1,mat_diff_back);
        glUniform4fv(se->poly_back_diffuse_index,1,mat_diff_back);
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

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f,1.0f);

    glEnableVertexAttribArray(se->poly_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_pos_buffer);
    glVertexAttribPointer(se->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(se->poly_normal_index);
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_normal_buffer);
    glVertexAttribPointer(se->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(se->poly_vertex_tex_index);
    glBindBuffer(GL_ARRAY_BUFFER,se->vertex_tex_coord_buffer);
    glVertexAttribPointer(se->poly_vertex_tex_index,2,GL_FLOAT,GL_FALSE,0,0);

    if (se->colors[pass] != COLORS_TWOSIDED &&
        se->colors[pass] != COLORS_EARTH)
    {
      glEnableVertexAttribArray(se->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorf_buffer[pass]);
      glVertexAttribPointer(se->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(se->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorb_buffer[pass]);
      glVertexAttribPointer(se->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);
    }
#ifdef VERTEXATTRIBARRAY_WORKAROUND
    else /* se->colors[pass] == COLORS_TWOSIDED ||
            se->colors[pass] == COLORS_EARTH */
    {
      glEnableVertexAttribArray(se->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorf_buffer[pass]);
      glVertexAttribPointer(se->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(se->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,se->vertex_colorb_buffer[pass]);
      glVertexAttribPointer(se->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);
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
      polys += 2*num_phi*(num_theta+1);
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
      polys += num_theta*(num_phi+1);
    }
    else /* se->appearance[pass] == APPEARANCE_MERIDIAN_BANDS */
    {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->meridian_indices_buffer);
      for (i=0; i<se->num_meridian_strips; i++)
      {
        index_offset = se->num_meridian_triangles*i*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,se->num_meridian_triangles,
                       GL_UNSIGNED_INT,(const GLvoid *)index_offset);
      }
      polys += num_phi*(num_theta+1);
    }

    glDisableVertexAttribArray(se->poly_pos_index);
    glDisableVertexAttribArray(se->poly_normal_index);
    if (se->colors[pass] != COLORS_TWOSIDED &&
        se->colors[pass] != COLORS_EARTH)
    {
      glDisableVertexAttribArray(se->poly_colorf_index);
      glDisableVertexAttribArray(se->poly_colorb_index);
    }
#ifdef VERTEXATTRIBARRAY_WORKAROUND
    else /* se->colors[pass] == COLORS_TWOSIDED ||
            se->colors[pass] == COLORS_EARTH */
    {
      glDisableVertexAttribArray(se->poly_colorf_index);
      glDisableVertexAttribArray(se->poly_colorb_index);
    }
#endif /* VERTEXATTRIBARRAY_WORKAROUND */
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f,0.0f);

    glUseProgram(0);

    if (se->graticule[pass])
    {
      glUseProgram(se->line_shader_program);

      glUniformMatrix4fv(se->line_mv_index,1,GL_FALSE,mv_mat);
      glUniformMatrix4fv(se->line_proj_index,1,GL_FALSE,p_mat);
      glUniform4f(se->line_color_index,1.0f,1.0f,1.0f,1.0f);

      glLineWidth(2.0f);

      glEnableVertexAttribArray(se->line_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,se->vertex_pos_buffer);
      glVertexAttribPointer(se->line_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,se->line_indices_buffer);

      index_offset = 0;
      glDrawElements(GL_LINES,se->num_lines,GL_UNSIGNED_INT,
                     (const void *)index_offset);

      glDisableVertexAttribArray(se->line_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

      glUseProgram(0);
    }
 
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


void init_sphereeversion_analytic(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (deform_speed == 0.0f)
    deform_speed = 10.0f;

  se->alpha = frand(120.0)-60.0;
  se->beta = frand(120.0)-60.0;
  se->delta = frand(360.0);

  se->eta_min = BEDNORZ_ETA_MIN;
  se->beta_max = BEDNORZ_BETA_MAX;

  se->anim_state = ANIM_DEFORM;
  se->tau = BEDNORZ_TAU_MAX;
  se->defdir = -1;
  se->turn_step = 0;
  se->num_turn = 0;

  se->offset3d[0] = 0.0f;
  se->offset3d[1] = 0.0f;
  se->offset3d[2] = -1.8f;

  se->sp = calloc(3*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->sn = calloc(3*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->colf[0] = calloc(4*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->colf[1] = calloc(4*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->colb[0] = calloc(4*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->colb[1] = calloc(4*(NUMPH+1)*(NUMTH+1),sizeof(float));

#ifdef HAVE_GLSL
  se->solid_indices = calloc(2*NUMPH*(NUMTH+1),sizeof(float));
  se->parallel_indices = calloc(NUMPH*(NUMTH+1),sizeof(float));
  se->meridian_indices = calloc((NUMPH+1)*NUMTH,sizeof(float));
  se->line_indices = calloc(4*NUMPH*NUMTH/NUMGRID,sizeof(float));

  init_glsl(mi);
#endif /* HAVE_GLSL */

  setup_surface_colors(mi,-M_PI_F,M_PI_F,-M_PI_F/2.0f,M_PI_F/2.0f,NUMPH,NUMTH);
#ifdef HAVE_GLSL
  se->tex = calloc(2*(NUMPH+1)*(NUMTH+1),sizeof(float));
  gen_textures(mi);
  setup_tex_coords(mi,-M_PI_F,M_PI_F,-M_PI_F/2.0f,M_PI_F/2.0f,NUMPH,NUMTH);
#endif /* HAVE_GLSL */
}


void display_sphereeversion_analytic(ModeInfo *mi)
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
      se->tau += se->defdir*deform_speed*0.001;
      if (se->tau < BEDNORZ_TAU_MIN)
      {
        se->tau = BEDNORZ_TAU_MIN;
        se->defdir = -se->defdir;
        se->anim_state = ANIM_TURN;
      }
      if (se->tau > BEDNORZ_TAU_MAX)
      {
        se->tau = BEDNORZ_TAU_MAX;
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
        se->graticule[1] = se->graticule[0];
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
        if (se->random_graticule)
          se->graticule[0] = random() & 1;
        if (se->random_g)
          se->g = random() % 4 + 2;
        setup_surface_colors(mi,-M_PI_F,M_PI_F,-M_PI_F/2.0f,M_PI_F/2.0f,
                             NUMPH,NUMTH);
      }
    }
    else /* se->anim_state == ANIM_TURN */
    {
      t = (float)se->turn_step/(float)se->num_turn;
      /* Apply an easing function to t. */
      t = (3.0f-2.0f*t)*t*t;
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

#ifdef HAVE_GLSL
  if (se->use_shaders)
    mi->polygon_count = bednorz_sphere_eversion_pf(mi,-M_PI_F,M_PI_F,
                                                   -M_PI_F/2.0f,M_PI_F/2.0f,
                                                   NUMPH,NUMTH,NUMBDIST,
                                                   NUMBDIR,NUMGRID);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = bednorz_sphere_eversion_ff(mi,-M_PI_F,M_PI_F,
                                                   -M_PI_F/2.0f,M_PI_F/2.0f,
                                                   NUMPH,NUMTH,NUMBDIST,
                                                   NUMBDIR,NUMGRID);
}


#endif /* USE_GL */
