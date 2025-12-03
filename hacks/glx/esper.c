/* esper, Copyright © 2017-2025 Jamie Zawinski <jwz@jwz.org>
 * Enhance 224 to 176. Pull out track right. Center in pull back.
 * Pull back. Wait a minute. Go right. Stop. Enhance 57 19. Track 45 left.
 * Gimme a hardcopy right there.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/*
  The Esper machine has a 4:3 display, about 12" diagonal.
  The display is overlayed with a 10x7 grid of blue lines.
  The scene goes approximately like this:

      "Enhance 224 To 176."

          ZM 0000  NS 0000  EW 0000

      The reticle is displayed centered.
      It moves in 8 steps with 3 frame blur to move to around to grid 1,4.

          ZM 0000  NS 0000  EW 0000
          ZM 0000  NS 0001  EW 0001
          ZM 0000  NS 0001  EW 0002
          ZM 0000  NS 0002  EW 0003
          ZM 0000  NS 0003  EW 0005
          ZM 0000  NS 0004  EW 0008
          ZM 0000  NS 0015  EW 0011

      These numbers appear to have little relation to what we are
      actually seeing on the screen.  Also the same numbers are
      repeated later when looking at totally different parts of
      the photograph.

          ZM 0000  NS 0117  EW 0334

      The box appears: 8 steps, final box is 1.5x2.25 at -0.5,4.0.

          ZM 4086  NS 0117  EW 0334

      The box blinks yellow 5x.
      The image's zoom-and-pan takes 8 steps, with no text on the screen.
      The zoom is in discreet steps, with flashes.
      The grid stays the same size the whole time.
      The flashes look like solarization to blue.
      When the zoom is finished, there is still no text.

      "Enhance."  Goes 4 more ticks down the same hole?
      "Stop."  Moves up a little bit at the end.

      Then with no instructions, it goes 20 ticks by itself, off camera.

      "Move in."  10 ticks.
      "Stop."  (We are looking at a fist in the picture.)
      "Pull out track right."
      "Stop."  (We are looking at a newspaper.)
      "Center and pull back."
      "Stop."  (We just passed the round mirror.)
      "Track 45 right."
      "Stop."
      "Center and stop."

      This time there was no grid until it stopped, then the grid showed up.
      There is video tearing at the bottom.

      "Enhance 34 to 36."

          ZM 0000  NS 0063  EW 0185
          ZM 0000  NS 0197  EW 0334
          ZM 3841  NS 0197  EW 0334

      It kind of zooms in to the center wobbly and willy-nilly.
      We are now looking at a glass.

      "Pan right and pull back."  (There is no grid while moving again.)
      "Stop."

      Ok, at this point, we enter fantasy-land.  From here on, the images
      shown are very high resolution with no noise.  And suddenly the 
      UI on the Esper is *way* higher resolution.  My theory is that from
      this point on in the scene, we are not looking at the literal Esper
      machine, but instead the movie is presenting Decard's perception of
      it.  We're seeing the room, not the photo of the room.  The map has
      become the territory. 

      "Enhance 34 to 46."

          ZM 0000  NS 0197  EW 0334

      This has the reticle and box only, no grid, ends with no grid.

      "Pull back."
      "Wait a minute. Go right."
      "Stop."
      Now it's going around the corner or something.

      "Enhance 57 19."
      This has a reticle then box, but the image started zooming early.

      "Track 45 left."
      zooms out and moves left

      "Stop."  (O hai Zhora.)
      "Enhance 15 to 23."

          ZM 3852  NS 0197  EW 0334

      "Gimme a hardcopy right there."

      The printer polaroid is WAY lower resolution than the image we see on
      the "screen" -- in keeping with my theory that we were not seeing the
      screen.


  TODO:

  * There's a glitch at the top/bottom of the texfont textures.
  * "Pull back" isn't quite symmetric: zoom origin is slightly off.
  * Maybe display text like "Pull right" and "Stop".
*/


/* Use a small point size to keep it nice and grainy. */
#define TITLE_FONT \
 "OCR A 10, OCR A Std 10, Lucida Console 10, Monaco 10, Courier 10, monospace 10"

#define DEFAULTS  "*delay:           20000                \n" \
		  "*wireframe:       False                \n" \
                  "*showFPS:         False                \n" \
                  "*fpsTop:          True                 \n" \
	          "*useSHM:          True                 \n" \
                  "*titleFont: "     TITLE_FONT          "\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
		  "*grabDesktopImages:   False \n" \
		  "*chooseRandomImages:  True  \n" \
		  "*gridColor:    #4444FF\n" \
		  "*reticleColor: #FFFF77\n" \
		  "*textColor:    #FFFFBB\n" \

# define refresh_esper 0
# define release_esper 0
# include "xlockmore.h"

#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)
#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#ifdef USE_GL

#undef SMOOTH
#define USE_ASYNC_LOADER

# define DEF_GRID_SIZE      "11"
# define DEF_GRID_THICKNESS "15"
# define DEF_TITLES         "True"
# define DEF_SPEED          "1.0"
# define DEF_DEBUG          "False"

#include "grab-ximage.h"
#include "texfont.h"
#include "doubletime.h"

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"  /* to get <sys/shm.h> */
#endif


typedef struct {
  double x, y, w, h;
} rect;

typedef struct {
  ModeInfo *mi;
  unsigned long id;		   /* unique */
  char *title;			   /* the filename of this image */
  int w, h;			   /* size in pixels of the image */
  int tw, th;			   /* size in pixels of the texture */
  XRectangle geom;		   /* where in the image the bits are */
  Bool loaded_p;		   /* whether the image has finished loading */
# ifdef USE_ASYNC_LOADER
  texture_loader_t *loader;	   /* asynchronous image loader */
# endif
  Bool used_p;			   /* whether the image has yet appeared
                                      on screen */
  GLuint texid;			   /* which texture contains the image */
  int refcount;			   /* how many sprites refer to this image */
} image;


typedef enum {
  BLANK,
  GRID_ON,
  IMAGE_LOAD,
  IMAGE_UNLOAD,
  IMAGE_FORCE_UNLOAD,
  REPOSITION,
  RETICLE_ON,
  RETICLE_MOVE,
  BOX_MOVE,
  IMAGE_ZOOM,
  MANUAL_RETICLE_ON,
  MANUAL_RETICLE,
  MANUAL_BOX_ON,
  MANUAL_BOX
} anim_state;

typedef enum { NEW, IN, FULL, OUT, DEAD } sprite_state;
typedef enum { IMAGE, RETICLE, BOX, GRID, FLASH, TEXT } sprite_type;

typedef struct {
  unsigned long id;		   /* unique */
  sprite_type type;
  image *img;			   /* type = IMAGE */
  unsigned long text_id;	   /* type = TEXT */
  char *text;
  GLfloat opacity;
  GLfloat thickness_scale;	   /* line and image types */
  Bool throb_p;
  double start_time;		   /* when this animation began */
  double duration;		   /* lifetime of sprite in seconds; 0 = inf */
  double fade_duration;		   /* speed of fade in and fade out */
  double pause_duration;	   /* delay before fade-in starts */
  Bool remain_p;		   /* pause forever before fade-out */
  rect from, to, current;	   /* the journey this image is taking */
  sprite_state state;		   /* the state we're in right now */
  double state_time;		   /* time of last state change */
  int frame_count;		   /* frames since last state change */
  Bool fatbits_p;		   /* For image texture rendering */	
  Bool back_p;			   /* If BOX, zooming out, not in */
} sprite;


typedef struct {
  GLXContext *glx_context;
  int nimages;			/* how many images are loaded or loading now */
  image *images[10];		/* pointers to the images */

  int nsprites;			/* how many sprites are animating right now */
  sprite *sprites[100];		/* pointers to the live sprites */

  double now;			/* current time in seconds */
  double dawn_of_time;		/* when the program launched */
  double image_load_time;	/* time when we last loaded a new image */

  texture_font_data *font_data;

  int sprite_id, image_id;      /* debugging id counters */

  GLfloat grid_color[4], reticle_color[4], text_color[4];

  anim_state anim_state;	/* Counters for global animation state, */
  double anim_start, anim_duration;

  Bool button_down_p;

} esper_state;

static esper_state *sss = NULL;


