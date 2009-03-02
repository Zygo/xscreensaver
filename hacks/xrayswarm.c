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
#include <sys/time.h>
#include "screenhack.h"
#include "config.h"


/**********************************************************************
 *								       
 * window crap 
 *
 **********************************************************************/

char *progclass="xrayswarm";

char *defaults [] ={
	".background:		black",
	"*delay:		0",
	0
};

XrmOptionDescRec options [] = {
	{"-delay",".delay",XrmoptionSepArg,0},
	{0,0,0,0}
};

static char colors[768];

static Display *dpy;
static Window win;
static GC fgc[256];
static GC cgc;
static int xsize, ysize;
static int xc, yc;
static unsigned long delay;
static float maxx, maxy;

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

static float dt = 0.3;
static float targetVel = 0.03;
static float targetAcc = 0.02;
static float maxVel = 0.05;
static float maxAcc = 0.03;
static float noise = 0.01;
static float minVelMultiplier = 0.5;

static int nbugs = -1;
static int ntargets = -1;
static int trailLen = -1;

/* vars dependent on those above */
static float dtInv;
static float halfDtSq;
static float targetVelSq;
static float maxVelSq;
static float minVelSq;
static float minVel;

static bug bugs[MAX_BUGS];
static bug targets[MAX_TARGETS];
static int head = 0;
static int tail = 0;
static int colorScheme = -1;
static float changeProb = 0.08;

static int grayIndex[MAX_TRAIL_LEN];
static int redIndex[MAX_TRAIL_LEN];
static int blueIndex[MAX_TRAIL_LEN];
static int graySIndex[MAX_TRAIL_LEN];
static int redSIndex[MAX_TRAIL_LEN];
static int blueSIndex[MAX_TRAIL_LEN];
static int randomIndex[MAX_TRAIL_LEN];
static int numColors;
static int numRandomColors;

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

