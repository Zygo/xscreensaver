/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * xscreensaver, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
 * and Dennis Sheil <dennis@panaceasupplies.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

package org.jwz.xscreensaver;

import java.util.Map;
import java.lang.RuntimeException;
import android.view.Display;
import android.view.WindowManager;
import android.view.Surface;
import android.view.KeyEvent;
import android.content.Context;
import android.graphics.Bitmap;
import android.opengl.GLSurfaceView;
import java.util.Timer;
import java.util.TimerTask;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import org.jwz.xscreensaver.jwxyz;
import android.os.Message;
import android.os.Handler;
import android.util.Log;


public class XScreenSaverRenderer implements GLSurfaceView.Renderer {

  boolean initTried = false;
  jwxyz jwxyz_obj = null;

  String hack;
  Handler.Callback abort_callback;

  Context app;
  Bitmap screenshot;

  GLSurfaceView glview;

  class RenderTask extends TimerTask {
    public void run() {
      glview.requestRender();
    }
  };

  Timer timer = new Timer();

  private void LOG (String fmt, Object... args) {
    Log.d ("xscreensaver",
           this.getClass().getSimpleName() + ": " +
           String.format (fmt, args));
  }

  private void except(Exception e) {
    jwxyz_obj = null;
    Message m = Message.obtain (null, 0, (Object) e);
    abort_callback.handleMessage (m);
  }

  public XScreenSaverRenderer (String hack,
                               Context app,
                               Bitmap screenshot,
                               Handler.Callback abort_callback,
                               GLSurfaceView glview) {
    super();
    this.hack   = hack;
    this.app    = app;
    this.screenshot = screenshot;
    this.abort_callback = abort_callback;
    this.glview = glview;
    LOG ("init %s", hack);

    this.glview.setEGLConfigChooser (8, 8, 8, 8, 16, 0);
    this.glview.setRenderer (this);
    this.glview.setRenderMode (GLSurfaceView.RENDERMODE_WHEN_DIRTY);
  }

  static public String saverNameOf (Object obj) {
    // Extract the saver name from e.g. "gen.Daydream$BouncingCow"
    String name = obj.getClass().getSimpleName();
    int index = name.lastIndexOf('$');
    if (index != -1) {
      index++;
      name = name.substring (index, name.length() - index);
    }
    return name.toLowerCase();
  }

  public void onDrawFrame (GL10 gl) {
    try {
      if (jwxyz_obj != null) {
        long delay = jwxyz_obj.nativeRender();
        // java.util.Timer doesn't seem to like to re-use TimerTasks, so
        // there's a slow object churn here: one TimerTask per frame.
        timer.schedule(new RenderTask(), delay / 1000);
      }
    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void onSurfaceChanged(GL10 gl, int w, int h) {
    try {
      if (jwxyz_obj == null)
        jwxyz_obj = new jwxyz (hack, app, screenshot, w, h);

      double r = 0;

      Display d = glview.getDisplay();

      if (d != null) {
        switch (d.getRotation()) {
        case Surface.ROTATION_90:  r = 90;  break;
        case Surface.ROTATION_180: r = 180; break;
        case Surface.ROTATION_270: r = 270; break;
        }
      }

      jwxyz_obj.nativeResize (w, h, r);

      glview.requestRender();

    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void onSurfaceCreated (GL10 gl, EGLConfig config) {
    try {
      LOG ("onSurfaceCreated %s / %s / %s", 
           gl.glGetString (GL10.GL_VENDOR),
           gl.glGetString (GL10.GL_RENDERER),
           gl.glGetString (GL10.GL_VERSION));

      if (!initTried) {
        initTried = true;
      } else {
        if (jwxyz_obj != null) {
          jwxyz_obj.nativeDone();
          jwxyz_obj = null;
        }
      }
    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void release() {
    try {
      if (jwxyz_obj != null) {
        jwxyz_obj.nativeDone();
        jwxyz_obj = null;
      }
    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void sendButtonEvent (int x, int y, boolean down) {
    try {
      jwxyz_obj.sendButtonEvent (x, y, down);
    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void sendMotionEvent (int x, int y) {
    try {
      jwxyz_obj.sendMotionEvent (x, y);
    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void sendKeyEvent (KeyEvent event) {
    try {
      jwxyz_obj.sendKeyEvent (event);
    } catch (RuntimeException e) {
      except (e);
    }
  }


  static
  {
    System.loadLibrary ("xscreensaver");
  }
}
