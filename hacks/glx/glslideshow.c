/* glslideshow, Copyright © 2003-2025 Jamie Zawinski <jwz@jwz.org>
 * Loads a sequence of images and smoothly pans around them; crossfades
 * when loading new images.
 *
 * Originally written by Mike Oliphant <oliphant@gtk.org> (c) 2002, 2003.
 * Rewritten by jwz, 21-Jun-2003.
 * Rewritten by jwz again, 6-Feb-2005.
 * Modified by Richard Weeks <rtweeks21@gmail.com> Copyright (c) 2020
 * Rewritten by jwz again, 27-Nov-2025.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 *****************************************************************************
 *
 *   Image loading happens in three stages:
 *
 *    1: Fork a process and run xscreensaver-getimage in the background.
 *       This writes image data to a server-side X pixmap.
 *
 *    2: When that completes, a callback informs us that the pixmap is ready.
 *       We must then download the pixmap data from the server with XGetImage
 *       (or XShmGetImage.)
 *
 *    3: Once we have the bits, we must convert them from server-native bitmap
 *       layout to 32 bit RGBA in client-endianness, and copy that into an
 *       OpenGL texture.
 *
 *   The speed of step 1 doesn't really matter, since that happens in the
 *   background.  Steps 2 and 3 happen in *this* process.
 *
 *   Step 2 can't be moved to another process without opening a second
 *   connection to the X server, which is pretty heavy-weight.  (That would
 *   be possible, though; the other process could open an X connection,
 *   retrieve the pixmap, and feed it back to us through a pipe or
 *   something.)
 *
 *   Step 3 is carried out over the course of several animation frames.  The
 *   bits are processed in "stripes" small enough to complete within a single
 *   frame.  Each stripe is first converted to a client-endian, 32 bit RGBA
 *   XImage, then copied into the partially completed OpenGL texture.
 *
 *   This entire process is accomplished via a texture loader created with
 *   alloc_texture_loader.
 */

#define DEFAULTS  "*delay:           20000                \n" \
		  "*wireframe:       False                \n" \
                  "*showFPS:         False                \n" \
	          "*fpsSolid:        True                 \n" \
	          "*useSHM:          True                 \n" \
                  "*titleFont: sans-serif 18\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
		  "*grabDesktopImages:   False \n" \
		  "*chooseRandomImages:  True  \n"

# define release_slideshow 0
# include "xlockmore.h"

#ifdef USE_GL


# define DEF_TRANSITION_DURATION "3"
# define DEF_FADE_DURATION       "2"
# define DEF_PAN_DURATION        "6"
# define DEF_IMAGE_DURATION      "30"
# define DEF_ZOOM                "75"
# define DEF_TITLES              "False"
# define DEF_LETTERBOX           "True"
# define DEF_DEBUG               "False"
# define DEF_VERBOSE             "False"
# define DEF_MIPMAP              "True"

#include "grab-ximage.h"
#include "texfont.h"
#include "easing.h"
#include "doubletime.h"

# ifndef HAVE_JWXYZ
#  include <X11/Intrinsic.h>     /* for XrmDatabase in -verbose mode */
# endif

typedef struct {
  double x, y, w, h;
  struct { double x, y, z; } r;
} rect;

typedef struct {
  ModeInfo *mi;
  int id;			   /* unique number for debugging */
  char *title;			   /* the filename of this image */
  int w, h;			   /* size in pixels of the image */
  int tw, th;			   /* size in pixels of the texture */
  XRectangle geom;		   /* where in the image the bits are */
  Bool loaded_p;		   /* whether the image has finished loading */
  Bool used_p;			   /* whether the image has yet appeared
                                      on screen */
  GLuint texid;			   /* which texture contains the image */
  int refcount;			   /* how many sprites refer to this image */
  texture_loader_t *loader;	   /* asynchronous image loader */
  int steps;			   /* How many steps it took to load */
} image;


typedef enum { NEW, IN, FULL, OUT, DEAD } sprite_state;
typedef enum { NONE, PANZOOM, FADE,
               LEFT, RIGHT, TOP, BOTTOM,
               ZOOM, FLIP, SPIN }
  transition_mode;

typedef enum { LOADING, FIRST_IN, IN_PANZOOM, MID_PANZOOM, RECENTER,
               TRANSITION_OUT, TRANSITION_PANZOOM } anim_state;

typedef struct {
  int id;			   /* unique number for debugging */
  image *img;			   /* which image this animation displays */
  GLfloat opacity;		   /* how to render it */
  double start_time;		   /* when this animation began */
  double duration;		   /* How long for all 3 sprite_states */
  transition_mode transition;      /* how to render it */
  easing_function easing;
  GLfloat zoom;			   /* randomized version of --zoom arg */
  rect from, to, current;	   /* the journey this image is taking */
  sprite_state state;		   /* the state we're in right now */
  sprite_state prev_state;	   /* the state we were in previously */
  double state_time;		   /* time of last state change */
  int frame_count;		   /* frames since last state change */
} sprite;


typedef struct {
  GLXContext *glx_context;

  int nimages;			/* how many images are loaded or loading now */
  image *images[10];		/* pointers to the images */

  int nsprites;			/* how many sprites are animating right now */
  sprite *sprites[10];		/* pointers to the live sprites */

  anim_state state;		/* state of the overall animation */
  double title_opacity;		/* the state of the image title */

  double now;			/* current time in seconds */
  double dawn_of_time;		/* when the program launched */
  double start_time;		/* when we began displaying this image */
  double prev_frame_time;	/* when we last drew a frame */

  Bool redisplay_needed_p;	/* Sometimes we can get away with not
                                   re-painting.  Tick this if a redisplay
                                   is required. */
  Bool change_now_p;		/* Set when the user clicks to ask for a new
                                   image right now. */

  GLfloat fps;                  /* approximate frame rate we're achieving */
  GLfloat theoretical_fps;      /* maximum frame rate that might be possible */
  Bool checked_fps_p;		/* Whether we have checked for a low
                                   frame rate. */

  texture_font_data *font_data;	/* for printing image file names */

  int sprite_id, image_id;      /* debugging id counters */

  double time_elapsed;
  int frames_elapsed;

} slideshow_state;

static slideshow_state *sss = NULL;


/* Command-line arguments
 */
static int transition_seconds;	/* Duration in seconds of in/out transitions.
				   If 0, jump-cut instead of fading. */
static int fade_seconds;	/* Duration in seconds of pan-zoom transitions.
				   If 0, jump-cut instead of fading. */
static int pan_seconds;		/* Duration of each pan-zoom. */
static int image_seconds;	/* How long until loading a new image. */
static int zoom;		/* How far in to zoom, in percent of image
				   size: that is, 75 means "when zoomed all
				   the way in, 75% of the image will be
				   visible."
                                 */
static Bool letterbox_p;	/* When a loaded image is not the same aspect
				   ratio as the window, whether to display
				   black bars.
				 */
static Bool mipmap_p;		/* Use mipmaps instead of single textures. */
static Bool do_titles;		/* Display image titles. */
static Bool verbose_p;		/* Print to stderr. */
static Bool debug_p;		/* Show image extents with boxes. */


