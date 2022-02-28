/* xscreensaver, Copyright © 2006-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a subclass of Apple's ScreenSaverView that knows how to run
   xscreensaver programs without X11 via the dark magic of the "jwxyz"
   library.  In xscreensaver terminology, this is the replacement for
   the "screenhack.c" module.
 */

#import <QuartzCore/QuartzCore.h>
#import <sys/mman.h>
#import <zlib.h>
#ifdef LOG_STACK
# include <execinfo.h>
#endif
#import "XScreenSaverView.h"
#import "XScreenSaverConfigSheet.h"
#import "Updater.h"
#import "screenhackI.h"
#import "pow2.h"
#import "jwxyzI.h"
#import "jwxyz-cocoa.h"
#import "jwxyz-timers.h"

#ifdef HAVE_IPHONE
// XScreenSaverView.m speaks OpenGL ES just fine, but enableBackbuffer does
// need (jwzgles_)gluCheckExtension.
# import "jwzglesI.h"
#else
# import <OpenGL/glu.h>
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* Duplicated in xlockmoreI.h and XScreenSaverGLView.m. */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

extern struct xscreensaver_function_table *xscreensaver_function_table;

/* Global variables used by the screen savers
 */
const char *progname;
const char *progclass;
int mono_p = 0;


# ifdef HAVE_IPHONE

#  define NSSizeToCGSize(x) (x)

extern NSDictionary *make_function_table_dict(void);  // ios-function-table.m

/* Stub definition of the superclass, for iPhone.
 */
@implementation ScreenSaverView
{
  NSTimeInterval anim_interval;
  Bool animating_p;
  NSTimer *anim_timer;
}

- (id)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview {
  self = [super initWithFrame:frame];
  if (! self) return 0;
  anim_interval = 1.0/30;
  return self;
}
- (NSTimeInterval)animationTimeInterval { return anim_interval; }
- (void)setAnimationTimeInterval:(NSTimeInterval)i { anim_interval = i; }
- (BOOL)hasConfigureSheet { return NO; }
- (NSWindow *)configureSheet { return nil; }
- (NSView *)configureView { return nil; }
- (BOOL)isPreview { return NO; }
- (BOOL)isAnimating { return animating_p; }
- (void)animateOneFrame { }

- (void)startAnimation {
  if (animating_p) return;
  animating_p = YES;
  anim_timer = [NSTimer scheduledTimerWithTimeInterval: anim_interval
                        target:self
                        selector:@selector(animateOneFrame)
                        userInfo:nil
                        repeats:YES];
}

- (void)stopAnimation {
  if (anim_timer) {
    [anim_timer invalidate];
    anim_timer = 0;
  }
  animating_p = NO;
}
@end

# endif // !HAVE_IPHONE



@interface XScreenSaverView (Private)
- (void) stopAndClose;
- (void) stopAndClose:(Bool)relaunch;
@end

@implementation XScreenSaverView

// Given a lower-cased saver name, returns the function table for it.
// If no name, guess the name from the class's bundle name.
//
- (struct xscreensaver_function_table *) findFunctionTable:(NSString *)title
{
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSAssert1 (nsb, @"no bundle for class %@", [self class]);

  NSString *path = [nsb bundlePath];
  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                               (CFStringRef) path,
                                               kCFURLPOSIXPathStyle,
                                               true);
  CFBundleRef cfb = CFBundleCreate (kCFAllocatorDefault, url);
  CFRelease (url);
  NSAssert1 (cfb, @"no CFBundle for \"%@\"", path);
  // #### Analyze says "Potential leak of an object stored into cfb"
  
# ifndef HAVE_IPHONE
  // CFBundleGetDataPointerForName doesn't work in "Archive" builds.
  // I'm guessing that symbol-stripping is mandatory.  Fuck.
  NSString *classname = [[nsb executablePath] lastPathComponent];
  NSString *table_name = [[classname lowercaseString]
                           stringByAppendingString:
                             @"_xscreensaver_function_table"];
  void *addr = CFBundleGetDataPointerForName (cfb, (CFStringRef) table_name);
  CFRelease (cfb);

  if (! addr)
    NSLog (@"no symbol \"%@\" for \"%@\"", table_name, path);

# else  // HAVE_IPHONE
  // Depends on the auto-generated "ios-function-table.m" being up to date.
  if (! function_tables)
    function_tables = [make_function_table_dict() retain];
  NSValue *v = [function_tables objectForKey: title];
  void *addr = v ? [v pointerValue] : 0;
# endif // HAVE_IPHONE

  return (struct xscreensaver_function_table *) addr;
}


// Add the "Contents/Resources/" subdirectory of this screen saver's .bundle
// to $PATH for the benefit of savers that include helper shell scripts.
//
- (void) setShellPath
{
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSAssert1 (nsb, @"no bundle for class %@", [self class]);
  
  NSString *nsrespath = [nsb resourcePath];    // "Contents/Resources"
  NSString *nsexepath = [[nsb executablePath]  // "Contents/MacOS/CLASSNAME"
                          stringByDeletingLastPathComponent];
  NSAssert1 (nsrespath, @"no resourcePath for class %@", [self class]);
  NSAssert1 (nsexepath, @"no executablePath for class %@", [self class]);
  const char *respath = [nsrespath cStringUsingEncoding:NSUTF8StringEncoding];
  const char *exepath = [nsexepath cStringUsingEncoding:NSUTF8StringEncoding];

  // Only read $PATH once, so that when running under Randomizer.m,
  // we don't keep adding more and more to it with each selected saver.
  static const char *opath = 0;
  if (!opath) {
    opath = getenv ("PATH");
    if (!opath) opath = "/bin"; // $PATH is unset when running under Shark!
    opath = strdup (opath);     // Leaks once, NBD.
  }

  char *npath = (char *) malloc (strlen (opath)   + 2 +
                                 strlen (respath) + 2 +
                                 strlen (exepath) + 2);
  strcpy (npath, exepath);
  strcat (npath, ":");
  strcat (npath, respath);
  strcat (npath, ":");
  strcat (npath, opath);
  if (setenv ("PATH", npath, 1)) {
    perror ("setenv");
    NSAssert1 (0, @"setenv \"PATH=%s\" failed", npath);
  }

  free (npath);
}


// set an $XSCREENSAVER_CLASSPATH variable so that included shell scripts
// (e.g., "xscreensaver-text") know how to look up resources.
//
- (void) setResourcesEnv:(NSString *) name
{
  const char *s = [name cStringUsingEncoding:NSUTF8StringEncoding];
  if (setenv ("XSCREENSAVER_CLASSPATH", s, 1)) {
    perror ("setenv");
    NSAssert1 (0, @"setenv \"XSCREENSAVER_CLASSPATH=%s\" failed", s);
  }
}


- (void) loadCustomFonts
{
# ifndef HAVE_IPHONE
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSMutableArray *fonts = [NSMutableArray arrayWithCapacity:20];
  for (NSString *ext in @[@"ttf", @"otf"]) {
    [fonts addObjectsFromArray: [nsb pathsForResourcesOfType:ext
                                     inDirectory:NULL]];
  }
  for (NSString *font in fonts) {
    CFURLRef url = (CFURLRef) [NSURL fileURLWithPath: font];
    CFErrorRef err = 0;
    if (! CTFontManagerRegisterFontsForURL (url, kCTFontManagerScopeProcess,
                                            &err)) {
      // Just ignore errors:
      // "The file has already been registered in the specified scope."
      // NSLog (@"loading font: %@ %@", url, err);
    }
  }
# endif // !HAVE_IPHONE
}


static void
add_default_options (const XrmOptionDescRec *opts,
                     const char * const *defs,
                     XrmOptionDescRec **opts_ret,
                     const char ***defs_ret)
{
  /* These aren't "real" command-line options (there are no actual command-line
     options in the Cocoa version); but this is the somewhat kludgey way that
     the <xscreensaver-text /> and <xscreensaver-image /> tags in the
     ../hacks/config/\*.xml files communicate with the preferences database.
  */
  static const XrmOptionDescRec default_options [] = {
    { "-text-mode",              ".textMode",          XrmoptionSepArg, 0 },
    { "-text-literal",           ".textLiteral",       XrmoptionSepArg, 0 },
    { "-text-file",              ".textFile",          XrmoptionSepArg, 0 },
    { "-text-url",               ".textURL",           XrmoptionSepArg, 0 },
    { "-text-program",           ".textProgram",       XrmoptionSepArg, 0 },
    { "-grab-desktop",           ".grabDesktopImages", XrmoptionNoArg, "True" },
    { "-no-grab-desktop",        ".grabDesktopImages", XrmoptionNoArg, "False"},
    { "-choose-random-images",   ".chooseRandomImages",XrmoptionNoArg, "True" },
    { "-no-choose-random-images",".chooseRandomImages",XrmoptionNoArg, "False"},
    { "-image-directory",        ".imageDirectory",    XrmoptionSepArg, 0 },
    { "-fps",                    ".doFPS",             XrmoptionNoArg, "True" },
    { "-no-fps",                 ".doFPS",             XrmoptionNoArg, "False"},
    { "-foreground",             ".foreground",        XrmoptionSepArg, 0 },
    { "-fg",                     ".foreground",        XrmoptionSepArg, 0 },
    { "-background",             ".background",        XrmoptionSepArg, 0 },
    { "-bg",                     ".background",        XrmoptionSepArg, 0 },

# ifndef HAVE_IPHONE
    // <xscreensaver-updater />
    {    "-" SUSUEnableAutomaticChecksKey,
         "." SUSUEnableAutomaticChecksKey, XrmoptionNoArg, "True"  },
    { "-no-" SUSUEnableAutomaticChecksKey,
         "." SUSUEnableAutomaticChecksKey, XrmoptionNoArg, "False" },
    {    "-" SUAutomaticallyUpdateKey,
         "." SUAutomaticallyUpdateKey, XrmoptionNoArg, "True"  },
    { "-no-" SUAutomaticallyUpdateKey,
         "." SUAutomaticallyUpdateKey, XrmoptionNoArg, "False" },
    {    "-" SUSendProfileInfoKey,
         "." SUSendProfileInfoKey, XrmoptionNoArg,"True" },
    { "-no-" SUSendProfileInfoKey,
         "." SUSendProfileInfoKey, XrmoptionNoArg,"False"},
    {    "-" SUScheduledCheckIntervalKey,
         "." SUScheduledCheckIntervalKey, XrmoptionSepArg, 0 },
# endif // !HAVE_IPHONE

# ifdef HAVE_IPHONE
    // Cycle mode
    { "-global-cycle",        ".globalCycle", XrmoptionNoArg, "True" },
    { "-no-global-cycle",     ".globalCycle", XrmoptionNoArg, "False" },
    { "-global-cycle-timeout",  ".globalCycleTimeout", XrmoptionSepArg, 0 },
    { "-global-cycle-selected", ".globalCycleSelected", XrmoptionNoArg,"True"},
    { "-no-global-cycle-selected", ".globalCycleSelected",XrmoptionNoArg,
      "False" },
# endif // HAVE_IPHONE

    { 0, 0, 0, 0 }
  };
  static const char *default_defaults [] = {

# if defined(HAVE_IPHONE) && !defined(__OPTIMIZE__)
    ".doFPS:              True",
# else
    ".doFPS:              False",
# endif
    ".doubleBuffer:       True",
    ".multiSample:        False",
    ".textMode:           url",
    ".textLiteral:        ",
    ".textFile:           ",
    ".textURL:            https://en.wikipedia.org/w/index.php?title=Special:NewPages&feed=rss",
    ".textProgram:        ",
    ".grabDesktopImages:  yes",
# ifndef HAVE_IPHONE
    ".chooseRandomImages: no",
# else
    ".chooseRandomImages: yes",
# endif
    ".imageDirectory:     ~/Pictures",
    ".relaunchDelay:      2",
    ".texFontCacheSize:   30",

# ifndef HAVE_IPHONE
#  define STR1(S) #S
#  define STR(S) STR1(S)
#  define __objc_yes Yes
#  define __objc_no  No
    "." SUSUEnableAutomaticChecksKey ": " STR(SUSUEnableAutomaticChecksDef),
    "." SUAutomaticallyUpdateKey ":  "    STR(SUAutomaticallyUpdateDef),
    "." SUSendProfileInfoKey ": "         STR(SUSendProfileInfoDef),
    "." SUScheduledCheckIntervalKey ": "  STR(SUScheduledCheckIntervalDef),
#  undef __objc_yes
#  undef __objc_no
#  undef STR1
#  undef STR
# endif // !HAVE_IPHONE

# ifdef HAVE_IPHONE
    ".globalCycle:         False",
    ".globalCycleTimeout:  300",
    ".globalCycleSelected: True",
# endif // HAVE_IPHONE
    0
  };

  int count = 0, i, j;
  for (i = 0; default_options[i].option; i++)
    count++;
  for (i = 0; opts[i].option; i++)
    count++;

  XrmOptionDescRec *opts2 = (XrmOptionDescRec *) 
    calloc (count + 1, sizeof (*opts2));

  i = 0;
  j = 0;
  while (default_options[j].option) {
    opts2[i] = default_options[j];
    i++, j++;
  }
  j = 0;
  while (opts[j].option) {
    opts2[i] = opts[j];
    i++, j++;
  }

  *opts_ret = opts2;


  /* now the defaults
   */
  count = 0;
  for (i = 0; default_defaults[i]; i++)
    count++;
  for (i = 0; defs[i]; i++)
    count++;

  const char **defs2 = (const char **) calloc (count + 1, sizeof (*defs2));

  i = 0;
  j = 0;
  while (default_defaults[j]) {
    defs2[i] = default_defaults[j];
    i++, j++;
  }
  j = 0;
  while (defs[j]) {
    defs2[i] = defs[j];
    i++, j++;
  }

  *defs_ret = defs2;
}


