package org.jwz.xscreensaver;

import android.app.Application;
import android.content.SharedPreferences;

public class XscreensaverApp extends Application {

    public XscreensaverApp() {
        super();
    }


    public SharedPreferences getThePrefs(String p) {
        SharedPreferences prefs = getApplicationContext().getSharedPreferences(p, 0);
        return prefs;
    }

}
