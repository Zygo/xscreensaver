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
#include "color.h"
#include "vis.h"

/* Formerly in util.c */
/*-
 * Create an HSB ramp.
 *
 * Revision History:
 * Changes maintained by David Bagley <bagleyd@tux.org>
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
allocPixel(Display * display, Colormap cmap, const char *name, const char *def)
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
fixColormap(ModeInfo * mi, int ncolors, float saturation,
	    Bool mono, Bool install, Bool inroot, Bool inwindow, Bool verbose)
{
	Display    *display = MI_DISPLAY(mi);
        Window     window = MI_WINDOW(mi);
        Screen     *scr = MI_SCREENPTR(mi);
	Colormap    cmap = MI_COLORMAP(mi);
	Colormap    dcmap = DefaultColormapOfScreen(scr);
	XColor      xcolor;
	unsigned char *red, *green, *blue;
	int         colorcount, i, fixed, visualclass;
	extern char *foreground;
	extern char *background;

#ifdef USE_DTSAVER
	extern Bool dtsaver;

#endif
#ifndef COMPLIANT_COLORMAP
	Bool        retry = False;

#endif

	if (mono || CellsOfScreen(scr) <= 2) {
		if (MI_PIXELS(mi))
			return;
		MI_PIXELS(mi) =
			(unsigned long *) calloc(2, sizeof (unsigned long));

		monoColormap(scr, MI_SCREENINFO(mi), foreground, background);
		return;
	}
	colorcount = ncolors;
	red = (unsigned char *) calloc(ncolors, sizeof (unsigned char));
	green = (unsigned char *) calloc(ncolors, sizeof (unsigned char));
	blue = (unsigned char *) calloc(ncolors, sizeof (unsigned char));

	visualclass = MI_VISUALCLASS(mi);
	fixed = (visualclass == StaticGray) || (visualclass == StaticColor) ||
		(visualclass == TrueColor);
	if (
#ifdef USE_DTSAVER
		   dtsaver ||	/* needs to be in focus without mouse */

#endif
		   inroot || (!install && !fixed) || cmap == None) {
		cmap = dcmap;
	}
	if (cmap != dcmap && MI_PIXELS(mi)) {
		XFreeColors(display, cmap, MI_PIXELS(mi), MI_NPIXELS(mi), 0);
#ifndef COMPLIANT_COLORMAP
		XFreeColors(display, cmap, &(MI_BLACK_PIXEL(mi)), 1, 0);
		XFreeColors(display, cmap, &(MI_WHITE_PIXEL(mi)), 1, 0);
#endif
		XFreeColors(display, cmap, &(MI_BG_PIXEL(mi)), 1, 0);
		XFreeColors(display, cmap, &(MI_FG_PIXEL(mi)), 1, 0);
		(void) free((void *) MI_PIXELS(mi));
		MI_PIXELS(mi) =
			(unsigned long *) calloc(ncolors, sizeof (unsigned long));
	} else {
		if (MI_PIXELS(mi)) {
			(void) free((void *) MI_PIXELS(mi));
			/* (void) printf("pixels: this case is possible?\n"); */
		}
		/* if (cmap) { (void) printf("cmap: this case is possible?\n");  } */
		MI_PIXELS(mi) =
			(unsigned long *) calloc(ncolors, sizeof (unsigned long));

		/* "allocate" the black and white pixels, so that they
		   will be included by XCopyColormapAndFree() if it gets called */
	}
#ifdef COMPLIANT_COLORMAP
	MI_BLACK_PIXEL(mi) = BlackPixelOfScreen(scr);
	MI_WHITE_PIXEL(mi) = WhitePixelOfScreen(scr);
#else
	MI_BLACK_PIXEL(mi) = allocPixel(display, cmap, "Black", "Black");
	MI_WHITE_PIXEL(mi) = allocPixel(display, cmap, "White", "White");
