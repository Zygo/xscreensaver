/* xscreensaver, Copyright (c) 2012-2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Loading URLs and returning the underlying text.
 *
 * This is necessary because iOS doesn't have Perl installed, so we can't
 * run "xscreensaver-text" to do this.
 */

#include "utils.h"

#ifdef USE_IPHONE // whole file

#include "textclient.h"

char *
textclient_mobile_date_string (void)
{
  UIDevice *dd = [UIDevice currentDevice];
  NSString *name = [dd name];			// My iPhone
  NSString *model = [dd model];			// iPad
  // NSString *system = [dd systemName];	// iPhone OS
  NSString *vers = [dd systemVersion];		// 5.0
  NSString *date =
    [NSDateFormatter
      localizedStringFromDate:[NSDate date]
      dateStyle: NSDateFormatterMediumStyle
      timeStyle: NSDateFormatterMediumStyle];
  NSString *nl = @"\n";

  NSString *result = name;
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: model];
  // result = [result stringByAppendingString: nl];
  // result = [result stringByAppendingString: system];
  result = [result stringByAppendingString: @" "];
  result = [result stringByAppendingString: vers];
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: date];
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: nl];
  return strdup ([result cStringUsingEncoding:NSISOLatin1StringEncoding]);
}


@interface TextLoader : NSObject
@property (nonatomic, retain) NSURL *url;
@property (nonatomic, retain) NSString *result;
@end

@implementation TextLoader
{
  NSURL    *_url;
  NSString *_result;
}

+ (TextLoader *) sharedLoader
{
  static TextLoader *singleton = nil;
  @synchronized(self) {
    if (!singleton)
      singleton = [[self alloc] init];
  }
  return singleton;
}

- (void) startLoading
{
  // NSLog(@"textclient thread loading %@", self.url);
  self.result = [NSString stringWithContentsOfURL: self.url
                          encoding: NSUTF8StringEncoding
                          error: nil];
  // NSLog(@"textclient thread finished %@ (length %d)", self.url,
  //      (unsigned int) [self.result length]);
}

@end



/* Returns the contents of the URL.
   Loads the URL in a background thread: if the URL has not yet loaded,
   this will return NULL. Once the URL has completely loaded, the full
   contents will be returned.  Calling this again after that starts the
   URL loading again.
 */
char *
textclient_mobile_url_string (Display *dpy, const char *url)
{
  TextLoader *loader = [TextLoader sharedLoader];
  NSString *result = [loader result];

  // Since this is a singleton, it's possible that if hack #1 starts
  // URL #1 loading and then quickly exits, and then hack #2 asks for
  // URL #2, it might get URL #1 instead.  Oh well, who cares.

  if (result) {						// Thread finished
    // NSLog(@"textclient finished %s (length %d)", url,
    //       (unsigned int) [result length]);
    char *s = strdup ([result cStringUsingEncoding:NSUTF8StringEncoding]);
    loader.url    = nil;
    loader.result = nil;
    return s;

  } else if ([loader url]) {				// Waiting on thread
    // NSLog(@"textclient waiting...");
    return 0;

  } else {						// Launch thread
    // NSLog(@"textclient launching %s...", url);
    loader.url =
      [NSURL URLWithString:
               [NSString stringWithCString: url
                         encoding:NSISOLatin1StringEncoding]];
    [NSThread detachNewThreadSelector: @selector(startLoading)
              toTarget: loader withObject: nil];
    return 0;
  }
}

#endif // USE_IPHONE -- whole file
