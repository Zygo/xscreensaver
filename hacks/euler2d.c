/* -*- Mode: C; tab-width: 4 -*- */
/* euler2d --- 2 Dimensional Incompressible Inviscid Fluid Flow */

#if 0
static const char sccsid[] = "@(#)euler2d.c	5.00 2000/11/01 xlockmore";
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
 * 04-Nov-2000: Added an option eulerpower.  This allows for example the
 *              quasi-geostrophic equation by setting eulerpower to 2.
 * 01-Nov-2000: Allocation checks.
 * 10-Sep-2000: Added optimizations, and removed subtle_perturb, by stephen.
 * 03-Sep-2000: Changed method of solving ode to Adams-Bashforth of order 2.
 *              Previously used a rather compilcated method of order 4.
 *              This doubles the speed of the program.  Also it seems
 *              to have improved numerical stability.  Done by stephen.
 * 27-Aug-2000: Added rotation of region to maximize screen fill by stephen.
 * 05-Jun-2000: Adapted from flow.c Copyright (c) 1996 by Tim Auckland
 * 18-Jul-1996: Adapted from swarm.c Copyright (c) 1991 by Patrick J. Naughton.
 * 31-Aug-1990: Adapted from xswarm by Jeff Butterworth. (butterwo@ncsc.org)
 */

/*
 * The mathematical aspects of this program are discussed in the file
 * euler2d.tex.
 */

#ifdef STANDALONE
# define MODE_euler2d
# define DEFAULTS	"*delay:   10000 \n" \
					"*count:   1024  \n" \
					"*cycles:  3000  \n" \
					"*ncolors: 64    \n" \
					"*fpsSolid: true    \n" \
					"*ignoreRotation: True \n" \

# define SMOOTH_COLORS
# define release_euler2d 0
# define reshape_euler2d 0
# define euler2d_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_euler2d

#define DEF_EULERTAIL "10"

#define DEBUG_POINTED_REGION    0

static int tail_len;
static int variable_boundary = 1;
static float power = 1;

static XrmOptionDescRec opts[] =
{
  {"-eulertail", ".euler2d.eulertail",   XrmoptionSepArg, NULL},
  {"-eulerpower", ".euler2d.eulerpower", XrmoptionSepArg, NULL},
};
static argtype vars[] =
{
  {&tail_len, "eulertail",
   "EulerTail", (char *) DEF_EULERTAIL, t_Int},
  {&power, "eulerpower",
   "EulerPower", "1", t_Float},
};
static OptionStruct desc[] =
{
  {"-eulertail len", "Length of Euler2d tails"},
  {"-eulerpower power", "power of interaction law for points for Euler2d"},
};

