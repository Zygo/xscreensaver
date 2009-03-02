/* piecewise, 21jan2003
 * Geoffrey Irving <irving@caltech.edu>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <stdarg.h>
#include <math.h>
#include "screenhack.h"

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#define X_PI (180 * 64)

/******** splaying code */

typedef struct _tree {
  struct _tree *l, *r;   /* left and right children */
                         /* extra stuff would go here */
  } tree;

typedef int (*cut)(tree*);    /* cut x is <, =, or > 0 given a <, =, or > x for some a */

/* Top-down splay routine.  Reference:
 *   "Self-adjusting Binary Search Trees", Sleator and Tarjan,
 *   JACM Volume 32, No 3, July 1985, pp 652-686.
 * See page 668 for specific splay transformations */

tree *splay(cut c, tree *t) {
  int v, vv;
  tree *l, *r;
  tree **lr, **rl;
  tree *x, *y, *z;

  if (!t)
    return 0;

  /* initialization */
  x = t;
  l = r = 0;
  lr = &l;
  rl = &r;

  /* top-down splaying loop */
  for (;;) {
    v = c(x);
    if (v == 0)
      break;               /*** success ***/
    else if (v < 0) {
      y = x->l;
      if (!y)
        break;             /*** trivial ***/
      else {
        vv = c(y);
        if (vv == 0) {
          *rl = x;         /*** zig ***/
          rl = &x->l;
          x = y;
          break;
          }
        else if (vv < 0) {
          z = y->l;
          if (!z) {
            *rl = x;       /*** zig ***/
            rl = &x->l;
            x = y;
            break;
            }
          else {
            x->l = y->r;   /*** zig-zig ***/
            y->r = x;
            *rl = y;
            rl = &y->l;
            x = z;
            }
          }
        else { /* vv > 0 */
          z = y->r;
          if (!z) {
            *rl = x;       /*** zig ***/
            rl = &x->l;
            x = y;
            break;
            }
          else {           /*** zig-zag ***/
            *lr = y;
            lr = &y->r;
            *rl = x;
            rl = &x->l;
            x = z;
            }
          }
        }
      }
    else { /* v > 0 */
      y = x->r;
      if (!y)
        break;             /*** trivial ***/
      else {
        vv = c(y);
        if (vv == 0) {
          *lr = x;         /*** zig ***/
          lr = &x->r;
          x = y;
          break;
          }
        else if (vv > 0) {
          z = y->r;
          if (!z) {
            *lr = x;       /*** zig ***/
            lr = &x->r;
            x = y;
            break;
            }
          else {
            x->r = y->l;   /*** zig-zig ***/
            y->l = x;
            *lr = y;
            lr = &y->r;
            x = z;
            }
          }
        else { /* vv < 0 */
          z = y->l;
          if (!z) {
            *lr = x;       /*** zig ***/
            lr = &x->r;
            x = y;
            break;
            }
          else {           /*** zig-zag ***/
            *rl = y;
            rl = &y->l;
            *lr = x;
            lr = &x->r;
            x = z;
            }
          }
        }
      }
    }

  /* completion */
  *lr = x->l;
  x->l = l;
  *rl = x->r;
  x->r = r;
  return x;
  }

tree *splay_min(tree *t) {
  tree *r, **rl;
  tree *x, *y, *z;

  if (!t)
    return 0;

  x = t;
  r = 0;
  rl = &r;

  for (;;) {
    y = x->l;
    if (!y)
      break;           /*** trivial ***/
    else {
      z = y->l;
      if (!z) {
        *rl = x;       /*** zig ***/
        rl = &x->l;
        x = y;
        break;
        }
      else {
        x->l = y->r;   /*** zig-zig ***/
        y->r = x;
        *rl = y;
        rl = &y->l;
        x = z;
        }
      }
    }

  x->l = 0;
  *rl = x->r;
  x->r = r;
  return x;
  }

tree *splay_max(tree *t) {
  tree *l, **lr;
  tree *x, *y, *z;

  if (!t)
    return 0;

  x = t;
  l = 0;
  lr = &l;

  for (;;) {
    y = x->r;
    if (!y)
      break;           /*** trivial ***/
    else {
      z = y->r;
      if (!z) {
        *lr = x;       /*** zig ***/
        lr = &x->r;
        x = y;
        break;
        }
      else {
        x->r = y->l;   /*** zig-zig ***/
        y->l = x;
        *lr = y;
        lr = &y->r;
        x = z;
        }
      }
    }

  *lr = x->l;
  x->l = l;
  x->r = 0;
  return x;
  }

