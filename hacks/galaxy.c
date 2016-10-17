/* -*- Mode: C; tab-width: 4 -*- */
/* galaxy --- spinning galaxies */
/* #include<math.h>*/
#if 0
static const char sccsid[] = "@(#)galaxy.c 4.04 97/07/28 xlockmore";
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
 * 26-Aug-00: robert.nagtegaal@phil.uu.nl and roland@tschai.demon.nl:
 *            various improvements
 * 10-May-97: jwz@jwz.org: turned into a standalone program.
 * 18-Apr-97: Memory leak fixed by Tom Schmidt <tschmidt@micron.com>
 * 07-Apr-97: Modified by Dave Mitchell <davem@magnet.com>
 * 23-Oct-94: Modified by David Bagley <bagleyd@bigfoot.com>
 *  random star sizes
 *  colors change depending on velocity
 * 10-Oct-94: Add colors by Hubert Feyer
 * 30-Sep-94: Initial port by Hubert Feyer
 * 09-Mar-94: VMS can generate a random number 0.0 which results in a
 *            division by zero, corrected by Jouk Jansen
 *            <joukj@crys.chem.uva.nl>
 */

#ifdef STANDALONE
# define DEFAULTS	"*delay:  20000  \n"   \
					"*count:  -5     \n"   \
					"*cycles:  250   \n"   \
					"*ncolors:  64   \n" \
					"*fpsSolid:  true   \n" \
					"*ignoreRotation: True \n" \

# define UNIFORM_COLORS
# include "xlockmore.h"    /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"     /* from the xlockmore distribution */
#endif /* !STANDALONE */

static Bool tracks;
static Bool spin;
static Bool dbufp;

#define DEF_TRACKS "True"
#define DEF_SPIN   "True"
#define DEF_DBUF   "True"

static XrmOptionDescRec opts[] =
{
 {"-tracks", ".galaxy.tracks", XrmoptionNoArg, "on"},
 {"+tracks", ".galaxy.tracks", XrmoptionNoArg, "off"},
 {"-spin",   ".galaxy.spin",   XrmoptionNoArg, "on"},
 {"+spin",   ".galaxy.spin",   XrmoptionNoArg, "off"},
 {"-dbuf",   ".galaxy.dbuf",   XrmoptionNoArg, "on"},
 {"+dbuf",   ".galaxy.dbuf",   XrmoptionNoArg, "off"},
};

static argtype vars[] =
{
 {&tracks, "tracks", "Tracks", DEF_TRACKS, t_Bool},
 {&spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
 {&dbufp,  "dbuf",   "Dbuf",   DEF_DBUF,   t_Bool}, 
};

static OptionStruct desc[] =
{
 {"-/+tracks", "turn on/off star tracks"},
 {"-/+spin",   "do/don't spin viewpoint"},
 {"-/+dbuf",   "turn on/off double buffering."},
};

ENTRYPOINT ModeSpecOpt galaxy_opts =
{sizeof opts / sizeof opts[0], opts,
 sizeof vars / sizeof vars[0], vars, desc};


#define FLOATRAND ((double) LRAND() / ((double) MAXRAND))

#if 0
#define WRAP       1  /* Warp around edges */
#define BOUNCE     1  /* Bounce from borders */
#endif

#define MINSIZE       1
#define MINGALAXIES    2
#define MAX_STARS    3000
#define MAX_IDELTAT    50
/* These come originally from the Cluster-version */
#define DEFAULT_GALAXIES  3
#define DEFAULT_STARS    1000
#define DEFAULT_HITITERATIONS  7500
#define DEFAULT_IDELTAT    200 /* 0.02 */
#define EPSILON 0.00000001

#define sqrt_EPSILON 0.0001

#define DELTAT (MAX_IDELTAT * 0.0001)

#define GALAXYRANGESIZE  0.1
#define GALAXYMINSIZE  0.15
#define QCONS    0.001


#define COLORBASE  16
/* colors per galaxy */
/* #define COLORSTEP  (NUMCOLORS/COLORBASE) */
# define COLORSTEP (MI_NCOLORS(mi)/COLORBASE)


typedef struct {
 double      pos[3], vel[3];
} Star;


typedef struct {
 int         mass;
 int         nstars;
 Star       *stars;
 XPoint     *oldpoints;
 XPoint     *newpoints;
 double      pos[3], vel[3];
 int         galcol;
} Galaxy;

typedef struct {
 double      mat[3][3]; /* Movement of stars(?) */
 double      scale; /* Scale */
 int         midx; /* Middle of screen, x */
 int         midy; /* Middle of screen, y */
 double      size; /* */
 double      diff[3]; /* */
 Galaxy     *galaxies; /* the Whole Universe */
 int         ngalaxies; /* # galaxies */
 int         f_hititerations; /* # iterations before restart */
 int         step; /* */
 double      rot_y; /* rotation of eye around center of universe, around
y-axis*/
 double      rot_x; /* rotation of eye around center of universe, around
x-axis */
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
   if (gt->oldpoints != NULL)
       (void) free((void *) gt->oldpoints);
   if (gt->newpoints != NULL)
       (void) free((void *) gt->newpoints);
  }
  (void) free((void *) gp->galaxies);
  gp->galaxies = NULL;
 }
}

