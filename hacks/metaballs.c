/* MetaBalls, Copyright (c) 2002-2003 W.P. van Paassen <peter@paassen.tmfweb.nl>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Module - "metaballs.c"
 *
 * [01/24/03] - W.P. van Paassen: Additional features
 * [12/29/02] - W.P. van Paassen: Port to X for use with XScreenSaver, the shadebob hack by Shane Smit was used as a template
 * [12/26/02] - W.P. van Paassen: Creation for the Demo Effects Collection (http://demo-effects.sourceforge.net)
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

/*#define VERBOSE*/ 

char *progclass = "Metaballs";

char *defaults [] = {
  ".background: black",
  ".foreground: white",
  "*color:    random",
  "*count:    10",
  "*cycles:   1000",
  "*ncolors:  256",
  "*delay:    5000",
  "*radius:   100",
  "*delta:   3",
  0
};

XrmOptionDescRec options [] = {
  { "-color",   ".color",   XrmoptionSepArg, 0 },
  { "-ncolors", ".ncolors", XrmoptionSepArg, 0 },
  { "-count",   ".count",   XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { "-radius",  ".radius",  XrmoptionSepArg, 0 },
  { "-delta",  ".delta",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

static unsigned short iWinWidth, iWinHeight;
static char *sColor;

/*blob structure*/
typedef struct 
{
  short xpos,ypos;
} BLOB;

static unsigned int nBlobCount;
static unsigned char radius;
static unsigned char delta;
static unsigned char dradius;
static unsigned short sradius;
static unsigned char **blob;
static BLOB *blobs;
static unsigned char **blub;

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static void init_blob(BLOB *blob)
{
  blob->xpos = iWinWidth/4  + BELLRAND(iWinWidth/2)  - radius;
  blob->ypos = iWinHeight/4 + BELLRAND(iWinHeight/2) - radius;
}

static void Execute( Display *pDisplay,
                     Window MainWindow,
                     GC *pGC, XImage *pImage,
                     signed short iColorCount, unsigned long *aiColorVals )
{
	unsigned int i, j, k;

	/* clear blub array */
	for (i = 0; i < iWinHeight; ++i)
	  memset(blub[i], 0, iWinWidth * sizeof(unsigned char));

	/* move blobs */
	for (i = 0; i < nBlobCount; i++)
	{
	  blobs[i].xpos += -delta + (int)((delta + .5f) * frand(2.0));
	  blobs[i].ypos += -delta + (int)((delta + .5f) * frand(2.0));
	}

	/* draw blobs to blub array */
	for (k = 0; k < nBlobCount; ++k)
	  { 
	    if (blobs[k].ypos > -dradius && blobs[k].xpos > -dradius && blobs[k].ypos < iWinHeight && blobs[k].xpos < iWinWidth)
	      {
		for (i = 0; i < dradius; ++i)
		  {
		    if (blobs[k].ypos + i >= 0 && blobs[k].ypos + i < iWinHeight)
		      {
			for (j = 0; j < dradius; ++j)
			  {
			    if (blobs[k].xpos + j >= 0 && blobs[k].xpos + j < iWinWidth)
			      {
				if (blub[blobs[k].ypos + i][blobs[k].xpos + j] < iColorCount)
				  {
				    if (blub[blobs[k].ypos + i][blobs[k].xpos + j] + blob[i][j] > iColorCount)
				      blub[blobs[k].ypos + i][blobs[k].xpos + j] = iColorCount;
				    else 
				      blub[blobs[k].ypos + i][blobs[k].xpos + j] += blob[i][j];     
				  }
			      }
			  }
		      }
		  }
	      }
	    else
	      init_blob(blobs + k);
	  }

	memset( pImage->data, 0, pImage->bytes_per_line * pImage->height);

	/* draw blub array to screen */
	for (i = 0; i < iWinHeight; ++i)
	  {
	    for (j = 0; j < iWinWidth; ++j)
	      {
		if (aiColorVals[blub[i][j]] > 0)
		  XPutPixel( pImage, j, i, aiColorVals[blub[i][j]] );
	      }
	  }

	XPutImage( pDisplay, MainWindow, *pGC, pImage,
		   0, 0, 0, 0, iWinWidth, iWinHeight );
	XSync( pDisplay, False );
}

static unsigned long * SetPalette(Display *pDisplay, Window Win, char *sColor, signed short *piColorCount )
{
	XWindowAttributes XWinAttribs;
	XColor Color, *aColors;
	unsigned long *aiColorVals;
	signed short iColor;
	float nHalfColors;
	
	XGetWindowAttributes( pDisplay, Win, &XWinAttribs );
	
	Color.red =   random() % 0xFFFF;
	Color.green = random() % 0xFFFF;
	Color.blue =  random() % 0xFFFF;

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
			(*piColorCount)--;

                        if (*piColorCount < 6)
                          {
                            fprintf (stderr, "%s: insufficient colors!\n",
                                     progname);
                            exit (1);
                          }

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
	int iBitsPerPixel, i, j;
	unsigned int distance_squared;
	float fraction;

	/* Create the Image for drawing */
	XGetWindowAttributes( pDisplay, Win, &XWinAttribs );

	/* Find the preferred bits-per-pixel. (jwz) */
	{
		int pfvc = 0;
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

	/*  Get the base color. */
	sColor = get_string_resource( "color", "Color" );

	/*  Get the delta. */
	delta = get_integer_resource( "delta", "Integer" );
	if (delta < 1)
	  delta = 1;
	else if (delta > 20)
	  delta = 20;
	
	/*  Get the radius. */
	radius = get_integer_resource( "radius", "Integer" );
	if (radius < 2)
	  radius = 2;
	if (radius > 100)
	  radius = 100;
	
	radius = (radius / 100.0) * (iWinHeight >> 3);
        if (radius >= 128) /* should use UCHAR_MAX? */
          radius = 127; /* dradius should fit in u_char */

	dradius = radius * 2;
	sradius = radius * radius;

	/* create blob */
	blob = malloc ( dradius * sizeof(unsigned char*));
	for (i = 0; i < dradius; ++i)
	  blob[i] = malloc( dradius * sizeof(unsigned char));

	/* create blub array */
	blub = malloc( iWinHeight * sizeof(unsigned char*));
	for (i = 0; i < iWinHeight; ++i)
	  blub[i] = malloc( iWinWidth * sizeof(unsigned char));

	/* create blob */
	for (i = -radius; i < radius; ++i)
	  {
	    for (j = -radius; j < radius; ++j)
	      {
		distance_squared = i * i + j * j;
		if (distance_squared <= sradius)
		  {
		    /* compute density */     
		    fraction = (float)distance_squared / (float)sradius;
		    blob[i + radius][j + radius] = pow((1.0 - (fraction * fraction)),4.0) * 255.0;
		  }
		else
		  {
		    blob[i + radius][j + radius] = 0;
		  }
	      }    
	  }
	
	for (i = 0; i < nBlobCount; i++)
	  {
	    init_blob(blobs + i);
	  }
}

void screenhack(Display *pDisplay, Window Win )
{
	GC gc;
	signed short iColorCount = 0;
	unsigned long *aiColorVals = NULL;
	XImage *pImage = NULL;
#ifdef VERBOSE
	time_t nTime = time( NULL );
	unsigned short iFrame = 0;
#endif  /*  VERBOSE */
	int delay, cycles, i;

	nBlobCount = get_integer_resource( "count", "Integer" );
	if( nBlobCount > 255 ) nBlobCount = 255;
	if( nBlobCount <  2 ) nBlobCount = 2;

	if( ( blobs = calloc( nBlobCount, sizeof(BLOB) ) ) == NULL )
	{
		fprintf( stderr, "%s: Could not allocate %d Blobs\n", progclass, nBlobCount );
		return;
	}
#ifdef VERBOSE 
	printf( "%s: Allocated %d Blobs\n", progclass, nBlobCount );
#endif  /*  VERBOSE */

	Initialize( pDisplay, Win, &gc, &pImage );

	delay = get_integer_resource( "delay", "Integer" );
	cycles = get_integer_resource( "cycles", "Integer" );
	i = cycles;

	while( 1 )
	{
		screenhack_handle_events( pDisplay );

		if( i++ >= cycles )
		{
                        XWindowAttributes XWinAttribs;
                        XGetWindowAttributes( pDisplay, Win, &XWinAttribs );

			memset( pImage->data, 0, pImage->bytes_per_line * pImage->height );
                        XFreeColors( pDisplay, XWinAttribs.colormap, aiColorVals, iColorCount, 0 );
			free( aiColorVals );
			aiColorVals = SetPalette( pDisplay, Win, sColor, &iColorCount );
                        XClearWindow( pDisplay, Win );
			for (i = 0; i < nBlobCount; i++)
			  {
			    init_blob(blobs + i);
			  }
			i = 0;
		}

		Execute( pDisplay, Win, &gc, pImage, iColorCount - 1, aiColorVals );

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

	free( pImage->data );
	XDestroyImage( pImage );
	free( aiColorVals );
	free( blobs );
	for (i = 0; i < iWinHeight; ++i)
	  free( blub[i] );
	free( blub );
	for (i = 0; i < dradius; ++i)
	  free( blob[i] );
	free( blob );
}


/* End of Module - "metaballs.c" */

