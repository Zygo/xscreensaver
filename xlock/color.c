#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)color.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * color.c - extracted from swirl.c, xlock.c and util.c
 *
 * See xlock.c for copying information.
 *
 * xlock.c and util.c Copyright (c) 1988-91 by Patrick J. Naughton.
 * swirl.c Copyright (c) 1994 M.Dobie <mrd@ecs.soton.ac.uk>
 *
 */

#include "xlock.h"

/* Formerly in util.c */
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
	XColor      col, tmp;

	(void) XParseColor(display, cmap, name, &col);
	if (!XAllocColor(display, cmap, &col)) {
		(void) fprintf(stderr, "could not allocate: %s, using %s instead\n",
			       name, def);
		(void) XAllocNamedColor(display, cmap, def, &col, &tmp);
	}
	return col.pixel;
}

static void
monoColormap(Screen * scr, ScreenInfo * si, char *foreground, char *background)
{
	si->black_pixel = BlackPixelOfScreen(scr);
	si->white_pixel = WhitePixelOfScreen(scr);
	if (strcmp(foreground, "White") == 0 || strcmp(foreground, "white") == 0 ||
	    strcmp(background, "Black") == 0 || strcmp(background, "black") == 0) {
		si->fg_pixel = WhitePixelOfScreen(scr);
		si->bg_pixel = BlackPixelOfScreen(scr);
	} else {
		si->fg_pixel = BlackPixelOfScreen(scr);
		si->bg_pixel = WhitePixelOfScreen(scr);
	}
	si->pixels[0] = WhitePixelOfScreen(scr);
	si->pixels[1] = BlackPixelOfScreen(scr);
	si->npixels = 2;
}

