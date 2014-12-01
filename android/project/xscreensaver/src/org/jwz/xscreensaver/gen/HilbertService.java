package org.jwz.xscreensaver.gen;
import net.rbgrn.android.glwallpaperservice.*;
import android.content.SharedPreferences;
import org.jwz.xscreensaver.*;

// Original code provided by Robert Green
// http://www.rbgrn.net/content/354-glsurfaceview-adapted-3d-live-wallpapers
public class HilbertService extends GLWallpaperService {
    SharedPreferences sp;
    public HilbertService() {
        super();
    }
    @Override
    public void onCreate() {
        sp = ((XscreensaverApp)getApplication()).getThePrefs(HilbertWallpaper.SHARED_PREFS_NAME);
    }
    public Engine onCreateEngine() {
        MyEngine engine = new MyEngine();
        return engine;
    }
    class MyEngine extends GLEngine {
        HilbertWallpaper renderer;
        public MyEngine() {
            super();
            // handle prefs, other initialization
            renderer = new HilbertWallpaper();
            setRenderer(renderer);
            setRenderMode(RENDERMODE_CONTINUOUSLY);
        }
        public void onDestroy() {
            super.onDestroy();
            if (renderer != null) {
                renderer.release(); // assuming yours has this method - it should!
            }
            renderer = null;
        }
        @Override
        public void onVisibilityChanged(boolean visible) {
            super.onVisibilityChanged(visible);
            if (visible) {
                renderer.doSP(sp);
            }
        }
    }
    static
    {
        System.loadLibrary ("xscreensaver");
    }
}
