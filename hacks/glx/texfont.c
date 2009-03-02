/* texfonts, Copyright (c) 2005 Jamie Zawinski <jwz@jwz.org>
 * Loads X11 fonts into textures for use with OpenGL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include "resources.h"
#include "texfont.h"

/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

/* screenhack.h */
extern char *progname;

struct texture_font_data {
  Display *dpy;
  XFontStruct *font;
  GLuint texid;
  int cell_width, cell_height;
  int tex_width, tex_height;
};


/* return the next larger power of 2. */
static int
to_pow2 (int i)
{
  static unsigned int pow2[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
                                 2048, 4096, 8192, 16384, 32768, 65536 };
  int j;
  for (j = 0; j < sizeof(pow2)/sizeof(*pow2); j++)
    if (pow2[j] >= i) return pow2[j];
  abort();  /* too big! */
}


/* Given a Pixmap of depth 1, converts it to an OpenGL luminance mipmap.
   The 1 bits are drawn, the 0 bits are alpha.
   Pass in the size of the pixmap; the size of the texture is returned
   (it may be larger, since GL like powers of 2.)
 */
static void
bitmap_to_texture (Display *dpy, Pixmap p, int *wP, int *hP)
{
  Bool mipmap_p = True;
  int ow = *wP;
  int oh = *hP;
  int w2 = to_pow2 (ow);
  int h2 = to_pow2 (oh);
  int x, y;
  XImage *image = XGetImage (dpy, p, 0, 0, ow, oh, ~0L, XYPixmap);
  unsigned char *data = (unsigned char *) calloc (w2, (h2 + 1));
  unsigned char *out = data;
  GLuint iformat = GL_INTENSITY;
  GLuint format = GL_LUMINANCE;
  GLuint type = GL_UNSIGNED_BYTE;

  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++)
      *out++ = (x >= ow || y >= oh ? 0 :
                XGetPixel (image, x, y) ? 255 : 0);
  XDestroyImage (image);
  image = 0;

  if (mipmap_p)
    gluBuild2DMipmaps (GL_TEXTURE_2D, iformat, w2, h2, format, type, data);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, iformat, w2, h2, 0, format, type, data);

  {
    char msg[100];
    sprintf (msg, "%s (%d x %d)",
             mipmap_p ? "gluBuild2DMipmaps" : "glTexImage2D",
             w2, h2);
    check_gl_error (msg);
  }


  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  free (data);

  *wP = w2;
  *hP = h2;
}


/* Loads the font named by the X resource "res" and returns
   a texture-font object.
*/
texture_font_data *
load_texture_font (Display *dpy, char *res)
{
  texture_font_data *data = 0;
  const char *font = get_string_resource (res, "Font");
  const char *def1 = "-*-times-bold-r-normal-*-240-*";
  const char *def2 = "-*-times-bold-r-normal-*-180-*";
  const char *def3 = "fixed";
  XFontStruct *f;

  if (!res || !*res) abort();
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

  if (!f && !!strcmp (font, def3))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def3);
      font = def3;
      f = XLoadQueryFont(dpy, font);
    }

  if (!f)
    {
      fprintf (stderr, "%s: unable to load fallback font \"%s\" either!\n",
               progname, font);
      exit (1);
    }

  data = (texture_font_data *) calloc (1, sizeof(*data));
  data->dpy = dpy;
  data->font = f;

  /* Create a pixmap big enough to fit every character in the font.
     Make it square-ish, since GL likes dimensions to be powers of 2.
   */
  {
    Screen *screen = DefaultScreenOfDisplay (dpy);
    Window root = RootWindowOfScreen (screen);
    XGCValues gcv;
    GC gc;
    Pixmap p;
    int cw = f->max_bounds.rbearing - f->min_bounds.lbearing;
    int ch = f->max_bounds.ascent   + f->max_bounds.descent;
    int w = cw * 16;
    int h = ch * 16;
    int i;

    data->cell_width  = cw;
    data->cell_height = ch;

    p = XCreatePixmap (dpy, root, w, h, 1);
    gcv.font = f->fid;
    gcv.foreground = 0;
    gcv.background = 0;
    gc = XCreateGC (dpy, p, (GCFont|GCForeground|GCBackground), &gcv);
    XFillRectangle (dpy, p, gc, 0, 0, w, h);
    XSetForeground (dpy, gc, 1);
    for (i = 0; i < 256; i++)
      {
        char c = (char) i;
        int x = (i % 16) * cw;
        int y = (i / 16) * ch;

        /* See comment in print_texture_string for bit layout explanation. */

        int lbearing = (f->per_char
                        ? f->per_char[i - f->min_char_or_byte2].lbearing
                        : f->min_bounds.lbearing);
        int ascent   = (f->per_char
                        ? f->per_char[i - f->min_char_or_byte2].ascent
                        : f->max_bounds.ascent);
        int width    = (f->per_char
                        ? f->per_char[i - f->min_char_or_byte2].width
                        : f->max_bounds.width);

        if (width == 0) continue;
        XDrawString (dpy, p, gc, x - lbearing, y + ascent, &c, 1);
      }
    XFreeGC (dpy, gc);

    glGenTextures (1, &data->texid);
    glBindTexture (GL_TEXTURE_2D, data->texid);
    data->tex_width  = w;
    data->tex_height = h;

#if 0  /* debugging: splat the bitmap onto the desktop root window */
    {
      Window win = RootWindow (dpy, 0);
      GC gc2 = XCreateGC (dpy, win, 0, &gcv);
      XSetForeground (dpy, gc2, BlackPixel (dpy, 0));
      XSetBackground (dpy, gc2, WhitePixel (dpy, 0));
      XCopyPlane (dpy, p, win, gc2, 0, 0, w, h, 0, 0, 1);
      XFreeGC (dpy, gc2);
      XSync(dpy, False);
    }
#endif

    bitmap_to_texture (dpy, p, &data->tex_width, &data->tex_height);
    XFreePixmap (dpy, p);
  }

  return data;
}


