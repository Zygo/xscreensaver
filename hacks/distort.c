/* -*- mode: C; tab-width: 4 -*-
 * xscreensaver, Copyright (c) 1992-2014 Jamie Zawinski <jwz@jwz.org>
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
 * 09 Oct 2016 Dave Odell (dmo2118@gmail.com)
 *  Updated for new xshm.c.
 */

#include <math.h>
#include <time.h>
#include "screenhack.h"
/*#include <X11/Xmd.h>*/
# include "xshm.h"

#define CARD32 unsigned int
#define CARD16 unsigned short
#define CARD8  unsigned char


struct coo {
	int x;
	int y;
	int r, r_change;
	int xmove, ymove;
};

struct state {
  Display *dpy;
  Window window;

  struct coo xy_coo[10];

  int delay, radius, speed, number, blackhole, vortex, magnify, reflect, slow;
  int duration;
  time_t start_time;

  XWindowAttributes xgwa;
  GC gc;
  unsigned long black_pixel;

  XImage *orig_map, *buffer_map;
  unsigned long *buffer_map_cache;

  int ***from;
  int ****from_array;
  int *fast_from;

  int bpp_size;

  XShmSegmentInfo shm_info;

  void (*effect) (struct state *, int);
  void (*draw) (struct state *, int);
  void (*draw_routine) (struct state *st, XImage *, XImage *, int, int, int *);

  async_load_state *img_loader;
  Pixmap pm;
};


static void move_lense(struct state *, int);
static void swamp_thing(struct state *, int);
static void new_rnd_coo(struct state *, int);
static void init_round_lense(struct state *st);
static void reflect_draw(struct state *, int);
static void plain_draw(struct state *, int);

static void fast_draw_8 (struct state *st, XImage *, XImage *, int, int, int *);
static void fast_draw_16(struct state *st, XImage *, XImage *, int, int, int *);
static void fast_draw_32(struct state *st, XImage *, XImage *, int, int, int *);
static void generic_draw(struct state *st, XImage *, XImage *, int, int, int *);


static void distort_finish_loading (struct state *);