static void sighandler (int sig)
{
  const char *s = strsignal(sig);
  if (!s) s = "Unknown";
# ifdef HAVE_IPHONE
  jwxyz_abort ("Signal: %s", s);	// Throw NSException, show dialog
# else
  NSLog (@"Signal: %s", s);		// Just make sure it is logged

  // Log stack trace too.
  // Same info shows up in Library/Logs/DiagnosticReports/ScreenSaverEngine*
#  ifdef LOG_STACK
  void *stack [20];
  int frames = backtrace (stack, countof(stack));
  char **strs = backtrace_symbols (stack, frames);
  NSMutableArray *backtrace = [NSMutableArray arrayWithCapacity:frames];
  for (int i = 2; i < frames; i++) {
    if (strs[i])
      [backtrace addObject:[NSString stringWithUTF8String: strs[i]]];
  }
  // Can't embed newlines in the message for /usr/bin/log
  NSLog(@"Stack:\\n\t%@", [backtrace componentsJoinedByString:@"\\n\t"]);
  // free (strs);
#  endif // LOG_STACK

  signal (sig, SIG_DFL);
  kill (getpid (), sig);
# endif
}

static void
catch_signal (int sig, void (*handler) (int))
{
  struct sigaction a;
  a.sa_handler = handler;
  sigemptyset (&a.sa_mask);
  a.sa_flags = SA_NODEFER;
  if (sigaction (sig, &a, 0) < 0)
    {
      char buf [255];
      sprintf (buf, "%s: couldn't catch signal %d", progname, sig);
      NSLog (@"%s", buf);
    }
}

static void catch_signals (void)
{
//catch_signal (SIGINT,  sighandler);  // shell ^C
//catch_signal (SIGQUIT, sighandler);  // shell ^|
  catch_signal (SIGILL,  sighandler);
  catch_signal (SIGTRAP, sighandler);
  catch_signal (SIGABRT, sighandler);
  catch_signal (SIGEMT,  sighandler);
  catch_signal (SIGFPE,  sighandler);
  catch_signal (SIGBUS,  sighandler);
  catch_signal (SIGSEGV, sighandler);
  catch_signal (SIGSYS,  sighandler);
//catch_signal (SIGTERM, sighandler);  // kill default
//catch_signal (SIGKILL, sighandler);  // -9 untrappable
  catch_signal (SIGXCPU, sighandler);
  catch_signal (SIGXFSZ, sighandler);
  NSLog (@"installed signal handlers");
}


- (id) initWithFrame:(NSRect)frame
               title:(NSString *)_title
           isPreview:(BOOL)isPreview
{
  if (! (self = [super initWithFrame:frame isPreview:isPreview]))
    return 0;
  
  saver_title = [_title retain];
  catch_signals();
  xsft = [self findFunctionTable: saver_title];
  if (! xsft) {
    [self release];
    return 0;
  }

  [self setShellPath];

  setup_p = YES;
  if (xsft->setup_cb)
    xsft->setup_cb (xsft, xsft->setup_arg);

  /* The plist files for these preferences show up in
     $HOME/Library/Preferences/ByHost/ in a file named like
     "org.jwz.xscreensaver.<CLASSNAME>.<NUMBERS>.plist"
   */
  NSString *name = [NSString stringWithCString:xsft->progclass
                             encoding:NSUTF8StringEncoding];
  name = [@"org.jwz.xscreensaver." stringByAppendingString:name];
  [self setResourcesEnv:name];
  [self loadCustomFonts];
  
  XrmOptionDescRec *opts = 0;
  const char **defs = 0;
  add_default_options (xsft->options, xsft->defaults, &opts, &defs);
  prefsReader = [[PrefsReader alloc]
                  initWithName:name xrmKeys:opts defaults:defs];
  free (defs);
  // free (opts);  // bah, we need these! #### leak!
  xsft->options = opts;
  
  progname = progclass = xsft->progclass;

  next_frame_time = 0;

# if !defined HAVE_IPHONE && defined JWXYZ_QUARTZ
  // When the view fills the screen and double buffering is enabled, OS X will
  // use page flipping for a minor CPU/FPS boost. In windowed mode, double
  // buffering reduces the frame rate to 1/2 the screen's refresh rate.
  double_buffered_p = !isPreview;
# endif

# ifdef HAVE_IPHONE
  [self initGestures];

# ifndef HAVE_TVOS
  // So we can tell when we're docked.
  [UIDevice currentDevice].batteryMonitoringEnabled = YES;
# endif // !HAVE_TVOS

  [self setBackgroundColor:[NSColor blackColor]];
# endif // HAVE_IPHONE

# ifdef JWXYZ_QUARTZ
  // Colorspaces and CGContexts only happen with non-GL hacks.
  colorspace = CGColorSpaceCreateDeviceRGB ();
# endif

  return self;
}


#ifndef HAVE_IPHONE
/* On 10.15, if "use random screen saver" is checked, then startAnimation
   is never called.  This may be related to Apple's buggy code in 
   ScreenSaverEngine calling nonexistent beginExtensionRequestWithUserInfo,
   but on 10.15 we're not even running in that process: now we're in the
   not-at-all-ominously-named legacyScreenSaver process.

   Dec 2020, noticed that this also happens on 10.14.6 when *not* in random
   mode.  Both System Preferences and ScreenSaverEngine fail to call
   StartAnimation.
 */
- (void) viewDidMoveToWindow
{
  if (self.window)
    [self startAnimation];
}

- (void) viewWillMoveToWindow:(NSWindow *)window
{
  if (window == nil)
    [self stopAnimation];
}
#endif  // HAVE_IPHONE


#ifdef HAVE_IPHONE
+ (Class) layerClass
{
  return [CAEAGLLayer class];
}
#endif


- (id) initWithFrame:(NSRect)frame isPreview:(BOOL)p
{
  return [self initWithFrame:frame title:0 isPreview:p];
}


- (void) dealloc
{
  if ([self isAnimating])
    [self stopAnimation];
  NSAssert(!xdata, @"xdata not yet freed");
  NSAssert(!xdpy, @"xdpy not yet freed");

# ifdef HAVE_IPHONE
  [[NSNotificationCenter defaultCenter] removeObserver:self];
# endif

#  ifdef BACKBUFFER_OPENGL
# ifndef HAVE_IPHONE
  [pixfmt release];
# endif // !HAVE_IPHONE
  [ogl_ctx release];
  // Releasing the OpenGL context should also free any OpenGL objects,
  // including the backbuffer texture and frame/render/depthbuffers.
#  endif // BACKBUFFER_OPENGL

# if defined JWXYZ_GL && defined HAVE_IPHONE
  [ogl_ctx_pixmap release];
# endif // JWXYZ_GL

# ifdef JWXYZ_QUARTZ
  if (colorspace)
    CGColorSpaceRelease (colorspace);
# endif // JWXYZ_QUARTZ

  [prefsReader release];

  // xsft
  // fpst

  [super dealloc];
}

- (PrefsReader *) prefsReader
{
  return prefsReader;
}


#ifdef HAVE_IPHONE
- (void) lockFocus { }
- (void) unlockFocus { }
#endif // HAVE_IPHONE



# ifdef HAVE_IPHONE
/* A few seconds after the saver launches, we store the "wasRunning"
   preference.  This is so that if the saver is crashing at startup,
   we don't launch it again next time, getting stuck in a crash loop.
 */
- (void) allSystemsGo: (NSTimer *) timer
{
  NSAssert (timer == crash_timer, @"crash timer screwed up");
  crash_timer = 0;

  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  [prefs setBool:YES forKey:@"wasRunning"];
  [prefs synchronize];
}


- (void) resizeGL
{
  if (!ogl_ctx)
    return;

  CGSize screen_size = self.bounds.size;
  double s = self.contentScaleFactor;
  screen_size.width *= s;
  screen_size.height *= s;

#if defined JWXYZ_GL
  GLuint *framebuffer = &xwindow->gl_framebuffer;
  GLuint *renderbuffer = &xwindow->gl_renderbuffer;
  xwindow->window.current_drawable = xwindow;
#elif defined JWXYZ_QUARTZ
  GLuint *framebuffer = &gl_framebuffer;
  GLuint *renderbuffer = &gl_renderbuffer;
#endif // JWXYZ_QUARTZ

  if (*framebuffer)  glDeleteFramebuffersOES  (1, framebuffer);
  if (*renderbuffer) glDeleteRenderbuffersOES (1, renderbuffer);

  create_framebuffer (framebuffer, renderbuffer);

  //   redundant?
  //     glRenderbufferStorageOES (GL_RENDERBUFFER_OES, GL_RGBA8_OES,
  //                               (int)size.width, (int)size.height);
  [ogl_ctx renderbufferStorage:GL_RENDERBUFFER_OES
                  fromDrawable:(CAEAGLLayer*)self.layer];

  glFramebufferRenderbufferOES (GL_FRAMEBUFFER_OES,  GL_COLOR_ATTACHMENT0_OES,
                                GL_RENDERBUFFER_OES, *renderbuffer);

  [self addExtraRenderbuffers:screen_size];

  check_framebuffer_status();
}
#endif // HAVE_IPHONE


- (void) startAnimation
{
  if ([self isAnimating]) return;  // macOS 10.15 stupidity

  NSAssert(![self isAnimating], @"already animating");
  NSAssert(!initted_p && !xdata, @"already initialized");

  // See comment in render_x11() for why this value is important:
  [self setAnimationTimeInterval: 1.0 / 240.0];

  [super startAnimation];
  /* We can't draw on the window from this method, so we actually do the
     initialization of the screen saver (xsft->init_cb) in the first call
     to animateOneFrame() instead.
   */

# ifdef HAVE_IPHONE
  if (crash_timer)
    [crash_timer invalidate];

  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  [prefs removeObjectForKey:@"wasRunning"];
  [prefs synchronize];

  crash_timer = [NSTimer scheduledTimerWithTimeInterval: 5
                         target:self
                         selector:@selector(allSystemsGo:)
                         userInfo:nil
                         repeats:NO];

  if (cycle_timer)
    [cycle_timer invalidate];
  cycle_timer = 0;
# endif // HAVE_IPHONE

  // Never automatically turn the screen off if we are docked,
  // and an animation is running.
  //
# ifdef HAVE_IPHONE
#  ifndef HAVE_TVOS
  [UIApplication sharedApplication].idleTimerDisabled =
    ([UIDevice currentDevice].batteryState != UIDeviceBatteryStateUnplugged);
#  endif // !HAVE_TVOS
# endif

  xwindow = (Window) calloc (1, sizeof(*xwindow));
  xwindow->type = WINDOW;
  xwindow->window.view = self;
  CFRetain (xwindow->window.view);   // needed for garbage collection?

#ifdef BACKBUFFER_OPENGL
  CGSize new_backbuffer_size;

  {
# ifndef HAVE_IPHONE
    if (!ogl_ctx) {

      pixfmt = [self getGLPixelFormat];
      [pixfmt retain];

      NSAssert (pixfmt, @"unable to create NSOpenGLPixelFormat");

      // Fun: On OS X 10.7, the second time an OpenGL context is created, after
      // the preferences dialog is launched in SaverTester, the context only
      // lasts until the first full GC. Then it turns black. Solution is to
      // reuse the OpenGL context after this point.
      // "Analyze" says that both pixfmt and ogl_ctx are leaked.
      ogl_ctx = [[NSOpenGLContext alloc] initWithFormat:pixfmt
                                         shareContext:nil];

      // Sync refreshes to the vertical blanking interval
      GLint r = 1;
      [ogl_ctx setValues:&r forParameter:NSOpenGLCPSwapInterval];
//    check_gl_error ("NSOpenGLCPSwapInterval");  // SEGV sometimes. Too early?
    }

    [ogl_ctx makeCurrentContext];
    check_gl_error ("makeCurrentContext");

    // NSOpenGLContext logs an 'invalid drawable' when this is called
    // from initWithFrame.
    [ogl_ctx setView:self];

    // Get device pixels instead of points.
    self.wantsBestResolutionOpenGLSurface = YES;

    // This may not be necessary if there's FBO support.
#  ifdef JWXYZ_GL
    xwindow->window.pixfmt = pixfmt;
    CFRetain (xwindow->window.pixfmt);
    xwindow->window.virtual_screen = [ogl_ctx currentVirtualScreen];
    xwindow->window.current_drawable = xwindow;
    NSAssert (ogl_ctx, @"no CGContext");
#  endif

    // Clear frame buffer ASAP, else there are bits left over from other apps.
    glClearColor (0, 0, 0, 1);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glFinish ();
//    glXSwapBuffers (mi->dpy, mi->window);


    // Enable multi-threading, if possible.  This runs most OpenGL commands
    // and GPU management on a second CPU.
    {
#  ifndef  kCGLCEMPEngine
#   define kCGLCEMPEngine 313  // Added in MacOS 10.4.8 + XCode 2.4.
#  endif
      CGLContextObj cctx = CGLGetCurrentContext();
      CGLError err = CGLEnable (cctx, kCGLCEMPEngine);
      if (err != kCGLNoError) {
        NSLog (@"enabling multi-threaded OpenGL failed: %d", err);
      }
    }

    new_backbuffer_size = NSSizeToCGSize ([self bounds].size);

    // Scale factor for desktop retina displays
    double s = [self hackedContentScaleFactor];
    new_backbuffer_size.width *= s;
    new_backbuffer_size.height *= s;

# else  // HAVE_IPHONE
    if (!ogl_ctx) {
      CAEAGLLayer *eagl_layer = (CAEAGLLayer *) self.layer;
      eagl_layer.opaque = TRUE;
      eagl_layer.drawableProperties = [self getGLProperties];

      // Without this, the GL frame buffer is half the screen resolution!
      eagl_layer.contentsScale = [UIScreen mainScreen].scale;

      PrefsReader *prefs = [self prefsReader];
      BOOL gles3p = [prefs getBooleanResource:"prefersGLSL"];

      ogl_ctx = [[EAGLContext alloc] initWithAPI:
                                       (gles3p
                                        ? kEAGLRenderingAPIOpenGLES3
                                        : kEAGLRenderingAPIOpenGLES1)];
# ifdef JWXYZ_GL
      ogl_ctx_pixmap = [[EAGLContext alloc]
                        initWithAPI:kEAGLRenderingAPIOpenGLES1
                        sharegroup:ogl_ctx.sharegroup];
# endif // JWXYZ_GL

      eagl_layer.contentsGravity = [self getCAGravity];
    }

# ifdef JWXYZ_GL
    xwindow->window.ogl_ctx_pixmap = ogl_ctx_pixmap;
# endif // JWXYZ_GL

    [EAGLContext setCurrentContext: ogl_ctx];

    [self resizeGL];

    double s = [self hackedContentScaleFactor];
    new_backbuffer_size = self.bounds.size;
    new_backbuffer_size.width *= s;
    new_backbuffer_size.height *= s;

# endif // HAVE_IPHONE

# ifdef JWXYZ_GL
    xwindow->ogl_ctx = ogl_ctx;
#  ifndef HAVE_IPHONE
    CFRetain (xwindow->ogl_ctx);
#  endif // HAVE_IPHONE
# endif // JWXYZ_GL

    check_gl_error ("startAnimation");

//  NSLog (@"%s / %s / %s\n", glGetString (GL_VENDOR),
//         glGetString (GL_RENDERER), glGetString (GL_VERSION));

    [self enableBackbuffer:new_backbuffer_size];
  }
#endif // BACKBUFFER_OPENGL

  [self setViewport];
  [self createBackbuffer:new_backbuffer_size];
}


