/*
 * Fireworkx 1.5 - pyrotechnics simulation program
 * Copyright (c) 1999-2005 Rony B Chandran <ronybc@asia.com>
 *
 * From Kerala, INDIA
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
 * Fixed array access problems by beating on it with a large hammer.
 * Nicholas Miell <nmiell@gmail.com>
 *
 * Freed 'free'ing up of memory by adding the forgotten 'XSync's
 * Renuka S <renuka@local.net>
 *
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

#define FWXVERSION "1.5"

#define SHELLCOUNT 3                   /* 3 or 5  */
#define PIXCOUNT 500                   /* 500     */
#define POWER 5                        /* 5       */
#define FTWEAK 12                      /* 12      */

#if HAVE_X86_MMX
void mmx_blur(char *a, int b, int c, int d); 
void mmx_glow(char *a, int b, int c, int d, char *e);
#endif

#define rnd(x) ((int)(random() % (x)))

static int depth;
static int bigendian;
static Bool flash_on   = True;
static Bool glow_on    = True;
static Bool verbose    = False;
static Bool shoot      = False;
static int width;
static int height;
static int rndlife;
static int minlife;
static int delay = 0;
static float flash_fade = 0.99;
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
  unsigned int cx,cy;
  unsigned int life;
  unsigned int color;
  unsigned int special;
  unsigned int cshift;
  unsigned int vgn,shy;
  float air,flash;
  firepix *fpix; }fireshell;

