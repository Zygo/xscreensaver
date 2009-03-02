/* -*- mode: C; tab-width: 4 -*-
 * xscreensaver, Copyright (c) 1992, 1993, 1994, 1996, 1997, 1998
 * Jamie Zawinski <jwz@jwz.org>
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
 * by Jonas Munsin (jmunsin@iki.fi) and Jamie Zawinski <jwz@jwz.org>
 * TODO:
 *	-check the allocations in init_round_lense again, maybe make it possible again
 * 	 to use swamp without pre-allocating/calculating (although that
 *	 makes it slower) - -swamp is memory hungry
 *	-more distortion matrices (fortunately, I'm out of ideas :)
 * Stuff that would be cool but probably too much of a resource hog:
 *	-some kind of interpolation to avoid jaggies
 * program idea borrowed from a screensaver on a non-*NIX OS,
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
static Bool use_shm;
static XShmSegmentInfo shm_info;
#endif /* HAVE_XSHM_EXTENSION */

struct coo {
	int x;
	int y;
	int r, r_change;
	int xmove, ymove;
};
static struct coo xy_coo[10];

static int delay, radius, speed, number, blackhole, vortex, magnify, reflect;
static XWindowAttributes xgwa;
static GC gc;
static Window g_window;
static Display *g_dpy;
static unsigned long black_pixel;

static XImage *orig_map, *buffer_map;

static int ***from;
static int ****from_array;
static void (*effect) (int) = NULL;
static void move_lense(int);
static void swamp_thing(int);
static void new_rnd_coo(int);
static void init_round_lense(void);
static void (*draw) (int) = NULL;
static void reflect_draw(int);
static void plain_draw(int);

