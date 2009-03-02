#ifndef __XLOCK_XLOCKMORE_H__
#define __XLOCK_XLOCKMORE_H__

#ifdef STANDALONE
/* xscreensaver compatibility layer for xlockmore modules. */

/*-
 * xscreensaver, Copyright (c) 1997, 1998 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * The definitions in this file make it possible to compile an xlockmore
 * module into a standalone program, and thus use it with xscreensaver.
 * By Jamie Zawinski <jwz@netscape.com> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#if !defined(PROGCLASS) || !defined(HACK_INIT) || !defined(HACK_DRAW)
ERROR ! Define PROGCLASS, HACK_INIT, and HACK_DRAW before including xlockmore.h
#endif

#include "xlock.h"

/*-
 * Prototypes for the actual drawing routines...
 */
extern void HACK_INIT(ModeInfo *);
extern void HACK_DRAW(ModeInfo *);

#ifdef HACK_FREE
extern void HACK_FREE(ModeInfo *);

#else
#define HACK_FREE 0
#endif

/*-
 * Emit code for the entrypoint used by screenhack.c, and pass control
 * down into xlockmore.c with the appropriate parameters.
 */

char       *progclass = PROGCLASS;

void
screenhack(Display * dpy, Window window)
{
	xlockmore_screenhack(dpy, window,

#ifdef WRITABLE_COLORS
			     True,
#else
			     False,
#endif

#ifdef UNIFORM_COLORS
			     True,
#else
			     False,
#endif

#ifdef SMOOTH_COLORS
			     True,
#else
			     False,
#endif

#ifdef BRIGHT_COLORS
			     True,
#else
			     False,
#endif

			     HACK_INIT,
			     HACK_DRAW,
			     HACK_FREE);
}


const char *app_defaults = DEFAULTS;

#endif /* STANDALONE */
#endif /* __XLOCK_XLOCKMORE_H__ */
