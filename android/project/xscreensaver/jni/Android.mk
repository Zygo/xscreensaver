LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := xscreensaver

# The base framework files:
LOCAL_SRC_FILES := \
    xscreensaver/android/screenhack-android.c \
    xscreensaver/android/grabscreen-android.c \
    xscreensaver/jwxyz/jwxyz-android.c \
    xscreensaver/jwxyz/jwxyz-common.c \
    xscreensaver/jwxyz/jwxyz-gl.c \
    xscreensaver/jwxyz/jwxyz-timers.c \
    xscreensaver/jwxyz/jwzgles.c \

# Utilities used by the hacks:
LOCAL_SRC_FILES += \
    xscreensaver/hacks/analogtv.c \
    xscreensaver/hacks/delaunay.c \
    xscreensaver/hacks/fps.c \
    xscreensaver/hacks/glx/dropshadow.c \
    xscreensaver/hacks/glx/chessmodels.c \
    xscreensaver/hacks/glx/fps-gl.c \
    xscreensaver/hacks/glx/gltrackball.c \
    xscreensaver/hacks/glx/glut_stroke.c \
    xscreensaver/hacks/glx/glut_swidth.c \
    xscreensaver/hacks/glx/grab-ximage.c \
    xscreensaver/hacks/glx/marching.c \
    xscreensaver/hacks/glx/normals.c \
    xscreensaver/hacks/glx/rotator.c \
    xscreensaver/hacks/glx/sphere.c \
    xscreensaver/hacks/glx/texfont.c \
    xscreensaver/hacks/glx/trackball.c \
    xscreensaver/hacks/glx/tube.c \
    xscreensaver/hacks/glx/xpm-ximage.c \
    xscreensaver/hacks/xlockmore.c \
    xscreensaver/hacks/xpm-pixmap.c \
    xscreensaver/utils/async_netdb.c \
    xscreensaver/utils/aligned_malloc.c \
    xscreensaver/utils/colorbars.c \
    xscreensaver/utils/colors.c \
    xscreensaver/utils/erase.c \
    xscreensaver/utils/grabclient.c \
    xscreensaver/utils/hsv.c \
    xscreensaver/utils/logo.c \
    xscreensaver/utils/minixpm.c \
    xscreensaver/utils/resources.c \
    xscreensaver/utils/spline.c \
    xscreensaver/utils/textclient-mobile.c \
    xscreensaver/utils/thread_util.c \
    xscreensaver/utils/usleep.c \
    xscreensaver/utils/utf8wc.c \
    xscreensaver/utils/xft.c \
    xscreensaver/utils/yarandom.c \

# The source files of all of the currently active hacks:
LOCAL_SRC_FILES += $(shell \
  for f in $$ANDROID_HACKS ; do \
    if [ "$$f" = "companioncube" ]; then f="companion"; fi ; \
    if [ -f "../../../../hacks/$$f.c" ]; then \
      echo "xscreensaver/hacks/$$f.c" ; \
    else \
      echo "xscreensaver/hacks/glx/$$f.c" ; \
    fi ; \
  done )

# Some savers occupy more than one source file:
LOCAL_SRC_FILES += \
    xscreensaver/hacks/apple2-main.c \
    xscreensaver/hacks/asm6502.c \
    xscreensaver/hacks/pacman_ai.c \
    xscreensaver/hacks/pacman_level.c \
    xscreensaver/hacks/glx/b_draw.c \
    xscreensaver/hacks/glx/b_lockglue.c \
    xscreensaver/hacks/glx/b_sphere.c \
    xscreensaver/hacks/glx/buildlwo.c \
    xscreensaver/hacks/glx/companion_quad.c \
    xscreensaver/hacks/glx/companion_disc.c \
    xscreensaver/hacks/glx/companion_heart.c \
    xscreensaver/hacks/glx/cow_face.c \
    xscreensaver/hacks/glx/cow_hide.c \
    xscreensaver/hacks/glx/cow_hoofs.c \
    xscreensaver/hacks/glx/cow_horns.c \
    xscreensaver/hacks/glx/cow_tail.c \
    xscreensaver/hacks/glx/cow_udder.c \
    xscreensaver/hacks/glx/dolphin.c \
    xscreensaver/hacks/glx/gllist.c \
    xscreensaver/hacks/glx/glschool_alg.c \
    xscreensaver/hacks/glx/glschool_gl.c \
    xscreensaver/hacks/glx/involute.c \
    xscreensaver/hacks/glx/lament_model.c \
    xscreensaver/hacks/glx/pipeobjs.c \
    xscreensaver/hacks/glx/robot.c \
    xscreensaver/hacks/glx/robot-wireframe.c \
    xscreensaver/hacks/glx/polyhedra-gl.c \
    xscreensaver/hacks/glx/s1_1.c \
    xscreensaver/hacks/glx/s1_2.c \
    xscreensaver/hacks/glx/s1_3.c \
    xscreensaver/hacks/glx/s1_4.c \
    xscreensaver/hacks/glx/s1_5.c \
    xscreensaver/hacks/glx/s1_6.c \
    xscreensaver/hacks/glx/s1_b.c \
    xscreensaver/hacks/glx/shark.c \
    xscreensaver/hacks/glx/sonar-sim.c \
    xscreensaver/hacks/glx/sonar-icmp.c \
    xscreensaver/hacks/glx/splitflap_obj.c \
    xscreensaver/hacks/glx/sproingiewrap.c \
    xscreensaver/hacks/glx/stonerview-move.c \
    xscreensaver/hacks/glx/stonerview-osc.c \
    xscreensaver/hacks/glx/stonerview-view.c \
    xscreensaver/hacks/glx/swim.c \
    xscreensaver/hacks/glx/tangram_shapes.c \
    xscreensaver/hacks/glx/teapot.c \
    xscreensaver/hacks/glx/toast.c \
    xscreensaver/hacks/glx/toast2.c \
    xscreensaver/hacks/glx/toaster.c \
    xscreensaver/hacks/glx/toaster_base.c \
    xscreensaver/hacks/glx/toaster_handle.c \
    xscreensaver/hacks/glx/toaster_handle2.c \
    xscreensaver/hacks/glx/toaster_jet.c \
    xscreensaver/hacks/glx/toaster_knob.c \
    xscreensaver/hacks/glx/toaster_slots.c \
    xscreensaver/hacks/glx/toaster_wing.c \
    xscreensaver/hacks/glx/tronbit_idle1.c \
    xscreensaver/hacks/glx/tronbit_idle2.c \
    xscreensaver/hacks/glx/tronbit_no.c \
    xscreensaver/hacks/glx/tronbit_yes.c \
    xscreensaver/hacks/glx/tunnel_draw.c \
    xscreensaver/hacks/glx/whale.c \

LOCAL_LDLIBS := -lGLESv1_CM -ldl -llog -lEGL

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/xscreensaver \
	$(LOCAL_PATH)/xscreensaver/android \
	$(LOCAL_PATH)/xscreensaver/utils \
	$(LOCAL_PATH)/xscreensaver/jwxyz \
	$(LOCAL_PATH)/xscreensaver/hacks \
	$(LOCAL_PATH)/xscreensaver/hacks/glx \

# -Wnested-externs would also be here, but for Android unistd.h.
LOCAL_CFLAGS += \
	-std=c99 \
	-Wall \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-DSTANDALONE=1 \
	-DHAVE_ANDROID=1 \
	-DUSE_GL=1 \
	-DHAVE_JWXYZ=1 \
	-DJWXYZ_GL=1 \
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

include $(BUILD_SHARED_LIBRARY)
