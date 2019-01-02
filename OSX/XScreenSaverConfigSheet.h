/* xscreensaver, Copyright (c) 2006-2018 Jamie Zawinski <jwz@jwz.org>
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

#ifdef USE_IPHONE
# import <Foundation/Foundation.h>
# import <UIKit/UIKit.h>
# define NSView UIView
# define NSUserDefaultsController NSUserDefaults
#else
# import <Cocoa/Cocoa.h>
#endif

#import "jwxyz.h"

#import <Foundation/NSXMLParser.h>

#undef USE_PICKER_VIEW

@interface XScreenSaverConfigSheet : 
# ifdef USE_IPHONE
	   UITableViewController <NSXMLParserDelegate,
				  UITextFieldDelegate
#  ifdef USE_PICKER_VIEW
				  , UIPickerViewDelegate
				  , UIPickerViewDataSource
#  endif
				  >
# else
	   NSPanel <NSXMLParserDelegate>
# endif
{
  NSString *saver_name;
  NSUserDefaultsController *userDefaultsController;
  NSUserDefaultsController *globalDefaultsController;
  NSDictionary *defaultOptions;
  const XrmOptionDescRec *opts;
  id xml_root, xml_parsing;

# ifdef USE_IPHONE
  UITextField *active_text_field;
  NSMutableArray *controls;
  NSMutableArray *pref_ctls;	// UIControl objects, with index = c.tag
  NSMutableArray *pref_keys;	// ...and their corresponding resources
#  ifdef USE_PICKER_VIEW
  NSMutableArray *picker_values;
#  endif
# endif

}

- (id)initWithXML: (NSData *) xml_data
          options: (const XrmOptionDescRec *) opts
       controller: (NSUserDefaultsController *) prefs
 globalController: (NSUserDefaultsController *) globalPrefs
         defaults: (NSDictionary *) defs;

@end
