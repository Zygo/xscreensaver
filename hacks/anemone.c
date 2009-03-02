/* anemon, Copyright (c) 2001 Gabriel Finch
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/*------------------------------------------------------------------------
  |
  |  FILE            anemone.c
  |  MODULE OF       xscreensaver
  |
  |  DESCRIPTION     Anemone.
  |
  |  WRITTEN BY      Gabriel Finch
  |                  
  |
  |
  |  MODIFICATIONS   june 2001 started
  |           
  +----------------------------------------------------------------------*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "screenhack.h"


#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
#include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */


/*-----------------------------------------------------------------------+
  |  PRIVATE DATA                                                          |
  +-----------------------------------------------------------------------*/


#define TWO_PI     (2.0 * M_PI)
#define RND(x)     (random() % (x))
#define MAXPEND    2000
#define MAXPTS    200
#define TRUE 1
#define FALSE 0

static Display *dpy;
static Window window;

static Pixmap b,ba,bb;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
static  XdbeBackBuffer backb;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

static int
arms,                       /* number of arms */
  finpoints;                  /* final number of points in each array. */
static long delay;              /* usecs to wait between updates. */

static int         scrWidth, scrHeight;
static GC          gcDraw, gcClear;

typedef struct {
  double x,y,z;
  int sx,sy,sz;
} vPend;

typedef unsigned short bool;

static bool dbuf;
static int width;

typedef struct {
  long col;
  int numpt;
  int growth;
  unsigned short rate;
} appDef;

static vPend *vPendage;  /* 3D representation of appendages */
static appDef *appD;  /* defaults */
static vPend *vCurr, *vNext;
static appDef *aCurr;

static double turn, turndelta;

static int    mx, my;            /* max screen coordinates. */
static int withdraw;

static    XGCValues         gcv;
static    Colormap          cmap;



/*-----------------------------------------------------------------------+
  |  PUBLIC DATA                                                           |
  +-----------------------------------------------------------------------*/

char *progclass = "Anemone";

char *defaults [] = {
  ".background: black",
  "*arms: 128",
  "*width: 2",
  "*finpoints: 64",
  "*delay: 40000",
  "*withdraw: 1200",
  "*turnspeed: 50",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};


