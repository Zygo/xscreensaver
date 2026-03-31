/* xftwrap.c --- XftDrawStringUtf8 with multi-line strings.
 * xscreensaver, Copyright © 2021-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include "utils.h"
#include "xft.h"
#include "xftwrap.h"
#include "utf8wc.h"

#undef MAX
#undef MIN
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))


#ifdef DEBUG
static void
LOG(const char *ss, const unsigned char *s, int n)
{
  int i;
  fprintf(stderr,"####%s [", ss);
  for (i = 0; i < n; i++)
    if (s[i] == '\n')
      fprintf (stderr, "\\n");
    else if (s[i] == '\t')
      fprintf (stderr, "\\t");
    else if (s[i] < ' ' /* || s[i] > 0177 */ )
      fprintf (stderr, "\\x%02X", s[i]);
    else
      fprintf (stderr, "%c", s[i]);
  fprintf (stderr,"]\n");
}
#else
# define LOG(ss,s,n) /**/
#endif


/* Returns a new string word-wrapped to fit in the width in pixels.
   Words may be wrapped at whitespace or ASCII punctuation.
   Words longer than the width will be split.
 */
char *
xft_word_wrap (Display *dpy, XftFont *font, const char *str, int pixels)
{
  const unsigned char *in  = (unsigned char *) str;
  const unsigned char *end = (unsigned char *) str + strlen (str);
  unsigned char *ret = (unsigned char *) malloc (strlen(str) * 2);
  unsigned char *out = ret;

  const unsigned char *splitpoint_in = in;
  unsigned char *splitpoint_out      = out;
  const unsigned char *start_of_line = out;

  while (in < end)
    {
      unsigned long uc = 0;
      long ucbytes = utf8_decode_combining (in, end-in, &uc);
      XGlyphInfo overall;
      int i;

      for (i = 0; i < ucbytes; i++)
        *out++ = in[i];

      in += ucbytes;

      if (isspace (*in)   || ispunct (*in) ||
          uc_isspace (uc) || uc_ispunct (uc))
        {
          splitpoint_in  = in;
          splitpoint_out = out;
        }

      XftTextExtentsUtf8 (dpy, font,
                          (FcChar8 *) start_of_line,
                          out - start_of_line,
                          &overall);
      if (overall.width - overall.x < pixels)
        {
          LOG("keep", start_of_line, out-start_of_line);
        }
      else
        {
          /* Backtrack 'out' and 'in' to the last splitpoint.
             Insert a newline and restart.
           */
          LOG("split", start_of_line, splitpoint_out - start_of_line);
          LOG("sp B2", splitpoint_out, out - splitpoint_out);
          if (splitpoint_out == start_of_line)
            {
              /* Breaking in the middle of a long word. */
              LOG("long", splitpoint_in, in - splitpoint_in);

              /* Trim trailing horizontal whitespace on line; does not work
                 on Unicode whitespace. */
              while (out > ret && (isblank (out[-1])))
                out--;

              *out++ = '\n';
              splitpoint_out = out;
              start_of_line = out;
            }
          else
            {
              LOG("back", splitpoint_in, in - splitpoint_in);
              out = splitpoint_out;
              in  = splitpoint_in;

              /* Trim trailing whitespace and blank lines; does not work
                 on Unicode whitespace. */
              while (out > ret && (isblank (out[-1])))
                out--;

              *out++ = '\n';
              splitpoint_out = out;
              start_of_line = out;

              /* Swallow horizontal whitespace after breaking line. */
              ucbytes = utf8_decode_combining (in, end-in, &uc);
              while (uc_isspace (uc))
                {
                  in += ucbytes;
                  ucbytes = utf8_decode_combining (in, end-in, &uc);
                }
            }
        }
    }

  /* Trim trailing whitespace and blank lines; does not work on Unicode. */
  while (out > ret && (isspace (out[-1])))
    out--;

  *out = 0;

  LOG("DONE", ret, strlen((char *) ret));

  return (char *) ret;
}


/* Like XftTextExtentsUtf8, but handles multi-line strings.
   XGlyphInfo will contain the bounding box that encloses all of the text.
   Return value is the number of lines in the text, >= 1.

   overall->y will be the distance from the top of the ink to the origin of
   the first character in the string, as usual.  If there are multiple lines,
   you can think of them as extremely tall descenders below the origin.
 */
int
XftTextExtentsUtf8_multi (Display *dpy, XftFont *font,
                          const FcChar8 *str, int len, XGlyphInfo *overall)
{
  int i, start = 0;
  int lines = 0;
  int line_y = 0;
  for (i = 0; i <= len; i++)
    {
      if (i == len || str[i] == '\r' || str[i] == '\n')
        {
          XGlyphInfo gi;
          XftTextExtentsUtf8 (dpy, font,
                              str + start,
                              i - start,
                              &gi);
          if (lines == 0)
            *overall = gi;
          else
            {
              /* Find the union of the two bounding boxes, placed at their
                 respective origins. */
              int ox1, oy1, ox2, oy2;		/* bbox of 'overall' */
              int nx1, ny1, nx2, ny2;		/* bbox of 'gi' */
              int ux1, uy1, ux2, uy2;		/* union */

              /* Cumulative bbox prior to this line */
              ox1 = overall->x;
              oy1 = overall->y;
              ox2 = ox1 + overall->width;
              oy2 = oy1 + overall->height;

              /* Advance the origin of this line downward. */
              line_y += font->ascent + font->descent;

              /* Bounding box of this line, with origin adjusted to be
                 the origin of line 0. */
              nx1 = gi.x;
              ny1 = gi.y + line_y;
              nx2 = nx1 + gi.width;
              ny2 = ny1 + gi.height;

              /* Find the union of the two same-origin bboxes. */
              ux1 = MIN (ox1, nx1);		/* upper left */
              uy1 = MIN (oy1, ny1);
              ux2 = MAX (ox2, nx2);		/* bottom right */
              uy2 = MAX (oy2, ny2);

              overall->x = ux1;
              overall->y = uy1;
              overall->width  = ux2 - ux1;
              overall->height = uy2 - uy1;
            }
          lines++;
          start = i+1;
        }
    }

  return lines;
}


/* Like XftDrawStringUtf8, but handles multi-line strings.
   Alignment is 1, 0 or -1 for left, center, right.
 */
void
XftDrawStringUtf8_multi (XftDraw *xftdraw, const XftColor *color,
                         XftFont *font, int x, int y, const FcChar8 *str,
                         int len,
                         int alignment)
{
  Display *dpy = XftDrawDisplay (xftdraw);
  int i, start = 0;
  XGlyphInfo overall;
  if (len == 0) return;

  XftTextExtentsUtf8_multi (dpy, font, str, len, &overall);

  for (i = 0; i <= len; i++)
    {
      if (i == len || str[i] == '\r' || str[i] == '\n')
        {
          XGlyphInfo gi;
          int x2 = x;
          XftTextExtentsUtf8 (dpy, font, str + start, i - start, &gi);
          switch (alignment) {
          case  1: break;
          case  0: x2 += (overall.width - gi.width) / 2; break;
          case -1: x2 += (overall.width - gi.width);     break;
          default: abort(); break;
          }

          XftDrawStringUtf8 (xftdraw, color, font, x2, y,
                             str + start,
                             i - start);
          y += font->ascent + font->descent;
          start = i+1;
        }
    }
}