static void init_distort(Display *dpy, Window window) 
{
	XGCValues gcv;
	long gcflags;
	int i;

	g_window=window;
	g_dpy=dpy;

	delay = get_integer_resource("delay", "Integer");
	radius = get_integer_resource("radius", "Integer");
	speed = get_integer_resource("speed", "Integer");
	number = get_integer_resource("number", "Integer");

#ifdef HAVE_XSHM_EXTENSION
	use_shm = get_boolean_resource("useSHM", "Boolean");
#endif /* HAVE_XSHM_EXTENSION */
	
	blackhole = get_boolean_resource("blackhole", "Boolean");
	vortex = get_boolean_resource("vortex", "Boolean");
	magnify = get_boolean_resource("magnify", "Boolean");
	reflect = get_boolean_resource("reflect", "Boolean");
	
	if (get_boolean_resource("swamp", "Boolean"))
		effect = &swamp_thing;
	if (get_boolean_resource("bounce", "Boolean"))
		effect = &move_lense;

	if (effect == NULL && radius == 0 && speed == 0 && number == 0
		&& !blackhole && !vortex && !magnify && !reflect) {
/* if no cmdline options are given, randomly choose one of:
 * -radius 50 -number 4 -speed 1 -bounce
 * -radius 50 -number 4 -speed 1 -blackhole
 * -radius 50 -number 4 -speed 1 -vortex
 * -radius 50 -number 4 -speed 1 -vortex -magnify
 * -radius 50 -number 4 -speed 1 -vortex -magnify -blackhole
 * -radius 100 -number 1 -speed 2 -bounce
 * -radius 100 -number 1 -speed 2 -blackhole
 * -radius 100 -number 1 -speed 2 -vortex
 * -radius 100 -number 1 -speed 2 -vortex -magnify
 * -radius 100 -number 1 -speed 2 -vortex -magnify -blackhole
 * -radius 50 -number 4 -speed 2 -swamp
 * -radius 50 -number 4 -speed 2 -swamp -blackhole
 * -radius 50 -number 4 -speed 2 -swamp -vortex
 * -radius 50 -number 4 -speed 2 -swamp -vortex -magnify
 * -radius 50 -number 4 -speed 2 -swamp -vortex -magnify -blackhole
 * -radius 80 -number 1 -speed 2 -reflect
 * -radius 50 -number 3 -speed 2 -reflect
 */
		
		i = (random() % 17);

		draw = &plain_draw;

		switch (i) {
			case 0:
				radius=50;number=4;speed=1;
				effect=&move_lense;break;
			case 1:
				radius=50;number=4;speed=1;blackhole=1;
				effect=&move_lense;break;
			case 2:
				radius=50;number=4;speed=1;vortex=1;
				effect=&move_lense;break;
			case 3:
				radius=50;number=4;speed=1;vortex=1;magnify=1;
				effect=&move_lense;break;
			case 4:
				radius=50;number=4;speed=1;vortex=1;magnify=1;blackhole=1;
				effect=&move_lense;break;
			case 5:
				radius=100;number=1;speed=2;
				effect=&move_lense;break;
			case 6:
				radius=100;number=1;speed=2;blackhole=1;
				effect=&move_lense;break;
			case 7:
				radius=100;number=1;speed=2;vortex=1;
				effect=&move_lense;break;
			case 8:
				radius=100;number=1;speed=2;vortex=1;magnify=1;
				effect=&move_lense;break;
			case 9:
				radius=100;number=1;speed=2;vortex=1;magnify=1;blackhole=1;
				effect=&move_lense;break;
			case 10:
				radius=50;number=4;speed=2;
				effect=&swamp_thing;break;
			case 11:
				radius=50;number=4;speed=2;blackhole=1;
				effect=&swamp_thing;break;
			case 12:
				radius=50;number=4;speed=2;vortex=1;
				effect=&swamp_thing;break;
			case 13:
				radius=50;number=4;speed=2;vortex=1;magnify=1;
				effect=&swamp_thing;break;
			case 14:
				radius=50;number=4;speed=2;vortex=1;magnify=1;blackhole=1;
				effect=&swamp_thing;break;
			case 15:
				radius=80;number=1;speed=2;reflect=1;
				draw = &reflect_draw;effect = &move_lense;break;
			case 16: default:
				radius=50;number=4;speed=2;reflect=1;
				draw = &reflect_draw;effect = &move_lense;break;
		}

	}

	if (delay < 0)
		delay = 0;
	if (radius <= 0)
		radius = 60;
	if (speed <= 0) 
		speed = 2;
	if (number <= 0)
		number=1;
	if (number >= 10)
		number=1;
	if (effect == NULL)
		effect = &move_lense;
	if (reflect) {
		draw = &reflect_draw;
		effect = &move_lense;
	}
	if (draw == NULL)
		draw = &plain_draw;

	XGetWindowAttributes (dpy, window, &xgwa);
	black_pixel = BlackPixelOfScreen( xgwa.screen );

	gcv.function = GXcopy;
	gcv.subwindow_mode = IncludeInferiors;
	gcflags = GCForeground |GCFunction;
	if (use_subwindow_mode_p(xgwa.screen, window)) /* see grabscreen.c */
		gcflags |= GCSubwindowMode;
	gc = XCreateGC (dpy, window, gcflags, &gcv);

	grab_screen_image (xgwa.screen, window);

	buffer_map = 0;
	orig_map = XGetImage(dpy, window, 0, 0, xgwa.width, xgwa.height,
						 ~0L, ZPixmap);

# ifdef HAVE_XSHM_EXTENSION

	if (use_shm)
	  {
		buffer_map = create_xshm_image(dpy, xgwa.visual, orig_map->depth,
									   ZPixmap, 0, &shm_info,
									   2*radius + speed + 2,
									   2*radius + speed + 2);
		if (!buffer_map)
		  use_shm = False;
	  }
# endif /* HAVE_XSHM_EXTENSION */

	if (!buffer_map)
	  {
		buffer_map = XCreateImage(dpy, xgwa.visual,
								  orig_map->depth, ZPixmap, 0, 0,
								  2*radius + speed + 2, 2*radius + speed + 2,
								  8, 0);
		buffer_map->data = (char *)
		  calloc(buffer_map->height, buffer_map->bytes_per_line);
	}

	init_round_lense();

	for (i = 0; i < number; i++) {
		new_rnd_coo(i);
		if (number != 1)
			xy_coo[i].r = (i*radius)/(number-1); /* "randomize" initial */
		else
			 xy_coo[i].r = 0;
		xy_coo[i].r_change = speed + (i%2)*2*(-speed);	/* values a bit */
		xy_coo[i].xmove = speed + (i%2)*2*(-speed);
		xy_coo[i].ymove = speed + (i%2)*2*(-speed);
	}
}

