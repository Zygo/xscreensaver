package org.jwz.xscreensaver.gen;
import org.jwz.xscreensaver.R;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceActivity;
public class SproingiesSettings extends PreferenceActivity
    implements SharedPreferences.OnSharedPreferenceChangeListener {
    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        getPreferenceManager().setSharedPreferencesName(
            SproingiesWallpaper.SHARED_PREFS_NAME);
        addPreferencesFromResource(R.xml.sproingies_settings);
        getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(
            this);
    }
    @Override
    protected void onResume() {
        super.onResume();
    }
    @Override
    protected void onDestroy() {
        getPreferenceManager().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(
            this);
        super.onDestroy();
    }
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences,
                                          String key) {
    }
}
