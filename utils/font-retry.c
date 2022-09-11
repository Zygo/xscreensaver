/* xscreensaver, Copyright © 2018-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* The value of XScreenSaver's font resource strings is a comma-separated
   list of font names that look like this:

       Helvetica Bold Italic 12

   We parse that into an XLFD and pass that along to the underlying systems:
   XftFontOpenXlfd or jwxyz.

   Xft does aggressive substitution of unknown fonts, so if we don't get an
   exact match on the font name, we move on to the next element in the list.
   For the last element, we take whatever substitution we got.

   XftNameParse uses this syntax:

      "Helvetica Neue-12:style=Bold"
      "Helvetica Neue-12:style=Bold Italic"
      "Helvetica Neue-12:slant=110:weight=200"

   Cocoa uses PostScript names, with hyphens:

     [NSFont fontWithName:@"Helvetica-BoldOblique" size:12];
     [NSFont fontWithName:@"Times-Roman" size:12];

     Alternately, with separated styles:

     [[NSFontManager sharedFontManager]
      fontWithFamily:@"Helvetica" traits:NSBoldFontMask weight:5 size:12];

   Android separates names from styles:

     Paint.setTypeface (Typeface.create ("Helvetica", Typeface.BOLD));
     Paint.setTextSize (12);

     See android/xscreensaver/src/org/jwz/xscreensaver/jwxyz.java


   In XScreenSaver 6, USE_XFT is always true, as all programs now use Xft.

   In XScreenSaver 5, this file was compiled in two different ways:
   - As font-retry.o for programs that did not link with libXft;
   - As font-retry-xft.o for programs that did.

   #ifdef USE_XFT is whether Xft code should be called.
   #ifdef HAVE_XFT is whether libXft is natively available.

   The reason there are two defines is that if HAVE_XFT is not defined,
   the Xft API is still available through emulation provided by "xft.h".
 */

#define _GNU_SOURCE  /* Why is this here? */

#include "utils.h"
#include "visual.h"
#include "xft.h"
#include "font-retry.h"

extern const char *progname;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef DEBUG
#undef SELFTEST

#ifdef HAVE_JWXYZ
# define USE_XFT 1
#endif

#ifdef SELFTEST
static void xft_selftest (Display *dpy, int screen);
#endif


/* Parse font names of the form "Helvetica Neue Bold Italic 12".
 */
static char *
font_name_to_xlfd (const char *font_name)
{
  char *name = strdup (font_name);
  char *xlfd = 0;
  char *s, *s2, *b, *i, *o, c;
  float size;

  if (name[0] == '-' || name[0] == '*')
    return name;  /* Already an XLFD */

  /* Downcase ASCII; dash to space */
  for (s = name; *s; s++)
    if (*s >= 'A' && *s <= 'Z')
      *s += 'a'-'A';
    else if (*s == '-')
      *s = ' ';

  /* "Family NN" or "Family-NN" */
  s  = strrchr (name, ' ');
  s2 = strrchr (name, '-');
  if (s && s2 && s2 > s) s = s2;
  if (!s) goto FAIL;
  if (1 != sscanf (s+1, " %f %c", &size, &c)) goto FAIL;
  if (size <= 0) goto FAIL;
  *s = 0;

  /* "Family Bold", etc. */
  b = strstr (name, " bold");
  i = strstr (name, " italic");
  o = strstr (name, " oblique");
  if (b) *b = 0;
  if (i) *i = 0;
  if (o) *o = 0;

  xlfd = (char *) malloc (strlen(name) + 80);
  sprintf (xlfd, "-*-%s-%s-%s-*-*-*-%d-*-*-*-*-*-*",
           name,
           (b ? "bold" : "medium"),
           (i ? "i" : o ? "o" : "r"),
           (int) (size * 10));
  free (name);
  return xlfd;

 FAIL:
  fprintf (stderr, "%s: XFT: unparsable: \"%s\"\n", progname, font_name);
  if (name) free (name);
  if (xlfd) free (xlfd);
  return 0;
}


