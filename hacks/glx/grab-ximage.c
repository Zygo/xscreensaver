/* grab-ximage.c --- grab the screen to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001-2008 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
#else
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <GL/gl.h>	/* only for GLfloat */
# include <GL/glu.h>	/* for gluBuild2DMipmaps */
# include <GL/glx.h>	/* for glXMakeCurrent() */
#endif

#include "grab-ximage.h"
#include "grabscreen.h"
#include "visual.h"

/* If REFORMAT_IMAGE_DATA is defined, then we convert Pixmaps to textures
   like this:

     - get Pixmap as an XImage in whatever form the server hands us;
     - convert that XImage to 32-bit RGBA in client-local endianness;
     - make the texture using RGBA, UNSIGNED_BYTE.

   If undefined, we do this:

     - get Pixmap as an XImage in whatever form the server hands us;
     - figure out what OpenGL texture packing parameters correspond to
       the image data that the server sent us and use that, e.g.,
       BGRA, INT_8_8_8_8_REV.

   You might expect the second method to be faster, since we're not making
   a second copy of the data and iterating each pixel before we hand it
   to GL.  But, you'd be wrong.  The first method is almost 6x faster.
   I guess GL is reformatting it *again*, and doing it very inefficiently!
*/
#define REFORMAT_IMAGE_DATA


#ifdef HAVE_XSHM_EXTENSION
# include "resources.h"
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */

extern char *progname;

#include <sys/time.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else
# include <X11/Xutil.h>
#endif

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


static int debug_p = 0;

static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


#ifdef REFORMAT_IMAGE_DATA

/* Given a bitmask, returns the position and width of the field.
 */
static void
decode_mask (unsigned int mask, unsigned int *pos_ret, unsigned int *size_ret)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1L << i))
      {
        int j = 0;
        *pos_ret = i;
        for (; i < 32; i++, j++)
          if (! (mask & (1L << i)))
            break;
        *size_ret = j;
        return;
      }
}


/* Given a value and a field-width, expands the field to fill out 8 bits.
 */
static unsigned char
spread_bits (unsigned char value, unsigned char width)
{
  switch (width)
    {
    case 8: return value;
    case 7: return (value << 1) | (value >> 6);
    case 6: return (value << 2) | (value >> 4);
    case 5: return (value << 3) | (value >> 2);
    case 4: return (value << 4) | (value);
    case 3: return (value << 5) | (value << 2) | (value >> 2);
    case 2: return (value << 6) | (value << 4) | (value);
    default: abort(); break;
    }
}


