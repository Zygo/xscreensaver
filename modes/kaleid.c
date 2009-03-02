/* -*- Mode: C; tab-width: 4 -*- */
/* kaleid --- Brewster's Kaleidoscope */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)kaleid.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 *
 *  kaleid.c - Brewster's Kaleidoscope  (Sir David Brewster invented the
 *     kaleidoscope in 1816 and patented it in 1817.)
 *
 *  Copyright (c) 1998 by Robert Adam, II  <raii@comm.net>
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
 * 1998: Written
 *
 *
 *  -batchcount n     number of pens [default 4]
 *
 *  -cycle n          percentage of black in the pattern (0%-95%)
 *
 *  -size n          symmetry mode [default -9]
 *                         <0     = random from 0 to -number
 *                         >0     = mirrored                  (-alternate)
 *                                  rotated                   (+alternate)
 *
 *
 *  -/+disconnected   turn on/off disconnected pen movement
 *
 *  -/+serial         turn on/off sequential allocation of colors
 *
 *  -/+alternate      turn on/off alternate display mode
 *
 *  -/+spiral         turn on/off spiral mode
 *
 *  -/+spots          turn on/off spots mode
 *
 *  -/+quad           turn on/off quad mirrored/rotated mode similar
 *                       to size 4, works with alternate
 *
 *  -/+oct            turn on/off oct mirrored/rotated moded similar
 *                       to size 8, works with alternate
 *
 *  -/+linear         select Cartesian/Polar coordinate mode. Cartesian
 *                       uses straight lines similar to -oct and -quad
 *                       mode instead of the curved lines of Polar mode.
 *                       [default off]
 *
 */

#include <math.h>

#ifdef STANDALONE
#define PROGCLASS "Kaleid"
#define HACK_INIT init_kaleid
#define HACK_DRAW draw_kaleid
#define kaleid_opts xlockmore_opts
#define DEFAULTS "*delay: 80000 \n" \
 "*count: 4 \n" \
 "*cycles: 40 \n" \
 "*size: -9 \n" \
 "*ncolors: 200 \n" \
 "*disconnected: True \n" \
 "*serial: False \n" \
 "*alternate: False \n" \
 "*spiral: False \n" \
 "*spots: False \n" \
 "*linear: False \n" \
 "*quad: False \n" \
 "*oct: False \n"
"*fullrandom: False \n"
#define UNIFORM_COLORS
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_kaleid

#define DEF_DISCONNECTED  "True"
#define DEF_SERIAL        "False"
#define DEF_ALTERNATE     "False"
#define DEF_QUAD          "False"
#define DEF_OCT           "False"
#define DEF_LINEAR        "False"
#define DEF_SPIRAL        "False"
#define DEF_SPOTS         "False"

static Bool Disconnected;
static Bool Serial;
static Bool Alternate;
static Bool Quad;
static Bool Oct;
static Bool Linear;
static Bool Spiral;
static Bool Spots;