/******** circles and fringe */

struct _fringe;

typedef struct _circle {
  int r;                    /* radius */
  double x, y;              /* position */
  double dx, dy;            /* velocity */

  int visible;              /* default visibility */
  struct _fringe *lo, *hi;  /* lo and hi fringes */

  int ni;                   /* number of intersections */
  int *i;                   /* sorted intersection list */
  } circle;             

typedef struct _fringe {
  struct _fringe *l, *r;  /* left and right children for splay trees */

  circle *c;              /* associated circle */
  int side;               /* 0 for lo, 1 for hi */

  int mni;                /* size of intersection array */
  int ni;                 /* number of intersections */
  int *i;                 /* sorted intersection list */
  } fringe;

inline double fringe_x(fringe *f, double y) {
  double dy, d;
  dy = f->c->y - y;
  d = sqrt(f->c->r * f->c->r - dy * dy);
  return f->side ? f->c->x + d : f->c->x - d;
  }

inline void fringe_add_intersection(fringe *f, double x, double y) {
  f->ni++;
  if (f->mni < f->ni) {
    f->mni += 2;
    f->i = realloc(f->i, sizeof(int) * f->mni);
    }
  f->i[f->ni-1] = rint(atan2(y - f->c->y, x - f->c->x) * X_PI / M_PI);
  }

circle *init_circles(int n, int w, int h) {
  int i, r0, dr, speed;
  double v, a;
  double minradius, maxradius;
  fringe *s = malloc(sizeof(fringe) * n * 2);    /* never freed */
  circle *c = malloc(sizeof(circle) * n);

  speed = get_integer_resource("speed", "Speed");
  minradius = get_float_resource("minradius", "Float");
  maxradius = get_float_resource("maxradius", "Float");
  if (maxradius < minradius)
    maxradius = minradius;

  r0 = ceil(minradius * h); 
  dr = floor(maxradius * h) - r0 + 1;

  for (i=0;i<n;i++) {
    c[i].r = r0 + random() % dr;
    c[i].x = c[i].r + frand(w - 1 - 2 * c[i].r);
    c[i].y = c[i].r + frand(h - 1 - 2 * c[i].r);
    c[i].visible = random() & 1;

    c[i].ni = 0;
    c[i].i = 0;

    a = frand(2 * M_PI);
    v = (1 + frand(0.5)) * speed / 10.0;
    c[i].dx = v * cos(a);
    c[i].dy = v * sin(a);

    c[i].lo = s+i+i;
    c[i].hi = s+i+i+1;
    c[i].lo->c = c[i].hi->c = c+i;
    c[i].lo->side = 0;
    c[i].hi->side = 1;
    c[i].lo->mni = c[i].lo->ni = c[i].hi->mni = c[i].hi->ni = 0;
    c[i].lo->i = c[i].hi->i = 0;
    }

  return c;
  }

/* this is a hack, but I guess that's what I writing anyways */
void tweak_circle(circle *c) {
  c->x += frand(2) - 1;
  c->y += frand(1) + 0.1;
  }

void move_circle(circle *c, int w, int h) {
  c->x += c->dx;
  if (c->x < c->r) {
    c->x = c->r;
    c->dx = -c->dx;
    }
  else if (c->x >= w - c->r) {
    c->x = w - 1 - c->r;
    c->dx = -c->dx;
    }
  c->y += c->dy;
  if (c->y < c->r) {
    c->y = c->r;
    c->dy = -c->dy;
    }
  else if (c->y >= h - c->r) {
    c->y = h - 1 - c->r;
    c->dy = -c->dy;
    }
  }

/******** event queue */

#define START 0
#define CROSS 1
#define FINISH 2

typedef struct _event {
  struct _event *l, *r;    /* left and right children for splay tree */

  int kind;                /* type of event */
  double x, y;             /* position */
  fringe *lo, *hi;         /* fringes */
  } event;

static double event_cut_y;

