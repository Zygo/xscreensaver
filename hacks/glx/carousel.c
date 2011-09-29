/* carousel, Copyright (c) 2005-2011 Jamie Zawinski <jwz@jwz.org>
 * Loads a sequence of images and rotates them around.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Created: 21-Feb-2005
 */

#define DEF_FONT "-*-helvetica-bold-r-normal-*-240-*"
#define DEFAULTS  "*count:           7         \n" \
		  "*delay:           10000     \n" \
		  "*wireframe:       False     \n" \
                  "*showFPS:         False     \n" \
	          "*fpsSolid:        True      \n" \
	          "*useSHM:          True      \n" \
		  "*font:	   " DEF_FONT "\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
		  "*grabDesktopImages:   False \n" \
		  "*chooseRandomImages:  True  \n"

# define refresh_carousel 0
# define release_carousel 0
# include "xlockmore.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef USE_GL

# define DEF_SPEED          "1.0"
# define DEF_DURATION	    "20"
# define DEF_TITLES         "True"
# define DEF_ZOOM           "True"
# define DEF_TILT           "XY"
# define DEF_MIPMAP         "True"
# define DEF_DEBUG          "False"

#include "rotator.h"
#include "gltrackball.h"
#include "grab-ximage.h"
#include "texfont.h"

# ifndef HAVE_COCOA
#  include <X11/Intrinsic.h>     /* for XrmDatabase in -debug mode */
# endif

/* Should be in <GL/glext.h> */
# ifndef  GL_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
# endif
# ifndef  GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
# endif

typedef struct {
  double x, y, w, h;
} rect;

typedef enum { EARLY, NORMAL, LOADING, OUT, IN, DEAD } fade_mode;
static int fade_ticks = 60;

typedef struct {
  char *title;			/* the filename of this image */
  int w, h;			/* size in pixels of the image */
  int tw, th;			/* size in pixels of the texture */
  XRectangle geom;		/* where in the image the bits are */
  GLuint texid;
} image;

typedef struct {
  ModeInfo *mi;
  image current, loading;
  GLfloat r, theta;		/* radius and rotation on the tube */
  rotator *rot;			/* for zoomery */
  Bool from_top_p;		/* whether this image drops in or rises up */
  time_t expires;		/* when this image should be replaced */
  fade_mode mode;		/* in/out animation state */
  int mode_tick;
  Bool loaded_p;		/* whether background load is done */
} image_frame;


typedef struct {
  GLXContext *glx_context;
  GLfloat anisotropic;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  time_t button_down_time;

  int nframes;			/* how many frames are loaded */
  int frames_size;
  image_frame **frames;		/* pointers to the frames */

  Bool awaiting_first_images_p;
  int loads_in_progress;

  texture_font_data *texfont;

  fade_mode mode;
  int mode_tick;

  int loading_sw, loading_sh;

  time_t last_time, now;
  int draw_tick;

} carousel_state;

static carousel_state *sss = NULL;


/* Command-line arguments
 */
static GLfloat speed;	    /* animation speed scale factor */
static int duration;	    /* reload images after this long */
static Bool mipmap_p;	    /* Use mipmaps instead of single textures. */
static Bool titles_p;	    /* Display image titles. */
static Bool zoom_p;	    /* Throb the images in and out as they spin. */
static char *tilt_str;
static Bool tilt_x_p;	    /* Tilt axis towards the viewer */
static Bool tilt_y_p;	    /* Tilt axis side to side */
static Bool debug_p;	    /* Be loud and do weird things. */


