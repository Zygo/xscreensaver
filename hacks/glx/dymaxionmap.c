/* dymaxionmap --- Buckminster Fuller's unwrapped icosahedral globe.
 * Copyright (c) 2016-2018 Jamie Zawinski.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.	The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 */

#define LABEL_FONT "Helvetica Bold 24"

#ifdef STANDALONE
#define DEFAULTS    "*delay:		20000	\n" \
		    "*showFPS:		False	\n" \
		    "*wireframe:	False	\n" \
		    "*labelFont:  " LABEL_FONT "\n"
# define release_planet 0
# include "xlockmore.h"		    /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"		    /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#include "sphere.h"
#include "normals.h"
#include "texfont.h"
#include "dymaxionmap-coords.h"

#define DEF_ROTATE  "True"
#define DEF_ROLL    "True"
#define DEF_WANDER  "True"
#define DEF_TEXTURE "True"
#define DEF_STARS   "True"
#define DEF_GRID    "True"
#define DEF_SPEED   "1.0"
#define DEF_IMAGE   "BUILTIN_FLAT"
#define DEF_IMAGE2  "NONE"
#define DEF_FRAMES  "720"

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)
#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

static int do_roll;
static int do_wander;
static int do_texture;
static int do_stars;
static int do_grid;
static int frames;
static GLfloat speed;
static char *which_image;
static char *which_image2;

static XrmOptionDescRec opts[] = {
  {"-speed",    ".speed",   XrmoptionSepArg, 0 },
  {"-roll",     ".roll",    XrmoptionNoArg, "true" },
  {"+roll",     ".roll",    XrmoptionNoArg, "false" },
  {"-wander",   ".wander",  XrmoptionNoArg, "true" },
  {"+wander",   ".wander",  XrmoptionNoArg, "false" },
  {"-texture",  ".texture", XrmoptionNoArg, "true" },
  {"+texture",  ".texture", XrmoptionNoArg, "false" },
  {"-stars",    ".stars",   XrmoptionNoArg, "true" },
  {"+stars",    ".stars",   XrmoptionNoArg, "false" },
  {"-grid",     ".grid",    XrmoptionNoArg, "true" },
  {"+grid",     ".grid",    XrmoptionNoArg, "false" },
  {"-flat",     ".image",   XrmoptionNoArg, "BUILTIN_FLAT" },
  {"-satellite",".image",   XrmoptionNoArg, "BUILTIN_SAT"  },
  {"-image",    ".image",   XrmoptionSepArg, 0 },
  {"-image2",   ".image2",  XrmoptionSepArg, 0 },
  {"-frames",   ".frames",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,       "speed",   "Speed",   DEF_SPEED,   t_Float},
  {&do_roll,	 "roll",    "Roll",    DEF_ROLL,    t_Bool},
  {&do_wander,	 "wander",  "Wander",  DEF_WANDER,  t_Bool},
  {&do_texture,	 "texture", "Texture", DEF_TEXTURE, t_Bool},
  {&do_stars,	 "stars",   "Stars",   DEF_STARS,   t_Bool},
  {&do_grid,	 "grid",    "Grid",    DEF_GRID,    t_Bool},
  {&which_image, "image",   "Image",   DEF_IMAGE,   t_String},
  {&which_image2,"image2",  "Image2",  DEF_IMAGE2,  t_String},
  {&frames,      "frames",  "Frames",  DEF_FRAMES,  t_Int},
};

