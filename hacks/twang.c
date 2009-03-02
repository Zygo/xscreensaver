/* twang, twist around screen bits, v1.3
 * by Dan Bornstein, danfuzz@milk.com
 * Copyright (c) 2003 Dan Bornstein. All rights reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * See the included man page for more details.
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif

#define FLOAT double

/* random float in the range (-1..1) */
#define RAND_FLOAT_PM1 \
        (((FLOAT) ((random() >> 8) & 0xffff)) / ((FLOAT) 0x10000) * 2 - 1)

/* random float in the range (0..1) */
#define RAND_FLOAT_01 \
        (((FLOAT) ((random() >> 8) & 0xffff)) / ((FLOAT) 0x10000))



/* parameters that are user configurable */

/* whether or not to use xshm */
#ifdef HAVE_XSHM_EXTENSION
static Bool useShm;
#endif

/* delay (usec) between iterations */
static int delay;

/* the maximum number of columns of tiles */
static int maxColumns;

/* the maximum number of rows of tiles */
static int maxRows;

/* the size (width and height) of a tile */
static int tileSize;

/* the width of the border around each tile */
static int borderWidth;

/* the chance, per iteration, of an interesting event happening */
static FLOAT eventChance;

/* friction: the fraction (0..1) by which velocity decreased per iteration */
static FLOAT friction;

/* springiness: the fraction (0..1) of the orientation that turns into 
 * velocity towards the center */
static FLOAT springiness;

/* transference: the fraction (0..1) of the orientations of orthogonal
 * neighbors that turns into velocity (in the same direction as the
 * orientation) */
static FLOAT transference;



/* non-user-modifiable immutable definitions */

/* width and height of the window */
static int windowWidth;
static int windowHeight;

static Display *display;        /* the display to draw on */
static Window window;           /* the window to draw on */
static Screen *screen;          /* the screen to draw on */

static XImage *sourceImage;     /* image source of stuff to draw */
static XImage *workImage;       /* work area image, used when rendering */
static XImage *backgroundImage; /* image filled with background pixels */

static GC backgroundGC;         /* GC for the background color */
static GC foregroundGC;         /* GC for the foreground color */
static GC borderGC;             /* GC for the border color */
unsigned long backgroundPixel;  /* background color as a pixel value */
unsigned long borderPixel;      /* border color as a pixel value */

#ifdef HAVE_XSHM_EXTENSION
XShmSegmentInfo shmInfo;
#endif



/* the model */

typedef struct
{
    int x;        /* x coordinate of the center of the tile */
    int y;        /* y coordinate of the center of the tile */
    FLOAT angle;  /* angle of the tile (-pi..pi) */
    FLOAT zoom;   /* log of the zoom of the tile (-1..1) */
    FLOAT vAngle; /* angular velocity (-pi/4..pi/4) */
    FLOAT vZoom;  /* zoomular velocity (-0.25..0.25) */
}
Tile;

static Tile *tiles;  /* array of tiles (left->right, top->bottom, row major) */
static int rows;     /* number of rows of tiles */
static int columns;  /* number of columns of tiles */

static Tile **sortedTiles; /* array of tile pointers, sorted by zoom */
static int tileCount;     /* total number of tiles */

#define TILE_AT(col,row) (&tiles[(row) * columns + (col)])

#define MAX_VANGLE (M_PI / 4.0)
#define MAX_VZOOM 0.25

#define RAND_ANGLE (RAND_FLOAT_PM1 * M_PI)
#define RAND_ZOOM (RAND_FLOAT_PM1)
#define RAND_VANGLE (RAND_FLOAT_PM1 * MAX_VANGLE)
#define RAND_VZOOM (RAND_FLOAT_PM1 * MAX_VZOOM)



/*
 * overall setup stuff
 */