static XrmOptionDescRec opts[] = {
  {"-zoom",         ".zoom",          XrmoptionNoArg, "True"  },
  {"-no-zoom",      ".zoom",          XrmoptionNoArg, "False" },
  {"-tilt",         ".tilt",          XrmoptionSepArg, 0  },
  {"-no-tilt",      ".tilt",          XrmoptionNoArg, ""  },
  {"-titles",       ".titles",        XrmoptionNoArg, "True"  },
  {"-no-titles",    ".titles",        XrmoptionNoArg, "False" },
  {"-mipmaps",      ".mipmap",        XrmoptionNoArg, "True"  },
  {"-no-mipmaps",   ".mipmap",        XrmoptionNoArg, "False" },
  {"-duration",	    ".duration",      XrmoptionSepArg, 0 },
  {"-debug",        ".debug",         XrmoptionNoArg, "True"  },
  {"-font",         ".font",          XrmoptionSepArg, 0 },
  {"-speed",        ".speed",         XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  { &mipmap_p,      "mipmap",       "Mipmap",       DEF_MIPMAP,      t_Bool},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,       t_Bool},
  { &titles_p,      "titles",       "Titles",       DEF_TITLES,      t_Bool},
  { &zoom_p,        "zoom",         "Zoom",         DEF_ZOOM,        t_Bool},
  { &tilt_str,      "tilt",         "Tilt",         DEF_TILT,        t_String},
  { &speed,         "speed",        "Speed",        DEF_SPEED,       t_Float},
  { &duration,      "duration",     "Duration",     DEF_DURATION,    t_Int},
};

ENTRYPOINT ModeSpecOpt carousel_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Allocates a frame structure and stores it in the list.
 */
static image_frame *
alloc_frame (ModeInfo *mi)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  image_frame *frame = (image_frame *) calloc (1, sizeof (*frame));

  frame->mi = mi;
  frame->mode = EARLY;
  frame->rot = make_rotator (0, 0, 0, 0, 0.04 * frand(1.0) * speed, False);

  glGenTextures (1, &frame->current.texid);
  glGenTextures (1, &frame->loading.texid);
  if (frame->current.texid <= 0) abort();
  if (frame->loading.texid <= 0) abort();

  if (ss->frames_size <= ss->nframes)
    {
      ss->frames_size = (ss->frames_size * 1.2) + ss->nframes;
      ss->frames = (image_frame **)
        realloc (ss->frames, ss->frames_size * sizeof(*ss->frames));
      if (! ss->frames)
        {
          fprintf (stderr, "%s: out of memory (%d images)\n",
                   progname, ss->frames_size);
          exit (1);
        }
    }

  ss->frames[ss->nframes++] = frame;

  return frame;
}


static void image_loaded_cb (const char *filename, XRectangle *geom,
                             int image_width, int image_height,
                             int texture_width, int texture_height,
                             void *closure);


/* Load a new file into the given image struct.
 */
