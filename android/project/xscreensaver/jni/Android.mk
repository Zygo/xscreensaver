LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := xscreensaver

LOCAL_SRC_FILES := \
    xscreensaver/hacks/xlockmore.c \
    xscreensaver/hacks/fps.c \
    xscreensaver/hacks/glx/fps-gl.c \
    xscreensaver/hacks/glx/jwzgles.c \
    xscreensaver/hacks/glx/rotator.c \
    xscreensaver/hacks/glx/tube.c \
    xscreensaver/hacks/glx/sphere.c \
    xscreensaver/hacks/glx/sproingies.c \
    xscreensaver/hacks/glx/sproingiewrap.c \
    xscreensaver/hacks/glx/gllist.c \
    xscreensaver/hacks/glx/s1_1.c \
    xscreensaver/hacks/glx/s1_2.c \
    xscreensaver/hacks/glx/s1_3.c \
    xscreensaver/hacks/glx/s1_4.c \
    xscreensaver/hacks/glx/s1_5.c \
    xscreensaver/hacks/glx/s1_6.c \
    xscreensaver/hacks/glx/s1_b.c \
    xscreensaver/hacks/glx/superquadrics.c \
    xscreensaver/hacks/glx/trackball.c \
    xscreensaver/hacks/glx/gltrackball.c \
    xscreensaver/hacks/glx/texfont.c \
    xscreensaver/hacks/glx/stonerview.c \
    xscreensaver/hacks/glx/stonerview-move.c \
    xscreensaver/hacks/glx/stonerview-osc.c \
    xscreensaver/hacks/glx/stonerview-view.c \
    xscreensaver/hacks/glx/hilbert.c \
    xscreensaver/hacks/glx/xpm-ximage.c \
    xscreensaver/hacks/glx/cow_face.c \
    xscreensaver/hacks/glx/cow_hide.c \
    xscreensaver/hacks/glx/cow_hoofs.c \
    xscreensaver/hacks/glx/cow_horns.c \
    xscreensaver/hacks/glx/cow_tail.c \
    xscreensaver/hacks/glx/cow_udder.c \
    xscreensaver/hacks/glx/bouncingcow.c \
    xscreensaver/hacks/glx/unknownpleasures.c \
    xscreensaver/hacks/glx/glhanoi.c \
    xscreensaver/utils/minixpm.c \
    xscreensaver/utils/hsv.c \
    xscreensaver/utils/colors.c \
    xscreensaver/utils/resources.c \
    xscreensaver/utils/xft.c \
    xscreensaver/utils/utf8wc.c \
    xscreensaver/utils/yarandom.c \
    xscreensaver/android/XScreenSaverView.c \
    xscreensaver/android/XScreenSaverGLView.c \
    xscreensaver/android/jwxyz.c \
    xscreensaver/android/gen/glue.c

LOCAL_LDLIBS := -lGLESv1_CM -ldl -llog

LOCAL_C_INCLUDES := $(LOCAL_PATH)/xscreensaver $(LOCAL_PATH)/xscreensaver/android $(LOCAL_PATH)/xscreensaver/utils $(LOCAL_PATH)/xscreensaver/hacks $(LOCAL_PATH)/xscreensaver/hacks/glx

LOCAL_CFLAGS += -std=c99 -DSTANDALONE=1 -DUSE_GL=1 -DGETTIMEOFDAY_TWO_ARGS=1 -DHAVE_JWZGLES=1 -DHAVE_ANDROID=1 -DGL_VERSION_ES_CM_1_0

include $(BUILD_SHARED_LIBRARY)
