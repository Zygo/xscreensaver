/* anemotaxis, Copyright (c) 2004 Eugene Balkovski 
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.  No representations are made
 * about the suitability of this software for any purpose. It is
 * provided "as is" without express or implied warranty.
 */

/*------------------------------------------------------------------------
  |
  |  FILE            anemotaxis.c
  |
  |  DESCRIPTION     Anemotaxis
  |
  |                  This code illustrates an optimal algorithm designed
  |                  for searching a source of particles on a plane.
  |                  The particles drift in one direction and walk randomly 
  |                  in the other. The only information available to the
  |                  searcher is the presence of a particle at its location
  |                  and the local direction from where particle arrived.
  |                  The algorithm "explains" the behavior
  |                  of some animals and insects 
  |                  who use olfactory and directional cues to find
  |                  sources of odor (mates, food, home etc) in
  |                  turbulent atmosphere (odor-modulated anemotaxis),
  |                  e.g. male moths locating females who release 
  |                  pheromones to attract males. The search trajectories
  |                  resemble the trajectories that the animals follow.
  |
  |
  |  WRITTEN BY      Eugene Balkovski
  |
  |  MODIFICATIONS   june 2004 started
  |           
  +----------------------------------------------------------------------*/

/* 
   Options:
   
   -distance <arg> size of the lattice
   -sources <arg>  number of sources
   -searhers <arg> number of searcher        */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
#include "xdbe.h"
#endif


/*-----------------------------------------------------------------------+
  |  PRIVATE DATA                                                          |
  +-----------------------------------------------------------------------*/

#define MAX_DIST 250
#define MIN_DIST 10
#define LINE_WIDTH 4
#define MAX_INV_RATE 5

#define RND(x)     (random() % (x))
#define X(x) ((int) (ax * (x) + bx))
#define Y(x) ((int) (ay * (x) + by))

typedef struct {
  short x;
  short y;
} Point;

typedef struct {

  short y;    /* y-coordinate of the particle (relative to the source) */

  short v;    /* velocity of the particle. Allowed values are -1, 0, 1.
		 2 means the particle is not valid */
} YV;

typedef struct {

  int n;             /* size of array xv */

  YV *yv;            /* yv[i] keeps velocity and y-coordinate of the
			particle at (i + 1, yv[i].y) relative to the
			source */

  int inv_rate;      /* Inverse rate of particle emission (if 0 then
			source doesn't emit new particles (although
			old ones can still exist )*/

  Point r;           /* Position of the source */

  long color;

} Source;


typedef struct PList {
  Point r;
  struct PList *next;
} PList;

typedef enum {UP_LEFT, UP_RIGHT, LEFT, RIGHT, DONE} State_t;

typedef enum {FALSE = 0, TRUE = 1} bool;

typedef struct {

  Point r;              /* Current position */

  Point vertex;         /* Position of the vertex of the most recent
			   cone, which is the region where the source
			   is located. We do exhaustive search in the
			   cone until we encounter a new particle,
			   which gives us a new cone. */

  State_t state;        /* Internal state variable */

  unsigned char c;      /* Concentration at r */

  short v;              /* Velocity at r (good only when c != 0) */

  PList *hist;          /* Trajectory */

  int rs;               /* small shift of x-coordinate to avoid
			   painting over the same x */

  long color;

} Searcher;

static Source **source;
static Searcher **searcher;

static int max_dist, max_src, max_searcher;

static double ax, ay, bx, by;
static int dx, dy;

static Display *dpy;
static Window window;

static Pixmap b, ba, bb;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
static  XdbeBackBuffer backb;
#endif

static long delay;              /* usecs to wait between updates. */

static int scrWidth, scrHeight;
static GC gcDraw, gcClear;

static bool dbuf;

static XGCValues gcv;
static Colormap cmap;


/*-----------------------------------------------------------------------+
  |  PUBLIC DATA                                                           |
  +-----------------------------------------------------------------------*/

char *progclass = "Anemotaxis";

char *defaults [] = {
  ".background: black",
  "*distance: 40",
  "*sources: 25",
  "*searchers: 25",
  "*delay: 20000",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
#endif
  0
};


