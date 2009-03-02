/* -*- Mode: C; tab-width: 4 -*- */
/* world --- world spinner */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)world.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1993 Matthew Moyle-Croft <matthew@moyle-croft.com>
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
 * 01-Nov-2000:
 * 10-May-1997: Compatible with xscreensaver
 * 04-Oct-1995: multiscreen patch, thanks to Grant McDorman <grant@isgtec.com>.
 * 10-Jul-1995: Backward spinning jump fixed by Neale Pickett <zephyr@nmt.edu>.
 * 17-Jul-1994: Got batchcount to work.
 * 09-Jan-1994: Written [ Modified from image.c ]
 * 29-Jul-1990: image.c written. Copyright (c) 1991 by Patrick J. Naughton.
 */

#ifdef STANDALONE
#define MODE_world
#define PROGCLASS "World"
#define HACK_INIT init_world
#define HACK_DRAW draw_world
#define world_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: -16 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_world

ModeSpecOpt world_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   world_description =
{"world", "init_world", "draw_world", "release_world",
 "refresh_world", "init_world", (char *) NULL, &world_opts,
 100000, -16, 1, 1, 64, 0.3, "",
 "Shows spinning Earths", 0, NULL};

#endif

#include "bitmaps/terra-00.xbm"
#include "bitmaps/terra-01.xbm"
#include "bitmaps/terra-02.xbm"
#include "bitmaps/terra-03.xbm"
#include "bitmaps/terra-04.xbm"
#include "bitmaps/terra-05.xbm"
#include "bitmaps/terra-06.xbm"
#include "bitmaps/terra-07.xbm"
#include "bitmaps/terra-08.xbm"
#include "bitmaps/terra-09.xbm"
#include "bitmaps/terra-10.xbm"
#include "bitmaps/terra-11.xbm"
#include "bitmaps/terra-12.xbm"
#include "bitmaps/terra-13.xbm"
#include "bitmaps/terra-14.xbm"
#include "bitmaps/terra-15.xbm"
#include "bitmaps/terra-16.xbm"
#include "bitmaps/terra-17.xbm"
#include "bitmaps/terra-18.xbm"
#include "bitmaps/terra-19.xbm"
#include "bitmaps/terra-20.xbm"
#include "bitmaps/terra-21.xbm"
#include "bitmaps/terra-22.xbm"
#include "bitmaps/terra-23.xbm"
#include "bitmaps/terra-24.xbm"
#include "bitmaps/terra-25.xbm"
#include "bitmaps/terra-26.xbm"
#include "bitmaps/terra-27.xbm"
#include "bitmaps/terra-28.xbm"
#include "bitmaps/terra-29.xbm"

#define NUM_EARTHS 30
#define SIZE_X terra00_width	/* 64 */
#define SIZE_Y terra00_height	/* 64 */
#define NUM_REV 4
#define MINWORLDS 1

static XImage Earths[NUM_EARTHS] =
{
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra00_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra01_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra02_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra03_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra04_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra05_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra06_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra07_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra08_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra09_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra10_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra11_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra12_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra13_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra14_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra15_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra16_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra17_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra18_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra19_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra20_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra21_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra22_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra23_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra24_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra25_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra26_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra27_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra28_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{SIZE_X, SIZE_Y, 0, XYBitmap, (char *) terra29_bits, LSBFirst, 8, LSBFirst, 8, 1}};

typedef struct {
	int         x;
	int         y;
	unsigned long color;
	int         frame;
	int         direction;
} planetstruct;

typedef struct {
	int         width;
	int         height;
	int         nrows;
	int         ncols;
	int         xb;
	int         yb;
	int         frame_num;
	int         nplanets;
	planetstruct *planets;
} worldstruct;

static worldstruct *worlds = (worldstruct *) NULL;

