/*
 * lockward.c:	First attempt at an Xscreensaver.
 *
 * Leo L. Schwab					2007.08.17
 ****
 * Copyright (c) 2007 Leo L. Schwab
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <ctype.h>
#include <strings.h>

#include "xlockmore.h"
#include "colors.h"


/***************************************************************************
 * #defines
 */
#ifdef USE_GL /* whole file */

#define	DEFAULTS	"*delay:	20000	\n"\
			"*showFPS:	False	\n"

#define	refresh_lockward	0


#define	NUMOF(x)	(sizeof ((x)) / sizeof ((*x)))

#define	NBLADES		12
#define	NSPINNERS	4
#define	NRADII		8
#define	COLORIDX_SHF	4


/***************************************************************************
 * Structure definitions.
 */
struct lockward_context;			/*  Forward declaration.  */

#define int8_t   char
#define int16_t  short
#define int32_t  int
#define uint8_t  unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int

typedef struct bladestate {
	uint8_t		outer, inner;	/*  Radii  */
} bladestate;

typedef struct spinnerstate {
	GLfloat		rot;	/*  Terminal rotation after count expires */
	GLfloat		rotinc;	/*  Per-frame increment to rot.  */
	XColor		*colors;
	bladestate	*bladeidx;
	int		ncolors;	/*  n.4 fixed-point  */
	int		ccolor;		/*  n.4 fixed-point  */
	int		colorinc;	/*  n.4 fixed-point  */
	int		rotcount;
	uint8_t		nblades;
} spinnerstate;

typedef struct blinkstate {
	int		(*drawfunc) (struct lockward_context *ctx,
			             struct blinkstate *bs);
	uint32_t	*noise;	/*  For draw_blink_segment_scatter()  */
	GLfloat		color[4];
	uint32_t	val;
	int16_t		dwell;	/*  <0: sharp  >0: decay  */
	int16_t		dwellcnt;
	uint8_t		type;
	int8_t		counter;
	int8_t		direction;
	int8_t		radius;
} blinkstate;

enum blinktype {
	BTYPE_RADIAL_SINGLE = 0,
	BTYPE_RADIAL_RANDOM,
	BTYPE_RADIAL_SEQ,
	BTYPE_RADIAL_DOUBLESEQ,
	BTYPE_SEGMENT_SINGLE,
	BTYPE_SEGMENT_RANDOM,
	BTYPE_CONCENTRIC_SINGLE,
	BTYPE_CONCENTRIC_RANDOM,
	BTYPE_CONCENTRIC_SEQ,
	BTYPE_SEGMENT_SCATTER,
	MAX_BTYPE
};

typedef struct lockward_context {
	GLXContext	*glx_context;

	spinnerstate	spinners[NSPINNERS];
	blinkstate	blink;

	GLuint		blades_outer, blades_inner;
	GLuint		rings;
	Bool		blendmode;
	int		nextblink;
	int		fps;
} lockward_context;


/***************************************************************************
 * Prototypes.
 */
static void free_lockward (lockward_context *ctx);


/***************************************************************************
 * Global variables.
 */
static lockward_context	*g_ctx = NULL;
static Bool		g_blink_p = True;
static int		g_blades = NBLADES;
static int		g_rotateidle_min,
			g_rotateidle_max;
static int		g_blinkidle_min,
			g_blinkidle_max;
static int		g_blinkdwell_min,
			g_blinkdwell_max;

#define DEF_BLINK		"True"
#define DEF_ROTATEIDLEMIN	"1000"
#define DEF_ROTATEIDLEMAX	"6000"
#define DEF_BLINKIDLEMIN	"1000"
#define DEF_BLINKIDLEMAX	"9000"
#define DEF_BLINKDWELLMIN	"100"
#define DEF_BLINKDWELLMAX	"600"


static XrmOptionDescRec opts[] = {
	{ "-blink",	".blink",  XrmoptionNoArg, "on" },
	{ "+blink",	".blink",  XrmoptionNoArg, "off" },
	{ "-rotateidle-min",	".rotateidlemin",	XrmoptionSepArg,	0 },
	{ "-rotateidle-max",	".rotateidlemax",	XrmoptionSepArg,	0 },
	{ "-blinkidle-min",	".blinkidlemin",	XrmoptionSepArg,	0 },
	{ "-blinkidle-max",	".blinkidlemax",	XrmoptionSepArg,	0 },
	{ "-blinkdwell-min",	".blinkdwellmin",	XrmoptionSepArg,	0 },
	{ "-blinkdwell-max",	".blinkdwellmax",	XrmoptionSepArg,	0 },
};

