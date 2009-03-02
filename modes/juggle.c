/* -*- Mode: C; tab-width: 4 -*- */
/* juggle */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)juggle.c 1.0 99/10/16 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <Tim.Auckland@Sun.COM>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 */

/*-
 * TODO
 * Need to add a pattern generator.
 * Add clubs
 * Clap when all the balls are in the air
 */


/*-
Notes on Adam Chalcraft Juggling Notation (used by permission)
a-> Adam's notation  s-> Site swap (Cambridge) notation

To define a map from a-notation to s-notation
("site-swap"), both of which look like doubly infinite sequences of natural
numbers. In s-notation, there is a restriction on what is allowed, namely
for the sequence s_n, the associated function f(n)=n+s_n must be a
bijection. In a-notation, there is no restriction.

To go from a-notation to s-notation, you start by mapping each a_n to a
permutation of N, the natural numbers.

0 -> the identity
1 -> (10) [i.e. f(1)=0, f(0)=1]
2 -> (210) [i.e. f(2)=1, f(1)=0, f(0)=2]
3 -> (3210) [i.e. f(3)=2, f(2)=1, f(1)=0, f(0)=3]
etc.

Then for each n, you look at how long 0 takes to get back to 0 again and
you call this t_n. If a_n=0, for example, then since the identity leaves 0
alone, it gets back to 0 in 1 step, so t_n=1. If a_n=1, then f(0)=1. Now any
further a_n=0 leave 1 alone, but the next a_n>0 sends 1 back to 0. Hence t_n
is 2 + the number of 0's following the 1. Finally, set s_n = t_n - 1.

To give some examples, it helps to have a notation for cyclic sequences. By
(123), for example, I mean ...123123123123... . Now under the a-notation ->
s-notation mapping we have some familiar examples:

(0)->(0), (1)->(1), (2)->(2) etc.
(21)->(31), (31)->(51), (41)->(71) etc.
(10)->(20), (20)->(40), (30)->(60) etc.
(331)->(441), (312)->(612), (303)->(504), (321)->(531)
(43)->(53), (434)->(534), (433)->(633)
(552)->(672)

In general, the number of balls is the *average* of the s-notation, and the
*maximum* of the a-notation. Another theorem is that the minimum values in
the a-notation and the s-notation and equal, and preserved in the same
positions.

The usefulness of a-notation is the fact that there are no restrictions on
what is allowed. This makes random juggle generation much easier. It also
makes enumeration very easy. Another handy feature is computing changes.
Suppose you can do (5) and want a neat change up to (771) in s-notation
[Mike Day actually needed this example!]. Write them both in a-notation,
which gives (5) and (551). Now concatenate them (in general, there may be
more than one way to do this, but not in this example), to get
...55555555551551551551551...
Now convert back to s-notation, to get
...55555566771771771771771...
So the answer is to do two 6 throws and then go straight into (771).
Coming back down of course,
...5515515515515515555555555...
converts to
...7717717717716615555555555...
so the answer is to do a single 661 and then drop straight down to (5).

*/

#ifdef STANDALONE
#define PROGCLASS "Juggle"
#define HACK_INIT init_juggle
#define HACK_DRAW draw_juggle
#define juggle_opts xlockmore_opts
#define DEFAULTS "*delay: 2000 \n" \
"*count: 1 \n" \
"*cycles: 30 \n" \
"*ncolors: 32 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_juggle

#define DEF_PATTERN "" /* All patterns */

static char *pattern;

static XrmOptionDescRec opts[] =
{
  {"-pattern", ".juggle.pattern", XrmoptionSepArg, (caddr_t) NULL}
};
static argtype vars[] =
{
  {(caddr_t *) &pattern, "pattern", "Pattern", DEF_PATTERN, t_String}
};
static OptionStruct desc[] =
{
  {"-pattern string", "Cambridge Juggling Pattern"},
};

ModeSpecOpt juggle_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   juggle_description = {
	"juggle", "init_juggle", "draw_juggle", "release_juggle",
	"draw_juggle", "init_juggle", NULL, &juggle_opts,
	2000, 30, 30, 1, 64, 1.0, "",
	"Shows a Juggler, juggling", 0, NULL
};

#endif

/* Figure */
#define ARMLENGTH 40
#define ARMWIDTH 6
#define POSE 10
#define SX 25
#define SZ 25
#define SY 25
#define HIPY 85
#define RHIPX -15
#define LHIPX 15
#define RFX -25
#define LFX 25
#define FY 155
#define WSTY 65
#define NEY 15
#define HED 35

#define BEAT 25

/* macros */