ENTRYPOINT ModeSpecOpt planet_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   planet_description =
{"planet", "init_planet", "draw_planet", NULL,
 "draw_planet", "init_planet", "free_planet", &planet_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Buckminster Fuller's unwrapped icosahedral globe", 0, NULL};
#endif

# ifdef __GNUC__
  __extension__	 /* don't warn about "string length is greater than the length
		    ISO C89 compilers are required to support" when including
		    the following XPM file... */
# endif

#include "images/gen/earth_flat_png.h"
#include "images/gen/earth_png.h"
#include "images/gen/earth_night_png.h"
#include "images/gen/ground_png.h"

#include "ximage-loader.h"
#include "rotator.h"
#include "gltrackball.h"


typedef struct {
  GLXContext *glx_context;
  GLuint starlist;
  int starcount;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  enum { STARTUP, FLAT, FOLD, 
         ICO, STEL_IN, AXIS, SPIN, STEL, STEL_OUT,
         ICO2, UNFOLD } state;
  GLfloat ratio;
  GLuint tex1, tex2;
  texture_font_data *font_data;
  int loading_sw, loading_sh;

  XImage *day, *night, *dusk, *cvt;
  XImage **images;  /* One image for each frame of time-of-day. */
  int nimages;

  double current_frame;
  Bool cache_p;
  long delay;

} planetstruct;


static planetstruct *planets = NULL;


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


/* Draw faint latitude and longitude lines into the RGBA XImage.
 */
static void
add_grid_lines (XImage *image)
{
  int i;
  int off = 24;
  for (i = 0; i < 24; i++)
    {
      int x = (i + 0.5) * image->width / (double) 24;
      int y;
      for (y = 0; y < image->height; y++)
        {
          unsigned long rgba = XGetPixel (image, x, y);
          int r = (rgba >> 24) & 0xFF;
          int g = (rgba >> 16) & 0xFF;
          int b = (rgba >>  8) & 0xFF;
          int a = (rgba >>  0) & 0xFF;
          int off2 = (((r + g + b) / 3) < 0x7F ? off : -off);
          r = MAX (0, MIN (0xFF, r + off2));
          g = MAX (0, MIN (0xFF, g + off2));
          b = MAX (0, MIN (0xFF, b + off2));
          XPutPixel (image, x, y, (r << 24) | (g << 16) | (b << 8) | a);
        }
    }

  for (i = 1; i < 11; i++)
    {
      int y = i * image->height / (double) 12;
      int x;
      for (x = 0; x < image->width; x++)
        {
          unsigned long rgba = XGetPixel (image, x, y);
          int r = (rgba >> 24) & 0xFF;
          int g = (rgba >> 16) & 0xFF;
          int b = (rgba >>  8) & 0xFF;
          int a = (rgba >>  0) & 0xFF;
          int off2 = (((r + g + b) / 3) < 0x7F ? off : -off);
          r = MAX (0, MIN (0xFF, r + off2));
          g = MAX (0, MIN (0xFF, g + off2));
          b = MAX (0, MIN (0xFF, b + off2));
          XPutPixel (image, x, y, (r << 24) | (g << 16) | (b << 8) | a);
        }
    }
}


static void
adjust_brightness (XImage *image, double amount)
{
  uint32_t *in = (uint32_t *) image->data;
  int i;
  int end = image->height * image->bytes_per_line / 4;
  for (i = 0; i < end; i++)
    {
      uint32_t p = *in;
      /* #### Why is this ABGR instead of RGBA? */
      uint32_t a = (p >> 24) & 0xFF;
      uint32_t g = (p >> 16) & 0xFF;
      uint32_t b = (p >>  8) & 0xFF;
      uint32_t r = (p >>  0) & 0xFF;
      r = MAX(0, MIN(0xFF, (long) (r * amount)));
      g = MAX(0, MIN(0xFF, (long) (g * amount)));
      b = MAX(0, MIN(0xFF, (long) (b * amount)));
      p = (a << 24) | (g << 16) | (b << 8) | r;
      *in++ = p;
    }
}


static GLfloat
vector_angle (XYZ a, XYZ b)
{
  double La = sqrt (a.x*a.x + a.y*a.y + a.z*a.z);
  double Lb = sqrt (b.x*b.x + b.y*b.y + b.z*b.z);
  double cc, angle;

  if (La == 0 || Lb == 0) return 0;
  if (a.x == b.x && a.y == b.y && a.z == b.z) return 0;

  /* dot product of two vectors is defined as:
       La * Lb * cos(angle between vectors)
     and is also defined as:
       ax*bx + ay*by + az*bz
     so:
      La * Lb * cos(angle) = ax*bx + ay*by + az*bz
      cos(angle)  = (ax*bx + ay*by + az*bz) / (La * Lb)
      angle = acos ((ax*bx + ay*by + az*bz) / (La * Lb));
  */
  cc = (a.x*b.x + a.y*b.y + a.z*b.z) / (La * Lb);
  if (cc > 1) cc = 1;  /* avoid fp rounding error (1.000001 => sqrt error) */
  angle = acos (cc);

  return (angle);
}


/* Creates a grayscale image encoding the day/night terminator for a random
   axial tilt.
 */
static XImage *
create_daylight_mask (Display *dpy, Visual *v, int w, int h)
{
  XImage *image = XCreateImage (dpy, v, 8, ZPixmap, 0, 0, w, h, 8, 0);
  int x, y;
# undef sun /* Doh */
  XYZ sun;
  double axial_tilt = frand(23.4) / (180/M_PI) * RANDSIGN();
  double dusk = M_PI * 0.035;
  sun.x = 0;
  sun.y = cos (axial_tilt);
  sun.z = sin (axial_tilt);

  image->data = (char *) malloc (image->height * image->bytes_per_line);

  for (y = 0; y < image->height; y++)
    {
      double lat = -M_PI_2 + (M_PI * (y / (double) image->height));
      double cosL = cos(lat);
      double sinL = sin(lat);
      for (x = 0; x < image->width; x++)
        {
          double lon = -M_PI_2 + (M_PI * 2 * (x / (double) image->width));
          XYZ v;
          double a;
          unsigned long p;
          v.x = cos(lon) * cosL;
          v.y = sin(lon) * cosL;
          v.z = sinL;
          a = vector_angle (sun, v);
          a -= M_PI_2;
          a = (a < -dusk ? 1 : a >= dusk ? 0 : (dusk - a) / (dusk * 2));
          p = 0xFF & (unsigned long) (a * 0xFF);
          XPutPixel (image, x, y, p);
        }
    }
  return image;
}


static void
load_images (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i;

  if (which_image && !strcmp (which_image, "BUILTIN_FLAT"))
    {
      which_image  = strdup("BUILTIN_FLAT");
      which_image2 = strdup("BUILTIN_FLAT");
    }
  else if (which_image && !strcmp (which_image, "BUILTIN_SAT"))
    {
      which_image  = strdup("BUILTIN_DAY");
      which_image2 = strdup("BUILTIN_NIGHT");
    }

  if (!which_image)  which_image  = strdup("");
  if (!which_image2) which_image2 = strdup("");

  for (i = 0; i < 2; i++)
    {
      char *s = (i == 0 ? which_image : which_image2);
      XImage *image;
      if (!strcmp (s, "BUILTIN_DAY"))
        image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                      earth_png, sizeof(earth_png));
      else if (!strcmp (s, "BUILTIN_NIGHT"))
        image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                      earth_night_png,sizeof(earth_night_png));
      else if (!strcmp (s, "BUILTIN") ||
               !strcmp (s, "BUILTIN_FLAT") ||
               (i == 0 && !strcmp (s, "")))
        image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                      earth_flat_png, sizeof(earth_flat_png));
      else if (!strcmp (s, "NONE"))
        image = 0;
      else if (*s)
        image = file_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi), s);
      else
        image = 0;

      /* if (image) fprintf (stderr, "%s: %d: loaded %s\n", progname, i, s); */

      if (i == 0)
        gp->day = image;
      else
        gp->night = image;
    }

  if (gp->night && !gp->day) 
    gp->day = gp->night, gp->night = 0;

  gp->nimages = frames;
  gp->current_frame = random() % gp->nimages;

  if (gp->nimages < 1)
    gp->nimages = 1;

  if (gp->nimages < 2 && gp->night)
    {
      XDestroyImage (gp->night);
      gp->night = 0;
      gp->nimages = 1;
    }

  if (! gp->night)
    gp->nimages = 1;

  if (do_grid)
    {
      if (gp->day)   add_grid_lines (gp->day);
      if (gp->night) add_grid_lines (gp->night);
    }

  if (gp->day && gp->night && !gp->dusk)
    {
      if (gp->day->width  != gp->night->width ||
          gp->day->height != gp->night->height)
        {
          fprintf (stderr, "%s: day and night images must be the same size"
                   " (%dx%d vs %dx%d)\n", progname,
                   gp->day->width, gp->day->height,
                   gp->night->width, gp->night->height);
          exit (1);
        }
      gp->dusk = create_daylight_mask (MI_DISPLAY (mi), MI_VISUAL (mi),
                                       gp->day->width, gp->day->height);
    }

  /* Make the day image brighter, because that's easier than doing it
     with GL lights. */
  adjust_brightness (gp->day, 1.4);

  if (!strcmp (which_image, which_image2))
    /* If day and night are the same image, make night way darker. */
    adjust_brightness (gp->night, 0.2);
  else if (gp->night)
    /* Otherwise make it just a little darker. */
    adjust_brightness (gp->night, 0.7);


  gp->images = (XImage **) calloc (gp->nimages, sizeof(*gp->images));

  /* Create 'cvt', a map that projects each pixel from Equirectangular to
     Dymaxion.  It is 2x the width/height of the source images. We iterate
     by half pixel to make sure we hit every pixel in 'out'. It would be
     cleaner to iterate over 'out' instead of over 'in' but
     dymaxionmap-coords.c only goes forward. This is... not super fast.
   */
  {
    double W = 5.5;
    double H = 3 * sqrt(3)/2;
    int x2, y2;
    int w = gp->day->width;
    int h = gp->day->height;
    uint32_t *out;

    gp->cvt = XCreateImage (MI_DISPLAY(mi), MI_VISUAL(mi), 32, ZPixmap, 0, 0,
                            gp->day->width*2, gp->day->height*2, 32, 0);
    gp->cvt->data = (char *)
      malloc (gp->cvt->height * gp->cvt->bytes_per_line);
    out = (uint32_t *) gp->cvt->data;

    for (y2 = 0; y2 < h*2; y2++)
      {
        double y = (double) y2/2;
        double lat = -90 + (180 * (y / (double) h));
        for (x2 = 0; x2 < w*2; x2++)
          {
            double x = (double) x2/2;
            double lon = -180 + (360 * x / w);
            double ox, oy;
            dymaxion_convert (lon, lat, &ox, &oy);
            ox = w - (w * ox / W);
            oy =     (h * oy / H);

            *out++ = (((((uint32_t) ox) & 0xFFFF) << 16) |
                      ((((uint32_t) oy) & 0xFFFF)));
          }
      }
  }

  /* A 128 GB iPhone 6s dies at around 540 frames, ~1 GB of XImages.
     A 16 GB iPad Air 2 dies at around 320 frames, ~640 MB.
     Caching on mobile doesn't matter much: we can just run at 100% CPU.

     On some systems it would be more efficient to cache the images inside
     a texture on the GPU instead of moving it from RAM to GPU every few
     frames; but on other systems, we'd just run out of GPU memory instead. */
  {
    unsigned long cache_size = (gp->day->width * gp->day->height * 4 *
                                gp->nimages);
# ifdef HAVE_MOBILE
    unsigned long max = 320 * 1024 * 1024L;		/* 320 MB */
# else
  /*unsigned long max = 2 * 1024 * 1024 * 1024L;*/	/* 2 GB */
    unsigned long max = 0x80000000L;			/* 2 GB */
# endif
    gp->cache_p = (cache_size < max);
  }
}


