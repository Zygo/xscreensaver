/* -*- Mode: C; tab-width: 4 -*- */
/* juggle */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)juggle.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <Tim.Auckland@Procket.com>
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
 * Revision History
 * 01-Nov-2000: Allocation checks
 * 1996: Written
 */

/*-
 * TODO
 * Fix timing to run at approx same speed on all machines.
 * Store shorter pattern and refill when required.
 * Use -cycles and -count in a rational manner.
 * Merge pattern selector with pattern generator.
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

[The number of balls in the generated pattern occasionally changes.  In
 order to decrease the number of balls I had to introduce a new symbol
 into the Adam notation, [*] which means 'lose the current ball'.]
*/

#ifdef STANDALONE
#define MODE_juggle
#define PROGCLASS "Juggle"
#define HACK_INIT init_juggle
#define HACK_DRAW draw_juggle
#define juggle_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
"*count: 150 \n" \
"*cycles: 30 \n" \
"*ncolors: 32 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_juggle

#define DEF_PATTERN "." /* All patterns */
#define DEF_TRAIL "0" /* No trace */
#ifdef UNI
#define DEF_UNI "FALSE" /* No unicycle */ /* Not implemented yet */
#endif
#define DEF_SOLID "FALSE" /* Not solid */

static char *pattern;
static int trail;
#ifdef UNI
static Bool uni;
#endif
static Bool solid;

