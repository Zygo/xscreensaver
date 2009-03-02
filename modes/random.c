/* -*- Mode: C; tab-width: 4 -*- */
/* random --- run random modes for a certain duration */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)random.c	4.07 97/11/24 xlockmore";

#endif

/*-
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
 * Revision History:
 * 10-May-97: Made more compatible with xscreensaver :)
 * 18-Mar-96: Ron Hitchens <ron@idiom.com>
 *		Re-coded for the ModeInfo calling scheme.  Added the
 *		change hook.  Get ready for 3.8 release.
 * 23-Dec-95: Ron Hitchens <ron@idiom.com>
 *		Re-coded pickMode() to keep track of the modes, so
 *		that all modes are tried before there are any repeats.
 *		Also prevent a mode from being picked twice in a row
 *		(could happen as first pick after refreshing the list).
 * 04-Sep-95: Written by Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 *
 */

#ifdef STANDALONE
#define PROGCLASS "Random"
#define HACK_INIT init_random
#define HACK_DRAW draw_random
#define random_opts xlockmore_opts
#define DEFAULTS ""
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#define DEF_DURATION	"60"	/* 0 == infinite duration */
#define DEF_MODELIST	""
#define DEF_SEQUENTIAL "False"

static int  duration;
static char *modelist;
static Bool sequential;

static XrmOptionDescRec opts[] =
{
	{"-duration", ".random.duration", XrmoptionSepArg, (caddr_t) NULL},
	{"-modelist", ".random.modelist", XrmoptionSepArg, (caddr_t) NULL},
	{"-sequential", ".random.sequential", XrmoptionNoArg, (caddr_t) "on"},
	{"+sequential", ".random.sequential", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & duration, "duration", "Duration", DEF_DURATION, t_Int},
    {(caddr_t *) & modelist, "modelist", "Modelist", DEF_MODELIST, t_String},
{(caddr_t *) & sequential, "sequential", "Sequential", DEF_SEQUENTIAL, t_Bool}
};

static OptionStruct desc[] =
{
	{"-duration num", "how long a mode runs before changing to another"},
	{"-modelist string", "list of modes to randomly choose from"},
	{"-/+sequential", "turn on/off picking of modes sequentially"}
};

ModeSpecOpt random_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   random_description =
{"random", "init_random", "draw_random", "release_random",
 "refresh_random", "change_random", NULL, &random_opts,
 1, 1, 1, 1, 64, 1.0, "",
#ifdef USE_BOMB
 "Shows a random mode from above except blank and bomb", 0, NULL};

#else
 "Shows a random mode from above except blank", 0, NULL};

#endif

#endif

#ifdef USE_BOMB
#define NUMSPECIAL	3	/* bomb, blank, random */
#else
#define NUMSPECIAL	2	/* blank, random */
#endif

#define GC_SAVE_VALUES (GCFunction|GCLineWidth|GCLineStyle|GCCapStyle|GCJoinStyle|GCGraphicsExposures|GCFont|GCSubwindowMode)

extern int  delay;
extern int  batchcount;
extern int  cycles;
extern int  size;
extern int  ncolors;
extern float saturation;
extern char *imagefile;

#define MAXMODECHARS 14

#ifdef USE_GL
static char gl_modes[][MAXMODECHARS] =
{
	"cage", "gears", "moebius", "morph3d", "pipes", "rubik",
	"sproingies", "stairs", "superquadrics"
};

#define NUMGL (sizeof (gl_modes) / sizeof (gl_modes[0]))
#else
static char gl_modes[][MAXMODECHARS] =
{""};

#define NUMGL 0
#endif

#if defined( USE_XPM ) || defined( USE_XPMINC )
static char xpm_modes[][MAXMODECHARS] =
{
	"bat", "cartoon", "image", "flag", "life", "life1d", "maze", "puzzle"
};

#define NUMXPM (sizeof (xpm_modes) / sizeof (xpm_modes[0]))
#else
/* They will not really be xpm_modes anymore... */
static char xpm_modes[][MAXMODECHARS] =
{
	"bat", "image", "flag", "life", "life1d", "maze", "puzzle"
};

