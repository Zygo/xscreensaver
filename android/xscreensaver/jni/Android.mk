# Included by android/Makefile
# https://developer.android.com/ndk/guides/android_mk

# Gradle has no mechanism to compile a given .o file with different flags.
# Every .o in a library is compiled with the same options.  So we have to
# have at least two libraries, with -DUSE_GL and also without.  Lovely.

LOCAL_PATH := $(call my-dir)/../../..

COMMON_CFLAGS := \
  -std=c99 \
  -Wall \
  -Wstrict-prototypes \
  -Wmissing-prototypes \
  -DJWXYZ_GL=1 \
  -DJWXYZ_IMAGE=1 \
  -DSTANDALONE=1 \
  -DGETTIMEOFDAY_TWO_ARGS=1 \
  -DHAVE_ANDROID=1 \
  -DHAVE_GETIFADDRS=1 \
  -DHAVE_GL=1 \
  -DHAVE_GLBINDTEXTURE=1 \
  -DHAVE_GLES3=1 \
  -DHAVE_GLSL=1 \
  -DHAVE_ICMP=1 \
  -DHAVE_INTTYPES_H=1 \
  -DHAVE_JWXYZ=1 \
  -DHAVE_JWZGLES=1 \
  -DHAVE_PTHREAD=1 \
  -DHAVE_UNAME=1 \
  -DHAVE_UNISTD_H=1 \
  -DHAVE_UTIL_H=1 \
  -DHAVE_XUTF8DRAWSTRING=1 \

COMMON_C_INCLUDES := \
  $(LOCAL_PATH) \
  $(LOCAL_PATH)/android \
  $(LOCAL_PATH)/utils \
  $(LOCAL_PATH)/jwxyz \
  $(LOCAL_PATH)/hacks \
  $(LOCAL_PATH)/hacks/glx \

COMMON_LDLIBS := -lGLESv1_CM -lGLESv3 -ldl -llog -lEGL -landroid -ljnigraphics


# Library-like source files used by nearly every hack.
# These can all be compiled with -DUSE_GL even if they don't use GL.
#
COMMON_SRC_FILES := \
  android/screenhack-android.c \
  jwxyz/jwxyz-common.c \
  jwxyz/jwxyz-gl.c \
  jwxyz/jwxyz-image.c \
  jwxyz/jwxyz-timers.c \
  jwxyz/jwzgles.c \
  jwxyz/jwxyz-android.c \
  \
  utils/aligned_malloc.c \
  utils/async_netdb.c \
  utils/colorbars.c \
  utils/colors.c \
  utils/doubletime.c \
  utils/easing.c \
  utils/erase.c \
  utils/font-retry.c \
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
  utils/xftwrap.c \
  utils/xshm.c \
  utils/yarandom.c \
  \
  hacks/fps.c \
  hacks/ximage-loader.c \
  hacks/xlockmore.c \
  \
  hacks/glx/fps-gl.c \
  hacks/glx/gllist.c \
  hacks/glx/glsl-utils.c \
  hacks/glx/gltrackball.c \
  hacks/glx/grab-ximage.c \
  hacks/glx/quaternion.c \
  hacks/glx/rotator.c \
  hacks/glx/sphere.c \
  hacks/glx/texfont.c \
  hacks/glx/trackball.c \
  hacks/glx/triangle.c \
  hacks/glx/tube.c \

# Source files that must not be compiled with -DUSE_GL:
X11_SRC_FILES := $(shell \
  for f in $$ANDROID_HACKS ; do \
    if [ -f "../../../hacks/$$f.c" ]; then echo "hacks/$$f.c" ; \
    fi ; \
  done )

# Source files that must be compiled with -DUSE_GL:
GL_SRC_FILES := $(shell \
  for f in $$ANDROID_HACKS ; do \
    if [ "$$f" = "companioncube" ]; then f="companion"; fi ; \
    \
    if   [ -f "../../../hacks/$$f.c"                  ]; then true; \
    elif [ -f "../../../hacks/glx/$$f.c" ]; then echo "hacks/glx/$$f.c" ; \
    elif [ -f "../../../hacks/glx/glsl/$$f.glsl"      ]; then true ; \
    elif [ -f "../../../hacks/glx/glsl/$${f}-0.glsl"  ]; then true ; \
    elif [ -f "../../../hacks/glx/glsl/$${f}0-0.glsl" ]; then true ; \
    else echo "SRC NOT FOUND: $$f" >&2 ; \
    fi ; \
  done )

# Hacks with more than one src file:
X11_SRC_FILES += \
  hacks/analogtv.c \
  hacks/ansi-tty.c \
  hacks/apple2-main.c \
  hacks/asm6502.c \
  hacks/delaunay.c \
  hacks/pacman_ai.c \
  hacks/pacman_level.c \

