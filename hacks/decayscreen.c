/* xscreensaver, Copyright (c) 1992-2018 Jamie Zawinski <jwz@jwz.org>
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
#include <time.h>

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  Pixmap saved;
  int saved_w, saved_h;

  int sizex, sizey;
  int delay;
  int duration;
  GC gc;
  int mode;
  int random_p;
  time_t start_time;

  int fuzz_toggle;
  const int *current_bias;

  async_load_state *img_loader;
};


#define SHUFFLE		0
#define UP		1
#define LEFT		2
#define RIGHT		3
#define DOWN		4
#define UPLEFT		5
#define DOWNLEFT	6
#define UPRIGHT		7
#define DOWNRIGHT	8
#define IN		9
#define OUT		10
#define MELT		11
#define STRETCH		12
#define FUZZ		13

static void
decayscreen_load_image (struct state *st)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->sizex = xgwa.width;
  st->sizey = xgwa.height;
  if (st->img_loader) abort();

  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                            st->window, 0, 0);
}

static void *
decayscreen_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  long gcflags;
  unsigned long bg;
  char *s;

  st->dpy = dpy;
  st->window = window;
  st->random_p = 0;

  s = get_string_resource(st->dpy, "mode", "Mode");
  if      (s && !strcmp(s, "shuffle")) st->mode = SHUFFLE;
  else if (s && !strcmp(s, "up")) st->mode = UP;
  else if (s && !strcmp(s, "left")) st->mode = LEFT;
  else if (s && !strcmp(s, "right")) st->mode = RIGHT;
  else if (s && !strcmp(s, "down")) st->mode = DOWN;
  else if (s && !strcmp(s, "upleft")) st->mode = UPLEFT;
  else if (s && !strcmp(s, "downleft")) st->mode = DOWNLEFT;
  else if (s && !strcmp(s, "upright")) st->mode = UPRIGHT;
  else if (s && !strcmp(s, "downright")) st->mode = DOWNRIGHT;
  else if (s && !strcmp(s, "in")) st->mode = IN;
  else if (s && !strcmp(s, "out")) st->mode = OUT;
  else if (s && !strcmp(s, "melt")) st->mode = MELT;
  else if (s && !strcmp(s, "stretch")) st->mode = STRETCH;
  else if (s && !strcmp(s, "fuzz")) st->mode = FUZZ;
  else {
    if (s && *s && !!strcmp(s, "random"))
      fprintf(stderr, "%s: unknown mode %s\n", progname, s);
    st->random_p = 1;
    st->mode = random() % (FUZZ+1);
  }

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;

  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  if (st->duration < 1) st->duration = 1;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  bg = get_pixel_resource (st->dpy, st->xgwa.colormap, "background", "Background");
  gcv.foreground = bg;

  gcflags = GCForeground | GCFunction;
  if (use_subwindow_mode_p(st->xgwa.screen, st->window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
  st->gc = XCreateGC (st->dpy, st->window, gcflags, &gcv);

  st->start_time = time ((time_t *) 0);
  decayscreen_load_image (st);

  return st;
}


/*
 * perform one iteration of decay
 */
