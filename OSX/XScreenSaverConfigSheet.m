/* xscreensaver, Copyright (c) 2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* XScreenSaver uses XML files to describe the user interface for configuring
   the various screen savers.  These files live in .../hacks/config/ and
   say relatively high level things like: "there should be a checkbox
   labelled "Leave Trails", and when it is checked, add the option '-trails'
   to the command line when launching the program."

   This code reads that XML and constructs a Cocoa interface from it.
   The Cocoa controls are hooked up to NSUserDefaultsController to save
   those settings into the MacOS preferences system.  The Cocoa preferences
   names are the same as the resource names specified in the screenhack's
   'options' array (we use that array to map the command line switches
   specified in the XML to the resource names to use).
 */

#import "XScreenSaverConfigSheet.h"

#import "jwxyz.h"
#import "InvertedSlider.h"
#import <Foundation/NSXMLDocument.h>

@implementation XScreenSaverConfigSheet

#define LEFT_MARGIN       20   // left edge of window
#define COLUMN_SPACING    10   // gap between e.g. labels and text fields
#define LEFT_LABEL_WIDTH  70   // width of all left labels
#define LINE_SPACING      10   // leading between each line

// redefine these since they don't work when not inside an ObjC method
#undef NSAssert
#undef NSAssert1
#undef NSAssert2
#undef NSAssert3
#define NSAssert(CC,S)        do { if (!(CC)) { NSLog(S);       }} while(0)
#define NSAssert1(CC,S,A)     do { if (!(CC)) { NSLog(S,A);     }} while(0)
#define NSAssert2(CC,S,A,B)   do { if (!(CC)) { NSLog(S,A,B);   }} while(0)
#define NSAssert3(CC,S,A,B,C) do { if (!(CC)) { NSLog(S,A,B,C); }} while(0)


/* Given a command-line option, returns the corresponding resource name.
   Any arguments in the switch string are ignored (e.g., "-foo x").
 */
static NSString *
switch_to_resource (NSString *cmdline_switch, const XrmOptionDescRec *opts,
                    NSString **val_ret)
{
  char buf[255];
  char *tail = 0;
  NSAssert(cmdline_switch, @"cmdline switch is null");
  if (! [cmdline_switch getCString:buf maxLength:sizeof(buf)
                          encoding:NSUTF8StringEncoding]) {
    NSAssert1(0, @"unable to convert %@", cmdline_switch);
    abort();
  }
  char *s = strpbrk(buf, " \t\r\n");
  if (s && *s) {
    *s = 0;
    tail = s+1;
    while (*tail && (*tail == ' ' || *tail == '\t'))
      tail++;
  }
  
  while (opts[0].option) {
    if (!strcmp (opts[0].option, buf)) {
      const char *ret = 0;

      if (opts[0].argKind == XrmoptionNoArg) {
        if (tail && *tail)
          NSAssert1 (0, @"expected no args to switch: \"%@\"",
                     cmdline_switch);
        ret = opts[0].value;
      } else {
        if (!tail || !*tail)
          NSAssert1 (0, @"expected args to switch: \"%@\"",
                     cmdline_switch);
        ret = tail;
      }

      if (val_ret)
        *val_ret = (ret
                    ? [NSString stringWithCString:ret
                                         encoding:NSUTF8StringEncoding]
                    : 0);
      
      const char *res = opts[0].specifier;
      while (*res && (*res == '.' || *res == '*'))
        res++;
      return [NSString stringWithCString:res
                                encoding:NSUTF8StringEncoding];
    }
    opts++;
  }
  
  NSAssert1 (0, @"\"%@\" not present in options", cmdline_switch);
  abort();
}


/* Connects a control (checkbox, etc) to the corresponding preferences key.
 */
static void
bind_resource_to_preferences (NSUserDefaultsController *prefs,
                              NSObject *control, 
                              NSString *pref_key,
                              const XrmOptionDescRec *opts)
{
  NSString *bindto = ([control isKindOfClass:[NSPopUpButton class]]
                      ? @"selectedObject"
                      : ([control isKindOfClass:[NSMatrix class]]
                         ? @"selectedIndex"
                         : @"value"));
  [control bind:bindto
       toObject:prefs
    withKeyPath:[@"values." stringByAppendingString: pref_key]
        options:nil];

# if 0 // ####
  NSObject *def = [[prefs defaults] objectForKey:pref_key];
  NSString *s = [NSString stringWithFormat:@"bind: \"%@\"", pref_key];
  s = [s stringByPaddingToLength:18 withString:@" " startingAtIndex:0];
  s = [NSString stringWithFormat:@"%@ = \"%@\"", s, def];
  s = [s stringByPaddingToLength:28 withString:@" " startingAtIndex:0];
  NSLog (@"%@ %@/%@", s, [def class], [control class]);
# endif
}

static void
bind_switch_to_preferences (NSUserDefaultsController *prefs,
                            NSObject *control, 
                            NSString *cmdline_switch,
                            const XrmOptionDescRec *opts)
{
  NSString *pref_key = switch_to_resource (cmdline_switch, opts, 0);
  bind_resource_to_preferences (prefs, control, pref_key, opts);
}


/* Parse the attributes of an XML tag into a dictionary.
   For input, the dictionary should have as attributes the keys, each
   with @"" as their value.
   On output, the dictionary will set the keys to the values specified,
   and keys that were not specified will not be present in the dictionary.
   Warnings are printed if there are duplicate or unknown attributes.
 */
static void
parse_attrs (NSMutableDictionary *dict, NSXMLNode *node)
{
  NSArray *attrs = [(NSXMLElement *) node attributes];
  int n = [attrs count];
  int i;
  
  // For each key in the dictionary, fill in the dict with the corresponding
  // value.  The value @"" is assumed to mean "un-set".  Issue a warning if
  // an attribute is specified twice.
  //
  for (i = 0; i < n; i++) {
    NSXMLNode *attr = [attrs objectAtIndex:i];
    NSString *key = [attr name];
    NSString *val = [attr objectValue];
    NSString *old = [dict objectForKey:key];
    
    if (! old) {
      NSAssert2 (0, @"unknown attribute \"%@\" in \"%@\"", key, [node name]);
    } else if ([old length] != 0) {
      NSAssert2 (0, @"duplicate %@: \"%@\", \"%@\"", old, val);
    } else {
      [dict setValue:val forKey:key];
    }
  }
  
  // Remove from the dictionary any keys whose value is still @"", 
  // meaning there was no such attribute specified.
  //
  NSArray *keys = [dict allKeys];
  n = [keys count];
  for (i = 0; i < n; i++) {
    NSString *key = [keys objectAtIndex:i];
    NSString *val = [dict objectForKey:key];
    if ([val length] == 0)
      [dict removeObjectForKey:key];
  }
}


/* Creates a label: an un-editable NSTextField displaying the given text.
 */
