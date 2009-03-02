/*-
 * @(#)ras.h	3.9 96/05/25 xlockmore
 *
 * Utilities for Sun rasterfile processing
 *
 * Copyright (c) 1995 by Tobias Gloth
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 12-Dec-95: Modified to be a used in more than one mode
 *            <joukj@crys.chem.uva.nl>
 * 22-May-95: Written.
 */

#define RasterColorError   1
#define RasterSuccess      0
#define RasterOpenFailed  -1
#define RasterFileInvalid -2
#define RasterNoMemory    -3
#define RasterColorFailed -4

extern int  RasterFileToImage(ModeInfo * mi, char *filename, XImage ** image);
extern void SetImageColors(Display * display, Colormap cmap);
extern unsigned long GetBlack(void);
extern unsigned long GetWhite(void);
extern unsigned long GetColor(ModeInfo * mi, unsigned long pixel);
