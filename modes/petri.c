/* -*- Mode: C; tab-width: 4 -*- */
/* petri ---  */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)petri.c	5.04 2002/07/19 xlockmore";

#endif

/* petri, simulate mold in a petri dish. v2.7
 * by Dan Bornstein, danfuzz@milk.com
 * with help from Jamie Zawinski, jwz@jwz.org
 * Copyright (c) 1992-1999 Dan Bornstein.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 *
 * Brief description of options/resources:
 *
 * delay: the delay in microseconds between iterations
 * size: the size of a cell in pixels
 * count: the number of different kinds of mold (minimum: 2)
 * diaglim: the age limit for diagonal growth as a multiplier of orthogonal
 *   growth (minimum: 1, maximum 2). 1 means square growth, 1.414
 *   (i.e., sqrt(2)) means approximately circular growth, 2 means diamond
 *   growth.
 * anychan: the chance (fraction, between 0 and 1) that at each iteration,
 *   any new cell will be born
 * minorchan: the chance (fraction, between 0 and 1) that, given that new
 *   cells will be added, that only two will be added (a minor cell birth
 *   event)
 * instantdeathchan: the chance (fraction, between 0 and 1) that, given
 *   that death and destruction will happen, that instead of using plague
 *   cells, death will be instantaneous
 * minlifespan: the minimum lifespan of a colony (before black death ensues)
 * maxlifespan: the maximum lifespan of a colony (before black death ensues)
 * minlifespeed: the minimum speed for living cells as a fraction of the
 *   maximum possible speed (fraction, between 0 and 1)
 * maxlifespeed: the maximum speed for living cells as a fraction of the
 *   maximum possible speed (fraction, between 0 and 1)
 * mindeathspeed: the minimum speed for black death cells as a fraction of the
 *   maximum possible speed (fraction, between 0 and 1)
 * maxdeathspeed: the maximum speed for black death cells as a fraction of the
 *   maximum possible speed (fraction, between 0 and 1)
 * originalcolors: if true, count must be 8 or less and the colors are a
 *   fixed set of primary and secondary colors (the artist's original choices)
 *
 * Interesting settings:
 *
 *      petri -originalcolors -size 8
 *      petri -size 2
 *      petri -size 8 -diaglim 1.8
 *      petri -diaglim 1.1
 *
 *      petri -count 4 -anychan 0.01 -minorchan 1 \
 *              -minlifespan 2000 -maxlifespan 5000
 *
 *      petri -count 3 -anychan 1 -minlifespan 100000 \
 *              -instantdeathchan 0
 *
 *      petri -minlifespeed 0.02 -maxlifespeed 0.03 -minlifespan 1 \
 *              -maxlifespan 1 -instantdeathchan 0 -minorchan 0 \
 *              -anychan 0.3 -delay 4000
 *
 * Revision History:
 * 22 Jul-2002: xlockmore version by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
 */

#ifdef STANDALONE
#define MODE_petri
#define PROGCLASS "Petri"
#define HACK_INIT init_petri
#define HACK_DRAW draw_petri
#define petri_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*size: 4 \n" \
 "*ncolors: 8 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#include "automata.h"
#endif /* STANDALONE */

#ifdef MODE_petri

#include <math.h>

#define FLOAT float
#define RAND_FLOAT (((FLOAT) (LRAND() & 0xffff)) / ((FLOAT) 0x10000))

#define DEF_MEMTHROTTLE "22M"
#define DEF_DIAGLIM "1.414"
#define DEF_ANYCHAN "0.0015"
#define DEF_MINORCHAN "0.5"
#define DEF_INSTANTDEATHCHAN "0.2"
#define DEF_MINLIFESPAN "500"
#define DEF_MAXLIFESPAN "1500"
#define DEF_MINLIFESPEED "0.04"
#define DEF_MAXLIFESPEED "0.13"
#define DEF_MINDEATHSPEED "0.42"
#define DEF_MAXDEATHSPEED "0.46"

#define REDRAWSTEP 2000         /* How many cells to draw per cycle */

static char* memThrottle;
static FLOAT st_diaglim;
static FLOAT orthlim = 1.0;
static FLOAT st_anychan;
static FLOAT st_minorchan;
static FLOAT st_instantdeathchan;
static int st_minlifespan;
static int st_maxlifespan;
static FLOAT st_minlifespeed;
static FLOAT st_maxlifespeed;
static FLOAT st_mindeathspeed;
static FLOAT st_maxdeathspeed;

