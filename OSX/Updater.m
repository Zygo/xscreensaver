/* xscreensaver, Copyright © 2013-2023 Jamie Zawinski <jwz@jwz.org>
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
 */

#define IN_UPDATER

#import "Updater.h"
#import "Sparkle/SPUUpdater.h"
#import "Sparkle/SPUStandardUpdaterController.h"
#import "nslog.h"

@implementation XScreenSaverUpdater
{
  NSTimer *timer;
  SPUStandardUpdaterController *updater;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
  NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
  [defs registerDefaults:UPDATER_DEFAULTS];

  updater = [[[SPUStandardUpdaterController alloc]
               initWithUpdaterDelegate: self
                    userDriverDelegate: self]
              retain];

  if (! [self timeToCheck]) {
    NSLog (@"updater: not checking");
    [[NSApplication sharedApplication] terminate:self];
  }

  // On macOS 10.15, if we tried to pop up the Sparkle dialog while the
  // screen was blanked, the dialog never showed up.  So on such systems,
  // we wait until the screen becomes un-blanked before running the updater.
  //
  // On macOS 14.0, it works just fine to pop up the dialog while the screen
  // is blanked: it becomes visible once the screen un-blanks, as you'd
  // expect.  Which is good, because we also have no way of knowing whether
  // the screen is blanked.
  //
  if (! [self screenSaverActive]) {
    NSLog (@"updater: running immediately");
    [self runUpdater];
  } else {
    NSLog (@"updater: waiting for screen to unblank");
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


// This is mostly informational now, since XScreenSaverView checkForUpdates
// also does this test, and only invokes us if it is time.
//
- (BOOL) timeToCheck
{
  NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];

  // This must always be small so we do an update each-ish time we're launched.
  // We check it here but so does the Sparkle bundle.
  NSTimeInterval interval = 60 * 60 * 24;
  [defs setObject: [NSNumber numberWithInt: interval]
           forKey: @SUScheduledCheckIntervalKey];
  updater.updater.updateCheckInterval = interval;

  NSDate *last_check = [defs objectForKey:@SULastCheckTimeKey];
  if ([last_check isKindOfClass:[NSString class]]) {
    NSString *s = (NSString *) last_check;
    if (!s.length) {
      last_check = nil;
    } else {
      NSDateFormatter *f = [[NSDateFormatter alloc] init];
      [f setDateFormat:@"yyyy-MM-dd HH:mm:ss ZZZZZ"];
      [f setTimeZone: [NSTimeZone timeZoneForSecondsFromGMT: 0]];
      last_check = [f dateFromString: s];
    }
  }

  if (!last_check) {
    NSLog (@"updater: never checked for updates (interval %d days)",
           (int) (interval / (60 * 60 * 24)));
    return YES;
  } else {
    NSTimeInterval elapsed = -[last_check timeIntervalSinceNow];
    if (elapsed < interval) {
      NSLog (@"updater: last checked for updates %d days ago, skipping check"
             " (interval %d days)",
             (int) (elapsed / (60 * 60 * 24)),
             (int) (interval / (60 * 60 * 24)));
      return NO;
    } else {
      NSLog (@"updater: last checked for updates %d days ago"
             " (interval %d days)",
             (int) (elapsed / (60 * 60 * 24)),
             (int) (interval / (60 * 60 * 24)));
      return YES;
    }
  }
}


// Whether ScreenSaverEngine is currently running, meaning screen is blanked.
// There's no easy way to determine this other than scanning the process table.
// This always returns false as of macOS 14.0.
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
  NSLog (@"updater: screen unblanked");
  [self runUpdater];
}


- (void) pollSaverTermination:(NSTimer *)t
{
  if (! [self screenSaverActive]) {
    NSLog (@"updater: screen unblanked, polled");
    [self runUpdater];
  }
}


