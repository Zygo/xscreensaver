#ifndef lint
static char sccsid[] = "@(#)rect.c 1.3 92/01/01 XLOCK";
#endif
/*-
 * Copyright (c) 1992 by Jamie Zawinski
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 29-Sep-94: multidisplay bug fix (epstein_caleb@jpmorgan.com)
 * 15-Jul-94: xlock version (David Bagley bagleyd@source.asset.com)
 * 1992:     xscreensaver version (Jamie Zawinski jwz@lucid.com)
 */

/* original copyright
 * xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include	"xlock.h"

#define NBITS 12
#define MAXTIME 256
 
#include <X11/bitmaps/stipple>
#include <X11/bitmaps/cross_weave>
#include <X11/bitmaps/dimple1>
#include <X11/bitmaps/dimple3>
#include <X11/bitmaps/flipped_gray>
#include <X11/bitmaps/gray1>
#include <X11/bitmaps/gray3>
#include <X11/bitmaps/hlines2>
#include <X11/bitmaps/light_gray>
#include <X11/bitmaps/root_weave>
#include <X11/bitmaps/vlines2>
#include <X11/bitmaps/vlines3>

#define BITS(n,w,h)\
  rp->pixmaps[rp->init_bits++]=\
  XCreatePixmapFromBitmapData(dsp,win,(char *)n,w,h,1,0,1)

typedef struct {
    int		width;
    int		height;
    int		color;
    int		time;
    int		init_bits;
    int		x, y, w, h;
    GC		stippled_GC;
    Pixmap	pixmaps[NBITS];
} rectstruct;

static rectstruct rects[MAXSCREENS];

void
initrect(win)
    Window      win;
{
    XGCValues gcv;
    Screen *scr;
    XWindowAttributes xgwa;
    rectstruct *rp = &rects[screen];

    XGetWindowAttributes(dsp, win, &xgwa);
    scr = ScreenOfDisplay(dsp, screen);
    rp->width = xgwa.width;
    rp->height = xgwa.height;
    rp->time = 0;

    gcv.foreground = WhitePixelOfScreen(scr);
    gcv.background = BlackPixelOfScreen(scr); 
    gcv.fill_style = FillOpaqueStippled;
    rp->stippled_GC = XCreateGC(dsp, win,
      GCForeground|GCBackground|GCFillStyle, &gcv);

    if (!rp->init_bits) {
      BITS(stipple_bits, stipple_width, stipple_height);
      BITS(cross_weave_bits, cross_weave_width, cross_weave_height);
      BITS(dimple1_bits, dimple1_width, dimple1_height);
      BITS(dimple3_bits, dimple3_width, dimple3_height);
      BITS(flipped_gray_bits, flipped_gray_width, flipped_gray_height);
      BITS(gray1_bits, gray1_width, gray1_height);
      BITS(gray3_bits, gray3_width, gray3_height);
      BITS(hlines2_bits, hlines2_width, hlines2_height);
      BITS(light_gray_bits, light_gray_width, light_gray_height);
      BITS(root_weave_bits, root_weave_width, root_weave_height);
      BITS(vlines2_bits, vlines2_width, vlines2_height);
      BITS(vlines3_bits, vlines3_width, vlines3_height);
    }
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, rp->width, rp->height);
}

void
drawrect(win)
    Window      win;
{
    XGCValues gcv;
    rectstruct *rp = &rects[screen];
    int i;

  for (i = 0; i < 10; i++) /* minimize area, but don't try too hard */ {
      rp->w = 50 + random () % (rp->width - 50);
      rp->h = 50 + random () % (rp->height - 50);
      if (rp->w + rp->h < rp->width && rp->w + rp->h < rp->height)
        break;
  }
  rp->x = random () % (rp->width - rp->w);
  rp->y = random () % (rp->height - rp->h);
  gcv.stipple = rp->pixmaps[random() % NBITS];
  gcv.foreground = (!mono && Scr[screen].npixels > 2) ?
    random() % Scr[screen].npixels : WhitePixel(dsp, screen);
  gcv.background = (!mono && Scr[screen].npixels > 2) ?
    random() % Scr[screen].npixels : BlackPixel(dsp, screen);
  XChangeGC (dsp, rp->stippled_GC, GCStipple|GCForeground|GCBackground, &gcv);
  XFillRectangle (dsp, win, rp->stippled_GC, rp->x, rp->y, rp->w, rp->h);

    if (++rp->time > MAXTIME)
      initrect(win);
}
