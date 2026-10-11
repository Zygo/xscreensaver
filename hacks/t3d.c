/* t3d -- Flying Balls Clock Demo
   by Bernd Paysan , paysan@informatik.tu-muenchen.de

   Copy, modify, and distribute T3D either under GPL  version 2 or newer, 
   or under the standard MIT/X license notice.

  partly based on flying balls demo by Georg Acher, 
  acher@informatik.tu-muenchen.de
  (developed on HP9000/720 (55 MIPS,20 MFLOPS) )
  NO warranty at all ! Complaints to /dev/null !

  4-Jan-99 jwz@jwz.org -- adapted to xscreensaver framework, to take advantage
                          of the command-line options provided by screenhack.c.
*/

#ifndef HAVE_JWXYZ
# define FASTDRAW
# define FASTCOPY
#endif /* !HAVE_JWXYZ */

#include <stdio.h>
#include <math.h>
#include <time.h> /* for localtime() and gettimeofday() */

#include "screenhack.h"


#define   WIDTH      200
#define   HEIGHT     200
#define   norm       20.0

#define   ROOT       0x1
#define PI M_PI
#define TWOPI 2*M_PI

#define MIN(i,j) ((i)<(j)?(i):(j))

#define kmax ((st->minutes?60:24))
/* Anzahl der Kugeln */
#define sines 52
/* Werte in der Sinus-Tabelle */
/*-----------------------------------------------------------------*/
#define setink(inkcolor) \
	XSetForeground (st->dpy,st->gc,inkcolor)

#define drawline(xx1,yy1,xx2,yy2) \
	XDrawLine(st->dpy,st->win,st->gc,xx1,yy1,xx2,yy2) 

#define drawseg(segments,nr_segments) \
	XDrawSegments(st->dpy,st->win,st->gc,segments,nr_segments) 


#define polyfill(ppts,pcount) \
	XFillPolygon(st->dpy,st->win,st->gc,ppts,pcount,Convex,CoordModeOrigin)


#define frac(argument) argument-floor(argument)

#undef ABS
#define ABS(x) ((x)<0.0 ? -(x) : (x))

typedef struct {
  double x,y,z,r,d,r1;
  int x1,y1;
} kugeldat;

/* Felder fuer 3D */


struct state {
  Display *dpy;
  Window window;

  int maxk;
  int timewait;

  Colormap cmap;
  double r,g,b;
  double hue,sat,val;

  kugeldat kugeln[100];

  double  a[3],am[3],x[3],y[3],v[3];
  double zoom,speed,zaehler,vspeed;
  double vturn;
  double sinus[sines];
  double cosinus[sines];

  int startx,starty;
  double mag;
  int minutes;
  int cycl;
  double hsvcycl;
  double movef, wobber, cycle;

  XWindowAttributes xgwa;
  GC	gc;
  GC orgc;
  GC andgc;
  int	scrnWidth, scrnHeight;
  Pixmap  buffer;
  int fastch;

  int scrnW2,scrnH2;
  XColor colors[64];
  struct tm *zeit;

  int planes;

  Window junk_win,in_win;
  int px,py,junk;
  unsigned int kb;

  int fastdraw;
  int draw_color;

# ifdef FASTDRAW
#  ifdef FASTCOPY
#   define sum1ton(a) (((a)*(a)+1)/2)
#   define fastcw sum1ton(st->fastch)
    Pixmap fastcircles;
    Pixmap fastmask;
#  else /* !FASTCOPY */
    XImage* fastcircles[maxfast];
    XImage* fastmask[maxfast];
#  endif /* !FASTCOPY */
# endif /* FASTDRAW */
};



#define maxfast 100

/* compute time */

static double
gettime (struct state *st)
{
  struct timeval time1;
  struct tm *zeit;
  time_t lt;

#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone zone1;
  gettimeofday(&time1,&zone1);
#else
  gettimeofday(&time1);
#endif
  lt = time1.tv_sec;	/* avoid type cast lossage */
  zeit=localtime(&lt);
  
  return (zeit->tm_sec+60*(zeit->tm_min+60*(zeit->tm_hour))
	  + time1.tv_usec*1.0E-6);
}

