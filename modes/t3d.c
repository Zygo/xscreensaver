/* -*- Mode: C; tab-width: 4 -*- */
/* t3d --- Flying Balls Clock Demo */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)t3d.c	5.0 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1998 Bernd Paysan , paysan@informatik.tu-muenchen.de.
 *
 * Copy, modify, and distribute T3D either under GPL  version 2 or newer,
 * or under the standard MIT/X license notice.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * partly based on flying balls demo by Georg Acher,
 * acher@informatik.tu-muenchen.de
 * (developed on HP9000/720 (55 MIPS,20 MFLOPS) )
 *
 * Todo for xlock:
 *    -Set default options right and make it more "random"
 *    -Add original colour scheme
 *    -Get trackmouse is working
 *    -Add some more options which were handled in the original by the XEvents.
 *    -Get working better in password window.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 26-Jan-2000: joukj@hrem.stm.tudelft.nl (Jouk Jansen) : adapted for
 *              xlockmore. Modified colour-schemes.
 * 04-Jan-1999: jwz@jwz.org -- adapted to xscreensaver framework,
 *              to take advantage of the command-line options
 *              provided by screenhack.c.
 */
#ifdef WIN32
#include <sys/time.h>
#else
#include <time.h>
#endif /* WIN32 */

#define FASTDRAW
#define FASTCOPY
#undef USE_POLYGON

#ifdef STANDALONE
#define MODE_t3d
#define PROGCLASS "t3d"
#define HACK_INIT init_t3d
#define HACK_DRAW draw_t3d
#define t3d_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*cycles: 60000 \n" \
 "*ncolors: 200 \n" \
 "*mouse: False \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */

#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */

#include <time.h>
#ifdef TIME_WITH_SYS_TIME /* for sco */
#include <sys/time.h>
#endif

#ifdef MODE_t3d

#define DEF_TRACKMOUSE  "False"
#define DEF_MOVE "0.5"
#define DEF_WOBBLE "2.0"
#define DEF_MAG "10.0"
#define DEF_FAST "50"
#define DEF_MINUTES "False"
#define DEF_CYCLE "True"

#define PI M_PI
#define TWOPI 2*M_PI

#define MAXFAST 100

#define   norm       20.0

#ifdef FASTCOPY
#define sum1ton(a) (((a)*(a)+1)/2)
#define fastcw sum1ton(fastch)
#endif

#undef ABS
#define ABS(x) ((x)<0.0 ? -(x) : (x))

/* Anzahl der Kugeln */
#define kmax ((t3dp->minutes?60:24))

/* Werte in der Sinus-Tabelle */
#define sines 52

#define setink(inkcolor) XSetForeground(display,t3dp->gc,inkcolor)

static Bool trackmouse , minutes , cycle_p;

static float move, wobble , mag;

static int fastch;

