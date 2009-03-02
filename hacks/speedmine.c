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

static Display * display;
static Pixmap dbuf, stars_mask;
static Colormap cmap;
static unsigned int default_fg_pixel;
static GC draw_gc, erase_gc, tunnelend_gc, stars_gc, stars_erase_gc;

/* No. of shades of each color (ground, walls, bonuses) */
#define MAX_COLORS 32
static int ncolors, nr_ground_colors, nr_wall_colors, nr_bonus_colors;
static XColor ground_colors[MAX_COLORS], wall_colors[MAX_COLORS];
static XColor bonus_colors[MAX_COLORS];
static GC ground_gcs[MAX_COLORS], wall_gcs[MAX_COLORS], bonus_gcs[MAX_COLORS];

static int be_wormy;

static int width, height;
static int delay;

static int smoothness;
static int verbose_flag;
static int wire_flag;
static int terrain_flag;
static int widening_flag;
static int bumps_flag;
static int bonuses_flag;
static int crosshair_flag;
static int psychedelic_flag;

#ifdef NDEBUG
#define DEBUG_FLAG 0
#else
#define DEBUG_FLAG 1
#endif

static double maxspeed;

static double thrust, gravity;

static double vertigo;
static double curviness;
static double twistiness;

static double pos=0.0;
static double speed=-1.1;
static double accel=0.00000001;
static double step=0.0;


#define FORWARDS 1
#define BACKWARDS -1
static int direction = FORWARDS;

static int pindex=0, nearest=0;
static int flipped_at=0;
static int xoffset=0, yoffset=0;

static int bonus_bright = 0;
static int wire_bonus=0;
#define wireframe (wire_flag||wire_bonus>8||wire_bonus%2==1)

static double speed_bonus=0.0;
#define effective_speed (direction*(speed+speed_bonus))

static int spin_bonus = 0;
static int backwards_bonus = 0;

/* No. of levels of interpolation, for perspective */
#define INTERP 32

/* These must be powers of 2 */
#define TERRAIN_LENGTH 256
#define TERRAIN_BREADTH 32

/* total "perspective distance" of terrain */
#define TERRAIN_PDIST (INTERP*TERRAIN_LENGTH)

#define ROTS 1024
#define TB_MUL (ROTS/TERRAIN_BREADTH)
static double sintab[ROTS], costab[ROTS];

static int orientation = (17*ROTS)/22;

static int terrain[TERRAIN_LENGTH][TERRAIN_BREADTH];
static double xcurvature[TERRAIN_LENGTH];
static double ycurvature[TERRAIN_LENGTH];
static double zcurvature[TERRAIN_LENGTH];
static int wideness[TERRAIN_LENGTH];
static int bonuses[TERRAIN_LENGTH];
static int xvals[TERRAIN_LENGTH][TERRAIN_BREADTH];
static int yvals[TERRAIN_LENGTH][TERRAIN_BREADTH];
static double worldx[TERRAIN_LENGTH][TERRAIN_BREADTH];
static double worldy[TERRAIN_LENGTH][TERRAIN_BREADTH];
static int minx[TERRAIN_LENGTH], maxx[TERRAIN_LENGTH];
static int miny[TERRAIN_LENGTH], maxy[TERRAIN_LENGTH];

#define random_elevation() (terrain_flag?(random() % 200):0)
#define random_curvature() (curviness>0.0?((double)(random() % 40)-20)*curviness:0.0)
#define random_twist() (twistiness>0.0?((double)(random() % 40)-20)*twistiness:0.0)
#define random_wideness() (widening_flag?(int)(random() % 1200):0)

#define STEEL_ELEVATION 300

/* a forward declaration ... */
static void change_colors(void);

#if HAVE_GETTIMEOFDAY
static int total_nframes = 0;
static int nframes = 0;
static double fps = 0.0;
static double fps_start, fps_end;
static struct timeval start_time;

/*
 * get_time ()
 *
 * returns the total time elapsed since the beginning of the demo
 */
static double get_time(void) {
  struct timeval t;
  float f;
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&t, NULL);
#else
  gettimeofday(&t);
#endif
  t.tv_sec -= start_time.tv_sec;
  f = ((double)t.tv_sec) + t.tv_usec*1e-6;
  return f;
}

/*
 * init_time ()
 *
 * initialises the timing structures
 */
static void init_time(void) {
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&start_time, NULL);
#else
  gettimeofday(&start_time);
#endif
  fps_start = get_time();
}
#endif

/*
 * perspective()
 *
 * perspective map the world coordinates worldx[i][j], worldy[i][j] onto
 * screen coordinates xvals[t][j], yvals[t][j]
 */
