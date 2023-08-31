/* xftwrap.c --- XftDrawStringUtf8 with multi-line strings.
 * xscreensaver, Copyright © 2021 Jamie Zawinski <jwz@jwz.org>
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

#include "utils.h"
#include "xft.h"
#include "xftwrap.h"

#undef MAX
#undef MIN
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))


#ifdef DEBUG
static void
LOG(const char *ss, const char *s, int n)
{
  int i;
  fprintf(stderr,"####%s [", ss);
  for (i = 0; i < n; i++) fprintf(stderr, "%c", s[i]);
  fprintf(stderr,"]\n");
}
#else
# define LOG(ss,s,n) /**/
#endif


/* Returns a new string word-wrapped to fit in the width in pixels.
 */
char *
xft_word_wrap (Display *dpy, XftFont *font, const char *str, int pixels)
{
  const char *in = str;
  char *ret = (char *) malloc (strlen(in) + 2);
  char *out = ret;
  const char *line_out = out;
  char *word_out = 0;
  while (1)
    {
      if (*in == ' ' || *in == '\t' ||
          *in == '\r' || *in == '\n' ||
          *in == 0)
        {
          Bool done = (*in == 0);	/* To wrap the *last* word. */
          XGlyphInfo overall;
          Bool nl = (*in == '\r' || *in == '\n');

          if (nl) in++;
          while (*in == ' ' || *in == '\t') in++;
          in--;

          XftTextExtentsUtf8 (dpy, font,
                              (FcChar8 *) line_out,
                              out - line_out,
                              &overall);
          if (overall.width - overall.x >= pixels &&
              word_out)
            {
              word_out[0] = '\n';
              line_out = word_out + 1;
              word_out = 0;
              if (done) break;
              *out++ = *in;
            }
          else
            {
              if (done) break;
              word_out = out;
              *out++ = *in;
              if (nl)
                {
                  line_out = out + 1;
                  word_out = 0;
                }
            }

          if (done) break;
        }
      else
        {
          *out++ = *in;
        }
      in++;
    }

  *out = 0;

  return ret;
}


/* Like XftTextExtentsUtf8, but handles multi-line strings.
   XGlyphInfo will contain the bounding box that encloses all of the text.
   Return value is the number of lines in the text, >= 1.
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

              ox1 = overall->x;
              oy1 = overall->y;
              ox2 = ox1 + overall->width;
              oy2 = oy1 + overall->height;

              line_y += font->ascent + font->descent;  /* advance origin */

              nx1 = gi.x;
              ny1 = gi.y + line_y;
              nx2 = nx1 + gi.width;
              ny2 = ny1 + gi.height + line_y;

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

