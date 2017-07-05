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
 * The superclass of every saver's Daydream.
 *
 * Each Daydream needs a distinct subclass in order to show up in the list.
 * We know which saver we are running by the subclass name; we know which
 * API to use by how the subclass calls super().
 */

package org.jwz.xscreensaver;

import java.lang.Exception;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.KeyEvent;
import android.service.dreams.DreamService;
import android.opengl.GLSurfaceView;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Message;
import android.os.Handler;
import android.os.Looper;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.util.Log;

public class XScreenSaverDaydream extends DreamService
  implements GestureDetector.OnGestureListener,
             GestureDetector.OnDoubleTapListener,
             Handler.Callback {

  private GLSurfaceView glview;
  XScreenSaverRenderer renderer;
  private GestureDetector detector;
  boolean button_down_p;
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
  public boolean handleMessage (Message msg) {

    String err = msg.obj.toString();
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

    return true;
  }


  @Override
  public void onAttachedToWindow() {
    super.onAttachedToWindow();

    setInteractive (true);
    setFullscreen (true);
    saveScreenshot();

    glview = new GLSurfaceView (this);
    renderer =
      new XScreenSaverRenderer (XScreenSaverRenderer.saverNameOf (this),
                                getApplicationContext(), screenshot,
                                this, glview);
    setContentView (glview);

    detector = new GestureDetector (this, this);
  }

  public void onDreamingStarted() {
    super.onDreamingStarted();
  }

  public void onDreamingStopped() {
    super.onDreamingStopped();
    glview.onPause();
  }

  public void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    glview.onPause();
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



  /* We distinguish between taps and drags.

     - Drags/pans (down, motion, up) are sent to the saver to handle.
     - Single-taps exit the saver.
     - Long-press single-taps are sent to the saver as ButtonPress/Release;
     - Double-taps are sent to the saver as a "Space" keypress.

     #### TODO:
     - Swipes (really, two-finger drags/pans) send Up/Down/Left/RightArrow.
   */

  @Override
  public boolean onSingleTapConfirmed (MotionEvent event) {
    this.finish();  // Exit the Daydream
    return true;
  }

  @Override
  public boolean onDoubleTap (MotionEvent event) {
    renderer.sendKeyEvent (new KeyEvent (KeyEvent.ACTION_DOWN,
                                         KeyEvent.KEYCODE_SPACE));
    return true;
  }

  @Override
  public void onLongPress (MotionEvent event) {
    if (! button_down_p) {
      int x = (int) event.getX (event.getPointerId (0));
      int y = (int) event.getY (event.getPointerId (0));
      renderer.sendButtonEvent (x, y, true);
      renderer.sendButtonEvent (x, y, false);
    }
  }

  @Override
  public void onShowPress (MotionEvent event) {
    if (! button_down_p) {
      button_down_p = true;
      int x = (int) event.getX (event.getPointerId (0));
      int y = (int) event.getY (event.getPointerId (0));
      renderer.sendButtonEvent (x, y, true);
    }
  }

  @Override
  public boolean onScroll (MotionEvent e1, MotionEvent e2, 
                           float distanceX, float distanceY) {
    if (button_down_p)
      renderer.sendMotionEvent ((int) e2.getX (e2.getPointerId (0)),
                                (int) e2.getY (e2.getPointerId (0)));
    return true;
  }

  // If you drag too fast, you get a single onFling event instead of a
  // succession of onScroll events.  I can't figure out how to disable it.
  @Override
  public boolean onFling (MotionEvent e1, MotionEvent e2, 
                          float velocityX, float velocityY) {
    return false;
  }

  public boolean dragEnded (MotionEvent event) {
    if (button_down_p) {
      int x = (int) event.getX (event.getPointerId (0));
      int y = (int) event.getY (event.getPointerId (0));
      renderer.sendButtonEvent (x, y, false);
      button_down_p = false;
    }
    return true;
  }

  @Override
  public boolean onDown (MotionEvent event) {
    return false;
  }

  @Override
  public boolean onSingleTapUp (MotionEvent event) {
    return false;
  }

  @Override
  public boolean onDoubleTapEvent (MotionEvent event) {
    return false;
  }

  @Override 
  public boolean dispatchTouchEvent (MotionEvent event) {
    detector.onTouchEvent (event);
    if (event.getAction() == MotionEvent.ACTION_UP)
      dragEnded (event);
    return super.dispatchTouchEvent (event);
  }

  @Override
  public boolean dispatchKeyEvent (KeyEvent event) {

    // In the emulator, this doesn't receive keyboard arrow keys, PgUp, etc.
    // Some other keys like "Home" are interpreted before we get here, and
    // function keys do weird shit.

    super.dispatchKeyEvent (event);
    renderer.sendKeyEvent (event);
    return true;
  }

  // Dunno what dispatchKeyShortcutEvent does, but apparently nothing useful.

}
