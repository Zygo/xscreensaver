/* juggle, Copyright (c) 1996-2009 Tim Auckland <tda10.geo@yahoo.com>
 * and Jamie Zawinski <jwz@jwz.org>
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
 * NOTE: this program was originally called "juggle" and was 2D Xlib.
 *       There was another program called "juggler3d" that was OpenGL.
 *       In 2009, jwz converted "juggle" to OpenGL and renamed
 *       "juggle" to "juggler3d".  The old "juggler3d" hack is gone.
 *
 * Revision History
 * 09-Aug-2009: jwz: converted from Xlib to OpenGL.
 * 13-Dec-2004: [TDA] Use -cycles and -count in a rational manner.
 *              Add -rings, -bballs.  Add -describe.  Finally made
 *              live pattern updates possible.  Add refill_juggle(),
 *              change_juggle() and reshape_juggle().  Make
 *              init_juggle() non-destructive.  Reorder erase/draw
 *              operations.  Update xscreensaver xml and manpage.
 * 15-Nov-2004: [TDA] Fix all memory leaks.
 * 12-Nov-2004: [TDA] Add -torches and another new trail
 *              implementation, so that different objects can have
 *              different length trails.
 * 11-Nov-2004: [TDA] Clap when all the balls are in the air.
 * 10-Nov-2004: [TDA] Display pattern name converted to hight
 *              notation.
 * 31-Oct-2004: [TDA] Add -clubs and new trail implementation.
 * 02-Sep-2003: Non-real time to see what is happening without a
 *              strobe effect for slow machines.
 * 01-Nov-2000: Allocation checks
 * 1996: Written
 */

/*-
 * TODO
 * Implement the anonymously promised -uni option.
 */


/*
 * Notes on Adam Chalcraft Juggling Notation (used by permission)
 * a-> Adam's notation  s-> Site swap (Cambridge) notation
 *
 * To define a map from a-notation to s-notation ("site-swap"), both
 * of which look like doubly infinite sequences of natural numbers. In
 * s-notation, there is a restriction on what is allowed, namely for
 * the sequence s_n, the associated function f(n)=n+s_n must be a
 * bijection. In a-notation, there is no restriction.
 *
 * To go from a-notation to s-notation, you start by mapping each a_n
 * to a permutation of N, the natural numbers.
 *
 * 0 -> the identity
 * 1 -> (10) [i.e. f(1)=0, f(0)=1]
 * 2 -> (210) [i.e. f(2)=1, f(1)=0, f(0)=2]
 * 3 -> (3210) [i.e. f(3)=2, f(2)=1, f(1)=0, f(0)=3]
 * etc.
 *
 * Then for each n, you look at how long 0 takes to get back to 0
 * again and you call this t_n. If a_n=0, for example, then since the
 * identity leaves 0 alone, it gets back to 0 in 1 step, so t_n=1. If
 * a_n=1, then f(0)=1. Now any further a_n=0 leave 1 alone, but the
 * next a_n>0 sends 1 back to 0. Hence t_n is 2 + the number of 0's
 * following the 1. Finally, set s_n = t_n - 1.
 *
 * To give some examples, it helps to have a notation for cyclic
 * sequences. By (123), for example, I mean ...123123123123... . Now
 * under the a-notation -> s-notation mapping we have some familiar
 * examples:
 *
 * (0)->(0), (1)->(1), (2)->(2) etc.
 * (21)->(31), (31)->(51), (41)->(71) etc.
 * (10)->(20), (20)->(40), (30)->(60) etc.
 * (331)->(441), (312)->(612), (303)->(504), (321)->(531)
 * (43)->(53), (434)->(534), (433)->(633)
 * (552)->(672)
 *
 * In general, the number of balls is the *average* of the s-notation,
 * and the *maximum* of the a-notation. Another theorem is that the
 * minimum values in the a-notation and the s-notation and equal, and
 * preserved in the same positions.
 *
 * The usefulness of a-notation is the fact that there are no
 * restrictions on what is allowed. This makes random juggle
 * generation much easier. It also makes enumeration very
 * easy. Another handy feature is computing changes.  Suppose you can
 * do (5) and want a neat change up to (771) in s-notation [Mike Day
 * actually needed this example!]. Write them both in a-notation,
 * which gives (5) and (551). Now concatenate them (in general, there
 * may be more than one way to do this, but not in this example), to
 * get
 *
 * ...55555555551551551551551...
 *
 * Now convert back to s-notation, to get
 *
 * ...55555566771771771771771...
 *
 * So the answer is to do two 6 throws and then go straight into
 * (771).  Coming back down of course,
 *
 * ...5515515515515515555555555...
 *
 * converts to
 *
 * ...7717717717716615555555555...
 *
 * so the answer is to do a single 661 and then drop straight down to
 * (5).
 *
 * [The number of balls in the generated pattern occasionally changes.
 * In order to decrease the number of balls I had to introduce a new
 * symbol into the Adam notation, [*] which means 'lose the current
 * ball'.]
 */

/* This code uses so many linked lists it's worth having a built-in
 * leak-checker */
#undef MEMTEST

# define DEFAULTS	"*delay:	10000	\n" \
			"*count:	200	\n" \
			"*cycles:	1000	\n" \
			"*ncolors:	32	\n" \
             "*titleFont:  -*-helvetica-bold-r-normal-*-*-180-*-*-*-*-*-*\n" \
			"*showFPS:	False	\n" \
			"*wireframe:	False	\n" \

# define release_juggle 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "sphere.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include "texfont.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_PATTERN "random" /* All patterns */
#define DEF_TAIL "1" /* No trace */
#ifdef UNI
/* Maybe a ROLA BOLA would be at a better angle for viewing */
#define DEF_UNI "False" /* No unicycle */ /* Not implemented yet */
#endif
#define DEF_REAL "True"
#define DEF_DESCRIBE "True"

#define DEF_BALLS "True" /* Use Balls */
#define DEF_CLUBS "True" /* Use Clubs */
#define DEF_TORCHES "True" /* Use Torches */
#define DEF_KNIVES "True" /* Use Knives */
#define DEF_RINGS "True" /* Use Rings */
#define DEF_BBALLS "True" /* Use Bowling Balls */

static char *pattern;
static int tail;
#ifdef UNI
static Bool uni;
#endif
static Bool real;
static Bool describe;
static Bool balls;
static Bool clubs;
static Bool torches;
static Bool knives;
static Bool rings;
static Bool bballs;
static char *only;

static XrmOptionDescRec opts[] = {
  {"-pattern",  ".juggle.pattern",  XrmoptionSepArg, NULL  },
  {"-tail",     ".juggle.tail",     XrmoptionSepArg, NULL  },
#ifdef UNI
  {"-uni",      ".juggle.uni",      XrmoptionNoArg,  "on"  },
  {"+uni",      ".juggle.uni",      XrmoptionNoArg,  "off" },
#endif
  {"-real",     ".juggle.real",     XrmoptionNoArg,  "on"  },
  {"+real",     ".juggle.real",     XrmoptionNoArg,  "off" },
  {"-describe", ".juggle.describe", XrmoptionNoArg,  "on"  },
  {"+describe", ".juggle.describe", XrmoptionNoArg,  "off" },
  {"-balls",    ".juggle.balls",    XrmoptionNoArg,  "on"  },
  {"+balls",    ".juggle.balls",    XrmoptionNoArg,  "off" },
  {"-clubs",    ".juggle.clubs",    XrmoptionNoArg,  "on"  },
  {"+clubs",    ".juggle.clubs",    XrmoptionNoArg,  "off" },
  {"-torches",  ".juggle.torches",  XrmoptionNoArg,  "on"  },
  {"+torches",  ".juggle.torches",  XrmoptionNoArg,  "off" },
  {"-knives",   ".juggle.knives",   XrmoptionNoArg,  "on"  },
  {"+knives",   ".juggle.knives",   XrmoptionNoArg,  "off" },
  {"-rings",    ".juggle.rings",    XrmoptionNoArg,  "on"  },
  {"+rings",    ".juggle.rings",    XrmoptionNoArg,  "off" },
  {"-bballs",   ".juggle.bballs",   XrmoptionNoArg,  "on"  },
  {"+bballs",   ".juggle.bballs",   XrmoptionNoArg,  "off" },
  {"-only",     ".juggle.only",     XrmoptionSepArg, NULL  },
};

static argtype vars[] = {
  { &pattern,  "pattern",  "Pattern",  DEF_PATTERN,  t_String },
  { &tail,     "tail",     "Tail",     DEF_TAIL,     t_Int    },
#ifdef UNI
  { &uni,      "uni",      "Uni",      DEF_UNI,      t_Bool   },
#endif
  { &real,     "real",     "Real",     DEF_REAL,     t_Bool   },
  { &describe, "describe", "Describe", DEF_DESCRIBE, t_Bool   },
  { &balls,    "balls",    "Clubs",    DEF_BALLS,    t_Bool   },
  { &clubs,    "clubs",    "Clubs",    DEF_CLUBS,    t_Bool   },
  { &torches,  "torches",  "Torches",  DEF_TORCHES,  t_Bool   },
  { &knives,   "knives",   "Knives",   DEF_KNIVES,   t_Bool   },
  { &rings,    "rings",    "Rings",    DEF_RINGS,    t_Bool   },
  { &bballs,   "bballs",   "BBalls",   DEF_BBALLS,   t_Bool   },
  { &only,     "only",     "BBalls",   " ",          t_String },
};

static OptionStruct desc[] =
{
  { "-pattern string", "Cambridge Juggling Pattern" },
  { "-tail num",       "Trace Juggling Patterns" },
#ifdef UNI
  { "-/+uni",          "Unicycle" },
#endif
  { "-/+real",         "Real-time" },
  { "-/+describe",     "turn on/off pattern descriptions." },
  { "-/+balls",        "turn on/off Balls." },
  { "-/+clubs",        "turn on/off Clubs." },
  { "-/+torches",      "turn on/off Flaming Torches." },
  { "-/+knives",       "turn on/off Knives." },
  { "-/+rings",        "turn on/off Rings." },
  { "-/+bballs",       "turn on/off Bowling Balls." },
  { "-only",           "Turn off all objects but the named one." },
};

ENTRYPOINT ModeSpecOpt juggle_opts =
 {countof(opts), opts, countof(vars), vars, desc};


/* Note: All "lengths" are scaled by sp->scale = MI_HEIGHT/480.  All
   "thicknesses" are scaled by sqrt(sp->scale) so that they are
   proportionally thicker for smaller windows.  Objects spinning out
   of the plane (such as clubs) fake perspective by compressing their
   horizontal coordinates by PERSPEC  */

/* Figure */
#define ARMLENGTH 50
#define ARMWIDTH ((int) (8.0 * sqrt(sp->scale)))
#define POSE 10
#define BALLRADIUS ARMWIDTH

/* build all the models assuming a 480px high scene */
#define SCENE_HEIGHT 480
#define SCENE_WIDTH ((int)(SCENE_HEIGHT*(MI_WIDTH(mi)/(float)MI_HEIGHT(mi))))

/*#define PERSPEC  0.4*/

/* macros */
#define GRAVITY(h, t) 4*(double)(h)/((t)*(t))

/* Timing based on count.  Units are milliseconds.  Juggles per second
       is: 2000 / THROW_CATCH_INTERVAL + CATCH_THROW_INTERVAL */

