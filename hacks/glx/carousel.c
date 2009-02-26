/* carousel, Copyright (c) 2005 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Intrinsic.h>

# define PROGCLASS "Carousel"
# define HACK_INIT init_carousel
# define HACK_DRAW draw_carousel
# define HACK_RESHAPE reshape_carousel
# define HACK_HANDLE_EVENT carousel_handle_event
# define EVENT_MASK        PointerMotionMask
# define carousel_opts xlockmore_opts

# define DEF_SPEED          "1.0"
# define DEF_DURATION	    "20"
# define DEF_TITLES         "True"
# define DEF_ZOOM           "True"
# define DEF_TILT           "XY"
# define DEF_MIPMAP         "True"
# define DEF_DEBUG          "False"

#define DEF_FONT "-*-times-bold-r-normal-*-240-*"

#define DEFAULTS  "*count:           7         \n" \
		  "*delay:           10000     \n" \
		  "*wireframe:       False     \n" \
                  "*showFPS:         False     \n" \
	          "*fpsSolid:        True      \n" \
	          "*useSHM:          True      \n" \
		  "*font:	   " DEF_FONT "\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n"

# include "xlockmore.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef USE_GL

#include <GL/glu.h>
#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "rotator.h"
#include "gltrackball.h"
#include "grab-ximage.h"
#include "texfont.h"

extern XtAppContext app;

typedef struct {
  double x, y, w, h;
} rect;

typedef enum { NORMAL, OUT, LOADING, IN, DEAD } fade_mode;
static int fade_ticks = 60;

typedef struct {
  ModeInfo *mi;

  char *title;			/* the filename of this image */
  int w, h;			/* size in pixels of the image */
  int tw, th;			/* size in pixels of the texture */
  XRectangle geom;		/* where in the image the bits are */
  GLuint texid;			/* which texture contains the image */

  GLfloat r, theta;		/* radius and rotation on the tube */
  rotator *rot;			/* for zoomery */
  Bool from_top_p;		/* whether this image drops in or rises up */
  time_t expires;		/* when this image should be replaced */
  fade_mode mode;		/* in/out animation state */
  int mode_tick;
  Bool loaded_p;		/* whether background load is done */

} image;


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  time_t button_down_time;

  int nimages;			/* how many images are loaded */
  int images_size;
  image **images;		/* pointers to the images */

  texture_font_data *texfont;

  fade_mode mode;
  int mode_tick;

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

ModeSpecOpt carousel_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Allocates an image structure and stores it in the list.
 */
static image *
alloc_image (ModeInfo *mi)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  image *img = (image *) calloc (1, sizeof (*img));

  img->mi = mi;
  img->rot = make_rotator (0, 0, 0, 0, 0.04 * frand(1.0) * speed, False);

  glGenTextures (1, &img->texid);
  if (img->texid <= 0) abort();

  if (ss->images_size <= ss->nimages)
    {
      ss->images_size = (ss->images_size * 1.2) + ss->nimages;
      ss->images = (image **)
        realloc (ss->images, ss->images_size * sizeof(*ss->images));
      if (! ss->images)
        {
          fprintf (stderr, "%s: out of memory (%d images)\n",
                   progname, ss->images_size);
          exit (1);
        }
    }

  ss->images[ss->nimages++] = img;

  return img;
}


static void image_loaded_cb (const char *filename, XRectangle *geom,
                             int image_width, int image_height,
                             int texture_width, int texture_height,
                             void *closure);


/* Load a new file into the given image struct.
 */
