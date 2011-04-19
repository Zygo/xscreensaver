/* xscreensaver, Copyright (c) 1997, 2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else /* !HAVE_COCOA - real X11 */
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>
#endif /* !HAVE_COCOA */
