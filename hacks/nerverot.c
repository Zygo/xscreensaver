/* nerverot, nervous rotation of random thingies, v1.4
 * by Dan Bornstein, danfuzz@milk.com
 * Copyright (c) 2000-2001 Dan Bornstein. All rights reserved.
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
 * See the included man page for more details.
 */

#include <math.h>
#include "screenhack.h"

#define FLOAT double

/* random float in the range (-1..1) */
#define RAND_FLOAT_PM1 \
        (((FLOAT) ((random() >> 8) & 0xffff)) / ((FLOAT) 0x10000) * 2 - 1)

/* random float in the range (0..1) */
#define RAND_FLOAT_01 \
        (((FLOAT) ((random() >> 8) & 0xffff)) / ((FLOAT) 0x10000))


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

/* each blot draws as a simple 2d shape with each coordinate as an int
 * in the range (-1..1); this is the base shape */
static const XPoint blotShape[] = { { 0, 0}, { 1, 0}, { 1, 1}, 
                                    { 0, 1}, {-1, 1}, {-1, 0}, 
                                    {-1,-1}, { 0,-1}, { 1,-1} };
static int blotShapeCount = sizeof (blotShape) / sizeof (XPoint);





struct state {
  Display *dpy;
  Window window;

   int requestedBlotCount;	/* number of blots */
   int delay;		/* delay (usec) between iterations */
   int maxIters;		/* max iterations per model */
   FLOAT nervousness;	/* variability of xoff/yoff per iteration (0..1) */
   FLOAT maxNerveRadius;	/* max nervousness radius (0..1) */
   FLOAT eventChance;	/* chance per iteration that an event will happen */
   FLOAT iterAmt;		/* fraction (0..1) towards rotation target or scale target to move each * iteration */
   FLOAT minScale;		/* min and max scale for drawing, as fraction of baseScale */
   FLOAT maxScale;
   int minRadius;		/* min and max radius of blot drawing */
   int maxRadius;
   int colorCount;		/* the number of colors to use */

   int lineWidth;		/* width of lines */

   Bool doubleBuffer;	/* whether or not to do double-buffering */


   int baseScale;		/* base scale factor for drawing, calculated as max(screenWidth,screenHeight) */


   int windowWidth;		/* width and height of the window */
   int windowHeight;

   int centerX;		/* center position of the window */
   int centerY;

   Drawable drawable; /* the thing to directly draw on */
   GC *gcs;           /* array of gcs, one per color used */

   Blot *blots;	/* array of the blots in the model */
   int blotCount;

   int segCount;		/* two arrays of line segments; one for the ones to erase, and one for the ones to draw */

   LineSegment *segsToDraw;
   LineSegment *segsToErase;


   FLOAT xRot;		/* current rotation values per axis, scale factor, and light position */

   FLOAT yRot;
   FLOAT zRot;
   FLOAT curScale;
   FLOAT lightX;
   FLOAT lightY;
   FLOAT lightZ;

   FLOAT xRotTarget;	/* target rotation values per axis, scale factor, and light position */

   FLOAT yRotTarget;
   FLOAT zRotTarget;
   FLOAT scaleTarget;
   FLOAT lightXTarget;
   FLOAT lightYTarget;
   FLOAT lightZTarget;

   int centerXOff;	/* current absolute offsets from the center */
   int centerYOff;

   int itersTillNext;	/* iterations until the model changes */
};


/*
 * generic blot setup and manipulation
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
static void scaleBlotsToRadius1 (struct state *st)
{
    FLOAT max = 0.0;
    int n;

    for (n = 0; n < st->blotCount; n++)
    {
	FLOAT distSquare = 
	    st->blots[n].x * st->blots[n].x +
	    st->blots[n].y * st->blots[n].y +
	    st->blots[n].z * st->blots[n].z;
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

    for (n = 0; n < st->blotCount; n++)
    {
	st->blots[n].x /= max;
	st->blots[n].y /= max;
	st->blots[n].z /= max;
    }
}

/* randomly reorder the blots */
static void randomlyReorderBlots (struct state *st)
{
    int n;

    for (n = 0; n < st->blotCount; n++)
    {
	int m = RAND_FLOAT_01 * (st->blotCount - n) + n;
	Blot tmpBlot = st->blots[n];
	st->blots[n] = st->blots[m];
	st->blots[m] = tmpBlot;
    }
}