/* --------------------COLORMAP---------------------*/ 

static void
hsv2rgb (double h, double s, double v, double *r, double *g, double *b)
{
  h/=360.0;	h=6*(h-floor(h));

  if(s==0.0)
    {
      *r=*g=*b=v;
    }
  else
    {	int i=(int)h;
	double t,u,w;
	
	h=h-floor(h);
	
	u=v*(s*(1.0-h));
	w=v*(1.0-s);
	t=v*(s*h+1.0-s);
	
	switch(i)
	  {
	  case 0:	*r=v;	*g=t;	*b=w;	break;
	  case 1:	*r=u;	*g=v;	*b=w;	break;
	  case 2:	*r=w;	*g=v;	*b=t;	break;
	  case 3:	*r=w;	*g=u;	*b=v;	break;
	  case 4:	*r=t;	*g=w;	*b=v;	break;
	  case 5:	*r=v;	*g=w;	*b=u;	break;
	  }
      }
#ifdef PRTDBX
  printf("HSV: %f %f %f to\nRGB: %f %f %f\n",h,s,v,*r,*g,*b);
#endif
}

static void
changeColor (struct state *st, double r, double g, double b)
{
  int n,n1;
  
  n1=0;
  for(n=30;n<64;n+=3)
    {
      st->colors[n1].red   =1023+ n*(int)(1024.*r);
      st->colors[n1].blue  =1023+ n*(int)(1024.*b);
      st->colors[n1].green =1023+ n*(int)(1024.*g);
      
      n1++;
    }
  
  XStoreColors (st->dpy, st->cmap, st->colors, 12);
}

static void 
initColor (struct state *st, double r, double g, double b)
{
  int n,n1;
  unsigned long pixels[12];
  unsigned long dummy;
  
  st->cmap = st->xgwa.colormap;
  
  if(st->hsvcycl!=0.0 && XAllocColorCells(st->dpy, st->cmap, 0, &dummy, 0, pixels, 12))
    {
      for(n1=0;n1<12;n1++)
	{
	  st->colors[n1].pixel=pixels[n1];
	  st->colors[n1].flags=DoRed | DoGreen | DoBlue;
	}
      
      changeColor(st,r,g,b);
    }
  else
    {
      n1=0;
      for(n=30;n<64;n+=3)
	{
	  st->colors[n1].red   =1023+ n*(int)(1024.*r);
	  st->colors[n1].blue  =1023+ n*(int)(1024.*b);
	  st->colors[n1].green =1023+ n*(int)(1024.*g);
	  
	  if (!(XAllocColor (st->dpy, st->cmap, &st->colors[n1]))) {
	    fprintf (stderr, "Error:  Cannot allocate colors\n");
	    exit (1);
	  }
	  
	  n1++;
	}
    }
}

/* ----------------WINDOW-------------------*/

