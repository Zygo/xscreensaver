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

#import "InvertedSlider.h"

@implementation InvertedSlider

- (id) initWithFrame:(NSRect)r
{
  self = [super initWithFrame:r];
  if (! self) return 0;
  inverted = YES;
  integers = NO;
  return self;
}

- (id) initWithFrame:(NSRect)r inverted:(BOOL)_inv integers:(BOOL)_int
{
  self = [self initWithFrame:r];
  inverted = _inv;
  integers = _int;
  return self;
}


-(double) transformValue:(double) value
{
  double v2 = (integers
               ? (int) (value + (value < 0 ? -0.5 : 0.5))
               : value);
  double low   = [self minValue];
  double high  = [self maxValue];
  double range = high - low;
  double off   = v2 - low;
  if (inverted)
    v2 = low + (range - off);
  // NSLog (@" ... %.1f -> %.1f    [%.1f - %.1f]", value, v2, low, high);
  return v2;
}

-(double) doubleValue;
{
# ifdef USE_IPHONE
  return [self transformValue:[self value]];
# else
  return [self transformValue:[super doubleValue]];
# endif
}

-(void) setDoubleValue:(double)v
{
# ifdef USE_IPHONE
  return [super setValue:[self transformValue:v]];
# else
  return [super setDoubleValue:[self transformValue:v]];
# endif
}


#ifdef USE_IPHONE

- (void)setValue:(float)v animated:(BOOL)a
{
  return [super setValue:[self transformValue:v] animated:a];
}


/* Draw the thumb in the right place by also inverting its position
   relative to the track.
 */
- (CGRect)thumbRectForBounds:(CGRect)bounds
                   trackRect:(CGRect)rect
                       value:(float)value
{
  CGRect thumb = [super thumbRectForBounds:bounds trackRect:rect value:value];
  if (inverted)
    thumb.origin.x = rect.size.width - thumb.origin.x - thumb.size.width;
  return thumb;
}

#endif // !USE_IPHONE



/* Implement all accessor methods in terms of "doubleValue" above.
   (Note that these normally exist only on MacOS, not on iOS.)
 */

-(float) floatValue;
{
  return (float) [self doubleValue];
}

-(int) intValue;
{
  return (int) [self doubleValue];
}

-(id) objectValue;
{
  return [NSNumber numberWithDouble:[self doubleValue]];
}

-(NSString *) stringValue;
{
  return [NSString stringWithFormat:@"%f", [self floatValue]];
}

- (NSAttributedString *)attributedStringValue;
{
  // #### "Build and Analyze" says this leaks. Unsure whether this is true.
  return [[NSAttributedString alloc] initWithString:[self stringValue]];
}


/* Implment all setter methods in terms of "setDoubleValue", above.
 */

-(void) setFloatValue:(float)v
{
  [self setDoubleValue:(double)v];
}

-(void) setIntValue:(int)v
{
  [self setDoubleValue:(double)v];
}

-(void) setObjectValue:(id <NSCopying>)v
{
  NSAssert2((v == nil) ||
            [(NSObject *) v respondsToSelector:@selector(doubleValue)], 
            @"argument %@ to %s does not respond to doubleValue",
            v, __PRETTY_FUNCTION__);
  [self setDoubleValue:[((NSNumber *) v) doubleValue]];
}

-(void) setStringValue:(NSString *)v
{
  [self setDoubleValue:[v doubleValue]];
}

-(void) setAttributedStringValue:(NSAttributedString *)v
{
  [self setStringValue:[v string]];
}

@end
