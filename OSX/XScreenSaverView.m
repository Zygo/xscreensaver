/* xscreensaver, Copyright (c) 2006-2018 Jamie Zawinski <jwz@jwz.org>
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
#import "XScreenSaverView.h"
#import "XScreenSaverConfigSheet.h"
#import "Updater.h"
#import "screenhackI.h"
#import "pow2.h"
#import "jwxyzI.h"
#import "jwxyz-cocoa.h"
#import "jwxyz-timers.h"

#ifdef USE_IPHONE
// XScreenSaverView.m speaks OpenGL ES just fine, but enableBackbuffer does
// need (jwzgles_)gluCheckExtension.
# import "jwzglesI.h"
#else
# import <OpenGL/glu.h>
#endif

/* Garbage collection only exists if we are being compiled against the 
   10.6 SDK or newer, not if we are building against the 10.4 SDK.
 */
#ifndef  MAC_OS_X_VERSION_10_6
# define MAC_OS_X_VERSION_10_6 1060  /* undefined in 10.4 SDK, grr */
#endif
#ifndef  MAC_OS_X_VERSION_10_12
# define MAC_OS_X_VERSION_10_12 101200
#endif
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6 && \
     MAC_OS_X_VERSION_MAX_ALLOWED <  MAC_OS_X_VERSION_10_12)
  /* 10.6 SDK or later, and earlier than 10.12 SDK */
# import <objc/objc-auto.h>
# define DO_GC_HACKERY
#endif

/* Duplicated in xlockmoreI.h and XScreenSaverGLView.m. */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

extern struct xscreensaver_function_table *xscreensaver_function_table;

/* Global variables used by the screen savers
 */
const char *progname;
const char *progclass;
int mono_p = 0;


# ifdef USE_IPHONE

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

# endif // !USE_IPHONE



@interface XScreenSaverView (Private)
- (void) stopAndClose;
- (void) stopAndClose:(Bool)relaunch;
@end

@implementation XScreenSaverView

// Given a lower-cased saver name, returns the function table for it.
// If no name, guess the name from the class's bundle name.
//
- (struct xscreensaver_function_table *) findFunctionTable:(NSString *)name
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
  
  if (! name)
    name = [[path lastPathComponent] stringByDeletingPathExtension];

  name = [[name lowercaseString]
           stringByReplacingOccurrencesOfString:@" "
           withString:@""];

# ifndef USE_IPHONE
  // CFBundleGetDataPointerForName doesn't work in "Archive" builds.
  // I'm guessing that symbol-stripping is mandatory.  Fuck.
  NSString *table_name = [name stringByAppendingString:
                                 @"_xscreensaver_function_table"];
  void *addr = CFBundleGetDataPointerForName (cfb, (CFStringRef) table_name);
  CFRelease (cfb);

  if (! addr)
    NSLog (@"no symbol \"%@\" for \"%@\"", table_name, path);

# else  // USE_IPHONE
  // Depends on the auto-generated "ios-function-table.m" being up to date.
  if (! function_tables)
    function_tables = [make_function_table_dict() retain];
  NSValue *v = [function_tables objectForKey: name];
  void *addr = v ? [v pointerValue] : 0;
# endif // USE_IPHONE

  return (struct xscreensaver_function_table *) addr;
}


// Add the "Contents/Resources/" subdirectory of this screen saver's .bundle
// to $PATH for the benefit of savers that include helper shell scripts.
//
- (void) setShellPath
{
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSAssert1 (nsb, @"no bundle for class %@", [self class]);
  
  NSString *nsdir = [nsb resourcePath];
  NSAssert1 (nsdir, @"no resourcePath for class %@", [self class]);
  const char *dir = [nsdir cStringUsingEncoding:NSUTF8StringEncoding];
  const char *opath = getenv ("PATH");
  if (!opath) opath = "/bin"; // $PATH is unset when running under Shark!
  char *npath = (char *) malloc (strlen (opath) + strlen (dir) + 2);
  strcpy (npath, dir);
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
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSAssert1 (nsb, @"no bundle for class %@", [self class]);
  
  const char *s = [name cStringUsingEncoding:NSUTF8StringEncoding];
  if (setenv ("XSCREENSAVER_CLASSPATH", s, 1)) {
    perror ("setenv");
    NSAssert1 (0, @"setenv \"XSCREENSAVER_CLASSPATH=%s\" failed", s);
  }
}


