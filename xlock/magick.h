#if !defined( lint ) && !defined( SABER )
/* #ident        "@(#)magick.h      4.18 00/10/26 xlockmore" */

#endif

/*-
 * Utilities for Reading images using ImageMagickSun rasterfile processing
 *
 * Copyright (c) 2000 by J.Jansen
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 26-Oct-00: Created
 */

#ifdef USE_MAGICK
#include <time.h>
#include <sys/types.h>
#include <magick/api.h>

#include "xlockimage.h"

#define MagickColorError   1
#define MagickSuccess      0
#define MagickFileInvalid -2
#define MagickNoMemory    -3

extern int  MagickFileToImage(ModeInfo * mi, char *filename, XImage ** image);

#endif /* USE_MAGICK */
