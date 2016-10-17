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
 * This class is how the C implementation of jwxyz calls back into Java
 * to do things that OpenGL does not have access to without Java-based APIs.
 * It is the Java companion to jwxyz-android.c and screenhack-android.c.
 */

package org.jwz.xscreensaver;

import java.util.Map;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.ArrayList;
import java.util.Random;
import android.view.KeyEvent;
import android.content.SharedPreferences;
import android.content.Context;
import android.content.ContentResolver;
import android.content.res.AssetManager;
import android.graphics.Typeface;
import android.graphics.Rect;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.net.Uri;
import java.net.URL;
import java.nio.ByteBuffer;
import java.io.File;
import java.io.InputStream;
import java.io.FileOutputStream;
import java.lang.Runnable;
import java.lang.Thread;
import android.database.Cursor;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.media.ExifInterface;
import org.jwz.xscreensaver.TTFAnalyzer;
import android.util.Log;

public class jwxyz {

  private void LOG (String fmt, Object... args) {
    Log.d ("xscreensaver",
           this.getClass().getSimpleName() + ": " +
           String.format (fmt, args));
  }

  private long nativeRunningHackPtr;

  String hack;
  Context app;
  public final static int API_XLIB = 0;
  public final static int API_GL   = 1;
  Bitmap screenshot;

  SharedPreferences prefs;
  Hashtable<String, String> defaults = new Hashtable<String, String>();


  // Maps font names to either: String (system font) or Typeface (bundled).
  private Hashtable<String, Object> all_fonts =
    new Hashtable<String, Object>();


  // These are defined in jwxyz-android.c:
  //
  private native void nativeInit (String hack, int api,
                                  Hashtable<String,String> defaults,
                                  int w, int h);
  public native void nativeResize (int w, int h, double rot);
  public native long nativeRender ();
  public native void nativeDone ();
  public native void sendButtonEvent (int x, int y, boolean down);
  public native void sendMotionEvent (int x, int y);
  public native void sendKeyEvent (boolean down_p, int code, int mods);

  // Constructor
  public jwxyz (String hack, int api, Context app, Bitmap screenshot,
                int w, int h) {

    this.hack = hack;
    this.app  = app;
    this.screenshot = screenshot;

    // nativeInit populates 'defaults' with the default values for keys
    // that are not overridden by SharedPreferences.

    prefs = app.getSharedPreferences (hack, 0);
    scanSystemFonts();
    nativeInit (hack, api, defaults, w, h);
  }

/*  TODO: Can't do this yet; nativeDone requires the OpenGL context to be set.
  protected void finalize() {
    if (nativeRunningHackPtr != 0) {
      nativeDone();
    }
  } */


  public String getStringResource (String name) {

    name = hack + "_" + name;

    if (prefs.contains(name)) {

      // SharedPreferences is very picky that you request the exact type that
      // was stored: if it is a float and you ask for a string, you get an
      // exception instead of the float converted to a string.

      String s = null;
      try { return prefs.getString (name, "");
      } catch (Exception e) { }

      try { return Float.toString (prefs.getFloat (name, 0));
      } catch (Exception e) { }

      try { return Long.toString (prefs.getLong (name, 0));
      } catch (Exception e) { }

      try { return Integer.toString (prefs.getInt (name, 0));
      } catch (Exception e) { }

      try { return (prefs.getBoolean (name, false) ? "true" : "false");
      } catch (Exception e) { }
    }

    // If we got to here, it's not in there, so return the default.
    return defaults.get (name);
  }


  private String mungeFontName (String name) {
    // Roboto-ThinItalic => RobotoThin
    // AndroidCock Regular => AndroidClock
    String tails[] = { "Bold", "Italic", "Oblique", "Regular" };
    for (String tail : tails) {
      String pres[] = { " ", "-", "_", "" };
      for (String pre : pres) {
        int i = name.indexOf(pre + tail);
        if (i > 0) name = name.substring (0, i);
      }
    }
    return name;
  }