static void
initialize (struct state *st)
{
  XGCValues *xgc;
  XGCValues *xorgc;
  XGCValues *xandgc;

  st->maxk=34;
  st->r = st->g = st->b = 1;
  st->hue = st->sat = 0;
  st->val = 1;
  st->mag = 10;
  st->movef = 0.5;
  st->wobber = 2;
  st->cycle = 6;
  st->scrnWidth = WIDTH;
  st->scrnHeight = HEIGHT;


  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->scrnWidth = st->xgwa.width;
  st->scrnHeight = st->xgwa.height;

  {
    float f = get_float_resource (st->dpy, "cycle", "Float");
    if (f <= 0 || f > 60) f = 6.0;
    st->cycle = 60.0 / f;
  }
  st->movef = get_float_resource (st->dpy, "move", "Float");
  st->wobber = get_float_resource (st->dpy, "wobble", "Float");

  {
    double magfac = get_float_resource (st->dpy, "mag", "Float");
    st->mag *= magfac;
    st->fastch=(int)(st->fastch*magfac);
  }

  if (get_boolean_resource (st->dpy, "minutes", "Boolean")) {
    st->minutes=1; st->maxk+=60-24;
  }

  st->timewait = get_integer_resource (st->dpy, "delay", "Integer");
  st->fastch = get_integer_resource (st->dpy, "fast", "Integer");
  st->cycl = get_boolean_resource (st->dpy, "colcycle", "Integer");
  st->hsvcycl = get_float_resource (st->dpy, "hsvcycle", "Integer");

  {
    char *s = get_string_resource (st->dpy, "rgb", "RGB");
    char dummy;
    if (s && *s)
      {
        double rr, gg, bb;
        if (3 == sscanf (s, "%lf %lf %lf %c", &rr, &gg, &bb, &dummy))
          st->r = rr, st->g = gg, st->b = bb;
      }
    if (s) free (s);

    s = get_string_resource (st->dpy, "hsv", "HSV");
    if (s && *s)
      {
        double hh, ss, vv;
        if (3 == sscanf (s, "%lf %lf %lf %c", &hh, &ss, &vv, &dummy)) {
          st->hue = hh, st->sat = ss, st->val = vv;
          hsv2rgb(st->hue,st->sat,st->val,&st->r,&st->g,&st->b);
        }
      }
    if (s) free (s);
  }

  if (st->fastch > maxfast)
    st->fastch=maxfast;
  
  xgc=( XGCValues *) malloc(sizeof(XGCValues) );
  xorgc=( XGCValues *) malloc(sizeof(XGCValues) );
  xandgc=( XGCValues *) malloc(sizeof(XGCValues) );

  st->planes=st->xgwa.depth;

#ifdef HAVE_JWXYZ
# define GXandInverted GXcopy  /* #### this can't be right, right? */
#endif
 st->gc = XCreateGC (st->dpy, st->window, 0,  xgc);
  xorgc->function =GXor;
  st->orgc = XCreateGC (st->dpy, st->window, GCFunction,  xorgc);
  xandgc->function =GXandInverted;
  st->andgc = XCreateGC (st->dpy, st->window, GCFunction,  xandgc);
  
  st->buffer = XCreatePixmap (st->dpy, st->window, st->scrnWidth, st->scrnHeight,
			  st->xgwa.depth); 
  
#ifdef DEBUG
  printf("Time 3D drawing ");
#ifdef FASTDRAW
#	ifdef FASTCOPY
  puts("fast by Pixmap copy");
#	else
  puts("fast by XImage copy");
#	endif
#else
  puts("slow");
#endif
#endif /* DEBUG */
  
#ifdef FASTCOPY
  st->fastcircles = XCreatePixmap (st->dpy, st->window, fastcw, st->fastch+1, st->xgwa.depth);
  st->fastmask    = XCreatePixmap (st->dpy, st->window, fastcw, st->fastch+1, st->xgwa.depth);
#endif
  
  setink(BlackPixelOfScreen (st->xgwa.screen));
  XFillRectangle (st->dpy, st->buffer     , st->gc, 0, 0, st->scrnWidth, st->scrnHeight);	
  
#ifdef FASTCOPY
  
  setink(0);
  XFillRectangle (st->dpy, st->fastcircles, st->gc, 0, 0, fastcw, st->fastch+1);
  XFillRectangle (st->dpy, st->fastmask   , st->gc, 0, 0, fastcw, st->fastch+1);
  
#endif

#ifdef PRTDBX
  printf("move\t%.2f\nwobber\t%.2f\nmag\t%.2f\ncycle\t%.4f\n",
	 st->movef,st->wobber,st->mag/10,st->cycle);
  printf("fast\t%i\nmarks\t%i\nwait\t%i\n",st->fastch,st->maxk,st->timewait);
#endif
 
  free (xgc);
  free (xorgc);
  free (xandgc);
}

static void fill_kugel(struct state *st, int i, Pixmap buf, int setcol);


