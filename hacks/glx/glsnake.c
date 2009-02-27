/* glsnake.c - OpenGL imitation of Rubik's Snake
 * 
 * (c) 2001-2003 Jamie Wilkinson <jaq@spacepants.org>
 * (c) 2001-2003 Andrew Bennetts <andrew@puzzling.org> 
 * (c) 2001-2003 Peter Aylett <peter@ylett.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* HAVE_GLUT defined if we're building a standalone glsnake,
 * and not defined if we're building as an xscreensaver hack */
#ifdef HAVE_GLUT
# include <GL/glut.h>
#else
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* angles */
#define ZERO	  0.0
#define LEFT	 90.0
#define PIN    	180.0
#define RIGHT   270.0

#ifdef HAVE_GETTIMEOFDAY
#ifdef GETTIMEOFDAY_TWO_ARGS
# include <sys/time.h>
# include <time.h>
  typedef struct timeval snaketime;
# define GETSECS(t) ((t).tv_sec)
# define GETMSECS(t) ((t).tv_usec/1000)
#else /* GETTIMEOFDAY_TWO_ARGS */
# include <sys/time.h>
# include <time.h>
  typedef struct timeval snaketime;
# define GETSECS(t) ((t).tv_sec)
# define GETMSECS(t) ((t).tv_usec/1000)
#endif
#else /* HAVE_GETTIMEOFDAY */
#ifdef HAVE_FTIME
# include <sys/timeb.h>
  typedef struct timeb snaketime;
# define GETSECS(t) ((long)(t).time)
# define GETMSECS(t) ((t).millitm/1000)
#endif /* HAVE_FTIME */
#endif /* HAVE_GETTIMEOFDAY */

#include <math.h>

#ifndef M_SQRT1_2	/* Win32 doesn't have this constant  */
#define M_SQRT1_2 0.70710678118654752440084436210485
#endif

#define NODE_COUNT 24

#ifdef HAVE_GLUT
#define DEF_YANGVEL     0.10
#define DEF_ZANGVEL     0.14
#define DEF_EXPLODE     0.03
#define DEF_ANGVEL      1.0
#define DEF_ACCEL       0.1
#define DEF_STATICTIME  5000
#define DEF_ALTCOLOUR   0
#define DEF_TITLES      1
#define DEF_INTERACTIVE 0
#define DEF_ZOOM        25.0
#define DEF_WIREFRAME   0
#else
/* xscreensaver options doobies prefer strings */
#define DEF_YANGVEL     "0.10"
#define DEF_ZANGVEL     "0.14"
#define DEF_EXPLODE     "0.03"
#define DEF_ANGVEL      "1.0"
#define DEF_ACCEL       "0.1"
#define DEF_STATICTIME  "5000"
#define DEF_ALTCOLOUR   "False"
#define DEF_TITLES      "True"
#define DEF_INTERACTIVE "False"
#define DEF_ZOOM        "25.0"
#define DEF_WIREFRAME   "False"
#endif

/* static variables */
#ifndef HAVE_GLUT
# include <X11/Intrinsic.h>
#else
/* xscreensaver boolean type */
# define Bool int
#endif

static GLfloat explode;
static GLfloat accel;
static long statictime;
static GLfloat yspin = 0;
static GLfloat zspin = 0;
static GLfloat yangvel;
static GLfloat zangvel;
static Bool altcolour;
static Bool titles;
static Bool interactive;
static Bool wireframe;
static GLfloat zoom;
static GLfloat angvel;

#ifndef HAVE_GLUT
/* xscreensaver setup */
extern XtAppContext app;

#define PROGCLASS "glsnake"
#define HACK_INIT glsnake_init
#define HACK_DRAW glsnake_display
#define HACK_RESHAPE glsnake_reshape
#define sws_opts xlockmore_opts


/* xscreensaver defaults */
#define DEFAULTS "*delay:          30000                      \n" \
                 "*count:          30                         \n" \
                 "*showFPS:        False                      \n" \
                 "*wireframe:      False                      \n" \
                 "*explode:      " DEF_EXPLODE              " \n" \
                 "*angvel:       " DEF_ANGVEL               " \n" \
                 "*accel:        " DEF_ACCEL                " \n" \
                 "*statictime:   " DEF_STATICTIME           " \n" \
                 "*yangvel:      " DEF_YANGVEL              " \n" \
                 "*zangvel:      " DEF_ZANGVEL              " \n" \
                 "*altcolour:    " DEF_ALTCOLOUR            " \n" \
                 "*titles:         True                       \n" \
		 "*labelfont:   -*-times-bold-r-normal-*-180-*\n" \
                 "*zoom:         " DEF_ZOOM                 " \n" \



#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

static XrmOptionDescRec opts[] = {
    { "-explode", ".explode", XrmoptionSepArg, DEF_EXPLODE },
    { "-angvel", ".angvel", XrmoptionSepArg, DEF_ANGVEL },
    { "-accel", ".accel", XrmoptionSepArg, DEF_ACCEL },
    { "-statictime", ".statictime", XrmoptionSepArg, DEF_STATICTIME },
    { "-yangvel", ".yangvel", XrmoptionSepArg, DEF_YANGVEL },
    { "-zangvel", ".zangvel", XrmoptionSepArg, DEF_ZANGVEL },
    { "-altcolour", ".altcolour", XrmoptionNoArg, "True" },
    { "-no-altcolour", ".altcolour", XrmoptionNoArg, "False" },
    { "-titles", ".titles", XrmoptionNoArg, "True" },
    { "-no-titles", ".titles", XrmoptionNoArg, "False" },
    { "-zoom", ".zoom", XrmoptionSepArg, DEF_ZOOM },
    { "-wireframe", ".wireframe", XrmoptionNoArg, "true" },
    { "-no-wireframe", ".wireframe", XrmoptionNoArg, "false" },
};

static argtype vars[] = {
    {&explode, "explode", "Explode", DEF_EXPLODE, t_Float},
    {&angvel, "angvel", "Angular Velocity", DEF_ANGVEL, t_Float},
    {&accel, "accel", "Acceleration", DEF_ACCEL, t_Float},
    {&statictime, "statictime", "Static Time", DEF_STATICTIME, t_Int},
    {&yangvel, "yangvel", "Angular Velocity about Y axis", DEF_YANGVEL, t_Float},
    {&zangvel, "zangvel", "Angular Velocity about X axis", DEF_ZANGVEL, t_Float},
    {&interactive, "interactive", "Interactive", DEF_INTERACTIVE, t_Bool},
    {&altcolour, "altcolour", "Alternate Colour Scheme", DEF_ALTCOLOUR, t_Bool},
    {&titles, "titles", "Titles", DEF_TITLES, t_Bool},
    {&zoom, "zoom", "Zoom", DEF_ZOOM, t_Float},
    {&wireframe, "wireframe", "Wireframe", DEF_WIREFRAME, t_Bool},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};
#endif

struct model_s {
    char * name;
    float node[NODE_COUNT];
};

struct glsnake_cfg {
#ifndef HAVE_GLUT
    GLXContext * glx_context;
    XFontStruct * font;
    GLuint font_list;
#else
    /* font list number */
    int font;
#endif

    /* window id */
    int window;

    /* is a morph in progress? */
    int morphing;

    /* has the model been paused? */
    int paused;

    /* snake metrics */
    int is_cyclic;
    int is_legal;
    int last_turn;
    int debug;

    /* the shape of the model */
    float node[NODE_COUNT];

    /* currently selected node for interactive mode */
    int selected;

    /* models */
    int prev_model;
    int next_model;

    /* model morphing */
    int new_morph;

    /* colours */
    float colour[2][3];
    int next_colour;
    int prev_colour;

    /* timing variables */
    snaketime last_iteration;
    snaketime last_morph;

    /* window size */
    int width, height;
    int old_width, old_height;

    /* the id of the display lists for drawing a node */
    int node_solid, node_wire;

    /* is the window fullscreen? */
    int fullscreen;
};

#define COLOUR_CYCLIC 0
#define COLOUR_ACYCLIC 1
#define COLOUR_INVALID 2
#define COLOUR_AUTHENTIC 3

float colour[][2][3] = {
    /* cyclic - green */
    { { 0.4, 0.8, 0.2 },
      { 1.0, 1.0, 1.0 } },
    /* acyclic - blue */
    { { 0.3, 0.1, 0.9 },
      { 1.0, 1.0, 1.0 } },
    /* invalid - grey */
    { { 0.3, 0.1, 0.9 },
      { 1.0, 1.0, 1.0 } },
    /* authentic - purple and green */
    { { 0.38, 0.0, 0.55 },
      { 0.0,  0.5, 0.34 } }
};

