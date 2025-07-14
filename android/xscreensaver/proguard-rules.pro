# ProGuard rules for XScreenSaver Android app
# This file helps reduce APK size by removing unused code

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep the main activity
-keep class org.jwz.xscreensaver.** { *; }

# Remove unused classes and methods
-dontwarn org.jwz.xscreensaver.**
-optimizations !code/simplification/arithmetic,!code/simplification/cast,!field/*,!class/merging/*
-optimizationpasses 5
-allowaccessmodification

# Remove debug information
-renamesourcefileattribute SourceFile
-keepattributes SourceFile,LineNumberTable,*Annotation*
