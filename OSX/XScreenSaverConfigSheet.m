/* xscreensaver, Copyright © 2006-2025 Jamie Zawinski <jwz@jwz.org>
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
#import "Updater.h"

#import "jwxyz.h"
#import "InvertedSlider.h"
#import "nslog.h"

#ifdef HAVE_IPHONE
# define NSView      UIView
# define NSRect      CGRect
# define NSSize      CGSize
# define NSTextField UITextField
# define NSButton    UIButton
# define NSFont      UIFont
# define NSStepper   UIStepper
# define NSMenuItem  UIMenuItem
# define NSText      UILabel
# define minValue    minimumValue
# define maxValue    maximumValue
# define setMinValue setMinimumValue
# define setMaxValue setMaximumValue
# define LABEL       UILabel
#else
# define LABEL       NSTextField
#endif // HAVE_IPHONE

# if defined(HAVE_IPHONE) && \
     !defined(HAVE_TVOS) && \
     (__IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_8_0)
#  define USE_WEBKIT
# endif

#ifdef USE_WEBKIT
# import <WebKit/WebKit.h>
# define UIWebView WKWebView
# define UIWebViewDelegate WKUIDelegate
#endif


#undef LABEL_ABOVE_SLIDER

#ifndef HAVE_TVOS
# define USE_HTML_LABELS
#endif


#pragma mark XML Parser

/* I used to use the "NSXMLDocument" XML parser, but that doesn't exist
   on iOS.  The "NSXMLParser" parser exists on both OSX and iOS, so I
   converted to use that.  However, to avoid having to re-write all of
   the old code, I faked out a halfassed implementation of the
   "NSXMLNode" class that "NSXMLDocument" used to return.
 */

#define NSXMLNode          SimpleXMLNode
#define NSXMLElement       SimpleXMLNode
#define NSXMLCommentKind   SimpleXMLCommentKind
#define NSXMLElementKind   SimpleXMLElementKind
#define NSXMLAttributeKind SimpleXMLAttributeKind
#define NSXMLTextKind      SimpleXMLTextKind

typedef enum { SimpleXMLCommentKind,
               SimpleXMLElementKind,
               SimpleXMLAttributeKind,
               SimpleXMLTextKind,
} SimpleXMLKind;

@interface SimpleXMLNode : NSObject
{
  SimpleXMLKind kind;
  NSString *name;
  SimpleXMLNode *parent;
  NSMutableArray *children;
  NSMutableArray *attributes;
  id object;
}

@property(nonatomic) SimpleXMLKind kind;
@property(nonatomic, retain) NSString *name;
@property(nonatomic, retain) SimpleXMLNode *parent;
@property(nonatomic, retain) NSMutableArray *children;
@property(nonatomic, retain) NSMutableArray *attributes;
@property(nonatomic, retain, getter=objectValue, setter=setObjectValue:)
  id object;

@end

@implementation SimpleXMLNode

@synthesize kind;
@synthesize name;
//@synthesize parent;
@synthesize children;
@synthesize attributes;
@synthesize object;

- (void) dealloc
{
  [name release];
  [children release];
  //[attributes release];
  [object release];
  [super dealloc];
}

- (id) init
{
  self = [super init];
  attributes = [[NSMutableArray alloc] initWithCapacity:10];
  return self;
}


- (id) initWithName:(NSString *)n
{
  self = [self init];
  [self setKind:NSXMLElementKind];
  [self setName:n];
  return self;
}


- (void) setAttributesAsDictionary:(NSDictionary *)dict
{
  for (NSString *key in dict) {
    NSObject *val = [dict objectForKey:key];
    SimpleXMLNode *n = [[SimpleXMLNode alloc] init];
    [n setKind:SimpleXMLAttributeKind];
    [n setName:key];
    [n setObjectValue:val];
    [attributes addObject:n];
    [n release];
  }
}

- (SimpleXMLNode *) parent { return parent; }

- (void) setParent:(SimpleXMLNode *)p
{
  NSAssert (!parent, @"parent already set");
  if (!p) return;
  parent = p;
  NSMutableArray *kids = [p children];
  if (!kids) {
    kids = [NSMutableArray arrayWithCapacity:10];
    [p setChildren:kids];
  }
  [kids addObject:self];
}
@end


#pragma mark textMode value transformer

// A value transformer for mapping "url" to "3" and vice versa in the
// "textMode" preference, since NSMatrix uses NSInteger selectedIndex.

#ifndef HAVE_IPHONE
@interface TextModeTransformer: NSValueTransformer {}
@end
@implementation TextModeTransformer
+ (Class)transformedValueClass { return [NSString class]; }
+ (BOOL)allowsReverseTransformation { return YES; }

- (id)transformedValue:(id)value {
  if ([value isKindOfClass:[NSString class]]) {
    int i = -1;
    if      ([value isEqualToString:@"date"])    { i = 0; }
    else if ([value isEqualToString:@"literal"]) { i = 1; }
    else if ([value isEqualToString:@"file"])    { i = 2; }
    else if ([value isEqualToString:@"url"])     { i = 3; }
    else if ([value isEqualToString:@"program"]) { i = 4; }
    if (i != -1)
      value = [NSNumber numberWithInt: i];
  }
  return value;
}

- (id)reverseTransformedValue:(id)value {
  if ([value isKindOfClass:[NSNumber class]]) {
    switch ((int) [value doubleValue]) {
    case 0: value = @"date";    break;
    case 1: value = @"literal"; break;
    case 2: value = @"file";    break;
    case 3: value = @"url";     break;
    case 4: value = @"program"; break;
    }
  }
  return value;
}
@end

/* Current theory is that the @"value" KVO binding on NSTextFields are
   sometimes returning nil when they're empty, but meanwhile
   [NSUserDefaults setObject:forKey:] needs non-nil objects.
 */
@interface NonNilStringTransformer: NSValueTransformer {}
@end
@implementation NonNilStringTransformer
+ (Class)transformedValueClass { return [NSString class]; }
+ (BOOL)allowsReverseTransformation { return YES; }

- (id)transformedValue:(id)value {
  return value ? value : @"";
}

- (id)reverseTransformedValue:(id)value {
  return value ? value : @"";
}
@end
#endif // !HAVE_IPHONE


#pragma mark Implementing radio buttons

/* The UIPickerView is a hideous and uncustomizable piece of shit.
   I can't believe Apple actually released that thing on the world.
   Let's fake up some radio buttons instead.
 */

#if defined(HAVE_IPHONE) && !defined(USE_PICKER_VIEW)

@interface RadioButton : UILabel
{
  int index;
  NSArray *items;
}

@property(nonatomic) int index;
@property(nonatomic, retain) NSArray *items;

@end

@implementation RadioButton

@synthesize index;
@synthesize items;

- (void)dealloc
{
  [items release];
  [super dealloc];
}

- (id) initWithIndex:(int)_index items:_items
{
  self = [super initWithFrame:CGRectZero];
  index = _index;
  items = [_items retain];

  [self setText: [[items objectAtIndex:index] objectAtIndex:0]];
  [self setBackgroundColor:[UIColor clearColor]];
  [self sizeToFit];

  return self;
}

@end


# endif // !USE_PICKER_VIEW


# pragma mark Implementing labels with clickable links

#if defined(HAVE_IPHONE) && defined(USE_HTML_LABELS)


@interface HTMLLabel : UIView <UIWebViewDelegate
# ifdef USE_WEBKIT
                                 , WKNavigationDelegate
# endif
                              >
{
  NSString *html;
  UIFont *font;
  UIWebView *webView;
}

@property(nonatomic, retain) NSString *html;
@property(nonatomic, retain) UIWebView *webView;

- (id) initWithHTML:(NSString *)h font:(UIFont *)f;
- (id) initWithText:(NSString *)t font:(UIFont *)f;
- (void) setHTML:(NSString *)h;
- (void) setText:(NSString *)t;
- (void) sizeToFit;

@end

@implementation HTMLLabel

@synthesize html;
@synthesize webView;

- (id) initWithHTML:(NSString *)h font:(UIFont *)f
{
  self = [super init];
  if (! self) return 0;
  font = [f retain];
  webView = [[UIWebView alloc] init];
# ifdef USE_WEBKIT
  webView.UIDelegate = self;
  webView.navigationDelegate = self;
# else
  webView.delegate = self;
  webView.dataDetectorTypes = UIDataDetectorTypeNone;
# endif
  self.   autoresizingMask = UIViewAutoresizingNone;  // we do it manually
  webView.autoresizingMask = UIViewAutoresizingNone;
  webView.scrollView.scrollEnabled = NO; 
  webView.scrollView.bounces = NO;
  webView.opaque = NO;
  [webView setBackgroundColor:[UIColor clearColor]];

  [self addSubview: webView];
  [self setHTML: h];
  return self;
}

- (id) initWithText:(NSString *)t font:(UIFont *)f
{
  self = [self initWithHTML:@"" font:f];
  if (! self) return 0;
  [self setText: t];
  return self;
}


- (void) setHTML: (NSString *)h
{
  if (! h) return;
  [h retain];
  if (html) [html release];
  html = h;

  BOOL dark_mode_p = FALSE;
  {
    UITraitCollection *t = [self traitCollection];
#   pragma clang diagnostic push   // "only available on iOS 12.0 or newer"
#   pragma clang diagnostic ignored "-Wunguarded-availability-new"
    if (t && [t respondsToSelector:@selector(userInterfaceStyle)] &&
        [t userInterfaceStyle] == UIUserInterfaceStyleDark)
      dark_mode_p = TRUE;
#   pragma clang diagnostic pop
  }

  double pointsize = [font pointSize];
  double lineheight = [font lineHeight];
  NSString *h2 =
    [NSString stringWithFormat:
                @"<!DOCTYPE HTML PUBLIC "
                   "\"-//W3C//DTD HTML 4.01 Transitional//EN\""
                   " \"http://www.w3.org/TR/html4/loose.dtd\">"
                 "<HTML>"
                  "<HEAD>"
                   "<META NAME=\"viewport\" CONTENT=\""
                      "width=device-width, "
                      "initial-scale=1.0, "
                      "maximum-scale=1.0\" />"
                   "<STYLE TYPE=\"text/css\">"
                    "<!--\n"
                      "body {"
                      " margin: 0; padding: 0; border: 0;"
                      " font-family: \"%@\";"
                      " font-size: %.4fpx;"	// Must be "px", not "pt"!
                      " line-height: %.4fpx;"   // And no spaces before it.
                      " -webkit-text-size-adjust: none;"
                      " color: %@;"
                      "}"
                    " a { color: %@ !important; }"
                    "\n//-->\n"
                   "</STYLE>"
                  "</HEAD>"
                  "<BODY>"
                   "%@"
                  "</BODY>"
                 "</HTML>",
              // [font fontName],  // Returns ".SFUI-Regular", doesn't work.
              @"Helvetica", // "SanFranciscoDisplay-Regular" also doesn't work.
              pointsize,
              lineheight,
              (dark_mode_p ? @"#FFF" : @"#000"),
              (dark_mode_p ? @"#0DF" : @"#00E"),
              h];
  [webView stopLoading];
  [webView loadHTMLString:h2 baseURL:[NSURL URLWithString:@""]];

#if 0  // Attempt to compute the size of the HTML the right way
# ifdef USE_WEBKIT
  [webView addObserver: self
            forKeyPath: @"loading"
               options: NSKeyValueObservingOptionNew
               context: nil];
# endif // USE_WEBKIT
# endif
}


#ifdef USE_WEBKIT  // Attempt to compute the size of the HTML the right way
#if 0

- (void) observeValueForKeyPath: (NSString *) path
                       ofObject: (id) object
                         change: (NSDictionary<NSKeyValueChangeKey,id> *)change
                        context: (void *) context
{
  // Wait for the HTML to load in the DOM, then set the height of the view
  // to be the height of the document.

  if ([object isKindOfClass: [WKWebView class]] &&
      [path isEqualToString: @"loading"] &&
      ! [change[NSKeyValueChangeNewKey] boolValue]) {  // loading == 0
    WKWebView *view = (WKWebView *) object;
    [view evaluateJavaScript: @"document.body.offsetHeight"
           completionHandler: ^(id response, NSError *error) {

        // #### This doesn't work: Munch gets clipped, Molecule is too tall.

        NSView *parent = view;
        while (parent && ![parent isKindOfClass: [HTMLLabel class]])
          parent = view.superview;
        CGRect frame = parent.frame;
        frame.size.height = [response floatValue];
        parent.frame = frame;

        while (parent &&
               ![parent isKindOfClass: [XScreenSaverConfigSheet class]])
          parent = view.superview;
        [((XScreenSaverConfigSheet *) parent) refreshTableView];
      }];
  }
}

# endif // 0
# endif // USE_WEBKIT


static char *anchorize (const char *url);

- (void) setText: (NSString *)t
{
  t = [t stringByTrimmingCharactersInSet:[NSCharacterSet
                                           whitespaceCharacterSet]];
  t = [t stringByReplacingOccurrencesOfString:@"&" withString:@"&amp;"];
  t = [t stringByReplacingOccurrencesOfString:@"<" withString:@"&lt;"];
  t = [t stringByReplacingOccurrencesOfString:@">" withString:@"&gt;"];
  t = [t stringByReplacingOccurrencesOfString:@"\n\n" withString:@" <P> "];
  t = [t stringByReplacingOccurrencesOfString:@"<P>  "
         withString:@"<P> &nbsp; &nbsp; &nbsp; &nbsp; "];
  t = [t stringByReplacingOccurrencesOfString:@"\n "
         withString:@"<BR> &nbsp; &nbsp; &nbsp; &nbsp; "];

  NSString *h = @"";
  for (NSString *s in
         [t componentsSeparatedByCharactersInSet:
              [NSCharacterSet whitespaceAndNewlineCharacterSet]]) {
    if ([s hasPrefix:@"http://"] ||
        [s hasPrefix:@"https://"]) {
      char *anchor = anchorize ([s cStringUsingEncoding:NSUTF8StringEncoding]);
      NSString *a2 = [NSString stringWithCString: anchor
                               encoding: NSUTF8StringEncoding];
      s = [NSString stringWithFormat: @"<A HREF=\"%@\">%@</A><BR>", s, a2];
      free (anchor);
    }
    h = [NSString stringWithFormat: @"%@ %@", h, s];
  }

  h = [h stringByReplacingOccurrencesOfString:@" <P> " withString:@"<P>"];
  h = [h stringByReplacingOccurrencesOfString:@"<BR><P>" withString:@"<P>"];
  h = [h stringByTrimmingCharactersInSet:[NSCharacterSet
                                           whitespaceAndNewlineCharacterSet]];

  [self setHTML: h];
}


-(BOOL) webView:(UIWebView *)wv
        shouldStartLoadWithRequest:(NSURLRequest *)req
        navigationType:(UIWebViewNavigationType)type
{
  // Force clicked links to open in Safari, not in this window.
  // Old way, 2020-ish?
  if (type == UIWebViewNavigationTypeLinkClicked) {
    UIApplication *app = [UIApplication sharedApplication];
    NSURL *url = [req URL];

    if ([app respondsToSelector:
              @selector(openURL:options:completionHandler:)]) {
      [app openURL:url options:@{} completionHandler:nil];
    } else {
#     pragma clang diagnostic push   // "openURL deprecated in iOS 10"
#     pragma clang diagnostic ignored "-Wdeprecated-declarations"
      [app openURL:url];
#     pragma clang diagnostic pop
    }

    return NO;
  }
  return YES;
}