bugParams good1 = {
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

bugParams *goodParams[] = { 
&good1
};

int numParamSets = 1;

void initCMap(void) {
  int i, n;
  int temp;

  n = 0;

  /* color 0 is black */
  colors[n++] = 0;
  colors[n++] = 0;
  colors[n++] = 0;

  /* color 1 is red */
  colors[n++] = 255;
  colors[n++] = 0;
  colors[n++] = 0;

  /* color 2 is green */
  colors[n++] = 255;
  colors[n++] = 0;
  colors[n++] = 0;

  /* color 3 is blue */
  colors[n++] = 255;
  colors[n++] = 0;
  colors[n++] = 0;

  /* start greyscale colors at 4; 16 levels */
  for (i = 0; i < 16; i++) {
    temp = i*16;
    if (temp > 255) temp = 255;
    colors[n++] = 255 - temp;
    colors[n++] = 255 - temp;
    colors[n++] = 255 - temp;
  }

  /* start red fade at 20; 16 levels */
  for (i = 0; i < 16; i++) {
    temp = i*16;
    if (temp > 255) temp = 255;
    colors[n++] = 255 - temp;
    colors[n++] = 255 - pow(i/16.0+0.001, 0.3)*255;
    colors[n++] = 65 - temp/4;
  }

  /* start blue fade at 36; 16 levels */
  for (i = 0; i < 16; i++) {
    temp = i*16;
    if (temp > 255) temp = 255;
    colors[n++] = 32 - temp/8;
    colors[n++] = 180 - pow(i/16.0+0.001, 0.3)*180;
    colors[n++] = 255 - temp;
  }

  /* random colors start at 52 */
  numRandomColors = MAX_TRAIL_LEN;

  colors[n++] = random()&255;
  colors[n++] = random()&255;
  colors[n++] = colors[n-2]/2 + colors[n-3]/2;

  for (i = 0; i < numRandomColors; i++) {
    colors[n++] = (colors[n-3] + (random()&31) - 16)&255;
    colors[n++] = (colors[n-3] + (random()&31) - 16)&255;
    colors[n++] = colors[n-2]/(float)(i+2) + colors[n-3]/(float)(i+2);
  }
  
  numColors = n/3 + 1;
}

static int initGraphics(void) {
  XGCValues xgcv;
  XWindowAttributes xgwa;
  XSetWindowAttributes xswa;
  Colormap cmap;
  XColor color;
  int n, i;
  
  initCMap();

  XGetWindowAttributes(dpy,win,&xgwa);
  cmap=xgwa.colormap;
  xswa.backing_store=Always;
  XChangeWindowAttributes(dpy,win,CWBackingStore,&xswa);
  xgcv.function=GXcopy;

  delay = get_integer_resource("delay","Integer");
  
  xgcv.foreground=get_pixel_resource ("background", "Background", dpy, cmap);
  fgc[0]=XCreateGC(dpy, win, GCForeground|GCFunction,&xgcv);
  
  n=0;
  if (mono_p) {
    xgcv.foreground=get_pixel_resource ("foreground", "Foreground", dpy, cmap);
    fgc[1]=XCreateGC(dpy,win,GCForeground|GCFunction,&xgcv);
    for (i=0;i<numColors;i+=2) fgc[i]=fgc[0];
    for (i=1;i<numColors;i+=2) fgc[i]=fgc[1];
  } else {
    for (i = 0; i < numColors; i++) {
      color.red=colors[n++]<<8;
      color.green=colors[n++]<<8;
      color.blue=colors[n++]<<8;
      color.flags=DoRed|DoGreen|DoBlue;
      XAllocColor(dpy,cmap,&color);
      xgcv.foreground=color.pixel;
      fgc[i] = XCreateGC(dpy, win, GCForeground | GCFunction,&xgcv);
    }
  }
  cgc = XCreateGC(dpy,win,GCForeground|GCFunction,&xgcv);
  XSetGraphicsExposures(dpy,cgc,False);

  xsize = xgwa.width;
  ysize = xgwa.height;
  xc = xsize >> 1;
  yc = ysize >> 1;

  maxx = 1.0;
  maxy = ysize/(float)xsize;

  if (colorScheme < 0) colorScheme = random()%NUM_SCHEMES;

  return True;
}

static void initBugs(void) {
  register bug *b;
  int i;

  head = tail = 0;

  bzero((char *)bugs, MAX_BUGS*sizeof(bug));
  bzero((char *)targets, MAX_TARGETS*sizeof(bug));

  if (ntargets < 0) ntargets = (0.25+frand(0.75)*frand(1))*MAX_TARGETS;
  if (ntargets < 1) ntargets = 1;

  if (nbugs < 0) nbugs = (0.25+frand(0.75)*frand(1))*MAX_BUGS;
  if (nbugs <= ntargets) nbugs = ntargets+1;

  if (trailLen < 0) {
    trailLen = (1.0 - frand(0.6)*frand(1))*MAX_TRAIL_LEN;
  }

  if (nbugs > MAX_BUGS) nbugs = MAX_BUGS;
  if (ntargets > MAX_TARGETS) ntargets = MAX_TARGETS;
  if (trailLen > MAX_TRAIL_LEN) trailLen = MAX_TRAIL_LEN;

  b = bugs;
  for (i = 0; i < nbugs; i++, b++) {
    b->pos[0] = frand(maxx);
    b->pos[1] = frand(maxy);
    b->vel[0] = frand(maxVel/2);
    b->vel[1] = frand(maxVel/2);

    b->hist[head][0] = b->pos[0]*xsize;
    b->hist[head][1] = b->pos[1]*xsize;
    b->closest = &targets[random()%ntargets];
  }

  b = targets;
  for (i = 0; i < ntargets; i++, b++) {
    b->pos[0] = frand(maxx);
    b->pos[1] = frand(maxy);

    b->vel[0] = frand(targetVel/2);
    b->vel[1] = frand(targetVel/2);

    b->hist[head][0] = b->pos[0]*xsize;
    b->hist[head][1] = b->pos[1]*xsize;
  }
}

static void pickNewTargets(void) {
  register int i;
  register bug *b;

  b = bugs;
  for (i = 0; i < nbugs; i++, b++) {
    b->closest = &targets[random()%ntargets];
  }
}

#if 0
static void addBugs(int numToAdd) {
  register bug *b;
  int i;

  if (numToAdd + nbugs > MAX_BUGS) numToAdd = MAX_BUGS-nbugs;
  else if (numToAdd < 0) numToAdd = 0;

  for (i = 0; i < numToAdd; i++) {
    b = &bugs[random()%nbugs];
    bcopy((char *)b, (char *)&bugs[nbugs+i], sizeof(bug));
    b->closest = &targets[random()%ntargets];
  }

  nbugs += numToAdd;
}

static void addTargets(int numToAdd) {
  register bug *b;
  int i;

  if (numToAdd + ntargets > MAX_TARGETS) numToAdd = MAX_TARGETS-ntargets;
  else if (numToAdd < 0) numToAdd = 0;

  for (i = 0; i < numToAdd; i++) {
    b = &targets[random()%ntargets];
    bcopy((char *)b, (char *)&targets[ntargets+i], sizeof(bug));
    b->closest = &targets[random()%ntargets];
  }

  ntargets += numToAdd;
}
#endif

static void computeConstants(void) {
  halfDtSq = dt*dt*0.5;
  dtInv = 1.0/dt;
  targetVelSq = targetVel*targetVel;
  maxVelSq = maxVel*maxVel;
  minVel = maxVel*minVelMultiplier;
  minVelSq = minVel*minVel;
}

void computeColorIndices(void) {
  int i;
  int schizoLength;

  /* note: colors are used in *reverse* order! */

  /* grayscale */
  for (i = 0; i < trailLen; i++) {
    grayIndex[trailLen-1-i] = 4 + i*16.0/trailLen + 0.5;
    if (grayIndex[trailLen-1-i] > 19) grayIndex[trailLen-1-i] = 19;
  }

  /* red */
  for (i = 0; i < trailLen; i++) {
    redIndex[trailLen-1-i] = 20 + i*16.0/trailLen + 0.5;
    if (redIndex[trailLen-1-i] > 35) redIndex[trailLen-1-i] = 35;
  }

  /* blue */
  for (i = 0; i < trailLen; i++) {
    blueIndex[trailLen-1-i] = 36 + i*16.0/trailLen + 0.5;
    if (blueIndex[trailLen-1-i] > 51) blueIndex[trailLen-1-i] = 51;
  }

  /* gray schizo - same as gray*/
  for (i = 0; i < trailLen; i++) {
    graySIndex[trailLen-1-i] = 4 + i*16.0/trailLen + 0.5;
    if (graySIndex[trailLen-1-i] > 19) graySIndex[trailLen-1-i] = 19;
  }

  schizoLength = trailLen/4;
  if (schizoLength < 3) schizoLength = 3;
  /* red schizo */
  for (i = 0; i < trailLen; i++) {
    /*    redSIndex[trailLen-1-i] = 
	  20 + 16.0*(i%schizoLength)/(schizoLength-1.0) + 0.5;*/
    redSIndex[trailLen-1-i] = 20 + i*16.0/trailLen + 0.5;
    if (redSIndex[trailLen-1-i] > 35) redSIndex[trailLen-1-i] = 35;
  }

  schizoLength = trailLen/2;
  if (schizoLength < 3) schizoLength = 3;
  /* blue schizo is next */
  for (i = 0; i < trailLen; i++) {
    blueSIndex[trailLen-1-i] = 
      36 + 16.0*(i%schizoLength)/(schizoLength-1.0) + 0.5;
    if (blueSIndex[trailLen-1-i] > 51) blueSIndex[trailLen-1-i] = 51;
  }

  /* random is next */
  for (i = 0; i < trailLen; i++) {
    randomIndex[i] = 52 + random()%(numRandomColors);
  }
}

#if 0
static void setParams(bugParams *p) {
  dt = p->dt;
  targetVel = p->targetVel;
  targetAcc = p->targetAcc;
  maxVel = p->maxVel;
  maxAcc = p->maxAcc;
  noise = p->noise;

  nbugs = p->nbugs;
  ntargets = p->ntargets;
  trailLen = p->trailLen;
  colorScheme = p->colorScheme;
  changeProb = p->changeProb;
  computeConstants();
  computeColorIndices();
}
#endif

static void drawBugs(int *tColorIdx, int tci0, int tnc,
		     int *colorIdx, int ci0, int nc) {
  register bug *b;
  register int i, j;
  int temp;

  if (((head+1)%trailLen) == tail) {
    /* first, erase last segment of bugs if necessary */
    temp = (tail+1) % trailLen;
    
    b = bugs;
    for (i = 0; i < nbugs; i++, b++) {
      XDrawLine(dpy, win, fgc[0],
		b->hist[tail][0], b->hist[tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
  
    b = targets;
    for (i = 0; i < ntargets; i++, b++) {
      XDrawLine(dpy, win, fgc[0],
		b->hist[tail][0], b->hist[tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    tail = (tail+1)%trailLen;
  }
  
  for (j = tail; j != head; j = temp) {
    temp = (j+1)%trailLen;

    b = bugs;
    for (i = 0; i < nbugs; i++, b++) {
      XDrawLine(dpy, win, fgc[colorIdx[ci0]],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    
    b = targets;
    for (i = 0; i < ntargets; i++, b++) {
      XDrawLine(dpy, win, fgc[tColorIdx[tci0]],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    ci0 = (ci0+1)%nc;  
    tci0 = (tci0+1)%tnc;  
  }
}

static void clearBugs(void) {
  register bug *b;
  register int i, j;
  int temp;

  tail = tail-1;
  if (tail < 0) tail = trailLen-1;

  if (((head+1)%trailLen) == tail) {
    /* first, erase last segment of bugs if necessary */
    temp = (tail+1) % trailLen;
    
    b = bugs;
    for (i = 0; i < nbugs; i++, b++) {
      XDrawLine(dpy, win, fgc[0],
		b->hist[tail][0], b->hist[tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
  
    b = targets;
    for (i = 0; i < ntargets; i++, b++) {
      XDrawLine(dpy, win, fgc[0],
		b->hist[tail][0], b->hist[tail][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    tail = (tail+1)%trailLen;
  }
  
  for (j = tail; j != head; j = temp) {
    temp = (j+1)%trailLen;

    b = bugs;
    for (i = 0; i < nbugs; i++, b++) {
      XDrawLine(dpy, win, fgc[0],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
    
    b = targets;
    for (i = 0; i < ntargets; i++, b++) {
      XDrawLine(dpy, win, fgc[0],
		b->hist[j][0], b->hist[j][1],
		b->hist[temp][0], b->hist[temp][1]);
    }
  }
}

void updateState(void) {
  register int i;
  register bug *b;
  register float ax, ay, temp;
  float theta;
  static int checkIndex = 0;
  bug *b2;
  int j;

  head = (head+1)%trailLen;
  
  for (j = 0; j < 5; j++) {
    /* update closets bug for the bug indicated by checkIndex */
    checkIndex = (checkIndex+1)%nbugs;
    b = &bugs[checkIndex];

    ax = b->closest->pos[0] - b->pos[0];
    ay = b->closest->pos[1] - b->pos[1];
    temp = ax*ax + ay*ay;
    for (i = 0; i < ntargets; i++) {
      b2 = &targets[i];
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

  b = targets;
  for (i = 0; i < ntargets; i++, b++) {
    theta = frand(6.28);
    ax = targetAcc*cos(theta);
    ay = targetAcc*sin(theta);

    b->vel[0] += ax*dt;
    b->vel[1] += ay*dt;

    /* check velocity */
    temp = sq(b->vel[0]) + sq(b->vel[1]);
    if (temp > targetVelSq) {
      temp = targetVel/sqrt(temp);
      /* save old vel for acc computation */
      ax = b->vel[0];
      ay = b->vel[1];

      /* compute new velocity */
      b->vel[0] *= temp;
      b->vel[1] *= temp;
      
      /* update acceleration */
      ax = (b->vel[0]-ax)*dtInv;
      ay = (b->vel[1]-ay)*dtInv;
    }

    /* update position */
    b->pos[0] += b->vel[0]*dt + ax*halfDtSq;
    b->pos[1] += b->vel[1]*dt + ay*halfDtSq;

    /* check limits on targets */
    if (b->pos[0] < 0) {
      /* bounce */
      b->pos[0] = -b->pos[0];
      b->vel[0] = -b->vel[0];
    } else if (b->pos[0] >= maxx) {
      /* bounce */
      b->pos[0] = 2*maxx-b->pos[0];
      b->vel[0] = -b->vel[0];
    }
    if (b->pos[1] < 0) {
      /* bounce */
      b->pos[1] = -b->pos[1];
      b->vel[1] = -b->vel[1];
    } else if (b->pos[1] >= maxy) {
      /* bounce */
      b->pos[1] = 2*maxy-b->pos[1];
      b->vel[1] = -b->vel[1];
    }

    b->hist[head][0] = b->pos[0]*xsize;
    b->hist[head][1] = b->pos[1]*xsize;
  }

  /* update bug state */
  b = bugs;
  for (i = 0; i < nbugs; i++, b++) {
    theta = atan2(b->closest->pos[1] - b->pos[1] + frand(noise),
		  b->closest->pos[0] - b->pos[0] + frand(noise));
    ax = maxAcc*cos(theta);
    ay = maxAcc*sin(theta);

    b->vel[0] += ax*dt;
    b->vel[1] += ay*dt;

    /* check velocity */
    temp = sq(b->vel[0]) + sq(b->vel[1]);
    if (temp > maxVelSq) {
      temp = maxVel/sqrt(temp);

      /* save old vel for acc computation */
      ax = b->vel[0];
      ay = b->vel[1];

      /* compute new velocity */
      b->vel[0] *= temp;
      b->vel[1] *= temp;
      
      /* update acceleration */
      ax = (b->vel[0]-ax)*dtInv;
      ay = (b->vel[1]-ay)*dtInv;
    } else if (temp < minVelSq) {
      temp = minVel/sqrt(temp);

      /* save old vel for acc computation */
      ax = b->vel[0];
      ay = b->vel[1];

      /* compute new velocity */
      b->vel[0] *= temp;
      b->vel[1] *= temp;
      
      /* update acceleration */
      ax = (b->vel[0]-ax)*dtInv;
      ay = (b->vel[1]-ay)*dtInv;
    }

    /* update position */
    b->pos[0] += b->vel[0]*dt + ax*halfDtSq;
    b->pos[1] += b->vel[1]*dt + ay*halfDtSq;

    /* check limits on targets */
    if (b->pos[0] < 0) {
      /* bounce */
      b->pos[0] = -b->pos[0];
      b->vel[0] = -b->vel[0];
    } else if (b->pos[0] >= maxx) {
      /* bounce */
      b->pos[0] = 2*maxx-b->pos[0];
      b->vel[0] = -b->vel[0];
    }
    if (b->pos[1] < 0) {
      /* bounce */
      b->pos[1] = -b->pos[1];
      b->vel[1] = -b->vel[1];
    } else if (b->pos[1] >= maxy) {
      /* bounce */
      b->pos[1] = 2*maxy-b->pos[1];
      b->vel[1] = -b->vel[1];
    }

    b->hist[head][0] = b->pos[0]*xsize;
    b->hist[head][1] = b->pos[1]*xsize;
  }
}

void mutateBug(int which) {
  int i, j;

  if (which == 0) {
    /* turn bug into target */
    if (ntargets < MAX_TARGETS-1 && nbugs > 1) {
      i = random() % nbugs;
      bcopy((char *)&bugs[i], (char *)&targets[ntargets], sizeof(bug));
      bcopy((char *)&bugs[nbugs-1], (char *)&bugs[i], sizeof(bug));
      targets[ntargets].pos[0] = frand(maxx);
      targets[ntargets].pos[1] = frand(maxy);
      nbugs--;
      ntargets++;

      for (i = 0; i < nbugs; i += ntargets) {
	bugs[i].closest = &targets[ntargets-1];
      }
    }
  } else {
    /* turn target into bug */
    if (ntargets > 1 && nbugs < MAX_BUGS-1) {
      /* pick a target */
      i = random() % ntargets;

      /* copy state into a new bug */
      bcopy((char *)&targets[i], (char *)&bugs[nbugs], sizeof(bug));
      ntargets--;

      /* pick a target for the new bug */
      bugs[nbugs].closest = &targets[random()%ntargets];

      for (j = 0; j < nbugs; j++) {
	if (bugs[j].closest == &targets[ntargets]) {
	  bugs[j].closest = &targets[i];
	} else if (bugs[j].closest == &targets[i]) {
	  bugs[j].closest = &targets[random()%ntargets];
	}
      }
      nbugs++;
      
      /* copy the last ntarget into the one we just deleted */
      bcopy((char *)&targets[ntargets], &targets[i], sizeof(bug));
    }
  }
}

void mutateParam(float *param) {
  *param *= 0.75+frand(0.5);
}

void randomSmallChange(void) {
  int whichCase = 0;
  static int callDepth = 0;

  whichCase = random()%11;

  if (++callDepth > 10) {
    callDepth--;
    return;
  }
  
  switch(whichCase) {
  case 0:
    /* acceleration */
    mutateParam(&maxAcc);
    break;

  case 1:
    /* target acceleration */
    mutateParam(&targetAcc);
    break;

  case 2:
    /* velocity */
    mutateParam(&maxVel);
    break;

  case 3: 
    /* target velocity */
    mutateParam(&targetVel);
    break;

  case 4:
    /* noise */
    mutateParam(&noise);
    break;

  case 5:
    /* minVelMultiplier */
    mutateParam(&minVelMultiplier);
    break;
    
  case 6:
  case 7:
    /* target to bug */
    if (ntargets < 2) break;
    mutateBug(1);
    break;

  case 8:
    /* bug to target */
    if (nbugs < 2) break;
    mutateBug(0);
    if (nbugs < 2) break;
    mutateBug(0);
    break;

  case 9:
    /* color scheme */
    colorScheme = random()%NUM_SCHEMES;
    if (colorScheme == RANDOM_SCHIZO || colorScheme == COLOR_SCHIZO) {
      /* don't use these quite as much */
      colorScheme = random()%NUM_SCHEMES;
    }
    break;

  default:
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
  }

  if (minVelMultiplier < 0.3) minVelMultiplier = 0.3;
  else if (minVelMultiplier > 0.9) minVelMultiplier = 0.9;
  if (noise < 0.01) noise = 0.01;
  if (maxVel < 0.02) maxVel = 0.02;
  if (targetVel < 0.02) targetVel = 0.02;
  if (targetAcc > targetVel*0.7) targetAcc = targetVel*0.7;
  if (maxAcc > maxVel*0.7) maxAcc = maxVel*0.7;
  if (targetAcc > targetVel*0.7) targetAcc = targetVel*0.7;
  if (maxAcc < 0.01) maxAcc = 0.01;
  if (targetAcc < 0.005) targetAcc = 0.005;

  computeConstants();
  callDepth--;
}

void randomBigChange(void) {
  static int whichCase = 0;
  static int callDepth = 0;
  int temp;

  whichCase = random()%4;
  
  if (++callDepth > 3) {
    callDepth--;
    return;
  }

  switch(whichCase) {
  case 0:
    /* trail length */
    temp = (random()%(MAX_TRAIL_LEN-25)) + 25;
    clearBugs();
    trailLen = temp;
    computeColorIndices();
    initBugs();
    break;

  case 1:  
    /* Whee! */
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    randomSmallChange();
    break;

  case 2:
    clearBugs();
    initBugs();
    break;
    
  case 3:
    pickNewTargets();
    break;
    
  default:
    temp = random()%ntargets;
    targets[temp].pos[0] += frand(maxx/4)-maxx/8;
    targets[temp].pos[1] += frand(maxy/4)-maxy/8;
    /* updateState() will fix bounds */
    break;
  }

  callDepth--;
}

void updateColorIndex(int **tColorIdx, int *tci0, int *tnc,
		      int **colorIdx, int *ci0, int *nc) {
  switch(colorScheme) {
  case COLOR_TRAILS:
    *tColorIdx = redIndex;
    *tci0 = 0;
    *tnc = trailLen;
    *colorIdx = blueIndex;
    *ci0 = 0;
    *nc = trailLen;
    break;

  case GRAY_SCHIZO:
    *tColorIdx = graySIndex;
    *tci0 = head;
    *tnc = trailLen;
    *colorIdx = graySIndex;
    *ci0 = head;
    *nc = trailLen;
    break;

  case COLOR_SCHIZO:
    *tColorIdx = redSIndex;
    *tci0 = head;
    *tnc = trailLen;
    *colorIdx = blueSIndex;
    *ci0 = head;
    *nc = trailLen;
    break;

  case GRAY_TRAILS:
    *tColorIdx = grayIndex;
    *tci0 = 0;
    *tnc = trailLen;
    *colorIdx = grayIndex;
    *ci0 = 0;
    *nc = trailLen;
    break;

  case RANDOM_TRAILS:
    *tColorIdx = redIndex;
    *tci0 = 0;
    *tnc = trailLen;
    *colorIdx = randomIndex;
    *ci0 = 0;
    *nc = trailLen;
    break;

  case RANDOM_SCHIZO:
    *tColorIdx = redIndex;
    *tci0 = head;
    *tnc = trailLen;
    *colorIdx = randomIndex;
    *ci0 = head;
    *nc = trailLen;
    break;
  }
}

#if HAVE_GETTIMEOFDAY
static struct timeval startupTime;
static void initTime(void) {
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&startupTime, NULL);
#else
  gettimeofday(&startupTime);
#endif
}

static double getTime(void) {
  struct timeval t;
  float f;
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&t, NULL);
#else
  gettimeofday(&t);
#endif
  t.tv_sec -= startupTime.tv_sec;
  f = ((double)t.tv_sec) + t.tv_usec*1e-6;
  return f;
}
#endif

void screenhack(Display *d, Window w) {
  int nframes, i;
  float fps;
  float timePerFrame, elapsed;
  int *targetColorIndex, *colorIndex;
  int targetStartColor, targetNumColors;
  int startColor, numColors;
  double start, end;
  int cnt;

  dpy = d;
  win = w;

  if (!initGraphics()) return;

  computeConstants();
  initBugs();
  initTime();
  computeColorIndices();

  if (changeProb > 0) {
    for (i = random()%5+5; i >= 0; i--) {
      randomSmallChange();
    }
  }

  nframes = 0;
#if HAVE_GETTIMEOFDAY
  start = getTime();
#endif

  while (1) {
    if (delay > 0) {
      cnt = 2;
      dt = DESIRED_DT/2;
    } else {
      cnt = 1;
      dt = DESIRED_DT;
    }

    for (; cnt > 0; cnt--) {
      updateState();
      updateColorIndex(&targetColorIndex, &targetStartColor, &targetNumColors,
		       &colorIndex, &startColor, &numColors);
      drawBugs(targetColorIndex, targetStartColor, targetNumColors,
	       colorIndex, startColor, numColors);
      XSync(dpy, False);
      screenhack_handle_events (dpy);
    }
#if HAVE_GETTIMEOFDAY
    end = getTime();
    nframes++;

    if (end > start+0.5) {
      if (frand(1.0) < changeProb) randomSmallChange();
      if (frand(1.0) < changeProb*0.3) randomBigChange();
      elapsed = end-start;
	
      timePerFrame = elapsed/nframes - delay*1e-6;
      fps = nframes/elapsed;
      /*
      printf("elapsed: %.3f\n", elapsed);
      printf("fps: %.1f  secs per frame: %.3f  delay: %f\n", 
	     fps, timePerFrame, delay);
      */

      if (fps > MAX_FPS) {
#if 0	  
	if (trailLen >= MAX_TRAIL_LEN) {
	  addBugs(nbugs);
	  addTargets(ntargets);
	} else {
	  /* make dt smaller, but use longer tail */
	  clearBugs();
	  dt = 0.1*trailLen/MAX_TRAIL_LEN;
	  trailLen = MAX_TRAIL_LEN;
	  computeColorIndices();
	  initBugs();
	}
#endif
	delay = (1.0/MAX_FPS - (timePerFrame + delay*1e-6))*1e6;
	
      } else if (dt*fps < MIN_FPS*DESIRED_DT) {
	/* need to speed things up somehow */
	if (0 && nbugs > 10) {
	  /*printf("reducing bugs to improve speed.\n");*/
	  clearBugs();
	  nbugs *= fps/MIN_FPS;
	  if (ntargets >= nbugs/2) mutateBug(1);
	} else if (0 && dt < 0.3) {
	  /*printf("increasing dt to improve speed.\n");*/
	  dt *= MIN_FPS/fps;
	  computeConstants();
	} else if (trailLen > 10) {
	  /*printf("reducing trail length to improve speed.\n");*/
	  clearBugs();
	  trailLen = trailLen * (fps/MIN_FPS);
	  if (trailLen < 10) trailLen = 10;
	  computeColorIndices();
	  initBugs();
	}
      }

      start = getTime();
      nframes = 0;
    }
#else
    if (frand(1) < changeProb*2/100.0) randomSmallChange();
    if (frand(1) < changeProb*0.3/100.0) randomBigChange();
#endif

    if (delay > 0) usleep(delay);
  }
}
