/* -*- Mode: C; tab-width: 4 -*- */
/* galaxy --- spinning galaxies */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)galaxy.c	4.04 97/07/28 xlockmore";
#endif

/* Originally done by Uli Siegmund <uli@wombat.okapi.sub.org> on Amiga
 *   for EGS in Cluster
 * Port from Cluster/EGS to C/Intuition by Harald Backert
 * Port to X11 and incorporation into xlockmore by Hubert Feyrer
 *   <hubert.feyrer@rz.uni-regensburg.de>
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
 * Revision History:
 * 10-May-97: jwz@jwz.org: turned into a standalone program.
 * 18-Apr-97: Memory leak fixed by Tom Schmidt <tschmidt@micron.com>
 * 07-Apr-97: Modified by Dave Mitchell <davem@magnet.com>
 *            random star sizes
 *            colors change depending on velocity
 * 23-Oct-94: Modified by David Bagley <bagleyd@tux.org>
 * 10-Oct-94: Add colors by Hubert Feyer
 * 30-Sep-94: Initial port by Hubert Feyer
 * 09-Mar-94: VMS can generate a random number 0.0 which results in a
 *            division by zero, corrected by Jouk Jansen
 *            <joukj@hrem.stm.tudelft.nl>
 */

#ifdef STANDALONE
# define PROGCLASS					"Galaxy"
# define HACK_INIT					init_galaxy
# define HACK_DRAW					draw_galaxy
# define galaxy_opts				xlockmore_opts
# define DEFAULTS	"*delay:		100  \n"			\
					"*count:		-5   \n"			\
					"*cycles:		250  \n"			\
					"*size:			-3   \n"			\
					"*ncolors:		64   \n"
# define UNIFORM_COLORS
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

static Bool tracks;

#define DEF_TRACKS "False"

static XrmOptionDescRec opts[] =
{
	{"-tracks", ".galaxy.tracks", XrmoptionNoArg, (caddr_t) "on"},
	{"+tracks", ".galaxy.tracks", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & tracks, "tracks", "Tracks", DEF_TRACKS, t_Bool}
};

static OptionStruct desc[] =
{
	{"-/+tracks", "turn on/off star tracks"}
};

ModeSpecOpt galaxy_opts = { 2, opts, 1, vars, desc };


#define FLOATRAND ((double) LRAND() / ((double) MAXRAND))

#if 0
#define WRAP       1		/* Warp around edges */
#define BOUNCE     1		/* Bounce from borders */
#endif

#define MINSIZE       1
#define MINGALAXIES    1
#define MAX_STARS    300
#define MAX_IDELTAT    50
/* These come originally from the Cluster-version */
#define DEFAULT_GALAXIES  2
#define DEFAULT_STARS    1000
#define DEFAULT_HITITERATIONS  7500
#define DEFAULT_IDELTAT    200	/* 0.02 */
#define EPSILON 0.00000001

#define sqrt_EPSILON 0.0001

#define DELTAT (MAX_IDELTAT * 0.0001)

#define GALAXYRANGESIZE  0.1
#define GALAXYMINSIZE  0.1
#define QCONS    0.001

/*-
 *  The following is enabled, it does not look that good for some.
 *  (But it looks great for me.)  Maybe velocities should be measured
 *  relative to their galaxy-centers instead of absolute.
 */
#if 0
#undef NO_VELOCITY_COLORING */	/* different colors for different speeds */
#endif

#define COLORBASE  8
  /* Colors for stars start here */
#define COLORSTEP  (NUMCOLORS/COLORBASE)	/* 8 colors per galaxy */

#define drawStar(x,y,size) if(size<=1) XDrawPoint(display,window,gc,x,y);\
  else XFillArc(display,window,gc,x,y,size,size,0,23040)

typedef struct {
	double      pos[3], vel[3];
	int         px, py;
	int         color;
	int         size;
} Star;

typedef struct {
	int         mass;
	int         nstars;
	Star       *stars;
	double      pos[3], vel[3];
	int         galcol;
} Galaxy;

