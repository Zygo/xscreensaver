/* lmorph, Copyright (c) 1993-1999 Sverre H. Huseby and Glenn T. Lines
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/*------------------------------------------------------------------------
 |
 |  FILE            lmorph.c
 |  MODULE OF       xscreensaver
 |
 |  DESCRIPTION     Smooth and non-linear morphing between 1D curves.
 |
 |  WRITTEN BY      Sverre H. Huseby                Glenn T. Lines
 |                  Kurvn. 30                       Østgaardsgt. 5
 |                  N-0495 Oslo                     N-0474 Oslo
 |                  Norway                          Norway
 |
 |                  Phone:  +47 901 63 579          Phone:  +47 22 04 67 28
 |                  E-mail: sverrehu@online.no      E-mail: glennli@ifi.uio.no
 |                  URL:    http://home.sol.no/~sverrehu/
 |
 |                  The original idea, and the bilinear interpolation
 |                  mathematics used, emerged in the head of the wise
 |                  Glenn T. Lines.
 |
 |  MODIFICATIONS   october 1999 (shh)
 |                    * Removed option to use integer arithmetic.
 |                    * Increased default number of points, and brightened
 |                      the foreground color a little bit.
 |                    * Minor code cleanup (very minor, that is).
 |                    * Default number of steps is no longer random.
 |                    * Added -linewidth option (and resource).
 |
 |                  october 1999 (gtl)
 |	              * Added cubic interpolation between shapes 
 |		      * Added non-linear transformation speed  
 |
 |                  june 1998 (shh)
 |                    * Minor code cleanup.
 |
 |                  january 1997 (shh)
 |                    * Some code reformatting.
 |                    * Added possibility to use float arithmetic.
 |                    * Added -figtype option.
 |                    * Made color blue default.
 |
 |                  december 1995 (jwz)
 |                    * Function headers converted from ANSI to K&R.
 |                    * Added posibility for random number of steps, and
 |                      made this the default.
 |
 |                  march 1995 (shh)
 |                    * Converted from an MS-Windows program to X Window.
 |
 |                  november 1993 (gtl, shh, lots of beer)
 |                    * Original Windows version (we didn't know better).
 +----------------------------------------------------------------------*/

#include <math.h>
#include "screenhack.h"

/*-----------------------------------------------------------------------+
|  PRIVATE DATA                                                          |
+-----------------------------------------------------------------------*/

/* define MARGINS to make some space around the figure. */
#define MARGINS

#define MAXFIGS    20
#define TWO_PI     (2.0 * M_PI)
#define RND(x)     (random() % (x))

#define FT_OPEN    1
#define FT_CLOSED  2
#define FT_ALL     (FT_OPEN | FT_CLOSED)

struct state {
  Display *dpy;
  Window window;

   int numFigs;             /* number of figure arrays. */
   int numPoints;           /* number of points in each array. */
   int nWork;               /* current work array number. */
   int nFrom;               /* current from array number. */
   int nTo;                 /* current to array number. */
   int nNext;               /* current next array number (after to).*/
   int shift;               /* shifts the starting point of a figure */
   int figType;

   long delay;              /* usecs to wait between updates. */

   XPoint *aWork[2];        /* working arrays. */
   XPoint *a[MAXFIGS];      /* the figure arrays. */
   XPoint *aTmp;            /* used as source when interrupting morph */
   XPoint *aPrev;           /* previous points displayed. */
   XPoint *aCurr;           /* the current points displayed. */  
   XPoint *aFrom;           /* figure converting from. */
   XPoint *aTo;             /* figure converting to. */
   XPoint *aNext;           /* figure converting to next time. */
   XPoint *aSlopeFrom;      /* slope at start of morph */ 
   XPoint *aSlopeTo;        /* slope at end of morph */ 

   int         scrWidth, scrHeight;
   double      currGamma, maxGamma, deltaGamma;
   GC          gcDraw, gcClear;
};


/*-----------------------------------------------------------------------+
|  PUBLIC DATA                                                           |
+-----------------------------------------------------------------------*/

static const char *lmorph_defaults [] = {
    ".background: black",
    ".foreground: #4444FF",
    "*points: 200",
    "*steps: 150",
    "*delay: 70000",
    "*figtype: all",
    "*linewidth: 5",
    0
};

