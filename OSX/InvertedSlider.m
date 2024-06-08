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
 */

#import "InvertedSlider.h"

#ifndef HAVE_TVOS

@implementation InvertedSlider

- (id) initWithFrame:(NSRect)r
{
  self = [super initWithFrame:r];
  if (! self) return 0;
  inverted = YES;
  ratio    = NO;
  integers = NO;
  return self;
}

- (id) initWithFrame:(NSRect)r
            inverted:(BOOL)_inv
               ratio:(BOOL)_ratio
            integers:(BOOL)_int
{
  self = [self initWithFrame:r];
  inverted = _inv;
  ratio    = _ratio;
  integers = _int;
  NSAssert (!(inverted && ratio), @"inverted and ratio can't both be true");
  return self;
}


-(double) increment
{
  return increment;
}


-(void) setIncrement:(double)v
{
  increment = v;
}


// For simplicity, "ratio" sliders in the UI all run from 0.0 to 1.0,
// so we need to wrap the setters.

#ifdef HAVE_IPHONE
# define VTYPE float
#else
# define VTYPE double
#endif

-(void) setMinValue:(VTYPE)v
{
  origMinValue = v;
  if (ratio) v = 0;
  [super setMinValue: v];
}

-(void) setMaxValue:(VTYPE)v
{
  origMaxValue = v;
  if (ratio) v = 1;
  [super setMaxValue: v];
}


/* In: 0-1; Out: low-high. */
static float
ratio_to_range (double low, double high, double ratio)
{
  return (ratio > 0.5
          ? (1   + (2 * (ratio - 0.5) * (high - 1)))
          : (low + (2 * ratio         * (1 - low))));
}

/* In: low-high; Out: 0-1. */
static double
range_to_ratio (double low, double high, double value)
{
  return (value > 1
          ? ((value - 1)   / (2 * (high - 1))) + 0.5
          : ((value - low) / (2 * (1 - low))));
}


-(double) transformValue:(double) value set:(BOOL)set
{
  double v2 = value;

  if (increment)
    v2 = round (v2 / increment) * increment;
  if (integers)
    v2 = (int) (v2 + (v2 < 0 ? -0.5 : 0.5));

  double low   = origMinValue;
  double high  = origMaxValue;
  double range = high - low;
  double off   = v2 - low;

  if (inverted)
    v2 = low + (range - off);
  else if (ratio)
    v2 = (set
          ? range_to_ratio (low, high, v2)
          : ratio_to_range (low, high, v2));

  // if (ratio)
  //   NSLog(@"... %d %.2f %.2f %.2f  mm %.2f %.2f  v %.2f %.2f",
  //         set, low, high, range,
  //         [self minValue], [self maxValue], 
  //         value, v2);

  // NSLog (@" ... %.1f -> %.1f    [%.1f - %.1f]", value, v2, low, high);
  return v2;
}

#ifndef HAVE_IPHONE

/* On MacOS, we have to transform the value on every entry and exit point
   to this class.  So, we implement doubleValue and setDoubleValue to
   transform the value; and we then have to re-implement every getter and
   setter in terms of those.  There's no way to simply change how the
   slider is displayed without mucking with the value inside of it.
 */

-(double) doubleValue
{
  return [self transformValue:[super doubleValue] set:NO];
}

-(void) setDoubleValue:(double)v
{
  return [super setDoubleValue:[self transformValue:v set:YES]];
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
  return [[[NSAttributedString alloc] initWithString:[self stringValue]]
           autorelease];
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

#else  // HAVE_IPHONE

/* On iOS, we have control over how the value is displayed, but there's no
   way to transform the value on input and output: if we wrap 'value' and
   'setValue' analogously to what we do on MacOS, things fail in weird
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
  CGRect thumb =
    [super thumbRectForBounds: bounds
                    trackRect: rect 
                        value: (ratio
                                ? value
                                : [self transformValue:value set:NO])];
  if (inverted)
    thumb.origin.x = rect.size.width - thumb.origin.x - thumb.size.width;

  return thumb;
}

-(double) transformedValue
{
  return [self transformValue: [self value] set:FALSE];
}

-(void) setTransformedValue:(double)v
{
  [self setValue: [self transformValue: v set:TRUE]];
}

#endif // HAVE_IPHONE


@end

#endif // !HAVE_TVOS
