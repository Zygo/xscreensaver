/* xscreensaver, Copyright (c) 1992-2016 Jamie Zawinski <jwz@jwz.org>
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
#ifndef USE_IPHONE
# import <Cocoa/Cocoa.h>
#else
# import "SaverRunner.h"
#endif
#import "jwxyz-cocoa.h"
#import "grabscreen.h"
#import "colorbars.h"
#import "resources.h"
#import "usleep.h"


#ifdef USE_IPHONE
# define NSImage UIImage
#endif


#ifndef  MAC_OS_X_VERSION_10_6
# define MAC_OS_X_VERSION_10_6 1060  /* undefined in 10.4 SDK, grr */
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5

     /* 10.4 code.

        This version of the code works on 10.4, but is flaky.  There is
        a better way to do it on 10.5 and newer, but taking this path,
        then we are being compiled against the 10.4 SDK instead of the
        10.5 SDK, and the newer API is not available to us.
      */

static void
copy_framebuffer_to_ximage (CGDirectDisplayID cgdpy, XImage *xim,
                            int window_x, int window_y)
{
  unsigned char *data = (unsigned char *) 
    CGDisplayAddressForPosition (cgdpy, window_x, window_y);
  int bpp = CGDisplayBitsPerPixel (cgdpy);
  int spp = CGDisplaySamplesPerPixel (cgdpy);
  int bps = CGDisplayBitsPerSample (cgdpy);
  int bpr = CGDisplayBytesPerRow (cgdpy);

  int y;
  int ximw = xim->width;
  int ximh = xim->height;

  uint32_t *odata = (uint32_t *) xim->data;

  switch (bpp) {
  case 32:
    if (spp != 3) abort();
    if (bps != 8) abort();
    int xwpl = xim->bytes_per_line/4;
    for (y = 0; y < ximh; y++) {
      // We can do this because the frame buffer and XImage are both ARGB 32.
      // Both PPC and Intel use ARGB, viewed in word order (not byte-order).
      memcpy (odata, data, ximw * 4);
      odata += xwpl;
      data += bpr;
    }
    break;

  case 16:
    if (spp != 3) abort();
    if (bps != 5) abort();
    for (y = 0; y < ximh; y++) {
      uint16_t *ip = (uint16_t *) data;
      int x;
      for (x = 0; x < ximw; x++) {
        uint16_t p = *ip++;
        // This should be ok on both PPC and Intel (ARGB, word order)
        unsigned char r = (p >> 10) & 0x1F;
        unsigned char g = (p >>  5) & 0x1F;
        unsigned char b = (p      ) & 0x1F;
        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);
        uint32_t pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
        // XPutPixel (xim, x, y, pixel);
        *odata++ = pixel;
      }
      data += bpr;
    }
    break;

  case 8:
    {
      /* Get the current palette of the display. */
      CGDirectPaletteRef pal = CGPaletteCreateWithDisplay (cgdpy);

      /* Map it to 32bpp pixels */
      uint32_t map[256];
      for (y = 0; y < 256; y++) {
        CGDeviceColor c = CGPaletteGetColorAtIndex (pal, y);
        unsigned char r = c.red   * 255.0;
        unsigned char g = c.green * 255.0;
        unsigned char b = c.blue  * 255.0;
        uint32_t pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
        map[y] = pixel;
      }

      for (y = 0; y < ximh; y++) {
        unsigned char *ip = data;
        int x;
        for (x = 0; x < ximw; x++) {
          *odata++ = map[*ip++];
        }
        data += bpr;
      }
      CGPaletteRelease (pal);
    }
    break;

  default:
    abort();
    break;
  }
}


/* Loads an image into the Drawable, returning once the image is loaded.
 */
