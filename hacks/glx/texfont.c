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
#elif defined(HAVE_ANDROID)
# include <GLES/gl.h>
# include "jwzgles.h"
#else
# include <GL/glx.h>
# include <GL/glu.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */

#include "xft.h"
#include "resources.h"
#include "texfont.h"
#include "fps.h"	/* for current_device_rotation() */

#undef HAVE_XSHM_EXTENSION  /* doesn't actually do any good here */


/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

/* screenhack.h */
extern char *progname;

/* LRU cache of textures, to optimize the case where we're drawing the
   same strings repeatedly.
 */
typedef struct texfont_cache texfont_cache;
struct texfont_cache {
  char *string;
  GLuint texid;
  int width, height;
  int width2, height2;
  texfont_cache *next;
};

struct texture_font_data {
  Display *dpy;
  XftFont *xftfont;
  int cache_size;
  texfont_cache *cache;
};


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


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
bitmap_to_texture (Display *dpy, Pixmap p, Visual *visual, int depth,
                   int *wP, int *hP)
{
  Bool mipmap_p = True;
  int ow = *wP;
  int oh = *hP;
  int w2 = to_pow2 (ow);
  int h2 = to_pow2 (oh);
  int x, y, max, scale;
  XImage *image = 0;
  unsigned char *data = (unsigned char *) calloc (w2 * 2, (h2 + 1));
  unsigned char *out = data;

  /* If either dimension is larger than the supported size, reduce.
     We still return the old size to keep the caller's math working,
     but the texture itself will have fewer pixels in it.
   */
  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max);
  scale = 1;
  while (w2 > max || h2 > max)
    {
      w2 /= 2;
      h2 /= 2;
      scale *= 2;
    }

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

# ifdef HAVE_XSHM_EXTENSION
  Bool use_shm = get_boolean_resource (dpy, "useSHM", "Boolean");
  XShmSegmentInfo shm_info;
# endif /* HAVE_XSHM_EXTENSION */

# ifdef HAVE_XSHM_EXTENSION
# error XX
  if (use_shm)
    {
      image = create_xshm_image (dpy, visual, depth, ZPixmap, 0, &shm_info,
                                 ow, oh);
      if (image)
        XShmGetImage (dpy, p, image, 0, 0, ~0L);
      else
        use_shm = False;
    }
# endif /* HAVE_XSHM_EXTENSION */

  if (!image)
    image = XGetImage (dpy, p, 0, 0, ow, oh, ~0L, ZPixmap);

# ifdef HAVE_JWZGLES
  /* This would work, but it's wasteful for no benefit. */
  mipmap_p = False;
# endif

  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++) {
      /* Might be better to average a scale x scale square of source pixels,
         but at the resolutions we're dealing with, this is probably good
         enough. */
      int sx = x * scale;
      int sy = y * scale;
      unsigned long pixel = (sx >= ow || sy >= oh ? 0 :
                             XGetPixel (image, sx, sy));
      /* instead of averaging all three channels, let's just use red,
         and assume it was already grayscale. */
      unsigned long r = pixel & visual->red_mask;
      /* This goofy trick is to make any of RGBA/ABGR/ARGB work. */
      pixel = ((r >> 24) | (r >> 16) | (r >> 8) | r) & 0xFF;
# ifndef GL_INTENSITY
      *out++ = 0xFF;  /* 2 bytes per pixel (luminance, alpha) */
# endif
      *out++ = pixel;
    }

# ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    destroy_xshm_image (dpy, image, &shm_info);
  else
# endif /* HAVE_XSHM_EXTENSION */
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

  free (data);

  *wP = w2 * scale;
  *hP = h2 * scale;
}


