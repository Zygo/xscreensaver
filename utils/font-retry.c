/* xscreensaver, Copyright © 2018-2026 Jamie Zawinski <jwz@jwz.org>
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

   We parse that into Xft's syntax and pass that along to the underlying
   systems: XftFontOpenPattern or jwxyz.

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


   In XScreenSaver 6, all programs now use Xft via XftFontOpenPattern.

   In XScreenSaver 6.14 and earlier, XftFontOpenXlfd was used instead.
   This was a bad idea.

   In XScreenSaver 5, this file was compiled in two different ways:
   - As font-retry.o for programs that did not link with libXft;
   - As font-retry-xft.o for programs that did.

   #ifdef HAVE_XFT is whether libXft is natively available (as opposed
   to the partially-emulated version provided by "xft.h").
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

#ifdef SELFTEST
static void xft_selftest (Display *dpy, int screen);
#endif


/* Parse font names of the form "Helvetica Neue Bold Italic 12"
   to XFT style, "Helvetica Neue-12:style=Bold Italic"
 */
static char *
font_name_to_xft_name (const char *font_name)
{
  char *name = 0;
  char *out = 0;
  char *s, *s2, *b, *i, *o, c;
  float size;

  if (font_name[0] == '-' || font_name[0] == '*')
    goto FAIL;  /* Looks like an XLFD. */

  if (!strcmp (font_name, "fixed"))
    font_name = "fixed 12";

  name = strdup (font_name);

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

  out = (char *) malloc (strlen(name) + 80);
  sprintf (out, "%s-%d", name, (int) size);
  if (b && i)
    strcat (out, ":style=Bold Italic");
  if (b && o)
    strcat (out, ":style=Bold Oblique");
  else if (b)
    strcat (out, ":style=Bold");
  else if (i)
    strcat (out, ":style=Italic");
  else if (o)
    strcat (out, ":style=Oblique");
  free (name);
  return out;

 FAIL:
  fprintf (stderr, "%s: XFT: unparsable: \"%s\"\n", progname, font_name);
  if (name) free (name);
  if (out) free (out);
  return 0;
}


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

   During the transition from XFontStruct to Xft, I initially made an effort
   to allow XLFD names to be used, which is why I was messing around with the
   extremely troublesome Xft*Xlfd routines.

   Xft source is here:
   https://opensource.apple.com/source/X11ForMacOSXSource/X11ForMacOSXSource-1.0/xc/lib/Xft1/

   Doc: https://www.keithp.com/~keithp/render/Xft.tutorial

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
xft_substituted_p (XftFont *f, XftPattern *pat)
{
# ifndef HAVE_XFT
  return False;		/* No substitutions in the Xft emulator. */
#else /* HAVE_XFT */
  Bool ok = False;
  const char *ofam = 0, *nfam = 0;
  XftPatternGetString (pat, XFT_FAMILY, 0, &ofam);
  XftPatternGetString (f->pattern, XFT_FAMILY, 0, &nfam);

  if (!ofam) ofam = "NULL";
  if (!nfam) nfam = "NULL";
  ok = (!strcasecmp (ofam, nfam));

#  ifdef DEBUG
  if (!ok)
    fprintf (stderr, "%s: XFT: requested \"%s\" but got \"%s\"\n",
             progname, ofam, nfam);
#  endif

  return !ok;
#endif /* HAVE_XFT */
}


XftFont *
load_xft_font_retry (Display *dpy, int screen, const char *font_list)
{
  char *font_name = 0;
  XftFont *f = 0;
  XftFont *fallback = 0;

# ifdef SELFTEST
  xft_selftest (dpy, screen);
# endif

  if (! font_list) font_list = "<null>";

  /* Treat the string as a comma-separated list of font names.
     Names are in the XScreenSaver syntax described above.
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
          font_name = font_name_to_xft_name (name2);

# ifdef DEBUG
          fprintf (stderr, "%s: trying \"%s\" as \"%s\"\n", progname,
                   name2, font_name);
# endif

          if (font_name)
            {
              XftResult ret;
              XftPattern *pat1 = XftNameParse (font_name);
              XftPattern *pat2 = XftFontMatch (dpy, screen, pat1, &ret);
              f = XftFontOpenPattern (dpy, pat2);

              /* If we did not get an exact match for the font family we
                 requested, reject this font and try the next one in the
                 list. */
              if (f && xft_substituted_p (f, pat1))
                {
                  if (fallback)
                    XftFontClose (dpy, fallback);
                  fallback = f;
                  f = 0;
                }

              XftPatternDestroy (pat1);
              /* XftFontOpenPattern seems to retain a pointer to pat2? */
              /* XftPatternDestroy (pat2); */
            }
# ifdef DEBUG
          else if (!f)
            fprintf (stderr, "%s: no match for \"%s\"\n", progname, font_name);
# endif

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
  abort();  /* Real Xft is required under real X11. */
# endif

  if (!f) abort();

# ifdef DEBUG
  if (f && font_name)
    fprintf (stderr, "%s: XFT: loaded %s\n", progname, font_name);
#  ifdef HAVE_XFT
   if (f)
     {
       XftPattern *p = f->pattern;
       char name[2048];
       char *s, *s1, *s2, *s3;
       XftNameUnparse (p, name, sizeof(name)-1);
       s = strstr (name, ":style=");
       s1 = (s ? strstr (s+1, ",") : 0);
       s2 = (s ? strstr (s+1, ":") : 0);
       s3 = (s1 && s1 < s2 ? s1 : s2);
       if (s3) strcpy (s3+1, " [...]");
       fprintf (stderr, "%s: XFT name: %s\n", progname, name);
     }
#  else  /* !HAVE_XFT */
   fprintf (stderr, "%s: X11 name: %s\n", progname, f->name);
#  endif /* HAVE_XFT */
# endif /* DEBUG */

   if (fallback) XftFontClose (dpy, fallback);
   if (font_name) free (font_name);
  return f;
}


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
    "OCR A-48",
    "OCR A-48:style=Bold Italic",
    "OCR A-48:spacing=100",
    "OCR A-48:spacing=110",
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
    if (pat1)
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
    if (pat2)
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
