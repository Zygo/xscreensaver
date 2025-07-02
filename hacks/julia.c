/* -*- Mode: C; tab-width: 4 -*-
 * julia --- continuously varying Julia set.
 */
#if 0
static const char sccsid[] = "@(#)julia.c	4.03 97/04/10 xlockmore";
#endif

/* Copyright (c) 1995 Sean McCullough <bankshot@mailhost.nmt.edu>.
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
 * 10-Jun-06: j.grahl@ucl.ac.uk: tweaked functions for parameter of Julia set
 * 28-May-97: jwz@jwz.org: added interactive frobbing with the mouse.
 * 10-May-97: jwz@jwz.org: turned into a standalone program.
 * 02-Dec-95: snagged boilerplate from hop.c
 *           used ifs {w0 = sqrt(x-c), w1 = -sqrt(x-c)} with random iteration 
 *           to plot the julia set, and sinusoidially varied parameter for set 
 *	     and plotted parameter with a circle.
 */

/*-
 * One thing to note is that batchcount is the *depth* of the search tree,
 * so the number of points computed is 2^batchcount - 1.  I use 8 or 9
 * on a dx266 and it looks okay.  The sinusoidal variation of the parameter
 * might not be as interesting as it could, but it still gives an idea of
 * the effect of the parameter.
 */

#ifdef STANDALONE
# define DEFAULTS	"*count:		  1000   \n" \
					"*cycles:		  20     \n" \
					"*delay:		  10000  \n" \
					"*ncolors:		  200    \n" \
					"*fpsSolid:		  true   \n" \
					"*ignoreRotation: True   \n" \

# define UNIFORM_COLORS
# define release_julia 0
# define reshape_julia 0
# include "xlockmore.h"				/* in xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* in xlockmore distribution */
#endif /* !STANDALONE */


ENTRYPOINT ModeSpecOpt julia_opts = { 0, };


#define numpoints ((0x2<<jp->depth)-1)

typedef struct {
	int         centerx;
	int         centery;	/* center of the screen */
	double      cr;
	double      ci;		/* julia params */
	int         depth;
	int         inc;
	int         circsize;
	int         erase;
	int         scale;
	int         pix;
	long        itree;
	int         buffer;
	int         nbuffers;
	int         redrawing, redrawpos;
	Pixmap      pixmap;
#ifndef HAVE_JWXYZ
	Cursor      cursor;
#endif
	GC          stippledGC;
	XRectangle **pointBuffer;
    Bool        button_down_p;
    int         mouse_x, mouse_y;

} juliastruct;

static juliastruct *julias = NULL;

/* How many segments to draw per cycle when redrawing */
#define REDRAWSTEP 3

static void
apply(juliastruct * jp, register double xr, register double xi, int d)
{
	double      theta, r;

	jp->pointBuffer[jp->buffer][jp->itree].x =
		(int) (0.5 * xr * jp->centerx + jp->centerx);
	jp->pointBuffer[jp->buffer][jp->itree].y =
		(int) (0.5 * xi * jp->centery + jp->centery);
	jp->itree++;

	if (d > 0) {
		xi -= jp->ci;
		xr -= jp->cr;

/* Avoid atan2: DOMAIN error message */
		if (xi == 0.0 && xr == 0.0)
			theta = 0.0;
		else
			theta = atan2(xi, xr) / 2.0;

		/*r = pow(xi * xi + xr * xr, 0.25); */
		r = sqrt(sqrt(xi * xi + xr * xr));	/* 3 times faster */

		xr = r * cos(theta);
		xi = r * sin(theta);

		d--;
		apply(jp, xr, xi, d);
		apply(jp, -xr, -xi, d);
	}
}

