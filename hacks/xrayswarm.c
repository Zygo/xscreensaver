/*
 * Copyright (c) 2000 by Chris Leger (xrayjones@users.sourceforge.net)
 *
 * xrayswarm - a shameless ripoff of the 'swarm' screensaver on SGI
 *   boxes.
 *
 * Version 1.0 - initial release.  doesn't read any special command-line
 *   options, and only supports the variable 'delay' via Xresources.
 *   (the delay resouces is most useful on systems w/o gettimeofday, in
 *   which case automagical level-of-detail for FPS maintainance can't
 *   be used.)
 *
 *   The code isn't commented, but isn't too ugly. It should be pretty
 *   easy to understand, with the exception of the colormap stuff.
 *
 */
/*
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.
*/

#include <math.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/time.h>
#include "screenhack.h"

#ifdef HAVE_COCOA
# define HAVE_GETTIMEOFDAY 1
#endif

/**********************************************************************
 *								       
 * window crap 
 *
 **********************************************************************/

static const char *xrayswarm_defaults [] ={
	".background:		black",
	"*delay:		20000",
	"*fpsSolid:		true",
	0
};

static XrmOptionDescRec xrayswarm_options [] = {
	{"-delay",".delay",XrmoptionSepArg,0},
	{0,0,0,0}
};


/**********************************************************************
 *								       
 * bug structs & variables
 *
 **********************************************************************/
#define MAX_TRAIL_LEN 60
#define MAX_BUGS 100
#define MAX_TARGETS 10
#define sq(x) ((x)*(x))

#define MAX_FPS 150
#define MIN_FPS 16
#define DESIRED_DT 0.2

typedef struct _sbug {
  float pos[3];
  int hist[MAX_TRAIL_LEN][2];
  float vel[2];
  struct _sbug *closest;
} bug;

#define GRAY_TRAILS 0
#define GRAY_SCHIZO 1
#define COLOR_TRAILS 2
#define RANDOM_TRAILS 3
#define RANDOM_SCHIZO 4
#define COLOR_SCHIZO 5
#define NUM_SCHEMES 6 /* too many schizos; don't use last 2 */  



struct state {
  Display *dpy;
  Window win;

  unsigned char colors[768];

  GC fgc[256];
  GC cgc;
  int xsize, ysize;
  int xc, yc;
  unsigned long delay;
  float maxx, maxy;

  float dt;
  float targetVel;
  float targetAcc;
  float maxVel;
  float maxAcc;
  float noise;
  float minVelMultiplier;

  int nbugs;
  int ntargets;
  int trailLen;

  float dtInv;
  float halfDtSq;
  float targetVelSq;
  float maxVelSq;
  float minVelSq;
  float minVel;

  bug bugs[MAX_BUGS];
  bug targets[MAX_TARGETS];
  int head;
  int tail;
  int colorScheme;
  float changeProb;

  int grayIndex[MAX_TRAIL_LEN];
  int redIndex[MAX_TRAIL_LEN];
  int blueIndex[MAX_TRAIL_LEN];
  int graySIndex[MAX_TRAIL_LEN];
  int redSIndex[MAX_TRAIL_LEN];
  int blueSIndex[MAX_TRAIL_LEN];
  int randomIndex[MAX_TRAIL_LEN];
  int numColors;
  int numRandomColors;

  int checkIndex;
  int rsc_callDepth;
  int rbc_callDepth;

  float draw_fps;
  float draw_timePerFrame, draw_elapsed;
  int *draw_targetColorIndex, *draw_colorIndex;
  int draw_targetStartColor, draw_targetNumColors;
  int draw_startColor, draw_numColors;
  double draw_start, draw_end;
  int draw_cnt;
  int draw_sleepCount;
  int draw_delayAccum;
  int draw_nframes;

  struct timeval startupTime;
};


typedef struct {
  float dt;
  float targetVel;
  float targetAcc;
  float maxVel;
  float maxAcc;
  float noise;

  int nbugs;
  int ntargets;
  int trailLen;
  int colorScheme;
  int changeProb;
} bugParams;

#if 0
static const bugParams good1 = {
  0.3,  /* dt */
  0.03,  /* targetVel */
  0.02,  /* targetAcc */
  0.05,  /* maxVel */
  0.03,  /* maxAcc */
  0.01,  /* noise */
  -1,    /* nbugs */
  -1,     /* ntargets */
  60,    /* trailLen */
  2,     /* colorScheme */
  0.15  /* changeProb */
};

static const bugParams *goodParams[] = { 
&good1
};

static int numParamSets = 1;
#endif

