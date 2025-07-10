/* xscreensaver, Copyright © 2020-2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a screen saver that runs other screen savers, and then stops them
   and runs a different one after a timeout.
 */

#import "Randomizer.h"
#import "XScreenSaverView.h"
#import "yarandom.h"
#include <sys/sysctl.h>
#import "nslog.h"

#define VENTURA_KLUDGE
#define SONOMA_KLUDGE
#undef  CATCH_SIGNALS

# undef ya_rand_init
# undef abort

#import <IOKit/graphics/IOGraphicsLib.h>  // For IODisplayCreateInfoDictionary
#import <CommonCrypto/CommonDigest.h>
#import <CommonCrypto/CommonHMAC.h>

#define CYCLE_TIME "cycleTime"
#define CYCLE_DEFAULT (5 * 60)


// Returns a string that (hopefully) uniquely identifies the display hardware
// associated with this NSScreen in such a way that it will persist across
// reboots.  Also the pretty name of that screen.
//
static NSArray *
screen_id (NSScreen *screen)
{
  NSData   *edid = NULL;
  NSString *name = NULL;

  NSDictionary *d = [screen deviceDescription];

  // This is the CGDirectDisplayID. "A display ID can persist across processes
  // and typically remains constant until the machine is restarted."  It does
  // not always persist across reboots.
  //
  unsigned long id = [[d objectForKey:@"NSScreenNumber"] unsignedLongValue];

  // Aug 2022: CGDisplayIOServicePort has been deprecated since 10.9, and as
  // of 12.5, it always returns NULL, at least on M1 hardware.  Perhaps this
  // should be #if __x86_64__ or !__arm64__?
  //
# pragma clang diagnostic push   // "CGDisplayIOServicePort deprecated in 10.9"
# pragma clang diagnostic ignored "-Wdeprecated-declarations"
  io_service_t dport = CGDisplayIOServicePort (id);
# pragma clang diagnostic pop

  if (dport != MACH_PORT_NULL) {
    NSDictionary *d2 = (NSDictionary *)  // CFDictionaryRef
      IODisplayCreateInfoDictionary (dport, kIODisplayOnlyPreferredName);

    // This returns the EDID as a 256-bytes binary blob that contains, among
    // other things, the manufacturer, product ID, and unique serial number.
    //
    edid = [[d2 objectForKey:[NSString stringWithUTF8String:kIODisplayEDIDKey]]
             retain];

    // Now get the human-readable name of the screen.
    // The dict has an entry like: "DisplayProductName": { "en_US": "iMac" };
    //
    NSDictionary *names =
      [d2 objectForKey: [NSString stringWithUTF8String:kDisplayProductName]];
    if (names && [names count] > 0)
      name = [[names objectForKey:[[names allKeys] objectAtIndex:0]] retain];

    [d2 release];
  }

  if (! edid) {
    //
    // If we weren't able to retrieve the full 256-byte EDID above, hopefully
    // these three numbers also uniquely identify the hardware.
    //
    // However, they won't always.  I have an "EDID Ghost" adapter (a dongle
    // to prevent resolution negotiation) whose serial number is 0.  Maybe if
    // I had two of them, even the 256-byte version would be non-unique.
    //
    // We could also dig through IOServiceMatching("IOMobileFramebuffer") in
    // order to find more info about the displays in the "DisplayAttributes"
    // dictionary (see "ioreg -lw0") but that probably won't give us anything
    // more unique than the above.  In particular, that dict contains an
    // entry for "EDID UUID" which is a hex string of 16 bytes that does
    // *not* distinguish between two different monitors of the same model.
    //
    uint32_t bin[3];
    bin[0] = CGDisplayVendorNumber (id);
    bin[1] = CGDisplayModelNumber  (id);
    bin[2] = CGDisplaySerialNumber (id);
    if (bin[0] || bin[1] || bin[2])
      edid = [[NSData dataWithBytes:bin length:sizeof(bin)] retain];
  }

  if (!name || !name.length) {
    //
    // We might have gotten the name from CGDisplayIOServicePort ->
    // kIODisplayOnlyPreferredName.  If not, use the localized display name.
    // which was introduced in macOS 10.15.  Maybe those are identical?
    //
    if (@available (macOS 10.15, *))
      name = [screen localizedName];
    if (!name || !name.length)
      name = @"unknown";
  }

  if (! edid) {
    //
    // If we weren't able to find the EDID at all, use the display name, size
    // and position, and hope that is invariant across boots.  Good luck!
    //
    edid = [[[NSString stringWithFormat:@"%dx%d @ %d+%d, %@",
                       (int) screen.frame.size.width,
                       (int) screen.frame.size.height,
                       (int) screen.frame.origin.x, 
                       (int) screen.frame.origin.y,
                       name]
              dataUsingEncoding: NSUTF8StringEncoding]
             retain];
  }

  //
  // The EDID might be only 12 bytes, or it might be 256 bytes, which is
  // annoyingly big.  Or somewhere in between.  So if it's more than 32 bytes,
  // hash it first.  After base64, the resource key will be no more than 43
  // characters.
  //
  if (edid.length > CC_SHA256_DIGEST_LENGTH) {
    const char *bytes = [edid bytes];
    unsigned char out[CC_SHA256_DIGEST_LENGTH + 1];
    CC_SHA256 (bytes, edid.length, out);
    edid = [[NSData dataWithBytes: out
                           length: CC_SHA256_DIGEST_LENGTH]
             retain];
  }

  NSString *b64 = [edid base64EncodedStringWithOptions:0];

  // Replace irritating b64 characters with safer ones.
  b64 = [[[b64 stringByReplacingOccurrencesOfString:@"+" withString:@"-"]
               stringByReplacingOccurrencesOfString:@"/" withString:@"_"]
               stringByReplacingOccurrencesOfString:@"=" withString:@""];

  return @[ b64, name ];  // Analyzer says name leaks?
}


static BOOL
get_boolean (NSString *key, NSUserDefaultsController *prefs)
{
  if (! key) return NO;
  NSObject *o = [[prefs defaults] objectForKey: key];
  if (!o) {
    return NO;
  } else if ([o isKindOfClass:[NSNumber class]]) {
    double n = [(NSNumber *) o doubleValue];
    return !!n;
  } else if ([o isKindOfClass:[NSString class]]) {
    NSString *ss = [((NSString *) o) lowercaseString];
    return ([ss isEqualToString:@"true"] ||
            [ss isEqualToString:@"yes"] ||
            [ss isEqualToString:@"1"]);
  } else {
    return NO;
  }
}


static NSString *
resource_key_for_name (NSString *s, BOOL screen_p, NSDictionary *screen_ids)
{
  if (screen_p) {
    NSString *id = [screen_ids objectForKey: s];
    return [NSString stringWithFormat:@"screen_%@_disabled", id];
  } else {
    // Keep risky characters out of the preferences database.
    NSMutableCharacterSet *set =
      [NSMutableCharacterSet whitespaceAndNewlineCharacterSet];
    [set formUnionWithCharacterSet:[NSCharacterSet controlCharacterSet]];
    [set formUnionWithCharacterSet:[NSCharacterSet punctuationCharacterSet]];
    [set formUnionWithCharacterSet:[NSCharacterSet symbolCharacterSet]];
    [set formUnionWithCharacterSet:[NSCharacterSet illegalCharacterSet]];
    s = [[[s componentsSeparatedByCharactersInSet: set]
          componentsJoinedByString: @""]
          lowercaseString];
    return [NSString stringWithFormat:@"saver_%@_disabled", s];
  }
}


@interface RandomizerConfig : NSPanel <NSTableViewDelegate,
                                       NSTableViewDataSource>
- (id)initWithSaverNames: (NSArray *)sn
                   prefs: (NSUserDefaultsController *)p;
@end

@interface RandomizerAttractMode : ScreenSaverView {}
- (id)initWithFrame:(NSRect)frame isPreview:(BOOL)p
                           saverBundlePaths:(NSArray *)_paths;
@end

@interface ExponentialDelayValueTransformer: NSValueTransformer {}
@end

@implementation Randomizer
{
  ScreenSaverView *saver1;  // Currently running, or fading out.
  ScreenSaverView *saver2;  // Fading in, moved to saver1 when in.
  NSTimer *cycle_timer;
  NSUserDefaultsController *prefs;
  NSArray *bundle_paths_all;
  NSArray *bundle_paths_enabled;
  BOOL first_time_p;
  int fade_duration;
  Bool animating_p;
  Bool preview_p;
  NSTextField *crash_label;
  time_t launch_time, cycle_time;
  int cycle_count;
  enum { JUMPCUT, FADE, CROSSFADE } crossfade_mode;
}

#ifdef VENTURA_KLUDGE   // Duplicated in XScreenSaverView.m
static NSMutableArray *all_saver_views = NULL;
#endif


