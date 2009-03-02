/* -*- Mode: C; tab-width: 4 -*- */
/* lightning --- fractal lightning bolds */

#if 0
static const char sccsid[] = "@(#)lightning.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1996 by Keith Romberg <kromberg@saxe.com>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 * 14-Jul-1996: Cleaned up code.
 * 27-Jun-1996: Written and submitted by Keith Romberg <kromberg@saxe.com>.
 */

#ifdef STANDALONE
# define MODE_lightning
# define DEFAULTS "*delay: 10000 \n" \
                  "*ncolors: 64  \n"

# define BRIGHT_COLORS
# define reshape_lightning 0
# define lightning_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_lightning

ENTRYPOINT ModeSpecOpt lightning_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   lightning_description =
{"lightning", "init_lightning", "draw_lightning", "release_lightning",
 "refresh_lightning", "init_lightning", (char *) NULL, &lightning_opts,
 10000, 1, 1, 1, 64, 0.6, "",
 "Shows Keith's fractal lightning bolts", 0, NULL};

#endif

#define BOLT_NUMBER 4
#define BOLT_ITERATION 4
#define LONG_FORK_ITERATION 3
#define MEDIUM_FORK_ITERATION 2
#define SMALL_FORK_ITERATION 1

#define WIDTH_VARIATION 30
#define HEIGHT_VARIATION 15

#define DELAY_TIME_AMOUNT 15
#define MULTI_DELAY_TIME_BASE 5

#define MAX_WIGGLES 16
#define WIGGLE_BASE 8
#define WIGGLE_AMOUNT 14

#define RANDOM_FORK_PROBILITY   4

#define FIRST_LEVEL_STRIKE 0
#define LEVEL_ONE_STRIKE 1
#define LEVEL_TWO_STRIKE 2

#define BOLT_VERTICIES ((1<<BOLT_ITERATION)-1)
  /* BOLT_ITERATION = 4. 2^(BOLT_ITERATION) - 1 = 15 */

#define NUMBER_FORK_VERTICIES 9

#define FLASH_PROBILITY 20
#define MAX_FLASH_AMOUNT 2	/*  half the total duration of the bolt  */

typedef struct {
	XPoint      ForkVerticies[NUMBER_FORK_VERTICIES];
	int         num_used;
} Fork;

typedef struct {
	XPoint      end1, end2;
	XPoint      middle[BOLT_VERTICIES];
	int         fork_number;
	int         forks_start[2];
	Fork        branch[2];
	int         wiggle_number;
	int         wiggle_amount;
	int         delay_time;
	int         flash;
	int         flash_begin, flash_stop;
	int         visible;
	int         strike_level;
} Lightning;

typedef struct {
	Lightning   bolts[BOLT_NUMBER];
	int         scr_width, scr_height;
	int         multi_strike;
	int         give_it_hell;
	int         draw_time;
	int         stage;
	int         busyLoop;
	unsigned long color;
} Storm;

static Storm *Helga = (Storm *) NULL;

/*-------------------   function prototypes  ----------------------------*/

static int  distance(XPoint a, XPoint b);

static int  setup_multi_strike(void);
static int  flashing_strike(void);
static void flash_duration(int *start, int *end, int total_duration);
static void random_storm(Storm * st);
static void generate(XPoint A, XPoint B, int iter, XPoint * verts, int *vert_index);
static void create_fork(Fork * f, XPoint start, XPoint end, int level);

static void first_strike(Lightning bolt, ModeInfo * mi);
static void draw_bolt(Lightning * bolt, ModeInfo * mi);
static void draw_line(ModeInfo * mi, XPoint * p, int number, GC use, int x_offset);
static void level1_strike(Lightning bolt, ModeInfo * mi);
static void level2_strike(Lightning bolt, ModeInfo * mi);

static int  storm_active(Storm * st);
static void update_bolt(Lightning * bolt, int time_now);
static void wiggle_bolt(Lightning * bolt);
static void wiggle_line(XPoint * p, int number, int wiggle_amount);

