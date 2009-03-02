/* -*- Mode: C; tab-width: 4 -*- */
/* deco --- art as ugly as sin */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)deco.c 4.07 97/11/24 xlockmore";

#endif
/* 
 * Copyright (c) 1997 by Jamie Zawinski <jwz@jwz.org>
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
 * 29-Oct-97: xlock version (David Bagley <bagleyd@bigfoot.com>)
 * 1997: xscreensaver version Jamie Zawinski <jwz@jwz.org>
 */

/*-
 * original copyright
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Concept snarfed from Michael D. Bayne in
 * http://www.go2net.com/internet/deep/1997/04/16/body.html
 */

#ifdef STANDALONE
#define PROGCLASS "Deco"
#define HACK_INIT init_deco
#define HACK_DRAW draw_deco
#define deco_opts xlockmore_opts
#define DEFAULTS "*delay: 1000000 \n" \
 "*count: -30 \n" \
 "*cycles: 2 \n" \
 "*size: -10 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_deco

ModeSpecOpt deco_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   deco_description =
{"deco", "init_deco", "draw_deco", "release_deco",
 "init_deco", "init_deco", NULL, &deco_opts,
 1000000, -30, 2, -10, 64, 0.6, "",
 "Shows art as ugly as sin", 0, NULL};

#endif

#define MINSIZE 2
#define MINDEPTH 1

typedef struct {
	int         max_depth;
	int         min_height;
	int         min_width;
	int         time;
	int         colorindex;
	unsigned long bordercolor;
} decostruct;

static decostruct *decos = NULL;

static void
deco(ModeInfo * mi, int x, int y, int w, int h, int depth)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	decostruct *dp = &decos[MI_SCREEN(mi)];

	if ((NRAND(dp->max_depth) + 1 < depth) ||
	    (w < dp->min_width) || (h < dp->min_height)) {
		if (w > 2 && h > 2) {
			if (MI_NPIXELS(mi) > 2) {
				XSetForeground(display, gc, MI_PIXEL(mi, dp->colorindex));
				if (++dp->colorindex >= MI_NPIXELS(mi))
					dp->colorindex = 0;
			} else
				XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
			XFillRectangle(display, window, gc, x + 1, y + 1, w - 2, h - 2);
		}
	} else {
		if (LRAND() & 1) {
			deco(mi, x, y, w / 2, h, depth + 1);
			deco(mi, x + w / 2, y, w - w / 2, h, depth + 1);
		} else {
			deco(mi, x, y, w, h / 2, depth + 1);
			deco(mi, x, y + h / 2, w, h - h / 2, depth + 1);
		}
	}
}

void
init_deco(ModeInfo * mi)
{
	decostruct *dp;
	int         depth = MI_COUNT(mi);
	int         size = MI_SIZE(mi);

	if (decos == NULL) {
		if ((decos = (decostruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (decostruct))) == NULL)
			return;
	}
	dp = &decos[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2) {
		dp->bordercolor = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
		dp->colorindex = NRAND(MI_NPIXELS(mi));
	} else
		dp->bordercolor = MI_BLACK_PIXEL(mi);
	if (depth < -MINDEPTH)
		dp->max_depth = NRAND(-depth - MINDEPTH + 1) + MINDEPTH;
	else if (depth < MINDEPTH)
		dp->max_depth = MINDEPTH;
	else
		dp->max_depth = depth;
	if (size < -MINSIZE) {
		dp->min_width = NRAND(-size - MINSIZE + 1) + MINSIZE;
		dp->min_height = NRAND(-size - MINSIZE + 1) + MINSIZE;
	} else if (size < MINDEPTH)
		dp->min_width = dp->min_height = MINSIZE;
	else
		dp->min_width = dp->min_height = size;
	dp->time = 0;
}

void
draw_deco(ModeInfo * mi)
{
	decostruct *dp = &decos[MI_SCREEN(mi)];

	if (dp->time == 0) {
		/* This fills up holes */
		MI_CLEARWINDOWCOLOR(mi, dp->bordercolor);
#ifdef SOLARIS2
		/*
		 * if this is not done the first rectangle is sometimes messed up on
		 * Solaris2 with 24 bit TrueColor (Ultra2)
		 */
		XDrawRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			  0, 0, MI_WIDTH(mi) / 2 + 1, MI_HEIGHT(mi) / 2 + 1);
#endif
		deco(mi, 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi), 0);
	}
	if (++dp->time > MI_CYCLES(mi))
		init_deco(mi);

	MI_IS_DRAWN(mi) = True;

}

void
release_deco(ModeInfo * mi)
{
	if (decos != NULL) {
		(void) free((void *) decos);
		decos = NULL;
	}
}

#endif /* MODE_deco */