#ifndef XtNumber
#define XtNumber(arr)   ((unsigned int) (sizeof(arr) / sizeof(arr[0])))
#endif

#define DRAW_BALL(c, x, y) \
	XSetForeground(dpy, gc, (c)); \
	XFillArc(dpy, window, gc, (x) - ARMWIDTH, (y) - ARMWIDTH, \
			 2*ARMWIDTH, 2*ARMWIDTH, 0, 23040)

#define cnew(type) (type *)calloc(1, sizeof(type))
#define GRAVITY(h, t) 4*(double)(h)/((t)*(t))

/* typedefs */

typedef enum {HEIGHT, ADAM} Notation;
typedef enum {Empty, Full, Ball} Throwable;
typedef enum {LEFT, RIGHT} Hand;
typedef enum {THROW, CATCH} Action; /* DROP is not an option */

typedef struct trajectory *TrajectoryPtr;

typedef struct {double a, b, c, d; } Spline;

typedef struct trajectory {
  TrajectoryPtr prev, next;  /* for building list */

  enum {ATCH, THRATCH, ACTION, LINKEDACTION, PTHRATCH, PREDICTOR} status;

  /* Throw */
  char posn;
  int height;
  int adam;

  /* Action */
  Hand hand;
  Action action;

  /* LinkedAction */
  int color;
  TrajectoryPtr balllink;
  TrajectoryPtr handlink;

  /* PThratch */

  /* Predictor */
  Throwable type;
  int start, finish;
  Spline xp, yp;
  int x, y; /* current position */
} Trajectory;

/* structs */

typedef struct {
  int         width;
  int         height;
  int         complexity;
  int         cx;
  int         cy;
  double      Gr;
  int         pattern;
  Trajectory  *head;
  struct {
	int pathn, segsn;
	XPoint   *path;
	XSegment *segs;
  } figure;
  XPoint      arm[2][3];
  int         count;
} jugglestruct;

static jugglestruct *juggles = NULL;

typedef struct {
  char * pattern;
  char * name;
} patternstruct;

#define MINBALLS 2
#define MAXBALLS 7 /* Must add to case statments if increased */

static patternstruct patterns2[] = {
  {"[+2 1]", "+3 1, Typical 2 ball juggler"},
  {"[2 0]", "4 0, 2 balls 1 hand"},
  {"[2 0 1]", "5 0 1"},
  {"[+2 0 +2 0 0]", "+5 0 +5 0 0"},
};
static patternstruct patterns3[] = {
  {"[3]", "3, cascade"},
  {"[+3]", "+3, reverse cascade"},
  {"[=3]", "=3, cascade under arm"},
  {"[&3]", "&3, cascade catching under arm"},
  {"[+3 x3 =3]", "+3 x3 =3, Mill's mess"},
  {"[3 2 1]", "5 3 1"},
  {"[3 3 1]", "4 4 1"},
  {"[3 1 2]",  "6 1 2, See-saw"},
  {"[=3 3 1 2]", "=4 5 1 2"},
  {"[=3 2 2 3 1 2]", "=6 2 2 5 1 2, =4 5 1 2 stretched"},
  {"[+3 3 1 3]",  "+4 4 1 3, anemic shower box"},
  {"[3 3 1]",  "4 4 1"},
  {"[+3 2 3]",  "+4 2 3"},
  {"[+3 1]",  "+5 1, 3 shower"},
  {"[3 0 3 0 3]", "5 0 5 0 5, shake 3 out of 5"},
  {"[3 3 3 0 0]",  "5 5 5 0 0, flash 3 out of 5"},
  {"[3 3 0]", "4 5 0, complete waste of a 5 ball juggler"},
  {"[3 3 3 0 0 0 0]", "7 7 7 0 0 0 0, 3 flash"},
  {"[+3 0 +3 0 +3 0 0]", "+7 0 +7 0 +7 0 0"},
};
static patternstruct patterns4[] = {
  {"[4]", "4, 4 cascade"},
  {"[+4 3]", "+5 3, 4 ball half shower"},
  {"[4 4 2]", "5 5 2"},
  {"[+4 4 4 +4]", "+4 4 4 +4, 4 columns"},
  {"[4 3 +4]", "5 3 +4"},
  {"[+4 1]", "+7 1, 4 shower"},
};
static patternstruct patterns5[] = {
  {"[5]", "5, 5 cascade"},
  {"[+5 x5 =5]", "+5 x5 =5, Mill's mess for 5"},
};
static patternstruct patterns6[] = {
  {"[6]", "6, 6 cascade"},
};
static patternstruct patterns7[] = {
  {"[7]", "7, 7 cascade"},
};

