/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * xscreensaver, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * The superclass of every saver's Wallpaper.
 *
 * Each Wallpaper needs a distinct subclass in order to show up in the list.
 * We know which saver we are running by the subclass name; we know which
 * API to use by how the subclass calls super().
 */

package org.jwz.xscreensaver;

import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Message;
import android.service.wallpaper.WallpaperService;
import android.view.GestureDetector;
import android.view.SurfaceHolder;
import android.util.Log;
import java.lang.RuntimeException;
import java.lang.Thread;

public class XScreenSaverWallpaper extends WallpaperService
/*implements GestureDetector.OnGestureListener,
             GestureDetector.OnDoubleTapListener, */ {

  /* TODO: Input! */

  @Override
  public Engine onCreateEngine() {
    return new XScreenSaverGLEngine();
  }

  class XScreenSaverGLEngine extends Engine
  implements Handler.Callback {

    GLSurfaceView glview;

    @Override
    public void onSurfaceCreated (SurfaceHolder holder) {
      /* Old GLWallpaperService warns against working with a GLSurfaceView
         in the Engine constructor.
       */
      glview = new GLSurfaceView (XScreenSaverWallpaper.this) {
        @Override
        public SurfaceHolder getHolder() {
          return XScreenSaverGLEngine.this.getSurfaceHolder();
        }
      };

      new XScreenSaverRenderer (
        XScreenSaverRenderer.saverNameOf (XScreenSaverWallpaper.this),
        XScreenSaverWallpaper.this, null, this, glview);
      // super.onSurfaceCreated(holder);
      glview.surfaceCreated(holder);
      // glview.requestRender();
    }

    @Override
    public void onVisibilityChanged(final boolean visible) {
      if (glview != null) {
        if (visible)
          glview.onResume();
        else
          glview.onPause();
      }
    }

    /* These aren't necessary...maybe? */
/*
    @Override
    public void onSurfaceChanged (SurfaceHolder holder, int format,
                                  int width, int height) {
      if (glview)
        glview.surfaceChanged(holder, format, width, height);
      // super.onSurfaceChanged(holder, format, width, height);
    }

    @Override
    public void onSurfaceDestroyed (SurfaceHolder holder) {
      glview.surfaceDestroyed(holder);
    }
*/

    @Override
    public boolean handleMessage (Message msg) {
      Log.d ("xscreensaver", msg.obj.toString());
      throw (RuntimeException)msg.obj;
      // return false;
    }
  }
}
