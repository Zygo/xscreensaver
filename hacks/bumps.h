/* Bumps, Copyright (c) 1999 Shane Smit <blackend@inconnect.com>
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
 *  Header file for module "Bumps.cpp"
 *
 * Modification History:
 *  [10/01/99] - Shane Smit: Creation
 *  [10/08/99] - Shane Smit: Port to C. (Ick)
 */


#ifndef _BUMPS_H
#define _BUMPS_H


#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>


/* Defines: */
/* #define VERBOSE */
#define PI		3.141592654
#define RANDOM() ((int) (random() & 0X7FFFFFFFL))

typedef char			int8_;
typedef unsigned char	uint8_;
typedef short			int16_;
typedef unsigned short	uint16_;
typedef long			int32_;
typedef unsigned long	uint32_;
typedef unsigned char	BOOL;


// Globals:
char *progclass = "Bumps";

char *defaults [] = {
  "*degrees:	360",
  "*color:		random",
  "*colorcount:	64",
  "*delay:		50000",
  "*soften:		1",
  "*invert:		FALSE",
  0
};

XrmOptionDescRec options [] = {
  { "-degrees",		".degrees",		XrmoptionSepArg, 0 },
  { "-color",		".color",		XrmoptionSepArg, 0 },
  { "-colorcount",	".colorcount",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-soften",		".soften",		XrmoptionSepArg, 0 },
  { "-invert",		".invert",		XrmoptionNoArg, "TRUE" },
  { 0, 0, 0, 0 }
};


/* This structure handles everything to do with the spotlight, and is designed to be
 * a member of TBumps. */
typedef struct
{
	uint16_ nDegreeCount;
	double *aSinTable;

	uint8_ *aLightMap;
	uint16_ nDiameter, nRadius;
	float nAngleX, nAngleY;		/* Spotlight's movement direction. */
	float nVelocityX, nVelocityY;
	uint16_ iWinXCenter, iWinYCenter;
} SSpotLight;

void CreateSpotLight( SSpotLight *, uint16_, uint16_ );
void CreateTables( SSpotLight * );
void CalcLightPos( SSpotLight *, uint16_ *, uint16_ * );
void DestroySpotLight( SSpotLight *pSpotLight ) { free( pSpotLight->aLightMap ); free( pSpotLight->aSinTable ); }


/* The entire program's operation is contained within this structure. */
typedef struct
{
	/* XWindows specific variables. */
	Display *pDisplay;
	Window Win;
	GC GraphicsContext;
	XColor *aXColors;
	XImage *pXImage;

	uint8_ nColorCount;				/* Number of colors used. */
	uint16_ iWinWidth, iWinHeight;
	uint16_ *aBumpMap;				/* The actual bump map. */

	SSpotLight SpotLight;
} SBumps;

void CreateBumps( SBumps *, Display *, Window );
void Execute( SBumps * );
void DestroyBumps( SBumps * );

void SetPalette( SBumps *, XWindowAttributes * );
void InitBumpMap( SBumps *, XWindowAttributes * );
void SoftenBumpMap( SBumps * );


#endif /* _BUMPS_H */


/*
 * End of Module: "Bumps.h"
 */

/* vim: ts=4
 */