/* Private Functions */

/* list management */
#define INSERT_AFTER(nt, t)						\
  (nt)->next = (t)->next;						\
  (nt)->prev = (t);								\
  (t)->next = (nt);								\
  (nt)->next->prev = (nt)

#define REMOVE(t)								\
  (t)->next->prev = (t)->prev;					\
  (t)->prev->next = (t)->next;					\
  (void) free((void *) t);

static void
add_throw(jugglestruct *sp, char type, int h, Notation n)
{
  Trajectory *t = cnew(Trajectory);
  t->posn = type;
  if (n == ADAM) {
	t->adam = h;
	t->status = ATCH;
  } else {
	t->height = h;
	t->status = THRATCH;
  }
  INSERT_AFTER(t, sp->head->prev);
}

/* add a Thratch to the performance */
static void
program(jugglestruct *sp, char *patn, int repeat)
{
  char *p;
  int h, a, i, seen;
  Notation notation;
  char type;

  for(i=0; i < repeat; i++) {
	type='-';
	h = 0;
	a = 0;
	seen = 0;
	notation = HEIGHT;
	for(p=patn; *p; p++) {
		if (*p >= '0' && *p <='9') { /* digit */
		seen = 1;
		h = 10*h + (*p - '0');
	  } else {
		Notation nn = notation;
		switch (*p) {
		case '[':
		  notation = ADAM;
		  break;
		case '-':
		case '+':
		case '=':
		case '&':
		case 'x':
		  type = *p;
		  break;
		case ']':
		  nn = HEIGHT;
		  /* fall through */
		case ' ':
		  if (seen) {
			add_throw(sp, type, h, notation);
			type='-';
			h = 0;
			seen = 0;
		  }
		  notation = nn;
		  break;
		default:
		  (void) fprintf(stderr, "Unexpected pattern instruction: '%s'\n", p);
		  break;
		}
	  }
	}
	if (seen) {
	  add_throw(sp, type, h, notation);
	}
  }
}

/* 
~~~~\~~~~~\~~~
\\~\\~\~\\\~~~ 
\\~\\\\~\\\~\~
\\\\\\\\\\\~\\

33134231334021
ddbdecdbddeacb

4 4 1 3 12 2 4 1 4 4 13 0 3 1

*/

static void
adam(jugglestruct *sp)
{
  Trajectory *t, *p;
  for(t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == ATCH) {
	  int a = t->adam;
	  t->height = 0;
	  for(p = t->next; a > 0 && p != sp->head; p = p->next) {
		if (p->status != ATCH || p->adam >= a) {
		  a--;
		}
		t->height++;
	  }
	  t->status = THRATCH;
#if 0
	  (void) fprintf(stderr, "%d\t%d\n", t->adam, t->height);
#endif
	}
  }
}

/* Split Thratch notation into explicit throws and catches.
   Usually Catch follows Throw in same hand, but take care of special
   cases. */

/* ..n1.. -> .. LTn RT1 LC RC .. */
/* ..nm.. -> .. LTn LC RTm RC .. */

static void
part(jugglestruct *sp)
{
  Trajectory *t, *nt, *p;
  Hand hand = (LRAND() & 1) ? RIGHT : LEFT;

  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status > THRATCH) {
	  hand = t->hand;
	} else if (t->status == THRATCH) {
	  char posn = '=';
	  switch (t->posn) {
		  /*         throw          catch    */
	    case '-': posn = '-'; t->posn = '+'; break;
	    case '+': posn = '+'; t->posn = '-'; break;
	    case '=': posn = '='; t->posn = '+'; break;
	    case '&': posn = '+'; t->posn = '='; break;
	    case 'x': posn = '='; t->posn = '='; break;
	    default: (void) fprintf(stderr, "unexpected posn %c\n", t->posn); break;
	  }
	  hand = (hand + 1) % 2;
	  t->status = ACTION;
	  t->hand = hand;
	  p = t->prev;

	  if (t->height == 1) {
		p = p->prev; /* early throw */
	  }

	  t->action = CATCH;
	  nt = cnew(Trajectory);
	  nt->status = ACTION;
	  nt->action = THROW;
	  nt->height = t->height;
	  nt->hand = hand;
	  nt->posn = posn;
	  INSERT_AFTER(nt, p);
	}
  }
}

/* Connnect up throws and catches to figure out which ball goes where.
   Do the same with the juggler's hands. */