void
init_world(ModeInfo * mi)
{
	int         i;
	worldstruct *wp;

	if (worlds == NULL) {
		if ((worlds = (worldstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (worldstruct))) == NULL)
			return;
	}
	wp = &worlds[MI_SCREEN(mi)];
	wp->frame_num = NUM_EARTHS * NUM_REV;
	for (i = 0; i < NUM_EARTHS; i++)
		Earths[i].bytes_per_line = 8;
	wp->width = MI_WIDTH(mi);
	wp->height = MI_HEIGHT(mi);
	wp->ncols = wp->width / SIZE_X;
	if (!wp->ncols)
		wp->ncols = 1;
	wp->nrows = wp->height / SIZE_Y;
	if (!wp->nrows)
		wp->nrows = 1;
	wp->xb = (wp->width - SIZE_X * wp->ncols) / 2;
	wp->yb = (wp->height - SIZE_Y * wp->nrows) / 2;
	wp->nplanets = MI_COUNT(mi);
	if (wp->nplanets < -MINWORLDS)
		wp->nplanets = NRAND(-wp->nplanets - MINWORLDS + 1) + MINWORLDS;
	else if (wp->nplanets < MINWORLDS)
		wp->nplanets = MINWORLDS;

	if (wp->nplanets > wp->ncols * wp->nrows)
		wp->nplanets = wp->ncols * wp->nrows;
#ifndef NOFLASH
	if (wp->nplanets > wp->ncols)
		wp->nplanets = wp->ncols;
#endif
	if (wp->planets != NULL)
		free(wp->planets);
	if ((wp->planets = (planetstruct *) malloc(wp->nplanets *
			sizeof (planetstruct))) == NULL) {
		return;
	}
	for (i = 0; i < wp->nplanets; i++)
		wp->planets[i].x = wp->planets[i].y = -1;

	MI_CLEARWINDOW(mi);
}

void
draw_world(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	int         i;
#ifndef NOFLASH
	int        *col, j;
#endif
	worldstruct *wp;

	if (worlds == NULL)
		return;
	wp = &worlds[MI_SCREEN(mi)];
	if (wp->planets == NULL)
		return;

#ifndef NOFLASH
	if ((col = (int *) calloc(wp->ncols, sizeof (int))) == NULL)
		            return;
#endif

	MI_IS_DRAWN(mi) = True;

	if (wp->frame_num == NUM_EARTHS * NUM_REV) {
		wp->frame_num = 0;
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		for (i = 0; i < wp->nplanets; i++) {
			XFillRectangle(display, MI_WINDOW(mi), gc,
				 wp->xb + SIZE_X * wp->planets[i].x,
				 wp->yb + SIZE_Y * wp->planets[i].y,
				 SIZE_X, SIZE_Y);
#ifdef NOFLASH
			wp->planets[i].x = NRAND(wp->ncols);
#else
			do {
				j = NRAND(wp->ncols);
				if (!col[j])
					wp->planets[i].x = j;
				col[j]++;
			} while (col[j] > 1);
#endif
			wp->planets[i].y = NRAND(wp->nrows);
			wp->planets[i].direction = (int) (LRAND() & 1);
			wp->planets[i].frame = NRAND(NUM_EARTHS);
			if (MI_NPIXELS(mi) > 2)
				wp->planets[i].color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				wp->planets[i].color = MI_WHITE_PIXEL(mi);
		}
	}
#ifndef NOFLASH
	free(col);
#endif
	for (i = 0; i < wp->nplanets; i++) {
		XSetForeground(display, gc, wp->planets[i].color);
		if (wp->planets[i].frame == NUM_EARTHS)
			wp->planets[i].frame = 0;
		else {
			if ((wp->planets[i].frame < 0) && wp->planets[i].direction == 0)
				wp->planets[i].frame = NUM_EARTHS - 1;
		}
		(void) XPutImage(display, MI_WINDOW(mi), gc,
				 (Earths + wp->planets[i].frame), 0, 0,
				 wp->xb + SIZE_X * wp->planets[i].x,
				 wp->yb + SIZE_Y * wp->planets[i].y,
				 SIZE_X, SIZE_Y);

		(wp->planets[i].direction) ? wp->planets[i].frame++ : wp->planets[i].frame--;
	}
	wp->frame_num++;
}

void
release_world(ModeInfo * mi)
{
	if (worlds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			if (worlds[screen].planets != NULL)
				free(worlds[screen].planets);
		free(worlds);
		worlds = (worldstruct *) NULL;
	}
}

void
refresh_world(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_world */
