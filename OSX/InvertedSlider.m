/* xscreensaver, Copyright (c) 2006-2013 Jamie Zawinski <jwz@jwz.org>
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

#ifndef USE_IPHONE

/* On MacOS, we have to transform the value on every entry and exit point
   to this class.  So, we implement doubleValue and setDoubleValue to
   transform the value; and we then have to re-implement every getter and
   setter in terms of those.  There's no way to simply change how the
   slider is displayed without mucking with the value inside of it.
 */

-(double) doubleValue
{
  return [self transformValue:[super doubleValue]];
}

-(void) setDoubleValue:(double)v
{
  return [super setDoubleValue:[self transformValue:v]];
}

-(float)floatValue       { return (float) [self doubleValue]; }
-(int)intValue           { return (int) [self doubleValue]; }
-(NSInteger)integerValue { return (NSInteger) [self doubleValue]; }
-(id)objectValue { return [NSNumber numberWithDouble:[self doubleValue]]; }

-(NSString *)stringValue
{
  if (integers)
    return [NSString stringWithFormat:@"%d", [self intValue]];
  else
    return [NSString stringWithFormat:@"%f", [self doubleValue]];
}

- (NSAttributedString *)attributedStringValue;
{
  // #### "Build and Analyze" says this leaks. Unsure whether this is true.
  return [[NSAttributedString alloc] initWithString:[self stringValue]];
}

-(void)setFloatValue:(float)v       { [self setDoubleValue: (double) v];      }
-(void)setIntValue:  (int)v         { [self setDoubleValue: (double) v];      }
-(void)setIntegerValue:(NSInteger)v { [self setDoubleValue: (double) v];      }
-(void)setStringValue:(NSString *)v { [self setDoubleValue: [v doubleValue]]; }
-(void)takeIntValueFrom:(id)f       { [self setIntValue:    [f intValue]];    }
-(void)takeFloatValueFrom:(id)f     { [self setFloatValue:  [f floatValue]];  }
-(void)takeDoubleValueFrom:(id)f    { [self setDoubleValue: [f doubleValue]]; }
-(void)takeStringValueFrom:(id)f    { [self setStringValue: [f stringValue]]; }
-(void)takeObjectValueFrom:(id)f    { [self setObjectValue: [f objectValue]]; }
-(void)takeIntegerValueFrom:(id)f   { [self setIntegerValue:[f integerValue]];}
-(void) setAttributedStringValue:(NSAttributedString *)v {
  [self setStringValue:[v string]];
}

-(void) setObjectValue:(id <NSCopying>)v
{
  NSAssert2((v == nil) ||
            [(NSObject *) v respondsToSelector:@selector(doubleValue)], 
            @"argument %@ to %s does not respond to doubleValue",
            v, __PRETTY_FUNCTION__);
  [self setDoubleValue:[((NSNumber *) v) doubleValue]];
}

#else  // USE_IPHONE

/* On iOS, we have control over how the value is displayed, but there's no
   way to transform the value on input and output: if we wrap 'value' and
   'setValue' analagously to what we do on MacOS, things fail in weird
   ways.  Presumably some parts of the system are accessing the value
   instance variable directly instead of going through the methods.

   So the only way around this is to enforce that all of our calls into
   this object use a new API: 'transformedValue' and 'setTransformedValue'.
   The code in XScreenSaverConfigSheet uses that instead.
 */

- (CGRect)thumbRectForBounds:(CGRect)bounds
                   trackRect:(CGRect)rect
                       value:(float)value
{
  CGRect thumb = [super thumbRectForBounds: bounds
                                 trackRect: rect 
                                     value: [self transformValue:value]];
  if (inverted)
    thumb.origin.x = rect.size.width - thumb.origin.x - thumb.size.width;
  return thumb;
}

-(double) transformedValue
{
  return [self transformValue: [self value]];
}

-(void) setTransformedValue:(double)v
{
  [self setValue: [self transformValue: v]];
}

#endif // USE_IPHONE


@end