Bool
osx_grab_desktop_image (Screen *screen, Window xwindow, Drawable drawable,
                        XRectangle *geom_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  NSView *nsview = jwxyz_window_view (xwindow);
  NSWindow *nswindow = [nsview window];
  XWindowAttributes xgwa;
  int window_x, window_y;
  Window unused;

  // Figure out where this window is on the screen.
  //
  XGetWindowAttributes (dpy, xwindow, &xgwa);
  XTranslateCoordinates (dpy, xwindow, RootWindowOfScreen (screen), 0, 0, 
                         &window_x, &window_y, &unused);

  // Use the size of the Drawable, not the Window.
  {
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    XGetGeometry (dpy, drawable, &r, &x, &y, &w, &h, &bbw, &d);
    xgwa.width = w;
    xgwa.height = h;
  }

  // Create a tmp ximage to hold the screen data.
  //
  XImage *xim = XCreateImage (dpy, xgwa.visual, 32, ZPixmap, 0, 0,
                              xgwa.width, xgwa.height, 8, 0);
  xim->data = (char *) malloc (xim->height * xim->bytes_per_line);


  // Find the address in the frame buffer of the top left of this window.
  //
  CGDirectDisplayID cgdpy = 0;
  {
    CGPoint p;
    // #### this isn't quite right for screen 2: it's offset slightly.
    p.x = window_x;
    p.y = window_y;
    CGDisplayCount n;
    CGGetDisplaysWithPoint (p, 1, &cgdpy, &n);
    if (!cgdpy) abort();
  }

  // Paint a transparent "hole" in this window.
  //
  BOOL oopaque = [nswindow isOpaque];
  [nswindow setOpaque:NO];

  [[NSColor clearColor] set];
  NSRectFill ([nsview frame]);
  [[nswindow graphicsContext] flushGraphics];


  // Without this, we get a dozen black scanlines at the top.
  // #### But with this, the screen saver loops, because calling this
  //      seems to implicitly mark the display as non-idle!
  // CGDisplayCaptureWithOptions (cgdpy, kCGCaptureNoFill);

  // #### So let's try waiting for the vertical blank instead...
  //      Nope, that doesn't work.
  //
  // CGDisplayWaitForBeamPositionOutsideLines (cgdpy, 0,
  //   window_y + [nswindow frame].size.height);

  // #### Ok, try a busy-wait?
  //      Nope.
  //

  // #### Ok, just fuckin' sleep!
  //
  usleep (100000);


  // Pull the bits out of the frame buffer.
  //
  copy_framebuffer_to_ximage (cgdpy, xim, window_x, window_y);

  // CGDisplayRelease (cgdpy);

  // Make the window visible again.
  //
  [nswindow setOpaque:oopaque];

  // Splat the XImage onto the target drawable (probably the window)
  // and free the bits.
  //
  XGCValues gcv;
  GC gc = XCreateGC (dpy, drawable, 0, &gcv);
  XPutImage (dpy, drawable, gc, xim, 0, 0, 0, 0, xim->width, xim->height);
  XFreeGC (dpy, gc);

  if (geom_ret) {
    geom_ret->x = 0;
    geom_ret->y = 0;
    geom_ret->width  = xim->width;
    geom_ret->height = xim->height;
  }

  XDestroyImage (xim);
  return True;
}


#elif defined(USE_IPHONE)

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


#else /* MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5

         10.5+ code.

         This version of the code is simpler and more reliable, but
         uses an API that only exist on 10.5 and newer, so we can only
         use it if when being compiled against the 10.5 SDK or later.
       */

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

  // Figure out where this window is on the screen.
  //
  XGetWindowAttributes (dpy, xwindow, &xgwa);
  XTranslateCoordinates (dpy, xwindow, RootWindowOfScreen (screen), 0, 0, 
                         &window_x, &window_y, &unused);

  // Grab only the rectangle of the screen underlying this window.
  //
  CGRect cgrect;
  cgrect.origin.x    = window_x;
  cgrect.origin.y    = window_y;
  cgrect.size.width  = xgwa.width;
  cgrect.size.height = xgwa.height;

  /* If a password is required to unlock the screen, a large black
     window will be on top of all of the desktop windows by the time
     we reach here, making the screen-grab rather uninteresting.  If
     we move ourselves temporarily below the login-window windows
     before capturing the image, we capture the real desktop as
     intended.

     Oct 2016: Surprise, this trick no longer works on MacOS 10.12.  Sigh.
   */

  // save our current level so we can restore it later
  int oldLevel = [[nsview window] level]; 

# if 0
  {
    FILE *f = fopen("/tmp/log.txt", "w");
    CFArrayRef L = CGWindowListCopyWindowInfo (kCGWindowListOptionOnScreenOnly,
                                               kCGNullWindowID);

    fprintf(f, "# %d\n", [[nsview window] windowNumber]);

    int n = CFArrayGetCount(L);
    for (int i = 0; i < n; i++) {
      NSDictionary *dict = (NSDictionary *) CFArrayGetValueAtIndex(L, i);
      fprintf(f,
              "%d \"%s\"\n",
              (int) [dict objectForKey:kCGWindowNumber],
              [(NSString *) [dict objectForKey:kCGWindowOwnerName]
                cStringUsingEncoding:[NSString defaultCStringEncoding]]);
    }
    fclose(f);
  }
