/* gears, Copyright (c) 2007-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Originally written by Brian Paul in 1996 or earlier;
 * rewritten by jwz in Nov 2007.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        0           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define refresh_gears 0
# define release_gears 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "involute.h"
#include "normals.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  Bool planetary_p;

  int ngears;
  gear **gears;

  GLuint armature_dlist;
  int armature_polygons;

  struct { GLfloat x1, y1, x2, y2; } bbox;

} gears_configuration;

static gears_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True"  },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0      },
  { "-wander", ".wander", XrmoptionNoArg, "True"  },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt gears_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_gears (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
free_gear (gear *g)
{
  if (g->dlist)
    glDeleteLists (g->dlist, 1);
  free (g);
}


/* Create and return a new gear sized for placement next to or on top of
   the given parent gear (if any.)  Returns 0 if out of memory.
   [Mostly lifted from pinion.c]
 */
static gear *
new_gear (ModeInfo *mi, gear *parent)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];
  gear *g = (gear *) calloc (1, sizeof (*g));
  static unsigned long id = 0;  /* only used in debugging output */

  if (!g) return 0;
  g->id = ++id;

  /* Pick the size of the teeth.
   */
  if (parent) /* adjascent gears need matching teeth */
    {
      g->tooth_w = parent->tooth_w;
      g->tooth_h = parent->tooth_h;
      g->tooth_slope = -parent->tooth_slope;
    }
  else                 /* gears that begin trains get any size they want */
    {
      g->tooth_w = 0.007 * (1.0 + BELLRAND(4.0));
      g->tooth_h = 0.005 * (1.0 + BELLRAND(8.0));
/*
      g->tooth_slope = ((random() % 8)
                        ? 0
                        : 0.5 + BELLRAND(1));
 */
    }

  /* Pick the number of teeth, and thus, the radius.
   */
  {
    double c;

    if (!parent || bp->ngears > 4)
      g->nteeth = 5 + BELLRAND (20);
    else
      g->nteeth = parent->nteeth * (0.5 + BELLRAND(2));

    c = g->nteeth * g->tooth_w * 2;     /* circumference = teeth + gaps */
    g->r = c / (M_PI * 2);              /* c = 2 pi r  */
  }

  g->thickness  = g->tooth_w + frand (g->r);
  g->thickness2 = g->thickness * 0.7;
  g->thickness3 = g->thickness;

  /* Colorize
   */
  g->color[0] = 0.5 + frand(0.5);
  g->color[1] = 0.5 + frand(0.5);
  g->color[2] = 0.5 + frand(0.5);
  g->color[3] = 1.0;

  g->color2[0] = g->color[0] * 0.85;
  g->color2[1] = g->color[1] * 0.85;
  g->color2[2] = g->color[2] * 0.85;
  g->color2[3] = g->color[3];


  /* Decide on shape of gear interior:
     - just a ring with teeth;
     - that, plus a thinner in-set "plate" in the middle;
     - that, plus a thin raised "lip" on the inner plate;
     - or, a wide lip (really, a thicker third inner plate.)
   */
  if ((random() % 10) == 0)
    {
      /* inner_r can go all the way in; there's no inset disc. */
      g->inner_r = (g->r * 0.1) + frand((g->r - g->tooth_h/2) * 0.8);
      g->inner_r2 = 0;
      g->inner_r3 = 0;
    }
  else
    {
      /* inner_r doesn't go in very far; inner_r2 is an inset disc. */
      g->inner_r  = (g->r * 0.5)  + frand((g->r - g->tooth_h) * 0.4);
      g->inner_r2 = (g->r * 0.1) + frand(g->inner_r * 0.5);
      g->inner_r3 = 0;

      if (g->inner_r2 > (g->r * 0.2))
        {
          int nn = (random() % 10);
          if (nn <= 2)
            g->inner_r3 = (g->r * 0.1) + frand(g->inner_r2 * 0.2);
          else if (nn <= 7 && g->inner_r2 >= 0.1)
            g->inner_r3 = g->inner_r2 - 0.01;
        }
    }

  /* If we have three discs, sometimes make the middle disc be spokes.
   */
  if (g->inner_r3 && ((random() % 5) == 0))
    {
      g->spokes = 2 + BELLRAND (5);
      g->spoke_thickness = 1 + frand(7.0);
      if (g->spokes == 2 && g->spoke_thickness < 2)
        g->spoke_thickness += 1;
    }

  /* Sometimes add little nubbly bits, if there is room.
   */
  if (g->nteeth > 5)
    {
      double size = 0;
      involute_biggest_ring (g, 0, &size, 0);
      if (size > g->r * 0.2 && (random() % 5) == 0)
        {
          g->nubs = 1 + (random() % 16);
          if (g->nubs > 8) g->nubs = 1;
        }
    }

  if (g->inner_r3 > g->inner_r2) abort();
  if (g->inner_r2 > g->inner_r) abort();
  if (g->inner_r  > g->r) abort();

  /* Decide how complex the polygon model should be.
   */
  {
    double pix = g->tooth_h * MI_HEIGHT(mi); /* approx. tooth size in pixels */
    if (pix <= 2.5)      g->size = INVOLUTE_SMALL;
    else if (pix <= 3.5) g->size = INVOLUTE_MEDIUM;
    else if (pix <= 25)  g->size = INVOLUTE_LARGE;
    else                 g->size = INVOLUTE_HUGE;
  }

  g->base_p = !parent;

  return g;
}


