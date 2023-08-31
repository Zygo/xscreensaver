/*
 * Copyright (c) 2004-2009 Steve Sundstrom
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include "screenhack.h"
#include "colors.h"
#include "hsv.h"
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
/*#include <sys/utsname.h>*/

#define MODE_CREATE		0   /* init, create, then finish sleep */
#define MODE_ERASE		1   /* erase, then reset colors */
#define MODE_DRAW		2

#define DIR_NONE		0
#define DIR_UP			1
#define DIR_DOWN		2
#define DIR_LEFT		3
#define DIR_RIGHT		4

#define LINE_FORCE		1
#define LINE_NEW		2
#define LINE_BRIN		3
#define LINE_BROUT		4

#define PT_UL			0
#define PT_MP			1
#define PT_LR			2
#define PT_NL			3

#define D3D_NONE		0
#define D3D_BLOCK		1
#define D3D_NEON		2
#define D3D_TILED		3

#define TILE_RANDOM		0
#define TILE_FLAT		1
#define TILE_THIN		2
#define TILE_OUTLINE		3
#define TILE_BLOCK		4
#define TILE_NEON		5
#define TILE_TILED		6

#define BASECOLORS		30
#define SHADES			12
#define MAXCOLORS		40
#define LAYERS			4
#define PATTERNS		40
#define SHAPES			18
#define DRAWORDERS		40
#define COLORMAPS		20
#define WAVES 			6
#define STRETCHES		8

struct lineStruct {
    unsigned int x, y, len, obj, color, ndol;
    int deo;
    Bool hv;
};

struct gridStruct {
    unsigned int line, hl, hr, vu, vd, dhl, dhr, dvu, dvd;
};

/* basically the same as global variables, but used to keep them in a bucket
   and pass them around easier like the original C++ implementation */
struct state {
  /* window values */
  Display *display;
  Window window;
  XWindowAttributes xgwa;
  GC fgc, bgc;
  XColor colors[255];

  /* memory values */
  struct lineStruct *dline, *eline;
  struct gridStruct *grid;
  unsigned int *zlist, *fdol;
  Bool *odi;
    /* draw, erase, fill, init, line, object, z indexes */
  unsigned int di, ei, fi, ii, bi, li, eli, oi, zi;
  /* size variables */
  int gridx, gridy;                     /* grid size */
  unsigned int gridn;
  int lwid, bwid, swid;/* line width, background width, shadow width */
  int narray, max_wxh;
  int elwid, elpu, egridx, egridy; /* for now */
  /* fill variables */
  int bnratio;                 /* ratio of branch lines to new lines */
  int maxlen;                              /* maximum length of line */
  int forcemax;                  /* make line be max possible length */
  int olen;                           /* open length set by findopen */
  int bln;          /* blocking line number set by findopen, -1=edge */
  /* color variables */
  int ncolors;                        /* number of colors for screen */
  int shades;
  int rco[MAXCOLORS];           /* random ordering of colors for deo */
  int cmap;
  int layers;
  Bool newcols;      /* can we create new colormaps with each screen */
  /* draw variables */
  int dmap, emap;  /* pattern by which line draw order is determined */
  int dvar, evar;             /* random number added to .deo to vary */
  int ddir, edir;      /* draw/erase in forward direction or reverse */
  int lpu;            /* lines drawn per update used to adjust speed */
  int d3d;
  int round;
  int outline;
  /* layered draw variables */
  int pattern[LAYERS], shape[LAYERS], mix[LAYERS];
  int csw[LAYERS], wsx[LAYERS], wsy[LAYERS], sec[LAYERS];
  int cs1[LAYERS], cs2[LAYERS], cs3[LAYERS]; int cs4[LAYERS];
  int wave[LAYERS], waveh[LAYERS], wavel[LAYERS];
  int rx1[LAYERS], rx2[LAYERS], rx3[LAYERS];
  int ry1[LAYERS], ry2[LAYERS], ry3[LAYERS];
  /* misc variables */
  int mode, sleep, speed, tile, dialog;
  Bool grid_full, resized;
  struct timeval time;
};

static int
_min(int a, int b)
{
  if (a<=b)
    return(a);
  return(b);
}

static int
_max(int a, int b)
{
  if (a>=b)
    return(a);
  return(b);
}

static int
_dist(struct state *st, int x1, int x2, int y1, int y2, int s)
{
  double xd=x1-x2;
  double yd=y1-y2;
  switch(s) {
    case 0:
      return((int)sqrt(xd*xd+yd*yd));
    case 1:
      return((int)sqrt(xd*xd*st->cs1[0]*2+yd*yd));
    case 2:
      return((int)sqrt(xd*xd+yd*yd*st->cs2[0]*2));
    default:
      return((int)sqrt(xd*xd*st->cs1[0]/st->cs2[0]+yd*yd*st->cs3[0]/st->cs4[0]));
  }
}

static int
_wave(struct state *st, int x, int h, int l, int wave)
{
  l+=1;
  switch(wave) {
    case 0:                                         /* cos wave*/
      return((int)(cos((double)x*M_PI/l)*h));
    case 1:                                      /* double wave*/
    case 2:                                      /* double wave*/
      return((int)(cos((double)x*M_PI/l)*h)+(int)(sin((double)x*M_PI/l/st->cs1[1])*h));
    case 3:                                         /* zig zag */
      return(abs((x%(l*2)-l))*h/l);
    case 4:                                   /* giant zig zag */
      return(abs((x%(l*4)-l*2))*h*3/l);
    case 5:                                        /* sawtooth */
      return((x%(l))*h/l);
    default:				  	    /* no wave */
      return(0);
  }
}

static int
_triangle(struct state *st, int x, int y, int rx, int ry, int t)
{
  switch(t) {
    case 1:
      return(_min(_min(x+y+rx-(st->gridx/2),st->gridx-x+y),(st->gridy-y+(ry/2))*3/2));
    case 2:
      return(_min(_min(x-rx,y-ry),(rx+ry-x-y)*2/3));
    case 3:
      return(_min(_min(st->gridx-x-rx,y-ry),(rx+ry-st->gridx+x-y)*2/3));
    case 4:
      return(_min(_min(x-rx,st->gridy-y-ry),(rx+ry-x-st->gridy+y)*2/3));
  }
  return(_min(_min(st->gridx-x-rx,st->gridy-y-ry),(rx+ry-st->gridx+x-st->gridy+y)*2/3));
}

static void
_init_zlist(struct state *st)
{
  unsigned int tmp, y, z;

  st->gridx=st->xgwa.width/st->lwid;
  st->gridy=st->xgwa.height/st->lwid;
  if ((st->gridx <= 0) || (st->gridy <= 0)) abort();
  st->gridn=st->gridx*st->gridy;
  /* clear grid */
  for (z=0; z<st->gridn; z++) {
    st->grid[z].line=st->grid[z].hl=st->grid[z].hr=st->grid[z].vu=st->grid[z].vd=st->grid[z].dhl=st->grid[z].dhr=st->grid[z].dvu=st->grid[z].dvd=0;
    st->zlist[z]=z;
  }
  /* rather than pull x,y points randomly and wait to hit final empy cells a
     list of all points is created and mixed so empty cells do get hit last */
  for (z=0; z<st->gridn; z++) {
    y=random()%st->gridn;
    tmp=st->zlist[y];
    st->zlist[y]=st->zlist[z];
    st->zlist[z]=tmp;
  }
}

static void
make_color_ramp_rgb (Screen *screen, Visual *visual, Colormap cmap,
    int r1, int g1, int b1,  int r2, int g2, int b2,
    XColor *colors, int *ncolorsP, Bool closed_p)
{
    int h1, h2;
    double s1, s2, v1, v2;
    rgb_to_hsv(r1, g1, b1, &h1, &s1, &v1);
    rgb_to_hsv(r2, g2, b2, &h2, &s2, &v2);
    make_color_ramp(screen, visual, cmap, h1, s1, v1, h2, s2, v2,
        colors, ncolorsP, False, True, 0);
}


