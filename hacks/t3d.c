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

#undef FASTDRAW
#undef USE_POLYGON

#ifdef FASTDRAW
# define FASTCOPY
#endif

#include <stdio.h>
#include <math.h>

#include "screenhack.h"


static int maxk=34;

#define   WIDTH      200
#define   HEIGHT     200
#define   norm       20.0

int timewait=40000;

#define   ROOT       0x1
#define PI M_PI
#define TWOPI 2*M_PI

#define MIN(i,j) ((i)<(j)?(i):(j))

#define kmax ((minutes?60:24))
/* Anzahl der Kugeln */
#define sines 52
/* Werte in der Sinus-Tabelle */
/*-----------------------------------------------------------------*/
#define setink(inkcolor) \
	XSetForeground (dpy,gc,inkcolor)

#define drawline(xx1,yy1,xx2,yy2) \
	XDrawLine(dpy,win,gc,xx1,yy1,xx2,yy2) 

#define drawseg(segments,nr_segments) \
	XDrawSegments(dpy,win,gc,segments,nr_segments) 


#define polyfill(ppts,pcount) \
	XFillPolygon(dpy,win,gc,ppts,pcount,Convex,CoordModeOrigin)


#define frac(argument) argument-floor(argument)

#define abs(x) ((x)<0.0 ? -(x) : (x))

static Colormap cmap;
/* static XColor gray1; */
static double r=1.0,g=1.0,b=1.0;
static double hue=0.0,sat=0.0,val=1.0;

typedef struct {
  double x,y,z,r,d,r1;
  int x1,y1;
} kugeldat;

/* Felder fuer 3D */

static kugeldat kugeln[100];

static double  a[3],/*m[3],*/am[3],x[3],y[3],v[3];
static double zoom,speed,zaehler,vspeed/*,AE*/;
static double vturn/*,aturn*/;
/* static XPoint track[sines]; */
static double sinus[sines];
static double cosinus[sines];

static int startx,starty;
static double /*magx,magy,*/mag=10;
/* static double lastx,lasty,lastz; */
/* static int lastcx,lastcy,lastcz; */
/* static int move=1; */
static int minutes=0;
static int cycl=0;
static double hsvcycl=0.0;
static double movef =0.5, wobber=2.0, cycle=6.0;

/* time */

/* static double sec; */

/* Windows */
static XWindowAttributes xgwa;
static GC	gc;
static GC orgc;
static GC andgc;
static Window	win;
/* static Font font; */
static Display	*dpy;
static int	screen, scrnWidth = WIDTH, scrnHeight = HEIGHT;
static Pixmap  buffer;
#define maxfast 100
static int fastch=50;
#ifdef FASTDRAW
#	ifdef FASTCOPY
#		define sum1ton(a) (((a)*(a)+1)/2)
#		define fastcw sum1ton(fastch)
		static Pixmap fastcircles;
		static Pixmap fastmask;
#	else
		static XImage* fastcircles[maxfast];
		static XImage* fastmask[maxfast];
#	endif
static int fastdraw=0;
#endif

static int scrnW2,scrnH2;
/* static unsigned short flags = 0; */
/* static char *text; */
static XColor colors[64];
static struct tm *zeit;

static int planes;
/* compute time */

