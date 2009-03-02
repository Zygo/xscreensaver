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
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

/* #define VERBOSE */

char *progclass = "ShadeBobs";

char *defaults [] = {
  "*degrees:  512",
  "*color:    random",
  "*count:    2",
  "*cycles:   50",
  "*ncolors:  64",    /* changing this doesn't work particularly well */
  "*delay:    5000",
  0
};

XrmOptionDescRec options [] = {
  { "-degrees", ".degrees", XrmoptionSepArg, 0 },
  { "-color",   ".color",   XrmoptionSepArg, 0 },
  { "-count",   ".count",   XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

static unsigned short nDegreeCount;
static double *anSinTable;
static unsigned short nMaxExtentX, nMaxExtentY;
static unsigned short nMinExtentX, nMinExtentY;
static unsigned short nHalfWidth, nHalfHeight;
static char *sColor;

#define RANDOM() ((int) (random() & 0X7FFFFFFFL))


/* Ahem. Chocolate is a flavor; not a food. Thank you */


#define MAPSIZE 32

typedef struct
{
  char anDeltaMap[ MAPSIZE * MAPSIZE ];  /* 32 x 32 Delta Map */
  double nVelocityX, nVelocityY;
  double nAngleX, nAngleY;
  short nExtentX, nExtentY;
} SShadeBob;


static void ResetShadeBob( SShadeBob *pShadeBob )
{
  pShadeBob->nAngleX = RANDOM() % nDegreeCount;
  pShadeBob->nAngleY = RANDOM() % nDegreeCount;

  pShadeBob->nExtentX = (RANDOM() % (nMaxExtentX - nMinExtentX)) + nMinExtentX;
  pShadeBob->nExtentY = (RANDOM() % (nMaxExtentY - nMinExtentY)) + nMinExtentY;
}


static void InitShadeBob( SShadeBob *pShadeBob, Bool bDark )
{
  double nDelta;
  char iWidth, iHeight;

  for( iHeight=-16; iHeight<16; iHeight++ )
    for( iWidth=-16; iWidth<16; iWidth++ )
    {
      nDelta = 9 - (sqrt( pow( iWidth+0.5, 2 ) + pow( iHeight+0.5, 2 ) ) / 2 );
      if( nDelta < 0 )  nDelta = 0;
      if( bDark ) nDelta = -nDelta;
      pShadeBob->anDeltaMap[ (iWidth+(MAPSIZE/2))*MAPSIZE
                           + iHeight+(MAPSIZE/2) ] = (char)nDelta;
    }

	ResetShadeBob( pShadeBob );
}


static void Execute( SShadeBob *pShadeBob, Display *pDisplay,
                     Window MainWindow,
                     GC *pGC, XImage *pXImage,
                     int ncolors, XColor *aXColors )
{
  long nColor;
  short nIndex;
  unsigned int nXPos, nYPos;
  unsigned int iWidth, iHeight;

  pShadeBob->nVelocityX += ( ( RANDOM() % 200 ) - 100 ) / 1000.0F;
  pShadeBob->nVelocityY += ( ( RANDOM() % 200 ) - 100 ) / 1000.0F;

  if(      pShadeBob->nVelocityX > 4 )  pShadeBob->nVelocityX = 4;
  else if( pShadeBob->nVelocityX < 3 )  pShadeBob->nVelocityX = 3;
  if(      pShadeBob->nVelocityY > 4 )  pShadeBob->nVelocityY = 4;
  else if( pShadeBob->nVelocityY < 3 )  pShadeBob->nVelocityY = 3;

  pShadeBob->nAngleX += pShadeBob->nVelocityX;
  pShadeBob->nAngleY += pShadeBob->nVelocityY;

  if(      pShadeBob->nAngleX >= nDegreeCount ) pShadeBob->nAngleX -= nDegreeCount;
  else if( pShadeBob->nAngleX < 0 )             pShadeBob->nAngleX += nDegreeCount;
  if(      pShadeBob->nAngleY >= nDegreeCount ) pShadeBob->nAngleY -= nDegreeCount;
  else if( pShadeBob->nAngleY < 0 )             pShadeBob->nAngleY += nDegreeCount;

  pShadeBob->nExtentX += ( RANDOM() % 5 ) - 2;
  if( pShadeBob->nExtentX > nMaxExtentX ) pShadeBob->nExtentX = nMaxExtentX;
  if( pShadeBob->nExtentX < nMinExtentX ) pShadeBob->nExtentX = nMinExtentX;
  pShadeBob->nExtentY += ( RANDOM() % 5 ) - 2;
  if( pShadeBob->nExtentY > nMaxExtentY ) pShadeBob->nExtentY = nMaxExtentY;
  if( pShadeBob->nExtentY < nMinExtentY ) pShadeBob->nExtentY = nMinExtentY;

  /* Trig is your friend :) */
  nXPos = (unsigned int)(( anSinTable[ (int)pShadeBob->nAngleX ] * pShadeBob->nExtentX )
                         + nHalfWidth);
  nYPos = (unsigned int)(( anSinTable[ (int)pShadeBob->nAngleY ] * pShadeBob->nExtentY )
                         + nHalfHeight);

  for( iHeight=0; iHeight < MAPSIZE; iHeight++ )
  {
    for( iWidth=0; iWidth < MAPSIZE; iWidth++ )
    {
      nColor = XGetPixel( pXImage, nXPos + iWidth, nYPos + iHeight );

      /*  FIXME: Here is a loop I'd love to take out. */
      for( nIndex=0; nIndex < ncolors; nIndex++ )
        if( aXColors[ nIndex ].pixel == nColor )
          break;

      nIndex += pShadeBob->anDeltaMap[ iWidth * MAPSIZE + iHeight ];
      if( nIndex >= ncolors ) nIndex = ncolors-1;
      if( nIndex < 0 )  nIndex = 0;

      XPutPixel( pXImage, nXPos + iWidth, nYPos + iHeight,
                 aXColors[ nIndex ].pixel );
    }
  }
  /* Place graphics in window */
  XPutImage( pDisplay, MainWindow, *pGC, pXImage,
             nXPos, nYPos, nXPos, nYPos, MAPSIZE, MAPSIZE );
  XSync (pDisplay, False);
}


static void CreateTables( unsigned int nDegrees )
{
  double nRadian;
  unsigned int iDegree;
  anSinTable = calloc( nDegrees, sizeof(double) );

  for( iDegree=0; iDegree<nDegrees; iDegree++ )
  {
    nRadian = ( (double)(2*iDegree) / (double)nDegrees ) * M_PI;
    anSinTable[ iDegree ] = sin( nRadian );
  }
}


static void SetPalette(Display *pDisplay, Window Win, char *sColor,
                       int *ncolorsP, XColor *aXColors )
{
  XWindowAttributes XWinAttrib;
  Colormap CMap;
  XColor Color;
  int nHue;
  double nSat, nVal;

  XGetWindowAttributes( pDisplay, Win, &XWinAttrib );
  CMap = XWinAttrib.colormap;

  Color.red = ( RANDOM() % 3 ) * 0x7FFF;    /*  Either full, half or none. */
  Color.green = ( RANDOM() % 3 ) * 0x7FFF;
  Color.blue = ( RANDOM() % 3 ) * 0x7FFF;

  /*  If Color is black, grey, or white, then make SURE its a color */
  if( Color.red == Color.green && Color.green == Color.blue )
  { Color.red = 0xFFFF;
    Color.green = ( RANDOM() % 3 ) * 0x7FFF;
    Color.blue = 0; }

  if( strcasecmp( sColor, "random" ) &&
      !XParseColor( pDisplay, CMap, sColor, &Color ) )
    fprintf( stderr,
             "%s: color %s not found in database. Choosing to random...\n",
             progname, sColor );

  rgb_to_hsv( Color.red, Color.green, Color.blue, &nHue, &nSat, &nVal );
#ifdef VERBOSE
  printf( "RGB (%d, %d, %d) = HSV (%d, %g, %g)\n",
          Color.red, Color.green, Color.blue, nHue, nSat, nVal );
#endif  /*  VERBOSE */

  if (*ncolorsP != 0)
    free_colors (pDisplay, CMap, aXColors, *ncolorsP);

  *ncolorsP = get_integer_resource ("ncolors", "Integer");
  if (*ncolorsP <= 0) *ncolorsP = 64;

  /* allocate two color ramps, black -> color -> white. */
  {
    int n1, n2;
    n1 = *ncolorsP / 2;
    make_color_ramp( pDisplay, CMap, 0, 0, 0, nHue, nSat, nVal,
                     aXColors, &n1,
                     False, True, False );
    n2 = *ncolorsP - n1;
    make_color_ramp( pDisplay, CMap, nHue, nSat, nVal, 0, 0, 1,
                     aXColors + n1, &n2,
                     False, True, False );
    *ncolorsP = n1 + n2;
  }
}


static void Initialize( Display *pDisplay, Window Win,
                        GC *pGC, XImage **pXImage,
                        int *ncolorsP, XColor *aXColors )
{
  XGCValues gcValues;
  XWindowAttributes XWinAttribs;
  int bpp;

  /* Create the Image for drawing */
  XGetWindowAttributes( pDisplay, Win, &XWinAttribs );

  /* Find the preferred bits-per-pixel. */
  {
    int i, pfvc = 0;
    XPixmapFormatValues *pfv = XListPixmapFormats (pDisplay, &pfvc);
    for (i = 0; i < pfvc; i++)
      if (pfv[i].depth == XWinAttribs.depth)
        {
          bpp = pfv[i].bits_per_pixel;
          break;
        }
    if (pfv)
      XFree (pfv);
  }

  /*  Create the GC. */
  *pGC = XCreateGC( pDisplay, Win, 0, &gcValues );

  *pXImage = XCreateImage(pDisplay, XWinAttribs.visual,
			  XWinAttribs.depth,
			  ZPixmap, 0, NULL,
			  XWinAttribs.width, XWinAttribs.height,
			  BitmapPad(pDisplay), 0);
  (*pXImage)->data = calloc((*pXImage)->bytes_per_line,
			    (*pXImage)->height);

  /*  These are precalculations used in Execute(). */
  nMaxExtentX = ( XWinAttribs.width / 2 ) - 20;
  nMaxExtentY = ( XWinAttribs.height / 2 ) - 20;
  nMinExtentX = nMaxExtentX / 3;
  nMinExtentY = nMaxExtentY / 3;
  nHalfWidth = ( XWinAttribs.width / 2 ) - 16;
  nHalfHeight = ( XWinAttribs.height / 2 ) - 16;

  /*  Create the Sin and Cosine lookup tables. */
  nDegreeCount = get_integer_resource( "degrees", "Integer" );
  if( nDegreeCount < 90 ) nDegreeCount = 90;
  if( nDegreeCount > 5400 ) nDegreeCount = 5400;
  CreateTables( nDegreeCount );

  /*  Get the colors. */
  sColor = get_string_resource( "color", "Color" );
  if( sColor == NULL)
    SetPalette( pDisplay, Win, "random", ncolorsP, aXColors );
  else
    SetPalette( pDisplay, Win, sColor, ncolorsP, aXColors );
}


void screenhack(Display *pDisplay, Window Win )
{
  GC gc;
  int ncolors = 0;
  XColor aXColors[ 256 ];
  XImage *pImage;
  unsigned char nShadeBobCount, iShadeBob;
  SShadeBob *aShadeBobs;
#ifdef VERBOSE
  time_t nTime = time( NULL );
  unsigned short iFrame = 0;
#endif  /*  VERBOSE */
  int delay, cycles, i;

  nShadeBobCount = get_integer_resource( "count", "Integer" );
  if( nShadeBobCount > 64 ) nShadeBobCount = 64;
  if( nShadeBobCount < 1 )  nShadeBobCount = 1;

  if( ( aShadeBobs = calloc( nShadeBobCount, sizeof(SShadeBob) ) ) == NULL )
  {
    fprintf( stderr, "Could not allocate %d ShadeBobs\n", nShadeBobCount );
    return;
  }
#ifdef VERBOSE 
  printf( "Allocated %d ShadeBobs\n", nShadeBobCount );
#endif  /*  VERBOSE */

  Initialize( pDisplay, Win, &gc, &pImage, &ncolors, aXColors );

  for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
    InitShadeBob( &aShadeBobs[ iShadeBob ], iShadeBob % 2 );

  delay = get_integer_resource( "delay", "Integer" );
  cycles = get_integer_resource( "cycles", "Integer" ) * (nDegreeCount / 3);
  i = cycles;

  while (1)
  {
    screenhack_handle_events( pDisplay );

    if (i++ >= cycles)
    {
      i = 0;
      XClearWindow (pDisplay, Win);
      memset (pImage->data, 0, pImage->bytes_per_line * pImage->height);
      for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
        ResetShadeBob( &aShadeBobs[ iShadeBob ] );
      SetPalette( pDisplay, Win, sColor, &ncolors, aXColors );
    }

    for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
      Execute( &aShadeBobs[ iShadeBob ], pDisplay, Win, &gc,
               pImage, ncolors, aXColors );

    if( delay && !(i % 4) )
		usleep(delay);

#ifdef VERBOSE
    iFrame++;
    if( nTime - time( NULL ) )
    {
      printf( "FPS: %d\n", iFrame );
      nTime = time( NULL );
      iFrame = 0;
    }
#endif  /*  VERBOSE */
  }

  free( anSinTable );
  free( pImage->data );
  XDestroyImage( pImage );
  free( aShadeBobs );
}


/* End of Module - "shadebobs.c" */