static void
_init_colors(struct state *st)
{
  int col[BASECOLORS];
  int c1, c2, c3, h1, h2, h3;
  int r1, g1, b1, r2, g2, b2, r3, g3, b3;
  double s1, s2, s3, v1, v2, v3;
  XColor tmp_col1[16], tmp_col2[16], tmp_col3[16];

  unsigned short basecol[BASECOLORS][3]={
    /* 0  dgray */    {0x3333,0x3333,0x3333},
    /* 1  dbrown */   {0x6666,0x3333,0x0000},
    /* 2  dred */     {0x9999,0x0000,0x0000},
    /* 3  orange */   {0xFFFF,0x6666,0x0000},
    /* 4  gold */     {0xFFFF,0xCCCC,0x0000},
    /* 5  olive */    {0x6666,0x6666,0x0000},
    /* 6  ivy */      {0x0000,0x6666,0x0000},
    /* 7  dgreen */   {0x0000,0x9999,0x0000},
    /* 8  bluegray */ {0x3333,0x6666,0x6666},
    /* 9  dblue */    {0x0000,0x0000,0x9999},
    /* 10 blue */     {0x3333,0x3333,0xFFFF},
    /* 11 dpurple */  {0x6666,0x0000,0xCCCC},
    /* 12 purple */   {0x6666,0x3333,0xFFFF},
    /* 13 violet */   {0x9999,0x3333,0x9999},
    /* 14 magenta */  {0xCCCC,0x3333,0xCCCC},
    /* lights */
    /* 15 gray */     {0x3333,0x3333,0x3333},
    /* 16 brown */    {0x9999,0x6666,0x3333},
    /* 17 tan */      {0xCCCC,0x9999,0x3333},
    /* 18 red */      {0xFFFF,0x0000,0x0000},
    /* 19 lorange */  {0xFFFF,0x9999,0x0000},
    /* 20 yellow */   {0xFFFF,0xFFFF,0x0000},
    /* 21 lolive */   {0x9999,0x9999,0x0000},
    /* 22 green */    {0x3333,0xCCCC,0x0000},
    /* 23 lgreen */   {0x3333,0xFFFF,0x3333},
    /* 24 cyan */     {0x0000,0xCCCC,0xCCCC},
    /* 25 sky */      {0x3333,0xFFFF,0xFFFF},
    /* 26 marine */   {0x3333,0x6666,0xFFFF},
    /* 27 lblue */    {0x3333,0xCCCC,0xFFFF},
    /* 28 lpurple */  {0x9999,0x9999,0xFFFF},
    /* 29 pink */     {0xFFFF,0x9999,0xFFFF}};

  if (st->d3d) {
    st->shades = (st->d3d==D3D_TILED) ? 5 : st->lwid/2+1;
    st->ncolors=4+random()%4;
    if (st->cmap>0) {                      /* tint the basecolors a bit */
      for (c1=0; c1<BASECOLORS; c1++)
        for (c2=0; c2<2; c2++)
	  if (!basecol[c1][c2]) {
            basecol[c1][c2]+=random()%16000;
	  } else if (basecol[c1][c2]==0xFFFF) {
            basecol[c1][c2]-=random()%16000;
          } else {
            basecol[c1][c2]-=8000;
            basecol[c1][c2]+=random()%16000;
          }
    }
    switch(st->cmap%4) {
      case 0:                                            /* all */
        for (c1=0; c1<st->ncolors; c1++)
          col[c1]=random()%BASECOLORS;
        break;
      case 1:                                          /* darks */
        for (c1=0; c1<st->ncolors; c1++)
          col[c1]=random()%15;
        break;
      case 2:                                   /* semi consecutive darks */
        col[0]=random()%15;
        for (c1=1; c1<st->ncolors; c1++)
          col[c1]=(col[c1-1]+1+random()%2)%15;
        break;
      case 3:                                   /* consecutive darks */
        col[0]=random()%(15-st->ncolors);
        for (c1=1; c1<st->ncolors; c1++)
          col[c1]=col[c1-1]+1;
        break;
    }
    for (c1=0; c1<st->ncolors; c1++) {
      /* adjust colors already set */
      for (h1=c1*st->shades-1; h1>=0; h1--)
        st->colors[h1+st->shades]=st->colors[h1];
      make_color_ramp_rgb(st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
        basecol[col[c1]][0], basecol[col[c1]][1], basecol[col[c1]][2],
        0xFFFF, 0xFFFF, 0xFFFF, st->colors, &st->shades,
        False);
    }
    return;
  }
  /* not 3d */
  st->shades=1;
  if (st->cmap%2) {                                  /* basecolors */
    if (random()%3) {
      c1=random()%15;
      c2=(c1+3+(random()%5))%15;
      c3=(c2+3+(random()%5))%15;
    } else {
      c1=random()%BASECOLORS;
      c2=(c1+5+(random()%10))%BASECOLORS;
      c3=(c2+5+(random()%10))%BASECOLORS;
    }
    r1=basecol[c1][0];
    g1=basecol[c1][1];
    b1=basecol[c1][2];
    r2=basecol[c2][0];
    g2=basecol[c2][1];
    b2=basecol[c2][2];
    r3=basecol[c3][0];
    g3=basecol[c3][1];
    b3=basecol[c3][2];
  } else {                                             /* random rgb's */
    r1=random()%65535;
    g1=random()%65535;
    b1=random()%65535;
    r2=(r1+16384+random()%32768)%65535;
    g2=(g1+16384+random()%32768)%65535;
    b2=(b1+16384+random()%32768)%65535;
    r3=(r2+16384+random()%32768)%65535;
    g3=(g2+16384+random()%32768)%65535;
    b3=(b2+16384+random()%32768)%65535;
 }
 switch(st->cmap) {
    case 0:                           /* make_color_ramp color->color */
    case 1:
    case 2:                           /* make_color_ramp color->white */
    case 3:
      st->ncolors=5+random()%5;
      if (st->cmap>1)
        r2=g2=b2=0xFFFF;
      make_color_ramp_rgb(st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
        r1, g1, b1, r2, g2, b2,
        st->colors, &st->ncolors, random()%2);
      break;
    case 4:                                /* 3 color make_color_loop */
    case 5:
    case 6:
    case 7:
      st->ncolors=8+random()%12;
      rgb_to_hsv(r1, g1, b1, &h1, &s1, &v1);
      rgb_to_hsv(r2, g2, b2, &h2, &s2, &v2);
      rgb_to_hsv(r3, g3, b3, &h3, &s3, &v3);

      make_color_loop(st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
        h1, s1, v1, h2, s2, v2, h3, s3, v3,
        st->colors, &st->ncolors, True, False);
      break;
    case 8:                                            /* random smooth */
    case 9:
      st->ncolors=(random()%4)*6+12;
      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual,
        st->xgwa.colormap, st->colors, &st->ncolors,
        True, False, True);
      break;
    case 10:                                                 /* rainbow */
      st->ncolors=(random()%4)*6+12;
      make_uniform_colormap (st->xgwa.screen, st->xgwa.visual,
        st->xgwa.colormap, st->colors, &st->ncolors,
        True, False, True);
      break;
    case 11:                                     /* dark to light blend */
    case 12:
    case 13:
    case 14:
      st->ncolors=7;
      make_color_ramp_rgb(st->xgwa.screen, st->xgwa.visual,  st->xgwa.colormap,
        r1, g1, b1, 0xFFFF, 0xFFFF, 0xFFFF,
        tmp_col1, &st->ncolors, False);
      make_color_ramp_rgb(st->xgwa.screen, st->xgwa.visual,  st->xgwa.colormap,
        r2, g2, b2, 0xFFFF, 0xFFFF, 0xFFFF,
        tmp_col2, &st->ncolors, False);
      if (st->cmap<13) {
        for(c1=0; c1<=4; c1++) {
           st->colors[c1*2]=tmp_col1[c1];
           st->colors[c1*2+1]=tmp_col2[c1];
        }
        st->ncolors=10;
      } else {
        make_color_ramp_rgb(st->xgwa.screen, st->xgwa.visual,  st->xgwa.colormap,
          r3, g3, b3, 0xFFFF, 0xFFFF, 0xFFFF,
          tmp_col3, &st->ncolors, False);
        for(c1=0; c1<=4; c1++) {
           st->colors[c1*3]=tmp_col1[c1];
           st->colors[c1*3+1]=tmp_col2[c1];
           st->colors[c1*3+2]=tmp_col3[c1];
        }
        st->ncolors=15;
      }
      break;
    default:                                                  /* random */
      st->ncolors=(random()%4)*6+12;
      make_random_colormap (st->xgwa.screen, st->xgwa.visual,
        st->xgwa.colormap, st->colors, &st->ncolors,
        False, True, False, True);
      break;
  }

  /* set random color order for drawing and erasing */
  for (c1=0; c1<MAXCOLORS; c1++)
    st->rco[c1]=c1;
  for (c1=0; c1<MAXCOLORS; c1++) {
    c3=random()%MAXCOLORS;
    c2=st->rco[c1];
    st->rco[c1]=st->rco[c3];
    st->rco[c3]=c2;
  }
}