static NSTextField *
make_label (NSString *text)
{
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;
  NSTextField *lab = [[NSTextField alloc] initWithFrame:rect];
  [lab setSelectable:NO];
  [lab setEditable:NO];
  [lab setBezeled:NO];
  [lab setDrawsBackground:NO];
  [lab setStringValue:text];
  [lab sizeToFit];
  return lab;
}


static NSView *
last_child (NSView *parent)
{
  NSArray *kids = [parent subviews];
  int nkids = [kids count];
  if (nkids == 0)
    return 0;
  else
    return [kids objectAtIndex:nkids-1];
}


/* Add the child as a subview of the parent, positioning it immediately
   below or to the right of the previously-added child of that view.
 */
static void
place_child (NSView *parent, NSView *child, BOOL right_p)
{
  NSRect rect = [child frame];
  NSView *last = last_child (parent);
  if (!last) {
    rect.origin.x = LEFT_MARGIN;
    rect.origin.y = [parent frame].size.height - rect.size.height 
      - LINE_SPACING;
  } else if (right_p) {
    rect = [last frame];
    rect.origin.x += rect.size.width + COLUMN_SPACING;
  } else {
    rect = [last frame];
    rect.origin.x = LEFT_MARGIN;
    rect.origin.y -= [child frame].size.height + LINE_SPACING;
  }
  [child setFrameOrigin:rect.origin];
  [parent addSubview:child];
}


static void traverse_children (NSUserDefaultsController *,
                               const XrmOptionDescRec *, 
                               NSView *, NSXMLNode *);


/* Creates the checkbox (NSButton) described by the given XML node.
 */
static void
make_checkbox (NSUserDefaultsController *prefs,
               const XrmOptionDescRec *opts, NSView *parent, NSXMLNode *node)
{
  NSMutableDictionary *dict =
    [NSMutableDictionary dictionaryWithObjectsAndKeys:
      @"", @"id",
      @"", @"_label",
      @"", @"arg-set",
      @"", @"arg-unset",
      nil];
  parse_attrs (dict, node);
  NSString *label     = [dict objectForKey:@"_label"];
  NSString *arg_set   = [dict objectForKey:@"arg-set"];
  NSString *arg_unset = [dict objectForKey:@"arg-unset"];
  
  if (!label) {
    NSAssert1 (0, @"no _label in %@", [node name]);
    return;
  }
  if (!arg_set && !arg_unset) {
    NSAssert1 (0, @"neither arg-set nor arg-unset provided in \"%@\"", 
               label);
  }
  if (arg_set && arg_unset) {
    NSAssert1 (0, @"only one of arg-set and arg-unset may be used in \"%@\"", 
               label);
  }
  
  // sanity-check the choice of argument names.
  //
  if (arg_set && ([arg_set hasPrefix:@"-no-"] ||
                  [arg_set hasPrefix:@"--no-"]))
    NSLog (@"arg-set should not be a \"no\" option in \"%@\": %@",
           label, arg_set);
  if (arg_unset && (![arg_unset hasPrefix:@"-no-"] &&
                    ![arg_unset hasPrefix:@"--no-"]))
    NSLog(@"arg-unset should be a \"no\" option in \"%@\": %@",
          label, arg_unset);
    
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;

  NSButton *button = [[NSButton alloc] initWithFrame:rect];
  [button setButtonType:([[node name] isEqualToString:@"radio"]
                         ? NSRadioButton
                         : NSSwitchButton)];
  [button setTitle:label];
  [button sizeToFit];
  place_child (parent, button, NO);
  
  bind_switch_to_preferences (prefs, button,
                              (arg_set ? arg_set : arg_unset),
                              opts);
  [button release];
}


/* Creates the NSTextField described by the given XML node.
*/
static void
make_text_field (NSUserDefaultsController *prefs,
                 const XrmOptionDescRec *opts, NSView *parent, NSXMLNode *node,
                 BOOL no_label_p)
{
  NSMutableDictionary *dict =
  [NSMutableDictionary dictionaryWithObjectsAndKeys:
    @"", @"id",
    @"", @"_label",
    @"", @"arg",
    nil];
  parse_attrs (dict, node);
  NSString *label = [dict objectForKey:@"_label"];
  NSString *arg   = [dict objectForKey:@"arg"];

  if (!label && !no_label_p) {
    NSAssert1 (0, @"no _label in %@", [node name]);
    return;
  }

  NSAssert1 (arg, @"no arg in %@", label);

  NSRect rect;
  rect.origin.x = rect.origin.y = 0;    
  rect.size.width = rect.size.height = 10;
  
  NSTextField *txt = [[NSTextField alloc] initWithFrame:rect];

  // make the default size be around 30 columns; a typical value for
  // these text fields is "xscreensaver-text --cols 40".
  //
  [txt setStringValue:@"123456789 123456789 123456789 "];
  [txt sizeToFit];
  [[txt cell] setWraps:NO];
  [[txt cell] setScrollable:YES];
  [txt setStringValue:@""];
  
  if (label) {
    NSTextField *lab = make_label (label);
    place_child (parent, lab, NO);
    [lab release];
  }

  place_child (parent, txt, (label ? YES : NO));

  bind_switch_to_preferences (prefs, txt, arg, opts);
  [txt release];
}


/* Creates the NSTextField described by the given XML node,
   and hooks it up to a Choose button and a file selector widget.
*/
static void
make_file_selector (NSUserDefaultsController *prefs,
                    const XrmOptionDescRec *opts, 
                    NSView *parent, NSXMLNode *node,
                    BOOL dirs_only_p,
                    BOOL no_label_p)
{
  NSMutableDictionary *dict =
  [NSMutableDictionary dictionaryWithObjectsAndKeys:
    @"", @"id",
    @"", @"_label",
    @"", @"arg",
    nil];
  parse_attrs (dict, node);
  NSString *label = [dict objectForKey:@"_label"];
  NSString *arg   = [dict objectForKey:@"arg"];

  if (!label && !no_label_p) {
    NSAssert1 (0, @"no _label in %@", [node name]);
    return;
  }

  NSAssert1 (arg, @"no arg in %@", label);

  NSRect rect;
  rect.origin.x = rect.origin.y = 0;    
  rect.size.width = rect.size.height = 10;
  
  NSTextField *txt = [[NSTextField alloc] initWithFrame:rect];

  // make the default size be around 20 columns.
  //
  [txt setStringValue:@"123456789 123456789 "];
  [txt sizeToFit];
  [txt setSelectable:YES];
  [txt setEditable:NO];
  [txt setBezeled:NO];
  [txt setDrawsBackground:NO];
  [[txt cell] setWraps:NO];
  [[txt cell] setScrollable:YES];
  [[txt cell] setLineBreakMode:NSLineBreakByTruncatingHead];
  [txt setStringValue:@""];

  NSTextField *lab = 0;
  if (label) {
    lab = make_label (label);
    place_child (parent, lab, NO);
    [lab release];
  }

  place_child (parent, txt, (label ? YES : NO));

  bind_switch_to_preferences (prefs, txt, arg, opts);
  [txt release];

  // Make the text field be the same height as the label.
  if (lab) {
    rect = [txt frame];
    rect.size.height = [lab frame].size.height;
    [txt setFrame:rect];
  }

  // Now put a "Choose" button next to it.
  //
  rect.origin.x = rect.origin.y = 0;    
  rect.size.width = rect.size.height = 10;
  NSButton *choose = [[NSButton alloc] initWithFrame:rect];
  [choose setTitle:@"Choose..."];
  [choose setBezelStyle:NSRoundedBezelStyle];
  [choose sizeToFit];

  place_child (parent, choose, YES);

  // center the Choose button around the midpoint of the text field.
  rect = [choose frame];
  rect.origin.y = ([txt frame].origin.y + 
                   (([txt frame].size.height - rect.size.height) / 2));
  [choose setFrameOrigin:rect.origin];

  [choose setTarget:[parent window]];
  if (dirs_only_p)
    [choose setAction:@selector(chooseClickedDirs:)];
  else
    [choose setAction:@selector(chooseClicked:)];

  [choose release];
}


