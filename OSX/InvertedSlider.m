/* xscreensaver, Copyright (c) 2006 Jamie Zawinski <jwz@jwz.org>
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

-(double) transformValue:(double) value
{
  double low   = [self minValue];
  double high  = [self maxValue];
  double range = high - low;
  double off   = value - low;
  double trans = low + (range - off);
  // NSLog (@" ... %.1f -> %.1f    [%.1f - %.1f]", value, trans, low, high);
  return trans;
}

-(double) doubleValue;
{
  return [self transformValue:[super doubleValue]];
}

-(void) setDoubleValue:(double)v
{
  return [super setDoubleValue:[self transformValue:v]];
}


/* Implement all accessor methods in terms of "doubleValue" above.
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