/*-------------------------  functions  ---------------------------------*/

static int
setup_multi_strike(void)
{
	int         result, multi_prob;

	multi_prob = NRAND(100);

	if (multi_prob < 50)
		result = 1;
	else if ((multi_prob >= 51) && (multi_prob < 75))
		result = 2;
	else if ((multi_prob >= 76) && (multi_prob < 92))
		result = 3;
	else
		result = BOLT_NUMBER;	/* 4 */

	return (result);
}

/*-------------------------------------------------------------------------*/

static int
flashing_strike(void)
{
	int         tmp = NRAND(FLASH_PROBILITY);

	if (tmp <= FLASH_PROBILITY)
		return (1);
	return (0);
}

/*-------------------------------------------------------------------------*/

static void
flash_duration(int *start, int *end, int total_duration)
{
	int         mid, d;

	mid = total_duration / MAX_FLASH_AMOUNT;
	d = NRAND(total_duration / MAX_FLASH_AMOUNT) / 2;
	*start = mid - d;
	*end = mid + d;
}

/*-------------------------------------------------------------------------*/

static void
random_storm(Storm * st)
{
	int         i, j, tmp;
	XPoint      p;

	for (i = 0; i < st->multi_strike; i++) {
		st->bolts[i].end1.x = NRAND(st->scr_width);
		st->bolts[i].end1.y = 0;
		st->bolts[i].end2.x = NRAND(st->scr_width);
		st->bolts[i].end2.y = st->scr_height;
		st->bolts[i].wiggle_number = WIGGLE_BASE + NRAND(MAX_WIGGLES);
		if ((st->bolts[i].flash = flashing_strike()))
			flash_duration(&(st->bolts[i].flash_begin), &(st->bolts[i].flash_stop),
				       st->bolts[i].wiggle_number);
		else
			st->bolts[i].flash_begin = st->bolts[i].flash_stop = 0;
		st->bolts[i].wiggle_amount = WIGGLE_AMOUNT;
		if (i == 0)
			st->bolts[i].delay_time = NRAND(DELAY_TIME_AMOUNT);
		else
			st->bolts[i].delay_time = NRAND(DELAY_TIME_AMOUNT) +
				(MULTI_DELAY_TIME_BASE * i);
		st->bolts[i].strike_level = FIRST_LEVEL_STRIKE;
		tmp = 0;
		generate(st->bolts[i].end1, st->bolts[i].end2, BOLT_ITERATION,
			 st->bolts[i].middle, &tmp);
		st->bolts[i].fork_number = 0;
		st->bolts[i].visible = 0;
		for (j = 0; j < BOLT_VERTICIES; j++) {
			if (st->bolts[i].fork_number >= 2)
				break;
			if (NRAND(100) < RANDOM_FORK_PROBILITY) {
				p.x = NRAND(st->scr_width);
				p.y = st->scr_height;
				st->bolts[i].forks_start[st->bolts[i].fork_number] = j;
				create_fork(&(st->bolts[i].branch[st->bolts[i].fork_number]),
					    st->bolts[i].middle[j], p, j);
				st->bolts[i].fork_number++;
			}
		}
	}
}

static void
generate(XPoint A, XPoint B, int iter, XPoint * verts, int *vert_index)
{
	XPoint      mid;

	mid.x = (A.x + B.x) / 2 + NRAND(WIDTH_VARIATION) - WIDTH_VARIATION / 2;
	mid.y = (A.y + B.y) / 2 + NRAND(HEIGHT_VARIATION) - HEIGHT_VARIATION / 2;

	if (!iter) {
		verts[*vert_index].x = mid.x;
		verts[*vert_index].y = mid.y;
		(*vert_index)++;
		return;
	}
	generate(A, mid, iter - 1, verts, vert_index);
	generate(mid, B, iter - 1, verts, vert_index);
}

/*------------------------------------------------------------------------*/

