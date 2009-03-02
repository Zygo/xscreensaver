/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)dclock.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * dclock.c - floating digital clock screen for xlock,
 *            the X Window System lockscreen.
 *
 * Copyright (C) 1995 by Michael Stembera <mrbig@fc.net>.
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
 * 29-Aug-95: Written.
 */

#ifdef STANDALONE
#define PROGCLASS            "Dclock"
#define HACK_INIT            init_dclock
#define HACK_DRAW            draw_dclock
#define DEF_DELAY            10000
#define DEF_CYCLES           10000
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt dclock_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

#include <time.h>

#define font_height(f) (f->ascent + f->descent)

extern XFontStruct *getFont(Display * display);

typedef struct {
	int         color;
	short       height, width;
	char       *str, str1[20], str2[20], str1old[40], str2old[40];
	char       *str1pta, *str2pta, *str1ptb, *str2ptb;
	time_t      timenew, timeold;
	short       maxx, maxy, clockx, clocky;
	short       text_height, text_width, cent_offset;
	short       hour;
	short       dx, dy;
	int         done;
	int         pixw, pixh;
	Pixmap      pixmap;
	GC          fgGC, bgGC;
} dclockstruct;

static dclockstruct *dclocks = NULL;

static XFontStruct *mode_font = None;

static void
drawDclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];
	short       xold, yold;
	char       *tmppt;

	xold = dp->clockx;
	yold = dp->clocky;
	dp->clockx += dp->dx;
	dp->clocky += dp->dy;

	if (dp->maxx < dp->cent_offset) {
		if (dp->clockx < dp->maxx + dp->cent_offset ||
		    dp->clockx > dp->cent_offset) {
			dp->dx = -dp->dx;
			dp->clockx += dp->dx;
		}
	} else if (dp->maxx > dp->cent_offset) {
		if (dp->clockx > dp->maxx + dp->cent_offset ||
		    dp->clockx < dp->cent_offset) {
			dp->dx = -dp->dx;
			dp->clockx += dp->dx;
		}
	}
	if (dp->maxy < mode_font->ascent) {
		if (dp->clocky > mode_font->ascent || dp->clocky < dp->maxy) {
			dp->dy = -dp->dy;
			dp->clocky += dp->dy;
		}
	} else if (dp->maxy > mode_font->ascent) {
		if (dp->clocky > dp->maxy || dp->clocky < mode_font->ascent) {
			dp->dy = -dp->dy;
			dp->clocky += dp->dy;
		}
	}
	if (dp->timeold != (dp->timenew = time((time_t *) NULL))) {
		/* only parse if time has changed */
		dp->timeold = dp->timenew;
		dp->str = ctime(&dp->timeold);

		/* keep last disp time so it can be cleared even if it changed */
		tmppt = dp->str1ptb;
		dp->str1ptb = dp->str1pta;
		dp->str1pta = tmppt;
		tmppt = dp->str2ptb;
		dp->str2ptb = dp->str2pta;
		dp->str2pta = tmppt;

		/* copy the hours portion for 24 to 12 hr conversion */
		(void) strncpy(dp->str1pta, (dp->str + 11), 8);
		dp->hour = (short) (dp->str1pta[0] - 48) * 10 +
			(short) (dp->str1pta[1] - 48);
		if (dp->hour > 12) {
			dp->hour -= 12;
			(void) strcpy(dp->str1pta + 8, " PM");
		} else {
			if (dp->hour == 0)
				dp->hour += 12;
			(void) strcpy(dp->str1pta + 8, " AM");
		}
		dp->str1pta[0] = (dp->hour / 10) + 48;
		dp->str1pta[1] = (dp->hour % 10) + 48;
		if (dp->str1pta[0] == '0')
			dp->str1pta[0] = ' ';

		/* copy day month */
		(void) strncpy(dp->str2pta, dp->str, 11);
		/* copy year */
		(void) strncpy(dp->str2pta + 11, (dp->str + 20), 4);
	}
	if (dp->pixw != dp->text_width || dp->pixh != 2 * dp->text_height) {
		XGCValues   gcv;

		if (dp->fgGC)
			XFreeGC(display, dp->fgGC);
		if (dp->bgGC)
			XFreeGC(display, dp->bgGC);
		if (dp->pixmap) {
			XFreePixmap(display, dp->pixmap);
			XClearWindow(display, window);
		}
		dp->pixw = dp->text_width;
		dp->pixh = 2 * dp->text_height;
		dp->pixmap = XCreatePixmap(display, window, dp->pixw, dp->pixh, 1);
		gcv.font = mode_font->fid;
		gcv.background = 0;
		gcv.foreground = 1;
		dp->fgGC = XCreateGC(display, dp->pixmap,
				 GCForeground | GCBackground | GCFont, &gcv);
		gcv.foreground = 0;
		dp->bgGC = XCreateGC(display, dp->pixmap,
				 GCForeground | GCBackground | GCFont, &gcv);
	}
	XFillRectangle(display, dp->pixmap, dp->bgGC, 0, 0, dp->pixw, dp->pixh);

	XDrawString(display, dp->pixmap, dp->fgGC,
		    dp->cent_offset, mode_font->ascent,
		    dp->str1pta, strlen(dp->str1pta));
	XDrawString(display, dp->pixmap, dp->fgGC,
		    0, mode_font->ascent + dp->text_height,
		    dp->str2pta, strlen(dp->str2pta));
	XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
	/* This could leave screen dust on the screen if the width changes
	   But that only happens once a day...
	   ... this is solved by the ClearWindow above
	 */
	ERASE_IMAGE(display, window, gc,
	    (dp->clockx - dp->cent_offset), (dp->clocky - mode_font->ascent),
		    (xold - dp->cent_offset), (yold - mode_font->ascent),
		    dp->pixw, dp->pixh);
	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, dp->color));
	else
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	XCopyPlane(display, dp->pixmap, window, gc,
		   0, 0, dp->text_width, 2 * dp->text_height,
		dp->clockx - dp->cent_offset, dp->clocky - mode_font->ascent,
		   1L);
}

