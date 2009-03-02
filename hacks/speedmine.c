/* -*- Mode: C; c-basic-offset: 4; tab-width: 4 -*-
 * speedmine, Copyright (C) 2001 Conrad Parker <conrad@deephackmode.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*
 * Written mostly over the Easter holiday, 2001. Psychedelic option due to
 * a night at Home nightclub, Sydney. Three all-nighters of solid partying
 * were involved in the week this hack was written.
 *
 * Happy Birthday to WierdArms (17 April) and Pat (18 April)
 */

/*
 * Hacking notes
 *
 * This program generates a rectangular terrain grid and maps this onto
 * a semi-circular tunnel. The terrain has length TERRAIN_LENGTH, which
 * corresponds to length along the tunnel, and breadth TERRAIN_BREADTH,
 * which corresponds to circumference around the tunnel. For each frame,
 * the tunnel is perspective mapped onto a set of X and Y screen values.
 *
 * Throughout this code the following temporary variable names are used:
 *
 * 			i	iterates along the tunnel in the direction of travel
 * 			j	iterates around the tunnel clockwise
 * 			t	iterates along the length of the perspective mapped values
 * 				from the furthest to the nearest
 *
 * Thus, the buffers are used with these iterators:
 *
 * 			terrain[i][j]					terrain map
 * 			worldx[i][j], worldy[i][j] 		world coordinates (after wrapping)
 * 			{x,y,z}curvature[i]				tunnel curvature
 * 			wideness[i]						tunnel wideness
 * 			bonuses[i]						bonus values
 *
 * 			xvals[t][j], yvals[t][j] 		screen coordinates
 * 			{min,max}{x,y}[t]				bounding boxes of screen coords
 */

/* Define or undefine NDEBUG to turn assert and abort debugging off or on */
#define NDEBUG
#include <assert.h>

#include <math.h>

#include "screenhack.h"
#include "erase.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define RAND(r) (int)(((r)>0)?(random() % (long)(r)): -(random() % (long)(-r)))

#define SIGN3(a) ((a)>0?1:((a)<0?-1:0))

#define MODULO(a,b) while ((a)<0) (a)+=(b); (a) %= (b);

/* No. of shades of each color (ground, walls, bonuses) */
#define MAX_COLORS 32

#ifdef NDEBUG
#define DEBUG_FLAG 0
#else
#define DEBUG_FLAG 1
#endif


#define FORWARDS 1
#define BACKWARDS -1
/* Apparently AIX's math.h bogusly defines `nearest' as a function,
   in violation of the ANSI C spec. */
#undef nearest
#define nearest n3arest

#define wireframe (st->wire_flag||st->wire_bonus>8||st->wire_bonus%2==1)
#define effective_speed (st->direction*(st->speed+st->speed_bonus))

/* No. of levels of interpolation, for perspective */
#define INTERP 32

/* These must be powers of 2 */
#define TERRAIN_LENGTH 256
#define TERRAIN_BREADTH 32

/* total "perspective distance" of terrain */
#define TERRAIN_PDIST (INTERP*TERRAIN_LENGTH)

#define ROTS 1024
#define TB_MUL (ROTS/TERRAIN_BREADTH)

#define random_elevation() (st->terrain_flag?(random() % 200):0)
#define random_curvature() (st->curviness>0.0?((double)(random() % 40)-20)*st->curviness:0.0)
#define random_twist() (st->twistiness>0.0?((double)(random() % 40)-20)*st->twistiness:0.0)
#define random_wideness() (st->widening_flag?(int)(random() % 1200):0)

#define STEEL_ELEVATION 300

struct state {
    Display *dpy;
    Window window;

    Pixmap dbuf, stars_mask;
    Colormap cmap;
    unsigned int default_fg_pixel;
    GC draw_gc, erase_gc, tunnelend_gc, stars_gc, stars_erase_gc;

    int ncolors, nr_ground_colors, nr_wall_colors, nr_bonus_colors;
    XColor ground_colors[MAX_COLORS], wall_colors[MAX_COLORS];
    XColor bonus_colors[MAX_COLORS];
    GC ground_gcs[MAX_COLORS], wall_gcs[MAX_COLORS], bonus_gcs[MAX_COLORS];

    int be_wormy;

    int width, height;
    int delay;

    int smoothness;
    int verbose_flag;
    int wire_flag;
    int terrain_flag;
    int widening_flag;
    int bumps_flag;
    int bonuses_flag;
    int crosshair_flag;
    int psychedelic_flag;

    double maxspeed;

    double thrust, gravity;

    double vertigo;
    double curviness;
    double twistiness;

    double pos;
    double speed;
    double accel;
    double step;

    int direction;

    int pindex, nearest;
    int flipped_at;
    int xoffset, yoffset;

    int bonus_bright;
    int wire_bonus;

    double speed_bonus;

    int spin_bonus;
    int backwards_bonus;

    double sintab[ROTS], costab[ROTS];

    int orientation;

    int terrain[TERRAIN_LENGTH][TERRAIN_BREADTH];
    double xcurvature[TERRAIN_LENGTH];
    double ycurvature[TERRAIN_LENGTH];
    double zcurvature[TERRAIN_LENGTH];
    int wideness[TERRAIN_LENGTH];
    int bonuses[TERRAIN_LENGTH];
    int xvals[TERRAIN_LENGTH][TERRAIN_BREADTH];
    int yvals[TERRAIN_LENGTH][TERRAIN_BREADTH];
    double worldx[TERRAIN_LENGTH][TERRAIN_BREADTH];
    double worldy[TERRAIN_LENGTH][TERRAIN_BREADTH];
    int minx[TERRAIN_LENGTH], maxx[TERRAIN_LENGTH];
    int miny[TERRAIN_LENGTH], maxy[TERRAIN_LENGTH];

    int total_nframes;
    int nframes;
    double fps;
    double fps_start, fps_end;
    struct timeval start_time;

    int rotation_offset;
    int jamming;
};

/* a forward declaration ... */
static void change_colors(struct state *st);



/*
 * get_time ()
 *
 * returns the total time elapsed since the beginning of the demo
 */
static double get_time(struct state *st) {
  struct timeval t;
  float f;
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&t, NULL);
#else
  gettimeofday(&t);
#endif
  t.tv_sec -= st->start_time.tv_sec;
  f = ((double)t.tv_sec) + t.tv_usec*1e-6;
  return f;
}

/*
 * init_time ()
 *
 * initialises the timing structures
 */
static void init_time(struct state *st) {
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&st->start_time, NULL);
#else
  gettimeofday(&st->start_time);
#endif
  st->fps_start = get_time(st);
}


/*
 * perspective()
 *
 * perspective map the world coordinates worldx[i][j], worldy[i][j] onto
 * screen coordinates xvals[t][j], yvals[t][j]
 */