static unsigned long
decayscreen_draw (Display *dpy, Window window, void *closure)
{
    struct state *st = (struct state *) closure;
    int left, top, width, height, toleft, totop;

#define L 101
#define R 102
#define U 103
#define D 104
    static const int no_bias[]        = { L,L,L,L, R,R,R,R, U,U,U,U, D,D,D,D };
    static const int up_bias[]        = { L,L,L,L, R,R,R,R, U,U,U,U, U,U,D,D };
    static const int down_bias[]      = { L,L,L,L, R,R,R,R, U,U,D,D, D,D,D,D };
    static const int left_bias[]      = { L,L,L,L, L,L,R,R, U,U,U,U, D,D,D,D };
    static const int right_bias[]     = { L,L,R,R, R,R,R,R, U,U,U,U, D,D,D,D };

    static const int upleft_bias[]    = { L,L,L,L, L,R,R,R, U,U,U,U, U,D,D,D };
    static const int downleft_bias[]  = { L,L,L,L, L,R,R,R, U,U,U,D, D,D,D,D };
    static const int upright_bias[]   = { L,L,L,R, R,R,R,R, U,U,U,U, U,D,D,D };
    static const int downright_bias[] = { L,L,L,R, R,R,R,R, U,U,U,D, D,D,D,D };

    int off = 1;
    if (st->sizex > 2560) off *= 2;  /* Retina displays */

    if (st->img_loader)   /* still loading */
      {
        st->img_loader = load_image_async_simple (st->img_loader, 
                                                  0, 0, 0, 0, 0);
        if (! st->img_loader) {  /* just finished */

          st->start_time = time ((time_t *) 0);
          if (st->random_p)
            st->mode = random() % (FUZZ+1);

          if (st->mode == MELT || st->mode == STRETCH)
            /* make sure screen eventually turns background color */
            XDrawLine (st->dpy, st->window, st->gc, 0, 0, st->sizex, 0); 

          if (!st->saved) {
            st->saved = XCreatePixmap (st->dpy, st->window,
                                       st->sizex, st->sizey,
                                       st->xgwa.depth);
            st->saved_w = st->sizex;
            st->saved_h = st->sizey;
          }
          XCopyArea (st->dpy, st->window, st->saved, st->gc, 0, 0,
                     st->sizex, st->sizey, 0, 0);
        }
      return st->delay;
    }

    if (!st->img_loader &&
        st->start_time + st->duration < time ((time_t *) 0)) {
      decayscreen_load_image (st);
    }

    switch (st->mode) {
      case SHUFFLE:	st->current_bias = no_bias; break;
      case UP:		st->current_bias = up_bias; break;
      case LEFT:	st->current_bias = left_bias; break;
      case RIGHT:	st->current_bias = right_bias; break;
      case DOWN:	st->current_bias = down_bias; break;
      case UPLEFT:	st->current_bias = upleft_bias; break;
      case DOWNLEFT:	st->current_bias = downleft_bias; break;
      case UPRIGHT:	st->current_bias = upright_bias; break;
      case DOWNRIGHT:	st->current_bias = downright_bias; break;
      case IN:		st->current_bias = no_bias; break;
      case OUT:		st->current_bias = no_bias; break;
      case MELT:	st->current_bias = no_bias; break;
      case STRETCH:	st->current_bias = no_bias; break;
      case FUZZ:	st->current_bias = no_bias; break;
     default: abort();
    }

#define nrnd(x) ((x) ? random() % (x) : x)

    if (st->mode == MELT || st->mode == STRETCH) {
      left = nrnd(st->sizex/2);
      top = nrnd(st->sizey);
      width = nrnd( st->sizex/2 ) + st->sizex/2 - left;
      height = nrnd(st->sizey - top);
      toleft = left;
      totop = top+off;

    } else if (st->mode == FUZZ) {  /* By Vince Levey <vincel@vincel.org>;
                                   inspired by the "melt" mode of the
                                   "scrhack" IrisGL program by Paul Haeberli
                                   circa 1991. */
      left = nrnd(st->sizex - 1);
      top  = nrnd(st->sizey - 1);
      st->fuzz_toggle = !st->fuzz_toggle;
      if (st->fuzz_toggle)
        {
          totop = top;
          height = off;
          toleft = nrnd(st->sizex - 1);
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
          width = off;
          totop  = nrnd(st->sizey - 1);
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

      left = nrnd(st->sizex - 1);
      top = nrnd(st->sizey);
      width = nrnd(st->sizex - left);
      height = nrnd(st->sizey - top);
      
      toleft = left;
      totop = top;
      if (st->mode == IN || st->mode == OUT) {
	int x = left+(width/2);
	int y = top+(height/2);
	int cx = st->sizex/2;
	int cy = st->sizey/2;
	if (st->mode == IN) {
	  if      (x > cx && y > cy)   st->current_bias = upleft_bias;
	  else if (x < cx && y > cy)   st->current_bias = upright_bias;
	  else if (x < cx && y < cy)   st->current_bias = downright_bias;
	  else /* (x > cx && y < cy)*/ st->current_bias = downleft_bias;
	} else {
	  if      (x > cx && y > cy)   st->current_bias = downright_bias;
	  else if (x < cx && y > cy)   st->current_bias = downleft_bias;
	  else if (x < cx && y < cy)   st->current_bias = upleft_bias;
	  else /* (x > cx && y < cy)*/ st->current_bias = upright_bias;
	}
      }
      
      switch (st->current_bias[random() % (sizeof(no_bias)/sizeof(*no_bias))]) {
      case L: toleft = left-off; break;
      case R: toleft = left+off; break;
      case U: totop = top-off; break;
      case D: totop = top+off; break;
      default: abort(); break;
      }
    }
    
    if (st->mode == STRETCH) {
      XCopyArea (st->dpy, st->window, st->window, st->gc,
                 0, st->sizey-top-off*2, st->sizex, top+off, 
		 0, st->sizey-top-off);
    } else {
      XCopyArea (st->dpy, st->window, st->window, st->gc,
                 left, top, width, height,
		 toleft, totop);
    }

#undef nrnd

    return st->delay;
}

static void
decayscreen_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  if (! st->saved) return; /* Image might not be loaded yet */
  XClearWindow (st->dpy, st->window);
  XCopyArea (st->dpy, st->saved, st->window, st->gc,
             0, 0, st->saved_w, st->saved_h,
             ((int)w - st->saved_w) / 2,
             ((int)h - st->saved_h) / 2);
  st->sizex = w;
  st->sizey = h;
}

static Bool
decayscreen_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;
      return True;
    }
  return False;
}

static void
decayscreen_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}



static const char *decayscreen_defaults [] = {
  ".background:			Black",
  ".foreground:			Yellow",
  "*dontClearRoot:		True",
  "*fpsSolid:			True",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  "*delay:			10000",
  "*mode:			random",
  "*duration:			120",
#ifdef HAVE_MOBILE
  "*ignoreRotation:             True",
  "*rotateImages:               True",
#endif
  0
};

static XrmOptionDescRec decayscreen_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { "-duration",	".duration",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("DecayScreen", decayscreen)
