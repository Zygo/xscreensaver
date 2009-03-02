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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int
    numFigs = 0,                /* number of figure arrays. */
    numPoints,                  /* number of points in each array. */
    nWork,                      /* current work array number. */
    nFrom,                      /* current from array number. */
    nTo,                        /* current to array number. */
    nNext,                      /* current next array number (after to).*/
    shift,                      /* shifts the starting point of a figure */
    figType;
static long delay;              /* usecs to wait between updates. */
static XPoint
    *aWork[2],                  /* working arrays. */
    *a[MAXFIGS],                /* the figure arrays. */
    *aTmp,                      /* used as source when interrupting morph */
    *aPrev,                     /* previous points displayed. */
    *aCurr,                     /* the current points displayed. */  
    *aFrom,                     /* figure converting from. */
    *aTo,                       /* figure converting to. */
    *aNext,                     /* figure converting to next time. */
    *aSlopeFrom,                /* slope at start of morph */ 
    *aSlopeTo;                  /* slope at end of morph */ 
static int         scrWidth, scrHeight;
static double      currGamma, maxGamma = 1.0, deltaGamma;
static GC          gcDraw, gcClear;
static Display     *dpy;
static Window      window;

/*-----------------------------------------------------------------------+
|  PUBLIC DATA                                                           |
+-----------------------------------------------------------------------*/

char *progclass = "LMorph";

char *defaults [] = {
    ".background: black",
    ".foreground: #4444FF",
    "*points: 200",
    "*steps: 150",
    "*delay: 70000",
    "*figtype: all",
    "*linewidth: 5",
    0
};

XrmOptionDescRec options [] = {
    { "-points",      ".points",      XrmoptionSepArg, 0 },
    { "-steps",       ".steps",       XrmoptionSepArg, 0 },
    { "-delay",       ".delay",       XrmoptionSepArg, 0 },
    { "-figtype",     ".figtype",     XrmoptionSepArg, 0 },
    { "-linewidth",   ".linewidth",   XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

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
initPointArrays(void)
{
    int    q, w;
    int    mx, my;            /* max screen coordinates. */
    int    mp;                /* max point number. */
    int    s, rx, ry;
    int    marginx, marginy;
    double scalex, scaley;

    mx = scrWidth - 1;
    my = scrHeight - 1;
    mp = numPoints - 1;

    aWork[0] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
    aWork[1] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
    aTmp     = (XPoint *) xmalloc(numPoints * sizeof(XPoint));

    if (figType & FT_CLOSED) {
	/* rectangle */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	s = numPoints / 4;
	for (q = 0; q < s; q++) {
	    a[numFigs][q].x = ((double) q / s) * mx;
	    a[numFigs][q].y = 0;
	    a[numFigs][s + q].x = mx;
	    a[numFigs][s + q].y = ((double) q / s) * my;
	    a[numFigs][2 * s + q].x = mx - ((double) q / s) * mx;
	    a[numFigs][2 * s + q].y = my;
	    a[numFigs][3 * s + q].x = 0;
	    a[numFigs][3 * s + q].y = my - ((double) q / s) * my;
	}
	for (q = 4 * s; q < numPoints; q++) 
	    a[numFigs][q].x = a[numFigs][q].y = 0;
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;

	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + rx * sin(1 * TWO_PI * (double) q / mp);
	    a[numFigs][q].y = my / 2 + ry * cos(3 * TWO_PI * (double) q / mp);
	}
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;

	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + ry * sin(3 * TWO_PI * (double) q / mp);
	    a[numFigs][q].y = my / 2 + ry * cos(1 * TWO_PI * (double) q / mp);
	}
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;

	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + ry 
		* (0.8 - 0.2 * sin(30 * TWO_PI * q / mp))
		* sin(TWO_PI * (double) q / mp);
	    a[numFigs][q].y = my / 2 + ry
		* (0.8 - 0.2 * sin(30 * TWO_PI * q / mp))
		* cos(TWO_PI * (double) q / mp);
	}
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;


	/* */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + ry * sin(TWO_PI * (double) q / mp);
	    a[numFigs][q].y = my / 2 + ry * cos(TWO_PI * (double) q / mp);
	}
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;


	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + rx * cos(TWO_PI * (double) q / mp);
	    a[numFigs][q].y = my / 2 + ry * sin(TWO_PI * (double) q / mp);
	}
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;

	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + rx * sin(2 * TWO_PI * (double) q / mp);
	    a[numFigs][q].y = my / 2 + ry * cos(3 * TWO_PI * (double) q / mp);
	}
	a[numFigs][mp].x = a[numFigs][0].x;
	a[numFigs][mp].y = a[numFigs][0].y;
	++numFigs;
    }

    if (figType & FT_OPEN) {
	/* sine wave, one period */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = ((double) q / numPoints) * mx;
	    a[numFigs][q].y = (1.0 - sin(((double) q / mp) * TWO_PI))
		* my / 2.0;
	}
	++numFigs;

	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = ((double) q / mp) * mx;
	    a[numFigs][q].y = (1.0 - cos(((double) q / mp) * 3 * TWO_PI))
		* my / 2.0;
	}
	++numFigs;

	/* spiral, one endpoint at bottom */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + ry * sin(5 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	    a[numFigs][q].y = my / 2 + ry * cos(5 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	}
	++numFigs;

	/* spiral, one endpoint at top */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	rx = mx / 2;
	ry = my / 2;
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = mx / 2 + ry * sin(6 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	    a[numFigs][q].y = my / 2 - ry * cos(6 * TWO_PI * (double) q / mp)
		* ((double) q / mp);
	}
	++numFigs;

	/*  */
	a[numFigs] = (XPoint *) xmalloc(numPoints * sizeof(XPoint));
	for (q = 0; q < numPoints; q++) {
	    a[numFigs][q].x = ((double) q / mp) * mx;
	    a[numFigs][q].y = (1.0 - sin(((double) q / mp) * 5 * TWO_PI))
		* my / 2.0;
	}
	++numFigs;
    }