- (id)initWithFrame:(NSRect)frame isPreview:(BOOL)p
{
  NSLog (@"%@: initWithFrame %gx%g+%g+%g, %d", [self blurb],
         frame.size.width, frame.size.height, frame.origin.x, frame.origin.y,
         p);

  // On macOS 10.15, isPreview is always YES, so if the window is big,
  // assume it should be NO.  Fixed in macOS 11.0.
  if (p && frame.size.width >= 640)
    p = NO;

//  if (p && getenv("SELECTED_SAVER")) // Running under SaverTester
//    p = NO;

  self = [super initWithFrame:frame isPreview:p];
  if (! self) return 0;

  ya_rand_init (0);

  preview_p      = p;
  first_time_p   = TRUE;
  crossfade_mode = CROSSFADE;  // Maybe make this a preference?
  fade_duration  = 5;
  launch_time    = time ((time_t *) 0);
  cycle_time     = launch_time;

  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSAssert1 (nsb, @"no bundle for class %@", [self class]);
  NSString *name = [nsb bundleIdentifier];
  NSAssert1 (name && [name length],
             @"no bundle identifier for class %@", [self class]);
  NSUserDefaults *defs = [ScreenSaverDefaults defaultsForModuleWithName:name];
  NSDictionary *extras = @{

    @CYCLE_TIME: [NSString stringWithFormat:@"%d", CYCLE_DEFAULT],

    // Let's un-check these shitty Apple savers by default.
    // #### Oh, except this makes them un-selectable, since we delete
    // entries rather than setting them to 0. Oh well. Bummer.
    @"saver_albumartwork_disabled":    [NSNumber numberWithBool:TRUE],
    @"saver_floatingmessage_disabled": [NSNumber numberWithBool:TRUE],
    @"saver_hello_disabled":           [NSNumber numberWithBool:TRUE],
    @"saver_itunesartwork_disabled":   [NSNumber numberWithBool:TRUE],
    @"saver_monterey_disabled":        [NSNumber numberWithBool:TRUE],
    @"saver_wordoftheday_disabled":    [NSNumber numberWithBool:TRUE],
  };
  NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:100];
  // All the example code does this, but it makes it impossible for us to
  // check any saver that was un-checked at startup:
  // [dict addEntriesFromDictionary: [defs dictionaryRepresentation]];
  [dict addEntriesFromDictionary: extras];
  [defs registerDefaults: dict];

  prefs = [[[NSUserDefaultsController alloc] initWithDefaults:defs
                                                initialValues:nil] retain];
  prefs.appliesImmediately = YES;  // Doesn't work

  [self getSaverPaths];

  // Text field for showing crashes
  //
  int margin = 16;
  frame.origin.x = frame.origin.y = margin;
  frame.size.width  = self.frame.size.width  - margin*2;
  frame.size.height = self.frame.size.height - margin*2;
  crash_label = [[NSTextField alloc] initWithFrame:frame];
  int fs = [NSFont systemFontSize] * 2;
  NSFont *f = [NSFont fontWithName:@"Andale Mono" size: fs];
  if (!f) f = [NSFont fontWithName:@"Courier Bold" size: fs];
  if (!f) f = [NSFont boldSystemFontOfSize: fs];
  [crash_label setFont: f];
  crash_label.cell.wraps = YES;
  [crash_label setStringValue:@""];
  crash_label.selectable = NO;
  crash_label.editable = NO;
  crash_label.bezeled = NO;
  crash_label.drawsBackground = NO;
  crash_label.textColor = [NSColor greenColor];
  crash_label.backgroundColor = [NSColor blackColor];
  crash_label.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [crash_label retain];

#if 0
# ifndef __OPTIMIZE__
  // Dump the entire resource database.
  NSLog (@"%@: defaults for %@", [self blurb], name);
  NSDictionary *d = [[prefs defaults] dictionaryRepresentation];
  for (NSObject *key in [[d allKeys]
                          sortedArrayUsingSelector:@selector(compare:)]) {
    NSObject *val = [d objectForKey:key];
    NSLog (@"  %@ = %@", key, val);
  }
    NSLog (@"---");
# endif
#endif

#ifdef VENTURA_KLUDGE
  if (! p) {  // isPreview
    if (!all_saver_views) {
      all_saver_views = [[NSMutableArray arrayWithCapacity:20] retain];
      [NSTimer scheduledTimerWithTimeInterval: 1  // must be > 0
                                       target: self
                                     selector: @selector(venturaLaunchKludge:)
                                     userInfo: nil
                                      repeats: NO];
    }
    if (! [all_saver_views containsObject:self])
      [all_saver_views addObject:self];
  }
#endif  // VENTURA_KLUDGE

# ifdef SONOMA_KLUDGE   // Duplicated in XScreenSaverView.m

  /* Oct 2023, macOS 14.0: we get startAnimation on each screen, but
     stopAnimation is never called, and our process (legacyScreenSaver)
     never exits.  This means that the screen saver just keeps running
     forever in the background on an invisible window, burning CPU!

     That invisible window is both 'visible' and 'onActiveSpace', and has
     no parentWindow, so its invisibility is not detectable.

     However, there is a "com.apple.screensaver.willstop" notification and
     from that we can intuit that we should send ourselves stopAnimation.

     Except, stopAnimation() isn't great for a couple of reasons:

     First: after a few days, the legacyScreenSaver module might have all
     250+ screen saver bundles loaded.  Aside from their memory usage and
     static code, there is also an open file handle for each bundle, and
     each resource in each bundle.

     Second: somehow, legacyScreenSaver is managing to send animateOneFrame
     messages to savers that have already been stopped upon previous runs.
     Maybe it is holding on to old, expired copies of Randomizer and
     re-running those?  So, screw it, let's just exit() instead.

   */
  if (!p) {
    [[NSDistributedNotificationCenter defaultCenter]
        addObserverForName: @"com.apple.screensaver.willstop"
                    object: nil
                     queue: nil
                usingBlock:^(NSNotification *n) {
        if ([self isAnimating]) {
          NSLog (@"%@: received %@", [self blurb], [n name]);
          [self stopAnimation];
        } else {
          NSLog (@"%@: received %@ but already stopped",
                 [self blurb], [n name]);
        }
        NSLog (@"%@: exiting", [self blurb]);
        [[NSApplication sharedApplication] terminate:self];
      }];

    // Do it before sleeping as well.  This is redundant, I think.
    // On macOS 14.0 we *do* receive stopAnimation before suspend, but even
    // if we didn't, it would just mean that savers would keep running for
    // a fraction of a second between un-suspend and un-blank.
    //
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserverForName: NSWorkspaceWillSleepNotification
                    object: nil
                     queue: nil
                usingBlock:^(NSNotification *n) {
        if ([self isAnimating]) {
          NSLog (@"%@: received %@", [self blurb], [n name]);
          [self stopAnimation];
        } else {
          NSLog (@"%@: received %@ but already stopped",
                 [self blurb], [n name]);
        }
      }];
  }
# endif // SONOMA_KLUDGE

  return self;
}


// Finds the full pathname of every available .saver bundle.
//
- (void)getSaverPaths
{
  NSMutableArray *dirs = [NSMutableArray arrayWithCapacity: 10];
  // macOS 10.15+: Use the real home directory, not the container's.
  //[dirs addObject:[@"~/Library/Screen Savers" stringByExpandingTildeInPath]];
  [dirs addObject:
          [NSString stringWithFormat:@"/Users/%@/Library/Screen Savers",
                    NSUserName()]];
  [dirs addObject: @"/Library/Screen Savers"];
  [dirs addObject: @"/System/Library/Screen Savers"];

  NSMutableArray *paths_all     = [NSMutableArray arrayWithCapacity: 250];
  NSMutableArray *paths_enabled = [NSMutableArray arrayWithCapacity: 250];
  NSMutableDictionary *seen = [[NSMutableDictionary alloc] init];

  // Omit these savers entirely.
  [seen setObject:@"blacklist" forKey:@"random"];
  [seen setObject:@"blacklist" forKey:@"randomextra"];
  [seen setObject:@"blacklist" forKey:@"randomextra2"];
  [seen setObject:@"blacklist" forKey:@"randomextra3"];
  [seen setObject:@"blacklist" forKey:@"randomxscreensaver"];

  for (NSString *dir in dirs) {
    NSArray *files = [[NSFileManager defaultManager]
                       contentsOfDirectoryAtPath:dir error:nil];
    if (!files) continue;
    int count = 0;
    for (NSString *p in files) {
      if ([[p pathExtension] caseInsensitiveCompare: @"saver"]) 
        continue;
      NSString *name = [p stringByDeletingPathExtension];
      p = [dir stringByAppendingPathComponent: p];

      // If there is a saver of the same name in multiple directories,
      // only take the first one seen.  $HOME overrides System.
      NSString *o = [seen objectForKey:[name lowercaseString]];
      if (o) {
        // NSLog(@"%@: omitting %@ for %@", [self blurb], p, o);
        continue;
      }

      [seen setObject:p forKey: [name lowercaseString]];

      NSString *res = resource_key_for_name (name, FALSE, NULL);
      BOOL disabled = get_boolean (res, prefs);
      if (disabled) {
        // NSLog(@"%@: disabled: %@", [self blurb], name);
      } else {
        [paths_enabled addObject: p];
      }
      [paths_all addObject: p];
      count++;
    }

    if (count)
      NSLog(@"%@: found %d in %@", [self blurb], count, dir);
  }

  [seen release];

  if (bundle_paths_all)     [bundle_paths_all release];
  if (bundle_paths_enabled) [bundle_paths_enabled release];
  bundle_paths_all     = [[NSArray arrayWithArray: paths_all] retain];
  bundle_paths_enabled = [[NSArray arrayWithArray: paths_enabled] retain];
}