ENTRYPOINT ModeSpecOpt euler2d_opts =
{sizeof opts / sizeof opts[0], opts,
 sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   euler2d_description = {
	"euler2d", "init_euler2d", "draw_euler2d", (char *) NULL,
	"refresh_euler2d", "init_euler2d", "free_euler2d", &euler2d_opts,
	1000, 1024, 3000, 1, 64, 1.0, "",
	"Simulates 2D incompressible invisid fluid.", 0, NULL
};

#endif

#define balance_rand(v)	((LRAND()/MAXRAND*(v))-((v)/2))	/* random around 0 */
#define positive_rand(v)	(LRAND()/MAXRAND*(v))	/* positive random */

#define number_of_vortex_points 20

#define n_bound_p 500
#define deg_p 6

static double delta_t;

typedef struct {
	int        width;
	int        height;
	int        count;
	double     xshift,yshift,scale;
	double     xshift2,yshift2;
	double     radius;

        int        N;
        int        Nvortex;

/*  x[2i+0] = x coord for nth point
    x[2i+1] = y coord for nth point
    w[i] = vorticity at nth point
*/
	double     *x;
	double     *w;

	double     *diffx;
	double     *olddiffx;
	double     *tempx;
	double     *tempdiffx;
/*  (xs[2i+0],xs[2i+1]) is reflection of (x[2i+0],x[2i+1]) about unit circle
    xs[2i+0] = x[2i+0]/nx
    xs[2i+1] = x[2i+1]/nx
    where
    nx = x[2i+0]*x[2i+0] + x[2i+1]*x[2i+1]

    x_is_zero[i] = (nx < 1e-10)
*/
	double     *xs;
	short      *x_is_zero;

/*  (p[2i+0],p[2i+1]) is image of (x[2i+0],x[2i+1]) under polynomial p.
    mod_dp2 is |p'(z)|^2 when z = (x[2i+0],x[2i+1]).
*/
	double	   *p;
	double	   *mod_dp2;

/* Sometimes in our calculations we get overflow or numbers that are too big.  
   If that happens with the point x[2*i+0], x[2*i+1], we set dead[i].
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

static euler2dstruct *euler2ds = (euler2dstruct *) NULL;

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
  double temp,mp1,mp2;

  mp1=0;
  mp2=0;
  for(i=deg_p;i>=2;i--)
  {
    add(mp1,mp2,i*p_coef[(i-2)*2],i*p_coef[(i-2)*2+1]);
    mult(mp1,mp2,z1,z2);
  }
  add(mp1,mp2,1,0);
  return mp1*mp1+mp2*mp2;
}

static void
calc_all_p(euler2dstruct *sp)
{
  int i,j;
  double temp,p1,p2,z1,z2;
  for(j=(sp->hide_vortex?sp->Nvortex:0);j<sp->N;j++) if(!sp->dead[j])
  {
    p1=0;
    p2=0;
    z1=sp->x[2*j+0];
    z2=sp->x[2*j+1];
    for(i=deg_p;i>=2;i--)
    {
      add(p1,p2,sp->p_coef[(i-2)*2],sp->p_coef[(i-2)*2+1]);
      mult(p1,p2,z1,z2);
    }
    add(p1,p2,1,0);
    mult(p1,p2,z1,z2);
    sp->p[2*j+0] = p1;
    sp->p[2*j+1] = p2;
  }
}

static void
calc_all_mod_dp2(double *x, euler2dstruct *sp)
{
  int i,j;
  double temp,mp1,mp2,z1,z2;
  for(j=0;j<sp->N;j++) if(!sp->dead[j])
  {
    mp1=0;
    mp2=0;
    z1=x[2*j+0];
    z2=x[2*j+1];
    for(i=deg_p;i>=2;i--)
    {
      add(mp1,mp2,i*sp->p_coef[(i-2)*2],i*sp->p_coef[(i-2)*2+1]);
      mult(mp1,mp2,z1,z2);
    }
    add(mp1,mp2,1,0);
    sp->mod_dp2[j] = mp1*mp1+mp2*mp2;
  }
}

static void
derivs(double *x, euler2dstruct *sp)
{
  int i,j;
  double u1,u2,x1,x2,xij1,xij2,nxij;
  double nx;

  if (variable_boundary)
    calc_all_mod_dp2(sp->x,sp);

  for (j=0;j<sp->Nvortex;j++) if (!sp->dead[j])
  {
    nx = x[2*j+0]*x[2*j+0] + x[2*j+1]*x[2*j+1];
    if (nx < 1e-10)
      sp->x_is_zero[j] = 1;
    else {
      sp->x_is_zero[j] = 0;
      sp->xs[2*j+0] = x[2*j+0]/nx;
      sp->xs[2*j+1] = x[2*j+1]/nx;
    }
  }

  (void) memset(sp->diffx,0,sizeof(double)*2*sp->N);

  for (i=0;i<sp->N;i++) if (!sp->dead[i])
  {
    x1 = x[2*i+0];
    x2 = x[2*i+1];
    for (j=0;j<sp->Nvortex;j++) if (!sp->dead[j])
    {
/*
  Calculate the Biot-Savart kernel, that is, effect of a 
  vortex point at a = (x[2*j+0],x[2*j+1]) at the point 
  x = (x1,x2), returning the vector field in (u1,u2).

  In the plane, this is given by the formula

  u = (x-a)/|x-a|^2  or zero if x=a.

  However, in the unit disk we have to subtract from the
  above:

  (x-as)/|x-as|^2

  where as = a/|a|^2 is the reflection of a about the unit circle.

  If however power != 1, then

  u = (x-a)/|x-a|^(power+1)  -  |a|^(1-power) (x-as)/|x-as|^(power+1)

*/

      xij1 = x1 - x[2*j+0];
      xij2 = x2 - x[2*j+1];
      nxij = (power==1.0) ? xij1*xij1+xij2*xij2 : pow(xij1*xij1+xij2*xij2,(power+1)/2.0);

      if(nxij >= 1e-4)  {
        u1 =  xij2/nxij;
        u2 = -xij1/nxij;
      }
      else
        u1 = u2 = 0.0;

      if (!sp->x_is_zero[j])
      {
        xij1 = x1 - sp->xs[2*j+0];
        xij2 = x2 - sp->xs[2*j+1];
        nxij = (power==1.0) ? xij1*xij1+xij2*xij2 : pow(xij1*xij1+xij2*xij2,(power+1)/2.0);

        if (nxij < 1e-5)
        {
          sp->dead[i] = 1;
          u1 = u2 = 0.0;
        }
        else
        {
          u1 -= xij2/nxij;
          u2 += xij1/nxij;
        }
      }

      if (!sp->dead[i])
      {
        sp->diffx[2*i+0] += u1*sp->w[j];
        sp->diffx[2*i+1] += u2*sp->w[j];
      }
    }

    if (!sp->dead[i] && variable_boundary)
    {
      if (sp->mod_dp2[i] < 1e-5)
        sp->dead[i] = 1;
      else
      {
        sp->diffx[2*i+0] /= sp->mod_dp2[i];
        sp->diffx[2*i+1] /= sp->mod_dp2[i];
      }
    }
  }
}