/*------------------------------------------------------------------*/
static void 
init_kugel(struct state *st)
{
  
#ifdef FASTDRAW
  int i;

  for(i=0; i<st->fastch; i++)
    {
#	ifdef FASTCOPY
      st->kugeln[i].r1=-((double) i)/2 -1;
      st->kugeln[i].x1=sum1ton(i);
      st->kugeln[i].y1=((double) i)/2 +1;
      
      fill_kugel(st,i,st->fastcircles,1);
      setink((1<<MIN(24,st->xgwa.depth))-1);
      fill_kugel(st,i,st->fastmask,0);
#	else
      st->kugeln[i].r1=-((double) i)/2 -1;
      st->kugeln[i].x1=st->kugeln[i].y1=((double) i)/2 +1;
      
      fill_kugel(i,st->buffer,1);
      st->fastcircles[i]=XGetImage(st->dpy,st->buffer,0,0,i+2,i+2,(1<<st->planes)-1,ZPixmap);
      
      setink((1<<MIN(24,st->xgwa.depth))-1);
      fill_kugel(i,st->buffer,0);
      st->fastmask[i]=XGetImage(st->dpy,st->buffer,0,0,i+2,i+2,(1<<st->planes)-1,ZPixmap);
      
      setink(0);
      XFillRectangle (st->dpy, st->buffer     , st->gc, 0, 0, st->scrnWidth, st->scrnHeight);	
#	endif
    }
  st->fastdraw=1;
#endif
}

/* Zeiger zeichnen */

static void
zeiger(struct state *st, double dist,double rad, double z, double sec, int *q)
{
  int i,n;
  double gratio=sqrt(2.0/(1.0+sqrt(5.0)));
  
  n = *q;
  
  for(i=0;i<3;i++)
    {
      st->kugeln[n].x=dist*cos(sec);
      st->kugeln[n].y=-dist*sin(sec);
      st->kugeln[n].z=z;
      st->kugeln[n].r=rad;
      n++;

      dist += rad;
      rad = rad*gratio;
    }
  *q = n;
}

/*-----------------------------------------------------------------*
 *                           Uhr zeichnen                          *
 *-----------------------------------------------------------------*/

static void
manipulate(struct state *st, double k)
{
  double i,l,/*xs,*/ys,zs,mod;
  double /*persec,*/sec,min,hour;
  int n;
  
  sec=TWOPI*modf(k/60,&mod);
  min=TWOPI*modf(k/3600,&mod);
  hour=TWOPI*modf(k/43200,&mod);
  
  l=TWOPI*modf(k/300,&mod);
  i=0.0;
  for (n=0;n<kmax;n++)
    {
      
      st->kugeln[n].x=4.0*sin(i);
      st->kugeln[n].y=4.0*cos(i);
      st->kugeln[n].z=st->wobber* /* (sin(floor(2+2*l/(PI))*i)*sin(2*l)); */
	cos((i-sec)*floor(2+5*l/(PI)))*sin(5*l);
      if(st->minutes)
	{
	  st->kugeln[n].r=/* (1.0+0.3*cos(floor(2+2*l/(PI))*i)*sin(2*l))* */
	    ((n % 5!=0) ? 0.3 : 0.6)*
	      ((n % 15 ==0) ? 1.25 : .75);
	}
      else
	{
	  st->kugeln[n].r=/* (1.0+0.3*cos(floor(2+2*l/(PI))*i)*sin(2*l))* */
	    ((n & 1) ? 0.5 : 1.0)*
	      ((n % 6==0) ? 1.25 : .75);
	}
      i+=TWOPI/kmax;
    }

  st->kugeln[n].x=0.0;
  st->kugeln[n].y=0.0;
  st->kugeln[n].z=0.0;
  st->kugeln[n].r=2.0+cos(TWOPI*modf(k,&mod))/2;
  n++;
  
  zeiger(st,2.0,0.75,-2.0,sec,&n);
  zeiger(st,1.0,1.0,-1.5,min,&n);
  zeiger(st,0.0,1.5,-1.0,hour,&n);
  
  for(n=0;n<st->maxk;n++)
    {
      ys=st->kugeln[n].y*cos(st->movef*sin(st->cycle*sec))+
	st->kugeln[n].z*sin(st->movef*sin(st->cycle*sec));
      zs=-st->kugeln[n].y*sin(st->movef*sin(st->cycle*sec))+
	st->kugeln[n].z*cos(st->movef*sin(st->cycle*sec));
      st->kugeln[n].y=ys;
      st->kugeln[n].z=zs;
    }
}
/*------------------------------------------------------------------*/
static void
t3d_sort(struct state *st, int l, int r)
{
  int i,j;
  kugeldat ex;
  double x;
  
  i=l;j=r;
  x=st->kugeln[(l+r)/2].d;
  while(1)
    {
      while(st->kugeln[i].d>x) i++;
      while(x>st->kugeln[j].d) j--;
      if (i<=j)
	{
	  ex=st->kugeln[i];st->kugeln[i]=st->kugeln[j];st->kugeln[j]=ex;
	  i++;j--;
	}
      if (i>j) break;
    }
  if (l<j) t3d_sort(st,l,j);
  if (i<r) t3d_sort(st,i,r);
}