- (BOOL) screenEnabled:(NSScreen *)s verbose:(BOOL)verbose
{
  NSArray  *aa = screen_id (s);
  NSString *id = [aa objectAtIndex:0];
  NSString *key = [NSString stringWithFormat:@"screen_%@_disabled", id];
  BOOL enabled_p = !get_boolean (key, prefs);
  if (!enabled_p && verbose) {
    NSString *desc = [aa objectAtIndex:1];
    NSLog (@"%@: savers disabled on %@", [self blurb], desc);
  }
  return enabled_p;
}


- (NSString *) blurb { return [self blurb: self.window]; }

- (NSString *) blurb: (NSWindow *)w
{
  NSString *s = @"Randomizer";
  if (@available (macOS 10.15, *)) {
    NSString *n = (w && w.screen ? [w.screen localizedName] : nil);
    if (n && n.length)
      s = [NSString stringWithFormat: @"%@: %@", s, n];
  }
  return s;
}


- (ScreenSaverView *) loadSaverWithFrame:(NSRect)frame
{
  Class sclass = 0;
  ScreenSaverView *saver2 = 0;
  NSString *path = 0;

  if (![self isPreview] &&
      ![self screenEnabled:self.window.screen verbose:YES])
    return nil;

  if ([self isPreview]) {
    NSLog(@"%@: loading built-in saver", [self blurb]);
    sclass = [RandomizerAttractMode class];
  } else {
    if (! [bundle_paths_enabled count]) {
      // NSLog(@"@: no savers available", [self blurb]);
      [[NSException exceptionWithName: NSInternalInconsistencyException
                               reason: @"no savers available"
                             userInfo: nil]
        raise];
    } else {
      path = [bundle_paths_enabled objectAtIndex:
               (random() % [bundle_paths_enabled count])];
      NSLog(@"%@: loading saver %@", [self blurb], path);
      NSBundle *bundle = [NSBundle bundleWithPath:path];
      if (bundle)
        sclass = [bundle principalClass];
    }
  }

  if (! sclass) {
    //NSLog(@"%@: no class in bundle: %@", [self blurb], path);
    [[NSException exceptionWithName: NSInternalInconsistencyException
                             reason: [NSString stringWithFormat: 
                                           @"no class in bundle: %@", path]
                           userInfo: nil]
      raise];
    return nil;
  } else {
    saver2 = [sclass alloc];
    
    frame.origin.x = frame.origin.y = 0;  // frame -> bounds

    if ([saver2 respondsToSelector:
                  @selector(initWithFrame:isPreview:saverBundlePaths:)]) {
      saver2 = [(RandomizerAttractMode *) saver2
                   initWithFrame: frame
                       isPreview: [self isPreview]
                saverBundlePaths: bundle_paths_all];
# ifdef VENTURA_KLUDGE
    } else if ([saver2 respondsToSelector:
                         @selector(initWithFrame:isPreview:randomizer:)]) {
      // Inform XScreenSaverView that it has been invoked by Randomizer,
      // so that it does not try to process VENTURA_KLUDGE a second time.
      saver2 = [(XScreenSaverView *) saver2
                   initWithFrame: frame
                       isPreview: [self isPreview]
                      randomizer: TRUE];
# endif // VENTURA_KLUDGE
    } else if ([saver2 respondsToSelector:
                         @selector(initWithFrame:isPreview:)]) {
      saver2 = [(ScreenSaverView *) saver2
                   initWithFrame: frame
                       isPreview: [self isPreview]];
    } else {
      saver2 = 0;
    }
  }

  if (! saver2) {
    //NSLog(@"%@: unable to instantiate: %@", [self blurb], path);
    [[NSException exceptionWithName: NSInternalInconsistencyException
                             reason: [NSString stringWithFormat: 
                                           @"unable to instantiate: %@", path]
                           userInfo: nil]
      raise];
    return nil;
  }

  saver2.wantsLayer = YES;
  [saver2 retain];
  [saver2 setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];

# ifdef CATCH_SIGNALS
  // XScreenSaverView catches signals in initWithFrame (to log a backtrace)
  // so we have to catch our signals after that.
  [self initSignalHandlers];
# endif

  return saver2;
}


- (void) handleException: (NSException *)e in:(NSObject *)o
{
  // This will catch calls to abort and exit (via jwxyz_abort) but not SEGV.
  NSLog (@"%@: Caught exception %@: %@",
         [self blurb],
         (o ? [o className] : @"<null>"),
         [e reason]);

  if (! crash_label.stringValue.length) {
    struct tm *tm = localtime (&launch_time);
    time_t dur = time ((time_t *) 0) - cycle_time;
    NSString *s = [NSString stringWithFormat:
                                  @"Launched at: %02d:%02d:%02d; "
                                   "Cycles: %d; "
                                   "Duration: %d:%02d:%02d",
                            tm->tm_hour, tm->tm_min, tm->tm_sec,
                            cycle_count,
                            (int) (dur / (60*60)),
                            (int) (dur / (60) % 60),
                            (int) (dur % 60)];
    [crash_label setStringValue:
                   (o
                    ? [NSString stringWithFormat:
                                @"Error in %@:\n"
                                "%@\n"
                                "%@\n\n"
                                "%@",
                                [o className],
                                s,
                                [e reason],
                                [[e callStackSymbols]
                                  componentsJoinedByString: @"\n"]]
                    : [NSString stringWithFormat: @"Error: %@\n%@",
                                [e reason], s])];
  } else {
    [crash_label setStringValue:
                   [NSString stringWithFormat: @"Double Exception: %@\n\n%@",
                             [e reason], crash_label.stringValue]];
  }

  [crash_label removeFromSuperview];
  [self addSubview: crash_label];
}


#ifdef CATCH_SIGNALS

static int saved_argc = 0;
static char **saved_argv, *saved_execpath;

static void
sighandler (int sig)
{
  signal (sig, SIG_DFL);

  char s[100];	// no fprintf in signal handler
  strcpy (s, "Randomizer: signal ");
  strcat (s, sys_signame[sig]);
  strcat (s, "\n");
  write (fileno (stderr), s, strlen(s));

# if 0
  // Throwing an exception from a signal handler is a terrible idea, but my
  // thinking was, "it might work, and what's the worst that can happen?
  // We crash anyway?"  Well, it turns out, the exception is caught, the
  // error text shows up on the screen, and after that we're hung and you
  // can't unlock the screen.  So... that's worse.
  //
  // macOS 14.0 update: throwing the exception no longer hangs, but nor
  // does it make the error message show up.
  //
  [[NSException exceptionWithName: NSInternalInconsistencyException
                           reason: [NSString stringWithFormat: @"Signal %s",
                                             sys_signame[sig]]
                         userInfo: nil]
    raise];
# elif 0
  // Ok, let's try re-invoking the current process on signal, and just
  // restarting legacyScreenSaver or whatever from scratch.  Maybe that
  // will work?  Nope.  The re-exec works under SaverTester, but when
  // legacyScreenSaver is re-exec'd with the same args, the savers do
  // not re-launch.
  //
  if (saved_argc) {
    NSLog (@"randomizer: signal %s: re-executing %s",
           sys_signame[sig], saved_execpath);
    execvp (saved_execpath, saved_argv);
    // Somehow after the exec, SIGTERM stops working. So that's great.
#   undef exit
    exit (1);
  }
# elif 0
  // Nope, this doesn't work either.
  system ("open -a ScreenSaverEngine");
# else
  // Even this doesn't work.
  NSLog (@"%s", s);
# endif
}

static void
catch_signal (int sig, void (*handler) (int))
{
  struct sigaction a;
  a.sa_handler = handler;
  sigemptyset (&a.sa_mask);
  a.sa_flags = SA_NODEFER;

# if 0
  // This should allow us to handle a signal on the main stack instead of
  // the signal stack, but it doesn't work.
  a.sa_handler = SIG_IGN;
  dispatch_source_t src = 
    dispatch_source_create (DISPATCH_SOURCE_TYPE_SIGNAL, sig, 0,
                            dispatch_get_global_queue (0, 0));
  dispatch_source_set_event_handler (src, ^{ (*handler) (sig); });
  dispatch_resume (src);
# endif

  if (sigaction (sig, &a, 0) < 0)
    NSLog (@"randomizer: couldn't catch signal %d", sig);
}