static void
lob(ModeInfo *mi, jugglestruct *sp)
{
  Trajectory *t, *p;
  int h;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == ACTION) {
#if 0
	  (void) fprintf(stderr, (t->action == CATCH) ? "A %c%c%c\n" : "A %c%c%c%d\n",
			  t->posn,
			  t->hand ? 'R' : 'L',
			  (t->action == THROW) ? 'T' : (t->action == CATCH ? 'C' : 'N'),
			  t->height);
#endif
	  if (t->action == THROW) {
		if (t->type == Empty) {
		  if (MI_NPIXELS(mi) > 2) {
			t->color = 1 + NRAND(MI_NPIXELS(mi) - 2);
		  }
		}
		
		/* search forward for next hand catch */
		for (p = t->next; t->handlink == NULL && p != sp->head; p = p->next) {
		  if (p->action == CATCH) {
			if (t->handlink == NULL && p->hand == t->hand) {
			  t->handlink = p; /* next catch in this hand */
			}
		  }
		}

		if (t->height > 0) {
		  h = t->height - 1;

		  /* search forward for next ball catch */
		  for (p = t->next; t->balllink == NULL&& p != sp->head; p = p->next) {
			if (p->action == CATCH) {
			  if (t->balllink == NULL && --h < 1) { /* caught */
#if 0
				if (p->type == Full) {
				  /* dropped */
				}
#endif
				t->balllink = p; /* complete trajectory */
				p->type = Full;
				p->color = t->color; /* accept catch */
			  }
			}
		  }
		}
		t->type = Empty; /* thrown */
	  } else if (t->action == CATCH) {
		/* search forward for next throw from this hand */
		for (p = t->next; t->handlink == NULL && p != sp->head; p = p->next) {
		  if (p->action == THROW && p->hand == t->hand) {
			p->type = t->type; /* pass ball */
			p->color = t->color; /* pass color */
			t->handlink = p;
		  }
		}
	  }
	  t->status = LINKEDACTION;
	}
  }
}

/* Convert hand position symbols into actual time/space coordinates */
static void
positions(jugglestruct *sp)
{
  Trajectory *t;
  int now = -sp->count;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == PTHRATCH) {
	  now = t->start;
	} else if (t->status == LINKEDACTION) {
	  int xo = 0, yo;

	  /* time */
	  now += sp->count;
	  t->start = now;

	  /* space */
	  yo = ARMLENGTH;
	  switch (t->posn) {
	  case '-': xo = SX - POSE; break;
	  case '+': xo = SX + POSE; break;
	  case '=': xo = - SX - POSE; yo += 2 * POSE; break;
	  default: (void) fprintf(stderr, "unexpected posn %c\n", t->posn); break;
	  }
	  t->x = sp->cx + ((t->hand == LEFT) ? xo : -xo);
	  t->y = sp->cy + yo;

	  t->status = PTHRATCH;
	}
  }
}


/* Private physics functions */

static Spline
makeSpline(int x0, double dx0, int t0, int x1, double dx1, int t1)
{
  Spline s;
  double a, b, c, d;
  int x10;
  double t10;

  x10 = x1 - x0;
  t10 = t1 - t0;
  a = ((dx0 + dx1)*t10 - 2*x10) / (t10*t10*t10);
  b = (3*x10 - (2*dx0 + dx1)*t10) / (t10*t10);
  c = dx0;
  d = x0;
  s.a = a;
  s.b = -3*a*t0 + b;
  s.c = (3*a*t0 - 2*b)*t0 + c;
  s.d = ((-a*t0 + b)*t0 - c)*t0 +d;
  return s;
}

static double
makeSplinePair(Spline *s1, Spline *s2,
			   int x0, double dx0, int t0,
			   int x1, int t1,
			   int x2, double dx2, int t2)
{
  int x10, x21;
  double t21, t10, t20, dx1;
  x10 = x1 - x0;
  x21 = x2 - x1;
  t21 = t2 - t1;
  t10 = t1 - t0;
  t20 = t2 - t0;
  dx1 = (3*x10*t21*t21 + 3*x21*t10*t10 + 3*dx0*t10*t21*t21
		 - dx2*t10*t10*t21 - 4*dx0*t10*t21*t21) /
	(2*t10*t21*t20);
  *s1 = makeSpline(x0, dx0, t0, x1, dx1, t1);
  *s2 = makeSpline(x1, dx1, t1, x2, dx2, t2);
  return dx1;
}

/* Turn abstract timings into physically appropriate trajectories,
   using parabolae for balls and cubic splines for hands. */
