/* xscreensaver, Copyright (c) 2000 Paul "Joey" Clark <pclark@bris.ac.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * 19971004: Johannes Keukelaar <johannes@nada.kth.se>: Use helix screen
 *           eraser.
 */

/* WhirlwindWarp: moving stars.  Ported from QBasic by Joey.
   Version 1.2.  FF parameters now driven by a
   dampened-walked velocity, making them smoother.

   This code adapted from original program by jwz/jk above.
   Freely distrubtable.  Please keep this tag with
   this code, and add your own if you contribute.
   I would be delighted to hear if have made use of this code.
   If you find this code useful or have any queries, please
   contact me: pclark@cs.bris.ac.uk / joeyclark@usa.net
   Paul "Joey" Clark, hacking for humanity, Feb 99
   www.cs.bris.ac.uk/~pclark | www.changetheworld.org.uk */

#include <math.h>

#include "screenhack.h"
#include "erase.h"
#include "hsv.h"

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;

/* Maximum number of points, tails, and fields (hard-coded) */
#define maxps 1000
#define maxts 30
#define fs 10

/* Screen width and height */
static int scrwid,scrhei;

/* Current x,y of stars in realspace */
static float cx[maxps];
static float cy[maxps];
/* Previous x,y plots in pixelspace for removal later */
static int tx[maxps*maxts];
static int ty[maxps*maxts];
/* The force fields and their parameters */
static char *name[fs];
static int fon[fs];     /* Is field on or off? */
static float var[fs];   /* Current parameter */
static float op[fs];    /* Optimum (central/mean) value */
static float damp[fs];  /* Dampening (how much drawn between current and optimal) */
static float force[fs]; /* Amount of change per moment */
static float acc[fs];

/* Number of points and tails */
static int ps=500;
static int ts=5;

/* Show meters or not? */
static Bool meters;

static Bool init_whirlwindwarp(Display *dpy, Window window) {
  XGCValues gcv;
  Colormap cmap;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  gcv.foreground = default_fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);

  ps = get_integer_resource ("points", "Integer");
  ts = get_integer_resource ("tails", "Integer");
  meters = get_boolean_resource ("meters", "Show meters");
  if (ps>maxps || ts>maxts)
    return 0;
  return 1;
}

static float myrnd(void) { /* between -1.0 and +1.0 */
  return 2.0*((float)((random()%10000000)/10000000.0)-0.5);
}

float mysgn(float x) {
  return ( x < 0 ? -1 :
           x > 0 ? +1 :
                    0 );
}

void stars_newp(int p) {
  cx[p]=myrnd();
  cy[p]=myrnd();
}

/* Adjust a variable var about optimum op,
     with damp = dampening about op
         force = force of random perturbation */
float stars_perturb(float var,float op,float damp,float force) {
  var=op+damp*(var-op)+force*myrnd()/4.0;
/*  if (fabs(var-op)>0.1)  // (to keep within bounds)
    var=op+0.1*mysgn(var-op);*/
  return var;
}

/* Get pixel coordinates of a star */
int stars_scrpos_x(int p) {
  return scrwid*(cx[p]+1.0)/2.0;
}
int stars_scrpos_y(int p) {
  return scrhei*(cy[p]+1.0)/2.0;
}

/* Draw a meter of a forcefield's parameter */
void stars_draw_meter(Display *dpy,Window window,GC draw_gc,int f) {
  int x,y,w,h;
  x=scrwid/2;
  y=f*10;
  w=(var[f]-op[f])*scrwid*4;
  h=7;
  if (w<0) {
    w=-w;
    x=x-w;
  }
  if (fon[f])
    XFillRectangle(dpy,window,draw_gc,x,y,w,h);
  else
    XDrawRectangle(dpy,window,draw_gc,x,y,w,h);
}

