/* Eruption, Copyright (c) 2002-2003 W.P. van Paassen <peter@paassen.tmfweb.nl>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Module - "eruption.c"
 *
 * [02-2003] - W.P. van Paassen: Improvements, added some code of jwz from the pyro hack for a spherical distribution of the particles
 * [01-2003] - W.P. van Paassen: Port to X for use with XScreenSaver, the shadebob hack by Shane Smit was used as a template
 * [04-2002] - W.P. van Paassen: Creation for the Demo Effects Collection (http://demo-effects.sourceforge.net)
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

/*#define VERBOSE*/ 

char *progclass = "Eruption";

char *defaults [] = {
  ".background: black",
  ".foreground: white",
  "*cycles:   80",
  "*ncolors:  256",
  "*delay:    5000",
  "*particles: 300",
  "*cooloff: 2",
  "*gravity: 1",
  "*heat: 256",
  0
};

XrmOptionDescRec options [] = {
  { "-ncolors", ".ncolors", XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { "-particles",  ".particles",  XrmoptionSepArg, 0 },
  { "-cooloff",  ".cooloff",  XrmoptionSepArg, 0 },
  { "-gravity",  ".gravity",  XrmoptionSepArg, 0 },
  { "-heat",  ".heat",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

/* Slightly whacked, for better explosions
 */
#define PI_2000 6284
#define SPREAD 15

static int sin_cache[PI_2000];
static int cos_cache[PI_2000];

/*particle structure*/
typedef struct 
{
  short xpos, ypos, xdir, ydir;
  unsigned char colorindex;
  unsigned char dead;
} PARTICLE;

static PARTICLE *particles;
static unsigned short iWinWidth, iWinHeight;
static unsigned char **fire;
static unsigned short nParticleCount;
static unsigned char xdelta, ydelta, decay;
static signed char gravity;
static signed short heat;

static void
cache(void) /* jwz */
{               /*needs to be run once. Could easily be */
  int i;        /*reimplemented to run and cache at compile-time,*/
  double dA;    
  for (i=0; i<PI_2000; i++)
    {
      dA=sin(((double) (random() % (PI_2000/2)))/1000.0);
      /*Emulation of spherical distribution*/
      dA+=asin(frand(1.0))/M_PI_2*0.1;
      /*Approximating the integration of the binominal, for
        well-distributed randomness*/
      cos_cache[i]=-abs((int) (cos(((double)i)/1000.0)*dA*ydelta));
      sin_cache[i]=(int) (sin(((double)i)/1000.0)*dA*xdelta);
    }
}

void init_particle(PARTICLE* particle, signed short iColorCount, unsigned short xcenter, unsigned short ycenter)
{
  int v = random() % PI_2000;
  particle->xpos = xcenter - SPREAD + (random() % (SPREAD * 2));
  particle->ypos = ycenter - SPREAD + (random() % (SPREAD * 2));;
  particle->xdir = sin_cache[v];
  particle->ydir = cos_cache[v];
  particle->colorindex = iColorCount;
  particle->dead = 0;
}

static void Execute( Display *pDisplay,
                     Window MainWindow,
                     GC *pGC, XImage *pImage,
                     signed short iColorCount, unsigned long *aiColorVals )
{
  int i, j;
  unsigned int temp;

  /* move and draw particles into fire array */
  
  for (i = 0; i < nParticleCount; i++)
    {
      if (!particles[i].dead)
	{
	  particles[i].xpos += particles[i].xdir;
	  particles[i].ypos += particles[i].ydir;
	  
	  /* is particle dead? */
	  
	  if (particles[i].colorindex == 0)
	    {
	      particles[i].dead = 1;
	      continue;
	    }
	  
	  if (particles[i].xpos < 1)
	    {
	      particles[i].xpos = 1;
	      particles[i].xdir = -particles[i].xdir - 4;
	      particles[i].colorindex = iColorCount;
	    }
	  else if (particles[i].xpos >= iWinWidth - 2)
	    {
	      particles[i].xpos = iWinWidth - 2;
	      particles[i].xdir = -particles[i].xdir + 4;
	      particles[i].colorindex = iColorCount;
	    }
	  
	  if (particles[i].ypos < 1)
	    {
	      particles[i].ypos = 1;
	      particles[i].ydir = -particles[i].ydir;
	      particles[i].colorindex = iColorCount;
	    }
	  else if (particles[i].ypos >= iWinHeight - 3)
	    {
	      particles[i].ypos = iWinHeight- 3;
	      particles[i].ydir = (-particles[i].ydir >> 2) - (random() % 2);
	      particles[i].colorindex = iColorCount;
	    }
	  
	  /* gravity kicks in */
	  particles[i].ydir += gravity;
	  
	  /* particle cools off */
	  particles[i].colorindex--;
	  
	  /* draw particle */
	  fire[particles[i].ypos][particles[i].xpos] = particles[i].colorindex;
	  fire[particles[i].ypos][particles[i].xpos - 1] = particles[i].colorindex;
	  fire[particles[i].ypos + 1][particles[i].xpos] = particles[i].colorindex;
	  fire[particles[i].ypos - 1][particles[i].xpos] = particles[i].colorindex;
	  fire[particles[i].ypos][particles[i].xpos + 1] = particles[i].colorindex;
	}
    }
  
  /* create fire effect */ 
  for (i = 0; i < iWinHeight; i++)
    {
      for (j = 0; j < iWinWidth; j++)
	{
	  if (j + 1 >= iWinWidth)
	    temp = 0;
	  else
	    temp = fire[i][j + 1];

	  if (j - 1 >= 0)  
	    temp += fire[i][j - 1];

	  if (i - 1 >= 0)
	    {
	      temp += fire[i - 1][j];
	      if (j - 1 >= 0)
		temp += fire[i - 1][j - 1];
	      if (j + 1 < iWinWidth)
		temp += fire[i - 1][j + 1];
	    }
	  
	  if (i + 1 < iWinHeight)
	    {
	      temp += fire[i + 1][j];
	      if (j + 1 < iWinWidth)
		temp += fire[i + 1][j + 1];
	      if (j - 1 >= 0)
		temp += fire[i + 1][j - 1];
	    }
	  
	  temp >>= 3;
	  
	  if (temp > decay)
	    {
	      temp -= decay;
	    }
	  else
	    temp = 0;
	  
	  fire[i][j] = temp;
	}
    }
  
  memset( pImage->data, 0, pImage->bytes_per_line * pImage->height );
  
  /* draw fire array to screen */
  for (i = 0; i < iWinHeight; ++i)
    {
      for (j = 0; j < iWinWidth; ++j)
	{
	  if (fire[i][j] > 0)
	    XPutPixel( pImage, j, i, aiColorVals[ fire[i][j] ] );
	}
    }
  XPutImage( pDisplay, MainWindow, *pGC, pImage,
	     0,0,0,0, iWinWidth, iWinHeight );
  XSync( pDisplay, False );
}

static unsigned long * SetPalette(Display *pDisplay, Window Win, signed short *piColorCount )
{
	XWindowAttributes XWinAttribs;
	XColor Color, *aColors;
	unsigned long *aiColorVals;
	signed short iColor;

	XGetWindowAttributes( pDisplay, Win, &XWinAttribs );
	
	*piColorCount = get_integer_resource( "ncolors", "Integer" );
	if( *piColorCount < 16 )	*piColorCount = 16;
	if( *piColorCount > 255 )	*piColorCount = 256;

	aColors    = calloc( *piColorCount, sizeof(XColor) );
	aiColorVals = calloc( *piColorCount, sizeof(unsigned long) );
	
	Color.red = Color.green = Color.blue = 65535 / *piColorCount;

	/* create fire palette */
	for( iColor=0; iColor < *piColorCount; iColor++ )
	{
	  if (iColor < *piColorCount >> 3)
	    {
	      /* black to blue */
	      aColors[iColor].red = 0;
	      aColors[iColor].green = 0;
	      aColors[iColor].blue = Color.blue * (iColor << 1);
	    }
	  else if (iColor < *piColorCount >> 2)
	    {
	      /* blue to red */
	      signed short temp = (iColor - (*piColorCount >> 3));
	      aColors[iColor].red = Color.red * (temp << 3);
	      aColors[iColor].green = 0;
	      aColors[iColor].blue = 16383 - Color.blue * (temp << 1);
	    }
	  else if (iColor < (*piColorCount >> 2) + (*piColorCount >> 3))
	    {
	      /* red to yellow */
	      signed short temp = (iColor - (*piColorCount >> 2)) << 3;
	      aColors[iColor].red = 65535;
	      aColors[iColor].green = Color.green * temp;
	      aColors[iColor].blue = 0;
	    }
	  else if (iColor < *piColorCount >> 1)
	    {
	      /* yellow to white */
	      signed int temp = (iColor - ((*piColorCount >> 2) + (*piColorCount >> 3))) << 3;
	      aColors[iColor].red = 65535;
	      aColors[iColor].green = 65535;
	      aColors[iColor].blue = Color.blue * temp;
	    }
	  else
	    {
	      /* white */
	      aColors[iColor].red = aColors[iColor].green = aColors[iColor].blue = 65535;
	    }

	  if( !XAllocColor( pDisplay, XWinAttribs.colormap, &aColors[ iColor ] ) )
	    {
	      /* start all over with less colors */	
	      XFreeColors( pDisplay, XWinAttribs.colormap, aiColorVals, iColor, 0 );
	      free( aColors );
	      free( aiColorVals );
	      (*piColorCount)--;
	      aColors     = calloc( *piColorCount, sizeof(XColor) );
	      aiColorVals = calloc( *piColorCount, sizeof(unsigned long) );
	      iColor = -1;
	    }
	  else
	    aiColorVals[ iColor ] = aColors[ iColor ].pixel;
	}

	if (heat < *piColorCount)
	  *piColorCount = heat;
	
	free( aColors );

	XSetWindowBackground( pDisplay, Win, aiColorVals[ 0 ] );

	return aiColorVals;
}


static void Initialize( Display *pDisplay, Window Win, GC *pGC, XImage **ppImage )
{
	XGCValues gcValues;
	XWindowAttributes XWinAttribs;
	int iBitsPerPixel, i;

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

	/* create fire array */
	fire = calloc( iWinHeight, sizeof(unsigned char*));
	for (i = 0; i < iWinHeight; ++i)
	  fire[i] = calloc( iWinWidth, sizeof(unsigned char));

	/*create particles */
	particles = malloc (nParticleCount * sizeof(PARTICLE));
}

void screenhack(Display *pDisplay, Window Win )
{
	XWindowAttributes XWinAttribs;
	GC gc;
	signed short iColorCount = 0;
	unsigned long *aiColorVals = NULL;
	unsigned short sum = 0;
	XImage *pImage = NULL;
#ifdef VERBOSE
	time_t nTime = time( NULL );
	unsigned short iFrame = 0;
#endif  /*  VERBOSE */
	int delay, cycles, i;

	nParticleCount = get_integer_resource( "particles", "Integer" );
	if (nParticleCount < 100)
	  nParticleCount = 100;
	if (nParticleCount > 2000)
	  nParticleCount = 2000;

	decay = get_integer_resource( "cooloff", "Integer" );
	if (decay <= 0)
	  decay = 0;
	if (decay > 10)
	  decay = 10;

	gravity = get_integer_resource( "gravity", "Integer" );
	if (gravity < -5)
	  gravity = -5;
	if (gravity > 5)
	  gravity = 5;

	heat = get_integer_resource( "heat", "Integer" );
	if (heat < 64)
	  heat = 64;
	if (heat > 256)
	  heat = 256;

#ifdef VERBOSE 
	printf( "%s: Allocated %d particles\n", progclass, nParticleCount );
#endif  /*  VERBOSE */

	Initialize( pDisplay, Win, &gc, &pImage );

	ydelta = 0;
	while (sum < (iWinHeight >> 1) - SPREAD)
	  {
	    ydelta++;
	    sum += ydelta;
	  }

	sum = 0;
	while (sum < (iWinWidth >> 3))
	  {
	    xdelta++;
	    sum += xdelta;
	  }

	delay = get_integer_resource( "delay", "Integer" );
	cycles = get_integer_resource( "cycles", "Integer" );
	i = cycles;

	cache();
	
	XGetWindowAttributes( pDisplay, Win, &XWinAttribs );
	XFreeColors( pDisplay, XWinAttribs.colormap, aiColorVals, iColorCount, 0 );
	free( aiColorVals );
	aiColorVals = SetPalette( pDisplay, Win, &iColorCount );
	XClearWindow( pDisplay, Win );
	memset( pImage->data, 0, pImage->bytes_per_line * pImage->height );

	while( 1 )
	  {
	    screenhack_handle_events( pDisplay );
	    
	    if( i++ >= cycles )
	      {
		/* compute random center */
		unsigned short xcenter, ycenter;
		xcenter = random() % iWinWidth;
		ycenter = random() % iWinHeight;
		
		for (i = 0; i < nParticleCount; i++)
		  init_particle(particles + i, iColorCount - 1, xcenter, ycenter);
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
	for (i = 0; i < iWinHeight; ++i)
	  free( fire[i] );
	free( fire );
	free( particles );
}

/* End of Module - "eruption.c" */