/*------------------------------------------------------------------*/
static void
fill_kugel(struct state *st, int i, Pixmap buf, int setcol)
{
  double ra;
  int m,col,inr=3,d;
#ifdef USE_POLYGON
  int inc=1;
#endif
  d=(int)((ABS(st->kugeln[i].r1)*2));
  if (d==0) d=1;
  
#ifdef FASTDRAW
  if(st->fastdraw && d<st->fastch)
    {
#	ifdef FASTCOPY
      XCopyArea(st->dpy, st->fastmask, buf, st->andgc, sum1ton(d)-(d+1)/2, 1,d,d,
		(int)(st->kugeln[i].x1)-d/2, (int)(st->kugeln[i].y1)-d/2);
      XCopyArea(st->dpy, st->fastcircles, buf, st->orgc, sum1ton(d)-(d+1)/2, 1,d,d,
		(int)(st->kugeln[i].x1)-d/2, (int)(st->kugeln[i].y1)-d/2);
#	else
      XPutImage(st->dpy, buf, st->andgc, st->fastmask[d-1], 0, 0,
		(int)(st->kugeln[i].x1)-d/2, (int)(st->kugeln[i].y1)-d/2, d, d);
      XPutImage(st->dpy, buf, st->orgc, st->fastcircles[d-1], 0, 0,
		(int)(st->kugeln[i].x1)-d/2, (int)(st->kugeln[i].y1)-d/2, d, d);
#	endif
    }
  else
#endif
    {
      if(ABS(st->kugeln[i].r1)<6.0) inr=9;
      
      for (m=0;m<=28;m+=inr)
	{
	  ra=st->kugeln[i].r1*sqrt(1-m*m/(28.0*28.0));
#ifdef PRTDBX
	  printf("Radius: %f\n",ra);
#endif
#ifdef USE_POLYGON
	  if(-ra< 3.0) inc=14;
	  else if(-ra< 6.0) inc=8;
	  else if(-ra<20.0) inc=4;
	  else if(-ra<40.0) inc=2;
#endif
	  if(setcol)
	    {
	      if (m==27)
                col=33;
	      else
		col=(int)(m);

	      if (col>33)
                col=33;

              col/=3;
	      setink(st->colors[col].pixel);
	    }

#ifdef USE_POLYGON
          {
            int n, nr;
	  for (n=0,nr=0;n<=sines-1;n+=inc,nr++)
	    {
	      track[nr].x=st->kugeln[i].x1+(int)(ra*st->sinus[n])+(st->kugeln[i].r1-ra)/2;
	      track[nr].y=st->kugeln[i].y1+(int)(ra*st->cosinus[n])+(st->kugeln[i].r1-ra)/2;
	    }
	  XFillPolygon(st->dpy,buf,st->gc,track,nr,Convex,CoordModeOrigin);
          }
#else /* Use XFillArc */
	  XFillArc(st->dpy, buf, st->gc,
		   (int)(st->kugeln[i].x1+(st->kugeln[i].r1+ra)/2),
		   (int)(st->kugeln[i].y1+(st->kugeln[i].r1+ra)/2),
		   (int)-(2*ra+1), (int)-(2*ra+1), 0, 360*64);
#endif
	}
    }
}

/*------------------------------------------------------------------*/

static void
init_3d(struct state *st)
{
  double i;
  int n=0;
  
  st->a[0]=0.0;
  st->a[1]=0.0;
  st->a[2]=-10.0;
  
  st->x[0]=10.0;
  st->x[1]=0.0;
  st->x[2]=0.0;
  
  st->y[0]=0.0;
  st->y[1]=10.0;
  st->y[2]=0.0;
  
  
  st->zoom=-10.0;
  st->speed=.0;
  
  for (i=0.0;n<sines;i+=TWOPI/sines,n++)
    {
      st->sinus[n]=sin(i);
      st->cosinus[n]=cos(i);
    }
}
/*------------------------------------------------------------------*/


