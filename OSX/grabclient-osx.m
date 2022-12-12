/* xscreensaver, Copyright © 1992-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is the OSX implementation of desktop-grabbing and image-loading.
   This code is invoked by "utils/grabclient.c", which is linked directly
   in to each screen saver bundle.

   X11-based builds of the savers do not use this code (even on MacOS).
   This is used only by the Cocoa build of the savers.
 */

#import <stdlib.h>
#import <stdint.h>
#ifndef HAVE_IPHONE
# import <Cocoa/Cocoa.h>
#else
# import "SaverRunner.h"
#endif
#import "jwxyz-cocoa.h"
#import "grabclient.h"
#import "colorbars.h"
#import "resources.h"
#import "usleep.h"


#ifdef HAVE_IPHONE
# define NSImage UIImage
#endif


#ifdef HAVE_IPHONE

	/* What a hack!

           On iOS, our application delegate, SaverRunner, grabs an image
           of itself as a UIImage before mapping the XScreenSaverView.
           In this code, we ask SaverRunner for that UIImage, then copy
           it to the root window.
         */

Bool
osx_grab_desktop_image (Screen *screen, Window xwindow, Drawable drawable,
                        XRectangle *geom_ret)
{
  SaverRunner *s = 
    (SaverRunner *) [[UIApplication sharedApplication] delegate];
  if (! s)
    return False;
  if (! [s isKindOfClass:[SaverRunner class]])
    return False;
  UIImage *img = [s screenshot];
  if (! img)
    return False;
  jwxyz_draw_NSImage_or_CGImage (DisplayOfScreen (screen), drawable,
                                 True, img, geom_ret, 0);
  return True;
}


#else /* !HAVE_IPHONE */

extern float jwxyz_scale (Window);  /* jwxyzI.h */

static void
authorize_screen_capture (void)
{
  static Bool done_once = False;
  if (done_once) return;
  done_once = True;

  /* The following nonsense is intended to get the system to pop up the dialog
     saying "do you want to allow legacyScreenSaver to be able to record your
     screen?"  Without the user saying yes to that, we are only able to get
     the desktop background image instead of a screen shot.  Also this is
     async, so if that dialog has popped up, the *current* screen grab will
     still fail.

     To reset answers given to that dialog: "tccutil reset ScreenCapture".

     Sadly, this is not working at all.  If the .saver bundle is launched by
     SaverTester, the dialog is triggered on behalf of SaverTester, and works.
     But if it the bundle is launched by the real screen saver, or by System
     Preferences, the dialog is not triggered.

     Manually dragging "/System/Library/Frameworks/ScreenSaver.framework/
     PlugIns/legacyScreenSaver.appex" onto the "System Preferences / Security /
     Privacy / Screen Recording" list makes screen grabbing work, but that's
     a lot to ask of the end user.  And, oddly, after dragging it there, it
     does not appear in the list at all (and there's no way to un-check it).

     We can open that preferences pane with
     "open x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture"
     but there's no way to auto-add legacyScreenSaver to it, even unchecked.
  */

# if 1
  /* CGDisplayStreamRef stream = */  /* Just leak it, I guess? */
  CGDisplayStreamCreateWithDispatchQueue (CGMainDisplayID(), 
                                          1, 1, kCVPixelFormatType_32BGRA,
                                          nil,
                                          dispatch_get_main_queue(),
                                          nil);
  /* I think that if the returned value is nil, it means the screen grab
     will be returning just the background image instead of the desktop.
     So in that case we could return False here to go with colorbars.
     But the desktop background image is still more interesting than
     colorbars, so let's just use that. */

# elif 0
  /* This also does not work. */
  if (@available (macos 10.15, *)) {
    CGImageRef img =
      CGWindowListCreateImage (CGRectMake(0, 0, 1, 1),
                               kCGWindowListOptionOnScreenOnly,
                               kCGNullWindowID,
                               kCGWindowImageDefault);
    CFRelease (img);
  }

# elif 0
  /* This also does not work. */
  if (@available (macos 10.15, *))
    CGRequestScreenCaptureAccess();
# endif
}


