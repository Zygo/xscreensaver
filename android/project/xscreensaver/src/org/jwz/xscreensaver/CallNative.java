package org.jwz.xscreensaver;

public class CallNative {

    void onSurfaceCreated() {
        nativeInit();
    }

    void onSurfaceChanged(int w, int h) {
        nativeResize(w, h);
    }

    void onDrawFrame() {
        nativeRender();
    }

    void onDone() {
        nativeDone();
    }

    public static native void nativeInit();
    public static native void nativeResize(int w, int h);
    public static native void nativeRender();
    public static native void nativeDone();
}