/* "The stopAnimation or startAnimation methods do not immediately start
   or stop animation. In particular, it is not safe to assume that your
   animateOneFrame method will not execute (or continue to execute) after
   you call stopAnimation." */


- (void)stopAnimation
{
# ifdef HAVE_IPHONE
  if (cycle_timer)
    [cycle_timer invalidate];
  cycle_timer = 0;
# endif // HAVE_IPHONE

  if (![self isAnimating]) return;  // macOS 10.15 stupidity

  if (initted_p) {

    [self lockFocus];       // in case something tries to draw from here
    [self prepareContext];

    /* All of the xlockmore hacks need to have their release functions
       called, or launching the same saver twice does not work.  Also
       webcollage-cocoa needs it in order to kill the inferior webcollage
       processes (since the screen saver framework never generates a
       SIGPIPE for them).
     */
     if (xdata)
       xsft->free_cb (xdpy, xwindow, xdata);
    [self unlockFocus];

    jwxyz_quartz_free_display (xdpy);
    xdpy = NULL;
# if defined JWXYZ_GL && !defined HAVE_IPHONE
    CFRelease (xwindow->ogl_ctx);
# endif
    CFRelease (xwindow->window.view);
    free (xwindow);
    xwindow = NULL;

//  setup_p = NO; // #### wait, do we need this?
    initted_p = NO;
    xdata = 0;
  }

# ifdef HAVE_IPHONE
  if (crash_timer)
    [crash_timer invalidate];
  crash_timer = 0;
  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  [prefs removeObjectForKey:@"wasRunning"];
  [prefs synchronize];
# endif // HAVE_IPHONE

  [super stopAnimation];

  // When an animation is no longer running (e.g., looking at the list)
  // then it's ok to power off the screen when docked.
  //
# ifdef HAVE_IPHONE
  [UIApplication sharedApplication].idleTimerDisabled = NO;
# endif

  // Without this, the GL frame stays on screen when switching tabs
  // in System Preferences.
  // (Or perhaps it used to. It doesn't seem to matter on 10.9.)
  //
# ifndef HAVE_IPHONE
  [NSOpenGLContext clearCurrentContext];
# endif // !HAVE_IPHONE

  clear_gl_error();	// This hack is defunct, don't let this linger.

# ifdef JWXYZ_QUARTZ
  CGContextRelease (backbuffer);
  backbuffer = nil;

  if (backbuffer_len)
    munmap (backbuffer_data, backbuffer_len);
  backbuffer_data = NULL;
  backbuffer_len = 0;
# endif
}


- (NSOpenGLContext *) oglContext
{
  return ogl_ctx;
}


// #### maybe this could/should just be on 'lockFocus' instead?
- (void) prepareContext
{
  if (xwindow) {
#ifdef HAVE_IPHONE
    [EAGLContext setCurrentContext:ogl_ctx];
#else  // !HAVE_IPHONE
    [ogl_ctx makeCurrentContext];
//    check_gl_error ("makeCurrentContext");
#endif // !HAVE_IPHONE

#ifdef JWXYZ_GL
    xwindow->window.current_drawable = xwindow;
#endif
  }
}


static void
screenhack_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure)
{
  fps_compute (fpst, 0, -1);
  fps_draw (fpst);
}


/* Some of the older X11 savers look bad if a "pixel" is not a thing you can
   see.  They expect big, chunky, luxurious 1990s pixels, and if they use
   "device" pixels on a Retina screen, everything just disappears.

   Retina iPads have 768x1024 point screens which are 1536x2048 pixels,
   2017 iMac screens are 5120x2880 in device pixels.

   This method is overridden in XScreenSaverGLView, since this kludge
   isn't necessary for GL programs, being resolution independent by
   nature.
 */
- (CGFloat) hackedContentScaleFactor
{
  return [self hackedContentScaleFactor:FALSE];
}

- (CGFloat) hackedContentScaleFactor:(BOOL)fonts_p
{
# ifdef HAVE_IPHONE
  CGFloat s = self.contentScaleFactor;
# else
  CGFloat s = self.window.backingScaleFactor;
# endif

  /* This notion of "scale fonts differently than the viewport seemed
     like it made sense for BSOD but it makes -fps text be stupidly
     large for all other hacks. So instead let's just make BSOD not
     be lowrez. There are no other lowrez hacks that make heavy use
     of fonts. */
  fonts_p = 0;

  if (_lowrez_p && !fonts_p) {
    NSSize b = [self bounds].size;  // This is in points, not pixels
    CGFloat wh = b.width > b.height ? b.width : b.height;
    wh *= s;  // points -> pixels

    // Scale down to as close to 1024 as we can get without going under,
    // while keeping an integral scale factor so that we don't get banding
    // artifacts and moire patterns.
    //
    // Retina sizes: 2208 => 1104, 2224 => 1112, 2732 => 1366, 2880 => 1440.
    //
    int s2 = wh / 1024;
    if (s2) s /= s2;
  }

  return s;
}


#ifdef HAVE_IPHONE

double
current_device_rotation (void)
{
# ifdef HAVE_TVOS
  return 0;
# else  // !HAVE_TVOS
  UIDeviceOrientation o = [[UIDevice currentDevice] orientation];

  /* Sometimes UIDevice doesn't know the proper orientation, or the device is
     face up/face down, so in those cases fall back to the status bar
     orientation. The SaverViewController tries to set the status bar to the
     proper orientation before it creates the XScreenSaverView; see
     _storedOrientation in SaverViewController.
   */
  if (o == UIDeviceOrientationUnknown ||
      o == UIDeviceOrientationFaceUp  ||
      o == UIDeviceOrientationFaceDown) {
    /* Mind the differences between UIInterfaceOrientation and
       UIDeviceOrientation:
       1. UIInterfaceOrientation does not include FaceUp and FaceDown.
       2. LandscapeLeft and LandscapeRight are swapped between the two. But
          converting between device and interface orientation doesn't need to
          take this into account, because (from the UIInterfaceOrientation
          description): "rotating the device requires rotating the content in
          the opposite direction."
	 */
    /* statusBarOrientation deprecated in iOS 9 */
    o = (UIDeviceOrientation)  // from UIInterfaceOrientation
      [UIApplication sharedApplication].statusBarOrientation;
  }

  switch (o) {
  case UIDeviceOrientationLandscapeLeft:      return -90; break;
  case UIDeviceOrientationLandscapeRight:     return  90; break;
  case UIDeviceOrientationPortraitUpsideDown: return 180; break;
  default:                                    return 0;   break;
  }
# endif // !HAVE_TVOS
}


- (void) handleException: (NSException *)e
{
  NSLog (@"Caught exception: %@", e);
  UIAlertController *c =
    [UIAlertController
      alertControllerWithTitle:
        [NSString stringWithFormat: @"%@ crashed!", saver_title]
      message: [NSString stringWithFormat:
                 @"The error message was:"
                  "\n\n%@\n\n"
                  "If it keeps crashing, try resetting its options.",
                  e]
      preferredStyle: UIAlertControllerStyleAlert];

  [c addAction: [UIAlertAction actionWithTitle:
                                 NSLocalizedString(@"Exit", @"")
                               style: UIAlertActionStyleDefault
                               handler: ^(UIAlertAction *a) {
        exit (-1);
      }]];
  [c addAction: [UIAlertAction actionWithTitle:
                                 NSLocalizedString(@"Keep going", @"")
                               style: UIAlertActionStyleDefault
                               handler: ^(UIAlertAction *a) {
        [self stopAndClose:NO];
      }]];

  UIViewController *vc =
    [UIApplication sharedApplication].keyWindow.rootViewController;
  while (vc.presentedViewController)
    vc = vc.presentedViewController;
  [vc presentViewController:c animated:YES completion:nil];
  [self stopAnimation];
}

#endif // HAVE_IPHONE


#ifdef JWXYZ_QUARTZ

# ifndef HAVE_IPHONE

struct gl_version
{
  // iOS always uses OpenGL ES 1.1.
  unsigned major;
  unsigned minor;
};

static GLboolean
gl_check_ver (const struct gl_version *caps,
              unsigned gl_major,
              unsigned gl_minor)
{
  return caps->major > gl_major ||
           (caps->major == gl_major && caps->minor >= gl_minor);
}

# endif

