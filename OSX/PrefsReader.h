/* xscreensaver, Copyright (c) 2006-2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This implements the substrate of the xscreensaver preferences code:
   It does this by writing defaults to, and reading values from, the
   NSUserDefaultsController (and ScreenSaverDefaults/NSUserDefaults)
   and thereby reading the preferences that may have been edited by
   the UI (XScreenSaverConfigSheet).
 */

#ifdef HAVE_IPHONE
# import <Foundation/Foundation.h>
# import <UIKit/UIKit.h>
# define NSUserDefaultsController NSUserDefaults
#else
# import <Cocoa/Cocoa.h>
#endif


#import "jwxyz.h"

@interface PrefsReader : NSObject
{
  NSString *saver_name;

  NSUserDefaults *userDefaults;   // this is actually a 'ScreenSaverDefaults'
  NSUserDefaultsController *userDefaultsController;

  NSUserDefaults *globalDefaults; // for prefs shared by all xscreensavers.
  NSUserDefaultsController *globalDefaultsController;

  NSDictionary *defaultOptions;   // Hardcoded defaults before any changes.
}

- (id) initWithName: (NSString *) name
            xrmKeys: (const XrmOptionDescRec *) opts
           defaults: (const char * const *) defs;

- (NSUserDefaultsController *) userDefaultsController;
- (NSUserDefaultsController *) globalDefaultsController;
- (NSDictionary *) defaultOptions;

- (char *) getStringResource:  (const char *) name;
- (double) getFloatResource:   (const char *) name;
- (int)    getIntegerResource: (const char *) name;
- (BOOL)   getBooleanResource: (const char *) name;

@end