#define THROW_CATCH_INTERVAL (sp->count)
#define THROW_NULL_INTERVAL  (sp->count * 0.5)
#define CATCH_THROW_INTERVAL (sp->count * 0.2)

/********************************************************************
 * Trace Definitions                                                *
 *                                                                  *
 * These record rendering data so that a drawn object can be erased *
 * later.  Each object has its own Trace list.                      *
 *                                                                  *
 ********************************************************************/

typedef struct {double x, y; } DXPoint;
typedef struct trace *TracePtr;
typedef struct trace {
  TracePtr next, prev;
  double x, y;
  double angle;
  int divisions;
  DXPoint dlast;
#ifdef MEMTEST
  char pad[1024];
#endif
} Trace;

/*******************************************************************
 * Object Definitions                                              *
 *                                                                 *
 * These describe the various types of Object that can be juggled  *
 *                                                                 *
 *******************************************************************/
typedef int (DrawProc)(ModeInfo*, unsigned long, Trace *);

static DrawProc show_ball, show_europeanclub, show_torch, show_knife;
static DrawProc show_ring, show_bball;

typedef enum {BALL, CLUB, TORCH, KNIFE, RING, BBALLS,
              NUM_OBJECT_TYPES} ObjType;

#define OBJMIXPROB 20   /* inverse of the chances of using an odd
						   object in the pattern */

static const GLfloat body_color_1[4] = { 0.9, 0.7, 0.5, 1 };
static const GLfloat body_color_2[4] = { 0.6, 0.4, 0.2, 1 };

static const struct {
  DrawProc *draw;                           /* Object Rendering function */
  int       handle;                         /* Length of object's handle */
  int       mintrail;                            /* Minimum trail length */
  double    cor;      /* Coefficient of Restitution.  perfect bounce = 1 */
  double    weight;          /* Heavier objects don't get thrown as high */
} ObjectDefs[] = {
  { /* Ball */
	show_ball,
	0,
	1,
	0.9,
	1.0,
  },
  { /* Club */
	show_europeanclub,
	15,
	1,
	0.55, /* Clubs don't bounce too well */
	1.0,
  },
  { /* Torch */
	show_torch,
	15,
	20, /* Torches need flames */
	0, /* Torches don't bounce -- fire risk! */
	1.0,
  },
  { /* Knife */
	show_knife,
	15,
	1,
	0, /* Knives don't bounce */
	1.0,
  },
  { /* Ring */
	show_ring,
	15,
	1,
	0.8,
	1.0,
  },
  { /* Bowling Ball */
	show_bball,
	0,
	1,
	0.2,
	5.0,
  },
};

/**************************
 * Trajectory definitions *
 **************************/

typedef enum {HEIGHT, ADAM} Notation;
typedef enum {Empty, Full, Ball} Throwable;
typedef enum {LEFT, RIGHT} Hand;
typedef enum {THROW, CATCH} Action;
typedef enum {HAND, ELBOW, SHOULDER} Joint;
typedef enum {ATCH, THRATCH, ACTION, LINKEDACTION,
			  PTHRATCH, BPREDICTOR, PREDICTOR} TrajectoryStatus;
typedef struct {double a, b, c, d; } Spline;
typedef DXPoint Arm[3];


/* Object is an arbitrary object being juggled.  Each Trajectory
 * references an Object ("count" tracks this), and each Object is also
 * linked into a global Objects list.  Objects may include a Trace
 * list for tracking erasures. */
typedef struct object *ObjectPtr;
typedef struct object {
  ObjectPtr next, prev;

  ObjType type;
  int     color;
  int     count; /* reference count */
  Bool    active; /* Object is in use */

  Trace  *trace;
  int     tracelen;
  int     tail;
#ifdef MEMTEST
  char pad[1024];
#endif
} Object;

/* Trajectory is a segment of juggling action.  A list of Trajectories
 * defines the juggling performance.  The Trajectory list goes through
 * multiple processing steps to convert it from basic juggling
 * notation into rendering data. */

typedef struct trajectory *TrajectoryPtr;
typedef struct trajectory {
  TrajectoryPtr prev, next;  /* for building list */
  TrajectoryStatus status;

  /* Throw */
  char posn;
  int height;
  int adam;
  char *pattern;
  char *name;

  /* Action */
  Hand hand;
  Action action;

  /* LinkedAction */
  int color;
  Object *object;
  int divisions;
  double angle, spin;
  TrajectoryPtr balllink;
  TrajectoryPtr handlink;

  /* PThratch */
  double cx; /* Moving juggler */
  double x, y; /* current position */
  double dx, dy; /* initial velocity */

  /* Predictor */
  Throwable type;
  unsigned long start, finish;
  Spline xp, yp;

#ifdef MEMTEST
  char pad[1024];
#endif
} Trajectory;


/*******************
 * Pattern Library *
 *******************/

typedef struct {
  const char * pattern;
  const char * name;
} patternstruct;

/* List of popular patterns, in any order */
/* Patterns should be given in Adam notation so the generator can
   concatenate them safely.  Null descriptions are ok.  Height
   notation will be displayed automatically.  */
/* Can't const this because it is qsorted.  This *should* be reentrant,
   I think... */
static /*const*/ patternstruct portfolio[] = {
  {"[+2 1]", /* +3 1 */ "Typical 2 ball juggler"},
  {"[2 0]", /* 4 0 */ "2 in 1 hand"},
  {"[2 0 1]", /* 5 0 1 */},
  {"[+2 0 +2 0 0]" /* +5 0 +5 0 0 */},
  {"[+2 0 1 2 2]", /* +4 0 1 2 3 */},
  {"[2 0 1 1]", /* 6 0 1 1 */},

  {"[3]", /* 3 */ "3 cascade"},
  {"[+3]", /* +3 */ "reverse 3 cascade"},
  {"[=3]", /* =3 */ "cascade 3 under arm"},
  {"[&3]", /* &3 */ "cascade 3 catching under arm"},
  {"[_3]", /* _3 */ "bouncing 3 cascade"},
  {"[+3 x3 =3]", /* +3 x3 =3 */ "Mill's mess"},
  {"[3 2 1]", /* 5 3 1" */},
  {"[3 3 1]", /* 4 4 1" */},
  {"[3 1 2]", /* 6 1 2 */ "See-saw"},
  {"[=3 3 1 2]", /* =4 5 1 2 */},
  {"[=3 2 2 3 1 2]", /* =6 2 2 5 1 2 */ "=4 5 1 2 stretched"},
  {"[+3 3 1 3]", /* +4 4 1 3 */ "anemic shower box"},
  {"[3 3 1]", /* 4 4 1 */},
  {"[+3 2 3]", /* +4 2 3 */},
  {"[+3 1]", /* +5 1 */ "3 shower"},
  {"[_3 1]", /* _5 1 */ "bouncing 3 shower"},
  {"[3 0 3 0 3]", /* 5 0 5 0 5 */ "shake 3 out of 5"},
  {"[3 3 3 0 0]", /* 5 5 5 0 0 */ "flash 3 out of 5"},
  {"[3 3 0]", /* 4 5 0 */ "complete waste of a 5 ball juggler"},
  {"[3 3 3 0 0 0 0]", /* 7 7 7 0 0 0 0 */ "3 flash"},
  {"[+3 0 +3 0 +3 0 0]", /* +7 0 +7 0 +7 0 0 */},
  {"[3 2 2 0 3 2 0 2 3 0 2 2 0]", /* 7 3 3 0 7 3 0 3 7 0 3 3 0 */},
  {"[3 0 2 0]", /* 8 0 4 0 */},
  {"[_3 2 1]", /* _5 3 1 */},
  {"[_3 0 1]", /* _8 0 1 */},
  {"[1 _3 1 _3 0 1 _3 0]", /* 1 _7 1 _7 0 1 _7 0 */},
  {"[_3 2 1 _3 1 2 1]", /* _6 3 1 _6 1 3 1 */},

  {"[4]", /* 4 */ "4 cascade"},
  {"[+4 3]", /* +5 3 */ "4 ball half shower"},
  {"[4 4 2]", /* 5 5 2 */},
  {"[+4 4 4 +4]", /* +4 4 4 +4 */ "4 columns"},
  {"[+4 3 +4]", /* +5 3 +4 */},
  {"[4 3 4 4]", /* 5 3 4 4 */},
  {"[4 3 3 4]", /* 6 3 3 4 */},
  {"[4 3 2 4", /* 6 4 2 4 */},
  {"[+4 1]", /* +7 1 */ "4 shower"},
  {"[4 4 4 4 0]", /* 5 5 5 5 0 */ "learning 5"},
  {"[+4 x4 =4]", /* +4 x4 =4 */ "Mill's mess for 4"},
  {"[+4 2 1 3]", /* +9 3 1 3 */},
  {"[4 4 1 4 1 4]", /* 6 6 1 5 1 5, by Allen Knutson */},
  {"[_4 _4 _4 1 _4 1]", /* _5 _6 _6 1 _5 1 */},
  {"[_4 3 3]", /* _6 3 3 */},
  {"[_4 3 1]", /* _7 4 1 */},
  {"[_4 2 1]", /* _8 3 1 */},
  {"[_4 3 3 3 0]", /* _8 4 4 4 0 */},
  {"[_4 1 3 1]", /* _9 1 5 1 */},
  {"[_4 1 3 1 2]", /* _10 1 6 1 2 */},

  {"[5]", /* 5 */ "5 cascade"},
  {"[_5 _5 _5 _5 _5 5 5 5 5 5]", /* _5 _5 _5 _5 _5 5 5 5 5 5 */},
  {"[+5 x5 =5]", /* +5 x5 =5 */ "Mill's mess for 5"},
  {"[5 4 4]", /* 7 4 4 */},
  {"[_5 4 4]", /* _7 4 4 */},
  {"[1 2 3 4 5 5 5 5 5]", /* 1 2 3 4 5 6 7 8 9 */ "5 ramp"},
  {"[5 4 5 3 1]", /* 8 5 7 4 1, by Allen Knutson */},
  {"[_5 4 1 +4]", /* _9 5 1 5 */},
  {"[_5 4 +4 +4]", /* _8 4 +4 +4 */},
  {"[_5 4 4 4 1]", /* _9 5 5 5 1 */},
  {"[_5 4 4 5 1]",},
  {"[_5 4 4 +4 4 0]", /*_10 5 5 +5 5 0 */},

  {"[6]", /* 6 */ "6 cascade"},
  {"[+6 5]", /* +7 5 */},
  {"[6 4]", /* 8 4 */},
  {"[+6 3]", /* +9 3 */},
  {"[6 5 4 4]", /* 9 7 4 4 */},
  {"[+6 5 5 5]", /* +9 5 5 5 */},
  {"[6 0 6]", /* 9 0 9 */},
  {"[_6 0 _6]", /* _9 0 _9 */},

  {"[_7]", /* _7 */ "bouncing 7 cascade"},
  {"[7]", /* 7 */ "7 cascade"},
  {"[7 6 6 6 6]", /* 11 6 6 6 6 */ "Gatto's High Throw"},

};



typedef struct { int start; int number; } PatternIndex;

struct patternindex {
  int minballs;
  int maxballs;
  PatternIndex index[countof(portfolio)];
};


/* Jugglestruct: per-screen global data.  The master Object
 * and Trajectory lists are anchored here. */
typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  double        scale;
  double        cx;
  double        Gr;
  Trajectory   *head;
  Arm           arm[2][2];
  char         *pattern;
  int           count;
  int           num_balls;
  time_t        begintime; /* should make 'time' usable for at least 48 days
							on a 32-bit machine */
  unsigned long time; /* millisecond timer*/
  ObjType       objtypes;
  Object       *objects;
  struct patternindex patternindex;
  texture_font_data *font_data;
} jugglestruct;

static jugglestruct *juggles = (jugglestruct *) NULL;

/*******************
 * list management *
 *******************/

#define DUP_OBJECT(n, t) { \
  (n)->object = (t)->object; \
  if((n)->object != NULL) (n)->object->count++; \
}

/* t must point to an existing element.  t must not be an
   expression ending ->next or ->prev */
#define REMOVE(t) { \
  (t)->next->prev = (t)->prev; \
  (t)->prev->next = (t)->next; \
  free(t); \
}

/* t receives element to be created and added to the list.  ot must
   point to an existing element or be identical to t to start a new
   list. Applicable to Trajectories, Objects and Traces. */
#define ADD_ELEMENT(type, t, ot) \
  if (((t) = (type*)calloc(1,sizeof(type))) != NULL) { \
    (t)->next = (ot)->next; \
    (t)->prev = (ot); \
    (ot)->next = (t); \
    (t)->next->prev = (t); \
  }

static void
object_destroy(Object* o)
{
  if(o->trace != NULL) {
	while(o->trace->next != o->trace) {
	  Trace *s = o->trace->next;
	  REMOVE(s); /* Don't eliminate 's' */
	}
	free(o->trace);
  }
  REMOVE(o);
}

static void
trajectory_destroy(Trajectory *t) {
  if(t->name != NULL) free(t->name);
  if(t->pattern != NULL) free(t->pattern);
  /* Reduce object link count and call destructor if necessary */
  if(t->object != NULL && --t->object->count < 1 && t->object->tracelen == 0) {
	object_destroy(t->object);
  }
  REMOVE(t); /* Unlink and free */
}

ENTRYPOINT void
free_juggle(ModeInfo *mi) {
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];

  if (!sp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *sp->glx_context);

  if (sp->trackball) gltrackball_free (sp->trackball);
  if (sp->rot) free_rotator (sp->rot);
  if (sp->font_data) free_texture_font (sp->font_data);

  if (sp->head != NULL) {
	while (sp->head->next != sp->head) {
	  trajectory_destroy(sp->head->next);
	}
	free(sp->head);
	sp->head = (Trajectory *) NULL;
  }
  if(sp->objects != NULL) {
	while (sp->objects->next != sp->objects) {
	  object_destroy(sp->objects->next);
	}
	free(sp->objects);
	sp->objects = (Object*)NULL;
  }
  if(sp->pattern != NULL) {
	free(sp->pattern);
	sp->pattern = NULL;
  }
}

static Bool
add_throw(ModeInfo *mi, char type, int h, Notation n, const char* name)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  Trajectory *t;

  ADD_ELEMENT(Trajectory, t, sp->head->prev);
  if(t == NULL){ /* Out of Memory */
	return False;
  }
  t->object = NULL;
  if(name != NULL)
	t->name = strdup(name);
  t->posn = type;
  if (n == ADAM) {
	t->adam = h;
	t->height = 0;
	t->status = ATCH;
  } else {
	t->height = h;
	t->status = THRATCH;
  }
  return True;
}

/* add a Thratch to the performance */
static Bool
program(ModeInfo *mi, const char *patn, const char *name, int cycles)
{
  const char *p;
  int w, h, i, seen;
  Notation notation;
  char type;

  if (MI_IS_VERBOSE(mi)) {
	(void) fprintf(stderr, "juggle[%d]: Programmed: %s x %d\n",
				   MI_SCREEN(mi), (name == NULL) ? patn : name, cycles);
  }

  for(w=i=0; i < cycles; i++, w++) { /* repeat until at least "cycles" throws
										have been programmed */
	/* title is the pattern name to be supplied to the first throw of
	   a sequence.  If no name if given, use an empty title so that
	   the sequences are still delimited. */
	const char *title = (name != NULL)? name : "";
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
		  type = ' ';
		  break;
		case '+':            /* Outside throw */
		case '=':            /* Cross throw */
		case '&':            /* Cross catch */
		case 'x':            /* Cross throw and catch */
		case '_':            /* Bounce */
		case 'k':            /* Kickup */
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
			i++;
			if (!add_throw(mi, type, h, notation, title))
				return False;
			title = NULL;
			type=' ';
			h = 0;
			seen = 0;
		  }
		  notation = nn;
		  break;
		default:
		  if(w == 0) { /* Only warn on first pass */
			(void) fprintf(stderr,
						   "juggle[%d]: Unexpected pattern instruction: '%c'\n",
						   MI_SCREEN(mi), *p);
		  }
		  break;
		}
	  }
	}
	if (seen) { /* end of sequence */
	  if (!add_throw(mi, type, h, notation, title))
		return False;
	  title = NULL;
	}
  }
  return True;
}

/*
 ~~~~\~~~~~\~~~
 \\~\\~\~\\\~~~
 \\~\\\\~\\\~\~
 \\\\\\\\\\\~\\

[ 3 3 1 3 4 2 3 1 3 3 4 0 2 1 ]

4 4 1 3 12 2 4 1 4 4 13 0 3 1

*/
#define BOUNCEOVER 10
#define KICKMIN 7
#define THROWMAX 20

/* Convert Adam notation into heights */
static void
adam(jugglestruct *sp)
{
  Trajectory *t, *p;
  for(t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == ATCH) {
	  int a = t->adam;
	  t->height = 0;
	  for(p = t->next; a > 0; p = p->next) {
		if(p == sp->head) {
		  t->height = -9; /* Indicate end of processing for name() */
		  return;
		}
		if (p->status != ATCH || p->adam < 0 || p->adam>= a) {
		  a--;
		}
		t->height++;
	  }
	  if(t->height > BOUNCEOVER && t->posn == ' '){
		t->posn = '_'; /* high defaults can be bounced */
	  } else if(t->height < 3 && t->posn == '_') {
		t->posn = ' '; /* Can't bounce short throws. */
	  }
	  if(t->height < KICKMIN && t->posn == 'k'){
		t->posn = ' '; /* Can't kick short throws */
	  }
	  if(t->height > THROWMAX){
		t->posn = 'k'; /* Use kicks for ridiculously high throws */
	  }
	  t->status = THRATCH;
	}
  }
}

/* Discover converted heights and update the sequence title */
static void
name(jugglestruct *sp)
{
  Trajectory *t, *p;
  char buffer[BUFSIZ];
  char *b;
  for(t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == THRATCH && t->name != NULL) {
	  b = buffer;
	  for(p = t; p == t || p->name == NULL; p = p->next) {
		if(p == sp->head || p->height < 0) { /* end of reliable data */
		  return;
		}
		if(p->posn == ' ') {
		  b += sprintf(b, " %d", p->height);
		} else {
		  b += sprintf(b, " %c%d", p->posn, p->height);
		}
		if(b - buffer > 500) break; /* otherwise this could eventually
									   overflow.  It'll be too big to
									   display anyway. */
	  }
	  if(*t->name != 0) {
		(void) sprintf(b, ", %s", t->name);
	  }
	  free(t->name); /* Don't need name any more, it's been converted
						to pattern */
	  t->name = NULL;
	  if(t->pattern != NULL) free(t->pattern);
	  t->pattern = strdup(buffer);
	}
  }
}

/* Split Thratch notation into explicit throws and catches.
   Usually Catch follows Throw in same hand, but take care of special
   cases. */

/* ..n1.. -> .. LTn RT1 LC RC .. */
/* ..nm.. -> .. LTn LC RTm RC .. */

static Bool
part(ModeInfo *mi)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
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
	  case ' ': posn = '-'; t->posn = '+'; break;
	  case '+': posn = '+'; t->posn = '-'; break;
	  case '=': posn = '='; t->posn = '+'; break;
	  case '&': posn = '+'; t->posn = '='; break;
	  case 'x': posn = '='; t->posn = '='; break;
	  case '_': posn = '_'; t->posn = '-'; break;
	  case 'k': posn = 'k'; t->posn = 'k'; break;
	  default:
		(void) fprintf(stderr, "juggle: unexpected posn %c\n", t->posn);
		break;
	  }
	  hand = (Hand) ((hand + 1) % 2);
	  t->status = ACTION;
	  t->hand = hand;
	  p = t->prev;

	  if (t->height == 1 && p != sp->head) {
		p = p->prev; /* '1's are thrown earlier than usual */
	  }



	  t->action = CATCH;
	  ADD_ELEMENT(Trajectory, nt, p);
	  if(nt == NULL){
		return False;
	  }
	  nt->object = NULL;
	  nt->status = ACTION;
	  nt->action = THROW;
	  nt->height = t->height;
	  nt->hand = hand;
	  nt->posn = posn;

	}
  }
  return True;
}

static ObjType
choose_object(void) {
  ObjType o;
  for (;;) {
	o = (ObjType)NRAND((ObjType)NUM_OBJECT_TYPES);
	if(balls && o == BALL) break;
	if(clubs && o == CLUB) break;
	if(torches && o == TORCH) break;
	if(knives && o == KNIFE) break;
	if(rings && o == RING) break;
	if(bballs && o == BBALLS) break;
  }
  return o;
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
	  if (t->action == THROW) {
		if (t->type == Empty) {
		  /* Create new Object */
		  ADD_ELEMENT(Object, t->object, sp->objects);
		  t->object->count = 1;
		  t->object->tracelen = 0;
		  t->object->active = False;
		  /* Initialise object's circular trace list */
		  ADD_ELEMENT(Trace, t->object->trace, t->object->trace);

		  if (MI_NPIXELS(mi) > 2) {
			t->object->color = 1 + NRAND(MI_NPIXELS(mi) - 2);
		  } else {
#ifdef STANDALONE
			t->object->color = 1;
#else
			t->object->color = 0;
#endif
		  }

		  /* Small chance of picking a random object instead of the
			 current theme. */
		  if(NRAND(OBJMIXPROB) == 0) {
			t->object->type = choose_object();
		  } else {
			t->object->type = sp->objtypes;
		  }

		  /* Check to see if we need trails for this object */
		  if(tail < ObjectDefs[t->object->type].mintrail) {
			t->object->tail = ObjectDefs[t->object->type].mintrail;
		  } else {
			t->object->tail = tail;
		  }
		}

		/* Balls can change divisions at each throw */
                /* no, that looks stupid. -jwz */
                if (t->divisions < 1)
                  t->divisions = 2 * (NRAND(2) + 1);

		/* search forward for next catch in this hand */
		for (p = t->next; t->handlink == NULL; p = p->next) {
		  if(p->status < ACTION || p == sp->head) return;
		  if (p->action == CATCH) {
			if (t->handlink == NULL && p->hand == t->hand) {
			  t->handlink = p;
			}
		  }
		}

		if (t->height > 0) {
		  h = t->height - 1;

		  /* search forward for next ball catch */
		  for (p = t->next; t->balllink == NULL; p = p->next) {
			if(p->status < ACTION || p == sp->head) {
			  t->handlink = NULL;
			  return;
			}
			if (p->action == CATCH) {
			  if (t->balllink == NULL && --h < 1) { /* caught */
				t->balllink = p; /* complete trajectory */
# if 0
				if (p->type == Full) {
				  (void) fprintf(stderr, "juggle[%d]: Dropped %d\n",
						  MI_SCREEN(mi), t->object->color);
				}
#endif
				p->type = Full;
				DUP_OBJECT(p, t); /* accept catch */
				p->angle = t->angle;
				p->divisions = t->divisions;
			  }
			}
		  }
		}
		t->type = Empty; /* thrown */
	  } else if (t->action == CATCH) {
		/* search forward for next throw from this hand */
		for (p = t->next; t->handlink == NULL; p = p->next) {
		  if(p->status < ACTION || p == sp->head) return;
		  if (p->action == THROW && p->hand == t->hand) {
			p->type = t->type; /* pass ball */
			DUP_OBJECT(p, t); /* pass object */
			p->divisions = t->divisions;
			t->handlink = p;
		  }
		}
	  }
	  t->status = LINKEDACTION;
	}
  }
}

