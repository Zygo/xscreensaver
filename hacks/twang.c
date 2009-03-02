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


struct state {
  Display *dpy;
  Window window;

  int delay;			/* delay (usec) between iterations */
  int maxColumns;		/* the maximum number of columns of tiles */
  int maxRows;			/* the maximum number of rows of tiles */
  int tileSize;			/* the size (width and height) of a tile */
  int borderWidth;		/* the width of the border around each tile */
  FLOAT eventChance;		/* the chance, per iteration, of an interesting event happening */
  FLOAT friction;		/* friction: the fraction (0..1) by which velocity decreased per iteration */
  FLOAT springiness;		/* springiness: the fraction (0..1) of the orientation that turns into velocity towards the center */
  FLOAT transference;		/* transference: the fraction (0..1) of the orientations of orthogonal neighbors that turns into velocity (in the same direction as the orientation) */
  int windowWidth;		/* width and height of the window */
  int windowHeight;
  Screen *screen;       	   /* the screen to draw on */
  XImage *sourceImage;  	   /* image source of stuff to draw */
  XImage *workImage;    	   /* work area image, used when rendering */
  XImage *backgroundImage;	 /* image filled with background pixels */

  GC backgroundGC;        	 /* GC for the background color */
  GC foregroundGC;        	 /* GC for the foreground color */
  GC borderGC;            	 /* GC for the border color */
  unsigned long backgroundPixel;  /* background color as a pixel value */
  unsigned long borderPixel;      /* border color as a pixel value */

  Tile *tiles;  /* array of tiles (left->right, top->bottom, row major) */
  int rows;     /* number of rows of tiles */
  int columns;  /* number of columns of tiles */

  Tile **sortedTiles; /* array of tile pointers, sorted by zoom */
  int tileCount;     /* total number of tiles */

  async_load_state *img_loader;

  Bool useShm;		/* whether or not to use xshm */
#ifdef HAVE_XSHM_EXTENSION
  XShmSegmentInfo shmInfo;
#endif
};


#define TILE_AT(col,row) (&st->tiles[(row) * st->columns + (col)])

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
static void grabImage_start (struct state *st, XWindowAttributes *xwa)
{
    XFillRectangle (st->dpy, st->window, st->backgroundGC, 0, 0, 
		    st->windowWidth, st->windowHeight);
    st->backgroundImage = 
	XGetImage (st->dpy, st->window, 0, 0, st->windowWidth, st->windowHeight,
		   ~0L, ZPixmap);

    st->img_loader = load_image_async_simple (0, xwa->screen, st->window,
                                              st->window, 0, 0);
}

static void grabImage_done (struct state *st)
{
   XWindowAttributes xwa;
   XGetWindowAttributes (st->dpy, st->window, &xwa);

    st->sourceImage = XGetImage (st->dpy, st->window, 0, 0, st->windowWidth, st->windowHeight,
			     ~0L, ZPixmap);

#ifdef HAVE_XSHM_EXTENSION
    st->workImage = NULL;
    if (st->useShm) 
    {
	st->workImage = create_xshm_image (st->dpy, xwa.visual, xwa.depth,
				       ZPixmap, 0, &st->shmInfo, 
				       st->windowWidth, st->windowHeight);
	if (!st->workImage) 
	{
	    st->useShm = False;
	    fprintf (stderr, "create_xshm_image failed\n");
	}
    }

    if (st->workImage == NULL)
#endif /* HAVE_XSHM_EXTENSION */

	/* just use XSubImage to acquire the right visual, depth, etc;
	 * easier than the other alternatives */
	st->workImage = XSubImage (st->sourceImage, 0, 0, st->windowWidth, st->windowHeight);
}

/* set up the system */
static void setup (struct state *st)
{
    XWindowAttributes xgwa;
    XGCValues gcv;

    XGetWindowAttributes (st->dpy, st->window, &xgwa);

    st->screen = xgwa.screen;
    st->windowWidth = xgwa.width;
    st->windowHeight = xgwa.height;

    gcv.line_width = st->borderWidth;
    gcv.foreground = get_pixel_resource (st->dpy, xgwa.colormap,
                                         "borderColor", "BorderColor");
    st->borderPixel = gcv.foreground;
    st->borderGC = XCreateGC (st->dpy, st->window, GCForeground | GCLineWidth, 
			  &gcv);

    gcv.foreground = get_pixel_resource (st->dpy, xgwa.colormap,
                                         "background", "Background");
    st->backgroundPixel = gcv.foreground;
    st->backgroundGC = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

    gcv.foreground = get_pixel_resource (st->dpy, xgwa.colormap,
                                         "foreground", "Foreground");
    st->foregroundGC = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

    grabImage_start (st, &xgwa);
}



