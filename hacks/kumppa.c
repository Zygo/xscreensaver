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

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static const char *kumppa_defaults [] ={
  ".background:		black",
  "*fpsSolid:		true",
  "*speed:		0.1",
  "*delay:		10000",
  "*random:		True",
  /* leave this off by default, since it slows things down.  -- jwz. */
  "*useDBE:		False",
  0
};

static XrmOptionDescRec kumppa_options [] = {
  {"-delay",     ".delay",  XrmoptionSepArg, 0 },
  {"-speed",     ".speed",  XrmoptionSepArg, 0 },
  {"-random",    ".random", XrmoptionNoArg, "True"  },
  {"-no-random", ".random", XrmoptionNoArg, "False" },
  {"-db",       ".useDBE",  XrmoptionNoArg, "True"  },
  {"-no-db",    ".useDBE",  XrmoptionNoArg, "False" },
  {0,0,0,0}
};

static const unsigned char colors[96]=
  {0,0,255, 0,51,255, 0,102,255, 0,153,255, 0,204,255,
   0,255,255,0,255,204, 0,255,153, 0,255,102, 0,255,51,
   0,255,0, 51,255,0, 102,255,0, 153,255,0, 204,255,0,
   255,255,0, 255,204,0, 255,153,0, 255,102,0, 255,51,0,
   255,0,0, 255,0,51, 255,0,102, 255,0,153, 255,0,204,
   255,0,255, 219,0,255, 182,0,255, 146,0,255, 109,0,255,
   73,0,255, 37,0,255};
static const float cosinus[8][6]=
 {{-0.07,0.12,-0.06,32,25,37},{0.08,-0.03,0.05,51,46,32},{0.12,0.07,-0.13,27,45,36},
  {0.05,-0.04,-0.07,36,27,39},{-0.02,-0.07,0.1,21,43,42},{-0.11,0.06,0.02,51,25,34},{0.04,-0.15,0.02,42,32,25},
  {-0.02,-0.04,-0.13,34,20,15}};


struct state {
  Display *dpy;
  Window win[2];

  float acosinus[8][3];
  int coords[8];
  int ocoords[8];

  GC fgc[33];
  GC cgc;
  int sizx,sizy;
  int midx,midy;
  unsigned long delay;
  Bool cosilines;

  int *Xrotations;
  int *Yrotations;
  int *Xrottable;
  int *Yrottable;

  int *rotateX;
  int *rotateY;

  int rotsizeX,rotsizeY;
  int stateX,stateY;

  int rx,ry;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeSwapInfo xdswp;
  Bool usedouble;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  int draw_count;
};


static int Satnum(int maxi)
{
  return (int)(maxi*frand(1));
}


static void palaRotate(struct state *st, int x,int y)
{
  int ax,ay,bx,by,cx,cy;

  ax=st->rotateX[x];
  ay=st->rotateY[y];
  bx=st->rotateX[x+1]+2;
  by=st->rotateY[y+1]+2;
  cx=st->rotateX[x]-(y-st->ry)+x-st->rx;
  cy=st->rotateY[y]+(x-st->rx)+y-st->ry;
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
  if (cx+bx-ax>st->sizx) bx=ax-cx+st->sizx;
  if (cy+by-ay>st->sizy) by=ay-cy+st->sizy;
  if (ax<bx && ay<by)
    XCopyArea(st->dpy,st->win[0],st->win[1],st->cgc,ax,ay,bx-ax,by-ay,cx,cy);
}


static void rotate(struct state *st)
{
  int x,y;
  int dx,dy;

  st->rx=st->Xrottable[st->stateX+1]-st->Xrottable[st->stateX];
  st->ry=st->Yrottable[st->stateY+1]-st->Yrottable[st->stateY];


  for (x=0;x<=st->rx;x++)
    st->rotateX[x]=(x)?st->midx-1-st->Xrotations[st->Xrottable[st->stateX+1]-x]:0;
  for (x=0;x<=st->rx;x++)
    st->rotateX[x+st->rx+1]=(x==st->rx)?st->sizx-1:st->midx+st->Xrotations[st->Xrottable[st->stateX]+x];
  for (y=0;y<=st->ry;y++)
    st->rotateY[y]=(y)?st->midy-1-st->Yrotations[st->Yrottable[st->stateY+1]-y]:0;
  for (y=0;y<=st->ry;y++)
    st->rotateY[y+st->ry+1]=(y==st->ry)?st->sizy-1:st->midy+st->Yrotations[st->Yrottable[st->stateY]+y];

  x=(st->rx>st->ry)?st->rx:st->ry;
  for (dy=0;dy<(x+1)<<1;dy++)
    for (dx=0;dx<(x+1)<<1;dx++)
      {
        y=(st->rx>st->ry)?st->ry-st->rx:0;
        if (dy+y>=0 && dy<(st->ry+1)<<1 && dx<(st->rx+1)<<1)
          if (dy+y+dx<=st->ry+st->rx && dy+y-dx<=st->ry-st->rx)
            {
              palaRotate(st, (st->rx<<1)+1-dx,dy+y);
              palaRotate(st, dx,(st->ry<<1)+1-dy-y);
            }
        y=(st->ry>st->rx)?st->rx-st->ry:0;
        if (dy+y>=0 && dx<(st->ry+1)<<1 && dy<(st->rx+1)<<1)
          if (dy+y+dx<=st->ry+st->rx && dx-dy-y>=st->ry-st->rx)
            {
              palaRotate(st, dy+y,dx);
              palaRotate(st, (st->rx<<1)+1-dy-y,(st->ry<<1)+1-dx);
            }
      }
  st->stateX++;
  if (st->stateX==st->rotsizeX) st->stateX=0;
  st->stateY++;
  if (st->stateY==st->rotsizeY) st->stateY=0;
}