int explode(fireshell *fs)
{
  float air,adg = 0.001;         /* gravity */
  unsigned int n,c;
  unsigned int h = height;
  unsigned int w = width;
  unsigned int *prgb;
  unsigned char *palaka = palaka1;
  firepix *fp = fs->fpix;
  if(fs->vgn){
    if(--fs->cy == fs->shy){  
      fs->vgn   = 0;
      fs->flash = rnd(30000)+15000;}
    else{  
      fs->flash = 50+(fs->cy - fs->shy)*2;
      prgb=(unsigned int *)(palaka + (fs->cy * w + fs->cx + rnd(5)-2)*4);
     *prgb=(rnd(8)+8)*0x000f0f10;
      return(1);}}    
  if(fs->cshift) --fs->cshift;
  if((fs->cshift+1)%50==0) fs->color = ~fs->color;
  c = fs->color;
  air = fs->air;
  fs->flash *= flash_fade;
  for(n=PIXCOUNT;n;n--){
  if(fp->burn){ --fp->burn; 
  if(fs->special){
  fp->x += fp->xv = fp->xv * air + (float)(rnd(200)-100)/2000;
  fp->y += fp->yv = fp->yv * air + (float)(rnd(200)-100)/2000 + adg; }
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
  } return(--fs->life); }

void recycle(fireshell *fs,int x,int y)
{
  unsigned int n,pixlife;
  firepix *fp = fs->fpix;
  fs->vgn = shoot;
  fs->shy = y;
  fs->cx = x;
  fs->cy = shoot ? height : y ;
  fs->color = (rnd(155)+100) <<16 |
              (rnd(155)+100) <<8  |
               rnd(255);
  fs->life = rnd(rndlife)+minlife;
  fs->air  = 1-(float)(rnd(200))/10000;
  fs->flash   = rnd(30000)+15000;        /* million jouls */
  fs->cshift  = !rnd(5) ? 120:0; 
  fs->special = !rnd(10) ? 1:0; 
  if(verbose)
  printf("recycle(): color = %x air = %f life = %d \n",fs->color,fs->air,fs->life);
  pixlife = rnd(fs->life)+fs->life/10+1;    /* ! */
  for(n=0;n<PIXCOUNT;n++){
  fp->burn = rnd(pixlife)+32;
  fp->xv = POWER*(float)(rnd(20000)-10000)/10000;
  fp->yv = sqrt(POWER*POWER - fp->xv * fp->xv) *
               (float)(rnd(20000)-10000)/10000;
  fp->x = x;
  fp->y = y; 
  fp++;             }}

void glow(void)
{
  unsigned int n,q;
  unsigned int w = width;
  unsigned int h = height;
  unsigned char *pa, *pb, *pm, *po;
  pm = palaka1;
  po = palaka2;
  for(n=0;n<w*4;n++) 
  {pm[n]=0; po[n]=0;}    /* clean first line */
  pm+=n; po+=n; h-=2; 
  pa = pm-(w*4);
  pb = pm+(w*4);
  for(n=4;n<w*h*4-4;n++){
  q    = pm[n-4] + (pm[n]*8) + pm[n+4] + 
         pa[n-4] + pa[n] + pa[n+4] + 
         pb[n-4] + pb[n] + pb[n+4];
  q    -= q>8 ? 8:q;
  pm[n] = q/16;
  q     = q/8;
  if(q>255) q=255;
  po[n] = q;}
  pm+=n; po+=n;
  for(n=0;n<w*4;n++)
  {pm[n]=0; po[n]=0;}}   /* clean last line */

void blur(void)
{
  unsigned int n,q;
  unsigned int w = width;
  unsigned int h = height;
  unsigned char *pa, *pb, *pm;
  pm = palaka1;
  pm += w*4; h-=2;  /* line 0&h */
  pa = pm-(w*4);
  pb = pm+(w*4);
  for(n=4;n<w*h*4-4;n++){
  q    = pm[n-4] + (pm[n]*8) + pm[n+4] + 
         pa[n-4] + pa[n] + pa[n+4] + 
         pb[n-4] + pb[n] + pb[n+4];
  q    -= q>8 ? 8:q;
  pm[n] = q>>4;}
  pm += n-4;    /* last line */
  for(n=0;n<w*4+4;n++) pm[n]=0;
  pm = palaka1; /* first line */
  for(n=0;n<w*4+4;n++) pm[n]=pm[n+w*4]; }

void light_2x2(fireshell *fss)
{
  unsigned int l,t,n,x,y;
  float s;
  int w = width;
  int h = height;
  unsigned char *dim = palaka2;
  unsigned char *sim = palaka1;
  int nl = w*4;
  fireshell *f;
  if(glow_on) sim=dim;
  for(y=0;y<h;y+=2){
  for(x=0;x<w;x+=2){
  f = fss; s = 0;
  for(n=SHELLCOUNT;n;n--,f++){
  s += f->flash/(sqrt(1+(f->cx - x)*(f->cx - x)+
                        (f->cy - y)*(f->cy - y)));}
  l = s;

  t = l + sim[0];
  dim[0] = (t > 255 ? 255 : t);	
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

  sim += 8; dim += 8; } sim += nl; dim += nl;}}

void resize(Display *display, Window win)
{
  XWindowAttributes xwa;
  XGetWindowAttributes (display, win, &xwa);
  xwa.width  -= xwa.width % 2;
  xwa.height -= xwa.height % 2;
  if(xwa.height != height || xwa.width != width) {
  width  = xwa.width;
  height = xwa.height;
  if (verbose)
  printf("sky size: %dx%d ------------------------------\n",width,height);
  XSync(display,0);
  if (xim) {
  if (xim->data==(char *)palaka2) xim->data=NULL;  
  XDestroyImage(xim);
  XSync(display,0);
  if (palaka2!=palaka1) free(palaka2 - 8);
  free(palaka1 - 8); 
  }
  palaka1 = NULL;
  palaka2 = NULL;
  xim = XCreateImage(display, xwa.visual, xwa.depth, ZPixmap, 0, 0,
		     width, height, 8, 0);
  palaka1 = (unsigned char *) calloc(xim->height+1,xim->width*4) + 8;
  if(flash_on|glow_on)
  palaka2 = (unsigned char *) calloc(xim->height+1,xim->width*4) + 8;
  else
  palaka2 = palaka1;
  if (depth>=24)
  xim->data = (char *)palaka2;
  else
  xim->data = calloc(xim->height,xim->bytes_per_line); }}

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
  XPutImage(display,win,gc,xim,0,0,0,0,xim->width,xim->height); }

