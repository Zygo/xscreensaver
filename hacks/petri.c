/* petri, simulate mold in a petri dish. v2.6
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
 */

#include <math.h>
#include "screenhack.h"
#include "spline.h"

#define FLOAT float
#define RAND_FLOAT (((FLOAT) (random() & 0xffff)) / ((FLOAT) 0x10000))

typedef struct cell_s 
{
    short x;                        /*  0    - */
    short y;                        /*  2      */
    unsigned char col;              /*  4      */
    unsigned char isnext;           /*  5      */
    unsigned char nextcol;          /*  6      */
                                    /*  7      */
    struct cell_s *next;            /*  8    - */
    struct cell_s *prev;            /* 12      */
    FLOAT speed;                    /* 16    - */
    FLOAT growth;                   /* 20 24   */
    FLOAT nextspeed;                /* 24 32 - */
                                    /* 28 40   */
} cell;

static int arr_width;
static int arr_height;
static int count;

static cell *arr;
static cell *head;
static cell *tail;
static int blastcount;

static Display *display;
static Window window;
static GC *coloredGCs;

static int windowWidth;
static int windowHeight;
static int xOffset;
static int yOffset;
static int xSize;
static int ySize;

static FLOAT orthlim = 1.0;
static FLOAT diaglim;
static FLOAT anychan;
static FLOAT minorchan;
static FLOAT instantdeathchan;
static int minlifespan;
static int maxlifespan;
static FLOAT minlifespeed;
static FLOAT maxlifespeed;
static FLOAT mindeathspeed;
static FLOAT maxdeathspeed;
static Bool originalcolors;

static int random_life_value (void)
{
    return (int) ((RAND_FLOAT * (maxlifespan - minlifespan)) + minlifespan);
}

static void setup_random_colormap (XWindowAttributes *xgwa)
{
    XGCValues gcv;
    int lose = 0;
    int ncolors = count - 1;
    int n;
    XColor *colors = (XColor *) calloc (sizeof(*colors), count*2);
    
    colors[0].pixel = get_pixel_resource ("background", "Background",
					  display, xgwa->colormap);
    
    make_random_colormap (display, xgwa->visual, xgwa->colormap,
			  colors+1, &ncolors, True, True, 0, True);
    if (ncolors < 1)
      {
        fprintf (stderr, "%s: couldn't allocate any colors\n", progname);
	exit (-1);
      }
    
    ncolors++;
    count = ncolors;
    
    memcpy (colors + count, colors, count * sizeof(*colors));
    colors[count].pixel = get_pixel_resource ("foreground", "Foreground",
					      display, xgwa->colormap);
    
    for (n = 1; n < count; n++)
    {
	int m = n + count;
	colors[n].red = colors[m].red / 2;
	colors[n].green = colors[m].green / 2;
	colors[n].blue = colors[m].blue / 2;
	
	if (!XAllocColor (display, xgwa->colormap, &colors[n]))
	{
	    lose++;
	    colors[n] = colors[m];
	}
    }

    if (lose)
    {
	fprintf (stderr, 
		 "%s: unable to allocate %d half-intensity colors.\n",
		 progname, lose);
    }
    
    for (n = 0; n < count*2; n++) 
    {
	gcv.foreground = colors[n].pixel;
	coloredGCs[n] = XCreateGC (display, window, GCForeground, &gcv);
    }

    free (colors);
}

static void setup_original_colormap (XWindowAttributes *xgwa)
{
    XGCValues gcv;
    int lose = 0;
    int n;
    XColor *colors = (XColor *) calloc (sizeof(*colors), count*2);
    
    colors[0].pixel = get_pixel_resource ("background", "Background",
					  display, xgwa->colormap);

    colors[count].pixel = get_pixel_resource ("foreground", "Foreground",
					      display, xgwa->colormap);

    for (n = 1; n < count; n++)
    {
	int m = n + count;
	colors[n].red =   ((n & 0x01) != 0) * 0x8000;
	colors[n].green = ((n & 0x02) != 0) * 0x8000;
	colors[n].blue =  ((n & 0x04) != 0) * 0x8000;

	if (!XAllocColor (display, xgwa->colormap, &colors[n]))
	{
	    lose++;
	    colors[n] = colors[0];
	}

	colors[m].red   = colors[n].red + 0x4000;
	colors[m].green = colors[n].green + 0x4000;
	colors[m].blue  = colors[n].blue + 0x4000;

	if (!XAllocColor (display, xgwa->colormap, &colors[m]))
	{
	    lose++;
	    colors[m] = colors[count];
	}
    }

    if (lose)
    {
	fprintf (stderr, 
		 "%s: unable to allocate %d colors.\n",
		 progname, lose);
    }
    
    for (n = 0; n < count*2; n++) 
    {
	gcv.foreground = colors[n].pixel;
	coloredGCs[n] = XCreateGC (display, window, GCForeground, &gcv);
    }

    free (colors);
}