static void
incr(ModeInfo * mi, juliastruct * jp)
{
	if (jp->button_down_p)
	  {
		jp->cr = ((double) (jp->mouse_x + 2 - jp->centerx)) * 2 / jp->centerx;
		jp->ci = ((double) (jp->mouse_y + 2 - jp->centery)) * 2 / jp->centery;
	  }
	else
	  {
#if 0
		jp->cr = 1.5 * (sin(M_PI * (jp->inc / 300.0)) *
						sin(jp->inc * M_PI / 200.0));
		jp->ci = 1.5 * (cos(M_PI * (jp->inc / 300.0)) *
						cos(jp->inc * M_PI / 200.0));

		jp->cr += 0.5 * cos(M_PI * jp->inc / 400.0);
		jp->ci += 0.5 * sin(M_PI * jp->inc / 400.0);
#else
        jp->cr = 1.5 * (sin(M_PI * (jp->inc / 290.0)) *
                        sin(jp->inc * M_PI / 210.0));
        jp->ci = 1.5 * (cos(M_PI * (jp->inc / 310.0)) *
                        cos(jp->inc * M_PI / 190.0));

        jp->cr += 0.5 * cos(M_PI * jp->inc / 395.0);
        jp->ci += 0.5 * sin(M_PI * jp->inc / 410.0);
#endif
	  }
}

ENTRYPOINT void
init_julia(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	juliastruct *jp;
	XGCValues   gcv;
	int         i;

	MI_INIT (mi, julias);
	jp = &julias[MI_SCREEN(mi)];

	jp->centerx = MI_WIN_WIDTH(mi) / 2;
	jp->centery = MI_WIN_HEIGHT(mi) / 2;

	jp->depth = MI_BATCHCOUNT(mi);
	if (jp->depth > 10)
		jp->depth = 10;

    jp->scale = 1;
    if (MI_WIDTH(mi) > 2560 || MI_HEIGHT(mi) > 2560)
      jp->scale *= 3;  /* Retina displays */

#ifndef HAVE_JWXYZ
	if (jp->button_down_p && !jp->cursor && !jp->cursor)
	  {
		Pixmap bit;
		XColor black;
		black.red = black.green = black.blue = 0;
		black.flags = DoRed|DoGreen|DoBlue;
		bit = XCreatePixmapFromBitmapData (display, window, "\000", 1, 1,
										   MI_WIN_BLACK_PIXEL(mi),
										   MI_WIN_BLACK_PIXEL(mi), 1);
		jp->cursor = XCreatePixmapCursor (display, bit, bit, &black, &black,
										  0, 0);
		XFreePixmap (display, bit);
	  }
#endif /* HAVE_JWXYZ */

	if (jp->pixmap != None &&
	    jp->circsize != (MIN(jp->centerx, jp->centery) / 60) * 2 + 1) {
		XFreePixmap(display, jp->pixmap);
		jp->pixmap = None;
	}
	if (jp->pixmap == None) {
		GC          fg_gc = None, bg_gc = None;

		jp->circsize = MAX(8, (MIN(jp->centerx, jp->centery) / 96) * 2 + 1);
		jp->pixmap = XCreatePixmap(display, window, jp->circsize, jp->circsize, 1);
		gcv.foreground = 1;
		fg_gc = XCreateGC(display, jp->pixmap, GCForeground, &gcv);
		gcv.foreground = 0;
		bg_gc = XCreateGC(display, jp->pixmap, GCForeground, &gcv);
		XFillRectangle(display, jp->pixmap, bg_gc,
                       0, 0, jp->circsize, jp->circsize);
		if (jp->circsize < 2)
          XFillRectangle(display, jp->pixmap, fg_gc, 0, 0,
                         jp->scale, jp->scale);
		else
          XFillArc(display, jp->pixmap, fg_gc,
                   0, 0, jp->circsize, jp->circsize, 0, 23040);
		if (fg_gc != None)
			XFreeGC(display, fg_gc);
		if (bg_gc != None)
			XFreeGC(display, bg_gc);
	}

#ifndef HAVE_JWXYZ
	if (MI_WIN_IS_INROOT(mi))
	  ;
	else if (jp->circsize > 0)
	  XDefineCursor (display, window, jp->cursor);
	else
	  XUndefineCursor (display, window);
#endif /* HAVE_JWXYZ */

	if (!jp->stippledGC) {
		gcv.foreground = MI_WIN_BLACK_PIXEL(mi);
		gcv.background = MI_WIN_BLACK_PIXEL(mi);
		if ((jp->stippledGC = XCreateGC(display, window,
				 GCForeground | GCBackground, &gcv)) == None)
			return;
	}
	if (MI_NPIXELS(mi) > 2)
		jp->pix = NRAND(MI_NPIXELS(mi));
	jp->inc = ((LRAND() & 1) * 2 - 1) * NRAND(200);
	jp->nbuffers = (MI_CYCLES(mi) + 1);
	if (!jp->pointBuffer)
		jp->pointBuffer = (XRectangle **)
          calloc(jp->nbuffers, sizeof (*jp->pointBuffer));
	for (i = 0; i < jp->nbuffers; ++i)
		if (jp->pointBuffer[i])
          memset(jp->pointBuffer[i], 0,
                 numpoints * sizeof (**jp->pointBuffer));
		else
			jp->pointBuffer[i] = (XRectangle *)
              calloc(numpoints, sizeof (**jp->pointBuffer));
	jp->buffer = 0;
	jp->redrawing = 0;
	jp->erase = 0;
	XClearWindow(display, window);
}