/* example: initializes a "see-trough" matrix */
/* static void make_null_lense(void)
{
	int i, j;
	for (i = 0; i < 2*radius+speed+2; i++) {
		for (j = 0 ; j < 2*radius+speed+2 ; j++) {
			from[i][j][0]=i;
			from[i][j][1]=j;
		}
	} 
}
*/

/* makes a lense with the Radius=loop and centred in
 * the point (radius, radius)
 */
static void make_round_lense(int radius, int loop)
{
	int i, j;

	for (i = 0; i < 2*radius+speed+2; i++) {
		for(j = 0; j < 2*radius+speed+2; j++) {
			double r, d;
			r = sqrt ((i-radius)*(i-radius)+(j-radius)*(j-radius));
			if (loop == 0)
			  d=0.0;
			else
			  d=r/loop;

			if (r < loop-1) {

				if (vortex) { /* vortex-twist effect */
					double angle;
		/* this one-line formula for getting a nice rotation angle is borrowed
		 * (with permission) from the whirl plugin for gimp,
		 * Copyright (C) 1996 Federico Mena Quintero
		 */
		/* 2.5 is just a constant used because it looks good :) */
					angle = 2.5*(1-d)*(1-d);

        /* Avoid atan2: DOMAIN error message */
					if ((radius-j) == 0.0 && (radius-i) == 0.0) {
						from[i][j][0] = radius + cos(angle)*r;
						from[i][j][1] = radius + sin(angle)*r;
					} else {
      				  	from[i][j][0] = radius +
									cos(angle - atan2(radius-j, -(radius-i)))*r;
			        	from[i][j][1] = radius +
									sin(angle - atan2(radius-j, -(radius-i)))*r;
					}
					if (magnify) {
						r = sin(d*M_PI_2);
						if (blackhole && r != 0) /* blackhole effect */
							r = 1/r;
						from[i][j][0] = radius + (from[i][j][0]-radius)*r;
						from[i][j][1] = radius + (from[i][j][1]-radius)*r;
					}
				} else { /* default is to magnify */
					r = sin(d*M_PI_2);
				
	/* raising r to different power here gives different amounts of
	 * distortion, a negative value sucks everything into a black hole
	 */
				/*	r = r*r; */
					if (blackhole && r != 0) /* blackhole effect */
						r = 1/r;
									/* bubble effect (and blackhole) */
					from[i][j][0] = radius + (i-radius)*r;
					from[i][j][1] = radius + (j-radius)*r;
				}
			} else { /* not inside loop */
				from[i][j][0] = i;
				from[i][j][1] = j;
			}
		}
	}
}

#ifndef EXIT_FAILURE
# define EXIT_FAILURE -1
#endif

static void allocate_lense(void)
{
	int i, j;
	/* maybe this should be redone so that from[][][] is in one block;
	 * then pointers could be used instead of arrays in some places (and
	 * maybe give a speedup - maybe also consume less memory)
	 */

	from = (int ***)malloc((2*radius+speed+2) * sizeof(int **));
	if (from == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < 2*radius+speed+2; i++) {
		from[i] = (int **)malloc((2*radius+speed+2) * sizeof(int *));
		if (from[i] == NULL) {
			perror("distort");
			exit(EXIT_FAILURE);
		}
		for (j = 0; j < 2*radius+speed+2; j++) {
			from[i][j] = (int *)malloc(2 * sizeof(int));
			if (from[i][j] == NULL) {
				perror("distort");
				exit(EXIT_FAILURE);
			}
		}
	}
}

/* from_array in an array containing precalculated from matrices,
 * this is a double faced mem vs speed trade, it's faster, but eats
 * _a lot_ of mem for large radius (is there a bug here? I can't see it)
 */
static void init_round_lense(void)
{
	int k;

	if (effect == &swamp_thing) {
		from_array = (int ****)malloc((radius+1)*sizeof(int ***));
		for (k=0; k <= radius; k++) {
			allocate_lense();
			make_round_lense(radius, k);
			from_array[k] = from;
		}
	} else { /* just allocate one from[][][] */
		allocate_lense();
		make_round_lense(radius,radius);
	}
}