static XrmOptionDescRec opts[] = {
  {"-transition",   ".transitionDuration", XrmoptionSepArg, 0 },
  {"-fade",         ".fadeDuration",  XrmoptionSepArg, 0      },
  {"-pan",          ".panDuration",   XrmoptionSepArg, 0      },
  {"-duration",     ".imageDuration", XrmoptionSepArg, 0      },
  {"-zoom",         ".zoom",          XrmoptionSepArg, 0      },
  {"-titles",       ".titles",        XrmoptionNoArg, "True"  },
  {"-letterbox",    ".letterbox",     XrmoptionNoArg, "True"  },
  {"-no-letterbox", ".letterbox",     XrmoptionNoArg, "False" },
  {"-clip",         ".letterbox",     XrmoptionNoArg, "False" },
  {"-mipmaps",      ".mipmap",        XrmoptionNoArg, "True"  },
  {"-no-mipmaps",   ".mipmap",        XrmoptionNoArg, "False" },
  {"-v",            ".verbose",       XrmoptionNoArg, "True"  },
  {"-verbose",      ".verbose",       XrmoptionNoArg, "True"  },
  {"-debug",        ".debug",         XrmoptionNoArg, "True"  },
};

static argtype vars[] = {
  { &transition_seconds, "transitionDuration", "TransitionDuration",
                                              DEF_TRANSITION_DURATION,  t_Int},
  { &fade_seconds,  "fadeDuration", "FadeDuration", DEF_FADE_DURATION,  t_Int},
  { &pan_seconds,   "panDuration",  "PanDuration",  DEF_PAN_DURATION,   t_Int},
  { &image_seconds, "imageDuration","ImageDuration",DEF_IMAGE_DURATION, t_Int},
  { &zoom,          "zoom",         "Zoom",         DEF_ZOOM,           t_Int},
  { &mipmap_p,      "mipmap",       "Mipmap",       DEF_MIPMAP,        t_Bool},
  { &letterbox_p,   "letterbox",    "Letterbox",    DEF_LETTERBOX,     t_Bool},
  { &verbose_p,     "verbose",      "Verbose",      DEF_VERBOSE,       t_Bool},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,         t_Bool},
  { &do_titles,     "titles",       "Titles",       DEF_TITLES,        t_Bool},
};

ENTRYPOINT ModeSpecOpt slideshow_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


static const char *
blurb (void)
{
# ifdef HAVE_JWXYZ
  return "GLSlideshow";
# else
  static char buf[255];
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
# endif
}


static void image_loaded_cb (const char *filename, XRectangle *geom,
                             int image_width, int image_height,
                             int texture_width, int texture_height,
                             void *closure);


/* Allocate an image structure and start asynchronous file loading in the
   background.

   The texture_loader_t referenced by *result->loader must be stepped with
   step_texture_loader() until it calls back to the callback function passed
   to it.
 */
static image *
alloc_image_incremental (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  image *img = (image *) calloc (1, sizeof (*img));

  img->id = ++ss->image_id;
  img->loaded_p = False;
  img->used_p = False;
  img->mi = mi;
  img->steps = 0;

  glGenTextures (1, &img->texid);
  if (img->texid <= 0) abort();

  if (wire)
    image_loaded_cb (0, 0, 0, 0, 0, 0, img);
  else
    img->loader = alloc_texture_loader (mi->xgwa.screen, mi->window,
                                        *ss->glx_context, 0, 0, mipmap_p,
                                        img->texid);

  ss->images[ss->nimages++] = img;
  if (ss->nimages >= countof(ss->images)) abort();

  return img;
}


/* Step the incremental image loader.
 */
static void
slideshow_idle (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  double allowed_time = ((double) mi->pause) / 2000000; /* 0.01 sec */
  int i;

  for (i = 0; i < ss->nimages; i++)
    {
      image *img = ss->images[i];
      if (img->loader)
        {
          if (texture_loader_failed (img->loader))
            abort();
          step_texture_loader (img->loader, allowed_time,
                               image_loaded_cb, img);
          img->steps++;
          break; /* only do the first one! */
        }
    }
}


/* Callback that tells us that the texture has been loaded.
 */
static void
image_loaded_cb (const char *filename, XRectangle *geom,
                 int image_width, int image_height,
                 int texture_width, int texture_height,
                 void *closure)
{
  image *img = (image *) closure;
  ModeInfo *mi = img->mi;
  /* slideshow_state *ss = &sss[MI_SCREEN(mi)]; */

  int wire = MI_IS_WIREFRAME(mi);

  if (wire)
    {
      img->w = MI_WIDTH (mi) * (0.5 + frand (1.0));
      img->h = MI_HEIGHT (mi);
      img->geom.width  = img->w;
      img->geom.height = img->h;
      goto DONE;
    }

  if (img->loader)
    {
      texture_loader_t *loader = img->loader;
      img->loader = 0;
      free_texture_loader(loader);
    }

  if (image_width == 0 || image_height == 0)
    exit (1);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  img->w  = image_width;
  img->h  = image_height;
  img->tw = texture_width;
  img->th = texture_height;
  img->geom = *geom;
  img->title = (filename ? strdup (filename) : 0);

  /* If the image's width doesn't come back as the width of the screen,
     then the image must have been scaled down (due to insufficient
     texture memory.)  Scale up the coordinates to stretch the image
     to fill the window.
   */
  if (img->w != MI_WIDTH(mi))
    {
      double scale = (double) MI_WIDTH(mi) / img->w;
      img->w  *= scale;
      img->h  *= scale;
      img->tw *= scale;
      img->th *= scale;
      img->geom.x      *= scale;
      img->geom.y      *= scale;
      img->geom.width  *= scale;
      img->geom.height *= scale;
    }

  /* xscreensaver-getimage returns paths relative to the image directory
     now, so leave the sub-directory part in.  Unless it's an absolute path.
  */
  if (img->title && img->title[0] == '/')
    {
      /* strip filename to part between last "/" and end. */
      /* xscreensaver-getimage has already stripped off the extension. */
      char *s = strrchr (img->title, '/');
      if (s) memmove (img->title, s+1, strlen (s));
    }

  if (verbose_p)
    fprintf (stderr, "%s: loaded   img %2d: \"%s\" (%d steps)\n",
             blurb(), img->id, (img->title ? img->title : "(null)"),
             img->steps + 1);
 DONE:

  img->loaded_p = True;
}



/* Free the image and texture, after nobody is referencing it.
 */
static void
destroy_image (ModeInfo *mi, image *img)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  Bool freed_p = False;
  int i;

  if (!img) abort();
  /* if (!img->loaded_p) abort(); */
  /* if (!img->used_p) abort(); */
  if (img->texid <= 0) abort();
  if (img->refcount != 0) abort();

  for (i = 0; i < ss->nimages; i++)		/* unlink it from the list */
    if (ss->images[i] == img)
      {
        int j;
        for (j = i; j < ss->nimages-1; j++)	/* pull remainder forward */
          ss->images[j] = ss->images[j+1];
        ss->images[j] = 0;
        ss->nimages--;
        freed_p = True;
        break;
      }

  if (!freed_p) abort();

  if (verbose_p)
    fprintf (stderr, "%s: unloaded img %2d: \"%s\"\n",
             blurb(), img->id, (img->title ? img->title : "(null)"));

  if (img->loader)
    {
      texture_loader_t *loader = img->loader;
      img->loader = 0;
      free_texture_loader (loader);
    }

  if (img->title) free (img->title);
  glDeleteTextures (1, &img->texid);
  free (img);
}


