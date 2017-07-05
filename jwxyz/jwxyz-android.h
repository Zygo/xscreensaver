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
/* Native EGL is Android 2.3/API 9. EGL in Java is available from API 1. */
#include <EGL/egl.h>
#include <GLES/glext.h>
#include <jni.h>

/* Keep synchronized with check-configs.pl and jwxyz.java. */
#define API_XLIB 0
#define API_GL   1

struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  XRectangle frame;
  union {
    EGLSurface egl_surface;
    GLuint texture; /* If this is 0, it's the default framebuffer. */
  };
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

struct running_hack {
  struct xscreensaver_function_table *xsft;
  jint api;
  Display *dpy;
  Window window;
  fps_state *fpst;
  void *closure;
  JNIEnv *jni_env;
  jobject jobject;

  /* These are set up in Java by the GLSurfaceView. */
  EGLContext egl_ctx;
  EGLSurface egl_surface;
  EGLDisplay egl_display;
  GLint fb_default;

  EGLConfig egl_config;

  PFNGLBINDFRAMEBUFFEROESPROC glBindFramebufferOES;
  PFNGLFRAMEBUFFERTEXTURE2DOESPROC glFramebufferTexture2DOES;

  struct jwxyz_Drawable frontbuffer;
  GC copy_gc;
  Bool gl_fbo_p, frontbuffer_p;
  GLuint fb_pixmap;

  Drawable current_drawable;
  Bool ignore_rotation_p;

  jwzgles_state *gles_state;

  unsigned long frame_count;
  Bool initted_p;
  struct event_queue *event_queue;
};


extern void prepare_context (struct running_hack *rh);


// Methods of the Java class org.jwz.jwxyz that are implemented in C.

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_nativeInit (JNIEnv *, jobject thiz,
                                            jstring jhack,
                                            jobject defaults,
                                            jint w, jint h);

JNIEXPORT void JNICALL
Java_org_jwz_xscreensaver_jwxyz_nativeResize (JNIEnv *, jobject thiz,
                                              jint w, jint h, jdouble rot);

JNIEXPORT jlong JNICALL
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