static void
distort_reset (struct state *st)
{
    char *s;
    int i;

    st->start_time = 0;

	XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

	st->delay = get_integer_resource(st->dpy, "delay", "Integer");
    st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
	st->radius = get_integer_resource(st->dpy, "radius", "Integer");
	st->speed = get_integer_resource(st->dpy, "speed", "Integer");
	st->number = get_integer_resource(st->dpy, "number", "Integer");

	st->blackhole = get_boolean_resource(st->dpy, "blackhole", "Boolean");
	st->vortex = get_boolean_resource(st->dpy, "vortex", "Boolean");
	st->magnify = get_boolean_resource(st->dpy, "magnify", "Boolean");
	st->reflect = get_boolean_resource(st->dpy, "reflect", "Boolean");
	st->slow = get_boolean_resource(st->dpy, "slow", "Boolean");
	
    if (st->delay < 0) st->delay = 0;
    if (st->duration < 1) st->duration = 1;

    st->effect = NULL;
    s = get_string_resource(st->dpy, "effect", "String");
	if (s && !strcasecmp(s,"swamp"))
      st->effect = &swamp_thing;
    else if (s && !strcasecmp(s,"bounce"))
      st->effect = &move_lense;
    else if (s && !strcasecmp(s,"none"))
      ;
    else if (s && *s)
      fprintf(stderr,"%s: bogus effect: %s\n", progname, s);

	if (st->effect == NULL && st->radius == 0 && st->speed == 0 && st->number == 0
		&& !st->blackhole && !st->vortex && !st->magnify && !st->reflect) {
/* if no cmdline options are given, randomly choose one of:
 * -radius 125 -number 4 -speed 1 -bounce
 * -radius 125 -number 4 -speed 1 -blackhole
 * -radius 125 -number 4 -speed 1 -vortex
 * -radius 125 -number 4 -speed 1 -vortex -magnify
 * -radius 125 -number 4 -speed 1 -vortex -magnify -blackhole
 * -radius 250 -number 1 -speed 2 -bounce
 * -radius 250 -number 1 -speed 2 -blackhole
 * -radius 250 -number 1 -speed 2 -vortex
 * -radius 250 -number 1 -speed 2 -vortex -magnify
 * -radius 250 -number 1 -speed 2 -vortex -magnify -blackhole
 * -radius 80 -number 1 -speed 2 -reflect
 * -radius 125 -number 3 -speed 2 -reflect
 * jwz: not these
 *   -radius 125 -number 4 -speed 2 -swamp
 *   -radius 125 -number 4 -speed 2 -swamp -blackhole
 *   -radius 125 -number 4 -speed 2 -swamp -vortex
 *   -radius 125 -number 4 -speed 2 -swamp -vortex -magnify
 *   -radius 125 -number 4 -speed 2 -swamp -vortex -magnify -blackhole
 */
		
		i = (random() % 12 /* 17 */);

		st->draw = &plain_draw;

		switch (i) {
			case 0:
				st->radius=125;st->number=4;st->speed=1;
				st->effect=&move_lense;break;
			case 1:
				st->radius=125;st->number=4;st->speed=1;st->blackhole=1;
				st->effect=&move_lense;break;
			case 2:
				st->radius=125;st->number=4;st->speed=1;st->vortex=1;
				st->effect=&move_lense;break;
			case 3:
				st->radius=125;st->number=4;st->speed=1;st->vortex=1;st->magnify=1;
				st->effect=&move_lense;break;
			case 4:
				st->radius=125;st->number=4;st->speed=1;st->vortex=1;st->magnify=1;st->blackhole=1;
				st->effect=&move_lense;break;
			case 5:
				st->radius=250;st->number=1;st->speed=2;
				st->effect=&move_lense;break;
			case 6:
				st->radius=250;st->number=1;st->speed=2;st->blackhole=1;
				st->effect=&move_lense;break;
			case 7:
				st->radius=250;st->number=1;st->speed=2;st->vortex=1;
				st->effect=&move_lense;break;
			case 8:
				st->radius=250;st->number=1;st->speed=2;st->vortex=1;st->magnify=1;
				st->effect=&move_lense;break;
			case 9:
				st->radius=250;st->number=1;st->speed=2;st->vortex=1;st->magnify=1;st->blackhole=1;
				st->effect=&move_lense;break;

			case 10:
				st->radius=80;st->number=1;st->speed=2;st->reflect=1;
				st->draw = &reflect_draw;st->effect = &move_lense;break;
			case 11:
				st->radius=125;st->number=4;st->speed=2;st->reflect=1;
				st->draw = &reflect_draw;st->effect = &move_lense;break;

#if 0 /* jwz: not these */
			case 12:
				st->radius=125;st->number=4;st->speed=2;
				effect=&swamp_thing;break;
			case 13:
				st->radius=125;st->number=4;st->speed=2;st->blackhole=1;
				effect=&swamp_thing;break;
			case 14:
				st->radius=125;st->number=4;st->speed=2;st->vortex=1;
				effect=&swamp_thing;break;
			case 15:
				st->radius=125;st->number=4;st->speed=2;st->vortex=1;st->magnify=1;
				effect=&swamp_thing;break;
			case 16:
				st->radius=125;st->number=4;st->speed=2;st->vortex=1;st->magnify=1;st->blackhole=1;
				effect=&swamp_thing;break;
#endif

            default:
                abort(); break;
		}
	}

    /* never allow the radius to be too close to the min window dimension
     */
    if (st->radius > st->xgwa.width  * 0.3) st->radius = st->xgwa.width  * 0.3;
    if (st->radius > st->xgwa.height * 0.3) st->radius = st->xgwa.height * 0.3;


    /* -swamp mode consumes vast amounts of memory, proportional to radius --
       so throttle radius to a small-ish value (60 => ~30MB.)
     */
    if (st->effect == &swamp_thing && st->radius > 60)
      st->radius = 60;

	if (st->delay < 0)
		st->delay = 0;
	if (st->radius <= 0)
		st->radius = 60;
	if (st->speed <= 0) 
		st->speed = 2;
	if (st->number <= 0)
		st->number=1;
	if (st->number >= 10)
		st->number=1;
	if (st->effect == NULL)
		st->effect = &move_lense;
	if (st->reflect) {
		st->draw = &reflect_draw;
		st->effect = &move_lense;
	}
	if (st->draw == NULL)
		st->draw = &plain_draw;
}