static argtype vars[] = {
	{ &g_blink_p, "blink",	"Blink", DEF_BLINK, t_Bool	},
	{ &g_rotateidle_min, "rotateidlemin",	"Rotateidlemin", DEF_ROTATEIDLEMIN, t_Int	},
	{ &g_rotateidle_max, "rotateidlemax",	"Rotateidlemax", DEF_ROTATEIDLEMAX, t_Int	},
	{ &g_blinkidle_min, "blinkidlemin", "Blinkidlemin", DEF_BLINKIDLEMIN, t_Int },
	{ &g_blinkidle_max, "blinkidlemax", "Blinkidlemax", DEF_BLINKIDLEMAX, t_Int },
	{ &g_blinkdwell_min, "blinkdwellmin", "Blinkdwellmin", DEF_BLINKDWELLMIN, t_Int },
	{ &g_blinkdwell_max, "blinkdwellmax", "Blinkdwellmax", DEF_BLINKDWELLMAX, t_Int },
};

static OptionStruct desc[] = {
	{ "-/+blink", "Turn on/off blinking effects." },
	{ "-rotateidle-min", "Minimum idle time for rotators, in milliseconds." },
	{ "-rotateidle-max", "Maximum idle time for rotators, in milliseconds." },
	{ "-blinkidle-min", "Minimum idle time between blink effects, in milliseconds." },
	{ "-blinkidle-max", "Maximum idle time between blink effects, in milliseconds." },
	{ "-blinkdwell-min", "Minimum dwell time for blink effects, in milliseconds." },
	{ "-blinkdwell-max", "Maximum dwell time for blink effects, in milliseconds." },
};

ENTRYPOINT ModeSpecOpt lockward_opts = {
	NUMOF(opts), opts, NUMOF(vars), vars, desc
};


/***************************************************************************
 * Window management.
 */
ENTRYPOINT void
reshape_lockward (ModeInfo *mi, int width, int height)
{
	GLfloat h = (GLfloat) height / (GLfloat) width;

	glViewport (0, 0, (GLint) width, (GLint) height);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	if (height > width)
		glOrtho (-8.0, 8.0, -8.0 * h, 8.0 * h, -1, 1);
	else
		glOrtho (-8.0 / h, 8.0 / h, -8.0, 8.0, -1, 1);

	glMatrixMode (GL_MODELVIEW);
}

ENTRYPOINT Bool
lockward_handle_event (ModeInfo *mi, XEvent *event)
{
	lockward_context *ctx = &g_ctx[MI_SCREEN (mi)];

	if (event->xany.type == KeyPress) {
		KeySym	keysym;
		char	c = 0;

		XLookupString (&event->xkey, &c, 1, &keysym, 0);
		if (c == 'b') {
			ctx->blendmode ^= 1;
			return True;
		}
	}

	return False;
}


/***************************************************************************
 * "Blade" routines.
 */
static void
random_blade_rot (lockward_context *ctx, struct spinnerstate *ss)
{
	/*
	 * The circle is divided up in to g_blades divisions.  The idea here
	 * is to rotate to an exact division point.
	 *
	 * The target rotation is computed via random numbers.
	 *
	 * The time it takes to get there is a maximum of six seconds per
	 * division, and a minimum of one second (no matter how far away it
	 * is), and is selected by random numbers.
	 *
	 * The time value is converted into frames, and a per-frame rotation
	 * is computed.
	 *
	 * During rendering, we approach the target rotation by subtracting
	 * from it the per-frame rotation times the number of outstanding
	 * ticks.  Doing it this way means we'll hit the target rotation
	 * exactly, without low-order errors creeping in to the values (at
	 * least not nearly as quickly).
	 */
	GLfloat	d;
	int	dist;

	dist = random() % g_blades + 1;

	ss->rotcount = random() % (6 * dist * ctx->fps - ctx->fps)
		     + ctx->fps;

	if (random() & 4)
		dist = -dist;
	d = dist * 360.0 / (GLfloat) g_blades;
	ss->rot += d;
	ss->rotinc = d / (GLfloat) ss->rotcount;
}


