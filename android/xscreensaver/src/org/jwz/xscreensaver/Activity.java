/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *
 * This is the XScreenSaver "application" that just brings up the
 * Live Wallpaper preferences.
 */

package org.jwz.xscreensaver;

import android.app.WallpaperManager;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.provider.Settings;
import android.Manifest;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.os.Build;
import android.content.pm.PackageManager;

public class Activity extends android.app.Activity
  implements View.OnClickListener {

  private boolean wallpaperButtonClicked, daydreamButtonClicked;
  private final static int MY_REQ_READ_EXTERNAL_STORAGE = 271828;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    // openList();
    setContentView(R.layout.activity_xscreensaver);
    wallpaperButtonClicked = false;
    daydreamButtonClicked = false;

    findViewById(R.id.apply_wallpaper).setOnClickListener(this);
    findViewById(R.id.apply_daydream).setOnClickListener(this);
  }

  @Override
  public void onClick(View v) {
    switch (v.getId()) {
    case R.id.apply_wallpaper:
      wallpaperButtonClicked();
      break;
    case R.id.apply_daydream:
      daydreamButtonClicked();
      break;
    }
  }

  // synchronized when dealing with wallpaper state - perhaps can
  // narrow down more
  private synchronized void withProceed() {
    if (daydreamButtonClicked) {
      String action;
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
        action = Settings.ACTION_DREAM_SETTINGS;
      } else {
        action = Settings.ACTION_DISPLAY_SETTINGS;
      }
      startActivity(new Intent(action));
    } else if (wallpaperButtonClicked) {
      startActivity(new Intent(WallpaperManager.ACTION_LIVE_WALLPAPER_CHOOSER));
    }
  }

  private void wallpaperButtonClicked() {
      wallpaperButtonClicked = true;
      checkPermission();
  }

  private void daydreamButtonClicked() {
      daydreamButtonClicked = true;
      checkPermission();
  }

  void checkPermission() {
      // RES introduced in API 16
      String permission = Manifest.permission.READ_EXTERNAL_STORAGE;
      if (havePermission(permission)) {
          withProceed();
      } else {
          noPermission(permission);
      }
  }

  private void noPermission(String permission) {
      int myRequestCode;
      myRequestCode = MY_REQ_READ_EXTERNAL_STORAGE;

      if (permissionsDeniedRationale(permission)) {
          showDeniedRationale();
      } else {
          requestPermission(permission, myRequestCode);
      }
  }

  private boolean permissionsDeniedRationale(String permission) {
      boolean rationale = ActivityCompat.shouldShowRequestPermissionRationale(this,
                permission);
      return rationale;
  }

  private void requestPermission(String permission, int myRequestCode) {
      ActivityCompat.requestPermissions(this,
              new String[]{permission},
              myRequestCode);

      // myRequestCode is an app-defined int constant.
      // The callback method gets the result of the request.
  }

  // TODO: This method should be asynchronous, and not block the thread
  private void showDeniedRationale() {
    withProceed();
  }

  boolean havePermission(String permission) {

      if (Build.VERSION.SDK_INT < 16) {
          return true;
      }

      if (permissionGranted(permission)) {
          return true;
      }

      return false;
  }

  private boolean permissionGranted(String permission) {
        boolean check = ContextCompat.checkSelfPermission(this, permission) ==
                PackageManager.PERMISSION_GRANTED;
      return check;
  }

  public void proceedIfPermissionGranted(int[] grantResults) {

      // If request is cancelled, the result arrays are empty.
      if (grantResults.length > 0
              && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
          withProceed();
      } else if (grantResults.length > 0) {
          withProceed();
      }
  }

  @Override
  public void onRequestPermissionsResult(int requestCode,
        String permissions[], int[] grantResults) {
    switch (requestCode) {
      case MY_REQ_READ_EXTERNAL_STORAGE:
          proceedIfPermissionGranted(grantResults);
    }
  }

}