struct model_s model[] = {
#define STRAIGHT_MODEL 0
    { "straight",
      { ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO }
    },
    /* the models in the Rubik's snake manual */
#define START_MODEL 1
    { "ball",
      { RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT,
	RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT,
	RIGHT, LEFT, RIGHT, LEFT }
    },
    { "snow",
      { RIGHT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, LEFT, RIGHT,
	RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT,
	RIGHT, LEFT, LEFT, LEFT }
    },
    { "propellor",
      { ZERO, ZERO, ZERO, RIGHT, LEFT, RIGHT, ZERO, LEFT, ZERO, ZERO,
	ZERO, RIGHT, LEFT, RIGHT, ZERO, LEFT, ZERO, ZERO, ZERO, RIGHT,
	LEFT, RIGHT, ZERO, LEFT }
    },
    { "flamingo",
      { ZERO, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, PIN, RIGHT, RIGHT, PIN,
	RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, RIGHT, ZERO, ZERO,
	ZERO, PIN }
    },
    { "cat",
      { ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN,
	ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO }
    },
    { "rooster",
      { ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO, LEFT, RIGHT, PIN, RIGHT,
	ZERO, PIN, PIN, ZERO, RIGHT, PIN, RIGHT, LEFT, ZERO, LEFT, ZERO,
	PIN }
    },
    /* These models were taken from Andrew and Peter's original snake.c
     * as well as some newer ones made up by Jamie, Andrew and Peter. */
    { "half balls",
      { LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT,
	LEFT, LEFT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT,
	RIGHT, LEFT, LEFT, LEFT }
    },
    { "zigzag1",
      { RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT,
	LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT,
	RIGHT, RIGHT, LEFT, LEFT }
    },
    { "zigzag2",
      { PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN,
	ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN }
    },
    { "zigzag3",
      { PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN,
	LEFT, PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN }
    },
    { "caterpillar",
      { RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT,
	LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN,
	LEFT, LEFT }
    },
    { "bow",
      { RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT,
	LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT,
	RIGHT, RIGHT, LEFT, LEFT }
    },
    { "turtle",
      { ZERO, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT,
	LEFT, RIGHT, LEFT, LEFT, PIN, LEFT, LEFT, LEFT, RIGHT, LEFT,
	RIGHT, RIGHT, RIGHT }
    },
    { "basket",
      { RIGHT, PIN, ZERO, ZERO, PIN, LEFT, ZERO, LEFT, LEFT, ZERO,
	LEFT, PIN, ZERO, ZERO, PIN, RIGHT, PIN, LEFT, PIN, ZERO, ZERO,
	PIN, LEFT }
    },
    { "thing",
      { PIN, RIGHT, LEFT, RIGHT, RIGHT, LEFT, PIN, LEFT, RIGHT, LEFT,
	LEFT, RIGHT, PIN, RIGHT, LEFT, RIGHT, RIGHT, LEFT, PIN, LEFT,
	RIGHT, LEFT, LEFT }
    },
    { "hexagon",
      { ZERO, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO, RIGHT, ZERO, ZERO,
	ZERO, ZERO, LEFT, ZERO, ZERO, RIGHT, ZERO, ZERO, ZERO, ZERO,
	LEFT, ZERO, ZERO, RIGHT }
    },
    { "tri1",
      { ZERO, ZERO, LEFT, RIGHT, ZERO, LEFT, ZERO, RIGHT, ZERO, ZERO,
	LEFT, RIGHT, ZERO, LEFT, ZERO, RIGHT, ZERO, ZERO, LEFT, RIGHT,
	ZERO, LEFT, ZERO, RIGHT }
    },
    { "triangle",
      { ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO,
	ZERO, ZERO, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO, LEFT, RIGHT }
    },
    { "flower",
      { ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO, LEFT, PIN, RIGHT,
	RIGHT, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO, LEFT, PIN,
	RIGHT, RIGHT, PIN }
    },
    { "crucifix",
      { ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN,
	PIN, ZERO, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN }
    },
    { "kayak",
      { PIN, RIGHT, LEFT, PIN, LEFT, PIN, ZERO, ZERO, RIGHT, PIN, LEFT,
	ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO,
	PIN, RIGHT }
    },
    { "bird",
      { ZERO, ZERO, ZERO, ZERO, RIGHT, RIGHT, ZERO, LEFT, PIN, RIGHT,
	ZERO, RIGHT, ZERO, RIGHT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT,
	LEFT, ZERO, PIN }
    },
    { "seal",
      { RIGHT, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, PIN, ZERO,
	LEFT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, LEFT, LEFT, PIN, RIGHT,
	RIGHT, LEFT }
    },
    { "dog",
      { ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, PIN,
	ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN }
    },
    { "frog",
      { RIGHT, RIGHT, LEFT, LEFT, RIGHT, PIN, RIGHT, PIN, LEFT, PIN,
	RIGHT, ZERO, LEFT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, LEFT,
	RIGHT, LEFT, LEFT }
    },
    { "quavers",
      { LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, ZERO, ZERO, ZERO,
	RIGHT, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, ZERO, LEFT, LEFT,
	RIGHT, LEFT, RIGHT, RIGHT }
    },
    { "fly",
      { LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, ZERO, PIN, ZERO, ZERO,
	LEFT, PIN, RIGHT, ZERO, ZERO, PIN, ZERO, LEFT, LEFT, RIGHT, LEFT,
	RIGHT, RIGHT }
    },
    { "puppy",
      { ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO,
	RIGHT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT,
	LEFT }
    },
    { "stars",
      { LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT,
	ZERO, ZERO, ZERO, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN,
	RIGHT, LEFT }
    },
    { "mountains",
      { RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN,
	LEFT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT,
	PIN, LEFT, PIN }
    },
    { "quad1",
      { RIGHT, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, LEFT, LEFT, PIN,
	LEFT, PIN, RIGHT, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, LEFT,
	LEFT, PIN, LEFT, PIN }
    },
    { "quad2",
      { ZERO, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, LEFT, LEFT, PIN,
	ZERO, PIN, ZERO, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, LEFT, LEFT,
	PIN, ZERO, PIN }
    },
    { "glasses",
      { ZERO, PIN, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT, ZERO, PIN,
	ZERO, PIN, ZERO, PIN, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT, ZERO,
	PIN, ZERO, PIN }
    },
    { "em",
      { ZERO, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN,
	ZERO, PIN, ZERO, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO,
	PIN, ZERO, PIN }
    },
    { "quad3",
      { ZERO, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, LEFT,
	ZERO, PIN, ZERO, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO,
	LEFT, ZERO, PIN }
    },
    { "vee",
      { ZERO, ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, ZERO,
	ZERO, PIN, ZERO, ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO,
	ZERO, ZERO, PIN }
    },
    { "square",
      { ZERO, ZERO, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT, ZERO, ZERO,
	ZERO, PIN, ZERO, ZERO, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT, ZERO,
	ZERO, ZERO, PIN }
    },
    { "eagle",
      { RIGHT, ZERO, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT, ZERO, ZERO,
	LEFT, PIN, RIGHT, ZERO, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT,
	ZERO, ZERO, LEFT, PIN }
    },
    { "volcano",
      { RIGHT, ZERO, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT,
	ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, RIGHT, RIGHT, PIN, LEFT,
	LEFT, RIGHT, ZERO, LEFT, PIN }
    },
    { "saddle",
      { RIGHT, ZERO, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, ZERO,
	LEFT, PIN, RIGHT, ZERO, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT,
	ZERO, LEFT, PIN }
    },
    { "c3d",
      { ZERO, ZERO, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, LEFT, ZERO,
	ZERO, PIN, ZERO, ZERO, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, LEFT,
	ZERO, ZERO, PIN }
    },
    { "block",
      { ZERO, ZERO, PIN, PIN, ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, PIN,
	RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, LEFT,
	PIN, RIGHT }
    },
    { "duck",
      { LEFT, PIN, LEFT, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, LEFT,
	PIN, RIGHT, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, LEFT, PIN,
	LEFT }
    },
    { "prayer",
      { RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, ZERO, ZERO,
	ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, ZERO, RIGHT, RIGHT, LEFT,
	RIGHT, LEFT, LEFT, LEFT, PIN }
    },
    { "giraffe",
      { ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, ZERO, RIGHT,
	RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, PIN, ZERO, LEFT, RIGHT,
	PIN, LEFT, LEFT, LEFT }
    },
    { "tie fighter",
      { PIN, LEFT, RIGHT, LEFT, LEFT, PIN, RIGHT, ZERO, RIGHT, LEFT,
	ZERO, PIN, LEFT, LEFT, RIGHT, RIGHT, RIGHT, PIN, LEFT, ZERO,
	LEFT, RIGHT, ZERO }
    },
    { "Strong Arms",
      { PIN, PIN, ZERO, ZERO, PIN, ZERO, ZERO, RIGHT, ZERO, RIGHT,
	RIGHT, PIN, RIGHT, RIGHT, ZERO, RIGHT, ZERO, ZERO, PIN, ZERO,
	ZERO, PIN, PIN, ZERO }
    },

    /* the following modesl were created during the slug/compsoc codefest
     * febrray 2003 */
    { "cool gegl",
      { PIN, PIN, ZERO, ZERO, RIGHT, ZERO, ZERO, PIN, PIN, ZERO, LEFT,
	ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, LEFT, RIGHT, PIN, ZERO,
	ZERO, ZERO }
    },
    { "knuckledusters",
      { ZERO, ZERO, ZERO, ZERO, PIN, RIGHT, ZERO, PIN, PIN, ZERO, PIN,
	PIN, ZERO, RIGHT, RIGHT, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO,
	RIGHT, ZERO }
    },
    { "k's turd",
      { RIGHT, RIGHT, PIN, RIGHT, LEFT, RIGHT, PIN, RIGHT, LEFT,
	RIGHT, PIN, RIGHT, LEFT, RIGHT, PIN, RIGHT, LEFT, RIGHT, PIN,
	RIGHT, LEFT, RIGHT, PIN, ZERO }
    },
    { "lightsabre",
      { ZERO, ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO }
    },
    { "not a stairway",
      { LEFT, ZERO, RIGHT, LEFT, RIGHT, ZERO, LEFT, RIGHT, LEFT, ZERO,
	RIGHT, LEFT, RIGHT, ZERO, LEFT, RIGHT, LEFT, ZERO, RIGHT, LEFT,
	RIGHT, ZERO, LEFT, ZERO }
    },
    { "arse gegl",
      { ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO,
	PIN, PIN, ZERO, RIGHT, LEFT, ZERO, PIN, ZERO, PIN, PIN, ZERO,
	PIN, ZERO }
    },
    { "box",
      { ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, PIN, ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO, ZERO }
    },
    { "kissy box",
      { PIN, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, PIN, ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO,
	ZERO, PIN, ZERO }
    },
    { "erect penis",     /* thanks benno */
      { PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, PIN,
	PIN, ZERO, ZERO, ZERO, RIGHT, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO }
    },
    { "flaccid penis",
      { PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, PIN,
	PIN, ZERO, ZERO, ZERO, RIGHT, PIN, ZERO, ZERO, ZERO, ZERO, ZERO,
	ZERO, ZERO }
    },
    { "vagina",
      { RIGHT, ZERO, ZERO, ZERO, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO,
	LEFT, ZERO, ZERO, ZERO, LEFT, ZERO, LEFT, PIN, LEFT, PIN, RIGHT,
	PIN, RIGHT, ZERO }
    },
    { "mask",
      { ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO, ZERO, PIN,
	ZERO, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO,
	ZERO, ZERO, ZERO }
    },
    { "poles or columns or something",
      { LEFT, RIGHT, LEFT, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO,
	ZERO, LEFT, RIGHT, LEFT, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO,
	ZERO, LEFT, ZERO }
    },
    { "crooked v",
      { ZERO, LEFT, ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO,
	ZERO, LEFT, ZERO, LEFT, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO,
	ZERO, ZERO, ZERO }
    },
    { "dog leg",
      { ZERO, LEFT, ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO, ZERO,
	LEFT, ZERO, RIGHT, ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO,
	ZERO, ZERO }
    },
    { "scrubby",
      { ZERO, ZERO, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO, ZERO, ZERO,
	LEFT, RIGHT, ZERO, ZERO, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO,
	LEFT, PIN, ZERO, ZERO }
    },
    { "voltron's eyes",
      { ZERO, ZERO, PIN, RIGHT, ZERO, LEFT, ZERO, ZERO, RIGHT, ZERO,
	LEFT, PIN, ZERO, ZERO, PIN, ZERO, LEFT, ZERO, RIGHT, LEFT, ZERO,
	RIGHT, ZERO, ZERO }
    },
    { "flying toaster",
      { PIN, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO,
	RIGHT, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, ZERO,
	PIN, ZERO }
    },
    { "dubbya",
      { PIN, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO,
	ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, ZERO,
	PIN, ZERO }
    },
    { "tap handle",
      { PIN, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO,
	LEFT, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, ZERO,
	PIN, ZERO }
    },
    { "wingnut",
      { PIN, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO,
	PIN, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, ZERO,
	PIN, ZERO }
    },
    { "tight twist",
      { RIGHT, ZERO, ZERO, LEFT, ZERO, LEFT, RIGHT, ZERO, RIGHT, LEFT,
	RIGHT, PIN, RIGHT, LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT,
	ZERO, ZERO, RIGHT, ZERO }
    },
    { "double helix",
      { RIGHT, ZERO, RIGHT, ZERO, RIGHT, ZERO, RIGHT, ZERO, RIGHT,
	ZERO, RIGHT, ZERO, RIGHT, LEFT, RIGHT, PIN, ZERO, RIGHT, ZERO,
	RIGHT, ZERO, RIGHT, ZERO, ZERO }
    },

