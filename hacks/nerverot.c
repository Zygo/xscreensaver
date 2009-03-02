/* nerverot, nervous rotation of random thingies, v1.0
 * by Dan Bornstein, danfuzz@milk.com
 * Copyright (c) 2000 Dan Bornstein.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * The goal of this screensaver is to be interesting and compelling to
 * watch, yet induce a state of nervous edginess in the viewer.
 *
 * Brief description of options/resources:
 *
 *   -fg <color>: foreground color
 *   -bg <color>: background color
 *   -delay <usec>: delay between frames
 *   -event-chance <frac>: chance, per iteration, that an interesting event
 *     will happen (range 0..1)
 *   -iter-amt <frac>: amount, per iteration, to move towards rotation and
 *     scale targets (range 0..1)
 *   -count <n>: number of blots
 *   -colors <n>: number of colors to use
 *   -lineWidth <n>: width of lines (0 means an optimized thin line)
 *   -nervousness <frac>: amount of nervousness (range 0..1)
 *   -min-scale <frac>: minimum scale of drawing as fraction of base scale
 *     (which is the minumum of the width or height of the screen) (range
 *     0..10)
 *   -max-scale <frac>: maximum scale of drawing as fraction of base scale
 *     (which is the minumum of the width or height of the screen) (range
 *     0..10)
 *   -min-radius <n>: minimum radius for drawing blots (range 1..100)
 *   -max-radius <n>: maximum radius for drawing blots (range 1..100)
 *   -max-nerve-radius <frac>: maximum nervousness radius (range 0..1)
 */

#include <math.h>
#include "screenhack.h"

#define FLOAT double

/* random float in the range (-1..1) */
#define RAND_FLOAT_PM1 \
        (((FLOAT) (random() & 0xffff)) / ((FLOAT) 0x10000) * 2 - 1)

/* random float in the range (0..1) */
#define RAND_FLOAT_01 \
        (((FLOAT) (random() & 0xffff)) / ((FLOAT) 0x10000))



/* parameters that are user configurable */

/* number of blots */
static int requestedBlotCount;

/* delay (usec) between iterations */
int delay;

/* variability of xoff/yoff per iteration (0..1) */
static FLOAT nervousness;

/* max nervousness radius (0..1) */
static FLOAT maxNerveRadius;

/* chance per iteration that an event will happen */
static FLOAT eventChance;

/* fraction (0..1) towards rotation target or scale target to move each
 * iteration */
static FLOAT iterAmt;

/* min and max scale for drawing, as fraction of baseScale */
static FLOAT minScale;
static FLOAT maxScale;

/* min and max radius of blot drawing */
static int minRadius;
static int maxRadius;

/* the number of colors to use */
static int colorCount;

/* width of lines */
static int lineWidth;



/* non-user-modifiable immutable definitions */

/* base scale factor for drawing, calculated as
 * max(screenWidth,screenHeight) */
static int baseScale;

/* width and height of the window */
static int windowWidth;
static int windowHeight;

/* center position of the window */
static int centerX;
static int centerY;

static Display *display; /* the display to draw on */
static Window window;    /* the window to draw on */
static GC *gcs;          /* array of gcs, one per color used */



/* structure of the model */

/* each point-like thingy to draw is represented as a blot */
typedef struct blot_s
{
    FLOAT x;           /* 3d x position (-1..1) */
    FLOAT y;           /* 3d y position (-1..1) */
    FLOAT z;           /* 3d z position (-1..1) */
    FLOAT xoff[3][3];  /* display x offset per drawn point (-1..1) */
    FLOAT yoff[3][3];  /* display x offset per drawn point (-1..1) */
} Blot;

/* each drawn line is represented as a LineSegment */
typedef struct linesegment_s
{
    GC gc;
    int x1;
    int y1;
    int x2;
    int y2;
} LineSegment;

/* array of the blots in the model */
static Blot *blots = NULL;
static int blotCount;

/* each blot draws as a simple 2d shape with each coordinate as an int
 * in the range (-1..1); this is the base shape */
