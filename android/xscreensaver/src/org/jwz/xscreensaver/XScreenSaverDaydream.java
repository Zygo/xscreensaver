/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * xscreensaver, Copyright (c) 2016-2017 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * The superclass of every saver's Daydream.
 *
 * Each Daydream needs a distinct subclass in order to show up in the list.
 * We know which saver we are running by the subclass name; we know which
 * API to use by how the subclass calls super().
 */

package org.jwz.xscreensaver;

import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.KeyEvent;
import android.service.dreams.DreamService;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Message;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

public class XScreenSaverDaydream extends DreamService {

  private class SaverView extends SurfaceView
    implements SurfaceHolder.Callback {

    private boolean initTried = false;
    private jwxyz jwxyz_obj;

    private GestureDetector detector;

    private Runnable on_quit = new Runnable() {
      @Override
      public void run() {
        finish();  // Exit the Daydream
      }
    };

    SaverView () {
      super (XScreenSaverDaydream.this);
      getHolder().addCallback(this);
    }

    @Override
    public void surfaceChanged (SurfaceHolder holder, int format,
                                int width, int height) {

      if (width == 0 || height == 0) {
        detector = null;
        jwxyz_obj.close();
        jwxyz_obj = null;
      }

      Log.d ("xscreensaver",
             String.format("surfaceChanged: %dx%d", width, height));

      /*
      double r = 0;

      Display d = view.getDisplay();

      if (d != null) {
        switch (d.getRotation()) {
        case Surface.ROTATION_90:  r = 90;  break;
        case Surface.ROTATION_180: r = 180; break;
        case Surface.ROTATION_270: r = 270; break;
        }
      }
      */

      if (jwxyz_obj == null) {
        jwxyz_obj = new jwxyz (jwxyz.saverNameOf (XScreenSaverDaydream.this),
                               XScreenSaverDaydream.this, screenshot,
                               width, height, holder.getSurface(), on_quit);
        detector = new GestureDetector (XScreenSaverDaydream.this, jwxyz_obj);
      } else {
        jwxyz_obj.resize (width, height);
      }

      jwxyz_obj.start();
    }

    @Override
    public void surfaceCreated (SurfaceHolder holder) {
      if (!initTried) {
        initTried = true;
      } else {
        if (jwxyz_obj != null) {
          jwxyz_obj.close();
          jwxyz_obj = null;
        }
      }
    }

    @Override
    public void surfaceDestroyed (SurfaceHolder holder) {
      if (jwxyz_obj != null) {
        jwxyz_obj.close();
        jwxyz_obj = null;
      }
    }

    @Override
    public boolean onTouchEvent (MotionEvent event) {
      detector.onTouchEvent (event);
      if (event.getAction() == MotionEvent.ACTION_UP)
        jwxyz_obj.dragEnded (event);
      return true;
    }

    @Override
    public boolean onKeyDown (int keyCode, KeyEvent event) {
      // In the emulator, this doesn't receive keyboard arrow keys, PgUp, etc.
      // Some other keys like "Home" are interpreted before we get here, and
      // function keys do weird shit.

      // TODO: Does this still work? And is the above still true?

      if (view.jwxyz_obj != null)
        view.jwxyz_obj.sendKeyEvent (event);
      return true;
    }
  }

  private SaverView view;
  Bitmap screenshot;

  private void LOG (String fmt, Object... args) {
    Log.d ("xscreensaver",
           this.getClass().getSimpleName() + ": " +
           String.format (fmt, args));
  }

  protected XScreenSaverDaydream () {
    super();
  }

  // Called when jwxyz_abort() is called, or other exceptions are thrown.
  //
/*
  @Override
  public void uncaughtException (Thread thread, Throwable ex) {

    renderer = null;
    String err = ex.toString();
    LOG ("Caught exception: %s", err);

    this.finish();  // Exit the Daydream

    final AlertDialog.Builder b = new AlertDialog.Builder(this);
    b.setMessage (err);
    b.setCancelable (false);
    b.setPositiveButton ("Bummer",
                         new DialogInterface.OnClickListener() {
                           public void onClick(DialogInterface d, int id) {
                           }
                         });

    // #### This isn't working:
    // "Attempted to add window with non-application token"
    // "Unable to add window -- token null is not for an application"
    // I think I need to get an "Activity" to run it on somehow?

    new Handler (Looper.getMainLooper()).post (new Runnable() {
        public void run() {
          AlertDialog alert = b.create();
          alert.setTitle (this.getClass().getSimpleName() + " crashed");
          alert.setIcon(android.R.drawable.ic_dialog_alert);
          alert.show();
        }
      });

    old_handler.uncaughtException (thread, ex);
  }
*/


  @Override
  public void onAttachedToWindow() {
    super.onAttachedToWindow();

    setInteractive (true);
    setFullscreen (true);
    saveScreenshot();

    view = new SaverView ();
    setContentView (view);
  }

  public void onDreamingStarted() {
    super.onDreamingStarted();
    // view.jwxyz_obj.start();
  }

  public void onDreamingStopped() {
    super.onDreamingStopped();
    view.jwxyz_obj.pause();
  }

  public void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    try {
      if (view.jwxyz_obj != null)
        view.jwxyz_obj.pause();
    } catch (Exception exc) {
      // Fun fact: Android swallows exceptions coming from here, then crashes
      // elsewhere.
      LOG ("onDetachedFromWindow: %s", exc.toString());
      throw exc;
    }
  }


  // At startup, before we have blanked the screen, save a screenshot
  // for later use by the hacks.
  //
  private void saveScreenshot() {
    View view = getWindow().getDecorView().getRootView();
    if (view == null) {
      LOG ("unable to get root view for screenshot");
    } else {

      // This doesn't work:
  /*
      boolean was = view.isDrawingCacheEnabled();
      if (!was) view.setDrawingCacheEnabled (true);
      view.buildDrawingCache();
      screenshot = view.getDrawingCache();
      if (!was) view.setDrawingCacheEnabled (false);
      if (screenshot == null) {
        LOG ("unable to get screenshot bitmap from %s", view.toString());
      } else {
        screenshot = Bitmap.createBitmap (screenshot);
      }
   */

      // This doesn't work either: width and height are both -1...

      int w = view.getLayoutParams().width;
      int h = view.getLayoutParams().height;
      if (w <= 0 || h <= 0) {
        LOG ("unable to get root view for screenshot");
      } else {
        screenshot = Bitmap.createBitmap (w, h, Bitmap.Config.ARGB_8888);
        Canvas c = new Canvas (screenshot);
        view.layout (0, 0, w, h);
        view.draw (c);
      }
    }
  }
}
