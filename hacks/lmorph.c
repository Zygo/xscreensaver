
/**************************************************************************
 *
 *  FILE            lmorph.c
 *  MODULE OF       xscreensaver
 *
 *  DESCRIPTION     Bilinear interpolation for morphing line shapes.
 *
 *  WRITTEN BY      Sverre H. Huseby                Glenn T. Lines
 *                  Maridalsvn. 122, leil 101       Frysjavn. 3, 5. etg.
 *                  N-0461 Oslo                     N-0883 Oslo
 *                  Norway                          Norway
 *
 *                  Phone:  +47 22 71 99 08         Phone:  +47 22 23 71 99
 *                  E-mail: sverrehu@ifi.uio.no     E-mail: gtl@si.sintef.no
 *
 *                  The original idea, and the bilinear interpolation
 *                  mathematics used, emerged in the head of the wise
 *                  Glenn Terje Lines.
 *
 *  MODIFICATIONS   march 1995
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

/* Define MARGINS to make some space around the figure */
#define MARGINS /**/

#define MAXFIGS    20
#define TWO_PI     (2.0 * M_PI)
#define RND(x)     (random() % (x))
static int
    cFig = 0,                   /* Number of figure arrays. */
    cPoint,                     /* Number of points in each array. */
    nWork,                      /* Current work array number. */
    nFrom,                      /* Current from array number. */
    nTo;                        /* Current to array number. */
static long
    delay;                      /* usecs to wait between updates. */
static XPoint
    *aWork[2],                  /* Working arrays. */
    *a[MAXFIGS],                /* The figure arrays. */
    *aTmp,                      /* Used as source when interrupting morph */
    *aPrev,                     /* Previous points displayed. */
    *aCurr,                     /* The current points displayed. */  
    *aFrom,                     /* Figure converting from. */
    *aTo;                       /* Figure converting to. */
static double
    gam,
    maxGamma = 1.0,
    delta_gam;
static GC
    gcDraw, gcClear;
static Display
    *dpy;
static Window
    window;



/**************************************************************************
 *                                                                        *
 *                        P U B L I C    D A T A                          *
 *                                                                        *
 **************************************************************************/

char *progclass = "LMorph";

char *defaults [] = {
    "LMorph.background: black",
    "LMorph.foreground: green",
    "*points: 150",
    "*steps: 0",
    "*delay: 50000",
    0
};

