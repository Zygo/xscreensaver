/* xscreensaver, Copyright (c) 2006-2015 Jamie Zawinski <jwz@jwz.org>
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
#import "xlockmoreI.h"
#import "jwxyz-timers.h"

#ifndef USE_IPHONE
# import <OpenGL/glu.h>
#endif

/* Garbage collection only exists if we are being compiled against the 
   10.6 SDK or newer, not if we are building against the 10.4 SDK.
 */
#ifndef  MAC_OS_X_VERSION_10_6
# define MAC_OS_X_VERSION_10_6 1060  /* undefined in 10.4 SDK, grr */
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6  /* 10.6 SDK */
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
  char *npath = (char *) malloc (strlen (opath) + strlen (dir) + 30);
  strcpy (npath, "PATH=");
  strcat (npath, dir);
  strcat (npath, ":");
  strcat (npath, opath);
  if (putenv (npath)) {
    perror ("putenv");
    NSAssert1 (0, @"putenv \"%s\" failed", npath);
  }

  /* Don't free (npath) -- MacOS's putenv() does not copy it. */
}


// set an $XSCREENSAVER_CLASSPATH variable so that included shell scripts
// (e.g., "xscreensaver-text") know how to look up resources.
//
- (void) setResourcesEnv:(NSString *) name
{
  NSBundle *nsb = [NSBundle bundleForClass:[self class]];
  NSAssert1 (nsb, @"no bundle for class %@", [self class]);
  
  const char *s = [name cStringUsingEncoding:NSUTF8StringEncoding];
  char *env = (char *) malloc (strlen (s) + 40);
  strcpy (env, "XSCREENSAVER_CLASSPATH=");
  strcat (env, s);
  if (putenv (env)) {
    perror ("putenv");
    NSAssert1 (0, @"putenv \"%s\" failed", env);
  }
  /* Don't free (env) -- MacOS's putenv() does not copy it. */
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
    ".doFPS:              False",
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


#ifdef USE_IPHONE
/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}
#endif // USE_IPHONE

#if TARGET_IPHONE_SIMULATOR
static const char *
orientname(unsigned long o)
{
  switch (o) {
  case UIDeviceOrientationUnknown:		return "Unknown";
  case UIDeviceOrientationPortrait:		return "Portrait";
  case UIDeviceOrientationPortraitUpsideDown:	return "PortraitUpsideDown";
  case UIDeviceOrientationLandscapeLeft:	return "LandscapeLeft";
  case UIDeviceOrientationLandscapeRight:	return "LandscapeRight";
  case UIDeviceOrientationFaceUp:		return "FaceUp";
  case UIDeviceOrientationFaceDown:		return "FaceDown";
  default:					return "ERROR";
  }
}
#endif // TARGET_IPHONE_SIMULATOR


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

# ifndef USE_IPHONE
  // When the view fills the screen and double buffering is enabled, OS X will
  // use page flipping for a minor CPU/FPS boost. In windowed mode, double
  // buffering reduces the frame rate to 1/2 the screen's refresh rate.
  double_buffered_p = !isPreview;
# endif

# ifdef USE_IPHONE
  double s = [self hackedContentScaleFactor];
# else
  double s = 1;
# endif

  CGSize bb_size;	// pixels, not points
  bb_size.width  = s * frame.size.width;
  bb_size.height = s * frame.size.height;

# ifdef USE_IPHONE
  initial_bounds = rot_current_size = rot_from = rot_to = bb_size;
  rotation_ratio = -1;

  orientation = UIDeviceOrientationUnknown;
  [self didRotate:nil];
  [self initGestures];

  // So we can tell when we're docked.
  [UIDevice currentDevice].batteryMonitoringEnabled = YES;

  [self setBackgroundColor:[NSColor blackColor]];

  [[NSNotificationCenter defaultCenter]
   addObserver:self
   selector:@selector(didRotate:)
   name:UIDeviceOrientationDidChangeNotification object:nil];
# endif // USE_IPHONE

  return self;
}


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

# ifdef USE_BACKBUFFER

#  ifdef BACKBUFFER_OPENGL
  [ogl_ctx release];
  // Releasing the OpenGL context should also free any OpenGL objects,
  // including the backbuffer texture and frame/render/depthbuffers.
#  endif // BACKBUFFER_OPENGL

  if (colorspace)
    CGColorSpaceRelease (colorspace);

# endif // USE_BACKBUFFER

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
  {
    CGSize b = self.bounds.size;
    double s = [self hackedContentScaleFactor];
    b.width  *= s;
    b.height *= s;
    NSAssert (initial_bounds.width == b.width &&
              initial_bounds.height == b.height,
              @"bounds changed unexpectedly");
  }

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
  [[UIApplication sharedApplication]
    setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];
# endif

