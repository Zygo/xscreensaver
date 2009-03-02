#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xlockimage.c	4.19 00/10/26 xlockmore";

#endif

/*-
 * Utilities for Sun rasterfile processing
 *
 * Copyright (c) 1995 by Tobias Gloth
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 *  3-Mar-96: Added random image selection.
 * 12-Dec-95: Modified to be a used in more than one mode
 *            <joukj@hrem.stm.tudelft.nl>
 * 22-May-95: Written.
 */

#include "xlock.h"
#include "xlockimage.h"
#include <time.h>

void
convert_colors(ModeInfo * mi)
{
	int         i;
	unsigned long black, white, fgpix, bgpix;
	XColor      fgcol, bgcol;

	black = MI_BLACK_PIXEL(mi);
	white = MI_WHITE_PIXEL(mi);
	fgpix = MI_FG_PIXEL(mi);
	bgpix = MI_BG_PIXEL(mi);
	fgcol.pixel = fgpix;
	bgcol.pixel = bgpix;
	XQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &fgcol);
	XQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &bgcol);

	/* Set these, if raster does not overwrite some, so much the better. */
	if (fgpix < COLORMAP_SIZE) {
		xlockimage.red[fgpix] = fgcol.red >> 8;
		xlockimage.green[fgpix] = fgcol.green >> 8;
		xlockimage.blue[fgpix] = fgcol.blue >> 8;
	}
	if (bgpix < COLORMAP_SIZE) {
		xlockimage.red[bgpix] = bgcol.red >> 8;
		xlockimage.green[bgpix] = bgcol.green >> 8;
		xlockimage.blue[bgpix] = bgcol.blue >> 8;
	}
	if (white < COLORMAP_SIZE) {
		xlockimage.red[white] = 0xFF;
		xlockimage.green[white] = 0xFF;
		xlockimage.blue[white] = 0xFF;
	}
	if (black < COLORMAP_SIZE) {
		xlockimage.red[black] = 0;
		xlockimage.green[black] = 0;
		xlockimage.blue[black] = 0;
	}
	/* Set up raster colors */
	/* Could try juggling this stuff around so it does not conflict
	   with fg,bg,white,black */
	for (i = 0; i < (int) xlockimage.colors; i++) {
		xlockimage.red[i] = xlockimage.color[i + 0 * xlockimage.colors];
		xlockimage.green[i] = xlockimage.color[i + 1 * xlockimage.colors];
		xlockimage.blue[i] = xlockimage.color[i + 2 * xlockimage.colors];
	}

	/* Make sure there is a black ... */
	if (xlockimage.colors <= 0xff)
		xlockimage.red[0xff] = xlockimage.green[0xff] = xlockimage.blue[0xff] = 0;
}

unsigned long
find_nearest(int r, int g, int b)
{
	unsigned long i, minimum = RGBCOLORMAP_SIZE, imin = 0, t;

	for (i = 0; i < COLORMAP_SIZE; i++) {
		t = abs(r - (int) xlockimage.red[i]) + abs(g - (int) xlockimage.green[i]) +
			abs(b - (int) xlockimage.blue[i]);
		if (t < minimum) {
			minimum = t;
			imin = i;
		}
	}
#ifdef DEBUG
	(void) fprintf(stderr, "giving (%d/%d/%d)[%ld] for (%d/%d/%d) diff %ld\n",
		       xlockimage.red[imin], xlockimage.green[imin], xlockimage.blue[imin], imin, r, g, b, minimum);
#endif
	return imin;
}

#if 0
unsigned long
GetBlack(void)
{
	return xlockimage.entry[find_nearest(0, 0, 0)];
}

unsigned long
GetWhite(void)
{
	return xlockimage.entry[find_nearest(0xff, 0xff, 0xff)];
}
#endif

unsigned long
GetColor(ModeInfo * mi, unsigned long pixel)
{
	XColor      col;

	col.pixel = pixel;
	XQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &col);
	return xlockimage.entry[find_nearest(col.red >> 8, col.green >> 8, col.blue >> 8)];
}

void
SetImageColors(Display * display, Colormap cmap)
{
	XColor      xcolor[COLORMAP_SIZE];
	int         i;

	for (i = 0; i < COLORMAP_SIZE; i++) {
		xcolor[i].flags = DoRed | DoGreen | DoBlue;
		xcolor[i].red = xlockimage.red[i] << 8;
		xcolor[i].green = xlockimage.green[i] << 8;
		xcolor[i].blue = xlockimage.blue[i] << 8;
		if (!XAllocColor(display, cmap, xcolor + i))
			error("not enough colors.\n");
		xlockimage.entry[i] = (int) xcolor[i].pixel;
	}
/* for (i = 0; i < xlockimage.width * xlockimage.height; i++) xlockimage.data[i] =
   xlockimage.entry[xlockimage.data[i]]; */
}