- (void) loadCustomFonts
{
# ifndef USE_IPHONE
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
# endif // !USE_IPHONE
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

# ifndef USE_IPHONE
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
# endif // !USE_IPHONE

    { 0, 0, 0, 0 }
  };
  static const char *default_defaults [] = {

# if defined(USE_IPHONE) && !defined(__OPTIMIZE__)
    ".doFPS:              True",
# else
    ".doFPS:              False",
# endif
    ".doubleBuffer:       True",
    ".multiSample:        False",
# ifndef USE_IPHONE
    ".textMode:           date",
# else
    ".textMode:           url",
# endif
 // ".textLiteral:        ",
 // ".textFile:           ",
    ".textURL:            https://en.wikipedia.org/w/index.php?title=Special:NewPages&feed=rss",
 // ".textProgram:        ",
    ".grabDesktopImages:  yes",
# ifndef USE_IPHONE
    ".chooseRandomImages: no",
# else
    ".chooseRandomImages: yes",
# endif
    ".imageDirectory:     ~/Pictures",
    ".relaunchDelay:      2",
    ".texFontCacheSize:   30",

# ifndef USE_IPHONE
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
# endif // USE_IPHONE
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


- (id) initWithFrame:(NSRect)frame
           saverName:(NSString *)saverName
           isPreview:(BOOL)isPreview
{
  if (! (self = [super initWithFrame:frame isPreview:isPreview]))
    return 0;
  
  xsft = [self findFunctionTable: saverName];
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
     "org.jwz.xscreensaver.<SAVERNAME>.<NUMBERS>.plist"
   */
  NSString *name = [NSString stringWithCString:xsft->progclass
                             encoding:NSISOLatin1StringEncoding];
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

# if !defined USE_IPHONE && defined JWXYZ_QUARTZ
  // When the view fills the screen and double buffering is enabled, OS X will
  // use page flipping for a minor CPU/FPS boost. In windowed mode, double
  // buffering reduces the frame rate to 1/2 the screen's refresh rate.
  double_buffered_p = !isPreview;
# endif

# ifdef USE_IPHONE
  [self initGestures];

  // So we can tell when we're docked.
  [UIDevice currentDevice].batteryMonitoringEnabled = YES;

  [self setBackgroundColor:[NSColor blackColor]];
# endif // USE_IPHONE

# ifdef JWXYZ_QUARTZ
  // Colorspaces and CGContexts only happen with non-GL hacks.
  colorspace = CGColorSpaceCreateDeviceRGB ();
# endif

  return self;
}


#ifdef USE_TOUCHBAR
- (id) initWithFrame:(NSRect)frame
           saverName:(NSString *)saverName
           isPreview:(BOOL)isPreview
           isTouchbar:(BOOL)isTouchbar
{
  if (! (self = [self initWithFrame:frame saverName:saverName
                      isPreview:isPreview]))
    return 0;
  touchbar_p = isTouchbar;
  return self;
}
#endif // USE_TOUCHBAR


#ifdef USE_IPHONE
+ (Class) layerClass
{
  return [CAEAGLLayer class];
}
#endif


- (id) initWithFrame:(NSRect)frame isPreview:(BOOL)p
{
  return [self initWithFrame:frame saverName:0 isPreview:p];
}


- (void) dealloc
{
  if ([self isAnimating])
    [self stopAnimation];
  NSAssert(!xdata, @"xdata not yet freed");
  NSAssert(!xdpy, @"xdpy not yet freed");

# ifdef USE_IPHONE
  [[NSNotificationCenter defaultCenter] removeObserver:self];
# endif

#  ifdef BACKBUFFER_OPENGL
# ifndef USE_IPHONE
  [pixfmt release];
# endif // !USE_IPHONE
  [ogl_ctx release];
  // Releasing the OpenGL context should also free any OpenGL objects,
  // including the backbuffer texture and frame/render/depthbuffers.
#  endif // BACKBUFFER_OPENGL

# if defined JWXYZ_GL && defined USE_IPHONE
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


#ifdef USE_IPHONE
- (void) lockFocus { }
- (void) unlockFocus { }
#endif // USE_IPHONE



# ifdef USE_IPHONE
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
#endif // USE_IPHONE


- (void) startAnimation
{
  NSAssert(![self isAnimating], @"already animating");
  NSAssert(!initted_p && !xdata, @"already initialized");

  // See comment in render_x11() for why this value is important:
  [self setAnimationTimeInterval: 1.0 / 240.0];

  [super startAnimation];
  /* We can't draw on the window from this method, so we actually do the
     initialization of the screen saver (xsft->init_cb) in the first call
     to animateOneFrame() instead.
   */

# ifdef USE_IPHONE
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

# endif // USE_IPHONE

  // Never automatically turn the screen off if we are docked,
  // and an animation is running.
  //
# ifdef USE_IPHONE
  [UIApplication sharedApplication].idleTimerDisabled =
    ([UIDevice currentDevice].batteryState != UIDeviceBatteryStateUnplugged);
# endif

  xwindow = (Window) calloc (1, sizeof(*xwindow));
  xwindow->type = WINDOW;
  xwindow->window.view = self;
  CFRetain (xwindow->window.view);   // needed for garbage collection?

#ifdef BACKBUFFER_OPENGL
  CGSize new_backbuffer_size;

  {
# ifndef USE_IPHONE
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

# else  // USE_IPHONE
    if (!ogl_ctx) {
      CAEAGLLayer *eagl_layer = (CAEAGLLayer *) self.layer;
      eagl_layer.opaque = TRUE;
      eagl_layer.drawableProperties = [self getGLProperties];

      // Without this, the GL frame buffer is half the screen resolution!
      eagl_layer.contentsScale = [UIScreen mainScreen].scale;

      ogl_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
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

# endif // USE_IPHONE

# ifdef JWXYZ_GL
    xwindow->ogl_ctx = ogl_ctx;
#  ifndef USE_IPHONE
    CFRetain (xwindow->ogl_ctx);
#  endif // USE_IPHONE
# endif // JWXYZ_GL

    check_gl_error ("startAnimation");

//  NSLog (@"%s / %s / %s\n", glGetString (GL_VENDOR),
//         glGetString (GL_RENDERER), glGetString (GL_VERSION));

    [self enableBackbuffer:new_backbuffer_size];
  }
#endif // BACKBUFFER_OPENGL

  [self setViewport];
  [self createBackbuffer:new_backbuffer_size];

# ifdef USE_TOUCHBAR
  if (touchbar_view) [touchbar_view startAnimation];
# endif // USE_TOUCHBAR
}

- (void)stopAnimation
{
  NSAssert([self isAnimating], @"not animating");

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
# if defined JWXYZ_GL && !defined USE_IPHONE
    CFRelease (xwindow->ogl_ctx);
# endif
    CFRelease (xwindow->window.view);
    free (xwindow);
    xwindow = NULL;

//  setup_p = NO; // #### wait, do we need this?
    initted_p = NO;
    xdata = 0;
  }

# ifdef USE_IPHONE
  if (crash_timer)
    [crash_timer invalidate];
  crash_timer = 0;
  NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
  [prefs removeObjectForKey:@"wasRunning"];
  [prefs synchronize];
# endif // USE_IPHONE

  [super stopAnimation];

  // When an animation is no longer running (e.g., looking at the list)
  // then it's ok to power off the screen when docked.
  //
# ifdef USE_IPHONE
  [UIApplication sharedApplication].idleTimerDisabled = NO;
# endif

  // Without this, the GL frame stays on screen when switching tabs
  // in System Preferences.
  // (Or perhaps it used to. It doesn't seem to matter on 10.9.)
  //
# ifndef USE_IPHONE
  [NSOpenGLContext clearCurrentContext];
# endif // !USE_IPHONE

  clear_gl_error();	// This hack is defunct, don't let this linger.

# ifdef JWXYZ_QUARTZ
  CGContextRelease (backbuffer);
  backbuffer = nil;

  if (backbuffer_len)
    munmap (backbuffer_data, backbuffer_len);
  backbuffer_data = NULL;
  backbuffer_len = 0;
# endif

# ifdef USE_TOUCHBAR
  if (touchbar_view) {
    [touchbar_view stopAnimation];
    [touchbar_view release];
    touchbar_view = nil;
  }
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
#ifdef USE_IPHONE
    [EAGLContext setCurrentContext:ogl_ctx];
#else  // !USE_IPHONE
    [ogl_ctx makeCurrentContext];
//    check_gl_error ("makeCurrentContext");
#endif // !USE_IPHONE

#ifdef JWXYZ_GL
    xwindow->window.current_drawable = xwindow;
#endif
  }
}


#ifdef USE_TOUCHBAR

static NSString *touchbar_cid = @"org.jwz.xscreensaver.touchbar";
static NSString *touchbar_iid = @"org.jwz.xscreensaver.touchbar";

- (NSTouchBar *) makeTouchBar
{
  NSTouchBar *t = [[NSTouchBar alloc] init];
  t.delegate = self;
  t.customizationIdentifier = touchbar_cid;
  t.defaultItemIdentifiers = @[touchbar_iid,
                               NSTouchBarItemIdentifierOtherItemsProxy];
  t.customizationAllowedItemIdentifiers = @[touchbar_iid];
  t.principalItemIdentifier = touchbar_iid;
  return t;
}

- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar
       makeItemForIdentifier:(NSTouchBarItemIdentifier)id
{
  if ([id isEqualToString:touchbar_iid])
    {
      NSRect rect = [self frame];
      // #### debugging
      rect.origin.x = 0;
      rect.origin.y = 0;
      rect.size.width = 200;
      rect.size.height = 40;
      touchbar_view = [[[self class] alloc]
                        initWithFrame:rect
                        saverName:[NSString stringWithCString:xsft->progclass
                                            encoding:NSISOLatin1StringEncoding]
                        isPreview:self.isPreview
                        isTouchbar:True];
      [touchbar_view setAutoresizingMask:
                       NSViewWidthSizable|NSViewHeightSizable];
      NSCustomTouchBarItem *item =
        [[NSCustomTouchBarItem alloc] initWithIdentifier:id];
      item.view = touchbar_view;
      item.customizationLabel = touchbar_cid;

      if ([self isAnimating])
        // TouchBar was created after animation begun.
        [touchbar_view startAnimation];
    }
  return nil;
}

#endif // USE_TOUCHBAR


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
# ifdef USE_IPHONE
  CGFloat s = self.contentScaleFactor;
# else
  CGFloat s = self.window.backingScaleFactor;
# endif

  if (_lowrez_p) {
    NSSize b = [self bounds].size;
    CGFloat wh = b.width > b.height ? b.width : b.height;

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


#ifdef USE_IPHONE

double
current_device_rotation (void)
{
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
}


- (void) handleException: (NSException *)e
{
  NSLog (@"Caught exception: %@", e);
  UIAlertController *c = [UIAlertController
                           alertControllerWithTitle:
                             [NSString stringWithFormat: @"%s crashed!",
                                       xsft->progclass]
                           message: [NSString stringWithFormat:
                                                @"The error message was:"
                                              "\n\n%@\n\n"
                                              "If it keeps crashing, try "
                                              "resetting its options.",
                                              e]
                           preferredStyle:UIAlertControllerStyleAlert];

  [c addAction: [UIAlertAction actionWithTitle: @"Exit"
                               style: UIAlertActionStyleDefault
                               handler: ^(UIAlertAction *a) {
    exit (-1);
  }]];
  [c addAction: [UIAlertAction actionWithTitle: @"Keep going"
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

#endif // USE_IPHONE


#ifdef JWXYZ_QUARTZ

# ifndef USE_IPHONE

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
# ifndef USE_IPHONE
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
# ifndef USE_IPHONE
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

# ifndef USE_IPHONE
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
# ifdef USE_IPHONE
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


#ifdef USE_IPHONE
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

#  ifdef USE_IPHONE
  GLfloat s = self.contentScaleFactor;
#  else // !USE_IPHONE
  const GLfloat s = self.window.backingScaleFactor;
#  endif
  GLfloat hs = self.hackedContentScaleFactor;

  // On OS X this almost isn't necessary, except for the ugly aliasing
  // artifacts.
  glViewport (0, 0, new_size.width * s, new_size.height * s);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
#  ifdef USE_IPHONE
  glOrthof
#  else
  glOrtho
#  endif
    (-new_size.width * hs, new_size.width * hs,
     -new_size.height * hs, new_size.height * hs,
     -1, 1);

#  ifdef USE_IPHONE
  if ([self ignoreRotation]) {
    int o = (int) -current_device_rotation();
    glRotatef (o, 0, 0, 1);
  }
#  endif // USE_IPHONE
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
# ifndef USE_IPHONE
            || gl_texture_target == GL_TEXTURE_RECTANGLE_EXT
# endif
		  , @"unexpected GL texture target");

# ifndef USE_IPHONE
  if (gl_texture_target != GL_TEXTURE_RECTANGLE_EXT)
# else
  if (!gl_limited_npot_p)
# endif
  {
    gl_texture_w = (GLsizei) to_pow2 (gl_texture_w);
    gl_texture_h = (GLsizei) to_pow2 (gl_texture_h);
  }

  GLsizei bytes_per_row = gl_texture_w * 4;

# if defined(BACKBUFFER_OPENGL) && !defined(USE_IPHONE)
  // APPLE_client_storage requires texture width to be aligned to 32 bytes, or
  // it will fall back to a memcpy.
  // https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html#//apple_ref/doc/uid/TP40001987-CH407-SW24
  bytes_per_row = (bytes_per_row + 31) & ~31;
# endif // BACKBUFFER_OPENGL && !USE_IPHONE

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

#ifdef USE_IPHONE
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

# if defined(BACKBUFFER_OPENGL) && !defined(USE_IPHONE)
  if (gl_apple_client_storage_p)
    glTextureRangeAPPLE (gl_texture_target, backbuffer_len, backbuffer_data);
# endif // BACKBUFFER_OPENGL && !USE_IPHONE

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

#  ifndef USE_IPHONE
  if (gl_texture_target != GL_TEXTURE_RECTANGLE_EXT)
#  endif // USE_IPHONE
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

# ifndef USE_IPHONE
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

# ifndef USE_IPHONE

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
# else // USE_IPHONE

  // jwxyz_bind_drawable() only binds the framebuffer, not the renderbuffer.
#  ifdef JWXYZ_GL
  GLint gl_renderbuffer = xwindow->gl_renderbuffer;
#  endif

  glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_renderbuffer);
  [ogl_ctx presentRenderbuffer:GL_RENDERBUFFER_OES];
# endif // USE_IPHONE

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

#  ifdef USE_IPHONE

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
#  endif // USE_IPHONE

  double s = self.hackedContentScaleFactor;
  new_size.width *= s;
  new_size.height *= s;

  [self prepareContext];
  [self setViewport];

  // On first resize, xwindow->frame is 0x0.
  if (xwindow->frame.width == new_size.width &&
      xwindow->frame.height == new_size.height)
    return;

#  if defined(BACKBUFFER_OPENGL) && !defined(USE_IPHONE)
  [ogl_ctx update];
#  endif // BACKBUFFER_OPENGL && !USE_IPHONE

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
# elif defined JWXYZ_GL && !defined USE_IPHONE
  [ogl_ctx update];
  [ogl_ctx setView:xwindow->window.view]; // (Is this necessary?)
# endif // JWXYZ_GL && USE_IPHONE

  jwxyz_window_resized (xdpy);

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  NSLog(@"reshape %.0fx%.0f", new_size.width, new_size.height);
# endif

  // Next time render_x11 is called, run the saver's reshape_cb.
  resized_p = YES;
}


#ifdef USE_IPHONE

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
#endif // USE_IPHONE


// Only render_x11 should call this.  XScreenSaverGLView specializes it.
- (void) reshape_x11
{
  xsft->reshape_cb (xdpy, xwindow, xdata,
                    xwindow->frame.width, xwindow->frame.height);
}

- (void) render_x11
{
# ifdef USE_IPHONE
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

# if defined USE_IPHONE
      /* Some X11 hacks (fluidballs) want to ignore all rotation events. */
      _ignoreRotation =
#  ifdef JWXYZ_GL
        TRUE; // Rotation doesn't work yet. TODO: Make rotation work.
#  else  // !JWXYZ_GL
        get_boolean_resource (xdpy, "ignoreRotation", "IgnoreRotation");
#  endif // !JWXYZ_GL
# endif // USE_IPHONE

      _lowrez_p = get_boolean_resource (xdpy, "lowrez", "Lowrez");
      if (_lowrez_p) {
        resized_p = YES;

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
        NSSize  b = [self bounds].size;
        CGFloat s = self.hackedContentScaleFactor;
#  ifdef USE_IPHONE
        CGFloat o = self.contentScaleFactor;
#  else
        CGFloat o = self.window.backingScaleFactor;
#  endif
        if (o != s)
          NSLog(@"lowrez: scaling %.0fx%.0f -> %.0fx%.0f (%.02f)",
                b.width * o, b.height * o,
                b.width * s, b.height * s, s);
# endif
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
    
# ifndef USE_IPHONE
    [[self window] setAcceptsMouseMovedEvents:YES];
# endif

    /* In MacOS 10.5, this enables "QuartzGL", meaning that the Quartz
       drawing primitives will run on the GPU instead of the CPU.
       It seems like it might make things worse rather than better,
       though...  Plus it makes us binary-incompatible with 10.4.

# if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
    [[self window] setPreferredBackingLocation:
                     NSWindowBackingLocationVideoMemory];
# endif
     */

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
      fps_cb = 0;
    }

# ifdef USE_IPHONE
    if (current_device_rotation() != 0)   // launched while rotated
      resized_p = YES;
# endif

    [self checkForUpdates];
  }


  /* I don't understand why we have to do this *every frame*, but we do,
     or else the cursor comes back on.
   */
# ifndef USE_IPHONE
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
     animationTimeInterval (33333 탎) and a screenhack takes 40000 탎 for a
     frame, there will be a 26666 탎 delay until the next frame, 66666 탎
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

# ifndef USE_IPHONE
    if (ogl_ctx)
      [ogl_ctx setView:self];
# endif // !USE_IPHONE

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

# ifdef USE_IPHONE	// Allow savers on the iPhone to run full-tilt.
  if (delay < [self animationTimeInterval])
    [self setAnimationTimeInterval:(delay / 1000000.0)];
# endif

# ifdef DO_GC_HACKERY
  /* Current theory is that the 10.6 garbage collector sucks in the
     following way:

     It only does a collection when a threshold of outstanding
     collectable allocations has been surpassed.  However, CoreGraphics
     creates lots of small collectable allocations that contain pointers
     to very large non-collectable allocations: a small CG object that's
     collectable referencing large malloc'd allocations (non-collectable)
     containing bitmap data.  So the large allocation doesn't get freed
     until GC collects the small allocation, which triggers its finalizer
     to run which frees the large allocation.  So GC is deciding that it
     doesn't really need to run, even though the process has gotten
     enormous.  GC eventually runs once pageouts have happened, but by
     then it's too late, and the machine's resident set has been
     sodomized.

     So, we force an exhaustive garbage collection in this process
     approximately every 5 seconds whether the system thinks it needs 
     one or not.
  */
  {
    static int tick = 0;
    if (++tick > 5*30) {
      tick = 0;
      objc_collect (OBJC_EXHAUSTIVE_COLLECTION);
    }
  }
# endif // DO_GC_HACKERY

# ifdef USE_IPHONE
  }
  @catch (NSException *e) {
    [self handleException: e];
  }
# endif // USE_IPHONE
}