#define NUMXPM (sizeof (xpm_modes) / sizeof (xpm_modes[0]))

#endif

static char write_modes[][MAXMODECHARS] =
{
	"mandelbrot", "swirl", "tube"	/* XPM modes are usually writable too ... */
};

#define NUMWRITE (sizeof (write_modes) / sizeof (write_modes[0]))

static char nice_modes[][MAXMODECHARS] =
{
	"blot", "bouboule", "bug",
	"clock", "crystal",
	"daisy" "dclock", "demon" "deco",
	"eyes",
	"fadeplot", "flag", "flame",
	"grav",
	"helix", "hyper",
	"ico", "image",
	"kaleid",
  "laser", "life", "life1d", "life3d", "lightning", "lisa", "lissie", "loop",
	"marquee",
	"nose",
	"penrose", "petal", "puzzle", "pyro",
	"qix",
	"roll", "rotor",
	"shape", "sierpinski", "spline", "star", "swarm",
	"triangle", "tube", "turtle",
	"vines", "voters",
	"wire",
	"world", "worm"
};

#define NUMNICE (sizeof (nice_modes) / sizeof (nice_modes[0]))

static char use3d_modes[][MAXMODECHARS] =
{
	"bouboule", "pyro", "star", "worm"
};

#define NUMUSE3D (sizeof (use3d_modes) / sizeof (use3d_modes[0]))

static char mouse_modes[][MAXMODECHARS] =
{
	"eyes", "julia", "swarm"
};

#define NUMMOUSE (sizeof (mouse_modes) / sizeof (mouse_modes[0]))

static char automata_modes[][MAXMODECHARS] =
{
	"ant", "bug", "demon", "dilemma", "life", "life1d", "life3d", "loop",
	"voters", "wator", "wire"
};

#define NUMAUTOMATA (sizeof (automata_modes) / sizeof (automata_modes[0]))

static char fractal_modes[][MAXMODECHARS] =
{
	"coral", "drift", "flame", "forest", "julia", "hop",
	"ifs", "lightning", "mandelbrot", "mountain",
	"sierpinski", "strange", "triangle", "turtle", "vines"
};

#define NUMFRACTAL (sizeof (fractal_modes) / sizeof (fractal_modes[0]))


static char geometry_modes[][MAXMODECHARS] =
{
	"braid", "fadeplot", "helix", "hyper", "ico", "kaleid",
	"laser", "lisa", "lissie", "penrose", "petal", "qix",
	"shape", "sphere", "spiral", "spline"
};

#define NUMGEOMETRY (sizeof (geometry_modes) / sizeof (geometry_modes[0]))

static char space_modes[][MAXMODECHARS] =
{
	"bouboule", "galaxy", "grav", "star", "world"
};

#define NUMSPACE (sizeof (space_modes) / sizeof (space_modes[0]))

typedef struct {
	XGCValues   gcvs;
	int         fix;
} randomstruct;

static int  currentmode = -1;
static int  previousmode = -1;
static unsigned long starttime;
static int *modes;
static int  nmodes;
static Bool change_now = False;

static randomstruct *randoms;

static int
pickMode(void)
{
	static int *mode_indexes = NULL;
	static int  mode_count = 0;
	static int  last_mode = -1, last_index = -1;
	int         mode, i;

	if (mode_indexes == NULL) {
		if ((mode_indexes = (int *) calloc(nmodes, sizeof (int))) ==
		    NULL) {
			if (sequential)
				return modes[0];
			else
				return modes[NRAND(nmodes)];
		}
	}
	if (mode_count == 0) {
		for (i = 0; i < nmodes; i++) {
			mode_indexes[i] = modes[i];
		}
		mode_count = nmodes;
	}
	if (mode_count == 1) {
		/* only one left, let's use that one */
		last_index = -1;
		return (last_mode = mode_indexes[--mode_count]);
	} else {
		/* pick a random slot in the list, check for last */
		if (sequential) {
			last_index = i = (last_index + 1) % nmodes;
		} else
			while (mode_indexes[i = NRAND(mode_count)] == last_mode);
	}

	mode = mode_indexes[i];	/* copy out chosen mode */
	/* move mode at end of list to slot vacated by chosen mode, dec count */
	mode_indexes[i] = mode_indexes[--mode_count];
	return (last_mode = mode);	/* remember last mode picked */
}