/* Return an image to use for a sprite.
   Might return 0 if a new image is unavailable because
   the machine is being really slow.
 */
static image *
get_image (ModeInfo *mi, Bool want_new_p)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  image *ret = 0;
  image *new_img = 0;
  image *old_img = 0;
  image *loading_img = 0;
  int i;

  for (i = 0; i < ss->nimages; i++)
    {
      image *img2 = ss->images[i];
      if (!img2) abort();
      if (!img2->loaded_p)
        loading_img = img2;
      else if (!img2->used_p)
        new_img = img2;
      else
        old_img = img2;
    }

  /* If a new image was requested but unavailable, return NULL. */
  ret = want_new_p ? new_img : old_img;

  /* Make sure that there is always one unused image in the pipe. */
  if (!new_img && !loading_img)
    alloc_image_incremental (mi);

  return ret;
}


/* Allocate a new sprite and start its animation going.
   Returns 0 if no images available yet.
 */
static sprite *
new_sprite (ModeInfo *mi, Bool want_new_p)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  image *img = get_image (mi, want_new_p);
  sprite *sp;

  if (!img)
    {
      /* Oops, no images yet!  The machine is probably hurting bad.
         Let's give it some time before thrashing again. */
      usleep (250000);
      return 0;
    }

  if (!img->loaded_p) abort();

  sp = (sprite *) calloc (1, sizeof (*sp));
  sp->id = ++ss->sprite_id;
  sp->start_time = ss->now;
  sp->state_time = sp->start_time;
  sp->state = sp->prev_state = NEW;
  sp->zoom = (zoom
              ? 1 + frand ((100.0 / zoom) - 1)  /* 75% => [1.0 - 1.33] */
              : 1);
  sp->img = img;

  sp->img->refcount++;
  sp->img->used_p = True;

  if (want_new_p)
    ss->start_time = ss->now;

  ss->sprites[ss->nsprites++] = sp;
  if (ss->nsprites >= countof(ss->sprites)) abort();

  if (verbose_p)
    fprintf (stderr, "%s: new sprite %d, img %d\n", blurb(), sp->id,
             sp->img->id);

  return sp;
}


/* Free the given sprite, and decrement the reference count on its image.
 */
static void
destroy_sprite (ModeInfo *mi, sprite *sp)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  Bool freed_p = False;
  image *img;
  int i;

  if (!sp) abort();
  if (sp->state != DEAD) abort();
  img = sp->img;
  /* if (!img) abort(); */
  if (img)
    {
      if (!img->loaded_p) abort();
      if (!img->used_p) abort();
      if (img->refcount <= 0) abort();
    }

  for (i = 0; i < ss->nsprites; i++)		/* unlink it from the list */
    if (ss->sprites[i] == sp)
      {
        int j;
        for (j = i; j < ss->nsprites-1; j++)	/* pull remainder forward */
          ss->sprites[j] = ss->sprites[j+1];
        ss->sprites[j] = 0;
        ss->nsprites--;
        freed_p = True;
        break;
      }

  if (!freed_p) abort();

  if (verbose_p)
    fprintf (stderr, "%s: free sprite %d\n", blurb(), sp->id);

  free (sp);
  sp = 0;

  if (img)
    {
      img->refcount--;
      if (img->refcount < 0) abort();
      if (img->refcount == 0)
        destroy_image (mi, img);
    }
}


