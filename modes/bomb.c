/* -*- Mode: C; tab-width: 4 -*- */
/* bomb --- temporary screen */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bomb.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1994 by Dave Shield
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
 * 10-May-1997: Made more compatible with xscreensaver :)
 * 09-Jan-1995: Assorted defines to control various aspects of bomb mode.
 *              Uncomment, or otherwise define the appropriate line
 *              to obtain the relevant behaviour, thanks to Dave Shield
 *              <D.T.Shield@csc.liv.ac.uk>.
 * 20-Dec-1994: Time patch for multiprocessor machines (ie. Sun10) thanks to
 *              Nicolas Pioch <pioch@Email.ENST.Fr>.
 * 1994:        Written.  Dave Shield  Liverpool Computer Science
 */

/*-
 * This mode may have limited appeal.  Its good for logging yourself out
 * if you do not know if your going to be back.  It is no longer to be used
 * as a way of forcing users in a lab to logout after locking the screen.
 */


#ifdef STANDALONE
#define USE_BOMB
#define PROGCLASS "Bomb"
#define HACK_INIT init_bomb
#define HACK_DRAW draw_bomb
#define bomb_opts xlockmore_opts
#define DEFAULTS "*delay: 1000000 \n" \
 "*count: 10 \n" \
 "*cycles: 20 \n" \
 "*ncolors: 200 \n" \
 "*verbose: False \n"
#define UNIFORM_COLORS
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_bomb

ModeSpecOpt bomb_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   bomb_description =
{"bomb", "init_bomb", "draw_bomb", "release_bomb",
 "refresh_bomb", "change_bomb", NULL, &bomb_opts,
 100000, 10, 20, 1, 64, 1.0, "",
 "Shows a bomb and will autologout after a time", 0, NULL};

#endif

#include <sys/signal.h>

#if 0
#define SIMPLE_COUNTDOWN	/* Display a simple integer countdown,     */
#endif
			      /*  rather than a "MIN:SEC" format.        */
#define COLOUR_CHANGE		/* Display cycles through the colour wheel */
			      /*  rather than staying red throughout.    */

#ifdef USE_MB
#define FULL_COUNT_FONT         "-adobe-courier-bold-r-*-*-34-*-*-*-*-*-iso8859-1"
#define ICON_COUNT_FONT         "-misc-fixed-medium-r-normal-*-8-*-*-*-*-*-iso8859-1"
#else
#define FULL_COUNT_FONT         "-*-*-*-*-*-*-34-*-*-*-*-*-*-*"
#define ICON_COUNT_FONT         "-*-*-*-*-*-*-8-*-*-*-*-*-*-*"
#endif
#define COUNTDOWN       600	/* No. seconds to lock for */
#define NDIGITS         4	/* Number of digits in count */

#define MAX_DELAY       1000000	/* Max delay between updates */
#define NAP_TIME        5	/* Sleep between shutdown attempts */
#define DELTA           10	/* Border around the digits */
#define RIVET_RADIUS    6	/* Size of detonator 'rivets' */

#ifdef USE_MB
#define free_font(d) if (mode_font!=None){XFreeFontSet(d,mode_font); \
mode_font = None;}
#else
#define free_font(d) if (mode_font!=None){XFreeFont(d,mode_font); \
mode_font = None;}
#endif

extern XFontStruct *getFont(Display * display);

typedef struct {
	Bool        painted;
	int         width, height;
	int         x, y;
	XPoint      loc;
	int         delta;
	int         color;
	time_t      countdown;
	int         startcountdown;
	int         text_width;
	int         text_ascent;
	int         text_descent;
	int         moveok;
} bombstruct;

static bombstruct *bombs = NULL;

#ifdef USE_MB
static XFontSet mode_font = None;
#else
static XFontStruct *mode_font = None;
#endif

