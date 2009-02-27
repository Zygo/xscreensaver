#ifndef lint
static char sccsid[] = "@(#)flame.c	2.7 95/02/21 xlockmore";
#endif
/*-
 * flame.c - recursive fractal cosmic flames.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 19-Jun-95: edited by David Bagley.
 * 13-Jun-95: updated. (received from Scott Graves, spot@cs.cmu.edu).
 * 27-Jun-91: vary number of functions used.
 * 24-Jun-91: fixed portability problem with integer mod (%).
 * 06-Jun-91: Written. (received from Scott Graves, spot@cs.cmu.edu).
 */

#include "xlock.h"
#include <math.h>
#include <stdio.h>

#define MAXBATCH1	200 /* mono */
#define MAXBATCH2	20  /* color */
#define FUSE		10  /* discard this many initial iterations */
#define NMAJORVARS	7

typedef struct {
   /* shape of current flame */
   int         nxforms;
   double      f[2][3][10];	/* a bunch of non-homogeneous xforms */
   int         variation[10];  /* for each xform */

   /* high-level control */
   int		mode;		/* 0->slow/single 1->fast/many */
   int		nfractals;	/* draw this many fractals */
   int		major_variation;
   int		fractal_len;	/* pts/fractal */
   int          color;
   int		rainbow;       /* more than one color per fractal
			      1-> computed by adding dimension to fractal */

   int          width, height;  /* of window */
   int		timer;
   
   /* draw info about current flame */
   int	       fuse;	/* iterate this many before drawing */
   int         total_points;   /* draw this many pts before fractal ends */
   int         npoints;    /* how many we've computed but not drawn */
   XPoint      pts[MAXBATCH1];  /* here they are */
   long        pixcol;
   /* when drawing in color, we have a buffer per color */
   int	       ncpoints[NUMCOLORS];
   XPoint      cpts[NUMCOLORS][MAXBATCH2];

   double      x, y, c;
} flamestruct;
static flamestruct flames[MAXSCREENS];



static short
halfrandom(mv)
    int         mv;
{
    static short lasthalf = 0;
    unsigned long r;

    if (lasthalf) {
	r = lasthalf;
	lasthalf = 0;
    } else {
	r = RAND();
	lasthalf = r >> 16;
    }
    r = r % mv;
    return r;
}

static int
frandom(n)
  int n;
{
   static long saved_random_bits = 0;
   static int  nbits = 0;
   int result;
   if (3 > nbits) {
     saved_random_bits = RAND();
     nbits = 31;
   }
   switch(n) {
     case 2:  result = saved_random_bits&1;
	      saved_random_bits >>= 1;
	      nbits -= 1;
	      return result;

     case 3:  result = saved_random_bits&3;
	      saved_random_bits >>= 2;
	      nbits -= 2;
	      if (3 == result)
		 return frandom(3);
	      return result;

     case 4:  result = saved_random_bits&3;
	      saved_random_bits >>= 2;
	      nbits -= 2;
	      return result;

     case 5:  result = saved_random_bits&7;
	      saved_random_bits >>= 3;
	      nbits -= 3;
	      if (4 < result)
		 return frandom(5);
	      return result;
     default:
       (void) fprintf(stderr, "bad arg to frandom\n");
       exit(1);
   }
   return 0;
}

#define DISTRIB_A (halfrandom(7000) + 9000)
#define DISTRIB_B ((frandom(3) + 1) * (frandom(3) + 1) * 120000)
#define LEN(x) (sizeof(x)/sizeof((x)[0]))

static void
initmode(win, mode)
   Window win;
   int mode;
{
   flamestruct *fs = &flames[screen];
   static variation_distrib[] = {
      0, 0, 1, 1, 2, 2, 3,
      4, 4, 5, 5, 6, 6, 6};

   fs->mode = mode;

   fs->major_variation =
      variation_distrib[halfrandom(LEN(variation_distrib))];

   fs->rainbow = 0;
   if (mode) {
      if (!fs->color || halfrandom(8)) {
	 fs->nfractals = halfrandom(30) + 5;
	 fs->fractal_len = DISTRIB_A;
      } else {
	 fs->nfractals = halfrandom(5) + 5;
	 fs->fractal_len = DISTRIB_B;
      }
   } else {
      fs->rainbow = fs->color;
      fs->nfractals = 1;
      fs->fractal_len = DISTRIB_B;
   }

   fs->fractal_len = (fs->fractal_len * batchcount) / 20;
   
   XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
   XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, fs->width, fs->height);
}

