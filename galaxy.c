#ifndef lint  
static char sccsid[] = "@(#)galaxy.c	2.7 95/02/21 xlockmore";   
#endif
/*
 * galaxy.c - Spinning galaxies for xlockmore
 *
 * Originally done by Uli Siegmund (uli@wombat.okapi.sub.org) on Amiga
 *   for EGS in Cluster
 * Port from Cluster/EGS to C/Intuition by Harald Backert
 * Port to X11 and incorporation into xlockmore by Hubert Feyrer
 *   (hubert.feyrer@rz.uni-regensburg.de)
 *
 * Revision History:
 * 30-Sep-94: Initial port by Hubert Feyer
 * 10-Oct-94: Add colors by Hubert Feyer
 * 23-Oct-94: Modified by David Bagley <bagleyd@source.asset.com>
 */

#include <math.h>
#include "xlock.h"

#define FLOATRAND ((double) RAND() / ((double) MAXRAND))

#ifndef STARSIZE
#define STARSIZE 1      /* Size of STARS */
#endif
/*#define WRAP       1    *//* Warp around edges */
/*#define BOUNCE     1    *//* Bounce from borders */

#define MAX_GALAXIES		5
#define MAX_STARS		300
#define MAX_HITITERATIONS	200
#define MAX_IDELTAT		50
/* These come originally from the Cluster-version */
#define DEFAULT_GALAXIES	2
#define DEFAULT_STARS		1000
#define DEFAULT_HITITERATIONS	7500
#define DEFAULT_IDELTAT		200     /* 0.02 */

#define GALAKSIZE	3.0
#define QCONS		0.001

#define COLOROFFSET	0		/* I hate green galaxies */
#define MAXCOLORS 64
#define COLORBASE	8
	/* Colors for stars start here */
#define COLORSTEP	(MAXCOLORS/COLORBASE)	/* 8 colors per galaxy */

#ifndef PI
#  define PI M_PI
#endif /*PI*/

#if (STARSIZE > 1)
#  define MyXDrawPoint(x,y) \
            XFillArc(dsp,win,Scr[screen].gc,x,y,gp->starsize,gp->starsize,0*64,360*64)
#else
#  define MyXDrawPoint(x,y) \
            XDrawPoint(dsp,win,Scr[screen].gc,x,y)
#endif /*STARSIZE*/


typedef double Vector[3];
typedef double Matrix[3][3];

typedef struct {
	Vector pos, vel;
	int px, py;
	int color;
	} Star;
typedef struct {
	int mass;
	int starscnt;
	Star *stars;
	int basecolor;
	Vector pos, vel;
	} Galaxy;

typedef struct {
    struct {
	int left;                             /* x minimum */
	int right;                            /* x maximum */
	int top;                              /* y minimum */
	int bottom;                           /* y maximum */
    } clip;				      
    int galcol[MAX_GALAXIES];                 /* colors */
    Matrix mat;                               /* Movement of stars(?) */
    double scale;                             /* Scale */
    int midx;                                 /* Middle of screen, x */
    int midy;                                 /* Middle of screen, y */
    double size;                              /* */
    Vector diff;                              /* */
    Galaxy galaxies[MAX_GALAXIES];            /* the Whole Universe */
    double f_deltat;                          /* quality of calculation, calc'd by d_ideltat */
    int f_galaxies;                           /* # galaxies */
    int f_stars;                              /* # stars per galaxy */
    int f_hititerations;                      /* # iterations before restart */
    int step;                                 /* */
    int init;                                 /* 1 -> re-initialize */
#if (STARSIZE > 1)
    int starsize;
#endif
} unistruct;

static unistruct universes[MAXSCREENS];

