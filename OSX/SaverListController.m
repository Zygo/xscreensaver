/* xscreensaver, Copyright © 2012-2021 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_IPHONE  // whole file


#import "SaverListController.h"
#import "SaverRunner.h"
#import "yarandom.h"
#import "version.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


@implementation SaverListController

- (void) titleTapped:(id) sender
{
  UIApplication *app = [UIApplication sharedApplication];
  NSURL *url = [NSURL URLWithString:@"https://www.jwz.org/xscreensaver/"];

  if ([app respondsToSelector:@selector(openURL:options:completionHandler:)]) {
    [app openURL:url options:@{} completionHandler:nil];
  } else {
#   pragma clang diagnostic push   // "openURL deprecated in iOS 10"
#   pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [app openURL:url];
#   pragma clang diagnostic pop
  }
}


- (void)makeTitleBar
{
  // Extract the version number and release date from the version string.
  // Here's an area where I kind of wish I had "Two Problems".
  // I guess I could add custom key to the Info.plist for this.

  NSArray *a = [[NSString stringWithCString: screensaver_id
                          encoding:NSUTF8StringEncoding]
                 componentsSeparatedByCharactersInSet:
                   [NSCharacterSet
                     characterSetWithCharactersInString:@" ()-"]];
  NSString *vers = [a objectAtIndex: 3];
  NSString *year = [a objectAtIndex: 7];

  NSString *line1 = [@"XScreenSaver " stringByAppendingString: vers];
  NSString *line2 = [@"\u00A9 " stringByAppendingString:
                        [year stringByAppendingString:
                                @" Jamie Zawinski <jwz@jwz.org>"]];

  v = [[UIView alloc] initWithFrame:CGRectZero];

  // The "go to web page" button on the right

  UIImage *img = [UIImage imageWithContentsOfFile:
                            [[[NSBundle mainBundle] bundlePath]
                              stringByAppendingPathComponent:
                                @"iSaverRunner57t.png"]];
  UIButton *button = [[UIButton alloc] init];
  [button setFrame: CGRectMake(0, 0, img.size.width/2, img.size.height/2)];
  [button setBackgroundImage:img forState:UIControlStateNormal];
  [button addTarget:self
          action:@selector(titleTapped:)
          forControlEvents:UIControlEventTouchUpInside];
  UIBarButtonItem *bi = [[UIBarButtonItem alloc] initWithCustomView: button];
  self.navigationItem.rightBarButtonItem = bi;
  [bi release];
  [button release];

  // The title bar

  label1 = [[UILabel alloc] initWithFrame:CGRectZero];
  label2 = [[UILabel alloc] initWithFrame:CGRectZero];
  [label1 setText: line1];
  [label2 setText: line2];
  [label1 setBackgroundColor:[UIColor clearColor]];
  [label2 setBackgroundColor:[UIColor clearColor]];

  [label1 setFont: [UIFont boldSystemFontOfSize: 17]];
  [label2 setFont: [UIFont systemFontOfSize: 12]];
  [label1 sizeToFit];
  [label2 sizeToFit];

  label2.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;

  v.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  [v setFrame:CGRectMake(0, 0, self.view.frame.size.width, 0)];

  [v addSubview:label1];
  [v addSubview:label2];

  // Default opacity looks bad.
  [v setBackgroundColor:[[v backgroundColor] colorWithAlphaComponent:1]];

  self.navigationItem.titleView = v;

# ifndef HAVE_TVOS
  search = [[UISearchBar alloc] init];
  search.delegate = self;
  search.placeholder = NSLocalizedString(@"Search...", @"");
  self.tableView.tableHeaderView = search;
# endif // !HAVE_TVOS

  // Dismiss the search field's keyboard as soon as we scroll.
# ifdef __IPHONE_7_0
  if ([self.tableView respondsToSelector:@selector(keyboardDismissMode)])
    [self.tableView setKeyboardDismissMode:
           UIScrollViewKeyboardDismissModeOnDrag];
# endif
}

- (void)viewWillLayoutSubviews
{
  CGRect r1 = [label1 frame];
  CGRect r2 = [label2 frame];
  CGRect r3 = [v frame];

  CGRect win = [self view].frame;
  if (r3.size.width > 385) {
    [label1 setTextAlignment: NSTextAlignmentLeft];
    [label2 setTextAlignment: NSTextAlignmentRight];
    r1.origin     = CGPointMake(6, 0);
    r1.size.width = r3.size.width - 12;
    r2 = r1;
    r3.size.height = r2.size.height;

  } else {
    [label1 setTextAlignment: NSTextAlignmentLeft];
    [label2 setTextAlignment: NSTextAlignmentLeft];
    r1.origin = CGPointMake(0, -1);    // make it fit in landscape
    r1.size.width = r3.size.width;
    r2.origin = CGPointMake(0, r1.origin.y + r1.size.height - 2);
    r2.size.width = r3.size.width;
    r3.size.height = r1.size.height + r2.size.height;
  }

  r3.size.width = win.size.width;

  [label1 setFrame:r1];
  [label2 setFrame:r2];
  [v setFrame:r3];

  win.origin.x = 0;
  win.origin.y = 0;
  win.size.height = 44; // #### This cannot possibly be right.

  search.frame = win;

  [super viewWillLayoutSubviews];
}


- (id)initWithNames:(NSArray *)_names descriptions:(NSDictionary *)_descs;
{
  self = [self init];
  if (! self) return 0;
  [self reload:_names descriptions:_descs search:nil];
  [self makeTitleBar];
  return self;
}


- (NSArray *)sectionIndexTitlesForTableView:(UITableView *)tv
{
  int n = countof(list_by_letter);
  NSMutableArray *a = [NSMutableArray arrayWithCapacity: n];
  for (int i = 0; i < n; i++) {
    if ([list_by_letter[i] count] == 0)  // Omit empty letter sections.
      continue;
    char s[2];
    s[0] = (i == 'Z'-'A'+1 ? '#' : i+'A');
    s[1] = 0;
    [a addObject: [NSString stringWithCString:s
                            encoding:NSUTF8StringEncoding]];
  }
  return a;
}


/* Called when text is typed into the top search bar.
 */
