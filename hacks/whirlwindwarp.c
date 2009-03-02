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
   Version 1.3.  Smooth with pretty colours.

   This code adapted from original program by jwz/jk above.
   Freely distrubtable.  Please keep this tag with
   this code, and add your own if you contribute.
   I would be delighted to hear if have made use of this code.
   If you find this code useful or have any queries, please
   contact me: pclark@cs.bris.ac.uk / joeyclark@usa.net
   Paul "Joey" Clark, hacking for humanity, Feb 99
   www.cs.bris.ac.uk/~pclark | www.changetheworld.org.uk */

/* 15/May/05: Added colour rotation, limit on max FPS, scaling size dots, and smoother drivers.
    4/Mar/01: Star colours are cycled when new colour can not be allocated.
    4/Mar/01: Stars are plotted as squares with size relative to screen.
   28/Nov/00: Submitted to xscreensaver as "whirlwindwarp".
   10/Oct/00: Ported to xscreensaver as "twinkle".
   19/Feb/98: Meters and interaction added for Ivor's birthday "stars11f".
   11/Aug/97: Original QBasic program. */

#include <math.h>

#include "screenhack.h"
#include "erase.h"
#include "hsv.h"

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;

/* Maximum number of points, maximum tail length, and the number of forcefields/effects (hard-coded) */
#define maxps 1000
#define maxts 50
#define fs 16
/* TODO: change ps and ts arrays into pointers, for dynamic allocation at runtime. */

/* Screen width and height */
static int scrwid,scrhei;
static int starsize;

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
/* These have now become standardised across all forcefields:
 static float damp[fs];  / * Dampening (how much drawn between current and optimal) * /
 static float force[fs]; / * Amount of change per moment * /
*/
static float acc[fs];
static float vel[fs];

/* Number of points and tail length */
static int ps=500;
static int ts=5;

/* Show meters or not? */
static Bool meters;