static void
load_image (ModeInfo *mi, image *img, Bool force_sync_p)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Bool async_p = !force_sync_p;
  int i;

  if (debug_p && !wire && img->w != 0)
    fprintf (stderr, "%s:  dropped %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname, img->geom.width, img->geom.height, img->tw, img->th,
             (img->title ? img->title : "(null)"));

  if (img->title)
    {
      free (img->title);
      img->title = 0;
    }

  img->mode = LOADING;

  if (wire)
    image_loaded_cb (0, 0, 0, 0, 0, 0, img);
  else if (async_p)
    screen_to_texture_async (mi->xgwa.screen, mi->window,
                             MI_WIDTH(mi)/2-1,
                             MI_HEIGHT(mi)/2-1,
                             mipmap_p, img->texid, image_loaded_cb, img);
  else
    {
      char *filename = 0;
      XRectangle geom;
      int iw=0, ih=0, tw=0, th=0;
      time_t start = time((time_t *) 0);
      time_t end;

      glBindTexture (GL_TEXTURE_2D, img->texid);
      if (! screen_to_texture (mi->xgwa.screen, mi->window,
                               MI_WIDTH(mi)/2-1,
                               MI_HEIGHT(mi)/2-1,
                               mipmap_p, &filename, &geom, &iw, &ih, &tw, &th))
        exit(1);
      image_loaded_cb (filename, &geom, iw, ih, tw, th, img);
      if (filename) free (filename);

      /* Push the expire times of all images forward by the amount of time
         it took to load *this* image, so that we don't count image-loading
         time against image duration.
      */
      end = time((time_t *) 0);
      for (i = 0; i < ss->nimages; i++)
        ss->images[i]->expires += end - start;
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

  if (img->title)   /* strip filename to part after last /. */
    {
      char *s = strrchr (img->title, '/');
      if (s) strcpy (img->title, s+1);
    }

  if (debug_p)
    fprintf (stderr, "%s:   loaded %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname,
             img->geom.width, img->geom.height, img->tw, img->th,
             (img->title ? img->title : "(null)"));

 DONE:

  /* This image expires N seconds after it finished loading. */
  img->expires = time((time_t *) 0) + (duration * MI_COUNT(mi));

  img->mode = IN;
  img->mode_tick = fade_ticks * speed;
  img->from_top_p = random() & 1;
  img->loaded_p = True;
}




/* Free the image and texture, after nobody is referencing it.
 */
#if 0
static void
destroy_image (ModeInfo *mi, image *img)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  Bool freed_p = False;
  int i;

  if (!img) abort();
  if (img->texid <= 0) abort();

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
    fprintf (stderr, "%s: unloaded %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname,
             img->geom.width, img->geom.height, img->tw, img->th,
             (img->title ? img->title : "(null)"));

  if (img->title) free (img->title);
  glDeleteTextures (1, &img->texid);
  free (img);
}
#endif


static void loading_msg (ModeInfo *mi, int n);

static void
load_initial_images (ModeInfo *mi)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int i;
  time_t now;
  Bool async_p = True;   /* computed after first image */

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      image *img;
      loading_msg (mi, (async_p ? 0 : i));
      img = alloc_image (mi);
      img->r = 1.0;
      img->theta = i * 360.0 / MI_COUNT(mi);

      load_image (mi, img, True);
      if (i == 0)
        async_p = !img->loaded_p;
    }

  /* Wait for the images to load...
   */
  while (1)
    {
      int j;
      int count = 0;

      for (j = 0; j < MI_COUNT(mi); j++)
        if (ss->images[j]->loaded_p)
          count++;
      if (count == MI_COUNT(mi))
        break;
      loading_msg (mi, count);

      usleep (100000);		/* check every 1/10th sec */
      if (i++ > 600) abort();	/* if a minute has passed, we're broken */

      while (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
        XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);
    }

  /* Stagger expire times of the first batch of images.
   */
  now = time((time_t *) 0);
  for (i = 0; i < ss->nimages; i++)
    {
      image *img = ss->images[i];
      img->expires = now + (duration * (i + 1));
      img->mode = NORMAL;
    }

  /* Instead of always going clockwise, shuffle the expire times
     of the images so that they drop out in a random order.
  */
  for (i = 0; i < ss->nimages; i++)
    {
      image *img1 = ss->images[i];
      image *img2 = ss->images[random() % ss->nimages];
      time_t swap = img1->expires;
      img1->expires = img2->expires;
      img2->expires = swap;
    }
}


void
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


Bool
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
             images, so that mouse-dragging doesn't count against
             image expiration.
           */
          int secs = time((time_t *) 0) - ss->button_down_time;
          int i;
          for (i = 0; i < ss->nimages; i++)
            ss->images[i]->expires += secs;
        }
      ss->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
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
#endif
}


static void
loading_msg (ModeInfo *mi, int n)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  char text[100];
  static int sw=0, sh=0;
  GLfloat scale;

  if (wire) return;

  if (n == 0)
    sprintf (text, "Loading images...");
  else
    sprintf (text, "Loading images...  (%d%%)",
             (int) (n * 100 / MI_COUNT(mi)));

  if (sw == 0)    /* only do this once, so that the string doesn't move. */
    sw = texture_string_width (ss->texfont, text, &sh);

  scale = sh / (GLfloat) MI_HEIGHT(mi);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi));

  glTranslatef ((MI_WIDTH(mi)  - sw) / 2,
                (MI_HEIGHT(mi) - sh) / 2,
                0);
  glColor3f (1, 1, 0);
  glEnable (GL_TEXTURE_2D);
  print_texture_string (ss->texfont, text);
  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);

  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}


