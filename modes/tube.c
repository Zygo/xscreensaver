/* -*- Mode: C; tab-width: 4 -*- */
/* tube --- animated tube */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tube.c	5.09 2003/06/30 xlockmore";

#endif

/*-
 * Copyright (c) 1997 Dan Stromberg <strombrg@nis.acs.uci.edu>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 30-Jun-2003: Changed writeable mode to be more consistent with
 *              xscreensaver's starfish
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 * 04-Mar-1997: Memory leak fix by Tom Schmidt <tschmidt@micron.com>
 * 07-Feb-1997: Written by Dan Stromberg <strombrg@nis.acs.uci.edu>
 */


#ifdef STANDALONE
#define MODE_tube
#define PROGCLASS "Tube"
#define HACK_INIT init_tube
#define HACK_DRAW draw_tube
#define tube_opts xlockmore_opts
#define DEFAULTS "*delay: 25000 \n" \
 "*cycles: 20000 \n" \
 "*size: -200 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#define WRITABLE_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_tube

#define DEF_CYCLE "True"

static Bool cycle_p;

static XrmOptionDescRec opts[] =
{
  {(char *) "-cycle", (char *) ".tube.cycle", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+cycle", (char *) ".tube.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
  {(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-/+cycle", (char *) "turn on/off colour cycling"}
};

ModeSpecOpt tube_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   tube_description =
{"tube", "init_tube", "draw_tube", "release_tube",
 NULL, "init_tube", (char *) NULL, &tube_opts,
 25000, -9, 20000, -200, 64, 1.0, "",
 "Shows an animated tube", 0, NULL};

#endif

#define MINSIZE 3
#define MINSHAPE 1

typedef struct {
	int         dx1, dy1;
	int         x1, y1;
	int         width, height, average, linewidth;
	int         shape;
	XPoint     *pts, *proto_pts;
	unsigned int cur_color;
	GC          gc;
	Colormap    cmap;
	unsigned long blackpixel, whitepixel, fg, bg;
	int direction;
	XColor     *colors;
	int         counter;
	int         ncolors;
	Bool        cycle_p, mono_p, no_colors, reverse;
	ModeInfo   *mi;
} tubestruct;

static tubestruct *tubes = (tubestruct *) NULL;

static void
free_pts(tubestruct *tp)
{
	if (tp->pts != NULL) {
		free(tp->pts);
		tp->pts = (XPoint *) NULL;
	}
	if (tp->proto_pts != NULL) {
		free(tp->proto_pts);
		tp->proto_pts = (XPoint *) NULL;
	}
}

static void
free_tube(Display *display,  tubestruct *tp)
{
  ModeInfo *mi = tp->mi;

  free_pts(tp);

  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    MI_WHITE_PIXEL(mi) = tp->whitepixel;
    MI_BLACK_PIXEL(mi) = tp->blackpixel;
#ifndef STANDALONE
    MI_FG_PIXEL(mi) = tp->fg;
    MI_BG_PIXEL(mi) = tp->bg;
#endif
    if (tp->colors != NULL) {
      if (tp->ncolors && !tp->no_colors)
        free_colors(display, tp->cmap, tp->colors, tp->ncolors);
      free(tp->colors);
      tp->colors = (XColor *) NULL;
    }
    if (tp->cmap != None) {
      XFreeColormap(display, tp->cmap);
      tp->cmap = None;
    }
  }
  if (tp->gc != None) {
     XFreeGC(display, tp->gc);
     tp->gc = None;
  }
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

void
init_tube(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         screen_width, screen_height;
	int         size = MI_SIZE(mi);
	int         star = 1, lw;
	tubestruct *tp;

	if (tubes == NULL) {
		if ((tubes = (tubestruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (tubestruct))) == NULL)
			return;
	}
	tp = &tubes[MI_SCREEN(mi)];
	tp->mi = mi;

	screen_width = MI_WIDTH(mi);
	screen_height = MI_HEIGHT(mi);

	if (size < -MINSIZE) {
		tp->width = NRAND(MIN(-size, MAX(MINSIZE, screen_width / 2)) -
				  MINSIZE + 1) + MINSIZE;
		tp->height = NRAND(MIN(-size, MAX(MINSIZE, screen_height / 2)) -
				   MINSIZE + 1) + MINSIZE;
	} else if (size < MINSIZE) {
		if (!size) {
			tp->width = MAX(MINSIZE, screen_width / 2);
			tp->height = MAX(MINSIZE, screen_height / 2);
		} else {
			tp->width = MINSIZE;
			tp->height = MINSIZE;
		}
	} else {
		tp->width = MIN(size, MAX(MINSIZE, screen_width / 2));
		tp->height = MIN(size, MAX(MINSIZE, screen_height / 2));
	}
	tp->average = (tp->width + tp->height) / 2;
	tp->reverse = (Bool) (LRAND() & 1);
	tp->dx1 = LRAND() & 1;
	if (tp->dx1 == 0)
		tp->dx1 = -1;
	tp->dy1 = LRAND() & 1;
	if (tp->dy1 == 0)
		tp->dy1 = -1;
	free_pts(tp);
	tp->shape = MI_COUNT(mi);
	if (tp->shape < -MINSHAPE) {
		tp->shape = NRAND(-tp->shape - MINSHAPE + 1) + MINSHAPE;
	} else if (tp->shape < MINSHAPE)
		tp->shape = MINSHAPE;

	if (tp->shape >= 3) {
		int         i;
		float       start;

		tp->width = tp->height = tp->average;
		if (((tp->pts = (XPoint *) malloc((tp->shape + 1) *
			 sizeof (XPoint))) == NULL) ||
		    ((tp->proto_pts = (XPoint *) malloc((tp->shape + 1) *
			 sizeof (XPoint))) == NULL)) {
			free_tube(display, tp);
			return;
		}
		start = (float) NRAND(360);
		do {
			star = NRAND(tp->shape / 2) + 1;	/* Not always a star but thats ok. */
		} while ((star != 1) && (!(tp->shape % star)));
		for (i = 0; i < tp->shape; i++) {
			tp->proto_pts[i].x = tp->average / 2 +
				(int) (cos((double) start * 2.0 * M_PI / 360.0) * tp->average / 2.0);
			tp->proto_pts[i].y = tp->average / 2 +
				(int) (sin((double) start * 2.0 * M_PI / 360.0) * tp->average / 2.0);
			start += star * 360 / tp->shape;
		}
		tp->proto_pts[tp->shape] = tp->proto_pts[0];
	}
	tp->linewidth = NRAND(tp->average / 6 + 1) + 1;
	if (star > 1)		/* Make the width less */
		tp->linewidth = NRAND(tp->linewidth / 2 + 1) + 1;
	lw = tp->linewidth / 2;
	tp->x1 = NRAND(screen_width - tp->width - 2 * lw) + lw;
	tp->y1 = NRAND(screen_height - tp->height - 2 * lw) + lw;

	tp->counter = 0;

	if (!tp->gc) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			tp->fg = MI_FG_PIXEL(mi);
			tp->bg = MI_BG_PIXEL(mi);
#endif
			tp->blackpixel = MI_BLACK_PIXEL(mi);
			tp->whitepixel = MI_WHITE_PIXEL(mi);
			if ((tp->cmap = XCreateColormap(display, window,
					MI_VISUAL(mi), AllocNone)) == None) {
				free_tube(display, tp);
				return;
			}
			XSetWindowColormap(display, window, tp->cmap);
			(void) XParseColor(display, tp->cmap, "black", &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tp->cmap, "white", &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, tp->cmap, background, &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tp->cmap, foreground, &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			tp->colors = (XColor *) NULL;
			tp->ncolors = 0;
		}
		if ((tp->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None) {
			free_tube(display, tp);
			return;
		}
	}
	MI_CLEARWINDOW(mi);

  /* Set up colour map */
  tp->direction = (LRAND() & 1) ? 1 : -1;
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    if (tp->colors != NULL) {
      if (tp->ncolors && !tp->no_colors)
        free_colors(display, tp->cmap, tp->colors, tp->ncolors);
      free(tp->colors);
      tp->colors = (XColor *) NULL;
    }
    tp->ncolors = MI_NCOLORS(mi);
    if (tp->ncolors < 2)
      tp->ncolors = 2;
    if (tp->ncolors <= 2)
      tp->mono_p = True;
    else
      tp->mono_p = False;

    if (tp->mono_p)
      tp->colors = (XColor *) NULL;
    else
      if ((tp->colors = (XColor *) malloc(sizeof (*tp->colors) *
          (tp->ncolors + 1))) == NULL) {
        free_tube(display, tp);
        return;
      }
    tp->cycle_p = has_writable_cells(mi);
    if (tp->cycle_p) {
      if (MI_IS_FULLRANDOM(mi)) {
        if (!NRAND(8))
          tp->cycle_p = False;
        else
          tp->cycle_p = True;
      } else {
        tp->cycle_p = cycle_p;
      }
    }
    if (!tp->mono_p) {
      if (!(LRAND() % 10))
        make_random_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
              tp->cmap, tp->colors, &tp->ncolors,
              True, True, &tp->cycle_p);
      else if (!(LRAND() % 2))
        make_uniform_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                  tp->cmap, tp->colors, &tp->ncolors,
                  True, &tp->cycle_p);
      else
        make_smooth_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                 tp->cmap, tp->colors, &tp->ncolors,
                 True, &tp->cycle_p);
    }
    XInstallColormap(display, tp->cmap);
    if (tp->ncolors < 2) {
      tp->ncolors = 2;
      tp->no_colors = True;
    } else
      tp->no_colors = False;
    if (tp->ncolors <= 2)
      tp->mono_p = True;

    if (tp->mono_p)
      tp->cycle_p = False;

  }
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (tp->mono_p) {
			tp->cur_color = MI_BLACK_PIXEL(mi);
		}
  }
}