int event_cut(event *e) {
  return event_cut_y == e->y ? 0 : event_cut_y < e->y ? -1 : 1;
  }

void event_insert(event **eq, event *e) {
  if (!*eq) {
    e->l = e->r = 0;
    *eq = e;
    }

  event_cut_y = e->y;
  *eq = (event*)splay((cut)event_cut, (tree*)*eq);

  if (e->y == (*eq)->y) {
    if (!((e->lo == (*eq)->lo && e->hi == (*eq)->hi) || (e->lo == (*eq)->hi && e->hi == (*eq)->lo))) {
      e->l = (*eq)->l;
      e->r = 0;             /* doing this instead of dying might be dangerous */
      (*eq)->l = e;           
      }
    }
  else if (e->y < (*eq)->y) {
    e->l = (*eq)->l;
    e->r = *eq;
    (*eq)->l = 0;
    *eq = e;
    }
  else {
    e->l = *eq;
    e->r = (*eq)->r; 
    (*eq)->r = 0;
    *eq = e;
    }
  }

void circle_start_event(event **eq, circle *c) {
  event *s;
  s = malloc(sizeof(event));
  s->kind = START;
  s->x = c->x;
  s->y = c->y - c->r;
  s->lo = c->lo;
  s->hi = c->hi;
  event_insert(eq, s);
  }

void circle_finish_event(event **eq, circle *c) {
  event *f;
  f = malloc(sizeof(event));
  f->kind = FINISH;
  f->x = c->x;
  f->y = c->y + c->r;
  f->lo = c->lo;
  f->hi = c->hi;
  event_insert(eq, f);
  }

event *event_next(event **eq) {
  event *e;
  if (!*eq)
    return 0;
  else {
    e = (event*)splay_min((tree*)*eq);
    *eq = e->r; 
    return e;
    }
  }

void event_shred(event *e) {
  if (e) {
    event_shred(e->l);
    event_shred(e->r);
    free(e);
    }
  }

/******** fringe intersection */

inline int check_fringe_intersection(double ye, fringe *lo, fringe *hi, double x, double y) {
  return ye <= y && ((x < lo->c->x) ^ lo->side) && ((x < hi->c->x) ^ hi->side);
  }

void fringe_intersect(event **eq, double y, fringe *lo, fringe *hi) {
  event *e;
  double dx, dy, sd, rs, rd, d, sx, sy, rp, sqd;
  double x1, y1, x2, y2;

  if (lo->c == hi->c)
    return;

  dx = hi->c->x - lo->c->x; 
  dy = hi->c->y - lo->c->y; 
  sd = dx * dx + dy * dy; 

  if (sd == 0)
    return;

  rs = hi->c->r + lo->c->r; 
  rd = hi->c->r - lo->c->r; 
  d = (rd * rd - sd) * (sd - rs * rs);

  if (d <= 0)
    return;
 
  sd = 0.5 / sd;
  rp = rs * rd; 
  sqd = sqrt(d); 
  sx = (lo->c->x + hi->c->x) / 2;
  sy = (lo->c->y + hi->c->y) / 2;
  x1 = sx + sd * (dy * sqd - dx * rp); 
  y1 = sy - sd * (dx * sqd + dy * rp);
  x2 = sx - sd * (dy * sqd + dx * rp);
  y2 = sy + sd * (dx * sqd - dy * rp);

  #define CHECK(xi, yi) (y <= yi && ((xi < lo->c->x) ^ lo->side) && ((xi < hi->c->x) ^ hi->side))

  #define ADD_CROSS(xi, yi, ilo, ihi) {  \
    e = malloc(sizeof(event));           \
    e->kind = CROSS;                     \
    e->x = xi; e->y = yi;                \
    e->lo = ilo; e->hi = ihi;            \
    event_insert(eq, e);                 \
    }

  if (CHECK(x1, y1)) {
    if (CHECK(x2, y2)) {
      if (y1 < y2) {
        ADD_CROSS(x1, y1, lo, hi);
        ADD_CROSS(x2, y2, hi, lo);
        }
      else {
        ADD_CROSS(x1, y1, hi, lo);
        ADD_CROSS(x2, y2, lo, hi);
        }
      }
    else
      ADD_CROSS(x1, y1, lo, hi);
    }
  else if (CHECK(x2, y2))
    ADD_CROSS(x2, y2, lo, hi); 

  return;
  }

