/* xscreensaver, Copyright (c) 2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * XScreenSaverUpdater.app -- downloads and installs XScreenSaver updates
 * via Sparkle.framework.
 *
 * Created: 7-Dec-2013
 *
 * NOTE: This does not work with Sparkle 1.5b6 -- it requires the "HEAD"
 *       version 4-Dec-2013 or later.
 */

#import "Updater.h"
#import "Sparkle/SUUpdater.h"

@implementation XScreenSaverUpdater : NSObject

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
  NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
  [defs registerDefaults:UPDATER_DEFAULTS];

  SUUpdater *updater = [SUUpdater updaterForBundle:
                                    [NSBundle bundleForClass:[self class]]];
  [updater setDelegate:self];

  [self awaitScreenSaverTermination:updater];
}


// Delegate method that lets us append extra info to the system-info URL.
//
- (NSArray *) feedParametersForUpdater:(SUUpdater *)updater
                  sendingSystemProfile:(BOOL)sending
{
  // Get the name of the saver that invoked us, and include that in the
  // system info.
  NSString *saver = [[[NSProcessInfo 
                        processInfo]environment]objectForKey:
                        @"XSCREENSAVER_CLASSPATH"];
  if (! saver) return nil;
  NSString *head = @"org.jwz.xscreensaver.";
  if ([saver hasPrefix:head])
    saver = [saver substringFromIndex:[head length]];

  return @[ @{ @"key":		@"saver",
               @"value":	saver,
               @"displayKey":	@"Current Saver",
               @"displayValue":	saver
             }
          ];
}


// Whether ScreenSaverEngine is currently running, meaning screen is blanked.
//
- (BOOL) screenSaverActive
{
  BOOL found = NO;
  NSString *target = @"/ScreenSaverEngine.app";
  ProcessSerialNumber psn = { kNoProcess, kNoProcess };
  while (GetNextProcess(&psn) == noErr) {
    CFDictionaryRef cfdict =
      ProcessInformationCopyDictionary (&psn,
        kProcessDictionaryIncludeAllInformationMask);
    if (cfdict) {
      NSDictionary *dict = (NSDictionary *) cfdict;
      NSString *path = [dict objectForKey:@"BundlePath"];
      if (path && [path hasSuffix:target])
        found = YES;
      CFRelease (cfdict);
    }
    if (found)
      break;
  }
  return found;
}


// If the screen saver is not running, then launch the updater.
// Otherwise, wait a while and try again.  This is because if the
// updater tries to pop up a dialog box while the screen saver is
// active, everything goes to hell and it never shows up.
//
- (void) awaitScreenSaverTermination:(SUUpdater *)updater
{
  if ([self screenSaverActive]) {
    static float delay = 1;
    [NSTimer scheduledTimerWithTimeInterval: delay
             target:self
             selector:@selector(awaitScreenSaverTerminationTimer:)
             userInfo:updater
             repeats:NO];
    // slightly exponential back-off
    delay *= 1.3;
    if (delay > 120)
      delay = 120;
  } else {
    // Launch the updater thread.
    [updater checkForUpdatesInBackground];

    // Now we need to wait for the Sparkle thread to finish before we can
    // exit, so just poll waiting for it.
    //
    [NSTimer scheduledTimerWithTimeInterval:1
             target:self
             selector:@selector(exitWhenDone:)
             userInfo:updater
             repeats:YES];
  }
}


- (void) awaitScreenSaverTerminationTimer:(NSTimer *)timer
{
  [self awaitScreenSaverTermination:[timer userInfo]];
}


- (void) exitWhenDone:(NSTimer *)timer
{
  SUUpdater *updater = [timer userInfo];
  if (![updater updateInProgress])
    [[NSApplication sharedApplication] terminate:self];
}


- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
  return YES;
}

@end