static XrmOptionDescRec opts[] =
{
	{(char *) "-cycle", (char *) ".t3d.cycle", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+cycle", (char *) ".t3d.cycle", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-move", (char *) ".t3d.move", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-wobble", (char *) ".t3d.wobble", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-mag", (char *) ".t3d.mag", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-fast", (char *) ".t3d.fast", XrmoptionSepArg, (caddr_t) NULL},
        {(char *) "-minutes", (char *) ".t3d.minutes", XrmoptionNoArg, (caddr_t) "on"},
        {(char *) "+minutes", (char *) ".t3d.minutes", XrmoptionNoArg, (caddr_t) "off"},
        {(char *) "-trackmouse", (char *) ".t3d.trackmouse", XrmoptionNoArg, (caddr_t) "on"},
        {(char *) "+trackmouse", (char *) ".t3d.trackmouse", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool},
	{(void *) & move, (char *) "move", (char *) "Move", (char *) DEF_MOVE, t_Float},
	{(void *) & wobble, (char *) "wobble", (char *) "Wobble", (char *) DEF_WOBBLE, t_Float},
	{(void *) & mag, (char *) "mag", (char *) "Magnification", (char *) DEF_MAG, t_Float},
	{(void *) & fastch, (char *) "fast", (char *) "Fast", (char *) DEF_FAST, t_Int},
        {(void *) & minutes, (char *) "minutes", (char *) "Minutes", (char *) DEF_MINUTES, t_Bool},
        {(void *) & trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool}
};

static OptionStruct desc[] =
{
	{(char *) "-/+cycle", (char *) "turn on/off colour cycling"},
	{(char *) "-move num", (char *) "Move speed"},
	{(char *) "-wobble num", (char *) "Wobble speed"},
	{(char *) "-mag num", (char *) "Magnification factor"},
	{(char *) "-fast num", (char *) "Fast"},
        {(char *) "-/+minutes", (char *) "turn on/off minutes"},
        {(char *) "-/+trackmouse", (char *) "turn on/off the tracking of the mouse"}
};

ModeSpecOpt t3d_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


#ifdef USE_MODULES
ModStruct   t3d_description =
{"t3d", "init_t3d", "draw_t3d", "release_t3d",
 "refresh_t3d", "init_t3d", (char *) NULL, &t3d_opts,
 250000, 1000, 60000 , 1, 64, 1.0, "",
 "Flying Balls Clock Demo", 0, NULL};

#endif

typedef struct {
  double x,y,z,r,d,r1;
  int x1,y1;
} kugeldat;

typedef struct {
   Cursor      cursor;
   double cycles , movef , wobber , a[3] , x[3] , y[3] , zoom , speed , vspeed;
   double sinus[sines] , cosinus[sines] , v[3] , am[3] , zaehler , vnorm;
   double vturn;
   int fastch , minutes , maxk;
   GC gc;
   Pixmap buffer;
#ifdef FASTCOPY
   GC orgc , andgc;
   Pixmap fastcircles , fastmask;
#else
   XImage* fastcircles[maxfast] , fastmask[maxfast];
#endif
   Colormap    cmap;
   XColor     *colors;
   int         width, height, depth;
   int         ncolors , fastdraw , scrnW2, scrnH2, startx , starty , px , py;
   Bool        cycle_p, mono_p, no_colors;
   unsigned long blackpixel, whitepixel, fg, bg;
   int         direction;
   struct tm  *zeit;
   kugeldat kugeln[100];
   int         color_offset;
   ModeInfo   *mi;
} t3dstruct;

static t3dstruct *t3ds = (t3dstruct *) NULL;

static void
t3d_zeiger( ModeInfo* mi , double dist,double rad, double z, double sec,
		int *q)
/* Zeiger zeichnen */
{
  int i,n;
  double gratio=sqrt(2.0/(1.0+sqrt(5.0)));
   t3dstruct *t3dp;

   t3dp = &t3ds[MI_SCREEN(mi)];

  n = *q;

  for(i=0;i<3;i++)
    {
      t3dp->kugeln[n].x=dist*cos(sec);
      t3dp->kugeln[n].y=-dist*sin(sec);
      t3dp->kugeln[n].z=z;
      t3dp->kugeln[n].r=rad;
      n++;

      dist += rad;
      rad = rad*gratio;
    }
  *q = n;
}

static void
t3d_manipulate( ModeInfo* mi , double k)
/*-----------------------------------------------------------------*
 *                           Uhr zeichnen                          *
 *-----------------------------------------------------------------*/
{
  double i,l,/*xs,*/ys,zs,mod;
  double /*persec,*/sec,min,hour;
  int n;
  t3dstruct *t3dp;

  t3dp = &t3ds[MI_SCREEN(mi)];

  sec=TWOPI*modf(k/60,&mod);
  min=TWOPI*modf(k/3600,&mod);
  hour=TWOPI*modf(k/43200,&mod);

  l=TWOPI*modf(k/300,&mod);
  i=0.0;
  for (n=0;n<kmax;n++)
    {

      t3dp->kugeln[n].x=4.0*sin(i);
      t3dp->kugeln[n].y=4.0*cos(i);
      t3dp->kugeln[n].z=t3dp->wobber* /* (sin(floor(2+2*l/(PI))*i)*sin(2*l)); */
	cos((i-sec)*floor(2+5*l/(PI)))*sin(5*l);
      if(t3dp->minutes)
	{
	  t3dp->kugeln[n].r=/* (1.0+0.3*cos(floor(2+2*l/(PI))*i)*sin(2*l))* */
	    ((n % 5!=0) ? 0.3 : 0.6)*
	      ((n % 15 ==0) ? 1.25 : .75);
	}
      else
	{
	  t3dp->kugeln[n].r=/* (1.0+0.3*cos(floor(2+2*l/(PI))*i)*sin(2*l))* */
	    ((n & 1) ? 0.5 : 1.0)*
	      ((n % 6==0) ? 1.25 : .75);
	}
      i+=TWOPI/kmax;
    }

  t3dp->kugeln[n].x=0.0;
  t3dp->kugeln[n].y=0.0;
  t3dp->kugeln[n].z=0.0;
  t3dp->kugeln[n].r=2.0+cos(TWOPI*modf(k,&mod))/2;
  n++;

  t3d_zeiger( mi , 2.0,0.75,-2.0,sec,&n);
  t3d_zeiger( mi , 1.0,1.0,-1.5,min,&n);
  t3d_zeiger( mi , 0.0,1.5,-1.0,hour,&n);

  for(n=0;n<t3dp->maxk;n++)
    {
       double cycle = (double) MI_CYCLES( mi );

      ys=t3dp->kugeln[n].y*cos(t3dp->movef*sin(cycle*sec))+
	t3dp->kugeln[n].z*sin(t3dp->movef*sin(cycle*sec));
      zs=-t3dp->kugeln[n].y*sin(t3dp->movef*sin(cycle*sec))+
	t3dp->kugeln[n].z*cos(t3dp->movef*sin(cycle*sec));
      t3dp->kugeln[n].y=ys;
      t3dp->kugeln[n].z=zs;
    }
}

static double
t3d_gettime (void)
{
  struct timeval time1;
  struct tm *zeit;
  time_t lt;

#if HAVE_GETTIMEOFDAY
  GETTIMEOFDAY(&time1);
  lt = time1.tv_sec;	/* avoid type cast lossage */
#else
  lt = NULL;
#endif
  zeit = localtime((const time_t *) &lt);

  return (zeit->tm_sec+60*(zeit->tm_min+60*(zeit->tm_hour))
	  + time1.tv_usec*1.0E-6);
}


static void
t3d__sort( ModeInfo* mi , int l, int r)
{
  int i,j;
  kugeldat ex;
  double x;
   t3dstruct *t3dp;

   t3dp = &t3ds[MI_SCREEN(mi)];

  i=l;j=r;
  x=t3dp->kugeln[(l+r)/2].d;
  for (;;) {
      while(t3dp->kugeln[i].d>x) i++;
      while(x>t3dp->kugeln[j].d) j--;
      if (i<=j)
	{
	  ex=t3dp->kugeln[i];
	   t3dp->kugeln[i]=t3dp->kugeln[j];
	   t3dp->kugeln[j]=ex;
	  i++;j--;
	}
      if (i>j) break;
    }
  if (l<j) t3d__sort( mi , l,j);
  if (i<r) t3d__sort ( mi , i,r);
}

static void
t3d_vektorprodukt(double feld1[], double feld2[], double feld3[])
{
  feld3[0]=feld1[1]*feld2[2]-feld1[2]*feld2[1];
  feld3[1]=feld1[2]*feld2[0]-feld1[0]*feld2[2];
  feld3[2]=feld1[0]*feld2[1]-feld1[1]*feld2[0];
}

static void
t3d_turn(double feld1[], double feld2[], double winkel)
{
  double temp[3];
  double s,ca,sa,sx1,sx2,sx3;

  t3d_vektorprodukt(feld1,feld2,temp);

  s=feld1[0]*feld2[0]+feld1[1]*feld2[1]+feld1[2]*feld2[2];

  sx1=s*feld2[0];
  sx2=s*feld2[1];
  sx3=s*feld2[2];
  sa=sin(winkel);ca=cos(winkel);
  feld1[0]=ca*(feld1[0]-sx1)+sa*temp[0]+sx1;
  feld1[1]=ca*(feld1[1]-sx2)+sa*temp[1]+sx2;
  feld1[2]=ca*(feld1[2]-sx3)+sa*temp[2]+sx3;
}

static void
t3d_projektion( ModeInfo* mi)
{
  double c1[3],c2[3],k[3],x_1,y_1;
  double cno,cnorm/*,magnit*/;
  int i;
   t3dstruct *t3dp;

   t3dp = &t3ds[MI_SCREEN(mi)];

  for (i=0;i<t3dp->maxk;i++)
    {
      c1[0]=t3dp->kugeln[i].x-t3dp->a[0];
      c1[1]=t3dp->kugeln[i].y-t3dp->a[1];
      c1[2]=t3dp->kugeln[i].z-t3dp->a[2];
      cnorm=sqrt(c1[0]*c1[0]+c1[1]*c1[1]+c1[2]*c1[2]);

      c2[0]=c1[0];
      c2[1]=c1[1];
      c2[2]=c1[2];

      cno=c2[0]*t3dp->v[0]+c2[1]*t3dp->v[1]+c2[2]*t3dp->v[2];
      t3dp->kugeln[i].d=cnorm;
      if (cno<0) t3dp->kugeln[i].d=-20.0;


      t3dp->kugeln[i].r1=(mag*t3dp->zoom*t3dp->kugeln[i].r/cnorm);

      c2[0]=t3dp->v[0]/cno;
      c2[1]=t3dp->v[1]/cno;
      c2[2]=t3dp->v[2]/cno;

      t3d_vektorprodukt(c2,c1,k);


      x_1=(t3dp->startx+(t3dp->x[0]*k[0]+t3dp->x[1]*k[1]+t3dp->x[2]*k[2])*mag);
      y_1=(t3dp->starty-(t3dp->y[0]*k[0]+t3dp->y[1]*k[1]+t3dp->y[2]*k[2])*mag);
      if(   (x_1>-2000.0)
	 && (x_1<t3dp->width+2000.0)
	 && (y_1>-2000.0)
	 && (y_1<t3dp->height+2000.0))
	{
	  t3dp->kugeln[i].x1=(int)x_1;
	  t3dp->kugeln[i].y1=(int)y_1;
	}
      else
	{
	  t3dp->kugeln[i].x1=0;
	  t3dp->kugeln[i].y1=0;
	  t3dp->kugeln[i].d=-20.0;
	}
    }
}


static void
t3d_viewpoint( ModeInfo* mi)
/* 1: Blickrichtung v;3:Ebenenmittelpunkt m
   double feld1[],feld3[]; */
{
   t3dstruct *t3dp;

   t3dp = &t3ds[MI_SCREEN(mi)];

   t3dp->am[0]=-t3dp->zoom*t3dp->v[0];
   t3dp->am[1]=-t3dp->zoom*t3dp->v[1];
   t3dp->am[2]=-t3dp->zoom*t3dp->v[2];

   t3dp->zaehler=norm*norm* t3dp->zoom;
}

static void
t3d_fill_kugel(int i, Pixmap buf, int setcol , ModeInfo* mi )
{
   t3dstruct *t3dp;
   Display    *display = MI_DISPLAY(mi);
  double ra;
  int m,inr=3,d;
#ifdef USE_POLYGON
   int inc = 1;
#endif

   t3dp = &t3ds[MI_SCREEN(mi)];

  d=(int)((ABS(t3dp->kugeln[i].r1)*2));
  if (d==0) d=1;

# ifdef FASTDRAW
  if(t3dp->fastdraw && d<t3dp->fastch)
    {
#	ifdef FASTCOPY
      XCopyArea(display, t3dp->fastmask, buf, t3dp->andgc, sum1ton(d)-(d+1)/2,
		1,d,d, (int)(t3dp->kugeln[i].x1)-d/2,
		(int)(t3dp->kugeln[i].y1)-d/2);
      XCopyArea(display, t3dp->fastcircles, buf, t3dp->orgc,
		sum1ton(d)-(d+1)/2, 1,d,d, (int)(t3dp->kugeln[i].x1)-d/2,
		(int)(t3dp->kugeln[i].y1)-d/2);
#else
      XPutImage(display, buf, t3dp->andgc, t3dp->fastmask[d-1], 0, 0,
		(int)(t3dp->kugeln[i].x1)-d/2, (int)(t3dp->kugeln[i].y1)-d/2,
		d, d);
      XPutImage(display, buf, t3dp->orgc, t3dp->fastcircles[d-1], 0, 0,
		(int)(t3dp->kugeln[i].x1)-d/2, (int)(t3dp->kugeln[i].y1)-d/2,
		d, d);
#	endif
    }
  else
#endif
    {
       if(ABS(t3dp->kugeln[i].r1)<6.0) inr=9;

      for (m=0;m<=28;m+=inr)
	{
	  ra=t3dp->kugeln[i].r1*sqrt(1-m*m/(28.0*28.0));
#ifdef USE_POLYGON
	  if(-ra< 3.0) inc=14;
	  else if(-ra< 6.0) inc=8;
	  else if(-ra<20.0) inc=4;
	  else if(-ra<40.0) inc=2;
#endif
	  if(setcol)
	    {
	       if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
	         setink(t3dp->colors[m].pixel);
	       } else if (MI_NPIXELS(mi) <= 2) {
	         if ((m + t3dp->color_offset) % 2)
			setink(MI_WHITE_PIXEL(mi));
		 else
			setink(MI_BLACK_PIXEL(mi));
	       } else {
	         setink(MI_PIXEL(mi, ((m + t3dp->color_offset) % 29) * MI_NPIXELS(mi) / 29));
	       }
	    }

#ifdef USE_POLYGON
          {
            int n, nr;
	  for (n=0,nr=0;n<=sines-1;n+=inc,nr++)
	    {
	      track[nr].x=t3dp->kugeln[i].x1+(int)(ra*t3dp->sinus[n])+
		 (t3dp->kugeln[i].r1-ra)/2;
	      track[nr].y=t3dp->kugeln[i].y1+(int)(ra*t3dp->cosinus[n])+
		 (t3dp->kugeln[i].r1-ra)/2;
	    }
	  XFillPolygon(display,buf,t3dp->gc,track,nr,Convex,CoordModeOrigin);
          }
#else /* Use XFillArc */
	  XFillArc(display, buf, t3dp->gc,
		   (int)(t3dp->kugeln[i].x1+(t3dp->kugeln[i].r1+ra)/2),
		   (int)(t3dp->kugeln[i].y1+(t3dp->kugeln[i].r1+ra)/2),
		   (int)-(2*ra+1), (int)-(2*ra+1), 0, 360*64);
#endif
	}
    }
}