/* Loads the font named by the X resource "res" and returns
   a texture-font object.
*/
texture_font_data *
load_texture_font (Display *dpy, char *res)
{
  int screen = DefaultScreen (dpy);
  char *font = get_string_resource (dpy, res, "Font");
  const char *def1 = "-*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*";
  const char *def2 = "-*-helvetica-medium-r-normal-*-*-140-*-*-*-*-*-*";
  const char *def3 = "fixed";
  XftFont *f = 0;
  texture_font_data *data;
  int cache_size = get_integer_resource (dpy, "texFontCacheSize", "Integer");

  /* Hacks that draw a lot of different strings on the screen simultaneously,
     like Star Wars, should set this to a larger value for performance. */
  if (cache_size <= 0)
    cache_size = 30;

  if (!res || !*res) abort();

  if (!strcmp (res, "fpsFont")) {  /* Kludge. */
    def1 = "-*-courier-bold-r-normal-*-*-140-*-*-*-*-*-*";
    cache_size = 0;  /* No need for a cache on FPS: already throttled. */
  }

  if (!font) font = strdup(def1);

  f = XftFontOpenXlfd (dpy, screen, font);
  if (!f && !!strcmp (font, def1))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def1);
      free (font);
      font = strdup (def1);
      f = XftFontOpenXlfd (dpy, screen, font);
    }

  if (!f && !!strcmp (font, def2))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def2);
      free (font);
      font = strdup (def2);
      f = XftFontOpenXlfd (dpy, screen, font);
    }

  if (!f && !!strcmp (font, def3))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def3);
      free (font);
      font = strdup (def3);
      f = XftFontOpenXlfd (dpy, screen, font);
    }

  if (!f)
    {
      fprintf (stderr, "%s: unable to load fallback font \"%s\" either!\n",
               progname, font);
      abort();
    }

  free (font);
  font = 0;

  data = (texture_font_data *) calloc (1, sizeof(*data));
  data->dpy = dpy;
  data->xftfont = f;
  data->cache_size = cache_size;

  return data;
}


/* Measure the string, or render it, depending on whether the XftDraw
   is supplied.
 */
static int
iterate_texture_string (texture_font_data *data,
                        const char *s, 
                        XftDraw *xftdraw, XftColor *xftcolor,
                        int x, int y,
                        int *height_ret)
{
  int line_height = data->xftfont->ascent + data->xftfont->descent;
  int left = x;
  int max_x, ox, oy;
  const char *os = s;
  Bool sub_p = False, osub_p = False;
  int cw = 0, tabs = 0;
  XGlyphInfo extents;

  y += line_height;
  max_x = x;
  ox = x;
  oy = y;

  while (1)
    {
      if (*s == 0 ||
          *s == '\n' ||
          *s == '\t' ||
          (*s == '[' && isdigit(s[1])) ||
          (*s == ']' && sub_p))
        {
          if (s == os)
            extents.xOff = 0;
          else
            XftTextExtentsUtf8 (data->dpy, data->xftfont,
                                (FcChar8 *) os, (int) (s - os),
                                &extents);
          x += extents.xOff;
          if (x > max_x)
            max_x = x;

          if (*s == '\n')
            {
              x = left;
              y += line_height;
              sub_p = False;
            }
          else if (*s == '\t')
            {
              if (! cw)
                {
                  /* Measure "m" to determine tab width. */
                  XftTextExtentsUtf8 (data->dpy, data->xftfont,
                                      (FcChar8 *) "m", 1, &extents);
                  cw = extents.xOff;
                  if (cw <= 0) cw = 1;
                  tabs = cw * 7;
                }
              x = ((x + tabs) / tabs) * tabs;
            }
          else if (*s == '[' && isdigit(s[1]))
            sub_p = True;
          else if (*s == ']' && sub_p)
            sub_p = False;

          if (xftdraw && s != os)
            XftDrawStringUtf8 (xftdraw, xftcolor, data->xftfont,
                               ox, 
                               oy + (int) (osub_p ? line_height * 0.3 : 0),
                               (FcChar8 *) os, (int) (s - os));
          if (!*s) break;
          os = s+1;
          ox = x;
          oy = y;
          osub_p = sub_p;
        }
      s++;
    }

  if (height_ret)
    *height_ret = y;
  return max_x;
}


/* Bounding box of the multi-line string, in pixels.
 */
int
texture_string_width (texture_font_data *data, const char *s, int *height_ret)
{
  return iterate_texture_string (data, s, 0, 0, 0, 0, height_ret);
}