/* Command-line arguments
 */
static int grid_size;
static int grid_thickness;

static Bool do_titles;	    /* Display image titles. */
static GLfloat speed;
static Bool debug_p;	    /* Be loud and do weird things. */


static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",     XrmoptionSepArg, 0 },
  { "-titles",     ".titles",    XrmoptionNoArg, "True"  },
  { "-no-titles",  ".titles",    XrmoptionNoArg, "False" },
  { "-debug",      ".debug",     XrmoptionNoArg, "True"  },
};

static argtype vars[] = {
  { &grid_size,     "gridSize",     "GridSize",     DEF_GRID_SIZE,      t_Int},
  { &grid_thickness,"gridThickness","GridThickness",DEF_GRID_THICKNESS, t_Int},
  { &do_titles,     "titles",       "Titles",       DEF_TITLES,        t_Bool},
  { &speed,         "speed",        "Speed",        DEF_SPEED,        t_Float},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,         t_Bool},
};

ENTRYPOINT ModeSpecOpt esper_opts = {countof(opts), opts, countof(vars), vars, NULL};


static const char *
state_name (anim_state s)
{
  switch (s) {
  case BLANK:         return "BLANK";
  case GRID_ON:       return "GRID_ON";
  case IMAGE_LOAD:    return "IMAGE_LOAD";
  case IMAGE_UNLOAD:  return "IMAGE_UNLOAD";
  case IMAGE_FORCE_UNLOAD: return "IMAGE_FORCE_UNLOAD";
  case REPOSITION:    return "REPOSITION";
  case RETICLE_ON:    return "RETICLE_ON";
  case RETICLE_MOVE:  return "RETICLE_MOVE";
  case BOX_MOVE:      return "BOX_MOVE";
  case IMAGE_ZOOM:    return "IMAGE_ZOOM";
  case MANUAL_BOX_ON: return "MANUAL_BOX_ON";
  case MANUAL_BOX:    return "MANUAL_BOX";
  case MANUAL_RETICLE_ON: return "MANUAL_RETICLE_ON";
  case MANUAL_RETICLE:    return "MANUAL_RETICLE";
  default:            return "UNKNOWN";
  }
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
  esper_state *ss = &sss[MI_SCREEN(mi)];
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
    {
      /* If possible, load images at much higher resolution than the window,
         to facilitate deep zooms.
       */
      int max_max = 4096;  /* ~12 megapixels */
      int max = 0;

# if defined(HAVE_XSHM_EXTENSION) && \
     !defined(HAVE_MOBILE) && \
     !defined(HAVE_COCOA)

      /* Try not to ask for an image larger than the SHM segment size.
         If XSHM fails in a real-X11 world, it can take a staggeringly long
         time to transfer the image bits from the server over Xproto -- like,
         *18 seconds* for 4096 px and 8 seconds for 3072 px on MacOS XQuartz.
         What madness is this?
       */
      unsigned long shmmax = 0;

#  if defined(SHMMAX)
      /* Linux 2.6 defines this to be 0x2000000, but on CentOS 6.9,
         "sysctl kernel.shmmax" reports a luxurious 0x1000000000. */
      shmmax = SHMMAX;
#  elif defined(__APPLE__)
      /* MacOS 10.13 "sysctl kern.sysv.shmmax" is paltry: */
      shmmax = 0x400000;
#  elif defined(__linux__)
      {
        /* Raspbian 10.6 = 0xFEFFFFFF, CentOS 7.7 = 0xFFFFFFFFFEFFFFFF */
        FILE *fp = fopen ("/proc/sys/kernel/shmmax", "r");
        if (fp)
          {
            int result = fscanf (fp, "%lu", &shmmax);
	    if (!result || result == EOF)
              shmmax = 0;  /* Just go with max_max. */ 
            fclose (fp);
          }
      }
#  endif /* !SHMMAX */

      if (shmmax)
        {
          /* Roughly, bytes => NxN. b = (n/8)*4n = n*n*4, so n^2 = 2b, so: */
          unsigned long n = sqrt(shmmax)/2;
          if (n < max_max)
            max_max = n;
        }
# endif /* HAVE_XSHM_EXTENSION and real X11 */

      glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max);
      if (max > max_max) max = max_max;

      /* Never ask for an image smaller than the window, even if that
         will make XSHM fall back to Xproto. */
      if (max < MI_WIDTH(mi) || max < MI_HEIGHT(mi))
        max = 0;

# ifdef USE_ASYNC_LOADER
      img->loader = alloc_texture_loader (mi->xgwa.screen, mi->window,
                                          *ss->glx_context, 0, 0, False,
                                          img->texid);
# else
      load_texture_async (mi->xgwa.screen, mi->window, *ss->glx_context,
                          max, max, False, img->texid, image_loaded_cb, img);
# endif
    }

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
  int ow, oh;
  /* esper_state *ss = &sss[MI_SCREEN(mi)]; */

  int wire = MI_IS_WIREFRAME(mi);

  if (wire)
    {
      img->w = MI_WIDTH (mi) * (0.5 + frand (1.0));
      img->h = MI_HEIGHT (mi);
      img->geom.width  = img->w;
      img->geom.height = img->h;
      goto DONE;
    }

# ifdef USE_ASYNC_LOADER
  if (img->loader)
    {
      texture_loader_t *loader = img->loader;
      img->loader = 0;
      free_texture_loader (loader);
    }
# endif

  if (image_width == 0 || image_height == 0)
    exit (1);

  img->w  = image_width;
  img->h  = image_height;
  img->tw = texture_width;
  img->th = texture_height;
  img->geom = *geom;
  img->title = (filename ? strdup (filename) : 0);

  ow = img->geom.width;
  oh = img->geom.height;

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
      if (s) memmove (img->title, s+1, strlen (s));
      s = strrchr (img->title, '.');
      if (s) *s = 0;
    }

  if (debug_p)
    fprintf (stderr, "%s: loaded %lu \"%s\" %dx%d\n",
             progname, img->id, (img->title ? img->title : "(null)"),
             ow, oh);
 DONE:

  img->loaded_p = True;
}



/* Free the image and texture, after nobody is referencing it.
 */
static void
destroy_image (ModeInfo *mi, image *img)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  Bool freed_p = False;
  int i;

  if (!img) abort();
  if (!img->loaded_p) abort();
  if (!img->used_p) abort();
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
    fprintf (stderr, "%s: unloaded img %2lu: \"%s\"\n",
             progname, img->id, (img->title ? img->title : "(null)"));

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
  esper_state *ss = &sss[MI_SCREEN(mi)];
  image *img = 0;
  image *loading_img = 0;
  int i;

  for (i = 0; i < ss->nimages; i++)
    {
      image *img2 = ss->images[i];
      if (!img2) abort();
      if (!img2->loaded_p)
        loading_img = img2;
      else
        img = img2;
    }

  /* Make sure that there is always one unused image in the pipe.
   */
  if (!img && !loading_img)
    alloc_image (mi);

  return img;
}


/* Allocate a new sprite and start its animation going.
 */
static sprite *
new_sprite (ModeInfo *mi, sprite_type type)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  image *img = (type == IMAGE ? get_image (mi) : 0);
  sprite *sp;

  if (type == IMAGE && !img)
    {
      /* Oops, no images yet!  The machine is probably hurting bad.
         Let's give it some time before thrashing again. */
      usleep (250000);
      return 0;
    }

  sp = (sprite *) calloc (1, sizeof (*sp));
  sp->id = ++ss->sprite_id;
  sp->type = type;
  sp->start_time = ss->now;
  sp->state_time = sp->start_time;
  sp->thickness_scale = 1;
  sp->throb_p = True;
  sp->to.x = 0.5;
  sp->to.y = 0.5;
  sp->to.w = 1.0;
  sp->to.h = 1.0;

  if (img)
    {
      sp->img = img;
      sp->img->refcount++;
      sp->img->used_p = True;
      sp->duration = 0;   /* forever, until further notice */
      sp->fade_duration = 0.5;

      /* Scale the sprite so that the image bits fill the window. */
      {
        double w = MI_WIDTH(mi);
        double h = MI_HEIGHT(mi);
        double r;
        r = ((img->geom.height / (double) img->geom.width) * (w / h));
        if (r > 1)
          sp->to.h *= r;
        else
          sp->to.w /= r;
      }

      /* Pan to a random spot */
      if (sp->to.h > 1)
        sp->to.y += frand ((sp->to.h - 1) / 2) * RANDSIGN();
      if (sp->to.w > 1)
        sp->to.x += frand ((sp->to.w - 1) / 2) * RANDSIGN();
    }

  sp->from = sp->current = sp->to;

  ss->sprites[ss->nsprites++] = sp;
  if (ss->nsprites >= countof(ss->sprites)) abort();

  return sp;
}