#ifdef BACKBUFFER_OPENGL
  CGSize new_backbuffer_size;

  {
# ifndef USE_IPHONE
    if (!ogl_ctx) {

      NSOpenGLPixelFormat *pixfmt = [self getGLPixelFormat];

      NSAssert (pixfmt, @"unable to create NSOpenGLPixelFormat");

      [pixfmt retain]; // #### ???

      // Fun: On OS X 10.7, the second time an OpenGL context is created, after
      // the preferences dialog is launched in SaverTester, the context only
      // lasts until the first full GC. Then it turns black. Solution is to
      // reuse the OpenGL context after this point.
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

# else  // USE_IPHONE
    if (!ogl_ctx) {
      CAEAGLLayer *eagl_layer = (CAEAGLLayer *) self.layer;
      eagl_layer.opaque = TRUE;
      eagl_layer.drawableProperties = [self getGLProperties];

      // Without this, the GL frame buffer is half the screen resolution!
      eagl_layer.contentsScale = [UIScreen mainScreen].scale;

      ogl_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
    }

    [EAGLContext setCurrentContext: ogl_ctx];

    CGSize screen_size = [[[UIScreen mainScreen] currentMode] size];
    // iPad, simulator: 768x1024
    // iPad, physical: 1024x768
    if (screen_size.width > screen_size.height) {
      CGFloat w = screen_size.width;
      screen_size.width = screen_size.height;
      screen_size.height = w;
    }

    if (gl_framebuffer)  glDeleteFramebuffersOES  (1, &gl_framebuffer);
    if (gl_renderbuffer) glDeleteRenderbuffersOES (1, &gl_renderbuffer);

    glGenFramebuffersOES  (1, &gl_framebuffer);
    glBindFramebufferOES  (GL_FRAMEBUFFER_OES,  gl_framebuffer);

    glGenRenderbuffersOES (1, &gl_renderbuffer);
    glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_renderbuffer);

//   redundant?
//     glRenderbufferStorageOES (GL_RENDERBUFFER_OES, GL_RGBA8_OES,
//                               (int)size.width, (int)size.height);
    [ogl_ctx renderbufferStorage:GL_RENDERBUFFER_OES
                    fromDrawable:(CAEAGLLayer*)self.layer];

    glFramebufferRenderbufferOES (GL_FRAMEBUFFER_OES,  GL_COLOR_ATTACHMENT0_OES,
                                  GL_RENDERBUFFER_OES, gl_renderbuffer);

    [self addExtraRenderbuffers:screen_size];

    int err = glCheckFramebufferStatusOES (GL_FRAMEBUFFER_OES);
    switch (err) {
      case GL_FRAMEBUFFER_COMPLETE_OES:
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES:
        NSAssert (0, @"framebuffer incomplete attachment");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES:
        NSAssert (0, @"framebuffer incomplete missing attachment");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES:
        NSAssert (0, @"framebuffer incomplete dimensions");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES:
        NSAssert (0, @"framebuffer incomplete formats");
        break;
      case GL_FRAMEBUFFER_UNSUPPORTED_OES:
        NSAssert (0, @"framebuffer unsupported");
        break;
/*
      case GL_FRAMEBUFFER_UNDEFINED:
        NSAssert (0, @"framebuffer undefined");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        NSAssert (0, @"framebuffer incomplete draw buffer");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        NSAssert (0, @"framebuffer incomplete read buffer");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        NSAssert (0, @"framebuffer incomplete multisample");
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        NSAssert (0, @"framebuffer incomplete layer targets");
        break;
 */
    default:
      NSAssert (0, @"framebuffer incomplete, unknown error 0x%04X", err);
      break;
    }

    glViewport (0, 0, screen_size.width, screen_size.height);

    new_backbuffer_size = initial_bounds;

# endif // USE_IPHONE

    check_gl_error ("startAnimation");

//  NSLog (@"%s / %s / %s\n", glGetString (GL_VENDOR),
//         glGetString (GL_RENDERER), glGetString (GL_VERSION));

    [self enableBackbuffer:new_backbuffer_size];
  }
#endif // BACKBUFFER_OPENGL

#ifdef USE_BACKBUFFER
  [self createBackbuffer:new_backbuffer_size];
#endif
}

- (void)stopAnimation
{
  NSAssert([self isAnimating], @"not animating");

  if (initted_p) {

    [self lockFocus];       // in case something tries to draw from here
    [self prepareContext];

    /* I considered just not even calling the free callback at all...
       But webcollage-cocoa needs it, to kill the inferior webcollage
       processes (since the screen saver framework never generates a
       SIGPIPE for them...)  Instead, I turned off the free call in
       xlockmore.c, which is where all of the bogus calls are anyway.
     */
    xsft->free_cb (xdpy, xwindow, xdata);
    [self unlockFocus];

    // xdpy must be freed before dealloc is called, because xdpy owns a
    // circular reference to the parent XScreenSaverView.
    jwxyz_free_display (xdpy);
    xdpy = NULL;
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
  [[UIApplication sharedApplication]
    setStatusBarHidden:NO withAnimation:UIStatusBarAnimationNone];
# endif

  // Without this, the GL frame stays on screen when switching tabs
  // in System Preferences.
  // (Or perhaps it used to. It doesn't seem to matter on 10.9.)
  //
# ifndef USE_IPHONE
  [NSOpenGLContext clearCurrentContext];
# endif // !USE_IPHONE

  clear_gl_error();	// This hack is defunct, don't let this linger.

  CGContextRelease (backbuffer);
  backbuffer = nil;

  if (backbuffer_len)
    munmap (backbuffer_data, backbuffer_len);
  backbuffer_data = NULL;
  backbuffer_len = 0;
}


// #### maybe this could/should just be on 'lockFocus' instead?
- (void) prepareContext
{
  if (ogl_ctx) {
#ifdef USE_IPHONE
    [EAGLContext setCurrentContext:ogl_ctx];
#else  // !USE_IPHONE
    [ogl_ctx makeCurrentContext];
//    check_gl_error ("makeCurrentContext");
#endif // !USE_IPHONE
  }
}


static void
screenhack_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure)
{
  fps_compute (fpst, 0, -1);
  fps_draw (fpst);
}


