/* erase.c: Erase the screen in various more or less interesting ways.
 * Copyright (c) 1997-2001, 2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_ERASE_H__
#define __XSCREENSAVER_ERASE_H__

typedef struct eraser_state eraser_state;

extern eraser_state *erase_window (Display *, Window, eraser_state *);

#endif /* __XSCREENSAVER_ERASE_H__ */
