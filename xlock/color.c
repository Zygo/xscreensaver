
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)color.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * color.c - extracted from swirl.c, xlock.c and utils.c
 *
 * See xlock.c for copying information.
 *
 * xlock.c and utils.c Copyright (c) 1988-91 by Patrick J. Naughton.
 * swirl.c Copyright (c) 1994 M.Dobie <mrd@ecs.soton.ac.uk>
 *
 */

#include "xlock.h"

/* Formerly in utils.c */
/*-
 * Create an HSB ramp.
 *
 * Revision History:
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
 * 22-Jun-94: Modified for VMS
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * Changes of Patrick J. Naughton
 * 29-Jul-90: renamed hsbramp.c from HSBmap.c
 *	      minor optimizations.
 * 01-Sep-88: Written.
 */

static void
hsb2rgb(double H, double S, double B,
	unsigned char *r, unsigned char *g, unsigned char *b)
{
	int         i;
	double      f, bb;
	unsigned char p, q, t;

	H -= floor(H);		/* remove anything over 1 */
	H *= 6.0;
	i = (int) floor(H);	/* 0..5 */
	f = H - (float) i;	/* f = fractional part of H */
	bb = 255.0 * B;
	p = (unsigned char) (bb * (1.0 - S));
	q = (unsigned char) (bb * (1.0 - (S * f)));
	t = (unsigned char) (bb * (1.0 - (S * (1.0 - f))));
	switch (i) {
		case 0:
			*r = (unsigned char) bb;
			*g = t;
			*b = p;
			break;
		case 1:
			*r = q;
			*g = (unsigned char) bb;
			*b = p;
			break;
		case 2:
			*r = p;
			*g = (unsigned char) bb;
			*b = t;
			break;
		case 3:
			*r = p;
			*g = q;
			*b = (unsigned char) bb;
			break;
		case 4:
			*r = t;
			*g = p;
			*b = (unsigned char) bb;
			break;
		case 5:
			*r = (unsigned char) bb;
			*g = p;
			*b = q;
			break;
	}
}


/*-
 * Input is two points in HSB color space and a count
 * of how many discreet rgb space values the caller wants.
 *
 * Output is that many rgb triples which describe a linear
 * interpolate ramp between the two input colors.
 */

static void
hsbramp(double h1, double s1, double b1, double h2, double s2, double b2,
    int count, unsigned char *red, unsigned char *green, unsigned char *blue)
{
	double      dh, ds, db;

	dh = (h2 - h1) / count;
	ds = (s2 - s1) / count;
	db = (b2 - b1) / count;
	while (count--) {
		hsb2rgb(h1, s1, b1, red++, green++, blue++);
		h1 += dh;
		s1 += ds;
		b1 += db;
	}
}


/* Formerly in xlock.c */

unsigned long
allocPixel(Display * display, Colormap cmap, char *name, char *def)
{
	XColor      col;
	XColor      tmp;

	(void) XParseColor(display, cmap, name, &col);
	if (!XAllocColor(display, cmap, &col)) {
		(void) fprintf(stderr, "could not allocate: %s, using %s instead\n",
			       name, def);
		(void) XAllocNamedColor(display, cmap, def, &col, &tmp);
	}
	return col.pixel;
}