#ifdef USE_IPHONE

/* On iPhones with Retina displays, we can draw the savers in "real"
   pixels, and that works great.  The 320x480 "point" screen is really
   a 640x960 *pixel* screen.  However, Retina iPads have 768x1024
   point screens which are 1536x2048 pixels, and apparently that's
   enough pixels that copying those bits to the screen is slow.  Like,
   drops us from 15fps to 7fps.  So, on Retina iPads, we don't draw in
   real pixels.  This will probably make the savers look better
   anyway, since that's a higher resolution than most desktop monitors
   have even today.  (This is only true for X11 programs, not GL 
   programs.  Those are fine at full rez.)

   This method is overridden in XScreenSaverGLView, since this kludge
   isn't necessary for GL programs, being resolution independent by
   nature.
 */
- (CGFloat) hackedContentScaleFactor
{
  NSSize ssize = [[[UIScreen mainScreen] currentMode] size];
  NSSize bsize = [self bounds].size;

  CGFloat
    max_ssize = ssize.width > ssize.height ? ssize.width : ssize.height,
    max_bsize = bsize.width > bsize.height ? bsize.width : bsize.height;

  // Ratio of screen size in pixels to view size in points.
  CGFloat s = max_ssize / max_bsize;

  // Two constraints:

  // 1. Don't exceed -- let's say 1280 pixels in either direction.
  //    (Otherwise the frame rate gets bad.)
  CGFloat mag0 = ceil(max_ssize / 1280);

  // 2. Don't let the pixel size get too small.
  //    (Otherwise pixels in IFS and similar are too fine.)
  //    So don't let the result be > 2 pixels per point.
  CGFloat mag1 = ceil(s / 2);

  // As of iPhone 6, mag0 is always >= mag1. This may not be true in the future.
  // (desired scale factor) = s / (desired magnification factor)
  return s / (mag0 > mag1 ? mag0 : mag1);
}


static GLfloat _global_rot_current_angle_kludge;

double current_device_rotation (void)
{
  return -_global_rot_current_angle_kludge;
}


- (void) hackRotation
{
  if (rotation_ratio >= 0) {	// in the midst of a rotation animation

#   define CLAMP180(N) while (N < 0) N += 360; while (N > 180) N -= 360
    GLfloat f = angle_from;
    GLfloat t = angle_to;
    CLAMP180(f);
    CLAMP180(t);
    GLfloat dist = -(t-f);
    CLAMP180(dist);

    // Intermediate angle.
    rot_current_angle = f - rotation_ratio * dist;

    // Intermediate frame size.
    rot_current_size.width = floor(rot_from.width +
      rotation_ratio * (rot_to.width - rot_from.width));
    rot_current_size.height = floor(rot_from.height +
      rotation_ratio * (rot_to.height - rot_from.height));

    // Tick animation.  Complete rotation in 1/6th sec.
    double now = double_time();
    double duration = 1/6.0;
    rotation_ratio = 1 - ((rot_start_time + duration - now) / duration);

    if (rotation_ratio > 1 || ignore_rotation_p) {	// Done animating.
      orientation = new_orientation;
      rot_current_angle = angle_to;
      rot_current_size = rot_to;
      rotation_ratio = -1;

# if TARGET_IPHONE_SIMULATOR
      NSLog (@"rotation ended: %s %d, %d x %d",
             orientname(orientation), (int) rot_current_angle,
             (int) rot_current_size.width, (int) rot_current_size.height);
# endif

      // Check orientation again in case we rotated again while rotating:
      // this is a no-op if nothing has changed.
      [self didRotate:nil];
    }
  } else {			// Not animating a rotation.
    rot_current_angle = angle_to;
    rot_current_size = rot_to;
  }

  CLAMP180(rot_current_angle);
  _global_rot_current_angle_kludge = rot_current_angle;

#   undef CLAMP180

  CGSize rotsize = ((ignore_rotation_p || ![self reshapeRotatedWindow])
                    ? initial_bounds
                    : rot_current_size);
  if ((int) backbuffer_size.width  != (int) rotsize.width ||
      (int) backbuffer_size.height != (int) rotsize.height)
    [self resize_x11];
}


- (void)alertView:(UIAlertView *)av clickedButtonAtIndex:(NSInteger)i
{
  if (i == 0) exit (-1);	// Cancel
  [self stopAndClose:NO];	// Keep going
}

