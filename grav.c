/*
 * grav.c - for xlock, the X Window System lockscreen.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 11-Jul-94: color version
 * 6-Oct-93: by Greg Bowering <greg@smug.student.adelaide.edu.au>
 */

#include "xlock.h"
#include <math.h>

#define GRAV			-0.02	/* Gravitational constant */
#define DIST			16.0
#define COLLIDE			0.0001
#define ALMOST			15.99
#define HALF			0.5
/*#define INTRINSIC_RADIUS	200.0*/
#define INTRINSIC_RADIUS	((float) (gp->height/5))
#define STARRADIUS		(unsigned int)(gp->height/(2*DIST))
#define AVG_RADIUS		(INTRINSIC_RADIUS/DIST)
#define RADIUS			(unsigned int)(INTRINSIC_RADIUS/(POS(Z)+DIST))

#define XR			HALF*ALMOST
#define YR			HALF*ALMOST
#define ZR			HALF*ALMOST

#define VR			0.04

#define DIMENSIONS		3
#define X			0
#define Y			1
#define Z			2

/*#define TRAIL 1 /* For trails (works good in mono only)
#define DAMPING 1 /* For decaying orbits */
#if DAMPING
#define DAMP			0.999999
#define MaxA			0.1	/* Maximum acceleration */
#endif

#define POS(c) planet->P[c]
#define VEL(c) planet->V[c]
#define ACC(c) planet->A[c]

#define Planet(x,y)\
  if ((x) >= 0 && (y) >= 0 && (x) <= gp->width && (y) <= gp->height) {\
    if (planet->ri < 2)\
     XDrawPoint(dsp, win, Scr[screen].gc, (x), (y));\
    else\
     XFillArc(dsp, win, Scr[screen].gc,\
      (x) - planet->ri / 2, (y) - planet->ri / 2, planet->ri, planet->ri,\
      0, 360*64);\
   }

#define FLOATRAND(min,max)	((min)+(random()/MAXRAND)*((max)-(min)))

typedef struct {
  double
    P[DIMENSIONS],
    V[DIMENSIONS],
    A[DIMENSIONS];
  int
    xi,
    yi,
    ri;
  unsigned long colors;
} planetstruct;

typedef struct {
  int width, height;
  int
    x,
    y,
    sr,
    nplanets;
  unsigned long starcolor;
  planetstruct *planets;
} gravstruct;

static gravstruct gravs[MAXSCREENS];

static void init_planet();
static void draw_planet();

void
initgrav(win)
  Window      win;
{
  XWindowAttributes xgwa;
  gravstruct *gp = &gravs[screen];
  unsigned char ball;
  
  XGetWindowAttributes(dsp, win, &xgwa);
  gp->width = xgwa.width;
  gp->height = xgwa.height;
 
  gp->sr = STARRADIUS;
  
  if (batchcount < 0)
    batchcount = 1;
  else if (batchcount > 20)
    batchcount = 10;
  gp->nplanets = batchcount;
 
  if (!gp->planets)
    gp->planets = (planetstruct *) calloc(gp->nplanets, sizeof(planetstruct));
  XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
  XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, gp->width, gp->height);
  if (!mono && Scr[screen].npixels > 2) {
    gp->starcolor = random() % Scr[screen].npixels;
    if (gp->starcolor == BlackPixel(dsp, screen))
      gp->starcolor = WhitePixel(dsp, screen);
  } else
    gp->starcolor = WhitePixel(dsp, screen);
  for (ball = 0; ball < gp->nplanets; ball++)
    init_planet(win, &gp->planets[ball]);
  
  /* Draw centrepoint */
  XDrawArc(dsp, win, Scr[screen].gc,
    gp->width/2 - gp->sr/2, gp->height/2 - gp->sr/2, gp->sr, gp->sr,
    0, 360*64);
}