static double
gettime (void)
{
  struct timeval time1;
  struct timezone zone1;
  struct tm *zeit;
  
  gettimeofday(&time1,&zone1);
  zeit=localtime(&time1.tv_sec);
  
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
changeColor (double r, double g, double b)
{
  int n,n1;
  
  n1=0;
  for(n=30;n<64;n+=3)
    {
      colors[n1].red   =1023+ n*(int)(1024.*r);
      colors[n1].blue  =1023+ n*(int)(1024.*b);
      colors[n1].green =1023+ n*(int)(1024.*g);
      
      n1++;
    }
  
  XStoreColors (dpy, cmap, colors, 12);
}

static void 
initColor (double r, double g, double b)
{
  int n,n1;
  unsigned long pixels[12];
  long dummy;
  
  cmap = xgwa.colormap;
  
  if(hsvcycl!=0.0 && XAllocColorCells(dpy, cmap, 0, &dummy, 0, pixels, 12))
    {
      for(n1=0;n1<12;n1++)
	{
	  colors[n1].pixel=pixels[n1];
	  colors[n1].flags=DoRed | DoGreen | DoBlue;
	}
      
      changeColor(r,g,b);
    }
  else
    {
      n1=0;
      for(n=30;n<64;n+=3)
	{
	  colors[n1].red   =1023+ n*(int)(1024.*r);
	  colors[n1].blue  =1023+ n*(int)(1024.*b);
	  colors[n1].green =1023+ n*(int)(1024.*g);
	  
	  if (!(XAllocColor (dpy, cmap, &colors[n1]))) {
	    (void) fprintf (stderr, "Error:  Cannot allocate colors\n");
	    exit (1);
	  }
	  
	  n1++;
	}
    }
}

/* ----------------WINDOW-------------------*/

static void
initialize (void)
{
  XGCValues *xgc;
  XGCValues *xorgc;
  XGCValues *xandgc;

  XGetWindowAttributes (dpy, win, &xgwa);
  scrnWidth = xgwa.width;
  scrnHeight = xgwa.height;

  cycle = 60.0 / get_float_resource ("cycle", "Float");
  movef = get_float_resource ("move", "Float") / 2;
  wobber *= get_float_resource ("wobble", "Float");

  {
    double magfac = get_float_resource ("mag", "Float");
    mag *= magfac;
    fastch=(int)(fastch*magfac);
  }

  if (get_boolean_resource ("minutes", "Boolean")) {
    minutes=1; maxk+=60-24;
  }

  timewait = get_integer_resource ("wait", "Integer");
  fastch = get_integer_resource ("fast", "Integer");
  cycl = get_boolean_resource ("colcycle", "Integer");
  hsvcycl = get_float_resource ("hsvcycle", "Integer");

  {
    char *s = get_string_resource ("rgb", "RGB");
    char dummy;
    if (s && *s)
      {
        double rr, gg, bb;
        if (3 == sscanf (s, "%lf %lf %lf %c", &rr, &gg, &bb, &dummy))
          r = rr, g = gg, b = bb;
      }
    if (s) free (s);

    s = get_string_resource ("hsv", "HSV");
    if (s && *s)
      {
        double hh, ss, vv;
        if (3 == sscanf (s, "%lf %lf %lf", &hh, &ss, &vv, &dummy)) {
          hue = hh, sat = ss, val = vv;
          hsv2rgb(hue,sat,val,&r,&g,&b);
        }
      }
    if (s) free (s);
  }

  if (fastch>maxfast)
		fastch=maxfast;
  
#ifdef PRTDBX
  printf("Set options:\ndisplay: '%s'\ngeometry: '%s'\n",display,geometry);
  printf("move\t%.2f\nwobber\t%.2f\nmag\t%.2f\ncycle\t%.4f\n",
	 movef,wobber,mag/10,cycle);
  printf("nice\t%i\nfast\t%i\nmarks\t%i\nwait\t%i\n",niced,fastch,maxk,timewait);
#endif
  
  xgc=( XGCValues *) malloc(sizeof(XGCValues) );
  xorgc=( XGCValues *) malloc(sizeof(XGCValues) );
  xandgc=( XGCValues *) malloc(sizeof(XGCValues) );

  screen = screen_number (xgwa.screen);
  
  planes=xgwa.depth;

  gc = XCreateGC (dpy, win, 0,  xgc);
  xorgc->function =GXor;
  orgc = XCreateGC (dpy, win, GCFunction,  xorgc);
  xandgc->function =GXandInverted;
  andgc = XCreateGC (dpy, win, GCFunction,  xandgc);
  
  buffer = XCreatePixmap (dpy, win, scrnWidth, scrnHeight,
			  xgwa.depth); 
  
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
  fastcircles = XCreatePixmap (dpy, win, fastcw, fastch+1, xgwa.depth);
  fastmask    = XCreatePixmap (dpy, win, fastcw, fastch+1, xgwa.depth);
#endif
  
  setink(BlackPixel (dpy, screen));
  XFillRectangle (dpy, buffer     , gc, 0, 0, scrnWidth, scrnHeight);	
  
#ifdef FASTCOPY
  
  setink(0);
  XFillRectangle (dpy, fastcircles, gc, 0, 0, fastcw, fastch+1);
  XFillRectangle (dpy, fastmask   , gc, 0, 0, fastcw, fastch+1);
  
#endif
}

static void fill_kugel(int i, Pixmap buf, int setcol);


/*------------------------------------------------------------------*/
static void 
init_kugel(void)
{
  
#ifdef FASTDRAW
  int i;

  for(i=0; i<fastch; i++)
    {
#	ifdef FASTCOPY
      kugeln[i].r1=-((double) i)/2 -1;
      kugeln[i].x1=sum1ton(i);
      kugeln[i].y1=((double) i)/2 +1;
      
      fill_kugel(i,fastcircles,1);
      setink((1<<MIN(24,xgwa.depth))-1);
      fill_kugel(i,fastmask,0);
#	else
      kugeln[i].r1=-((double) i)/2 -1;
      kugeln[i].x1=kugeln[i].y1=((double) i)/2 +1;
      
      fill_kugel(i,buffer,1);
      fastcircles[i]=XGetImage(dpy,buffer,0,0,i+2,i+2,(1<<planes)-1,ZPixmap);
      
      setink((1<<MIN(24,xgwa.depth))-1);
      fill_kugel(i,buffer,0);
      fastmask[i]=XGetImage(dpy,buffer,0,0,i+2,i+2,(1<<planes)-1,ZPixmap);
      
      setink(0);
      XFillRectangle (dpy, buffer     , gc, 0, 0, scrnWidth, scrnHeight);	
#	endif
    }
  fastdraw=1;
#endif
}

/* Zeiger zeichnen */

static void
zeiger(double dist,double rad, double z, double sec, int *q)
{
  int i,n;
  double gratio=sqrt(2.0/(1.0+sqrt(5.0)));
  
  n = *q;
  
  for(i=0;i<3;i++)
    {
      kugeln[n].x=dist*cos(sec);
      kugeln[n].y=-dist*sin(sec);
      kugeln[n].z=z;
      kugeln[n].r=rad;
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
manipulate(double k)
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
      
      kugeln[n].x=4.0*sin(i);
      kugeln[n].y=4.0*cos(i);
      kugeln[n].z=wobber* /* (sin(floor(2+2*l/(PI))*i)*sin(2*l)); */
	cos((i-sec)*floor(2+5*l/(PI)))*sin(5*l);
      if(minutes)
	{
	  kugeln[n].r=/* (1.0+0.3*cos(floor(2+2*l/(PI))*i)*sin(2*l))* */
	    ((n % 5!=0) ? 0.3 : 0.6)*
	      ((n % 15 ==0) ? 1.25 : .75);
	}
      else
	{
	  kugeln[n].r=/* (1.0+0.3*cos(floor(2+2*l/(PI))*i)*sin(2*l))* */
	    ((n & 1) ? 0.5 : 1.0)*
	      ((n % 6==0) ? 1.25 : .75);
	}
      i+=TWOPI/kmax;
    }

  kugeln[n].x=0.0;
  kugeln[n].y=0.0;
  kugeln[n].z=0.0;
  kugeln[n].r=2.0+cos(TWOPI*modf(k,&mod))/2;
  n++;
  
  zeiger(2.0,0.75,-2.0,sec,&n);
  zeiger(1.0,1.0,-1.5,min,&n);
  zeiger(0.0,1.5,-1.0,hour,&n);
  
  for(n=0;n<maxk;n++)
    {
      ys=kugeln[n].y*cos(movef*sin(cycle*sec))+
	kugeln[n].z*sin(movef*sin(cycle*sec));
      zs=-kugeln[n].y*sin(movef*sin(cycle*sec))+
	kugeln[n].z*cos(movef*sin(cycle*sec));
      kugeln[n].y=ys;
      kugeln[n].z=zs;
    }
}
/*------------------------------------------------------------------*/
static void
t3d_sort(int l, int r)
{
  int i,j;
  kugeldat ex;
  double x;
  
  i=l;j=r;
  x=kugeln[(l+r)/2].d;
  while(1)
    {
      while(kugeln[i].d>x) i++;
      while(x>kugeln[j].d) j--;
      if (i<=j)
	{
	  ex=kugeln[i];kugeln[i]=kugeln[j];kugeln[j]=ex;
	  i++;j--;
	}
      if (i>j) break;
    }
  if (l<j) t3d_sort(l,j);
  if (i<r) t3d_sort (i,r);
}

/*------------------------------------------------------------------*/
static void
fill_kugel(int i, Pixmap buf, int setcol)
{
  double ra;
  int m,col,inc=1,inr=3,d;
  d=(int)((abs(kugeln[i].r1)*2));
  if (d==0) d=1;
  
#ifdef FASTDRAW
  if(fastdraw && d<fastch)
    {
#	ifdef FASTCOPY
      XCopyArea(dpy, fastmask, buf, andgc, sum1ton(d)-(d+1)/2, 1,d,d,
		(int)(kugeln[i].x1)-d/2, (int)(kugeln[i].y1)-d/2);
      XCopyArea(dpy, fastcircles, buf, orgc, sum1ton(d)-(d+1)/2, 1,d,d,
		(int)(kugeln[i].x1)-d/2, (int)(kugeln[i].y1)-d/2);
#	else
      XPutImage(dpy, buf, andgc, fastmask[d-1], 0, 0,
		(int)(kugeln[i].x1)-d/2, (int)(kugeln[i].y1)-d/2, d, d);
      XPutImage(dpy, buf, orgc, fastcircles[d-1], 0, 0,
		(int)(kugeln[i].x1)-d/2, (int)(kugeln[i].y1)-d/2, d, d);
#	endif
	}
  else
#endif
    {
      if(abs(kugeln[i].r1)<6.0) inr=9;
      
      for (m=0;m<=28;m+=inr)
	{
	  ra=kugeln[i].r1*sqrt(1-m*m/(28.0*28.0));
#ifdef PRTDBX
	  printf("Radius: %f\n",ra);
#endif
	  if(-ra< 3.0) inc=14;
	  else if(-ra< 6.0) inc=8;
	  else if(-ra<20.0) inc=4;
	  else if(-ra<40.0) inc=2;
	  if(setcol)
	    {
	      if (m==27) col=33;
	      else
		col=(int)(m);
	      if (col>33) col=33;	col/=3;
	      setink(colors[col].pixel);
	    }

#ifdef USE_POLYGON
          {
            int n, nr;
	  for (n=0,nr=0;n<=sines-1;n+=inc,nr++)
	    {
	      track[nr].x=kugeln[i].x1+(int)(ra*sinus[n])+(kugeln[i].r1-ra)/2;
	      track[nr].y=kugeln[i].y1+(int)(ra*cosinus[n])+(kugeln[i].r1-ra)/2;
	    }
	  XFillPolygon(dpy,buf,gc,track,nr,Convex,CoordModeOrigin);
          }
#else /* Use XFillArc */
	  XFillArc(dpy, buf, gc,
		   (int)(kugeln[i].x1+(kugeln[i].r1+ra)/2),
		   (int)(kugeln[i].y1+(kugeln[i].r1+ra)/2),
		   (int)-(2*ra+1), (int)-(2*ra+1), 0, 360*64);
#endif
	}
    }
}

/*------------------------------------------------------------------*/

static void
init_3d(void)
{
  double i;
  int n=0;
  
  a[0]=0.0;
  a[1]=0.0;
  a[2]=-10.0;
  
  x[0]=10.0;
  x[1]=0.0;
  x[2]=0.0;
  
  y[0]=0.0;
  y[1]=10.0;
  y[2]=0.0;
  
  
  zoom=-10.0;
  speed=.0;
  
  for (i=0.0;n<sines;i+=TWOPI/sines,n++)
    {
      sinus[n]=sin(i);
      cosinus[n]=cos(i);
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
viewpoint(void)
{
  am[0]=-zoom*v[0];
  am[1]=-zoom*v[1];
  am[2]=-zoom*v[2];
  
  zaehler=norm*norm*zoom;
}
/*------------------------------------------------------------------*/
static void 
projektion(void)
{
  double c1[3],c2[3],k[3],x1,y1;
  double cno,cnorm/*,magnit*/;
  int i;
  
  for (i=0;i<maxk;i++)
    {
      c1[0]=kugeln[i].x-a[0];
      c1[1]=kugeln[i].y-a[1];
      c1[2]=kugeln[i].z-a[2];
      cnorm=sqrt(c1[0]*c1[0]+c1[1]*c1[1]+c1[2]*c1[2]);
      
      c2[0]=c1[0];
      c2[1]=c1[1];
      c2[2]=c1[2];
      
      cno=c2[0]*v[0]+c2[1]*v[1]+c2[2]*v[2];
      kugeln[i].d=cnorm;
      if (cno<0) kugeln[i].d=-20.0;
      
      
      kugeln[i].r1=(mag*zoom*kugeln[i].r/cnorm);
      
      c2[0]=v[0]/cno;
      c2[1]=v[1]/cno;
      c2[2]=v[2]/cno;
      
      vektorprodukt(c2,c1,k);

      
      x1=(startx+(x[0]*k[0]+x[1]*k[1]+x[2]*k[2])*mag);
      y1=(starty-(y[0]*k[0]+y[1]*k[1]+y[2]*k[2])*mag);
      if(   (x1>-2000.0)
	 && (x1<scrnWidth+2000.0)
	 && (y1>-2000.0)
	 && (y1<scrnHeight+2000.0))
	{
	  kugeln[i].x1=(int)x1;
	  kugeln[i].y1=(int)y1;
	}
      else
	{
	  kugeln[i].x1=0;
	  kugeln[i].y1=0;
	  kugeln[i].d=-20.0;
	}
    }
}

/*---------- event-handler ----------------*/
static void
event_handler(void)
{
  while (XEventsQueued (dpy, QueuedAfterReading))
    {
      XEvent event;
      
      XNextEvent (dpy, &event);
      switch (event.type)
	{
	case ConfigureNotify:
	  if (event.xconfigure.width != scrnWidth ||
	      event.xconfigure.height != scrnHeight)
	    {
	      XFreePixmap (dpy, buffer); 
	      scrnWidth = event.xconfigure.width;
	      scrnHeight = event.xconfigure.height;
	      buffer = XCreatePixmap (dpy, win, scrnWidth, scrnHeight,
                                      xgwa.depth);
	      
	      startx=scrnWidth/2;
	      starty=scrnHeight/2;
	      scrnH2=startx;
	      scrnW2=starty;
	    }; break;
	case KeyPress:
	  {
	    KeySym kpr=XKeycodeToKeysym(dpy,event.xkey.keycode,0);
	    if (kpr=='s') /* s */
	      vspeed=0.5;
	    if (kpr=='a')
	      vspeed=-0.3;
	    if (kpr=='q')
	      {
		speed=0;vspeed=0;
	      }
	    /*	printf("%i\n",event.xkey.keycode);*/
	    if (kpr=='z') mag*=1.02;
	    if (kpr=='x') mag/=1.02;
	  }
	default:
	  break;
	}
    }
  /*nap(40);-Ersatz*/ 
  {
    struct timeval timeout;
    timeout.tv_sec=timewait/1000000;
    timeout.tv_usec=timewait%1000000;
    (void)select(0,0,0,0,&timeout);
  }
}


/*-------------------------------------------------*/

char *progclass = "T3D";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*move:	0.5",
  "*wobble:	2.0",
  "*cycle:	6.0",
  "*mag:	1",
  "*minutes:	False",
  "*timewait:   40000",
  "*fast:	50",
  "*ccycle:	False",
  "*hsvcycle:	0.0",
  0
};

XrmOptionDescRec options [] = {
  { "-move",		".move",	XrmoptionSepArg, 0 },
  { "-wobble",		".wobble",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionSepArg, 0 },
  { "-mag",		".mag",		XrmoptionSepArg, 0 },
  { "-minutes",		".minutes",	XrmoptionSepArg, 0 },
  { "-timewait",	".timewait",	XrmoptionSepArg, 0 },
  { "-fast",		".fast",	XrmoptionSepArg, 0 },
  { "-colcycle",	".colcycle",	XrmoptionSepArg, 0 },
  { "-hsvcycle",	".hsvcycle",	XrmoptionSepArg, 0 },
  { "-rgb",		".rgb",		XrmoptionSepArg, 0 },
  { "-hsv",		".hsv",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *d, Window w)
{
  Window junk_win,in_win;
  
  int px,py,junk,kb/*,wai*/;
/*  int act,act1,tc;*/
  double vnorm;
  /* double var=0.0; */
  int color=0/*, dir=1*/;
  
  dpy = d;
  win = w;
  initialize();
  
  initColor(r,g,b);
  init_3d();
  zeit=(struct tm *)malloc(sizeof(struct tm));
  init_kugel();
  
  startx=scrnWidth/2;
  starty=scrnHeight/2;
  scrnH2=startx;
  scrnW2=starty;
  vspeed=0;
  
  
  vektorprodukt(x,y,v);
  viewpoint(/*m,v*/);
  
  setink (BlackPixel (dpy, screen));
  XFillRectangle (dpy, win, gc, 0, 0, scrnWidth, scrnHeight);
  XQueryPointer (dpy, win, &junk_win, &junk_win, &junk, &junk,
		 &px, &py, &kb);
  
  for (;;)
    {	double dtime;
	
	/*--------------- Zeichenteil --------------*/

	event_handler();
	
	vektorprodukt(x,y,v);
	
	vnorm=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
	v[0]=v[0]*norm/vnorm;
	v[1]=v[1]*norm/vnorm;
	v[2]=v[2]*norm/vnorm;
	vnorm=sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
	x[0]=x[0]*norm/vnorm;
	x[1]=x[1]*norm/vnorm;
	x[2]=x[2]*norm/vnorm;
	vnorm=sqrt(y[0]*y[0]+y[1]*y[1]+y[2]*y[2]);
	y[0]=y[0]*norm/vnorm;
	y[1]=y[1]*norm/vnorm;
	y[2]=y[2]*norm/vnorm;
	
	projektion();
	t3d_sort (0,maxk-1);
	
	dtime=gettime();
	
	if(cycl)
	  {
	    color=(int)(64.0*(dtime/60-floor(dtime/60)))-32;
	    
	    if(color<0)
	      color=-color;
	    
	    setink(colors[color/3].pixel);
	  }
	else
	  setink(BlackPixel (dpy, screen));
	
	XFillRectangle(dpy,buffer,gc,0,0,scrnWidth,scrnHeight);
	
	{
	  int i;
	  
	  manipulate(dtime);
	  
	  for (i=0;i<maxk;i++)
	    {
	      if (kugeln[i].d>0.0)
		fill_kugel(i,buffer,1);
	    }
	}

	XSync(dpy,0);
	
	/* manipulate(gettime());
	   var+=PI/500;
	   if (var>=TWOPI) var=PI/500; */
	
	/*event_handler();*/
	
	if(hsvcycl!=0.0)
	  {
	    dtime=hsvcycl*dtime/10.0+hue/360.0;
	    dtime=360*(dtime-floor(dtime));
	    
	    hsv2rgb(dtime,sat,val,&r,&g,&b);
	    changeColor(r,g,b);
	  }

	XCopyArea (dpy, buffer, win, gc, 0, 0, scrnWidth, scrnHeight, 0, 0);
	
	
	/*-------------------------------------------------*/
	XSync(dpy,0);
	
	event_handler();
	
	(void)(XQueryPointer (dpy, win, &junk_win, &in_win, &junk, &junk,
			      &px, &py, &kb));
	
	if ((px>0)&&(px<scrnWidth)&&(py>0)&&(py<scrnHeight) )		
	  {
	    if ((px !=startx)&&(kb&Button2Mask))
	      {
		/* printf("y=(%f,%f,%f)",y[0],y[1],y[2]);*/
		turn(y,x,((double)(px-startx))/(8000*mag));
		/* printf("->(%f,%f,%f)\n",y[0],y[1],y[2]);*/
	      }
	    if ((py !=starty)&&(kb&Button2Mask)) 
	      {
		/* printf("x=(%f,%f,%f)",x[0],x[1],x[2]);*/
		turn(x,y,((double)(py-starty))/(-8000*mag));
		/* printf("->(%f,%f,%f)\n",x[0],x[1],x[2]);*/
	      }
	    if ((kb&Button1Mask)) 
	      {
		if (vturn==0.0) vturn=.005; else if (vturn<2)  vturn+=.01;
		turn(x,v,.002*vturn);
		turn(y,v,.002*vturn); 
	      }
	    if ((kb&Button3Mask)) 
	      {
		if (vturn==0.0) vturn=.005; else if (vturn<2) vturn+=.01;
		turn(x,v,-.002*vturn);
		turn(y,v,-.002*vturn);
	      }
	  }
	if (!(kb&Button1Mask)&&!(kb&Button3Mask)) 
	  vturn=0;
	
	speed=speed+speed*vspeed;
	if ((speed<0.0000001) &&(vspeed>0.000001)) speed=0.000001;
	vspeed=.1*vspeed;
	if (speed>0.01) speed=.01;
	a[0]=a[0]+speed*v[0];
	a[1]=a[1]+speed*v[1];
	a[2]=a[2]+speed*v[2];
      }
}
