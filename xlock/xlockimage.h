#if !defined( lint ) && !defined( SABER )
/* #ident        "@(#)xlockimage.h      4.18 00/10/26 xlockmore" */

#endif

/*-
 * Utilities for Sun rasterfile processing
 *
 * Copyright (c) 1995 by Tobias Gloth
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 12-Dec-95: Modified to be a used in more than one mode
 *            <joukj@hrem.stm.tudelft.nl>
 * 22-May-95: Written.
 */

extern void SetImageColors(Display * display, Colormap cmap);
#if 0
extern unsigned long GetBlack(void);
extern unsigned long GetWhite(void);
#endif
extern unsigned long GetColor(ModeInfo * mi, unsigned long pixel);
extern unsigned long find_nearest(int r, int g, int b);
extern void convert_colors(ModeInfo * mi);

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
} XLockImage;

extern XLockImage xlockimage;