- (void) handleException: (NSException *)e
{
  NSLog (@"Caught exception: %@", e);
  [[[UIAlertView alloc] initWithTitle:
                          [NSString stringWithFormat: @"%s crashed!",
                                    xsft->progclass]
                        message:
                          [NSString stringWithFormat:
                                      @"The error message was:"
                                    "\n\n%@\n\n"
                                    "If it keeps crashing, try "
                                    "resetting its options.",
                                    e]
                        delegate: self
                        cancelButtonTitle: @"Exit"
                        otherButtonTitles: @"Keep going", nil]
    show];
  [self stopAnimation];
}

#endif // USE_IPHONE


#ifdef USE_BACKBUFFER

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

# else

static GLboolean
gluCheckExtension (const GLubyte *ext_name, const GLubyte *ext_string)
{
  size_t ext_len = strlen ((const char *)ext_name);

  for (;;) {
    const GLubyte *found = (const GLubyte *)strstr ((const char *)ext_string,
                                                    (const char *)ext_name);
    if (!found)
      break;

    char last_ch = found[ext_len];
    if ((found == ext_string || found[-1] == ' ') &&
        (last_ch == ' ' || !last_ch)) {
      return GL_TRUE;
    }

    ext_string = found + ext_len;
  }

  return GL_FALSE;
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
  gl_limited_npot_p = gluCheckExtension ((const GLubyte *)
                                         "GL_APPLE_texture_2D_limited_npot",
                                         extensions);
  gl_texture_target = GL_TEXTURE_2D;
# endif

  glBindTexture (gl_texture_target, &backbuffer_texture);
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
    gluCheckExtension ((const GLubyte *)"GL_APPLE_texture_format_BGRA8888",
                       extensions) ? GL_BGRA : GL_RGBA;
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

# ifdef USE_IPHONE
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  NSAssert (new_backbuffer_size.width != 0 && new_backbuffer_size.height != 0,
            @"initial_bounds never got set");
  // This is pretty similar to the glOrtho in createBackbuffer for OS X.
  glOrthof (-new_backbuffer_size.width, new_backbuffer_size.width,
            -new_backbuffer_size.height, new_backbuffer_size.height, -1, 1);
# endif // USE_IPHONE

  check_gl_error ("enableBackbuffer");
}


static GLsizei
to_pow2 (size_t x)
{
  if (x <= 1)
    return 1;

  size_t mask = (size_t)-1;
  unsigned bits = sizeof(x) * CHAR_BIT;
  unsigned log2 = bits;

  --x;
  while (bits) {
    if (!(x & mask)) {
      log2 -= bits;
      x <<= bits;
    }

    bits >>= 1;
    mask <<= bits;
  }

  return 1 << log2;
}


/* Create a bitmap context into which we render everything.
   If the desired size has changed, re-created it.
   new_size is in rotated pixels, not points: the same size
   and shape as the X11 window as seen by the hacks.
 */
- (void) createBackbuffer:(CGSize)new_size
{
  // Colorspaces and CGContexts only happen with non-GL hacks.
  if (colorspace)
    CGColorSpaceRelease (colorspace);

# ifdef BACKBUFFER_OPENGL
  NSAssert ([NSOpenGLContext currentContext] ==
            ogl_ctx, @"invalid GL context");

  // This almost isn't necessary, except for the ugly aliasing artifacts.
#  ifndef USE_IPHONE
  glViewport (0, 0, new_size.width, new_size.height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  // This is pretty similar to the glOrthof in enableBackbuffer for iPhone.
  glOrtho (-new_size.width, new_size.width, -new_size.height, new_size.height,
           -1, 1);
#  endif // !USE_IPHONE
# endif // BACKBUFFER_OPENGL
	
  NSWindow *window = [self window];

  if (window && xdpy) {
    [self lockFocus];

# ifdef BACKBUFFER_OPENGL
    // Was apparently faster until 10.9.
    colorspace = CGColorSpaceCreateDeviceRGB ();
# endif // BACKBUFFER_OPENGL

    [self unlockFocus];
  } else {
    colorspace = CGColorSpaceCreateDeviceRGB();
  }

  if (backbuffer &&
      (int)backbuffer_size.width  == (int)new_size.width &&
      (int)backbuffer_size.height == (int)new_size.height)
    return;

  CGContextRef ob = backbuffer;
  void *odata = backbuffer_data;
  size_t olen = backbuffer_len;

  CGSize osize = backbuffer_size;	// pixels, not points.
  backbuffer_size = new_size;		// pixels, not points.

# if TARGET_IPHONE_SIMULATOR
  NSLog(@"backbuffer %.0fx%.0f",
        backbuffer_size.width, backbuffer_size.height);
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
  gl_texture_w = (int)backbuffer_size.width;
  gl_texture_h = (int)backbuffer_size.height;

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
    gl_texture_w = to_pow2 (gl_texture_w);
    gl_texture_h = to_pow2 (gl_texture_h);
  }

  size_t bytes_per_row = gl_texture_w * 4;

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
                                      (int)backbuffer_size.width,
                                      (int)backbuffer_size.height,
                                      8,
                                      bytes_per_row,
                                      colorspace,
                                      bitmap_info);
  NSAssert (backbuffer, @"unable to allocate back buffer");

  // Clear it.
  CGRect r;
  r.origin.x = r.origin.y = 0;
  r.size = backbuffer_size;
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
    rect.origin.y = (backbuffer_size.height - osize.height);
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

  GLfloat vertices[4][2] =
  {
    {-backbuffer_size.width,  backbuffer_size.height},
    { backbuffer_size.width,  backbuffer_size.height},
    { backbuffer_size.width, -backbuffer_size.height},
    {-backbuffer_size.width, -backbuffer_size.height}
  };

  GLfloat tex_coords[4][2];