#ifdef USE_WEBKIT
- (void)webView:(WKWebView *)wv
        decidePolicyForNavigationAction:(WKNavigationAction *)na
        decisionHandler:(void (^)(WKNavigationActionPolicy))dh
{
  // Force clicked links to open in Safari, not in this window.
  // New way, 2025-ish.
  if (na.navigationType == WKNavigationTypeLinkActivated) {
#   pragma clang diagnostic push   // "openURL deprecated in iOS 10"
#   pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [[UIApplication sharedApplication] openURL:na.request.URL];
#   pragma clang diagnostic pop
    dh (WKNavigationActionPolicyCancel);
  } else {
    dh (WKNavigationActionPolicyAllow);
  }
}
#endif // USE_WEBKIT


- (void) setFrame: (CGRect)r
{
  [super setFrame: r];
  r.origin.x = 0;
  r.origin.y = 0;
  [webView setFrame: r];
}


- (NSString *) stripTags:(NSString *)str
{
  NSString *result = @"";

  // Add newlines.
  str = [str stringByReplacingOccurrencesOfString:@"<P>"
             withString:@"<BR><BR>"
             options:NSCaseInsensitiveSearch
             range:NSMakeRange(0, [str length])];
  str = [str stringByReplacingOccurrencesOfString:@"<BR>"
             withString:@"\n"
             options:NSCaseInsensitiveSearch
             range:NSMakeRange(0, [str length])];

  // Remove HREFs.
  for (NSString *s in [str componentsSeparatedByString: @"<"]) {
    NSRange r = [s rangeOfString:@">"];
    if (r.length > 0)
      s = [s substringFromIndex: r.location + r.length];
    result = [result stringByAppendingString: s];
  }

  // Compress internal horizontal whitespace.
  str = result;
  result = @"";
  for (NSString *s in [str componentsSeparatedByCharactersInSet:
                             [NSCharacterSet whitespaceCharacterSet]]) {
    if ([result length] == 0)
      result = s;
    else if ([s length] > 0)
      result = [NSString stringWithFormat: @"%@ %@", result, s];
  }

  return result;
}


- (void) sizeToFit
{
  CGRect r = [self frame];

  /* It would be sensible to just ask the UIWebView how tall the page is,
     instead of hoping that NSString and UIWebView measure fonts and do
     wrapping in exactly the same way, but since UIWebView is asynchronous,
     we'd have to wait for the document to load first, e.g.:

       - Start the document loading;
       - return a default height to use for the UITableViewCell;
       - wait for the webViewDidFinishLoad delegate method to fire,
         or observe "loading" going to 0;
       - get document.body.offsetHeight from JavaScript;
       - then force the UITableView to reload, to pick up the new height.

     But I couldn't make that work with either UIWebView or WKWebView.
     So instead we measure the string itself and just assume that the
     fonts and layout are the same.  Or close enough.
   */
  NSString *text = [self stripTags: html];
  CGSize s = r.size;
  s.height = 999999;
  s = [text boundingRectWithSize:s
                         options:NSStringDrawingUsesLineFragmentOrigin
                      attributes:@{NSFontAttributeName: font}
                         context:nil].size;
  r.size.height = s.height;
  [self setFrame: r];
}


- (void) dealloc
{
  [html release];
  [font release];
  [webView release];
  [super dealloc];
}

@end

#endif // HAVE_IPHONE && USE_HTML_LABELS


@interface XScreenSaverConfigSheet (Private)

- (void)traverseChildren:(NSXMLNode *)node on:(NSView *)parent;

# ifndef HAVE_IPHONE
- (void) placeChild: (NSView *)c on:(NSView *)p right:(BOOL)r;
- (void) placeChild: (NSView *)c on:(NSView *)p;
static NSView *last_child (NSView *parent);
static void layout_group (NSView *group, BOOL horiz_p);
# else // HAVE_IPHONE
- (void) placeChild: (NSObject *)c on:(NSView *)p right:(BOOL)r;
- (void) placeChild: (NSObject *)c on:(NSView *)p;
- (void) placeSeparator;
- (void) bindResource:(NSObject *)ctl key:(NSString *)k reload:(BOOL)r;
- (void) refreshTableView;
# endif // HAVE_IPHONE

@end


@implementation XScreenSaverConfigSheet
{
  NSString *prev_imagedir;
}

# define LEFT_MARGIN      20   // left edge of window
# define COLUMN_SPACING   10   // gap between e.g. labels and text fields
# define LEFT_LABEL_WIDTH 70   // width of all left labels
# define LINE_SPACING     10   // leading between each line

# define FONT_SIZE	  17   // Magic hardcoded UITableView font size.

#pragma mark Talking to the resource database


/* Normally we read resources by looking up "KEY" in the database
   "org.jwz.xscreensaver.CLASSNAME".  But in the all-in-one iPhone
   app, everything is stored in the database "org.jwz.xscreensaver"
   instead, so transform keys to "CLASSNAME.KEY".

   NOTE: This is duplicated in PrefsReader.m, cause I suck.
 */
- (NSString *) makeKey:(NSString *)key
{
# ifdef HAVE_IPHONE
  if ([key isEqualToString:@"globalCycle"] ||
      [key isEqualToString:@"globalCycleTimeout"])
    return key;  // These two are global, not per-saver.

  NSString *prefix = [classname stringByAppendingString:@"."];
  if (! [key hasPrefix:prefix])  // Don't double up!
    key = [prefix stringByAppendingString:key];
# endif
  return key;
}


- (NSString *) makeCKey:(const char *)key
{
  return [self makeKey:[NSString stringWithCString:key
                                 encoding:NSUTF8StringEncoding]];
}


/* Given a command-line option, returns the corresponding resource name.
   Any arguments in the switch string are ignored (e.g., "-foo x").
 */
