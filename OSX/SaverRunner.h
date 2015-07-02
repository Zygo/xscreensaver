/* xscreensaver, Copyright (c) 2006-2014 Jamie Zawinski <jwz@jwz.org>
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

#import <XScreenSaverView.h>

#ifdef USE_IPHONE

@class SaverRunner;

@interface SaverViewController : UIViewController
{
  SaverRunner *_parent;
  NSString *_saverName;
  XScreenSaverView *_saverView;

  /* When a the SaverViewController is presented, iOS automatically changes
     the status bar orientation. (And, by extension, the notification center
     orientation.) But there's no willPresentAsModal: event for a
     UIViewController so that it knows when this is going to happen, and the
     other event handlers occur after the status bar is changed. So save the
     orientation just before the view controller is modal-presented, and
     restore the proper status bar orientation just before the saverView is
     created so it can pick it up in didRotate:. */
  UIInterfaceOrientation _storedOrientation;
}

@property(nonatomic, retain) NSString *saverName;

@end

#endif

@interface SaverRunner : NSObject
# ifdef USE_IPHONE
  <XScreenSaverViewDelegate>
# endif
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

  UINavigationController *rotating_nav;		// Hierarchy 1 (UI)
  IBOutlet UIWindow *window;
  IBOutlet UIView *view;

  SaverViewController *nonrotating_controller;	// Hierarchy 2 (savers)

  EAGLContext *eagl_ctx;
  GLuint gl_framebuffer, gl_renderbuffer;
  UIImage *saved_screenshot;
  UIView *aboutBox;
  NSTimer *splashTimer;

# endif // USE_IPHONE
}

- (XScreenSaverView *) makeSaverView: (NSString *) module
                            withSize: (NSSize) size;
- (void) loadSaver: (NSString *)name;
- (void) selectedSaverDidChange:(NSDictionary *)change;

#ifndef USE_IPHONE
- (void) openPreferences: (id)sender;
#else  // USE_IPHONE
- (void) openPreferences: (NSString *)which;
- (UIImage *) screenshot;
- (void)aboutPanel:(UIView *)saverView
       orientation:(UIInterfaceOrientation)orient;
#endif // USE_IPHONE

@end