static void *
distort_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
	XGCValues gcv;
	long gcflags;

    st->dpy = dpy;
    st->window = window;

    distort_reset (st);

	st->black_pixel = BlackPixelOfScreen( st->xgwa.screen );

	gcv.function = GXcopy;
	gcv.subwindow_mode = IncludeInferiors;
	gcflags = GCFunction;
	if (use_subwindow_mode_p(st->xgwa.screen, st->window)) /* see grabscreen.c */
		gcflags |= GCSubwindowMode;
	st->gc = XCreateGC (st->dpy, st->window, gcflags, &gcv);

    /* On MacOS X11, XGetImage on a Window often gets an inexplicable BadMatch,
       possibly due to the window manager having occluded something?  It seems
       nondeterministic. Loading the image into a pixmap instead fixes it. */
    if (st->pm) XFreePixmap (st->dpy, st->pm);
    st->pm = XCreatePixmap (st->dpy, st->window,
                            st->xgwa.width, st->xgwa.height, st->xgwa.depth);

    st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                              st->pm, 0, 0);
    st->start_time = time ((time_t *) 0);
    return st;
}

static void
distort_finish_loading (struct state *st)
{
    int i;

    st->start_time = time ((time_t *) 0);

    if (! st->pm) abort();
    XClearWindow (st->dpy, st->window);
    XCopyArea (st->dpy, st->pm, st->window, st->gc, 
               0, 0, st->xgwa.width, st->xgwa.height, 0, 0);
	st->orig_map = XGetImage(st->dpy, st->pm, 0, 0,
                             st->xgwa.width, st->xgwa.height,
                             ~0L, ZPixmap);
	st->buffer_map_cache = malloc(sizeof(unsigned long)*(2*st->radius+st->speed+2)*(2*st->radius+st->speed+2));

	if (st->buffer_map_cache == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}

	st->buffer_map = create_xshm_image(st->dpy, st->xgwa.visual, st->orig_map->depth,
	                                   ZPixmap, &st->shm_info,
	                                   2*st->radius + st->speed + 2,
	                                   2*st->radius + st->speed + 2);

	if ((st->buffer_map->byte_order == st->orig_map->byte_order)
			&& (st->buffer_map->depth == st->orig_map->depth)
			&& (st->buffer_map->format == ZPixmap)
			&& (st->orig_map->format == ZPixmap)
			&& !st->slow) {
		switch (st->orig_map->bits_per_pixel) {
			case 32:
				st->draw_routine = &fast_draw_32;
				st->bpp_size = sizeof(CARD32);
				break;
			case 16:
				st->draw_routine = &fast_draw_16;
				st->bpp_size = sizeof(CARD16);
				break;
			case 8:
				st->draw_routine = &fast_draw_8;
				st->bpp_size = sizeof(CARD8);
				break;
			default:
				st->draw_routine = &generic_draw;
				break;
		}
	} else {
		st->draw_routine = &generic_draw;
	}
	init_round_lense(st);

	for (i = 0; i < st->number; i++) {
		new_rnd_coo(st,i);
		if (st->number != 1)
			st->xy_coo[i].r = (i*st->radius)/(st->number-1); /* "randomize" initial */
		else
			 st->xy_coo[i].r = 0;
		st->xy_coo[i].r_change = st->speed + (i%2)*2*(-st->speed);	/* values a bit */
		st->xy_coo[i].xmove = st->speed + (i%2)*2*(-st->speed);
		st->xy_coo[i].ymove = st->speed + (i%2)*2*(-st->speed);
	}
}

/* example: initializes a "see-trough" matrix */
/* static void make_null_lense(struct state *st)
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
static void convert(struct state *st) 
{
	int *p;
	int i, j;
	st->fast_from = calloc(1, sizeof(int)*((st->buffer_map->bytes_per_line/st->bpp_size)*(2*st->radius+st->speed+2) + 2*st->radius+st->speed+2));
	if (st->fast_from == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}
	p = st->fast_from;
	for (i = 0; i < 2*st->radius+st->speed+2; i++) {
		for (j = 0; j < 2*st->radius+st->speed+2; j++) {
			*(p + i + j*st->buffer_map->bytes_per_line/st->bpp_size)
				= st->from[i][j][0] + st->xgwa.width*st->from[i][j][1];
			if (*(p + i + j*st->buffer_map->bytes_per_line/st->bpp_size) < 0
					|| *(p + i + j*st->buffer_map->bytes_per_line/st->bpp_size) >= st->orig_map->height*st->orig_map->width) {
				*(p + i + j*st->buffer_map->bytes_per_line/st->bpp_size) = 0;
			}
		}
	}
}

/* makes a lense with the Radius=loop and centred in
 * the point (radius, radius)
 */
