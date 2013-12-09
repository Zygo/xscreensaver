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
  SUUpdater *updater = [SUUpdater updaterForBundle:
                                    [NSBundle bundleForClass:[self class]]];
  NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
  [defs registerDefaults:UPDATER_DEFAULTS];

  // Launch the updater thread.
  [updater checkForUpdatesInBackground];

  // Now we need to wait for the Sparkle thread to finish before we can
  // exit, so just poll waiting for it.
  [NSTimer scheduledTimerWithTimeInterval:1
           target:self
           selector:@selector(exitWhenDone:)
           userInfo:updater
           repeats:YES];
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