static void
load_image (ModeInfo *mi, image_frame *frame)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  if (debug_p && !wire && frame->current.w != 0)
    fprintf (stderr, "%s:  dropped %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname, 
             frame->current.geom.width, 
             frame->current.geom.height, 
             frame->current.tw, frame->current.th,
             (frame->current.title ? frame->current.title : "(null)"));

  switch (frame->mode) 
    {
    case EARLY:  break;
    case NORMAL: frame->mode = LOADING; break;
    default: abort();
    }

  ss->loads_in_progress++;

  if (wire)
    image_loaded_cb (0, 0, 0, 0, 0, 0, frame);
  else
    {
      int w = (MI_WIDTH(mi)  / 2) - 1;
      int h = (MI_HEIGHT(mi) / 2) - 1;
      if (w <= 10) w = 10;
      if (h <= 10) h = 10;
      load_texture_async (mi->xgwa.screen, mi->window, *ss->glx_context, w, h,
                          mipmap_p, frame->loading.texid, 
                          image_loaded_cb, frame);
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
  image_frame *frame = (image_frame *) closure;
  ModeInfo *mi = frame->mi;
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  if (wire)
    {
      frame->loading.w = MI_WIDTH (mi) * (0.5 + frand (1.0));
      frame->loading.h = MI_HEIGHT (mi);
      frame->loading.geom.width  = frame->loading.w;
      frame->loading.geom.height = frame->loading.h;
      goto DONE;
    }

  if (image_width == 0 || image_height == 0)
    exit (1);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

  if (ss->anisotropic >= 1.0)
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
                     ss->anisotropic);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  frame->loading.w  = image_width;
  frame->loading.h  = image_height;
  frame->loading.tw = texture_width;
  frame->loading.th = texture_height;
  frame->loading.geom = *geom;

  if (frame->loading.title)
    free (frame->loading.title);
  frame->loading.title = (filename ? strdup (filename) : 0);

  /* xscreensaver-getimage returns paths relative to the image directory
     now, so leave the sub-directory part in.  Unless it's an absolute path.
  */
  if (frame->loading.title && frame->loading.title[0] == '/')
    {    /* strip filename to part after last /. */
      char *s = strrchr (frame->loading.title, '/');
      if (s) strcpy (frame->loading.title, s+1);
    }

  if (debug_p)
    fprintf (stderr, "%s:   loaded %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname,
             frame->loading.geom.width, 
             frame->loading.geom.height, 
             frame->loading.tw, frame->loading.th,
             (frame->loading.title ? frame->loading.title : "(null)"));

 DONE:

  frame->loaded_p = True;

  if (ss->loads_in_progress <= 0) abort();
  ss->loads_in_progress--;

  /* This image expires N seconds after it finished loading. */
  frame->expires = time((time_t *) 0) + (duration * MI_COUNT(mi));

  switch (frame->mode) 
    {
    case EARLY:		/* part of the initial batch of images */
      {
        image swap = frame->current;
        frame->current = frame->loading;
        frame->loading = swap;
      }
      break;
    case LOADING:	/* start dropping the old image out */
      {
        frame->mode = OUT;
        frame->mode_tick = fade_ticks / speed;
        frame->from_top_p = random() & 1;
      }
      break;
    default:
      abort();
    }
}


static void loading_msg (ModeInfo *mi, int n);

static Bool
load_initial_images (ModeInfo *mi)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int i;
  Bool all_loaded_p = True;
  for (i = 0; i < ss->nframes; i++)
    if (! ss->frames[i]->loaded_p)
      all_loaded_p = False;

  if (all_loaded_p)
    {
      if (ss->nframes < MI_COUNT (mi))
        {
          /* The frames currently on the list are fully loaded.
             Start the next one loading.  (We run the image loader
             asynchronously, but we load them one at a time.)
           */
          load_image (mi, alloc_frame (mi));
        }
      else
        {
          /* The first batch of images are now all loaded!
             Stagger the expire times so that they don't all drop out at once.
           */
          time_t now = time((time_t *) 0);
          int i;

          for (i = 0; i < ss->nframes; i++)
            {
              image_frame *frame = ss->frames[i];
              frame->r = 1.0;
              frame->theta = i * 360.0 / ss->nframes;
              frame->expires = now + (duration * (i + 1));
              frame->mode = NORMAL;
            }

          /* Instead of always going clockwise, shuffle the expire times
             of the frames so that they drop out in a random order.
          */
          for (i = 0; i < ss->nframes; i++)
            {
              image_frame *frame1 = ss->frames[i];
              image_frame *frame2 = ss->frames[random() % ss->nframes];
              time_t swap = frame1->expires;
              frame1->expires = frame2->expires;
              frame2->expires = swap;
            }

          ss->awaiting_first_images_p = False;
        }
    }
      
  loading_msg (mi, ss->nframes-1);

  return !ss->awaiting_first_images_p;
}