static Bool init_whirlwindwarp(Display *dpy, Window window)
{
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

static float myrnd(void)
{ /* between -1.0 (inclusive) and +1.0 (exclusive) */
  return 2.0*((float)((random()%10000000)/10000000.0)-0.5);
}

float mysgn(float x)
{
  return ( x < 0 ? -1 :
           x > 0 ? +1 :
                    0 );
}

void stars_newp(int p)
{
  cx[p]=myrnd();
  cy[p]=myrnd();
}

/* Adjust a variable var about optimum op,
     with damp = dampening about op
         force = force of random perturbation */
/* float stars_perturb(float var,float op,float damp,float force) {
   return op+damp*(var-op)+force*myrnd()/4.0;
 }*/
#define stars_perturb(var,op,damp,force) \
  ( (op) + (damp)*((var)-(op)) + (force)*myrnd()/4.0 )

/* Get pixel coordinates of a star */
int stars_scrpos_x(int p)
{
  return scrwid*(cx[p]+1.0)/2.0;
}

int stars_scrpos_y(int p)
{
  return scrhei*(cy[p]+1.0)/2.0;
}

/* Draw a meter of a forcefield's parameter */
void stars_draw_meter(Display *dpy,Window window,GC draw_gc,int f)
{
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
  /* else
     XDrawRectangle(dpy,window,draw_gc,x,y,w,h); */
}

/* Move a star according to acting forcefields */
void stars_move(int p)
{
  float nx,ny;
  float x=cx[p];
  float y=cy[p];

  /* In theory all these if checks are unneccessary,
     since each forcefield effect should do nothing when its var = op.
     But the if's are good for efficiency because this function
     is called once for every point.

    Squirge towards edges (makes a leaf shape, previously split the screen in 4 but now only 1 :)
    These ones must go first, to avoid x+1.0 < 0
   */
  if (fon[6]) {
    /* x = mysgn(x) * pow(fabs(x),var[6]);
       y = mysgn(y) * pow(fabs(y),var[6]);*/
    x = -1.0 + 2.0*pow((x + 1.0)/2.0,var[6]);
  }
  if (fon[7]) {
    y = -1.0 + 2.0*pow((y + 1.0)/2.0,var[7]);
  }

  /* Warping in/out */
  if (fon[1]) {
    x = x * var[1]; y = y * var[1];
  }

  /* Rotation */
  if (fon[2]) {
    nx=x*cos(1.1*var[2])+y*sin(1.1*var[2]);
    ny=-x*sin(1.1*var[2])+y*cos(1.1*var[2]);
    x=nx;
    y=ny;
  }

  /* Asymptotes (looks like a plane with a horizon; equivalent to 1D warp) */
  if (fon[3]) { /* Horizontal asymptote */
    y=y*var[3];
  }
  if (fon[4]) { /* Vertical asymptote */
    x=x+var[4]*x; /* this is the same maths as the last, but with op=0 */
  }
  if (fon[5]) { /* Vertical asymptote at right of screen */
    x=(x-1.0)*var[5]+1.0;
  }

  /* Splitting (whirlwind effect): */
  #define num_splits ( 2 + (int) (fabs(var[0]) * 1000) )
  /* #define thru ( (float)(p%num_splits)/(float)(num_splits-1) ) */
  #define thru ( (float)((int)(num_splits*(float)(p)/(float)(ps)))/(float)(num_splits-1) )
  if (fon[8]) {
    x=x+0.5*var[8]*(-1.0+2.0*thru);
  }
  if (fon[9]) {
    y=y+0.5*var[9]*(-1.0+2.0*thru);
  }

  /* Waves */
  if (fon[10]) {
    y = y + 0.4*var[10]*sin(300.0*var[12]*x + 600.0*var[11]);
  }
  if (fon[13]) {
    x = x + 0.4*var[13]*sin(300.0*var[15]*y + 600.0*var[14]);
  }

  cx[p]=x;
  cy[p]=y;
}

/* Turns a forcefield on, and ensures its vars are suitable. */
void turn_on_field(int f)
{
  if (!fon[f]) {
    /* acc[f]=0.0; */
    acc[f]=0.02 * myrnd();
    vel[f]=0.0;
    var[f]=op[f];
  }
  fon[f] = 1;
  if (f == 10) {
    turn_on_field(11);
    turn_on_field(12);
  }
  if (f == 13) {
    turn_on_field(14);
    turn_on_field(15);
  }
}

static void do_whirlwindwarp(Display *dpy, Window window)
{
  Colormap cmap;
  XWindowAttributes xgwa;
  int got_color = 0;
  XColor color[maxps]; /* The colour assigned to each star */
  XColor bgcolor;
  int p,f,nt, sx,sy, resets,lastresets,cnt;
  int colsavailable;
  int hue;

  /* time_t lastframe = time((time_t) 0); */
  struct timeval lastframe;

  XClearWindow (dpy, window);
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  scrwid = xgwa.width;
  scrhei = xgwa.height;

  starsize=scrhei/480;
  if (starsize<=0)
    starsize=1;

  /* Setup colours */
  hsv_to_rgb (0.0, 0.0, 0.0, &bgcolor.red, &bgcolor.green, &bgcolor.blue);
  got_color = XAllocColor (dpy, cmap, &bgcolor);
  colsavailable=0;
  for (p=0;p<ps;p++) {
        if (!mono_p)
          hsv_to_rgb (random()%360, .6+.4*myrnd(), .6+.4*myrnd(), &color[p].red, &color[p].green, &color[p].blue);
          /* hsv_to_rgb (random()%360, 1.0, 1.0, &color[p].red, &color[p].green, &color[p].blue);   for stronger colours! */
        if ((!mono_p) && (got_color = XAllocColor (dpy, cmap, &color[p]))) {
          colsavailable=p;
        } else {
          if (colsavailable>0) /* assign colours from those already allocated */
            color[p]=color[ p % colsavailable ];
          else
            color[p].pixel=default_fg_pixel;
        }
  }

  /* Set up central (optimal) points for each different forcefield */
  op[1] = 1; name[1] = "Warp";
  op[2] = 0; name[2] = "Rotation";
  op[3] = 1; name[3] = "Horizontal asymptote";
  op[4] = 0; name[4] = "Vertical asymptote";
  op[5] = 1; name[5] = "Vertical asymptote right";
  op[6] = 1; name[6] = "Squirge x";
  op[7] = 1; name[7] = "Squirge y";
  op[0] = 0; name[0] = "Split number (inactive)";
  op[8] = 0; name[8] = "Split velocity x";
  op[9] = 0; name[9] = "Split velocity y";
  op[10] = 0; name[10] = "Horizontal wave amplitude";
  op[11] = myrnd()*3.141; name[11] = "Horizontal wave phase (inactive)";
  op[12] = 0.01; name[12] = "Horizontal wave frequency (inactive)";
  op[13] = 0; name[13] = "Vertical wave amplitude";
  op[14] = myrnd()*3.141; name[14] = "Vertical wave phase (inactive)";
  op[15] = 0.01; name[15] = "Vertical wave frequency (inactive)";

  /* Initialise parameters to optimum, all off */
  for (f=0;f<fs;f++) {
    var[f]=op[f];
    fon[f]=( myrnd()>0.5 ? 1 : 0 );
    acc[f]=0.02 * myrnd();
    vel[f]=0;
  }

  /* Initialise stars */
  for (p=0;p<ps;p++)
    stars_newp(p);

  /* tx[nt],ty[nt] remember earlier screen plots (tails of stars)
     which are deleted when nt comes round again */
  nt = 0;
  resets = 0;

  hue = 180 + 180*myrnd();

  gettimeofday(&lastframe, NULL);

  while (1) {

    if (myrnd()>0.75) {
      /* Change one of the allocated colours to something near the current hue. */
      /* By changing a random colour, we sometimes get a tight colour spread, sometime a diverse one. */
      int p = colsavailable * (0.5+myrnd()/2);
      hsv_to_rgb (hue, .6+.4*myrnd(), .6+.4*myrnd(), &color[p].red, &color[p].green, &color[p].blue);
      if ((!mono_p) && (got_color = XAllocColor (dpy, cmap, &color[p]))) {
      }
      hue = hue + 0.5 + myrnd()*9.0;
      if (hue<0) hue+=360;
      if (hue>=360) hue-=360;
    }

      /* Move current points */
      lastresets=resets;
      resets=0;
      for (p=0;p<ps;p++) {
        /* Erase old */
        XSetForeground (dpy, draw_gc, bgcolor.pixel);
        /* XDrawPoint(dpy,window,draw_gc,tx[nt],ty[nt]); */
        XFillRectangle(dpy,window,draw_gc,tx[nt],ty[nt],starsize,starsize);

        /* Move */
        stars_move(p);
        /* If moved off screen, create a new one */
        if (cx[p]<=-0.9999 || cx[p]>=+0.9999 ||
            cy[p]<=-0.9999 || cy[p]>=+0.9999 ||
            fabs(cx[p])<.0001 || fabs(cy[p])<.0001) {
          stars_newp(p);
          resets++;
        } else if (myrnd()>0.99) /* Reset at random */
          stars_newp(p);

        /* Draw point */
        sx=stars_scrpos_x(p);
        sy=stars_scrpos_y(p);
        XSetForeground (dpy, draw_gc, color[p].pixel);
        /* XDrawPoint(dpy,window,draw_gc,sx,sy); */
        XFillRectangle(dpy,window,draw_gc,sx,sy,starsize,starsize);

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
        if (fon[f]) {
          /* This configuration produces var[f]s usually below 0.01 */
          acc[f]=stars_perturb(acc[f],0,0.98,0.005);
          vel[f]=stars_perturb(vel[f]+0.03*acc[f],0,0.995,0.0);
          var[f]=op[f]+(var[f]-op[f])*0.9995+0.001*vel[f];
        }
        /* fprintf(stderr,"f=%i fon=%i acc=%f vel=%f var=%f\n",f,fon[f],acc[f],vel[f],var[f]); */

        /* Decide whether to turn this forcefield on or off. */
        /* prob_on makes the "splitting" effects less likely than the rest */
        #define prob_on ( f==8 || f==9 ? 0.999975 : 0.9999 )
        if ( fon[f]==0 && myrnd()>prob_on ) {
          turn_on_field(f);
        } else if ( fon[f]!=0 && myrnd()>0.99 && fabs(var[f]-op[f])<0.0005 && fabs(vel[f])<0.005 /* && fabs(acc[f])<0.01 */ ) {
          /* We only turn it off if it has gently returned to its optimal (as opposed to rapidly passing through it). */
          fon[f] = 0;
        }

        if (meters) { /* Redraw the meter */
          XSetForeground(dpy, draw_gc, color[f].pixel);
          stars_draw_meter(dpy,window,draw_gc,f);
        }

        if (fon[f])
          cnt++;
      }

      /* Ensure at least three forcefields are on.
       * BUG: Picking randomly might not be enough since 0,11,12,14 and 15 do nothing!
       * But then what's wrong with a rare gentle twinkle?!
      */
      if (cnt<3) {
        f=random() % fs;
        turn_on_field(f);
      }

      if (meters) {
        XSetForeground(dpy, draw_gc, bgcolor.pixel);
        XDrawRectangle(dpy,window,draw_gc,0,0,lastresets*5,3);
        XSetForeground(dpy, draw_gc, default_fg_pixel);
        XDrawRectangle(dpy,window,draw_gc,0,0,resets*5,3);
      }

      /* Cap frames per second; do not go above specified fps: */
      {
        int maxfps = 200;
        long utimeperframe = 1000000/maxfps;
        struct timeval now;
        long timediff;
        gettimeofday(&now, NULL);
        timediff = now.tv_sec*1000000 + now.tv_usec - lastframe.tv_sec*1000000 - lastframe.tv_usec;
        if (timediff < utimeperframe) {
          /* fprintf(stderr,"sleeping for %i\n",utimeperframe-timediff); */
          usleep(utimeperframe-timediff);
        }
        lastframe = now;

        XSync (dpy, False);
        screenhack_handle_events (dpy);
      }
  }

}


char *progclass = "WhirlwindWarp";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*points:	400",
  "*tails:	8",
  "*meters:	false",
  0
};

XrmOptionDescRec options [] = {
  { "-points",	".points",	XrmoptionSepArg, 0 },
  { "-tails",	".tails",	XrmoptionSepArg, 0 },
  { "-meters",	".meters",	XrmoptionNoArg, "true" },
  { 0, 0, 0, 0 }
};

void screenhack(Display *dpy, Window window)
{
  if (init_whirlwindwarp(dpy, window))
    do_whirlwindwarp(dpy, window);
}
