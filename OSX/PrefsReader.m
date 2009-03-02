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

/* This implements the substrate of the xscreensaver preferences code:
   It does this by writing defaults to, and reading values from, the
   NSUserDefaultsController (and ScreenSaverDefaults/NSUserDefaults)
   and thereby reading the preferences that may have been edited by
   the UI (XScreenSaverConfigSheet).
 */

#import <ScreenSaver/ScreenSaverDefaults.h>
#import "PrefsReader.h"
#import "screenhackI.h"

@implementation PrefsReader

/* Converts an array of "key:value" strings to an NSDictionary.
 */
- (NSDictionary *) defaultsToDict: (const char * const *) defs
{
  NSDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:20];
  while (*defs) {
    char *line = strdup (*defs);
    char *key, *val;
    key = line;
    while (*key == '.' || *key == '*' || *key == ' ' || *key == '\t')
      key++;
    val = key;
    while (*val && *val != ':')
      val++;
    if (*val != ':') abort();
    *val++ = 0;
    while (*val == ' ' || *val == '\t')
      val++;

    int L = strlen(val);
    while (L > 0 && (val[L-1] == ' ' || val[L-1] == '\t'))
      val[--L] = 0;

    // When storing into preferences, look at the default string and
    // decide whether it's a boolean, int, float, or string, and store
    // an object of the appropriate type in the prefs.
    //
    NSString *nskey = [NSString stringWithCString:key
                                         encoding:NSUTF8StringEncoding];
    NSObject *nsval;
    int dd;
    double ff;
    char cc;
    if (!strcasecmp (val, "true") || !strcasecmp (val, "yes"))
      nsval = [NSNumber numberWithBool:YES];
    else if (!strcasecmp (val, "false") || !strcasecmp (val, "no"))
      nsval = [NSNumber numberWithBool:NO];
    else if (1 == sscanf (val, " %d %c", &dd, &cc))
      nsval = [NSNumber numberWithInt:dd];
    else if (1 == sscanf (val, " %lf %c", &ff, &cc))
      nsval = [NSNumber numberWithDouble:ff];
    else
      nsval = [NSString stringWithCString:val encoding:NSUTF8StringEncoding];
      
//    NSLog (@"default: \"%@\" = \"%@\" [%@]\n", nskey, nsval, [nsval class]);
    [dict setValue:nsval forKey:nskey];
    free (line);
    defs++;
  }
  return dict;
}


/* Initialize the Cocoa preferences database:
   - sets the default preferences values from the 'defaults' array;
   - binds 'self' to each preference as an observer;
   - ensures that nothing is mentioned in 'options' and not in 'defaults';
   - ensures that nothing is mentioned in 'defaults' and not in 'options'.
 */
- (void) registerXrmKeys: (const XrmOptionDescRec *) opts
                defaults: (const char * const *) defs
{
  // Store the contents of 'defaults' into the real preferences database.
  NSDictionary *defsdict = [self defaultsToDict:defs];
  [userDefaults registerDefaults:defsdict];

  userDefaultsController = 
    [[NSUserDefaultsController alloc] initWithDefaults:userDefaults
                                      initialValues:defsdict];

  NSDictionary *optsdict = [NSMutableDictionary dictionaryWithCapacity:20];

  while (opts[0].option) {
    //const char *option   = opts->option;
    const char *resource = opts->specifier;
    
    while (*resource == '.' || *resource == '*')
      resource++;
    NSString *nsresource = [NSString stringWithCString:resource
                                              encoding:NSUTF8StringEncoding];
    
    // make sure there's no resource mentioned in options and not defaults.
    if (![defsdict objectForKey:nsresource]) {
      if (! (!strcmp(resource, "font") ||    // don't warn about these
             !strcmp(resource, "textLiteral") ||
             !strcmp(resource, "textFile") ||
             !strcmp(resource, "textURL") ||
             !strcmp(resource, "imageDirectory")))
        NSLog (@"warning: \"%s\" is in options but not defaults", resource);
    }
    [optsdict setValue:nsresource forKey:nsresource];
    
    opts++;
  }

  // make sure there's no resource mentioned in defaults and not options.
  NSEnumerator *enumerator = [defsdict keyEnumerator];
  NSString *key;
  while ((key = [enumerator nextObject])) {
#if 0
    if (! [optsdict objectForKey:key])
      if (! ([key isEqualToString:@"foreground"] || // don't warn about these
             [key isEqualToString:@"background"] ||
             [key isEqualToString:@"Background"] ||
             [key isEqualToString:@"geometry"] ||
             [key isEqualToString:@"font"] ||
             [key isEqualToString:@"dontClearRoot"] ||

             // fps.c settings
             [key isEqualToString:@"fpsSolid"] ||
             [key isEqualToString:@"fpsTop"] ||
             [key isEqualToString:@"titleFont"] ||

             // analogtv.c settings
             [key isEqualToString:@"TVBrightness"] ||
             [key isEqualToString:@"TVColor"] ||
             [key isEqualToString:@"TVContrast"] ||
             [key isEqualToString:@"TVTint"]
             ))
      NSLog (@"warning: \"%@\" is in defaults but not options", key);
#endif /* 0 */
  }

}

