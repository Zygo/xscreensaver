/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@lucid.com>
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
 */

#include "screenhack.h"

static int sizex, sizey;
static int delay;
static GC gc;

static Bool
MapNotify_event_p (dpy, event, window)
     Display *dpy;
     XEvent *event;
     XPointer window;
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}


static Bool
screensaver_window_p (dpy, window)
     Display *dpy;
     Window window;
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  char *version;
  if (XGetWindowProperty (dpy, window,
			  XInternAtom (dpy, "_SCREENSAVER_VERSION", False),
			  0, 1, False, XA_STRING,
			  &type, &format, &nitems, &bytesafter,
			  (unsigned char **) &version)
      == Success
      && type != None)
    return True;
  return False;
}

static void
init_decay (dpy, window)
     Display *dpy;
     Window window;
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  int root_p;
  Pixmap pixmap;

  delay = get_integer_resource ("delay", "Integer");
  root_p = get_boolean_resource ("root", "Boolean");

  if (delay < 0) delay = 0;

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gc = XCreateGC (dpy, window, GCForeground |GCFunction | GCSubwindowMode,
		  &gcv);

  pixmap = grab_screen_image (dpy, window, root_p);

  XGetWindowAttributes (dpy, window, &xgwa);
  sizex = xgwa.width;
  sizey = xgwa.height;
}


/*
 * perform one iteration of decay
 */
static void
decay1 (dpy, window)
     Display *dpy;
     Window window;
{
    int left, top, width, height;

#define nrnd(x) (random() % (x))

    switch (random() % 8) {
      case 0:			/* move a block left */
      case 1:
	left = nrnd(sizex - 1) + 1;
	top = nrnd(sizey);
	width = nrnd(sizex - left);
	height = nrnd(sizey - top);
	XCopyArea (dpy, window, window, gc, left, top, width, height,
		   left - 1, top);
	break;
      case 2:			/* move a block right */
      case 3:
	left = nrnd(sizex - 1);
	top = nrnd(sizey);
	width = nrnd(sizex - 1 - left);
	height = nrnd(sizey - top);
	XCopyArea (dpy, window, window, gc, left, top, width, height,
		   left + 1, top);
	break;
      case 4:			/* move a block up */
	left = nrnd(sizex);
	top = nrnd(sizey - 1) + 1;
	width = nrnd(sizex - left);
	height = nrnd(sizey - top);
	XCopyArea (dpy, window, window, gc, left, top, width, height,
		   left, top - 1);
	break;
      default:			/* move block down (biased to this) */
	left = nrnd(sizex);
	top = nrnd(sizey - 1);
	width = nrnd(sizex - left);
	height = nrnd(sizey - 1 - top);
	XCopyArea (dpy, window, window, gc, left, top, width, height,
		   left, top + 1);
	break;
    }
    XSync (dpy, True);
#undef nrnd
}


char *progclass = "DecayScreen";

char *defaults [] = {
  "DecayScreen.mappedWhenManaged:false",
  "DecayScreen.dontClearWindow:	 true",
  "*delay:			10",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
};

int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
    init_decay (dpy, window);
    while (1) {
	decay1 (dpy, window);
	if (delay) usleep (delay);
    }
}
