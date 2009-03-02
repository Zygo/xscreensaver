/* xscreensaver, Copyright (c) 1992, 1993, 1994, 1996, 1997, 1998
 * Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* distort
 * by Jonas Munsin (jmunsin@iki.fi)
 * it's a bit of a resource hog at the moment
 * TODO:
 *	-optimize for speed
 *	-mutiple spheres/lenses (with bounces/layering)
 *	-different distortion matrices
 *	-randomize movement a bit
 * program idea borrowed from a screensaver on a non-*NIX OS,
 * code based on decayscreen by Jamie Zawinski
 */

#include <math.h>
#include "screenhack.h"

static int delay, radius, speed, size_x, size_y;
static XWindowAttributes xgwa;
static GC gc;
static Pixmap orig_map, buffer_map;

static int ***from;

static void init_distort (Display *dpy, Window window) {
	XGCValues gcv;
	long gcflags;
	int i, j;
    
	delay = get_integer_resource ("delay", "Integer");
	radius = get_integer_resource ("radius", "Integer");
	speed = get_integer_resource ("speed", "Integer");

	if (delay < 0)
		delay = 0;
	if (radius <= 0)
		radius = 60;
	if (speed == 0) 
		speed = 2;

	XGetWindowAttributes (dpy, window, &xgwa);

	gcv.function = GXcopy;
	gcv.subwindow_mode = IncludeInferiors;
	gcflags = GCForeground |GCFunction;
	if (use_subwindow_mode_p(xgwa.screen, window)) /* see grabscreen.c */
		gcflags |= GCSubwindowMode;
	gc = XCreateGC (dpy, window, gcflags, &gcv);

	size_x = xgwa.width;
	size_y = xgwa.height;
    
	grab_screen_image (xgwa.screen, window);

	orig_map = XCreatePixmap(dpy, window,
			xgwa.width, xgwa.height, xgwa.depth);
	XCopyArea(dpy, window,
			orig_map, gc, 0, 0, xgwa.width, xgwa.height, 0, 0);
	buffer_map = XCreatePixmap(dpy, window,
			2*radius + speed, 2*radius + speed,
			xgwa.depth);

	from = (int ***)malloc ((2*radius+1) * sizeof(int **));
	for(i = 0; i <= 2*radius; i++) {
		from[i] = (int **)malloc((2*radius+1) * sizeof(int *));
		for (j = 0; j <= 2*radius; j++)
			from[i][j] = (int *)malloc(2*sizeof(int));
	}

	/* initialize a "see-trough" matrix */
	for (i = 0; i <= 2*radius; i++) {
		for (j = 0 ; j <= 2*radius ; j++) {
			from[i][j][0]=i-radius/2;
			from[i][j][1]=j-radius/2;
		}
	}

	/* initialize the distort matrix */
	for (i = 0; i <= 2*radius; i++) {
		for(j = 0; j <= 2*radius; j++) {
			double r;
			r = sqrt ((i-radius)*(i-radius)+(j-radius)*(j-radius));
			if (r < radius) {
				r = sin(r*(M_PI_2)/radius);
				if (i < radius)
					from[i][j][0] = radius/2 + (i-radius)*r;
				else
					from[i][j][0] = radius/2 + (i-radius)*r;
				if (j < radius)
					from[i][j][1] = radius/2 + (j-radius)*r;
				else
					from[i][j][1] = radius/2 + (j-radius)*r;
			}
		}
	}

	XSetGraphicsExposures(dpy, gc, False); /* stop events from XCopyArea */
}

static void
move_lens (int *x, int *y, int *xmove, int *ymove) {
	if (*xmove==0)
		*xmove=speed;
	if (*ymove==0)
		*ymove=speed;
	if (*x==0)
		*x = radius + (random() % (size_x-2*radius));
	if (*y==0)
		*y = radius + (random() % (size_y-2*radius));
	if (*x + 3*radius/2 >= size_x)
		*xmove = -abs(*xmove);
	if (*x - radius/2 <= 0) 
		*xmove = abs(*xmove);
	if (*y + 3*radius/2 >= size_y)
		*ymove = -abs(*ymove);
	if (*y - radius/2 <= 0)
		*ymove = abs(*ymove);

	*x = *x + *xmove;
	*y = *y + *ymove;
}

static void distort (Display *dpy, Window window)
{
	static int x, y, xmove=0, ymove=0;
	int i,j;

	move_lens (&x, &y, &xmove, &ymove);

	XCopyArea(dpy, orig_map, buffer_map, gc,
			x-radius/2 - xmove, y-radius/2 - ymove,
			2*radius + abs(xmove), 2*radius + abs(ymove),
			0,0);

	/* it's possible to lower the number of loop iterations by a factor
	 * of 4, but since it's the XCopyArea's which eat resources, and
	 * I've only supplied one distortion routine (which is circular),
	 * here's a check-if-inside circle variation of this for loop.
	 * Using both optimizations turns the matrix rendering into one
	 * ugly mess... I'm counting on gcc optimization ;)
	 */

	for(i = 0 ; i <= 2*radius ; i++) {
		for(j = 0 ; j <= 2*radius ; j++) {
			if (((radius-i)*(radius-i) + (j-radius)*(j-radius))
				< radius*radius) {
			XCopyArea (dpy, orig_map, buffer_map, gc,
					x+from[i][j][0],
					y+from[i][j][1],
					1, 1, i + xmove, j+ymove);
			}
		}
	}

	XCopyArea(dpy, buffer_map, window, gc, 0, 0,
			2*radius + abs(xmove), 2*radius + abs(ymove),
			x-radius/2 - xmove, y-radius/2 - ymove);
}



char *progclass = "Distort";

char *defaults [] = {
	"*dontClearRoot:		True",
#ifdef __sgi    /* really, HAVE_READ_DISPLAY_EXTENSION */
	"*visualID:			Best",
#endif

	"*delay:			10000",
	"*radius:			60",
	"*speed:			2",
	0
};

XrmOptionDescRec options [] = {
	{ "-delay",	".delay",	XrmoptionSepArg, 0 },
	{ "-radius",	".radius",	XrmoptionSepArg, 0 },
	{ "-speed",	".speed",	XrmoptionSepArg, 0 },
	{ 0, 0, 0, 0 }
};
		

void screenhack (Display *dpy, Window window) {
	init_distort (dpy, window);
	while (1) {
		distort (dpy, window);
		XSync (dpy, True);
		if (delay) usleep (delay);
	}
}