#endif

  [[nsview window] setLevel:CGWindowLevelForKey(kCGPopUpMenuWindowLevelKey)];

  // Grab a screen shot of those windows below this one
  // (hey, X11 can't do that!)
  //
  CGImageRef img = 
    CGWindowListCreateImage (cgrect,
                             kCGWindowListOptionOnScreenBelowWindow,
                             [[nsview window] windowNumber],
                             kCGWindowImageDefault);

  // put us back above the login windows so the screensaver is visible.
  [[nsview window] setLevel:oldLevel];

  if (! img) return False;

  // Render the grabbed CGImage into the Drawable.
  jwxyz_draw_NSImage_or_CGImage (DisplayOfScreen (screen), drawable, 
                                 False, img, geom_ret, 0);
  CGImageRelease (img);
  return True;
}

#endif /* 10.5+ code */


# ifndef USE_IPHONE

/* Returns the EXIF rotation property of the image, if any.
 */
static int
exif_rotation (const char *filename)
{
  /* As of 10.6, NSImage rotates according to EXIF by default:
     http://developer.apple.com/mac/library/releasenotes/cocoa/appkit.html
     So this function should return -1 when *running* on 10.6 systems.
     But when running against older systems, we need to examine the image
     to figure out its rotation.
   */

# if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6  /* 10.6 SDK */

  /* When we have compiled against the 10.6 SDK, we know that we are 
     running on a 10.6 or later system.
   */
  return -1;

# else /* Compiled against 10.5 SDK or earlier */

  /* If this selector exists, then we are running against a 10.6 runtime
     that does automatic EXIF rotation (despite the fact that we were
     compiled against the 10.5 or earlier SDK).  So in that case, this
     function should no-op.
   */
  if ([NSImage instancesRespondToSelector:
                 @selector(initWithDataIgnoringOrientation:)])
    return -1;

  /* Otherwise, go ahead and figure out what the rotational characteristics
     of this image are. */



  /* This is a ridiculous amount of rigamarole to go through, but for some
     reason the "Orientation" tag does not exist in the "NSImageEXIFData"
     dictionary inside the NSBitmapImageRep of the NSImage.  Several other
     EXIF tags are there (e.g., shutter speed) but not orientation.  WTF?
   */
  CFStringRef s = CFStringCreateWithCString (NULL, filename, 
                                             kCFStringEncodingUTF8);
  CFURLRef url = CFURLCreateWithFileSystemPath (NULL, s, 
                                                kCFURLPOSIXPathStyle, 0);
  CGImageSourceRef cgimg = CGImageSourceCreateWithURL (url, NULL);
  if (! cgimg) return -1;

  NSDictionary *props = (NSDictionary *)
    CGImageSourceCopyPropertiesAtIndex (cgimg, 0, NULL);
  int rot = [[props objectForKey:@"Orientation"] intValue];
  CFRelease (cgimg);
  CFRelease (url);
  CFRelease (s);
  return rot;

# endif /* 10.5 */
}

# endif /* USE_IPHONE */



/* Loads an image file and splats it onto the drawable.
   The image is drawn as large as possible while preserving its aspect ratio.
   If geom_ret is provided, the actual rectangle the rendered image takes
   up will be returned there.
 */
Bool
osx_load_image_file (Screen *screen, Window xwindow, Drawable drawable,
                     const char *filename, XRectangle *geom_ret)
{
# ifndef USE_IPHONE

  if (!filename || !*filename) return False;

  NSImage *img = [[NSImage alloc] initWithContentsOfFile:
                                    [NSString stringWithCString:filename
                                              encoding:NSUTF8StringEncoding]];
  if (!img)
    return False;

  jwxyz_draw_NSImage_or_CGImage (DisplayOfScreen (screen), drawable, 
                                 True, img, geom_ret,
                                 exif_rotation (filename));
  [img release];
  return True;

# else  /* USE_IPHONE */

  /* This is handled differently: see grabclient.c and grabclient-ios.m. */
  return False;

# endif /* USE_IPHONE */
}
