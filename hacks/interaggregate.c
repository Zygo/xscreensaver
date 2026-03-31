/*
 *  InterAggregate (dagraz@gmail.com)
 *  Based on code from complexification.net Intersection Aggregate
 *  http://www.complexification.net/gallery/machines/interAggregate/index.php
 *
 *  Intersection Aggregate code:
 *  j.tarbell   May, 2004
 *  Albuquerque, New Mexico
 *  complexification.net
 *
 *  Also based on substrate, a port of j.tarbell's Substrate Art done
 *  by dragorn@kismetwireless.net
 *
 *  
 *  Interesting command line options, all of which serve to
 *  concentrate the painting in some way:
 *  
 *  -percent-orbits 100 -base-orbits 50 -base-on-center -growth-delay 0
 *
 *  Paint should be concentrated in the center of the canvas, orbiting
 *  about it.  -percent-orbits 100 implies -base-on-center, so that's
 *  not really needed.
 *
 *
 *  -percent-orbits 99 -base-orbits 50 -growth-delay 0
 *
 *  Like the above example, but the 'center' will rove about the screen.
 *
 *  -percent-orbits 98 -base-orbits 50 -growth-delay 0
 *
 *  Like the above example, but there will be two roving centers.
 *
 *
 *  TODO:
 *  -fix alpha blending / byte ordering
 *
 *  CHANGES
 *
 *
 * Directly based the hacks of: 
 * 
 * xscreensaver, Copyright (c) 1997, 1998, 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include "screenhack.h"


/* this program goes faster if some functions are inline.  The following is
 * borrowed from ifs.c */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif

#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

#ifndef MAX
#define MAX(x,y) ((x < y) ? y : x)
#endif

static const char *interaggregate_defaults[] = 
{
    ".background: white",
    ".foreground: black",
    "*fpsSolid:	true",
    "*maxCycles: 100000",
#ifdef TIME_ME
    "*growthDelay: 0",
#else
    "*growthDelay: 18000", 
#endif
    "*numCircles: 100",
    "*percentOrbits: 0",
    "*baseOrbits: 75",
    "*baseOnCenter: False",
    "*drawCenters: False",
#ifdef HAVE_MOBILE
    "*ignoreRotation: True",
#endif
    0
};

static XrmOptionDescRec interaggregate_options[] = 
{
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-max-cycles", ".maxCycles", XrmoptionSepArg, 0},
    {"-growth-delay", ".growthDelay", XrmoptionSepArg, 0},
    {"-num-circles", ".numCircles", XrmoptionSepArg, 0},
    {"-percent-orbits", ".percentOrbits", XrmoptionSepArg, 0},
    {"-base-orbits", ".baseOrbits", XrmoptionSepArg, 0},
    {"-base-on-center", ".baseOnCenter", XrmoptionNoArg, "true"}, 
    {"-draw-centers", ".drawCenters", XrmoptionNoArg, "true"}, 
    {0, 0, 0, 0}
};

/* Raw colormap extracted from pollockEFF.gif */
#if 0
char *rgb_colormap[] = 
{
    "#FFFFFF", /* white */
    "#000000", /* black */
    "#000000", /* more black */
    /* "#736451",  */
    "#4e3e2e", /* olive */
    /* "#666666", */
    "#694d35",  /* camel */
    "#b9a88c",  /* tan */
    0
};
#endif

static const char *rgb_colormap[] = 
{
    "#FFFFFF", /* white */
    "#000000", /* more black */
    "#000000", /* more black */
    "#4e3e2e", /* olive */
    "#694d35",  /* camel */
    "#b0a085",  /* tan */
    "#e6d3ae",
    0
};

/* black white brown olive grey camel */

typedef enum { LINEAR, ORBIT } PathType;

typedef struct 
{

    unsigned long color;
    double gain;
    double p;

} SandPainter;

