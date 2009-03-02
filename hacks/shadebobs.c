/* shadebobs, Copyright (c) 1999 Shane Smit <blackend@inconnect.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Module - "shadebobs.c"
 *
 * Description:
 *  There are two little shading circles (bobs) that zip around the screen.
 *  one of them shades up towards white, and the other shades down toward
 *  black.
 *  This keeps the screen in color balance at a chosen color.
 *
 *  Its kinda like 'The Force'
 *   There is a light side, a dark side, and it keeps the world in balance.
 *
 * [05/23/99] - Shane Smit: Creation
 * [05/26/99] - Shane Smit: Port to C/screenhack for use with XScreenSaver
 * [06/11/99] - Shane Smit: Stopped trying to rape the palette.
 * [06/20/99] - jwz: cleaned up ximage handling, gave resoources short names,
 *                introduced delay, made it restart after N iterations.
 * [06/21/99] - Shane Smit: Modified default values slightly, color changes
 *                on cycle, and the extents of the sinus pattern change in
 *                real-time.
 * [06/22/99] - Shane Smit: Fixed delay to be fast and use little CPU :).
 * [09/17/99] - Shane Smit: Made all calculations based on the size of the
 *                window. Thus, it'll look the same at 100x100 as it does at
 *                1600x1200 ( Only smaller :).
 * [04/24/00] - Shane Smit: Revamped entire source code:
 *                Shade Bob movement is calculated very differently.
 *                Base color can be any color now.
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

/* #define VERBOSE */

char *progclass = "ShadeBobs";

char *defaults [] = {
  ".background: black",
  ".foreground: white",
  "*degrees:  0",	/* default: Automatic degree calculation */
  "*color:    random",
  "*count:    4",
  "*cycles:   10",
  "*ncolors:  64",    /* changing this doesn't work particularly well */
  "*delay:    5000",
  0
};

