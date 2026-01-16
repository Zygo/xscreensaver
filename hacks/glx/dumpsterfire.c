/* dumpsterfire, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Created by jwz: 22-Apr-2025
 *
 * Dumpster model by: xx_n0va_x https://skfb.ly/psRAy
 *   and slightly modified by jwz.
 *   Licensed under Creative Commons Attribution
 *   http://creativecommons.org/licenses/by/4.0/
 */

#define DEFAULTS	"*delay:	      20000       \n" \
			"*showFPS:            False       \n" \
			"*wireframe:          False       \n" \
			"*dumpsterFrameColor: #777799"   "\n" \
			"*dumpsterPanelColor: #8888AA"   "\n" \
			"*insideColor:        #112211"   "\n" \
 			"*hingesColor:        #666666"   "\n" \
			"*axleColor:          #444444"   "\n" \
			"*lidColor:           #8888FF"   "\n" \
			"*lidPanelColor:      #7777EE"   "\n" \

# define release_dumpster 0

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include "gllist.h"
#include "easing.h"
#include <ctype.h>

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

extern const struct gllist
  *dumpster_model_frame_half,
  *dumpster_model_panels_half,
  *dumpster_model_inside_half,
  *dumpster_model_hinges_half,
  *dumpster_model_axle,
  *dumpster_model_lid,
  *dumpster_model_lid_panels;

static const struct gllist * const *all_objs[] = {
  &dumpster_model_frame_half,
  &dumpster_model_panels_half,
  &dumpster_model_inside_half,
  &dumpster_model_hinges_half,
  &dumpster_model_axle,
  &dumpster_model_lid,
  &dumpster_model_lid_panels,
};

enum { FRAME_HALF, PANELS_HALF, INSIDE_HALF, HINGES_HALF, AXLE,
       LID, LID_PANELS };

typedef struct { GLfloat x, y, z; } XYZ;

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_DENSITY     "1.0"

typedef struct {
  float	fade;
  GLfloat color[4];
  XYZ pos, speed, accel;
  GLdouble sortkey;
} particle;


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  XYZ pos;
  XYZ wind;
  GLfloat tick;
  enum { DROP, IGNITE, OPEN, BURN, QUENCH, CLOSE, ROLL } state;
  GLfloat lid_angle[2];
  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];
  GLuint texid;
  GLuint sprite_dlist;
  GLfloat density;
  int nparticles;
  particle *particles;

} dumpster_configuration;

static dumpster_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed_arg;
static Bool do_wander;
static GLfloat density_arg;

static XrmOptionDescRec opts[] = {
  { "-spin",    ".spin",    XrmoptionNoArg, "True" },
  { "+spin",    ".spin",    XrmoptionNoArg, "False" },
  { "-speed",   ".speed",   XrmoptionSepArg, 0 },
  { "-wander",  ".wander",  XrmoptionNoArg, "True" },
  { "+wander",  ".wander",  XrmoptionNoArg, "False" },
  { "-density", ".density", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,     "spin",    "Spin",    DEF_SPIN,    t_Bool},
  {&do_wander,   "wander",  "Wander",  DEF_WANDER,  t_Bool},
  {&speed_arg,   "speed",   "Speed",   DEF_SPEED,   t_Float},
  {&density_arg, "density", "Density", DEF_DENSITY, t_Float},
};

ENTRYPOINT ModeSpecOpt dumpster_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


/* Color spectrum of temperature per Planck’s law.
 */
#undef RGB
#define RGB(x) \
  { (((x) >> 16) & 0xFF) / 255.0, \
    (((x) >>  8) & 0xFF) / 255.0, \
    (((x) >>  0) & 0xFF) / 255.0, \
    1.0 }