XrmOptionDescRec options [] = {
  { "-points", ".points", XrmoptionSepArg, 0 },
  { "-steps",  ".steps",  XrmoptionSepArg, 0 },
  { "-delay",  ".delay",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


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



static double frnd (void)
{
    /*
     *  Hm. for some reason the second line (using RAND_MAX) didn't
     *  work on some machines, so I always use the first.
     */
#ifndef dont_use_RAND_MAX
    return (double) (random() & 0x7FFF) / 0x7FFF;
#else  /* RAND_MAX */
    return ((double) random()) / RAND_MAX;
#endif /* RAND_MAX */
}



static void initPointArrays (void)
{
    XWindowAttributes wa;
    int q, w,
        mx, my,                 /* Max screen coordinates. */
        mp,                     /* Max point number. */
        s, rx, ry,
        marginx, marginy;
    double scalex, scaley;

    XGetWindowAttributes(dpy, window, &wa);
    mx = wa.width - 1;
    my = wa.height - 1;
    mp = cPoint - 1;

    aWork[0] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    aWork[1] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    aTmp     = (XPoint *) xmalloc(cPoint * sizeof(XPoint));


    /*
     *  Figure 0
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    s = cPoint / 4;
    for (q = 0; q < s; q++) {
        a[cFig][q].x = ((double) q / s) * mx;
        a[cFig][q].y = 0;
        a[cFig][s + q].x = mx;
        a[cFig][s + q].y = ((double) q / s) * my;
        a[cFig][2 * s + q].x = mx - ((double) q / s) * mx;
        a[cFig][2 * s + q].y = my;
        a[cFig][3 * s + q].x = 0;
        a[cFig][3 * s + q].y = my - ((double) q / s) * my;
    }
    for (q = 4 * s; q < cPoint; q++) 
        a[cFig][q].x = a[cFig][q].y = 0;
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 1
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = ((double) q / cPoint) * mx;
        a[cFig][q].y = (1.0 - sin(((double) q / mp) * TWO_PI)) * my / 2.0;
    }
    ++cFig;

    /*
     *  Figure 2
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + rx * sin(1 * TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * cos(3 * TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 3
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + ry * sin(3 * TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * cos(1 * TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 4
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + ry * (1 - 0.1 * frnd())
	    * sin(TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * (1 - 0.1 * frnd())
	    * cos(TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 5
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + ry * (0.8 - 0.2 * sin(30 * TWO_PI * q / mp))
	    * sin(TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * (0.8 - 0.2 * sin(30 * TWO_PI * q / mp))
	    * cos(TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 6
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + ry * sin(TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * cos(TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 7
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + rx * cos(TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * sin(TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 8
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = ((double) q / mp) * mx;
        a[cFig][q].y = (1.0 - cos(((double) q / mp) * 3 * TWO_PI)) * my / 2.0;
    }
    ++cFig;

    /*
     *  Figure 9
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + rx * sin(2 * TWO_PI * (double) q / mp);
        a[cFig][q].y = my / 2 + ry * cos(3 * TWO_PI * (double) q / mp);
    }
    a[cFig][mp].x = a[cFig][0].x;
    a[cFig][mp].y = a[cFig][0].y;
    ++cFig;

    /*
     *  Figure 10
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + ry * sin(5 * TWO_PI * (double) q / mp)
	    * ((double) q / mp);
        a[cFig][q].y = my / 2 + ry * cos(5 * TWO_PI * (double) q / mp)
	    * ((double) q / mp);
    }
    ++cFig;

    /*
     *  Figure 11
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    rx = mx / 2;
    ry = my / 2;
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = mx / 2 + ry * sin(6 * TWO_PI * (double) q / mp)
	    * ((double) q / mp);
        a[cFig][q].y = my / 2 - ry * cos(6 * TWO_PI * (double) q / mp)
	    * ((double) q / mp);
    }
    ++cFig;

    /*
     *  Figure 12
     */
    a[cFig] = (XPoint *) xmalloc(cPoint * sizeof(XPoint));
    for (q = 0; q < cPoint; q++) {
        a[cFig][q].x = ((double) q / mp) * mx;
        a[cFig][q].y = (1.0 - sin(((double) q / mp) * 5 * TWO_PI)) * my / 2.0;
    }
    ++cFig;

#ifdef MARGINS
    /*
     *  Make some space around the figures.
     */
    marginx = (mx + 1) / 10;
    marginy = (my + 1) / 10;
    scalex = (double) ((mx + 1) - 2.0 * marginx) / (mx + 1.0);
    scaley = (double) ((my + 1) - 2.0 * marginy) / (my + 1.0);
    for (q = 0; q < cFig; q++)
	for (w = 0; w < cPoint; w++) {
 	    a[q][w].x = marginx + a[q][w].x * scalex;
 	    a[q][w].y = marginy + a[q][w].y * scaley;
	}
#endif
}



static void createPoints (void)
{
    int    q;
    XPoint *pa = aCurr, *pa1 = aFrom, *pa2 = aTo;
    long   lg, l1g;


    lg = 8192L * gam, l1g = 8192L * (1.0 - gam);
    for (q = 0; q < cPoint; q++) {
        pa->x = (short) ((l1g * pa1->x + lg * pa2->x) / 8192L);
        pa->y = (short) ((l1g * pa1->y + lg * pa2->y) / 8192L);
        ++pa;
        ++pa1;
        ++pa2;
    }
}


static void drawImage (void)
{
    register int q;
    XPoint *old0, *old1, *new0, *new1;

    /*
     *  Problem: update the window without too much flickering. I do
     *  this by handling each linesegment separately. First remove a
     *  line, then draw the new line. The problem is that this leaves
     *  small black pixels on the figure. To fix this, I draw the
     *  entire figure using XDrawLines() afterwards.
     */
    if (aPrev) {
	old0 = aPrev;
	old1 = aPrev + 1;
	new0 = aCurr;
	new1 = aCurr + 1;
	for (q = cPoint - 1; q; q--) {
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
    XDrawLines(dpy, window, gcDraw, aCurr, cPoint, CoordModeOrigin);
    XFlush(dpy);
}

static void initLMorph (void)
{
    int               steps;
    XGCValues         gcv;
    XWindowAttributes wa;
    Colormap          cmap;
    
    cPoint = get_integer_resource("points", "Integer");
    steps = get_integer_resource("steps", "Integer");
    delay = get_integer_resource("delay", "Integer");

    if (steps <= 0)
      steps = (random() % 400) + 100;

    delta_gam = 1.0 / steps;
    XGetWindowAttributes(dpy, window, &wa);
    cmap = wa.colormap;
    gcv.foreground = get_pixel_resource("foreground", "Foreground", dpy, cmap);
    gcDraw = XCreateGC(dpy, window, GCForeground, &gcv);
    XSetForeground(dpy, gcDraw, gcv.foreground);
    gcv.foreground = get_pixel_resource("background", "Background", dpy, cmap);
    gcClear = XCreateGC(dpy, window, GCForeground, &gcv);
    XClearWindow(dpy, window);

    initPointArrays();
    aCurr = aWork[nWork = 0];
    aPrev = NULL;
    gam = 2.0;
    nTo = RND(cFig);

    {
      int width = random() % 10;
      int style = LineSolid;
      int cap   = (width > 1 ? CapRound  : CapButt);
      int join  = (width > 1 ? JoinRound : JoinBevel);
      if (width == 1) width = 0;
      XSetLineAttributes(dpy, gcDraw,  width, style, cap, join);
      XSetLineAttributes(dpy, gcClear, width, style, cap, join);
    }

}

static void animateLMorph (void)
{
    if (gam > maxGamma) {
        gam = 0.0;
        if (maxGamma == 1.0) {
            nFrom = nTo;
            aFrom = a[nFrom];
        } else {
            memcpy(aTmp, aCurr, cPoint * sizeof(XPoint));
            aFrom = aTmp;
            nFrom = -1;
        }
        do {
            nTo = RND(cFig);
        } while (nTo == nFrom);
        aTo = a[nTo];
        if (RND(2)) {
            /*
             *  Reverse the array to get more variation.
             */
            int    i1, i2;
            XPoint p;
            
            for (i1 = 0, i2 = cPoint - 1; i1 < cPoint / 2; i1++, i2--) {
                p = aTo[i1];
                aTo[i1] = aTo[i2];
                aTo[i2] = p;
            }
        }
        /*
         *  It may be nice to interrupt the next run.
         */
        if (RND(3) > 0)
            maxGamma = 0.1 + 0.7 * (RND(1001) / 1000.0);
        else
            maxGamma = 1.0;
    }

    createPoints();
    drawImage();
    aPrev = aCurr;
    aCurr = aWork[nWork ^= 1];

    gam += delta_gam;
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