/* Clap when both hands are empty */
static void
clap(jugglestruct *sp)
{
  Trajectory *t, *p;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status == LINKEDACTION &&
		t->action == CATCH &&
		t->type == Empty &&
		t->handlink != NULL &&
		t->handlink->height == 0) { /* Completely idle hand */

	  for (p = t->next; p != sp->head; p = p->next) {
		if (p->status == LINKEDACTION &&
			p->action == CATCH &&
			p->hand != t->hand) { /* Next catch other hand */
		  if(p->type == Empty &&
			 p->handlink != NULL &&
			 p->handlink->height == 0) { /* Also completely idle */

			t->handlink->posn = '^'; /* Move first hand's empty throw */
			p->posn = '^';           /* to meet second hand's empty
										catch */

		  }
		  break; /* Only need first catch */
		}
	  }
	}
  }
}

#define CUBIC(s, t) ((((s).a * (t) + (s).b) * (t) + (s).c) * (t) + (s).d)

/* Compute single spline from x0 with velocity dx0 at time t0 to x1
   with velocity dx1 at time t1 */
static Spline
makeSpline(double x0, double dx0, int t0, double x1, double dx1, int t1)
{
  Spline s;
  double a, b, c, d;
  double x10;
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

/* Compute a pair of splines.  s1 goes from x0 vith velocity dx0 at
   time t0 to x1 at time t1.  s2 goes from x1 at time t1 to x2 with
   velocity dx2 at time t2.  The arrival and departure velocities at
   x1, t1 must be the same. */
static double
makeSplinePair(Spline *s1, Spline *s2,
			   double x0, double dx0, int t0,
			   double x1,             int t1,
			   double x2, double dx2, int t2)
{
  double x10, x21, t21, t10, t20, dx1;
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

/* Compute a Ballistic path in a pair of degenerate splines.  sx goes
   from x at time t at constant velocity dx.  sy goes from y at time t
   with velocity dy and constant acceleration g. */
static void
makeParabola(Trajectory *n,
			 double x, double dx, double y, double dy, double g)
{
  double t = (double)n->start;
  n->xp.a = 0;
  n->xp.b = 0;
  n->xp.c = dx;
  n->xp.d = -dx*t + x;
  n->yp.a = 0;
  n->yp.b = g/2;
  n->yp.c = -g*t + dy;
  n->yp.d = g/2*t*t - dy*t + y;
}




#define SX 25 /* Shoulder Width */

/* Convert hand position symbols into actual time/space coordinates */
static void
positions(jugglestruct *sp)
{
  Trajectory *t;
  unsigned long now = sp->time; /* Make sure we're not lost in the past */
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status >= PTHRATCH) {
	  now = t->start;
	} else if (t->status == ACTION || t->status == LINKEDACTION) {
	  /* Allow ACTIONs to be annotated, but we won't mark them ready
		 for the next stage */

	  double xo = 0, yo;
	  double sx = SX;
	  double pose = SX/2;

	  /* time */
	  if (t->action == CATCH) { /* Throw-to-catch */
		if (t->type == Empty) {
		  now += (int) THROW_NULL_INTERVAL; /* failed catch is short */
		} else {     /* successful catch */
		  now += (int)(THROW_CATCH_INTERVAL);
		}
	  } else { /* Catch-to-throw */
		if(t->object != NULL) {
		  now += (int) (CATCH_THROW_INTERVAL *
						ObjectDefs[t->object->type].weight);
		} else {
		  now += (int) (CATCH_THROW_INTERVAL);
		}
	  }

	  if(t->start == 0)
		t->start = now;
	  else /* Concatenated performances may need clock resync */
		now = t->start;

	  t->cx = 0;

	  /* space */
	  yo = 90;

	  /* Add room for the handle */
	  if(t->action == CATCH && t->object != NULL)
		yo -= ObjectDefs[t->object->type].handle;

	  switch (t->posn) {
	  case '-': xo = sx - pose; break;
	  case '_':
	  case 'k':
	  case '+': xo = sx + pose; break;
	  case '~':
	  case '=': xo = - sx - pose; yo += pose; break;
	  case '^': xo = 0; yo += pose*2; break; /* clap */
	  default:
		(void) fprintf(stderr, "juggle: unexpected posn %c\n", t->posn);
		break;
	  }

#ifdef _2DSpinsDontWorkIn3D
	  t->angle = (((t->hand == LEFT) ^
				   (t->posn == '+' || t->posn == '_' || t->posn == 'k' ))?
					-1 : 1) * M_PI/2;
#else
         t->angle = -M_PI/2;
#endif

	  t->x = t->cx + ((t->hand == LEFT) ? xo : -xo);
	  t->y = yo;

	  /* Only mark complete if it was already linked */
	  if(t->status == LINKEDACTION) {
		t->status = PTHRATCH;
	  }
	}
  }
}


/* Private physics functions */

/* Compute the spin-rate for a trajectory.  Different types of throw
   (eg, regular thows, bounces, kicks, etc) have different spin
   requirements.

   type = type of object
   h = trajectory of throwing hand (throws), or next throwing hand (catches)
   old = earlier spin to consider
   dt = time span of this trajectory
   height = height of ball throw or 0 if based on old spin
   turns = full club turns required during this operation
   togo = partial club turns required to match hands
*/
static double
spinrate(ObjType type, Trajectory *h, double old, double dt,
		 int height, int turns, double togo)
{
#ifdef _2DSpinsDontWorkIn3D
  const int dir = (h->hand == LEFT) ^ (h->posn == '+')? -1 : 1;
#else
  const int dir = 1;
#endif

  if(ObjectDefs[type].handle != 0) { /* Clubs */
	return (dir * turns * 2 * M_PI + togo) / dt;
  } else if(height == 0) { /* Balls already spinning */
	return old/2;
  } else { /* Balls */
	return dir * NRAND(height*10)/20/ObjectDefs[type].weight * 2 * M_PI / dt;
  }
}


/* compute the angle at the end of a spinning trajectory */
static double
end_spin(Trajectory *t)
{
  return t->angle + t->spin * (t->finish - t->start);
}

/* Sets the initial angle of the catch following hand movement t to
   the final angle of the throw n.  Also sets the angle of the
   subsequent throw to the same angle plus half a turn. */
static void
match_spins_on_catch(Trajectory *t, Trajectory *n)
{
  if(ObjectDefs[t->balllink->object->type].handle == 0) {
	t->balllink->angle = end_spin(n);
	if(t->balllink->handlink != NULL) {
#ifdef _2DSpinsDontWorkIn3D
	  t->balllink->handlink->angle = t->balllink->angle + M_PI;
#else
         t->balllink->handlink->angle = t->balllink->angle;
#endif
	}
  }
}

static double
find_bounce(jugglestruct *sp,
			double yo, double yf, double yc, double tc, double cor)
{
  double tb, i, dy = 0;
  const double e = 1; /* permissible error in yc */

  /*
	tb = time to bounce
	yt = height at catch time after one bounce
	one or three roots according to timing
	find one by interval bisection
  */
  tb = tc;
  for(i = tc / 2; i > 0.0001; i/=2){
	double dt, yt;
	if(tb == 0){
	  (void) fprintf(stderr, "juggle: bounce div by zero!\n");
	  break;
	}
	dy = (yf - yo)/tb + sp->Gr/2*tb;
	dt = tc - tb;
	yt = -cor*dy*dt + sp->Gr/2*dt*dt + yf;
	if(yt < yc + e){
	  tb-=i;
	}else if(yt > yc - e){
	  tb+=i;
	}else{
	  break;
	}
  }
  if(dy*THROW_CATCH_INTERVAL < -200) { /* bounce too hard */
	tb = -1;
  }
  return tb;
}

static Trajectory*
new_predictor(const Trajectory *t, int start, int finish, double angle)
{
  Trajectory *n;
  ADD_ELEMENT(Trajectory, n, t->prev);
  if(n == NULL){
	return NULL;
  }
  DUP_OBJECT(n, t);
  n->divisions = t->divisions;
  n->type = Ball;
  n->status = PREDICTOR;

  n->start = start;
  n->finish = finish;
  n->angle = angle;
  return n;
}