static struct texfont_cache *
get_cache (texture_font_data *data, const char *string)
{
  int count = 0;
  texfont_cache *prev = 0, *prev2 = 0, *curr = 0, *next = 0;

  if (data->cache)
    for (prev2 = 0, prev = 0, curr = data->cache, next = curr->next;
         curr;
         prev2 = prev, prev = curr, curr = next,
           next = (curr ? curr->next : 0), count++)
      {
        if (!strcmp (string, curr->string))
          {
            if (prev)
              prev->next = next;       /* Unlink from list */
            if (curr != data->cache)
              {
                curr->next = data->cache;  /* Move to front */
                data->cache = curr;
              }
            return curr;
          }
      }

  /* Made it to the end of the list without a hit.
     If the cache is full, empty out the last one on the list,
     and move it to the front.  Keep the texid.
   */
  if (count > data->cache_size)
    {
      free (prev->string);
      prev->string  = 0;
      prev->width   = 0;
      prev->height  = 0;
      prev->width2  = 0;
      prev->height2 = 0;
      if (prev2)
        prev2->next = 0;
      if (prev != data->cache)
        prev->next = data->cache;
      data->cache = prev;
      return prev;
    }

  /* Not cached, and cache not full.  Add a new entry at the front,
     and allocate a new texture for it.
   */
  curr = (struct texfont_cache *) calloc (1, sizeof(*prev));
  glGenTextures (1, &curr->texid);
  curr->string = 0;
  curr->next = data->cache;
  data->cache = curr;

  return curr;
}


/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().
 */
void
print_texture_string (texture_font_data *data, const char *string)
{
  int line_height = data->xftfont->ascent + data->xftfont->descent;
  int margin = line_height * 0.35;
  int width, height;
  int width2, height2;
  XWindowAttributes xgwa;
  Pixmap p = 0;
  texfont_cache *cache;

  if (!*string) return;

  cache = get_cache (data, string);

  /* Measure the string and make a pixmap that will fit it,
     unless it's cached.
   */
  if (cache->string)
    {
      width   = data->cache->width;
      height  = data->cache->height;
    }
  else
    {
      Window window = RootWindow (data->dpy, 0);
      XGCValues gcv;
      GC gc;

      XGetWindowAttributes (data->dpy, window, &xgwa);
      width = iterate_texture_string (data, string, 0, 0, 0, 0, &height);
      p = XCreatePixmap (data->dpy, window, 
                         width  + margin*2,
                         height + margin*2,
                         xgwa.depth);
      gcv.foreground = BlackPixelOfScreen (xgwa.screen);
      gc = XCreateGC (data->dpy, p, GCForeground, &gcv);
      XFillRectangle (data->dpy, p, gc, 0, 0, 
                      width  + margin*2,
                      height + margin*2);
      XFreeGC (data->dpy, gc);
    }

  /* Draw the string into the pixmap, unless it's cached.
   */
  if (!cache->string)
    {
      XRenderColor rcolor;
      XftColor xftcolor;
      XftDraw *xftdraw;
      rcolor.red = rcolor.green = rcolor.blue = rcolor.alpha = 0xFFFF;
      XftColorAllocValue (data->dpy, xgwa.visual, xgwa.colormap,
                          &rcolor, &xftcolor);
      xftdraw = XftDrawCreate (data->dpy, p, xgwa.visual, xgwa.colormap);
      iterate_texture_string (data, string, xftdraw, &xftcolor,
                              margin, 0, 0);
      XftDrawDestroy (xftdraw);
      XftColorFree (data->dpy, xgwa.visual, xgwa.colormap, &xftcolor);
    }

  {
    GLint old_texture;
    int ofront, oblend;
    Bool alpha_p, blend_p;
    GLfloat omatrix[16];
    GLfloat qx0, qy0, qx1, qy1;
    GLfloat tx0, ty0, tx1, ty1;

    /* Save the prevailing texture environment, and set up ours.
     */
    glGetIntegerv (GL_TEXTURE_BINDING_2D, &old_texture);
    glGetIntegerv (GL_FRONT_FACE, &ofront);
    glGetIntegerv (GL_BLEND_DST, &oblend);
    glGetFloatv (GL_TEXTURE_MATRIX, omatrix);
    blend_p = glIsEnabled (GL_BLEND);
    alpha_p = glIsEnabled (GL_ALPHA_TEST);

    clear_gl_error ();

    glPushMatrix();

    glNormal3f (0, 0, 1);
    glFrontFace (GL_CW);

    glMatrixMode (GL_TEXTURE);
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);

    glBindTexture (GL_TEXTURE_2D, cache->texid);
    check_gl_error ("texture font binding");

    glEnable(GL_TEXTURE_2D);

    /* Copy the bits from the Pixmap into a texture, unless it's cached.
     */
    if (cache->string)
      {
        width2  = data->cache->width2;
        height2 = data->cache->height2;
        if (p) abort();
      }
    else
      {
        width2  = width  + margin*2;
        height2 = height + margin*2;
        bitmap_to_texture (data->dpy, p, xgwa.visual, xgwa.depth,
                           &width2, &height2);
        XFreePixmap (data->dpy, p);
      }


    /* Texture-rendering parameters to make font pixmaps tolerable to look at.
     */

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     GL_LINEAR_MIPMAP_LINEAR);

    /* LOD bias is part of OpenGL 1.4.
       GL_EXT_texture_lod_bias has been present since the original iPhone.
     */
