/* xscreensaver, Copyright (c) 2006-2012 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This program serves three purposes:

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

   Third, it is the scaffolding which turns a set of screen savers into
   a single iPhone / iPad program.  In that case, all of the savers are
   linked in to this executable, since iOS does not allow dynamic loading
   of bundles that have executable code in them.  Bleh.
 */

#import <TargetConditionals.h>
#import "SaverRunner.h"
#import "SaverListController.h"
#import "XScreenSaverGLView.h"
#import "yarandom.h"

#ifdef USE_IPHONE

@interface RotateyViewController : UINavigationController
@end

@implementation RotateyViewController
- (BOOL)shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)o
{
  return YES;
}
@end

#endif // USE_IPHONE


@implementation SaverRunner


- (ScreenSaverView *) makeSaverView: (NSString *) module
                           withSize: (NSSize) size
{
  Class new_class = 0;

# ifndef USE_IPHONE

  // Load the XScreenSaverView subclass and code from a ".saver" bundle.

  NSString *name = [module stringByAppendingPathExtension:@"saver"];
  NSString *path = [saverDir stringByAppendingPathComponent:name];

  if (! [[NSFileManager defaultManager] fileExistsAtPath:path]) {
    NSLog(@"bundle \"%@\" does not exist", path);
    return 0;
  }

  NSLog(@"Loading %@", path);

  // NSBundle *obundle = saverBundle;

  saverBundle = [NSBundle bundleWithPath:path];
  if (saverBundle)
    new_class = [saverBundle principalClass];

  // Not entirely unsurprisingly, this tends to break the world.
  // if (obundle && obundle != saverBundle)
  //  [obundle unload];

# else  // USE_IPHONE

  // Determine whether to create an X11 view or an OpenGL view by
  // looking for the "gl" tag in the xml file.  This is kind of awful.

  NSString *path = [saverDir
                     stringByAppendingPathComponent:
                       [[[module lowercaseString]
                          stringByReplacingOccurrencesOfString:@" "
                          withString:@""]
                         stringByAppendingPathExtension:@"xml"]];
  NSString *xml = [NSString stringWithContentsOfFile:path
                            encoding:NSISOLatin1StringEncoding
                            error:nil];
  NSAssert (xml, @"no XML: %@", path);
  Bool gl_p = (xml && [xml rangeOfString:@"gl=\"yes\""].length > 0);

  new_class = (gl_p
               ? [XScreenSaverGLView class]
               : [XScreenSaverView class]);

# endif // USE_IPHONE

  if (! new_class)
    return 0;

  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width  = size.width;
  rect.size.height = size.height;

  XScreenSaverView *instance =
    [(XScreenSaverView *) [new_class alloc]
                          initWithFrame:rect
                          saverName:module
                          isPreview:YES];
  if (! instance) return 0;


  /* KLUGE: Inform the underlying program that we're in "standalone"
     mode, e.g. running as "Phosphor.app" rather than "Phosphor.saver".
     This is kind of horrible but I haven't thought of a more sensible
     way to make this work.
   */
# ifndef USE_IPHONE
  if ([saverNames count] == 1) {
    putenv (strdup ("XSCREENSAVER_STANDALONE=1"));
  }
# endif

  return (ScreenSaverView *) instance;
}


#ifndef USE_IPHONE

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


/* Changes the contents of the menubar menus to correspond to
   the running saver.  Desktop only.
 */
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
    if ([sv isAnimating])
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

#else  // USE_IPHONE


- (UIImage *) screenshot
{
  return saved_screenshot;
}

