/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * xscreensaver, Copyright © 2017-2024 Jamie Zawinski <jwz@jwz.org>
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
 * Daydream preferences for Android TV.
 */

package org.jwz.xscreensaver;

import android.app.Activity;
import android.app.WallpaperManager;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.provider.Settings;

public class TVActivity extends Activity
  implements View.OnClickListener {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_tv_xscreensaver);
    findViewById(R.id.apply_daydream).setOnClickListener(this);
  }

  @Override
  public void onClick(View v) {
    if (v.getId() == R.id.apply_daydream) {
      Intent intent = new Intent(android.provider.Settings.ACTION_SETTINGS);
      startActivityForResult(intent, 0);
    }
  }
}
