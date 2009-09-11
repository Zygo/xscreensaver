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
#define X(x) ((int) (st->ax * (x) + st->bx))
#define Y(x) ((int) (st->ay * (x) + st->by))

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

struct state {
  Source **source;
  Searcher **searcher;

  int max_dist, max_src, max_searcher;

  double ax, ay, bx, by;
  int dx, dy;

  Display *dpy;
  Window window;

  Pixmap b, ba, bb;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb;
#endif

  long delay;              /* usecs to wait between updates. */

  int scrWidth, scrHeight;
  GC gcDraw, gcClear;

  Bool dbuf;

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

static void *emalloc(size_t size)
{
  void *ret = malloc(size);

  if (ret == NULL) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }
  return ret;
}

static Searcher *new_searcher(struct state *st)
{
  Searcher *m = (Searcher *) emalloc(sizeof(Searcher));

  m->r.x = m->vertex.x = st->max_dist;

  do {
    m->r.y = RND(2 * st->max_dist);
  } while(m->r.y < MIN_DIST || m->r.y > 2 * st->max_dist - MIN_DIST);

  m->vertex.y = m->r.y;

  m->state = (RND(2) == 0 ? UP_RIGHT : UP_LEFT);
  m->hist = NULL;
  m->color = st->colors[random() % st->ncolors].pixel;

  m->rs = RND(st->dx);

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

  if(m->c == True) {
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

static Source *new_source(struct state *st)
{
  int i;

  Source *s = (Source *) emalloc(sizeof(Source));
  if(st->max_searcher == 0) {
    s->r.x = 0;
    s->r.y = RND(2 * st->max_dist);
  }
  else {
    s->r.x = RND(st->max_dist / 3);
    do {
      s->r.y = RND(2 * st->max_dist);
    } while(s->r.y < MIN_DIST || s->r.y > 2 * st->max_dist - MIN_DIST);
  }

  s->n = st->max_dist - s->r.x;
  s->yv = emalloc(sizeof(YV) * s->n);

  for(i = 0; i < s->n; i++)
    s->yv[i].v = 2;

  s->inv_rate = RND(MAX_INV_RATE);

  if(s->inv_rate == 0)
    s->inv_rate = 1;

  s->color = st->colors[random() % st->ncolors].pixel;

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


static void *
anemotaxis_init (Display *disp, Window win)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XWindowAttributes wa;

  st->dpy = disp;
  st->window = win;

  XGetWindowAttributes(st->dpy, st->window, &wa);

  st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  st->ncolors++;
  st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));
  make_random_colormap (st->dpy, wa.visual, wa.colormap, st->colors, &st->ncolors,
                        True, True, 0, True);

  st->delay = get_integer_resource(st->dpy, "delay", "Integer");
  st->max_dist = get_integer_resource(st->dpy, "distance", "Integer");

  if(st->max_dist < MIN_DIST)
    st->max_dist = MIN_DIST + 1;
  if(st->max_dist > MAX_DIST)
    st->max_dist = MAX_DIST;

  st->max_src = get_integer_resource(st->dpy, "sources", "Integer");

  if(st->max_src <= 0) 
    st->max_src = 1;

  st->max_searcher = get_integer_resource(st->dpy, "searchers", "Integer");

  if(st->max_searcher < 0)
    st->max_searcher = 0;

  st->dbuf = True;

# ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
    st->dbuf = False;
# endif

  st->source = (Source **) emalloc(sizeof(Source *) * st->max_src);
  memset(st->source, 0, st->max_src * sizeof(Source *));

  st->source[0] = new_source(st);

  st->searcher = (Searcher **) emalloc(st->max_searcher * sizeof(Searcher *));

  memset(st->searcher, 0, st->max_searcher * sizeof(Searcher *));

  st->b = st->ba = st->bb = 0;	/* double-buffer to reduce flicker */

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  st->b = st->backb = xdbe_get_backbuffer (st->dpy, st->window, XdbeUndefined);
#endif

  st->scrWidth = wa.width;
  st->scrHeight = wa.height;
  st->cmap = wa.colormap;
  st->gcDraw = XCreateGC(st->dpy, st->window, 0, &st->gcv);
  st->gcv.foreground = get_pixel_resource(st->dpy, st->cmap,
                                          "background", "Background");
  st->gcClear = XCreateGC(st->dpy, st->window, GCForeground, &st->gcv);

  if (st->dbuf) {
    if (!st->b) {
	st->ba = XCreatePixmap (st->dpy, st->window, st->scrWidth, st->scrHeight, wa.depth);
	st->bb = XCreatePixmap (st->dpy, st->window, st->scrWidth, st->scrHeight, wa.depth);
	st->b = st->ba;
      }
  }
  else
    st->b = st->window;
  

  if (st->ba) XFillRectangle (st->dpy, st->ba, st->gcClear, 0, 0, st->scrWidth, st->scrHeight);
  if (st->bb) XFillRectangle (st->dpy, st->bb, st->gcClear, 0, 0, st->scrWidth, st->scrHeight);

  st->ax = st->scrWidth / (double) st->max_dist;
  st->ay = st->scrHeight / (2. * st->max_dist);
  st->bx = 0.;
  st->by = 0.;

  if((st->dx = st->scrWidth / (2 * st->max_dist)) == 0)
    st->dx = 1;
  if((st->dy = st->scrHeight / (4 * st->max_dist)) == 0)
    st->dy = 1;
  XSetLineAttributes(st->dpy, st->gcDraw, st->dx / 3 + 1, LineSolid, CapRound, JoinRound);
  XClearWindow(st->dpy, st->window);

  return st;
}