/* Called during startAnimation before the first call to createBackbuffer. */
- (void) enableBackbuffer:(CGSize)new_backbuffer_size
{
# ifndef HAVE_IPHONE
  struct gl_version version;

  {
    const char *version_str = (const char *)glGetString (GL_VERSION);

    /* iPhone is always OpenGL ES 1.1. */
    if (sscanf ((const char *)version_str, "%u.%u",
                &version.major, &version.minor) < 2)
    {
      version.major = 1;
      version.minor = 1;
    }
  }
# endif

  // The OpenGL extensions in use in here are pretty are pretty much ubiquitous
  // on OS X, but it's still good form to check.
  const GLubyte *extensions = glGetString (GL_EXTENSIONS);

  glGenTextures (1, &backbuffer_texture);

  // On really old systems, it would make sense to split the texture
  // into subsections
# ifndef HAVE_IPHONE
  gl_texture_target = (gluCheckExtension ((const GLubyte *)
                                         "GL_ARB_texture_rectangle",
                                         extensions)
                       ? GL_TEXTURE_RECTANGLE_EXT : GL_TEXTURE_2D);
# else
  // OES_texture_npot also provides this, but iOS never provides it.
  gl_limited_npot_p = jwzgles_gluCheckExtension
    ((const GLubyte *) "GL_APPLE_texture_2D_limited_npot", extensions);
  gl_texture_target = GL_TEXTURE_2D;
# endif

  glBindTexture (gl_texture_target, backbuffer_texture);
  glTexParameteri (gl_texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // GL_LINEAR might make sense on Retina iPads.
  glTexParameteri (gl_texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (gl_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (gl_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

# ifndef HAVE_IPHONE
  // There isn't much sense in supporting one of these if the other
  // isn't present.
  gl_apple_client_storage_p =
    gluCheckExtension ((const GLubyte *)"GL_APPLE_client_storage",
                       extensions) &&
    gluCheckExtension ((const GLubyte *)"GL_APPLE_texture_range", extensions);

  if (gl_apple_client_storage_p) {
    glTexParameteri (gl_texture_target, GL_TEXTURE_STORAGE_HINT_APPLE,
                     GL_STORAGE_SHARED_APPLE);
    glPixelStorei (GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
  }
# endif

  // If a video adapter suports BGRA textures, then that's probably as fast as
  // you're gonna get for getting a texture onto the screen.
# ifdef HAVE_IPHONE
  gl_pixel_format =
    jwzgles_gluCheckExtension
      ((const GLubyte *)"GL_APPLE_texture_format_BGRA8888", extensions) ?
      GL_BGRA :
      GL_RGBA;

  gl_pixel_type = GL_UNSIGNED_BYTE;
  // See also OES_read_format.
# else
  if (gl_check_ver (&version, 1, 2) ||
      (gluCheckExtension ((const GLubyte *)"GL_EXT_bgra", extensions) &&
       gluCheckExtension ((const GLubyte *)"GL_APPLE_packed_pixels",
                          extensions))) {
    gl_pixel_format = GL_BGRA;
    // Both Intel and PowerPC-era docs say to use GL_UNSIGNED_INT_8_8_8_8_REV.
    gl_pixel_type = GL_UNSIGNED_INT_8_8_8_8_REV;
  } else {
    gl_pixel_format = GL_RGBA;
    gl_pixel_type = GL_UNSIGNED_BYTE;
  }
  // GL_ABGR_EXT/GL_UNSIGNED_BYTE is another possibilty that may have made more
  // sense on PowerPC.
# endif

  glEnable (gl_texture_target);
  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);

  check_gl_error ("enableBackbuffer");
}


#ifdef HAVE_IPHONE
- (BOOL) suppressRotationAnimation
{
  return [self ignoreRotation];	// Don't animate if we aren't rotating
}

- (BOOL) rotateTouches
{
  return FALSE;			// Adjust event coordinates only if rotating
}
#endif


- (void) setViewport
{
# ifdef BACKBUFFER_OPENGL
  NSAssert ([NSOpenGLContext currentContext] ==
            ogl_ctx, @"invalid GL context");

  NSSize new_size = self.bounds.size;

#  ifdef HAVE_IPHONE
  GLfloat s = self.contentScaleFactor;
#  else // !HAVE_IPHONE
  const GLfloat s = self.window.backingScaleFactor;
#  endif
  GLfloat hs = self.hackedContentScaleFactor;

  // On OS X this almost isn't necessary, except for the ugly aliasing
  // artifacts.
  glViewport (0, 0, new_size.width * s, new_size.height * s);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
#  ifdef HAVE_IPHONE
  glOrthof
#  else
  glOrtho
#  endif
    (-new_size.width * hs, new_size.width * hs,
     -new_size.height * hs, new_size.height * hs,
     -1, 1);

#  ifdef HAVE_IPHONE
  if ([self ignoreRotation]) {
    int o = (int) -current_device_rotation();
    glRotatef (o, 0, 0, 1);
  }
#  endif // HAVE_IPHONE
# endif // BACKBUFFER_OPENGL
}


/* Create a bitmap context into which we render everything.
   If the desired size has changed, re-created it.
   new_size is in rotated pixels, not points: the same size
   and shape as the X11 window as seen by the hacks.
 */
- (void) createBackbuffer:(CGSize)new_size
{
  CGSize osize = CGSizeZero;
  if (backbuffer) {
    osize.width = CGBitmapContextGetWidth(backbuffer);
    osize.height = CGBitmapContextGetHeight(backbuffer);
  }

  if (backbuffer &&
      (int)osize.width  == (int)new_size.width &&
      (int)osize.height == (int)new_size.height)
    return;

  CGContextRef ob = backbuffer;
  void *odata = backbuffer_data;
  GLsizei olen = backbuffer_len;

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  NSLog(@"backbuffer %.0fx%.0f",
        new_size.width, new_size.height);
# endif

  /* OS X uses APPLE_client_storage and APPLE_texture_range, as described in
     <https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html>.

     iOS uses bog-standard glTexImage2D (for now).

     glMapBuffer is the standard way to get data from system RAM to video
     memory asynchronously and without a memcpy, but support for
     APPLE_client_storage is ubiquitous on OS X (not so for glMapBuffer),
     and on iOS GL_PIXEL_UNPACK_BUFFER is only available on OpenGL ES 3
     (iPhone 5S or newer). Plus, glMapBuffer doesn't work well with
     CGBitmapContext: glMapBuffer can return a different pointer on each
     call, but a CGBitmapContext doesn't allow its data pointer to be
     changed -- and recreating the context for a new pointer can be
     expensive (glyph caches get dumped, for instance).

     glMapBufferRange has MAP_FLUSH_EXPLICIT_BIT and MAP_UNSYNCHRONIZED_BIT,
     and these seem to allow mapping the buffer and leaving it where it is
     in client address space while OpenGL works with the buffer, but it
     requires OpenGL 3 Core profile on OS X (and ES 3 on iOS for
     GL_PIXEL_UNPACK_BUFFER), so point goes to APPLE_client_storage.

     AMD_pinned_buffer provides the same advantage as glMapBufferRange, but
     Apple never implemented that one for OS X.
   */

  backbuffer_data = NULL;
  gl_texture_w = (int)new_size.width;
  gl_texture_h = (int)new_size.height;

  NSAssert (gl_texture_target == GL_TEXTURE_2D
# ifndef HAVE_IPHONE
            || gl_texture_target == GL_TEXTURE_RECTANGLE_EXT
# endif
		  , @"unexpected GL texture target");

# ifndef HAVE_IPHONE
  if (gl_texture_target != GL_TEXTURE_RECTANGLE_EXT)
# else
  if (!gl_limited_npot_p)
# endif
  {
    gl_texture_w = (GLsizei) to_pow2 (gl_texture_w);
    gl_texture_h = (GLsizei) to_pow2 (gl_texture_h);
  }

  GLsizei bytes_per_row = gl_texture_w * 4;

# if defined(BACKBUFFER_OPENGL) && !defined(HAVE_IPHONE)
  // APPLE_client_storage requires texture width to be aligned to 32 bytes, or
  // it will fall back to a memcpy.
  // https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html#//apple_ref/doc/uid/TP40001987-CH407-SW24
  bytes_per_row = (bytes_per_row + 31) & ~31;
# endif // BACKBUFFER_OPENGL && !HAVE_IPHONE

  backbuffer_len = bytes_per_row * gl_texture_h;
  if (backbuffer_len) // mmap requires this to be non-zero.
    backbuffer_data = mmap (NULL, backbuffer_len,
                            PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED,
                            -1, 0);

  BOOL alpha_first_p, order_little_p;

  if (gl_pixel_format == GL_BGRA) {
    alpha_first_p = YES;
    order_little_p = YES;
/*
  } else if (gl_pixel_format == GL_ABGR_EXT) {
    alpha_first_p = NO;
    order_little_p = YES; */
  } else {
    NSAssert (gl_pixel_format == GL_RGBA, @"unknown GL pixel format");
    alpha_first_p = NO;
    order_little_p = NO;
  }

#ifdef HAVE_IPHONE
  NSAssert (gl_pixel_type == GL_UNSIGNED_BYTE, @"unknown GL pixel type");
#else
  NSAssert (gl_pixel_type == GL_UNSIGNED_INT_8_8_8_8 ||
            gl_pixel_type == GL_UNSIGNED_INT_8_8_8_8_REV ||
            gl_pixel_type == GL_UNSIGNED_BYTE,
            @"unknown GL pixel type");

#if defined __LITTLE_ENDIAN__
  const GLenum backwards_pixel_type = GL_UNSIGNED_INT_8_8_8_8;
#elif defined __BIG_ENDIAN__
  const GLenum backwards_pixel_type = GL_UNSIGNED_INT_8_8_8_8_REV;
#else
# error Unknown byte order.
#endif

  if (gl_pixel_type == backwards_pixel_type)
    order_little_p ^= YES;
#endif

  CGBitmapInfo bitmap_info =
    (alpha_first_p ? kCGImageAlphaNoneSkipFirst : kCGImageAlphaNoneSkipLast) |
    (order_little_p ? kCGBitmapByteOrder32Little : kCGBitmapByteOrder32Big);

  backbuffer = CGBitmapContextCreate (backbuffer_data,
                                      (int)new_size.width,
                                      (int)new_size.height,
                                      8,
                                      bytes_per_row,
                                      colorspace,
                                      bitmap_info);
  NSAssert (backbuffer, @"unable to allocate back buffer");

  // Clear it.
  CGRect r;
  r.origin.x = r.origin.y = 0;
  r.size = new_size;
  CGContextSetGrayFillColor (backbuffer, 0, 1);
  CGContextFillRect (backbuffer, r);

# if defined(BACKBUFFER_OPENGL) && !defined(HAVE_IPHONE)
  if (gl_apple_client_storage_p)
    glTextureRangeAPPLE (gl_texture_target, backbuffer_len, backbuffer_data);
# endif // BACKBUFFER_OPENGL && !HAVE_IPHONE

  if (ob) {
    // Restore old bits, as much as possible, to the X11 upper left origin.

    CGRect rect;   // pixels, not points
    rect.origin.x = 0;
    rect.origin.y = (new_size.height - osize.height);
    rect.size = osize;

    CGImageRef img = CGBitmapContextCreateImage (ob);
    CGContextDrawImage (backbuffer, rect, img);
    CGImageRelease (img);
    CGContextRelease (ob);

    if (olen)
      // munmap should round len up to the nearest page.
      munmap (odata, olen);
  }

  check_gl_error ("createBackbuffer");
}


- (void) drawBackbuffer
{
# ifdef BACKBUFFER_OPENGL

  NSAssert ([ogl_ctx isKindOfClass:[NSOpenGLContext class]],
            @"ogl_ctx is not an NSOpenGLContext");

  NSAssert (! (CGBitmapContextGetBytesPerRow (backbuffer) % 4),
            @"improperly-aligned backbuffer");

  // This gets width and height from the backbuffer in case
  // APPLE_client_storage is in use. See the note in createBackbuffer.
  // This still has to happen every frame even when APPLE_client_storage has
  // the video adapter pulling texture data straight from
  // XScreenSaverView-owned memory.
  glTexImage2D (gl_texture_target, 0, GL_RGBA,
                (GLsizei)(CGBitmapContextGetBytesPerRow (backbuffer) / 4),
                gl_texture_h, 0, gl_pixel_format, gl_pixel_type,
                backbuffer_data);

  GLfloat w = xwindow->frame.width, h = xwindow->frame.height;

  GLfloat vertices[4][2] = {{-w,  h}, {w,  h}, {w, -h}, {-w, -h}};

  GLfloat tex_coords[4][2];

#  ifndef HAVE_IPHONE
  if (gl_texture_target != GL_TEXTURE_RECTANGLE_EXT)
#  endif // HAVE_IPHONE
  {
    w /= gl_texture_w;
    h /= gl_texture_h;
  }

  tex_coords[0][0] = 0;
  tex_coords[0][1] = 0;
  tex_coords[1][0] = w;
  tex_coords[1][1] = 0;
  tex_coords[2][0] = w;
  tex_coords[2][1] = h;
  tex_coords[3][0] = 0;
  tex_coords[3][1] = h;

  glVertexPointer (2, GL_FLOAT, 0, vertices);
  glTexCoordPointer (2, GL_FLOAT, 0, tex_coords);
  glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

#  if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  check_gl_error ("drawBackbuffer");
#  endif
# endif // BACKBUFFER_OPENGL
}

#endif // JWXYZ_QUARTZ

#ifdef JWXYZ_GL

- (void)enableBackbuffer:(CGSize)new_backbuffer_size;
{
  jwxyz_set_matrices (new_backbuffer_size.width, new_backbuffer_size.height);
  check_gl_error ("enableBackbuffer");
}

- (void)createBackbuffer:(CGSize)new_size
{
  NSAssert ([NSOpenGLContext currentContext] ==
            ogl_ctx, @"invalid GL context");
  NSAssert (xwindow->window.current_drawable == xwindow,
            @"current_drawable not set properly");

# ifndef HAVE_IPHONE
  /* On iOS, Retina means glViewport gets called with the screen size instead
     of the backbuffer/xwindow size. This happens in startAnimation.

     The GL screenhacks call glViewport themselves.
   */
  glViewport (0, 0, new_size.width, new_size.height);
# endif

  // TODO: Preserve contents on resize.
  glClear (GL_COLOR_BUFFER_BIT);
  check_gl_error ("createBackbuffer");
}

#endif // JWXYZ_GL


- (void)flushBackbuffer
{
# ifdef JWXYZ_GL
  // Make sure the right context is active: there's two under JWXYZ_GL.
  jwxyz_bind_drawable (xwindow, xwindow);
# endif // JWXYZ_GL

# ifndef HAVE_IPHONE

#  ifdef JWXYZ_QUARTZ
  // The OpenGL pipeline is not automatically synchronized with the contents
  // of the backbuffer, so without glFinish, OpenGL can start rendering from
  // the backbuffer texture at the same time that JWXYZ is clearing and
  // drawing the next frame in the backing store for the backbuffer texture.
  // This is only a concern under JWXYZ_QUARTZ because of
  // APPLE_client_storage; JWXYZ_GL doesn't use that.
  glFinish();
#  endif // JWXYZ_QUARTZ

  // If JWXYZ_GL was single-buffered, there would need to be a glFinish (or
  // maybe just glFlush?) here, because single-buffered contexts don't always
  // update what's on the screen after drawing finishes. (i.e., in safe mode)

#  ifdef JWXYZ_QUARTZ
  // JWXYZ_GL is always double-buffered.
  if (double_buffered_p)
#  endif // JWXYZ_QUARTZ
    [ogl_ctx flushBuffer]; // despite name, this actually swaps
# else // HAVE_IPHONE

  // jwxyz_bind_drawable() only binds the framebuffer, not the renderbuffer.
#  ifdef JWXYZ_GL
  GLint gl_renderbuffer = xwindow->gl_renderbuffer;
#  endif

  glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_renderbuffer);
  [ogl_ctx presentRenderbuffer:GL_RENDERBUFFER_OES];
# endif // HAVE_IPHONE

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  // glGetError waits for the OpenGL command pipe to flush, so skip it in
  // release builds.
  // OpenGL Programming Guide for Mac -> OpenGL Application Design
  // Strategies -> Allow OpenGL to Manage Your Resources
  // https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_designstrategies/opengl_designstrategies.html#//apple_ref/doc/uid/TP40001987-CH2-SW7
  check_gl_error ("flushBackbuffer");
# endif
}


/* Inform X11 that the size of our window has changed.
 */
- (void) resize_x11
{
  if (!xdpy) return;     // early

  NSSize new_size;	// pixels, not points

  new_size = self.bounds.size;

#  ifdef HAVE_IPHONE

  // If this hack ignores rotation, then that means that it pretends to
  // always be in portrait mode.  If the View has been resized to a 
  // landscape shape, swap width and height to keep the backbuffer
  // in portrait.
  //
  double rot = current_device_rotation();
  if ([self ignoreRotation] && (rot == 90 || rot == -90)) {
    CGFloat swap    = new_size.width;
    new_size.width  = new_size.height;
    new_size.height = swap;
  }
#  endif // HAVE_IPHONE

  double s = self.hackedContentScaleFactor;
  new_size.width *= s;
  new_size.height *= s;

  [self prepareContext];
  [self setViewport];

  // On first resize, xwindow->frame is 0x0.
  if (xwindow->frame.width == new_size.width &&
      xwindow->frame.height == new_size.height)
    return;

#  if defined(BACKBUFFER_OPENGL) && !defined(HAVE_IPHONE)
  [ogl_ctx update];
#  endif // BACKBUFFER_OPENGL && !HAVE_IPHONE

  NSAssert (xwindow && xwindow->type == WINDOW, @"not a window");
  xwindow->frame.x    = 0;
  xwindow->frame.y    = 0;
  xwindow->frame.width  = new_size.width;
  xwindow->frame.height = new_size.height;

  [self createBackbuffer:CGSizeMake(xwindow->frame.width,
                                    xwindow->frame.height)];

# if defined JWXYZ_QUARTZ
  xwindow->cgc = backbuffer;
  NSAssert (xwindow->cgc, @"no CGContext");
# elif defined JWXYZ_GL && !defined HAVE_IPHONE
  [ogl_ctx update];
  [ogl_ctx setView:xwindow->window.view]; // (Is this necessary?)
# endif // JWXYZ_GL && HAVE_IPHONE

  jwxyz_window_resized (xdpy);

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  NSLog(@"reshape %.0fx%.0f", new_size.width, new_size.height);
# endif

  // Next time render_x11 is called, run the saver's reshape_cb.
  resized_p = YES;
}


#ifdef HAVE_IPHONE

/* Called by SaverRunner when the device has changed orientation.
   That means we need to generate a resize event, even if the size
   has not changed (e.g., from LandscapeLeft to LandscapeRight).
 */
- (void) orientationChanged
{
  [self setViewport];
  resized_p = YES;
  next_frame_time = 0;  // Get a new frame on screen quickly
}

/* A hook run after the 'reshape_' method has been called.  Used by
  XScreenSaverGLView to adjust the in-scene GL viewport.
 */
- (void) postReshape
{
}
#endif // HAVE_IPHONE


// Only render_x11 should call this.  XScreenSaverGLView specializes it.
- (void) reshape_x11
{
  xsft->reshape_cb (xdpy, xwindow, xdata,
                    xwindow->frame.width, xwindow->frame.height);
}

- (void) render_x11
{
# ifdef HAVE_IPHONE
  @try {
# endif

  // jwxyz_make_display needs this.
  [self prepareContext]; // resize_x11 also calls this.

  if (!initted_p) {

    resized_p = NO;

    if (! xdpy) {
# ifdef JWXYZ_QUARTZ
      xwindow->cgc = backbuffer;
# endif // JWXYZ_QUARTZ
      xdpy = jwxyz_quartz_make_display (xwindow);

# if defined HAVE_IPHONE
      /* Some X11 hacks (fluidballs) want to ignore all rotation events. */
      _ignoreRotation =
#  ifdef JWXYZ_GL
        TRUE; // Rotation doesn't work yet. TODO: Make rotation work.
#  else  // !JWXYZ_GL
        get_boolean_resource (xdpy, "ignoreRotation", "IgnoreRotation");
#  endif // !JWXYZ_GL
# endif // HAVE_IPHONE

      _lowrez_p = get_boolean_resource (xdpy, "lowrez", "Lowrez");
      if (_lowrez_p) {
        resized_p = YES;

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
        NSSize  b = [self bounds].size;
        CGFloat s = self.hackedContentScaleFactor;
#  ifdef HAVE_IPHONE
        CGFloat o = self.contentScaleFactor;
#  else
        CGFloat o = self.window.backingScaleFactor;
#  endif

        if (o != s)
          NSLog(@"lowrez: scaling %.0fx%.0f -> %.0fx%.0f (%.02f)",
                b.width * o, b.height * o,
                b.width * s, b.height * s, s);
# endif // DEBUG
      }

      [self resize_x11];
    }

    if (!setup_p) {
      setup_p = YES;
      if (xsft->setup_cb)
        xsft->setup_cb (xsft, xsft->setup_arg);
    }
    initted_p = YES;
    NSAssert(!xdata, @"xdata already initialized");


# undef ya_rand_init
    ya_rand_init (0);
    
    XSetWindowBackground (xdpy, xwindow,
                          get_pixel_resource (xdpy, 0,
                                              "background", "Background"));
    XClearWindow (xdpy, xwindow);
    
# ifndef HAVE_IPHONE
    [[self window] setAcceptsMouseMovedEvents:YES];
# endif

    /* Kludge: even though the init_cb functions are declared to take 2 args,
      actually call them with 3, for the benefit of xlockmore_init() and
      xlockmore_setup().
      */
    void *(*init_cb) (Display *, Window, void *) = 
      (void *(*) (Display *, Window, void *)) xsft->init_cb;
    
    xdata = init_cb (xdpy, xwindow, xsft->setup_arg);
    // NSAssert(xdata, @"no xdata from init");
    if (! xdata) abort();

    if (get_boolean_resource (xdpy, "doFPS", "DoFPS")) {
      fpst = fps_init (xdpy, xwindow);
      fps_cb = xsft->fps_cb;
      if (! fps_cb) fps_cb = screenhack_do_fps;
    } else {
      fpst = NULL;
    }

# ifdef HAVE_IPHONE
    if (current_device_rotation() != 0)   // launched while rotated
      resized_p = YES;
# endif

    [self checkForUpdates];

# ifdef HAVE_IPHONE
    BOOL cyclep = get_boolean_resource (xdpy, "globalCycle", "GlobalCycle");
    int cycle_sec =
      (cyclep
       ? get_integer_resource(xdpy, "globalCycleTimeout", "GlobalCycleTimeout")
       : -1);
    NSLog (@"cycle_sec = %d", cycle_sec);
    if (cycle_sec > 0)
      cycle_timer = [NSTimer scheduledTimerWithTimeInterval: cycle_sec
                             target:self
                             selector:@selector(cycleSaver)
                             userInfo:nil
                             repeats:NO];
# endif // HAVE_IPHONE
  }


  /* I don't understand why we have to do this *every frame*, but we do,
     or else the cursor comes back on.
   */
# ifndef HAVE_IPHONE
  if (![self isPreview])
    [NSCursor setHiddenUntilMouseMoves:YES];
# endif


  if (fpst)
    {
      /* This is just a guess, but the -fps code wants to know how long
         we were sleeping between frames.
       */
      long usecs = 1000000 * [self animationTimeInterval];
      usecs -= 200;  // caller apparently sleeps for slightly less sometimes...
      if (usecs < 0) usecs = 0;
      fps_slept (fpst, usecs);
    }


  /* Run any XtAppAddInput and XtAppAddTimeOut callbacks now.
     Do this before delaying for next_frame_time to avoid throttling
     timers to the hack's frame rate.
   */
  XtAppProcessEvent (XtDisplayToApplicationContext (xdpy),
                     XtIMTimer | XtIMAlternateInput);


  /* It turns out that on some systems (possibly only 10.5 and older?)
     [ScreenSaverView setAnimationTimeInterval] does nothing.  This means
     that we cannot rely on it.

     Some of the screen hacks want to delay for long periods, and letting the
     framework run the update function at 30 FPS when it really wanted half a
     minute between frames would be bad.  So instead, we assume that the
     framework's animation timer might fire whenever, but we only invoke the
     screen hack's "draw frame" method when enough time has expired.
  
     This means two extra calls to gettimeofday() per frame.  For fast-cycling
     screen savers, that might actually slow them down.  Oh well.

     A side-effect of this is that it's not possible for a saver to request
     an animation interval that is faster than animationTimeInterval.

     HOWEVER!  On modern systems where setAnimationTimeInterval is *not*
     ignored, it's important that it be faster than 30 FPS.  240 FPS is good.

     An NSTimer won't fire if the timer is already running the invocation
     function from a previous firing.  So, if we use a 30 FPS
     animationTimeInterval (33333 µs) and a screenhack takes 40000 µs for a
     frame, there will be a 26666 µs delay until the next frame, 66666 µs
     after the beginning of the current frame.  In other words, 25 FPS
     becomes 15 FPS.

     Frame rates tend to snap to values of 30/N, where N is a positive
     integer, i.e. 30 FPS, 15 FPS, 10, 7.5, 6. And the 'snapped' frame rate
     is rounded down from what it would normally be.

     So if we set animationTimeInterval to 1/240 instead of 1/30, frame rates
     become values of 60/N, 120/N, or 240/N, with coarser or finer frame rate
     steps for higher or lower animation time intervals respectively.
   */
  struct timeval tv;
  gettimeofday (&tv, 0);
  double now = tv.tv_sec + (tv.tv_usec / 1000000.0);
  if (now < next_frame_time) return;

  // [self flushBackbuffer];

  if (resized_p) {
    // We do this here instead of in setFrame so that all the
    // Xlib drawing takes place under the animation timer.

# ifndef HAVE_IPHONE
    if (ogl_ctx)
      [ogl_ctx setView:self];
# endif // !HAVE_IPHONE

    [self reshape_x11];
    resized_p = NO;
  }


  // And finally:
  //
  // NSAssert(xdata, @"no xdata when drawing");
  if (! xdata) abort();
  unsigned long delay = xsft->draw_cb (xdpy, xwindow, xdata);
  if (fpst && fps_cb)
    fps_cb (xdpy, xwindow, fpst, xdata);

  gettimeofday (&tv, 0);
  now = tv.tv_sec + (tv.tv_usec / 1000000.0);
  next_frame_time = now + (delay / 1000000.0);

# ifdef JWXYZ_QUARTZ
  [self drawBackbuffer];
# endif
  // This can also happen near the beginning of render_x11.
  [self flushBackbuffer];

# ifdef HAVE_IPHONE	// Allow savers on the iPhone to run full-tilt.
  if (delay < [self animationTimeInterval])
    [self setAnimationTimeInterval:(delay / 1000000.0)];
# endif

# ifdef HAVE_IPHONE
  }
  @catch (NSException *e) {
    [self handleException: e];
  }
# endif // HAVE_IPHONE

# if 0
  {
    static int frame = 0;
    if (++frame == 100) {
      fprintf(stderr,"BOOM\n");
      int y = 0;
      //    int aa = *((int*)y);
      int x = 30/y;
    }
  }
# endif
}


/* drawRect always does nothing, and animateOneFrame renders bits to the
   screen.  This is (now) true of both X11 and GL on both MacOS and iOS.
   But this null method needs to exist or things complain.

   Note that drawRect is called before startAnimation, with the intent
   that it draws the initial state to be exposed by the fade-in.
 */
- (void)drawRect:(NSRect)rect
{
}


- (void) animateOneFrame
{
  // Render X11 into the backing store bitmap...

# ifdef JWXYZ_QUARTZ
  NSAssert (backbuffer, @"no back buffer");

#  ifdef HAVE_IPHONE
  UIGraphicsPushContext (backbuffer);
#  endif
# endif // JWXYZ_QUARTZ

  [self render_x11];

# if defined HAVE_IPHONE && defined JWXYZ_QUARTZ
  UIGraphicsPopContext();
# endif
}


- (BOOL)isAnimating
{
  return !!xdpy;
}


# ifndef HAVE_IPHONE  // Doesn't exist on iOS

- (void) setFrame:(NSRect) newRect
{
  [super setFrame:newRect];

  if (xwindow)     // inform Xlib that the window has changed now.
    [self resize_x11];
}

- (void) setFrameSize:(NSSize) newSize
{
  [super setFrameSize:newSize];
  if (xwindow)
    [self resize_x11];
}

# else // HAVE_IPHONE

- (void) layoutSubviews
{
  [super layoutSubviews];
  [self resizeGL];
  if (xwindow)
    [self resize_x11];
}

# endif


+(BOOL) performGammaFade
{
  return YES;
}

- (BOOL) hasConfigureSheet
{
  return YES;
}

+ (NSString *) decompressXML: (NSData *)data
{
  if (! data) return 0;
  BOOL compressed_p = !!strncmp ((const char *) data.bytes, "<?xml", 5);

  // If it's not already XML, decompress it.
  NSAssert (compressed_p, @"xml isn't compressed");
  if (compressed_p) {
    NSMutableData *data2 = 0;
    int ret = -1;
    z_stream zs;
    memset (&zs, 0, sizeof(zs));
    ret = inflateInit2 (&zs, 16 + MAX_WBITS);
    if (ret == Z_OK) {
      UInt32 usize = * (UInt32 *) (data.bytes + data.length - 4);
      data2 = [NSMutableData dataWithLength: usize];
      zs.next_in   = (Bytef *) data.bytes;
      zs.avail_in  = (uint) data.length;
      zs.next_out  = (Bytef *) data2.bytes;
      zs.avail_out = (uint) data2.length;
      ret = inflate (&zs, Z_FINISH);
      inflateEnd (&zs);
    }
    if (ret == Z_OK || ret == Z_STREAM_END)
      data = data2;
    else
      NSAssert2 (0, @"gunzip error: %d: %s",
                 ret, (zs.msg ? zs.msg : "<null>"));
  }

  NSString *s = [[NSString alloc]
                  initWithData:data encoding:NSUTF8StringEncoding];
  [s autorelease];
  return s;
}


#ifndef HAVE_IPHONE
- (NSWindow *) configureSheet
#else
- (UIViewController *) configureView
#endif
{
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  NSString *classname = [NSString stringWithCString:xsft->progclass
                                           encoding:NSUTF8StringEncoding];
  NSString *file = [classname lowercaseString];
  NSString *path = [bundle pathForResource:file ofType:@"xml"];
  if (!path) {
    NSLog (@"%@.xml does not exist in the application bundle: %@/",
           file, [bundle resourcePath]);
    return nil;
  }
  
# ifdef HAVE_IPHONE
  UIViewController *sheet;
  NSString *updater = 0;
# else  // !HAVE_IPHONE
  NSWindow *sheet;
  NSString *updater = [self updaterPath];
# endif // !HAVE_IPHONE


  NSData *xmld = [NSData dataWithContentsOfFile:path];
  NSString *xml = [[self class] decompressXML: xmld];
  sheet = [[XScreenSaverConfigSheet alloc]
            initWithXML:[xml dataUsingEncoding:NSUTF8StringEncoding]
              classname:classname
                options:xsft->options
             controller:[prefsReader userDefaultsController]
       globalController:[prefsReader globalDefaultsController]
               defaults:[prefsReader defaultOptions]
            haveUpdater:(updater ? TRUE : FALSE)];

  // #### am I expected to retain this, or not? wtf.
  //      I thought not, but if I don't do this, we (sometimes) crash.
  // #### Analyze says "potential leak of an object stored into sheet"
  // [sheet retain];

  return sheet;
}


- (NSUserDefaultsController *) userDefaultsController
{
  return [prefsReader userDefaultsController];
}


/* Announce our willingness to accept keyboard input.
 */
- (BOOL)acceptsFirstResponder
{
  return YES;
}


- (void) beep
{
# ifndef HAVE_IPHONE
  NSBeep();
# else // HAVE_IPHONE 

  // There's no way to play a standard system alert sound!
  // We'd have to include our own WAV for that.
  //
  // Or we could vibrate:
  // #import <AudioToolbox/AudioToolbox.h>
  // AudioServicesPlaySystemSound (kSystemSoundID_Vibrate);
  //
  // Instead, just flash the screen white, then fade.
  //
  UIView *v = [[UIView alloc] initWithFrame: [self frame]]; 
  [v setBackgroundColor: [UIColor whiteColor]];
  [[self window] addSubview:v];
  [UIView animateWithDuration: 0.1
          animations:^{ [v setAlpha: 0.0]; }
          completion:^(BOOL finished) { [v removeFromSuperview]; } ];

# endif  // HAVE_IPHONE
}


/* Send an XEvent to the hack.  Returns YES if it was handled.
 */
- (BOOL) sendEvent: (XEvent *) e
{
  if (!initted_p || ![self isAnimating]) // no event handling unless running.
    return NO;

//  [self lockFocus];  // As of 10.14 this causes flicker on mouse motion
  [self prepareContext];
  BOOL result = xsft->event_cb (xdpy, xwindow, xdata, e);
//  [self unlockFocus];cp -Rf ${CONFIGURATION_BUILD_DIR}/BuildOutputPrefPane.prefPane ~/Library/PreferencePanes
  return result;
}


#ifndef HAVE_IPHONE

/* Convert an NSEvent into an XEvent, and pass it along.
   Returns YES if it was handled.
 */
- (BOOL) convertEvent: (NSEvent *) e
            type: (int) type
{
  XEvent xe;
  memset (&xe, 0, sizeof(xe));
  
  int state = 0;
  
  int flags = [e modifierFlags];
  if (flags & NSAlphaShiftKeyMask) state |= LockMask;
  if (flags & NSShiftKeyMask)      state |= ShiftMask;
  if (flags & NSControlKeyMask)    state |= ControlMask;
  if (flags & NSAlternateKeyMask)  state |= Mod1Mask;
  if (flags & NSCommandKeyMask)    state |= Mod2Mask;
  
  NSPoint p = [[[e window] contentView] convertPoint:[e locationInWindow]
                                            toView:self];
  double s = [self hackedContentScaleFactor];
  int x = s * p.x;
  int y = s * ([self bounds].size.height - p.y);

  xe.xany.type = type;
  switch (type) {
    case ButtonPress:
    case ButtonRelease:
      xe.xbutton.x = x;
      xe.xbutton.y = y;
      xe.xbutton.state = state;
      if ([e type] == NSScrollWheel)
        xe.xbutton.button = ([e deltaY] > 0 ? Button4 :
                             [e deltaY] < 0 ? Button5 :
                             [e deltaX] > 0 ? Button6 :
                             [e deltaX] < 0 ? Button7 :
                             0);
      else
        xe.xbutton.button = (unsigned int) [e buttonNumber] + 1;
      break;
    case MotionNotify:
      xe.xmotion.x = x;
      xe.xmotion.y = y;
      xe.xmotion.state = state;
      break;
    case KeyPress:
    case KeyRelease:
      {
        NSString *ns = (([e type] == NSFlagsChanged) ? 0 :
                        [e charactersIgnoringModifiers]);
        KeySym k = 0;

        if (!ns || [ns length] == 0)			// dead key
          {
            // Cocoa hides the difference between left and right keys.
            // Also we only get KeyPress events for these, no KeyRelease
            // (unless we hack the mod state manually.  Bleh.)
            //
            if      (flags & NSAlphaShiftKeyMask)   k = XK_Caps_Lock;
            else if (flags & NSShiftKeyMask)        k = XK_Shift_L;
            else if (flags & NSControlKeyMask)      k = XK_Control_L;
            else if (flags & NSAlternateKeyMask)    k = XK_Alt_L;
            else if (flags & NSCommandKeyMask)      k = XK_Meta_L;
          }
        else if ([ns length] == 1)			// real key
          {
            switch ([ns characterAtIndex:0]) {
            case NSLeftArrowFunctionKey:  k = XK_Left;      break;
            case NSRightArrowFunctionKey: k = XK_Right;     break;
            case NSUpArrowFunctionKey:    k = XK_Up;        break;
            case NSDownArrowFunctionKey:  k = XK_Down;      break;
            case NSPageUpFunctionKey:     k = XK_Page_Up;   break;
            case NSPageDownFunctionKey:   k = XK_Page_Down; break;
            case NSHomeFunctionKey:       k = XK_Home;      break;
            case NSPrevFunctionKey:       k = XK_Prior;     break;
            case NSNextFunctionKey:       k = XK_Next;      break;
            case NSBeginFunctionKey:      k = XK_Begin;     break;
            case NSEndFunctionKey:        k = XK_End;       break;
            case NSF1FunctionKey:	  k = XK_F1;	    break;
            case NSF2FunctionKey:	  k = XK_F2;	    break;
            case NSF3FunctionKey:	  k = XK_F3;	    break;
            case NSF4FunctionKey:	  k = XK_F4;	    break;
            case NSF5FunctionKey:	  k = XK_F5;	    break;
            case NSF6FunctionKey:	  k = XK_F6;	    break;
            case NSF7FunctionKey:	  k = XK_F7;	    break;
            case NSF8FunctionKey:	  k = XK_F8;	    break;
            case NSF9FunctionKey:	  k = XK_F9;	    break;
            case NSF10FunctionKey:	  k = XK_F10;	    break;
            case NSF11FunctionKey:	  k = XK_F11;	    break;
            case NSF12FunctionKey:	  k = XK_F12;	    break;
            default:
              {
                const char *ss = [ns cStringUsingEncoding:NSUTF8StringEncoding];
                k = (ss && *ss ? *ss : 0);
              }
              break;
            }
          }

        if (! k) return YES;   // E.g., "KeyRelease XK_Shift_L"

        xe.xkey.keycode = k;
        xe.xkey.state = state;
        break;
      }
    default:
      NSAssert1 (0, @"unknown X11 event type: %d", type);
      break;
  }

  return [self sendEvent: &xe];
}


- (void) mouseDown: (NSEvent *) e
{
  if (! [self convertEvent:e type:ButtonPress])
    [super mouseDown:e];
}

- (void) mouseUp: (NSEvent *) e
{
  if (! [self convertEvent:e type:ButtonRelease])
    [super mouseUp:e];
}

- (void) otherMouseDown: (NSEvent *) e
{
  if (! [self convertEvent:e type:ButtonPress])
    [super otherMouseDown:e];
}

- (void) otherMouseUp: (NSEvent *) e
{
  if (! [self convertEvent:e type:ButtonRelease])
    [super otherMouseUp:e];
}

- (void) mouseMoved: (NSEvent *) e
{
  if (! [self convertEvent:e type:MotionNotify])
    [super mouseMoved:e];
}

- (void) mouseDragged: (NSEvent *) e
{
  if (! [self convertEvent:e type:MotionNotify])
    [super mouseDragged:e];
}

- (void) otherMouseDragged: (NSEvent *) e
{
  if (! [self convertEvent:e type:MotionNotify])
    [super otherMouseDragged:e];
}

- (void) scrollWheel: (NSEvent *) e
{
  if (! [self convertEvent:e type:ButtonPress])
    [super scrollWheel:e];
}

- (void) keyDown: (NSEvent *) e
{
  if (! [self convertEvent:e type:KeyPress])
    [super keyDown:e];
}

- (void) keyUp: (NSEvent *) e
{
  if (! [self convertEvent:e type:KeyRelease])
    [super keyUp:e];
}

- (void) flagsChanged: (NSEvent *) e
{
  if (! [self convertEvent:e type:KeyPress])
    [super flagsChanged:e];
}


- (NSOpenGLPixelFormat *) getGLPixelFormat
{
  NSAssert (prefsReader, @"no prefsReader for getGLPixelFormat");

  NSOpenGLPixelFormatAttribute attrs[40];
  int i = 0;
  attrs[i++] = NSOpenGLPFAColorSize; attrs[i++] = 24;

/* OpenGL's core profile removes a lot of the same stuff that was removed in
   OpenGL ES (e.g. glBegin, glDrawPixels), so it might be a possibility.

  opengl_core_p = True;
  if (opengl_core_p) {
    attrs[i++] = NSOpenGLPFAOpenGLProfile;
    attrs[i++] = NSOpenGLProfileVersion3_2Core;
  }
 */

/* Eventually: multisampled pixmaps. May not be supported everywhere.
   if (multi_sample_p) {
     attrs[i++] = NSOpenGLPFASampleBuffers; attrs[i++] = 1;
     attrs[i++] = NSOpenGLPFASamples;       attrs[i++] = 6;
   }
 */

# ifdef JWXYZ_QUARTZ
  // Under Quartz, we're just blitting a texture.
  if (double_buffered_p)
    attrs[i++] = NSOpenGLPFADoubleBuffer;
# endif

# ifdef JWXYZ_GL
  /* Under OpenGL, all sorts of drawing commands are being issued, and it might
     be a performance problem if this activity occurs on the front buffer.
     Also, some screenhacks expect OS X/iOS to always double-buffer.
     NSOpenGLPFABackingStore prevents flickering with screenhacks that
     don't redraw the entire screen every frame.
   */
  attrs[i++] = NSOpenGLPFADoubleBuffer;
  attrs[i++] = NSOpenGLPFABackingStore;
# endif

# pragma clang diagnostic push   // "NSOpenGLPFAWindow deprecated in 10.9"
# pragma clang diagnostic ignored "-Wdeprecated"
  attrs[i++] = NSOpenGLPFAWindow;
# pragma clang diagnostic pop

# ifdef JWXYZ_GL
  attrs[i++] = NSOpenGLPFAPixelBuffer;
  /* ...But not NSOpenGLPFAFullScreen, because that would be for
     [NSOpenGLContext setFullScreen].
   */
# endif

  /* NSOpenGLPFAFullScreen would go here if initWithFrame's isPreview == NO.
   */

  attrs[i] = 0;

  NSOpenGLPixelFormat *p = [[NSOpenGLPixelFormat alloc]
                             initWithAttributes:attrs];
  [p autorelease];
  return p;
}

#else  // HAVE_IPHONE


- (void) stopAndClose
{
  [self stopAndClose:NO];
}


- (void) stopAndClose:(Bool)relaunch_p
{
  if ([self isAnimating])
    [self stopAnimation];

  /* Need to make the SaverListController be the firstResponder again
     so that it can continue to receive its own shake events.  I
     suppose that this abstraction-breakage means that I'm adding
     XScreenSaverView to the UINavigationController wrong...
   */
//  UIViewController *v = [[self window] rootViewController];
//  if ([v isKindOfClass: [UINavigationController class]]) {
//    UINavigationController *n = (UINavigationController *) v;
//    [[n topViewController] becomeFirstResponder];
//  }
  [self resignFirstResponder];

  if (relaunch_p) {   // Fake a shake on the SaverListController.
    [_delegate didShake:self];
  } else {	// Not launching another, animate our return to the list.
# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
    NSLog (@"fading back to saver list");
# endif
    [_delegate wantsFadeOut:self];
  }
}


// The cycle timer behaves just like a shake event.
- (void) cycleSaver
{
  [self stopAndClose:YES];
}


/* We distinguish between taps and drags.

   - Drags/pans (down, motion, up) are sent to the saver to handle.
   - Single-taps are sent to the saver to handle.
   - Double-taps are sent to the saver as a "Space" keypress.
   - Swipes (really, two-finger drags/pans) send Up/Down/Left/RightArrow keys.
   - All taps expose the momentary "Close" button.
 */

- (void)initGestures
{
  UITapGestureRecognizer *dtap = [[UITapGestureRecognizer alloc]
                                   initWithTarget:self
                                   action:@selector(handleDoubleTap)];
  dtap.numberOfTapsRequired = 2;
# ifndef HAVE_TVOS
  dtap.numberOfTouchesRequired = 1;
# endif // !HAVE_TVOS

  UITapGestureRecognizer *stap = [[UITapGestureRecognizer alloc]
                                   initWithTarget:self
                                   action:@selector(handleTap:)];
  stap.numberOfTapsRequired = 1;
# ifndef HAVE_TVOS
  stap.numberOfTouchesRequired = 1;
# endif // !HAVE_TVOS
 
  UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc]
                                  initWithTarget:self
                                  action:@selector(handlePan:)];
# ifndef HAVE_TVOS
  pan.maximumNumberOfTouches = 1;
  pan.minimumNumberOfTouches = 1;
# endif // !HAVE_TVOS
 
  // I couldn't get Swipe to work, but using a second Pan recognizer works.
  UIPanGestureRecognizer *pan2 = [[UIPanGestureRecognizer alloc]
                                   initWithTarget:self
                                   action:@selector(handlePan2:)];
# ifndef HAVE_TVOS
  pan2.maximumNumberOfTouches = 2;
  pan2.minimumNumberOfTouches = 2;
# endif // !HAVE_TVOS

  // Also handle long-touch, and treat that the same as Pan.
  // Without this, panning doesn't start until there's motion, so the trick
  // of holding down your finger to freeze the scene doesn't work.
  //
  UILongPressGestureRecognizer *hold = [[UILongPressGestureRecognizer alloc]
                                         initWithTarget:self
                                         action:@selector(handleLongPress:)];
  hold.numberOfTapsRequired = 0;
# ifndef HAVE_TVOS
  hold.numberOfTouchesRequired = 1;
# endif // !HAVE_TVOS
  hold.minimumPressDuration = 0.25;   /* 1/4th second */

# ifndef HAVE_TVOS
  // Two finger pinch to zoom in on the view.
  UIPinchGestureRecognizer *pinch = [[UIPinchGestureRecognizer alloc] 
                                      initWithTarget:self 
                                      action:@selector(handlePinch:)];
# endif // !HAVE_TVOS

  [stap requireGestureRecognizerToFail: dtap];
  [stap requireGestureRecognizerToFail: hold];
  [dtap requireGestureRecognizerToFail: hold];
  [pan  requireGestureRecognizerToFail: hold];
# ifndef HAVE_TVOS
  [pan2 requireGestureRecognizerToFail: pinch];

  [self setMultipleTouchEnabled:YES];
# endif // !HAVE_TVOS

  // As of Oct 2021 (macOS 11.6, iOS 15.1) minimumNumberOfTouches and
  // maximumNumberOfTouches are being ignored in gesture recognisers,
  // so the 'pan2' recognizer was firing for single-touch pans.
  // Instead, we now have the 'pan' do a horrible kludge, see below.

  [self addGestureRecognizer: dtap];
  [self addGestureRecognizer: stap];
  [self addGestureRecognizer: pan];
//[self addGestureRecognizer: pan2];
  [self addGestureRecognizer: hold];
# ifndef HAVE_TVOS
  [self addGestureRecognizer: pinch];
# endif // !HAVE_TVOS

  [dtap release];
  [stap release];
  [pan  release];
  [pan2 release];
  [hold release];
# ifndef HAVE_TVOS
  [pinch release];
# endif // !HAVE_TVOS
}


/* Given a mouse (touch) coordinate in unrotated, unscaled view coordinates,
   convert it to what X11 and OpenGL expect.

   Getting this crap right is tricky, given the confusion of the various
   scale factors, so here's a checklist that I think covers all of the X11
   and OpenGL cases. For each of these: rotate to all 4 orientations;
   ensure the mouse tracks properly to all 4 corners.

   Test it in Xcode 6, because Xcode 5.0.2 can't run the iPhone6+ simulator.

   Test hacks must cover:
     X11 ignoreRotation = true
     X11 ignoreRotation = false
     OpenGL (rotation is handled manually, so they never ignoreRotation)

   Test devices must cover:
     contentScaleFactor = 1, hackedContentScaleFactor = 1 (iPad 2)
     contentScaleFactor = 2, hackedContentScaleFactor = 1 (iPad Retina Air)
     contentScaleFactor = 2, hackedContentScaleFactor = 2 (iPhone 5 5s 6 6+)

     iPad 2:    768x1024 / 1 = 768x1024
     iPad Air: 1536x2048 / 2 = 768x1024 (iPad Retina is identical)
     iPhone 4s:  640x960 / 2 = 320x480
     iPhone 5:  640x1136 / 2 = 320x568 (iPhone 5s and iPhone 6 are identical)
     iPhone 6+: 640x1136 / 2 = 320x568 (nativeBounds 960x1704 nativeScale 3)
   
   Tests:
		      iPad2 iPadAir iPhone4s iPhone5 iPhone6+
     Attraction	X  yes	-	-	-	-	Y
     Fireworkx	X  no	-	-	-	-	Y
     Carousel	GL yes	-	-	-	-	Y
     Voronoi	GL no	-	-	-	-	-
 */
- (void) convertMouse:(CGPoint *)p
{
  CGFloat xx = p->x, yy = p->y;

# if 0 // TARGET_IPHONE_SIMULATOR
  {
    XWindowAttributes xgwa;
    XGetWindowAttributes (xdpy, xwindow, &xgwa);
    NSLog (@"TOUCH %4g, %-4g in %4d x %-4d  cs=%.0f hcs=%.0f r=%d ig=%d\n",
           p->x, p->y,
           xgwa.width, xgwa.height,
           [self contentScaleFactor],
           [self hackedContentScaleFactor],
           [self rotateTouches], [self ignoreRotation]);
  }
# endif // TARGET_IPHONE_SIMULATOR

  if ([self rotateTouches]) {

    // The XScreenSaverGLView case:
    // The X11 window is rotated, as is the framebuffer.
    // The device coordinates match the framebuffer dimensions,
    // but might have axes swapped... and we need to swap them
    // by ratios.
    //
    int w = [self frame].size.width;
    int h = [self frame].size.height;
    GLfloat xr = (GLfloat) xx / w;
    GLfloat yr = (GLfloat) yy / h;
    GLfloat swap;
    int o = (int) current_device_rotation();
    switch (o) {
    case -90: case  270: swap = xr; xr = 1-yr; yr = swap;   break;
    case  90: case -270: swap = xr; xr = yr;   yr = 1-swap; break;
    case 180: case -180:            xr = 1-xr; yr = 1-yr;   break;
    default: break;
    }
    xx = xr * w;
    yy = yr * h;

  } else if ([self ignoreRotation]) {

    // The X11 case, where the hack has opted not to rotate:
    // The X11 window is unrotated, but the framebuffer is rotated.
    // The device coordinates match the framebuffer, so they need to
    // be de-rotated to match the X11 window.
    //
    int w = [self frame].size.width;
    int h = [self frame].size.height;
    int swap;
    int o = (int) current_device_rotation();
    switch (o) {
    case -90: case  270: swap = xx; xx = h-yy; yy = swap;   break;
    case  90: case -270: swap = xx; xx = yy;   yy = w-swap; break;
    case 180: case -180:            xx = w-xx; yy = h-yy;   break;
    default: break;
    }
  }

  double s = [self hackedContentScaleFactor];
  p->x = xx * s;
  p->y = yy * s;

# if 0 // TARGET_IPHONE_SIMULATOR || !defined __OPTIMIZE__
  {
    XWindowAttributes xgwa;
    XGetWindowAttributes (xdpy, xwindow, &xgwa);
    NSLog (@"touch %4g, %-4g in %4d x %-4d  cs=%.0f hcs=%.0f r=%d ig=%d\n",
           p->x, p->y,
           xgwa.width, xgwa.height,
           [self contentScaleFactor],
           [self hackedContentScaleFactor],
           [self rotateTouches], [self ignoreRotation]);
    if (p->x < 0 || p->y < 0 || p->x > xgwa.width || p->y > xgwa.height)
      abort();
  }
# endif // TARGET_IPHONE_SIMULATOR
}


- (void) handleTap:(UIGestureRecognizer *)sender
{
  if (!xwindow)
    return;

  XEvent xe;
  memset (&xe, 0, sizeof(xe));

  [self showCloseButton];

  CGPoint p = [sender locationInView:self];  // this is in points, not pixels
  [self convertMouse:&p];
  NSAssert (xwindow->type == WINDOW, @"not a window");
  xwindow->window.last_mouse_x = p.x;
  xwindow->window.last_mouse_y = p.y;

  xe.xany.type = ButtonPress;
  xe.xbutton.button = 1;
  xe.xbutton.x = p.x;
  xe.xbutton.y = p.y;

# ifndef __OPTIMIZE__
  NSLog (@"tap ButtonPress %d %d", xe.xbutton.x, xe.xbutton.y);
# endif

  if (! [self sendEvent: &xe])
    ; //[self beep];

  xe.xany.type = ButtonRelease;
  xe.xbutton.button = 1;
  xe.xbutton.x = p.x;
  xe.xbutton.y = p.y;

  [self sendEvent: &xe];
}


/* Double click sends Space KeyPress.
 */
- (void) handleDoubleTap
{
  if (!xsft->event_cb || !xwindow) return;

  [self showCloseButton];

# ifndef __OPTIMIZE__
  NSLog (@"double-tap KeyPress Space");
# endif

  XEvent xe;
  memset (&xe, 0, sizeof(xe));
  xe.xkey.keycode = ' ';
  xe.xany.type = KeyPress;
  BOOL ok1 = [self sendEvent: &xe];
  xe.xany.type = KeyRelease;
  BOOL ok2 = [self sendEvent: &xe];
  if (!(ok1 || ok2))
    [self beep];
}


/* Drag with one finger down: send MotionNotify.
 */
- (void) handlePan:(UIGestureRecognizer *)sender
{
  if (!xsft->event_cb || !xwindow) return;

  // As of Oct 2021 (macOS 11.6, iOS 15.1) minimumNumberOfTouches and
  // maximumNumberOfTouches are being ignored in gesture recognisers.
  // Thus, this bullshit.  If we get a multi-touch in this recogniser
  // (which we should not, as it set max and min touches to 1) then
  // do the double-touch swipe handler instead.  the static state is
  // needed because 'StateEnded' is called with numberOfTouches == 0.
  {
    static BOOL pan2_p = FALSE;
    if (sender.state == UIGestureRecognizerStateBegan) {
      pan2_p = FALSE;
      if (sender.numberOfTouches == 2)
        pan2_p = TRUE;
    }

    if (pan2_p) {
      [self handlePan2: (UIPanGestureRecognizer *) sender];
      return;
    }
  }

  if (sender.numberOfTouches > 1)
    return;

  [self showCloseButton];

  XEvent xe;
  memset (&xe, 0, sizeof(xe));

  CGPoint p = [sender locationInView:self];  // this is in points, not pixels
  [self convertMouse:&p];
  NSAssert (xwindow && xwindow->type == WINDOW, @"not a window");
  xwindow->window.last_mouse_x = p.x;
  xwindow->window.last_mouse_y = p.y;

  switch (sender.state) {
  case UIGestureRecognizerStateBegan:
    xe.xany.type = ButtonPress;
    xe.xbutton.button = 1;
    xe.xbutton.x = p.x;
    xe.xbutton.y = p.y;
    break;

  case UIGestureRecognizerStateEnded:
    xe.xany.type = ButtonRelease;
    xe.xbutton.button = 1;
    xe.xbutton.x = p.x;
    xe.xbutton.y = p.y;
    break;

  case UIGestureRecognizerStateChanged:
    xe.xany.type = MotionNotify;
    xe.xmotion.x = p.x;
    xe.xmotion.y = p.y;
    break;

  default:
    break;
  }

# ifndef __OPTIMIZE__
  if (xe.xany.type != MotionNotify)
    NSLog (@"pan (%lu) %s %d %d",
           sender.numberOfTouches,
           (xe.xany.type == ButtonPress ? "ButtonPress" :
            xe.xany.type == ButtonRelease ? "ButtonRelease" :
            xe.xany.type == MotionNotify ? "MotionNotify" : "???"),
           (int) p.x, (int) p.y);
# endif

  BOOL ok = [self sendEvent: &xe];
  if (!ok && xe.xany.type == ButtonRelease)
    [self beep];
}


/* Hold one finger down: assume we're about to start dragging.
   Treat the same as Pan.
 */
- (void) handleLongPress:(UIGestureRecognizer *)sender
{
# ifndef __OPTIMIZE__
  NSLog (@"long-press");
# endif
  [self handlePan:sender];
}



/* Drag with 2 fingers down: send arrow keys.
 */
- (void) handlePan2:(UIPanGestureRecognizer *)sender
{
  if (!xsft->event_cb || !xwindow) return;

  [self showCloseButton];

  if (sender.state != UIGestureRecognizerStateEnded)
    return;

  XEvent xe;
  memset (&xe, 0, sizeof(xe));

  CGPoint p = [sender translationInView:self];  // this is in points, not pixels
  [self convertMouse:&p];

  if (fabs(p.x) > fabs(p.y))
    xe.xkey.keycode = (p.x > 0 ? XK_Right : XK_Left);
  else
    xe.xkey.keycode = (p.y > 0 ? XK_Down : XK_Up);

# ifndef __OPTIMIZE__
  NSLog (@"pan2 (%lu) KeyPress %s",
         sender.numberOfTouches,
         (xe.xkey.keycode == XK_Right ? "Right" :
          xe.xkey.keycode == XK_Left ? "Left" :
          xe.xkey.keycode == XK_Up ? "Up" : "Down"));
# endif

  xe.xany.type = KeyPress;
  BOOL ok1 = [self sendEvent: &xe];
  xe.xany.type = KeyRelease;
  BOOL ok2 = [self sendEvent: &xe];
  if (!(ok1 || ok2))
    [self beep];
}


# ifndef HAVE_TVOS
/* Pinch with 2 fingers: zoom in around the center of the fingers.
 */
- (void) handlePinch:(UIPinchGestureRecognizer *)sender
{
  if (!xsft->event_cb || !xwindow) return;

  [self showCloseButton];

  if (sender.state == UIGestureRecognizerStateBegan)
    pinch_transform = self.transform;  // Save the base transform

  switch (sender.state) {
  case UIGestureRecognizerStateBegan:
# ifndef __OPTIMIZE__
    NSLog (@"Pinch start");
# endif
  case UIGestureRecognizerStateChanged:
    {
      double scale = sender.scale;

      if (scale < 1)
        return;

      self.transform = CGAffineTransformScale (pinch_transform, scale, scale);

      CGPoint p = [sender locationInView: self];
      p.x /= self.layer.bounds.size.width;
      p.y /= self.layer.bounds.size.height;

      CGPoint np = CGPointMake (self.bounds.size.width * p.x,
                                self.bounds.size.height * p.y);
      CGPoint op = CGPointMake (self.bounds.size.width *
                                self.layer.anchorPoint.x, 
                                self.bounds.size.height *
                                self.layer.anchorPoint.y);
      np = CGPointApplyAffineTransform (np, self.transform);
      op = CGPointApplyAffineTransform (op, self.transform);

      CGPoint pos = self.layer.position;
      pos.x -= op.x;
      pos.x += np.x;
      pos.y -= op.y;
      pos.y += np.y;
      self.layer.position = pos;
      self.layer.anchorPoint = p;


    }
    break;

  case UIGestureRecognizerStateEnded:
    {
      // When released, snap back to the default zoom (but animate it).

# ifndef __OPTIMIZE__
      NSLog (@"Pinch end");
# endif

      CABasicAnimation *a1 = [CABasicAnimation
                               animationWithKeyPath:@"position.x"];
      a1.fromValue = [NSNumber numberWithFloat: self.layer.position.x];
      a1.toValue   = [NSNumber numberWithFloat: self.bounds.size.width / 2];

      CABasicAnimation *a2 = [CABasicAnimation
                               animationWithKeyPath:@"position.y"];
      a2.fromValue = [NSNumber numberWithFloat: self.layer.position.y];
      a2.toValue   = [NSNumber numberWithFloat: self.bounds.size.height / 2];

      CABasicAnimation *a3 = [CABasicAnimation
                               animationWithKeyPath:@"anchorPoint.x"];
      a3.fromValue = [NSNumber numberWithFloat: self.layer.anchorPoint.x];
      a3.toValue   = [NSNumber numberWithFloat: 0.5];

      CABasicAnimation *a4 = [CABasicAnimation
                               animationWithKeyPath:@"anchorPoint.y"];
      a4.fromValue = [NSNumber numberWithFloat: self.layer.anchorPoint.y];
      a4.toValue   = [NSNumber numberWithFloat: 0.5];

      CABasicAnimation *a5 = [CABasicAnimation
                               animationWithKeyPath:@"transform.scale"];
      a5.fromValue = [NSNumber numberWithFloat: sender.scale];
      a5.toValue   = [NSNumber numberWithFloat: 1.0];

      CAAnimationGroup *group = [CAAnimationGroup animation];
      group.duration     = 0.3;
      group.repeatCount  = 1;
      group.autoreverses = NO;
      group.animations = @[ a1, a2, a3, a4, a5 ];
      group.timingFunction = [CAMediaTimingFunction
                               functionWithName:
                                 kCAMediaTimingFunctionEaseIn];
      [self.layer addAnimation:group forKey:@"unpinch"];

      self.transform = pinch_transform;
      self.layer.anchorPoint = CGPointMake (0.5, 0.5);
      self.layer.position = CGPointMake (self.bounds.size.width / 2,
                                         self.bounds.size.height / 2);
    }
    break;
  default:
    abort();
  }
}
# endif // !HAVE_TVOS


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

/* Shake means exit and launch a new saver.
 */
- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent *)event
{
  [self stopAndClose:YES];
}