static XImage *
convert_ximage_to_rgba32 (Screen *screen, XImage *image)
{
  Display *dpy = DisplayOfScreen (screen);
  Visual *visual = DefaultVisualOfScreen (screen);

  int x, y;
  unsigned int crpos=0, cgpos=0, cbpos=0, capos=0; /* bitfield positions */
  unsigned int srpos=0, sgpos=0, sbpos=0;
  unsigned int srmsk=0, sgmsk=0, sbmsk=0;
  unsigned int srsiz=0, sgsiz=0, sbsiz=0;
  int i;
  XColor *colors = 0;
  unsigned char spread_map[3][256];

  /* Note: height+2 in "to" to work around an array bounds overrun
     in gluBuild2DMipmaps / gluScaleImage.
   */
  XImage *from = image;
  XImage *to = XCreateImage (dpy, visual, 32,  /* depth */
                             ZPixmap, 0, 0, from->width, from->height + 2,
                             32, /* bitmap pad */
                             0);
  to->data = (char *) calloc (to->height, to->bytes_per_line);

  /* Set the bit order in the XImage structure to whatever the
     local host's native bit order is.
   */
  to->bitmap_bit_order =
    to->byte_order =
    (bigendian() ? MSBFirst : LSBFirst);

  if (visual_class (screen, visual) == PseudoColor ||
      visual_class (screen, visual) == GrayScale)
    {
      Colormap cmap = DefaultColormapOfScreen (screen);
      int ncolors = visual_cells (screen, visual);
      int i;
      colors = (XColor *) calloc (sizeof (*colors), ncolors+1);
      for (i = 0; i < ncolors; i++)
        colors[i].pixel = i;
      XQueryColors (dpy, cmap, colors, ncolors);
    }

  if (colors == 0)  /* truecolor */
    {
      srmsk = to->red_mask;
      sgmsk = to->green_mask;
      sbmsk = to->blue_mask;

      decode_mask (srmsk, &srpos, &srsiz);
      decode_mask (sgmsk, &sgpos, &sgsiz);
      decode_mask (sbmsk, &sbpos, &sbsiz);
    }

  /* Pack things in "RGBA" order in client endianness. */
  if (bigendian())
    crpos = 24, cgpos = 16, cbpos =  8, capos =  0;
  else
    crpos =  0, cgpos =  8, cbpos = 16, capos = 24;

  if (colors == 0)  /* truecolor */
    {
      for (i = 0; i < 256; i++)
        {
          spread_map[0][i] = spread_bits (i, srsiz);
          spread_map[1][i] = spread_bits (i, sgsiz);
          spread_map[2][i] = spread_bits (i, sbsiz);
        }
    }

  /* trying to track down an intermittent crash in ximage_putpixel_32 */
  if (to->width  < from->width)  abort();
  if (to->height < from->height) abort();

  for (y = 0; y < from->height; y++)
    for (x = 0; x < from->width; x++)
      {
        unsigned long sp = XGetPixel (from, x, y);
        unsigned char sr, sg, sb;
        unsigned long cp;

        if (colors)
          {
            sr = colors[sp].red   & 0xFF;
            sg = colors[sp].green & 0xFF;
            sb = colors[sp].blue  & 0xFF;
          }
        else
          {
            sr = (sp & srmsk) >> srpos;
            sg = (sp & sgmsk) >> sgpos;
            sb = (sp & sbmsk) >> sbpos;

            sr = spread_map[0][sr];
            sg = spread_map[1][sg];
            sb = spread_map[2][sb];
          }

        cp = ((sr << crpos) |
              (sg << cgpos) |
              (sb << cbpos) |
              (0xFF << capos));

        XPutPixel (to, x, y, cp);
      }

  if (colors) free (colors);

  return to;
}

#endif /* REFORMAT_IMAGE_DATA */

/* Shrinks the XImage by a factor of two.
   We use this when mipmapping fails on large textures.
 */
static void
halve_image (XImage *ximage, XRectangle *geom)
{
  int w2 = ximage->width/2;
  int h2 = ximage->height/2;
  int x, y;
  XImage *ximage2;

  if (w2 <= 32 || h2 <= 32)   /* let's not go crazy here, man. */
    return;

  if (debug_p)
    fprintf (stderr, "%s: shrinking image %dx%d -> %dx%d\n",
             progname, ximage->width, ximage->height, w2, h2);

  ximage2 = (XImage *) calloc (1, sizeof (*ximage2));
  *ximage2 = *ximage;
  ximage2->width = w2;
  ximage2->height = h2;
  ximage2->bytes_per_line = 0;
  ximage2->data = 0;
  XInitImage (ximage2);

  ximage2->data = (char *) calloc (h2, ximage2->bytes_per_line);
  if (!ximage2->data)
    {
      fprintf (stderr, "%s: out of memory (scaling %dx%d image to %dx%d)\n",
               progname, ximage->width, ximage->height, w2, h2);
      exit (1);
    }

  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++)
      XPutPixel (ximage2, x, y, XGetPixel (ximage, x*2, y*2));

  free (ximage->data);
  *ximage = *ximage2;
  ximage2->data = 0;
  XFree (ximage2);

  if (geom)
    {
      geom->x /= 2;
      geom->y /= 2;
      geom->width  /= 2;
      geom->height /= 2;
    }
}


#ifdef REFORMAT_IMAGE_DATA

/* Pulls the Pixmap bits from the server and returns an XImage
   in some format acceptable to OpenGL.
 */
