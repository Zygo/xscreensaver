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

/* This is just a test harness, really -- it makes a window with an
   XScreenSaverGLView in it and a Preferences button, because I don't want
   to have to debug these programs by installing them as screen savers
   first!
 */

#import "AppController.h"
#import "XScreenSaverGLView.h"

@implementation AppController

- (ScreenSaverView *) makeSaverView
{
  const char *var = "SCREENSAVER_MODULE";
  const char *s = getenv (var);
  if (!s || !*s) {
    NSAssert1 (0, @"set $%s to the name of the module to run", var);
    exit(1);
  }
  NSString *module = [NSString stringWithCString:s
                                        encoding:NSUTF8StringEncoding];
  NSString *dir = [[[NSBundle mainBundle] bundlePath]
                   stringByDeletingLastPathComponent];
  NSString *name = [module stringByAppendingPathExtension:@"saver"];
  NSString *path = [dir stringByAppendingPathComponent:name];
  NSBundle *bundleToLoad = [NSBundle bundleWithPath:path];
  Class new_class = [bundleToLoad principalClass];
  NSAssert1 (new_class, @"unable to load \"%@\"", path);

  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = 320;
  rect.size.height = 240;

  id instance = [[new_class alloc] initWithFrame:rect isPreview:YES];
  NSAssert1 (instance, @"unable to instantiate %@", new_class);
  return (ScreenSaverView *) instance;
}


- (void) openPreferences: (int) which
{
  ScreenSaverView *sv = (which ? saverView1 : saverView0);
  NSAssert (sv, @"no saver view");
  NSWindow *prefs = [sv configureSheet];

  [NSApp beginSheet:prefs
     modalForWindow:[sv window]
      modalDelegate:self
     didEndSelector:@selector(preferencesClosed:returnCode:contextInfo:)
        contextInfo:nil];
  int code = [NSApp runModalForWindow:prefs];
  
  /* Restart the animation if the "OK" button was hit, but not if "Cancel".
     We have to restart *both* animations, because the xlockmore-style
     ones will blow up if one re-inits but the other doesn't.
   */
  if (code != NSCancelButton) {
    if (saverView0) [saverView0 stopAnimation];
    if (saverView1) [saverView1 stopAnimation];
    if (saverView0) [saverView0 startAnimation];
    if (saverView1) [saverView1 startAnimation];
  }
}

- (void) openPreferences0: (NSObject *) button
{
  [self openPreferences:0];
}

- (void) openPreferences1: (NSObject *) button
{
  [self openPreferences:1];
}

- (void) preferencesClosed: (NSWindow *) sheet
                returnCode: (int) returnCode
               contextInfo: (void  *) contextInfo
{
  [NSApp stopModalWithCode:returnCode];
}


- (void) makeWindow: (ScreenSaverView *)sv which:(int)which
{
  NSRect rect;
  
  if (which)
    saverView1 = sv;
  else
    saverView0 = sv;
  
  // make a "Preferences" button
  //
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;
  NSButton *pb = [[NSButton alloc] initWithFrame:rect];
  [pb setTitle:@"Preferences"];
  [pb setBezelStyle:NSRoundedBezelStyle];
  [pb sizeToFit];
  rect.origin.x = ([sv frame].size.width -
                   [pb frame].size.width) / 2;
  [pb setFrameOrigin:rect.origin];
  
  // grab the click
  //
  [pb setTarget:self];
  if (which)
    [pb setAction:@selector(openPreferences1:)];
  else
    [pb setAction:@selector(openPreferences0:)];
      
  // make a box to wrap the saverView
  //
  rect = [sv frame];
  rect.origin.x = 0;
  rect.origin.y = [pb frame].origin.y + [pb frame].size.height;
  NSBox *gbox = [[NSBox alloc] initWithFrame:rect];
  rect.size.width = rect.size.height = 10;
  [gbox setContentViewMargins:rect.size];
  [gbox setTitlePosition:NSNoTitle];
  [gbox addSubview:sv];
  [gbox sizeToFit];

  // make a box to wrap the other two boxes
  //
  rect.origin.x = rect.origin.y = 0;
  rect.size.width  = [gbox frame].size.width;
  rect.size.height = [gbox frame].size.height + [gbox frame].origin.y;
  NSBox *pbox = [[NSBox alloc] initWithFrame:rect];
  [pbox setTitlePosition:NSNoTitle];
  [pbox setBorderType:NSNoBorder];
  [pbox addSubview:gbox];
  [pbox addSubview:pb];
  [pbox sizeToFit];

  // and make a window to hold that.
  //
  NSScreen *screen = [NSScreen mainScreen];
  rect = [pbox frame];
  rect.origin.x = ([screen frame].size.width  - rect.size.width)  / 2;
  rect.origin.y = ([screen frame].size.height - rect.size.height) / 2;
  
  rect.origin.x += rect.size.width * (which ? 0.55 : -0.55);
  
  NSWindow *window = [[NSWindow alloc]
                      initWithContentRect:rect
                                styleMask:(NSTitledWindowMask |
                                           NSClosableWindowMask |
                                           NSMiniaturizableWindowMask |
                                           NSResizableWindowMask)
                                  backing:NSBackingStoreBuffered
                                    defer:YES
                                   screen:screen];
  [window setTitle:@"XScreenSaver"];
  [window setMinSize:[window frameRectForContentRect:rect].size];

  [[window contentView] addSubview:pbox];

  [sv setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  [pb setAutoresizingMask:NSViewMinXMargin|NSViewMaxXMargin];
  [gbox setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  [pbox setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    
  [window setFrameAutosaveName:(which
                                ? @"SaverDebugWindow1"
                                : @"SaverDebugWindow0")];
  [window setFrameUsingName:[window frameAutosaveName]];
  
  [window makeKeyAndOrderFront:window];
  
  [sv startAnimation];
}


- (void)applicationDidFinishLaunching: (NSNotification *) n
{
  [self makeWindow:[self makeSaverView] which:0];
  [self makeWindow:[self makeSaverView] which:1];
}

/* When the window closes, exit (even if prefs still open.)
*/
- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) n
{
  return YES;
}

@end