- (void) animateOneFrame
{
  // Render X11 into the backing store bitmap...

# ifdef USE_TOUCHBAR
  if (touchbar_p) return;
# endif

# ifdef JWXYZ_QUARTZ
  NSAssert (backbuffer, @"no back buffer");

#  ifdef USE_IPHONE
  UIGraphicsPushContext (backbuffer);
#  endif
# endif // JWXYZ_QUARTZ

  [self render_x11];

# if defined USE_IPHONE && defined JWXYZ_QUARTZ
  UIGraphicsPopContext();
# endif

# ifdef USE_TOUCHBAR
  if (touchbar_view) [touchbar_view animateOneFrame];
# endif
}


# ifndef USE_IPHONE  // Doesn't exist on iOS

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

# else // USE_IPHONE

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


#ifndef USE_IPHONE
- (NSWindow *) configureSheet
#else
- (UIViewController *) configureView
#endif
{
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  NSString *file = [NSString stringWithCString:xsft->progclass
                                      encoding:NSISOLatin1StringEncoding];
  file = [file lowercaseString];
  NSString *path = [bundle pathForResource:file ofType:@"xml"];
  if (!path) {
    NSLog (@"%@.xml does not exist in the application bundle: %@/",
           file, [bundle resourcePath]);
    return nil;
  }
  
# ifdef USE_IPHONE
  UIViewController *sheet;
# else  // !USE_IPHONE
  NSWindow *sheet;
# endif // !USE_IPHONE

  NSData *xmld = [NSData dataWithContentsOfFile:path];
  NSString *xml = [[self class] decompressXML: xmld];
  sheet = [[XScreenSaverConfigSheet alloc]
            initWithXML:[xml dataUsingEncoding:NSUTF8StringEncoding]
                options:xsft->options
             controller:[prefsReader userDefaultsController]
       globalController:[prefsReader globalDefaultsController]
               defaults:[prefsReader defaultOptions]];

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
# ifndef USE_IPHONE
  NSBeep();
# else // USE_IPHONE 

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

# endif  // USE_IPHONE
}