/*
 * A "blade" is pie-wedge shaped flat thing that is rotated around where the
 * apex is/would be.  Initially envisioned as 1/12th of a circle, but that
 * could be configurable.  The inner and outer edges are rounded off using
 * six subdivisions so that, when multiple blades are assembled, it looks
 * more like a circle and less like a polygon.
 *
 * The blade is assembled as a tri-fan.  It is oriented centered at 3
 * o'clock.  The blade is composed of two display lists -- arcs, essentially
 * -- the outer and the inner one.  The outer one *must* be called before
 * the inner one, or the blade clockwise-ness will be wrong, and become
 * invisible.  Arcs of various radii are compiled.
 */
#define	SUBDIV		6

static void
gen_blade_arcs (lockward_context *ctx)
{
	GLfloat	here, there, step;
	int	i, n;

	here = 0;
	there = M_PI * 2.0 / g_blades;
	step = there / SUBDIV;
	here -= SUBDIV * step / 2.0;

	/*
	 * Build outer blade arcs.
	 * Start at left side of outer radius.  Strike all its vertices.
	 */
	for (n = 0;  n < NRADII;  ++n) {
		glNewList (ctx->blades_outer + n, GL_COMPILE);
		for (i = SUBDIV;  i >= 0;  --i)
			glVertex3f (cos (here + step * i) * (n + 1.0),
			            sin (here + step * i) * (n + 1.0), 0);
		glEndList ();
	}

	/*
	 * Build inner blade arcs.
	 * Move to inner radius, strike all vertices in opposite order.
	 */
	for (n = 0;  n < NRADII;  ++n) {
		glNewList (ctx->blades_inner + n, GL_COMPILE);
		for (i = 0;  i <= SUBDIV;  ++i)
			glVertex3f (cos (here + step * i) * (n + 1.0),
			            sin (here + step * i) * (n + 1.0), 0);
		glEndList ();
	}
}

static void
gen_rings (lockward_context *ctx)
{
	GLfloat step;
	int	i, n;

	step = M_PI * 2.0 / (g_blades * SUBDIV);

	for (n = 0;  n < NRADII - 1;  ++n) {
		glNewList (ctx->rings + n, GL_COMPILE);
		glBegin (GL_TRIANGLE_STRIP);
		for (i = g_blades * SUBDIV;  i >= 0;  --i) {
			glVertex3f (cos (step * i) * (n + 1.0),
			            sin (step * i) * (n + 1.0), 0);
			glVertex3f (cos (step * i) * (n + 2.0),
			            sin (step * i) * (n + 2.0), 0);
		}
		glEnd();
		glEndList ();
	}
}


/***************************************************************************
 * "Blink" routines.
 */
static int
calc_interval_frames (lockward_context *ctx, int min, int max)
{
	/*
	 * Compute random interval between min and max milliseconds.
	 * Returned value is in frames.
	 */
	register int i;

	i = min;
	if (max > min)
		i += random() % (max - min);

	return i * ctx->fps / 1000;
}

static void
set_alpha_by_dwell (struct blinkstate *bs)
{
	if (bs->dwell > 0)
		bs->color[3] = (GLfloat) bs->dwellcnt / (GLfloat) bs->dwell;
	else
		bs->color[3] = bs->dwellcnt > (-bs->dwell >> 2)  ?  1.0  : 0.0;
}

static void
draw_blink_blade (lockward_context *ctx, int inner, int outer)
{
	glBegin (GL_TRIANGLE_FAN);
	glCallList (ctx->blades_outer + outer);
	glCallList (ctx->blades_inner + inner);
	glEnd ();
}

