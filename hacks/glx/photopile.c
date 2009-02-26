/* photopile, Copyright (c) 2008 Jens Kilian <jjk@acm.org>
 * Based on carousel, Copyright (c) 2005-2008 Jamie Zawinski <jwz@jwz.org>
 * Loads a sequence of images and shuffles them into a pile.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#define DEF_FONT "-*-helvetica-bold-r-normal-*-240-*"
#define DEFAULTS  "*count:           7         \n" \
                  "*delay:           10000     \n" \
                  "*wireframe:       False     \n" \
                  "*showFPS:         False     \n" \
                  "*fpsSolid:        True      \n" \
                  "*useSHM:          True      \n" \
                  "*font:          " DEF_FONT "\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
                  "*grabDesktopImages:   False \n" \
                  "*chooseRandomImages:  True  \n"

# define refresh_photopile 0
# define release_photopile 0
# define photopile_handle_event 0

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifndef HAVE_COCOA
# include <X11/Intrinsic.h>     /* for XrmDatabase in -debug mode */
#endif
#include <math.h>

#include "xlockmore.h"
#include "grab-ximage.h"
#include "texfont.h"

#ifdef USE_GL

# define DEF_SCALE          "0.4"
# define DEF_MAX_TILT       "50"
# define DEF_SPEED          "1.0"
# define DEF_DURATION       "5"
# define DEF_TITLES         "False"
# define DEF_MIPMAP         "True"
# define DEF_DEBUG          "False"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef struct {
  GLfloat x, y;                 /* position on screen */
  GLfloat angle;                /* rotation angle */

} position;

typedef struct {
  Bool loaded_p;                /* true if image can be drawn */

  char *title;                  /* the filename of this image */
  int w, h;                     /* size in pixels of the image */
  int tw, th;                   /* size in pixels of the texture */
  XRectangle geom;              /* where in the image the bits are */

  position pos[4];              /* control points for calculating position */

  GLuint texid;                 /* GL texture ID */

} image;


typedef enum { EARLY, IN, NORMAL, LOADING, SHUFFLE } fade_mode;
static int fade_ticks = 60;

typedef struct {
  ModeInfo *mi;
  GLXContext *glx_context;

  image *frames;                /* pointer to array of images */
  int nframe;                   /* image being (resp. next to be) loaded */

  texture_font_data *texfont;
  int loading_sw, loading_sh;

  time_t last_time, now;
  int draw_tick;
  fade_mode mode;
  int mode_tick;

} photopile_state;

static photopile_state *sss = NULL;


/* Command-line arguments
 */
static GLfloat scale;        /* Scale factor for loading images. */
static GLfloat max_tilt;     /* Maximum angle from vertical. */
static GLfloat speed;        /* Animation speed scale factor. */
static int duration;         /* Reload images after this long. */
static Bool mipmap_p;        /* Use mipmaps instead of single textures. */
static Bool titles_p;        /* Display image titles. */
static Bool debug_p;         /* Be loud and do weird things. */


static XrmOptionDescRec opts[] = {
  {"-scale",        ".scale",         XrmoptionSepArg, 0 },
  {"-maxTilt",      ".maxTilt",       XrmoptionSepArg, 0 },
  {"-titles",       ".titles",        XrmoptionNoArg, "True"  },
  {"-no-titles",    ".titles",        XrmoptionNoArg, "False" },
  {"-mipmaps",      ".mipmap",        XrmoptionNoArg, "True"  },
  {"-no-mipmaps",   ".mipmap",        XrmoptionNoArg, "False" },
  {"-duration",     ".duration",      XrmoptionSepArg, 0 },
  {"-debug",        ".debug",         XrmoptionNoArg, "True"  },
  {"-font",         ".font",          XrmoptionSepArg, 0 },
  {"-speed",        ".speed",         XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  { &scale,         "scale",        "Scale",        DEF_SCALE,       t_Float},
  { &max_tilt,      "maxTilt",      "MaxTilt",      DEF_MAX_TILT,    t_Float},
  { &mipmap_p,      "mipmap",       "Mipmap",       DEF_MIPMAP,      t_Bool},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,       t_Bool},
  { &titles_p,      "titles",       "Titles",       DEF_TITLES,      t_Bool},
  { &speed,         "speed",        "Speed",        DEF_SPEED,       t_Float},
  { &duration,      "duration",     "Duration",     DEF_DURATION,    t_Int},
};

