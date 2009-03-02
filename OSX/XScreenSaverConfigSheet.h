/* xscreensaver, Copyright (c) 2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* XScreenSaver uses XML files to describe the user interface for configuring
   the various screen savers.  These files live in .../hacks/config/ and
   say relatively high level things like: "there should be a checkbox
   labelled "Leave Trails", and when it is checked, add the option '-trails'
   to the command line when launching the program."

   This code reads that XML and constructs a Cocoa interface from it.
   The Cocoa controls are hooked up to NSUserDefaultsController to save
   those settings into the MacOS preferences system.  The Cocoa preferences
   names are the same as the resource names specified in the screenhack's
   'options' array (we use that array to map the command line switches
   specified in the XML to the resource names to use).
 */

#import <Cocoa/Cocoa.h>
#import "jwxyz.h"

@interface XScreenSaverConfigSheet : NSWindow
{
  NSUserDefaultsController *userDefaultsController;
}

- (id)initWithXMLFile: (NSString *) xml_file
              options: (const XrmOptionDescRec *) opts
           controller: (NSUserDefaultsController *) prefs;

@end