static XPoint blotShape[] = { { 0, 0}, { 1, 0}, { 1, 1}, 
			      { 0, 1}, {-1, 1}, {-1, 0}, 
			      {-1,-1}, { 0,-1}, { 1,-1} };
static int blotShapeCount = sizeof (blotShape) / sizeof (XPoint);

/* two arrays of line segments; one for the ones to erase, and one for the
 * ones to draw */
static int segCount;
static LineSegment *segsToDraw = NULL;
static LineSegment *segsToErase = NULL;

/* current rotation values per axis, scale factor, and light position */
static FLOAT xRot;
static FLOAT yRot;
static FLOAT zRot;
static FLOAT curScale;
static FLOAT lightX;
static FLOAT lightY;
static FLOAT lightZ;

/* target rotation values per axis, scale factor, and light position */
static FLOAT xRotTarget;
static FLOAT yRotTarget;
static FLOAT zRotTarget;
static FLOAT scaleTarget;
static FLOAT lightXTarget;
static FLOAT lightYTarget;
static FLOAT lightZTarget;

/* current absolute offsets from the center */
static int centerXOff = 0;
static int centerYOff = 0;

/* iterations until the model changes */
static int itersTillNext;



/*
 * blot setup stuff
 */

/* initialize a blot with the given coordinates and random display offsets */
static void initBlot (Blot *b, FLOAT x, FLOAT y, FLOAT z)
{
    int i, j;

    b->x = x;
    b->y = y;
    b->z = z;

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    b->xoff[i][j] = RAND_FLOAT_PM1;
	    b->yoff[i][j] = RAND_FLOAT_PM1;
	}
    }
}

/* scale the blots to have a max distance of 1 from the center */
static void scaleBlotsToRadius1 (void)
{
    FLOAT max = 0.0;
    int n;

    for (n = 0; n < blotCount; n++)
    {
	FLOAT distSquare = 
	    blots[n].x * blots[n].x +
	    blots[n].y * blots[n].y +
	    blots[n].z * blots[n].z;
	if (distSquare > max)
	{
	    max = distSquare;
	}
    }

    if (max == 0.0)
    {
	return;
    }

    max = sqrt (max);

    for (n = 0; n < blotCount; n++)
    {
	blots[n].x /= max;
	blots[n].y /= max;
	blots[n].z /= max;
    }
}

/* randomly reorder the blots */
static void randomlyReorderBlots (void)
{
    int n;

    for (n = 0; n < blotCount; n++)
    {
	int m = RAND_FLOAT_01 * (blotCount - n) + n;
	Blot tmpBlot = blots[n];
	blots[n] = blots[m];
	blots[m] = tmpBlot;
    }
}

/* set up the initial array of blots to be a at the edge of a sphere */
static void setupBlotsSphere (void)
{
    int n;

    blotCount = requestedBlotCount;
    blots = calloc (sizeof (Blot), blotCount);

    for (n = 0; n < blotCount; n++)
    {
	/* pick a spot, but reject if its radius is < 0.2 or > 1 to
	 * avoid scaling problems */
	FLOAT x, y, z, radius;

	for (;;)
	{
	    x = RAND_FLOAT_PM1;
	    y = RAND_FLOAT_PM1;
	    z = RAND_FLOAT_PM1;

	    radius = sqrt (x * x + y * y + z * z);
	    if ((radius >= 0.2) && (radius <= 1.0))
	    {
		break;
	    }
	}

	x /= radius;
	y /= radius;
	z /= radius;

	initBlot (&blots[n], x, y, z);
    }

}

