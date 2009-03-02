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

/*#define VERBOSE*/ 

/* Slightly whacked, for better explosions
 */
#define PI_2000 6284
#define SPREAD 15

/*particle structure*/
typedef struct 
{
  short xpos, ypos, xdir, ydir;
  unsigned char colorindex;
  unsigned char dead;
} PARTICLE;

struct state {
  Display *dpy;
  Window window;

 int sin_cache[PI_2000];
 int cos_cache[PI_2000];

  PARTICLE *particles;
  unsigned short iWinWidth, iWinHeight;
  unsigned char **fire;
  unsigned short nParticleCount;
  unsigned char xdelta, ydelta, decay;
  signed char gravity;
  signed short heat;

  int cycles, delay;
  GC gc;
  signed short iColorCount;
  unsigned long *aiColorVals;
  XImage *pImage;

  int draw_i;
};

static void
cache(struct state *st) /* jwz */
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
      st->cos_cache[i]=-abs((int) (cos(((double)i)/1000.0)*dA*st->ydelta));
      st->sin_cache[i]=(int) (sin(((double)i)/1000.0)*dA*st->xdelta);
    }
}

static void init_particle(struct state *st, PARTICLE* particle, unsigned short xcenter, unsigned short ycenter)
{
  int v = random() % PI_2000;
  particle->xpos = xcenter - SPREAD + (random() % (SPREAD * 2));
  particle->ypos = ycenter - SPREAD + (random() % (SPREAD * 2));;
  particle->xdir = st->sin_cache[v];
  particle->ydir = st->cos_cache[v];
  particle->colorindex = st->iColorCount-1;
  particle->dead = 0;
}

static void Execute( struct state *st )
{
  int i, j;
  unsigned int temp;

  /* move and draw particles into st->fire array */
  
  for (i = 0; i < st->nParticleCount; i++)
    {
      if (!st->particles[i].dead)
	{
	  st->particles[i].xpos += st->particles[i].xdir;
	  st->particles[i].ypos += st->particles[i].ydir;
	  
	  /* is particle dead? */
	  
	  if (st->particles[i].colorindex == 0)
	    {
	      st->particles[i].dead = 1;
	      continue;
	    }
	  
	  if (st->particles[i].xpos < 1)
	    {
	      st->particles[i].xpos = 1;
	      st->particles[i].xdir = -st->particles[i].xdir - 4;
	      st->particles[i].colorindex = st->iColorCount;
	    }
	  else if (st->particles[i].xpos >= st->iWinWidth - 2)
	    {
	      st->particles[i].xpos = st->iWinWidth - 2;
	      st->particles[i].xdir = -st->particles[i].xdir + 4;
	      st->particles[i].colorindex = st->iColorCount;
	    }
	  
	  if (st->particles[i].ypos < 1)
	    {
	      st->particles[i].ypos = 1;
	      st->particles[i].ydir = -st->particles[i].ydir;
	      st->particles[i].colorindex = st->iColorCount;
	    }
	  else if (st->particles[i].ypos >= st->iWinHeight - 3)
	    {
	      st->particles[i].ypos = st->iWinHeight- 3;
	      st->particles[i].ydir = (-st->particles[i].ydir >> 2) - (random() % 2);
	      st->particles[i].colorindex = st->iColorCount;
	    }
	  
	  /* st->gravity kicks in */
	  st->particles[i].ydir += st->gravity;
	  
	  /* particle cools off */
	  st->particles[i].colorindex--;
	  
	  /* draw particle */
	  st->fire[st->particles[i].ypos][st->particles[i].xpos] = st->particles[i].colorindex;
	  st->fire[st->particles[i].ypos][st->particles[i].xpos - 1] = st->particles[i].colorindex;
	  st->fire[st->particles[i].ypos + 1][st->particles[i].xpos] = st->particles[i].colorindex;
	  st->fire[st->particles[i].ypos - 1][st->particles[i].xpos] = st->particles[i].colorindex;
	  st->fire[st->particles[i].ypos][st->particles[i].xpos + 1] = st->particles[i].colorindex;
	}
    }
  
  /* create st->fire effect */ 
  for (i = 0; i < st->iWinHeight; i++)
    {
      for (j = 0; j < st->iWinWidth; j++)
	{
	  if (j + 1 >= st->iWinWidth)
	    temp = 0;
	  else
	    temp = st->fire[i][j + 1];

	  if (j - 1 >= 0)  
	    temp += st->fire[i][j - 1];

	  if (i - 1 >= 0)
	    {
	      temp += st->fire[i - 1][j];
	      if (j - 1 >= 0)
		temp += st->fire[i - 1][j - 1];
	      if (j + 1 < st->iWinWidth)
		temp += st->fire[i - 1][j + 1];
	    }
	  
	  if (i + 1 < st->iWinHeight)
	    {
	      temp += st->fire[i + 1][j];
	      if (j + 1 < st->iWinWidth)
		temp += st->fire[i + 1][j + 1];
	      if (j - 1 >= 0)
		temp += st->fire[i + 1][j - 1];
	    }
	  
	  temp >>= 3;
	  
	  if (temp > st->decay)
	    {
	      temp -= st->decay;
	    }
	  else
	    temp = 0;
	  
	  st->fire[i][j] = temp;
	}
    }
  
  memset( st->pImage->data, 0, st->pImage->bytes_per_line * st->pImage->height );
  
  /* draw st->fire array to screen */
  for (i = 0; i < st->iWinHeight; ++i)
    {
      for (j = 0; j < st->iWinWidth; ++j)
	{
	  if (st->fire[i][j] > 0)
	    XPutPixel( st->pImage, j, i, st->aiColorVals[ st->fire[i][j] ] );
	}
    }
  XPutImage( st->dpy, st->window, st->gc, st->pImage,
	     0,0,0,0, st->iWinWidth, st->iWinHeight );
}

