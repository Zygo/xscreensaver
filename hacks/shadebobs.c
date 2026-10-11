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

/* #define VERBOSE */

static const char *shadebobs_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*fpsSolid:	true",
  "*degrees:  0",	/* default: Automatic degree calculation */
  "*color:    random",
  "*count:    4",
  "*cycles:   10",
  "*ncolors:  64",    /* changing this doesn't work particularly well */
  "*delay:    10000",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec shadebobs_options [] = {
  { "-degrees", ".degrees", XrmoptionSepArg, 0 },
  { "-color",   ".color",   XrmoptionSepArg, 0 },
  { "-ncolors", ".ncolors", XrmoptionSepArg, 0 },
  { "-count",   ".count",   XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

/* Ahem. Chocolate is a flavor; not a food. Thank you */


typedef struct
{
	signed char *anDeltaMap;
	double nAngle, nAngleDelta, nAngleInc;
	double nPosX, nPosY;
} SShadeBob;

struct state {
  Display *dpy;
  Window window;
  unsigned short iDegreeCount;
  double *anSinTable, *anCosTable;
  unsigned short iWinWidth, iWinHeight;
  unsigned short iWinCenterX, iWinCenterY;
  char *sColor;
  unsigned char iBobRadius, iBobDiameter; 
  unsigned char iVelocity;

  unsigned long *aiColorVals;
  signed short iColorCount;
  int cycles;
  XImage *pImage;
  unsigned char nShadeBobCount, iShadeBob;
  SShadeBob *aShadeBobs;
  GC gc;
  int delay;
  int draw_i;
};

#define RANDOM() ((int) (random() & 0X7FFFFFFFL))



static void ResetShadeBob( struct state *st, SShadeBob *pShadeBob )
{
	pShadeBob->nPosX = RANDOM() % st->iWinWidth;
	pShadeBob->nPosY = RANDOM() % st->iWinHeight;
	pShadeBob->nAngle = RANDOM() % st->iDegreeCount;
	pShadeBob->nAngleDelta = ( RANDOM() % st->iDegreeCount ) - ( st->iDegreeCount / 2.0F );
	pShadeBob->nAngleInc = pShadeBob->nAngleDelta / 50.0F; 
	if( pShadeBob->nAngleInc == 0.0F )	
		pShadeBob->nAngleInc = ( pShadeBob->nAngleDelta > 0.0F ) ? 0.0001F : -0.0001F;
}


static void InitShadeBob( struct state *st, SShadeBob *pShadeBob, Bool bDark )
{
	double nDelta;
	int iWidth, iHeight;

	if( ( pShadeBob->anDeltaMap = calloc( st->iBobDiameter * st->iBobDiameter, sizeof(signed char) ) ) == NULL )
	{
		fprintf( stderr, "%s: Could not allocate Delta Map!\n", progname );
		return;
	}

	for( iHeight=-st->iBobRadius; iHeight<st->iBobRadius; iHeight++ )
		for( iWidth=-st->iBobRadius; iWidth<st->iBobRadius; iWidth++ )
		{
			nDelta = 9 - ( ( sqrt( pow( iWidth+0.5, 2 ) + pow( iHeight+0.5, 2 ) ) / st->iBobRadius ) * 8 );
			if( nDelta < 0 )  nDelta = 0;
			if( bDark ) nDelta = -nDelta;
			pShadeBob->anDeltaMap[ ( iWidth + st->iBobRadius ) * st->iBobDiameter + iHeight + st->iBobRadius ] = (char)nDelta;
		}
  
	ResetShadeBob( st, pShadeBob );
}


/* A delta is calculated, and the shadebob turns at an increment.  When the delta
 * falls to 0, a new delta and increment are calculated. */
static void MoveShadeBob( struct state *st, SShadeBob *pShadeBob )
{
	pShadeBob->nAngle	   += pShadeBob->nAngleInc;
	pShadeBob->nAngleDelta -= pShadeBob->nAngleInc;

	/* Since it can happen that nAngle < 0 and nAngle + iDegreeCount >= iDegreeCount
	   on floating point, we set some marginal value.
	*/
	if( pShadeBob->nAngle + 0.5 >= st->iDegreeCount )	pShadeBob->nAngle -= st->iDegreeCount;
	else if( pShadeBob->nAngle < -0.5 )		pShadeBob->nAngle += st->iDegreeCount;
	
	if( ( pShadeBob->nAngleInc>0.0F  && pShadeBob->nAngleDelta<pShadeBob->nAngleInc ) ||
	    ( pShadeBob->nAngleInc<=0.0F && pShadeBob->nAngleDelta>pShadeBob->nAngleInc ) )
	{
		pShadeBob->nAngleDelta = ( RANDOM() % st->iDegreeCount ) - ( st->iDegreeCount / 2.0F );
		pShadeBob->nAngleInc = pShadeBob->nAngleDelta / 50.0F;
		if( pShadeBob->nAngleInc == 0.0F )
			pShadeBob->nAngleInc = ( pShadeBob->nAngleDelta > 0.0F ) ? 0.0001F : -0.0001F;
   	}
	
	pShadeBob->nPosX = ( st->anSinTable[ (int)pShadeBob->nAngle ] * st->iVelocity ) + pShadeBob->nPosX;
	pShadeBob->nPosY = ( st->anCosTable[ (int)pShadeBob->nAngle ] * st->iVelocity ) + pShadeBob->nPosY;

	/* This wraps it around the screen. */
	if( pShadeBob->nPosX >= st->iWinWidth )	pShadeBob->nPosX -= st->iWinWidth;
	else if( pShadeBob->nPosX < 0 )		pShadeBob->nPosX += st->iWinWidth;
	
	if( pShadeBob->nPosY >= st->iWinHeight )	pShadeBob->nPosY -= st->iWinHeight;
	else if( pShadeBob->nPosY < 0 )			pShadeBob->nPosY += st->iWinHeight;
}


static void Execute( struct state *st, SShadeBob *pShadeBob )
{
	unsigned long iColor;
	short iColorVal;
	int iPixelX, iPixelY;
	unsigned int iWidth, iHeight;

	MoveShadeBob( st, pShadeBob );
	
	for( iHeight=0; iHeight<st->iBobDiameter; iHeight++ )
	{
		iPixelY = pShadeBob->nPosY + iHeight;
		if( iPixelY >= st->iWinHeight )	iPixelY -= st->iWinHeight;

		for( iWidth=0; iWidth<st->iBobDiameter; iWidth++ )
		{
			iPixelX = pShadeBob->nPosX + iWidth;
			if( iPixelX >= st->iWinWidth )	iPixelX -= st->iWinWidth;

			iColor = XGetPixel( st->pImage, iPixelX, iPixelY );

			/*  FIXME: Here is a loop I'd love to take out. */
			for( iColorVal=0; iColorVal<st->iColorCount; iColorVal++ )
				if( st->aiColorVals[ iColorVal ] == iColor )
					break;

			iColorVal += pShadeBob->anDeltaMap[ iWidth * st->iBobDiameter + iHeight ];
			if( iColorVal >= st->iColorCount ) iColorVal = st->iColorCount - 1;
			if( iColorVal < 0 )			   iColorVal = 0;

			XPutPixel( st->pImage, iPixelX, iPixelY, st->aiColorVals[ iColorVal ] );
		}
	}

	/* FIXME: if it's next to the top or left sides of screen this will break. However, it's not noticable. */
	XPutImage( st->dpy, st->window, st->gc, st->pImage,
             pShadeBob->nPosX, pShadeBob->nPosY, pShadeBob->nPosX, pShadeBob->nPosY, st->iBobDiameter, st->iBobDiameter );
}


static void CreateTables( struct state *st, unsigned int nDegrees )
{
	double nRadian;
	unsigned int iDegree;
	st->anSinTable = calloc( nDegrees, sizeof(double) );
	st->anCosTable = calloc( nDegrees, sizeof(double) );

	for( iDegree=0; iDegree<nDegrees; iDegree++ )
	{
		nRadian = ( (double)(2*iDegree) / (double)nDegrees ) * M_PI;
		st->anSinTable[ iDegree ] = sin( nRadian );
		st->anCosTable[ iDegree ] = cos( nRadian );
	}
}


static unsigned long * SetPalette(struct state *st )
{
	XWindowAttributes XWinAttribs;
	XColor Color, *aColors;
	signed short iColor;
	float nHalfColors;
	
	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );
	
	Color.red =   RANDOM() % 0xFFFF;
	Color.green = RANDOM() % 0xFFFF;
	Color.blue =  RANDOM() % 0xFFFF;

	if( strcasecmp( st->sColor, "random" ) && !XParseColor( st->dpy, XWinAttribs.colormap, st->sColor, &Color ) )
		fprintf( stderr, "%s: color %s not found in database. Choosing to random...\n", progname, st->sColor );

#ifdef VERBOSE
	printf( "%s: Base color (RGB): <%d, %d, %d>\n", progname, Color.red, Color.green, Color.blue );
#endif  /*  VERBOSE */

	st->iColorCount = get_integer_resource(st->dpy,  "ncolors", "Integer" );
	if( st->iColorCount <   2 )	st->iColorCount = 2;
	if( st->iColorCount > 255 )	st->iColorCount = 255;

	aColors    = calloc( st->iColorCount, sizeof(XColor) );
	st->aiColorVals = calloc( st->iColorCount, sizeof(unsigned long) );
	
	for( iColor=0; iColor<st->iColorCount; iColor++ )
	{
		nHalfColors = st->iColorCount / 2.0F;
		/* Black -> Base Color */
		if( iColor < (st->iColorCount/2) )
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

		if( !XAllocColor( st->dpy, XWinAttribs.colormap, &aColors[ iColor ] ) )
		{
			/* start all over with less colors */	
			XFreeColors( st->dpy, XWinAttribs.colormap, st->aiColorVals, iColor, 0 );
			free( aColors );
			free( st->aiColorVals );
			st->iColorCount--;
			aColors     = calloc( st->iColorCount, sizeof(XColor) );
			st->aiColorVals = calloc( st->iColorCount, sizeof(unsigned long) );
			iColor = -1;
		}
		else
			st->aiColorVals[ iColor ] = aColors[ iColor ].pixel;
	}

	free( aColors );

	XSetWindowBackground( st->dpy, st->window, st->aiColorVals[ 0 ] );

	return st->aiColorVals;
}


static void Initialize( struct state *st )
{
	XGCValues gcValues;
	XWindowAttributes XWinAttribs;
	/*int iBitsPerPixel;*/

	/* Create the Image for drawing */
	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );

#if 0
  /* Find the preferred bits-per-pixel. (jwz) */
	{
		int i, pfvc = 0;
		XPixmapFormatValues *pfv = XListPixmapFormats( st->dpy, &pfvc );
		for( i=0; i<pfvc; i++ )
			if( pfv[ i ].depth == XWinAttribs.depth )
			{
				iBitsPerPixel = pfv[ i ].bits_per_pixel;
				break;
			}
		if( pfv )
			XFree (pfv);
	}
#endif

	/*  Create the GC. */
	st->gc = XCreateGC( st->dpy, st->window, 0, &gcValues );

	st->pImage = XCreateImage( st->dpy, XWinAttribs.visual, XWinAttribs.depth, ZPixmap, 0, NULL,
							  XWinAttribs.width, XWinAttribs.height, 8 /*BitmapPad( st->dpy )*/, 0 );
	st->pImage->data = calloc((st->pImage)->bytes_per_line, (st->pImage)->height);

	st->iWinWidth = XWinAttribs.width;
	st->iWinHeight = XWinAttribs.height;

	/*  These are precalculations used in Execute(). */
	st->iBobDiameter = ( ( st->iWinWidth < st->iWinHeight ) ? st->iWinWidth : st->iWinHeight ) / 25;
	st->iBobRadius = st->iBobDiameter / 2;
#ifdef VERBOSE
	printf( "%s: Bob Diameter = %d\n", progname, st->iBobDiameter );
#endif

	st->iWinCenterX = ( XWinAttribs.width / 2 ) - st->iBobRadius;
	st->iWinCenterY = ( XWinAttribs.height / 2 ) - st->iBobRadius;

	st->iVelocity = ( ( st->iWinWidth < st->iWinHeight ) ? st->iWinWidth : st->iWinHeight ) / 150;
	
	/*  Create the Sin and Cosine lookup tables. */
	st->iDegreeCount = get_integer_resource(st->dpy,  "degrees", "Integer" );
	if(      st->iDegreeCount == 0   ) st->iDegreeCount = ( XWinAttribs.width / 6 ) + 400;
	else if( st->iDegreeCount < 90   ) st->iDegreeCount = 90;
	else if( st->iDegreeCount > 5400 ) st->iDegreeCount = 5400;
	CreateTables( st, st->iDegreeCount );
#ifdef VERBOSE
	printf( "%s: Using a %d degree circle.\n", progname, st->iDegreeCount );
#endif /* VERBOSE */
  
	/*  Get the base color. */
	st->sColor = get_string_resource(st->dpy,  "color", "Color" );
}