/* Given a newly-created gear, place it next to its parent in the scene,
   with its teeth meshed and the proper velocity.  Returns False if it
   didn't work.  (Call this a bunch of times until either it works, or
   you decide it's probably not going to.)
   [Mostly lifted from pinion.c]
 */
static Bool
place_gear (ModeInfo *mi, gear *g, gear *parent)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];

  /* Compute this gear's velocity.
   */
  if (! parent)
    {
      g->ratio = 0.8 + BELLRAND(0.4);  /* 0.8-1.2 = 8-12rpm @ 60fps */
      g->th = 1; /* not 0 */
    }
  else
    {
      /* Gearing ratio is the ratio of the number of teeth to previous gear
         (which is also the ratio of the circumferences.)
       */
      g->ratio = (double) parent->nteeth / (double) g->nteeth;

      /* Set our initial rotation to match that of the previous gear,
         multiplied by the gearing ratio.  (This is finessed later,
         once we know the exact position of the gear relative to its
         parent.)
      */
      g->th = -(parent->th * g->ratio);

      if (g->nteeth & 1)    /* rotate 1/2 tooth-size if odd number of teeth */
        {
          double off = (180.0 / g->nteeth);
          if (g->th > 0)
            g->th += off;
          else
            g->th -= off;
        }

      /* ratios are cumulative for all gears in the train. */
      g->ratio *= parent->ratio;
    }


  if (parent)	/* Place the gear next to the parent. */
    {
      double r_off = parent->r + g->r;
      int angle;

      angle = (random() % 360) - 180;   /* -180 to +180 degrees */

      g->x = parent->x + (cos ((double) angle * (M_PI / 180)) * r_off);
      g->y = parent->y + (sin ((double) angle * (M_PI / 180)) * r_off);
      g->z = parent->z;

      /* avoid accidentally changing sign of "th" in the math below. */
      g->th += (g->th > 0 ? 360 : -360);

      /* Adjust the rotation of the gear so that its teeth line up with its
         parent, based on the position of the gear and the current rotation
         of the parent.
       */
      {
        double p_c = 2 * M_PI * parent->r;  /* circumference of parent */
        double g_c = 2 * M_PI * g->r;       /* circumference of g  */

        double p_t = p_c * (angle/360.0);   /* distance travelled along
                                               circumference of parent when
                                               moving "angle" degrees along
                                               parent. */
        double g_rat = p_t / g_c;           /* if travelling that distance
                                               along circumference of g,
                                               ratio of g's circumference
                                               travelled. */
        double g_th = 360.0 * g_rat;        /* that ratio in degrees */

        g->th += angle + g_th;
      }
    }

  /* If the position we picked for this gear causes it to overlap
     with any earlier gear in the train, give up.
   */
  {
    int i;

    for (i = bp->ngears-1; i >= 0; i--)
      {
        gear *og = bp->gears[i];

        if (og == g) continue;
        if (og == parent) continue;
        if (g->z != og->z) continue;    /* Ignore unless on same layer */

        /* Collision detection without sqrt:
             d = sqrt(a^2 + b^2)   d^2 = a^2 + b^2
             d < r1 + r2           d^2 < (r1 + r2)^2
         */
        if (((g->x - og->x) * (g->x - og->x) +
             (g->y - og->y) * (g->y - og->y)) <
            ((g->r + g->tooth_h + og->r + og->tooth_h) *
             (g->r + g->tooth_h + og->r + og->tooth_h)))
          return False;
      }
  }

  return True;
}


/* Make a new gear, place it next to its parent in the scene,
   with its teeth meshed and the proper velocity.  Returns the gear;
   or 0 if it didn't work.  (Call this a bunch of times until either
   it works, or you decide it's probably not going to.)
   [Mostly lifted from pinion.c]
 */
