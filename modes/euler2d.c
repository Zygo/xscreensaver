/* -*- Mode: C; tab-width: 4 -*- */
/* euler2d --- 2 Dimensional Incompressible Inviscid Fluid Flow */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)euler2d.c 4.17 2000/16/06 xlockmore";

#endif

/*
 * Copyright (c) 2000 by Stephen Montgomery-Smith <stephen@math.missouri.edu>
 *
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
 * Revision History:
 * 05-Jun-00: Adapted from flow.c Copyright (c) 1996 by Tim Auckland
 * 18-Jul-96: Adapted from swarm.c Copyright (c) 1991 by Patrick J. Naughton.
 * 31-Aug-90: Adapted from xswarm by Jeff Butterworth. (butterwo@ncsc.org)
 */

/*
 * The mathematical aspects of this program are discussed in the file
 * euler2d.tex.
 */

#ifdef STANDALONE
#define PROGCLASS "Euler2d"
#define HACK_INIT init_euler2d
#define HACK_DRAW draw_euler2d
#define euler2d_opts xlockmore_opts
#define DEFAULTS "*delay: 1000 \n" \
"*count: 1024 \n" \
"*cycles: 3000 \n" \
"*ncolors: 200 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_euler2d

#define DEF_EULERTAIL "10"

#define DEBUG_FLOATING_OVERFLOW 0
#define DEBUG_POINTED_REGION    0

static int tail_len;
static int variable_boundary = 1;

static XrmOptionDescRec opts[] =
{
  {(char* ) "-eulertail", (char *) ".euler2d.eulertail",
   XrmoptionSepArg, (caddr_t) NULL},
};
static argtype vars[] =
{
  {(caddr_t *) &tail_len, (char *) "eulertail",
   (char *) "EulerTail", (char *) DEF_EULERTAIL, t_Int},
};
static OptionStruct desc[] =
{
  {(char *) "-eulertail len", (char *) "Length of Euler2d tails"},
};

