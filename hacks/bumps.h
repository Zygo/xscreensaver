/* Bumps, Copyright (c) 2001 Shane Smit <CodeWeaver@DigitalLoom.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Module: "Bumps.h"
 * Tab Size: 4
 *
 * Description:
 *  Header file for module "Bumps.c"
 *
 * Modification History:
 *  [10/01/99] - Shane Smit: Creation
 *  [10/08/99] - Shane Smit: Port to C. (Ick)
 *  [03/08/02] - Shane Smit: New movement code.
 *  [09/12/02] - Shane Smit: MIT-SHM XImages.
 * 							 Thanks to Kennett Galbraith <http://www.Alpha-II.com/>
 * 							 for code optimization.
 */


#ifndef _BUMPS_H
#define _BUMPS_H


#include <math.h>
#include "screenhack.h"

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */


/* Defines: */
/* #define VERBOSE */
#define RANDOM() ((int) (random() & 0X7FFFFFFFL))

typedef signed char		int8_;
typedef unsigned char	uint8_;
typedef short			int16_;
typedef unsigned short	uint16_;
typedef long			int32_;
typedef unsigned long	uint32_;
typedef unsigned char	BOOL;


/* Globals: */

static const char *bumps_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*color:		random",
  "*colorcount:	64",
  "*delay:		30000",
  "*duration:	120",
  "*soften:		1",
  "*invert:		FALSE",
#ifdef __sgi    /* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:	Best",
#endif
#ifdef HAVE_XSHM_EXTENSION
  "*useSHM:		True",
#endif /* HAVE_XSHM_EXTENSION */
  0
};

static XrmOptionDescRec bumps_options [] = {
  { "-color",		".color",		XrmoptionSepArg, 0 },
  { "-colorcount",	".colorcount",	XrmoptionSepArg, 0 },
  { "-duration",	".duration",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-soften",		".soften",		XrmoptionSepArg, 0 },
  { "-invert",		".invert",		XrmoptionNoArg, "TRUE" },
#ifdef HAVE_XSHM_EXTENSION
  { "-shm",			".useSHM",		XrmoptionNoArg, "True" },
  { "-no-shm",		".useSHM",		XrmoptionNoArg, "False" },
#endif /* HAVE_XSHM_EXTENSION */

  { 0, 0, 0, 0 }
};


/* This structure handles everything to do with the spotlight, and is designed to be
 * a member of TBumps. */
typedef struct
{
	uint8_ *aLightMap;
	uint16_ nFalloffDiameter, nFalloffRadius;
	uint16_ nLightDiameter, nLightRadius;
	float nAccelX, nAccelY;
	float nAccelMax;
	float nVelocityX, nVelocityY;
	float nVelocityMax;
	float nXPos, nYPos;
} SSpotLight;

void CreateTables( SSpotLight * );


/* The entire program's operation is contained within this structure. */
typedef struct
{
	/* XWindows specific variables. */
	Display *dpy;
	Window Win;
	Screen *screen;
        Pixmap source;
	GC GraphicsContext;
	XColor *xColors;
	uint32_ *aColors;
	XImage *pXImage;
#ifdef HAVE_XSHM_EXTENSION
	XShmSegmentInfo XShmInfo;
	Bool	bUseShm;
#endif /* HAVE_XSHM_EXTENSION */

	uint8_ nColorCount;				/* Number of colors used. */
	uint8_ bytesPerPixel;
	uint16_ iWinWidth, iWinHeight;
	uint16_ *aBumpMap;				/* The actual bump map. */
	SSpotLight SpotLight;

        int delay;
        int duration;
        time_t start_time;

        async_load_state *img_loader;
} SBumps;


#endif /* _BUMPS_H */


/* vim: ts=4
 */
