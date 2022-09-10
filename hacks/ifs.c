/* Copyright © Chris Le Sueur and Robby Griffin, 2005-2006

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

Ultimate thanks go to Massimino Pascal, who created the original
xscreensaver hack, and inspired me with it's swirly goodness. This
version adds things like variable quality, number of functions and also
a groovier colouring mode.

This version by Chris Le Sueur <thefishface@gmail.com>, Feb 2005
Many improvements by Robby Griffin <rmg@terc.edu>, Mar 2006
Multi-coloured mode added by Jack Grahl <j.grahl@ucl.ac.uk>, Jan 2007
*/

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "screenhack.h"

typedef struct {
  float r, s, tx, ty;   /* Rotation, Scale, Translation X & Y */
  float ro, rt, rc;     /* Old Rotation, Rotation Target, Rotation Counter */
  float so, st, sc;     /* Old Scale, Scale Target, Scale Counter */
  float sa, txa, tya;   /* Scale change, Translation change */

  int ua, ub, utx;      /* Precomputed combined r,s,t values */
  int uc, ud, uty;      /* Precomputed combined r,s,t values */

} Lens;	

struct state {
  Display *dpy;
  Window window;
  GC gc;
  Drawable backbuffer;
  XColor *colours;
  int ncolours;
  int ccolour;
  int blackColor, whiteColor;

  int width, widthb, height;
  int width8, height8;
  unsigned int *board;
  XPoint pointbuf[1000];
  int npoints;
  int xmin, xmax, ymin, ymax;
  int x, y;

  int delay;

  int lensnum;
  Lens *lenses;
  int length;
  int mode;
  Bool recurse;
  Bool multi;
  Bool translate, scale, rotate;
};

#define getdot(x,y) (st->board[((y)*st->widthb)+((x)>>5)] &  (1<<((x) & 31)))
#define setdot(x,y) (st->board[((y)*st->widthb)+((x)>>5)] |= (1<<((x) & 31)))

static float
myrandom(float up)
{
  return (((float)random() / RAND_MAX) * up);
}