ENTRYPOINT void
reshape_carousel (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (60.0, 1/h, 1.0, 8.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 2.6,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
carousel_handle_event (ModeInfo *mi, XEvent *event)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      if (! ss->button_down_p)
        ss->button_down_time = time((time_t *) 0);
      ss->button_down_p = True;
      gltrackball_start (ss->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      if (ss->button_down_p)
        {
          /* Add the time the mouse was held to the expire times of all
             frames, so that mouse-dragging doesn't count against
             image expiration.
           */
          int secs = time((time_t *) 0) - ss->button_down_time;
          int i;
          for (i = 0; i < ss->nframes; i++)
            ss->frames[i]->expires += secs;
        }
      ss->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (ss->trackball, event->xbutton.button, 5,
                              !event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           ss->button_down_p)
    {
      gltrackball_track (ss->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


/* Kludge to add "-v" to invocation of "xscreensaver-getimage" in -debug mode
 */
static void
hack_resources (Display *dpy)
{
# ifndef HAVE_COCOA
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
# endif /* !HAVE_COCOA */
}


static void
loading_msg (ModeInfo *mi, int n)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  char text[100];

  if (wire) return;

  if (n == 0)
    sprintf (text, "Loading images...");
  else
    sprintf (text, "Loading images...  (%d%%)",
             (int) (n * 100 / MI_COUNT(mi)));

  if (ss->loading_sw == 0)    /* only do this once, so that the string doesn't move. */
    ss->loading_sw = texture_string_width (ss->texfont, text, &ss->loading_sh);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi));

  glTranslatef ((MI_WIDTH(mi)  - ss->loading_sw) / 2,
                (MI_HEIGHT(mi) - ss->loading_sh) / 2,
                0);
  glColor3f (1, 1, 0);
  glEnable (GL_TEXTURE_2D);
  glDisable (GL_DEPTH_TEST);
  print_texture_string (ss->texfont, text);
  glEnable (GL_DEPTH_TEST);
  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);

  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}


ENTRYPOINT void
init_carousel (ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  carousel_state *ss;
  int wire = MI_IS_WIREFRAME(mi);
  
  if (sss == NULL) {
    if ((sss = (carousel_state *)
         calloc (MI_NUM_SCREENS(mi), sizeof(carousel_state))) == NULL)
      return;
  }
  ss = &sss[screen];

  if ((ss->glx_context = init_GL(mi)) != NULL) {
    reshape_carousel (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */
  } else {
    MI_CLEARWINDOW(mi);
  }

  if (!tilt_str || !*tilt_str)
    ;
  else if (!strcasecmp (tilt_str, "0"))
    ;
  else if (!strcasecmp (tilt_str, "X"))
    tilt_x_p = 1;
  else if (!strcasecmp (tilt_str, "Y"))
    tilt_y_p = 1;
  else if (!strcasecmp (tilt_str, "XY"))
    tilt_x_p = tilt_y_p = 1;
  else
    {
      fprintf (stderr, "%s: tilt must be 'X', 'Y', 'XY' or '', not '%s'\n",
               progname, tilt_str);
      exit (1);
    }

  {
    double spin_speed   = speed * 0.2;    /* rotation of tube around axis */
    double spin_accel   = speed * 0.1;
    double wander_speed = speed * 0.001;  /* tilting of axis */

    spin_speed   *= 0.9 + frand(0.2);
    wander_speed *= 0.9 + frand(0.2);

    ss->rot = make_rotator (spin_speed, spin_speed, spin_speed,
                            spin_accel, wander_speed, True);

    ss->trackball = gltrackball_init ();
  }

  if (strstr ((char *) glGetString(GL_EXTENSIONS),
              "GL_EXT_texture_filter_anisotropic"))
    glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &ss->anisotropic);
  else
    ss->anisotropic = 0.0;

  glDisable (GL_LIGHTING);
  glEnable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);

  if (! wire)
    {
      glShadeModel (GL_SMOOTH);
      glEnable (GL_LINE_SMOOTH);
      /* This gives us a transparent diagonal slice through each image! */
      /* glEnable (GL_POLYGON_SMOOTH); */
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable (GL_ALPHA_TEST);

      glEnable (GL_POLYGON_OFFSET_FILL);
      glPolygonOffset (1.0, 1.0);

    }

  ss->texfont = load_texture_font (MI_DISPLAY(mi), "font");

  if (debug_p)
    hack_resources (MI_DISPLAY (mi));

  ss->nframes = 0;
  ss->frames_size = 10;
  ss->frames = (image_frame **)
    calloc (1, ss->frames_size * sizeof(*ss->frames));

  ss->mode = IN;
  ss->mode_tick = fade_ticks / speed;

  ss->awaiting_first_images_p = True;
}