static const GLfloat fire_colors[][4] = {
  RGB(0x352201),  /*   550°C  */
  RGB(0x542803),  /*   630°C  */
  RGB(0x681100),  /*   680°C  */
  RGB(0x861600),  /*   740°C  */
  RGB(0xA00000),  /*   780°C  */
  RGB(0xC11B1B),  /*   810°C  */
  RGB(0xD44115),  /*   850°C  */
  RGB(0xE9582C),  /*   900°C  */
  RGB(0xE97E1C),  /*   950°C  */
  RGB(0xFFAA0F),  /*  1000°C  */
  RGB(0xFBC034),  /*  1100°C  */
  RGB(0xFFCF61),  /*  1200°C  */
  RGB(0xFFE6AD),  /* >1300°C  */
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),  /* Some padding so that the brightest flames aren't */
  RGB(0xFFE6AD),  /* hidden inside the dumpster. */
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
  RGB(0xFFE6AD),
};


static void
build_texture (ModeInfo *mi)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int x, y;
  int size = 128;
  int s2 = size / 2;
  int bpl = size * 4;
  unsigned char *data = malloc (bpl * size);

  for (y = 0; y < size; y++)
    {
      for (x = 0; x < size; x++)
        {
          double dist = (sqrt (((s2 - x) * (s2 - x)) +
                               ((s2 - y) * (s2 - y)))
                         / s2);
          unsigned char *c = &data [y * bpl + x * 4];
          unsigned char v = 0xFF * sin (dist > 1 ? 0 : (1 - dist));
          c[0] = 0xFF;
          c[1] = 0xFF;
          c[2] = 0xFF;
          c[3] = v;
        }
    }

  glGenTextures (1, &bp->texid);
  glBindTexture (GL_TEXTURE_2D, bp->texid);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  check_gl_error ("texture param");

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
  check_gl_error ("texture");
  free (data);

  bp->sprite_dlist = glGenLists (1);
  glNewList (bp->sprite_dlist, GL_COMPILE);
  if (wire)
    {
      double th = 0;
      glBegin(GL_LINE_LOOP);
      for (th = 0; th < M_PI*2; th += M_PI / 6)
        glVertex3f (0.5 * cos (th), 0.5 * sin (th), 0);
      glEnd();
    }
  else
    {
      glBegin(GL_QUADS);
      glTexCoord2d(1,1); glVertex3f( 0.5,  0.5, 0);
      glTexCoord2d(0,1); glVertex3f(-0.5,  0.5, 0);
      glTexCoord2d(0,0); glVertex3f(-0.5, -0.5, 0);
      glTexCoord2d(1,0); glVertex3f( 0.5, -0.5, 0);
      glEnd();
    }
  glEndList ();
}


static int
particle_sort_cmp (const void *aa, const void *bb)
{
  const particle *a = (particle *) aa;
  const particle *b = (particle *) bb;
  return (a->sortkey == b->sortkey ? 0 : a->sortkey < b->sortkey ? -1 : 1);
}