static void
create_fork(Fork * f, XPoint start, XPoint end, int level)
{
	int         tmp = 1;

	f->ForkVerticies[0].x = start.x;
	f->ForkVerticies[0].y = start.y;

	if (level <= 6) {
		generate(start, end, LONG_FORK_ITERATION, f->ForkVerticies, &tmp);
		f->num_used = 9;
	} else if ((level > 6) && (level <= 11)) {
		generate(start, end, MEDIUM_FORK_ITERATION, f->ForkVerticies, &tmp);
		f->num_used = 5;
	} else {
		if (distance(start, end) > 100) {
			generate(start, end, MEDIUM_FORK_ITERATION, f->ForkVerticies, &tmp);
			f->num_used = 5;
		} else {
			generate(start, end, SMALL_FORK_ITERATION, f->ForkVerticies, &tmp);
			f->num_used = 3;
		}
	}

	f->ForkVerticies[f->num_used - 1].x = end.x;
	f->ForkVerticies[f->num_used - 1].y = end.y;
}

/*------------------------------------------------------------------------*/

static void
update_bolt(Lightning * bolt, int time_now)
{
	wiggle_bolt(bolt);
	if ((bolt->wiggle_amount == 0) && (bolt->wiggle_number > 2))
		bolt->wiggle_number = 0;
	if (((time_now % 3) == 0))
		bolt->wiggle_amount++;

	if (((time_now >= bolt->delay_time) && (time_now < bolt->flash_begin)) ||
	    (time_now > bolt->flash_stop))
		bolt->visible = 1;
	else
		bolt->visible = 0;

	if (time_now == bolt->delay_time)
		bolt->strike_level = FIRST_LEVEL_STRIKE;
	else if (time_now == (bolt->delay_time + 1))
		bolt->strike_level = LEVEL_ONE_STRIKE;
	else if ((time_now > (bolt->delay_time + 1)) &&
		 (time_now <= (bolt->delay_time + bolt->flash_begin - 2)))
		bolt->strike_level = LEVEL_TWO_STRIKE;
	else if (time_now == (bolt->delay_time + bolt->flash_begin - 1))
		bolt->strike_level = LEVEL_ONE_STRIKE;
	else if (time_now == (bolt->delay_time + bolt->flash_stop + 1))
		bolt->strike_level = LEVEL_ONE_STRIKE;
	else
		bolt->strike_level = LEVEL_TWO_STRIKE;
}

/*------------------------------------------------------------------------*/

static void
draw_bolt(Lightning * bolt, ModeInfo * mi)
{
	if (bolt->visible) {
		if (bolt->strike_level == FIRST_LEVEL_STRIKE)
			first_strike(*bolt, mi);
		else if (bolt->strike_level == LEVEL_ONE_STRIKE)
			level1_strike(*bolt, mi);
		else
			level2_strike(*bolt, mi);
	}
}

/*------------------------------------------------------------------------*/

static void
first_strike(Lightning bolt, ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         i;

	XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XDrawLine(display, window, gc,
	       bolt.end1.x, bolt.end1.y, bolt.middle[0].x, bolt.middle[0].y);
	draw_line(mi, bolt.middle, BOLT_VERTICIES, gc, 0);
	XDrawLine(display, window, gc,
	bolt.middle[BOLT_VERTICIES - 1].x, bolt.middle[BOLT_VERTICIES - 1].y,
		  bolt.end2.x, bolt.end2.y);

	for (i = 0; i < bolt.fork_number; i++)
		draw_line(mi, bolt.branch[i].ForkVerticies, bolt.branch[i].num_used,
			  gc, 0);
}

/*------------------------------------------------------------------------*/

static void
draw_line(ModeInfo * mi, XPoint * points, int number, GC to_use, int offset)
{
	int         i;

	for (i = 0; i < number - 1; i++) {
		if (points[i].y <= points[i + 1].y)
			XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi), to_use, points[i].x + offset,
				  points[i].y, points[i + 1].x + offset, points[i + 1].y);
		else {
			if (points[i].x < points[i + 1].x)
				XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi), to_use, points[i].x +
					  offset, points[i].y + offset, points[i + 1].x + offset,
					  points[i + 1].y + offset);
			else
				XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi), to_use, points[i].x -
					  offset, points[i].y + offset, points[i + 1].x - offset,
					  points[i + 1].y + offset);
		}
	}
}

