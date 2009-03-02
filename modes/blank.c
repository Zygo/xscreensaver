/* -*- Mode: C; tab-width: 4 -*- */
/* blank --- blank screen */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)blank.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 10-May-1997: Compatible with xscreensaver :)  OK you probably should not
 *              use this for xscreensaver but I could not resist.
 * 21-Mar-1996: Ron Hitchens <ron@idiom.com>
 *		        Bonehead alert.  Don't blank during password prompting.
 * 19-Mar-1996: Ron Hitchens <ron@idiom.com>
 *		        Changed to activate X server's native screensaver.
 *		        On some devices, this will result in power saving "sleep"
 *		        mode, or video blanking.
 * 31-Aug-1990: Written.
 */

#ifdef STANDALONE
#define PROGCLASS "Blank"
#define HACK_INIT init_blank
#define HACK_DRAW draw_blank
#define blank_opts xlockmore_opts
#define DEFAULTS "*delay: 1000000 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt blank_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   blank_description =
{"blank", "init_blank", "draw_blank", "release_blank",
 "refresh_blank", "init_blank", NULL, &blank_opts,
 3000000, 1, 1, 1, 64, 1.0, "",
 "Shows nothing but a black screen", 0, NULL};

#endif

extern Bool enablesaver;

void
init_blank(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
	/* Must set PreferBlanking, or XForceScreenSaver won't work */
	if (!MI_IS_INWINDOW(mi) && !MI_IS_INROOT(mi) && !enablesaver)
		XSetScreenSaver(MI_DISPLAY(mi), 0, 0, PreferBlanking, 0);
}

/* ARGSUSED */
void
draw_blank(ModeInfo * mi)
{
	/* Leave the lights on while user types password */
	if (!MI_IS_INWINDOW(mi) && !MI_IS_INROOT(mi) && !enablesaver) {
		if (MI_IS_ICONIC(mi))
			XForceScreenSaver(MI_DISPLAY(mi), ScreenSaverReset);
		else
			XForceScreenSaver(MI_DISPLAY(mi), ScreenSaverActive);
	}
}

void
release_blank(ModeInfo * mi)
{
	/* clear screensaver settings, just in case */
	if (!MI_IS_INWINDOW(mi) && !MI_IS_INROOT(mi) && !enablesaver) {
		XForceScreenSaver(MI_DISPLAY(mi), ScreenSaverReset);
		XSetScreenSaver(MI_DISPLAY(mi), 0, 0, 0, 0);
	}
}

void
refresh_blank(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself :) */
}