# if !defined(GL_TEXTURE_LOD_BIAS) && defined(GL_TEXTURE_LOD_BIAS_EXT)
#   define GL_TEXTURE_LOD_BIAS GL_TEXTURE_LOD_BIAS_EXT
# endif
# ifdef GL_TEXTURE_LOD_BIAS
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.25);
# endif
    clear_gl_error();  /* invalid enum on iPad 3 */

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    /* Don't write the transparent parts of the quad into the depth buffer. */
    glAlphaFunc (GL_GREATER, 0.01);
    glEnable (GL_ALPHA_TEST);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Draw a quad with that texture on it, possibly using a cached texture.
     */
    qx0 = -margin;
    qy0 = line_height + margin;
    qx1 = width + margin;
    qy1 = line_height - height - margin;

    tx0 = 0;
    ty0 = 0;
    tx1 = (width  + margin*2) / (GLfloat) width2;
    ty1 = (height + margin*2) / (GLfloat) height2;

    glBegin (GL_QUADS);
    glTexCoord2f (tx0, ty0); glVertex3f (qx0, qy0, 0);
    glTexCoord2f (tx1, ty0); glVertex3f (qx1, qy0, 0);
    glTexCoord2f (tx1, ty1); glVertex3f (qx1, qy1, 0);
    glTexCoord2f (tx0, ty1); glVertex3f (qx0, qy1, 0);
    glEnd();

    glPopMatrix();

    /* Reset to the caller's texture environment.
     */
    glBindTexture (GL_TEXTURE_2D, old_texture);
    glFrontFace (ofront);
    if (!alpha_p) glDisable (GL_ALPHA_TEST);
    if (!blend_p) glDisable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, oblend);
  
    glMatrixMode (GL_TEXTURE);
    glMultMatrixf (omatrix);
    glMatrixMode (GL_MODELVIEW);

    check_gl_error ("texture font print");

    /* Store this string into the cache, unless that's where it came from.
     */
    if (!cache->string)
      {
        cache->string  = strdup (string);
        cache->width   = width;
        cache->height  = height;
        cache->width2  = width2;
        cache->height2 = height2;
      }
  }
}


/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().

   Position is 0 for center, 1 for top left, 2 for bottom left.
 */