/* randomly rotate the blots around the origin */
static void randomlyRotateBlots (struct state *st)
{
    int n;

    /* random amounts to rotate about each axis */
    FLOAT xRot = RAND_FLOAT_PM1 * M_PI;
    FLOAT yRot = RAND_FLOAT_PM1 * M_PI;
    FLOAT zRot = RAND_FLOAT_PM1 * M_PI;

    /* rotation factors */
    FLOAT sinX = sin (xRot);
    FLOAT cosX = cos (xRot);
    FLOAT sinY = sin (yRot);
    FLOAT cosY = cos (yRot);
    FLOAT sinZ = sin (zRot);
    FLOAT cosZ = cos (zRot);

    for (n = 0; n < st->blotCount; n++)
    {
	FLOAT x1 = st->blots[n].x;
	FLOAT y1 = st->blots[n].y;
	FLOAT z1 = st->blots[n].z;
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

	st->blots[n].x = x2;
	st->blots[n].y = y2;
	st->blots[n].z = z2;
    }
}



/*
 * blot configurations
 */

/* set up the initial array of blots to be a at the edge of a sphere */
static void setupBlotsSphere (struct state *st)
{
    int n;

    st->blotCount = st->requestedBlotCount;
    st->blots = calloc (sizeof (Blot), st->blotCount);

    for (n = 0; n < st->blotCount; n++)
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

	initBlot (&st->blots[n], x, y, z);
    }
}

/* set up the initial array of blots to be a simple cube */
static void setupBlotsCube (struct state *st)
{
    int i, j, k, n;

    /* derive blotsPerEdge from blotCount, but then do the reverse
     * since roundoff may have changed blotCount */
    int blotsPerEdge = ((st->requestedBlotCount - 8) / 12) + 2;
    FLOAT distBetween;

    if (blotsPerEdge < 2)
    {
	blotsPerEdge = 2;
    }

    distBetween = 2.0 / (blotsPerEdge - 1.0);

    st->blotCount = 8 + (blotsPerEdge - 2) * 12;
    st->blots = calloc (sizeof (Blot), st->blotCount);
    n = 0;

    /* define the corners */
    for (i = -1; i < 2; i += 2)
    {
	for (j = -1; j < 2; j += 2)
	{
	    for (k = -1; k < 2; k += 2)
	    {
		initBlot (&st->blots[n], i, j, k);
		n++;
	    } 
	}
    }

    /* define the edges */
    for (i = 1; i < (blotsPerEdge - 1); i++)
    {
	FLOAT varEdge = distBetween * i - 1;
	initBlot (&st->blots[n++], varEdge, -1, -1);
	initBlot (&st->blots[n++], varEdge,  1, -1);
	initBlot (&st->blots[n++], varEdge, -1,  1);
	initBlot (&st->blots[n++], varEdge,  1,  1);
	initBlot (&st->blots[n++], -1, varEdge, -1);
	initBlot (&st->blots[n++],  1, varEdge, -1);
	initBlot (&st->blots[n++], -1, varEdge,  1);
	initBlot (&st->blots[n++],  1, varEdge,  1);
	initBlot (&st->blots[n++], -1, -1, varEdge);
	initBlot (&st->blots[n++],  1, -1, varEdge);
	initBlot (&st->blots[n++], -1,  1, varEdge);
	initBlot (&st->blots[n++],  1,  1, varEdge);
    }

    scaleBlotsToRadius1 (st);
    randomlyReorderBlots (st);
    randomlyRotateBlots (st);
}

/* set up the initial array of blots to be a cylinder */
static void setupBlotsCylinder (struct state *st)
{
    int i, j, n;
    FLOAT distBetween;

    /* derive blotsPerEdge and blotsPerRing from blotCount, but then do the
     * reverse since roundoff may have changed blotCount */
    FLOAT reqRoot = sqrt ((FLOAT) st->requestedBlotCount);
    int blotsPerRing = ceil (RAND_FLOAT_PM1 * reqRoot) / 2 + reqRoot;
    int blotsPerEdge = st->requestedBlotCount / blotsPerRing;

    if (blotsPerRing < 2)
    {
	blotsPerRing = 2;
    }

    if (blotsPerEdge < 2)
    {
	blotsPerEdge = 2;
    }

    distBetween = 2.0 / (blotsPerEdge - 1);

    st->blotCount = blotsPerEdge * blotsPerRing;
    st->blots = calloc (sizeof (Blot), st->blotCount);
    n = 0;

    /* define the edges */
    for (i = 0; i < blotsPerRing; i++)
    {
	FLOAT x = sin (2 * M_PI / blotsPerRing * i);
	FLOAT y = cos (2 * M_PI / blotsPerRing * i);
	for (j = 0; j < blotsPerEdge; j++)
	{
	    initBlot (&st->blots[n], x, y, j * distBetween - 1);
	    n++;
	}
    }

    scaleBlotsToRadius1 (st);
    randomlyReorderBlots (st);
    randomlyRotateBlots (st);
}

