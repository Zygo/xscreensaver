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

#ifdef USE_IPHONE
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

#ifdef USE_IPHONE

@class XScreenSaverView;

@protocol XScreenSaverViewDelegate
- (void) wantsFadeOut:(XScreenSaverView *)saverView;
- (void) didShake:(XScreenSaverView *)saverView;
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

#endif // USE_IPHONE


#define USE_BACKBUFFER  // must be in sync with jwxyz.m
// Currently only OpenGL backbuffers are supported (OSX and iOS).
# define BACKBUFFER_OPENGL

@interface XScreenSaverView : ScreenSaverView
# ifdef USE_IPHONE
			      <UIAlertViewDelegate>
# endif
{
  struct xscreensaver_function_table *xsft;
  PrefsReader *prefsReader;

  BOOL setup_p;		   // whether xsft->setup_cb() has been run
  BOOL initted_p;          // whether xsft->init_cb() has been run
  BOOL resized_p;	   // whether to run the xsft->reshape_cb() soon
  double next_frame_time;  // time_t in milliseconds of when to tick the frame

  // Data used by the Xlib-flavored screensaver
  Display *xdpy;
  Window xwindow;
  void *xdata;
  fps_state *fpst;

# ifdef USE_IPHONE
  UIDeviceOrientation orientation, new_orientation;
  BOOL screenLocked;

  CGSize initial_bounds;	// portrait-mode size (pixels, not points).
	
  GLfloat rotation_ratio;	// ratio [0-1] thru rotation anim, or -1
  NSSize rot_current_size;	// intermediate or at-rest orientation.
  NSSize rot_from, rot_to;	// start/end size rect (pixels, not points)
  GLfloat rot_current_angle;	// only right angles when rotation complete.
  GLfloat angle_from, angle_to;	// start angle, end angle (degrees)
  double rot_start_time;

  BOOL ignore_rotation_p;	// whether hack requested "always portrait".
				// some want this, some do not.

  NSTimer *crash_timer;

  NSDictionary *function_tables;

  id<XScreenSaverViewDelegate> _delegate;

# endif // USE_IPHONE

# ifdef USE_BACKBUFFER
  CGContextRef backbuffer;
  CGSize backbuffer_size;	// pixels, not points.
  CGColorSpaceRef colorspace;

#  ifdef BACKBUFFER_OPENGL
  void *backbuffer_data;
  size_t backbuffer_len;

  GLsizei gl_texture_w, gl_texture_h;

  NSOpenGLContext *ogl_ctx;      // OpenGL rendering context
  GLuint backbuffer_texture;
  GLenum gl_texture_target;
  GLenum gl_pixel_format, gl_pixel_type;
#   ifndef USE_IPHONE
  BOOL double_buffered_p, gl_apple_client_storage_p;
#   else // USE_IPHONE
  BOOL gl_limited_npot_p;
  GLuint gl_framebuffer, gl_renderbuffer;
#   endif // USE_IPHONE
#  endif

# endif // USE_BACKBUFFER
}

- (id)initWithFrame:(NSRect)frame saverName:(NSString*)n isPreview:(BOOL)p;

- (void) render_x11;
- (void) prepareContext;
- (NSUserDefaultsController *) userDefaultsController;
+ (NSString *) decompressXML:(NSData *)xml;

#ifdef USE_IPHONE
- (BOOL)reshapeRotatedWindow;
- (void)setScreenLocked:(BOOL)locked;
- (NSDictionary *)getGLProperties;
- (void)addExtraRenderbuffers:(CGSize)size;
@property (nonatomic, assign) id<XScreenSaverViewDelegate> delegate;
#else // !USE_IPHONE
- (NSOpenGLPixelFormat *)getGLPixelFormat;
#endif // !USE_IPHONE

#ifdef USE_BACKBUFFER
- (void)enableBackbuffer:(CGSize)new_backbuffer_size;
- (void)createBackbuffer:(CGSize)s;
- (void)drawBackbuffer;
- (void)flushBackbuffer;
#endif // USE_BACKBUFFER

@end