static void
perspective (struct state *st)
{
	int i, j, jj, t=0, depth, view_pos;
   	int rotation_bias, r;
	double xc=0.0, yc=0.0, zc=0.0;
	double xcc=0.0, ycc=0.0, zcc=0.0;
	double xx, yy;
	double zfactor, zf;

	zf = 8.0*28.0 / (double)(st->width*TERRAIN_LENGTH);
	if (st->be_wormy) zf *= 3.0;

	depth = TERRAIN_PDIST - INTERP + st->pindex;

	view_pos = (st->nearest+3*TERRAIN_LENGTH/4)%TERRAIN_LENGTH;
	
	st->xoffset += - st->xcurvature[view_pos]*st->curviness/8;
	st->xoffset /= 2;

	st->yoffset += - st->ycurvature[view_pos]*st->curviness/4;
	st->yoffset /= 2;

	st->rotation_offset += (int)((st->zcurvature[view_pos]-st->zcurvature[st->nearest])*ROTS/8);
	st->rotation_offset /= 2;
	rotation_bias = st->orientation + st->spin_bonus - st->rotation_offset;

	if (st->bumps_flag) {
		if (st->be_wormy) {
			st->yoffset -= ((st->terrain[view_pos][TERRAIN_BREADTH/4] * st->width /(8*1600)));
			rotation_bias += (st->terrain[view_pos][TERRAIN_BREADTH/4+2] -
							 st->terrain[view_pos][TERRAIN_BREADTH/4-2])/8;
		} else {
			st->yoffset -= ((st->terrain[view_pos][TERRAIN_BREADTH/4] * st->width /(2*1600)));
			rotation_bias += (st->terrain[view_pos][TERRAIN_BREADTH/4+2] -
							 st->terrain[view_pos][TERRAIN_BREADTH/4-2])/16;
		}
	}

	MODULO(rotation_bias, ROTS);

	for (t=0; t < TERRAIN_LENGTH; t++) {
		i = st->nearest + t; MODULO(i, TERRAIN_LENGTH);
		xc += st->xcurvature[i]; yc += st->ycurvature[i]; zc += st->zcurvature[i];
		xcc += xc; ycc += yc; zcc += zc;
		st->maxx[i] = st->maxy[i] = 0;
		st->minx[i] = st->width; st->miny[i] = st->height;
	}

	for (t=0; t < TERRAIN_LENGTH; t++) {
		i = st->nearest - 1 - t; MODULO(i, TERRAIN_LENGTH);

		zfactor = (double)depth* (12.0 - TERRAIN_LENGTH/8.0) * zf;
		for (j=0; j < TERRAIN_BREADTH; j++) {
			jj = st->direction * j; MODULO(jj, TERRAIN_BREADTH);
            /* jwz: not totally sure if this is right, but it avoids div0 */
            if (zfactor != 0) {
                xx = (st->worldx[i][jj]-(st->vertigo*xcc))/zfactor;
                yy = (st->worldy[i][j]-(st->vertigo*ycc))/zfactor;
            } else {
                xx = 0;
                yy = 0;
            }
			r = rotation_bias + (int)(st->vertigo*zcc); MODULO(r, ROTS);

			st->xvals[t][j] = st->xoffset + (st->width>>1) +
				       	  (int)(xx * st->costab[r] - yy * st->sintab[r]);
			st->maxx[t] = MAX(st->maxx[t], st->xvals[t][j]);
			st->minx[t] = MIN(st->minx[t], st->xvals[t][j]);

			st->yvals[t][j] = st->yoffset + st->height/2 +
						  (int)(xx * st->sintab[r] + yy * st->costab[r]);
			st->maxy[t] = MAX(st->maxy[t], st->yvals[t][j]);
			st->miny[t] = MIN(st->miny[t], st->yvals[t][j]);
		}
		xcc -= xc; ycc -= yc; zcc -= zc;
		xc -= st->xcurvature[i]; yc -= st->ycurvature[i]; zc -= st->zcurvature[i];
		depth -= INTERP;
	}
}

/*
 * wrap_tunnel (start, end)
 *
 * wrap the terrain terrain[i][j] around the semi-circular tunnel function
 * 
 * 			x' = x/2 * cos(theta) - (y-k) * x * sin(theta)
 * 			y' = x/4 * sin(theta) + y * cos(theta)
 *
 * between i=start and i=end inclusive, producing world coordinates
 * worldx[i][j], worldy[i][j]
 */
static void
wrap_tunnel (struct state *st, int start, int end)
{
	int i, j, v;
	double x, y;

	assert (start < end);

	for (i=start; i <= end; i++) {
		for (j=0; j < TERRAIN_BREADTH; j++) {
			x = j * (1.0/TERRAIN_BREADTH);
			v = st->terrain[i][j];
			y = (double)(v==STEEL_ELEVATION?200:v) - st->wideness[i] - 1200;

			/* lower road */
			if (j > TERRAIN_BREADTH/8 && j < 3*TERRAIN_BREADTH/8) y -= 300;

		    st->worldx[i][j] = x/2 * st->costab[j*TB_MUL] -
				 			(y-st->height/4.0)*x*st->sintab[j*TB_MUL];
			st->worldy[i][j] = x/4 * st->sintab[j*TB_MUL] +
							y * st->costab[j*TB_MUL];
		}
	}
}

/*
 * flip_direction()
 *
 * perform the state transitions and terrain transformation for the
 * "look backwards/look forwards" bonus
 */
static void
flip_direction (struct state *st)
{
	int i, ip, in, j, t;

	st->direction = -st->direction;

	st->bonus_bright = 20;

	for (i=0; i < TERRAIN_LENGTH; i++) {
		in = st->nearest + i; MODULO(in, TERRAIN_BREADTH);
		ip = st->nearest - i; MODULO(ip, TERRAIN_BREADTH);
		for (j=0; j < TERRAIN_BREADTH; j++) {
			t = st->terrain[ip][j];
			st->terrain[ip][j] = st->terrain[in][j];
			st->terrain[in][j] = t;
		}
	}
}

/*
 * generate_smooth (start, end)
 *
 * generate smooth terrain between i=start and i=end inclusive
 */
static void
generate_smooth (struct state *st, int start, int end)
{
	int i,j, ii;

	assert (start < end);

	for (i=start; i <= end; i++) {
		ii = i; MODULO(ii, TERRAIN_LENGTH);
		for (j=0; j < TERRAIN_BREADTH; j++) {
			st->terrain[i][j] = STEEL_ELEVATION;
		}
	}
}

/*
 * generate_straight (start, end)
 *
 * zero the curvature and wideness between i=start and i=end inclusive
 */
static void
generate_straight (struct state *st, int start, int end)
{
	int i,j, ii;

	assert (start < end);

	for (i=start; i <= end; i++) {
		ii = i; MODULO(ii, TERRAIN_LENGTH);
		for (j=0; j < TERRAIN_BREADTH; j++) {
	  		st->xcurvature[ii] = 0;
	 		st->ycurvature[ii] = 0;
			st->zcurvature[ii] = 0;
			st->wideness[ii] = 0;
		}
	}
}

/*
 * int generate_terrain_value (v1, v2, v3, v4, w)
 *
 * generate terrain value near the average of v1, v2, v3, v4, with
 * perturbation proportional to w
 */