void
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
  } else {
    MI_CLEARWINDOW(mi);
  }

  if (!tilt_str || !*tilt_str)
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
    hack_resources();

  ss->nimages = 0;
  ss->images_size = 10;
  ss->images = (image **) calloc (1, ss->images_size * sizeof(*ss->images));

  ss->mode = IN;
  ss->mode_tick = fade_ticks * speed;

  load_initial_images (mi);
}


static void
draw_img (ModeInfo *mi, image *img, time_t now, Bool body_p)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat texw  = img->geom.width  / (GLfloat) img->tw;
  GLfloat texh  = img->geom.height / (GLfloat) img->th;
  GLfloat texx1 = img->geom.x / (GLfloat) img->tw;
  GLfloat texy1 = img->geom.y / (GLfloat) img->th;
  GLfloat texx2 = texx1 + texw;
  GLfloat texy2 = texy1 + texh;
  GLfloat aspect = img->geom.height / (GLfloat) img->geom.width;

  glBindTexture (GL_TEXTURE_2D, img->texid);

  glPushMatrix();

  /* Position this image on the wheel.
   */
  glRotatef (img->theta, 0, 1, 0);
  glTranslatef (0, 0, img->r);

  /* Scale down the image so that all N images fit on the wheel
     without bumping in to each other.
  */
  {
    GLfloat t, s;
    switch (ss->nimages)
      {
      case 1:  t = -1.0; s = 1.7; break;
      case 2:  t = -0.8; s = 1.6; break;
      case 3:  t = -0.4; s = 1.5; break;
      case 4:  t = -0.2; s = 1.3; break;
      default: t =  0.0; s = 6.0 / ss->nimages; break;
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
      get_position (img->rot, &x, &y, &z, !ss->button_down_p);
      glTranslatef (0, 0, z/2);
    }

  /* Compute the "drop in and out" state.
   */
  switch (img->mode)
    {
    case NORMAL:
      if (!ss->button_down_p &&
          now >= img->expires)
        {
          img->mode = OUT;
          img->mode_tick = fade_ticks * speed;
        }
      break;
    case OUT:
      if (--img->mode_tick <= 0)
        load_image (mi, img, False);
      break;
    case LOADING:
      /* just wait, with the image off screen. */
      break;
    case IN:
      if (--img->mode_tick <= 0)
        img->mode = NORMAL;
      break;
    default:
      abort();
    }

  /* Now drop in/out.
   */
  if (img->mode != NORMAL)
    {
      GLfloat t = (img->mode == LOADING
                   ? -100
                   : img->mode == OUT
                   ? img->mode_tick / (fade_ticks * speed)
                   : (((fade_ticks * speed) - img->mode_tick + 1) /
                      (fade_ticks * speed)));
      t = 5 * (1 - t);
      if (img->from_top_p) t = -t;
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
      char *title = img->title ? img->title : "(untitled)";
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


void
draw_carousel (ModeInfo *mi)
{
  carousel_state *ss = &sss[MI_SCREEN(mi)];
  static time_t last_time = 0;
  static time_t now = 0;
  int i;

  if (!ss->glx_context)
    return;

  /* Only check the wall clock every 10 frames */
  {
    static int tick = 0;
    if (now == 0 || tick++ > 10)
      {
        now = time((time_t *) 0);
        if (last_time == 0) last_time = now;
        tick = 0;
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
          last_time = time((time_t *) 0);
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
                   ? ss->mode_tick / (fade_ticks * speed)
                   : (((fade_ticks * speed) - ss->mode_tick + 1) /
                      (fade_ticks * speed)));
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

  /* First draw each image, then draw the titles.  Insists that you
     draw back-to-front in order to make alpha blending work properly,
     so we need to draw all of the 100% opaque images before drawing
     any of the not-100%-opaque titles.
   */
  for (i = 0; i < ss->nimages; i++)
    draw_img (mi, ss->images[i], now, True);
  if (titles_p)
    for (i = 0; i < ss->nimages; i++)
      draw_img (mi, ss->images[i], now, False);

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}

#endif /* USE_GL */
