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


typedef struct {
  double x,y,z;
  int sx,sy,sz;
} vPend;

typedef struct {
  long col;
  int numpt;
  int growth;
  unsigned short rate;
} appDef;

struct state {
  Display *dpy;
  Pixmap b, ba, bb;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  int arms;                       /* number of arms */
  int finpoints;                  /* final number of points in each array. */
  long delay;              /* usecs to wait between updates. */

  int scrWidth, scrHeight;
  GC gcDraw, gcClear;

  Bool dbuf;
  int width;

  vPend *vPendage;  /* 3D representation of appendages */
  appDef *appD;  /* defaults */
  vPend *vCurr, *vNext;
  appDef *aCurr;

  double turn, turndelta;

  int mx, my;            /* max screen coordinates. */
  int withdraw;

  XGCValues gcv;
  Colormap cmap;
  XColor *colors;
  int ncolors;
};



/*-----------------------------------------------------------------------+
  |  PUBLIC DATA                                                           |
  +-----------------------------------------------------------------------*/



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
initAppendages(struct state *st)
{
  int    i;
  /*int    marginx, marginy; */
    
  /*double scalex, scaley;*/

  double x,y,z,dist;

  st->mx = st->scrWidth - 1;
  st->my = st->scrHeight - 1;

  /* each appendage will have: colour,
     number of points, and a grow or shrink indicator */

  /* added: growth rate 1-10 (smaller==faster growth) */
  /* each appendage needs virtual coords (x,y,z) with y and z combining to
     give the screen y */

  st->vPendage = (vPend *) xmalloc((st->finpoints + 1) * sizeof(vPend) * st->arms);
  st->appD = (appDef *) xmalloc(sizeof(appDef) * st->arms);


  for (i = 0; i < st->arms; i++) {
    st->aCurr = st->appD + i;
    st->vCurr = st->vPendage + (st->finpoints + 1) * i;
    st->vNext = st->vCurr + 1;

    st->aCurr->col = st->colors[random() % st->ncolors].pixel;
    st->aCurr->numpt = 1;
    st->aCurr->growth = st->finpoints / 2 + RND(st->finpoints / 2);
    st->aCurr->rate = RND(11) * RND(11);

    do {
      x = (1 - RND(1001) / 500);
      y = (1 - RND(1001) / 500);
      z = (1 - RND(1001) / 500);
      dist = x * x + y * y + z * z;
    } while (dist >= 1.);

    st->vCurr->x = x * 200;
    st->vCurr->y = st->my / 2 + y * 200;
    st->vCurr->z = 0 + z * 200;

    /* start the arm going outwards */
    st->vCurr->sx = st->vCurr->x / 5;
    st->vCurr->sy = (st->vCurr->y - st->my / 2) / 5;
    st->vCurr->sz = (st->vCurr->z) / 5;

    
    st->vNext->x = st->vCurr->x + st->vCurr->sx;
    st->vNext->y = st->vCurr->y + st->vCurr->sy;
    st->vNext->z = st->vCurr->z + st->vCurr->sz;
  }
}

static void *
anemone_init (Display *disp, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XWindowAttributes wa;

  st->dpy = disp;
  st->turn = 0.;
  
  st->width = get_integer_resource(st->dpy, "width", "Integer");
  st->arms = get_integer_resource(st->dpy, "arms", "Integer");
  st->finpoints = get_integer_resource(st->dpy, "finpoints", "Integer");
  st->delay = get_integer_resource(st->dpy, "delay", "Integer");
  st->withdraw = get_integer_resource(st->dpy, "withdraw", "Integer");
  st->turndelta = get_float_resource(st->dpy, "turnspeed", "float") / 100000;

  st->dbuf = TRUE;

# ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
  st->dbuf = False;
# endif

  st->b = st->ba = st->bb = 0;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  st->b = st->backb = xdbe_get_backbuffer (st->dpy, window, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */


  XGetWindowAttributes(st->dpy, window, &wa);
  st->scrWidth = wa.width;
  st->scrHeight = wa.height;
  st->cmap = wa.colormap;

  st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  st->ncolors += 3;
  st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));
  make_smooth_colormap (st->dpy, wa.visual, st->cmap, st->colors, &st->ncolors,
                        True, 0, True);

  st->gcDraw = XCreateGC(st->dpy, window, 0, &st->gcv);
  st->gcv.foreground = get_pixel_resource(st->dpy, st->cmap,
                                          "background", "Background");
  st->gcClear = XCreateGC(st->dpy, window, GCForeground, &st->gcv);

  if (st->dbuf) {
    if (!st->b)
      {
	st->ba = XCreatePixmap (st->dpy, window, st->scrWidth, st->scrHeight, wa.depth);
	st->bb = XCreatePixmap (st->dpy, window, st->scrWidth, st->scrHeight, wa.depth);
	st->b = st->ba;
      }
  }
  else
    {	
      st->b = window;
    }

  if (st->ba) XFillRectangle (st->dpy, st->ba, st->gcClear, 0, 0, st->scrWidth, st->scrHeight);
  if (st->bb) XFillRectangle (st->dpy, st->bb, st->gcClear, 0, 0, st->scrWidth, st->scrHeight);

  XClearWindow(st->dpy, window);
  XSetLineAttributes(st->dpy, st->gcDraw,  st->width, LineSolid, CapRound, JoinBevel);

  initAppendages(st);

  return st;
}