/* Turn abstract timings into physically appropriate object trajectories. */
static Bool
projectile(jugglestruct *sp)
{
  Trajectory *t;
  const int yf = 0; /* Floor height */

  for (t = sp->head->next; t != sp->head; t = t->next) {
	if (t->status != PTHRATCH || t->action != THROW) {
	  continue;
	} else if (t->balllink == NULL) { /* Zero Throw */
	  t->status = BPREDICTOR;
	} else if (t->balllink->handlink == NULL) { /* Incomplete */
	  return True;
	} else if(t->balllink == t->handlink) {
	  /* '2' height - hold on to ball.  Don't need to consider
		 flourishes, 'hands' will do that automatically anyway */

	  t->type = Full;
	  /* Zero spin to avoid wrist injuries */
	  t->spin = 0;
	  match_spins_on_catch(t, t);
	  t->dx = t->dy = 0;
	  t->status = BPREDICTOR;
	  continue;
	} else {
	  if (t->posn == '_') { /* Bounce once */

		const int tb = t->start +
		  find_bounce(sp, t->y, (double) yf, t->balllink->y,
					  (double) (t->balllink->start - t->start),
					  ObjectDefs[t->object->type].cor);

		if(tb < t->start) { /* bounce too hard */
		  t->posn = '+'; /* Use regular throw */
		} else {
		  Trajectory *n; /* First (throw) trajectory. */
		  double dt; /* Time span of a trajectory */
		  double dy; /* Distance span of a follow-on trajectory.
						First trajectory uses t->dy */
		  /* dx is constant across both trajectories */
		  t->dx = (t->balllink->x - t->x) / (t->balllink->start - t->start);

		  { /* ball follows parabola down */
			n = new_predictor(t, t->start, tb, t->angle);
			if(n == NULL) return False;
			dt = n->finish - n->start;
			/* Ball rate 4, no flight or matching club turns */
			n->spin = spinrate(t->object->type, t, 0.0, dt, 4, 0, 0.0);
			t->dy = (yf - t->y)/dt - sp->Gr/2*dt;
			makeParabola(n, t->x, t->dx, t->y, t->dy, sp->Gr);
		  }

		  { /* ball follows parabola up */
			Trajectory *m = new_predictor(t, n->finish, t->balllink->start,
										  end_spin(n));
			if(m == NULL) return False;
			dt = m->finish - m->start;
			/* Use previous ball rate, no flight club turns */
			m->spin = spinrate(t->object->type, t, n->spin, dt, 0, 0,
							   t->balllink->angle - m->angle);
			match_spins_on_catch(t, m);
			dy = (t->balllink->y - yf)/dt - sp->Gr/2 * dt;
			makeParabola(m, t->balllink->x - t->dx * dt,
						 t->dx, (double) yf, dy, sp->Gr);
		  }

		  t->status = BPREDICTOR;
		  continue;
		}
	  } else if (t->posn == 'k') { /* Drop & Kick */
		Trajectory *n; /* First (drop) trajectory. */
		Trajectory *o; /* Second (rest) trajectory */
		Trajectory *m; /* Third (kick) trajectory */
		const int td = t->start + 2*THROW_CATCH_INTERVAL; /* Drop time */
		const int tk = t->balllink->start - 5*THROW_CATCH_INTERVAL; /* Kick */
		double dt, dy;

		{ /* Fall to ground */
		  n = new_predictor(t, t->start, td, t->angle);
		  if(n == NULL) return False;
		  dt = n->finish - n->start;
		  /* Ball spin rate 4, no flight club turns */
		  n->spin = spinrate(t->object->type, t, 0.0, dt, 4, 0,
							 t->balllink->angle - n->angle);
		  t->dx = (t->balllink->x - t->x) / dt;
		  t->dy = (yf - t->y)/dt - sp->Gr/2*dt;
		  makeParabola(n, t->x, t->dx, t->y, t->dy, sp->Gr);
		}

		{ /* Rest on ground */
		  o = new_predictor(t, n->finish, tk, end_spin(n));
		  if(o == NULL) return False;
		  o->spin = 0;
		  makeParabola(o, t->balllink->x, 0.0, (double) yf, 0.0, 0.0);
		}

		/* Kick up */
		{
		  m = new_predictor(t, o->finish, t->balllink->start, end_spin(o));
		  if(m == NULL) return False;
		  dt = m->finish - m->start;
		  /* Match receiving hand, ball rate 4, one flight club turn */
		  m->spin = spinrate(t->object->type, t->balllink->handlink, 0.0, dt,
							 4, 1, t->balllink->angle - m->angle);
		  match_spins_on_catch(t, m);
		  dy = (t->balllink->y - yf)/dt - sp->Gr/2 * dt;
		  makeParabola(m, t->balllink->x, 0.0, (double) yf, dy, sp->Gr);
		}

		t->status = BPREDICTOR;
		continue;
	  }

	  /* Regular flight, no bounce */
	  { /* ball follows parabola */
		double dt;
		Trajectory *n = new_predictor(t, t->start,
									  t->balllink->start, t->angle);
		if(n == NULL) return False;
		dt = t->balllink->start - t->start;
		/* Regular spin */
		n->spin = spinrate(t->object->type, t, 0.0, dt, t->height, t->height/2,
						   t->balllink->angle - n->angle);
		match_spins_on_catch(t, n);
		t->dx = (t->balllink->x - t->x) / dt;
		t->dy = (t->balllink->y - t->y) / dt - sp->Gr/2 * dt;
		makeParabola(n, t->x, t->dx, t->y, t->dy, sp->Gr);
	  }

	  t->status = BPREDICTOR;
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


	/* FIXME: These adjustments leave a small glitch when alternating
	   balls and clubs.  Just hope no-one notices.  :-) */

	/* make sure empty hand spin matches the thrown object in case it
	   had a handle */

	t->spin = ((t->hand == LEFT)? -1 : 1 ) *
	  fabs((u->angle - t->angle)/(u->start - t->start));

	u->spin = ((v->hand == LEFT) ^ (v->posn == '+')? -1 : 1 ) *
	  fabs((v->angle - u->angle)/(v->start - u->start));

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
find_elbow(int armlength, DXPoint *h, DXPoint *e, DXPoint *p, DXPoint *s,
		   int z)
{
  double r, h2, t;
  double x = p->x - s->x;
  double y = p->y - s->y;
  h2 = x*x + y*y + z*z;
  if (h2 > 4 * armlength * armlength) {
	t = armlength/sqrt(h2);
	e->x = t*x + s->x;
	e->y = t*y + s->y;
	h->x = 2 * t * x + s->x;
	h->y = 2 * t * y + s->y;
  } else {
	r = sqrt((double)(x*x + z*z));
	t = sqrt(4 * armlength * armlength / h2 - 1);
	e->x = x*(1 + y*t/r)/2 + s->x;
	e->y = (y - r*t)/2 + s->y;
	h->x = x + s->x;
	h->y = y + s->y;
  }
}


/* NOTE: returned x, y adjusted for arm reach */
static void
reach_arm(ModeInfo * mi, Hand side, DXPoint *p)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  DXPoint h, e;
  find_elbow(40, &h, &e, p, &sp->arm[1][side][SHOULDER], 25);
  *p = sp->arm[1][side][HAND] = h;
  sp->arm[1][side][ELBOW] = e;
}

#if DEBUG
/* dumps a human-readable rendition of the current state of the juggle
   pipeline to stderr for debugging */
static void
dump(jugglestruct *sp)
{
  Trajectory *t;
  for (t = sp->head->next; t != sp->head; t = t->next) {
	switch (t->status) {
	case ATCH:
	  (void) fprintf(stderr, "%p a %c%d\n", (void*)t, t->posn, t->adam);
	  break;
	case THRATCH:
	  (void) fprintf(stderr, "%p T %c%d %s\n", (void*)t, t->posn, t->height,
					 t->pattern == NULL?"":t->pattern);
	  break;
	case ACTION:
	  if (t->action == CATCH)
	    (void) fprintf(stderr, "%p A %c%cC\n",
					 (void*)t, t->posn,
					 t->hand ? 'R' : 'L');
	  else
	    (void) fprintf(stderr, "%p A %c%c%c%d\n",
					 (void*)t, t->posn,
					 t->hand ? 'R' : 'L',
					 (t->action == THROW)?'T':'N',
					 t->height);
	  break;
	case LINKEDACTION:
	  (void) fprintf(stderr, "%p L %c%c%c%d %d %p %p\n",
					 (void*)t, t->posn,
					 t->hand?'R':'L',
					 (t->action == THROW)?'T':(t->action == CATCH?'C':'N'),
					 t->height, t->object == NULL?0:t->object->color,
					 (void*)t->handlink, (void*)t->balllink);
	  break;
	case PTHRATCH:
	  (void) fprintf(stderr, "%p O %c%c%c%d %d %2d %6lu %6lu\n",
					 (void*)t, t->posn,
					 t->hand?'R':'L',
					 (t->action == THROW)?'T':(t->action == CATCH?'C':'N'),
					 t->height, t->type, t->object == NULL?0:t->object->color,
					 t->start, t->finish);
	  break;
	case BPREDICTOR:
	  (void) fprintf(stderr, "%p B %c      %2d %6lu %6lu %g\n",
					 (void*)t, t->type == Ball?'b':t->type == Empty?'e':'f',
					 t->object == NULL?0:t->object->color,
					 t->start, t->finish, t->yp.c);
	  break;
	case PREDICTOR:
	  (void) fprintf(stderr, "%p P %c      %2d %6lu %6lu %g\n",
					 (void*)t, t->type == Ball?'b':t->type == Empty?'e':'f',
					 t->object == NULL?0:t->object->color,
					 t->start, t->finish, t->yp.c);
	  break;
	default:
	  (void) fprintf(stderr, "%p: status %d not implemented\n",
					 (void*)t, t->status);
	  break;
	}
  }
  (void) fprintf(stderr, "---\n");
}
#endif

static int get_num_balls(const char *j)
{
  int balls = 0;
  const char *p;
  int h = 0;
  if (!j) abort();
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

static int
compare_num_balls(const void *p1, const void *p2)
{
  int i, j;
  i = get_num_balls(((patternstruct*)p1)->pattern);
  j = get_num_balls(((patternstruct*)p2)->pattern);
  if (i > j) {
	return (1);
  } else if (i < j) {
	return (-1);
  } else {
	return (0);
  }
}


/**************************************************************************
 *                        Rendering Functions                             *
 *                                                                        *
 **************************************************************************/

static int
show_arms(ModeInfo * mi)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  unsigned int i, j;
  Hand side;
  XPoint a[countof(sp->arm[0][0])];
  int slices = 12;
  int thickness = 7;
  int soffx = 10;
  int soffy = 11;

  glFrontFace(GL_CCW);

  j = 1;
  for(side = LEFT; side <= RIGHT; side = (Hand)((int)side + 1)) {
	/* Translate into device coords */
	for(i = 0; i < countof(a); i++) {
	  a[i].x = (short)(SCENE_WIDTH/2 + sp->arm[j][side][i].x*sp->scale);
	  a[i].y = (short)(SCENE_HEIGHT  - sp->arm[j][side][i].y*sp->scale);
	  if(j == 1)
		sp->arm[0][side][i] = sp->arm[1][side][i];
	}

        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);

        /* Upper arm */
        polys += tube (a[2].x - (side == LEFT ? soffx : -soffx), a[2].y + soffy, 0,
                       a[1].x, a[1].y, ARMLENGTH/2,
                       thickness, 0, slices,
                       True, True, MI_IS_WIREFRAME(mi));

        /* Lower arm */
        polys += tube (a[1].x, a[1].y, ARMLENGTH/2,
                       a[0].x, a[0].y, ARMLENGTH,
                       thickness * 0.8, 0, slices,
                       True, True, MI_IS_WIREFRAME(mi));

        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_2);

        /* Shoulder */
        glPushMatrix();
        glTranslatef (a[2].x - (side == LEFT ? soffx : -soffx), 
                      a[2].y + soffy, 
                      0);
        glScalef(9, 9, 9);
        polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        /* Elbow */
        glPushMatrix();
        glTranslatef (a[1].x, a[1].y, ARMLENGTH/2);
        glScalef(4, 4, 4);
        polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        /* Hand */
        glPushMatrix();
        glTranslatef (a[0].x, a[0].y, ARMLENGTH);
        glScalef(8, 8, 8);
        polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
        glPopMatrix();

  }
  return polys;
}

