/* flyingtoasters, Copyright (c) 2003, 2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws 3D flying toasters, and toast.  Inspired by the ancient 
 * Berkeley Systems / After Dark hack, but now updated to the wide
 * wonderful workd of OpenGL and 3D!
 *
 * Code by jwz; object models by Baconmonkey.
 *
 * The original After Dark flying toasters, with the fluffy white wings,
 * were a trademark of Berkeley Systems.  Berkeley Systems ceased to exist
 * some time in 1998, having been gobbled up by Sierra Online, who were
 * subsequently gobbled up by Flipside and/or Vivendi (it's hard to tell
 * exactly what happened when.)
 *
 * I doubt anyone even cares any more, but if they do, hopefully this homage,
 * with the space-age 3D jet-plane toasters, will be considered different
 * enough that whoever still owns the trademark to the fluffy-winged 2D
 * bitmapped toasters won't get all huffy at us.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"FlyingToasters"
#define HACK_INIT	init_toasters
#define HACK_DRAW	draw_toasters
#define HACK_RESHAPE	reshape_toasters
#define HACK_HANDLE_EVENT toaster_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts	xlockmore_opts

#define DEF_SPEED       "1.0"
#define DEF_NTOASTERS   "20"
#define DEF_NSLICES     "25"
#define DEF_TEXTURE     "True"

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

/* #define DEBUG */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#include "xlockmore.h"
#include "gltrackball.h"
#include "xpm-ximage.h"
#include <ctype.h>

#include "../images/chromesphere.xpm"
#include "../images/toast.xpm"

#ifdef USE_GL /* whole file */

#include <GL/glu.h>


#include "gllist.h"

extern struct gllist
  *toaster, *toaster_base, *toaster_handle, *toaster_handle2, *toaster_jet,
  *toaster_knob, *toaster_slots, *toaster_wing, *toast, *toast2;

struct gllist **all_objs[] = {
  &toaster, &toaster_base, &toaster_handle, &toaster_handle2, &toaster_jet,
  &toaster_knob, &toaster_slots, &toaster_wing, &toast, &toast2
};

#define BASE_TOASTER	0
#define BASE		1
#define HANDLE		2
#define HANDLE_SLOT	3
#define JET		4
#define KNOB		5
#define SLOTS		6
#define JET_WING	7
#define TOAST		8
#define TOAST_BITTEN	9

#define GRID_SIZE   60
#define GRID_DEPTH 500


static struct { GLfloat x, y; } nice_views[] = {
  {  0,  120 },
  {  0, -120 },
  { 12,   28 },     /* this is a list of viewer rotations that look nice. */
  { 12,  -28 },     /* every now and then we switch to a new one.         */
  {-10,  -28 },     /* (but we only use the first two at start-up.)       */
  { 40,  -60 },
  {-40,  -60 },
  { 40,   60 },
  {-40,   60 },
  { 30,    0 },
  {-30,    0 },
};


typedef struct {
  GLfloat x, y, z;
  GLfloat dx, dy, dz;
  Bool toaster_p;
  int toast_type;      /* 0, 1 */
  GLfloat handle_pos;  /* 0.0 - 1.0 */
  GLfloat knob_pos;    /* degrees */
  int loaded;          /* 2 bits */
} floater;

typedef struct {
  GLXContext *glx_context;
  trackball_state *user_trackball;
  Bool button_down_p;

  int last_view, target_view;
  GLfloat view_x, view_y;
  int view_steps, view_tick;
  Bool auto_tracking_p;

  GLuint *dlists;
  GLuint chrome_texture;
  GLuint toast_texture;

  int nfloaters;
  floater *floaters;

} toaster_configuration;

static toaster_configuration *bps = NULL;

static GLfloat speed;
static int ntoasters;
static int nslices;
static int do_texture;