#ifdef MARGINS
    /* make some space around the figures.  */
    marginx = (mx + 1) / 10;
    marginy = (my + 1) / 10;
    scalex = (double) ((mx + 1) - 2.0 * marginx) / (mx + 1.0);
    scaley = (double) ((my + 1) - 2.0 * marginy) / (my + 1.0);
    for (q = 0; q < numFigs; q++)
	for (w = 0; w < numPoints; w++) {
 	    a[q][w].x = marginx + a[q][w].x * scalex;
 	    a[q][w].y = marginy + a[q][w].y * scaley;
	}
#endif
}

static void
initLMorph(void)
{
    int               steps;
    XGCValues         gcv;
    XWindowAttributes wa;
    Colormap          cmap;
    char              *ft;
    int               i;

    numPoints = get_integer_resource("points", "Integer");
    steps = get_integer_resource("steps", "Integer");
    delay = get_integer_resource("delay", "Integer");
    ft = get_string_resource("figtype", "String");

    if (strcmp(ft, "all") == 0)
	figType = FT_ALL;
    else if (strcmp(ft, "open") == 0)
	figType = FT_OPEN;
    else if (strcmp(ft, "closed") == 0)
	figType = FT_CLOSED;
    else {
	fprintf(stderr, "figtype should be `all', `open' or `closed'.\n");
	figType = FT_ALL;
    }

    if (steps <= 0)
      steps = (random() % 400) + 100;

    deltaGamma = 1.0 / steps;
    XGetWindowAttributes(dpy, window, &wa);
    scrWidth = wa.width;
    scrHeight = wa.height;
    cmap = wa.colormap;
    gcv.foreground = get_pixel_resource("foreground", "Foreground", dpy, cmap);
    gcDraw = XCreateGC(dpy, window, GCForeground, &gcv);
    XSetForeground(dpy, gcDraw, gcv.foreground);
    gcv.foreground = get_pixel_resource("background", "Background", dpy, cmap);
    gcClear = XCreateGC(dpy, window, GCForeground, &gcv);
    XClearWindow(dpy, window);

    srandom(time(NULL));
    initPointArrays();
    aCurr = aWork[nWork = 0];
    aPrev = NULL;
    currGamma = maxGamma + 1.0;  /* force creation of new figure at startup */
    nTo = RND(numFigs);
    do {
        nNext = RND(numFigs);
    } while (nNext == nTo);

    aSlopeTo = (XPoint *) xmalloc(numPoints * sizeof(XPoint)); 
    aSlopeFrom = (XPoint *) xmalloc(numPoints * sizeof(XPoint)); 
    aNext = (XPoint *) xmalloc(numPoints * sizeof(XPoint)); 

    for (i = 0; i < numPoints ; i++) {
        aSlopeTo[i].x = 0.0;
        aSlopeTo[i].y = 0.0; 
    }

    {   /* jwz for version 2.11 */
        /*      int width = random() % 10;*/
        int width = get_integer_resource("linewidth", "Integer");
        int style = LineSolid;
        int cap   = (width > 1 ? CapRound  : CapButt);
        int join  = (width > 1 ? JoinRound : JoinBevel);
        if (width == 1)
            width = 0;
        XSetLineAttributes(dpy, gcDraw,  width, style, cap, join);
        XSetLineAttributes(dpy, gcClear, width, style, cap, join);
    }
}