  private void scanSystemFonts() {

    // First parse the system font directories for the global fonts.

    String[] fontdirs = { "/system/fonts", "/system/font", "/data/fonts" };
    TTFAnalyzer analyzer = new TTFAnalyzer();
    for (String fontdir : fontdirs) {
      File dir = new File(fontdir);
      if (!dir.exists())
        continue;
      File[] files = dir.listFiles();
      if (files == null)
        continue;

      for (File file : files) {
        String name = analyzer.getTtfFontName (file.getAbsolutePath());
        if (name == null) {
          // LOG ("unparsable system font: %s", file);
        } else {
          name = mungeFontName (name);
          if (! all_fonts.contains (name)) {
            // LOG ("system font \"%s\" %s", name, file);
            all_fonts.put (name, name);
          }
        }
      }
    }

    // Now parse our assets, for our bundled fonts.

    AssetManager am = app.getAssets();
    String dir = "fonts";
    String[] files = null;
    try { files = am.list(dir); }
    catch (Exception e) { LOG("listing assets: %s", e.toString()); }

    for (String fn : files) {
      String fn2 = dir + "/" + fn;
      Typeface t = Typeface.createFromAsset (am, fn2);

      File tmpfile = null;
      try {
        tmpfile = new File(app.getCacheDir(), fn);
        if (tmpfile.createNewFile() == false) {
          tmpfile.delete();
          tmpfile.createNewFile();
        }

        InputStream in = am.open (fn2);
        FileOutputStream out = new FileOutputStream (tmpfile);
        byte[] buffer = new byte[1024 * 512];
        while (in.read(buffer, 0, 1024 * 512) != -1) {
          out.write(buffer);
        }
        out.close();
        in.close();

        String name = analyzer.getTtfFontName (tmpfile.getAbsolutePath());
        tmpfile.delete();

        name = mungeFontName (name);
        all_fonts.put (name, t);
        // LOG ("asset font \"%s\" %s", name, fn);
      } catch (Exception e) {
        if (tmpfile != null) tmpfile.delete();
        LOG ("error: %s", e.toString());
      }
    }
  }


  // Parses X Logical Font Descriptions, and a few standard X font names.
  // Returns [ String name, Float size, Typeface ]
  private Object[] parseXLFD (String name) {
    float   size   = 12;
    boolean bold   = false;
    boolean italic = false;
    boolean fixed  = false;
    boolean serif  = false;

    if      (name.equals("6x10"))     { size = 8;  fixed = true; }
    else if (name.equals("6x10bold")) { size = 8;  fixed = true; bold = true; }
    else if (name.equals("fixed"))    { size = 12; fixed = true; }
    else if (name.equals("9x15"))     { size = 12; fixed = true; }
    else if (name.equals("9x15bold")) { size = 12; fixed = true; bold = true; }
    else if (name.equals("vga"))      { size = 12; fixed = true; }
    else if (name.equals("console"))  { size = 12; fixed = true; }
    else if (name.equals("gallant"))  { size = 12; fixed = true; }
    else {
      String[] tokens = name.split("-");	// XLFD
      int L = tokens.length;
      int i = 1;
      String foundry  = (i < L ? tokens[i++] : "");
      String family   = (i < L ? tokens[i++] : "");
      String weight   = (i < L ? tokens[i++] : "");
      String slant    = (i < L ? tokens[i++] : "");
      String setwidth = (i < L ? tokens[i++] : "");
      String adstyle  = (i < L ? tokens[i++] : "");
      String pxsize   = (i < L ? tokens[i++] : "");
      String ptsize   = (i < L ? tokens[i++] : "");
      String resx     = (i < L ? tokens[i++] : "");
      String resy     = (i < L ? tokens[i++] : "");
      String spacing  = (i < L ? tokens[i++] : "");
      String avgw     = (i < L ? tokens[i++] : "");
      String charset  = (i < L ? tokens[i++] : "");
      String registry = (i < L ? tokens[i++] : "");

      if (spacing.equals("m") ||
          family.equals("fixed") ||
          family.equals("courier") ||
          family.equals("console") ||
          family.equals("lucidatypewriter")) {
        fixed = true;
      } else if (family.equals("times") ||
                 family.equals("georgia")) {
        serif = true;
      }

      if (weight.equals("bold") || weight.equals("demibold")) {
        bold = true; 
      }

      if (slant.equals("i") || slant.equals("o")) {
        italic = true;
      }

      // -*-courier-bold-r-*-*-14-*-*-*-*-*-*-*		14 px
      // -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*	14 pt
      // -*-courier-bold-r-*-*-140-*			14 pt, via wildcard
      // -*-courier-bold-r-*-140-*			14 pt, not handled
      // -*-courier-bold-r-*-*-14-180-*-*-*-*-*-*	error

      if (!ptsize.equals("") && !ptsize.equals("*")) {
        // It was in the ptsize field, so that's definitely what it is.
        size = Float.valueOf(ptsize) / 10.0f;
      } else if (!pxsize.equals("") && !pxsize.equals("*")) {
        size = Float.valueOf(pxsize);
        // If it's a fully qualified XLFD, then this really is the pxsize.
        // Otherwise, this is probably point size with a multi-field wildcard.
        if (registry.equals(""))   // not a fully qualified XLFD
          size /= 10.0f;
      }
    }

    if (name.equals("random")) {
      Random r = new Random();
      serif = r.nextBoolean();      // Not much to randomize here...
      fixed = (r.nextInt(8) == 0);
    }

    name = (fixed
            ? (serif ? "serif-monospace" : "monospace")
            : (serif ? "serif" : "sans-serif"));

    Typeface font = Typeface.create (name,
                                     (bold && italic ? Typeface.BOLD_ITALIC :
                                      bold   ? Typeface.BOLD :
                                      italic ? Typeface.ITALIC :
                                      Typeface.NORMAL));

    Object ret[] = { name, new Float(size), font };
    return ret;
  }


