/* xscreensaver, Copyright © 2013-2021 Jamie Zawinski <jwz@jwz.org>
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

#define IN_UPDATER

#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import "Updater.h"
#import "Sparkle/SUUpdater.h"

@implementation XScreenSaverUpdater : NSObject

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
  NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
  [defs registerDefaults:UPDATER_DEFAULTS];

  // If it's not time to run the updater, then bail immediately.
  // I'm not sure why this is necessary, but Sparkle seems to be
  // checking too often.
  //
  if (! [self timeToCheck])
    [[NSApplication sharedApplication] terminate:self];

  // If the screen saver is not running, then launch the updater now.
  // Otherwise, wait until the screen saver deactivates, and then do
  // it.  This is because if the updater tries to pop up a dialog box
  // while the screen saver is active, everything goes to hell and it
  // never shows up.  You'd expect the dialog to just map below the
  // screen saver window, but no.

  if (! [self screenSaverActive]) {
    [self runUpdater];
  } else {
    // Run the updater when the "screensaver.didstop" notification arrives.
    [[NSDistributedNotificationCenter defaultCenter]
      addObserver:self
      selector:@selector(saverStoppedNotification:)
      name:@"com.apple.screensaver.didstop"
      object:nil];

    // But I'm not sure I trust that, so also poll every couple minutes.
    timer = [NSTimer scheduledTimerWithTimeInterval: 60 * 2
                     target:self
                     selector:@selector(pollSaverTermination:)
                     userInfo:nil
                     repeats:YES];
  }
}


- (BOOL) timeToCheck
{
  NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
  NSTimeInterval interval = [defs doubleForKey:@SUScheduledCheckIntervalKey];
  NSDate *last = [defs objectForKey:@SULastCheckTimeKey];
  if (!interval || !last)
    return YES;
  NSTimeInterval since = [[NSDate date] timeIntervalSinceDate:last];
  return (since > interval);
}


// Whether ScreenSaverEngine is currently running, meaning screen is blanked.
// There's no easy way to determine this other than scanning the process table.
//
- (BOOL) screenSaverActive
{
  BOOL found = NO;
  NSString *target = @"/ScreenSaverEngine.app";
  ProcessSerialNumber psn = { kNoProcess, kNoProcess };

# pragma clang diagnostic push   // "GetNextProcess deprecated in 10.9"
# pragma clang diagnostic ignored "-Wdeprecated-declarations"

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
# pragma clang diagnostic pop

  return found;
}


- (void) saverStoppedNotification:(NSNotification *)note
{
  [self runUpdater];
}


- (void) pollSaverTermination:(NSTimer *)t
{
  if (! [self screenSaverActive])
    [self runUpdater];
}


- (void) runUpdater
{
  if (timer) {
    [timer invalidate];
    timer = nil;
  }

  SUUpdater *updater = [SUUpdater updaterForBundle:
                                    [NSBundle bundleForClass:[self class]]];
  [updater setDelegate:self];

  // Launch the updater thread.
  [updater checkForUpdatesInBackground];

  // Now we need to wait for the Sparkle thread to finish before we can
  // exit, so just poll waiting for it.
  //
  [NSTimer scheduledTimerWithTimeInterval:1
           target:self
           selector:@selector(pollUpdaterTermination:)
           userInfo:updater
           repeats:YES];
}


// Delegate method that lets us append extra info to the system-info URL.
//
- (NSArray *) feedParametersForUpdater:(SUUpdater *)updater
                  sendingSystemProfile:(BOOL)sending
{
  // Get the name of the saver that invoked us, and include that in the
  // system info.
  NSString *saver = [[[NSProcessInfo processInfo] environment]
                      objectForKey:@"XSCREENSAVER_CLASSPATH"];
  if (! saver) return @[];
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


- (void) pollUpdaterTermination:(NSTimer *)t
{
  SUUpdater *updater = [t userInfo];
  if (![updater updateInProgress])
    [[NSApplication sharedApplication] terminate:self];
}


- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
  return YES;
}

@end