/* Runs a modal file selector and sets the text field's value to the
   selected file or directory.
 */
static void
do_file_selector (NSTextField *txt, BOOL dirs_p)
{
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseFiles:!dirs_p];
  [panel setCanChooseDirectories:dirs_p];

  NSString *file = [txt stringValue];
  if ([file length] <= 0) {
    file = NSHomeDirectory();
    if (dirs_p)
      file = [file stringByAppendingPathComponent:@"Pictures"];
  }

//  NSString *dir = [file stringByDeletingLastPathComponent];

  int result = [panel runModalForDirectory:file //dir
                                      file:nil //[file lastPathComponent]
                                     types:nil];
  if (result == NSOKButton) {
    NSArray *files = [panel filenames];
    NSString *file = ([files count] > 0 ? [files objectAtIndex:0] : @"");
    file = [file stringByAbbreviatingWithTildeInPath];
    [txt setStringValue:file];

    // Fuck me!  Just setting the value of the NSTextField does not cause
    // that to end up in the preferences!
    //
    NSDictionary *dict = [txt infoForBinding:@"value"];
    NSUserDefaultsController *prefs = [dict objectForKey:@"NSObservedObject"];
    NSString *path = [dict objectForKey:@"NSObservedKeyPath"];
    if ([path hasPrefix:@"values."])  // WTF.
      path = [path substringFromIndex:7];
    [[prefs values] setValue:file forKey:path];

#if 0
    // make sure the end of the string is visible.
    NSText *fe = [[txt window] fieldEditor:YES forObject:txt];
    NSRange range;
    range.location = [file length]-3;
    range.length = 1;
    if (! [[txt window] makeFirstResponder:[txt window]])
      [[txt window] endEditingFor:nil];
//    [[txt window] makeFirstResponder:nil];
    [fe setSelectedRange:range];
//    [tv scrollRangeToVisible:range];
//    [txt setNeedsDisplay:YES];
//    [[txt cell] setNeedsDisplay:YES];
//    [txt selectAll:txt];
#endif
  }
}

/* Returns the NSTextField that is to the left of or above the NSButton.
 */
static NSTextField *
find_text_field_of_button (NSButton *button)
{
  NSView *parent = [button superview];
  NSArray *kids = [parent subviews];
  int nkids = [kids count];
  int i;
  NSTextField *f = 0;
  for (i = 0; i < nkids; i++) {
    NSObject *kid = [kids objectAtIndex:i];
    if ([kid isKindOfClass:[NSTextField class]]) {
      f = (NSTextField *) kid;
    } else if (kid == button) {
      if (! f) abort();
      return f;
    }
  }
  abort();
}


- (void) chooseClicked:(NSObject *)arg
{
  NSButton *choose = (NSButton *) arg;
  NSTextField *txt = find_text_field_of_button (choose);
  do_file_selector (txt, NO);
}

- (void) chooseClickedDirs:(NSObject *)arg
{
  NSButton *choose = (NSButton *) arg;
  NSTextField *txt = find_text_field_of_button (choose);
  do_file_selector (txt, YES);
}