static void
initfractal()
{
   static int xform_distrib[] = {2, 2, 2, 3, 3, 3, 4, 4, 5};
   flamestruct *fs = &flames[screen];
   int i, j, k;

   fs->fuse = FUSE;
   fs->total_points = 0;
   if (fs->rainbow)
      for (i = 0; i < Scr[screen].npixels; i++)
	 fs->ncpoints[i] = 0;
   else
      fs->npoints = 0;
   fs->nxforms = xform_distrib[halfrandom(LEN(xform_distrib))];
   fs->c = fs->x = fs->y = 0.0;
   for (i = 0; i < fs->nxforms; i++) {
      if (NMAJORVARS == fs->major_variation)
	 fs->variation[i] = halfrandom(NMAJORVARS);
      else
	 fs->variation[i] = fs->major_variation;
      for (j = 0; j < 2; j++)
	 for (k = 0; k < 3; k++) {
	    fs->f[j][k][i] = ((double) halfrandom(1000) / 500.0 - 1.0);
         }
   }
   if (fs->color)
      fs->pixcol = Scr[screen].pixels[halfrandom(Scr[screen].npixels)];
   else
      fs->pixcol = WhitePixel(dsp, screen);

}


void
initflame(win)
    Window      win;
{
   static int first_time = 1;
   flamestruct *fs = &flames[screen];
   XWindowAttributes xwa;
   
   (void) XGetWindowAttributes(dsp, win, &xwa);
   fs->width = xwa.width;
   fs->height = xwa.height;
   fs->color = Scr[screen].npixels > 2;

   if (first_time)
      initmode(win, 1);
   else
      initmode(win, frandom(2));
   initfractal();

   first_time = 0;
}

static void
iter(fs)
   flamestruct *fs;
{
   int i = frandom(fs->nxforms);
   double nx, ny, nc;

   if (i)
      nc = (fs->c + 1.0) / 2.0;
   else
      nc = fs->c / 2.0;

   nx = fs->f[0][0][i] * fs->x + fs->f[0][1][i] * fs->y + fs->f[0][2][i];
   ny = fs->f[1][0][i] * fs->x + fs->f[1][1][i] * fs->y + fs->f[1][2][i];


   switch (fs->variation[i]) {
    case 1:
      /* sinusoidal */
      nx = sin(nx);
      ny = sin(ny);
      break;
    case 2: {
       /* complex */
       double r2 = nx * nx + ny * ny + 1e-6;
       nx = nx / r2;
       ny = ny / r2;
       break;
    }
    case 3:
      /* bent */
      if (nx < 0.0) nx = nx * 2.0;
      if (ny < 0.0) ny = ny / 2.0;
      break;
    case 4: {
       /* swirl */
       double r = (nx * nx + ny * ny);  /* times k here is fun */
       double c1 = sin(r);
       double c2 = cos(r);
       double t = nx;
       
       nx = c1 * nx - c2 * ny;
       ny = c2 * t  + c1 * ny;
       break;
    }
    case 5: {
       /* horseshoe */
       double r = atan2(nx, ny);  /* times k here is fun */
       double c1 = sin(r);
       double c2 = cos(r);
       double t = nx;
       
       nx = c1 * nx - c2 * ny;
       ny = c2 * t  + c1 * ny;
       break;
    }
    case 6: {
       /* drape */
       double t;
       t = atan2(nx, ny)/M_PI;
       ny = sqrt(nx * nx + ny * ny) - 1.0;
       nx = t;
       break;
    }
   }

#if 0
      /* here are some others */
{
   /* broken */
   if (nx >  1.0) nx = nx - 1.0;
   if (nx < -1.0) nx = nx + 1.0;
   if (ny >  1.0) ny = ny - 1.0;
   if (ny < -1.0) ny = ny + 1.0;
   break;
}
{
   /* complex sine */
   double u = nx, v = ny;
   double ev = exp(v);
   double emv = exp(-v);

   nx = (ev + emv) * sin(u) / 2.0;
   ny = (ev - emv) * cos(u) / 2.0;
}
{

   /* polynomial */
   if (nx < 0) nx = -nx * nx;
   else        nx =  nx * nx;

   if (ny < 0) ny = -ny * ny;
   else        ny =  ny * ny;
}
{
   /* spherical */
   double r = 0.5 + sqrt(nx * nx + ny * ny + 1e-6);
   nx = nx / r;
   ny = ny / r;
}
{
   nx = atan(nx)/M_PI_2
   ny = atan(ny)/M_PI_2
}
#endif

   if (nx > 1e10 || nx < -1e10 || ny > 1e10 || ny < -1e10) {
      nx = halfrandom(1000) / 500.0 - 1.0;
      ny = halfrandom(1000) / 500.0 - 1.0;
      fs->fuse = FUSE;
   }

   fs->x = nx;
   fs->y = ny;
   fs->c = nc;

}

