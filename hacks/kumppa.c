/*

Copyright (C) Teemu Suutari (temisu@utu.fi) Feb 1998

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/




/*

*** This is contest-version. Don't look any further, code is *very* ugly.

*/


#include <math.h>
#include "screenhack.h"

#ifdef HAVE_XDBE_EXTENSION
# include <X11/extensions/Xdbe.h>
#endif /* HAVE_XDBE_EXTENSION */

char *progclass="Kumppa";

char *defaults [] ={
	".background:		black",
	"*speed:		0.1",
	"*delay:		0",
	0
};

XrmOptionDescRec options [] = {
	{"-delay",".delay",XrmoptionSepArg,0},
	{"-speed",".speed",XrmoptionSepArg,0},
	{"-random",".random",XrmoptionIsArg,0},
#ifdef HAVE_XDBE_EXTENSION
	{"-dbuf",".dbuf",XrmoptionIsArg,0},
#endif /* HAVE_XDBE_EXTENSION */
	{0,0,0,0}
};

const char colors[96]=
	{0,0,255, 0,51,255, 0,102,255, 0,153,255, 0,204,255,
	0,255,255,0,255,204, 0,255,153, 0,255,102, 0,255,51,
	0,255,0, 51,255,0, 102,255,0, 153,255,0, 204,255,0,
	255,255,0, 255,204,0, 255,153,0, 255,102,0, 255,51,0,
	255,0,0, 255,0,51, 255,0,102, 255,0,153, 255,0,204,
	255,0,255, 219,0,255, 182,0,255, 146,0,255, 109,0,255,
	73,0,255, 37,0,255};
const float cosinus[8][6]={{-0.07,0.12,-0.06,32,25,37},{0.08,-0.03,0.05,51,46,32},{0.12,0.07,-0.13,27,45,36},
	{0.05,-0.04,-0.07,36,27,39},{-0.02,-0.07,0.1,21,43,42},{-0.11,0.06,0.02,51,25,34},{0.04,-0.15,0.02,42,32,25},
	{-0.02,-0.04,-0.13,34,20,15}};

static float acosinus[8][3]={{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}};
static int coords[8];
static int ocoords[8]={0,0,0,0,0,0,0,0};

static Display *dpy;
static Window win[2];
static GC fgc[33];
static GC cgc;
static int sizx,sizy;
static int midx,midy;
static unsigned long delay;
static Bool cosilines=True;
#ifdef HAVE_XDBE_EXTENSION
static Bool usedouble=False;
#endif /* HAVE_XDBE_EXTENSION */

static int *Xrotations;
static int *Yrotations;
static int *Xrottable;
static int *Yrottable;

static int *rotateX;
static int *rotateY;

static int rotsizeX,rotsizeY;
static int stateX,stateY;

static int rx,ry;


int Satnum(int maxi)
{
return (int)(maxi*frand(1));
}


void palaRotate(int x,int y)
{
int ax,ay,bx,by,cx,cy;

ax=rotateX[x];
ay=rotateY[y];
bx=rotateX[x+1]+2;
by=rotateY[y+1]+2;
cx=rotateX[x]-(y-ry)+x-rx;
cy=rotateY[y]+(x-rx)+y-ry;
if (cx<0)
	{
	ax-=cx;
	cx=0;
	}
if (cy<0)
	{
	ay-=cy;
	cy=0;
	}
if (cx+bx-ax>sizx) bx=ax-cx+sizx;
if (cy+by-ay>sizy) by=ay-cy+sizy;
if (ax<bx && ay<by)
	XCopyArea(dpy,win[0],win[1],cgc,ax,ay,bx-ax,by-ay,cx,cy);
}


