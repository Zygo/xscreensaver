/* xscreensaver, Copyright (c) 2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This stub is compiled differently for each saver, just to ensure that
   each one has a different class name.  If multiple savers use the
   XScreenSaver class directly, System Preferences gets really confused.
 */

#ifndef CLASS
 ERROR! -DCLASS missing
#endif

#ifdef USE_GL
# import "XScreenSaverGLView.h"
# define SUPERCLASS XScreenSaverGLView
#else
# import "XScreenSaverView.h"
# define SUPERCLASS XScreenSaverView
#endif

@interface CLASS : SUPERCLASS { }
@end

@implementation CLASS
@end
