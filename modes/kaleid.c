/* -*- Mode: C; tab-width: 4 -*- */
/* kaleid --- kaleidoscope */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)kaleid.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Based on a kaleidoscope algorithm from a PC-based program by:
 * Judson D. McClendon (Compuserve: [74415,1003])
 *
 * KALEIDOSCOPE (X11 Version)
 * By Nathan Meyers, nathanm@hp-pcd.hp.com.
 *
 * Modified by Laurent JULLIARD, laurentj@hpgnse2.grenoble.hp.com
 * for LINUX 0.96 and X11 v1.0 shared library
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
 * 23-Oct-94: Ported to xlock by David Bagley <bagleyd@bigfoot.com>
 */

#ifdef STANDALONE
#define PROGCLASS "Kaleid"
#define HACK_INIT init_kaleid
#define HACK_DRAW draw_kaleid
#define kaleid_opts xlockmore_opts
#define DEFAULTS "*delay: 20000 \n" \
 "*cycles: 700 \n" \
 "*ncolors: 200 \n"
#define UNIFORM_COLORS
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt kaleid_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   kaleid_description =
{"kaleid", "init_kaleid", "draw_kaleid", "release_kaleid",
 "refresh_kaleid", "init_kaleid", NULL, &kaleid_opts,
 20000, 1, 700, 1, 64, 1.0, "",
 "Shows a kaleidoscope", 0, NULL};

#endif

#define INTRAND(min,max) (NRAND(((max+1)-(min)))+(min))

typedef struct {
	int         x;
	int         y;
} point;

typedef struct {
	int         pix;
	int         cx, cy, m;
	int         ox, oy;
	int         x1, y1, x2, y2, xv1, yv1, xv2, yv2;
	int         xa, ya, xb, yb, xc, yc, xd, yd;
	int         width;
	int         height;
	int         itercount;
} kaleidstruct;

static kaleidstruct *kaleids = NULL;

void
init_kaleid(ModeInfo * mi)
{
	kaleidstruct *kp = &kaleids[MI_SCREEN(mi)];

	if (kaleids == NULL) {
		if ((kaleids = (kaleidstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (kaleidstruct))) == NULL)
			return;
	}
	kp = &kaleids[MI_SCREEN(mi)];

	kp->width = MI_WIDTH(mi);
	kp->height = MI_HEIGHT(mi);
	if (kp->width < 2)
		kp->width = 2;
	if (kp->height < 2)
		kp->height = 2;
	if (MI_NPIXELS(mi) > 2)
		kp->pix = NRAND(MI_NPIXELS(mi));
	kp->cx = kp->width / 2;
	kp->cy = kp->height / 2;
	kp->m = (kp->cx > kp->cy) ? kp->cx : kp->cy;
	kp->m = kp->m ? kp->m : 1;
	kp->x1 = NRAND(kp->m) + 1;
	kp->x2 = NRAND(kp->m) + 1;
	kp->y1 = NRAND(kp->x1) + 1;
	kp->y2 = NRAND(kp->x2) + 1;
	kp->xv1 = NRAND(7) - 3;
	kp->xv2 = NRAND(7) - 3;
	kp->yv1 = NRAND(7) - 3;
	kp->yv2 = NRAND(7) - 3;
	kp->itercount = 0;

	MI_CLEARWINDOW(mi);
}

