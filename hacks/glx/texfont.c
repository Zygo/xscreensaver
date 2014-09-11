/* texfonts, Copyright (c) 2005-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Renders X11 fonts into textures for use with OpenGL.
 * A higher level API is in glxfonts.c.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_COCOA
# ifdef USE_IPHONE
#  include "jwzgles.h"
# else
#  include <OpenGL/glu.h>
# endif
#else
# include <GL/glx.h>
# include <GL/glu.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "resources.h"
#include "texfont.h"

#define DO_SUBSCRIPTS


/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

/* screenhack.h */
extern char *progname;

struct texture_font_data {
  Display *dpy;
  XFontStruct *font;
  int cell_width, cell_height;  /* maximal charcell */
  int tex_width, tex_height;    /* size of each texture */

  int grid_mag;			/* 1,  2,  4, or 8 */
  int ntextures;		/* 1,  4, 16, or 64 (grid_mag ^ 2) */

  GLuint texid[64];		/* must hold ntextures */
};


/* return the next larger power of 2. */
static int
to_pow2 (int i)
{
  static const unsigned int pow2[] = { 
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 
    2048, 4096, 8192, 16384, 32768, 65536 };
  int j;
  for (j = 0; j < sizeof(pow2)/sizeof(*pow2); j++)
    if (pow2[j] >= i) return pow2[j];
  abort();  /* too big! */
}


/* Given a Pixmap (of screen depth), converts it to an OpenGL luminance mipmap.
   RGB are averaged to grayscale, and the resulting value is treated as alpha.
   Pass in the size of the pixmap; the size of the texture is returned
   (it may be larger, since GL like powers of 2.)

   We use a screen-depth pixmap instead of a 1bpp bitmap so that if the fonts
   were drawn with antialiasing, that is preserved.
 */
static void
bitmap_to_texture (Display *dpy, Pixmap p, Visual *visual, int *wP, int *hP)
{
  Bool mipmap_p = True;
  int ow = *wP;
  int oh = *hP;
  int w2 = to_pow2 (ow);
  int h2 = to_pow2 (oh);
  int x, y;
  XImage *image = XGetImage (dpy, p, 0, 0, ow, oh, ~0L, ZPixmap);
  unsigned char *data = (unsigned char *) calloc (w2 * 2, (h2 + 1));
  unsigned char *out = data;

  /* OpenGLES doesn't support GL_INTENSITY, so instead of using a
     texture with 1 byte per pixel, the intensity value, we have
     to use 2 bytes per pixel: solid white, and an alpha value.
   */
# ifdef HAVE_JWZGLES
#  undef GL_INTENSITY
# endif

# ifdef GL_INTENSITY
  GLuint iformat = GL_INTENSITY;
  GLuint format  = GL_LUMINANCE;
# else
  GLuint iformat = GL_LUMINANCE_ALPHA;
  GLuint format  = GL_LUMINANCE_ALPHA;
# endif
  GLuint type    = GL_UNSIGNED_BYTE;

# ifdef HAVE_JWZGLES
  /* This would work, but it's wasteful for no benefit. */
  mipmap_p = False;
# endif

  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++) {
      unsigned long pixel = (x >= ow || y >= oh ? 0 : XGetPixel (image, x, y));
      /* instead of averaging all three channels, let's just use red,
         and assume it was already grayscale. */
      unsigned long r = pixel & visual->red_mask;
      /* This goofy trick is to make any of RGBA/ABGR/ARGB work. */
      pixel = ((r >> 24) | (r >> 16) | (r >> 8) | r) & 0xFF;
# ifndef GL_INTENSITY
      *out++ = 0xFF;  /* 2 bytes per pixel */
# endif
      *out++ = pixel;
    }
  XDestroyImage (image);
  image = 0;

  if (mipmap_p)
    gluBuild2DMipmaps (GL_TEXTURE_2D, iformat, w2, h2, format, type, data);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, iformat, w2, h2, 0, format, type, data);

  {
    char msg[100];
    sprintf (msg, "texture font %s (%d x %d)",
             mipmap_p ? "gluBuild2DMipmaps" : "glTexImage2D",
             w2, h2);
    check_gl_error (msg);
  }


  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);


  /* This makes scaled font pixmaps tolerable to look at.
     LOD bias is part of OpenGL 1.4.
     GL_EXT_texture_lod_bias has been present since the original iPhone.
   */
