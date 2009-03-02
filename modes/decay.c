/* -*- Mode: C; tab-width: 4 -*- */
/* decay --- decayscreen */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)decay.c  4.13 98/12/03 xlockmore";

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

/* decayscreen
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

#ifdef MODE_decay

ModeSpecOpt decay_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   blot_description =
{"decay", "init_decay", "draw_decay", "release_decay",
 "refresh_decay", "init_decay", NULL, &decay_opts,
 200000, 6, 30, 1, 64, 0.3, "",
 "Shows a decaying screen", 0, NULL};

#endif

typedef struct
{
	Display *	dsp;
	Window		w;
	int		sizex;
	int		sizey;
	GC		gc;
	int		mode;
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


void
init_decay (ModeInfo * mi)
{
  decaystruct *dc;
	XGCValues gcv;
	long gcflags;

	char *s = "random";
	
	if (decay_info == NULL)
		decay_info = (decaystruct *) calloc(MI_NUM_SCREENS(mi),
						sizeof(decaystruct));
	if(decay_info == NULL) return;
	dc = &decay_info[MI_SCREEN(mi)];
	
	
	if      (s && !strcmp(s, "shuffle")) dc->mode = SHUFFLE;
	else if (s && !strcmp(s, "up")) dc->mode = UP;
	else if (s && !strcmp(s, "left")) dc->mode = LEFT;
	else if (s && !strcmp(s, "right")) dc->mode = RIGHT;
	else if (s && !strcmp(s, "down")) dc->mode = DOWN;
	else if (s && !strcmp(s, "upleft")) dc->mode = UPLEFT;
	else if (s && !strcmp(s, "downleft")) dc->mode = DOWNLEFT;
	else if (s && !strcmp(s, "upright")) dc->mode = UPRIGHT;
	else if (s && !strcmp(s, "downright")) dc->mode = DOWNRIGHT;
	else if (s && !strcmp(s, "in")) dc->mode = IN;
	else if (s && !strcmp(s, "out")) dc->mode = OUT;
	else
	{
		if (s && *s && !!strcmp(s, "random"))
		fprintf(stderr, "%s: unknown mode %s\n", ProgramName, s);
		dc->mode = random() % (OUT+1);
	}

	dc->dsp = MI_DISPLAY(mi);
	dc->w = MI_WINDOW(mi);
	dc->sizex = MI_WIDTH(mi);
	dc->sizey = MI_HEIGHT(mi);
	gcv.function = GXcopy;
	gcflags = GCFunction;
	dc->gc = XCreateGC (dc->dsp, dc->w, gcflags, &gcv);
#ifdef USE_UNSTABLE
	XCopyArea (dc->dsp, MI_ROOT_PIXMAP(mi), dc->w,
	       dc->gc, 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi),
	       0, 0);
#endif
	XFlush(dc->dsp);
	return;
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

    decaystruct * dc = &decay_info[MI_SCREEN(mi)];
	
    switch (dc->mode) {
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

#define nrnd(x) (random() % (x))

    left = nrnd(dc->sizex - 1);
    top = nrnd(dc->sizey);
    width = nrnd(dc->sizex - left);
    height = nrnd(dc->sizey - top);

    toleft = left;
    totop = top;

    if (dc->mode == IN || dc->mode == OUT) {
      int x = left+(width/2);
      int y = top+(height/2);
      int cx = dc->sizex/2;
      int cy = dc->sizey/2;
      if (dc->mode == IN) {
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

    switch (bias[random() % (sizeof(no_bias)/sizeof(*no_bias))]) {
      case L: toleft = left-DEGREE; break;
      case R: toleft = left+DEGREE; break;
      case U: totop = top-DEGREE; break;
      case D: totop = top+DEGREE; break;
      default: abort(); break;
    }

    XCopyArea (dc->dsp, dc->w, dc->w,
	       dc->gc, left, top, width, height,
	       toleft, totop);
    XFlush(dc->dsp);
#undef nrnd
	
}

void
refresh_decay(ModeInfo * mi)
{
	return;
}

void
release_decay(ModeInfo * mi)
{
  if (decay_info != NULL) {
		int	screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			if (decay_info[screen].gc != NULL)
			  XFreeGC(MI_DISPLAY(mi), decay_info[screen].gc);
		}
		(void) free((void *) decay_info);
		decay_info = NULL;
	}
}

#endif /* MODE_decay */