typedef struct {
	struct {
		int         left;	/* x minimum */
		int         right;	/* x maximum */
		int         top;	/* y minimum */
		int         bottom;	/* y maximum */
	} clip;
	double      mat[3][3];	/* Movement of stars(?) */
	double      scale;	/* Scale */
	int         midx;	/* Middle of screen, x */
	int         midy;	/* Middle of screen, y */
	double      size;	/* */
	double      diff[3];	/* */
	Galaxy     *galaxies;	/* the Whole Universe */
	int         ngalaxies;	/* # galaxies */
	int         f_hititerations;	/* # iterations before restart */
	int         step;	/* */
} unistruct;

static unistruct *universes = NULL;

static void
free_galaxies(unistruct * gp)
{
	if (gp->galaxies != NULL) {
		int         i;

		for (i = 0; i < gp->ngalaxies; i++) {
			Galaxy     *gt = &gp->galaxies[i];

			if (gt->stars != NULL)
				(void) free((void *) gt->stars);
		}
		(void) free((void *) gp->galaxies);
		gp->galaxies = NULL;
	}
}

static void
startover(ModeInfo * mi)
{
	unistruct  *gp = &universes[MI_SCREEN(mi)];
	int         size = MI_SIZE(mi);
	int         i, j;	/* more tmp */
	double      w1, w2;	/* more tmp */
	double      d, v, w, h;	/* yet more tmp */

	gp->step = 0;

	if (MI_BATCHCOUNT(mi) < -MINGALAXIES)
		free_galaxies(gp);
	gp->ngalaxies = MI_BATCHCOUNT(mi);
	if (gp->ngalaxies < -MINGALAXIES)
		gp->ngalaxies = NRAND(-gp->ngalaxies - MINGALAXIES + 1) + MINGALAXIES;
	else if (gp->ngalaxies < MINGALAXIES)
		gp->ngalaxies = MINGALAXIES;
	if (gp->galaxies == NULL)
		gp->galaxies = (Galaxy *) calloc(gp->ngalaxies, sizeof (Galaxy));

	for (i = 0; i < gp->ngalaxies; ++i) {
		Galaxy     *gt = &gp->galaxies[i];
		double      sinw1, sinw2, cosw1, cosw2;

		gt->galcol = NRAND(COLORBASE - 2);
		if (gt->galcol > 1)
			gt->galcol += 2;	/* Mult 8; 16..31 no green stars */
		/* Galaxies still may have some green stars but are not all green. */

		if (gt->stars != NULL) {
			(void) free((void *) gt->stars);
			gt->stars = NULL;
		}
		gt->nstars = (NRAND(MAX_STARS / 2)) + MAX_STARS / 2;
		gt->stars = (Star *) malloc(gt->nstars * sizeof (Star));
		w1 = 2.0 * M_PI * FLOATRAND;
		w2 = 2.0 * M_PI * FLOATRAND;
		sinw1 = SINF(w1);
		sinw2 = SINF(w2);
		cosw1 = COSF(w1);
		cosw2 = COSF(w2);

		gp->mat[0][0] = cosw2;
		gp->mat[0][1] = -sinw1 * sinw2;
		gp->mat[0][2] = cosw1 * sinw2;
		gp->mat[1][0] = 0.0;
		gp->mat[1][1] = cosw1;
		gp->mat[1][2] = sinw1;
		gp->mat[2][0] = -sinw2;
		gp->mat[2][1] = -sinw1 * cosw2;
		gp->mat[2][2] = cosw1 * cosw2;

		gt->vel[0] = FLOATRAND * 2.0 - 1.0;
		gt->vel[1] = FLOATRAND * 2.0 - 1.0;
		gt->vel[2] = FLOATRAND * 2.0 - 1.0;
		gt->pos[0] = -gt->vel[0] * DELTAT *
			gp->f_hititerations + FLOATRAND - 0.5;
		gt->pos[1] = -gt->vel[1] * DELTAT *
			gp->f_hititerations + FLOATRAND - 0.5;
		gt->pos[2] = -gt->vel[2] * DELTAT *
			gp->f_hititerations + FLOATRAND - 0.5;

		gt->mass = (int) (FLOATRAND * 1000.0) + 1;

		gp->size = GALAXYRANGESIZE * FLOATRAND + GALAXYMINSIZE;

		for (j = 0; j < gt->nstars; ++j) {
			Star       *st = &gt->stars[j];
			double      sinw, cosw;

			w = 2.0 * M_PI * FLOATRAND;
			sinw = SINF(w);
			cosw = COSF(w);
			d = FLOATRAND * gp->size;
			h = FLOATRAND * exp(-2.0 * (d / gp->size)) / 5.0 * gp->size;
			if (FLOATRAND < 0.5)
				h = -h;
			st->pos[0] = gp->mat[0][0] * d * cosw + gp->mat[1][0] * d * sinw +
				gp->mat[2][0] * h + gt->pos[0];
			st->pos[1] = gp->mat[0][1] * d * cosw + gp->mat[1][1] * d * sinw +
				gp->mat[2][1] * h + gt->pos[1];
			st->pos[2] = gp->mat[0][2] * d * cosw + gp->mat[1][2] * d * sinw +
				gp->mat[2][2] * h + gt->pos[2];

			v = sqrt(gt->mass * QCONS / sqrt(d * d + h * h));
			st->vel[0] = -gp->mat[0][0] * v * sinw + gp->mat[1][0] * v * cosw +
				gt->vel[0];
			st->vel[1] = -gp->mat[0][1] * v * sinw + gp->mat[1][1] * v * cosw +
				gt->vel[1];
			st->vel[2] = -gp->mat[0][2] * v * sinw + gp->mat[1][2] * v * cosw +
				gt->vel[2];

			st->vel[0] *= DELTAT;
			st->vel[1] *= DELTAT;
			st->vel[2] *= DELTAT;

			st->px = 0;
			st->py = 0;

			if (size < -MINSIZE)
				st->size = NRAND(-size - MINSIZE + 1) + MINSIZE;
			else if (size < MINSIZE)
				st->size = MINSIZE;
			else
				st->size = size;
		}
	}

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

#if 0
	(void) printf("ngalaxies=%d, f_hititerations=%d\n",
		      gp->ngalaxies, gp->f_hititerations);
	(void) printf("f_deltat=%g\n", DELTAT);
	(void) printf("Screen: ");
	(void) printf("%dx%d pixel (%d-%d, %d-%d)\n",
	  (gp->clip.right - gp->clip.left), (gp->clip.bottom - gp->clip.top),
	       gp->clip.left, gp->clip.right, gp->clip.top, gp->clip.bottom);
#endif /*0 */
}