  // Parses "Native Font Name One 12, Native Font Name Two 14".
  // Returns [ String name, Float size, Typeface ]
  private Object[] parseNativeFont (String names) {
    for (String name : names.split(",")) {
      float size = 0;
      name = name.trim();
      if (name.equals("")) continue;
      int spc = name.lastIndexOf(" ");
      if (spc > 0) {
        size = Float.valueOf (name.substring (spc + 1));
        name = name.substring (0, spc);
      }
      if (size <= 0)
        size = 12;

      Object font = all_fonts.get (name);
      if (font instanceof String)
        font = Typeface.create (name, Typeface.NORMAL);

      if (font != null) {
        Object ret[] = { name, size, (Typeface) font };
        return ret;
      }
    }

    return null;
  }


  // Returns [ Paint paint, String name, Float size, ascent, descent ]
  public Object[] loadFont(String name) {
    Object pair[];

    if (name.equals("")) return null;

    if (name.contains(" ")) {
      pair = parseNativeFont (name);
    } else {
      pair = parseXLFD (name);
    }

    if (pair == null) return null;

    String name2  = (String)   pair[0];
    float size    = (Float)    pair[1];
    Typeface font = (Typeface) pair[2];

    size *= 2;

    name2 += (font.isBold() && font.isItalic() ? " bold italic" :
              font.isBold()   ? " bold"   :
              font.isItalic() ? " italic" :
              "");
    Paint paint = new Paint();
    paint.setTypeface (font);
    paint.setTextSize (size);
    paint.setColor (Color.argb (0xFF, 0xFF, 0xFF, 0xFF));

    LOG ("load font \"%s\" = \"%s %.1f\"", name, name2, size);

    FontMetrics fm = paint.getFontMetrics();
    Object ret[] = { paint, name2, new Float(size),
                     new Float(-fm.ascent), new Float(fm.descent) };
    return ret;
  }


