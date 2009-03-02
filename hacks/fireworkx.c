/*
 * Fireworkx 1.3 - pyrotechnics simulation program
 * Copyright (c) 1999-2004 Rony B Chandran <ronybc@asia.com>
 *
 * url: http://www.ronybc.8k.com
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Additional programming: 
 * ------------------------
 * Support for different display color modes: 
 * Jean-Pierre Demailly <Jean-Pierre.Demailly@ujf-grenoble.fr>
 *
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

#define FWXVERSION "1.3"

#define WIDTH 640
#define HEIGHT 480
#define SHELLCOUNT 3
#define PIXCOUNT 500                   /* 500     */
#define RNDLIFE0 250                   /* violent */
#define RNDLIFE1 1200                  /* 1200    */
#define MINLIFE0 50                    /* violent */
#define MINLIFE1 500                   /* 500     */
#define POWER 5                        /* 5       */
#define FTWEAK 12                      /* 12      */

static int depth;
static int bigendian;
static Bool light_on = True;
static Bool fade_on  = False;
static Bool shoot    = False;
static Bool verbose  = False;
static int delay     = 0;
static int fsc_width = 0;
static int fsc_height= 0;
static int rndlife = RNDLIFE1;
static int minlife = MINLIFE1;
static float light_fade = 0.99;
static unsigned char *palaka1=NULL;
static unsigned char *palaka2=NULL;
static XImage *xim=NULL;
static XColor *colors;
static int ncolors = 255;

typedef struct {
  unsigned int burn;
  float x;
  float y;
  float xv;
  float yv;}firepix;

typedef struct {
  unsigned int life;
  unsigned int color;
  unsigned int cx,cy;
  unsigned int special;
  unsigned int cshift;
  unsigned int vgn,shy;
  float air,lum;
  firepix *fpix; }fireshell;

int seed = 2387776;
int rnd(int x)
{   /* xscreensaver */
  if ((seed = seed % 44488 * 48271 - seed / 44488 * 3399) < 0)
  seed += 2147483647;
  return (seed-1) % x;
}

int explode(fireshell *fs)
{
  float air,adg = 0.001;     /* gravity */
  unsigned int n,c;
  unsigned int h = fsc_height;
  unsigned int w = fsc_width;
  unsigned int *prgb;
  unsigned char *palaka = palaka1;
  firepix *fp = fs->fpix;
  if(fs->vgn){
    if(--fs->cy == fs->shy){  
      fs->vgn = 0;
      fs->lum = 20000;}    /* millions of jouls..! */
    else{  
      fs->lum = 50+(fs->cy - fs->shy)*2;
      return(1);}}    
  if(fs->cshift) --fs->cshift;
  if((fs->cshift+1)%50==0) fs->color = ~fs->color;
  c = fs->color;
  air = fs->air;
  fs->lum *= light_fade;
  for(n=PIXCOUNT;n;n--){
  if(fp->burn){ --fp->burn; 
  if(fs->special){
  fp->x += fp->xv = fp->xv * air + (float)(rnd(200)-100)/2000;
  fp->y += fp->yv = fp->yv * air + (float)(rnd(200)-100)/2000 +adg; }
  else{
  fp->x += fp->xv = fp->xv * air + (float)(rnd(200)-100)/20000;
  fp->y += fp->yv = fp->yv * air + adg; }
  if(fp->y > h){
  if(rnd(5)==3) {fp->yv *= -0.24; fp->y = h;}
  else fp->burn=0;} /* touch muddy ground :) */
  if(fp->x < w && fp->x > 0 && fp->y < h && fp->y > 0){
     prgb = (unsigned int *)(palaka + ((int)fp->y * w + (int)fp->x)*4);
    *prgb = c; }
  } fp++;
  } return(--(fs->life));
}

void recycle(fireshell *fs,int x,int y)
{
  unsigned int n,pixlife;
  firepix *fp = fs->fpix;
  fs->vgn = shoot;
  fs->shy = y;
  fs->cx = x;
  fs->cy = shoot ? fsc_height : y ;
  fs->color = (rnd(155)+100) <<16 |
              (rnd(155)+100) <<8  |
               rnd(255);
  fs->life = rnd(rndlife)+minlife;
  fs->air  = 1-(float)(rnd(200))/10000;
  fs->lum  = 20000;
  fs->cshift  = !rnd(5) ? 120:0; 
  fs->special = !rnd(10) ? 1:0; 
  pixlife = rnd(fs->life)+fs->life/10+1;    /* ! */
  for(n=0;n<PIXCOUNT;n++){
  fp->burn = rnd(pixlife)+32;
  fp->xv = POWER*(float)(rnd(20000)-10000)/10000;
  fp->yv = sqrt(POWER*POWER - fp->xv * fp->xv) *
               (float)(rnd(20000)-10000)/10000;
  fp->x = x;
  fp->y = y; 
  fp++;             }
}

