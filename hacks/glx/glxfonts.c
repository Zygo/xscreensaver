/* glxfonts, Copyright (c) 2001-2008 Jamie Zawinski <jwz@jwz.org>
 * Loads X11 fonts for use with OpenGL.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <AGL/agl.h>
#else
# include <GL/glx.h>
# include <GL/glu.h>
#endif

#include "resources.h"
#include "glxfonts.h"

/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

/* screenhack.h */
extern char *progname;


#undef DEBUG  /* Defining this causes check_gl_error() to be called inside
                 time-critical sections, which could slow things down (since
                 it might result in a round-trip, and stall of the pipeline.)
               */


/* Loads the font named by the X resource "res".
   Returns an XFontStruct.
   Also converts the font to a set of GL lists and returns the first list.
*/
void
load_font (Display *dpy, char *res, XFontStruct **font_ret, GLuint *dlist_ret)
{
  XFontStruct *f;

  const char *font = get_string_resource (dpy, res, "Font");
  const char *def1 = "-*-helvetica-medium-r-normal-*-180-*";
  const char *def2 = "fixed";
  Font id;
  int first, last;

  if (!res || !*res) abort();
  if (!font_ret && !dlist_ret) abort();

  if (!font) font = def1;

  f = XLoadQueryFont(dpy, font);
  if (!f && !!strcmp (font, def1))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def1);
      font = def1;
      f = XLoadQueryFont(dpy, font);
    }

  if (!f && !!strcmp (font, def2))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def2);
      font = def2;
      f = XLoadQueryFont(dpy, font);
    }

  if (!f)
    {
      fprintf (stderr, "%s: unable to load fallback font \"%s\" either!\n",
               progname, font);
      exit (1);
    }

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  

# ifndef HAVE_COCOA /* Xlib version */

  if (dlist_ret)
    {
      clear_gl_error ();
      *dlist_ret = glGenLists ((GLuint) last+1);
      check_gl_error ("glGenLists");
      glXUseXFont(id, first, last-first+1, *dlist_ret + first);
      check_gl_error ("glXUseXFont");
    }

# else  /* HAVE_COCOA */

  {
    int afid, face, size;
    afid = jwxyz_font_info (id, &size, &face);

    if (dlist_ret)
      {
        clear_gl_error ();
        *dlist_ret = glGenLists ((GLuint) last+1);
        check_gl_error ("glGenLists");

        AGLContext ctx = aglGetCurrentContext();
        if (! aglUseFont (ctx, afid, face, size, 
                          first, last-first+1, *dlist_ret + first)) {
          check_gl_error ("aglUseFont");
          abort();
      }
    }
  }

# endif  /* HAVE_COCOA */

  if (font_ret)
    *font_ret = f;
}


/* Width (and optionally height) of the string in pixels.
 */
int
string_width (XFontStruct *f, const char *c, int *height_ret)
{
  int x = 0;
  int max_w = 0;
  int h = f->ascent + f->descent;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      if (*c == '\n')
        {
          if (x > max_w) max_w = x;
          x = 0;
          h += f->ascent + f->descent;
        }
      else
        x += (f->per_char
              ? f->per_char[cc-f->min_char_or_byte2].width
              : f->min_bounds.rbearing);
      c++;
    }
  if (x > max_w) max_w = x;
  if (height_ret) *height_ret = h;

  return max_w;
}


/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any text inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_font().
 */
void
print_gl_string (Display *dpy,
                 XFontStruct *font,
                 GLuint font_dlist,
                 int window_width, int window_height,
                 GLfloat x, GLfloat y,
                 const char *string,
                 Bool clear_background_p)
{
  GLfloat line_height = font->ascent + font->descent;
  GLfloat sub_shift = (line_height * 0.3);
  int cw = string_width (font, "m", 0);
  int tabs = cw * 7;

  y -= line_height;

  /* Sadly, this causes a stall of the graphics pipeline (as would the
     equivalent calls to glGet*.)  But there's no way around this, short
     of having each caller set up the specific display matrix we need
     here, which would kind of defeat the purpose of centralizing this
     code in one file.
   */
  glPushAttrib (GL_TRANSFORM_BIT |  /* for matrix contents */
                GL_ENABLE_BIT |     /* for various glDisable calls */
                GL_CURRENT_BIT |    /* for glColor3f() */
                GL_LIST_BIT);       /* for glListBase() */
  {
# ifdef DEBUG
    check_gl_error ("glPushAttrib");
# endif

    /* disable lighting and texturing when drawing bitmaps!
       (glPopAttrib() restores these.)
     */
    glDisable (GL_TEXTURE_2D);
    glDisable (GL_LIGHTING);
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);

    /* glPopAttrib() does not restore matrix changes, so we must
       push/pop the matrix stacks to be non-intrusive there.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
# ifdef DEBUG
      check_gl_error ("glPushMatrix");
# endif
      glLoadIdentity();

      /* Each matrix mode has its own stack, so we need to push/pop
         them separately. */
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        unsigned int i;
        int x2 = x;
        Bool sub_p = False;

# ifdef DEBUG
        check_gl_error ("glPushMatrix");
# endif

        glLoadIdentity();
        gluOrtho2D (0, window_width, 0, window_height);
# ifdef DEBUG
        check_gl_error ("gluOrtho2D");
# endif

        if (clear_background_p)
          {
            int w, h;
            int lh = font->ascent + font->descent;
            w = string_width (font, string, &h);
            glColor3f (0, 0, 0);
            glRecti (x - font->descent,
                     y + lh, 
                     x + w + 2*font->descent,
                     y + lh - h - font->descent);
            glColor3f (1, 1, 1);
          }

        /* draw the text */
        glRasterPos2f (x, y);
/*        glListBase (font_dlist);*/
        for (i = 0; i < strlen(string); i++)
          {
            unsigned char c = (unsigned char) string[i];
            if (c == '\n')
              {
                glRasterPos2f (x, (y -= line_height));
                x2 = x;
              }
            else if (c == '\t')
              {
                x2 -= x;
                x2 = ((x2 + tabs) / tabs) * tabs;  /* tab to tab stop */
                x2 += x;
                glRasterPos2f (x2, y);
              }
            else if (c == '[' && (isdigit (string[i+1])))
              {
                sub_p = True;
                glRasterPos2f (x2, (y -= sub_shift));
              }
            else if (c == ']' && sub_p)
              {
                sub_p = False;
                glRasterPos2f (x2, (y += sub_shift));
              }
            else
              {
/*            glCallLists (s - string, GL_UNSIGNED_BYTE, string);*/
                glCallList (font_dlist + (int)(c));
                x2 += (font->per_char
                       ? font->per_char[c - font->min_char_or_byte2].width
                       : font->min_bounds.width);
              }
          }
# ifdef DEBUG
        check_gl_error ("print_gl_string");
# endif
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
  }
  glPopAttrib();
# ifdef DEBUG
  check_gl_error ("glPopAttrib");
# endif

  glMatrixMode(GL_MODELVIEW);
}