static void
t3d_init_kugel( ModeInfo* mi )
{
   t3dstruct *t3dp;

#ifdef FASTDRAW
   Display    *display = MI_DISPLAY(mi);
  int i;

   t3dp = &t3ds[MI_SCREEN(mi)];

  for(i=0; i<t3dp->fastch; i++)
    {
#	ifdef FASTCOPY
      t3dp->kugeln[i].r1=-((double) i)/2 -1;
      t3dp->kugeln[i].x1=sum1ton(i);
      t3dp->kugeln[i].y1=i/2 +1;

      t3d_fill_kugel(i,t3dp->fastcircles,1 , mi );
/*      setink((1<<MIN(24,t3dp->depth))-1);*/
      setink(MI_BLACK_PIXEL(mi));
      t3d_fill_kugel(i,t3dp->fastmask,0 , mi );
#	else
      t3dp->kugeln[i].r1=-((double) i)/2 -1;
      t3dp->kugeln[i].x1=t3dp->kugeln[i].y1=i/2 +1;

      t3d_fill_kugel(i,t3dp->buffer,1 , mi );
      t3dp->fastcircles[i]=XGetImage(display,t3dp->buffer,0,0,i+2,i+2,
				     (1<<t3dp->depth)-1, ZPixmap);
       /* setink((1<<MIN(24,t3dp->depth))-1); */
      setink(MI_WHITE_PIXEL(mi));
      t3d_fill_kugel(i,t3dp->buffer,0 , mi );
      t3dp->fastmask[i]=XGetImage(display,t3dp->buffer,0,0,i+2,i+2,
				  (1<<t3dp->depth)-1,ZPixmap);
      setink(MI_BLACK_PIXEL(mi));
      XFillRectangle (display, t3dp->buffer     , t3dp->gc, 0, 0,
		      t3dp->width, t3dp->height);
#	endif
    }
  t3dp->fastdraw=1;
#endif
}