static XrmOptionDescRec opts[] =
{
   {(char *) "-anychan", (char *) ".petri.anychan", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-diaglim", (char *) ".petri.diaglim", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-instantdeathchan", (char *) ".petri.instantdeathchan", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-memthrottle", (char *) ".petri.memthrottle", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-maxlifespan", (char *) ".petri.maxlifespan", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-minlifespan", (char *) ".petri.minlifespan", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-maxlifespeed", (char *) ".petri.maxlifespeed", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-minlifespeed", (char *) ".petri.minlifespeed", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-maxdeathspeed", (char *) ".petri.maxdeathspeed", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-mindeathspeed", (char *) ".petri.mindeathspeed", XrmoptionSepArg, (caddr_t) NULL},
   {(char *) "-minorchan", (char *) ".petri.minorchan", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
   {(void *) &st_anychan, (char *) "anychan",
   (char *) "Anychan", (char *) DEF_ANYCHAN , t_Float},
   {(void *) &st_diaglim, (char *) "diaglim",
   (char *) "Diaglim", (char *) DEF_DIAGLIM , t_Float},
   {(void *) &st_instantdeathchan, (char *) "instantdeathchan",
   (char *) "Instantdeathchan", (char *) DEF_DIAGLIM , t_Float},
   {(void *) &memThrottle, (char *) "memthrottle",
   (char *) "Memthrottle", (char *) DEF_MEMTHROTTLE , t_String},
   {(void *) &st_maxlifespan, (char *) "maxlifespan",
   (char *) "Maxlifespan", (char *) DEF_MAXLIFESPAN , t_Int},
   {(void *) &st_minlifespan, (char *) "minlifespan",
   (char *) "Minlifespan", (char *) DEF_MINLIFESPAN , t_Int},
   {(void *) &st_maxlifespeed, (char *) "maxlifespeed",
   (char *) "Maxlifespeed", (char *) DEF_MAXLIFESPEED , t_Float},
   {(void *) &st_minlifespeed, (char *) "minlifespeed",
   (char *) "Minlifespeed", (char *) DEF_MINLIFESPEED , t_Float},
   {(void *) &st_maxdeathspeed, (char *) "maxdeathspeed",
   (char *) "Maxdeathspeed", (char *) DEF_MAXDEATHSPEED , t_Float},
   {(void *) &st_mindeathspeed, (char *) "mindeathspeed",
   (char *) "Mindeathspeed", (char *) DEF_MINDEATHSPEED , t_Float},
   {(void *) &st_minorchan, (char *) "minorchan",
   (char *) "Minorchan", (char *) DEF_MINORCHAN , t_Float}
};

static OptionStruct desc[] =
{
     {(char *) "-anychan float", (char *) "The chance that new cell will be born"},
   {(char *) "-diaglim float", (char *) "The age limit for diagonal growth"},
   {(char *) "-instantdeathchan float", (char *) "Instant death chance"},
   {(char *) "-mem-throttle string", (char *) "memThrottle"},
   {(char *) "-maxlifespan num", (char *) "the maximum lifespan of a colony"},
   {(char *) "-minlifespan num", (char *) "the minimum lifespan of a colony"},
   {(char *) "-maxlifespeed float", (char *) "the maximum speed for living cells"},
   {(char *) "-minlifespeed float", (char *) "the minimum speed for living cells"},
   {(char *) "-maxdeathspeed float", (char *) "the maximum speed for black death cells"},
   {(char *) "-mindeathspeed float", (char *) "the minimum speed for black death cells"},
   {(char *) "-minorchan float", (char *) "The chance on a minor cell birth"}
};

ModeSpecOpt petri_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   petri_description =
{"petri", "init_petri", "draw_petri", "release_petri",
 "refresh_petri", "init_petri", (char *) NULL, &petri_opts,
 10000, 1, 1, 4, 8, 1.0, "",
 "Shows a mold simulation in a petri dish", 0, NULL};

#endif

typedef struct cell_s
{
    unsigned char col;              /*  0      */
    unsigned char isnext;           /*  1      */
    unsigned char nextcol;          /*  2      */
                                    /*  3      */
    struct cell_s *next;            /*  4      */
    struct cell_s *prev;            /*  8    - */
    FLOAT speed;                    /* 12      */
    FLOAT growth;                   /* 16 20 - */
    FLOAT nextspeed;                /* 20 28   */
                                    /* 24 36 - */
} cell;

#define PETRIBITS(n,w,h)\
  if ((sp->pixmaps[sp->init_bits]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1))==None){\
  free_petri(display,sp); return False;} else {sp->init_bits++;}

#define cell_x(c) (((c) - sp->arr) % sp->arr_width)
#define cell_y(c) (((c) - sp->arr) / sp->arr_width)

typedef struct {
   cell *arr;
   cell *head;
   cell *tail;
   int count;
   Bool originalcolors;
   GC *coloredGCs;
   int blastcount;
   int windowWidth;
   int windowHeight;
   int arr_width;
   int arr_height;
   int xSize;
   int ySize;
   int xOffset;
   int yOffset;
   FLOAT diaglim;
   FLOAT anychan;
   FLOAT minorchan;
   FLOAT instantdeathchan;
   int minlifespan;
   int maxlifespan;
   FLOAT minlifespeed;
   FLOAT maxlifespeed;
   FLOAT mindeathspeed;
   FLOAT maxdeathspeed;
   int redrawing, redrawpos;
   Colormap cmap;
   Bool mono;
   int         init_bits;
   unsigned char colors[NUMSTIPPLES - 1];
   GC          stippledGC;
   Pixmap      pixmaps[NUMSTIPPLES - 1];
} petristruct;

static petristruct *petries = (petristruct *) NULL;

static int random_life_value (petristruct *sp)
{
    return (int) ((RAND_FLOAT * (sp->maxlifespan - sp->minlifespan)) +
		  sp->minlifespan);
}

static void
free_petri(Display *display, petristruct *sp)
{
    int n, shade;

    if (sp->coloredGCs != NULL) {
	for (n = 0; n < sp->count*2; n++)
		if (sp->coloredGCs[n] != None)
			XFreeGC(display, sp->coloredGCs[n]);
	free(sp->coloredGCs);
	sp->coloredGCs = (GC *) NULL;
    }
    if (sp->stippledGC != None) {
        XFreeGC(display, sp->stippledGC);
        sp->stippledGC = None;
    }
    for (shade = 0; shade < sp->init_bits; shade++) {
        XFreePixmap(display, sp->pixmaps[shade]);
    }
    sp->init_bits = 0;
    if (sp->arr != NULL) {
		free(sp->arr);
		sp->arr = (cell *) NULL;
    }
    if (sp->head != NULL) {
		free(sp->head);
		sp->head = (cell *) NULL;
    }
    if (sp->tail != NULL) {
		free(sp->tail);
		sp->tail = (cell *) NULL;
    }
}

static int setup_random_colormap (ModeInfo * mi)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
   int lose = 0;
   int ncolors = sp->count - 1;
   int n;
   XColor *colors = (XColor *) NULL;
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);

    colors = (XColor *) calloc (sizeof(*colors), sp->count*2);
    if (colors == NULL) {
	free_petri(display, sp);
        return False;
    }
    colors[0].pixel = MI_BLACK_PIXEL(mi);

    if (MI_NPIXELS(mi) <= 2) {
 	sp->mono = True;
    } else
        make_random_colormap (
#ifdef STANDALONE
			  display , window ,
#else
			  mi ,
#endif
			  sp->cmap ,
			  colors+1, &ncolors, True, True, 0);
    if (ncolors < 1) {
	sp->mono = True;
    }
    ncolors++;
    sp->count = ncolors;

    if (sp->mono) {
	if (colors != NULL) {
		XFree((caddr_t) colors);
		colors = (XColor *) NULL;
	}

        if (2 * sp->count > NUMSTIPPLES - 1)
		sp->count = (NUMSTIPPLES - 1) / 2;
	if (sp->stippledGC == None) {
	    XGCValues   gcv;

	    gcv.fill_style = FillOpaqueStippled;
	    if ((sp->stippledGC = XCreateGC(display, window, GCFillStyle,
		   &gcv)) == None) {
		free_petri(display, sp);
		return False;
	    }
	}
	if (sp->init_bits == 0) {
            int i;
	    for (i = 1; i < 2 * sp->count; i++) {
		PETRIBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
	    }
	}
	return True;
    }
    (void) memcpy (colors + sp->count, colors, sp->count * sizeof(*colors));
    colors[sp->count].pixel = MI_WHITE_PIXEL(mi);

    for (n = 1; n < sp->count; n++)
    {
	int m = n + sp->count;
	colors[n].red = colors[m].red / 2;
	colors[n].green = colors[m].green / 2;
	colors[n].blue = colors[m].blue / 2;

	if (!XAllocColor (display, sp->cmap, &colors[n]))
	{
	    lose++;
	    colors[n] = colors[m];
	}
    }
    if (lose)
    {
	(void) fprintf (stderr,
		 "petri: unable to allocate %d half-intensity colors.\n",
		 lose);
    }
    for (n = 0; n < sp->count*2; n++)
    {
        XGCValues gcv;

	gcv.foreground = colors[n].pixel;
	sp->coloredGCs[n] = XCreateGC (display, window, GCForeground, &gcv);
    }
    if (colors != NULL) {
	XFree((caddr_t) colors);
	colors = (XColor *) NULL;
    }
    return True;
}

