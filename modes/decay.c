/* -*- Mode: C; tab-width: 4 -*- */
/* decay --- decayscreen */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)decay.c	5.00 2000/11/01 xlockmore";

#endif

/* xscreensaver, Copyright (c) 1992, 1993, 1994, 1996, 1997
 * Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/*
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 17-Mar-1999: Converted from xscreensaver's decayscreen
 * 
 * decayscreen
 *
 * Based on slidescreen program from the xscreensaver application and the
 * decay program for Sun framebuffers.  This is the comment from the decay.c
 * file:

 * decay.c
 *   find the screen bitmap for the console and make it "decay" by
 *   randomly shifting random rectangles by one pixelwidth at a time.
 *
 *   by David Wald, 1988
 *        rewritten by Natuerlich!
 *   based on a similar "utility" on the Apollo ring at Yale.

 * X version by
 *
 *  Vivek Khera <khera@cs.duke.edu>
 *  5-AUG-1993
 *
 *  Hacked by jwz, 28-Nov-97 (sped up and added new motion directions)
 */

#ifdef STANDALONE
#define PROGCLASS "Decay"
#define HACK_INIT init_decay
#define HACK_DRAW draw_decay
#define decay_opts xlockmore_opts
#define DEFAULTS "*delay: 200000 \n" \
 "*count: 6 \n" \
 "*cycles: 30 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"    /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"    /* in xlockmore distribution */
#endif /* STANDALONE */

#include "iostuff.h"
#include "color.h"

#ifdef MODE_decay

extern Bool hide;

ModeSpecOpt decay_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   decay_description =
{"decay", "init_decay", "draw_decay", "release_decay",
 "refresh_decay", "init_decay", NULL, &decay_opts,
 200000, 6, 30, 1, 64, 0.3, "",
 "Shows a decaying screen", 0, NULL};

#endif

#define DECAY_WIDTH   image_width
#define DECAY_HEIGHT    image_height
#define DECAY_BITS    image_bits
#include "decay.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define DECAY_NAME    image_name
#include "decay.xpm"
#define DEFAULT_XPM 1
#endif

typedef struct
{
	XPoint   windowsize;
	int      mode;

	XPoint   randompos, randpos;
	XImage  *logo;
	GC       backGC;
	Colormap cmap;
	int      graphics_format;
	unsigned long black;
	Bool     hide;
} decaystruct;

static decaystruct *	decay_info = NULL;

#define SHUFFLE 0
#define UP 1
#define LEFT 2
#define RIGHT 3
#define DOWN 4
#define UPLEFT 5
#define DOWNLEFT 6
#define UPRIGHT 7
#define DOWNRIGHT 8
#define IN 9
#define OUT 10
#define	DEGREE	1

static void
free_decay(Display * display, decaystruct * dp)
{
	if (dp->cmap != None) {
		XFreeColormap(display, dp->cmap);
		if (dp->backGC != None) {
			XFreeGC(display, dp->backGC);
			dp->backGC = None;
		}
		dp->cmap = None;
	} else
		dp->backGC = None;
	if (dp->hide && (dp->logo != None)) {
		destroyImage(&dp->logo, &dp->graphics_format);
		dp->logo = None;
	}
}

static void
alloc_decay(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	decaystruct *dp = &decay_info[MI_SCREEN(mi)];

	if (dp->hide) {
		if (dp->logo == None) {
			getImage(mi, &dp->logo, DECAY_WIDTH, DECAY_HEIGHT, DECAY_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
				DEFAULT_XPM, DECAY_NAME,
#endif
				&dp->graphics_format, &dp->cmap, &dp->black);
			if (dp->logo == None) {
				free_decay(display, dp);
				return;
			}
		}
#ifndef STANDALONE
		if (dp->cmap != None) {
			setColormap(display, window, dp->cmap, MI_IS_INWINDOW(mi));
			if (dp->backGC == None) {
				XGCValues   xgcv;

				xgcv.background = dp->black;
				dp->backGC = XCreateGC(display, window, GCBackground, &xgcv);
				if (dp->backGC == None) {
					free_decay(display, dp);
					return;
				}
			}
		} else
#endif /* STANDALONE */
		{
			dp->black = MI_BLACK_PIXEL(mi);
			dp->backGC = MI_GC(mi);
		}
	}
#ifndef STANDALONE
	else {
		setColormap(display, window, DefaultColormapOfScreen(MI_SCREENPTR(mi)), MI_IS_INWINDOW(mi));
		dp->backGC = MI_GC(mi);
	}
#endif /* STANDALONE */
}

