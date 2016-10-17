/* photopile, Copyright (c) 2008-2015 Jens Kilian <jjk@acm.org>
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

#define DEF_FONT "-*-helvetica-bold-r-normal-*-*-480-*-*-*-*-*-*"
#define DEFAULTS  "*count:           7         \n" \
                  "*delay:           10000     \n" \
                  "*wireframe:       False     \n" \
                  "*showFPS:         False     \n" \
                  "*fpsSolid:        True      \n" \
                  "*useSHM:          True      \n" \
                  "*font:          " DEF_FONT "\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
                  "*grabDesktopImages:   False \n" \
                  "*chooseRandomImages:  True  \n" \
		  "*suppressRotationAnimation: True\n" \

# define refresh_photopile 0
# define release_photopile 0
# define photopile_handle_event 0

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifndef HAVE_JWXYZ
# include <X11/Intrinsic.h>     /* for XrmDatabase in -debug mode */
#endif
#include <math.h>

#include "xlockmore.h"
#include "grab-ximage.h"
#include "texfont.h"
#include "dropshadow.h"

#ifdef USE_GL

# define DEF_SCALE          "0.4"
# define DEF_MAX_TILT       "50"
# define DEF_SPEED          "1.0"
# define DEF_DURATION       "5"
# define DEF_MIPMAP         "True"
# define DEF_TITLES         "False"
# define DEF_POLAROID       "True"
# define DEF_CLIP           "True"
# define DEF_SHADOWS        "True"
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


typedef enum { EARLY, SHUFFLE, NORMAL, LOADING } fade_mode;
static int fade_ticks = 60;

typedef struct {
  ModeInfo *mi;
  GLXContext *glx_context;

  image *frames;                /* pointer to array of images */
  int nframe;                   /* image being (resp. next to be) loaded */

  GLuint shadow;
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
static Bool polaroid_p;      /* Use instant-film look for images. */
static Bool clip_p;          /* Clip images instead of scaling for -polaroid. */
static Bool shadows_p;       /* Draw drop shadows. */
static Bool debug_p;         /* Be loud and do weird things. */


static XrmOptionDescRec opts[] = {
  {"-scale",        ".scale",         XrmoptionSepArg, 0 },
  {"-maxTilt",      ".maxTilt",       XrmoptionSepArg, 0 },
  {"-speed",        ".speed",         XrmoptionSepArg, 0 },
  {"-duration",     ".duration",      XrmoptionSepArg, 0 },
  {"-mipmaps",      ".mipmap",        XrmoptionNoArg, "True"  },
  {"-no-mipmaps",   ".mipmap",        XrmoptionNoArg, "False" },
  {"-titles",       ".titles",        XrmoptionNoArg, "True"  },
  {"-no-titles",    ".titles",        XrmoptionNoArg, "False" },
  {"-polaroid",     ".polaroid",      XrmoptionNoArg, "True"  },
  {"-no-polaroid",  ".polaroid",      XrmoptionNoArg, "False" },
  {"-clip",         ".clip",          XrmoptionNoArg, "True"  },
  {"-no-clip",      ".clip",          XrmoptionNoArg, "False" },
  {"-shadows",      ".shadows",       XrmoptionNoArg, "True"  },
  {"-no-shadows",   ".shadows",       XrmoptionNoArg, "False" },
  {"-debug",        ".debug",         XrmoptionNoArg, "True"  },
  {"-font",         ".font",          XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  { &scale,         "scale",        "Scale",        DEF_SCALE,       t_Float},
  { &max_tilt,      "maxTilt",      "MaxTilt",      DEF_MAX_TILT,    t_Float},
  { &speed,         "speed",        "Speed",        DEF_SPEED,       t_Float},
  { &duration,      "duration",     "Duration",     DEF_DURATION,    t_Int},
  { &mipmap_p,      "mipmap",       "Mipmap",       DEF_MIPMAP,      t_Bool},
  { &titles_p,      "titles",       "Titles",       DEF_TITLES,      t_Bool},
  { &polaroid_p,    "polaroid",     "Polaroid",     DEF_POLAROID,    t_Bool},
  { &clip_p,        "clip",         "Clip",         DEF_CLIP,        t_Bool},
  { &shadows_p,     "shadows",      "Shadows",      DEF_SHADOWS,     t_Bool},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,       t_Bool},
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
      GLfloat w = frame->w;
      GLfloat h = frame->h;
      GLfloat d = sqrt(w*w + h*h);
      GLfloat leave = frand(M_PI * 2.0);
      GLfloat enter = frand(M_PI * 2.0);

      /* start position */
      frame->pos[0] = frame->pos[3];

      /* end position */
      frame->pos[3].x = BELLRAND(MI_WIDTH(mi));
      frame->pos[3].y = BELLRAND(MI_HEIGHT(mi));
      frame->pos[3].angle = (frand(2.0) - 1.0) * max_tilt;

      /* Try to keep the images mostly inside the screen bounds */
      frame->pos[3].x = MAX(0.5*w, MIN(MI_WIDTH(mi)-0.5*w, frame->pos[3].x));
      frame->pos[3].y = MAX(0.5*h, MIN(MI_HEIGHT(mi)-0.5*h, frame->pos[3].y));

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
      if (random() % 2)
        {
          frame->w = (int)(MI_WIDTH(mi)  * scale) - 1;
          frame->h = (int)(MI_HEIGHT(mi) * scale) - 1;
        }
      else
        {
          frame->w = (int)(MI_HEIGHT(mi) * scale) - 1;
          frame->h = (int)(MI_WIDTH(mi)  * scale) - 1;
        }
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

  /* xscreensaver-getimage returns paths relative to the image directory
     now, so leave the sub-directory part in.  Unless it's an absolute path.
  */
  if (frame->title && frame->title[0] == '/')
    {
      /* strip filename to part after last /. */
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
      int w = MI_WIDTH(mi);
      int h = MI_HEIGHT(mi);
      int size = (int)((w > h ? w : h) * scale);
      if (size <= 10) size = 10;
      load_texture_async (mi->xgwa.screen, mi->window, *ss->glx_context,
                          size, size,
                          mipmap_p, frame->texid, 
                          image_loaded_cb, ss);
    }
}


static void
loading_msg (ModeInfo *mi)
{
  photopile_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  const char text[] = "Loading...";

  if (wire) return;

  if (ss->loading_sw == 0)    /* only do this once */
    {
      XCharStruct e;
      texture_string_metrics (ss->texfont, text, &e, 0, 0);
      ss->loading_sw = e.width;
      ss->loading_sh = e.ascent + e.descent;
    }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi), -1, 1);

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
loading_initial_image (ModeInfo *mi)
{
  photopile_state *ss = &sss[MI_SCREEN(mi)];

  if (ss->frames[ss->nframe].loaded_p)
    {
      /* The initial image has been fully loaded, start fading it in. */
      int i;

      for (i = 0; i < ss->nframe; ++i)
        {
          ss->frames[i].pos[3].x = MI_WIDTH(mi) * 0.5;
          ss->frames[i].pos[3].y = MI_HEIGHT(mi) * 0.5;
          ss->frames[i].pos[3].angle = 0.0;
        }
      set_new_positions(ss);

      ss->mode = SHUFFLE;
      ss->mode_tick = fade_ticks / speed;
    }
  else
    {
      loading_msg(mi);
    }

  return (ss->mode == EARLY);
}