static XrmOptionDescRec opts[] =
{
   {(char *) "-disconnected", (char *) ".kaleid.disconnected", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+disconnected", (char *) ".kaleid.disconnected", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-serial", (char *) ".kaleid.serial", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+serial", (char *) ".kaleid.serial", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-alternate", (char *) ".kaleid.alternate", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+alternate", (char *) ".kaleid.alternate", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-spiral", (char *) ".kaleid.spiral", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+spiral", (char *) ".kaleid.spiral", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-spots", (char *) ".kaleid.spots", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+spots", (char *) ".kaleid.spots", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-quad", (char *) ".kaleid.quad", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+quad", (char *) ".kaleid.quad", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-oct", (char *) ".kaleid.oct", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+oct", (char *) ".kaleid.oct", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-linear", (char *) ".kaleid.linear", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+linear", (char *) ".kaleid.linear", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & Disconnected, (char *) "disconnected", (char *) "Disconnected", (char *) DEF_DISCONNECTED, t_Bool},
	{(caddr_t *) & Serial, (char *) "serial", (char *) "Serial", (char *) DEF_SERIAL, t_Bool},
    {(caddr_t *) & Alternate, (char *) "alternate", (char *) "Alternate", (char *) DEF_ALTERNATE, t_Bool},
    {(caddr_t *) & Spiral, (char *) "spiral", (char *) "Spiral", (char *) DEF_SPIRAL, t_Bool},
    {(caddr_t *) & Spots, (char *) "spots", (char *) "Spots", (char *) DEF_SPOTS, t_Bool},
	{(caddr_t *) & Quad, (char *) "quad", (char *) "Quad", (char *) DEF_QUAD, t_Bool},
	{(caddr_t *) & Oct, (char *) "oct", (char *) "Oct", (char *) DEF_OCT, t_Bool},
	{(caddr_t *) & Linear, (char *) "linear", (char *) "Linear", (char *) DEF_LINEAR, t_Bool}
};

static OptionStruct desc[] =
{
	{(char *) "-/+disconnected", (char *) "turn on/off disconnected pen movement"},
	{(char *) "-/+serial", (char *) "turn on/off sequential color selection"},
	{(char *) "-/+alternate", (char *) "turn on/off alternate display mode"},
	{(char *) "-/+spiral", (char *) "turn on/off angular bounding mode"},
	{(char *) "-/+spots", (char *) "turn on/off circle mode"},
	{(char *) "-/+quad", (char *) "turn on/off quad mirrored display mode"},
	{(char *) "-/+oct", (char *) "turn on/off oct mirrored display mode"},
	{(char *) "-/+linear", (char *) "select Cartesian/Polar coordinate display mode"}
};

ModeSpecOpt kaleid_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   kaleid_description =
{
	"kaleid", "init_kaleid", "draw_kaleid", "release_kaleid",
	"refresh_kaleid", "init_kaleid", NULL, &kaleid_opts,
	80000, 4, 40, -9, 64, 0.6, "",
	"Shows Brewster's Kaleidoscope", 0, NULL
};

#endif

#define INTRAND(min,max) (NRAND(((max+1)-(min)))+(min))

#define MINPENS 1
#define MINSIZE 1
#define QUAD (-4)
#define OCT (-8)

#define SpotsMult           3

#define MinVelocity         6
#define MaxVelocity         4

#define MinRadialVelocity   6
#define MaxRadialVelocity   30

#define MinAngularVelocity  5
#define MaxAngularVelocity  3

#define WidthPercent  25
#define ChangeChance  2

#define ToRadians 0.017453293
#define ToDegrees 57.29577951
#define PolarToCartX(r, t) ((r) * cos((t) * ToRadians))
#define PolarToCartY(r, t) ((r) * sin((t) * ToRadians))
#define CartToRadius(x, y) (sqrt(((x)*(x)) + ((y)*(y))))
#define CartToAngle(x,y)   ((((x) == 0.0) && ((y) == 0.0)) ? 0.0 : (atan2((y),(x)) * ToDegrees))

typedef struct {
	double      cx;
	double      cy;
	double      ox;
	double      oy;
	double      xv;
	double      yv;
	int         curlwidth;
	int         pix;
	int         White;
	Bool        RadiusOut;
	Bool        AngleOut;
	Bool        DeferredChange;
} penstruct;

typedef struct {
	penstruct  *pen;


	int         PercentBlack;
	int         PenCount;
	int         maxlwidth;
	double      width, widthby2;
	double      height, heightby2;
	double      radius;
	double      slice;
	int         bouncetype;
	int         modetype;
	Bool        alternate, disconnected, serial, linear, spiral, spots;
} kaleidstruct;

static kaleidstruct *kaleids = NULL;

/*-
 *
 */
static void
QuadMirrored(
		    kaleidstruct * kp,
		    int pn,
		    XSegment segs[4]
)
{
	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	if (kp->pen[pn].cx < 0.0) {
		kp->pen[pn].cx = -kp->pen[pn].cx;
		kp->pen[pn].xv = -kp->pen[pn].xv;
	} else if (kp->pen[pn].cx >= kp->widthby2) {
		kp->pen[pn].cx = (kp->widthby2 - 1.0)
			- (kp->pen[pn].cx - kp->widthby2);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	}
	kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;
	if (kp->pen[pn].cy < 0.0) {
		kp->pen[pn].cy = -kp->pen[pn].cy;
		kp->pen[pn].yv = -kp->pen[pn].yv;
	} else if (kp->pen[pn].cy >= kp->heightby2) {
		kp->pen[pn].cy = (kp->heightby2 - 1.0)
			- (kp->pen[pn].cy - kp->heightby2);
		kp->pen[pn].yv = -kp->pen[pn].yv;
	}
	segs[0].x1 = (int) kp->pen[pn].ox;
	segs[0].y1 = (int) kp->pen[pn].oy;
	segs[0].x2 = (int) kp->pen[pn].cx;
	segs[0].y2 = (int) kp->pen[pn].cy;

	segs[1].x1 = (int) (kp->width - kp->pen[pn].ox);
	segs[1].y1 = (int) kp->pen[pn].oy;
	segs[1].x2 = (int) (kp->width - kp->pen[pn].cx);
	segs[1].y2 = (int) kp->pen[pn].cy;

	segs[2].x1 = (int) (kp->width - kp->pen[pn].ox);
	segs[2].y1 = (int) (kp->height - kp->pen[pn].oy);
	segs[2].x2 = (int) (kp->width - kp->pen[pn].cx);
	segs[2].y2 = (int) (kp->height - kp->pen[pn].cy);

	segs[3].x1 = (int) kp->pen[pn].ox;
	segs[3].y1 = (int) (kp->height - kp->pen[pn].oy);
	segs[3].x2 = (int) kp->pen[pn].cx;
	segs[3].y2 = (int) (kp->height - kp->pen[pn].cy);

}

/*-
 *
 */
static void
QuadRotated(
		   kaleidstruct * kp,
		   int pn,
		   XSegment segs[4]
)
{
	double      oxscaled2y;
	double      oyscaled2x;
	double      cxscaled2y;
	double      cyscaled2x;

	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	if (kp->pen[pn].cx < 0.0) {
		kp->pen[pn].cx = -kp->pen[pn].cx;
		kp->pen[pn].xv = -kp->pen[pn].xv;
	} else if (kp->pen[pn].cx >= kp->widthby2) {
		kp->pen[pn].cx = (kp->widthby2 - 1.0)
			- (kp->pen[pn].cx - kp->widthby2);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	}
	kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;
	if (kp->pen[pn].cy < 0.0) {
		kp->pen[pn].cy = -kp->pen[pn].cy;
		kp->pen[pn].yv = -kp->pen[pn].yv;
	} else if (kp->pen[pn].cy >= kp->heightby2) {
		kp->pen[pn].cy = (kp->heightby2 - 1.0)
			- (kp->pen[pn].cy - kp->heightby2);
		kp->pen[pn].yv = -kp->pen[pn].yv;
	}
	segs[0].x1 = (int) kp->pen[pn].ox;
	segs[0].y1 = (int) kp->pen[pn].oy;
	segs[0].x2 = (int) kp->pen[pn].cx;
	segs[0].y2 = (int) kp->pen[pn].cy;

	oxscaled2y = ((kp->pen[pn].ox * kp->heightby2) / kp->widthby2);
	oyscaled2x = ((kp->pen[pn].oy * kp->widthby2) / kp->heightby2);
	cxscaled2y = ((kp->pen[pn].cx * kp->heightby2) / kp->widthby2);
	cyscaled2x = ((kp->pen[pn].cy * kp->widthby2) / kp->heightby2);

	segs[1].x1 = (int) (kp->width - oyscaled2x);
	segs[1].y1 = (int) oxscaled2y;
	segs[1].x2 = (int) (kp->width - cyscaled2x);
	segs[1].y2 = (int) cxscaled2y;

	segs[2].x1 = (int) (kp->width - kp->pen[pn].ox);
	segs[2].y1 = (int) (kp->height - kp->pen[pn].oy);
	segs[2].x2 = (int) (kp->width - kp->pen[pn].cx);
	segs[2].y2 = (int) (kp->height - kp->pen[pn].cy);

	segs[3].x1 = (int) oyscaled2x;
	segs[3].y1 = (int) (kp->height - oxscaled2y);
	segs[3].x2 = (int) cyscaled2x;
	segs[3].y2 = (int) (kp->height - cxscaled2y);

}

/*-
 *
 */
static void
GeneralPolarMoveAndBounce(
				 kaleidstruct * kp,
				 int pn
)
{
	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	if (kp->pen[pn].cx < 0.0) {
	    kp->pen[pn].cx = -kp->pen[pn].cx;
		kp->pen[pn].xv = -kp->pen[pn].xv;
	} else if (kp->pen[pn].cx >= kp->radius) {
	    kp->pen[pn].cx = (kp->radius - 1.0)
		    - (kp->pen[pn].cx - kp->radius);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	}
	switch (kp->bouncetype) {
		case 0:
			{
				kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;
				break;
			}
		case 1:
			{
				kp->pen[pn].cy = kp->pen[pn].cy
					+ ((kp->pen[pn].yv * kp->pen[pn].cx) / kp->radius);
				break;
			}
		case 2:
			{
				kp->pen[pn].cy = kp->pen[pn].cy
					+ ((kp->pen[pn].yv * (kp->radius - kp->pen[pn].cx)) / kp->radius);
				break;
			}
		default:
			{
				kp->pen[pn].cy = kp->slice / 2.0;
				kp->pen[pn].cx = kp->radius / 2.0;
				break;
			}
	}

	if (kp->pen[pn].cy < 0) {
	    if (kp->spiral) {
		    kp->pen[pn].RadiusOut = False;
		    kp->pen[pn].cy = kp->pen[pn].cy + 360.0;
		} else {
		    kp->pen[pn].RadiusOut = True;
			if (kp->pen[pn].oy >= 0) {
			    kp->pen[pn].yv = -kp->pen[pn].yv;
			}
		}
	} else if (kp->spiral) {
	    if (kp->pen[pn].cy > 360.0) {
		    kp->pen[pn].RadiusOut = False;
			kp->pen[pn].cy = kp->pen[pn].cy - 360.0;
		}
	} else if (kp->pen[pn].cy >= kp->slice) {
	    kp->pen[pn].RadiusOut = True;
		if (kp->pen[pn].oy < kp->slice) {
		    kp->pen[pn].yv = -kp->pen[pn].yv;
		}
	} else {
		kp->pen[pn].RadiusOut = False;
	}

}


/*-
 *
 */
static void
GeneralRotated(
		      kaleidstruct * kp,
		      int pn,
		      XSegment * segs
)
{
	double      Angle;
	int         segnum;


	GeneralPolarMoveAndBounce(kp, pn);

	Angle = 0.0;
	for (segnum = 0; segnum < kp->modetype; segnum += 1) {
	    if (! kp->spots) {
		    segs[segnum].x1 = (int) (PolarToCartX(kp->pen[pn].ox,
								  kp->pen[pn].oy + Angle
					     ) + kp->widthby2
		        );

			segs[segnum].y1 = (int) (PolarToCartY(kp->pen[pn].ox,
								  kp->pen[pn].oy + Angle
						 ) + kp->heightby2
				);
		}
		segs[segnum].x2 = (int) (PolarToCartX(kp->pen[pn].cx,
						      kp->pen[pn].cy + Angle
					 ) + kp->widthby2
			);
		segs[segnum].y2 = (int) (PolarToCartY(kp->pen[pn].cx,
						      kp->pen[pn].cy + Angle
					 ) + kp->heightby2
			);

		if (kp->spots) {
		    segs[segnum].x1 = segs[segnum].x2;
		    segs[segnum].y1 = segs[segnum].y2;
		}

		Angle += kp->slice;
	}
}

/*-
 *
 */
static void
GeneralMirrored(
		       kaleidstruct * kp,
		       int pn,
		       XSegment * segs
)
{
	double      Angle;
	int         segnum;

	GeneralPolarMoveAndBounce(kp, pn);

	Angle = 0.0;
	for (segnum = 0; segnum < kp->modetype; segnum += 2) {
	    if (! kp->spots) {
		    segs[segnum].x1 = (int) (PolarToCartX(kp->pen[pn].ox,
								  kp->pen[pn].oy + Angle
						 ) + kp->widthby2
				);

			segs[segnum].y1 = (int) (PolarToCartY(kp->pen[pn].ox,
								  kp->pen[pn].oy + Angle
						 ) + kp->heightby2
				);
        }

		segs[segnum].x2 = (int) (PolarToCartX(kp->pen[pn].cx,
						      kp->pen[pn].cy + Angle
					 ) + kp->widthby2
			);

		segs[segnum].y2 = (int) (PolarToCartY(kp->pen[pn].cx,
						      kp->pen[pn].cy + Angle
					 ) + kp->heightby2
			);

		if (kp->spots) {
		    segs[segnum].x1 = segs[segnum].x2;
		    segs[segnum].y1 = segs[segnum].y2;
		}

		Angle += (2.0 * kp->slice);
	}

	Angle = 360.0 - kp->slice;
	for (segnum = 1; segnum < kp->modetype; segnum += 2) {
	    if (! kp->spots) {
		    segs[segnum].x1 =
			    (int) (PolarToCartX(kp->pen[pn].ox,
						 (kp->slice - kp->pen[pn].oy) + Angle
					   ) + kp->widthby2
			    );

			segs[segnum].y1 =
			    (int) (PolarToCartY(kp->pen[pn].ox,
					     (kp->slice - kp->pen[pn].oy) + Angle
					   ) + kp->heightby2
				);
		}

		segs[segnum].x2 =
			(int) (PolarToCartX(kp->pen[pn].cx,
					 (kp->slice - kp->pen[pn].cy) + Angle
			       ) + kp->widthby2
			);

		segs[segnum].y2 =
			(int) (PolarToCartY(kp->pen[pn].cx,
					 (kp->slice - kp->pen[pn].cy) + Angle
			       ) + kp->heightby2
			);

		if (kp->spots) {
		    segs[segnum].x1 = segs[segnum].x2;
		    segs[segnum].y1 = segs[segnum].y2;
		}
		Angle -= (2.0 * kp->slice);
	}
}

/*-
 *
 */
static void
OctMirrored(
		   kaleidstruct * kp,
		   int pn,
		   XSegment * segs
)
{
	double      xdiag;
	double      ydiag;
	double      oxscaled2y, cxscaled2y;
	double      oyscaled2x, cyscaled2x;

	/*
	 *  I know that the "bounce" is not really accurate, but I like the way
	 * it looks.
	 */

	xdiag = (kp->widthby2 * kp->pen[pn].oy) / kp->heightby2;

	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	if (kp->pen[pn].cx < xdiag) {
		kp->pen[pn].cx = xdiag + (xdiag - kp->pen[pn].cx);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	} else if (kp->pen[pn].cx >= kp->widthby2) {
		kp->pen[pn].cx = (kp->widthby2 - 1.0)
			- (kp->pen[pn].cx - kp->widthby2);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	}
	ydiag = (kp->heightby2 * kp->pen[pn].cx) / kp->widthby2;

	kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;
	if (kp->pen[pn].cy < 0.0) {
		kp->pen[pn].cy = -kp->pen[pn].cy;
		kp->pen[pn].yv = -kp->pen[pn].yv;
	} else if (kp->pen[pn].cy > ydiag) {
		kp->pen[pn].cy = ydiag - (kp->pen[pn].cy - ydiag);
		kp->pen[pn].yv = -kp->pen[pn].yv;
	}
	segs[0].x1 = (int) kp->pen[pn].ox;
	segs[0].y1 = (int) kp->pen[pn].oy;
	segs[0].x2 = (int) kp->pen[pn].cx;
	segs[0].y2 = (int) kp->pen[pn].cy;

	segs[1].x1 = (int) (kp->width - kp->pen[pn].ox);
	segs[1].y1 = (int) kp->pen[pn].oy;
	segs[1].x2 = (int) (kp->width - kp->pen[pn].cx);
	segs[1].y2 = (int) kp->pen[pn].cy;

	segs[4].x1 = segs[1].x1;
	segs[4].y1 = (int) kp->height - segs[0].y1;
	segs[4].x2 = segs[1].x2;
	segs[4].y2 = (int) kp->height - segs[0].y2;

	segs[5].x1 = segs[0].x1;
	segs[5].y1 = (int) kp->height - segs[0].y1;
	segs[5].x2 = segs[0].x2;
	segs[5].y2 = (int) kp->height - segs[0].y2;


	oxscaled2y = ((kp->pen[pn].ox * kp->heightby2) / kp->widthby2);
	oyscaled2x = ((kp->pen[pn].oy * kp->widthby2) / kp->heightby2);
	cxscaled2y = ((kp->pen[pn].cx * kp->heightby2) / kp->widthby2);
	cyscaled2x = ((kp->pen[pn].cy * kp->widthby2) / kp->heightby2);


	segs[7].x1 = (int) oyscaled2x;
	segs[7].y1 = (int) oxscaled2y;
	segs[7].x2 = (int) cyscaled2x;
	segs[7].y2 = (int) cxscaled2y;


	segs[2].x1 = (int) kp->width - segs[7].x1;
	segs[2].y1 = segs[7].y1;
	segs[2].x2 = (int) kp->width - segs[7].x2;
	segs[2].y2 = segs[7].y2;

	segs[6].x1 = segs[7].x1;
	segs[6].y1 = (int) kp->height - segs[7].y1;
	segs[6].x2 = segs[7].x2;
	segs[6].y2 = (int) kp->height - segs[7].y2;

	segs[3].x1 = (int) kp->width - segs[7].x1;
	segs[3].y1 = segs[6].y1;
	segs[3].x2 = (int) kp->width - segs[7].x2;
	segs[3].y2 = segs[6].y2;

}



/*-
 *
 */
#if 0
static void
OldOctRotated(
		     kaleidstruct * kp,
		     int pn,
		     XSegment * segs
)
{
	double      angle, radius;
	double      oangle, oradius;
	double      rv, av;
	double      perp, para;
	int         i;

	/*
	 *  I know that the "bounce" is not really accurate, but I like the way
	 * it looks.
	 */

	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;
	angle = CartToAngle(kp->pen[pn].cx, kp->pen[pn].cy);
	radius = CartToRadius(kp->pen[pn].cx, kp->pen[pn].cy);

	oangle = CartToAngle(kp->pen[pn].ox, kp->pen[pn].oy);
	oradius = CartToRadius(kp->pen[pn].ox, kp->pen[pn].oy);

	if (radius < 0.0) {
		if (kp->pen[pn].xv < 0.0) {
			kp->pen[pn].xv = -kp->pen[pn].xv;
		}
	} else if (radius > kp->radius) {
		if (kp->pen[pn].xv > 0.0) {
			kp->pen[pn].xv = -kp->pen[pn].xv;
		}
	}
	if (angle < 0.0) {
		if (oangle > 0.0) {
			kp->pen[pn].yv = -kp->pen[pn].yv;
		}
	} else if (angle > 45.0) {
		if (oangle < 45.0) {
			rv = CartToRadius(kp->pen[pn].xv, kp->pen[pn].yv);
			av = CartToAngle(kp->pen[pn].xv, kp->pen[pn].yv);

			para = PolarToCartX(rv, av - 45.0);
			perp = PolarToCartY(rv, av - 45.0);

			rv = CartToRadius(para, -perp);
			av = CartToAngle(para, -perp);

			kp->pen[pn].xv = PolarToCartX(rv, av + 45.0);
			kp->pen[pn].yv = PolarToCartY(rv, av + 45.0);
		}
	}
	for (i = 0; i < 8; i += 1) {
		segs[i].x1 = (int) (kp->widthby2 + PolarToCartX(oradius, oangle));
		segs[i].y1 = (int) (kp->heightby2 - PolarToCartY(oradius, oangle));
		segs[i].x2 = (int) (kp->widthby2 + PolarToCartX(radius, angle));
		segs[i].y2 = (int) (kp->heightby2 - PolarToCartY(radius, angle));

		oangle += 45.0;
		angle += 45.0;
	}
}

#endif

/*-
 *
 */
static void
OctRotated(
		  kaleidstruct * kp,
		  int pn,
		  XSegment * segs
)
{
	double      xdiag;
	double      ydiag;
	double      radius, angle;
	double      oradius, oangle;
	double      ox, oy, cx, cy;
	double      offset;
	int         i;

	/*
	 *  I know that the "bounce" is not really accurate, but I like the way
	 * it looks.
	 */

	xdiag = (kp->widthby2 * kp->pen[pn].oy) / kp->heightby2;

	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	if (kp->pen[pn].cx < xdiag) {
		kp->pen[pn].cx = xdiag + (xdiag - kp->pen[pn].cx);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	} else if (kp->pen[pn].cx >= kp->widthby2) {
		kp->pen[pn].cx = (kp->widthby2 - 1.0)
			- (kp->pen[pn].cx - kp->widthby2);
		kp->pen[pn].xv = -kp->pen[pn].xv;
	}
	ydiag = (kp->heightby2 * kp->pen[pn].cx) / kp->widthby2;

	kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;
	if (kp->pen[pn].cy < 0.0) {
		kp->pen[pn].cy = -kp->pen[pn].cy;
		kp->pen[pn].yv = -kp->pen[pn].yv;
	} else if (kp->pen[pn].cy > ydiag) {
		kp->pen[pn].cy = ydiag - (kp->pen[pn].cy - ydiag);
		kp->pen[pn].yv = -kp->pen[pn].yv;
	}
	offset = CartToRadius(kp->heightby2, kp->widthby2);
	ox = (kp->pen[pn].ox * offset) / kp->widthby2;
	oy = (kp->pen[pn].oy * offset) / kp->heightby2;
	cx = (kp->pen[pn].cx * offset) / kp->widthby2;
	cy = (kp->pen[pn].cy * offset) / kp->heightby2;

	angle = CartToAngle(cx - offset,
			    offset - cy
		) - 90.0;
	radius = CartToRadius(cx - offset,
			      offset - cy
		);

	oangle = CartToAngle(ox - offset,
			     offset - oy
		) - 90.0;
	oradius = CartToRadius(ox - offset,
			       offset - oy
		);


	for (i = 0; i < 8; i += 1) {
		segs[i].x1 = (int) (kp->widthby2 + PolarToCartX(oradius, oangle));
		segs[i].y1 = (int) (kp->heightby2 - PolarToCartY(oradius, oangle));
		segs[i].x2 = (int) (kp->widthby2 + PolarToCartX(radius, angle));
		segs[i].y2 = (int) (kp->heightby2 - PolarToCartY(radius, angle));

		oangle += 45.0;
		angle += 45.0;
	}

}

/*-
 *
 */
static void
GeneralLinearMoveAndBounce(
				  kaleidstruct * kp,
				  int pn,
				  double *angle,
				  double *radius,
				  double *oangle,
				  double *oradius
)
{
	double      rv, av;
	double      perp, para;

	kp->pen[pn].cx = kp->pen[pn].cx + kp->pen[pn].xv;
	kp->pen[pn].cy = kp->pen[pn].cy + kp->pen[pn].yv;

	*angle = CartToAngle(kp->pen[pn].cx, kp->pen[pn].cy);
	*radius = CartToRadius(kp->pen[pn].cx, kp->pen[pn].cy);

	*oangle = CartToAngle(kp->pen[pn].ox, kp->pen[pn].oy);
	*oradius = CartToRadius(kp->pen[pn].ox, kp->pen[pn].oy);

	if (*radius < 0.0) {
		kp->pen[pn].RadiusOut = True;
		if (*oradius > 0.0) {
			rv = CartToRadius(kp->pen[pn].xv, kp->pen[pn].yv);
			av = CartToAngle(kp->pen[pn].xv, kp->pen[pn].yv);

			para = PolarToCartX(rv, av - (*angle + 90.0));
			perp = PolarToCartY(rv, av - (*angle + 90.0));

			rv = CartToRadius(para, -perp);
			av = CartToAngle(para, -perp);

			kp->pen[pn].xv = PolarToCartX(rv, av + (*angle + 90.0));
			kp->pen[pn].yv = PolarToCartY(rv, av + (*angle + 90.0));
		}
	} else if (*radius > kp->radius) {
		kp->pen[pn].RadiusOut = True;
		if (*oradius < kp->radius) {
			rv = CartToRadius(kp->pen[pn].xv, kp->pen[pn].yv);
			av = CartToAngle(kp->pen[pn].xv, kp->pen[pn].yv);

			para = PolarToCartX(rv, av - (*angle + 90.0));
			perp = PolarToCartY(rv, av - (*angle + 90.0));

			rv = CartToRadius(para, -perp);
			av = CartToAngle(para, -perp);

			kp->pen[pn].xv = PolarToCartX(rv, av + (*angle + 90.0));
			kp->pen[pn].yv = PolarToCartY(rv, av + (*angle + 90.0));
		}
	} else {
		kp->pen[pn].RadiusOut = False;
	}


	if (*angle < 0.0) {
	    if (kp->spiral) {
		    kp->pen[pn].AngleOut = False;
		    *angle = *angle + 360.0;
		} else {
		    kp->pen[pn].AngleOut = True;
			  if (*oangle > 0.0) {
				  rv = CartToRadius(kp->pen[pn].xv, kp->pen[pn].yv);
				  av = CartToAngle(kp->pen[pn].xv, kp->pen[pn].yv);

				  para = PolarToCartX(rv, av);
				  perp = PolarToCartY(rv, av);

				  rv = CartToRadius(para, -perp);
				  av = CartToAngle(para, -perp);

				  kp->pen[pn].xv = PolarToCartX(rv, av);
				  kp->pen[pn].yv = PolarToCartY(rv, av);
			  }
		}
    } else if (kp->spiral) {
	    if (*angle > 360.0) {
	         kp->pen[pn].AngleOut = False;
			 *angle = *angle - 360.0;
	    }
	} else if (*angle > kp->slice) {
		kp->pen[pn].AngleOut = True;
		if (*oangle < kp->slice) {
			rv = CartToRadius(kp->pen[pn].xv, kp->pen[pn].yv);
			av = CartToAngle(kp->pen[pn].xv, kp->pen[pn].yv);

			para = PolarToCartX(rv, av - kp->slice);
			perp = PolarToCartY(rv, av - kp->slice);

			rv = CartToRadius(para, -perp);
			av = CartToAngle(para, -perp);

			kp->pen[pn].xv = PolarToCartX(rv, av + kp->slice);
			kp->pen[pn].yv = PolarToCartY(rv, av + kp->slice);
		}
	} else {
		kp->pen[pn].AngleOut = False;
	}
}

/*-
 *
 */
static void
GeneralLinearRotated(
			    kaleidstruct * kp,
			    int pn,
			    XSegment * segs
)
{
	double      angle, radius;
	double      oangle, oradius;
	int         i;

	GeneralLinearMoveAndBounce(kp, pn, &angle, &radius, &oangle, &oradius);

	for (i = 0; i < kp->modetype; i += 1) {
	    if (! kp->spots) {
		    segs[i].x1 = (int) (kp->widthby2 + PolarToCartX(oradius, oangle));
			segs[i].y1 = (int) (kp->heightby2 - PolarToCartY(oradius, oangle));
		}

		segs[i].x2 = (int) (kp->widthby2 + PolarToCartX(radius, angle));
		segs[i].y2 = (int) (kp->heightby2 - PolarToCartY(radius, angle));

		if (kp->spots) {
		    segs[i].x1 = segs[i].x2;
			segs[i].y1 = segs[i].y2;
		}

		oangle += kp->slice;
		angle += kp->slice;
	}
}



/*-
 *
 */
static void
GeneralLinearMirrored(
			     kaleidstruct * kp,
			     int pn,
			     XSegment * segs
)
{
	double      hangle, angle, radius;
	double      hoangle, oangle, oradius;
	int         i;


	GeneralLinearMoveAndBounce(kp, pn, &angle, &radius, &oangle, &oradius);

	hoangle = oangle;
	hangle = angle;

	for (i = 0; i < kp->modetype; i += 2) {
	    if (! kp->spots) {
		    segs[i].x1 = (int) (kp->widthby2 + PolarToCartX(oradius, oangle));
			segs[i].y1 = (int) (kp->heightby2 - PolarToCartY(oradius, oangle));
		}

		segs[i].x2 = (int) (kp->widthby2 + PolarToCartX(radius, angle));
		segs[i].y2 = (int) (kp->heightby2 - PolarToCartY(radius, angle));

		if (kp->spots) {
		    segs[i].x1 = segs[i].x2;
			segs[i].y1 = segs[i].y2;
		}

		oangle += 2.0 * kp->slice;
		angle += 2.0 * kp->slice;
	}

	oangle = kp->slice * 2.0;
	angle = kp->slice * 2.0;
	for (i = 1; i < kp->modetype; i += 2) {
	    if (! kp->spots) {
		    segs[i].x1 = (int) (kp->widthby2 + PolarToCartX(oradius, oangle - hoangle));
			segs[i].y1 = (int) (kp->heightby2 - PolarToCartY(oradius, oangle - hoangle));
		}

		segs[i].x2 = (int) (kp->widthby2 + PolarToCartX(radius, angle - hangle));
		segs[i].y2 = (int) (kp->heightby2 - PolarToCartY(radius, angle - hangle));

		if (kp->spots) {
		    segs[i].x1 = segs[i].x2;
			segs[i].y1 = segs[i].y2;
		}

		oangle += 2.0 * kp->slice;
		angle += 2.0 * kp->slice;
	}
}

/*-
 *
 */
static void
random_velocity(kaleidstruct * kp, int i)
{
    int tMxRV, tMnRV;
	int tMxAV, tMnAV;
    int tMxV,  tMnV;
	
	if (kp->spots) {
	    tMxRV = MaxRadialVelocity * SpotsMult;
	    tMnRV = MinRadialVelocity * SpotsMult;
	    tMxAV = MaxAngularVelocity * SpotsMult;
	    tMnAV = MinAngularVelocity * SpotsMult;
        tMxV  = MaxVelocity * SpotsMult;
        tMnV  = MinVelocity * SpotsMult;
	} else {
	    tMxRV = MaxRadialVelocity;
	    tMnRV = MinRadialVelocity;
	    tMxAV = MaxAngularVelocity;
	    tMnAV = MinAngularVelocity;
        tMxV  = MaxVelocity;
        tMnV  = MinVelocity;
	}


	if (kp->modetype > 0) {
		kp->pen[i].xv = INTRAND(-tMxRV, tMxRV);
		if (kp->pen[i].xv > 0.0) {
			kp->pen[i].xv += tMnRV;
		} else if (kp->pen[i].xv < 0.0) {
			kp->pen[i].xv -= tMnRV;
		}
		kp->pen[i].yv = INTRAND(-tMxAV, tMxAV);
		if (kp->pen[i].yv > 0.0) {
			kp->pen[i].yv += tMnAV;
		} else if (kp->pen[i].yv < 0.0) {
			kp->pen[i].yv -= tMnAV;
		}
	} else {
		kp->pen[i].xv = INTRAND(-tMxV, tMxV);
		if (kp->pen[i].xv > 0.0) {
			kp->pen[i].xv += tMnV;
		} else if (kp->pen[i].xv < 0.0) {
			kp->pen[i].xv -= tMnV;
		}
		kp->pen[i].yv = INTRAND(-tMxV, tMxV);
		if (kp->pen[i].yv > 0.0) {
			kp->pen[i].yv += tMnV;
		} else if (kp->pen[i].yv < 0.0) {
			kp->pen[i].yv -= tMnV;
		}
	}
}


static void
random_position(kaleidstruct * kp, int i)
{

	if (kp->modetype >= 0) {
		if (kp->linear) {
			double      radius, angle;

			radius = (double) INTRAND(0, (int) (kp->radius - 1.0));
			angle = (double) INTRAND(0, (int) (kp->slice - 1.0));

			kp->pen[i].cx = PolarToCartX(radius, angle);
			kp->pen[i].cy = PolarToCartY(radius, angle);
		} else {
			kp->pen[i].cx = (double) INTRAND(0, (int) (kp->radius - 1.0));
			kp->pen[i].cy = (double) INTRAND(0, (int) (kp->slice - 1.0));
		}
	} else if (kp->modetype == OCT) {
		double      radius, angle;

		radius = (double) INTRAND(0, (int) (kp->radius - 1.0));
		angle = (double) INTRAND(0, 44);

		kp->pen[i].cx = PolarToCartX(radius, angle);
		kp->pen[i].cy = PolarToCartY(radius, angle);
	} else {
		kp->pen[i].cx = (double) INTRAND(0, (int) (kp->widthby2 - 1.0));
		kp->pen[i].cy = (double) INTRAND(0,
		     (int) ((kp->heightby2 * kp->pen[i].cx) / kp->widthby2));
	}
}


/*-
 *
 */
void
init_kaleid(ModeInfo * mi)
{
	int         i;
	kaleidstruct *kp;


	if (kaleids == NULL) {
		if ((kaleids = (kaleidstruct *) calloc(MI_NUM_SCREENS(mi),
			       sizeof (kaleidstruct))) == NULL)
			return;
	}
	kp = &kaleids[MI_SCREEN(mi)];

	kp->PenCount = MI_COUNT(mi);

	if (MI_IS_FULLRANDOM(mi)) {
		kp->alternate = (Bool) (LRAND() & 1);
		kp->disconnected = (Bool) (LRAND() & 1);
		kp->serial = (Bool) (LRAND() & 1);
		kp->linear = (Bool) (LRAND() & 1);
		kp->spiral = (Bool) (LRAND() & 1);
		kp->spots = (Bool) (LRAND() & 1);
	} else {
		kp->alternate = Alternate;
		kp->disconnected = Disconnected;
		kp->serial = Serial;
		kp->linear = Linear;
		kp->spiral = Spiral;
		kp->spots = Spots;
	}

	if (kp->PenCount < -MINPENS) {
		/* if kp->PenCount is random ... the size can change */
		if (kp->pen != NULL) {
			(void) free((void *) kp->pen);
			kp->pen = NULL;
		}
		kp->PenCount = NRAND(-kp->PenCount - MINPENS + 1) + MINPENS;
	} else if (kp->PenCount < MINPENS)
		kp->PenCount = MINPENS;

	if (kp->pen == NULL) {
		if ((kp->pen = (penstruct *) malloc(kp->PenCount *
				sizeof (penstruct))) == NULL)
			return;
	}

	if ((MI_SIZE(mi)) > MINSIZE) {
		kp->modetype = (!kp->alternate + 1) * MI_SIZE(mi);
	} else if ((MI_SIZE(mi)) < -MINSIZE) {
		kp->modetype = (!kp->alternate + 1) * (NRAND(-MI_SIZE(mi) + 1) + MINSIZE);
	} else {
		kp->modetype = (!kp->alternate + 1) * MINSIZE;
	}
	if (MI_IS_FULLRANDOM(mi)) {
		int         tmp;

		tmp = NRAND(ABS(MI_SIZE(mi)) + 2);
		if (tmp == 0)
			kp->modetype = OCT;
		else if (tmp == 1)
			kp->modetype = QUAD;
	} else {
		if (Oct)
			kp->modetype = OCT;
		else if (Quad)
			kp->modetype = QUAD;
	}

	kp->PercentBlack = (int) MAX(0, MIN(MI_CYCLES(mi), 95));


	/* set various size parameters */

	kp->width = (double) MI_WIDTH(mi);
	kp->height = (double) MI_HEIGHT(mi);

	if (kp->width < 2.0)
		kp->width = 2.0;
	if (kp->height < 2.0)
		kp->height = 2.0;

	kp->radius = sqrt(((kp->width * kp->width) +
			   (kp->height * kp->height)
			  ) / 4.0
		);


	if (kp->modetype >= 0) {
		kp->bouncetype = INTRAND(0, 2);

		kp->slice = 360.0 / (double) kp->modetype;

		kp->widthby2 = kp->width / 2.0;
		kp->heightby2 = kp->height / 2.0;
	} else {
		kp->widthby2 = kp->width / 2.0;
		kp->heightby2 = kp->height / 2.0;
	}

	/* set the maximum pen width */
	if (kp->modetype >= 0) {
		if ((kp->slice == 360.0) || (kp->slice == 180.0)) {
			kp->maxlwidth = (int) ((((double) MIN(kp->widthby2, kp->heightby2)) *
					     (double) WidthPercent) / 100.0);
		} else {
			kp->maxlwidth = (int) (((sin(kp->slice * ToRadians) *
					  MIN(kp->widthby2, kp->heightby2)) *
					     (double) WidthPercent) / 100.0);
		}
	} else {
		kp->maxlwidth = (int) ((MIN(kp->widthby2,
					    kp->heightby2
					) * (double) WidthPercent
				       ) / 100.0
			);
	}

	if (kp->spots) {
	    kp->maxlwidth = 2 * kp->maxlwidth;
	}

	if (kp->maxlwidth <= 0) {
		kp->maxlwidth = 1;
	}

	for (i = 0; i < kp->PenCount; i += 1) {
		if (MI_NPIXELS(mi) > 2) {
			kp->pen[i].pix = NRAND(MI_NPIXELS(mi));
			kp->pen[i].White = 1;
		} else {
			kp->pen[i].White = 1;
		}

		kp->pen[i].curlwidth = INTRAND(1, kp->maxlwidth);

		kp->pen[i].RadiusOut = False;
		kp->pen[i].AngleOut = False;

		random_position(kp, i);

		kp->pen[i].ox = kp->pen[i].cx;
		kp->pen[i].oy = kp->pen[i].cy;
		kp->pen[i].DeferredChange = False;

		random_velocity(kp, i);
	}

	MI_CLEARWINDOW(mi);

}

/*-
 *
 */
static void
set_pen_attributes(ModeInfo * mi, kaleidstruct * kp, int i)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);

	if (kp->pen[i].White) {
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, gc, MI_PIXEL(mi, kp->pen[i].pix));
		else
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	} else {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	}
	XSetLineAttributes(display, gc,
		       kp->pen[i].curlwidth, LineSolid, CapRound, JoinRound);
}