static unsigned long * SetPalette(struct state *st)
{
	XWindowAttributes XWinAttribs;
	XColor Color, *aColors;
	signed short iColor;

	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );
	
	st->iColorCount = get_integer_resource(st->dpy,  "ncolors", "Integer" );
	if( st->iColorCount < 16 )	st->iColorCount = 16;
	if( st->iColorCount > 255 )	st->iColorCount = 256;

	aColors    = calloc( st->iColorCount, sizeof(XColor) );
	st->aiColorVals = calloc( st->iColorCount, sizeof(unsigned long) );
	
	Color.red = Color.green = Color.blue = 65535 / st->iColorCount;

	/* create st->fire palette */
	for( iColor=0; iColor < st->iColorCount; iColor++ )
	{
	  if (iColor < st->iColorCount >> 3)
	    {
	      /* black to blue */
	      aColors[iColor].red = 0;
	      aColors[iColor].green = 0;
	      aColors[iColor].blue = Color.blue * (iColor << 1);
	    }
	  else if (iColor < st->iColorCount >> 2)
	    {
	      /* blue to red */
	      signed short temp = (iColor - (st->iColorCount >> 3));
	      aColors[iColor].red = Color.red * (temp << 3);
	      aColors[iColor].green = 0;
	      aColors[iColor].blue = 16383 - Color.blue * (temp << 1);
	    }
	  else if (iColor < (st->iColorCount >> 2) + (st->iColorCount >> 3))
	    {
	      /* red to yellow */
	      signed short temp = (iColor - (st->iColorCount >> 2)) << 3;
	      aColors[iColor].red = 65535;
	      aColors[iColor].green = Color.green * temp;
	      aColors[iColor].blue = 0;
	    }
	  else if (iColor < st->iColorCount >> 1)
	    {
	      /* yellow to white */
	      signed int temp = (iColor - ((st->iColorCount >> 2) + (st->iColorCount >> 3))) << 3;
	      aColors[iColor].red = 65535;
	      aColors[iColor].green = 65535;
	      aColors[iColor].blue = Color.blue * temp;
	    }
	  else
	    {
	      /* white */
	      aColors[iColor].red = aColors[iColor].green = aColors[iColor].blue = 65535;
	    }

	  if( !XAllocColor( st->dpy, XWinAttribs.colormap, &aColors[ iColor ] ) )
	    {
	      /* start all over with less colors */	
	      XFreeColors( st->dpy, XWinAttribs.colormap, st->aiColorVals, iColor, 0 );
	      free( aColors );
	      free( st->aiColorVals );
	      (st->iColorCount)--;
	      aColors     = calloc( st->iColorCount, sizeof(XColor) );
	      st->aiColorVals = calloc( st->iColorCount, sizeof(unsigned long) );
	      iColor = -1;
	    }
	  else
	    st->aiColorVals[ iColor ] = aColors[ iColor ].pixel;
	}

	if (st->heat < st->iColorCount)
	  st->iColorCount = st->heat;
	
	free( aColors );

	XSetWindowBackground( st->dpy, st->window, st->aiColorVals[ 0 ] );

	return st->aiColorVals;
}


static void Initialize( struct state *st )
{
	XGCValues gcValues;
	XWindowAttributes XWinAttribs;
	int iBitsPerPixel, i;

	/* Create the Image for drawing */
	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );

	/* Find the preferred bits-per-pixel. (jwz) */
	{
		int pfvc = 0;
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

	/*  Create the GC. */
	st->gc = XCreateGC( st->dpy, st->window, 0, &gcValues );

	st->pImage = XCreateImage( st->dpy, XWinAttribs.visual, XWinAttribs.depth, ZPixmap, 0, NULL,
							  XWinAttribs.width, XWinAttribs.height, BitmapPad( st->dpy ), 0 );
	(st->pImage)->data = calloc((st->pImage)->bytes_per_line, (st->pImage)->height);

	st->iWinWidth = XWinAttribs.width;
	st->iWinHeight = XWinAttribs.height;

	/* create st->fire array */
	st->fire = calloc( st->iWinHeight, sizeof(unsigned char*));
	for (i = 0; i < st->iWinHeight; ++i)
	  st->fire[i] = calloc( st->iWinWidth, sizeof(unsigned char));

	/*create st->particles */
	st->particles = malloc (st->nParticleCount * sizeof(PARTICLE));
}