static void make_round_lense(struct state *st, int radius, int loop)
{
	int i, j;

	for (i = 0; i < 2*radius+st->speed+2; i++) {
		for(j = 0; j < ((0 == st->bpp_size) ? (2*radius+st->speed+2) : (st->buffer_map->bytes_per_line/st->bpp_size)); j++) {
			double r, d;
			r = sqrt ((i-radius)*(i-radius)+(j-radius)*(j-radius));
			if (loop == 0)
			  d=0.0;
			else
			  d=r/loop;

			if (r < loop-1) {

				if (st->vortex) { /* vortex-twist effect */
					double angle;
		/* this one-line formula for getting a nice rotation angle is borrowed
		 * (with permission) from the whirl plugin for gimp,
		 * Copyright (C) 1996 Federico Mena Quintero
		 */
		/* 5 is just a constant used because it looks good :) */
					angle = 5*(1-d)*(1-d);

        /* Avoid atan2: DOMAIN error message */
					if ((radius-j) == 0.0 && (radius-i) == 0.0) {
						st->from[i][j][0] = radius + cos(angle)*r;
						st->from[i][j][1] = radius + sin(angle)*r;
					} else {
						st->from[i][j][0] = radius +
							cos(angle - atan2(radius-j, -(radius-i)))*r;
						st->from[i][j][1] = radius +
							sin(angle - atan2(radius-j, -(radius-i)))*r;
					}
					if (st->magnify) {
						r = sin(d*M_PI_2);
						if (st->blackhole && r != 0) /* blackhole effect */
							r = 1/r;
						st->from[i][j][0] = radius + (st->from[i][j][0]-radius)*r;
						st->from[i][j][1] = radius + (st->from[i][j][1]-radius)*r;
					}
				} else { /* default is to magnify */
					r = sin(d*M_PI_2);
				
	/* raising r to different power here gives different amounts of
	 * distortion, a negative value sucks everything into a black hole
	 */
				/*	r = r*r; */
					if (st->blackhole && r != 0) /* blackhole effect */
						r = 1/r;
									/* bubble effect (and blackhole) */
					st->from[i][j][0] = radius + (i-radius)*r;
					st->from[i][j][1] = radius + (j-radius)*r;
				}
			} else { /* not inside loop */
				st->from[i][j][0] = i;
				st->from[i][j][1] = j;
			}
		}
	}

	/* this is really just a quick hack to keep both the compability mode with all depths and still
	 * allow the custom optimized draw routines with the minimum amount of work */
	if (0 != st->bpp_size) {
		convert(st);
	}
}

#ifndef EXIT_FAILURE
# define EXIT_FAILURE -1
#endif