static void
rivet(ModeInfo * mi, int x, int y)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	if (MI_NPIXELS(mi) > 2) {
		XDrawArc(display, MI_WINDOW(mi), gc, x, y,
		      2 * RIVET_RADIUS, 2 * RIVET_RADIUS, 270 * 64, 90 * 64);
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		XDrawArc(display, MI_WINDOW(mi), gc, x, y,
		      2 * RIVET_RADIUS, 2 * RIVET_RADIUS, 70 * 64, 130 * 64);
	} else
		XDrawArc(display, MI_WINDOW(mi), gc, x, y,
			 2 * RIVET_RADIUS, 2 * RIVET_RADIUS, 0, 360 * 64);
}

static void
detonator(ModeInfo * mi, int draw)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	bombstruct *bp = &bombs[MI_SCREEN(mi)];
	int         b_width, b_height;

	b_width = bp->width / 2;
	b_height = bp->height / 3;
	if (draw) {
#ifdef SOLARIS2
		/*
		 * if this is not done the rectangle is sometimes messed up on
		 * Solaris2 with 24 bit TrueColor (Ultra2)
		 */
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		XFillRectangle(display, MI_WINDOW(mi), gc,
			       bp->loc.x, bp->loc.y, b_width, b_height);
#endif
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
				       MI_PIXEL(mi, bp->color));
		else
			XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
		/*XSetForeground(display, gc, allocPixel(display, MI_COLORMAP(mi),
		   "grey", "white")); */
		XFillRectangle(display, MI_WINDOW(mi), gc,
			       bp->loc.x, bp->loc.y, b_width, b_height);

		/*
		 *  If a full size screen (and colour),
		 *      'rivet' the box to it
		 */
		if (bp->width > 160 && bp->height > 160) {
			rivet(mi, bp->loc.x + RIVET_RADIUS, bp->loc.y + RIVET_RADIUS);
			rivet(mi, bp->loc.x + RIVET_RADIUS,
			      bp->loc.y + b_height - 3 * RIVET_RADIUS);
			rivet(mi, bp->loc.x + b_width - 3 * RIVET_RADIUS,
			      bp->loc.y + RIVET_RADIUS);
			rivet(mi, bp->loc.x + b_width - 3 * RIVET_RADIUS,
			      bp->loc.y + b_height - 3 * RIVET_RADIUS);
		}
	} else {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		XFillRectangle(display, MI_WINDOW(mi), gc,
			       bp->loc.x, bp->loc.y, b_width, b_height);
	}
}

void
release_bomb(ModeInfo * mi)
{
	if (bombs != NULL) {
		(void) free((void *) bombs);
		bombs = NULL;
	}
	free_font(MI_DISPLAY(mi));
}

void
init_bomb(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	bombstruct *bp;
	char        number[NDIGITS + 2];
#ifdef USE_MB
	char **miss, *def;
	int n_miss;
	XRectangle ink, log;
#endif

	if (bombs == NULL) {
		if ((bombs = (bombstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (bombstruct))) == NULL)
			return;
	}
	bp = &bombs[MI_SCREEN(mi)];

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);

	if (mode_font != None) {
#ifdef USE_MB
		XFreeFontSet(display, mode_font);
#else
		XFreeFont(display, mode_font);
#endif
		mode_font = None;
	}
	/* Set up text font */
	if (bp->width > 256 && bp->height > 256) {	/* Full screen */
#ifdef USE_MB
		mode_font = XCreateFontSet(display, FULL_COUNT_FONT, &miss, &n_miss, &def);
#else
		mode_font = XLoadQueryFont(display, FULL_COUNT_FONT);
#endif
		bp->delta = DELTA;
	} else {		/* icon window */
#ifdef USE_MB
		mode_font = XCreateFontSet(display, ICON_COUNT_FONT, &miss, &n_miss, &def);
#else
		mode_font = XLoadQueryFont(display, ICON_COUNT_FONT);
#endif
		bp->delta = 2;
	}
	if (mode_font == None) {
#ifdef USE_MB
		mode_font = XCreateFontSet(display, "-*-medium-r-normal--14-*", &miss, &n_miss, &def);
#else
		mode_font = getFont(display);
#endif
	}
	if (mode_font == None) {
		release_bomb(mi);
		return;
	}
