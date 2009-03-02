/*Copyright © Chris Le Sueur (thefishface@gmail.com) February 2005

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

Ultimate thanks go to Massimino Pascal, who created the original
xscreensaver hack, and inspired me with it's swirly goodness. This
version adds things like variable quality, number of functions and also
a groovier colouring mode.

*/

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "screenhack.h"

static float myrandom(float up)
{
  return(((float)random()/RAND_MAX)*up);
}

static int delay;
static int lensnum;
static int length;
static int mode;
static Bool notranslate, noscale, norotate;
float ht,wt;
float hs,ws;
int width,height;
float r;
long wcol;
int coloffset;

float nx,ny;

int count;

/*X stuff*/
GC gc;
Window w;
Display *dpy;
Pixmap backbuffer;
XColor *colours;
int ncolours;
int screen_num;

int blackColor, whiteColor;

char *progclass = "IFS";

char *defaults [] = {
  ".lensnum:    3",
  ".length:     9",
  ".mode:     0",
  ".colors:     200",
  "*delay:      10000",
  "*notranslate:  False",
  "*noscale:    False",
  "*norotate:   False",
  0
};

XrmOptionDescRec options [] = {
  { "-detail",    ".length",    XrmoptionSepArg, 0 },
  { "-delay",   ".delay",   XrmoptionSepArg, 0 },
  { "-mode",    ".mode",    XrmoptionSepArg, 0 },
  { "-colors",    ".colors",    XrmoptionSepArg, 0 },
  { "-functions", ".lensnum",   XrmoptionSepArg, 0 },
  { "-notranslate", ".notranslate", XrmoptionNoArg,  "True" },
  { "-noscale",   ".noscale",   XrmoptionNoArg,  "True" },
  { "-norotate",  ".norotate",  XrmoptionNoArg,  "True" },
  { 0, 0, 0, 0 }
};

/*Takes the average of two colours, with some nifty bit-shifting*/
static long blend(long c1, long c2)
{
  long R1=(c1 & 0xFF0000) >> 16;
  long R2=(c2 & 0xFF0000) >> 16;
  long G1=(c1 & 0x00FF00) >> 8;
  long G2=(c2 & 0x00FF00) >> 8;
  long B1=(c1 & 0x0000FF);
  long B2=(c2 & 0x0000FF);
  
  return (((R1+R2)/2 << 16) | ((G1+G2)/2 << 8) | ((B1+B2)/2));
}

/*Draw a point on the backbuffer*/
static void sp(float x, float y, long c)
{
  x+=16;    x*=wt;
  y=16.5-y; y*=ht;
  if(x<0 || x>=width || y<0 || y>=height) return;
  XSetForeground(dpy, gc, c);
  XDrawPoint(dpy, backbuffer, gc, (int)x, (int)y);
}

/*Copy backbuffer to front buffer and clear backbuffer*/
static void draw(void)
{
  XCopyArea(  dpy,
              backbuffer, w,
              gc,
              0, 0,
              width, height,
              0, 0);
  
  XSetForeground(dpy, gc, blackColor);
  XFillRectangle( dpy,
                  backbuffer,
                  gc,
                  0, 0,
                  width, height);
}

typedef struct {
  float r,s,tx,ty;
  /*Rotation, Scale, Translation X & Y*/
  float ra,raa,sa,txa,tya;
  
  int co;
} Lens; 


static void CreateLens(float nr, 
        float ns, 
        float nx, 
        float ny, 
        int nco,
        Lens *newlens)
{
  newlens->ra=newlens->raa=newlens->sa=newlens->txa=newlens->tya=0;
  if(!norotate) newlens->r=nr;
  else newlens->r=0;
    
  if(!noscale) newlens->s=ns;
  else newlens->s=0.5;
    
  if(!notranslate) {
    newlens->tx=nx;
    newlens->ty=ny;
  } else {
    newlens->tx=nx;
    newlens->tx=ny;
  }
    
  newlens->co=nco;
}
  
static float stepx(float x, float y, Lens *l)
{
  return l->s*cos(l->r)*x+l->s*sin(l->r)*y+l->tx;
}

static float stepy(float x, float y, Lens *l)
{
  return l->s*sin(l->r)*-x+l->s*cos(l->r)*y+l->ty;
}

static void mutate(Lens *l)
{
  if(!norotate) {
    l->raa+=myrandom(0.002)-0.001;
    l->ra+=l->raa;
    l->r +=l->ra;
    if(l->ra>0.07  || l->ra<-0.07)  l->ra/=1.4;
    if(l->raa>0.005 || l->raa<-0.005) l->raa/=1.2;
  }
  if(!noscale) {
    l->sa+=myrandom(0.01)-0.005;
    l->s +=l->sa;
    if(l->s>0.4) l->sa -=0.004;
    if(l->s<-0.4) l->sa +=0.004;
    if(l->sa>0.07  || l->sa<-0.07)  l->sa/=1.4;
  }
  if(!notranslate) {
    l->txa+=myrandom(0.004)-0.002;
    l->tya+=myrandom(0.004)-0.002;
    l->tx+=l->txa;
    l->ty+=l->tya;
    if(l->tx>6)  l->txa-=0.004;
    if(l->ty>6)  l->tya-=0.004;
    if(l->tx<-6)  l->txa+=0.004;
    if(l->ty<-6)  l->tya+=0.004;
    if(l->txa>0.05 || l->txa<-0.05) l->txa/=1.7;
    if(l->tya>0.05 || l->tya<-0.05) l->tya/=1.7;
  }
  
  /*Groovy, colour-shifting functions!*/
  l->co++;
  l->co %= ncolours;
}