static void *
eruption_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
	XWindowAttributes XWinAttribs;
	unsigned short sum = 0;
#ifdef VERBOSE
	time_t nTime = time( NULL );
	unsigned short iFrame = 0;
#endif  /*  VERBOSE */
        int i;

  st->dpy = dpy;
  st->window = window;

	st->nParticleCount = get_integer_resource(st->dpy,  "particles", "Integer" );
	if (st->nParticleCount < 100)
	  st->nParticleCount = 100;
	if (st->nParticleCount > 2000)
	  st->nParticleCount = 2000;

	st->decay = get_integer_resource(st->dpy,  "cooloff", "Integer" );
	if (st->decay <= 0)
	  st->decay = 0;
	if (st->decay > 10)
	  st->decay = 10;

	st->gravity = get_integer_resource(st->dpy,  "gravity", "Integer" );
	if (st->gravity < -5)
	  st->gravity = -5;
	if (st->gravity > 5)
	  st->gravity = 5;

	st->heat = get_integer_resource(st->dpy,  "heat", "Integer" );
	if (st->heat < 64)
	  st->heat = 64;
	if (st->heat > 256)
	  st->heat = 256;

#ifdef VERBOSE 
	printf( "%s: Allocated %d st->particles\n", progclass, st->nParticleCount );
#endif  /*  VERBOSE */

	Initialize( st );

	st->ydelta = 0;
	while (sum < (st->iWinHeight >> 1) - SPREAD)
	  {
	    st->ydelta++;
	    sum += st->ydelta;
	  }

	sum = 0;
	while (sum < (st->iWinWidth >> 3))
	  {
	    st->xdelta++;
	    sum += st->xdelta;
	  }

	st->delay = get_integer_resource(st->dpy,  "delay", "Integer" );
	st->cycles = get_integer_resource(st->dpy,  "cycles", "Integer" );
	i = st->cycles;

	cache(st);
	
	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );
	XFreeColors( st->dpy, XWinAttribs.colormap, st->aiColorVals, st->iColorCount, 0 );
	free( st->aiColorVals );
	st->aiColorVals = SetPalette( st );
	XClearWindow( st->dpy, st->window );
	memset( st->pImage->data, 0, st->pImage->bytes_per_line * st->pImage->height );

        st->draw_i = -1;

        return st;
}


static unsigned long
eruption_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
	    
  if( st->draw_i < 0 || st->draw_i++ >= st->cycles )
    {
      /* compute random center */
      unsigned short xcenter, ycenter;
      xcenter = random() % st->iWinWidth;
      ycenter = random() % st->iWinHeight;
		
      for (st->draw_i = 0; st->draw_i < st->nParticleCount; st->draw_i++)
        init_particle(st, st->particles + st->draw_i, xcenter, ycenter);
      st->draw_i = 0;
    }
	    
  Execute( st );
	    
#ifdef VERBOSE
  iFrame++;
  if( nTime - time( NULL ) )
    {
      printf( "%s: %d FPS\n", progclass, iFrame );
      nTime = time( NULL );
      iFrame = 0;
    }
#endif  /*  VERBOSE */

  return st->delay;
}
	

static void
eruption_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
eruption_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
eruption_free (Display *dpy, Window window, void *closure)
{
#if 0
  struct state *st = (struct state *) closure;
	free( st->pImage->data );
	XDestroyImage( st->pImage );
	free( st->aiColorVals );
	for (i = 0; i < st->iWinHeight; ++i)
	  free( st->fire[i] );
	free( st->fire );
	free( st->particles );
#endif
}


static const char *eruption_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*cycles:   80",
  "*ncolors:  256",
  "*delay:    10000",
  "*particles: 300",
  "*cooloff: 2",
  "*gravity: 1",
  "*heat: 256",
  0
};

static XrmOptionDescRec eruption_options [] = {
  { "-ncolors", ".ncolors", XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { "-particles",  ".particles",  XrmoptionSepArg, 0 },
  { "-cooloff",  ".cooloff",  XrmoptionSepArg, 0 },
  { "-gravity",  ".gravity",  XrmoptionSepArg, 0 },
  { "-heat",  ".heat",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Eruption", eruption)

/* End of Module - "eruption.c" */

