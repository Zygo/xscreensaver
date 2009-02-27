#ifndef lint
static char sccsid[] = "@(#)world.c	1.7 91/05/24 XLOCK";
#endif
/*
 * world.c - World Spinner for Xlock
 *
 * Copyright (c) 1993 Matthew Moyle-Croft <mmc@cs.adelaide.edu.au>
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-Jul-94 Got batchcount to work.
 * 09-Jan-94 Written [ Modified from image.c ]
 * 29-Jul-90 image.c written. Copyright (c) 1991 by Patrick J. Naughton.
 */

#include "xlock.h"
#define NUM_EARTHS 30
#define SIZE_X 64
#define SIZE_Y 64
#define NUM_REV 4

#include <math.h>
#include "bitmaps/terra-00.xbm"
#include "bitmaps/terra-01.xbm"
#include "bitmaps/terra-02.xbm"
#include "bitmaps/terra-03.xbm"
#include "bitmaps/terra-04.xbm"
#include "bitmaps/terra-05.xbm"
#include "bitmaps/terra-06.xbm"
#include "bitmaps/terra-07.xbm"
#include "bitmaps/terra-08.xbm"
#include "bitmaps/terra-09.xbm"
#include "bitmaps/terra-10.xbm"
#include "bitmaps/terra-11.xbm"
#include "bitmaps/terra-12.xbm"
#include "bitmaps/terra-13.xbm"
#include "bitmaps/terra-14.xbm"
#include "bitmaps/terra-15.xbm"
#include "bitmaps/terra-16.xbm"
#include "bitmaps/terra-17.xbm"
#include "bitmaps/terra-18.xbm"
#include "bitmaps/terra-19.xbm"
#include "bitmaps/terra-20.xbm"
#include "bitmaps/terra-21.xbm"
#include "bitmaps/terra-22.xbm"
#include "bitmaps/terra-23.xbm"
#include "bitmaps/terra-24.xbm"
#include "bitmaps/terra-25.xbm"
#include "bitmaps/terra-26.xbm"
#include "bitmaps/terra-27.xbm"
#include "bitmaps/terra-28.xbm"
#include "bitmaps/terra-29.xbm"

static XImage Earths[NUM_EARTHS] =
{ 
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra00_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra01_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra02_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra03_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra04_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra05_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra06_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra07_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra08_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra09_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra10_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra11_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra12_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra13_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra14_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra15_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra16_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra17_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra18_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra19_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra20_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra21_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra22_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra23_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra24_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra25_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra26_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra27_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra28_bits,LSBFirst,8,LSBFirst,8,1},
  {SIZE_X,SIZE_Y,0, XYBitmap, (char *) terra29_bits,LSBFirst,8,LSBFirst,8,1}};

#define MAXICONS 64

typedef struct {
  int         x;
  int         y;
  unsigned long color;
  int         frame;
  int	      direction;
}           point;

typedef struct {
    int         width;
    int         height;
    int         nrows;
    int         ncols;
    int         xb;
    int         yb;
    int         iconmode;
    int         iconcount;
    point       icons[MAXICONS];
}           worldstruct;

static worldstruct worlds[MAXSCREENS];

static int frame_num;

void
drawworld(win)
    Window      win;
{
    worldstruct *wp = &worlds[screen];
    int         i;
#ifndef NOFLASH
    int col[MAXICONS], j;

    for (i = 0; i < wp->ncols; i++)
      col[i] = 0;
#endif
    if (frame_num == NUM_EARTHS * NUM_REV) {
	frame_num = 0;
	XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
	for (i = 0; i < wp->iconcount; i++) {
	    if (!wp->iconmode)
	      XFillRectangle(dsp, win, Scr[screen].gc,
			     wp->xb + SIZE_X * wp->icons[i].x,
			     wp->yb + SIZE_Y * wp->icons[i].y,
			     SIZE_X, SIZE_Y);
#ifdef NOFLASH
	    wp->icons[i].x = random() % wp->ncols;
#else
	    do {
              j = random() % wp->ncols;
              if (!col[j])
                wp->icons[i].x = j;
              col[j]++;
            } while (col[j] > 1);
#endif
	    wp->icons[i].y = random() % wp->nrows;
	    wp->icons[i].direction = random() % 2;
	    wp->icons[i].frame = random() % NUM_EARTHS;
            if (!mono && Scr[screen].npixels > 2) {
              wp->icons[i].color = random() % Scr[screen].npixels;
              if (wp->icons[i].color == BlackPixel(dsp, screen))
                wp->icons[i].color = WhitePixel(dsp, screen);
            } else
              wp->icons[i].color = WhitePixel(dsp, screen);
	  }
      }
    for(i = 0; i< wp->iconcount;i++) {
	XSetForeground(dsp, Scr[screen].gc, wp->icons[i].color);
	if ( wp->icons[i].frame == NUM_EARTHS)
		wp->icons[i].frame = 0;
	else {
		if ((wp->icons[i].frame <= 0) && wp->icons[i].direction == 0)
			wp->icons[i].frame = NUM_EARTHS - 1;
	}
	XPutImage(dsp, win, Scr[screen].gc, (Earths + wp->icons[i].frame),
		  0, 0,
		  wp->xb + SIZE_X * wp->icons[i].x,
		  wp->yb + SIZE_Y * wp->icons[i].y,
		  SIZE_X, SIZE_Y);

	(wp->icons[i].direction) ? wp->icons[i].frame++ : wp->icons[i].frame--; 
      }	       
    frame_num++;
  }

void
initworld(win)
    Window      win;
{
    int x;
    XWindowAttributes xgwa;
    worldstruct *wp = &worlds[screen];
 
    frame_num = NUM_EARTHS * NUM_REV;
    for (x = 0; x < NUM_EARTHS; x++)
      Earths[x].bytes_per_line = 8;
    XGetWindowAttributes(dsp, win, &xgwa);
    wp->width = xgwa.width;
    wp->height = xgwa.height;
    wp->ncols = wp->width / SIZE_X;
    wp->nrows = wp->height / SIZE_Y;
    wp->iconmode = (wp->ncols < 2 || wp->nrows < 2);
    if (wp->iconmode) {
	wp->xb = 0;
	wp->yb = 0;
	wp->iconcount = 1;	/* icon mode */
    } else {
	wp->xb = (wp->width - SIZE_X * wp->ncols) / 2;
	wp->yb = (wp->height - SIZE_Y * wp->nrows) / 2;
#ifdef NOFLASH
        wp->iconcount = batchcount;
        if (wp->iconcount > MAXICONS)
          wp->iconcount = 16;
#else
        wp->iconcount = batchcount;
        if (wp->iconcount > MAXICONS)
          wp->iconcount = 8;
        if (wp->iconcount > wp->ncols)
	  wp->iconcount = wp->ncols;
#endif
    }
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, wp->width, wp->height);
}