static gear *
place_new_gear (ModeInfo *mi, gear *parent)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];
  int loop_count = 0;
  gear *g = 0;

  while (1)
    {
      loop_count++;
      if (loop_count >= 100)
        {
          if (g)
            free_gear (g);
          g = 0;
          break;
        }

      g = new_gear (mi, parent);
      if (!g) return 0;  /* out of memory? */

      if (place_gear (mi, g, parent))
        break;
    }

  if (! g) return 0;

  /* We got a gear, and it is properly positioned.
     Insert it in the scene.
   */
  bp->gears[bp->ngears++] = g;
  return g;
}


static int
arm (GLfloat length,
     GLfloat width1, GLfloat height1,
     GLfloat width2, GLfloat height2,
     Bool wire)
{
  int polys = 0;
  glShadeModel(GL_FLAT);

#if 0  /* don't need these - they're embedded in other objects */
  /* draw end 1 */
  glFrontFace(GL_CW);
  glNormal3f(-1, 0, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2,  height1/2);
  glVertex3f(-length/2, -width1/2,  height1/2);
  polys++;
  glEnd();

  /* draw end 2 */
  glFrontFace(GL_CCW);
  glNormal3f(1, 0, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(length/2, -width2/2, -height2/2);
  glVertex3f(length/2,  width2/2, -height2/2);
  glVertex3f(length/2,  width2/2,  height2/2);
  glVertex3f(length/2, -width2/2,  height2/2);
  polys++;
  glEnd();
#endif

  /* draw top */
  glFrontFace(GL_CCW);
  glNormal3f(0, 0, -1);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2, -height1/2);
  glVertex3f( length/2,  width2/2, -height2/2);
  glVertex3f( length/2, -width2/2, -height2/2);
  polys++;
  glEnd();

  /* draw bottom */
  glFrontFace(GL_CW);
  glNormal3f(0, 0, 1);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, height1/2);
  glVertex3f(-length/2,  width1/2, height1/2);
  glVertex3f( length/2,  width2/2, height2/2);
  glVertex3f( length/2, -width2/2, height2/2);
  polys++;
  glEnd();

  /* draw left */
  glFrontFace(GL_CW);
  glNormal3f(0, -1, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, -height1/2);
  glVertex3f(-length/2, -width1/2,  height1/2);
  glVertex3f( length/2, -width2/2,  height2/2);
  glVertex3f( length/2, -width2/2, -height2/2);
  polys++;
  glEnd();

  /* draw right */
  glFrontFace(GL_CCW);
  glNormal3f(0, 1, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2,  width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2,  height1/2);
  glVertex3f( length/2,  width2/2,  height2/2);
  glVertex3f( length/2,  width2/2, -height2/2);
  polys++;
  glEnd();

  glFrontFace(GL_CCW);

  return polys;
}


static int
ctube (GLfloat diameter, GLfloat width, Bool wire)
{
  tube (0, 0,  width/2,
        0, 0, -width/2,
        diameter, 0, 
        32, True, True, wire);
  return 0; /* #### */
}

