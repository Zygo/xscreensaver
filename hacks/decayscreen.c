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
 
 *  R. Schultz
 *  Added "melt" & "stretch" modes 28-Mar-1999
 *
 */

#include "screenhack.h"

static int sizex, sizey;
static int delay;
static GC gc;
static int mode;
static int iterations=100;

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
#define MELT 11
#define STRETCH 12
#define FUZZ 13

static void
init_decay (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  long gcflags;
  unsigned long bg;

  char *s = get_string_resource("mode", "Mode");
  if      (s && !strcmp(s, "shuffle")) mode = SHUFFLE;
  else if (s && !strcmp(s, "up")) mode = UP;
  else if (s && !strcmp(s, "left")) mode = LEFT;
  else if (s && !strcmp(s, "right")) mode = RIGHT;
  else if (s && !strcmp(s, "down")) mode = DOWN;
  else if (s && !strcmp(s, "upleft")) mode = UPLEFT;
  else if (s && !strcmp(s, "downleft")) mode = DOWNLEFT;
  else if (s && !strcmp(s, "upright")) mode = UPRIGHT;
  else if (s && !strcmp(s, "downright")) mode = DOWNRIGHT;
  else if (s && !strcmp(s, "in")) mode = IN;
  else if (s && !strcmp(s, "out")) mode = OUT;
  else if (s && !strcmp(s, "melt")) mode = MELT;
  else if (s && !strcmp(s, "stretch")) mode = STRETCH;
  else if (s && !strcmp(s, "fuzz")) mode = FUZZ;
  else {
    if (s && *s && !!strcmp(s, "random"))
      fprintf(stderr, "%s: unknown mode %s\n", progname, s);
    mode = random() % (FUZZ+1);
  }

  delay = get_integer_resource ("delay", "Integer");

  if (delay < 0) delay = 0;

  XGetWindowAttributes (dpy, window, &xgwa);

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;

  if (mode == MELT || mode == STRETCH) {
    bg = get_pixel_resource ("background", "Background", dpy, xgwa.colormap);
    gcv.foreground = bg;
  }

  gcflags = GCForeground |GCFunction;
  if (use_subwindow_mode_p(xgwa.screen, window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
  gc = XCreateGC (dpy, window, gcflags, &gcv);

  sizex = xgwa.width;
  sizey = xgwa.height;

  load_random_image (xgwa.screen, window, window);
  
  if (mode == MELT || mode == STRETCH) {
    /* make sure screen eventually turns background color */
    XDrawLine(dpy, window, gc, 0, 0, sizex, 0); 

    /* slow down for smoother melting*/
    iterations = 1;
  }

}


/*
 * perform one iteration of decay
 */
static void
decay1 (Display *dpy, Window window)
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

    switch (mode) {
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
      case MELT:	bias = no_bias; break;
      case STRETCH:	bias = no_bias; break;
      case FUZZ:	bias = no_bias; break;
     default: abort();
    }

#define nrnd(x) (random() % (x))

    if (mode == MELT || mode == STRETCH) {
      left = nrnd(sizex/2);
      top = nrnd(sizey);
      width = nrnd( sizex/2 ) + sizex/2 - left;
      height = nrnd(sizey - top);
      toleft = left;
      totop = top+1;

    } else if (mode == FUZZ) {  /* By Vince Levey <vincel@vincel.org>;
                                   inspired by the "melt" mode of the
                                   "scrhack" IrisGL program by Paul Haeberli
                                   circa 1991. */
      static int toggle = 0;

      left = nrnd(sizex - 1);
      top  = nrnd(sizey - 1);
      toggle = !toggle;
      if (toggle)
        {
          totop = top;
          height = 1;
          toleft = nrnd(sizex - 1);
          if (toleft > left)
            {
              width = toleft-left;
              toleft = left;
              left++;
            }
          else
            {
              width = left-toleft;
              left = toleft;
              toleft++;
            }
        }
      else
        {
          toleft = left;
          width = 1;
          totop  = nrnd(sizey - 1);
          if (totop > top)
            {
              height = totop-top;
              totop = top;
              top++;
            }
          else
            {
              height = top-totop;
              top = totop;
              totop++;
            }
        }

    } else {

      left = nrnd(sizex - 1);
      top = nrnd(sizey);
      width = nrnd(sizex - left);
      height = nrnd(sizey - top);
      
      toleft = left;
      totop = top;
      if (mode == IN || mode == OUT) {
	int x = left+(width/2);
	int y = top+(height/2);
	int cx = sizex/2;
	int cy = sizey/2;
	if (mode == IN) {
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
      case L: toleft = left-1; break;
      case R: toleft = left+1; break;
      case U: totop = top-1; break;
      case D: totop = top+1; break;
      default: abort(); break;
      }
    }
    
    if (mode == STRETCH) {
      XCopyArea (dpy, window, window, gc, 0, sizey-top-2, sizex, top+1, 
		 0, sizey-top-1); 
    } else {
      XCopyArea (dpy, window, window, gc, left, top, width, height,
		 toleft, totop);
    }

#undef nrnd
}


char *progclass = "DecayScreen";

char *defaults [] = {
  "*dontClearRoot:		True",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  "*delay:			10000",
  "*mode:			random",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
    init_decay (dpy, window);
    while (1) {
      int i;
      for (i = 0; i < iterations; i++)
	decay1 (dpy, window);
      XSync(dpy, False);
      screenhack_handle_events (dpy);
      if (delay) usleep (delay);
    }
}