static void
draw_frame (ModeInfo *mi, image_frame *frame, time_t now, Bool body_p)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat texw  = frame->current.geom.width  / (GLfloat) frame->current.tw;
  GLfloat texh  = frame->current.geom.height / (GLfloat) frame->current.th;
  GLfloat texx1 = frame->current.geom.x / (GLfloat) frame->current.tw;
  GLfloat texy1 = frame->current.geom.y / (GLfloat) frame->current.th;
  GLfloat texx2 = texx1 + texw;
  GLfloat texy2 = texy1 + texh;
  GLfloat aspect = ((GLfloat) frame->current.geom.height /
                    (GLfloat) frame->current.geom.width);

  glBindTexture (GL_TEXTURE_2D, frame->current.texid);

  glPushMatrix();

  /* Position this image on the wheel.
   */
  glRotatef (frame->theta, 0, 1, 0);
  glTranslatef (0, 0, frame->r);

  /* Scale down the image so that all N frames fit on the wheel
     without bumping in to each other.
  */
  {
    GLfloat t, s;
    switch (ss->nframes)
      {
      case 1:  t = -1.0; s = 1.7; break;
      case 2:  t = -0.8; s = 1.6; break;
      case 3:  t = -0.4; s = 1.5; break;
      case 4:  t = -0.2; s = 1.3; break;
      default: t =  0.0; s = 6.0 / ss->nframes; break;
      }
    glTranslatef (0, 0, t);
    glScalef (s, s, s);
  }

  /* Center this image on the wheel plane.
   */
  glTranslatef (-0.5, -(aspect/2), 0);

  /* Move as per the "zoom in and out" setting.
   */
  if (zoom_p)
    {
      double x, y, z;
      /* Only use the Z component of the rotator for in/out position. */
      get_position (frame->rot, &x, &y, &z, !ss->button_down_p);
      glTranslatef (0, 0, z/2);
    }

  /* Compute the "drop in and out" state.
   */
  switch (frame->mode)
    {
    case EARLY:
      abort();
      break;
    case NORMAL:
      if (!ss->button_down_p &&
          now >= frame->expires &&
          ss->loads_in_progress == 0)  /* only load one at a time */
        load_image (mi, frame);
      break;
    case LOADING:
      break;
    case OUT:
      if (--frame->mode_tick <= 0) {
        image swap = frame->current;
        frame->current = frame->loading;
        frame->loading = swap;

        frame->mode = IN;
        frame->mode_tick = fade_ticks / speed;
      }
      break;
    case IN:
      if (--frame->mode_tick <= 0)
        frame->mode = NORMAL;
      break;
    default:
      abort();
    }

  /* Now translate for current in/out state.
   */
  if (frame->mode == OUT || frame->mode == IN)
    {
      GLfloat t = (frame->mode == OUT
                   ? frame->mode_tick / (fade_ticks / speed)
                   : (((fade_ticks / speed) - frame->mode_tick + 1) /
                      (fade_ticks / speed)));
      t = 5 * (1 - t);
      if (frame->from_top_p) t = -t;
      glTranslatef (0, t, 0);
    }

  if (body_p)					/* Draw the image quad. */
    {
      if (! wire)
        {
          glColor3f (1, 1, 1);
          glNormal3f (0, 0, 1);
          glEnable (GL_TEXTURE_2D);
          glBegin (GL_QUADS);
          glNormal3f (0, 0, 1);
          glTexCoord2f (texx1, texy2); glVertex3f (0, 0, 0);
          glTexCoord2f (texx2, texy2); glVertex3f (1, 0, 0);
          glTexCoord2f (texx2, texy1); glVertex3f (1, aspect, 0);
          glTexCoord2f (texx1, texy1); glVertex3f (0, aspect, 0);
          glEnd();
        }

      /* Draw a box around it.
       */
      glLineWidth (2.0);
      glColor3f (0.5, 0.5, 0.5);
      glDisable (GL_TEXTURE_2D);
      glBegin (GL_LINE_LOOP);
      glVertex3f (0, 0, 0);
      glVertex3f (1, 0, 0);
      glVertex3f (1, aspect, 0);
      glVertex3f (0, aspect, 0);
      glEnd();

    }
  else					/* Draw a title under the image. */
    {
      int sw, sh;
      GLfloat scale = 0.05;
      char *title = frame->current.title ? frame->current.title : "(untitled)";
      sw = texture_string_width (ss->texfont, title, &sh);

      glTranslatef (0, -scale, 0);

      scale /= sh;
      glScalef (scale, scale, scale);

      glTranslatef (((1/scale) - sw) / 2, 0, 0);
      glColor3f (1, 1, 1);

      if (!wire)
        {
          glEnable (GL_TEXTURE_2D);
          print_texture_string (ss->texfont, title);
        }
      else
        {
          glBegin (GL_LINE_LOOP);
          glVertex3f (0,  0,  0);
          glVertex3f (sw, 0,  0);
          glVertex3f (sw, sh, 0);
          glVertex3f (0,  sh, 0);
          glEnd();
        }
    }

  glPopMatrix();
}