/*-
 *
 */
static void
change_pen(ModeInfo * mi, kaleidstruct * kp, int i)
{
	if (INTRAND(0, 100) < kp->PercentBlack) {
		kp->pen[i].White = 0;
	} else {
		kp->pen[i].White = 1;
		if (kp->serial) {
			if (MI_NPIXELS(mi) > 2) {
				if (++kp->pen[i].pix >= MI_NPIXELS(mi))
					kp->pen[i].pix = 0;
			}
		} else {
			if (MI_NPIXELS(mi) > 2) {
				kp->pen[i].pix = NRAND(MI_NPIXELS(mi));
			}
		}
	}

	random_velocity(kp, i);

	kp->pen[i].curlwidth = INTRAND(1, kp->maxlwidth);

	if (kp->modetype >= 0) {
		kp->bouncetype = INTRAND(0, 2);
	}
	if (kp->disconnected) {
		random_position(kp, i);
		kp->pen[i].ox = kp->pen[i].cx;
		kp->pen[i].oy = kp->pen[i].cy;
	}
}

/*-
 *
 */
void
draw_kaleid(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	XSegment   *segs;
	int         NumberOfSegments;
	int         i;
	kaleidstruct *kp;

	if (kaleids == NULL)
			return;
	kp = &kaleids[MI_SCREEN(mi)];
	if (kp->pen == NULL)
			return;

	MI_IS_DRAWN(mi) = True;
	if (kp->modetype == QUAD) {
		NumberOfSegments = 4;
	} else if (kp->modetype == OCT) {
		NumberOfSegments = 8;
	} else {		/* if (kp->modetype > 0) */
		NumberOfSegments = kp->modetype;
	}
	if ((segs = (XSegment *) malloc(NumberOfSegments *
			sizeof (XSegment))) == NULL) {
		(void) free((void *) kp->pen);
		kp->pen = NULL;
		return;
	}

	for (i = 0; i < kp->PenCount; i++) {
		set_pen_attributes(mi, kp, i);

		if (kp->modetype == QUAD) {
			if (kp->alternate) {
				QuadRotated(kp, i, segs);
			} else {
				QuadMirrored(kp, i, segs);
			}
		} else if (kp->modetype == OCT) {
			if (kp->alternate) {
				OctRotated(kp, i, segs);
			} else {
				OctMirrored(kp, i, segs);
			}
		} else {
			if (kp->alternate) {	  

				if (kp->linear) {
					GeneralLinearRotated(kp, i, segs);
				} else {
					GeneralRotated(kp, i, segs);
				}
			} else {
				if (kp->linear) {
					GeneralLinearMirrored(kp, i, segs);
				} else {
					GeneralMirrored(kp, i, segs);
				}
			}
		}
		XDrawSegments(
				     display,
				     MI_WINDOW(mi),
				     gc,
				     segs,
				     NumberOfSegments
			);

		kp->pen[i].ox = kp->pen[i].cx;
		kp->pen[i].oy = kp->pen[i].cy;


		if ((INTRAND(0, 100) < ChangeChance) || kp->pen[i].DeferredChange) {
			if (!kp->pen[i].AngleOut && !kp->pen[i].RadiusOut) {
				kp->pen[i].DeferredChange = False;
				change_pen(mi, kp, i);
			} else {
				kp->pen[i].DeferredChange = True;
			}
		}
	}
	  

	XSetLineAttributes(display, gc, 1, LineSolid, CapRound, JoinRound);
	(void) free((void *) segs);
}

/*-
 *
 */
void
release_kaleid(ModeInfo * mi)
{
	if (kaleids != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			kaleidstruct *kp = &kaleids[screen];

			if (kp->pen != NULL)
				(void) free((void *) kp->pen);
		}
		(void) free((void *) kaleids);
		kaleids = NULL;
	}
}

/*-
 *
 */
void
refresh_kaleid(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_kaleid */