static sprite *
copy_sprite (ModeInfo *mi, sprite *old)
{
  sprite *sp = new_sprite (mi, (sprite_type) ~0L);
  int id;
  double tt;
  if (!sp) abort();
  tt = sp->start_time;
  id = sp->id;
  memcpy (sp, old, sizeof(*sp));
  sp->id = id;
  sp->state = NEW;
  sp->state_time = sp->start_time = tt;
  if (sp->img)
    sp->img->refcount++;
  return sp;
}


/* Free the given sprite, and decrement the reference count on its image.
 */
static void
destroy_sprite (ModeInfo *mi, sprite *sp)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  Bool freed_p = False;
  image *img;
  int i;

  if (!sp) abort();
  if (sp->state != DEAD) abort();
  img = sp->img;

  if (sp->type != IMAGE)
    {
      if (img) abort();
    }
  else
    {
      if (!img) abort();
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
  if (sp->text) free (sp->text);
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


/* Updates the sprite for the current frame of the animation based on
   its creation time compared to the current wall clock.
 */
static void
tick_sprite (ModeInfo *mi, sprite *sp)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  image *img = sp->img;
  double now = ss->now;
  double secs;
  double ratio;
  GLfloat visible = sp->duration + sp->fade_duration * 2;
  GLfloat total = sp->pause_duration + visible;

  if (sp->type != IMAGE)
    {
      if (sp->img) abort();
    }
  else
    {
      if (! sp->img) abort();
      if (! img->loaded_p) abort();
    }

  /*          pause        fade  duration        fade
     |------------|------------|---------|-----------|
                   ....----====##########====----....
               from             current            to
   */

  secs = now - sp->start_time;
  ratio = (visible <= 0 ? 1 : ((secs - sp->pause_duration) / visible));
  if (ratio < 0) ratio = 0;
  else if (ratio > 1) ratio = 1;

  sp->current.x = sp->from.x + ratio * (sp->to.x - sp->from.x);
  sp->current.y = sp->from.y + ratio * (sp->to.y - sp->from.y);
  sp->current.w = sp->from.w + ratio * (sp->to.w - sp->from.w);
  sp->current.h = sp->from.h + ratio * (sp->to.h - sp->from.h);

  sp->thickness_scale = 1;

  if (secs < sp->pause_duration)
    {
      sp->state = IN;
      sp->opacity = 0;
    }
  else if (secs < sp->pause_duration + sp->fade_duration)
    {
      sp->state = IN;
      sp->opacity = (secs - sp->pause_duration) / (GLfloat) sp->fade_duration;
    }
  else if (sp->duration == 0 ||  /* 0 means infinite lifetime */
           sp->remain_p ||
           secs < sp->pause_duration + sp->fade_duration + sp->duration)
    {
      sp->state = FULL;
      sp->opacity = 1;

      /* Just after reaching full opacity, pulse the width up and down. */
      if (sp->fade_duration > 0 &&
          secs < sp->pause_duration + sp->fade_duration * 2)
        {
          GLfloat f = ((secs - (sp->pause_duration + sp->fade_duration)) /
                       sp->fade_duration);
          if (sp->throb_p)
            sp->thickness_scale = 1 + 3 * (f > 0.5 ? 1-f : f);
        }
    }
  else if (secs < total)
    {
      sp->state = OUT;
      sp->opacity = (total - secs) / sp->fade_duration;
    }
  else
    {
      sp->state = DEAD;
      sp->opacity = 0;
    }

  sp->frame_count++;
}


/* Draw the given sprite at the phase of its animation dictated by
   its creation time compared to the current wall clock.
 */
static void
draw_image_sprite (ModeInfo *mi, sprite *sp)
{
  /* esper_state *ss = &sss[MI_SCREEN(mi)]; */
  int wire = MI_IS_WIREFRAME(mi);
  image *img = sp->img;

  if (! sp->img) abort();
  if (! img->loaded_p) abort();

  glPushMatrix();
  {
    GLfloat s = 1 + (sp->thickness_scale - 1) / 40.0;
    glTranslatef (0.5, 0.5, 0);
    glScalef (s, s, 1);
    glTranslatef (-0.5, -0.5, 0);

    glTranslatef (sp->current.x, sp->current.y, 0);
    glScalef (sp->current.w, sp->current.h, 1);

    glTranslatef (-0.5, -0.5, 0);

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
        GLfloat o = sp->opacity;
        GLint mag = (sp->fatbits_p ? GL_NEAREST : GL_LINEAR);

        glBindTexture (GL_TEXTURE_2D, img->texid);

        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);

        /* o = 1 - sin ((1 - o*o*o) * M_PI/2); */
        glColor4f (1, 1, 1, o);

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
            glColor4f (sp->opacity, 0, 0, 1);
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
  glPopMatrix();
}