/*
 * the simulation
 */

/* event: randomize all the angular velocities */
static void randomizeAllAngularVelocities (struct state *st)
{
    int c;
    int r;

    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
	{
	    TILE_AT (c, r)->vAngle = RAND_VANGLE;
	}
    }
}

/* event: randomize all the zoomular velocities */
static void randomizeAllZoomularVelocities (struct state *st)
{
    int c;
    int r;

    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
	{
	    TILE_AT (c, r)->vZoom = RAND_VZOOM;
	}
    }
}

/* event: randomize all the velocities */
static void randomizeAllVelocities (struct state *st)
{
    randomizeAllAngularVelocities (st);
    randomizeAllZoomularVelocities (st);
}

/* event: randomize all the angular orientations */
static void randomizeAllAngularOrientations (struct state *st)
{
    int c;
    int r;

    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
	{
	    TILE_AT (c, r)->angle = RAND_ANGLE;
	}
    }
}

/* event: randomize all the zoomular orientations */
static void randomizeAllZoomularOrientations (struct state *st)
{
    int c;
    int r;

    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
	{
	    TILE_AT (c, r)->zoom = RAND_ZOOM;
	}
    }
}

/* event: randomize all the orientations */
static void randomizeAllOrientations (struct state *st)
{
    randomizeAllAngularOrientations (st);
    randomizeAllZoomularOrientations (st);
}

/* event: randomize everything */
static void randomizeEverything (struct state *st)
{
    randomizeAllVelocities (st);
    randomizeAllOrientations (st);
}

/* event: pick one tile and randomize all its stats */
static void randomizeOneTile (struct state *st)
{
    int c = RAND_FLOAT_01 * st->columns;
    int r = RAND_FLOAT_01 * st->rows;

    Tile *t = TILE_AT (c, r);
    t->angle = RAND_ANGLE;
    t->zoom = RAND_ZOOM;
    t->vAngle = RAND_VANGLE;
    t->vZoom = RAND_VZOOM;
}

/* event: pick one row and randomize everything about each of its tiles */
static void randomizeOneRow (struct state *st)
{
    int c;
    int r = RAND_FLOAT_01 * st->rows;

    for (c = 0; c < st->columns; c++)
    {
	Tile *t = TILE_AT (c, r);
	t->angle = RAND_ANGLE;
	t->zoom = RAND_ZOOM;
	t->vAngle = RAND_VANGLE;
	t->vZoom = RAND_VZOOM;
    }
}

/* event: pick one column and randomize everything about each of its tiles */
static void randomizeOneColumn (struct state *st)
{
    int c = RAND_FLOAT_01 * st->columns;
    int r;

    for (r = 0; r < st->rows; r++)
    {
	Tile *t = TILE_AT (c, r);
	t->angle = RAND_ANGLE;
	t->zoom = RAND_ZOOM;
	t->vAngle = RAND_VANGLE;
	t->vZoom = RAND_VZOOM;
    }
}

/* do model event processing */
static void modelEvents (struct state *st)
{
    int which;

    if (RAND_FLOAT_01 > st->eventChance)
    {
	return;
    }

    which = RAND_FLOAT_01 * 10;

    switch (which)
    {
	case 0: randomizeAllAngularVelocities (st);    break;
	case 1: randomizeAllZoomularVelocities (st);   break;
	case 2: randomizeAllVelocities (st);           break;
	case 3: randomizeAllAngularOrientations (st);  break;
	case 4: randomizeAllZoomularOrientations (st); break;
	case 5: randomizeAllOrientations (st);         break;
	case 6: randomizeEverything (st);              break;
	case 7: randomizeOneTile (st);                 break;
	case 8: randomizeOneColumn (st);               break;
	case 9: randomizeOneRow (st);                  break;
    }
}

