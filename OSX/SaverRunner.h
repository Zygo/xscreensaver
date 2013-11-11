/* xscreensaver, Copyright (c) 2006-2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef USE_IPHONE
# import <Foundation/Foundation.h>
# import <UIKit/UIKit.h>
# import <OpenGLES/EAGL.h>
# import <OpenGLES/ES1/gl.h>
# import <OpenGLES/ES1/glext.h>
# import <QuartzCore/QuartzCore.h>
# define NSView  UIView
# define NSRect  CGRect
# define NSSize  CGSize
# define NSColor UIColor
# define NSImage UIImage
# define NSEvent UIEvent
# define NSWindow UIWindow
# define NSButton UIButton
# define NSApplication UIApplication
# define NSScreen UIScreen
#else
# import <Cocoa/Cocoa.h>
# import <ScreenSaver/ScreenSaver.h>
#endif

@class ScreenSaverView;

@interface SaverRunner : NSObject
{
  NSString *saverName;		// the one currently loaded
  NSArray  *saverNames;		// Names of available savers
  NSString *saverDir;		// Where we find saver bundles

# ifndef USE_IPHONE

  NSBundle *saverBundle;
  NSArray  *windows;
  IBOutlet NSMenu *menubar;
  NSTimer *anim_timer;

# else  // USE_IPHONE

  ScreenSaverView *saverView;
  UIView *backgroundView;
  UINavigationController *rootViewController;
  IBOutlet UIWindow *window;
  EAGLContext *eagl_ctx;
  GLuint gl_framebuffer, gl_renderbuffer;
  IBOutlet UIView *view;
  UIImage *saved_screenshot;
  UIView *aboutBox;
  NSTimer *splashTimer;

# endif // USE_IPHONE
}

- (void) loadSaver: (NSString *)name launch:(BOOL)launch;
- (void) loadSaver: (NSString *)name;
- (void) selectedSaverDidChange:(NSDictionary *)change;
- (void) aboutPanel: (id)sender;

#ifndef USE_IPHONE
- (void) openPreferences: (id)sender;
#else  // USE_IPHONE
- (void) openPreferences: (NSString *)which;
- (UIImage *) screenshot;
#endif // USE_IPHONE

@end