static const char *ifs_defaults [] = {
  ".lowrez:             true",
  ".background:		Black",
  "*lensnum:		3",
  "*fpsSolid:		true",
  "*length:		9",
  "*mode:		0",
  "*colors:		200",
  "*delay:		20000",
  "*translate:		True",
  "*ifsScale:		True",
  "*rotate:		True",
  "*recurse:		False",
  "*multi:              True",
# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  "*doubleBuffer:	False",
#else
  "*doubleBuffer:	True",
#endif
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec ifs_options [] = {
  { "-detail",		".length",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-functions",	".lensnum", 	XrmoptionSepArg, 0 },
  { "-no-translate",	".translate",   XrmoptionNoArg, "False" },
  { "-no-scale",	".ifsScale",	XrmoptionNoArg, "False" },
  { "-no-rotate",	".rotate",	XrmoptionNoArg, "False" },
  { "-recurse",		".recurse",	XrmoptionNoArg, "True" },
  { "-iterate",		".recurse",	XrmoptionNoArg, "False" },
  { "-multi",           ".multi",       XrmoptionNoArg, "True" },
  { "-no-multi",        ".multi",       XrmoptionNoArg, "False" },
  { "-db",		".doubleBuffer",XrmoptionNoArg, "True" },
  { "-no-db",		".doubleBuffer",XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};


/* Draw all the queued points on the backbuffer */
static void
drawpoints(struct state *st)
{
  XDrawPoints(st->dpy, st->backbuffer, st->gc, st->pointbuf, st->npoints,
	      CoordModeOrigin);
  st->npoints = 0;
}

/* Set a point to be drawn, if it hasn't been already.
 * Expects coordinates in 256ths of a pixel. */
static void
sp(struct state *st, int x, int y)
{
  if (x < 0 || x >= st->width8 || y < 0 || y >= st->height8)
    return;

  x >>= 8;
  y >>= 8;

  if (getdot(x, y)) return;
  setdot(x, y);

  if (x < st->xmin) st->xmin = x;
  if (x > st->xmax) st->xmax = x;
  if (y < st->ymin) st->ymin = y;
  if (y > st->ymax) st->ymax = y;

  st->pointbuf[st->npoints].x = x;
  st->pointbuf[st->npoints].y = y;
  st->npoints++;

  if (st->npoints >= countof(st->pointbuf)) {
    drawpoints(st);
  }
}


/* Precompute integer values for matrix multiplication and vector
 * addition. The matrix multiplication will go like this (see iterate()):
 *   |x2|     |ua ub|   |x|     |utx|
 *   |  |  =  |     | * | |  +  |   |
 *   |y2|     |uc ud|   |y|     |uty|
 * 
 * There is an extra factor of 2^10 in these values, and an extra factor of
 * 2^8 in the coordinates, in order to implement fixed-point arithmetic.
 */
static void
lensmatrix(struct state *st, Lens *l)
{
  l->ua = 1024.0 * l->s * cos(l->r);
  l->ub = -1024.0 * l->s * sin(l->r);
  l->uc = -l->ub;
  l->ud = l->ua;
  l->utx = 131072.0 * st->width * (l->s * (sin(l->r) - cos(l->r))
				   + l->tx / 16 + 1);
  l->uty = -131072.0 * st->height * (l->s * (sin(l->r) + cos(l->r))
				     + l->ty / 16 - 1);
}

static void
CreateLens(struct state *st,
           float nr,
           float ns,
           float nx,
           float ny,
           Lens *newlens)
{
  newlens->sa = newlens->txa = newlens->tya = 0;
  if (st->rotate) {
    newlens->r = newlens->ro = newlens->rt = nr;
    newlens->rc = 1;
  }
  else newlens->r = 0;

  if (st->scale) {
    newlens->s = newlens->so = newlens->st = ns;
    newlens->sc = 1;
  }
  else newlens->s = 0.5;

  newlens->tx = nx;
  newlens->ty = ny;

  lensmatrix(st, newlens);
}
	
static void
mutate(struct state *st, Lens *l)
{
  if (st->rotate) {
    float factor;
    if(l->rc >= 1) {
      l->rc = 0;
      l->ro = l->rt;
      l->rt = myrandom(4) - 2;
    }
    factor = (sin((-M_PI / 2.0) + M_PI * l->rc) + 1.0) / 2.0;
    l->r = l->ro + (l->rt - l->ro) * factor;
    l->rc += 0.01;
  }
  if (st->scale) {
    float factor;
    if (l->sc >= 1) {
      /* Reset counter, obtain new target value */
      l->sc = 0;
      l->so = l->st;
      l->st = myrandom(2) - 1;
    }
    factor = (sin((-M_PI / 2.0) + M_PI * l->sc) + 1.0) / 2.0;
    /* Take average of old target and new target, using factor to *
     * weight. It's computed sinusoidally, resulting in smooth,   *
     * rhythmic transitions.                                      */
    l->s = l->so + (l->st - l->so) * factor;
    l->sc += 0.01;
  }
  if (st->translate) {
    l->txa += myrandom(0.004) - 0.002;
    l->tya += myrandom(0.004) - 0.002;
    l->tx += l->txa;
    l->ty += l->tya;
    if (l->tx > 6) l->txa -= 0.004;
    if (l->ty > 6) l->tya -= 0.004;
    if (l->tx < -6) l->txa += 0.004;
    if (l->ty < -6) l->tya += 0.004;
    if (l->txa > 0.05 || l->txa < -0.05) l->txa /= 1.7;
    if (l->tya > 0.05 || l->tya < -0.05) l->tya /= 1.7;
  }
  if (st->rotate || st->scale || st->translate) {
    lensmatrix(st, l);
  }
}


#define STEPX(l,x,y) (((l)->ua * (x) + (l)->ub * (y) + (l)->utx) >> 10)
#define STEPY(l,x,y) (((l)->uc * (x) + (l)->ud * (y) + (l)->uty) >> 10)
/*#define STEPY(l,x,y) (((l)->ua * (y) - (l)->ub * (x) + (l)->uty) >> 10)*/

/* Calls itself <lensnum> times - with results from each lens/function.  *
 * After <length> calls to itself, it stops iterating and draws a point. */
static void
recurse(struct state *st, int x, int y, int length, int p)
{
  int i;
  Lens *l;

  if (length == 0) {
    if (p == 0) 
      sp(st, x, y);
    else {
      l = &st->lenses[p];
      sp(st, STEPX(l, x, y), STEPY(l, x, y));
    }
  }
  else {
    for (i = 0; i < st->lensnum; i++) {
      l = &st->lenses[i];
      recurse(st, STEPX(l, x, y), STEPY(l, x, y), length - 1, p);
    }
  }
}

/* Performs <count> random lens transformations, drawing a point at each
 * iteration after the first 10.
 */
static void
iterate(struct state *st, int count, int p)
{
  int i;
  Lens *l;
  int x = st->x;
  int y = st->y;
  int tx;

# define STEP()                              \
    l = &st->lenses[random() % st->lensnum]; \
    tx = STEPX(l, x, y);                     \
    y = STEPY(l, x, y);                      \
    x = tx

  for (i = 0; i < 10; i++) {
    STEP();
  }

  for ( ; i < count; i++) {
    STEP();
    if (p == 0)
      sp(st, x, y);
    else
      {
	l = &st->lenses[p];
	sp(st, STEPX(l, x, y), STEPY(l, x, y));
      }
  }

# undef STEP

  st->x = x;
  st->y = y;
}

/* Come on and iterate, iterate, iterate and sing... *
 * Yeah, this function just calls iterate, mutate,   *
 * and then draws everything.                        */
static unsigned long
ifs_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  int xmin = st->xmin, xmax = st->xmax, ymin = st->ymin, ymax = st->ymax;
  int partcolor, x, y;
  

  /* erase whatever was drawn in the previous frame */
  if (xmin <= xmax && ymin <= ymax) {
    XSetForeground(st->dpy, st->gc, st->blackColor);
    XFillRectangle(st->dpy, st->backbuffer, st->gc,
		   xmin, ymin,
		   xmax - xmin + 1, ymax - ymin + 1);
    st->xmin = st->width + 1;
    st->xmax = st->ymax = -1;
    st->ymin = st->height + 1;
  }

  st->ccolour++;
  st->ccolour %= st->ncolours;

  /* calculate and draw points for this frame */
  x = st->width << 7;
  y = st->height << 7;
  
  if (st->multi) {
    for (i = 0; i < st->lensnum; i++) {  
      partcolor = st->ccolour * (i+1);
      partcolor %= st->ncolours;
      XSetForeground(st->dpy, st->gc, st->colours[partcolor].pixel);
      memset(st->board, 0, st->widthb * st->height * sizeof(*st->board));
      if (st->recurse)   
	recurse(st, x, y, st->length - 1, i);
      else
	iterate(st, pow(st->lensnum, st->length - 1), i);
      if (st->npoints) 
	drawpoints(st);
    }
  } 
  else {
    
    XSetForeground(st->dpy, st->gc, st->colours[st->ccolour].pixel);
    memset(st->board, 0, st->widthb * st->height * sizeof(*st->board));
    if (st->recurse)
      recurse(st, x, y, st->length, 0);
    else
      iterate(st, pow(st->lensnum, st->length), 0);
    if (st->npoints)
      drawpoints(st);
  }
  
  /* if we just drew into a buffer, copy the changed area (including
   * erased area) to screen */
  if (st->backbuffer != st->window
      && ((st->xmin <= st->xmax && st->ymin <= st->ymax)
	  || (xmin <= xmax && ymin <= ymax))) {
    if (st->xmin < xmin) xmin = st->xmin;
    if (st->xmax > xmax) xmax = st->xmax;
    if (st->ymin < ymin) ymin = st->ymin;
    if (st->ymax > ymax) ymax = st->ymax;
    XCopyArea(st->dpy, st->backbuffer, st->window, st->gc,
	      xmin, ymin,
	      xmax - xmin + 1, ymax - ymin + 1,
	      xmin, ymin);
  }

  for(i = 0; i < st->lensnum; i++) {
    mutate(st, &st->lenses[i]);
  }

  return st->delay;
}

static void
ifs_reshape (Display *, Window, void *, unsigned int, unsigned int);

static void *
ifs_init (Display *d_arg, Window w_arg)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  XWindowAttributes xgwa;
	
  /* Initialise all this X shizzle */
  st->dpy = d_arg;
  st->window = w_arg;

  st->blackColor = BlackPixel(st->dpy, DefaultScreen(st->dpy));
  st->whiteColor = WhitePixel(st->dpy, DefaultScreen(st->dpy));
  st->gc = XCreateGC(st->dpy, st->window, 0, NULL);

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  ifs_reshape(st->dpy, st->window, st, xgwa.width, xgwa.height);
	
  st->ncolours = get_integer_resource(st->dpy, "colors", "Colors");
  if (st->ncolours < st->lensnum)
    st->ncolours = st->lensnum;
  if (st->colours) free(st->colours);
  if (st->ncolours < 1) st->ncolours = 1;
  st->colours = (XColor *)calloc(st->ncolours, sizeof(XColor));
  if (!st->colours) exit(1);
  make_smooth_colormap (xgwa.screen, xgwa.visual, xgwa.colormap, 
                        st->colours, &st->ncolours,
                        True, 0, False);

  /* Initialize IFS data */
 
  st->delay = get_integer_resource(st->dpy, "delay", "Delay");
  st->length = get_integer_resource(st->dpy, "length", "Detail");
  if (st->length < 0) st->length = 0;
  st->mode = get_integer_resource(st->dpy, "mode", "Mode");

  st->rotate    = get_boolean_resource(st->dpy, "rotate", "Boolean");
                  /* Xft uses 'scale' */
  st->scale     = get_boolean_resource(st->dpy, "ifsScale", "Boolean");
  st->translate = get_boolean_resource(st->dpy, "translate", "Boolean");
  st->recurse = get_boolean_resource(st->dpy, "recurse", "Boolean");
  st->multi = get_boolean_resource(st->dpy, "multi", "Boolean");

  st->lensnum = get_integer_resource(st->dpy, "lensnum", "Functions");
  if (st->lenses) free (st->lenses);
  st->lenses = (Lens *)calloc(st->lensnum, sizeof(Lens));
  if (!st->lenses) exit(1);
  for (i = 0; i < st->lensnum; i++) {
    CreateLens(st,
	       myrandom(1)-0.5,
	       myrandom(1),
	       myrandom(4)-2,
	       myrandom(4)+2,
	       &st->lenses[i]);
  }

  return st;
}

static void
ifs_reshape (Display *dpy, Window window, void *closure, 
	     unsigned int w, unsigned int h)
{
  struct state *st = (struct state *)closure;
  XWindowAttributes xgwa;

  /* oh well, we need the screen depth anyway */
  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->width = xgwa.width;
  st->widthb = ((xgwa.width + 31) >> 5);
  st->height = xgwa.height;
  st->width8 = xgwa.width << 8;
  st->height8 = xgwa.height << 8;

  if (!st->xmax && !st->ymax && !st->xmin && !st->ymin) {
    st->xmin = xgwa.width + 1;
    st->xmax = st->ymax = -1;
    st->ymin = xgwa.height + 1;
  }

  if (st->backbuffer != None && st->backbuffer != st->window) {
    XFreePixmap(st->dpy, st->backbuffer);
    st->backbuffer = None;
  }

  if (get_boolean_resource (st->dpy, "doubleBuffer", "Boolean")) {
    st->backbuffer = XCreatePixmap(st->dpy, st->window, st->width, st->height, xgwa.depth);
    XSetForeground(st->dpy, st->gc, st->blackColor);
    XFillRectangle(st->dpy, st->backbuffer, st->gc,
		   0, 0, st->width, st->height);
  } else {
    st->backbuffer = st->window;
    XClearWindow(st->dpy, st->window);
  }

  if (st->board) free(st->board);
  st->board = (unsigned int *)calloc(st->widthb * st->height, sizeof(unsigned int));
  if (!st->board) exit(1);
}

static Bool
ifs_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *)closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      int i;
      for (i = 0; i < st->lensnum; i++) {
        CreateLens(st,
                   myrandom(1)-0.5,
                   myrandom(1),
                   myrandom(4)-2,
                   myrandom(4)+2,
                   &st->lenses[i]);
      }
      return True;
    }
  return False;
}

static void
ifs_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->board) free(st->board);
  if (st->lenses) free(st->lenses);
  if (st->colours) free(st->colours);
  if (st->backbuffer != None && st->backbuffer != st->window)
    XFreePixmap(st->dpy, st->backbuffer);
  XFreeGC (dpy, st->gc);
  free(st);
}

XSCREENSAVER_MODULE ("IFS", ifs)