static void draw_searcher(struct state *st, Drawable curr_window, int i)
{
  Point r1, r2;
  PList *l;

  if(st->searcher[i] == NULL)
    return;

  XSetForeground(st->dpy, st->gcDraw, st->searcher[i]->color);

  r1.x = X(st->searcher[i]->r.x) + st->searcher[i]->rs;
  r1.y = Y(st->searcher[i]->r.y);

  XFillRectangle(st->dpy, curr_window, st->gcDraw, r1.x - 2, r1.y - 2, 4, 4);

  for(l = st->searcher[i]->hist; l != NULL; l = l->next) {

    r2.x = X(l->r.x) + st->searcher[i]->rs;
    r2.y = Y(l->r.y);

    XDrawLine(st->dpy, curr_window, st->gcDraw, r1.x, r1.y, r2.x, r2.y);

    r1 = r2;
  }

}

static void draw_image(struct state *st, Drawable curr_window)
{
  int i, j;
  int x, y;

  for(i = 0; i < st->max_src; i++) {

    if(st->source[i] == NULL)
      continue;

    XSetForeground(st->dpy, st->gcDraw, st->source[i]->color);

    if(st->source[i]->inv_rate > 0) {

      if(st->max_searcher > 0) {
      x = (int) X(st->source[i]->r.x);
      y = (int) Y(st->source[i]->r.y);
      j = st->dx * (MAX_INV_RATE + 1 - st->source[i]->inv_rate) / (2 * MAX_INV_RATE);
      if(j == 0)
	j = 1;
      XFillArc(st->dpy, curr_window, st->gcDraw, x - j, y - j, 2 * j, 2 * j, 0, 360 * 64);
      }}

    for(j = 0; j < st->source[i]->n; j++) {

      if(st->source[i]->yv[j].v == 2)
	continue;

      /* Move the particles slightly off lattice */
      x =  X(st->source[i]->r.x + 1 + j) + RND(st->dx);
      y = Y(st->source[i]->r.y + st->source[i]->yv[j].y) + RND(st->dy);
      XFillArc(st->dpy, curr_window, st->gcDraw, x - 2, y - 2, 4, 4, 0, 360 * 64);
    }

  }

  for(i = 0; i < st->max_searcher; i++)
    draw_searcher(st, curr_window, i);
 
}