static void
startover(ModeInfo * mi)
{
 unistruct  *gp = &universes[MI_SCREEN(mi)];
 int         i, j; /* more tmp */
 double      w1, w2; /* more tmp */
 double      d, v, w, h; /* yet more tmp */

 gp->step = 0;
 gp->rot_y = 0;
 gp->rot_x = 0;

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
   gt->galcol += 2; /* Mult 8; 16..31 no green stars */
  /* Galaxies still may have some green stars but are not all green. */

  if (gt->stars != NULL) {
   (void) free((void *) gt->stars);
   gt->stars = NULL;
  }
  gt->nstars = (NRAND(MAX_STARS / 2)) + MAX_STARS / 2;
  gt->stars = (Star *) malloc(gt->nstars * sizeof (Star));
  gt->oldpoints = (XPoint *) malloc(gt->nstars * sizeof (XPoint));
  gt->newpoints = (XPoint *) malloc(gt->nstars * sizeof (XPoint));

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
  gt->pos[0] = -gt->vel[0] * DELTAT * gp->f_hititerations + FLOATRAND -
0.5;
  gt->pos[1] = -gt->vel[1] * DELTAT * gp->f_hititerations + FLOATRAND -
0.5;
  gt->pos[2] = -gt->vel[2] * DELTAT * gp->f_hititerations + FLOATRAND -
0.5;

  gt->mass = (int) (FLOATRAND * 1000.0) + 1;

  gp->size = GALAXYRANGESIZE * FLOATRAND + GALAXYMINSIZE;

  for (j = 0; j < gt->nstars; ++j) {
   Star       *st = &gt->stars[j];
   XPoint     *oldp = &gt->oldpoints[j];
   XPoint     *newp = &gt->newpoints[j];

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

   oldp->x = 0;
   oldp->y = 0;
   newp->x = 0;
   newp->y = 0;
  }

 }

 XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

#if 0
 (void) printf("ngalaxies=%d, f_hititerations=%d\n", gp->ngalaxies,
gp->f_hititerations);
 (void) printf("f_deltat=%g\n", DELTAT);
 (void) printf("Screen: ");
#endif /*0 */
}

ENTRYPOINT void
init_galaxy(ModeInfo * mi)
{
 unistruct  *gp;

 if (universes == NULL) {
  if ((universes = (unistruct *) calloc(MI_NUM_SCREENS(mi),
      sizeof (unistruct))) == NULL)
   return;
 }
 gp = &universes[MI_SCREEN(mi)];

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  dbufp = False;
# endif

 gp->f_hititerations = MI_CYCLES(mi);

 gp->scale = (double) (MI_WIN_WIDTH(mi) + MI_WIN_HEIGHT(mi)) / 8.0;
 gp->midx =  MI_WIN_WIDTH(mi)  / 2;
 gp->midy =  MI_WIN_HEIGHT(mi) / 2;
 startover(mi);
}

