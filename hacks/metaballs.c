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

/*#define VERBOSE*/ 

/*blob structure*/
typedef struct 
{
  short xpos,ypos;
} BLOB;

struct state {
  Display *dpy;
  Window window;

  unsigned short iWinWidth, iWinHeight;
  char *sColor;

  unsigned int nBlobCount;
  unsigned char radius;
  unsigned char delta;
  unsigned char dradius;
  unsigned short sradius;
  unsigned char **blob;
  BLOB *blobs;
  unsigned char **blub;

  int delay, cycles;
  signed short iColorCount;
  unsigned long *aiColorVals;
  XImage *pImage;
  GC gc;
  int draw_i;
};


#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static void init_blob(struct state *st, BLOB *blob)
{
  blob->xpos = st->iWinWidth/4  + BELLRAND(st->iWinWidth/2)  - st->radius;
  blob->ypos = st->iWinHeight/4 + BELLRAND(st->iWinHeight/2) - st->radius;
}

static void Execute( struct state *st )
{
	unsigned int i, j, k;

	/* clear st->blub array */
	for (i = 0; i < st->iWinHeight; ++i)
	  memset(st->blub[i], 0, st->iWinWidth * sizeof(unsigned char));

	/* move st->blobs */
	for (i = 0; i < st->nBlobCount; i++)
	{
	  st->blobs[i].xpos += -st->delta + (int)((st->delta + .5f) * frand(2.0));
	  st->blobs[i].ypos += -st->delta + (int)((st->delta + .5f) * frand(2.0));
	}

	/* draw st->blobs to st->blub array */
	for (k = 0; k < st->nBlobCount; ++k)
	  { 
	    if (st->blobs[k].ypos > -st->dradius && st->blobs[k].xpos > -st->dradius && st->blobs[k].ypos < st->iWinHeight && st->blobs[k].xpos < st->iWinWidth)
	      {
		for (i = 0; i < st->dradius; ++i)
		  {
		    if (st->blobs[k].ypos + i >= 0 && st->blobs[k].ypos + i < st->iWinHeight)
		      {
			for (j = 0; j < st->dradius; ++j)
			  {
			    if (st->blobs[k].xpos + j >= 0 && st->blobs[k].xpos + j < st->iWinWidth)
			      {
				if (st->blub[st->blobs[k].ypos + i][st->blobs[k].xpos + j] < st->iColorCount-1)
				  {
				    if (st->blub[st->blobs[k].ypos + i][st->blobs[k].xpos + j] + st->blob[i][j] > st->iColorCount-1)
				      st->blub[st->blobs[k].ypos + i][st->blobs[k].xpos + j] = st->iColorCount-1;
				    else 
				      st->blub[st->blobs[k].ypos + i][st->blobs[k].xpos + j] += st->blob[i][j];     
				  }
			      }
			  }
		      }
		  }
	      }
	    else
	      init_blob(st, st->blobs + k);
	  }

	memset( st->pImage->data, 0, st->pImage->bytes_per_line * st->pImage->height);

	/* draw st->blub array to screen */
	for (i = 0; i < st->iWinHeight; ++i)
	  {
	    for (j = 0; j < st->iWinWidth; ++j)
	      {
		if (st->aiColorVals[st->blub[i][j]] > 0)
		  XPutPixel( st->pImage, j, i, st->aiColorVals[st->blub[i][j]] );
	      }
	  }

	XPutImage( st->dpy, st->window, st->gc, st->pImage,
		   0, 0, 0, 0, st->iWinWidth, st->iWinHeight );
}

static unsigned long * SetPalette(struct state *st )
{
	XWindowAttributes XWinAttribs;
	XColor Color, *aColors;
	signed short iColor;
	float nHalfColors;
	
	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );
	
	Color.red =   random() % 0xFFFF;
	Color.green = random() % 0xFFFF;
	Color.blue =  random() % 0xFFFF;

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
	
	for( iColor=0; iColor < st->iColorCount; iColor++ )
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
			(st->iColorCount)--;

                        if (st->iColorCount < 6)
                          {
                            fprintf (stderr, "%s: insufficient colors!\n",
                                     progname);
                            exit (1);
                          }

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
	int /*iBitsPerPixel,*/ i, j;
	unsigned int distance_squared;
	float fraction;

	/* Create the Image for drawing */
	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );

	/* Find the preferred bits-per-pixel. (jwz) */