static void
physics(jugglestruct *sp)
{
  Trajectory *t, *u, *v, *n;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status != PTHRATCH) {
	  continue;
	}
	if (t->action == THROW) {
	  double dx = 0, dy = 0, ndx = 0, ndy = 0; /* no throw => no velocity */ 
		
	  if (t->balllink != NULL) {
		double t0, dt;

		/* ball follows parabola */
		n = cnew(Trajectory);
		n->start = t->start;
		n->finish = t->balllink->start;
		n->type = Ball;
		n->color = t->color;
		n->status = PREDICTOR;

		t0 = n->start;
		dt = t->balllink->start - t->start;
		dx = (t->balllink->x - t->x) / dt;
		dy = (t->balllink->y - t->y) / dt - sp->Gr/2 * dt;

		/* Represent parabola as a degenerate spline -
		   linear in x, quadratic in y */
		n->xp.a = 0;
		n->xp.b = 0;
		n->xp.c = dx;
		n->xp.d = -dx*t0 + t->x;
		n->yp.a = 0;
		n->yp.b = sp->Gr/2;
		n->yp.c = -sp->Gr*t0 + dy;
		n->yp.d = sp->Gr/2*t0*t0 - dy*t0 + t->y;

		INSERT_AFTER(n, t->prev);
	  }

	  u = t->handlink;
	  if (u == NULL) { /* no next catch */
		continue;
	  }
	  v = u->handlink;
	  if (v == NULL) { /* no next throw */
		continue;
	  }

	  /* double spline takes hand from throw, thru catch, to
		 next throw */
	  t->finish = u->start;
	  t->status = PREDICTOR;

	  u->finish = v->start;
	  u->status = PREDICTOR;

	  if (v->balllink != NULL) { /* compute velocity of next throw */
		double dt;
		dt = v->balllink->start - v->start;
		ndx = (v->balllink->x - v->x) / dt;
		ndy = (v->balllink->y - v->y) / dt - sp->Gr/2 * dt;
	  }

	  (void) makeSplinePair(&t->xp, &u->xp,
					 t->x, dx, t->start,
					 u->x, u->start,
					 v->x, ndx, v->start);
	  (void) makeSplinePair(&t->yp, &u->yp,
					 t->y, dy, t->start,
					 u->y, u->start,
					 v->y, ndy, v->start);
	}
  }
}

/* Given target x, y find_elbow puts hand at target if possible,
 * otherwise makes hand point to the target */
static void
find_elbow(XPoint *h, XPoint *e, int x, int y, int z)
{
  double r, h2, t;

  h2 = x*x + y*y + z*z;
  if (h2 > 4*ARMLENGTH*ARMLENGTH) {
	t = ARMLENGTH/sqrt(h2);
	e->x = t*x;
	e->y = t*y;
	h->x = 2 * e->x;
	h->y = 2 * e->y;
  } else {
	r = sqrt((double)(x*x + z*z));
	t = sqrt(4 * ARMLENGTH * ARMLENGTH / h2 - 1);
	e->x = x*(1 - y*t/r)/2;
	e->y = (y + r*t)/2;
	h->x = x;
	h->y = y;
  }
}

/* NOTE: returned x, y adjusted for arm reach */
static void
draw_arm(ModeInfo * mi, Hand side, int *x, int *y)
{
  Display *dpy = MI_DISPLAY(mi);
  Window win = MI_WINDOW(mi);
  GC gc = MI_GC(mi);
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];

  int sig = (side == LEFT) ? 1 : -1;

  if (sp->arm[side][0].x != *x || sp->arm[side][0].y != *y) {
	XPoint h, e;
	XSetForeground(dpy, gc, MI_BLACK_PIXEL(mi));
	find_elbow(&h, &e, *x - sig*SX - sp->cx, *y - SY - sp->cy, SZ);
	XDrawLines(dpy, win, gc, sp->arm[side], 3, CoordModeOrigin);
	*x = sp->arm[side][0].x = sp->cx + sig*SX + h.x;
	*y = sp->arm[side][0].y = sp->cy + SY + h.y;
	sp->arm[side][1].x = sp->cx + sig*SX + e.x;
	sp->arm[side][1].y = sp->cy + SY + e.y;
  }
  XSetForeground(dpy, gc, MI_WHITE_PIXEL(mi));
  XDrawLines(dpy, win, gc, sp->arm[side], 3, CoordModeOrigin);
}

static void
draw_figure(ModeInfo * mi)
{
  Display *dpy = MI_DISPLAY(mi);
  Window win = MI_WINDOW(mi);
  GC gc = MI_GC(mi);
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];

  XSetForeground(dpy, gc, MI_WHITE_PIXEL(mi));
  XDrawLines(dpy, win, gc, sp->figure.path, sp->figure.pathn,
			 CoordModeOrigin);
  XDrawSegments(dpy, win, gc, sp->figure.segs, sp->figure.segsn);
  XDrawArc(dpy, win, gc,
		   sp->cx - HED/2, sp->cy + NEY - HED, HED, HED, 0, 64*360);

}