typedef struct _circle
{
    double radius;

    double x;
    double y;

    PathType path_type;

    /* for a linear path */
    double dx;
    double dy;

    /* for orbital path */
    double theta;
    double r; 
    double dtheta;
    
    struct _circle* center;

    int num_painters;
    SandPainter* painters;

} Circle;


struct field 
{
    int height;
    int width;

    int num_circles;
    Circle* circles;

    int percent_orbits;
    int base_orbits;
    Bool base_on_center;

    /* used for orbits circling the center of the screen */
    Circle center_of_universe;

    /* Raw map of pixels we need to keep for alpha blending */
    unsigned long int *off_img;
   
    /* color parms */
    int numcolors;
    unsigned long *parsedcolors;
    unsigned long fgcolor;
    unsigned long bgcolor;
    int visdepth;

    unsigned int cycles;

    double max_gain;

    /* for debugging */
    Bool draw_centers;

    /* for profiling whatnot */ 
    int possible_intersections;
    int intersection_count;
};


static struct field *
init_field(void)
{
    struct field *f = (struct field*) malloc(sizeof(struct field));
    if ( f == NULL )
    {
	fprintf(stderr, "%s: Failed to allocate field!\n", progname);
	exit(1);
    }

    f->height = 0;
    f->width = 0;
    f->num_circles = 0;
    f->circles = NULL;
    f->percent_orbits = 0;
    f->base_orbits = 0;
    f->base_on_center = False;
    f->off_img = NULL;
    f->numcolors = 0;
    f->parsedcolors = NULL;
    f->fgcolor = 0;
    f->bgcolor = 0;
    f->visdepth = 0;

    f->cycles = 0;

    f->max_gain = 0.22;

    f->draw_centers = False;

    f->possible_intersections = 0;
    f->intersection_count = 0;

    return f;
}

/* Quick references to pixels in the offscreen map and in the crack grid */
#define ref_pixel(f, x, y)   ((f)->off_img[(y) * (f)->width + (x)])

#define in_bounds(f, x, y) ((x >= 0) && (x < f->width) && (y >= 0) && (y < f->height))

/* Consider rewriting with XQueryColor, or ImageByteOrder */

static inline void point2rgb(int depth, unsigned long c, int *r, int *g, int *b) 
{
    switch(depth) 
    {
    case 32:
    case 24:
#ifdef HAVE_JWXYZ
        /* This program idiotically does not go through a color map, so
           we have to hardcode in knowledge of how jwxyz.a packs pixels!
           Fix it to go through st->colors[st->ncolors] instead!
         */
        *r = (c & 0x00ff0000) >> 16; 
        *g = (c & 0x0000ffff) >>  8;
        *b = (c & 0x000000ff);
#else
	*b = c & 0xff; 
	*g = (c & 0xff00) >> 8; 
	*r = (c & 0xff0000) >> 16; 
#endif
	break;
    case 16:
	*b = (int) (c & 0x1f) << 3;
	*g = (int) ((c >> 5) & 0x3f) << 2;
	*r = (int) ((c >> 11) & 0x1f) << 3;
	break;
    case 15:
	*b = (int) (c & 0x1f) << 3;
	*g = (int) ((c >> 5) & 0x1f) << 3;
	*r = (int) ((c >> 10) & 0x1f) << 3;
	break;
    }
}

static inline unsigned long rgb2point(int depth, int r, int g, int b) 
{
    unsigned long ret = 0;

    switch(depth) 
    {
    case 32:
    case 24:
#ifdef HAVE_JWXYZ
        /* This program idiotically does not go through a color map, so
           we have to hardcode in knowledge of how jwxyz.a packs pixels!
           Fix it to go through st->colors[st->ncolors] instead!
         */
        ret = 0xFF000000 | (r << 16) | (g << 8) | b;
#else
	ret |= (r << 16) | (g << 8) | b;
#endif
	break;
    case 16:
	ret = ((r>>3) << 11) | ((g>>2)<<5) | (b>>3);
	break;
    case 15:
	ret = ((r>>3) << 10) | ((g>>3)<<5) | (b>>3);
	break;
    }

    return ret;
}