- (void) runUpdater
{
  if (timer) {
    [timer invalidate];
    timer = nil;
  }

  // Launch the updater thread.
  [updater.updater checkForUpdatesInBackground];

  // Now we need to wait for the Sparkle thread to finish before we can
  // exit, so just poll waiting for it.
  //
  NSLog (@"updater: waiting for sparkle");
  [NSTimer scheduledTimerWithTimeInterval: 1
                                   target: self
                                 selector: @selector(pollUpdaterTermination:)
                                 userInfo: nil
                                  repeats: YES];
}


- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
  return YES;
}

- (void) pollUpdaterTermination:(NSTimer *)t
{
  if ([updater.updater sessionInProgress]) {
    // NSLog (@"updater: waiting for sparkle");
  } else {
    NSLog (@"updater: sparkle finished (polled)");
    [[NSApplication sharedApplication] terminate:self];
  }
}

- (void) updater: (SPUUpdater *) updater
         didFinishUpdateCycleForUpdateCheck: (SPUUpdateCheck) uc
           error: (NSError *) err
{
  if (err)
    NSLog (@"updater: finished with error: %@", err);
  else
    NSLog (@"updater: finished");
  [[NSApplication sharedApplication] terminate:self];
}


//
// Sparkle configuration
//

- (BOOL) updaterShouldRelaunchApplication: (SPUStandardUpdaterController *) u
{
  return NO;  // No need for Sparkle to re-launch XScreenSaverUpdater
}

// When the updater loads updates.xml, it appends some URL parameters with
// OS and hardware versions.  This adds the currently-running saver name to
// that list.  Except that as of 10.15 or so it longer works, thanks to
// sandboxing.  The saver can't communicate shit to the updater, including
// its name.
//
- (NSArray *) feedParametersForUpdater:(SPUStandardUpdaterController *) u
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


// Just add some more logging to various delegate hooks.


- (void) updater: (SPUStandardUpdaterController *) u
         didFinishLoadingAppcast: (SUAppcast *) appcast
{
  NSLog (@"updater: sparkle finished loading %@", appcast);
}

- (BOOL) updaterShouldShowUpdateAlertForScheduledUpdate:
         (SPUStandardUpdaterController *) u
         forItem: (SUAppcastItem *) item
{
  NSLog (@"updater: update alert: %@", item);
  return YES;
}

- (void) updater: (SPUStandardUpdaterController *) u
         didDismissUpdateAlertPermanently: (BOOL) permanently
         forItem: (SUAppcastItem *) item
{
  NSLog (@"updater: dismissed: %@", item);
}

- (void) updaterDidNotFindUpdate:(SPUStandardUpdaterController *) u
{
  NSLog (@"updater: no updates");
}

- (void) updater: (SPUStandardUpdaterController *) u
         willDownloadUpdate: (SUAppcastItem *) item
         withRequest: (NSMutableURLRequest *) request
{
  NSLog (@"updater: downloading %@", item);
}

- (void) updater: (SPUStandardUpdaterController *) u
         didDownloadUpdate: (SUAppcastItem *) item
{
  NSLog (@"updater: downloaded %@", item);
}

- (void) updater: (SPUStandardUpdaterController *) u
         failedToDownloadUpdate: (SUAppcastItem *) item
         error: (NSError *) error
{
  NSLog (@"updater: download failed: %@ %@", item, error);
}

- (void) userDidCancelDownload: (SPUStandardUpdaterController *) u
{
  NSLog (@"updater: download cancelled");
}

- (void) updater: (SPUStandardUpdaterController *) u
         willExtractUpdate: (SUAppcastItem *) item
{
  NSLog (@"updater: extracting %@", item);
}

- (void) updater: (SPUStandardUpdaterController *) u
         didExtractUpdate: (SUAppcastItem *) item
{
  NSLog (@"updater: extracted %@", item);
}

- (void) updater: (SPUStandardUpdaterController *) u
         willInstallUpdate: (SUAppcastItem *) item
{
  NSLog (@"updater: installing %@", item);
}

- (void) updater: (SPUStandardUpdaterController *) u
         didAbortWithError: (NSError *) error
{
  NSLog (@"updater: failed: %@", error);
}

@end
