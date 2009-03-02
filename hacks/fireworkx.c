/*
 * Fireworkx 1.6 - pyrotechnics simulation program (XScreensaver version)
 * Copyright (c) 1999-2007 Rony B Chandran <ronybc@asia.com>
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
 * Support for different dpy color modes: 
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

#define FWXVERSION "1.6"

#define SHELLCOUNT 3                   /* 3 ; see light() before changing this value */
#define PIXCOUNT 500                   /* 500     */
#define POWER 5                        /* 5       */
#define FTWEAK 12                      /* 12      */

#if HAVE_X86_MMX
void mmx_blur(char *a, int b, int c, int d); 
void mmx_glow(char *a, int b, int c, int d, char *e);
#endif

#define rnd(x) ((x) ? (int)(random() % (x)) : 0)

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

struct state {
  Display *dpy;
  Window window;

  fireshell *fshell, *ffshell;
  GC gc;

  int depth;
  int bigendian;
  Bool flash_on;
  Bool glow_on;
  Bool verbose;
  Bool shoot;
  int width;
  int height;
  int rndlife;
  int minlife;
  int delay;
  float flash_fade;
  unsigned char *palaka1;
  unsigned char *palaka2;
  XImage *xim;
  XColor *colors;
  int ncolors;
};

static int explode(struct state *st, fireshell *fs)
{
  float air,adg = 0.001;         /* gravity */
  unsigned int n,c;
  unsigned int h = st->height;
  unsigned int w = st->width;
  unsigned int *prgb;
  unsigned char *palaka = st->palaka1;
  firepix *fp = fs->fpix;
  if(fs->vgn){
    if(--fs->cy == fs->shy){  
      fs->vgn   = 0;
      fs->flash = rnd(30000)+25000;
      fs->flash = fs->flash*fs->flash; }
    else{  
      fs->flash = 200000+(fs->cy - fs->shy)*(fs->cy - fs->shy)*8;
      prgb=(unsigned int *)(palaka + (fs->cy * w + fs->cx + rnd(5)-2)*4);
     *prgb=(rnd(8)+8)*0x000f0f10;
      return(1);}}    
  if(fs->cshift) --fs->cshift;
  if((fs->cshift+1)%50==0) fs->color = ~fs->color;
  c = fs->color;
  air = fs->air;
  fs->flash *= st->flash_fade;
  for(n=PIXCOUNT;n;n--){
  if(fp->burn){ --fp->burn; 
  if(fs->special){
  fp->x += fp->xv = fp->xv * air + (float)(rnd(200)-100)/2000;
  fp->y += fp->yv = fp->yv * air + (float)(rnd(200)-100)/2000+ adg;}
  else{
  fp->x += fp->xv = fp->xv * air + (float)(rnd(200)-100)/20000;
  fp->y += fp->yv = fp->yv * air + (float)(rnd(200)-100)/40000+ adg; }
  if(fp->y > h){
  if(rnd(5)==3) {fp->yv *= -0.24; fp->y = h;}
  else fp->burn=0;} /* touch muddy ground :) */
  if(fp->x < w && fp->x > 0 && fp->y < h && fp->y > 0){
     prgb = (unsigned int *)(palaka + ((int)fp->y * w + (int)fp->x)*4);
    *prgb = c; }
  } fp++;
  } return(--fs->life); }

static void recycle(struct state *st, fireshell *fs,int x,int y)
{
  unsigned int n,pixlife;
  firepix *fp = fs->fpix;
  fs->vgn = st->shoot;
  fs->shy = y;
  fs->cx = x;
  fs->cy = st->shoot ? st->height : y ;
  fs->color = (rnd(155)+100) <<16 |
              (rnd(155)+100) <<8  |
               rnd(255);
  fs->life = rnd(st->rndlife)+st->minlife;
  fs->air  = 1-(float)(rnd(200))/10000;
  fs->flash   = rnd(30000)+25000;       /*  million jouls...              */
  fs->flash   = fs->flash*fs->flash;
  fs->cshift  = !rnd(5) ? 120:0; 
  fs->special = !rnd(10) ? 1:0; 
  if(st->verbose)
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

static void glow(struct state *st)
{
  unsigned int n,q;
  unsigned int w = st->width;
  unsigned int h = st->height;
  unsigned char *pa, *pb, *pm, *po;
  if (!st->xim) return;
  pm = st->palaka1;
  po = st->palaka2;
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
  pm[n] = q>>4;
  q     = q>>3;
  po[n] = q>255 ? 255 : q;}
  pm+=n; po+=n;
  for(n=0;n<w*4;n++)
  {pm[n]=0; po[n]=0;}}   /* clean last line */

static void blur(struct state *st)
{
  unsigned int n,q;
  unsigned int w = st->width;
  unsigned int h = st->height;
  unsigned char *pa, *pb, *pm;
  pm = st->palaka1;
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
  pm = st->palaka1; /* first line */
  for(n=0;n<w*4+4;n++) pm[n]=pm[n+w*4]; }