void
draw_kaleid(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	kaleidstruct *kp = &kaleids[MI_SCREEN(mi)];
	XSegment    segs[8];

	if (!(NRAND(50))) {
		kp->x1 = NRAND(kp->m) + 1;
		kp->x2 = NRAND(kp->m) + 1;
		kp->y1 = NRAND(kp->x1);
		kp->y2 = NRAND(kp->x2);
	}
	if (!(NRAND(10))) {
		kp->xv1 = NRAND(7) - 3;
		kp->xv2 = NRAND(7) - 3;
		kp->yv1 = NRAND(7) - 3;
		kp->yv2 = NRAND(7) - 3;
		if (MI_NPIXELS(mi) > 2) {
			XSetForeground(display, gc, MI_PIXEL(mi, kp->pix));
			if (++kp->pix >= MI_NPIXELS(mi))
				kp->pix = 0;
		}
	}
	if (kp->cx < kp->cy) {
		kp->xa = kp->x1 * kp->cx / kp->cy;
		kp->ya = kp->y1 * kp->cx / kp->cy;
		kp->xb = kp->x1;
		kp->yb = kp->y1;
		kp->xc = kp->x2 * kp->cx / kp->cy;
		kp->yc = kp->y2 * kp->cx / kp->cy;
		kp->xd = kp->x2;
		kp->yd = kp->y2;
	} else {
		kp->xa = kp->x1;
		kp->ya = kp->y1;
		kp->xb = kp->x1 * kp->cy / kp->cx;
		kp->yb = kp->y1 * kp->cy / kp->cx;
		kp->xc = kp->x2;
		kp->yc = kp->y2;
		kp->xd = kp->x2 * kp->cy / kp->cx;
		kp->yd = kp->y2 * kp->cy / kp->cx;
	}
	segs[0].x1 = kp->cx + kp->xa + kp->ox;
	segs[0].y1 = kp->cy - kp->yb + kp->oy;
	segs[0].x2 = kp->cx + kp->xc + kp->ox;
	segs[0].y2 = kp->cy - kp->yd + kp->oy;
	segs[1].x1 = kp->cx - kp->ya + kp->ox;
	segs[1].y1 = kp->cy + kp->xb + kp->oy;
	segs[1].x2 = kp->cx - kp->yc + kp->ox;
	segs[1].y2 = kp->cy + kp->xd + kp->oy;
	segs[2].x1 = kp->cx - kp->xa + kp->ox;
	segs[2].y1 = kp->cy - kp->yb + kp->oy;
	segs[2].x2 = kp->cx - kp->xc + kp->ox;
	segs[2].y2 = kp->cy - kp->yd + kp->oy;
	segs[3].x1 = kp->cx - kp->ya + kp->ox;
	segs[3].y1 = kp->cy - kp->xb + kp->oy;
	segs[3].x2 = kp->cx - kp->yc + kp->ox;
	segs[3].y2 = kp->cy - kp->xd + kp->oy;
	segs[4].x1 = kp->cx - kp->xa + kp->ox;
	segs[4].y1 = kp->cy + kp->yb + kp->oy;
	segs[4].x2 = kp->cx - kp->xc + kp->ox;
	segs[4].y2 = kp->cy + kp->yd + kp->oy;
	segs[5].x1 = kp->cx + kp->ya + kp->ox;
	segs[5].y1 = kp->cy - kp->xb + kp->oy;
	segs[5].x2 = kp->cx + kp->yc + kp->ox;
	segs[5].y2 = kp->cy - kp->xd + kp->oy;
	segs[6].x1 = kp->cx + kp->xa + kp->ox;
	segs[6].y1 = kp->cy + kp->yb + kp->oy;
	segs[6].x2 = kp->cx + kp->xc + kp->ox;
	segs[6].y2 = kp->cy + kp->yd + kp->oy;
	segs[7].x1 = kp->cx + kp->ya + kp->ox;
	segs[7].y1 = kp->cy + kp->xb + kp->oy;
	segs[7].x2 = kp->cx + kp->yc + kp->ox;
	segs[7].y2 = kp->cy + kp->xd + kp->oy;
	XDrawSegments(display, MI_WINDOW(mi), gc, segs, 8);
	kp->x1 = (kp->x1 + kp->xv1) % kp->m;
	kp->y1 = (kp->y1 + kp->yv1) % kp->m;
	kp->x2 = (kp->x2 + kp->xv2) % kp->m;
	kp->y2 = (kp->y2 + kp->yv2) % kp->m;
	kp->itercount++;

	if (kp->itercount > MI_CYCLES(mi)) {

		MI_CLEARWINDOW(mi);

		kp->itercount = 0;
	}
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, kp->pix));
		if (++kp->pix >= MI_NPIXELS(mi))
			kp->pix = 0;
	} else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
}

void
release_kaleid(ModeInfo * mi)
{
	if (kaleids != NULL) {
		(void) free((void *) kaleids);
		kaleids = NULL;
	}
}

void
refresh_kaleid(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