#ifdef USE_XFT

/* Xft silently substitutes fonts if the one you requested wasn't available.
   This leads to the deplorable situation where we ask for a fixed width font
   and are given a variable width font instead.  This doesn't happen with
   Courier, since Xft special-cases that one, but it happens with any other
   fixed width font that is not similarly privileged.

   Even worse: when Xft substitutes fonts, it makes no attempt to return fonts
   that are capable of displaying the language of the current locale.  For
   example: if your locale is set to Japanese and you request "Helvetica", Xft
   silently substitutes "Nimbus Sans" -- a font which is not capable of
   displaying Japanese characters.  If instead you requested "asdfasdfghjkl",
   you get "Noto Sans CJK JP", which does work.  So that's just spectacular.

   Since there does not appear to be a way to ask Xft whether a particular
   font exists, we load the font and then check to see if the name we got
   is the name we requested.

   In more detail:

     - XftFontOpenXlfd is defined as:
       XftFontOpenPattern (XftFontMatch (XftXlfdParse (xlfd)))
     - XftFontOpenName is defined as:
       XftFontOpenPattern (XftFontMatch (XftNameParse (name)))
     - Calling XftFontOpenPattern with a pattern that has not been filtered
       through XftFontMatch does not work.
     - XftFontMatch substitutes another font if the pattern doesn't match.
         - If the pattern has family "Courier", it substitutes a fixed width
           font, e.g. "Liberation Mono".
         - Otherwise it substitutes a variable width font, e.g. "DejaVu Sans".
           It does this even if the pattern contained "spacing=100" or "110",
           indicating a monospace or charcell font.
     - XftXlfdParse does not translate "spacing" from XLFD to XftPattern,
       but that doesn't matter since XftFontMatch ignores it anyway.

   Xft source is here:
   https://opensource.apple.com/source/X11ForMacOSXSource/X11ForMacOSXSource-1.0/xc/lib/Xft1/

   Courier, Helvetica and the other historical PostScript font names seem to
   be special-cased in /etc/fonts/conf.d/ in the files 30-metric-aliases.conf,
   45-latin.conf and/or 60-latin.conf, which uses an idiosyncratic scripting
   language implemented as XML!  Some incomplete documentation on that baroque
   mess is here:
   https://www.freedesktop.org/software/fontconfig/fontconfig-user.html

   The Xft configuration files seem to special-case the names "monospace",
   "serif" and "sans-serif" as generic fallback fonts.

   However, "sans-serif" is problematic because it does not survive the trip
   through XftXlfdParse and XftFontOpenPattern -- XLFD font families cannot
   include hyphens.  So we have no choice but to turn it into "sans serif",
   which is *not* special-cased by the Xft config files.

   In summary, everything is terrible, and it's a wonder anything works at all.
 */
static Bool
xlfd_substituted_p (XftFont *f, const char *xlfd)
{
# ifndef HAVE_XFT
  return False;		/* No substitutions in the Xft emulator. */
# else /* HAVE_XFT */

  Bool ret = True;
  const char *oxlfd = xlfd;
  char *s = 0;
  char *xname = 0;
  char fname[1024];

  if (*xlfd == '-' || strchr (xlfd, '*'))	/* it's an xlfd */
    {
      if (*xlfd != '-') goto FAIL;
      xlfd++;
      s = strchr (xlfd, '-'); if (!s) goto FAIL;	/* skip foundry */
      xlfd = s+1;
      s = strchr (xlfd, '-'); if (!s) goto FAIL;	/* skip family */

      {
        char buf[1024];
        XftPattern *pat = XftXlfdParse (oxlfd, True, True);
        XftNameUnparse (pat, buf, sizeof(buf)-1);
      }
    }
  else						/* It's an Xft name */
    {
      s = strchr (xlfd, '-'); if (!s) goto FAIL;	/* skip family */
    }

  xname = strdup (xlfd);
  xname[s - xlfd] = 0;

  *fname = 0;
  XftNameUnparse (f->pattern, fname, sizeof(fname)-1);
  s = strchr (fname, ':');   /* Strip to "Family-NN" */
  if (s) *s = 0;
  s = strrchr (fname, '-');  /* Strip to family */
  if (s) *s = 0;

  ret = !strcasestr (fname, xname);

#  ifdef DEBUG
  if (ret)
    fprintf (stderr, "%s: XFT: requested \"%s\" but got \"%s\"\n",
             progname, xname, fname);
#  endif

 FAIL:

  if (!s)
    fprintf (stderr, "%s: unparsable XLFD: %s\n", progname, oxlfd);
  if (xname) free (xname);
  return ret;
# endif /* HAVE_XFT */
}
#endif /* USE_XFT */


