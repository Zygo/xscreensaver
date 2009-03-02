/* -*- mode: C; tab-width: 4 -*-
 * xscreensaver, Copyright (c) 1992-2005 Jamie Zawinski <jwz@jwz.org>
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
 *    -large speed values leaves the image distorted
 * program idea borrowed from a screensaver on a non-*NIX OS,
 *
 * 28 Sep 1999 Jonas Munsin (jmunsin@iki.fi)
 *    Added about 10x faster algortim for 8, 16 and 32 bpp (modifies pixels
 *    directly avoiding costly XPutPixle(XGetPixel()) calls, inspired by
 *    xwhirl made by horvai@clipper.ens.fr (Peter Horvai) and the XFree86
 *    Xlib sources.
 *    This piece of code is really horrible, but it works, and at the moment
 *    I don't have time or inspiration to fix something that works (knock
 *    on wood).
 * 08 Oct 1999 Jonas Munsin (jmunsin@iki.fi)
 *	Corrected several bugs causing references beyond allocated memory.
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>
#include <X11/Xmd.h>

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
static Bool use_shm = False;
static XShmSegmentInfo shm_info;
#endif /* HAVE_XSHM_EXTENSION */

struct coo {
	int x;
	int y;
	int r, r_change;
	int xmove, ymove;
};
static struct coo xy_coo[10];

static int delay, radius, speed, number, blackhole, vortex, magnify, reflect, slow;
static XWindowAttributes xgwa;
static GC gc;
static Window g_window;
static Display *g_dpy;
static unsigned long black_pixel;

static XImage *orig_map, *buffer_map;
static unsigned long *buffer_map_cache;

static int ***from;
static int ****from_array;
static int *fast_from = NULL;
static void (*effect) (int) = NULL;
static void move_lense(int);
static void swamp_thing(int);
static void new_rnd_coo(int);
static void init_round_lense(void);
static void (*draw) (int) = NULL;
static void reflect_draw(int);
static void plain_draw(int);

static void (*draw_routine)(XImage *, XImage *, int, int, int *) = NULL;
static void fast_draw_8(XImage *, XImage *, int, int, int *);
static void fast_draw_16(XImage *, XImage *, int, int, int *);
static void fast_draw_32(XImage *, XImage *, int, int, int *);
static void generic_draw(XImage *, XImage *, int, int, int *);
static int bpp_size = 0;


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
	slow = get_boolean_resource("slow", "Boolean");
	
	if (get_boolean_resource("swamp", "Boolean"))
		effect = &swamp_thing;
	if (get_boolean_resource("bounce", "Boolean"))
		effect = &move_lense;

	XGetWindowAttributes (dpy, window, &xgwa);

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
 * -radius 80 -number 1 -speed 2 -reflect
 * -radius 50 -number 3 -speed 2 -reflect
 * jwz: not these
 *   -radius 50 -number 4 -speed 2 -swamp
 *   -radius 50 -number 4 -speed 2 -swamp -blackhole
 *   -radius 50 -number 4 -speed 2 -swamp -vortex
 *   -radius 50 -number 4 -speed 2 -swamp -vortex -magnify
 *   -radius 50 -number 4 -speed 2 -swamp -vortex -magnify -blackhole
 */
		
		i = (random() % 12 /* 17 */);

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
				radius=80;number=1;speed=2;reflect=1;
				draw = &reflect_draw;effect = &move_lense;break;
			case 11:
				radius=50;number=4;speed=2;reflect=1;
				draw = &reflect_draw;effect = &move_lense;break;

#if 0 /* jwz: not these */
			case 12:
				radius=50;number=4;speed=2;
				effect=&swamp_thing;break;
			case 13:
				radius=50;number=4;speed=2;blackhole=1;
				effect=&swamp_thing;break;
			case 14:
				radius=50;number=4;speed=2;vortex=1;
				effect=&swamp_thing;break;
			case 15:
				radius=50;number=4;speed=2;vortex=1;magnify=1;
				effect=&swamp_thing;break;
			case 16:
				radius=50;number=4;speed=2;vortex=1;magnify=1;blackhole=1;
				effect=&swamp_thing;break;
