/* xscreensaver, Copyright (c) 1998, 1999 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* The XDBE (Double Buffering) extension is pretty tricky to use, since you
   can get X errors at inconvenient times during initialization.  This file
   contains a utility routine to make it easier to deal with.
 */

#ifndef __XSCREENSAVER_XDBE_H__

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION

# include <X11/extensions/Xdbe.h>

extern XdbeBackBuffer xdbe_get_backbuffer (Display *, Window, XdbeSwapAction);

#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#endif /* __XSCREENSAVER_XDBE_H__ */
