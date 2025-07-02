/* texfont, Copyright © 2005-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Renders X11 fonts into textures for use with OpenGL or GLSL.
 */

#include "screenhackI.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */

#include "fps.h"    /* for current_device_rotation() */
#include "pow2.h"
#include "resources.h"
#include "texfont.h"
#include "utf8wc.h"

#ifdef HAVE_GLSL
# include "glsl-utils.h"
#endif /* HAVE_GLSL */

#undef HAVE_XSHM_EXTENSION  /* doesn't actually do any good here */

extern void clear_gl_error (void);		/* xlock-gl.c */
extern void check_gl_error (const char *type);
extern float jwxyz_font_scale (Window);		/* jwxyz-cocoa.m */


#ifdef HAVE_GLSL
/* Shader programs for rendering text textures when the caller is using
   GLSL and the GLES 3.x API rather than the OpenGL 3.1 or GLES 1.x APIs.
 */
static const GLchar *shader_version_2_1 = "#version 120\n";
static const GLchar *shader_version_3_0 = "#version 130\n";
static const GLchar *shader_version_3_0_es = "#version 300 es		\n\
   precision highp float;						\n\
   precision highp int;							\n\
";
static const GLchar *vertex_shader_attribs_2_1 = "\
   attribute vec2 VertexCoord;						\n\
   attribute vec2 VertexTex;						\n\
   varying vec2 TexCoord;						\n\
   varying vec4 Color;							\n\
";
static const GLchar *vertex_shader_attribs_3_0 = "\
   in vec2 VertexCoord;							\n\
   in vec2 VertexTex;							\n\
   out vec2 TexCoord;							\n\
   out vec4 Color;							\n\
";
static const GLchar *vertex_shader_main = "\
   uniform mat4 ProjMat;						\n\
   uniform vec4 FontColor;						\n\
   void main (void)							\n\
   {									\n\
     gl_Position = ProjMat*vec4 (VertexCoord, 0, 1);			\n\
     TexCoord = VertexTex;						\n\
     Color = FontColor;							\n\
   }									\n\
";
static const GLchar *fragment_shader_attribs_2_1 = "\
   varying vec4 Color;							\n\
   varying vec2 TexCoord;						\n\
";
static const GLchar *fragment_shader_attribs_3_0 = "\
   in vec4 Color;							\n\
   in vec2 TexCoord;							\n\
   out vec4 FragColor;							\n\
";
static const GLchar *fragment_shader_main = "\
   uniform sampler2D TexSampler;					\n\
   void main (void)							\n\
   {									\n\
     const float MinAlpha = 0.01;					\n\
     const float LODBias = 0.25;					\n\
     if (Color.a <= MinAlpha)						\n\
       discard;								\n\
";
static const GLchar *fragment_shader_out_2_1 = "\
     gl_FragColor = Color*texture2D (TexSampler, TexCoord.st, LODBias); \n\
   }\n";
static const GLchar *fragment_shader_out_3_0 = "\
     FragColor = Color*texture (TexSampler, TexCoord.st, LODBias);	\n\
   }\n";
#endif /* HAVE_GLSL */


/* LRU cache of textures, to optimize the case where we're drawing the
   same strings repeatedly.
 */
typedef struct texfont_cache texfont_cache;
struct texfont_cache {
  char *string;
  GLuint texid;
  XCharStruct extents;
  int tex_width, tex_height;
  texfont_cache *next;
};

struct texture_font_data {
  Display *dpy;
  XftFont *xftfont;
  int cache_size;
  texfont_cache *cache;
  Bool dropshadow_p;
  Bool mipmap_p;
# ifdef HAVE_GLSL
  Bool shaders_initialized, use_shaders;
  GLuint shader_program;
  GLint vertex_coord_index, vertex_tex_index;
  GLint proj_mat_index, font_color_index, tex_sampler_index;
# endif /* HAVE_GLSL */
};


/* Given a Pixmap (of screen depth), converts it to an OpenGL luminance mipmap.
   RGB are averaged to grayscale, and the resulting value is treated as alpha.
   Pass in the size of the pixmap; the size of the texture is returned
   (it may be larger, since GL like powers of 2.)

   We use a screen-depth pixmap instead of a 1bpp bitmap so that if the fonts
   were drawn with antialiasing, that is preserved.
 */