/* Creates the number selection control described by the given XML node.
   If "type=slider", it's an NSSlider.
   If "type=spinbutton", it's a text field with up/down arrows next to it.
*/
static void
make_number_selector (NSUserDefaultsController *prefs,
                      const XrmOptionDescRec *opts, 
                      NSView *parent, NSXMLNode *node)
{
  NSMutableDictionary *dict =
  [NSMutableDictionary dictionaryWithObjectsAndKeys:
    @"", @"id",
    @"", @"_label",
    @"", @"_low-label",
    @"", @"_high-label",
    @"", @"type",
    @"", @"arg",
    @"", @"low",
    @"", @"high",
    @"", @"default",
    @"", @"convert",
    nil];
  parse_attrs (dict, node);
  NSString *label      = [dict objectForKey:@"_label"];
  NSString *low_label  = [dict objectForKey:@"_low-label"];
  NSString *high_label = [dict objectForKey:@"_high-label"];
  NSString *type       = [dict objectForKey:@"type"];
  NSString *arg        = [dict objectForKey:@"arg"];
  NSString *low        = [dict objectForKey:@"low"];
  NSString *high       = [dict objectForKey:@"high"];
  NSString *def        = [dict objectForKey:@"default"];
  NSString *cvt        = [dict objectForKey:@"convert"];
  
  NSAssert1 (arg,  @"no arg in %@", label);
  NSAssert1 (type, @"no type in %@", label);

  if (! low) {
    NSAssert1 (0, @"no low in %@", [node name]);
    return;
  }
  if (! high) {
    NSAssert1 (0, @"no high in %@", [node name]);
    return;
  }
  if (! def) {
    NSAssert1 (0, @"no default in %@", [node name]);
    return;
  }
  if (cvt && ![cvt isEqualToString:@"invert"]) {
    NSAssert1 (0, @"if provided, \"convert\" must be \"invert\" in %@",
               label);
  }
    
  if ([type isEqualToString:@"slider"]) {

    NSRect rect;
    rect.origin.x = rect.origin.y = 0;    
    rect.size.width = 150;
    rect.size.height = 20;
    NSSlider *slider;
    if (cvt)
      slider = [[InvertedSlider alloc] initWithFrame:rect];
    else
      slider = [[NSSlider alloc] initWithFrame:rect];

    [slider setMaxValue:[high doubleValue]];
    [slider setMinValue:[low  doubleValue]];
    
    if (label) {
      NSTextField *lab = make_label (label);
      place_child (parent, lab, NO);
      [lab release];
    }
    
    if (low_label) {
      NSTextField *lab = make_label (low_label);
      [lab setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];      
      [lab setAlignment:1];  // right aligned
      rect = [lab frame];
      if (rect.size.width < LEFT_LABEL_WIDTH)
        rect.size.width = LEFT_LABEL_WIDTH;  // make all left labels same size
      rect.size.height = [slider frame].size.height;
      [lab setFrame:rect];
      place_child (parent, lab, NO);
      [lab release];
     }
    
    place_child (parent, slider, (low_label ? YES : NO));
    
    if (! low_label) {
      rect = [slider frame];
      if (rect.origin.x < LEFT_LABEL_WIDTH)
        rect.origin.x = LEFT_LABEL_WIDTH; // make unlabelled sliders line up too
      [slider setFrame:rect];
    }
        
    if (high_label) {
      NSTextField *lab = make_label (high_label);
      [lab setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];      
      rect = [lab frame];
      rect.size.height = [slider frame].size.height;
      [lab setFrame:rect];
      place_child (parent, lab, YES);
      [lab release];
     }

    bind_switch_to_preferences (prefs, slider, arg, opts);
    [slider release];
    
  } else if ([type isEqualToString:@"spinbutton"]) {

    if (! label) {
      NSAssert1 (0, @"no _label in spinbutton %@", [node name]);
      return;
    }
    NSAssert1 (!low_label,
              @"low-label not allowed in spinbutton \"%@\"", [node name]);
    NSAssert1 (!high_label,
               @"high-label not allowed in spinbutton \"%@\"", [node name]);
    NSAssert1 (!cvt, @"convert not allowed in spinbutton \"%@\"",
               [node name]);
    
    NSRect rect;
    rect.origin.x = rect.origin.y = 0;    
    rect.size.width = rect.size.height = 10;
    
    NSTextField *txt = [[NSTextField alloc] initWithFrame:rect];
    [txt setStringValue:@"0000.0"];
    [txt sizeToFit];
    [txt setStringValue:@""];
    
    if (label) {
      NSTextField *lab = make_label (label);
      //[lab setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];      
      [lab setAlignment:1];  // right aligned
      rect = [lab frame];
      if (rect.size.width < LEFT_LABEL_WIDTH)
        rect.size.width = LEFT_LABEL_WIDTH;  // make all left labels same size
      rect.size.height = [txt frame].size.height;
      [lab setFrame:rect];
      place_child (parent, lab, NO);
      [lab release];
     }
    
    place_child (parent, txt, (label ? YES : NO));
    
    if (! label) {
      rect = [txt frame];
      if (rect.origin.x < LEFT_LABEL_WIDTH)
        rect.origin.x = LEFT_LABEL_WIDTH; // make unlabelled spinbtns line up
      [txt setFrame:rect];
    }
    
    rect.size.width = rect.size.height = 10;
    NSStepper *step = [[NSStepper alloc] initWithFrame:rect];
    [step sizeToFit];
    place_child (parent, step, YES);
    rect = [step frame];
    rect.size.height = [txt frame].size.height;
    rect.origin.x -= COLUMN_SPACING;  // this one goes close
    [step setFrame:rect];
    
    [step setMinValue:[low  doubleValue]];
    [step setMaxValue:[high doubleValue]];
    [step setAutorepeat:YES];
    [step setValueWraps:NO];
    
    double range = [high doubleValue] - [low doubleValue];
    if (range < 1.0)
      [step setIncrement:range / 10.0];
    else if (range >= 500)
      [step setIncrement:range / 100.0];
    else
      [step setIncrement:1.0];

    bind_switch_to_preferences (prefs, step, arg, opts);
    bind_switch_to_preferences (prefs, txt,  arg, opts);
    
    [step release];
    [txt release];
    
  } else {
    NSAssert2 (0, @"unknown type \"%@\" in \"%@\"", type, label);
  }
}


static void
set_menu_item_object (NSMenuItem *item, NSObject *obj)
{
  /* If the object associated with this menu item looks like a boolean,
     store an NSNumber instead of an NSString, since that's what
     will be in the preferences (due to similar logic in PrefsReader).
   */
  if ([obj isKindOfClass:[NSString class]]) {
    NSString *string = (NSString *) obj;
    if (NSOrderedSame == [string caseInsensitiveCompare:@"true"] ||
        NSOrderedSame == [string caseInsensitiveCompare:@"yes"])
      obj = [NSNumber numberWithBool:YES];
    else if (NSOrderedSame == [string caseInsensitiveCompare:@"false"] ||
             NSOrderedSame == [string caseInsensitiveCompare:@"no"])
      obj = [NSNumber numberWithBool:NO];
    else
      obj = string;
  }

  [item setRepresentedObject:obj];
  //NSLog (@"menu item \"%@\" = \"%@\" %@", [item title], obj, [obj class]);
}


/* Creates the popup menu described by the given XML node (and its children).
*/
static void
make_option_menu (NSUserDefaultsController *prefs,
                  const XrmOptionDescRec *opts, 
                  NSView *parent, NSXMLNode *node)
{
  NSArray *children = [node children];
  int i, count = [children count];

  if (count <= 0) {
    NSAssert1 (0, @"no menu items in \"%@\"", [node name]);
    return;
  }

  // get the "id" attribute off the <select> tag.
  //
  NSMutableDictionary *dict =
    [NSMutableDictionary dictionaryWithObjectsAndKeys:
      @"", @"id",
      nil];
  parse_attrs (dict, node);
  
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = 10;
  rect.size.height = 10;
  NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:rect
                                                     pullsDown:NO];

  NSMenuItem *def_item = nil;
  float max_width = 0;
  
  NSString *menu_key = nil;   // the resource key used by items in this menu
  
  for (i = 0; i < count; i++) {
    NSXMLNode *child = [children objectAtIndex:i];

    if ([child kind] == NSXMLCommentKind)
      continue;
    if ([child kind] != NSXMLElementKind) {
      NSAssert2 (0, @"weird XML node kind: %d: %@", [child kind], node);
      continue;
    }

    // get the "id", "_label", and "arg-set" attrs off of the <option> tags.
    //
    NSMutableDictionary *dict2 =
      [NSMutableDictionary dictionaryWithObjectsAndKeys:
        @"", @"id",
        @"", @"_label",
        @"", @"arg-set",
        nil];
    parse_attrs (dict2, child);
    NSString *label   = [dict2 objectForKey:@"_label"];
    NSString *arg_set = [dict2 objectForKey:@"arg-set"];
    
    if (!label) {
      NSAssert1 (0, @"no _label in %@", [child name]);
      return;
    }

    // create the menu item (and then get a pointer to it)
    [popup addItemWithTitle:label];
    NSMenuItem *item = [popup itemWithTitle:label];

    if (arg_set) {
      NSString *this_val = NULL;
      NSString *this_key = switch_to_resource (arg_set, opts, &this_val);
      NSAssert1 (this_val, @"this_val null for %@", arg_set);
      if (menu_key && ![menu_key isEqualToString:this_key])
        NSAssert3 (0,
                   @"multiple resources in menu: \"%@\" vs \"%@\" = \"%@\"",
                   menu_key, this_key, this_val);
      if (this_key)
        menu_key = this_key;

      /* If this menu has the cmd line "-mode foo" then set this item's
         value to "foo" (the menu itself will be bound to e.g. "modeString")
       */
      set_menu_item_object (item, this_val);

    } else {
      // no arg-set -- only one menu item can be missing that.
      NSAssert1 (!def_item, @"no arg-set in \"%@\"", label);
      def_item = item;
    }

    /* make sure the menu button has room for the text of this item,
       and remember the greatest width it has reached.
     */
    [popup setTitle:label];
    [popup sizeToFit];
    NSRect r = [popup frame];
    if (r.size.width > max_width) max_width = r.size.width;
  }
  
  if (!menu_key) {
    NSAssert1 (0, @"no switches in menu \"%@\"", [dict objectForKey:@"id"]);
    abort();
  }

  /* We've added all of the menu items.  If there was an item with no
     command-line switch, then it's the item that represents the default
     value.  Now we must bind to that item as well...  (We have to bind
     this one late, because if it was the first item, then we didn't
     yet know what resource was associated with this menu.)
   */
  if (def_item) {
    NSDictionary *defs = [prefs initialValues];
    NSObject *def_obj = [defs objectForKey:menu_key];

    NSAssert2 (def_obj, 
               @"no default value for resource \"%@\" in menu item \"%@\"",
               menu_key, [def_item title]);

    set_menu_item_object (def_item, def_obj);
  }

  /* Finish tweaking the menu button itself.
   */
  if (def_item)
    [popup setTitle:[def_item title]];
  NSRect r = [popup frame];
  r.size.width = max_width;
  [popup setFrame:r];
  place_child (parent, popup, NO);

  bind_resource_to_preferences (prefs, popup, menu_key, opts);
  [popup release];
}


