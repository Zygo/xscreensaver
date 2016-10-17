/* xscreensaver, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __JWXYZ_ANDROID_H__
#define __JWXYZ_ANDROID_H__

#include "jwxyz.h"
#include "../hacks/fps.h"

#include <android/log.h>
#include <EGL/egl.h>
#include <jni.h>

/* Keep synchronized with check-configs.pl and jwxyz.java. */
#define API_XLIB 0
#define API_GL   1

struct running_hack {
  struct xscreensaver_function_table *xsft;
  jint api;
  Display *dpy;
  Window window;
  fps_state *fpst;
  void *closure;
  JNIEnv *jni_env;
  jobject jobject;

  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLContext egl_window_ctx, egl_xlib_ctx;
  Drawable current_drawable;
  Bool ignore_rotation_p;

  unsigned long frame_count;
  Bool initted_p;
  double next_frame_time;  // time_t in milliseconds of when to tick the frame
  struct event_queue *event_queue;
};

struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  XRectangle frame;
  EGLSurface egl_surface;
  union {
    struct {
      struct running_hack *rh;
      int last_mouse_x, last_mouse_y;
    } window;
    struct {
      int depth;
    } pixmap;
  };
};

extern void do_logv(int prio, const char *fmt, va_list args);

extern void Log(const char *format, ...); // TODO: GCC can verify printf strings.
#define Logv(format, args) (do_logv(ANDROID_LOG_INFO, format, args))

extern void prepare_context (struct running_hack *rh);


// Methods of the Java class org.jwz.jwxyz that are implemented in C.

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_nativeInit (JNIEnv *, jobject thiz,
                                            jstring jhack, jint api,
                                            jobject defaults,
                                            jint w, jint h);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_nativeResize (JNIEnv *, jobject thiz,
                                              jint w, jint h, jdouble rot);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_nativeRender (JNIEnv *, jobject thiz);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_nativeDone (JNIEnv *, jobject thiz);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_allnativeSettings (JNIEnv *, jobject thiz,
                                                   jstring jhack,
                                                   jstring hackPref,
                                                   jint draw, jstring key);

JNIEXPORT jboolean JNICALL
Java_org_jwz_xscreensaver_jwxyz_ignoreRotation (JNIEnv *, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_org_jwz_xscreensaver_jwxyz_suppressRotationAnimation (JNIEnv *,
                                                           jobject thiz);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_sendButtonEvent (JNIEnv *, jobject thiz,
                                                 int x, int y, jboolean down);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_sendMotionEvent (JNIEnv *, jobject thiz,
                                                 int x, int y);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_sendKeyEvent (JNIEnv *, jobject thiz,
                                              jboolean down_p,
                                              int code, int mods);

#endif // __JWXYZ_ANDROID_H__