static void
bitmap_to_texture (const texture_font_data *tfdata, Pixmap p,
                   Visual *visual, int depth, int *wP, int *hP)
{
  Display *dpy = tfdata->dpy;
  int ow = *wP;
  int oh = *hP;
  GLsizei w2 = (GLsizei) to_pow2 (ow);
  GLsizei h2 = (GLsizei) to_pow2 (oh);
  int x, y, max, scale;
  XImage *image = 0;
  unsigned char *data = (unsigned char *) calloc (w2 * 2, (h2 + 1));
  unsigned char *out = data;
  GLint rowpack = 0;
  GLint alignment = 0;

  /* OpenGLES doesn't support GL_INTENSITY, so instead of using a
     texture with 1 byte per pixel, the intensity value, we have
     to use 2 bytes per pixel: solid white, and an alpha value.
   */
# ifdef HAVE_JWZGLES
#  undef GL_INTENSITY
# endif

# ifdef HAVE_XSHM_EXTENSION
  Bool use_shm = get_boolean_resource (dpy, "useSHM", "Boolean");
  XShmSegmentInfo shm_info;
# endif /* HAVE_XSHM_EXTENSION */

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

# ifdef HAVE_XSHM_EXTENSION
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

  if (!image) {
    /* XCreateImage fills in (red|green_blue)_mask. XGetImage only does that
       when reading from a Window, not when it's a Pixmap.
     */
    image = XCreateImage (dpy, visual, depth, ZPixmap, 0, NULL, ow, oh,
                          BitmapPad (dpy), 0);
    image->data = malloc (image->height * image->bytes_per_line);
    XGetSubImage (dpy, p, 0, 0, ow, oh, ~0L, ZPixmap, image, 0, 0);
  }

# ifdef DUMP_BITMAPS
  fprintf (stderr, "\n\n%d x %d => %d x %d, %d\n", ow, oh, w2, h2, scale);
# endif
  for (y = 0; y < h2; y++) {
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
      unsigned long r = pixel & image->red_mask;
      /* This goofy trick is to make any of RGBA/ABGR/ARGB work. */
      pixel = ((r >> 24) | (r >> 16) | (r >> 8) | r) & 0xFF;

# ifdef DUMP_BITMAPS
      if (sx < ow && sy < oh && sx <= 79 && sy <= 40)
#  ifdef HAVE_JWXYZ
        fprintf (stderr, "%c", 
                 r >= 0xFF000000 ? '#' : 
                 r >= 0x88000000 ? '%' : 
                 r ? '.' : ' ');
#  else
        fprintf (stderr, "%c", 
                 r >= 0xFF0000 ? '#' : 
                 r >= 0x880000 ? '%' : 
                 r ? '.' : ' ');
#  endif
# endif

# if 0  /* Debugging checkerboard */
      if (sx < ow && sy < oh && (((sx / 4) & 1) ^ ((sy / 4) & 1)))
        pixel = 0x3F;
# endif

# ifndef GL_INTENSITY
      *out++ = 0xFF;  /* 2 bytes per pixel (luminance, alpha) */
# endif
      *out++ = pixel;
    }
# ifdef DUMP_BITMAPS
    if (y * scale <= 40) fprintf (stderr, "\n");
# endif
  }

# ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    destroy_xshm_image (dpy, image, &shm_info);
  else
# endif /* HAVE_XSHM_EXTENSION */
  {
    /* We malloc'd image->data, so we free it. */
    free (image->data);
    image->data = NULL;
    XDestroyImage (image);
  }

  image = 0;

  glGetIntegerv (GL_UNPACK_ROW_LENGTH, &rowpack);
  glGetIntegerv (GL_UNPACK_ALIGNMENT, &alignment);

  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

  {
# ifdef GL_INTENSITY
    GLuint iformat = GL_INTENSITY;
    GLuint format  = GL_LUMINANCE;
# else
    GLuint iformat = GL_LUMINANCE_ALPHA;
    GLuint format  = GL_LUMINANCE_ALPHA;
# endif
    GLuint type    = GL_UNSIGNED_BYTE;

#ifdef HAVE_GLSL
    if (tfdata->use_shaders)
      {
        glTexImage2D (GL_TEXTURE_2D, 0, iformat, w2, h2, 0, format,
                      type, data);
        glGenerateMipmap (GL_TEXTURE_2D);
      }
    else
#endif /* HAVE_GLSL */
      {
        if (tfdata->mipmap_p)
          gluBuild2DMipmaps (GL_TEXTURE_2D, iformat, w2, h2, format, 
                             type, data);
        else
          glTexImage2D (GL_TEXTURE_2D, 0, iformat, w2, h2, 0, format,
                        type, data);
      }
  }

  glPixelStorei (GL_UNPACK_ROW_LENGTH, rowpack);
  glPixelStorei (GL_UNPACK_ALIGNMENT, alignment);

  {
    char msg[100];
    sprintf (msg, "texture font %s (%d x %d)",
             tfdata->mipmap_p ? "gluBuild2DMipmaps" : "glTexImage2D",
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
  const char *def1 = "sans-serif 18";
  const char *def2 = "sans-serif 14";
  const char *def3 = "monospace";
  XftFont *f = 0;
  texture_font_data *data;
  int cache_size = get_integer_resource (dpy, "texFontCacheSize", "Integer");

  /* Hacks that draw a lot of different strings on the screen simultaneously,
     like Star Wars, should set this to a larger value for performance. */
  if (cache_size <= 0)
    cache_size = 30;

  if (!res || !*res) abort();

  if (!strcmp (res, "fpsFont")) {  /* Kludge. */
    def1 = "monospace bold 18"; /* also fps.c */
    cache_size = 0;  /* No need for a cache on FPS: already throttled. */
  }

  if (!font) font = strdup(def1);

  f = load_xft_font_retry (dpy, screen, font);
  if (!f && !!strcmp (font, def1))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def1);
      free (font);
      font = strdup (def1);
      f = load_xft_font_retry (dpy, screen, font);
    }

  if (!f && !!strcmp (font, def2))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def2);
      free (font);
      font = strdup (def2);
      f = load_xft_font_retry (dpy, screen, font);
    }

  if (!f && !!strcmp (font, def3))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def3);
      free (font);
      font = strdup (def3);
      f = load_xft_font_retry (dpy, screen, font);
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
  data->dropshadow_p =
    !get_boolean_resource (dpy, "texFontOmitDropShadow", "Boolean");

  data->mipmap_p = True;