- (void) showCloseButton
{
  int width = self.bounds.size.width;
  double scale = width > 800 ? 2 : 1;  // iPad
  double iw = 24 * scale;
  double ih = iw;
  double off = 4 * scale;

  if (!closeBox) {
    closeBox = [[UIView alloc]
                initWithFrame:CGRectMake(0, 0, width, ih + off)];
    closeBox.backgroundColor = [UIColor clearColor];
    closeBox.autoresizingMask =
      UIViewAutoresizingFlexibleBottomMargin |
      UIViewAutoresizingFlexibleWidth;

    // Add the buttons to the bar
    UIImage *img1 = [UIImage imageNamed:@"stop"];
    UIImage *img2 = [UIImage imageNamed:@"settings"];

    UIButton *button = [[UIButton alloc] init];
    [button setFrame: CGRectMake(off, off, iw, ih)];
    [button setBackgroundImage:img1 forState:UIControlStateNormal];
    [button addTarget:self
            action:@selector(stopAndClose)
            forControlEvents:UIControlEventTouchUpInside];
    [closeBox addSubview:button];
    [button release];

    button = [[UIButton alloc] init];
    [button setFrame: CGRectMake(width - iw - off, off, iw, ih)];
    [button setBackgroundImage:img2 forState:UIControlStateNormal];
    [button addTarget:self
            action:@selector(stopAndOpenSettings)
            forControlEvents:UIControlEventTouchUpInside];
    button.autoresizingMask =
      UIViewAutoresizingFlexibleBottomMargin |
      UIViewAutoresizingFlexibleLeftMargin;
    [closeBox addSubview:button];
    [button release];

    [self addSubview:closeBox];
  }

  // Don't hide the buttons under the iPhone X bezel.
  UIEdgeInsets is = { 0, };
  if ([self respondsToSelector:@selector(safeAreaInsets)]) {
#   pragma clang diagnostic push   // "only available on iOS 11.0 or newer"
#   pragma clang diagnostic ignored "-Wunguarded-availability-new"
    is = [self safeAreaInsets];
#   pragma clang diagnostic pop
    [closeBox setFrame:CGRectMake(is.left, is.top,
                                  self.bounds.size.width - is.right - is.left,
                                  ih + off)];
  }

  if (closeBox.layer.opacity <= 0) {  // Fade in

    CABasicAnimation *anim = [CABasicAnimation animationWithKeyPath:@"opacity"];
    anim.duration     = 0.2;
    anim.repeatCount  = 1;
    anim.autoreverses = NO;
    anim.fromValue    = [NSNumber numberWithFloat:0.0];
    anim.toValue      = [NSNumber numberWithFloat:1.0];
    [closeBox.layer addAnimation:anim forKey:@"animateOpacity"];
    closeBox.layer.opacity = 1;
  }

  // Fade out N seconds from now.
  if (closeBoxTimer)
    [closeBoxTimer invalidate];
  closeBoxTimer = [NSTimer scheduledTimerWithTimeInterval: 3
                           target:self
                           selector:@selector(closeBoxOff)
                           userInfo:nil
                           repeats:NO];
}