static void
launch_sprite (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int vp_w = MI_WIDTH(mi);
  int vp_h = MI_HEIGHT(mi);
  sprite *out, *in;
  sprite *sprites[2];
  int sprite_w[countof(sprites)];
  int sprite_max_w = 0;
  int sprite_max_h = 0;
  int i;

  if (ss->state == LOADING)
    return;

  if (ss->state == FIRST_IN)
    out = NULL;
  else if (ss->nsprites > 0)
    out = ss->sprites[ss->nsprites-1];
  else
    {
      fprintf (stderr, "%s: no out sprite\n", blurb());
      abort();
    }

  in = new_sprite (mi, (ss->state == FIRST_IN ||
                        ss->state == TRANSITION_OUT ||
                        ss->state == TRANSITION_PANZOOM));
  if (!in)
    {
      fprintf (stderr, "%s: no in sprite\n", blurb());
      abort();
    }

  sprites[0] = out;
  sprites[1] = in;

  switch (ss->state) {
  case FIRST_IN:
  case TRANSITION_OUT:

    /* Compute the maximal extents of the pair, including letterboxing.
       For the overlapping horizontal/vertical transitions to look right,
       the two sprites have to transit the same distance, not just 100%.
     */
    sprite_max_w = vp_w;
    sprite_max_h = vp_h;

    for (i = 0; i < countof(sprites); i++)
      {
        sprite *sp = sprites[i];
        int w, h;
        double ratio;
        if (!sp) continue;
        ratio = (double) sp->img->geom.height / sp->img->geom.width;

        if (letterbox_p)
          {
            if (vp_w * ratio < vp_h)
              w = vp_w;			/* full width, smaller height */
            else
              w = vp_h / ratio;		/* full height, smaller width */
          }
        else
          {
            if (vp_w * ratio < vp_h)
              w = vp_h / ratio;		/* full height, crop width */
            else
              w = vp_w;			/* full width, crop height */
          }

        w *= sp->zoom;

        sprite_w[i] = w;
        h = w * ratio;
        if (w > sprite_max_w) sprite_max_w = w;
        if (h > sprite_max_h) sprite_max_h = h;
      }

    {
      const transition_mode mm[] = {
        FADE, LEFT, RIGHT, TOP, BOTTOM, FLIP, SPIN
      };
      const easing_function ee[] = {
        EASE_IN_OUT_QUAD, EASE_IN_OUT_QUAD, EASE_IN_OUT_QUAD, EASE_IN_OUT_QUAD,
        EASE_IN_OUT_QUINT, EASE_IN_OUT_QUINT,
        EASE_IN_OUT_BACK, EASE_IN_OUT_BACK,
        EASE_IN_CUBIC, EASE_IN_CUBIC,
      };
      transition_mode t;
      easing_function e;
      t = mm[random() % countof(mm)];
      e = ee[random() % countof(ee)];

      if (ss->state == FIRST_IN && out) abort();
      if (ss->state == TRANSITION_OUT && !out) abort();

      for (i = 0; i < countof(sprites); i++)
        {
          Bool out_p = (i == 0);
          sprite *sp = sprites[i];
          double ratio;
          if (!sp) continue;

          sp->state      = out_p ? OUT : IN;
          sp->start_time = ss->now;
          sp->duration   = transition_seconds;

          if (!out_p && pan_seconds <= 0)
            /* If we are not doing PANZOOM, then this image will
               linger on screen for the full image duration. */
            sp->duration += image_seconds;

          ratio = (double) sp->img->geom.height / sp->img->geom.width;

          /* Default coordinates are centered. */
          sp->to.w   = sprite_w[i];
          sp->to.h   = sp->to.w * ratio;
          sp->to.x   = (sp->to.w > vp_w
                        ? -(sp->to.w - vp_w) / 2
                        :  (vp_w - sp->to.w) / 2);
          sp->to.y   = (sp->to.h > vp_h
                        ? -(sp->to.h - vp_h) / 2
                        :  (vp_h - sp->to.h) / 2);
          sp->to.r.x = 0;
          sp->to.r.y = 0;
          sp->to.r.z = 0;
          sp->from   = sp->to;

          sp->transition = t;
          sp->easing = e;

          switch (t) {
          case FADE:
            /* No motion, only alpha */
            break;

          case LEFT:
            if (out_p)
              sp->to.x   = sp->to.x + sprite_max_w;
            else
              sp->from.x = sp->to.x - sprite_max_w;
            break;

          case RIGHT:
            if (out_p)
              sp->to.x   = sp->to.x - sprite_max_w;
            else
              sp->from.x = sp->to.x + sprite_max_w;
            break;
    
          case TOP:
            if (out_p)
              sp->to.y   = sp->to.y - sprite_max_h;
            else
              sp->from.y = sp->to.y + sprite_max_h;
            break;
    
          case BOTTOM:
            if (out_p)
              sp->to.y   = sp->to.y + sprite_max_h;
            else
              sp->from.y = sp->to.y - sprite_max_h;
            break;
    
          case ZOOM:
            if (out_p)
              {
                sp->to.x   = vp_w / 2;
                sp->to.y   = vp_h / 2;
                sp->to.w   = 1;
                sp->to.h   = sp->to.w * ratio;
              }
            else
              {
                sp->from.x = vp_w / 2;
                sp->from.y = vp_h / 2;
                sp->from.w = 1;
                sp->from.h = sp->from.w * ratio;
              }
            break;
    
          case SPIN:
            {
              int quads = 2 + (random() % 6);
              GLfloat spin = (!out_p && out
                              ? -out->to.r.z
                              : (90 * quads *
                                  (random() & 1 ? 1 : -1)));
              GLfloat scale = 0.0001;
              if (out_p)
                {
                  sp->to.r.z  = spin;
                  sp->to.w   *= scale;
                  sp->to.h   *= scale;
                  sp->to.x    = (vp_w - sp->to.w) / 2;
                  sp->to.y    = (vp_h - sp->to.h) / 2;
                }
              else
                {
                  sp->from.r.z = spin;
                  sp->from.w  *= scale;
                  sp->from.h  *= scale;
                  sp->from.x   = (vp_w - sp->from.w) / 2;
                  sp->from.y   = (vp_h - sp->from.h) / 2;
                }
            }
            break;
    
          case FLIP:
            {
              Bool horiz_p = random() & 1;
              Bool sign = (random() & 1 ? 1 : -1);
              GLfloat flipx = (!out_p && out
                               ? -out->to.r.x
                               : horiz_p ? 180 * sign : 0);
              GLfloat flipy = (!out_p && out
                               ? -out->to.r.y
                               : horiz_p ? 0 : 180 * sign);
              if (out_p)
                {
                  sp->to.r.x   = flipx;
                  sp->to.r.y   = flipy;
                }
              else
                {
                  sp->from.r.x = flipx;
                  sp->from.r.y = flipy;
                }
            }
            break;
    
          default:
            abort();
            break;
          }
        }

      if (out && verbose_p)
        fprintf (stderr, "%s: adjust TR sprite %d dur %.1f start %.1f\n",
                 blurb(), out->id, out->duration,
                 out->start_time - ss->now);
    }
    break;

  case IN_PANZOOM:
    /* Adjust the just-transitioned-in image to fade out, as the 
       second sprite starts fading in.
     */
    if (!out) abort();
    out->duration   = pan_seconds;
    out->duration   = fade_seconds + pan_seconds;
    out->start_time = ss->now - pan_seconds;
    out->transition = PANZOOM;
    out->state      = FULL;
    out->prev_state = OUT;
    out->to         = out->current;
    out->from       = out->current;

    if (verbose_p)
      fprintf (stderr, "%s: adjust PZ sprite %d dur %.1f start %.1f\n",
               blurb(), out->id, out->duration,
               out->start_time - ss->now);

    /* fallthrough */

  case MID_PANZOOM:
  case RECENTER:
  case TRANSITION_PANZOOM:
    if (!in) abort();
    in->duration   = fade_seconds + pan_seconds;
    in->start_time = ss->now;
    in->transition = PANZOOM;
    in->state      = IN;

    {
      int w;
      double ratio = (double) in->img->geom.height / in->img->geom.width;
      double in_zoom[2];

      if (letterbox_p)
        {
          if (vp_w * ratio < vp_h)
            w = vp_w;			/* full width, smaller height */
          else
            w = vp_h / ratio;		/* full height, smaller width */
        }
      else
        {
          if (vp_w * ratio < vp_h)
            w = vp_h / ratio;		/* full height, crop width */
          else
            w = vp_w;			/* full width, crop height */
        }

      in_zoom[0] = (zoom
                    ? 1 + frand ((100.0 / zoom) - 1)  /* 75% => [1.0 - 1.33] */
                    : 1);
      in_zoom[1] = (zoom
                    ? 1 + frand ((100.0 / zoom) - 1)
                    : 1);

      if (ss->state == RECENTER)
        in_zoom[1] = in->zoom;

      in->from.w   = w * in_zoom[0];
      in->from.h   = in->from.w * ratio;
      in->from.r.x = 0;
      in->from.r.y = 0;
      in->from.r.z = 0;

      in->from.x = (in->from.w > vp_w
                    ? -frand (in->from.w - vp_w)
                    :  frand (vp_w - in->from.w));
      in->from.y = (in->from.h > vp_h
                    ? -frand (in->from.h - vp_h)
                    :  frand (vp_h - in->from.h));

      in->to.w   = w * in_zoom[1];
      in->to.h   = in->to.w * ratio;
      in->to.r.x = 0;
      in->to.r.y = 0;
      in->to.r.z = 0;

      if (ss->state == RECENTER)
        {
          in->to.x   = (in->to.w > vp_w
                        ? -(in->to.w - vp_w) / 2
                        :  (vp_w - in->to.w) / 2);
          in->to.y   = (in->to.h > vp_h
                        ? -(in->to.h - vp_h) / 2
                        :  (vp_h - in->to.h) / 2);
        }
      else
        {
          in->to.x = (in->to.w > vp_w
                      ? -frand (in->to.w - vp_w)
                      :  frand (vp_w - in->to.w));
          in->to.y = (in->to.h > vp_h
                      ? -frand (in->to.h - vp_h)
                      :  frand (vp_h - in->to.h));
        }

    }
    break;

  default:
    abort();
    break;
  }

  if (verbose_p)
    fprintf (stderr, "%s: launch sprite %d dur %.1f\n",
             blurb(), in->id, in->duration);
}



/* Draw the given sprite at the phase of its animation dictated by
   its creation time compared to the current wall clock.
 */
