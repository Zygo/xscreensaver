/* -*- Mode: C; tab-width: 4 -*- */
/* puzzle --- the familiar Sam Loyd puzzle */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)puzzle.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1995 by Heath Rice <rice@asl.dl.nec.com>.
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
 * 20-Mar-1998: Ideas from slip.c and eyes.c, problems with XGetImage when
 *              image moved off screen (-inwindow or -debug).
 * 10-May-1997: Compatible with xscreensaver
 * 15-Mar-1996: cleaned up, NUMBERED compile-time switch is now broken.
 * Feb-1996: combined with rastering.  Jouk Jansen <joukj@hrem.stm.tudelft.nl>.
 * Feb-1995: written.  Heath Rice <hrice@netcom.com>
 */

/*-
 * Chops up the screen into squares and randomly slides them around
 * like that game where you try to rearrange the little tiles in their
 * original order.  After it scrambles the tiles for awhile, it reverses
 * itself and puts them back like they started.  This mode looks the coolest
 * if you have a picture on your background.
 */

#ifdef STANDALONE
#define PROGCLASS "Puzzle"
#define HACK_INIT init_puzzle
#define HACK_DRAW draw_puzzle
#define puzzle_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: 250 \n" \
 "*ncolors: 64 \n" \
 "*bitmap: \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_puzzle

ModeSpecOpt puzzle_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   puzzle_description =
{"puzzle", "init_puzzle", "draw_puzzle", "release_puzzle",
 "init_puzzle", "init_puzzle", NULL, &puzzle_opts,
 10000, 250, 1, 1, 64, 1.0, "",
 "Shows a puzzle being scrambled and then solved", 0, NULL};

#endif

#define PUZZLE_WIDTH   image_width
#define PUZZLE_HEIGHT    image_height
#define PUZZLE_BITS    image_bits
#include "puzzle.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define PUZZLE_NAME    image_name
#include "puzzle.xpm"
#define DEFAULT_XPM 1
#endif

#define NOWAY 255

  /*int storedmoves, *moves, *position, space;  To keep track of puzzle */
typedef struct {
	Bool        painted;	/* For debouncing */
	int         excount;
	int        *fixbuff;
	XPoint      count, boxsize, windowsize;
	XPoint      usablewindow, offsetwindow;
	XPoint      randompos, randpos;
	unsigned long black;
	int         row, col, nextrow, nextcol, nextbox;
	int         incrementOfMove;
	int         lastbox;
	int         forward;
	int         prev;
	int         moves;

	/* Delta move stuff */
	int         movingBox;
	Pixmap      bufferBox;
	int         cfactor, rfactor;
	int         Lp;
	int         cbs, cbw, rbs, rbw;
	int         lengthOfMove;

#ifdef NUMBERED
	XImage     *image;
	GC          gc;
	/* Font stuff */
	int         done;
	int         ascent, fontWidth, fontHeight;
#else
	XImage     *logo;
	GC          backGC;
	Colormap    cmap;
	int         graphics_format;
#endif
} puzzlestruct;

static puzzlestruct *puzzles = NULL;

static void
free_stuff(Display * display, puzzlestruct * pp)
{
	if (pp->cmap != None) {
		XFreeColormap(display, pp->cmap);
		if (pp->backGC != None) {
			XFreeGC(display, pp->backGC);
			pp->backGC = None;
		}
		pp->cmap = None;
	} else
		pp->backGC = None;
}

static void
free_puzzle(Display * display, puzzlestruct * pp)
{
	if (pp->fixbuff != NULL) {
		(void) free((void *) pp->fixbuff);
		pp->fixbuff = NULL;
	}
	if (pp->bufferBox != None) {
		XFreePixmap(display, pp->bufferBox);
		pp->bufferBox = None;
	}
	free_stuff(display, pp);
	if (pp->logo) {
		destroyImage(&pp->logo, &pp->graphics_format);
		pp->logo = NULL;
	}
}

#ifdef NUMBERED
extern XFontStruct *getFont(Display * display);

#define font_height(f) (f->ascent + f->descent)
static XFontStruct *mode_font = None;

static int
font_width(XFontStruct * font, char ch)
{
	int         dummy;
	XCharStruct xcs;

	(void) XTextExtents(font, &ch, 1, &dummy, &dummy, &dummy, &xcs);
	return xcs.width;
}