/* Move a star according to acting forcefields */
void stars_move(int p) {
  float nx,ny;
  float x=cx[p];
  float y=cy[p];
    if (fon[1]) {
        x = x * var[1]; y = y * var[1];
    }
    if (fon[2]) {
      nx=x*cos(var[2])+y*sin(var[2]);
      ny=-x*sin(var[2])+y*cos(var[2]);
      x=nx;
      y=ny;
    }
    if (fon[3]) {
      y=y*var[3];
    }
    if (fon[4]) {
      x=(x-1.0)*var[3]+1.0;
    }
    if (fon[5]) {
      x=x+var[5]*x;
    }
    if (fon[6]) {
        x = mysgn(x) * pow(fabs(x),var[6]);
        y = mysgn(y) * pow(fabs(y),var[6]);
    }
    if (fon[7]) {
      if (fon[0]) {
        if (fon[9]) {
          x=x+var[7]*(-1.0+2.0*(float)(p%2));
        } else {
          x=x+var[7]*(-1.0+2.0*(float)((p%50)/49.0));
        }
      }
    }
    if (fon[8]) {
      if (fon[0]) {
        if (fon[9]) {
          y=y+var[8]*(-1.0+2.0*(float)(p%2));
        } else {
          y=y+var[8]*(-1.0+2.0*(float)((p%50)/49.0));
        }
      }
    }
  cx[p]=x;
  cy[p]=y;
}