  /* Returns a byte[] array containing XCharStruct with an optional
     bitmap appended to it.
     lbearing, rbearing, width, ascent, descent: 2 bytes each.
     Followed by a WxH pixmap, 32 bits per pixel.
   */
  public ByteBuffer renderText (Paint paint, String text, boolean render_p) {

    if (paint == null) {
      LOG ("no font");
      return null;
    }

    /* Font metric terminology, as used by X11:

       "lbearing" is the distance from the logical origin to the leftmost
       pixel.  If a character's ink extends to the left of the origin, it is
       negative.

       "rbearing" is the distance from the logical origin to the rightmost
       pixel.

       "descent" is the distance from the logical origin to the bottommost
       pixel.  For characters with descenders, it is positive.  For
       superscripts, it is negative.

       "ascent" is the distance from the logical origin to the topmost pixel.
       It is the number of pixels above the baseline.

       "width" is the distance from the logical origin to the position where
       the logical origin of the next character should be placed.

       If "rbearing" is greater than "width", then this character overlaps the
       following character.  If smaller, then there is trailing blank space.

       The bbox coordinates returned by getTextBounds grow down and right:
       for a character with ink both above and below the baseline, top is
       negative and bottom is positive.
     */
    FontMetrics fm = paint.getFontMetrics();
    Rect bbox = new Rect();
    paint.getTextBounds (text, 0, text.length(), bbox);

    /* The bbox returned by getTextBounds measures from the logical origin
       with right and down being positive.  This means most characters have
       a negative top, and characters with descenders have a positive bottom.
     */
    int lbearing  =  bbox.left;
    int rbearing  =  bbox.right;
    int ascent    = -bbox.top;
    int descent   =  bbox.bottom;
    int width     = (int) paint.measureText (text);

    int w = rbearing - lbearing;
    int h = ascent + descent;
    int size = 5 * 2 + (render_p ? w * h * 4 : 0);

    ByteBuffer bits = ByteBuffer.allocateDirect (size);

    bits.put ((byte) ((lbearing >> 8) & 0xFF));
    bits.put ((byte) ( lbearing       & 0xFF));
    bits.put ((byte) ((rbearing >> 8) & 0xFF));
    bits.put ((byte) ( rbearing       & 0xFF));
    bits.put ((byte) ((width    >> 8) & 0xFF));
    bits.put ((byte) ( width          & 0xFF));
    bits.put ((byte) ((ascent   >> 8) & 0xFF));
    bits.put ((byte) ( ascent         & 0xFF));
    bits.put ((byte) ((descent  >> 8) & 0xFF));
    bits.put ((byte) ( descent        & 0xFF));

    if (render_p && w > 0 && h > 0) {
      Bitmap bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
      Canvas canvas = new Canvas (bitmap);
      canvas.drawText (text, -lbearing, ascent, paint);
      bitmap.copyPixelsToBuffer (bits);
      bitmap.recycle();
    }

    return bits;
  }


  /* Returns the contents of the URL.
     Loads the URL in a background thread: if the URL has not yet loaded,
     this will return null.  Once the URL has completely loaded, the full
     contents will be returned.  Calling this again after that starts the
     URL loading again.
   */
  private String loading_url = null;
  private ByteBuffer loaded_url_body = null;

  public synchronized ByteBuffer loadURL (String url) {

    if (loaded_url_body != null) {			// Thread finished

      // LOG ("textclient finished %s", loading_url);

      ByteBuffer bb = loaded_url_body;
      loading_url = null;
      loaded_url_body = null;
      return bb;

    } else if (loading_url != null) {			// Waiting on thread
      // LOG ("textclient waiting...");
      return null;

    } else {						// Launch thread

      loading_url = url;
      LOG ("textclient launching %s...", url);

      new Thread (new Runnable() {
          public void run() {
            int size0 = 10240;
            int size = size0;
            int count = 0;
            ByteBuffer body = ByteBuffer.allocateDirect (size);

            try {
              URL u = new URL (loading_url);
              // LOG ("textclient thread loading: %s", u.toString());
              InputStream s = u.openStream();
              byte buf[] = new byte[10240];
              while (true) {
                int n = s.read (buf);
                if (n == -1) break;
                // LOG ("textclient thread read %d", n);
                if (count + n + 1 >= size) {
                  int size2 = (int) (size * 1.2 + size0);
                  // LOG ("textclient thread expand %d -> %d", size, size2);
                  ByteBuffer body2 = ByteBuffer.allocateDirect (size2);
                  body.rewind();
                  body2.put (body);
                  body2.position (count);
                  body = body2;
                  size = size2;
                }
                body.put (buf, 0, n);
                count += n;
              }
            } catch (Exception e) {
              LOG ("load URL error: %s", e.toString());
              body.clear();
              body.put (e.toString().getBytes());
              body.put ((byte) 0);
            }

            // LOG ("textclient thread finished %s (%d)", loading_url, size);
            loaded_url_body = body;
          }
        }).start();

      return null;
    }
  }


