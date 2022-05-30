/* xscreensaver, Copyright © 2006-2022 Jamie Zawinski <jwz@jwz.org>
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
 *
 * It also implements ratio sliders, where 1.0 is forced to the middle.
 */

#ifdef HAVE_IPHONE
# import <UIKit/UIKit.h>
# define NSSlider UISlider
# define NSRect   CGRect
# define minValue minimumValue
# define maxValue maximumValue
# define setMinValue setMinimumValue
# define setMaxValue setMaximumValue
#else
# import <Cocoa/Cocoa.h>
#endif

#ifndef HAVE_TVOS

@interface InvertedSlider : NSSlider
{
  BOOL inverted;
  BOOL ratio;
  BOOL integers;
  double increment;
  double origMaxValue;
  double origMinValue;
}

- (double) increment;
- (void) setIncrement:(double)v;

- (id) initWithFrame:(NSRect)r
            inverted:(BOOL)_inv
               ratio:(BOOL)_ratio
            integers:(BOOL)_int;

# ifdef HAVE_IPHONE
- (double) transformedValue;
- (void) setTransformedValue:(double)v;
# endif

@end

#endif // !HAVE_TVOS
