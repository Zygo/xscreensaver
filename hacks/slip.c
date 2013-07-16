/* -*- Mode: C; tab-width: 4 -*- */
/* slip --- lots of slipping blits */

#if 0
static const char sccsid[] = "@(#)slip.c	5.00 2000/11/01 xlockmore";
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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Jamie Zawinski <jwz@jwz.org> compatible with xscreensaver
 * 01-Dec-1995: Patched for VMS <joukj@hrem.stm.tudelft.nl>
 */

#ifdef STANDALONE
# define MODE_slip
# define DEFAULTS	"*delay: 50000 \n" \
					"*count: 35 \n" \
					"*cycles: 50 \n" \
					"*ncolors: 200 \n" \
					"*fpsSolid:	true \n" \
					"*ignoreRotation: True \n" \

# define refresh_slip 0
# define slip_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_slip

ENTRYPOINT ModeSpecOpt slip_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   slip_description =
{"slip", "init_slip", "draw_slip", "release_slip",
 "init_slip", "init_slip", (char *) NULL, &slip_opts,
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
	short       lasthalf;
	int         stage;
	unsigned long r;
    Bool image_loading_p;
} slipstruct;
static slipstruct *slips = (slipstruct *) NULL;

static short
halfrandom(slipstruct *sp, int mv)
{
	unsigned long r;

	if (sp->lasthalf) {
		r = sp->lasthalf;
		sp->lasthalf = 0;
	} else {
		r = LRAND();
		sp->lasthalf = (short) (r >> 16);
	}
	return r % mv;
}

static int
erandom(slipstruct *sp, int mv)
{
	int         res;

	if (0 == sp->stage) {
		sp->r = LRAND();
		sp->stage = 7;
	}
	res = (int) (sp->r & 0xf);
	sp->r = sp->r >> 4;
	sp->stage--;
	if (res & 8)
		return res & mv;
	else
		return -(res & mv);
}

#ifdef STANDALONE
static void
image_loaded_cb (Screen *screen, Window w, Drawable d,
                 const char *name, XRectangle *geom,
                 void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;
  slipstruct *sp = &slips[MI_SCREEN(mi)];
  Display *dpy = DisplayOfScreen (screen);
  XCopyArea (dpy, d, w, mi->gc, 0, 0,
             sp->width, sp->height, 0, 0);
  XFreePixmap (dpy, d);
  sp->image_loading_p = False;
}
#endif /* STANDALONE */

static void
prepare_screen(ModeInfo * mi, slipstruct * sp)
{

	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	int         i, n, w = sp->width / 20;
	int         not_solid = halfrandom(sp, 10);

	sp->backwards = (int) (LRAND() & 1);	/* jwz: go the other way sometimes */

	if (sp->first_time || !halfrandom(sp, 10)) {
		MI_CLEARWINDOW(mi);
		n = 300;
	} else {
		if (halfrandom(sp, 5))
			return;
		if (halfrandom(sp, 5))
			n = 100;
		else
			n = 2000;
	}

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, halfrandom(sp, MI_NPIXELS(mi))));
	else if (halfrandom(sp, 2))
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

	for (i = 0; i < n; i++) {
		int         ww = ((w / 2) + halfrandom(sp, MAX(w, 1)));

		if (not_solid) {
			if (MI_NPIXELS(mi) > 2)
				XSetForeground(display, gc, MI_PIXEL(mi, halfrandom(sp, MI_NPIXELS(mi))));
			else if (halfrandom(sp, 2))
				XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
			else
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		}
		XFillRectangle(display, MI_WINDOW(mi), gc,
			       halfrandom(sp, MAX(sp->width - ww, 1)),
			       halfrandom(sp, MAX(sp->height - ww, 1)),
			       ww, ww);
	}
	sp->first_time = 0;


#ifdef STANDALONE			  /* jwz -- sometimes hack the desktop image! */
	if (!sp->image_loading_p &&
        (1||halfrandom(sp, 2) == 0)) {
      /* Load it into a pixmap so that the "Loading" message and checkerboard
         don't show up on the window -- we keep running while the image is
         in progress... */
      Pixmap p = XCreatePixmap (MI_DISPLAY(mi), MI_WINDOW(mi),
                                sp->width, sp->height, mi->xgwa.depth);
      sp->image_loading_p = True;
      load_image_async (ScreenOfDisplay (MI_DISPLAY(mi), 0/*####MI_SCREEN(mi)*/),
                        MI_WINDOW(mi), p, image_loaded_cb, mi);
	}