static int
generate_terrain_value (struct state *st, int v1, int v2, int v3, int v4, int w)
{
	int sum, ret;
	int rval;

	if (!st->terrain_flag) return 0;

	sum = v1 + v2 + v3 + v4;

	rval = w*sum/st->smoothness;
	if (rval == 0) rval = 2;

	ret = (sum/4 -(rval/2) + RAND(rval));

	if (ret < -400 || ret > 400) {
		ret = sum/4;
	}

	return ret;
}

/*
 * generate_terrain (start, end, final)
 *
 * generate terrain[i][j] between i=start and i=end inclusive
 *
 * This is performed by successive subdivision of the terrain into
 * rectangles of decreasing size. Subdivision continues until the
 * the minimum width or height of these rectangles is 'final'; ie.
 * final=1 indicates to subdivide as far as possible, final=2 indicates
 * to stop one subdivision before that (leaving a checkerboard pattern
 * uncalculated) etc.
 */
static void
generate_terrain (struct state *st, int start, int end, int final)
{
	int i,j,w,l;
	int ip, jp, in, jn; /* prev, next values */
	int diff;

	assert (start < end);
	assert (start >= 0 && start < TERRAIN_LENGTH);
	assert (end >= 0 && end < TERRAIN_LENGTH);

	diff = end - start + 1;

	st->terrain[end][0] = random_elevation();
	st->terrain[end][TERRAIN_BREADTH/2] = random_elevation();

	for (w= diff/2, l=TERRAIN_BREADTH/4;
	     w >= final || l >= final; w /= 2, l /= 2) {

		if (w<1) w=1; if (l<1) l=1;

		for (i=start+w-1; i < end; i += (w*2)) {
			ip = i-w; MODULO(ip, TERRAIN_LENGTH);
			in = i+w; MODULO(in, TERRAIN_LENGTH);
			for (j=l-1; j < TERRAIN_BREADTH; j += (l*2)) {
				jp = j-1; MODULO(jp, TERRAIN_BREADTH);
				jn = j+1; MODULO(jn, TERRAIN_BREADTH);
				st->terrain[i][j] =
					generate_terrain_value (st, st->terrain[ip][jp], st->terrain[in][jp], 
		                            st->terrain[ip][jn], st->terrain[in][jn], w);
		   	}
		}

		for (i=start+(w*2)-1; i < end; i += (w*2)) {
			ip = i-w; MODULO(ip, TERRAIN_LENGTH);
			in = i+w; MODULO(in, TERRAIN_LENGTH);
			for (j=l-1; j < TERRAIN_BREADTH; j += (l*2)) {
				jp = j-1; MODULO(jp, TERRAIN_BREADTH);
				jn = j+1; MODULO(jn, TERRAIN_BREADTH);
				st->terrain[i][j] =
					generate_terrain_value (st, st->terrain[ip][j], st->terrain[in][j],
		                            st->terrain[i][jp], st->terrain[i][jn], w);
		   	}
		}

		for (i=start+w-1; i < end; i += (w*2)) {
			ip = i-w; MODULO(ip, TERRAIN_LENGTH);
			in = i+w; MODULO(in, TERRAIN_LENGTH);
			for (j=2*l-1; j < TERRAIN_BREADTH; j += (l*2)) {
				jp = j-1; MODULO(jp, TERRAIN_BREADTH);
				jn = j+1; MODULO(jn, TERRAIN_BREADTH);
				st->terrain[i][j] =
					generate_terrain_value (st, st->terrain[ip][j], st->terrain[in][j],
		                            st->terrain[i][jp], st->terrain[i][jn], w);
		   	}
		}
	}
}

/*
 * double generate_curvature_value (v1, v2, w)
 *
 * generate curvature value near the average of v1 and v2, with perturbation
 * proportional to w
 */
static double
generate_curvature_value (double v1, double v2, int w)
{
	double sum, avg, diff, ret;
	int rval;

	assert (!isnan(v1) && !isnan(v2));

	sum = v1+v2;
	avg = sum/2.0;

	diff = MIN(v1 - avg, v2 - avg);

	rval = (int)diff * w;
	if (rval == 0.0) return avg;

	ret = (avg -((double)rval)/500.0 + ((double)RAND(rval))/1000.0);

	assert (!isnan(ret));

	return ret;
}

/*
 * generate_curves (start, end)
 *
 * generate xcurvature[i], ycurvature[i], zcurvature[i] and wideness[i]
 * between start and end inclusive
 */
static void
generate_curves (struct state *st, int start, int end)
{
	int i, diff, ii, in, ip, w;

	assert (start < end);

	diff = end - start + 1; MODULO (diff, TERRAIN_LENGTH);

	if (random() % 100 == 0)
	  st->xcurvature[end] = 30 * random_curvature();
	else if (random() % 10 == 0)
	  st->xcurvature[end] = 20 * random_curvature();
	else
	  st->xcurvature[end] = 10 * random_curvature();

	if (random() % 50 == 0)
	  st->ycurvature[end] = 20 * random_curvature();
	else if (random() % 25 == 0)
	  st->ycurvature[end] = 30 * random_curvature();
	else
	  st->ycurvature[end] = 10 * random_curvature();

	if (random() % 3 == 0)
	  st->zcurvature[end] = random_twist();
	else
	  st->zcurvature[end] =
			  generate_curvature_value (st->zcurvature[end], random_twist(), 1);

	if (st->be_wormy)
			st->wideness[end] = random_wideness();
	else
		st->wideness[end] =
			generate_curvature_value (st->wideness[end], random_wideness(), 1);

    for (w=diff/2; w >= 1; w /= 2) {
      for (i=start+w-1; i < end; i+=(w*2)) {
        ii = i; MODULO (ii, TERRAIN_LENGTH);
  	  	ip = i-w; MODULO (ip, TERRAIN_LENGTH);
  	    in = i+w; MODULO (in, TERRAIN_LENGTH);
  	    st->xcurvature[ii] =
				generate_curvature_value (st->xcurvature[ip], st->xcurvature[in], w);
  	    st->ycurvature[ii] =
				generate_curvature_value (st->ycurvature[ip], st->ycurvature[in], w);
	    st->zcurvature[ii] =
				generate_curvature_value (st->zcurvature[ip], st->zcurvature[in], w);
	    st->wideness[ii] =
				generate_curvature_value (st->wideness[ip], st->wideness[in], w);
      }
    }
}

/*
 * do_bonus ()
 *
 * choose a random bonus and perform its state transition
 */
static void
do_bonus (struct state *st)
{
	st->bonus_bright = 20;

	if (st->jamming > 0) {
		st->jamming--;
		st->nearest -= 2; MODULO(st->nearest, TERRAIN_LENGTH);
		return;
	}

	if (st->psychedelic_flag) change_colors(st);

	switch (random() % 7) {
	case 0: /* switch to or from wireframe */
		st->wire_bonus = (st->wire_bonus?0:300);
		break;
	case 1: /* speedup */
		st->speed_bonus = 40.0;
		break;
	case 2:
		st->spin_bonus += ROTS;
		break;
	case 3:
		st->spin_bonus -= ROTS;
		break;
	case 4: /* look backwards / look forwards */
		st->flipped_at = st->nearest;
		flip_direction (st);
		st->backwards_bonus = (st->backwards_bonus?0:10);
		break;
	case 5:
		change_colors(st);
		break;
	case 6: /* jam against the bonus a few times; deja vu! */
		st->nearest -= 2; MODULO(st->nearest, TERRAIN_LENGTH);
		st->jamming = 3;
		break;
	default:
		assert(0);
		break;
	}
}