ENTRYPOINT Bool
julia_handle_event (ModeInfo *mi, XEvent *event)
{
  juliastruct *jp = &julias[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      jp->button_down_p = True;
      jp->mouse_x = event->xbutton.x;
      jp->mouse_y = event->xbutton.y;
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      jp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify && jp->button_down_p)
    {
      jp->mouse_x = event->xmotion.x;
      jp->mouse_y = event->xmotion.y;
      return True;
    }

  return False;
}



/* hack: moved here by jwz. */
#define ERASE_IMAGE(d,w,g,x,y,xl,yl,xs,ys) \
if (yl<y) \
(y<yl+ys)?XFillRectangle(d,w,g,xl,yl,xs,y-yl): \
XFillRectangle(d,w,g,xl,yl,xs,ys); \
else if (yl>y) \
(y>yl-ys)?XFillRectangle(d,w,g,xl,y+ys,xs,yl-y): \
XFillRectangle(d,w,g,xl,yl,xs,ys); \
if (xl<x) \
(x<xl+xs)?XFillRectangle(d,w,g,xl,yl,x-xl,ys): \
XFillRectangle(d,w,g,xl,yl,xs,ys); \
else if (xl>x) \
(x>xl-xs)?XFillRectangle(d,w,g,x+xs,yl,xl-x,ys): \
XFillRectangle(d,w,g,xl,yl,xs,ys)