static void
draw_sprite (ModeInfo *mi, sprite *sp)
{
  /* slideshow_state *ss = &sss[MI_SCREEN(mi)]; */
  int wire = MI_IS_WIREFRAME(mi);
  image *img = sp->img;
  int vp_w = MI_WIDTH(mi);
  int vp_h = MI_HEIGHT(mi);

  if (! sp->img) abort();
  if (! img->loaded_p) abort();

  glPushMatrix();
  {
    GLfloat aspect = vp_w / (GLfloat) vp_h;
    glScalef (1, aspect, 1);
    glRotatef (sp->current.r.x, 1, 0, 0);
    glRotatef (sp->current.r.y, 0, 1, 0);
    glRotatef (sp->current.r.z, 0, 0, 1);
    glScalef (1, 1/aspect, 1);

    glTranslatef (sp->current.x / vp_w - 0.5,
                  sp->current.y / vp_h - 0.5,
                  0);
    glScalef (sp->current.w / vp_w,
              sp->current.h / vp_h,
              1);

    if (wire)			/* Draw a grid inside the box */
      {
        GLfloat dy = 0.1;
        GLfloat dx = dy * img->w / img->h;
        GLfloat x, y;

        if (sp->id & 1)
          glColor4f (sp->opacity, 0, 0, 1);
        else
          glColor4f (0, 0, sp->opacity, 1);

        glBegin(GL_LINES);
        glVertex3f (0, 0, 0); glVertex3f (1, 1, 0);
        glVertex3f (1, 0, 0); glVertex3f (0, 1, 0);

        for (y = 0; y < 1+dy; y += dy)
          {
            GLfloat yy = (y > 1 ? 1 : y);
            for (x = 0.5; x < 1+dx; x += dx)
              {
                GLfloat xx = (x > 1 ? 1 : x);
                glVertex3f (0, xx, 0); glVertex3f (1, xx, 0);
                glVertex3f (yy, 0, 0); glVertex3f (yy, 1, 0);
              }
            for (x = 0.5; x > -dx; x -= dx)
              {
                GLfloat xx = (x < 0 ? 0 : x);
                glVertex3f (0, xx, 0); glVertex3f (1, xx, 0);
                glVertex3f (yy, 0, 0); glVertex3f (yy, 1, 0);
              }
          }
        glEnd();
      }
    else			/* Draw the texture quad */
      {
        GLfloat texw  = img->geom.width  / (GLfloat) img->tw;
        GLfloat texh  = img->geom.height / (GLfloat) img->th;
        GLfloat texx1 = img->geom.x / (GLfloat) img->tw;
        GLfloat texy1 = img->geom.y / (GLfloat) img->th;
        GLfloat texx2 = texx1 + texw;
        GLfloat texy2 = texy1 + texh;

        glEnable (GL_TEXTURE_2D);
        glBindTexture (GL_TEXTURE_2D, img->texid);
        glColor4f (1, 1, 1, sp->opacity);
        glNormal3f (0, 0, 1);
        glBegin (GL_QUADS);
        glTexCoord2f (texx1, texy2); glVertex3f (0, 0, 0);
        glTexCoord2f (texx2, texy2); glVertex3f (1, 0, 0);
        glTexCoord2f (texx2, texy1); glVertex3f (1, 1, 0);
        glTexCoord2f (texx1, texy1); glVertex3f (0, 1, 0);
        glEnd();

        if (debug_p)		/* Draw a border around the image */
          {
            if (!wire) glDisable (GL_TEXTURE_2D);

            if (sp->id & 1)
              glColor4f (sp->opacity, 0, 0, 1);
            else
              glColor4f (0, 0, sp->opacity, 1);

            glBegin (GL_LINE_LOOP);
            glVertex3f (0, 0, 0);
            glVertex3f (0, 1, 0);
            glVertex3f (1, 1, 0);
            glVertex3f (1, 0, 0);
            glEnd();

            if (!wire) glEnable (GL_TEXTURE_2D);
          }
      }
  }

  if (debug_p)
    {
#if 1
      if (!wire) glDisable (GL_TEXTURE_2D);

      if (sp->id & 1)
        glColor4f (1, 0, 0, 1);
      else
        glColor4f (0, 0, 1, 1);

      /* Draw the "from" and "to" boxes
       */
      glBegin (GL_LINE_LOOP);
      glVertex3f (sp->from.x,              sp->from.y,              0);
      glVertex3f (sp->from.x + sp->from.w, sp->from.y,              0);
      glVertex3f (sp->from.x + sp->from.w, sp->from.y + sp->from.h, 0);
      glVertex3f (sp->from.x,              sp->from.y + sp->from.h, 0);
      glEnd();

      glBegin (GL_LINE_LOOP);
      glVertex3f (sp->to.x,                sp->to.y,                0);
      glVertex3f (sp->to.x + sp->to.w,     sp->to.y,                0);
      glVertex3f (sp->to.x + sp->to.w,     sp->to.y + sp->to.h,     0);
      glVertex3f (sp->to.x,                sp->to.y + sp->to.h,     0);
      glEnd();

      if (!wire) glEnable (GL_TEXTURE_2D);
#endif
    }

  glPopMatrix();
}