/*------------------------------------------------------------------------*/

static void
level1_strike(Lightning bolt, ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	Storm      *st = &Helga[MI_SCREEN(mi)];
	GC          gc = MI_GC(mi);
	int         i;

	if (MI_NPIXELS(mi) > 2)	/* color */
		XSetForeground(display, gc, MI_PIXEL(mi, st->color));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XDrawLine(display, window, gc,
	bolt.end1.x - 1, bolt.end1.y, bolt.middle[0].x - 1, bolt.middle[0].y);
	draw_line(mi, bolt.middle, BOLT_VERTICIES, gc, -1);
	XDrawLine(display, window, gc,
		  bolt.middle[BOLT_VERTICIES - 1].x - 1,
	    bolt.middle[BOLT_VERTICIES - 1].y, bolt.end2.x - 1, bolt.end2.y);
	XDrawLine(display, window, gc,
	bolt.end1.x + 1, bolt.end1.y, bolt.middle[0].x + 1, bolt.middle[0].y);
	draw_line(mi, bolt.middle, BOLT_VERTICIES, gc, 1);
	XDrawLine(display, window, gc,
		  bolt.middle[BOLT_VERTICIES - 1].x + 1,
	    bolt.middle[BOLT_VERTICIES - 1].y, bolt.end2.x + 1, bolt.end2.y);

	for (i = 0; i < bolt.fork_number; i++) {
		draw_line(mi, bolt.branch[i].ForkVerticies, bolt.branch[i].num_used,
			  gc, -1);
		draw_line(mi, bolt.branch[i].ForkVerticies, bolt.branch[i].num_used,
			  gc, 1);
	}
	first_strike(bolt, mi);
}

/*------------------------------------------------------------------------*/

static int
distance(XPoint a, XPoint b)
{
	return ((int) sqrt((double) (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)));
}

/*------------------------------------------------------------------------*/

static void
level2_strike(Lightning bolt, ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	Storm      *st = &Helga[MI_SCREEN(mi)];
	GC          gc = MI_GC(mi);
	int         i;

	/* This was originally designed to be a little darker then the
	   level1 strike.  This was changed to get it to work on
	   multiscreens and to add more color variety.   I tried
	   stippling but it did not look good. */
	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, st->color));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XDrawLine(display, window, gc,
	bolt.end1.x - 2, bolt.end1.y, bolt.middle[0].x - 2, bolt.middle[0].y);
	draw_line(mi, bolt.middle, BOLT_VERTICIES, gc, -2);
	XDrawLine(display, window, gc,
		  bolt.middle[BOLT_VERTICIES - 1].x - 2,
	    bolt.middle[BOLT_VERTICIES - 1].y, bolt.end2.x - 2, bolt.end2.y);

	XDrawLine(display, window, gc,
	bolt.end1.x + 2, bolt.end1.y, bolt.middle[0].x + 2, bolt.middle[0].y);
	draw_line(mi, bolt.middle, BOLT_VERTICIES, gc, 2);
	XDrawLine(display, window, gc,
		  bolt.middle[BOLT_VERTICIES - 1].x + 2,
	    bolt.middle[BOLT_VERTICIES - 1].y, bolt.end2.x + 2, bolt.end2.y);

	for (i = 0; i < bolt.fork_number; i++) {
		draw_line(mi, bolt.branch[i].ForkVerticies, bolt.branch[i].num_used,
			  gc, -2);
		draw_line(mi, bolt.branch[i].ForkVerticies, bolt.branch[i].num_used,
			  gc, 2);
	}
	level1_strike(bolt, mi);
}

/*------------------------------------------------------------------------*/