static void allocate_lense(struct state *st)
{
	int i, j;
	int s = ((0 != st->bpp_size) ? (st->buffer_map->bytes_per_line/st->bpp_size) : (2*st->radius+st->speed+2));
	/* maybe this should be redone so that from[][][] is in one block;
	 * then pointers could be used instead of arrays in some places (and
	 * maybe give a speedup - maybe also consume less memory)
	 */
	st->from = (int ***)malloc(s*sizeof(int **));
	if (st->from == NULL) {
		perror("distort");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < s; i++) {
		st->from[i] = (int **)malloc((2*st->radius+st->speed+2) * sizeof(int *));
		if (st->from[i] == NULL) {
			perror("distort");
			exit(EXIT_FAILURE);
		}
		for (j = 0; j < s; j++) {
			st->from[i][j] = (int *)malloc(2 * sizeof(int));
			if (st->from[i][j] == NULL) {
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
static void init_round_lense(struct state *st)
{
	int k;

	if (st->effect == &swamp_thing) {
		st->from_array = (int ****)malloc((st->radius+1)*sizeof(int ***));
		for (k=0; k <= st->radius; k++) {
			allocate_lense(st);
			make_round_lense(st, st->radius, k);
			st->from_array[k] = st->from;
		}
	} else { /* just allocate one from[][][] */
		allocate_lense(st);
		make_round_lense(st, st->radius,st->radius);
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

static void fast_draw_8(struct state *st, XImage *src, XImage *dest, int x, int y, int *distort_matrix)
{
	CARD8 *u = (CARD8 *)dest->data;
	CARD8 *t = (CARD8 *)src->data + x + y*src->bytes_per_line/sizeof(CARD8);

	while (u < (CARD8 *)(dest->data + sizeof(CARD8)*dest->height
				*dest->bytes_per_line/sizeof(CARD8))) {
		*u++ = t[*distort_matrix++];
	}
}

static void fast_draw_16(struct state *st, XImage *src, XImage *dest, int x, int y, int *distort_matrix)
{
	CARD16 *u = (CARD16 *)dest->data;
	CARD16 *t = (CARD16 *)src->data + x + y*src->bytes_per_line/sizeof(CARD16);

	while (u < (CARD16 *)(dest->data + sizeof(CARD16)*dest->height
				*dest->bytes_per_line/sizeof(CARD16))) {
		*u++ = t[*distort_matrix++];
	}
}

static void fast_draw_32(struct state *st, XImage *src, XImage *dest, int x, int y, int *distort_matrix)
{
	CARD32 *u = (CARD32 *)dest->data;
	CARD32 *t = (CARD32 *)src->data + x + y*src->bytes_per_line/sizeof(CARD32);

	while (u < (CARD32 *)(dest->data + sizeof(CARD32)*dest->height
				*dest->bytes_per_line/sizeof(CARD32))) {
		*u++ = t[*distort_matrix++];
	}
}

static void generic_draw(struct state *st, XImage *src, XImage *dest, int x, int y, int *distort_matrix)
{
	int i, j;
	for (i = 0; i < dest->width; i++)
		for (j = 0; j < dest->height; j++)
			if (st->from[i][j][0] + x >= 0 &&
					st->from[i][j][0] + x < src->width &&
					st->from[i][j][1] + y >= 0 &&
					st->from[i][j][1] + y < src->height)
				XPutPixel(dest, i, j,
						XGetPixel(src,
							st->from[i][j][0] + x,
							st->from[i][j][1] + y));
}

/* generate an XImage of from[][][] and draw it on the screen */
static void plain_draw(struct state *st, int k)
{
	if (st->xy_coo[k].x+2*st->radius+st->speed+2 > st->orig_map->width ||
			st->xy_coo[k].y+2*st->radius+st->speed+2 > st->orig_map->height)
		return;

	st->draw_routine(st, st->orig_map, st->buffer_map, st->xy_coo[k].x, st->xy_coo[k].y, st->fast_from);

	put_xshm_image(st->dpy, st->window, st->gc, st->buffer_map, 0, 0, st->xy_coo[k].x, st->xy_coo[k].y,
	               2*st->radius+st->speed+2, 2*st->radius+st->speed+2, &st->shm_info);
}


/* generate an XImage from the reflect algoritm submitted by
 * Randy Zack <randy@acucorp.com>
 * draw really got too big and ugly so I split it up
 * it should be possible to use the from[][] to speed it up
 * (once I figure out the algorithm used :)
 */
static void reflect_draw(struct state *st, int k)
{
	int i, j;
	int	cx, cy;
	int	ly, lysq, lx, ny, dist, rsq = st->radius * st->radius;

	cx = cy = st->radius;
	if (st->xy_coo[k].ymove > 0)
		cy += st->speed;
	if (st->xy_coo[k].xmove > 0)
		cx += st->speed;

	for(i = 0 ; i < 2*st->radius+st->speed+2; i++) {
		ly = i - cy;
		lysq = ly * ly;
		ny = st->xy_coo[k].y + i;
		if (ny >= st->orig_map->height) ny = st->orig_map->height-1;
		for(j = 0 ; j < 2*st->radius+st->speed+2 ; j++) {
			lx = j - cx;
			dist = lx * lx + lysq;
			if (dist > rsq ||
				ly < -st->radius || ly > st->radius ||
				lx < -st->radius || lx > st->radius)
				XPutPixel( st->buffer_map, j, i,
						   XGetPixel( st->orig_map, st->xy_coo[k].x + j, ny ));
			else if (dist == 0)
				XPutPixel( st->buffer_map, j, i, st->black_pixel );
			else {
				int	x = st->xy_coo[k].x + cx + (lx * rsq / dist);
				int	y = st->xy_coo[k].y + cy + (ly * rsq / dist);
				if (x < 0 || x >= st->xgwa.width ||
					y < 0 || y >= st->xgwa.height)
					XPutPixel( st->buffer_map, j, i, st->black_pixel );
				else
					XPutPixel( st->buffer_map, j, i,
							   XGetPixel( st->orig_map, x, y ));
			}
		}
	}

	XPutImage(st->dpy, st->window, st->gc, st->buffer_map, 0, 0, st->xy_coo[k].x, st->xy_coo[k].y,
			2*st->radius+st->speed+2, 2*st->radius+st->speed+2);
}

/* create a new, random coordinate, that won't interfer with any other
 * coordinates, as the drawing routines would be significantly slowed
 * down if they were to handle serveral layers of distortions
 */
static void new_rnd_coo(struct state *st, int k)
{
	int i;
    int loop = 0;

	st->xy_coo[k].x = (random() % (st->xgwa.width-2*st->radius));
	st->xy_coo[k].y = (random() % (st->xgwa.height-2*st->radius));
	
	for (i = 0; i < st->number; i++) {
		if (i != k) {
			if ((abs(st->xy_coo[k].x - st->xy_coo[i].x) <= 2*st->radius+st->speed+2)
			 && (abs(st->xy_coo[k].y - st->xy_coo[i].y) <= 2*st->radius+st->speed+2)) {
				st->xy_coo[k].x = (random() % (st->xgwa.width-2*st->radius));
				st->xy_coo[k].y = (random() % (st->xgwa.height-2*st->radius));
				i=-1; /* ugly */
			} 
		}
        if (loop++ > 1000) return;  /* let's not get stuck */
	}
}

/* move lens and handle bounces with walls and other lenses */
static void move_lense(struct state *st, int k)
{
	int i;

	if (st->xy_coo[k].x + 2*st->radius + st->speed + 2 >= st->xgwa.width)
		st->xy_coo[k].xmove = -abs(st->xy_coo[k].xmove);
	if (st->xy_coo[k].x <= st->speed) 
		st->xy_coo[k].xmove = abs(st->xy_coo[k].xmove);
	if (st->xy_coo[k].y + 2*st->radius + st->speed + 2 >= st->xgwa.height)
		st->xy_coo[k].ymove = -abs(st->xy_coo[k].ymove);
	if (st->xy_coo[k].y <= st->speed)
		st->xy_coo[k].ymove = abs(st->xy_coo[k].ymove);

	st->xy_coo[k].x = st->xy_coo[k].x + st->xy_coo[k].xmove;
	st->xy_coo[k].y = st->xy_coo[k].y + st->xy_coo[k].ymove;

	/* bounce against othe lenses */
	for (i = 0; i < st->number; i++) {
		if ((i != k)
		
/* This commented test is for rectangular lenses (not currently used) and
 * the one used is for circular ones
		&& (abs(xy_coo[k].x - xy_coo[i].x) <= 2*radius)
		&& (abs(xy_coo[k].y - xy_coo[i].y) <= 2*radius)) { */

		&& ((st->xy_coo[k].x - st->xy_coo[i].x)*(st->xy_coo[k].x - st->xy_coo[i].x)
		  + (st->xy_coo[k].y - st->xy_coo[i].y)*(st->xy_coo[k].y - st->xy_coo[i].y)
			<= 2*st->radius*2*st->radius)) {

			int x, y;
			x = st->xy_coo[k].xmove;
			y = st->xy_coo[k].ymove;
			st->xy_coo[k].xmove = st->xy_coo[i].xmove;
			st->xy_coo[k].ymove = st->xy_coo[i].ymove;
			st->xy_coo[i].xmove = x;
			st->xy_coo[i].ymove = y;
		}
	}

}

/* make xy_coo[k] grow/shrink */
static void swamp_thing(struct state *st, int k)
{
	if (st->xy_coo[k].r >= st->radius)
		st->xy_coo[k].r_change = -abs(st->xy_coo[k].r_change);
	
	if (st->xy_coo[k].r <= 0) {
		st->from = st->from_array[0];
		st->draw(st,k); 
		st->xy_coo[k].r_change = abs(st->xy_coo[k].r_change);
		new_rnd_coo(st,k);
		st->xy_coo[k].r=st->xy_coo[k].r_change;
		return;
	}

	st->xy_coo[k].r = st->xy_coo[k].r + st->xy_coo[k].r_change;

	if (st->xy_coo[k].r >= st->radius)
		st->xy_coo[k].r = st->radius;
	if (st->xy_coo[k].r <= 0)
		st->xy_coo[k].r=0;

	st->from = st->from_array[st->xy_coo[k].r];
}


static unsigned long
distort_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int k;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
        distort_finish_loading (st);
      }
      return st->delay;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < time ((time_t *) 0)) {
    if (st->pm) XFreePixmap (st->dpy, st->pm);
    st->pm = XCreatePixmap (st->dpy, st->window,
                            st->xgwa.width, st->xgwa.height, st->xgwa.depth);
    st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                              st->pm, 0, 0);
    return st->delay;
  }

  for (k = 0; k < st->number; k++) {
    st->effect(st,k);
    st->draw(st,k);
  }
  return st->delay;
}

static void
distort_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  /* XClearWindow (dpy, window); */
  /* Why doesn't this work? */
  if (st->orig_map)  /* created in distort_finish_loading, might be early */
    XPutImage (st->dpy, st->window, st->gc, st->orig_map,
               0, 0, st->orig_map->width, st->orig_map->height, 0, 0);
}

static Bool
distort_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      distort_reset(st);
      return True;
    }
  return False;
}