static void draw(fs, d)
   flamestruct *fs;
   Drawable d;

{
   double x = fs->x;
   double y = fs->y;
   int fixed_x, fixed_y, npix, c, n;

   if (fs->fuse) {
      fs->fuse--;
      return;
   }

   if (!(x > -1.0 && x < 1.0 && y > -1.0 && y < 1.0))
      return;
   
   fixed_x = (int) ((fs->width  / 2) * (x + 1.0));
   fixed_y = (int) ((fs->height / 2) * (y + 1.0));

   if (!fs->rainbow) {
      
      fs->pts[fs->npoints].x = fixed_x;
      fs->pts[fs->npoints].y = fixed_y;
      fs->npoints++;
      if (fs->npoints == MAXBATCH1) {
	 XSetForeground(dsp, Scr[screen].gc, fs->pixcol);
	 XDrawPoints(dsp, d, Scr[screen].gc, fs->pts,
		     fs->npoints, CoordModeOrigin);
	 fs->npoints = 0;
      }
   } else {
      
      npix = Scr[screen].npixels;
      c = fs->c * npix;
      
      if (c < 0) c = 0;
      if (c >= npix) c = npix-1;
      n = fs->ncpoints[c];
      fs->cpts[c][n].x = fixed_x;
      fs->cpts[c][n].y = fixed_y;
      if (++fs->ncpoints[c] == MAXBATCH2) {
	 XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[c]);
	 XDrawPoints(dsp, d, Scr[screen].gc, fs->cpts[c],
		     fs->ncpoints[c], CoordModeOrigin);
	 fs->ncpoints[c] = 0;
      }
   }
}

static void draw_flush(fs, d)
   flamestruct *fs;
   Drawable d;

{
   if (fs->rainbow) {
      int npix = Scr[screen].npixels;
      int i;
      for (i = 0; i < npix; i++) {
	 if (fs->ncpoints[i]) {
	    XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[i]);
	    XDrawPoints(dsp, d, Scr[screen].gc, fs->cpts[i],
			fs->ncpoints[i], CoordModeOrigin);
	    fs->ncpoints[i] = 0;
	 }
      }
   } else {
      if (fs->npoints)
	 XSetForeground(dsp, Scr[screen].gc, fs->pixcol);
	 XDrawPoints(dsp, d, Scr[screen].gc, fs->pts,
		     fs->npoints, CoordModeOrigin);
      fs->npoints = 0;
   }
}   


void
drawflame(win)
    Window      win;
{
   flamestruct *fs = &flames[screen];

   fs->timer = batchcount * 1000;

   while (fs->timer) {
      iter(fs);
      draw(fs, win);
      if (fs->total_points++ > fs->fractal_len) {
	 draw_flush(fs, win);
	 if (0 == --fs->nfractals)
	    initmode(win, frandom(2));
	 initfractal();
      }
      
      fs->timer--;
   }
}