static void
draw_line_sprite (ModeInfo *mi, sprite *sp)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int w = MI_WIDTH(mi);
  int h = MI_HEIGHT(mi);
  int wh = (w > h ? w : h);
  int gs = (sp->type == RETICLE ? grid_size+1 : grid_size);
  int sx = wh / (gs + 1);
  int sy;
  int k;
  GLfloat t = grid_thickness * sp->thickness_scale;
  int fade;
  GLfloat color[4];

  GLfloat x  = w * sp->current.x;
  GLfloat y  = h * sp->current.y;
  GLfloat bw = w * sp->current.w;
  GLfloat bh = h * sp->current.h;

  if (MI_WIDTH(mi) > 2560) t *= 3;  /* Retina displays */

  if (sx < 10) sx = 10;
  sy = sx;

  if (t > sx/3) t = sx/3;
  if (t < 1) t = 1;
  fade = t;
  if (fade < 1) fade = 1;

  if (t <= 0 || sp->opacity <= 0) return;

  glPushMatrix();
  glLoadIdentity();

  if (debug_p)
    {
      GLfloat s = 0.75;
      glScalef (s, s, s);
    }

  glOrtho (0, w, 0, h, -1, 1);

  switch (sp->type) {
  case GRID:    memcpy (color, ss->grid_color,    sizeof(color)); break;
  case RETICLE: memcpy (color, ss->reticle_color, sizeof(color)); break;
  case BOX:     memcpy (color, ss->reticle_color, sizeof(color)); break;
  default: abort();
  }

  if (sp->type == GRID)
    {
      GLfloat s = 1 + (sp->thickness_scale - 1) / 120.0;
      glTranslatef (w/2, h/2, 0);
      glScalef (s, s, 1);
      glTranslatef (-w/2, -h/2, 0);
    }

  glColor4fv (color);

  if (!wire) glDisable (GL_TEXTURE_2D);

  for (k = 0; k < fade; k++)
    {
      GLfloat t2 = t * (1 - (k / (fade * 1.0)));
      if (t2 <= 0) break;
      color[3] = sp->opacity / fade;
      glColor4fv (color);

      glBegin (wire ? GL_LINES : GL_QUADS);

      switch (sp->type) {
      case GRID:
        {
          GLfloat xoff = (w - sx * (w / sx)) / 2.0;
          GLfloat yoff = (h - sy * (h / sy)) / 2.0;
          for (y = -sy/2+t2/2; y < h; y += sy)
            for (x = -sx/2-t2/2; x < w; x += sx)
              {
                glVertex3f (xoff+x+t2, yoff+y,       0);
                glVertex3f (xoff+x+t2, yoff+y+sy-t2, 0);
                glVertex3f (xoff+x,    yoff+y+sy-t2, 0);
                glVertex3f (xoff+x,    yoff+y,       0);
                mi->polygon_count++;

                glVertex3f (xoff+x,    yoff+y-t2, 0);
                glVertex3f (xoff+x+sx, yoff+y-t2, 0);
                glVertex3f (xoff+x+sx, yoff+y,    0);
                glVertex3f (xoff+x,    yoff+y,    0);
                mi->polygon_count++;
              }
        }
        break;

      case BOX:
        glVertex3f (x-bw/2-t2/2, y-bh/2-t2/2, 0);
        glVertex3f (x+bw/2+t2/2, y-bh/2-t2/2, 0);
        glVertex3f (x+bw/2+t2/2, y-bh/2+t2/2, 0);
        glVertex3f (x-bw/2-t2/2, y-bh/2+t2/2, 0);
        mi->polygon_count++;

        glVertex3f (x-bw/2-t2/2, y+bh/2-t2/2, 0);
        glVertex3f (x+bw/2+t2/2, y+bh/2-t2/2, 0);
        glVertex3f (x+bw/2+t2/2, y+bh/2+t2/2, 0);
        glVertex3f (x-bw/2-t2/2, y+bh/2+t2/2, 0);
        mi->polygon_count++;

        glVertex3f (x-bw/2+t2/2, y-bh/2+t2/2, 0);
        glVertex3f (x-bw/2+t2/2, y+bh/2-t2/2, 0);
        glVertex3f (x-bw/2-t2/2, y+bh/2-t2/2, 0);
        glVertex3f (x-bw/2-t2/2, y-bh/2+t2/2, 0);
        mi->polygon_count++;

        glVertex3f (x+bw/2+t2/2, y-bh/2+t2/2, 0);
        glVertex3f (x+bw/2+t2/2, y+bh/2-t2/2, 0);
        glVertex3f (x+bw/2-t2/2, y+bh/2-t2/2, 0);
        glVertex3f (x+bw/2-t2/2, y-bh/2+t2/2, 0);
        mi->polygon_count++;
        break;

      case RETICLE:
        glVertex3f (x+t2/2, y+sy/2-t2/2, 0);
        glVertex3f (x+t2/2, h,           0);
        glVertex3f (x-t2/2, h,           0);
        glVertex3f (x-t2/2, y+sy/2-t2/2, 0);
        mi->polygon_count++;

        glVertex3f (x-t2/2, y-sy/2+t2/2, 0);
        glVertex3f (x-t2/2, 0,           0);
        glVertex3f (x+t2/2, 0,           0);
        glVertex3f (x+t2/2, y-sy/2+t2/2, 0);
        mi->polygon_count++;

        glVertex3f (x-sx/2+t2/2, y+t2/2, 0);
        glVertex3f (0,           y+t2/2, 0);
        glVertex3f (0,           y-t2/2, 0);
        glVertex3f (x-sx/2+t2/2, y-t2/2, 0);
        mi->polygon_count++;

        glVertex3f (x+sx/2-t2/2, y-t2/2, 0);
        glVertex3f (w,           y-t2/2, 0);
        glVertex3f (w,           y+t2/2, 0);
        glVertex3f (x+sx/2-t2/2, y+t2/2, 0);
        mi->polygon_count++;
        break;

      default: abort();
      }
      glEnd();
    }

  glPopMatrix();

  if (!wire) glEnable (GL_TEXTURE_2D);
}


static sprite * find_newest_sprite (ModeInfo *, sprite_type);
static void compute_image_rect (rect *, sprite *, Bool);

static void
draw_text_sprite (ModeInfo *mi, sprite *sp)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat w = MI_WIDTH(mi);
  GLfloat h = MI_HEIGHT(mi);
  GLfloat s;
  int x, y, z;
  XCharStruct e;
  sprite *target = 0;
  char text[255];
  GLfloat color[4];
  int i;

  if (sp->opacity <= 0)
    return;

  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp2 = ss->sprites[i];
      if (sp2->id == sp->text_id && sp2->state != DEAD)
        {
          target = sp2;
          break;
        }
    }

  if (target)
    {
      rect r;
      sprite *img;

      if (target->opacity <= 0 && 
          (target->state == NEW || target->state == IN))
        return;

      r = target->current;

      img = find_newest_sprite (mi, IMAGE);
      if (img)
        compute_image_rect (&r, img, target->back_p);

      mi->recursion_depth = (img
                             ? MIN (img->current.w, img->current.h)
                             : 0);

      x = abs ((int) (r.x * 10000)) % 10000;
      y = abs ((int) (r.y * 10000)) % 10000;
      z = abs ((int) (r.w * 10000)) % 10000;

      sprintf (text, "ZM %04d  NS %04d  EW %04d", z, y, x);

      if ((x == 0 || x == 5000) &&		/* startup */
          (y == 0 || y == 5000) &&
          (z == 0 || z == 5000))
        *text = 0;

      if (do_titles && 
          target->type == IMAGE &&
          target->remain_p)  /* The initial background image */
        {
          char *s = (target->img &&
                     target->img->title && *target->img->title
                     ? target->img->title
                     : "Loading");
          int L = strlen (s);
          int i = (L > 23 ? L-23 : 0);
          sprintf (text, ">>%-23s", target->img->title + i);
          for (s = text; *s; s++)
            if (*s >= 'a' && *s <= 'z') *s += ('A'-'a');
            else if (*s == '/' || *s == '-' || *s == '.') *s = '_';
        }

      if (!*text) return;

      if (sp->text) free (sp->text);
      sp->text = strdup (text);
    }
  else if (sp->text && *sp->text)
    /* The target sprite might be dead, but we saved our last text. */
    strcpy (text, sp->text);
  else
    /* No target, no saved text. */
    return;

  texture_string_metrics (ss->font_data, text, &e, 0, 0);

  glPushMatrix();
  glLoadIdentity();
  glOrtho (0, 1, 0, 1, -1, 1);

  /* Scale the text to fit N characters horizontally. */
  {
# ifdef HAVE_MOBILE
    GLfloat c = 25;
# else /* desktop */
    GLfloat c = (MI_WIDTH(mi) <= 640  ? 25 :
                 MI_WIDTH(mi) <= 1280 ? 32 : 64);
# endif
    s = w / (e.ascent * c);
  }
  w /= s;
  h /= s;
  x = (w - e.width) / 2;
  y = e.ascent + e.descent * 2;

  glScalef (1.0/w, 1.0/h, 1);
  glTranslatef (x, y, 0);

  memcpy (color, ss->text_color, sizeof(color));
  color[3] = sp->opacity;
  glColor4fv (color);

  if (wire)
    glEnable (GL_TEXTURE_2D);

#ifndef HAVE_ANDROID  /* Doesn't work -- prevents image loading? */
  print_texture_string (ss->font_data, text);
# endif
  mi->polygon_count++;

  if (wire)
    glDisable (GL_TEXTURE_2D);
  glPopMatrix();
}


static void
draw_flash_sprite (ModeInfo *mi, sprite *sp)
{
  /* esper_state *ss = &sss[MI_SCREEN(mi)]; */
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat o = sp->opacity;

  if (o <= 0) return;
  o = 0.7;  /* Too fast to see, so keep it consistent */

  glPushMatrix();
  if (!wire)
    glDisable (GL_TEXTURE_2D);
  glColor4f (0, 0, 1, o);
  glColorMask (0, 0, 1, 1); /* write only into blue and alpha channels */
  glBegin (GL_QUADS);
  glVertex3f (0, 0, 0);
  glVertex3f (1, 0, 0);
  glVertex3f (1, 1, 0);
  glVertex3f (0, 1, 0);
  glEnd();
  glColorMask (1, 1, 1, 1);
  if (!wire)
    glEnable (GL_TEXTURE_2D);
  glPopMatrix();
}


static void
draw_sprite (ModeInfo *mi, sprite *sp)
{
  glEnable (GL_BLEND);
  switch (sp->type) {
  case IMAGE:
    draw_image_sprite (mi, sp);
    break;
  case RETICLE:
  case BOX:
  case GRID:
    draw_line_sprite (mi, sp);
    break;
  case TEXT:
    draw_text_sprite (mi, sp);
    break;
  case FLASH:
    draw_flash_sprite (mi, sp);
    break;
  default:
    abort();
  }
}


static void
tick_sprites (ModeInfo *mi)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  int i;