#  ifndef USE_IPHONE
  if (gl_texture_target == GL_TEXTURE_RECTANGLE_EXT) {
    tex_coords[0][0] = 0;
    tex_coords[0][1] = 0;
    tex_coords[1][0] = backbuffer_size.width;
    tex_coords[1][1] = 0;
    tex_coords[2][0] = backbuffer_size.width;
    tex_coords[2][1] = backbuffer_size.height;
    tex_coords[3][0] = 0;
    tex_coords[3][1] = backbuffer_size.height;
  } else
#  endif // USE_IPHONE
  {
    GLfloat x = backbuffer_size.width / gl_texture_w;
    GLfloat y = backbuffer_size.height / gl_texture_h;
    tex_coords[0][0] = 0;
    tex_coords[0][1] = 0;
    tex_coords[1][0] = x;
    tex_coords[1][1] = 0;
    tex_coords[2][0] = x;
    tex_coords[2][1] = y;
    tex_coords[3][0] = 0;
    tex_coords[3][1] = y;
  }

#  ifdef USE_IPHONE
  if (!ignore_rotation_p) {
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
    glRotatef (rot_current_angle, 0, 0, -1);

    if (rotation_ratio >= 0)
      glClear (GL_COLOR_BUFFER_BIT);
  }
#  endif  // USE_IPHONE

  glVertexPointer (2, GL_FLOAT, 0, vertices);
  glTexCoordPointer (2, GL_FLOAT, 0, tex_coords);
  glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

#  if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  check_gl_error ("drawBackbuffer");
#  endif

  // This can also happen near the beginning of render_x11.
  [self flushBackbuffer];

# endif // BACKBUFFER_OPENGL
}


- (void)flushBackbuffer
{
# ifndef USE_IPHONE
  // The OpenGL pipeline is not automatically synchronized with the contents
  // of the backbuffer, so without glFinish, OpenGL can start rendering from
  // the backbuffer texture at the same time that JWXYZ is clearing and
  // drawing the next frame in the backing store for the backbuffer texture.
  glFinish();

  if (double_buffered_p)
    [ogl_ctx flushBuffer]; // despite name, this actually swaps
# else
  glBindRenderbufferOES (GL_RENDERBUFFER_OES, gl_renderbuffer);
  [ogl_ctx presentRenderbuffer:GL_RENDERBUFFER_OES];
# endif

# if !defined __OPTIMIZE__ || TARGET_IPHONE_SIMULATOR
  // glGetError waits for the OpenGL command pipe to flush, so skip it in
  // release builds.
  // OpenGL Programming Guide for Mac -> OpenGL Application Design
  // Strategies -> Allow OpenGL to Manage Your Resources
  // https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_designstrategies/opengl_designstrategies.html#//apple_ref/doc/uid/TP40001987-CH2-SW7
  check_gl_error ("flushBackbuffer");
# endif
}


#endif // USE_BACKBUFFER


/* Inform X11 that the size of our window has changed.
 */
- (void) resize_x11
{
  if (!xwindow) return;  // early

  CGSize new_size;	// pixels, not points

# ifdef USE_BACKBUFFER

  [self prepareContext];

#  if defined(BACKBUFFER_OPENGL) && !defined(USE_IPHONE)
  [ogl_ctx update];
#  endif // BACKBUFFER_OPENGL && !USE_IPHONE

#  ifdef USE_IPHONE
  CGSize rotsize = ((ignore_rotation_p || ![self reshapeRotatedWindow])
                    ? initial_bounds
                    : rot_current_size);
  new_size.width  = rotsize.width;
  new_size.height = rotsize.height;
#  else  // !USE_IPHONE
  new_size = NSSizeToCGSize([self bounds].size);
#  endif // !USE_IPHONE

  [self createBackbuffer:new_size];
  jwxyz_window_resized (xdpy, xwindow, 0, 0, new_size.width, new_size.height,
                        backbuffer);
# else   // !USE_BACKBUFFER
  new_size = [self bounds].size;
  jwxyz_window_resized (xdpy, xwindow, 0, 0, new_size.width, new_size.height,
                        0);
# endif  // !USE_BACKBUFFER

# if TARGET_IPHONE_SIMULATOR
  NSLog(@"reshape %.0fx%.0f", new_size.width, new_size.height);
# endif

  // Next time render_x11 is called, run the saver's reshape_cb.
  resized_p = YES;
}


