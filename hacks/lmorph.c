
/**************************************************************************
 *
 *  FILE            lmorph.c
 *  MODULE OF       xscreensaver
 *
 *  DESCRIPTION     Bilinear interpolation for morphing line shapes.
 *
 *  WRITTEN BY      Sverre H. Huseby                Glenn T. Lines
 *                  Kurvn. 30                       Østgaardsgt. 5
 *                  N-0495 Oslo                     N-0474 Oslo
 *                  Norway                          Norway
 *
 *                  Phone:  +47 901 63 579          Phone:  +47 22 04 67 28
 *                  E-mail: sverrehu@online.no      E-mail: gtl@si.sintef.no
 *                  URL:    http://home.sol.no/~sverrehu/
 *
 *                  The original idea, and the bilinear interpolation
 *                  mathematics used, emerged in the head of the wise
 *                  Glenn T. Lines.
 *
 *  MODIFICATIONS   june 1998 (shh)
 *                    * Minor code cleanup.
 *
 *                  march 1997 (shh)
 *                    * Added -mailfile option to allow checking for
 *                      new mail while the screensaver is active.
 *
 *                  january 1997 (shh)
 *                    * Some code reformatting.
 *                    * Added possibility to use float arithmetic.
 *                    * Added -figtype option.
 *                    * Made color blue default.
 *
 *                  december 1995 (jwz)
 *                    * Function headers converted from ANSI to K&R.
 *                    * Added posibility for random number of steps, and
 *                      made this the default.
 *
 *                  march 1995 (shh)
 *                    * Converted from an MS-Windows program to X Window.
 *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "screenhack.h"

/**************************************************************************
 *                                                                        *
 *                       P R I V A T E    D A T A                         *
 *                                                                        *
 **************************************************************************/

/* define MARGINS to make some space around the figure. */
#define MARGINS

/* define USE_FLOAT to avoid using integer calculations in
   createPoints. integer calculation is supposed to be faster, but it
   won't work for displays larger than 2048x2048 or so pixels. */
#undef USE_FLOAT

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
    figType;
static long delay;              /* usecs to wait between updates. */
static XPoint
    *aWork[2],                  /* working arrays. */
    *a[MAXFIGS],                /* the figure arrays. */
    *aTmp,                      /* used as source when interrupting morph */
    *aPrev,                     /* previous points displayed. */
    *aCurr,                     /* the current points displayed. */  
    *aFrom,                     /* figure converting from. */
    *aTo;                       /* figure converting to. */
static int         scrWidth, scrHeight;
static double      currGamma, maxGamma = 1.0, deltaGamma;
static GC          gcDraw, gcClear;
static Display     *dpy;
static Window      window;



/**************************************************************************
 *                                                                        *
 *                        P U B L I C    D A T A                          *
 *                                                                        *
 **************************************************************************/

char *progclass = "LMorph";

char *defaults [] = {
    ".background: black",
    ".foreground: blue",
    "*points: 150",
    "*steps: 0",
    "*delay: 50000",
    "*figtype: all",
    0
};

XrmOptionDescRec options [] = {
  { "-points",      ".points",      XrmoptionSepArg, 0 },
  { "-steps",       ".steps",       XrmoptionSepArg, 0 },
  { "-delay",       ".delay",       XrmoptionSepArg, 0 },
  { "-figtype",     ".figtype",     XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));



/**************************************************************************
 *                                                                        *
 *                   P R I V A T E    F U N C T I O N S                   *
 *                                                                        *
 **************************************************************************/

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
    int q, w,
        mx, my,                 /* max screen coordinates. */
        mp,                     /* max point number. */
        s, rx, ry,
        marginx, marginy;
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

	/*  */
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

    { /* jwz for version 2.11 */
      int width = random() % 10;
      int style = LineSolid;
      int cap   = (width > 1 ? CapRound  : CapButt);
      int join  = (width > 1 ? JoinRound : JoinBevel);
      if (width == 1) width = 0;
      XSetLineAttributes(dpy, gcDraw,  width, style, cap, join);
      XSetLineAttributes(dpy, gcClear, width, style, cap, join);
    }
}

/* 55% of execution time */
static void
createPoints(void)
{
    int             q;
    XPoint *pa = aCurr, *pa1 = aFrom, *pa2 = aTo;
#ifdef USE_FLOAT
    float           fg, f1g;
#else
    long            lg, l1g;
#endif

#ifdef USE_FLOAT
    fg  = currGamma;
    f1g = 1.0 - currGamma;
#else
    lg  = 8192L * currGamma;
    l1g = 8192L * (1.0 - currGamma);
#endif
    for (q = numPoints; q; q--) {
#ifdef USE_FLOAT
        pa->x = (short) (f1g * pa1->x + fg * pa2->x);
        pa->y = (short) (f1g * pa1->y + fg * pa2->y);
#else
        pa->x = (short) ((l1g * pa1->x + lg * pa2->x) / 8192L);
        pa->y = (short) ((l1g * pa1->y + lg * pa2->y) / 8192L);
#endif
        ++pa;
        ++pa1;
        ++pa2;
    }
}

/* 36% of execution time */
static void
drawImage(void)
{
    int             q;
    XPoint *old0, *old1, *new0, *new1;

    /* Problem: update the window without too much flickering. I do
     * this by handling each linesegment separately. First remove a
     * line, then draw the new line. The problem is that this leaves
     * small black pixels on the figure. To fix this, I draw the
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
    if (currGamma > maxGamma) {
        currGamma = 0.0;
        if (maxGamma == 1.0) {
            nFrom = nTo;
            aFrom = a[nFrom];
        } else {
            memcpy(aTmp, aCurr, numPoints * sizeof(XPoint));
            aFrom = aTmp;
            nFrom = -1;
        }
        do {
            nTo = RND(numFigs);
        } while (nTo == nFrom);
        aTo = a[nTo];
        if (RND(2)) {
            /* reverse the array to get more variation. */
            int    i1, i2;
            XPoint p;
            
            for (i1 = 0, i2 = numPoints - 1; i1 < numPoints / 2; i1++, i2--) {
                p = aTo[i1];
                aTo[i1] = aTo[i2];
                aTo[i2] = p;
            }
        }
        /* occationally interrupt the next run. */
        if (RND(4) == 0)
            maxGamma = 0.1 + 0.7 * (RND(1001) / 1000.0); /* partial run */
        else
            maxGamma = 1.0;                              /* full run */
    }

    createPoints();
    drawImage();
    aPrev = aCurr;
    aCurr = aWork[nWork ^= 1];

    currGamma += deltaGamma;
}



/**************************************************************************
 *                                                                        *
 *                    P U B L I C    F U N C T I O N S                    *
 *                                                                        *
 **************************************************************************/

void
screenhack(Display *disp, Window win)
{
    dpy = disp;
    window = win;
    initLMorph();
    for (;;) {
	animateLMorph();
	screenhack_usleep(delay);
    }
}