- (NSString *) switchToResource:(NSString *)cmdline_switch
                           opts:(const XrmOptionDescRec *)opts_array
                         valRet:(NSString **)val_ret
{
  char buf[1280];
  char *tail = 0;
  NSAssert(cmdline_switch, @"cmdline switch is null");
  if (! [cmdline_switch getCString:buf maxLength:sizeof(buf)
                          encoding:NSUTF8StringEncoding]) {
    NSAssert1(0, @"unable to convert %@", cmdline_switch);
    return 0;
  }
  char *s = strpbrk(buf, " \t\r\n");
  if (s && *s) {
    *s = 0;
    tail = s+1;
    while (*tail && (*tail == ' ' || *tail == '\t'))
      tail++;
  }
  
  while (opts_array[0].option) {
    const char *s1 = opts_array[0].option;
    const char *s2 = buf;
    if (s1[0] == '-' && s1[1] == '-') s1++;  /* -x and --x are the same */
    if (s2[0] == '-' && s2[1] == '-') s2++;
    if (!strcmp (s1, s2)) {
      const char *ret = 0;

      if (opts_array[0].argKind == XrmoptionNoArg) {
        if (tail && *tail)
          NSAssert1 (0, @"expected no args to switch: \"%@\"",
                     cmdline_switch);
        ret = opts_array[0].value;
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
      
      const char *res = opts_array[0].specifier;
      while (*res && (*res == '.' || *res == '*'))
        res++;
      return [self makeCKey:res];
    }
    opts_array++;
  }
  
  NSAssert1 (0, @"\"%@\" not present in options", cmdline_switch);
  return 0;
}


- (NSUserDefaultsController *)controllerForKey:(NSString *)key
{
  static NSDictionary *a = 0;
  if (! a) {
    a = UPDATER_DEFAULTS;
    [a retain];
  }
  if ([a objectForKey:key])
    // These preferences are global to all xscreensavers.
    return globalDefaultsController;
  else
    // All other preferences are per-saver.
    return userDefaultsController;
}


#ifdef HAVE_IPHONE

// Called when a slider is bonked.
//
# ifndef HAVE_TVOS
- (void)sliderAction:(UISlider*)sender
{
  if ([active_text_field canResignFirstResponder])
    [active_text_field resignFirstResponder];
  NSString *pref_key = [pref_keys objectAtIndex: [sender tag]];

  // Hacky API. See comment in InvertedSlider.m.
  double v = ([sender isKindOfClass: [InvertedSlider class]]
              ? [(InvertedSlider *) sender transformedValue]
              : [sender value]);

  [[self controllerForKey:pref_key]
    setObject:((v == (int) v)
               ? [NSNumber numberWithInt:(int) v]
               : [NSNumber numberWithDouble: v])
    forKey:pref_key];
}
# endif // !HAVE_TVOS


# ifndef HAVE_TVOS
// Called when a checkbox/switch is bonked.
//
- (void)switchAction:(UISwitch*)sender
{
  if ([active_text_field canResignFirstResponder])
    [active_text_field resignFirstResponder];
  NSString *pref_key = [pref_keys objectAtIndex: [sender tag]];
  NSString *v = ([sender isOn] ? @"true" : @"false");
  [[self controllerForKey:pref_key] setObject:v forKey:pref_key];
}
# endif // !HAVE_TVOS


# ifdef USE_PICKER_VIEW
// Called when a picker is bonked.
//
- (void)pickerView:(UIPickerView *)pv
        didSelectRow:(NSInteger)row
        inComponent:(NSInteger)column
{
  if ([active_text_field canResignFirstResponder])
    [active_text_field resignFirstResponder];

  NSAssert (column == 0, @"internal error");
  NSArray *a = [picker_values objectAtIndex: [pv tag]];
  if (! a) return;  // Too early?
  a = [a objectAtIndex:row];
  NSAssert (a, @"missing row");

//NSString *label    = [a objectAtIndex:0];
  NSString *pref_key = [a objectAtIndex:1];
  NSObject *pref_val = [a objectAtIndex:2];
  [[self controllerForKey:pref_key] setObject:pref_val forKey:pref_key];
}
# else  // !USE_PICKER_VIEW

// Called when a RadioButton is bonked.
//
- (void)radioAction:(RadioButton*)sender
{
  if ([active_text_field canResignFirstResponder])
    [active_text_field resignFirstResponder];

  NSArray *item = [[sender items] objectAtIndex: [sender index]];
  NSString *pref_key = [item objectAtIndex:1];
  NSObject *pref_val = [item objectAtIndex:2];
  [[self controllerForKey:pref_key] setObject:pref_val forKey:pref_key];
}

- (BOOL)textFieldShouldBeginEditing:(UITextField *)tf
{
  active_text_field = tf;
  return YES;
}

- (void)textFieldDidEndEditing:(UITextField *)tf
{
  NSString *pref_key = [pref_keys objectAtIndex: [tf tag]];
  NSString *txt = [tf text];
  [[self controllerForKey:pref_key] setObject:txt forKey:pref_key];
}

- (BOOL)textFieldShouldReturn:(UITextField *)tf
{
  active_text_field = nil;
  [tf resignFirstResponder];
  return YES;
}

# endif // !USE_PICKER_VIEW

#endif // HAVE_IPHONE


# ifndef HAVE_IPHONE

- (void) okAction:(NSObject *)arg
{
  // Without this, edits to text fields only happen if the user hits RET.
  // Clicking OK should also commit those edits.
  [self makeFirstResponder:nil];

  // Without the setAppliesImmediately:, when the saver restarts, it's still
  // got the old settings. -[XScreenSaverConfigSheet traverseTree] sets this
  // to NO; default is YES.

  [userDefaultsController   setAppliesImmediately:YES];
  [globalDefaultsController setAppliesImmediately:YES];
  [userDefaultsController   commitEditing];
  [globalDefaultsController commitEditing];
  [userDefaultsController   save:self];
  [globalDefaultsController save:self];

  /* Validate the new value of the imageDirectory as the OK button is clicked.
     If the user selected a directory from the Browse file selector, this 
     check has already happened; but if they edited the text field directly,
     this is our last chance to validate it.  Note that in this case, we are
     validating it after it has already been stored in the preferences DB. */
  {
    NSString *pref_key = @"imageDirectory";
    NSUserDefaultsController *prefs = [self controllerForKey:pref_key];
    NSString *imagedir = [[prefs defaults] objectForKey:pref_key];
    [self validateImageDirectory: imagedir];
  }

  [NSApp endSheet:self returnCode:NSModalResponseOK];
  [self close];
}

- (void) cancelAction:(NSObject *)arg
{
  [userDefaultsController   revert:self];
  [globalDefaultsController revert:self];
  [NSApp endSheet:self returnCode:NSModalResponseCancel];
  [self close];
}
# endif // !HAVE_IPHONE


- (void) resetAction:(NSObject *)arg
{
# ifndef HAVE_IPHONE
  [userDefaultsController   revertToInitialValues:self];
  [globalDefaultsController revertToInitialValues:self];
# else  // HAVE_IPHONE

  for (NSString *key in defaultOptions) {
    NSObject *val = [defaultOptions objectForKey:key];
    [[self controllerForKey:key] setObject:val forKey:key];
  }

  for (UIControl *ctl in pref_ctls) {
    NSString *pref_key = [pref_keys objectAtIndex: ctl.tag];
    [self bindResource:ctl key:pref_key reload:YES];
  }

  [self refreshTableView];
# endif // HAVE_IPHONE
}


/* Connects a control (checkbox, etc) to the corresponding preferences key.
 */
- (void) bindResource:(NSObject *)control key:(NSString *)pref_key
         reload:(BOOL)reload_p
{
  NSUserDefaultsController *prefs = [self controllerForKey:pref_key];
# ifndef HAVE_IPHONE
  NSDictionary *opts_dict = nil;
  NSString *bindto = ([control isKindOfClass:[NSPopUpButton class]]
                      ? @"selectedObject"
                      : ([control isKindOfClass:[NSMatrix class]]
                         ? @"selectedIndex"
                         : @"value"));

  if ([control isKindOfClass:[NSMatrix class]]) {
    opts_dict = @{ NSValueTransformerNameBindingOption:
                   @"TextModeTransformer" };
  } else if ([control isKindOfClass:[NSTextField class]]) {
    opts_dict = @{ NSValueTransformerNameBindingOption:
                     @"NonNilStringTransformer" };
  }

  [control bind:bindto
       toObject:prefs
    withKeyPath:[@"values." stringByAppendingString: pref_key]
        options:opts_dict];

# else  // HAVE_IPHONE
  SEL sel;
  NSObject *val = [prefs objectForKey:pref_key];
  NSString *sval = 0;
  double dval = 0;

  if ([val isKindOfClass:[NSString class]]) {
    sval = (NSString *) val;
    if (NSOrderedSame == [sval caseInsensitiveCompare:@"true"] ||
        NSOrderedSame == [sval caseInsensitiveCompare:@"yes"] ||
        NSOrderedSame == [sval caseInsensitiveCompare:@"1"])
      dval = 1;
    else
      dval = [sval doubleValue];
  } else if ([val isKindOfClass:[NSNumber class]]) {
    // NSBoolean (__NSCFBoolean) is really NSNumber.
    dval = [(NSNumber *) val doubleValue];
    sval = [(NSNumber *) val stringValue];
  }

# ifndef HAVE_TVOS
  if ([control isKindOfClass:[UISlider class]]) {
    sel = @selector(sliderAction:);
    // Hacky API. See comment in InvertedSlider.m.
    if ([control isKindOfClass:[InvertedSlider class]])
      [(InvertedSlider *) control setTransformedValue: dval];
    else
      [(UISlider *) control setValue: dval];
  } else if ([control isKindOfClass:[UISwitch class]]) {
    sel = @selector(switchAction:);
    [(UISwitch *) control setOn: ((int) dval != 0)];
# else  // HAVE_TVOS
  if (0) {
# endif // HAVE_TVOS

# ifdef USE_PICKER_VIEW
  } else if ([control isKindOfClass:[UIPickerView class]]) {
    sel = 0;
    [(UIPickerView *) control selectRow:((int)dval) inComponent:0
                      animated:NO];
# else  // !USE_PICKER_VIEW
  } else if ([control isKindOfClass:[RadioButton class]]) {
    sel = 0;  // radioAction: sent from didSelectRowAtIndexPath.
  } else if ([control isKindOfClass:[UITextField class]]) {
    sel = 0;  // ####
    [(UITextField *) control setText: sval];
# endif // !USE_PICKER_VIEW
  } else {
    NSAssert (0, @"unknown class");
  }

  // NSLog(@"\"%@\" = \"%@\" [%@, %.1f]", pref_key, val, [val class], dval);

  if (!reload_p) {
    if (! pref_keys) {
      pref_keys = [[NSMutableArray arrayWithCapacity:10] retain];
      pref_ctls = [[NSMutableArray arrayWithCapacity:10] retain];
    }

    [pref_keys addObject: [self makeKey:pref_key]];
    [pref_ctls addObject: control];
    ((UIControl *) control).tag = [pref_keys count] - 1;

    if (sel) {
      [(UIControl *) control addTarget:self action:sel
                     forControlEvents:UIControlEventValueChanged];
    }
  }

# endif // HAVE_IPHONE

# if 0
  NSObject *def = [[prefs defaults] objectForKey:pref_key];
  NSString *s = [NSString stringWithFormat:@"bind: \"%@\"", pref_key];
  s = [s stringByPaddingToLength:18 withString:@" " startingAtIndex:0];
  s = [NSString stringWithFormat:@"%@ = %@", s, 
                ([def isKindOfClass:[NSString class]]
                 ? [NSString stringWithFormat:@"\"%@\"", def]
                 : def)];
  s = [s stringByPaddingToLength:30 withString:@" " startingAtIndex:0];
  s = [NSString stringWithFormat:@"%@ %@ / %@", s,
                [def class], [control class]];
#  ifndef HAVE_IPHONE
  s = [NSString stringWithFormat:@"%@ / %@", s, bindto];
#  endif
  NSLog (@"%@", s);
# endif
}


- (void) bindResource:(NSObject *)control key:(NSString *)pref_key
{
  [self bindResource:(NSObject *)control key:(NSString *)pref_key reload:NO];
}



- (void) bindSwitch:(NSObject *)control
            cmdline:(NSString *)cmd
{
  [self bindResource:control 
        key:[self switchToResource:cmd opts:opts valRet:0]];
}


#pragma mark Text-manipulating utilities


static NSString *
unwrap (NSString *text)
{
  // Unwrap lines: delete \n but do not delete \n\n.
  //
  NSArray *lines = [text componentsSeparatedByString:@"\n"];
  NSUInteger i, nlines = [lines count];
  BOOL eolp = YES;

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
               [s hasPrefix:@"https://"] ||
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


# ifndef HAVE_IPHONE
/* Makes the text up to the first comma be bold.
 */
static void
boldify (NSText *nstext)
{
  NSString *text = [nstext string];
  NSRange r = [text rangeOfString:@"," options:0];
  r.length = r.location+1;

  r.location = 0;

  NSFont *font = [nstext font];
  font = [NSFont boldSystemFontOfSize:[font pointSize]];
  [nstext setFont:font range:r];
}
# endif // !HAVE_IPHONE


/* Creates a human-readable anchor to put on a URL.
   Duplicated in driver/demo-Gtk.c.
 */
static char *
anchorize (const char *url)
{
  const char *wiki  = "https://en.wikipedia.org/wiki/";
  const char *math  = "https://mathworld.wolfram.com/";
  const char *shade = "https://www.shadertoy.com/view/";
  if (!strncmp (wiki, url, strlen(wiki))) {
    char *anchor = (char *) malloc (strlen(url) * 3 + 10);
    const char *in;
    char *out;
    strcpy (anchor, "Wikipedia: \"");
    in = url + strlen(wiki);
    out = anchor + strlen(anchor);
    while (*in) {
      if (*in == '_') {
        *out++ = ' ';
      } else if (*in == '#') {
        *out++ = ':';
        *out++ = ' ';
      } else if (*in == '%') {
        char hex[3];
        unsigned int n = 0;
        hex[0] = in[1];
        hex[1] = in[2];
        hex[2] = 0;
        sscanf (hex, "%x", &n);
        *out++ = (char) n;
        in += 2;
      } else {
        *out++ = *in;
      }
      in++;
    }
    *out++ = '"';
    *out = 0;
    return anchor;

  } else if (!strncmp (math, url, strlen(math))) {
    char *anchor = (char *) malloc (strlen(url) * 3 + 10);
    const char *start, *in;
    char *out;
    strcpy (anchor, "MathWorld: \"");
    start = url + strlen(math);
    in = start;
    out = anchor + strlen(anchor);
    while (*in) {
      if (*in == '_') {
        *out++ = ' ';
      } else if (in != start && *in >= 'A' && *in <= 'Z') {
        *out++ = ' ';
        *out++ = *in;
      } else if (!strncmp (in, ".htm", 4)) {
        break;
      } else {
        *out++ = *in;
      }
      in++;
    }
    *out++ = '"';
    *out = 0;
    return anchor;

  } else if (!strncmp (shade, url, strlen(shade))) {
    char *anchor = (char *) malloc (strlen(url) * 3 + 10);
    const char *in;
    char *out;
    strcpy (anchor, "Shadertoy: ");
    in = url + strlen(shade);
    out = anchor + strlen(anchor);
    strcat (out, in);
    return anchor;

  } else {
    return strdup (url);
  }
}


#if !defined(HAVE_IPHONE) || !defined(USE_HTML_LABELS)

/* Converts any http: URLs in the given text field to clickable links.
 */
static void
hreffify (NSText *nstext)
{
# ifndef HAVE_IPHONE
  NSString *text = [nstext string];
  [nstext setRichText:YES];
# else
  NSString *text = [nstext text];
# endif

  NSUInteger L = [text length];
  NSRange start;		// range is start-of-search to end-of-string
  start.location = 0;
  start.length = L;
  while (start.location < L) {

    // Find the beginning of a URL...
    //
    NSRange r2 = [text rangeOfString: @"http://" options:0 range:start];
    NSRange r3 = [text rangeOfString:@"https://" options:0 range:start];
    if ((r2.location == NSNotFound &&
         r3.location != NSNotFound) ||
        (r2.location != NSNotFound &&
         r3.location != NSNotFound &&
         r3.location < r2.location))
      r2 = r3;
    if (r2.location == NSNotFound)
      break;

    // Next time around, start searching after this.
    start.location = r2.location + r2.length;
    start.length = L - start.location;

    // Find the end of a URL (whitespace or EOF)...
    //
    r3 = [text rangeOfCharacterFromSet:
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

    // If this is a Wikipedia URL, make the linked text be prettier.
    //
    char *anchor = anchorize(url);

# ifndef HAVE_IPHONE

    // Construct the RTF corresponding to <A HREF="url">anchor</A>
    //
    const char *fmt = "{\\field{\\*\\fldinst{HYPERLINK \"%s\"}}%s}";
    char *rtf = malloc (strlen (fmt) + strlen(url) + strlen(anchor) + 10);
    sprintf (rtf, fmt, url, anchor);

    NSData *rtfdata = [NSData dataWithBytesNoCopy:rtf length:strlen(rtf)];
    [nstext replaceCharactersInRange:r2 withRTF:rtfdata];

# else  // !HAVE_IPHONE
    // *anchor = 0; // Omit Wikipedia anchor 
    text = [text stringByReplacingCharactersInRange:r2
                 withString:[NSString stringWithCString:anchor
                                      encoding:NSUTF8StringEncoding]];
    // text = [text stringByReplacingOccurrencesOfString:@"\n\n\n"
    //              withString:@"\n\n"];
# endif // !HAVE_IPHONE

    free (anchor);

    NSUInteger L2 = [text length];  // might have changed
    start.location -= (L - L2);
    L = L2;
  }

# ifdef HAVE_IPHONE
  [nstext setText:text];
  [nstext sizeToFit];
# endif
}

#endif /* !HAVE_IPHONE || !USE_HTML_LABELS */



#pragma mark Creating controls from XML


/* Parse the attributes of an XML tag into a dictionary.
   For input, the dictionary should have as attributes the keys, each
   with @"" as their value.
   On output, the dictionary will set the keys to the values specified,
   and keys that were not specified will not be present in the dictionary.
   Warnings are printed if there are duplicate or unknown attributes.
 */
- (void) parseAttrs:(NSMutableDictionary *)dict node:(NSXMLNode *)node
{
  NSArray *attrs = [(NSXMLElement *) node attributes];
  NSUInteger n = [attrs count];
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
      NSAssert3 (0, @"duplicate %@: \"%@\", \"%@\"", key, old, val);
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

# ifdef HAVE_IPHONE
  // Kludge for starwars.xml:
  // If there is a "_low-label" and no "_label", but "_low-label" contains
  // spaces, divide them.
  NSString *lab = [dict objectForKey:@"_label"];
  NSString *low = [dict objectForKey:@"_low-label"];
  if (low && !lab) {
    NSArray *split =
      [[[low stringByTrimmingCharactersInSet:
               [NSCharacterSet whitespaceAndNewlineCharacterSet]]
         componentsSeparatedByString: @"  "]
        filteredArrayUsingPredicate:
          [NSPredicate predicateWithFormat:@"length > 0"]];
    if (split && [split count] == 2) {
      [dict setValue:[split objectAtIndex:0] forKey:@"_label"];
      [dict setValue:[split objectAtIndex:1] forKey:@"_low-label"];
    }
  }
# endif // HAVE_IPHONE
}


/* Handle the options on the top level <xscreensaver> tag.
 */
- (NSString *) parseXScreenSaverTag:(NSXMLNode *)node
{
  NSMutableDictionary *dict = [@{ @"name":   @"",
                                  @"_label": @"",
                                  @"gl":     @"" }
                                mutableCopy];
  [self parseAttrs:dict node:node];
  NSString *name  = [dict objectForKey:@"name"];
  NSString *label = [dict objectForKey:@"_label"];
  [dict release];
  dict = 0;
    
  NSAssert1 (label, @"no _label in %@", [node name]);
  NSAssert1 (name, @"no name in \"%@\"", label);
  return label;
}


/* Creates a label: an un-editable NSTextField displaying the given text.
 */
- (LABEL *) makeLabel:(NSString *)text
{
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 10;
# ifndef HAVE_IPHONE
  NSTextField *lab = [[NSTextField alloc] initWithFrame:rect];
  [lab setSelectable:NO];
  [lab setEditable:NO];
  [lab setBezeled:NO];
  [lab setDrawsBackground:NO];
  [lab setStringValue:NSLocalizedString(text, @"")];
  [lab sizeToFit];
# else  // HAVE_IPHONE
  UILabel *lab = [[UILabel alloc] initWithFrame:rect];
  [lab setText: [text stringByTrimmingCharactersInSet:
                 [NSCharacterSet whitespaceAndNewlineCharacterSet]]];
  [lab setBackgroundColor:[UIColor clearColor]];
  [lab setNumberOfLines:0]; // unlimited
  // [lab setLineBreakMode:UILineBreakModeWordWrap];
  [lab setLineBreakMode:NSLineBreakByTruncatingHead];
  [lab setAutoresizingMask: (UIViewAutoresizingFlexibleWidth |
                             UIViewAutoresizingFlexibleHeight)];
# endif // HAVE_IPHONE
  [lab autorelease];
  return lab;
}


/* Creates the checkbox (NSButton) described by the given XML node.
 */
- (void) makeCheckbox:(NSXMLNode *)node on:(NSView *)parent
{
  NSMutableDictionary *dict = [@{ @"id":        @"",
                                  @"_label":    @"",
                                  @"arg-set":   @"",
                                  @"arg-unset": @"",
                                  @"disabled":  @"" }
                                mutableCopy];
  [self parseAttrs:dict node:node];
  NSString *label     = [dict objectForKey:@"_label"];
  NSString *arg_set   = [dict objectForKey:@"arg-set"];
  NSString *arg_unset = [dict objectForKey:@"arg-unset"];

  NSString *dd   = [dict objectForKey:@"disabled"];
  BOOL disabledp = (dd &&
                    (NSOrderedSame == [dd caseInsensitiveCompare:@"true"] ||
                     NSOrderedSame == [dd caseInsensitiveCompare:@"yes"]));
  [dict release];
  dict = 0;
  
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

# ifndef HAVE_IPHONE

  NSButton *button = [[NSButton alloc] initWithFrame:rect];
  [button setButtonType:NSSwitchButton];
  [button setTitle:label];
  [button sizeToFit];
  [self placeChild:button on:parent];

# else  // HAVE_IPHONE

  LABEL *lab = [self makeLabel:label];
  [self placeChild:lab on:parent];
# ifndef HAVE_TVOS
  UISwitch *button = [[UISwitch alloc] initWithFrame:rect];
  [self placeChild:button on:parent right:YES];
# endif // !HAVE_TVOS

# endif // HAVE_IPHONE
  
# ifndef HAVE_TVOS
  if (disabledp)
    [button setEnabled:NO];
  else
    [self bindSwitch:button cmdline:(arg_set ? arg_set : arg_unset)];

  [button release];
# endif // !HAVE_TVOS
}


/* Creates the number selection control described by the given XML node.
   If "type=slider", it's an NSSlider.
   If "type=spinbutton", it's a text field with up/down arrows next to it.
 */
- (void) makeNumberSelector:(NSXMLNode *)node on:(NSView *)parent
{
  NSMutableDictionary *dict = [@{ @"id":          @"",
                                  @"_label":      @"",
                                  @"_low-label":  @"",
                                  @"_high-label": @"",
                                  @"type":        @"",
                                  @"arg":         @"",
                                  @"low":         @"",
                                  @"high":        @"",
                                  @"step":        @"",
                                  @"default":     @"",
                                  @"convert":     @"" }
                                mutableCopy];
  [self parseAttrs:dict node:node];
  NSString *label      = [dict objectForKey:@"_label"];
  NSString *low_label  = [dict objectForKey:@"_low-label"];
  NSString *high_label = [dict objectForKey:@"_high-label"];
  NSString *type       = [dict objectForKey:@"type"];
  NSString *arg        = [dict objectForKey:@"arg"];
  NSString *low        = [dict objectForKey:@"low"];
  NSString *high       = [dict objectForKey:@"high"];
  NSString *step_by    = [dict objectForKey:@"step"];
  NSString *def        = [dict objectForKey:@"default"];
  NSString *cvt        = [dict objectForKey:@"convert"];
  [dict release];
  dict = 0;
  
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
  if (cvt &&
      !([cvt isEqualToString:@"invert"] ||
        [cvt isEqualToString:@"ratio"])) {
    NSAssert1 (0,
           @"if provided, \"convert\" must be \"invert\" or \"ratio\" in %@",
               label);
  }
    
  // If either the min or max field contains a decimal point, then this
  // option may have a floating point value; otherwise, it is constrained
  // to be an integer.
  //
  NSCharacterSet *dot =
    [NSCharacterSet characterSetWithCharactersInString:@"."];
  BOOL float_p = ([low rangeOfCharacterFromSet:dot].location != NSNotFound ||
                  [high rangeOfCharacterFromSet:dot].location != NSNotFound);

  if ([type isEqualToString:@"slider"]
# ifdef HAVE_IPHONE  // On iPhone, we use sliders for all numeric values.
      || [type isEqualToString:@"spinbutton"]
# endif
      ) {

    NSRect rect;
    rect.origin.x = rect.origin.y = 0;    
    rect.size.width = 150;
    rect.size.height = 23;  // apparent min height for slider with ticks...
# ifndef HAVE_TVOS
    InvertedSlider *slider =
      [[InvertedSlider alloc] initWithFrame:rect
                                   inverted: [cvt isEqualToString:@"invert"]
                                   ratio:    [cvt isEqualToString:@"ratio"]
                                   integers: !float_p];
    // Fortunately 'doubleValue' does not use locale-specific number parsing
    // the way that sscanf %f does.  doubleValue always expects "1.0", but
    // sscanf expects "1,0" if LC_NUMERIC is e.g. "de_DE".
    [slider setMaxValue:[high doubleValue]];
    [slider setMinValue:[low  doubleValue]];
    if (step_by)
      [slider setIncrement:[step_by doubleValue]];
    
    int range = [slider maxValue] - [slider minValue] + 1;
    int range2 = range;
    int max_ticks = 21;
    while (range2 > max_ticks)
      range2 /= 10;

# ifndef HAVE_IPHONE
    // If we have elided ticks, leave it at the max number of ticks.
    if (range != range2 && range2 < max_ticks)
      range2 = max_ticks;

    // If it's a float, always display the max number of ticks.
    if (float_p && range2 < max_ticks)
      range2 = max_ticks;

    [slider setNumberOfTickMarks:range2];

    [slider setAllowsTickMarkValuesOnly:
              (range == range2 &&  // we are showing the actual number of ticks
               !float_p)];         // and we want integer results
# endif // !HAVE_IPHONE
# endif // !HAVE_TVOS

    // #### Note: when the slider's range is large enough that we aren't
    //      showing all possible ticks, the slider's value is not constrained
    //      to be an integer, even though it should be...
    //      Maybe we need to use a value converter or something?

    LABEL *lab;
    if (label) {
      lab = [self makeLabel:label];
      [self placeChild:lab on:parent];
# ifdef HAVE_IPHONE
# ifndef HAVE_TVOS
      if (low_label) {
        CGFloat s = [NSFont systemFontSize] + 4;
        [lab setFont:[NSFont boldSystemFontOfSize:s]];
      }
# endif // !HAVE_TVOS
# endif
    }
    
    if (low_label) {
      lab = [self makeLabel:low_label];
# ifndef HAVE_TVOS
      [lab setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
# endif // !HAVE_TVOS
# ifndef HAVE_IPHONE
      [lab setAlignment:1];  // right aligned
      rect = [lab frame];
      if (rect.size.width < LEFT_LABEL_WIDTH)
        rect.size.width = LEFT_LABEL_WIDTH;  // make all left labels same size
      rect.size.height = [slider frame].size.height;
      [lab setFrame:rect];
      [self placeChild:lab on:parent];
# else  // HAVE_IPHONE
      [lab setTextAlignment: NSTextAlignmentRight];
      // Sometimes rotation screws up truncation.
      [lab setLineBreakMode:NSLineBreakByClipping];
      [self placeChild:lab on:parent right:(label ? YES : NO)];
# endif // HAVE_IPHONE
     }
    
# ifndef HAVE_TVOS

# ifndef HAVE_IPHONE
    [self placeChild:slider on:parent right:(low_label ? YES : NO)];
# else  // HAVE_IPHONE
    [self placeChild:slider on:parent right:(label || low_label ? YES : NO)];
# endif // HAVE_IPHONE
    
    if (low_label) {
      // Make left label be same height as slider.
      rect = [lab frame];
      rect.size.height = [slider frame].size.height;
      [lab setFrame:rect];
    }

    if (! low_label) {
      rect = [slider frame];
      if (rect.origin.x < LEFT_LABEL_WIDTH)
        rect.origin.x = LEFT_LABEL_WIDTH; // make unlabelled sliders line up too
      [slider setFrame:rect];
    }
        
    if (high_label) {
      lab = [self makeLabel:high_label];
      [lab setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
      rect = [lab frame];

      // Make right label be same height as slider.
      rect.size.height = [slider frame].size.height;
      [lab setFrame:rect];
# ifdef HAVE_IPHONE
      // Sometimes rotation screws up truncation.
      [lab setLineBreakMode:NSLineBreakByClipping];
# endif
      [self placeChild:lab on:parent right:YES];
     }

    [self bindSwitch:slider cmdline:arg];
    [slider release];
# endif // !HAVE_TVOS
    
#ifndef HAVE_IPHONE  // On iPhone, we use sliders for all numeric values.

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
    [txt setStringValue:NSLocalizedString(@"0000.0", @"")];
    [txt sizeToFit];
    [txt setStringValue:@""];
    
    if (label) {
      LABEL *lab = [self makeLabel:label];
      //[lab setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
      [lab setAlignment:1];  // right aligned
      rect = [lab frame];
      if (rect.size.width < LEFT_LABEL_WIDTH)
        rect.size.width = LEFT_LABEL_WIDTH;  // make all left labels same size
      rect.size.height = [txt frame].size.height;
      [lab setFrame:rect];
      [self placeChild:lab on:parent];
     }
    
    [self placeChild:txt on:parent right:(label ? YES : NO)];
    
    if (! label) {
      rect = [txt frame];
      if (rect.origin.x < LEFT_LABEL_WIDTH)
        rect.origin.x = LEFT_LABEL_WIDTH; // make unlabelled spinbtns line up
      [txt setFrame:rect];
    }
    
    rect.size.width = rect.size.height = 10;
    NSStepper *step = [[NSStepper alloc] initWithFrame:rect];
    [step sizeToFit];
    [self placeChild:step on:parent right:YES];
    rect = [step frame];
    rect.origin.x -= COLUMN_SPACING;  // this one goes close
    rect.origin.y += ([txt frame].size.height - rect.size.height) / 2;
    [step setFrame:rect];
    
    [step setMinValue:[low  doubleValue]];
    [step setMaxValue:[high doubleValue]];
    [step setAutorepeat:YES];
    [step setValueWraps:NO];
    
    double range = [high doubleValue] - [low doubleValue];
    if (step_by)
      [step setIncrement:[step_by doubleValue]];
    else if (range < 1.0)
      [step setIncrement:range / 10.0];
    else if (range >= 500)
      [step setIncrement:range / 100.0];
    else
      [step setIncrement:1.0];

    NSNumberFormatter *fmt = [[[NSNumberFormatter alloc] init] autorelease];
    [fmt setFormatterBehavior:NSNumberFormatterBehavior10_4];
    [fmt setNumberStyle:NSNumberFormatterDecimalStyle];
    [fmt setMinimum:[NSNumber numberWithDouble:[low  doubleValue]]];
    [fmt setMaximum:[NSNumber numberWithDouble:[high doubleValue]]];
    [fmt setMinimumFractionDigits: (float_p ? 1 : 0)];
    [fmt setMaximumFractionDigits: (float_p ? 2 : 0)];

    [fmt setGeneratesDecimalNumbers:float_p];
    [[txt cell] setFormatter:fmt];

    [self bindSwitch:step cmdline:arg];
    [self bindSwitch:txt  cmdline:arg];
    
    [step release];
    [txt release];

# endif // HAVE_IPHONE

  } else {
    NSAssert2 (0, @"unknown type \"%@\" in \"%@\"", type, label);
  }
}


# ifndef HAVE_IPHONE
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
# endif // !HAVE_IPHONE


/* Creates the popup menu described by the given XML node (and its children).
 */
- (void) makeOptionMenu:(NSXMLNode *)node on:(NSView *)parent
               disabled:(BOOL)disabled
{
  NSArray *children = [node children];
  NSUInteger i, count = [children count];

  if (count <= 0) {
    NSAssert1 (0, @"no menu items in \"%@\"", [node name]);
    return;
  }

  // get the "id" attribute off the <select> tag.
  //
  NSMutableDictionary *dict = [@{ @"id": @"", } mutableCopy];
  [self parseAttrs:dict node:node];
  [dict release];
  dict = 0;
  
  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = 10;
  rect.size.height = 10;

  NSString *menu_key = nil;   // the resource key used by items in this menu

# ifndef HAVE_IPHONE
  // #### "Build and Analyze" says that all of our widgets leak, because it
  //      seems to not realize that placeChild -> addSubview retains them.
  //      Not sure what to do to make these warnings go away.

  NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:rect
                                                     pullsDown:NO];
  NSMenuItem *def_item = nil;
  float max_width = 0;

# else  // HAVE_IPHONE

  NSString *def_item = nil;

  rect.size.width  = 0;
  rect.size.height = 0;
#  ifdef USE_PICKER_VIEW
  UIPickerView *popup = [[[UIPickerView alloc] initWithFrame:rect] retain];
  popup.delegate = self;
  popup.dataSource = self;
#  endif // !USE_PICKER_VIEW
  NSMutableArray *items = [NSMutableArray arrayWithCapacity:10];

# endif // HAVE_IPHONE
  
  for (i = 0; i < count; i++) {
    NSXMLNode *child = [children objectAtIndex:i];

    if ([child kind] == NSXMLCommentKind)
      continue;
    if ([child kind] != NSXMLElementKind) {
//    NSAssert2 (0, @"weird XML node kind: %d: %@", (int)[child kind], node);
      continue;
    }

    // get the "id", "_label", and "arg-set" attrs off of the <option> tags.
    //
    NSMutableDictionary *dict2 = [@{ @"id":      @"",
                                     @"_label":  @"",
                                     @"arg-set": @"" }
                                   mutableCopy];
    [self parseAttrs:dict2 node:child];
    NSString *label   = [dict2 objectForKey:@"_label"];
    NSString *arg_set = [dict2 objectForKey:@"arg-set"];
    [dict2 release];
    dict2 = 0;
    
    if (!label) {
      NSAssert1 (0, @"no _label in %@", [child name]);
      continue;
    }

# ifndef HAVE_IPHONE
    // create the menu item (and then get a pointer to it)
    [popup addItemWithTitle:label];
    NSMenuItem *item = [popup itemWithTitle:label];
# endif // HAVE_IPHONE

    if (arg_set) {
      NSString *this_val = NULL;
      NSString *this_key = [self switchToResource: arg_set
                                 opts: opts
                                 valRet: &this_val];
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
# ifndef HAVE_IPHONE
      set_menu_item_object (item, this_val);
# else
      // Array holds ["Label", "resource-key", "resource-val"].
      [items addObject:[NSMutableArray arrayWithObjects:
                                         label, @"", this_val, nil]];
# endif

    } else {
      // no arg-set -- only one menu item can be missing that.
      NSAssert1 (!def_item, @"no arg-set in \"%@\"", label);
# ifndef HAVE_IPHONE
      def_item = item;
# else
      def_item = label;
      // Array holds ["Label", "resource-key", "resource-val"].
      [items addObject:[NSMutableArray arrayWithObjects:
                                         label, @"", @"", nil]];
# endif
    }

    /* make sure the menu button has room for the text of this item,
       and remember the greatest width it has reached.
     */
# ifndef HAVE_IPHONE
    [popup setTitle:label];
    [popup sizeToFit];
    NSRect r = [popup frame];
    if (r.size.width > max_width) max_width = r.size.width;
# endif // HAVE_IPHONE
  }
  
  if (!menu_key) {
    NSAssert1 (0, @"no switches in menu \"%@\"", [dict objectForKey:@"id"]);
    return;
  }

  /* We've added all of the menu items.  If there was an item with no
     command-line switch, then it's the item that represents the default
     value.  Now we must bind to that item as well...  (We have to bind
     this one late, because if it was the first item, then we didn't
     yet know what resource was associated with this menu.)
   */
  if (def_item) {
    NSObject *def_obj = [defaultOptions objectForKey:menu_key];
    NSAssert2 (def_obj, 
               @"no default value for resource \"%@\" in menu item \"%@\"",
               menu_key,
# ifndef HAVE_IPHONE
               [def_item title]
# else
               def_item
# endif
               );

# ifndef HAVE_IPHONE
    set_menu_item_object (def_item, def_obj);
# else  // !HAVE_IPHONE
    for (NSMutableArray *a in items) {
      // Make sure each array contains the resource key.
      [a replaceObjectAtIndex:1 withObject:menu_key];
      // Make sure the default item contains the default resource value.
      if (def_obj && def_item &&
          [def_item isEqualToString:[a objectAtIndex:0]])
        [a replaceObjectAtIndex:2 withObject:def_obj];
    }
# endif // !HAVE_IPHONE
  }

# ifndef HAVE_IPHONE
#  ifdef USE_PICKER_VIEW
  /* Finish tweaking the menu button itself.
   */
  if (def_item)
    [popup setTitle:[def_item title]];
  NSRect r = [popup frame];
  r.size.width = max_width;
  [popup setFrame:r];
#  endif // USE_PICKER_VIEW
# endif

# if !defined(HAVE_IPHONE)
  [self placeChild:popup on:parent];
  if (disabled)
    [popup setEnabled:NO];
  else
    [self bindResource:popup key:menu_key];
  [popup release];
# elif defined(USE_PICKER_VIEW)
  [self placeChild:popup on:parent];
  [self bindResource:popup key:menu_key];
  [popup release];
# endif

# ifdef HAVE_IPHONE
#  ifdef USE_PICKER_VIEW
  // Store the items for this picker in the picker_values array.
  // This is so fucking stupid.

  unsigned long menu_number = [pref_keys count] - 1;
  if (! picker_values)
    picker_values = [[NSMutableArray arrayWithCapacity:menu_number] retain];
  while ([picker_values count] <= menu_number)
    [picker_values addObject:@{}];
  [picker_values replaceObjectAtIndex:menu_number withObject:items];
  [popup reloadAllComponents];

#  else  // !USE_PICKER_VIEW

  [self placeSeparator];

  i = 0;
  for (__attribute__((unused)) NSArray *item in items) {
    RadioButton *b = [[RadioButton alloc] initWithIndex: (int)i
                                          items:items];
    [b setLineBreakMode:NSLineBreakByTruncatingHead];
    [b setFont:[NSFont boldSystemFontOfSize: FONT_SIZE]];
    [self placeChild:b on:parent];
    [b release];
    i++;
  }

  [self placeSeparator];

#  endif // !USE_PICKER_VIEW
# endif // !HAVE_IPHONE

}

- (void) makeOptionMenu:(NSXMLNode *)node on:(NSView *)parent
{
 [self makeOptionMenu:node on:parent disabled:NO];
}


/* Creates an uneditable, wrapping NSTextField to display the given
   text enclosed by <description> ... </description> in the XML.
 */
- (void) makeDescLabel:(NSXMLNode *)node on:(NSView *)parent
{
# ifdef HAVE_IPHONE
  // The "auto-cycle" controls go just above the description.
  [self makeCycleSaverControlBox:node on:parent];
# endif

  NSString *text = nil;
  NSArray *children = [node children];
  NSUInteger i, count = [children count];

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
# ifndef HAVE_IPHONE
  NSText *lab = [[NSText alloc] initWithFrame:rect];
  [lab autorelease];
  [lab setEditable:NO];
  [lab setDrawsBackground:NO];
  [lab setHorizontallyResizable:YES];
  [lab setVerticallyResizable:YES];
  [lab setString:text];
  hreffify (lab);
  boldify (lab);
  [lab sizeToFit];

# else  // HAVE_IPHONE

#  ifndef USE_HTML_LABELS

  UILabel *lab = [self makeLabel:text];
# ifndef HAVE_TVOS
  [lab setFont:[NSFont systemFontOfSize: [NSFont systemFontSize]]];
# endif // !HAVE_TVOS
  hreffify (lab);

#  else  // USE_HTML_LABELS

  // if (self.view.frame.size.width > 640)  // A little vertical padding
  text = [@"\n" stringByAppendingString: text];

  // Small font on iPhone; normal font on iPad.
  double pointsize = (self.view.frame.size.width > 800
                      ? FONT_SIZE
                      : [NSFont systemFontSize]);
  HTMLLabel *lab = [[HTMLLabel alloc] 
                     initWithText:text
                             font:[NSFont systemFontOfSize: pointsize]];
  [lab autorelease];
  [lab setFrame:rect];
  [lab sizeToFit];
#  endif // USE_HTML_LABELS

  [self placeSeparator];

# endif // HAVE_IPHONE

  [self placeChild:lab on:parent];
}


/* Creates the NSTextField described by the given XML node.
 */
- (void) makeTextField: (NSXMLNode *)node
                    on: (NSView *)parent
             withLabel: (BOOL) label_p
            horizontal: (BOOL) horiz_p
{
  NSMutableDictionary *dict = [@{ @"id":     @"",
                                  @"_label": @"",
                                  @"arg":    @"" }
                                mutableCopy];
  [self parseAttrs:dict node:node];
  NSString *label = [dict objectForKey:@"_label"];
  NSString *arg   = [dict objectForKey:@"arg"];
  [dict release];
  dict = 0;

  if (!label && label_p) {
    NSAssert1 (0, @"no _label in %@", [node name]);
    return;
  }

  NSAssert1 (arg, @"no arg in %@", label);

  NSRect rect;
  rect.origin.x = rect.origin.y = 0;    
  rect.size.width = rect.size.height = 10;
  
  NSTextField *txt = [[NSTextField alloc] initWithFrame:rect];

# ifndef HAVE_IPHONE

  // make the default size be around 30 columns; a typical value for
  // these text fields is "xscreensaver-text --cols 40".
  //
  [txt setStringValue:
         NSLocalizedString(@"123456789 123456789 123456789 ", @"")];
  [txt sizeToFit];
  [[txt cell] setWraps:NO];
  [[txt cell] setScrollable:YES];
  [txt setStringValue:@""];
  
# else  // HAVE_IPHONE

  txt.adjustsFontSizeToFitWidth = YES;
  // Why did I do this? Messes up dark mode.
  // txt.textColor = [UIColor blackColor];
  txt.font = [UIFont systemFontOfSize: FONT_SIZE];
  txt.placeholder = @"";
  txt.borderStyle = UITextBorderStyleRoundedRect;
  txt.textAlignment = NSTextAlignmentRight;
  txt.keyboardType = UIKeyboardTypeDefault;  // Full kbd
  txt.autocorrectionType = UITextAutocorrectionTypeNo;
  txt.autocapitalizationType = UITextAutocapitalizationTypeNone;
  txt.clearButtonMode = UITextFieldViewModeAlways;
  txt.returnKeyType = UIReturnKeyDone;
  txt.delegate = self;
  txt.text = @"";
  [txt setEnabled: YES];

  rect.size.height = [txt.font lineHeight] * 1.2;
  [txt setFrame:rect];

# endif // HAVE_IPHONE

  if (label) {
    LABEL *lab = [self makeLabel:label];
    [self placeChild:lab on:parent];
  }

  [self placeChild:txt on:parent right:(label ? YES : NO)];

  [self bindSwitch:txt cmdline:arg];
  [txt release];
}


#ifndef HAVE_IPHONE
- (void) endValidateSheet:(NSWindow *)win
{
  [NSApp endSheet: win returnCode: NSModalResponseOK];
}

- (void) validateImageDirectory: (NSString *) imagedir
{
  if (!prev_imagedir || !imagedir ||
      [prev_imagedir isEqualToString: imagedir]) {
    return;
  }

  prev_imagedir = imagedir;

  // Create a background task running xscreensaver-getimage-file.
  //
  // Note that "Contents/Resources/" in the .saver bundle is on $PATH,
  // but NSTask doesn't search $PATH for the executable.  Sigh.
  //
  NSString *cmd0 = @"xscreensaver-getimage-file";
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSString *dir = [nsb resourcePath];    // "Contents/Resources"
  NSString *cmd = [dir stringByAppendingPathComponent: cmd0];

  if (! [[NSFileManager defaultManager] fileExistsAtPath: cmd]) {
    NSAssert1 (0, @"file does not exist: %@", cmd);
    return;
  }

  NSArray *av = @[ imagedir ];
  NSTask *task = [[NSTask alloc] init];
  task.launchPath = cmd;
  task.arguments = av;
  NSPipe *pipe = [NSPipe pipe];
  task.standardOutput = [NSPipe pipe];  // Just to close it.
  task.standardError = pipe;

  // Create an alert with a spinner in it.
  //
  NSAlert *alert = [[NSAlert alloc] init];
  [alert setMessageText: [NSString stringWithFormat:
                                     @"Populating image cache for:\n%@",
                                   imagedir]];
  [alert setInformativeText: @"This may take a little while..."];
  [alert addButtonWithTitle: @"Cancel"];
  [alert setAlertStyle: NSAlertStyleWarning];

  NSProgressIndicator *spinner =
    [[NSProgressIndicator alloc] initWithFrame: NSMakeRect(0,0,40,40)];
  spinner.indeterminate = TRUE;
  spinner.style = NSProgressIndicatorStyleSpinning;
  [spinner sizeToFit];
  [spinner startAnimation: self];
  [alert setAccessoryView: spinner];

  task.terminationHandler = ^(NSTask *tt) {
    // The task terminated, so tell the main UI thread to un-post the alert.
    [self performSelectorOnMainThread:
            @selector(endValidateSheet:)
                           withObject: alert.window
                        waitUntilDone: YES];
  };

  NSLog (@"launching %@ %@", cmd, av[0]);
  [task launch];

  // Wait for either the Cancel button or the NSTask to end.
  //
  [alert beginSheetModalForWindow: self
                completionHandler:^(NSModalResponse returnCode) {
      if (task.running)
        [task terminate];
      NSLog (@"%@ finished", cmd);

      NSData *data = [pipe.fileHandleForReading readDataToEndOfFile];
      NSString *txt = [[NSString alloc] initWithData:data
                                            encoding:NSUTF8StringEncoding];

      if ([txt length]) {

        // "s/^xscreensaver-getimage-file: //gm;"
        txt = [txt stringByReplacingOccurrencesOfString:
                           [cmd0 stringByAppendingString: @": "]
                                             withString:@""];

        NSAlert *alert2 = [[NSAlert alloc] init];
        // [alert2 setMessageText: @"Warning"];
        // [alert2 setInformativeText: txt];
        [alert2 setMessageText: [@"Warning:\n\n"
                                    stringByAppendingString: txt]];

        [alert2 addButtonWithTitle: @"OK"];
        [alert2 setAlertStyle: NSAlertStyleWarning];
        [alert2 beginSheetModalForWindow: self
                       completionHandler:^(NSModalResponse returnCode) {
          }];
      }
    }];
}
#endif // !HAVE_IPHONE


/* Creates the NSTextField described by the given XML node,
   and hooks it up to a Choose button and a file selector widget.
 */
- (void) makeFileSelector: (NSXMLNode *)node
                       on: (NSView *)parent
                 dirsOnly: (BOOL) dirsOnly
                withLabel: (BOOL) label_p
                 editable: (BOOL) editable_p
                 imagedir: (BOOL) imagedir_p
{
# ifndef HAVE_IPHONE	// No files. No selectors.
  NSMutableDictionary *dict = [@{ @"id":     @"",
                                  @"_label": @"",
                                  @"arg":    @"" }
                                mutableCopy];
  [self parseAttrs:dict node:node];
  NSString *label = [dict objectForKey:@"_label"];
  NSString *arg   = [dict objectForKey:@"arg"];
  [dict release];
  dict = 0;

  if (!label && label_p) {
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
  [txt setStringValue:NSLocalizedString(@"123456789 123456789 ", @"")];
  [txt sizeToFit];
  [txt setSelectable:YES];
  [txt setEditable:editable_p];
  [txt setBezeled:editable_p];
  [txt setDrawsBackground:editable_p];
  [[txt cell] setWraps:NO];
  [[txt cell] setScrollable:YES];
  [[txt cell] setLineBreakMode:NSLineBreakByTruncatingHead];
  [txt setStringValue:@""];

  LABEL *lab = 0;
  if (label) {
    lab = [self makeLabel:label];
    [self placeChild:lab on:parent];
  }

  [self placeChild:txt on:parent right:(label ? YES : NO)];

  [self bindSwitch:txt cmdline:arg];

//####  [txt setDelegate: self];   // For controlTextDidEndEditing, above.
  [txt release];

  // Make the text field and label be the same height, whichever is taller.
  if (lab) {
    rect = [txt frame];
    rect.size.height = ([lab frame].size.height > [txt frame].size.height
                        ? [lab frame].size.height
                        : [txt frame].size.height);
    [txt setFrame:rect];
  }

  // Now put a "Choose" button next to it.
  //
  rect.origin.x = rect.origin.y = 0;    
  rect.size.width = rect.size.height = 10;
  NSButton *choose = [[NSButton alloc] initWithFrame:rect];
  [choose setTitle:NSLocalizedString(@"Choose...", @"")];
  [choose setBezelStyle:NSRoundedBezelStyle];
  [choose sizeToFit];

  [self placeChild:choose on:parent right:YES];

  // center the Choose button around the midpoint of the text field.
  rect = [choose frame];
  rect.origin.y = ([txt frame].origin.y + 
                   (([txt frame].size.height - rect.size.height) / 2));
  [choose setFrameOrigin:rect.origin];

  [choose setTarget:[parent window]];
  if (imagedir_p)
    [choose setAction:@selector(fileSelectorChooseImageDirAction:)];
  else if (dirsOnly)
    [choose setAction:@selector(fileSelectorChooseDirsAction:)];
  else
    [choose setAction:@selector(fileSelectorChooseAction:)];

  if (imagedir_p) {
    /* Hang on to the value that "imageDirectory" had before posting the
       dialog so that once the user clicks OK, we can tell whether it has
       changed, and validate the new value if so. */
    NSString *pref_key = @"imageDirectory";
    NSUserDefaultsController *prefs = [self controllerForKey:pref_key];
    prev_imagedir = [[prefs defaults] objectForKey:pref_key];
  }

  [choose release];
# endif // !HAVE_IPHONE
}


# ifndef HAVE_IPHONE

/* Runs a modal file selector and sets the text field's value to the
   selected file or directory.
 */
- (void) doFileSelector: (NSTextField *)txt
                   dirs: (BOOL)dirs_p
               imagedir: (BOOL)imagedir_p
{
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseFiles:!dirs_p];
  [panel setCanChooseDirectories:dirs_p];
  [panel setCanCreateDirectories:NO];

  NSString *def = [[txt stringValue] stringByExpandingTildeInPath];
  if (dirs_p) {
    // Open in the previously-selected directory.
    [panel setDirectoryURL:
             [NSURL fileURLWithPath:def isDirectory:YES]];
    [panel setNameFieldStringValue:[def lastPathComponent]];
  } else {
    // Open in the directory of the previously-selected file.
    [panel setDirectoryURL:
             [NSURL fileURLWithPath:[def stringByDeletingLastPathComponent]
                    isDirectory:YES]];
    // I hoped that this would select that file by default, but it does not.
    [panel setNameFieldStringValue:[def lastPathComponent]];
  }

  NSInteger result = [panel runModal];
  if (result == NSModalResponseOK) {
    NSArray *files = [panel URLs];
    NSString *file = ([files count] > 0 ? [[files objectAtIndex:0] path] : @"");

    file = [file stringByAbbreviatingWithTildeInPath];
    [txt setStringValue:file];

    // Fuck me!  Just setting the value of the NSTextField does not cause
    // that to end up in the preferences!
    //
    [[txt window] makeFirstResponder:nil];  // And this doesn't fix it.

    // So set the value manually.
    NSDictionary *dict = [txt infoForBinding:@"value"];
    NSUserDefaultsController *prefs = [dict objectForKey:@"NSObservedObject"];
    NSString *path = [dict objectForKey:@"NSObservedKeyPath"];
    if ([path hasPrefix:@"values."])  // WTF.
      path = [path substringFromIndex:7];
    [[prefs values] setValue:file forKey:path];

    if (imagedir_p) {
      /* Validate the new value of "imageDirectory" if it has changed.  If we
         didn't do this here it would still happen when the OK button of the
         settings panel was pressed, but doing it as soon as the file selector
         dialog is closed is more timely.  Note that in this case we are
         validating a pathanme that has not yet been written to the resource
         database: that doesn't happen until they click OK (and not Cancel). */
      [self validateImageDirectory: file];
    }
  }
}


/* Returns the NSTextField that is to the left of or above the NSButton.
 */
static NSTextField *
find_text_field_of_button (NSButton *button)
{
  NSView *parent = [button superview];
  NSArray *kids = [parent subviews];
  NSUInteger nkids = [kids count];
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


- (void) fileSelectorChooseAction:(NSObject *)arg
{
  NSButton *choose = (NSButton *) arg;
  NSTextField *txt = find_text_field_of_button (choose);
  [self doFileSelector: txt dirs:NO imagedir:NO];
}

- (void) fileSelectorChooseDirsAction:(NSObject *)arg
{
  NSButton *choose = (NSButton *) arg;
  NSTextField *txt = find_text_field_of_button (choose);
  [self doFileSelector: txt dirs:YES imagedir:NO];
}

- (void) fileSelectorChooseImageDirAction:(NSObject *)arg
{
  NSButton *choose = (NSButton *) arg;
  NSTextField *txt = find_text_field_of_button (choose);
  [self doFileSelector: txt dirs:YES imagedir:YES];
}

#endif // !HAVE_IPHONE


- (void) makeTextLoaderControlBox:(NSXMLNode *)node on:(NSView *)parent
{
# ifndef HAVE_IPHONE
  /*
    Display Text:
     (x)  Computer name and time
     ( )  Text       [__________________________]
     ( )  Text file  [_________________] [Choose]
     ( )  URL        [__________________________]
     ( )  Shell Cmd  [__________________________]

    textMode -text-mode date
    textMode -text-mode literal   textLiteral -text-literal %
    textMode -text-mode file      textFile    -text-file %
    textMode -text-mode url       textURL     -text-url %
    textMode -text-mode program   textProgram -text-program %
   */
  NSRect rect;
  rect.size.width = rect.size.height = 1;
  rect.origin.x = rect.origin.y = 0;
  NSView *group  = [[NSView alloc] initWithFrame:rect];
  NSView *rgroup = [[NSView alloc] initWithFrame:rect];

  Bool program_p = TRUE;


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
                       numberOfRows: 4 + (program_p ? 1 : 0)
                       numberOfColumns:1];
  [matrix setAllowsEmptySelection:NO];

  NSArrayController *cnames  = [[NSArrayController alloc] initWithContent:nil];
  [cnames addObject:@"Computer name and time"];
  [cnames addObject:@"Text"];
  [cnames addObject:@"File"];
  [cnames addObject:@"URL"];
  if (program_p) [cnames addObject:@"Shell Cmd"];
  [matrix bind:@"content"
          toObject:cnames
          withKeyPath:@"arrangedObjects"
          options:nil];
  [cnames release];

  [self bindSwitch:matrix cmdline:@"-text-mode %"];

  [self placeChild:matrix on:group];
  [self placeChild:rgroup on:group right:YES];
  [proto release];
  [matrix release];
  [rgroup release];

  NSXMLNode *node2;

# else  // HAVE_IPHONE

  NSView *rgroup = parent;
  NSXMLNode *node2;

  // <select id="textMode">
  //   <option id="date"  _label="Display date" arg-set="-text-mode date"/>
  //   <option id="text"  _label="Display text" arg-set="-text-mode literal"/>
  //   <option id="url"   _label="Display URL"/>
  // </select>

  node2 = [[NSXMLElement alloc] initWithName:@"select"];
  [node2 setAttributesAsDictionary:@{ @"id": @"textMode" }];

  NSXMLNode *node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id":	  @"date",
              @"arg-set": @"-text-mode date",
              @"_label":  @"Display the date and time" }];
  [node3 setParent: node2];
  [node3 autorelease];

  node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id":      @"text",
              @"arg-set": @"-text-mode literal",
              @"_label":  @"Display static text" }];
  [node3 setParent: node2];
  [node3 autorelease];

  node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id":     @"url",                           
              @"_label": @"Display the contents of a URL" }];
  [node3 setParent: node2];
  [node3 autorelease];

  [self makeOptionMenu:node2 on:rgroup];
  [node2 release];