# if defined(__APPLE__) && !defined(HAVE_COCOA)   /* macOS X11 */
  /* Some time before macOS 14.7.3, gluBuild2DMipmaps() started segfaulting. */
  data->mipmap_p = False;
# endif

# ifdef HAVE_JWZGLES
  /* This would work, but it's wasteful for no benefit. */
  /* Wait, is it ever useful? */
  data->mipmap_p = False;
# endif

#ifdef HAVE_GLSL
  /* Setting data->shaders_initialized to False will cause
     initialize_textfont_shaders_glsl to be called by print_texture_label,
     if necessary.  If strings are displayed by print_texture_string, the
     caller is responsible for calling initialize_textfont_shaders_glsl
     first. */
  data->shaders_initialized = False;
  data->use_shaders = False;
  data->shader_program = 0;
  data->vertex_coord_index = -1;
  data->vertex_tex_index = -1;
  data->proj_mat_index = -1;
  data->font_color_index = -1;
  data->tex_sampler_index = -1;
#endif /* HAVE_GLSL */

  return data;
}


/* Measure the string, returning the overall metrics.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.

   The origin is at the origin of the first character, so subsequent
   lines of a multi-line string look like descenders (below baseline).

   If an XftDraw is supplied, render the string as well, at X,Y.
   Positive Y is down (X11 style, not OpenGL style).
 */