static NSString *unwrap (NSString *);
static void hreffify (NSText *);
static void boldify (NSText *);

/* Creates an uneditable, wrapping NSTextField to display the given
   text enclosed by <description> ... </description> in the XML.
 */
static void
make_desc_label (NSView *parent, NSXMLNode *node)
{
  NSString *text = nil;
  NSArray *children = [node children];
  int i, count = [children count];

  for (i = 0; i < count; i++) {
    NSXMLNode *child = [children objectAtIndex:i];
    NSString *s = [child objectValue];
    if (text)
      text = [text stringByAppendingString:s];
    else
      text = s;
  }
  
  text = unwrap (text);
  
  NSRect rect = [parent frame];
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = 200;
  rect.size.height = 50;  // sized later
  NSText *lab = [[NSText alloc] initWithFrame:rect];
  [lab setEditable:NO];
  [lab setDrawsBackground:NO];
  [lab setHorizontallyResizable:YES];
  [lab setVerticallyResizable:YES];
  [lab setString:text];
  hreffify (lab);
  boldify (lab);
  [lab sizeToFit];

  place_child (parent, lab, NO);
  [lab release];
}

static NSString *
unwrap (NSString *text)
{
  // Unwrap lines: delete \n but do not delete \n\n.
  //
  NSArray *lines = [text componentsSeparatedByString:@"\n"];
  int nlines = [lines count];
  BOOL eolp = YES;
  int i;

  text = @"\n";      // start with one blank line

  // skip trailing blank lines in file
  for (i = nlines-1; i > 0; i--) {
    NSString *s = (NSString *) [lines objectAtIndex:i];
    if ([s length] > 0)
      break;
    nlines--;
  }

  // skip leading blank lines in file
  for (i = 0; i < nlines; i++) {
    NSString *s = (NSString *) [lines objectAtIndex:i];
    if ([s length] > 0)
      break;
  }

  // unwrap
  Bool any = NO;
  for (; i < nlines; i++) {
    NSString *s = (NSString *) [lines objectAtIndex:i];
    if ([s length] == 0) {
      text = [text stringByAppendingString:@"\n\n"];
      eolp = YES;
    } else if ([s characterAtIndex:0] == ' ' ||
               [s hasPrefix:@"Copyright "] ||
               [s hasPrefix:@"http://"]) {
      // don't unwrap if the following line begins with whitespace,
      // or with the word "Copyright", or if it begins with a URL.
      if (any && !eolp)
        text = [text stringByAppendingString:@"\n"];
      text = [text stringByAppendingString:s];
      any = YES;
      eolp = NO;
    } else {
      if (!eolp)
        text = [text stringByAppendingString:@" "];
      text = [text stringByAppendingString:s];
      eolp = NO;
      any = YES;
    }
  }

  return text;
}


/* Converts any http: URLs in the given text field to clickable links.
 */
static void
hreffify (NSText *nstext)
{
  NSString *text = [nstext string];
  [nstext setRichText:YES];

  int L = [text length];
  NSRange start;		// range is start-of-search to end-of-string
  start.location = 0;
  start.length = L;
  while (start.location < L) {

    // Find the beginning of a URL...
    //
    NSRange r2 = [text rangeOfString:@"http://" options:0 range:start];
    if (r2.location == NSNotFound)
      break;

    // Next time around, start searching after this.
    start.location = r2.location + r2.length;
    start.length = L - start.location;

    // Find the end of a URL (whitespace or EOF)...
    //
    NSRange r3 = [text rangeOfCharacterFromSet:
                         [NSCharacterSet whitespaceAndNewlineCharacterSet]
                       options:0 range:start];
    if (r3.location == NSNotFound)    // EOF
      r3.location = L, r3.length = 0;

    // Next time around, start searching after this.
    start.location = r3.location;
    start.length = L - start.location;

    // Set r2 to the start/length of this URL.
    r2.length = start.location - r2.location;

    // Extract the URL.
    NSString *nsurl = [text substringWithRange:r2];
    const char *url = [nsurl UTF8String];

    // Construct the RTF corresponding to <A HREF="url">url</A>
    //
    const char *fmt = "{\\field{\\*\\fldinst{HYPERLINK \"%s\"}}%s}";
    char *rtf = malloc (strlen (fmt) + (strlen (url) * 2) + 10);
    sprintf (rtf, fmt, url, url);
    NSData *rtfdata = [NSData dataWithBytesNoCopy:rtf length:strlen(rtf)];

    // Insert the RTF into the NSText.
    [nstext replaceCharactersInRange:r2 withRTF:rtfdata];
  }
}

/* Makes the first word of the text be bold.
 */
static void
boldify (NSText *nstext)
{
  NSString *text = [nstext string];
  NSRange r = [text rangeOfCharacterFromSet:
                      [NSCharacterSet whitespaceCharacterSet]];
  r.length = r.location;
  r.location = 0;

  NSFont *font = [nstext font];
  font = [NSFont boldSystemFontOfSize:[font pointSize]];
  [nstext setFont:font range:r];
}


static void layout_group (NSView *group, BOOL horiz_p);


/* Creates an invisible NSBox (for layout purposes) to enclose the widgets
   wrapped in <hgroup> or <vgroup> in the XML.
 */