static void light_2x2(struct state *st, fireshell *fss)
{
  unsigned int l,t,x,y;
  float s;
  int w = st->width;
  int h = st->height;
  unsigned char *dim = st->palaka2;
  unsigned char *sim = st->palaka1;
  int nl = w*4;
  fireshell *f;
  if(st->glow_on) sim=dim;
  for(y=0;y<h;y+=2){
  for(x=0;x<w;x+=2){
  f = fss;

/* Note: The follwing loop is unrolled for speed.
         check this before changing the value of SHELLCOUNT
  s = 0;
  for(n=SHELLCOUNT;n;n--,f++){
  s += f->flash/(1+(f->cx - x)*(f->cx - x)+(f->cy - y)*(f->cy - y));} */

  s  = f->flash/(1+(f->cx - x)*(f->cx - x)+(f->cy - y)*(f->cy - y));
  f++;
  s += f->flash/(1+(f->cx - x)*(f->cx - x)+(f->cy - y)*(f->cy - y));
  f++;
  s += f->flash/(1+(f->cx - x)*(f->cx - x)+(f->cy - y)*(f->cy - y));

  l = sqrtf(s);

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

static void resize(struct state *st)
{
  XWindowAttributes xwa;
  XGetWindowAttributes (st->dpy, st->window, &xwa);
  xwa.width  -= xwa.width % 2;
  xwa.height -= xwa.height % 2;
  if(xwa.height != st->height || xwa.width != st->width) {
  st->width  = xwa.width;
  st->height = xwa.height;
  if (st->verbose)
  printf("sky size: %dx%d ------------------------------\n",st->width,st->height);
  if (st->xim) {
  if (st->xim->data==(char *)st->palaka2) st->xim->data=NULL;  
  XDestroyImage(st->xim);
  if (st->palaka2!=st->palaka1) free(st->palaka2 - 8);
  free(st->palaka1 - 8); 
  }
  st->palaka1 = NULL;
  st->palaka2 = NULL;
  st->xim = XCreateImage(st->dpy, xwa.visual, xwa.depth, ZPixmap, 0, 0,
		     st->width, st->height, 8, 0);
  if (!st->xim) return;
  st->palaka1 = (unsigned char *) calloc(st->xim->height+1,st->xim->width*4) + 8;
  if(st->flash_on|st->glow_on)
  st->palaka2 = (unsigned char *) calloc(st->xim->height+1,st->xim->width*4) + 8;
  else
  st->palaka2 = st->palaka1;
  if (st->depth>=24)
  st->xim->data = (char *)st->palaka2;
  else
  st->xim->data = calloc(st->xim->height,st->xim->bytes_per_line); }}

static void put_image(struct state *st)
{
  int x,y,i,j;
  unsigned char r, g, b;
  if (!st->xim) return;
  i = 0;
  j = 0;
  if (st->depth==16) {
     if(st->bigendian)
     for (y=0;y<st->xim->height; y++)
     for (x=0;x<st->xim->width; x++) {
     r = st->palaka2[j++];
     g = st->palaka2[j++];
     b = st->palaka2[j++];
     j++;
     st->xim->data[i++] = (g&224)>>5 | (r&248);
     st->xim->data[i++] = (b&248)>>3 | (g&28)<<3;
     }
     else
     for (y=0;y<st->xim->height; y++)
     for (x=0;x<st->xim->width; x++) {
     r = st->palaka2[j++];
     g = st->palaka2[j++];
     b = st->palaka2[j++];
     j++;
     st->xim->data[i++] = (b&248)>>3 | (g&28)<<3;
     st->xim->data[i++] = (g&224)>>5 | (r&248);
     }
  }
  if (st->depth==15) {
     if(st->bigendian)
     for (y=0;y<st->xim->height; y++)
     for (x=0;x<st->xim->width; x++) {
     r = st->palaka2[j++];
     g = st->palaka2[j++];
     b = st->palaka2[j++];
     j++;
     st->xim->data[i++] = (g&192)>>6 | (r&248)>>1;
     st->xim->data[i++] = (b&248)>>3 | (g&56)<<2;
     }
     else
     for (y=0;y<st->xim->height; y++)
     for (x=0;x<st->xim->width; x++) {
     r = st->palaka2[j++];
     g = st->palaka2[j++];
     b = st->palaka2[j++];
     j++;
     st->xim->data[i++] = (b&248)>>3 | (g&56)<<2;
     st->xim->data[i++] = (g&192)>>6 | (r&248)>>1;
     }
  }
  if (st->depth==8) {
     for (y=0;y<st->xim->height; y++)
     for (x=0;x<st->xim->width; x++) {
     r = st->palaka2[j++];
     g = st->palaka2[j++];
     b = st->palaka2[j++];
     j++;     
     st->xim->data[i++] = (((7*g)/256)*36)+(((6*r)/256)*6)+((6*b)/256);
     }
  }
  XPutImage(st->dpy,st->window,st->gc,st->xim,0,0,0,0,st->xim->width,st->xim->height); }


static void *
fireworkx_init (Display *dpy, Window win)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  unsigned int n;
  Visual *vi;
  Colormap cmap;
  Bool writable;
  XWindowAttributes xwa;
  XGCValues gcv;
  firepix *fpix, *ffpix;

  st->dpy = dpy;
  st->window = win;

  st->glow_on  = get_boolean_resource(st->dpy, "glow"    , "Boolean");
  st->flash_on = get_boolean_resource(st->dpy, "flash"   , "Boolean");
  st->shoot    = get_boolean_resource(st->dpy, "shoot"   , "Boolean");
  st->verbose  = get_boolean_resource(st->dpy, "verbose" , "Boolean");
  st->rndlife  = get_integer_resource(st->dpy, "maxlife" , "Integer");
  st->delay    = get_integer_resource(st->dpy, "delay"   , "Integer");
  st->minlife  = st->rndlife/4;
  st->flash_fade=0.98;
  if(st->rndlife < 1000) st->flash_fade=0.96;
  if(st->rndlife <  500) st->flash_fade=0.94;
  if(st->verbose){
  printf("Fireworkx %s - pyrotechnics simulation program \n", FWXVERSION);
  printf("Copyright (c) 1999-2007 Rony B Chandran <ronybc@asia.com> \n\n");
  printf("url: http://www.ronybc.8k.com \n\n");}

  XGetWindowAttributes(st->dpy,win,&xwa);
  st->depth = xwa.depth;
  vi        = xwa.visual;
  cmap      = xwa.colormap;
  st->bigendian = (ImageByteOrder(st->dpy) == MSBFirst);
 
  if(st->depth==8){
  if(st->verbose){
  printf("Pseudocolor color: use '-no-flash' for better results.\n");}
  st->colors = (XColor *) calloc(sizeof(XColor),st->ncolors+1);
  writable = False;
  make_smooth_colormap(st->dpy, vi, cmap, st->colors, &st->ncolors,
                                False, &writable, True);
  }
  st->gc = XCreateGC(st->dpy, win, 0, &gcv);

  resize(st);   /* initialize palakas */ 
  
  ffpix = malloc(sizeof(firepix) * PIXCOUNT * SHELLCOUNT);
  st->ffshell = malloc(sizeof(fireshell) * SHELLCOUNT);
  st->fshell = st->ffshell;
  fpix = ffpix;
  for (n=0;n<SHELLCOUNT;n++){
  st->fshell->fpix = fpix;
  recycle (st, st->fshell,rnd(st->width),rnd(st->height));
  st->fshell++; 
  fpix += PIXCOUNT; }
  
  return st;
}


static unsigned long
fireworkx_draw (Display *dpy, Window win, void *closure)
{
  struct state *st = (struct state *) closure;
  int q,n;
  for(q=FTWEAK;q;q--){
    st->fshell=st->ffshell;
    for(n=SHELLCOUNT;n;n--){
      if (!explode(st, st->fshell)){
        recycle(st, st->fshell,rnd(st->width),rnd(st->height)); }
      st->fshell++; }}
#if HAVE_X86_MMX
  if(st->glow_on) mmx_glow(st->palaka1,st->width,st->height,8,st->palaka2);
#else
  if(st->glow_on) glow(st);
#endif
  if(st->flash_on) light_2x2(st, st->ffshell);
  put_image(st);
#if HAVE_X86_MMX
  if(!st->glow_on) mmx_blur(st->palaka1,st->width,st->height,8);
#else
  if(!st->glow_on) blur(st);
#endif

  return st->delay;
}


static void
fireworkx_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  resize(st);
}

static Bool
fireworkx_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->type == ButtonPress) {
    recycle(st, st->fshell, event->xbutton.x, event->xbutton.y);
    return True;
  }
  return False;
}

static void
fireworkx_free (Display *dpy, Window window, void *closure)
{
}


static const char *fireworkx_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*delay:	20000",  /* never default to zero! */
  "*maxlife:	2000",
  "*flash:	True",
  "*glow:	True",
  "*shoot:	False",
  "*verbose:	False",
  0
};

static XrmOptionDescRec fireworkx_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-maxlife",		".maxlife",	XrmoptionSepArg, 0 },
  { "-no-flash",	".flash",	XrmoptionNoArg, "False" },
  { "-no-glow",		".glow",	XrmoptionNoArg, "False" },
  { "-shoot",		".shoot",	XrmoptionNoArg, "True" },
  { "-verbose",		".verbose",	XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Fireworkx", fireworkx)