void rotate(void)
{
int x,y;
int dx,dy;

rx=Xrottable[stateX+1]-Xrottable[stateX];
ry=Yrottable[stateY+1]-Yrottable[stateY];


for (x=0;x<=rx;x++)
	rotateX[x]=(x)?midx-1-Xrotations[Xrottable[stateX+1]-x]:0;
for (x=0;x<=rx;x++)
	rotateX[x+rx+1]=(x==rx)?sizx-1:midx+Xrotations[Xrottable[stateX]+x];
for (y=0;y<=ry;y++)
	rotateY[y]=(y)?midy-1-Yrotations[Yrottable[stateY+1]-y]:0;
for (y=0;y<=ry;y++)
	rotateY[y+ry+1]=(y==ry)?sizy-1:midy+Yrotations[Yrottable[stateY]+y];

x=(rx>ry)?rx:ry;
for (dy=0;dy<(x+1)<<1;dy++)
	for (dx=0;dx<(x+1)<<1;dx++)
		{
		y=(rx>ry)?ry-rx:0;
		if (dy+y>=0 && dy<(ry+1)<<1 && dx<(rx+1)<<1)
			if (dy+y+dx<=ry+rx && dy+y-dx<=ry-rx)
				{
				palaRotate((rx<<1)+1-dx,dy+y);
				palaRotate(dx,(ry<<1)+1-dy-y);
				}
		y=(ry>rx)?rx-ry:0;
		if (dy+y>=0 && dx<(ry+1)<<1 && dy<(rx+1)<<1)
			if (dy+y+dx<=ry+rx && dx-dy-y>=ry-rx)
				{
				palaRotate(dy+y,dx);
				palaRotate((rx<<1)+1-dy-y,(ry<<1)+1-dx);
				}
		}
stateX++;
if (stateX==rotsizeX) stateX=0;
stateY++;
if (stateY==rotsizeY) stateY=0;
}



Bool make_rots(double xspeed,double yspeed)
{
int a,b,c,f,g,j,k=0,l;
double m,om,ok;
double d,ix,iy;
int maxi;

Bool *chks;

rotsizeX=(int)(2/xspeed+1);
ix=(double)(midx+1)/(double)(rotsizeX);
rotsizeY=(int)(2/yspeed+1);
iy=(double)(midy+1)/(double)(rotsizeY);

Xrotations=malloc((midx+2)*sizeof(unsigned int));
Xrottable=malloc((rotsizeX+1)*sizeof(unsigned int));
Yrotations=malloc((midy+2)*sizeof(unsigned int));
Yrottable=malloc((rotsizeY+1)*sizeof(unsigned int));
chks=malloc(((midx>midy)?midx:midy)*sizeof(Bool));
if (!Xrottable || !Yrottable || !Xrotations || !Yrotations || !chks) return False;


maxi=0;
c=0;
d=0;
g=0;
for (a=0;a<midx;a++) chks[a]=True;
for (a=0;a<rotsizeX;a++)
	{
	Xrottable[a]=c;
	f=(int)(d+ix)-g;				/*viivojen lkm.*/
	g+=f;
	if (g>midx)
		{
		f-=g-midx;
		g=midx;
		}
	for (b=0;b<f;b++)
		{
		m=0;
		for (j=0;j<midx;j++)			/*testi*/
			{
			if (chks[j])
				{
				om=0;
				ok=1;
				l=0;
				while (j+l<midx && om+12*ok>m)
					{
					if (j-l>=0) if (chks[j-l]) om+=ok;
						else; else if (chks[l-j]) om+=ok;
					if (chks[j+l]) om+=ok;
					ok/=1.5;
					l++;
					}
				if (om>=m)
					{
					k=j;
					m=om;
					}
				}
			}
		chks[k]=False;
		l=c;
		while (l>=Xrottable[a])
			{
			if (l!=Xrottable[a]) Xrotations[l]=Xrotations[l-1];
			if (k>Xrotations[l] || l==Xrottable[a])
				{
				Xrotations[l]=k;
				c++;
				l=Xrottable[a];
				}
			l--;
			}
		}
	d+=ix;
	if (maxi<c-Xrottable[a]) maxi=c-Xrottable[a];
	}
Xrottable[a]=c;
rotateX=malloc((maxi+2)*sizeof(int)<<1);
if (!rotateX) return False;

maxi=0;
c=0;
d=0;
g=0;
for (a=0;a<midy;a++) chks[a]=True;
for (a=0;a<rotsizeY;a++)
	{
	Yrottable[a]=c;
	f=(int)(d+iy)-g;				/*viivojen lkm.*/
	g+=f;
	if (g>midy)
		{
		f-=g-midy;
		g=midy;
		}
	for (b=0;b<f;b++)
		{
		m=0;
		for (j=0;j<midy;j++)			/*testi*/
			{
			if (chks[j])
				{
				om=0;
				ok=1;
				l=0;
				while (j+l<midy && om+12*ok>m)
					{
					if (j-l>=0) if (chks[j-l]) om+=ok;
						else; else if (chks[l-j]) om+=ok;
					if (chks[j+l]) om+=ok;
					ok/=1.5;
					l++;
					}
				if (om>=m)
					{
					k=j;
					m=om;
					}
				}
			}
		chks[k]=False;
		l=c;
		while (l>=Yrottable[a])
			{
			if (l!=Yrottable[a]) Yrotations[l]=Yrotations[l-1];
			if (k>Yrotations[l] || l==Yrottable[a])
				{
				Yrotations[l]=k;
				c++;
				l=Yrottable[a];
				}
			l--;
			}

		}
	d+=iy;
	if (maxi<c-Yrottable[a]) maxi=c-Yrottable[a];
	}
Yrottable[a]=c;
rotateY=malloc((maxi+2)*sizeof(int)<<1);
if (!rotateY) return False;

free(chks);
return (True);
}