/*
  What perturb does is effectively
    ret = x + k,
  where k should be of order delta_t.  

  We have the option to do this more subtly by mapping points x 
  in the unit disk to points y in the plane, where y = f(|x|) x, 
  with f(t) = -log(1-t)/t.

  This might reduce (but does not remove) problems where particles near 
  the edge of the boundary bounce around.

  But it seems to be not that effective, so for now switch it off.
*/

#define SUBTLE_PERTURB 0

static void
perturb(double ret[], double x[], double k[], euler2dstruct *sp)
{
  int i;
  double x1,x2,k1,k2;

#if SUBTLE_PERTURB
  double d1,d2,t1,t2,mag,mag2,mlog1mmag,memmagdmag,xdotk;
  for (i=0;i<sp->N;i++) if (!sp->dead[i])
  {
    x1 = x[2*i+0];
    x2 = x[2*i+1];
    k1 = k[2*i+0];
    k2 = k[2*i+1];
    mag2 = x1*x1 + x2*x2;
    if (mag2 < 1e-10)
    {
      ret[2*i+0] = x1+k1;
      ret[2*i+1] = x2+k2;
    }
    else if (mag2 > 1-1e-5)
      sp->dead[i] = 1;
    else
    {
      mag = sqrt(mag2);
      mlog1mmag = -log(1-mag);
      xdotk = x1*k1 + x2*k2;
      t1 = (x1 + k1)*mlog1mmag/mag + x1*xdotk*(1.0/(1-mag)-mlog1mmag/mag)/mag/mag;
      t2 = (x2 + k2)*mlog1mmag/mag + x2*xdotk*(1.0/(1-mag)-mlog1mmag/mag)/mag/mag;
      mag = sqrt(t1*t1+t2*t2);
      if (mag > 11.5 /* log(1e5) */)
        sp->dead[i] = 1;
      else
      {
        memmagdmag = (mag>1e-5) ? ((1.0-exp(-mag))/mag) : (1-mag/2.0);
        ret[2*i+0] = t1*memmagdmag;
        ret[2*i+1] = t2*memmagdmag;
      }
    }
    if (!sp->dead[i])
    {
      d1 = ret[2*i+0]-x1;
      d2 = ret[2*i+1]-x2;
      if (d1*d1+d2*d2 > 0.1)
        sp->dead[i] = 1;
    }
  }

#else

  for (i=0;i<sp->N;i++) if (!sp->dead[i])
  {
    x1 = x[2*i+0];
    x2 = x[2*i+1];
    k1 = k[2*i+0];
    k2 = k[2*i+1];
    if (k1*k1+k2*k2 > 0.1 || x1*x1+x2*x2 > 1-1e-5)
      sp->dead[i] = 1;
    else
    {
      ret[2*i+0] = x1+k1;
      ret[2*i+1] = x2+k2;
    }
  }
#endif
}