static void
cache_current_frame (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  XImage *blended, *dymaxion;
  int i, x, y, w, h, end, xoff;
  uint32_t *day, *night, *cvt, *out;
  uint8_t *dusk;

  if (gp->images[(int) gp->current_frame])
    return;

  xoff = (gp->dusk
          ? gp->dusk->width * ((double) gp->current_frame / gp->nimages)
          : 0);

  w = gp->day->width;
  h = gp->day->height;

  if (!gp->night)
    blended = gp->day;
  else
    {
      /* Blend the foreground and background images through the dusk map.
       */
      blended = XCreateImage (MI_DISPLAY(mi), MI_VISUAL(mi), 
                              gp->day->depth, ZPixmap, 0, 0, w, h, 32, 0);
      if (!blended) abort();
      blended->data = (char *) malloc (h * blended->bytes_per_line);
      if (!blended->data) abort();

      end = blended->height * blended->bytes_per_line / 4;
      day   = (uint32_t *) gp->day->data;
      night = (uint32_t *) gp->night->data;
      dusk  = (uint8_t  *) gp->dusk->data;
      out   = (uint32_t *) blended->data;

      for (i = 0; i < end; i++)
        {
          uint32_t d = *day++;
          uint32_t n = *night++;
          uint32_t x = i % w;
          uint32_t y = i / w;
          double r = dusk[y * w + ((x + xoff) % w)] / 256.0;
          double r2 = 1-r;
# define ADD(M) (((unsigned long)             \
                  ((((d >> M) & 0xFF) * r) +  \
                   (((n >> M) & 0xFF) * r2))) \
                 << M)
          /* #### Why is this ABGR instead of RGBA? */
          *out++ = (0xFF << 24) | ADD(16) | ADD(8) | ADD(0);
# undef ADD
        }
    }

  /* Convert blended Equirectangular to Dymaxion through the 'cvt' map.
   */
  dymaxion = XCreateImage (MI_DISPLAY(mi), MI_VISUAL(mi), 
                          gp->day->depth, ZPixmap, 0, 0, w, h, 32, 0);
  dymaxion->data = (char *) calloc (h, dymaxion->bytes_per_line);
  
  day = (uint32_t *) blended->data;
  out = (uint32_t *) dymaxion->data;
  cvt = (uint32_t *) gp->cvt->data;

  for (y = 0; y < h*2; y++)
    for (x = 0; x < w*2; x++)
      {
        unsigned long m  = *cvt++;
        unsigned long dx = (m >> 16) & 0xFFFF;
        unsigned long dy = m & 0xFFFF;
        unsigned long p  = day[(y>>1) * w + (x>>1)];
        unsigned long p2 = out[dy * w + dx];
        if (p2 & 0xFF000000)
          /* RGBA nonzero alpha: initialized. Average with existing,
             otherwise the grid lines look terrible. */
          p = (((((p>>24) & 0xFF) + ((p2>>24) & 0xFF)) >> 1) << 24 |
               ((((p>>16) & 0xFF) + ((p2>>16) & 0xFF)) >> 1) << 16 |
               ((((p>> 8) & 0xFF) + ((p2>> 8) & 0xFF)) >> 1) <<  8 |
               ((((p>> 0) & 0xFF) + ((p2>> 0) & 0xFF)) >> 1) <<  0);
        out[dy * w + dx] = p;
      }

  /* Fill in the triangles that are not a part of The World with the
     color of the ocean to avoid texture-tearing on the folded edges.
  */
  out = (uint32_t *) dymaxion->data;
  end = dymaxion->height * dymaxion->bytes_per_line / 4;
  {
    double lat = -48.44, lon = -123.39;   /* R'Lyeh */
    int x = (lon + 180) * blended->width  / 360.0;
    int y = (lat + 90)  * blended->height / 180.0;
    unsigned long ocean = XGetPixel (gp->day, x, y);
    for (i = 0; i < end; i++)
      {
        uint32_t p = *out;
        if (! (p & 0xFF000000)) /* AGBR */
          *out = ocean;
        out++;
      }
  }

  if (blended != gp->day)
    XDestroyImage (blended);

  gp->images[(int) gp->current_frame] = dymaxion;

  if (!gp->cache_p)  /* Keep only one image around; recompute every time. */
    {
      i = ((int) gp->current_frame) - 1;
      if (i < 0) i = gp->nimages - 1;
      if (gp->images[i])
        {
          XDestroyImage (gp->images[i]);
          gp->images[i] = 0;
        }
    }
}