static void
armature (ModeInfo *mi)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  static const GLfloat spec[4] = {1.0, 1.0, 1.0, 1.0};
  GLfloat shiny = 128.0;
  GLfloat color[4];

  color[0] = 0.5 + frand(0.5);
  color[1] = 0.5 + frand(0.5);
  color[2] = 0.5 + frand(0.5);
  color[3] = 1.0;

  bp->armature_polygons = 0;

  bp->armature_dlist = glGenLists (1);
  if (! bp->armature_dlist)
    {
      check_gl_error ("glGenLists");
      abort();
    }

  glNewList (bp->armature_dlist, GL_COMPILE);

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
  glColor3f (color[0], color[1], color[2]);

  glPushMatrix();

  {
    GLfloat s = bp->gears[0]->r * 2.7;
    s = s/5.6;
    glScalef (s, s, s);
  }

  glTranslatef (0, 0, 1.4 + bp->gears[0]->thickness);
  glRotatef (30, 0, 0, 1);

  bp->armature_polygons += ctube (0.5, 10, wire);       /* center axle */

  glPushMatrix();
  glTranslatef(0.0, 4.2, -1);
  bp->armature_polygons += ctube (0.5, 3, wire);       /* axle 1 */
  glTranslatef(0, 0, 1.8);
  bp->armature_polygons += ctube (0.7, 0.7, wire);
  glPopMatrix();

  glPushMatrix();
  glRotatef(120, 0.0, 0.0, 1.0);
  glTranslatef(0.0, 4.2, -1);
  bp->armature_polygons += ctube (0.5, 3, wire);       /* axle 2 */
  glTranslatef(0, 0, 1.8);
  bp->armature_polygons += ctube (0.7, 0.7, wire);
  glPopMatrix();

  glPushMatrix();
  glRotatef(240, 0.0, 0.0, 1.0);
  glTranslatef(0.0, 4.2, -1);
  bp->armature_polygons += ctube (0.5, 3, wire);       /* axle 3 */
  glTranslatef(0, 0, 1.8);
  bp->armature_polygons += ctube (0.7, 0.7, wire);
  glPopMatrix();

  glTranslatef(0, 0, 1.5);			      /* center disk */
  bp->armature_polygons += ctube (1.5, 2, wire);

  glPushMatrix();
  glRotatef(270, 0, 0, 1);
  glRotatef(-10, 0, 1, 0);
  glTranslatef(-2.2, 0, 0);
  bp->armature_polygons += arm (4.0, 1.0, 0.5,
                                2.0, 1.0, wire);	/* arm 1 */
  glPopMatrix();

  glPushMatrix();
  glRotatef(30, 0, 0, 1);
  glRotatef(-10, 0, 1, 0);
  glTranslatef(-2.2, 0, 0);
  bp->armature_polygons += arm (4.0, 1.0, 0.5,
                                2.0, 1.0, wire);	/* arm 2 */
  glPopMatrix();

  glPushMatrix();
  glRotatef(150, 0, 0, 1);
  glRotatef(-10, 0, 1, 0);
  glTranslatef(-2.2, 0, 0);
  bp->armature_polygons += arm (4.0, 1.0, 0.5,
                                2.0, 1.0, wire);	/* arm 3 */
  glPopMatrix();

  glPopMatrix();

  glEndList ();
}


static void
planetary_gears (ModeInfo *mi)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];
  gear *g0, *g1, *g2, *g3, *g4;
  GLfloat distance = 2.02;

  bp->planetary_p = True;

  g0 = new_gear (mi, 0);
  g1 = new_gear (mi, 0);
  g2 = new_gear (mi, 0);
  g3 = new_gear (mi, 0);
  g4 = new_gear (mi, 0);

  if (! place_gear (mi, g0, 0)) abort();
  if (! place_gear (mi, g1, 0)) abort();
  if (! place_gear (mi, g2, 0)) abort();
  if (! place_gear (mi, g3, 0)) abort();
  if (! place_gear (mi, g4, 0)) abort();

  g0->nteeth = 12 + (3 * (random() % 10));  /* must be multiple of 3 */
  g0->tooth_w = g0->r / g0->nteeth;
  g0->tooth_h = g0->tooth_w * 2.8;

# define COPY(F) g4->F = g3->F = g2->F = g1->F = g0->F
  COPY(r);
  COPY(th);
  COPY(nteeth);
  COPY(tooth_w);
  COPY(tooth_h);
  COPY(tooth_slope);
  COPY(inner_r);
  COPY(inner_r2);
  COPY(inner_r3);
  COPY(thickness);
  COPY(thickness2);
  COPY(thickness3);
  COPY(ratio);
  COPY(size);
# undef COPY

  g1->x = cos (M_PI * 2 / 3) * g1->r * distance;
  g1->y = sin (M_PI * 2 / 3) * g1->r * distance;

  g2->x = cos (M_PI * 4 / 3) * g2->r * distance;
  g2->y = sin (M_PI * 4 / 3) * g2->r * distance;

  g3->x = cos (M_PI * 6 / 3) * g3->r * distance;
  g3->y = sin (M_PI * 6 / 3) * g3->r * distance;

  g4->x = 0;
  g4->y = 0;
  g4->th = -g3->th;

  /* rotate central gear 1/2 tooth-size if odd number of teeth */
  if (g4->nteeth & 1)
    g4->th -= (180.0 / g4->nteeth);

  g0->inverted_p  = True;
  g0->x           = 0;
  g0->y           = 0;
  g0->nteeth      = g1->nteeth * 3;
  g0->r           = g1->r * 3.05;
  g0->inner_r     = g0->r * 0.8;
  g0->inner_r2    = 0;
  g0->inner_r3    = 0;
  g0->th          = g1->th + (180 / g0->nteeth);
  g0->ratio       = g1->ratio / 3;

  g0->tooth_slope = 0;
  g0->nubs        = 3;
  g0->spokes      = 0;
  g0->size        = INVOLUTE_LARGE;

  bp->gears = (gear **) calloc (6, sizeof(**bp->gears));
  bp->ngears = 0;

  bp->gears[bp->ngears++] = g1;
  bp->gears[bp->ngears++] = g2;
  bp->gears[bp->ngears++] = g3;
  bp->gears[bp->ngears++] = g4;
  bp->gears[bp->ngears++] = g0;
}