static int _comparedeo(const void *i, const void *j)
{
        struct lineStruct *h1, *h2;

        h1=(struct lineStruct *)i;
        h2=(struct lineStruct *)j;
        if (h1->deo > h2->deo)
                return(1);
        if (h1->deo < h2->deo)
                return(-1);
        return(0);
}

static int
_hv(struct state *st, int x, int y, int d1, int d2, int pn, Bool de)
{
  int v1, v2, r;

  switch (d1) {
    case 0:
      v1 = (de) ? st->egridx-x : st->gridx-x;
      break;
    case 1:
      v1 = y;
      break;
    case 2:
      v1 = x;
      break;
    default:
      v1 = (de) ? st->egridy-y : st->gridy-y;
      break;
  }
  switch (d2) {
    case 0:
      v2 = (de) ? st->egridx-x : st->gridx-x;
      break;
    case 1:
      v2 = y;
      break;
    case 2:
      v2 = x;
      break;
    default:
      v2 = (de) ? st->egridy-y : st->gridy-y;
      break;
  }
  r = (de) ? (st->dline[st->li].hv) ? (v1+10000)*pn : (v2+10000)*-pn :
    (st->eline[st->li].hv) ? (v1+10000)*pn : (v2+10000)*-pn;
  return(r);
}
	
static int
_getdeo(struct state *st, int x, int y, int map, int de)
{
  int cr;
  switch(map) {
    case 0:                                        /* horizontal one side */
      return(x);
    case 1:                                          /* vertical one side */
      return(y);
    case 2:                                        /* horizontal two side */
      return(_min(x,st->gridx-x)+1);
    case 3:                                          /* vertical two side */
      return(_min(y,st->gridy-y)+1);
    case 4:                                                     /* square */
      return(_max(abs(x-st->rx3[de]),abs(y-st->ry3[de]))+1);
    case 5:                                                /* two squares */
      return(_min(_max(abs(x-(st->rx3[de]/2)),abs(y-st->ry3[de])),_max(abs(x-(st->gridx-(st->rx2[de]/2))),abs(y-st->ry2[de])))+1);
    case 6:                                       /* horizontal rectangle */
      return(_max(abs(x-st->rx3[de]),abs(y-(st->ry3[de]))*st->cs1[de])+1);
    case 7:                                         /* vertical rectangle */
      return(_max(abs(x-st->rx3[de])*st->cs1[de],abs(y-(st->ry3[de])))+1);
    case 8:                                                    /* + cross */
      return(_min(abs(x-st->rx3[de]),abs(y-(st->ry3[de])))+1);
    case 9:                                                   /* diagonal */
      return((x*3/4+y)+1);
    case 10:                                         /* opposite diagonal */
      return((x*3/4+st->gridy-y)+1);
    case 11:                                                   /* diamond */
      return((abs(x-st->rx3[de])+abs(y-st->ry3[de]))/2+1);
    case 12:                                              /* two diamonds */
      return(_min(abs(x-(st->rx3[de]/2))+abs(y-st->ry3[de]),abs(x-(st->gridx-(st->rx2[de]/2)))+abs(y-st->ry2[de]))/2+1);
    case 13:                                                    /* circle */
      return(_dist(st,x,st->rx3[de],y,st->ry3[de],0)+1);
    case 14:                                        /* horizontal ellipse */
      return(_dist(st,x,st->rx3[de],y,st->ry3[de],1)+1);
    case 15:                                          /* vertical ellipse */
      return(_dist(st,x,st->rx3[de],y,st->ry3[de],2)+1);
    case 16:                                               /* two circles */
      return(_min(_dist(st,x,st->rx3[de]/2,y,st->ry3[de],0),_dist(st,x,st->gridx-(st->rx2[de]/2),y,st->ry2[de],0))+1);
    case 17:                                  /* horizontal straight wave */
      return(x+_wave(st,st->gridy+y,st->csw[0]*st->cs1[0],st->csw[0]*st->cs2[0],st->wave[de]));
    case 18:                                    /* vertical straight wave */
      return(y+_wave(st,st->gridx+x,st->csw[0]*st->cs1[0],st->csw[0]*st->cs2[0],st->wave[de]));
    case 19:                                     /* horizontal wavey wave */
      return(x+_wave(st,st->gridy+y+((x/5)*st->edir),st->csw[de]*st->cs1[de],st->csw[de]*st->cs2[de],st->wave[de])+1);
    case 20:                                       /* vertical wavey wave */
      return(y+_wave(st,st->gridx+x+((y/5)*st->edir),st->csw[de]*st->cs1[de],st->csw[de]*st->cs2[de],st->wave[de])+1);
/* no d3d for 21,22 */
    case 21:                                  /* simultaneous directional */
      return(_hv(st,x,y,st->cs1[0]%2,st->cs2[0]%2,1,de));
    case 22:                                       /* reverse directional */
      return(_hv(st,x,y,st->cs1[0]%2,st->cs2[0]%2,-1,de));
    case 23:                                                    /* length */
      if (de)
        return(st->dline[st->li].len*1000+random()%5000);
      else
        return(st->eline[st->li].len*1000+random()%5000);
    case 24:                                                    /* object */
    case 25:
    case 26:
    case 27:
      if (de)
        return(st->dline[st->li].obj*100);
      else
        return(st->eline[st->li].obj*100);
    default:                                                     /* color */
      cr = (de) ? st->dline[st->li].color : st->eline[st->li].color;
      if (map<34) cr=st->rco[cr];
      if ((map%6<4) || (de))  {                               /* by color */
        cr*=1000;
        cr+=random()%1000;
      } else if (map%6==4) {                      /* by color horizontaly */
        cr*=st->gridx;
        cr+=(x+random()%(st->gridx/2));
      } else {                                     /* by color vertically */
        cr*=st->gridy;
        cr+=(y+random()%(st->gridy/2));
      }
      return(cr);
  }
  return(1);
}