static void
draw_fire (ModeInfo *mi)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat size = 0.25 / bp->density;
  GLfloat max_size = 1.20;  /* Don't peek through closed lid */
  GLfloat min_size = 0.80;  /* Don't be too spotty */
  int i;

  switch (bp->state) {
  case DROP: case CLOSE: case ROLL:
    return;
  default:
    break;
  }

  /* I thought about making the sprites be tall ovals instead of circles,
     for more of a flame shape, but with billboarding that only looks right
     if "up" is the same for both camera and scene -- no tilting.
   */

  if (size > max_size) size = max_size;
  if (size < min_size) size = min_size;
  if (wire) size *= 0.6;

  /* For transparency to work right, we have to draw the sprites from
     back to front, from the perspective of the observer.  So project
     the origin of each sprite to screen coordinates, and sort that by Z.
   */
  {
    GLdouble mm[16], pm[16];
    GLint vp[4];
    glGetDoublev (GL_MODELVIEW_MATRIX, mm);
    glGetDoublev (GL_PROJECTION_MATRIX, pm);
    glGetIntegerv (GL_VIEWPORT, vp);

    for (i = 0; i < bp->nparticles; i++)
      {
        GLdouble x, y, z;
        particle *p = &bp->particles[i];
        gluProject (p->pos.y, p->pos.z, p->pos.x, mm, pm, vp, &x, &y, &z);
        p->sortkey = -z;
      }
    qsort (bp->particles, bp->nparticles, sizeof(*bp->particles),
           particle_sort_cmp);
  }

  /* Render each particle.
   */
  glPushMatrix();

  if (! wire)
    {
      glEnable (GL_DEPTH_TEST);
      glDepthFunc (GL_LESS);
      glDepthMask (GL_FALSE);

      glEnable (GL_TEXTURE_2D);
      glDisable (GL_LIGHTING);
      glShadeModel (GL_FLAT);
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glPolygonMode (GL_FRONT, GL_FILL);
      glBindTexture (GL_TEXTURE_2D, bp->texid);
    }

  /* Position at the lid that is open. */
  {
    GLfloat s = 0.5;
    glTranslatef ((bp->lid_angle[0] == 0 ? -1 : 1) * 2.3,
                  4,
                  0);
    glRotatef (90, -1, 0, 0);	/* Z is up */
    glScalef (s, s, s);
  }

  for (i = 0; i < bp->nparticles; i++)
    {
      GLfloat m[4][4];
      particle *p = &bp->particles[i];

      glColor4fv (p->color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                    p->color);

      /* Billboard the sprites to always face the camera.  Sadly, we have to
         glTranslate and then de-billboard for each sprite, or the flame
         always points toward the top of the screen instead of toward the top
         of the dumpster.  I think this makes it impossible to draw all of
         the polygons at once with glDrawArrays and a pair of vertex/color
         arrays. */

      glPushMatrix();

      glTranslatef (p->pos.x, p->pos.y, p->pos.z);

      glGetFloatv (GL_MODELVIEW_MATRIX, &m[0][0]);
      m[0][0] = 1; m[1][0] = 0; m[2][0] = 0;
      m[0][1] = 0; m[1][1] = 1; m[2][1] = 0;
      m[0][2] = 0; m[1][2] = 0; m[2][2] = 1;
      glLoadIdentity();
      glMultMatrixf (&m[0][0]);

      glScalef (size, size, size);
      glCallList (bp->sprite_dlist);
      mi->polygon_count++;

      glPopMatrix();
    }

  glPopMatrix();

  if (! wire)
    glDepthMask (GL_TRUE);


  /* Tick each particle for the next round.
     #### TODO: Make the fire animate faster per speed_arg. It's hard.
   */
  for (i = 0; i < bp->nparticles; i++)
    {
      particle *p = &bp->particles[i];

      p->pos.x += p->speed.x;
      p->pos.y += p->speed.y;
      p->pos.z += p->speed.z;

      if (p->pos.z > 5.0)		/* Turn fire shape into a cone. */
        {
          p->accel.x = 0.0016 * (p->pos.x > 0 ? -1 : 1);
          p->accel.y = 0.0016 * (p->pos.y > 0 ? -1 : 1);
        }

      p->speed.x += p->accel.x;
      p->speed.y += p->accel.y;
      p->speed.z += p->accel.z;

      if (p->pos.z > 4.5)		/* Wind outside of the dumpster.     */
        {
          p->accel.x += bp->wind.x;	/* Technically this should increase  */
          p->accel.y += bp->wind.y;	/* speed, but increasing accel looks */
          p->accel.z += bp->wind.z;	/* better. */
        }

      /* Alpha is particle's remaining lifetime, and temperature. */
      p->color[3] -= p->fade;

      if (bp->state == QUENCH || bp->state == IGNITE)
        p->color[3] -= p->fade * 3;

      if (p->color[3] <= 0.0)	/* Dead: reset. */
        {
          if (bp->state < QUENCH)
            {
              /* First tune speed.z and accel.z to get the vertical seething
                 to look ok.  Then tune fade to control flame height. */
              p->pos.x   = 0;
              p->pos.y   = 0;
              p->pos.z   = 0;
              p->speed.x = 0.12 * (frand(1) - 0.5);
              p->speed.y = 0.12 * (frand(1) - 0.5);
              p->speed.z = 0.06 * (frand(1) - 0.5);
              p->accel.x = 0;
              p->accel.y = 0;
              p->accel.z = 0.0032;
              p->fade = (frand (0.2) + 0.006);

              memcpy (p->color, fire_colors[countof(fire_colors)-1],
                      sizeof(p->color));
            }
        }
      else
        {
          /* Colorize but preserve alpha. */
          int i = p->color[3] * countof(fire_colors);
          if (i >= countof(fire_colors)) i = countof(fire_colors)-1;
          memcpy (p->color, fire_colors[i], sizeof(GLfloat) * 3);
        }
    }
}