static void
vektorprodukt(double feld1[], double feld2[], double feld3[])
{
  feld3[0]=feld1[1]*feld2[2]-feld1[2]*feld2[1];
  feld3[1]=feld1[2]*feld2[0]-feld1[0]*feld2[2];
  feld3[2]=feld1[0]*feld2[1]-feld1[1]*feld2[0];
}


/*------------------------------------------------------------------*/
static void
turn(double feld1[], double feld2[], double winkel)
{
  double temp[3];
  double s,ca,sa,sx1,sx2,sx3;
  
  vektorprodukt(feld1,feld2,temp);
  
  s=feld1[0]*feld2[0]+feld1[1]*feld2[1]+feld1[2]*feld2[2];
  
  sx1=s*feld2[0];
  sx2=s*feld2[1];
  sx3=s*feld2[2];
  sa=sin(winkel);ca=cos(winkel);
  feld1[0]=ca*(feld1[0]-sx1)+sa*temp[0]+sx1;
  feld1[1]=ca*(feld1[1]-sx2)+sa*temp[1]+sx2;
  feld1[2]=ca*(feld1[2]-sx3)+sa*temp[2]+sx3;
}
/*------------------------------------------------------------------*/

/* 1: Blickrichtung v;3:Ebenenmittelpunkt m 
   double feld1[],feld3[]; */
static void 
viewpoint(struct state *st)
{
  st->am[0]=-st->zoom*st->v[0];
  st->am[1]=-st->zoom*st->v[1];
  st->am[2]=-st->zoom*st->v[2];
  
  st->zaehler=norm*norm*st->zoom;
}

/*------------------------------------------------------------------*/
static void 
projektion(struct state *st)
{
  double c1[3],c2[3],k[3],x1,y1;
  double cno,cnorm/*,magnit*/;
  int i;
  
  for (i=0;i<st->maxk;i++)
    {
      c1[0]=st->kugeln[i].x-st->a[0];
      c1[1]=st->kugeln[i].y-st->a[1];
      c1[2]=st->kugeln[i].z-st->a[2];
      cnorm=sqrt(c1[0]*c1[0]+c1[1]*c1[1]+c1[2]*c1[2]);
      
      c2[0]=c1[0];
      c2[1]=c1[1];
      c2[2]=c1[2];
      
      cno=c2[0]*st->v[0]+c2[1]*st->v[1]+c2[2]*st->v[2];
      st->kugeln[i].d=cnorm;
      if (cno<0) st->kugeln[i].d=-20.0;
      
      
      st->kugeln[i].r1=(st->mag*st->zoom*st->kugeln[i].r/cnorm);
      
      c2[0]=st->v[0]/cno;
      c2[1]=st->v[1]/cno;
      c2[2]=st->v[2]/cno;
      
      vektorprodukt(c2,c1,k);

      
      x1=(st->startx+(st->x[0]*k[0]+st->x[1]*k[1]+st->x[2]*k[2])*st->mag);
      y1=(st->starty-(st->y[0]*k[0]+st->y[1]*k[1]+st->y[2]*k[2])*st->mag);
      if(   (x1>-2000.0)
	 && (x1<st->scrnWidth+2000.0)
	 && (y1>-2000.0)
	 && (y1<st->scrnHeight+2000.0))
	{
	  st->kugeln[i].x1=(int)x1;
	  st->kugeln[i].y1=(int)y1;
	}
      else
	{
	  st->kugeln[i].x1=0;
	  st->kugeln[i].y1=0;
	  st->kugeln[i].d=-20.0;
	}
    }
}