/* alpha blended point drawing -- this is Not Right and will likely fail on 
 * non-intel platforms as it is now, needs fixing */
static inline unsigned long trans_point(int x1, int y1, unsigned long myc, double a, 
				 struct field *f) 
{
    if (a >= 1.0) 
    {
	ref_pixel(f, x1, y1) = myc;
	return myc;
    } 
    else 
    {
	int or=0, og=0, ob=0;
	int r=0, g=0, b=0;
	int nr, ng, nb;
	unsigned long c;

	c = ref_pixel(f, x1, y1);

	point2rgb(f->visdepth, c, &or, &og, &ob);
	point2rgb(f->visdepth, myc, &r, &g, &b);

	nr = or + (r - or) * a;
	ng = og + (g - og) * a;
	nb = ob + (b - ob) * a;

	c = rgb2point(f->visdepth, nr, ng, nb);

	ref_pixel(f, x1, y1) = c;

	return c;
    }
}

static inline void drawPoint(int x, int y, unsigned long color, double intensity,
		      Display *dpy, Window window, GC fgc, struct field *f)
	       
{
    unsigned long c;

    while ( x >= f->width ) x -= f->width;
    while ( x < 0 ) x += f->width;
	
    while ( y >= f->height ) y -= f->height;
    while ( y < 0 ) y += f->height;

    /* if ( in_bounds(f, x, y) ) ... */

    c = trans_point(x, y, color, intensity, f);

    XSetForeground(dpy, fgc, c);
    XDrawPoint(dpy, window, fgc, x, y);
}

static inline void paint(SandPainter* painter, double ax, double ay, double bx, double by,
		  Display *dpy, Window window, GC fgc, 
		  struct field *f)
{
    /* the sand painter */

    double inc, sandp;
    int i;

    /* XXX try adding tpoint here, like orig */

    /* jitter the painter's values */
    painter->gain += frand(0.05) - 0.025;
    
    if ( painter->gain > f->max_gain )
	painter->gain = -f->max_gain;
    else if ( painter->gain < -f->max_gain )
	painter->gain = f->max_gain;

    painter->p += frand(0.1) - 0.05;
    
    if ( 0 < painter->p )
	painter->p = 0;
    else if ( painter->p > 1.0 )
	painter->p = 1.0;

    /* replace 0.1 with 1 / f->grains */
    inc = painter->gain * 0.1;
    sandp = 0;

    for(i = 0; i <= 10; ++i)
    {
	int drawx, drawy;
	double sp, sm;
	double intensity = 0.1 - 0.009 * i;

	sp = sin(painter->p + sandp);
	drawx = ax + (bx - ax) * sp;
	drawy = ay + (by - ay) * sp;

	drawPoint(drawx, drawy, painter->color,
		  intensity,
		  dpy, window, fgc, f);

	sm = sin(painter->p - sandp);
	drawx = ax + (bx - ax) * sm;
	drawy = ay + (by - ay) * sm;

	drawPoint(drawx, drawy, painter->color,
		  intensity,
		  dpy, window, fgc, f);

	sandp += inc;
    }
}

static void build_colors(struct field *f, Display *dpy, XWindowAttributes *xgwa) 
{

    XColor tmpcolor;
    int i;
    /* Count the colors in our map and assign them in a horrifically inefficient 
     * manner but it only happens once */

    for( f->numcolors = 0; 
	 rgb_colormap[f->numcolors] != NULL; 
	 ++f->numcolors )
    {
	;
    }

    if (f->numcolors < 1) f->numcolors = 1;
    f->parsedcolors = (unsigned long *) calloc(f->numcolors,
					       sizeof(unsigned long));
    if ( f->parsedcolors == NULL )
    {
        fprintf(stderr, "%s: Failed to allocate parsedcolors\n",
                progname);
	exit(1);
    }
	
    for(i = 0; i < f->numcolors; ++i)
    {
        if (!XParseColor(dpy, xgwa->colormap, 
			 rgb_colormap[i], &tmpcolor)) 
	{
            fprintf(stderr, "%s: couldn't parse color %s\n", progname,
                    rgb_colormap[i]);
            exit(1);
        }

        if (!XAllocColor(dpy, xgwa->colormap, &tmpcolor)) 
	{
            fprintf(stderr, "%s: couldn't allocate color %s\n", progname,
                    rgb_colormap[i]);
            exit(1);
        }

        f->parsedcolors[i] = tmpcolor.pixel;

    }
}

