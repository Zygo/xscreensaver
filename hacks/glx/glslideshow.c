/* glslideshow, Copyright (c) 2003-2014 Jamie Zawinski <jwz@jwz.org>
 * Loads a sequence of images and smoothly pans around them; crossfades
 * when loading new images.
 *
 * Originally written by Mike Oliphant <oliphant@gtk.org> (c) 2002, 2003.
 * Rewritten by jwz, 21-Jun-2003.
 * Rewritten by jwz again, 6-Feb-2005.
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
 * TODO:
 *
 * - When a new image is loaded, there is a glitch: animation pauses during
 *   the period when we're loading the image-to-fade-in.  On fast (2GHz)
 *   machines, this stutter is short but noticable (usually around 1/10th
 *   second.)  On slower machines, it can be much more pronounced.
 *   This turns out to be hard to fix...
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
 *       layout to 32 bit RGBA in client-endianness, to make them usable as
 *       OpenGL textures.
 *
 *    4: We must actually construct a texture.
 *
 *   So, the speed of step 1 doesn't really matter, since that happens in
 *   the background.  But steps 2, 3, and 4 happen in *this* process, and
 *   cause the visible glitch.
 *
 *   Step 2 can't be moved to another process without opening a second
 *   connection to the X server, which is pretty heavy-weight.  (That would
 *   be possible, though; the other process could open an X connection,
 *   retrieve the pixmap, and feed it back to us through a pipe or
 *   something.)
 *
 *   Step 3 might be able to be optimized by coding tuned versions of
 *   grab-ximage.c:copy_ximage() for the most common depths and bit orders.
 *   (Or by moving it into the other process along with step 2.)
 *
 *   Step 4 is the hard one, though.  It might be possible to speed up this
 *   step if there is some way to allow two GL processes share texture
 *   data.  Unless, of course, all the time being consumed by step 4 is
 *   because the graphics pipeline is flooded, in which case, that other
 *   process would starve the screen anyway.
 *
 *   Is it possible to use a single GLX context in a multithreaded way?
 *   Or use a second GLX context, but allow the two contexts to share data?
 *   I can't find any documentation about this.
 *
 *   How does Apple do this with their MacOSX slideshow screen saver?
 *   Perhaps it's easier for them because their OpenGL libraries have
 *   thread support at a lower level?
 */

#define DEFAULTS  "*delay:           20000                \n" \
		  "*wireframe:       False                \n" \
                  "*showFPS:         False                \n" \
	          "*fpsSolid:        True                 \n" \
	          "*useSHM:          True                 \n" \
            "*titleFont: -*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
		  "*grabDesktopImages:   False \n" \
		  "*chooseRandomImages:  True  \n"

# define release_slideshow 0
# include "xlockmore.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef USE_GL


# define DEF_FADE_DURATION  "2"
# define DEF_PAN_DURATION   "6"
# define DEF_IMAGE_DURATION "30"
# define DEF_ZOOM           "75"
# define DEF_FPS_CUTOFF     "5"
# define DEF_TITLES         "False"
# define DEF_LETTERBOX      "True"
# define DEF_DEBUG          "False"
# define DEF_MIPMAP         "True"

#include "grab-ximage.h"
#include "texfont.h"

typedef struct {
  double x, y, w, h;
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
} image;


typedef enum { NEW, IN, FULL, OUT, DEAD } sprite_state;