/* set up the initial array of blots to be a simple cube */
static void setupBlotsCube (void)
{
    int i, j, k, n;

    /* derive blotsPerEdge from blotCount, but then do the reverse
     * since roundoff may have changed blotCount */
    int blotsPerEdge = ((requestedBlotCount - 8) / 12) + 2;
    FLOAT distBetween;

    if (blotsPerEdge < 2)
    {
	blotsPerEdge = 2;
    }

    distBetween = 2.0 / (blotsPerEdge - 1.0);

    blotCount = 8 + (blotsPerEdge - 2) * 12;
    blots = calloc (sizeof (Blot), blotCount);
    n = 0;

    /* define the corners */
    for (i = -1; i < 2; i += 2)
    {
	for (j = -1; j < 2; j += 2)
	{
	    for (k = -1; k < 2; k += 2)
	    {
		initBlot (&blots[n], i, j, k);
		n++;
	    } 
	}
    }

    /* define the edges */
    for (i = 1; i < (blotsPerEdge - 1); i++)
    {
	FLOAT varEdge = distBetween * i - 1;
	initBlot (&blots[n++], varEdge, -1, -1);
	initBlot (&blots[n++], varEdge,  1, -1);
	initBlot (&blots[n++], varEdge, -1,  1);
	initBlot (&blots[n++], varEdge,  1,  1);
	initBlot (&blots[n++], -1, varEdge, -1);
	initBlot (&blots[n++],  1, varEdge, -1);
	initBlot (&blots[n++], -1, varEdge,  1);
	initBlot (&blots[n++],  1, varEdge,  1);
	initBlot (&blots[n++], -1, -1, varEdge);
	initBlot (&blots[n++],  1, -1, varEdge);
	initBlot (&blots[n++], -1,  1, varEdge);
	initBlot (&blots[n++],  1,  1, varEdge);
    }

    scaleBlotsToRadius1 ();
    randomlyReorderBlots ();
}


/* set up the initial array of blots to be a cylinder */
static void setupBlotsCylinder (void)
{
    int i, j, n;

    /* derive blotsPerEdge from blotCount, but then do the reverse
     * since roundoff may have changed blotCount */
    int blotsPerEdge = requestedBlotCount / 32;
    FLOAT distBetween;

    if (blotsPerEdge < 2)
    {
	blotsPerEdge = 2;
    }

    distBetween = 2.0 / (blotsPerEdge - 1);

    blotCount = blotsPerEdge * 32;
    blots = calloc (sizeof (Blot), blotCount);
    n = 0;

    /* define the edges */
    for (i = 0; i < 32; i++)
    {
	FLOAT x = sin (2 * M_PI / 32 * i);
	FLOAT y = cos (2 * M_PI / 32 * i);
	for (j = 0; j < blotsPerEdge; j++)
	{
	    initBlot (&blots[n], x, y, j * distBetween - 1);
	    n++;
	}
    }

    scaleBlotsToRadius1 ();
    randomlyReorderBlots ();
}



/* set up the initial array of blots to be a squiggle */
static void setupBlotsSquiggle (void)
{
    FLOAT x, y, z, xv, yv, zv, len;
    int n;

    blotCount = requestedBlotCount;
    blots = calloc (sizeof (Blot), blotCount);

    x = RAND_FLOAT_PM1;
    y = RAND_FLOAT_PM1;
    z = RAND_FLOAT_PM1;

    xv = RAND_FLOAT_PM1;
    yv = RAND_FLOAT_PM1;
    zv = RAND_FLOAT_PM1;
    len = sqrt (xv * xv + yv * yv + zv * zv);
    xv /= len;
    yv /= len;
    zv /= len;
    
    for (n = 0; n < blotCount; n++)
    {
	FLOAT newx, newy, newz;
	initBlot (&blots[n], x, y, z);

	for (;;)
	{
	    xv += RAND_FLOAT_PM1 * 0.1;
	    yv += RAND_FLOAT_PM1 * 0.1;
	    zv += RAND_FLOAT_PM1 * 0.1;
	    len = sqrt (xv * xv + yv * yv + zv * zv);
	    xv /= len;
	    yv /= len;
	    zv /= len;

	    newx = x + xv * 0.1;
	    newy = y + yv * 0.1;
	    newz = z + zv * 0.1;

	    if (   (newx >= -1) && (newx <= 1)
		&& (newy >= -1) && (newy <= 1)
		&& (newz >= -1) && (newz <= 1))
	    {
		break;
	    }
	}

	x = newx;
	y = newy;
	z = newz;
    }

    scaleBlotsToRadius1 ();
    randomlyReorderBlots ();
}