#endif

            default:
                abort(); break;
		}

        /* but if the window is small, reduce default radius */
        if (xgwa.width < radius * 8)
          radius = xgwa.width/8;
	}

    /* never allow the radius to be too close to the min window dimension
     */
    if (radius >= xgwa.width  * 0.45) radius = xgwa.width  * 0.45;
    if (radius >= xgwa.height * 0.45) radius = xgwa.height * 0.45;


    /* -swamp mode consumes vast amounts of memory, proportional to radius --
       so throttle radius to a small-ish value (60 => ~30MB.)
     */
    if (effect == &swamp_thing && radius > 60)
      radius = 60;

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

	black_pixel = BlackPixelOfScreen( xgwa.screen );

	gcv.function = GXcopy;
	gcv.subwindow_mode = IncludeInferiors;
	gcflags = GCForeground |GCFunction;
	if (use_subwindow_mode_p(xgwa.screen, window)) /* see grabscreen.c */
		gcflags |= GCSubwindowMode;
	gc = XCreateGC (dpy, window, gcflags, &gcv);

    load_random_image (xgwa.screen, window, window, NULL, NULL);

	buffer_map = 0;
	orig_map = XGetImage(dpy, window, 0, 0, xgwa.width, xgwa.height,
						 ~0L, ZPixmap);
	buffer_map_cache = malloc(sizeof(unsigned long)*(2*radius+speed+2)*(2*radius+speed+2));

	if (buffer_map_cache == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}

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

	if ((buffer_map->byte_order == orig_map->byte_order)
			&& (buffer_map->depth == orig_map->depth)
			&& (buffer_map->format == ZPixmap)
			&& (orig_map->format == ZPixmap)
			&& !slow) {
		switch (orig_map->bits_per_pixel) {
			case 32:
				draw_routine = &fast_draw_32;
				bpp_size = sizeof(CARD32);
				break;
			case 16:
				draw_routine = &fast_draw_16;
				bpp_size = sizeof(CARD16);
				break;
			case 8:
				draw_routine = &fast_draw_8;
				bpp_size = sizeof(CARD8);
				break;
			default:
				draw_routine = &generic_draw;
				break;
		}
	} else {
		draw_routine = &generic_draw;
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
static void convert(void) {
	int *p;
	int i, j;
	fast_from = calloc(1, sizeof(int)*((buffer_map->bytes_per_line/bpp_size)*(2*radius+speed+2) + 2*radius+speed+2));
	if (fast_from == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}
	p = fast_from;
	for (i = 0; i < 2*radius+speed+2; i++) {
		for (j = 0; j < 2*radius+speed+2; j++) {
			*(p + i + j*buffer_map->bytes_per_line/bpp_size)
				= from[i][j][0] + xgwa.width*from[i][j][1];
			if (*(p + i + j*buffer_map->bytes_per_line/bpp_size) < 0
					|| *(p + i + j*buffer_map->bytes_per_line/bpp_size) >= orig_map->height*orig_map->width) {
				*(p + i + j*buffer_map->bytes_per_line/bpp_size) = 0;
			}
		}
	}
}

/* makes a lense with the Radius=loop and centred in
 * the point (radius, radius)
 */
static void make_round_lense(int radius, int loop)
{
	int i, j;

	for (i = 0; i < 2*radius+speed+2; i++) {
		for(j = 0; j < ((0 == bpp_size) ? (2*radius+speed+2) : (buffer_map->bytes_per_line/bpp_size)); j++) {
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
		/* 5 is just a constant used because it looks good :) */
					angle = 5*(1-d)*(1-d);

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

	/* this is really just a quick hack to keep both the compability mode with all depths and still
	 * allow the custom optimized draw routines with the minimum amount of work */
	if (0 != bpp_size) {
		convert();
	}
}

#ifndef EXIT_FAILURE
# define EXIT_FAILURE -1
#endif

static void allocate_lense(void)
{
	int i, j;
	int s = ((0 != bpp_size) ? (buffer_map->bytes_per_line/bpp_size) : (2*radius+speed+2));
	/* maybe this should be redone so that from[][][] is in one block;
	 * then pointers could be used instead of arrays in some places (and
	 * maybe give a speedup - maybe also consume less memory)
	 */
	from = (int ***)malloc(s*sizeof(int **));
	if (from == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < s; i++) {
		from[i] = (int **)malloc((2*radius+speed+2) * sizeof(int *));
		if (from[i] == NULL) {
			perror("distort");
			exit(EXIT_FAILURE);
		}
		for (j = 0; j < s; j++) {
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

/* If fast_draw_8, fast_draw_16 or fast_draw_32 are to be used, the following properties
 * of the src and dest XImages must hold (otherwise the generic, slooow, method provided
 * by X is to be used):
 *	src->byte_order == dest->byte_order
 *	src->format == ZPixmap && dest->format == ZPixmap
 *	src->depth == dest->depth == the depth the function in question asumes
 * x and y is the coordinates in src from where to cut out the image from,
 * distort_matrix is a precalculated array of how to distort the matrix
 */

static void fast_draw_8(XImage *src, XImage *dest, int x, int y, int *distort_matrix) {
	CARD8 *u = (CARD8 *)dest->data;
	CARD8 *t = (CARD8 *)src->data + x + y*src->bytes_per_line/sizeof(CARD8);

	while (u < (CARD8 *)(dest->data + sizeof(CARD8)*dest->height
				*dest->bytes_per_line/sizeof(CARD8))) {
		*u++ = t[*distort_matrix++];
	}
}

static void fast_draw_16(XImage *src, XImage *dest, int x, int y, int *distort_matrix) {
	CARD16 *u = (CARD16 *)dest->data;
	CARD16 *t = (CARD16 *)src->data + x + y*src->bytes_per_line/sizeof(CARD16);

	while (u < (CARD16 *)(dest->data + sizeof(CARD16)*dest->height
				*dest->bytes_per_line/sizeof(CARD16))) {
		*u++ = t[*distort_matrix++];
	}
}

static void fast_draw_32(XImage *src, XImage *dest, int x, int y, int *distort_matrix) {
	CARD32 *u = (CARD32 *)dest->data;
	CARD32 *t = (CARD32 *)src->data + x + y*src->bytes_per_line/sizeof(CARD32);

	while (u < (CARD32 *)(dest->data + sizeof(CARD32)*dest->height
				*dest->bytes_per_line/sizeof(CARD32))) {
		*u++ = t[*distort_matrix++];
	}
}

static void generic_draw(XImage *src, XImage *dest, int x, int y, int *distort_matrix) {
	int i, j;
	for (i = 0; i < dest->width; i++)
		for (j = 0; j < dest->height; j++)
			if (from[i][j][0] + x >= 0 &&
					from[i][j][0] + x < src->width &&
					from[i][j][1] + y >= 0 &&
					from[i][j][1] + y < src->height)
				XPutPixel(dest, i, j,
						XGetPixel(src,
							from[i][j][0] + x,
							from[i][j][1] + y));
}

/* generate an XImage of from[][][] and draw it on the screen */
static void plain_draw(int k)
{
	if (xy_coo[k].x+2*radius+speed+2 > orig_map->width ||
			xy_coo[k].y+2*radius+speed+2 > orig_map->height)
		return;

	draw_routine(orig_map, buffer_map, xy_coo[k].x, xy_coo[k].y, fast_from);

# ifdef HAVE_XSHM_EXTENSION
	if (use_shm)
		XShmPutImage(g_dpy, g_window, gc, buffer_map, 0, 0, xy_coo[k].x, xy_coo[k].y,
				2*radius+speed+2, 2*radius+speed+2, False);
	else

	if (!use_shm)
# endif
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
		if (ny >= orig_map->height) ny = orig_map->height-1;
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

	/* bounce against othe lenses */
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

	"*delay:			20000",
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
	{ "-slow",	".slow",	XrmoptionNoArg, "True" },
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