/******** fringe trees and event handling */

#define PANIC ((fringe*)1)     /* by alignment, no fringe should every be 1 */

fringe *check_lo(event **eq, double y, fringe *f, fringe *hi) {
  if (f) {
    f = (fringe*)splay_max((tree*)f);
    fringe_intersect(eq, y, f, hi);
    }
  return f;
  }

fringe *check_hi(event **eq, double y, fringe *lo, fringe *f) {
  if (f) {
    f = (fringe*)splay_min((tree*)f);
    fringe_intersect(eq, y, lo, f);
    }
  return f;
  }

double fringe_start_cut_x;
double fringe_start_cut_y;

int fringe_start_cut(fringe *f) {
  double x = fringe_x(f, fringe_start_cut_y);
  return fringe_start_cut_x == x ? 0 : fringe_start_cut_x < x ? -1 : 1;
  }

fringe *fringe_start(event **eq, fringe *f, double x, double y, fringe *lo, fringe *hi) {
  double sx;

  if (!f) {
    circle_finish_event(eq, lo->c);
    lo->l = 0;
    lo->r = hi;
    hi->l = hi->r = 0;
    return lo;
    }

  fringe_start_cut_x = x;
  fringe_start_cut_y = y;
  f = (fringe*)splay((cut)fringe_start_cut, (tree*)f);

  sx = fringe_x(f, y);
  if (x == sx) {       /* time to cheat my way out of handling degeneracies */
    tweak_circle(lo->c);
    circle_start_event(eq, lo->c);
    return f;
    }
  else if (x < sx) {
    circle_finish_event(eq, lo->c);
    f->l = check_lo(eq, y, f->l, lo);    
    fringe_intersect(eq, y, hi, f);
    lo->l = f->l;
    lo->r = f;
    f->l = hi;
    hi->l = hi->r = 0;
    return lo;
    }
  else {
    circle_finish_event(eq, lo->c);
    fringe_intersect(eq, y, f, lo);
    f->r = check_hi(eq, y, hi, f->r);
    hi->r = f->r;
    hi->l = f;
    f->r = lo;
    lo->l = lo->r = 0;
    return hi;
    }
  }

double fringe_double_cut_x;
double fringe_double_cut_y;
fringe *fringe_double_cut_lo;
fringe *fringe_double_cut_hi;

int fringe_double_cut(fringe *f) {
  double x;
  if (f == fringe_double_cut_lo || f == fringe_double_cut_hi)
    return 0;
  x = fringe_x(f, fringe_double_cut_y);
  return fringe_double_cut_x == x ? 0 : fringe_double_cut_x < x ? -1 : 1;
  }

int fringe_double_splay(fringe *f, double x, double y, fringe *lo, fringe *hi) {
  fringe_double_cut_x = x;
  fringe_double_cut_y = y;
  fringe_double_cut_lo = lo;
  fringe_double_cut_hi = hi;
  f = (fringe*)splay((cut)fringe_double_cut, (tree*)f);

  if (f == lo)
    return (f->r = (fringe*)splay_min((tree*)f->r)) == hi;
  else if (f == hi)
    return (f->l = (fringe*)splay_max((tree*)f->l)) == lo;
  else
    return 0;
  }

fringe *fringe_cross(event **eq, fringe *f, double x, double y, fringe *lo, fringe *hi) {
  fringe *l, *r;
  if (!fringe_double_splay(f, x, y, lo, hi))
    return PANIC;
  l = check_lo(eq, y, lo->l, hi);
  r = check_hi(eq, y, lo, hi->r);
  lo->l = hi;
  lo->r = r;
  hi->l = l;
  hi->r = 0;
  return lo;
  }

fringe *fringe_finish(event **eq, fringe *f, double x, double y, fringe *lo, fringe *hi) {
  if (!fringe_double_splay(f, x, y, lo, hi))
    return PANIC;
  else if (!lo->l)
    return hi->r;
  else if (!hi->r)
    return lo->l;
  else {
    lo->l = (fringe*)splay_max((tree*)lo->l);
    hi->r = (fringe*)splay_min((tree*)hi->r);
    fringe_intersect(eq, y, lo->l, hi->r);
    lo->l->r = hi->r;
    hi->r->l = 0;
    return lo->l;
    }
  }