GL_SRC_FILES += \
  hacks/glx/b_draw.c \
  hacks/glx/b_lockglue.c \
  hacks/glx/b_sphere.c \
  hacks/glx/buildlwo.c \
  hacks/glx/chessmodels.c \
  hacks/glx/companion_disc.c \
  hacks/glx/companion_heart.c \
  hacks/glx/companion_quad.c \
  hacks/glx/countries.c \
  hacks/glx/cow_face.c \
  hacks/glx/cow_hide.c \
  hacks/glx/cow_hoofs.c \
  hacks/glx/cow_horns.c \
  hacks/glx/cow_tail.c \
  hacks/glx/cow_udder.c \
  hacks/glx/dolphin.c \
  hacks/glx/dropshadow.c \
  hacks/glx/dumpster_model.c \
  hacks/glx/dymaxionmap-coords.c \
  hacks/glx/earth.c \
  hacks/glx/floppy_model.c \
  hacks/glx/glschool_alg.c \
  hacks/glx/glschool_gl.c \
  hacks/glx/glut_stroke.c \
  hacks/glx/glut_swidth.c \
  hacks/glx/handsy_model.c \
  hacks/glx/headroom_model.c \
  hacks/glx/highvoltage_model.c \
  hacks/glx/hopfanimations.c \
  hacks/glx/involute.c \
  hacks/glx/kallisti_model.c \
  hacks/glx/klondike-game.c \
  hacks/glx/lament_model.c \
  hacks/glx/marching.c \
  hacks/glx/normals.c \
  hacks/glx/pipeobjs.c \
  hacks/glx/polyhedra-gl.c \
  hacks/glx/quickhull.c \
  hacks/glx/robot-wireframe.c \
  hacks/glx/robot.c \
  hacks/glx/s1_1.c \
  hacks/glx/s1_2.c \
  hacks/glx/s1_3.c \
  hacks/glx/s1_4.c \
  hacks/glx/s1_5.c \
  hacks/glx/s1_6.c \
  hacks/glx/s1_b.c \
  hacks/glx/seccam.c \
  hacks/glx/shark.c \
  hacks/glx/ships.c \
  hacks/glx/skull_model.c \
  hacks/glx/sonar-icmp.c \
  hacks/glx/sonar-sim.c \
  hacks/glx/sphereeversion-analytic.c \
  hacks/glx/sphereeversion-corrugations.c \
  hacks/glx/splitflap_obj.c \
  hacks/glx/sproingiewrap.c \
  hacks/glx/stonerview-move.c \
  hacks/glx/stonerview-osc.c \
  hacks/glx/stonerview-view.c \
  hacks/glx/swim.c \
  hacks/glx/tangram_shapes.c \
  hacks/glx/teapot.c \
  hacks/glx/teeth_model.c \
  hacks/glx/timezones.c \
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
  hacks/glx/xshadertoy.c \

# These don't work well enough to turn on by default:
#
# hacks/glx/flurry-smoke.c \
# hacks/glx/flurry-spark.c \
# hacks/glx/flurry-star.c \
# hacks/glx/flurry-texture.c \


##############################################################################
# xscreensaver-x11.so for the X11 hacks

include $(CLEAR_VARS)

LOCAL_MODULE     := xscreensaver-x11
LOCAL_CFLAGS     := $(COMMON_CFLAGS)
LOCAL_C_INCLUDES := $(COMMON_C_INCLUDES)
LOCAL_LDLIBS     := $(COMMON_LDLIBS)
LOCAL_SRC_FILES  := $(X11_SRC_FILES)
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(BUILD_SHARED_LIBRARY)

##############################################################################
# xscreensaver-gl.so for the GL hacks

include $(CLEAR_VARS)

LOCAL_MODULE     := xscreensaver-gl
LOCAL_CFLAGS     := $(COMMON_CFLAGS) -DUSE_GL
LOCAL_C_INCLUDES := $(COMMON_C_INCLUDES)
LOCAL_LDLIBS     := $(COMMON_LDLIBS)
LOCAL_SRC_FILES  := $(GL_SRC_FILES)
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(BUILD_SHARED_LIBRARY)

##############################################################################
# xscreensaver.so for the common code, and to tie it all together.

include $(CLEAR_VARS)

LOCAL_MODULE     := xscreensaver
LOCAL_CFLAGS     := $(COMMON_CFLAGS) -DUSE_GL
LOCAL_C_INCLUDES := $(COMMON_C_INCLUDES)
LOCAL_LDLIBS     := $(COMMON_LDLIBS)
LOCAL_SRC_FILES  := $(COMMON_SRC_FILES)
LOCAL_SHARED_LIBRARIES := xscreensaver-x11 xscreensaver-gl

include $(BUILD_SHARED_LIBRARY)