/* generate an XImage of from[][][] and draw it on the screen */
static void plain_draw(int k)
{
	int i, j;
	for(i = 0 ; i < 2*radius+speed+2; i++) {
		for(j = 0 ; j < 2*radius+speed+2 ; j++) {
			if (xy_coo[k].x+from[i][j][0] >= 0 &&
					xy_coo[k].x+from[i][j][0] < xgwa.width &&
					xy_coo[k].y+from[i][j][1] >= 0 &&
					xy_coo[k].y+from[i][j][1] < xgwa.height)
				XPutPixel(buffer_map, i, j,
						XGetPixel(orig_map,
							xy_coo[k].x+from[i][j][0],
							xy_coo[k].y+from[i][j][1]));
		}
	}

	XPutImage(g_dpy, g_window, gc, buffer_map, 0, 0, xy_coo[k].x, xy_coo[k].y,
			2*radius+speed+2, 2*radius+speed+2);
}

/* generate an XImage from the reflect algoritm submitted by
 * Randy Zack <randy@acucorp.com>
 * draw really got too big and ugly so I split it up
 * it should be possible to use the from[][] to speed it up
 * (once I figure out the algorithm used :)
 */
static void reflect_draw(int k)
{
	int i, j;
	int	cx, cy;
	int	ly, lysq, lx, ny, dist, rsq = radius * radius;

	cx = cy = radius;
	if (xy_coo[k].ymove > 0)
		cy += speed;
	if (xy_coo[k].xmove > 0)
		cx += speed;

	for(i = 0 ; i < 2*radius+speed+2; i++) {
		ly = i - cy;
		lysq = ly * ly;
		ny = xy_coo[k].y + i;
		for(j = 0 ; j < 2*radius+speed+2 ; j++) {
			lx = j - cx;
			dist = lx * lx + lysq;
			if (dist > rsq ||
				ly < -radius || ly > radius ||
				lx < -radius || lx > radius)
				XPutPixel( buffer_map, j, i,
						   XGetPixel( orig_map, xy_coo[k].x + j, ny ));
			else if (dist == 0)
				XPutPixel( buffer_map, j, i, black_pixel );
			else {
				int	x = xy_coo[k].x + cx + (lx * rsq / dist);
				int	y = xy_coo[k].y + cy + (ly * rsq / dist);
				if (x < 0 || x >= xgwa.width ||
					y < 0 || y >= xgwa.height)
					XPutPixel( buffer_map, j, i, black_pixel );
				else
					XPutPixel( buffer_map, j, i,
							   XGetPixel( orig_map, x, y ));
			}
		}
	}

	XPutImage(g_dpy, g_window, gc, buffer_map, 0, 0, xy_coo[k].x, xy_coo[k].y,
			2*radius+speed+2, 2*radius+speed+2);
}

/* create a new, random coordinate, that won't interfer with any other
 * coordinates, as the drawing routines would be significantly slowed
 * down if they were to handle serveral layers of distortions
 */
static void new_rnd_coo(int k)
{
	int i;

	xy_coo[k].x = (random() % (xgwa.width-2*radius));
	xy_coo[k].y = (random() % (xgwa.height-2*radius));
	
	for (i = 0; i < number; i++) {
		if (i != k) {
			if ((abs(xy_coo[k].x - xy_coo[i].x) <= 2*radius+speed+2)
			 && (abs(xy_coo[k].y - xy_coo[i].y) <= 2*radius+speed+2)) {
				xy_coo[k].x = (random() % (xgwa.width-2*radius));
				xy_coo[k].y = (random() % (xgwa.height-2*radius));
				i=-1; /* ugly */
			} 
		}
	}
}