# endif // HAVE_IPHONE


  //  <string id="textLiteral" _label="" arg-set="-text-literal %"/>
  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
           @{ @"id":     @"textLiteral",
              @"arg":    @"-text-literal %",
# ifdef HAVE_IPHONE
              @"_label": @"Text to display"
# endif
            }];
  [self makeTextField:node2 on:rgroup 
# ifndef HAVE_IPHONE
        withLabel:NO
# else
        withLabel:YES
# endif
        horizontal:NO];
  [node2 release];

//  rect = [last_child(rgroup) frame];

/* // trying to make the text fields be enabled only when the checkbox is on..
  control = last_child (rgroup);
  [control bind:@"enabled"
           toObject:[matrix cellAtRow:1 column:0]
           withKeyPath:@"value"
           options:nil];
 */


# ifndef HAVE_IPHONE
  //  <file id="textFile" _label="" arg-set="-text-file %"/>
  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
           @{ @"id":  @"textFile",
              @"arg": @"-text-file %" }];
  [self makeFileSelector:node2 on:rgroup
        dirsOnly:NO withLabel:NO editable:NO imagedir:NO];
  [node2 release];
# endif // !HAVE_IPHONE

//  rect = [last_child(rgroup) frame];

  //  <string id="textURL" _label="" arg-set="text-url %"/>
  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
           @{ @"id":     @"textURL",            
              @"arg":    @"-text-url %",
# ifdef HAVE_IPHONE
              @"_label": @"URL to display",     
# endif
            }];
  [self makeTextField:node2 on:rgroup 
# ifndef HAVE_IPHONE
        withLabel:NO
# else
        withLabel:YES
# endif
        horizontal:NO];
  [node2 release];