- (void) render_x11
{
# ifdef USE_IPHONE
  @try {

  if (orientation == UIDeviceOrientationUnknown)
    [self didRotate:nil];
  [self hackRotation];
# endif

  if (!initted_p) {

    if (! xdpy) {
# ifdef USE_BACKBUFFER
      NSAssert (backbuffer, @"no back buffer");
      xdpy = jwxyz_make_display (self, backbuffer);
# else
      xdpy = jwxyz_make_display (self, 0);
# endif
      xwindow = XRootWindow (xdpy, 0);

# ifdef USE_IPHONE
      /* Some X11 hacks (fluidballs) want to ignore all rotation events. */
      ignore_rotation_p =
        get_boolean_resource (xdpy, "ignoreRotation", "IgnoreRotation");
# endif // USE_IPHONE

      [self resize_x11];
    }

    if (!setup_p) {
      setup_p = YES;
      if (xsft->setup_cb)
        xsft->setup_cb (xsft, xsft->setup_arg);
    }
    initted_p = YES;
    resized_p = NO;
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
      if (! xsft->fps_cb) xsft->fps_cb = screenhack_do_fps;
    } else {
      fpst = NULL;
      xsft->fps_cb = 0;
    }

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

  [self prepareContext]; // resize_x11 also calls this.
  // [self flushBackbuffer];

  if (resized_p) {
    // We do this here instead of in setFrame so that all the
    // Xlib drawing takes place under the animation timer.

# ifndef USE_IPHONE
    if (ogl_ctx)
      [ogl_ctx setView:self];
# endif // !USE_IPHONE

    NSRect r;
# ifndef USE_BACKBUFFER
    r = [self bounds];
# else  // USE_BACKBUFFER
    r.origin.x = 0;
    r.origin.y = 0;
    r.size.width  = backbuffer_size.width;
    r.size.height = backbuffer_size.height;
# endif // USE_BACKBUFFER

    xsft->reshape_cb (xdpy, xwindow, xdata, r.size.width, r.size.height);
    resized_p = NO;
  }

  // Run any XtAppAddInput callbacks now.
  // (Note that XtAppAddTimeOut callbacks have already been run by
  // the Cocoa event loop.)
  //
  jwxyz_sources_run (display_sources_data (xdpy));


  // And finally:
  //
  // NSAssert(xdata, @"no xdata when drawing");
  if (! xdata) abort();
  unsigned long delay = xsft->draw_cb (xdpy, xwindow, xdata);
  if (fpst && xsft->fps_cb)
    xsft->fps_cb (xdpy, xwindow, fpst, xdata);

  gettimeofday (&tv, 0);
  now = tv.tv_sec + (tv.tv_usec / 1000000.0);
  next_frame_time = now + (delay / 1000000.0);

  [self drawBackbuffer];

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


#ifndef USE_BACKBUFFER

- (void) animateOneFrame
{
  [self render_x11];
  jwxyz_flush_context(xdpy);
}

#else  // USE_BACKBUFFER

- (void) animateOneFrame
{
  // Render X11 into the backing store bitmap...

  NSAssert (backbuffer, @"no back buffer");

# ifdef USE_IPHONE
  UIGraphicsPushContext (backbuffer);
# endif

  [self render_x11];

# if defined USE_IPHONE && defined USE_BACKBUFFER
  UIGraphicsPopContext();
# endif
}

#endif // USE_BACKBUFFER



- (void) setFrame:(NSRect) newRect
{
  [super setFrame:newRect];

  if (xwindow)     // inform Xlib that the window has changed now.
    [self resize_x11];
}


# ifndef USE_IPHONE  // Doesn't exist on iOS
- (void) setFrameSize:(NSSize) newSize
{
  [super setFrameSize:newSize];
  if (xwindow)
    [self resize_x11];
}
# endif // !USE_IPHONE


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

  return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
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
# ifdef USE_IPHONE
  double s = [self hackedContentScaleFactor];
# else
  int s = 1;
# endif
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
        xe.xbutton.button = [e buttonNumber] + 1;
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
                const char *s =
                  [ns cStringUsingEncoding:NSISOLatin1StringEncoding];
                k = (s && *s ? *s : 0);
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

  if (double_buffered_p)
    attrs[i++] = NSOpenGLPFADoubleBuffer;

  attrs[i] = 0;

  return [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
}

#else  // USE_IPHONE


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
# if TARGET_IPHONE_SIMULATOR
    NSLog (@"fading back to saver list");
# endif
    [_delegate wantsFadeOut:self];
  }
}


/* Whether the shape of the X11 Window should be changed to HxW when the
   device is in a landscape orientation.  X11 hacks want this, but OpenGL
   hacks do not.
 */
- (BOOL)reshapeRotatedWindow
{
  return YES;
}


/* Called after the device's orientation has changed.
   
   Rotation is complicated: the UI, X11 and OpenGL work in 3 different ways.

   The UI (list of savers, preferences panels) is rotated by the system,
   because its UIWindow is under a UINavigationController that does
   automatic rotation, using Core Animation.

   The savers are under a different UIWindow and a UINavigationController
   that does not do automatic rotation.

   We have to do it this way because using Core Animation on an EAGLContext
   causes the OpenGL pipeline used on both X11 and GL savers to fall back on
   software rendering and performance goes to hell.

   During and after rotation, the size/shape of the X11 window changes,
   and ConfigureNotify events are generated.

   X11 code (jwxyz) continues to draw into the (reshaped) backbuffer, which is
   rendered onto a rotating OpenGL quad.

   GL code always recieves a portrait-oriented X11 Window whose size never
   changes.  The GL COLOR_BUFFER is displayed on the hardware directly and
   unrotated, so the GL hacks themselves are responsible for rotating the
   GL scene to match current_device_rotation().

   Touch events are converted to mouse clicks, and those XEvent coordinates
   are reported in the coordinate system currently in use by the X11 window.
   Again, GL must convert those.
 */