/*
 * check_bonus ()
 *
 * check if a bonus has been passed in the last frame, and handle it
 */
static void
check_bonuses (struct state *st)
{
	int i, ii, start, end;

	if (!st->bonuses_flag) return;

	if (st->step >= 0.0) {
		start = st->nearest; end = st->nearest + (int)floor(st->step);
	} else {
		end = st->nearest; start = st->nearest + (int)floor(st->step);
	}

	if (st->be_wormy) {
		start += TERRAIN_LENGTH/4;
		end += TERRAIN_LENGTH/4;
	}

	for (i=start; i < end; i++) {
		ii = i; MODULO(ii, TERRAIN_LENGTH);
		if (st->bonuses[ii] == 1) do_bonus (st);
	}
}

/*
 * decrement_bonuses (double time_per_frame)
 *
 * decrement timers associated with bonuses
 */
static void
decrement_bonuses (struct state *st, double time_per_frame)
{
	if (!st->bonuses_flag) return;

	if (st->bonus_bright > 0) st->bonus_bright-=4;
	if (st->wire_bonus > 0) st->wire_bonus--;
	if (st->speed_bonus > 0) st->speed_bonus -= 2.0;

	if (st->spin_bonus > 10) st->spin_bonus -= (int)(st->step*13.7);
	else if (st->spin_bonus < -10) st->spin_bonus += (int)(st->step*11.3);

	if (st->backwards_bonus > 1) st->backwards_bonus--;
	else if (st->backwards_bonus == 1) {
		st->nearest += 2*(MAX(st->flipped_at, st->nearest) - MIN(st->flipped_at,st->nearest));
		MODULO(st->nearest, TERRAIN_LENGTH);
		flip_direction (st);
		st->backwards_bonus = 0;
	}
}

/*
 * set_bonuses (start, end)
 *
 * choose if to and where to set a bonus between i=start and i=end inclusive
 */
static void
set_bonuses (struct state *st, int start, int end)
{
	int i, diff, ii;

	if (!st->bonuses_flag) return;

	assert (start < end);

	diff = end - start;

	for (i=start; i <= end; i++) {
		ii = i; if (ii>=TERRAIN_LENGTH) ii -= TERRAIN_LENGTH;
		st->bonuses[ii] = 0;
	}
	if (random() % 4 == 0) {
		i = start + RAND(diff-3);
		ii = i; if (ii>=TERRAIN_LENGTH) ii -= TERRAIN_LENGTH;
		st->bonuses[ii] = 2; /* marker */
		ii = i+3; if (ii>=TERRAIN_LENGTH) ii -= TERRAIN_LENGTH;
		st->bonuses[ii] = 1; /* real thing */
	}
}

/*
 * regenerate_terrain ()
 *
 * regenerate a portion of the terrain map of length TERRAIN_LENGTH/4 iff
 * we just passed between two quarters of the terrain.
 *
 * choose the kind of terrain to produce, produce it and wrap the tunnel
 */
static void
regenerate_terrain (struct state *st)
{
	int start, end;
	int passed;

	passed = st->nearest % (TERRAIN_LENGTH/4);

	if (st->speed == 0.0 ||
		(st->speed > 0.0 && passed > (int)st->step) ||
		(st->speed < 0.0 && (TERRAIN_LENGTH/4)-passed > (int)fabs(st->step))) {

		return;
	}

	end = st->nearest - passed - 1; MODULO(end, TERRAIN_LENGTH);
	start = end - TERRAIN_LENGTH/4 + 1; MODULO(start, TERRAIN_LENGTH);

	if (DEBUG_FLAG) printf ("Regenerating [%d - %d]\n", start, end);

	set_bonuses (st, start, end);

	switch (random() % 64) {
	case 0:
	case 1:
		generate_terrain (st, start, end, 1);
		generate_smooth (st, start,
			start + TERRAIN_LENGTH/8 + (random() % TERRAIN_LENGTH/8));
		break;
	case 2:
		generate_smooth (st, start, end);
		generate_terrain (st, start, end, 4); break;
	case 3:
		generate_smooth (st, start, end);
		generate_terrain (st, start, end, 2); break;
	default:
		generate_terrain (st, start, end, 1);
	}

	if (random() % 16 == 0) {
		generate_straight (st, start, end);
	} else {
		generate_curves (st, start, end);
	}

	wrap_tunnel (st, start, end);
}

/*
 * init_terrain ()
 *
 * initialise the terrain map for the beginning of the demo
 */
static void
init_terrain (struct state *st)
{
	int i, j;

	for (i=0; i < TERRAIN_LENGTH; i++) {
		for (j=0; j < TERRAIN_BREADTH; j++) {
			st->terrain[i][j] = 0;
		}
	}

	st->terrain[TERRAIN_LENGTH-1][0] =  - (random() % 300);
	st->terrain[TERRAIN_LENGTH-1][TERRAIN_BREADTH/2] =  - (random() % 300);

	generate_smooth (st, 0, TERRAIN_LENGTH-1);
	generate_terrain (st, 0, TERRAIN_LENGTH/4 -1, 4);
	generate_terrain (st, TERRAIN_LENGTH/4, TERRAIN_LENGTH/2 -1, 2);
	generate_terrain (st, TERRAIN_LENGTH/2, 3*TERRAIN_LENGTH/4 -1, 1);
	generate_smooth (st, 3*TERRAIN_LENGTH/4, TERRAIN_LENGTH-1);
}

/*
 * init_curves ()
 *
 * initialise the curvatures and wideness for the beginning of the demo.
 */
static void
init_curves (struct state *st)
{
	int i;

	for (i=0; i < TERRAIN_LENGTH-1; i++) {
    	st->xcurvature[i] = 0.0;
	    st->ycurvature[i] = 0.0;
		st->zcurvature[i] = 0.0;
	}

    st->xcurvature[TERRAIN_LENGTH-1] = random_curvature();
    st->ycurvature[TERRAIN_LENGTH-1] = random_curvature();
    st->zcurvature[TERRAIN_LENGTH-1] = random_twist();

	generate_straight (st, 0, TERRAIN_LENGTH/4-1);
	generate_curves (st, TERRAIN_LENGTH/4, TERRAIN_LENGTH/2-1);
	generate_curves (st, TERRAIN_LENGTH/2, 3*TERRAIN_LENGTH/4-1);
	generate_straight (st, 3*TERRAIN_LENGTH/4, TERRAIN_LENGTH-1);

}

/*
 * render_quads (dpy, d, t, dt, i)
 *
 * renders the quadrilaterals from perspective depth t to t+dt.
 * i is passed as a hint, where i corresponds to t as asserted.
 */