  private ByteBuffer convertBitmap (String name, Bitmap bitmap,
                                    int target_width, int target_height,
                                    ExifInterface exif,
                                    boolean rotate_p) {
    if (bitmap == null) return null;

    try {

      int width  = bitmap.getWidth();
      int height = bitmap.getHeight();

      LOG ("read image %s: %d x %d", name, width, height);

      // First rotate the image as per EXIF.

      if (exif != null) {
        int deg = 0;
        switch (exif.getAttributeInt (ExifInterface.TAG_ORIENTATION,
                                      ExifInterface.ORIENTATION_NORMAL)) {
        case ExifInterface.ORIENTATION_ROTATE_90:  deg = 90;  break;
        case ExifInterface.ORIENTATION_ROTATE_180: deg = 180; break;
        case ExifInterface.ORIENTATION_ROTATE_270: deg = 270; break;
        }
        if (deg != 0) {
          LOG ("%s: EXIF rotate %d", name, deg);
          Matrix matrix = new Matrix();
          matrix.preRotate (deg);
          bitmap = Bitmap.createBitmap (bitmap, 0, 0, width, height,
                                        matrix, true);
          width  = bitmap.getWidth();
          height = bitmap.getHeight();
        }
      }

      // If the caller requested that we rotate the image to best fit the
      // screen, rotate it again.  (Could combine this with the above and
      // avoid copying the image again, but I'm lazy.)

      if (rotate_p &&
          (width > height) != (target_width > target_height)) {
        LOG ("%s: rotated to fit screen", name);
        Matrix matrix = new Matrix();
        matrix.preRotate (90);
        bitmap = Bitmap.createBitmap (bitmap, 0, 0, width, height,
                                      matrix, true);
        width  = bitmap.getWidth();
        height = bitmap.getHeight();
      }

      // Resize the image to be not larger than the screen, potentially
      // copying it for the third time.
      // Actually, always scale it, scaling up if necessary.

//    if (width > target_width || height > target_height)
      {
        float r1 = target_width  / (float) width;
        float r2 = target_height / (float) height;
        float r = (r1 > r2 ? r2 : r1);
        LOG ("%s: resize %.1f: %d x %d => %d x %d", name,
             r, width, height, (int) (width * r), (int) (height * r));
        width  = (int) (width * r);
        height = (int) (height * r);
        bitmap = Bitmap.createScaledBitmap (bitmap, width, height, true);
        width  = bitmap.getWidth();
        height = bitmap.getHeight();
      }

      // Now convert it to a ByteBuffer in the form expected by the C caller.

      byte[] nameb = name.getBytes("UTF-8");
      int size     = bitmap.getByteCount() + 4 + nameb.length + 1;

      ByteBuffer bits = ByteBuffer.allocateDirect (size);

      bits.put ((byte) ((width  >> 8) & 0xFF));
      bits.put ((byte) ( width        & 0xFF));
      bits.put ((byte) ((height >> 8) & 0xFF));
      bits.put ((byte) ( height       & 0xFF));
      bits.put (nameb);
      bits.put ((byte) 0);

      // The fourth of five copies.  Good thing these are supercomputers.
      bitmap.copyPixelsToBuffer (bits);

      return bits;

    } catch (Exception e) {
      LOG ("image %s unreadable: %s", name, e.toString());
    }

    return null;
  }


  public ByteBuffer loadRandomImage (int target_width, int target_height,
                                     boolean rotate_p) {

    int min_size = 480;
    int max_size = 0x7FFF;

    ArrayList<String> imgs = new ArrayList<String>();

    ContentResolver cr = app.getContentResolver();
    String[] cols = { MediaColumns.DATA,
                      MediaColumns.MIME_TYPE,
                      MediaColumns.WIDTH,
                      MediaColumns.HEIGHT };
    Uri uris[] = {
      android.provider.MediaStore.Images.Media.INTERNAL_CONTENT_URI,
      android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI };

    for (int i = 0; i < uris.length; i++) {
      Cursor cursor = cr.query (uris[i], cols, null, null, null);
      int j = 0;
      int path_col   = cursor.getColumnIndexOrThrow (cols[j++]);
      int type_col   = cursor.getColumnIndexOrThrow (cols[j++]);
      int width_col  = cursor.getColumnIndexOrThrow (cols[j++]);
      int height_col = cursor.getColumnIndexOrThrow (cols[j++]);
      while (cursor.moveToNext()) {
        String path = cursor.getString(path_col);
        String type = cursor.getString(type_col);
        int w = Integer.parseInt (cursor.getString(width_col));
        int h = Integer.parseInt (cursor.getString(height_col));
        if (type.startsWith("image/") &&
            w > min_size && h > min_size &&
            w < max_size && h < max_size) {
          imgs.add (path);
        }
      }
      cursor.close();
    }

    String which = null;

    int count = imgs.size();
    if (count == 0) {
      LOG ("no images");
      return null;
    }

    int i = new Random().nextInt (count);
    which = imgs.get (i);
    LOG ("picked image %d of %d: %s", i, count, which);

    Uri uri = Uri.fromFile (new File (which));
    String name = uri.getLastPathSegment();
    Bitmap bitmap = null;
    ExifInterface exif = null;

    try {
      bitmap = MediaStore.Images.Media.getBitmap (cr, uri);
    } catch (Exception e) {
      LOG ("image %s unloadable: %s", which, e.toString());
      return null;
    }

    try {
      exif = new ExifInterface (uri.getPath());  // If it fails, who cares
    } catch (Exception e) {
    }

    ByteBuffer bits = convertBitmap (name, bitmap,
                                     target_width, target_height,
                                     exif, rotate_p);
    bitmap.recycle();
    return bits;
  }