static void *
load_font_retry_1 (Display *dpy, int screen, const char *font_list, Bool xft_p)
{

# ifdef USE_XFT
#  define LOADFONT(F) (xft_p \
                      ? (void *) XftFontOpenXlfd (dpy, screen, (F))  \
                      : (void *) XLoadQueryFont (dpy, (F)))
#  define UNLOADFONT(F) (xft_p \
                         ? (void) XftFontClose (dpy, (F)) \
                         : (void) XFreeFont (dpy, (F)))
# else
#  define LOADFONT(F) ((void *) XLoadQueryFont (dpy, (F)))
#  define UNLOADFONT(F) XFreeFont (dpy, (F))
# endif

  char *font_name = 0;
  void *f = 0;
  void *fallback = 0;

# ifndef USE_XFT
  if (xft_p) abort();
# endif

# ifdef SELFTEST
  xft_selftest (dpy, screen);
# endif

  if (! font_list) font_list = "<null>";

  /* Treat the string as a comma-separated list of font names.
     Names are XLFDs or the XScreenSaver syntax described above.
     Try to load each of them in order.
     If a substitution was made, keep going, unless this is the last.
   */
  if (font_list)
    {
      char *token = strdup (font_list);
      char *otoken = token;
      char *name2, *lasts;
      if (!token) abort();
      while ((name2 = strtok_r (token, ",", &lasts)))
         {
          int L;
          token = 0;

          /* Strip leading and trailing whitespace */
          while (*name2 == ' ' || *name2 == '\t' || *name2 == '\n')
            name2++;
          L = strlen(name2);
          while (L && (name2[L-1] == ' ' || name2[L-1] == '\t' ||
                       name2[L-1] == '\n'))
            name2[--L] = 0;

          if (font_name) free (font_name);
          font_name = font_name_to_xlfd (name2);

# ifdef DEBUG
          fprintf (stderr, "%s: trying \"%s\" as \"%s\"\n", progname,
                   name2, font_name);
# endif

          f = LOADFONT (font_name);

# ifdef USE_XFT
          /* If we did not get an exact match for the font family we requested,
             reject this font and try the next one in the list. */
          if (f && xft_p && xlfd_substituted_p (f, font_name))
            {
              if (fallback)
                UNLOADFONT (fallback);
              fallback = f;
              f = 0;
            }
#  ifdef DEBUG
          else if (!f)
            fprintf (stderr, "%s: no match for \"%s\"\n", progname, font_name);
#  endif
# endif /* USE_XFT */

          if (f) break;
        }
      free (otoken);
      if (!font_name) abort();

      /* If the last font in the list was an Xft pattern that matched but
         was inexact, use it. */
      if (!f)
        {
          f = fallback;
          fallback = 0;
        }
    }

  if (!font_name) abort();

# if !defined(HAVE_XFT) && !defined(HAVE_JWXYZ)
  /* Xft is now REQUIRED. All of XScreenSaver's resource settings use
     font names that assume that it is available. However, this lets you
     limp along without it. But, really, don't do that to yourself. */