/* used when the window is resized */
static void build_img(struct field *f)
{
    if (f->off_img) {
        free(f->off_img);
        f->off_img = NULL;
    }

    f->off_img = (unsigned long *) calloc(f->width * f->height,
					  sizeof(unsigned long));
					   

    if ( f->off_img == NULL )
    {
        fprintf(stderr, "%s: Failed to allocate off_img\n",
                progname);
	exit(1);
    }

    memset(f->off_img, f->bgcolor, 
	   sizeof(unsigned long) * f->width * f->height);
}

static void free_circles(struct field *f) 
{
    int i;

    if ( f->circles != NULL )
    {
	for(i = 0; i < f->num_circles; ++i)
	{
	    free (f->circles[i].painters);
	}

	free (f->circles);
	f->circles = NULL;
    }
}

static void build_field(Display *dpy, Window window, XWindowAttributes xgwa, GC fgc, 
		 struct field *f) 
{
    int i;
    int num_orbits;
    int base_orbits;
    int orbit_start;
    build_img(f);

    f->center_of_universe.x = f->width / 2.0;
    f->center_of_universe.y = f->height / 2.0;
    f->center_of_universe.r = MAX(f->width, f->height) / 2.0;

    num_orbits = (f->percent_orbits * f->num_circles) / 100;
    orbit_start = f->num_circles - num_orbits;
    base_orbits = orbit_start + (num_orbits * f->base_orbits) / 100;

    free_circles(f);

    f->circles = (Circle*) calloc(f->num_circles, sizeof(Circle));
    if ( f->circles == NULL )
    {
        fprintf(stderr, "%s: Failed to allocate off_img\n",
                progname);
	exit(1);
    }

    for(i = 0; i < f->num_circles; ++i)
    {
	int j;
	Circle *circle = f->circles + i;

	/* make this a pref */

	if ( i >= orbit_start )
	  circle->path_type = ORBIT;
	else
	  circle->path_type = LINEAR;


	if ( circle->path_type == LINEAR )
	{
	    circle->x = frand(f->width);
	    circle->y = frand(f->height);

	    circle->dx = frand(0.5) - 0.25; 
	    circle->dy = frand(0.5) - 0.25;
	    /* circle->dy = 0; */
	    /* circle->r  = f->height * (0.05 + frand(0.1)); */
	    circle->radius = 5 + frand(55);

	    /* in case we want orbits based on lines */
	    circle->r = MIN(f->width, f->height) / 2.0;
	    circle->center = NULL;
	}
	else /* == ORBIT */
	{
	    if (i < base_orbits )
	    {
	        if ( f->base_on_center )
		    circle->center = &f->center_of_universe; 
		else
		{
		    circle->center = f->circles + 
			((int)frand(orbit_start - 0.1));
		}

		circle->r = 1 + frand(MIN(f->width, f->height) / 2.0);

		/* circle->radius = 5 + frand(55); */
	    }
	    else
	    {
		/* give a preference for the earlier circles */

		double p = frand(0.9);

		circle->center = f->circles + (int) (p*i);

		circle->r = 1 + 0.5 * circle->center->r + 0.5 * frand(circle->center->r); 
		/* circle->r = 1 + frand(circle->center->r / 2); */


		/* circle->radius = MAX(5, frand(circle->r)); */
		/* circle->radius = 5 + frand(55); */
	    }

	    circle->radius = 5 + frand(MIN(55, circle->r)); 
	    circle->dtheta = (frand(0.5) - 0.25) / circle->r;
	    circle->theta = frand(2 * M_PI);

	    circle->x = circle->r * cos(circle->theta) + circle->center->x;
	    circle->y = circle->r * sin(circle->theta) + circle->center->y;

	}

	/* make this a command line option */
	circle->num_painters = 3;
	circle->painters = (SandPainter*) calloc(circle->num_painters, 
						 sizeof(SandPainter));
	if ( circle->painters == NULL )
	{
	    fprintf(stderr, "%s: failed to allocate painters", progname);
	    exit(1);
	}

	for(j = 0; j < circle->num_painters; ++j)
	{
	    SandPainter *painter = circle->painters + j;

	    painter->gain = frand(0.09) + 0.01;
	    painter->p = frand(1.0);
	    painter->color = 
		f->parsedcolors[(int)(frand(0.999) * f->numcolors)];
	}
    }
}

