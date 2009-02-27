#ifndef lint
static char sccsid[] = "@(#)blot.c	2.7 95/02/21 xlockmore";
#endif
/*
 * Rorschach's ink blot test
 *
 * Copyright (c) 1992 by Jamie Zawinski
 *
 * 05-Jan-95: patch for Dual-Headed machines from Greg Onufer
 *            <Greg.Onufer@Eng.Sun.COM>
 * 07-Dec-94: now randomly has xsym, ysym, or both.
 * 02-Sep-93: xlock version (David Bagley bagleyd@source.asset.com)
 * 1992:      xscreensaver version (Jamie Zawinski jwz@lucid.com)
 */

/* original copyright
 * Copyright (c) 1992 by Jamie Zawinski
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include "xlock.h"

typedef struct {
    int width;
    int height;
    int xmid, ymid;
    int offset;
    int xsym, ysym;
    int size;
    int pix;
    long startTime;
} blotstruct;

static blotstruct blots[MAXSCREENS];
static XPoint *pointBuffer = 0; /* pointer for XDrawPoints */
static unsigned long pointBufferSize = 0;

#define TIMEOUT 30

void
initblot(win)
     Window win;
{
    XWindowAttributes xgwa;
    blotstruct *bp = &blots[screen];

    (void) XGetWindowAttributes(dsp, win, &xgwa);
    bp->width = xgwa.width;
    bp->height = xgwa.height;
    bp->xmid = bp->width / 2;
    bp->ymid = bp->height / 2;

    bp->offset = 4;
    bp->ysym = RAND() % 2;
    bp->xsym = (bp->ysym) ? RAND() % 2 : 1;
    bp->pix = 0;
    if (bp->offset <= 0) bp->offset = 3;
    if (batchcount >= 100 || batchcount < 1)
      batchcount = 6;
    /* Fudge the size so it takes up the whole screen */
    bp->size = batchcount * bp->width * bp->height / 1024;

    if (!pointBuffer || pointBufferSize < (bp->size * sizeof(XPoint))) {
      if (pointBuffer != NULL)
        free(pointBuffer);
      pointBuffer = (XPoint *) malloc(bp->size * sizeof(XPoint));
      pointBufferSize = bp->size * sizeof(XPoint);
    }

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, bp->width, bp->height);
    XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    bp->startTime = seconds();
}

void
drawblot(win)
     Window win;
{
  int x, y;
  int         k;
  XPoint     *xp = pointBuffer;
  blotstruct *bp = &blots[screen];

  if (!mono && Scr[screen].npixels > 2) {
         XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[bp->pix]);
         if (++bp->pix >= Scr[screen].npixels)
             bp->pix = 0;
  }

  x = bp->xmid;
  y = bp->ymid;
  k = bp->size;
  while (k >= 4) {
      x += ((RAND() % (1 + (bp->offset << 1))) - bp->offset);
      y += ((RAND() % (1 + (bp->offset << 1))) - bp->offset);
      k--;
      xp->x = x;
      xp->y = y;
      xp++;
      if (bp->xsym)
	{
          k--;
	  xp->x = bp->width - x;
	  xp->y = y;
	  xp++;
	}
      if (bp->ysym)
	{
          k--;
	  xp->x = x;
	  xp->y = bp->height - y;
	  xp++;
	}
      if (bp->xsym && bp->ysym)
	{
          k--;
	  xp->x = bp->width - x;
	  xp->y = bp->height - y;
	  xp++;
	}
    }
      XDrawPoints (dsp, win, Scr[screen].gc,
         pointBuffer, bp->size - k, CoordModeOrigin);
    if (seconds() - bp->startTime > TIMEOUT)
        initblot(win);
}