- (void) initSignalHandlers
{
  if (! saved_argc) {
    // Get the current process's argv from the kernel.

    int mib[3] = { CTL_KERN, KERN_PROCARGS2, getpid() };
    char *buf, *s;
    int i;
    size_t L;

    if (sysctl (mib, 3, NULL, &L, NULL, 0) < 0) goto ERR;  // get buffer size
    buf = malloc (L);
    if (sysctl (mib, 3, buf, &L, NULL, 0)  < 0) goto ERR;  // get buffer data

    saved_argc = ((int *) buf)[0];
    if (saved_argc <= 0 || saved_argc > 100) goto ERR;
    saved_argv = (char **) calloc (saved_argc + 2, sizeof (*saved_argv));

    s = buf + sizeof(int);
    saved_execpath = strdup (s);
    while (*s) s++;
    while (!*s) s++;		// execpath is followed by one or more nulls

    for (i = 0; i < saved_argc; i++) {	// then we have null-separated argv
      saved_argv[i] = strdup (s);
      while (*s) s++; s++;
    }
    saved_argv[i] = 0;
  }

//catch_signal (SIGINT,  sighandler);  // shell ^C
//catch_signal (SIGQUIT, sighandler);  // shell ^|
  catch_signal (SIGILL,  sighandler);
  catch_signal (SIGTRAP, sighandler);
  catch_signal (SIGABRT, sighandler);
  catch_signal (SIGEMT,  sighandler);
  catch_signal (SIGFPE,  sighandler);
  catch_signal (SIGBUS,  sighandler);
  catch_signal (SIGSEGV, sighandler);
  catch_signal (SIGSYS,  sighandler);
//catch_signal (SIGTERM, sighandler);  // kill default
//catch_signal (SIGKILL, sighandler);  // -9 untrappable
  catch_signal (SIGXCPU, sighandler);
  catch_signal (SIGXFSZ, sighandler);
  NSLog (@"%@: installed signal handlers for %s", [self blurb], saved_execpath);
  return;

 ERR:
  saved_argc = 0;
  NSLog (@"%@: error getting argv", [self blurb]);
}

#endif // CATCH_SIGNALS


- (void)unloadSaver:(BOOL)firstp
{
  [self unloadSaver:firstp exception:NULL];
}

- (void)unloadSaver:(BOOL)firstp exception:(NSException *)e
{
  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;
  ScreenSaverView *ss = (firstp ? saver1 : saver2);
  if (!ss) return;
  NSLog(@"%@: unloading saver %@", [self blurb], [ss className]);

  @try {
    if ([ss isAnimating]) {
      if (e &&
          [ss respondsToSelector: @selector(stopAnimationWithException:)]) {
        // Emergency stop: don't do any more graphics when stopping; leak.
        [(XScreenSaverView *) ss stopAnimationWithException: e];
      } else {
        [ss stopAnimation];
      }
    }
  }
  @catch (NSException *e) {
    [self handleException:e in:ss];
  }

  [ss removeFromSuperview];
  [ss release];
  if (firstp)
    saver1 = 0;
  else
    saver2 = 0;

  // Is there some way to unload the bundle? Or would its static data screw
  // things up if the bundle were to be re-loaded later?
}


// Loads a new saver, starts it running while invisible, launches a
// background animation to cross-fade the old and new savers, then
// unloads the old one and calls launchCycleTimer.
//
- (void)fadeSaverOut
{
  NSAssert1 (!saver2, @"should not have saver2: %@", saver2);
  if (saver2) abort();

  switch (crossfade_mode) {
  case JUMPCUT:
    if (saver1) first_time_p = FALSE;
    [self unloadSaver: TRUE];

    saver1 = nil;
    @try { saver1 = [self loadSaverWithFrame:[self frame]]; }
    @catch (NSException *e) { [self handleException:e in:nil]; }

    if (saver1) {
      @try {
        [self addSubview: saver1];    // Sometimes gets "invalid GL context"
        saver1.alphaValue = 1;
        [self.window makeFirstResponder: saver1];
      }
      @catch (NSException *e) {
        [self handleException:e in:saver1];
        [self unloadSaver:TRUE  exception:e];
        [self unloadSaver:FALSE exception:e];
      }
    }

    [self launchCycleTimer];

    break;

  case FADE:    // Launch a background animation, then calls fadeSaverIn.
    if (!saver1) {
      [self fadeSaverIn];
      break;
    }

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = fade_duration;
        saver1.animator.alphaValue = 0;
      }
    completionHandler:^{
        [self fadeSaverIn];
      }];
    break;

  case CROSSFADE:

    // Load a new saver, starts it running while invisible, launches a
    // background animation, then calls launchCycleTimer.

    saver2 = nil;
    @try { saver2 = [self loadSaverWithFrame:[self frame]]; }
    @catch (NSException *e) { [self handleException:e in:nil]; }

    if (!saver2) {
      [self launchCycleTimer];
      return;
    }

    if (saver1)
      NSLog(@"%@: crossfade %@ to %@", [self blurb],
            [saver1 className], [saver2 className]);
    else
      NSLog(@"%@: fade in %@", [self blurb], [saver2 className]);

    @try {
      [self addSubview: saver2];    // Sometimes gets "invalid GL context"
    }
    @catch (NSException *e) {
      [self handleException:e in:saver2];
      [self unloadSaver:TRUE  exception:e];
      [self unloadSaver:FALSE exception:e];
      [self launchCycleTimer];
      return;
    }
    
    saver2.alphaValue = 0;

    @try {
      [saver2 startAnimation];
    }
    @catch (NSException *e) {
      [self handleException:e in:saver2];
      [self unloadSaver:TRUE  exception:e];
      [self unloadSaver:FALSE exception:e];
      [self launchCycleTimer];
      return;
    }

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = fade_duration * (saver1 ? 2 : 1);
        if (saver1)
          saver1.animator.alphaValue = 0;
        if (saver2)
          saver2.animator.alphaValue = 1;
      }
    completionHandler:^{
        [self unloadSaver: TRUE];
        saver1 = saver2;
        saver2 = 0;
        if (saver1)
          [self.window makeFirstResponder: saver1];
        [self launchCycleTimer];
        first_time_p = FALSE;
      }];
    break;

  default: abort(); break;
  }
}


- (void)fadeSaverIn
{
  if (crossfade_mode != FADE) abort();

  [self unloadSaver: TRUE];

  saver1 = nil;
  @try { saver1 = [self loadSaverWithFrame:[self frame]]; }
  @catch (NSException *e) { [self handleException:e in:nil]; }

  if (!saver1) {
    [self launchCycleTimer];
    return;
  }

  first_time_p = FALSE;

  [self addSubview: saver1];
  [self.window makeFirstResponder: saver1];
  saver1.alphaValue = 0;

  @try { [saver1 startAnimation]; }
  @catch (NSException *e) {
    [self handleException:e in:saver1];
    [self unloadSaver:TRUE exception:e];
    return;
  }

  [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
      context.duration = fade_duration;
      saver1.animator.alphaValue = 1;
    }
  completionHandler:^{
      [self launchCycleTimer];
    }];
}


- (void)launchCycleTimer
{
  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;

  if (! [self isAnimating]) {
    NSLog (@"%@: not animating, not launching cycle timer", [self blurb]);
    return;
  }

  // Parse the cycle time out of whatever garbage is in preferences.
  //
  NSObject *o = [[prefs defaults] objectForKey: @CYCLE_TIME];
  NSTimeInterval interval = 0;
  if (o && [o isKindOfClass:[NSNumber class]]) {
    interval = [(NSNumber *) o doubleValue];
  } else if (o && [o isKindOfClass:[NSString class]]) {
    interval = [((NSString *) o) doubleValue];
  }

  if (interval <= 0)
    interval = CYCLE_DEFAULT;

  // If there are multiple screens, offset the restart time of subsequent
  // screens: they will all change every N minutes, but not at the same time.
  // But don't let that offset be more than about 5 minutes.
  //
  if (first_time_p && ![self isPreview]) {
    NSArray *dscreens = [NSScreen screens];
    int nscreens = [dscreens count];
    int enabled_screens = 0;
    int which_screen = 0;
    int which_enabled = 0;
    for (int i = 0, j = 0; i < nscreens; i++) {
      NSScreen *s = [dscreens objectAtIndex:i];
      if (s == self.window.screen) which_screen = i;
      if ([self screenEnabled:s verbose:NO]) {
        enabled_screens++;
        if (s == self.window.screen) which_enabled = j;
        j++;
      }
    }

    double t2 = interval;
    double max = 60*60*10;
    if (t2 > max) t2 = max;
    double off = (enabled_screens ? t2 * which_enabled / enabled_screens : 0);
    interval += off;
    if (off)
      NSLog (@"%@: screen %d cycle offset: %d sec",
             [self blurb], which_screen, (int) off);
  }

  if (! [self isPreview]) {
    NSLog (@"%@: cycle_timer %.1f", [self blurb], interval);
    cycle_timer =
      [NSTimer scheduledTimerWithTimeInterval: interval
                                       target: self
                                     selector: @selector(cycleSaver)
                                     userInfo: nil
                                      repeats: NO];
  }
}