/* Send an XEvent to the hack.  Returns YES if it was handled.
 */
- (BOOL) sendEvent: (XEvent *) e
{
  if (!initted_p || ![self isAnimating]) // no event handling unless running.
    return NO;

  [self lockFocus];
  [self prepareContext];
  BOOL result = xsft->event_cb (xdpy, xwindow, xdata, e);
  [self unlockFocus];
  return result;
}


#ifndef USE_IPHONE

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
                const char *ss =
                  [ns cStringUsingEncoding:NSISOLatin1StringEncoding];
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

  attrs[i++] = NSOpenGLPFAWindow;
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

#else  // USE_IPHONE


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
  dtap.numberOfTouchesRequired = 1;

  UITapGestureRecognizer *stap = [[UITapGestureRecognizer alloc]
                                   initWithTarget:self
                                   action:@selector(handleTap:)];
  stap.numberOfTapsRequired = 1;
  stap.numberOfTouchesRequired = 1;
 
  UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc]
                                  initWithTarget:self
                                  action:@selector(handlePan:)];
  pan.maximumNumberOfTouches = 1;
  pan.minimumNumberOfTouches = 1;
 
  // I couldn't get Swipe to work, but using a second Pan recognizer works.
  UIPanGestureRecognizer *pan2 = [[UIPanGestureRecognizer alloc]
                                   initWithTarget:self
                                   action:@selector(handlePan2:)];
  pan2.maximumNumberOfTouches = 2;
  pan2.minimumNumberOfTouches = 2;

  // Also handle long-touch, and treat that the same as Pan.
  // Without this, panning doesn't start until there's motion, so the trick
  // of holding down your finger to freeze the scene doesn't work.
  //
  UILongPressGestureRecognizer *hold = [[UILongPressGestureRecognizer alloc]
                                         initWithTarget:self
                                         action:@selector(handleLongPress:)];
  hold.numberOfTapsRequired = 0;
  hold.numberOfTouchesRequired = 1;
  hold.minimumPressDuration = 0.25;   /* 1/4th second */

  // Two finger pinch to zoom in on the view.
  UIPinchGestureRecognizer *pinch = [[UIPinchGestureRecognizer alloc] 
                                      initWithTarget:self 
                                      action:@selector(handlePinch:)];

  [stap requireGestureRecognizerToFail: dtap];
  [stap requireGestureRecognizerToFail: hold];
  [dtap requireGestureRecognizerToFail: hold];
  [pan  requireGestureRecognizerToFail: hold];
  [pan2 requireGestureRecognizerToFail: pinch];

  [self setMultipleTouchEnabled:YES];

  [self addGestureRecognizer: dtap];
  [self addGestureRecognizer: stap];
  [self addGestureRecognizer: pan];
  [self addGestureRecognizer: pan2];
  [self addGestureRecognizer: hold];
  [self addGestureRecognizer: pinch];

  [dtap release];
  [stap release];
  [pan  release];
  [pan2 release];
  [hold release];
  [pinch release];
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