    /* These models come from the website at 
     * http://www.geocities.com/stigeide/snake */
    { "Abstract",
        { RIGHT, LEFT, RIGHT, ZERO, PIN, ZERO, LEFT, RIGHT, LEFT, PIN, ZERO, ZERO, PIN, LEFT, RIGHT, LEFT, ZERO, PIN, ZERO, RIGHT, LEFT, RIGHT, ZERO }
    },
    { "AlanH1",
        { LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, PIN, RIGHT, LEFT, PIN }
    },
    { "AlanH2",
        { LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT }
    },
    { "AlanH3",
        { LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN }
    },
    { "AlanH4",
        { ZERO, ZERO, PIN, LEFT, RIGHT, LEFT, ZERO, RIGHT, LEFT, RIGHT, ZERO, PIN, ZERO, LEFT, RIGHT, LEFT, ZERO, RIGHT, LEFT, RIGHT, PIN, ZERO, ZERO }
    },
    { "Alien",
        { RIGHT, LEFT, RIGHT, PIN, ZERO, ZERO, PIN, RIGHT, LEFT, RIGHT, ZERO, PIN, PIN, ZERO, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, ZERO, PIN, PIN }
    },
    { "Angel",
        { ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT }
    },
    { "AnotherFigure",
        { LEFT, PIN, RIGHT, ZERO, ZERO, PIN, RIGHT, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, PIN, ZERO }
    },
    { "Ball",
        { LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT }
    },
    { "Basket",
        { ZERO, RIGHT, RIGHT, ZERO, RIGHT, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, LEFT, RIGHT, PIN, LEFT, LEFT, ZERO, LEFT }
    },
    { "Beetle",
        { PIN, LEFT, RIGHT, ZERO, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, ZERO, LEFT, RIGHT, PIN, RIGHT }
    },
    { "Bone",
        { PIN, PIN, LEFT, ZERO, PIN, PIN, ZERO, LEFT, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, RIGHT, PIN, PIN }
    },
    { "Bow",
        { LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT }
    },
    { "Bra",
        { RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT }
    },
    { "BronchoSaurian",
        { ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, PIN }
    },
    { "Cactus",
        { PIN, LEFT, ZERO, PIN, PIN, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, RIGHT, PIN, LEFT, ZERO, ZERO, RIGHT, PIN, LEFT, PIN, ZERO, ZERO }
    },
    { "Camel",
        { RIGHT, ZERO, PIN, RIGHT, PIN, RIGHT, ZERO, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, ZERO, RIGHT, PIN, RIGHT, ZERO, ZERO, LEFT }
    },
    { "Candlestick",
        { LEFT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, RIGHT }
    },
    { "Cat",
        { ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO }
    },
    { "Cave",
        { RIGHT, ZERO, ZERO, PIN, LEFT, ZERO, PIN, PIN, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, LEFT, PIN, RIGHT, RIGHT, LEFT, PIN, ZERO, ZERO }
    },
    { "Chains",
        { PIN, ZERO, ZERO, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO }
    },
    { "Chair",
        { RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, RIGHT, ZERO, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, LEFT, RIGHT, LEFT, LEFT }
    },
    { "Chick",
        { RIGHT, RIGHT, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, LEFT, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, LEFT, LEFT }
    },
    { "Clockwise",
        { RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT }
    },
    { "Cobra",
        { ZERO, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, LEFT, ZERO, LEFT, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, RIGHT }
    },
    { "Cobra2",
        { LEFT, ZERO, PIN, ZERO, PIN, LEFT, ZERO, PIN, ZERO, LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, PIN, ZERO, RIGHT, PIN, ZERO, PIN, ZERO, RIGHT }
    },
    { "Cobra3",
        { ZERO, LEFT, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, ZERO, ZERO, LEFT, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, LEFT }
    },
    { "Compact1",
        { ZERO, ZERO, PIN, ZERO, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO, PIN, PIN, ZERO, ZERO, LEFT, PIN }
    },
    { "Compact2",
        { LEFT, PIN, RIGHT, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, ZERO, ZERO }
    },
    { "Compact3",
        { ZERO, PIN, ZERO, PIN, PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN }
    },
    { "Compact4",
        { PIN, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, RIGHT, PIN, LEFT, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, ZERO }
    },
    { "Compact5",
        { LEFT, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, RIGHT, PIN, RIGHT, PIN, LEFT }
    },
    { "Contact",
        { PIN, ZERO, ZERO, PIN, LEFT, LEFT, PIN, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, PIN, RIGHT, RIGHT, PIN, ZERO, ZERO, PIN, RIGHT, PIN }
    },
    { "Contact2",
        { RIGHT, PIN, ZERO, LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, PIN, LEFT, PIN, RIGHT, PIN, ZERO, LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, PIN, LEFT }
    },
    { "Cook",
        { ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, RIGHT, LEFT, PIN, LEFT, ZERO, PIN, PIN, ZERO, LEFT, PIN, LEFT, RIGHT, ZERO, RIGHT, ZERO, PIN }
    },
    { "Counterclockwise",
        { LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT }
    },
    { "Cradle",
        { LEFT, LEFT, ZERO, PIN, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, PIN, ZERO, RIGHT, RIGHT, LEFT, LEFT, ZERO, ZERO, RIGHT }
    },
    { "Crankshaft",
        { ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, LEFT, PIN, RIGHT, ZERO, ZERO, ZERO, PIN, RIGHT }
    },
    { "Cross",
        { ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN }
    },
    { "Cross2",
        { ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO, ZERO, PIN, PIN, ZERO }
    },
    { "Cross3",
        { ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO, ZERO, PIN, PIN, ZERO }
    },
    { "CrossVersion1",
        { PIN, ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, PIN, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN }
    },
    { "CrossVersion2",
        { RIGHT, LEFT, PIN, LEFT, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, LEFT, LEFT, PIN, LEFT, RIGHT }
    },
    { "Crown",
        { LEFT, ZERO, PIN, ZERO, RIGHT, ZERO, ZERO, LEFT, ZERO, PIN, ZERO, RIGHT, LEFT, ZERO, PIN, ZERO, RIGHT, ZERO, ZERO, LEFT, ZERO, PIN, ZERO }
    },
    { "DNAStrand",
        { RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT }
    },
    { "Diamond",
        { ZERO, RIGHT, ZERO, ZERO, LEFT, ZERO, ZERO, RIGHT, PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, ZERO, ZERO, RIGHT, ZERO, ZERO, LEFT }
    },
    { "Dog",
        { RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, ZERO, LEFT, RIGHT }
    },
    { "DogFace",
        { ZERO, ZERO, PIN, PIN, ZERO, LEFT, LEFT, RIGHT, PIN, ZERO, PIN, PIN, ZERO, PIN, LEFT, RIGHT, RIGHT, ZERO, PIN, PIN, ZERO, ZERO, PIN }
    },
    { "DoublePeak",
        { ZERO, ZERO, PIN, ZERO, ZERO, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, LEFT, ZERO, PIN, ZERO, RIGHT, RIGHT, LEFT, PIN, LEFT, RIGHT }
    },
    { "DoubleRoof",
        { ZERO, LEFT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, ZERO, LEFT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT }
    },
    { "DoubleToboggan",
        { ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, ZERO, ZERO, PIN }
    },
    { "Doubled",
        { LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, LEFT, ZERO, LEFT, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT }
    },
    { "Doubled1",
        { LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, ZERO, RIGHT, ZERO, RIGHT, ZERO, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT }
    },
    { "Doubled2",
        { LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, LEFT, RIGHT, ZERO, RIGHT, LEFT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT }
    },
    { "DumblingSpoon",
        { PIN, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, RIGHT, ZERO, ZERO, ZERO, ZERO }
    },
    { "Embrace",
        { PIN, ZERO, ZERO, PIN, RIGHT, PIN, LEFT, PIN, ZERO, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, ZERO, PIN, RIGHT, PIN, LEFT, PIN, ZERO }
    },
    { "EndlessBelt",
        { ZERO, RIGHT, LEFT, ZERO, ZERO, ZERO, LEFT, RIGHT, ZERO, PIN, RIGHT, LEFT, ZERO, LEFT, RIGHT, LEFT, PIN, LEFT, RIGHT, LEFT, ZERO, LEFT, RIGHT }
    },
    { "Entrance",
        { LEFT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT }
    },
    { "Esthetic",
        { LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT }
    },
    { "Explotion",
        { RIGHT, RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT }
    },
    { "F-ZeroXCar",
        { RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT }
    },
    { "Face",
        { ZERO, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT }
    },
    { "Fantasy",
        { LEFT, LEFT, RIGHT, PIN, ZERO, RIGHT, ZERO, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, ZERO, LEFT, ZERO, PIN, LEFT, RIGHT, RIGHT, RIGHT, PIN }
    },
    { "Fantasy1",
        { PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, LEFT, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN }
    },
    { "FaserGun",
        { ZERO, ZERO, LEFT, RIGHT, PIN, RIGHT, ZERO, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, ZERO, RIGHT, PIN, RIGHT, RIGHT, ZERO, PIN }
    },
    { "FelixW",
        { ZERO, RIGHT, ZERO, PIN, LEFT, ZERO, LEFT, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, LEFT, RIGHT, ZERO, RIGHT, PIN, ZERO, LEFT, ZERO }
    },
    { "Flamingo",
        { ZERO, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, PIN, LEFT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, LEFT, ZERO, ZERO, ZERO, PIN }
    },
    { "FlatOnTheTop",
        { ZERO, PIN, PIN, ZERO, PIN, RIGHT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, PIN }
    },
    { "Fly",
        { ZERO, LEFT, PIN, RIGHT, ZERO, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT }
    },
    { "Fountain",
        { LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, PIN, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, PIN, RIGHT, PIN }
    },
    { "Frog",
        { LEFT, LEFT, RIGHT, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO, RIGHT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, RIGHT, LEFT, RIGHT, RIGHT }
    },
    { "Frog2",
        { LEFT, ZERO, LEFT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, LEFT, RIGHT, PIN, LEFT, LEFT, RIGHT, ZERO, RIGHT }
    },
    { "Furby",
        { PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, RIGHT, PIN, LEFT, ZERO, PIN, ZERO, ZERO, PIN }
    },
    { "Gate",
        { ZERO, ZERO, PIN, ZERO, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, LEFT, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, RIGHT, ZERO, PIN, PIN, ZERO }
    },
    { "Ghost",
        { LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, LEFT, RIGHT }
    },
    { "Globus",
        { RIGHT, LEFT, ZERO, PIN, LEFT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, PIN, ZERO, RIGHT, LEFT, ZERO }
    },
    { "Grotto",
        { PIN, PIN, ZERO, LEFT, RIGHT, LEFT, ZERO, PIN, RIGHT, PIN, LEFT, ZERO, ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, PIN, ZERO, RIGHT, LEFT, RIGHT }
    },
    { "H",
        { PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO, PIN, LEFT, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, PIN, PIN, ZERO }
    },
    { "HeadOfDevil",
        { PIN, ZERO, RIGHT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, RIGHT, LEFT, ZERO, ZERO }
    },
    { "Heart",
        { RIGHT, ZERO, ZERO, ZERO, PIN, LEFT, PIN, LEFT, RIGHT, RIGHT, ZERO, PIN, ZERO, LEFT, LEFT, RIGHT, PIN, RIGHT, PIN, ZERO, ZERO, ZERO, LEFT }
    },
    { "Heart2",
        { ZERO, PIN, ZERO, ZERO, LEFT, ZERO, LEFT, ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO, RIGHT, ZERO, RIGHT, ZERO, ZERO, PIN, ZERO }
    },
    { "Hexagon",
        { ZERO, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO, RIGHT, ZERO, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO, RIGHT, ZERO, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO }
    },
    { "HoleInTheMiddle1",
        { ZERO, LEFT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, LEFT, RIGHT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, RIGHT }
    },
    { "HoleInTheMiddle2",
        { ZERO, LEFT, RIGHT, ZERO, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT, RIGHT, ZERO, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, RIGHT }
    },
    { "HouseBoat",
        { RIGHT, RIGHT, PIN, LEFT, LEFT, LEFT, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, RIGHT, PIN }
    },
    { "HouseByHouse",
        { LEFT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, RIGHT }
    },
    { "Infinity",
        { LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT }
    },
    { "Integral",
        { RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT }
    },
    { "Iron",
        { ZERO, ZERO, ZERO, ZERO, PIN, RIGHT, ZERO, RIGHT, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, RIGHT, ZERO, RIGHT }
    },
    { "JustSquares",
        { RIGHT, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, LEFT, PIN, RIGHT, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, LEFT }
    },
    { "Kink",
        { ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO }
    },
    { "Knot",
        { LEFT, LEFT, PIN, LEFT, ZERO, LEFT, RIGHT, LEFT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, RIGHT, LEFT, RIGHT, ZERO, RIGHT, PIN, RIGHT, RIGHT, LEFT }
    },
    { "Leaf",
        { ZERO, PIN, PIN, ZERO, ZERO, LEFT, ZERO, LEFT, ZERO, ZERO, PIN, ZERO, ZERO, RIGHT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO }
    },
    { "LeftAsRight",
        { RIGHT, PIN, LEFT, RIGHT, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, RIGHT, LEFT, RIGHT, PIN, LEFT }
    },
    { "Long-necked",
        { PIN, ZERO, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, ZERO, PIN, ZERO, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO, PIN, PIN, ZERO }
    },
    { "LunaModule",
        { PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT }
    },
    { "MagnifyingGlass",
        { ZERO, ZERO, PIN, ZERO, LEFT, ZERO, PIN, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, ZERO, ZERO }
    },
    { "Mask",
        { ZERO, ZERO, ZERO, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO, PIN, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT }
    },
    { "Microscope",
        { PIN, PIN, ZERO, ZERO, PIN, ZERO, RIGHT, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, LEFT, ZERO, PIN, PIN, ZERO, PIN, PIN }
    },
    { "Mirror",
        { PIN, RIGHT, LEFT, ZERO, PIN, PIN, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, PIN, ZERO, ZERO, LEFT, RIGHT, PIN, RIGHT, ZERO, PIN, PIN, ZERO }
    },
    { "MissPiggy",
        { ZERO, LEFT, LEFT, PIN, RIGHT, ZERO, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, ZERO, LEFT, PIN, RIGHT, RIGHT, ZERO, RIGHT }
    },
    { "Mole",
        { ZERO, RIGHT, ZERO, RIGHT, LEFT, RIGHT, PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, LEFT, RIGHT, LEFT, ZERO, LEFT, ZERO, RIGHT, RIGHT, PIN, LEFT }
    },
    { "Monk",
        { LEFT, ZERO, PIN, PIN, ZERO, LEFT, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT }
    },
    { "Mountain",
        { ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, LEFT, PIN, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO }
    },
    { "Mountains",
        { ZERO, PIN, ZERO, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, ZERO, PIN, ZERO }
    },
    { "MouseWithoutTail",
        { ZERO, PIN, PIN, ZERO, LEFT, ZERO, PIN, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO }
    },
    { "Mushroom",
        { PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, ZERO, LEFT, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, LEFT, ZERO, PIN }
    },
    { "Necklace",
        { ZERO, ZERO, LEFT, ZERO, ZERO, ZERO, LEFT, ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO, RIGHT, ZERO, ZERO, ZERO, RIGHT, ZERO, ZERO }
    },
    { "NestledAgainst",
        { LEFT, ZERO, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT }
    },
    { "NoClue",
        { ZERO, RIGHT, PIN, LEFT, LEFT, LEFT, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, ZERO, RIGHT, RIGHT, RIGHT, PIN, LEFT, ZERO }
    },
    { "Noname",
        { LEFT, PIN, RIGHT, PIN, RIGHT, ZERO, PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT }
    },
    { "Obelisk",
        { PIN, ZERO, ZERO, ZERO, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, ZERO, ZERO, ZERO }
    },
    { "Ostrich",
        { ZERO, ZERO, PIN, PIN, ZERO, LEFT, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, PIN }
    },
    { "Ostrich2",
        { PIN, PIN, ZERO, PIN, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO }
    },
    { "PairOfGlasses",
        { ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO, LEFT, ZERO, PIN, ZERO, RIGHT, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO }
    },
    { "Parrot",
        { ZERO, ZERO, ZERO, ZERO, RIGHT, RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, RIGHT, ZERO, RIGHT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, LEFT, ZERO, PIN }
    },
    { "Penis",
        { PIN, PIN, RIGHT, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, LEFT, PIN, PIN }
    },
    { "PictureCommingSoon",
        { LEFT, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, RIGHT, PIN, RIGHT, LEFT, ZERO, RIGHT, RIGHT }
    },
    { "Pitti",
        { LEFT, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, RIGHT }
    },
    { "Plait",
        { LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, LEFT }
    },
    { "Platform",
        { RIGHT, PIN, ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, LEFT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, RIGHT }
    },
    { "PodRacer",
        { ZERO, PIN, ZERO, PIN, RIGHT, PIN, ZERO, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, PIN, ZERO, ZERO, LEFT, ZERO, PIN, LEFT }
    },
    { "Pokemon",
        { LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT }
    },
    { "Prawn",
        { RIGHT, PIN, ZERO, PIN, RIGHT, ZERO, PIN, PIN, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, PIN, PIN, ZERO, LEFT, PIN, ZERO, PIN, LEFT }
    },
    { "Propeller",
        { ZERO, ZERO, ZERO, RIGHT, ZERO, LEFT, RIGHT, LEFT, ZERO, ZERO, ZERO, RIGHT, ZERO, LEFT, RIGHT, LEFT, ZERO, ZERO, ZERO, RIGHT, ZERO, LEFT, RIGHT }
    },
    { "Pyramid",
        { ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, RIGHT, LEFT, LEFT, LEFT, PIN, RIGHT, RIGHT, RIGHT, LEFT }
    },
    { "QuarterbackTiltedAndReadyToHut",
        { PIN, ZERO, RIGHT, RIGHT, LEFT, RIGHT, PIN, RIGHT, LEFT, RIGHT, ZERO, PIN, ZERO, LEFT, RIGHT, LEFT, PIN, LEFT, RIGHT, LEFT, LEFT, ZERO, PIN }
    },
    { "Ra",
        { PIN, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, ZERO, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT }
    },
    { "Rattlesnake",
        { LEFT, ZERO, LEFT, ZERO, LEFT, ZERO, LEFT, LEFT, ZERO, LEFT, ZERO, LEFT, ZERO, LEFT, RIGHT, ZERO, PIN, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT }
    },
    { "Revelation",
        { ZERO, ZERO, ZERO, PIN, ZERO, ZERO, PIN, RIGHT, LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, PIN, ZERO, ZERO, PIN }
    },
    { "Revolution1",
        { LEFT, LEFT, PIN, RIGHT, ZERO, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, ZERO, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT }
    },
    { "Ribbon",
        { RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT }
    },
    { "Rocket",
        { RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, RIGHT, ZERO, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, ZERO, LEFT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT }
    },
    { "Roofed",
        { ZERO, LEFT, PIN, RIGHT, ZERO, PIN, LEFT, ZERO, PIN, ZERO, RIGHT, PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, LEFT, ZERO, PIN, ZERO, RIGHT }
    },
    { "Roofs",
        { PIN, PIN, RIGHT, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, LEFT, PIN, PIN }
    },
    { "RowHouses",
        { RIGHT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT }
    },
    { "Sculpture",
        { RIGHT, LEFT, PIN, ZERO, ZERO, ZERO, LEFT, RIGHT, LEFT, PIN, ZERO, ZERO, PIN, LEFT, RIGHT, LEFT, ZERO, ZERO, ZERO, PIN, LEFT, RIGHT, LEFT }
    },
    { "Seal",
        { LEFT, LEFT, LEFT, PIN, RIGHT, RIGHT, RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, PIN, ZERO, LEFT }
    },
    { "Seal2",
        { RIGHT, PIN, ZERO, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, PIN, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO }
    },
    { "Sheep",
        { RIGHT, LEFT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, LEFT, LEFT, RIGHT, LEFT }
    },
    { "Shelter",
        { LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, ZERO, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO, RIGHT }
    },
    { "Ship",
        { PIN, RIGHT, LEFT, LEFT, LEFT, LEFT, PIN, RIGHT, RIGHT, RIGHT, RIGHT, LEFT, ZERO, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, LEFT, ZERO, PIN, PIN }
    },
    { "Shpongle",
        { LEFT, RIGHT, ZERO, RIGHT, LEFT, RIGHT, ZERO, RIGHT, LEFT, RIGHT, ZERO, RIGHT, LEFT, RIGHT, ZERO, RIGHT, LEFT, RIGHT, ZERO, RIGHT, LEFT, RIGHT, ZERO }
    },
    { "Slide",
        { LEFT, RIGHT, LEFT, RIGHT, ZERO, LEFT, RIGHT, LEFT, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, RIGHT, LEFT }
    },
    { "SmallShip",
        { ZERO, LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT, RIGHT, ZERO, LEFT, RIGHT, ZERO, LEFT, RIGHT, ZERO, RIGHT, LEFT, ZERO, LEFT, RIGHT, ZERO, LEFT }
    },
    { "SnakeReadyToStrike",
        { LEFT, ZERO, LEFT, ZERO, LEFT, ZERO, LEFT, RIGHT, ZERO, RIGHT, ZERO, RIGHT, ZERO, LEFT, ZERO, ZERO, ZERO, PIN, ZERO, ZERO, ZERO, ZERO, LEFT }
    },
    { "Snakes14",
        { RIGHT, RIGHT, PIN, ZERO, RIGHT, LEFT, RIGHT, ZERO, ZERO, ZERO, RIGHT, PIN, LEFT, PIN, ZERO, PIN, LEFT, PIN, RIGHT, ZERO, ZERO, LEFT, RIGHT }
    },
    { "Snakes15",
        { ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, LEFT, PIN, RIGHT, ZERO, ZERO, ZERO, PIN, RIGHT }
    },
    { "Snakes18",
        { PIN, PIN, LEFT, PIN, LEFT, PIN, RIGHT, ZERO, RIGHT, PIN, RIGHT, ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, PIN, PIN, ZERO, ZERO, PIN }
    },
    { "Snowflake",
        { LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, RIGHT }
    },
    { "Snowman",
        { ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO }
    },
    { "Source",
        { PIN, RIGHT, ZERO, PIN, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN }
    },
    { "Spaceship",
        { PIN, PIN, RIGHT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, PIN, RIGHT, PIN, PIN }
    },
    { "Spaceship2",
        { PIN, PIN, LEFT, PIN, LEFT, PIN, RIGHT, ZERO, PIN, PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, ZERO, LEFT, PIN, LEFT, LEFT, PIN, PIN }
    },
    { "Speedboat",
        { LEFT, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, LEFT, ZERO, ZERO, PIN, ZERO, ZERO, RIGHT, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, RIGHT }
    },
    { "Speedboat2",
        { PIN, RIGHT, LEFT, LEFT, RIGHT, RIGHT, RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, LEFT, LEFT, RIGHT, RIGHT, LEFT, PIN, ZERO, RIGHT, PIN, LEFT }
    },
    { "Spider",
        { RIGHT, RIGHT, ZERO, ZERO, LEFT, RIGHT, LEFT, PIN, ZERO, LEFT, ZERO, PIN, PIN, ZERO, RIGHT, ZERO, PIN, RIGHT, LEFT, RIGHT, ZERO, ZERO, LEFT }
    },
    { "Spitzbergen",
        { PIN, LEFT, ZERO, RIGHT, RIGHT, LEFT, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, ZERO, PIN, RIGHT, LEFT, LEFT, ZERO }
    },
    { "Square",
        { ZERO, ZERO, LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, ZERO, LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, ZERO, LEFT, LEFT, PIN, RIGHT, RIGHT, ZERO, ZERO }
    },
    { "SquareHole",
        { PIN, ZERO, PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, PIN }
    },
    { "Stage",
        { RIGHT, ZERO, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, LEFT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, ZERO }
    },
    { "Stairs",
        { ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO }
    },
    { "Stairs2",
        { ZERO, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO }
    },
    { "Straight",
        { ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO }
    },
    { "Swan",
        { ZERO, PIN, ZERO, PIN, LEFT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, PIN, LEFT, PIN, LEFT, RIGHT }
    },
    { "Swan2",
        { PIN, ZERO, PIN, RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, RIGHT, PIN, ZERO, ZERO, ZERO, ZERO, ZERO, PIN, PIN }
    },
    { "Swan3",
        { PIN, PIN, ZERO, ZERO, ZERO, RIGHT, ZERO, RIGHT, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, ZERO, ZERO, RIGHT, ZERO, RIGHT }
    },
    { "Symbol",
        { RIGHT, RIGHT, PIN, ZERO, PIN, PIN, ZERO, PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, ZERO, PIN, PIN, ZERO, PIN, LEFT, LEFT, RIGHT }
    },
    { "Symmetry",
        { RIGHT, ZERO, LEFT, RIGHT, LEFT, ZERO, LEFT, RIGHT, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, LEFT, RIGHT, ZERO, RIGHT, LEFT, RIGHT, ZERO, LEFT }
    },
    { "Symmetry2",
        { ZERO, PIN, LEFT, LEFT, PIN, ZERO, ZERO, LEFT, PIN, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, PIN, LEFT }
    },
    { "TableFireworks",
        { ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, RIGHT, PIN, RIGHT, LEFT, ZERO, RIGHT, PIN }
    },
    { "Tapering",
        { ZERO, ZERO, RIGHT, LEFT, PIN, LEFT, ZERO, PIN, PIN, ZERO, LEFT, PIN, RIGHT, ZERO, PIN, PIN, ZERO, RIGHT, PIN, RIGHT, LEFT, ZERO, ZERO }
    },
    { "TaperingTurned",
        { ZERO, ZERO, RIGHT, LEFT, PIN, LEFT, ZERO, PIN, PIN, ZERO, LEFT, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, RIGHT, PIN, RIGHT, LEFT, ZERO, ZERO }
    },
    { "TeaLightStick",
        { RIGHT, ZERO, PIN, PIN, ZERO, LEFT, RIGHT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN }
    },
    { "Tent",
        { RIGHT, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO, LEFT, RIGHT, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO }
    },
    { "Terraces",
        { RIGHT, LEFT, ZERO, RIGHT, LEFT, PIN, LEFT, LEFT, PIN, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT, RIGHT, PIN, RIGHT, RIGHT, PIN, RIGHT, LEFT }
    },
    { "Terrier",
        { PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO }
    },
    { "Three-Legged",
        { RIGHT, ZERO, LEFT, RIGHT, ZERO, LEFT, PIN, RIGHT, ZERO, RIGHT, ZERO, PIN, ZERO, LEFT, ZERO, LEFT, PIN, RIGHT, ZERO, LEFT, RIGHT, ZERO, LEFT }
    },
    { "ThreePeaks",
        { RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO, ZERO, LEFT }
    },
    { "ToTheFront",
        { ZERO, PIN, RIGHT, LEFT, LEFT, LEFT, PIN, RIGHT, LEFT, ZERO, PIN, PIN, ZERO, LEFT, LEFT, PIN, ZERO, LEFT, RIGHT, ZERO, PIN, ZERO, LEFT }
    },
    { "Top",
        { PIN, LEFT, LEFT, PIN, LEFT, ZERO, ZERO, RIGHT, LEFT, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, PIN, RIGHT, PIN, RIGHT, RIGHT, PIN, ZERO }
    },
    { "Transport",
        { PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, ZERO, ZERO }
    },
    { "Triangle",
        { ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, RIGHT, LEFT, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, RIGHT }
    },
    { "Tripple",
        { PIN, ZERO, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, PIN, ZERO, PIN, LEFT, PIN, RIGHT, PIN, ZERO, PIN, LEFT, PIN, LEFT, PIN, RIGHT, PIN }
    },
    { "Turtle",
        { RIGHT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, PIN, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, ZERO, LEFT, RIGHT, ZERO }
    },
    { "Twins",
        { ZERO, PIN, ZERO, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, PIN, ZERO, ZERO, PIN, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, ZERO, PIN, ZERO, ZERO }
    },
    { "TwoSlants",
        { ZERO, PIN, ZERO, ZERO, PIN, PIN, ZERO, PIN, ZERO, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO, ZERO, RIGHT, PIN }
    },
    { "TwoWings",
        { PIN, LEFT, ZERO, RIGHT, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, PIN, PIN, ZERO, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, LEFT, ZERO }
    },
    { "UFO",
        { LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, PIN, LEFT, LEFT, LEFT, RIGHT, LEFT, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT }
    },
    { "USSEnterprice",
        { LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, ZERO, PIN, PIN, ZERO, RIGHT, LEFT, ZERO, PIN, PIN, ZERO, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, ZERO }
    },
    { "UpAndDown",
        { ZERO, PIN, ZERO, PIN, ZERO, PIN, LEFT, PIN, RIGHT, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, PIN, LEFT, PIN, RIGHT, PIN, ZERO }
    },
    { "Upright",
        { ZERO, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, PIN, ZERO, ZERO, LEFT, PIN, RIGHT, ZERO, ZERO, PIN, RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, ZERO }
    },
    { "Upside-down",
        { PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, RIGHT, RIGHT, LEFT, LEFT, PIN, RIGHT, RIGHT, LEFT, LEFT, ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN }
    },
    { "Valley",
        { ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, RIGHT, ZERO, PIN, ZERO, LEFT, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, PIN, LEFT, ZERO }
    },
    { "Viaduct",
        { PIN, RIGHT, PIN, LEFT, PIN, ZERO, ZERO, PIN, RIGHT, ZERO, RIGHT, RIGHT, ZERO, RIGHT, PIN, ZERO, ZERO, PIN, LEFT, PIN, RIGHT, PIN, ZERO }
    },
    { "View",
        { ZERO, RIGHT, PIN, LEFT, PIN, RIGHT, ZERO, ZERO, RIGHT, PIN, LEFT, LEFT, RIGHT, RIGHT, PIN, LEFT, ZERO, ZERO, LEFT, PIN, RIGHT, PIN, LEFT }
    },
    { "Waterfall",
        { LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT, PIN, LEFT, ZERO, RIGHT }
    },
    { "WindWheel",
        { PIN, RIGHT, RIGHT, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, ZERO }
    },
    { "Window",
        { PIN, ZERO, PIN, PIN, ZERO, ZERO, PIN, ZERO, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO, PIN, ZERO, PIN, PIN, ZERO, ZERO, ZERO, ZERO }
    },
    { "WindowToTheWorld",
        { PIN, LEFT, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO, RIGHT, PIN, LEFT, ZERO, PIN, ZERO, ZERO, PIN, ZERO, ZERO, PIN, ZERO }
    },
    { "Windshield",
        { PIN, PIN, ZERO, RIGHT, PIN, LEFT, LEFT, PIN, RIGHT, ZERO, PIN, ZERO, LEFT, PIN, RIGHT, RIGHT, PIN, LEFT, ZERO, PIN, PIN, ZERO, PIN }
    },
    { "WingNut",
        { ZERO, ZERO, ZERO, ZERO, PIN, RIGHT, RIGHT, RIGHT, PIN, RIGHT, LEFT, PIN, LEFT, RIGHT, PIN, RIGHT, RIGHT, RIGHT, PIN, ZERO, ZERO, ZERO, ZERO }
    },
    { "Wings2",
        { RIGHT, ZERO, PIN, ZERO, LEFT, PIN, RIGHT, PIN, RIGHT, LEFT, RIGHT, RIGHT, LEFT, LEFT, RIGHT, LEFT, PIN, LEFT, PIN, RIGHT, ZERO, PIN, ZERO }
    },
    { "WithoutName",
        { PIN, RIGHT, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN, ZERO, PIN, RIGHT, PIN, LEFT, PIN, ZERO, PIN, RIGHT, RIGHT, PIN, LEFT, LEFT, PIN }
    },
    { "Wolf",
        { ZERO, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, PIN, ZERO, PIN, PIN, ZERO, PIN, ZERO, ZERO, ZERO, PIN, PIN, ZERO, ZERO, ZERO, PIN }
    },
    { "X",
        { LEFT, ZERO, ZERO, PIN, LEFT, RIGHT, RIGHT, PIN, LEFT, RIGHT, ZERO, PIN, PIN, ZERO, LEFT, RIGHT, PIN, LEFT, LEFT, RIGHT, PIN, ZERO, ZERO }
    },
};