static void
make_group (NSUserDefaultsController *prefs,
            const XrmOptionDescRec *opts, NSView *parent, NSXMLNode *node, 
            BOOL horiz_p)
{
  NSRect rect;
  rect.size.width = rect.size.height = 1;
  rect.origin.x = rect.origin.y = 0;
  NSView *group = [[NSView alloc] initWithFrame:rect];
  traverse_children (prefs, opts, group, node);

  layout_group (group, horiz_p);

  rect.size.width = rect.size.height = 0;
  NSBox *box = [[NSBox alloc] initWithFrame:rect];
  [box setTitlePosition:NSNoTitle];
  [box setBorderType:NSNoBorder];
  [box setContentViewMargins:rect.size];
  [box setContentView:group];
  [box sizeToFit];

  place_child (parent, box, NO);
}


static void
layout_group (NSView *group, BOOL horiz_p)
{
  NSArray *kids = [group subviews];
  int nkids = [kids count];
  int i;
  double maxx = 0, miny = 0;
  for (i = 0; i < nkids; i++) {
    NSView *kid = [kids objectAtIndex:i];
    NSRect r = [kid frame];
    
    if (horiz_p) {
      maxx += r.size.width + COLUMN_SPACING;
      if (r.size.height > -miny) miny = -r.size.height;
    } else {
      if (r.size.width > maxx)  maxx = r.size.width;
      miny = r.origin.y - r.size.height;
    }
  }
  
  NSRect rect;
  rect.size.width = maxx;
  rect.size.height = -miny;
  [group setFrame:rect];

  double x = 0;
  for (i = 0; i < nkids; i++) {
    NSView *kid = [kids objectAtIndex:i];
    NSRect r = [kid frame];
    if (horiz_p) {
      r.origin.y = rect.size.height - r.size.height;
      r.origin.x = x;
      x += r.size.width + COLUMN_SPACING;
    } else {
      r.origin.y -= miny;
    }
    [kid setFrame:r];
  }
}


static void
make_text_controls (NSUserDefaultsController *prefs,
                    const XrmOptionDescRec *opts, 
                    NSView *parent, NSXMLNode *node)
{
  /*
    Display Text:
     (x)  Computer Name and Time
     ( )  Text       [__________________________]
     ( )  Text file  [_________________] [Choose]
     ( )  URL        [__________________________]

    textMode -text-mode date
    textMode -text-mode literal   textLiteral -text-literal %
    textMode -text-mode file      textFile    -text-file %
    textMode -text-mode url       textURL     -text-url %
   */
  NSRect rect;
  rect.size.width = rect.size.height = 1;
  rect.origin.x = rect.origin.y = 0;
  NSView *group = [[NSView alloc] initWithFrame:rect];
  NSView *rgroup = [[NSView alloc] initWithFrame:rect];


  NSXMLElement *node2;
  NSView *control;

  // This is how you link radio buttons together.
  //
  NSButtonCell *proto = [[NSButtonCell alloc] init];
  [proto setButtonType:NSRadioButton];

  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;
  NSMatrix *matrix = [[NSMatrix alloc] 
                       initWithFrame:rect
                       mode:NSRadioModeMatrix
                       prototype:proto
                       numberOfRows:4
                       numberOfColumns:1];
  [matrix setAllowsEmptySelection:NO];

  NSArrayController *cnames  = [[NSArrayController alloc] initWithContent:nil];
  [cnames addObject:@"Computer Name and Time"];
  [cnames addObject:@"Text"];
  [cnames addObject:@"File"];
  [cnames addObject:@"URL"];
  [matrix bind:@"content"
          toObject:cnames
          withKeyPath:@"arrangedObjects"
          options:nil];
  [cnames release];

  bind_switch_to_preferences (prefs, matrix, @"-text-mode %", opts);

  place_child (group, matrix, NO);
  place_child (group, rgroup, YES);

  //  <string id="textLiteral" _label="" arg-set="-text-literal %"/>
  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
          [NSDictionary dictionaryWithObjectsAndKeys:
                        @"textLiteral",        @"id",
                        @"-text-literal %",    @"arg",
                        nil]];
  make_text_field (prefs, opts, rgroup, node2, YES);
  [node2 release];

  rect = [last_child(rgroup) frame];

/* // trying to make the text fields be enabled only when the checkbox is on..
  control = last_child (rgroup);
  [control bind:@"enabled"
           toObject:[matrix cellAtRow:1 column:0]
           withKeyPath:@"value"
           options:nil];
*/


  //  <file id="textFile" _label="" arg-set="-text-file %"/>
  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
          [NSDictionary dictionaryWithObjectsAndKeys:
                        @"textFile",           @"id",
                        @"-text-file %",       @"arg",
                        nil]];
  make_file_selector (prefs, opts, rgroup, node2, NO, YES);
  [node2 release];

  rect = [last_child(rgroup) frame];

  //  <string id="textURL" _label="" arg-set="text-url %"/>
  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
          [NSDictionary dictionaryWithObjectsAndKeys:
                        @"textURL",            @"id",
                        @"-text-url %",        @"arg",
                        nil]];
  make_text_field (prefs, opts, rgroup, node2, YES);
  [node2 release];

  rect = [last_child(rgroup) frame];

  layout_group (rgroup, NO);

  rect = [rgroup frame];
  rect.size.width += 35;    // WTF?  Why is rgroup too narrow?
  [rgroup setFrame:rect];


  // Set the height of the cells in the radio-box matrix to the height of
  // the (last of the) text fields.
  control = last_child (rgroup);
  rect = [control frame];
  rect.size.width = 30;  // width of the string "Text", plus a bit...
  rect.size.height += LINE_SPACING;
  [matrix setCellSize:rect.size];
  [matrix sizeToCells];

  layout_group (group, YES);
  rect = [matrix frame];
  rect.origin.x += rect.size.width + COLUMN_SPACING;
  rect.origin.y -= [control frame].size.height - LINE_SPACING;
  [rgroup setFrameOrigin:rect.origin];

  // now cheat on the size of the matrix: allow it to overlap (underlap)
  // the text fields.
  // 
  rect.size = [matrix cellSize];
  rect.size.width *= 10;
  [matrix setCellSize:rect.size];
  [matrix sizeToCells];

  // Cheat on the position of the stuff on the right (the rgroup).
  // GAAAH, this code is such crap!
  rect = [rgroup frame];
  rect.origin.y -= 5;
  [rgroup setFrame:rect];


  rect.size.width = rect.size.height = 0;
  NSBox *box = [[NSBox alloc] initWithFrame:rect];
  [box setTitlePosition:NSAtTop];
  [box setBorderType:NSBezelBorder];
  [box setTitle:@"Display Text"];

  rect.size.width = rect.size.height = 12;
  [box setContentViewMargins:rect.size];
  [box setContentView:group];
  [box sizeToFit];

  place_child (parent, box, NO);
}


