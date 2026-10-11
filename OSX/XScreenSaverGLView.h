/* xscreensaver, Copyright (c) 2006-2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a subclass of Apple's ScreenSaverView that knows how to run
   xscreensaver programs without X11 via the dark magic of the "jwxyz"
   library.  In xscreensaver terminology, this is the replacement for
   the "screenhack.c" module.
 */

#import "XScreenSaverView.h"

#ifdef HAVE_IPHONE
# import <OpenGLES/EAGL.h>
# import <OpenGLES/ES1/gl.h>
# import <OpenGLES/ES1/glext.h>
# import <QuartzCore/QuartzCore.h>
# import "jwzglesI.h"
#else
# import <AppKit/NSOpenGL.h>
#endif

@interface XScreenSaverGLView : XScreenSaverView
{
# ifdef HAVE_IPHONE
  GLuint gl_depthbuffer;
  BOOL _suppressRotationAnimation;
  jwzgles_state *_glesState;
# endif /* HAVE_IPHONE */
}

@end