- (void)closeBoxOff
{
  if (closeBoxTimer) {
    [closeBoxTimer invalidate];
    closeBoxTimer = 0;
  }
  if (!closeBox)
    return;

  CABasicAnimation *anim = [CABasicAnimation animationWithKeyPath:@"opacity"];
  anim.duration     = 0.2;
  anim.repeatCount  = 1;
  anim.autoreverses = NO;
  anim.fromValue    = [NSNumber numberWithFloat: 1];
  anim.toValue      = [NSNumber numberWithFloat: 0];
  [closeBox.layer addAnimation:anim forKey:@"animateOpacity"];
  closeBox.layer.opacity = 0;
}


- (void) stopAndOpenSettings
{
  if ([self isAnimating])
    [self stopAnimation];
  [self resignFirstResponder];
  [_delegate wantsFadeOut:self];
  [_delegate openPreferences: saver_title];
}


- (void)setScreenLocked:(BOOL)locked
{
  if (screenLocked == locked) return;
  screenLocked = locked;
  if (locked) {
    if ([self isAnimating])
      [self stopAnimation];
  } else {
    if (! [self isAnimating])
      [self startAnimation];
  }
}

- (NSDictionary *)getGLProperties
{
  return [NSDictionary dictionaryWithObjectsAndKeys:
          kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
# ifdef JWXYZ_GL
          /* This could be disabled if we knew the screen would be redrawn
             entirely for every frame.
           */
          [NSNumber numberWithBool:YES], kEAGLDrawablePropertyRetainedBacking,
# endif // JWXYZ_GL
          nil];
}