/* set up the initial array of blots to be a squiggle */
static void setupBlotsSquiggle (struct state *st)
{
    FLOAT x, y, z, xv, yv, zv, len;
    int minCoor, maxCoor;
    int n;

    st->blotCount = st->requestedBlotCount;
    st->blots = calloc (sizeof (Blot), st->blotCount);

    maxCoor = (int) (RAND_FLOAT_01 * 5) + 1;
    minCoor = -maxCoor;

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
    
    for (n = 0; n < st->blotCount; n++)
    {
	FLOAT newx, newy, newz;
	initBlot (&st->blots[n], x, y, z);

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

	    if (   (newx >= minCoor) && (newx <= maxCoor)
		&& (newy >= minCoor) && (newy <= maxCoor)
		&& (newz >= minCoor) && (newz <= maxCoor))
	    {
		break;
	    }
	}

	x = newx;
	y = newy;
	z = newz;
    }

    scaleBlotsToRadius1 (st);
    randomlyReorderBlots (st);
}

/* set up the initial array of blots to be near the corners of a
 * cube, distributed slightly */
static void setupBlotsCubeCorners (struct state *st)
{
    int n;

    st->blotCount = st->requestedBlotCount;
    st->blots = calloc (sizeof (Blot), st->blotCount);

    for (n = 0; n < st->blotCount; n++)
    {
	FLOAT x = rint (RAND_FLOAT_01) * 2 - 1;
	FLOAT y = rint (RAND_FLOAT_01) * 2 - 1;
	FLOAT z = rint (RAND_FLOAT_01) * 2 - 1;

	x += RAND_FLOAT_PM1 * 0.3;
	y += RAND_FLOAT_PM1 * 0.3;
	z += RAND_FLOAT_PM1 * 0.3;

	initBlot (&st->blots[n], x, y, z);
    }

    scaleBlotsToRadius1 (st);
    randomlyRotateBlots (st);
}

/* set up the initial array of blots to be randomly distributed
 * on the surface of a tetrahedron */
static void setupBlotsTetrahedron (struct state *st)
{
    /* table of corners of the tetrahedron */
    static const FLOAT cor[4][3] = { {  0.0,   1.0,  0.0 },
                                     { -0.75, -0.5, -0.433013 },
                                     {  0.0,  -0.5,  0.866025 },
                                     {  0.75, -0.5, -0.433013 } };

    int n, c;

    /* derive blotsPerSurface from blotCount, but then do the reverse
     * since roundoff may have changed blotCount */
    int blotsPerSurface = st->requestedBlotCount / 4;

    st->blotCount = blotsPerSurface * 4;
    st->blots = calloc (sizeof (Blot), st->blotCount);

    for (n = 0; n < st->blotCount; n += 4)
    {
	/* pick a random point on a unit right triangle */
	FLOAT rawx = RAND_FLOAT_01;
	FLOAT rawy = RAND_FLOAT_01;

	if ((rawx + rawy) > 1)
	{
	    /* swap coords into place */
	    FLOAT t = 1.0 - rawx;
	    rawx = 1.0 - rawy;
	    rawy = t;
	}

	/* translate the point to be on each of the surfaces */
	for (c = 0; c < 4; c++)
	{
	    FLOAT x, y, z;
	    
	    int c1 = (c + 1) % 4;
	    int c2 = (c + 2) % 4;
	    
	    x = (cor[c1][0] - cor[c][0]) * rawx + 
		(cor[c2][0] - cor[c][0]) * rawy + 
		cor[c][0];

	    y = (cor[c1][1] - cor[c][1]) * rawx + 
		(cor[c2][1] - cor[c][1]) * rawy + 
		cor[c][1];

	    z = (cor[c1][2] - cor[c][2]) * rawx + 
		(cor[c2][2] - cor[c][2]) * rawy + 
		cor[c][2];

	    initBlot (&st->blots[n + c], x, y, z);
	}
    }

    randomlyRotateBlots (st);
}

/* set up the initial array of blots to be an almost-evenly-distributed
 * square sheet */