static XImage *
pixmap_to_gl_ximage (Screen *screen, Window window, Pixmap pixmap)
{
  Display *dpy = DisplayOfScreen (screen);
  unsigned int width, height, depth;

# ifdef HAVE_XSHM_EXTENSION
  Bool use_shm = get_boolean_resource (dpy, "useSHM", "Boolean");
  XShmSegmentInfo shm_info;
# endif /* HAVE_XSHM_EXTENSION */

  XImage *server_ximage = 0;
  XImage *client_ximage = 0;

  {
    Window root;
    int x, y;
    unsigned int bw;
    XGetGeometry (dpy, pixmap, &root, &x, &y, &width, &height, &bw, &depth);
  }

  if (width < 5 || height < 5)  /* something's gone wrong somewhere... */
    return 0;

  /* Convert the server-side Pixmap to a client-side GL-ordered XImage.
   */
# ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    {
      Visual *visual = DefaultVisualOfScreen (screen);
      server_ximage = create_xshm_image (dpy, visual, depth,
                                         ZPixmap, 0, &shm_info,
                                         width, height);
      if (server_ximage)
        XShmGetImage (dpy, pixmap, server_ximage, 0, 0, ~0L);
      else
        use_shm = False;
    }
# endif /* HAVE_XSHM_EXTENSION */

  if (!server_ximage)
    server_ximage = XGetImage (dpy, pixmap, 0, 0, width, height, ~0L, ZPixmap);

  client_ximage = convert_ximage_to_rgba32 (screen, server_ximage);

# ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    destroy_xshm_image (dpy, server_ximage, &shm_info);
  else
# endif /* HAVE_XSHM_EXTENSION */
    XDestroyImage (server_ximage);

  return client_ximage;
}


# else /* ! REFORMAT_IMAGE_DATA */

typedef struct {
  unsigned int depth, red_mask, green_mask, blue_mask;	/* when this... */
  GLint type, format;					/* ...use this. */
} conversion_table;

/* Abbreviate these so that the table entries all fit on one line...
 */
#define BYTE               GL_UNSIGNED_BYTE
#define BYTE_2_3_3_REV     GL_UNSIGNED_BYTE_2_3_3_REV
#define BYTE_3_3_2         GL_UNSIGNED_BYTE_3_3_2
#define INT_10_10_10_2     GL_UNSIGNED_INT_10_10_10_2
#define INT_2_10_10_10_REV GL_UNSIGNED_INT_2_10_10_10_REV
#define INT_8_8_8_8        GL_UNSIGNED_INT_8_8_8_8
#define INT_8_8_8_8_REV    GL_UNSIGNED_INT_8_8_8_8_REV
#define SHORT_1_5_5_5_REV  GL_UNSIGNED_SHORT_1_5_5_5_REV
#define SHORT_4_4_4_4      GL_UNSIGNED_SHORT_4_4_4_4
#define SHORT_4_4_4_4_REV  GL_UNSIGNED_SHORT_4_4_4_4_REV
#define SHORT_5_5_5_1      GL_UNSIGNED_SHORT_5_5_5_1
#define SHORT_5_6_5        GL_UNSIGNED_SHORT_5_6_5
#define SHORT_5_6_5_REV    GL_UNSIGNED_SHORT_5_6_5_REV