typedef struct {
  int id;			   /* unique number for debugging */
  image *img;			   /* which image this animation displays */
  GLfloat opacity;		   /* how to render it */
  double start_time;		   /* when this animation began */
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

  double now;			/* current time in seconds */
  double dawn_of_time;		/* when the program launched */
  double image_load_time;	/* time when we last loaded a new image */
  double prev_frame_time;	/* time when we last drew a frame */

  Bool awaiting_first_image_p;  /* Early in startup: nothing to display yet */
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
static int fade_seconds;    /* Duration in seconds of fade transitions.
                               If 0, jump-cut instead of fading. */
static int pan_seconds;     /* Duration of each pan through an image. */
static int image_seconds;   /* How many seconds until loading a new image. */
static int zoom;            /* How far in to zoom when panning, in percent of
                               image size: that is, 75 means "when zoomed all
                               the way in, 75% of the image will be visible."
                             */
static int fps_cutoff;      /* If the frame-rate falls below this, turn off
                               zooming.*/
static Bool letterbox_p;    /* When a loaded image is not the same aspect
                               ratio as the window, whether to display black
                               bars.
                             */
static Bool mipmap_p;	    /* Use mipmaps instead of single textures. */
static Bool do_titles;	    /* Display image titles. */
static Bool debug_p;	    /* Be loud and do weird things. */


static XrmOptionDescRec opts[] = {
  {"-fade",         ".fadeDuration",  XrmoptionSepArg, 0      },
  {"-pan",          ".panDuration",   XrmoptionSepArg, 0      },
  {"-duration",     ".imageDuration", XrmoptionSepArg, 0      },
  {"-zoom",         ".zoom",          XrmoptionSepArg, 0      },
  {"-cutoff",       ".FPScutoff",     XrmoptionSepArg, 0      },
  {"-titles",       ".titles",        XrmoptionNoArg, "True"  },
  {"-letterbox",    ".letterbox",     XrmoptionNoArg, "True"  },
  {"-no-letterbox", ".letterbox",     XrmoptionNoArg, "False" },
  {"-clip",         ".letterbox",     XrmoptionNoArg, "False" },
  {"-mipmaps",      ".mipmap",        XrmoptionNoArg, "True"  },
  {"-no-mipmaps",   ".mipmap",        XrmoptionNoArg, "False" },
  {"-debug",        ".debug",         XrmoptionNoArg, "True"  },
};

static argtype vars[] = {
  { &fade_seconds,  "fadeDuration", "FadeDuration", DEF_FADE_DURATION,  t_Int},
  { &pan_seconds,   "panDuration",  "PanDuration",  DEF_PAN_DURATION,   t_Int},
  { &image_seconds, "imageDuration","ImageDuration",DEF_IMAGE_DURATION, t_Int},
  { &zoom,          "zoom",         "Zoom",         DEF_ZOOM,           t_Int},
  { &mipmap_p,      "mipmap",       "Mipmap",       DEF_MIPMAP,        t_Bool},
  { &letterbox_p,   "letterbox",    "Letterbox",    DEF_LETTERBOX,     t_Bool},
  { &fps_cutoff,    "FPScutoff",    "FPSCutoff",    DEF_FPS_CUTOFF,     t_Int},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,         t_Bool},
  { &do_titles,     "titles",       "Titles",       DEF_TITLES,        t_Bool},
};

ENTRYPOINT ModeSpecOpt slideshow_opts = {countof(opts), opts, countof(vars), vars, NULL};


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


static void image_loaded_cb (const char *filename, XRectangle *geom,
                             int image_width, int image_height,
                             int texture_width, int texture_height,
                             void *closure);


/* Allocate an image structure and start a file loading in the background.
 */
static image *
alloc_image (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  image *img = (image *) calloc (1, sizeof (*img));

  img->id = ++ss->image_id;
  img->loaded_p = False;
  img->used_p = False;
  img->mi = mi;

  glGenTextures (1, &img->texid);
  if (img->texid <= 0) abort();

  ss->image_load_time = ss->now;

  if (wire)
    image_loaded_cb (0, 0, 0, 0, 0, 0, img);
  else
    load_texture_async (mi->xgwa.screen, mi->window, *ss->glx_context,
                        0, 0, mipmap_p, img->texid, image_loaded_cb, img);

  ss->images[ss->nimages++] = img;
  if (ss->nimages >= countof(ss->images)) abort();

  return img;
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
      /* strip filename to part between last "/" and last ".". */
      char *s = strrchr (img->title, '/');
      if (s) strcpy (img->title, s+1);
      s = strrchr (img->title, '.');
      if (s) *s = 0;
    }

  if (debug_p)
    fprintf (stderr, "%s: loaded   img %2d: \"%s\"\n",
             blurb(), img->id, (img->title ? img->title : "(null)"));
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

  if (debug_p)
    fprintf (stderr, "%s: unloaded img %2d: \"%s\"\n",
             blurb(), img->id, (img->title ? img->title : "(null)"));

  if (img->title) free (img->title);
  glDeleteTextures (1, &img->texid);
  free (img);
}