- (void) saveScreenshot
{
  // Most of this is from:
  // http://developer.apple.com/library/ios/#qa/qa1703/_index.html
  // The rotation stuff is by me.

  CGSize size = [[UIScreen mainScreen] bounds].size;

  UIInterfaceOrientation orient =
    [[window rootViewController] interfaceOrientation];
  if (orient == UIInterfaceOrientationLandscapeLeft ||
      orient == UIInterfaceOrientationLandscapeRight) {
    // Rotate the shape of the canvas 90 degrees.
    double s = size.width;
    size.width = size.height;
    size.height = s;
  }


  // Create a graphics context with the target size
  // On iOS 4 and later, use UIGraphicsBeginImageContextWithOptions to
  // take the scale into consideration
  // On iOS prior to 4, fall back to use UIGraphicsBeginImageContext

  if (UIGraphicsBeginImageContextWithOptions)
    UIGraphicsBeginImageContextWithOptions (size, NO, 0);
  else
    UIGraphicsBeginImageContext (size);

  CGContextRef ctx = UIGraphicsGetCurrentContext();


  // Rotate the graphics context to match current hardware rotation.
  //
  switch (orient) {
  case UIInterfaceOrientationPortraitUpsideDown:
    CGContextTranslateCTM (ctx,  [window center].x,  [window center].y);
    CGContextRotateCTM (ctx, M_PI);
    CGContextTranslateCTM (ctx, -[window center].x, -[window center].y);
    break;
  case UIInterfaceOrientationLandscapeLeft:
  case UIInterfaceOrientationLandscapeRight:
    CGContextTranslateCTM (ctx,  
                           ([window frame].size.height -
                            [window frame].size.width) / 2,
                           ([window frame].size.width -
                            [window frame].size.height) / 2);
    CGContextTranslateCTM (ctx,  [window center].x,  [window center].y);
    CGContextRotateCTM (ctx, 
                        (orient == UIInterfaceOrientationLandscapeLeft
                         ?  M_PI/2
                         : -M_PI/2));
    CGContextTranslateCTM (ctx, -[window center].x, -[window center].y);
    break;
  default:
    break;
  }

  // Iterate over every window from back to front
  //
  for (UIWindow *win in [[UIApplication sharedApplication] windows]) {
    if (![win respondsToSelector:@selector(screen)] ||
        [win screen] == [UIScreen mainScreen]) {

      // -renderInContext: renders in the coordinate space of the layer,
      // so we must first apply the layer's geometry to the graphics context
      CGContextSaveGState (ctx);

      // Center the context around the window's anchor point
      CGContextTranslateCTM (ctx, [win center].x, [win center].y);

      // Apply the window's transform about the anchor point
      CGContextConcatCTM (ctx, [win transform]);

      // Offset by the portion of the bounds left of and above anchor point
      CGContextTranslateCTM (ctx,
        -[win bounds].size.width  * [[win layer] anchorPoint].x,
        -[win bounds].size.height * [[win layer] anchorPoint].y);

      // Render the layer hierarchy to the current context
      [[win layer] renderInContext:ctx];

      // Restore the context
      CGContextRestoreGState (ctx);
    }
  }

  if (saved_screenshot)
    [saved_screenshot release];
  saved_screenshot = [UIGraphicsGetImageFromCurrentImageContext() retain];

  UIGraphicsEndImageContext();
}


- (void) openPreferences: (NSString *) saver
{
  [self loadSaver:saver launch:NO];
  if (! saverView) return;

  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  [prefs setObject:saver forKey:@"selectedSaverName"];
  [prefs synchronize];

  [rootViewController pushViewController: [saverView configureView]
                      animated:YES];
}


#endif // USE_IPHONE



