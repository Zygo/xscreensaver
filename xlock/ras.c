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
 *            <joukj@hrem.stm.tudelft.nl>
 * 22-May-95: Written.
 */

#include "xlock.h"
#include "iostuff.h"
#include "ras.h"
#include <time.h>

XLockImage xlockimage;

static unsigned long get_long(int n);

static void
analyze_header(void)
{
	xlockimage.sign = get_long(0);
	xlockimage.width = get_long(1);
	xlockimage.height = get_long(2);
	xlockimage.depth = get_long(3);
	xlockimage.colors = get_long(7) / 3;
}

static unsigned long
get_long(int n)
{
	return
		(((unsigned long) xlockimage.header[4 * n + 0]) << 24) +
		(((unsigned long) xlockimage.header[4 * n + 1]) << 16) +
		(((unsigned long) xlockimage.header[4 * n + 2]) << 8) +
		(((unsigned long) xlockimage.header[4 * n + 3]) << 0);
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
	(void) fread((void *) xlockimage.header, 8, 4, file);
	analyze_header();
	if (xlockimage.sign != 0x59a66a95) {
		/* not a raster file */
		(void) fclose(file);
		return RasterFileInvalid;
	}
	if (xlockimage.depth != 8) {
		(void) fclose(file);
		(void) fprintf(stderr, "only 8-bit Raster files are supported\n");
		return RasterColorFailed;
	}
	read_width = (int) xlockimage.width;
	if ((xlockimage.width & 1) != 0)
		read_width++;
	xlockimage.data = (unsigned char *) malloc((int) (read_width * xlockimage.height));
	if (!xlockimage.data) {
		(void) fclose(file);
		(void) fprintf(stderr, "out of memory for Raster file\n");
		return RasterNoMemory;
	}
	*image = XCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi),
			      8, ZPixmap, 0,
			(char *) xlockimage.data, (int) xlockimage.width, (int) xlockimage.height,
			      16, (int) read_width);
	if (!*image) {
		(void) fclose(file);
		(void) fprintf(stderr, "could not create image from Raster file\n");
		return RasterColorError;
	}
	(void) fread((void *) xlockimage.color, (int) xlockimage.colors, 3, file);
	(void) fread((void *) xlockimage.data, read_width, (int) xlockimage.height, file);
	(void) fclose(file);
	convert_colors(mi);
	return RasterSuccess;
}