void
initgalaxy(win)
    Window      win;
{
    XWindowAttributes xwa;                        /* attributes of display */
    unistruct *gp = &universes[screen];
    int i;
    
    if (batchcount < 1)
      batchcount = 1;
    else if (batchcount > MAX_GALAXIES)
      batchcount = MAX_GALAXIES;
    gp->f_galaxies       = batchcount;
    gp->f_stars          = MAX_STARS;
    gp->f_hititerations  = MAX_HITITERATIONS;
    gp->f_deltat         = ((double) MAX_IDELTAT)/10000.0;

    (void) XGetWindowAttributes(dsp, win, &xwa);
#ifdef BORDER
    gp->clip.left    = xwa.border_width;
    gp->clip.top     = xwa.border_width;
    gp->clip.right   = xwa.width-2*xwa.border_width;
    gp->clip.bottom  = xwa.height-2*xwa.border_width;
#else
    gp->clip.left    = 0;
    gp->clip.top     = 0;
    gp->clip.right   = xwa.width;
    gp->clip.bottom  = xwa.height;
#endif
    gp->scale        = (double)(gp->clip.right)/4.0;
    gp->midx         = gp->clip.right/2;
    gp->midy         = gp->clip.bottom/2;
    gp->init         = 1;
#if (STARSIZE > 1)
    gp->starsize   = STARSIZE;
#endif

    if(!gp->galaxies[0].stars){
	
	for (i=0; i<MAX_GALAXIES; ++i) {
	    gp->galaxies[i].starscnt=0;	/* 0 valid entries */
	    gp->galaxies[i].stars=(Star *)malloc(gp->f_stars*sizeof(Star));
	}
	
    }
}


