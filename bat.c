#ifndef lint
static  char sccsid[] = "@(#)bat.c 1.3 88/02/26 XLOCK";
#endif

/*-
 * bat.c - A bouncing bat for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1988 by Sun Microsystems
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 20-Sep-94: 8 bats instead of bouncing balls, based on bounce.c
	      (patol@info.isbiel.ch)
 * 2-Sep-93: bounce version (David Bagley bagleyd@source.asset.com)
 * 1986: Sun Microsystems
 */

/* original copyright
 ******************************************************************************
 Copyright 1988 by Sun Microsystems, Inc. Mountain View, CA.

 All Rights Reserved

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and that
 both that copyright notice and this permission notice appear in
 supporting documentation, and that the names of Sun or MIT not be
 used in advertising or publicity pertaining to distribution of the
 software without specific prior written permission. Sun and M.I.T.
 make no representations about the suitability of this software for
 any purpose. It is provided "as is" without any express or implied warranty.

 SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE. IN NO EVENT SHALL SUN BE LIABLE FOR ANY SPECIAL, INDIRECT
 OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

#include "xlock.h"
#include <math.h>
#include "bitmaps/bat0.bit"
#include "bitmaps/bat1.bit"
#include "bitmaps/bat2.bit"
#include "bitmaps/bat3.bit"
#include "bitmaps/bat4.bit"
#include "bitmaps/bat5.bit"
#include "bitmaps/bat6.bit"
#include "bitmaps/bat7.bit"

#define MAX_STRENGTH 24
#define FRICTION 15
#define PENETRATION 0.4
#define SLIPAGE 4
#define TIME 10

#define ORIENTS 8
#define ORIENTCYCLE 32
#define CCW 1
#define CW (ORIENTS-1) 
#define DIR(x)	(((x)>=0)?CCW:CW)
#define SIGN(x)	(((x)>=0)?1:-1)
#define ABS(x)	(((x)>=0)?x:-(x))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define TRUE 1
#define FALSE 0

static XImage logo[ORIENTS] = {
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1}
}; 

typedef struct {
    int x, y, xlast, ylast;
    int spincount, spindelay, spindir, orient;
    int vx, vy, vang;
    int mass, size, sizex, sizey;
    unsigned long color;
} batstruct;

typedef struct {
    int width, height;
    int nbats;
    batstruct *bats;
    GC draw_GC, erase_GC;
} bouncestruct;

static bouncestruct bounces[MAXSCREENS];

static void checkCollision();
static void draw_bat();
static void movebat();
static void flapbat();
static int collide();
static void XEraseImage();

void
initbat(win)
    Window win;
{
    XWindowAttributes xgwa;
    XGCValues gcv;
    Screen *scr;
    bouncestruct *bp = &bounces[screen];
    int i;

    logo[0].data = (char *) bat0_bits;
    logo[0].width = bat0_width;
    logo[0].height = bat0_height;
    logo[0].bytes_per_line = (bat0_width + 7) / 8;
    logo[1].data = (char *) bat1_bits;
    logo[1].width = bat1_width;
    logo[1].height = bat1_height;
    logo[1].bytes_per_line = (bat1_width + 7) / 8;
    logo[2].data = (char *) bat2_bits;
    logo[2].width = bat2_width;
    logo[2].height = bat2_height;
    logo[2].bytes_per_line = (bat2_width + 7) / 8;
    logo[3].data = (char *) bat3_bits;
    logo[3].width = bat3_width;
    logo[3].height = bat3_height;
    logo[3].bytes_per_line = (bat3_width + 7) / 8;
    logo[4].data = (char *) bat4_bits;
    logo[4].width = bat4_width;
    logo[4].height = bat4_height;
    logo[4].bytes_per_line = (bat4_width + 7) / 8;
    logo[5].data = (char *) bat5_bits;
    logo[5].width = bat5_width;
    logo[5].height = bat5_height;
    logo[5].bytes_per_line = (bat5_width + 7) / 8;
    logo[6].data = (char *) bat6_bits;
    logo[6].width = bat6_width;
    logo[6].height = bat6_height;
    logo[6].bytes_per_line = (bat6_width + 7) / 8;
    logo[7].data = (char *) bat7_bits;
    logo[7].width = bat7_width;
    logo[7].height = bat7_height;
    logo[7].bytes_per_line = (bat7_width + 7) / 8;

    XGetWindowAttributes(dsp, win, &xgwa);
    scr = ScreenOfDisplay(dsp, screen);
    bp->width = xgwa.width;
    bp->height = xgwa.height;

    gcv.background = BlackPixelOfScreen(scr);
    bp->draw_GC = XCreateGC(dsp, win, GCForeground|GCBackground, &gcv);
    bp->erase_GC = XCreateGC(dsp, win, GCForeground|GCBackground, &gcv);
    if (batchcount < 1)
      batchcount = 4;
    bp->nbats =batchcount; 
    if (!bp->bats)
      bp->bats = (batstruct *) malloc(batchcount * sizeof(batstruct));
    i = 0;
    while (i < bp->nbats) {
      if (logo[0].width > bp->width / 2 || logo[0].height > bp->height / 2)
        bp->bats[i].size = 5;
      else {
        bp->bats[i].size = (logo[0].width + logo[0].height) / 2;
        bp->bats[i].sizex = logo[0].width;
        bp->bats[i].sizey = logo[0].height;
      }
      bp->bats[i].vx = ((random() % 2) ? -1 : 1) *
        (random() % MAX_STRENGTH + 1);
      bp->bats[i].x = (bp->bats[i].vx >= 0) ?
        0 : bp->width - bp->bats[i].sizex;
      bp->bats[i].y = random() % (bp->height / 2);
      if (i == collide(i)) {
        if (!mono && Scr[screen].npixels > 2) {
          bp->bats[i].color = random() % Scr[screen].npixels;
          if (bp->bats[i].color == BlackPixel(dsp, screen))
            bp->bats[i].color = WhitePixel(dsp, screen);
        } else
          bp->bats[i].color = WhitePixel(dsp, screen);
        bp->bats[i].xlast = -1;
        bp->bats[i].ylast = 0;
        bp->bats[i].spincount = 1;
        bp->bats[i].spindelay = 1;
        bp->bats[i].vy = ((random() % 2) ? -1 : 1) * (random() % MAX_STRENGTH);
        bp->bats[i].spindir = 0;
        bp->bats[i].vang = 0;
        bp->bats[i].orient = random() % ORIENTS;
        i++;
      } else
        bp->nbats--;
    }
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, bp->width, bp->height);
    XSetForeground(dsp, bp->erase_GC, BlackPixel(dsp, screen));
}

static void checkCollision(a_bat)
        int a_bat;
{
        bouncestruct *bp = &bounces[screen];
        int i, amount, spin, d, size;
        double x, y;

        for (i = 0; i < bp->nbats; i++) {
          if (i != a_bat) {
            x = (double) (bp->bats[i].x - bp->bats[a_bat].x);
            y = (double) (bp->bats[i].y - bp->bats[a_bat].y);
            d = (int) sqrt(x * x + y * y);
            size = (bp->bats[i].size + bp->bats[a_bat].size) / 2;
            if (d > 0 && d < size) {
              amount = size - d;
              if (amount > PENETRATION * size)
                amount = PENETRATION * size;
              bp->bats[i].vx += amount * x / d;
              bp->bats[i].vy += amount * y / d;
              bp->bats[i].vx -= bp->bats[i].vx / FRICTION;
              bp->bats[i].vy -= bp->bats[i].vy / FRICTION;
              bp->bats[a_bat].vx -= amount * x / d;
              bp->bats[a_bat].vy -= amount * y / d;
              bp->bats[a_bat].vx -= bp->bats[a_bat].vx / FRICTION;
              bp->bats[a_bat].vy -= bp->bats[a_bat].vy / FRICTION;
              spin = (bp->bats[i].vang - bp->bats[a_bat].vang) /
                (2 * size * SLIPAGE);
              bp->bats[i].vang -= spin;
              bp->bats[a_bat].vang += spin;
              bp->bats[i].spindir = DIR(bp->bats[i].vang);
              bp->bats[a_bat].spindir = DIR(bp->bats[a_bat].vang);
              if (!bp->bats[i].vang) {
                bp->bats[i].spindelay = 1;
                bp->bats[i].spindir = 0;
              } else
                bp->bats[i].spindelay = M_PI * bp->bats[i].size /
                  (ABS(bp->bats[i].vang)) + 1;
              if (!bp->bats[a_bat].vang) {
                bp->bats[a_bat].spindelay = 1;
                bp->bats[a_bat].spindir = 0;
              } else
                bp->bats[a_bat].spindelay = M_PI * bp->bats[a_bat].size /
                  (ABS(bp->bats[a_bat].vang)) + 1;
              return;
            }
          }
        }
}

void
drawbat(win)
    Window win;
{
       bouncestruct *bp = &bounces[screen];
       int i;
       static restartnum = TIME;

       for (i = 0; i < bp->nbats; i++) {
         draw_bat(win, &bp->bats[i]);
         movebat(&bp->bats[i]);
       }
       for (i = 0; i < bp->nbats; i++)
         checkCollision(i);
       if (!(random() % TIME *100)/100) /* Put some randomness into the time */
         restartnum--;
       if (!restartnum) {
	 initbat(win);
         restartnum = TIME;
       }

}