ENTRYPOINT void
draw_carousel (ModeInfo *mi)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int i;

  if (!ss->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(ss->glx_context));

  if (ss->awaiting_first_images_p)
    if (!load_initial_images (mi))
      return;

  /* Only check the wall clock every 10 frames */
  {
    if (ss->now == 0 || ss->draw_tick++ > 10)
      {
        ss->now = time((time_t *) 0);
        if (ss->last_time == 0) ss->last_time = ss->now;
        ss->draw_tick = 0;
      }
  }


  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();


  /* Run the startup "un-shrink" animation.
   */
  switch (ss->mode)
    {
    case IN:
      if (--ss->mode_tick <= 0)
        {
          ss->mode = NORMAL;
          ss->last_time = time((time_t *) 0);
        }
      break;
    case NORMAL:
      break;
    default:
      abort();
    }


  /* Scale as per the startup "un-shrink" animation.
   */
  if (ss->mode != NORMAL)
    {
      GLfloat s = (ss->mode == OUT
                   ? ss->mode_tick / (fade_ticks / speed)
                   : (((fade_ticks / speed) - ss->mode_tick + 1) /
                      (fade_ticks / speed)));
      glScalef (s, s, s);
    }

  /* Rotate and tilt as per the user, and the motion modeller.
   */
  {
    double x, y, z;
    gltrackball_rotate (ss->trackball);

    /* Tilt the tube up or down by up to 30 degrees */
    get_position (ss->rot, &x, &y, &z, !ss->button_down_p);
    if (tilt_x_p)
      glRotatef (15 - (x * 30), 1, 0, 0);
    if (tilt_y_p)
      glRotatef (7  - (y * 14), 0, 0, 1);

    /* Only use the Y component of the rotator. */
    get_rotation (ss->rot, &x, &y, &z, !ss->button_down_p);
    glRotatef (y * 360, 0, 1, 0);
  }

  /* First draw each image, then draw the titles.  GL insists that you
     draw back-to-front in order to make alpha blending work properly,
     so we need to draw all of the 100% opaque images before drawing
     any of the not-100%-opaque titles.
   */
  for (i = 0; i < ss->nframes; i++)
    draw_frame (mi, ss->frames[i], ss->now, True);
  if (titles_p)
    for (i = 0; i < ss->nframes; i++)
      draw_frame (mi, ss->frames[i], ss->now, False);

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}

XSCREENSAVER_MODULE ("Carousel", carousel)

#endif /* USE_GL */
