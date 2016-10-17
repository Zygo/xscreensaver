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


/* Returns the contents of the URL. */
char *
textclient_mobile_url_string (Display *dpy, const char *url)
{
  NSURL *nsurl =
    [NSURL URLWithString:
             [NSString stringWithCString: url
                       encoding:NSISOLatin1StringEncoding]];
  NSString *body =
    [NSString stringWithContentsOfURL: nsurl
              encoding: NSUTF8StringEncoding
              error: nil];
  return (body
          ? strdup ([body cStringUsingEncoding:NSUTF8StringEncoding])
          : 0);
}

#endif // USE_IPHONE -- whole file