static XrmOptionDescRec opts[] =
{
  {(char* ) "-pattern", (char *) ".juggle.pattern",
   XrmoptionSepArg, (caddr_t) NULL},
  {(char* ) "-trail", (char *) ".juggle.trail",
   XrmoptionSepArg, (caddr_t) NULL},
#ifdef UNI
  {(char *) "-uni", (char *) ".juggle.uni", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+uni", (char *) ".juggle.uni", XrmoptionNoArg, (caddr_t) "off"},
#endif
  {(char *) "-solid", (char *) ".juggle.solid", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+solid", (char *) ".juggle.solid", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
  {(caddr_t *) &pattern, (char *) "pattern",
   (char *) "Pattern", (char *) DEF_PATTERN, t_String},
  {(caddr_t *) &trail, (char *) "trail",
   (char *) "Trail", (char *) DEF_TRAIL, t_Int},
#ifdef UNI
  {(caddr_t *) &uni, (char *) "uni",
   (char *) "Uni", (char *) DEF_UNI, t_Bool},
#endif
  {(caddr_t *) &solid, (char *) "solid",
   (char *) "Solid", (char *) DEF_SOLID, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-pattern string", (char *) "Cambridge Juggling Pattern"},
  {(char *) "-trail num", (char *) "Trace Juggling Patterns"},
#ifdef UNI
  {(char *) "-/+uni", (char *) "Unicycle"},
#endif
  {(char *) "-/+solid", (char *) "solid color (else its a 4 panel look (half white))"}
};

ModeSpecOpt juggle_opts =
{sizeof opts / sizeof opts[0], opts,
 sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   juggle_description = {
	"juggle", "init_juggle", "draw_juggle", "release_juggle",
	"draw_juggle", "init_juggle", (char *) NULL, &juggle_opts,
	10000, 150, 30, 1, 64, 1.0, "",
	"Shows a Juggler, juggling", 0, NULL
};

#endif

#ifdef USE_XVMSUTILS
#include <X11/unix_time.h>
#endif
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

/* Figure */
#define ARMLENGTH ((int) (40.0 * sp->scale))
#define ARMWIDTH ((int) (8.0 * sqrt(sp->scale)))
#define POSE ((int) (10.0 * sp->scale))
#define SX ((int) (25.0 * sp->scale))
#define SZ ((int) (25.0 * sp->scale))
#define SY ((int) (25.0 * sp->scale))
#define HIPY ((int) (85.0 * sp->scale))
#define RHIPX ((int) (-15.0 * sp->scale))
#define LHIPX ((int) (15.0 * sp->scale))
#define RFX ((int) (-25.0 * sp->scale))
#define LFX ((int) (25.0 * sp->scale))
#define FY ((int) (155.0 * sp->scale))
#define WSTY ((int) (65.0 * sp->scale))
#define NEY ((int) (15.0 * sp->scale))
#define HED ((int) (35.0 * sp->scale))
#define BALLRADIUS ARMWIDTH
#define FIGURE1 7
#define FIGURE2 3
#define TRACE_LENGTH 50
#define SPIN_DEGREES 750  /* Average spinning between a throw and the next catch */

/* macros */

#ifndef XtNumber
#define XtNumber(arr)   ((unsigned int) (sizeof(arr) / sizeof(arr[0])))
#endif

#define GRAVITY(h, t) 4*(double)(h)/((t)*(t))

#define THROW_CATCH_INTERVAL (sp->count)
#define THROW_NULL_INTERVAL  (sp->count * 0.5)
#define CATCH_THROW_INTERVAL (sp->count * 0.2)
#define COR 0.8  /* coeff of restitution of balls (1 = perfect bounce) */


/* typedefs */

typedef enum {HEIGHT, ADAM} Notation;
typedef enum {Empty, Full, Ball} Throwable;
typedef enum {LEFT, RIGHT} Hand;
typedef enum {THROW, CATCH} Action; /* DROP is not an option */
typedef enum {ATCH, THRATCH, ACTION, LINKEDACTION, PTHRATCH, BPREDICTOR,
	PREDICTOR} TrajectoryStatus;

typedef struct trajectory *TrajectoryPtr;

typedef struct {double a, b, c, d; } Spline;

typedef struct trajectory {
  TrajectoryPtr prev, next;  /* for building list */
  TrajectoryStatus status;

  /* Throw */
  char posn;
  int height;
  int adam;

  /* Action */
  Hand hand;
  Action action;

  /* LinkedAction */
  int color;
  int spin, divisions;
  double degree_offset;
  TrajectoryPtr balllink;
  TrajectoryPtr handlink;

  /* PThratch */

  double dx; /* initial velocity */
  double dy;

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
  double      scale;
  int         complexity;
  int         cx;
  int         cy;
  double      Gr;
  int         pattern;
  Trajectory  *head;
  XPoint   figure_path[FIGURE1];
  XSegment figure_segs[FIGURE2];
  XPoint      arm[2][3];
  XPoint      *trace;
  int         traceindex;
  int         count;
  time_t      begintime; /* seconds */
  int         time; /* millisecond timer */
  Bool        solid, uni;	
} jugglestruct;

static jugglestruct *juggles = (jugglestruct *) NULL;

typedef struct {
  char * pattern;
  char * name;
} patternstruct;

#define MINBALLS 2
#define MAXBALLS 7

typedef struct {
  int start;
  int number;
} PatternIndex;

static PatternIndex* patternindex = (PatternIndex *) NULL;

/* List of popular patterns, in any order */
static patternstruct portfolio[] = {
  {(char *) "[+2 1]", (char *) "+3 1, Typical 2 ball juggler"},
  {(char *) "[2 0]", (char *) "4 0, 2 balls 1 hand"},
  {(char *) "[2 0 1]", (char *) "5 0 1"},
  {(char *) "[+2 0 +2 0 0]", (char *) "+5 0 +5 0 0"},
  {(char *) "[3]", (char *) "3, cascade"},
  {(char *) "[+3]", (char *) "+3, reverse cascade"},
  {(char *) "[=3]", (char *) "=3, cascade under arm"},
  {(char *) "[&3]", (char *) "&3, cascade catching under arm"},
  {(char *) "[_3]", (char *) "_3, bouncing cascade"},
  {(char *) "[+3 x3 =3]", (char *) "+3 x3 =3, Mill's mess"},
  {(char *) "[3 2 1]", (char *) "5 3 1"},
  {(char *) "[3 3 1]", (char *) "4 4 1"},
  {(char *) "[3 1 2]", (char *) "6 1 2, See-saw"},
  {(char *) "[=3 3 1 2]", (char *) "=4 5 1 2"},
  {(char *) "[=3 2 2 3 1 2]", (char *) "=6 2 2 5 1 2, =4 5 1 2 stretched"},
  {(char *) "[+3 3 1 3]", (char *) "+4 4 1 3, anemic shower box"},
  {(char *) "[3 3 1]", (char *) "4 4 1"},
  {(char *) "[+3 2 3]", (char *) "+4 2 3"},
  {(char *) "[+3 1]", (char *) "+5 1, 3 shower"},
  {(char *) "[_3 1]", (char *) "_5 1, bouncing 3 shower"},
  {(char *) "[3 0 3 0 3]", (char *) "5 0 5 0 5, shake 3 out of 5"},
  {(char *) "[3 3 3 0 0]", (char *) "5 5 5 0 0, flash 3 out of 5"},
  {(char *) "[3 3 0]", (char *) "4 5 0, complete waste of a 5 ball juggler"},
  {(char *) "[3 3 3 0 0 0 0]", (char *) "7 7 7 0 0 0 0, 3 flash"},
  {(char *) "[+3 0 +3 0 +3 0 0]", (char *) "+7 0 +7 0 +7 0 0"},
  {(char *) "[4]", (char *) "4, 4 cascade"},
  {(char *) "[+4 3]", (char *) "+5 3, 4 ball half shower"},
  {(char *) "[4 4 2]", (char *) "5 5 2"},
  {(char *) "[+4 4 4 +4]", (char *) "+4 4 4 +4, 4 columns"},
  {(char *) "[4 3 +4]", (char *) "5 3 +4"},
  {(char *) "[+4 1]", (char *) "+7 1, 4 shower"},
  {(char *) "[4 4 4 4 0]", (char *) "5 5 5 5 0, learning 5"},
  {(char *) "[5]", (char *) "5, 5 cascade"},
  {(char *) "[_5 _5 _5 _5 _5 5 5 5 5 5]", (char *) "_5 _5 _5 _5 _5 5 5 5 5 5, jump rope"},
  {(char *) "[+5 x5 =5]", (char *) "+5 x5 =5, Mill's mess for 5"},
  {(char *) "[6]", (char *) "6, 6 cascade"},
  {(char *) "[7]", (char *) "7, 7 cascade"},
  {(char *) "[_7]", (char *) "_7, bouncing 7 cascade"},
};

/* Private Functions */

/* list management */

/* t receives trajectory to be created.  ot must point to an existing
   trajectory or be identical to t to start a new list. */
#define INSERT_AFTER_TOP(t, ot)					\
  if ((t = (Trajectory *)calloc(1, sizeof(Trajectory))) == NULL) \
    {free_juggle(sp); return;}					\
  (t)->next = (ot)->next;					\
  (t)->prev = (ot);						\
  (ot)->next = (t);						\
  (t)->next->prev = (t)
#define INSERT_AFTER(t, ot)					\
  if ((t = (Trajectory *)calloc(1, sizeof(Trajectory))) == NULL) \
    {free_juggle(sp); return False;}				\
  (t)->next = (ot)->next;					\
  (t)->prev = (ot);						\
  (ot)->next = (t);						\
  (t)->next->prev = (t)


/* t must point to an existing trajectory.  t must not be an
   expression ending ->next or ->prev */
#define REMOVE(t)						\
  (t)->next->prev = (t)->prev;					\
  (t)->prev->next = (t)->next;					\
  (void) free((void *) t)

static void
free_pattern(jugglestruct *sp) {
	if (sp->head != NULL) {
		while (sp->head->next != sp->head) {
			Trajectory *t = sp->head->next;

			REMOVE(t); /* don't eliminate t */
		}
		(void) free((void *) sp->head);
  		sp->head = (Trajectory *) NULL;
	}
}

static void
free_juggle(jugglestruct *sp)
{
	if (sp->trace != NULL) {
		(void) free((void *) sp->trace);
		sp->trace = (XPoint *) NULL;
	}
	free_pattern(sp);
}

static Bool
add_throw(jugglestruct *sp, char type, int h, Notation n)
{
  Trajectory *t;

  INSERT_AFTER(t, sp->head->prev);
  t->posn = type;
  if (n == ADAM) {
	t->adam = h;
	t->status = ATCH;
  } else {
	t->height = h;
	t->status = THRATCH;
  }
  return True;
}

/* add a Thratch to the performance */
static Bool
program(ModeInfo *mi, const char *patn, int repeat)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  const char *p;
  int h, i, seen;
  Notation notation;
  char type;

  if (MI_IS_VERBOSE(mi)) {
	(void) fprintf(stderr, "%s x %d\n", patn, repeat);
  }

  for(i=0; i < repeat; i++) {
	type=' ';
	h = 0;
	seen = 0;
	notation = HEIGHT;
	for(p=patn; *p; p++) {
		if (*p >= '0' && *p <='9') {
		seen = 1;
		h = 10*h + (*p - '0');
	  } else {
		Notation nn = notation;
		switch (*p) {
		case '[':            /* begin Adam notation */
		  notation = ADAM;
		  break;
		case '-':            /* Inside throw */
		case '+':            /* Outside throw */
		case '=':            /* Cross throw */
		case '&':            /* Cross catch */
		case 'x':            /* Cross throw and catch */
		case '_':            /* Bounce */
		  type = *p;
		  break;
		case '*':            /* Lose ball */
		  seen = 1;
		  h = -1;
		  /* fall through */
		case ']':             /* end Adam notation */
		  nn = HEIGHT;
		  /* fall through */
		case ' ':
		  if (seen) {
			if (!add_throw(sp, type, h, notation))
				return False;
			type=' ';
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
	  if (!add_throw(sp, type, h, notation))
		return False;
	}
  }
	return True;
}

/*
 ~~~~\~~~~~\~~~
 \\~\\~\~\\\~~~
 \\~\\\\~\\\~\~
 \\\\\\\\\\\~\\

[33134231334021]

4 4 1 3 12 2 4 1 4 4 13 0 3 1

*/
#define BOUNCEOVER 10

static void
adam(jugglestruct *sp)
{
  Trajectory *t, *p;
  for(t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == ATCH) {
	  int a = t->adam;
	  t->height = 0;
	  for(p = t->next; a > 0 && p != sp->head; p = p->next) {
		if (p->status != ATCH || p->adam < 0 || p->adam>= a) {
		  a--;
		}
		t->height++;
	  }
	  if(t->height > BOUNCEOVER && t->posn == ' '){
		t->posn = '_'; /* high defaults can be bounced */
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

static Bool
part(jugglestruct *sp)
{
  Trajectory *t, *nt, *p;
  Hand hand = (LRAND() & 1) ? RIGHT : LEFT;

  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status > THRATCH) {
	  hand = t->hand;
	} else if (t->status == THRATCH) {
	  char posn = '=';

	  /* plausibility check */
	  if (t->height <= 2 && t->posn == '_') {
		t->posn = ' '; /* no short bounces */
	  }
	  if (t->height <= 1 && (t->posn == '=' || t->posn == '&')) {
		t->posn = ' '; /* 1's need close catches */
	  }

	  switch (t->posn) {
		  /*         throw          catch    */
	  case ' ': /* fall through */
	  case '-': posn = '-'; t->posn = '+'; break;
	  case '+': posn = '+'; t->posn = '-'; break;
	  case '=': posn = '='; t->posn = '+'; break;
	  case '&': posn = '+'; t->posn = '='; break;
	  case 'x': posn = '='; t->posn = '='; break;
	  case '_': posn = '_'; t->posn = '-'; break;
	  default: (void) fprintf(stderr, "unexpected posn %c\n", t->posn); break;
	  }
	  hand = (Hand) ((hand + 1) % 2);
	  t->status = ACTION;
	  t->hand = hand;
	  p = t->prev;

	  if (t->height == 1) {
		p = p->prev; /* early throw */
	  }
	  t->action = CATCH;
	  INSERT_AFTER(nt, p);
	  nt->status = ACTION;
	  nt->action = THROW;
	  nt->height = t->height;
	  nt->hand = hand;
	  nt->posn = posn;
	}
  }
  return True;
}

/* Connnect up throws and catches to figure out which ball goes where.
   Do the same with the juggler's hands. */

static void
lob(ModeInfo *mi)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
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
		  t->spin = NRAND(5) - 2;
		  t->degree_offset = NRAND(360);
		  t->divisions = 2 * ((LRAND() & 1) + 1);
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
				p->spin = t->spin;
				p->degree_offset = t->degree_offset;
				p->divisions = t->divisions;
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
			p->spin = NRAND(5) - 2;
			p->degree_offset = NRAND(360);
			p->divisions = 2 * ((LRAND() & 1) + 1);
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
  int now = 0;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == PTHRATCH) {
	  now = t->start;
	} else if (t->status == LINKEDACTION) {
	  int xo = 0, yo;

	  /* time */
	  if (t->action == CATCH) {
		if (t->type == Empty) {
		  now += (int) THROW_NULL_INTERVAL; /* failed catch is short */
		} else {
		  now += THROW_CATCH_INTERVAL;     /* successful catch */
		}
	  } else {
		now += (int) CATCH_THROW_INTERVAL;  /* throws are very short */
	  }
	  t->start = now;

	  /* space */
	  yo = ARMLENGTH;
	  switch (t->posn) {
	  case '-': xo = SX - POSE; break;
	  case '_':
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

/* Turn abstract timings into physically appropriate ball trajectories. */
static Bool
projectile(jugglestruct *sp)
{
  Trajectory *t, *n;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status != PTHRATCH) {
	  continue;
	}
	if (t->action == THROW) {
	  if (t->balllink != NULL) {
		if (t->posn == '_') { /* Bounce once */
		  double tc, y0, yf, yc, tb, e, i;

		  tc = t->balllink->start - t->start;

		  yf = sp->cy + FY;
		  y0 = t->y;
		  yc = t->balllink->y;
		  e = 1; /* permissible error in yc */

		  /*
			 tb = time to bounce
			 yt = height at catch time after one bounce
			 one or three roots according to timing
			 find one by interval bisection
		  */
		  tb = tc;
		  for(i = tc / 2; i > 0; i/=2){
			double dy, dt, yt;
			if(tb == 0){
			  (void) fprintf(stderr, "div by zero!\n");
			  break;
			}
			dy = (yf - y0)/tb + 0.5*sp->Gr*tb;
			dt = tc - tb;
			yt = -COR*dy*dt + 0.5*sp->Gr*dt*dt + yf;
			if(yt > yc + e){
			  tb-=i;
			}else if(yt < yc - e){
			  tb+=i;
			}else{
			  break;
			}
		  }

		  {
			double t0, dy;

			t->dx = (t->balllink->x - t->x) / tc;

			/* ball follows parabola down */
			INSERT_AFTER(n, t->prev);
			n->start = t->start;
			n->finish = (int) (t->start + tb);
			n->type = Ball;
			n->color = t->color;
			n->spin = t->spin;
			n->degree_offset = t->degree_offset;
			n->divisions = t->divisions;
			n->status = PREDICTOR;

			t->dy = (yf - y0)/tb - 0.5*sp->Gr*tb;
			t0 = n->start;
			/* Represent parabola as a degenerate spline -
			   linear in x, quadratic in y */
			n->xp.a = 0;
			n->xp.b = 0;
			n->xp.c = t->dx;
			n->xp.d = -t->dx*t0 + t->x;
			n->yp.a = 0;
			n->yp.b = sp->Gr/2;
			n->yp.c = -sp->Gr*t0 + t->dy;
			n->yp.d = sp->Gr/2*t0*t0 - t->dy*t0 + t->y;


			/* ball follows parabola up */
			INSERT_AFTER(n, t->prev);
			n->start = (int) (t0 + tb);
			n->finish = (int) (t0 + tc);
			n->type = Ball;
			n->color = t->color;
			n->spin = t->spin;
			n->degree_offset = t->degree_offset;
			n->divisions = t->divisions;
			n->status = PREDICTOR;

			n->xp.a = 0;
			n->xp.b = 0;
			n->xp.c = t->dx;
			n->xp.d = -t->dx*t0 + t->x;

			dy = (yf - y0)/tb + 0.5*sp->Gr*tb;
			t0 = n->start;
			/* Represent parabola as a degenerate spline -
			   linear in x, quadratic in y */
			n->yp.a = 0;
			n->yp.b = sp->Gr/2;
			n->yp.c = -sp->Gr*t0 - COR*dy;
			n->yp.d = sp->Gr/2*t0*t0 + COR*dy*t0 + yf;
		  }

		  t->status = BPREDICTOR;

		} else {
		  double t0, dt;

		  /* ball follows parabola */
		  INSERT_AFTER(n, t->prev);
		  n->start = t->start;
		  n->finish = t->balllink->start;
		  n->type = Ball;
		  n->color = t->color;
		  n->spin = t->spin;
		  n->degree_offset = t->degree_offset;
	  	  n->divisions = t->divisions;
		  n->status = PREDICTOR;

		  t0 = n->start;
		  dt = t->balllink->start - t->start;
		  t->dx = (t->balllink->x - t->x) / dt;
		  t->dy = (t->balllink->y - t->y) / dt - sp->Gr/2 * dt;

		  /* Represent parabola as a degenerate spline -
			 linear in x, quadratic in y */
		  n->xp.a = 0;
		  n->xp.b = 0;
		  n->xp.c = t->dx;
		  n->xp.d = -t->dx*t0 + t->x;
		  n->yp.a = 0;
		  n->yp.b = sp->Gr/2;
		  n->yp.c = -sp->Gr*t0 + t->dy;
		  n->yp.d = sp->Gr/2*t0*t0 - t->dy*t0 + t->y;


		  t->status = BPREDICTOR;
		}
	  } else { /* Zero Throw */
		t->status = BPREDICTOR;
	  }
	}
  }
  return True;
}

/* Turn abstract hand motions into cubic splines. */
static void
hands(jugglestruct *sp)
{
  Trajectory *t, *u, *v;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	/* no throw => no velocity */
	if (t->status != BPREDICTOR) {
	  continue;
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

	(void) makeSplinePair(&t->xp, &u->xp,
						  t->x, t->dx, t->start,
						  u->x, u->start,
						  v->x, v->dx, v->start);
	(void) makeSplinePair(&t->yp, &u->yp,
						  t->y, t->dy, t->start,
						  u->y, u->start,
						  v->y, v->dy, v->start);

	t->status = PREDICTOR;
  }
}

/* Given target x, y find_elbow puts hand at target if possible,
 * otherwise makes hand point to the target */
static void
find_elbow(jugglestruct *sp, XPoint *h, XPoint *e, int x, int y, int z)
{
  double r, h2, t;

  h2 = x*x + y*y + z*z;
  if (h2 > 4*ARMLENGTH*ARMLENGTH) {
	t = ARMLENGTH/sqrt(h2);
	e->x = (short) (t*x);
	e->y = (short) (t*y);
	h->x = 2 * e->x;
	h->y = 2 * e->y;
  } else {
	r = sqrt((double)(x*x + z*z));
	t = sqrt(4 * ARMLENGTH * ARMLENGTH / h2 - 1);
	e->x = (short) (x*(1 - y*t/r)/2);
	e->y = (short) ((y + r*t)/2);
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

  XSetLineAttributes(dpy, gc,
		ARMWIDTH, LineSolid, CapRound, JoinRound);
  if (sp->arm[side][0].x != *x || sp->arm[side][0].y != *y) {
	XPoint h, e;
	XSetForeground(dpy, gc, MI_BLACK_PIXEL(mi));
	find_elbow(sp, &h, &e, *x - sig*SX - sp->cx, *y - SY - sp->cy, SZ);
	XDrawLines(dpy, win, gc, sp->arm[side], 3, CoordModeOrigin);
	*x = sp->arm[side][0].x = sp->cx + sig*SX + h.x;
	*y = sp->arm[side][0].y = sp->cy + SY + h.y;
	sp->arm[side][1].x = sp->cx + sig*SX + e.x;
	sp->arm[side][1].y = sp->cy + SY + e.y;
  }
  XSetForeground(dpy, gc, MI_WHITE_PIXEL(mi));
  XDrawLines(dpy, win, gc, sp->arm[side], 3, CoordModeOrigin);
  XSetLineAttributes(dpy, gc,
		1, LineSolid, CapNotLast, JoinRound);
}

static void
draw_figure(ModeInfo * mi)
{
  Display *dpy = MI_DISPLAY(mi);
  Window win = MI_WINDOW(mi);
  GC gc = MI_GC(mi);
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];

  XSetLineAttributes(dpy, gc,
		ARMWIDTH, LineSolid, CapRound, JoinRound);
  XSetForeground(dpy, gc, MI_WHITE_PIXEL(mi));
  XDrawLines(dpy, win, gc, sp->figure_path, FIGURE1, CoordModeOrigin);
  XDrawSegments(dpy, win, gc, sp->figure_segs, FIGURE2);
  XDrawArc(dpy, win, gc,
	 sp->cx - HED/2, sp->cy + NEY - HED, HED, HED, 0, 64*360);
  XSetLineAttributes(dpy, gc,
		1, LineSolid, CapNotLast, JoinRound);
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

static int get_num_balls(const char *j)
{
  int balls = 0;
  const char *p;
  int h = 0;
  for (p = j; *p; p++) {
	if (*p >= '0' && *p <='9') { /* digit */
	  h = 10*h + (*p - '0');
	} else {
	  if (h > balls) {
		balls = h;
	  }
	  h = 0;
	}
  }
  return balls;
}

#ifdef __cplusplus
extern "C" {
#endif

static int compare_num_balls(const void *p1, const void *p2)
{
  int i = get_num_balls(((patternstruct*)p1)->pattern);
  int j = get_num_balls(((patternstruct*)p2)->pattern);
  if (i > j) {
	return (1);
  } else if (i < j) {
	return (-1);
  } else {
	return (0);
  }
}

#ifdef __cplusplus
}
#endif

/* Public functions */

void
release_juggle(ModeInfo * mi)
{
	if (juggles != NULL) {
		int screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_juggle(&juggles[screen]);
		(void) free((void *) juggles);
		juggles = (jugglestruct *) NULL;
	}
	if (patternindex != NULL) {
		(void) free((void *) patternindex);
		patternindex = (PatternIndex *) NULL;
	}
}

void
init_juggle(ModeInfo * mi)
{
	jugglestruct *sp;
	int i;
	XPoint figure1[FIGURE1];
	XSegment figure2[FIGURE2];
	if (pattern != NULL && *pattern == '.') {
	  pattern = NULL;
	}
	if (pattern == NULL && patternindex == NULL) {
	  /* pattern list needs indexing */
	  int i;
	  int nelements = sizeof(portfolio)/sizeof(patternstruct);
	  int maxballs;
	  int numpat = 0;

	  /* sort according to number of balls */
	  qsort((void*)portfolio, nelements,
			sizeof(patternstruct), compare_num_balls);

	  /* last pattern has most balls */
	  maxballs = get_num_balls(portfolio[nelements - 1].pattern);
	  /* allocate index */
	  if ((patternindex = (PatternIndex *) calloc(maxballs + 1,
				sizeof (PatternIndex))) == NULL) {
		return;
	  }

	  /* run through sorted list, indexing start of each group
		 and number in group */
	  maxballs = 1;
	  for (i = 0; i < nelements; i++) {
		int b = get_num_balls(portfolio[i].pattern);
		if (b > maxballs) {
		  if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "%d %d ball pattern%s\n",
					numpat, maxballs, (numpat == 1) ? "" : "s");
		  }
		  patternindex[maxballs].number = numpat;
		  maxballs = b;
		  numpat = 1;
		  patternindex[maxballs].start = i;
		} else {
		  numpat++;
		}
	  }
	  if (MI_IS_VERBOSE(mi)) {
		(void) fprintf(stderr, "%d %d ball pattern%s\n",
				numpat, maxballs, (numpat == 1) ? "" : "s");
	  }
	  patternindex[maxballs].number = numpat;
	}

	if (juggles == NULL) { /* allocate jugglestruct */
		if ((juggles = (jugglestruct *) calloc(MI_NUM_SCREENS(mi),
			       sizeof (jugglestruct))) == NULL) {
			release_juggle(mi);
			return;
		}
	}
	sp = &juggles[MI_SCREEN(mi)];

	sp->count = 0;

	if (MI_IS_FULLRANDOM(mi)) {
		sp->solid = (Bool) (LRAND() & 1);
#ifdef UNI
		sp->uni = (Bool) (LRAND() & 1);
#endif
	} else {
		sp->solid = solid;
#ifdef UNI
		sp->uni = uni;
#endif
	}

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);
	sp->count = ABS(MI_COUNT(mi));
	if (sp->count == 0)
		sp->count = 150;
	sp->scale = sp->height / 480.0;
	/* vary x a little so the juggler does not burn the screen */
	sp->cx = sp->width / 2 + RFX + NRAND(LFX - RFX + 1);
	sp->cy = sp->height - FY - ((int) sp->uni) * FY / 3; /* raise higher */
	/* "7" hits top of screen */
	sp->Gr = GRAVITY(sp->cy, 7 * THROW_CATCH_INTERVAL);

	figure1[0].x = LHIPX, figure1[0].y = HIPY;
	figure1[1].x = 0, figure1[1].y = WSTY;
	figure1[2].x = SX, figure1[2].y = SY;
	figure1[3].x = -SX, figure1[3].y = SY;
	figure1[4].x = 0, figure1[4].y = WSTY;
	figure1[5].x = RHIPX, figure1[5].y = HIPY;
	figure1[6].x = LHIPX, figure1[6].y = HIPY;
	figure2[0].x1 = 0, figure2[0].y1 = SY,
	  figure2[0].x2 = 0, figure2[0].y2 = NEY;
	figure2[1].x1 = LHIPX, figure2[1].y1 = HIPY,
	  figure2[1].x2 = LFX, figure2[1].y2 = FY;
	figure2[2].x1 = RHIPX, figure2[2].y1 = HIPY,
	  figure2[2].x2 = RFX, figure2[2].y2 = FY;

	/* Body Path */
	for (i = 0; i <  FIGURE1; i++) {
	  sp->figure_path[i].x = figure1[i].x + sp->cx;
	  sp->figure_path[i].y = figure1[i].y + sp->cy;
	}
	/* Body Segments */
	for (i = 0; i < FIGURE2; i++) {
	  sp->figure_segs[i].x1 = figure2[i].x1 + sp->cx;
	  sp->figure_segs[i].y1 = figure2[i].y1 + sp->cy;
	  sp->figure_segs[i].x2 = figure2[i].x2 + sp->cx;
	  sp->figure_segs[i].y2 = figure2[i].y2 + sp->cy;
	}
	/* Shoulders */
	sp->arm[LEFT][2].x = sp->cx + SX;
	sp->arm[LEFT][2].y = sp->cy + SY;
	sp->arm[RIGHT][2].x = sp->cx - SX;
	sp->arm[RIGHT][2].y = sp->cy + SY;

	if (sp->trace == NULL) {
	  if ((sp->trace = (XPoint *)calloc(trail, sizeof(XPoint))) == NULL) {
		free_juggle(sp);
 		return;
	  }
	}

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	draw_figure(mi);

	/* record start time */
	sp->begintime = time(NULL);

	free_pattern(sp);

	/* create circular list */
	INSERT_AFTER_TOP(sp->head, sp->head);

	/* generate pattern */
	if (pattern == NULL) {

#define MAXPAT 10
#define MAXREPEAT 30
#define CHANGE_BIAS 8 /* larger makes num_ball changes less likely */
#define POSITION_BIAS 20 /* larger makes hand movements less likely */

	  int count = 0;
	  int num_balls = MINBALLS + NRAND(MAXBALLS - MINBALLS);
	  while (count < MI_CYCLES(mi)) {
		char buf[MAXPAT * 3 + 3], *b = buf;
		int maxseen = 0;
		int l = NRAND(MAXPAT) + 1;
		int t = NRAND(MAXREPEAT) + 1;

		{ /* vary number of balls */
		  int new_balls = num_balls;
		  int change;

		  if (new_balls == 2) /* Do not juggle 2 that often */
		    change = NRAND(2 + CHANGE_BIAS / 4);
		  else
		    change = NRAND(2 + CHANGE_BIAS);
		  switch (change) {
		  case 0:
			new_balls++;
			break;
		  case 1:
			new_balls--;
			break;
		  default:
			break; /* NO-OP */
		  }
		  if (new_balls < MINBALLS) {
			new_balls += 2;
		  }
		  if (new_balls > MAXBALLS) {
			new_balls -= 2;
		  }
		  if (new_balls < num_balls) {
			if (!program(mi, "[*]", 1)) /* lose ball */
				return;
		  }
		  num_balls = new_balls;
		}
		count++;

		if (NRAND(2) && patternindex[num_balls].number) {
		  /* Pick from PortFolio */
		  if (!program(mi,
			  portfolio[patternindex[num_balls].start +
				  NRAND(patternindex[num_balls].number)].pattern,
				  t))
			return;
		} else {
		  /* Invent a new pattern */
		  *b++='[';
		  for(i = 0; i < l; i++){
			int n, m;
			do { /* Triangular Distribution => high values more likely */
			  m = NRAND(num_balls + 1);
			  n = NRAND(num_balls + 1);
			} while(m >= n);
			if (n == num_balls) {
			  maxseen = 1;
			}
			switch(NRAND(6 + POSITION_BIAS)){
			case 0:            /* Inside throw */
			  *b++ = '-'; break;
			case 1:            /* Outside throw */
			  *b++ = '+'; break;
			case 2:            /* Cross throw */
			  *b++ = '='; break;
			case 3:            /* Cross catch */
			  *b++ = '&'; break;
			case 4:            /* Cross throw and catch */
			  *b++ = 'x'; break;
			case 5:            /* Bounce */
			  *b++ = '_'; break;
			default:
			  break; /* NO-OP */
			}

			*b++ = n + '0';
			*b++ = ' ';
		  }
		  *b++ = ']';
		  *b = '\0';
		  if (maxseen) {
			if (!program(mi, buf, t))
				return;
		  }
		}
	  }
	} else { /* pattern supplied in height or 'a' notation */
	  if (!program(mi, pattern, MI_CYCLES(mi)))
		return;
	}

	adam(sp);

	if (!part(sp))
		return;

 	lob(mi);

	positions(sp);

	if (!projectile(sp))
		return;

	hands(sp);

#ifdef OLDDEBUG
	dump(sp);
#endif
}

#define CUBIC(s, t) ((((s).a * (t) + (s).b) * (t) + (s).c) * (t) + (s).d)

#ifdef SUNOS4
/*-
 * Workaround SunOS 4 framebuffer bug which causes balls to leave dust
 * trace behind when erased
 */
#define ERASE_BALL(x,y) \
	XSetForeground(dpy, gc, MI_BLACK_PIXEL(mi)); \
	XFillArc(dpy, window, gc, \
		(x) - BALLRADIUS - 2, (y) - BALLRADIUS - 2, \
		2*(BALLRADIUS + 2), 2*(BALLRADIUS + 2), 0, 23040)
#else

#define ERASE_BALL(x,y) \
	XSetForeground(dpy, gc, MI_BLACK_PIXEL(mi)); \
	XFillArc(dpy, window, gc, \
		(x) - BALLRADIUS, (y) - BALLRADIUS, \
		2*BALLRADIUS, 2*BALLRADIUS, 0, 23040)
#endif

static void
draw_juggle_ball(ModeInfo *mi, unsigned long color, int x, int y, double degree_offset, int divisions)
{
	Display    *dpy = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	jugglestruct *sp = &juggles[MI_SCREEN(mi)];
	int offset;

	XSetForeground(dpy, gc, color);
	if ((color == MI_WHITE_PIXEL(mi)) ||
	    ((divisions != 2) && (divisions != 4)) || sp->solid) {
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			0, 23040);
		return;
	}
	offset = (int) (degree_offset * 64);
	if (divisions == 4) { /* 90 degree divisions */
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			offset, 5760);
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			(offset + 11520) % 23040, 5760);
		XSetForeground(dpy, gc, MI_WHITE_PIXEL(mi));
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			(offset + 5760) % 23040, 5760);
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			(offset + 17280) % 23040, 5760);
	} else { /* 180 degree divisions */
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			offset, 11520);
		XSetForeground(dpy, gc, MI_WHITE_PIXEL(mi));
		XFillArc(dpy, window, gc,
			x - BALLRADIUS, y - BALLRADIUS,
			2*BALLRADIUS, 2*BALLRADIUS,
			(offset + 11520) % 23040, 11520);
	}
	XFlush(dpy);
}