static int
draw_blink_radial_random (lockward_context *ctx, struct blinkstate *bs)
{
	int i;

	/*
	 * There is no sense of direction in a random sweep, so re-use the
	 * 'direction' field to hold the current blade we're messing with.
	 */
	if (bs->dwellcnt < 0) {
		if (bs->counter <= 0) {
			bs->drawfunc = NULL;
			return 0;
		}

		/*
		 * Find available blade.  Potentially very slow, depending on
		 * how unlucky we are.
		 */
		do {
			i = random() % g_blades;
		} while (bs->val & (1 << i));
		bs->val |= (1 << i);	/*  Mark as used.  */
		bs->direction = i;
		if ((bs->dwellcnt = bs->dwell) < 0)
			bs->dwellcnt = -bs->dwellcnt;
		
		if (    bs->type == BTYPE_SEGMENT_SINGLE
		    ||  bs->type == BTYPE_SEGMENT_RANDOM)
			bs->radius = random() % (NRADII - 1);

		--bs->counter;
	}

	set_alpha_by_dwell (bs);
	glBlendFunc (GL_DST_COLOR, GL_SRC_ALPHA);
	glColor4fv (bs->color);
	glRotatef (bs->direction * 360.0 / (GLfloat) g_blades, 0, 0, 1);
	if (bs->radius >= 0)
		draw_blink_blade (ctx, bs->radius, bs->radius + 1);
	else
		draw_blink_blade (ctx, 0, NRADII - 1);

	--bs->dwellcnt;

	return SUBDIV + SUBDIV;
}

static int
draw_blink_radial_sequential (lockward_context *ctx, struct blinkstate *bs)
{
	if (bs->dwellcnt < 0) {
		if (bs->counter <= 0) {
			bs->drawfunc = NULL;
			return 0;
		}
		if ((bs->dwellcnt = bs->dwell) < 0)
			bs->dwellcnt = -bs->dwellcnt;
		--bs->counter;
	}

	set_alpha_by_dwell (bs);
	glBlendFunc (GL_DST_COLOR, GL_SRC_ALPHA);
	glColor4fv (bs->color);
	glRotatef ((bs->counter * bs->direction + (int) bs->val)
	            * 360.0 / (GLfloat) g_blades,
	           0, 0, 1);
	draw_blink_blade (ctx, 0, NRADII - 1);

	--bs->dwellcnt;

	return SUBDIV + SUBDIV;
}

static int
draw_blink_radial_doubleseq (lockward_context *ctx, struct blinkstate *bs)
{
	int polys;

	if (bs->dwellcnt < 0) {
		if (bs->counter <= 0) {
			bs->drawfunc = NULL;
			return 0;
		}
		if ((bs->dwellcnt = bs->dwell) < 0)
			bs->dwellcnt = -bs->dwellcnt;
		--bs->counter;
	}

	set_alpha_by_dwell (bs);
	glBlendFunc (GL_DST_COLOR, GL_SRC_ALPHA);
	glColor4fv (bs->color);

	glPushMatrix ();
	glRotatef (((int) bs->val + bs->counter) * 360.0 / (GLfloat) g_blades,
	           0, 0, 1);
	draw_blink_blade (ctx, 0, NRADII - 1);
	glPopMatrix ();
	polys = SUBDIV + SUBDIV;

	if (bs->counter  &&  bs->counter < g_blades / 2) {
		glRotatef (((int) bs->val - bs->counter)
		            * 360.0 / (GLfloat) g_blades,
		           0, 0, 1);
		draw_blink_blade (ctx, 0, NRADII - 1);
		polys += SUBDIV + SUBDIV;
	}

	--bs->dwellcnt;

	return polys;
}

static int
draw_blink_concentric_random (lockward_context *ctx, struct blinkstate *bs)
{
	int i;

	if (bs->dwellcnt < 0) {
		if (bs->counter <= 0) {
			bs->drawfunc = NULL;
			return 0;
		}

		do {
			i = random() % (NRADII - 1);
		} while (bs->val & (1 << i));
		bs->val |= (1 << i);
		bs->direction = i;
		if ((bs->dwellcnt = bs->dwell) < 0)
			bs->dwellcnt = -bs->dwellcnt;

		--bs->counter;
	}

	set_alpha_by_dwell (bs);
	glBlendFunc (GL_DST_COLOR, GL_SRC_ALPHA);
	glColor4fv (bs->color);
	glCallList (ctx->rings + bs->direction);

	--bs->dwellcnt;

	return g_blades * SUBDIV * 2;
}