static int
show_figure(ModeInfo * mi, Bool init)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  /*XPoint p[7];*/
  int i;

  /*      +-----+ 9
          |  6  |
       10 +--+--+
       2 +---+---+ 3
          \  5  /
           \   /
            \ /
           1 +
            / \
           /   \
        0 +-----+ 4
          |     |
          |     |
          |     |
        7 +     + 8
  */

  /* #### most of this is unused now */
  static const XPoint figure[] = {
	{ 15,  70}, /* 0  Left Hip */
	{  0,  90}, /* 1  Waist */
	{ SX, 130}, /* 2  Left Shoulder */
	{-SX, 130}, /* 3  Right Shoulder */
	{-15,  70}, /* 4  Right Hip */
	{  0, 130}, /* 5  Neck */
	{  0, 140}, /* 6  Chin */
	{ SX,   0}, /* 7  Left Foot */
	{-SX,   0}, /* 8  Right Foot */
	{-17, 174}, /* 9  Head1 */
	{ 17, 140}, /* 10 Head2 */
  };
  XPoint a[countof(figure)];
  GLfloat gcolor[4] = { 1, 1, 1, 1 };

  /* Translate into device coords */
  for(i = 0; i < countof(figure); i++) {
	a[i].x = (short)(SCENE_WIDTH/2 + (sp->cx + figure[i].x)*sp->scale);
	a[i].y = (short)(SCENE_HEIGHT - figure[i].y*sp->scale);
  }

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor);

  glFrontFace(GL_CCW);

  {
    GLfloat scale = ((GLfloat) a[10].x - a[9].x) / 2;
    int slices = 12;

    glPushMatrix();
    {
      glTranslatef(a[6].x, a[6].y - scale, 0);
      glScalef(scale, scale, scale);

      /* Head */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);
      glPushMatrix();
      scale = 0.75;
      glScalef(scale, scale, scale);
      glTranslatef(0, 0.3, 0);
      glPushMatrix();
      glTranslatef(0, 0, 0.35);
      polys += tube (0, 0, 0,
                     0, 1.1, 0,
                     0.64, 0,
                     slices, True, True, MI_IS_WIREFRAME(mi));
      glPopMatrix();
      glScalef(0.9, 0.9, 1);
      polys += unit_sphere(2*slices, 2*slices, MI_IS_WIREFRAME(mi));
      glPopMatrix();

      /* Neck */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_2);
      glTranslatef(0, 1.1, 0);
      glPushMatrix();
      scale = 0.35;
      glScalef(scale, scale, scale);
      polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
      glPopMatrix();

      /* Torso */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);
      glTranslatef(0, 1.1, 0);
      glPushMatrix();
      glScalef(0.9, 1.0, 0.9);
      polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
      glPopMatrix();

      /* Belly */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_2);
      glTranslatef(0, 1.0, 0);
      glPushMatrix();
      scale = 0.6;
      glScalef(scale, scale, scale);
      polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
      glPopMatrix();

      /* Hips */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);
      glTranslatef(0, 0.8, 0);
      glPushMatrix();
      scale = 0.85;
      glScalef(scale, scale, scale);
      polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
      glPopMatrix();


      /* Legs */
      glTranslatef(0, 0.7, 0);

      for (i = -1; i <= 1; i += 2) {
        glPushMatrix();

        glRotatef (i*10, 0, 0, 1);
        glTranslatef(-i*0.65, 0, 0);
        
        /* Hip socket */
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_2);
        scale = 0.45;
        glScalef(scale, scale, scale);
        polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));

        /* Thigh */
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);
        glPushMatrix();
        glTranslatef(0, 0.6, 0);
        polys += tube (0, 0, 0,
                       0, 3.5, 0,
                       1, 0,
                       slices, True, True, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        /* Knee */
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_2);
        glPushMatrix();
        glTranslatef(0, 4.4, 0);
        scale = 0.7;
        glScalef(scale, scale, scale);
        polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        /* Calf */
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);
        glPushMatrix();
        glTranslatef(0, 4.7, 0);
        polys += tube (0, 0, 0,
                       0, 4.7, 0,
                       0.8, 0,
                       slices, True, True, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        /* Ankle */
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_2);
        glPushMatrix();
        glTranslatef(0, 9.7, 0);
        scale = 0.5;
        glScalef(scale, scale, scale);
        polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        /* Foot */
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, body_color_1);
        glPushMatrix();
        glRotatef (-i*10, 0, 0, 1);
        glTranslatef(-i*1.75, 9.7, 0.9);

        glScalef (0.4, 1, 1);
        polys += tube (0, 0, 0,
                       0, 0.6, 0,
                       1.9, 0,
                       slices*4, True, True, MI_IS_WIREFRAME(mi));
        glPopMatrix();

        glPopMatrix();
      }
    }
    glPopMatrix();
  }

  sp->arm[1][LEFT][SHOULDER].x = sp->cx + figure[2].x;
  sp->arm[1][RIGHT][SHOULDER].x = sp->cx + figure[3].x;
  if(init) {
	/* Initialise arms */
	unsigned int i;
	for(i = 0; i < 2; i++){
	  sp->arm[i][LEFT][SHOULDER].y = figure[2].y;
	  sp->arm[i][LEFT][ELBOW].x = figure[2].x;
	  sp->arm[i][LEFT][ELBOW].y = figure[1].y;
	  sp->arm[i][LEFT][HAND].x = figure[0].x;
	  sp->arm[i][LEFT][HAND].y = figure[1].y;
	  sp->arm[i][RIGHT][SHOULDER].y = figure[3].y;
	  sp->arm[i][RIGHT][ELBOW].x = figure[3].x;
	  sp->arm[i][RIGHT][ELBOW].y = figure[1].y;
	  sp->arm[i][RIGHT][HAND].x = figure[4].x;
	  sp->arm[i][RIGHT][HAND].y = figure[1].y;
	}
  }
  return polys;
}

typedef struct { GLfloat x, y, z; } XYZ;

/* lifted from sphere.c */
static int
striped_unit_sphere (int stacks, int slices, 
                     int stripes, 
                     GLfloat *color1, GLfloat *color2,
                     int wire_p)
{
  int polys = 0;
  int i,j;
  double theta1, theta2, theta3;
  XYZ e, p;
  XYZ la = { 0, 0, 0 }, lb = { 0, 0, 0 };
  XYZ c = {0, 0, 0};  /* center */
  double r = 1.0;     /* radius */
  int stacks2 = stacks * 2;

  if (r < 0)
    r = -r;
  if (slices < 0)
    slices = -slices;

  if (slices < 4 || stacks < 2 || r <= 0)
    {
      glBegin (GL_POINTS);
      glVertex3f (c.x, c.y, c.z);
      glEnd();
      return 1;
    }

  glFrontFace(GL_CW);

  for (j = 0; j < stacks; j++)
    {
      theta1 = j       * (M_PI+M_PI) / stacks2 - M_PI_2;
      theta2 = (j + 1) * (M_PI+M_PI) / stacks2 - M_PI_2;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
                    ((j == 0 || j == stacks-1 ||
                     j % (stacks / (stripes+1)))
                     ? color1 : color2));

      glBegin (wire_p ? GL_LINE_LOOP : GL_TRIANGLE_STRIP);
      for (i = 0; i <= slices; i++)
        {
          theta3 = i * (M_PI+M_PI) / slices;

          if (wire_p && i != 0)
            {
              glVertex3f (lb.x, lb.y, lb.z);
              glVertex3f (la.x, la.y, la.z);
            }

          e.x = cos (theta2) * cos(theta3);
          e.y = sin (theta2);
          e.z = cos (theta2) * sin(theta3);
          p.x = c.x + r * e.x;
          p.y = c.y + r * e.y;
          p.z = c.z + r * e.z;

          glNormal3f (e.x, e.y, e.z);
          glTexCoord2f (i       / (double)slices,
                        2*(j+1) / (double)stacks2);
          glVertex3f (p.x, p.y, p.z);
          if (wire_p) la = p;

          e.x = cos(theta1) * cos(theta3);
          e.y = sin(theta1);
          e.z = cos(theta1) * sin(theta3);
          p.x = c.x + r * e.x;
          p.y = c.y + r * e.y;
          p.z = c.z + r * e.z;

          glNormal3f (e.x, e.y, e.z);
          glTexCoord2f (i   / (double)slices,
                        2*j / (double)stacks2);
          glVertex3f (p.x, p.y, p.z);
          if (wire_p) lb = p;
          polys++;
        }
      glEnd();
    }
  return polys;
}



static int
show_ball(ModeInfo *mi, unsigned long color, Trace *s)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  /*int offset = (int)(s->angle*64*180/M_PI);*/
  short x = (short)(SCENE_WIDTH/2 + s->x * sp->scale);
  short y = (short)(SCENE_HEIGHT - s->y * sp->scale);
  GLfloat gcolor1[4] = { 0, 0, 0, 1 };
  GLfloat gcolor2[4] = { 0, 0, 0, 1 };
  int slices = 24;

  /* Avoid wrapping */
  if(s->y*sp->scale >  SCENE_HEIGHT * 2) return 0;

  gcolor1[0] = mi->colors[color].red   / 65536.0;
  gcolor1[1] = mi->colors[color].green / 65536.0;
  gcolor1[2] = mi->colors[color].blue  / 65536.0;

  gcolor2[0] = gcolor1[0] / 3;
  gcolor2[1] = gcolor1[1] / 3;
  gcolor2[2] = gcolor1[2] / 3;

  glFrontFace(GL_CCW);

  {
    GLfloat scale = BALLRADIUS;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, scale);

    glRotatef (s->angle / M_PI*180, 1, 1, 0);

    polys += striped_unit_sphere (slices, slices, s->divisions, 
                                  gcolor1, gcolor2, MI_IS_WIREFRAME(mi));
    glPopMatrix();
  }
  return polys;
}

static int
show_europeanclub(ModeInfo *mi, unsigned long color, Trace *s)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  /*int offset = (int)(s->angle*64*180/M_PI);*/
  short x = (short)(SCENE_WIDTH/2 + s->x * sp->scale);
  short y = (short)(SCENE_HEIGHT - s->y * sp->scale);
  double radius = 12 * sp->scale;
  GLfloat gcolor1[4] = { 0, 0, 0, 1 };
  GLfloat gcolor2[4] = { 1, 1, 1, 1 };
  int slices = 16;
  int divs = 4;

  /*    6   6
         +-+
        /   \
     4 +-----+ 7
      ////////\
   3 +---------+ 8
   2 +---------+ 9
      |///////|
    1 +-------+ 10
       |     |
       |     |
        |   |
        |   |
         | |
         | |
         +-+
        0  11 	*/

  /* Avoid wrapping */
  if(s->y*sp->scale >  SCENE_HEIGHT * 2) return 0;

  gcolor1[0] = mi->colors[color].red   / 65536.0;
  gcolor1[1] = mi->colors[color].green / 65536.0;
  gcolor1[2] = mi->colors[color].blue  / 65536.0;

  glFrontFace(GL_CCW);

  {
    GLfloat scale = radius;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, scale);

    glTranslatef (0, 0, 2);  /* put end of handle in hand */

    glRotatef (s->angle / M_PI*180, 1, 0, 0);

    glPushMatrix();
    glScalef (0.5, 1, 0.5);
    polys += striped_unit_sphere (slices, slices, divs, gcolor2, gcolor1, 
                                  MI_IS_WIREFRAME(mi));
    glPopMatrix();
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor2);
    polys += tube (0, 0, 0,
                   0, 2, 0,
                   0.2, 0,
                   slices, True, True, MI_IS_WIREFRAME(mi));

    glTranslatef (0, 2, 0);
    glScalef (0.25, 0.25, 0.25);
    polys += unit_sphere(slices, slices, MI_IS_WIREFRAME(mi));

    glPopMatrix();
  }
  return polys;
}