static void
t3d_init_3d( ModeInfo* mi )
{
   t3dstruct *t3dp;
   double i;
   int n=0;

   t3dp = &t3ds[MI_SCREEN(mi)];

   t3dp->a[0]=0.0;
   t3dp->a[1]=0.0;
   t3dp->a[2]=-10.0;

   t3dp->x[0]=10.0;
   t3dp->x[1]=0.0;
   t3dp->x[2]=0.0;

   t3dp->y[0]=0.0;
   t3dp->y[1]=10.0;
   t3dp->y[2]=0.0;

   t3dp->zoom=-10.0;
   t3dp->speed=.0;

   for (i=0.0;n<sines;i+=TWOPI/sines,n++)
    {
      t3dp->sinus[n]=sin(i);
      t3dp->cosinus[n]=cos(i);
    }

}


static void
free_t3d(Display *display, t3dstruct *t3dp)
{
	ModeInfo *mi = t3dp->mi;

	if (t3dp->cursor != None) {
		XFreeCursor(display, t3dp->cursor);
		t3dp->cursor = None;
	}
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		MI_WHITE_PIXEL(mi) = t3dp->whitepixel;
		MI_BLACK_PIXEL(mi) = t3dp->blackpixel;
#ifndef STANDALONE
		MI_FG_PIXEL(mi) = t3dp->fg;
		MI_BG_PIXEL(mi) = t3dp->bg;
#endif
		if (t3dp->colors != NULL) {
			if (t3dp->ncolors && !t3dp->no_colors)
				free_colors(display, t3dp->cmap, t3dp->colors, t3dp->ncolors);
			free(t3dp->colors);
			t3dp->colors = (XColor *) NULL;
		}
		if (t3dp->cmap != None) {
			XFreeColormap(display, t3dp->cmap);
			t3dp->cmap = None;
		}
	}
	if (t3dp->buffer != None) {
		XFreePixmap(display, t3dp->buffer);
		t3dp->buffer = None;
	}
	if (t3dp->gc != None) {
		XFreeGC(display, t3dp->gc);
		t3dp->gc = None;
	}