static void
render_quads (struct state *st, Drawable d, int t, int dt, int i)
{
	int j, t2, j2, in;
	int index;
	XPoint points[4];
	GC gc;

	assert (i == (st->nearest - (t + dt) + TERRAIN_LENGTH) % TERRAIN_LENGTH);

	in = i + 1; MODULO(in, TERRAIN_LENGTH);

	for (j=0; j < TERRAIN_BREADTH; j+=dt) {
		t2 = t+dt; if (t2 >= TERRAIN_LENGTH) t2 -= TERRAIN_LENGTH;
		j2 = j+dt; if (j2 >= TERRAIN_BREADTH) j2 -= TERRAIN_BREADTH;
		points[0].x = st->xvals[t][j]; points[0].y = st->yvals[t][j];
		points[1].x = st->xvals[t2][j]; points[1].y = st->yvals[t2][j];
		points[2].x = st->xvals[t2][j2]; points[2].y = st->yvals[t2][j2];
		points[3].x = st->xvals[t][j2]; points[3].y = st->yvals[t][j2];

	    index = st->bonus_bright + st->ncolors/3 +
				t*(t*INTERP + st->pindex) * st->ncolors /
			    (3*TERRAIN_LENGTH*TERRAIN_PDIST);
		if (!wireframe) {
			index += (int)((points[0].y - points[3].y) / 8);
			index += (int)((st->worldx[i][j] - st->worldx[in][j]) / 40);
			index += (int)((st->terrain[in][j] - st->terrain[i][j]) / 100);
		}
		if (st->be_wormy && st->psychedelic_flag) index += st->ncolors/4;

		index = MIN (index, st->ncolors-1);
		index = MAX (index, 0);

		if (st->bonuses[i]) {
			XSetClipMask (st->dpy, st->bonus_gcs[index], None);
		}

		if (wireframe) {
			if (st->bonuses[i]) gc = st->bonus_gcs[index];
			else gc = st->ground_gcs[index];
			XDrawLines (st->dpy, d, gc, points, 4, CoordModeOrigin);
		} else {
			if (st->bonuses[i])
				gc = st->bonus_gcs[index];
			else if ((st->direction>0 && j < TERRAIN_BREADTH/8) ||
				(j > TERRAIN_BREADTH/8 && j < 3*TERRAIN_BREADTH/8-1) ||
				(st->direction < 0 && j > 3*TERRAIN_BREADTH/8-1 &&
					j < TERRAIN_BREADTH/2) ||
				st->terrain[i][j] == STEEL_ELEVATION ||
				st->wideness[in] - st->wideness[i] > 200) 
				gc = st->ground_gcs[index];
			else
				gc = st->wall_gcs[index];

			XFillPolygon (st->dpy, d, gc, points, 4, Nonconvex, CoordModeOrigin);
		}
	}
}

/*
 * render_pentagons (dpy, d, t, dt, i)
 *
 * renders the pentagons from perspective depth t to t+dt.
 * i is passed as a hint, where i corresponds to t as asserted.
 */
static void
render_pentagons (struct state *st, Drawable d, int t, int dt, int i)
{
	int j, t2, j2, j3, in;
	int index;
	XPoint points[5];
	GC gc;

	assert (i == (st->nearest -t + TERRAIN_LENGTH) % TERRAIN_LENGTH);

	in = i + 1; MODULO(in, TERRAIN_LENGTH);

	for (j=0; j < TERRAIN_BREADTH; j+=dt*2) {
		t2 = t+(dt*2); if (t2 >= TERRAIN_LENGTH) t2 -= TERRAIN_LENGTH;
		j2 = j+dt; if (j2 >= TERRAIN_BREADTH) j2 -= TERRAIN_BREADTH;
		j3 = j+dt+dt; if (j3 >= TERRAIN_BREADTH) j3 -= TERRAIN_BREADTH;
		points[0].x = st->xvals[t][j]; points[0].y = st->yvals[t][j];
		points[1].x = st->xvals[t2][j]; points[1].y = st->yvals[t2][j];
		points[2].x = st->xvals[t2][j2]; points[2].y = st->yvals[t2][j2];
		points[3].x = st->xvals[t2][j3]; points[3].y = st->yvals[t2][j3];
		points[4].x = st->xvals[t][j3]; points[4].y = st->yvals[t][j3];

	    index = st->bonus_bright + st->ncolors/3 +
				t*(t*INTERP + st->pindex) * st->ncolors /
			    (3*TERRAIN_LENGTH*TERRAIN_PDIST);
		if (!wireframe) {
			index += (int)((points[0].y - points[3].y) / 8);
			index += (int)((st->worldx[i][j] - st->worldx[in][j]) / 40);
			index += (int)((st->terrain[in][j] - st->terrain[i][j]) / 100);
		}
		if (st->be_wormy && st->psychedelic_flag) index += st->ncolors/4;

		index = MIN (index, st->ncolors-1);
		index = MAX (index, 0);

		if (st->bonuses[i]) {
			XSetClipMask (st->dpy, st->bonus_gcs[index], None);
		}

		if (wireframe) {
			if (st->bonuses[i]) gc = st->bonus_gcs[index];
			else gc = st->ground_gcs[index];
			XDrawLines (st->dpy, d, gc, points, 5, CoordModeOrigin);
		} else {
			if (st->bonuses[i])
				gc = st->bonus_gcs[index];
			else if (j < TERRAIN_BREADTH/8 ||
				(j > TERRAIN_BREADTH/8 && j < 3*TERRAIN_BREADTH/8-1) ||
				st->terrain[i][j] == STEEL_ELEVATION ||
				st->wideness[in] - st->wideness[i] > 200) 
				gc = st->ground_gcs[index];
			else
				gc = st->wall_gcs[index];

			XFillPolygon (st->dpy, d, gc, points, 5, Complex, CoordModeOrigin);
		}
	}
}

/*
 * render_block (dpy, d, gc, t)
 *
 * render a filled polygon at perspective depth t using the given GC
 */
static void
render_block (struct state *st, Drawable d, GC gc, int t)
{
	int i;

	XPoint erase_points[TERRAIN_BREADTH/2];

	for (i=0; i < TERRAIN_BREADTH/2; i++) {
		erase_points[i].x = st->xvals[t][i*2];
		erase_points[i].y = st->yvals[t][i*2];
	}

	XFillPolygon (st->dpy, d, gc, erase_points,
				  TERRAIN_BREADTH/2, Complex, CoordModeOrigin);
}

/*
 * regenerate_stars_mask (dpy, t)
 *
 * regenerate the clip mask 'stars_mask' for drawing the bonus stars at
 * random positions within the bounding box at depth t
 */