/* dumps a human-readable rendition of the current state of the juggle
   pipeline to stderr for debugging */
#ifdef OLDDEBUG
static void
dump(jugglestruct *sp)
{
  Trajectory *t;

  for (t = sp->head->next; t != sp->head; t = t->next) {
	switch (t->status) {
	case THROW:
	  (void) fprintf(stderr, "T %c%d\n", t->posn, t->height);
	  break;
	case ACTION:
	  (void) fprintf(stderr, t->action == CATCH?"A %c%c%c\n":"A %c%c%c%d\n",
			  t->posn,
			  t->hand ? 'R' : 'L',
			  (t->action == THROW)?'T':(t->action == CATCH?'C':'N'),
			  t->height);
	  break;
	case LINKEDACTION:
	  (void) fprintf(stderr, "L %c%c%c%d %d\n",
			  t->posn,
			  t->hand?'R':'L',
			  (t->action == THROW)?'T':(t->action == CATCH?'C':'N'),
			  t->height, t->color);
	  break;
	case PTHRATCH:
	  (void) fprintf(stderr, "O %c%c%c%d %d %2d %6d %6d\n", t->posn,
			  t->hand?'R':'L',
			  (t->action == THROW)?'T':(t->action == CATCH?'C':'N'),
			  t->height, t->type, t->color,
			  t->start, t->finish);
	  break;
	case PREDICTOR:
	  (void) fprintf(stderr, "P %c      %2d %6d %6d %g\n",
			  t->type == Ball?'b':t->type == Empty?'e':'f',
			  t->color,
			  t->start, t->finish, t->yp.c);
	  break;
	default:
	  (void) fprintf(stderr, "status %d not implemented\n", t->status);
	  break;
	}
  }
}
#endif

static void
free_pattern(jugglestruct *sp) {
       if (sp->head) {
              while (sp->head->next != sp->head) {
                Trajectory *t;

                t = sp->head->next;
                REMOVE(t);
              }
              (void) free((void *) sp->head);
	}
        sp->head = NULL;
}

/* Public functions */