- (void)addExtraRenderbuffers:(CGSize)size
{
  // No extra renderbuffers are needed for 2D screenhacks.
}
 

- (NSString *)getCAGravity
{
  return kCAGravityCenter;  // Looks better in e.g. Compass.
//  return kCAGravityBottomLeft;
}

#endif // HAVE_IPHONE


# ifndef HAVE_IPHONE

// Returns the full pathname to the Sparkle updater app.
//
- (NSString *) updaterPath
{
  NSString *updater = @"XScreenSaverUpdater.app";

  // There may be multiple copies of the updater: e.g., one in /Applications
  // and one in the mounted installer DMG!  It's important that we run the
  // one from the disk and not the DMG, so search for the right one.
  //
  NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  NSArray *search =
    @[[[bundle bundlePath] stringByDeletingLastPathComponent],
      [@"~/Library/Screen Savers" stringByExpandingTildeInPath],
      @"/Library/Screen Savers",
      @"/System/Library/Screen Savers",
      @"/Applications",
      @"/Applications/Utilities"];
  NSString *app_path = nil;
  for (NSString *dir in search) {
    NSString *p = [dir stringByAppendingPathComponent:updater];
    if ([[NSFileManager defaultManager] fileExistsAtPath:p]) {
      app_path = p;
      break;
    }
  }

  if (! app_path)
    app_path = [workspace fullPathForApplication:updater];

  if (app_path && [app_path hasPrefix:@"/Volumes/XScreenSaver "])
    app_path = 0;  // The DMG version will not do.

  return app_path;
}
# endif // !HAVE_IPHONE