/******** plane sweep */

void sweep(int n, circle *c) {
  int i;
  event *eq, *e;
  fringe *f;

  RESTART:
  #define CHECK_PANIC()                 \
    if (f == PANIC) {                   \
      free(e);                          \
      event_shred(eq);                  \
      for (i=0;i<n;i++) {               \
        tweak_circle(c+i);              \
        c[i].lo->ni = c[i].hi->ni = 0;  \
        }                               \
      goto RESTART;                     \
      }

  eq = 0;
  for (i=0;i<n;i++)
    circle_start_event(&eq, c+i); 
  f = 0;

  while ((e = event_next(&eq))) {
    switch (e->kind) {
      case START:
        f = fringe_start(&eq, f, e->x, e->y, e->lo, e->hi);
        break;
      case CROSS:
        f = fringe_cross(&eq, f, e->x, e->y, e->lo, e->hi);
        CHECK_PANIC();
        fringe_add_intersection(e->lo, e->x, e->y);
        fringe_add_intersection(e->hi, e->x, e->y);
        break;
      case FINISH:
        f = fringe_finish(&eq, f, e->x, e->y, e->lo, e->hi);
        CHECK_PANIC();
        break;
      }
    free(e);
    }
  }

/******** circle drawing */

void adjust_circle_visibility(circle *c) {
  int i, j, n, a;
  int *in;
  n = c->lo->ni + c->hi->ni;
  in = malloc(sizeof(int) * n);
  for (i=0;i<c->hi->ni;i++)
    in[i] = c->hi->i[i]; 
  for (i=c->lo->ni-1;i>=0;i--)
    in[n-i-1] = c->lo->i[i] > 0 ? c->lo->i[i] : c->lo->i[i] + 2 * X_PI;
  c->lo->ni = c->hi->ni = 0;

  i = j = 0;
  a = 0;
  while (i < n && j < c->ni)           /* whee */
    a = (in[i] < c->i[j] ? in[i++] : c->i[j++]) - a;
  while (i < n)
    a = in[i++] - a;
  while (j < c->ni)
    a = c->i[j++] - a;

  if (a > X_PI) 
    c->visible = !c->visible;
  free(c->i);  
  c->ni = n;
  c->i = in;
  }

#define ARC_BUFFER_SIZE 256
int arc_buffer_count = 0;
XArc arc_buffer[ARC_BUFFER_SIZE];

void flush_arc_buffer(Display *dpy, Drawable w, GC gc) {
  if (arc_buffer_count) {
    XDrawArcs(dpy, w, gc, arc_buffer, arc_buffer_count);
    arc_buffer_count = 0;
    }
  }

void draw_circle(Display *dpy, Drawable w, GC gc, circle *c) {
  int i, xi, yi, di;
  adjust_circle_visibility(c); 

  xi = rint(c->x - c->r);
  yi = rint(c->y - c->r);
  di = c->r << 1;

  #define ARC(p, a1, a2) {                                \
    if (((p) & 1) ^ c->visible) {                         \
      arc_buffer[arc_buffer_count].x      = xi;           \
      arc_buffer[arc_buffer_count].y      = yi;           \
      arc_buffer[arc_buffer_count].width  = di;           \
      arc_buffer[arc_buffer_count].height = di;           \
      arc_buffer[arc_buffer_count].angle1 = -(a1);        \
      arc_buffer[arc_buffer_count].angle2 = (a1) - (a2);  \
      arc_buffer_count++;                                 \
      if (arc_buffer_count == ARC_BUFFER_SIZE)            \
        flush_arc_buffer(dpy, w, gc);                     \
      }                                                   \
    }

  if (!c->ni)
    ARC(0, 0, 2 * X_PI)
  else
    ARC(0, c->i[c->ni-1], c->i[0] + 2 * X_PI)
  for (i=1;i<c->ni;i++)
    ARC(i, c->i[i-1], c->i[i])
  }

/******** toplevel */

char *progclass = "Piecewise";

char *defaults [] = {
  ".background:         black",
  ".foreground:         white",
  "*delay:              5000",
  "*speed:              15",
  "*ncolors:            256",
  ".colorspeed:         10",

  ".count:              32",
  ".minradius:          0.05",
  ".maxradius:          0.2",   

  "*doubleBuffer:       True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:             True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
  };

