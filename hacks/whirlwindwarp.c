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

/* Maximum number of points, maximum tail length, and the number of forcefields/effects (hard-coded) */
#define maxps 1000
#define maxts 50
#define fs 16
/* TODO: change ps and ts arrays into pointers, for dynamic allocation at runtime. */

struct state {
  Display *dpy;
  Window window;

   GC draw_gc, erase_gc;
   unsigned int default_fg_pixel;

   int scrwid,scrhei;
   int starsize;

   float cx[maxps];	/* Current x,y of stars in realspace */
   float cy[maxps];
   int tx[maxps*maxts];	/* Previous x,y plots in pixelspace for removal later */
   int ty[maxps*maxts];
   char *name[fs];	/* The force fields and their parameters */

   int fon[fs];     /* Is field on or off? */
   float var[fs];   /* Current parameter */
   float op[fs];    /* Optimum (central/mean) value */
   float acc[fs];
   float vel[fs];

   int ps;	/* Number of points and tail length */
   int ts;

   Bool meters;

   int initted;
   XWindowAttributes xgwa;
   int got_color;
   XColor color[maxps]; /* The colour assigned to each star */
   XColor bgcolor;
   int p,f,nt, sx,sy, resets,lastresets,cnt;
   int colsavailable;
   int hue;

  struct timeval lastframe;
};


static void *
whirlwindwarp_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  Colormap cmap;

  st->dpy = dpy;
  st->window = window;

  st->ps=500;
  st->ts=5;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  cmap = st->xgwa.colormap;
  gcv.foreground = st->default_fg_pixel = get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
  st->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource (st->dpy, cmap, "background", "Background");
  st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

  st->ps = get_integer_resource (st->dpy, "points", "Integer");
  st->ts = get_integer_resource (st->dpy, "tails", "Integer");
  st->meters = get_boolean_resource (st->dpy, "meters", "Show meters");
  if (st->ps > maxps) st->ps = maxps;
  if (st->ts > maxts) st->ts = maxts;

  return st;
}

static float myrnd(void)
{ /* between -1.0 (inclusive) and +1.0 (exclusive) */
  return 2.0*((float)((random()%10000000)/10000000.0)-0.5);
}

#if 0
static float mysgn(float x)
{
  return ( x < 0 ? -1 :
           x > 0 ? +1 :
                    0 );
}
#endif