static void moveCircles(struct field *f)
{
    int i;

    for(i = 0; i < f->num_circles; ++i)
    {
	Circle *circle = f->circles + i;

	if ( circle->path_type == LINEAR )
	{
	    circle->x += circle->dx;
	    circle->y += circle->dy;

#if 0
	    if ( circle->x < -circle->radius )
		circle->x = f->width + circle->radius;
	    else if ( circle->x >= f->width + circle->radius )
		circle->x = -circle->radius;

	    if ( circle->y < -circle->radius )
		circle->y = f->height + circle->radius;
	    else if ( circle->y >= f->height + circle->radius )
		circle->y = -circle->radius;
#else
	    if ( circle->x < 0 ) circle->x += f->width;
	    else if ( circle->x >= f->width ) circle->x -= f->width;

	    if ( circle->y < 0 ) circle->y += f->height;
	    else if ( circle->y >= f->height ) circle->y -= f->height;
#endif
	}
	else /* (circle->path_type == ORBIT) */
	{
	    circle->theta += circle->dtheta;

	    if ( circle->theta < 0 ) circle->theta += 2 * M_PI;
	    else if ( circle->theta > 2 * M_PI ) circle->theta -= 2 * M_PI;

	    circle->x = circle->r * cos(circle->theta) + circle->center->x;
	    circle->y = circle->r * sin(circle->theta) + circle->center->y;

#if 0
	    if ( circle->x < -circle->radius )
		circle->x += f->width + 2 * circle->radius;
	    else if ( circle->x >= f->width + circle->radius )
		circle->x -= f->width + 2 * circle->radius;

	    if ( circle->y < -circle->radius )
		circle->y += f->height + 2 * circle->radius;
	    else if ( circle->y >= f->height + circle->radius )
		circle->y -= f->height + 2 * circle->radius;
#else
	    if ( circle->x < 0 ) circle->x += f->width;
	    else if ( circle->x >= f->width ) circle->x -= f->width;

	    if ( circle->y < 0 ) circle->y += f->height;
	    else if ( circle->y >= f->height ) circle->y -= f->height;
#endif
	}
    }
}