void
init_juggle(ModeInfo * mi)
{
	jugglestruct *sp;
	int i;

	XPoint figure1[] = {
	  {LHIPX, HIPY},
	  {0, WSTY},
	  {SX, SY},
	  {-SX, SY},
	  {0, WSTY},
	  {RHIPX, HIPY},
	  {LHIPX, HIPY},
	};
	XSegment figure2[] = {
	  {0, SY, 0, NEY},
	  {LHIPX, HIPY, LFX, FY},
	  {RHIPX, HIPY, RFX, FY},
	};

	if (juggles == NULL) {
		if ((juggles = (jugglestruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (jugglestruct))) == NULL)
			return;
	}
	sp = &juggles[MI_SCREEN(mi)];

	sp->count = 0;

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);
	sp->count = MI_COUNT(mi);
	sp->cx = sp->width/2;
	sp->cy = sp->height/2;
	/* "7" hits top of screen */
	sp->Gr = GRAVITY(sp->cy, 7 * sp->count);

	/* Body Path */
	sp->figure.pathn = XtNumber(figure1);
    if (sp->figure.path)
      (void) free((void *) sp->figure.path);
	sp->figure.path = (XPoint*)malloc(sp->figure.pathn * sizeof(XPoint));
	for (i = 0; i <  sp->figure.pathn; i++) {
	  sp->figure.path[i].x = figure1[i].x + sp->cx;
	  sp->figure.path[i].y = figure1[i].y + sp->cy;
	}
	/* Body Segments */
	sp->figure.segsn = XtNumber(figure2);
    if (sp->figure.segs)
      (void) free((void *) sp->figure.segs);
	sp->figure.segs = (XSegment*)malloc(sp->figure.segsn *
										sizeof(XSegment));
	for (i = 0; i < sp->figure.segsn; i++) {
	  sp->figure.segs[i].x1 = figure2[i].x1 + sp->cx;
	  sp->figure.segs[i].y1 = figure2[i].y1 + sp->cy;
	  sp->figure.segs[i].x2 = figure2[i].x2 + sp->cx;
	  sp->figure.segs[i].y2 = figure2[i].y2 + sp->cy;
	}
	/* Shoulders */
	sp->arm[LEFT][2].x = sp->cx + SX;
	sp->arm[LEFT][2].y = sp->cy + SY;
	sp->arm[RIGHT][2].x = sp->cx - SX;
	sp->arm[RIGHT][2].y = sp->cy + SY;

	XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), ARMWIDTH, LineSolid,
					   CapRound, JoinRound);

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	draw_figure(mi);

	if (sp->head)
	   free_pattern(sp);

	/* close circular list */
	sp->head = cnew(Trajectory);
	sp->head->next = sp->head;
	sp->head->prev = sp->head;

	/*** FIXME-BEGIN ***/
	/* Replace with random pattern generator */
	/* Short-term: choose a set of compatible components and switch
       between them. */

	if (pattern == NULL) {

	  int numberballs = NRAND(MAXBALLS - MINBALLS + 1) + MINBALLS;
	  int startballs = numberballs;

          sp->complexity = (LRAND() & 1) ? 1 : -1;

	  do {
		int maxpatterns = 1;
		int numberpattern; 
		int startpattern;

		switch (numberballs) { /* Did David write this?  Yeah David should know better... */
        case 2:
		  maxpatterns = sizeof (patterns2) / sizeof (patterns2)[0]; 
		  break;
        case 3:
		  maxpatterns = sizeof (patterns3) / sizeof (patterns3)[0]; 
		  break;
        case 4:
		  maxpatterns = sizeof (patterns4) / sizeof (patterns4)[0]; 
		  break;
        case 5:
		  maxpatterns = sizeof (patterns5) / sizeof (patterns5)[0]; 
		  break;
        case 6:
		  maxpatterns = sizeof (patterns6) / sizeof (patterns6)[0]; 
		  break;
        case 7:
		  maxpatterns = sizeof (patterns7) / sizeof (patterns7)[0]; 
		  break;
		}
		numberpattern = NRAND(maxpatterns);
		startpattern = numberpattern;
		do {
		  switch (numberballs) {
          case 2:
            if (MI_IS_VERBOSE(mi))
              (void) printf("number of balls %d, pattern %s\n", numberballs, patterns2[numberpattern].name);
            program(sp, patterns2[numberpattern].pattern, MI_CYCLES(mi));
            break;
          case 3:
            if (MI_IS_VERBOSE(mi))
              (void) printf("number of balls %d, pattern %s\n", numberballs, patterns3[numberpattern].name);
            program(sp, patterns3[numberpattern].pattern, MI_CYCLES(mi));
            break;
          case 4:
            if (MI_IS_VERBOSE(mi))
              (void) printf("number of balls %d, pattern %s\n", numberballs, patterns4[numberpattern].name);
            program(sp, patterns4[numberpattern].pattern, MI_CYCLES(mi));
            break;
          case 5:
            if (MI_IS_VERBOSE(mi))
              (void) printf("number of balls %d, pattern %s\n", numberballs, patterns5[numberpattern].name);
            program(sp, patterns5[numberpattern].pattern, MI_CYCLES(mi));
            break;
          case 6:
            if (MI_IS_VERBOSE(mi))
              (void) printf("number of balls %d, pattern %s\n", numberballs, patterns6[numberpattern].name);
            program(sp, patterns6[numberpattern].pattern, MI_CYCLES(mi));
            break;
          case 7:
            if (MI_IS_VERBOSE(mi))
              (void) printf("number of balls %d, pattern %s\n", numberballs, patterns7[numberpattern].name);
            program(sp, patterns7[numberpattern].pattern, MI_CYCLES(mi));
            break;
		  }
		  numberpattern++;
		  if (numberpattern >= maxpatterns)
			numberpattern = 0;
		} while (startpattern != numberpattern);
		if  (sp->complexity == 1) {
			/* Get progressively harder */
			numberballs++;
			if (numberballs > MAXBALLS) {
			  char buf[2];
			  numberballs = MINBALLS;
			  buf[0] = numberballs + '0';
			  buf[1] = '\0';
			  program(sp, buf, MAXBALLS - MINBALLS);
			}
		} else {
			/* Drop a ball */
			numberballs--;
			if (numberballs < MINBALLS) {
			  numberballs = MAXBALLS;
			} else {
			  char buf[2];
			  buf[0] = numberballs + '0';
			  buf[1] = '\0';
			  program(sp, buf, 1);
			}
		}
#if 0
		{
		  char temp[2];
		  
		  (void) sprintf(temp, "%d", numberballs - 1);
		  program(sp, temp, 1);
		}
#endif
	  } while (startballs != numberballs); /* kind of dangerous */
	} else
	  /* Long-term: patern generator from Adam Chalcraft's juggle
		 enumeration theorem. */

	/* To check ball height frequency spectrum:

	   ../xlock/xlock -inwin -mode juggle 2>&1 |\
	      nawk '{a[$1]++; b[$2]++};
		        END{for (i in b) {print i "," a[i]+0 "," b[i]+0}}'|\
				   sort -n > ~/s

	   gnuplot> plot "../ie/uniform" using 1:2 "%lf,%lf,%lf" title "Uniform A" , "../ie/uniform" using 1:3 "%lf,%lf,%lf" title "Uniform S", "../ie/s" using 1:2 "%lf,%lf,%lf" title "A" , "../ie/s" using 1:3 "%lf,%lf,%lf" title "S"

	*/
