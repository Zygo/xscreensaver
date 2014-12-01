/* companioncube, Copyright (c) 2011-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* The symptoms most commonly produced by Enrichment Center testing are
   superstition, perceiving inanimate objects as alive, and hallucinations.
   The Enrichment Center reminds you that the weighted companion cube will
   never threaten to stab you and, in fact, cannot speak.  In the event that
   the Weighted Companion Cube does speak, the Enrichment Center urges you to
   disregard its advice.
 */


#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*count:        3           \n" \
			"*wireframe:    False       \n" \

/* #define DEBUG */


# define refresh_cube 0
# define release_cube 0
#define DEF_SPEED  "1.0"
#define DEF_SPIN   "False"
#define DEF_WANDER "False"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)
#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

#include "xlockmore.h"
#include "rotator.h"
#include "gltrackball.h"
#include "xpm-ximage.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gllist.h"

extern const struct gllist *companion_quad, *companion_disc, *companion_heart;
static const struct gllist * const *all_objs[] = {
  &companion_quad, &companion_disc, &companion_heart
};
#define BASE_QUAD  0
#define BASE_DISC  1
#define BASE_HEART 2
#define FULL_CUBE  3

#define SPEED_SCALE 0.2

typedef struct {
  GLfloat x, y, z;
  GLfloat ix, iy, iz;
  GLfloat dx, dy, dz;
  GLfloat ddx, ddy, ddz;
  GLfloat zr;
  rotator *rot;
  Bool spinner_p;
} floater;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint *dlists;
  int cube_polys;

  int nfloaters;
  floater *floaters;

} cube_configuration;

static cube_configuration *bps = NULL;

static GLfloat speed;
static Bool do_spin;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",    XrmoptionSepArg, 0       },
  { "-spin",       ".spin",     XrmoptionNoArg,  "True"  },
  { "+spin",       ".spin",     XrmoptionNoArg,  "False" },
  { "-wander",     ".wander",   XrmoptionNoArg,  "True"  },
  { "+wander",     ".wander",   XrmoptionNoArg,  "False" },
};

static argtype vars[] = {
  {&speed,     "speed",    "Speed",   DEF_SPEED,    t_Float},
  {&do_spin,   "spin",     "Spin",    DEF_SPIN,     t_Bool},
  {&do_wander, "wander",   "Wander",  DEF_WANDER,   t_Bool},
};

ENTRYPOINT ModeSpecOpt cube_opts = {countof(opts), opts, countof(vars), vars, NULL};


#define BOTTOM 28.0

static void
reset_floater (ModeInfo *mi, floater *f)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];

  f->y = -BOTTOM;
  f->x = f->ix;
  f->z = f->iz;

  /* Yes, I know I'm varying the force of gravity instead of varying the
     launch velocity.  That's intentional: empirical studies indicate
     that it's way, way funnier that way. */

  f->dy = 5.0;
  f->dx = 0;
  f->dz = 0;

  /* -0.18 max  -0.3 top -0.4 middle  -0.6 bottom */
  f->ddy = speed * SPEED_SCALE * (-0.6 + BELLRAND(0.45));
  f->ddx = 0;
  f->ddz = 0;

  if (do_spin || do_wander)
    f->spinner_p = 0;
  else
    f->spinner_p = !(random() % (3 * bp->nfloaters));

  if (! (random() % (30 * bp->nfloaters)))
    {
      f->dx = BELLRAND(1.8) * RANDSIGN();
      f->dz = BELLRAND(1.8) * RANDSIGN();
    }

  f->zr = frand(180);
  if (do_spin || do_wander)
    {
      f->y = 0;
      if (bp->nfloaters > 2)
        f->y += frand(3.0) * RANDSIGN();
    }
}


static void
tick_floater (ModeInfo *mi, floater *f)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];

  if (bp->button_down_p) return;

  if (do_spin || do_wander) return;

  f->dx += f->ddx;
  f->dy += f->ddy;
  f->dz += f->ddz;

  f->x += f->dx * speed * SPEED_SCALE;
  f->y += f->dy * speed * SPEED_SCALE;
  f->z += f->dz * speed * SPEED_SCALE;

  if (f->y < -BOTTOM ||
      f->x < -BOTTOM*8 || f->x > BOTTOM*8 ||
      f->z < -BOTTOM*8 || f->z > BOTTOM*8)
    reset_floater (mi, f);
}





