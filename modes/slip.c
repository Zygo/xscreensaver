/* -*- Mode: C; tab-width: 4 -*- */
/* slip --- lots of slipping blits */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)slip.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1992 by Scott Draves <spot@cs.cmu.edu>
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
 * 10-May-97: Jamie Zawinski <jwz@jwz.org> compatible with xscreensaver
 * 01-Dec-95: Patched for VMS <joukj@hrem.stm.tudelft.nl>
 */

#ifdef STANDALONE
#define PROGCLASS "Slip"
#define HACK_INIT init_slip
#define HACK_DRAW draw_slip
#define slip_opts xlockmore_opts
#define DEFAULTS "*delay: 50000 \n" \
 "*count: 35 \n" \
 "*cycles: 50 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_slip

ModeSpecOpt slip_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   slip_description =
{"slip", "init_slip", "draw_slip", "release_slip",
 "init_slip", "init_slip", NULL, &slip_opts,
 50000, 35, 50, 1, 64, 1.0, "",
 "Shows slipping blits", 0, NULL};

#endif

typedef struct {
	int         width, height;
	int         nblits_remaining;
	int         blit_width, blit_height;
	int         mode;
	int         first_time;
	int         backwards;
} slipstruct;
static slipstruct *slips = NULL;

static short
halfrandom(int mv)
{
	static short lasthalf = 0;
	unsigned long r;

	if (lasthalf) {
		r = lasthalf;
		lasthalf = 0;
	} else {
		r = LRAND();
		lasthalf = (short) (r >> 16);
	}
	return r % mv;
}

static int
erandom(int mv)
{
	static int  stage = 0;
	static unsigned long r;
	int         res;

	if (0 == stage) {
		r = LRAND();
		stage = 7;
	}
	res = (int) (r & 0xf);
	r = r >> 4;
	stage--;
	if (res & 8)
		return res & mv;
	else
		return -(res & mv);
}

static void
prepare_screen(ModeInfo * mi, slipstruct * s)
{

	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	int         i, n, w = s->width / 20;
	int         not_solid = halfrandom(10);

#ifdef STANDALONE		/* jwz -- sometimes hack the desktop image! */
	if (halfrandom(2) == 0) {
#if 0
		Pixmap      p =
#endif
		grab_screen_image(DefaultScreenOfDisplay(display),
				  MI_WINDOW(mi));

#if 0
		if (p)
			XFreePixmap(display, p);
		return;
#endif
	}
#endif

	s->backwards = (int) (LRAND() & 1);	/* jwz: go the other way sometimes */

	if (s->first_time || !halfrandom(10)) {
		MI_CLEARWINDOW(mi);
		n = 300;
	} else {
		if (halfrandom(5))
			return;
		if (halfrandom(5))
			n = 100;
		else
			n = 2000;
	}

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, halfrandom(MI_NPIXELS(mi))));
	else if (halfrandom(2))
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

	for (i = 0; i < n; i++) {
		int         ww = ((w / 2) + halfrandom(w));

		if (not_solid) {
			if (MI_NPIXELS(mi) > 2)
				XSetForeground(display, gc, MI_PIXEL(mi, halfrandom(MI_NPIXELS(mi))));
			else if (halfrandom(2))
				XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
			else
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		}
		XFillRectangle(display, MI_WINDOW(mi), gc,
			       halfrandom(s->width - ww),
			       halfrandom(s->height - ww),
			       ww, ww);
	}
	s->first_time = 0;
}

static int
quantize(double d)
{
	int         i = (int) floor(d);
	double      f = d - i;

	if ((LRAND() & 0xff) < f * 0xff)
		i++;
	return i;
}

void
init_slip(ModeInfo * mi)
{
	slipstruct *sp;

	if (slips == NULL) {
		if ((slips = (slipstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (slipstruct))) == NULL)
			return;
	}
	sp = &slips[MI_SCREEN(mi)];

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

	sp->blit_width = sp->width / 25;
	sp->blit_height = sp->height / 25;
	sp->nblits_remaining = 0;
	sp->mode = 0;
	sp->first_time = 1;

	/* no "NoExpose" events from XCopyArea wanted */
	XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);
}

void
draw_slip(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	slipstruct *s = &slips[MI_SCREEN(mi)];
	int         timer;

	timer = MI_COUNT(mi) * MI_CYCLES(mi);

	MI_IS_DRAWN(mi) = True;

	while (timer--) {
		int         xi = halfrandom(s->width - s->blit_width);
		int         yi = halfrandom(s->height - s->blit_height);
		double      x, y, dx = 0, dy = 0, t, s1, s2;

		if (0 == s->nblits_remaining--) {
			static int  lut[] =
			{0, 0, 0, 1, 1, 1, 2};

			prepare_screen(mi, s);
			s->nblits_remaining = MI_COUNT(mi) *
				(2000 + halfrandom(1000) + halfrandom(1000));
			if (s->mode == 2)
				s->mode = halfrandom(2);
			else
				s->mode = lut[halfrandom(7)];
		}
		x = (2 * xi + s->blit_width) / (double) s->width - 1;
		y = (2 * yi + s->blit_height) / (double) s->height - 1;

		/* (x,y) is in biunit square */
		switch (s->mode) {
			case 0:	/* rotor */
				dx = x;
				dy = y;

				if (dy < 0) {
					dy += 0.04;
					if (dy > 0)
						dy = 0.00;
				}
				if (dy > 0) {
					dy -= 0.04;
					if (dy < 0)
						dy = 0.00;
				}
				t = dx * dx + dy * dy + 1e-10;
				s1 = 2 * dx * dx / t - 1;
				s2 = 2 * dx * dy / t;
				dx = s1 * 5;
				dy = s2 * 5;

				if (s->backwards) {	/* jwz: go the other way sometimes */
					dx = -dx;
					dy = -dy;
				}
				break;
			case 1:	/* shuffle */
				dx = erandom(3);
				dy = erandom(3);
				break;
			case 2:	/* explode */
				dx = x * 3;
				dy = y * 3;
				break;
		}
		{
			int         qx = xi + quantize(dx), qy = yi + quantize(dy);
			int         wrap;

			if (qx < 0 || qy < 0 ||
			    qx >= s->width - s->blit_width ||
			    qy >= s->height - s->blit_height)
				continue;

			XCopyArea(display, window, window, gc, xi, yi,
				  s->blit_width, s->blit_height,
				  qx, qy);

			switch (s->mode) {
				case 0:
					/* wrap */
					wrap = s->width - (2 * s->blit_width);
					if (qx > wrap)
						XCopyArea(display, window, window, gc, qx, qy,
						s->blit_width, s->blit_height,
							  qx - wrap, qy);

					if (qx < 2 * s->blit_width)
						XCopyArea(display, window, window, gc, qx, qy,
						s->blit_width, s->blit_height,
							  qx + wrap, qy);

					wrap = s->height - (2 * s->blit_height);
					if (qy > wrap)
						XCopyArea(display, window, window, gc, qx, qy,
						s->blit_width, s->blit_height,
							  qx, qy - wrap);

					if (qy < 2 * s->blit_height)
						XCopyArea(display, window, window, gc, qx, qy,
						s->blit_width, s->blit_height,
							  qx, qy + wrap);
					break;
				case 1:
				case 2:
					break;
			}
		}
	}
}

void
release_slip(ModeInfo * mi)
{
	if (slips != NULL) {
		(void) free((void *) slips);
		slips = NULL;
	}
}

#endif /* MODE_slip */