#   define FALLBACK2(F) do {    \
      free (font_name);         \
      font_name = (strdup(F));  \
      f = LOADFONT (font_name); \
      if (f) fprintf (stderr, "%s: non-XFT fallback \"%s\"\n", \
                      progname, font_name);                    \
    } while (0)
    if (!f && !strcasestr (font_name, "mono")) {
      if (!f) FALLBACK2 ("-*-sans serif-medium-r-*-*-*-180-*-*-*-*-*-*");
      if (!f) FALLBACK2 ("-*-helvetica-medium-r-*-*-*-180-*-*-*-*-*-*");
      if (!f) FALLBACK2 ("-*-nimbus sans l-medium-r-*-*-*-180-*-*-*-*-*-*");
    }
    if (!f) FALLBACK2 ("-*-courier-medium-r-*-*-*-180-*-*-*-*-*-*");
    if (!f) FALLBACK2 ("fixed");
# endif

  if (!f) abort();

# ifdef DEBUG
  if (f && font_name)
    fprintf (stderr, "%s:%s loaded %s\n", progname,
             (xft_p ? " XFT:" : ""), font_name);
#  if defined(USE_XFT) && defined(HAVE_XFT)
   if (xft_p && f)
     {
       XftPattern *p = ((XftFont *) f)->pattern;
       char name[1024];
       char *s, *s1, *s2, *s3;
       XftNameUnparse (p, name, sizeof(name)-1);
       s = strstr (name, ":style=");
       s1 = (s ? strstr (s+1, ",") : 0);
       s2 = (s ? strstr (s+1, ":") : 0);
       s3 = (s1 && s1 < s2 ? s1 : s2);
       if (s3) strcpy (s3+1, " [...]");
       fprintf (stderr, "%s: XFT name: %s\n", progname, name);
     }
#  endif /* USE_XFT && HAVE_XFT */
# endif /* DEBUG */

   if (fallback) UNLOADFONT (fallback);
   if (font_name) free (font_name);
  return f;
}


#if 1  /* No longer used in XScreenSaver 6.
          (Used by retired flag, juggle, xsublim) */
XFontStruct *
load_font_retry (Display *dpy, const char *font_list)
{
  return (XFontStruct *) load_font_retry_1 (dpy, 0, font_list, 0);
}
#endif

#if defined(USE_XFT) || defined(HAVE_JWXYZ)
XftFont *
load_xft_font_retry (Display *dpy, int screen, const char *font_list)
{
  return (XftFont *) load_font_retry_1 (dpy, screen, font_list, 1);
}
#endif