- (void)searchBar:(UISearchBar *)bar textDidChange:(NSString *)txt
{
  [self reload:names descriptions:descriptions search:txt];
}


- (void) reload:(NSArray *)_names descriptions:(NSDictionary *)_descs
         search:search
{
  if (names != _names) {
    if (names) [names release];
    names = [_names retain];
  }
  if (_descs != descriptions) {
    if (descriptions) [descriptions release];
    descriptions = [_descs retain];
  }

  int n = countof(list_by_letter);
  for (int i = 0; i < n; i++) {
    list_by_letter[i] = [[NSMutableArray alloc] init];
  }

  for (NSString *name in names) {

    // If we're searching, omit any items that don't have a match in the
    // title or description.
    //
    BOOL matchp = (!search || [search length] == 0);
    if (! matchp) {
      matchp = ([name rangeOfString:search
                            options:NSCaseInsensitiveSearch].location
                != NSNotFound);
    }
    if (! matchp) {
      NSString *desc = [descriptions objectForKey:name];
      matchp = ([desc rangeOfString:search
                            options:NSCaseInsensitiveSearch].location
                != NSNotFound);
    }
    if (! matchp)
      continue;

    int index = ([name cStringUsingEncoding: NSUTF8StringEncoding])[0];
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
      [list_by_letter[i] sortUsingSelector:
                           @selector(localizedCaseInsensitiveCompare:)];
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
  NSString *title = 
    [[letter_sections objectAtIndex: [ip indexAtPosition: 0]]
      objectAtIndex: [ip indexAtPosition: 1]];
  NSString *desc = [descriptions objectForKey:title];

  NSString *id = @"Cell";
  UITableViewCell *cell = [tv dequeueReusableCellWithIdentifier:id];
  if (!cell)
    cell = [[[UITableViewCell alloc]
                initWithStyle: UITableViewCellStyleSubtitle
              reuseIdentifier: id]
             autorelease];

  cell.textLabel.text = title;
# ifndef HAVE_TVOS
  cell.accessoryType = UITableViewCellAccessoryDetailDisclosureButton;
# endif // !HAVE_TVOS
  cell.detailTextLabel.text = desc;

  return cell;
}


/* Selecting a row launches the saver.
 */
- (void)tableView:(UITableView *)tv
        didSelectRowAtIndexPath:(NSIndexPath *)ip
{
  UITableViewCell *cell = [tv cellForRowAtIndexPath: ip];
  SaverRunner *s = 
    (SaverRunner *) [[UIApplication sharedApplication] delegate];
  if (! s) return;

  // Dismiss the search field's keyboard before launching a saver.
  [self.tableView.tableHeaderView resignFirstResponder];

  NSAssert ([s isKindOfClass:[SaverRunner class]], @"not a SaverRunner");
  [s loadSaver: cell.textLabel.text];
}

/* Selecting a row's Disclosure Button opens the preferences.
 */
- (void)tableView:(UITableView *)tv
        accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)ip
{
  UITableViewCell *cell = [tv cellForRowAtIndexPath: ip];
  SaverRunner *s = 
    (SaverRunner *) [[UIApplication sharedApplication] delegate];
  if (! s) return;
  NSAssert ([s isKindOfClass:[SaverRunner class]], @"not a SaverRunner");
  [s openPreferences: cell.textLabel.text];
}