/* Return an image to use for a sprite.
   If it's time for a new one, get a new one.
   Otherwise, use an old one.
   Might return 0 if the machine is really slow.
 */
static image *
get_image (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  image *img = 0;
  double now = ss->now;
  Bool want_new_p = (ss->change_now_p ||
                     ss->image_load_time + image_seconds <= now);
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

  if (want_new_p && new_img)
    img = new_img, new_img = 0, ss->change_now_p = False;
  else if (old_img)
    img = old_img, old_img = 0;
  else if (new_img)
    img = new_img, new_img = 0, ss->change_now_p = False;

  /* Make sure that there is always one unused image in the pipe.
   */
  if (!new_img && !loading_img)
    alloc_image (mi);

  return img;
}


/* Pick random starting and ending positions for the given sprite.
 */
static void
randomize_sprite (ModeInfo *mi, sprite *sp)
{
  int vp_w = MI_WIDTH(mi);
  int vp_h = MI_HEIGHT(mi);
  int img_w = sp->img->geom.width;
  int img_h = sp->img->geom.height;
  int min_w, max_w;
  double ratio = (double) img_h / img_w;

  if (letterbox_p)
    {
      min_w = img_w;
    }
  else
    {
      if (img_w < vp_w)
        min_w = vp_w;
      else
        min_w = img_w * (float) vp_h / img_h;
    }

  max_w = min_w * 100 / zoom;

  sp->from.w = min_w + frand ((max_w - min_w) * 0.4);
  sp->to.w   = max_w - frand ((max_w - min_w) * 0.4);
  sp->from.h = sp->from.w * ratio;
  sp->to.h   = sp->to.w   * ratio;

  if (zoom == 100)	/* only one box, and it is centered */
    {
      sp->from.x = (sp->from.w > vp_w
                    ? -(sp->from.w - vp_w) / 2
                    :  (vp_w - sp->from.w) / 2);
      sp->from.y = (sp->from.h > vp_h
                    ? -(sp->from.h - vp_h) / 2
                    :  (vp_h - sp->from.h) / 2);
      sp->to = sp->from;
    }
  else			/* position both boxes randomly */
    {
      sp->from.x = (sp->from.w > vp_w
                    ? -frand (sp->from.w - vp_w)
                    :  frand (vp_w - sp->from.w));
      sp->from.y = (sp->from.h > vp_h
                    ? -frand (sp->from.h - vp_h)
                    :  frand (vp_h - sp->from.h));
      sp->to.x   = (sp->to.w > vp_w
                    ? -frand (sp->to.w - vp_w)
                    :  frand (vp_w - sp->to.w));
      sp->to.y   = (sp->to.h > vp_h
                    ? -frand (sp->to.h - vp_h)
                    :  frand (vp_h - sp->to.h));
    }

  if (random() & 1)
    {
      rect swap = sp->to;
      sp->to = sp->from;
      sp->from = swap;
    }

  /* Make sure the aspect ratios are within 0.001 of each other.
   */
  {
    int r1 = 0.5 + (sp->from.w * 1000 / sp->from.h);
    int r2 = 0.5 + (sp->to.w   * 1000 / sp->to.h);
    if (r1 < r2-1 || r1 > r2+1)
      {
        fprintf (stderr,
                 "%s: botched aspect: %f x %f (%d) vs  %f x %f (%d): %s\n",
                 progname, 
                 sp->from.w, sp->from.h, r1,
                 sp->to.w, sp->to.h, r2,
                 (sp->img->title ? sp->img->title : "[null]"));
        abort();
      }
  }

  sp->from.x /= vp_w;
  sp->from.y /= vp_h;
  sp->from.w /= vp_w;
  sp->from.h /= vp_h;
  sp->to.x   /= vp_w;
  sp->to.y   /= vp_h;
  sp->to.w   /= vp_w;
  sp->to.h   /= vp_h;
}


/* Allocate a new sprite and start its animation going.
 */