static void
setup_texture (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  XImage *ground;

  glGenTextures (1, &gp->tex1);
  glBindTexture (GL_TEXTURE_2D, gp->tex1);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  load_images (mi);

  glGenTextures (1, &gp->tex2);
  glBindTexture (GL_TEXTURE_2D, gp->tex2);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  /* The underground image can go on flat, without the dymaxion transform. */
  ground = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                 ground_png, sizeof(ground_png));
  clear_gl_error();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  /* glPixelStorei(GL_UNPACK_ROW_LENGTH, ground->width); */
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                ground->width, ground->height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, ground->data);
  check_gl_error ("ground texture");
  XDestroyImage (ground);
}


static void
init_stars (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i, j;
  int width  = MI_WIDTH(mi);
  int height = MI_HEIGHT(mi);
  int size = (width > height ? width : height);
  int nstars = size * size / 80;
  int max_size = 3;
  GLfloat inc = 0.5;
  int steps = max_size / inc;
  GLfloat scale = 1;

  if (MI_WIDTH(mi) > 2560) {  /* Retina displays */
    scale *= 2;
    nstars /= 2;
  }

  gp->starlist = glGenLists(1);
  glNewList(gp->starlist, GL_COMPILE);
  for (j = 1; j <= steps; j++)
    {
      glPointSize(inc * j * scale);
      glBegin (GL_POINTS);
      for (i = 0; i < nstars / steps; i++)
	{
	  GLfloat d = 0.1;
	  GLfloat r = 0.15 + frand(0.3);
	  GLfloat g = r + frand(d) - d;
	  GLfloat b = r + frand(d) - d;

	  GLfloat x = frand(1)-0.5;
	  GLfloat y = frand(1)-0.5;
	  GLfloat z = ((random() & 1)
		       ? frand(1)-0.5
		       : (BELLRAND(1)-0.5)/12);	  /* milky way */
	  d = sqrt (x*x + y*y + z*z);
	  x /= d;
	  y /= d;
	  z /= d;
	  glColor3f (r, g, b);
	  glVertex3f (x, y, z);
	  gp->starcount++;
	}
      glEnd ();
    }
  glEndList ();

  check_gl_error("stars initialization");
}