int models = (sizeof(model) / sizeof(struct model_s));

#define VOFFSET 0.045

#define X_MASK	1
#define Y_MASK	2
#define Z_MASK	4

/* the connecting string that holds the snake together */
#define MAGICAL_RED_STRING 0

#define GETSCALAR(vec,mask) ((vec)==(mask) ? 1 : ((vec)==-(mask) ? -1 : 0 ))

#ifndef MAX
# define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
# define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#define RAND(n) ((random() & 0x7fffffff) % ((long) (n)))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

/* the triangular prism what makes up the basic unit */
float solid_prism_v[][3] = {
    /* first corner, bottom left front */
    { VOFFSET, VOFFSET, 1.0 },
    { VOFFSET, 0.00, 1.0 - VOFFSET },
    { 0.00, VOFFSET, 1.0 - VOFFSET },
    /* second corner, rear */
    { VOFFSET, VOFFSET, 0.00 },
    { VOFFSET, 0.00, VOFFSET },
    { 0.00, VOFFSET, VOFFSET },
    /* third, right front */
    { 1.0 - VOFFSET / M_SQRT1_2, VOFFSET, 1.0 },
    { 1.0 - VOFFSET / M_SQRT1_2, 0.0, 1.0 - VOFFSET },
    { 1.0 - VOFFSET * M_SQRT1_2, VOFFSET, 1.0 - VOFFSET },
    /* fourth, right rear */
    { 1.0 - VOFFSET / M_SQRT1_2, VOFFSET, 0.0 },
    { 1.0 - VOFFSET / M_SQRT1_2, 0.0, VOFFSET },
    { 1.0 - VOFFSET * M_SQRT1_2, VOFFSET, VOFFSET },
    /* fifth, upper front */
    { VOFFSET, 1.0 - VOFFSET / M_SQRT1_2, 1.0 },
    { VOFFSET / M_SQRT1_2, 1.0 - VOFFSET * M_SQRT1_2, 1.0 - VOFFSET },
    { 0.0, 1.0 - VOFFSET / M_SQRT1_2, 1.0 - VOFFSET},
    /* sixth, upper rear */
    { VOFFSET, 1.0 - VOFFSET / M_SQRT1_2, 0.0 },
    { VOFFSET / M_SQRT1_2, 1.0 - VOFFSET * M_SQRT1_2, VOFFSET },
    { 0.0, 1.0 - VOFFSET / M_SQRT1_2, VOFFSET }};