ENTRYPOINT void
reshape_dumpster (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
dumpster_handle_event (ModeInfo *mi, XEvent *event)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      switch (bp->state) {
      case DROP: case IGNITE:
        bp->state = ROLL-1;
        bp->tick = 1.0;
        return True;
      case OPEN: case BURN:
        bp->state = QUENCH-1;
        bp->tick = 1.0;
        if (bp->lid_angle[0] != 0)
          bp->lid_angle[0] -= 0.6;
        else
          bp->lid_angle[1] -= 0.6;
        return True;
      default:
        return False;
      }
    }

  return False;
}


static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "Color");
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
init_dumpster (ModeInfo *mi)
{
  dumpster_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_dumpster (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
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

  {
    double spin_speed   = speed_arg * 0.04;
    double wander_speed = speed_arg * 0.004;
    double spin_accel   = 0.5;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }

  bp->density = density_arg;
  bp->nparticles = 10000 * bp->density;
  if (bp->nparticles < 10) bp->nparticles = 10;
  bp->particles = (particle *)
    calloc (bp->nparticles, sizeof(*bp->particles));

  build_texture (mi);

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  bp->state = DROP;
  bp->tick = 0;

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case FRAME_HALF:  key = "dumpsterFrameColor"; break;
      case PANELS_HALF: key = "dumpsterPanelColor"; break;
      case INSIDE_HALF: key = "insideColor";        break;
      case HINGES_HALF: key = "hingesColor";        break;
      case AXLE:        key = "axleColor";          break;
      case LID:         key = "lidColor";           break;
      case LID_PANELS:  key = "lidPanelColor";      break;
      default:
        abort();
      }

      parse_color (mi, key, bp->component_colors[i]);
      renderList (gll, wire);

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }
}


static int
draw_component (ModeInfo *mi, int i, Bool half_p)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];

  static const GLfloat spec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat shiny    = 128.0;
  glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
  glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);

  glFrontFace (GL_CCW);
  glCallList (bp->dlists[i]);

  if (half_p)
    {
      glPushMatrix();
      glScalef (-1, 1, 1);
      glFrontFace (GL_CW);
      glCallList (bp->dlists[i]);
      glPopMatrix();
    }

  return (half_p ? 2 : 1) * (*all_objs[i])->points / 3;
}


static void
tick_dumpster (ModeInfo *mi)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat ts;
  /* double fps = MI_DELAY(mi) ? 1000000.0 / MI_DELAY(mi) : 30; */
  double fps = 27;

  if (bp->button_down_p) return;

  switch (bp->state) {
  case DROP:   ts = 3;  break;
  case IGNITE: ts = 1;  break;
  case OPEN:   ts = 1;  break;
  case BURN:   ts = 99; break;
  case QUENCH: ts = 3;  break;
  case CLOSE:  ts = 1;  break;
  case ROLL:   ts = 3;  break;
  default:    abort();  break;
  }

  bp->tick += speed_arg * (1 / (ts * fps));

  if (bp->tick < 1) return;

  bp->tick = 0;
  bp->state = (bp->state + 1) % (ROLL + 1);

  switch (bp->state) {
  case IGNITE:				   /* Pick which lid we are opening */
    bp->lid_angle[random() % 2] += 0.001;
    bp->wind.x =  0.15 * (BELLRAND (1.0) - 0.5);
    bp->wind.y = -0.15 * (BELLRAND (0.5));
    bp->wind.z = 0;
    break;
  case ROLL:				   /* Close the rest of the way */
    bp->lid_angle[0] = bp->lid_angle[1] = 0;
    break;
  case CLOSE:
    memset (bp->particles, 0, bp->nparticles * sizeof (*bp->particles));
    break;
  case DROP:
    gltrackball_reset (bp->trackball, 0, 0);
  default:
    break;
  }
}


