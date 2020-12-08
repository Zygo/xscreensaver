/* xscreensaver, Copyright (c) 2012-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This implements the top-level screen-saver selection list in the iOS app.
 */

#ifdef HAVE_IPHONE

#import <UIKit/UIKit.h>

@interface SaverListController : UITableViewController <UISearchBarDelegate> {

  UIView *v;
  UILabel *label1;
  UILabel *label2;
  UISearchBar *search;

  int active_section_count;
  NSMutableArray *list_by_letter[26];  // 27 to get "#" after "Z".
  NSMutableArray *letter_sections;
  NSMutableArray *section_titles;
  NSArray *names;
  NSDictionary *descriptions;

  int tap_count;
  NSTimer *tap_timer;
  NSIndexPath *last_tap;
}

- (id)initWithNames:(NSArray *)names descriptions:(NSDictionary *)descs;
- (void) scrollTo:(NSString *)name;
@end

#endif // HAVE_IPHONE