float solid_prism_n[][3] = {/* corners */
    { -VOFFSET, -VOFFSET, VOFFSET },
    { VOFFSET, -VOFFSET, VOFFSET },
    { -VOFFSET, VOFFSET, VOFFSET },
    { -VOFFSET, -VOFFSET, -VOFFSET },
    { VOFFSET, -VOFFSET, -VOFFSET },
    { -VOFFSET, VOFFSET, -VOFFSET },
    /* edges */
    { -VOFFSET, 0.0, VOFFSET },
    { 0.0, -VOFFSET, VOFFSET },
    { VOFFSET, VOFFSET, VOFFSET },
    { -VOFFSET, 0.0, -VOFFSET },
    { 0.0, -VOFFSET, -VOFFSET },
    { VOFFSET, VOFFSET, -VOFFSET },
    { -VOFFSET, -VOFFSET, 0.0 },
    { VOFFSET, -VOFFSET, 0.0 },
    { -VOFFSET, VOFFSET, 0.0 },
    /* faces */
    { 0.0, 0.0, 1.0 },
    { 0.0, -1.0, 0.0 },
    { M_SQRT1_2, M_SQRT1_2, 0.0 },
    { -1.0, 0.0, 0.0 },
    { 0.0, 0.0, -1.0 }};

float wire_prism_v[][3] = {{ 0.0, 0.0, 1.0 },
			   { 1.0, 0.0, 1.0 },
			   { 0.0, 1.0, 1.0 },
			   { 0.0, 0.0, 0.0 },
			   { 1.0, 0.0, 0.0 },
			   { 0.0, 1.0, 0.0 }};

float wire_prism_n[][3] = {{ 0.0, 0.0, 1.0},
			   { 0.0,-1.0, 0.0},
			   { M_SQRT1_2, M_SQRT1_2, 0.0},
			   {-1.0, 0.0, 0.0},
			   { 0.0, 0.0,-1.0}};

static struct glsnake_cfg * glc = NULL;
#ifdef HAVE_GLUT
# define bp glc
#endif

typedef float (*morphFunc)(long);

#ifdef HAVE_GLUT
/* forward definitions for GLUT functions */
void calc_rotation();
inline void ui_mousedrag();
#endif

void gl_init(
#ifndef HAVE_GLUT
	     ModeInfo * mi
#endif
	     ) {
    float light_pos[][3] = {{0.0, 0.0, 20.0}, {0.0, 20.0, 0.0}};
    float light_dir[][3] = {{0.0, 0.0,-20.0}, {0.0,-20.0, 0.0}};

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_NORMALIZE);

    if (!wireframe) {
	glColor3f(1.0, 1.0, 1.0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_pos[0]);
	glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, light_dir[0]);
	glLightfv(GL_LIGHT1, GL_POSITION, light_pos[1]);
	glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, light_dir[1]);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_COLOR_MATERIAL);
    }
}

void gettime(snaketime *t)
{
#ifdef HAVE_GETTIMEOFDAY
#ifdef GETTIMEOFDAY_TWO_ARGS
	struct timezone tzp;
	gettimeofday(t, &tzp);
#else /* !GETTIMEOFDAY_TWO_ARGS */
	gettimeofday(t);
#endif /* !GETTIMEOFDAY_TWO_ARGS */
#else /* !HAVE_GETTIMEOFDAY */
#ifdef HAVE_FTIME
	ftime(t);
#endif /* HAVE_FTIME */
#endif /* !HAVE_GETTIMEOFDAY */
}

#ifndef HAVE_GLUT
static void load_font(ModeInfo * mi, char * res, XFontStruct ** fontp, GLuint * dlistp) {
    const char * font = get_string_resource(res, "Font");
    XFontStruct * f;
    Font id;
    int first, last;

    if (!font)
	font = "-*-helvetica-medium-r-*-*-*-120-*";

    f = XLoadQueryFont(mi->dpy, font);
    if (!f)
	f = XLoadQueryFont(mi->dpy, "fixed");

    id = f->fid;
    first = f->min_char_or_byte2;
    last = f->max_char_or_byte2;

    clear_gl_error();
    *dlistp = glGenLists((GLuint) last + 1);
    check_gl_error("glGenLists");
    glXUseXFont(id, first, last - first + 1, *dlistp + first);
    check_gl_error("glXUseXFont");

    *fontp = f;
}
#endif

void start_morph(int model_index, int immediate);