XrmOptionDescRec options [] = {
  { "-delay",       ".delay",       XrmoptionSepArg, 0 },
  { "-distance",    ".distance",    XrmoptionSepArg, 0 },
  { "-sources",     ".sources",     XrmoptionSepArg, 0 },
  { "-searchers",   ".searchers",   XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};



/*-----------------------------------------------------------------------+
  |  PRIVATE FUNCTIONS                                                     |
  +-----------------------------------------------------------------------*/

static void *emalloc(size_t size)
{
  void *ret = malloc(size);

  if (ret == NULL) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }
  return ret;
}

static Searcher *new_searcher(void)
{
  Searcher *m = (Searcher *) emalloc(sizeof(Searcher));

  m->r.x = m->vertex.x = max_dist;

  do {
    m->r.y = RND(2 * max_dist);
  } while(m->r.y < MIN_DIST || m->r.y > 2 * max_dist - MIN_DIST);

  m->vertex.y = m->r.y;

  m->state = (RND(2) == 0 ? UP_RIGHT : UP_LEFT);
  m->hist = NULL;
  m->color = random();

  m->rs = RND(dx);

  return m;
}

static void destroy_searcher(Searcher *m)
{
  PList *p = m->hist, *q;

  while(p != NULL) {
    q = p->next;
    free(p);
    p = q;
  }

  free(m);
}

static void write_hist(Searcher *m)
{
  PList *l;

  l = m->hist;
  m->hist = (PList *) emalloc(sizeof(PList));
  m->hist->next = l;
  m->hist->r = m->r;

}

static void move_searcher(Searcher *m)
{

  if(m->c == TRUE) {
    write_hist(m);
    m->r.x -= 1;
    m->r.y -= m->v;
    write_hist(m);
    m->state = (RND(2) == 0 ? UP_LEFT : UP_RIGHT);
    m->vertex = m->r;
    return;

  }

  switch(m->state) {
  case UP_LEFT:

    m->r.x -= 1;
    m->r.y += 1;
    m->state = RIGHT;
    write_hist(m);
    return;

  case RIGHT:

    m->r.y -= 1;
    if(m->vertex.x - m->r.x == m->vertex.y - m->r.y) {
      write_hist(m);
      m->state = UP_RIGHT;
    }
    return;

  case UP_RIGHT:

    m->r.x -= 1;
    m->r.y -= 1;

    m->state = LEFT;
    write_hist(m);
    return;

  case LEFT:

    m->r.y += 1;

    if(m->vertex.x - m->r.x == m->r.y - m->vertex.y) {
      write_hist(m);
      m->state = UP_LEFT;
    }
    return;

  default:
    break;
  }

}

static void evolve_source(Source *s)
{

  int i;

  /* propagate existing particles */

  for(i = s->n - 1; i > 0; i--) {

    if(s->yv[i - 1].v == 2)
      s->yv[i].v = 2;
    else {
      s->yv[i].v = RND(3) - 1;
      s->yv[i].y = s->yv[i - 1].y + s->yv[i].v;
    }

  }


  if(s->inv_rate > 0 && (RND(s->inv_rate) == 0)) /* emit a particle */
    s->yv[0].y = s->yv[0].v = RND(3) - 1;
  else
    s->yv[0].v = 2;

}

static Source *new_source(void)
{
  int i;

  Source *s = (Source *) emalloc(sizeof(Source));
  if(max_searcher == 0) {
    s->r.x = 0;
    s->r.y = RND(2 * max_dist);
  }
  else {
    s->r.x = RND(max_dist / 3);
    do {
      s->r.y = RND(2 * max_dist);
    } while(s->r.y < MIN_DIST || s->r.y > 2 * max_dist - MIN_DIST);
  }

  s->n = max_dist - s->r.x;
  s->yv = emalloc(sizeof(YV) * s->n);

  for(i = 0; i < s->n; i++)
    s->yv[i].v = 2;

  s->inv_rate = RND(MAX_INV_RATE);

  if(s->inv_rate == 0)
    s->inv_rate = 1;

  s->color = random();

  return s;
}

static void destroy_source(Source *s)
{
  free(s->yv);
  free(s);
}

static void get_v(const Source *s, Searcher *m)
{
  int x = m->r.x - s->r.x - 1;

  m->c = 0;

  if(x < 0 || x >= s->n)
    return;

  if((s->yv[x].v == 2) || (s->yv[x].y != m->r.y - s->r.y))
    return;

  m->c = 1;
  m->v = s->yv[x].v;
  m->color = s->color;
}