ENTRYPOINT void
reshape_planet (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (h, h, h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


static void
do_normal2 (ModeInfo *mi, Bool frontp, XYZ a, XYZ b, XYZ c)
{
  XYZ n = (frontp
           ? calc_normal (a, b, c)
           : calc_normal (b, a, c));
  glNormal3f (n.x, n.y, n.z);

# if 0
  if (frontp && MI_IS_WIREFRAME(mi))
    {
      glBegin (GL_LINES);
      glVertex3f ((a.x + b.x + c.x) / 3,
                  (a.y + b.y + c.y) / 3,
                  (a.z + b.z + c.z) / 3);
      glVertex3f ((a.x + b.x + c.x) / 3 + n.x,
                  (a.y + b.y + c.y) / 3 + n.y,
                  (a.z + b.z + c.z) / 3 + n.z);
      glEnd();
    }
# endif
}


static void
triangle0 (ModeInfo *mi, Bool frontp, GLfloat stel_ratio, int facemask, 
           XYZ *corners_ret)
{
  /* Render a triangle as six sub-triangles.
     Facemask bits 0-5 indicate which sub-triangle to draw.

                A
               / \
              / | \
             /  |  \
            / 0 | 1 \
        E  /_   |   _\  F
          /  \_ | _/  \
         / 5   \D/   2 \
        /    /  |  \    \
       /   / 4  | 3  \   \
      /  /      |       \ \
   B ----------------------- C
                G
   */

  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat h = sqrt(3) / 2;
  GLfloat h2 = sqrt(h*h - (h/2)*(h/2)) - 0.5;
  XYZ  A,  B,  C,  D,  E,  F,  G;
  XYZ tA, tB, tC, tD, tE, tF, tG;
  XYZ  a,  b,  c;
  XYZ ta, tb, tc;
  A.x =  0;   A.y = h;   A.z = 0;
  B.x = -0.5, B.y = 0;   B.z = 0;
  C.x =  0.5, C.y = 0;   C.z = 0;
  D.x =  0;   D.y = h/3; D.z = 0;
  E.x = -h2;  E.y = h/2; E.z = 0;
  F.x =  h2;  F.y = h/2; F.z = 0;
  G.x =  0;   G.y = 0;   G.z = 0;

  /* When tweaking object XY to stellate, don't change texture coordinates. */
  tA = A; tB = B; tC = C; tD = D; tE = E; tF = F; tG = G;

  /* Eyeballed this to find the depth of stellation that seems to most
     approximate a sphere.
   */
  D.z = 0.193 * stel_ratio;

  /* We want to raise E, F and G as well but we can't just shift Z:
     we need to keep them on the same vector from the center of the sphere,
     which means also changing F and G's X and Y.
  */
  E.z = F.z = G.z = 0.132 * stel_ratio;
  {
    double magic_x = 0.044;
    double magic_y = 0.028;
    /* G.x stays 0 */
    G.y -= sqrt (magic_x*magic_x + magic_y*magic_y) * stel_ratio;
    E.x -= magic_x * stel_ratio;
    E.y += magic_y * stel_ratio;
    F.x += magic_x * stel_ratio;
    F.y += magic_y * stel_ratio;
  }


  if (facemask & 1<<0)
    {
      a  =  E;  b =  D;  c =  A;
      ta = tE; tb = tD; tc = tA;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (facemask & 1<<1)
    {
      a  =  D;  b =  F;  c =  A;
      ta = tD; tb = tF; tc = tA;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (facemask & 1<<2)
    {
      a  =  D;  b =  C;  c =  F;
      ta = tD; tb = tC; tc = tF;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (facemask & 1<<3)
    {
      a  =  G;  b =  C;  c =  D;
      ta = tG; tb = tC; tc = tD;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (facemask & 1<<4)
    {
      a  =  B;  b =  G;  c =  D;
      ta = tB; tb = tG; tc = tD;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (facemask & 1<<5)
    {
      a  =  B;  b =  D;  c =  E;
      ta = tB; tb = tD; tc = tE;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (facemask & 1<<6)
    {
      a  =  E;  b =  D;  c =  A;
      ta = tE; tb = tD; tc = tA;
      do_normal2 (mi, frontp, a, b, c);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }

  if (corners_ret)
    {
      corners_ret[0] = A;
      corners_ret[1] = B;
      corners_ret[2] = C;
      corners_ret[3] = D;
      corners_ret[4] = E;
      corners_ret[5] = F;
      corners_ret[6] = G;
    }
}


/* The segments, numbered arbitrarily from the top left:
             ________         _      ________
             \      /\      /\ \    |\      /
              \ 0  /  \    /  \3>   | \ 5  /
               \  / 1  \  / 2  \| ..|4 \  /-6-..
     ___________\/______\/______\/______\/______\
    |   /\      /\      /\      /\      /\   
    |7 /  \ 9  /  \ 11 /  \ 13 /  \ 15 /  \  
    | / 8  \  / 10 \  / 12 \  / 14 \  / 16 \ 
    |/______\/______\/______\/______\/______\
     \      /\      /       /\      /\
      \ 17 /  \ 18 /       /  \ 20 /  \
       \  /    \  /       / 19 \  / 21 \
        \/      \/       /______\/______\

   Each triangle can be connected to at most two other triangles.
   We start from the middle, #12, and work our way to the edges.
   Its centroid is 0,0.

   (Note that dymaxionmap-coords.c uses a different numbering system.)
 */
static void
triangle (ModeInfo *mi, int which, Bool frontp, 
          GLfloat fold_ratio, GLfloat stel_ratio)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  const GLfloat fg[] = { 1, 1, 1, 1 };
  const GLfloat bg[] = { 0.3, 0.3, 0.3, 1 };
  int a = -1, b = -1;
  GLfloat max = acos (sqrt(5)/3);
  GLfloat rot = -max * fold_ratio / (M_PI/180);
  Bool wire = MI_IS_WIREFRAME(mi);
  XYZ corners[7];

  glColor3fv (fg);
  if (!wire)
    glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, fg);

  switch (which) {
  case 3:				/* One third of the face. */
    triangle0 (mi, frontp, stel_ratio, 1<<3 | 1<<4, corners);
    break;
  case 4:				/* Two thirds of the face: convex. */
    triangle0 (mi, frontp, stel_ratio, 1<<1 | 1<<2 | 1<<3 | 1<<4, corners);
    break;
  case 6:				/* One half of the face. */
    triangle0 (mi, frontp, stel_ratio, 1<<1 | 1<<2 | 1<<3, corners);
    break;
  case 7:				/* One half of the face. */
    triangle0 (mi, frontp, stel_ratio, 1<<2 | 1<<3 | 1<<4, corners);
    break;
  default:				/* Full face. */
    triangle0 (mi, frontp, stel_ratio, 0x3F, corners);
    break;
  }

  if (wire)
    {
      char tag[20];
      glColor3fv (bg);
      sprintf (tag, "%d", which);
      glPushMatrix();
      glTranslatef (-0.1, 0.2, 0);
      glScalef (0.005, 0.005, 0.005);
      print_texture_string (gp->font_data, tag);
      glPopMatrix();
      mi->polygon_count++;
    }


  /* The connection hierarchy of the faces starting at the middle, #12. */
  switch (which) {
  case  0: break;
  case  1: a =  0; b = -1; break;
  case  2: a = -1; b =  3; break;
  case  3: break;
  case  4: a = -1; b =  5; break;
  case  5: a = -1; b =  6; break;
  case  7: break;
  case  6: break;
  case  8: a = 17; b =  7; break;
  case  9: a =  8; b = -1; break;
  case 10: a = 18; b =  9; break;
  case 11: a = 10; b =  1; break;
  case 12: a = 11; b = 13; break;
  case 13: a =  2; b = 14; break;
  case 14: a = 15; b = 20; break;
  case 15: a =  4; b = 16; break;
  case 16: break;
  case 17: break;
  case 18: break;
  case 19: break;
  case 20: a = 21; b = 19; break;
  case 21: break;
  default: abort(); break;
  }

  if (a != -1)
    {
      glPushMatrix();
      glTranslatef (-0.5, 0, 0);	/* Move model matrix to upper left */
      glRotatef (60, 0, 0, 1);
      glTranslatef ( 0.5, 0, 0);

      glMatrixMode(GL_TEXTURE);
      /* glPushMatrix(); */
      glTranslatef (-0.5, 0, 0);	/* Move texture matrix the same way */
      glRotatef (60, 0, 0, 1);
      glTranslatef ( 0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);

      glRotatef (rot, 1, 0, 0);
      triangle (mi, a, frontp, fold_ratio, stel_ratio);

      /* This should just be a PopMatrix on the TEXTURE stack, but
         fucking iOS has GL_MAX_TEXTURE_STACK_DEPTH == 4!  WTF!
         So we have to undo our rotations and translations manually.
       */
      glMatrixMode(GL_TEXTURE);
      /* glPopMatrix(); */
      glTranslatef (-0.5, 0, 0);
      glRotatef (-60, 0, 0, 1);
      glTranslatef (0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
    }

  if (b != -1)
    {
      glPushMatrix();
      glTranslatef (0.5, 0, 0);		/* Move model matrix to upper right */
      glRotatef (-60, 0, 0, 1);
      glTranslatef (-0.5, 0, 0);

      glMatrixMode(GL_TEXTURE);
      /* glPushMatrix(); */
      glTranslatef (0.5, 0, 0);		/* Move texture matrix the same way */
      glRotatef (-60, 0, 0, 1);
      glTranslatef (-0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);

      glRotatef (rot, 1, 0, 0);
      triangle (mi, b, frontp, fold_ratio, stel_ratio);

      /* See above. Grr. */
      glMatrixMode(GL_TEXTURE);
      /* glPopMatrix(); */
      glTranslatef (0.5, 0, 0);
      glRotatef (60, 0, 0, 1);
      glTranslatef (-0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
    }


  /* Draw a border around the edge of the world.
   */
  if (!wire && frontp && stel_ratio == 0 && fold_ratio < 0.95)
    {
      int edges = 0;
      GLfloat c[] = { 0, 0.2, 0.5, 1 };
      c[3] = 1-fold_ratio;

      switch (which)
        {
        case  0: edges = 1<<0 | 1<<2; break;
        case  1: edges = 1<<2;        break;
        case  2: edges = 1<<0;        break;
        case  3: edges = 1<<3 | 1<<4; break;
        case  4: edges = 1<<3 | 1<<5; break;
        case  5: edges = 1<<0 | 1<<6; break;
        case  6: edges = 1<<2 | 1<<7; break;
        case 16: edges = 1<<0 | 1<<2; break;
        case 21: edges = 1<<0 | 1<<2; break;
        case 19: edges = 1<<0 | 1<<2; break;
        case 12: edges = 1<<1;        break;
        case 18: edges = 1<<0 | 1<<2; break;
        case 17: edges = 1<<0 | 1<<2; break;
        case  7: edges = 1<<8 | 1<<9; break;
        case  9: edges = 1<<2;        break;
        default: break;
        }

      glDisable (GL_TEXTURE_2D);
      glDisable (GL_LIGHTING);
      glLineWidth (2);
      glColor4fv (c);
      glBegin (GL_LINES);
      if (edges & 1<<0)
        {
          glVertex3f (corners[0].x, corners[0].y, corners[0].z);
          glVertex3f (corners[1].x, corners[1].y, corners[1].z);
        }
      if (edges & 1<<1)
        {
          glVertex3f (corners[1].x, corners[1].y, corners[1].z);
          glVertex3f (corners[2].x, corners[2].y, corners[2].z);
        }
      if (edges & 1<<2)
        {
          glVertex3f (corners[2].x, corners[2].y, corners[2].z);
          glVertex3f (corners[0].x, corners[0].y, corners[0].z);
        }
      if (edges & 1<<3)
        {
          glVertex3f (corners[1].x, corners[1].y, corners[1].z);
          glVertex3f (corners[3].x, corners[3].y, corners[3].z);
        }
      if (edges & 1<<4)
        {
          glVertex3f (corners[3].x, corners[3].y, corners[3].z);
          glVertex3f (corners[2].x, corners[2].y, corners[2].z);
        }
      if (edges & 1<<5)
        {
          glVertex3f (corners[3].x, corners[3].y, corners[3].z);
          glVertex3f (corners[0].x, corners[0].y, corners[0].z);
        }
      if (edges & 1<<6)
        {
          glVertex3f (corners[0].x, corners[0].y, corners[0].z);
          glVertex3f (corners[5].x, corners[5].y, corners[5].z);
        }
      if (edges & 1<<7)
        {
          glVertex3f (corners[0].x, corners[0].y, corners[0].z);
          glVertex3f (corners[6].x, corners[6].y, corners[6].z);
        }
      if (edges & 1<<8)
        {
          glVertex3f (corners[1].x, corners[1].y, corners[1].z);
          glVertex3f (corners[5].x, corners[5].y, corners[5].z);
        }
      if (edges & 1<<9)
        {
          glVertex3f (corners[5].x, corners[5].y, corners[5].z);
          glVertex3f (corners[2].x, corners[2].y, corners[2].z);
        }
      glEnd();
      glEnable (GL_TEXTURE_2D);
      glEnable (GL_LIGHTING);
    }
}


static void
draw_triangles (ModeInfo *mi, GLfloat fold_ratio, GLfloat stel_ratio)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat h = sqrt(3) / 2;
  GLfloat c = h / 3;

  glTranslatef (0, -h/3, 0);  /* Center on face 12 */

  /* When closed, center on midpoint of icosahedron. Eyeballed this. */
  glTranslatef (0, 0, fold_ratio * 0.754);

  glFrontFace (GL_CCW);

  /* Adjust the texture matrix so that it has the same coordinate space
     as the model. */

  glMatrixMode(GL_TEXTURE);
  glPushMatrix();
  {
    GLfloat texw = 5.5;
    GLfloat texh = 3 * h;
    GLfloat midx = 2.5;
    GLfloat midy = 3 * c;
    glScalef (1/texw, -1/texh, 1);
    glTranslatef (midx, midy, 0);
  }
  glMatrixMode(GL_MODELVIEW);



  /* Front faces */

  if (wire)
    glDisable (GL_TEXTURE_2D);
  else if (do_texture)
    {
      glEnable (GL_TEXTURE_2D);
      glBindTexture (GL_TEXTURE_2D, gp->tex1);
    }
  else
    glDisable (GL_TEXTURE_2D);

  triangle (mi, 12, True, fold_ratio, stel_ratio);

  /* Back faces */

  if (wire)
    glDisable (GL_TEXTURE_2D);
  else if (do_texture)
    {
      glEnable (GL_TEXTURE_2D);
      glBindTexture (GL_TEXTURE_2D, gp->tex2);
    }
  else
    glDisable (GL_TEXTURE_2D);

  glFrontFace (GL_CW);

  triangle (mi, 12, False, fold_ratio, 0);

  glMatrixMode(GL_TEXTURE);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}


static void
align_axis (ModeInfo *mi, int undo)
{
  /* Rotate so that an axis is lined up with the north and south poles
     on the map, which are not in the center of their faces, or any
     other easily computable spot. */

  GLfloat r1 = 20.5;
  GLfloat r2 = 28.5;

  if (undo)
    {
      glRotatef (-r2, 0, 1, 0);
      glRotatef ( r2, 1, 0, 0);
      glRotatef (-r1, 1, 0, 0);
    }
  else
    {
      glRotatef (r1, 1, 0, 0);
      glRotatef (-r2, 1, 0, 0);
      glRotatef ( r2, 0, 1, 0);
    }
}


static void
draw_axis (ModeInfo *mi)
{
  GLfloat s;
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);
  glPushMatrix();

  align_axis (mi, 0);
  glTranslatef (0.34, 0.39, -0.61);

  s = 0.96;
  glScalef (s, s, s);   /* tighten up the enclosing sphere */

  glLineWidth (1);
  glColor3f (0.5, 0.5, 0);

  glRotatef (90,  1, 0, 0);    /* unit_sphere is off by 90 */
  glRotatef (9.5, 0, 1, 0);    /* line up the time zones */
  glFrontFace (GL_CCW);
  unit_sphere (12, 24, True);
  glBegin(GL_LINES);
  glVertex3f(0, -2, 0);
  glVertex3f(0,  2, 0);
  glEnd();

  glPopMatrix();
}




ENTRYPOINT Bool
planet_handle_event (ModeInfo *mi, XEvent *event)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, gp->trackball,
				 MI_WIDTH (mi), MI_HEIGHT (mi),
				 &gp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
          int i;
          double cf = gp->current_frame;

          /* Switch between the satellite and flat map, preserving position. */
          if (gp->day)   XDestroyImage (gp->day);
          if (gp->night) XDestroyImage (gp->night);
          if (gp->cvt)   XDestroyImage (gp->cvt);
          gp->day    = 0;
          gp->night  = 0;
          gp->cvt    = 0;

          for (i = 0; i < gp->nimages; i++)
            if (gp->images[i]) XDestroyImage (gp->images[i]);
          free (gp->images);
          gp->images = 0;

          which_image  = strdup (!strcmp (which_image, "BUILTIN_DAY")
                                 ? "BUILTIN_FLAT" : "BUILTIN_DAY");
          which_image2 = strdup (!strcmp (which_image2, "BUILTIN_NIGHT")
                                 ? "BUILTIN_FLAT" : "BUILTIN_NIGHT");
          load_images (mi);
          gp->current_frame = cf;
# if 0
          switch (gp->state) {
          case FLAT: case ICO: case STEL: case AXIS: case ICO2:
            gp->ratio = 1;
            break;
          default:
            break;
          }
# endif
          return True;
        }
    }

  return False;
}


ENTRYPOINT void
init_planet (ModeInfo * mi)
{
  planetstruct *gp;
  int screen = MI_SCREEN(mi);
  Bool wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, planets);
  gp = &planets[screen];

  if ((gp->glx_context = init_GL(mi)) != NULL) {
    reshape_planet(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  gp->state = STARTUP;
  gp->ratio = 0;
  gp->font_data = load_texture_font (mi->dpy, "labelFont");
  gp->delay = MI_DELAY(mi);

  {
    double spin_speed	= 0.1;
    double wander_speed = 0.002;
    gp->rot = make_rotator (do_roll ? spin_speed : 0,
			    do_roll ? spin_speed : 0,
			    0, 1,
			    do_wander ? wander_speed : 0,
			    False);
    gp->rot2 = make_rotator (0, 0, 0, 0, wander_speed, False);
    gp->trackball = gltrackball_init (True);
  }

  if (wire)
    do_texture = False;

  if (do_texture)
    setup_texture (mi);

  if (do_stars)
    init_stars (mi);

  glEnable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {1, 1, 1, 0};
      GLfloat amb[4] = {0, 0, 0, 1};
      GLfloat dif[4] = {1, 1, 1, 1};
      GLfloat spc[4] = {0, 1, 1, 1};
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,	amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,	dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }
}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.35;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


ENTRYPOINT void
draw_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  long delay = gp->delay;
  double x, y, z;

  if (!gp->glx_context)
    return;

  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glXMakeCurrent (dpy, window, *gp->glx_context);

  mi->polygon_count = 0;

  if (! gp->button_down_p)
    switch (gp->state) {
    case STARTUP:  gp->ratio += speed * 0.01;  break;
    case FLAT:     gp->ratio += speed * 0.005 *
        /* Stay flat longer if animating day and night. */
        (gp->nimages <= 1 ? 1 : 0.3);
      break;
    case FOLD:     gp->ratio += speed * 0.01;  break;
    case ICO:      gp->ratio += speed * 0.01;  break;
    case STEL_IN:  gp->ratio += speed * 0.05;  break;
    case STEL:     gp->ratio += speed * 0.01;  break;
    case STEL_OUT: gp->ratio += speed * 0.07;  break;
    case ICO2:     gp->ratio += speed * 0.07;  break;
    case AXIS:     gp->ratio += speed * 0.02;  break;
    case SPIN:     gp->ratio += speed * 0.005; break;
    case UNFOLD:   gp->ratio += speed * 0.01;  break;
    default:       abort();
    }

  if (gp->ratio > 1.0)
    {
      gp->ratio = 0;
      switch (gp->state) {
      case STARTUP:  gp->state = FLAT;     break;
      case FLAT:     gp->state = FOLD;     break;
      case FOLD:     gp->state = ICO;      break;
      case ICO:      gp->state = STEL_IN;  break;
      case STEL_IN:  gp->state = STEL;     break;
      case STEL:
        {
          int i = (random() << 9) % 7;
          gp->state = (i < 3 ? STEL_OUT :
                       i < 6 ? SPIN : AXIS);
        }
        break;
      case AXIS:     gp->state = STEL_OUT; break;
      case SPIN:     gp->state = STEL_OUT; break;
      case STEL_OUT: gp->state = ICO2;     break;
      case ICO2:     gp->state = UNFOLD;   break;
      case UNFOLD:   gp->state = FLAT;     break;
      default:       abort();
      }
    }

  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK); 

  glPushMatrix();

  gltrackball_rotate (gp->trackball);
  glRotatef (current_device_rotation(), 0, 0, 1);

# ifdef HAVE_MOBILE   /* Fill more of the screen. */
    {
      int size = MI_WIDTH(mi) < MI_HEIGHT(mi)
        ? MI_WIDTH(mi) : MI_HEIGHT(mi);
      GLfloat s = (size > 768 ? 1.4 :  /* iPad */
                   2);                 /* iPhone */
      glScalef (s, s, s);
      if (MI_WIDTH(mi) < MI_HEIGHT(mi))
        glRotatef (90, 0, 0, 1);
    }
# endif

  if (gp->state != STARTUP)
    {
      get_position (gp->rot, &x, &y, &z, !gp->button_down_p);
      x = (x - 0.5) * 3;
      y = (y - 0.5) * 3;
      z = 0;
      glTranslatef(x, y, z);
    }

  if (do_roll && gp->state != STARTUP)
    {
      double max = 65;
      get_position (gp->rot2, &x, &y, 0, !gp->button_down_p);
      glRotatef (max/2 - x*max, 1, 0, 0);
      glRotatef (max/2 - y*max, 0, 1, 0);
    }

  if (do_stars)
    {
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_LIGHTING);
      glPushMatrix();
      glScalef (60, 60, 60);
      glRotatef (90, 1, 0, 0);
      glRotatef (35, 1, 0, 0);
      glCallList (gp->starlist);
      mi->polygon_count += gp->starcount;
      glPopMatrix();
      glClear(GL_DEPTH_BUFFER_BIT);
    }

  if (! wire)
    {
      glEnable (GL_LIGHTING);
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

  if (do_texture)
    glEnable(GL_TEXTURE_2D);

  if (do_texture /* && !gp->button_down_p */)
    {
      int i;
      int prev = gp->current_frame;

      /* By default, advance terminator by about an hour every 5 seconds. */
      gp->current_frame += 0.1 * speed * (gp->nimages / 360.0);
      while (gp->current_frame >= gp->nimages)
        gp->current_frame -= gp->nimages;
      i = gp->current_frame;

      /* Load the current image into the texture.
       */
      if (i != prev || !gp->images[i])
        {
          double start = double_time();
          cache_current_frame (mi);

          glBindTexture (GL_TEXTURE_2D, gp->tex1);

          /* Must be after glBindTexture */
          glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

          glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                        gp->images[i]->width,
                        gp->images[i]->height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        gp->images[i]->data);
          check_gl_error ("texture");

          /* If caching the image took a bunch of time, deduct that from
             our per-frame delay to keep the timing a little smoother. */
          delay -= 1000000 * (double_time() - start);
          if (delay < 0) delay = 0;
        }
    }

  glTranslatef (-0.5, -0.4, 0);
  glScalef (2.6, 2.6, 2.6);

  {
    GLfloat fold_ratio = 0;
    GLfloat stel_ratio = 0;
    switch (gp->state) {
    case FOLD:     fold_ratio =     gp->ratio; break;
    case UNFOLD:   fold_ratio = 1 - gp->ratio; break;
    case ICO: case ICO2: fold_ratio = 1; break;
    case STEL: case AXIS: case SPIN: fold_ratio = 1; stel_ratio = 1; break;
    case STEL_IN:  fold_ratio = 1; stel_ratio = gp->ratio; break;
    case STEL_OUT: fold_ratio = 1; stel_ratio = 1 - gp->ratio; break;
    case STARTUP:      /* Tilt in from flat */
      glRotatef (-90 * ease_ratio (1 - gp->ratio), 1, 0, 0);
      break;

    default: break;
    }

# ifdef HAVE_MOBILE  /* Enlarge the icosahedron a bit to make it more visible */
    {
      GLfloat s = 1 + 1.3 * ease_ratio (fold_ratio);
      glScalef (s, s, s);
    }
# endif

    if (gp->state == SPIN)
      {
        align_axis (mi, 0);
        glRotatef (ease_ratio (gp->ratio) * 360 * 3, 0, 0, 1);
        align_axis (mi, 1);
      }

    draw_triangles (mi, ease_ratio (fold_ratio), ease_ratio (stel_ratio));

    if (gp->state == AXIS)
      draw_axis(mi);
  }

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);

  MI_DELAY(mi) = delay;
}


ENTRYPOINT void
free_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i;

  if (!gp->glx_context) return;
  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *gp->glx_context);

  if (gp->font_data) free_texture_font (gp->font_data);
  if (gp->trackball) gltrackball_free (gp->trackball);
  if (gp->rot) free_rotator (gp->rot);
  if (gp->rot2) free_rotator (gp->rot2);

  if (gp->day)   XDestroyImage (gp->day);
  if (gp->night) XDestroyImage (gp->night);
  if (gp->dusk)  XDestroyImage (gp->dusk);
  if (gp->cvt)   XDestroyImage (gp->cvt);

  if (gp->images) {
    for (i = 0; i < gp->nimages; i++)
      if (gp->images[i]) XDestroyImage (gp->images[i]);
    free (gp->images);
  }

  if (glIsList(gp->starlist)) glDeleteLists(gp->starlist, 1);
  if (gp->tex1) glDeleteTextures (1, &gp->tex1);
  if (gp->tex2) glDeleteTextures (1, &gp->tex2);
}


XSCREENSAVER_MODULE_2 ("DymaxionMap", dymaxionmap, planet)

#endif
