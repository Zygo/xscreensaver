/* fps, Copyright (c) 2001-2008 Jamie Zawinski <jwz@jwz.org>
 * Draw a frames-per-second display (Xlib and OpenGL).
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_FPSI_H__
# define __XSCREENSAVER_FPSI_H__

#include "fps.h"

struct fps_state {
  Display *dpy;
  Window window;
  int x, y;
  XFontStruct *font;
  unsigned long font_dlist;  /* for glx/fps-gl.c */
  Bool clear_p;
  char string[1024];

  GC draw_gc, erase_gc;

  int last_ifps;
  double last_fps;
  int frame_count;
  unsigned long slept;
  struct timeval prev_frame_end, this_frame_end;
};

#endif /* __XSCREENSAVER_FPSI_H__ */