static void *
shadebobs_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));

#ifdef VERBOSE
	time_t nTime = time( NULL );
	unsigned short iFrame = 0;
#endif  /*  VERBOSE */

  st->dpy = dpy;
  st->window = window;

	st->nShadeBobCount = get_integer_resource(st->dpy,  "count", "Integer" );
	if( st->nShadeBobCount > 64 ) st->nShadeBobCount = 64;
	if( st->nShadeBobCount <  1 ) st->nShadeBobCount = 1;

	if( ( st->aShadeBobs = calloc( st->nShadeBobCount, sizeof(SShadeBob) ) ) == NULL )
	{
		fprintf( stderr, "%s: Could not allocate %d ShadeBobs\n", progname, st->nShadeBobCount );
                abort();
	}
#ifdef VERBOSE 
	printf( "%s: Allocated %d ShadeBobs\n", progname, st->nShadeBobCount );
#endif  /*  VERBOSE */

	Initialize( st );

	for( st->iShadeBob=0; st->iShadeBob<st->nShadeBobCount; st->iShadeBob++ )
		InitShadeBob( st, &st->aShadeBobs[ st->iShadeBob ], st->iShadeBob % 2 );

	st->delay = get_integer_resource(st->dpy,  "delay", "Integer" );
	st->cycles = get_integer_resource(st->dpy,  "cycles", "Integer" ) * st->iDegreeCount;

        st->draw_i = 99999999;
        return st;
}