static void
iterate_texture_string (texture_font_data *data,
                        const char *s,
                        int draw_x, int draw_y,
                        XftDraw *xftdraw, XftColor *xftcolor,
                        XCharStruct *metrics_ret)
{
  int line_height = data->xftfont->ascent + data->xftfont->descent;
  int subscript_offset = line_height * 0.3;
  const char *os = s;
  Bool sub_p = False, osub_p = False;
  int cw = 0, tabs = 0;
  XCharStruct overall = { 0, };
  int x  = 0, y  = 0;
  int ox = x, oy = y;

  while (1)
    {
      if (*s == 0 ||
          *s == '\n' ||
          *s == '\t' ||
          (*s == '[' && isdigit(s[1])) ||
          (*s == ']' && sub_p))
        {
          if (s != os)
            {
              XGlyphInfo e;
              XCharStruct c;
              int y2 = y;
              if (sub_p) y2 += subscript_offset;

              XftTextExtentsUtf8 (data->dpy, data->xftfont,
                                  (FcChar8 *) os, (int) (s - os),
                                  &e);
              c.lbearing = -e.x;		/* XGlyphInfo to XCharStruct */
              c.rbearing =  e.width  - e.x;
              c.ascent   =  e.y;
              c.descent  =  e.height - e.y;
              c.width    =  e.xOff;

# undef MAX
# undef MIN
# define MAX(A,B) ((A)>(B)?(A):(B))
# define MIN(A,B) ((A)<(B)?(A):(B))
              overall.ascent   = MAX (overall.ascent,   -y2 + c.ascent);
              overall.descent  = MAX (overall.descent,   y2 + c.descent);
              overall.lbearing = MIN (overall.lbearing, (x  + c.lbearing));
              overall.rbearing = MAX (overall.rbearing,  x  + c.rbearing);
              overall.width    = MAX (overall.width,     x  + c.width);
              x += c.width;
# undef MAX
# undef MIN
            }

          if (*s == '\n')
            {
              x = 0;
              y += line_height;
              sub_p = False;
            }
          else if (*s == '\t')
            {
              if (! cw)
                {
                  /* Measure "m" to determine tab width. */
                  XGlyphInfo e;
                  XftTextExtentsUtf8 (data->dpy, data->xftfont,
                                      (FcChar8 *) "m", 1, &e);
                  cw = e.xOff;
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
                               draw_x + ox,
                               draw_y +
                               oy + (osub_p ? subscript_offset : 0),
                               (FcChar8 *) os, (int) (s - os));
          if (!*s) break;
          os = s+1;
          ox = x;
          oy = y;
          osub_p = sub_p;
        }
      s++;
    }

  if (metrics_ret)
    *metrics_ret = overall;
}


/* Bounding box of the multi-line string, in pixels,
   and overall ascent/descent of the font.
 */
void
texture_string_metrics (texture_font_data *data, const char *s,
                        XCharStruct *metrics_ret,
                        int *ascent_ret, int *descent_ret)
{
  if (metrics_ret)
    iterate_texture_string (data, s, 0, 0, 0, 0, metrics_ret);
  if (ascent_ret)  *ascent_ret  = data->xftfont->ascent;
  if (descent_ret) *descent_ret = data->xftfont->descent;
}


/* Returns a cache entry for this string, with a valid texid.
   If the returned entry has a string in it, the texture is valid.
   Otherwise it is an empty entry waiting to be rendered.
 */
static struct texfont_cache *
texfont_get_cache (texture_font_data *data, const char *string)
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
      if (!prev) abort();
      free (prev->string);
      prev->string     = 0;
      prev->tex_width  = 0;
      prev->tex_height = 0;
      memset (&prev->extents, 0, sizeof(prev->extents));
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


static Pixmap
string_to_pixmap (texture_font_data *data, const char *string,
                  XCharStruct *extents_ret,
                  int *width_ret, int *height_ret)
{
  Window window = RootWindow (data->dpy, 0);
  Pixmap p;
  XGCValues gcv;
  GC gc;
  XWindowAttributes xgwa;
  XRenderColor rcolor;
  XftColor xftcolor;
  XftDraw *xftdraw;
  int width, height;
  XCharStruct overall;

  /* Measure the string and create a Pixmap of the proper size.
   */
  XGetWindowAttributes (data->dpy, window, &xgwa);
  iterate_texture_string (data, string, 0, 0, 0, 0, &overall);
  width  = overall.rbearing - overall.lbearing;
  height = overall.ascent   + overall.descent;
  if (width  <= 0) width  = 1;
  if (height <= 0) height = 1;
  p = XCreatePixmap (data->dpy, window, width, height, xgwa.depth);

  gcv.foreground = BlackPixelOfScreen (xgwa.screen);
  gc = XCreateGC (data->dpy, p, GCForeground, &gcv);
  XFillRectangle (data->dpy, p, gc, 0, 0, width, height);
  XFreeGC (data->dpy, gc);

  /* Render the string into the pixmap.
   */
  rcolor.red = rcolor.green = rcolor.blue = rcolor.alpha = 0xFFFF;
  XftColorAllocValue (data->dpy, xgwa.visual, xgwa.colormap,
                      &rcolor, &xftcolor);
  xftdraw = XftDrawCreate (data->dpy, p, xgwa.visual, xgwa.colormap);
  iterate_texture_string (data, string,
                          -overall.lbearing, overall.ascent,
                          xftdraw, &xftcolor, 0);
  XftDrawDestroy (xftdraw);
  XftColorFree (data->dpy, xgwa.visual, xgwa.colormap, &xftcolor);
  if (width_ret)   *width_ret   = width;
  if (height_ret)  *height_ret  = height;
  if (extents_ret) *extents_ret = overall;
  return p;
}


/* Renders the given string into the prevailing texture.
   Returns the metrics of the text, and size of the texture.
 */
void
string_to_texture (texture_font_data *data, const char *string,
                   XCharStruct *extents_ret,
                   int *tex_width_ret, int *tex_height_ret)
{
  Window window = RootWindow (data->dpy, 0);
  XWindowAttributes xgwa;
  int width, height;
  Pixmap p = string_to_pixmap (data, string, extents_ret, &width, &height);

  /* Copy the bits from the Pixmap into a texture, unless it's cached.
   */
  XGetWindowAttributes (data->dpy, window, &xgwa);
  bitmap_to_texture (data, p, xgwa.visual, xgwa.depth, &width, &height);
  XFreePixmap (data->dpy, p);

  if (tex_width_ret)  *tex_width_ret  = width;
  if (tex_height_ret) *tex_height_ret = height;
}


/* True if the string appears to be a "missing" character.
   The string should contain a single UTF8 character.

   Since there may be no reliable way to tell whether a font contains a
   character or has substituted a "missing" glyph for it, this function
   examines the bits to see if it is either solid black, or is a simple
   rectangle, which is what most fonts use.
 */
Bool
blank_character_p (texture_font_data *data, const char *string)
{
  Window window = RootWindow (data->dpy, 0);
  int width, height;
  Pixmap p;
  XImage *im;
  int x, y, j;
  int *xings;
  XWindowAttributes xgwa;
  Bool ret = False;

# ifdef HAVE_XFT
  /* Xft lets us actually ask whether the character is in the font!
     I'm not sure that this is a guarantee that the character is not
     a substitution rectangle, however. */
  {
    unsigned long uc = 0;
    utf8_decode ((const unsigned char *) string, strlen (string), &uc);
    ret = !XftCharExists (data->dpy, data->xftfont, (FcChar32) uc);

#  if 0
    {
      const unsigned char *s = (unsigned char *) string;
      fprintf (stderr, "## %d %lu", ret, uc);
      for (; *s; s++) fprintf (stderr, " %02x", *s);
      fprintf (stderr, "\t [%s]\n", string);
    }
#  endif

    if (ret) return ret;   /* If Xft says it is blank, believe it. */
  }
# endif /* HAVE_XFT */

  /* If we don't have real Xft, we have to check the bits.
     If we do have Xft and it says that the character exists,
     verify that by checking the bits anyway.
   */

  p = string_to_pixmap (data, string, 0, &width, &height);
  im = XGetImage (data->dpy, p, 0, 0, width, height, ~0L, ZPixmap);
  xings = (int *) calloc (height, sizeof(*xings));
  XFreePixmap (data->dpy, p);
  XGetWindowAttributes (data->dpy, window, &xgwa);

  for (y = 0, j = 0; y < im->height; y++)
    {
      unsigned long prev = 0;
      int c = 0;
      for (x = 0; x < im->width; x++)
        {
          unsigned long p = XGetPixel (im, x, y);
          p = (p & 0x0000FF00);	 /* Take just one channel, any channel */
          p = p ? 1 : 0;	 /* Only care about B&W */
          if (p != prev) c++;
          prev = p;
        }
      if (j == 0 || xings[j-1] != c)
        xings[j++] = c;
    }

  /* xings contains a schematic of how many times the color changed
     on a line, with duplicates removed, e.g.:

             *     1      *****   1       ****   1       ****   1
            * *    3      *    *  3      *    *  3      *    *  3
           *   *   .      *****   1      *       1      *    *  .
          *******  1      *    *  3      *       .      *    *  .
          *     *  3      *    *  .      *    *  3      *    *  .
          *     *  .      *****   1       ****   1       ****   1

     So "131" is the pattern for a hollow rectangle, which is what most
     fonts use for missing characters.  It also gets a false positive on
     capital or lower case O, and on 0 without a slash, but I can live
     with that.
   */

  if (j <= 1)
    ret = True;
  else if (j == 3 && xings[0] == 1 && xings[1] == 3 && xings[2] == 1)
    ret = True;
  else if (im->width < 2 || im->height < 2)
    ret = True;

  XDestroyImage (im);
  free (xings);
  return ret;
}



/* Set the various OpenGL parameters for properly rendering things
   with a texture generated by string_to_texture().
 */
void
enable_texture_string_parameters (texture_font_data *data)
{
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   data->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

# ifdef HAVE_GLSL
  if (data->use_shaders)
    return;
# endif /* HAVE_GLSL */

  glEnable (GL_TEXTURE_2D);

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

  /* Don't write the transparent parts of the quad into the depth buffer. */
# ifndef HAVE_ANDROID /* WTF? */
  glAlphaFunc (GL_GREATER, 0.01);
# endif
  glEnable (GL_ALPHA_TEST);
  glDisable (GL_LIGHTING);
  glDisable (GL_TEXTURE_GEN_S);
  glDisable (GL_TEXTURE_GEN_T);
}


/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().

   The origin is at the origin of the first character, so subsequent
   lines of a multi-line string are below that.
 */
void
print_texture_string (texture_font_data *data, const char *string)
{
  XCharStruct overall;
  int tex_width, tex_height;
  texfont_cache *cache;
  GLint old_texture;

  if (!*string) return;

  clear_gl_error ();

  /* Save the prevailing texture ID, and bind ours.  Restored at the end. */
  glGetIntegerv (GL_TEXTURE_BINDING_2D, &old_texture);

  cache = texfont_get_cache (data, string);

  glBindTexture (GL_TEXTURE_2D, cache->texid);
  check_gl_error ("texture font binding");

  /* Measure the string and make a pixmap that will fit it,
     unless it's cached.
   */
  if (cache->string)
    {
      overall    = data->cache->extents;
      tex_width  = data->cache->tex_width;
      tex_height = data->cache->tex_height;
    }
  else
    {
      string_to_texture (data, string, &overall, &tex_width, &tex_height);
    }

  {
    int ofront, oblend;
    Bool alpha_p = False, blend_p = False, light_p = False;
    Bool gen_s_p = False, gen_t_p = False;
    GLfloat omatrix[16];
    GLfloat qx0, qy0, qx1, qy1;
    GLfloat tx0, ty0, tx1, ty1;

    /* If face culling is not enabled, draw front and back. */
    Bool draw_back_face_p = !glIsEnabled (GL_CULL_FACE);

    /* Save the prevailing texture environment. */
    glGetIntegerv (GL_FRONT_FACE, &ofront);

# ifdef HAVE_GLSL
    if (!data->use_shaders)
# endif /* HAVE_GLSL */
      {
        glGetIntegerv (GL_BLEND_DST, &oblend);
        glGetFloatv (GL_TEXTURE_MATRIX, omatrix);
        check_gl_error ("texture font matrix");
        blend_p = glIsEnabled (GL_BLEND);
        alpha_p = glIsEnabled (GL_ALPHA_TEST);
        light_p = glIsEnabled (GL_LIGHTING);
        gen_s_p = glIsEnabled (GL_TEXTURE_GEN_S);
        gen_t_p = glIsEnabled (GL_TEXTURE_GEN_T);

        glPushMatrix();

        glNormal3f (0, 0, 1);

        glMatrixMode (GL_TEXTURE);
        glLoadIdentity ();
        glMatrixMode (GL_MODELVIEW);
      }

    glFrontFace (GL_CW);

    enable_texture_string_parameters (data);

    /* Draw a quad with that texture on it, possibly using a cached texture.
       Position the XCharStruct origin at 0,0 in the scene.
     */
    qx0 =  overall.lbearing;
    qy0 = -overall.descent;
    qx1 =  overall.rbearing;
    qy1 =  overall.ascent;

    tx0 = 0;
    ty1 = 0;
    tx1 = (overall.rbearing - overall.lbearing) / (GLfloat) tex_width;
    ty0 = (overall.ascent + overall.descent)    / (GLfloat) tex_height;

# ifdef HAVE_GLSL
    if (data->use_shaders)
      {
        static const GLuint indices[6] = { 0, 1, 2, 2, 3, 0 };
        GLfloat v[4][2], t[4][2];

        v[0][0] = qx0; v[0][1] = qy0; t[0][0] = tx0; t[0][1] = ty0;
        v[1][0] = qx1; v[1][1] = qy0; t[1][0] = tx1; t[1][1] = ty0;
        v[2][0] = qx1; v[2][1] = qy1; t[2][0] = tx1; t[2][1] = ty1;
        v[3][0] = qx0; v[3][1] = qy1; t[3][0] = tx0; t[3][1] = ty1;

        glEnableVertexAttribArray (data->vertex_coord_index);
        glVertexAttribPointer (data->vertex_coord_index, 2, GL_FLOAT, GL_FALSE,
                               2 * sizeof(GLfloat), v);

        glEnableVertexAttribArray (data->vertex_tex_index);
        glVertexAttribPointer (data->vertex_tex_index, 2, GL_FLOAT, GL_FALSE,
                               2 * sizeof(GLfloat), t);

        glEnable (GL_CULL_FACE);
        glFrontFace (GL_CCW);
        glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);

        if (draw_back_face_p)
          {
            glFrontFace (GL_CW);
            glDrawElements (GL_TRIANGLE_STRIP, 2, GL_UNSIGNED_INT, indices);
          }

        glDisableVertexAttribArray (data->vertex_coord_index);
        glDisableVertexAttribArray (data->vertex_tex_index);

        glDisable(GL_CULL_FACE);
      }
    else
# endif /* HAVE_GLSL */
      {
        glEnable (GL_CULL_FACE);
        glFrontFace (GL_CCW);
        glBegin (GL_QUADS);
        glTexCoord2f (tx0, ty0); glVertex3f (qx0, qy0, 0);
        glTexCoord2f (tx1, ty0); glVertex3f (qx1, qy0, 0);
        glTexCoord2f (tx1, ty1); glVertex3f (qx1, qy1, 0);
        glTexCoord2f (tx0, ty1); glVertex3f (qx0, qy1, 0);
        glEnd();

        if (draw_back_face_p)
          {
            glFrontFace (GL_CW);
            glBegin (GL_QUADS);
            glTexCoord2f (tx0, ty0); glVertex3f (qx0, qy0, 0);
            glTexCoord2f (tx1, ty0); glVertex3f (qx1, qy0, 0);
            glTexCoord2f (tx1, ty1); glVertex3f (qx1, qy1, 0);
            glTexCoord2f (tx0, ty1); glVertex3f (qx0, qy1, 0);
            glEnd();
          }

        glDisable (GL_CULL_FACE);

        glPopMatrix();
      }

    /* Reset to the caller's texture environment.
     */
# ifdef HAVE_GLSL
    if (!data->use_shaders)
# endif /* HAVE_GLSL */
      {
        glBlendFunc (GL_SRC_ALPHA, oblend);
        glMatrixMode (GL_TEXTURE);
        glMultMatrixf (omatrix);
        glMatrixMode (GL_MODELVIEW);
        if (!alpha_p) glDisable (GL_ALPHA_TEST);
        if (light_p) glEnable (GL_LIGHTING);
        if (gen_s_p) glEnable (GL_TEXTURE_GEN_S);
        if (gen_t_p) glEnable (GL_TEXTURE_GEN_T);
      }

    if (!blend_p) glDisable (GL_BLEND);

    glFrontFace (ofront);
  
    glBindTexture (GL_TEXTURE_2D, old_texture);

    check_gl_error ("texture font print");

    /* Store this string into the cache, unless that's where it came from.
     */
    if (!cache->string)
      {
        cache->string     = strdup (string);
        cache->extents    = overall;
        cache->tex_width  = tex_width;
        cache->tex_height = tex_height;
      }
  }
}