- (void)didRotate:(NSNotification *)notification
{
  UIDeviceOrientation current = [[UIDevice currentDevice] orientation];

  /* Sometimes UIDevice doesn't know the proper orientation, or the device is
     face up/face down, so in those cases fall back to the status bar
     orientation. The SaverViewController tries to set the status bar to the
     proper orientation before it creates the XScreenSaverView; see
     _storedOrientation in SaverViewController.
   */
  if (current == UIDeviceOrientationUnknown ||
    current == UIDeviceOrientationFaceUp ||
    current == UIDeviceOrientationFaceDown) {
    /* Mind the differences between UIInterfaceOrientation and
       UIDeviceOrientaiton:
       1. UIInterfaceOrientation does not include FaceUp and FaceDown.
       2. LandscapeLeft and LandscapeRight are swapped between the two. But
          converting between device and interface orientation doesn't need to
          take this into account, because (from the UIInterfaceOrientation
          description): "rotating the device requires rotating the content in
          the opposite direction."
	 */
    current = [UIApplication sharedApplication].statusBarOrientation;
  }

  /* On the iPad (but not iPhone 3GS, or the simulator) sometimes we get
     an orientation change event with an unknown orientation.  Those seem
     to always be immediately followed by another orientation change with
     a *real* orientation change, so let's try just ignoring those bogus
     ones and hoping that the real one comes in shortly...
   */
  if (current == UIDeviceOrientationUnknown)
    return;

  if (rotation_ratio >= 0) return;	// in the midst of rotation animation
  if (orientation == current) return;	// no change

  new_orientation = current;		// current animation target
  rotation_ratio = 0;			// start animating
  rot_start_time = double_time();

  switch (orientation) {
  case UIDeviceOrientationLandscapeLeft:      angle_from = 90;  break;
  case UIDeviceOrientationLandscapeRight:     angle_from = 270; break;
  case UIDeviceOrientationPortraitUpsideDown: angle_from = 180; break;
  default:                                    angle_from = 0;   break;
  }

  switch (new_orientation) {
  case UIDeviceOrientationLandscapeLeft:      angle_to = 90;  break;
  case UIDeviceOrientationLandscapeRight:     angle_to = 270; break;
  case UIDeviceOrientationPortraitUpsideDown: angle_to = 180; break;
  default:                                    angle_to = 0;   break;
  }

  switch (orientation) {
  case UIDeviceOrientationLandscapeRight:	// from landscape
  case UIDeviceOrientationLandscapeLeft:
    rot_from.width  = initial_bounds.height;
    rot_from.height = initial_bounds.width;
    break;
  default:					// from portrait
    rot_from.width  = initial_bounds.width;
    rot_from.height = initial_bounds.height;
    break;
  }

  switch (new_orientation) {
  case UIDeviceOrientationLandscapeRight:	// to landscape
  case UIDeviceOrientationLandscapeLeft:
    rot_to.width  = initial_bounds.height;
    rot_to.height = initial_bounds.width;
    break;
  default:					// to portrait
    rot_to.width  = initial_bounds.width;
    rot_to.height = initial_bounds.height;
    break;
  }

# if TARGET_IPHONE_SIMULATOR
  NSLog (@"%srotation begun: %s %d -> %s %d; %d x %d",
         initted_p ? "" : "initial ",
         orientname(orientation), (int) rot_current_angle,
         orientname(new_orientation), (int) angle_to,
         (int) rot_current_size.width, (int) rot_current_size.height);
# endif

  // Even though the status bar isn't on the screen, this still does two things:
  // 1. It fixes the orientation of the iOS simulator.
  // 2. It places the iOS notification center on the expected edge.
  // 3. It prevents the notification center from causing rotation events.
  [[UIApplication sharedApplication] setStatusBarOrientation:new_orientation
                                                    animated:NO];

 if (! initted_p) {
   // If we've done a rotation but the saver hasn't been initialized yet,
   // don't bother going through an X11 resize, but just do it now.
   rot_start_time = 0;  // dawn of time
   [self hackRotation];
 }
}