/* grab the source image */
static void grabImage (XWindowAttributes *xwa)
{
    XFillRectangle (display, window, backgroundGC, 0, 0, 
		    windowWidth, windowHeight);
    backgroundImage = 
	XGetImage (display, window, 0, 0, windowWidth, windowHeight,
		   ~0L, ZPixmap);

    load_random_image (screen, window, window);
    sourceImage = XGetImage (display, window, 0, 0, windowWidth, windowHeight,
			     ~0L, ZPixmap);

#ifdef HAVE_XSHM_EXTENSION
    workImage = NULL;
    if (useShm) 
    {
	workImage = create_xshm_image (display, xwa->visual, xwa->depth,
				       ZPixmap, 0, &shmInfo, 
				       windowWidth, windowHeight);
	if (!workImage) 
	{
	    useShm = False;
	    fprintf (stderr, "create_xshm_image failed\n");
	}
    }

    if (workImage == NULL)
#endif /* HAVE_XSHM_EXTENSION */

	/* just use XSubImage to acquire the right visual, depth, etc;
	 * easier than the other alternatives */
	workImage = XSubImage (sourceImage, 0, 0, windowWidth, windowHeight);
}

/* set up the system */
static void setup (void)
{
    XWindowAttributes xgwa;
    XGCValues gcv;

    XGetWindowAttributes (display, window, &xgwa);

    screen = xgwa.screen;
    windowWidth = xgwa.width;
    windowHeight = xgwa.height;

    gcv.line_width = borderWidth;
    gcv.foreground = get_pixel_resource ("borderColor", "BorderColor",
					 display, xgwa.colormap);
    borderPixel = gcv.foreground;
    borderGC = XCreateGC (display, window, GCForeground | GCLineWidth, 
			  &gcv);

    gcv.foreground = get_pixel_resource ("background", "Background",
					 display, xgwa.colormap);
    backgroundPixel = gcv.foreground;
    backgroundGC = XCreateGC (display, window, GCForeground, &gcv);

    gcv.foreground = get_pixel_resource ("foreground", "Foreground",
					 display, xgwa.colormap);
    foregroundGC = XCreateGC (display, window, GCForeground, &gcv);

    grabImage (&xgwa);
}



/*
 * the simulation
 */

/* event: randomize all the angular velocities */
static void randomizeAllAngularVelocities (void)
{
    int c;
    int r;

    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    TILE_AT (c, r)->vAngle = RAND_VANGLE;
	}
    }
}

/* event: randomize all the zoomular velocities */
static void randomizeAllZoomularVelocities (void)
{
    int c;
    int r;

    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    TILE_AT (c, r)->vZoom = RAND_VZOOM;
	}
    }
}

/* event: randomize all the velocities */
static void randomizeAllVelocities (void)
{
    randomizeAllAngularVelocities ();
    randomizeAllZoomularVelocities ();
}

/* event: randomize all the angular orientations */
static void randomizeAllAngularOrientations (void)
{
    int c;
    int r;

    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    TILE_AT (c, r)->angle = RAND_ANGLE;
	}
    }
}

/* event: randomize all the zoomular orientations */
static void randomizeAllZoomularOrientations (void)
{
    int c;
    int r;

    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    TILE_AT (c, r)->zoom = RAND_ZOOM;
	}
    }
}

/* event: randomize all the orientations */
static void randomizeAllOrientations (void)
{
    randomizeAllAngularOrientations ();
    randomizeAllZoomularOrientations ();
}

/* event: randomize everything */
static void randomizeEverything (void)
{
    randomizeAllVelocities ();
    randomizeAllOrientations ();
}

/* event: pick one tile and randomize all its stats */
static void randomizeOneTile (void)
{
    int c = RAND_FLOAT_01 * columns;
    int r = RAND_FLOAT_01 * rows;

    Tile *t = TILE_AT (c, r);
    t->angle = RAND_ANGLE;
    t->zoom = RAND_ZOOM;
    t->vAngle = RAND_VANGLE;
    t->vZoom = RAND_VZOOM;
}

/* event: pick one row and randomize everything about each of its tiles */
static void randomizeOneRow (void)
{
    int c;
    int r = RAND_FLOAT_01 * rows;

    for (c = 0; c < columns; c++)
    {
	Tile *t = TILE_AT (c, r);
	t->angle = RAND_ANGLE;
	t->zoom = RAND_ZOOM;
	t->vAngle = RAND_VANGLE;
	t->vZoom = RAND_VZOOM;
    }
}