#ifdef HAVE_GLSL
/* Initialize the texture font shaders that are used if the hack prefers
   to use a GLSL implementation of the text drawing functionality.  This
   function must be called before the first string is displayed. It assumes
   that the OpenGL graphics context is set to the actual context in which
   the shaders will be used.  Note that this is not necessarily true in
   load_texture_font (for example, if the -pair option is used to run a
   hack). */
static void
initialize_textfont_shaders_glsl (texture_font_data *data)
{
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const GLchar *vertex_shader_source[3];
  const GLchar *fragment_shader_source[4];

  data->use_shaders = False;

  if (!glsl_GetGlAndGlslVersions(&gl_major,&gl_minor,&glsl_major,&glsl_minor,
                                 &gl_gles3))
    {
      data->shaders_initialized = True;
      return;
    }

  if (!gl_gles3)
    {
      if (gl_major < 3 ||
          (glsl_major < 1 || (glsl_major == 1 && glsl_minor < 30)))
        {
          if ((gl_major < 2 || (gl_major == 2 && gl_minor < 1)) ||
              (glsl_major < 1 || (glsl_major == 1 && glsl_minor < 20)))
            {
              data->shaders_initialized = True;
              return;
            }
          /* We have at least OpenGL 2.1 and at least GLSL 1.20. */
          vertex_shader_source[0] = shader_version_2_1;
          vertex_shader_source[1] = vertex_shader_attribs_2_1;
          vertex_shader_source[2] = vertex_shader_main;
          fragment_shader_source[0] = shader_version_2_1;
          fragment_shader_source[1] = fragment_shader_attribs_2_1;
          fragment_shader_source[2] = fragment_shader_main;
          fragment_shader_source[3] = fragment_shader_out_2_1;
        }
      else
        {
          /* We have at least OpenGL 3.0 and at least GLSL 1.30. */
          vertex_shader_source[0] = shader_version_3_0;
          vertex_shader_source[1] = vertex_shader_attribs_3_0;
          vertex_shader_source[2] = vertex_shader_main;
          fragment_shader_source[0] = shader_version_3_0;
          fragment_shader_source[1] = fragment_shader_attribs_3_0;
          fragment_shader_source[2] = fragment_shader_main;
          fragment_shader_source[3] = fragment_shader_out_3_0;
        }
    }
  else /* gl_gles3 */
    {
      if (gl_major < 3 || glsl_major < 3)
        {
          data->shaders_initialized = True;
          return;
        }
      /* We have at least OpenGL ES 3.0 and at least GLSL ES 3.0. */
      vertex_shader_source[0] = shader_version_3_0_es;
      vertex_shader_source[1] = vertex_shader_attribs_3_0;
      vertex_shader_source[2] = vertex_shader_main;
      fragment_shader_source[0] = shader_version_3_0_es;
      fragment_shader_source[1] = fragment_shader_attribs_3_0;
      fragment_shader_source[2] = fragment_shader_main;
      fragment_shader_source[3] = fragment_shader_out_3_0;
    }
  if (!glsl_CompileAndLinkShaders(3,vertex_shader_source,
                                  4,fragment_shader_source,
                                  &data->shader_program))
    {
      data->shaders_initialized = True;
      return;
    }
  data->vertex_coord_index = glGetAttribLocation(data->shader_program,
                                                 "VertexCoord");
  data->vertex_tex_index = glGetAttribLocation(data->shader_program,
                                               "VertexTex");
  data->proj_mat_index = glGetUniformLocation(data->shader_program,
                                              "ProjMat");
  data->font_color_index = glGetUniformLocation(data->shader_program,
                                                "FontColor");
  data->tex_sampler_index = glGetUniformLocation(data->shader_program,
                                                 "TexSampler");
  if (data->vertex_coord_index != -1 && data->vertex_tex_index != -1 &&
      data->proj_mat_index != -1 && data->font_color_index != -1 &&
      data->tex_sampler_index != -1)
    {
      data->use_shaders = True;
      data->mipmap_p = True;
      data->shaders_initialized = True;
    }
  else
    {
      glDeleteProgram(data->shader_program);
      data->shader_program = 0;
      data->shaders_initialized = True;
    }
}
#endif /* HAVE_GLSL */