static void setupBlotsSheet (struct state *st)
{
    int x, y;

    int blotsPerDimension = floor (sqrt (st->requestedBlotCount));
    FLOAT spaceBetween;

    if (blotsPerDimension < 2)
    {
	blotsPerDimension = 2;
    }

    spaceBetween = 2.0 / (blotsPerDimension - 1);

    st->blotCount = blotsPerDimension * blotsPerDimension;
    st->blots = calloc (sizeof (Blot), st->blotCount);

    for (x = 0; x < blotsPerDimension; x++)
    {
	for (y = 0; y < blotsPerDimension; y++)
	{
	    FLOAT x1 = x * spaceBetween - 1.0;
	    FLOAT y1 = y * spaceBetween - 1.0;
	    FLOAT z1 = 0.0;

	    x1 += RAND_FLOAT_PM1 * spaceBetween / 3;
	    y1 += RAND_FLOAT_PM1 * spaceBetween / 3;
	    z1 += RAND_FLOAT_PM1 * spaceBetween / 2;

	    initBlot (&st->blots[x + y * blotsPerDimension], x1, y1, z1);
	}
    }

    scaleBlotsToRadius1 (st);
    randomlyReorderBlots (st);
    randomlyRotateBlots (st);
}

/* set up the initial array of blots to be a swirlycone */
static void setupBlotsSwirlyCone (struct state *st)
{
    FLOAT radSpace = 1.0 / (st->requestedBlotCount - 1);
    FLOAT zSpace = radSpace * 2;
    FLOAT rotAmt = RAND_FLOAT_PM1 * M_PI / 10;

    int n;
    FLOAT rot = 0.0;

    st->blotCount = st->requestedBlotCount;
    st->blots = calloc (sizeof (Blot), st->blotCount);

    for (n = 0; n < st->blotCount; n++)
    {
	FLOAT radius = n * radSpace;
	FLOAT x = cos (rot) * radius;
	FLOAT y = sin (rot) * radius;
	FLOAT z = n * zSpace - 1.0;

	rot += rotAmt;
	initBlot (&st->blots[n], x, y, z);
    }

    scaleBlotsToRadius1 (st);
    randomlyReorderBlots (st);
    randomlyRotateBlots (st);
}

/* forward declaration for recursive use immediately below */
static void setupBlots (struct state *st);

/* set up the blots to be two of the other choices, placed next to
 * each other */
static void setupBlotsDuo (struct state *st)
{
    int origRequest = st->requestedBlotCount;
    FLOAT tx, ty, tz, radius;
    Blot *blots1, *blots2;
    int count1, count2;
    int n;

    if (st->requestedBlotCount < 15)
    {
	/* special case bottom-out */
	setupBlotsSphere (st);
	return;
    }

    tx = RAND_FLOAT_PM1;
    ty = RAND_FLOAT_PM1;
    tz = RAND_FLOAT_PM1;
    radius = sqrt (tx * tx + ty * ty + tz * tz);
    tx /= radius;
    ty /= radius;
    tz /= radius;

    /* recursive call to setup set 1 */
    st->requestedBlotCount = origRequest / 2;
    setupBlots (st);

    if (st->blotCount >= origRequest)
    {
	/* return immediately if this satisfies the original count request */
	st->requestedBlotCount = origRequest;
	return;
    }

    blots1 = st->blots;
    count1 = st->blotCount;
    st->blots = NULL;
    st->blotCount = 0;
    
    /* translate to new position */
    for (n = 0; n < count1; n++)
    {
	blots1[n].x += tx;
	blots1[n].y += ty;
	blots1[n].z += tz;
    }

    /* recursive call to setup set 2 */
    st->requestedBlotCount = origRequest - count1;
    setupBlots (st);
    blots2 = st->blots;
    count2 = st->blotCount;

    /* translate to new position */
    for (n = 0; n < count2; n++)
    {
	blots2[n].x -= tx;
	blots2[n].y -= ty;
	blots2[n].z -= tz;
    }

    /* combine the two arrays */
    st->blotCount = count1 + count2;
    st->blots = calloc (sizeof (Blot), st->blotCount);
    memcpy (&st->blots[0],      blots1, sizeof (Blot) * count1);
    memcpy (&st->blots[count1], blots2, sizeof (Blot) * count2);
    free (blots1);
    free (blots2);

    scaleBlotsToRadius1 (st);
    randomlyReorderBlots (st);

    /* restore the original requested count, for future iterations */
    st->requestedBlotCount = origRequest;
}



/*
 * main blot setup
 */