static void
make_image_controls (NSUserDefaultsController *prefs,
                     const XrmOptionDescRec *opts, 
                     NSView *parent, NSXMLNode *node)
{
  /*
    [x]  Grab Desktop Images
    [ ]  Choose Random Image:
         [__________________________]  [Choose]

   <boolean id="grabDesktopImages" _label="Grab Desktop Images"
       arg-unset="-no-grab-desktop"/>
   <boolean id="chooseRandomImages" _label="Grab Desktop Images"
       arg-unset="-choose-random-images"/>
   <file id="imageDirectory" _label="" arg-set="-image-directory %"/>
   */

  NSXMLElement *node2;

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
          [NSDictionary dictionaryWithObjectsAndKeys:
                        @"grabDesktopImages",   @"id",
                        @"Grab Desktop Images", @"_label",
                        @"-no-grab-desktop",    @"arg-unset",
                        nil]];
  make_checkbox (prefs, opts, parent, node2);
  [node2 release];

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
          [NSDictionary dictionaryWithObjectsAndKeys:
                        @"chooseRandomImages",    @"id",
                        @"Choose Random Images",  @"_label",
                        @"-choose-random-images", @"arg-set",
                        nil]];
  make_checkbox (prefs, opts, parent, node2);
  [node2 release];

  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
          [NSDictionary dictionaryWithObjectsAndKeys:
                        @"imageDirectory",     @"id",
                        @"Images Directory:",  @"_label",
                        @"-image-directory %", @"arg",
                        nil]];
  make_file_selector (prefs, opts, parent, node2, YES, NO);
  [node2 release];
}



/* Create some kind of control corresponding to the given XML node.
 */
static void
make_control (NSUserDefaultsController *prefs,
              const XrmOptionDescRec *opts, NSView *parent, NSXMLNode *node)
{
  NSString *name = [node name];

  if ([node kind] == NSXMLCommentKind)
    return;
  if ([node kind] != NSXMLElementKind) {
    NSAssert2 (0, @"weird XML node kind: %d: %@", [node kind], node);
    return;
  }

  if ([name isEqualToString:@"hgroup"] ||
      [name isEqualToString:@"vgroup"]) {

    BOOL horiz_p = [name isEqualToString:@"hgroup"];
    make_group (prefs, opts, parent, node, horiz_p);

  } else if ([name isEqualToString:@"command"]) {
    // do nothing: this is the "-root" business

  } else if ([name isEqualToString:@"boolean"]) {
    make_checkbox (prefs, opts, parent, node);

  } else if ([name isEqualToString:@"string"]) {
    make_text_field (prefs, opts, parent, node, NO);

  } else if ([name isEqualToString:@"file"]) {
    make_file_selector (prefs, opts, parent, node, NO, NO);

  } else if ([name isEqualToString:@"number"]) {
    make_number_selector (prefs, opts, parent, node);

  } else if ([name isEqualToString:@"select"]) {
    make_option_menu (prefs, opts, parent, node);

  } else if ([name isEqualToString:@"_description"]) {
    make_desc_label (parent, node);

  } else if ([name isEqualToString:@"xscreensaver-text"]) {
    make_text_controls (prefs, opts, parent, node);

  } else if ([name isEqualToString:@"xscreensaver-image"]) {
    make_image_controls (prefs, opts, parent, node);

  } else {
    NSAssert1 (0, @"unknown tag: %@", name);
  }
}


/* Iterate over and process the children of this XML node.
 */
static void
traverse_children (NSUserDefaultsController *prefs,
                   const XrmOptionDescRec *opts,
                   NSView *parent, NSXMLNode *node)
{
  NSArray *children = [node children];
  int i, count = [children count];
  for (i = 0; i < count; i++) {
    NSXMLNode *child = [children objectAtIndex:i];
    make_control (prefs, opts, parent, child);
  }
}

/* Handle the options on the top level <xscreensaver> tag.
 */
static void
parse_xscreensaver_tag (NSXMLNode *node)
{
  NSMutableDictionary *dict =
  [NSMutableDictionary dictionaryWithObjectsAndKeys:
    @"", @"name",
    @"", @"_label",
    nil];
  parse_attrs (dict, node);
  NSString *name  = [dict objectForKey:@"name"];
  NSString *label = [dict objectForKey:@"_label"];
    
  if (!label) {
    NSAssert1 (0, @"no _label in %@", [node name]);
    return;
  }
  if (!name) {
    NSAssert1 (0, @"no name in \"%@\"", label);
    return;
  }
  
  // #### do any callers need the "name" field for anything?
}


/* Kludgey magic to make the window enclose the controls we created.
 */
static void
fix_contentview_size (NSView *parent)
{
  NSRect f;
  NSArray *kids = [parent subviews];
  int nkids = [kids count];
  NSView *text;  // the NSText at the bottom of the window
  NSView *last;  // the last child before the NSText
  double maxx = 0, miny = 0;
  int i;

  /* Find the size of the rectangle taken up by each of the children
     except the final "NSText" child.
  */
  for (i = 0; i < nkids; i++) {
    NSView *kid = [kids objectAtIndex:i];
    if ([kid isKindOfClass:[NSText class]]) {
      text = kid;
      continue;
    }
    f = [kid frame];
    if (f.origin.x + f.size.width > maxx)  maxx = f.origin.x + f.size.width;
    if (f.origin.y - f.size.height < miny) miny = f.origin.y;
    last = kid;
//    NSLog(@"start: %3.0f x %3.0f @ %3.0f %3.0f  %3.0f  %@",
//          f.size.width, f.size.height, f.origin.x, f.origin.y,
//          f.origin.y + f.size.height, [kid class]);
  }
  
  if (maxx < 350) maxx = 350;   // leave room for the NSText paragraph...
  
  /* Now that we know the width of the window, set the width of the NSText to
     that, so that it can decide what its height needs to be.
   */
  f = [text frame];
//  NSLog(@"text old: %3.0f x %3.0f @ %3.0f %3.0f  %3.0f  %@",
//        f.size.width, f.size.height, f.origin.x, f.origin.y,
//        f.origin.y + f.size.height, [text class]);
  
  // set the NSText's width (this changes its height).
  f.size.width = maxx - LEFT_MARGIN;
  [text setFrame:f];
  
  // position the NSText below the last child (this gives us a new miny).
  f = [text frame];
  f.origin.y = miny - f.size.height - LINE_SPACING;
  miny = f.origin.y - LINE_SPACING;
  [text setFrame:f];
  
  // Stop second-guessing us on sizing now.  Size is now locked.
  [(NSText *) text setHorizontallyResizable:NO];
  [(NSText *) text setVerticallyResizable:NO];
  
//  NSLog(@"text new: %3.0f x %3.0f @ %3.0f %3.0f  %3.0f  %@",
//        f.size.width, f.size.height, f.origin.x, f.origin.y,
//        f.origin.y + f.size.height, [text class]);
  
  
  /* Set the contentView to the size of the children.
   */
  f = [parent frame];
  float yoff = f.size.height;
  f.size.width = maxx + LEFT_MARGIN;
  f.size.height = -(miny - LEFT_MARGIN*2);
  yoff = f.size.height - yoff;
  [parent setFrame:f];

//  NSLog(@"max: %3.0f x %3.0f @ %3.0f %3.0f", 
//        f.size.width, f.size.height, f.origin.x, f.origin.y);

  /* Now move all of the kids up into the window.
   */
  f = [parent frame];
  float shift = f.size.height;
//  NSLog(@"shift: %3.0f", shift);
  for (i = 0; i < nkids; i++) {
    NSView *kid = [kids objectAtIndex:i];
    f = [kid frame];
    f.origin.y += shift;
    [kid setFrame:f];
//    NSLog(@"move: %3.0f x %3.0f @ %3.0f %3.0f  %3.0f  %@",
//          f.size.width, f.size.height, f.origin.x, f.origin.y,
//          f.origin.y + f.size.height, [kid class]);
  }
  
  /* Set the kids to track the top left corner of the window when resized.
     Set the NSText to track the bottom right corner as well.
   */
  for (i = 0; i < nkids; i++) {
    NSView *kid = [kids objectAtIndex:i];
    unsigned long mask = NSViewMaxXMargin | NSViewMinYMargin;
    if ([kid isKindOfClass:[NSText class]])
      mask |= NSViewWidthSizable|NSViewHeightSizable;
    [kid setAutoresizingMask:mask];
  }
}