  public ByteBuffer getScreenshot (int target_width, int target_height,
                                   boolean rotate_p) {
    return convertBitmap ("Screenshot", screenshot,
                          target_width, target_height,
                          null, rotate_p);
  }


  // Sadly duplicated from jwxyz.h (and thence X.h and keysymdef.h)
  //
  private static final int ShiftMask =	   (1<<0);
  private static final int LockMask =	   (1<<1);
  private static final int ControlMask =   (1<<2);
  private static final int Mod1Mask =	   (1<<3);
  private static final int Mod2Mask =	   (1<<4);
  private static final int Mod3Mask =	   (1<<5);
  private static final int Mod4Mask =	   (1<<6);
  private static final int Mod5Mask =	   (1<<7);
  private static final int Button1Mask =   (1<<8);
  private static final int Button2Mask =   (1<<9);
  private static final int Button3Mask =   (1<<10);
  private static final int Button4Mask =   (1<<11);
  private static final int Button5Mask =   (1<<12);

  private static final int XK_Shift_L =	   0xFFE1;
  private static final int XK_Shift_R =	   0xFFE2;
  private static final int XK_Control_L =  0xFFE3;
  private static final int XK_Control_R =  0xFFE4;
  private static final int XK_Caps_Lock =  0xFFE5;
  private static final int XK_Shift_Lock = 0xFFE6;
  private static final int XK_Meta_L =	   0xFFE7;
  private static final int XK_Meta_R =	   0xFFE8;
  private static final int XK_Alt_L =	   0xFFE9;
  private static final int XK_Alt_R =	   0xFFEA;
  private static final int XK_Super_L =	   0xFFEB;
  private static final int XK_Super_R =	   0xFFEC;
  private static final int XK_Hyper_L =	   0xFFED;
  private static final int XK_Hyper_R =	   0xFFEE;

  private static final int XK_Home =	   0xFF50;
  private static final int XK_Left =	   0xFF51;
  private static final int XK_Up =	   0xFF52;
  private static final int XK_Right =	   0xFF53;
  private static final int XK_Down =	   0xFF54;
  private static final int XK_Prior =	   0xFF55;
  private static final int XK_Page_Up =	   0xFF55;
  private static final int XK_Next =	   0xFF56;
  private static final int XK_Page_Down =  0xFF56;
  private static final int XK_End =	   0xFF57;
  private static final int XK_Begin =	   0xFF58;

  private static final int XK_F1 =	   0xFFBE;
  private static final int XK_F2 =	   0xFFBF;
  private static final int XK_F3 =	   0xFFC0;
  private static final int XK_F4 =	   0xFFC1;
  private static final int XK_F5 =	   0xFFC2;
  private static final int XK_F6 =	   0xFFC3;
  private static final int XK_F7 =	   0xFFC4;
  private static final int XK_F8 =	   0xFFC5;
  private static final int XK_F9 =	   0xFFC6;
  private static final int XK_F10 =	   0xFFC7;
  private static final int XK_F11 =	   0xFFC8;
  private static final int XK_F12 =	   0xFFC9;