ENTRYPOINT void
draw_galaxy(ModeInfo * mi)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  GC          gc = MI_GC(mi);
  unistruct  *gp = &universes[MI_SCREEN(mi)];
  double      d, eps, cox, six, cor, sir;  /* tmp */
  int         i, j, k; /* more tmp */
  XPoint    *dummy = NULL;

  if (! dbufp)
    XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

  if(spin){
    gp->rot_y += 0.01;
    gp->rot_x += 0.004;
  }

  cox = COSF(gp->rot_y);
  six = SINF(gp->rot_y);
  cor = COSF(gp->rot_x);
  sir = SINF(gp->rot_x);

  eps = 1/(EPSILON * sqrt_EPSILON * DELTAT * DELTAT * QCONS);

  for (i = 0; i < gp->ngalaxies; ++i) {
    Galaxy     *gt = &gp->galaxies[i];

    for (j = 0; j < gp->galaxies[i].nstars; ++j) {
      Star       *st = &gt->stars[j];
      XPoint     *newp = &gt->newpoints[j];
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
          d = gtk->mass / (d * sqrt(d)) * DELTAT * DELTAT * QCONS;
        else
          d = gtk->mass / (eps * sqrt(eps));
        v0 += d0 * d;
        v1 += d1 * d;
        v2 += d2 * d;
      }

      st->vel[0] = v0;
      st->vel[1] = v1;
      st->vel[2] = v2;

      st->pos[0] += v0;
      st->pos[1] += v1;
      st->pos[2] += v2;

      newp->x = (short) (((cox * st->pos[0]) - (six * st->pos[2])) *
                         gp->scale) + gp->midx;
      newp->y = (short) (((cor * st->pos[1]) - (sir * ((six * st->pos[0]) +
                                                       (cox * st->pos[2]))))
                         * gp->scale) + gp->midy;

    }

    for (k = i + 1; k < gp->ngalaxies; ++k) {
      Galaxy     *gtk = &gp->galaxies[k];
      double      d0 = gtk->pos[0] - gt->pos[0];
      double      d1 = gtk->pos[1] - gt->pos[1];
      double      d2 = gtk->pos[2] - gt->pos[2];

      d = d0 * d0 + d1 * d1 + d2 * d2;
      if (d > EPSILON)
        d = 1 / (d * sqrt(d)) * DELTAT * QCONS;
      else
        d = 1 / (EPSILON * sqrt_EPSILON) * DELTAT * QCONS;

      d0 *= d;
      d1 *= d;
      d2 *= d;
      gt->vel[0] += d0 * gtk->mass;
      gt->vel[1] += d1 * gtk->mass;
      gt->vel[2] += d2 * gtk->mass;
      gtk->vel[0] -= d0 * gt->mass;
      gtk->vel[1] -= d1 * gt->mass;
      gtk->vel[2] -= d2 * gt->mass;
    }

    gt->pos[0] += gt->vel[0] * DELTAT;
    gt->pos[1] += gt->vel[1] * DELTAT;
    gt->pos[2] += gt->vel[2] * DELTAT;

    if (dbufp) {
      XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
      XDrawPoints(display, window, gc, gt->oldpoints, gt->nstars,
                  CoordModeOrigin);
    }
    XSetForeground(display, gc, MI_PIXEL(mi, COLORSTEP * gt->galcol));
    XDrawPoints(display, window, gc, gt->newpoints, gt->nstars,
                CoordModeOrigin);

    dummy = gt->oldpoints;
    gt->oldpoints = gt->newpoints;
    gt->newpoints = dummy;
  }

  gp->step++;
  if (gp->step > gp->f_hititerations * 4)
    startover(mi);
}

ENTRYPOINT void
reshape_galaxy(ModeInfo * mi, int width, int height)
{
  XClearWindow (MI_DISPLAY (mi), MI_WINDOW(mi));
  init_galaxy (mi);
}

ENTRYPOINT void
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

ENTRYPOINT void
refresh_galaxy(ModeInfo * mi)
{
 /* Do nothing, it will refresh by itself */
}

ENTRYPOINT Bool
galaxy_handle_event (ModeInfo *mi, XEvent *event)
{
  if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      reshape_galaxy (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  return False;
}


XSCREENSAVER_MODULE ("Galaxy", galaxy)