static XrmOptionDescRec opts[] = {
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-ntoasters",  ".ntoasters", XrmoptionSepArg, 0 },
  { "-nslices",    ".nslices",   XrmoptionSepArg, 0 },
  {"-texture",     ".texture",   XrmoptionNoArg, "True" },
  {"+texture",     ".texture",   XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&speed,      "speed",      "Speed",   DEF_SPEED,     t_Float},
  {&ntoasters,  "ntoasters",  "Count",   DEF_NTOASTERS, t_Int},
  {&nslices,    "nslices",    "Count",   DEF_NSLICES,   t_Int},
  {&do_texture, "texture",    "Texture", DEF_TEXTURE,   t_Bool},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void
reset_floater (ModeInfo *mi, floater *f)
{
/*  toaster_configuration *bp = &bps[MI_SCREEN(mi)]; */

  GLfloat n = GRID_SIZE/2.0;
  GLfloat n2 = GRID_DEPTH/2.0;
  GLfloat delta = GRID_SIZE * speed / 200.0;

  f->dx = 0;
  f->dy = 0;
  f->dz = delta;

  f->dz += BELLRAND(delta) - delta/3;

  if (! (random() % 5)) {
    f->dx += (BELLRAND(delta*2) - delta);
    f->dy += (BELLRAND(delta*2) - delta);
  }

  if (! (random() % 40)) f->dz *= 10;    /* occasional speedy one */

  f->x = frand(n) - n/2;
  f->y = frand(n) - n/2;
  f->z = -n2 - frand(delta * 4);

  if (f->toaster_p)
    {
      f->loaded = 0;
      f->knob_pos = frand(180) - 90;
      f->handle_pos = ((random() & 1) ? 0.0 : 1.0);

      if (f->handle_pos > 0.8 && (! (random() % 5)))
        f->loaded = (random() & 3);  /* let's toast! */
    }
  else
    {
      if (! (random() % 10))
        f->toast_type = 1;	/* toast_bitten */
    }
}


static void
tick_floater (ModeInfo *mi, floater *f)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];

  GLfloat n1 = GRID_DEPTH/2.0;
  GLfloat n2 = GRID_SIZE*4;

  if (bp->button_down_p) return;

  f->x += f->dx;
  f->y += f->dy;
  f->z += f->dz;

  if (! (random() % 50000))  /* sudden gust of gravity */
    f->dy -= 2.8;

  if (f->x < -n2 || f->x > n2 ||
      f->y < -n2 || f->y > n2 ||
      f->z > n1)
    reset_floater (mi, f);
}


static void
auto_track_init (ModeInfo *mi)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];
  bp->last_view = (random() % 2);
  bp->target_view = bp->last_view + 2;
  bp->view_x = nice_views[bp->last_view].x;
  bp->view_y = nice_views[bp->last_view].y;
  bp->view_steps = 100;
  bp->view_tick = 0;
  bp->auto_tracking_p = True;
}


static void
auto_track (ModeInfo *mi)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];

  if (bp->button_down_p)
    return;

  /* if we're not moving, maybe start moving.  Otherwise, do nothing. */
  if (! bp->auto_tracking_p)
    {
      static int tick = 0;
      if (++tick < 200/speed) return;
      tick = 0;
      if (! (random() % 5))
        bp->auto_tracking_p = True;
      else
        return;
    }


  {
    GLfloat ox = nice_views[bp->last_view].x;
    GLfloat oy = nice_views[bp->last_view].y;
    GLfloat tx = nice_views[bp->target_view].x;
    GLfloat ty = nice_views[bp->target_view].y;

    /* move from A to B with sinusoidal deltas, so that it doesn't jerk
       to a stop. */
    GLfloat th = sin ((M_PI / 2) * (double) bp->view_tick / bp->view_steps);

    bp->view_x = (ox + ((tx - ox) * th));
    bp->view_y = (oy + ((ty - oy) * th));
    bp->view_tick++;

  if (bp->view_tick >= bp->view_steps)
    {
      bp->view_tick = 0;
      bp->view_steps = (350.0 / speed);
      bp->last_view = bp->target_view;
      bp->target_view = (random() % (countof(nice_views) - 2)) + 2;
      bp->auto_tracking_p = False;
    }
  }
}


/* Window management, etc
 */
