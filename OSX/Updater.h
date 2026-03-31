/* xscreensaver, Copyright © 2013-2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef IN_UPDATER
# import <Cocoa/Cocoa.h>
# import <Sparkle/SPUUpdaterDelegate.h>
# import <Sparkle/SPUStandardUserDriverDelegate.h>

@interface XScreenSaverUpdater : NSObject <NSApplicationDelegate,
                                           SPUUpdaterDelegate,
                                           SPUStandardUserDriverDelegate>
{
}
@end
#endif // IN_UPDATER

#define UPDATER_DOMAIN "org.jwz.xscreensaver.XScreenSaverUpdater"

// Strings must match Sparkle/SUConstants.m
#define SUSUEnableAutomaticChecksKey	"SUEnableAutomaticChecks"
#define SUSUEnableAutomaticChecksDef	YES
#define SUAutomaticallyUpdateKey	"SUAutomaticallyUpdate"
#define SUAutomaticallyUpdateDef	NO
#define SUSendProfileInfoKey		"SUSendProfileInfo"
#define SUSendProfileInfoDef		YES
#define SULastCheckTimeKey		"SULastCheckTime"
#define SULastCheckTimeDef		"1992-08-17 19:00:00 +0000"

#define SUScheduledCheckIntervalKey	"SUScheduledCheckInterval"
#ifdef IN_UPDATER
# define SUScheduledCheckIntervalDef	86400	// Updater: 1 day
#else
# define SUScheduledCheckIntervalDef	604800	// Savers: 2 weeks
#endif

#define UPDATER_DEFAULTS @{					\
  @SUSUEnableAutomaticChecksKey: @SUSUEnableAutomaticChecksDef,	\
  @SUAutomaticallyUpdateKey:	 @SUAutomaticallyUpdateDef,	\
  @SUSendProfileInfoKey:         @SUSendProfileInfoDef,		\
  @SULastCheckTimeKey:		 @SULastCheckTimeDef,		\
  @SUScheduledCheckIntervalKey:  @SUScheduledCheckIntervalDef,	\
}