/* update the model for one iteration */
static void updateModel (struct state *st)
{
    int r;
    int c;

    /* for each tile, decrease its velocities according to the friction,
     * and increase them based on its current orientation and the orientations
     * of its orthogonal neighbors */
    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
	{
	    Tile *t = TILE_AT (c, r);
	    FLOAT a = t->angle;
	    FLOAT z = t->zoom;
	    FLOAT va = t->vAngle;
	    FLOAT vz = t->vZoom;

	    va -= t->angle * st->springiness;
	    vz -= t->zoom * st->springiness;

	    if (c > 0)
	    {
		Tile *t2 = TILE_AT (c - 1, r);
		va += (t2->angle - a) * st->transference;
		vz += (t2->zoom - z) * st->transference;
	    }

	    if (c < (st->columns - 1))
	    {
		Tile *t2 = TILE_AT (c + 1, r);
		va += (t2->angle - a) * st->transference;
		vz += (t2->zoom - z) * st->transference;
	    }

	    if (r > 0)
	    {
		Tile *t2 = TILE_AT (c, r - 1);
		va += (t2->angle - a) * st->transference;
		vz += (t2->zoom - z) * st->transference;
	    }

	    if (r < (st->rows - 1))
	    {
		Tile *t2 = TILE_AT (c, r + 1);
		va += (t2->angle - a) * st->transference;
		vz += (t2->zoom - z) * st->transference;
	    }

	    va *= (1.0 - st->friction);
	    vz *= (1.0 - st->friction);

	    if (va > MAX_VANGLE) va = MAX_VANGLE;
	    else if (va < -MAX_VANGLE) va = -MAX_VANGLE;
	    t->vAngle = va;

	    if (vz > MAX_VZOOM) vz = MAX_VZOOM;
	    else if (vz < -MAX_VZOOM) vz = -MAX_VZOOM;
	    t->vZoom = vz;
	}
    }

    /* for each tile, update its orientation based on its velocities */
    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
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
static void sortTiles (struct state *st)
{
    qsort (st->sortedTiles, st->tileCount, sizeof (Tile *), sortTilesComparator);
}

/* render the given tile */
static void renderTile (struct state *st, Tile *t)
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

    FLOAT innerBorder = (st->tileSize - st->borderWidth) / 2.0;
    FLOAT outerBorder = innerBorder + st->borderWidth;

    int maxCoord = outerBorder * zoom * (fabs (sinAng) + fabs (cosAng));
    int minX = tx - maxCoord;
    int maxX = tx + maxCoord;
    int minY = ty - maxCoord;
    int maxY = ty + maxCoord;

    FLOAT prey;

    if (minX < 0) minX = 0;
    if (maxX > st->windowWidth) maxX = st->windowWidth;
    if (minY < 0) minY = 0;
    if (maxY > st->windowHeight) maxY = st->windowHeight;

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
		XPutPixel (st->workImage, x, y, st->borderPixel);
	    }
	    else
	    {
		unsigned long p = 
		    XGetPixel (st->sourceImage, srcx + tx, srcy + ty);
		XPutPixel (st->workImage, x, y, p);
	    }
	}
    }
}

/* render and display the current model */
static void renderFrame (struct state *st)
{
    int n;

    memcpy (st->workImage->data, st->backgroundImage->data, 
	    st->workImage->bytes_per_line * st->workImage->height);

    sortTiles (st);

    for (n = 0; n < st->tileCount; n++)
    {
	renderTile (st, st->sortedTiles[n]);
    }

#ifdef HAVE_XSHM_EXTENSION
    if (st->useShm)
	XShmPutImage (st->dpy, st->window, st->backgroundGC, st->workImage, 0, 0, 0, 0,
		      st->windowWidth, st->windowHeight, False);
    else
#endif /* HAVE_XSHM_EXTENSION */
	XPutImage (st->dpy, st->window, st->backgroundGC, st->workImage, 
		   0, 0, 0, 0, st->windowWidth, st->windowHeight);
}

