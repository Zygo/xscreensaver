LOCAL_PATH := $(call my-dir)/../../..

# -Wnested-externs would also be here, but for Android unistd.h.
SHARED_CFLAGS = \
    -std=c99 \
    -Wall \
    -Wstrict-prototypes \
    -Wmissing-prototypes \
    -DSTANDALONE=1 \
    -DHAVE_ANDROID=1 \
    -DHAVE_GL=1 \
    -DHAVE_JWXYZ=1 \
    -DJWXYZ_GL=1 \
    -DJWXYZ_IMAGE=1 \
    -DHAVE_JWZGLES=1 \
    -DHAVE_XUTF8DRAWSTRING=1 \
    -DHAVE_GLBINDTEXTURE=1 \
    -DGL_VERSION_ES_CM_1_0 \
    -DHAVE_UNISTD_H=1 \
    -DHAVE_INTTYPES_H=1 \
    -DHAVE_UNAME=1 \
    -DHAVE_UTIL_H=1 \
    -DGETTIMEOFDAY_TWO_ARGS=1 \
    -DHAVE_ICMP=1 \
    -DHAVE_PTHREAD=1 \

SHARED_C_INCLUDES = \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/android \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/jwxyz \
    $(LOCAL_PATH)/hacks \
    $(LOCAL_PATH)/hacks/glx \

include $(CLEAR_VARS)

LOCAL_MODULE := xscreensaver-gl

LOCAL_SRC_FILES := \
    android/screenhack-android.c \
    hacks/glx/dropshadow.c \
    hacks/glx/chessmodels.c \
    hacks/glx/fps-gl.c \
    hacks/glx/gltrackball.c \
    hacks/glx/glut_stroke.c \
    hacks/glx/glut_swidth.c \
    hacks/glx/grab-ximage.c \
    hacks/glx/marching.c \
    hacks/glx/normals.c \
    hacks/glx/rotator.c \
    hacks/glx/sphere.c \
    hacks/glx/texfont.c \
    hacks/glx/trackball.c \
    hacks/glx/tube.c \
    hacks/glx/xpm-ximage.c \

# Some savers occupy more than one source file:
LOCAL_SRC_FILES += \
    hacks/glx/b_draw.c \
    hacks/glx/b_lockglue.c \
    hacks/glx/b_sphere.c \
    hacks/glx/buildlwo.c \
    hacks/glx/companion_quad.c \
    hacks/glx/companion_disc.c \
    hacks/glx/companion_heart.c \
    hacks/glx/cow_face.c \
    hacks/glx/cow_hide.c \
    hacks/glx/cow_hoofs.c \
    hacks/glx/cow_horns.c \
    hacks/glx/cow_tail.c \
    hacks/glx/cow_udder.c \
    hacks/glx/dolphin.c \
    hacks/glx/gllist.c \
    hacks/glx/glschool_alg.c \
    hacks/glx/glschool_gl.c \
    hacks/glx/involute.c \
    hacks/glx/lament_model.c \
    hacks/glx/pipeobjs.c \
    hacks/glx/robot.c \
    hacks/glx/robot-wireframe.c \
    hacks/glx/polyhedra-gl.c \
    hacks/glx/s1_1.c \
    hacks/glx/s1_2.c \
    hacks/glx/s1_3.c \
    hacks/glx/s1_4.c \
    hacks/glx/s1_5.c \
    hacks/glx/s1_6.c \
    hacks/glx/s1_b.c \
    hacks/glx/seccam.c \
    hacks/glx/shark.c \
    hacks/glx/sonar-sim.c \
    hacks/glx/sonar-icmp.c \
    hacks/glx/splitflap_obj.c \
    hacks/glx/sproingiewrap.c \
    hacks/glx/stonerview-move.c \
    hacks/glx/stonerview-osc.c \
    hacks/glx/stonerview-view.c \
    hacks/glx/swim.c \
    hacks/glx/tangram_shapes.c \
    hacks/glx/teapot.c \
    hacks/glx/toast.c \
    hacks/glx/toast2.c \
    hacks/glx/toaster.c \
    hacks/glx/toaster_base.c \
    hacks/glx/toaster_handle.c \
    hacks/glx/toaster_handle2.c \
    hacks/glx/toaster_jet.c \
    hacks/glx/toaster_knob.c \
    hacks/glx/toaster_slots.c \
    hacks/glx/toaster_wing.c \
    hacks/glx/tronbit_idle1.c \
    hacks/glx/tronbit_idle2.c \
    hacks/glx/tronbit_no.c \
    hacks/glx/tronbit_yes.c \
    hacks/glx/tunnel_draw.c \
    hacks/glx/whale.c \

# The source files of the currently active GL hacks:
LOCAL_SRC_FILES += $(shell \
  for f in $$ANDROID_HACKS ; do \
    if [ "$$f" = "companioncube" ]; then f="companion"; fi ; \
    if [ -f "../../../hacks/glx/$$f.c" ]; then \
      echo "hacks/glx/$$f.c" ; \
    fi ; \
  done )

LOCAL_C_INCLUDES := $(SHARED_C_INCLUDES)
LOCAL_CFLAGS += $(SHARED_CFLAGS) -DUSE_GL

include $(BUILD_STATIC_LIBRARY)

##############################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := xscreensaver

LOCAL_STATIC_LIBRARIES := xscreensaver-gl

# The base framework files:
LOCAL_SRC_FILES := \
    jwxyz/jwxyz-android.c \
    jwxyz/jwxyz-common.c \
    jwxyz/jwxyz-gl.c \
    jwxyz/jwxyz-image.c \
    jwxyz/jwxyz-timers.c \
    jwxyz/jwzgles.c \

# Utilities used by the hacks:
LOCAL_SRC_FILES += \
    hacks/analogtv.c \
    hacks/delaunay.c \
    hacks/fps.c \
    hacks/xlockmore.c \
    hacks/xpm-pixmap.c \
    utils/async_netdb.c \
    utils/aligned_malloc.c \
    utils/colorbars.c \
    utils/colors.c \
    utils/erase.c \
    utils/grabclient.c \
    utils/hsv.c \
    utils/logo.c \
    utils/minixpm.c \
    utils/pow2.c \
    utils/resources.c \
    utils/spline.c \
    utils/textclient-mobile.c \
    utils/thread_util.c \
    utils/usleep.c \
    utils/utf8wc.c \
    utils/xft.c \
    utils/xshm.c \
    utils/yarandom.c \

# The source files of the currently active Xlib hacks:
LOCAL_SRC_FILES += $(shell \
  for f in $$ANDROID_HACKS ; do \
    if [ -f "../../../hacks/$$f.c" ]; then \
      echo "hacks/$$f.c" ; \
    fi ; \
  done )

# Some savers occupy more than one source file:
LOCAL_SRC_FILES += \
    hacks/apple2-main.c \
    hacks/asm6502.c \
    hacks/pacman_ai.c \
    hacks/pacman_level.c \

LOCAL_LDLIBS := -lGLESv1_CM -ldl -llog -lEGL -latomic -landroid

LOCAL_C_INCLUDES := $(SHARED_C_INCLUDES)
LOCAL_CFLAGS += $(SHARED_CFLAGS)

include $(BUILD_SHARED_LIBRARY)
