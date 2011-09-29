/* xscreensaver, Copyright (c) 2006-2011 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This program serves two purposes:

   First, It is a test harness for screen savers.  When it launches, it
   looks around for .saver bundles (in the current directory, and then in
   the standard directories) and puts up a pair of windows that allow you
   to select the saver to run.  This is less clicking than running them
   through System Preferences.  This is the "SaverTester.app" program.

   Second, it can be used to transform any screen saver into a standalone
   program.  Just put one (and only one) .saver bundle into the app
   bundle's Contents/PlugIns/ directory, and it will load and run that
   saver at start-up (without the saver-selection menu or other chrome).
   This is how the "Phosphor.app" and "Apple2.app" programs work.
 */

#import "SaverRunner.h"
#import "XScreenSaverGLView.h"

@implementation SaverRunner

- (ScreenSaverView *) makeSaverView: (NSString *) module
{
  NSString *name = [module stringByAppendingPathExtension:@"saver"];
  NSString *path = [saverDir stringByAppendingPathComponent:name];
  saverBundle = [NSBundle bundleWithPath:path];
  Class new_class = [saverBundle principalClass];
  NSAssert1 (new_class, @"unable to load \"%@\"", path);


  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = 320;
  rect.size.height = 240;

  id instance = [[new_class alloc] initWithFrame:rect isPreview:YES];
  NSAssert1 (instance, @"unable to instantiate %@", new_class);


  /* KLUGE: Inform the underlying program that we're in "standalone"
     mode.  This is kind of horrible but I haven't thought of a more
     sensible way to make this work.
   */
  if ([saverNames count] == 1) {
    putenv (strdup ("XSCREENSAVER_STANDALONE=1"));
  }

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


static void
relabel_menus (NSObject *v, NSString *old_str, NSString *new_str)
{
  if ([v isKindOfClass:[NSMenu class]]) {
    NSMenu *m = (NSMenu *)v;
    [m setTitle: [[m title] stringByReplacingOccurrencesOfString:old_str
                            withString:new_str]];
    NSArray *kids = [m itemArray];
    int nkids = [kids count];
    int i;
    for (i = 0; i < nkids; i++) {
      relabel_menus ([kids objectAtIndex:i], old_str, new_str);
    }
  } else if ([v isKindOfClass:[NSMenuItem class]]) {
    NSMenuItem *mi = (NSMenuItem *)v;
    [mi setTitle: [[mi title] stringByReplacingOccurrencesOfString:old_str
                              withString:new_str]];
    NSMenu *m = [mi submenu];
    if (m) relabel_menus (m, old_str, new_str);
  }
}


- (void) openPreferences: (id) sender
{
  ScreenSaverView *sv;

  if ([sender isKindOfClass:[NSView class]]) {	// Sent from button
    sv = find_saverView ((NSView *) sender);
  } else {
    int i;
    NSWindow *w = 0;
    for (i = [windows count]-1; i >= 0; i--) {	// Sent from menubar
      w = [windows objectAtIndex:i];
      if ([w isKeyWindow]) break;
    }
    sv = find_saverView ([w contentView]);
  }

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


- (void)loadSaver:(NSString *)name
{
  int i;
  for (i = 0; i < [windows count]; i++) {
    NSWindow *window = [windows objectAtIndex:i];
    NSView *cv = [window contentView];
    ScreenSaverView *old_view = find_saverView (cv);
    NSView *sup = [old_view superview];

    NSString *old_title = [window title];
    if (!old_title) old_title = @"XScreenSaver";
    [window setTitle: name];
    relabel_menus (menubar, old_title, name);

    [old_view stopAnimation];
    [old_view removeFromSuperview];

    ScreenSaverView *new_view = [self makeSaverView:name];
    [new_view setFrame: [old_view frame]];
    [sup addSubview: new_view];
    [window makeFirstResponder:new_view];
    [new_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [new_view startAnimation];
  }

  NSUserDefaultsController *ctl =
    [NSUserDefaultsController sharedUserDefaultsController];
  [ctl save:self];
}


- (void)aboutPanel:(id)sender
{
  NSDictionary *bd = [saverBundle infoDictionary];
  NSMutableDictionary *d = [NSMutableDictionary dictionaryWithCapacity:20];

  [d setValue:[bd objectForKey:@"CFBundleName"] forKey:@"ApplicationName"];
  [d setValue:[bd objectForKey:@"CFBundleVersion"] forKey:@"Version"];
  [d setValue:[bd objectForKey:@"CFBundleShortVersionString"] 
     forKey:@"ApplicationVersion"];
  [d setValue:[bd objectForKey:@"NSHumanReadableCopyright"] forKey:@"Copy"];
  [d setValue:[[NSAttributedString alloc]
                initWithString: (NSString *) 
                  [bd objectForKey:@"CFBundleGetInfoString"]]
     forKey:@"Credits"];

  [[NSApplication sharedApplication]
    orderFrontStandardAboutPanelWithOptions:d];
}



- (void)selectedSaverDidChange:(NSDictionary *)change
{
  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  NSString *name = [prefs stringForKey:@"selectedSaverName"];

  if (! [saverNames containsObject:name]) {
    NSLog (@"Saver \"%@\" does not exist", name);
    return;
  }

  if (name) [self loadSaver: name];
}


- (NSArray *) listSaverBundleNamesInDir:(NSString *)dir
{
  NSArray *files = [[NSFileManager defaultManager]
                     contentsOfDirectoryAtPath:dir error:nil];
  if (! files) return 0;

  int n = [files count];
  NSMutableArray *result = [NSMutableArray arrayWithCapacity: n+1];

  int i;
  for (i = 0; i < n; i++) {
    NSString *p = [files objectAtIndex:i];
    if ([[p pathExtension] caseInsensitiveCompare:@"saver"]) 
      continue;
    [result addObject: [[p lastPathComponent] stringByDeletingPathExtension]];
  }

  return result;
}


- (NSArray *) listSaverBundleNames
{
  NSMutableArray *dirs = [NSMutableArray arrayWithCapacity: 10];

  // First look in the bundle itself.
  [dirs addObject: [[NSBundle mainBundle] builtInPlugInsPath]];

  // Then look in the same directory as the executable.
  [dirs addObject: [[[NSBundle mainBundle] bundlePath]
                     stringByDeletingLastPathComponent]];

  // Then look in standard screensaver directories.
  [dirs addObject: @"~/Library/Screen Savers"];
  [dirs addObject: @"/Library/Screen Savers"];
  [dirs addObject: @"/System/Library/Screen Savers"];

  int i;
  for (i = 0; i < [dirs count]; i++) {
    NSString *dir = [dirs objectAtIndex:i];
    NSArray *names = [self listSaverBundleNamesInDir:dir];
    if (! names) continue;

    // Make sure this directory is on $PATH.

    const char *cdir = [dir cStringUsingEncoding:NSUTF8StringEncoding];
    const char *opath = getenv ("PATH");
    if (!opath) opath = "/bin"; // $PATH is unset when running under Shark!
    char *npath = (char *) malloc (strlen (opath) + strlen (cdir) + 30);
    strcpy (npath, "PATH=");
    strcat (npath, cdir);
    strcat (npath, ":");
    strcat (npath, opath);
    if (putenv (npath)) {
      perror ("putenv");
      abort();
    }
    /* Don't free (npath) -- MacOS's putenv() does not copy it. */

    saverDir   = [dir retain];
    saverNames = [names retain];

    return names;
  }

  NSString *err = @"no .saver bundles found in: ";
  for (i = 0; i < [dirs count]; i++) {
    if (i) err = [err stringByAppendingString:@", "];
    err = [err stringByAppendingString:[[dirs objectAtIndex:i] 
                                         stringByAbbreviatingWithTildeInPath]];
    err = [err stringByAppendingString:@"/"];
  }
  NSLog (@"%@", err);
  exit (1);
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
  Bool simple_p = ([saverNames count] == 1);
  NSButton *pb = 0;
  NSPopUpButton *menu = 0;
  NSBox *gbox = 0;
  NSBox *pbox = 0;

  NSRect sv_rect;
  sv_rect.origin.x = sv_rect.origin.y = 0;
  sv_rect.size.width = 320;
  sv_rect.size.height = 240;
  ScreenSaverView *sv = [[ScreenSaverView alloc]  // dummy placeholder
                          initWithFrame:sv_rect
                          isPreview:YES];

  // make a "Preferences" button
  //
  if (! simple_p) {
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.width = rect.size.height = 10;
    pb = [[NSButton alloc] initWithFrame:rect];
    [pb setTitle:@"Preferences"];
    [pb setBezelStyle:NSRoundedBezelStyle];
    [pb sizeToFit];

    rect.origin.x = ([sv frame].size.width -
                     [pb frame].size.width) / 2;
    [pb setFrameOrigin:rect.origin];
  
    // grab the click
    //
    [pb setTarget:self];
    [pb setAction:@selector(openPreferences:)];

    // Make a saver selection menu
    //
    menu = [self makeMenu];
    rect.origin.x = 2;
    rect.origin.y = 2;
    [menu setFrameOrigin:rect.origin];

    // make a box to wrap the saverView
    //
    rect = [sv frame];
    rect.origin.x = 0;
    rect.origin.y = [pb frame].origin.y + [pb frame].size.height;
    gbox = [[NSBox alloc] initWithFrame:rect];
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
    pbox = [[NSBox alloc] initWithFrame:rect];
    [pbox setTitlePosition:NSNoTitle];
    [pbox setBorderType:NSNoBorder];
    [pbox addSubview:gbox];
    if (menu) [pbox addSubview:menu];
    if (pb)   [pbox addSubview:pb];
    [pbox sizeToFit];

    [pb   setAutoresizingMask:NSViewMinXMargin|NSViewMaxXMargin];
    [menu setAutoresizingMask:NSViewMinXMargin|NSViewMaxXMargin];
    [gbox setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [pbox setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
  }

  [sv     setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];


  // and make a window to hold that.
  //
  NSScreen *screen = [NSScreen mainScreen];
  rect = pbox ? [pbox frame] : [sv frame];
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
  [window setMinSize:[window frameRectForContentRect:rect].size];

  [[window contentView] addSubview: (pbox ? (NSView *) pbox : (NSView *) sv)];

  [window makeKeyAndOrderFront:window];
  
  [sv startAnimation]; // this is the dummy saver

  count++;

  return window;
}


- (void)applicationDidFinishLaunching: (NSNotification *) notif
{
  [self listSaverBundleNames];

  int n = ([saverNames count] == 1 ? 1 : 2);
  NSMutableArray *a = [[NSMutableArray arrayWithCapacity: n+1] retain];
  windows = a;
  int i;
  for (i = 0; i < n; i++) {
    NSWindow *window = [self makeWindow];
    // Get the last-saved window position out of preferences.
    [window setFrameAutosaveName:
              [NSString stringWithFormat:@"XScreenSaverWindow%d", i]];
    [window setFrameUsingName:[window frameAutosaveName]];
    [a addObject: window];
  }

  if (n == 1) {
    [self loadSaver:[saverNames objectAtIndex:0]];
  } else {

    /* In the XCode project, each .saver scheme sets this env var when
       launching SaverTester.app so that it knows which one we are
       currently debugging.  If this is set, it overrides the default
       selection in the popup menu.  If unset, that menu persists to
       whatever it was last time.
     */
    const char *forced = getenv ("SELECTED_SAVER");
    if (forced && *forced) {
      NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
      NSString *s = [NSString stringWithCString:(char *)forced
                              encoding:NSUTF8StringEncoding];
      NSLog (@"selecting saver %@", s);
      [prefs setObject:s forKey:@"selectedSaverName"];
    }

    [self selectedSaverDidChange:nil];
  }
}


/* When the window closes, exit (even if prefs still open.)
*/
- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) n
{
  return YES;
}

@end