static int setup_original_colormap (ModeInfo * mi)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
   int lose = 0;
   int n;
   XColor *colors = (XColor *) NULL;
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);

    if (MI_NPIXELS(mi) <= 2)
 	sp->mono = True;
    if (sp->mono) {
        if (2 * sp->count > NUMSTIPPLES - 1)
		sp->count = (NUMSTIPPLES - 1) / 2;
	if (sp->stippledGC == None) {
	    XGCValues   gcv;

	    gcv.fill_style = FillOpaqueStippled;
	    if ((sp->stippledGC = XCreateGC(display, window, GCFillStyle,
		   &gcv)) == None) {
		free_petri(display, sp);
		return False;
	    }
	}
	if (sp->init_bits == 0) {
            int i;
	    for (i = 1; i < 2 * sp->count; i++) {
		PETRIBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
	    }
	}
	return True;
    }

    colors = (XColor *) calloc (sizeof(*colors), sp->count*2);
    if (colors == NULL) {
	free_petri(display, sp);
        return False;
    }
    colors[0].pixel = MI_BLACK_PIXEL(mi);
    colors[sp->count].pixel = MI_WHITE_PIXEL(mi);

    for (n = 1; n < sp->count; n++)
    {
	int m = n + sp->count;
	colors[n].red =   ((n & 0x01) != 0) * 0x8000;
	colors[n].green = ((n & 0x02) != 0) * 0x8000;
	colors[n].blue =  ((n & 0x04) != 0) * 0x8000;

	if (!sp->mono && !XAllocColor (display, sp->cmap, &colors[n]))
	{
	    lose++;
	    colors[n] = colors[0];
	}

	colors[m].red   = colors[n].red + 0x4000;
	colors[m].green = colors[n].green + 0x4000;
	colors[m].blue  = colors[n].blue + 0x4000;

	if (!XAllocColor (display, sp->cmap, &colors[m]))
	{
	    lose++;
	    colors[m] = colors[sp->count];
	}
    }

    if (lose)
    {
	(void) fprintf (stderr,
		 "petri: unable to allocate %d colors.\n",
		 lose);
    }

    for (n = 0; n < sp->count*2; n++)
    {
        XGCValues gcv;

	gcv.foreground = colors[n].pixel;
	sp->coloredGCs[n] = XCreateGC (display, window, GCForeground, &gcv);
    }
    if (colors != NULL) {
	XFree((caddr_t) colors);
	colors = (XColor *) NULL;
    }
    return True;
}