static Bool
NumberScreen(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];

	if (mode_font == None)
		mode_font = getFont(display);
	if (!pp->done) {
		XGCValues   gcv;

		pp->done = 1;
		gcv.font = mode_font->fid;
		XSetFont(display, MI_GC(mi), mode_font->fid);
		gcv.graphics_exposures = False;
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((pp->gc = XCreateGC(display, window,
				GCForeground | GCBackground | GCGraphicsExposures | GCFont,
				&gcv)) == None) {
			free_puzzle(display, pp);
			return False;
		}
		pp->ascent = mode_font->ascent;
		pp->fontHeight = font_height(mode_font);
		pp->fontWidth = font_width(mode_font, '5');
	}
	XSetForeground(display, pp->gc, MI_WHITE_PIXEL(mi));

	{
		XPoint      pos, letter;
		int         count = 1, digitOffset = 1, temp, letterOffset;
		int         i, j, mult = pp->count.x * pp->count.y;
		char        buf[16];

		letter.x = pp->boxsize.x / 2 - 3;
		letter.y = pp->boxsize.y / 2 + pp->ascent / 2 - 1;
		letterOffset = pp->fontWidth / 2;
		pos.y = 0;
		for (j = 0; j < pp->count.y; j++) {
			pos.x = 0;
			for (i = 0; i < pp->count.x; i++) {
				if (count < mult) {
					if (pp->boxsize.x > 2 * pp->fontWidth &&
					    pp->boxsize.y > pp->fontHeight) {
						(void) sprintf(buf, "%d", count);
						(void) XDrawString(display, window, pp->gc,
								   pos.x + letter.x - letterOffset * digitOffset +
							     pp->randompos.x,
								   pos.y + letter.y + pp->randompos.y, buf, digitOffset);
					}
					XDrawRectangle(display, window, pp->gc,
						       pos.x + 1 + pp->randompos.x, pos.y + 1 + pp->randompos.y,
					pp->boxsize.x - 3, pp->boxsize.y - 3);
					count++;
					digitOffset = 0;
					temp = count;
					while (temp >= 1) {
						temp /= 10;
						digitOffset++;
					}
				}
				pos.x += pp->boxsize.x;
			}
			pos.y += pp->boxsize.y;
		}
	}
	return True;
}
#endif

static int
setupmove(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];

	if ((pp->prev == pp->excount) && (pp->excount > 0) && (pp->forward == 1)) {
		pp->lastbox = -1;
		pp->forward = 0;
		pp->prev--;
	} else if ((pp->prev == -1) && (pp->excount > 0) && (pp->forward == 0)) {
		pp->lastbox = -1;
		pp->forward = 1;
		pp->prev++;
	}
	if (pp->forward)
		pp->nextbox = NRAND(5);
	else
		pp->nextbox = pp->fixbuff[pp->prev];

	switch (pp->nextbox) {
		case 0:
			if ((pp->row == 0) || (pp->lastbox == 2))
				pp->nextbox = NOWAY;
			else {
				pp->nextrow = pp->row - 1;
				pp->nextcol = pp->col;
			}
			break;
		case 1:
			if ((pp->col == pp->count.x - 1) || (pp->lastbox == 3))
				pp->nextbox = NOWAY;
			else {
				pp->nextrow = pp->row;
				pp->nextcol = pp->col + 1;
			}
			break;
		case 2:
			if ((pp->row == pp->count.y - 1) || (pp->lastbox == 0))
				pp->nextbox = NOWAY;
			else {
				pp->nextrow = pp->row + 1;
				pp->nextcol = pp->col;
			}
			break;
		case 3:
			if ((pp->col == 0) || (pp->lastbox == 1))
				pp->nextbox = NOWAY;
			else {
				pp->nextrow = pp->row;
				pp->nextcol = pp->col - 1;
			}
			break;
		default:
			pp->nextbox = NOWAY;
			break;
	}

	if (pp->nextbox != NOWAY) {
		pp->lastbox = pp->nextbox;
		return True;
	} else
		return False;
}