- (void) okClicked:(NSObject *)arg
{
  [userDefaultsController commitEditing];
  [userDefaultsController save:self];
  [NSApp endSheet:self returnCode:NSOKButton];
  [self close];
}

- (void) cancelClicked:(NSObject *)arg
{
  [userDefaultsController revert:self];
  [NSApp endSheet:self returnCode:NSCancelButton];
  [self close];
}

- (void) resetClicked:(NSObject *)arg
{
  [userDefaultsController revertToInitialValues:self];
}


static NSView *
wrap_with_buttons (NSWindow *window, NSView *panel)
{
  NSRect rect;
  
  // Make a box to hold the buttons at the bottom of the window.
  //
  rect = [panel frame];
  rect.origin.x = rect.origin.y = 0;
  rect.size.height = 10;
  NSBox *bbox = [[NSBox alloc] initWithFrame:rect];
  [bbox setTitlePosition:NSNoTitle];  
  [bbox setBorderType:NSNoBorder];
  
  // Make some buttons: Default, Cancel, OK
  //
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;
  NSButton *reset = [[NSButton alloc] initWithFrame:rect];
  [reset setTitle:@"Reset to Defaults"];
  [reset setBezelStyle:NSRoundedBezelStyle];
  [reset sizeToFit];

  rect = [reset frame];
  NSButton *ok = [[NSButton alloc] initWithFrame:rect];
  [ok setTitle:@"OK"];
  [ok setBezelStyle:NSRoundedBezelStyle];
  [ok sizeToFit];
  rect = [bbox frame];
  rect.origin.x = rect.size.width - [ok frame].size.width;
  [ok setFrameOrigin:rect.origin];

  rect = [ok frame];
  NSButton *cancel = [[NSButton alloc] initWithFrame:rect];
  [cancel setTitle:@"Cancel"];
  [cancel setBezelStyle:NSRoundedBezelStyle];
  [cancel sizeToFit];
  rect.origin.x -= [cancel frame].size.width + 10;
  [cancel setFrameOrigin:rect.origin];

  // Bind OK to RET and Cancel to ESC.
  [ok     setKeyEquivalent:@"\r"];
  [cancel setKeyEquivalent:@"\e"];

  // The correct width for OK and Cancel buttons is 68 pixels
  // ("Human Interface Guidelines: Controls: Buttons: 
  // Push Button Specifications").
  //
  rect = [ok frame];
  rect.size.width = 68;
  [ok setFrame:rect];

  rect = [cancel frame];
  rect.size.width = 68;
  [cancel setFrame:rect];

  // It puts the buttons in the box or else it gets the hose again
  //
  [bbox addSubview:ok];
  [bbox addSubview:cancel];
  [bbox addSubview:reset];
  [bbox sizeToFit];
  
  // make a box to hold the button-box, and the preferences view
  //
  rect = [bbox frame];
  rect.origin.y += rect.size.height;
  NSBox *pbox = [[NSBox alloc] initWithFrame:rect];
  [pbox setTitlePosition:NSNoTitle];
  [pbox setBorderType:NSNoBorder];
  [pbox addSubview:panel];
  [pbox addSubview:bbox];
  [pbox sizeToFit];

  [reset  setAutoresizingMask:NSViewMaxXMargin];
  [cancel setAutoresizingMask:NSViewMinXMargin];
  [ok     setAutoresizingMask:NSViewMinXMargin];
  [bbox   setAutoresizingMask:NSViewWidthSizable];
  
  // grab the clicks
  //
  [ok     setTarget:window];
  [cancel setTarget:window];
  [reset  setTarget:window];
  [ok     setAction:@selector(okClicked:)];
  [cancel setAction:@selector(cancelClicked:)];
  [reset  setAction:@selector(resetClicked:)];
  
  return pbox;
}


/* Iterate over and process the children of the root node of the XML document.
 */
static void
traverse_tree (NSUserDefaultsController *prefs,
               NSWindow *window, const XrmOptionDescRec *opts, NSXMLNode *node)
{
  if (![[node name] isEqualToString:@"screensaver"]) {
    NSAssert (0, @"top level node is not <xscreensaver>");
  }

  parse_xscreensaver_tag (node);
  
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 1;

  NSView *panel = [[NSView alloc] initWithFrame:rect];
  
  traverse_children (prefs, opts, panel, node);
  fix_contentview_size (panel);

  NSView *root = wrap_with_buttons (window, panel);
  [prefs setAppliesImmediately:NO];

  [panel setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];

  rect = [window frameRectForContentRect:[root frame]];
  [window setFrame:rect display:NO];
  [window setMinSize:rect.size];
  
  [window setContentView:root];
}


/* When this object is instantiated, it parses the XML file and creates
   controls on itself that are hooked up to the appropriate preferences.
   The default size of the view is just big enough to hold them all.
 */
- (id)initWithXMLFile: (NSString *) xml_file
              options: (const XrmOptionDescRec *) opts
           controller: (NSUserDefaultsController *) prefs
{
  if (! (self = [super init]))
    return 0;

  // instance variable
  userDefaultsController = prefs;
  [prefs retain];

  NSURL *furl = [NSURL fileURLWithPath:xml_file];

  if (!furl) {
    NSAssert1 (0, @"can't URLify \"%@\"", xml_file);
    return nil;
  }

  NSError *err = nil;
  NSXMLDocument *xmlDoc = [[NSXMLDocument alloc] 
                            initWithContentsOfURL:furl
                            options:(NSXMLNodePreserveWhitespace |
                                     NSXMLNodePreserveCDATA)
                            error:&err];
/* clean up?
    if (!xmlDoc) {
      xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL:furl
                                      options:NSXMLDocumentTidyXML
                                      error:&err];
    }
*/
  if (!xmlDoc || err) {
    if (err)
      NSAssert2 (0, @"XML Error: %@: %@",
                 xml_file, [err localizedDescription]);
    return nil;
  }

  traverse_tree (prefs, self, opts, [xmlDoc rootElement]);

  return self;
}


- (void) dealloc
{
  [userDefaultsController release];
  [super dealloc];
}

@end