/* Loads an image into the Drawable, returning once the image is loaded.
 */
Bool
osx_grab_desktop_image (Screen *screen, Window xwindow, Drawable drawable,
                        XRectangle *geom_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  NSView *nsview = jwxyz_window_view (xwindow);
  XWindowAttributes xgwa;
  int window_x, window_y;
  Window unused;

  authorize_screen_capture();

  // Figure out where this window is on the screen.
  //
  XGetWindowAttributes (dpy, xwindow, &xgwa);
  XTranslateCoordinates (dpy, xwindow, RootWindowOfScreen (screen), 0, 0, 
                         &window_x, &window_y, &unused);

  // Grab only the rectangle of the screen underlying this window.
  //
  CGRect cgrect;
  double s = jwxyz_scale (xwindow);
  cgrect.origin.x    = window_x;
  cgrect.origin.y    = window_y;
  cgrect.size.width  = xgwa.width  / s;
  cgrect.size.height = xgwa.height / s;

  CGWindowID windowNumber = (CGWindowID) nsview.window.windowNumber;

  /* If a password is required to unlock the screen, a large black
     window will be on top of all of the desktop windows by the time
     we reach here, making the screen-grab rather uninteresting.  If
     we move ourselves temporarily below the login-window windows
     before capturing the image, we capture the real desktop as
     intended.

     Oct 2016: Surprise, this trick no longer works on MacOS 10.12.  Sigh.

     Nov 2021, macOS 11.6, 12.0: This works again.  Without the following
     clause, we do indeed grab a black image.
   */
  {
    CFArrayRef L = CGWindowListCopyWindowInfo (kCGWindowListOptionOnScreenOnly,
                                               kCGNullWindowID);

    CFIndex n = CFArrayGetCount(L);
    for (int i = 0; i < n; i++) {
      NSDictionary *dict = (NSDictionary *) CFArrayGetValueAtIndex(L, i);

      // loginwindow creates multiple toplevel windows. Grab the lowest one.
      if(![([dict objectForKey:(NSString *)kCGWindowOwnerName])
           compare:@"loginwindow"]) {
        windowNumber = ((NSNumber *)[dict objectForKey:
                                     (NSString *)kCGWindowNumber]).intValue;
      }
    }
    CFRelease (L);
  }


  // Grab a screen shot of those windows below this one
  // (hey, X11 can't do that!)
  //
  CGImageRef img = 
    CGWindowListCreateImage (cgrect,
                             kCGWindowListOptionOnScreenBelowWindow,
                             windowNumber,
                             kCGWindowImageDefault);

  if (! img) return False;

  // Render the grabbed CGImage into the Drawable.
  jwxyz_draw_NSImage_or_CGImage (DisplayOfScreen (screen), drawable, 
                                 False, img, geom_ret, 0);
  CGImageRelease (img);
  return True;
}

# endif /* !HAVE_IPHONE */



/* Loads an image file and splats it onto the drawable.
   The image is drawn as large as possible while preserving its aspect ratio.
   If geom_ret is provided, the actual rectangle the rendered image takes
   up will be returned there.
 */
Bool
osx_load_image_file (Screen *screen, Window xwindow, Drawable drawable,
                     const char *filename, XRectangle *geom_ret)
{
# ifndef HAVE_IPHONE

  if (!filename || !*filename) return False;

  NSImage *img = [[NSImage alloc] initWithContentsOfFile:
                                    [NSString stringWithCString:filename
                                              encoding:NSUTF8StringEncoding]];
  if (!img)
    return False;

  jwxyz_draw_NSImage_or_CGImage (DisplayOfScreen (screen), drawable, 
                                 True, img, geom_ret, -1);
  [img release];
  return True;

# else  /* HAVE_IPHONE */

  /* This is handled differently: see grabclient.c and grabclient-ios.m. */
  return False;

# endif /* HAVE_IPHONE */
}