# ifdef USE_ASYNC_LOADER
  double end_by_time = ss->now + ((double) mi->pause) / 2000000;
  for (i = 0; i < ss->nimages; i++)
    {
      image *img = ss->images[i];
      if (img->loader)
        {
          if (texture_loader_failed (img->loader))
            abort();
          step_texture_loader (img->loader, end_by_time - double_time(),
                               image_loaded_cb, img);
        }
    }
# endif

  for (i = 0; i < ss->nsprites; i++)
    tick_sprite (mi, ss->sprites[i]);

  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      if (sp->state == DEAD)
        {
          destroy_sprite (mi, sp);
          i--;
        }
    }
}


static void
draw_sprites (ModeInfo *mi)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
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

  /* Draw the images first, then the overlays. */
  for (i = 0; i < ss->nsprites; i++)
    if (ss->sprites[i]->type == IMAGE)
      draw_sprite (mi, ss->sprites[i]);
  for (i = 0; i < ss->nsprites; i++)
    if (ss->sprites[i]->type != IMAGE)
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


static void
fadeout_sprite (ModeInfo *mi, sprite *sp)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];

  /* If it hasn't faded in yet, don't fade out. */
  if (ss->now <= sp->start_time + sp->pause_duration)
    sp->fade_duration = 0;

  /* Pretend it's at the point where it should fade out. */
  sp->pause_duration = 0;
  sp->duration = 9999;
  sp->remain_p = False;
  sp->start_time = ss->now - sp->duration;
}

static void
fadeout_sprites (ModeInfo *mi, sprite_type type)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp = ss->sprites[i];
      if (sp->type == type)
        fadeout_sprite (mi, sp);
    }
}


static sprite *
find_newest_sprite (ModeInfo *mi, sprite_type type)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  sprite *sp = 0;
  int i;
  for (i = 0; i < ss->nsprites; i++)
    {
      sprite *sp2 = ss->sprites[i];
      if (sp2->type == type &&
          (!sp || 
           (sp->start_time < sp2->start_time &&
            ss->now >= sp2->start_time + sp2->pause_duration)))
        sp = sp2;
    }
  return sp;
}


/* Enqueue a text sprite describing the given sprite that runs at the 
   same time.
 */
static sprite *
push_text_sprite (ModeInfo *mi, sprite *sp)
{
  /* esper_state *ss = &sss[MI_SCREEN(mi)]; */
  sprite *sp2 = new_sprite (mi, TEXT);
  if (!sp2) abort();
  sp2->text_id = sp->id;
  sp2->fade_duration  = sp->fade_duration;
  sp2->duration       = sp->duration;
  sp2->pause_duration = sp->pause_duration;
  return sp2;
}


/* Enqueue a flash sprite that fires at the same time.
 */
#ifndef SMOOTH
static sprite *
push_flash_sprite (ModeInfo *mi, sprite *sp)
{
  /* esper_state *ss = &sss[MI_SCREEN(mi)]; */
  sprite *sp2 = new_sprite (mi, FLASH);
  if (!sp2) abort();
  if (sp->type != IMAGE) abort();
  sp2->text_id = sp->id;
  sp2->duration       = MAX (0.07 / speed, 0.07);
  sp2->fade_duration  = 0;  /* Fading these is too fast to see */
  sp2->pause_duration = sp->pause_duration + (sp->fade_duration * 0.3);
  return sp2;
}
#endif /* !SMOOTH */


/* Set the sprite's duration based on distance travelled.
 */
static void
compute_sprite_duration (ModeInfo *mi, sprite *sp, Bool blink_p)
{
  /* Compute max distance traveled by any point (corners or center). */
  /* (cpp is the devil) */
# define L(F)    (sp->F.x - sp->F.w/2)   /* delta of left edge, from/to */
# define R(F) (1-(sp->F.x + sp->F.w/2))  /* right */
# define B(F)    (sp->F.y - sp->F.h/2)   /* top */
# define T(F) (1-(sp->F.y + sp->F.h/2))  /* bottom */
# define D(F,G) sqrt(F(from)*F(from) + G(to)*G(to))  /* corner traveled */
  double BL = D(B,L);
  double BR = D(B,R);
  double TL = D(T,L);
  double TR = D(T,R);
  double cx = sp->to.x - sp->from.x;
  double cy = sp->to.y - sp->from.y;
  double C  = sqrt(cx*cx + cy*cy);
  double dist = MAX (BL, MAX (BR, MAX (TL, MAX (TR, C))));
# undef L
# undef R
# undef B
# undef T
# undef D

  int steps = 1 + dist * 28;
  if (steps > 10) steps = 10;

  sp->duration = steps * 0.2 / speed;

# ifndef SMOOTH
  sp->duration += 1.5 / speed;  /* For linger added by animate_sprite_path() */
  if (blink_p) sp->duration += 0.6 / speed;
# endif
}


/* Convert the sprite to a jerky transition.
   Instead of smoothly animating, move in discrete steps,
   using multiple staggered sprites.
 */
static void
animate_sprite_path (ModeInfo *mi, sprite *sp, Bool blink_p)
{
# ifndef SMOOTH
  /* esper_state *ss = &sss[MI_SCREEN(mi)]; */
  double dx = sp->to.x - sp->from.x;
  double dy = sp->to.y - sp->from.y;
  double dw = sp->to.w - sp->from.w;
  double dh = sp->to.h - sp->from.h;
  double linger  = 1.5 / speed;
  double blinger = 0.6 / speed;
  double dur = sp->duration - linger - (blink_p ? blinger : 0);
  int steps = dur / 0.3 * speed;  /* step duration in seconds */
  int i;

  if (sp->type == IMAGE)
    steps *= 0.8;

  if (steps < 2)  steps = 2;
  if (steps > 10) steps = 10;

  /* if (dur <= 0.01) abort(); */
  if (dur < 0.01)
    linger = blinger = 0;

  for (i = 0; i <= steps; i++)
    {
      sprite *sp2 = copy_sprite (mi, sp);
      if (!sp2) abort();

      sp2->to.x = (sp->current.x + i * dx / steps);
      sp2->to.y = (sp->current.y + i * dy / steps);
      sp2->to.w = (sp->current.w + i * dw / steps);
      sp2->to.h = (sp->current.h + i * dh / steps);
      sp2->current = sp2->from = sp2->to;
      sp2->duration = dur / steps;
      sp2->pause_duration += i * sp2->duration;
      sp2->remain_p = False;
      sp2->fatbits_p = True;

      if (i == steps)
        sp2->duration += linger;	/* last one lingers for a bit */

      if (i == steps && !blink_p)
        {
          sp2->remain_p = sp->remain_p;
          sp2->fatbits_p = False;
        }

      if (sp2->type == IMAGE && i > 0)
        push_flash_sprite (mi, sp2);

      if (sp2->type == RETICLE || sp2->type == BOX)
        {
          sp2 = push_text_sprite (mi, sp2);
          if (i == steps)
            sp2->duration += linger * 2;
        }
    }

  if (blink_p && blinger)		/* last one blinks before vanishing */
    {
      int blinkers = 3;
      for (i = 1; i <= blinkers; i++)
        {
          sprite *sp2 = copy_sprite (mi, sp);
          if (!sp2) abort();

          sp2->current = sp2->from = sp->to;
          sp2->duration = blinger / blinkers;
          sp2->pause_duration += dur + linger + i * sp2->duration;
          sp2->remain_p = False;
          if (i == blinkers)
            {
              sp2->remain_p = sp->remain_p;
              sp2->fatbits_p = False;
            }
        }
    }

  /* Fade out the template sprite. It might not have even appeared yet. */
  fadeout_sprite (mi, sp);
# endif
}


/* Input rect is of a reticle or box.
   Output rect is what the image's rect should be so that the only part
   visible is the part indicated by the input rect.
 */
static void
compute_image_rect (rect *r, sprite *img, Bool inverse_p)
{
  double scale = (inverse_p ? 1/r->w : r->w);
  double dx = r->x - 0.5;
  double dy = r->y - 0.5;

  /* Adjust size and center by zoom factor */
  r->w = img->current.w / scale;
  r->h = img->current.h / scale;
  r->x = 0.5 + (img->current.x - 0.5) / scale;
  r->y = 0.5 + (img->current.y - 0.5) / scale;

  /* Move center */

  if (inverse_p)
    {
      dx = -dx;		/* #### Close but not quite right */
      dy = -dy;
    }

  r->x -= dx / scale;
  r->y -= dy / scale;
}


