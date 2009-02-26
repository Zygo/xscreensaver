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

#ifndef __XSCREENSAVER_FPS_H__
# define __XSCREENSAVER_FPS_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

typedef struct fps_state fps_state;

extern fps_state *fps_init (Display *, Window);
extern void fps_free (fps_state *);
extern void fps_slept (fps_state *, unsigned long usecs);
extern double fps_compute (fps_state *, unsigned long polys);
extern void fps_draw (fps_state *);

#endif /* __XSCREENSAVER_FPS_H__ */