- (void) checkForUpdates
{
# ifndef HAVE_IPHONE
  // We only check once at startup, even if there are multiple screens,
  // and even if this saver is running for many days.
  // (Uh, except this doesn't work because this static isn't shared,
  // even if we make it an exported global. Not sure why. Oh well.)
  static BOOL checked_p = NO;
  if (checked_p) return;
  checked_p = YES;

  // If it's off, don't bother running the updater.  Otherwise, the
  // updater will decide if it's time to hit the network.
  if (! get_boolean_resource (xdpy,
                              SUSUEnableAutomaticChecksKey,
                              SUSUEnableAutomaticChecksKey))
    return;

  NSString *app_path = [self updaterPath];

  if (!app_path) {
    NSLog(@"Unable to find XScreenSaverUpdater.app");
    return;
  }

  NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
  NSError *err = nil;
  if (! [workspace launchApplicationAtURL:[NSURL fileURLWithPath:app_path]
                   options:(NSWorkspaceLaunchWithoutAddingToRecents |
                            NSWorkspaceLaunchWithoutActivation |
                            NSWorkspaceLaunchAndHide)
                   configuration:[NSMutableDictionary dictionary]
                   error:&err]) {
    NSLog(@"Unable to launch %@: %@", app_path, err);
  }

# endif // !HAVE_IPHONE
}


@end

/* Utility functions...
 */

static PrefsReader *
get_prefsReader (Display *dpy)
{
  if (! dpy) return 0;
  XScreenSaverView *view = jwxyz_window_view (XRootWindow (dpy, 0));
  if (!view) return 0;
  return [view prefsReader];
}


char *
get_string_resource (Display *dpy, char *name, char *class)
{
  return [get_prefsReader(dpy) getStringResource:name];
}

Bool
get_boolean_resource (Display *dpy, char *name, char *class)
{
  return [get_prefsReader(dpy) getBooleanResource:name];
}

int
get_integer_resource (Display *dpy, char *name, char *class)
{
  return [get_prefsReader(dpy) getIntegerResource:name];
}

double
get_float_resource (Display *dpy, char *name, char *class)
{
  return [get_prefsReader(dpy) getFloatResource:name];
}