//  rect = [last_child(rgroup) frame];

# ifndef HAVE_IPHONE
  if (program_p) {
    //  <string id="textProgram" _label="" arg-set="text-program %"/>
    node2 = [[NSXMLElement alloc] initWithName:@"string"];
    [node2 setAttributesAsDictionary:
             @{ @"id":   @"textProgram",
                 @"arg": @"-text-program %",
              }];
    [self makeTextField:node2 on:rgroup withLabel:NO horizontal:NO];
    [node2 release];
  }

//  rect = [last_child(rgroup) frame];

  layout_group (rgroup, NO);

  rect = [rgroup frame];
  rect.size.width += 35;    // WTF?  Why is rgroup too narrow?
  [rgroup setFrame:rect];


  // Set the height of the cells in the radio-box matrix to the height of
  // the (last of the) text fields.
  control = last_child (rgroup);
  rect = [control frame];
  rect.size.width = 30;  // width of the string "Text", plus a bit...
  if (program_p)
    rect.size.width += 25;
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
  rect.size.width = 300;
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
  [box setTitle:NSLocalizedString(@"Display Text", @"")];

  rect.size.width = rect.size.height = 12;
  [box setContentViewMargins:rect.size];
  [box setContentView:group];
  [box sizeToFit];

  [self placeChild:box on:parent];
  [group release];
  [box release];