static const conversion_table ctable[] = {
  { 8,  0x000000E0, 0x0000001C, 0x00000003, BYTE_3_3_2,         GL_RGB      },
  { 8,  0x00000007, 0x00000038, 0x000000C0, BYTE_2_3_3_REV,     GL_RGB      },
  { 16, 0x0000F800, 0x000007E0, 0x0000001F, SHORT_5_6_5,        GL_RGB      },
  { 16, 0x0000001F, 0x000007E0, 0x0000F800, SHORT_5_6_5_REV,    GL_RGB      },
  { 16, 0x0000F000, 0x00000F00, 0x000000F0, SHORT_4_4_4_4,      GL_RGBA     },
  { 16, 0x000000F0, 0x00000F00, 0x0000F000, SHORT_4_4_4_4,      GL_BGRA     },
  { 16, 0x0000000F, 0x000000F0, 0x00000F00, SHORT_4_4_4_4,      GL_ABGR_EXT },
  { 16, 0x0000000F, 0x000000F0, 0x00000F00, SHORT_4_4_4_4_REV,  GL_RGBA     },
  { 16, 0x00000F00, 0x000000F0, 0x0000000F, SHORT_4_4_4_4_REV,  GL_BGRA     },
  { 16, 0x0000F800, 0x000007C0, 0x0000003E, SHORT_5_5_5_1,      GL_RGBA     },
  { 16, 0x0000003E, 0x000007C0, 0x0000F800, SHORT_5_5_5_1,      GL_BGRA     },
  { 16, 0x00000001, 0x0000003E, 0x000007C0, SHORT_5_5_5_1,      GL_ABGR_EXT },
  { 16, 0x0000001F, 0x000003E0, 0x00007C00, SHORT_1_5_5_5_REV,  GL_RGBA     },
  { 16, 0x00007C00, 0x000003E0, 0x0000001F, SHORT_1_5_5_5_REV,  GL_BGRA     },
  { 32, 0xFF000000, 0x00FF0000, 0x0000FF00, INT_8_8_8_8,        GL_RGBA     },
  { 32, 0x0000FF00, 0x00FF0000, 0xFF000000, INT_8_8_8_8,        GL_BGRA     },
  { 32, 0x000000FF, 0x0000FF00, 0x00FF0000, INT_8_8_8_8,        GL_ABGR_EXT },
  { 32, 0x000000FF, 0x0000FF00, 0x00FF0000, INT_8_8_8_8_REV,    GL_RGBA     },
  { 32, 0x00FF0000, 0x0000FF00, 0x000000FF, INT_8_8_8_8_REV,    GL_BGRA     },
  { 32, 0xFFC00000, 0x003FF000, 0x00000FFC, INT_10_10_10_2,     GL_RGBA     },
  { 32, 0x00000FFC, 0x003FF000, 0xFFC00000, INT_10_10_10_2,     GL_BGRA     },
  { 32, 0x00000003, 0x00000FFC, 0x003FF000, INT_10_10_10_2,     GL_ABGR_EXT },
  { 32, 0x000003FF, 0x000FFC00, 0x3FF00000, INT_2_10_10_10_REV, GL_RGBA     },
  { 32, 0x3FF00000, 0x000FFC00, 0x000003FF, INT_2_10_10_10_REV, GL_BGRA     },
  { 24, 0x000000FF, 0x0000FF00, 0x00FF0000, BYTE,               GL_RGB      },
  { 24, 0x00FF0000, 0x0000FF00, 0x000000FF, BYTE,               GL_BGR      },
};


/* Given an XImage, returns the GL settings to use its data as a texture.
 */
