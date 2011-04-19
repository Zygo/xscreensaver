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

#import <Cocoa/Cocoa.h>

@interface InvertedSlider : NSSlider
{
}
@end
