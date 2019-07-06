/* xscreensaver, Copyright (c) 2018 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Like XLoadQueryFont, but if it fails, it tries some heuristics to
   load something close.
 */

#define _GNU_SOURCE

#include "utils.h"
#include "visual.h"
#include "xft.h"
#include "font-retry.h"

extern const char *progname;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef DEBUG

static void *
load_font_retry_1 (Display *dpy, int screen, const char *xlfd, Bool xft_p)
{

# ifdef USE_XFT
#  define LOADFONT(F) (xft_p \
                      ? (void *) XftFontOpenXlfd (dpy, screen, (F)) \
                      : (void *) XLoadQueryFont (dpy, (F)))
# else
#  define LOADFONT(F) ((void *) XLoadQueryFont (dpy, (F)))
# endif

  void *f = xlfd ? LOADFONT(xlfd) : 0;

# ifndef USE_XFT
  if (xft_p) abort();
# endif

# ifdef HAVE_JWXYZ
  return f;
# else /* !HAVE_JWXYZ */
  if (! xlfd) xlfd = "<null>";
  if (f)
    {
# ifdef DEBUG
      fprintf (stderr, "%s: loaded %s\n", progname, xlfd);
# endif
      return f;
    }
  else
    {
      Bool bold_p   = (!!strcasestr (xlfd, "-bold-") ||
                       !!strcasestr (xlfd, "-ocr"));
      Bool italic_p = (!!strcasestr (xlfd, "-i-") ||
                       !!strcasestr (xlfd, "-o-"));
      Bool fixed_p  = (!!strcasestr (xlfd, "courier") ||
                       !!strcasestr (xlfd, "-ocr") ||
                       !!strcasestr (xlfd, "-m-") ||
                       !!strcasestr (xlfd, "-c-"));
      int size = 0;

# ifdef DEBUG
      fprintf (stderr, "%s: failed %s\n", progname, xlfd);
# endif

      if (!strcmp (xlfd, "vga"))  /* BSOD uses this: it has no XLFD name. */
        fixed_p = True, size = 120;

      /* Look for the first number in the string like "-180-" */
      if (! size)
        {
          const char *s;
          for (s = xlfd; *s; s++)
            if (s[0] == '-' && s[1] >= '0' && s[1] <= '9')
              {
                int i = s[1] - '0';
                const char *s2 = s+2;
                while (*s2 >= '0' && *s2 <= '9')
                  {
                    i = i * 10 + *s2 - '0';
                    s2++;
                  }
                if (*s2 != '-') continue;          /* Number ends with dash */
                if (i < 60 || i >= 2000) continue; /* In range 6pt - 200pt */
                if (i % 10) continue;              /* Multiple of 10 */

                size = i;
                break;
              }
        }

      if (! size)
        {
          fprintf (stderr, "%s: unloadable, unparsable font: \"%s\"\n",
                   progname, xlfd);
          xlfd = "fixed";
          return LOADFONT(xlfd);
        }
      else
        {
          const char *variable[] = {
            "helvetica",
            "arial",
            "bitstream vera sans",
            "gill sans",
            "times",
            "times new roman",
            "new century schoolbook",
            "utopia",
            "palatino",
            "lucida",
            "bitstream charter",

            /* Don't use a wildcard family. If none of the above worked, then
               then almost none of the X11 fonts are installed, and it's not
               unlikely that "-*-*-medium-r-*-*-*-140-*-*-*-10646-1" will
               match an Arabic or or Japanese font that contains no Latin
               glyphs at all, even in a Latin locale. So in that case, just
               let "helvetica" fall back to "fixed".
             */
            /* "*" */
          };
          const char *fixed[] = {
            "courier",
            "courier new",
            "courier 10 pitch",
            "lucidatypewriter",
            "american typewriter",
            "fixed",
            "ocr a std",
            /* As above, but "can't happen" because we already tried fixed? */
            /* "*" */
          };
          const char *charsets[] = { "iso10646-1", "iso8859-1", "*-*" };
          const char *weights[]  = { "bold", "medium" };
          const char *slants[]   = { "o", "i", "r" };
          const char *spacings[] = { "m", "c", "p" };
          int a, b, c, d, e, g;
          char buf[1024];

          for (a = 0; a < countof(charsets); a++)
            for (b = (bold_p ? 0 : 1); b < countof(weights); b++)
              for (c = (italic_p ? 0 : 2); c < countof(slants); c++)
                for (d = 0;
                     d < (fixed_p ? countof(fixed) : countof(variable));
                     d++)
                  for (g = size; g >= 60; g -= 10)
                    for (e = (fixed_p ? 0 : 2); e < countof(spacings); e++)
                      {
                        sprintf (buf,
                                 "-%s-%s-%s-%s-%s-%s-%s-%d-%s-%s-%s-%s-%s",
                                 "*",			/* foundry */
                                 (fixed_p ? fixed[d] : variable[d]),
                                 weights[b],
                                 slants[c],
                                 "*",			/* set width */
                                 "*",			/* add style */
                                 "*",			/* pixel size */
                                 g,			/* point size */
                                 "*",			/* x resolution */
                                 "*",			/* y resolution */
                                 spacings[e],
                                 "*",			/* average width */
                                 charsets[a]);
# ifdef DEBUG
                        fprintf(stderr, "%s: trying %s\n", progname, buf);
# endif
                        f = LOADFONT(buf);
                        if (f)
                          {
# ifdef DEBUG
                            fprintf (stderr,
                                     "%s: substituted \"%s\" for \"%s\"\n",
                                     progname, buf, xlfd);
# endif
                            return f;
                          }
                      }

          fprintf (stderr, "%s: unable to find any alternatives to \"%s\"\n",
                   progname, xlfd);
          xlfd = "fixed";
          return LOADFONT(xlfd);
        }
    }
# endif /* !HAVE_JWXYZ */
}

XFontStruct *
load_font_retry (Display *dpy, const char *xlfd)
{
  return (XFontStruct *) load_font_retry_1 (dpy, 0, xlfd, 0);
}

#ifdef USE_XFT
XftFont *
load_xft_font_retry (Display *dpy, int screen, const char *xlfd)
{
  return (XftFont *) load_font_retry_1 (dpy, screen, xlfd, 1);
}

#elif defined(HAVE_JWXYZ)

XftFont *
load_xft_font_retry (Display *dpy, int screen, const char *xlfd)
{
  return XftFontOpenXlfd (dpy, screen, xlfd);
}

#endif /* !HAVE_JWXYZ */