static void
gl_settings_for_ximage (XImage *image,
                        GLint *type_ret, GLint *format_ret, GLint *swap_ret)
{
  int i;
  for (i = 0; i < countof(ctable); ++i)
    {
      if (image->bits_per_pixel == ctable[i].depth &&
          image->red_mask       == ctable[i].red_mask &&
          image->green_mask     == ctable[i].green_mask &&
          image->blue_mask      == ctable[i].blue_mask)
        {
          *type_ret   = ctable[i].type;
          *format_ret = ctable[i].format;

          if (image->bits_per_pixel == 24)
            {
              /* don't know how to test this... */
              *type_ret = (ctable[i].type == GL_RGB) ? GL_BGR : GL_RGB;
              *swap_ret = 0;
            }
          else
            {
              *swap_ret = !!(image->byte_order == MSBFirst) ^ !!bigendian();
            }

          if (debug_p)
            {
              fprintf (stderr, "%s: using %s %s %d for %d %08lX %08lX %08lX\n",
                       progname,
                       (*format_ret == GL_RGB      ? "RGB" :
                        *format_ret == GL_BGR      ? "BGR" :
                        *format_ret == GL_RGBA     ? "RGBA" :
                        *format_ret == GL_BGRA     ? "BGRA" :
                        *format_ret == GL_ABGR_EXT ? "ABGR_EXT" :
                        "???"),
                       (*type_ret == BYTE               ? "BYTE" :
                        *type_ret == BYTE_3_3_2         ? "BYTE_3_3_2" :
                        *type_ret == BYTE_2_3_3_REV     ? "BYTE_2_3_3_REV" :
                        *type_ret == INT_8_8_8_8        ? "INT_8_8_8_8" :
                        *type_ret == INT_8_8_8_8_REV    ? "INT_8_8_8_8_REV" :
                        *type_ret == INT_10_10_10_2     ? "INT_10_10_10_2" :
                        *type_ret == INT_2_10_10_10_REV ? "INT_2_10_10_10_REV":
                        *type_ret == SHORT_4_4_4_4      ? "SHORT_4_4_4_4" :
                        *type_ret == SHORT_4_4_4_4_REV  ? "SHORT_4_4_4_4_REV" :
                        *type_ret == SHORT_5_5_5_1      ? "SHORT_5_5_5_1" :
                        *type_ret == SHORT_1_5_5_5_REV  ? "SHORT_1_5_5_5_REV" :
                        *type_ret == SHORT_5_6_5        ? "SHORT_5_6_5" :
                        *type_ret == SHORT_5_6_5_REV    ? "SHORT_5_6_5_REV" :
                        "???"),
                       *swap_ret,
                       image->bits_per_pixel,
                       image->red_mask, image->green_mask, image->blue_mask);
            }

          return;
        }
    }

  /* Unknown RGB fields? */
  abort();
}

#endif /* ! REFORMAT_IMAGE_DATA */

typedef struct {
  GLXContext glx_context;
  Pixmap pixmap;
  int pix_width, pix_height, pix_depth;
  int texid;
  Bool mipmap_p;
  double load_time;

  /* Used in async mode
   */
  void (*callback) (const char *filename, XRectangle *geometry,
                    int iw, int ih, int tw, int th,
                    void *closure);
  void *closure;

  /* Used in sync mode
   */
  char **filename_return;
  XRectangle *geometry_return;
  int *image_width_return;
  int *image_height_return;
  int *texture_width_return;
  int *texture_height_return;

} img_closure;


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


/* return the next larger power of 2. */
static int
to_pow2 (int i)
{
  static const unsigned int pow2[] = { 
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 
    2048, 4096, 8192, 16384, 32768, 65536 };
  int j;
  for (j = 0; j < countof(pow2); j++)
    if (pow2[j] >= i) return pow2[j];
  abort();  /* too big! */
}


/* Loads the given XImage into GL's texture memory.
   The image may be of any size.
   If mipmap_p is true, then make mipmaps instead of just a single texture.
   Writes to stderr and returns False on error.
 */