- (void)loadSaver:(NSString *)name launch:(BOOL)launch
{
  // NSLog (@"selecting saver \"%@\"", name);

# ifndef USE_IPHONE

  if (saverName && [saverName isEqualToString: name]) {
    if (launch)
      for (NSWindow *win in windows) {
        ScreenSaverView *sv = find_saverView ([win contentView]);
        if (![sv isAnimating])
          [sv startAnimation];
      }
    return;
  }

  saverName = name;

  for (NSWindow *win in windows) {
    NSView *cv = [win contentView];
    NSString *old_title = [win title];
    if (!old_title) old_title = @"XScreenSaver";
    [win setTitle: name];
    relabel_menus (menubar, old_title, name);

    ScreenSaverView *old_view = find_saverView (cv);
    NSView *sup = old_view ? [old_view superview] : cv;

    if (old_view) {
      if ([old_view isAnimating])
        [old_view stopAnimation];
      [old_view removeFromSuperview];
    }

    NSSize size = [cv frame].size;
    ScreenSaverView *new_view = [self makeSaverView:name withSize: size];
    NSAssert (new_view, @"unable to make a saver view");

    [new_view setFrame: (old_view ? [old_view frame] : [cv frame])];
    [sup addSubview: new_view];
    [win makeFirstResponder:new_view];
    [new_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [new_view retain];
    if (launch)
      [new_view startAnimation];
  }

  NSUserDefaultsController *ctl =
    [NSUserDefaultsController sharedUserDefaultsController];
  [ctl save:self];

# else  // USE_IPHONE

  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  [prefs setObject:name forKey:@"selectedSaverName"];
  [prefs synchronize];

  if (saverName && [saverName isEqualToString: name]) {
    if ([saverView isAnimating])
      return;
    else
      goto LAUNCH;
  }

  saverName = name;

  if (saverView) {
    if ([saverView isAnimating])
      [saverView stopAnimation];
    [saverView removeFromSuperview];
  }

  NSSize size = [window frame].size;
  saverView = [self makeSaverView:name withSize: size];

  if (! saverView) {
    [[[UIAlertView alloc] initWithTitle: name
                          message: @"Unable to load!"
                          delegate: nil
                          cancelButtonTitle: @"Bummer"
                          otherButtonTitles: nil]
     show];
    return;
  }

  [saverView setFrame: [window frame]];
  [saverView retain];
  [[NSNotificationCenter defaultCenter]
    addObserver:saverView
    selector:@selector(didRotate:)
    name:UIDeviceOrientationDidChangeNotification object:nil];

 LAUNCH:
  if (launch) {
    [self saveScreenshot];
    [window addSubview: saverView];
    [saverView becomeFirstResponder];
    [saverView startAnimation];
  }
# endif // USE_IPHONE
}


- (void)loadSaver:(NSString *)name
{
  [self loadSaver:name launch:YES];
}


# ifndef USE_IPHONE

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

# endif // USE_IPHONE



- (void)selectedSaverDidChange:(NSDictionary *)change
{
  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  NSString *name = [prefs stringForKey:@"selectedSaverName"];

  if (! name) return;

  if (! [saverNames containsObject:name]) {
    NSLog (@"saver \"%@\" does not exist", name);
    return;
  }

  [self loadSaver: name];
}


- (NSArray *) listSaverBundleNamesInDir:(NSString *)dir
{
# ifndef USE_IPHONE
  NSString *ext = @"saver";
# else
  NSString *ext = @"xml";
# endif

  NSArray *files = [[NSFileManager defaultManager]
                     contentsOfDirectoryAtPath:dir error:nil];
  if (! files) return 0;
  NSMutableArray *result = [NSMutableArray arrayWithCapacity: [files count]+1];

  for (NSString *p in files) {
    if ([[p pathExtension] caseInsensitiveCompare: ext]) 
      continue;

# ifndef USE_IPHONE
    NSString *name = [[p lastPathComponent] stringByDeletingPathExtension];
# else  // !USE_IPHONE

    // Get the saver name's capitalization right by reading the XML file.

    p = [dir stringByAppendingPathComponent: p];
    NSString *name = [NSString stringWithContentsOfFile:p
                               encoding:NSISOLatin1StringEncoding
                               error:nil];
    NSRange r = [name rangeOfString:@"_label=\"" options:0];
    name = [name substringFromIndex: r.location + r.length];
    r = [name rangeOfString:@"\"" options:0];
    name = [name substringToIndex: r.location];

    NSAssert1 (name, @"no name in %@", p);

# endif // !USE_IPHONE

    [result addObject: name];
  }

  return result;
}



- (NSArray *) listSaverBundleNames
{
  NSMutableArray *dirs = [NSMutableArray arrayWithCapacity: 10];

# ifndef USE_IPHONE
  // On MacOS, look in the "Contents/PlugIns/" directory in the bundle.
  [dirs addObject: [[NSBundle mainBundle] builtInPlugInsPath]];

  // Also look in the same directory as the executable.
  [dirs addObject: [[[NSBundle mainBundle] bundlePath]
                     stringByDeletingLastPathComponent]];

  // Finally, look in standard MacOS screensaver directories.
  [dirs addObject: @"~/Library/Screen Savers"];
  [dirs addObject: @"/Library/Screen Savers"];
  [dirs addObject: @"/System/Library/Screen Savers"];

# else
  // On iOS, just look in the bundle's root directory.
  [dirs addObject: [[NSBundle mainBundle] bundlePath]];
# endif

  int i;
  for (i = 0; i < [dirs count]; i++) {
    NSString *dir = [dirs objectAtIndex:i];
    NSArray *names = [self listSaverBundleNamesInDir:dir];
    if (! names) continue;
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
  return [NSArray array];
}


/* Create the popup menu of available saver names.
 */
#ifndef USE_IPHONE

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

#else  // USE_IPHONE

/* Create a dictionary of one-line descriptions of every saver,
   for display on the UITableView.
 */
- (NSDictionary *)makeDescTable
{
  NSMutableDictionary *dict = 
    [NSMutableDictionary dictionaryWithCapacity:[saverNames count]];

  for (NSString *saver in saverNames) {
    NSString *desc = 0;
    NSString *path = [saverDir stringByAppendingPathComponent:
                                 [[saver lowercaseString]
                                   stringByReplacingOccurrencesOfString:@" "
                                   withString:@""]];
    NSRange r;

    path = [path stringByAppendingPathExtension:@"xml"];
    desc = [NSString stringWithContentsOfFile:path
                     encoding:NSISOLatin1StringEncoding
                     error:nil];
    if (! desc) goto FAIL;

    r = [desc rangeOfString:@"<_description>"
              options:NSCaseInsensitiveSearch];
    if (r.length == 0) {
      desc = 0;
      goto FAIL;
    }
    desc = [desc substringFromIndex: r.location + r.length];
    r = [desc rangeOfString:@"</_description>"
              options:NSCaseInsensitiveSearch];
    if (r.length > 0)
      desc = [desc substringToIndex: r.location];

    // Leading and trailing whitespace.
    desc = [desc stringByTrimmingCharactersInSet:
                   [NSCharacterSet whitespaceAndNewlineCharacterSet]];

    // Let's see if we can find a year on the last line.
    r = [desc rangeOfString:@"\n" options:NSBackwardsSearch];
    NSString *year = 0;
    for (NSString *word in
           [[desc substringFromIndex:r.location + r.length]
             componentsSeparatedByCharactersInSet:
               [NSCharacterSet characterSetWithCharactersInString:
                                 @" \t\n-."]]) {
      int n = [word doubleValue];
      if (n > 1970 && n < 2100)
        year = word;
    }

    // Delete everything after the first blank line.
    r = [desc rangeOfString:@"\n\n" options:0];
    if (r.length > 0)
      desc = [desc substringToIndex: r.location];

    // Truncate really long ones.
    int max = 140;
    if ([desc length] > max)
      desc = [desc substringToIndex: max];

    if (year)
      desc = [year stringByAppendingString:
                     [@": " stringByAppendingString: desc]];

  FAIL:
    if (! desc) {
      desc = @"Oops, this module appears to be incomplete.";
      // NSLog(@"broken saver: %@", path);
    }

    [dict setObject:desc forKey:saver];
  }

  return dict;
}


#endif // USE_IPHONE



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


# ifndef USE_IPHONE

/* Create the desktop window shell, possibly including a preferences button.
 */
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
  
  NSWindow *win = [[NSWindow alloc]
                      initWithContentRect:rect
                                styleMask:(NSTitledWindowMask |
                                           NSClosableWindowMask |
                                           NSMiniaturizableWindowMask |
                                           NSResizableWindowMask)
                                  backing:NSBackingStoreBuffered
                                    defer:YES
                                   screen:screen];
  [win setMinSize:[win frameRectForContentRect:rect].size];
  [[win contentView] addSubview: (pbox ? (NSView *) pbox : (NSView *) sv)];

  [win makeKeyAndOrderFront:win];
  
  [sv startAnimation]; // this is the dummy saver

  count++;

  return win;
}

# endif // !USE_IPHONE


- (void)applicationDidFinishLaunching:
# ifndef USE_IPHONE
    (NSNotification *) notif
# else  // USE_IPHONE
    (UIApplication *) application
# endif // USE_IPHONE
{
  [self listSaverBundleNames];

# ifndef USE_IPHONE
  int window_count = ([saverNames count] <= 1 ? 1 : 2);
  NSMutableArray *a = [[NSMutableArray arrayWithCapacity: window_count+1]
                        retain];
  windows = a;

  int i;
  // Create either one window (for standalone, e.g. Phosphor.app)
  // or two windows for SaverTester.app.
  for (i = 0; i < window_count; i++) {
    NSWindow *win = [self makeWindow];
    // Get the last-saved window position out of preferences.
    [win setFrameAutosaveName:
              [NSString stringWithFormat:@"XScreenSaverWindow%d", i]];
    [win setFrameUsingName:[win frameAutosaveName]];
    [a addObject: win];
  }
# else  // USE_IPHONE

# undef ya_rand_init
  ya_rand_init (0);	// Now's a good time.

  rootViewController = [[[RotateyViewController alloc] init] retain];
  [window setRootViewController: rootViewController];

  SaverListController *menu = [[SaverListController alloc] 
                                initWithNames:saverNames
                                descriptions:[self makeDescTable]];
  [rootViewController pushViewController:menu animated:YES];
  [menu becomeFirstResponder];

  [window makeKeyAndVisible];
  [window setAutoresizesSubviews:YES];
  [window setAutoresizingMask: 
            (UIViewAutoresizingFlexibleWidth | 
             UIViewAutoresizingFlexibleHeight)];

  application.applicationSupportsShakeToEdit = YES;

# endif // USE_IPHONE

  NSString *forced = 0;
  /* In the XCode project, each .saver scheme sets this env var when
     launching SaverTester.app so that it knows which one we are
     currently debugging.  If this is set, it overrides the default
     selection in the popup menu.  If unset, that menu persists to
     whatever it was last time.
   */
  const char *f = getenv ("SELECTED_SAVER");
  if (f && *f)
    forced = [NSString stringWithCString:(char *)f
                       encoding:NSUTF8StringEncoding];

  if (forced && ![saverNames containsObject:forced]) {
    NSLog(@"forced saver \"%@\" does not exist", forced);
    forced = 0;
  }

  // If there's only one saver, run that.
  if (!forced && [saverNames count] == 1)
    forced = [saverNames objectAtIndex:0];

  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];

# ifdef USE_IPHONE
  NSString *prev = [prefs stringForKey:@"selectedSaverName"];

  if (forced)
    prev = forced;

  // If nothing was selected (e.g., this is the first launch)
  // then scroll randomly instead of starting up at "A".
  //
  if (!prev)
    prev = [saverNames objectAtIndex: (random() % [saverNames count])];

  if (prev)
    [menu scrollTo: prev];
# endif // USE_IPHONE

  if (forced)
    [prefs setObject:forced forKey:@"selectedSaverName"];

# ifdef USE_IPHONE
  /* Don't auto-launch the saver unless it was running last time.
     XScreenSaverView manages this, on crash_timer.
   */
  if (! [prefs boolForKey:@"wasRunning"])
    return;
# endif

  [self selectedSaverDidChange:nil];
}


#ifndef USE_IPHONE

/* When the window closes, exit (even if prefs still open.)
*/
- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) n
{
  return YES;
}

# else // USE_IPHONE

- (void)applicationWillResignActive:(UIApplication *)app
{
  [(XScreenSaverView *)view setScreenLocked:YES];
}

- (void)applicationDidBecomeActive:(UIApplication *)app
{
  [(XScreenSaverView *)view setScreenLocked:NO];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
  [(XScreenSaverView *)view setScreenLocked:YES];
}

#endif // USE_IPHONE


@end
