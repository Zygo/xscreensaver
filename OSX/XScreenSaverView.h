/* xscreensaver, Copyright (c) 2006-2008 Jamie Zawinski <jwz@jwz.org>
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

#import <Cocoa/Cocoa.h>
#import <ScreenSaver/ScreenSaver.h>
#import "screenhackI.h"
#import "PrefsReader.h"

@interface XScreenSaverView : ScreenSaverView
{
  struct xscreensaver_function_table *xsft;
  PrefsReader *prefsReader;

  BOOL setup_p;		   // whether xsft->setup_cb() has been run
  BOOL initted_p;          // whether xsft->init_cb() has been run
  BOOL resized_p;	   // whether to run the xsft->reshape_cb() soon
  double next_frame_time;  // time_t in milliseconds of when to tick the frame

  // Data used by the Xlib-flavored screensaver
  Display *xdpy;
  Window xwindow;
  void *xdata;
  fps_state *fpst;
}

- (void) prepareContext;
- (void) resizeContext;

@end