static void setup_display (void)
{
    XWindowAttributes xgwa;
    Colormap cmap;

    int cell_size = get_integer_resource ("size", "Integer");
    int osize, alloc_size, oalloc;
    int mem_throttle = 0;
    char *s;

    if (cell_size < 1) cell_size = 1;

    osize = cell_size;

    s = get_string_resource ("memThrottle", "MemThrottle");
    if (s)
      {
        int n;
        char c;
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
            fprintf (stderr, "%s: invalid memThrottle \"%s\" (try \"10M\")\n",
                     progname, s);
            exit (1);
          }
        
        free (s);
      }

    XGetWindowAttributes (display, window, &xgwa);

    originalcolors = get_boolean_resource ("originalcolors", "Boolean");

    count = get_integer_resource ("count", "Integer");
    if (count < 2) count = 2;
    if (count > (1L << (xgwa.depth-1)))
      count = (1L << (xgwa.depth-1));

    if (originalcolors && (count > 8))
    {
	count = 8;
    }

    coloredGCs = (GC *) calloc (sizeof(GC), count * 2);

    diaglim  = get_float_resource ("diaglim", "Float");
    if (diaglim < 1.0)
    {
	diaglim = 1.0;
    }
    else if (diaglim > 2.0)
    {
	diaglim = 2.0;
    }
    diaglim *= orthlim;

    anychan  = get_float_resource ("anychan", "Float");
    if (anychan < 0.0)
    {
	anychan = 0.0;
    }
    else if (anychan > 1.0)
    {
	anychan = 1.0;
    }
    
    minorchan = get_float_resource ("minorchan","Float");
    if (minorchan < 0.0)
    {
	minorchan = 0.0;
    }
    else if (minorchan > 1.0)
    {
	minorchan = 1.0;
    }
    
    instantdeathchan = get_float_resource ("instantdeathchan","Float");
    if (instantdeathchan < 0.0)
    {
	instantdeathchan = 0.0;
    }
    else if (instantdeathchan > 1.0)
    {
	instantdeathchan = 1.0;
    }

    minlifespan = get_integer_resource ("minlifespan", "Integer");
    if (minlifespan < 1)
    {
	minlifespan = 1;
    }

    maxlifespan = get_integer_resource ("maxlifespan", "Integer");
    if (maxlifespan < minlifespan)
    {
	maxlifespan = minlifespan;
    }

    minlifespeed = get_float_resource ("minlifespeed", "Float");
    if (minlifespeed < 0.0)
    {
	minlifespeed = 0.0;
    }
    else if (minlifespeed > 1.0)
    {
	minlifespeed = 1.0;
    }

    maxlifespeed = get_float_resource ("maxlifespeed", "Float");
    if (maxlifespeed < minlifespeed)
    {
	maxlifespeed = minlifespeed;
    }
    else if (maxlifespeed > 1.0)
    {
	maxlifespeed = 1.0;
    }

    mindeathspeed = get_float_resource ("mindeathspeed", "Float");
    if (mindeathspeed < 0.0)
    {
	mindeathspeed = 0.0;
    }
    else if (mindeathspeed > 1.0)
    {
	mindeathspeed = 1.0;
    }

    maxdeathspeed = get_float_resource ("maxdeathspeed", "Float");
    if (maxdeathspeed < mindeathspeed)
    {
	maxdeathspeed = mindeathspeed;
    }
    else if (maxdeathspeed > 1.0)
    {
	maxdeathspeed = 1.0;
    }

    minlifespeed *= diaglim;
    maxlifespeed *= diaglim;
    mindeathspeed *= diaglim;
    maxdeathspeed *= diaglim;

    cmap = xgwa.colormap;
    
    windowWidth = xgwa.width;
    windowHeight = xgwa.height;
    
    arr_width = windowWidth / cell_size;
    arr_height = windowHeight / cell_size;

    alloc_size = sizeof(cell) * arr_width * arr_height;
    oalloc = alloc_size;

    if (mem_throttle > 0)
      while (cell_size < windowWidth/10 &&
             cell_size < windowHeight/10 &&
             alloc_size > mem_throttle)
        {
          cell_size++;
          arr_width = windowWidth / cell_size;
          arr_height = windowHeight / cell_size;
          alloc_size = sizeof(cell) * arr_width * arr_height;
        }

    if (osize != cell_size)
      {
        static int warned = 0;
        if (!warned)
          {
            fprintf (stderr,
             "%s: throttling cell size from %d to %d because of %dM limit.\n",
                     progname, osize, cell_size, mem_throttle / (1 << 20));
            fprintf (stderr, "%s: %dx%dx%d = %.1fM, %dx%dx%d = %.1fM.\n",
                     progname,
                     windowWidth, windowHeight, osize,
                     ((float) oalloc) / (1 << 20),
                     windowWidth, windowHeight, cell_size,
                     ((float) alloc_size) / (1 << 20));
            warned = 1;
          }
      }

    xSize = windowWidth / arr_width;
    ySize = windowHeight / arr_height;
    if (xSize > ySize)
    {
	xSize = ySize;
    }
    else
    {
	ySize = xSize;
    }
    
    xOffset = (windowWidth - (arr_width * xSize)) / 2;
    yOffset = (windowHeight - (arr_height * ySize)) / 2;

    if (originalcolors)
    {
	setup_original_colormap (&xgwa);
    }
    else
    {
	setup_random_colormap (&xgwa);
    }
}