# endif // !HAVE_IPHONE
}


- (void) makeImageLoaderControlBox:(NSXMLNode *)node on:(NSView *)parent
{
  /*
    [x]  Grab desktop images
    [ ]  Choose random image:
         [__________________________]  [Choose]

   <boolean id="grabDesktopImages" _label="Grab desktop images"
       arg-unset="-no-grab-desktop"/>
   <boolean id="chooseRandomImages" _label="Grab desktop images"
       arg-unset="-choose-random-images"/>
   <file id="imageDirectory" _label="" arg-set="-image-directory %"/>
   */

  NSXMLElement *node2;

# ifndef HAVE_IPHONE
#  define SCREENS "Grab desktop images"
#  define PHOTOS  "Choose random images"
# else
#  define SCREENS "Grab screenshots"
#  define PHOTOS  "Use photo library"
# endif

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
           @{ @"id":        @"grabDesktopImages",
              @"_label":    @ SCREENS,
              @"arg-unset": @"-no-grab-desktop",
            }];
  [self makeCheckbox:node2 on:parent];
  [node2 release];

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
           @{ @"id":      @"chooseRandomImages",
              @"_label":  @ PHOTOS,
              @"arg-set": @"-choose-random-images",
            }];
  [self makeCheckbox:node2 on:parent];
  [node2 release];

  node2 = [[NSXMLElement alloc] initWithName:@"string"];
  [node2 setAttributesAsDictionary:
           @{ @"id":     @"imageDirectory",
              @"_label": @"Images from:",
              @"arg":    @"-image-directory %",
            }];
  [self makeFileSelector:node2 on:parent
        dirsOnly:YES withLabel:YES editable:YES imagedir:YES];
  [node2 release];

# undef SCREENS
# undef PHOTOS

# ifndef HAVE_IPHONE
  // Add a second, explanatory label below the file/URL selector.

  LABEL *lab2 = 0;
  lab2 = [self makeLabel:@"(Local folder, or URL of RSS or Atom feed)"];
  [self placeChild:lab2 on:parent];

  // Pack it in a little tighter vertically.
  NSRect r2 = [lab2 frame];
  r2.origin.x += 20;
  r2.origin.y += 14;
  [lab2 setFrameOrigin:r2.origin];
# endif // HAVE_IPHONE
}


- (void) makeUpdaterControlBox:(NSXMLNode *)node on:(NSView *)parent
{
# ifndef HAVE_IPHONE
  /*
    [x]  Check for Updates  [ Monthly ]

  <hgroup>
   <boolean id="SUAutomaticallyUpdate"
            _label="Automatically check for updates"
            arg-unset="-no-SUAutomaticallyUpdate" />
   <select id="SUScheduledCheckInterval">
    <option="hourly" _label="Hourly" arg-set="-SUScheduledCheckInterval 3600"/>
    <option="daily"  _label="Daily"  arg-set="-SUScheduledCheckInterval 86400"/>
    <option="weekly" _label="Weekly" arg-set="-SUScheduledCheckInterval 604800"/>
    <option="monthly" _label="Monthly" arg-set="-SUScheduledCheckInterval 2629800"/>
   </select>
  </hgroup>
   */

  // <hgroup>

  NSRect rect;
  rect.size.width = rect.size.height = 1;
  rect.origin.x = rect.origin.y = 0;
  NSView *group = [[NSView alloc] initWithFrame:rect];

  NSXMLElement *node2;

  // <boolean ...>

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
           @{ @"id":        @SUSUEnableAutomaticChecksKey,
              @"_label":    @"Automatically check for updates",
              @"arg-unset": @"-no-" SUSUEnableAutomaticChecksKey,
              @"disabled":  (haveUpdater ? @"no" : @"yes")
            }];
  [self makeCheckbox:node2 on:group];
  [node2 release];

  // <select ...>

  node2 = [[NSXMLElement alloc] initWithName:@"select"];
  [node2 setAttributesAsDictionary:
           @{ @"id": @SUScheduledCheckIntervalKey }];

  //   <option ...>

  NSXMLNode *node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id":      @"hourly",
              @"arg-set": @"-" SUScheduledCheckIntervalKey " 3600",
              @"_label":  @"Hourly" }];
  [node3 setParent: node2];
  [node3 autorelease];

  node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id":      @"daily",
              @"arg-set": @"-" SUScheduledCheckIntervalKey " 86400",
              @"_label":  @"Daily" }];
  [node3 setParent: node2];
  [node3 autorelease];

  node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id": @"weekly",
           // @"arg-set": @"-" SUScheduledCheckIntervalKey " 604800",
              @"_label": @"Weekly",
            }];
  [node3 setParent: node2];
  [node3 autorelease];

  node3 = [[NSXMLElement alloc] initWithName:@"option"];
  [node3 setAttributesAsDictionary:
           @{ @"id":      @"monthly",
              @"arg-set": @"-" SUScheduledCheckIntervalKey " 2629800",
              @"_label":  @"Monthly",
             }];
  [node3 setParent: node2];
  [node3 autorelease];

  // </option>
  [self makeOptionMenu:node2 on:group disabled:!haveUpdater];
  [node2 release];

  // </hgroup>
  layout_group (group, TRUE);

  if (!haveUpdater) {
    // Add a second, explanatory label.
    LABEL *lab2 = 0;
    lab2 = [self makeLabel:@"XScreenSaverUpdater.app is not installed!\n"
                            "Unable to check for updates."];
    [self placeChild:lab2 on:group];

    // Pack it in a little tighter vertically.
    NSRect r2 = [lab2 frame];
    r2.origin.x += -4;
    r2.origin.y += 14;
    [lab2 setFrameOrigin:r2.origin];
  }

  rect.size.width = rect.size.height = 0;
  NSBox *box = [[NSBox alloc] initWithFrame:rect];
  [box setTitlePosition:NSNoTitle];
  [box setBorderType:NSNoBorder];
  [box setContentViewMargins:rect.size];
  [box setContentView:group];
  [box sizeToFit];

  [self placeChild:box on:parent];

  [group release];
  [box release];

# endif // !HAVE_IPHONE
}


#ifdef HAVE_IPHONE

- (void) makeCycleSaverControlBox:(NSXMLNode *)node on:(NSView *)parent
{
  /*
    Select a new saver after timeout     [x]
          [--------------------------------]
    Include this saver in "random" mode  [x]

   <boolean id="globalCycle"
            _label="Select a new saver after timeout"
            arg-set="-global-cycle" />
    <number id="globalCycleTimeout" type="slider"
            arg="-global-cycle-timeout %"
            _label="After"
            _low-label="5 minutes"
            _high-label="4 hours"
            low="300" high="14400" default="3600" step="300" />
   <boolean id="globalCycleSelected"
            _label="Include this saver in \"random\" mode"
            arg-set="-global-cycle-selected" />
   */

  NSXMLElement *node2;

  [self placeSeparator];

  // <boolean ...>

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
           @{ @"id":        @"globalCycle",
              @"_label":    @"Select a new saver after timeout",
              @"arg-unset": @"-no-global-cycle"
            }];
  [self makeCheckbox:node2 on:parent];
  [node2 release];

  // <number ...>

  node2 = [[NSXMLElement alloc] initWithName:@"number"];
  [node2 setAttributesAsDictionary:
           @{ @"id":          @"globalCycleTimeout",
              @"type":        @"slider",
              @"_label":      @"",
              @"arg":         @"-global-cycle-timeout %",
              @"_low-label":  @"5 minutes",
              @"_high-label": @"4 hours",
              @"low":         @"300",
              @"high":        @"14400",
              @"default":     @"4600",
              @"step":        @"300",
             }];
  [self makeNumberSelector:node2 on:parent];
  [node2 release];

  // <boolean ...>

  node2 = [[NSXMLElement alloc] initWithName:@"boolean"];
  [node2 setAttributesAsDictionary:
           @{ @"id":        @"globalCycleSelected",
              @"_label":    @"Include this saver in \"random\" mode",
              @"arg-unset": @"-no-global-cycle-selected"
            }];
  [self makeCheckbox:node2 on:parent];
  [node2 release];
}
#endif // HAVE_IPHONE


#pragma mark Layout for controls


# ifndef HAVE_IPHONE
static NSView *
last_child (NSView *parent)
{
  NSArray *kids = [parent subviews];
  NSUInteger nkids = [kids count];
  if (nkids == 0)
    return 0;
  else
    return [kids objectAtIndex:nkids-1];
}
#endif // HAVE_IPHONE


/* Add the child as a subview of the parent, positioning it immediately
   below or to the right of the previously-added child of that view.
 */
- (void) placeChild:
# ifdef HAVE_IPHONE
	(NSObject *)child
# else
	(NSView *)child
# endif
	on:(NSView *)parent right:(BOOL)right_p
{
# ifndef HAVE_IPHONE
  NSRect rect = [child frame];
  NSView *last = last_child (parent);
  if (!last) {
    rect.origin.x = LEFT_MARGIN;
    rect.origin.y = ([parent frame].size.height - rect.size.height 
                     - LINE_SPACING);
  } else if (right_p) {
    rect = [last frame];
    rect.origin.x += rect.size.width + COLUMN_SPACING;
  } else {
    rect = [last frame];
    rect.origin.x = LEFT_MARGIN;
    rect.origin.y -= [child frame].size.height + LINE_SPACING;
  }
  NSRect r = [child frame];
  r.origin = rect.origin;
  [child setFrame:r];
  [parent addSubview:child];

# else // HAVE_IPHONE

  /* Controls is an array of arrays of the controls, divided into sections.
     Each hgroup / vgroup gets a nested array, too, e.g.:

       [ [ [ <label>, <checkbox> ],
           [ <label>, <checkbox> ],
           [ <label>, <checkbox> ] ],
         [ <label>, <text-field> ],
         [ <label>, <low-label>, <slider>, <high-label> ],
         [ <low-label>, <slider>, <high-label> ],
         <HTML-label>
       ];

     If an element begins with a label, it is terminal, otherwise it is a
     group.  There are (currently) never more than 4 elements in a single
     terminal element.

     A blank vertical spacer is placed between each hgroup / vgroup,
     by making each of those a new section in the TableView.
   */
  if (! controls)
    controls = [[NSMutableArray arrayWithCapacity:10] retain];
  if ([controls count] == 0)
    [controls addObject: [NSMutableArray arrayWithCapacity:10]];
  NSMutableArray *current = [controls objectAtIndex:[controls count]-1];

  if (!right_p || [current count] == 0) {
    // Nothing on the current line. Add this object.
    [current addObject: child];
  } else {
    // Something's on the current line already.
    NSObject *old = [current objectAtIndex:[current count]-1];
    if ([old isKindOfClass:[NSMutableArray class]]) {
      // Already an array in this cell. Append.
      NSAssert ([(NSArray *) old count] < 4, @"internal error");
      [(NSMutableArray *) old addObject: child];
    } else {
      // Replace the control in this cell with an array, then append
      NSMutableArray *a = [NSMutableArray arrayWithObjects: old, child, nil];
      [current replaceObjectAtIndex:[current count]-1 withObject:a];
    }
  }
# endif // HAVE_IPHONE
}