static void
texfont_transrot (texture_font_data *data, 
                  GLfloat mat[16],
                  GLfloat tx, GLfloat ty, GLfloat tz,
                  GLfloat r, GLfloat rx, GLfloat ry, GLfloat rz)
{
# ifdef HAVE_GLSL
  if (data->use_shaders)
    {
      glsl_Translate (mat, tx, ty, tz);
      glsl_Rotate (mat, r, rx, ry, rz);
    }
  else
# endif /* HAVE_GLSL */
    {
      glTranslatef (tx, ty, tz);
      glRotatef (r, rx, ry, rz);
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
  GLfloat color[4] = { 1, 1, 1, 1 };
  Bool tex_p = False, texs_p = False, text_p = False;
  Bool depth_p = False, fog_p = False, cull_p = False;
  GLint ovp[4];
  GLfloat proj_mat[16];
# ifndef HAVE_JWZGLES
  GLint opoly[2];
# endif

  cull_p = glIsEnabled (GL_CULL_FACE);
  depth_p = glIsEnabled (GL_DEPTH_TEST);

  glGetIntegerv (GL_VIEWPORT, ovp);

  /* This call will fail with an OpenGL ES 3.0 context. color will maintain
     its initial value of {1,1,1,1} in this case. We clear the potential
     error afterwards. */
  clear_gl_error();
  glGetFloatv (GL_CURRENT_COLOR, color);
  clear_gl_error ();
#ifdef HAVE_ANDROID
  Log ("texfont current color 0x%04X (%5.3f,%5.3f,%5.3f,%5.3f)",
       GL_CURRENT_COLOR,color[0], color[1], color[2], color[3]);
#endif

  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable (GL_DEPTH_TEST);


# ifdef HAVE_GLSL
  if (!data->shaders_initialized)
    {
      if (get_boolean_resource (dpy, "prefersGLSL", "PrefersGLSL"))
        initialize_textfont_shaders_glsl (data);
    }

  if (data->use_shaders)
    {
      glUseProgram (data->shader_program);
      glActiveTexture (GL_TEXTURE0);
      glUniform1i (data->tex_sampler_index, 0);
    }
  else
# endif /* HAVE_GLSL */
    {
      tex_p   = glIsEnabled (GL_TEXTURE_2D);
      texs_p  = glIsEnabled (GL_TEXTURE_GEN_S);
      text_p  = glIsEnabled (GL_TEXTURE_GEN_T);
      fog_p   = glIsEnabled (GL_FOG);

      glDisable (GL_TEXTURE_GEN_S);
      glDisable (GL_TEXTURE_GEN_T);
      glDisable (GL_CULL_FACE);
      glDisable (GL_FOG);
      glEnable (GL_TEXTURE_2D);

#ifndef HAVE_JWZGLES
      glGetIntegerv (GL_POLYGON_MODE, opoly);
#endif
      glPolygonMode (GL_FRONT, GL_FILL);

      /* Each matrix mode has its own stack, so we need to push/pop
         them separately.
       */
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();
    }

  {
    XCharStruct cs;
    int ascent, descent;
    int x, y, w, h, swap;
    /* int rot = (int) current_device_rotation(); */
    int rot = 0;  /* Since GL hacks rotate now */

    glViewport (0, 0, window_width, window_height);

# ifdef HAVE_GLSL
    if (data->use_shaders)
      {
        glsl_Identity (proj_mat);
        glsl_Orthographic (proj_mat, 0, window_width, 0, window_height, -1 ,1);
      }
    else
# endif /* HAVE_GLSL */
      {
        glOrtho (0, window_width, 0, window_height, -1, 1);
      }

    while (rot <= -180) rot += 360;
    while (rot >   180) rot -= 360;

    texture_string_metrics (data, string, &cs, &ascent, &descent);
    h = cs.ascent + cs.descent;
    w  = cs.width;

# ifdef HAVE_IPHONE
    {
      /* Size of the font is in points, so scale iOS pixels to points. */
      GLfloat scale = ((window_width > window_height
                        ? window_width : window_height)
                       / 768.0);
      if (scale < 1) scale = 1;

      /* jwxyz-XLoadFont has already doubled the font size, to compensate
         for physically smaller screens.  Undo that, since OpenGL hacks
         use full-resolution framebuffers, unlike X11 hacks. */
      scale /= jwxyz_font_scale (RootWindow (dpy, 0));

      window_width  /= scale;
      window_height /= scale;

#  ifdef HAVE_GLSL
      if (data->use_shaders)
        glsl_Scale (proj_mat, scale, scale, scale);
      else
#  endif /* HAVE_GLSL */
        glScalef (scale, scale, scale);
    }
# endif /* HAVE_IPHONE */

    if (rot > 135 || rot < -135)	/* 180 */
      {
        texfont_transrot (data, proj_mat,
                          window_width, window_height, 0,
                          180, 0, 0, 1);
      }
    else if (rot > 45)			/* 90 */
      {
        texfont_transrot (data, proj_mat,
                          window_width, 0, 0,
                          90, 0, 0, 1);
        swap = window_width;
        window_width = window_height;
        window_height = swap;
      }
    else if (rot < -45)			/* 270 */
      {
        texfont_transrot (data, proj_mat,
                          0, window_height, 0,
                          -90, 0, 0, 1);
        swap = window_width;
        window_width = window_height;
        window_height = swap;
      }

    switch (position) {
    case 0:					/* center */
      x = (window_width  - w) / 2;
      y = (window_height + h) / 2 - ascent;
      break;
    case 1:					/* top */
      x = ascent;
      y = window_height - ascent*2;
      break;
    case 2:					/* bottom */
      x = ascent;
      y = h;
      break;
    default:
      abort();
    }

    texfont_transrot (data, proj_mat,
                      x, y, 0,
                      0, 0, 0, 1);

    /* draw the text five times, to give it a border. */
    {
      const XPoint offsets[] = {{ -1, -1 },
                                { -1,  1 },
                                {  1,  1 },
                                {  1, -1 },
                                {  0,  0 }};
      int i;

# ifdef HAVE_GLSL
      if (data->use_shaders)
        glUniform4f (data->font_color_index, 0, 0, 0, color[3]);
      else
# endif /* HAVE_GLSL */
        glColor4f (0, 0, 0, color[3]);

      for (i = (data->dropshadow_p ? 0 : countof(offsets)-1);
           i < countof(offsets); i++)
        {
          if (offsets[i].x == 0)
            {
# ifdef HAVE_GLSL
              if (data->use_shaders)
                glUniform4fv (data->font_color_index, 1, color);
              else
# endif /* HAVE_GLSL */
                glColor4fv (color);
            }

# ifdef HAVE_GLSL
          if (data->use_shaders)
            {
              GLfloat proj_mat_trans[16];
              glsl_CopyMatrix (proj_mat_trans, proj_mat);
              glsl_Translate (proj_mat_trans, offsets[i].x,offsets[i].y, 0);
              glUniformMatrix4fv (data->proj_mat_index, 1, GL_FALSE, 
                                  proj_mat_trans);
              print_texture_string (data, string);
            }
          else
# endif /* HAVE_GLSL */
            {
              glPushMatrix();
              glTranslatef (offsets[i].x, offsets[i].y, 0);
              print_texture_string (data, string);
              glPopMatrix();
            }
        }
    }
  }

# ifdef HAVE_GLSL
  if (data->use_shaders)
    {
      glUseProgram (0);
    }
  else
# endif /* HAVE_GLSL */
    {
      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);

      if (tex_p)   glEnable (GL_TEXTURE_2D); else glDisable (GL_TEXTURE_2D);
      if (texs_p)  glEnable (GL_TEXTURE_GEN_S);
      if (text_p)  glEnable (GL_TEXTURE_GEN_T);
      if (fog_p)   glEnable (GL_FOG); /*else glDisable (GL_FOG);*/
#ifndef HAVE_JWZGLES
      glPolygonMode (GL_FRONT, opoly[0]);
#endif
    }

  glViewport (ovp[0], ovp[1], ovp[2], ovp[3]);

  if (cull_p) glEnable (GL_CULL_FACE); /*else glDisable (GL_CULL_FACE);*/
  if (depth_p) glEnable (GL_DEPTH_TEST); else glDisable (GL_DEPTH_TEST);
}


#ifdef HAVE_JWXYZ
char *
texfont_unicode_character_name (texture_font_data *data, unsigned long uc)
{
  Font fid = data->xftfont->xfont->fid;
  return jwxyz_unicode_character_name (data->dpy, fid, uc);
}
#endif /* HAVE_JWXYZ */



/* Releases the font and texture.
 */
void
free_texture_font (texture_font_data *data)
{
  while (data->cache)
    {
      texfont_cache *next = data->cache->next;
      glDeleteTextures (1, &data->cache->texid);
      free (data->cache->string);
      free (data->cache);
      data->cache = next;
    }
  if (data->xftfont)
    XftFontClose (data->dpy, data->xftfont);

# ifdef HAVE_GLSL
  if (data->use_shaders &&
      data->shaders_initialized &&
      data->shader_program != 0)
    {
      glUseProgram (0);
      glDeleteProgram (data->shader_program);
    }
# endif /* HAVE_GLSL */

  free (data);
}