#endif
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

ENTRYPOINT void
reshape_slip (ModeInfo * mi, int w, int h)
{
  slipstruct *sp = &slips[MI_SCREEN(mi)];
  sp->width = w;
  sp->height = h;
  sp->blit_width = sp->width / 25;
  sp->blit_height = sp->height / 25;

  sp->mode = 0;
  sp->nblits_remaining = 0;
}

ENTRYPOINT void
init_slip (ModeInfo * mi)
{
	slipstruct *sp;

	if (slips == NULL) {
		if ((slips = (slipstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (slipstruct))) == NULL)
			return;
	}
	sp = &slips[MI_SCREEN(mi)];

	sp->nblits_remaining = 0;
	sp->mode = 0;
	sp->first_time = 1;

	/* no "NoExpose" events from XCopyArea wanted */
	XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);

    reshape_slip (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}

ENTRYPOINT void
draw_slip (ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         timer;
	slipstruct *sp;

	if (slips == NULL)
		return;
	sp = &slips[MI_SCREEN(mi)];

	timer = MI_COUNT(mi) * MI_CYCLES(mi);

	MI_IS_DRAWN(mi) = True;

	while (timer--) {
		int         xi = halfrandom(sp, MAX(sp->width - sp->blit_width, 1));
		int         yi = halfrandom(sp, MAX(sp->height - sp->blit_height, 1));
		double      x, y, dx = 0, dy = 0, t, s1, s2;

		if (0 == sp->nblits_remaining--) {
			static const int lut[] = {0, 0, 0, 1, 1, 1, 2};

			prepare_screen(mi, sp);
			sp->nblits_remaining = MI_COUNT(mi) *
				(2000 + halfrandom(sp, 1000) + halfrandom(sp, 1000));
			if (sp->mode == 2)
				sp->mode = halfrandom(sp, 2);
			else
				sp->mode = lut[halfrandom(sp, 7)];
		}
		x = (2 * xi + sp->blit_width) / (double) sp->width - 1;
		y = (2 * yi + sp->blit_height) / (double) sp->height - 1;

		/* (x,y) is in biunit square */
		switch (sp->mode) {
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

				if (sp->backwards) {	/* jwz: go the other way sometimes */
					dx = -dx;
					dy = -dy;
				}
				break;
			case 1:	/* shuffle */
				dx = erandom(sp, 3);
				dy = erandom(sp, 3);
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
			    qx >= sp->width - sp->blit_width ||
			    qy >= sp->height - sp->blit_height)
				continue;

/*-
Seems to cause problems using Exceed
with PseudoColor
X Error of failed request:  BadGC (invalid GC parameter)
with TrueColor
X Error of failed request:  BadDrawable (invalid Pixmap or Window parameter)
  Major opcode of failed request:  62 (X_CopyArea)
 */
			XCopyArea(display, window, window, gc, xi, yi,
				  sp->blit_width, sp->blit_height,
				  qx, qy);
			switch (sp->mode) {
				case 0:
					/* wrap */
					wrap = sp->width - (2 * sp->blit_width);
					if (qx > wrap ) {
						XCopyArea(display, window, window, gc, qx, qy,
						sp->blit_width, sp->blit_height,
							  qx - wrap, qy);
					}
					if (qx < 2 * sp->blit_width) {
						XCopyArea(display, window, window, gc, qx, qy,
						sp->blit_width, sp->blit_height,
							  qx + wrap, qy);
					}
					wrap = sp->height - (2 * sp->blit_height);
					if (qy > wrap) {
						XCopyArea(display, window, window, gc, qx, qy,
						sp->blit_width, sp->blit_height,
							  qx, qy - wrap);
					}
					if (qy < 2 * sp->blit_height) {
						XCopyArea(display, window, window, gc, qx, qy,
						sp->blit_width, sp->blit_height,
							  qx, qy + wrap);
					}
					break;
				case 1:
				case 2:
					break;
			}
		}
	}
}

ENTRYPOINT void
release_slip (ModeInfo * mi)
{
	if (slips != NULL) {
		(void) free((void *) slips);
		slips = (slipstruct *) NULL;
	}
}

XSCREENSAVER_MODULE ("Slip", slip)

#endif /* MODE_slip */