void
init_decay(ModeInfo * mi)
{
	Display *display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	decaystruct *dp;

	char *s = (char*) "random";

	if (decay_info == NULL) {
		if ((decay_info = (decaystruct *) calloc(MI_NUM_SCREENS(mi),
						sizeof(decaystruct))) == NULL)
			return;
	}
	dp = &decay_info[MI_SCREEN(mi)];

	if      (s && !strcmp(s, "shuffle")) dp->mode = SHUFFLE;
	else if (s && !strcmp(s, "up")) dp->mode = UP;
	else if (s && !strcmp(s, "left")) dp->mode = LEFT;
	else if (s && !strcmp(s, "right")) dp->mode = RIGHT;
	else if (s && !strcmp(s, "down")) dp->mode = DOWN;
	else if (s && !strcmp(s, "upleft")) dp->mode = UPLEFT;
	else if (s && !strcmp(s, "downleft")) dp->mode = DOWNLEFT;
	else if (s && !strcmp(s, "upright")) dp->mode = UPRIGHT;
	else if (s && !strcmp(s, "downright")) dp->mode = DOWNRIGHT;
	else if (s && !strcmp(s, "in")) dp->mode = IN;
	else if (s && !strcmp(s, "out")) dp->mode = OUT;
	else {
		if (s && *s && !!strcmp(s, "random"))
			(void) fprintf(stderr, "%s: unknown mode %s\n", ProgramName, s);
		dp->mode = (int) (LRAND() % (OUT+1));
	}

	if (MI_IS_FULLRANDOM(mi) && !hide)
		dp->hide = (Bool) (LRAND() & 1);
	else
		dp->hide = hide;


	dp->windowsize.x = MI_WIDTH(mi);
	dp->windowsize.y = MI_HEIGHT(mi);
	alloc_decay(mi);
	if (!dp->backGC)
		return;
	if (dp->hide) {
  		/* do not want any exposure events from XCopyArea */
  		XSetGraphicsExposures(display, dp->backGC, False);
		MI_CLEARWINDOWCOLORMAP(mi, dp->backGC, dp->black);
		dp->randompos.x =
			NRAND(MAX((dp->windowsize.x - dp->logo->width), 1));
                dp->randompos.y =
			NRAND(MAX((dp->windowsize.y - dp->logo->height), 1));
		if (MI_NPIXELS(mi) <= 2)
			XSetForeground(display, dp->backGC, MI_WHITE_PIXEL(mi));
		else
			XSetForeground(display, dp->backGC, MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		(void) XPutImage(display, window, dp->backGC, dp->logo,
			(int) (NRAND(MAX(1, (dp->logo->width - dp->windowsize.x)))),
			(int) (NRAND(MAX(1, (dp->logo->height - dp->windowsize.y)))),
                                 dp->randompos.x, dp->randompos.y,
                                 dp->windowsize.x, dp->windowsize.y);

	} else {
		XCopyArea (MI_DISPLAY(mi), MI_ROOT_PIXMAP(mi), MI_WINDOW(mi),
		       dp->backGC, 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi),
		       0, 0);
		XFlush(MI_DISPLAY(mi));
	}
}


/*
 * perform one iteration of decay
 */
