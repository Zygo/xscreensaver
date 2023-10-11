/* xscreensaver, Copyright © 2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_NSLOG__
#define __XSCREENSAVER_NSLOG__

#ifndef HAVE_IPHONE

/* WTF! On macOS 14.0, NSLog "%.02f" prints like "1,234.0000"
   But both sprintf and stringWithFormat work correctly!
 */
static void
xscreensaver_nslog (NSString *format, ...)
{
  va_list args;
  va_start (args, format);
  va_end (args);
  NSString *s = [[NSString alloc] initWithFormat:format arguments:args];
  NSLog (@"%@", s);

# if 0
  // Debugging legacyScreenSaver without the /usr/bin/log quagmire.
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
# endif // 0
}

#define NSLog xscreensaver_nslog

#endif // HAVE_IPHONE
#endif // __XSCREENSAVER_NSLOG__
