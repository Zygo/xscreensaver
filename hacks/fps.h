/* fps, Copyright (c) 2001-2011 Jamie Zawinski <jwz@jwz.org>
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

#ifndef __XSCREENSAVER_FPS_H__
# define __XSCREENSAVER_FPS_H__

typedef struct fps_state fps_state;

extern fps_state *fps_init (Display *, Window);
extern void fps_free (fps_state *);
extern void fps_slept (fps_state *, unsigned long usecs);
extern double fps_compute (fps_state *, unsigned long polys, double depth);
extern void fps_draw (fps_state *);

/* Doesn't really belong here, but close enough. */
#ifdef HAVE_MOBILE
  extern double current_device_rotation (void);
#else
# define current_device_rotation() (0)
#endif

#endif /* __XSCREENSAVER_FPS_H__ */