- (void)cycleSaver
{
  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;

  [crash_label removeFromSuperview];
  [crash_label setStringValue:@""];

  if (! [self isAnimating]) {
    NSLog (@"%@: not animating, not cycling", [self blurb]);
    return;
  }

  cycle_time = time ((time_t *) 0);
  cycle_count++;

  NSLog(@"%@: cycle timer", [self blurb]);
  [self fadeSaverOut];
}


- (NSTimeInterval)animationTimeInterval
{
  NSTimeInterval i = 1/30.0;
  if      (saver1) i = [saver1 animationTimeInterval];
  else if (saver2) i = [saver2 animationTimeInterval];
  return i;
}


- (void)setAnimationTimeInterval:(NSTimeInterval)i
{
  if (saver1) [saver1 setAnimationTimeInterval:i];
  if (saver2) [saver2 setAnimationTimeInterval:i];
  [super setAnimationTimeInterval:i];
}


- (void)startAnimation
{
  if ([self isAnimating]) {
    NSLog (@"%@: startAnimation called while animating", [self blurb]);
    return;
    // [self stopAnimation];
  } else {
    NSLog (@"%@: startAnimation", [self blurb]);
  }

  animating_p = TRUE;

  // Do the initial load of the saver here rather than in initWithFrame
  // so that we have self.window and check whether this screen is disabled.
  //
  if (!saver1 && !saver2)
    [self cycleSaver];

  if (saver1) {
    @try { [saver1 startAnimation]; }
    @catch (NSException *e) {
      [self handleException:e in:saver1];
      [self unloadSaver:TRUE exception:e];
    }
  }

  if (saver2) {
    @try { [saver2 startAnimation]; }
    @catch (NSException *e) {
      [self handleException:e in:saver2];
      [self unloadSaver:FALSE exception:e];
    }
  }

  [super startAnimation];
}


- (void)stopAnimation
{
  if ([self isAnimating])
    NSLog (@"%@: stopAnimation", [self blurb]);
  else
    NSLog (@"%@: stopAnimation but already stopped", [self blurb]);

  animating_p = FALSE;

  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;

  if (saver1) {
    @try { [saver1 stopAnimation]; }
    @catch (NSException *e) {
      [self handleException:e in:saver1];
      [self unloadSaver:TRUE exception:e];
    }
  }

  if (saver2) {
    @try { [saver2 stopAnimation]; }
    @catch (NSException *e) {
      [self handleException:e in:saver2];
      [self unloadSaver:FALSE exception:e];
    }
  }

  [super stopAnimation];
}


// Originally this just returned whether either saver1 or saver2 was
// animating, but since, as of macOS 14.0, legacyScreenSaver never exits,
// we need to track whether we *should* be animating instead, or there
// can be a race with crossfades that results in savers running when they
// should be dead.
//
- (BOOL) isAnimating { return animating_p; }
- (BOOL) isPreview   { return preview_p;   }


- (void)animateOneFrame
{
#if 0
  if (! (random() % 2000)) {
    NSLog(@"%@: BOOM ####", [self blurb]);
    // int x = 123; char segv = * ((char *)x);
    #undef abort
    abort();
  }
#endif

  if (saver1) {
    @try { [saver1 animateOneFrame]; }
    @catch (NSException *e) {
      [self handleException:e in:saver1];
      [self unloadSaver:TRUE exception:e];
    }
  }
  if (saver2) {
    @try { [saver2 animateOneFrame]; }
    @catch (NSException *e) {
      [self handleException:e in:saver2];
      [self unloadSaver:FALSE exception:e];
    }
  }
}


/* On 10.15, if "use random screen saver" is checked, then startAnimation
   is never called.  This may be related to Apple's buggy code in 
   ScreenSaverEngine calling nonexistent beginExtensionRequestWithUserInfo,
   but on 10.15 we're not even running in that process: now we're in the
   not-at-all-ominously-named legacyScreenSaver process.

   Dec 2020, noticed that this also happens on 10.14.6 when *not* in random
   mode.  Both System Preferences and ScreenSaverEngine fail to call
   StartAnimation.

   June 2023, macOS 13.4: On a system with 3 screens, initWithFrame is called
   on every screen, but viewDidMoveToWindow is called only on screen 3 -- but
   that screen's view has the frame of screen 1!  So we get only one saver
   running, and it is the wrong size.  We detect and correct this insanity
   with the VENTURA_KLUDGE stuff.
 */
- (void) viewDidMoveToWindow
{
  NSLog (@"%@: viewDidMoveToWindow", [self blurb]);
  if (self.window &&
      self.window.frame.size.width  > 0 &&
      self.window.frame.size.height > 0)
    [self startAnimation];
}

- (void) viewWillMoveToWindow:(NSWindow *)window
{
  NSLog (@"%@: viewWillMoveToWindow", [self blurb: window]);
  if (window == nil)
    [self stopAnimation];
}


#ifdef VENTURA_KLUDGE
/* Correct the insane shit that LegacyScreenSaver is throwing at us now.
   We keep track of each ScreenSaverView that was created; and then a little
   while after startup, we check to see which of those views have not been
   attached to windows, or have the wrong geometry.
   Duplicated in XScreenSaverView.m.
 */
- (void) venturaLaunchKludge: (NSTimer *) timer
{
  NSArray<NSWindow *> *windows = [NSApplication sharedApplication].windows;

  const char *tag = "Ventura kludge";

  // First log what was wrong.
  //
  int i = 0;
  for (NSWindow *w in windows) {
    NSView *v = NULL;
    i++;

    // Find the XScreenSaverView on this window.
    for (NSView *v1 in all_saver_views) {
      if (w.contentView == v1.superview) {
        v = v1;
        break;
      }
    }

    if (!v) {
      NSLog (@"%@: %s: screen %d %gx%g+%g+%g had no saver view",
             [self blurb], tag, i,
             w.frame.size.width, w.frame.size.height,
             w.frame.origin.x,   w.frame.origin.y);
    } else {
      NSRect target = w.frame;
      target.origin.x = 0;
      target.origin.y = 0;
      if (v.frame.size.width  == target.size.width  &&
          v.frame.size.height == target.size.height &&
          v.frame.origin.x    == target.origin.x    &&
          v.frame.origin.y    == target.origin.y) {
        NSLog (@"%@: %s: screen %d %gx%g+%g+%g had correct view frame"
               " %gx%g+%g+%g",
               [self blurb], tag, i,
               w.frame.size.width, w.frame.size.height,
               w.frame.origin.x,   w.frame.origin.y,
               target.size.width,  target.size.height,
               target.origin.x,    target.origin.y);
      } else {
        NSLog (@"%@: %s: screen %d %gx%g+%g+%g had view frame"
               " %gx%g+%g+%g instead of %gx%g+%g+%g",
               [self blurb], tag, i,
               w.frame.size.width, w.frame.size.height,
               w.frame.origin.x,   w.frame.origin.y,
               v.frame.size.width, v.frame.size.height,
               v.frame.origin.x,   v.frame.origin.y,
               target.size.width,  target.size.height,
               target.origin.x,    target.origin.y);
      }
    }
  }

  // Now repair it.
  //
  i = 0;
  for (NSWindow *w in windows) {
    NSView *v = NULL;
    i++;

    // Find the XScreenSaverView on this window.
    for (NSView *v1 in all_saver_views) {
      if (w.contentView == v1.superview) {
        v = v1;
        break;
      }
    }

    BOOL attached_p = FALSE;
    if (!v) {
      // This window has no ScreenSaverView.  Pick any unattached one.
      for (NSView *v1 in all_saver_views) {
        if (!v1.window) {
          v = v1;
          NSLog (@"%@: %s: screen %d %gx%g+%g+%g: attaching saver view",
                 [self blurb], tag, i,
                 w.frame.size.width, w.frame.size.height,
                 w.frame.origin.x,   w.frame.origin.y);
          attached_p = TRUE;
          [w.contentView addSubview: v];
          break;
        }
      }
    }

    if (v) {
      // A view is attached to this window, but the frame might have the
      // wrong size or origin.
      NSRect target = w.frame;
      target.origin.x = 0;
      target.origin.y = 0;
      if (v.frame.size.width  != target.size.width  ||
          v.frame.size.height != target.size.height ||
          v.frame.origin.x    != target.origin.x    ||
          v.frame.origin.y    != target.origin.y) {
        if (!attached_p)
          NSLog (@"%@: %s: screen %d %gx%g+%g+%g: correcting frame: "
                 "%gx%g+%g+%g => %gx%g+%g+%g",
                 [self blurb], tag, i,
                 w.frame.size.width, w.frame.size.height,
                 w.frame.origin.x,   w.frame.origin.y,
                 v.frame.size.width, v.frame.size.height,
                 v.frame.origin.x,   v.frame.origin.y,
                 target.size.width,  target.size.height,
                 target.origin.x,    target.origin.y);
        [v setFrame: target];
      }
    }
  }
}
#endif // VENTURA_KLUDGE


