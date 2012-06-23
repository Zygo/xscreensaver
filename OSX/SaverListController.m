/* xscreensaver, Copyright (c) 2012 Jamie Zawinski <jwz@jwz.org>
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

#ifdef USE_IPHONE  // whole file


#import "SaverListController.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


@implementation SaverListController

- (id)initWithNames:(NSArray *)names descriptions:(NSDictionary *)descs;
{
  self = [self init];
  if (! self) return 0;
  [self reload:names descriptions:descs];
  return self;
}


- (NSArray *)sectionIndexTitlesForTableView:(UITableView *)tv
{
  int n = countof(list_by_letter);
  NSMutableArray *a = [NSMutableArray arrayWithCapacity: n];
  for (int i = 0; i < n; i++) {
    char s[2];
    s[0] = (i == 'Z'-'A'+1 ? '#' : i+'A');
    s[1] = 0;
    [a addObject: [NSString stringWithCString:s
                            encoding:NSASCIIStringEncoding]];
  }
  return a;
}


- (void) reload:(NSArray *)names descriptions:(NSDictionary *)descs
{
  if (descriptions)
    [descriptions release];
  descriptions = [descs retain];

  int n = countof(list_by_letter);
  for (int i = 0; i < n; i++) {
    list_by_letter[i] = [[NSMutableArray alloc] init];
  }

  for (NSString *name in names) {
    int index = ([name cStringUsingEncoding: NSASCIIStringEncoding])[0];
    if (index >= 'a' && index <= 'z')
      index -= 'a'-'A';
    if (index >= 'A' && index <= 'Z')
      index -= 'A';
    else
      index = n-1;
    [list_by_letter[index] addObject: name];
  }

  active_section_count = 0;
  letter_sections = [[[NSMutableArray alloc] init] retain];
  section_titles = [[[NSMutableArray alloc] init] retain];
  for (int i = 0; i < n; i++) {
    if ([list_by_letter[i] count] > 0) {
      active_section_count++;
      [letter_sections addObject: list_by_letter[i]];
      if (i <= 'Z'-'A')
        [section_titles addObject: [NSString stringWithFormat: @"%c", i+'A']];
      else
        [section_titles addObject: @"#"];
    }
  }
  [self.tableView reloadData];
}


- (NSString *)tableView:(UITableView *)tv
              titleForHeaderInSection:(NSInteger)section
{
  return [section_titles objectAtIndex: section];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv
{
  return active_section_count;
}


- (NSInteger)tableView:(UITableView *)tv
                       numberOfRowsInSection:(NSInteger)section
{
  return [[letter_sections objectAtIndex: section] count];
}

- (NSInteger)tableView:(UITableView *)tv
             sectionForSectionIndexTitle:(NSString *)title
               atIndex:(NSInteger) index
{
  int i = 0;
  for (NSString *sectionTitle in section_titles) {
    if ([sectionTitle isEqualToString: title])
      return i;
    i++;
  }
  return -1;
}


- (UITableViewCell *)tableView:(UITableView *)tv
                     cellForRowAtIndexPath:(NSIndexPath *)ip
{
  NSString *id =
    [[letter_sections objectAtIndex: [ip indexAtPosition: 0]]
      objectAtIndex: [ip indexAtPosition: 1]];
  NSString *desc = [descriptions objectForKey:id];

  UITableViewCell *cell = [tv dequeueReusableCellWithIdentifier: id];
  if (!cell) {
    cell = [[[UITableViewCell alloc]
              initWithStyle: (desc
                              ? UITableViewCellStyleSubtitle
                              : UITableViewCellStyleDefault)
              reuseIdentifier: id]
             autorelease];
    cell.textLabel.text = id;
    if (desc)
      cell.detailTextLabel.text = desc;
  }
  return cell;
}


- (void)tapTimer:(NSTimer *)t
{
  [last_tap release];
  last_tap = 0;
  tap_count = 0;
  tap_timer = 0;
}


- (void)tableView:(UITableView *)tv
    didSelectRowAtIndexPath:(NSIndexPath *)ip
{
  UITableViewCell *cell = [tv cellForRowAtIndexPath: ip];
  selected = cell.textLabel.text;
  [self.navigationItem.leftBarButtonItem  setEnabled: !!selected];
  [self.navigationItem.rightBarButtonItem setEnabled: !!selected];

  if (tap_count == 0) {					// First tap
    tap_count = 1;
    last_tap = [[ip copy] retain];
    tap_timer = [NSTimer scheduledTimerWithTimeInterval: 0.3
                         target:self 
                         selector:@selector(tapTimer:)
                         userInfo:nil
                         repeats:NO];

  } else if (tap_count == 1 && tap_timer &&		// Second tap
             [ip isEqual:last_tap]) {
    [tap_timer invalidate];
    [last_tap release];
    last_tap = 0;
    tap_timer = 0;
    tap_count = 0;
 
    // Press the leftmost button in the button-bar.
    UIBarButtonItem *b = self.navigationItem.leftBarButtonItem;
    [[b target] performSelector: [b action] withObject: cell];

  } else if (! [ip isEqual:last_tap]) {			// Tap on a new row
    if (tap_timer) [tap_timer invalidate];
    tap_timer = 0;
    tap_count = 0;
  }
}


/* We can't select a row immediately after creation, but selecting it
   a little while later works (presumably after redisplay has happened)
   so do it on a timer.
 */
- (void) scrollToCB: (NSTimer *) timer
{
  NSString *name = [timer userInfo];

  int i = 0;
  int j = 0;
  Bool ok = NO;
  for (NSArray *a in letter_sections) {
    j = 0;
    for (NSString *n in a) {
      ok = [n isEqualToString: name];
      if (ok) goto DONE;
      j++;
    }
    i++;
  }
 DONE:
  if (ok) {
    NSIndexPath *ip = [NSIndexPath indexPathForRow: j inSection: i];
    [self.tableView selectRowAtIndexPath:ip
                    animated:NO
                    scrollPosition: UITableViewScrollPositionMiddle];
    [self tableView:self.tableView didSelectRowAtIndexPath:ip];
  }
}


- (void) scrollTo: (NSString *) name
{
  [NSTimer scheduledTimerWithTimeInterval: 0
           target:self
           selector:@selector(scrollToCB:)
           userInfo:name
           repeats:NO];
}


- (void)viewWillAppear:(BOOL)animated 
{
  /* Hitting the back button and returing to this view deselects,
     and we can't re-select it from here, so again, do it once
     we return to the event loop.
   */
  if (selected)
    [NSTimer scheduledTimerWithTimeInterval: 0
             target:self
             selector:@selector(scrollToCB:)
             userInfo:selected
             repeats:NO];
  [super viewWillAppear:animated];
}


- (NSString *) selected
{
  return selected;
}


- (BOOL)shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)o
{
  return YES;
}


- (void)dealloc
{
  for (int i = 0; i < countof(list_by_letter); i++)
    [list_by_letter[i] release];
  [letter_sections release];
  [section_titles release];
  [descriptions release];
  [super dealloc];
}

@end


#endif // USE_IPHONE -- whole file