/* wot initialises it */
void glsnake_init(
#ifndef HAVE_GLUT
ModeInfo * mi
#endif
) {
#ifndef HAVE_GLUT
    struct glsnake_cfg * bp;

    /* set up the conf struct and glx contexts */
    if (!glc) {
	glc = (struct glsnake_cfg *) calloc(MI_NUM_SCREENS(mi), sizeof(struct glsnake_cfg));
	if (!glc) {
	    fprintf(stderr, "%s: out of memory\n", progname);
	    exit(1);
	}
    }
    bp = &glc[MI_SCREEN(mi)];

    if ((bp->glx_context = init_GL(mi)) != NULL) {
	gl_init(mi);
	glsnake_reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    }
#else
    gl_init();
#endif

    /* initialise conf struct */
    memset(&bp->node, 0, sizeof(float) * NODE_COUNT);

    bp->selected = 11;
    bp->is_cyclic = 0;
    bp->is_legal = 1;
    bp->last_turn = -1;
    bp->morphing = 0;
    bp->paused = 0;
    bp->new_morph = 0;

    gettime(&bp->last_iteration);
    memcpy(&bp->last_morph, &bp->last_iteration, sizeof(bp->last_morph));

    bp->prev_colour = bp->next_colour = COLOUR_ACYCLIC;
    bp->next_model = RAND(models);
    bp->prev_model = START_MODEL;
    start_morph(bp->prev_model, 1);

    /* set up a font for the labels */
#ifndef HAVE_GLUT
    if (titles)
	load_font(mi, "labelfont", &bp->font, &bp->font_list);
#endif
    
    /* build a solid display list */
    glc->node_solid = glGenLists(1);
    glNewList(glc->node_solid, GL_COMPILE);
    /* corners */
    glBegin(GL_TRIANGLES);
    glNormal3fv(solid_prism_n[0]);
    glVertex3fv(solid_prism_v[0]);
    glVertex3fv(solid_prism_v[2]);
    glVertex3fv(solid_prism_v[1]);
    
    glNormal3fv(solid_prism_n[1]);
    glVertex3fv(solid_prism_v[6]);
    glVertex3fv(solid_prism_v[7]);
    glVertex3fv(solid_prism_v[8]);
    
    glNormal3fv(solid_prism_n[2]);
    glVertex3fv(solid_prism_v[12]);
    glVertex3fv(solid_prism_v[13]);
    glVertex3fv(solid_prism_v[14]);
    
    glNormal3fv(solid_prism_n[3]);
    glVertex3fv(solid_prism_v[3]);
    glVertex3fv(solid_prism_v[4]);
    glVertex3fv(solid_prism_v[5]);
    
    glNormal3fv(solid_prism_n[4]);
    glVertex3fv(solid_prism_v[9]);
    glVertex3fv(solid_prism_v[11]);
    glVertex3fv(solid_prism_v[10]);
    
    glNormal3fv(solid_prism_n[5]);
    glVertex3fv(solid_prism_v[16]);
    glVertex3fv(solid_prism_v[15]);
    glVertex3fv(solid_prism_v[17]);
    glEnd();
    /* edges */
    glBegin(GL_QUADS);
    glNormal3fv(solid_prism_n[6]);
    glVertex3fv(solid_prism_v[0]);
    glVertex3fv(solid_prism_v[12]);
    glVertex3fv(solid_prism_v[14]);
    glVertex3fv(solid_prism_v[2]);
    
    glNormal3fv(solid_prism_n[7]);
    glVertex3fv(solid_prism_v[0]);
    glVertex3fv(solid_prism_v[1]);
    glVertex3fv(solid_prism_v[7]);
    glVertex3fv(solid_prism_v[6]);
    
    glNormal3fv(solid_prism_n[8]);
    glVertex3fv(solid_prism_v[6]);
    glVertex3fv(solid_prism_v[8]);
    glVertex3fv(solid_prism_v[13]);
    glVertex3fv(solid_prism_v[12]);
    
    glNormal3fv(solid_prism_n[9]);
    glVertex3fv(solid_prism_v[3]);
    glVertex3fv(solid_prism_v[5]);
    glVertex3fv(solid_prism_v[17]);
    glVertex3fv(solid_prism_v[15]);
    
    glNormal3fv(solid_prism_n[10]);
    glVertex3fv(solid_prism_v[3]);
    glVertex3fv(solid_prism_v[9]);
    glVertex3fv(solid_prism_v[10]);
    glVertex3fv(solid_prism_v[4]);
    
    glNormal3fv(solid_prism_n[11]);
    glVertex3fv(solid_prism_v[15]);
    glVertex3fv(solid_prism_v[16]);
    glVertex3fv(solid_prism_v[11]);
    glVertex3fv(solid_prism_v[9]);
    
    glNormal3fv(solid_prism_n[12]);
    glVertex3fv(solid_prism_v[1]);
    glVertex3fv(solid_prism_v[2]);
    glVertex3fv(solid_prism_v[5]);
    glVertex3fv(solid_prism_v[4]);
    
    glNormal3fv(solid_prism_n[13]);
    glVertex3fv(solid_prism_v[8]);
    glVertex3fv(solid_prism_v[7]);
    glVertex3fv(solid_prism_v[10]);
    glVertex3fv(solid_prism_v[11]);
    
    glNormal3fv(solid_prism_n[14]);
    glVertex3fv(solid_prism_v[13]);
    glVertex3fv(solid_prism_v[16]);
    glVertex3fv(solid_prism_v[17]);
    glVertex3fv(solid_prism_v[14]);
    glEnd();
    
    /* faces */
    glBegin(GL_TRIANGLES);
    glNormal3fv(solid_prism_n[15]);
    glVertex3fv(solid_prism_v[0]);
    glVertex3fv(solid_prism_v[6]);
    glVertex3fv(solid_prism_v[12]);
    
    glNormal3fv(solid_prism_n[19]);
    glVertex3fv(solid_prism_v[3]);
    glVertex3fv(solid_prism_v[15]);
    glVertex3fv(solid_prism_v[9]);
    glEnd();
    
    glBegin(GL_QUADS);
    glNormal3fv(solid_prism_n[16]);
    glVertex3fv(solid_prism_v[1]);
    glVertex3fv(solid_prism_v[4]);
    glVertex3fv(solid_prism_v[10]);
    glVertex3fv(solid_prism_v[7]);
    
    glNormal3fv(solid_prism_n[17]);
    glVertex3fv(solid_prism_v[8]);
    glVertex3fv(solid_prism_v[11]);
    glVertex3fv(solid_prism_v[16]);
    glVertex3fv(solid_prism_v[13]);
    
    glNormal3fv(solid_prism_n[18]);
    glVertex3fv(solid_prism_v[2]);
    glVertex3fv(solid_prism_v[14]);
    glVertex3fv(solid_prism_v[17]);
    glVertex3fv(solid_prism_v[5]);
    glEnd();
    glEndList();
    
    /* build wire display list */
    glc->node_wire = glGenLists(1);
    glNewList(glc->node_wire, GL_COMPILE);
    glBegin(GL_LINE_STRIP);
    glVertex3fv(wire_prism_v[0]);
    glVertex3fv(wire_prism_v[1]);
    glVertex3fv(wire_prism_v[2]);
    glVertex3fv(wire_prism_v[0]);
    glVertex3fv(wire_prism_v[3]);
    glVertex3fv(wire_prism_v[4]);
    glVertex3fv(wire_prism_v[5]);
    glVertex3fv(wire_prism_v[3]);
    glEnd();
    glBegin(GL_LINES);
    glVertex3fv(wire_prism_v[1]);
    glVertex3fv(wire_prism_v[4]);
    glVertex3fv(wire_prism_v[2]);
    glVertex3fv(wire_prism_v[5]);
    glEnd();
    glEndList();
    
#ifdef HAVE_GLUT
    /* initialise the rotation */
    calc_rotation();
#endif
}