void sniff_events(Display *dis, Window win, fireshell *fss)
{
  XEvent e;
  while (XPending(dis)){
  XNextEvent (dis, &e);
  if (e.type == ConfigureNotify) resize(dis,win);
  if (e.type == ButtonPress)     recycle(fss,e.xbutton.x, e.xbutton.y);
  screenhack_handle_event(dis,&e); }}


char *progclass = "Fireworkx";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*delay:	10000",  /* never default to zero! */
  "*maxlife:	2000",
  "*flash:	True",
  "*glow:	True",
  "*shoot:	False",
  "*verbose:	False",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-maxlife",		".maxlife",	XrmoptionSepArg, 0 },
  { "-noflash",		".flash",	XrmoptionNoArg, "False" },
  { "-noglow",		".glow",	XrmoptionNoArg, "False" },
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
  firepix *fpix, *ffpix;
  fireshell *fshell, *ffshell;
  glow_on  = get_boolean_resource("glow"    , "Boolean");
  flash_on = get_boolean_resource("flash"   , "Boolean");
  shoot    = get_boolean_resource("shoot"   , "Boolean");
  verbose  = get_boolean_resource("verbose" , "Boolean");
  rndlife  = get_integer_resource("maxlife" , "Integer");
  delay    = get_integer_resource("delay"   , "Integer");
  minlife  = rndlife/4;
  if(rndlife<1000) flash_fade=0.98;
  if(rndlife<500) flash_fade=0.97;
  if(verbose){
  printf("Fireworkx %s - pyrotechnics simulation program \n", FWXVERSION);
  printf("Copyright (c) 1999-2005 Rony B Chandran <ronybc@asia.com> \n\n");
  printf("url: http://www.ronybc.8k.com \n\n");}

  XGetWindowAttributes(display,win,&xwa);
  depth     = xwa.depth;
  vi        = xwa.visual;
  cmap      = xwa.colormap;
  bigendian = (ImageByteOrder(display) == MSBFirst);
 
  if(depth==8){
  if(verbose){
  printf("Pseudocolor color: use '-noflash' for better results.\n");}
  colors = (XColor *) calloc(sizeof(XColor),ncolors+1);
  writable = False;
  make_smooth_colormap(display, vi, cmap, colors, &ncolors,
                                False, &writable, True);
  }
  gc = XCreateGC(display, win, 0, &gcv);

  resize(display,win);   /* initialize palakas */ 
  
  ffpix = malloc(sizeof(firepix) * PIXCOUNT * SHELLCOUNT);
  ffshell = malloc(sizeof(fireshell) * SHELLCOUNT);
  fshell = ffshell;
  fpix = ffpix;
  for (n=0;n<SHELLCOUNT;n++){
  fshell->fpix = fpix;
  recycle (fshell,rnd(width),rnd(height));
  fshell++; 
  fpix += PIXCOUNT; }
  
  while(1) {
  for(q=FTWEAK;q;q--){
  fshell=ffshell;
  for(n=SHELLCOUNT;n;n--){
  if (!explode(fshell)){
       recycle(fshell,rnd(width),rnd(height)); }
       fshell++; }}
#if HAVE_X86_MMX
  if(glow_on) mmx_glow(palaka1,width,height,8,palaka2);
#else
  if(glow_on) glow();
#endif
  if(flash_on) light_2x2(ffshell);
  put_image(display,win,gc,xim);
  usleep(delay);
  XSync(display,0);
  sniff_events(display, win, ffshell);
#if HAVE_X86_MMX
  if(!glow_on) mmx_blur(palaka1,width,height,8);
#else
  if(!glow_on) blur();
#endif

}}