static sprite *
new_sprite (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  image *img = get_image (mi);
  sprite *sp;

  if (!img)
    {
      /* Oops, no images yet!  The machine is probably hurting bad.
         Let's give it some time before thrashing again. */
      usleep (250000);
      return 0;
    }

  sp = (sprite *) calloc (1, sizeof (*sp));
  sp->id = ++ss->sprite_id;
  sp->start_time = ss->now;
  sp->state_time = sp->start_time;
  sp->state = sp->prev_state = NEW;
  sp->img = img;

  sp->img->refcount++;
  sp->img->used_p = True;

  ss->sprites[ss->nsprites++] = sp;
  if (ss->nsprites >= countof(ss->sprites)) abort();

  randomize_sprite (mi, sp);

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
  if (!img) abort();
  if (!img->loaded_p) abort();
  if (!img->used_p) abort();
  if (img->refcount <= 0) abort();

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
  free (sp);
  sp = 0;

  img->refcount--;
  if (img->refcount < 0) abort();
  if (img->refcount == 0)
    destroy_image (mi, img);
}


/* Updates the sprite for the current frame of the animation based on
   its creation time compared to the current wall clock.
 */
static void
tick_sprite (ModeInfo *mi, sprite *sp)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  image *img = sp->img;
  double now = ss->now;
  double secs;
  double ratio;
  rect prev_rect = sp->current;
  GLfloat prev_opacity = sp->opacity;

  if (! sp->img) abort();
  if (! img->loaded_p) abort();

  secs = now - sp->start_time;
  ratio = secs / (pan_seconds + fade_seconds);
  if (ratio > 1) ratio = 1;

  sp->current.x = sp->from.x + ratio * (sp->to.x - sp->from.x);
  sp->current.y = sp->from.y + ratio * (sp->to.y - sp->from.y);
  sp->current.w = sp->from.w + ratio * (sp->to.w - sp->from.w);
  sp->current.h = sp->from.h + ratio * (sp->to.h - sp->from.h);

  sp->prev_state = sp->state;

  if (secs < fade_seconds)
    {
      sp->state = IN;
      sp->opacity = secs / (GLfloat) fade_seconds;
    }
  else if (secs < pan_seconds)
    {
      sp->state = FULL;
      sp->opacity = 1;
    }
  else if (secs < pan_seconds + fade_seconds)
    {
      sp->state = OUT;
      sp->opacity = 1 - ((secs - pan_seconds) / (GLfloat) fade_seconds);
    }
  else
    {
      sp->state = DEAD;
      sp->opacity = 0;
    }

  if (sp->state != sp->prev_state &&
      (sp->prev_state == IN ||
       sp->prev_state == FULL))
    {
      double secs = now - sp->state_time;

      if (debug_p)
        fprintf (stderr,
                 "%s: %s %3d frames %2.0f sec %5.1f fps (%.1f fps?)\n",
                 blurb(),
                 (sp->prev_state == IN ? "fade" : "pan "),
                 sp->frame_count,
                 secs,
                 sp->frame_count / secs,
                 ss->theoretical_fps);

      sp->state_time = now;
      sp->frame_count = 0;
    }

  sp->frame_count++;

  if (sp->state != DEAD &&
      (prev_rect.x != sp->current.x ||
       prev_rect.y != sp->current.y ||
       prev_rect.w != sp->current.w ||
       prev_rect.h != sp->current.h ||
       prev_opacity != sp->opacity))
    ss->redisplay_needed_p = True;
}


/* Draw the given sprite at the phase of its animation dictated by
   its creation time compared to the current wall clock.
 */
static void
draw_sprite (ModeInfo *mi, sprite *sp)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  image *img = sp->img;

  if (! sp->img) abort();
  if (! img->loaded_p) abort();

  glPushMatrix();
  {
    glTranslatef (sp->current.x, sp->current.y, 0);
    glScalef (sp->current.w, sp->current.h, 1);

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


    if (do_titles &&
        img->title && *img->title &&
        (sp->state == IN || sp->state == FULL))
      {
        glColor4f (1, 1, 1, sp->opacity);
        print_texture_label (mi->dpy, ss->font_data,
                             mi->xgwa.width, mi->xgwa.height,
                             1, img->title);
      }
  }
  glPopMatrix();

  if (debug_p)
    {
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
    }
}