/* free the blots, in preparation for a new shape */
static void freeBlots (struct state *st)
{
    if (st->blots != NULL)
    {
	free (st->blots);
	st->blots = NULL;
    }

    if (st->segsToErase != NULL)
    {
	free (st->segsToErase);
	st->segsToErase = NULL;
    }

    if (st->segsToDraw != NULL)
    {
	free (st->segsToDraw);
	st->segsToDraw = NULL;
    }
}

/* set up the initial arrays of blots */
static void setupBlots (struct state *st)
{
    int which = RAND_FLOAT_01 * 11;

    freeBlots (st);

    switch (which)
    {
	case 0:
	    setupBlotsCube (st);
	    break;
	case 1:
	    setupBlotsSphere (st);
	    break;
	case 2:
	    setupBlotsCylinder (st);
	    break;
	case 3:
	    setupBlotsSquiggle (st);
	    break;
	case 4:
	    setupBlotsCubeCorners (st);
	    break;
	case 5:
	    setupBlotsTetrahedron (st);
	    break;
	case 6:
	    setupBlotsSheet (st);
	    break;
	case 7:
	    setupBlotsSwirlyCone (st);
	    break;
	case 8:
	case 9:
	case 10:
	    setupBlotsDuo (st);
	    break;
    }
}

/* set up the segments arrays */
static void setupSegs (struct state *st)
{
    /* there are blotShapeCount - 1 line segments per blot */
    st->segCount = st->blotCount * (blotShapeCount - 1);
    st->segsToErase = calloc (sizeof (LineSegment), st->segCount);
    st->segsToDraw = calloc (sizeof (LineSegment), st->segCount);
}



/*
 * color setup stuff
 */

/* set up the colormap */
static void setupColormap (struct state *st, XWindowAttributes *xgwa)
{
    int n;
    XGCValues gcv;
    XColor *colors = (XColor *) calloc (sizeof (XColor), st->colorCount + 1);

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
    
    colors[0].pixel = get_pixel_resource (st->dpy, xgwa->colormap,
                                          "background", "Background");
    
    make_color_ramp (xgwa->screen, xgwa->visual, xgwa->colormap,
                     h1, s1, v1, h2, s2, v2,
		     colors + 1, &st->colorCount, False, True, False);

    if (st->colorCount < 1)
    {
        fprintf (stderr, "%s: couldn't allocate any colors\n", progname);
	exit (-1);
    }
    
    st->gcs = (GC *) calloc (sizeof (GC), st->colorCount + 1);

    for (n = 0; n <= st->colorCount; n++) 
    {
	gcv.foreground = colors[n].pixel;
	gcv.line_width = st->lineWidth;
	st->gcs[n] = XCreateGC (st->dpy, st->window, GCForeground | GCLineWidth, &gcv);
    }

    free (colors);
}



/*
 * overall setup stuff
 */

/* set up the system */
static void setup (struct state *st)
{
    XWindowAttributes xgwa;

    XGetWindowAttributes (st->dpy, st->window, &xgwa);

    st->windowWidth = xgwa.width;
    st->windowHeight = xgwa.height;
    st->centerX = st->windowWidth / 2;
    st->centerY = st->windowHeight / 2;
    st->baseScale = (xgwa.height < xgwa.width) ? xgwa.height : xgwa.width;

    if (st->doubleBuffer)
    {
	st->drawable = XCreatePixmap (st->dpy, st->window, xgwa.width, xgwa.height,
				  xgwa.depth);
    }
    else
    {
	st->drawable = st->window;
    }

    setupColormap (st, &xgwa);
    setupBlots (st);
    setupSegs (st);

    /* set up the initial rotation, scale, and light values as random, but
     * with the targets equal to where it is */
    st->xRot = st->xRotTarget = RAND_FLOAT_01 * M_PI;
    st->yRot = st->yRotTarget = RAND_FLOAT_01 * M_PI;
    st->zRot = st->zRotTarget = RAND_FLOAT_01 * M_PI;
    st->curScale = st->scaleTarget = RAND_FLOAT_01 * (st->maxScale - st->minScale) + st->minScale;
    st->lightX = st->lightXTarget = RAND_FLOAT_PM1;
    st->lightY = st->lightYTarget = RAND_FLOAT_PM1;
    st->lightZ = st->lightZTarget = RAND_FLOAT_PM1;

    st->itersTillNext = RAND_FLOAT_01 * st->maxIters;
}



/*
 * the simulation
 */