static void
regenerate_stars_mask (struct state *st, int t)
{
	int i, w, h, a, b, l1, l2;
	const int lim = st->width*TERRAIN_LENGTH/(300*(TERRAIN_LENGTH-t));

	w = st->maxx[t] - st->minx[t];
	h = st->maxy[t] - st->miny[t];

	if (w<6||h<6) return;

	XFillRectangle (st->dpy, st->stars_mask, st->stars_erase_gc,
					0, 0, st->width, st->height);

	l1 = (t>3*TERRAIN_LENGTH/4?2:1);
	l2 = (t>7*TERRAIN_LENGTH/8?2:1);

	for (i=0; i < lim; i++) {
		a = RAND(w); b = RAND(h);
		XDrawLine (st->dpy, st->stars_mask, st->stars_gc,
					st->minx[t]+a-l1, st->miny[t]+b, st->minx[t]+a+l1, st->miny[t]+b);
		XDrawLine (st->dpy, st->stars_mask, st->stars_gc,
					st->minx[t]+a, st->miny[t]+b-l1, st->minx[t]+a, st->miny[t]+b+l1);
	}
	for (i=0; i < lim; i++) {
		a = RAND(w); b = RAND(h);
		XDrawLine (st->dpy, st->stars_mask, st->stars_gc,
					st->minx[t]+a-l2, st->miny[t]+b, st->minx[t]+a+l2, st->miny[t]+b);
		XDrawLine (st->dpy, st->stars_mask, st->stars_gc,
					st->minx[t]+a, st->miny[t]+b-l2, st->minx[t]+a, st->miny[t]+b+l2);
	}
}

/*
 * render_bonus_block (dpy, d, t, i)
 *
 * draw the bonus stars at depth t.
 * i is passed as a hint, where i corresponds to t as asserted.
 */
static void
render_bonus_block (struct state *st, Drawable d, int t, int i)
{
	int bt;

	assert (i == (st->nearest -t + TERRAIN_LENGTH) % TERRAIN_LENGTH);

	if (!st->bonuses[i] || wireframe) return;

	regenerate_stars_mask (st, t);

	bt = t * st->nr_bonus_colors / (2*TERRAIN_LENGTH);

	XSetClipMask (st->dpy, st->bonus_gcs[bt], st->stars_mask);

	render_block (st, d, st->bonus_gcs[bt], t);
}

static int
begin_at (struct state *st)
{
	int t;
	int max_minx=0, min_maxx=st->width, max_miny=0, min_maxy=st->height;

	for (t=TERRAIN_LENGTH-1; t > 0; t--) {
		max_minx = MAX (max_minx, st->minx[t]);
		min_maxx = MIN (min_maxx, st->maxx[t]);
		max_miny = MAX (max_miny, st->miny[t]);
		min_maxy = MIN (min_maxy, st->maxy[t]);

		if (max_miny >= min_maxy || max_minx >= min_maxx) break;
	}

	return t;
}

/*
 * render_speedmine (dpy, d)
 *
 * render the current frame.
 */
static void
render_speedmine (struct state *st, Drawable d)
{
	int t, i=st->nearest, dt=1;
	GC gc;

	assert (st->nearest >= 0 && st->nearest < TERRAIN_LENGTH);

	if (st->be_wormy || wireframe) {
		XFillRectangle (st->dpy, d, st->erase_gc, 0, 0, st->width, st->height);

		dt=4;
		for (t=0; t < TERRAIN_LENGTH/4; t+=dt) {
			render_bonus_block (st, d, t, i);
			i -= dt; MODULO(i, TERRAIN_LENGTH);
			render_quads (st, d, t, dt, i);
		}

		assert (t == TERRAIN_LENGTH/4);
	} else {
		t = MAX(begin_at(st), TERRAIN_LENGTH/4);
		/*t = TERRAIN_LENGTH/4; dt = 2; */
		dt = (t >= 3*TERRAIN_LENGTH/4 ? 1 : 2);
		i = (st->nearest -t + TERRAIN_LENGTH) % TERRAIN_LENGTH;
		render_block (st, d, st->tunnelend_gc, t);
	}

	dt=2;

	if (t == TERRAIN_LENGTH/4)
		render_pentagons (st, d, t, dt, i);

	for (; t < 3*TERRAIN_LENGTH/4; t+=dt) {
		render_bonus_block (st, d, t, i);
		i -= dt; MODULO(i, TERRAIN_LENGTH);
		render_quads (st, d, t, dt, i);
	}

	dt=1;
	if (st->be_wormy) {
		for (; t < TERRAIN_LENGTH-(1+(st->pindex<INTERP/2)); t+=dt) {
			render_bonus_block (st, d, t, i);
			i -= dt; MODULO(i, TERRAIN_LENGTH);
		}
	} else {
		if (wireframe) {assert (t == 3*TERRAIN_LENGTH/4);}

		if (t == 3*TERRAIN_LENGTH/4)
			render_pentagons (st, d, t, dt, i);

		for (; t < TERRAIN_LENGTH-(1+(st->pindex<INTERP/2)); t+=dt) {
			render_bonus_block (st, d, t, i);
			i -= dt; MODULO(i, TERRAIN_LENGTH);
			render_quads (st, d, t, dt, i);
		}
	}

	/* Draw crosshair */
	if (st->crosshair_flag) {
		gc = (wireframe ? st->bonus_gcs[st->nr_bonus_colors/2] : st->erase_gc);
		XFillRectangle (st->dpy, d, gc,
						st->width/2+(st->xoffset)-8, st->height/2+(st->yoffset*2)-1, 16, 3);
		XFillRectangle (st->dpy, d, gc,
						st->width/2+(st->xoffset)-1, st->height/2+(st->yoffset*2)-8, 3, 16);
	}

}

/*
 * move (step)
 *
 * move to the position for the next frame, and modify the state variables
 * st->nearest, pindex, pos, speed
 */
static void
move (struct state *st)
{
	double dpos;

	st->pos += st->step;
	dpos = SIGN3(st->pos) * floor(fabs(st->pos));

	st->pindex += SIGN3(effective_speed) + INTERP;
	while (st->pindex >= INTERP) {
		st->nearest --;
		st->pindex -= INTERP;
	}
	while (st->pindex < 0) {
		st->nearest ++;
		st->pindex += INTERP;
	}

    st->nearest += dpos; MODULO(st->nearest, TERRAIN_LENGTH);

	st->pos -= dpos;

	st->accel = st->thrust + st->ycurvature[st->nearest] * st->gravity;
	st->speed += st->accel;
	if (st->speed > st->maxspeed) st->speed = st->maxspeed;
	if (st->speed < -st->maxspeed) st->speed = -st->maxspeed;
}

/*
 * speedmine (dpy, window)
 *
 * do everything required for one frame of the demo
 */
static unsigned long
speedmine_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
	double elapsed, time_per_frame = 0.04;

	regenerate_terrain (st);

	perspective (st);

	render_speedmine (st, st->dbuf);
    if (st->dbuf != st->window)
      XCopyArea (st->dpy, st->dbuf, st->window, st->draw_gc, 0, 0, st->width, st->height, 0, 0);

	st->fps_end = get_time(st);
	st->nframes++;
	st->total_nframes++;

	if (st->fps_end > st->fps_start + 0.5) {
		elapsed = st->fps_end - st->fps_start;
		st->fps_start = get_time(st);

		time_per_frame = elapsed / st->nframes - st->delay*1e-6;
		st->fps = st->nframes / elapsed;
		if (DEBUG_FLAG) {
			printf ("%f s elapsed\t%3f s/frame\t%.1f FPS\n", elapsed,
					time_per_frame, st->fps);
		}
		st->step = effective_speed * elapsed;

		st->nframes = 0;
	}


	move (st);

	decrement_bonuses (st, time_per_frame);

	check_bonuses (st);

    return st->delay;
}

