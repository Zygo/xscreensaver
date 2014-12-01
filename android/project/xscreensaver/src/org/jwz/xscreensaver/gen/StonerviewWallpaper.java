package org.jwz.xscreensaver.gen;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import net.rbgrn.android.glwallpaperservice.*;
import android.opengl.GLU;

import android.content.Context;
import android.content.SharedPreferences;
import org.jwz.xscreensaver.*;

public class StonerviewWallpaper extends ARenderer {

    private static native void nativeSettings(String hack, String hackPref, int draw);
    public static final String SHARED_PREFS_NAME="stonerviewsettings";
    CallNative cn;

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {

        super.onSurfaceCreated(gl, config);

        cn = new CallNative();
        NonSurfaceCreated();
    }

    public void onDrawFrame(GL10 gl) {
        super.onDrawFrame(gl);
        nativeSettings("bogus", "bogus", 1);
        NonDrawFrame();
    }

    void NonDrawFrame() {
        cn.nativeRender();
    }

    void doSP(SharedPreferences sspp) {
        String hackPref = sspp.getString("stonerview_speed", "1"); // key
        String hack = "stonerview";
        nativeSettings(hack, hackPref, 0);
    }



    static
    {
        System.loadLibrary ("xscreensaver");
    }

}