static void
draw_bat(win, bat)
    Window win;
    batstruct *bat;
{        
        bouncestruct *bp = &bounces[screen];

        XSetForeground(dsp, bp->draw_GC, bat->color);
        if (bat->size <= 5) {
	  if (bat->xlast != -1)
            XFillRectangle(dsp, win, bp->erase_GC,
	      bat->xlast, bat->ylast, bat->size, bat->size);
	  XFillRectangle(dsp, win, bp->draw_GC, 
	    bat->x, bat->y, bat->size, bat->size);
        } else {
          XPutImage(dsp, win, bp->draw_GC, &logo[bat->orient],
            0, 0, bat->x, bat->y, bat->sizex, bat->sizey);
	  if (bat->xlast != -1)
            XEraseImage(dsp, win, bp->erase_GC,
              bat->x, bat->y,
              bat->xlast, bat->ylast, bat->sizex, bat->sizey);
        }
}

static void
movebat(bat)
    batstruct *bat;
{        
        bouncestruct *bp = &bounces[screen];

        bat->xlast = bat->x;
        bat->ylast = bat->y;
        bat->x += bat->vx;
        if (bat->x > (bp->width - bat->sizex)) {
          /* Bounce off the right edge */
          bat->x = 2 * (bp->width - bat->sizex) - bat->x;
          bat->vx = -bat->vx + bat->vx / FRICTION;
          flapbat(bat, 1, &bat->vy);
        } else if (bat->x < 0) {
          /* Bounce off the left edge */
          bat->x = -bat->x;
          bat->vx = -bat->vx + bat->vx / FRICTION;
          flapbat(bat, -1, &bat->vy);
        }
        bat->vy++;
        bat->y += bat->vy;
        if (bat->y >= (bp->height /*- bat->sizey*/)) {
          /* Bounce off the bottom edge */
          bat->y = (bp->height - bat->sizey);
          bat->vy = -bat->vy + bat->vy / FRICTION;
          flapbat(bat, -1, &bat->vx);
        } else if (bat->y < 0) {
          /* Bounce off the top edge */
	  /*bat->y = -bat->y;
          bat->vy = -bat->vy + bat->vy / FRICTION;
          flapbat(bat, 1, &bat->vx);*/
        }
        if (bat->spindir) {
          bat->spincount--;
          if (!bat->spincount) {
            bat->orient = (bat->spindir + bat->orient) % ORIENTS;
            bat->spincount = bat->spindelay;
          }
        }
}