/* "render" the blots into segsToDraw, with the current rotation factors */
static void renderSegs (struct state *st)
{
    int n;
    int m = 0;

    /* rotation factors */
    FLOAT sinX = sin (st->xRot);
    FLOAT cosX = cos (st->xRot);
    FLOAT sinY = sin (st->yRot);
    FLOAT cosY = cos (st->yRot);
    FLOAT sinZ = sin (st->zRot);
    FLOAT cosZ = cos (st->zRot);

    for (n = 0; n < st->blotCount; n++)
    {
	Blot *b = &st->blots[n];
	int i, j;
	int baseX, baseY;
	FLOAT radius;
	int x[3][3];
	int y[3][3];
	int color;

	FLOAT x1 = st->blots[n].x;
	FLOAT y1 = st->blots[n].y;
	FLOAT z1 = st->blots[n].z;
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
	x1 = x2 - st->lightX;
	y1 = y2 - st->lightY;
	z1 = z2 - st->lightZ;
	color = 1 + (x1 * x1 + y1 * y1 + z1 * z1) / 4 * st->colorCount;
	if (color > st->colorCount)
	{
	    color = st->colorCount;
	}

	/* set up the base screen coordinates for drawing */
	baseX = x2 / 2 * st->baseScale * st->curScale + st->centerX + st->centerXOff;
	baseY = y2 / 2 * st->baseScale * st->curScale + st->centerY + st->centerYOff;
	
	radius = (z2 + 1) / 2 * (st->maxRadius - st->minRadius) + st->minRadius;

	for (i = 0; i < 3; i++)
	{
	    for (j = 0; j < 3; j++)
	    {
		x[i][j] = baseX + 
		    ((i - 1) + (b->xoff[i][j] * st->maxNerveRadius)) * radius;
		y[i][j] = baseY + 
		    ((j - 1) + (b->yoff[i][j] * st->maxNerveRadius)) * radius;
	    }
	}

	for (i = 1; i < blotShapeCount; i++)
	{
	    st->segsToDraw[m].gc = st->gcs[color];
	    st->segsToDraw[m].x1 = x[blotShape[i-1].x + 1][blotShape[i-1].y + 1];
	    st->segsToDraw[m].y1 = y[blotShape[i-1].x + 1][blotShape[i-1].y + 1];
	    st->segsToDraw[m].x2 = x[blotShape[i].x   + 1][blotShape[i].y   + 1];
	    st->segsToDraw[m].y2 = y[blotShape[i].x   + 1][blotShape[i].y   + 1];
	    m++;
	}
    }
}