# if !defined(GL_TEXTURE_LOD_BIAS) && defined(GL_TEXTURE_LOD_BIAS_EXT)
#   define GL_TEXTURE_LOD_BIAS GL_TEXTURE_LOD_BIAS_EXT
# endif
# ifdef GL_TEXTURE_LOD_BIAS
  if (mipmap_p)
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.25);
# endif
  clear_gl_error();  /* invalid enum on iPad 3 */

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  free (data);

  *wP = w2;
  *hP = h2;
}


static texture_font_data *
load_texture_xfont (Display *dpy, XFontStruct *f)
{
  Screen *screen = DefaultScreenOfDisplay (dpy);
  Window root = RootWindowOfScreen (screen);
  XWindowAttributes xgwa;
  int which;
  GLint old_texture = 0;
  texture_font_data *data = 0;

  glGetIntegerv (GL_TEXTURE_BINDING_2D, &old_texture);

  XGetWindowAttributes (dpy, root, &xgwa);

  data = (texture_font_data *) calloc (1, sizeof(*data));
  data->dpy = dpy;
  data->font = f;

  /* Figure out how many textures to use.
     E.g., if we need 1024x1024 bits, use four 512x512 textures,
     to be gentle to machines with low texture size limits.
   */
  {
    int w = to_pow2 (16 * (f->max_bounds.rbearing - f->min_bounds.lbearing));
    int h = to_pow2 (16 * (f->max_bounds.ascent   + f->max_bounds.descent));
    int i = (w > h ? w : h);

    if      (i <= 512)  data->grid_mag = 1;  /*  1 tex of 16x16 chars */
    else if (i <= 1024) data->grid_mag = 2;  /*  4 tex of 8x8 chars */
    else if (i <= 2048) data->grid_mag = 4;  /* 16 tex of 4x4 chars */
    else                data->grid_mag = 8;  /* 32 tex of 2x2 chars */

    data->ntextures = data->grid_mag * data->grid_mag;

# if 0
    fprintf (stderr,
             "%s: %dx%d grid of %d textures of %dx%d chars (%dx%d bits)\n",
             progname,
             data->grid_mag, data->grid_mag,
             data->ntextures,
             16 / data->grid_mag, 16 / data->grid_mag,
             i, i);
# endif
  }

  for (which = 0; which < data->ntextures; which++)
    {
      /* Create a pixmap big enough to fit every character in the font.
         (modulo the "ntextures" scaling.)
         Make it square-ish, since GL likes dimensions to be powers of 2.
       */
      XGCValues gcv;
      GC gc;
      Pixmap p;
      int cw = f->max_bounds.rbearing - f->min_bounds.lbearing;
      int ch = f->max_bounds.ascent   + f->max_bounds.descent;
      int grid_size = (16 / data->grid_mag);
      int w = cw * grid_size;
      int h = ch * grid_size;
      int i;

      data->cell_width  = cw;
      data->cell_height = ch;

      p = XCreatePixmap (dpy, root, w, h, xgwa.depth);
      gcv.font = f->fid;
      gcv.foreground = BlackPixelOfScreen (xgwa.screen);
      gcv.background = BlackPixelOfScreen (xgwa.screen);
      gc = XCreateGC (dpy, p, (GCFont|GCForeground|GCBackground), &gcv);
      XFillRectangle (dpy, p, gc, 0, 0, w, h);
      XSetForeground (dpy, gc, WhitePixelOfScreen (xgwa.screen));
      for (i = 0; i < 256 / data->ntextures; i++)
        {
          int ii = (i + (which * 256 / data->ntextures));
          char c = (char) ii;
          int x = (i % grid_size) * cw;
          int y = (i / grid_size) * ch;

          /* See comment in print_texture_string for bit layout explanation.
           */
          int lbearing = (f->per_char && ii >= f->min_char_or_byte2
                          ? f->per_char[ii - f->min_char_or_byte2].lbearing
                          : f->min_bounds.lbearing);
          int ascent   = (f->per_char && ii >= f->min_char_or_byte2
                          ? f->per_char[ii - f->min_char_or_byte2].ascent
                          : f->max_bounds.ascent);
          int width    = (f->per_char && ii >= f->min_char_or_byte2
                          ? f->per_char[ii - f->min_char_or_byte2].width
                          : f->max_bounds.width);

          if (width == 0) continue;
          XDrawString (dpy, p, gc, x - lbearing, y + ascent, &c, 1);
        }
      XFreeGC (dpy, gc);

      glGenTextures (1, &data->texid[which]);
      glBindTexture (GL_TEXTURE_2D, data->texid[which]);
      check_gl_error ("texture font load");
      data->tex_width  = w;
      data->tex_height = h;

#if 0  /* debugging: write the bitmap to a pgm file */
      {
        char file[255];
        XImage *image;
        int x, y;
        FILE *ff;
        sprintf (file, "/tmp/%02d.pgm", which);
        image = XGetImage (dpy, p, 0, 0, w, h, ~0L, ZPixmap);
        ff = fopen (file, "w");
        fprintf (ff, "P5\n%d %d\n255\n", w, h);
        for (y = 0; y < h; y++)
          for (x = 0; x < w; x++) {
            unsigned long pix = XGetPixel (image, x, y);
            unsigned long r = (pix & xgwa.visual->red_mask);
            r = ((r >> 24) | (r >> 16) | (r >> 8) | r);
            fprintf (ff, "%c", (char) r);
          }
        fclose (ff);
        XDestroyImage (image);
        fprintf (stderr, "%s: wrote %s (%d x %d)\n", progname, file,
                 f->max_bounds.rbearing - f->min_bounds.lbearing,
                 f->max_bounds.ascent   + f->max_bounds.descent);
      }
#endif /* 0 */

      bitmap_to_texture (dpy, p, xgwa.visual, 
                         &data->tex_width, &data->tex_height);
      XFreePixmap (dpy, p);
    }

  /* Reset to the caller's default */
  glBindTexture (GL_TEXTURE_2D, old_texture);

  return data;
}