static void
tick_sprites (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < ss->nsprites; i++)
      tick_sprite (mi, ss->sprites[i]);
}


static void
draw_sprites (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int i;

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

/*
  {
    GLfloat rot = current_device_rotation();
    glTranslatef (0.5, 0.5, 0);
    glRotatef(rot, 0, 0, 1);
    if ((rot >  45 && rot <  135) ||
        (rot < -45 && rot > -135))
      {
        GLfloat s = MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
        glScalef (s, 1/s, 1);
      }
    glTranslatef (-0.5, -0.5, 0);
  }
*/

  for (i = 0; i < ss->nsprites; i++)
    draw_sprite (mi, ss->sprites[i]);
  glPopMatrix();

  if (debug_p)				/* draw a white box (the "screen") */
    {
      int wire = MI_IS_WIREFRAME(mi);

      if (!wire) glDisable (GL_TEXTURE_2D);

      glColor4f (1, 1, 1, 1);
      glBegin (GL_LINE_LOOP);
      glVertex3f (0, 0, 0);
      glVertex3f (0, 1, 0);
      glVertex3f (1, 1, 0);
      glVertex3f (1, 0, 0);
      glEnd();

      if (!wire) glEnable (GL_TEXTURE_2D);
    }
}


ENTRYPOINT void
reshape_slideshow (ModeInfo *mi, int width, int height)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  GLfloat s;
  glViewport (0, 0, width, height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  glRotatef (current_device_rotation(), 0, 0, 1);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();

  s = 2;

  if (debug_p)
    {
      s *= (zoom / 100.0) * 0.75;
      if (s < 0.1) s = 0.1;
    }

  glScalef (s, s, s);
  glTranslatef (-0.5, -0.5, 0);

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
      if (debug_p)
        fprintf (stderr, "%s: exposure\n", blurb());
      return False;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      ss->change_now_p = True;
      return True;
    }

  return False;
}


/* Do some sanity checking on various user-supplied values, and make
   sure they are all internally consistent.
 */
static void
sanity_check (ModeInfo *mi)
{
  if (zoom < 1) zoom = 1;           /* zoom is a positive percentage */
  else if (zoom > 100) zoom = 100;

  if (zoom == 100)		    /* with no zooming, there is no panning */
    pan_seconds = 0;

  if (pan_seconds < fade_seconds)   /* pan is inclusive of fade */
    pan_seconds = fade_seconds;

  if (pan_seconds == 0)             /* no zero-length cycles, please... */
    pan_seconds = 1;

  if (image_seconds < pan_seconds)  /* we only change images at fade-time */
    image_seconds = pan_seconds;

  /* If we're not panning/zooming within the image, then there's no point
     in crossfading the image with itself -- only do crossfades when changing
     to a new image. */
  if (zoom == 100 && pan_seconds < image_seconds)
    pan_seconds = image_seconds;

  /* No need to use mipmaps if we're not changing the image size much */
  if (zoom >= 80) mipmap_p = False;

  if      (fps_cutoff < 0)  fps_cutoff = 0;
  else if (fps_cutoff > 30) fps_cutoff = 30;
}


static void
check_fps (ModeInfo *mi)
{
#ifndef HAVE_JWXYZ  /* always assume Cocoa and mobile are fast enough */

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

  if (wall_elapsed <= 8)    /* too early to be sure */
    return;

  ss->checked_fps_p = True;

  if (fps >= fps_cutoff)
    {
      if (debug_p)
        fprintf (stderr,
                 "%s: %.1f fps is fast enough (with %d frames in %.1f secs)\n",
                 blurb(), fps, ss->frames_elapsed, wall_elapsed);
      return;
    }

  fprintf (stderr,
           "%s: only %.1f fps!  Turning off pan/fade to compensate...\n",
           blurb(), fps);
  zoom = 100;
  fade_seconds = 0;

  sanity_check (mi);

  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      randomize_sprite (mi, sp);
      sp->state = FULL;
    }

  ss->redisplay_needed_p = True;

  /* Need this in case zoom changed. */
  reshape_slideshow (mi, mi->xgwa.width, mi->xgwa.height);
#endif /* HAVE_JWXYZ */
}