ENTRYPOINT void
reshape_photopile (ModeInfo *mi, int width, int height)
{
  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glOrtho(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi), -1, 1);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, h, 1);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


/* Kludge to add "-v" to invocation of "xscreensaver-getimage" in -debug mode
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
# endif /* !HAVE_JWXYZ */
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
    clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */
  } else {
    MI_CLEARWINDOW(mi);
  }

  ss->shadow = init_drop_shadow();
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
          glGenTextures (1, &(ss->frames[i].texid));
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

  position pos = interpolate(t, frame->pos);
  GLfloat w = frame->geom.width * 0.5;
  GLfloat h = frame->geom.height * 0.5;
  GLfloat z1 = z - 0.25 / (MI_COUNT(mi) + 1);
  GLfloat z2 = z - 0.5  / (MI_COUNT(mi) + 1); 
  GLfloat w1 = w;
  GLfloat h1 = h;
  GLfloat h2 = h;

  if (polaroid_p)
    {
      GLfloat minSize = MIN(w, h);
      GLfloat maxSize = MAX(w, h);

      /* Clip or scale image to fit in the frame.
       */
      if (clip_p)
        {
          w = h = minSize;
        }
      else
        {
          GLfloat scale = minSize / maxSize;
          w *= scale;
          h *= scale;
        }

      w1 = minSize * 1.16;      /* enlarge frame border */
      h1 = minSize * 1.5;
      h2 = w1;
      s /= 1.5;                 /* compensate for border size */
    }

  glPushMatrix();

  /* Position and scale this image.
   */
  glTranslatef (pos.x, pos.y, 0);
  glRotatef (pos.angle, 0, 0, 1);
  glScalef (s, s, 1);

  /* Draw the drop shadow. */
  if (shadows_p && !wire)
    {
      glColor3f (0, 0, 0);
      draw_drop_shadow(ss->shadow, -w1, -h1, z2, 2.0 * w1, h1 + h2, 20.0);
      glDisable (GL_BLEND);
    }

  glDisable (GL_LIGHTING);
  glEnable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);

  /* Draw the retro instant-film frame.
   */
  if (polaroid_p)
    {
      if (! wire)
        {
          glShadeModel (GL_SMOOTH);
          glEnable (GL_LINE_SMOOTH);
          glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);

          glColor3f (1, 1, 1);
          glBegin (GL_QUADS);
          glVertex3f (-w1, -h1, z2);
          glVertex3f ( w1, -h1, z2);
          glVertex3f ( w1,  h2, z2);
          glVertex3f (-w1,  h2, z2);
          glEnd();
        }

      glLineWidth (1.0);
      glColor3f (0.5, 0.5, 0.5);
      glBegin (GL_LINE_LOOP);
      glVertex3f (-w1, -h1, z);
      glVertex3f ( w1, -h1, z);
      glVertex3f ( w1,  h2, z);
      glVertex3f (-w1,  h2, z);
      glEnd();
    }

  /* Draw the image quad.
   */
  if (! wire)
    {
      GLfloat texw = w / frame->tw;
      GLfloat texh = h / frame->th;
      GLfloat texx = (frame->geom.x + 0.5 * frame->geom.width)  / frame->tw;
      GLfloat texy = (frame->geom.y + 0.5 * frame->geom.height) / frame->th;

      glBindTexture (GL_TEXTURE_2D, frame->texid);
      glEnable (GL_TEXTURE_2D);
      glColor3f (1, 1, 1);
      glBegin (GL_QUADS);
      glTexCoord2f (texx - texw, texy + texh); glVertex3f (-w, -h, z1);
      glTexCoord2f (texx + texw, texy + texh); glVertex3f ( w, -h, z1);
      glTexCoord2f (texx + texw, texy - texh); glVertex3f ( w,  h, z1);
      glTexCoord2f (texx - texw, texy - texh); glVertex3f (-w,  h, z1);
      glEnd();
      glDisable (GL_TEXTURE_2D);
    }

  /* Draw a box around it.
   */
  if (! wire)
    {
      glShadeModel (GL_SMOOTH);
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
    }
  glLineWidth (1.0);
  glColor3f (0.5, 0.5, 0.5);
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
      int sw, sh, ascent, descent;
      GLfloat scale = 1;
      const char *title = frame->title ? frame->title : "(untitled)";
      XCharStruct e;

      /* #### Highly approximate, but doing real clipping is harder... */
      int max = 35;
      if (strlen(title) > max)
        title += strlen(title) - max;

      texture_string_metrics (ss->texfont, title, &e, &ascent, &descent);
      sw = e.width;
      sh = ascent + descent;

      /* Scale the text to match the pixel size of the photo */
      scale *= w / 300.0;

      /* Move to below photo */
      glTranslatef (0, -h - sh * (polaroid_p ? 2.2 : 0.5), 0);
      glTranslatef (-sw*scale/2, sh*scale/2, z);
      glScalef (scale, scale, 1);

      if (wire || !polaroid_p)
        {
          glColor3f (1, 1, 1);
        }
      else
        {
          glColor3f (0, 0, 0);
        }

      if (!wire)
        {
          glEnable (GL_TEXTURE_2D);
          glEnable (GL_BLEND);
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
    if (loading_initial_image (mi))
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

    glPushMatrix();
    glTranslatef (MI_WIDTH(mi)/2, MI_HEIGHT(mi)/2, 0);
    glRotatef(current_device_rotation(), 0, 0, 1);
    glTranslatef (-MI_WIDTH(mi)/2, -MI_HEIGHT(mi)/2, 0);

    /* Handle state transitions. */
    switch (ss->mode)
      {
      case SHUFFLE:
        if (--ss->mode_tick <= 0)
          {
            ss->nframe = (ss->nframe+1) % (MI_COUNT(mi)+1);

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
      default:
        abort();
      }

    t = 1.0 - ss->mode_tick / (fade_ticks / speed);
    t = 0.5 * (1.0 - cos(M_PI * t));

    /* Draw the images. */
    for (i = 0; i < MI_COUNT(mi) + (ss->mode == SHUFFLE); ++i)
      {
        int j = (ss->nframe + i + 1) % (MI_COUNT(mi) + 1);

        if (ss->frames[j].loaded_p)
          {
            GLfloat s = 1.0;
            GLfloat z = (GLfloat)i / (MI_COUNT(mi) + 1);

            switch (ss->mode)
              {
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
              case NORMAL:
              case LOADING:
                t = 1.0;
                break;
              default:
                abort();
              }

            draw_image(mi, j, t, s, z);
          }
      }
    glPopMatrix();
  }

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), MI_WINDOW(mi));
}

XSCREENSAVER_MODULE ("Photopile", photopile)

#endif /* USE_GL */