static void
_init_screen(struct state *st)
{
  int nstr, x;
  struct lineStruct *tmp;

  /* malloc memory in case of resize */
  if (st->resized) {
    st->max_wxh=st->xgwa.width*st->xgwa.height;
    if (st->dline!=NULL)
      free(st->dline);
    if (st->eline!=NULL)
      free(st->eline);
    if (st->grid!=NULL)
      free(st->grid);
    if (st->zlist!=NULL)
      free(st->zlist);
    if (st->fdol!=NULL)
      free(st->fdol);
    if (st->odi!=NULL)
      free(st->odi);
    st->narray = (st->xgwa.width+1)*(st->xgwa.height+1)/4+1;
    st->dline = calloc(st->narray, sizeof(struct lineStruct));
    st->eline = calloc(st->narray, sizeof(struct lineStruct));
    st->grid = calloc(st->narray, sizeof(struct gridStruct));
    st->zlist = calloc(st->narray, sizeof(unsigned int));
    st->fdol = calloc(st->narray, sizeof(unsigned int));
    st->odi = calloc(st->narray, sizeof(Bool));
    if ((st->dline == NULL) || (st->eline == NULL) ||
      (st->grid == NULL) || (st->zlist == NULL) ||
      (st->fdol == NULL) || (st->odi == NULL)) {
      fprintf(stderr, "not enough memory\n");
      exit(1);
    }
    st->dialog = (st->xgwa.width<500) ? 1 : 0;
    st->resized=False;
  }
  if (st->ii) {
    /* swap st->dline and st->eline pointers to resort and erase */
    tmp=st->eline;
    st->eline=st->dline;
    st->dline=tmp;
    st->eli=st->li;
    st->elwid=st->lwid;
    st->elpu=st->lpu;
    st->egridx=st->gridx;
    st->egridy=st->gridy;

    /* create new erase order */
    for (st->li=1; st->li<=st->eli; st->li++)
      st->eline[st->li].deo=(_getdeo(st,st->eline[st->li].x,st->eline[st->li].y,st->emap,0) + (random()%st->evar) + (random()%st->evar))*st->edir;
    qsort(st->eline, st->eli+1, sizeof(struct lineStruct), _comparedeo);
  }
  st->ii++;

  /* clear arrays and other counters */
  st->di=st->ei=st->fi=st->li=st->oi=st->zi=0;
  st->grid_full=False;
  /* li starts from 1 */
  st->dline[0].x=st->dline[0].y=st->dline[0].len=0;
  /* to keep it first after sorting so di is never null */
  st->dline[0].deo=-999999999;

  /* set random screen variables */
  st->lwid = (st->ii==1) ? 3 : 2+((random()%6)%4);
  st->d3d = ((st->tile==TILE_FLAT) || (st->tile==TILE_THIN) ||
    (st->tile==TILE_OUTLINE)) ? D3D_NONE :
    (st->tile==TILE_BLOCK) ? D3D_BLOCK :
    (st->tile==TILE_NEON) ? D3D_NEON :
    (st->tile==TILE_TILED) ? D3D_TILED :
    /* force TILE_D3D on first screen to properly load all shades */
    ((st->ii==1) && (!st->newcols)) ? D3D_TILED : (random()%5)%4;
/* st->d3d=D3D_BLOCK; st->lwid=2; */
  st->outline = (st->tile==TILE_OUTLINE) ? 1 :
     ((st->tile!=TILE_RANDOM) || (random()%5)) ? 0 : 1;
  st->round = (st->d3d==D3D_NEON) ? 1 :
    ((st->d3d==D3D_BLOCK) || (st->outline) || (random()%6)) ? 0 : 1;
  if ((st->d3d) || (st->outline) || (st->round))
    st->lwid+=2;
  if ((!st->d3d) && (!st->round) && (!st->outline) && (st->lwid>3))
    st->lwid-=2;
  if (st->d3d==D3D_TILED)
    st->lwid++;
  if (st->tile==TILE_THIN)
    st->lwid=2;

  if (st->xgwa.width > 2560 || st->xgwa.height > 2560)
    st->lwid *= 3;  /* Retina displays */

  _init_zlist(st);

  st->maxlen=(st->lwid>6) ? 2+(random()%4) :
                (st->lwid>4) ? 2+(random()%8)%6 :
                (st->lwid>2) ? 2+(random()%12)%8 : 2+(random()%15)%10;
  st->bnratio = 4+(random()%4)+(random()%4);
  st->forcemax = (random()%6) ? 0 : 1;

  if ((st->ii==1) || (st->newcols))
    _init_colors(st);

  st->dmap = (st->emap+5+(random()%5))%DRAWORDERS;

  st->dmap=20+random()%20;

  st->dvar = (st->dmap>22) ? 100 : 10+(st->csw[0]*(random()%5));
  st->ddir= (random()%2) ? 1 : -1;

  st->emap = (st->dmap+10+(random()%10))%20;
  st->evar = (st->emap>22) ? 100 : 10+(st->csw[0]*(random()%5));
  st->edir= (random()%2) ? 1 : -1;

  st->layers= (random()%2) ? 2 : (random()%2) ? 1 : (random()%2) ? 3 : 4;
  st->cmap=(st->cmap+5+(random()%10))%COLORMAPS;

  for (x=0; x<LAYERS; x++) {
    st->pattern[x]=random()%PATTERNS;
    st->shape[x]=random()%SHAPES;
    st->mix[x]=random()%20;
    nstr = (st->lwid==2) ? 20+random()%12 :
      (st->lwid==3) ? 16+random()%8 :
      (st->lwid==4) ? 12+random()%6 :
      (st->lwid==5) ? 10+random()%5 :
      (st->lwid==6) ? 8+random()%4 :
        5+random()%5;
    st->csw[x] = _max(5,st->gridy/nstr);
    st->wsx[x] = (st->wsx[x]+3+(random()%3))%STRETCHES;
    st->wsy[x] = (st->wsy[x]+3+(random()%3))%STRETCHES;
    st->sec[x] = random()%5;
    if ((!st->dialog) && (st->sec[x]<2)) st->csw[x]/=2;
    st->cs1[x] = (st->dialog) ? 1+random()%3 : 2+random()%5;
    st->cs2[x] = (st->dialog) ? 1+random()%3 : 2+random()%5;
    st->cs3[x] = (st->dialog) ? 1+random()%3 : 2+random()%5;
    st->cs4[x] = (st->dialog) ? 1+random()%3 : 2+random()%5;
    st->wave[x]=random()%WAVES;
    st->wavel[x]=st->csw[x]*(2+random()%6);
    st->waveh[x]=st->csw[x]*(1+random()%3);
    st->rx1[x]=(st->gridx/10+random()%(st->gridx*8/10));
    st->ry1[x]=(st->gridy/10+random()%(st->gridy*8/10));
    st->rx2[x]=(st->gridx*2/10+random()%(st->gridx*6/10));
    st->ry2[x]=(st->gridy*2/10+random()%(st->gridy*6/10));
    st->rx3[x]=(st->gridx*3/10+random()%(st->gridx*4/10));
    st->ry3[x]=(st->gridy*3/10+random()%(st->gridy*4/10));
  }
}

static int
_shape(struct state *st, int x, int y, int rx, int ry, int n)
{
  switch(st->shape[n]) {
    case 0:                                        /* square/rectangle */
    case 1:
    case 2:
      return(1+_max(abs(x-rx)*st->cs1[n]/st->cs2[n],abs(y-ry)*st->cs3[n]/st->cs4[n]));
    case 3:                                                 /* diamond */
    case 4:
      return(1+(abs(x-rx)*st->cs1[n]/st->cs2[n]+abs(y-ry)*st->cs3[n]/st->cs4[n]));
    case 5:                                            /* 8 point star */
      return(1+_min(_max(abs(x-rx),abs(y-ry))*3/2,abs(x-rx)+abs(y-ry)));
    case 6:					        /* circle/oval */
    case 7:
    case 8:
      return(1+_dist(st,x,rx,y,ry,st->cs1[n]));
    case 9:					  /* black hole circle */
      return(1+(st->gridx*st->gridy/(1+(_dist(st,x,rx,y,ry,st->cs2[n])))));
    case 10:                                                   /* sun */
      return(1+_min(abs(x-rx)*st->gridx/(abs(y-ry)+1),abs(y-ry)*st->gridx/(abs(x-rx)+1)));
    case 11:                             /* 2 circles+inverted circle */
      return(1+(_dist(st,x,rx,y,ry,st->cs1[n])*_dist(st,x,(rx*3)%st->gridx,y,(ry*5)%st->gridy,st->cs1[n])/(1+_dist(st,x,(rx*4)%st->gridx,y,(ry*7)%st->gridy,st->cs1[n]))));
    case 12:                                                   /* star */
      return(1+(int)sqrt(abs((x-rx)*(y-ry))));
    case 13:                                       /* centered ellipse */
      return(1+_dist(st,x,rx,y,ry,0)+_dist(st,x,st->gridx-rx,y,st->gridy-ry,0));
    default:                                               /* triangle */
      return(1+_triangle(st,x,y,rx,ry,st->cs4[n]));
  }
  return(1+_triangle(st,x,y,rx,ry,st->cs4[n]));
}