void blur_best(void)
{
  unsigned int n;
  unsigned int w = fsc_width;
  unsigned int h = fsc_height;
  unsigned char *pa, *pb, *pm;
  pm = palaka1;
  for(n=0;n<w*4;n++) pm[n]=0;    /* clean first line */
  pm+=n;
  h-=2; 
  pa = pm-(w*4);
  pb = pm+(w*4);
  if(fade_on){
  for(n=0;n<w*h*4;n++){
  pm[n]=(pm[n-4] + pm[n] + pm[n+4] + 
         pa[n-4] + pa[n] + pa[n+4] + 
         pb[n-4] + pb[n] + pb[n+4] +
         pm[n] + pm[n] + (pm[n]<<2))/ 16;}}
  else{
  for(n=0;n<w*h*4;n++){
  pm[n]=(pm[n-4] + pm[n] + pm[n+4] + 
         pa[n-4] + pa[n] + pa[n+4] + 
         pb[n-4] + pb[n] + pb[n+4])/ 9;}}
  pm+=n;
  for(n=0;n<w*4;n++) pm[n]=0;    /* clean last line */
}

void light_2x2(fireshell *fss)
{
  unsigned int l,t,n,x,y;
  float s;
  int w = fsc_width;
  int h = fsc_height;
  unsigned char *dim = palaka2;
  unsigned char *sim = palaka1;
  int nl = w*4;
  fireshell *f;
  for(y=0;y<h;y+=2){
  for(x=0;x<w;x+=2){
  f = fss; s = 0;
  for(n=SHELLCOUNT;n;n--,f++){
  s += f->lum / ( 1 + sqrt((f->cx - x)*(f->cx - x)+(f->cy - y)*(f->cy - y))); }
  l = s;

  t = l + sim[0];
  dim[0] = (t > 255 ? 255 : t);	 /* cmov's */
  t = l + sim[1];
  dim[1] = (t > 255 ? 255 : t);
  t = l + sim[2];
  dim[2] = (t > 255 ? 255 : t);

  t = l + sim[4];
  dim[4] = (t > 255 ? 255 : t);
  t = l + sim[5];
  dim[5] = (t > 255 ? 255 : t);
  t = l + sim[6];
  dim[6] = (t > 255 ? 255 : t);

  t = l + sim[nl+0];
  dim[nl+0] = (t > 255 ? 255 : t);
  t = l + sim[nl+1];
  dim[nl+1] = (t > 255 ? 255 : t);
  t = l + sim[nl+2];
  dim[nl+2] = (t > 255 ? 255 : t);

  t = l + sim[nl+4];
  dim[nl+4] = (t > 255 ? 255 : t);
  t = l + sim[nl+5];
  dim[nl+5] = (t > 255 ? 255 : t);
  t = l + sim[nl+6];
  dim[nl+6] = (t > 255 ? 255 : t);

  sim += 8; dim += 8; } sim += nl; dim += nl;}
}

void resize(Display *display, Window win)
{
  XWindowAttributes xwa;
  XGetWindowAttributes (display, win, &xwa);
  xwa.width  -= xwa.width % 4;
  xwa.height -= xwa.height % 4;
  if(xwa.height != fsc_height || xwa.width != fsc_width) {
  fsc_width  = xwa.width;
  fsc_height = xwa.height;
  if (xim) {
  if (xim->data==(char *)palaka2) xim->data=NULL;  
  XDestroyImage(xim);
  if (palaka2!=palaka1) free(palaka2);
  free(palaka1); 
  }
  palaka1 = NULL;     
  palaka2 = NULL; 
  xim = XCreateImage(display, xwa.visual, xwa.depth, ZPixmap, 0, 0,
		     fsc_width, fsc_height, 32, 0);
  palaka1 = calloc(xim->height,xim->width*4);
  if(light_on)
  palaka2 = calloc(xim->height,xim->width*4);
  else
  palaka2 = palaka1;
  if (depth>=24)
  xim->data = (char *)palaka2;
  else
  xim->data = calloc(xim->height,xim->bytes_per_line);       
 }
}

