package org.jwz.xscreensaver;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import net.rbgrn.android.glwallpaperservice.*;
import android.opengl.GLU;

import android.content.Context;
import android.content.SharedPreferences;


// Original code provided by Robert Green
// http://www.rbgrn.net/content/354-glsurfaceview-adapted-3d-live-wallpapers
public class ARenderer implements GLWallpaperService.Renderer {

    CallNative cn;
    boolean secondRun = false;

    // Used for Lighting
    //private static float[] ambientComponent0 = {0.3f, 0.3f, 1.0f, 1.0f};
    //private static float[] ambientComponent0 = {0.3f, 0.3f, 0.3f, 1.0f};
    private static float[] ambientComponent0 = {0.5f, 0.5f, 0.5f, 1.0f};
    private static float[] diffuseComponent0 = {1.0f, 1.0f, 1.0f, 1.0f};
    private static float[] lightPosition0 =    {1f, 1f, -1f, 0f};

    public void onDrawFrame(GL10 gl) {
        if (!secondRun) {
            secondRun = true;
            return;
        }
        //gl.glClearColor(0.2f, 0.6f, 0.2f, 1f);
        gl.glClear(GL10.GL_COLOR_BUFFER_BIT | GL10.GL_DEPTH_BUFFER_BIT);
    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {

        gl.glMatrixMode(GL10.GL_PROJECTION);
        gl.glLoadIdentity();
        GLU.gluPerspective(gl, 60f, (float)width/(float)height, 1f, 100f);

        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glLoadIdentity();

        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glTranslatef(0, 0, -5);
        gl.glRotatef(30f, 1, 0, 0);

        gl.glEnable(GL10.GL_LIGHTING);
        gl.glEnable(GL10.GL_RESCALE_NORMAL);
        gl.glEnableClientState(GL10.GL_NORMAL_ARRAY);
//Set the color of light bouncing off of surfaces to respect the surface color
        gl.glEnable(GL10.GL_COLOR_MATERIAL);

        setupLightSources(gl);

// Turn on a global ambient light. The "Cosmic Background Radiation", if you will.
        //float[] ambientLightRGB = {0.3f, 0.3f, 0.3f, 1.0f};
        float[] ambientLightRGB = {0.5f, 0.5f, 0.5f, 1.0f};
        gl.glLightModelfv(GL10.GL_LIGHT_MODEL_AMBIENT, ambientLightRGB, 0);
        NonSurfaceChanged(width, height);
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {

        if (!secondRun) {
            return;
        }
        cn = new CallNative();

        gl.glClearDepthf(1f);
        gl.glEnable(GL10.GL_DEPTH_TEST);
        gl.glDepthFunc(GL10.GL_LEQUAL);

//Turn on culling, so OpenGL only draws one side of the primitives
        gl.glEnable(GL10.GL_CULL_FACE);
//Define the front of a primitive to be the side where the listed vertexes are counterclockwise
        gl.glFrontFace(GL10.GL_CCW);
//Do not draw the backs of primitives
        gl.glCullFace(GL10.GL_BACK);

    }

    private void setupLightSources(GL10 gl) {
        if (!secondRun) {
            return;
        }
        //Enable Light source 0
        gl.glEnable(GL10.GL_LIGHT0);

        //Useful part of the Arrays start a 0
        gl.glLightfv(GL10.GL_LIGHT0, GL10.GL_AMBIENT, ambientComponent0, 0);
        gl.glLightfv(GL10.GL_LIGHT0, GL10.GL_DIFFUSE, diffuseComponent0, 0);

        //Position the light in the scene
        gl.glLightfv(GL10.GL_LIGHT0, GL10.GL_POSITION, lightPosition0, 0);
    }

    /**
     * Called when the engine is destroyed. Do any necessary clean up because
     * at this point your renderer instance is now done for.
     */
    public void release() {
        NonDone();

    }

    public void NonSurfaceCreated() {
        cn.nativeInit();
    }

    void NonSurfaceChanged(int w, int h) {
        if (!secondRun) {
            return;
        }
        cn.nativeResize(w, h);
    }

    void NonDrawFrame() {
        cn.nativeRender();
    }

    void NonDone() {
        cn.nativeDone();
    }

    static
    {
        System.loadLibrary ("xscreensaver");
    }

}
