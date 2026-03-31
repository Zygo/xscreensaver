/* xscreensaver, Copyright (c) 2006-2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This stub is compiled differently for each saver, just to ensure that
   each one has a different class name.  If multiple savers use the
   XScreenSaver class directly, System Preferences gets really confused.
 */

#define INLINE_ALL_CLASSES

#ifndef CLASS
 ERROR! -DCLASS missing
#endif


#if !defined(INLINE_ALL_CLASSES) || defined(HAVE_IPHONE)

# ifdef USE_GL
#  import "XScreenSaverGLView.h"
#  define SUPERCLASS XScreenSaverGLView
# else
#  import "XScreenSaverView.h"
#  define SUPERCLASS XScreenSaverView
# endif
 
 @interface CLASS : SUPERCLASS { }
 @end

 @implementation CLASS
 @end

#else  // INLINE_ALL_CLASSES

   // Let's make these be private classes in each saver, to avoid
   //    "Class XXX is implemented in both A.saver and B.saver.
   //    One of the two will be used. Which one is undefined."
   //
#  define _CLASSHACK_(C,NAME) C##NAME
#  define _CLASSHACK(C,NAME) _CLASSHACK_(C,NAME)
#  define GlobalDefaults          _CLASSHACK(CLASS,GlobalDefaults)
#  define PrefsReader             _CLASSHACK(CLASS,PrefsReader)
#  define InvertedSlider          _CLASSHACK(CLASS,InvertedSlider)
#  define SimpleXMLNode           _CLASSHACK(CLASS,SimpleXMLNode)
#  define TextModeTransformer     _CLASSHACK(CLASS,TextModeTransformer)
#  define NonNilStringTransformer _CLASSHACK(CLASS,NonNilStringTransformer)
#  define XScreenSaverConfigSheet _CLASSHACK(CLASS,XScreenSaverConfigSheet)
#  ifdef USE_GL
#    define XScreenSaverView      _CLASSHACK(CLASS,Base)
#    define XScreenSaverGLView    CLASS
#  else
#    define XScreenSaverView      CLASS
#    define XScreenSaverGLView    #error
#  endif

#  include "XScreenSaverView.m"  // Must be first or principalClass is wrong
#  include "PrefsReader.m"
#  include "InvertedSlider.m"
#  include "XScreenSaverConfigSheet.m"
#  ifdef USE_GL
#   include "XScreenSaverGLView.m"
#  else
    void check_gl_error (const char *s) {}
    void clear_gl_error (void) {}
#  endif

#endif // INLINE_ALL_CLASSES