- (NSUserDefaultsController *) userDefaultsController
{
  NSAssert(userDefaultsController, @"userDefaultsController uninitialized");
  return userDefaultsController;
}


- (NSObject *) getObjectResource: (const char *) name
{
  while (1) {
    NSString *key = [NSString stringWithCString:name
                                       encoding:NSUTF8StringEncoding];
    NSObject *obj = [userDefaults objectForKey:key];
    if (obj)
      return obj;

    // If key is "foo.bar.baz", check "foo.bar.baz", "bar.baz", and "baz".
    //
    const char *dot = strchr (name, '.');
    if (dot && dot[1])
      name = dot + 1;
    else
      return nil;
  }
}


- (char *) getStringResource: (const char *) name
{
  NSObject *o = [self getObjectResource:name];
  //NSLog(@"%s = %@\n",name,o);
  if (o == nil) {
    if (! (!strcmp(name, "eraseMode") || // erase.c
           // xlockmore.c reads all of these whether used or not...
           !strcmp(name, "right3d") ||
           !strcmp(name, "left3d") ||
           !strcmp(name, "both3d") ||
           !strcmp(name, "none3d") ||
           !strcmp(name, "font") ||
           !strcmp(name, "labelFont") ||  // grabclient.c
           !strcmp(name, "titleFont") ||
           !strcmp(name, "background")
           ))
      NSLog(@"warning: no preference \"%s\" [string]\n", name);
    return NULL;
  }
#if 0
  if (! [o isKindOfClass:[NSString class]]) {
    NSAssert2(0, @"%s = \"%@\" but should have been an NSString", name, o);
    abort();
  }
#else
  if (! [o isKindOfClass:[NSString class]]) {
    NSLog(@"asked for %s as a string, but it is a %@", name, [o class]);
    o = [(NSNumber *) o stringValue];
  }
#endif

  NSString *os = (NSString *) o;
  const char *result = [os cStringUsingEncoding:NSUTF8StringEncoding];
  return strdup (result);
}


- (double) getFloatResource: (const char *) name
{
  NSObject *o = [self getObjectResource:name];
  if (o == nil) {
    // xlockmore.c reads all of these whether used or not...
    if (! (!strcmp(name, "cycles") ||
           !strcmp(name, "size") ||
           !strcmp(name, "use3d") ||
           !strcmp(name, "delta3d") ||
           !strcmp(name, "wireframe") ||
           !strcmp(name, "showFPS") ||
           !strcmp(name, "fpsSolid") ||
           !strcmp(name, "fpsTop") ||
           !strcmp(name, "mono") ||
           !strcmp(name, "count") ||
           !strcmp(name, "ncolors") ||
           !strcmp(name, "eraseSeconds")  // erase.c
           ))
      NSLog(@"warning: no preference \"%s\" [float]\n", name);
    return 0.0;
  }
  if ([o isKindOfClass:[NSString class]]) {
    return [(NSString *) o doubleValue];
  } else if ([o isKindOfClass:[NSNumber class]]) {
    return [(NSNumber *) o doubleValue];
  } else {
    NSAssert2(0, @"%s = \"%@\" but should have been an NSNumber", name, o);
    abort();
  }
}


- (int) getIntegerResource: (const char *) name
{
  return (int) [self getFloatResource:name];
}


- (BOOL) getBooleanResource: (const char *) name
{
  int n = [self getIntegerResource:name];
  if (n == 0) return NO;
  else if (n == 1) return YES;
  else {
    NSAssert2(0, @"%s = %d but should have been 0 or 1\n", name, n);
    abort();
  }
}


- (id) initWithName: (NSString *) name
            xrmKeys: (const XrmOptionDescRec *) opts
           defaults: (const char * const *) defs
{
  self = [self init];
  if (!self) return nil;

  userDefaults = [ScreenSaverDefaults defaultsForModuleWithName:name];

  [self registerXrmKeys:opts defaults:defs];
  return self;
}

- (void) dealloc
{
  [userDefaultsController release];
  [super dealloc];
}

@end