/* move lens and handle bounces with walls and other lenses */
static void move_lense(int k)
{
	int i;

	if (xy_coo[k].x + 2*radius + speed + 2 >= xgwa.width)
		xy_coo[k].xmove = -abs(xy_coo[k].xmove);
	if (xy_coo[k].x <= speed) 
		xy_coo[k].xmove = abs(xy_coo[k].xmove);
	if (xy_coo[k].y + 2*radius + speed + 2 >= xgwa.height)
		xy_coo[k].ymove = -abs(xy_coo[k].ymove);
	if (xy_coo[k].y <= speed)
		xy_coo[k].ymove = abs(xy_coo[k].ymove);

	xy_coo[k].x = xy_coo[k].x + xy_coo[k].xmove;
	xy_coo[k].y = xy_coo[k].y + xy_coo[k].ymove;

	for (i = 0; i < number; i++) {
		if ((i != k)
		
/* This commented test is for rectangular lenses (not currently used) and
 * the one used is for circular ones
		&& (abs(xy_coo[k].x - xy_coo[i].x) <= 2*radius)
		&& (abs(xy_coo[k].y - xy_coo[i].y) <= 2*radius)) { */

		&& ((xy_coo[k].x - xy_coo[i].x)*(xy_coo[k].x - xy_coo[i].x)
		  + (xy_coo[k].y - xy_coo[i].y)*(xy_coo[k].y - xy_coo[i].y)
			<= 2*radius*2*radius)) {

			int x, y;
			x = xy_coo[k].xmove;
			y = xy_coo[k].ymove;
			xy_coo[k].xmove = xy_coo[i].xmove;
			xy_coo[k].ymove = xy_coo[i].ymove;
			xy_coo[i].xmove = x;
			xy_coo[i].ymove = y;
		}
	}

}

/* make xy_coo[k] grow/shrink */
static void swamp_thing(int k)
{
	if (xy_coo[k].r >= radius)
		xy_coo[k].r_change = -abs(xy_coo[k].r_change);
	
	if (xy_coo[k].r <= 0) {
		from = from_array[0];
		draw(k); 
		xy_coo[k].r_change = abs(xy_coo[k].r_change);
		new_rnd_coo(k);
		xy_coo[k].r=xy_coo[k].r_change;
		return;
	}

	xy_coo[k].r = xy_coo[k].r + xy_coo[k].r_change;

	if (xy_coo[k].r >= radius)
		xy_coo[k].r = radius;
	if (xy_coo[k].r <= 0)
		xy_coo[k].r=0;

	from = from_array[xy_coo[k].r];
}




char *progclass = "Distort";

char *defaults [] = {
	"*dontClearRoot:		True",
#ifdef __sgi    /* really, HAVE_READ_DISPLAY_EXTENSION */
	"*visualID:			Best",
#endif

	"*delay:			10000",
	"*radius:			0",
	"*speed:			0",
	"*number:			0",
	"*vortex:			False",
	"*magnify:			False",
	"*swamp:			False",
	"*bounce:			False",
	"*reflect:			False",
	"*blackhole:		False",
#ifdef HAVE_XSHM_EXTENSION
	"*useSHM:			False",		/* xshm turns out not to help. */
#endif /* HAVE_XSHM_EXTENSION */
	0
};

XrmOptionDescRec options [] = {
	{ "-delay",	".delay",	XrmoptionSepArg, 0 },
	{ "-radius",	".radius",	XrmoptionSepArg, 0 },
	{ "-speed",	".speed",	XrmoptionSepArg, 0 },
	{ "-number",	".number",	XrmoptionSepArg, 0 },
	{ "-swamp",	".swamp",	XrmoptionNoArg, "True" },
	{ "-bounce",	".bounce",	XrmoptionNoArg, "True" },
	{ "-reflect",	".reflect",	XrmoptionNoArg, "True" },
	{ "-vortex",	".vortex",	XrmoptionNoArg, "True" },
	{ "-magnify",	".magnify",	XrmoptionNoArg, "True" },
	{ "-blackhole",	".blackhole",	XrmoptionNoArg, "True" },
#ifdef HAVE_XSHM_EXTENSION
	{ "-shm",		".useSHM",	XrmoptionNoArg, "True" },
	{ "-no-shm",	".useSHM",	XrmoptionNoArg, "False" },
#endif /* HAVE_XSHM_EXTENSION */
	{ 0, 0, 0, 0 }
};


void screenhack(Display *dpy, Window window)
{
	int k;

	init_distort (dpy, window);
	while (1) {
		for (k = 0; k < number; k++) {
			effect(k);
			draw(k);
		}

		XSync(dpy, False);
        screenhack_handle_events (dpy);
		if (delay) usleep(delay);
	}

}