static void drawIntersections(Display *dpy, Window window, GC fgc, struct field *f)
{
    int i,j;

    /* One might be tempted to think 'hey, this is a crude algorithm
     * that is going to check each of the n (n-1) / 2 possible
     * intersections!  Why not try bsp trees, quad trees, etc, etc,
     * etc'
     *
     * In practice the time spent drawing the intersection of two
     * circles dwarfs the time takes to check for intersection.
     * Profiling on a 640x480 screen with 100 circles shows possible
     * speed gains to be only a couple of percent.
     * 
     * But hey, if you're bored, go have fun.  Let me know how it
     * turns out.
     */


    for(i = 0; i < f->num_circles; ++i)
    {
	Circle *c1 = f->circles + i;

	if ( !f->draw_centers )
	{
	    /* the default branch */

	    for(j = i + 1; j < f->num_circles; ++j)
	    {
		double d, dsqr, dx, dy;
		Circle *c2 = f->circles + j;

#ifdef TIME_ME
		++f->possible_intersections;
#endif
		dx = c2->x - c1->x;
		dy = c2->y - c1->y;

		dsqr = dx * dx + dy * dy;
		d = sqrt(dsqr);

		if ( (fabs(dx) < (c1->radius + c2->radius)) &&
		     (fabs(dy) < (c1->radius + c2->radius)) &&
		     ( d < (c1->radius + c2->radius) ) &&
		     ( d > fabs(c1->radius - c2->radius) ) )
		{
		    double d1, d2, r1sqr; 
		    double bx, by;
		    double midpx, midpy;
		    double int1x, int1y;
		    double int2x, int2y;
		    int s;

		    /* woo-hoo.  the circles are neither outside nor
		     * inside each other.  they intersect.  
		     *
		     * Now, compute the coordinates of the points of
		     * intersection 
		     */

#ifdef TIME_ME
		    ++f->intersection_count;
#endif

		    /* unit vector in direction of c1 to c2 */
		    bx = dx / d;
		    by = dy / d;

		    r1sqr = c1->radius * c1->radius;

		    /* distance from c1's center midpoint of intersection
		     * points */

		    d1 = 0.5 * (r1sqr - c2->radius * c2->radius + dsqr) / d;
		
		    midpx = c1->x + d1 * bx;
		    midpy = c1->y + d1 * by;

		    /* distance from midpoint to points of intersection */

		    d2 = sqrt(r1sqr - d1 * d1);

		    int1x = midpx + d2 * by;
		    int1y = midpy - d2 * bx;

		    int2x = midpx - d2 * by;
		    int2y = midpy + d2 * bx;

		    for(s = 0; s < c1->num_painters; ++s)
		    {
			paint(c1->painters + s, int1x, int1y, int2x, int2y, 
			      dpy, window, fgc, f);
		    }
		}
	    }
	}
	else /* f->draw_centers */
	{
	    XDrawPoint(dpy, window, fgc, c1->x, c1->y);
	}
    }
}

struct state {
  Display *dpy;
  Window window;

  unsigned int max_cycles;
  int growth_delay;
  GC fgc;
  XGCValues gcv;
  XWindowAttributes xgwa;

  struct field *f;
};


static void *
interaggregate_init (Display *dpy, Window window)
{
    struct state *st = (struct state *) calloc (1, sizeof(*st));

#ifdef TIME_ME
    int frames;
    struct timeval tm1, tm2;
    double tdiff;
#endif

    st->dpy = dpy;
    st->window = window;
    st->f = init_field();
    st->growth_delay = (get_integer_resource(st->dpy, "growthDelay", "Integer"));
    st->max_cycles = (get_integer_resource(st->dpy, "maxCycles", "Integer"));
    st->f->num_circles = (get_integer_resource(st->dpy, "numCircles", "Integer"));
    st->f->percent_orbits = (get_integer_resource(st->dpy, "percentOrbits", "Integer"));
    st->f->base_orbits = (get_integer_resource(st->dpy, "baseOrbits", "Integer"));
    st->f->base_on_center = (get_boolean_resource(st->dpy, "baseOnCenter", "Boolean"));
    st->f->draw_centers = (get_boolean_resource(st->dpy, "drawCenters", "Boolean"));

    if (st->f->num_circles <= 1) 
    {
        fprintf(stderr, "%s: Minimum number of circles is 2\n", 
                progname);
        exit (1);
    }

    if ( (st->f->percent_orbits < 0) || (st->f->percent_orbits > 100) )
    {
        fprintf(stderr, "%s: percent-oribts must be between 0 and 100\n", 
                progname);
        exit (1);
    }

    if ( (st->f->base_orbits < 0) || (st->f->base_orbits > 100) )
    {
        fprintf(stderr, "%s: base-oribts must be between 0 and 100\n", 
                progname);
        exit (1);
    }

    if ( st->f->percent_orbits == 100 )
	st->f->base_on_center = True;

    XGetWindowAttributes(st->dpy, st->window, &st->xgwa);

    build_colors(st->f, st->dpy, &st->xgwa);

    st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                        "foreground", "Foreground");
    st->gcv.background = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                        "background", "Background");

    st->fgc = XCreateGC(st->dpy, st->window, GCForeground, &st->gcv);

    st->f->height = st->xgwa.height;
    st->f->width = st->xgwa.width;
    st->f->visdepth = st->xgwa.depth;
    st->f->fgcolor = st->gcv.foreground;
    st->f->bgcolor = st->gcv.background;

    /* Initialize stuff */
    build_field(st->dpy, st->window, st->xgwa, st->fgc, st->f);