static int setup_display (ModeInfo * mi)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);
    XWindowAttributes xgwa;

    int cell_size = MI_SIZE( mi );
    int osize, alloc_size, oalloc;
    int mem_throttle = 0;
    char *s;

    if (cell_size < 1) cell_size = 1;

    osize = cell_size;

   s = (char*) malloc( strlen( memThrottle ) + 1);
    if (s)
      {
        int n;
        char c;

	(void) strcpy( s , memThrottle );
        if (1 == sscanf (s, " %d M %c", &n, &c) ||
            1 == sscanf (s, " %d m %c", &n, &c))
          mem_throttle = n * (1 << 20);
        else if (1 == sscanf (s, " %d K %c", &n, &c) ||
                 1 == sscanf (s, " %d k %c", &n, &c))
          mem_throttle = n * (1 << 10);
        else if (1 == sscanf (s, " %d %c", &n, &c))
          mem_throttle = n;
        else
          {
            (void) fprintf (stderr,
		     "petri: invalid memThrottle \"%s\" (try \"10M\")\n", s);
            free_petri(display, sp);
	    return False;
          }

        free(s);
      }

    (void) XGetWindowAttributes (display, window, &xgwa);

    sp->originalcolors = ( NRAND( 3 ) == 1 );

    sp->count = MI_NCOLORS(mi);
    if ( sp->count < 2) sp->count = 2;

    /* number of colors can't be greater than the half depth of the screen. */
    if ( sp->count > (1L << (xgwa.depth-1)))
      sp->count = (1L << (xgwa.depth-1));

    /* Actually, since cell->col is of type char, this has to be small. */
    if ( sp->count >= (1L << ((sizeof( sp->arr[0].col) * 8) - 1)))
      sp->count = (1L << ((sizeof( sp->arr[0].col) * 8) - 1));


    if ( sp->originalcolors && ( sp->count > 8))
    {
	sp->count = 8;
    }

    sp->coloredGCs = (GC *) calloc (sizeof(GC), sp->count * 2);
    if (sp->coloredGCs == NULL) {
	free_petri(display, sp);
        return False;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->diaglim = 1.0 + (float) LRAND() / (float) MAXRAND;
   else
     sp->diaglim = st_diaglim;
    if (sp->diaglim < 1.0)
    {
	sp->diaglim = 1.0;
    }
    else if (sp->diaglim > 2.0)
    {
	sp->diaglim = 2.0;
    }
    sp->diaglim *= orthlim;

   if (MI_IS_FULLRANDOM(mi))
     sp->anychan = (float) pow( (double) LRAND() / (double) MAXRAND , 15.0 );
   else
     sp->anychan = st_anychan;
    if (sp->anychan < 0.0)
    {
	sp->anychan = 0.0;
    }
    else if (sp->anychan > 1.0)
    {
	sp->anychan = 1.0;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->minorchan = (float) LRAND() / (float) MAXRAND;
   else
     sp->minorchan = st_minorchan;
    if (sp->minorchan < 0.0)
    {
	sp->minorchan = 0.0;
    }
    else if (sp->minorchan > 1.0)
    {
	sp->minorchan = 1.0;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->instantdeathchan = (float) pow( (double) LRAND() / (double) MAXRAND ,
					 8.0 );
   else
     sp->instantdeathchan = st_instantdeathchan;
    if (sp->instantdeathchan < 0.0)
    {
	sp->instantdeathchan = 0.0;
    }
    else if (sp->instantdeathchan > 1.0)
    {
	sp->instantdeathchan = 1.0;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->minlifespan = NRAND( st_minlifespan ) + 1;
   else
     sp->minlifespan = st_minlifespan;
    if (sp->minlifespan < 1)
    {
	sp->minlifespan = 1;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->maxlifespan = NRAND( st_maxlifespan ) + sp->minlifespan;
   else
     sp->maxlifespan = st_maxlifespan;
    if (sp->maxlifespan < sp->minlifespan)
    {
	sp->maxlifespan = sp->minlifespan;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->minlifespeed = st_maxlifespeed * (float) LRAND() / (float) MAXRAND;
   else
     sp->minlifespeed = st_minlifespeed;
    if (sp->minlifespeed < 0.0)
    {
	sp->minlifespeed = 0.0;
    }
    else if (sp->minlifespeed > 1.0 )
    {
	sp->minlifespeed = 1.0;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->maxlifespeed = ( (st_maxlifespeed - sp->minlifespeed ) *
			 (float) LRAND() / (float) MAXRAND ) +
     sp->minlifespeed;
   else
     sp->maxlifespeed = st_maxlifespeed;
    if (sp->maxlifespeed < sp->minlifespeed)
    {
	sp->maxlifespeed = sp->minlifespeed;
    }
    else if (sp->maxlifespeed > 1.0)
    {
	sp->maxlifespeed = 1.0;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->mindeathspeed = st_maxdeathspeed * (float) LRAND() / (float) MAXRAND;
   else
     sp->mindeathspeed = st_mindeathspeed;
    if (sp->mindeathspeed < 0.0)
    {
	sp->mindeathspeed = 0.0;
    }
    else if (sp->mindeathspeed > 1.0)
    {
	sp->mindeathspeed = 1.0;
    }

   if (MI_IS_FULLRANDOM(mi))
     sp->maxdeathspeed = ( (st_maxdeathspeed - sp->mindeathspeed ) *
			 (float) LRAND() / (float) MAXRAND ) +
     sp->mindeathspeed;
   else
     sp->maxdeathspeed = st_maxdeathspeed;
    if (sp->maxdeathspeed < sp->mindeathspeed)
    {
	sp->maxdeathspeed = sp->mindeathspeed;
    }
    else if (sp->maxdeathspeed > 1.0)
    {
	sp->maxdeathspeed = 1.0;
    }

    sp->minlifespeed *= sp->diaglim;
    sp->maxlifespeed *= sp->diaglim;
    sp->mindeathspeed *= sp->diaglim;
    sp->maxdeathspeed *= sp->diaglim;

    sp->cmap = xgwa.colormap;

    sp->windowWidth = xgwa.width;
    sp->windowHeight = xgwa.height;

    sp->arr_width = sp->windowWidth / cell_size;
    sp->arr_height = sp->windowHeight / cell_size;

    alloc_size = sizeof(cell) * sp->arr_width * sp->arr_height;
    oalloc = alloc_size;

    if (mem_throttle > 0)
      while (cell_size < sp->windowWidth/10 &&
             cell_size < sp->windowHeight/10 &&
             alloc_size > mem_throttle)
        {
          cell_size++;
          sp->arr_width = sp->windowWidth / cell_size;
          sp->arr_height = sp->windowHeight / cell_size;
          alloc_size = sizeof(cell) * sp->arr_width * sp->arr_height;
        }

    if (osize != cell_size)
      {
        static int warned = 0;
        if (!warned)
          {
            (void) fprintf (stderr,
             "petri: throttling cell size from %d to %d because of %dM limit.\n",
                     osize, cell_size, mem_throttle / (1 << 20));
            (void) fprintf (stderr, "petri: %dx%dx%d = %.1fM, %dx%dx%d = %.1fM.\n",
                     sp->windowWidth, sp->windowHeight, osize,
                     ((float) oalloc) / (1 << 20),
                     sp->windowWidth, sp->windowHeight, cell_size,
                     ((float) alloc_size) / (1 << 20));
            warned = 1;
          }
      }

    sp->xSize = sp->windowWidth / sp->arr_width;
    sp->ySize = sp->windowHeight / sp->arr_height;
    if (sp->xSize > sp->ySize)
    {
	sp->xSize = sp->ySize;
    }
    else
    {
	sp->ySize = sp->xSize;
    }

    sp->xOffset = (sp->windowWidth - (sp->arr_width * sp->xSize)) / 2;
    sp->yOffset = (sp->windowHeight - (sp->arr_height * sp->ySize)) / 2;

    if (sp->originalcolors)
    {
	if (!setup_original_colormap (mi))
		return False;
    }
    else
    {
	/* if (!setup_random_colormap (mi)) *//* PseudoColor can error out */
	if (!setup_original_colormap (mi))
		return False;
    }
    return True;
}

static void drawblock (ModeInfo * mi , int x, int y, unsigned char c)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);

   if (sp->mono) {
	if (c == 0) {
	   XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
       XFillRectangle (display, window, MI_GC(mi),
		    x * sp->xSize + sp->xOffset, y * sp->ySize + sp->yOffset,
		    sp->xSize, sp->ySize);
	} else {
                XGCValues   gcv;

                gcv.stipple = sp->pixmaps[c - 1];
                gcv.foreground = MI_WHITE_PIXEL(mi);
                gcv.background = MI_BLACK_PIXEL(mi);
                XChangeGC(display, sp->stippledGC,
                          GCStipple | GCForeground | GCBackground, &gcv);
       XFillRectangle (display, window, sp->stippledGC,
		    x * sp->xSize + sp->xOffset, y * sp->ySize + sp->yOffset,
		    sp->xSize, sp->ySize);
	}
   } else
   if (sp->xSize == 1 && sp->ySize == 1)
    XDrawPoint (display, window, sp->coloredGCs[c], x + sp->xOffset, y +
		sp->yOffset);
  else
    XFillRectangle (display, window, sp->coloredGCs[c],
		    x * sp->xSize + sp->xOffset, y * sp->ySize + sp->yOffset,
		    sp->xSize, sp->ySize);
}

static int setup_arr (ModeInfo * mi)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
    int x, y;
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);

    if (sp->arr != NULL)
    {
	free(sp->arr);
    }

    if (sp->mono) {
      XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
      XFillRectangle (display, window, MI_GC(mi), 0, 0,
		    sp->windowWidth, sp->windowHeight);
    } else
      XFillRectangle (display, window, sp->coloredGCs[0], 0, 0,
		    sp->windowWidth, sp->windowHeight);

    sp->arr = (cell *) calloc (sizeof(cell), sp->arr_width * sp->arr_height);
    if (!sp->arr)
      {
        (void) fprintf (stderr, "petri: out of memory allocating %dx%d grid\n",
                 sp->arr_width, sp->arr_height);
	free_petri(display, sp);
        return False;
      }

    for (y = 0; y < sp->arr_height; y++)
    {
      int row = y * sp->arr_width;
	for (x = 0; x < sp->arr_width; x++)
	{
	    sp->arr[row+x].speed = 0.0;
	    sp->arr[row+x].growth = 0.0;
	    sp->arr[row+x].col = 0;
	    sp->arr[row+x].isnext = 0;
	    sp->arr[row+x].next = 0;
	    sp->arr[row+x].prev = 0;
	}
    }

    if (sp->head == NULL)
    {
	sp->head = (cell *) malloc (sizeof (cell));
    if (!sp->head)
      {
        (void) fprintf (stderr, "petri: out of memory allocating %dx%d grid\n",
                 sp->arr_width, sp->arr_height);
	free_petri(display, sp);
        return False;
    }
    }

    if (sp->tail == NULL)
    {
	sp->tail = (cell *) malloc (sizeof (cell));
    if (!sp->tail)
      {
        (void) fprintf (stderr, "petri: out of memory allocating %dx%d grid\n",
                 sp->arr_width, sp->arr_height);
	free_petri(display, sp);
        return False;
    }
    }

    sp->head->next = sp->tail;
    sp->head->prev = sp->head;
    sp->tail->next = sp->tail;
    sp->tail->prev = sp->head;

    sp->blastcount = random_life_value (sp);
    return True;
}

static void newcell (ModeInfo * mi , cell *c, unsigned char col, FLOAT spf)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
    if (! c) return;

    if (c->col == col) return;

    c->nextcol = col;
    c->nextspeed = spf;
    c->isnext = 1;

    if (c->prev == 0) {
	c->next = sp->head->next;
	c->prev = sp->head;
	sp->head->next = c;
	c->next->prev = c;
    }
}

static void killcell (ModeInfo * mi , cell *c)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];

   c->prev->next = c->next;
    c->next->prev = c->prev;
    c->prev = 0;
    c->speed = 0.0;
    drawblock (mi , cell_x(c), cell_y(c), c->col);
}

