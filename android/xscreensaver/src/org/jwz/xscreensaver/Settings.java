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
 *
 * The superclass of every saver's preferences panel.
 *
 * The only reason the subclasses of this class exist is so that we know
 * which "_settings.xml" to read -- we extract the base name from self's
 * class.
 *
 * project/xscreensaver/res/xml/SAVER_dream.xml refers to it as
 * android:settingsActivity="SAVER_Settings".  If there was some way
 * to pass an argument from the XML into here, or to otherwise detect
 * which Dream was instantiating this Settings, we wouldn't need those
 * hundreds of Settings subclasses.
 */

package org.jwz.xscreensaver;

import android.content.SharedPreferences;
import android.os.Bundle;

import android.content.SharedPreferences;
import android.preference.PreferenceActivity;
import android.preference.Preference;
import android.preference.ListPreference;
import android.preference.EditTextPreference;
import android.preference.CheckBoxPreference;
import org.jwz.xscreensaver.SliderPreference;

import org.jwz.xscreensaver.R;
import java.util.Map;
import java.lang.reflect.Field;

public abstract class Settings extends PreferenceActivity
  implements SharedPreferences.OnSharedPreferenceChangeListener {

  @Override
  protected void onCreate (Bundle icicle) {
    super.onCreate (icicle);

    // Extract the saver name from e.g. "BouncingCowSettings"
    String name = this.getClass().getSimpleName();
    String tail = "Settings";
    if (name.endsWith(tail))
      name = name.substring (0, name.length() - tail.length());
    name = name.toLowerCase();

    // #### All of these have been deprecated:
    //   getPreferenceManager()
    //   addPreferencesFromResource(int)
    //   findPreference(CharSequence)

    getPreferenceManager().setSharedPreferencesName (name);

    // read R.xml.SAVER_settings dynamically
    int res = -1;
    String pref_class = name + "_settings";
    try { res = R.xml.class.getDeclaredField(pref_class).getInt (null); }
    catch (Exception e) { }
    if (res != -1)
      addPreferencesFromResource (res);

    final int res_final = res;

    SharedPreferences prefs = getPreferenceManager().getSharedPreferences();
    prefs.registerOnSharedPreferenceChangeListener (this);
    updateAllPrefsSummaries (prefs);

    // Find the "Reset to defaults" button and install a click handler on it.
    //
    Preference reset = findPreference (name + "_reset");
    reset.setOnPreferenceClickListener(
      new Preference.OnPreferenceClickListener() {
        @Override
        public boolean onPreferenceClick(Preference preference) {

          SharedPreferences prefs =
            getPreferenceManager().getSharedPreferences();

          // Wipe everything from the preferences hash, then reload defaults.
          prefs.edit().clear().commit();
          getPreferenceScreen().removeAll();
          addPreferencesFromResource (res_final);

          // I guess we need to re-get this after the removeAll?
          prefs = getPreferenceManager().getSharedPreferences();

          // But now we need to iterate over every Preference widget and
          // push the new value down into it.  If you think this all looks
          // ridiculously non-object-oriented and completely insane, that's
          // because it is.

          Map <String, ?> keys = prefs.getAll();
          for (Map.Entry <String, ?> entry : keys.entrySet()) {
            String key = entry.getKey();
            String val = String.valueOf (entry.getValue());

            Preference pref = findPreference (key);
            if (pref instanceof ListPreference) {
              ((ListPreference) pref).setValue (prefs.getString (key, ""));
            } else if (pref instanceof SliderPreference) {
              ((SliderPreference) pref).setValue (prefs.getFloat (key, 0));
            } else if (pref instanceof EditTextPreference) {
              ((EditTextPreference) pref).setText (prefs.getString (key, ""));
            } else if (pref instanceof CheckBoxPreference) {
              ((CheckBoxPreference) pref).setChecked (
                prefs.getBoolean (key,false));
            }

            updatePrefsSummary (prefs, pref);
          }
          return true;
        }
      });
  }

  @Override
  protected void onResume() {
    super.onResume();
    SharedPreferences prefs = getPreferenceManager().getSharedPreferences();
    prefs.registerOnSharedPreferenceChangeListener (this);
    updateAllPrefsSummaries(prefs);
  }

  @Override
  protected void onPause() {
    getPreferenceManager().getSharedPreferences().
      unregisterOnSharedPreferenceChangeListener(this);
    super.onPause();
  }

  @Override
  protected void onDestroy() {
    getPreferenceManager().getSharedPreferences().
      unregisterOnSharedPreferenceChangeListener(this);
    super.onDestroy();
  }

  public void onSharedPreferenceChanged (SharedPreferences sharedPreferences,
                                         String key) {
    updatePrefsSummary(sharedPreferences, findPreference(key));
  }

  protected void updatePrefsSummary(SharedPreferences sharedPreferences,
                                    Preference pref) {
    if (pref == null)
      return;

    if (pref instanceof ListPreference) {
      pref.setTitle (((ListPreference) pref).getEntry());
    } else if (pref instanceof SliderPreference) {
      float v = ((SliderPreference) pref).getValue();
      int i = (int) Math.floor (v);
      if (v == i)
        pref.setSummary (String.valueOf (i));
      else
        pref.setSummary (String.valueOf (v));
    } else if (pref instanceof EditTextPreference) {
      pref.setSummary (((EditTextPreference) pref).getText());
    }
  }

  protected void updateAllPrefsSummaries(SharedPreferences prefs) {

    Map <String, ?> keys = prefs.getAll();
    for (Map.Entry <String, ?> entry : keys.entrySet()) {
      updatePrefsSummary (prefs, findPreference (entry.getKey()));
    }
  }
}