static void *
t3d_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  st->dpy = dpy;
  st->window = window;
  initialize(st);
  
  initColor(st,st->r,st->g,st->b);
  init_3d(st);
  st->zeit=(struct tm *)malloc(sizeof(struct tm));
  init_kugel(st);
  
  st->startx=st->scrnWidth/2;
  st->starty=st->scrnHeight/2;
  st->scrnH2=st->startx;
  st->scrnW2=st->starty;
  st->vspeed=0;
  
  
  vektorprodukt(st->x,st->y,st->v);
  viewpoint(st/*m,v*/);
  
  setink (BlackPixelOfScreen(st->xgwa.screen));
  XFillRectangle (st->dpy, st->window, st->gc, 0, 0, st->scrnWidth, st->scrnHeight);
  XQueryPointer (st->dpy, st->window, &st->junk_win, &st->junk_win, &st->junk, &st->junk,
		 &st->px, &st->py, &st->kb);
  
  return st;
}

static unsigned long
t3d_draw (Display *d, Window w, void *closure)
{
  struct state *st = (struct state *) closure;
  double dtime;
  double vnorm;

	
  /*--------------- Zeichenteil --------------*/

  vektorprodukt(st->x,st->y,st->v);
	
  vnorm=sqrt(st->v[0]*st->v[0]+st->v[1]*st->v[1]+st->v[2]*st->v[2]);
  st->v[0]=st->v[0]*norm/vnorm;
  st->v[1]=st->v[1]*norm/vnorm;
  st->v[2]=st->v[2]*norm/vnorm;
  vnorm=sqrt(st->x[0]*st->x[0]+st->x[1]*st->x[1]+st->x[2]*st->x[2]);
  st->x[0]=st->x[0]*norm/vnorm;
  st->x[1]=st->x[1]*norm/vnorm;
  st->x[2]=st->x[2]*norm/vnorm;
  vnorm=sqrt(st->y[0]*st->y[0]+st->y[1]*st->y[1]+st->y[2]*st->y[2]);
  st->y[0]=st->y[0]*norm/vnorm;
  st->y[1]=st->y[1]*norm/vnorm;
  st->y[2]=st->y[2]*norm/vnorm;
	
  projektion(st);
  t3d_sort (st,0,st->maxk-1);
	
  dtime=gettime(st);
	
  if(st->cycl)
    {
      st->draw_color=(int)(64.0*(dtime/60-floor(dtime/60)))-32;
	    
      if(st->draw_color<0)
        st->draw_color=-st->draw_color;
	    
      setink(st->colors[st->draw_color/3].pixel);
    }
  else
    setink(BlackPixelOfScreen (st->xgwa.screen));
	
  XFillRectangle(st->dpy,st->buffer,st->gc,0,0,st->scrnWidth,st->scrnHeight);
	
  {
    int i;
	  
    manipulate(st,dtime);
	  
    for (i=0;i<st->maxk;i++)
      {
        if (st->kugeln[i].d>0.0)
          fill_kugel(st,i,st->buffer,1);
      }
  }

  /* manipulate(gettime());
     var+=PI/500;
     if (var>=TWOPI) var=PI/500; */
	
  if(st->hsvcycl!=0.0)
    {
      dtime=st->hsvcycl*dtime/10.0+st->hue/360.0;
      dtime=360*(dtime-floor(dtime));
	    
      hsv2rgb(dtime,st->sat,st->val,&st->r,&st->g,&st->b);
      changeColor(st,st->r,st->g,st->b);
    }

  XCopyArea (st->dpy, st->buffer, st->window, st->gc, 0, 0, st->scrnWidth, st->scrnHeight, 0, 0);
	
	
  /*-------------------------------------------------*/
	
  XQueryPointer (st->dpy, st->window, &st->junk_win, &st->in_win, &st->junk, &st->junk,
                        &st->px, &st->py, &st->kb);
	
  if ((st->px>0)&&(st->px<st->scrnWidth)&&(st->py>0)&&(st->py<st->scrnHeight) )		
    {
      if ((st->px !=st->startx)&&(st->kb&Button2Mask))
        {
          /* printf("y=(%f,%f,%f)",y[0],y[1],y[2]);*/
          turn(st->y,st->x,((double)(st->px-st->startx))/(8000*st->mag));
          /* printf("->(%f,%f,%f)\n",y[0],y[1],y[2]);*/
        }
      if ((st->py !=st->starty)&&(st->kb&Button2Mask)) 
        {
          /* printf("x=(%f,%f,%f)",x[0],x[1],x[2]);*/
          turn(st->x,st->y,((double)(st->py-st->starty))/(-8000*st->mag));
          /* printf("->(%f,%f,%f)\n",x[0],x[1],x[2]);*/
        }
      if ((st->kb&Button1Mask)) 
        {
          if (st->vturn==0.0) st->vturn=.005; else if (st->vturn<2)  st->vturn+=.01;
          turn(st->x,st->v,.002*st->vturn);
          turn(st->y,st->v,.002*st->vturn); 
        }
      if ((st->kb&Button3Mask)) 
        {
          if (st->vturn==0.0) st->vturn=.005; else if (st->vturn<2) st->vturn+=.01;
          turn(st->x,st->v,-.002*st->vturn);
          turn(st->y,st->v,-.002*st->vturn);
        }
    }
  if (!(st->kb&Button1Mask)&&!(st->kb&Button3Mask)) 
    st->vturn=0;
	
  st->speed=st->speed+st->speed*st->vspeed;
  if ((st->speed<0.0000001) &&(st->vspeed>0.000001)) st->speed=0.000001;
  st->vspeed=.1*st->vspeed;
  if (st->speed>0.01) st->speed=.01;
  st->a[0]=st->a[0]+st->speed*st->v[0];
  st->a[1]=st->a[1]+st->speed*st->v[1];
  st->a[2]=st->a[2]+st->speed*st->v[2];

  return st->timewait;
}