static Bool make_rots(struct state *st, double xspeed,double yspeed)
{
  int a,b,c,f,g,j,k=0,l;
  double m,om,ok;
  double d,ix,iy;
  int maxi;

  Bool *chks;

  st->rotsizeX=(int)(2/xspeed+1);
  ix=(double)(st->midx+1)/(double)(st->rotsizeX);
  st->rotsizeY=(int)(2/yspeed+1);
  iy=(double)(st->midy+1)/(double)(st->rotsizeY);

  st->Xrotations=malloc((st->midx+2)*sizeof(unsigned int));
  st->Xrottable=malloc((st->rotsizeX+1)*sizeof(unsigned int));
  st->Yrotations=malloc((st->midy+2)*sizeof(unsigned int));
  st->Yrottable=malloc((st->rotsizeY+1)*sizeof(unsigned int));
  chks=malloc(((st->midx>st->midy)?st->midx:st->midy)*sizeof(Bool));
  if (!st->Xrottable || !st->Yrottable || !st->Xrotations || !st->Yrotations || !chks) return False;


  maxi=0;
  c=0;
  d=0;
  g=0;
  for (a=0;a<st->midx;a++) chks[a]=True;
  for (a=0;a<st->rotsizeX;a++)
    {
      st->Xrottable[a]=c;
      f=(int)(d+ix)-g;				/*viivojen lkm.*/
      g+=f;
      if (g>st->midx)
        {
          f-=g-st->midx;
          g=st->midx;
        }
      for (b=0;b<f;b++)
        {
          m=0;
          for (j=0;j<st->midx;j++)			/*testi*/
            {
              if (chks[j])
                {
                  om=0;
                  ok=1;
                  l=0;
                  while (j+l<st->midx && om+12*ok>m)
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
          while (l>=st->Xrottable[a])
            {
              if (l!=st->Xrottable[a]) st->Xrotations[l]=st->Xrotations[l-1];
              if (k>st->Xrotations[l] || l==st->Xrottable[a])
                {
                  st->Xrotations[l]=k;
                  c++;
                  l=st->Xrottable[a];
                }
              l--;
            }
        }
      d+=ix;
      if (maxi<c-st->Xrottable[a]) maxi=c-st->Xrottable[a];
    }
  st->Xrottable[a]=c;
  st->rotateX=malloc((maxi+2)*sizeof(int)<<1);
  if (!st->rotateX) return False;

  maxi=0;
  c=0;
  d=0;
  g=0;
  for (a=0;a<st->midy;a++) chks[a]=True;
  for (a=0;a<st->rotsizeY;a++)
    {
      st->Yrottable[a]=c;
      f=(int)(d+iy)-g;				/*viivojen lkm.*/
      g+=f;
      if (g>st->midy)
        {
          f-=g-st->midy;
          g=st->midy;
        }
      for (b=0;b<f;b++)
        {
          m=0;
          for (j=0;j<st->midy;j++)			/*testi*/
            {
              if (chks[j])
                {
                  om=0;
                  ok=1;
                  l=0;
                  while (j+l<st->midy && om+12*ok>m)
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
          while (l>=st->Yrottable[a])
            {
              if (l!=st->Yrottable[a]) st->Yrotations[l]=st->Yrotations[l-1];
              if (k>st->Yrotations[l] || l==st->Yrottable[a])
                {
                  st->Yrotations[l]=k;
                  c++;
                  l=st->Yrottable[a];
                }
              l--;
            }

        }
      d+=iy;
      if (maxi<c-st->Yrottable[a]) maxi=c-st->Yrottable[a];
    }
  st->Yrottable[a]=c;
  st->rotateY=malloc((maxi+2)*sizeof(int)<<1);
  if (!st->rotateY) return False;

  free(chks);
  return (True);
}


static Bool InitializeAll(struct state *st)
{
  XGCValues xgcv;
  XWindowAttributes xgwa;
/*  XSetWindowAttributes xswa;*/
  Colormap cmap;
  XColor color;
  int n,i;
  double rspeed;

  st->cosilines = True;

  XGetWindowAttributes(st->dpy,st->win[0],&xgwa);
  cmap=xgwa.colormap;
/*  xswa.backing_store=Always;
  XChangeWindowAttributes(st->dpy,st->win[0],CWBackingStore,&xswa);*/
  xgcv.function=GXcopy;

  xgcv.foreground=get_pixel_resource (st->dpy, cmap, "background", "Background");
  st->fgc[32]=XCreateGC(st->dpy,st->win[0],GCForeground|GCFunction,&xgcv);

  n=0;
  if (mono_p)
    {
      st->fgc[0]=st->fgc[32];
      xgcv.foreground=get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
      st->fgc[1]=XCreateGC(st->dpy,st->win[0],GCForeground|GCFunction,&xgcv);
      for (i=0;i<32;i+=2) st->fgc[i]=st->fgc[0];
      for (i=1;i<32;i+=2) st->fgc[i]=st->fgc[1];
    } else
    for (i=0;i<32;i++)
      {
        color.red=colors[n++]<<8;
        color.green=colors[n++]<<8;
        color.blue=colors[n++]<<8;
        color.flags=DoRed|DoGreen|DoBlue;
        XAllocColor(st->dpy,cmap,&color);
        xgcv.foreground=color.pixel;
        st->fgc[i]=XCreateGC(st->dpy,st->win[0],GCForeground|GCFunction,&xgcv);
      }
  st->cgc=XCreateGC(st->dpy,st->win[0],GCForeground|GCFunction,&xgcv);
  XSetGraphicsExposures(st->dpy,st->cgc,False);

  st->cosilines = get_boolean_resource(st->dpy, "random","Boolean");

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (get_boolean_resource (st->dpy, "useDBE", "Boolean"))
    st->usedouble = True;
  st->win[1] = xdbe_get_backbuffer (st->dpy, st->win[0], XdbeUndefined);
  if (!st->win[1])
    {
      st->usedouble = False;
      st->win[1] = st->win[0];
    }
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  st->delay=get_integer_resource(st->dpy, "delay","Integer");
  rspeed=get_float_resource(st->dpy, "speed","Float");
  if (rspeed<0.0001 || rspeed>0.2)
    {
      fprintf(stderr,"Speed not in valid range! (0.0001 - 0.2), using 0.1 \n");
      rspeed=0.1;
    }

  st->sizx=xgwa.width;
  st->sizy=xgwa.height;
  st->midx=st->sizx>>1;
  st->midy=st->sizy>>1;
  st->stateX=0;
  st->stateY=0;

  if (!make_rots(st,rspeed,rspeed))
    {
      fprintf(stderr,"Not enough memory for tables!\n");
      return False;
    }
  return True;
}

static void *
kumppa_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy=d;
  st->win[0]=w;
  if (!InitializeAll(st)) abort();

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->usedouble)
    {
      st->xdswp.swap_action=XdbeUndefined;
      st->xdswp.swap_window=st->win[0];
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    st->win[1]=st->win[0];

  return st;
}

static unsigned long
kumppa_draw (Display *d, Window w, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->cosilines)
    {
      int a;
      st->draw_count++;
      for (a=0;a<8;a++)
        {
          float f=0;
          int b;
          for (b=0;b<3;b++)
            {
              st->acosinus[a][b]+=cosinus[a][b];
              f+=cosinus[a][b+3]*sin((double)st->acosinus[a][b]);
            }
          st->coords[a]=(int)f;
        }
      for (a=0;a<4;a++)
        {
          XDrawLine(st->dpy,st->win[0],(mono_p)?st->fgc[1]:st->fgc[((a<<2)+st->draw_count)&31],st->midx+st->ocoords[a<<1],st->midy+st->ocoords[(a<<1)+1]
                    ,st->midx+st->coords[a<<1],st->midy+st->coords[(a<<1)+1]);
          st->ocoords[a<<1]=st->coords[a<<1];
          st->ocoords[(a<<1)+1]=st->coords[(a<<1)+1];
        }

    } else {
    int e;
    for (e=0;e<8;e++)
      {
        int a=Satnum(50);
        int b;
        if (a>=32) a=32;
        b=Satnum(32)-16+st->midx;
        st->draw_count=Satnum(32)-16+st->midy;
        XFillRectangle(st->dpy,st->win[0],st->fgc[a],b,st->draw_count,2,2);
      }
  }
  XFillRectangle(st->dpy,st->win[0],st->fgc[32],st->midx-2,st->midy-2,4,4);
  rotate(st);
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->usedouble) XdbeSwapBuffers(st->dpy,&st->xdswp,1);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  return st->delay;
}


static void
kumppa_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->sizx=w;
  st->sizy=w;
  st->midx=st->sizx>>1;
  st->midy=st->sizy>>1;
  st->stateX=0;
  st->stateY=0;
}

static Bool
kumppa_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
kumppa_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  for (i = 0; i < countof(st->fgc); i++)
    if (st->fgc[i]) XFreeGC (dpy, st->fgc[i]);
  XFreeGC (dpy, st->cgc);
  free (st->Xrotations);
  free (st->Yrotations);
  free (st->Xrottable);
  free (st->Yrottable);
  free (st);
}

XSCREENSAVER_MODULE ("Kumppa", kumppa)