- (BOOL)hasConfigureSheet
{
  return TRUE;
}


- (NSWindow*)configureSheet
{
  NSMutableArray *names = [bundle_paths_all mutableCopy];
  for (int i = 0; i < [names count]; i++) {
    [names replaceObjectAtIndex: i
                     withObject: [[[names objectAtIndex: i] lastPathComponent]
                                   stringByDeletingPathExtension]];
  }
  [names sortUsingSelector: @selector(localizedCaseInsensitiveCompare:)];
  return [[RandomizerConfig alloc]
           initWithSaverNames: [NSArray arrayWithArray: names]
                        prefs: prefs];
}


- (void) dealloc
{
  NSLog (@"%@: dealloc", [self blurb]);
  [self unloadSaver: TRUE];
  [self unloadSaver: FALSE];
  [crash_label release];
  [super dealloc];
}

@end


@implementation ExponentialDelayValueTransformer

// The cycleTime slider has tick marks [1, 16] which transforms to
// an exponential number of minutes, expressed in seconds.
//
#define SLIDER_EXPONENT 2.0

+ (Class)transformedValueClass { return [NSNumber class]; }
+ (BOOL)allowsReverseTransformation { return YES; }

- (id)transformedValue:(id)value {
  double v = [value doubleValue];
  v = (int) (0.5 + pow (v, 1 / SLIDER_EXPONENT));
  v /= 60;
  return [NSNumber numberWithDouble: v];
}

- (id)reverseTransformedValue:(id)value {
  double v = [value doubleValue];
  v = (int) (0.5 + pow (v, SLIDER_EXPONENT));
  v *= 60;
  return [NSNumber numberWithDouble: v];
}
@end


/*************************************************************************

 The preferences panel for the randomizer.

 *************************************************************************/


@implementation RandomizerConfig
{
  NSTableView *screen_table, *saver_table;
  NSTextField *timerLabel;
  NSArray *savers;
  NSArray *screen_names;     // Readable, sortable strings printed in the list
  NSDictionary *screen_ids;  // screen_names -> resource id
  NSUserDefaultsController *prefs;
}