/* We distinguish between taps and drags.

   - Drags/pans (down, motion, up) are sent to the saver to handle.
   - Single-taps exit the saver.
   - Double-taps are sent to the saver as a "Space" keypress.
   - Swipes (really, two-finger drags/pans) send Up/Down/Left/RightArrow keys.

   This means a saver cannot respond to a single-tap.  Only a few try to.
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
                                   action:@selector(handleTap)];
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

  [stap requireGestureRecognizerToFail: dtap];
  [stap requireGestureRecognizerToFail: hold];
  [dtap requireGestureRecognizerToFail: hold];
  [pan  requireGestureRecognizerToFail: hold];

  [self setMultipleTouchEnabled:YES];

  [self addGestureRecognizer: dtap];
  [self addGestureRecognizer: stap];
  [self addGestureRecognizer: pan];
  [self addGestureRecognizer: pan2];
  [self addGestureRecognizer: hold];

  [dtap release];
  [stap release];
  [pan  release];
  [pan2 release];
  [hold release];
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
     Attraction	X  yes	Y	Y	Y	Y	Y
     Fireworkx	X  no	Y	Y	Y	Y	Y
     Carousel	GL yes	Y	Y	Y	Y	Y
     Voronoi	GL no	Y	Y	Y	Y	Y
 */
- (void) convertMouse:(int)rot x:(int*)x y:(int *)y
{
  int xx = *x, yy = *y;

# if TARGET_IPHONE_SIMULATOR
  {
    XWindowAttributes xgwa;
    XGetWindowAttributes (xdpy, xwindow, &xgwa);
    NSLog (@"TOUCH %4d, %-4d in %4d x %-4d  ig=%d rr=%d cs=%.0f hcs=%.0f\n",
           *x, *y, 
           xgwa.width, xgwa.height,
           ignore_rotation_p, [self reshapeRotatedWindow],
           [self contentScaleFactor],
           [self hackedContentScaleFactor]);
  }
# endif // TARGET_IPHONE_SIMULATOR

  if (!ignore_rotation_p && [self reshapeRotatedWindow]) {
    //
    // For X11 hacks with ignoreRotation == false, we need to rotate the
    // coordinates to match the unrotated X11 window.  We do not do this
    // for GL hacks, or for X11 hacks with ignoreRotation == true.
    //
    int w = [self frame].size.width;
    int h = [self frame].size.height;
    int swap;
    switch (orientation) {
    case UIDeviceOrientationLandscapeRight:
      swap = xx; xx = h-yy; yy = swap;
      break;
    case UIDeviceOrientationLandscapeLeft:
      swap = xx; xx = yy; yy = w-swap;
      break;
    case UIDeviceOrientationPortraitUpsideDown: 
      xx = w-xx; yy = h-yy;
    default:
      break;
    }
  }

  double s = [self hackedContentScaleFactor];
  *x = xx * s;
  *y = yy * s;

# if TARGET_IPHONE_SIMULATOR || !defined __OPTIMIZE__
  {
    XWindowAttributes xgwa;
    XGetWindowAttributes (xdpy, xwindow, &xgwa);
    NSLog (@"touch %4d, %-4d in %4d x %-4d  ig=%d rr=%d cs=%.0f hcs=%.0f\n",
           *x, *y, 
           xgwa.width, xgwa.height,
           ignore_rotation_p, [self reshapeRotatedWindow],
           [self contentScaleFactor],
           [self hackedContentScaleFactor]);
    if (*x < 0 || *y < 0 || *x > xgwa.width || *y > xgwa.height)
      abort();
  }
# endif // TARGET_IPHONE_SIMULATOR
}


/* Single click exits saver.
 */
- (void) handleTap
{
  [self stopAndClose:NO];
}


/* Double click sends Space KeyPress.
 */
- (void) handleDoubleTap
{
  if (!xsft->event_cb || !xwindow) return;

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

  XEvent xe;
  memset (&xe, 0, sizeof(xe));

  CGPoint p = [sender locationInView:self];  // this is in points, not pixels
  int x = p.x;
  int y = p.y;
  [self convertMouse: rot_current_angle x:&x y:&y];
  jwxyz_mouse_moved (xdpy, xwindow, x, y);

  switch (sender.state) {
  case UIGestureRecognizerStateBegan:
    xe.xany.type = ButtonPress;
    xe.xbutton.button = 1;
    xe.xbutton.x = x;
    xe.xbutton.y = y;
    break;

  case UIGestureRecognizerStateEnded:
    xe.xany.type = ButtonRelease;
    xe.xbutton.button = 1;
    xe.xbutton.x = x;
    xe.xbutton.y = y;
    break;

  case UIGestureRecognizerStateChanged:
    xe.xany.type = MotionNotify;
    xe.xmotion.x = x;
    xe.xmotion.y = y;
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

  if (sender.state != UIGestureRecognizerStateEnded)
    return;

  XEvent xe;
  memset (&xe, 0, sizeof(xe));

  CGPoint p = [sender locationInView:self];  // this is in points, not pixels
  int x = p.x;
  int y = p.y;
  [self convertMouse: rot_current_angle x:&x y:&y];

  if (abs(x) > abs(y))
    xe.xkey.keycode = (x > 0 ? XK_Right : XK_Left);
  else
    xe.xkey.keycode = (y > 0 ? XK_Down : XK_Up);

  BOOL ok1 = [self sendEvent: &xe];
  xe.xany.type = KeyRelease;
  BOOL ok2 = [self sendEvent: &xe];
  if (!(ok1 || ok2))
    [self beep];
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
          nil];
}

- (void)addExtraRenderbuffers:(CGSize)size
{
  // No extra renderbuffers are needed for 2D screenhacks.
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
                   configuration:nil
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