static void stars_newp(struct state *st, int pp)
{
  st->cx[pp]=myrnd();
  st->cy[pp]=myrnd();
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
static int stars_scrpos_x(struct state *st, int pp)
{
  return st->scrwid*(st->cx[pp]+1.0)/2.0;
}

static int stars_scrpos_y(struct state *st, int pp)
{
  return st->scrhei*(st->cy[pp]+1.0)/2.0;
}

/* Draw a meter of a forcefield's parameter */
static void stars_draw_meter(struct state *st, int ff)
{
  int x,y,w,h;
  x=st->scrwid/2;
  y=ff*10;
  w=(st->var[ff]-st->op[ff])*st->scrwid*4;
  h=7;
  if (w<0) {
    w=-w;
    x=x-w;
  }
  if (st->fon[ff])
    XFillRectangle(st->dpy,st->window,st->draw_gc,x,y,w,h);
  /* else
     XDrawRectangle(dpy,window,draw_gc,x,y,w,h); */
}

/* Move a star according to acting forcefields */
static void stars_move(struct state *st, int pp)
{
  float nx,ny;
  float x=st->cx[pp];
  float y=st->cy[pp];

  /* In theory all these if checks are unneccessary,
     since each forcefield effect should do nothing when its var = op.
     But the if's are good for efficiency because this function
     is called once for every point.

    Squirge towards edges (makes a leaf shape, previously split the screen in 4 but now only 1 :)
    These ones must go first, to avoid x+1.0 < 0
   */
  if (st->fon[6]) {
    /* x = mysgn(x) * pow(fabs(x),var[6]);
       y = mysgn(y) * pow(fabs(y),var[6]);*/
    x = -1.0 + 2.0*pow((x + 1.0)/2.0,st->var[6]);
  }
  if (st->fon[7]) {
    y = -1.0 + 2.0*pow((y + 1.0)/2.0,st->var[7]);
  }

  /* Warping in/out */
  if (st->fon[1]) {
    x = x * st->var[1]; y = y * st->var[1];
  }

  /* Rotation */
  if (st->fon[2]) {
    nx=x*cos(1.1*st->var[2])+y*sin(1.1*st->var[2]);
    ny=-x*sin(1.1*st->var[2])+y*cos(1.1*st->var[2]);
    x=nx;
    y=ny;
  }

  /* Asymptotes (looks like a plane with a horizon; equivalent to 1D warp) */
  if (st->fon[3]) { /* Horizontal asymptote */
    y=y*st->var[3];
  }
  if (st->fon[4]) { /* Vertical asymptote */
    x=x+st->var[4]*x; /* this is the same maths as the last, but with op=0 */
  }
  if (st->fon[5]) { /* Vertical asymptote at right of screen */
    x=(x-1.0)*st->var[5]+1.0;
  }

  /* Splitting (whirlwind effect): */
  #define num_splits ( 2 + (int) (fabs(st->var[0]) * 1000) )
  /* #define thru ( (float)(pp%num_splits)/(float)(num_splits-1) ) */
  #define thru ( (float)((int)(num_splits*(float)(pp)/(float)(st->ps)))/(float)(num_splits-1) )
  if (st->fon[8]) {
    x=x+0.5*st->var[8]*(-1.0+2.0*thru);
  }
  if (st->fon[9]) {
    y=y+0.5*st->var[9]*(-1.0+2.0*thru);
  }

  /* Waves */
  if (st->fon[10]) {
    y = y + 0.4*st->var[10]*sin(300.0*st->var[12]*x + 600.0*st->var[11]);
  }
  if (st->fon[13]) {
    x = x + 0.4*st->var[13]*sin(300.0*st->var[15]*y + 600.0*st->var[14]);
  }

  st->cx[pp]=x;
  st->cy[pp]=y;
}

/* Turns a forcefield on, and ensures its vars are suitable. */
static void turn_on_field(struct state *st, int ff)
{
  if (!st->fon[ff]) {
    /* acc[ff]=0.0; */
    st->acc[ff]=0.02 * myrnd();
    st->vel[ff]=0.0;
    st->var[ff]=st->op[ff];
  }
  st->fon[ff] = 1;
  if (ff == 10) {
    turn_on_field(st, 11);
    turn_on_field(st, 12);
  }
  if (ff == 13) {
    turn_on_field(st, 14);
    turn_on_field(st, 15);
  }
}

static unsigned long
whirlwindwarp_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  /* time_t lastframe = time((time_t) 0); */

  if (!st->initted) {
    st->initted = 1;

    XClearWindow (st->dpy, st->window);
    XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
    st->scrwid = st->xgwa.width;
    st->scrhei = st->xgwa.height;

    st->starsize=st->scrhei/480;
    if (st->starsize<=0)
      st->starsize=1;

    /* Setup colours */
    hsv_to_rgb (0.0, 0.0, 0.0, &st->bgcolor.red, &st->bgcolor.green, &st->bgcolor.blue);
    st->got_color = XAllocColor (st->dpy, st->xgwa.colormap, &st->bgcolor);
    st->colsavailable=0;
    for (st->p=0;st->p<st->ps;st->p++) {
      if (!mono_p)
        hsv_to_rgb (random()%360, .6+.4*myrnd(), .6+.4*myrnd(), &st->color[st->p].red, &st->color[st->p].green, &st->color[st->p].blue);
      /* hsv_to_rgb (random()%360, 1.0, 1.0, &color[p].red, &color[p].green, &color[p].blue);   for stronger colours! */
      if ((!mono_p) && (st->got_color = XAllocColor (st->dpy, st->xgwa.colormap, &st->color[st->p]))) {
        st->colsavailable=st->p;
      } else {
        if (st->colsavailable>0) /* assign colours from those already allocated */
          st->color[st->p]=st->color[ st->p % st->colsavailable ];
        else
          st->color[st->p].pixel=st->default_fg_pixel;
      }
    }

  /* Set up central (optimal) points for each different forcefield */
  st->op[1] = 1; st->name[1] = "Warp";
  st->op[2] = 0; st->name[2] = "Rotation";
  st->op[3] = 1; st->name[3] = "Horizontal asymptote";
  st->op[4] = 0; st->name[4] = "Vertical asymptote";
  st->op[5] = 1; st->name[5] = "Vertical asymptote right";
  st->op[6] = 1; st->name[6] = "Squirge x";
  st->op[7] = 1; st->name[7] = "Squirge y";
  st->op[0] = 0; st->name[0] = "Split number (inactive)";
  st->op[8] = 0; st->name[8] = "Split velocity x";
  st->op[9] = 0; st->name[9] = "Split velocity y";
  st->op[10] = 0; st->name[10] = "Horizontal wave amplitude";
  st->op[11] = myrnd()*3.141; st->name[11] = "Horizontal wave phase (inactive)";
  st->op[12] = 0.01; st->name[12] = "Horizontal wave frequency (inactive)";
  st->op[13] = 0; st->name[13] = "Vertical wave amplitude";
  st->op[14] = myrnd()*3.141; st->name[14] = "Vertical wave phase (inactive)";
  st->op[15] = 0.01; st->name[15] = "Vertical wave frequency (inactive)";

  /* Initialise parameters to optimum, all off */
  for (st->f=0;st->f<fs;st->f++) {
    st->var[st->f]=st->op[st->f];
    st->fon[st->f]=( myrnd()>0.5 ? 1 : 0 );
    st->acc[st->f]=0.02 * myrnd();
    st->vel[st->f]=0;
  }

  /* Initialise stars */
  for (st->p=0;st->p<st->ps;st->p++)
    stars_newp(st, st->p);

  /* tx[nt],ty[nt] remember earlier screen plots (tails of stars)
     which are deleted when nt comes round again */
  st->nt = 0;
  st->resets = 0;

  st->hue = 180 + 180*myrnd();

  gettimeofday(&st->lastframe, NULL);

  }


    if (myrnd()>0.75) {
      /* Change one of the allocated colours to something near the current hue. */
      /* By changing a random colour, we sometimes get a tight colour spread, sometime a diverse one. */
      int pp = st->colsavailable * (0.5+myrnd()/2);
      hsv_to_rgb (st->hue, .6+.4*myrnd(), .6+.4*myrnd(), &st->color[pp].red, &st->color[pp].green, &st->color[pp].blue);
      if ((!mono_p) && (st->got_color = XAllocColor (st->dpy, st->xgwa.colormap, &st->color[pp]))) {
      }
      st->hue = st->hue + 0.5 + myrnd()*9.0;
      if (st->hue<0) st->hue+=360;
      if (st->hue>=360) st->hue-=360;
    }

      /* Move current points */
      st->lastresets=st->resets;
      st->resets=0;
      for (st->p=0;st->p<st->ps;st->p++) {
        /* Erase old */
        XSetForeground (st->dpy, st->draw_gc, st->bgcolor.pixel);
        /* XDrawPoint(dpy,window,draw_gc,tx[nt],ty[nt]); */
        XFillRectangle(st->dpy,st->window,st->draw_gc,st->tx[st->nt],st->ty[st->nt],st->starsize,st->starsize);

        /* Move */
        stars_move(st, st->p);
        /* If moved off screen, create a new one */
        if (st->cx[st->p]<=-0.9999 || st->cx[st->p]>=+0.9999 ||
            st->cy[st->p]<=-0.9999 || st->cy[st->p]>=+0.9999 ||
            fabs(st->cx[st->p])<.0001 || fabs(st->cy[st->p])<.0001) {
          stars_newp(st, st->p);
          st->resets++;
        } else if (myrnd()>0.99) /* Reset at random */
          stars_newp(st, st->p);

        /* Draw point */
        st->sx=stars_scrpos_x(st, st->p);
        st->sy=stars_scrpos_y(st, st->p);
        XSetForeground (st->dpy, st->draw_gc, st->color[st->p].pixel);
        /* XDrawPoint(dpy,window,draw_gc,sx,sy); */
        XFillRectangle(st->dpy,st->window,st->draw_gc,st->sx,st->sy,st->starsize,st->starsize);

        /* Remember it for removal later */
        st->tx[st->nt]=st->sx;
        st->ty[st->nt]=st->sy;
        st->nt=(st->nt+1)%(st->ps*st->ts);
      }

      /* Adjust force fields */
      st->cnt=0;
      for (st->f=0;st->f<fs;st->f++) {

        if (st->meters) { /* Remove meter from display */
          XSetForeground(st->dpy, st->draw_gc, st->bgcolor.pixel);
          stars_draw_meter(st,st->f);
        }

        /* Adjust forcefield's parameter */
        if (st->fon[st->f]) {
          /* This configuration produces var[f]s usually below 0.01 */
          st->acc[st->f]=stars_perturb(st->acc[st->f],0,0.98,0.005);
          st->vel[st->f]=stars_perturb(st->vel[st->f]+0.03*st->acc[st->f],0,0.995,0.0);
          st->var[st->f]=st->op[st->f]+(st->var[st->f]-st->op[st->f])*0.9995+0.001*st->vel[st->f];
        }
        /* fprintf(stderr,"f=%i fon=%i acc=%f vel=%f var=%f\n",f,fon[f],acc[f],vel[f],var[f]); */

        /* Decide whether to turn this forcefield on or off. */
        /* prob_on makes the "splitting" effects less likely than the rest */
        #define prob_on ( st->f==8 || st->f==9 ? 0.999975 : 0.9999 )
        if ( st->fon[st->f]==0 && myrnd()>prob_on ) {
          turn_on_field(st, st->f);
        } else if ( st->fon[st->f]!=0 && myrnd()>0.99 && fabs(st->var[st->f]-st->op[st->f])<0.0005 && fabs(st->vel[st->f])<0.005 /* && fabs(acc[f])<0.01 */ ) {
          /* We only turn it off if it has gently returned to its optimal (as opposed to rapidly passing through it). */
          st->fon[st->f] = 0;
        }

        if (st->meters) { /* Redraw the meter */
          XSetForeground(st->dpy, st->draw_gc, st->color[st->f].pixel);
          stars_draw_meter(st,st->f);
        }

        if (st->fon[st->f])
          st->cnt++;
      }

      /* Ensure at least three forcefields are on.
       * BUG: Picking randomly might not be enough since 0,11,12,14 and 15 do nothing!
       * But then what's wrong with a rare gentle twinkle?!
      */
      if (st->cnt<3) {
        st->f=random() % fs;
        turn_on_field(st, st->f);
      }

      if (st->meters) {
        XSetForeground(st->dpy, st->draw_gc, st->bgcolor.pixel);
        XDrawRectangle(st->dpy,st->window,st->draw_gc,0,0,st->lastresets*5,3);
        XSetForeground(st->dpy, st->draw_gc, st->default_fg_pixel);
        XDrawRectangle(st->dpy,st->window,st->draw_gc,0,0,st->resets*5,3);
      }

      /* Cap frames per second; do not go above specified fps: */
      {
        unsigned long this_delay = 0;
        int maxfps = 200;
        long utimeperframe = 1000000/maxfps;
        struct timeval now;
        long timediff;
        gettimeofday(&now, NULL);
        timediff = now.tv_sec*1000000 + now.tv_usec - st->lastframe.tv_sec*1000000 - st->lastframe.tv_usec;
        if (timediff < utimeperframe) {
          /* fprintf(stderr,"sleeping for %i\n",utimeperframe-timediff); */
          this_delay = (utimeperframe-timediff);
        }
        st->lastframe = now;

        return this_delay;
      }
}


static void
whirlwindwarp_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->scrwid = w;
  st->scrhei = h;
}

static Bool
whirlwindwarp_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
whirlwindwarp_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *whirlwindwarp_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*points:	400",
  "*tails:	8",
  "*meters:	false",
  0
};

static XrmOptionDescRec whirlwindwarp_options [] = {
  { "-points",	".points",	XrmoptionSepArg, 0 },
  { "-tails",	".tails",	XrmoptionSepArg, 0 },
  { "-meters",	".meters",	XrmoptionNoArg, "true" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("WhirlWindWarp", whirlwindwarp)