static void initCMap(struct state *st)
{
  int i, n;
  int temp;

  n = 0;

  /* color 0 is black */
  st->colors[n++] = 0;
  st->colors[n++] = 0;
  st->colors[n++] = 0;

  /* color 1 is red */
  st->colors[n++] = 255;
  st->colors[n++] = 0;
  st->colors[n++] = 0;

  /* color 2 is green */
  st->colors[n++] = 255;
  st->colors[n++] = 0;
  st->colors[n++] = 0;

  /* color 3 is blue */
  st->colors[n++] = 255;
  st->colors[n++] = 0;
  st->colors[n++] = 0;

  /* start greyscale colors at 4; 16 levels */
  for (i = 0; i < 16; i++) {
    temp = i*16;
    if (temp > 255) temp = 255;
    st->colors[n++] = 255 - temp;
    st->colors[n++] = 255 - temp;
    st->colors[n++] = 255 - temp;
  }

  /* start red fade at 20; 16 levels */
  for (i = 0; i < 16; i++) {
    temp = i*16;
    if (temp > 255) temp = 255;
    st->colors[n++] = 255 - temp;
    st->colors[n++] = 255 - pow(i/16.0+0.001, 0.3)*255;
    st->colors[n++] = 65 - temp/4;
  }

  /* start blue fade at 36; 16 levels */
  for (i = 0; i < 16; i++) {
    temp = i*16;
    if (temp > 255) temp = 255;
    st->colors[n++] = 32 - temp/8;
    st->colors[n++] = 180 - pow(i/16.0+0.001, 0.3)*180;
    st->colors[n++] = 255 - temp;
  }

  /* random colors start at 52 */
  st->numRandomColors = MAX_TRAIL_LEN;

  st->colors[n] = random()&255; n++;
  st->colors[n] = random()&255; n++;
  st->colors[n] = st->colors[n-2]/2 + st->colors[n-3]/2; n++;

  for (i = 0; i < st->numRandomColors; i++) {
    st->colors[n] = (st->colors[n-3] + (random()&31) - 16)&255; n++;
    st->colors[n] = (st->colors[n-3] + (random()&31) - 16)&255; n++;
    st->colors[n] = st->colors[n-2]/(float)(i+2) + st->colors[n-3]/(float)(i+2); n++;
  }
  
  st->numColors = n/3 + 1;
}

static int initGraphics(struct state *st)
{
  XGCValues xgcv;
  XWindowAttributes xgwa;
/*  XSetWindowAttributes xswa;*/
  Colormap cmap;
  XColor color;
  int n, i;
  
  initCMap(st);

  XGetWindowAttributes(st->dpy,st->win,&xgwa);
  cmap=xgwa.colormap;
/*  xswa.backing_store=Always;
  XChangeWindowAttributes(st->dpy,st->win,CWBackingStore,&xswa);*/
  xgcv.function=GXcopy;

  st->delay = get_integer_resource(st->dpy, "delay","Integer");
  
  xgcv.foreground=get_pixel_resource (st->dpy, cmap, "background", "Background");
  st->fgc[0]=XCreateGC(st->dpy, st->win, GCForeground|GCFunction,&xgcv);
#ifdef HAVE_COCOA
  jwxyz_XSetAntiAliasing (st->dpy, st->fgc[0], False);
#endif
  
  n=0;
  if (mono_p) {
    xgcv.foreground=get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
    st->fgc[1]=XCreateGC(st->dpy,st->win,GCForeground|GCFunction,&xgcv);
#ifdef HAVE_COCOA
    jwxyz_XSetAntiAliasing (st->dpy, st->fgc[1], False);
#endif
    for (i=0;i<st->numColors;i+=2) st->fgc[i]=st->fgc[0];
    for (i=1;i<st->numColors;i+=2) st->fgc[i]=st->fgc[1];
  } else {
    for (i = 0; i < st->numColors; i++) {
      color.red=st->colors[n++]<<8;
      color.green=st->colors[n++]<<8;
      color.blue=st->colors[n++]<<8;
      color.flags=DoRed|DoGreen|DoBlue;
      XAllocColor(st->dpy,cmap,&color);
      xgcv.foreground=color.pixel;
      st->fgc[i] = XCreateGC(st->dpy, st->win, GCForeground | GCFunction,&xgcv);
#ifdef HAVE_COCOA
      jwxyz_XSetAntiAliasing (st->dpy, st->fgc[i], False);
#endif
    }
  }
  st->cgc = XCreateGC(st->dpy,st->win,GCForeground|GCFunction,&xgcv);
  XSetGraphicsExposures(st->dpy,st->cgc,False);
#ifdef HAVE_COCOA
  jwxyz_XSetAntiAliasing (st->dpy, st->cgc, False);
#endif

  st->xsize = xgwa.width;
  st->ysize = xgwa.height;
  st->xc = st->xsize >> 1;
  st->yc = st->ysize >> 1;

  st->maxx = 1.0;
  st->maxy = st->ysize/(float)st->xsize;

  if (st->colorScheme < 0) st->colorScheme = random()%NUM_SCHEMES;

  return True;
}