#if 0
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
#endif

	/*  Create the GC. */
	st->gc = XCreateGC( st->dpy, st->window, 0, &gcValues );

	st->pImage = XCreateImage( st->dpy, XWinAttribs.visual, XWinAttribs.depth, ZPixmap, 0, NULL,
							  XWinAttribs.width, XWinAttribs.height, BitmapPad( st->dpy ), 0 );
	(st->pImage)->data = calloc((st->pImage)->bytes_per_line, (st->pImage)->height);

	st->iWinWidth = XWinAttribs.width;
	st->iWinHeight = XWinAttribs.height;

	/*  Get the base color. */
	st->sColor = get_string_resource(st->dpy,  "color", "Color" );

	/*  Get the st->delta. */
	st->delta = get_integer_resource(st->dpy,  "delta", "Integer" );
	if (st->delta < 1)
	  st->delta = 1;
	else if (st->delta > 20)
	  st->delta = 20;
	
	/*  Get the st->radius. */
	st->radius = get_integer_resource(st->dpy,  "radius", "Integer" );
	if (st->radius < 2)
	  st->radius = 2;
	if (st->radius > 100)
	  st->radius = 100;
	
	st->radius = (st->radius / 100.0) * (st->iWinHeight >> 3);
        if (st->radius >= 128) /* should use UCHAR_MAX? */
          st->radius = 127; /* st->dradius should fit in u_char */

	st->dradius = st->radius * 2;
	st->sradius = st->radius * st->radius;

	/* create st->blob */
	st->blob = malloc ( st->dradius * sizeof(unsigned char*));
	for (i = 0; i < st->dradius; ++i)
	  st->blob[i] = malloc( st->dradius * sizeof(unsigned char));

	/* create st->blub array */
	st->blub = malloc( st->iWinHeight * sizeof(unsigned char*));
	for (i = 0; i < st->iWinHeight; ++i)
	  st->blub[i] = malloc( st->iWinWidth * sizeof(unsigned char));

	/* create st->blob */
	for (i = -st->radius; i < st->radius; ++i)
	  {
	    for (j = -st->radius; j < st->radius; ++j)
	      {
		distance_squared = i * i + j * j;
		if (distance_squared <= st->sradius)
		  {
		    /* compute density */     
		    fraction = (float)distance_squared / (float)st->sradius;
		    st->blob[i + st->radius][j + st->radius] = pow((1.0 - (fraction * fraction)),4.0) * 255.0;
		  }
		else
		  {
		    st->blob[i + st->radius][j + st->radius] = 0;
		  }
	      }    
	  }
	
	for (i = 0; i < st->nBlobCount; i++)
	  {
	    init_blob(st, st->blobs + i);
	  }
}

static void *
metaballs_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
#ifdef VERBOSE
  time_t nTime = time( NULL );
  unsigned short iFrame = 0;
#endif  /*  VERBOSE */

  st->dpy = dpy;
  st->window = window;

  st->nBlobCount = get_integer_resource(st->dpy,  "count", "Integer" );
  if( st->nBlobCount > 255 ) st->nBlobCount = 255;
  if( st->nBlobCount <  2 ) st->nBlobCount = 2;

  if( ( st->blobs = calloc( st->nBlobCount, sizeof(BLOB) ) ) == NULL )
    {
      fprintf( stderr, "%s: Could not allocate %d Blobs\n", progname, st->nBlobCount );
      abort();
    }
#ifdef VERBOSE 
  printf( "%s: Allocated %d Blobs\n", progname, st->nBlobCount );
#endif  /*  VERBOSE */

  Initialize( st );

  st->delay = get_integer_resource(st->dpy,  "delay", "Integer" );
  st->cycles = get_integer_resource(st->dpy,  "cycles", "Integer" );

  st->draw_i = -1;
  return st;
}



static unsigned long
metaballs_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if( st->draw_i < 0 || st->draw_i++ >= st->cycles )
    {
      int i;
      XWindowAttributes XWinAttribs;
      XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );

      memset( st->pImage->data, 0, st->pImage->bytes_per_line * st->pImage->height );
      XFreeColors( st->dpy, XWinAttribs.colormap, st->aiColorVals, st->iColorCount, 0 );
      free( st->aiColorVals );
      st->aiColorVals = SetPalette( st );
      XClearWindow( st->dpy, st->window );
      for (i = 0; i < st->nBlobCount; i++)
        {
          init_blob(st, st->blobs + i);
        }
      st->draw_i = 0;
    }

  Execute( st );

#ifdef VERBOSE
  iFrame++;
  if( nTime - time( NULL ) )
    {
      printf( "%s: %d FPS\n", progname, iFrame );
      nTime = time( NULL );
      iFrame = 0;
    }
#endif  /*  VERBOSE */

  return st->delay;
}


static void
metaballs_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
metaballs_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
metaballs_free (Display *dpy, Window window, void *closure)
{
#if 0
  struct state *st = (struct state *) closure;
	free( st->pImage->data );
	XDestroyImage( st->pImage );
	free( st->aiColorVals );
	free( st->blobs );
	for (i = 0; i < st->iWinHeight; ++i)
	  free( st->blub[i] );
	free( st->blub );
	for (i = 0; i < st->dradius; ++i)
	  free( st->blob[i] );
	free( st->blob );
#endif
}


static const char *metaballs_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*color:    random",
  "*count:    10",
  "*cycles:   1000",
  "*ncolors:  256",
  "*delay:    10000",
  "*radius:   100",
  "*delta:   3",
  0
};

static XrmOptionDescRec metaballs_options [] = {
  { "-color",   ".color",   XrmoptionSepArg, 0 },
  { "-ncolors", ".ncolors", XrmoptionSepArg, 0 },
  { "-count",   ".count",   XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-cycles",  ".cycles",  XrmoptionSepArg, 0 },
  { "-radius",  ".radius",  XrmoptionSepArg, 0 },
  { "-delta",  ".delta",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("MetaBalls", metaballs)

/* End of Module - "metaballs.c" */