Lens **lenses;

/* Calls itself <lensnum> times - with results from each lens/function.  *
 * After <length> calls to itself, it stops iterating and draws a point. */
static void iterate(float x, float y, long curcol, int length)
{
  int i;
  if(length == 0) {
    sp(x,y,curcol);
  } else {
    /*iterate(lenses[0].stepx(x,y),lenses[0].stepy(x,y),length-1);
      iterate(lenses[1].stepx(x,y),lenses[1].stepy(x,y),length-1);
      iterate(lenses[2].stepx(x,y),lenses[2].stepy(x,y),length-1);*/
    for(i=0;i<lensnum;i++) {
      switch(mode) {
      case 0 : iterate(stepx( x, y, lenses[i]), stepy( x, y, lenses[i]), blend( curcol,colours[(int)lenses[i]->co].pixel ), length-1); break;
      case 1 : iterate(stepx( x, y, lenses[i]), stepy( x, y, lenses[i]), colours[(int)lenses[i]->co].pixel, length-1); break;
      case 2 : iterate(stepx( x, y, lenses[i]), stepy( x, y, lenses[i]), curcol, length-1); break;
      default: exit(0);
      }
    }
  }
  count++;
}

/* Come on and iterate, iterate, iterate and sing... *
 * Yeah, this function just calls iterate, mutate,   *
 * and then draws everything.                        */
static void step(void)
{
  int i;
  if(mode == 2) {
    wcol++;
    wcol %= ncolours;
    iterate(0,0,colours[wcol].pixel,length);
  } else {
    iterate(0,0,0xFFFFFF,length);
  }
  
  
  count=0;
  
  for(i=0;i<lensnum;i++) {
    mutate(lenses[i]);
  }
  draw();
}

static void init_ifs(void)
{
  Window rw;
  int i;
  XWindowAttributes xgwa;
  
  delay = get_integer_resource("delay", "Delay");
  length = get_integer_resource("length", "Detail");
  mode = get_integer_resource("mode", "Mode");

  norotate    = get_boolean_resource("norotate", "NoRotate");
  noscale     = get_boolean_resource("noscale", "NoScale");
  notranslate = get_boolean_resource("notranslate", "NoTranslate");

  lensnum = get_integer_resource("lensnum", "Functions");
  
  lenses = malloc(sizeof(Lens)*lensnum);
  
  for(i=0;i<lensnum;i++) {
    lenses[i]=malloc(sizeof(Lens));
  }
  
  /*Thanks go to Dad for teaching me how to allocate memory for struct**s . */
  
  XGetWindowAttributes (dpy, w, &xgwa);
  width=xgwa.width;
  height=xgwa.height;
  
  /*Initialise all this X shizzle*/
  blackColor = BlackPixel(dpy, DefaultScreen(dpy));
  whiteColor = WhitePixel(dpy, DefaultScreen(dpy));
  rw = RootWindow(dpy, screen_num);
  screen_num = DefaultScreen(dpy);
  gc = XCreateGC(dpy, rw, 0, NULL);
  
  /* Do me some colourmap magic. If we're using blend mode, this is just   *
   * for the nice colours - we're still using true/hicolour. Screw me if   *
   * I'm going to work out how to blend with colourmaps - I'm too young to *
   * die!! On a sidenote, this is mostly stolen from halftone because I    *
   * don't really know what the hell I'm doing, here.                      */
  ncolours = get_integer_resource("colors", "Colors");
  if(ncolours < lensnum) ncolours=lensnum; /*apparently you're allowed to do this kind of thing...*/
  colours = (XColor *)calloc(ncolours, sizeof(XColor));
  make_smooth_colormap (  dpy,
                          xgwa.visual,
                          xgwa.colormap,
                          colours,
                          &ncolours,
                          True, 0, False);
  /*No, I didn't have a clue what that really did... hopefully I have some colours in an array, now.*/
  wcol = (int)myrandom(ncolours);
    
  /*Double buffering - I can't be bothered working out the XDBE thingy*/
  backbuffer = XCreatePixmap(dpy, w, width, height, XDefaultDepth(dpy, screen_num));
  
  /*Scaling factor*/
  wt=width/32;
  ht=height/24;
  
  ws=400;
  hs=400;

  /*Colourmapped colours for the general prettiness*/
  for(i=0;i<lensnum;i++) {
    CreateLens( myrandom(1)-0.5,
                myrandom(1),
                myrandom(4)-2,
                myrandom(4)+2,
                myrandom(ncolours),
                lenses[i]);
  }
}

void
screenhack (Display *display, Window window)
{
  dpy = display;
  w   = window;
  
  init_ifs();
  
  while (1) {
    step();
    screenhack_handle_events(dpy);
    if (delay) usleep(delay);
  }
}