#ifndef USE_MB
	XSetFont(display, gc, mode_font->fid);
#endif

#ifdef SIMPLE_COUNTDOWN
	(void) sprintf(number, "%0*d", NDIGITS, 0);
#else
#ifdef USE_MB
	(void) sprintf(number, "%.*s:00", NDIGITS - 2, "0000000");
#else
	(void) sprintf(number, "%.*s:**", NDIGITS - 2, "*******");
#endif
#endif
	/* if (mode_font == None) bp->text_width = 8; */
#ifdef USE_MB
	XmbTextExtents(mode_font, number, strlen(number), &ink, &log);
	bp->text_width = ink.width;
	bp->text_ascent = ink.height;
	bp->text_descent = 0;
#else
	bp->text_width = XTextWidth(mode_font, number, NDIGITS + 1);
	bp->text_ascent = mode_font->max_bounds.ascent;
	bp->text_descent = mode_font->max_bounds.descent;
#endif

#ifndef STANDALONE
	if (MI_DELAY(mi) > MAX_DELAY)
		MI_DELAY(mi) = MAX_DELAY;	/* Time cannot move slowly */
#endif /* STANDALONE */
	if (MI_COUNT(mi) < 1)
		bp->startcountdown = 1;		/* Do not want an instantaneous logout */
	bp->startcountdown = MI_COUNT(mi);
	bp->startcountdown *= 60;
#if 0				/* Stricter if uncommented but people do not have to run bomb */
	if (bp->startcountdown > COUNTDOWN)
		bp->startcountdown = COUNTDOWN;
#endif
	if (bp->countdown == 0)	/* <--Stricter if uncommented */
		bp->countdown = time((time_t *) NULL) + bp->startcountdown;
	/* Detonator Primed */

	MI_CLEARWINDOW(mi);
	bp->painted = False;

	/*
	 *  Draw the graphics
	 *      Very simple - detonator box with countdown
	 *
	 *  ToDo:  Improve the graphics
	 *      (e.g. stick of dynamite, with burning fuse?)
	 */
	bp->loc.x = NRAND(bp->width / 2);
	bp->loc.y = NRAND(bp->height * 3 / 5);
	bp->x = bp->loc.x + bp->width / 4 - (bp->text_width / 2);
	bp->y = bp->loc.y + bp->height / 5;	/* Central-ish */
	if (MI_NPIXELS(mi) > 2)
		bp->color = NRAND(MI_NPIXELS(mi));
	detonator(mi, 1);
	bp->moveok = 0;
}

	/*
	 *  Game Over
	 */
static void
explode(ModeInfo * mi)
{
	bombstruct *bp = &bombs[MI_SCREEN(mi)];
	char        buff[NDIGITS + 2];
	extern void logoutUser(Display * display);

#ifdef SunCplusplus
/* #include <signal.h> */
	extern int  kill(pid_t, int);

#else
#if 0
	extern int  kill(int, int);

#endif
#endif

	/*
	 *  ToDo:
	 *      Improve the graphics - some sort of explosion?
	 *      (Will need to involve the main X event loop)
	 */
#ifdef USE_MB
#ifdef SIMPLE_COUNTDOWN
	(void) sprintf(buff, "%.*s", NDIGITS, "000000000");
#else
	(void) sprintf(buff, "%.*s:00", NDIGITS - 2, "0000000");
#endif
#else
#ifdef SIMPLE_COUNTDOWN
	(void) sprintf(buff, "%.*s", NDIGITS, "*********");
#else
	(void) sprintf(buff, "%.*s:**", NDIGITS - 2, "*******");
#endif
#endif
	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, 1));
	(void) XDrawString(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			   bp->x, bp->y, buff, NDIGITS);

	(void) fprintf(stderr, "BOOM!!!!\n");