- (void) scrollTo: (NSString *) name
{
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
  }
}


/* We need this to respond to "shake" gestures
 */
- (BOOL)canBecomeFirstResponder
{
  return YES;
}

- (void)motionBegan:(UIEventSubtype)motion withEvent:(UIEvent *)event
{
}

- (void)motionCancelled:(UIEventSubtype)motion withEvent:(UIEvent *)event
{
}


/* Shake means load a random screen saver.
 */
- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent *)event
{
  if (motion != UIEventSubtypeMotionShake)
    return;
  NSMutableArray *a = [NSMutableArray arrayWithCapacity: 200];
  for (NSArray *sec in letter_sections)
    for (NSString *s in sec) {

# ifdef HAVE_IPHONE
      // Check the "SaverName.globalCycleSelected" preference.

      NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
      NSString *key = [s stringByAppendingString:@".globalCycleSelected"];
      NSObject *o = [prefs objectForKey:key];
      BOOL checked_p;

      // Roughly duplicate logic of PrefsReader:getBooleanResource
      if (o && [o isKindOfClass:[NSNumber class]]) {
        double n = [(NSNumber *) o doubleValue];
        checked_p = !!n;
      } else if (o && [o isKindOfClass:[NSString class]]) {
        NSString *ss = [((NSString *) o) lowercaseString];
        checked_p = ([ss isEqualToString:@"true"] ||
                     [ss isEqualToString:@"yes"] ||
                     [ss isEqualToString:@"1"]);
      } else {
        checked_p = YES;  // Default true
      }

      if (! checked_p)  // Omit from list if un-checked
        continue;
# endif // HAVE_IPHONE

      [a addObject: s];
    }
  int n = [a count];
  if (! n) return;
  NSString *which = [a objectAtIndex: (random() % n)];

  SaverRunner *s = 
    (SaverRunner *) [[UIApplication sharedApplication] delegate];
  if (! s) return;
  NSAssert ([s isKindOfClass:[SaverRunner class]], @"not a SaverRunner");
  [self scrollTo: which];
  [s loadSaver: which];
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


#endif // HAVE_IPHONE -- whole file