void draw_title(
#ifndef HAVE_GLUT
		ModeInfo * mi
#endif
		) {
#ifndef HAVE_GLUT
    struct glsnake_cfg * bp = &glc[MI_SCREEN(mi)];
#endif

    /* draw some text */
    glPushAttrib(GL_TRANSFORM_BIT | GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
#ifdef HAVE_GLUT
    gluOrtho2D(0, glc->width, 0, glc->height);
#else
    gluOrtho2D(0, mi->xgwa.width, 0, mi->xgwa.height);
#endif
    glColor3f(1.0, 1.0, 1.0);
    {
	char interactstr[] = "interactive";
	char * s;
	int i = 0;
#ifdef HAVE_GLUT
	int w;
#endif
	
	if (interactive)
	    s = interactstr;
	else
	    s = model[glc->next_model].name;
#ifdef HAVE_GLUT
	w = glutBitmapLength(GLUT_BITMAP_HELVETICA_12, (unsigned char *) s);
	glRasterPos2f(glc->width - w - 3, 4);
	while (s[i] != '\0')
	    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, s[i++]);
#else
	glRasterPos2f(10, mi->xgwa.height - 10 - (bp->font->ascent + bp->font->descent));
	while (s[i] != '\0')
	    glCallList(bp->font_list + (int)s[i++]);
#endif
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

/* apply the matrix to the origin and stick it in vec */
void matmult_origin(float rotmat[16], float vec[4]) {
#if 1
    vec[0] = 0.5 * rotmat[0] + 0.5 * rotmat[4] + 0.5 * rotmat [8] + 1 * rotmat[12];
    vec[1] = 0.5 * rotmat[1] + 0.5 * rotmat[5] + 0.5 * rotmat [9] + 1 * rotmat[13];
    vec[2] = 0.5 * rotmat[2] + 0.5 * rotmat[6] + 0.5 * rotmat[10] + 1 * rotmat[14];
    vec[3] = 0.5 * rotmat[3] + 0.5 * rotmat[7] + 0.5 * rotmat[11] + 1 * rotmat[15];
#else
    vec[0] = 0 * rotmat [0] + 0 * rotmat [1] + 0 * rotmat [2] + 1 * rotmat [3];
    vec[1] = 0 * rotmat [4] + 0 * rotmat [5] + 0 * rotmat [6] + 1 * rotmat [7];
    vec[2] = 0 * rotmat [8] + 0 * rotmat [9] + 0 * rotmat[10] + 1 * rotmat[11];
    vec[3] = 0 * rotmat[12] + 0 * rotmat[13] + 0 * rotmat[14] + 1 * rotmat[15];
#endif
    vec[0] /= vec[3];
    vec[1] /= vec[3];
    vec[2] /= vec[3];
    vec[3] = 1.0;
}

/* wot gets called when the winder is resized */
void glsnake_reshape(
#ifndef HAVE_GLUT
		     ModeInfo * mi,
#endif
		     int w, int h) {
    glViewport(0, 0, (GLint) w, (GLint) h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(zoom, w/(GLfloat)h, 0.05, 100.0);
    gluLookAt(0.0, 0.0, 20.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    glMatrixMode(GL_MODELVIEW);
    /*gluLookAt(0.0, 0.0, 20.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);*/
    glLoadIdentity();
#ifdef HAVE_GLUT
    glc->width = w;
    glc->height = h;
#endif
}

/* Returns the new dst_dir for the given src_dir and dst_dir */
int cross_product(int src_dir, int dst_dir) {
    return X_MASK*(GETSCALAR(src_dir,Y_MASK) * GETSCALAR(dst_dir,Z_MASK) -
		   GETSCALAR(src_dir,Z_MASK) * GETSCALAR(dst_dir,Y_MASK))+ 
	Y_MASK*(GETSCALAR(src_dir,Z_MASK) * GETSCALAR(dst_dir,X_MASK) -
		GETSCALAR(src_dir,X_MASK) * GETSCALAR(dst_dir,Z_MASK))+ 
	Z_MASK*(GETSCALAR(src_dir,X_MASK) * GETSCALAR(dst_dir,Y_MASK) -
		GETSCALAR(src_dir,Y_MASK) * GETSCALAR(dst_dir,X_MASK));
}

/* calculate orthogonal snake metrics
 *  is_legal  = true if model does not pass through itself
 *  is_cyclic = true if last node connects back to first node
 *  last_turn = for cyclic snakes, specifes what the 24th turn would be
 */
void calc_snake_metrics(void) {
    int srcDir, dstDir;
    int i, x, y, z;
    int prevSrcDir = -Y_MASK;
    int prevDstDir = Z_MASK;
    int grid[25][25][25];
    
    /* zero the grid */
    memset(&grid, 0, sizeof(int) * 25*25*25);
    
    glc->is_legal = 1;
    x = y = z = 12;
    
    /* trace path of snake - and keep record for is_legal */
    for (i = 0; i < NODE_COUNT - 1; i++) {
	/*int ang_card;*/ /* cardinal direction of node angle */
	/* establish new state vars */
	srcDir = -prevDstDir;
	x += GETSCALAR(prevDstDir, X_MASK);
	y += GETSCALAR(prevDstDir, Y_MASK);
	z += GETSCALAR(prevDstDir, Z_MASK);

	switch ((int) model[glc->next_model].node[i]) {
	  case (int) (ZERO):
	    dstDir = -prevSrcDir;
	    break;
	  case (int) (PIN):
	    dstDir = prevSrcDir;
	    break;
	  case (int) (RIGHT):
	  case (int) (LEFT):
	    dstDir = cross_product(prevSrcDir, prevDstDir);
	    if (model[glc->next_model].node[i] == (int) (RIGHT))
		dstDir = -dstDir;
	    break;
	  default:
	    /* Prevent spurious "might be used 
	     * uninitialised" warnings when compiling
	     * with -O2 */
	    dstDir = 0;
	    break;
	}
	
	if (grid[x][y][z] == 0)
	    grid[x][y][z] = srcDir + dstDir;
	else if (grid[x][y][z] + srcDir + dstDir == 0)
	    grid[x][y][z] = 8;
	else
	    glc->is_legal = 0;
	
	prevSrcDir = srcDir;
	prevDstDir = dstDir;
    }	
    
    /* determine if the snake is cyclic */
    glc->is_cyclic = (dstDir == Y_MASK && x == 12 && y == 11 && z == 12);
    
    /* determine last_turn */
    glc->last_turn = -1;
    if (glc->is_cyclic)
	switch (srcDir) {
	  case -Z_MASK: glc->last_turn = ZERO; break;
	  case Z_MASK:  glc->last_turn = PIN; break;
	  case X_MASK:  glc->last_turn = LEFT; break;
	  case -X_MASK: glc->last_turn = RIGHT; break;
	}
}

/* work out how far through the current morph we are */
float morph_percent(void) {
    float retval;
    int i;

    /* extend this function later with a case statement for each of the
     * morph schemes */

    /* when morphing all nodes at once, the longest morph will be the node
     * that needs to rotate 180 degrees.  For each node, work out how far it
     * has to go, and store the maximum rotation and current largest angular
     * difference, returning the angular difference over the maximum. */
    {
	float rot_max = 0.0, ang_diff_max = 0.0;

	for (i = 0; i < NODE_COUNT - 1; i++) {
	    float rot, ang_diff;

	    /* work out the maximum rotation this node has to go through
	     * from the previous to the next model, taking into account that
	     * the snake always morphs through the smaller angle */
	    rot = fabs(model[glc->prev_model].node[i] -
		       model[glc->next_model].node[i]);
	    if (rot > 180.0) rot = 180.0 - rot;
	    /* work out the difference between the current position and the
	     * target */
	    ang_diff = fabs(glc->node[i] -
			    model[glc->next_model].node[i]);
	    if (ang_diff > 180.0) ang_diff = 180 - ang_diff;
	    /* if it's the biggest so far, record it */
	    if (rot > rot_max) rot_max = rot;
	    if (ang_diff > ang_diff_max) ang_diff_max = ang_diff;
	}
	
	/* ang_diff / rot approaches 0, we want the complement */
	retval = 1.0 - (ang_diff_max / rot_max);
	/* protect against naan */

/* Apparently some systems (Solaris) don't have isinf() */
#undef isinf
#define isinf(x) (((x) > 999999999999.9) || ((x) < -999999999999.9))

	if (isnan(retval) || isinf(retval)) retval = 1.0;
    }
    /*printf("morph_pct = %f\n", retval);*/
    return retval;
}

void morph_colour(void) {
    float percent, compct; /* complement of percentage */

    percent = morph_percent();
    compct = 1.0 - percent;

    glc->colour[0][0] = colour[glc->prev_colour][0][0] * compct + colour[glc->next_colour][0][0] * percent;
    glc->colour[0][1] = colour[glc->prev_colour][0][1] * compct + colour[glc->next_colour][0][1] * percent;
    glc->colour[0][2] = colour[glc->prev_colour][0][2] * compct + colour[glc->next_colour][0][2] * percent;

    glc->colour[1][0] = colour[glc->prev_colour][1][0] * compct + colour[glc->next_colour][1][0] * percent;
    glc->colour[1][1] = colour[glc->prev_colour][1][1] * compct + colour[glc->next_colour][1][1] * percent;
    glc->colour[1][2] = colour[glc->prev_colour][1][2] * compct + colour[glc->next_colour][1][2] * percent;
}

/* Start morph process to this model */
void start_morph(int model_index, int immediate) {
    /* if immediate, don't bother morphing, go straight to the next model */
    if (immediate) {
	int i;

	for (i = 0; i < NODE_COUNT; i++)
	    glc->node[i] = model[model_index].node[i];
    }

    glc->prev_model = glc->next_model;
    glc->next_model = model_index;
    glc->prev_colour = glc->next_colour;

    calc_snake_metrics();
    if (!glc->is_legal)
	glc->next_colour = COLOUR_INVALID;
    else if (altcolour)
	glc->next_colour = COLOUR_AUTHENTIC;
    else if (glc->is_cyclic)
	glc->next_colour = COLOUR_CYCLIC;
    else
	glc->next_colour = COLOUR_ACYCLIC;

    if (immediate) {
	glc->colour[0][0] = colour[glc->next_colour][0][0];
	glc->colour[0][1] = colour[glc->next_colour][0][1];
	glc->colour[0][2] = colour[glc->next_colour][0][2];
	glc->colour[1][0] = colour[glc->next_colour][1][0];
	glc->colour[1][1] = colour[glc->next_colour][1][1];
	glc->colour[1][2] = colour[glc->next_colour][1][2];
    }
    glc->morphing = 1;

    morph_colour();
}

/* Returns morph progress */
float morph(long iter_msec) {
    /* work out the maximum angle for this iteration */
    int still_morphing;
    float iter_angle_max, largest_diff, largest_progress;
    int i;

    if (glc->new_morph)
	glc->new_morph = 0;
	
    iter_angle_max = 90.0 * (angvel/1000.0) * iter_msec;
	
    still_morphing = 0;
    largest_diff = largest_progress = 0.0;
    for (i = 0; i < NODE_COUNT; i++) {
	float curAngle = glc->node[i];
	float destAngle = model[glc->next_model].node[i];
	if (curAngle != destAngle) {
	    still_morphing = 1;
	    if (fabs(curAngle-destAngle) <= iter_angle_max)
		glc->node[i] = destAngle;
	    else if (fmod(curAngle-destAngle+360,360) > 180)
		glc->node[i] = fmod(curAngle + iter_angle_max, 360);
	    else
		glc->node[i] = fmod(curAngle+360 - iter_angle_max, 360);
	    largest_diff = MAX(largest_diff, fabs(destAngle-glc->node[i]));
	    largest_progress = MAX(largest_diff, fabs(glc->node[i] - model[glc->prev_model].node[i]));
	}
    }
	
    return MIN(largest_diff / largest_progress, 1.0);
}

#ifdef HAVE_GLUT
void glsnake_idle();

void restore_idle(int value)
{
    glutIdleFunc(glsnake_idle);
}
#endif

void quick_sleep(void)
{
#ifdef HAVE_GLUT
    /* By using glutTimerFunc we can keep responding to 
     * mouse and keyboard events, unlike using something like
     * usleep. */
    glutIdleFunc(NULL);
    glutTimerFunc(1, restore_idle, 0);
#else
    usleep(1);
#endif
}

void glsnake_idle(
#ifndef HAVE_GLUT
		  struct glsnake_cfg * bp
#endif
		  ) {
    /* time since last iteration */
    long iter_msec;
    /* time since the beginning of last morph */
    long morf_msec;
    float iter_angle_max;
    snaketime current_time;
    /*    morphFunc transition; */
    int still_morphing;
    int i;
    
    /* Do nothing to the model if we are paused */
    if (glc->paused) {
	/* Avoid busy waiting when nothing is changing */
	quick_sleep();
#ifdef HAVE_GLUT
	glutSwapBuffers();
	glutPostRedisplay();
#endif
	return;
    }

    /* <spiv> Well, ftime gives time with millisecond resolution.
     * <spiv> (or worse, perhaps... who knows what the OS will do)
     * <spiv> So if no discernable amount of time has passed:
     * <spiv>   a) There's no point updating the screen, because
     *             it would be the same
     * <spiv>   b) The code will divide by zero
     */
    gettime(&current_time);
    
    iter_msec = (long) GETMSECS(current_time) - GETMSECS(glc->last_iteration) + 
	((long) GETSECS(current_time) - GETSECS(glc->last_iteration)) * 1000L;

    if (iter_msec) {
	/* save the current time */
	memcpy(&glc->last_iteration, &current_time, sizeof(snaketime));
	
	/* work out if we have to switch models */
	morf_msec = GETMSECS(glc->last_iteration) - GETMSECS(glc->last_morph) +
	    ((long) (GETSECS(glc->last_iteration)-GETSECS(glc->last_morph)) * 1000L);

	if ((morf_msec > statictime) && !interactive && !glc->morphing) {
	    /*printf("starting morph\n");*/
	    memcpy(&glc->last_morph, &(glc->last_iteration), sizeof(glc->last_morph));
	    start_morph(RAND(models), 0);
	}
	
	if (interactive && !glc->morphing) {
	    quick_sleep();
	    return;
	}
	
	/*	if (!glc->dragging && !glc->interactive) { */
	if (!interactive) {
	    
	    yspin += 360/((1000/yangvel)/iter_msec);
	    zspin += 360/((1000/zangvel)/iter_msec);
	    /*
	    yspin += 360 * (yangvel/1000.0) * iter_msec;
	    zspin += 360 * (zangvel/1000.0) * iter_msec;
	    */
	    
	    /*printf("yspin: %f, zspin: %f\n", yspin, zspin);*/

	}

	/* work out the maximum angle we could turn this node in this
	 * timeslice, iter_msec milliseconds long */
	iter_angle_max = 90.0 * (angvel/1000.0) * iter_msec;

	still_morphing = 0;
	for (i = 0; i < NODE_COUNT; i++) {
	    float cur_angle = glc->node[i];
	    float dest_angle = model[glc->next_model].node[i];
	    if (cur_angle != dest_angle) {
		still_morphing = 1;
		if (fabs(cur_angle - dest_angle) <= iter_angle_max)
		    glc->node[i] = dest_angle;
		else if (fmod(cur_angle - dest_angle + 360, 360) > 180)
		    glc->node[i] = fmod(cur_angle + iter_angle_max, 360);
		else
		    glc->node[i] = fmod(cur_angle + 360 - iter_angle_max, 360);
	    }
	}

	if (!still_morphing)
	    glc->morphing = 0;

	/* colour cycling */
	morph_colour();
	
#ifdef HAVE_GLUT
	glutSwapBuffers();
	glutPostRedisplay();
#endif
    } else {
	/* We are going too fast, so we may as well let the 
	 * cpu relax a little by sleeping for a millisecond. */
	quick_sleep();
    }
}

/* wot draws it */
void glsnake_display(
#ifndef HAVE_GLUT
		     ModeInfo * mi
#endif
		     ) {
#ifndef HAVE_GLUT
    struct glsnake_cfg * bp = &glc[MI_SCREEN(mi)];
    Display * dpy = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
#endif

    int i;
    float ang;
    float positions[NODE_COUNT][4]; /* origin points for each node */
    float com[4]; /* it's the CENTRE of MASS */

#ifndef HAVE_GLUT
    if (!bp->glx_context)
	return;
#endif
    
    /* clear the buffer */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    /* go into the modelview stack */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    /* get the centre of each node, by moving through the snake and
     * performing the rotations, then grabbing the matrix at each point
     * and applying it to the origin */
    glPushMatrix();

#ifdef HAVE_GLUT
    /* apply the mouse drag rotation */
    ui_mousedrag();
#endif
    
    /* apply the continuous rotation */
    glRotatef(yspin, 0.0, 1.0, 0.0); 
    glRotatef(zspin, 0.0, 0.0, 1.0); 
    
    com[0] = 0.0;
    com[1] = 0.0;
    com[2] = 0.0;
    com[3] = 0.0;
    for (i = 0; i < NODE_COUNT; i++) {
	float rotmat[16];

	ang = glc->node[i];

	/*printf("ang = %f\n", ang);*/
	
	glTranslatef(0.5, 0.5, 0.5);		/* move to center */
	glRotatef(90, 0.0, 0.0, -1.0);		/* reorient  */
	glTranslatef(1.0 + explode, 0.0, 0.0);	/* move to new pos. */
	glRotatef(180 + ang, 1.0, 0.0, 0.0);	/* pivot to new angle */
	glTranslatef(-0.5, -0.5, -0.5);		/* return from center */

	glGetFloatv(GL_MODELVIEW_MATRIX, rotmat);

	matmult_origin(rotmat, positions[i]);

	/*printf("positions %f %f %f %f\n", positions[i][0], positions[i][1], positions[i][2], positions[i][3]);*/

	com[0] += positions[i][0];
	com[1] += positions[i][1];
	com[2] += positions[i][2];
	com[3] += positions[i][3];
    }
    glPopMatrix();
    com[0] /= NODE_COUNT;
    com[1] /= NODE_COUNT;
    com[2] /= NODE_COUNT;
    com[3] /= NODE_COUNT;

    com[0] /= com[3];
    com[1] /= com[3];
    com[2] /= com[3];

    /*printf("com: %f, %f, %f, %f\n", com[0], com[1], com[2], com[3]);*/

#if MAGICAL_RED_STRING
    glPushMatrix();
    glTranslatef(-com[0], -com[1], -com[2]);

    glDisable(GL_LIGHTING);
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINE_STRIP);
    for (i = 0; i < NODE_COUNT - 1; i++) {
	glVertex3fv(positions[i]);
    }
    glEnd();
    glEnable(GL_LIGHTING);
    /*glTranslatef(com[0], com[1], com[2]);*/
    glPopMatrix();
#endif

    glPushMatrix();
    glTranslatef(-com[0], -com[1], -com[2]);

#ifdef HAVE_GLUT
    /* apply the mouse drag rotation */
    ui_mousedrag();
#endif
    
    /* apply the continuous rotation */
    glRotatef(yspin, 0.0, 1.0, 0.0); 
    glRotatef(zspin, 0.0, 0.0, 1.0); 

    /* now draw each node along the snake -- this is quite ugly :p */
    for (i = 0; i < NODE_COUNT; i++) {
	/* choose a colour for this node */
	if ((i == glc->selected || i == glc->selected+1) && interactive)
	    /* yellow */
	    glColor3f(1.0, 1.0, 0.0);
	else
	    glColor3fv(glc->colour[(i+1)%2]);

	/* draw the node */
	if (wireframe)
	    glCallList(glc->node_wire);
	else
	    glCallList(glc->node_solid);

	/* now work out where to draw the next one */
	
	/* Interpolate between models */
	ang = glc->node[i];
	
	glTranslatef(0.5, 0.5, 0.5);		/* move to center */
	glRotatef(90, 0.0, 0.0, -1.0);		/* reorient  */
	glTranslatef(1.0 + explode, 0.0, 0.0);	/* move to new pos. */
	glRotatef(180 + ang, 1.0, 0.0, 0.0);	/* pivot to new angle */
	glTranslatef(-0.5, -0.5, -0.5);		/* return from center */
    }

    glPopMatrix();
    
    if (titles)
#ifdef HAVE_GLUT
	draw_title();
#else
	draw_title(mi);
#endif

#ifndef HAVE_GLUT
	glsnake_idle(bp);
#endif
    
    glFlush();
#ifdef HAVE_GLUT
    glutSwapBuffers();
#else
    glXSwapBuffers(dpy, window);
#endif
}

#ifdef HAVE_GLUT
/* anything that needs to be cleaned up goes here */
void unmain() {
    glutDestroyWindow(glc->window);
    free(glc);
}

void ui_init(int *, char **);

int main(int argc, char ** argv) {
    glc = malloc(sizeof(struct glsnake_cfg));
    memset(glc, 0, sizeof(struct glsnake_cfg));

    glc->width = 320;
    glc->height = 240;
    
    ui_init(&argc, argv);

    gettime(&glc->last_iteration);
    memcpy(&glc->last_morph, &glc->last_iteration, sizeof(snaketime));
    srand((unsigned int)GETSECS(glc->last_iteration));

    glc->prev_colour = glc->next_colour = COLOUR_ACYCLIC;
    glc->next_model = RAND(models);
    glc->prev_model = 0;
    start_morph(glc->prev_model, 1);	

    glsnake_init();
    
    atexit(unmain);
    glutSwapBuffers();
    glutMainLoop();
    
    return 0;
}
#endif

/*
 * GLUT FUNCTIONS
 */

#ifdef HAVE_GLUT

/* trackball quaternions */
float cumquat[4] = {0.0,0.0,0.0,0.0}, oldquat[4] = {0.0,0.0,0.0,0.1};

/* rotation matrix */
float rotation[16];

/* mouse drag vectors: start and end */
float m_s[3], m_e[3];

/* dragging boolean */
int dragging = 0;

/* this function calculates the rotation matrix based on the quaternions
 * generated from the mouse drag vectors */
void calc_rotation() {
    double Nq, s;
    double xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

    /* this bit ripped from Shoemake's quaternion notes from SIGGRAPH */
    Nq = cumquat[0] * cumquat[0] + cumquat[1] * cumquat[1] +
	cumquat[2] * cumquat[2] + cumquat[3] * cumquat[3];
    s = (Nq > 0.0) ? (2.0 / Nq) : 0.0;
    xs = cumquat[0] *  s; ys = cumquat[1] *  s; zs = cumquat[2] * s;
    wx = cumquat[3] * xs; wy = cumquat[3] * ys; wz = cumquat[3] * zs;
    xx = cumquat[0] * xs; xy = cumquat[0] * ys; xz = cumquat[0] * zs;
    yy = cumquat[1] * ys; yz = cumquat[1] * zs; zz = cumquat[2] * zs;

    rotation[0] = 1.0 - (yy + zz);
    rotation[1] = xy + wz;
    rotation[2] = xz - wy;
    rotation[4] = xy - wz;
    rotation[5] = 1.0 - (xx + zz);
    rotation[6] = yz + wx;
    rotation[8] = xz + wy;
    rotation[9] = yz - wx;
    rotation[10] = 1.0 - (xx + yy);
    rotation[3] = rotation[7] = rotation[11] = 0.0;
    rotation[12] = rotation[13] = rotation[14] = 0.0;
    rotation[15] = 1.0;
}

inline void ui_mousedrag() {
    glMultMatrixf(rotation);
}

void ui_keyboard(unsigned char c, int x, int y) {
    int i;
    
    switch (c) {
      case 27:  /* ESC */
      case 'q':
	exit(0);
	break;
      case 'e':
	explode += DEF_EXPLODE;
	glutPostRedisplay();
	break;
      case 'E':
	explode -= DEF_EXPLODE;
	if (explode < 0.0) explode = 0.0;
	glutPostRedisplay();
	break;
      case '.':
	/* next model */
	glc->next_model++;
	glc->next_model %= models;
	start_morph(glc->next_model, 0);
	
	/* Reset last_morph time */
	gettime(&glc->last_morph);			
	break;
      case ',':
	/* previous model */
	glc->next_model = (glc->next_model + models - 1) % models;
	start_morph(glc->next_model, 0);
	
	/* Reset glc->last_morph time */
	gettime(&glc->last_morph);			
	break;
      case '+':
	angvel += DEF_ACCEL;
	break;
      case '-':
	if (angvel > DEF_ACCEL)
	    angvel -= DEF_ACCEL;
	break;
      case 'i':
	if (interactive) {
	    /* Reset last_iteration and last_morph time */
	    gettime(&glc->last_iteration);
	    gettime(&glc->last_morph);
	}
	interactive = 1 - interactive;
	glutPostRedisplay();
	break;
      case 'w':
	wireframe = 1 - wireframe;
	if (wireframe)
	    glDisable(GL_LIGHTING);
	else
	    glEnable(GL_LIGHTING);
	glutPostRedisplay();
	break;
      case 'p':
	if (glc->paused) {
	    /* unpausing, reset last_iteration and last_morph time */
	    gettime(&glc->last_iteration);
	    gettime(&glc->last_morph);
	}
	glc->paused = 1 - glc->paused;
	break;
      case 'd':
	/* dump the current model so we can add it! */
	printf("# %s\nnoname:\t", model[glc->next_model].name);
	for (i = 0; i < NODE_COUNT; i++) {
	    if (glc->node[i] == ZERO)
		printf("Z");
	    else if (glc->node[i] == LEFT)
		printf("L");
	    else if (glc->node[i] == PIN)
		printf("P");
	    else if (glc->node[i] == RIGHT)
		printf("R");
	    /*
	      else
	      printf("%f", node[i].curAngle);
	    */
	    if (i < NODE_COUNT - 1)
		printf(" ");
	}
	printf("\n");
	break;
      case 'f':
	glc->fullscreen = 1 - glc->fullscreen;
	if (glc->fullscreen) {
	    glc->old_width = glc->width;
	    glc->old_height = glc->height;
	    glutFullScreen();
	} else {
	    glutReshapeWindow(glc->old_width, glc->old_height);
	    glutPositionWindow(50,50);
	}
	break;
      case 't':
	titles = 1 - titles;
	if (interactive || glc->paused)
	    glutPostRedisplay();
	break;
      case 'a':
	altcolour = 1 - altcolour;
	break;
      case 'z':
	zoom += 1.0;
	glsnake_reshape(glc->width, glc->height);
	break;
      case 'Z':
	zoom -= 1.0;
	glsnake_reshape(glc->width, glc->height);
	break;
      default:
	break;
    }
}

void ui_special(int key, int x, int y) {
    int i;
    float *destAngle = &(model[glc->next_model].node[glc->selected]);
    int unknown_key = 0;

    if (interactive) {
	switch (key) {
	  case GLUT_KEY_UP:
	    glc->selected = (glc->selected + (NODE_COUNT - 2)) % (NODE_COUNT - 1);
	    break;
	  case GLUT_KEY_DOWN:
	    glc->selected = (glc->selected + 1) % (NODE_COUNT - 1);
	    break;
	  case GLUT_KEY_LEFT:
	    *destAngle = fmod(*destAngle+(LEFT), 360);
	    glc->morphing = glc->new_morph = 1;
	    break;
	  case GLUT_KEY_RIGHT:
	    *destAngle = fmod(*destAngle+(RIGHT), 360);
	    glc->morphing = glc->new_morph = 1;
	    break;
	  case GLUT_KEY_HOME:
	    start_morph(STRAIGHT_MODEL, 0);
	    break;
	  default:
	    unknown_key = 1;
	    break;
	}
    }
    calc_snake_metrics();

    if (!unknown_key)
	glutPostRedisplay();
}

void ui_mouse(int button, int state, int x, int y) {
    if (button==0) {
	switch (state) {
	  case GLUT_DOWN:
	    dragging = 1;
	    m_s[0] = M_SQRT1_2 * 
		(x - (glc->width / 2.0)) / (glc->width / 2.0);
	    m_s[1] = M_SQRT1_2 * 
		((glc->height / 2.0) - y) / (glc->height / 2.0);
	    m_s[2] = sqrt(1-(m_s[0]*m_s[0]+m_s[1]*m_s[1]));
	    break;
	  case GLUT_UP:
	    dragging = 0;
	    oldquat[0] = cumquat[0];
	    oldquat[1] = cumquat[1];
	    oldquat[2] = cumquat[2];
	    oldquat[3] = cumquat[3];
	    break;
	  default:
	    break;
	}
    }
    glutPostRedisplay();
}

void ui_motion(int x, int y) {
    double norm;
    float q[4];
    
    if (dragging) {
	/* construct the motion end vector from the x,y position on the
	 * window */
	m_e[0] = (x - (glc->width/ 2.0)) / (glc->width / 2.0);
	m_e[1] = ((glc->height / 2.0) - y) / (glc->height / 2.0);
	/* calculate the normal of the vector... */
	norm = m_e[0] * m_e[0] + m_e[1] * m_e[1];
	/* check if norm is outside the sphere and wraparound if necessary */
	if (norm > 1.0) {
	    m_e[0] = -m_e[0];
	    m_e[1] = -m_e[1];
	    m_e[2] = sqrt(norm - 1);
	} else {
	    /* the z value comes from projecting onto an elliptical spheroid */
	    m_e[2] = sqrt(1 - norm);
	}

	/* now here, build a quaternion from m_s and m_e */
	q[0] = m_s[1] * m_e[2] - m_s[2] * m_e[1];
	q[1] = m_s[2] * m_e[0] - m_s[0] * m_e[2];
	q[2] = m_s[0] * m_e[1] - m_s[1] * m_e[0];
	q[3] = m_s[0] * m_e[0] + m_s[1] * m_e[1] + m_s[2] * m_e[2];

	/* new rotation is the product of the new one and the old one */
	cumquat[0] = q[3] * oldquat[0] + q[0] * oldquat[3] + 
	    q[1] * oldquat[2] - q[2] * oldquat[1];
	cumquat[1] = q[3] * oldquat[1] + q[1] * oldquat[3] + 
	    q[2] * oldquat[0] - q[0] * oldquat[2];
	cumquat[2] = q[3] * oldquat[2] + q[2] * oldquat[3] + 
	    q[0] * oldquat[1] - q[1] * oldquat[0];
	cumquat[3] = q[3] * oldquat[3] - q[0] * oldquat[0] - 
	    q[1] * oldquat[1] - q[2] * oldquat[2];
	
	calc_rotation();
    }
    glutPostRedisplay();
}

void ui_init(int * argc, char ** argv) {
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(glc->width, glc->height);
    glc->window = glutCreateWindow("glsnake");

    glutDisplayFunc(glsnake_display);
    glutReshapeFunc(glsnake_reshape);
    glutIdleFunc(glsnake_idle);
    glutKeyboardFunc(ui_keyboard);
    glutSpecialFunc(ui_special);
    glutMouseFunc(ui_mouse);
    glutMotionFunc(ui_motion);

    yangvel = DEF_YANGVEL;
    zangvel = DEF_ZANGVEL;
    explode = DEF_EXPLODE;
    angvel = DEF_ANGVEL;
    statictime = DEF_STATICTIME;
    altcolour = DEF_ALTCOLOUR;
    titles = DEF_TITLES;
    interactive = DEF_INTERACTIVE;
    zoom = DEF_ZOOM;
    wireframe = DEF_WIREFRAME;
}
#endif /* HAVE_GLUT */
