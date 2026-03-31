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
 * [01-2015] - Dave Odell <dmo2118@gmail.com>: Performance tweaks. Also, click-for-explosion.
 * [02-2003] - W.P. van Paassen: Improvements, added some code of jwz from the pyro hack for a spherical distribution of the particles
 * [01-2003] - W.P. van Paassen: Port to X for use with XScreenSaver, the shadebob hack by Shane Smit was used as a template
 * [04-2002] - W.P. van Paassen: Creation for the Demo Effects Collection (http://demo-effects.sourceforge.net)
 */

#include <assert.h>
#include <math.h>
#include "screenhack.h"
#include "xshm.h"

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#else
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
#endif

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
  unsigned char *fire;
  unsigned short nParticleCount;
  unsigned char xdelta, ydelta, decay;
  signed char gravity;
  signed short heat;

  int cycles, delay;
  GC gc;
  signed short iColorCount;
  unsigned long *aiColorVals;
  XImage *pImage;
  XShmSegmentInfo shm_info;

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

#define X_PAD 1 /* Could be more if anybody wants the blur to fall off the left/right edges. */
#define Y_PAD 1


static void Execute( struct state *st )
{
  XImage *img = st->pImage;
  int i, j;
  size_t fire_pitch = st->iWinWidth + X_PAD * 2;
  unsigned char *line0, *line1, *line2;

  /* move particles */
  
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
	      if (st->particles[i].xpos < 1) st->particles[i].xpos = 1;
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
	      if (st->particles[i].ypos < 1) st->particles[i].ypos = 1;
	      st->particles[i].ydir = (-st->particles[i].ydir >> 2) - (random() % 2);
	      st->particles[i].colorindex = st->iColorCount;
	    }

	  
	  /* st->gravity kicks in */
	  st->particles[i].ydir += st->gravity;
	  
	  /* particle cools off */
	  st->particles[i].colorindex--;
	}
    }

  /* draw particles into st->fire array */
  for( i = 0; i < st->nParticleCount; i++ )
    {
      PARTICLE *p = &st->particles[i];
      if( !p->dead && p->ypos >= -Y_PAD + 1 && p->ypos < st->iWinHeight + Y_PAD - 1 )
        {
          /* draw particle */
          unsigned char *center = st->fire + (p->ypos - -Y_PAD) * fire_pitch + (p->xpos + X_PAD);
          unsigned char color = p->colorindex;
          *center = color;
          center[-1] = color;
          if (p->ypos < st->iWinHeight + Y_PAD - 2)
            center[fire_pitch] = color;
          if (p->ypos >= -Y_PAD + 2)
            center[-fire_pitch] = color;
          center[1] = color;
        }
    }

  line0 = st->fire + X_PAD;
  line1 = line0 + fire_pitch;
  line2 = line1 + fire_pitch;

  /* create st->fire effect */

  switch( img->bits_per_pixel )
    {
    case 8:
    case 16:
    case 32:
      break;
    default:
      memset( img->data, 0, img->bytes_per_line * img->height );
      break;
    }

  for( i = -Y_PAD + 1; i < st->iWinHeight + Y_PAD - 1; i++ )
    {
      const int j0 = -X_PAD + 1;

      unsigned
        t0 = line0[j0 - 1] + line1[j0 - 1] + line2[j0 - 1],
        t1 = line0[j0] + line1[j0] + line2[j0];

      unsigned j1 = st->iWinWidth + X_PAD;

      /* This is basically like the GIMP's "Convolution Matrix" filter, with
         the following settings:
         Matrix:
         | 1 1 1 |    Divisor = 8
         | 1 0 1 |    Offset = -eruption.cooloff
         | 1 1 1 |
         Except that the effect is applied to each pixel individually, in order
         from left-to-right, then top-to-bottom, resulting in a slight smear
         going rightwards and downwards.
       */

      for( j = j0 + 1; j != j1; j++ )
        {
          unsigned t2 = line0[j] + line1[j] + line2[j];
          unsigned char *px = line1 + j - 1;
          int temp;
          t1 -= *px;
          temp = t0 + t1 + t2 - st->decay;
          temp = temp >= 0 ? temp >> 3 : 0;
          *px = temp;
          t0 = t1 + temp;
          t1 = t2;
        }

      if( i >= 0 && i < st->iWinHeight )
        {
          /* draw st->fire array to screen */
          void *out_ptr = img->data + img->bytes_per_line * i;

          switch( img->bits_per_pixel )
            {
            case 32:
              {
                uint32_t *out = (uint32_t *)out_ptr;
                for( j = 0; j < st->iWinWidth; ++j )
                  out[j] = st->aiColorVals[ line1[j] ];
              }
              break;
            case 16:
              {
                uint16_t *out = (uint16_t *)out_ptr;
                for( j = 0; j < st->iWinWidth; ++j )
                  out[j] = st->aiColorVals[ line1[j] ];
              }
              break;
            case 8:
              {
                uint8_t *out = (uint8_t *)out_ptr;
                for( j = 0; j < st->iWinWidth; ++j )
                  out[j] = st->aiColorVals[ line1[j] ];
              }
              break;
            default:
              for( j = 0; j < st->iWinWidth; ++j )
                {
                  if( line1[j] )
                    XPutPixel( img, j, i, st->aiColorVals[ line1[j] ] );
                }
              break;
            }
        }

      line0 += fire_pitch;
      line1 += fire_pitch;
      line2 += fire_pitch;
    }
  put_xshm_image( st->dpy, st->window, st->gc, st->pImage,
                  0,0,0,0, st->iWinWidth, st->iWinHeight, &st->shm_info );
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