/*
 * speedmine_color_ramp (dpy, gcs, colors, ncolors, s1, s2, v1, v2)
 *
 * generate a color ramp of up to *ncolors between randomly chosen hues,
 * varying from saturation s1 to s2 and value v1 to v2, placing the colors
 * in 'colors' and creating corresponding GCs in 'gcs'.
 *
 * The number of colors actually allocated is returned in ncolors.
 */
static void
speedmine_color_ramp (struct state *st, GC *gcs, XColor * colors,
					 int *ncolors, double s1, double s2, double v1, double v2)
{
	XGCValues gcv;
	int h1, h2;
	unsigned long flags;
	int i;

	assert (*st->ncolors >= 0);
	assert (s1 >= 0.0 && s1 <= 1.0 && v1 >= 0.0 && v2 <= 1.0);

	if (st->psychedelic_flag) {
		h1 = RAND(360); h2 = (h1 + 180) % 360;
	} else {
		h1 = h2 = RAND(360);
	}

	make_color_ramp (st->dpy, st->cmap, 
                     h1, s1, v1, h2, s2, v2,
				     colors, ncolors, False, True, False);

	flags = GCForeground;
	for (i=0; i < *ncolors; i++) {
		gcv.foreground = colors[i].pixel;
		gcs[i] = XCreateGC (st->dpy, st->dbuf, flags, &gcv);
	}

}

/*
 * change_colors ()
 *
 * perform the color changing bonus. New colors are allocated for the
 * walls and bonuses, and if the 'psychedelic' option is set then new
 * colors are also chosen for the ground.
 */
static void
change_colors (struct state *st)
{
	double s1, s2;

	if (st->psychedelic_flag) {
		free_colors (st->dpy, st->cmap, st->bonus_colors, st->nr_bonus_colors);
		free_colors (st->dpy, st->cmap, st->wall_colors, st->nr_wall_colors);
		free_colors (st->dpy, st->cmap, st->ground_colors, st->nr_ground_colors);
		st->ncolors = MAX_COLORS;

		s1 = 0.4; s2 = 0.9;

  		speedmine_color_ramp (st, st->ground_gcs, st->ground_colors,
							  &st->ncolors, 0.0, 0.8, 0.0, 0.9);
  		st->nr_ground_colors = st->ncolors;
	} else {
		free_colors (st->dpy, st->cmap, st->bonus_colors, st->nr_bonus_colors);
		free_colors (st->dpy, st->cmap, st->wall_colors, st->nr_wall_colors);
		st->ncolors = st->nr_ground_colors;

		s1 = 0.0; s2 = 0.6;
	}

    speedmine_color_ramp (st, st->wall_gcs, st->wall_colors, &st->ncolors,
				 		  s1, s2, 0.0, 0.9);
    st->nr_wall_colors = st->ncolors;

    speedmine_color_ramp (st, st->bonus_gcs, st->bonus_colors, &st->ncolors,
				  		  0.6, 0.9, 0.4, 1.0);
	st->nr_bonus_colors = st->ncolors;
}

/*
 * init_psychedelic_colors (dpy, window, cmap)
 *
 * initialise a psychedelic colormap
 */
static void
init_psychedelic_colors (struct state *st)
{
  XGCValues gcv;

  gcv.foreground = get_pixel_resource (st->dpy, st->cmap, "tunnelend", "TunnelEnd");
  st->tunnelend_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

  st->ncolors = MAX_COLORS;

  speedmine_color_ramp (st, st->ground_gcs, st->ground_colors, &st->ncolors,
				  		0.0, 0.8, 0.0, 0.9);
  st->nr_ground_colors = st->ncolors;

  speedmine_color_ramp (st, st->wall_gcs, st->wall_colors, &st->ncolors,
				 		0.0, 0.6, 0.0, 0.9);
  st->nr_wall_colors = st->ncolors;

  speedmine_color_ramp (st, st->bonus_gcs, st->bonus_colors, &st->ncolors,
				  		0.6, 0.9, 0.4, 1.0);
  st->nr_bonus_colors = st->ncolors;
}

/*
 * init_colors (dpy, window, cmap)
 *
 * initialise a normal colormap
 */
static void
init_colors (struct state *st)
{
  XGCValues gcv;
  XColor dark, light;
  int h1, h2;
  double s1, s2, v1, v2;
  unsigned long flags;
  int i;

  gcv.foreground = get_pixel_resource (st->dpy, st->cmap, "tunnelend", "TunnelEnd");
  st->tunnelend_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

  st->ncolors = MAX_COLORS;

  dark.pixel = get_pixel_resource (st->dpy, st->cmap, "darkground", "DarkGround");
  XQueryColor (st->dpy, st->cmap, &dark);

  light.pixel = get_pixel_resource (st->dpy, st->cmap, "lightground", "LightGround");
  XQueryColor (st->dpy, st->cmap, &light);

  rgb_to_hsv (dark.red, dark.green, dark.blue, &h1, &s1, &v1);
  rgb_to_hsv (light.red, light.green, light.blue, &h2, &s2, &v2);
  make_color_ramp (st->dpy, st->cmap, h1, s1, v1, h2, s2, v2,
				  st->ground_colors, &st->ncolors, False, True, False);
  st->nr_ground_colors = st->ncolors;

  flags = GCForeground;
  for (i=0; i < st->ncolors; i++) {
	gcv.foreground = st->ground_colors[i].pixel;
	st->ground_gcs[i] = XCreateGC (st->dpy, st->dbuf, flags, &gcv);
  }

  speedmine_color_ramp (st, st->wall_gcs, st->wall_colors, &st->ncolors,
				 		0.0, 0.6, 0.0, 0.9);
  st->nr_wall_colors = st->ncolors;

  speedmine_color_ramp (st, st->bonus_gcs, st->bonus_colors, &st->ncolors,
				  		0.6, 0.9, 0.4, 1.0);
  st->nr_bonus_colors = st->ncolors;
}

/*
 * print_stats ()
 *
 * print out average FPS stats for the demo
 */
#if 0
static void
print_stats (struct state *st)
{
	if (st->total_nframes >= 1)
		printf ("Rendered %d frames averaging %f FPS\n", st->total_nframes,
				st->total_nframes / get_time(st));
}
#endif

/*
 * init_speedmine (dpy, window)
 *
 * initialise the demo
 */
static void *
speedmine_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  int i;
  double th;
  int wide;

  st->dpy = dpy;
  st->window = window;

  st->speed = 1.1;
  st->accel = 0.00000001;
  st->direction = FORWARDS;
  st->orientation = (17*ROTS)/22;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->cmap = xgwa.colormap;
  st->width = xgwa.width;
  st->height = xgwa.height;

  st->verbose_flag = get_boolean_resource (st->dpy, "verbose", "Boolean");

# ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
  st->dbuf = st->window;
#else
  st->dbuf = XCreatePixmap (st->dpy, st->window, st->width, st->height, xgwa.depth);
#endif
  st->stars_mask = XCreatePixmap (st->dpy, st->window, st->width, st->height, 1);

  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");
  st->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  gcv.foreground = 1;
  st->stars_gc = XCreateGC (st->dpy, st->stars_mask, GCForeground, &gcv);

  gcv.foreground = get_pixel_resource (st->dpy, st->cmap, "background", "Background");
  st->erase_gc = XCreateGC (st->dpy, st->dbuf, GCForeground, &gcv);
  gcv.foreground = 0;
  st->stars_erase_gc = XCreateGC (st->dpy, st->stars_mask, GCForeground, &gcv);

  st->wire_flag = get_boolean_resource (st->dpy, "wire", "Boolean");

  st->psychedelic_flag = get_boolean_resource (st->dpy, "psychedelic", "Boolean");

  st->delay = get_integer_resource(st->dpy, "delay", "Integer");

  st->smoothness = get_integer_resource(st->dpy, "smoothness", "Integer");
  if (st->smoothness < 1) st->smoothness = 1;

  st->maxspeed = get_float_resource(st->dpy, "maxspeed", "Float");
  st->maxspeed *= 0.01;
  st->maxspeed = fabs(st->maxspeed);

  st->thrust = get_float_resource(st->dpy, "thrust", "Float");
  st->thrust *= 0.2;

  st->gravity = get_float_resource(st->dpy, "gravity", "Float");
  st->gravity *= 0.002/9.8;

  st->vertigo = get_float_resource(st->dpy, "vertigo", "Float");
  st->vertigo *= 0.2;

  st->curviness = get_float_resource(st->dpy, "curviness", "Float");
  st->curviness *= 0.25;

  st->twistiness = get_float_resource(st->dpy, "twistiness", "Float");
  st->twistiness *= 0.125;

  st->terrain_flag = get_boolean_resource (st->dpy, "terrain", "Boolean");
  st->widening_flag = get_boolean_resource (st->dpy, "widening", "Boolean");
  st->bumps_flag = get_boolean_resource (st->dpy, "bumps", "Boolean");
  st->bonuses_flag = get_boolean_resource (st->dpy, "bonuses", "Boolean");
  st->crosshair_flag = get_boolean_resource (st->dpy, "crosshair", "Boolean");

  st->be_wormy = get_boolean_resource (st->dpy, "worm", "Boolean");
  if (st->be_wormy) {
      st->maxspeed   *= 1.43;
      st->thrust     *= 10;
      st->gravity    *= 3;
      st->vertigo    *= 0.5;
      st->smoothness *= 2;
      st->curviness  *= 2;
      st->twistiness *= 2;
      st->psychedelic_flag = True;
      st->crosshair_flag = False;
  }

  if (st->psychedelic_flag) init_psychedelic_colors (st);
  else init_colors (st);

  for (i=0; i<ROTS; i++) {
	th = M_PI * 2.0 * i / ROTS;
	st->costab[i] = cos(th);
	st->sintab[i] = sin(th);
  }

  wide = random_wideness();

  for (i=0; i < TERRAIN_LENGTH; i++) {
	st->wideness[i] = wide;
	st->bonuses[i] = 0;
  }

  init_terrain (st);
  init_curves (st);
  wrap_tunnel (st, 0, TERRAIN_LENGTH-1);

#if 0
  if (DEBUG_FLAG || st->verbose_flag) atexit(print_stats);
#endif

  st->step = effective_speed;

  init_time (st);

  return st;
}


static void
speedmine_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
speedmine_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
speedmine_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}



/*
 * Down the speedmine, you'll find speed
 * to satisfy your moving needs;
 * So if you're looking for a blast
 * then hit the speedmine, really fast.
 */

/*
 * Speedworm likes to choke and spit
 * and chase his tail, and dance a bit
 * he really is a funky friend;
 * he's made of speed from end to end.
 */


static const char *speedmine_defaults [] = {
  ".verbose: False",
  "*worm: False",
  "*wire: False",
  ".background:	black",
  ".foreground:	white",
  "*darkground: #101010",
  "*lightground: #a0a0a0",
  "*tunnelend: #000000",
  "*delay:	30000",
  "*maxspeed: 700",
  "*thrust: 1.0",
  "*gravity: 9.8",
  "*vertigo: 1.0",
  "*terrain: True",
  "*smoothness: 6",
  "*curviness: 1.0",
  "*twistiness: 1.0",
  "*widening: True",
  "*bumps: True",
  "*bonuses: True",
  "*crosshair: True",
  "*psychedelic: False",
  0
};

static XrmOptionDescRec speedmine_options [] = {
  { "-verbose",			".verbose",				XrmoptionNoArg, "True"},
  { "-worm",			".worm",				XrmoptionNoArg, "True"},
  { "-wireframe",		".wire",				XrmoptionNoArg, "True"},
  { "-no-wireframe",	".wire",				XrmoptionNoArg, "False"},
  { "-darkground",		".darkground",			XrmoptionSepArg, 0 },
  { "-lightground",		".lightground",			XrmoptionSepArg, 0 },
  { "-tunnelend",		".tunnelend",			XrmoptionSepArg, 0 },
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { "-maxspeed",		".maxspeed",			XrmoptionSepArg, 0 },
  { "-thrust",			".thrust",				XrmoptionSepArg, 0 },
  { "-gravity",			".gravity",				XrmoptionSepArg, 0 },
  { "-vertigo",			".vertigo",				XrmoptionSepArg, 0 },
  { "-terrain",			".terrain",				XrmoptionNoArg, "True"},
  { "-no-terrain",		".terrain",				XrmoptionNoArg, "False"},
  { "-smoothness",      ".smoothness",			XrmoptionSepArg, 0 },
  { "-curviness",		".curviness",			XrmoptionSepArg, 0 },
  { "-twistiness",		".twistiness",			XrmoptionSepArg, 0 },
  { "-widening",		".widening",			XrmoptionNoArg, "True"},
  { "-no-widening",		".widening",			XrmoptionNoArg, "False"},
  { "-bumps",			".bumps",				XrmoptionNoArg, "True"},
  { "-no-bumps",		".bumps",				XrmoptionNoArg, "False"},
  { "-bonuses",			".bonuses",				XrmoptionNoArg, "True"},
  { "-no-bonuses",		".bonuses",				XrmoptionNoArg, "False"},
  { "-crosshair",		".crosshair",			XrmoptionNoArg, "True"},
  { "-no-crosshair",	".crosshair",			XrmoptionNoArg, "False"},
  { "-psychedelic",		".psychedelic",			XrmoptionNoArg, "True"},
  { "-no-psychedelic",	".psychedelic",			XrmoptionNoArg, "False"},
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Speedmine", speedmine)

/* vim: ts=4
 */