/* Sets 'to' such that the image zooms out so that the only part visible
   is the part indicated by the box.
 */
static void
track_box_with_image (ModeInfo *mi, sprite *sp, sprite *img)
{
  rect r = sp->current;
  compute_image_rect (&r, img, sp->back_p);
  img->to = r;

  /* Never zoom out too far. */
  if (img->to.w < 1 && img->to.h < 1)
    {
      if (img->to.w > img->to.h)
        {
          img->to.w = img->to.w / img->to.h;
          img->to.h = 1;
        }
      else
        {
          img->to.h = img->to.h / img->to.w;
          img->to.w = 1;
        }
    }

  /* Never pan beyond the bounds of the image. */
  if (img->to.x < -img->to.w/2+1) img->to.x = -img->to.w/2+1;
  if (img->to.x >  img->to.w/2)   img->to.x =  img->to.w/2;
  if (img->to.y < -img->to.h/2+1) img->to.y = -img->to.h/2+1;
  if (img->to.y >  img->to.h/2)   img->to.y =  img->to.h/2;
}


static void
tick_animation (ModeInfo *mi)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  anim_state prev_state = ss->anim_state;
  sprite *sp = 0;
  int i;

  switch (ss->anim_state) {
  case BLANK:
    ss->anim_state = GRID_ON;
    break;
  case GRID_ON:
    ss->anim_state = IMAGE_LOAD;
    break;
  case IMAGE_LOAD:
    /* Only advance once an image has loaded. */
    if (find_newest_sprite (mi, IMAGE))
      ss->anim_state = RETICLE_ON;
    else {
      ss->anim_state = IMAGE_LOAD;
    }
    break;
  case RETICLE_ON:
    ss->anim_state = RETICLE_MOVE;
    break;
  case RETICLE_MOVE:
    if (random() % 6)
      ss->anim_state = BOX_MOVE;
    else
      ss->anim_state = IMAGE_ZOOM;
    break;
  case BOX_MOVE:
    ss->anim_state = IMAGE_ZOOM;
    break;
  case IMAGE_ZOOM:
    {
      sprite *sp = find_newest_sprite (mi, IMAGE);
      double depth = (sp
                      ? MIN (sp->current.w, sp->current.h)
                      : 0);
      if (depth > 20)
        ss->anim_state = IMAGE_UNLOAD;
      else
        ss->anim_state = RETICLE_ON;
    }
    break;
  case IMAGE_FORCE_UNLOAD:
    ss->anim_state = IMAGE_UNLOAD;
    break;
  case IMAGE_UNLOAD:
    ss->anim_state = IMAGE_LOAD;
    break;
  case MANUAL_BOX_ON:
    ss->anim_state = MANUAL_BOX;
    break;
  case MANUAL_BOX:
    break;
  case MANUAL_RETICLE_ON:
    ss->anim_state = MANUAL_RETICLE;
    break;
  case MANUAL_RETICLE:
    break;
  default:
    abort();
    break;
  }

  ss->anim_start = ss->now;
  ss->anim_duration = 0;

  if (debug_p)
    fprintf (stderr, "%s: entering %s\n",
             progname, state_name (ss->anim_state));

  switch (ss->anim_state) {

  case GRID_ON:		/* Start the grid fading in. */
    if (! find_newest_sprite (mi, GRID))
      {
        sp = new_sprite (mi, GRID);
        if (!sp) abort();
        sp->fade_duration = 1.0 / speed;
        sp->duration = 2.0 / speed;
        sp->remain_p = True;
        ss->anim_duration = (sp->pause_duration + sp->fade_duration * 2 +
                             sp->duration);
      }
    break;

  case IMAGE_LOAD:
    fadeout_sprites (mi, IMAGE);
    sp = new_sprite (mi, IMAGE);
    if (! sp) 
      {
        if (debug_p) fprintf (stderr, "%s: image load failed\n", progname);
        break;
      }

    sp->fade_duration = 0.5 / speed;
    sp->duration = sp->fade_duration * 3;
    sp->remain_p = True;
    /* If we zoom in, we lose the pulse at the end. */
    /* sp->from.w = sp->from.h = 0.0001; */
    sp->current = sp->from;

    ss->anim_duration = (sp->pause_duration + sp->fade_duration * 2 +
                         sp->duration);

    sp = push_text_sprite (mi, sp);
    sp->fade_duration = 0.2 / speed;
    sp->pause_duration = 0;
    sp->duration = 2.5 / speed;
    break;

  case IMAGE_FORCE_UNLOAD:
    break;

  case IMAGE_UNLOAD:
    sp = find_newest_sprite (mi, IMAGE);    
    if (sp)
      sp->fade_duration = ((prev_state == IMAGE_FORCE_UNLOAD ? 0.2 : 3.0)
                           / speed);
    fadeout_sprites (mi, IMAGE);
    fadeout_sprites (mi, RETICLE);
    fadeout_sprites (mi, BOX);
    fadeout_sprites (mi, TEXT);
    ss->anim_duration = (sp ? sp->fade_duration : 0) + 3.5 / speed;
    break;

  case RETICLE_ON:		/* Display reticle at center. */
    fadeout_sprites (mi, TEXT);
    sp = new_sprite (mi, RETICLE);
    if (!sp) abort();
    sp->fade_duration  = 0.2 / speed;
    sp->pause_duration = 1.0 / speed;
    sp->duration       = 1.5 / speed;
    ss->anim_duration = (sp->pause_duration + sp->fade_duration * 2 +
                         sp->duration);
    ss->anim_duration -= sp->fade_duration * 2;
    break;

  case RETICLE_MOVE:
    /* Reticle has faded in.  Now move it to somewhere else.
       Create N new reticle sprites, wih staggered pause_durations.
     */
    {
      GLfloat ox = 0.5;
      GLfloat oy = 0.5;
      GLfloat nx, ny, dist;

      do {		/* pick a new position not too near the old */
        nx = 0.3 + BELLRAND(0.4);
        ny = 0.3 + BELLRAND(0.4);
        dist = sqrt ((nx-ox)*(nx-ox) + (ny-oy)*(ny-oy));
      } while (dist < 0.1);

      sp = new_sprite (mi, RETICLE);
      if (!sp) abort();

      sp->from.x = ox;
      sp->from.y = oy;
      sp->current = sp->to = sp->from;
      sp->to.x = nx;
      sp->to.y = ny;
      sp->fade_duration  = 0.2 / speed;
      sp->pause_duration = 0;
      compute_sprite_duration (mi, sp, False);

      ss->anim_duration = (sp->pause_duration + sp->fade_duration * 2 +
                           sp->duration - 0.1);
      animate_sprite_path (mi, sp, False);
    }
    break;

  case BOX_MOVE:
    /* Reticle has moved, and faded out.
       Start the box zooming into place.
     */
    {
      GLfloat ox = 0.5;
      GLfloat oy = 0.5;
      GLfloat nx, ny;
      GLfloat z;

      /* Find the last-added reticle, for our destination position. */
      sp = 0;
      for (i = 0; i < ss->nsprites; i++)
        {
          sprite *sp2 = ss->sprites[i];
          if (sp2->type == RETICLE &&
              (!sp || sp->start_time < sp2->start_time))
            sp = sp2;
        }
      if (sp)
        {
          nx = sp->to.x;
          ny = sp->to.y;
        }
      else
        {
          nx = ny = 0.5;
          if (debug_p)
            fprintf (stderr, "%s: no reticle before box?\n", progname);
        }

      z = 0.3 + frand(0.5);

      /* Ensure that the selected box is contained within the screen */
      {
        double margin = 0.005;
        double maxw = 2 * MIN (1 - margin - nx, nx - margin);
        double maxh = 2 * MIN (1 - margin - ny, ny - margin);
        double max = MIN (maxw, maxh);
        if (z > max) z = max;
      }

      sp = new_sprite (mi, BOX);
      if (!sp) abort();
      sp->from.x = ox;
      sp->from.y = oy;
      sp->from.w = 1.0;
      sp->from.h = 1.0;
      sp->current = sp->from;
      sp->to.x = nx;
      sp->to.y = ny;
      sp->to.w = z;
      sp->to.h = z;

      /* Maybe zoom out instead of in.
       */
      {
        sprite *img = find_newest_sprite (mi, IMAGE);
        double depth = MIN (img->current.w, img->current.h);
        if (depth > 1 &&  /* if zoomed in */
            (depth < 6 ?  !(random() % 5) :   /* 20% */
             depth < 12 ? !(random() % 2) :   /* 50% */
             (random() % 3)))                 /* 66% */
          {
            sp->back_p = True;
            if (depth < 1.5 && z < 0.8)
              {
                z = 0.8;	/* don't zoom out much past 100% */
                sp->to.w = z;
                sp->to.h = z;
              }
          }
      }

      sp->fade_duration  = 0.2 / speed;
      sp->pause_duration = 2.0 / speed;
      compute_sprite_duration (mi, sp, True);
      ss->anim_duration = (sp->pause_duration + sp->fade_duration * 2 +
                           sp->duration - 0.1);
      animate_sprite_path (mi, sp, True);
    }
    break;

  case IMAGE_ZOOM:

    /* Box has moved, and faded out.
       Or, if no box, then just a reticle.
       Zoom the underlying image to track the box's position. */
    {
      sprite *img, *img2;

      /* Find latest box or reticle, for our destination position. */
      sp = find_newest_sprite (mi, BOX);
      if (! sp)
        sp = find_newest_sprite (mi, RETICLE);
      if (! sp)
        {
          if (debug_p)
            fprintf (stderr, "%s: no box or reticle before image\n",
                     progname);
          break;
        }

      img = find_newest_sprite (mi, IMAGE);
      if (!img)
        {
          if (debug_p)
            fprintf (stderr, "%s: no image?\n", progname);
          break;
        }

      img2 = copy_sprite (mi, img);
      if (!img2) abort();

      img2->from = img->current;

      fadeout_sprite (mi, img);

      track_box_with_image (mi, sp, img2);

      img2->fade_duration  = 0.2 / speed;
      img2->pause_duration = 0.5 / speed;
      img2->remain_p = True;
      img2->throb_p = False;
      compute_sprite_duration (mi, img2, False);

      img->start_time += img2->pause_duration;

      ss->anim_duration = (img2->pause_duration + img2->fade_duration * 2 +
                           img2->duration);
      animate_sprite_path (mi, img2, False);
      fadeout_sprites (mi, TEXT);
    }
    break;

  case MANUAL_BOX_ON:
  case MANUAL_RETICLE_ON:
    break;

  case MANUAL_BOX:
  case MANUAL_RETICLE:
    {
      sprite_type tt = (ss->anim_state == MANUAL_BOX ? BOX : RETICLE);
      sprite *osp = find_newest_sprite (mi, tt);
      fadeout_sprites (mi, RETICLE);
      fadeout_sprites (mi, BOX);

      sp = new_sprite (mi, tt);
      if (!sp) abort();
      if (osp)
        sp->from = osp->current;
      else
        {
          sp->from.x = 0.5;
          sp->from.y = 0.5;
          sp->from.w = 0.5;
          sp->from.h = 0.5;
        }
      sp->to = sp->current = sp->from;
      sp->fade_duration = 0.2 / speed;
      sp->duration = 0.2 / speed;
      sp->remain_p = True;
      sp->throb_p = False;
      ss->anim_duration = 9999;
    }
    break;

  default:
    fprintf (stderr,"%s: unknown state %d\n",
             progname, (int) ss->anim_state);
    abort();
  }
}