static void
setupmovedelta(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];

	if (pp->bufferBox != None) {
		XFreePixmap(display, pp->bufferBox);
		pp->bufferBox = None;
	}
	if ((pp->bufferBox = XCreatePixmap(display, MI_WINDOW(mi),
				 pp->boxsize.x, pp->boxsize.y, MI_DEPTH(mi))) == None) {
		free_puzzle(display, pp);
		return;
	}
	XCopyArea(MI_DISPLAY(mi), MI_WINDOW(mi), pp->bufferBox, pp->backGC,
		  pp->nextcol * pp->boxsize.x + pp->randompos.x + 1,
		  pp->nextrow * pp->boxsize.y + pp->randompos.y + 1,
		  pp->boxsize.x - 2, pp->boxsize.y - 2, 0, 0);
	XFlush(MI_DISPLAY(mi));

	if (pp->nextcol > pp->col) {
		pp->cfactor = -1;
		pp->cbs = pp->boxsize.x;
		pp->cbw = pp->incrementOfMove;
	} else if (pp->col > pp->nextcol) {
		pp->cfactor = 1;
		pp->cbs = -pp->incrementOfMove;
		pp->cbw = pp->incrementOfMove;
	} else {
		pp->cfactor = 0;
		pp->cbs = 0;
		pp->cbw = pp->boxsize.x;
	}
	if (pp->nextrow > pp->row) {
		pp->rfactor = -1;
		pp->rbs = pp->boxsize.y;
		pp->rbw = pp->incrementOfMove;
	} else if (pp->row > pp->nextrow) {
		pp->rfactor = 1;
		pp->rbs = -pp->incrementOfMove;
		pp->rbw = pp->incrementOfMove;
	} else {
		pp->rfactor = 0;
		pp->rbs = 0;
		pp->rbw = pp->boxsize.y;
	}

	if (pp->cfactor == 0)
		pp->lengthOfMove = pp->boxsize.y;
	else if (pp->rfactor == 0)
		pp->lengthOfMove = pp->boxsize.x;
	else
		pp->lengthOfMove = MIN(pp->boxsize.x, pp->boxsize.y);
	pp->Lp = pp->incrementOfMove;
}

static void
wrapupmove(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];

	if (pp->excount) {
		if (pp->forward) {
			pp->fixbuff[pp->prev] = (pp->nextbox + 2) % 4;
			pp->prev++;
		} else
			pp->prev--;
	}
}

static void
wrapupmovedelta(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];

	if (pp->bufferBox) {

		XCopyArea(MI_DISPLAY(mi), pp->bufferBox, MI_WINDOW(mi), pp->backGC,
			  0, 0, pp->boxsize.x - 2, pp->boxsize.y - 2,
			  pp->col * pp->boxsize.x + pp->randompos.x + 1,
			  pp->row * pp->boxsize.y + pp->randompos.y + 1);

		XFlush(MI_DISPLAY(mi));

		pp->row = pp->nextrow;
		pp->col = pp->nextcol;

		XFreePixmap(MI_DISPLAY(mi), pp->bufferBox);
		pp->bufferBox = None;
	}
}

static int
moveboxdelta(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];
	int         cf = pp->nextcol * pp->boxsize.x +
	pp->Lp * pp->cfactor + pp->randompos.x;
	int         rf = pp->nextrow * pp->boxsize.y +
	pp->Lp * pp->rfactor + pp->randompos.y;

	if (pp->Lp <= pp->lengthOfMove) {
		if (pp->bufferBox) {
			XCopyArea(MI_DISPLAY(mi), pp->bufferBox, MI_WINDOW(mi),
				  pp->backGC, 0, 0, pp->boxsize.x - 2, pp->boxsize.y - 2,
				  cf + 1, rf + 1);
			XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), pp->backGC,
				       cf + pp->cbs - 1, rf + pp->rbs - 1, pp->cbw + 2, pp->rbw + 2);
		}
		if ((pp->Lp + pp->incrementOfMove > pp->lengthOfMove) &&
		    (pp->Lp != pp->lengthOfMove))
			pp->Lp = pp->lengthOfMove - pp->incrementOfMove;
		pp->Lp += pp->incrementOfMove;
		return False;
	} else
		return True;
}