#define NUM_BALLS 4
#define SEQUENCE_LENGTH 1000
#define TRIANGULAR

	  if (!strcasecmp(pattern, "uniform")) {
		char buf[4];

		(void) sprintf(buf, "[*]");
		for (i = 0; i < SEQUENCE_LENGTH; i++) {
		  int n = NRAND(NUM_BALLS + 1);
		  buf[1] = n + '0';
		  program(sp, buf, 1);
		}
	  } else if (!strcasecmp(pattern, "triangular")) {
		char buf[4];

		(void) sprintf(buf, "[*]");
		for (i = 0; i < SEQUENCE_LENGTH; i++) {
		  int n, m;
		  do {
			m = NRAND(NUM_BALLS + 1);
			n = NRAND(NUM_BALLS + 1);
		  } while(m >= n);
		  buf[1] = n + '0';
		  program(sp, buf, 1);
		}
	  } else { /* pattern supplied in height or 'a' notation */
		program(sp, pattern, MI_CYCLES(mi));
	  }

	/*** FIXME-END ***/

	adam(sp);

	part(sp);

 	lob(mi, sp);

	positions(sp);

	physics(sp);

#ifdef OLDDEBUG
	dump(sp);
#endif
}

#define CUBIC(s, t) (((s).a * (t) + (s).b) * (t) + (s).c) * (t) + (s).d

void
draw_juggle(ModeInfo * mi)
{
	Display    *dpy = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	jugglestruct *sp = &juggles[MI_SCREEN(mi)];
	Trajectory *traj;
	int future = 0;
	int length = 0;

	MI_IS_DRAWN(mi) = True;

	draw_figure(mi);

	for (traj = sp->head->next; traj != sp->head; traj = traj->next) {
	  length++;
	  if (traj->status != PREDICTOR) {
 		continue;
 	  }
	  if (traj->start > future) {
		future = traj->start;
	  }
	  if (sp->count < traj->start) { /* early */
		continue;
	  } else if (sp->count < traj->finish) { /* working */
		int x = CUBIC(traj->xp, sp->count);
		int y = CUBIC(traj->yp, sp->count);
		int color;

		if (MI_NPIXELS(mi) > 2) {
		  color = MI_PIXEL(mi, traj->color);
		} else {
		  color = MI_WHITE_PIXEL(mi);
		}
		switch (traj->type) {
		case Ball:
		  DRAW_BALL(MI_BLACK_PIXEL(mi), traj->x, traj->y);
		  DRAW_BALL(color, x, y);
		  break;
		case Empty:
		  draw_arm(mi, traj->hand, &x, &y);
		  break;
		case Full:
		  /* draw_arm adjusts x & y */
		  draw_arm(mi, traj->hand, &x, &y);
		  DRAW_BALL(MI_BLACK_PIXEL(mi), traj->x, traj->y);
		  DRAW_BALL(color, x, y);
		  break;
		default:
		  (void) fprintf(stderr, "Unknown trajectory type %d\n", traj->type);
		}
		traj->x = x;
		traj->y = y;
	  } else { /* expired */
		Trajectory *n;
		DRAW_BALL(MI_BLACK_PIXEL(mi), traj->x, traj->y);
		n=traj->prev;
		REMOVE(traj);
		traj=n;
	  }
	}
	sp->count++;

	/*** FIXME-BEGIN ***/
	/* pattern generator would refill here when necessary */
#if 1
	if (future == 0) {
#else
	if (sp->count > MI_CYCLES(mi)) { /* pick a new juggle */
#endif
			init_juggle(mi);
	}
	/*** FIXME-END ***/

}

void
release_juggle(ModeInfo * mi)
{
	if (juggles != NULL) {
      int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			jugglestruct *sp = &juggles[screen];

            if (sp->figure.path)
              (void) free((void *) sp->figure.path);
            if (sp->figure.segs)
              (void) free((void *) sp->figure.segs);
            free_pattern(sp);
		}
		(void) free((void *) juggles);
		juggles = NULL;
	}
}

#endif /* MODE_juggle */