static void
init_planet(win, planet)
  Window win;
  planetstruct *planet;
{
  gravstruct *gp = &gravs[screen];

    if (!mono && Scr[screen].npixels > 2) {
      planet->colors = random() % Scr[screen].npixels;
      if (planet->colors == BlackPixel(dsp, screen))
        planet->colors = WhitePixel(dsp, screen);
    } else
      planet->colors = WhitePixel(dsp, screen);
    /* Initialize positions */
    POS(X) = FLOATRAND(-XR,XR);
    POS(Y) = FLOATRAND(-YR,YR);
    POS(Z) = FLOATRAND(-ZR,ZR);
    
    if (POS(Z) > -ALMOST) {
      planet->xi = (unsigned int)
        ((double)gp->width * (HALF + POS(X) / (POS(Z) + DIST)));
      planet->yi = (unsigned int)
        ((double)gp->height * (HALF + POS(Y) / (POS(Z) + DIST)));
    }
    else
      planet->xi = planet->yi = -1;
    planet->ri = RADIUS;
    
    /* Initialize velocities */
    VEL(X) = FLOATRAND(-VR,VR);
    VEL(Y) = FLOATRAND(-VR,VR);
    VEL(Z) = FLOATRAND(-VR,VR);
    
    /* Draw planets */
    Planet(planet->xi, planet->yi);
}
 
void
drawgrav(win)
     Window      win;
{
  gravstruct *gp = &gravs[screen];
  register unsigned char ball;
  
  /* Mask centrepoint */
  XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
  XDrawArc(dsp, win, Scr[screen].gc,
    gp->width / 2 - gp->sr / 2, gp->height / 2 - gp->sr / 2, gp->sr, gp->sr,
    0, 360*64);

  /* Resize centrepoint */
  switch (random() % 4) {
   case 0:
    if (gp->sr < STARRADIUS)
      gp->sr++;
    break;
   case 1:
    if (gp->sr > 2)
      gp->sr--;
  }		

  /* Draw centrepoint */
  XSetForeground(dsp, Scr[screen].gc, gp->starcolor);
  XDrawArc(dsp, win, Scr[screen].gc,
    gp->width / 2 - gp->sr / 2, gp->height / 2 - gp->sr / 2, gp->sr, gp->sr,
    0, 360*64);
  
  for (ball = 0; ball < gp->nplanets; ball++)
    draw_planet(win, &gp->planets[ball]);
}

static void
draw_planet(win, planet)
  Window win;
  planetstruct *planet;
{
  gravstruct *gp = &gravs[screen];
  double D;    /* A distance variable to work with */
  register unsigned char cmpt;

    D = POS(X) * POS(X) + POS(Y) * POS(Y) + POS(Z) * POS(Z);
    if (D < COLLIDE)
      D = COLLIDE;
    D = sqrt(D);
    D = D * D * D;
    for (cmpt = X; cmpt < DIMENSIONS; cmpt++)
    {
      ACC(cmpt) = POS(cmpt) * GRAV / D;
#ifdef DAMPING
      if (ACC(cmpt) > MaxA)
        ACC(cmpt) = MaxA;
      else if (ACC(cmpt) < -MaxA)
        ACC(cmpt) = -MaxA;
      VEL(cmpt) = VEL(cmpt) + ACC(cmpt);
      VEL(cmpt) *= DAMP;
#else
      /* update velocity */
      VEL(cmpt) = VEL(cmpt) + ACC(cmpt);
#endif
      /* update position */
      POS(cmpt) = POS(cmpt) + VEL(cmpt);
    }
    
    gp->x = planet->xi;
    gp->y = planet->yi;
    
    if (POS(Z) > -ALMOST) {
      planet->xi = (unsigned int)
        ((double) gp->width * (HALF + POS(X) / (POS(Z) + DIST)));
      planet->yi = (unsigned int)
        ((double) gp->height * (HALF + POS(Y) / (POS(Z) + DIST)));
    } else
      planet->xi = planet->yi = -1;
    
    /* Mask */
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    Planet(gp->x, gp->y);
#ifdef TRAIL
    XSetForeground(dsp, Scr[screen].gc, planet->colors);
    XDrawPoint(dsp, win, Scr[screen].gc, gp->x, gp->y);
#endif
    /* Move */
    gp->x = planet->xi;
    gp->y = planet->yi;
    planet->ri = RADIUS;
    
    /* Redraw */
    XSetForeground(dsp, Scr[screen].gc, planet->colors);
    Planet(gp->x, gp->y);
}