static void DestroyImage (struct state *st)
{
  free (st->fire);
  destroy_xshm_image (st->dpy, st->pImage, &st->shm_info);
}

static void CreateImage( struct state *st, XWindowAttributes *XWinAttribs )
{
  /* Create the Image for drawing */

  /* Must be NULL because of how DestroyImage works. */
  assert( !st->fire );

  st->pImage = create_xshm_image( st->dpy, XWinAttribs->visual,
                                  XWinAttribs->depth, ZPixmap, &st->shm_info,
                                  XWinAttribs->width, XWinAttribs->height );

  memset( (st->pImage)->data, 0,
          (st->pImage)->bytes_per_line * (st->pImage)->height);

  st->iWinWidth = XWinAttribs->width;
  st->iWinHeight = XWinAttribs->height;

  /* create st->fire array */
  st->fire = calloc( (st->iWinHeight + Y_PAD * 2) * (st->iWinWidth + X_PAD * 2), sizeof(unsigned char));
}

static void Initialize( struct state *st, XWindowAttributes *XWinAttribs )
{
	XGCValues gcValues;

	/*  Create the GC. */
	st->gc = XCreateGC( st->dpy, st->window, 0, &gcValues );

	CreateImage (st, XWinAttribs);

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
	st->decay <<= 3;

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

	XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );

	Initialize( st, &XWinAttribs );

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

	cache(st);
	
	XFreeColors( st->dpy, XWinAttribs.colormap, st->aiColorVals, st->iColorCount, 0 );
	free( st->aiColorVals );
	st->aiColorVals = SetPalette( st );
	XClearWindow( st->dpy, st->window );

        st->draw_i = -1;

        return st;
}


static void
new_eruption (struct state *st, unsigned short xcenter, unsigned short ycenter)
{
  for (st->draw_i = 0; st->draw_i < st->nParticleCount; st->draw_i++)
    init_particle(st, st->particles + st->draw_i, xcenter, ycenter);
  st->draw_i = 0;
}


static void
random_eruption (struct state *st)
{
  /* compute random center */
  new_eruption (st, random() % st->iWinWidth, random() % st->iWinHeight);
}


static unsigned long
eruption_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
	    
  if( st->draw_i < 0 || st->draw_i++ >= st->cycles )
    {
      random_eruption (st);
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
  struct state *st = (struct state *) closure;
  XWindowAttributes XWinAttribs;

  DestroyImage (st);
  st->fire = NULL;

  XGetWindowAttributes( st->dpy, st->window, &XWinAttribs );
  XWinAttribs.width = w;
  XWinAttribs.height = h;
  CreateImage (st, &XWinAttribs);
  st->draw_i = -1;
}

static Bool
eruption_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;

  if (event->type == ButtonPress)
    {
      new_eruption (st, event->xbutton.x, event->xbutton.y);
      return True;
    }
  else if (screenhack_event_helper (dpy, window, event))
    {
      random_eruption (st);
      return True;
    }

  return False;
}

static void
eruption_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  DestroyImage (st);
  free (st->aiColorVals);
  free (st->particles);
  XFreeGC (dpy, st->gc);
  free (st);
}


static const char *eruption_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*fpsTop:	true",
  "*cycles:   80",
  "*ncolors:  256",
  "*delay:    10000",
  "*particles: 300",
  "*cooloff: 2",
  "*gravity: 1",
  "*heat: 256",
  ".lowrez: true",  /* Too slow on macOS Retina screens otherwise */
  "*useSHM: True",
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
#ifdef HAVE_XSHM_EXTENSION
  { "-shm", ".useSHM", XrmoptionNoArg, "True" },
  { "-no-shm",  ".useSHM", XrmoptionNoArg, "False" },
#endif
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Eruption", eruption)

/* End of Module - "eruption.c" */