static XrmOptionDescRec lmorph_options [] = {
    { "-points",      ".points",      XrmoptionSepArg, 0 },
    { "-steps",       ".steps",       XrmoptionSepArg, 0 },
    { "-delay",       ".delay",       XrmoptionSepArg, 0 },
    { "-figtype",     ".figtype",     XrmoptionSepArg, 0 },
    { "-linewidth",   ".linewidth",   XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};

/*-----------------------------------------------------------------------+
|  PRIVATE FUNCTIONS                                                     |
+-----------------------------------------------------------------------*/

static void *
xmalloc(size_t size)
{
    void *ret;

    if ((ret = malloc(size)) == NULL) {
	fprintf(stderr, "lmorph: out of memory\n");
	exit(1);
    }
    return ret;
}

static void
initPointArrays(struct state *st)
{
    int    q, w;
    int    mx, my;            /* max screen coordinates. */
    int    mp;                /* max point number. */
    int    s, rx, ry;
    int    marginx, marginy;
    double scalex, scaley;

    mx = st->scrWidth - 1;
    my = st->scrHeight - 1;
    mp = st->numPoints - 1;

    st->aWork[0] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
    st->aWork[1] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
    st->aTmp     = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));

    if (st->figType & FT_CLOSED) {
	/* rectangle */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	s = st->numPoints / 4;
	for (q = 0; q < s; q++) {
	    st->a[st->numFigs][q].x = ((double) q / s) * mx;
	    st->a[st->numFigs][q].y = 0;
	    st->a[st->numFigs][s + q].x = mx;
	    st->a[st->numFigs][s + q].y = ((double) q / s) * my;
	    st->a[st->numFigs][2 * s + q].x = mx - ((double) q / s) * mx;
	    st->a[st->numFigs][2 * s + q].y = my;
	    st->a[st->numFigs][3 * s + q].x = 0;
	    st->a[st->numFigs][3 * s + q].y = my - ((double) q / s) * my;
	}
	for (q = 4 * s; q < st->numPoints; q++) 
	    st->a[st->numFigs][q].x = st->a[st->numFigs][q].y = 0;
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;

	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + rx * sin(1 * TWO_PI * (double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry * cos(3 * TWO_PI * (double) q / mp);
	}
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;

	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + ry * sin(3 * TWO_PI * (double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry * cos(1 * TWO_PI * (double) q / mp);
	}
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;

	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + ry 
		* (0.8 - 0.2 * sin(30 * TWO_PI * q / mp))
		* sin(TWO_PI * (double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry
		* (0.8 - 0.2 * sin(30 * TWO_PI * q / mp))
		* cos(TWO_PI * (double) q / mp);
	}
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;


	/* */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + ry * sin(TWO_PI * (double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry * cos(TWO_PI * (double) q / mp);
	}
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;


	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + rx * cos(TWO_PI * (double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry * sin(TWO_PI * (double) q / mp);
	}
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;

	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + rx * sin(2 * TWO_PI * (double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry * cos(3 * TWO_PI * (double) q / mp);
	}
	st->a[st->numFigs][mp].x = st->a[st->numFigs][0].x;
	st->a[st->numFigs][mp].y = st->a[st->numFigs][0].y;
	++st->numFigs;
    }

    if (st->figType & FT_OPEN) {
	/* sine wave, one period */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = ((double) q / st->numPoints) * mx;
	    st->a[st->numFigs][q].y = (1.0 - sin(((double) q / mp) * TWO_PI))
		* my / 2.0;
	}
	++st->numFigs;

	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = ((double) q / mp) * mx;
	    st->a[st->numFigs][q].y = (1.0 - cos(((double) q / mp) * 3 * TWO_PI))
		* my / 2.0;
	}
	++st->numFigs;

	/* spiral, one endpoint at bottom */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + ry * sin(5 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 + ry * cos(5 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	}
	++st->numFigs;

	/* spiral, one endpoint at top */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = mx / 2 + ry * sin(6 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	    st->a[st->numFigs][q].y = my / 2 - ry * cos(6 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	}
	++st->numFigs;

	/*  */
	st->a[st->numFigs] = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint));
	for (q = 0; q < st->numPoints; q++) {
	    st->a[st->numFigs][q].x = ((double) q / mp) * mx;
	    st->a[st->numFigs][q].y = (1.0 - sin(((double) q / mp) * 5 * TWO_PI))
		* my / 2.0;
	}
	++st->numFigs;
    }

#ifdef MARGINS
    /* make some space around the figures.  */
    marginx = (mx + 1) / 10;
    marginy = (my + 1) / 10;
    scalex = (double) ((mx + 1) - 2.0 * marginx) / (mx + 1.0);
    scaley = (double) ((my + 1) - 2.0 * marginy) / (my + 1.0);
    for (q = 0; q < st->numFigs; q++)
	for (w = 0; w < st->numPoints; w++) {
 	    st->a[q][w].x = marginx + st->a[q][w].x * scalex;
 	    st->a[q][w].y = marginy + st->a[q][w].y * scaley;
	}
#endif
}

static void
initLMorph(struct state *st)
{
    int               steps;
    XGCValues         gcv;
    XWindowAttributes wa;
    Colormap          cmap;
    char              *ft;
    int               i;

    st->maxGamma = 1.0;
    st->numPoints = get_integer_resource(st->dpy, "points", "Integer");
    steps = get_integer_resource(st->dpy, "steps", "Integer");
    st->delay = get_integer_resource(st->dpy, "delay", "Integer");
    ft = get_string_resource(st->dpy, "figtype", "String");

    if (strcmp(ft, "all") == 0)
	st->figType = FT_ALL;
    else if (strcmp(ft, "open") == 0)
	st->figType = FT_OPEN;
    else if (strcmp(ft, "closed") == 0)
	st->figType = FT_CLOSED;
    else {
	fprintf(stderr, "figtype should be `all', `open' or `closed'.\n");
	st->figType = FT_ALL;
    }
    if (ft) free (ft);

    if (steps <= 0)
      steps = (random() % 400) + 100;

    st->deltaGamma = 1.0 / steps;
    XGetWindowAttributes(st->dpy, st->window, &wa);
    st->scrWidth = wa.width;
    st->scrHeight = wa.height;
    cmap = wa.colormap;
    gcv.foreground = get_pixel_resource(st->dpy, cmap, "foreground", "Foreground");
    st->gcDraw = XCreateGC(st->dpy, st->window, GCForeground, &gcv);
    XSetForeground(st->dpy, st->gcDraw, gcv.foreground);
    gcv.foreground = get_pixel_resource(st->dpy, cmap, "background", "Background");
    st->gcClear = XCreateGC(st->dpy, st->window, GCForeground, &gcv);
    XClearWindow(st->dpy, st->window);

    initPointArrays(st);
    st->aCurr = st->aWork[st->nWork = 0];
    st->aPrev = NULL;
    st->currGamma = st->maxGamma + 1.0;  /* force creation of new figure at startup */
    st->nTo = RND(st->numFigs);
    do {
        st->nNext = RND(st->numFigs);
    } while (st->nNext == st->nTo);

    st->aSlopeTo = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint)); 
    st->aSlopeFrom = (XPoint *) xmalloc(st->numPoints * sizeof(XPoint)); 
    st->aNext = 0;

    for (i = 0; i < st->numPoints ; i++) {
        st->aSlopeTo[i].x = 0.0;
        st->aSlopeTo[i].y = 0.0; 
    }

    {   /* jwz for version 2.11 */
        /*      int width = random() % 10;*/
        int width = get_integer_resource(st->dpy, "linewidth", "Integer");
        int style = LineSolid;
        int cap   = (width > 1 ? CapRound  : CapButt);
        int join  = (width > 1 ? JoinRound : JoinBevel);
        if (width == 1)
            width = 0;
        XSetLineAttributes(st->dpy, st->gcDraw,  width, style, cap, join);
        XSetLineAttributes(st->dpy, st->gcClear, width, style, cap, join);
    }
}

/* 55% of execution time */
static void
createPoints(struct state *st)
{
    int    q;
    XPoint *pa = st->aCurr, *pa1 = st->aFrom, *pa2 = st->aTo;
    XPoint *qa1 = st->aSlopeFrom, *qa2 = st->aSlopeTo; 
    float  fg, f1g;
    float  speed;

    fg  = st->currGamma;
    f1g = 1.0 - st->currGamma;
    for (q = st->numPoints; q; q--) {
        speed = 0.45 * sin(TWO_PI * (double) (q + st->shift) / (st->numPoints - 1));
        fg = st->currGamma + 1.67 * speed
            * exp(-200.0 * (st->currGamma - 0.5 + 0.7 * speed)
                  * (st->currGamma - 0.5 + 0.7 * speed));

        f1g = 1.0 - fg;
        pa->x = (short) (f1g * f1g * f1g * pa1->x + f1g * f1g * fg
                         * (3 * pa1->x + qa1->x) + f1g * fg * fg
                         * (3 * pa2->x - qa2->x) + fg * fg * fg * pa2->x);
        pa->y = (short) (f1g * f1g * f1g * pa1->y + f1g * f1g * fg
                         * (3 * pa1->y + qa1->y) + f1g * fg * fg
                         * (3 * pa2->y - qa2->y) + fg * fg * fg * pa2->y);

        ++pa;
        ++pa1;
        ++pa2;
	++qa1;
	++qa2;
    }
}

/* 36% of execution time */
static void
drawImage(struct state *st)
{
#if 0
    int    q;
    XPoint *old0, *old1, *new0, *new1;

    /* Problem: update the window without too much flickering. I do
     * this by handling each linesegment separately. First remove a
     * line, then draw the new line. The problem is that this leaves
     * small black pixels on the figure. To fix this, we draw the
     * entire figure using XDrawLines() afterwards. */
    if (st->aPrev) {
	old0 = st->aPrev;
	old1 = st->aPrev + 1;
	new0 = st->aCurr;
	new1 = st->aCurr + 1;
	for (q = st->numPoints - 1; q; q--) {
	   XDrawLine(st->dpy, st->window, st->gcClear,
	     old0->x, old0->y, old1->x, old1->y); 
	    XDrawLine(st->dpy, st->window, st->gcDraw,
		      new0->x, new0->y, new1->x, new1->y);
	    ++old0;
	    ++old1;
	    ++new0;
	    ++new1;
	}
    }
#else
    XClearWindow(st->dpy,st->window);
#endif
    XDrawLines(st->dpy, st->window, st->gcDraw, st->aCurr, st->numPoints, CoordModeOrigin);
}

/* neglectible % of execution time */
static void
animateLMorph(struct state *st)
{
    int i;
    if (st->currGamma > st->maxGamma) {
        st->currGamma = 0.0;
        st->nFrom = st->nTo;
        st->nTo = st->nNext;
        st->aFrom = st->a[st->nFrom];
	st->aTo = st->a[st->nTo];
        do {
            st->nNext = RND(st->numFigs);
        } while (st->nNext == st->nTo);
	st->aNext = st->a[st->nNext];

	st->shift = RND(st->numPoints);
        if (RND(2)) {
            /* reverse the array to get more variation. */
            int    i1, i2;
            XPoint p;
            
            for (i1 = 0, i2 = st->numPoints - 1; i1 < st->numPoints / 2; i1++, i2--) {
                p = st->aNext[i1];
                st->aNext[i1] = st->aNext[i2];
                st->aNext[i2] = p;
            }
        }

	/* calculate the slopes */
	for (i = 0; i < st->numPoints ; i++) {
            st->aSlopeFrom[i].x = st->aSlopeTo[i].x;
            st->aSlopeFrom[i].y = st->aSlopeTo[i].y;
            st->aSlopeTo[i].x = st->aNext[i].x - st->aTo[i].x;
            st->aSlopeTo[i].y = (st->aNext[i].y - st->aTo[i].y);
	}
    }

    createPoints(st);
    drawImage(st);
    st->aPrev = st->aCurr;
    st->aCurr = st->aWork[st->nWork ^= 1];

    st->currGamma += st->deltaGamma;
}

/*-----------------------------------------------------------------------+
|  PUBLIC FUNCTIONS                                                      |
+-----------------------------------------------------------------------*/

static void *
lmorph_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = d;
  st->window = w;
  initLMorph(st);
  return st;
}

static unsigned long
lmorph_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  animateLMorph(st);
  return st->delay;
}

static void
lmorph_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
lmorph_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
lmorph_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  XFreeGC (dpy, st->gcDraw);
  XFreeGC (dpy, st->gcClear);
  free (st->aWork[0]);
  free (st->aWork[1]);
  free (st->aTmp);
  free (st->aSlopeTo);
  free (st->aSlopeFrom);
  for (i = 0; i < MAXFIGS; i++)
    if (st->a[i]) free (st->a[i]);
  free (st);
}

XSCREENSAVER_MODULE ("LMorph", lmorph)