static int
storm_active(Storm * st)
{
	int         i, atleast_1 = 0;

	for (i = 0; i < st->multi_strike; i++)
		if (st->bolts[i].wiggle_number > 0)
			atleast_1++;

	return (atleast_1);
}

/*------------------------------------------------------------------------*/

static void
wiggle_bolt(Lightning * bolt)
{
	int         i;

	wiggle_line(bolt->middle, BOLT_VERTICIES, bolt->wiggle_amount);
	bolt->end2.x += NRAND(bolt->wiggle_amount) - bolt->wiggle_amount / 2;
	bolt->end2.y += NRAND(bolt->wiggle_amount) - bolt->wiggle_amount / 2;

	for (i = 0; i < bolt->fork_number; i++) {
		wiggle_line(bolt->branch[i].ForkVerticies, bolt->branch[i].num_used,
			    bolt->wiggle_amount);
		bolt->branch[i].ForkVerticies[0].x = bolt->middle[bolt->forks_start[i]].x;
		bolt->branch[i].ForkVerticies[0].y = bolt->middle[bolt->forks_start[i]].y;
	}

	if (bolt->wiggle_amount > 1)
		bolt->wiggle_amount -= 1;
	else
		bolt->wiggle_amount = 0;
}

/*------------------------------------------------------------------------*/

static void
wiggle_line(XPoint * p, int number, int amount)
{
	int         i;

	for (i = 0; i < number; i++) {
		p[i].x += NRAND(amount) - amount / 2;
		p[i].y += NRAND(amount) - amount / 2;
	}
}

/*------------------------------------------------------------------------*/

ENTRYPOINT void
init_lightning (ModeInfo * mi)
{
	Storm      *st;

	if (Helga == NULL) {
		if ((Helga = (Storm *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (Storm))) == NULL)
			return;
	}
	st = &Helga[MI_SCREEN(mi)];

	st->scr_width = MI_WIDTH(mi);
	st->scr_height = MI_HEIGHT(mi);

	st->multi_strike = setup_multi_strike();
	random_storm(st);
	st->stage = 0;
}

/*------------------------------------------------------------------------*/

ENTRYPOINT void
draw_lightning (ModeInfo * mi)
{
	int         i;
	Storm      *st;

	if (Helga == NULL)
		return;
	st = &Helga[MI_SCREEN(mi)];
	MI_IS_DRAWN(mi) = True;
	switch (st->stage) {
		case 0:
			MI_IS_DRAWN(mi) = False;
			MI_CLEARWINDOW(mi);
			MI_IS_DRAWN(mi) = True;

			st->color = NRAND(MI_NPIXELS(mi));
			st->draw_time = 0;
			if (storm_active(st))
				st->stage++;
			else
				st->stage = 4;
			break;
		case 1:
			for (i = 0; i < st->multi_strike; i++) {
				if (st->bolts[i].visible)
					draw_bolt(&(st->bolts[i]), mi);
				update_bolt(&(st->bolts[i]), st->draw_time);
			}
			st->draw_time++;
			st->stage++;
			st->busyLoop = 0;
			break;
		case 2:
			if (++st->busyLoop > 6) {
				st->stage++;
				st->busyLoop = 0;
			}
			break;
		case 3:
			MI_IS_DRAWN(mi) = False;
			MI_CLEARWINDOW(mi);
			MI_IS_DRAWN(mi) = True;

			if (storm_active(st))
				st->stage = 1;
			else
				st->stage++;
			break;
		case 4:
			if (++st->busyLoop > 100) {
				st->busyLoop = 0;
			}
			init_lightning(mi);
			break;
	}
}

ENTRYPOINT void
release_lightning(ModeInfo * mi)
{
	if (Helga != NULL) {
		(void) free((void *) Helga);
		Helga = (Storm *) NULL;
	}
}

ENTRYPOINT void
refresh_lightning(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}

XSCREENSAVER_MODULE ("Lightning", lightning)


#endif /* MODE_lightning */