static int
draw_blink_concentric_sequential (lockward_context *ctx, struct blinkstate *bs)
{
	if (bs->dwellcnt < 0) {
		if (bs->counter <= 0) {
			bs->drawfunc = NULL;
			return 0;
		}
		if ((bs->dwellcnt = bs->dwell) < 0)
			bs->dwellcnt = -bs->dwellcnt;
		--bs->counter;
	}

	set_alpha_by_dwell (bs);
	glBlendFunc (GL_DST_COLOR, GL_SRC_ALPHA);
	glColor4fv (bs->color);
	if (bs->direction > 0)
		glCallList (ctx->rings + (NRADII - 2) - bs->counter);
	else
		glCallList (ctx->rings + bs->counter);

	--bs->dwellcnt;

	return g_blades * SUBDIV * 2;
}

static int
draw_blink_segment_scatter (lockward_context *ctx, struct blinkstate *bs)
{
	int i, polys = 0;

	if (bs->dwellcnt < 0) {
		if (bs->counter <= 0) {
			bs->drawfunc = NULL;
			return 0;
		}

		/*
		 * Init random noise array.  On average, 1/4 of the bits will
		 * be set, which should look nice.  (1/2 looks too busy.)
		 */
		for (i = g_blades;  --i >= 0; )
			bs->noise[i] = random() & random()
			             & ((1 << (NRADII - 1)) - 1);

		if ((bs->dwellcnt = bs->dwell) < 0)
			bs->dwellcnt = -bs->dwellcnt;
		--bs->counter;
	}

	set_alpha_by_dwell (bs);
	glBlendFunc (GL_DST_COLOR, GL_SRC_ALPHA);
	glColor4fv (bs->color);

	for (i = g_blades;  --i >= 0; ) {
		register uint32_t	bits;
		int			inner, outer;

		/*
		 * Find consecutive runs of 1 bits.  Keep going until we run
		 * out of them.
		 */
		for (bits = bs->noise[i];  bits; ) {
			inner = ffs (bits) - 1;
			bits = ~bits & ~((1 << inner) - 1);
			outer = ffs (bits) - 1;
			bits = ~bits & ~((1 << outer) - 1);

			glPushMatrix ();
			glRotatef (i * 360.0 / (GLfloat) g_blades, 0, 0, 1);
			draw_blink_blade (ctx, inner, outer);
			glPopMatrix ();

			polys += SUBDIV + SUBDIV;
		}
	}

	--bs->dwellcnt;

	return polys;
}

static void
random_blink (lockward_context *ctx, struct blinkstate *bs)
{
	bs->color[0]	=
	bs->color[1]	=
	bs->color[2]	=
	bs->color[3]	= 1.0;
	bs->dwellcnt	= -1;
	bs->radius	= -1;
	bs->dwell	= calc_interval_frames
			  (ctx, g_blinkdwell_min, g_blinkdwell_max);
	if (random() & 2)
		bs->dwell = -bs->dwell;

	bs->type = random() % MAX_BTYPE;

	switch (bs->type) {
	case BTYPE_RADIAL_SINGLE:
	case BTYPE_SEGMENT_SINGLE:
		bs->drawfunc = draw_blink_radial_random;
		bs->val = 0;
		bs->counter = 1;
		break;
	case BTYPE_RADIAL_RANDOM:
	case BTYPE_SEGMENT_RANDOM:
		bs->drawfunc = draw_blink_radial_random;
		bs->val = 0;
		bs->counter = g_blades;
		break;
	case BTYPE_RADIAL_SEQ:
		bs->drawfunc = draw_blink_radial_sequential;
		bs->val = random() % g_blades;  /*  Initial offset  */
		bs->direction = random() & 8  ?  1 :  -1;
		bs->counter = g_blades;
		break;
	case BTYPE_RADIAL_DOUBLESEQ:
		bs->drawfunc = draw_blink_radial_doubleseq;
		bs->val = random() % g_blades;  /*  Initial offset  */
		bs->counter = g_blades / 2 + 1;
		break;
	case BTYPE_CONCENTRIC_SINGLE:
		bs->drawfunc = draw_blink_concentric_random;
		bs->val = 0;
		bs->counter = 1;
		break;
	case BTYPE_CONCENTRIC_RANDOM:
		bs->drawfunc = draw_blink_concentric_random;
		bs->val = 0;
		bs->counter = NRADII - 1;
		break;
	case BTYPE_CONCENTRIC_SEQ:
		bs->drawfunc = draw_blink_concentric_sequential;
		bs->direction = random() & 8  ?  1 :  -1;
		bs->counter = NRADII - 1;
		break;
	case BTYPE_SEGMENT_SCATTER:
		bs->drawfunc = draw_blink_segment_scatter;
		bs->counter = random() % (g_blades / 2) + (g_blades / 2) + 1;
		break;
	}
}


