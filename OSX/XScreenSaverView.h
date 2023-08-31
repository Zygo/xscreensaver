/* xscreensaver, Copyright © 2006-2023 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_IPHONE
# import <Foundation/Foundation.h>
# import <UIKit/UIKit.h>
# define NSView  UIView
# define NSRect  CGRect
# define NSSize  CGSize
# define NSColor UIColor
# define NSImage UIImage
# define NSEvent UIEvent
# define NSWindow UIWindow
# define NSOpenGLContext EAGLContext
#else
# import <Cocoa/Cocoa.h>
# import <ScreenSaver/ScreenSaver.h>
#endif


#import "screenhackI.h"
#import "PrefsReader.h"

#ifdef HAVE_IPHONE

# if TARGET_OS_TV && !defined(HAVE_TVOS)
   // We aren't receiving this from Xcode when compiling libjwxyz.a for tvOS.
#  define HAVE_TVOS 1
# endif

@class XScreenSaverView;

@protocol XScreenSaverViewDelegate
- (void) wantsFadeOut:(XScreenSaverView *)saverView;
- (void) didShake:(XScreenSaverView *)saverView;
- (void) openPreferences: (NSString *)which;
@end

@interface ScreenSaverView : NSView
- (id)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview;
- (NSTimeInterval)animationTimeInterval;
- (void)setAnimationTimeInterval:(NSTimeInterval)timeInterval;
- (void)startAnimation;
- (void)stopAnimation;
- (BOOL)isAnimating;
- (void)animateOneFrame;
- (BOOL)hasConfigureSheet;
//- (NSWindow*)configureSheet;
- (UIViewController*)configureView;
- (BOOL)isPreview;
@end

#endif // HAVE_IPHONE


// Currently only OpenGL backbuffers are supported (OSX and iOS).
# define BACKBUFFER_OPENGL

@interface XScreenSaverView : ScreenSaverView
# if defined(HAVE_TVOS)
			      <UIApplicationDelegate>
# elif defined(HAVE_IPHONE)
			      <UIAlertViewDelegate>
# endif
{
  struct xscreensaver_function_table *xsft;
  PrefsReader *prefsReader;
  NSString *saver_title;  // "Möbius Gears", not "MoebiusGears"

  BOOL setup_p;		   // whether xsft->setup_cb() has been run
  BOOL initted_p;          // whether xsft->init_cb() has been run
  BOOL resized_p;	   // whether to run the xsft->reshape_cb() soon
  double next_frame_time;  // time_t in milliseconds of when to tick the frame

  // Data used by the Xlib-flavored screensaver
  Display *xdpy;
  Window xwindow;
  void *xdata;
  fps_state *fpst;
  void (*fps_cb) (Display *, Window, fps_state *, void *);

  BOOL _lowrez_p;		// Whether the saver prefers 1990s pixels.

# ifdef HAVE_IPHONE
  BOOL screenLocked;
  BOOL _ignoreRotation;		// whether hack requested "always portrait".
				// some want this, some do not.
  NSTimer *crash_timer;
  NSTimer *cycle_timer;

  NSDictionary *function_tables;

  id<XScreenSaverViewDelegate> _delegate;

  UIView *closeBox;
  NSTimer *closeBoxTimer;

  CGAffineTransform pinch_transform;

# else // !USE_PHONE

  NSOpenGLPixelFormat *pixfmt;

# endif // !HAVE_IPHONE

  NSOpenGLContext *ogl_ctx;      // OpenGL rendering context

# ifdef JWXYZ_QUARTZ
  CGContextRef backbuffer;
  CGColorSpaceRef colorspace;

#  ifdef BACKBUFFER_OPENGL
  void *backbuffer_data;
  GLsizei backbuffer_len;

  GLsizei gl_texture_w, gl_texture_h;

  GLuint backbuffer_texture;
  GLenum gl_texture_target;
  GLenum gl_pixel_format, gl_pixel_type;
#   ifndef HAVE_IPHONE
  BOOL double_buffered_p, gl_apple_client_storage_p;
#   else // HAVE_IPHONE
  BOOL gl_limited_npot_p;
  GLuint gl_framebuffer, gl_renderbuffer;
#   endif // HAVE_IPHONE
#  endif

# endif // JWXYZ_QUARTZ

# if defined JWXYZ_GL && defined HAVE_IPHONE
  NSOpenGLContext *ogl_ctx_pixmap;
# endif // JWXYZ_GL && HAVE_IPHONE
}

- (id)initWithFrame:(NSRect)f title:(NSString*)t isPreview:(BOOL)p
         randomizer:(BOOL)r;
- (id)initWithFrame:(NSRect)f title:(NSString*)t isPreview:(BOOL)p;
- (id)initWithFrame:(NSRect)f isPreview:(BOOL)p randomizer:(BOOL)r;

- (void) render_x11;
- (NSOpenGLContext *) oglContext;
- (void) prepareContext;
- (NSUserDefaultsController *) userDefaultsController;
+ (NSString *) decompressXML:(NSData *)xml;

- (CGFloat) hackedContentScaleFactor;
- (CGFloat) hackedContentScaleFactor:(BOOL)fonts_p;

#ifdef HAVE_IPHONE
- (void)setScreenLocked:(BOOL)locked;
- (NSDictionary *)getGLProperties;
- (void)addExtraRenderbuffers:(CGSize)size;
- (NSString *)getCAGravity;
- (void)orientationChanged;
@property (nonatomic, assign) id<XScreenSaverViewDelegate> delegate;
@property (nonatomic) BOOL ignoreRotation;
- (BOOL)suppressRotationAnimation;
- (BOOL)rotateTouches;
#else // !HAVE_IPHONE
- (NSOpenGLPixelFormat *)getGLPixelFormat;
#endif // !HAVE_IPHONE

- (void)enableBackbuffer:(CGSize)new_backbuffer_size;
- (void)setViewport;
- (void)createBackbuffer:(CGSize)s;
- (void)reshape_x11;
#ifdef JWXYZ_QUARTZ
- (void)drawBackbuffer;
#endif // JWXYZ_QUARTZ
- (void)flushBackbuffer;

@end