static void
tick_sprites (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  anim_state ostate = ss->state;
  double total_secs = ss->now - ss->start_time;
  sprite *sp = ss->nsprites > 0 ? ss->sprites[ss->nsprites-1] : NULL;
  double sp_secs = sp ? ss->now - sp->start_time : 0;
  Bool launch_p = FALSE;
  Bool image_p;
  int i;

  /* Make sure that there is always one unused image in the pipe. */
  get_image (mi, TRUE);

  image_p = (ss->nimages > 0 &&
             ss->images[ss->nimages-1]->loaded_p &&
             !ss->images[ss->nimages-1]->used_p);

  switch (ss->state) {

  case LOADING:
    if (image_p)
      ss->state = FIRST_IN;
    ss->redisplay_needed_p = TRUE;
    break;

  case FIRST_IN:
  case TRANSITION_OUT:
    if (!sp && ss->state == TRANSITION_OUT)
      {
        fprintf (stderr, "%s: no TRANSITION_OUT sprite\n", blurb());
        abort();
      }
    else if (ss->change_now_p && image_p)
      {
        ss->state = TRANSITION_OUT;
        launch_p = TRUE;
      }
    else if (total_secs >= transition_seconds && pan_seconds)
      ss->state = IN_PANZOOM;
    else if (total_secs >= image_seconds && image_p)
      {
        ss->state = TRANSITION_OUT;
        launch_p = TRUE;	/* do TRANSITION_OUT again */
      }
    break;

  case IN_PANZOOM:
    if (!sp)
      {
        fprintf (stderr, "%s: no IN_PANZOOM sprite\n", blurb());
        abort();
      }
    else if (ss->change_now_p && image_p)
      {
        if (transition_seconds <= 0)
          ss->state = TRANSITION_PANZOOM;
        else
          ss->state = RECENTER;
      }
    else if (sp_secs >= pan_seconds)
      ss->state = MID_PANZOOM;
    break;

  case MID_PANZOOM:
  case TRANSITION_PANZOOM:
    if (!sp)
      {
        fprintf (stderr, "%s: no MID_PANZOOM sprite\n", blurb());
        abort();
      }
    else if (image_p &&
             (ss->change_now_p ||
              sp_secs >= pan_seconds))
      {
        if ((ss->change_now_p && image_p) ||
            total_secs >= image_seconds - (fade_seconds + pan_seconds))
          {
            if (transition_seconds <= 0)
              {
                /* TRANSITION_PANZOOM is just like MID_PANZOOM but loads
                   a new image instead of reusing an old one. */
                ss->state = TRANSITION_PANZOOM;
                launch_p = TRUE;
              }
            else
              {
                ss->state = RECENTER;
              }
          }
        else
          {
            ss->state = MID_PANZOOM;
            launch_p = TRUE;
          }
      }
    break;

  case RECENTER:
    if (!sp)
      {
        fprintf (stderr, "%s: no RECENTER sprite\n", blurb());
        abort();
      }
    else if (sp_secs >= pan_seconds + fade_seconds &&
             total_secs >= image_seconds)
      ss->state = TRANSITION_OUT;
    break;

  default:
    abort();
    break;
  }

  ss->change_now_p = FALSE;

  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      sp_secs = ss->now - sp->start_time;
    }

  if (ostate != ss->state) launch_p = TRUE;

  if (launch_p)
    {
      if (verbose_p)
        fprintf (stderr, "%s: %s => %s\n", blurb(),
                 (ostate == LOADING        ? "LOADING" :
                  ostate == FIRST_IN       ? "FIRST_IN" :
                  ostate == IN_PANZOOM     ? "IN_PANZOOM" :
                  ostate == MID_PANZOOM    ? "MID_PANZOOM" :
                  ostate == RECENTER       ? "RECENTER" :
                  ostate == TRANSITION_OUT ? "TRANSITION_OUT" :
                  ostate == TRANSITION_PANZOOM ? "TRANSITION_PANZOOM" : "???"),
                 (ss->state == LOADING        ? "LOADING" :
                  ss->state == FIRST_IN       ? "FIRST_IN" :
                  ss->state == IN_PANZOOM     ? "IN_PANZOOM" :
                  ss->state == MID_PANZOOM    ? "MID_PANZOOM" :
                  ss->state == RECENTER       ? "RECENTER" :
                  ss->state == TRANSITION_OUT ? "TRANSITION_OUT" :
                  ss->state == TRANSITION_PANZOOM ? "TRANSITION_PANZOOM" :
                  "???"));

      launch_sprite (mi);
      return;
    }

  for (i = 0; i < countof(ss->sprites); i++)
    {
      sprite *sp = ss->sprites[i];
      double ratio;
      double secs;
      double prev_opacity;
      rect prev_rect;
      if (!sp) continue;

      secs = ss->now - sp->start_time;

      sp->prev_state  = sp->state;
      prev_rect       = sp->current;
      prev_opacity    = sp->opacity;

      switch (sp->transition) {
      case PANZOOM:
        ratio = secs / (fade_seconds + pan_seconds);

        if (secs <= fade_seconds)	/* PANZOOM is 3 transitions in one */
          {
            sp->opacity = secs / fade_seconds;
            if (sp->opacity > 1) sp->opacity = 1;
            sp->state = IN;
          }
        else if (secs <= pan_seconds)
          {
            sp->state = FULL;
            sp->opacity = 1;
          }
        else if (secs <= fade_seconds + pan_seconds)
          {
            sp->opacity = 1 - ((secs - pan_seconds) / fade_seconds);
            sp->state = OUT;
            if (ss->state == RECENTER &&
                i == ss->nsprites-1)
              sp->opacity = 1;
          }
        else
          {
            sp->state = DEAD;
            sp->opacity = 0;
          }
        break;

      case LEFT:
      case RIGHT:
      case TOP:
      case BOTTOM:
      case FLIP:
      case FADE:
      case ZOOM:
      case SPIN:
        ratio = secs / transition_seconds;
        if (ratio <= 1)
          {
            if (sp->transition == FADE ||
                sp->transition == ZOOM ||
                sp->transition == SPIN)
              sp->opacity = (sp->state == IN ? ratio : 1-ratio);
            else
              sp->opacity = 1;
          }
        else if (secs <= sp->duration)
          {
            ratio = 1;		/* Linger */
            sp->opacity = 1;
          }
        else
          {
            sp->state = DEAD;
            ratio = 1;
            sp->opacity = 0;
          }
        break;
      default:
        abort();
        break;
      }

      ratio = ease (sp->easing, ratio);
      sp->current.x   = sp->from.x +   ratio * (sp->to.x   - sp->from.x);
      sp->current.y   = sp->from.y +   ratio * (sp->to.y   - sp->from.y);
      sp->current.w   = sp->from.w +   ratio * (sp->to.w   - sp->from.w);
      sp->current.h   = sp->from.h +   ratio * (sp->to.h   - sp->from.h);
      sp->current.r.x = sp->from.r.x + ratio * (sp->to.r.x - sp->from.r.x);
      sp->current.r.y = sp->from.r.y + ratio * (sp->to.r.y - sp->from.r.y);
      sp->current.r.z = sp->from.r.z + ratio * (sp->to.r.z - sp->from.r.z);

      sp->frame_count++;
    
      if (sp->state != DEAD &&
          (prev_rect.x   != sp->current.x ||
           prev_rect.y   != sp->current.y ||
           prev_rect.w   != sp->current.w ||
           prev_rect.h   != sp->current.h ||
           prev_rect.r.x != sp->current.r.x ||
           prev_rect.r.y != sp->current.r.y ||
           prev_rect.r.z != sp->current.r.z ||
           prev_opacity != sp->opacity))
        ss->redisplay_needed_p = True;

      if (0 && verbose_p && debug_p && ss->redisplay_needed_p)
        fprintf (stderr, "%s: %d: %5.0f %-5.0f  %5.0f %-5.0f  %.1f\n", blurb(),
                 sp->id, 
                 sp->current.x, sp->current.y, 
                 sp->current.w, sp->current.h, 
                 sp->opacity);

      if (sp->state != sp->prev_state)
        {
          if (verbose_p)
            fprintf (stderr,
                     "%s: %d: %-4s %-5s %3d frames %2.0f sec %5.1f fps"
                     " (max %.0f fps?)\n",
                     blurb(),
                     sp->id,
                     (sp->prev_state == NEW  ? "NEW"  :
                      sp->prev_state == IN   ? "IN"   :
                      sp->prev_state == FULL ? "FULL" :
                      sp->prev_state == OUT  ? "FULL" :
                      sp->prev_state == DEAD ? "DEAD" : "???"),
                     (sp->transition == NONE    ? "NONE"  :
                      sp->transition == PANZOOM ? "PZ"    :
                      sp->transition == FADE    ? "FADE"  :
                      sp->transition == LEFT    ? "LEFT"  :
                      sp->transition == RIGHT   ? "RIGHT" :
                      sp->transition == TOP     ? "TOP"   :
                      sp->transition == BOTTOM  ? "BOT"   :
                      sp->transition == ZOOM    ? "ZOOM"  :
                      sp->transition == FLIP    ? "FLIP"  :
                      sp->transition == SPIN    ? "SPIN"  : "???"),
                     sp->frame_count,
                     secs,
                     sp->frame_count / secs,
                     ss->theoretical_fps);
          sp->state_time = ss->now;
          sp->frame_count = 0;
        }
    }

  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      if (sp->state == DEAD)
        {
          /* Don't unload the last sprite if we are stalled waiting for an
             image to load. */
          if (ss->nsprites < 2 && !image_p)
            continue;
          destroy_sprite (mi, sp);
          i--;
        }
    }

  if (ss->nsprites > 3 ||
      ss->nsprites > countof (ss->sprites) ||
      ss->nsprites < (!image_p ? 0 : 1))
    {
      fprintf (stderr, "%s: %d sprites\n", blurb(), ss->nsprites);
      abort();
    }

  /* The state machine for drawing titles is a bit disconnected from the
     rest of the state machine, since there's a single title that spans
     multiple sprites. We want to start it fading in once an image is
     fully visible, and start it fading out before the transition to a
     new image.  And either or both of transition_seconds or fade_seconds
     might be 0.
   */
  if (do_titles)
    {
      double oopacity = ss->title_opacity;
      double t = ss->now - ss->start_time;

      /* When should the title start fading in? */
      double start = (transition_seconds
                      ? transition_seconds
                      : fade_seconds);

      /* When should the title finish fading out? */
      double end = (transition_seconds && fade_seconds
                    ? image_seconds + fade_seconds
                    : transition_seconds
                    ? image_seconds
                    : image_seconds - fade_seconds);

      double sec_fade = 3;	/* Duration of fade for titles */
      double dur;

      /* Try to show titles even if --duration is very short. */
      dur = end - start;
      if (sec_fade > dur / 2)
        sec_fade = dur / 2;

      if (transition_seconds == 0 && fade_seconds == 0)
        sec_fade = 0;  /* If images are doing jump cuts, so do titles. */

      if (dur <= 0.5 && sec_fade > 0)
        ss->title_opacity = 0;					/* DEAD */
      else if (t <= start)
        ss->title_opacity = 0;					/* NEW  */
      else if (t <= start + sec_fade)
        ss->title_opacity = (t - start) / sec_fade;		/* IN   */
      else if (t <= end - sec_fade)
        ss->title_opacity = 1;					/* FULL */
      else if (t <= end)
        ss->title_opacity = (end - t) / sec_fade;		/* OUT  */
      else
        ss->title_opacity = 0;					/* DEAD */

      if (ss->title_opacity != oopacity)
        ss->redisplay_needed_p = True;
    }
}