void
reshape_toasters (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (40.0, 1/h, 1.0, 250);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 2.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


Bool
toaster_handle_event (ModeInfo *mi, XEvent *event)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->user_trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (bp->user_trackball, event->xbutton.button, 5,
                              !event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      gltrackball_track (bp->user_trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
load_textures (ModeInfo *mi)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];
  XImage *xi;

  xi = xpm_to_ximage (mi->dpy, mi->xgwa.visual, mi->xgwa.colormap,
                      chromesphere_xpm);
  clear_gl_error();

  glGenTextures (1, &bp->chrome_texture);
  glBindTexture (GL_TEXTURE_2D, bp->chrome_texture);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                xi->width, xi->height, 0,
                GL_RGBA,
                /* GL_UNSIGNED_BYTE, */
                GL_UNSIGNED_INT_8_8_8_8_REV,
                xi->data);
  check_gl_error("texture");

  xi = xpm_to_ximage (mi->dpy, mi->xgwa.visual, mi->xgwa.colormap,
                      toast_xpm);

  glGenTextures (1, &bp->toast_texture);
  glBindTexture (GL_TEXTURE_2D, bp->toast_texture);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                xi->width, xi->height, 0,
                GL_RGBA,
                /* GL_UNSIGNED_BYTE, */
                GL_UNSIGNED_INT_8_8_8_8_REV,
                xi->data);
  check_gl_error("texture");

  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_T);
  glEnable(GL_TEXTURE_2D);
}


