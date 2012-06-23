/* xscreensaver, Copyright (c) 2006-2012 Jamie Zawinski <jwz@jwz.org>
*
* Permission to use, copy, modify, distribute, and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice appear in all copies and that both that
* copyright notice and this permission notice appear in supporting
* documentation.  No representations are made about the suitability of this
* software for any purpose.  It is provided "as is" without express or 
* implied warranty.
*/

#ifdef USE_IPHONE
# import <UIKit/UIKit.h>
#else
# import <Cocoa/Cocoa.h>
#endif

int
main (int argc, char *argv[])
{
# ifdef USE_IPHONE
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int ret = UIApplicationMain (argc, argv, nil, nil);
  [pool release];
  return ret;
# else
  return NSApplicationMain(argc, (const char **) argv);
# endif
}