/* Width of the string in pixels.
 */
int
texture_string_width (texture_font_data *data, const char *c,
                      int *line_height_ret)
{
  int w = 0;
  XFontStruct *f = data->font;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      w += (f->per_char
            ? f->per_char[cc-f->min_char_or_byte2].width
            : f->max_bounds.width);
      c++;
    }
  if (line_height_ret)
    *line_height_ret = f->ascent + f->descent;
  return w;
}


/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
 */
void
print_texture_string (texture_font_data *data, const char *string)
{
  XFontStruct *f = data->font;
  GLfloat line_height = f->ascent + f->descent;
# ifdef DO_SUBSCRIPTS
  GLfloat sub_shift = (line_height * 0.3);
  Bool sub_p = False;
# endif /* DO_SUBSCRIPTS */
  int cw = texture_string_width (data, "m", 0);
  int tabs = cw * 7;
  int x, y;
  unsigned int i;

  glPushMatrix();

  glBindTexture (GL_TEXTURE_2D, data->texid);
  glNormal3f (0, 0, 1);

  x = 0;
  y = 0;
  for (i = 0; i < strlen(string); i++)
    {
      char c = string[i];
      if (c == '\n')
        {
          y -= line_height;
          x = 0;
        }
      else if (c == '\t')
        {
          x = ((x + tabs) / tabs) * tabs;  /* tab to tab stop */
        }
# ifdef DO_SUBSCRIPTS
      else if (c == '[' && (isdigit (string[i+1])))
        {
          sub_p = True;
          y -= sub_shift;
        }
      else if (c == ']' && sub_p)
        {
          sub_p = False;
          y += sub_shift;
        }
# endif /* DO_SUBSCRIPTS */
      else
        {
          /* The texture is divided into 16x16 rectangles whose size are
             the max_bounds charcell of the font.  Within each rectangle,
             the individual characters' charcells sit in the upper left.

               [A]----------------------------
                |     |           |   |      |
                |   l |         w |   | r    |
                |   b |         i |   | b    |
                |   e |         d |   | e    |
                |   a |         t |   | a    |
                |   r |         h |   | r    |
                |   i |           |   | i    |
                |   n |           |   | n    |
                |   g |           |   | g    |
                |     |           |   |      |
                |----[B]----------|---|      |
                |     |   ascent  |   |      |
                |     |           |   |      |
                |     |           |   |      |
                |--------------------[C]     |
                |         descent            |
                |                            | cell_width,
                ------------------------------ cell_height

             We want to make a quad from point A to point C.
             We want to position that quad so that point B lies at x,y.
           */
          int lbearing = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].lbearing
                          : f->min_bounds.lbearing);
          int rbearing = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].rbearing
                          : f->max_bounds.rbearing);
          int ascent   = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].ascent
                          : f->max_bounds.ascent);
          int descent  = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].descent
                          : f->max_bounds.descent);
          int cwidth   = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].width
                          : f->max_bounds.width);

          int ax = ((int) c % 16) * data->cell_width;     /* point A */
          int ay = ((int) c / 16) * data->cell_height;

          int bx = ax - lbearing;                         /* point B */
          int by = ay + ascent;

          int cx = bx + rbearing;                         /* point C */
          int cy = by + descent;

          GLfloat tax = (GLfloat) ax / data->tex_width;  /* tex coords of A */
          GLfloat tay = (GLfloat) ay / data->tex_height;

          GLfloat tcx = (GLfloat) cx / data->tex_width;  /* tex coords of C */
          GLfloat tcy = (GLfloat) cy / data->tex_height;

          GLfloat qx0 = x + lbearing;			 /* quad top left */
          GLfloat qy0 = y + ascent;
          GLfloat qx1 = qx0 + rbearing - lbearing;       /* quad bot right */
          GLfloat qy1 = qy0 - (ascent + descent);

          if (cwidth > 0 && c != ' ')
            {
              glBegin (GL_QUADS);
              glTexCoord2f (tax, tay); glVertex3f (qx0, qy0, 0);
              glTexCoord2f (tcx, tay); glVertex3f (qx1, qy0, 0);
              glTexCoord2f (tcx, tcy); glVertex3f (qx1, qy1, 0);
              glTexCoord2f (tax, tcy); glVertex3f (qx0, qy1, 0);
              glEnd();
#if 0
              glDisable(GL_TEXTURE_2D);
              glBegin (GL_LINE_LOOP);
              glTexCoord2f (tax, tay); glVertex3f (qx0, qy0, 0);
              glTexCoord2f (tcx, tay); glVertex3f (qx1, qy0, 0);
              glTexCoord2f (tcx, tcy); glVertex3f (qx1, qy1, 0);
              glTexCoord2f (tax, tcy); glVertex3f (qx0, qy1, 0);
              glEnd();
              glEnable(GL_TEXTURE_2D);
#endif
            }

          x += cwidth;
        }
      }
  glPopMatrix();
}

/* Releases the font and texture.
 */
void
free_texture_font (texture_font_data *data)
{
  if (data->font)
    XFreeFont (data->dpy, data->font);
  if (data->texid)
    glDeleteTextures (1, &data->texid);
  free (data);
}