static void init_anemotaxis(void)
{
  XWindowAttributes wa;

  delay = get_integer_resource("delay", "Integer");
  max_dist = get_integer_resource("distance", "Integer");

  if(max_dist < MIN_DIST)
    max_dist = MIN_DIST + 1;
  if(max_dist > MAX_DIST)
    max_dist = MAX_DIST;

  max_src = get_integer_resource("sources", "Integer");

  if(max_src <= 0) 
    max_src = 1;

  max_searcher = get_integer_resource("searchers", "Integer");

  if(max_searcher < 0)
    max_searcher = 0;

  dbuf = TRUE;

  source = (Source **) emalloc(sizeof(Source *) * max_src);
  memset(source, 0, max_src * sizeof(Source *));

  source[0] = new_source();

  searcher = (Searcher **) emalloc(max_searcher * sizeof(Searcher *));

  memset(searcher, 0, max_searcher * sizeof(Searcher *));

  b = ba = bb = 0;	/* double-buffer to reduce flicker */

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  b = backb = xdbe_get_backbuffer (dpy, window, XdbeUndefined);
#endif

  XGetWindowAttributes(dpy, window, &wa);
  scrWidth = wa.width;
  scrHeight = wa.height;
  cmap = wa.colormap;
  gcDraw = XCreateGC(dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource("background", "Background", dpy, cmap);
  gcClear = XCreateGC(dpy, window, GCForeground, &gcv);

  if (dbuf) {
    if (!b) {
	ba = XCreatePixmap (dpy, window, scrWidth, scrHeight, wa.depth);
	bb = XCreatePixmap (dpy, window, scrWidth, scrHeight, wa.depth);
	b = ba;
      }
  }
  else
    b = window;
  

  if (ba) XFillRectangle (dpy, ba, gcClear, 0, 0, scrWidth, scrHeight);
  if (bb) XFillRectangle (dpy, bb, gcClear, 0, 0, scrWidth, scrHeight);

  ax = scrWidth / (double) max_dist;
  ay = scrHeight / (2. * max_dist);
  bx = 0.;
  by = 0.;

  if((dx = scrWidth / (2 * max_dist)) == 0)
    dx = 1;
  if((dy = scrHeight / (4 * max_dist)) == 0)
    dy = 1;
  XSetLineAttributes(dpy, gcDraw, dx / 3 + 1, LineSolid, CapRound, JoinRound);
  XClearWindow(dpy, window);
}

static void draw_searcher(Drawable curr_window, int i)
{
  Point r1, r2;
  PList *l;

  if(searcher[i] == NULL)
    return;

  XSetForeground(dpy, gcDraw, searcher[i]->color);

  r1.x = X(searcher[i]->r.x) + searcher[i]->rs;
  r1.y = Y(searcher[i]->r.y);

  XFillRectangle(dpy, curr_window, gcDraw, r1.x - 2, r1.y - 2, 4, 4);

  for(l = searcher[i]->hist; l != NULL; l = l->next) {

    r2.x = X(l->r.x) + searcher[i]->rs;
    r2.y = Y(l->r.y);

    XDrawLine(dpy, curr_window, gcDraw, r1.x, r1.y, r2.x, r2.y);

    r1 = r2;
  }

}

static void draw_image(Drawable curr_window)
{
  int i, j;
  int x, y;

  for(i = 0; i < max_src; i++) {

    if(source[i] == NULL)
      continue;

    XSetForeground(dpy, gcDraw, source[i]->color);

    if(source[i]->inv_rate > 0) {

      if(max_searcher > 0) {
      x = (int) X(source[i]->r.x);
      y = (int) Y(source[i]->r.y);
      j = dx * (MAX_INV_RATE + 1 - source[i]->inv_rate) / (2 * MAX_INV_RATE);
      if(j == 0)
	j = 1;
      XFillArc(dpy, curr_window, gcDraw, x - j, y - j, 2 * j, 2 * j, 0, 360 * 64);
      }}

    for(j = 0; j < source[i]->n; j++) {

      if(source[i]->yv[j].v == 2)
	continue;

      /* Move the particles slightly off lattice */
      x =  X(source[i]->r.x + 1 + j) + RND(dx);
      y = Y(source[i]->r.y + source[i]->yv[j].y) + RND(dy);
      XFillArc(dpy, curr_window, gcDraw, x - 2, y - 2, 4, 4, 0, 360 * 64);
    }

  }

  for(i = 0; i < max_searcher; i++)
    draw_searcher(curr_window, i);
 
}

static void animate_anemotaxis(Drawable curr_window)
{
  int i, j;
  bool dead;

  for(i = 0; i < max_src; i++) {

    if(source[i] == NULL)
      continue;

    evolve_source(source[i]);

    /* reap dead sources for which all particles are gone */
    if(source[i]->inv_rate == 0) {

      dead = TRUE;

      for(j = 0; j < source[i]->n; j++) {
	if(source[i]->yv[j].v != 2) {
	  dead = FALSE;
	  break;
	}
      }

      if(dead == TRUE) {
	destroy_source(source[i]);
	source[i] = NULL;
      }
    }
  }

  /* Decide if we want to add new  sources */

  for(i = 0; i < max_src; i++) {
    if(source[i] == NULL && RND(max_dist * max_src) == 0)
      source[i] = new_source();
  }

  if(max_searcher == 0) { /* kill some sources when searchers don't do that */
    for(i = 0; i < max_src; i++) {
      if(source[i] != NULL && RND(max_dist * max_src) == 0) {
	destroy_source(source[i]);
	source[i] = 0;
      }
    }
  }

  /* Now deal with searchers */

  for(i = 0; i < max_searcher; i++) {

    if((searcher[i] != NULL) && (searcher[i]->state == DONE)) {
      destroy_searcher(searcher[i]);
      searcher[i] = NULL;
    }

    if(searcher[i] == NULL) {

      if(RND(max_dist * max_searcher) == 0) {

	searcher[i] = new_searcher();

      }
    }

    if(searcher[i] == NULL)
      continue;

    /* Check if searcher found a source or missed all of them */
    for(j = 0; j < max_src; j++) {

      if(source[j] == NULL || source[j]->inv_rate == 0)
	continue;

      if(searcher[i]->r.x < 0) {
	searcher[i]->state = DONE;
	break;
      }

      if((source[j]->r.y == searcher[i]->r.y) && 
	 (source[j]->r.x == searcher[i]->r.x)) {
	searcher[i]->state = DONE;
	source[j]->inv_rate = 0;  /* source disappears */

	/* Make it flash */
	searcher[i]->color = WhitePixel(dpy, DefaultScreen(dpy));

	break;
      }
    }

    searcher[i]->c = 0; /* set it here in case we don't get to get_v() */

    /* Find the concentration at searcher's location */
    
    if(searcher[i]->state != DONE) {
      for(j = 0; j < max_src; j++) {
	
	if(source[j] == NULL)
	  continue;

	get_v(source[j], searcher[i]);
      
	if(searcher[i]->c == 1)
	  break;
      }
    }

    move_searcher(searcher[i]);

  }

  draw_image(curr_window);
  usleep(delay);
}

/*-----------------------------------------------------------------------+
  |  PUBLIC FUNCTIONS                                                      |
  +-----------------------------------------------------------------------*/

void screenhack(Display *disp, Window win)
{

  XWindowAttributes wa;
  int w, h;


  dpy = disp;
  window = win;

  init_anemotaxis();

  for(;;) {
    
    XGetWindowAttributes(dpy, window, &wa);

    w = wa.width;
    h = wa.height;

    if(w != scrWidth || h != scrHeight) {
      scrWidth = w;
      scrHeight = h;
      ax = scrWidth / (double) max_dist;
      ay = scrHeight / (2. * max_dist);
      bx = 0.;
      by = 0.;
      
      if((dx = scrWidth / (2 * max_dist)) == 0)
	dx = 1;
      if((dy = scrHeight / (4 * max_dist)) == 0)
	dy = 1;
      XSetLineAttributes(dpy, gcDraw, dx / 3 + 1, LineSolid, CapRound, JoinRound);
    } 

    XFillRectangle (dpy, b, gcClear, 0, 0, scrWidth, scrHeight);
    animate_anemotaxis(b);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    if (backb) {
	XdbeSwapInfo info[1];
	info[0].swap_window = window;
	info[0].swap_action = XdbeUndefined;
	XdbeSwapBuffers (dpy, info, 1);
      }
    else
#endif
      if (dbuf)	{
	  XCopyArea (dpy, b, window, gcClear, 0, 0,
		     scrWidth, scrHeight, 0, 0);
	  b = (b == ba ? bb : ba);
	}

    screenhack_handle_events (dpy);
  }

}