ENTRYPOINT void
draw_julia (ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	juliastruct *jp = &julias[MI_SCREEN(mi)];
	double      r, theta;
	register double xr = 0.0, xi = 0.0;
	int         k = 64, rnd = 0, i, j;
	XRectangle *xp = jp->pointBuffer[jp->buffer], old_circle, new_circle;

	old_circle.x = (int) (jp->centerx * jp->cr / 2) + jp->centerx - 2;
	old_circle.y = (int) (jp->centery * jp->ci / 2) + jp->centery - 2;
	old_circle.width = old_circle.height = jp->scale;
	incr(mi, jp);
	new_circle.x = (int) (jp->centerx * jp->cr / 2) + jp->centerx - 2;
	new_circle.y = (int) (jp->centery * jp->ci / 2) + jp->centery - 2;
	new_circle.width = new_circle.height = jp->scale;
	XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
	XFillArc(display, window, gc, 
             old_circle.x-jp->circsize/2-2,
             old_circle.y-jp->circsize/2-2,
             jp->circsize+4, jp->circsize+4,
             0, 360*64);
	/* draw a circle at the c-parameter so you can see it's effect on the
	   structure of the julia set */
	XSetForeground(display, jp->stippledGC, MI_WIN_WHITE_PIXEL(mi));
#ifndef HAVE_JWXYZ
	XSetTSOrigin(display, jp->stippledGC, new_circle.x, new_circle.y);
	XSetStipple(display, jp->stippledGC, jp->pixmap);
	XSetFillStyle(display, jp->stippledGC, FillOpaqueStippled);
#endif /* HAVE_JWXYZ */
	XDrawArc(display, window, jp->stippledGC, 
             new_circle.x-jp->circsize/2,
             new_circle.y-jp->circsize/2,
             jp->circsize, jp->circsize,
             0, 360*64);

	if (jp->erase == 1) {
		XFillRectangles(display, window, gc,
                        jp->pointBuffer[jp->buffer], numpoints);
	}
	jp->inc++;
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, jp->pix));
		if (++jp->pix >= MI_NPIXELS(mi))
			jp->pix = 0;
	} else
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	while (k--) {

		/* save calls to LRAND by using bit shifts over and over on the same
		   int for 32 iterations, then get a new random int */
		if (!(k % 32))
			rnd = LRAND();

		/* complex sqrt: x^0.5 = radius^0.5*(cos(theta/2) + i*sin(theta/2)) */

		xi -= jp->ci;
		xr -= jp->cr;

		/* Avoid atan2: DOMAIN error message */
		if (xi == 0.0 && xr == 0.0)
			theta = 0.0;
		else
			theta = atan2(xi, xr) / 2.0;

		/*r = pow(xi * xi + xr * xr, 0.25); */
		r = sqrt(sqrt(xi * xi + xr * xr));	/* 3 times faster */

		xr = r * cos(theta);
		xi = r * sin(theta);

		if ((rnd >> (k % 32)) & 0x1) {
			xi = -xi;
			xr = -xr;
		}
		xp->x = jp->centerx + (int) ((jp->centerx >> 1) * xr);
		xp->y = jp->centery + (int) ((jp->centery >> 1) * xi);
        xp->width = xp->height = jp->scale;
		xp++;
	}

	jp->itree = 0;
	apply(jp, xr, xi, jp->depth);

	XFillRectangles (display, window, gc,
                     jp->pointBuffer[jp->buffer], numpoints);

	jp->buffer++;
	if (jp->buffer > jp->nbuffers - 1) {
		jp->buffer -= jp->nbuffers;
		jp->erase = 1;
	}
	if (jp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			j = (jp->buffer - jp->redrawpos + jp->nbuffers) % jp->nbuffers;
			XFillRectangles (display, window, gc,
                             jp->pointBuffer[j], numpoints);

			if (++(jp->redrawpos) >= jp->nbuffers) {
				jp->redrawing = 0;
				break;
			}
		}
	}
}

ENTRYPOINT void
free_julia (ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	juliastruct *jp = &julias[MI_SCREEN(mi)];
	int         buffer;

	if (jp->pointBuffer) {
		for (buffer = 0; buffer < jp->nbuffers; buffer++)
			if (jp->pointBuffer[buffer])
				(void) free((void *) jp->pointBuffer[buffer]);
		(void) free((void *) jp->pointBuffer);
	}
	if (jp->stippledGC != None)
		XFreeGC(display, jp->stippledGC);
	if (jp->pixmap != None)
		XFreePixmap(display, jp->pixmap);
#ifndef HAVE_JWXYZ
	if (jp->cursor)
	  XFreeCursor (display, jp->cursor);
#endif
}

#ifndef STANDALONE
ENTRYPOINT void
refresh_julia (ModeInfo * mi)
{
	juliastruct *jp = &julias[MI_SCREEN(mi)];

	jp->redrawing = 1;
	jp->redrawpos = 0;
}
#endif

XSCREENSAVER_MODULE ("Julia", julia)