static void initBugs(struct state *st)
{
  register bug *b;
  int i;

  st->head = st->tail = 0;

  memset((char *)st->bugs, 0,MAX_BUGS*sizeof(bug));
  memset((char *)st->targets, 0, MAX_TARGETS*sizeof(bug));

  if (st->ntargets < 0) st->ntargets = (0.25+frand(0.75)*frand(1))*MAX_TARGETS;
  if (st->ntargets < 1) st->ntargets = 1;

  if (st->nbugs < 0) st->nbugs = (0.25+frand(0.75)*frand(1))*MAX_BUGS;
  if (st->nbugs <= st->ntargets) st->nbugs = st->ntargets+1;

  if (st->trailLen < 0) {
    st->trailLen = (1.0 - frand(0.6)*frand(1))*MAX_TRAIL_LEN;
  }

  if (st->nbugs > MAX_BUGS) st->nbugs = MAX_BUGS;
  if (st->ntargets > MAX_TARGETS) st->ntargets = MAX_TARGETS;
  if (st->trailLen > MAX_TRAIL_LEN) st->trailLen = MAX_TRAIL_LEN;

  b = st->bugs;
  for (i = 0; i < st->nbugs; i++, b++) {
    b->pos[0] = frand(st->maxx);
    b->pos[1] = frand(st->maxy);
    b->vel[0] = frand(st->maxVel/2);
    b->vel[1] = frand(st->maxVel/2);

    b->hist[st->head][0] = b->pos[0]*st->xsize;
    b->hist[st->head][1] = b->pos[1]*st->xsize;
    b->closest = &st->targets[random()%st->ntargets];
  }

  b = st->targets;
  for (i = 0; i < st->ntargets; i++, b++) {
    b->pos[0] = frand(st->maxx);
    b->pos[1] = frand(st->maxy);

    b->vel[0] = frand(st->targetVel/2);
    b->vel[1] = frand(st->targetVel/2);

    b->hist[st->head][0] = b->pos[0]*st->xsize;
    b->hist[st->head][1] = b->pos[1]*st->xsize;
  }
}

static void pickNewTargets(struct state *st)
{
  register int i;
  register bug *b;

  b = st->bugs;
  for (i = 0; i < st->nbugs; i++, b++) {
    b->closest = &st->targets[random()%st->ntargets];
  }
}

#if 0
static void addBugs(int numToAdd)
{
  register bug *b;
  int i;

  if (numToAdd + st->nbugs > MAX_BUGS) numToAdd = MAX_BUGS-st->nbugs;
  else if (numToAdd < 0) numToAdd = 0;

  for (i = 0; i < numToAdd; i++) {
    b = &st->bugs[random()%st->nbugs];
    memcpy((char *)&st->bugs[st->nbugs+i], (char *)b, sizeof(bug));
    b->closest = &st->targets[random()%st->ntargets];
  }

  st->nbugs += numToAdd;
}

static void addTargets(int numToAdd)
{
  register bug *b;
  int i;

  if (numToAdd + st->ntargets > MAX_TARGETS) numToAdd = MAX_TARGETS-st->ntargets;
  else if (numToAdd < 0) numToAdd = 0;

  for (i = 0; i < numToAdd; i++) {
    b = &st->targets[random()%st->ntargets];
    memcpy((char *)&st->targets[st->ntargets+i], (char *)b, sizeof(bug));
    b->closest = &st->targets[random()%st->ntargets];
  }

  st->ntargets += numToAdd;
}
#endif

static void computeConstants(struct state *st)
{
  st->halfDtSq = st->dt*st->dt*0.5;
  st->dtInv = 1.0/st->dt;
  st->targetVelSq = st->targetVel*st->targetVel;
  st->maxVelSq = st->maxVel*st->maxVel;
  st->minVel = st->maxVel*st->minVelMultiplier;
  st->minVelSq = st->minVel*st->minVel;
}