static void
t3d_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  if (w != st->scrnWidth ||
      h != st->scrnHeight)
    {
      XFreePixmap (st->dpy, st->buffer); 
      st->scrnWidth = w;
      st->scrnHeight = h;
      st->buffer = XCreatePixmap (st->dpy, st->window, st->scrnWidth, st->scrnHeight, st->xgwa.depth);
	      
      st->startx=st->scrnWidth/2;
      st->starty=st->scrnHeight/2;
      st->scrnH2=st->startx;
      st->scrnW2=st->starty;
    }
}

static Bool
t3d_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->type == KeyPress)
    {
      KeySym keysym;
      char kpr = 0;
      XLookupString (&event->xkey, &kpr, 1, &keysym, 0);
      switch (kpr)
        {
        case 's': case 'S':
          st->vspeed = 0.5;
          return True;
        case 'a': case 'A':
          st->vspeed = -0.3;
          return True;
        case '0':
          st->speed = 0;
          st->vspeed = 0;
          return True;
        case 'z': case 'Z':
          st->mag *= 1.02;
          return True;
        case 'x': case 'X':
          st->mag /= 1.02;
          return True;
        default:
          break;
        }
    }
  return False;
}

static void
t3d_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->gc);
  XFreeGC (dpy, st->orgc);
  XFreeGC (dpy, st->andgc);
  XFreePixmap (dpy, st->buffer);
  free (st->zeit);
  free (st);
}




/*-------------------------------------------------*/

static const char *t3d_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*move:	0.5",
  "*wobble:	2.0",
  "*cycle:	10.0",
  "*mag:	1.0",
  "*minutes:	False",
  "*delay:      40000",
  "*fast:	50",
  "*colcycle:	False",
  "*hsvcycle:	0.0",
  "*rgb:        ",
  "*hsv:        ",
  0
};

static XrmOptionDescRec t3d_options [] = {
  { "-move",		".move",	XrmoptionSepArg, 0 },
  { "-wobble",		".wobble",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionSepArg, 0 },
  { "-mag",		".mag",		XrmoptionSepArg, 0 },
  { "-minutes",		".minutes",	XrmoptionNoArg, "True" },
  { "-no-minutes",	".minutes",	XrmoptionNoArg, "False" },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-fast",		".fast",	XrmoptionSepArg, 0 },
  { "-colcycle",	".colcycle",	XrmoptionSepArg, 0 },
  { "-hsvcycle",	".hsvcycle",	XrmoptionSepArg, 0 },
  { "-rgb",		".rgb",		XrmoptionSepArg, 0 },
  { "-hsv",		".hsv",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("T3D", t3d)