/* event: pick one column and randomize everything about each of its tiles */
static void randomizeOneColumn (void)
{
    int c = RAND_FLOAT_01 * columns;
    int r;

    for (r = 0; r < rows; r++)
    {
	Tile *t = TILE_AT (c, r);
	t->angle = RAND_ANGLE;
	t->zoom = RAND_ZOOM;
	t->vAngle = RAND_VANGLE;
	t->vZoom = RAND_VZOOM;
    }
}

/* do model event processing */
static void modelEvents (void)
{
    int which;

    if (RAND_FLOAT_01 > eventChance)
    {
	return;
    }

    which = RAND_FLOAT_01 * 10;

    switch (which)
    {
	case 0: randomizeAllAngularVelocities ();    break;
	case 1: randomizeAllZoomularVelocities ();   break;
	case 2: randomizeAllVelocities ();           break;
	case 3: randomizeAllAngularOrientations ();  break;
	case 4: randomizeAllZoomularOrientations (); break;
	case 5: randomizeAllOrientations ();         break;
	case 6: randomizeEverything ();              break;
	case 7: randomizeOneTile ();                 break;
	case 8: randomizeOneColumn ();               break;
	case 9: randomizeOneRow ();                  break;
    }
}

/* update the model for one iteration */
static void updateModel (void)
{
    int r;
    int c;

    /* for each tile, decrease its velocities according to the friction,
     * and increase them based on its current orientation and the orientations
     * of its orthogonal neighbors */
    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    Tile *t = TILE_AT (c, r);
	    FLOAT a = t->angle;
	    FLOAT z = t->zoom;
	    FLOAT va = t->vAngle;
	    FLOAT vz = t->vZoom;

	    va -= t->angle * springiness;
	    vz -= t->zoom * springiness;

	    if (c > 0)
	    {
		Tile *t2 = TILE_AT (c - 1, r);
		va += (t2->angle - a) * transference;
		vz += (t2->zoom - z) * transference;
	    }

	    if (c < (columns - 1))
	    {
		Tile *t2 = TILE_AT (c + 1, r);
		va += (t2->angle - a) * transference;
		vz += (t2->zoom - z) * transference;
	    }

	    if (r > 0)
	    {
		Tile *t2 = TILE_AT (c, r - 1);
		va += (t2->angle - a) * transference;
		vz += (t2->zoom - z) * transference;
	    }

	    if (r < (rows - 1))
	    {
		Tile *t2 = TILE_AT (c, r + 1);
		va += (t2->angle - a) * transference;
		vz += (t2->zoom - z) * transference;
	    }

	    va *= (1.0 - friction);
	    vz *= (1.0 - friction);

	    if (va > MAX_VANGLE) va = MAX_VANGLE;
	    else if (va < -MAX_VANGLE) va = -MAX_VANGLE;
	    t->vAngle = va;

	    if (vz > MAX_VZOOM) vz = MAX_VZOOM;
	    else if (vz < -MAX_VZOOM) vz = -MAX_VZOOM;
	    t->vZoom = vz;
	}
    }

    /* for each tile, update its orientation based on its velocities */
    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    Tile *t = TILE_AT (c, r);
	    FLOAT a = t->angle + t->vAngle;
	    FLOAT z = t->zoom + t->vZoom;

	    if (a > M_PI) a = M_PI;
	    else if (a < -M_PI) a = -M_PI;
	    t->angle = a;

	    if (z > 1.0) z = 1.0;
	    else if (z < -1.0) z = -1.0;
	    t->zoom = z;
	}
    }
}

/* the comparator to us to sort the tiles (used immediately below); it'd
 * sure be nice if C allowed inner functions (or jeebus-forbid *real
 * closures*!) */
static int sortTilesComparator (const void *v1, const void *v2)
{
    Tile *t1 = *(Tile **) v1;
    Tile *t2 = *(Tile **) v2;
    
    if (t1->zoom < t2->zoom)
    {
	return -1;
    }

    if (t1->zoom > t2->zoom)
    {
	return 1;
    }

    return 0;
}

/* sort the tiles in sortedTiles by zoom */
static void sortTiles (void)
{
    qsort (sortedTiles, tileCount, sizeof (Tile *), sortTilesComparator);
}

