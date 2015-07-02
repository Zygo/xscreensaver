#include <jni.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <GLES/gl.h>

#include "screenhackI.h"
#include "jwzglesI.h"
#include "version.h"

void drawXscreensaver();

int sWindowWidth = 0;
int sWindowHeight = 0;
int initTried = 0;
int renderTried = 0;
int resetTried = 0;
int currentFlip = 0;

pthread_mutex_t mutg = PTHREAD_MUTEX_INITIALIZER;

extern struct xscreensaver_function_table *xscreensaver_function_table;

// if adding a table here, increase the magic number
struct xscreensaver_function_table
*hypertorus_xscreensaver_function_table,
    *hilbert_xscreensaver_function_table,
    *endgame_xscreensaver_function_table,
    *stonerview_xscreensaver_function_table,
    *sproingies_xscreensaver_function_table,
    *blinkbox_xscreensaver_function_table,
    *bouncingcow_xscreensaver_function_table,
    *superquadrics_xscreensaver_function_table;

struct running_hack {
    struct xscreensaver_function_table *xsft;
    Display *dpy;
    Window window;
    void *closure;
};

const char *progname;
const char *progclass;

struct running_hack rh[8];
// ^ magic number of hacks - TODO: remove magic number


int chosen;
// 0 superquadrics
// 1 stonerview
// 2 sproingies
// 3 hilbert

JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeInit
    (JNIEnv * env);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeResize
    (JNIEnv * env, jobject thiz, jint w, jint h);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeRender
    (JNIEnv * env);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeDone
    (JNIEnv * env);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_SproingiesWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_SuperquadricsWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_HilbertWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_StonerviewWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key);
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_BouncingcowWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key);



void doinit()
{

    if (chosen == 0) {
	progname = "superquadrics";
	rh[chosen].xsft = &superquadrics_xscreensaver_function_table;
    } else if (chosen == 1) {
	progname = "stonerview";
	rh[chosen].xsft = &stonerview_xscreensaver_function_table;

    } else if (chosen == 2) {
	progname = "sproingies";
	rh[chosen].xsft = &sproingies_xscreensaver_function_table;

    } else if (chosen == 3) {
	progname = "hilbert";
	rh[chosen].xsft = &hilbert_xscreensaver_function_table;
    } else if (chosen == 4) {
	progname = "bouncingcow";
	rh[chosen].xsft = &bouncingcow_xscreensaver_function_table;
    } else {
	progname = "sproingies";
	rh[chosen].xsft = &sproingies_xscreensaver_function_table;
    }

    rh[chosen].dpy = jwxyz_make_display(0, 0);
    rh[chosen].window = XRootWindow(rh[chosen].dpy, 0);
// TODO: Zero looks right, but double-check that is the right number

    progclass = rh[chosen].xsft->progclass;

    if (rh[chosen].xsft->setup_cb)
	rh[chosen].xsft->setup_cb(rh[chosen].xsft,
				  rh[chosen].xsft->setup_arg);

    if (resetTried < 1) {
	resetTried++;
        jwzgles_reset();
    }

    void *(*init_cb) (Display *, Window, void *) =
	(void *(*)(Display *, Window, void *)) rh[chosen].xsft->init_cb;

    rh[chosen].closure =
	init_cb(rh[chosen].dpy, rh[chosen].window,
		rh[chosen].xsft->setup_arg);

}



void drawXscreensaver()
{
    pthread_mutex_lock(&mutg);
    rh[chosen].xsft->draw_cb(rh[chosen].dpy, rh[chosen].window,
			     rh[chosen].closure);
    pthread_mutex_unlock(&mutg);

}


JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeInit
    (JNIEnv * env) {

    if (initTried < 1) {
	initTried++;
    } else {
	if (!rh[chosen].dpy) {
	    doinit();
	} else {
	    rh[chosen].xsft->free_cb(rh[chosen].dpy, rh[chosen].window,
				     rh[chosen].closure);
	    jwxyz_free_display(rh[chosen].dpy);
	    rh[chosen].dpy = NULL;
	    rh[chosen].window = NULL;
	    if (!rh[chosen].dpy) {
		doinit();
	    }

	}
    }

}


JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeResize
    (JNIEnv * env, jobject thiz, jint w, jint h) {

    sWindowWidth = w;
    sWindowHeight = h;

    if (!rh[chosen].dpy) {
	doinit();
    }

    jwxyz_window_resized(rh[chosen].dpy, rh[chosen].window, 0, 0, w, h, 0);

    rh[chosen].xsft->reshape_cb(rh[chosen].dpy, rh[chosen].window,
				rh[chosen].closure, w, h);
}

JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeRender
    (JNIEnv * env) {
    if (renderTried < 1) {
	renderTried++;
    } else {
	drawXscreensaver();
    }
}

// TODO: Check Java side is calling this properly
JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_CallNative_nativeDone
    (JNIEnv * env) {

    rh[chosen].xsft->free_cb(rh[chosen].dpy, rh[chosen].window,
			     rh[chosen].closure);
    jwxyz_free_display(rh[chosen].dpy);
    rh[chosen].dpy = NULL;
    rh[chosen].window = NULL;

}

JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_HilbertWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key) {


    const char *chack = (*env)->GetStringUTFChars(env, hackPref, NULL);
    char *hck = (char *) chack;
    const char *kchack = (*env)->GetStringUTFChars(env, key, NULL);
    char *khck = (char *) kchack;

    if (draw == 2) {
        setHilbertSettings(hck, khck);
    }

    chosen = 3;
}

JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_SuperquadricsWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key) {

    const char *chack = (*env)->GetStringUTFChars(env, hackPref, NULL);
    char *hck = (char *) chack;

    const char *kchack = (*env)->GetStringUTFChars(env, key, NULL);
    char *khck = (char *) kchack;

    if (draw == 2) {
        setSuperquadricsSettings(hck, khck);
    }

    chosen = 0;
}


JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_SproingiesWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key) {

    const char *chack = (*env)->GetStringUTFChars(env, hackPref, NULL);
    char *hck = (char *) chack;

    const char *kchack = (*env)->GetStringUTFChars(env, key, NULL);
    char *khck = (char *) kchack;

    if (draw == 2) {
        setSproingiesSettings(hck, khck);
    }

    chosen = 2;
}

JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_StonerviewWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key) {

    const char *chack = (*env)->GetStringUTFChars(env, hackPref, NULL);
    char *hck = (char *) chack;
    const char *kchack = (*env)->GetStringUTFChars(env, key, NULL);
    char *khck = (char *) kchack;

    if (draw == 2) {
        setStonerviewSettings(hck, khck);
    }

    chosen = 1;
}

JNIEXPORT void JNICALL
    Java_org_jwz_xscreensaver_gen_BouncingcowWallpaper_allnativeSettings
    (JNIEnv * env, jobject thiz, jstring jhack, jstring hackPref,
     jint draw, jstring key) {

    const char *chack = (*env)->GetStringUTFChars(env, hackPref, NULL);
    char *hck = (char *) chack;
    const char *kchack = (*env)->GetStringUTFChars(env, key, NULL);
    char *khck = (char *) kchack;

    if (draw == 2) {
        setBouncingcowSettings(hck, khck);
    }

    chosen = 4;
}