static int randblip (ModeInfo * mi , int doit)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
    int n;
    int b = 0;
    if (!doit
	&& (sp->blastcount-- >= 0)
	&& (RAND_FLOAT > sp->anychan))
    {
	return True;
    }

    if (sp->blastcount < 0)
    {
	b = 1;
	n = 2;
	sp->blastcount = random_life_value(sp);
	if (RAND_FLOAT < sp->instantdeathchan)
	{
	    /* clear everything every so often to keep from getting into a
	     * rut */
	    if (!setup_arr(mi)) {
               return False;
	    }
	    b = 0;
	}
    }
    else if (RAND_FLOAT <= sp->minorchan)
    {
	n = 2;
    }
    else
    {
	n = NRAND(3) + 3;
    }

    while (n--)
    {
	int x = NRAND(sp->arr_width);
	int y = NRAND(sp->arr_height);
	int c;
	FLOAT s;
	if (b)
	{
	    c = 0;
	    s = RAND_FLOAT * (sp->maxdeathspeed - sp->mindeathspeed) + sp->mindeathspeed;
	}
	else
	{
	    if ( sp->count <= 1 )
	      c = 1;
	    else
	      c = (NRAND(sp->count-1)) + 1;
	    s = RAND_FLOAT * (sp->maxlifespeed - sp->minlifespeed) + sp->minlifespeed;
	}
	newcell (mi , &sp->arr[y * sp->arr_width + x], c, s);
    }
    return True;
}