static int
show_torch(ModeInfo *mi, unsigned long color, Trace *s)
{
  int polys = 0;
#if 0
	jugglestruct *sp = &juggles[MI_SCREEN(mi)];
	XPoint head, tail, last;
	DXPoint dhead, dlast;
	const double sa = sin(s->angle);
	const double ca = cos(s->angle);

	const double TailLen = -24;
	const double HeadLen = 16;
	const short Width   = (short)(5 * sqrt(sp->scale));

	/*
      +///+ head
    last  |
          |
          |
          |
          |
          + tail
	*/

	dhead.x = s->x + HeadLen * PERSPEC * sa;
	dhead.y = s->y - HeadLen * ca;

	if(color == MI_BLACK_PIXEL(mi)) { /* Use 'last' when erasing */
	  dlast = s->dlast;
	} else { /* Store 'last' so we can use it later when s->prev has
				gone */
	  if(s->prev != s->next) {
		dlast.x = s->prev->x + HeadLen * PERSPEC * sin(s->prev->angle);
		dlast.y = s->prev->y - HeadLen * cos(s->prev->angle);
	  } else {
		dlast = dhead;
	  }
	  s->dlast = dlast;
	}

	/* Avoid wrapping (after last is stored) */
	if(s->y*sp->scale >  SCENE_HEIGHT * 2) return 0;

	head.x = (short)(SCENE_WIDTH/2 + dhead.x*sp->scale);
	head.y = (short)(SCENE_HEIGHT - dhead.y*sp->scale);

	last.x = (short)(SCENE_WIDTH/2 + dlast.x*sp->scale);
	last.y = (short)(SCENE_HEIGHT - dlast.y*sp->scale);

	tail.x = (short)(SCENE_WIDTH/2 +
					 (s->x + TailLen * PERSPEC * sa)*sp->scale );
	tail.y = (short)(SCENE_HEIGHT - (s->y - TailLen * ca)*sp->scale );

	if(color != MI_BLACK_PIXEL(mi)) {
	  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	  XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi),
						 Width, LineSolid, CapRound, JoinRound);
	  draw_line(mi, head.x, head.y, tail.x, tail.y);
	}
	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), color);
	XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi),
					   Width * 2, LineSolid, CapRound, JoinRound);

	draw_line(mi, head.x, head.y, last.x, last.y);

#endif /* 0 */
   return polys;
}


static int
show_knife(ModeInfo *mi, unsigned long color, Trace *s)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  /*int offset = (int)(s->angle*64*180/M_PI);*/
  short x = (short)(SCENE_WIDTH/2 + s->x * sp->scale);
  short y = (short)(SCENE_HEIGHT - s->y * sp->scale);
  GLfloat gcolor1[4] = { 0, 0, 0, 1 };
  GLfloat gcolor2[4] = { 1, 1, 1, 1 };
  int slices = 8;

  /* Avoid wrapping */
  if(s->y*sp->scale >  SCENE_HEIGHT * 2) return 0;

  gcolor1[0] = mi->colors[color].red   / 65536.0;
  gcolor1[1] = mi->colors[color].green / 65536.0;
  gcolor1[2] = mi->colors[color].blue  / 65536.0;

  glFrontFace(GL_CCW);

  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef (2, 2, 2);

  glTranslatef (0, 0, 2);  /* put end of handle in hand */
  glRotatef (s->angle / M_PI*180, 1, 0, 0);

  glScalef (0.3, 1, 1);  /* flatten blade */

  glTranslatef(0, 6, 0);
  glRotatef (180, 1, 0, 0);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor1);
  polys += tube (0, 0, 0,
                 0, 10, 0,
                 1, 0,
                 slices, True, True, MI_IS_WIREFRAME(mi));

  glTranslatef (0, 12, 0);
  glScalef (0.7, 10, 0.7);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor2);
  polys += unit_sphere (slices, slices, MI_IS_WIREFRAME(mi));

  glPopMatrix();
  return polys;
}


static int
show_ring(ModeInfo *mi, unsigned long color, Trace *s)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  /*int offset = (int)(s->angle*64*180/M_PI);*/
  short x = (short)(SCENE_WIDTH/2 + s->x * sp->scale);
  short y = (short)(SCENE_HEIGHT - s->y * sp->scale);
  double radius = 12 * sp->scale;
  GLfloat gcolor1[4] = { 0, 0, 0, 1 };
  GLfloat gcolor2[4] = { 0, 0, 0, 1 };
  int slices = 24;
  int i, j;
  int wire_p = MI_IS_WIREFRAME(mi);
  GLfloat width = M_PI * 2 / slices;
  GLfloat ra = 1.0;
  GLfloat rb = 0.7;
  GLfloat thickness = 0.15;

  /* Avoid wrapping */
  if(s->y*sp->scale >  SCENE_HEIGHT * 2) return 0;

  gcolor1[0] = mi->colors[color].red   / 65536.0;
  gcolor1[1] = mi->colors[color].green / 65536.0;
  gcolor1[2] = mi->colors[color].blue  / 65536.0;

  gcolor2[0] = gcolor1[0] / 3;
  gcolor2[1] = gcolor1[1] / 3;
  gcolor2[2] = gcolor1[2] / 3;

  glFrontFace(GL_CCW);

  glPushMatrix();
  glTranslatef(0, 0, 12);  /* back of ring in hand */

  glTranslatef(x, y, 0);
  glScalef(radius, radius, radius);

  glRotatef (90, 0, 1, 0);
  glRotatef (s->angle / M_PI*180, 0, 0, 1);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor1);

  /* discs */
  for (j = -1; j <= 1; j += 2)
    {
      GLfloat z = j * thickness/2;
      glFrontFace (j < 0 ? GL_CCW : GL_CW);
      glNormal3f (0, 0, j*1);
      glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
      for (i = 0; i < slices + (wire_p ? 0 : 1); i++) {
        GLfloat th, cth, sth;
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
                      (i % (slices/3) ? gcolor1 : gcolor2));
        th = i * width;
        cth = cos(th);
        sth = sin(th);
        glVertex3f (cth * ra, sth * ra, z);
        glVertex3f (cth * rb, sth * rb, z);
        polys++;
      }
      glEnd();
    }

  /* outer ring */
  glFrontFace (GL_CCW);
  glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
  for (i = 0; i < slices + (wire_p ? 0 : 1); i++)
    {
      GLfloat th = i * width;
      GLfloat cth = cos(th);
      GLfloat sth = sin(th);
      glNormal3f (cth, sth, 0);
      glVertex3f (cth * ra, sth * ra, thickness/2);
      glVertex3f (cth * ra, sth * ra, -thickness/2);
      polys++;
    }
  glEnd();

  /* inner ring */
  glFrontFace (GL_CW);
  glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
  for (i = 0; i < slices + (wire_p ? 0 : 1); i++)
    {
      GLfloat th = i * width;
      GLfloat cth = cos(th);
      GLfloat sth = sin(th);
      glNormal3f (-cth, -sth, 0);
      glVertex3f (cth * rb, sth * ra, thickness/2);
      glVertex3f (cth * rb, sth * ra, -thickness/2);
      polys++;
    }
  glEnd();

  glFrontFace (GL_CCW);
  glPopMatrix();
  return polys;
}


static int
show_bball(ModeInfo *mi, unsigned long color, Trace *s)
{
  int polys = 0;
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  /*int offset = (int)(s->angle*64*180/M_PI);*/
  short x = (short)(SCENE_WIDTH/2 + s->x * sp->scale);
  short y = (short)(SCENE_HEIGHT - s->y * sp->scale);
  double radius = 12 * sp->scale;
  GLfloat gcolor1[4] = { 0, 0, 0, 1 };
  GLfloat gcolor2[4] = { 0, 0, 0, 1 };
  int slices = 16;
  int i, j;

  /* Avoid wrapping */
  if(s->y*sp->scale >  SCENE_HEIGHT * 2) return 0;

  gcolor1[0] = mi->colors[color].red   / 65536.0;
  gcolor1[1] = mi->colors[color].green / 65536.0;
  gcolor1[2] = mi->colors[color].blue  / 65536.0;

  glFrontFace(GL_CCW);

  {
    GLfloat scale = radius;
    glPushMatrix();

    glTranslatef(0, -6, 5);  /* position on top of hand */

    glTranslatef(x, y, 0);
    glScalef(scale, scale, scale);
    glRotatef (s->angle / M_PI*180, 1, 0, 1);

    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor1);
    polys += unit_sphere (slices, slices, MI_IS_WIREFRAME(mi));

    glRotatef (90, 0, 0, 1);
    glTranslatef (0, 0, 0.81);
    glScalef(0.15, 0.15, 0.15);
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gcolor2);
    for (i = 0; i < 3; i++) {
      glPushMatrix();
      glTranslatef (0, 0, 1);
      glRotatef (360 * i / 3, 0, 0, 1);
      glTranslatef (2, 0, 0);
      glRotatef (18, 0, 1, 0);
      glBegin (MI_IS_WIREFRAME(mi) ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
      glVertex3f (0, 0, 0);
      for (j = slices; j >= 0; j--) {
        GLfloat th = j * M_PI*2 / slices;
        glVertex3f (cos(th), sin(th), 0);
        polys++;
      }
      glEnd();
      glPopMatrix();
    }

    glPopMatrix();
  }
  return polys;
}


/**************************************************************************
 *                    Public Functions                                    *
 *                                                                        *
 **************************************************************************/


/* FIXME: refill_juggle currently just appends new throws to the
 * programme.  This is fine if the programme is empty, but if there
 * are still some trajectories left then it really should take these
 * into account */

static void
refill_juggle(ModeInfo * mi)
{
  jugglestruct *sp = NULL;
  int i;

  if (juggles == NULL)
	return;
  sp = &juggles[MI_SCREEN(mi)];

  /* generate pattern */

  if (pattern == NULL) {

#define MAXPAT 10
#define MAXREPEAT 300
#define CHANGE_BIAS 8 /* larger makes num_ball changes less likely */
#define POSITION_BIAS 20 /* larger makes hand movements less likely */

	int count = 0;
	while (count < MI_CYCLES(mi)) {
	  char buf[MAXPAT * 3 + 3], *b = buf;
	  int maxseen = 0;
	  int l = NRAND(MAXPAT) + 1;
	  int t = NRAND(MIN(MAXREPEAT, (MI_CYCLES(mi) - count))) + 1;

	  { /* vary number of balls */
		int new_balls = sp->num_balls;
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
		if (new_balls < sp->patternindex.minballs) {
		  new_balls += 2;
		}
		if (new_balls > sp->patternindex.maxballs) {
		  new_balls -= 2;
		}
		if (new_balls < sp->num_balls) {
		  if (!program(mi, "[*]", NULL, 1)) /* lose ball */
			return;
		}
		sp->num_balls = new_balls;
	  }

	  count += t;
	  if (NRAND(2) && sp->patternindex.index[sp->num_balls].number) {
		/* Pick from PortFolio */
		int p = sp->patternindex.index[sp->num_balls].start +
		  NRAND(sp->patternindex.index[sp->num_balls].number);
		if (!program(mi, portfolio[p].pattern, portfolio[p].name, t))
		  return;
	  } else {
		/* Invent a new pattern */
		*b++='[';
		for(i = 0; i < l; i++){
		  int n, m;
		  do { /* Triangular Distribution => high values more likely */
			m = NRAND(sp->num_balls + 1);
			n = NRAND(sp->num_balls + 1);
		  } while(m >= n);
		  if (n == sp->num_balls) {
			maxseen = 1;
		  }
		  switch(NRAND(5 + POSITION_BIAS)){
		  case 0:            /* Outside throw */
			*b++ = '+'; break;
		  case 1:            /* Cross throw */
			*b++ = '='; break;
		  case 2:            /* Cross catch */
			*b++ = '&'; break;
		  case 3:            /* Cross throw and catch */
			*b++ = 'x'; break;
		  case 4:            /* Bounce */
			*b++ = '_'; break;
		  default:
			break;             /* Inside throw (default) */
		  }

		  *b++ = n + '0';
		  *b++ = ' ';
		}
		*b++ = ']';
		*b = '\0';
		if (maxseen) {
		  if (!program(mi, buf, NULL, t))
			return;
		}
	  }
	}
  } else { /* pattern supplied in height or 'a' notation */
	if (!program(mi, pattern, NULL, MI_CYCLES(mi)))
	  return;
  }

  adam(sp);

  name(sp);

  if (!part(mi))
	return;

  lob(mi);

  clap(sp);

  positions(sp);

  if (!projectile(sp)) {
	return;
  }

  hands(sp);
#ifdef DEBUG
  if(MI_IS_DEBUG(mi)) dump(sp);
#endif
}