#ifdef HAVE_XDBE_EXTENSION
static XErrorHandler old_handler = 0;
static Bool got_BadMatch = False;
static int
BadMatch_ehandler (Display *dpy, XErrorEvent *error)
{
  if (error->error_code == BadMatch) {
    got_BadMatch = True;
    return 0;
  } else if (old_handler)
    return old_handler(dpy, error);
  else
    exit(1);
}
#endif /* HAVE_XDBE_EXTENSION */


Bool InitializeAll(void)
{
XGCValues xgcv;
XWindowAttributes xgwa;
XSetWindowAttributes xswa;
Colormap cmap;
XColor color;
int n,i;
double rspeed;

XGetWindowAttributes(dpy,win[0],&xgwa);
cmap=xgwa.colormap;
xswa.backing_store=Always;
XChangeWindowAttributes(dpy,win[0],CWBackingStore,&xswa);
xgcv.function=GXcopy;

xgcv.foreground=get_pixel_resource ("background", "Background", dpy, cmap);
fgc[32]=XCreateGC(dpy,win[0],GCForeground|GCFunction,&xgcv);

n=0;
if (mono_p)
	{
	fgc[0]=fgc[32];
	xgcv.foreground=get_pixel_resource ("foreground", "Foreground", dpy, cmap);
	fgc[1]=XCreateGC(dpy,win[0],GCForeground|GCFunction,&xgcv);
	for (i=0;i<32;i+=2) fgc[i]=fgc[0];
	for (i=1;i<32;i+=2) fgc[i]=fgc[1];
	} else
	for (i=0;i<32;i++)
	{
		color.red=colors[n++]<<8;
		color.green=colors[n++]<<8;
		color.blue=colors[n++]<<8;
		color.flags=DoRed|DoGreen|DoBlue;
		XAllocColor(dpy,cmap,&color);
		xgcv.foreground=color.pixel;
		fgc[i]=XCreateGC(dpy,win[0],GCForeground|GCFunction,&xgcv);
	}
cgc=XCreateGC(dpy,win[0],GCForeground|GCFunction,&xgcv);
XSetGraphicsExposures(dpy,cgc,False);

if (get_string_resource("random","String")!=NULL && get_string_resource("random","String")[0]!=0) cosilines=False;

#ifdef HAVE_XDBE_EXTENSION
if (get_string_resource("dbuf","String")!=NULL && get_string_resource("dbuf","String")[0]!=0) usedouble=True;
if (usedouble)
	{
	XdbeQueryExtension(dpy,&n,&i);
	if (n==0 && i==0)
		{
		fprintf(stderr,"Double buffer extension not supported!\n");
		usedouble=False;
		}
	}
if (usedouble)
  {
    /* We need to trap an X error when calling XdbeAllocateBackBufferName,
       because there is no way to know beforehand whether the call will
       succeed!  This is a totally fucked design, but the man page says:

       ERRORS
	  BadMatch
	       The specified window is not an InputOutput window or
	       its visual does not support DBE.

       With SGI's O2 X server, some visuals support double-buffering (the
       12-bit pseudocolor visuals) and others yield a BadMatch error, as
       documented.

       However, it doesn't matter, because using the DBUF extension seems
       to make it run *slower* instead of faster anyway.

                                                        -- jwz, 1-Jul-98
     */
    XSync(dpy, False);
    old_handler = XSetErrorHandler (BadMatch_ehandler);
    got_BadMatch = False;
    win[1] = 0;
    win[1] = XdbeAllocateBackBufferName(dpy,win[0],XdbeUndefined);
    XSync(dpy, False);
    XSetErrorHandler (old_handler);
    old_handler = 0;
    XSync(dpy, False);
    if (got_BadMatch || !win[1])
      {
	fprintf(stderr, "%s: visual 0x%x does not support double-buffering.\n",
		progname, XVisualIDFromVisual(xgwa.visual));
	usedouble = False;
	win[1] = win[0];
	got_BadMatch = False;
      }
  }
#endif /* HAVE_XDBE_EXTENSION */

delay=get_integer_resource("delay","Integer");
rspeed=get_float_resource("speed","Float");
if (rspeed<0.0001 || rspeed>0.2)
	{
	fprintf(stderr,"Speed not in valid range! (0.0001 - 0.2), using 0.1 \n");
	rspeed=0.1;
	}

sizx=xgwa.width;
sizy=xgwa.height;
midx=sizx>>1;
midy=sizy>>1;
stateX=0;
stateY=0;

if (!make_rots(rspeed,rspeed))
	{
	fprintf(stderr,"Not enough memory for tables!\n");
	return False;
	}
return True;
}