/* set up the model */
static void setupModel (struct state *st)
{
    int c;
    int r;

    int leftX; /* x of the center of the top-left tile */
    int topY;  /* y of the center of the top-left tile */

    if (st->tileSize > (st->windowWidth / 2))
    {
	st->tileSize = st->windowWidth / 2;
    }

    if (st->tileSize > (st->windowHeight / 2))
    {
	st->tileSize = st->windowHeight / 2;
    }

    st->columns = st->windowWidth / st->tileSize;
    st->rows = st->windowHeight / st->tileSize;

    if ((st->maxColumns != 0) && (st->columns > st->maxColumns))
    {
	st->columns = st->maxColumns;
    }

    if ((st->maxRows != 0) && (st->rows > st->maxRows))
    {
	st->rows = st->maxRows;
    }

    st->tileCount = st->rows * st->columns;

    leftX = (st->windowWidth - (st->columns * st->tileSize) + st->tileSize) / 2;
    topY = (st->windowHeight - (st->rows * st->tileSize) + st->tileSize) / 2;

    st->tiles = calloc (st->tileCount, sizeof (Tile));
    st->sortedTiles = calloc (st->tileCount, sizeof (Tile *));

    for (r = 0; r < st->rows; r++)
    {
	for (c = 0; c < st->columns; c++)
	{
	    Tile *t = TILE_AT (c, r);
	    t->x = leftX + c * st->tileSize;
	    t->y = topY + r * st->tileSize;
	    st->sortedTiles[c + r * st->columns] = t;
	}
    }

    randomizeEverything (st);
}

/* do one iteration */

static unsigned long
twang_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
        grabImage_done (st);
      }
      return st->delay;
    }


  modelEvents (st);
  updateModel (st);
  renderFrame (st);
  return st->delay;
}


static void
twang_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
twang_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
twang_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}



/* main and options and stuff */

/* initialize the user-specifiable params */
static void initParams (struct state *st)
{
    int problems = 0;

    st->borderWidth = get_integer_resource (st->dpy, "borderWidth", "Integer");
    if (st->borderWidth < 0)
    {
	fprintf (stderr, "error: border width must be at least 0\n");
	problems = 1;
    }

    st->delay = get_integer_resource (st->dpy, "delay", "Delay");
    if (st->delay < 0)
    {
	fprintf (stderr, "error: delay must be at least 0\n");
	problems = 1;
    }

    st->eventChance = get_float_resource (st->dpy, "eventChance", "Double");
    if ((st->eventChance < 0.0) || (st->eventChance > 1.0))
    {
	fprintf (stderr, "error: eventChance must be in the range 0..1\n");
	problems = 1;
    }

    st->friction = get_float_resource (st->dpy, "friction", "Double");
    if ((st->friction < 0.0) || (st->friction > 1.0))
    {
	fprintf (stderr, "error: friction must be in the range 0..1\n");
	problems = 1;
    }

    st->maxColumns = get_integer_resource (st->dpy, "maxColumns", "Integer");
    if (st->maxColumns < 0)
    {
	fprintf (stderr, "error: max columns must be at least 0\n");
	problems = 1;
    }

    st->maxRows = get_integer_resource (st->dpy, "maxRows", "Integer");
    if (st->maxRows < 0)
    {
	fprintf (stderr, "error: max rows must be at least 0\n");
	problems = 1;
    }

    st->springiness = get_float_resource (st->dpy, "springiness", "Double");
    if ((st->springiness < 0.0) || (st->springiness > 1.0))
    {
	fprintf (stderr, "error: springiness must be in the range 0..1\n");
	problems = 1;
    }

    st->tileSize = get_integer_resource (st->dpy, "tileSize", "Integer");
    if (st->tileSize < 1)
    {
	fprintf (stderr, "error: tile size must be at least 1\n");
	problems = 1;
    }
    
    st->transference = get_float_resource (st->dpy, "transference", "Double");
    if ((st->transference < 0.0) || (st->transference > 1.0))
    {
	fprintf (stderr, "error: transference must be in the range 0..1\n");
	problems = 1;
    }

#ifdef HAVE_XSHM_EXTENSION
    st->useShm = get_boolean_resource (st->dpy, "useSHM", "Boolean");
#endif

    if (problems)
    {
	exit (1);
    }
}

static void *
twang_init (Display *dpy, Window win)
{
    struct state *st = (struct state *) calloc (1, sizeof(*st));
    st->dpy = dpy;
    st->window = win;

    initParams (st);
    setup (st);
    setupModel (st);

    return st;
}


static const char *twang_defaults [] = {
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
#else
    "*useSHM: False",
#endif
    0
};

static XrmOptionDescRec twang_options [] = {
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
  { "-shm",              ".useSHM",         XrmoptionNoArg, "True" },
  { "-no-shm",           ".useSHM",         XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Twang", twang)