#ifdef TIME_ME
    gettimeofday(&tm1, NULL);
    frames = 0;
#endif

    return st;
}


static unsigned long
interaggregate_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if ((st->f->cycles % 10) == 0) 
    {
      /* Restart if the window size changes */
      XGetWindowAttributes(st->dpy, st->window, &st->xgwa);

      if (st->f->height != st->xgwa.height || st->f->width != st->xgwa.width) 
        {
          st->f->height = st->xgwa.height;
          st->f->width = st->xgwa.width;
          st->f->visdepth = st->xgwa.depth;

          build_field(st->dpy, st->window, st->xgwa, st->fgc, st->f);
          XSetForeground(st->dpy, st->fgc, st->gcv.background);
          XFillRectangle(st->dpy, st->window, st->fgc, 0, 0, st->xgwa.width, st->xgwa.height);
          XSetForeground(st->dpy, st->fgc, st->gcv.foreground);
        }
    }

  moveCircles(st->f);
  drawIntersections(st->dpy, st->window, st->fgc, st->f);

  st->f->cycles++;


  if (st->f->cycles >= st->max_cycles && st->max_cycles != 0)
    {
      build_field(st->dpy, st->window, st->xgwa, st->fgc, st->f);
      XSetForeground(st->dpy, st->fgc, st->gcv.background);
      XFillRectangle(st->dpy, st->window, st->fgc, 0, 0, st->xgwa.width, st->xgwa.height);
      XSetForeground(st->dpy, st->fgc, st->gcv.foreground);
    }

#ifdef TIME_ME
  frames++;
  gettimeofday(&tm2, NULL);

  tdiff = (tm2.tv_sec - tm1.tv_sec) 
    + (tm2.tv_usec - tm1.tv_usec) * 0.00001;

  if ( tdiff > 1 )
    {
      fprintf(stderr, "fps: %d %f %f\n", 
              frames, tdiff, frames / tdiff );

      fprintf(stderr, "intersections: %d %d %f\n", 
              f->intersection_count, f->possible_intersections, 
              ((double)f->intersection_count) / 
              f->possible_intersections);

      fprintf(stderr, "fpi: %f\n", 
              ((double)frames) / f->intersection_count );

      frames = 0;
      tm1.tv_sec = tm2.tv_sec;
      tm1.tv_usec = tm2.tv_usec;

      f->intersection_count = f->possible_intersections = 0;
    }
#endif

  return st->growth_delay;
}


static void
interaggregate_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
interaggregate_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->f->height--;  /* act like a resize */
      return True;
    }
  return False;
}

static void
interaggregate_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->f) {
    free_circles (st->f);
    if (st->f->off_img) free (st->f->off_img);
    if (st->f->parsedcolors) free (st->f->parsedcolors);
    free (st->f);
  }
  if (st->fgc) XFreeGC (st->dpy, st->fgc);
  free (st);
}


XSCREENSAVER_MODULE ("Interaggregate", interaggregate)