void
draw_decay (ModeInfo * mi)
{
    int left, top, width, height, toleft, totop;

#define L 101
#define R 102
#define U 103
#define D 104
    static int no_bias[]        = { L,L,L,L, R,R,R,R, U,U,U,U, D,D,D,D };
    static int up_bias[]        = { L,L,L,L, R,R,R,R, U,U,U,U, U,U,D,D };
    static int down_bias[]      = { L,L,L,L, R,R,R,R, U,U,D,D, D,D,D,D };
    static int left_bias[]      = { L,L,L,L, L,L,R,R, U,U,U,U, D,D,D,D };
    static int right_bias[]     = { L,L,R,R, R,R,R,R, U,U,U,U, D,D,D,D };

    static int upleft_bias[]    = { L,L,L,L, L,R,R,R, U,U,U,U, U,D,D,D };
    static int downleft_bias[]  = { L,L,L,L, L,R,R,R, U,U,U,D, D,D,D,D };
    static int upright_bias[]   = { L,L,L,R, R,R,R,R, U,U,U,U, U,D,D,D };
    static int downright_bias[] = { L,L,L,R, R,R,R,R, U,U,U,D, D,D,D,D };
    static int *bias;
    decaystruct * dp;

	if (decay_info == NULL)
		return;
	dp = &decay_info[MI_SCREEN(mi)];
	if (dp->backGC == None)
		return;

    switch (dp->mode) {
      case SHUFFLE:	bias = no_bias; break;
      case UP:		bias = up_bias; break;
      case LEFT:	bias = left_bias; break;
      case RIGHT:	bias = right_bias; break;
      case DOWN:	bias = down_bias; break;
      case UPLEFT:	bias = upleft_bias; break;
      case DOWNLEFT:	bias = downleft_bias; break;
      case UPRIGHT:	bias = upright_bias; break;
      case DOWNRIGHT:	bias = downright_bias; break;
      case IN:		bias = no_bias; break;
      case OUT:		bias = no_bias; break;
      default: abort();
    }

    left = NRAND(dp->windowsize.x - 1);
    top = NRAND(dp->windowsize.y);
    width = NRAND(dp->windowsize.x - left);
    height = NRAND(dp->windowsize.y - top);

    toleft = left;
    totop = top;

    if (dp->mode == IN || dp->mode == OUT) {
      int x = left+(width/2);
      int y = top+(height/2);
      int cx = dp->windowsize.x/2;
      int cy = dp->windowsize.y/2;
      if (dp->mode == IN) {
	if      (x > cx && y > cy)   bias = upleft_bias;
	else if (x < cx && y > cy)   bias = upright_bias;
	else if (x < cx && y < cy)   bias = downright_bias;
	else /* (x > cx && y < cy)*/ bias = downleft_bias;
      } else {
	if      (x > cx && y > cy)   bias = downright_bias;
	else if (x < cx && y > cy)   bias = downleft_bias;
	else if (x < cx && y < cy)   bias = upleft_bias;
	else /* (x > cx && y < cy)*/ bias = upright_bias;
      }
    }

    switch (bias[LRAND() % (sizeof(no_bias)/sizeof(*no_bias))]) {
      case L: toleft = left-DEGREE; break;
      case R: toleft = left+DEGREE; break;
      case U: totop = top-DEGREE; break;
      case D: totop = top+DEGREE; break;
      default: abort(); break;
    }

    XCopyArea (MI_DISPLAY(mi), MI_WINDOW(mi), MI_WINDOW(mi),
	       dp->backGC, left, top, width, height,
	       toleft, totop);
    XFlush(MI_DISPLAY(mi));

}

void
refresh_decay(ModeInfo * mi)
{
#if defined( USE_XPM ) || defined( USE_XPMINC )
	decaystruct *dp;

	if (decay_info == NULL)
		return;
	dp = &decay_info[MI_SCREEN(mi)];

	if (dp->graphics_format >= IS_XPM) {
		/* This is needed when another program changes the colormap. */
		free_decay(MI_DISPLAY(mi), dp);
		init_decay(mi);
		return;
	}
#endif
}

void
release_decay(ModeInfo * mi)
{
	if (decay_info != NULL) {
		int	screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_decay(MI_DISPLAY(mi), &decay_info[screen]);
		(void) free((void *) decay_info);
		decay_info = NULL;
	}
}

#endif /* MODE_decay */