static
char       *
strpmtok(int *sign, char *str)
{
	static int  nextsign = 0;
	static char *loc;
	char       *p, *r;

	if (str)
		loc = str;
	if (nextsign) {
		*sign = nextsign;
		nextsign = 0;
	}
	p = loc - 1;
	for (;;) {
		switch (*++p) {
			case '+':
				*sign = 1;
				continue;
			case '-':
				*sign = -1;
				continue;
			case ' ':
			case ',':
			case '\t':
			case '\n':
				continue;
			case 0:
				loc = p;
				return NULL;
		}
		break;
	}
	r = p;

	for (;;) {
		switch (*++p) {
			case '+':
				nextsign = 1;
				break;
			case '-':
				nextsign = -1;
				break;
			case ' ':
			case ',':
			case '\t':
			case '\n':
			case 0:
				break;
			default:
				continue;
		}
		break;
	}
	if (*p) {
		*p = 0;
		loc = p + 1;
	} else
		loc = p;

	return r;
}

static void
parsemodelist(ModeInfo * mi)
{
	int         i, sign = 1, j;
	char       *p;

	modes = (int *) calloc(numprocs - 1, sizeof (int));

	p = strpmtok(&sign, (modelist) ? modelist : "");

	while (p) {
		if (!strcmp(p, "all")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				modes[i] = (sign > 0);
		} else if (!strcmp(p, "allgl")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMGL; j++)
					if (!strcmp(gl_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allxpm")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMXPM; j++)
					if (!strcmp(xpm_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allwrite")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMWRITE; j++)
					if (!strcmp(write_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allnice")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMNICE; j++)
					if (!strcmp(nice_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "all3d")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMUSE3D; j++)
					if (!strcmp(use3d_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allmouse")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMMOUSE; j++)
					if (!strcmp(mouse_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allautomata")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMAUTOMATA; j++)
					if (!strcmp(automata_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allfractal")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMFRACTAL; j++)
					if (!strcmp(fractal_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allgeometry")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMGEOMETRY; j++)
					if (!strcmp(geometry_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else if (!strcmp(p, "allspace")) {
			for (i = 0; i < numprocs - NUMSPECIAL; i++)
				for (j = 0; j < NUMSPACE; j++)
					if (!strcmp(space_modes[j], LockProcs[i].cmdline_arg))
						modes[i] = (sign > 0);
		} else {
			for (i = 0; i < numprocs - 1; i++)
				if (!strcmp(p, LockProcs[i].cmdline_arg))
					break;
			if (i < numprocs - 1)
				modes[i] = (sign > 0);
			else
				(void) fprintf(stderr, "unrecognized mode \"%s\"\n", p);
		}
		p = strpmtok(&sign, (char *) NULL);
	}

	nmodes = 0;
	for (i = 0; i < numprocs - 1; i++)
		if (modes[i])
			modes[nmodes++] = i;
	if (!nmodes) {		/* empty list */
		for (i = 0; i < numprocs - NUMSPECIAL; i++)
			modes[i] = i;
		nmodes = i;
	}
	if (MI_WIN_IS_DEBUG(mi)) {
		(void) fprintf(stderr, "%d mode%s: ", nmodes, ((nmodes == 1) ? "" : "s"));
		for (i = 0; i < nmodes; i++)
			(void) fprintf(stderr, "%d ", modes[i]);
		(void) fprintf(stderr, "\n");
	}
}

static
void
setMode(ModeInfo * mi, int newmode)
{
	randomstruct *rp = &randoms[MI_SCREEN(mi)];
	int         i;

	previousmode = currentmode;
	currentmode = newmode;

/* FIX THIS GLOBAL ACCESS */
	delay = MI_DELAY(mi) = LockProcs[currentmode].def_delay;
	batchcount = MI_BATCHCOUNT(mi) = LockProcs[currentmode].def_batchcount;
	cycles = MI_CYCLES(mi) = LockProcs[currentmode].def_cycles;
	size = MI_SIZE(mi) = LockProcs[currentmode].def_size;
	ncolors = MI_NCOLORS(mi) = LockProcs[currentmode].def_ncolors;
	saturation = MI_SATURATION(mi) = LockProcs[currentmode].def_saturation;
	imagefile = MI_IMAGEFILE(mi) = LockProcs[currentmode].def_imagefile;

	for (i = 0; i < MI_NUM_SCREENS(mi); i++) {

		XChangeGC(MI_DISPLAY(mi), MI_GC(mi), GC_SAVE_VALUES,
			  &(rp->gcvs));		/* Not sure if this is right for multiscreens */
		randoms[i].fix = True;
	}
	if (MI_WIN_IS_VERBOSE(mi))
		(void) fprintf(stderr, "mode: %s\n", LockProcs[currentmode].cmdline_arg);
}

void
init_random(ModeInfo * mi)
{
	randomstruct *rp;
	int         i;

	if (randoms == NULL) {
		if ((randoms = (randomstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (randomstruct))) == NULL)
			return;
	}
	rp = &randoms[MI_SCREEN(mi)];

	if (currentmode < 0) {
		parsemodelist(mi);

		for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
			(void) XGetGCValues(MI_DISPLAY(mi), MI_GC(mi),
					    GC_SAVE_VALUES, &(rp->gcvs));
		}
		setMode(mi, pickMode());
		starttime = seconds();
		if (duration < 0)
			duration = 0;
	}
	if (rp->fix) {
		fixColormap(MI_DISPLAY(mi), MI_WINDOW(mi), MI_SCREEN(mi), MI_NCOLORS(mi),
		MI_SATURATION(mi), MI_WIN_IS_MONO(mi), MI_WIN_IS_INSTALL(mi),
			    MI_WIN_IS_INROOT(mi), MI_WIN_IS_INWINDOW(mi), MI_WIN_IS_VERBOSE(mi));
		rp->fix = False;
	}
	call_init_hook(&LockProcs[currentmode], mi);
}

void
draw_random(ModeInfo * mi)
{
	int         scrn = MI_SCREEN(mi);
	randomstruct *rp = &randoms[scrn];
	int         newmode;
	unsigned long now = seconds();
	int         has_run = (duration == 0) ? 0 : (int) (now - starttime);
	static int  do_init = 0;

	if ((scrn == 0) && do_init) {
		do_init = 0;
	}
	if ((scrn == 0) && (change_now || (has_run > duration))) {
		newmode = pickMode();

		MI_CLEARWINDOW(mi);

		setMode(mi, newmode);
		starttime = now;
		do_init = 1;
		change_now = False;
	}
	if (rp->fix) {
		fixColormap(MI_DISPLAY(mi), MI_WINDOW(mi), MI_SCREEN(mi), MI_NCOLORS(mi),
		MI_SATURATION(mi), MI_WIN_IS_MONO(mi), MI_WIN_IS_INSTALL(mi),
			    MI_WIN_IS_INROOT(mi), MI_WIN_IS_INWINDOW(mi), MI_WIN_IS_VERBOSE(mi));
		rp->fix = False;
	}
	if (do_init) {
		call_init_hook(&LockProcs[currentmode], mi);
	}
	call_callback_hook(&LockProcs[currentmode], mi);
}

void
refresh_random(ModeInfo * mi)
{
	call_refresh_hook(&LockProcs[currentmode], mi);
}

void
change_random(ModeInfo * mi)
{
	if (MI_SCREEN(mi) == 0)
		change_now = True;	/* force a change on next draw callback */

	draw_random(mi);
}

void
release_random(ModeInfo * mi)
{
	if (previousmode >= 0 && previousmode != currentmode)
		call_release_hook(&LockProcs[previousmode], mi);
	previousmode = currentmode;
}