void draw_petri (ModeInfo * mi)
{
   petristruct *sp = &petries[MI_SCREEN(mi)];
    cell *a;

   if (petries == NULL)
	return;
   if (sp->arr == NULL)
      return;
   MI_IS_DRAWN(mi) = True;

    for (a = sp->head->next; a != sp->tail; a = a->next)
    {
	static XPoint all_coords[] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
				      {-1,  0}, { 1, 0}, {0, -1}, {0, 1},
				      {99, 99}};

        XPoint *coords = 0;

        if (a->speed == 0) continue;
        a->growth += a->speed;

	if (a->growth >= sp->diaglim)
	{
	    coords = all_coords;
	}
        else if (a->growth >= orthlim)
	{
	    coords = &all_coords[4];
	}
	else
	{
	    continue;
	}

	while (coords->x != 99)
	{
	    int x = cell_x(a) + coords->x;
	    int y = cell_y(a) + coords->y;
	    coords++;

	    if (x < 0) x = sp->arr_width - 1;
	    else if (x >= sp->arr_width) x = 0;

	    if (y < 0) y = sp->arr_height - 1;
	    else if (y >= sp->arr_height) y = 0;

	    newcell (mi , &sp->arr[y * sp->arr_width + x], a->col, a->speed);
	}

	if (a->growth >= sp->diaglim)
	    killcell (mi ,a);
    }

    if (!randblip (mi , (sp->head->next) == sp->tail))
	 return;
    for (a = sp->head->next; a != sp->tail; a = a->next)
    {
	if (a->isnext)
	{
	    a->isnext = 0;
	    a->speed = a->nextspeed;
	    a->growth = 0.0;
	    a->col = a->nextcol;
	    drawblock (mi , cell_x(a), cell_y(a), a->col + sp->count);
	}
    }
    if (sp->redrawing) {
        int i;

        for (i = 0; i < REDRAWSTEP; i++) {
            cell *a = &sp->arr[sp->redrawpos];
            drawblock(mi, cell_x(a), cell_y(a), a->col);
            if (++(sp->redrawpos) >= sp->arr_width * sp->arr_height) {
                sp->redrawing = 0;
                break;
            }
        }
    }
}