static void drawblock (int x, int y, unsigned char c)
{
  if (xSize == 1 && ySize == 1)
    XDrawPoint (display, window, coloredGCs[c], x + xOffset, y + yOffset);
  else
    XFillRectangle (display, window, coloredGCs[c],
		    x * xSize + xOffset, y * ySize + yOffset,
		    xSize, ySize);
}

static void setup_arr (void)
{
    int x, y;

    if (arr != NULL)
    {
	free (arr);
    }

    XFillRectangle (display, window, coloredGCs[0], 0, 0, 
		    windowWidth, windowHeight);

    arr = (cell *) calloc (sizeof(cell), arr_width * arr_height);  
    if (!arr)
      {
        fprintf (stderr, "%s: out of memory allocating %dx%d grid\n",
                 progname, arr_width, arr_height);
        exit (1);
      }

    for (y = 0; y < arr_height; y++)
    {
      int row = y * arr_width;
	for (x = 0; x < arr_width; x++) 
	{
	    arr[row+x].x = x;
	    arr[row+x].y = y;
	    arr[row+x].speed = 0.0;
	    arr[row+x].growth = 0.0;
	    arr[row+x].col = 0;
	    arr[row+x].isnext = 0;
	    arr[row+x].next = 0;
	    arr[row+x].prev = 0;
	}
    }

    if (head == NULL)
    {
	head = (cell *) malloc (sizeof (cell));
    }
    
    if (tail == NULL)
    {
	tail = (cell *) malloc (sizeof (cell));
    }

    head->next = tail;
    head->prev = head;
    tail->next = tail;
    tail->prev = head;

    blastcount = random_life_value ();
}

static void newcell (cell *c, unsigned char col, FLOAT sp)
{
    if (! c) return;
    
    if (c->col == col) return;
    
    c->nextcol = col;
    c->nextspeed = sp;
    c->isnext = 1;
    
    if (c->prev == 0) {
	c->next = head->next;
	c->prev = head;
	head->next = c;
	c->next->prev = c;
    }
}

static void killcell (cell *c)
{
    c->prev->next = c->next;
    c->next->prev = c->prev;
    c->prev = 0;
    c->speed = 0.0;
    drawblock (c->x, c->y, c->col);
}