static void computeColorIndices(struct state *st)
{
  int i;
  int schizoLength;

  /* note: colors are used in *reverse* order! */

  /* grayscale */
  for (i = 0; i < st->trailLen; i++) {
    st->grayIndex[st->trailLen-1-i] = 4 + i*16.0/st->trailLen + 0.5;
    if (st->grayIndex[st->trailLen-1-i] > 19) st->grayIndex[st->trailLen-1-i] = 19;
  }

  /* red */
  for (i = 0; i < st->trailLen; i++) {
    st->redIndex[st->trailLen-1-i] = 20 + i*16.0/st->trailLen + 0.5;
    if (st->redIndex[st->trailLen-1-i] > 35) st->redIndex[st->trailLen-1-i] = 35;
  }

  /* blue */
  for (i = 0; i < st->trailLen; i++) {
    st->blueIndex[st->trailLen-1-i] = 36 + i*16.0/st->trailLen + 0.5;
    if (st->blueIndex[st->trailLen-1-i] > 51) st->blueIndex[st->trailLen-1-i] = 51;
  }

  /* gray schizo - same as gray*/
  for (i = 0; i < st->trailLen; i++) {
    st->graySIndex[st->trailLen-1-i] = 4 + i*16.0/st->trailLen + 0.5;
    if (st->graySIndex[st->trailLen-1-i] > 19) st->graySIndex[st->trailLen-1-i] = 19;
  }

  /*schizoLength = st->trailLen/4;
  if (schizoLength < 3) schizoLength = 3;*/
  /* red schizo */
  for (i = 0; i < st->trailLen; i++) {
    /*    redSIndex[trailLen-1-i] = 
	  20 + 16.0*(i%schizoLength)/(schizoLength-1.0) + 0.5;*/
    st->redSIndex[st->trailLen-1-i] = 20 + i*16.0/st->trailLen + 0.5;
    if (st->redSIndex[st->trailLen-1-i] > 35) st->redSIndex[st->trailLen-1-i] = 35;
  }

  schizoLength = st->trailLen/2;
  if (schizoLength < 3) schizoLength = 3;
  /* blue schizo is next */
  for (i = 0; i < st->trailLen; i++) {
    st->blueSIndex[st->trailLen-1-i] = 
      36 + 16.0*(i%schizoLength)/(schizoLength-1.0) + 0.5;
    if (st->blueSIndex[st->trailLen-1-i] > 51) st->blueSIndex[st->trailLen-1-i] = 51;
  }

  /* random is next */
  for (i = 0; i < st->trailLen; i++) {
    st->randomIndex[i] = 52 + random()%(st->numRandomColors);
  }
}

#if 0
static void setParams(bugParams *p)
{
  st->dt = p->dt;
  st->targetVel = p->targetVel;
  st->targetAcc = p->targetAcc;
  st->maxVel = p->maxVel;
  st->maxAcc = p->maxAcc;
  st->noise = p->noise;

  st->nbugs = p->nbugs;
  st->ntargets = p->ntargets;
  st->trailLen = p->trailLen;
  st->colorScheme = p->colorScheme;
  st->changeProb = p->changeProb;
  computeConstants();
  computeColorIndices();
}
#endif