void
init_petri(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	petristruct *sp;

   if (MI_IS_VERBOSE(mi))
     {
	(void) printf( "memThrottle : %s\n" , memThrottle );
        (void) printf( "diaglim : %f\n" , st_diaglim );
        (void) printf( "anychan : %f\n" , st_anychan );
        (void) printf( "minorchan : %f\n" , st_minorchan );
        (void) printf( "instantdeathchan : %f\n" , st_instantdeathchan );
        (void) printf( "minlifespan : %d\n" , st_minlifespan );
        (void) printf( "maxlifespan : %d\n" , st_maxlifespan );
        (void) printf( "minlifespeed : %f\n" , st_minlifespeed );
        (void) printf( "maxlifespeed : %f\n" , st_maxlifespeed );
        (void) printf( "mindeathspeed : %f\n" , st_mindeathspeed );
        (void) printf( "maxdeathspeed : %f\n" , st_maxdeathspeed );
     }

/* initialize */
	if (petries == NULL) {
		if ((petries = (petristruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (petristruct))) == NULL)
			return;
	}
	sp = &petries[MI_SCREEN(mi)];
	free_petri(display, sp);
        sp->redrawing = 0;
        if (!setup_display(mi))
	   return;
	if (!setup_arr(mi))
	    return;

    (void) randblip (mi , 1);
}

void
release_petri(ModeInfo * mi)
{
   if (petries != NULL) {
      int screen;

      for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
	free_petri(MI_DISPLAY(mi), &petries[screen]);
      free(petries);
      petries = (petristruct *) NULL;
   }
}

void
refresh_petri(ModeInfo * mi)
{
  petristruct *sp;

  if (petries == NULL)
    return;
  sp = &petries[MI_SCREEN(mi)];

  sp->redrawing = 1;
  sp->redrawpos = 0;
}
#endif /* MODE_petri */
