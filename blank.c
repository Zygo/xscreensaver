#ifndef lint
static char sccsid[] = "@(#)blank.c	0.1.5 91/05/24 xlockmore";
#endif
/*-
 * blank.c - blank screen for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 31-Aug-90: Written.
 */

#include "xlock.h"

/*ARGSUSED*/
void
drawblank(win)
    Window      win;
{
}

void
initblank(win)
    Window      win;
{
    XClearWindow(dsp, win);
}