- (id)initWithSaverNames: (NSArray *)sn
                   prefs: (NSUserDefaultsController *) _prefs;
{
  self = [super init];
  if (! self) return 0;

  savers = [sn retain];
  prefs = _prefs;

  NSRect frame;
  frame.origin.x = 0;
  frame.origin.y = 0;
  frame.size.width  = 460;
  frame.size.height = 520;
  [self setFrame:frame display:NO];
  self.minSize = frame.size;
  frame.size.height = 99999;
  self.maxSize = frame.size;
  self.styleMask |= NSWindowStyleMaskResizable;

  // In a sane world, almost all of this would be in a .xib file.  But after
  // I spent 4 hours unsuccessfully trying to figure out how to ctrl-drag
  // the little blue line to make an IBOutlet connection show up in my .h
  // file, I realized it would be faster to just write all the code by hand.

  NSPanel *panel = self;
  int margin = 18;

  // Top label

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  frame.size.width = panel.frame.size.width - margin*2;
  NSTextField *lab = [[NSTextField alloc] initWithFrame:frame];
  [lab setFont:[NSFont systemFontOfSize: [NSFont systemFontSize]]];
  lab.cell.wraps = YES;
  [lab setStringValue:NSLocalizedString(
    @"This screen saver runs a different random screen saver on "
    "each monitor, and selects a new one after a while.",
    @"")];
  lab.selectable = NO;
  lab.editable = NO;
  lab.bezeled = NO;
  lab.drawsBackground = NO;
  [lab sizeToFit];
  lab.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin;
  frame = lab.frame;
  frame.size.width = panel.frame.size.width - margin*2;
  frame.size.height *= 2;  // WTF, wrapping!
  frame.origin.x = margin;
  frame.origin.y = panel.frame.size.height - frame.size.height - margin*2;
  lab.frame = frame;
  [panel.contentView addSubview: lab];

  // Cycle label

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSTextField *clab = [[NSTextField alloc] initWithFrame:frame];
  [clab setFont:[NSFont systemFontOfSize: [NSFont systemFontSize]]];
  [clab setStringValue:NSLocalizedString(@"Change saver every", @"")];
  clab.selectable = NO;
  clab.editable = NO;
  clab.bezeled = NO;
  clab.drawsBackground = NO;
  [clab sizeToFit];
  clab.autoresizingMask = NSViewMinYMargin;
  frame = clab.frame;
  frame.origin.x = lab.frame.origin.x;
  frame.origin.y = lab.frame.origin.y - lab.frame.size.height;
  clab.frame = frame;
  [panel.contentView addSubview: clab];
  [clab release];

  double line_height = frame.size.height;


  // Cycle slider

  frame.origin.x = clab.frame.origin.x + clab.frame.size.width + margin;
  frame.origin.y = clab.frame.origin.y;
  frame.size.width = panel.frame.size.width - frame.origin.x - margin;
  frame.size.height = clab.frame.size.height;
  NSSlider *slider = [[NSSlider alloc] initWithFrame:frame];
  [slider setTarget: self];
  [slider setAction: @selector(cycleTimerSliderChanged:)];

  slider.minValue = 2;			// 4 minutes
  slider.maxValue = 22;			// 8 hours
  slider.numberOfTickMarks = 16;
  slider.allowsTickMarkValuesOnly = YES;

  slider.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
  [slider bind: @"value"
      toObject: prefs withKeyPath:@"values." CYCLE_TIME
       options: @{ NSValueTransformerNameBindingOption: 
                     @"ExponentialDelayValueTransformer" }];
  [panel.contentView addSubview: slider];
  [slider release];

  // The checkmarks push the slider up a few pixels.
  // Align top of the label to the top of the slider.
  frame = clab.frame;
  frame.origin.y += 6;
  clab.frame = frame;


  // Low label

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSTextField *llab = [[NSTextField alloc] initWithFrame:frame];
  llab.font = [NSFont systemFontOfSize: [NSFont smallSystemFontSize]];
  llab.stringValue =
    [self formatTime: 60 * (0.5 + pow (slider.minValue, SLIDER_EXPONENT))];
  llab.selectable = NO;
  llab.editable = NO;
  llab.bezeled = NO;
  llab.drawsBackground = NO;
  [llab sizeToFit];
  llab.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
  frame = llab.frame;
  frame.origin.x = slider.frame.origin.x;
  frame.origin.y = slider.frame.origin.y - slider.frame.size.height;
  llab.frame = frame;
  [panel.contentView addSubview: llab];
  [llab release];

  // High label

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSTextField *hlab = [[NSTextField alloc] initWithFrame:frame];
  hlab.font = [NSFont systemFontOfSize: [NSFont smallSystemFontSize]];
  hlab.stringValue =
    [self formatTime: 60 * (0.5 + pow (slider.maxValue, SLIDER_EXPONENT))];
  hlab.selectable = NO;
  hlab.editable = NO;
  hlab.bezeled = NO;
  hlab.drawsBackground = NO;
  [hlab sizeToFit];
  hlab.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
  frame = hlab.frame;
  frame.origin.x = slider.frame.origin.x + slider.frame.size.width -
    frame.size.width;
  frame.origin.y = llab.frame.origin.y;
  hlab.frame = frame;
  [panel.contentView addSubview: hlab];
  [hlab release];

  // Middle label

  frame = llab.frame;
  NSTextField *mlab = [[NSTextField alloc] initWithFrame:frame];
  mlab.font = [NSFont systemFontOfSize: [NSFont smallSystemFontSize]];
  mlab.stringValue = @"999 hours 99 minutes";
  mlab.selectable = NO;
  mlab.editable = NO;
  mlab.bezeled = NO;
  mlab.drawsBackground = NO;
  mlab.alignment = NSTextAlignmentCenter;
  [mlab sizeToFit];
  mlab.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
  frame = mlab.frame;
  frame.origin.x = slider.frame.origin.x + 
    (slider.frame.size.width - frame.size.width) / 2;
  frame.origin.y = llab.frame.origin.y;
  mlab.frame = frame;
  mlab.stringValue = @"";
  [panel.contentView addSubview: mlab];
  [mlab release];
  timerLabel = mlab;

  // Screen table view

  NSArray *dscreens = [NSScreen screens];

  NSMutableDictionary *screen_ords =
    [NSMutableDictionary dictionaryWithCapacity: [dscreens count]];

  {
    // Save the original number of the screens.
    for (int i = 0; i < [dscreens count]; i++) {
      NSScreen *sc = [dscreens objectAtIndex: i];
      NSDictionary *d = [sc deviceDescription];
      NSNumber *sid = [d objectForKey:@"NSScreenNumber"];
      [screen_ords setObject: [NSNumber numberWithInteger:i] forKey: sid];
    }

    // Sort the screens left to right, then top to bottom.
    dscreens = [dscreens sortedArrayUsingComparator:
                           ^ NSComparisonResult (id a, id b) {
        NSScreen *sa = a;
        NSScreen *sb = b;
        return (sa.frame.origin.x > sb.frame.origin.x ? NSOrderedDescending :
                sa.frame.origin.x < sb.frame.origin.x ? NSOrderedAscending  :
                sa.frame.origin.y > sb.frame.origin.y ? NSOrderedDescending :
                sa.frame.origin.y < sb.frame.origin.y ? NSOrderedAscending  :
                NSOrderedSame);
      }];

    NSMutableArray *sn = [NSMutableArray arrayWithCapacity: [dscreens count]];
    NSMutableDictionary *si = [NSMutableDictionary
                                dictionaryWithCapacity:[dscreens count]];
    for (int i = 0; i < [dscreens count]; i++) {
      NSScreen *sc = [dscreens objectAtIndex: i];
      NSDictionary *d = [sc deviceDescription];
      NSNumber *sid = [d objectForKey:@"NSScreenNumber"];
      int ord = [(NSNumber *) [screen_ords objectForKey: sid] integerValue];

      NSArray  *aa = screen_id (sc);
      NSString *id   = [aa objectAtIndex:0];
      NSString *desc = [aa objectAtIndex:1];
      NSString *name = [NSString stringWithFormat:
                                   @"Screen %d - %d x %d @ %d, %d (%@)",
                                 (ord + 1),
                                 (int) sc.frame.size.width,
                                 (int) sc.frame.size.height,
                                 (int) sc.frame.origin.x, 
                                 (int) sc.frame.origin.y,
                                 desc];
      [sn addObject: name];
      [si setObject:id forKey:name];
    }
    screen_names = [sn retain];
    screen_ids   = [si retain];
  }

  NSScrollView *sv = [[NSScrollView alloc] init];
  sv.hasVerticalScroller = YES;
  [sv setAutoresizingMask: NSViewMinXMargin | NSViewMaxXMargin |
      NSViewMinYMargin];
  [panel.contentView addSubview: sv];

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSTableView *stab = [[NSTableView alloc] initWithFrame:frame];
  NSTableColumn *col1 = [[NSTableColumn alloc] initWithIdentifier:@"check"];
  NSTableColumn *col2 = [[NSTableColumn alloc] initWithIdentifier:@"name"];
  col1.title = @"";
  col2.title = NSLocalizedString(@"Run savers on screens", @"");
  col1.width = line_height * 1.5;
  [stab addTableColumn: col1];
  [stab addTableColumn: col2];
  screen_table = stab;
  stab.delegate = self;
  stab.dataSource = self;
  [stab reloadData];
  sv.contentView.documentView = stab;

  NSSortDescriptor *sd1 =
    [NSSortDescriptor sortDescriptorWithKey: col1.identifier ascending: YES
                                   selector: @selector(compare:)];
  NSSortDescriptor *sd2 =
    [NSSortDescriptor sortDescriptorWithKey: col2.identifier ascending: YES
                                   selector: @selector(compare:)];
  [col1 setSortDescriptorPrototype: sd1];
  [col2 setSortDescriptorPrototype: sd2];
  [stab setSortDescriptors:@[sd2]];
  [col1 release];
  [col2 release];

  frame = sv.frame;
  frame.size.width = panel.frame.size.width - margin * 2;
  frame.size.height = stab.frame.size.height;
  frame.size.height += line_height * 2;  // tab headers, sigh
  int max = line_height * 10;  // 7 = 3 lines??
  if (frame.size.height > max) frame.size.height = max;
  frame.origin.x = margin;
  frame.origin.y = hlab.frame.origin.y - frame.size.height - margin;
  sv.frame = frame;

  if ([screen_names count] <= 1) {  // Hide the screens list if only one.
    [sv removeFromSuperview];
    frame.origin.y += frame.size.height + margin;
    frame.size.height = 0;
    sv.frame = frame;
  }


  // Select All button

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSButton *ball = [[NSButton alloc] initWithFrame:frame];
  ball.font = [NSFont systemFontOfSize: [NSFont systemFontSize]];
  ball.title = NSLocalizedString(@"Select all", @"");
  ball.bezelStyle = NSRoundedBezelStyle;
  [ball sizeToFit];
  ball.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMaxYMargin;
  frame = ball.frame;
  frame.origin.x = lab.frame.origin.x;
  frame.origin.y = margin;
  ball.frame = frame;
  [panel.contentView addSubview: ball];
  ball.action = @selector(selectAllAction:);


  // Unselect All button

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSButton *bnone = [[NSButton alloc] initWithFrame:frame];
  bnone.font = [NSFont systemFontOfSize: [NSFont systemFontSize]];
  bnone.title = NSLocalizedString(@"Unselect all", @"");
  bnone.bezelStyle = NSRoundedBezelStyle;
  [bnone sizeToFit];
  bnone.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMaxYMargin;
  frame = bnone.frame;
  frame.origin.x = ball.frame.origin.x + ball.frame.size.width + margin;
  frame.origin.y = ball.frame.origin.y;
  bnone.frame = frame;
  [panel.contentView addSubview: bnone];
  bnone.action = @selector(selectNoneAction:);


  // OK button

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSButton *bok = [[NSButton alloc] initWithFrame:frame];
  bok.font = [NSFont systemFontOfSize: [NSFont systemFontSize]];
  bok.title = NSLocalizedString(@"OK", @"");
  bok.bezelStyle = NSRoundedBezelStyle;
  [bok sizeToFit];
  bok.autoresizingMask =  NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMaxYMargin;
  frame = bok.frame;
  frame.origin.x = panel.frame.size.width - frame.size.width - margin;
  frame.origin.y = ball.frame.origin.y;
  bok.frame = frame;
  [panel.contentView addSubview: bok];
  [bok setKeyEquivalent:@"\r"];
  bok.target = self;
  bok.action = @selector(okAction:);


  // Saver table view

  NSScrollView *sv2 = [[NSScrollView alloc] init];
  sv2.hasVerticalScroller = YES;
  [sv2 setAutoresizingMask: NSViewMinXMargin | NSViewMaxXMargin |
       NSViewHeightSizable];
  [panel.contentView addSubview: sv2];

  frame.origin.x = frame.origin.y = frame.size.width = frame.size.height = 0;
  NSTableView *stab2 = [[NSTableView alloc] initWithFrame:frame];
  NSTableColumn *col3 = [[NSTableColumn alloc] initWithIdentifier:@"check"];
  NSTableColumn *col4 = [[NSTableColumn alloc] initWithIdentifier:@"name"];
  col3.title = @"";
  col4.title = NSLocalizedString(@"Enable these savers", @"");
  col3.width = line_height * 2;
  [stab2 addTableColumn: col3];
  [stab2 addTableColumn: col4];
  saver_table = stab2;
  stab2.delegate = self;
  stab2.dataSource = self;
  [stab2 reloadData];
  sv2.contentView.documentView = stab2;

  [col3 setSortDescriptorPrototype: sd1];
  [col4 setSortDescriptorPrototype: sd2];
  [stab2 setSortDescriptors:@[sd2]];
  [col3 release];
  [col4 release];

  frame = sv2.frame;
  frame.origin.x = margin;
  frame.origin.y = ball.frame.origin.y + ball.frame.size.height + margin;
  frame.size.width = panel.frame.size.width - margin * 2;
  frame.size.height = sv.frame.origin.y - frame.origin.y - margin;
  sv2.frame = frame;

  [lab release];
  [sv  release];
  [sv2 release];

  [self cycleTimerSliderChanged: slider];

  return self;
}


// Convert number of seconds to an approximate string; if it's more than
// 2 hours, round to 15 minute increments.
//
- (NSString *) formatTime: (double) secs
{
  int min = secs / 60;
  int hr = min / 60;
  int min2 = (min % 60) / 15 * 15;
  // int si = secs; NSLog(@"%d:%02d:%02d", si/60/60, (si/60)%60, si%60);
  return
    (min == 1 ? @"1 minute" :
     min < 60 ? [NSString stringWithFormat:@"%d minutes", min] :
     hr == 1  ? [NSString stringWithFormat:@"%d hour %d minutes", hr, min%60] :
     min2 == 0? [NSString stringWithFormat:@"%d hours", hr] :
                [NSString stringWithFormat:@"%d hours %d minutes",hr, min2]);
}


- (void) cycleTimerSliderChanged:(NSObject *)arg
{
  [prefs save:self];
  double v = [[[prefs defaults] objectForKey: @CYCLE_TIME] doubleValue];
  timerLabel.stringValue = [self formatTime:v];
}