void
draw_tube(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	unsigned int i;
	int         lw;
	tubestruct *tp;

	if (tubes == NULL)
		return;
	tp = &tubes[MI_SCREEN(mi)];

	if (tp->no_colors) {
		init_tube(mi);
		return;
	}
	if (tp->shape >= 3 && tp->pts == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	lw = tp->linewidth / 2;

        if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (tp->mono_p) {
                        XSetForeground(display, tp->gc, tp->cur_color);
                } else {
                        tp->cur_color = (tp->cur_color + 1) % tp->ncolors;
                        XSetForeground(display, tp->gc, tp->colors[tp->cur_color].pixel);
                }
        } else {
                if (MI_NPIXELS(mi) > 2)
                        XSetForeground(display, tp->gc, MI_PIXEL(mi, tp->cur_color));
                else if (tp->cur_color)
                        XSetForeground(display, tp->gc, MI_BLACK_PIXEL(mi));
                else
                        XSetForeground(display, tp->gc, MI_WHITE_PIXEL(mi));
                if (++tp->cur_color >= (unsigned int) MI_NPIXELS(mi))
                        tp->cur_color = 0;
        }

  /* Rotate colours */
  if (tp->cycle_p) {
    rotate_colors(display, tp->cmap, tp->colors, tp->ncolors,
      tp->direction);
    if (!(LRAND() % 1000))
      tp->direction = -tp->direction;
  }

	/* move rectangle forward, horiz */
	tp->x1 += tp->dx1;
	if (tp->x1 < lw) {
		tp->x1 = lw;
		tp->dx1 = -tp->dx1;
	}
	if (tp->x1 + tp->width + lw >= MI_WIDTH(mi)) {
		tp->x1 = MI_WIDTH(mi) - tp->width - 1 - lw;
		tp->dx1 = -tp->dx1;
	}
	/* move rectange forward, vert */
	tp->y1 += tp->dy1;
	if (tp->y1 < lw) {
		tp->y1 = lw;
		tp->dy1 = -tp->dy1;
	}
	if (tp->y1 + tp->height + lw >= MI_HEIGHT(mi)) {
		tp->y1 = MI_HEIGHT(mi) - tp->height - 1 - lw;
		tp->dy1 = -tp->dy1;
	}
	XSetLineAttributes(display, tp->gc, tp->linewidth + (tp->shape >= 2),
			   LineSolid, CapNotLast, JoinRound);
	if (tp->shape < 2)
		XDrawRectangle(display, window, tp->gc,
			       tp->x1, tp->y1, tp->width, tp->height);
	else if (tp->shape == 2)
		XDrawArc(display, window, tp->gc,
			 tp->x1, tp->y1, tp->width, tp->height, 0, 23040);
	else {
		for (i = 0; i <= (unsigned int) tp->shape; i++) {
			tp->pts[i].x = tp->x1 + tp->proto_pts[i].x;
			tp->pts[i].y = tp->y1 + tp->proto_pts[i].y;
		}
		XDrawLines(display, window, tp->gc, tp->pts, tp->shape + 1,
			   CoordModeOrigin);
	}
	XSetLineAttributes(display, tp->gc, 1, LineSolid, CapNotLast, JoinRound);

	tp->counter++;
	if (tp->counter > MI_CYCLES(mi)) {
		init_tube(mi);
	}
}

void
release_tube(ModeInfo * mi)
{
	if (tubes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_tube(MI_DISPLAY(mi), &tubes[screen]);
		free(tubes);
		tubes = (tubestruct *) NULL;
	}
}

#endif /* MODE_tube */