/* Loads the font named by the X resource "res" and returns
   a texture-font object.
*/
texture_font_data *
load_texture_font (Display *dpy, char *res)
{
  char *font = get_string_resource (dpy, res, "Font");
  const char *def1 = "-*-helvetica-medium-r-normal-*-240-*";
  const char *def2 = "-*-helvetica-medium-r-normal-*-180-*";
  const char *def3 = "fixed";
  XFontStruct *f;

  if (!strcmp (res, "fpsFont"))
    def1 = "-*-courier-bold-r-normal-*-180-*";  /* Kludge. Sue me. */

  if (!res || !*res) abort();
  if (!font) font = strdup(def1);

  f = XLoadQueryFont(dpy, font);
  if (!f && !!strcmp (font, def1))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def1);
      free (font);
      font = strdup (def1);
      f = XLoadQueryFont(dpy, font);
    }

  if (!f && !!strcmp (font, def2))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def2);
      free (font);
      font = strdup (def2);
      f = XLoadQueryFont(dpy, font);
    }

  if (!f && !!strcmp (font, def3))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def3);
      free (font);
      font = strdup (def3);
      f = XLoadQueryFont(dpy, font);
    }

  if (!f)
    {
      fprintf (stderr, "%s: unable to load fallback font \"%s\" either!\n",
               progname, font);
      exit (1);
    }

  free (font);
  font = 0;

  return load_texture_xfont (dpy, f);
}