- (NSInteger)numberOfRowsInTableView:(NSTableView *) tv
{
  if (tv == screen_table) {
    return [screen_names count];
  } else if (tv == saver_table) {
    return [savers count];
  } else {
    return 0;
  }
}


- (NSCell *)     tableView: (NSTableView *) tv
    dataCellForTableColumn: (NSTableColumn *) tc
                       row: (NSInteger) y
{
  if (! tc) return nil;

  if ([[tc identifier] isEqualToString:@"check"]) {
    NSButtonCell *cell = [[NSButtonCell alloc] init];
    [cell setTitle: @""];
    [cell setAllowsMixedState: YES];
    [cell setButtonType: NSSwitchButton];
    //[cell release];  // Analyzer says this leaks?
    return cell;
  } else {
    NSCell *cell = [[NSCell alloc] initTextCell: @""];
    //[cell release];  // Analyzer says this leaks?
    return cell;
  }
}


// Returns the resource key we used to see if this item is disabled.
//
- (NSString *) resourceKeyForTableView: (NSTableView *) tv
                           tableColumn: (NSTableColumn *) tc
                                   row: (NSInteger) y
{
  BOOL screen_p = (tv == screen_table);
  NSString *s = (screen_p
                 ? [screen_names objectAtIndex: y]
                 : [savers objectAtIndex: y]);
  return resource_key_for_name (s, screen_p, screen_ids);
}


// Feeds labels and checkbox states into the tables.
//
- (id)              tableView: (NSTableView *) tv
    objectValueForTableColumn: (NSTableColumn *) tc
                          row: (NSInteger) y
{
  if (!tc) return nil;

  NSString *s;
  if (tv == screen_table) {
    s = [screen_names objectAtIndex: y];
  } else {  // if (tv == saver_table) {
    s = [savers objectAtIndex: y];
  }

  if ([[tc identifier] isEqualToString:@"check"]) {
    NSString *res = [self resourceKeyForTableView:tv tableColumn:tc row:y];
    BOOL v = get_boolean (res, prefs);
    BOOL checked = !v;  // Invert the sense of the checkbox for "disabled"
    return [NSNumber numberWithInteger: (checked ? NSOnState : NSOffState)];

  } else {
    return s;   // The text in the "name" column
  }
}


// Called when a checkbox is clicked in a table.
// Inverts the value of the corresponding resource.
// A redisplay will happen afterward that reads it again.
//
- (void) tableView: (NSTableView *) tv
    setObjectValue: (id) val
    forTableColumn: (NSTableColumn *) tc
               row:(NSInteger) y
{
  if ([[tc identifier] isEqualToString:@"check"]) {
    NSString *res = [self resourceKeyForTableView:tv tableColumn:tc row:y];
    BOOL checked = !!get_boolean (res, prefs);
    if (checked)  // checkmark means no "disabled" entry in prefs
      [[prefs defaults] removeObjectForKey: res];
    else
      [[prefs defaults] setObject:[NSNumber numberWithBool:TRUE] forKey:res];
    [prefs save:self];
  }
}


- (void)           tableView: (NSTableView *) tv
    sortDescriptorsDidChange: (NSArray *) od
{
  BOOL screen_p = (tv == screen_table);
  NSArray *aa = (screen_p ? screen_names : savers);

  // Only one column should be sorting at a time.  Surely this is the wrong
  // right way to accomplish this, as I don't know which of the two columns
  // in the array was the most-recently-clicked one.
  //
  if ([od count] > 1) {
    od = @[ [od objectAtIndex:0] ];
    [tv setSortDescriptors: od];
  }

  for (NSSortDescriptor *sd in od) {

    if ([[sd key] isEqualToString:@"name"]) {
      aa = [aa sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
          if ([sd ascending])
            return [a caseInsensitiveCompare: b];
          else
            return [b caseInsensitiveCompare: a];
        }];

    } else {  // sorting by checkbox

      NSMutableDictionary *cc = [[NSMutableDictionary alloc] init];
      for (NSString *s in aa) {
        NSString *res = resource_key_for_name (s, screen_p, screen_ids);
        BOOL checked = !get_boolean (res, prefs);
        [cc setObject:[NSNumber numberWithBool: checked] forKey:s];
      }

      aa = [aa sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
          int ba = [[cc objectForKey:a] intValue];
          int bb = [[cc objectForKey:b] intValue];
          if (ba == bb) return NSOrderedSame;
          if ([sd ascending])
            return (ba < bb ? NSOrderedAscending : NSOrderedDescending);
          else
            return (bb < ba ? NSOrderedAscending : NSOrderedDescending);
        }];
    }
  }

  [aa retain];
  if (tv == screen_table) {
    [screen_names release];
    screen_names = aa;
  } else if (tv == saver_table) {
    [savers release];
    savers = aa;
  }

  [tv reloadData];
}


- (void) selectAllAction:(NSObject *)arg selected:(BOOL)selected
{
  for (NSString *s in savers) {
    NSString *res = resource_key_for_name (s, NO, screen_ids);
    if (selected)  // checkmark means no "disabled" entry in prefs
      [[prefs defaults] removeObjectForKey: res];
    else
      [[prefs defaults] setObject:[NSNumber numberWithBool:TRUE] forKey:res];
  }
  [prefs save:self];
  [saver_table reloadData];
}


- (void) selectAllAction:(NSObject *)arg
{
  [self selectAllAction:arg selected:YES];
}


- (void) selectNoneAction:(NSObject *)arg
{
  [self selectAllAction:arg selected:NO];
}


- (void) okAction:(NSObject *)arg
{
  [prefs save:self];
  [NSApp endSheet:self returnCode:NSModalResponseOK];
  [self close];
}

- (void) dealloc
{
  [screen_names release];
  [screen_ids release];
  [savers release];
  [super dealloc];
}

@end


/*************************************************************************

 A built-in screen saver that shows the thumbnail images of the others.

 *************************************************************************/


@implementation RandomizerAttractMode
{
  NSArray *imgs;
  NSTimer *cycle_timer;
  NSImageView *view;
}

- (id)initWithFrame:(NSRect)frame isPreview:(BOOL)p
                           saverBundlePaths:(NSArray *)paths
{
  self = [super initWithFrame:frame isPreview:p];
  if (! self) return 0;

  ya_rand_init (0);

  NSMutableArray *a = [NSMutableArray arrayWithCapacity: 300];
  for (NSString *path in paths) {
    NSString *dir = [[path stringByAppendingPathComponent:@"Contents"]
                           stringByAppendingPathComponent:@"Resources"];
    for (NSString *thumb in @[ @"thumbnail@2x.png", @"thumbnail.png" ]) {
      thumb = [dir stringByAppendingPathComponent: thumb];
      NSImage *img = [[NSImage alloc] initByReferencingFile: thumb];
      if (img && [img size].width > 1) {
        [a addObject: img];
        break;
      }
    }
  }

  imgs = [[NSArray arrayWithArray: a] retain];
  NSLog(@"loaded %lu thumbs", (unsigned long) [imgs count]);

  return self;
}


- (void)cycleThumb
{
  if (!imgs || ![imgs count])
    return;

  NSImage *prev = (view ? view.image : 0);
  NSImage *img = 0;

  // Pick one at random, but try not to use the one we just used, even if
  // there are a small number of them.
  for (int i = 0; i < 100; i++) {
    img = [imgs objectAtIndex: (random() % [imgs count])];
    if (img != prev) break;
  }

  NSRect r = self.bounds;
  int margin = 4;
  r.origin.x = r.origin.y = margin;
  r.size.width  -= margin*2;
  r.size.height -= margin*2;
  NSImageView *view2 = [[NSImageView alloc] initWithFrame: r];

  view2.imageScaling = NSImageScaleProportionallyUpOrDown;
  view2.alphaValue   = 0.0;
  view2.wantsLayer   = YES;
  view2.image        = img;

  [self addSubview: view2];
  [view2 release];

  [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
      context.duration = 0.5;
      if (view)
        view.animator.alphaValue = 0;
      view2.animator.alphaValue = 1;
    }
  completionHandler:^{
      if (view) [view removeFromSuperview];
      view = view2;
    }];

  NSTimeInterval interval = 0.75;

  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;
  if ([imgs count] <= 1) return;  // If there's only one, don't flicker.
  cycle_timer = [NSTimer scheduledTimerWithTimeInterval: interval
                                                 target:self
                                               selector:@selector(cycleThumb)
                                               userInfo:nil
                                                repeats:NO];
}


- (void)startAnimation
{
  [self cycleThumb];
  [super startAnimation];
}

- (void)stopAnimation
{
  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;
  [super stopAnimation];
}

- (BOOL)isAnimating { return !!cycle_timer; }
- (NSTimeInterval)animationTimeInterval { return 1/30.0; }
- (BOOL)hasConfigureSheet { return FALSE; }
- (NSWindow*)configureSheet { return 0; }

- (void) dealloc
{
  if (cycle_timer) [cycle_timer invalidate];
  cycle_timer = 0;
  [imgs release];
  [super dealloc];
}

@end