#endif
	MI_BG_PIXEL(mi) = allocPixel(display, cmap, background, "White");
	MI_FG_PIXEL(mi) = allocPixel(display, cmap, foreground, "Black");
	hsbramp(0.0, saturation, 1.0, 1.0, saturation, 1.0, colorcount,
		red, green, blue);

	MI_NPIXELS(mi) = 0;
	for (i = 0; i < colorcount; i++) {
		xcolor.red = red[i] << 8;
		xcolor.green = green[i] << 8;
		xcolor.blue = blue[i] << 8;
		xcolor.flags = DoRed | DoGreen | DoBlue;

		if (!XAllocColor(display, cmap, &xcolor)) {
#ifdef COMPLIANT_COLORMAP
			if (!install || cmap != dcmap)
				break;
			if ((cmap = XCopyColormapAndFree(display, cmap)) == dcmap)
				break;
			if (verbose)
				(void) fprintf(stderr, "using private colormap\n");
			if (!XAllocColor(display, cmap, &xcolor))
				break;
#else
			if (verbose)
				(void) fprintf(stderr, "ran out of colors on colormap\n");
			if ((saturation != 1.0 || ncolors != 64) && MI_NPIXELS(mi) < 2) {
				if (verbose)
					(void) fprintf(stderr,
						       "retrying with saturation = 1.0 and ncolors = 64\n");
				retry = True;
			}
			break;
#endif
		}
		MI_PIXELS(mi)[i] = xcolor.pixel;
		MI_NPIXELS(mi)++;
	}
	(void) free((void *) red);
	(void) free((void *) green);
	(void) free((void *) blue);
	if (verbose)
		(void) fprintf(stderr, "%d pixel%s allocated\n", MI_NPIXELS(mi),
			       (MI_NPIXELS(mi) == 1) ? "" : "s");
	if (MI_NPIXELS(mi) <= 4) {
		XFreeColors(display, cmap, MI_PIXELS(mi), MI_NPIXELS(mi), 0);
#ifndef COMPLIANT_COLORMAP
		XFreeColors(display, cmap, &(MI_BLACK_PIXEL(mi)), 1, 0);
		XFreeColors(display, cmap, &(MI_WHITE_PIXEL(mi) ), 1, 0);
#endif
		XFreeColors(display, cmap, &(MI_BG_PIXEL(mi)), 1, 0);
		XFreeColors(display, cmap, &(MI_BG_PIXEL(mi)), 1, 0);
#ifndef COMPLIANT_COLORMAP
		if (retry) {
			fixColormap(mi, 64, 1.0,
				    mono, install, inroot, inwindow, verbose);
			return;
		}
#endif
		monoColormap(scr, MI_SCREENINFO(mi), foreground, background);
		MI_COLORMAP(mi) = cmap = DefaultColormapOfScreen(scr);
		return;
	}
	MI_COLORMAP(mi) = cmap;
	if ((install || fixed) && !inroot && MI_NPIXELS(mi) > 2) {
#if 0
		(void) XGetWindowAttributes(display, window, &xgwa);
		if (cmap != xgwa.colormap)
#endif
#if 1				/* Turn off to simulate fvwm and tvwm */
			setColormap(display, window, cmap, inwindow);
#endif
	}