static void do_whirlwindwarp(Display *dpy, Window window) {
  Colormap cmap;
  XWindowAttributes xgwa;
  int got_color = 0;
  XColor color[maxps];
  XColor bgcolor;
  int p,f,nt, sx,sy, resets,lastresets,cnt;

  XClearWindow (dpy, window);
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  scrwid = xgwa.width;
  scrhei = xgwa.height;

  /* Setup colours */
  hsv_to_rgb (0.0, 0.0, 0.0, &bgcolor.red, &bgcolor.green, &bgcolor.blue);
  got_color = XAllocColor (dpy, cmap, &bgcolor);
  for (p=0;p<ps;p++) {
        if (!mono_p)
          hsv_to_rgb (random()%360, .6+.4*myrnd(), .6+.4*myrnd(), &color[p].red, &color[p].green, &color[p].blue);
          /* hsv_to_rgb (random()%360, 1.0, 1.0, &color[p].red, &color[p].green, &color[p].blue);   for stronger colours! */
        if ((!mono_p) && (got_color = XAllocColor (dpy, cmap, &color[p]))) {
        } else {
          if (p>0)
            color[p]=color[0];
          else
            color[p].pixel=default_fg_pixel;
        }
  }

  /* Set up parameter movements for the different forcefields */
  name[1] = "Velocity";
  op[1] = 1; damp[1] = .999; force[1] = .002;
  name[2] = "Rotation";
  op[2] = 0; damp[2] = .999; force[2] = .002;
  name[3] = "Drip";
  op[3] = 1; damp[3] = .999; force[3] = .005;
  name[4] = "Dribble";
  op[4] = 1; damp[4] = .999; force[4] = .005;
  name[5] = "Slide";
  op[5] = 0; damp[5] = .999; force[5] = .002;
  name[6] = "Accelerate";
  op[6] = 1.0; damp[6] = .999; force[6] = .005;
  name[7] = "xDisplace";
  op[7] = 0; damp[7] = .999; force[7] = .005;
  name[8] = "yDisplace";
  op[8] = 0; damp[8] = .999; force[8] = .005;
  /* 0 and 9 are options for splitting displacements [no var] */
  name[0] = "Split";
  op[0] = 0; damp[0] = 0; force[0] = 0;
  name[9] = "2d/3d split";
  op[9] = 0; damp[9] = 0; force[9] = 0;

  /* Initialise parameters to optimum, all off */
  for (f=0;f<fs;f++) {
    var[f]=op[f];
    fon[f]=0;
    acc[f]=0;
  }

  /* Initialise stars */
  for (p=0;p<ps;p++)
    stars_newp(p);

  /* tx[nt],ty[nt] remeber earlier screen plots (tails of stars)
     which are deleted when nt comes round again */
  nt=0;
  resets=0;

  while (1) {

      /* Move current points */
      lastresets=resets;
      resets=0;
      for (p=0;p<ps;p++) {
        /* Erase old */
        XSetForeground (dpy, draw_gc, bgcolor.pixel);
        XDrawPoint(dpy,window,draw_gc,tx[nt],ty[nt]);

        /* Move */
        stars_move(p);
        /* If moved off screen, create a new one */
        if (cx[p]<-1.0 || cx[p]>+1.0 ||
            cy[p]<-1.0 || cy[p]>+1.0 ||
            fabs(cx[p])<.0001 || fabs(cy[p])<.0001) {
          stars_newp(p);
          resets++;
        } else if (myrnd()>0.99) /* Reset at random */
          stars_newp(p);

        /* Draw point */
        sx=stars_scrpos_x(p);
        sy=stars_scrpos_y(p);
        XSetForeground (dpy, draw_gc, color[p].pixel);
        XDrawPoint(dpy,window,draw_gc,sx,sy);

        /* Remember it for removal later */
        tx[nt]=sx;
        ty[nt]=sy;
        nt=(nt+1)%(ps*ts);
      }

      /* Adjust force fields */
      cnt=0;
      for (f=0;f<fs;f++) {

        if (meters) { /* Remove meter from display */
          XSetForeground(dpy, draw_gc, bgcolor.pixel);
          stars_draw_meter(dpy,window,draw_gc,f);
        }

        /* Adjust forcefield's parameter */
        acc[f]=stars_perturb(acc[f],0,0.98,0.009);
        var[f]=op[f]+(var[f]-op[f])*damp[f]+force[f]*acc[f];

        if (myrnd()>0.998) /* Turn it on/off ? */
          fon[f]=(myrnd()<0.0);
          /* fon[f]=!fon[f]; */

        if (meters) { /* Redraw the meter */
          XSetForeground(dpy, draw_gc, color[f].pixel);
          stars_draw_meter(dpy,window,draw_gc,f);
        }

        if (fon[f])
          cnt++;
      }
      if (cnt==0) { /* Ensure at least one is on! */
        f=random() % fs;
        fon[f]=1;
      }
      if (meters) {
        XSetForeground(dpy, draw_gc, bgcolor.pixel);
        XDrawRectangle(dpy,window,draw_gc,0,0,lastresets*5,3);
        XSetForeground(dpy, draw_gc, default_fg_pixel);
        XDrawRectangle(dpy,window,draw_gc,0,0,resets*5,3);
      }
/*      if (resets*5>scrwid) {
       Turn one off a field if too many points are flying off screen */
      /* This was a problem when one of the force-fields was acting wrong,
         but not really needed any more unless we need to debug new ones ...! */
        /* for (f=0;f<fs;f++)
            if (fon[f])
            printf("%i",f);
          printf("\n");
          XSync (dpy, False);
          screenhack_handle_events (dpy);
         sleep(1);
        do { // In fact this bit might go wrong if
          f=random() % fs; // we have not ensured at least one is on (above)
        } while (!fon[f]);
        fon[f]=0;
      }            */

      XSync (dpy, False);
      screenhack_handle_events (dpy);

  }

}


char *progclass = "WhirlwindWarp";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*points:	400",
  "*tails:	10",
  "*meters:	false",
  0
};

XrmOptionDescRec options [] = {
  { "-points",	".points",	XrmoptionSepArg, 0 },
  { "-tails",	".tails",	XrmoptionSepArg, 0 },
  { "-meters",	".meters",	XrmoptionNoArg, "true" },
  { 0, 0, 0, 0 }
};

void screenhack(Display *dpy, Window window) {
  if (init_whirlwindwarp(dpy, window))
    do_whirlwindwarp(dpy, window);
}