static int
_pattern(struct state *st, int x, int y, int n)
{
  int v=0, ox;
  ox=x;
  switch(st->wsx[n]) {
    case 0:                                             /* slants */
      x+=y/(1+st->cs4[n]);
      break;
    case 1:
      x+=(st->gridy-y)/(1+st->cs4[n]);
      break;
    case 2:                                             /* curves */
      x+=_wave(st,y,st->gridx/(1+st->cs1[n]),st->gridy,0);
      break;
    case 3:
      x+=_wave(st,st->gridy-y,st->gridy/(1+st->cs1[n]),st->gridy,0);
      break;
    case 4:                                           /* U curves */
      x+=_wave(st,y,st->cs1[n]*st->csw[n]/2,st->gridy*2/M_PI,0);
      break;
    case 5:
      x-=_wave(st,y,st->cs1[n]*st->csw[n]/2,st->gridy*2/M_PI,0);
      break;
  }
  switch(st->wsy[0]) {
    case 0:                                          /* slants */
      y+=ox/(1+st->cs1[n]);
      break;
    case 1:
      y+=(st->gridx-ox)/(1+st->cs1[n]);
      break;
    case 2:                                           /* curves */
      y+=_wave(st,ox,st->gridx/(1+st->cs1[n]),st->gridx,0);
      break;
    case 3:
      y+=_wave(st,st->gridx-ox,st->gridx/(1+st->cs1[n]),st->gridx,0);
      break;
    case 4:                                         /* U curves */
      y+=_wave(st,ox,st->cs1[n]*st->csw[n]/2,st->gridy*2/M_PI,0);
      break;
    case 5:
      y-=_wave(st,ox,st->cs1[n]*st->csw[n]/2,st->gridy*2/M_PI,0);
      break;
  }
  switch(st->pattern[n]) {
    case 0:                                          /* horizontal stripes */
      v=y;
      break;
    case 1:                                            /* vertical stripes */
      v=x;
      break;
    case 2:                                            /* diagonal stripes */
      v=(x+(y*st->cs1[n]/st->cs2[n]));
      break;
    case 3:                                    /* reverse diagonal stripes */
      v=(x-(y*st->cs1[n]/st->cs2[n]));
      break;
    case 4:			   	        	   /* checkerboard */
      v=(y/st->csw[n]*3+x/st->csw[n])*st->csw[n];
      break;
    case 5:			   	          /* diagonal checkerboard */
      v=((x+y)/2/st->csw[n]+(x+st->gridy-y)/2/st->csw[n]*3)*st->csw[n];
      break;
    case 6:                                                     /* + cross */
      v=st->gridx+(_min(abs(x-st->rx3[n]),abs(y-st->ry3[n]))*2);
      break;
    case 7:                                              /* double + cross */
      v=_min(_min(abs(x-st->rx2[n]),abs(y-st->ry2[n])),_min(abs(x-st->rx1[n]),abs(y-st->ry1[n])))*2;
      break;
    case 8:                                                     /* X cross */
      v=st->gridx+(_min(abs(x-st->rx3[n])*st->cs1[n]/st->cs2[n]+abs(y-st->ry2[n])*st->cs3[n]/st->cs4[n],abs(x-st->rx3[n])*st->cs1[n]/st->cs2[n]-abs(y-st->ry3[n])*st->cs3[n]/st->cs4[n])*2);
      break;
    case 9:                                              /* double X cross */
      v=_min(_min(abs(x-st->rx2[n])+abs(y-st->ry2[n]),abs(x-st->rx2[n])-abs(y-st->ry2[n])),_min(abs(x-st->rx1[n])+abs(y-st->ry1[n]),abs(x-st->rx1[n])-abs(y-st->ry1[n])))*2;
      break;
    case 10:                                   /* horizontal stripes/waves */
      v=st->gridy+(y+_wave(st,x,st->waveh[n],st->wavel[n],st->wave[n]));
      break;
    case 11:                                     /* vertical stripes/waves */
      v=st->gridx+(x+_wave(st,y,st->waveh[n],st->wavel[n],st->wave[n]));
      break;
    case 12:                                     /* diagonal stripes/waves */
      v=st->gridx+(x+(y*st->cs1[n]/st->cs2[n])+_wave(st,x,st->waveh[n],st->wavel[n],st->wave[n]));
      break;
    case 13:                                     /* diagonal stripes/waves */
      v=st->gridx+(x-(y*st->cs1[n]/st->cs2[n])+_wave(st,y,st->waveh[n],st->wavel[n],st->wave[n]));
      break;
    case 14:                                    /* horizontal spikey waves */
      v=y+(st->csw[n]*st->cs4[n]/st->cs3[n])+_wave(st,x+((y/st->cs3[n])*st->edir),st->csw[n]/2*st->cs1[n]/st->cs2[n],st->csw[n]/2*st->cs2[n]/st->cs1[n],st->wave[n]);
      break;
    case 15:                                      /* vertical spikey waves */
      v=x+(st->csw[n]*st->cs1[n]/st->cs2[n])+_wave(st,y+((x/st->cs3[n])*st->edir),st->csw[n]/2*st->cs1[n]/st->cs2[n],st->csw[n]/2*st->cs3[n]/st->cs4[n],st->wave[n]);
      break;
    case 16:                                    /* big slanted hwaves */
      v=st->gridy-y-(x*st->cs1[n]/st->cs3[n])+(st->csw[n]*st->cs1[n]*st->cs2[n]) +_wave(st,x,st->csw[n]/3*st->cs1[n]*st->cs2[n],st->csw[n]/3*st->cs3[n]*st->cs2[n],st->wave[n]);
      break;
    case 17:                                    /* big slanted vwaves */
      v=x-(y*st->cs1[n]/st->cs3[n])+(st->csw[n]*st->cs1[n]*st->cs2[n]) +_wave(st,y, st->csw[n]/3*st->cs1[n]*st->cs2[n], st->csw[n]/3*st->cs3[n]*st->cs2[n], st->wave[n]);
      break;
    case 18:                                          /* double hwave */
      v=y+(y+st->csw[n]*st->cs3[n])+_wave(st,x,st->csw[n]/3*st->cs3[n],st->csw[n]/3*st->cs2[n],st->wave[n])+_wave(st,x,st->csw[n]/3*st->cs4[n],st->csw[n]/3*st->cs1[n]*3/2,st->wave[n]);
      break;
    case 19:                                          /* double vwave */
      v=x+(x+st->csw[n]*st->cs1[n])+_wave(st,y,st->csw[n]/3*st->cs1[n],st->csw[n]/3*st->cs3[n],st->wave[n])+_wave(st,y,st->csw[n]/3*st->cs2[n],st->csw[n]/3*st->cs4[n]*3/2,st->wave[n]);
      break;
    case 20:					              /* one shape */
    case 21:
    case 22:
      v=_shape(st,x, y, st->rx3[n], st->ry3[n], n);
      break;
    case 23:					             /* two shapes */
    case 24:
    case 25:
      v=_min(_shape(st,x, y, st->rx1[n], st->ry1[n], n),_shape(st,x, y, st->rx2[n], st->ry2[n], n));
      break;
    case 26:					   /* two shapes opposites */
    case 27:
      v=_min(_shape(st,x, y, st->rx2[n], st->ry2[n], n),_shape(st,x, y, st->gridx-st->rx2[n], st->gridy-st->rx2[n], n));
      break;
    case 28:				         /* two shape checkerboard */
    case 29:
      v=((_shape(st,x, y, st->rx1[n], st->ry1[n], n)/st->csw[n])+(_shape(st,x, y, st->rx2[n], st->ry2[n], n)/st->csw[n]))*st->csw[n];
      break;
    case 30:				                 /* two shape blob */
    case 31:
      v=(_shape(st,x, y, st->rx1[n], st->ry1[n], n)+_shape(st,x, y, st->rx2[n], st->ry2[n], n))/2;
      break;
    case 32:				        /* inverted two shape blob */
    case 33:
      v=(_shape(st,x, y, st->rx1[n], st->ry1[n], n)+_shape(st,st->gridx-x, st->gridy-y, st->rx1[n], st->ry1[n], n))/2;
      break;
    case 34:					           /* three shapes */
    case 35:
      v=_min(_shape(st,x, y, st->rx3[n], st->ry3[n], n),_min(_shape(st,x, y, st->rx1[n], st->ry1[n], n),_shape(st,x, y, st->rx2[n], st->ry2[n], n)));
      break;
    case 36:					       /* three shape blob */
    case 37:
      v=(_shape(st,x, y, st->rx1[n], st->ry1[n], n)+_shape(st,x, y, st->rx2[n], st->ry2[n], n)+_shape(st,x, y, st->rx3[n], st->ry3[n], n))/3;
      break;
    case 38:				                      /* 4 shapes */	
      v=(_min(_shape(st,x, y, st->rx2[n], st->ry2[n], n),_shape(st,x, y, st->gridx-st->rx2[n], st->gridy-st->ry2[n], n)),_min(_shape(st,x, y, st->gridx-st->rx2[n], st->ry2[n], n),_shape(st,x, y, st->rx2[n], st->gridy-st->ry2[n], n)));
      break;
    case 39:				    	    	/* four rainbows */
      v=(_min(_shape(st,x, y, st->gridx-st->rx2[n]/2, st->csw[n], n),_shape(st,x, y, st->csw[n], st->ry2[n]/2, n)),_min(_shape(st,x, y, st->rx2[n]/2, st->gridy-st->csw[n], n),_shape(st,x, y, st->gridx-st->csw[n], st->gridy-(st->ry2[n]/2), n)));
      break;
  }
  /* stretch or contract stripe */
  switch(st->sec[n]) {
    case 0:
      v=(int)sqrt((int)sqrt(abs(v)*st->gridx)*st->gridx);
      break;
    case 1:
      v=((int)pow(v,2)/st->gridx);
      break;
  }
  return (abs(v));
}