- (void) placeChild:(NSView *)child on:(NSView *)parent
{
  [self placeChild:child on:parent right:NO];
}


#ifdef HAVE_IPHONE

// Start putting subsequent children in a new group, to create a new
// section on the UITableView.
//
- (void) placeSeparator
{
  if (! controls) return;
  if ([controls count] == 0) return;
  if ([[controls objectAtIndex:[controls count]-1]
        count] > 0)
    [controls addObject: [NSMutableArray arrayWithCapacity:10]];
}
#endif // HAVE_IPHONE



/* Creates an invisible NSBox (for layout purposes) to enclose the widgets
   wrapped in <hgroup> or <vgroup> in the XML.
 */
- (void) makeGroup:(NSXMLNode *)node 
                on:(NSView *)parent
        horizontal:(BOOL) horiz_p
{
# ifdef HAVE_IPHONE
  if (!horiz_p) [self placeSeparator];
  [self traverseChildren:node on:parent];
  if (!horiz_p) [self placeSeparator];
# else  // !HAVE_IPHONE
  NSRect rect;
  rect.size.width = rect.size.height = 1;
  rect.origin.x = rect.origin.y = 0;
  NSView *group = [[NSView alloc] initWithFrame:rect];
  [self traverseChildren:node on:group];

  layout_group (group, horiz_p);

  rect.size.width = rect.size.height = 0;
  NSBox *box = [[NSBox alloc] initWithFrame:rect];
  [box setTitlePosition:NSNoTitle];
  [box setBorderType:NSNoBorder];
  [box setContentViewMargins:rect.size];
  [box setContentView:group];
  [box sizeToFit];

  [self placeChild:box on:parent];
  [group release];
  [box release];
# endif // !HAVE_IPHONE
}