/* update blots, adjusting the offsets and rotation factors. */
static void updateWithFeeling (struct state *st)
{
    int n, i, j;

    /* pick a new model if the time is right */
    st->itersTillNext--;
    if (st->itersTillNext < 0)
    {
	st->itersTillNext = RAND_FLOAT_01 * st->maxIters;
	setupBlots (st);
	setupSegs (st);
	renderSegs (st);
    }

    /* update the rotation factors by moving them a bit toward the targets */
    st->xRot = st->xRot + (st->xRotTarget - st->xRot) * st->iterAmt; 
    st->yRot = st->yRot + (st->yRotTarget - st->yRot) * st->iterAmt;
    st->zRot = st->zRot + (st->zRotTarget - st->zRot) * st->iterAmt;

    /* similarly the scale factor */
    st->curScale = st->curScale + (st->scaleTarget - st->curScale) * st->iterAmt;

    /* and similarly the light position */
    st->lightX = st->lightX + (st->lightXTarget - st->lightX) * st->iterAmt; 
    st->lightY = st->lightY + (st->lightYTarget - st->lightY) * st->iterAmt; 
    st->lightZ = st->lightZ + (st->lightZTarget - st->lightZ) * st->iterAmt; 

    /* for each blot... */
    for (n = 0; n < st->blotCount; n++)
    {
	/* add a bit of random jitter to xoff/yoff */
	for (i = 0; i < 3; i++)
	{
	    for (j = 0; j < 3; j++)
	    {
		FLOAT newOff;

		newOff = st->blots[n].xoff[i][j] + RAND_FLOAT_PM1 * st->nervousness;
		if (newOff < -1) newOff = -(newOff + 1) - 1;
		else if (newOff > 1) newOff = -(newOff - 1) + 1;
		st->blots[n].xoff[i][j] = newOff;

		newOff = st->blots[n].yoff[i][j] + RAND_FLOAT_PM1 * st->nervousness;
		if (newOff < -1) newOff = -(newOff + 1) - 1;
		else if (newOff > 1) newOff = -(newOff - 1) + 1;
		st->blots[n].yoff[i][j] = newOff;
	    }
	}
    }

    /* depending on random chance, update one or more factors */
    if (RAND_FLOAT_01 <= st->eventChance)
    {
	int which = RAND_FLOAT_01 * 14;
	switch (which)
	{
	    case 0:
	    {
		st->xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 1:
	    {
		st->yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 2:
	    {
		st->zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 3:
	    {
		st->xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		st->yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 4:
	    {
		st->xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		st->zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 5:
	    {
		st->yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		st->zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 6:
	    {
		st->xRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		st->yRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		st->zRotTarget = RAND_FLOAT_PM1 * M_PI * 2;
		break;
	    }
	    case 7:
	    {
		st->centerXOff = RAND_FLOAT_PM1 * st->maxRadius;
		break;
	    }
	    case 8:
	    {
		st->centerYOff = RAND_FLOAT_PM1 * st->maxRadius;
		break;
	    }
	    case 9:
	    {
		st->centerXOff = RAND_FLOAT_PM1 * st->maxRadius;
		st->centerYOff = RAND_FLOAT_PM1 * st->maxRadius;
		break;
	    }
	    case 10:
	    {
		st->scaleTarget = 
		    RAND_FLOAT_01 * (st->maxScale - st->minScale) + st->minScale;
		break;
	    }
	    case 11:
	    {
		st->curScale = 
		    RAND_FLOAT_01 * (st->maxScale - st->minScale) + st->minScale;
		break;
	    }
	    case 12:
	    {
		st->lightX = RAND_FLOAT_PM1;
		st->lightY = RAND_FLOAT_PM1;
		st->lightZ = RAND_FLOAT_PM1;
		break;
	    }
	    case 13:
	    {
		st->lightXTarget = RAND_FLOAT_PM1;
		st->lightYTarget = RAND_FLOAT_PM1;
		st->lightZTarget = RAND_FLOAT_PM1;
		break;
	    }
	}
    }
}

/* erase segsToErase and draw segsToDraw */
static void eraseAndDraw (struct state *st)
{
    int n;

    if (st->doubleBuffer)
      XFillRectangle (st->dpy, st->drawable, st->gcs[0], 0, 0, 
                      st->windowWidth, st->windowHeight);
    else
      XClearWindow (st->dpy, st->drawable);

    for (n = 0; n < st->segCount; n++)
    {
	LineSegment *seg = &st->segsToErase[n];
#ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
	XDrawLine (st->dpy, st->drawable, st->gcs[0], 
		   seg->x1, seg->y1, seg->x2, seg->y2);
#endif
	seg = &st->segsToDraw[n];
	XDrawLine (st->dpy, st->drawable, seg->gc,
		   seg->x1, seg->y1, seg->x2, seg->y2);
    }

    if (st->doubleBuffer)
    {
	XCopyArea (st->dpy, st->drawable, st->window, st->gcs[0], 0, 0, 
		   st->windowWidth, st->windowHeight, 0, 0);
    }
}

/* do one iteration */
static unsigned long
nerverot_draw (Display *dpy, Window win, void *closure)
{
  struct state *st = (struct state *) closure;
    /* switch segsToErase and segsToDraw */
    LineSegment *temp = st->segsToDraw;
    st->segsToDraw = st->segsToErase;
    st->segsToErase = temp;

    /* update the model */
    updateWithFeeling (st);

    /* render new segments */
    renderSegs (st);

    /* erase old segments and draw new ones */
    eraseAndDraw (st);

    return st->delay;
}

/* initialize the user-specifiable params */
static void initParams (struct state *st)
{
    int problems = 0;

    st->delay = get_integer_resource (st->dpy, "delay", "Delay");
    if (st->delay < 0)
    {
	fprintf (stderr, "error: delay must be at least 0\n");
	problems = 1;
    }
    
    st->maxIters = get_integer_resource (st->dpy, "maxIters", "Integer");
    if (st->maxIters < 0)
    {
	fprintf (stderr, "error: maxIters must be at least 0\n");
	problems = 1;
    }
    
    st->doubleBuffer = get_boolean_resource (st->dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
    st->doubleBuffer = False;
# endif

    st->requestedBlotCount = get_integer_resource (st->dpy, "count", "Count");
    if (st->requestedBlotCount <= 0)
    {
	fprintf (stderr, "error: count must be at least 0\n");
	problems = 1;
    }

    st->colorCount = get_integer_resource (st->dpy, "colors", "Colors");
    if (st->colorCount <= 0)
    {
	fprintf (stderr, "error: colors must be at least 1\n");
	problems = 1;
    }

    st->lineWidth = get_integer_resource (st->dpy, "lineWidth", "LineWidth");
    if (st->lineWidth < 0)
    {
	fprintf (stderr, "error: line width must be at least 0\n");
	problems = 1;
    }

    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (st->dpy, st->window, &xgwa);
      if (st->lineWidth <= 0) st->lineWidth = 1;
      if (xgwa.width > 2560 || xgwa.height > 2560)
        st->lineWidth *= 3;  /* Retina displays */
    }

    st->nervousness = get_float_resource (st->dpy, "nervousness", "Float");
    if ((st->nervousness < 0) || (st->nervousness > 1))
    {
	fprintf (stderr, "error: nervousness must be in the range 0..1\n");
	problems = 1;
    }

    st->maxNerveRadius = get_float_resource (st->dpy, "maxNerveRadius", "Float");
    if ((st->maxNerveRadius < 0) || (st->maxNerveRadius > 1))
    {
	fprintf (stderr, "error: maxNerveRadius must be in the range 0..1\n");
	problems = 1;
    }

    st->eventChance = get_float_resource (st->dpy, "eventChance", "Float");
    if ((st->eventChance < 0) || (st->eventChance > 1))
    {
	fprintf (stderr, "error: eventChance must be in the range 0..1\n");
	problems = 1;
    }

    st->iterAmt = get_float_resource (st->dpy, "iterAmt", "Float");
    if ((st->iterAmt < 0) || (st->iterAmt > 1))
    {
	fprintf (stderr, "error: iterAmt must be in the range 0..1\n");
	problems = 1;
    }

    st->minScale = get_float_resource (st->dpy, "minScale", "Float");
    if ((st->minScale < 0) || (st->minScale > 10))
    {
	fprintf (stderr, "error: minScale must be in the range 0..10\n");
	problems = 1;
    }

    st->maxScale = get_float_resource (st->dpy, "maxScale", "Float");
    if ((st->maxScale < 0) || (st->maxScale > 10))
    {
	fprintf (stderr, "error: maxScale must be in the range 0..10\n");
	problems = 1;
    }

    if (st->maxScale < st->minScale)
    {
	fprintf (stderr, "error: maxScale must be >= minScale\n");
	problems = 1;
    }	

    st->minRadius = get_integer_resource (st->dpy, "minRadius", "Integer");
    if ((st->minRadius < 1) || (st->minRadius > 100))
    {
	fprintf (stderr, "error: minRadius must be in the range 1..100\n");
	problems = 1;
    }

    st->maxRadius = get_integer_resource (st->dpy, "maxRadius", "Integer");
    if ((st->maxRadius < 1) || (st->maxRadius > 100))
    {
	fprintf (stderr, "error: maxRadius must be in the range 1..100\n");
	problems = 1;
    }

    if (st->maxRadius < st->minRadius)
    {
	fprintf (stderr, "error: maxRadius must be >= minRadius\n");
	problems = 1;
    }	

    if (problems)
    {
	exit (1);
    }
}

static void *
nerverot_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;

    initParams (st);
    setup (st);

    /* make a valid set to erase at first */
    renderSegs (st);
    return st;
}

static void
nerverot_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
nerverot_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
nerverot_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  freeBlots (st);
  for (i = 0; i <= st->colorCount; i++) 
    XFreeGC (dpy, st->gcs[i]);
  free (st->gcs);
  free (st);
}


static const char *nerverot_defaults [] = {
    ".background:	black",
    ".foreground:	white",
    "*count:		250",
    "*colors:		4",
    "*delay:		10000",
    "*maxIters:		1200",
    "*doubleBuffer:	false",
    "*eventChance:      0.2",
    "*iterAmt:          0.01",
    "*lineWidth:        0",
    "*minScale:         0.6",
    "*maxScale:         1.75",
    "*minRadius:        3",
    "*maxRadius:        25",
    "*maxNerveRadius:	0.7",
    "*nervousness:	0.3",
#ifdef HAVE_MOBILE
    "*ignoreRotation:   True",
#endif
    0
};

static XrmOptionDescRec nerverot_options [] = {
  { "-count",            ".count",          XrmoptionSepArg, 0 },
  { "-colors",           ".colors",         XrmoptionSepArg, 0 },
  { "-delay",            ".delay",          XrmoptionSepArg, 0 },
  { "-max-iters",        ".maxIters",       XrmoptionSepArg, 0 },
  { "-db",               ".doubleBuffer",   XrmoptionNoArg,  "true" },
  { "-no-db",            ".doubleBuffer",   XrmoptionNoArg,  "false" },
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


XSCREENSAVER_MODULE ("NerveRot", nerverot)
