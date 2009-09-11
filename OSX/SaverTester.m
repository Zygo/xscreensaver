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

/* This is just a test harness, really -- it makes a window with an
   XScreenSaverGLView in it and a Preferences button, because I don't want
   to have to debug these programs by installing them as screen savers
   first!
 */

#import "SaverTester.h"
#import "XScreenSaverGLView.h"

@implementation SaverTester

- (ScreenSaverView *) makeSaverView: (NSString *) module
{
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


static ScreenSaverView *
find_saverView_child (NSView *v)
{
  NSArray *kids = [v subviews];
  int nkids = [kids count];
  int i;
  for (i = 0; i < nkids; i++) {
    NSObject *kid = [kids objectAtIndex:i];
    if ([kid isKindOfClass:[ScreenSaverView class]]) {
      return (ScreenSaverView *) kid;
    } else {
      ScreenSaverView *sv = find_saverView_child ((NSView *) kid);
      if (sv) return sv;
    }
  }
  return 0;
}


static ScreenSaverView *
find_saverView (NSView *v)
{
  while (1) {
    NSView *p = [v superview];
    if (p) v = p;
    else break;
  }
  return find_saverView_child (v);
}


- (void) openPreferences: (NSObject *) button
{
  ScreenSaverView *sv = find_saverView ((NSView *) button);
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
    [sv stopAnimation];
    [sv startAnimation];
  }
}

- (void) preferencesClosed: (NSWindow *) sheet
                returnCode: (int) returnCode
               contextInfo: (void  *) contextInfo
{
  [NSApp stopModalWithCode:returnCode];
}


- (void)selectedSaverDidChange:(NSDictionary *)change
{
  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  NSString *module = [prefs stringForKey:@"selectedSaverName"];
  int i;

  if (! module) return;

  for (i = 0; i < [windows count]; i++) {
    NSView *cv = [[windows objectAtIndex:i] contentView];
    ScreenSaverView *old_view = find_saverView (cv);
    NSView *sup = [old_view superview];

    [old_view stopAnimation];
    [old_view removeFromSuperview];

    ScreenSaverView *new_view = [self makeSaverView:module];
    [new_view setFrame: [old_view frame]];
    [sup addSubview: new_view];
    [new_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [new_view startAnimation];
  }

  NSUserDefaultsController *ctl =
    [NSUserDefaultsController sharedUserDefaultsController];
  [ctl save:self];
}


- (NSArray *) listSaverBundleNames
{
  NSString *dir = [[[NSBundle mainBundle] bundlePath]
                   stringByDeletingLastPathComponent];
  NSString *longest = 0;
  NSArray *matches = 0;
  NSArray *exts = [NSArray arrayWithObjects:@"saver", nil];
  unsigned int n = [dir completePathIntoString: &longest
                                 caseSensitive: NO
                              matchesIntoArray: &matches
                                   filterTypes: exts];
  if (n <= 0) {
    NSLog (@"no .saver bundles found in \"%@/\"!", dir);
    exit (1);
  }

  int i;
  NSMutableArray *result = [NSMutableArray arrayWithCapacity: n+1];
  for (i = 0; i < n; i++)
    [result addObject: [[[matches objectAtIndex: i] lastPathComponent]
                         stringByDeletingPathExtension]];
  return result;
}


- (NSPopUpButton *) makeMenu
{
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = 10;
  rect.size.height = 10;
  NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:rect
                                                    pullsDown:NO];
  int i;
  float max_width = 0;
  for (i = 0; i < [saverNames count]; i++) {
    NSString *name = [saverNames objectAtIndex:i];
    [popup addItemWithTitle:name];
    [[popup itemWithTitle:name] setRepresentedObject:name];
    [popup sizeToFit];
    NSRect r = [popup frame];
    if (r.size.width > max_width) max_width = r.size.width;
  }

  // Bind the menu to preferences, and trigger a callback when an item
  // is selected.
  //
  NSString *key = @"values.selectedSaverName";
  NSUserDefaultsController *prefs =
    [NSUserDefaultsController sharedUserDefaultsController];
  [prefs addObserver:self
         forKeyPath:key
            options:0
            context:@selector(selectedSaverDidChange:)];
  [popup   bind:@"selectedObject"
       toObject:prefs
    withKeyPath:key
        options:nil];
  [prefs setAppliesImmediately:YES];

  NSRect r = [popup frame];
  r.size.width = max_width;
  [popup setFrame:r];
  return popup;
}


/* This is called when the "selectedSaverName" pref changes, e.g.,
   when a menu selection is made.
 */
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
  SEL dispatchSelector = (SEL)context;
  if (dispatchSelector != NULL) {
    [self performSelector:dispatchSelector withObject:change];
  } else {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
  }
}



- (NSWindow *) makeWindow
{
  NSRect rect;
  static int count = 0;

  // make a "Preferences" button
  //
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;
  NSButton *pb = [[NSButton alloc] initWithFrame:rect];
  [pb setTitle:@"Preferences"];
  [pb setBezelStyle:NSRoundedBezelStyle];
  [pb sizeToFit];


  NSRect sv_rect = rect;
  sv_rect.size.width = 320;
  sv_rect.size.height = 240;
  ScreenSaverView *sv = [[ScreenSaverView alloc]  // dummy placeholder
                          initWithFrame:sv_rect
                          isPreview:YES];

  rect.origin.x = ([sv frame].size.width -
                   [pb frame].size.width) / 2;
  [pb setFrameOrigin:rect.origin];
  
  // grab the click
  //
  [pb setTarget:self];
  [pb setAction:@selector(openPreferences:)];

  // Make a saver selection menu
  //
  NSPopUpButton *menu = [self makeMenu];
  rect.origin.x = 2;
  rect.origin.y = 2;
  [menu setFrameOrigin:rect.origin];

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
  [pbox addSubview:menu];
  [pbox addSubview:pb];
  [pbox sizeToFit];

  // and make a window to hold that.
  //
  NSScreen *screen = [NSScreen mainScreen];
  rect = [pbox frame];
  rect.origin.x = ([screen frame].size.width  - rect.size.width)  / 2;
  rect.origin.y = ([screen frame].size.height - rect.size.height) / 2;
  
  rect.origin.x += rect.size.width * (count ? 0.55 : -0.55);
  
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

  [sv   setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  [pb   setAutoresizingMask:NSViewMinXMargin|NSViewMaxXMargin];
  [menu setAutoresizingMask:NSViewMinXMargin|NSViewMaxXMargin];
  [gbox setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  [pbox setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    
  char buf[100];
  sprintf (buf, "SaverDebugWindow%d", count);
  [window setFrameAutosaveName:
            [NSString stringWithCString:buf encoding:NSUTF8StringEncoding]];
  [window setFrameUsingName:[window frameAutosaveName]];
  
  [window makeKeyAndOrderFront:window];
  
  [sv startAnimation]; // this is the dummy saver

  count++;

  return window;
}


- (void)applicationDidFinishLaunching: (NSNotification *) notif
{
  saverNames = [[self listSaverBundleNames] retain];

  int i, n = 2;
  NSMutableArray *w = [[NSMutableArray arrayWithCapacity: n+1] retain];
  windows = w;
  for (i = 0; i < n; i++)
    [w addObject: [self makeWindow]];

  [self selectedSaverDidChange:nil];
}


/* When the window closes, exit (even if prefs still open.)
*/
- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) n
{
  return YES;
}

@end