static int
build_corner (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat s;
  const struct gllist *gll = *all_objs[BASE_QUAD];

  glPushMatrix();
  glTranslatef (-0.5, -0.5, -0.5);
  s = 0.659;
  glScalef (s, s, s);

  glRotatef (180, 0, 1, 0);
  glRotatef (180, 0, 0, 1);
  glTranslatef (-0.12, -1.64, 0.12);
  glCallList (bp->dlists[BASE_QUAD]);
  glPopMatrix();

  return gll->points / 3;
}


static int
build_face (ModeInfo *mi)
{
  int polys = 0;
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat s;
  const struct gllist *gll;

  GLfloat base_color[4]   = {0.53, 0.60, 0.66, 1.00};
  GLfloat heart_color[4]  = {0.92, 0.67, 1.00, 1.00};
  GLfloat disc_color[4]   = {0.75, 0.92, 1.00, 1.00};
  GLfloat corner_color[4] = {0.75, 0.92, 1.00, 1.00};

  if (!wire) 
    {
      GLfloat w = 0.010;
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, base_color);
      glPushMatrix();
      glNormal3f (0, 0, -1);
      glTranslatef (-0.5, -0.5, -0.5);

      glBegin(GL_QUADS);
      glVertex3f (0,     0,     0);
      glVertex3f (0,     0.5-w, 0);
      glVertex3f (0.5-w, 0.5-w, 0);
      glVertex3f (0.5-w, 0,     0);

      glVertex3f (0.5+w, 0,     0);
      glVertex3f (0.5+w, 0.5-w, 0);
      glVertex3f (1,     0.5-w, 0);
      glVertex3f (1,     0,     0);

      glVertex3f (0,     0.5+w, 0);
      glVertex3f (0,     1,     0);
      glVertex3f (0.5-w, 1,     0);
      glVertex3f (0.5-w, 0.5+w, 0);

      glVertex3f (0.5+w, 0.5+w, 0);
      glVertex3f (0.5+w, 1,     0);
      glVertex3f (1,     1,     0);
      glVertex3f (1,     0.5+w, 0);
      glEnd();

      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, heart_color);

      glNormal3f (0, -1, 0);
      glBegin(GL_QUADS);
      glVertex3f (0, 0.5+w, 0);
      glVertex3f (1, 0.5+w, 0);
      glVertex3f (1, 0.5+w, w);
      glVertex3f (0, 0.5+w, w);
      glEnd();

      glNormal3f (0, 1, 0);
      glBegin(GL_QUADS);
      glVertex3f (0, 0.5-w, w);
      glVertex3f (1, 0.5-w, w);
      glVertex3f (1, 0.5-w, 0);
      glVertex3f (0, 0.5-w, 0);
      glEnd();

      glNormal3f (-1, 0, 0);
      glBegin(GL_QUADS);
      glVertex3f (0.5+w, 0, w);
      glVertex3f (0.5+w, 1, w);
      glVertex3f (0.5+w, 1, 0);
      glVertex3f (0.5+w, 0, 0);
      glEnd();

      glNormal3f (1, 0, 0);
      glBegin(GL_QUADS);
      glVertex3f (0.5-w, 0, 0);
      glVertex3f (0.5-w, 1, 0);
      glVertex3f (0.5-w, 1, w);
      glVertex3f (0.5-w, 0, w);
      glEnd();

      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, heart_color);

      glNormal3f (0, 0, -1);
      glTranslatef (0, 0, w);
      glBegin(GL_QUADS);
      glVertex3f (0, 0, 0);
      glVertex3f (0, 1, 0);
      glVertex3f (1, 1, 0);
      glVertex3f (1, 0, 0);
      glEnd();

      glPopMatrix();
    }

  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, corner_color);

  glPushMatrix();
  polys += build_corner (mi); glRotatef (90, 0, 0, 1);
  polys += build_corner (mi); glRotatef (90, 0, 0, 1);
  polys += build_corner (mi); glRotatef (90, 0, 0, 1);
  polys += build_corner (mi);

  glRotatef (90, 0, 0, 1);
  glTranslatef (0.585, -0.585, -0.5655);

  s = 10.5;
  glScalef (s, s, s);
  glRotatef (180, 0, 1, 0);

  if (! wire)
    {
      gll = *all_objs[BASE_HEART];
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, heart_color);
      glCallList (bp->dlists[BASE_HEART]);
      polys += gll->points / 3;
    }

  gll = *all_objs[BASE_DISC];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, disc_color);
  glCallList (bp->dlists[BASE_DISC]);
  polys += gll->points / 3;

  glPopMatrix();
  return polys;
}


