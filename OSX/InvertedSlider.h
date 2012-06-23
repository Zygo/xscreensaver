/* xscreensaver, Copyright (c) 2006-2012 Jamie Zawinski <jwz@jwz.org>
*
* Permission to use, copy, modify, distribute, and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice appear in all copies and that both that
* copyright notice and this permission notice appear in supporting
* documentation.  No representations are made about the suitability of this
* software for any purpose.  It is provided "as is" without express or 
* implied warranty.
*
* This is a subclass of NSSlider that is flipped horizontally:
* the high value is on the left and the low value is on the right.
*/

#ifdef USE_IPHONE
# import <UIKit/UIKit.h>
# define NSSlider UISlider
# define NSRect   CGRect
# define minValue minimumValue
# define maxValue maximumValue
#else
# import <Cocoa/Cocoa.h>
#endif

@interface InvertedSlider : NSSlider
{
  BOOL inverted;
  BOOL integers;
}

- (id) initWithFrame:(NSRect)r inverted:(BOOL)_inv integers:(BOOL)_int;

@end
