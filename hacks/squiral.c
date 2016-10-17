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

#define R(x)  (random()%x)
#define PROB(x) (frand(1.0) < (x))

#define NCOLORSMAX 255
#define STATES 8

/* 0- 3 left-winding  */
/* 4- 7 right-winding */

struct worm {
    int h;
    int v;
    int s;
    int c;
    int cc;
};


struct state {
  Display *dpy;
  Window window;

   int width, height, count, cycle;
   double frac, disorder, handedness;
   int ncolors;
   GC draw_gc, erase_gc;
   XColor colors[NCOLORSMAX];

   int delay;

   int cov;
   int dirh[4];
   int dirv[4];

   int *fill;

   struct worm *worms;
   int inclear;
};

#define CLEAR1(x,y) (!st->fill[((y)%st->height)*st->width+(x)%st->width])
#define MOVE1(x,y) (st->fill[((y)%st->height)*st->width+(x)%st->width]=1, XDrawPoint(st->dpy, st->window, st->draw_gc, (x)%st->width,(y)%st->height), st->cov++)

#define CLEARDXY(x,y,dx,dy) CLEAR1(x+dx, y+dy) && CLEAR1(x+dx+dx, y+dy+dy)
#define MOVEDXY(x,y,dx,dy)  MOVE1 (x+dx, y+dy), MOVE1 (x+dx+dx, y+dy+dy)

#define CLEAR(d) CLEARDXY(w->h,w->v, st->dirh[d],st->dirv[d])
#define MOVE(d) (XSetForeground(st->dpy, st->draw_gc, st->colors[w->c].pixel), \
                  MOVEDXY(w->h,w->v, st->dirh[d],st->dirv[d]), \
		  w->h=w->h+st->dirh[d]*2, \
		  w->v=w->v+st->dirv[d]*2, dir=d)

#define RANDOM (void) (w->h = R(st->width), w->v = R(st->height), w->c = R(st->ncolors), \
		  type=R(2), dir=R(4), (st->cycle && (w->cc=R(3)+st->ncolors)))



#define SUCC(x) ((x+1)%4)
#define PRED(x) ((x+3)%4)
#define CCW     PRED(dir)
#define CW      SUCC(dir)
#define REV     ((dir+2)%4)
#define STR     (dir)
#define TRY(x)  if (CLEAR(x)) { MOVE(x); break; }

static void
do_worm(struct state *st, struct worm *w)
{
    int type = w->s / 4;
    int dir = w->s % 4;

    w->c = (w->c+w->cc) % st->ncolors;

    if (PROB(st->disorder)) type=PROB(st->handedness);
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
    w->h = w->h % st->width;
    w->v = w->v % st->height;
}

static void
squiral_init_1 (struct state *st)
{
    int i;
    if (st->worms) free (st->worms);
    if (st->fill) free (st->fill);

    st->worms=calloc(st->count, sizeof(struct worm));
    st->fill=calloc(st->width*st->height, sizeof(int));

    st->dirh[0]=0; st->dirh[1]=1; st->dirh[2]=0; st->dirh[3]=st->width-1;
    st->dirv[0]=st->height-1; st->dirv[1]=0; st->dirv[2]=1; st->dirv[3]=0;
    for(i=0;i<st->count;i++) {
	st->worms[i].h=R(st->width);
	st->worms[i].v=R(st->height);
	st->worms[i].s=R(4)+4*PROB(st->handedness);
	st->worms[i].c=R(st->ncolors);
	if(st->cycle) { st->worms[i].cc=R(3)+st->ncolors; }
	else st->worms[i].cc=0;
    }
}