  public void sendKeyEvent (KeyEvent event) {
    int uc    = event.getUnicodeChar();
    int jcode = event.getKeyCode();
    int jmods = event.getModifiers();
    int xcode = 0;
    int xmods = 0;

    switch (jcode) {
    case KeyEvent.KEYCODE_SHIFT_LEFT:	     xcode = XK_Shift_L;   break;
    case KeyEvent.KEYCODE_SHIFT_RIGHT:	     xcode = XK_Shift_R;   break;
    case KeyEvent.KEYCODE_CTRL_LEFT:	     xcode = XK_Control_L; break;
    case KeyEvent.KEYCODE_CTRL_RIGHT:	     xcode = XK_Control_R; break;
    case KeyEvent.KEYCODE_CAPS_LOCK:	     xcode = XK_Caps_Lock; break;
    case KeyEvent.KEYCODE_META_LEFT:	     xcode = XK_Meta_L;	   break;
    case KeyEvent.KEYCODE_META_RIGHT:	     xcode = XK_Meta_R;	   break;
    case KeyEvent.KEYCODE_ALT_LEFT:	     xcode = XK_Alt_L;	   break;
    case KeyEvent.KEYCODE_ALT_RIGHT:	     xcode = XK_Alt_R;	   break;

    case KeyEvent.KEYCODE_HOME:		     xcode = XK_Home;	   break;
    case KeyEvent.KEYCODE_DPAD_LEFT:	     xcode = XK_Left;	   break;
    case KeyEvent.KEYCODE_DPAD_UP:	     xcode = XK_Up;	   break;
    case KeyEvent.KEYCODE_DPAD_RIGHT:	     xcode = XK_Right;	   break;
    case KeyEvent.KEYCODE_DPAD_DOWN:	     xcode = XK_Down;	   break;
  //case KeyEvent.KEYCODE_NAVIGATE_PREVIOUS: xcode = XK_Prior;	   break;
    case KeyEvent.KEYCODE_PAGE_UP:	     xcode = XK_Page_Up;   break;
  //case KeyEvent.KEYCODE_NAVIGATE_NEXT:     xcode = XK_Next;	   break;
    case KeyEvent.KEYCODE_PAGE_DOWN:	     xcode = XK_Page_Down; break;
    case KeyEvent.KEYCODE_MOVE_END:	     xcode = XK_End;	   break;
    case KeyEvent.KEYCODE_MOVE_HOME:	     xcode = XK_Begin;	   break;

    case KeyEvent.KEYCODE_F1:		     xcode = XK_F1;	   break;
    case KeyEvent.KEYCODE_F2:		     xcode = XK_F2;	   break;
    case KeyEvent.KEYCODE_F3:		     xcode = XK_F3;	   break;
    case KeyEvent.KEYCODE_F4:		     xcode = XK_F4;	   break;
    case KeyEvent.KEYCODE_F5:		     xcode = XK_F5;	   break;
    case KeyEvent.KEYCODE_F6:		     xcode = XK_F6;	   break;
    case KeyEvent.KEYCODE_F7:		     xcode = XK_F7;	   break;
    case KeyEvent.KEYCODE_F8:		     xcode = XK_F8;	   break;
    case KeyEvent.KEYCODE_F9:		     xcode = XK_F9;	   break;
    case KeyEvent.KEYCODE_F10:		     xcode = XK_F10;	   break;
    case KeyEvent.KEYCODE_F11:		     xcode = XK_F11;	   break;
    case KeyEvent.KEYCODE_F12:		     xcode = XK_F12;	   break;
    default:				     xcode = uc;	   break;
    }

    if (0 != (jmods & KeyEvent.META_SHIFT_ON))     xmods |= ShiftMask;
    if (0 != (jmods & KeyEvent.META_CAPS_LOCK_ON)) xmods |= LockMask;
    if (0 != (jmods & KeyEvent.META_CTRL_MASK))    xmods |= ControlMask;
    if (0 != (jmods & KeyEvent.META_ALT_MASK))	   xmods |= Mod1Mask;
    if (0 != (jmods & KeyEvent.META_META_ON))      xmods |= Mod1Mask;
    if (0 != (jmods & KeyEvent.META_SYM_ON))       xmods |= Mod2Mask;
    if (0 != (jmods & KeyEvent.META_FUNCTION_ON))  xmods |= Mod3Mask;

    /* If you touch and release Shift, you get no events.
       If you type Shift-A, you get Shift down, A down, A up, Shift up.
       So let's just ignore all lone modifier key events.
     */
    if (xcode >= XK_Shift_L && xcode <= XK_Hyper_R)
      return;

    boolean down_p = event.getAction() == KeyEvent.ACTION_DOWN;
    sendKeyEvent (down_p, xcode, xmods);
  }

}