static void
createPoints(struct state *st)
{
  int i;
  int withdrawall = RND(st->withdraw);

  for (i = 0; i< st->arms; i++) {
    st->aCurr = st->appD + i;
    if (!withdrawall) {
      st->aCurr->growth = -st->finpoints;
      st->turndelta = -st->turndelta;
    }

    else if (withdrawall<11) st->aCurr->growth = -st->aCurr->numpt;

    else if (RND(100)<st->aCurr->rate) {
      if (st->aCurr->growth>0) {
	if (!(--st->aCurr->growth)) st->aCurr->growth = -RND(st->finpoints) - 1;
	st->vCurr = st->vPendage + (st->finpoints + 1) * i + st->aCurr->numpt - 1;
	if (st->aCurr->numpt<st->finpoints - 1) {
	  /* add a piece */	
	  st->vNext = st->vCurr + 1;
	  st->aCurr->numpt++;
	  st->vNext->sx = st->vCurr->sx + RND(3) - 1;
	  st->vNext->sy = st->vCurr->sy + RND(3) - 1;
	  st->vNext->sz = st->vCurr->sz + RND(3) - 1;
	  st->vCurr = st->vNext + 1;
	  st->vCurr->x = st->vNext->x + st->vNext->sx;
	  st->vCurr->y = st->vNext->y + st->vNext->sy;
	  st->vCurr->z = st->vNext->z + st->vNext->sz;
	}
      }
    }
  }
}


static void
drawImage(struct state *st, Drawable curr_window, double sint, double cost)
{
  int q,numpt,mx2 = st->mx / 2;
  double cx,cy,cz,nx = 0,ny = 0,nz = 0;

  if ((numpt = st->aCurr->numpt)==1) return;
  XSetForeground(st->dpy, st->gcDraw, st->aCurr->col);
    
  st->vNext = st->vCurr + 1;

  cx = st->vCurr->x;
  cy = st->vCurr->y;
  cz = st->vCurr->z;


  for (q = 0; q < numpt - 1; q++) {
    nx = st->vNext->x + 2 - RND(5);
    ny = st->vNext->y + 2 - RND(5);
    nz = st->vNext->z + 2 - RND(5);

    XDrawLine(st->dpy, curr_window, st->gcDraw,
              mx2 + cx * cost - cz * sint, cy,
              mx2 + nx * cost - nz * sint, ny);
    st->vCurr++;
    st->vNext++;

    cx = nx;
    cy = ny;
    cz = nz;
  }
  XSetLineAttributes(st->dpy, st->gcDraw, st->width * 3,
                     LineSolid, CapRound, JoinBevel);
  XDrawLine(st->dpy, curr_window, st->gcDraw,
            st->mx / 2 + cx * cost - cz * sint, cy,
            st->mx / 2 + nx * cost - nz * sint, ny);
  XSetLineAttributes(st->dpy, st->gcDraw, st->width,
                     LineSolid, CapRound, JoinBevel);

}

static void
animateAnemone(struct state *st, Drawable curr_window)
{
  int i;
  double sint = sin(st->turn),cost = cos(st->turn);

  st->aCurr = st->appD;
  for (i = 0; i< st->arms; i++) {
    st->vCurr = st->vPendage + (st->finpoints + 1) * i;
    if (RND(25)<st->aCurr->rate) {
      if (st->aCurr->growth<0) {
	st->aCurr->numpt -= st->aCurr->numpt>1;
	if (!(++st->aCurr->growth)) st->aCurr->growth = RND(st->finpoints - st->aCurr->numpt) + 1;
      }
    }
    drawImage(st, curr_window, sint, cost);
    st->turn += st->turndelta;
    st->aCurr++;
  }
  createPoints(st);

  if (st->turn >= TWO_PI) st->turn -= TWO_PI;
}

static unsigned long
anemone_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

    XFillRectangle (st->dpy, st->b, st->gcClear, 0, 0, st->scrWidth, st->scrHeight);

    animateAnemone(st, st->b);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    if (st->backb)
      {
	XdbeSwapInfo info[1];
	info[0].swap_window = window;
	info[0].swap_action = XdbeUndefined;
	XdbeSwapBuffers (st->dpy, info, 1);
      }
    else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
      if (st->dbuf)
	{
	  XCopyArea (st->dpy, st->b, window, st->gcClear, 0, 0,
		     st->scrWidth, st->scrHeight, 0, 0);
	  st->b = (st->b == st->ba ? st->bb : st->ba);
	}

    return st->delay;
}


static void
anemone_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->scrWidth = w;
  st->scrHeight = h;
#if 0
  if (st->dbuf) {
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, window, &wa);
    if (st->ba) XFreePixmap (dpy, st->ba);
    if (st->bb) XFreePixmap (dpy, st->bb);
    st->ba = XCreatePixmap (dpy, window, st->scrWidth, st->scrHeight, wa.depth);
    st->bb = XCreatePixmap (dpy, window, st->scrWidth, st->scrHeight, wa.depth);
    st->b = st->ba;
  }
#endif
}

static Bool
anemone_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
anemone_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->vPendage) free (st->vPendage);
  if (st->appD) free (st->appD);
  free (st);
}



static const char *anemone_defaults [] = {
  ".background: black",
  "*arms: 128",
  "*width: 2",
  "*finpoints: 64",
  "*delay: 40000",
  "*withdraw: 1200",
  "*turnspeed: 50",
  "*colors: 20",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};


static XrmOptionDescRec anemone_options [] = {
  { "-arms",        ".arms",        XrmoptionSepArg, 0 },
  { "-finpoints",   ".finpoints",   XrmoptionSepArg, 0 },
  { "-delay",       ".delay",       XrmoptionSepArg, 0 },
  { "-width",       ".width",       XrmoptionSepArg, 0 },
  { "-withdraw",    ".withdraw",    XrmoptionSepArg, 0 },
  { "-turnspeed",   ".turnspeed",   XrmoptionSepArg, 0 },
  { "-colors",      ".colors",      XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Anemone", anemone)
