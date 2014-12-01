Truly Excellent Live Wallpaper - Version 0.1


Upstream codebases
------------------

This is a port of Xscreensaver 5.29 to Android.

On the Android side, it uses Ben Gruver's version of Mark Guerra's GL 
WallpaperService for Android.

Xscreensaver's GLX hacks are OpenGL, and Android is OpenGL ES, 
so we use the OpenGL to OpenGL ES translation shim that is in Xscreensaver.


Compiling
---------

We are compiling our APK with ant.  If you have a problem getting this 
to work with Eclipse, Android Studio, or some other IDE, let us know.  
TrulyCreative is the Service, and GLWallpaperService is the library 
which TrulyCreative uses.  Also don't forget this uses C/C++ code via 
the NDK, so you have to build both the C/C++ and Java/Dalvik code.



Licenses
--------

Some code in gl1.c is based off code from
Jetro Lauha's San Angeles Observation project
which is under a BSD-style license.

GLWallpaperService is under an Apache License, Version 2.0.

Look in the TrulyExcellent/jni/xscreensaver to see what licenses are used.
MIT/X11 seems to be the primary license.

The rest of the code is released under Apache License, Version 2.0.  
Some of that code is from the Android Open Source Project, some is from 
Dennis Sheil.  Dennis Sheil dual-licenses his code as MIT/X11 license also.