static void
draw_sprites (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int i;

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glPushMatrix();

  if (debug_p)
    {
      GLfloat s = 0.75;
      glScalef (s, s, s);
    }

  if (ss->state == LOADING)
    {
      double secs = ss->now - ss->dawn_of_time;
      double opacity = secs / 6;
      if (opacity > 1) opacity = 1;
      glColor4f (1, 1, 0, opacity);
      print_texture_label (mi->dpy, ss->font_data,
                           mi->xgwa.width, mi->xgwa.height,
                           0, "Loading...");
    }

  for (i = 0; i < ss->nsprites; i++)
    draw_sprite (mi, ss->sprites[i]);

  if (do_titles && ss->title_opacity > 0)
    {
      sprite *sp = (ss->nsprites > 0
                    ? ss->sprites[ss->nsprites-1]
                    : 0);
      if (sp && sp->img && sp->img->loaded_p &&
          sp->img->title && *sp->img->title)
        {
          glColor4f (1, 1, 1, ss->title_opacity);
          print_texture_label (mi->dpy, ss->font_data,
                               mi->xgwa.width, mi->xgwa.height,
                               1, sp->img->title);
        }
    }

  if (debug_p)				/* draw a white box (the "screen") */
    {
      int wire = MI_IS_WIREFRAME(mi);

      if (!wire) glDisable (GL_TEXTURE_2D);

      glColor4f (1, 1, 1, 1);
      glBegin (GL_LINE_LOOP);
      glVertex3f (-0.5, -0.5, 0);
      glVertex3f (-0.5,  0.5, 0);
      glVertex3f ( 0.5,  0.5, 0);
      glVertex3f ( 0.5, -0.5, 0);
      glEnd();

      glBegin (GL_LINES);
      glVertex3f (-0.05, 0, 0);
      glVertex3f ( 0.05, 0, 0);
      glVertex3f (0, -0.05, 0);
      glVertex3f (0,  0.05, 0);
      glEnd();

      if (!wire) glEnable (GL_TEXTURE_2D);
    }

  glPopMatrix();
}


ENTRYPOINT void
reshape_slideshow (ModeInfo *mi, int width, int height)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];

  /* Empirically, these numbers give us a projection where a 1x1 quad
     centered at 0,0,0 fills the viewport 100%, while still having a
     non-ortho projection.  All 3 numbers are interdependent.
     Presumably 'scale' could be computed from the others, somehow...
   */
  GLfloat fov   = 30;
  GLfloat cam   = 15;
  GLfloat scale = 8.03845;

  glViewport (0, 0, width, height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (fov, 1, 0.01, 100);

  glRotatef (current_device_rotation(), 0, 0, 1);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();

  gluLookAt (0, 0, cam,
             0, 0, 0,
             0, 1, 0);
  glScalef (scale, scale, scale);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  ss->redisplay_needed_p = True;
}


ENTRYPOINT Bool
slideshow_handle_event (ModeInfo *mi, XEvent *event)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];

  if (event->xany.type == Expose ||
      event->xany.type == GraphicsExpose ||
      event->xany.type == VisibilityNotify)
    {
      ss->redisplay_needed_p = True;
      if (verbose_p)
        fprintf (stderr, "%s: exposure\n", blurb());
      return False;
    }
#if 0   /* This tends to trigger "no sprites" aborts, and I don't care
           enough to debug it. */
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      ss->change_now_p = True;
      return True;
    }
#endif

  return False;
}


/* Do some sanity checking on various user-supplied values, and make
   sure they are all internally consistent.
 */
static void
sanity_check (ModeInfo *mi)
{
  /* When transitioning one image out and another in (e.g.,
     one image slides out to the right while a new one slides
     in from the left) the two sprites run simultaneously over
     'transition' seconds.

     After an image has transitioned in and is centered on screen,
     we begin the PANZOOM phase, in which each sprite has three
     states: fading in, full, fading out.  The in/out states
     overlap like this:

     iiiiiiFFFFFFFFFFFFoooooo  . . . . . . . . . . . . . . . . . 
     . . . . . . . . . iiiiiiFFFFFFFFFFFFoooooo  . . . . . . . .
     . . . . . . . . . . . . . . . . . . iiiiiiFFFFFFFFFFFFooooo

     In PANZOOM, we create a new sprite in the IN state as soon
     as the old sprite enters the OUT state.

     The "fade in + pan" phase lasts 'pan' seconds: 'fade' is
     inclusive of 'pan'.  So the non-overlapped part takes 'pan'
     seconds but the whole animation takes 'fade + pan' seconds
     (not 'pan + fade + pan' seconds).

     To avoid necessitating a jump-cut or blank screen, when
     entering the PANZOOM phase, there is a single IN_PANZOOM
     state that smoothly turns the just-landed centered image
     into a panner.  And when exiting PANZOOM for transition-out,
     the final RECENTER state does a PANZOOM that lands the image
     centered on the screen in preparation for TRANSITION_OUT.

     To do PANZOOM only, set 'transition' to 0.

     To do TRANSITION only, set 'pan' to 0.

     The full cycle (before loading a new image) takes 'image'
     seconds (and is inclusive of the 'transition' seconds).
   */

  if (zoom < 1) zoom = 1;           /* zoom is a positive percentage */
  else if (zoom > 100) zoom = 100;

  if (transition_seconds < 0)
    transition_seconds = 0;

  if (image_seconds < transition_seconds) /* img is inclusive of transition. */
    image_seconds = transition_seconds;

  if (image_seconds <= 0)           /* no zero-length cycles, please... */
    image_seconds = 1;

  if (pan_seconds < fade_seconds)   /* pan is inclusive of fade */
    pan_seconds = fade_seconds;

  if (fade_seconds == 0)	    /* Panning without fading looks terrible */
    pan_seconds = 0;

  /* No need to use mipmaps if we're not changing the image size much */
  if (zoom >= 80) mipmap_p = False;
}