/* free the blots, in preparation for a new shape */
static void freeBlots (void)
{
    if (blots != NULL)
    {
	free (blots);
	blots = NULL;
    }

    if (segsToErase != NULL)
    {
	free (segsToErase);
	segsToErase = NULL;
    }

    if (segsToDraw != NULL)
    {
	free (segsToDraw);
	segsToDraw = NULL;
    }
}



/* set up the initial arrays of blots */
static void setupBlots (void)
{
    int which = RAND_FLOAT_01 * 4;

    freeBlots ();

    switch (which)
    {
	case 0:
	    setupBlotsCube ();
	    break;
	case 1:
	    setupBlotsSphere ();
	    break;
	case 2:
	    setupBlotsCylinder ();
	    break;
	case 3:
	    setupBlotsSquiggle ();
	    break;
    }

    /* there are blotShapeCount - 1 line segments per blot */
    segCount = blotCount * (blotShapeCount - 1);
    segsToErase = calloc (sizeof (LineSegment), segCount);
    segsToDraw = calloc (sizeof (LineSegment), segCount);

    /* erase the world */
    XFillRectangle (display, window, gcs[0], 0, 0, windowWidth, windowHeight);
}



/*
 * color setup stuff
 */

/* set up the colormap */
static void setupColormap (XWindowAttributes *xgwa)
{
    int n;
    XGCValues gcv;
    XColor *colors = (XColor *) calloc (sizeof (XColor), colorCount + 1);

    unsigned short r, g, b;
    int h1, h2;
    double s1, s2, v1, v2;

    r = RAND_FLOAT_01 * 0x10000;
    g = RAND_FLOAT_01 * 0x10000;
    b = RAND_FLOAT_01 * 0x10000;
    rgb_to_hsv (r, g, b, &h1, &s1, &v1);
    v1 = 1.0;
    s1 = 1.0;

    r = RAND_FLOAT_01 * 0x10000;
    g = RAND_FLOAT_01 * 0x10000;
    b = RAND_FLOAT_01 * 0x10000;
    rgb_to_hsv (r, g, b, &h2, &s2, &v2);
    s2 = 0.7;
    v2 = 0.7;
    
    colors[0].pixel = get_pixel_resource ("background", "Background",
					  display, xgwa->colormap);
    
    make_color_ramp (display, xgwa->colormap, h1, s1, v1, h2, s2, v2,
		     colors + 1, &colorCount, False, True, False);

    if (colorCount < 1)
    {
        fprintf (stderr, "%s: couldn't allocate any colors\n", progname);
	exit (-1);
    }
    
    gcs = (GC *) calloc (sizeof (GC), colorCount + 1);

    for (n = 0; n <= colorCount; n++) 
    {
	gcv.foreground = colors[n].pixel;
	gcv.line_width = lineWidth;
	gcs[n] = XCreateGC (display, window, GCForeground | GCLineWidth, &gcv);
    }

    free (colors);
}



/*
 * overall setup stuff
 */

/* set up the system */
static void setup (void)
{
    XWindowAttributes xgwa;

    XGetWindowAttributes (display, window, &xgwa);

    windowWidth = xgwa.width;
    windowHeight = xgwa.height;
    centerX = windowWidth / 2;
    centerY = windowHeight / 2;
    baseScale = (xgwa.height < xgwa.width) ? xgwa.height : xgwa.width;

    setupColormap (&xgwa);
    setupBlots ();

    /* set up the initial rotation, scale, and light values as random, but
     * with the targets equal to where it is */
    xRot = xRotTarget = RAND_FLOAT_01 * M_PI;
    yRot = yRotTarget = RAND_FLOAT_01 * M_PI;
    zRot = zRotTarget = RAND_FLOAT_01 * M_PI;
    curScale = scaleTarget = RAND_FLOAT_01 * (maxScale - minScale) + minScale;
    lightX = lightXTarget = RAND_FLOAT_PM1;
    lightY = lightYTarget = RAND_FLOAT_PM1;
    lightZ = lightZTarget = RAND_FLOAT_PM1;

    itersTillNext = RAND_FLOAT_01 * 1234;
}



/*
 * the simulation
 */