static void animate_anemotaxis(struct state *st, Drawable curr_window)
{
  int i, j;
  Bool dead;

  for(i = 0; i < st->max_src; i++) {

    if(st->source[i] == NULL)
      continue;

    evolve_source(st->source[i]);

    /* reap dead sources for which all particles are gone */
    if(st->source[i]->inv_rate == 0) {

      dead = True;

      for(j = 0; j < st->source[i]->n; j++) {
	if(st->source[i]->yv[j].v != 2) {
	  dead = False;
	  break;
	}
      }

      if(dead == True) {
	destroy_source(st->source[i]);
	st->source[i] = NULL;
      }
    }
  }

  /* Decide if we want to add new  sources */

  for(i = 0; i < st->max_src; i++) {
    if(st->source[i] == NULL && RND(st->max_dist * st->max_src) == 0)
      st->source[i] = new_source(st);
  }

  if(st->max_searcher == 0) { /* kill some sources when searchers don't do that */
    for(i = 0; i < st->max_src; i++) {
      if(st->source[i] != NULL && RND(st->max_dist * st->max_src) == 0) {
	destroy_source(st->source[i]);
	st->source[i] = 0;
      }
    }
  }

  /* Now deal with searchers */

  for(i = 0; i < st->max_searcher; i++) {

    if((st->searcher[i] != NULL) && (st->searcher[i]->state == DONE)) {
      destroy_searcher(st->searcher[i]);
      st->searcher[i] = NULL;
    }

    if(st->searcher[i] == NULL) {

      if(RND(st->max_dist * st->max_searcher) == 0) {

	st->searcher[i] = new_searcher(st);

      }
    }

    if(st->searcher[i] == NULL)
      continue;

    /* Check if searcher found a source or missed all of them */
    for(j = 0; j < st->max_src; j++) {

      if(st->source[j] == NULL || st->source[j]->inv_rate == 0)
	continue;

      if(st->searcher[i]->r.x < 0) {
	st->searcher[i]->state = DONE;
	break;
      }

      if((st->source[j]->r.y == st->searcher[i]->r.y) && 
	 (st->source[j]->r.x == st->searcher[i]->r.x)) {
	st->searcher[i]->state = DONE;
	st->source[j]->inv_rate = 0;  /* source disappears */

	/* Make it flash */
	st->searcher[i]->color = WhitePixel(st->dpy, DefaultScreen(st->dpy));

	break;
      }
    }

    st->searcher[i]->c = 0; /* set it here in case we don't get to get_v() */

    /* Find the concentration at searcher's location */
    
    if(st->searcher[i]->state != DONE) {
      for(j = 0; j < st->max_src; j++) {
	
	if(st->source[j] == NULL)
	  continue;

	get_v(st->source[j], st->searcher[i]);
      
	if(st->searcher[i]->c == 1)
	  break;
      }
    }

    move_searcher(st->searcher[i]);

  }

  draw_image(st, curr_window);
}

static unsigned long
anemotaxis_draw (Display *disp, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XWindowAttributes wa;
  int w, h;

    
  XGetWindowAttributes(st->dpy, st->window, &wa);

  w = wa.width;
  h = wa.height;

  if(w != st->scrWidth || h != st->scrHeight) {
    st->scrWidth = w;
    st->scrHeight = h;
    st->ax = st->scrWidth / (double) st->max_dist;
    st->ay = st->scrHeight / (2. * st->max_dist);
    st->bx = 0.;
    st->by = 0.;
      
    if((st->dx = st->scrWidth / (2 * st->max_dist)) == 0)
      st->dx = 1;
    if((st->dy = st->scrHeight / (4 * st->max_dist)) == 0)
      st->dy = 1;
    XSetLineAttributes(st->dpy, st->gcDraw, st->dx / 3 + 1, LineSolid, CapRound, JoinRound);
  } 

  XFillRectangle (st->dpy, st->b, st->gcClear, 0, 0, st->scrWidth, st->scrHeight);
  animate_anemotaxis(st, st->b);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->backb) {
    XdbeSwapInfo info[1];
    info[0].swap_window = st->window;
    info[0].swap_action = XdbeUndefined;
    XdbeSwapBuffers (st->dpy, info, 1);
  }
  else
#endif
    if (st->dbuf)	{
      XCopyArea (st->dpy, st->b, st->window, st->gcClear, 0, 0,
                 st->scrWidth, st->scrHeight, 0, 0);
      st->b = (st->b == st->ba ? st->bb : st->ba);
    }

  return st->delay;
}



static void
anemotaxis_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
anemotaxis_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
anemotaxis_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  if (st->source) {
    for (i = 0; i < st->max_src; i++)
      if (st->source[i]) destroy_source (st->source[i]);
    free (st->source);
  }
  if (st->searcher) {
    for (i = 0; i < st->max_searcher; i++)
      if (st->searcher[i]) destroy_searcher (st->searcher[i]);
    free (st->searcher);
  }
  free (st);
}




static const char *anemotaxis_defaults [] = {
  ".background: black",
  "*distance: 40",
  "*sources: 25",
  "*searchers: 25",
  "*delay: 20000",
  "*colors: 20",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
#endif
  0
};


static XrmOptionDescRec anemotaxis_options [] = {
  { "-delay",       ".delay",       XrmoptionSepArg, 0 },
  { "-distance",    ".distance",    XrmoptionSepArg, 0 },
  { "-sources",     ".sources",     XrmoptionSepArg, 0 },
  { "-searchers",   ".searchers",   XrmoptionSepArg, 0 },
  { "-colors",      ".colors",      XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Anemotaxis", anemotaxis)