void
fixColormap(Display * display, Window window,
	    int screen, int ncolors, float saturation,
	    Bool mono, Bool install, Bool inroot, Bool inwindow, Bool verbose)
{
	Screen     *scr = ScreenOfDisplay(display, screen);
	extern ScreenInfo *Scr;
	Colormap    cmap = Scr[screen].colormap;
	XColor      xcolor;
	unsigned char *red, *green, *blue;
	int         colorcount, i, fixed, visualclass;
	extern char *foreground;
	extern char *background;

#ifdef USE_DTSAVER
	extern Bool dtsaver;

#endif

	if (mono || CellsOfScreen(scr) <= 2) {
		if (Scr[screen].pixels)
			return;
		Scr[screen].pixels =
			(unsigned long *) calloc(2, sizeof (unsigned long));

		monoColormap(scr, &(Scr[screen]), foreground, background);
		return;
	}
	colorcount = ncolors;
	red = (unsigned char *) calloc(ncolors, sizeof (unsigned char));
	green = (unsigned char *) calloc(ncolors, sizeof (unsigned char));
	blue = (unsigned char *) calloc(ncolors, sizeof (unsigned char));

	visualclass = Scr[screen].visual->CLASS;
	fixed = (visualclass == StaticGray) || (visualclass == StaticColor) ||
		(visualclass == TrueColor);
	if (
#ifdef USE_DTSAVER
		   dtsaver ||	/* needs to be in focus without mouse */

#endif
		   inroot || (!install && !fixed) || cmap == None) {
		cmap = DefaultColormapOfScreen(scr);
	}
	if (cmap != DefaultColormapOfScreen(scr) && Scr[screen].pixels) {
		XFreeColors(display, cmap, Scr[screen].pixels, Scr[screen].npixels, 0);
		XFreeColors(display, cmap, &(Scr[screen].black_pixel), 1, 0);
		XFreeColors(display, cmap, &(Scr[screen].white_pixel), 1, 0);
		XFreeColors(display, cmap, &(Scr[screen].bg_pixel), 1, 0);
		XFreeColors(display, cmap, &(Scr[screen].fg_pixel), 1, 0);
		(void) free((void *) Scr[screen].pixels);
		Scr[screen].pixels =
			(unsigned long *) calloc(ncolors, sizeof (unsigned long));
	} else {
		if (Scr[screen].pixels) {
			(void) free((void *) Scr[screen].pixels);
			/* (void) printf("pixels: this case is possible?\n"); */
		}
		/* if (cmap) { (void) printf("cmap: this case is possible?\n");  } */
		Scr[screen].pixels =
			(unsigned long *) calloc(ncolors, sizeof (unsigned long));

		/* "allocate" the black and white pixels, so that they
		   will be included by XCopyColormapAndFree() if it gets called */
	}

	Scr[screen].black_pixel = allocPixel(display, cmap, "Black", "Black");
	Scr[screen].white_pixel = allocPixel(display, cmap, "White", "White");
	Scr[screen].bg_pixel = allocPixel(display, cmap, background, "White");
	Scr[screen].fg_pixel = allocPixel(display, cmap, foreground, "Black");
	hsbramp(0.0, saturation, 1.0, 1.0, saturation, 1.0, colorcount,
		red, green, blue);

	Scr[screen].npixels = 0;
	for (i = 0; i < colorcount; i++) {
		xcolor.red = red[i] << 8;
		xcolor.green = green[i] << 8;
		xcolor.blue = blue[i] << 8;
		xcolor.flags = DoRed | DoGreen | DoBlue;

		if (!XAllocColor(display, cmap, &xcolor)) {
			if (verbose)
				(void) fprintf(stderr, "ran out of colors on colormap\n");
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
		XFreeColors(display, cmap, &(Scr[screen].black_pixel), 1, 0);
		XFreeColors(display, cmap, &(Scr[screen].white_pixel), 1, 0);
		XFreeColors(display, cmap, &(Scr[screen].bg_pixel), 1, 0);
		XFreeColors(display, cmap, &(Scr[screen].fg_pixel), 1, 0);
		monoColormap(scr, &(Scr[screen]), foreground, background);
		Scr[screen].colormap = cmap = DefaultColormapOfScreen(scr);
		return;
	}
	Scr[screen].colormap = cmap;
	if ((install || fixed) && !inroot && Scr[screen].npixels > 2) {
#if 0
		XGetWindowAttributes(display, window, &xgwa);
		if (cmap != xgwa.colormap)
#endif
			setColormap(display, window, cmap, inwindow);
	}
#if 0
	else {
		/* white and black colors may not be right for GL modes so lets set them */
		Scr[screen].black_pixel = BlackPixelOfScreen(scr);
		Scr[screen].white_pixel = WhitePixelOfScreen(scr);
		/* foreground and background colors may not be right.... */
	}
#endif
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

/*-
 * useableColors
 */
int
preserveColors(unsigned long black, unsigned long white,
	       unsigned long bg, unsigned long fg)
{
	/* how many colours should we preserve (out of white, black, fg, bg)? */
	if (((bg == black) || (bg == white)) && ((fg == black) || (fg == white)))
		return 2;
	else if ((bg == black) || (fg == black) ||
		 (bg == white) || (fg == white) || (bg == fg))
		return 3;
	else
		return 4;
}

#if defined( USE_XPM ) || defined( USE_XPMINC )
void
reserveColors(ModeInfo * mi, Colormap cmap, unsigned long *black)
{
	Display    *display = MI_DISPLAY(mi);
	XColor      blackcolor, whitecolor;

	blackcolor.flags = DoRed | DoGreen | DoBlue;
	blackcolor.pixel = MI_BLACK_PIXEL(mi);
	blackcolor.red = 0;
	blackcolor.green = 0;
	blackcolor.blue = 0;
	whitecolor.flags = DoRed | DoGreen | DoBlue;
	whitecolor.pixel = MI_WHITE_PIXEL(mi);
	whitecolor.red = 0xFFFF;
	whitecolor.green = 0xFFFF;
	whitecolor.blue = 0xFFFF;

	/* If they fail what should I do? */
	(void) XAllocColor(display, cmap, &blackcolor);
	(void) XAllocColor(display, cmap, &whitecolor);
	*black = blackcolor.pixel;

#if 0
	{
		XColor      bgcolor, fgcolor;

		bgcolor.pixel = MI_BG_PIXEL(mi);
		fgcolor.pixel = MI_FG_PIXEL(mi);
		XQueryColor(display, cmap, &bgcolor);
		XQueryColor(display, cmap, &fgcolor);
		(void) XAllocColor(display, cmap, &bgcolor);
		(void) XAllocColor(display, cmap, &fgcolor);
	}
#endif
}

#endif