void 
init_toasters (ModeInfo *mi)
{
  toaster_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bps) {
    bps = (toaster_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (toaster_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_toasters (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
/*      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};*/
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  if (!wire && do_texture)
    load_textures (mi);

  bp->user_trackball = gltrackball_init ();
  auto_track_init (mi);

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      struct gllist *gll = *all_objs[i];
      if (wire)
        gll->primitive = GL_LINE_LOOP;

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);
      glRotatef (180, 0, 0, 1);
      glScalef (6, 6, 6);

      glBindTexture (GL_TEXTURE_2D, 0);

      if (i == BASE_TOASTER)
        {
          GLfloat color[4] = {1.00, 1.00, 1.00, 1.00};
          GLfloat spec[4]  = {1.00, 1.00, 1.00, 1.0};
          GLfloat shiny    = 20.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
          if (do_texture)
            glBindTexture (GL_TEXTURE_2D, bp->chrome_texture);
          glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
          glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
        }
      else if (i == TOAST || i == TOAST_BITTEN)
        {
          GLfloat color[4] = {0.80, 0.80, 0.00, 1.0};
          GLfloat spec[4]  = {0.00, 0.00, 0.00, 1.0};
          GLfloat shiny    = 0.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
          if (do_texture)
            glBindTexture (GL_TEXTURE_2D, bp->toast_texture);
          glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
          glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

          glMatrixMode(GL_TEXTURE);
          glTranslatef(0.5, 0.5, 0);
          glMatrixMode(GL_MODELVIEW);
        }
      else if (i == SLOTS || i == HANDLE_SLOT)
        {
          GLfloat color[4] = {0.30, 0.30, 0.40, 1.0};
          GLfloat spec[4]  = {0.40, 0.40, 0.70, 1.0};
          GLfloat shiny    = 128.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else if (i == HANDLE)
        {
          GLfloat color[4] = {0.80, 0.10, 0.10, 1.0};
          GLfloat spec[4]  = {1.00, 1.00, 1.00, 1.0};
          GLfloat shiny    = 20.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else if (i == KNOB)
        {
          GLfloat color[4] = {0.80, 0.10, 0.10, 1.0};
          GLfloat spec[4]  = {0.00, 0.00, 0.00, 1.0};
          GLfloat shiny    = 0.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else if (i == JET || i == JET_WING)
        {
          GLfloat color[4] = {0.70, 0.70, 0.70, 1.0};
          GLfloat spec[4]  = {1.00, 1.00, 1.00, 1.0};
          GLfloat shiny    = 20.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else if (i == BASE)
        {
          GLfloat color[4] = {0.50, 0.50, 0.50, 1.0};
          GLfloat spec[4]  = {1.00, 1.00, 1.00, 1.0};
          GLfloat shiny    = 20.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else
        {
          GLfloat color[4] = {1.00, 1.00, 1.00, 1.00};
          GLfloat spec[4]  = {1.00, 1.00, 1.00, 1.0};
          GLfloat shiny    = 128.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }

      renderList (gll);

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

  bp->nfloaters = ntoasters + nslices;
  bp->floaters = (floater *) calloc (bp->nfloaters, sizeof (floater));

  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f = &bp->floaters[i];
      /* arrange the list so that half the toasters are in front of bread,
         and half are behind. */
      f->toaster_p = ((i < ntoasters / 2) ||
                      (i >= (nslices + (ntoasters / 2))));
      reset_floater (mi, f);

      /* Position the first generation randomly, but make sure they aren't
         on screen yet (until we rotate the view into position.)
       */
      {
        GLfloat min = -GRID_DEPTH/2;
        GLfloat max =  GRID_DEPTH/3.5;
        f->z = frand (max - min) + min;
      }
    }
}


static void
draw_origin (ModeInfo *mi)
{
# ifdef DEBUG
/*  toaster_configuration *bp = &bps[MI_SCREEN(mi)];*/

  if (!MI_IS_WIREFRAME(mi)) glDisable(GL_LIGHTING);
  if (!MI_IS_WIREFRAME(mi) && do_texture) glDisable(GL_TEXTURE_2D);

  glPushMatrix();
  glScalef (5, 5, 5);
  glBegin(GL_LINES);
  glVertex3f(-1, 0, 0); glVertex3f(1, 0, 0);
  glVertex3f(0, -1, 0); glVertex3f(0, 1, 0);
  glVertex3f(0, 0, -1); glVertex3f(0, 0, 1);
  glEnd();
  glPopMatrix();

  if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
  if (!MI_IS_WIREFRAME(mi) && do_texture) glEnable(GL_TEXTURE_2D);
# endif /* DEBUG */
}


static void
draw_grid (ModeInfo *mi)
{
# ifdef DEBUG
/*  toaster_configuration *bp = &bps[MI_SCREEN(mi)];*/

  if (!MI_IS_WIREFRAME(mi)) glDisable(GL_LIGHTING);
  if (!MI_IS_WIREFRAME(mi) && do_texture) glDisable(GL_TEXTURE_2D);
  glPushMatrix();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-GRID_SIZE/2, -GRID_SIZE/2, 0);
  glVertex3f(-GRID_SIZE/2,  GRID_SIZE/2, 0);
  glVertex3f( GRID_SIZE/2,  GRID_SIZE/2, 0);
  glVertex3f( GRID_SIZE/2, -GRID_SIZE/2, 0);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-GRID_SIZE/2, GRID_SIZE/2, -GRID_DEPTH/2);
  glVertex3f(-GRID_SIZE/2, GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2, GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2, GRID_SIZE/2, -GRID_DEPTH/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-GRID_SIZE/2, -GRID_SIZE/2, -GRID_DEPTH/2);
  glVertex3f(-GRID_SIZE/2, -GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2, -GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2, -GRID_SIZE/2, -GRID_DEPTH/2);
  glEnd();
  glBegin(GL_LINES);
  glVertex3f(-GRID_SIZE/2, -GRID_SIZE/2, -GRID_DEPTH/2);
  glVertex3f(-GRID_SIZE/2,  GRID_SIZE/2, -GRID_DEPTH/2);
  glVertex3f(-GRID_SIZE/2, -GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f(-GRID_SIZE/2,  GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2, -GRID_SIZE/2, -GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2,  GRID_SIZE/2, -GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2, -GRID_SIZE/2,  GRID_DEPTH/2);
  glVertex3f( GRID_SIZE/2,  GRID_SIZE/2,  GRID_DEPTH/2);
  glEnd();
  glPopMatrix();

  if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
  if (!MI_IS_WIREFRAME(mi) && do_texture) glEnable(GL_TEXTURE_2D);
# endif /* DEBUG */
}


static void
draw_floater (ModeInfo *mi, floater *f)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat n;

  glFrontFace(GL_CCW);

  glPushMatrix();
  glTranslatef (f->x, f->y, f->z);
  if (f->toaster_p)
    {
      glPushMatrix();
      glRotatef (180, 0, 1, 0);
      glCallList (bp->dlists[BASE_TOASTER]);
      mi->polygon_count += (*all_objs[BASE_TOASTER])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glTranslatef(0, 1.01, 0);
      n = 0.91; glScalef(n,n,n);
      glCallList (bp->dlists[SLOTS]);
      mi->polygon_count += (*all_objs[SLOTS])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glRotatef (180, 0, 1, 0);
      glTranslatef(0, -0.4, -2.38);
      n = 0.33; glScalef(n,n,n);
      glCallList (bp->dlists[HANDLE_SLOT]);
      mi->polygon_count += (*all_objs[HANDLE_SLOT])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glTranslatef(0, -1.1, 3);
      n = 0.3; glScalef (n,n,n);
      glTranslatef(0, f->handle_pos * 4.8, 0);
      glCallList (bp->dlists[HANDLE]);
      mi->polygon_count += (*all_objs[HANDLE])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glRotatef (180, 0, 1, 0);
      glTranslatef(0, -1.1, -3);     /* where the handle is */
      glTranslatef (1, -0.4, 0);     /* down and to the left */
      n = 0.08; glScalef (n,n,n);
      glRotatef (f->knob_pos, 0, 0, 1);
      glCallList (bp->dlists[KNOB]);
      mi->polygon_count += (*all_objs[KNOB])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glRotatef (180, 0, 1, 0);
      glTranslatef (0, -2.3, 0);
      glCallList (bp->dlists[BASE]);
      mi->polygon_count += (*all_objs[BASE])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glTranslatef(-4.8, 0, 0);
      glCallList (bp->dlists[JET_WING]);
      mi->polygon_count += (*all_objs[JET_WING])->points / 3;
      glScalef (0.5, 0.5, 0.5);
      glTranslatef (-2, -1, 0);
      glCallList (bp->dlists[JET]);
      mi->polygon_count += (*all_objs[JET])->points / 3;
      glPopMatrix();

      glPushMatrix();
      glTranslatef(4.8, 0, 0);
      glScalef(-1, 1, 1);
      glFrontFace(GL_CW);
      glCallList (bp->dlists[JET_WING]);
      mi->polygon_count += (*all_objs[JET_WING])->points / 3;
      glScalef (0.5, 0.5, 0.5);
      glTranslatef (-2, -1, 0);
      glCallList (bp->dlists[JET]);
      mi->polygon_count += (*all_objs[JET])->points / 3;
      glFrontFace(GL_CCW);
      glPopMatrix();

      if (f->loaded)
        {
          glPushMatrix();
          glTranslatef(0, 1.01, 0);
          n = 0.91; glScalef(n,n,n);
          glRotatef (90, 0, 0, 1);
          glRotatef (90, 0, 1, 0);
          glTranslatef(0, 0, -0.95);
          glTranslatef(0, 0.72, 0);
          if (f->loaded & 1)
            {
              glCallList (bp->dlists[TOAST]);
              mi->polygon_count += (*all_objs[TOAST])->points / 3;
            }
          glTranslatef(0, -1.46, 0);
          if (f->loaded & 2)
            {
              glCallList (bp->dlists[TOAST]);
              mi->polygon_count += (*all_objs[TOAST])->points / 3;
            }
          glPopMatrix();
        }
    }
  else
    {
      glScalef (0.7, 0.7, 0.7);
      if (f->toast_type == 0)
        {
          glCallList (bp->dlists[TOAST]);
          mi->polygon_count += (*all_objs[TOAST])->points / 3;
        }
      else
        {
          glCallList (bp->dlists[TOAST_BITTEN]);
          mi->polygon_count += (*all_objs[TOAST_BITTEN])->points / 3;
        }
    }

  glPopMatrix();
}



void
draw_toasters (ModeInfo *mi)
{
  toaster_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  glRotatef(bp->view_x, 1, 0, 0);
  glRotatef(bp->view_y, 0, 1, 0);
  gltrackball_rotate (bp->user_trackball);


#if 0
  {
    floater F;
    F.toaster_p = 0;
    F.toast_type = 1;
    F.handle_pos = 0;
    F.knob_pos = -90;
    F.loaded = 3;
    F.x = F.y = F.z = 0;
    F.dx = F.dy = F.dz = 0;

    glScalef(2,2,2);
    if (!MI_IS_WIREFRAME(mi)) glDisable(GL_LIGHTING);
    if (!MI_IS_WIREFRAME(mi) && do_texture) glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    glVertex3f(-10, 0, 0); glVertex3f(10, 0, 0);
    glVertex3f(0, -10, 0); glVertex3f(0, 10, 0);
    glVertex3f(0, 0, -10); glVertex3f(0, 0, 10);
    glEnd();
    if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
    if (!MI_IS_WIREFRAME(mi) && do_texture) glEnable(GL_TEXTURE_2D);

    draw_floater (mi, &F);
    glPopMatrix ();
    if (mi->fps_p) do_fps (mi);
    glFinish();
    glXSwapBuffers(dpy, window);
    return;
  }
#endif

  glScalef (0.5, 0.5, 0.5);
  draw_origin (mi);
  glTranslatef (0, 0, -GRID_DEPTH/2.5);
  draw_grid (mi);

  mi->polygon_count = 0;
  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f = &bp->floaters[i];
      draw_floater (mi, f);
      tick_floater (mi, f);
    }
  auto_track (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