/* "render" the blots into segsToDraw, with the current rotation factors */
static void renderSegs (void)
{
    int n;
    int m = 0;

    /* rotation factors */
    FLOAT sinX = sin (xRot);
    FLOAT cosX = cos (xRot);
    FLOAT sinY = sin (yRot);
    FLOAT cosY = cos (yRot);
    FLOAT sinZ = sin (zRot);
    FLOAT cosZ = cos (zRot);

    for (n = 0; n < blotCount; n++)
    {
	Blot *b = &blots[n];
	int i, j;
	int baseX, baseY;
	FLOAT radius;
	int x[3][3];
	int y[3][3];
	int color;

	FLOAT x1 = blots[n].x;
	FLOAT y1 = blots[n].y;
	FLOAT z1 = blots[n].z;
	FLOAT x2, y2, z2;

	/* rotate on z axis */
	x2 = x1 * cosZ - y1 * sinZ;
	y2 = x1 * sinZ + y1 * cosZ;
	z2 = z1;

	/* rotate on x axis */
	y1 = y2 * cosX - z2 * sinX;
	z1 = y2 * sinX + z2 * cosX;
	x1 = x2;

	/* rotate on y axis */
	z2 = z1 * cosY - x1 * sinY;
	x2 = z1 * sinY + x1 * cosY;
	y2 = y1;

	/* the color to draw is based on the distance from the light of
	 * the post-rotation blot */
	x1 = x2 - lightX;
	y1 = y2 - lightY;
	z1 = z2 - lightZ;
	color = 1 + (x1 * x1 + y1 * y1 + z1 * z1) / 4 * colorCount;
	if (color > colorCount)
	{
	    color = colorCount;
	}

	/* set up the base screen coordinates for drawing */
	baseX = x2 / 2 * baseScale * curScale + centerX + centerXOff;
	baseY = y2 / 2 * baseScale * curScale + centerY + centerYOff;
	
	radius = (z2 + 1) / 2 * (maxRadius - minRadius) + minRadius;

	for (i = 0; i < 3; i++)
	{
	    for (j = 0; j < 3; j++)
	    {
		x[i][j] = baseX + 
		    ((i - 1) + (b->xoff[i][j] * maxNerveRadius)) * radius;
		y[i][j] = baseY + 
		    ((j - 1) + (b->yoff[i][j] * maxNerveRadius)) * radius;
	    }
	}

	for (i = 1; i < blotShapeCount; i++)
	{
	    segsToDraw[m].gc = gcs[color];
	    segsToDraw[m].x1 = x[blotShape[i-1].x + 1][blotShape[i-1].y + 1];
	    segsToDraw[m].y1 = y[blotShape[i-1].x + 1][blotShape[i-1].y + 1];
	    segsToDraw[m].x2 = x[blotShape[i].x   + 1][blotShape[i].y   + 1];
	    segsToDraw[m].y2 = y[blotShape[i].x   + 1][blotShape[i].y   + 1];
	    m++;
	}
    }
}