/* render the given tile */
static void renderTile (Tile *t)
{
    /* note: the zoom as stored per tile is log-based (centered on 0, with
     * 0 being no zoom, but the range for zoom-as-drawn is 0.4..2.5,
     * hence the alteration of t->zoom, below */

    int x, y;

    int tx = t->x;
    int ty = t->y;

    FLOAT zoom = pow (2.5, t->zoom);
    FLOAT ang = -t->angle;
    FLOAT sinAng = sin (ang);
    FLOAT cosAng = cos (ang);

    FLOAT innerBorder = (tileSize - borderWidth) / 2.0;
    FLOAT outerBorder = innerBorder + borderWidth;

    int maxCoord = outerBorder * zoom * (fabs (sinAng) + fabs (cosAng));
    int minX = tx - maxCoord;
    int maxX = tx + maxCoord;
    int minY = ty - maxCoord;
    int maxY = ty + maxCoord;

    FLOAT prey;

    if (minX < 0) minX = 0;
    if (maxX > windowWidth) maxX = windowWidth;
    if (minY < 0) minY = 0;
    if (maxY > windowHeight) maxY = windowHeight;

    sinAng /= zoom;
    cosAng /= zoom;

    for (y = minY, prey = y - ty; y < maxY; y++, prey++)
    {
	FLOAT prex = minX - tx;
	FLOAT srcx = prex * cosAng - prey * sinAng;
	FLOAT srcy = prex * sinAng + prey * cosAng;

	for (x = minX; 
	     x < maxX; 
	     x++, srcx += cosAng, srcy += sinAng)
	{
	    if ((srcx < -innerBorder) || (srcx >= innerBorder) ||
		(srcy < -innerBorder) || (srcy >= innerBorder))
	    {
		if ((srcx < -outerBorder) || (srcx >= outerBorder) ||
		    (srcy < -outerBorder) || (srcy >= outerBorder))
		{
		    continue;
		}
		XPutPixel (workImage, x, y, borderPixel);
	    }
	    else
	    {
		unsigned long p = 
		    XGetPixel (sourceImage, srcx + tx, srcy + ty);
		XPutPixel (workImage, x, y, p);
	    }
	}
    }
}

/* render and display the current model */
static void renderFrame (void)
{
    int n;

    memcpy (workImage->data, backgroundImage->data, 
	    workImage->bytes_per_line * workImage->height);

    sortTiles ();

    for (n = 0; n < tileCount; n++)
    {
	renderTile (sortedTiles[n]);
    }

#ifdef HAVE_XSHM_EXTENSION
    if (useShm)
	XShmPutImage (display, window, backgroundGC, workImage, 0, 0, 0, 0,
		      windowWidth, windowHeight, False);
    else
#endif /* HAVE_XSHM_EXTENSION */
	XPutImage (display, window, backgroundGC, workImage, 
		   0, 0, 0, 0, windowWidth, windowHeight);
}

/* set up the model */
static void setupModel (void)
{
    int c;
    int r;

    int leftX; /* x of the center of the top-left tile */
    int topY;  /* y of the center of the top-left tile */

    if (tileSize > (windowWidth / 2))
    {
	tileSize = windowWidth / 2;
    }

    if (tileSize > (windowHeight / 2))
    {
	tileSize = windowHeight / 2;
    }

    columns = windowWidth / tileSize;
    rows = windowHeight / tileSize;

    if ((maxColumns != 0) && (columns > maxColumns))
    {
	columns = maxColumns;
    }

    if ((maxRows != 0) && (rows > maxRows))
    {
	rows = maxRows;
    }

    tileCount = rows * columns;

    leftX = (windowWidth - (columns * tileSize) + tileSize) / 2;
    topY = (windowHeight - (rows * tileSize) + tileSize) / 2;

    tiles = calloc (tileCount, sizeof (Tile));
    sortedTiles = calloc (tileCount, sizeof (Tile *));

    for (r = 0; r < rows; r++)
    {
	for (c = 0; c < columns; c++)
	{
	    Tile *t = TILE_AT (c, r);
	    t->x = leftX + c * tileSize;
	    t->y = topY + r * tileSize;
	    sortedTiles[c + r * columns] = t;
	}
    }

    randomizeEverything ();
}