#ifdef FASTCOPY
	if (t3dp->orgc != None) {
		XFreeGC(display, t3dp->orgc);
		t3dp->orgc = None;
	}
	if (t3dp->andgc != None) {
		XFreeGC(display, t3dp->andgc);
		t3dp->andgc = None;
	}
	if (t3dp->fastcircles != None) {
		XFreePixmap(display, t3dp->fastcircles);
		t3dp->fastcircles = None;
	}
	if (t3dp->fastmask != None) {
		XFreePixmap(display, t3dp->fastmask);
		t3dp->fastmask = None;
	}
#endif
	if (t3dp->zeit != NULL) {
		free(t3dp->zeit);
		t3dp->zeit = (struct tm *) NULL;
	}
}

static Bool
t3d_initialize( ModeInfo* mi )
{
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);
   t3dstruct *t3dp;
   XGCValues   xgcv;

   t3dp = &t3ds[MI_SCREEN(mi)];

   t3dp->cycles = MI_CYCLES( mi ) / 6000.0;
   if ( t3dp->cycles <= 1.0 )
     t3dp->cycles = 10.0;

   t3dp->movef = (double) move;
   t3dp->wobber = (double) wobble;
   t3dp->fastch = (int) (fastch * mag);
   if ( t3dp->fastch > MAXFAST )
     t3dp->fastch = MAXFAST;

   if ( minutes )
     {
	t3dp->minutes = 1;
	t3dp->maxk = 70;
     }
   else
     {
	t3dp->minutes = 0;
	t3dp->maxk = 34;
     }
   xgcv.foreground = MI_WHITE_PIXEL(mi);
   if ((t3dp->gc = XCreateGC (display, window, GCForeground, &xgcv)) == None) {
	free_t3d(display, t3dp);
	return False;
   }