/* update blots, adjusting the offsets and rotation factors. */
static void updateWithFeeling (void)
{
    int n, i, j;

    /* pick a new model if the time is right */
    itersTillNext--;
    if (itersTillNext < 0)
    {
	itersTillNext = RAND_FLOAT_01 * 1234;
	setupBlots ();
	renderSegs ();
    }

    /* update the rotation factors by moving them a bit toward the targets */
    xRot = xRot + (xRotTarget - xRot) * iterAmt; 
    yRot = yRot + (yRotTarget - yRot) * iterAmt;
    zRot = zRot + (zRotTarget - zRot) * iterAmt;

    /* similarly the scale factor */
    curScale = curScale + (scaleTarget - curScale) * iterAmt;

    /* and similarly the light position */
    lightX = lightX + (lightXTarget - lightX) * iterAmt; 
    lightY = lightY + (lightYTarget - lightY) * iterAmt; 
    lightZ = lightZ + (lightZTarget - lightZ) * iterAmt; 

    /* for each blot... */
    for (n = 0; n < blotCount; n++)
    {
	/* add a bit of random jitter to xoff/yoff */
	for (i = 0; i < 3; i++)
	{
	    for (j = 0; j < 3; j++)
	    {
		FLOAT newOff;

		newOff = blots[n].xoff[i][j] + RAND_FLOAT_PM1 * nervousness;
		if (newOff < -1) newOff = -(newOff + 1) - 1;
		else if (newOff > 1) newOff = -(newOff - 1) + 1;
		blots[n].xoff[i][j] = newOff;

		newOff = blots[n].yoff[i][j] + RAND_FLOAT_PM1 * nervousness;
		if (newOff < -1) newOff = -(newOff + 1) - 1;
		else if (newOff > 1) newOff = -(newOff - 1) + 1;
		blots[n].yoff[i][j] = newOff;
	    }
	}
    }

    /* depending on random chance, update one or more factors */
    if (RAND_FLOAT_01 <= eventChance)
    {
	int which = RAND_FLOAT_01 * 14;
	switch (which)
	{
	    case 0:
	    {
		xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 1:
	    {
		yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 2:
	    {
		zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 3:
	    {
		xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 4:
	    {
		xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 5:
	    {
		yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 6:
	    {
		xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 7:
	    {
		centerXOff = RAND_FLOAT_PM1 * maxRadius / 2;
		break;
	    }
	    case 8:
	    {
		centerYOff = RAND_FLOAT_PM1 * maxRadius / 2;
		break;
	    }
	    case 9:
	    {
		centerXOff = RAND_FLOAT_PM1 * maxRadius / 2;
		centerYOff = RAND_FLOAT_PM1 * maxRadius / 2;
		break;
	    }
	    case 10:
	    {
		scaleTarget = 
		    RAND_FLOAT_01 * (maxScale - minScale) + minScale;
		break;
	    }
	    case 11:
	    {
		curScale = 
		    RAND_FLOAT_01 * (maxScale - minScale) + minScale;
		break;
	    }
	    case 12:
	    {
		lightX = RAND_FLOAT_PM1;
		lightY = RAND_FLOAT_PM1;
		lightZ = RAND_FLOAT_PM1;
		break;
	    }
	    case 13:
	    {
		lightXTarget = RAND_FLOAT_PM1;
		lightYTarget = RAND_FLOAT_PM1;
		lightZTarget = RAND_FLOAT_PM1;
		break;
	    }
	}
    }
}

/* erase segsToErase and draw segsToDraw */
static void eraseAndDraw (void)
{
    int n;

    for (n = 0; n < segCount; n++)
    {
	LineSegment *seg = &segsToErase[n];
	XDrawLine (display, window, gcs[0], 
		   seg->x1, seg->y1, seg->x2, seg->y2);
	seg = &segsToDraw[n];
	XDrawLine (display, window, seg->gc,
		   seg->x1, seg->y1, seg->x2, seg->y2);
    }
}

/* do one iteration */
static void oneIteration (void)
{
    /* switch segsToErase and segsToDraw */
    LineSegment *temp = segsToDraw;
    segsToDraw = segsToErase;
    segsToErase = temp;

    /* update the model */
    updateWithFeeling ();

    /* render new segments */
    renderSegs ();

    /* erase old segments and draw new ones */
    eraseAndDraw ();
}

char *progclass = "NerveRot";

char *defaults [] = {
    ".background:	black",
    ".foreground:	white",
    "*count:		250",
    "*colors:		4",
    "*delay:		10000",
    "*eventChance:      0.2",
    "*iterAmt:          0.01",
    "*lineWidth:        0",
    "*minScale:         0.6",
    "*maxScale:         1.75",
    "*minRadius:        3",
    "*maxRadius:        25",
    "*maxNerveRadius:	0.7",
    "*nervousness:	0.3",
    0
};

XrmOptionDescRec options [] = {
  { "-count",            ".count",          XrmoptionSepArg, 0 },
  { "-colors",           ".colors",         XrmoptionSepArg, 0 },
  { "-cube",             ".cube",           XrmoptionNoArg,  "true" },
  { "-delay",            ".delay",          XrmoptionSepArg, 0 },
  { "-event-chance",     ".eventChance",    XrmoptionSepArg, 0 },
  { "-iter-amt",         ".iterAmt",        XrmoptionSepArg, 0 },
  { "-line-width",       ".lineWidth",      XrmoptionSepArg, 0 },
  { "-min-scale",        ".minScale",       XrmoptionSepArg, 0 },
  { "-max-scale",        ".maxScale",       XrmoptionSepArg, 0 },
  { "-min-radius",       ".minRadius",      XrmoptionSepArg, 0 },
  { "-max-radius",       ".maxRadius",      XrmoptionSepArg, 0 },
  { "-max-nerve-radius", ".maxNerveRadius", XrmoptionSepArg, 0 },
  { "-nervousness",      ".nervousness",    XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

/* initialize the user-specifiable params */
static void initParams (void)
{
    int problems = 0;

    delay = get_integer_resource ("delay", "Delay");
    if (delay < 0)
    {
	fprintf (stderr, "error: delay must be at least 0\n");
	problems = 1;
    }

    requestedBlotCount = get_integer_resource ("count", "Count");
    if (requestedBlotCount <= 0)
    {
	fprintf (stderr, "error: count must be at least 0\n");
	problems = 1;
    }

    colorCount = get_integer_resource ("colors", "Colors");
    if (colorCount <= 0)
    {
	fprintf (stderr, "error: colors must be at least 1\n");
	problems = 1;
    }

    lineWidth = get_integer_resource ("lineWidth", "LineWidth");
    if (lineWidth < 0)
    {
	fprintf (stderr, "error: line width must be at least 0\n");
	problems = 1;
    }

    nervousness = get_float_resource ("nervousness", "Float");
    if ((nervousness < 0) || (nervousness > 1))
    {
	fprintf (stderr, "error: nervousness must be in the range 0..1\n");
	problems = 1;
    }

    maxNerveRadius = get_float_resource ("maxNerveRadius", "Float");
    if ((maxNerveRadius < 0) || (maxNerveRadius > 1))
    {
	fprintf (stderr, "error: maxNerveRadius must be in the range 0..1\n");
	problems = 1;
    }

    eventChance = get_float_resource ("eventChance", "Float");
    if ((eventChance < 0) || (eventChance > 1))
    {
	fprintf (stderr, "error: eventChance must be in the range 0..1\n");
	problems = 1;
    }

    iterAmt = get_float_resource ("iterAmt", "Float");
    if ((iterAmt < 0) || (iterAmt > 1))
    {
	fprintf (stderr, "error: iterAmt must be in the range 0..1\n");
	problems = 1;
    }

    minScale = get_float_resource ("minScale", "Float");
    if ((minScale < 0) || (minScale > 10))
    {
	fprintf (stderr, "error: minScale must be in the range 0..10\n");
	problems = 1;
    }

    maxScale = get_float_resource ("maxScale", "Float");
    if ((maxScale < 0) || (maxScale > 10))
    {
	fprintf (stderr, "error: maxScale must be in the range 0..10\n");
	problems = 1;
    }

    if (maxScale < minScale)
    {
	fprintf (stderr, "error: maxScale must be >= minScale\n");
	problems = 1;
    }	

    minRadius = get_integer_resource ("minRadius", "Integer");
    if ((minRadius < 1) || (minRadius > 100))
    {
	fprintf (stderr, "error: minRadius must be in the range 1..100\n");
	problems = 1;
    }

    maxRadius = get_integer_resource ("maxRadius", "Integer");
    if ((maxRadius < 1) || (maxRadius > 100))
    {
	fprintf (stderr, "error: maxRadius must be in the range 1..100\n");
	problems = 1;
    }

    if (maxRadius < minRadius)
    {
	fprintf (stderr, "error: maxRadius must be >= minRadius\n");
	problems = 1;
    }	

    if (problems)
    {
	exit (1);
    }
}

/* main function */
void screenhack (Display *dpy, Window win)
{
    display = dpy;
    window = win;

    initParams ();
    setup ();

    /* make a valid set to erase at first */
    renderSegs ();
    
    for (;;) 
    {
	oneIteration ();
        XSync (dpy, False);
        screenhack_handle_events (dpy);
	usleep (delay);
    }
}