/* do one iteration */
static void oneIteration (void)
{
    modelEvents ();
    updateModel ();
    renderFrame ();
}



/* main and options and stuff */

char *progclass = "Twang";

char *defaults [] = {
    ".background:	black",
    ".foreground:	white",
    "*borderColor:      blue",
    "*borderWidth:	3",
    "*delay:		10000",
    "*eventChance:      0.01",
    "*friction:		0.05",
    "*maxColumns:       0",
    "*maxRows:          0",
    "*springiness:	0.1",
    "*tileSize:		120",
    "*transference:	0.025",
#ifdef HAVE_XSHM_EXTENSION
    "*useSHM: True",
#endif
    0
};

XrmOptionDescRec options [] = {
  { "-border-color",     ".borderColor",    XrmoptionSepArg, 0 },
  { "-border-width",     ".borderWidth",    XrmoptionSepArg, 0 },
  { "-delay",            ".delay",          XrmoptionSepArg, 0 },
  { "-event-chance",     ".eventChance",    XrmoptionSepArg, 0 },
  { "-friction",         ".friction",       XrmoptionSepArg, 0 },
  { "-max-columns",      ".maxColumns",     XrmoptionSepArg, 0 },
  { "-max-rows",         ".maxRows",        XrmoptionSepArg, 0 },
  { "-springiness",      ".springiness",    XrmoptionSepArg, 0 },
  { "-tile-size",        ".tileSize",       XrmoptionSepArg, 0 },
  { "-transference",     ".transference",   XrmoptionSepArg, 0 },
#ifdef HAVE_XSHM_EXTENSION
  { "-shm",              ".useSHM",         XrmoptionNoArg, "True" },
  { "-no-shm",           ".useSHM",         XrmoptionNoArg, "False" },
#endif
  { 0, 0, 0, 0 }
};

/* initialize the user-specifiable params */
static void initParams (void)
{
    int problems = 0;

    borderWidth = get_integer_resource ("borderWidth", "Integer");
    if (borderWidth < 0)
    {
	fprintf (stderr, "error: border width must be at least 0\n");
	problems = 1;
    }

    delay = get_integer_resource ("delay", "Delay");
    if (delay < 0)
    {
	fprintf (stderr, "error: delay must be at least 0\n");
	problems = 1;
    }

    eventChance = get_float_resource ("eventChance", "Double");
    if ((eventChance < 0.0) || (eventChance > 1.0))
    {
	fprintf (stderr, "error: eventChance must be in the range 0..1\n");
	problems = 1;
    }

    friction = get_float_resource ("friction", "Double");
    if ((friction < 0.0) || (friction > 1.0))
    {
	fprintf (stderr, "error: friction must be in the range 0..1\n");
	problems = 1;
    }

    maxColumns = get_integer_resource ("maxColumns", "Integer");
    if (maxColumns < 0)
    {
	fprintf (stderr, "error: max columns must be at least 0\n");
	problems = 1;
    }

    maxRows = get_integer_resource ("maxRows", "Integer");
    if (maxRows < 0)
    {
	fprintf (stderr, "error: max rows must be at least 0\n");
	problems = 1;
    }

    springiness = get_float_resource ("springiness", "Double");
    if ((springiness < 0.0) || (springiness > 1.0))
    {
	fprintf (stderr, "error: springiness must be in the range 0..1\n");
	problems = 1;
    }

    tileSize = get_integer_resource ("tileSize", "Integer");
    if (tileSize < 1)
    {
	fprintf (stderr, "error: tile size must be at least 1\n");
	problems = 1;
    }
    
    transference = get_float_resource ("transference", "Double");
    if ((transference < 0.0) || (transference > 1.0))
    {
	fprintf (stderr, "error: transference must be in the range 0..1\n");
	problems = 1;
    }

#ifdef HAVE_XSHM_EXTENSION
    useShm = get_boolean_resource ("useSHM", "Boolean");
#endif

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
    setupModel ();

    for (;;) 
    {
	oneIteration ();
        XSync (dpy, False);
        screenhack_handle_events (dpy);
	usleep (delay);
    }
}