/***************************************************************************
 * Main rendering routine.
 */
ENTRYPOINT void
draw_lockward (ModeInfo *mi)
{
	lockward_context	*ctx = &g_ctx[MI_SCREEN (mi)];
	spinnerstate	*ss;
	Display		*dpy = MI_DISPLAY(mi);
	Window		window = MI_WINDOW(mi);
	int		i, n;

	GLfloat scolor[4] = {0.0, 0.0, 0.0, 0.5};

	if (!ctx->glx_context)
		return;

	glXMakeCurrent (MI_DISPLAY (mi), MI_WINDOW (mi), *(ctx->glx_context));


	glClear (GL_COLOR_BUFFER_BIT);

	if (ctx->blendmode)
		glBlendFunc (GL_ONE, GL_ONE);
	else
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix ();
	glLoadIdentity ();

	mi->polygon_count = 0;

	for (n = NSPINNERS;  --n >= 0; ) {
		ss = &ctx->spinners[n];

		/*  Set color.  */
		i = ss->ccolor >> COLORIDX_SHF;
		scolor[0] = ss->colors[i].red   / 65535.0;
		scolor[1] = ss->colors[i].green / 65535.0;
		scolor[2] = ss->colors[i].blue  / 65535.0;
		glColor4fv (scolor);

		glPushMatrix ();
		glRotatef (ss->rot - ss->rotcount * ss->rotinc, 0, 0, 1);
		for (i = ss->nblades;  --i >= 0; ) {
			glPushMatrix ();
			glRotatef (360.0 * i / ss->nblades, 0, 0, 1);

			glBegin (GL_TRIANGLE_FAN);
			glCallList (ctx->blades_outer + ss->bladeidx[i].outer);
			glCallList (ctx->blades_inner + ss->bladeidx[i].inner);
			glEnd ();

			glPopMatrix ();
			mi->polygon_count += SUBDIV + SUBDIV;
		}
		glPopMatrix ();

		/*  Advance rotation.  */
		if (ss->rotcount) {
			if (ss->rotcount > 0)
				--ss->rotcount;
		} else {
			if (ss->rotinc == 0.0)
				random_blade_rot (ctx, ss);
			else {
				/*  Compute # of ticks to sit idle.  */
				ss->rotinc = 0.0;
				ss->rotcount =
				 calc_interval_frames (ctx,
				                       g_rotateidle_min,
				                       g_rotateidle_max);
			}
		}

		/*  Advance colors.  */
		if ((ss->ccolor += ss->colorinc) >= ss->ncolors)
			ss->ccolor -= ss->ncolors;
		else if (ss->ccolor < 0)
			ss->ccolor += ss->ncolors;
	}

	if (g_blink_p) {
		if (ctx->blink.drawfunc) {
			mi->polygon_count +=
			 ctx->blink.drawfunc (ctx, &ctx->blink);
		} else {
			if (ctx->nextblink > 0)
				--ctx->nextblink;
			else {
				/* Compute # of frames for blink idle time. */
				ctx->nextblink =
				 calc_interval_frames (ctx,
				                       g_blinkidle_min,
				                       g_blinkidle_max);
				random_blink (ctx, &ctx->blink);
			}
		}
	}
	glPopMatrix ();

	if (MI_IS_FPS (mi)) do_fps (mi);
	glFinish();

	glXSwapBuffers (dpy, window);
}


/***************************************************************************
 * Initialization/teardown.
 */
