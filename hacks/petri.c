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
 */

#include <math.h>
#include "screenhack.h"
#include "spline.h"

#define FLOAT float
#define RAND_FLOAT (((FLOAT) (random() & 0xffff)) / ((FLOAT) 0x10000))

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

struct state {
  Display *dpy;
  Window window;

  int arr_width;
  int arr_height;
  int count;

  cell *arr;
  cell *head;
  cell *tail;
  int blastcount;

  GC *coloredGCs;

  int windowWidth;
  int windowHeight;
  int xOffset;
  int yOffset;
  int xSize;
  int ySize;

  FLOAT orthlim;
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
  Bool originalcolors;

  int warned;
  int delay;
};


#define cell_x(c) (st->arr_width ? ((c) - st->arr) % st->arr_width : 0)
#define cell_y(c) (st->arr_width ? ((c) - st->arr) / st->arr_width : 0)


static int random_life_value (struct state *st)
{
    return (int) ((RAND_FLOAT * (st->maxlifespan - st->minlifespan)) + st->minlifespan);
}

static void setup_random_colormap (struct state *st, XWindowAttributes *xgwa)
{
    XGCValues gcv;
    int lose = 0;
    int ncolors = st->count - 1;
    int n;
    XColor *colors = (XColor *) calloc (sizeof(*colors), st->count*2);
    
    colors[0].pixel = get_pixel_resource (st->dpy, xgwa->colormap,
                                          "background", "Background");
    
    make_random_colormap (st->dpy, xgwa->visual, xgwa->colormap,
			  colors+1, &ncolors, True, True, 0, True);
    if (ncolors < 1)
      {
        fprintf (stderr, "%s: couldn't allocate any colors\n", progname);
	exit (-1);
      }
    
    ncolors++;
    st->count = ncolors;
    
    memcpy (colors + st->count, colors, st->count * sizeof(*colors));
    colors[st->count].pixel = get_pixel_resource (st->dpy, xgwa->colormap,
                                              "foreground", "Foreground");
    
    for (n = 1; n < st->count; n++)
    {
	int m = n + st->count;
	colors[n].red = colors[m].red / 2;
	colors[n].green = colors[m].green / 2;
	colors[n].blue = colors[m].blue / 2;
	
	if (!XAllocColor (st->dpy, xgwa->colormap, &colors[n]))
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
    
    for (n = 0; n < st->count*2; n++) 
    {
	gcv.foreground = colors[n].pixel;
	st->coloredGCs[n] = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
    }

    free (colors);
}

static void setup_original_colormap (struct state *st, XWindowAttributes *xgwa)
{
    XGCValues gcv;
    int lose = 0;
    int n;
    XColor *colors = (XColor *) calloc (sizeof(*colors), st->count*2);
    
    colors[0].pixel = get_pixel_resource (st->dpy, xgwa->colormap,
                                          "background", "Background");

    colors[st->count].pixel = get_pixel_resource (st->dpy, xgwa->colormap,
                                              "foreground", "Foreground");

    for (n = 1; n < st->count; n++)
    {
	int m = n + st->count;
	colors[n].red =   ((n & 0x01) != 0) * 0x8000;
	colors[n].green = ((n & 0x02) != 0) * 0x8000;
	colors[n].blue =  ((n & 0x04) != 0) * 0x8000;

	if (!XAllocColor (st->dpy, xgwa->colormap, &colors[n]))
	{
	    lose++;
	    colors[n] = colors[0];
	}

	colors[m].red   = colors[n].red + 0x4000;
	colors[m].green = colors[n].green + 0x4000;
	colors[m].blue  = colors[n].blue + 0x4000;

	if (!XAllocColor (st->dpy, xgwa->colormap, &colors[m]))
	{
	    lose++;
	    colors[m] = colors[st->count];
	}
    }

    if (lose)
    {
	fprintf (stderr, 
		 "%s: unable to allocate %d colors.\n",
		 progname, lose);
    }
    
    for (n = 0; n < st->count*2; n++) 
    {
	gcv.foreground = colors[n].pixel;
	st->coloredGCs[n] = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
    }

    free (colors);
}

static void
setup_display (struct state *st)
{
    XWindowAttributes xgwa;
    Colormap cmap;

    int cell_size = get_integer_resource (st->dpy, "size", "Integer");
    int osize, alloc_size, oalloc;
    int mem_throttle = 0;
    char *s;

    if (cell_size < 1) cell_size = 1;

    osize = cell_size;

    s = get_string_resource (st->dpy, "memThrottle", "MemThrottle");
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

    XGetWindowAttributes (st->dpy, st->window, &xgwa);

    st->originalcolors = get_boolean_resource (st->dpy, "originalcolors", "Boolean");

    st->count = get_integer_resource (st->dpy, "count", "Integer");
    if (st->count < 2) st->count = 2;

    /* number of colors can't be greater than the half depth of the screen. */
    if (st->count > (unsigned int) (1L << (xgwa.depth-1)))
      st->count = (unsigned int) (1L << (xgwa.depth-1));

    /* Actually, since cell->col is of type char, this has to be small. */
    if (st->count >= (unsigned int) (1L << ((sizeof(st->arr[0].col) * 8) - 1)))
      st->count = (unsigned int) (1L << ((sizeof(st->arr[0].col) * 8) - 1));


    if (st->originalcolors && (st->count > 8))
    {
	st->count = 8;
    }

    st->coloredGCs = (GC *) calloc (sizeof(GC), st->count * 2);

    st->diaglim  = get_float_resource (st->dpy, "diaglim", "Float");
    if (st->diaglim < 1.0)
    {
	st->diaglim = 1.0;
    }
    else if (st->diaglim > 2.0)
    {
	st->diaglim = 2.0;
    }
    st->diaglim *= st->orthlim;

    st->anychan  = get_float_resource (st->dpy, "anychan", "Float");
    if (st->anychan < 0.0)
    {
	st->anychan = 0.0;
    }
    else if (st->anychan > 1.0)
    {
	st->anychan = 1.0;
    }
    
    st->minorchan = get_float_resource (st->dpy, "minorchan","Float");
    if (st->minorchan < 0.0)
    {
	st->minorchan = 0.0;
    }
    else if (st->minorchan > 1.0)
    {
	st->minorchan = 1.0;
    }
    
    st->instantdeathchan = get_float_resource (st->dpy, "instantdeathchan","Float");
    if (st->instantdeathchan < 0.0)
    {
	st->instantdeathchan = 0.0;
    }
    else if (st->instantdeathchan > 1.0)
    {
	st->instantdeathchan = 1.0;
    }

    st->minlifespan = get_integer_resource (st->dpy, "minlifespan", "Integer");
    if (st->minlifespan < 1)
    {
	st->minlifespan = 1;
    }

    st->maxlifespan = get_integer_resource (st->dpy, "maxlifespan", "Integer");
    if (st->maxlifespan < st->minlifespan)
    {
	st->maxlifespan = st->minlifespan;
    }

    st->minlifespeed = get_float_resource (st->dpy, "minlifespeed", "Float");
    if (st->minlifespeed < 0.0)
    {
	st->minlifespeed = 0.0;
    }
    else if (st->minlifespeed > 1.0)
    {
	st->minlifespeed = 1.0;
    }

    st->maxlifespeed = get_float_resource (st->dpy, "maxlifespeed", "Float");
    if (st->maxlifespeed < st->minlifespeed)
    {
	st->maxlifespeed = st->minlifespeed;
    }
    else if (st->maxlifespeed > 1.0)
    {
	st->maxlifespeed = 1.0;
    }

    st->mindeathspeed = get_float_resource (st->dpy, "mindeathspeed", "Float");
    if (st->mindeathspeed < 0.0)
    {
	st->mindeathspeed = 0.0;
    }
    else if (st->mindeathspeed > 1.0)
    {
	st->mindeathspeed = 1.0;
    }

    st->maxdeathspeed = get_float_resource (st->dpy, "maxdeathspeed", "Float");
    if (st->maxdeathspeed < st->mindeathspeed)
    {
	st->maxdeathspeed = st->mindeathspeed;
    }
    else if (st->maxdeathspeed > 1.0)
    {
	st->maxdeathspeed = 1.0;
    }

    st->minlifespeed *= st->diaglim;
    st->maxlifespeed *= st->diaglim;
    st->mindeathspeed *= st->diaglim;
    st->maxdeathspeed *= st->diaglim;

    cmap = xgwa.colormap;
    
    st->windowWidth = xgwa.width;
    st->windowHeight = xgwa.height;
    
    st->arr_width = st->windowWidth / cell_size;
    st->arr_height = st->windowHeight / cell_size;

    alloc_size = sizeof(cell) * st->arr_width * st->arr_height;
    oalloc = alloc_size;

    if (mem_throttle > 0)
      while (cell_size < st->windowWidth/10 &&
             cell_size < st->windowHeight/10 &&
             alloc_size > mem_throttle)
        {
          cell_size++;
          st->arr_width = st->windowWidth / cell_size;
          st->arr_height = st->windowHeight / cell_size;
          alloc_size = sizeof(cell) * st->arr_width * st->arr_height;
        }

    if (osize != cell_size)
      {
        if (!st->warned)
          {
            fprintf (stderr,
             "%s: throttling cell size from %d to %d because of %dM limit.\n",
                     progname, osize, cell_size, mem_throttle / (1 << 20));
            fprintf (stderr, "%s: %dx%dx%d = %.1fM, %dx%dx%d = %.1fM.\n",
                     progname,
                     st->windowWidth, st->windowHeight, osize,
                     ((float) oalloc) / (1 << 20),
                     st->windowWidth, st->windowHeight, cell_size,
                     ((float) alloc_size) / (1 << 20));
            st->warned = 1;
          }
      }

    st->xSize = st->arr_width ? st->windowWidth / st->arr_width : 0;
    st->ySize = st->arr_height ? st->windowHeight / st->arr_height : 0;
    if (st->xSize > st->ySize)
    {
	st->xSize = st->ySize;
    }
    else
    {
	st->ySize = st->xSize;
    }
    
    st->xOffset = (st->windowWidth - (st->arr_width * st->xSize)) / 2;
    st->yOffset = (st->windowHeight - (st->arr_height * st->ySize)) / 2;

    if (st->originalcolors)
    {
	setup_original_colormap (st, &xgwa);
    }
    else
    {
	setup_random_colormap (st, &xgwa);
    }
}

static void drawblock (struct state *st, int x, int y, unsigned char c)
{
  if (st->xSize == 1 && st->ySize == 1)
    XDrawPoint (st->dpy, st->window, st->coloredGCs[c], x + st->xOffset, y + st->yOffset);
  else
    XFillRectangle (st->dpy, st->window, st->coloredGCs[c],
		    x * st->xSize + st->xOffset, y * st->ySize + st->yOffset,
		    st->xSize, st->ySize);
}

static void setup_arr (struct state *st)
{
    int x, y;

    if (st->arr != NULL)
    {
	free (st->arr);
    }

    XFillRectangle (st->dpy, st->window, st->coloredGCs[0], 0, 0, 
		    st->windowWidth, st->windowHeight);
    
    if (!st->arr_width) st->arr_width = 1;
    if (!st->arr_height) st->arr_height = 1;

    st->arr = (cell *) calloc (sizeof(cell), st->arr_width * st->arr_height);  
    if (!st->arr)
      {
        fprintf (stderr, "%s: out of memory allocating %dx%d grid\n",
                 progname, st->arr_width, st->arr_height);
        exit (1);
      }

    for (y = 0; y < st->arr_height; y++)
    {
      int row = y * st->arr_width;
       for (x = 0; x < st->arr_width; x++) 
	{
	    st->arr[row+x].speed = 0.0;
	    st->arr[row+x].growth = 0.0;
	    st->arr[row+x].col = 0;
	    st->arr[row+x].isnext = 0;
	    st->arr[row+x].next = 0;
	    st->arr[row+x].prev = 0;
	}
    }

    if (st->head == NULL)
    {
	st->head = (cell *) malloc (sizeof (cell));
    }
    
    if (st->tail == NULL)
    {
	st->tail = (cell *) malloc (sizeof (cell));
    }

    st->head->next = st->tail;
    st->head->prev = st->head;
    st->tail->next = st->tail;
    st->tail->prev = st->head;

    st->blastcount = random_life_value (st);
}

static void newcell (struct state *st, cell *c, unsigned char col, FLOAT sp)
{
    if (! c) return;
    
    if (c->col == col) return;
    
    c->nextcol = col;
    c->nextspeed = sp;
    c->isnext = 1;
    
    if (c->prev == 0) {
	c->next = st->head->next;
	c->prev = st->head;
	st->head->next = c;
	c->next->prev = c;
    }
}

static void killcell (struct state *st, cell *c)
{
    c->prev->next = c->next;
    c->next->prev = c->prev;
    c->prev = 0;
    c->speed = 0.0;
    drawblock (st, cell_x(c), cell_y(c), c->col);
}


static void randblip (struct state *st, int doit)
{
    int n;
    int b = 0;
    if (!doit 
	&& (st->blastcount-- >= 0) 
	&& (RAND_FLOAT > st->anychan))
    {
	return;
    }
    
    if (st->blastcount < 0) 
    {
	b = 1;
	n = 2;
	st->blastcount = random_life_value (st);
	if (RAND_FLOAT < st->instantdeathchan)
	{
	    /* clear everything every so often to keep from getting into a
	     * rut */
	    setup_arr (st);
	    b = 0;
	}
    }
    else if (RAND_FLOAT <= st->minorchan) 
    {
	n = 2;
    }
    else 
    {
	n = random () % 3 + 3;
    }
    
    while (n--) 
    {
      int x = st->arr_width ? random () % st->arr_width : 0;
      int y = st->arr_height ? random () % st->arr_height : 0;
	int c;
	FLOAT s;
	if (b)
	{
	    c = 0;
	    s = RAND_FLOAT * (st->maxdeathspeed - st->mindeathspeed) + st->mindeathspeed;
	}
	else
	{
	    c = ((st->count - 1) ? random () % (st->count-1) : 0) + 1;
	    s = RAND_FLOAT * (st->maxlifespeed - st->minlifespeed) + st->minlifespeed;
	}
	newcell (st, &st->arr[y * st->arr_width + x], c, s);
    }
}

static void update (struct state *st)
{
    cell *a;
    
    for (a = st->head->next; a != st->tail; a = a->next) 
    {
	static const XPoint all_coords[] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
                                            {-1,  0}, { 1, 0}, {0, -1}, {0, 1},
                                            {99, 99}};

        const XPoint *coords = 0;

        if (a->speed == 0) continue;
        a->growth += a->speed;

	if (a->growth >= st->diaglim) 
	{
	    coords = all_coords;
	}
        else if (a->growth >= st->orthlim)
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

	    if (x < 0) x = st->arr_width - 1;
	    else if (x >= st->arr_width) x = 0;
	    
	    if (y < 0) y = st->arr_height - 1;
	    else if (y >= st->arr_height) y = 0;
	    
	    newcell (st, &st->arr[y * st->arr_width + x], a->col, a->speed);
	}

	if (a->growth >= st->diaglim) 
	    killcell (st, a);
    }

    randblip (st, (st->head->next) == st->tail);

    for (a = st->head->next; a != st->tail; a = a->next)
    {
	if (a->isnext) 
	{
	    a->isnext = 0;
	    a->speed = a->nextspeed;
	    a->growth = 0.0;
	    a->col = a->nextcol;
	    drawblock (st, cell_x(a), cell_y(a), a->col + st->count);
	}
    }
}

static void *
petri_init (Display *dpy, Window win)
{
    struct state *st = (struct state *) calloc (1, sizeof(*st));
    st->dpy = dpy;
    st->window = win;

    st->delay = get_integer_resource (st->dpy, "delay", "Delay");
    st->orthlim = 1;

    setup_display (st);
    setup_arr (st);
    randblip (st, 1);
    
    return st;
}

static unsigned long
petri_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  update (st);
  return st->delay;
}

static void
petri_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
petri_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
petri_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}



static const char *petri_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*delay:		10000",
  "*count:		20",
  "*size:		2",
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

static XrmOptionDescRec petri_options [] = {
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


XSCREENSAVER_MODULE ("Petri", petri)