static void
ode_solve(euler2dstruct *sp)
{
  int i;
  double *temp;

  if (sp->count < 1) {
    /* midpoint method */
    derivs(sp->x,sp);
    (void) memcpy(sp->olddiffx,sp->diffx,sizeof(double)*2*sp->N);
    for (i=0;i<sp->N;i++) if (!sp->dead[i]) {
      sp->tempdiffx[2*i+0] = 0.5*delta_t*sp->diffx[2*i+0];
      sp->tempdiffx[2*i+1] = 0.5*delta_t*sp->diffx[2*i+1];
    }
    perturb(sp->tempx,sp->x,sp->tempdiffx,sp);
    derivs(sp->tempx,sp);
    for (i=0;i<sp->N;i++) if (!sp->dead[i]) {
      sp->tempdiffx[2*i+0] = delta_t*sp->diffx[2*i+0];
      sp->tempdiffx[2*i+1] = delta_t*sp->diffx[2*i+1];
    }
    perturb(sp->x,sp->x,sp->tempdiffx,sp);
  } else {
    /* Adams Basforth */
    derivs(sp->x,sp);
    for (i=0;i<sp->N;i++) if (!sp->dead[i]) {
      sp->tempdiffx[2*i+0] = delta_t*(1.5*sp->diffx[2*i+0] - 0.5*sp->olddiffx[2*i+0]);
      sp->tempdiffx[2*i+1] = delta_t*(1.5*sp->diffx[2*i+1] - 0.5*sp->olddiffx[2*i+1]);
    }
    perturb(sp->x,sp->x,sp->tempdiffx,sp);
    temp = sp->olddiffx;
    sp->olddiffx = sp->diffx;
    sp->diffx = temp;
  }
}

#define deallocate(p,t) if (p!=NULL) {(void) free((void *) p); p=(t*)NULL; }
#define allocate(p,t,s) if ((p=(t*)malloc(sizeof(t)*s))==NULL)\
{free_euler2d(mi);return;}

ENTRYPOINT void
free_euler2d(ModeInfo * mi)
{
	euler2dstruct *sp = &euler2ds[MI_SCREEN(mi)];
	deallocate(sp->csegs, XSegment);
	deallocate(sp->old_segs, XSegment);
	deallocate(sp->nold_segs, int);
	deallocate(sp->lastx, short);
	deallocate(sp->x, double);
	deallocate(sp->diffx, double);
	deallocate(sp->w, double);
	deallocate(sp->olddiffx, double);
	deallocate(sp->tempdiffx, double);
	deallocate(sp->tempx, double);
	deallocate(sp->dead, short);
	deallocate(sp->boundary, XSegment);
	deallocate(sp->xs, double);
	deallocate(sp->x_is_zero, short);
	deallocate(sp->p, double);
	deallocate(sp->mod_dp2, double);
}