/* Bounding box of the multi-line string, in pixels.
 */
int
texture_string_width (texture_font_data *data, const char *c, int *height_ret)
{
  XFontStruct *f = data->font;
  int x = 0;
  int max_w = 0;
  int lh = f->ascent + f->descent;
  int h = lh;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      if (*c == '\n')
        {
          if (x > max_w) max_w = x;
          x = 0;
          h += lh;
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


/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().
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
  GLint old_texture = 0;
  GLfloat omatrix[16];
  int ofront;

  glGetIntegerv (GL_TEXTURE_BINDING_2D, &old_texture);
  glGetIntegerv (GL_FRONT_FACE, &ofront);
  glGetFloatv (GL_TEXTURE_MATRIX, omatrix);

  clear_gl_error ();

  glPushMatrix();

  glNormal3f (0, 0, 1);
  glFrontFace (GL_CW);

  glMatrixMode (GL_TEXTURE);
  glLoadIdentity ();
  glMatrixMode (GL_MODELVIEW);

  x = 0;
  y = 0;
  for (i = 0; i < strlen(string); i++)
    {
      unsigned char c = string[i];
      if (c == '\n')
        {
          y -= line_height;
          x = 0;
        }
      else if (c == '\t')
        {
          if (tabs)
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
          /* For a small font, the texture is divided into 16x16 rectangles
             whose size are the max_bounds charcell of the font.  Within each
             rectangle, the individual characters' charcells sit in the upper
             left.

             For a larger font, the texture will itself be subdivided, to
             keep the texture sizes small (in that case we deal with, e.g.,
             4 grids of 8x8 characters instead of 1 grid of 16x16.)

             Within each texture:

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
          int lbearing = (f->per_char && c >= f->min_char_or_byte2
                          ? f->per_char[c - f->min_char_or_byte2].lbearing
                          : f->min_bounds.lbearing);
          int rbearing = (f->per_char && c >= f->min_char_or_byte2
                          ? f->per_char[c - f->min_char_or_byte2].rbearing
                          : f->max_bounds.rbearing);
          int ascent   = (f->per_char && c >= f->min_char_or_byte2
                          ? f->per_char[c - f->min_char_or_byte2].ascent
                          : f->max_bounds.ascent);
          int descent  = (f->per_char && c >= f->min_char_or_byte2
                          ? f->per_char[c - f->min_char_or_byte2].descent
                          : f->max_bounds.descent);
          int cwidth   = (f->per_char && c >= f->min_char_or_byte2
                          ? f->per_char[c - f->min_char_or_byte2].width
                          : f->max_bounds.width);

          unsigned char cc = c % (256 / data->ntextures);

          int gs = (16 / data->grid_mag);		  /* grid size */

          int ax = ((int) cc % gs) * data->cell_width;    /* point A */
          int ay = ((int) cc / gs) * data->cell_height;

          int bx = ax - lbearing;                         /* point B */
          int by = ay + ascent;

          int cx = bx + rbearing + 1;                     /* point C */
          int cy = by + descent  + 1;

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
              int which = c / (256 / data->ntextures);
              if (which >= data->ntextures) abort();
              glBindTexture (GL_TEXTURE_2D, data->texid[which]);

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

  /* Reset to the caller's default */
  glBindTexture (GL_TEXTURE_2D, old_texture);
  glFrontFace (ofront);
  
  glMatrixMode (GL_TEXTURE);
  glMultMatrixf (omatrix);
  glMatrixMode (GL_MODELVIEW);

  check_gl_error ("texture font print");
}

/* Releases the font and texture.
 */
void
free_texture_font (texture_font_data *data)
{
  int i;

  for (i = 0; i < data->ntextures; i++)
    if (data->texid[i])
      glDeleteTextures (1, &data->texid[i]);

  if (data->font)
    XFreeFont (data->dpy, data->font);

  free (data);
}