void
drawgalaxy(win)
     Window      win;
{
    unistruct *gp = &universes[screen];
    double d;                                  /* tmp */
    int i, j, k;                               /* more tmp */

    if(gp->init){
	double w1, w2, w3;                     /* more tmp */
	double v,w, h;                         /* yet more tmp */

	gp->init=0;
	gp->step=0;
	
	for (i=0; i<MAX_GALAXIES; ++i){
	    gp->galcol[i]=RAND() % COLORBASE;
	}
	
	for (i=0; i<gp->f_galaxies; ++i) {
	    gp->galaxies[i].basecolor=gp->galcol[i];
	
	    gp->galaxies[i].starscnt=(RAND() % (gp->f_stars/2))+gp->f_stars/2;
	
	    w1=2.0*PI*FLOATRAND;
	    w2=2.0*PI*FLOATRAND;

	    gp->mat[0][0]=         cos(w2);
	    gp->mat[0][1]=-sin(w1)*sin(w2);
	    gp->mat[0][2]= cos(w1)*sin(w2);
	    gp->mat[1][0]= 0.0;
	    gp->mat[1][1]= cos(w1);
	    gp->mat[1][2]= sin(w1);
	    gp->mat[2][0]=-        sin(w2);
	    gp->mat[2][1]=-sin(w1)*cos(w2);
	    gp->mat[2][2]= cos(w1)*cos(w2);
	
	    gp->galaxies[i].vel[0]=FLOATRAND*2.0-1.0;
	    gp->galaxies[i].vel[1]=FLOATRAND*2.0-1.0;
	    gp->galaxies[i].vel[2]=FLOATRAND*2.0-1.0;
	    gp->galaxies[i].pos[0]=-gp->galaxies[i].vel[0]*gp->f_deltat*
              gp->f_hititerations+FLOATRAND-0.5;
	    gp->galaxies[i].pos[1]=-gp->galaxies[i].vel[1]*gp->f_deltat*
              gp->f_hititerations+FLOATRAND-0.5;
	    gp->galaxies[i].pos[2]=-gp->galaxies[i].vel[2]*gp->f_deltat*
              gp->f_hititerations+FLOATRAND-0.5;
	
	    gp->galaxies[i].mass=FLOATRAND*1000.0;
	
	    /*w3=FLOATRAND;*/
            w3 = 0.0;
	    gp->size=w3*w3*GALAKSIZE+0.1;
	
	    for (j=0; j<gp->galaxies[i].starscnt; ++j) {
		w=2.0*PI*FLOATRAND;
		d=FLOATRAND*gp->size;
		h=FLOATRAND*exp(-2.0*(d/gp->size))/5.0*gp->size;
		if (FLOATRAND<0.5) h=-h;
		gp->galaxies[i].stars[j].pos[0]=gp->mat[0][0]*d*cos(w)+
                  gp->mat[1][0]*d*sin(w)+gp->mat[2][0]*h+gp->galaxies[i].pos[0];
		gp->galaxies[i].stars[j].pos[1]=gp->mat[0][1]*d*cos(w)+gp->mat[1][1]*d*sin(w)+gp->mat[2][1]*h+gp->galaxies[i].pos[1];
		gp->galaxies[i].stars[j].pos[2]=gp->mat[0][2]*d*cos(w)+gp->mat[1][2]*d*sin(w)+gp->mat[2][2]*h+gp->galaxies[i].pos[2];
	    
		v=sqrt(gp->galaxies[i].mass*QCONS/sqrt(d*d+h*h));
		gp->galaxies[i].stars[j].vel[0]=-gp->mat[0][0]*v*sin(w)+gp->mat[1][0]*v*cos(w)+gp->galaxies[i].vel[0];
		gp->galaxies[i].stars[j].vel[1]=-gp->mat[0][1]*v*sin(w)+gp->mat[1][1]*v*cos(w)+gp->galaxies[i].vel[1];
		gp->galaxies[i].stars[j].vel[2]=-gp->mat[0][2]*v*sin(w)+gp->mat[1][2]*v*cos(w)+gp->galaxies[i].vel[2];
	    
		gp->galaxies[i].stars[j].color=COLORSTEP*gp->galaxies[i].basecolor+j%COLORSTEP;
	    
		gp->galaxies[i].stars[j].px=0;
		gp->galaxies[i].stars[j].py=0;
	    }
	}
	
	XSetForeground(dsp,Scr[screen].gc,BlackPixel(dsp, screen));
	XFillRectangle(dsp,win,Scr[screen].gc,gp->clip.left,gp->clip.top,gp->clip.right,gp->clip.bottom);

#if 0
	(void) printf("f_galaxies=%d, f_stars=%d, f_hititerations=%d\n", gp->f_galaxies, gp->f_stars, gp->f_hititerations);
	(void) printf("f_deltat=%g\n", gp->f_deltat);
	(void) printf("Screen: ");
	(void) printf("%dx%d pixel (%d-%d, %d-%d)\n",
	       (gp->clip.right-gp->clip.left),(gp->clip.bottom-gp->clip.top),
	       gp->clip.left,
	       gp->clip.right,
	       gp->clip.top,
	       gp->clip.bottom);
#endif /*0*/
    }
    
    for (i=0; i<gp->f_galaxies; ++i){
	for (j=0; j<gp->galaxies[i].starscnt; ++j) {
	    for (k=0; k<gp->f_galaxies; ++k) {
		gp->diff[0]=gp->galaxies[k].pos[0]-gp->galaxies[i].stars[j].pos[0];
		gp->diff[1]=gp->galaxies[k].pos[1]-gp->galaxies[i].stars[j].pos[1];
		gp->diff[2]=gp->galaxies[k].pos[2]-gp->galaxies[i].stars[j].pos[2];
		d=gp->diff[0]*gp->diff[0]+gp->diff[1]*gp->diff[1]+gp->diff[2]*gp->diff[2];
		d=gp->galaxies[k].mass/(d*sqrt(d))*gp->f_deltat*QCONS;
		gp->diff[0]*=d;
		gp->diff[1]*=d;
		gp->diff[2]*=d;
		gp->galaxies[i].stars[j].vel[0]+=gp->diff[0];
		gp->galaxies[i].stars[j].vel[1]+=gp->diff[1];
		gp->galaxies[i].stars[j].vel[2]+=gp->diff[2];
	    }
	    gp->galaxies[i].stars[j].pos[0]+=gp->galaxies[i].stars[j].vel[0]*
              gp->f_deltat;
	    gp->galaxies[i].stars[j].pos[1]+=gp->galaxies[i].stars[j].vel[1]*
              gp->f_deltat;
	    gp->galaxies[i].stars[j].pos[2]+=gp->galaxies[i].stars[j].vel[2]*
              gp->f_deltat;
	    
	    if (gp->galaxies[i].stars[j].px>=gp->clip.left
		&& gp->galaxies[i].stars[j].px<=gp->clip.right-STARSIZE
		&& gp->galaxies[i].stars[j].py>=gp->clip.top
		&& gp->galaxies[i].stars[j].py<=gp->clip.bottom-STARSIZE){
		XSetForeground(dsp,Scr[screen].gc,BlackPixel(dsp, screen));
		MyXDrawPoint(gp->galaxies[i].stars[j].px, gp->galaxies[i].stars[j].py);
	    }
	    
	    gp->galaxies[i].stars[j].px=(int)(gp->galaxies[i].stars[j].pos[0]*
              gp->scale)+gp->midx;
	    gp->galaxies[i].stars[j].py=(int)(gp->galaxies[i].stars[j].pos[1]*
              gp->scale)+gp->midy;

	    
#ifdef WRAP
	    if(gp->galaxies[i].stars[j].px < gp->clip.left){
		(void) printf("wrap l -> r\n");
		gp->galaxies[i].stars[j].px = gp->clip.right;
	    }
	    if(gp->galaxies[i].stars[j].px > gp->clip.right){
		(void) printf("wrap r -> l\n");
		gp->galaxies[i].stars[j].px = gp->clip.left;
	    }
	    if(gp->galaxies[i].stars[j].py > gp->clip.bottom){
		(void) printf("wrap b -> t\n");
		gp->galaxies[i].stars[j].py = gp->clip.top;
	    }
	    if(gp->galaxies[i].stars[j].py < gp->clip.top){
		(void) printf("wrap t -> b\n");
		gp->galaxies[i].stars[j].py = gp->clip.bottom;
	    }
#endif /*WRAP*/

	    
	    if (gp->galaxies[i].stars[j].px>=gp->clip.left
		&& gp->galaxies[i].stars[j].px<=gp->clip.right-STARSIZE
		&& gp->galaxies[i].stars[j].py>=gp->clip.top
		&& gp->galaxies[i].stars[j].py<=gp->clip.bottom-STARSIZE){
		if (!mono && Scr[screen].npixels > 2){
		    XSetForeground(dsp,Scr[screen].gc,
                      Scr[screen].pixels[gp->galaxies[i].stars[j].color]);
		} else {
		    XSetForeground(dsp,Scr[screen].gc,WhitePixel(dsp, screen));
		}
		MyXDrawPoint(gp->galaxies[i].stars[j].px,
                  gp->galaxies[i].stars[j].py);
	    }
	}
	
	for (k=i+1; k<gp->f_galaxies; ++k) {
	    gp->diff[0]=gp->galaxies[k].pos[0]-gp->galaxies[i].pos[0];
	    gp->diff[1]=gp->galaxies[k].pos[1]-gp->galaxies[i].pos[1];
	    gp->diff[2]=gp->galaxies[k].pos[2]-gp->galaxies[i].pos[2];
	    d=gp->diff[0]*gp->diff[0]+gp->diff[1]*gp->diff[1]+gp->diff[2]*gp->diff[2];
	    d=gp->galaxies[i].mass*gp->galaxies[k].mass/(d*sqrt(d))*gp->f_deltat*QCONS;
	    gp->diff[0]*=d;
	    gp->diff[1]*=d;
	    gp->diff[2]*=d;
	    gp->galaxies[i].vel[0]+=gp->diff[0]/gp->galaxies[i].mass;
	    gp->galaxies[i].vel[1]+=gp->diff[1]/gp->galaxies[i].mass;
	    gp->galaxies[i].vel[2]+=gp->diff[2]/gp->galaxies[i].mass;
	    gp->galaxies[k].vel[0]-=gp->diff[0]/gp->galaxies[k].mass;
	    gp->galaxies[k].vel[1]-=gp->diff[1]/gp->galaxies[k].mass;
	    gp->galaxies[k].vel[2]-=gp->diff[2]/gp->galaxies[k].mass;
	}
	gp->galaxies[i].pos[0]+=gp->galaxies[i].vel[0]*gp->f_deltat;
	gp->galaxies[i].pos[1]+=gp->galaxies[i].vel[1]*gp->f_deltat;
	gp->galaxies[i].pos[2]+=gp->galaxies[i].vel[2]*gp->f_deltat;
    }

    gp->step++;
    if(gp->step > gp->f_hititerations*4) gp->init=1;
}
