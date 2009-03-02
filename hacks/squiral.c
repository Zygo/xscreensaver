/* squiral, by "Jeff Epler" <jepler@inetnebr.com>, 18-mar-1999.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhack.h"
#include "colors.h"
#include "erase.h"
#include "yarandom.h"

#define R(x)  (abs(random())%x)
#define PROB(x) (frand(1.0) < (x))

#define NCOLORSMAX 255
static int width, height, count, cycle;
static double frac, disorder, handedness;
static int ncolors=0;
static GC draw_gc, erase_gc;
static XColor colors[NCOLORSMAX];
#define STATES 8

/* 0- 3 left-winding  */
/* 4- 7 right-winding */

#define CLEAR1(x,y) (!fill[((y)%height)*width+(x)%width])
#define MOVE1(x,y) (fill[((y)%height)*width+(x)%width]=1, XDrawPoint(dpy, window, draw_gc, (x)%width,(y)%height), cov++)

static int cov;
static int dirh[4];
static int dirv[4];

static int *fill;

#define CLEARDXY(x,y,dx,dy) CLEAR1(x+dx, y+dy) && CLEAR1(x+dx+dx, y+dy+dy)
#define MOVEDXY(x,y,dx,dy)  MOVE1 (x+dx, y+dy), MOVE1 (x+dx+dx, y+dy+dy)

#define CLEAR(d) CLEARDXY(w->h,w->v, dirh[d],dirv[d])
#define MOVE(d) (XSetForeground(dpy, draw_gc, colors[w->c].pixel), \
                  MOVEDXY(w->h,w->v, dirh[d],dirv[d]), \
		  w->h=w->h+dirh[d]*2, \
		  w->v=w->v+dirv[d]*2, dir=d)

#define RANDOM (void) (w->h = R(width), w->v = R(height), w->c = R(ncolors), \
		  type=R(2), dir=R(4), (cycle && (w->cc=R(3)+ncolors)))

struct worm {
    int h;
    int v;
    int s;
    int c;
    int cc;
} *worms;

#define SUCC(x) ((x+1)%4)
#define PRED(x) ((x+3)%4)
#define CCW     PRED(dir)
#define CW      SUCC(dir)
#define REV     ((dir+2)%4)
#define STR     (dir)
#define TRY(x)  if (CLEAR(x)) { MOVE(x); break; }

static void
do_worm(Display *dpy, Window window, struct worm *w)
{
    int type = w->s / 4;
    int dir = w->s % 4;

    w->c = (w->c+w->cc) % ncolors;

    if (PROB(disorder)) type=PROB(handedness);
    switch(type) {
    case 0: /* CCW */
	TRY(CCW)
	TRY(STR)
	TRY(CW)
	RANDOM;
	break;
    case 1: /* CW */
	TRY(CW)
	TRY(STR)
	TRY(CCW)
	RANDOM;
	break;
    }
    w->s = type*4+dir;
    w->h = w->h % width;
    w->v = w->v % height;
}

