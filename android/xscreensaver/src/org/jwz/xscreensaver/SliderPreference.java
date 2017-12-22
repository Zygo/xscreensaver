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
 * A numeric preference as a slider, inline in the preferences list.
 * XML options include:
 *
 *  low, high (floats) -- smallest and largest allowed values.
 *  If low > high, the value increases as the slider's thumb moves left.
 *
 *  lowLabel, highLabel (strings) -- labels shown at the left and right
 *  ends of the slider.
 *
 *  integral (boolean) -- whether to use whole numbers instead of floats;
 */

package org.jwz.xscreensaver;

import android.content.Context;
import android.content.res.TypedArray;
import android.content.res.Resources;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.SeekBar;
import android.widget.TextView;
import android.util.Log;

public class SliderPreference extends Preference {

  protected float low, high;
  protected String low_label, high_label;
  protected boolean integral;
  protected float mValue;
  protected int seekbar_ticks;

  public SliderPreference(Context context, AttributeSet attrs) {
    this (context, attrs, 0);
  }

  public SliderPreference (Context context, AttributeSet attrs, int defStyle) {
    super (context, attrs, defStyle);

    Resources res = context.getResources();

    // Parse these from the "<SliderPreference>" tag
    low        = Float.parseFloat (attrs.getAttributeValue (null, "low"));
    high       = Float.parseFloat (attrs.getAttributeValue (null, "high"));
    integral   = attrs.getAttributeBooleanValue (null, "integral", false);
    low_label  = res.getString(
                   attrs.getAttributeResourceValue (null, "lowLabel", 0));
    high_label = res.getString(
                   attrs.getAttributeResourceValue (null, "highLabel", 0));

    seekbar_ticks = (integral
                     ? (int) Math.floor (Math.abs (high - low))
                     : 100000);

    setWidgetLayoutResource (R.layout.slider_preference);
  }


  @Override
  protected void onSetInitialValue (boolean restore, Object def) {
    if (restore) {
      mValue = getPersistedFloat (low);
    } else {
      mValue = (Float) def;
      persistFloat (mValue);
    }
    //Log.d("xscreensaver", String.format("SLIDER INIT %s: %f",
    //      low_label, mValue));
  }

  @Override
  protected Object onGetDefaultValue(TypedArray a, int index) {
    return a.getFloat (index, low);
  }


  public float getValue() {
    return mValue;
  }

  public void setValue (float value) {

    if (low < high) {
      value = Math.max (low, Math.min (high, value));
    } else {
      value = Math.max (high, Math.min (low, value));
    }

    if (integral)
      value = Math.round (value);

    if (value != mValue) {
      //Log.d("xscreensaver", String.format("SLIDER %s: %f", low_label, value));
      persistFloat (value);
      mValue = value;
      notifyChanged();
    }
  }


  @Override
  protected View onCreateView (ViewGroup parent) {
    View view = super.onCreateView(parent);

    TextView low_view = (TextView)
      view.findViewById (R.id.slider_preference_low);
    low_view.setText (low_label);

    TextView high_view = (TextView)
      view.findViewById (R.id.slider_preference_high);
    high_view.setText (high_label);

    SeekBar seekbar = (SeekBar)
      view.findViewById (R.id.slider_preference_seekbar);
    seekbar.setMax (seekbar_ticks);

    float ratio = (mValue - low) / (high - low);
    int seek_value = (int) (ratio * (float) seekbar_ticks);

    seekbar.setProgress (seek_value);

    final SliderPreference slider = this;

    seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onProgressChanged (SeekBar seekBar, int progress,
                                       boolean fromUser) {
          if (fromUser) {
            float ratio = (float) progress / (float) seekbar_ticks;
            float value = low + (ratio * (high - low));
            slider.setValue (value);
            callChangeListener (progress);
          }
        }
      });

    return view;
  }
}