/* Kludge to add "-v" to invocation of "xscreensaver-getimage" in -debug mode
 */
static void
hack_resources (void)
{
#if 0
  char *res = "desktopGrabber";
  char *val = get_string_resource (res, "DesktopGrabber");
  char buf1[255];
  char buf2[255];
  XrmValue value;
  sprintf (buf1, "%.100s.%.100s", progclass, res);
  sprintf (buf2, "%.200s -v", val);
  value.addr = buf2;
  value.size = strlen(buf2);
  XrmPutResource (&db, buf1, "String", &value);
  free (val);
#endif
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

  if (debug_p)
    fprintf (stderr, "%s: pan: %d; fade: %d; img: %d; zoom: %d%%\n",
             blurb(), pan_seconds, fade_seconds, image_seconds, zoom);

  sanity_check(mi);

  if (debug_p)
    fprintf (stderr, "%s: pan: %d; fade: %d; img: %d; zoom: %d%%\n\n",
             blurb(), pan_seconds, fade_seconds, image_seconds, zoom);

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
    hack_resources();

  ss->now = double_time();
  ss->dawn_of_time = ss->now;
  ss->prev_frame_time = ss->now;

  ss->awaiting_first_image_p = True;
  alloc_image (mi);
}


ENTRYPOINT void
draw_slideshow (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int i;

  if (!ss->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ss->glx_context);

  if (ss->awaiting_first_image_p)
    {
      image *img = ss->images[0];
      if (!img) abort();
      if (!img->loaded_p)
        return;

      ss->awaiting_first_image_p = False;
      ss->dawn_of_time = double_time();

      /* start the very first sprite fading in */
      new_sprite (mi);
    }

  ss->now = double_time();

  /* Each sprite has three states: fading in, full, fading out.
     The in/out states overlap like this:

     iiiiiiFFFFFFFFFFFFoooooo  . . . . . . . . . . . . . . . . . 
     . . . . . . . . . iiiiiiFFFFFFFFFFFFoooooo  . . . . . . . .
     . . . . . . . . . . . . . . . . . . iiiiiiFFFFFFFFFFFFooooo

     So as soon as a sprite goes into the "out" state, we create
     a new sprite (in the "in" state.)
   */

  if (ss->nsprites > 2) abort();

  /* If a sprite is just entering the fade-out state,
     then add a new sprite in the fade-in state.
   */
  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      if (sp->state != sp->prev_state &&
          sp->state == (fade_seconds == 0 ? DEAD : OUT))
        new_sprite (mi);
    }

  tick_sprites (mi);

  /* Now garbage collect the dead sprites.
   */
  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      if (sp->state == DEAD)
        {
          destroy_sprite (mi, sp);
          i--;
        }
    }

  /* We can only ever end up with no sprites at all if the machine is
     being really slow and we hopped states directly from FULL to DEAD
     without passing OUT... */
  if (ss->nsprites == 0)
    new_sprite (mi);

  if (!ss->redisplay_needed_p)
    /* Nothing to do! Don't bother drawing a texture or even swapping the
       frame buffers. Note that this means that the FPS display will be
       wrong: "Load" will be frozen on whatever it last was, when in
       reality it will be close to 0. */
    return;

  if (debug_p && ss->now - ss->prev_frame_time > 1)
    fprintf (stderr, "%s: static screen for %.1f secs\n",
             blurb(), ss->now - ss->prev_frame_time);

  draw_sprites (mi);

  if (mi->fps_p) do_fps (mi);

  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
  ss->prev_frame_time = ss->now;
  ss->redisplay_needed_p = False;
  check_fps (mi);
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
  /* The lifetime of these objects is incomprehensible.
     Doing this causes free pointers to be run from the XtInput.
   */
  for (i = ss->nimages-1; i >= 0; i--) {
    if (ss->images[i] && ss->images[i]->refcount == 0)
      destroy_image (mi, ss->images[i]);
  }

  for (i = countof(ss->sprites)-1; i >= 0; i--) {
    if (ss->sprites[i])
      destroy_sprite (mi, ss->sprites[i]);
  }
# endif
}

XSCREENSAVER_MODULE_2 ("GLSlideshow", glslideshow, slideshow)

#endif /* USE_GL */