void
print_texture_label (Display *dpy,
                     texture_font_data *data,
                     int window_width, int window_height,
                     int position,
                     const char *string)
{
  GLfloat color[4];

  Bool tex_p   = glIsEnabled (GL_TEXTURE_2D);
  Bool texs_p  = glIsEnabled (GL_TEXTURE_GEN_S);
  Bool text_p  = glIsEnabled (GL_TEXTURE_GEN_T);
  Bool light_p = glIsEnabled (GL_LIGHTING);
  Bool depth_p = glIsEnabled (GL_DEPTH_TEST);
  Bool cull_p  = glIsEnabled (GL_CULL_FACE);
  Bool fog_p   = glIsEnabled (GL_FOG);
  GLint ovp[4];

#  ifndef HAVE_JWZGLES
  GLint opoly[2];
  glGetIntegerv (GL_POLYGON_MODE, opoly);
#  endif

  glGetIntegerv (GL_VIEWPORT, ovp);

  glGetFloatv (GL_CURRENT_COLOR, color);

  glEnable (GL_TEXTURE_2D);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPolygonMode (GL_FRONT, GL_FILL);

  glDisable (GL_TEXTURE_GEN_S);
  glDisable (GL_TEXTURE_GEN_T);
  glDisable (GL_LIGHTING);
  glDisable (GL_CULL_FACE);
  glDisable (GL_FOG);

  glDisable (GL_DEPTH_TEST);

  /* Each matrix mode has its own stack, so we need to push/pop
     them separately.
   */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  {
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    {
      int x, y, w, h, lh, swap;
      int rot = (int) current_device_rotation();

      glLoadIdentity();
      glViewport (0, 0, window_width, window_height);
      glOrtho (0, window_width, 0, window_height, -1, 1);

      while (rot <= -180) rot += 360;
      while (rot >   180) rot -= 360;

      lh = texture_string_width (data, "M", 0);
      w = texture_string_width (data, string, &h);

# ifdef USE_IPHONE
      /* Size of the font is in points, so scale iOS pixels to points. */
      {
        GLfloat scale = 1;
        scale = window_width / 768.0;
        if (scale < 1) scale = 1;

        /* jwxyz-XLoadFont has already doubled the font size, to compensate
           for physically smaller screens.  Undo that, since OpenGL hacks
           use full-resolution framebuffers, unlike X11 hacks. */
        scale /= 2;

        window_width  /= scale;
        window_height /= scale;
        glScalef (scale, scale, scale);
      }
# endif /* USE_IPHONE */

      if (rot > 135 || rot < -135)		/* 180 */
        {
          glTranslatef (window_width, window_height, 0);
          glRotatef (180, 0, 0, 1);
        }
      else if (rot > 45)			/* 90 */
        {
          glTranslatef (window_width, 0, 0);
          glRotatef (90, 0, 0, 1);
          swap = window_width;
          window_width = window_height;
          window_height = swap;
        }
      else if (rot < -45)			/* 270 */
        {
          glTranslatef(0, window_height, 0);
          glRotatef (-90, 0, 0, 1);
          swap = window_width;
          window_width = window_height;
          window_height = swap;
        }

      switch (position) {
      case 0:					/* center */
        x = (window_width  - w) / 2;
        y = (window_height - h) / 2;
        break;
      case 1:					/* top */
        x = lh;
        y = window_height - lh * 2;
        break;
      case 2:					/* bottom */
        x = lh;
        y = h - lh;
        break;
      default:
        abort();
      }

      glTranslatef (x, y, 0);

      /* draw the text five times, to give it a border. */
      {
        const XPoint offsets[] = {{ -1, -1 },
                                  { -1,  1 },
                                  {  1,  1 },
                                  {  1, -1 },
                                  {  0,  0 }};
        int i;

        glColor3f (0, 0, 0);
        for (i = 0; i < countof(offsets); i++)
          {
            if (offsets[i].x == 0)
              glColor4fv (color);
            glPushMatrix();
            glTranslatef (offsets[i].x, offsets[i].y, 0);
            print_texture_string (data, string);
            glPopMatrix();
          }
      }
    }
    glPopMatrix();
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  if (tex_p)   glEnable (GL_TEXTURE_2D); else glDisable (GL_TEXTURE_2D);
  if (texs_p)  glEnable (GL_TEXTURE_GEN_S);/*else glDisable(GL_TEXTURE_GEN_S);*/
  if (text_p)  glEnable (GL_TEXTURE_GEN_T);/*else glDisable(GL_TEXTURE_GEN_T);*/
  if (light_p) glEnable (GL_LIGHTING); /*else glDisable (GL_LIGHTING);*/
  if (depth_p) glEnable (GL_DEPTH_TEST); else glDisable (GL_DEPTH_TEST);
  if (cull_p)  glEnable (GL_CULL_FACE); /*else glDisable (GL_CULL_FACE);*/
  if (fog_p)   glEnable (GL_FOG); /*else glDisable (GL_FOG);*/

  glViewport (ovp[0], ovp[1], ovp[2], ovp[3]);

# ifndef HAVE_JWZGLES
  glPolygonMode (GL_FRONT, opoly[0]);
# endif

  glMatrixMode(GL_MODELVIEW);
}


/* Releases the font and texture.
 */
void
free_texture_font (texture_font_data *data)
{
  while (data->cache)
    {
      texfont_cache *next = data->cache->next;
      glDeleteTextures (1, &data->cache->texid);
      free (data->cache);
      data->cache = next;
    }
  if (data->xftfont)
    XftFontClose (data->dpy, data->xftfont);
  free (data);
}