/* Single click exits saver.
 */
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

  BOOL ok = [self sendEvent: &xe];
  if (!ok && xe.xany.type == ButtonRelease)
    [self beep];
}


/* Hold one finger down: assume we're about to start dragging.
   Treat the same as Pan.
 */
- (void) handleLongPress:(UIGestureRecognizer *)sender
{
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

  CGPoint p = [sender locationInView:self];  // this is in points, not pixels
  [self convertMouse:&p];

  if (fabs(p.x) > fabs(p.y))
    xe.xkey.keycode = (p.x > 0 ? XK_Right : XK_Left);
  else
    xe.xkey.keycode = (p.y > 0 ? XK_Down : XK_Up);

  BOOL ok1 = [self sendEvent: &xe];
  xe.xany.type = KeyRelease;
  BOOL ok2 = [self sendEvent: &xe];
  if (!(ok1 || ok2))
    [self beep];
}


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
  double iw = 24;
  double ih = iw;
  double off = 4;

  if (!closeBox) {
    int width = self.bounds.size.width;
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
  NSString *s = [NSString stringWithCString:xsft->progclass
                          encoding:NSISOLatin1StringEncoding];
  if ([self isAnimating])
    [self stopAnimation];
  [self resignFirstResponder];
  [_delegate wantsFadeOut:self];
  [_delegate openPreferences: s];

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

#endif // USE_IPHONE


- (void) checkForUpdates
{
# ifndef USE_IPHONE
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

  if (!app_path) {
    NSLog(@"Unable to find %@", updater);
    return;
  }

  NSError *err = nil;
  if (! [workspace launchApplicationAtURL:[NSURL fileURLWithPath:app_path]
                   options:(NSWorkspaceLaunchWithoutAddingToRecents |
                            NSWorkspaceLaunchWithoutActivation |
                            NSWorkspaceLaunchAndHide)
                   configuration:[NSMutableDictionary dictionary]
                   error:&err]) {
    NSLog(@"Unable to launch %@: %@", app_path, err);
  }

# endif // !USE_IPHONE
}


@end

/* Utility functions...
 */

static PrefsReader *
get_prefsReader (Display *dpy)
{
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
