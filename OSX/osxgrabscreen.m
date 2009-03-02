/* xscreensaver, Copyright (c) 1992-2006 Jamie Zawinski <jwz@jwz.org>
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
 */

#import <stdlib.h>
#import <Cocoa/Cocoa.h>
#import "jwxyz.h"
#import "grabscreen.h"
#import "colorbars.h"
#import "resources.h"
#import "usleep.h"


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

  unsigned long *odata = (unsigned long *) xim->data;

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
      unsigned short *ip = (unsigned short *) data;
      int x;
      for (x = 0; x < ximw; x++) {
        unsigned short p = *ip++;
        // This should be ok on both PPC and Intel (ARGB, word order)
        unsigned char r = (p >> 10) & 0x1F;
        unsigned char g = (p >>  5) & 0x1F;
        unsigned char b = (p      ) & 0x1F;
        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);
        unsigned long pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
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
      unsigned long map[256];
      for (y = 0; y < 256; y++) {
        CGDeviceColor c = CGPaletteGetColorAtIndex (pal, y);
        unsigned char r = c.red   * 255.0;
        unsigned char g = c.green * 255.0;
        unsigned char b = c.blue  * 255.0;
        unsigned long pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
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
void
osx_grab_desktop_image (Screen *screen, Window xwindow, Drawable drawable)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  NSView *nsview = jwxyz_window_view (xwindow);
  NSWindow *nswindow = [nsview window];
  int window_x, window_y;
  Window unused;

  // figure out where this window is on the screen
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
  GC gc = 0;
  XPutImage (dpy, drawable, gc, xim, 0, 0, 0, 0, xim->width, xim->height);
  XDestroyImage (xim);
}


/* Loads an image file and splats it onto the drawable.
   The image is drawn as large as possible while preserving its aspect ratio.
   If geom_ret is provided, the actual rectangle the rendered image takes
   up will be returned there.
 */
Bool
osx_load_image_file (Screen *screen, Window xwindow, Drawable drawable,
                     const char *filename, XRectangle *geom_ret)
{
  NSImage *img = [[NSImage alloc] initWithContentsOfFile:
                                    [NSString stringWithCString:filename
                                              encoding:NSUTF8StringEncoding]];
  if (!img)
    return False;

  jwxyz_draw_NSImage (DisplayOfScreen (screen), drawable, img, geom_ret);
  [img release];
  return True;
}
