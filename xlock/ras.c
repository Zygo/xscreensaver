#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)ras.c	4.00 97/01/01 xlockmore";

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
 *            <joukj@crys.chem.uva.nl>
 * 22-May-95: Written.
 */

#include "xlock.h"
#include <time.h>
#include "ras.h"

extern char *imagefile;

#define COLORMAP_SIZE 0x100
#define RGBCOLORMAP_SIZE 0x300
typedef struct {
	int         entry[COLORMAP_SIZE];
	unsigned char header[32];
	unsigned char color[RGBCOLORMAP_SIZE];
	unsigned char red[COLORMAP_SIZE], green[COLORMAP_SIZE], blue[COLORMAP_SIZE];
	unsigned char *data;
	unsigned long sign;
	unsigned long width, height;
	unsigned long depth;
	unsigned long colors;
} raster;

static raster ras;

static unsigned long find_nearest(int r, int g, int b);
static unsigned long get_long(int n);

static void
analyze_header(void)
{
	ras.sign = get_long(0);
	ras.width = get_long(1);
	ras.height = get_long(2);
	ras.depth = get_long(3);
	ras.colors = get_long(7) / 3;
}

static void
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
		ras.red[fgpix] = fgcol.red >> 8;
		ras.green[fgpix] = fgcol.green >> 8;
		ras.blue[fgpix] = fgcol.blue >> 8;
	}
	if (bgpix < COLORMAP_SIZE) {
		ras.red[bgpix] = bgcol.red >> 8;
		ras.green[bgpix] = bgcol.green >> 8;
		ras.blue[bgpix] = bgcol.blue >> 8;
	}
	if (white < COLORMAP_SIZE) {
		ras.red[white] = 0xFF;
		ras.green[white] = 0xFF;
		ras.blue[white] = 0xFF;
	}
	if (black < COLORMAP_SIZE) {
		ras.red[black] = 0;
		ras.green[black] = 0;
		ras.blue[black] = 0;
	}
	/* Set up raster colors */
	/* Could try juggling this stuff around so it does not conflict
	   with fg,bg,white,black */
	for (i = 0; i < (int) ras.colors; i++) {
		ras.red[i] = ras.color[i + 0 * ras.colors];
		ras.green[i] = ras.color[i + 1 * ras.colors];
		ras.blue[i] = ras.color[i + 2 * ras.colors];
	}

	/* Make sure there is a black ... */
	if (ras.colors <= 0xff)
		ras.red[0xff] = ras.green[0xff] = ras.blue[0xff] = 0;
}

static unsigned long
find_nearest(int r, int g, int b)
{
	unsigned long i, minimum = RGBCOLORMAP_SIZE, imin = 0, t;

	for (i = 0; i < COLORMAP_SIZE; i++) {
		t = abs(r - (int) ras.red[i]) + abs(g - (int) ras.green[i]) +
			abs(b - (int) ras.blue[i]);
		if (t < minimum) {
			minimum = t;
			imin = i;
		}
	}
#ifdef DEBUG
	(void) fprintf(stderr, "giving (%d/%d/%d)[%ld] for (%d/%d/%d) diff %ld\n",
		       ras.red[imin], ras.green[imin], ras.blue[imin], imin, r, g, b, minimum);
#endif
	return imin;
}

unsigned long
GetBlack(void)
{
	return ras.entry[find_nearest(0, 0, 0)];
}

unsigned long
GetWhite(void)
{
	return ras.entry[find_nearest(0xff, 0xff, 0xff)];
}

unsigned long
GetColor(ModeInfo * mi, unsigned long pixel)
{
	XColor      col;

	col.pixel = pixel;
	XQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &col);
	return ras.entry[find_nearest(col.red >> 8, col.green >> 8, col.blue >> 8)];
}

static unsigned long
get_long(int n)
{
	return
		(((unsigned long) ras.header[4 * n + 0]) << 24) +
		(((unsigned long) ras.header[4 * n + 1]) << 16) +
		(((unsigned long) ras.header[4 * n + 2]) << 8) +
		(((unsigned long) ras.header[4 * n + 3]) << 0);
}

int
RasterFileToImage(ModeInfo * mi, char *filename, XImage ** image)
{
	int         read_width;
	FILE       *file;

	if ((file = my_fopen(filename, "r")) == NULL) {
		/*(void) fprintf(stderr, "could not read file \"%s\"\n", filename); */
		return RasterOpenFailed;
	}
	(void) fread((void *) ras.header, 8, 4, file);
	analyze_header();
	if (ras.sign != 0x59a66a95) {
		/* not a raster file */
		(void) fclose(file);
		return RasterFileInvalid;
	}
	if (ras.depth != 8) {
		(void) fclose(file);
		(void) fprintf(stderr, "only 8-bit Raster files are supported\n");
		return RasterColorFailed;
	}
	read_width = ras.width;
	if ((ras.width & 1) != 0)
		read_width++;
	ras.data = (unsigned char *) malloc((int) (read_width * ras.height));
	if (!ras.data) {
		(void) fclose(file);
		(void) fprintf(stderr, "out of memory for Raster file\n");
		return RasterNoMemory;
	}
	*image = XCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi),
			      8, ZPixmap, 0,
			(char *) ras.data, (int) ras.width, (int) ras.height,
			      16, (int) read_width);
	if (!*image) {
		(void) fclose(file);
		(void) fprintf(stderr, "could not create image from Raster file\n");
		return RasterColorError;
	}
	(void) fread((void *) ras.color, (int) ras.colors, 3, file);
	(void) fread((void *) ras.data, read_width, (int) ras.height, file);
	(void) fclose(file);
	convert_colors(mi);
	return RasterSuccess;
}

void
SetImageColors(Display * display, Colormap cmap)
{
	XColor      xcolor[COLORMAP_SIZE];
	int         i;

	for (i = 0; i < COLORMAP_SIZE; i++) {
		xcolor[i].flags = DoRed | DoGreen | DoBlue;
		xcolor[i].red = ras.red[i] << 8;
		xcolor[i].green = ras.green[i] << 8;
		xcolor[i].blue = ras.blue[i] << 8;
		if (!XAllocColor(display, cmap, xcolor + i))
			error("not enough colors.\n");
		ras.entry[i] = xcolor[i].pixel;
	}
/* for (i = 0; i < ras.width * ras.height; i++) ras.data[i] =
   ras.entry[ras.data[i]]; */
}