static int
_getcolor(struct state *st, int x, int y)
{
  int n, cv[LAYERS];

  cv[0] = 0;
  for (n=0; n<st->layers; n++) {
    cv[n]=_pattern(st,x,y,n);
                  /* first wave/shape */
    cv[0] = (!n) ? cv[0]/st->csw[0] :
                    /* checkerboard+1 */
      (st->mix[n]<5) ? (cv[0]*st->csw[0]+cv[n])/st->csw[n] :
               /* checkerboard+ncol/2 */
      (st->mix[n]<12) ? cv[0]+(cv[n]/st->csw[n]*st->ncolors/2) :
                           /* add mix */
      (st->mix[n]<16) ? cv[0]+(cv[n]/st->csw[n]) :
                      /* subtract mix */
      (st->mix[n]<18) ? cv[0]-(cv[n]/st->csw[n]) :
                  /* r to l morph mix */
      (st->mix[n]==18) ? ((cv[0]*x)+(cv[n]*(st->gridx-x)/st->csw[n]))/st->gridx :
                  /* u to d morph mix */
      ((cv[0]*y)+(cv[n]*(st->gridy-y)/st->csw[n]))/st->gridy;
  }
  return(cv[0]);
}

/* return value=line direction
   st->olen=open space to edge or next blocking line
   st->bln=blocking line number or -1 if edge blocks */
static int
_findopen(struct state *st, int x, int y, int z)
{
  int dir, od[4], no=0;

  if (((st->grid[z].hl) || (st->grid[z].hr)) &&
    ((st->grid[z].vu) || (st->grid[z].vd)))
    return(DIR_NONE);
  if ((z>st->gridx) && (!st->grid[z].hl) && (!st->grid[z].hr) &&
    (!st->grid[z-st->gridx].line)) {
    od[no]=DIR_UP;
    no++;
  }
  if ((z<st->gridn-st->gridx) && (!st->grid[z].hl) &&
    (!st->grid[z].hr) && (!st->grid[z+st->gridx].line)) {
    od[no]=DIR_DOWN;
    no++;
  }
  if ((x) && (!st->grid[z].hl) && (!st->grid[z].hr) &&
    (!st->grid[z-1].line)) {
    od[no]=DIR_LEFT;
    no++;
  }
  if (((z+1)%st->gridx) && (!st->grid[z].hl) && (!st->grid[z].hr) &&
    (!st->grid[z+1].line)) {
    od[no]=DIR_RIGHT;
    no++;
  }
  if (!no)
    return(DIR_NONE);
  dir=od[random()%no];
  st->olen=st->bln=0;
  while ((st->olen<=st->maxlen) && (!st->bln)) {
    st->olen++;
    if (dir==DIR_UP)
      st->bln = (y-st->olen<0) ? -1 :
        st->grid[z-(st->olen*st->gridx)].line;
    if (dir==DIR_DOWN)
      st->bln = (y+st->olen>=st->gridy) ? -1 :
        st->grid[z+(st->olen*st->gridx)].line;
    if (dir==DIR_LEFT)
      st->bln = (x-st->olen<0) ? -1 :
        st->grid[z-st->olen].line;
    if (dir==DIR_RIGHT)
      st->bln = (x+st->olen>=st->gridx) ? -1 :
        st->grid[z+st->olen].line;
  }
  st->olen--;
  return(dir);
}

static void
_fillgrid(struct state *st)
{
  unsigned int gridc, n, add;

  gridc=st->gridx*st->dline[st->li].y+st->dline[st->li].x;
  add = (st->dline[st->li].hv) ? 1 : st->gridx;
  for (n=0;  n<=st->dline[st->li].len; n++) {
    if (n)
      gridc+=add;
    if (!st->grid[gridc].line) {
      st->fi++;
      st->grid[gridc].line=st->li;
    }
    if (st->dline[st->li].hv) {
      if (n)
        st->grid[gridc].hr=st->li;
      if (n<st->dline[st->li].len)
        st->grid[gridc].hl=st->li;
    } else {
      if (n)
        st->grid[gridc].vd=st->li;
      if (n<st->dline[st->li].len)
        st->grid[gridc].vu=st->li;
    }
    if (st->fi>=st->gridn) {
      st->grid_full=True;
      return;
    }
  }
}

static void
_newline(struct state *st)
{
  int bl, bz, dir, lt, x, y, z;

  bl=0;
  z=st->zlist[st->zi];
  x=z%st->gridx;
  y=z/st->gridx;
  st->zi++;
  dir=_findopen(st,x,y,z);

  if (!st->grid[z].line) {
  /* this is an empty space, make a new line unless nothing is open around it */
    if (dir==DIR_NONE) {
      /* nothing is open, force a len 1 branch in any direction */
      lt=LINE_FORCE;
      while ((dir==DIR_NONE) ||
        ((dir==DIR_UP) && (!y)) ||
        ((dir==DIR_DOWN) && (y+1==st->gridy)) ||
        ((dir==DIR_LEFT) && (!x)) ||
        ((dir==DIR_RIGHT) && (x+1==st->gridx))) {
          dir=random()%4;
      }
      bz = (dir==DIR_UP) ? z-st->gridx : (dir==DIR_DOWN) ? z+st->gridx : (dir==DIR_LEFT) ? z-1 : z+1;
      bl = st->grid[bz].line;
    } else if ((st->bnratio>1) && (st->bln>0) &&
      (st->olen<st->maxlen) && (random()%st->bnratio)) {
      /* branch into blocking line */
      lt=LINE_BRIN;
      bl = st->bln;
    } else {
      /* make a new line and new object */
      lt=LINE_NEW;
      st->oi++;
    }
  } else {
    /* this is a filled space, make a branch unless nothing is open around it */
    if (dir==DIR_NONE)
      return;
    /* make a branch out of this line */
    lt=LINE_BROUT;
    bl=st->grid[z].line;
  }
  st->li++;
  st->dline[st->li].len = (lt==LINE_FORCE) ? 1 :  (lt==LINE_BRIN) ?
    st->olen+1 : (!st->forcemax) ? st->olen : 1+random()%st->olen;
  st->dline[st->li].x=x;
  if (dir==DIR_LEFT)
    st->dline[st->li].x-=st->dline[st->li].len;
  st->dline[st->li].y=y;
  if (dir==DIR_UP)
    st->dline[st->li].y-=st->dline[st->li].len;
  st->dline[st->li].hv = ((dir==DIR_LEFT) || (dir==DIR_RIGHT)) ?
    True : False;
  st->dline[st->li].obj = (lt==LINE_NEW) ? st->oi :
    st->dline[bl].obj;
  if (lt==LINE_NEW) {
    int color =  (_getcolor(st,x,y))%st->ncolors;
    if (color < 0) color += st->ncolors;
    st->dline[st->li].color = color;
  } else {
    st->dline[st->li].color = st->dline[bl].color;
  }
  st->dline[st->li].deo=(_getdeo(st,x,y,st->dmap,1) +
    (random()%st->dvar) + (random()%st->dvar))*st->ddir;
  st->dline[st->li].ndol=0;
  _fillgrid(st);
}

static void
_create_screen(struct state *st)
{
  while(!st->grid_full)
    _newline(st);
  qsort(st->dline, st->li+1, sizeof(struct lineStruct), _comparedeo);
/*st->lpu=st->li/20/((6-st->speed)*3);
  Used to use a computed lpu, lines per update to control draw speed
  draw 1/lpu of the lines before each XSync which takes a split second
  the higher the lpu, the quicker the screen draws.  This worked somewhat
  after the 4->5 update, however with the Mac updating so much more slowly,
  values tuned for it draw the screen in a blink on Linux.  Therefore we
  draw 1/200th of the screen with each update and sleep, if necessary */
  st->lpu = (st->dialog) ? st->li/50 : st->li/200;
  if (!st->lpu) st->lpu = 1;
  st->bi=1;
  st->mode=MODE_ERASE;
}

static void
_fill_outline(struct state *st, int di)
{
  int x, y, h, w;

  if (!di)
    return;
  x=st->dline[di].x*st->lwid+1;
  y=st->dline[di].y*st->lwid+1;
  if (st->dline[di].hv) {
    w=(st->dline[di].len+1)*st->lwid-3;
    h=st->lwid-3;
  } else {
    w=st->lwid-3;
    h=(st->dline[di].len+1)*st->lwid-3;
  }
  XFillRectangle (st->display, st->window, st->bgc, x, y, w, h);
}