/* Copied from gltrackball.c, sigh.
 */
static void
adjust_for_device_rotation (double *x, double *y, double *w, double *h)
{
  int rot = (int) current_device_rotation();
  int swap;

  while (rot <= -180) rot += 360;
  while (rot >   180) rot -= 360;

  if (rot > 135 || rot < -135)		/* 180 */
    {
      *x = *w - *x;
      *y = *h - *y;
    }
  else if (rot > 45)			/* 90 */
    {
      swap = *x; *x = *y; *y = swap;
      swap = *w; *w = *h; *h = swap;
      *x = *w - *x;
    }
  else if (rot < -45)			/* 270 */
    {
      swap = *x; *x = *y; *y = swap;
      swap = *w; *w = *h; *h = swap;
      *y = *h - *y;
    }
}


ENTRYPOINT Bool
esper_handle_event (ModeInfo *mi, XEvent *event)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];

  if (event->xany.type == Expose ||
      event->xany.type == GraphicsExpose ||
      event->xany.type == VisibilityNotify)
    {
      return False;
    }
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      sprite *sp = 0;
      double delta = 0.025;
      double margin = 0.005;
      Bool ok = False;

      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if (c == '\t')
        {
          ss->anim_state = IMAGE_FORCE_UNLOAD;
          return True;
        }

      if (! find_newest_sprite (mi, IMAGE))
        return False;  /* Too early */

      ss->now = double_time();