#ifdef SELFTEST
static void
xft_selftest (Display *dpy, int screen)
{
  int i;
  const char *tests[] = {
    "-*-ocr a std-medium-r-*-*-*-480-*-*-m-*-*-*",
    "OCR A Std-48",
    "OCR A Std-48:style=Bold Italic",
    "OCR A Std-48:spacing=100",
    "OCR A Std-48:spacing=110",
    "-*-courier-medium-r-*-*-*-480-*-*-m-*-*-*",
    "-*-courier-bold-o-*-*-*-480-*-*-m-*-*-*",
    "Courier-48:style=Bold Italic",
    "Courier-48:style=Italic Bold",  /* seems to be illegal */
    "Courier-48:spacing=100",
    "Courier-48:spacing=110",
    "-*-helvetica-bold-o-*-*-*-480-*-*-m-*-*-*",
    "Helvetica-48:style=Bold Italic",
    "Liberation Mono-48:style=Bold Italic",
    "Liberation Sans-48:style=Bold Italic",
    "-*-sans serif-bold-o-*-*-*-480-*-*-m-*-*-*",
    "-*-sans-serif-bold-o-*-*-*-480-*-*-m-*-*-*",
    "-*-sans\\-serif-bold-o-*-*-*-480-*-*-m-*-*-*",
  };

  const char *tests2[] = { "Helvetica 10",
                           "Helvetica Bold 10",
                           "Helvetica Bold Italic 10",
                           "Helvetica Oblique Bold-10.5",
                           "Times New Roman-10",
                           "Times New Roman Bold-10",
                           "Times New Roman-Bold Oblique Italic 10",
                           "Times New Roman-Oblique Italic Bold 10",
                           "Times-20:style=Bold",
                           "Times-Oblique-20:style=Bold",
                           "sans serif-20:style=Bold",
                           "sans-serif-20:style=Bold",
                           "sans\\-serif-20:style=Bold",
  };

  fprintf (stderr, "\n");
  for (i = 0; i < countof(tests2); i++)
    fprintf (stderr, "%s\n%s\n\n", tests2[i], font_name_to_xlfd (tests2[i]));

  fprintf (stderr, "\n");
  for (i = 0; i < countof(tests); i++) {
    const char *name1 = tests[i];
    XftResult ret;
    XftPattern *pat1 = 0, *pat2 = 0, *pat3 = 0;
    char name2[1024], name3[1024];
    XftFont *ff;
    Bool xlfd_p = (*name1 == '-');

# define TRUNC(V) do {                          \
      char *s = strstr (V, ":style=");          \
      char *s1 = (s ? strstr (s+1, ",") : 0);   \
      char *s2 = (s ? strstr (s+1, ":") : 0);   \
      char *s3 = (s1 && s1 < s2 ? s1 : s2);     \
      if (s3) strcpy (s3+1, " [...]");          \
    } while(0)

    *name2 = 0;

    /* Name to Parse */

    if (xlfd_p)
      pat1 = XftXlfdParse (name1, True, True);
    else
      pat1 = XftNameParse (name1);
    XftNameUnparse (pat1, name2, sizeof(name2)-1);
    TRUNC(name2);
    fprintf (stderr, "%s (\"%s\")\n\t-> \"%s\"\n",
             (xlfd_p ? "XftXlfdParse" : "XftNameParse"),
             name1, name2);


    /* Name to pattern to Open */

    ff = XftFontOpenPattern (dpy, pat1);
    if (ff) {
      pat2 = ff->pattern;
      XftNameUnparse (pat2, name3, sizeof(name3)-1);
    } else {
      pat2 = 0;
      strcpy (name3, "NULL");
    }
    TRUNC(name3);
    fprintf (stderr, "XftFontOpenPattern (\"%s\")\n\t-> \"%s\"\n",
             name2, name3);


    /* Name to pattern to Match */

    pat2 = XftFontMatch (dpy, screen, pat1, &ret);
    XftNameUnparse (pat2, name3, sizeof(name3)-1);
    TRUNC(name3);
    fprintf (stderr, "XftFontMatch (\"%s\")\n\t-> \"%s\", %s\n",
             name2, name3,
             (ret == XftResultMatch ? "Match" :
              ret == XftResultNoMatch ? "NoMatch" :
              ret == XftResultTypeMismatch ? "TypeMismatch" :
              ret == XftResultNoId ? "NoId" : "???"));


    /* Name to pattern to Match to Open */

    strcpy (name2, name3);
    ff = XftFontOpenPattern (dpy, pat2);
    if (ff) {
      pat3 = ff->pattern;
      XftNameUnparse (pat3, name3, sizeof(name3)-1);
    } else {
      pat3 = 0;
      strcpy (name3, "NULL");
    }
    TRUNC(name3);
    fprintf (stderr, "XftFontOpenPattern (\"%s\")\n\t-> \"%s\"\n",
             name2, name3);


    /* Name to Open */

    ff = (xlfd_p
          ? XftFontOpenXlfd (dpy, screen, name1)
          : XftFontOpenName (dpy, screen, name1));
    *name2 = 0;
    if (ff) {
      pat1 = ff->pattern;
      XftNameUnparse (pat1, name2, sizeof(name2)-1);
    } else {
      strcpy (name2, "NULL");
    }
    TRUNC(name2);
    fprintf (stderr, "%s (\"%s\")\n\t-> \"%s\"\n",
             (xlfd_p ? "XftFontOpenXlfd" : "XftFontOpenName"),
             name1, name2);
    fprintf (stderr, "\n");
  }

  exit (0);
}
#endif /* SELFTEST */