static void
draw_box (ModeInfo *mi)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat s;
  int i;

  glPushMatrix();

  glDisable (GL_BLEND);
  glEnable (GL_NORMALIZE);
  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LESS);
  glDepthMask (GL_TRUE);

  if (!wire)
    {
      glEnable (GL_LIGHTING);
      glShadeModel (GL_SMOOTH);
    }

  s = 12;
  glScalef (s, s, s);

  switch (bp->state) {
  case DROP:
    bp->pos.x = bp->pos.y = 0;
    bp->pos.z = (1 - ease (EASE_OUT_BOUNCE, (bp->tick))) * 3;
    break;
  case OPEN:
    bp->lid_angle[(bp->lid_angle[0] == 0.0 ? 1 : 0)] = bp->tick     + 0.0001;
    break;
  case CLOSE:
    bp->lid_angle[(bp->lid_angle[0] == 0.0 ? 1 : 0)] = (1-bp->tick) + 0.0001;
    break;
  case ROLL:
    bp->pos.x += bp->tick * 2;
    break;
  default:
    bp->pos.z = 0;
    break;
  }

  if (wire)
    glColor3f (1, 1, 1);

  glTranslatef (bp->pos.x, bp->pos.z, bp->pos.y);

  mi->polygon_count += draw_component (mi, FRAME_HALF,  True);
  mi->polygon_count += draw_component (mi, PANELS_HALF, True);
  mi->polygon_count += draw_component (mi, INSIDE_HALF, True);
  mi->polygon_count += draw_component (mi, AXLE,        False);
  mi->polygon_count += draw_component (mi, HINGES_HALF, True);

  for (i = 0; i < 2; i++)
    {
      const GLfloat deg = 115;
      const XYZ off = { 0, 0.63, -0.25 };
      double a2 = (bp->state == CLOSE
                   ? 1 - ease (EASE_OUT_BOUNCE, (1 - bp->lid_angle[i]))
                   :     ease (EASE_OUT_BOUNCE, (    bp->lid_angle[i])));

      glPushMatrix();
      glTranslatef (off.x, off.y, off.z);
      glRotatef (-deg * a2, 1, 0, 0);
      glTranslatef (-off.x, -off.y, -off.z);

      if (i == 1)
        glTranslatef (-0.46, 0, 0);

      mi->polygon_count += draw_component (mi, LID, False);
      mi->polygon_count += draw_component (mi, LID_PANELS, False);
      glPopMatrix();
    }

  glPopMatrix();
}


ENTRYPOINT void
draw_dumpster (ModeInfo *mi)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat s;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 1,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
 /* glRotatef (x * 360, 1.0, 0.0, 0.0); */
    glRotatef (y * 360, 0.0, 1.0, 0.0);
 /* glRotatef (z * 360, 0.0, 0.0, 1.0); */
  }

  glRotatef (current_device_rotation(), 0, 0, 1);

  mi->polygon_count = 0;

  glTranslatef (0, -6, 0);
  s = 0.7;
  glScalef (s, s, s);
  glRotatef (5, 1, 0, 0);
  glRotatef (-10, 0, 1, 0);

  draw_box (mi);
  draw_fire (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  tick_dumpster (mi);

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_dumpster (ModeInfo *mi)
{
  dumpster_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  free (bp->particles);
  if (bp->texid) glDeleteTextures (1, &bp->texid);
  for (i = 0; i < countof(all_objs); i++)
    if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
  if (glIsList(bp->sprite_dlist)) glDeleteLists(bp->sprite_dlist, 1);
}

XSCREENSAVER_MODULE_2 ("DumpsterFire", dumpsterfire, dumpster)

#endif /* USE_GL */