static void
_XFillRectangle(struct state *st, int di, int adj)
{
  int a, b, x, y, w, h;

  x=st->dline[di].x*st->lwid;
  y=st->dline[di].y*st->lwid;
  if (st->dline[di].hv) {
    w=(st->dline[di].len+1)*st->lwid-1;
    h=st->lwid-1;
  } else {
    w=st->lwid-1;
    h=(st->dline[di].len+1)*st->lwid-1;
  }
  switch (st->d3d) {
    case D3D_NEON:
      x+=adj;
      y+=adj;
      w-=adj*2;
      h-=adj*2;
    break;
    case D3D_BLOCK:
      x+=adj;
      y+=adj;
      w-=st->lwid/2-1;
      h-=st->lwid/2-1;
    break;
  }
  if (!st->round) {
    XFillRectangle(st->display, st->window, st->fgc, x, y, w, h);
  } else {
    if (h<st->lwid) {                                   /* horizontal */
      a=(h-1)/2;
      for (b=0; b<=a; b++)
        XFillRectangle(st->display, st->window, st->fgc,
          x+b, y+a-b, w-b*2, h-((a-b)*2));
    } else {                                               /* vertical */
      a=(w-1)/2;
      for (b=0; b<=a; b++)
        XFillRectangle(st->display, st->window, st->fgc,
          x+a-b, y+b, w-((a-b)*2), h-b*2);
    }
  }
}

static void
_XFillTriangle(struct state *st, int color, int x1, int y1, int x2, int y2,
  int x3, int y3)
{
  XPoint points[3];

  points[0].x=x1;
  points[0].y=y1;
  points[1].x=x2;
  points[1].y=y2;
  points[2].x=x3;
  points[2].y=y3;
  XSetForeground(st->display, st->fgc, st->colors[color].pixel);
  XFillPolygon (st->display, st->window, st->fgc, points, 3, Convex,
      CoordModeOrigin);
}

static void
_XFillPolygon4(struct state *st, int color, int x1, int y1, int x2, int y2,
  int x3, int y3, int x4, int y4)
{
  XPoint points[4];

  points[0].x=x1;
  points[0].y=y1;
  points[1].x=x2;
  points[1].y=y2;
  points[2].x=x3;
  points[2].y=y3;
  points[3].x=x4;
  points[3].y=y4;
  XSetForeground(st->display, st->fgc, st->colors[color].pixel);
  XFillPolygon (st->display, st->window, st->fgc, points, 4, Convex,
      CoordModeOrigin);
}

static void
_draw_tiled(struct state *st, int color)
{
  int a, c, d, x, y, z, m1, m2, lr, nl, w, h;
  a = (st->dline[st->di].hv) ? 1 : st->gridx;
  z = st->dline[st->di].y*st->gridx+st->dline[st->di].x;
  m1 = (st->lwid-1)/2;
  m2 = st->lwid/2;
  lr = st->lwid-1;
  nl = st->lwid;

  /* draw tiles one grid cell at a time */
  for (c=0; c<=st->dline[st->di].len; c++) {
    if (st->dline[st->di].hv) {
      x = (st->dline[st->di].x+c)*st->lwid;
      y = st->dline[st->di].y*st->lwid;
      if (c)
        st->grid[z].dhr=st->di;
      if (c<st->dline[st->di].len)
        st->grid[z].dhl=st->di;
    } else {
      x = st->dline[st->di].x*st->lwid;
      y = (st->dline[st->di].y+c)*st->lwid;
      if (c)
        st->grid[z].dvd=st->di;
      if (c<st->dline[st->di].len)
        st->grid[z].dvu=st->di;
    }
    d=0;
    if (st->grid[z].dhl)
      d+=8;
    if (st->grid[z].dhr)
      d+=4;
    if (st->grid[z].dvu)
      d+=2;
    if (st->grid[z].dvd)
      d++;
    /* draw line base */
    switch (d) {
      case 1:
      case 2:                                    /* vertical */
      case 3:
      case 5:
      case 6:
      case 7:
      case 11:
      case 15:
        h = ((d==1) || (d==5)) ? lr : nl;
        XSetForeground(st->display, st->fgc,
          st->colors[color].pixel);
        XFillRectangle (st->display, st->window, st->fgc,
          x, y, m2, h);
        XSetForeground(st->display, st->fgc,
           st->colors[color+3].pixel);
        XFillRectangle (st->display, st->window, st->fgc,
          x+m2, y, m1, h);
        break;
      case 4:
      case 8:                                     /* horizontal */
      case 9:
      case 10:
      case 12:
      case 13:
      case 14:
        w = (d==4) ? lr : nl;
        XSetForeground(st->display, st->fgc,
          st->colors[color+1].pixel);
        XFillRectangle (st->display, st->window, st->fgc,
          x, y, w, m2);
        XSetForeground(st->display, st->fgc,
           st->colors[color+2].pixel);
        XFillRectangle (st->display, st->window, st->fgc,
          x, y+m2, w, m1);
        break;
    }
    /* draw angles */
    switch(d) {
      case 1:                                      /* bottom end ^ */
        _XFillTriangle(st,color+2, x, y+lr, x+lr, y+lr, x+m2, y+m2);
        break;
      case 2:                                       /* top end \/ */
        _XFillTriangle(st,color+1, x, y, x+lr, y, x+m2, y+m2);
        break;
      case 4:                                       /* right end < */
        _XFillTriangle(st,color+3, x+lr, y, x+lr, y+lr, x+m2, y+m2);
        break;
      case 5:                                        /* LR corner */
        _XFillTriangle(st,color+1, x, y+m2, x+m2, y+m2, x, y);
        _XFillPolygon4(st,color+2, x, y+m2, x+m2, y+m2, x+lr, y+lr, x, y+lr);
        break;
      case 6:                                        /* UR corner */
        _XFillPolygon4(st,color+1, x, y+m2, x+m2, y+m2, x+lr, y, x, y);
        _XFillTriangle(st,color+2, x, y+m2, x+m2, y+m2, x, y+lr);
        break;
      case 7:                                        /* T > into line */
        _XFillTriangle(st,color+1, x, y+m2, x+m2, y+m2, x, y);
        _XFillTriangle(st,color+2, x, y+m2, x+m2, y+m2, x, y+lr);
        break;
      case 8:                                       /* left end > */
        _XFillTriangle(st,color, x, y, x, y+lr, x+m2, y+m2);
        break;
      case 9:                                       /* LL corner */
        _XFillPolygon4(st,color, x+m2, y, x+m2, y+m2, x, y+lr, x, y);
        _XFillTriangle(st,color+3, x+m2, y, x+m2, y+m2, x+lr, y);
        break;
      case 10:                                      /* UL corner */
        _XFillPolygon4(st,color, x+m2, y+nl, x+m2, y+m2, x, y, x, y+nl);
        _XFillPolygon4(st,color+3, x+m2, y+nl, x+m2, y+m2, x+lr, y+lr, x+lr, y+nl);
        break;
      case 11:                                       /* T < into line */
        _XFillPolygon4(st,color+1, x+nl, y+m2, x+m2, y+m2, x+lr, y, x+nl, y);
        _XFillPolygon4(st,color+2, x+nl, y+m2, x+m2, y+m2, x+lr, y+lr, x+nl, y+lr);
        break;
      case 13:                                        /* T \/ into line */
        _XFillTriangle(st,color, x+m2, y, x+m2, y+m2, x, y);
        _XFillTriangle(st,color+3, x+m2, y, x+m2, y+m2, x+lr, y);
        break;
      case 14:                                        /* T ^ into line */
        _XFillPolygon4(st,color, x+m2, y+nl, x+m2, y+m2, x, y+lr, x, y+nl);
        _XFillPolygon4(st,color+3, x+m2, y+nl, x+m2, y+m2, x+lr, y+lr, x+lr, y+nl);
        break;
      case 15:                                        /* X intersection */
        _XFillTriangle(st,color+1, x, y+m2, x+m2, y+m2, x, y);
        _XFillTriangle(st,color+2, x, y+m2, x+m2, y+m2, x, y+lr);
        _XFillPolygon4(st,color+1, x+nl, y+m2, x+m2, y+m2, x+lr, y, x+nl, y);
        _XFillPolygon4(st,color+2, x+nl, y+m2, x+m2, y+m2, x+lr, y+lr, x+nl, y+lr);
        break;
    }
    z+=a;
  }
}