/* 55% of execution time */
static void
createPoints(void)
{
    int    q;
    XPoint *pa = aCurr, *pa1 = aFrom, *pa2 = aTo;
    XPoint *qa1 = aSlopeFrom, *qa2 = aSlopeTo; 
    float  fg, f1g;
    float  speed;

    fg  = currGamma;
    f1g = 1.0 - currGamma;
    for (q = numPoints; q; q--) {
        speed = 0.45 * sin(TWO_PI * (double) (q + shift) / (numPoints - 1));
        fg = currGamma + 1.67 * speed
            * exp(-200.0 * (currGamma - 0.5 + 0.7 * speed)
                  * (currGamma - 0.5 + 0.7 * speed));

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
drawImage(void)
{
    int    q;
    XPoint *old0, *old1, *new0, *new1;

    /* Problem: update the window without too much flickering. I do
     * this by handling each linesegment separately. First remove a
     * line, then draw the new line. The problem is that this leaves
     * small black pixels on the figure. To fix this, we draw the
     * entire figure using XDrawLines() afterwards. */
    if (aPrev) {
	old0 = aPrev;
	old1 = aPrev + 1;
	new0 = aCurr;
	new1 = aCurr + 1;
	for (q = numPoints - 1; q; q--) {
	   XDrawLine(dpy, window, gcClear,
	     old0->x, old0->y, old1->x, old1->y); 
	    XDrawLine(dpy, window, gcDraw,
		      new0->x, new0->y, new1->x, new1->y);
	    ++old0;
	    ++old1;
	    ++new0;
	    ++new1;
	}
    }
    XDrawLines(dpy, window, gcDraw, aCurr, numPoints, CoordModeOrigin);

    XFlush(dpy);
}

/* neglectible % of execution time */
static void
animateLMorph(void)
{
    int i;
    if (currGamma > maxGamma) {
        currGamma = 0.0;
        nFrom = nTo;
        nTo = nNext;
        aFrom = a[nFrom];
	aTo = a[nTo];
        do {
            nNext = RND(numFigs);
        } while (nNext == nTo);
	aNext = a[nNext];

	shift = RND(numPoints);
        if (RND(2)) {
            /* reverse the array to get more variation. */
            int    i1, i2;
            XPoint p;
            
            for (i1 = 0, i2 = numPoints - 1; i1 < numPoints / 2; i1++, i2--) {
                p = aNext[i1];
                aNext[i1] = aNext[i2];
                aNext[i2] = p;
            }
        }

	/* calculate the slopes */
	for (i = 0; i < numPoints ; i++) {
            aSlopeFrom[i].x = aSlopeTo[i].x;
            aSlopeFrom[i].y = aSlopeTo[i].y;
            aSlopeTo[i].x = aNext[i].x - aTo[i].x;
            aSlopeTo[i].y = (aNext[i].y - aTo[i].y);
	}
    }

    createPoints();
    drawImage();
    aPrev = aCurr;
    aCurr = aWork[nWork ^= 1];

    currGamma += deltaGamma;
}

/*-----------------------------------------------------------------------+
|  PUBLIC FUNCTIONS                                                      |
+-----------------------------------------------------------------------*/

void
screenhack(Display *disp, Window win)
{
    dpy = disp;
    window = win;
    initLMorph();
    for (;;) {
      	animateLMorph();
        screenhack_handle_events (dpy);
	usleep(delay);
	}

}