void
draw_juggle(ModeInfo * mi)
{
	Display    *dpy = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	Trajectory *traj;
	int future = 0;
	int length = 0;
	jugglestruct *sp;

	if (juggles == NULL)
		return;
	sp = &juggles[MI_SCREEN(mi)];
	if (sp->trace == NULL)
		return;

	MI_IS_DRAWN(mi) = True;

	draw_figure(mi);

	{
	  struct timeval tv;
	  (void)gettimeofday(&tv, NULL);
	  sp->time = (int) ((tv.tv_sec - sp->begintime)*1000 + tv.tv_usec/1000);
	}
	for (traj = sp->head->next; traj != sp->head; traj = traj->next) {
	  length++;
	  if (traj->status != PREDICTOR) {
 		continue;
 	  }
	  if (traj->start > future) {
		future = traj->start;
	  }
	  if (sp->time < traj->start) { /* early */
		continue;
	  } else if (sp->time < traj->finish) { /* working */
		int x = (int) CUBIC(traj->xp, sp->time);
		int y = (int) CUBIC(traj->yp, sp->time);
		unsigned long color;

		if (MI_NPIXELS(mi) > 2) {
		  color = MI_PIXEL(mi, traj->color);
		} else {
		  color = MI_WHITE_PIXEL(mi);
		}
		if (traj->type == Empty || traj->type == Full) {
		  draw_arm(mi, traj->hand, &x, &y);
		}
		if (traj->type == Ball || traj->type == Full) {
		  if(trail > 0) {
			ERASE_BALL(sp->trace[sp->traceindex].x,
				sp->trace[sp->traceindex].y);
			sp->trace[sp->traceindex].x = traj->x;
			sp->trace[sp->traceindex].y = traj->y;
			if (++sp->traceindex >= trail) {
			  sp->traceindex = 0;
			}
		  } else {
			ERASE_BALL(traj->x, traj->y);
		  }
		  draw_juggle_ball(mi, color, x, y, traj->degree_offset, traj->divisions);
		  traj->degree_offset = traj->degree_offset +
		    SPIN_DEGREES * traj->spin / sp->count;
		  if (traj->degree_offset < 0.0)
			traj->degree_offset += 360.0;
		  else if (traj->degree_offset >= 360.0)
			traj->degree_offset -= 360.0;
		}
		traj->x = x;
		traj->y = y;
	  } else { /* expired */
		Trajectory *n = traj;

		ERASE_BALL(traj->x, traj->y);
		traj=traj->prev;
		REMOVE(n);
	  }
	}

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

#endif /* MODE_juggle */