static long
_mselapsed(struct state *st)
{
  struct timeval t;
  gettimeofday(&t, NULL);
  t.tv_sec -= st->time.tv_sec;
  t.tv_usec -= st->time.tv_usec;
  return ((long)t.tv_sec*1000000+t.tv_usec);
}

static void
_draw_lines(struct state *st)
{
  int n, z, a, color, sh, di;
  if (st->bi==1)
    for (a=0; a<=st->oi; a++)
      st->fdol[a]=0;

  for (st->di=st->bi; st->di<_min(st->li+1,st->bi+st->lpu); st->di++) {
    color=(st->dline[st->di].color%st->ncolors)*st->shades;
    XSetForeground(st->display, st->fgc, st->colors[color].pixel);

    switch (st->d3d) {
      case D3D_NEON:
        st->dline[st->di].ndol=st->fdol[st->dline[st->di].obj];
        st->fdol[st->dline[st->di].obj]=st->di;
        for (sh=0; sh<st->lwid/2; sh++) {
          XSetForeground(st->display, st->fgc,
            st->colors[color+sh].pixel);
          di=st->di;
          while(di>0) {
            _XFillRectangle(st,di,sh);
            di=st->dline[di].ndol;
          }
        }
        break;
      case D3D_BLOCK:
        st->dline[st->di].ndol=st->fdol[st->dline[st->di].obj];
        st->fdol[st->dline[st->di].obj]=st->di;
        for (sh=0; sh<st->lwid/2; sh++) {
          XSetForeground(st->display, st->fgc,
            st->colors[color+(st->lwid/2)-sh-1].pixel);
          di=st->di;
          while(di>0) {
            _XFillRectangle(st,di,sh);
            di=st->dline[di].ndol;
          }
        }
        break;
      case D3D_TILED:
        _draw_tiled(st,color);
        break;
      default:               /* D3D_NONE */
        _XFillRectangle(st,st->di,0);
        if (st->outline) {
          _fill_outline(st, st->di);
          z=st->dline[st->di].y*st->gridx+st->dline[st->di].x;
          a = (st->dline[st->di].hv) ? 1 : st->gridx;
          for (n=0; n<=st->dline[st->di].len; n++) {
            _fill_outline(st, st->grid[z].dhl);
            _fill_outline(st, st->grid[z].dhr);
            _fill_outline(st, st->grid[z].dvu);
            _fill_outline(st, st->grid[z].dvd);
            if (st->dline[st->di].hv) {
              if (n)
                st->grid[z].dhr=st->di;
              if (n<st->dline[st->di].len)
                st->grid[z].dhl=st->di;
            } else {
              if (n)
                st->grid[z].dvd=st->di;
              if (n<st->dline[st->di].len)
                st->grid[z].dvu=st->di;
            }
            z+=a;
          }
        }
        break;
    }
  }
  if (st->di>st->li) {
      st->bi=1;
      st->mode=MODE_CREATE;
  } else {
      st->bi+=st->lpu;
  }
}

static void
_erase_lines(struct state *st)
{
  if (!st->ii)
    return;
  for (st->di=st->bi; st->di<_min(st->eli+1,st->bi+st->elpu); st->di++) {
    if (st->eline[st->di].hv) {
      XFillRectangle (st->display, st->window, st->bgc,
      st->eline[st->di].x*st->elwid,
      st->eline[st->di].y*st->elwid,
      (st->eline[st->di].len+1)*st->elwid, st->elwid);
    } else {
      XFillRectangle (st->display, st->window, st->bgc,
      st->eline[st->di].x*st->elwid,
      st->eline[st->di].y*st->elwid,
      st->elwid, (st->eline[st->di].len+1)*st->elwid);
    }
    if (st->di==st->eli) /* clear just in case */
      XFillRectangle(st->display, st->window, st->bgc, 0, 0,
        st->xgwa.width, st->xgwa.height);
  }
  if (st->di>st->eli) {
      st->bi=1;
      if (st->resized) {
         st->mode=MODE_CREATE;
      } else {
         st->mode=MODE_DRAW;
      }
  } else {
      st->bi+=st->elpu;
  }
}

static void *
abstractile_init(Display *display, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
/*  struct utsname os;*/

  char *tile = get_string_resource(display, "tile", "Tile");
  if      (tile && !strcmp(tile, "random")) st->tile = TILE_RANDOM;
  else if (tile && !strcmp(tile, "flat")) st->tile = TILE_FLAT;
  else if (tile && !strcmp(tile, "thin")) st->tile = TILE_THIN;
  else if (tile && !strcmp(tile, "outline")) st->tile = TILE_OUTLINE;
  else if (tile && !strcmp(tile, "block")) st->tile = TILE_BLOCK;
  else if (tile && !strcmp(tile, "neon")) st->tile = TILE_NEON;
  else if (tile && !strcmp(tile, "tiled")) st->tile = TILE_TILED;
  else {
    if (tile && *tile && !!strcmp(tile, "random"))
      fprintf(stderr, "%s: unknown tile option %s\n", progname, tile);
    st->tile = TILE_RANDOM;
  }
  if (tile) free (tile);

  st->speed = get_integer_resource(display, "speed", "Integer");
  if (st->speed < 0) st->speed = 0;
  if (st->speed > 5) st->speed = 5;
  st->sleep = get_integer_resource(display, "sleep", "Integer");
  if (st->sleep < 0) st->sleep = 0;
  if (st->sleep > 60) st->sleep = 60;

  st->display=display;
  st->window=window;

  /* get screen size and create Graphics Contexts */
  XGetWindowAttributes (display, window, &st->xgwa);
  gcv.foreground = get_pixel_resource(display, st->xgwa.colormap,
      "foreground", "Foreground");
  st->fgc = XCreateGC (display, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource(display, st->xgwa.colormap,
      "background", "Background");
  st->bgc = XCreateGC (display, window, GCForeground, &gcv);

/* Um, no. This is obscene. -jwz.
  uname(&os);
  st->newcols=((!strcmp(os.sysname,"Linux")) || (!strcmp(os.sysname,"Darwin")))
      ? True : False;
*/
  st->newcols=False;

  st->mode=MODE_CREATE;
  st->ii=0;
  st->resized=True;
  return st;
}

static unsigned long
abstractile_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int mse, usleep;

  gettimeofday(&st->time, NULL);

  /* If the window is too small, do nothing, sorry! */
  if (st->xgwa.width > 20 && st->xgwa.height > 20) {
    switch (st->mode) {
    case MODE_CREATE:
      _init_screen(st);
      _create_screen(st);
      break;
    case MODE_ERASE:
      _erase_lines(st);
      break;
    case MODE_DRAW:
      _draw_lines(st);
      break;
    }
  }
  mse=_mselapsed(st);
  usleep = ((!st->ii) && (st->mode==MODE_CREATE)) ?  0 :
      (st->mode==MODE_CREATE) ?  st->sleep*1000000-mse :
      /* speed=0-5, goal is 10,8,6,4,2,0 sec normal and 5,4,3,2,1,0 dialog */
      (5-st->speed)*(2-st->dialog)*100000/st->lpu-mse;
  if (usleep>=0)
      return usleep;
  return 0;
}

static void
abstractile_reshape (Display *dpy, Window window, void *closure,
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xgwa.width = w;
  st->xgwa.height = h;
  if (w*h>st->max_wxh)
    st->resized=True;
}

static Bool
abstractile_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->mode=MODE_CREATE;
      return True;
    }

  return False;
}

static void
abstractile_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->dline) free(st->dline);
  if (st->eline) free(st->eline);
  if (st->grid)  free(st->grid);
  if (st->zlist) free(st->zlist);
  if (st->fdol)  free(st->fdol);
  if (st->odi)   free(st->odi);
  XFreeGC (dpy, st->fgc);
  XFreeGC (dpy, st->bgc);
  free (st);
}

static const char *abstractile_defaults [] = {
  ".background:    black",
  ".foreground:    white",
  "*fpsSolid:	   true",
  "*sleep:             3",
  "*speed:             3",
  "*tile:         random",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec abstractile_options [] = {
  { "-sleep",  ".sleep",  XrmoptionSepArg, 0 },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-tile",   ".tile",   XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Abstractile", abstractile)