static Bool
init_stuff(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	puzzlestruct *pp = &puzzles[MI_SCREEN(mi)];

	if (pp->logo == None)
		getImage(mi, &pp->logo, PUZZLE_WIDTH, PUZZLE_HEIGHT, PUZZLE_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
			 DEFAULT_XPM, PUZZLE_NAME,
#endif
			 &pp->graphics_format, &pp->cmap, &pp->black);
		if (pp->logo == None) {
			free_puzzle(display, pp);
			return False;
		}
#ifndef STANDALONE
	if (pp->cmap != None) {
		setColormap(display, window, pp->cmap, MI_IS_INWINDOW(mi));
		if (pp->backGC == None) {
			XGCValues   xgcv;

			xgcv.background = pp->black;
			if ((pp->backGC = XCreateGC(display, window, GCBackground,
					 &xgcv)) == None) {
				free_puzzle(display, pp);
				return False;
			}
		}
	} else
#endif /* STANDALONE */
	{
		pp->black = MI_BLACK_PIXEL(mi);
		pp->backGC = MI_GC(mi);
	}
	return True;
}

void
init_puzzle(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	puzzlestruct *pp;
	int         x, y;
	XPoint      size;

	if (puzzles == NULL) {
		if ((puzzles = (puzzlestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (puzzlestruct))) == NULL)
			return;
	}
	pp = &puzzles[MI_SCREEN(mi)];

	if (pp->painted && pp->windowsize.x == MI_WIDTH(mi) &&
	    pp->windowsize.y == MI_HEIGHT(mi))
		return;		/* Debounce since refresh_puzzle is init_puzzle */

#if defined( USE_XPM ) || defined( USE_XPMINC )
	if (pp->graphics_format >= IS_XPM) {
        	/* This is needed when another program changes the colormap. */
        	free_puzzle(display, pp);
	}
#endif
	if (!init_stuff(mi))
		return;
	pp->excount = MI_COUNT(mi);
	if (pp->excount < 0) {
		if (pp->fixbuff != NULL) {
			(void) free((void *) pp->fixbuff);
			pp->fixbuff = NULL;
		}
		pp->excount = NRAND(-pp->excount) + 1;
	}
	pp->lastbox = -1;
	pp->moves = 0;
	pp->movingBox = False;

	pp->windowsize.x = MI_WIDTH(mi);
	pp->windowsize.y = MI_HEIGHT(mi);
	if (pp->windowsize.x < 7)
		pp->windowsize.x = 7;
	if (pp->windowsize.y < 7)
		pp->windowsize.y = 7;
	pp->forward = 1;
	pp->prev = 0;
	/* don't want any exposure events from XCopyArea */
	XSetGraphicsExposures(display, pp->backGC, False);

	MI_CLEARWINDOWCOLORMAP(mi, pp->backGC, pp->black);

	if (pp->logo) {
		size.x = (pp->logo->width < pp->windowsize.x) ?
			pp->logo->width : pp->windowsize.x;
		size.y = (pp->logo->height < pp->windowsize.y) ?
			pp->logo->height : pp->windowsize.y;
	} else {
		size.x = pp->windowsize.x;
		size.y = pp->windowsize.y;
	}
	pp->boxsize.y = NRAND(1 + size.y / 4) + 6;
	pp->boxsize.x = NRAND(1 + size.x / 4) + 6;
	if ((pp->boxsize.x > 4 * pp->boxsize.y) ||
	    pp->boxsize.y > 4 * pp->boxsize.x)
		pp->boxsize.x = pp->boxsize.y = 2 * MIN(pp->boxsize.x, pp->boxsize.y);
	pp->count.x = size.x / pp->boxsize.x;
	pp->count.y = size.y / pp->boxsize.y;

	if (pp->bufferBox != None) {
		XFreePixmap(display, pp->bufferBox);
		pp->bufferBox = None;
	}
	pp->usablewindow.x = pp->count.x * pp->boxsize.x;
	pp->usablewindow.y = pp->count.y * pp->boxsize.y;
	pp->offsetwindow.x = (pp->windowsize.x - pp->usablewindow.x) / 2;
	pp->offsetwindow.y = (pp->windowsize.y - pp->usablewindow.y) / 2;

	pp->incrementOfMove = MIN(pp->usablewindow.x, pp->usablewindow.y) / 20;
	pp->incrementOfMove = MAX(pp->incrementOfMove, 1);

	if (pp->logo) {
		pp->randompos.x = NRAND(MAX((pp->windowsize.x - pp->logo->width),
					    2 * pp->offsetwindow.x + 1));
		pp->randompos.y = NRAND(MAX((pp->windowsize.y - pp->logo->height),
					    2 * pp->offsetwindow.y + 1));
		if (MI_NPIXELS(mi) <= 2)
			XSetForeground(display, pp->backGC, MI_WHITE_PIXEL(mi));
		else
			XSetForeground(display, pp->backGC, MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		(void) XPutImage(display, window, pp->backGC, pp->logo,
		(int) (NRAND(MAX(1, (pp->logo->width - pp->usablewindow.x)))),
				 (int) (NRAND(MAX(1, (pp->logo->height - pp->usablewindow.y)))),
				 pp->randompos.x, pp->randompos.y,
				 pp->usablewindow.x, pp->usablewindow.y);
		XSetForeground(display, pp->backGC, pp->black);
		for (x = 0; x <= pp->count.x; x++) {
			int         tempx = x * pp->boxsize.x;

			XDrawLine(display, window, pp->backGC,
				  tempx + pp->randompos.x, pp->randompos.y,
				  tempx + pp->randompos.x, pp->usablewindow.y + pp->randompos.y);
			XDrawLine(display, window, pp->backGC,
				tempx + pp->randompos.x - 1, pp->randompos.y,
				  tempx + pp->randompos.x - 1, pp->usablewindow.y + pp->randompos.y);
		}
		for (y = 0; y <= pp->count.y; y++) {
			int         tempy = y * pp->boxsize.y;

			XDrawLine(display, window, pp->backGC,
				  pp->randompos.x, tempy + pp->randompos.y,
				  pp->usablewindow.x + pp->randompos.x, tempy + pp->randompos.y);
			XDrawLine(display, window, pp->backGC,
				pp->randompos.x, tempy + pp->randompos.y - 1,
				  pp->usablewindow.x + pp->randompos.x, tempy + pp->randompos.y - 1);
		}
	}
#ifdef NUMBERED
	else {
		if (pp->image)
			(void) XDestroyImage(pp->image);
		pp->randompos.x = pp->offsetwindow.x;
		pp->randompos.y = pp->offsetwindow.y;
		if (!NumberScreen(mi)) {
			release_puzzles(mi);
			return;
		}
		if ((pp->image = XGetImage(display, window,
				      pp->offsetwindow.x, pp->offsetwindow.y,
				      pp->usablewindow.x, pp->usablewindow.y,
				      AllPlanes,
				 (MI_NPIXELS(mi) <= 2) ? XYPixmap : ZPixmap)) == None) {
			free_puzzle(display, pp);
			return;
		}
	}

	pp->row = pp->count.y - 1;
	pp->col = pp->count.x - 1;
#else
	pp->row = NRAND(pp->count.y);
	pp->col = NRAND(pp->count.x);
#endif

	if ((pp->excount) && (pp->fixbuff == NULL))
		if ((pp->fixbuff = (int *) calloc(pp->excount,
				sizeof (int))) == NULL) {
			free_puzzle(display, pp);
			return;
		}
	pp->painted = True;
}

void
draw_puzzle(ModeInfo * mi)
{
	puzzlestruct *pp;

	if (puzzles == NULL)
		return;
	pp = &puzzles[MI_SCREEN(mi)];
	if (pp->fixbuff == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	pp->painted = False;
	if (pp->movingBox) {
		if (moveboxdelta(mi)) {
			wrapupmovedelta(mi);
			wrapupmove(mi);
			pp->movingBox = False;
			if (pp->moves++ > 2 * MI_COUNT(mi))
				init_puzzle(mi);
		}
	} else {
		if (setupmove(mi)) {
			setupmovedelta(mi);
			pp->movingBox = True;
		}
	}
}

void
release_puzzle(ModeInfo * mi)
{
	if (puzzles != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_puzzle(MI_DISPLAY(mi), &puzzles[screen]);
		(void) free((void *) puzzles);
		puzzles = NULL;
	}
#ifdef NUMBERED
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
#endif
}

#endif /* MODE_puzzle */