static void
flapbat(bat, dir, vel)
    batstruct *bat;
    int dir;
    int *vel;
{
        *vel -= (*vel + SIGN(*vel * dir) * bat->spindelay * ORIENTCYCLE /
           (M_PI * bat->size)) / SLIPAGE;
        if (*vel) {
          bat->spindir = DIR(*vel * dir);
          bat->vang = *vel * ORIENTCYCLE;
          bat->spindelay = M_PI * bat->size / (ABS(bat->vang)) + 1;
        } else
          bat->spindir = 0;
}

static int collide(a_bat)
        int a_bat;
{
        bouncestruct *bp = &bounces[screen];
        int i, d, x, y;

        for (i = 0; i < a_bat; i++) {
          x = (bp->bats[i].x - bp->bats[a_bat].x);
          y = (bp->bats[i].y - bp->bats[a_bat].y);
          d = (int) sqrt((double) (x * x + y * y));
          if (d < (bp->bats[i].size + bp->bats[a_bat].size) / 2)
            return i;
        }
        return i;
}

/* This stops some flashing, could be more efficient */
static void
XEraseImage(display, win, gc, x, y, xlast, ylast, xsize, ysize)
Display *display;
Window win;
GC gc;
int x, y, xlast, ylast, xsize, ysize;
{
  if (ylast < y) {
    if (y < ylast + ysize)
      XFillRectangle(display, win, gc, xlast, ylast, xsize, y - ylast);
    else
      XFillRectangle(display, win, gc, xlast, ylast, xsize, ysize);
  } else if (ylast > y) {
    if (y > ylast - ysize)
      XFillRectangle(display, win, gc, xlast, y + ysize, xsize, ylast - y);
    else
      XFillRectangle(display, win, gc, xlast, ylast, xsize, ysize);
  }
  if (xlast < x) {
    if (x < xlast + xsize)
      XFillRectangle(display, win, gc, xlast, ylast, x - xlast, ysize);
    else
      XFillRectangle(display, win, gc, xlast, ylast, xsize, ysize);
  } else if (xlast > x) {
    if (x > xlast - xsize)
      XFillRectangle(display, win, gc, x + xsize, ylast, xlast - x, ysize);
    else
      XFillRectangle(display, win, gc, xlast, ylast, xsize, ysize);
  }
}
