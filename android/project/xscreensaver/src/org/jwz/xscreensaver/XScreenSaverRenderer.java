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
  int api;
  Handler.Callback abort_callback;

  Iterable<Map.Entry<String, String>> prefs;
  Context app;
  WindowManager wm;
  Bitmap screenshot;

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

  public XScreenSaverRenderer (String hack, int api,
                               Context app, WindowManager wm,
                               Bitmap screenshot,
                               Handler.Callback abort_callback) {
    super();
    this.hack   = hack;
    this.api    = api;
    this.app    = app;
    this.wm     = wm;
    this.prefs  = prefs;
    this.screenshot = screenshot;
    this.abort_callback = abort_callback;
    LOG ("init %s %d", hack, api);
  }

  public void onDrawFrame (GL10 gl) {
    try {
      if (jwxyz_obj != null)
        jwxyz_obj.nativeRender();
    } catch (RuntimeException e) {
      except (e);
    }
  }

  public void onSurfaceChanged(GL10 gl, int w, int h) {
    try {
      if (jwxyz_obj == null)
        jwxyz_obj = new jwxyz (hack, api, app, screenshot, w, h);

      double r;

      Display d = wm.getDefaultDisplay();

      switch (d.getRotation()) {
      case Surface.ROTATION_90:  r = 90;  break;
      case Surface.ROTATION_180: r = 180; break;
      case Surface.ROTATION_270: r = 270; break;
      default: r = 0; break;
      }

      jwxyz_obj.nativeResize (w, h, r);

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