#ifndef DEBUG
	if (MI_IS_INWINDOW(mi) || MI_IS_INROOT(mi) ||
	    MI_IS_NOLOCK(mi) || MI_IS_DEBUG(mi))
		(void) kill((int) getpid(), SIGTERM);
	else if ((int) getuid() == 0) {		/* Do not try to logout root! */
		bp->countdown = 0;
		init_bomb(mi);
	} else
		logoutUser(MI_DISPLAY(mi));
#else
	(void) kill(getpid(), SIGTERM);
#endif
}

void
draw_bomb(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	char        number[NDIGITS + 2];
	unsigned long crayon;
	int         countleft;
	bombstruct *bp;

	if (bombs == NULL)
		return;
	bp = &bombs[MI_SCREEN(mi)];
	if (mode_font == None)
		return;

	countleft = (int) (bp->countdown - time((time_t *) NULL));
	if (countleft <= 0)
		explode(mi);	/* Bye, bye.... */
	else {
		bp->painted = True;
#ifdef SIMPLE_COUNTDOWN
		(void) sprintf(number, "%0*d", NDIGITS, countleft);
#else
		(void) sprintf(number, "%0*d:%02d", NDIGITS - 2,
			       countleft / 60, countleft % 60);
#endif

		/* Blank out the previous number .... */
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, MI_WINDOW(mi), gc,
			       bp->x - bp->delta,
			       (bp->y - bp->text_ascent) - bp->delta,
			       bp->text_width + (2 * bp->delta),
		     (bp->text_ascent + bp->text_descent) + (2 * bp->delta));

		/* ... and count down */
		if (MI_NPIXELS(mi) <= 2)
			crayon = MI_WHITE_PIXEL(mi);
		else
#ifdef COLOUR_CHANGE		/* Blue to red */
			crayon = MI_PIXEL(mi, countleft * MI_NPIXELS(mi) / bp->startcountdown);
#else
			crayon = MI_PIXEL(mi, 1);
#endif
		if (!(countleft % 60)) {
			if (bp->moveok) {
				detonator(mi, 0);
				bp->loc.x = NRAND(bp->width / 2);
				bp->loc.y = NRAND(bp->height * 3 / 5);
				bp->x = bp->loc.x + bp->width / 4 - (bp->text_width / 2);
				bp->y = bp->loc.y + bp->height / 5;	/* Central-ish */
				if (MI_NPIXELS(mi) > 2)
					bp->color = NRAND(MI_NPIXELS(mi));
				detonator(mi, 1);
				bp->moveok = 0;
			}
		} else {
			bp->moveok = 1;
		}
		XSetForeground(display, gc, crayon);
#ifndef USE_MB
		(void) XDrawString(display, MI_WINDOW(mi), gc, bp->x, bp->y,
				   number, strlen(number));
#else
		(void) XmbDrawString(display, MI_WINDOW(mi), mode_font, gc, bp->x, bp->y,
							 number, strlen(number));
#endif
	}
}

void
refresh_bomb(ModeInfo * mi)
{
	bombstruct *bp;

	if (bombs == NULL)
		return;
	bp = &bombs[MI_SCREEN(mi)];
	if (mode_font == None)
		return;

	if (bp->painted) {
		MI_CLEARWINDOW(mi);
		detonator(mi, 1);
	}
}

void
change_bomb(ModeInfo * mi)
{
	bombstruct *bp;

	if (bombs == NULL)
		return;
	bp = &bombs[MI_SCREEN(mi)];
	if (mode_font == None)
		return;

	detonator(mi, 0);
	bp->painted = False;
	bp->loc.x = NRAND(bp->width / 2);
	bp->loc.y = NRAND(bp->height * 3 / 5);
	bp->x = bp->loc.x + bp->width / 4 - (bp->text_width / 2);
	bp->y = bp->loc.y + bp->height / 5;	/* Central-ish */
	if (MI_NPIXELS(mi) > 2)
		bp->color = NRAND(MI_NPIXELS(mi));
	detonator(mi, 1);
	bp->moveok = 0;
}

#endif  /* MODE_bomb */