ENTRYPOINT void 
init_gears (ModeInfo *mi)
{
  gears_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps, NULL);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_gears (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  if (! bp->rot)
    {
      double spin_speed   = 0.5;
      double wander_speed = 0.01;
      double spin_accel   = 0.25;

      bp->rot = make_rotator (do_spin ? spin_speed : 0,
                              do_spin ? spin_speed : 0,
                              do_spin ? spin_speed : 0,
                              spin_accel,
                              do_wander ? wander_speed : 0,
                              True
                              );
      bp->trackball = gltrackball_init (True);
    }

  if (bp->gears)
    {
      for (i = 0; i < bp->ngears; i++)
        free_gear (bp->gears[i]);
      free (bp->gears);
      bp->gears = 0;
      bp->ngears = 0;
    }

  if (!(random() % 8))
    {
      planetary_gears (mi);
    }
  else
    {
      gear *g = 0;
      int total_gears = MI_COUNT (mi);

      bp->planetary_p = False;

      if (total_gears <= 0)
        total_gears = 3 + fabs (BELLRAND (8) - 4);  /* 3 - 7, mostly 3. */
      bp->gears = (gear **) calloc (total_gears+2, sizeof(**bp->gears));
      bp->ngears = 0;

      for (i = 0; i < total_gears; i++)
        g = place_new_gear (mi, g);
    }


  /* Center gears in scene. */
  {
    GLfloat minx=99999, miny=99999, maxx=-99999, maxy=-99999;
    int i;
    for (i = 0; i < bp->ngears; i++)
      {
        gear *g = bp->gears[i];
        if (g->x - g->r < minx) minx = g->x - g->r;
        if (g->x + g->r > maxx) maxx = g->x + g->r;
        if (g->y - g->r < miny) miny = g->y - g->r;
        if (g->y + g->r > maxy) maxy = g->y + g->r;
      }
    bp->bbox.x1 = minx;
    bp->bbox.y1 = miny;
    bp->bbox.x2 = maxx;
    bp->bbox.y2 = maxy;
  }

  /* Now render each gear into its display list.
   */
  for (i = 0; i < bp->ngears; i++)
    {
      gear *g = bp->gears[i];
      g->dlist = glGenLists (1);
      if (! g->dlist)
        {
          check_gl_error ("glGenLists");
          abort();
        }

      glNewList (g->dlist, GL_COMPILE);
      g->polygons += draw_involute_gear (g, wire);
      glEndList ();
    }
  if (bp->planetary_p)
    armature (mi);
}


ENTRYPOINT void
draw_gears (ModeInfo *mi)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef ((x - 0.5) * 4,
                  (y - 0.5) * 4,
                  (z - 0.5) * 7);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);

    /* add a little rotation for -no-spin mode */
    x -= 0.14;
    y -= 0.06;

    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  /* Center the scene's bounding box in the window,
     and scale it to fit. 
   */
  {
    GLfloat w = bp->bbox.x2 - bp->bbox.x1;
    GLfloat h = bp->bbox.y2 - bp->bbox.y1;
    GLfloat s = 10.0 / (w > h ? w : h);
    glScalef (s, s, s);
    glTranslatef (-(bp->bbox.x1 + w/2),
                  -(bp->bbox.y1 + h/2),
                  0);
  }

  mi->polygon_count = 0;

  for (i = 0; i < bp->ngears; i++)
    {
      gear *g = bp->gears[i];

      glPushMatrix();

      glTranslatef (g->x, g->y, g->z);
      glRotatef (g->th, 0, 0, 1);

      glCallList (g->dlist);
      mi->polygon_count += g->polygons;

      glPopMatrix ();
    }

  if (bp->planetary_p)
    {
      glCallList (bp->armature_dlist);
      mi->polygon_count += bp->armature_polygons;
    }

  glPopMatrix ();

  /* spin gears */
  if (!bp->button_down_p)
    for (i = 0; i < bp->ngears; i++)
      {
        gear *g = bp->gears[i];
        double off = g->ratio * 5 * speed;
        if (g->th > 0)
          g->th += off;
        else
          g->th -= off;
      }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

ENTRYPOINT Bool
gears_handle_event (ModeInfo *mi, XEvent *event)
{
  gears_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      init_gears (mi);
      return True;
    }

  return False;
}

XSCREENSAVER_MODULE ("Gears", gears)

#endif /* USE_GL */