XrmOptionDescRec options [] = {
  { "-delay",           ".delay",       XrmoptionSepArg, 0 },
  { "-ncolors",         ".ncolors",     XrmoptionSepArg, 0 },
  { "-speed",           ".speed",       XrmoptionSepArg, 0 },
  { "-colorspeed",      ".colorspeed",  XrmoptionSepArg, 0 },

  { "-count",           ".count",       XrmoptionSepArg, 0 },
  { "-minradius",       ".minradius",   XrmoptionSepArg, 0 },
  { "-maxradius",       ".maxradius",   XrmoptionSepArg, 0 },

  { "-db",              ".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",           ".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
  };

void screenhack(Display *dpy, Window window) {
  int i;
  Bool dbuf;
  XColor *colors;
  XGCValues gcv;
  GC erase_gc, draw_gc;
  XWindowAttributes xgwa;
  Pixmap b = 0, ba = 0, bb = 0;    /* double-buffering pixmap */

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb = 0;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  int count, delay, ncolors, colorspeed, color_index, flags, iterations;
  int color_iterations;
  circle *circles;

  count = get_integer_resource("count", "Integer");
  delay = get_integer_resource("delay", "Integer");
  ncolors = get_integer_resource("ncolors", "Integer");
  colorspeed = get_integer_resource("colorspeed", "Integer");
  dbuf = get_boolean_resource("doubleBuffer", "Boolean");

  color_iterations = colorspeed ? 100 / colorspeed : 100000;
  if (!color_iterations)
    color_iterations = 1;

  XGetWindowAttributes(dpy, window, &xgwa);
  colors = calloc(sizeof(XColor), ncolors);

  if (get_boolean_resource("mono", "Boolean")) {  
    MONO:
      ncolors = 1;
      colors[0].pixel = get_pixel_resource("foreground", "Foreground", dpy, xgwa.colormap);
    }
  else {
    make_color_loop(dpy, xgwa.colormap, 0, 1, 1, 120, 1, 1, 240, 1, 1, colors, &ncolors, True, False);
    if (ncolors < 2)
      goto MONO; 
    }

  if (dbuf) {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    b = backb = xdbe_get_backbuffer(dpy, window, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    
    if (!b) {
      ba = XCreatePixmap(dpy, window, xgwa.width, xgwa.height,xgwa.depth);
      bb = XCreatePixmap(dpy, window, xgwa.width, xgwa.height,xgwa.depth);
      b = ba;
      }
    }
  else
    b = window;

  /* erasure gc */
  gcv.foreground = get_pixel_resource("background", "Background", dpy, xgwa.colormap);
  erase_gc = XCreateGC (dpy, b, GCForeground, &gcv);

  /* drawing gc */
  flags = GCForeground;
  color_index = random() % ncolors;
  gcv.foreground = colors[color_index].pixel;
  draw_gc = XCreateGC(dpy, b, flags, &gcv);

  /* initialize circles */
  circles = init_circles(count, xgwa.width, xgwa.height);
  
  iterations = 0;
  for (;;) {
    XFillRectangle (dpy, b, erase_gc, 0, 0, xgwa.width, xgwa.height);

    sweep(count, circles);
    for (i=0;i<count;i++) {
      draw_circle(dpy, b, draw_gc, circles+i);
      move_circle(circles+i, xgwa.width, xgwa.height);
      }
    flush_arc_buffer(dpy, b, draw_gc);

    if (++iterations % color_iterations == 0) {
      color_index = (color_index + 1) % ncolors;
      XSetForeground(dpy, draw_gc, colors[color_index].pixel);
      }

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    if (backb) {
      XdbeSwapInfo info[1];
      info[0].swap_window = window;
      info[0].swap_action = XdbeUndefined;
      XdbeSwapBuffers (dpy, info, 1);
      }
    else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    if (dbuf) {
      XCopyArea (dpy, b, window, erase_gc, 0, 0, xgwa.width, xgwa.height, 0, 0);
      b = (b == ba ? bb : ba);
      }

    XSync(dpy, False);
    screenhack_handle_events(dpy);
    if (delay)
      usleep(delay);
    }
  }
