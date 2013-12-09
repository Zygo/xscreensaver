/* xscreensaver, Copyright (c) 2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef USE_IPHONE
#import <Cocoa/Cocoa.h>
@interface XScreenSaverUpdater : NSObject <NSApplicationDelegate>
@end
#endif // !USE_IPHONE

#define UPDATER_DOMAIN "org.jwz.xscreensaver.updater"

// Strings must match Sparkle/SUConstants.m
#define SUSUEnableAutomaticChecksKey	"SUEnableAutomaticChecks"
#define SUSUEnableAutomaticChecksDef	YES
#define SUAutomaticallyUpdateKey	"SUAutomaticallyUpdate"
#define SUAutomaticallyUpdateDef	NO
#define SUSendProfileInfoKey		"SUSendProfileInfo"
#define SUSendProfileInfoDef		YES
#define SUScheduledCheckIntervalKey	"SUScheduledCheckInterval"
#define SUScheduledCheckIntervalDef	604800

#define UPDATER_DEFAULTS @{					\
  @SUSUEnableAutomaticChecksKey: @SUSUEnableAutomaticChecksDef,	\
  @SUAutomaticallyUpdateKey:	 @SUAutomaticallyUpdateDef,	\
  @SUSendProfileInfoKey:         @SUSendProfileInfoDef,		\
  @SUScheduledCheckIntervalKey:  @SUScheduledCheckIntervalDef	\
}