static Bool
ximage_to_texture (XImage *ximage,
                   GLint type, GLint format,
                   int *width_return,
                   int *height_return,
                   XRectangle *geometry,
                   Bool mipmap_p)
{
  int max_reduction = 7;
  int err_count = 0;
  GLenum err = 0;
  int orig_width = ximage->width;
  int orig_height = ximage->height;
  int tex_width = 0;
  int tex_height = 0;

 AGAIN:

  if (mipmap_p)
    {
      /* gluBuild2DMipmaps doesn't require textures to be a power of 2. */
      tex_width  = ximage->width;
      tex_height = ximage->height;

      if (debug_p)
        fprintf (stderr, "%s: mipmap %d x %d\n",
                 progname, ximage->width, ximage->height);

      gluBuild2DMipmaps (GL_TEXTURE_2D, 3, ximage->width, ximage->height,
                         format, type, ximage->data);
      err = glGetError();
    }
  else
    {
      /* glTexImage2D() requires the texture sizes to be powers of 2.
         So first, create a texture of that size (but don't write any
         data into it.)
       */
      tex_width  = to_pow2 (ximage->width);
      tex_height = to_pow2 (ximage->height);

      if (debug_p)
        fprintf (stderr, "%s: texture %d x %d (%d x %d)\n",
                 progname, ximage->width, ximage->height,
                 tex_width, tex_height);

      glTexImage2D (GL_TEXTURE_2D, 0, 3, tex_width, tex_height, 0,
                    format, type, 0);
      err = glGetError();

      /* Now load our non-power-of-2 image data into the existing texture. */
      if (!err)
        {
          glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
                           ximage->width, ximage->height,
                           GL_RGBA, GL_UNSIGNED_BYTE, ximage->data);
          err = glGetError();
        }
    }

  if (err)
    {
      char buf[100];
      const char *s = (char *) gluErrorString (err);

      if (!s || !*s)
        {
          sprintf (buf, "unknown error %d", (int) err);
          s = buf;
        }

      while (glGetError() != GL_NO_ERROR)
        ;  /* clear any lingering errors */

      if (++err_count > max_reduction)
        {
          fprintf (stderr,
                   "\n"
                   "%s: %dx%d texture failed, even after reducing to %dx%d:\n"
                   "%s: The error was: \"%s\".\n"
                   "%s: probably this means "
                   "\"your video card is worthless and weak\"?\n\n",
                   progname, orig_width, orig_height,
                   ximage->width, ximage->height,
                   progname, s,
                   progname);
          return False;
        }
      else
        {
          if (debug_p)
            fprintf (stderr, "%s: mipmap error (%dx%d): %s\n",
                     progname, ximage->width, ximage->height, s);
          halve_image (ximage, geometry);
          goto AGAIN;
        }
    }

  if (width_return)  *width_return  = tex_width;
  if (height_return) *height_return = tex_height;
  return True;
}


static void load_texture_async_cb (Screen *screen,
                                        Window window, Drawable drawable,
                                        const char *name, XRectangle *geometry,
                                        void *closure);


/* Grabs an image of the desktop (or another random image file) and
   loads the image into GL's texture memory.
   When the callback is called, the image data will have been loaded
   into texture number `texid' (via glBindTexture.)

   If an error occurred, width/height will be 0.
 */
void
load_texture_async (Screen *screen, Window window,
                    GLXContext glx_context,
                    int desired_width, int desired_height,
                    Bool mipmap_p,
                    GLuint texid,
                    void (*callback) (const char *filename,
                                      XRectangle *geometry,
                                      int image_width,
                                      int image_height,
                                      int texture_width,
                                      int texture_height,
                                      void *closure),
                    void *closure)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  img_closure *data = (img_closure *) calloc (1, sizeof(*data));

  if (debug_p)
    data->load_time = double_time();

  data->texid       = texid;
  data->mipmap_p    = mipmap_p;
  data->glx_context = glx_context;
  data->callback    = callback;
  data->closure     = closure;

  XGetWindowAttributes (dpy, window, &xgwa);
  data->pix_width  = xgwa.width;
  data->pix_height = xgwa.height;
  data->pix_depth  = xgwa.depth;

  if (desired_width  && desired_width  < xgwa.width)
    data->pix_width  = desired_width;
  if (desired_height && desired_height < xgwa.height)
    data->pix_height = desired_height;

  data->pixmap = XCreatePixmap (dpy, window, data->pix_width, data->pix_height,
                                data->pix_depth);
  load_image_async (screen, window, data->pixmap, 
                    load_texture_async_cb, data);
}


/* Once we have an XImage, this loads it into GL.
   This is used in both synchronous and asynchronous mode.
 */