void
init_dclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp;

	if (dclocks == NULL) {
		if ((dclocks = (dclockstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (dclockstruct))) == NULL)
			return;
	}
	dp = &dclocks[MI_SCREEN(mi)];

	dp->width = MI_WIN_WIDTH(mi);
	dp->height = MI_WIN_HEIGHT(mi);

	XClearWindow(display, MI_WINDOW(mi));

	if (mode_font == None)
		mode_font = getFont(display);
	if (!dp->done) {
		dp->done = 1;
		if (mode_font != None)
			XSetFont(display, MI_GC(mi), mode_font->fid);
	}
	/* (void)time(&dp->timenew); */
	dp->timeold = dp->timenew = time((time_t *) NULL);
	dp->str = ctime(&dp->timeold);
	dp->dx = (LRAND() & 1) ? 1 : -1;
	dp->dy = (LRAND() & 1) ? 1 : -1;

	(void) strncpy(dp->str1, (dp->str + 11), 8);
	dp->hour = (short) (dp->str1[0] - 48) * 10 + (short) (dp->str1[1] - 48);
	if (dp->hour > 12) {
		dp->hour -= 12;
		(void) strcpy(dp->str1 + 8, " PM");
	} else {
		if (dp->hour == 0)
			dp->hour += 12;
		(void) strcpy(dp->str1 + 8, " AM");
	}
	dp->str1[0] = (dp->hour / 10) + 48;
	dp->str1[1] = (dp->hour % 10) + 48;
	if (dp->str1[0] == '0')
		dp->str1[0] = ' ';
	dp->str1[11] = 0;	/* terminate dp->str1 */
	dp->str1old[11] = 0;	/* terminate dp->str1old */

	(void) strncpy(dp->str2, dp->str, 11);
	(void) strncpy(dp->str2 + 11, (dp->str + 20), 4);
	dp->str2[15] = 0;	/* terminate dp->str2 */
	dp->str2old[15] = 0;	/* terminate dp->str2old */

	dp->text_height = font_height(mode_font);
	dp->text_width = XTextWidth(mode_font, dp->str2, strlen(dp->str2));
	dp->cent_offset = (dp->text_width -
		      XTextWidth(mode_font, dp->str1, strlen(dp->str1))) / 2;
	dp->maxx = dp->width - dp->text_width;
	dp->maxy = dp->height - dp->text_height - mode_font->descent;
	if (dp->maxx == 0)
		dp->clockx = dp->cent_offset;
	else if (dp->maxx < 0)
		dp->clockx = -NRAND(-dp->maxx) + dp->cent_offset;
	else
		dp->clockx = NRAND(dp->maxx) + dp->cent_offset;
	if (dp->maxy - mode_font->ascent == 0)
		dp->clocky = mode_font->ascent;
	else if (dp->maxy - mode_font->ascent < 0)
		dp->clocky = -NRAND(mode_font->ascent - dp->maxy) + mode_font->ascent;
	else
		dp->clocky = NRAND(dp->maxy - mode_font->ascent) + mode_font->ascent;

	dp->str1pta = dp->str1;
	dp->str2pta = dp->str2;
	dp->str1ptb = dp->str1old;
	dp->str2ptb = dp->str2old;

	if (MI_NPIXELS(mi) > 2)
		dp->color = NRAND(MI_NPIXELS(mi));
}

void
draw_dclock(ModeInfo * mi)
{
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];

	drawDclock(mi);
	if (MI_NPIXELS(mi) > 2) {
		if (++dp->color >= MI_NPIXELS(mi))
			dp->color = 0;
	}
}

void
release_dclock(ModeInfo * mi)
{
	if (dclocks != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			dclockstruct *dp = &dclocks[screen];
			Display    *display = MI_DISPLAY(mi);

			if (dp->fgGC)
				XFreeGC(display, dp->fgGC);
			if (dp->bgGC)
				XFreeGC(display, dp->bgGC);
			if (dp->pixmap)
				XFreePixmap(display, dp->pixmap);

		}
		(void) free((void *) dclocks);
		dclocks = NULL;
	}
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
}

void
refresh_dclock(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