#ifndef HAVE_IPHONE
static void
layout_group (NSView *group, BOOL horiz_p)
{
  NSArray *kids = [group subviews];
  NSUInteger nkids = [kids count];
  NSUInteger i;
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
  rect.origin.x = 0;
  rect.origin.y = 0;
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
#endif // !HAVE_IPHONE


/* Create some kind of control corresponding to the given XML node.
 */
-(void)makeControl:(NSXMLNode *)node on:(NSView *)parent
{
  NSString *name = [node name];

  if ([node kind] == NSXMLCommentKind)
    return;

  if ([node kind] == NSXMLTextKind) {
    NSString *s = [(NSString *) [node objectValue]
                   stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (! [s isEqualToString:@""]) {
      NSAssert1 (0, @"unexpected text: %@", s);
    }
    return;
  }

  if ([node kind] != NSXMLElementKind) {
    NSAssert2 (0, @"weird XML node kind: %d: %@", (int)[node kind], node);
    return;
  }

  if ([name isEqualToString:@"hgroup"] ||
      [name isEqualToString:@"vgroup"]) {

    [self makeGroup:node on:parent 
          horizontal:[name isEqualToString:@"hgroup"]];

  } else if ([name isEqualToString:@"command"]) {
    // do nothing: this is the "-root" business

  } else if ([name isEqualToString:@"video"]) {
    // ignored

  } else if ([name isEqualToString:@"boolean"]) {
    [self makeCheckbox:node on:parent];

  } else if ([name isEqualToString:@"string"]) {
    [self makeTextField:node on:parent withLabel:NO horizontal:NO];

  } else if ([name isEqualToString:@"file"]) {
    [self makeFileSelector:node on:parent
          dirsOnly:NO withLabel:YES editable:NO imagedir:NO];

  } else if ([name isEqualToString:@"number"]) {
    [self makeNumberSelector:node on:parent];

  } else if ([name isEqualToString:@"select"]) {
    [self makeOptionMenu:node on:parent];

  } else if ([name isEqualToString:@"_description"]) {
    [self makeDescLabel:node on:parent];

  } else if ([name isEqualToString:@"xscreensaver-text"]) {
    [self makeTextLoaderControlBox:node on:parent];

  } else if ([name isEqualToString:@"xscreensaver-image"]) {
    [self makeImageLoaderControlBox:node on:parent];

  } else if ([name isEqualToString:@"xscreensaver-updater"]) {
    [self makeUpdaterControlBox:node on:parent];

  } else {
    NSAssert1 (0, @"unknown tag: %@", name);
  }
}


/* Iterate over and process the children of this XML node.
 */
- (void)traverseChildren:(NSXMLNode *)node on:(NSView *)parent
{
  NSArray *children = [node children];
  NSUInteger i, count = [children count];
  for (i = 0; i < count; i++) {
    NSXMLNode *child = [children objectAtIndex:i];
    [self makeControl:child on:parent];
  }
}


# ifndef HAVE_IPHONE

/* Kludgey magic to make the window enclose the controls we created.
 */
static void
fix_contentview_size (NSView *parent)
{
  NSRect f;
  NSArray *kids = [parent subviews];
  NSUInteger nkids = [kids count];
  NSView *text = 0;  // the NSText at the bottom of the window
  double maxx = 0, miny = 0;
  NSUInteger i;

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
//    NSLog(@"start: %3.0f x %3.0f @ %3.0f %3.0f  %3.0f  %@",
//          f.size.width, f.size.height, f.origin.x, f.origin.y,
//          f.origin.y + f.size.height, [kid class]);
  }
  
  if (maxx < 400) maxx = 400;   // leave room for the NSText paragraph...
  
  /* Now that we know the width of the window, set the width of the NSText to
     that, so that it can decide what its height needs to be.
   */
  if (! text) abort();
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
  
  // Lock the width of the field and unlock the height, and let it resize
  // once more, to compute the proper height of the text for that width.
  //
  [(NSText *) text setHorizontallyResizable:NO];
  [(NSText *) text setVerticallyResizable:YES];
  [(NSText *) text sizeToFit];

  // Now lock the height too: no more resizing this text field.
  //
  [(NSText *) text setVerticallyResizable:NO];

  // Now reposition the top edge of the text field to be back where it
  // was before we changed the height.
  //
  float oh = f.size.height;
  f = [text frame];
  float dh = f.size.height - oh;
  f.origin.y += dh;

  // #### This is needed in OSX 10.5, but is wrong in OSX 10.6.  WTF??
  //      If we do this in 10.6, the text field moves down, off the window.
  //      So instead we repair it at the end, at the "WTF2" comment.
  [text setFrame:f];

  // Also adjust the parent height by the change in height of the text field.
  miny -= dh;

//  NSLog(@"text new: %3.0f x %3.0f @ %3.0f %3.0f  %3.0f  %@",
//        f.size.width, f.size.height, f.origin.x, f.origin.y,
//        f.origin.y + f.size.height, [text class]);
  
  
  /* Set the contentView to the size of the children.
   */
  f = [parent frame];
//  float yoff = f.size.height;
  f.size.width = maxx + LEFT_MARGIN;
  f.size.height = -(miny - LEFT_MARGIN*2);
//  yoff = f.size.height - yoff;
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
  
/*
    Bad:
     parent: 420 x 541 @   0   0
     text:   380 x 100 @  20  22  miny=-501

    Good:
     parent: 420 x 541 @   0   0
     text:   380 x 100 @  20  50  miny=-501
 */

  // #### WTF2: See "WTF" above.  If the text field is off the screen,
  //      move it up.  We need this on 10.6 but not on 10.5.  Auugh.
  //
  f = [text frame];
  if (f.origin.y < 50) {    // magic numbers, yay
    f.origin.y = 50;
    [text setFrame:f];
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
# endif // !HAVE_IPHONE



#ifndef HAVE_IPHONE
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
  [reset setTitle:NSLocalizedString(@"Reset to Defaults", @"")];
  [reset setBezelStyle:NSRoundedBezelStyle];
  [reset sizeToFit];

  rect = [reset frame];
  NSButton *ok = [[NSButton alloc] initWithFrame:rect];
  [ok setTitle:NSLocalizedString(@"OK", @"")];
  [ok setBezelStyle:NSRoundedBezelStyle];
  [ok sizeToFit];
  rect = [bbox frame];
  rect.origin.x = rect.size.width - [ok frame].size.width;
  [ok setFrameOrigin:rect.origin];

  rect = [ok frame];
  NSButton *cancel = [[NSButton alloc] initWithFrame:rect];
  [cancel setTitle:NSLocalizedString(@"Cancel", @"")];
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
  [pbox setBorderType:NSBezelBorder];

  // Enforce a max height on the dialog, so that it's obvious to me
  // (on a big screen) when the dialog will fall off the bottom of
  // a small screen (e.g., 1024x768 laptop with a huge bottom dock).
# ifndef __OPTIMIZE__
  {
    NSRect f = [panel frame];
    int screen_height = (768    // shortest "modern" Mac display
                         - 22   // menu bar
                         - 56   // System Preferences toolbar
                         - 140  // default magnified bottom dock icon
                         );
    if (f.size.height > screen_height) {
      NSLog(@"%@ height was %.0f; clipping to %d", 
          [panel class], f.size.height, screen_height);
      f.size.height = screen_height;
      [panel setFrame:f];
    }
  }
# endif // !__OPTIMIZE__

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
  [ok     setAction:@selector(okAction:)];
  [cancel setAction:@selector(cancelAction:)];
  [reset  setAction:@selector(resetAction:)];
  
  [bbox release];

  return pbox;
}
#endif // !HAVE_IPHONE


/* Iterate over and process the children of the root node of the XML document.
 */
- (void)traverseTree
{
# ifdef HAVE_IPHONE
  NSView *parent = [self view];
# else
  NSWindow *parent = self;
#endif
  NSXMLNode *node = xml_root;

  if (![[node name] isEqualToString:@"screensaver"]) {
    NSAssert (0, @"top level node is not <xscreensaver>");
  }

  saver_title = [[self parseXScreenSaverTag: node] retain];
  
# ifndef HAVE_IPHONE

  NSRect rect;
  rect.origin.x = rect.origin.y = 0;
  rect.size.width = rect.size.height = 1;

  NSView *panel = [[NSView alloc] initWithFrame:rect];
  [self traverseChildren:node on:panel];
  fix_contentview_size (panel);

  NSView *root = wrap_with_buttons (parent, panel);
  [userDefaultsController   setAppliesImmediately:NO];
  [globalDefaultsController setAppliesImmediately:NO];

  [panel setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];

  rect = [parent frameRectForContentRect:[root frame]];
  [parent setFrame:rect display:NO];
  [parent setMinSize:rect.size];
  
  [parent setContentView:root];
	
  [panel release];
  [root release];

# else  // HAVE_IPHONE

  CGRect r = [parent frame];
  r.size = [[UIScreen mainScreen] bounds].size;
  [parent setFrame:r];

  [self traverseChildren:node on:parent];

# endif // HAVE_IPHONE
}


#ifdef HAVE_IPHONE
- (void)viewWillLayoutSubviews
{
  // Add left and right padding so that the lines aren't super wide.
  // The whitespace looks a little odd, but short lines are easier to read.
  //
  double max = 700;
  double w = self.tableView.frame.size.width;
  if (w > max) {
    double margin = (w - max) / 2;
# if 0  // Why doesn't this work?
    self.tableView.layoutMargins = UIEdgeInsetsMake (0, margin, 0, margin);
# else
    NSRect f = self.tableView.frame;
    f.origin.x += margin;
    f.size.width -= margin * 2;
    [self.tableView setFrame: f];

    // Use white instead of gray between sections.
    self.tableView.backgroundColor = [UIColor whiteColor];
# endif
  }

  // Make the left and right margins match our background color.
  self.tableView.superview.backgroundColor = self.tableView.backgroundColor;

}
#endif // HAVE_IPHONE


- (void)parser:(NSXMLParser *)parser
        didStartElement:(NSString *)elt
        namespaceURI:(NSString *)ns
        qualifiedName:(NSString *)qn
        attributes:(NSDictionary *)attrs
{
  NSXMLElement *e = [[NSXMLElement alloc] initWithName:elt];
  [e autorelease];
  [e setKind:SimpleXMLElementKind];
  [e setAttributesAsDictionary:attrs];
  NSXMLElement *p = xml_parsing;
  [e setParent:p];
  xml_parsing = e;
  if (! xml_root)
    xml_root = xml_parsing;
}

- (void)parser:(NSXMLParser *)parser
        didEndElement:(NSString *)elt
        namespaceURI:(NSString *)ns
        qualifiedName:(NSString *)qn
{
  NSXMLElement *p = xml_parsing;
  if (! p) {
    NSLog(@"extra close: %@", elt);
  } else if (![[p name] isEqualToString:elt]) {
    NSLog(@"%@ closed by %@", [p name], elt);
  } else {
    NSXMLElement *n = xml_parsing;
    xml_parsing = [n parent];
  }
}


- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string
{
  NSXMLElement *e = [[NSXMLElement alloc] initWithName:@"text"];
  [e setKind:SimpleXMLTextKind];
  NSXMLElement *p = xml_parsing;
  [e setParent:p];
  [e setObjectValue: string];
  [e autorelease];
}


# ifdef HAVE_IPHONE
# ifdef USE_PICKER_VIEW

#pragma mark UIPickerView delegate methods

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pv
{
  return 1;	// Columns
}

- (NSInteger)pickerView:(UIPickerView *)pv
             numberOfRowsInComponent:(NSInteger)column
{
  NSAssert (column == 0, @"weird column");
  NSArray *a = [picker_values objectAtIndex: [pv tag]];
  if (! a) return 0;  // Too early?
  return [a count];
}

- (CGFloat)pickerView:(UIPickerView *)pv
           rowHeightForComponent:(NSInteger)column
{
  return FONT_SIZE;
}

- (CGFloat)pickerView:(UIPickerView *)pv
           widthForComponent:(NSInteger)column
{
  NSAssert (column == 0, @"weird column");
  NSArray *a = [picker_values objectAtIndex: [pv tag]];
  if (! a) return 0;  // Too early?

  UIFont *f = [UIFont systemFontOfSize:[NSFont systemFontSize]];
  CGFloat max = 0;
  for (NSArray *a2 in a) {
    NSString *s = [a2 objectAtIndex:0];
    //CGSize r = [s sizeWithFont:f];
    CGSize r = pv.frame.size;
    r.height = 999999;
    r = [s boundingRectWithSize:r
                        options:NSStringDrawingUsesLineFragmentOrigin
                     attributes:@{NSFontAttributeName: f}
                        context:nil].size;

    if (r.width > max) max = r.width;
  }

  max *= 1.7;	// WTF!!

  if (max > 320)
    max = 320;
  else if (max < 120)
    max = 120;

  return max;

}


- (NSString *)pickerView:(UIPickerView *)pv
              titleForRow:(NSInteger)row
              forComponent:(NSInteger)column
{
  NSAssert (column == 0, @"weird column");
  NSArray *a = [picker_values objectAtIndex: [pv tag]];
  if (! a) return 0;  // Too early?
  a = [a objectAtIndex:row];
  NSAssert (a, @"internal error");
  return [a objectAtIndex:0];
}

# endif // USE_PICKER_VIEW


#pragma mark UITableView delegate methods

- (void) addResetButton
{
  [[self navigationItem] 
    setRightBarButtonItem: [[UIBarButtonItem alloc]
                             initWithTitle:
                               NSLocalizedString(@"Reset to Defaults", @"")
                             style: UIBarButtonItemStylePlain
                             target:self
                             action:@selector(resetAction:)]];
  NSString *s = saver_title;
  if ([self view].frame.size.width > 320)
    s = [s stringByAppendingString: @" Settings"];
  [self navigationItem].title = s;
}


# ifndef HAVE_TVOS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (BOOL)shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)o
{
  return YES;					/* Deprecated in iOS 6 */
}
#pragma clang diagnostic pop

- (BOOL)shouldAutorotate			/* Added in iOS 6 */
{
  return YES;
}
# endif // !HAVE_TVOS


- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv {
  // Number of vertically-stacked white boxes.
  return [controls count];
}

- (NSInteger)tableView:(UITableView *)tableView
             numberOfRowsInSection:(NSInteger)section
{
  // Number of lines in each vertically-stacked white box.
  NSAssert (controls, @"internal error");
  return [[controls objectAtIndex:section] count];
}

- (NSString *)tableView:(UITableView *)tv
              titleForHeaderInSection:(NSInteger)section
{
  // Titles above each vertically-stacked white box.
//  if (section == 0)
//    return [saver_title stringByAppendingString:@" Settings"];
  return nil;
}


- (CGFloat)tableView:(UITableView *)tv
           heightForRowAtIndexPath:(NSIndexPath *)ip
{
  CGFloat h = 0;

  NSView *ctl = [[controls objectAtIndex:[ip indexAtPosition:0]]
                  objectAtIndex:[ip indexAtPosition:1]];

  if ([ctl isKindOfClass:[NSArray class]]) {
    NSArray *set = (NSArray *) ctl;
    switch ([set count]) {
    case 4:			// label + left/slider/right.
    case 3:			// left/slider/right.
      h = FONT_SIZE * 3.0;
      break;
    case 2:			// Checkboxes, or text fields.
      h = FONT_SIZE * 2.4;
      break;
    }
  } else if ([ctl isKindOfClass:[UILabel class]]) {
    // Radio buttons in a multi-select list.
    h = FONT_SIZE * 1.9;

# ifdef USE_HTML_LABELS
  } else if ([ctl isKindOfClass:[HTMLLabel class]]) {
    
    HTMLLabel *t = (HTMLLabel *) ctl;
    CGRect r = t.frame;
    r.size.width = [tv frame].size.width;
    r.size.width -= LEFT_MARGIN * 2;
    [t setFrame:r];
    [t sizeToFit];
    r = t.frame;
    h = r.size.height;
# endif // USE_HTML_LABELS

# ifdef USE_PICKER_VIEW
  } else if ([ctl isKindOfClass:[UIPickerView class]]) {
    UIPickerView *pv = (UIPickerView *) ctl;
    NSArray *a = [picker_values objectAtIndex: [pv tag]];
    int lines = [a count];
    lines = 3;
    h = FONT_SIZE * lines + LINE_SPACING * 2;
# endif

  } else {			// Does this ever happen?
    h = FONT_SIZE + LINE_SPACING * 2;
  }

  if (h <= 0) abort();
  return h;
}


- (void)refreshTableView
{
  UITableView *tv = (UITableView *) [self view];
  NSMutableArray *a = [NSMutableArray arrayWithCapacity:20];
  NSInteger rows = [self numberOfSectionsInTableView:tv];
  for (int i = 0; i < rows; i++) {
    NSInteger cols = [self tableView:tv numberOfRowsInSection:i];
    for (int j = 0; j < cols; j++) {
      NSUInteger ip[2];
      ip[0] = i;
      ip[1] = j;
      [a addObject: [NSIndexPath indexPathWithIndexes:ip length:2]];
    }
  }

  [tv beginUpdates];
  [tv reloadRowsAtIndexPaths:a withRowAnimation:UITableViewRowAnimationNone];
  [tv endUpdates];

  // Default opacity looks bad.
  // #### Oh great, this only works *sometimes*.
  UIView *v = [[self navigationItem] titleView];
  [v setBackgroundColor:[[v backgroundColor] colorWithAlphaComponent:1]];
}


#pragma clang diagnostic push	 /* Deprecated in iOS 8 */
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)o
{
  [NSTimer scheduledTimerWithTimeInterval: 0
           target:self
           selector:@selector(refreshTableView)
           userInfo:nil
           repeats:NO];
}
#pragma clang diagnostic pop


#ifndef USE_PICKER_VIEW

- (void)updateRadioGroupCell:(UITableViewCell *)cell
                      button:(RadioButton *)b
{
  NSArray *item = [[b items] objectAtIndex: [b index]];
  NSString *pref_key = [item objectAtIndex:1];
  NSObject *pref_val = [item objectAtIndex:2];

  NSObject *current = [[self controllerForKey:pref_key] objectForKey:pref_key];

  // Convert them both to strings and compare those, so that
  // we don't get screwed by int 1 versus string "1".
  // Will boolean true/1 screw us here too?
  //
  NSString *pref_str = ([pref_val isKindOfClass:[NSString class]]
                        ? (NSString *) pref_val
                        : [(NSNumber *) pref_val stringValue]);
  NSString *current_str = ([current isKindOfClass:[NSString class]]
                           ? (NSString *) current
                           : [(NSNumber *) current stringValue]);
  BOOL match_p = [current_str isEqualToString:pref_str];

  // NSLog(@"\"%@\" = \"%@\" | \"%@\" ", pref_key, pref_val, current_str);

  if (match_p)
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  else
    [cell setAccessoryType:UITableViewCellAccessoryNone];
}


- (void)tableView:(UITableView *)tv
        didSelectRowAtIndexPath:(NSIndexPath *)ip
{
  RadioButton *ctl = [[controls objectAtIndex:[ip indexAtPosition:0]]
                       objectAtIndex:[ip indexAtPosition:1]];
  if (! [ctl isKindOfClass:[RadioButton class]])
    return;

  [self radioAction:ctl];
  [self refreshTableView];
}


#endif // !USE_PICKER_VIEW



- (UITableViewCell *)tableView:(UITableView *)tv
                     cellForRowAtIndexPath:(NSIndexPath *)ip
{
  CGFloat ww = [tv frame].size.width;
  CGFloat hh = [self tableView:tv heightForRowAtIndexPath:ip];

  float os_version = [[[UIDevice currentDevice] systemVersion] floatValue];

  // Width of the column of labels on the left.
  CGFloat left_width = ww * 0.4;
  CGFloat right_edge = ww - LEFT_MARGIN;

  if (os_version < 7)  // margins were wider on iOS 6.1
    right_edge -= 10;

  CGFloat max = FONT_SIZE * 12;
  if (left_width > max) left_width = max;

  NSView *ctl = [[controls objectAtIndex:[ip indexAtPosition:0]]
                           objectAtIndex:[ip indexAtPosition:1]];

  if ([ctl isKindOfClass:[NSArray class]]) {
    // This cell has a set of objects in it.
    NSArray *set = (NSArray *) ctl;
    switch ([set count]) {
    case 2:
      {
        // With 2 elements, the first of the pair must be a label.
        UILabel *label = (UILabel *) [set objectAtIndex: 0];
        NSAssert ([label isKindOfClass:[UILabel class]], @"unhandled type");
        ctl = [set objectAtIndex: 1];

        CGRect r = [ctl frame];

# ifndef HAVE_TVOS
        if ([ctl isKindOfClass:[UISwitch class]]) {	// Checkboxes.
          r.size.width = 80;  // Magic.
          r.origin.x = right_edge - r.size.width + 30;  // beats me

          if (os_version < 7)  // checkboxes were wider on iOS 6.1
            r.origin.x -= 25;

        } else
# endif // !HAVE_TVOS
        {
          r.origin.x = left_width;			// Text fields, etc.
          r.size.width = right_edge - r.origin.x;
        }

        r.origin.y = (hh - r.size.height) / 2;   // Center vertically.
        [ctl setFrame:r];

        // Make a box and put the label and checkbox/slider into it.
        r.origin.x = 0;
        r.origin.y = 0;
        r.size.width  = ww;
        r.size.height = hh;
        NSView *box = [[UIView alloc] initWithFrame:r];
        [box addSubview: ctl];

        // Let the label make use of any space not taken up by the control.
        r = [label frame];
        r.origin.x = LEFT_MARGIN;
        r.origin.y = 0;
        r.size.width  = [ctl frame].origin.x - r.origin.x;
        r.size.height = hh;
        [label setFrame:r];
        [label setFont:[NSFont boldSystemFontOfSize: FONT_SIZE]];
        [box addSubview: label];
        [box autorelease];

        ctl = box;
      }
      break;
    case 3:
    case 4:
      {
        // With 3 elements, 1 and 3 are labels.
        // With 4 elements, 1, 2 and 4 are labels.
        int i = 0;
        UILabel *top  = ([set count] == 4
                         ? [set objectAtIndex: i++]
                         : 0);
        UILabel *left  = [set objectAtIndex: i++];
        NSView  *mid   = [set objectAtIndex: i++];
        UILabel *right = [set objectAtIndex: i++];
        NSAssert (!top || [top   isKindOfClass:[UILabel class]], @"WTF");
        NSAssert (        [left  isKindOfClass:[UILabel class]], @"WTF");
        NSAssert (       ![mid   isKindOfClass:[UILabel class]], @"WTF");
        NSAssert (        [right isKindOfClass:[UILabel class]], @"WTF");

        // 3 elements: control at top of cell.
        // 4 elements: center the control vertically.
        CGRect r = [mid frame];
        r.size.height = 32;   // Unchangable height of the slider thumb.

        // Center the slider between left_width and right_edge.
# ifdef  LABEL_ABOVE_SLIDER
        r.origin.x = LEFT_MARGIN;
# else
        r.origin.x = left_width;
# endif
        r.origin.y = (hh - r.size.height) / 2;
        r.size.width = right_edge - r.origin.x;
        [mid setFrame:r];

        if (top) {
# ifdef LABEL_ABOVE_SLIDER
          // Top label goes above, flush center/top.
          r.origin.x = (ww - r.size.width) / 2;
          r.origin.y = 4;
          // #### sizeWithFont deprecated as of iOS 7; use boundingRectWithSize.
          r.size = [[top text] sizeWithFont:[top font]
                               constrainedToSize:
                                 CGSizeMake (ww - LEFT_MARGIN*2, 100000)
                               lineBreakMode:[top lineBreakMode]];
# else  // !LABEL_ABOVE_SLIDER
          // Label goes on the left.
          r.origin.x = LEFT_MARGIN;
          r.origin.y = 0;
          r.size.width  = left_width - LEFT_MARGIN;
          r.size.height = hh;
# endif // !LABEL_ABOVE_SLIDER
          [top setFrame:r];
        }

        // Left label goes under control, flush left/bottom.
        left.frame = CGRectMake([mid frame].origin.x, hh - 4,
                                ww - LEFT_MARGIN*2, 100000);
        [left sizeToFit];
        r = left.frame;
        r.origin.y -= r.size.height;
        left.frame = r;

        // Right label goes under control, flush right/bottom.
        right.frame =
          CGRectMake([mid frame].origin.x + [mid frame].size.width,
                     [left frame].origin.y, ww - LEFT_MARGIN*2, 1000000);
        [right sizeToFit];
        r = right.frame;
        r.origin.x -= r.size.width;
        right.frame = r;

        // Make a box and put the labels and slider into it.
        r.origin.x = 0;
        r.origin.y = 0;
        r.size.width  = ww;
        r.size.height = hh;
        NSView *box = [[UIView alloc] initWithFrame:r];
        if (top)
          [box addSubview: top];
        [box addSubview: left];
        [box addSubview: right];
        [box addSubview: mid];
        [box autorelease];

        ctl = box;
      }
      break;
    default:
      NSAssert (0, @"unhandled size");
    }
  } else {	// A single view, not a pair.
    CGRect r = [ctl frame];
    r.origin.x = LEFT_MARGIN;
    r.origin.y = 0;
    r.size.width = right_edge - r.origin.x;
    r.size.height = hh;
    [ctl setFrame:r];
  }

  NSString *id = @"Cell";
  UITableViewCell *cell = [tv dequeueReusableCellWithIdentifier:id];
  if (!cell)
    cell = [[[UITableViewCell alloc] initWithStyle: UITableViewCellStyleDefault
                                     reuseIdentifier: id]
             autorelease];

  for (UIView *subview in [cell.contentView subviews])
    [subview removeFromSuperview];
  [cell.contentView addSubview: ctl];
  CGRect r = [ctl frame];
  r.origin.x = 0;
  r.origin.y = 0;
  [cell setFrame:r];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  [cell setAccessoryType:UITableViewCellAccessoryNone];

# ifndef USE_PICKER_VIEW
  if ([ctl isKindOfClass:[RadioButton class]])
    [self updateRadioGroupCell:cell button:(RadioButton *)ctl];
# endif // USE_PICKER_VIEW

  return cell;
}
# endif  // HAVE_IPHONE


/* When this object is instantiated, it parses the XML file and creates
   controls on itself that are hooked up to the appropriate preferences.
   The default size of the view is just big enough to hold them all.
 */
- (id)initWithXML: (NSData *) xml_data
        classname: (NSString *) _classname
          options: (const XrmOptionDescRec *) _opts
       controller: (NSUserDefaultsController *) _prefs
 globalController: (NSUserDefaultsController *) _globalPrefs
         defaults: (NSDictionary *) _defs
      haveUpdater: (BOOL) _haveUpdater
{
# ifndef HAVE_IPHONE
  self = [super init];
# else  // !HAVE_IPHONE
  self = [super initWithStyle:UITableViewStyleGrouped];
# endif // !HAVE_IPHONE
  if (! self) return 0;

  // instance variables
  opts = _opts;
  defaultOptions = _defs;
  userDefaultsController   = [_prefs retain];
  globalDefaultsController = [_globalPrefs retain];
  classname = [_classname retain];
  haveUpdater = _haveUpdater;

  NSXMLParser *xmlDoc = [[NSXMLParser alloc] initWithData:xml_data];

  if (!xmlDoc) {
    NSAssert1 (0, @"XML Error: %@",
               [[NSString alloc] initWithData:xml_data
                                 encoding:NSUTF8StringEncoding]);
    return nil;
  }
  [xmlDoc setDelegate:self];
  if (! [xmlDoc parse]) {
    NSError *err = [xmlDoc parserError];
    NSAssert2 (0, @"XML Error: %@: %@",
               [[NSString alloc] initWithData:xml_data
                                 encoding:NSUTF8StringEncoding],
               err);
    return nil;
  }

# ifndef HAVE_IPHONE
  TextModeTransformer *t = [[TextModeTransformer alloc] init];
  [NSValueTransformer setValueTransformer:t
                      forName:@"TextModeTransformer"];
  [t release];

  NonNilStringTransformer *t2 = [[NonNilStringTransformer alloc] init];
  [NSValueTransformer setValueTransformer:t2
                      forName:@"NonNilStringTransformer"];
  [t2 release];
# endif // HAVE_IPHONE

  [self traverseTree];
  xml_root = 0;

# ifdef HAVE_IPHONE
  self.title = [saver_title stringByAppendingString:@" Settings"];
  [self addResetButton];
# endif

  return self;
}


- (void) dealloc
{
  [classname release];
  [saver_title release];
  [userDefaultsController release];
  [globalDefaultsController release];
# ifdef HAVE_IPHONE
  [controls release];
  [pref_keys release];
  [pref_ctls release];
#  ifdef USE_PICKER_VIEW
  [picker_values release];
#  endif
# endif
  [super dealloc];
}

@end