static void
change_juggle(ModeInfo * mi)
{
  jugglestruct *sp = NULL;
  Trajectory *t;

  if (juggles == NULL)
	return;
  sp = &juggles[MI_SCREEN(mi)];

  /* Strip pending trajectories */
  for (t = sp->head->next; t != sp->head; t = t->next) {
	if(t->start > sp->time || t->finish < sp->time) {
	  Trajectory *n = t;
	  t=t->prev;
	  trajectory_destroy(n);
	}
  }

  /* Pick the current object theme */
  sp->objtypes = choose_object();

  refill_juggle(mi);

  mi->polygon_count += show_figure(mi, True);
}


ENTRYPOINT void
reshape_juggle (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT void
init_juggle (ModeInfo * mi)
{
  jugglestruct *sp = 0;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, juggles);

  sp = &juggles[MI_SCREEN(mi)];

  if (!sp->glx_context)   /* re-initting breaks print_texture_label */
    sp->glx_context = init_GL(mi);

  sp->font_data = load_texture_font (mi->dpy, "titleFont");

  reshape_juggle (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  make_random_colormap (0, 0, 0,
                        mi->colors, &MI_NPIXELS(mi),
                        True, False, 0, False);

  {
    double spin_speed   = 0.05;
    double wander_speed = 0.001;
    double spin_accel   = 0.05;
    sp->rot = make_rotator (0, spin_speed, 0, 
                            spin_accel, wander_speed, False);
    sp->trackball = gltrackball_init (False);
  }

  if (only && *only && strcmp(only, " ")) {
    balls = clubs = torches = knives = rings = bballs = False;
    if (!strcasecmp (only, "balls"))   balls   = True;
    else if (!strcasecmp (only, "clubs"))   clubs   = True;
    else if (!strcasecmp (only, "torches")) torches = True;
    else if (!strcasecmp (only, "knives"))  knives  = True;
    else if (!strcasecmp (only, "rings"))   rings   = True;
    else if (!strcasecmp (only, "bballs"))  bballs  = True;
    else {
      (void) fprintf (stderr,
               "Juggle: -only must be one of: balls, clubs, torches, knives,\n"
               "\t rings, or bballs (not \"%s\")\n", only);
#ifdef STANDALONE /* xlock mustn't exit merely because of a bad argument */
      exit (1);
#endif
    }
  }

  /* #### hard to make this look good in OpenGL... */
  torches = False;


  if (sp->head == 0) {  /* first time initializing this juggler */

	sp->count = ABS(MI_COUNT(mi));
	if (sp->count == 0)
	  sp->count = 200;

	/* record start time */
	sp->begintime = time(NULL);
	if(sp->patternindex.maxballs > 0) {
	  sp->num_balls = sp->patternindex.minballs +
		NRAND(sp->patternindex.maxballs - sp->patternindex.minballs);
	}

        mi->polygon_count +=
          show_figure(mi, True); /* Draw figure.  Also discovers
                                    information about the juggler's
                                    proportions */

	/* "7" should be about three times the height of the juggler's
	   shoulders */
	sp->Gr = -GRAVITY(3 * sp->arm[0][RIGHT][SHOULDER].y,
					  7 * THROW_CATCH_INTERVAL);

	if(!balls && !clubs && !torches && !knives && !rings && !bballs)
	  balls = True; /* Have to juggle something! */

	/* create circular trajectory list */
	ADD_ELEMENT(Trajectory, sp->head, sp->head);
	if(sp->head == NULL){
	  return;
	}

	/* create circular object list */
	ADD_ELEMENT(Object, sp->objects, sp->objects);
	if(sp->objects == NULL){
	  return;
	}

	sp->pattern =  strdup(""); /* Initialise saved pattern with 
                                      free-able memory */
  }

  sp = &juggles[MI_SCREEN(mi)];

  if (pattern &&
      (!*pattern ||
       !strcasecmp (pattern, ".") ||
       !strcasecmp (pattern, "random")))
	pattern = NULL;

  if (pattern == NULL && sp->patternindex.maxballs == 0) {
	/* pattern list needs indexing */
	int nelements = countof(portfolio);
	int numpat = 0;
	int i;

	/* sort according to number of balls */
	qsort((void*)portfolio, nelements,
		  sizeof(portfolio[1]), compare_num_balls);

	/* last pattern has most balls */
	sp->patternindex.maxballs = get_num_balls(portfolio[nelements - 1].pattern);
	/* run through sorted list, indexing start of each group
	   and number in group */
	sp->patternindex.maxballs = 1;
	for (i = 0; i < nelements; i++) {
	  int b = get_num_balls(portfolio[i].pattern);
	  if (b > sp->patternindex.maxballs) {
		sp->patternindex.index[sp->patternindex.maxballs].number = numpat;
		if(numpat == 0) sp->patternindex.minballs = b;
		sp->patternindex.maxballs = b;
		numpat = 1;
		sp->patternindex.index[sp->patternindex.maxballs].start = i;
	  } else {
		numpat++;
	  }
	}
	sp->patternindex.index[sp->patternindex.maxballs].number = numpat;
  }

  /* Set up programme */
  change_juggle(mi);

  /* Only put things here that won't interrupt the programme during
	 a window resize */

  /* Use MIN so that users can resize in interesting ways, eg
	 narrow windows for tall patterns, etc */
  sp->scale = MIN(SCENE_HEIGHT/480.0, SCENE_WIDTH/160.0);

}

ENTRYPOINT Bool
juggle_handle_event (ModeInfo *mi, XEvent *event)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, sp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &sp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      change_juggle (mi);
      return True;
    }

  return False;
}


ENTRYPOINT void
draw_juggle (ModeInfo *mi)
{
  jugglestruct *sp = &juggles[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  Trajectory *traj = NULL;
  Object *o = NULL;
  unsigned long future = 0;
  char *pattern = NULL;

  if (!sp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *sp->glx_context);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);

  glTranslatef(0,-3,0);

  {
    double x, y, z;
    get_position (sp->rot, &x, &y, &z, !sp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 3,
                 (z - 0.5) * 15);

    gltrackball_rotate (sp->trackball);

    get_rotation (sp->rot, &x, &y, &z, !sp->button_down_p);

    if (y < 0.8) y = 0.8 - (y - 0.8);	/* always face forward */
    if (y > 1.2) y = 1.2 - (y - 1.2);

    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  {
    GLfloat scale = 20.0 / SCENE_HEIGHT;
    glScalef(scale, scale, scale);
  }

  glRotatef (180, 0, 0, 1);
  glTranslatef(-SCENE_WIDTH/2, -SCENE_HEIGHT/2, 0);
  glTranslatef(0, -150, 0);

  mi->polygon_count = 0;

  /* Update timer */
  if (real) {
	struct timeval tv;
	(void)gettimeofday(&tv, NULL);
	sp->time = (int) ((tv.tv_sec - sp->begintime)*1000 + tv.tv_usec/1000);
  } else {
	sp->time += MI_DELAY(mi) / 1000;
  }

  /* First pass: Move arms and strip out expired elements */
  for (traj = sp->head->next; traj != sp->head; traj = traj->next) {
	if (traj->status != PREDICTOR) {
	  /* Skip any elements that need further processing */
	  /* We could remove them, but there shoudn't be many and they
		 would be needed if we ever got the pattern refiller
		 working */
	  continue;
	}
	if (traj->start > future) { /* Lookahead to the end of the show */
	  future = traj->start;
	}
	if (sp->time < traj->start) { /* early */
	  continue;
	} else if (sp->time < traj->finish) { /* working */

	  /* Look for pattern name */
	  if(traj->pattern != NULL) {
		pattern=traj->pattern;
	  }

	  if (traj->type == Empty || traj->type == Full) {
		/* Only interested in hands on this pass */
/*		double angle = traj->angle + traj->spin * (sp->time - traj->start);*/
		double xd = 0, yd = 0;
		DXPoint p;

		/* Find the catching offset */
		if(traj->object != NULL) {
#if 0
                  /* #### not sure what this is doing, but I'm guessing
                     that the use of PERSPEC means this isn't needed
                     in the OpenGL version? -jwz
                   */
		  if(ObjectDefs[traj->object->type].handle > 0) {
			/* Handles Need to be oriented */
			xd = ObjectDefs[traj->object->type].handle *
			  PERSPEC * sin(angle);
			yd = ObjectDefs[traj->object->type].handle *
			  cos(angle);
		  } else
#endif
                    {
			/* Balls are always caught at the bottom */
			xd = 0;
			yd = -4;
		  }
		}
		p.x = (CUBIC(traj->xp, sp->time) - xd);
		p.y = (CUBIC(traj->yp, sp->time) + yd);
		reach_arm(mi, traj->hand, &p);

		/* Store updated hand position */
		traj->x = p.x + xd;
		traj->y = p.y - yd;
	  }
	  if (traj->type == Ball || traj->type == Full) {
		/* Only interested in objects on this pass */
		double x, y;
		Trace *s;

		if(traj->type == Full) {
		  /* Adjusted these in the first pass */
		  x = traj->x;
		  y = traj->y;
		} else {
		  x = CUBIC(traj->xp, sp->time);
		  y = CUBIC(traj->yp, sp->time);
		}

		ADD_ELEMENT(Trace, s, traj->object->trace->prev);
		s->x = x;
		s->y = y;
		s->angle = traj->angle + traj->spin * (sp->time - traj->start);
		s->divisions = traj->divisions;
		traj->object->tracelen++;
		traj->object->active = True;
	  }
	} else { /* expired */
	  Trajectory *n = traj;
	  traj=traj->prev;
	  trajectory_destroy(n);
	}
  }


  mi->polygon_count += show_figure(mi, False);
  mi->polygon_count += show_arms(mi);

  /* Draw Objects */
  glTranslatef(0, 0, ARMLENGTH);
  for (o = sp->objects->next; o != sp->objects; o = o->next) {
	if(o->active) {
	  mi->polygon_count += ObjectDefs[o->type].draw(mi, o->color, 
                                                        o->trace->prev);
	  o->active = False;
	}
  }


  /* Save pattern name so we can erase it when it changes */
  if(pattern != NULL && strcmp(sp->pattern, pattern) != 0 ) {
	free(sp->pattern);
	sp->pattern = strdup(pattern);

	if (MI_IS_VERBOSE(mi)) {
	  (void) fprintf(stderr, "Juggle[%d]: Running: %s\n",
					 MI_SCREEN(mi), sp->pattern);
	}
  }

  glColor3f (1, 1, 0);
  print_texture_label (mi->dpy, sp->font_data,
                       mi->xgwa.width, mi->xgwa.height,
                       1, sp->pattern);

#ifdef MEMTEST
  if((int)(sp->time/10) % 1000 == 0)
	(void) fprintf(stderr, "sbrk: %d\n", (int)sbrk(0));
#endif

  if (future < sp->time + 100 * THROW_CATCH_INTERVAL) {
	refill_juggle(mi);
  } else if (sp->time > 1<<30) { /* Hard Reset before the clock wraps */
	init_juggle(mi);
  }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("Juggler3D", juggler3d, juggle)

#endif /* USE_GL */