#ifdef FASTDRAW
   xgcv.function = GXor;
   if ((t3dp->orgc = XCreateGC (display, window, GCFunction | GCForeground,
		&xgcv)) == None) {
	free_t3d(display, t3dp);
	return False;
   }
   xgcv.function = GXandInverted;
   if ((t3dp->andgc = XCreateGC (display, window, GCFunction | GCForeground,
		&xgcv)) == None) {
	free_t3d(display, t3dp);
	return False;
   }
#endif
   if (MI_IS_VERBOSE(mi))
     {
	(void) printf("Time 3D drawing ");
#ifdef FASTDRAW
#	ifdef FASTCOPY
	(void) puts("fast by Pixmap copy");
#	else
	(void) puts("fast by XImage copy");
#	endif
#else
	(void) puts("slow");
#endif
     }

#ifdef FASTCOPY
   if (((t3dp->fastcircles = XCreatePixmap (display, window,
		fastcw, t3dp->fastch+1, t3dp->depth)) == None) ||
       ((t3dp->fastmask    = XCreatePixmap (display, window,
		fastcw, t3dp->fastch+1, t3dp->depth)) == None)) {
	free_t3d(display, t3dp);
	return False;
   }
#endif

  setink(MI_BLACK_PIXEL(mi));
  XFillRectangle (display, t3dp->buffer , t3dp->gc, 0, 0, t3dp->width,
		  t3dp->height);

#ifdef FASTCOPY
  setink(MI_BLACK_PIXEL(mi));
  XFillRectangle (display, t3dp->fastcircles, t3dp->gc, 0, 0, fastcw,
		  t3dp->fastch+1);
  XFillRectangle (display, t3dp->fastmask   , t3dp->gc, 0, 0, fastcw,
		  t3dp->fastch+1);
#endif

   if (MI_IS_VERBOSE(mi))
   {
	(void) printf("move\t%.2f\nwobber\t%.2f\nmag\t%.2f\n",
	       t3dp->movef, t3dp->wobber, mag );
	(void) printf("fast\t%i\nmarks\t%i\n", t3dp->fastch, t3dp->maxk);
   }
   return True;
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

void
init_t3d(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	t3dstruct *t3dp;

	if (t3ds == NULL) {
		if ((t3ds = (t3dstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (t3dstruct))) == NULL)
			return;
	}
	t3dp = &t3ds[MI_SCREEN(mi)];
	t3dp->mi = mi;

	if (trackmouse && !t3dp->cursor) {	/* Create an invisible cursor */
		Pixmap      bit;
		XColor      black;

		black.red = 0;
		black.green = 0;
		black.blue = 0;
		black.flags = DoRed | DoGreen | DoBlue;
		if ((bit = XCreatePixmapFromBitmapData(display, window,
				(char *) "\000", 1, 1, MI_BLACK_PIXEL(mi),
				MI_BLACK_PIXEL(mi), 1)) == None) {
			free_t3d(display, t3dp);
			return;
		}
		if ((t3dp->cursor = XCreatePixmapCursor(display, bit, bit,
				&black, &black, 0, 0)) == None) {
			XFreePixmap(display, bit);
			free_t3d(display, t3dp);
			return;
		}
		XFreePixmap(display, bit);
	}
	XDefineCursor(display, window, t3dp->cursor);

	t3dp->width = MI_WIDTH(mi);
	t3dp->height = MI_HEIGHT(mi);
	t3dp->depth = MI_DEPTH(mi);
	if (t3dp->buffer != None) {
		XFreePixmap(display, t3dp->buffer);
		t3dp->buffer = None;
	}
	if ((t3dp->buffer = XCreatePixmap (display, window,
			t3dp->width, t3dp->height, t3dp->depth)) == None) {
		free_t3d(display, t3dp);
		return;
  	}

   /* Initialization */
   if (t3dp->gc == None) {
	if (!t3d_initialize(mi))
		return;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2)
	  {
	     XColor      color;

#ifndef STANDALONE
	     t3dp->fg = MI_FG_PIXEL(mi);
	     t3dp->bg = MI_BG_PIXEL(mi);
#endif
	     t3dp->blackpixel = MI_BLACK_PIXEL(mi);
	     t3dp->whitepixel = MI_WHITE_PIXEL(mi);
	     if ((t3dp->cmap = XCreateColormap(display, window, MI_VISUAL(mi),
			AllocNone)) == None) {
		free_t3d(display, t3dp);
		return;
	     }
	     XSetWindowColormap(display, window, t3dp->cmap);
	     (void) XParseColor(display, t3dp->cmap, "black", &color);
	     (void) XAllocColor(display, t3dp->cmap, &color);
	     MI_BLACK_PIXEL(mi) = color.pixel;
	     (void) XParseColor(display, t3dp->cmap, "white", &color);
	     (void) XAllocColor(display, t3dp->cmap, &color);
	     MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
	     (void) XParseColor(display, t3dp->cmap, background, &color);
	     (void) XAllocColor(display, t3dp->cmap, &color);
	     MI_BG_PIXEL(mi) = color.pixel;
	     (void) XParseColor(display, t3dp->cmap, foreground, &color);
	     (void) XAllocColor(display, t3dp->cmap, &color);
	     MI_FG_PIXEL(mi) = color.pixel;
#endif
	     t3dp->colors = (XColor *) NULL;
	     t3dp->ncolors = 0;
     	  }
     }
     t3dp->color_offset = NRAND(29);