static void
load_texture_async_cb (Screen *screen, Window window, Drawable drawable,
                       const char *name, XRectangle *geometry, void *closure)
{
  Display *dpy = DisplayOfScreen (screen);
  Bool ok;
  XImage *ximage;
  GLint type, format;
  int iw=0, ih=0, tw=0, th=0;
  double cvt_time=0, tex_time=0, done_time=0;
  img_closure *data = (img_closure *) closure;
  /* copy closure data to stack and free the original before running cb */
  img_closure dd = *data;
  memset (data, 0, sizeof (*data));
  free (data);
  data = 0;

  if (dd.glx_context)
    glXMakeCurrent (dpy, window, dd.glx_context);

  if (geometry->width <= 0 || geometry->height <= 0)
    {
      /* This can happen if an old version of xscreensaver-getimage
         is installed. */
      geometry->x = 0;
      geometry->y = 0;
      geometry->width  = dd.pix_width;
      geometry->height = dd.pix_height;
    }

  if (geometry->width <= 0 || geometry->height <= 0)
    abort();

  if (debug_p)
    cvt_time = double_time();

# ifdef REFORMAT_IMAGE_DATA
  ximage = pixmap_to_gl_ximage (screen, window, dd.pixmap);
  format = GL_RGBA;
  type = GL_UNSIGNED_BYTE;

#else /* ! REFORMAT_IMAGE_DATA */
  {
    Visual *visual = DefaultVisualOfScreen (screen);
    GLint swap;

    ximage = XCreateImage (dpy, visual, dd.pix_depth, ZPixmap, 0, 0,
                           dd.pix_width, dd.pix_height, 32, 0);

    /* Note: height+2 in "to" to be to work around an array bounds overrun
       in gluBuild2DMipmaps / gluScaleImage. */
    ximage->data = (char *) calloc (ximage->height+2, ximage->bytes_per_line);

    if (!ximage->data ||
        !XGetSubImage (dpy, dd.pixmap, 0, 0, ximage->width, ximage->height,
                       ~0L, ximage->format, ximage, 0, 0))
      {
        XDestroyImage (ximage);
        ximage = 0;
      }

    gl_settings_for_ximage (ximage, &type, &format, &swap);
    glPixelStorei (GL_UNPACK_SWAP_BYTES, !swap);
  }
#endif /* REFORMAT_IMAGE_DATA */

  XFreePixmap (dpy, dd.pixmap);
  dd.pixmap = 0;

  if (debug_p)
    tex_time = double_time();

  if (! ximage)
    ok = False;
  else
    {
      iw = ximage->width;
      ih = ximage->height;
      if (dd.texid != -1)
        glBindTexture (GL_TEXTURE_2D, dd.texid);

      glPixelStorei (GL_UNPACK_ALIGNMENT, ximage->bitmap_pad / 8);
      ok = ximage_to_texture (ximage, type, format, &tw, &th, geometry,
                              dd.mipmap_p);
      if (ok)
        {
          iw = ximage->width;	/* in case the image was shrunk */
          ih = ximage->height;
        }
    }

  if (ximage) XDestroyImage (ximage);

  if (! ok)
    iw = ih = tw = th = 0;

  if (debug_p)
    done_time = double_time();

  if (debug_p)
    fprintf (stderr,
             /* prints: A + B + C = D
                A = file I/O time (happens in background)
                B = time to pull bits from server (this process)
                C = time to convert bits to GL textures (this process)
                D = total elapsed time from "want image" to "see image"

                B+C is responsible for any frame-rate glitches.
              */
             "%s: loading elapsed: %.2f + %.2f + %.2f = %.2f sec\n",
             progname,
             cvt_time  - dd.load_time,
             tex_time  - cvt_time,
             done_time - tex_time,
             done_time - dd.load_time);

  if (dd.callback)
    /* asynchronous mode */
    dd.callback (name, geometry, iw, ih, tw, th, dd.closure);
  else
    {
      /* synchronous mode */
      if (dd.filename_return)       *dd.filename_return       = (char *) name;
      if (dd.geometry_return)       *dd.geometry_return       = *geometry;
      if (dd.image_width_return)    *dd.image_width_return    = iw;
      if (dd.image_height_return)   *dd.image_height_return   = ih;
      if (dd.texture_width_return)  *dd.texture_width_return  = tw;
      if (dd.texture_height_return) *dd.texture_height_return = th;
    }
}