static void
perspective (void)
{
	static int rotation_offset=0;

	int i, j, jj, t=0, depth, view_pos;
   	int rotation_bias, r;
	double xc=0.0, yc=0.0, zc=0.0;
	double xcc=0.0, ycc=0.0, zcc=0.0;
	double xx, yy;
	double zfactor, zf;

	zf = 8.0*28.0 / (double)(width*TERRAIN_LENGTH);
	if (be_wormy) zf *= 3.0;

	depth = TERRAIN_PDIST - INTERP + pindex;

	view_pos = (nearest+3*TERRAIN_LENGTH/4)%TERRAIN_LENGTH;
	
	xoffset += - xcurvature[view_pos]*curviness/8;
	xoffset /= 2;

	yoffset += - ycurvature[view_pos]*curviness/4;
	yoffset /= 2;

	rotation_offset += (int)((zcurvature[view_pos]-zcurvature[nearest])*ROTS/8);
	rotation_offset /= 2;
	rotation_bias = orientation + spin_bonus - rotation_offset;

	if (bumps_flag) {
		if (be_wormy) {
			yoffset -= ((terrain[view_pos][TERRAIN_BREADTH/4] * width /(8*1600)));
			rotation_bias += (terrain[view_pos][TERRAIN_BREADTH/4+2] -
							 terrain[view_pos][TERRAIN_BREADTH/4-2])/8;
		} else {
			yoffset -= ((terrain[view_pos][TERRAIN_BREADTH/4] * width /(2*1600)));
			rotation_bias += (terrain[view_pos][TERRAIN_BREADTH/4+2] -
							 terrain[view_pos][TERRAIN_BREADTH/4-2])/16;
		}
	}

	MODULO(rotation_bias, ROTS);

	for (t=0; t < TERRAIN_LENGTH; t++) {
		i = nearest + t; MODULO(i, TERRAIN_LENGTH);
		xc += xcurvature[i]; yc += ycurvature[i]; zc += zcurvature[i];
		xcc += xc; ycc += yc; zcc += zc;
		maxx[i] = maxy[i] = 0;
		minx[i] = width; miny[i] = height;
	}

	for (t=0; t < TERRAIN_LENGTH; t++) {
		i = nearest - 1 - t; MODULO(i, TERRAIN_LENGTH);

		zfactor = (double)depth* (12.0 - TERRAIN_LENGTH/8.0) * zf;
		for (j=0; j < TERRAIN_BREADTH; j++) {
			jj = direction * j; MODULO(jj, TERRAIN_BREADTH);
			xx = (worldx[i][jj]-(vertigo*xcc))/zfactor; 
			yy = (worldy[i][j]-(vertigo*ycc))/zfactor;

			r = rotation_bias + (int)(vertigo*zcc); MODULO(r, ROTS);

			xvals[t][j] = xoffset + width/2 +
				       	  (int)(xx * costab[r] - yy * sintab[r]);
			maxx[t] = MAX(maxx[t], xvals[t][j]);
			minx[t] = MIN(minx[t], xvals[t][j]);

			yvals[t][j] = yoffset + height/2 +
						  (int)(xx * sintab[r] + yy * costab[r]);
			maxy[t] = MAX(maxy[t], yvals[t][j]);
			miny[t] = MIN(miny[t], yvals[t][j]);
		}
		xcc -= xc; ycc -= yc; zcc -= zc;
		xc -= xcurvature[i]; yc -= ycurvature[i]; zc -= zcurvature[i];
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
wrap_tunnel (int start, int end)
{
	int i, j, v;
	double x, y;

	assert (start < end);

	for (i=start; i <= end; i++) {
		for (j=0; j < TERRAIN_BREADTH; j++) {
			x = j * (1.0/TERRAIN_BREADTH);
			v = terrain[i][j];
			y = (double)(v==STEEL_ELEVATION?200:v) - wideness[i] - 1200;

			/* lower road */
			if (j > TERRAIN_BREADTH/8 && j < 3*TERRAIN_BREADTH/8) y -= 300;

		    worldx[i][j] = x/2 * costab[j*TB_MUL] -
				 			(y-height/4.0)*x*sintab[j*TB_MUL];
			worldy[i][j] = x/4 * sintab[j*TB_MUL] +
							y * costab[j*TB_MUL];
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
flip_direction (void)
{
	int i, ip, in, j, t;

	direction = -direction;

	bonus_bright = 20;

	for (i=0; i < TERRAIN_LENGTH; i++) {
		in = nearest + i; MODULO(in, TERRAIN_BREADTH);
		ip = nearest - i; MODULO(ip, TERRAIN_BREADTH);
		for (j=0; j < TERRAIN_BREADTH; j++) {
			t = terrain[ip][j];
			terrain[ip][j] = terrain[in][j];
			terrain[in][j] = t;
		}
	}
}

/*
 * generate_smooth (start, end)
 *
 * generate smooth terrain between i=start and i=end inclusive
 */
static void
generate_smooth (int start, int end)
{
	int i,j, ii;

	assert (start < end);

	for (i=start; i <= end; i++) {
		ii = i; MODULO(ii, TERRAIN_LENGTH);
		for (j=0; j < TERRAIN_BREADTH; j++) {
			terrain[i][j] = STEEL_ELEVATION;
		}
	}
}

/*
 * generate_straight (start, end)
 *
 * zero the curvature and wideness between i=start and i=end inclusive
 */
static void
generate_straight (int start, int end)
{
	int i,j, ii;

	assert (start < end);

	for (i=start; i <= end; i++) {
		ii = i; MODULO(ii, TERRAIN_LENGTH);
		for (j=0; j < TERRAIN_BREADTH; j++) {
	  		xcurvature[ii] = 0;
	 		ycurvature[ii] = 0;
			zcurvature[ii] = 0;
			wideness[ii] = 0;
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
generate_terrain_value (int v1, int v2, int v3, int v4, int w)
{
	int sum, ret;
	int rval;

	if (!terrain_flag) return 0;

	sum = v1 + v2 + v3 + v4;

	rval = w*sum/smoothness;
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
generate_terrain (int start, int end, int final)
{
	int i,j,w,l;
	int ip, jp, in, jn; /* prev, next values */
	int diff;

	assert (start < end);
	assert (start >= 0 && start < TERRAIN_LENGTH);
	assert (end >= 0 && end < TERRAIN_LENGTH);

	diff = end - start + 1;

	terrain[end][0] = random_elevation();
	terrain[end][TERRAIN_BREADTH/2] = random_elevation();

	for (w= diff/2, l=TERRAIN_BREADTH/4;
	     w >= final || l >= final; w /= 2, l /= 2) {

		if (w<1) w=1; if (l<1) l=1;

		for (i=start+w-1; i < end; i += (w*2)) {
			ip = i-w; MODULO(ip, TERRAIN_LENGTH);
			in = i+w; MODULO(in, TERRAIN_LENGTH);
			for (j=l-1; j < TERRAIN_BREADTH; j += (l*2)) {
				jp = j-1; MODULO(jp, TERRAIN_BREADTH);
				jn = j+1; MODULO(jn, TERRAIN_BREADTH);
				terrain[i][j] =
					generate_terrain_value (terrain[ip][jp], terrain[in][jp], 
		                            terrain[ip][jn], terrain[in][jn], w);
		   	}
		}

		for (i=start+(w*2)-1; i < end; i += (w*2)) {
			ip = i-w; MODULO(ip, TERRAIN_LENGTH);
			in = i+w; MODULO(in, TERRAIN_LENGTH);
			for (j=l-1; j < TERRAIN_BREADTH; j += (l*2)) {
				jp = j-1; MODULO(jp, TERRAIN_BREADTH);
				jn = j+1; MODULO(jn, TERRAIN_BREADTH);
				terrain[i][j] =
					generate_terrain_value (terrain[ip][j], terrain[in][j],
		                            terrain[i][jp], terrain[i][jn], w);
		   	}
		}

		for (i=start+w-1; i < end; i += (w*2)) {
			ip = i-w; MODULO(ip, TERRAIN_LENGTH);
			in = i+w; MODULO(in, TERRAIN_LENGTH);
			for (j=2*l-1; j < TERRAIN_BREADTH; j += (l*2)) {
				jp = j-1; MODULO(jp, TERRAIN_BREADTH);
				jn = j+1; MODULO(jn, TERRAIN_BREADTH);
				terrain[i][j] =
					generate_terrain_value (terrain[ip][j], terrain[in][j],
		                            terrain[i][jp], terrain[i][jn], w);
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
generate_curves (int start, int end)
{
	int i, diff, ii, in, ip, w;

	assert (start < end);

	diff = end - start + 1; MODULO (diff, TERRAIN_LENGTH);

	if (random() % 100 == 0)
	  xcurvature[end] = 30 * random_curvature();
	else if (random() % 10 == 0)
	  xcurvature[end] = 20 * random_curvature();
	else
	  xcurvature[end] = 10 * random_curvature();

	if (random() % 50 == 0)
	  ycurvature[end] = 20 * random_curvature();
	else if (random() % 25 == 0)
	  ycurvature[end] = 30 * random_curvature();
	else
	  ycurvature[end] = 10 * random_curvature();

	if (random() % 3 == 0)
	  zcurvature[end] = random_twist();
	else
	  zcurvature[end] =
			  generate_curvature_value (zcurvature[end], random_twist(), 1);

	if (be_wormy)
			wideness[end] = random_wideness();
	else
		wideness[end] =
			generate_curvature_value (wideness[end], random_wideness(), 1);

    for (w=diff/2; w >= 1; w /= 2) {
      for (i=start+w-1; i < end; i+=(w*2)) {
        ii = i; MODULO (ii, TERRAIN_LENGTH);
  	  	ip = i-w; MODULO (ip, TERRAIN_LENGTH);
  	    in = i+w; MODULO (in, TERRAIN_LENGTH);
  	    xcurvature[ii] =
				generate_curvature_value (xcurvature[ip], xcurvature[in], w);
  	    ycurvature[ii] =
				generate_curvature_value (ycurvature[ip], ycurvature[in], w);
	    zcurvature[ii] =
				generate_curvature_value (zcurvature[ip], zcurvature[in], w);
	    wideness[ii] =
				generate_curvature_value (wideness[ip], wideness[in], w);
      }
    }
}

/*
 * do_bonus ()
 *
 * choose a random bonus and perform its state transition
 */
static void
do_bonus (void)
{
	static int jamming=0;

	bonus_bright = 20;

	if (jamming > 0) {
		jamming--;
		nearest -= 2; MODULO(nearest, TERRAIN_LENGTH);
		return;
	}

	if (psychedelic_flag) change_colors();

	switch (random() % 7) {
	case 0: /* switch to or from wireframe */
		wire_bonus = (wire_bonus?0:300);
		break;
	case 1: /* speedup */
		speed_bonus = 40.0;
		break;
	case 2:
		spin_bonus += ROTS;
		break;
	case 3:
		spin_bonus -= ROTS;
		break;
	case 4: /* look backwards / look forwards */
		flipped_at = nearest;
		flip_direction ();
		backwards_bonus = (backwards_bonus?0:10);
		break;
	case 5:
		change_colors();
		break;
	case 6: /* jam against the bonus a few times; deja vu! */
		nearest -= 2; MODULO(nearest, TERRAIN_LENGTH);
		jamming = 3;
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
check_bonuses (void)
{
	int i, ii, start, end;

	if (!bonuses_flag) return;

	if (step >= 0.0) {
		start = nearest; end = nearest + (int)floor(step);
	} else {
		end = nearest; start = nearest + (int)floor(step);
	}

	if (be_wormy) {
		start += TERRAIN_LENGTH/4;
		end += TERRAIN_LENGTH/4;
	}

	for (i=start; i < end; i++) {
		ii = i; MODULO(ii, TERRAIN_LENGTH);
		if (bonuses[ii] == 1) do_bonus ();
	}
}

/*
 * decrement_bonuses (double time_per_frame)
 *
 * decrement timers associated with bonuses
 */
static void
decrement_bonuses (double time_per_frame)
{
	if (!bonuses_flag) return;

	if (bonus_bright > 0) bonus_bright-=4;
	if (wire_bonus > 0) wire_bonus--;
	if (speed_bonus > 0) speed_bonus -= 2.0;

	if (spin_bonus > 10) spin_bonus -= (int)(step*13.7);
	else if (spin_bonus < -10) spin_bonus += (int)(step*11.3);

	if (backwards_bonus > 1) backwards_bonus--;
	else if (backwards_bonus == 1) {
		nearest += 2*(MAX(flipped_at, nearest) - MIN(flipped_at,nearest));
		MODULO(nearest, TERRAIN_LENGTH);
		flip_direction ();
		backwards_bonus = 0;
	}
}

/*
 * set_bonuses (start, end)
 *
 * choose if to and where to set a bonus between i=start and i=end inclusive
 */
static void
set_bonuses (int start, int end)
{
	int i, diff, ii;

	if (!bonuses_flag) return;

	assert (start < end);

	diff = end - start;

	for (i=start; i <= end; i++) {
		ii = i; if (ii>=TERRAIN_LENGTH) ii -= TERRAIN_LENGTH;
		bonuses[ii] = 0;
	}
	if (random() % 4 == 0) {
		i = start + RAND(diff-3);
		ii = i; if (ii>=TERRAIN_LENGTH) ii -= TERRAIN_LENGTH;
		bonuses[ii] = 2; /* marker */
		ii = i+3; if (ii>=TERRAIN_LENGTH) ii -= TERRAIN_LENGTH;
		bonuses[ii] = 1; /* real thing */
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
regenerate_terrain (void)
{
	int start, end;
	int passed;

	passed = nearest % (TERRAIN_LENGTH/4);

	if (speed == 0.0 ||
		(speed > 0.0 && passed > (int)step) ||
		(speed < 0.0 && (TERRAIN_LENGTH/4)-passed > (int)fabs(step))) {

		return;
	}

	end = nearest - passed - 1; MODULO(end, TERRAIN_LENGTH);
	start = end - TERRAIN_LENGTH/4 + 1; MODULO(start, TERRAIN_LENGTH);

	if (DEBUG_FLAG) printf ("Regenerating [%d - %d]\n", start, end);

	set_bonuses (start, end);

	switch (random() % 64) {
	case 0:
	case 1:
		generate_terrain (start, end, 1);
		generate_smooth (start,
			start + TERRAIN_LENGTH/8 + (random() % TERRAIN_LENGTH/8));
		break;
	case 2:
		generate_smooth (start, end);
		generate_terrain (start, end, 4); break;
	case 3:
		generate_smooth (start, end);
		generate_terrain (start, end, 2); break;
	default:
		generate_terrain (start, end, 1);
	}

	if (random() % 16 == 0) {
		generate_straight (start, end);
	} else {
		generate_curves (start, end);
	}

	wrap_tunnel (start, end);
}

/*
 * init_terrain ()
 *
 * initialise the terrain map for the beginning of the demo
 */
static void
init_terrain (void)
{
	int i, j;

	for (i=0; i < TERRAIN_LENGTH; i++) {
		for (j=0; j < TERRAIN_BREADTH; j++) {
			terrain[i][j] = 0;
		}
	}

	terrain[TERRAIN_LENGTH-1][0] =  - (random() % 300);
	terrain[TERRAIN_LENGTH-1][TERRAIN_BREADTH/2] =  - (random() % 300);

	generate_smooth (0, TERRAIN_LENGTH-1);
	generate_terrain (0, TERRAIN_LENGTH/4 -1, 4);
	generate_terrain (TERRAIN_LENGTH/4, TERRAIN_LENGTH/2 -1, 2);
	generate_terrain (TERRAIN_LENGTH/2, 3*TERRAIN_LENGTH/4 -1, 1);
	generate_smooth (3*TERRAIN_LENGTH/4, TERRAIN_LENGTH-1);
}

/*
 * init_curves ()
 *
 * initialise the curvatures and wideness for the beginning of the demo.
 */
static void
init_curves (void)
{
	int i;

	for (i=0; i < TERRAIN_LENGTH-1; i++) {
    	xcurvature[i] = 0.0;
	    ycurvature[i] = 0.0;
		zcurvature[i] = 0.0;
	}

    xcurvature[TERRAIN_LENGTH-1] = random_curvature();
    ycurvature[TERRAIN_LENGTH-1] = random_curvature();
    zcurvature[TERRAIN_LENGTH-1] = random_twist();

	generate_straight (0, TERRAIN_LENGTH/4-1);
	generate_curves (TERRAIN_LENGTH/4, TERRAIN_LENGTH/2-1);
	generate_curves (TERRAIN_LENGTH/2, 3*TERRAIN_LENGTH/4-1);
	generate_straight (3*TERRAIN_LENGTH/4, TERRAIN_LENGTH-1);

}

/*
 * render_quads (dpy, d, t, dt, i)
 *
 * renders the quadrilaterals from perspective depth t to t+dt.
 * i is passed as a hint, where i corresponds to t as asserted.
 */
static void
render_quads (Display * dpy, Drawable d, int t, int dt, int i)
{
	int j, t2, j2, in;
	int index;
	XPoint points[4];
	GC gc;

	assert (i == (nearest - (t + dt) + TERRAIN_LENGTH) % TERRAIN_LENGTH);

	in = i + 1; MODULO(in, TERRAIN_LENGTH);

	for (j=0; j < TERRAIN_BREADTH; j+=dt) {
		t2 = t+dt; if (t2 >= TERRAIN_LENGTH) t2 -= TERRAIN_LENGTH;
		j2 = j+dt; if (j2 >= TERRAIN_BREADTH) j2 -= TERRAIN_BREADTH;
		points[0].x = xvals[t][j]; points[0].y = yvals[t][j];
		points[1].x = xvals[t2][j]; points[1].y = yvals[t2][j];
		points[2].x = xvals[t2][j2]; points[2].y = yvals[t2][j2];
		points[3].x = xvals[t][j2]; points[3].y = yvals[t][j2];

	    index = bonus_bright + ncolors/3 +
				t*(t*INTERP + pindex) * ncolors /
			    (3*TERRAIN_LENGTH*TERRAIN_PDIST);
		if (!wireframe) {
			index += (int)((points[0].y - points[3].y) / 8);
			index += (int)((worldx[i][j] - worldx[in][j]) / 40);
			index += (int)((terrain[in][j] - terrain[i][j]) / 100);
		}
		if (be_wormy && psychedelic_flag) index += ncolors/4;

		index = MIN (index, ncolors-1);
		index = MAX (index, 0);

		if (bonuses[i]) {
			XSetClipMask (dpy, bonus_gcs[index], None);
		}

		if (wireframe) {
			if (bonuses[i]) gc = bonus_gcs[index];
			else gc = ground_gcs[index];
			XDrawLines (dpy, d, gc, points, 4, CoordModeOrigin);
		} else {
			if (bonuses[i])
				gc = bonus_gcs[index];
			else if ((direction>0 && j < TERRAIN_BREADTH/8) ||
				(j > TERRAIN_BREADTH/8 && j < 3*TERRAIN_BREADTH/8-1) ||
				(direction < 0 && j > 3*TERRAIN_BREADTH/8-1 &&
					j < TERRAIN_BREADTH/2) ||
				terrain[i][j] == STEEL_ELEVATION ||
				wideness[in] - wideness[i] > 200) 
				gc = ground_gcs[index];
			else
				gc = wall_gcs[index];

			XFillPolygon (dpy, d, gc, points, 4, Nonconvex, CoordModeOrigin);
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
render_pentagons (Display *dpy, Drawable d, int t, int dt, int i)
{
	int j, t2, j2, j3, in;
	int index;
	XPoint points[5];
	GC gc;

	assert (i == (nearest -t + TERRAIN_LENGTH) % TERRAIN_LENGTH);

	in = i + 1; MODULO(in, TERRAIN_LENGTH);

	for (j=0; j < TERRAIN_BREADTH; j+=dt*2) {
		t2 = t+(dt*2); if (t2 >= TERRAIN_LENGTH) t2 -= TERRAIN_LENGTH;
		j2 = j+dt; if (j2 >= TERRAIN_BREADTH) j2 -= TERRAIN_BREADTH;
		j3 = j+dt+dt; if (j3 >= TERRAIN_BREADTH) j3 -= TERRAIN_BREADTH;
		points[0].x = xvals[t][j]; points[0].y = yvals[t][j];
		points[1].x = xvals[t2][j]; points[1].y = yvals[t2][j];
		points[2].x = xvals[t2][j2]; points[2].y = yvals[t2][j2];
		points[3].x = xvals[t2][j3]; points[3].y = yvals[t2][j3];
		points[4].x = xvals[t][j3]; points[4].y = yvals[t][j3];

	    index = bonus_bright + ncolors/3 +
				t*(t*INTERP + pindex) * ncolors /
			    (3*TERRAIN_LENGTH*TERRAIN_PDIST);
		if (!wireframe) {
			index += (int)((points[0].y - points[3].y) / 8);
			index += (int)((worldx[i][j] - worldx[in][j]) / 40);
			index += (int)((terrain[in][j] - terrain[i][j]) / 100);
		}
		if (be_wormy && psychedelic_flag) index += ncolors/4;

		index = MIN (index, ncolors-1);
		index = MAX (index, 0);

		if (bonuses[i]) {
			XSetClipMask (dpy, bonus_gcs[index], None);
		}

		if (wireframe) {
			if (bonuses[i]) gc = bonus_gcs[index];
			else gc = ground_gcs[index];
			XDrawLines (dpy, d, gc, points, 5, CoordModeOrigin);
		} else {
			if (bonuses[i])
				gc = bonus_gcs[index];
			else if (j < TERRAIN_BREADTH/8 ||
				(j > TERRAIN_BREADTH/8 && j < 3*TERRAIN_BREADTH/8-1) ||
				terrain[i][j] == STEEL_ELEVATION ||
				wideness[in] - wideness[i] > 200) 
				gc = ground_gcs[index];
			else
				gc = wall_gcs[index];

			XFillPolygon (dpy, d, gc, points, 5, Complex, CoordModeOrigin);
		}
	}
}

/*
 * render_block (dpy, d, gc, t)
 *
 * render a filled polygon at perspective depth t using the given GC
 */
static void
render_block (Display * dpy, Drawable d, GC gc, int t)
{
	int i;

	XPoint erase_points[TERRAIN_BREADTH/2];

	for (i=0; i < TERRAIN_BREADTH/2; i++) {
		erase_points[i].x = xvals[t][i*2];
		erase_points[i].y = yvals[t][i*2];
	}

	XFillPolygon (dpy, d, gc, erase_points,
				  TERRAIN_BREADTH/2, Complex, CoordModeOrigin);
}

/*
 * regenerate_stars_mask (dpy, t)
 *
 * regenerate the clip mask 'stars_mask' for drawing the bonus stars at
 * random positions within the bounding box at depth t
 */
static void
regenerate_stars_mask (Display * dpy, int t)
{
	int i, w, h, a, b, l1, l2;
	const int lim = width*TERRAIN_LENGTH/(300*(TERRAIN_LENGTH-t));

	w = maxx[t] - minx[t];
	h = maxy[t] - miny[t];

	if (w<6||h<6) return;

	XFillRectangle (dpy, stars_mask, stars_erase_gc,
					0, 0, width, height);

	l1 = (t>3*TERRAIN_LENGTH/4?2:1);
	l2 = (t>7*TERRAIN_LENGTH/8?2:1);

	for (i=0; i < lim; i++) {
		a = RAND(w); b = RAND(h);
		XDrawLine (dpy, stars_mask, stars_gc,
					minx[t]+a-l1, miny[t]+b, minx[t]+a+l1, miny[t]+b);
		XDrawLine (dpy, stars_mask, stars_gc,
					minx[t]+a, miny[t]+b-l1, minx[t]+a, miny[t]+b+l1);
	}
	for (i=0; i < lim; i++) {
		a = RAND(w); b = RAND(h);
		XDrawLine (dpy, stars_mask, stars_gc,
					minx[t]+a-l2, miny[t]+b, minx[t]+a+l2, miny[t]+b);
		XDrawLine (dpy, stars_mask, stars_gc,
					minx[t]+a, miny[t]+b-l2, minx[t]+a, miny[t]+b+l2);
	}
}

/*
 * render_bonus_block (dpy, d, t, i)
 *
 * draw the bonus stars at depth t.
 * i is passed as a hint, where i corresponds to t as asserted.
 */
static void
render_bonus_block (Display * dpy, Drawable d, int t, int i)
{
	int bt;

	assert (i == (nearest -t + TERRAIN_LENGTH) % TERRAIN_LENGTH);

	if (!bonuses[i] || wireframe) return;

	regenerate_stars_mask (dpy, t);

	bt = t * nr_bonus_colors / (2*TERRAIN_LENGTH);

	XSetClipMask (dpy, bonus_gcs[bt], stars_mask);

	render_block (dpy, d, bonus_gcs[bt], t);
}

static int
begin_at (void)
{
	int t;
	int max_minx=0, min_maxx=width, max_miny=0, min_maxy=height;

	for (t=TERRAIN_LENGTH-1; t > 0; t--) {
		max_minx = MAX (max_minx, minx[t]);
		min_maxx = MIN (min_maxx, maxx[t]);
		max_miny = MAX (max_miny, miny[t]);
		min_maxy = MIN (min_maxy, maxy[t]);

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
render_speedmine (Display * dpy, Drawable d)
{
	int t, i=nearest, dt=1;
	GC gc;

	assert (nearest >= 0 && nearest < TERRAIN_LENGTH);

	if (be_wormy || wireframe) {
		XFillRectangle (dpy, d, erase_gc, 0, 0, width, height);

		dt=4;
		for (t=0; t < TERRAIN_LENGTH/4; t+=dt) {
			render_bonus_block (dpy, d, t, i);
			i -= dt; MODULO(i, TERRAIN_LENGTH);
			render_quads (dpy, d, t, dt, i);
		}

		assert (t == TERRAIN_LENGTH/4);
	} else {
		t = MAX(begin_at(), TERRAIN_LENGTH/4);
		/*t = TERRAIN_LENGTH/4; dt = 2; */
		dt = (t >= 3*TERRAIN_LENGTH/4 ? 1 : 2);
		i = (nearest -t + TERRAIN_LENGTH) % TERRAIN_LENGTH;
		render_block (dpy, d, tunnelend_gc, t);
	}

	dt=2;

	if (t == TERRAIN_LENGTH/4)
		render_pentagons (dpy, d, t, dt, i);

	for (; t < 3*TERRAIN_LENGTH/4; t+=dt) {
		render_bonus_block (dpy, d, t, i);
		i -= dt; MODULO(i, TERRAIN_LENGTH);
		render_quads (dpy, d, t, dt, i);
	}

	dt=1;
	if (be_wormy) {
		for (; t < TERRAIN_LENGTH-(1+(pindex<INTERP/2)); t+=dt) {
			render_bonus_block (dpy, d, t, i);
			i -= dt; MODULO(i, TERRAIN_LENGTH);
		}
	} else {
		if (wireframe) {assert (t == 3*TERRAIN_LENGTH/4);}

		if (t == 3*TERRAIN_LENGTH/4)
			render_pentagons (dpy, d, t, dt, i);

		for (; t < TERRAIN_LENGTH-(1+(pindex<INTERP/2)); t+=dt) {
			render_bonus_block (dpy, d, t, i);
			i -= dt; MODULO(i, TERRAIN_LENGTH);
			render_quads (dpy, d, t, dt, i);
		}
	}

	/* Draw crosshair */
	if (crosshair_flag) {
		gc = (wireframe ? bonus_gcs[nr_bonus_colors/2] : erase_gc);
		XFillRectangle (dpy, d, gc,
						width/2+(xoffset)-8, height/2+(yoffset*2)-1, 16, 3);
		XFillRectangle (dpy, d, gc,
						width/2+(xoffset)-1, height/2+(yoffset*2)-8, 3, 16);
	}

}

/*
 * move (step)
 *
 * move to the position for the next frame, and modify the state variables
 * nearest, pindex, pos, speed
 */
static void
move (double step)
{
	double dpos;

	pos += step;
	dpos = SIGN3(pos) * floor(fabs(pos));

	pindex += SIGN3(effective_speed) + INTERP;
	while (pindex >= INTERP) {
		nearest --;
		pindex -= INTERP;
	}
	while (pindex < 0) {
		nearest ++;
		pindex += INTERP;
	}

    nearest += dpos; MODULO(nearest, TERRAIN_LENGTH);

	pos -= dpos;

	accel = thrust + ycurvature[nearest] * gravity;
	speed += accel;
	if (speed > maxspeed) speed = maxspeed;
	if (speed < -maxspeed) speed = -maxspeed;
}

/*
 * speedmine (dpy, window)
 *
 * do everything required for one frame of the demo
 */
static void
speedmine (Display *dpy, Window window)
{
	double elapsed, time_per_frame = 0.04;

	regenerate_terrain ();

	perspective ();

	render_speedmine (dpy, dbuf);
	XCopyArea (dpy, dbuf, window, draw_gc, 0, 0, width, height, 0, 0);

#if HAVE_GETTIMEOFDAY
	fps_end = get_time();
	nframes++;
	total_nframes++;

	if (fps_end > fps_start + 0.5) {
		elapsed = fps_end - fps_start;
		fps_start = get_time();

		time_per_frame = elapsed / nframes - delay*1e-6;
		fps = nframes / elapsed;
		if (DEBUG_FLAG) {
			printf ("%f s elapsed\t%3f s/frame\t%.1f FPS\n", elapsed,
					time_per_frame, fps);
		}
		step = effective_speed * elapsed;

		nframes = 0;
	}
#else
	time_per_frame = 0.04;
	step = effective_speed;
#endif

	move (step);

	decrement_bonuses (time_per_frame);

	check_bonuses ();
}

/*
 * speedmine_color_ramp (dpy, cmap, gcs, colors, ncolors, s1, s2, v1, v2)
 *
 * generate a color ramp of up to *ncolors between randomly chosen hues,
 * varying from saturation s1 to s2 and value v1 to v2, placing the colors
 * in 'colors' and creating corresponding GCs in 'gcs'.
 *
 * The number of colors actually allocated is returned in ncolors.
 */
static void
speedmine_color_ramp (Display * dpy, Colormap cmap, GC *gcs, XColor * colors,
					 int *ncolors, double s1, double s2, double v1, double v2)
{
	XGCValues gcv;
	int h1, h2;
	unsigned long flags;
	int i;

	assert (*ncolors >= 0);
	assert (s1 >= 0.0 && s1 <= 1.0 && v1 >= 0.0 && v2 <= 1.0);

	if (psychedelic_flag) {
		h1 = RAND(360); h2 = (h1 + 180) % 360;
	} else {
		h1 = h2 = RAND(360);
	}

	make_color_ramp (dpy, cmap, h1, s1, v1, h2, s2, v2,
				     colors, ncolors, False, True, False);

	flags = GCForeground;
	for (i=0; i < *ncolors; i++) {
		gcv.foreground = colors[i].pixel;
		gcs[i] = XCreateGC (dpy, dbuf, flags, &gcv);
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
change_colors (void)
{
	double s1, s2;

	if (psychedelic_flag) {
		free_colors (display, cmap, bonus_colors, nr_bonus_colors);
		free_colors (display, cmap, wall_colors, nr_wall_colors);
		free_colors (display, cmap, ground_colors, nr_ground_colors);
		ncolors = MAX_COLORS;

		s1 = 0.4; s2 = 0.9;

  		speedmine_color_ramp (display, cmap, ground_gcs, ground_colors,
							  &ncolors, 0.0, 0.8, 0.0, 0.9);
  		nr_ground_colors = ncolors;
	} else {
		free_colors (display, cmap, bonus_colors, nr_bonus_colors);
		free_colors (display, cmap, wall_colors, nr_wall_colors);
		ncolors = nr_ground_colors;

		s1 = 0.0; s2 = 0.6;
	}

    speedmine_color_ramp (display, cmap, wall_gcs, wall_colors, &ncolors,
				 		  s1, s2, 0.0, 0.9);
    nr_wall_colors = ncolors;

    speedmine_color_ramp (display, cmap, bonus_gcs, bonus_colors, &ncolors,
				  		  0.6, 0.9, 0.4, 1.0);
	nr_bonus_colors = ncolors;
}

/*
 * init_psychedelic_colors (dpy, window, cmap)
 *
 * initialise a psychedelic colormap
 */
static void
init_psychedelic_colors (Display * dpy, Window window, Colormap cmap)
{
  XGCValues gcv;

  gcv.foreground = get_pixel_resource ("tunnelend", "TunnelEnd", dpy, cmap);
  tunnelend_gc = XCreateGC (dpy, window, GCForeground, &gcv);

  ncolors = MAX_COLORS;

  speedmine_color_ramp (dpy, cmap, ground_gcs, ground_colors, &ncolors,
				  		0.0, 0.8, 0.0, 0.9);
  nr_ground_colors = ncolors;

  speedmine_color_ramp (dpy, cmap, wall_gcs, wall_colors, &ncolors,
				 		0.0, 0.6, 0.0, 0.9);
  nr_wall_colors = ncolors;

  speedmine_color_ramp (dpy, cmap, bonus_gcs, bonus_colors, &ncolors,
				  		0.6, 0.9, 0.4, 1.0);
  nr_bonus_colors = ncolors;
}

/*
 * init_colors (dpy, window, cmap)
 *
 * initialise a normal colormap
 */
static void
init_colors (Display * dpy, Window window, Colormap cmap)
{
  XGCValues gcv;
  XColor dark, light;
  int h1, h2;
  double s1, s2, v1, v2;
  unsigned long flags;
  int i;

  gcv.foreground = get_pixel_resource ("tunnelend", "TunnelEnd", dpy, cmap);
  tunnelend_gc = XCreateGC (dpy, window, GCForeground, &gcv);

  ncolors = MAX_COLORS;

  dark.pixel = get_pixel_resource ("darkground", "DarkGround", dpy, cmap);
  XQueryColor (dpy, cmap, &dark);

  light.pixel = get_pixel_resource ("lightground", "LightGround", dpy, cmap);
  XQueryColor (dpy, cmap, &light);

  rgb_to_hsv (dark.red, dark.green, dark.blue, &h1, &s1, &v1);
  rgb_to_hsv (light.red, light.green, light.blue, &h2, &s2, &v2);
  make_color_ramp (dpy, cmap, h1, s1, v1, h2, s2, v2,
				  ground_colors, &ncolors, False, True, False);
  nr_ground_colors = ncolors;

  flags = GCForeground;
  for (i=0; i < ncolors; i++) {
	gcv.foreground = ground_colors[i].pixel;
	ground_gcs[i] = XCreateGC (dpy, dbuf, flags, &gcv);
  }

  speedmine_color_ramp (dpy, cmap, wall_gcs, wall_colors, &ncolors,
				 		0.0, 0.6, 0.0, 0.9);
  nr_wall_colors = ncolors;

  speedmine_color_ramp (dpy, cmap, bonus_gcs, bonus_colors, &ncolors,
				  		0.6, 0.9, 0.4, 1.0);
  nr_bonus_colors = ncolors;
}

/*
 * print_stats ()
 *
 * print out average FPS stats for the demo
 */
static void
print_stats (void)
{
	if (total_nframes >= 1)
		printf ("Rendered %d frames averaging %f FPS\n", total_nframes,
				total_nframes / get_time());
}

/*
 * init_speedmine (dpy, window)
 *
 * initialise the demo
 */
static void
init_speedmine (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  int i;
  double th;
  int wide;

  display = dpy;

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  width = xgwa.width;
  height = xgwa.height;

  verbose_flag = get_boolean_resource ("verbose", "Boolean");

  dbuf = XCreatePixmap (dpy, window, width, height, xgwa.depth);
  stars_mask = XCreatePixmap (dpy, window, width, height, 1);

  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  stars_gc = XCreateGC (dpy, stars_mask, GCForeground, &gcv);

  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, dbuf, GCForeground, &gcv);
  stars_erase_gc = XCreateGC (dpy, stars_mask, GCForeground, &gcv);

  wire_flag = get_boolean_resource ("wire", "Boolean");

  psychedelic_flag = get_boolean_resource ("psychedelic", "Boolean");

  delay = get_integer_resource("delay", "Integer");

  smoothness = get_integer_resource("smoothness", "Integer");
  if (smoothness < 1) smoothness = 1;

  maxspeed = get_float_resource("maxspeed", "Float");
  maxspeed *= 0.01;
  maxspeed = fabs(maxspeed);

  thrust = get_float_resource("thrust", "Float");
  thrust *= 0.2;

  gravity = get_float_resource("gravity", "Float");
  gravity *= 0.002/9.8;

  vertigo = get_float_resource("vertigo", "Float");
  vertigo *= 0.2;

  curviness = get_float_resource("curviness", "Float");
  curviness *= 0.25;

  twistiness = get_float_resource("twistiness", "Float");
  twistiness *= 0.125;

  terrain_flag = get_boolean_resource ("terrain", "Boolean");
  widening_flag = get_boolean_resource ("widening", "Boolean");
  bumps_flag = get_boolean_resource ("bumps", "Boolean");
  bonuses_flag = get_boolean_resource ("bonuses", "Boolean");
  crosshair_flag = get_boolean_resource ("crosshair", "Boolean");

  be_wormy = get_boolean_resource ("worm", "Boolean");
  if (be_wormy) {
      maxspeed   *= 1.43;
      thrust     *= 10;
      gravity    *= 3;
      vertigo    *= 0.5;
      smoothness *= 2;
      curviness  *= 2;
      twistiness *= 2;
      psychedelic_flag = True;
      crosshair_flag = False;
  }

  if (psychedelic_flag) init_psychedelic_colors (dpy, window, cmap);
  else init_colors (dpy, window, cmap);

  for (i=0; i<ROTS; i++) {
	th = M_PI * 2.0 * i / ROTS;
	costab[i] = cos(th);
	sintab[i] = sin(th);
  }

  wide = random_wideness();

  for (i=0; i < TERRAIN_LENGTH; i++) {
	wideness[i] = wide;
	bonuses[i] = 0;
  }

  init_terrain ();
  init_curves ();
  wrap_tunnel (0, TERRAIN_LENGTH-1);

  if (DEBUG_FLAG || verbose_flag) atexit(print_stats);

  step = effective_speed;

#ifdef HAVE_GETTIMEOFDAY
  init_time ();
#endif

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

char *progclass = "Speedmine";

char *defaults [] = {
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

XrmOptionDescRec options [] = {
  { "-verbose",			".verbose",				XrmoptionNoArg, "True"},
  { "-worm",			".worm",				XrmoptionNoArg, "True"},
  { "-wire",			".wire",				XrmoptionNoArg, "True"},
  { "-nowire",			".wire",				XrmoptionNoArg, "False"},
  { "-darkground",		".darkground",			XrmoptionSepArg, 0 },
  { "-lightground",		".lightground",			XrmoptionSepArg, 0 },
  { "-tunnelend",		".tunnelend",			XrmoptionSepArg, 0 },
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { "-maxspeed",		".maxspeed",			XrmoptionSepArg, 0 },
  { "-thrust",			".thrust",				XrmoptionSepArg, 0 },
  { "-gravity",			".gravity",				XrmoptionSepArg, 0 },
  { "-vertigo",			".vertigo",				XrmoptionSepArg, 0 },
  { "-terrain",			".terrain",				XrmoptionNoArg, "True"},
  { "-noterrain",		".terrain",				XrmoptionNoArg, "False"},
  { "-smoothness",      ".smoothness",			XrmoptionSepArg, 0 },
  { "-curviness",		".curviness",			XrmoptionSepArg, 0 },
  { "-twistiness",		".twistiness",			XrmoptionSepArg, 0 },
  { "-widening",		".widening",			XrmoptionNoArg, "True"},
  { "-nowidening",		".widening",			XrmoptionNoArg, "False"},
  { "-bumps",			".bumps",				XrmoptionNoArg, "True"},
  { "-nobumps",			".bumps",				XrmoptionNoArg, "False"},
  { "-bonuses",			".bonuses",				XrmoptionNoArg, "True"},
  { "-nobonuses",		".bonuses",				XrmoptionNoArg, "False"},
  { "-crosshair",		".crosshair",			XrmoptionNoArg, "True"},
  { "-nocrosshair",		".crosshair",			XrmoptionNoArg, "False"},
  { "-psychedelic",		".psychedelic",			XrmoptionNoArg, "True"},
  { "-nopsychedelic",	".psychedelic",			XrmoptionNoArg, "False"},
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
#ifndef NDEBUG
	atexit (abort);
#endif

	init_speedmine (dpy, window);

	while (1) {
		speedmine (dpy, window);
		XSync (dpy, False);
		screenhack_handle_events (dpy);
		if (delay) usleep(delay);
	}
}

/* vim: ts=4
 */