ENTRYPOINT void 
init_lockward (ModeInfo *mi)
{
	lockward_context	*ctx;
	int		i, n;

	if (!g_ctx) {
		g_ctx = (lockward_context *) calloc (MI_NUM_SCREENS (mi),
		                                   sizeof (lockward_context));
		if (!g_ctx) {
			fprintf (stderr, "%s: can't allocate context.\n",
			         progname);
			exit (1);
		}
	}
	ctx = &g_ctx[MI_SCREEN (mi)];

	ctx->glx_context = init_GL (mi);

	reshape_lockward (mi, MI_WIDTH (mi), MI_HEIGHT (mi));

	glEnable (GL_CULL_FACE);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);

	glShadeModel (GL_FLAT);
	glFrontFace (GL_CW);

	ctx->blades_outer	= glGenLists (NRADII);
	ctx->blades_inner	= glGenLists (NRADII);
	ctx->rings		= glGenLists (NRADII - 1);
	ctx->blendmode		= 0;
	ctx->fps		= 1000000 / MI_DELAY (mi);
	ctx->nextblink		= calc_interval_frames
				   (ctx, g_blinkidle_min, g_blinkidle_max);
	ctx->blink.drawfunc	= NULL;
	ctx->blink.noise	= malloc (sizeof (uint32_t) * g_blades);
	if (!ctx->blink.noise) {
		fprintf (stderr, "Can't allocate noise array.\n");
		exit (1);
	}

	gen_blade_arcs (ctx);
	gen_rings (ctx);

	for (i = NSPINNERS;  --i >= 0; ) {
		spinnerstate *ss = &ctx->spinners[i];

		ss->rot		= 0.0;
		ss->rotcount	= -1;

		/*  Establish rotation  */
		random_blade_rot (ctx, ss);

		/*
		 * Establish color cycling path and rate.  Rate avoids zero.
		 */
		ss->ncolors = 128;
		ss->colorinc = (random() & ((2 << COLORIDX_SHF) - 1))
		             - (1 << COLORIDX_SHF);
		if (ss->colorinc >= 0)
			++ss->colorinc;

		ss->colors = (XColor *) calloc (ss->ncolors, sizeof (XColor));
		if (!ss->colors) {
			fprintf (stderr,
			         "Can't allocate XColors for spinner %d.\n",
			         i);
			exit (1);
		}
		make_smooth_colormap (0, 0, 0,
				      ss->colors, &ss->ncolors,
				      False, 0, False);
		ss->ncolors <<= COLORIDX_SHF;

		/*
		 * Create blades.
		 */
		ss->nblades	= g_blades;
		ss->bladeidx	= malloc (sizeof (bladestate) * g_blades);
		if (!ss->bladeidx) {
			fprintf (stderr, "Can't allocate blades.\n");
			exit (1);
		}
		for (n = g_blades;  --n >= 0; ) {
			/*
			 * Establish blade radii.  Can't be equal.  Ensure
			 * outer > inner.
			 */
			do {
				ss->bladeidx[n].outer = random() & 7;
				ss->bladeidx[n].inner = random() & 7;
			} while (ss->bladeidx[n].outer ==
			         ss->bladeidx[n].inner);

			if (ss->bladeidx[n].outer < ss->bladeidx[n].inner) {
				uint8_t	tmp;

				tmp = ss->bladeidx[n].outer;
				ss->bladeidx[n].outer = ss->bladeidx[n].inner;
				ss->bladeidx[n].inner = tmp;
			}
		}
	}
}

static void
free_lockward (lockward_context *ctx)
{
	int i;

	if (ctx->blink.noise)
		free (ctx->blink.noise);
	if (glIsList (ctx->rings))
		glDeleteLists (ctx->rings, NRADII - 1);
	if (glIsList (ctx->blades_outer))
		glDeleteLists (ctx->blades_outer, NRADII);
	if (glIsList (ctx->blades_inner))
		glDeleteLists (ctx->blades_inner, NRADII);

	for (i = NSPINNERS;  --i >= 0; ) {
		spinnerstate *ss = &ctx->spinners[i];

		if (ss->colors)
			free (ss->colors);
		if (ss->bladeidx)
			free (ss->bladeidx);
	}
}

ENTRYPOINT void
release_lockward (ModeInfo *mi)
{
	int i;

	if (!g_ctx)
		return;

	for (i = MI_NUM_SCREENS (mi);  --i >= 0; ) {
		if (g_ctx[i].glx_context)
			glXMakeCurrent (MI_DISPLAY (mi), MI_WINDOW (mi),
			                *(g_ctx[i].glx_context));
		free_lockward (&g_ctx[i]);
	}

	FreeAllGL (mi);
	free (g_ctx);  g_ctx = NULL;
}


XSCREENSAVER_MODULE ("Lockward", lockward)

#endif /* USE_GL */

/*  vim:se ts=8 sts=8 sw=8:  */
