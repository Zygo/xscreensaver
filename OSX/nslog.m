/* xscreensaver, Copyright © 2023-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#import <os/log.h>
#import "nslog.h"

/* 2023: WTF! On macOS 14.0, NSLog "%.02f" prints like "1,234.0000"
   But both sprintf and stringWithFormat work correctly!

   2026: On macOS 27, most of our NSLog calls are turning into "<private>".
   Using os_log() instead, with the "%{public}@" incantation, fixes that.
 */
void
xscreensaver_nslog (NSString *format, ...)
{
  va_list args;
  va_start (args, format);
  NSString *s = [[NSString alloc] initWithFormat:format arguments:args];
  va_end (args);

  // NSLog (@"%@", s);
  os_log_with_type (OS_LOG_DEFAULT, OS_LOG_TYPE_DEFAULT, "%{public}@", s);

# if 0   // For debugging legacyScreenSaver without the /usr/bin/log quagmire.
#  ifndef HAVE_IPHONE
  NSDateFormatter *f = [[NSDateFormatter alloc] init];
  [f setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
  NSString *date = [f stringFromDate: [NSDate date]];
  s = [NSString stringWithFormat:@"%@ %@\n", date, s];
  NSString *file = @"/tmp/log.txt";
  NSFileHandle *out = [NSFileHandle fileHandleForWritingAtPath: file];
  if (! out) {
    [[NSFileManager defaultManager] createFileAtPath:file
                                            contents:nil attributes:nil];
    out = [NSFileHandle fileHandleForWritingAtPath: file];
    // if (!out) abort();
  }
  [out seekToEndOfFile];
  [out writeData: [s dataUsingEncoding:NSUTF8StringEncoding]];
  [out closeFile];
#  endif // HAVE_IPHONE
# endif // 0
}