void screenhack(Display *d, Window w)
{
#ifdef HAVE_XDBE_EXTENSION
XdbeSwapInfo xdswp;
#endif /* HAVE_XDBE_EXTENSION */
int a,b,c=0,e;
float f;

dpy=d;
win[0]=w;
if (!InitializeAll()) return;

#ifdef HAVE_XDBE_EXTENSION
if (usedouble)
	{
	xdswp.swap_action=XdbeUndefined;
	xdswp.swap_window=win[0];
	}
 else
#endif /* HAVE_XDBE_EXTENSION */
   win[1]=win[0];

while (0==0)
	{
	if (cosilines)
		{
		c++;
		for (a=0;a<8;a++)
			{
			f=0;
			for (b=0;b<3;b++)
				{
				acosinus[a][b]+=cosinus[a][b];
				f+=cosinus[a][b+3]*sin((double)acosinus[a][b]);
				}
			coords[a]=(int)f;
			}
		for (a=0;a<4;a++)
			{
			XDrawLine(dpy,win[0],(mono_p)?fgc[1]:fgc[((a<<2)+c)&31],midx+ocoords[a<<1],midy+ocoords[(a<<1)+1]
			,midx+coords[a<<1],midy+coords[(a<<1)+1]);
			ocoords[a<<1]=coords[a<<1];
			ocoords[(a<<1)+1]=coords[(a<<1)+1];
			}

		} else {
		for (e=0;e<8;e++)
			{
			a=Satnum(50);
			if (a>=32) a=32;
			b=Satnum(32)-16+midx;
			c=Satnum(32)-16+midy;
			XFillRectangle(dpy,win[0],fgc[a],b,c,2,2);
			}
		}
	XFillRectangle(dpy,win[0],fgc[32],midx-2,midy-2,4,4);
	rotate();
#ifdef HAVE_XDBE_EXTENSION
	if (usedouble) XdbeSwapBuffers(dpy,&xdswp,1);
#endif /* HAVE_XDBE_EXTENSION */
	XSync(dpy,True);
	if (delay) usleep (delay);
	}
}