/*Set up t2d data */
   t3dp->direction = (LRAND() & 1) ? 1 : -1;
   t3dp->fastdraw = 0;
   t3dp->vturn = 0.0;

   if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
/* Set up colour map */
		if (t3dp->colors != NULL) {
			if (t3dp->ncolors && !t3dp->no_colors)
				free_colors(display, t3dp->cmap, t3dp->colors, t3dp->ncolors);
			free(t3dp->colors);
			t3dp->colors = (XColor *) NULL;
		}
		t3dp->ncolors = MI_NCOLORS(mi);
		if (t3dp->ncolors < 2)
			t3dp->ncolors = 2;
		if (t3dp->ncolors <= 2)
			t3dp->mono_p = True;
		else
			t3dp->mono_p = False;

		if (t3dp->mono_p)
			t3dp->colors = (XColor *) NULL;
		else
			if ((t3dp->colors = (XColor *) malloc(sizeof (*t3dp->colors) *
					(t3dp->ncolors + 1))) == NULL) {
				free_t3d(display, t3dp);
				return;
			}
		t3dp->cycle_p = has_writable_cells(mi);
		if (t3dp->cycle_p) {
			if (MI_IS_FULLRANDOM(mi)) {
				if (!NRAND(8))
					t3dp->cycle_p = False;
				else
					t3dp->cycle_p = True;
			} else {
				t3dp->cycle_p = cycle_p;
			}
		}
		if (!t3dp->mono_p) {
			if (!(LRAND() % 10))
				make_random_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
						t3dp->cmap, t3dp->colors, &t3dp->ncolors,
						True, True, &t3dp->cycle_p);
			else if (!(LRAND() % 2))
				make_uniform_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                  t3dp->cmap, t3dp->colors, &t3dp->ncolors,
						      True, &t3dp->cycle_p);
			else
				make_smooth_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                 t3dp->cmap, t3dp->colors, &t3dp->ncolors,
						     True, &t3dp->cycle_p);
		}
		XInstallColormap(display, t3dp->cmap);
		if (t3dp->ncolors < 2) {
			t3dp->ncolors = 2;
			t3dp->no_colors = True;
		} else
			t3dp->no_colors = False;
		if (t3dp->ncolors <= 2)
			t3dp->mono_p = True;

		if (t3dp->mono_p)
			t3dp->cycle_p = False;

	}

   t3d_init_3d( mi );

   if (t3dp->zeit == NULL)
     if ((t3dp->zeit = (struct tm *) malloc(sizeof(struct tm))) == NULL) {
	free_t3d(display, t3dp);
	return;
     }

   t3d_init_kugel( mi );

   t3dp->startx = t3dp->width / 2;
   t3dp->starty = t3dp->height / 2;
   t3dp->scrnH2 = t3dp->startx;
   t3dp->scrnW2 = t3dp->starty;
   t3dp->vspeed=0;

   t3d_vektorprodukt( t3dp->x, t3dp->y, t3dp->v);
   t3d_viewpoint( mi );

   MI_CLEARWINDOW(mi);
}

void
release_t3d(ModeInfo * mi)
{
	if (t3ds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_t3d(MI_DISPLAY(mi), &t3ds[screen]);
		free(t3ds);
		t3ds = (t3dstruct *) NULL;
	}
}

