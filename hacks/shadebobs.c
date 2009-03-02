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
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

/* #define VERBOSE */

char *progclass = "ShadeBobs";

char *defaults [] = {
  "*degrees:  720",
  "*color:    random",
  "*count:    2",
  "*cycles:   8",
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
static unsigned short nMaxWidth, nMaxHeight;
static unsigned short nHalfWidth, nHalfHeight;


/* Ahem. Chocolate is a flavor; not a food. Thank you */


typedef struct
{
  char anDeltaMap[ 1024 ];  /* 32 x 32 Delta Map */
  double nAngleX, nAngleY;
  double nVelocityX, nVelocityY;
} SShadeBob;


void InitShadeBob( SShadeBob *pShadeBob, Bool bDark )
{
  double nDelta;
  char iWidth, iHeight;

  for( iHeight=-16; iHeight<16; iHeight++ )
    for( iWidth=-16; iWidth<16; iWidth++ )
    {
      nDelta = 9 - (sqrt( pow( iWidth+0.5, 2 ) + pow( iHeight+0.5, 2 ) ) / 2 );
      if( nDelta < 0 )  nDelta = 0;
      if( bDark ) nDelta = -nDelta;
      pShadeBob->anDeltaMap[ (iWidth+16)*32 + iHeight+16 ] = (char)nDelta;
    }

  pShadeBob->nAngleX = random() % nDegreeCount;
  pShadeBob->nAngleY = random() % nDegreeCount;
}


void Execute( SShadeBob *pShadeBob, Display *pDisplay, Window MainWindow,
              GC *pGC, XImage *pXImage, XColor *aXColors )
{
  long nColor;
  short nIndex;
  unsigned int nXPos, nYPos;
  unsigned int iWidth, iHeight;
/*  unsigned short *pData; */

  pShadeBob->nVelocityX += ( ( random() % 200 ) - 100 ) / 5000.0F;
  pShadeBob->nVelocityY += ( ( random() % 200 ) - 100 ) / 5000.0F;

  if(    pShadeBob->nVelocityX > 3 )  pShadeBob->nVelocityX = 3;
  else if( pShadeBob->nVelocityX < 2 )  pShadeBob->nVelocityX = 2;
  if(    pShadeBob->nVelocityY > 3 )  pShadeBob->nVelocityY = 3;
  else if( pShadeBob->nVelocityY < 2 )  pShadeBob->nVelocityY = 2;

  pShadeBob->nAngleX += pShadeBob->nVelocityX;
  pShadeBob->nAngleY += pShadeBob->nVelocityY;

  if(    pShadeBob->nAngleX >= nDegreeCount )
    pShadeBob->nAngleX -= nDegreeCount;
  else if( pShadeBob->nAngleX < 0 )
    pShadeBob->nAngleX += nDegreeCount;
  if(    pShadeBob->nAngleY >= nDegreeCount )
    pShadeBob->nAngleY -= nDegreeCount;
  else if( pShadeBob->nAngleY < 0 )       pShadeBob->nAngleY += nDegreeCount;

  /* Trig is your friend :) */
  nXPos = (unsigned int)(( anSinTable[ (int)pShadeBob->nAngleX ] * nMaxWidth )
                         + nHalfWidth);
  nYPos = (unsigned int)(( anSinTable[ (int)pShadeBob->nAngleY ] * nMaxHeight )
                         + nHalfHeight);

  for( iHeight=0; iHeight<32; iHeight++ )
  {
/*  pData = (unsigned short *)pXImage->data + ( ( nYPos + iHeight )
        * pXImage->width ); */
    for( iWidth=0; iWidth<32; iWidth++ )
    {
/*      nColor = pData[ nXPos + iWidth ]; */
      nColor = XGetPixel( pXImage, nXPos + iWidth, nYPos + iHeight );

      /*  FIXME: Here is a loop I'd love to take out. */
      for( nIndex=0; nIndex<64; nIndex++ )
        if( aXColors[ nIndex ].pixel == nColor )
          break;

      nIndex += pShadeBob->anDeltaMap[ iWidth*32+iHeight ];
      if( nIndex > 63 ) nIndex = 63;
      if( nIndex < 0 )  nIndex = 0;

/*      pData[ nXPos + iWidth ] = (unsigned short)aXColors[ nIndex ].pixel; */
      XPutPixel( pXImage, nXPos + iWidth, nYPos + iHeight,
                 aXColors[ nIndex ].pixel );
    }
  }
  /* Place graphics in window */
  XPutImage( pDisplay, MainWindow, *pGC, pXImage,
             nXPos, nYPos, nXPos, nYPos, 32, 32 );
  XSync (pDisplay, False);
}


void CreateTables( unsigned int nDegrees )
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


void SetPalette(Display *pDisplay, Window Win, char *sColor, XColor *aXColors)
{
  XWindowAttributes XWinAttrib;
  Colormap CMap;
  XColor Color;
  int nHue;
  double nSat, nVal;
  int nRampCount = 32;

  Color.red = ( random() % 3 ) * 0x7FFF;    /*  Either full, half or none. */
  Color.green = ( random() % 3 ) * 0x7FFF;
  Color.blue = ( random() % 3 ) * 0x7FFF;

  /*  If Color is black, grey, or white, then make SURE its a color */
  if( Color.red == Color.green && Color.green == Color.blue )
  { Color.red = 0xFFFF;
    Color.green = ( random() % 3 ) * 0x7FFF;
    Color.blue = 0; }

  XGetWindowAttributes( pDisplay, Win, &XWinAttrib );
  CMap = XWinAttrib.colormap;

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

  make_color_ramp( pDisplay, CMap, 0, 0, 0, nHue, nSat, nVal, aXColors,
                   &nRampCount, False, True, False );
  make_color_ramp( pDisplay, CMap, nHue, nSat, nVal, 0, 0, 1,
                   &aXColors[ nRampCount ], &nRampCount, False, True, False );
}


void Initialize( Display *pDisplay, Window Win,
                 GC *pGC, XImage *pXImage, XColor *aXColors )
{
  XGCValues gcValues;
  XWindowAttributes XWinAttribs;
  char *sColor;
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

  memset (pXImage, 0, sizeof(*pXImage));
  pXImage->width = XWinAttribs.width;                   /* Width of image */
  pXImage->height = XWinAttribs.height;                 /* Height of image */
  pXImage->format = ZPixmap;                /* XYBitmap, XYPixmap, ZPixmap */

  /* Pointer to image data */
  pXImage->byte_order = ImageByteOrder(pDisplay);
  pXImage->bitmap_unit = BitmapUnit(pDisplay);
  pXImage->bitmap_bit_order = BitmapBitOrder(pDisplay);
  pXImage->bitmap_pad = BitmapPad(pDisplay);
  pXImage->depth = XWinAttribs.depth;
  pXImage->bytes_per_line = 0;                /* Accelerator to next line */
  pXImage->bits_per_pixel = bpp;
  XInitImage( pXImage );
  pXImage->data = calloc(pXImage->bytes_per_line, pXImage->height);

  /*  These are precalculations used in Execute(). */
  nMaxWidth = ( XWinAttribs.width / 2 ) - 20;
  nMaxHeight = ( XWinAttribs.height / 2 ) - 20;
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
    SetPalette( pDisplay, Win, "random", aXColors );
  else
    SetPalette( pDisplay, Win, sColor, aXColors );
}


void screenhack(Display *pDisplay, Window Win )
{
  GC gc;
  XColor aXColors[ 64 ];
  XImage Image;
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

  Initialize( pDisplay, Win, &gc, &Image, aXColors );

  delay = get_integer_resource( "delay", "Integer" );
  cycles = get_integer_resource( "cycles", "Integer" ) * (nDegreeCount / 3);
  i = cycles;

  while (1)
  {

    if (i++ >= cycles)
      {
        i = 0;
        XClearWindow (pDisplay, Win);
        memset (Image.data, 0, Image.bytes_per_line * Image.height);
        for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
          InitShadeBob( &aShadeBobs[ iShadeBob ], iShadeBob % 2 );
      }

    for( iShadeBob=0; iShadeBob<nShadeBobCount; iShadeBob++ )
      Execute( &aShadeBobs[ iShadeBob ], pDisplay, Win, &gc,
               &Image, aXColors );

#ifdef VERBOSE
    iFrame++;
    if( nTime - time( NULL ) )
    {
      printf( "FPS: %d\n", iFrame );
      nTime = time( NULL );
      iFrame = 0;
    }
#endif  /*  VERBOSE */

    if (delay) usleep(delay);

      screenhack_handle_events( pDisplay );
  }

  free( anSinTable );
  free( Image.data );
  XDestroyImage( &Image );
  free( aShadeBobs );
}


/* End of Module - "shadebobs.c" */