void
init_galaxy(ModeInfo * mi)
{
	unistruct  *gp;

	if (universes == NULL) {
		if ((universes = (unistruct *) calloc(MI_NUM_SCREENS(mi),
						sizeof (unistruct))) == NULL)
			return;
	}
	gp = &universes[MI_SCREEN(mi)];

	gp->f_hititerations = MI_CYCLES(mi);

	gp->clip.left = 0;
	gp->clip.top = 0;
	gp->clip.right = MI_WIN_WIDTH(mi);
	gp->clip.bottom = MI_WIN_HEIGHT(mi);

	gp->scale = (double) (gp->clip.right + gp->clip.bottom) / 8.0;
	gp->midx = gp->clip.right / 2;
	gp->midy = gp->clip.bottom / 2;
	startover(mi);
}

void
draw_galaxy(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	unistruct  *gp = &universes[MI_SCREEN(mi)];
	double      d;		/* tmp */
	int         i, j, k;	/* more tmp */

	for (i = 0; i < gp->ngalaxies; ++i) {
		Galaxy     *gt = &gp->galaxies[i];

		for (j = 0; j < gp->galaxies[i].nstars; ++j) {
			Star       *st = &gt->stars[j];
			double      v0 = st->vel[0];
			double      v1 = st->vel[1];
			double      v2 = st->vel[2];

			for (k = 0; k < gp->ngalaxies; ++k) {
				Galaxy     *gtk = &gp->galaxies[k];
				double      d0 = gtk->pos[0] - st->pos[0];
				double      d1 = gtk->pos[1] - st->pos[1];
				double      d2 = gtk->pos[2] - st->pos[2];

				d = d0 * d0 + d1 * d1 + d2 * d2;
				if (d > EPSILON)
					d = gt->mass / (d * sqrt(d)) * DELTAT * DELTAT * QCONS;
				else
					d = gt->mass / (EPSILON * sqrt_EPSILON) * DELTAT * DELTAT * QCONS;
				v0 += d0 * d;
				v1 += d1 * d;
				v2 += d2 * d;
			}

			st->vel[0] = v0;
			st->vel[1] = v1;
			st->vel[2] = v2;

#ifndef NO_VELOCITY_COLORING
			d = (v0 * v0 + v1 * v1 + v2 * v2) / (3.0 * DELTAT * DELTAT);
			if (d > (double) COLORSTEP)
				st->color = COLORSTEP * gt->galcol + COLORSTEP - 1;
			else
				st->color = COLORSTEP * gt->galcol + ((int) d) % COLORSTEP;
#endif
			st->pos[0] += v0;
			st->pos[1] += v1;
			st->pos[2] += v2;

			if (st->px >= gp->clip.left &&
			    st->px <= gp->clip.right - st->size &&
			    st->py >= gp->clip.top &&
			    st->py <= gp->clip.bottom - st->size) {
				XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
				drawStar(st->px, st->py, st->size);
			}
			st->px = (int) (st->pos[0] * gp->scale) + gp->midx;
			st->py = (int) (st->pos[1] * gp->scale) + gp->midy;


#ifdef WRAP
			if (st->px < gp->clip.left) {
				(void) printf("wrap l -> r\n");
				st->px = gp->clip.right;
			}
			if (st->px > gp->clip.right) {
				(void) printf("wrap r -> l\n");
				st->px = gp->clip.left;
			}
			if (st->py > gp->clip.bottom) {
				(void) printf("wrap b -> t\n");
				st->py = gp->clip.top;
			}
			if (st->py < gp->clip.top) {
				(void) printf("wrap t -> b\n");
				st->py = gp->clip.bottom;
			}
#endif /*WRAP */


			if (st->px >= gp->clip.left &&
			    st->px <= gp->clip.right - st->size &&
			    st->py >= gp->clip.top &&
			    st->py <= gp->clip.bottom - st->size) {
				if (MI_NPIXELS(mi) > 2)
#ifdef NO_VELOCITY_COLORING
					XSetForeground(display, gc, MI_PIXEL(mi, COLORSTEP * gt->galcol));
#else
					XSetForeground(display, gc, MI_PIXEL(mi, st->color));
#endif
				else
					XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
				if (tracks)
					drawStar(st->px + 1, st->py, st->size);
				else
					drawStar(st->px, st->py, st->size);
			}
		}

		for (k = i + 1; k < gp->ngalaxies; ++k) {
			Galaxy     *gtk = &gp->galaxies[k];
			double      d0 = gtk->pos[0] - gt->pos[0];
			double      d1 = gtk->pos[1] - gt->pos[1];
			double      d2 = gtk->pos[2] - gt->pos[2];

			d = d0 * d0 + d1 * d1 + d2 * d2;
			if (d > EPSILON)
				d = gt->mass * gt->mass / (d * sqrt(d)) * DELTAT * QCONS;
			else
				d = gt->mass * gt->mass / (EPSILON * sqrt_EPSILON) * DELTAT * QCONS;
			d0 *= d;
			d1 *= d;
			d2 *= d;
			gt->vel[0] += d0 / gt->mass;
			gt->vel[1] += d1 / gt->mass;
			gt->vel[2] += d2 / gt->mass;
			gtk->vel[0] -= d0 / gtk->mass;
			gtk->vel[1] -= d1 / gtk->mass;
			gtk->vel[2] -= d2 / gtk->mass;
		}
		gt->pos[0] += gt->vel[0] * DELTAT;
		gt->pos[1] += gt->vel[1] * DELTAT;
		gt->pos[2] += gt->vel[2] * DELTAT;
	}

	gp->step++;
	if (gp->step > gp->f_hititerations * 4)
		startover(mi);
}

void
release_galaxy(ModeInfo * mi)
{
	if (universes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_galaxies(&universes[screen]);
		(void) free((void *) universes);
		universes = NULL;
	}
}

void
refresh_galaxy(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