void put_image(Display *display, Window win, GC gc, XImage *xim)
{
  int x,y,i,j;
  unsigned char r, g, b;
  i = 0;
  j = 0;
  if (depth==16) {
     if(bigendian)
     for (y=0;y<xim->height; y++)
     for (x=0;x<xim->width; x++) {
     r = palaka2[j++];
     g = palaka2[j++];
     b = palaka2[j++];
     j++;
     xim->data[i++] = (g&224)>>5 | (r&248);
     xim->data[i++] = (b&248)>>3 | (g&28)<<3;
     }
     else
     for (y=0;y<xim->height; y++)
     for (x=0;x<xim->width; x++) {
     r = palaka2[j++];
     g = palaka2[j++];
     b = palaka2[j++];
     j++;
     xim->data[i++] = (b&248)>>3 | (g&28)<<3;
     xim->data[i++] = (g&224)>>5 | (r&248);
     }
  }
  if (depth==15) {
     if(bigendian)
     for (y=0;y<xim->height; y++)
     for (x=0;x<xim->width; x++) {
     r = palaka2[j++];
     g = palaka2[j++];
     b = palaka2[j++];
     j++;
     xim->data[i++] = (g&192)>>6 | (r&248)>>1;
     xim->data[i++] = (b&248)>>3 | (g&56)<<2;
     }
     else
     for (y=0;y<xim->height; y++)
     for (x=0;x<xim->width; x++) {
     r = palaka2[j++];
     g = palaka2[j++];
     b = palaka2[j++];
     j++;
     xim->data[i++] = (b&248)>>3 | (g&56)<<2;
     xim->data[i++] = (g&192)>>6 | (r&248)>>1;
     }
  }
  if (depth==8) {
     for (y=0;y<xim->height; y++)
     for (x=0;x<xim->width; x++) {
     r = palaka2[j++];
     g = palaka2[j++];
     b = palaka2[j++];
     j++;     
     xim->data[i++] = (((7*g)/256)*36)+(((6*r)/256)*6)+((6*b)/256);
     }
  }
  XPutImage(display,win,gc,xim,0,0,0,0,xim->width,xim->height); 
}

void sniff_events(Display *dis, Window win, fireshell *fss)
{
  XEvent e;
  while (XPending(dis)){
  XNextEvent (dis, &e);
  if (e.type == ConfigureNotify) resize(dis,win);
  if (e.type == ButtonPress)     recycle(fss,e.xbutton.x, e.xbutton.y);
  screenhack_handle_event(dis,&e);}
}


char *progclass = "Fireworkx";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*delay:	5000",
  "*maxlife:	1200",
  "*light:	True",
  "*fade:	False",
  "*shoot:	False",
  "*verbose:	False",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-maxlife",		".maxlife",	XrmoptionSepArg, 0 },
  { "-nolight",		".light",	XrmoptionNoArg, "False" },
  { "-fade",		".fade",	XrmoptionNoArg, "True" },
  { "-shoot",		".shoot",	XrmoptionNoArg, "True" },
  { "-verbose",		".verbose",	XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *display, Window win)
{
  unsigned int n,q;
  Visual *vi;
  Colormap cmap;
  Bool writable;
  XWindowAttributes xwa;
  GC gc;
  XGCValues gcv;
  firepix *fpixs, *fpix;
  fireshell *fshells, *fshell;
  fade_on  = get_boolean_resource("fade"    , "Boolean");
  light_on = get_boolean_resource("light"   , "Boolean");
  shoot    = get_boolean_resource("shoot"   , "Boolean");
  verbose  = get_boolean_resource("verbose" , "Boolean");
  rndlife  = get_integer_resource("maxlife" , "Integer");
  delay    = get_integer_resource("delay"   , "Integer");
  minlife  = rndlife/4;
  if(rndlife<1000) light_fade=0.98;
  if(rndlife<500) light_fade=0.97;
  if(fade_on) light_fade=0.97;
  if(verbose){
  printf("Fireworkx %s - pyrotechnics simulation program \n", FWXVERSION);
  printf("Copyright (c) 1999-2004 Rony B Chandran <ronybc@asia.com> \n\n");
  printf("url: http://www.ronybc.8k.com \n\n");}

  XGetWindowAttributes(display,win,&xwa);
  depth     = xwa.depth;
  vi        = xwa.visual;
  cmap      = xwa.colormap;
  bigendian = (ImageByteOrder(display) == MSBFirst);
 
  if(depth==8){
  if(verbose){
  printf("Pseudocolor color: use '-fade' & '-nolight' for better results.\n");}
  colors = (XColor *) calloc(sizeof(XColor),ncolors+1);
  writable = False;
  make_smooth_colormap(display, vi, cmap, colors, &ncolors,
                                False, &writable, True);
  }
  gc = XCreateGC(display, win, 0, &gcv);

  resize(display,win);   /* initialize palakas */ 
  seed += time(0);
  
  fpixs = malloc(sizeof(firepix) * PIXCOUNT * SHELLCOUNT);
  fshells = malloc(sizeof(fireshell) * SHELLCOUNT);
  fshell = fshells;
  fpix = fpixs;
  for (n=0;n<SHELLCOUNT;n++){
  fshell->fpix = fpix;
  recycle (fshell,rnd(fsc_width),rnd(fsc_height));
  fshell++; 
  fpix += PIXCOUNT; }
  
  while(1) {
  for(q=FTWEAK;q;q--){
  fshell=fshells;
  for(n=SHELLCOUNT;n;n--){
  if (!explode(fshell)){
       recycle(fshell,rnd(fsc_width),rnd(fsc_height)); }
       fshell++; }}
  if(light_on) light_2x2(fshells);
  put_image(display,win,gc,xim);
  usleep(delay);
  XSync(display,0);
  sniff_events(display, win, fshells);
  blur_best();}

}