#if 0 /* This if flaky and probably not necessary any more */
static void
check_fps (ModeInfo *mi)
{
# ifndef HAVE_JWXYZ  /* always assume Cocoa and mobile are fast enough */

  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  double start_time, end_time, wall_elapsed, frame_duration, fps;
  int i;

  start_time = ss->now;
  end_time = double_time();
  frame_duration = end_time - start_time;   /* time spent drawing this frame */
  ss->time_elapsed += frame_duration;       /* time spent drawing all frames */
  ss->frames_elapsed++;

  wall_elapsed = end_time - ss->dawn_of_time;
  fps = ss->frames_elapsed / ss->time_elapsed;
  ss->theoretical_fps = fps;

  if (ss->checked_fps_p) return;
  if (debug_p) return;

  if (wall_elapsed <= 8)    /* too early to be sure */
    return;

  ss->checked_fps_p = True;

  if (fps >= fps_cutoff)
    {
      if (verbose_p)
        fprintf (stderr,
                 "%s: %.1f fps is fast enough (with %d frames in %.1f secs)\n",
                 blurb(), fps, ss->frames_elapsed, wall_elapsed);
      return;
    }

  fprintf (stderr,
           "%s: only %.1f fps!  Turning off pan/fade to compensate...\n",
           blurb(), fps);

  zoom = 100;
  transition_seconds = 0;
  fade_seconds = 0;
  pan_seconds = 0;

  sanity_check (mi);

  if (verbose_p)
    fprintf (stderr,
             "%s: tr: %d, pan: %d; fade: %d; img: %d; zoom: %d%%\n",
             blurb(), transition_seconds, pan_seconds, fade_seconds,
             image_seconds, zoom);

  ss->state = LOADING;
  ss->redisplay_needed_p = True;

  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      if (!sp) continue;
      sp->state = DEAD;
      if (!sp->img->used_p) abort();
      if (!sp->img->loaded_p) abort();
      if (sp->img->refcount <= 0) abort();
      sp->img->used_p = FALSE;
      sp->img->refcount--;
      if (verbose_p)
        fprintf (stderr, "%s: %d: deref img %d\n",
                 blurb(), sp->id, sp->img->id);
      sp->img = NULL;
      destroy_sprite (mi, sp);
      i--;
    }

  for (i = 0; i < ss->nimages; i++)
    {
      image *img = ss->images[i];
      if (!img) continue;
      img->used_p = FALSE;
    }

#endif /* HAVE_JWXYZ */
}
#endif /* 0 */


/* Add "-v" to invocation of "xscreensaver-getimage" in -debug mode.
 */
static void
hack_resources (Display *dpy)
{
# ifndef HAVE_JWXYZ
  char *res = "desktopGrabber";
  char *val = get_string_resource (dpy, res, "DesktopGrabber");
  char buf1[255];
  char buf2[255];
  XrmValue value;
  XrmDatabase db = XtDatabase (dpy);
  sprintf (buf1, "%.100s.%.100s", progname, res);
  sprintf (buf2, "%.200s -v", val);
  value.addr = buf2;
  value.size = strlen(buf2);
  XrmPutResource (&db, buf1, "String", &value);
  free (val);
# endif /* !HAVE_JWXYZ */
}


ENTRYPOINT void
init_slideshow (ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  slideshow_state *ss;
  int wire = MI_IS_WIREFRAME(mi);
  
  MI_INIT (mi, sss);
  ss = &sss[screen];

  if ((ss->glx_context = init_GL(mi)) != NULL) {
    reshape_slideshow (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }

  if (debug_p) verbose_p = True;

  if (verbose_p)
    fprintf (stderr,
             "%s: tr: %d, pan: %d; fade: %d; img: %d; zoom: %d%%\n",
             blurb(), transition_seconds, pan_seconds, fade_seconds,
             image_seconds, zoom);

  sanity_check (mi);

  if (verbose_p)
    fprintf (stderr,
             "%s: tr: %d, pan: %d; fade: %d; img: %d; zoom: %d%%\n",
             blurb(), transition_seconds, pan_seconds, fade_seconds,
             image_seconds, zoom);

  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  glDepthMask (GL_FALSE);
  glEnable (GL_CULL_FACE);
  glCullFace (GL_BACK);

  if (! wire)
    {
      glEnable (GL_TEXTURE_2D);
      glShadeModel (GL_SMOOTH);
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

  if (debug_p) glLineWidth (3);

  ss->font_data = load_texture_font (mi->dpy, "titleFont");

  if (debug_p)
    hack_resources (MI_DISPLAY(mi));

  ss->state = LOADING;

  ss->now = double_time();
  ss->dawn_of_time = ss->now;
  ss->prev_frame_time = ss->now;

  get_image (mi, TRUE);
}


ENTRYPOINT void
draw_slideshow (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];

  if (!ss->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ss->glx_context);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ss->now = double_time();

  slideshow_idle (mi);
  tick_sprites (mi);

  if (!ss->redisplay_needed_p)
    /* Nothing to do! Don't bother drawing a texture or even swapping the
       frame buffers. Note that this means that the FPS display will be
       wrong: "Load" will be frozen on whatever it last was, when in
       reality it will be close to 0. */
    return;

  if (verbose_p && ss->now - ss->prev_frame_time > 1)
    fprintf (stderr, "%s: static screen for %.1f secs\n",
             blurb(), ss->now - ss->prev_frame_time);

  draw_sprites (mi);

  if (mi->fps_p) do_fps (mi);

  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
  ss->prev_frame_time = ss->now;
  ss->redisplay_needed_p = False;

  /* check_fps (mi); */
}


ENTRYPOINT void
free_slideshow (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  /* int i; */
  if (!ss->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ss->glx_context);

  if (ss->font_data) free_texture_font (ss->font_data);
  ss->font_data = 0;

# if 0
  /* Doing this causes free pointers to be run from the XtInput.
     I don't know how to shut that down properly. */
  for (i = countof(ss->sprites)-1; i >= 0; i--)
    {
      sprite *sp = ss->sprites[i];
      if (!sp) continue;
      sp->state = DEAD;
      destroy_sprite (mi, sp);
    }

  for (i = ss->nimages-1; i >= 0; i--)
    {
      image *img = ss->images[i];
      if (!img) continue;
      if (img->refcount == 0)
        destroy_image (mi, img);
    }
# endif
}


XSCREENSAVER_MODULE_2 ("GLSlideshow", glslideshow, slideshow)

#endif /* USE_GL */