static void
init_squiral(Display *dpy, Window window)
{
    XGCValues gcv;
    Colormap cmap;
    XWindowAttributes xgwa;
    Bool writeable = False;
    int i;

    XClearWindow(dpy, window);
    XGetWindowAttributes(dpy, window, &xgwa);
    width  = xgwa.width;
    height = xgwa.height;

    cmap = xgwa.colormap;
    gcv.foreground = get_pixel_resource("foreground",
	"Foreground", dpy, cmap);
    draw_gc = XCreateGC(dpy, window, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource ("background", "Background",dpy, cmap);
    erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
    cmap = xgwa.colormap;
    if( ncolors ) {
        free_colors(dpy, cmap, colors, ncolors);
        ncolors = 0;
    }
    if( mono_p ) {
      ncolors=1;
      colors[0].pixel=get_pixel_resource("foreground","Foreground", dpy, cmap);
    } else {
      ncolors = get_integer_resource("ncolors", "Integer");
      if (ncolors < 0 || ncolors > NCOLORSMAX)
        ncolors = NCOLORSMAX;
      make_uniform_colormap(dpy, xgwa.visual, cmap, colors, &ncolors, True,
	  &writeable, False);
      if (ncolors <= 0) {
        ncolors = 1;
        colors[0].pixel=get_pixel_resource("foreground","Foreground",dpy, cmap);
      }
    }
    count= get_integer_resource("count", "Integer");
    frac = get_integer_resource("fill",  "Integer")*0.01;
    cycle= get_boolean_resource("cycle", "Cycle");
    disorder=get_float_resource("disorder", "Float");
    handedness=get_float_resource("handedness", "Float");

    if(frac<0.01) frac=0.01;
    if(frac>0.99) frac=0.99;
    if(count==0) count=width/32;
    if(count<1) count=1;
    if(count>1000) count=1000;

    if(worms) free(worms);
    if(fill)  free(fill);

    worms=calloc(count, sizeof(struct worm));
    fill=calloc(width*height, sizeof(int));

    dirh[0]=0; dirh[1]=1; dirh[2]=0; dirh[3]=width-1;
    dirv[0]=height-1; dirv[1]=0; dirv[2]=1; dirv[3]=0;
    for(i=0;i<count;i++) {
	worms[i].h=R(width);
	worms[i].v=R(height);
	worms[i].s=R(4)+4*PROB(handedness);
	worms[i].c=R(ncolors);
	if(cycle) { worms[i].cc=R(3)+ncolors; }
	else worms[i].cc=0;
    }
}

void
screenhack(Display *dpy, Window window)
{
   int inclear, i;
   int delay= get_integer_resource("delay", "Integer");
   init_squiral(dpy, window);
   cov=0; inclear=height;
   while(1) {
	if(inclear<height) {
	    XDrawLine(dpy, window, erase_gc, 0, inclear, width-1, inclear);
	    memset(&fill[inclear*width], 0, sizeof(int)*width);
	    XDrawLine(dpy, window, erase_gc, 0, height-inclear-1, width-1,
	   	    height-inclear-1);
	    memset(&fill[(height-inclear-1)*width], 0, sizeof(int)*width);
	    inclear++;
	    XDrawLine(dpy, window, erase_gc, 0, inclear, width-1, inclear);
	    memset(&fill[inclear*width], 0, sizeof(int)*width);
	    XDrawLine(dpy, window, erase_gc, 0, height-inclear-1, width-1,
		    height-inclear-1);
	    memset(&fill[(height-inclear-1)*width], 0, sizeof(int)*width);
	    inclear++;
	    if(inclear>height/2) inclear=height;
	}
	else if(cov>(frac*width*height)) {
	    inclear=0;
	    cov=0;
	}
	for(i=0;i<count;i++) do_worm(dpy, window, &worms[i]);
	screenhack_handle_events(dpy);
	usleep(delay);
    }	
}

char *progclass="Squiral";

char *defaults[] = {
  ".background: black",
  ".foreground: white",
  "*fill:       75",
  "*count:      0",
  "*ncolors:    100",
  "*delay:      1000",
  "*disorder:   0.005",
  "*cycle:      False",
  "*handedness: .5",
  0
};

XrmOptionDescRec options[] = {
    {"-fill", ".fill", XrmoptionSepArg, 0},
    {"-count", ".count", XrmoptionSepArg, 0},
    {"-delay", ".delay", XrmoptionSepArg, 0},
    {"-disorder", ".disorder", XrmoptionSepArg, 0},
    {"-handedness", ".handedness", XrmoptionSepArg, 0},
    {"-ncolors", ".ncolors", XrmoptionSepArg, 0},
    {"-cycle", ".cycle", XrmoptionNoArg, "True"},
    {"-no-cycle", ".cycle", XrmoptionNoArg, "False"},
    { 0, 0, 0, 0 }
};