ENTRYPOINT void
init_euler2d (ModeInfo * mi)
{
#define nr_rotates 18 /* how many rotations to try to fill as much of screen as possible - must be even number */
	euler2dstruct *sp;
	int         i,k,n,np;
	double      r,theta,x,y,w;
	double      mag,xscale,yscale,p1,p2;
	double      low[nr_rotates],high[nr_rotates],pp1,pp2,pn1,pn2,angle1,angle2,tempangle,dist,scale,bestscale,temp;
	int	    besti = 0;

        if (power<0.5) power = 0.5;
        if (power>3.0) power = 3.0;
	variable_boundary &= power == 1.0;
	delta_t = 0.001;
        if (power>1.0) delta_t *= pow(0.1,power-1);

	MI_INIT (mi, euler2ds);
	sp = &euler2ds[MI_SCREEN(mi)];

#ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (MI_DISPLAY(mi), MI_GC(mi),  False);
#endif

	sp->boundary_color = NRAND(MI_NPIXELS(mi));
	sp->hide_vortex = NRAND(4) != 0;

	sp->count = 0;

    sp->xshift2 = sp->yshift2 = 0;

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

    if (sp->width > sp->height * 5 ||  /* window has weird aspect */
        sp->height > sp->width * 5)
    {
      if (sp->width > sp->height)
        {
          sp->height = sp->width * 0.8;
          sp->yshift2 = -sp->height/2;
        }
      else
        {
          sp->width = sp->height * 0.8;
          sp->xshift2 = -sp->width/2;
        }
    }

    if (sp->width > 2560 || sp->height > 2560)  /* Retina displays */
      XSetLineAttributes (MI_DISPLAY(mi), MI_GC(mi),
                          3, LineSolid, CapRound, JoinRound);


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
	 
	/* Allocate memory. */

	if (sp->csegs == NULL) {
		allocate(sp->csegs, XSegment, sp->N);
		allocate(sp->old_segs, XSegment, sp->N * tail_len);
		allocate(sp->nold_segs, int, tail_len);
		allocate(sp->lastx, short, sp->N * 2);
		allocate(sp->x, double, sp->N * 2);
		allocate(sp->diffx, double, sp->N * 2);
		allocate(sp->w, double, sp->Nvortex);
		allocate(sp->olddiffx, double, sp->N * 2);
		allocate(sp->tempdiffx, double, sp->N * 2);
		allocate(sp->tempx, double, sp->N * 2);
		allocate(sp->dead, short, sp->N);
		allocate(sp->boundary, XSegment, n_bound_p);
		allocate(sp->xs, double, sp->Nvortex * 2);
		allocate(sp->x_is_zero, short, sp->Nvortex);
		allocate(sp->p, double, sp->N * 2);
		allocate(sp->mod_dp2, double, sp->N);
	}
	for (i=0;i<tail_len;i++) {
		sp->nold_segs[i] = 0;
	}
	sp->c_old_seg = 0;
	(void) memset(sp->dead,0,sp->N*sizeof(short));

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


/* Here we figure out the best rotation of the domain so that it fills as
   much of the screen as possible.  The number of angles we look at is determined
   by nr_rotates (we look every 180/nr_rotates degrees).  
   While we figure out the best angle to rotate, we also figure out the correct scaling factors.
*/

		for(k=0;k<nr_rotates;k++) {
			low[k] = 1e5;
			high[k] = -1e5;
		}

		for(k=0;k<n_bound_p;k++) {
			calc_p(&p1,&p2,cos((double)k/(n_bound_p)*2*M_PI),sin((double)k/(n_bound_p)*2*M_PI),sp->p_coef);
			calc_p(&pp1,&pp2,cos((double)(k-1)/(n_bound_p)*2*M_PI),sin((double)(k-1)/(n_bound_p)*2*M_PI),sp->p_coef);
			calc_p(&pn1,&pn2,cos((double)(k+1)/(n_bound_p)*2*M_PI),sin((double)(k+1)/(n_bound_p)*2*M_PI),sp->p_coef);
			angle1 = nr_rotates/M_PI*atan2(p2-pp2,p1-pp1)-nr_rotates/2;
			angle2 = nr_rotates/M_PI*atan2(pn2-p2,pn1-p1)-nr_rotates/2;
			while (angle1<0) angle1+=nr_rotates*2;
			while (angle2<0) angle2+=nr_rotates*2;
			if (angle1>nr_rotates*1.75 && angle2<nr_rotates*0.25) angle2+=nr_rotates*2;
			if (angle1<nr_rotates*0.25 && angle2>nr_rotates*1.75) angle1+=nr_rotates*2;
                        if (angle2<angle1) {
				tempangle=angle1;
				angle1=angle2;
				angle2=tempangle;
			}
			for(i=(int)floor(angle1);i<(int)ceil(angle2);i++) {
				dist = cos((double)i*M_PI/nr_rotates)*p1 + sin((double)i*M_PI/nr_rotates)*p2;
				if (i%(nr_rotates*2)<nr_rotates) {
					if (dist>high[i%nr_rotates]) high[i%nr_rotates] = dist;
					if (dist<low[i%nr_rotates]) low[i%nr_rotates] = dist;
				}
				else {
					if (-dist>high[i%nr_rotates]) high[i%nr_rotates] = -dist;
					if (-dist<low[i%nr_rotates]) low[i%nr_rotates] = -dist;
				}
			}
		}
		bestscale = 0;
		for (i=0;i<nr_rotates;i++) {
			xscale = (sp->width-5.0)/(high[i]-low[i]);
			yscale = (sp->height-5.0)/(high[(i+nr_rotates/2)%nr_rotates]-low[(i+nr_rotates/2)%nr_rotates]);
			scale = (xscale>yscale) ? yscale : xscale;
			if (scale>bestscale) {
				bestscale = scale;
				besti = i;
			}
		}
/* Here we do the rotation.  The way we do this is to replace the
   polynomial p(z) by a^{-1} p(a z) where a = exp(i best_angle).
*/
		p1 = 1;
		p2 = 0;
		for(k=2;k<=deg_p;k++)
		{
			mult(p1,p2,cos((double)besti*M_PI/nr_rotates),sin((double)besti*M_PI/nr_rotates));
			mult(sp->p_coef[2*(k-2)+0],sp->p_coef[2*(k-2)+1],p1,p2);
		}

		sp->scale = bestscale;
		sp->xshift = -(low[besti]+high[besti])/2.0*sp->scale+sp->width/2;
                if (besti<nr_rotates/2)
			sp->yshift = -(low[besti+nr_rotates/2]+high[besti+nr_rotates/2])/2.0*sp->scale+sp->height/2;
		else
			sp->yshift = (low[besti-nr_rotates/2]+high[besti-nr_rotates/2])/2.0*sp->scale+sp->height/2;

        sp->xshift += sp->xshift2;
        sp->yshift += sp->yshift2;

/* Initialize boundary */

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

ENTRYPOINT void
draw_euler2d (ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         b, col, n_non_vortex_segs;
	euler2dstruct *sp;

	MI_IS_DRAWN(mi) = True;

	if (euler2ds == NULL)
		return;
	sp = &euler2ds[MI_SCREEN(mi)];
	if (sp->csegs == NULL)
		return;

	ode_solve(sp);
	if (variable_boundary)
		calc_all_p(sp);

	sp->cnsegs = 0;
	for(b=sp->Nvortex;b<sp->N;b++) if(!sp->dead[b])
	{
		sp->csegs[sp->cnsegs].x1 = sp->lastx[2*b+0];
		sp->csegs[sp->cnsegs].y1 = sp->lastx[2*b+1];
		if (variable_boundary)
		{
			sp->csegs[sp->cnsegs].x2 = (short)(sp->p[2*b+0]*sp->scale+sp->xshift);
			sp->csegs[sp->cnsegs].y2 = (short)(sp->p[2*b+1]*sp->scale+sp->yshift);
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
			sp->csegs[sp->cnsegs].x2 = (short)(sp->p[2*b+0]*sp->scale+sp->xshift);
			sp->csegs[sp->cnsegs].y2 = (short)(sp->p[2*b+1]*sp->scale+sp->yshift);
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
		(void) memcpy(sp->old_segs+sp->c_old_seg*sp->N, sp->csegs, sp->cnsegs*sizeof(XSegment));
		sp->nold_segs[sp->c_old_seg] = sp->cnsegs;
		sp->c_old_seg++;
		if (sp->c_old_seg >= tail_len)
		  sp->c_old_seg = 0;
	}

	if (++sp->count > MI_CYCLES(mi)) /* pick a new flow */
		init_euler2d(mi);

}

#ifndef STANDALONE
ENTRYPOINT void
refresh_euler2d (ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
#endif

XSCREENSAVER_MODULE ("Euler2D", euler2d)

#endif /* MODE_euler2d */