ENTRYPOINT ModeSpecOpt photopile_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Functions to interpolate between image positions.
 */
static position
add_pos(position p, position q)
{
  p.x += q.x;
  p.y += q.y;
  p.angle += q.angle;
  return p;
}

static position
scale_pos(GLfloat t, position p)
{
  p.x *= t;
  p.y *= t;
  p.angle *= t;
  return p;
}

static position
linear_combination(GLfloat t, position p, position q)
{
  return add_pos(scale_pos(1.0 - t, p), scale_pos(t, q));
}

static position
interpolate(GLfloat t, position p[4])
{
  /* de Casteljau's algorithm, 4 control points */
  position p10 = linear_combination(t, p[0], p[1]);
  position p11 = linear_combination(t, p[1], p[2]);
  position p12 = linear_combination(t, p[2], p[3]);

  position p20 = linear_combination(t, p10, p11);
  position p21 = linear_combination(t, p11, p12);

  return linear_combination(t, p20, p21);
}

static position
offset_pos(position p, GLfloat th, GLfloat r)
{
  p.x += cos(th) * r;
  p.y += sin(th) * r;
  p.angle = (frand(2.0) - 1.0) * max_tilt;
  return p;
}

/* Calculate new positions for all images.
 */
static void
set_new_positions(photopile_state *ss)
{
  ModeInfo *mi = ss->mi;
  int i;

  for (i = 0; i < MI_COUNT(mi)+1; ++i)
    {
      image *frame = ss->frames + i;
      GLfloat d = sqrt(frame->w*frame->w + frame->h*frame->h);
      GLfloat leave = frand(M_PI * 2.0);
      GLfloat enter = frand(M_PI * 2.0);

      /* start position */
      frame->pos[0] = frame->pos[3];

      /* end position */
      frame->pos[3].x = BELLRAND(MI_WIDTH(mi));
      frame->pos[3].y = BELLRAND(MI_HEIGHT(mi));
      frame->pos[3].angle = (frand(2.0) - 1.0) * max_tilt;

      /* intermediate points */
      frame->pos[1] = offset_pos(frame->pos[0], leave, d * (0.5 + frand(1.0)));
      frame->pos[2] = offset_pos(frame->pos[3], enter, d * (0.5 + frand(1.0)));
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
  photopile_state *ss = (photopile_state *) closure;
  ModeInfo *mi = ss->mi;
  int wire = MI_IS_WIREFRAME(mi);
  image *frame = ss->frames + ss->nframe;

  if (wire)
    {
      frame->w = (int)(MI_WIDTH(mi)  * scale) - 1;
      frame->h = (int)(MI_HEIGHT(mi) * scale) - 1;
      if (frame->w <= 10) frame->w = 10;
      if (frame->h <= 10) frame->h = 10;
      frame->geom.width  = frame->w;
      frame->geom.height = frame->h;
      goto DONE;
    }

  if (image_width == 0 || image_height == 0)
    exit (1);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  frame->w  = image_width;
  frame->h  = image_height;
  frame->tw = texture_width;
  frame->th = texture_height;
  frame->geom = *geom;

  if (frame->title)
    free (frame->title);
  frame->title = (filename ? strdup (filename) : 0);

  if (frame->title)   /* strip filename to part after last /. */
    {
      char *s = strrchr (frame->title, '/');
      if (s) strcpy (frame->title, s+1);
    }

  if (debug_p)
    fprintf (stderr, "%s:   loaded %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname,
             frame->geom.width, 
             frame->geom.height, 
             frame->tw, frame->th,
             (frame->title ? frame->title : "(null)"));

 DONE:
  frame->loaded_p = True;
}


/* Load a new file.
 */
static void
load_image (ModeInfo *mi)
{
  photopile_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  image *frame = ss->frames + ss->nframe;

  if (debug_p && !wire && frame->w != 0)
    fprintf (stderr, "%s:  dropped %4d x %-4d  %4d x %-4d  \"%s\"\n",
             progname, 
             frame->geom.width, 
             frame->geom.height, 
             frame->tw, frame->th,
             (frame->title ? frame->title : "(null)"));

  frame->loaded_p = False;

  if (wire)
    image_loaded_cb (0, 0, 0, 0, 0, 0, ss);
  else
    {
      int w = (int)(MI_WIDTH(mi)  * scale) - 1;
      int h = (int)(MI_HEIGHT(mi) * scale) - 1;
      if (w <= 10) w = 10;
      if (h <= 10) h = 10;
      load_texture_async (mi->xgwa.screen, mi->window, *ss->glx_context, w, h,
                          mipmap_p, frame->texid, 
                          image_loaded_cb, ss);
    }
}


static void
loading_msg (ModeInfo *mi, int n)
{
  photopile_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  char text[100];
  GLfloat scale;

  if (wire) return;

  if (n == 0)
    sprintf (text, "Loading images...");
  else
    sprintf (text, "Loading images...  (%d%%)",
             (int) (n * 100 / MI_COUNT(mi)));

  if (ss->loading_sw == 0)    /* only do this once, so that the string doesn't move. */
    ss->loading_sw = texture_string_width (ss->texfont, text, &ss->loading_sh);

  scale = ss->loading_sh / (GLfloat) MI_HEIGHT(mi);

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


static Bool
load_initial_images (ModeInfo *mi)
{
  photopile_state *ss = &sss[MI_SCREEN(mi)];

  if (ss->frames[ss->nframe].loaded_p)
    {
      /* The current image has been fully loaded. */
      if (++ss->nframe < MI_COUNT(mi))
        {
          /* Start the next one loading.  (We run the image loader
           * asynchronously, but we load them one at a time.)
           */
          load_image(mi);
        }
      else
        {
          /* loaded all initial images, start fading them in */
          int i;

          for (i = 0; i < ss->nframe; ++i)
            {
              ss->frames[i].pos[3].x = MI_WIDTH(mi) * 0.5;
              ss->frames[i].pos[3].y = MI_HEIGHT(mi) * 0.5;
              ss->frames[i].pos[3].angle = 0.0;
            }
          set_new_positions(ss);

          ss->mode = IN;
          ss->mode_tick = fade_ticks / speed;
        }
    }

  loading_msg(mi, ss->nframe);
  return (ss->mode != EARLY);
}


ENTRYPOINT void
reshape_photopile (ModeInfo *mi, int width, int height)
{
  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluOrtho2D(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi));

  glClear(GL_COLOR_BUFFER_BIT);
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


ENTRYPOINT void
init_photopile (ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  photopile_state *ss;
  int wire = MI_IS_WIREFRAME(mi);

  if (sss == NULL) {
    if ((sss = (photopile_state *)
         calloc (MI_NUM_SCREENS(mi), sizeof(photopile_state))) == NULL)
      return;
  }
  ss = &sss[screen];
  ss->mi = mi;

  if ((ss->glx_context = init_GL(mi)) != NULL) {
    reshape_photopile (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
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

      glEnable (GL_POLYGON_OFFSET_FILL);
      glPolygonOffset (1.0, 1.0);
    }

  ss->texfont = load_texture_font (MI_DISPLAY(mi), "font");

  if (debug_p)
    hack_resources (MI_DISPLAY (mi));

  ss->frames = (image *)calloc (MI_COUNT(mi) + 1, sizeof(image));
  ss->nframe = 0;
  if (!wire)
    {
      int i;
      for (i = 0; i < MI_COUNT(mi) + 1; ++i)
        {
          glGenTextures(1, &(ss->frames[i].texid));
          if (ss->frames[i].texid <= 0) abort();
        }
    }

  ss->mode = EARLY;
  load_image(mi); /* start loading the first image */
}


static void
draw_image (ModeInfo *mi, int i, GLfloat t, GLfloat s, GLfloat z)
{
  int wire = MI_IS_WIREFRAME(mi);
  photopile_state *ss = &sss[MI_SCREEN(mi)];
  image *frame = ss->frames + i;

  GLfloat texw  = frame->geom.width  / (GLfloat) frame->tw;
  GLfloat texh  = frame->geom.height / (GLfloat) frame->th;
  GLfloat texx1 = frame->geom.x / (GLfloat) frame->tw;
  GLfloat texy1 = frame->geom.y / (GLfloat) frame->th;
  GLfloat texx2 = texx1 + texw;
  GLfloat texy2 = texy1 + texh;

  position pos = interpolate(t, frame->pos);
  GLfloat w = frame->geom.width * 0.5;
  GLfloat h = frame->geom.height * 0.5;

  glBindTexture (GL_TEXTURE_2D, frame->texid);

  glPushMatrix();

  /* Position and scale this image.
   */
  glTranslatef (pos.x, pos.y, 0);
  glRotatef (pos.angle, 0, 0, 1);
  glScalef (s, s, 1);

  /* Draw the image quad. */
  if (! wire)
    {
      glColor3f (1, 1, 1);
      glEnable (GL_TEXTURE_2D);
      glBegin (GL_QUADS);
      glTexCoord2f (texx1, texy2); glVertex3f (-w, -h, z);
      glTexCoord2f (texx2, texy2); glVertex3f ( w, -h, z);
      glTexCoord2f (texx2, texy1); glVertex3f ( w,  h, z);
      glTexCoord2f (texx1, texy1); glVertex3f (-w,  h, z);
      glEnd();
    }

  /* Draw a box around it.
   */
  glLineWidth (2.0);
  glColor3f (0.5, 0.5, 0.5);
  glDisable (GL_TEXTURE_2D);
  glBegin (GL_LINE_LOOP);
  glVertex3f (-w, -h, z);
  glVertex3f ( w, -h, z);
  glVertex3f ( w,  h, z);
  glVertex3f (-w,  h, z);
  glEnd();

  /* Draw a title under the image.
   */
  if (titles_p)
    {
      int sw, sh;
      GLfloat scale = 0.5;
      char *title = frame->title ? frame->title : "(untitled)";
      sw = texture_string_width (ss->texfont, title, &sh);

      glTranslatef (-sw*scale*0.5, -h - sh*scale, z);
      glScalef (scale, scale, 1);

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
draw_photopile (ModeInfo *mi)
{
  photopile_state *ss = &sss[MI_SCREEN(mi)];
  int i;

  if (!ss->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(ss->glx_context));

  if (ss->mode == EARLY)
    if (!load_initial_images (mi))
      return;

  /* Only check the wall clock every 10 frames */
  if (ss->now == 0 || ss->draw_tick++ > 10)
    {
      ss->now = time((time_t *) 0);
      if (ss->last_time == 0) ss->last_time = ss->now;
      ss->draw_tick = 0;
    }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  {
    GLfloat t;

    /* Handle state transitions. */
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
        if (ss->now - ss->last_time > duration)
          {
            ss->mode = LOADING;
            load_image(mi);
          }
        break;
      case LOADING:
        if (ss->frames[ss->nframe].loaded_p)
          {
            set_new_positions(ss);
            ss->mode = SHUFFLE;
            ss->mode_tick = fade_ticks / speed;
          }
        break;
      case SHUFFLE:
        if (--ss->mode_tick <= 0)
          {
            ss->nframe = (ss->nframe+1) % (MI_COUNT(mi)+1);

            ss->mode = NORMAL;
            ss->last_time = time((time_t *) 0);
          }
        break;
      default:
        abort();
      }

    t = 1.0 - ss->mode_tick / (fade_ticks / speed);
    t = 0.5 * (1.0 - cos(M_PI * t));

    /* Draw the images. */
    for (i = 0; i < MI_COUNT(mi) + (ss->mode == SHUFFLE); ++i)
      {
        int j = (ss->nframe + i + 1) % (MI_COUNT(mi) + 1);
        GLfloat s = 1.0;
        GLfloat z = (GLfloat)i / (MI_COUNT(mi) + 1);

        switch (ss->mode)
          {
          case IN:
            s *= t;
            break;
          case NORMAL:
          case LOADING:
            t = 1.0;
            break;
          case SHUFFLE:
            if (i == MI_COUNT(mi))
              {
                s *= t;
              }
            else if (i == 0)
              {
                s *= 1.0 - t;
              }
            break;
          default:
            abort();
          }

        draw_image(mi, j, t, s, z);
      }
  }

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}

XSCREENSAVER_MODULE ("Photopile", photopile)

#endif /* USE_GL */