static void drawBugs(struct state *st, int *tColorIdx, int tci0, int tnc,
		     int *colorIdx, int ci0, int nc)
{
  register bug *b;
  register int i, j;
  int temp;

  if (((st->head+1)%st->trailLen) == st->tail) {
    /* first, erase last segment of bugs if necessary */
    temp = (st->tail+1) % st->trailLen;
    
    b = st->bugs;
    for (i = 0; i < st->nbugs; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[0],
		b->hist[st->tail][0], b->hist[st->tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
  
    b = st->targets;
    for (i = 0; i < st->ntargets; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[0],
		b->hist[st->tail][0], b->hist[st->tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    st->tail = (st->tail+1)%st->trailLen;
  }
  
  for (j = st->tail; j != st->head; j = temp) {
    temp = (j+1)%st->trailLen;

    b = st->bugs;
    for (i = 0; i < st->nbugs; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[colorIdx[ci0]],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    
    b = st->targets;
    for (i = 0; i < st->ntargets; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[tColorIdx[tci0]],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    ci0 = (ci0+1)%nc;  
    tci0 = (tci0+1)%tnc;  
  }
}

static void clearBugs(struct state *st)
{
  register bug *b;
  register int i, j;
  int temp;

  st->tail = st->tail-1;
  if (st->tail < 0) st->tail = st->trailLen-1;

  if (((st->head+1)%st->trailLen) == st->tail) {
    /* first, erase last segment of bugs if necessary */
    temp = (st->tail+1) % st->trailLen;
    
    b = st->bugs;
    for (i = 0; i < st->nbugs; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[0],
		b->hist[st->tail][0], b->hist[st->tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
  
    b = st->targets;
    for (i = 0; i < st->ntargets; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[0],
		b->hist[st->tail][0], b->hist[st->tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    st->tail = (st->tail+1)%st->trailLen;
  }
  
  for (j = st->tail; j != st->head; j = temp) {
    temp = (j+1)%st->trailLen;

    b = st->bugs;
    for (i = 0; i < st->nbugs; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[0],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    
    b = st->targets;
    for (i = 0; i < st->ntargets; i++, b++) {
      XDrawLine(st->dpy, st->win, st->fgc[0],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
  }
}

static void updateState(struct state *st)
{
  register int i;
  register bug *b;
  register float ax, ay, temp;
  float theta;
  bug *b2;
  int j;

  st->head = (st->head+1)%st->trailLen;
  
  for (j = 0; j < 5; j++) {
    /* update closets bug for the bug indicated by checkIndex */
    st->checkIndex = (st->checkIndex+1)%st->nbugs;
    b = &st->bugs[st->checkIndex];

    ax = b->closest->pos[0] - b->pos[0];
    ay = b->closest->pos[1] - b->pos[1];
    temp = ax*ax + ay*ay;
    for (i = 0; i < st->ntargets; i++) {
      b2 = &st->targets[i];
      if (b2 == b->closest) continue;
      ax = b2->pos[0] - b->pos[0];
      ay = b2->pos[1] - b->pos[1];
      theta = ax*ax + ay*ay;
      if (theta < temp*2) {
	b->closest = b2;
	temp = theta;
      }
    }
  }
  
  /* update target state */

  b = st->targets;
  for (i = 0; i < st->ntargets; i++, b++) {
    theta = frand(6.28);
    ax = st->targetAcc*cos(theta);
    ay = st->targetAcc*sin(theta);

    b->vel[0] += ax*st->dt;
    b->vel[1] += ay*st->dt;

    /* check velocity */
    temp = sq(b->vel[0]) + sq(b->vel[1]);
    if (temp > st->targetVelSq) {
      temp = st->targetVel/sqrt(temp);
      /* save old vel for acc computation */
      ax = b->vel[0];
      ay = b->vel[1];

      /* compute new velocity */
      b->vel[0] *= temp;
      b->vel[1] *= temp;
      
      /* update acceleration */
      ax = (b->vel[0]-ax)*st->dtInv;
      ay = (b->vel[1]-ay)*st->dtInv;
    }

    /* update position */
    b->pos[0] += b->vel[0]*st->dt + ax*st->halfDtSq;
    b->pos[1] += b->vel[1]*st->dt + ay*st->halfDtSq;

    /* check limits on targets */
    if (b->pos[0] < 0) {
      /* bounce */
      b->pos[0] = -b->pos[0];
      b->vel[0] = -b->vel[0];
    } else if (b->pos[0] >= st->maxx) {
      /* bounce */
      b->pos[0] = 2*st->maxx-b->pos[0];
      b->vel[0] = -b->vel[0];
    }
    if (b->pos[1] < 0) {
      /* bounce */
      b->pos[1] = -b->pos[1];
      b->vel[1] = -b->vel[1];
    } else if (b->pos[1] >= st->maxy) {
      /* bounce */
      b->pos[1] = 2*st->maxy-b->pos[1];
      b->vel[1] = -b->vel[1];
    }

    b->hist[st->head][0] = b->pos[0]*st->xsize;
    b->hist[st->head][1] = b->pos[1]*st->xsize;
  }

  /* update bug state */
  b = st->bugs;
  for (i = 0; i < st->nbugs; i++, b++) {
    theta = atan2(b->closest->pos[1] - b->pos[1] + frand(st->noise),
		  b->closest->pos[0] - b->pos[0] + frand(st->noise));
    ax = st->maxAcc*cos(theta);
    ay = st->maxAcc*sin(theta);

    b->vel[0] += ax*st->dt;
    b->vel[1] += ay*st->dt;

    /* check velocity */
    temp = sq(b->vel[0]) + sq(b->vel[1]);
    if (temp > st->maxVelSq) {
      temp = st->maxVel/sqrt(temp);

      /* save old vel for acc computation */
      ax = b->vel[0];
      ay = b->vel[1];

      /* compute new velocity */
      b->vel[0] *= temp;
      b->vel[1] *= temp;
      
      /* update acceleration */
      ax = (b->vel[0]-ax)*st->dtInv;
      ay = (b->vel[1]-ay)*st->dtInv;
    } else if (temp < st->minVelSq) {
      temp = st->minVel/sqrt(temp);

      /* save old vel for acc computation */
      ax = b->vel[0];
      ay = b->vel[1];

      /* compute new velocity */
      b->vel[0] *= temp;
      b->vel[1] *= temp;
      
      /* update acceleration */
      ax = (b->vel[0]-ax)*st->dtInv;
      ay = (b->vel[1]-ay)*st->dtInv;
    }

    /* update position */
    b->pos[0] += b->vel[0]*st->dt + ax*st->halfDtSq;
    b->pos[1] += b->vel[1]*st->dt + ay*st->halfDtSq;

    /* check limits on targets */
    if (b->pos[0] < 0) {
      /* bounce */
      b->pos[0] = -b->pos[0];
      b->vel[0] = -b->vel[0];
    } else if (b->pos[0] >= st->maxx) {
      /* bounce */
      b->pos[0] = 2*st->maxx-b->pos[0];
      b->vel[0] = -b->vel[0];
    }
    if (b->pos[1] < 0) {
      /* bounce */
      b->pos[1] = -b->pos[1];
      b->vel[1] = -b->vel[1];
    } else if (b->pos[1] >= st->maxy) {
      /* bounce */
      b->pos[1] = 2*st->maxy-b->pos[1];
      b->vel[1] = -b->vel[1];
    }

    b->hist[st->head][0] = b->pos[0]*st->xsize;
    b->hist[st->head][1] = b->pos[1]*st->xsize;
  }
}

static void mutateBug(struct state *st, int which)
{
  int i, j;

  if (which == 0) {
    /* turn bug into target */
    if (st->ntargets < MAX_TARGETS-1 && st->nbugs > 1) {
      i = random() % st->nbugs;
      memcpy((char *)&st->targets[st->ntargets], (char *)&st->bugs[i], sizeof(bug));
      memcpy((char *)&st->bugs[i], (char *)&st->bugs[st->nbugs-1], sizeof(bug));
      st->targets[st->ntargets].pos[0] = frand(st->maxx);
      st->targets[st->ntargets].pos[1] = frand(st->maxy);
      st->nbugs--;
      st->ntargets++;

      for (i = 0; i < st->nbugs; i += st->ntargets) {
	st->bugs[i].closest = &st->targets[st->ntargets-1];
      }
    }
  } else {
    /* turn target into bug */
    if (st->ntargets > 1 && st->nbugs < MAX_BUGS-1) {
      /* pick a target */
      i = random() % st->ntargets;

      /* copy state into a new bug */
      memcpy((char *)&st->bugs[st->nbugs], (char *)&st->targets[i], sizeof(bug));
      st->ntargets--;

      /* pick a target for the new bug */
      st->bugs[st->nbugs].closest = &st->targets[random()%st->ntargets];

      for (j = 0; j < st->nbugs; j++) {
	if (st->bugs[j].closest == &st->targets[st->ntargets]) {
	  st->bugs[j].closest = &st->targets[i];
	} else if (st->bugs[j].closest == &st->targets[i]) {
	  st->bugs[j].closest = &st->targets[random()%st->ntargets];
	}
      }
      st->nbugs++;
      
      /* copy the last ntarget into the one we just deleted */
      memcpy(&st->targets[i], (char *)&st->targets[st->ntargets], sizeof(bug));
    }
  }
}

static void mutateParam(float *param)
{
  *param *= 0.75+frand(0.5);
}

static void randomSmallChange(struct state *st)
{
  int whichCase = 0;

  whichCase = random()%11;

  if (++st->rsc_callDepth > 10) {
    st->rsc_callDepth--;
    return;
  }
  
  switch(whichCase) {
  case 0:
    /* acceleration */
    mutateParam(&st->maxAcc);
    break;

  case 1:
    /* target acceleration */
    mutateParam(&st->targetAcc);
    break;

  case 2:
    /* velocity */
    mutateParam(&st->maxVel);
    break;

  case 3: 
    /* target velocity */
    mutateParam(&st->targetVel);
    break;

  case 4:
    /* noise */
    mutateParam(&st->noise);
    break;

  case 5:
    /* minVelMultiplier */
    mutateParam(&st->minVelMultiplier);
    break;
    
  case 6:
  case 7:
    /* target to bug */
    if (st->ntargets < 2) break;
    mutateBug(st, 1);
    break;

  case 8:
    /* bug to target */
    if (st->nbugs < 2) break;
    mutateBug(st, 0);
    if (st->nbugs < 2) break;
    mutateBug(st, 0);
    break;

  case 9:
    /* color scheme */
    st->colorScheme = random()%NUM_SCHEMES;
    if (st->colorScheme == RANDOM_SCHIZO || st->colorScheme == COLOR_SCHIZO) {
      /* don't use these quite as much */
      st->colorScheme = random()%NUM_SCHEMES;
    }
    break;

  default:
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
  }

  if (st->minVelMultiplier < 0.3) st->minVelMultiplier = 0.3;
  else if (st->minVelMultiplier > 0.9) st->minVelMultiplier = 0.9;
  if (st->noise < 0.01) st->noise = 0.01;
  if (st->maxVel < 0.02) st->maxVel = 0.02;
  if (st->targetVel < 0.02) st->targetVel = 0.02;
  if (st->targetAcc > st->targetVel*0.7) st->targetAcc = st->targetVel*0.7;
  if (st->maxAcc > st->maxVel*0.7) st->maxAcc = st->maxVel*0.7;
  if (st->targetAcc > st->targetVel*0.7) st->targetAcc = st->targetVel*0.7;
  if (st->maxAcc < 0.01) st->maxAcc = 0.01;
  if (st->targetAcc < 0.005) st->targetAcc = 0.005;

  computeConstants(st);
  st->rsc_callDepth--;
}

static void randomBigChange(struct state *st)
{
  int whichCase = 0;
  int temp;

  whichCase = random()%4;
  
  if (++st->rbc_callDepth > 3) {
    st->rbc_callDepth--;
    return;
  }

  switch(whichCase) {
  case 0:
    /* trail length */
    temp = (random()%(MAX_TRAIL_LEN-25)) + 25;
    clearBugs(st);
    st->trailLen = temp;
    computeColorIndices(st);
    initBugs(st);
    break;

  case 1:  
    /* Whee! */
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    randomSmallChange(st);
    break;

  case 2:
    clearBugs(st);
    initBugs(st);
    break;
    
  case 3:
    pickNewTargets(st);
    break;
    
  default:
    temp = random()%st->ntargets;
    st->targets[temp].pos[0] += frand(st->maxx/4)-st->maxx/8;
    st->targets[temp].pos[1] += frand(st->maxy/4)-st->maxy/8;
    /* updateState() will fix bounds */
    break;
  }

  st->rbc_callDepth--;
}

static void updateColorIndex(struct state *st, 
                             int **tColorIdx, int *tci0, int *tnc,
		      int **colorIdx, int *ci0, int *nc)
{
  switch(st->colorScheme) {
  case COLOR_TRAILS:
    *tColorIdx = st->redIndex;
    *tci0 = 0;
    *tnc = st->trailLen;
    *colorIdx = st->blueIndex;
    *ci0 = 0;
    *nc = st->trailLen;
    break;

  case GRAY_SCHIZO:
    *tColorIdx = st->graySIndex;
    *tci0 = st->head;
    *tnc = st->trailLen;
    *colorIdx = st->graySIndex;
    *ci0 = st->head;
    *nc = st->trailLen;
    break;

  case COLOR_SCHIZO:
    *tColorIdx = st->redSIndex;
    *tci0 = st->head;
    *tnc = st->trailLen;
    *colorIdx = st->blueSIndex;
    *ci0 = st->head;
    *nc = st->trailLen;
    break;

  case GRAY_TRAILS:
    *tColorIdx = st->grayIndex;
    *tci0 = 0;
    *tnc = st->trailLen;
    *colorIdx = st->grayIndex;
    *ci0 = 0;
    *nc = st->trailLen;
    break;

  case RANDOM_TRAILS:
    *tColorIdx = st->redIndex;
    *tci0 = 0;
    *tnc = st->trailLen;
    *colorIdx = st->randomIndex;
    *ci0 = 0;
    *nc = st->trailLen;
    break;

  case RANDOM_SCHIZO:
    *tColorIdx = st->redIndex;
    *tci0 = st->head;
    *tnc = st->trailLen;
    *colorIdx = st->randomIndex;
    *ci0 = st->head;
    *nc = st->trailLen;
    break;
  }
}

#if HAVE_GETTIMEOFDAY
static void initTime(struct state *st)
{
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&st->startupTime, NULL);
#else
  gettimeofday(&st->startupTime);
#endif
}

static double getTime(struct state *st)
{
  struct timeval t;
  float f;
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&t, NULL);
#else
  gettimeofday(&t);
#endif
  t.tv_sec -= st->startupTime.tv_sec;
  f = ((double)t.tv_sec) + t.tv_usec*1e-6;
  return f;
}
#endif

static void *
xrayswarm_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;

  st->dpy = d;
  st->win = w;

  st->dt = 0.3;
  st->targetVel = 0.03;
  st->targetAcc = 0.02;
  st->maxVel = 0.05;
  st->maxAcc = 0.03;
  st->noise = 0.01;
  st->minVelMultiplier = 0.5;

  st->nbugs = -1;
  st->ntargets = -1;
  st->trailLen = -1;

  st->colorScheme = /* -1 */  2;
  st->changeProb = 0.08;

  if (!initGraphics(st)) abort();

  computeConstants(st);
  initBugs(st);
  initTime(st);
  computeColorIndices(st);

  if (st->changeProb > 0) {
    for (i = random()%5+5; i >= 0; i--) {
      randomSmallChange(st);
    }
  }

  return st;
}

static unsigned long
xrayswarm_draw (Display *d, Window w, void *closure)
{
  struct state *st = (struct state *) closure;
  unsigned long this_delay = st->delay;

#if HAVE_GETTIMEOFDAY
  st->draw_start = getTime(st);
#endif

    if (st->delay > 0) {
      st->draw_cnt = 2;
      st->dt = DESIRED_DT/2;
    } else {
      st->draw_cnt = 1;
      st->dt = DESIRED_DT;
    }

    for (; st->draw_cnt > 0; st->draw_cnt--) {
      updateState(st);
      updateColorIndex(st, &st->draw_targetColorIndex, &st->draw_targetStartColor, &st->draw_targetNumColors,
		       &st->draw_colorIndex, &st->draw_startColor, &st->draw_numColors);
      drawBugs(st, st->draw_targetColorIndex, st->draw_targetStartColor, st->draw_targetNumColors,
	       st->draw_colorIndex, st->draw_startColor, st->draw_numColors);
    }
#if HAVE_GETTIMEOFDAY
    st->draw_end = getTime(st);
    st->draw_nframes++;

    if (st->draw_end > st->draw_start+0.5) {
      if (frand(1.0) < st->changeProb) randomSmallChange(st);
      if (frand(1.0) < st->changeProb*0.3) randomBigChange(st);
      st->draw_elapsed = st->draw_end-st->draw_start;
	
      st->draw_timePerFrame = st->draw_elapsed/st->draw_nframes - st->delay*1e-6;
      st->draw_fps = st->draw_nframes/st->draw_elapsed;
      /*
      printf("elapsed: %.3f\n", elapsed);
      printf("fps: %.1f  secs per frame: %.3f  delay: %f\n", 
	     fps, timePerFrame, delay);
      */

      if (st->draw_fps > MAX_FPS) {
	st->delay = (1.0/MAX_FPS - (st->draw_timePerFrame + st->delay*1e-6))*1e6;
      } else if (st->dt*st->draw_fps < MIN_FPS*DESIRED_DT) {
	/* need to speed things up somehow */
	if (0 && st->nbugs > 10) {
	  /*printf("reducing bugs to improve speed.\n");*/
	  clearBugs(st);
	  st->nbugs *= st->draw_fps/MIN_FPS;
	  if (st->ntargets >= st->nbugs/2) mutateBug(st, 1);
	} else if (0 && st->dt < 0.3) {
	  /*printf("increasing dt to improve speed.\n");*/
	  st->dt *= MIN_FPS/st->draw_fps;
	  computeConstants(st);
	} else if (st->trailLen > 10) {
	  /*printf("reducing trail length to improve speed.\n");*/
	  clearBugs(st);
	  st->trailLen = st->trailLen * (st->draw_fps/MIN_FPS);
	  if (st->trailLen < 10) st->trailLen = 10;
	  computeColorIndices(st);
	  initBugs(st);
	}
      }

      st->draw_start = getTime(st);
      st->draw_nframes = 0;
    }
#else
    if (frand(1) < st->changeProb*2/100.0) randomSmallChange(st);
    if (frand(1) < st->changeProb*0.3/100.0) randomBigChange(st);
#endif
    
    if (st->delay <= 10000) {
      st->draw_delayAccum += st->delay;
      if (st->draw_delayAccum > 10000) {
	this_delay = st->draw_delayAccum;
	st->draw_delayAccum = 0;
	st->draw_sleepCount = 0;
      }
      if (++st->draw_sleepCount > 2) {
	st->draw_sleepCount = 0;
        this_delay = 10000;
      }
    }

    return this_delay;
}

static void
xrayswarm_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xsize = w;
  st->ysize = h;
  st->xc = st->xsize >> 1;
  st->yc = st->ysize >> 1;
  st->maxy = st->ysize/(float)st->xsize;
}

static Bool
xrayswarm_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
xrayswarm_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}

XSCREENSAVER_MODULE ("XRaySwarm", xrayswarm)