static void *
squiral_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
    XGCValues gcv;
    Colormap cmap;
    XWindowAttributes xgwa;
    Bool writeable = False;

    st->dpy = dpy;
    st->window = window;

   st->delay= get_integer_resource(st->dpy, "delay", "Integer");

    XClearWindow(st->dpy, st->window);
    XGetWindowAttributes(st->dpy, st->window, &xgwa);
    st->width  = xgwa.width;
    st->height = xgwa.height;

    cmap = xgwa.colormap;
    gcv.foreground = get_pixel_resource(st->dpy, cmap, "foreground",
	"Foreground");
    st->draw_gc = XCreateGC(st->dpy, st->window, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource (st->dpy, cmap, "background", "Background");
    st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
    cmap = xgwa.colormap;
    if( st->ncolors ) {
        free_colors(xgwa.screen, cmap, st->colors, st->ncolors);
        st->ncolors = 0;
    }
    if( mono_p ) {
      st->ncolors=1;
      st->colors[0].pixel=get_pixel_resource(st->dpy, cmap, "foreground","Foreground");
    } else {
      st->ncolors = get_integer_resource(st->dpy, "ncolors", "Integer");
      if (st->ncolors < 0 || st->ncolors > NCOLORSMAX)
        st->ncolors = NCOLORSMAX;
      make_uniform_colormap(xgwa.screen, xgwa.visual, cmap,
                            st->colors, &st->ncolors, True,
	  &writeable, False);
      if (st->ncolors <= 0) {
        st->ncolors = 1;
        st->colors[0].pixel=get_pixel_resource(st->dpy, cmap, "foreground","Foreground");
      }
    }
    st->count= get_integer_resource(st->dpy, "count", "Integer");
    st->frac = get_integer_resource(st->dpy, "fill",  "Integer")*0.01;
    st->cycle= get_boolean_resource(st->dpy, "cycle", "Cycle");
    st->disorder=get_float_resource(st->dpy, "disorder", "Float");
    st->handedness=get_float_resource(st->dpy, "handedness", "Float");

    if(st->frac<0.01) st->frac=0.01;
    if(st->frac>0.99) st->frac=0.99;
    if(st->count==0) st->count=st->width/32;
    if(st->count<1) st->count=1;
    if(st->count>1000) st->count=1000;

    if(st->worms) free(st->worms);
    if(st->fill)  free(st->fill);

    squiral_init_1 (st);

    return st;
}

static unsigned long
squiral_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;

  if(st->inclear<st->height) {
    XDrawLine(st->dpy, st->window, st->erase_gc, 0, st->inclear, st->width-1, st->inclear);
    memset(&st->fill[st->inclear*st->width], 0, sizeof(int)*st->width);
    XDrawLine(st->dpy, st->window, st->erase_gc, 0, st->height-st->inclear-1, st->width-1,
              st->height-st->inclear-1);
    memset(&st->fill[(st->height-st->inclear-1)*st->width], 0, sizeof(int)*st->width);
    st->inclear++;
    XDrawLine(st->dpy, st->window, st->erase_gc, 0, st->inclear, st->width-1, st->inclear);
    if (st->inclear < st->height)
      memset(&st->fill[st->inclear*st->width], 0, sizeof(int)*st->width);
    XDrawLine(st->dpy, st->window, st->erase_gc, 0, st->height-st->inclear-1, st->width-1,
              st->height-st->inclear-1);
    if (st->height - st->inclear >= 1)
      memset(&st->fill[(st->height-st->inclear-1)*st->width], 0, sizeof(int)*st->width);
    st->inclear++;
    if(st->inclear>st->height/2) st->inclear=st->height;
  }
  else if(st->cov>(st->frac*st->width*st->height)) {
    st->inclear=0;
    st->cov=0;
  }
  for(i=0;i<st->count;i++) do_worm(st, &st->worms[i]);
  return st->delay;
}

static void
squiral_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->width  = w;
  st->height = h;
  squiral_init_1 (st);
  XClearWindow (dpy, window);
}

static Bool
squiral_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      squiral_init_1 (st);
      XClearWindow (dpy, window);
      return True;
    }
  return False;
}

static void
squiral_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *squiral_defaults[] = {
  ".background: black",
  ".foreground: white",
  "*fpsSolid:	true",
  "*fill:       75",
  "*count:      0",
  "*ncolors:    100",
  "*delay:      10000",
  "*disorder:   0.005",
  "*cycle:      False",
  "*handedness: 0.5",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec squiral_options[] = {
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

XSCREENSAVER_MODULE ("Squiral", squiral)
