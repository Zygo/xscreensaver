/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)puzzle.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * puzzle.c - puzzle for xlock, the X Window System lockscreen.
 *
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
 * 10-May-97: Compatible with xscreensaver
 * 15-Mar-96: cleaned up, NUMBERED compile-time switch is now broken.
 * Feb-96: combined with rastering.  Jouk Jansen <joukj@crys.chem.uva.nl>.
 * Feb-95: written.  Heath Rice <hrice@netcom.com>
 */

/*-
 * Chops up the screen into squares and randomly slides them around
 * like that game where you try to rearrange the little tiles in their
 * original order.  After it scrambles the tiles for awhile, it reverses
 * itself and puts them back like they started.  This mode looks the coolest
 * if you have a picture on your background.
 */

#ifdef STANDALONE
#define PROGCLASS            "puzzle"
#define HACK_INIT            init_puzzle
#define HACK_DRAW            draw_puzzle
#define DEF_DELAY            10000
#define DEF_BATCHCOUNT       250
#define DEF_NCOLORS          64
#include "xlockmore.h"    /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"    /* in xlockmore distribution */
ModeSpecOpt puzzle_opts =
{0, NULL, 0, NULL, NULL};
 
#endif /* STANDALONE */

#include <time.h>

#define PUZZLE_WIDTH   image_width
#define PUZZLE_HEIGHT    image_height
#define PUZZLE_BITS    image_bits
#define NOWAY 255
#include "puzzle.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define PUZZLE_NAME    image_name
#include "puzzle.xpm"
#endif

extern void getImage(ModeInfo * mi, XImage ** logo,
		     int width, int height, unsigned char *bits,
#if defined( USE_XPM ) || defined( USE_XPMINC )
		     char **name,
#endif
		     int *graphics_format, Colormap * newcolormap,
		     unsigned long *blackpix, unsigned long *whitepix);
extern void destroyImage(XImage ** logo, int *graphics_format);

typedef struct {
	int         excount;
	int        *fixbuff;
	XPoint      count, boxsize, windowsize;
	XPoint      usablewindow, offsetwindow;
	XPoint      randompos, randpos;
	unsigned long black, white;
	int         row, col, nextrow, nextcol, nextbox;
	int         incrementOfMove;
	int         lastbox;
	int         forward;
	int         prev;
	int         moves;

	/* Delta move stuff */
	int         movingBox;
	XImage     *box;
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
	Colormap    cm;
	int         graphics_format;
#endif
} puzzlestruct;

static puzzlestruct *puzzs = NULL;

#ifdef NUMBERED
extern XFontStruct *getFont();

#define font_height(f) (f->ascent + f->descent)
static XFontStruct *mode_font = None;

static int
font_width(XFontStruct * font, char ch)
{
	int         dummy;
	XCharStruct xcs;

	XTextExtents(font, &ch, 1, &dummy, &dummy, &dummy, &xcs);
	return xcs.width;
}

void
NumberScreen(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];

	if (mode_font == None)
		mode_font = getFont(display);
	if (!pp->done) {
		XGCValues   gcv;

		pp->done = 1;
		gcv.font = mode_font->fid;
		XSetFont(display, MI_GC(mi), mode_font->fid);
		gcv.graphics_exposures = False;
		gcv.foreground = MI_WIN_WHITE_PIXEL(mi);
		gcv.background = MI_WIN_BLACK_PIXEL(mi);
		pp->gc = XCreateGC(display, window,
				   GCForeground | GCBackground | GCGraphicsExposures | GCFont, &gcv);
		pp->ascent = mode_font->ascent;
		pp->fontHeight = font_height(mode_font);
		pp->fontWidth = font_width(mode_font, '5');
	}
	XSetForeground(display, pp->gc, MI_WIN_WHITE_PIXEL(mi));

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
						XDrawString(display, window, pp->gc,
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
}
#endif

static int
setupmove(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];

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
	Window      window = MI_WINDOW(mi);
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];

	/* if (pp->boxsize.x > 1 && pp->boxsize.y > 1) This is already true */

	/* There is a big problem here when used with -inwindow , possible solutions
	 * seem to expose more levels of complication when implemented.
	 */
	/* UnmapNotify VisibilityPartiallyObscured)) */
	if (pp->box) {
		XDestroyImage(pp->box);
		pp->box = NULL;
	} {
		/* This will bomb out easily if using -inwindow or -debug .
		 * X Error of failed request:  BadMatch (invalid parameter attributes)
		 */
		XWindowAttributes wa;

		(void) XGetWindowAttributes(display, window, &wa);

#if 0
		if (!(wa.all_event_masks & NoExpose))
#endif
#if 0
			do {
				(void) XGetWindowAttributes(display, window, &wa);
			}
			while (wa.map_state != IsViewable);
#endif
		if (wa.map_state == IsViewable)
/* Next line bombs out with BadMatch if moved off screen. */
			pp->box = XGetImage(display, window,
			   pp->nextcol * pp->boxsize.x + pp->randompos.x + 1,
			   pp->nextrow * pp->boxsize.y + pp->randompos.y + 1,
					pp->boxsize.x - 2, pp->boxsize.y - 2,
					    AllPlanes, ZPixmap);
	}
#if 0
	/* Current state of the puzzle has to be known for this to work
	 * (pp->nextrow) and (pp->nextcol) are the correct starting values, but
	 * as the puzzle progresses it is not correct.
	 */
	if (pp->graphics_format >= IS_XPM)
		/* This can only be used with a fully initialized image where logo is
		 * initialized directly or indirectly with XCreateImage .
		 * It also has the disadvantage that it uses XGetPixel and XPutPixel
		 * internally, so it could be slow.
		 */
		pp->box = XSubImage(pp->logo,
				    (pp->nextrow) * pp->boxsize.x + 1,
				    (pp->nextcol) * pp->boxsize.y + 1,
				    pp->boxsize.x - 2, pp->boxsize.y - 2);