XrmOptionDescRec options [] = {
  { "-arms",        ".arms",        XrmoptionSepArg, 0 },
  { "-finpoints",   ".finpoints",   XrmoptionSepArg, 0 },
  { "-delay",       ".delay",       XrmoptionSepArg, 0 },
  { "-width",       ".width",       XrmoptionSepArg, 0 },
  { "-withdraw",    ".withdraw",    XrmoptionSepArg, 0 },
  { "-turnspeed",   ".turnspeed",   XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

/*-----------------------------------------------------------------------+
  |  PRIVATE FUNCTIONS                                                     |
  +-----------------------------------------------------------------------*/

static void *
xmalloc(size_t size)
{
  void *ret;

  if ((ret = malloc(size)) == NULL) {
    fprintf(stderr, "anemone: out of memory\n");
    exit(1);
  }
  return ret;
}


static void
initAppendages(void)
{
  int    i;
  /*int    marginx, marginy; */
    
  /*double scalex, scaley;*/

  double x,y,z,dist;

  mx = scrWidth - 1;
  my = scrHeight - 1;

  /* each appendage will have: colour,
     number of points, and a grow or shrink indicator */

  /* added: growth rate 1-10 (smaller==faster growth) */
  /* each appendage needs virtual coords (x,y,z) with y and z combining to
     give the screen y */

  vPendage = (vPend *) xmalloc((finpoints + 1) * sizeof(vPend) * arms);
  appD = (appDef *) xmalloc(sizeof(appDef) * arms);


  for (i = 0; i < arms; i++) {
    aCurr = appD + i;
    vCurr = vPendage + (finpoints + 1) * i;
    vNext = vCurr + 1;

    aCurr->col = (long)RND(256)*RND(256)+32768;
    aCurr->numpt = 1;
    aCurr->growth=finpoints/2+RND(finpoints/2);
    aCurr->rate=RND(11)*RND(11);

    dist=1.;

    do {
      x=(1-RND(1001)/500);
      y=(1-RND(1001)/500);
      z=(1-RND(1001)/500);
      dist=x*x+y*y+z*z;
    } while (dist>=1.);

    vCurr->x=x*200;
    vCurr->y=my/2+y*200;
    vCurr->z=0+z*200;

    /* start the arm going outwards */
    vCurr->sx=vCurr->x/5;
    vCurr->sy=(vCurr->y-my/2)/5;
    vCurr->sz=(vCurr->z)/5;

    
    vNext->x=vCurr->x+vCurr->sx;
    vNext->y=vCurr->y+vCurr->sy;
    vNext->z=vCurr->z+vCurr->sz;
  }
}

static void
initAnemone(void)
{
  XWindowAttributes wa;

  turn = 0.;
  
  width = get_integer_resource("width", "Integer");
  arms = get_integer_resource("arms", "Integer");
  finpoints = get_integer_resource("finpoints", "Integer");
  delay = get_integer_resource("delay", "Integer");
  withdraw = get_integer_resource("withdraw", "Integer");
  turndelta = get_float_resource("turnspeed", "float")/100000;

  dbuf=TRUE;


  b=ba=bb=0;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  b = backb = xdbe_get_backbuffer (dpy, window, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */


  XGetWindowAttributes(dpy, window, &wa);
  scrWidth = wa.width;
  scrHeight = wa.height;
  cmap = wa.colormap;
  gcDraw = XCreateGC(dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource("background", "Background", dpy, cmap);
  gcClear = XCreateGC(dpy, window, GCForeground, &gcv);

  if (dbuf) {
    if (!b)
      {
	ba = XCreatePixmap (dpy, window, scrWidth, scrHeight, wa.depth);
	bb = XCreatePixmap (dpy, window, scrWidth, scrHeight, wa.depth);
	b = ba;
      }
  }
  else
    {	
      b = window;
    }

  if (ba) XFillRectangle (dpy, ba, gcClear, 0, 0, scrWidth, scrHeight);
  if (bb) XFillRectangle (dpy, bb, gcClear, 0, 0, scrWidth, scrHeight);

  XClearWindow(dpy, window);
  XSetLineAttributes(dpy, gcDraw,  width, LineSolid, CapRound, JoinBevel);

  initAppendages();
}


static void
createPoints(void)
{
  int i;
  int withdrawall=RND(withdraw);

  for (i = 0; i< arms; i++) {
    aCurr = appD + i;
    if (!withdrawall) {
      aCurr->growth=-finpoints;
      turndelta=-turndelta;
    }

    else if (withdrawall<11) aCurr->growth=-aCurr->numpt;

    else if (RND(100)<aCurr->rate) {
      if (aCurr->growth>0) {
	if (!(--aCurr->growth)) aCurr->growth=-RND(finpoints)-1;
	vCurr = vPendage + (finpoints + 1) * i + aCurr->numpt-1;
	if (aCurr->numpt<finpoints - 1) {
	  /* add a piece */	
	  vNext=vCurr + 1;
	  aCurr->numpt++;
	  vNext->sx=vCurr->sx+RND(3)-1;
	  vNext->sy=vCurr->sy+RND(3)-1;
	  vNext->sz=vCurr->sz+RND(3)-1;
	  vCurr=vNext+1;
	  vCurr->x=vNext->x+vNext->sx;
	  vCurr->y=vNext->y+vNext->sy;
	  vCurr->z=vNext->z+vNext->sz;
	}
      }
    }
  }
}


static void
drawImage(Drawable curr_window, double sint, double cost)
{
  int q,numpt,mx2=mx/2;
  double cx,cy,cz,nx=0,ny=0,nz=0;

  if ((numpt=aCurr->numpt)==1) return;
  XSetForeground(dpy, gcDraw, aCurr->col);
    
  vNext=vCurr+1;

  cx=vCurr->x;
  cy=vCurr->y;
  cz=vCurr->z;


  for (q = 0; q < numpt-1; q++) {
    nx=vNext->x+2-RND(5);
    ny=vNext->y+2-RND(5);
    nz=vNext->z+2-RND(5);

    XDrawLine(dpy, curr_window, gcDraw,mx2+cx*cost-cz*sint, cy, mx2+nx*cost-nz*sint, ny);
    vCurr++;
    vNext++;

    cx=nx;
    cy=ny;
    cz=nz;
  }
  XSetLineAttributes(dpy, gcDraw, width*3, LineSolid, CapRound, JoinBevel);
  XDrawLine(dpy, curr_window, gcDraw,mx/2+cx*cost-cz*sint, cy, mx/2+nx*cost-nz*sint, ny);
  XSetLineAttributes(dpy, gcDraw, width, LineSolid, CapRound, JoinBevel);

}

static void
animateAnemone(Drawable curr_window)
{
  int i;
  double sint=sin(turn),cost=cos(turn);

  aCurr = appD;
  for (i = 0; i< arms; i++) {
    vCurr=vPendage + (finpoints + 1) * i;
    if (RND(25)<aCurr->rate) {
      if (aCurr->growth<0) {
	aCurr->numpt-=aCurr->numpt>1;
	if (!(++aCurr->growth)) aCurr->growth=RND(finpoints-aCurr->numpt)+1;
      }
    }
    drawImage(curr_window, sint, cost);
    turn+=turndelta;
    aCurr++;
  }
  createPoints();
  usleep(delay);

  if (turn>=TWO_PI) turn-=TWO_PI;
}

/*-----------------------------------------------------------------------+
  |  PUBLIC FUNCTIONS                                                      |
  +-----------------------------------------------------------------------*/

void
screenhack(Display *disp, Window win)
{

  dpy=disp;
  window=win;

  initAnemone();
  for (;;) {

    XFillRectangle (dpy, b, gcClear, 0, 0, scrWidth, scrHeight);

    animateAnemone(b);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    if (backb)
      {
	XdbeSwapInfo info[1];
	info[0].swap_window = window;
	info[0].swap_action = XdbeUndefined;
	XdbeSwapBuffers (dpy, info, 1);
      }
    else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
      if (dbuf)
	{
	  XCopyArea (dpy, b, window, gcClear, 0, 0,
		     scrWidth, scrHeight, 0, 0);
	  b = (b == ba ? bb : ba);
	}

    screenhack_handle_events (dpy);
  }

}