void
fixColormap(Display * display, Window window, int screen, float saturation,
	    Bool mono, Bool install, Bool inroot, Bool inwindow, Bool verbose)
{
	Screen     *scr = ScreenOfDisplay(display, screen);
	Colormap    cmap, dcmap = DefaultColormapOfScreen(scr);
	int         colorcount = NUMCOLORS;
	int         i;
	static float *lastsat = NULL;
	extern perscreen Scr[MAXSCREENS];

	if (mono || CellsOfScreen(scr) <= 2) {
		Scr[screen].pixels[0] = WhitePixelOfScreen(scr);
		Scr[screen].pixels[1] = BlackPixelOfScreen(scr);
		Scr[screen].npixels = 2;
		return;
	}
	if (!lastsat) {
		lastsat = (float *) malloc(MAXSCREENS * sizeof (float));

		for (i = 0; i < MAXSCREENS; i++)
			lastsat[i] = -1.0;
	}
	if (saturation != lastsat[screen]) {
		unsigned char *red, *green, *blue;
		XColor      xcolor;

		lastsat[screen] = saturation;
		red = (unsigned char *) calloc(NUMCOLORS, sizeof (unsigned char));
		green = (unsigned char *) calloc(NUMCOLORS, sizeof (unsigned char));
		blue = (unsigned char *) calloc(NUMCOLORS, sizeof (unsigned char));

		cmap = Scr[screen].colormap;
		if (cmap != None)
			XFreeColors(display, cmap, Scr[screen].pixels, Scr[screen].npixels, 0);
		else {
			/* "allocate" the black and white pixels, so that they
			   will be included by XCopyColormapAndFree() if it
			   gets called */
			xcolor.pixel = BlackPixelOfScreen(scr);
			XQueryColor(display, dcmap, &xcolor);
			(void) XAllocColor(display, dcmap, &xcolor);
			xcolor.pixel = WhitePixelOfScreen(scr);
			XQueryColor(display, dcmap, &xcolor);
			(void) XAllocColor(display, dcmap, &xcolor);
			/*xcolor.pixel = Scr[screen].fgcol;
			   XQueryColor(display, dcmap, &xcolor);
			   (void) XAllocColor(display, dcmap, &xcolor);
			   xcolor.pixel = Scr[screen].bgcol;
			   XQueryColor(display, dcmap, &xcolor);
			   (void) XAllocColor(display, dcmap, &xcolor); */
			cmap = dcmap;
		}

		hsbramp(0.0, saturation, 1.0, 1.0, saturation, 1.0, colorcount,
			red, green, blue);

		Scr[screen].npixels = 0;
		for (i = 0; i < colorcount; i++) {
			xcolor.red = red[i] << 8;
			xcolor.green = green[i] << 8;
			xcolor.blue = blue[i] << 8;
			xcolor.flags = DoRed | DoGreen | DoBlue;

			if (!XAllocColor(display, cmap, &xcolor)) {
				if (!install || cmap != dcmap)
					break;
				if ((cmap = XCopyColormapAndFree(display, cmap)) == dcmap)
					break;
				if (verbose)
					(void) fprintf(stderr, "using private colormap\n");
				if (!XAllocColor(display, cmap, &xcolor))
					break;
			}
			Scr[screen].pixels[i] = xcolor.pixel;
			Scr[screen].npixels++;
		}
		(void) free((void *) red);
		(void) free((void *) green);
		(void) free((void *) blue);
		if (verbose)
			(void) fprintf(stderr, "%d pixel%s allocated\n", Scr[screen].npixels,
				       (Scr[screen].npixels == 1) ? "" : "s");
		if (Scr[screen].npixels < 2) {
			XFreeColors(display, cmap, Scr[screen].pixels, Scr[screen].npixels, 0);
			Scr[screen].pixels[0] = WhitePixelOfScreen(scr);
			Scr[screen].pixels[1] = BlackPixelOfScreen(scr);
			Scr[screen].npixels = 2;
			Scr[screen].colormap = None;
			return;
		}
	} else
		cmap = Scr[screen].colormap;

	if (install && !inroot && Scr[screen].npixels > 2) {
#if 0
		XGetWindowAttributes(display, window, &xgwa);
		if (cmap != xgwa.colormap)
#endif
			setColormap(display, window, cmap, inwindow);
	}
	Scr[screen].colormap = cmap;
}

void
setColormap(Display * display, Window window, Colormap cmap, Bool inwindow)
{
	XSetWindowColormap(display, window, cmap);
	/* Now, here we have a problem.  When we are running full-screen, the
	   window's override_redirect attribute is on.  So, the window manager
	   never gets the ColormapNotify event that gets generated on the
	   above XSetWindowColormap() call, and does not So, a quick solution
	   is to install it ourselves.  The problem with this is that it
	   violates the ICCCM convention that only window managers should
	   install colormaps. Indeed, Fvwm _enforces_ this by immediately
	   un-doing any XInstallColormap() performed by a client (which is why
	   this does not work right under Fvwm). */
	if (!inwindow)
		XInstallColormap(display, cmap);
}

#if defined( USE_XPM ) || defined( USE_XPMINC )
void
reserveColors(ModeInfo * mi, Colormap cmap,
	      unsigned long *blackpix, unsigned long *whitepix)
{
	Display    *display = MI_DISPLAY(mi);
	unsigned long black, white;
	XColor      whitecolor, blackcolor;

	black = MI_WIN_BLACK_PIXEL(mi);
	white = MI_WIN_WHITE_PIXEL(mi);

	blackcolor.flags = DoRed | DoGreen | DoBlue;
	blackcolor.pixel = black;
	blackcolor.red = 0;
	blackcolor.green = 0;
	blackcolor.blue = 0;
	whitecolor.flags = DoRed | DoGreen | DoBlue;
	whitecolor.pixel = white;
	whitecolor.red = 0xFFFF;
	whitecolor.green = 0xFFFF;
	whitecolor.blue = 0xFFFF;

	/* If they fail what should I do? */
	(void) XAllocColor(display, cmap, &blackcolor);
	(void) XAllocColor(display, cmap, &whitecolor);
#if 0
	/* This stuff is supposed to reserve the fg and bg
	   colors but does not work. */
	{
		unsigned long fg, bg;
		XColor      fgcolor, bgcolor;
		int         screen = MI_SCREEN(mi);

		fg = MI_FG_COLOR(mi);
		bg = MI_BG_COLOR(mi);

		fgcolor.pixel = fg;
		bgcolor.pixel = bg;
		XQueryColor(display, MI_WIN_COLORMAP(mi), &fgcolor);
		XQueryColor(display, MI_WIN_COLORMAP(mi), &bgcolor);

		if ((fgcolor.pixel != blackcolor.pixel) &&
		    (fgcolor.pixel != whitecolor.pixel))
			(void) XAllocColor(display, cmap, &fgcolor);
		if ((bgcolor.pixel != blackcolor.pixel) &&
		    (bgcolor.pixel != whitecolor.pixel) &&
		    (bgcolor.pixel != fgcolor.pixel))
			(void) XAllocColor(display, cmap, &bgcolor);
	}
#endif
	*blackpix = blackcolor.pixel;
	*whitepix = whitecolor.pixel;
}

#endif