#endif
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
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];

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
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];

	if (pp->box) {
		XPutImage(MI_DISPLAY(mi), MI_WINDOW(mi), pp->backGC, pp->box, 0, 0,
			  pp->col * pp->boxsize.x + pp->randompos.x + 1,
			  pp->row * pp->boxsize.y + pp->randompos.y + 1,
			  pp->boxsize.x - 2, pp->boxsize.y - 2);
		XFlush(MI_DISPLAY(mi));

		pp->row = pp->nextrow;
		pp->col = pp->nextcol;

		XDestroyImage(pp->box);
		pp->box = NULL;
	}
}

static int
moveboxdelta(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];
	int         cf = pp->nextcol * pp->boxsize.x +
	pp->Lp * pp->cfactor + pp->randompos.x;
	int         rf = pp->nextrow * pp->boxsize.y +
	pp->Lp * pp->rfactor + pp->randompos.y;

	if (pp->Lp <= pp->lengthOfMove) {
		if (pp->box) {
			XPutImage(MI_DISPLAY(mi), MI_WINDOW(mi), pp->backGC, pp->box,
				  0, 0, cf + 1, rf + 1, pp->boxsize.x - 2, pp->boxsize.y - 2);
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

void
init_puzzle(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	puzzlestruct *pp;
	int         x, y;
	XPoint      size;

	if (puzzs == NULL) {
		if ((puzzs = (puzzlestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (puzzlestruct))) == NULL)
			return;
	}
	pp = &puzzs[MI_SCREEN(mi)];

	pp->excount = MI_BATCHCOUNT(mi);
	if (pp->excount < 0) {
		if (pp->fixbuff != NULL)
			(void) free((void *) pp->fixbuff);
		pp->excount = NRAND(-pp->excount + 1);
	}
	pp->lastbox = -1;
	pp->moves = 0;
	pp->movingBox = False;

	pp->windowsize.x = MI_WIN_WIDTH(mi);
	pp->windowsize.y = MI_WIN_HEIGHT(mi);
	if (pp->windowsize.x < 7)
		pp->windowsize.x = 7;
	if (pp->windowsize.y < 7)
		pp->windowsize.y = 7;
	pp->forward = 1;
	pp->prev = 0;

	if (pp->box) {
		XDestroyImage(pp->box);
		pp->box = NULL;
	}
	if (!pp->logo)
		getImage(mi, &pp->logo, PUZZLE_WIDTH, PUZZLE_HEIGHT, PUZZLE_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
			 PUZZLE_NAME,
#endif
			 &pp->graphics_format, &pp->cm,
			 &pp->black, &pp->white);
	if (pp->cm != None) {
		if (pp->backGC == None) {
			XGCValues   xgcv;

			setColormap(display, window, pp->cm, MI_WIN_IS_INWINDOW(mi));
			xgcv.background = pp->black;
			pp->backGC = XCreateGC(display, window, GCBackground, &xgcv);
		}
	} else {
		pp->black = MI_WIN_BLACK_PIXEL(mi);
		pp->backGC = MI_GC(mi);
	}
	XSetForeground(display, pp->backGC, pp->black);
	XFillRectangle(display, window, pp->backGC, 0, 0,
		       pp->windowsize.x, pp->windowsize.y);
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
			XSetForeground(display, pp->backGC, MI_WIN_WHITE_PIXEL(mi));
		else
			XSetForeground(display, pp->backGC, MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		XPutImage(display, window, pp->backGC, pp->logo,
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
			XDestroyImage(pp->image);
		pp->randompos.x = pp->offsetwindow.x;
		pp->randompos.y = pp->offsetwindow.y;
		NumberScreen(mi);
		pp->image = XGetImage(display, window,
				      pp->offsetwindow.x, pp->offsetwindow.y,
				      pp->usablewindow.x, pp->usablewindow.y,
				      AllPlanes,
				 (MI_NPIXELS(mi) <= 2) ? XYPixmap : ZPixmap);
	}

	pp->row = pp->count.y - 1;
	pp->col = pp->count.x - 1;
#else
	pp->row = NRAND(pp->count.y);
	pp->col = NRAND(pp->count.x);
#endif

	if ((pp->excount) && (pp->fixbuff == NULL))
		if ((pp->fixbuff = (int *) calloc(pp->excount, sizeof (int))) == NULL)
			            error("%s: Couldn't allocate memory for puzzle buffer\n");
}

void
draw_puzzle(ModeInfo * mi)
{
	puzzlestruct *pp = &puzzs[MI_SCREEN(mi)];

	if (pp->movingBox) {
		if (moveboxdelta(mi)) {
			wrapupmovedelta(mi);
			wrapupmove(mi);
			pp->movingBox = False;
			if (pp->moves++ > 2 * MI_BATCHCOUNT(mi))
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
	if (puzzs != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			puzzlestruct *pp = &puzzs[screen];

			if (pp->fixbuff != NULL)
				(void) free((void *) pp->fixbuff);
			if (pp->cm != None) {
				if (pp->backGC != None)
					XFreeGC(MI_DISPLAY(mi), pp->backGC);
				XFreeColormap(MI_DISPLAY(mi), pp->cm);
			}
			destroyImage(&pp->logo, &pp->graphics_format);
			if (pp->box)
				XDestroyImage(pp->box);
		}
		(void) free((void *) puzzs);
		puzzs = NULL;
	}
#ifdef NUMBERED
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
#endif
}