void
draw_t3d(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	double dtime;
	int button;
	t3dstruct *t3dp;

	if (t3ds == NULL)
		return;
	t3dp = &t3ds[MI_SCREEN(mi)];
 	if (t3dp->zeit == NULL)
		return;

	if (t3dp->no_colors) {
		free_t3d(display, t3dp);
		init_t3d(mi);
		return;
	}

   MI_IS_DRAWN(mi) = True;

/* Rotate colours */
   if (t3dp->cycle_p)
     {
	rotate_colors(display, t3dp->cmap, t3dp->colors, t3dp->ncolors,
		      t3dp->direction);
	if (!(LRAND() % 1000))
	  t3dp->direction = -t3dp->direction;
     }

   t3d_vektorprodukt( t3dp->x, t3dp->y , t3dp->v);

   	t3dp->vnorm=sqrt(t3dp->v[0]*t3dp->v[0]+t3dp->v[1]*t3dp->v[1]+
			 t3dp->v[2]*t3dp->v[2]);
	t3dp->v[0]=t3dp->v[0]*norm/t3dp->vnorm;
	t3dp->v[1]=t3dp->v[1]*norm/t3dp->vnorm;
	t3dp->v[2]=t3dp->v[2]*norm/t3dp->vnorm;
	t3dp->vnorm=sqrt(t3dp->x[0]*t3dp->x[0]+t3dp->x[1]*t3dp->x[1]+
			 t3dp->x[2]*t3dp->x[2]);
	t3dp->x[0]=t3dp->x[0]*norm/t3dp->vnorm;
	t3dp->x[1]=t3dp->x[1]*norm/t3dp->vnorm;
	t3dp->x[2]=t3dp->x[2]*norm/t3dp->vnorm;
	t3dp->vnorm=sqrt(t3dp->y[0]*t3dp->y[0]+t3dp->y[1]*t3dp->y[1]+
			 t3dp->y[2]*t3dp->y[2]);
	t3dp->y[0]=t3dp->y[0]*norm/t3dp->vnorm;
	t3dp->y[1]=t3dp->y[1]*norm/t3dp->vnorm;
	t3dp->y[2]=t3dp->y[2]*norm/t3dp->vnorm;

   t3d_projektion( mi );
   t3d__sort ( mi , 0,t3dp->maxk-1);

   dtime= t3d_gettime();

   setink(MI_BLACK_PIXEL(mi));
   XFillRectangle(display, t3dp->buffer,t3dp->gc,0,0,t3dp->width,
		  t3dp->height);

   	{
	  int i;

	  t3d_manipulate( mi , dtime);

	  for (i=0;i<t3dp->maxk;i++)
	    {
	      if (t3dp->kugeln[i].d>0.0)
		t3d_fill_kugel(i,t3dp->buffer,1 , mi );
	    }
	}

   XFlush(display);

   XCopyArea (display, t3dp->buffer, window, t3dp->gc, 0, 0, t3dp->width,
	      t3dp->height, 0, 0);

   XFlush(display);

   if ( trackmouse )
     {
	Window junk_win,in_win;
	int junk;
	unsigned int kb;

	(void)(XQueryPointer (display, window, &junk_win, &in_win, &junk,
			      &junk, &t3dp->px, &t3dp->py, &kb));
	/* The following may have to much interference with the Xlockmore
	 * event handling  (JJ) */
	if ( (kb&Button2Mask) )
	  {
	     button = 2;
	  }
	else
	  {
	     if (kb&Button1Mask)
	       {
		  button = 1;
	       }
	     else
	       {
		  if (kb&Button3Mask)
		    {
		       button = 3;
		    }
		  else
		    {
		       button = 0;
		    }
	       }
	  }
     }
   else
     {
	t3dp->px = NRAND( t3dp->width );
        t3dp->py = NRAND( t3dp->height );
	button = NRAND( 50 );
	if ( button > 3 ) button = 0;
     }

   if ((t3dp->px>0)&&(t3dp->px<t3dp->width)&&(t3dp->py>0)&&
       (t3dp->py<t3dp->height) )
	  {
	    if ((t3dp->px !=t3dp->startx)&&( button == 2 ))
	      {
		t3d_turn(t3dp->y,t3dp->x,((double)(t3dp->px-t3dp->startx))/
			 (8000*mag));
	      }
	    if ((t3dp->py !=t3dp->starty)&&( button == 2 ))
	      {
		t3d_turn(t3dp->x,t3dp->y,((double)(t3dp->py-t3dp->starty))/
			 (-8000*mag));
	      }
	    if ( button == 1 )
	      {
		if (t3dp->vturn==0.0) t3dp->vturn=.005;
		 else if (t3dp->vturn<2)  t3dp->vturn+=.01;
		t3d_turn(t3dp->x,t3dp->v,.002*t3dp->vturn);
		t3d_turn(t3dp->y,t3dp->v,.002*t3dp->vturn);
	      }
	    if ( button == 3 )
	      {
		if (t3dp->vturn==0.0) t3dp->vturn=.005;
		 else if (t3dp->vturn<2) t3dp->vturn+=.01;
		t3d_turn(t3dp->x,t3dp->v,-.002*t3dp->vturn);
		t3d_turn(t3dp->y,t3dp->v,-.002*t3dp->vturn);
	      }
	  }
	if (!( button == 1 )&&!( button == 3 ))
	  t3dp->vturn=0;

	t3dp->speed=t3dp->speed+t3dp->speed*t3dp->vspeed;
	if ((t3dp->speed<0.0000001) &&(t3dp->vspeed>0.000001)) t3dp->speed=0.000001;
	t3dp->vspeed=.1*t3dp->vspeed;
	if (t3dp->speed>0.01) t3dp->speed=.01;
	t3dp->a[0]=t3dp->a[0]+t3dp->speed*t3dp->v[0];
	t3dp->a[1]=t3dp->a[1]+t3dp->speed*t3dp->v[1];
	t3dp->a[2]=t3dp->a[2]+t3dp->speed*t3dp->v[2];
}

void
refresh_t3d(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_t3d */