# define BONK() do {				      \
	if (ss->anim_state != MANUAL_BOX_ON &&	      \
	    ss->anim_state != MANUAL_BOX) {	      \
	  ss->anim_state = MANUAL_BOX_ON;	      \
	  tick_animation (mi);			      \
	}					      \
	sp = find_newest_sprite (mi, BOX);	      \
	if (!sp) abort() ;			      \
	sp->from = sp->current;			      \
	sp->to = sp->from;			      \
	sp->start_time = ss->now - sp->fade_duration; \
	ok = True;				      \
      } while(0)

      if (keysym == XK_Left || c == ',' || c == '<')
        {
          BONK();
          sp->to.x -= delta;
        }
      else if (keysym == XK_Right || c == '.' || c == '>')
        {
          BONK();
          sp->to.x += delta;
        }
      else if (keysym == XK_Down || c == '-')
        {
          BONK();
          sp->to.y -= delta;
        }
      else if (keysym == XK_Up || c == '=')
        {
          BONK();
          sp->to.y += delta, ok = True;
        }
      else if (keysym == XK_Prior || c == '+')
        {
          BONK();
          sp->to.w += delta;
          sp->to.h = sp->to.w * sp->from.w / sp->from.h;
        }
      else if (keysym == XK_Next || c == '_')
        {
          BONK();
          sp->to.w -= delta;
          sp->to.h = sp->to.w * sp->from.w / sp->from.h;
        }
      else if ((c == ' ' || c == '\t') && debug_p &&
               ss->anim_state == MANUAL_BOX)
        {
          BONK();     /* Null motion: just flash the current image. */
        }
      else if ((keysym == XK_Home || c == ' ' || c == '\t') &&
               ss->anim_state == MANUAL_BOX)
        {
          BONK();
          sp->to.x = 0.5;
          sp->to.y = 0.5;
          sp->to.w = 0.5;
          sp->to.h = 0.5;
        }
      else if ((c == '\r' || c == '\n' || c == 033) &&
               ss->anim_state == MANUAL_BOX)
        {
          BONK();
          ss->anim_state = BOX_MOVE;
          ss->anim_duration = 9999;
          ss->anim_start = ss->now - ss->anim_duration;
          fadeout_sprite (mi, sp);
          return True;
        }
      else
        return False;

      if (! ok)
        return False;

      /* Keep it on screen */
      if (sp->to.w > 1 - margin)
        {
          GLfloat r = sp->to.h / sp->to.w;
          sp->to.w = 1-margin;
          sp->to.h = (1-margin) * r;
        }
      if (sp->to.h > 1)
        {
          GLfloat r = sp->to.h / sp->to.w;
          sp->to.w = (1-margin) / r;
          sp->to.h = 1-margin;
        }

      if (sp->to.x - sp->to.w/2 < margin)
        sp->to.x = sp->to.w/2 + margin;
      if (sp->to.y - sp->to.h/2 < margin)
        sp->to.y = sp->to.h/2 + margin;

      if (sp->to.x + sp->to.w/2 >= 1 + margin)
        sp->to.x = 1 - (sp->to.w/2 + margin);
      if (sp->to.y + sp->to.h/2 >= 1 + margin)
        sp->to.y = 1 - (sp->to.h/2 + margin);

      /* Now let's give a momentary glimpse of what the image would do. */
      if (debug_p)
        {
          sprite *img = 0;
          int i;

          /* Find the lingering image */
          /* img = find__sprite (mi, IMAGE); */
          for (i = 0; i < ss->nsprites; i++)
            {
              sprite *sp2 = ss->sprites[i];
              if (sp2->type == IMAGE &&
                  sp2->remain_p &&
                  (!img || 
                   (img->start_time < sp2->start_time &&
                    ss->now >= sp2->start_time + sp2->pause_duration)))
                img = sp2;
            }

          if (!img) abort();
          img = copy_sprite (mi, img);
          img->pause_duration = 0;
          img->fade_duration = 0.1 / speed;
          img->duration = 0.5 / speed;
          img->start_time = ss->now;
          img->remain_p = False;
          track_box_with_image (mi, sp, img);
          img->from = img->current = img->to;
        }

      return True;
    }
  else if (event->xany.type == ButtonPress &&
           event->xbutton.button == Button1)
    {
      ss->button_down_p = 1;
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      ss->button_down_p = 0;

      if (ss->anim_state == MANUAL_BOX)
        {
          sprite *sp = find_newest_sprite (mi, BOX);
          if (sp) fadeout_sprite (mi, sp);
          ss->anim_state = BOX_MOVE;
          ss->anim_duration = 9999;
          ss->anim_start = ss->now - ss->anim_duration;
        }
      else if (ss->anim_state == MANUAL_RETICLE)
        {
          sprite *sp = find_newest_sprite (mi, RETICLE);
          if (sp) fadeout_sprite (mi, sp);
          ss->anim_state = RETICLE_MOVE;
          ss->anim_duration = 9999;
          ss->anim_start = ss->now - ss->anim_duration;
        }
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           ss->button_down_p &&
           (ss->anim_state == MANUAL_RETICLE ||
            ss->anim_state == RETICLE_MOVE))
    {
      sprite *sp = 0;
      double x = event->xmotion.x;
      double y = event->xmotion.y;
      double w = MI_WIDTH(mi);
      double h = MI_HEIGHT(mi);

      adjust_for_device_rotation (&x, &y, &w, &h);
      x = x/w;
      y = 1-y/h;

      if (ss->anim_state != MANUAL_RETICLE_ON &&
          ss->anim_state != MANUAL_RETICLE)
        {
	  ss->anim_state = MANUAL_RETICLE_ON;
	  tick_animation (mi);
	}
      sp = find_newest_sprite (mi, RETICLE);
      if (!sp) abort();
      sp->from = sp->current;
      sp->to = sp->from;
      sp->start_time = ss->now - sp->fade_duration;
      sp->remain_p = True;

      sp->current.x = MIN (0.95, MAX (0.05, x));
      sp->current.y = MIN (0.95, MAX (0.05, y));
      sp->from = sp->to = sp->current;

      /* Don't update the text sprite more often than once a second. */
      {
        sprite *sp2 = find_newest_sprite (mi, TEXT);
        if (!sp2 || sp2->start_time < ss->now-1)
          {
            fadeout_sprites (mi, TEXT);
            sp = push_text_sprite (mi, sp);
            sp->remain_p = True;
          }
      }

      return True;
    }
  else if (event->xany.type == MotionNotify &&
           ss->button_down_p &&
           (ss->anim_state == MANUAL_BOX ||
            ss->anim_state == BOX_MOVE))
    {
      sprite *sp = 0;
      double x = event->xmotion.x;
      double y = event->xmotion.y;
      double w = MI_WIDTH(mi);
      double h = MI_HEIGHT(mi);
      double max;
      Bool ok = True;

      adjust_for_device_rotation (&x, &y, &w, &h);
      x = x/w;
      y = 1-y/h;

      BONK();
      max = (2 * (0.5 - MAX (fabs (sp->current.x - 0.5),
                             fabs (sp->current.y - 0.5)))
             * 0.95);

      x = fabs (x - sp->current.x);
      y = fabs (y - sp->current.y);

      if (x > y)
        sp->current.w = sp->current.h = MIN (max, MAX (0.05, 2*x));
      else
        sp->current.w = sp->current.h = MIN (max, MAX (0.05, 2*y));
      sp->from = sp->to = sp->current;

      /* Don't update the text sprite more often than once a second. */
      {
        sprite *sp2 = find_newest_sprite (mi, TEXT);
        if (!sp2 || sp2->start_time < ss->now-1)
          {
            fadeout_sprites (mi, TEXT);
            sp = push_text_sprite (mi, sp);
            sp->remain_p = True;
          }
      }

      return ok;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      ss->anim_state = IMAGE_FORCE_UNLOAD;
      return True;
    }
# undef BONK

  return False;
}


ENTRYPOINT void
reshape_esper (ModeInfo *mi, int width, int height)
{
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
      s *= 0.75;
      if (s < 0.1) s = 0.1;
    }

  glScalef (s, s, s);
  glTranslatef (-0.5, -0.5, 0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* Stretch each existing image to match new window aspect. */
  {
    esper_state *ss = &sss[MI_SCREEN(mi)];
    int i;
    for (i = 0; i < ss->nsprites; i++)
      {
        sprite *sp = ss->sprites[i];
        if (sp && sp->type == IMAGE && sp->img && sp->img->loaded_p)
          {
            GLfloat sp_asp  = sp->current.h / sp->current.w;
            GLfloat img_asp = (sp->img->geom.height /
                               (GLfloat) sp->img->geom.width);
            GLfloat new_win = (MI_WIDTH(mi) / (double) MI_HEIGHT(mi));
            GLfloat old_win = sp_asp / img_asp;
            GLfloat r = old_win / new_win;
            if (img_asp > 1)
              {
                sp->from.h    /= r;
                sp->current.h /= r;
                sp->to.h      /= r;
              }
            else
              {
                sp->from.w    *= r;
                sp->current.w *= r;
                sp->to.w      *= r;
              }
          }
      }
  }
}


static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "EsperColor");
  if (!XParseColor (mi->dpy, mi->xgwa.colormap, string, &xcolor))
    {
      fprintf (stderr, "%s: unparsable color in %s: %s\n", progname,
               key, string);
      exit (1);
    }
  free (string);

  color[0] = xcolor.red   / 65536.0;
  color[1] = xcolor.green / 65536.0;
  color[2] = xcolor.blue  / 65536.0;
  color[3] = 1;
}


ENTRYPOINT void
init_esper (ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  esper_state *ss;
  
  MI_INIT (mi, sss);
  ss = &sss[screen];

  if ((ss->glx_context = init_GL(mi)) != NULL) {
    reshape_esper (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }

  parse_color (mi, "gridColor", ss->grid_color);
  parse_color (mi, "reticleColor", ss->reticle_color);
  parse_color (mi, "textColor", ss->text_color);

  ss->font_data = load_texture_font (mi->dpy, "titleFont");

  ss->now = double_time();
  ss->dawn_of_time = ss->now;

  alloc_image (mi);

  ss->anim_state = BLANK;
  ss->anim_start = 0;
  ss->anim_duration = 0;
}


ENTRYPOINT void
draw_esper (ModeInfo *mi)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  if (!ss->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ss->glx_context);

  mi->polygon_count = 0;

  ss->now = double_time();

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

  tick_sprites (mi);
  draw_sprites (mi);
  if (ss->now >= ss->anim_start + ss->anim_duration)
    tick_animation (mi);

  if (mi->fps_p) do_fps (mi);

  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}


ENTRYPOINT void
free_esper (ModeInfo *mi)
{
  esper_state *ss = &sss[MI_SCREEN(mi)];
  int i;
  if (!ss->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ss->glx_context);

  if (ss->font_data) free_texture_font (ss->font_data);
  for (i = 0; i < ss->nimages; i++) {
    if (ss->images[i]) {
      if (ss->images[i]->title) free (ss->images[i]->title);
      if (ss->images[i]->texid) glDeleteTextures (1, &ss->images[i]->texid);
      free (ss->images[i]);
    }
  }
  for (i = 0; i < countof(ss->sprites); i++) {
    if (ss->sprites[i]) {
      if (ss->sprites[i]->text) free (ss->sprites[i]->text);
      if (ss->sprites[i]) free (ss->sprites[i]);
    }
  }
}

XSCREENSAVER_MODULE ("Esper", esper)

#endif /* USE_GL */