static void randblip (int doit)
{
    int n;
    int b = 0;
    if (!doit 
	&& (blastcount-- >= 0) 
	&& (RAND_FLOAT > anychan))
    {
	return;
    }
    
    if (blastcount < 0) 
    {
	b = 1;
	n = 2;
	blastcount = random_life_value ();
	if (RAND_FLOAT < instantdeathchan)
	{
	    /* clear everything every so often to keep from getting into a
	     * rut */
	    setup_arr ();
	    b = 0;
	}
    }
    else if (RAND_FLOAT <= minorchan) 
    {
	n = 2;
    }
    else 
    {
	n = random () % 3 + 3;
    }
    
    while (n--) 
    {
	int x = random () % arr_width;
	int y = random () % arr_height;
	int c;
	FLOAT s;
	if (b)
	{
	    c = 0;
	    s = RAND_FLOAT * (maxdeathspeed - mindeathspeed) + mindeathspeed;
	}
	else
	{
	    c = (random () % (count-1)) + 1;
	    s = RAND_FLOAT * (maxlifespeed - minlifespeed) + minlifespeed;
	}
	newcell (&arr[y * arr_width + x], c, s);
    }
}

static void update (void)
{
    cell *a;
    
    for (a = head->next; a != tail; a = a->next) 
    {
        static XPoint coords1[] = {{-1,  0}, { 1, 0}, {0, -1}, {0, 1}};
        static XPoint coords2[] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
        XPoint *coords = 0;
        int i;

        if (a->speed == 0) continue;
        a->growth += a->speed;
        if (a->growth >= orthlim) 
          coords = coords1;

	if (a->growth >= diaglim) 
          coords = coords2;

        if (coords)
          for (i = 0; i < 4; i++)
            {
              int x = a->x + coords[i].x;
              int y = a->y + coords[i].y;

              if (x < 0) x = arr_width - 1;
              else if (x >= arr_width) x = 0;

              if (y < 0) y = arr_height - 1;
              else if (y >= arr_height) y = 0;

              newcell (&arr[y * arr_width + x], a->col, a->speed);
            }

	if (a->growth >= diaglim) 
	    killcell (a);
    }
    
    randblip ((head->next) == tail);
    
    for (a = head->next; a != tail; a = a->next)
    {
	if (a->isnext) 
	{
	    a->isnext = 0;
	    a->speed = a->nextspeed;
	    a->growth = 0.0;
	    a->col = a->nextcol;
	    drawblock (a->x, a->y, a->col + count);
	}
    }
}


char *progclass = "Petri";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*delay:		10000",
  "*count:		8",
  "*size:		4",
  "*diaglim:		1.414",
  "*anychan:		0.0015",
  "*minorchan:		0.5",
  "*instantdeathchan:	0.2",
  "*minlifespan:	500",
  "*maxlifespan:	1500",
  "*minlifespeed:	0.04",
  "*maxlifespeed:	0.13",
  "*mindeathspeed:	0.42",
  "*maxdeathspeed:	0.46",
  "*originalcolors:	false",
  "*memThrottle:        22M",	/* don't malloc more than this much.
                                   Scale the pixels up if necessary. */
    0
};

XrmOptionDescRec options [] = {
  { "-delay",		 ".delay",		XrmoptionSepArg, 0 },
  { "-size",		 ".size",		XrmoptionSepArg, 0 },
  { "-count",		 ".count",		XrmoptionSepArg, 0 },
  { "-diaglim",		 ".diaglim",		XrmoptionSepArg, 0 },
  { "-anychan",		 ".anychan",		XrmoptionSepArg, 0 },
  { "-minorchan",	 ".minorchan",		XrmoptionSepArg, 0 },
  { "-instantdeathchan", ".instantdeathchan",	XrmoptionSepArg, 0 },
  { "-minlifespan",	 ".minlifespan",	XrmoptionSepArg, 0 },
  { "-maxlifespan",	 ".maxlifespan",	XrmoptionSepArg, 0 },
  { "-minlifespeed",	 ".minlifespeed",	XrmoptionSepArg, 0 },
  { "-maxlifespeed",	 ".maxlifespeed",	XrmoptionSepArg, 0 },
  { "-mindeathspeed",	 ".mindeathspeed",	XrmoptionSepArg, 0 },
  { "-maxdeathspeed",	 ".maxdeathspeed",	XrmoptionSepArg, 0 },
  { "-originalcolors",	 ".originalcolors",	XrmoptionNoArg,  "true" },
  { "-mem-throttle",	 ".memThrottle",	XrmoptionSepArg,  0 },
  { 0, 0, 0, 0 }
};

void screenhack (Display *dpy, Window win)
{
    int delay = get_integer_resource ("delay", "Delay");
    display = dpy;
    window = win;
    setup_display ();
    
    setup_arr ();
    
    randblip (1);
    
    for (;;) 
    {
	update ();
        XSync (dpy, False);
        screenhack_handle_events (dpy);
	usleep (delay);
    }
}