#if 0
	else {
		/* white and black colors may not be right for GL modes so lets set them */
		MI_BLACK_PIXEL(mi) = BlackPixelOfScreen(scr);
		MI_WHITE_PIXEL(mi) = WhitePixelOfScreen(scr);
		/* foreground and background colors may not be right.... */
		BlackPixelOfScreen(scr) = MI_BLACK_PIXEL(mi);
		WhitePixelOfScreen(scr) = MI_WHITE_PIXEL(mi);
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

	if (!inwindow) {
		XInstallColormap(display, cmap);
	}
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

/* the remaining of this file was hacked from colors.c and hsv.c from
 * xscreensaver
 *
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
 * Modified for the use with xlockmore by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
 * 12 June 1998
 */

/* This file contains some utility routines for randomly picking the colors
   to hack the screen with.
 */

void
free_colors(Display * dpy, Colormap cmap, XColor * colors, int ncolors)
{
	int         i;

	if (ncolors > 0) {
		unsigned long *pixels = (unsigned long *)
		malloc(sizeof (unsigned long) * ncolors);

		for (i = 0; i < ncolors; i++)
			pixels[i] = colors[i].pixel;
		XFreeColors(dpy, cmap, pixels, ncolors, 0L);
		(void) free((void *) pixels);
	}
}


void
allocate_writable_colors(Display * dpy, Colormap cmap,
			 unsigned long *pixels, int *ncolorsP)
{
	int         desired = *ncolorsP;
	int         got = 0;
	int         requested = desired;
	unsigned long *new_pixels = pixels;

	*ncolorsP = 0;
	while (got < desired
	       && requested > 0) {
		if (desired - got < requested)
			requested = desired - got;

		if (XAllocColorCells(dpy, cmap, False, 0, 0, new_pixels, requested)) {
			/* Got all the pixels we asked for. */
			new_pixels += requested;
			got += requested;
		} else {
			/* We didn't get all/any of the pixels we asked for.  This time, ask
			   for half as many.  (If we do get all that we ask for, we ask for
			   the same number again next time, so we only do O(log(n)) server
			   roundtrips.)
			 */
			requested = requested / 2;
		}
	}
	*ncolorsP += got;
}



void
make_color_ramp(Display * dpy, Colormap cmap,
		int h1, double s1, double v1,	/* 0-360, 0-1.0, 0-1.0 */
		int h2, double s2, double v2,	/* 0-360, 0-1.0, 0-1.0 */
		XColor * colors, int *ncolorsP,
		Bool closed_p,
		Bool allocate_p,
		Bool writable_p)
{
	int         i;
	int         ncolors = *ncolorsP;
	double      dh, ds, dv;	/* deltas */

      AGAIN:

	(void) memset(colors, 0, (*ncolorsP) * sizeof (*colors));

	if (closed_p)
		ncolors = (ncolors / 2) + 1;

	/* Note: unlike other routines in this module, this function assumes that
	   if h1 and h2 are more than 180 degrees apart, then the desired direction
	   is always from h1 to h2 (rather than the shorter path.)  make_uniform
	   depends on this.
	 */
	dh = ((double) h2 - (double) h1) / ncolors;
	ds = (s2 - s1) / ncolors;
	dv = (v2 - v1) / ncolors;

	for (i = 0; i < ncolors; i++) {
		colors[i].flags = DoRed | DoGreen | DoBlue;
		hsv_to_rgb((int) (h1 + (i * dh)), (s1 + (i * ds)), (v1 + (i * dv)),
			   &colors[i].red, &colors[i].green, &colors[i].blue);
	}

	if (closed_p)
		for (i = ncolors; i < *ncolorsP; i++)
			colors[i] = colors[(*ncolorsP) - i];

	if (!allocate_p)
		return;

	if (writable_p) {
		unsigned long *pixels = (unsigned long *)
		malloc(sizeof (unsigned long) * ((*ncolorsP) + 1));

		/* allocate_writable_colors() won't do here, because we need exactly this
		   number of cells, or the color sequence we've chosen won't fit. */
		if (!XAllocColorCells(dpy, cmap, False, 0, 0, pixels, *ncolorsP)) {
			(void) free((void *) pixels);
			goto FAIL;
		}
		for (i = 0; i < *ncolorsP; i++)
			colors[i].pixel = pixels[i];
		(void) free((void *) pixels);

		XStoreColors(dpy, cmap, colors, *ncolorsP);
	} else {
		for (i = 0; i < *ncolorsP; i++) {
			XColor      color;

			color = colors[i];
			if (XAllocColor(dpy, cmap, &color)) {
				colors[i].pixel = color.pixel;
			} else {
				free_colors(dpy, cmap, colors, i);
				goto FAIL;
			}
		}
	}

	return;

      FAIL:
	/* we weren't able to allocate all the colors we wanted;
	   decrease the requested number and try again.
	 */
	ncolors = (ncolors > 170 ? ncolors - 20 :
		   ncolors > 100 ? ncolors - 10 :
		   ncolors > 75 ? ncolors - 5 :
		   ncolors > 25 ? ncolors - 3 :
		   ncolors > 10 ? ncolors - 2 :
		   ncolors > 2 ? ncolors - 1 :
		   0);
	*ncolorsP = ncolors;
	if (ncolors > 0)
		goto AGAIN;
}


#define MAXPOINTS 50		/* yeah, so I'm lazy */


static void
make_color_path(Display * dpy, Colormap cmap,
		int npoints, int *h, double *s, double *v,
		XColor * colors, int *ncolorsP,
		Bool allocate_p,
		Bool writable_p)
{
	int         i, k;
	int         total_ncolors = *ncolorsP;

	int         ncolors[MAXPOINTS];		/* number of pixels per edge */
	double      dh[MAXPOINTS];	/* distance between pixels, per edge (0 - 360.0) */
	double      ds[MAXPOINTS];	/* distance between pixels, per edge (0 - 1.0) */
	double      dv[MAXPOINTS];	/* distance between pixels, per edge (0 - 1.0) */

	if (npoints == 0) {
		*ncolorsP = 0;
		return;
	} else if (npoints == 2) {	/* using make_color_ramp() will be faster */
		make_color_ramp(dpy, cmap,
				h[0], s[0], v[0], h[1], s[1], v[1],
				colors, ncolorsP,
				True,	/* closed_p */
				allocate_p, writable_p);
		return;
	} else if (npoints >= MAXPOINTS) {
		npoints = MAXPOINTS - 1;
	}
      AGAIN:

	{
		double      DH[MAXPOINTS];	/* Distance between H values in the shortest

						   direction around the circle, that is, the
						   distance between 10 and 350 is 20.
						   (Range is 0 - 360.0.)
						 */
		double      edge[MAXPOINTS];	/* lengths of edges in unit HSV space. */
		double      ratio[MAXPOINTS];	/* proportions of the edges (total 1.0) */
		double      circum = 0;
		double      one_point_oh = 0;	/* (debug) */

		for (i = 0; i < npoints; i++) {
			int         j = (i + 1) % npoints;
			double      d = ((double) (h[i] - h[j])) / 360;

			if (d < 0)
				d = -d;
			if (d > 0.5)
				d = 0.5 - (d - 0.5);
			DH[i] = d;
		}

		for (i = 0; i < npoints; i++) {
			int         j = (i + 1) % npoints;

			edge[i] = sqrt((DH[i] * DH[j]) +
				       ((s[j] - s[i]) * (s[j] - s[i])) +
				       ((v[j] - v[i]) * (v[j] - v[i])));
			circum += edge[i];
		}

#ifdef DEBUG
		(void) fprintf(stderr, "\ncolors:");
		for (i = 0; i < npoints; i++)
			(void) fprintf(stderr, " (%d, %.3f, %.3f)", h[i], s[i], v[i]);
		(void) fprintf(stderr, "\nlengths:");
		for (i = 0; i < npoints; i++)
			(void) fprintf(stderr, " %.3f", edge[i]);
#endif /* DEBUG */

		if (circum < 0.0001)
			goto FAIL;

		for (i = 0; i < npoints; i++) {
			ratio[i] = edge[i] / circum;
			one_point_oh += ratio[i];
		}

#ifdef DEBUG
		(void) fprintf(stderr, "\nratios:");
		for (i = 0; i < npoints; i++)
			(void) fprintf(stderr, " %.3f", ratio[i]);
#endif /* DEBUG */

		if (one_point_oh < 0.99999 || one_point_oh > 1.00001)
			abort();

		/* space the colors evenly along the circumference -- that means that the
		   number of pixels on a edge is proportional to the length of that edge
		   (relative to the lengths of the other edges.)
		 */
		for (i = 0; i < npoints; i++)
			ncolors[i] = (int) (total_ncolors * ratio[i]);


#ifdef DEBUG
		(void) fprintf(stderr, "\npixels:");
		for (i = 0; i < npoints; i++)
			(void) fprintf(stderr, " %d", ncolors[i]);
		(void) fprintf(stderr, "  (%d)\n", total_ncolors);
#endif /* DEBUG */

		for (i = 0; i < npoints; i++) {
			int         j = (i + 1) % npoints;

			if (ncolors[i] > 0) {
				dh[i] = 360 * (DH[i] / ncolors[i]);
				ds[i] = (s[j] - s[i]) / ncolors[i];
				dv[i] = (v[j] - v[i]) / ncolors[i];
			}
		}
	}

	(void) memset(colors, 0, (*ncolorsP) * sizeof (*colors));

	k = 0;
	for (i = 0; i < npoints; i++) {
		int         distance, direction, j;

		distance = h[(i + 1) % npoints] - h[i];
		direction = (distance >= 0 ? -1 : 1);

		if (distance > 180)
			distance = 180 - (distance - 180);
		else if (distance < -180)
			distance = -(180 - ((-distance) - 180));
		else
			direction = -direction;

#ifdef DEBUG
		(void) fprintf(stderr, "point %d: %3d %.2f %.2f\n",
			i, h[i], s[i], v[i]);
		(void) fprintf(stderr, "  h[i]=%d  dh[i]=%.2f  ncolors[i]=%d\n",
			h[i], dh[i], ncolors[i]);
#endif /* DEBUG */
		for (j = 0; j < ncolors[i]; j++, k++) {
			double      hh = (h[i] + (j * dh[i] * direction));

			if (hh < 0)
				hh += 360;
			else if (hh > 360)
				hh -= 0;
			colors[k].flags = DoRed | DoGreen | DoBlue;
			hsv_to_rgb((int)
				   hh,
				   (s[i] + (j * ds[i])),
				   (v[i] + (j * dv[i])),
			  &colors[k].red, &colors[k].green, &colors[k].blue);
#ifdef DEBUG
			(void) fprintf(stderr, "point %d+%d: %.2f %.2f %.2f  %04X %04X %04X\n",
				i, j,
				hh,
				(s[i] + (j * ds[i])),
				(v[i] + (j * dv[i])),
			     colors[k].red, colors[k].green, colors[k].blue);
#endif /* DEBUG */
		}
	}

	/* Floating-point round-off can make us decide to use fewer colors. */
	if (k < *ncolorsP) {
		*ncolorsP = k;
		if (k <= 0)
			return;
	}
	if (!allocate_p)
		return;

	if (writable_p) {
		unsigned long *pixels = (unsigned long *)
		malloc(sizeof (unsigned long) * ((*ncolorsP) + 1));

		/* allocate_writable_colors() won't do here, because we need exactly this
		   number of cells, or the color sequence we've chosen won't fit. */
		if (!XAllocColorCells(dpy, cmap, False, 0, 0, pixels, *ncolorsP)) {
			(void) free((void *) pixels);
			goto FAIL;
		}
		for (i = 0; i < *ncolorsP; i++)
			colors[i].pixel = pixels[i];
		(void) free((void *) pixels);

		XStoreColors(dpy, cmap, colors, *ncolorsP);
	} else {
		for (i = 0; i < *ncolorsP; i++) {
			XColor      color;

			color = colors[i];
			if (XAllocColor(dpy, cmap, &color)) {
				colors[i].pixel = color.pixel;
			} else {
				free_colors(dpy, cmap, colors, i);
				goto FAIL;
			}
		}
	}

	return;

      FAIL:
	/* we weren't able to allocate all the colors we wanted;
	   decrease the requested number and try again.
	 */
	total_ncolors = (total_ncolors > 170 ? total_ncolors - 20 :
			 total_ncolors > 100 ? total_ncolors - 10 :
			 total_ncolors > 75 ? total_ncolors - 5 :
			 total_ncolors > 25 ? total_ncolors - 3 :
			 total_ncolors > 10 ? total_ncolors - 2 :
			 total_ncolors > 2 ? total_ncolors - 1 :
			 0);
	*ncolorsP = total_ncolors;
	if (total_ncolors > 0)
		goto AGAIN;
}


void
make_color_loop(Display * dpy, Colormap cmap,
		int h0, double s0, double v0,	/* 0-360, 0-1.0, 0-1.0 */
		int h1, double s1, double v1,	/* 0-360, 0-1.0, 0-1.0 */
		int h2, double s2, double v2,	/* 0-360, 0-1.0, 0-1.0 */
		XColor * colors, int *ncolorsP,
		Bool allocate_p,
		Bool writable_p)
{
	int         h[3];
	double      s[3], v[3];

	h[0] = h0;
	h[1] = h1;
	h[2] = h2;
	s[0] = s0;
	s[1] = s1;
	s[2] = s2;
	v[0] = v0;
	v[1] = v1;
	v[2] = v2;
	make_color_path(dpy, cmap,
			3, h, s, v,
			colors, ncolorsP,
			allocate_p, writable_p);
}


static void
complain(int wanted_colors, int got_colors,
	 Bool wanted_writable, Bool got_writable)
{
	if (wanted_writable && !got_writable)
		(void) fprintf(stderr,
		 "%s: wanted %d writable colors; got %d read-only colors.\n",
			ProgramName, wanted_colors, got_colors);

	else if (wanted_colors > (got_colors + 10))
		/* don't bother complaining if we're within ten pixels. */
		(void) fprintf(stderr, "%s: wanted %d%s colors; got %d.\n",
		  ProgramName, wanted_colors, (got_writable ? " writable" : ""),
			got_colors);
}


void
make_smooth_colormap(ModeInfo * mi, Colormap cmap,
		     XColor * colors, int *ncolorsP,
		     Bool allocate_p,
		     Bool * writable_pP)
{
	int         npoints;
	int         ncolors = *ncolorsP;
	Bool        wanted_writable = (allocate_p && writable_pP && *writable_pP);
	int         i;
	int         h[MAXPOINTS];
	double      s[MAXPOINTS];
	double      v[MAXPOINTS];
	double      total_s = 0;
	double      total_v = 0;

	if (*ncolorsP <= 0)
		return;

	{
		int         n = (int) (LRAND() % 20);

		if (n <= 5)
			npoints = 2;	/* 30% of the time */
		else if (n <= 15)
			npoints = 3;	/* 50% of the time */
		else if (n <= 18)
			npoints = 4;	/* 15% of the time */
		else
			npoints = 5;	/*  5% of the time */
	}

      REPICK_ALL_COLORS:
	for (i = 0; i < npoints; i++) {
	      REPICK_THIS_COLOR:
		h[i] = (int) (LRAND() % 360);
		s[i] = LRAND() / MAXRAND;
		v[i] = 0.8 * LRAND() / MAXRAND + 0.2;

		/* Make sure that no two adjascent colors are *too* close together.
		   If they are, try again.
		 */
		if (i > 0) {
			int         j = (i + 1 == npoints) ? 0 : (i - 1);
			double      hi = ((double) h[i]) / 360;
			double      hj = ((double) h[j]) / 360;
			double      dh = hj - hi;
			double      distance;

			if (dh < 0)
				dh = -dh;
			if (dh > 0.5)
				dh = 0.5 - (dh - 0.5);
			distance = sqrt((dh * dh) +
					((s[j] - s[i]) * (s[j] - s[i])) +
					((v[j] - v[i]) * (v[j] - v[i])));
			if (distance < 0.2)
				goto REPICK_THIS_COLOR;
		}
		total_s += s[i];
		total_v += v[i];
	}

	/* If the average saturation or intensity are too low, repick the colors,
	   so that we don't end up with a black-and-white or too-dark map.
	 */
	if (total_s / npoints < 0.2)
		goto REPICK_ALL_COLORS;
	if (total_v / npoints < 0.3)
		goto REPICK_ALL_COLORS;

	/* If this visual doesn't support writable cells, don't bother trying.
	 */
	if (wanted_writable && !has_writable_cells(mi))
		*writable_pP = False;

      RETRY_NON_WRITABLE:
	make_color_path(MI_DISPLAY(mi), cmap, npoints, h, s, v, colors, &ncolors,
			allocate_p, (writable_pP && *writable_pP));

	/* If we tried for writable cells and got none, try for non-writable. */
	if (allocate_p && *ncolorsP == 0 && *writable_pP) {
		*writable_pP = False;
		goto RETRY_NON_WRITABLE;
	}
	if (MI_IS_VERBOSE(mi) || MI_IS_DEBUG(mi))
		complain(*ncolorsP, ncolors, wanted_writable,
			 wanted_writable && *writable_pP);

	*ncolorsP = ncolors;
}


void
make_uniform_colormap(ModeInfo * mi, Colormap cmap,
		      XColor * colors, int *ncolorsP,
		      Bool allocate_p,
		      Bool * writable_pP)
{
	int         ncolors = *ncolorsP;
	Bool        wanted_writable = (allocate_p && writable_pP && *writable_pP);

	double      S = ((double) (LRAND() % 34) + 66) / 100.0;		/* range 66%-100% */
	double      V = ((double) (LRAND() % 34) + 66) / 100.0;		/* range 66%-100% */

	if (*ncolorsP <= 0)
		return;

	/* If this visual doesn't support writable cells, don't bother trying. */
	if (wanted_writable && !has_writable_cells(mi))
		*writable_pP = False;

      RETRY_NON_WRITABLE:
	make_color_ramp(MI_DISPLAY(mi), cmap,
			0, S, V,
			359, S, V,
			colors, &ncolors,
			False, True, wanted_writable);

	/* If we tried for writable cells and got none, try for non-writable. */
	if (allocate_p && *ncolorsP == 0 && writable_pP && *writable_pP) {
		ncolors = *ncolorsP;
		*writable_pP = False;
		goto RETRY_NON_WRITABLE;
	}
	if (MI_IS_VERBOSE(mi) || MI_IS_DEBUG(mi))
		complain(*ncolorsP, ncolors, wanted_writable,
			 wanted_writable && *writable_pP);

	*ncolorsP = ncolors;
}


void
make_random_colormap(ModeInfo * mi, Colormap cmap,
		     XColor * colors, int *ncolorsP,
		     Bool bright_p,
		     Bool allocate_p,
		     Bool * writable_pP)
{
	Bool        wanted_writable = (allocate_p && writable_pP && *writable_pP);
	int         ncolors = *ncolorsP;
	int         i;

	if (*ncolorsP <= 0)
		return;

	/* If this visual doesn't support writable cells, don't bother trying. */
	if (wanted_writable && !has_writable_cells(mi))
		*writable_pP = False;

	for (i = 0; i < ncolors; i++) {
		colors[i].flags = DoRed | DoGreen | DoBlue;
		if (bright_p) {
			int         H = (int) LRAND() % 360;	/* range 0-360    */
			double      S = ((double) (LRAND() % 70) + 30) / 100.0;		/* range 30%-100% */
			double      V = ((double) (LRAND() % 34) + 66) / 100.0;		/* range 66%-100% */

			hsv_to_rgb(H, S, V,
			  &colors[i].red, &colors[i].green, &colors[i].blue);
		} else {
			colors[i].red = (unsigned short) (LRAND() % 0xFFFF);
			colors[i].green = (unsigned short) (LRAND() % 0xFFFF);
			colors[i].blue = (unsigned short) (LRAND() % 0xFFFF);
		}
	}

	if (!allocate_p)
		return;

      RETRY_NON_WRITABLE:
	if (writable_pP && *writable_pP) {
		unsigned long *pixels = (unsigned long *)
		malloc(sizeof (unsigned long) * (ncolors + 1));

		allocate_writable_colors(MI_DISPLAY(mi), cmap, pixels, &ncolors);
		if (ncolors > 0)
			for (i = 0; i < ncolors; i++)
				colors[i].pixel = pixels[i];
		(void) free((void *) pixels);
		if (ncolors > 0)
			XStoreColors(MI_DISPLAY(mi), cmap, colors, ncolors);
	} else {
		for (i = 0; i < ncolors; i++) {
			XColor      color;

			color = colors[i];
			if (!XAllocColor(MI_DISPLAY(mi), cmap, &color))
				break;
			colors[i].pixel = color.pixel;
		}
		ncolors = i;
	}

	/* If we tried for writable cells and got none, try for non-writable. */
	if (allocate_p && ncolors == 0 && writable_pP && *writable_pP) {
		ncolors = *ncolorsP;
		*writable_pP = False;
		goto RETRY_NON_WRITABLE;
	}
	if (MI_IS_VERBOSE(mi) || MI_IS_DEBUG(mi))
		complain(*ncolorsP, ncolors, wanted_writable,
			 wanted_writable && *writable_pP);

	*ncolorsP = ncolors;
}


void
rotate_colors(Display * dpy, Colormap cmap,
	      XColor * colors, int ncolors, int distance)
{
	int         i;
	XColor     *colors2 = (XColor *) malloc(sizeof (XColor) * ncolors);

	if (ncolors < 2)
		return;
	distance = distance % ncolors;
	for (i = 0; i < ncolors; i++) {
		int         j = i - distance;

		if (j >= ncolors)
			j -= ncolors;
		if (j < 0)
			j += ncolors;
		colors2[i] = colors[j];
		colors2[i].pixel = colors[i].pixel;
	}
	XStoreColors(dpy, cmap, colors2, ncolors);
	XFlush(dpy);
	(void) memcpy((char *) colors, colors2, sizeof (*colors) * ncolors);
	(void) free((void *) colors2);
}

/* xscreensaver, Copyright (c) 1992, 1997 Jamie Zawinski <jwz@jwz.org>

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/* This file contains some utility routines for randomly picking the colors
   to hack the screen with.
 */

void
hsv_to_rgb(int h, double s, double v,
	   unsigned short *r, unsigned short *g, unsigned short *b)
{
	double      H, S, V, R, G, B;
	double      p1, p2, p3;
	double      f;
	int         i;

	if (s < 0)
		s = 0;
	if (v < 0)
		v = 0;
	if (s > 1)
		s = 1;
	if (v > 1)
		v = 1;

	S = s;
	V = v;
	H = (h % 360) / 60.0;
	i = (int) H;
	f = H - i;
	p1 = V * (1 - S);
	p2 = V * (1 - (S * f));
	p3 = V * (1 - (S * (1 - f)));
	if (i == 0) {
		R = V;
		G = p3;
		B = p1;
	} else if (i == 1) {
		R = p2;
		G = V;
		B = p1;
	} else if (i == 2) {
		R = p1;
		G = V;
		B = p3;
	} else if (i == 3) {
		R = p1;
		G = p2;
		B = V;
	} else if (i == 4) {
		R = p3;
		G = p1;
		B = V;
	} else {
		R = V;
		G = p1;
		B = p2;
	}
	*r = (short unsigned int) (R * 65535);
	*g = (short unsigned int) (G * 65535);
	*b = (short unsigned int) (B * 65535);
}

void
rgb_to_hsv(unsigned short r, unsigned short g, unsigned short b,
	   int *h, double *s, double *v)
{
	double      R, G, B, H, S, V;
	double      cmax, cmin;
	double      cmm;
	int         imax;

	R = ((double) r) / 65535.0;
	G = ((double) g) / 65535.0;
	B = ((double) b) / 65535.0;
	cmax = R;
	cmin = G;
	imax = 1;
	if (cmax < G) {
		cmax = G;
		cmin = R;
		imax = 2;
	}
	if (cmax < B) {
		cmax = B;
		imax = 3;
	}
	if (cmin > B) {
		cmin = B;
	}
	cmm = cmax - cmin;
	V = cmax;
	if (cmm == 0)
		S = H = 0;
	else {
		S = cmm / cmax;
		if (imax == 1)
			H = (G - B) / cmm;
		else if (imax == 2)
			H = 2.0 + (B - R) / cmm;
		else		/*if (imax == 3) */
			H = 4.0 + (R - G) / cmm;
		if (H < 0)
			H += 6.0;
	}
	*h = (int) (H * 60.0);
	*s = S;
	*v = V;
}