static unsigned long
shadebobs_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if( st->draw_i++ >= st->cycles )
    {
      XWindowAttributes XWinAttribs;
      XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );

      st->draw_i = 0;
#if 0
      memset( st->pImage->data, 0, st->pImage->bytes_per_line * st->pImage->height );
#else
      {
        /* fill the image with the actual value of the black pixel, not 0. */
        unsigned long black = BlackPixelOfScreen (XWinAttribs.screen);
        int x, y;
        for (y = 0; y < XWinAttribs.height; y++)
          for (x = 0; x < XWinAttribs.width; x++)
            XPutPixel (st->pImage, x, y, black);
      }
#endif

      for( st->iShadeBob=0; st->iShadeBob<st->nShadeBobCount; st->iShadeBob++ )
        ResetShadeBob( st, &st->aShadeBobs[ st->iShadeBob ] );
      XFreeColors( st->dpy, XWinAttribs.colormap, st->aiColorVals, st->iColorCount, 0 );
      free( st->aiColorVals );
      st->aiColorVals = SetPalette( st );
      XClearWindow( st->dpy, st->window );
    }

  for( st->iShadeBob=0; st->iShadeBob<st->nShadeBobCount; st->iShadeBob++ )
    Execute( st, &st->aShadeBobs[ st->iShadeBob ] );

  return st->delay;
}

static void
shadebobs_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
shadebobs_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
shadebobs_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
	free( st->anSinTable );
	free( st->anCosTable );
        if (st->sColor) free (st->sColor);
        XFreeGC (dpy, st->gc);
	XDestroyImage( st->pImage );
	for( st->iShadeBob=0; st->iShadeBob<st->nShadeBobCount; st->iShadeBob++ )
		free( st->aShadeBobs[ st->iShadeBob ].anDeltaMap );
	free( st->aShadeBobs );
	free( st->aiColorVals );
        free(st);
}


XSCREENSAVER_MODULE ("ShadeBobs", shadebobs)

/* End of Module - "shadebobs.c" */

/* vim: ts=4
 */