static int
build_cube (ModeInfo *mi)
{
  int polys = 0;
  glPushMatrix();
  polys += build_face (mi); glRotatef (90, 0, 1, 0);
  polys += build_face (mi); glRotatef (90, 0, 1, 0);
  polys += build_face (mi); glRotatef (90, 0, 1, 0);
  polys += build_face (mi); glRotatef (90, 1, 0, 0);
  polys += build_face (mi); glRotatef (180,1, 0, 0);
  polys += build_face (mi);
  glPopMatrix();
  return polys;
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void
init_cube (ModeInfo *mi)
{
  cube_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bps) {
    bps = (cube_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (cube_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.7, 0.2, 0.4, 0.0};
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

  bp->trackball = gltrackball_init (False);

  bp->dlists = (GLuint *) calloc (countof(all_objs)+2, sizeof(GLuint));
  for (i = 0; i < countof(all_objs)+1; i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      glNewList (bp->dlists[i], GL_COMPILE);
      renderList (gll, wire);
      glEndList ();
    }

  glNewList (bp->dlists[i], GL_COMPILE);
  bp->cube_polys = build_cube (mi);
  glEndList ();


  bp->nfloaters = MI_COUNT (mi);
  bp->floaters = (floater *) calloc (bp->nfloaters, sizeof (floater));

  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f = &bp->floaters[i];
      double spin_speed   = do_spin ? 0.7 : 10;
      double wander_speed = do_wander ? 0.02 : 0.05 * speed * SPEED_SCALE;
      double spin_accel   = 0.5;
      f->rot = make_rotator (spin_speed, spin_speed, spin_speed,
                             spin_accel,
                             wander_speed,
                             True);
      if (bp->nfloaters == 2)
        {
          f->x = (i ? 2 : -2);
        }
      else if (i != 0)
        {
          double th = (i - 1) * M_PI*2 / (bp->nfloaters-1);
          double r = 3;
          f->x = r * cos(th);
          f->z = r * sin(th);
        }

      f->ix = f->x;
      f->iy = f->y;
      f->iz = f->z;
      reset_floater (mi, f);
    }
}


static void
draw_floater (ModeInfo *mi, floater *f)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat n;
  double x, y, z;

  get_position (f->rot, &x, &y, &z, !bp->button_down_p);

  glPushMatrix();
  glTranslatef (f->x, f->y, f->z);

  if (do_wander)
    glTranslatef (x, y, z);

  if (do_spin)
    get_rotation (f->rot, &x, &y, &z, !bp->button_down_p);

  if (do_spin || f->spinner_p)
    {
      glRotatef (x * 360, 1, 0, 0);
      glRotatef (y * 360, 0, 1, 0);
      glRotatef (z * 360, 0, 0, 1);
    }
  else
    {
      glRotatef (f->zr * 360, 0, 1, 0);
    }

  n = 1.5;
  if      (bp->nfloaters > 99) n *= 0.05;
  else if (bp->nfloaters > 25) n *= 0.18;
  else if (bp->nfloaters > 9)  n *= 0.3;
  else if (bp->nfloaters > 1)  n *= 0.7;

  n *= 2;

  if ((do_spin || do_wander) && bp->nfloaters > 1)
    n *= 0.7;

  glScalef(n, n, n);

  glCallList (bp->dlists[FULL_CUBE]);
  mi->polygon_count += bp->cube_polys;
/*  build_cube (mi);*/

  glPopMatrix();
}



ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);
  gltrackball_rotate (bp->trackball);

  glScalef (2, 2, 2);

  mi->polygon_count = 0;

# if 0
  {
    floater F;
    F.x = F.y = F.z = 0;
    F.dx = F.dy = F.dz = 0;
    F.ddx = F.ddy = F.ddz = 0;
    F.rot = make_rotator (0, 0, 0, 1, 0, False);
    glRotatef (45, 0, 1, 0);
    draw_floater (mi, &F);
  }
# else
  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f = &bp->floaters[i];
      draw_floater (mi, f);
      tick_floater (mi, f);
    }
# endif

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("CompanionCube", companioncube, cube)

#endif /* USE_GL */
