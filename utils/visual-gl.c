/* xscreensaver, Copyright (c) 1999 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for picking the best visual for GL programs by
   actually asking the GL library to figure it out for us.  The code in
   visual.c might do a good job of this on most systems, but not, in
   particular, on SGIs.

   Why?  Because with the SGI O2 X server is weird.

   GL programs tend to work best on a visual that is *half* as deep as the
   depth of the screen, since that way, they can do double-buffering.  So
   generally, if the screen is 24 bits deep, but a 12-bit TrueColor visual
   is available, then that's the visual you should use.

   But on the server that runs on the O2 (a machine that has serious hardware
   support for GL) the 12-bit PseudoColor visual looks awful (you get a black
   and white, flickery image.)  On these machines, the visual you want turns
   out to be 0x31 -- this is but one of the eight 15-bit TrueColor visuals
   (yes, 8, and yes, 15) that O2s provide.  This is the only visual that works
   properly -- as far as `xdpyinfo' is concerned, all of the 15-bit TrueColor
   visuals are identical, but some flicker like mad, and some have deeply
   weird artifacts (hidden surfaces show through!)  I suppose these other
   visuals must be tied to some arcane hardware feature...

   So the bottom line is, there exists information about visuals which is
   available to GL, but which is not available via Xlib calls.  So the only
   way to know which visual to use (other than impirically) is to actually
   call GLX routines.
 */

#include "utils.h"
#include "visual.h"

#ifdef HAVE_GL
# include <GL/gl.h>
# include <GL/glx.h>
#endif /* HAVE_GL */

Visual *
get_gl_visual (Screen *screen)
{
#ifdef HAVE_GL
  XVisualInfo *vi = 0;
  Display *dpy = DisplayOfScreen (screen);
  int screen_num = screen_number (screen);
  int attrs[20];
  int i = 0;

  attrs[i++] = GLX_RGBA;
  attrs[i++] = GLX_RED_SIZE;     attrs[i++] = 1;
  attrs[i++] = GLX_GREEN_SIZE;   attrs[i++] = 1;
  attrs[i++] = GLX_BLUE_SIZE;    attrs[i++] = 1;
  attrs[i++] = GLX_DEPTH_SIZE;   attrs[i++] = 1;
  attrs[i++] = GLX_DOUBLEBUFFER;
  attrs[i++] = 0;

  vi = glXChooseVisual (dpy, screen_num, attrs);

  if (!vi)				/* Try without double-buffering. */
    {
      attrs[i-1] = 0;
      vi = glXChooseVisual (dpy, screen_num, attrs);
    }

  if (!vi)				/* Try mono. */
    {
      i = 0;
      attrs[i++] = GLX_DOUBLEBUFFER;
      attrs[i++] = 0;
      vi = glXChooseVisual (dpy, screen_num, attrs);
    }

  if (!vi)				/* Try mono without double-buffer. */
    {
      attrs[0] = 0;
      vi = glXChooseVisual (dpy, screen_num, attrs);
    }

  if (!vi)
    return 0;
  else
    {
      Visual *v = vi->visual;
      XFree (vi);
      return v;
    }
#else  /* !HAVE_GL */
  return 0;
#endif /* !HAVE_GL */
}