ModeSpecOpt euler2d_opts =
{sizeof opts / sizeof opts[0], opts,
 sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   euler2d_description = {
	"euler2d", "init_euler2d", "draw_euler2d", "release_euler2d",
	"refresh_euler2d", "init_euler2d", NULL, &euler2d_opts,
	1000, 1024, 3000, 1, 64, 1.0, "",
	"Simulates 2D incompressible invisid fluid.", 0, NULL
};

#endif

#define balance_rand(v)	((LRAND()/MAXRAND*(v))-((v)/2))	/* random around 0 */
#define positive_rand(v)	(LRAND()/MAXRAND*(v))	/* positive random */

#define number_of_vortex_points 20
#define delta_t 0.001

#define DO_SUBTLE_PERTURB
/*
  This feature, if switched on, reduces (but does not remove) problems
  where particles near the edge of the boundary bounce around.  The
  way it works is to map points x in the unit disk to points y
  in the plane, where y = f(|x|) x, with f(t) = -log(1-t)/t.
*/

#define n_bound_p 500
#define deg_p 6

typedef struct {
	int        width;
	int        height;
	int        count;
	double     xshift,yshift,scale;
	double     radius;

        int        N;
        int        Nvortex;

/*
x[2n+0] = x coord for nth point
x[2n+1] = y coord for nth point
w[n] = vorticity at nth point
*/

	double     *x;
	double     *w;

	double     *diffx;

	double     *olddiffs[3];
	int        olddiffindex[3];
#define olddiff(i) sp->olddiffs[sp->olddiffindex[i]]
	double     *k1, *k2, *k3, *k4;
        double     *tempx;
#ifdef DO_SUBTLE_PERTURB
	double     *tempk;
#endif

/* sometimes in our calculations we get nan.  If that
   happens with the point x[2*i+0], x[2*i+1], we set dead[i].
*/
	short      *dead;

	XSegment   *csegs;
	int         cnsegs;
	XSegment   *old_segs;
	int        *nold_segs;
	int         c_old_seg;
	int	    boundary_color;
	int         hide_vortex;
	short      *lastx;

	double      p_coef[2*(deg_p-1)];
	XSegment   *boundary;

} euler2dstruct;

static euler2dstruct *euler2ds = NULL;

/*
  greens_fun calculates the effect of a vortex point at (a1,a2)
  at the point (z1,z2), returning the vector field in
  (u1,u2).

  In the plane, this is given by the formula

  z = x-a/|x-a|^2  or zero if z=a.

  However, in the unit disk we have to subtract from the
  above:

  x-as/|x-as|^2

  where as = a/|a|^2 is the reflection of a about the unit circle.
*/

static void
greens_fun(double z1, double z2, double a1, double a2, double *u1, double *u2, 
           short *dead)
{
  double na,za1,za2,nza,as1,as2,zas1=0.0,zas2=0.0,nzas=0.0;
  int azero, self;

  na = a1*a1+a2*a2;
  azero = na < 1e-10;
  za1 = z1 - a1;
  za2 = z2 - a2;
  nza = za1*za1+za2*za2;
  self = nza < 1e-4;
  if (!azero)
  {
/* as is the reflection of a in the unit sphere */
    as1 = a1/na;
    as2 = a2/na;
    zas1 = z1 - as1;
    zas2 = z2 - as2;
    nzas = zas1*zas1+zas2*zas2;
    if (nzas < 1e-5)
      *dead = 1;
  }

  if (!*dead)
  {
    *u1 = - (self?0.0:(-za2/nza)) - (azero?0.0:(zas2/nzas));
    *u2 =   (self?0.0:(-za1/nza)) + (azero?0.0:(zas1/nzas));
  }
  else
    *u1 = *u2 = 0.0;
}

/*
  If variable_boundary == 1, then we make a variable boundary.
  The way this is done is to map the unit disk under a 
  polynomial p, where 
  p(z) = z + c_2 z^2 + ... + c_n z^n
  where n = deg_p.  sp->p_coef contains the complex numbers
  c_2, c_3, ... c_n.
*/

#define add(a1,a2,b1,b2) (a1)+=(b1);(a2)+=(b2)
#define mult(a1,a2,b1,b2) temp=(a1)*(b1)-(a2)*(b2); \
                          (a2)=(a1)*(b2)+(a2)*(b1);(a1)=temp

static void
calc_p(double *p1, double *p2, double z1, double z2, double p_coef[])
{
  int i;
  double temp;

  *p1=0;
  *p2=0;
  for(i=deg_p;i>=2;i--)
  {
    add(*p1,*p2,p_coef[(i-2)*2],p_coef[(i-2)*2+1]);
    mult(*p1,*p2,z1,z2);
  }
  add(*p1,*p2,1,0);
  mult(*p1,*p2,z1,z2);
}

/* Calculate |p'(z)|^2 */
static double
calc_mod_dp2(double z1, double z2, double p_coef[])
{
  int i;
  double temp,p1,p2;

  p1=0;
  p2=0;
  for(i=deg_p;i>=2;i--)
  {
    add(p1,p2,i*p_coef[(i-2)*2],i*p_coef[(i-2)*2+1]);
    mult(p1,p2,z1,z2);
  }
  add(p1,p2,1,0);
  return p1*p1+p2*p2;
}

static void
derivs(double *x, euler2dstruct *sp)
{
  int i,j;
  double u1,u2;
  double mod_dp2;

  memset(sp->diffx,0,sizeof(double)*2*sp->N);

  for (i=0;i<sp->N;i++) if (!sp->dead[i])
  {
    for (j=0;j<sp->Nvortex;j++) if (!sp->dead[j])
    {
      greens_fun(x[2*i+0],x[2*i+1],x[2*j+0],x[2*j+1],&u1,&u2,sp->dead+i);
      if (!sp->dead[i])
      {
        sp->diffx[2*i+0] += u1*sp->w[j];
        sp->diffx[2*i+1] += u2*sp->w[j];
      }
#if DEBUG_FLOATING_OVERFLOW
      else
        printf("dead greens_fun %d\n",i);fflush(stdout);
#endif
    }
    if (!sp->dead[i] && variable_boundary)
    {
      mod_dp2=calc_mod_dp2(x[2*i+0],x[2*i+1],sp->p_coef);
      if (mod_dp2 < 1e-5)
      {
#if DEBUG_FLOATING_OVERFLOW
        printf("dead mod_dp2 %d\n",i);fflush(stdout);
#endif
        sp->dead[i] = 1;
      }
      else
      {
        sp->diffx[2*i+0] /= mod_dp2;
        sp->diffx[2*i+1] /= mod_dp2;
      }
    }
  }
}

#ifdef DO_SUBTLE_PERTURB
/* This is described above */

static void
subtle_perturb(double ret[], double x[], double k[], euler2dstruct *sp)
{
  int i;
  double t1,t2,mag,mag2,mlog1mmag,memmagdmag,xdotk;
  double d1,d2,x1,x2;

  for (i=0;i<sp->N;i++) if (!sp->dead[i])
  {
    x1 = x[2*i+0];
    x2 = x[2*i+1];
    mag2 = x1*x1 + x2*x2;
    if (mag2 < 1e-10)
    {
      ret[2*i+0] = x1+k[2*i+0];
      ret[2*i+1] = x2+k[2*i+1];
    }
    else if (mag2 > 1-1e-5)
    {
#if DEBUG_FLOATING_OVERFLOW
      printf("dead mag2 %d\n",i);fflush(stdout);
#endif
      sp->dead[i] = 1;
    }
    else
    {
      mag = sqrt(mag2);
      mlog1mmag = -log(1-mag);
      xdotk = x1*k[2*i+0] + x2*k[2*i+1];
      t1 = (x1 + k[2*i+0])*mlog1mmag/mag + x1*xdotk*(1.0/(1-mag)-mlog1mmag/mag)/mag/mag;
      t2 = (x2 + k[2*i+1])*mlog1mmag/mag + x2*xdotk*(1.0/(1-mag)-mlog1mmag/mag)/mag/mag;
      mag = sqrt(t1*t1+t2*t2);
      if (mag > 11.5 /* log(1e5) */)
      {
#if DEBUG_FLOATING_OVERFLOW
        printf("dead exp %d %g\n",i,mag);fflush(stdout);
#endif
        sp->dead[i] = 1;
      }
      else
      {
        memmagdmag = (mag>1e-5) ? ((1.0-exp(-mag))/mag) : (1-mag/2.0);
        ret[2*i+0] = t1*memmagdmag;
        ret[2*i+1] = t2*memmagdmag;
      }
    }
#if DEBUG_FLOATING_OVERFLOW
    if (!sp->dead[i] && (isnan(ret[2*i+0]) || isnan(ret[2*i+1])))
    {
      printf("dead isnan %d\n",i);fflush(stdout);
      sp->dead[i] = 1;
    }
    else
#endif
    if (!sp->dead[i])
    {
      d1 = ret[2*i+0]-x1;
      d2 = ret[2*i+1]-x2;
      if (d1*d1+d2*d2 > 0.1)
      {
        sp->dead[i] = 1;
#if DEBUG_FLOATING_OVERFLOW
        printf("dead big %d\n",i);fflush(stdout);
#endif
      }
    }
  }
}
#endif

static void
runge_kutta_4(euler2dstruct *sp)
{
  int i;

#ifdef DO_SUBTLE_PERTURB
  derivs(sp->x,sp);
  for (i=0;i<2*sp->N;i++) sp->k1[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->tempk[i] = sp->k1[i]/2;
  subtle_perturb(sp->tempx,sp->x,sp->tempk,sp);
  derivs(sp->tempx,sp);
  for (i=0;i<2*sp->N;i++) sp->k2[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->tempk[i] = sp->k2[i]/2;
  subtle_perturb(sp->tempx,sp->x,sp->tempk,sp);
  derivs(sp->tempx,sp);
  for (i=0;i<2*sp->N;i++) sp->k3[i] = delta_t*sp->diffx[i];
  subtle_perturb(sp->tempx,sp->x,sp->k3,sp);
  derivs(sp->tempx,sp);
  for (i=0;i<2*sp->N;i++) sp->k4[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->tempk[i] = (sp->k1[i]+2*sp->k2[i]+2*sp->k3[i]+sp->k4[i])/6;
  subtle_perturb(sp->x,sp->x,sp->k3,sp);
#else
  derivs(sp->x,sp);
  for (i=0;i<2*sp->N;i++) sp->k1[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->tempx[i] = sp->x[i] + sp->k1[i]/2;
  derivs(sp->tempx,sp);
  for (i=0;i<2*sp->N;i++) sp->k2[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->tempx[i] = sp->x[i] + sp->k2[i]/2;
  derivs(sp->tempx,sp);
  for (i=0;i<2*sp->N;i++) sp->k3[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->tempx[i] = sp->x[i] + sp->k2[i];
  derivs(sp->tempx,sp);
  for (i=0;i<2*sp->N;i++) sp->k4[i] = delta_t*sp->diffx[i];
  for (i=0;i<2*sp->N;i++) sp->x[i] += (sp->k1[i]+2*sp->k2[i]+2*sp->k3[i]+sp->k4[i])/6;
#endif
}

static void
pushdiff(euler2dstruct *sp)
{
  int i,t;

  t = sp->olddiffindex[2];
  for (i=1;i>=0;i--)
    sp->olddiffindex[i+1] = sp->olddiffindex[i];
  sp->olddiffindex[0] = t;
  memcpy(olddiff(0),sp->diffx,sizeof(double)*2*sp->N);
}

static void
error_predictor(euler2dstruct *sp)
{
  int i;

#ifdef DO_SUBTLE_PERTURB
  for (i=0;i<2*sp->N;i++)
    sp->tempk[i] = delta_t/12 * (23*olddiff(0)[i]-16*olddiff(1)[i]+5*olddiff(2)[i]);
  subtle_perturb(sp->x,sp->x,sp->tempk,sp);
  derivs(sp->x,sp);
  for (i=0;i<2*sp->N;i++)
    sp->tempk[i] = delta_t/24 * (9*sp->diffx[i]+19*olddiff(0)[i]-5*olddiff(1)[i]+olddiff(2)[i]);
  subtle_perturb(sp->x,sp->x,sp->tempk,sp);
#else
  for (i=0;i<2*sp->N;i++)
    sp->x[i] += delta_t/12 * (23*olddiff(0)[i]-16*olddiff(1)[i]+5*olddiff(2)[i]);
  derivs(sp->x,sp);
  for (i=0;i<2*sp->N;i++)
    sp->x[i] += delta_t/24 * (9*sp->diffx[i]+19*olddiff(0)[i]-5*olddiff(1)[i]+olddiff(2)[i]);
#endif
}

static void
ode_solve(euler2dstruct *sp)
{
  if (sp->count <= 3)
    runge_kutta_4(sp);
  else
    error_predictor(sp);
  derivs(sp->x,sp);
  pushdiff(sp);
}

#define deallocate(p) if (p != NULL) {(void) free((void *) p); p = NULL; }

static void
deallocateall(euler2dstruct *sp)
{
	int i;

	deallocate(sp->csegs);
	deallocate(sp->old_segs);
	deallocate(sp->nold_segs);
	deallocate(sp->lastx);
	deallocate(sp->x);
	deallocate(sp->diffx);
	deallocate(sp->w);
	for (i=0;i<3;i++) {
		deallocate(sp->olddiffs[i]);
	}
	deallocate(sp->k1);
	deallocate(sp->k2);
	deallocate(sp->k3);
	deallocate(sp->k4);
#ifdef DO_SUBTLE_PERTURB
	deallocate(sp->tempk);
#endif
	deallocate(sp->tempx);
	deallocate(sp->dead);
	deallocate(sp->boundary);
}

void
init_euler2d(ModeInfo * mi)
{
	euler2dstruct *sp;
	static int  allocated = 0;
	int         i,k,n,np;
	double      r,theta,x,y,w;
	double      mag,xlow,xhigh,ylow,yhigh,xscale,yscale,p1,p2;

	if (euler2ds == NULL) {
		if ((euler2ds = (euler2dstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (euler2dstruct))) == NULL)
			return;
	}
	sp = &euler2ds[MI_SCREEN(mi)];

	sp->boundary_color = NRAND(MI_NPIXELS(mi));
	sp->hide_vortex = NRAND(4) != 0;

	sp->count = 0;

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

	sp->N = MI_COUNT(mi)+number_of_vortex_points;
	sp->Nvortex = number_of_vortex_points;

	if (tail_len < 1) { /* minimum tail */
	  tail_len = 1;
	}
	if (tail_len > MI_CYCLES(mi)) { /* maximum tail */
	  tail_len = MI_CYCLES(mi);
	}

	/* Clear the background. */
	MI_CLEARWINDOW(mi);
	 
	if(!allocated || sp->N != allocated){ /* reallocate */
	  deallocateall(sp);
	}

	/* Allocate memory. */

	if (!sp->csegs) {
		sp->csegs = (XSegment *) malloc(sizeof (XSegment) * sp->N );
		sp->old_segs = (XSegment *) malloc(sizeof (XSegment) * sp->N * tail_len);
		sp->nold_segs = (int *) malloc(sizeof(int) * tail_len);
		sp->lastx = (short *) malloc(sizeof (short) * sp->N * 2);
		sp->x = (double *) malloc(sizeof (double) * sp->N * 2);
		sp->diffx = (double *) malloc(sizeof (double) * sp->N * 2);
		sp->w = (double *) malloc(sizeof (double) * sp->Nvortex);
		for (i=0;i<3;i++) {
			sp->olddiffs[i] = (double *) malloc(sizeof (double) * sp->N * 2);
		}
		sp->k1 = (double *) malloc(sizeof (double) * sp->N * 2);
		sp->k2 = (double *) malloc(sizeof (double) * sp->N * 2);
		sp->k3 = (double *) malloc(sizeof (double) * sp->N * 2);
		sp->k4 = (double *) malloc(sizeof (double) * sp->N * 2);
#ifdef DO_SUBTLE_PERTURB
		sp->tempk = (double *) malloc(sizeof (double) * sp->N * 2);
#endif
		sp->tempx = (double *) malloc(sizeof (double) * sp->N * 2);
		sp->dead = (short *) malloc(sizeof (short) * sp->N);
		sp->boundary = (XSegment *) malloc(sizeof (double) * n_bound_p);
	}
	for (i=0;i<tail_len;i++) {
		sp->nold_segs[i] = 0;
	}
	sp->c_old_seg = 0;
	for (i=0;i<3;i++) {
		sp->olddiffindex[i]=i;
	}
	memset(sp->dead,0,sp->N*sizeof(short));

	if (variable_boundary)
	{
	/* Initialize polynomial p */
/*
  The polynomial p(z) = z + c_2 z^2 + ... c_n z^n needs to be
  a bijection of the unit disk onto its image.  This is achieved
  by insisting that sum_{k=2}^n k |c_k| <= 1.  Actually we set
  the inequality to be equality (to get more interesting shapes).
*/
		mag = 0;
		for(k=2;k<=deg_p;k++)
		{
			r = positive_rand(1.0/k);
			theta = balance_rand(2*M_PI);
			sp->p_coef[2*(k-2)+0]=r*cos(theta);
			sp->p_coef[2*(k-2)+1]=r*sin(theta);
			mag += k*r;
		}
		if (mag > 0.0001) for(k=2;k<=deg_p;k++)
		{
			sp->p_coef[2*(k-2)+0] /= mag;
			sp->p_coef[2*(k-2)+1] /= mag;
		}

#if DEBUG_POINTED_REGION
		for(k=2;k<=deg_p;k++){
			sp->p_coef[2*(k-2)+0]=0;
			sp->p_coef[2*(k-2)+1]=0;
		}
		sp->p_coef[2*(6-2)+0] = 1.0/6.0;
#endif

	/* Initialize boundary and scaling factors */
		xlow = 1e5;
		xhigh = -1e5;
		ylow = 1e5;
		yhigh = -1e5;
		for(k=0;k<n_bound_p;k++)
		{
			calc_p(&p1,&p2,cos((double)k/(n_bound_p)*2*M_PI),sin((double)k/(n_bound_p)*2*M_PI),sp->p_coef);
			if (p1<xlow) xlow = p1;
			if (p1>xhigh) xhigh = p1;
			if (p2<ylow) ylow = p2;
			if (p2>yhigh) yhigh = p2;
		}
		xscale = (sp->width-5.0)/(xhigh-xlow);
		yscale = (sp->height-5.0)/(yhigh-ylow);
		if (xscale > yscale)
			sp->scale = yscale;
		else
			sp->scale = xscale;
		sp->xshift = -(xlow+xhigh)/2.0*sp->scale+sp->width/2;
		sp->yshift = -(ylow+yhigh)/2.0*sp->scale+sp->height/2;

		for(k=0;k<n_bound_p;k++)
		{

			calc_p(&p1,&p2,cos((double)k/(n_bound_p)*2*M_PI),sin((double)k/(n_bound_p)*2*M_PI),sp->p_coef);
			sp->boundary[k].x1 = (short)(p1*sp->scale+sp->xshift);
			sp->boundary[k].y1 = (short)(p2*sp->scale+sp->yshift);
		}
		for(k=1;k<n_bound_p;k++)
		{
			sp->boundary[k].x2 = sp->boundary[k-1].x1;
			sp->boundary[k].y2 = sp->boundary[k-1].y1;
		}
		sp->boundary[0].x2 = sp->boundary[n_bound_p-1].x1;
		sp->boundary[0].y2 = sp->boundary[n_bound_p-1].y1;
	}
	else
	{
		if (sp->width>sp->height)
			sp->radius = sp->height/2.0-5.0;
		else
			sp->radius = sp->width/2.0-5.0;
	}

	/* Initialize point positions */

	for (i=sp->Nvortex;i<sp->N;i++) {
		do {
			r = sqrt(positive_rand(1.0));
			theta = balance_rand(2*M_PI);
			sp->x[2*i+0]=r*cos(theta);
			sp->x[2*i+1]=r*sin(theta);
		/* This is to make sure the initial distribution of points is uniform */
		} while (variable_boundary && 
                          calc_mod_dp2(sp->x[2*i+0],sp->x[2*i+1],sp->p_coef)
                          < positive_rand(4));
	}

	n = NRAND(4)+2;
	/* number of vortex points with negative vorticity */
	if (n%2) {
		np = NRAND(n+1); 
	}
	else {
		/* if n is even make sure that np==n/2 is twice as likely
		   as the other possibilities. */
		np = NRAND(n+2);
		if (np==n+1) np=n/2;
	}
	for(k=0;k<n;k++)
	{
		r = sqrt(positive_rand(0.77));
		theta = balance_rand(2*M_PI);
		x=r*cos(theta);
		y=r*sin(theta);
		r = 0.02+positive_rand(0.1);
		w = (2*(k<np)-1)*2.0/sp->Nvortex;
		for (i=sp->Nvortex*k/n;i<sp->Nvortex*(k+1)/n;i++) {
			theta = balance_rand(2*M_PI);
			sp->x[2*i+0]=x + r*cos(theta);
			sp->x[2*i+1]=y + r*sin(theta);
			sp->w[i]=w;
		}
	}
}

void
draw_euler2d(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	euler2dstruct *sp = &euler2ds[MI_SCREEN(mi)];
	int         b, col, n_non_vortex_segs;
	double      p1,p2;

	MI_IS_DRAWN(mi) = True;

	ode_solve(sp);

	sp->cnsegs = 0;
	for(b=sp->Nvortex;b<sp->N;b++) if(!sp->dead[b])
	{
		sp->csegs[sp->cnsegs].x1 = sp->lastx[2*b+0];
		sp->csegs[sp->cnsegs].y1 = sp->lastx[2*b+1];
		if (variable_boundary)
		{
			calc_p(&p1,&p2,sp->x[2*b+0],sp->x[2*b+1],sp->p_coef);
			sp->csegs[sp->cnsegs].x2 = (short)(p1*sp->scale+sp->xshift);
			sp->csegs[sp->cnsegs].y2 = (short)(p2*sp->scale+sp->yshift);
		}
		else
		{
			sp->csegs[sp->cnsegs].x2 = (short)(sp->x[2*b+0]*sp->radius+sp->width/2);
			sp->csegs[sp->cnsegs].y2 = (short)(sp->x[2*b+1]*sp->radius+sp->height/2);
		}
		sp->lastx[2*b+0] = sp->csegs[sp->cnsegs].x2;
		sp->lastx[2*b+1] = sp->csegs[sp->cnsegs].y2;
		sp->cnsegs++;
	}
	n_non_vortex_segs = sp->cnsegs;

	if (!sp->hide_vortex) for(b=0;b<sp->Nvortex;b++) if(!sp->dead[b])
	{
		sp->csegs[sp->cnsegs].x1 = sp->lastx[2*b+0];
		sp->csegs[sp->cnsegs].y1 = sp->lastx[2*b+1];
		if (variable_boundary)
		{
			calc_p(&p1,&p2,sp->x[2*b+0],sp->x[2*b+1],sp->p_coef);
			sp->csegs[sp->cnsegs].x2 = (short)(p1*sp->scale+sp->xshift);
			sp->csegs[sp->cnsegs].y2 = (short)(p2*sp->scale+sp->yshift);
		}
		else
		{
			sp->csegs[sp->cnsegs].x2 = (short)(sp->x[2*b+0]*sp->radius+sp->width/2);
			sp->csegs[sp->cnsegs].y2 = (short)(sp->x[2*b+1]*sp->radius+sp->height/2);
		}
		sp->lastx[2*b+0] = sp->csegs[sp->cnsegs].x2;
		sp->lastx[2*b+1] = sp->csegs[sp->cnsegs].y2;
		sp->cnsegs++;
	}

	if (sp->count) {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

		XDrawSegments(display, window, gc, sp->old_segs+sp->c_old_seg*sp->N, sp->nold_segs[sp->c_old_seg]);

		if (MI_NPIXELS(mi) > 2){ /* render colour */
			for (col = 0; col < MI_NPIXELS(mi); col++) {
			  int start = col*n_non_vortex_segs/MI_NPIXELS(mi);
			  int finish = (col+1)*n_non_vortex_segs/MI_NPIXELS(mi);
			  XSetForeground(display, gc, MI_PIXEL(mi, col));
			  XDrawSegments(display, window, gc,sp->csegs+start, finish-start);
			}
			if (!sp->hide_vortex) {
			  XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
			  XDrawSegments(display, window, gc,sp->csegs+n_non_vortex_segs, sp->cnsegs-n_non_vortex_segs);
			}

		} else { 		/* render mono */
		  XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		  XDrawSegments(display, window, gc,
						sp->csegs, sp->cnsegs);
		}

		if (MI_NPIXELS(mi) > 2) /* render colour */
			XSetForeground(display, gc, MI_PIXEL(mi, sp->boundary_color));
		else
			XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
		if (variable_boundary)
			XDrawSegments(display, window, gc,
						sp->boundary, n_bound_p);
		else
			XDrawArc(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
				 sp->width/2 - (int) sp->radius - 1, sp->height/2 - (int) sp->radius -1,
				 (int) (2*sp->radius) + 2, (int) (2* sp->radius) + 2, 0, 64*360);

		/* Copy to erase-list */
		memcpy(sp->old_segs+sp->c_old_seg*sp->N, sp->csegs, sp->cnsegs*sizeof(XSegment));
		sp->nold_segs[sp->c_old_seg] = sp->cnsegs;
		sp->c_old_seg++;
		if (sp->c_old_seg >= tail_len)
		  sp->c_old_seg = 0;
	}

	if (++sp->count > MI_CYCLES(mi)) /* pick a new flow */
		init_euler2d(mi);

}

void
release_euler2d(ModeInfo * mi)
{
	if (euler2ds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			deallocateall(&euler2ds[screen]);
		}
		(void) free((void *) euler2ds);
		euler2ds = NULL;
	}
}

void
refresh_euler2d(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_euler2d */