XrmOptionDescRec options [] = {
  { "-degrees", ".degrees", XrmoptionSepArg, 0 },
  { "-color",   ".color",   XrmoptionSepArg, 0 },
  { "-ncolors", ".ncolors", XrmoptionSepArg, 0 },
  { "-count",   ".count",   XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

static unsigned short iDegreeCount;
static double *anSinTable, *anCosTable;
static unsigned short iWinWidth, iWinHeight;
static unsigned short iWinCenterX, iWinCenterY;
static char *sColor;
static unsigned char iBobRadius, iBobDiameter; 
static unsigned char iVelocity;

#define RANDOM() ((int) (random() & 0X7FFFFFFFL))


/* Ahem. Chocolate is a flavor; not a food. Thank you */


typedef struct
{
	signed char *anDeltaMap;
	double nAngle, nAngleDelta, nAngleInc;
	double nPosX, nPosY;
} SShadeBob;


static void ResetShadeBob( SShadeBob *pShadeBob )
{
	pShadeBob->nPosX = RANDOM() % iWinWidth;
	pShadeBob->nPosY = RANDOM() % iWinHeight;
	pShadeBob->nAngle = RANDOM() % iDegreeCount;
	pShadeBob->nAngleDelta = ( RANDOM() % iDegreeCount ) - ( iDegreeCount / 2.0F );
	pShadeBob->nAngleInc = pShadeBob->nAngleDelta / 50.0F; 
	if( pShadeBob->nAngleInc == 0.0F )	
		pShadeBob->nAngleInc = ( pShadeBob->nAngleDelta > 0.0F ) ? 0.0001F : -0.0001F;
}


static void InitShadeBob( SShadeBob *pShadeBob, Bool bDark )
{
	double nDelta;
	int iWidth, iHeight;

	if( ( pShadeBob->anDeltaMap = calloc( iBobDiameter * iBobDiameter, sizeof(char) ) ) == NULL )
	{
		fprintf( stderr, "%s: Could not allocate Delta Map!\n", progclass );
		return;
	}

	for( iHeight=-iBobRadius; iHeight<iBobRadius; iHeight++ )
		for( iWidth=-iBobRadius; iWidth<iBobRadius; iWidth++ )
		{
			nDelta = 9 - ( ( sqrt( pow( iWidth+0.5, 2 ) + pow( iHeight+0.5, 2 ) ) / iBobRadius ) * 8 );
			if( nDelta < 0 )  nDelta = 0;
			if( bDark ) nDelta = -nDelta;
			pShadeBob->anDeltaMap[ ( iWidth + iBobRadius ) * iBobDiameter + iHeight + iBobRadius ] = (char)nDelta;
		}
  
	ResetShadeBob( pShadeBob );
}


/* A delta is calculated, and the shadebob turns at an increment.  When the delta
 * falls to 0, a new delta and increment are calculated. */
static void MoveShadeBob( SShadeBob *pShadeBob )
{
	pShadeBob->nAngle	   += pShadeBob->nAngleInc;
	pShadeBob->nAngleDelta -= pShadeBob->nAngleInc;

	if( pShadeBob->nAngle >= iDegreeCount )	pShadeBob->nAngle -= iDegreeCount;
	else if( pShadeBob->nAngle < 0 )		pShadeBob->nAngle += iDegreeCount;
	
	if( ( pShadeBob->nAngleInc>0.0F  && pShadeBob->nAngleDelta<pShadeBob->nAngleInc ) ||
	    ( pShadeBob->nAngleInc<=0.0F && pShadeBob->nAngleDelta>pShadeBob->nAngleInc ) )
	{
		pShadeBob->nAngleDelta = ( RANDOM() % iDegreeCount ) - ( iDegreeCount / 2.0F );
		pShadeBob->nAngleInc = pShadeBob->nAngleDelta / 50.0F;
		if( pShadeBob->nAngleInc == 0.0F )
			pShadeBob->nAngleInc = ( pShadeBob->nAngleDelta > 0.0F ) ? 0.0001F : -0.0001F;
   	}
	
	pShadeBob->nPosX = ( anSinTable[ (int)pShadeBob->nAngle ] * iVelocity ) + pShadeBob->nPosX;
	pShadeBob->nPosY = ( anCosTable[ (int)pShadeBob->nAngle ] * iVelocity ) + pShadeBob->nPosY;

	/* This wraps it around the screen. */
	if( pShadeBob->nPosX >= iWinWidth )	pShadeBob->nPosX -= iWinWidth;
	else if( pShadeBob->nPosX < 0 )		pShadeBob->nPosX += iWinWidth;
	
	if( pShadeBob->nPosY >= iWinHeight )	pShadeBob->nPosY -= iWinHeight;
	else if( pShadeBob->nPosY < 0 )			pShadeBob->nPosY += iWinHeight;
}


static void Execute( SShadeBob *pShadeBob, Display *pDisplay,
                     Window MainWindow,
                     GC *pGC, XImage *pImage,
                     signed short iColorCount, unsigned long *aiColorVals )
{
	unsigned long iColor;
	short iColorVal;
	int iPixelX, iPixelY;
	unsigned int iWidth, iHeight;

	MoveShadeBob( pShadeBob );
	
	for( iHeight=0; iHeight<iBobDiameter; iHeight++ )
	{
		iPixelY = pShadeBob->nPosY + iHeight;
		if( iPixelY >= iWinHeight )	iPixelY -= iWinHeight;

		for( iWidth=0; iWidth<iBobDiameter; iWidth++ )
		{
			iPixelX = pShadeBob->nPosX + iWidth;
			if( iPixelX >= iWinWidth )	iPixelX -= iWinWidth;

			iColor = XGetPixel( pImage, iPixelX, iPixelY );

			/*  FIXME: Here is a loop I'd love to take out. */
			for( iColorVal=0; iColorVal<iColorCount; iColorVal++ )
				if( aiColorVals[ iColorVal ] == iColor )
					break;

			iColorVal += pShadeBob->anDeltaMap[ iWidth * iBobDiameter + iHeight ];
			if( iColorVal >= iColorCount ) iColorVal = iColorCount - 1;
			if( iColorVal < 0 )			   iColorVal = 0;

			XPutPixel( pImage, iPixelX, iPixelY, aiColorVals[ iColorVal ] );
		}
	}

	/* FIXME: if it's next to the top or left sides of screen this will break. However, it's not noticable. */
	XPutImage( pDisplay, MainWindow, *pGC, pImage,
             pShadeBob->nPosX, pShadeBob->nPosY, pShadeBob->nPosX, pShadeBob->nPosY, iBobDiameter, iBobDiameter );
	XSync( pDisplay, False );
}


static void CreateTables( unsigned int nDegrees )
{
	double nRadian;
	unsigned int iDegree;
	anSinTable = calloc( nDegrees, sizeof(double) );
	anCosTable = calloc( nDegrees, sizeof(double) );

	for( iDegree=0; iDegree<nDegrees; iDegree++ )
	{
		nRadian = ( (double)(2*iDegree) / (double)nDegrees ) * M_PI;
		anSinTable[ iDegree ] = sin( nRadian );
		anCosTable[ iDegree ] = cos( nRadian );
	}
}


static unsigned long * SetPalette(Display *pDisplay, Window Win, char *sColor, signed short *piColorCount )
{
	XWindowAttributes XWinAttribs;
	XColor Color, *aColors;
	unsigned long *aiColorVals;
	signed short iColor;
	float nHalfColors;
	
	XGetWindowAttributes( pDisplay, Win, &XWinAttribs );
	
	Color.red =   RANDOM() % 0xFFFF;
	Color.green = RANDOM() % 0xFFFF;
	Color.blue =  RANDOM() % 0xFFFF;

	if( strcasecmp( sColor, "random" ) && !XParseColor( pDisplay, XWinAttribs.colormap, sColor, &Color ) )
		fprintf( stderr, "%s: color %s not found in database. Choosing to random...\n", progname, sColor );

#ifdef VERBOSE
	printf( "%s: Base color (RGB): <%d, %d, %d>\n", progclass, Color.red, Color.green, Color.blue );
#endif  /*  VERBOSE */

	*piColorCount = get_integer_resource( "ncolors", "Integer" );
	if( *piColorCount <   2 )	*piColorCount = 2;
	if( *piColorCount > 255 )	*piColorCount = 255;

	aColors    = calloc( *piColorCount, sizeof(XColor) );
	aiColorVals = calloc( *piColorCount, sizeof(unsigned long) );
	
	for( iColor=0; iColor<*piColorCount; iColor++ )
	{
		nHalfColors = *piColorCount / 2.0F;
		/* Black -> Base Color */
		if( iColor < (*piColorCount/2) )
		{
			aColors[ iColor ].red   = ( Color.red   / nHalfColors ) * iColor;
			aColors[ iColor ].green = ( Color.green / nHalfColors ) * iColor;
			aColors[ iColor ].blue  = ( Color.blue  / nHalfColors ) * iColor;
		}
		/* Base Color -> White */
		else
		{
			aColors[ iColor ].red   = ( ( ( 0xFFFF - Color.red )   / nHalfColors ) * ( iColor - nHalfColors ) ) + Color.red;
			aColors[ iColor ].green = ( ( ( 0xFFFF - Color.green ) / nHalfColors ) * ( iColor - nHalfColors ) ) + Color.green;
			aColors[ iColor ].blue  = ( ( ( 0xFFFF - Color.blue )  / nHalfColors ) * ( iColor - nHalfColors ) ) + Color.blue;
		}

		if( !XAllocColor( pDisplay, XWinAttribs.colormap, &aColors[ iColor ] ) )
		{
			/* start all over with less colors */	
			XFreeColors( pDisplay, XWinAttribs.colormap, aiColorVals, iColor, 0 );
			free( aColors );
			free( aiColorVals );
			piColorCount--;
			aColors     = calloc( *piColorCount, sizeof(XColor) );
			aiColorVals = calloc( *piColorCount, sizeof(unsigned long) );
			iColor = -1;
		}
		else
			aiColorVals[ iColor ] = aColors[ iColor ].pixel;
	}

	free( aColors );

	XSetWindowBackground( pDisplay, Win, aiColorVals[ 0 ] );

	return aiColorVals;
}


static void Initialize( Display *pDisplay, Window Win, GC *pGC, XImage **ppImage )
{
	XGCValues gcValues;
	XWindowAttributes XWinAttribs;
	int iBitsPerPixel;

	/* Create the Image for drawing */
	XGetWindowAttributes( pDisplay, Win, &XWinAttribs );

	/* Find the preferred bits-per-pixel. (jwz) */
	{
		int i, pfvc = 0;
		XPixmapFormatValues *pfv = XListPixmapFormats( pDisplay, &pfvc );
		for( i=0; i<pfvc; i++ )
			if( pfv[ i ].depth == XWinAttribs.depth )
			{
				iBitsPerPixel = pfv[ i ].bits_per_pixel;
				break;
			}
		if( pfv )
			XFree (pfv);
	}

	/*  Create the GC. */
	*pGC = XCreateGC( pDisplay, Win, 0, &gcValues );

	*ppImage = XCreateImage( pDisplay, XWinAttribs.visual, XWinAttribs.depth, ZPixmap, 0, NULL,
							  XWinAttribs.width, XWinAttribs.height, BitmapPad( pDisplay ), 0 );
	(*ppImage)->data = calloc((*ppImage)->bytes_per_line, (*ppImage)->height);

	iWinWidth = XWinAttribs.width;
	iWinHeight = XWinAttribs.height;

	/*  These are precalculations used in Execute(). */
	iBobDiameter = ( ( iWinWidth < iWinHeight ) ? iWinWidth : iWinHeight ) / 25;
	iBobRadius = iBobDiameter / 2;
#ifdef VERBOSE
	printf( "%s: Bob Diameter = %d\n", progclass, iBobDiameter );
#endif

	iWinCenterX = ( XWinAttribs.width / 2 ) - iBobRadius;
	iWinCenterY = ( XWinAttribs.height / 2 ) - iBobRadius;

	iVelocity = ( ( iWinWidth < iWinHeight ) ? iWinWidth : iWinHeight ) / 150;
	
	/*  Create the Sin and Cosine lookup tables. */
	iDegreeCount = get_integer_resource( "degrees", "Integer" );
	if(      iDegreeCount == 0   ) iDegreeCount = ( XWinAttribs.width / 6 ) + 400;
	else if( iDegreeCount < 90   ) iDegreeCount = 90;
	else if( iDegreeCount > 5400 ) iDegreeCount = 5400;
	CreateTables( iDegreeCount );
#ifdef VERBOSE
	printf( "%s: Using a %d degree circle.\n", progclass );
#endif /* VERBOSE */
  
	/*  Get the base color. */
	sColor = get_string_resource( "color", "Color" );
}


void screenhack(Display *pDisplay, Window Win )
{
	GC gc;
	signed short iColorCount = 0;
	unsigned long *aiColorVals = NULL;
	XImage *pImage = NULL;
	unsigned char nShadeBobCount, iShadeBob;
	SShadeBob *aShadeBobs;
#ifdef VERBOSE
	time_t nTime = time( NULL );
	unsigned short iFrame = 0;
#endif  /*  VERBOSE */
	int delay, cycles, i;

	nShadeBobCount = get_integer_resource( "count", "Integer" );
	if( nShadeBobCount > 64 ) nShadeBobCount = 64;
	if( nShadeBobCount <  1 ) nShadeBobCount = 1;

	if( ( aShadeBobs = calloc( nShadeBobCount, sizeof(SShadeBob) ) ) == NULL )
	{
		fprintf( stderr, "%s: Could not allocate %d ShadeBobs\n", progclass, nShadeBobCount );
		return;
	}
#ifdef VERBOSE 
	printf( "%s: Allocated %d ShadeBobs\n", progclass, nShadeBobCount );
#endif  /*  VERBOSE */

	Initialize( pDisplay, Win, &gc, &pImage );

	for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
		InitShadeBob( &aShadeBobs[ iShadeBob ], iShadeBob % 2 );

	delay = get_integer_resource( "delay", "Integer" );
	cycles = get_integer_resource( "cycles", "Integer" ) * iDegreeCount;
	i = cycles;

	while( 1 )
	{
		screenhack_handle_events( pDisplay );

		if( i++ >= cycles )
		{
                        XWindowAttributes XWinAttribs;
                        XGetWindowAttributes( pDisplay, Win, &XWinAttribs );

			i = 0;
			memset( pImage->data, 0, pImage->bytes_per_line * pImage->height );
			for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
				ResetShadeBob( &aShadeBobs[ iShadeBob ] );
                        XFreeColors( pDisplay, XWinAttribs.colormap, aiColorVals, iColorCount, 0 );
			free( aiColorVals );
			aiColorVals = SetPalette( pDisplay, Win, sColor, &iColorCount );
                        XClearWindow( pDisplay, Win );
		}

		for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
			Execute( &aShadeBobs[ iShadeBob ], pDisplay, Win, &gc, pImage, iColorCount, aiColorVals );

		if( delay && !(i % 4) )
			usleep(delay);

#ifdef VERBOSE
		iFrame++;
		if( nTime - time( NULL ) )
		{
			printf( "%s: %d FPS\n", progclass, iFrame );
			nTime = time( NULL );
			iFrame = 0;
		}
#endif  /*  VERBOSE */
	}

	free( anSinTable );
	free( anCosTable );
	free( pImage->data );
	XDestroyImage( pImage );
	for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
		free( aShadeBobs[ iShadeBob ].anDeltaMap );
	free( aShadeBobs );
	free( aiColorVals );
}


/* End of Module - "shadebobs.c" */

/* vim: ts=4
 */