static void
distort_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (st->dpy, st->gc);
  if (st->pm) XFreePixmap (dpy, st->pm);
  if (st->orig_map) XDestroyImage (st->orig_map);
  if (st->buffer_map) destroy_xshm_image (st->dpy, st->buffer_map, &st->shm_info);
  if (st->from) free (st->from);
  if (st->fast_from) free (st->fast_from);
  if (st->from_array) free (st->from_array);
  free (st);
}




static const char *distort_defaults [] = {
	"*dontClearRoot:		True",
	"*background:			Black",
    "*fpsSolid:				true",
#ifdef __sgi    /* really, HAVE_READ_DISPLAY_EXTENSION */
	"*visualID:			Best",
#endif

	"*delay:			20000",
    "*duration:			120",
	"*radius:			0",
	"*speed:			0",
	"*number:			0",
	"*slow:				False",
	"*vortex:			False",
	"*magnify:			False",
	"*reflect:			False",
	"*blackhole:		False",
	"*effect:		    none",
#ifdef HAVE_XSHM_EXTENSION
	"*useSHM:			False",		/* xshm turns out not to help. */
#endif /* HAVE_XSHM_EXTENSION */
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
  "*rotateImages:       True",
#endif
	0
};

static XrmOptionDescRec distort_options [] = {
  { "-delay",     ".delay",       XrmoptionSepArg, 0 },
  { "-duration",  ".duration",	  XrmoptionSepArg, 0 },
  { "-radius",    ".radius",      XrmoptionSepArg, 0 },
  { "-speed",     ".speed",       XrmoptionSepArg, 0 },
  { "-number",    ".number",      XrmoptionSepArg, 0 },

  { "-effect",    ".effect",      XrmoptionSepArg, 0 },
  { "-swamp",     ".effect",      XrmoptionNoArg, "swamp"  },
  { "-bounce",    ".effect",      XrmoptionNoArg, "bounce" },

  { "-reflect",   ".reflect",     XrmoptionNoArg, "True" },
  { "-vortex",    ".vortex",      XrmoptionNoArg, "True" },
  { "-magnify",   ".magnify",     XrmoptionNoArg, "True" },
  { "-blackhole", ".blackhole",   XrmoptionNoArg, "True" },
  { "-slow",      ".slow",        XrmoptionNoArg, "True" },
#ifdef HAVE_XSHM_EXTENSION
  { "-shm",       ".useSHM",      XrmoptionNoArg, "True" },
  { "-no-shm",    ".useSHM",      XrmoptionNoArg, "False" },
#endif /* HAVE_XSHM_EXTENSION */
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Distort", distort)
