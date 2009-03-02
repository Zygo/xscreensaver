/* Sky Tentacles, Copyright (c) 2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        8           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define refresh_tentacles 0
# define release_tentacles 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPEED       "1.0"
#define DEF_SMOOTH      "True"
#define DEF_SLICES      "32"
#define DEF_SEGMENTS    "32"
#define DEF_WIGGLINESS  "0.35"
#define DEF_FLEXIBILITY "0.35"
#define DEF_THICKNESS   "1.0"
#define DEF_LENGTH      "9.0"
#define DEF_COLOR       "#305A30"
#define DEF_STRIPE      "#451A30"
#define DEF_SUCKER      "#453E30"
#define DEF_DEBUG       "False"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef struct {
  GLfloat length;     /* length of the segment coming out of this segment */
  GLfloat th;         /* vector tilt (on yz plane) from previous segment */
  GLfloat phi;	      /* vector rotation (on xy plane) from previous segment */
  GLfloat thickness;  /* radius of tentacle at this segment */
  rotator *rot;	      /* motion modeller */
} segment;

typedef struct {
  ModeInfo *mi;
  GLfloat x, y, z;	/* position of the base */
  int nsegments;
  segment *segments;
  GLfloat tentacle_color[4], stripe_color[4], sucker_color[4];
} tentacle;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;

  int ntentacles;
  int tentacles_size;
  tentacle **tentacles;
  GLfloat tentacle_color[4], stripe_color[4], sucker_color[4];

  GLuint sucker_list;
  int sucker_polys;

  Bool left_p;
  

} tentacles_configuration;

static tentacles_configuration *tcs = NULL;

static int debug_p;
static GLfloat arg_speed;
static int smooth_p;
static int arg_slices;
static int arg_segments;
static GLfloat arg_thickness;
static GLfloat arg_length;
static GLfloat arg_wiggliness;
static GLfloat arg_flexibility;
static char *arg_color, *arg_stripe, *arg_sucker;

static XrmOptionDescRec opts[] = {
  { "-speed",        ".speed",         XrmoptionSepArg, 0 },
  { "-no-smooth",    ".smooth",        XrmoptionNoArg, "False" },
  { "-slices",       ".slices",        XrmoptionSepArg, 0 },
  { "-segments",     ".segments",      XrmoptionSepArg, 0 },
  { "-thickness",    ".thickness",     XrmoptionSepArg, 0 },
  { "-length",       ".length",        XrmoptionSepArg, 0 },
  { "-wiggliness",   ".wiggliness",    XrmoptionSepArg, 0 },
  { "-flexibility",  ".flexibility",   XrmoptionSepArg, 0 },
  { "-color",        ".tentacleColor", XrmoptionSepArg, 0 },
  { "-stripe-color", ".stripeColor",   XrmoptionSepArg, 0 },
  { "-sucker-color", ".suckerColor",   XrmoptionSepArg, 0 },
  { "-debug",        ".debug",         XrmoptionNoArg, "True" },
};

static argtype vars[] = {
  {&arg_speed,       "speed",         "Speed",       DEF_SPEED,       t_Float},
  {&smooth_p,        "smooth",        "Smooth",      DEF_SMOOTH,      t_Bool},
  {&arg_slices,      "slices",        "Slices",      DEF_SLICES,      t_Int},
  {&arg_segments,    "segments",      "Segments",    DEF_SEGMENTS,    t_Int},
  {&arg_thickness,   "thickness",     "Thickness",   DEF_THICKNESS,   t_Float},
  {&arg_length,      "length",        "Length",      DEF_LENGTH,      t_Float},
  {&arg_wiggliness,  "wiggliness",    "Wiggliness",  DEF_WIGGLINESS,  t_Float},
  {&arg_flexibility, "flexibility",   "Flexibility", DEF_FLEXIBILITY, t_Float},
  {&arg_color,       "tentacleColor", "Color",       DEF_COLOR,       t_String},
  {&arg_stripe,      "stripeColor",   "Color",       DEF_STRIPE,      t_String},
  {&arg_sucker,      "suckerColor",   "Color",       DEF_SUCKER,      t_String},
  {&debug_p,         "debug",         "Debug",       DEF_DEBUG,       t_Bool},
};

ENTRYPOINT ModeSpecOpt tentacles_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_tentacles (ModeInfo *mi, int width, int height)
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

  glClear(GL_COLOR_BUFFER_BIT);
}



static int
unit_torus (double ratio, int slices1, int slices2, Bool wire)
{
  int i, j, k, polys = 0;

  if (wire) slices1 /= 2;
  if (wire) slices2 /= 4;
  if (slices1 < 3) slices1 = 3;
  if (slices2 < 3) slices2 = 3;

  glFrontFace (GL_CW);
  glBegin (wire ? GL_LINE_STRIP : GL_QUAD_STRIP);
  for (i = 0; i < slices1; i++)
    for (j = 0; j <= slices2; j++)
      for (k = 0; k <= 1; k++)
        {
          double s = (i + k) % slices1 + 0.5;
          double t = j % slices2;

          double x = cos(t*M_PI*2/slices2) * cos(s*M_PI*2/slices1);
          double y = sin(t*M_PI*2/slices2) * cos(s*M_PI*2/slices1);
          double z = sin(s*M_PI*2/slices1);
          glNormal3f(x, y, z);

          x = (1 + ratio * cos(s*M_PI*2/slices1)) * cos(t*M_PI*2/slices2) / 2;
          y = (1 + ratio * cos(s*M_PI*2/slices1)) * sin(t*M_PI*2/slices2) / 2;
          z = ratio * sin(s*M_PI*2/slices1) / 2;
          glVertex3f(x, y, z);
          polys++;
        }
  glEnd();
  return polys;
}




/* Initializes a new tentacle and stores it in the list.
 */
static tentacle *
make_tentacle (ModeInfo *mi, int which, int total)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(mi)];
  tentacle *t = (tentacle *) calloc (1, sizeof (*t));
  double brightness;
  int i;

  t->mi = mi;

  /* position tentacles on a grid */
  {
    int cols = (int) (sqrt(total) + 0.5);
    int rows = (total+cols-1) / cols;
    int xx = which % cols;
    int yy = which / cols;
    double spc = arg_thickness * 0.8;
    t->x = (cols * spc / 2) - (spc * (xx + 0.5));
    t->y = (rows * spc / 2) - (spc * (yy + 0.5));
    t->z = 0;
  }

  /* Brighten or darken the colors of this tentacle from the default.
   */
  brightness = 0.6 + frand(3.0);
  memcpy (t->tentacle_color, tc->tentacle_color, 4 * sizeof(*t->tentacle_color));
  memcpy (t->stripe_color,   tc->stripe_color,   4 * sizeof(*t->stripe_color));
  memcpy (t->sucker_color,   tc->sucker_color,   4 * sizeof(*t->sucker_color));
# define FROB(X) \
    t->X[0] *= brightness; if (t->X[0] > 1) t->X[0] = 1; \
    t->X[1] *= brightness; if (t->X[1] > 1) t->X[1] = 1; \
    t->X[2] *= brightness; if (t->X[2] > 1) t->X[2] = 1
  FROB (tentacle_color);
  FROB (stripe_color);
  FROB (sucker_color);
# undef FROB

  t->nsegments = (arg_segments) + BELLRAND(arg_segments);

  t->segments = (segment *) calloc (t->nsegments+1, sizeof(*t->segments));
  for (i = 0; i < t->nsegments; i++)
    {
      double spin_speed   = 0;
      double spin_accel   = 0;
      double wander_speed = arg_speed * (0.02 + BELLRAND(0.1));
      t->segments[i].rot = make_rotator (spin_speed, spin_speed, spin_speed,
                                         spin_accel, wander_speed, True);
    }

  t->segments[0].thickness = (((arg_thickness * 0.5) + 
                               BELLRAND(arg_thickness * 0.6))
                            / 1.0);

  if (tc->tentacles_size <= tc->ntentacles)
    {
      tc->tentacles_size = (tc->tentacles_size * 1.2) + tc->ntentacles;
      tc->tentacles = (tentacle **)
        realloc (tc->tentacles, tc->tentacles_size * sizeof(*tc->tentacles));
      if (! tc->tentacles)
        {
          fprintf (stderr, "%s: out of memory (%d tentacles)\n",
                   progname, tc->tentacles_size);
          exit (1);
        }
    }

  tc->tentacles[tc->ntentacles++] = t;
  return t;
}


static void
draw_tentacle (tentacle *t)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(t->mi)];
  int i;
  Bool wire = MI_IS_WIREFRAME (t->mi);
  XYZ ctr = { 0,0,0 };     /* current position of base of segment */
  double cth  = 0;         /* overall orientation of current segment */
  double cphi = 0;
  double cth_cos  = 1, cth_sin  = 0;
  double cphi_cos = 1, cphi_sin = 0;

  XYZ *ring, *oring;       /* points around the edge (this, and previous) */
  XYZ *norm, *onorm;       /* their normals */

  /* Which portion of the radius the colored stripe takes up */
  int indented_points = arg_slices * 0.2;

  /* We do rotation way to minimize number of calls to sin/cos. */
# define ROT(P) do { \
    XYZ _p = P; \
    _p.y = ((P.y * cth_sin - P.x * cth_cos)); \
    _p.x = ((P.y * cth_cos + P.x * cth_sin) * cphi_sin - (P.z * cphi_cos)); \
    _p.z = ((P.y * cth_cos + P.x * cth_sin) * cphi_cos + (P.z * cphi_sin)); \
    P = _p; \
  } while(0)

  ring  = (XYZ *) malloc (arg_slices * sizeof(*ring));
  norm  = (XYZ *) malloc (arg_slices * sizeof(*norm));
  oring = (XYZ *) malloc (arg_slices * sizeof(*oring));
  onorm = (XYZ *) malloc (arg_slices * sizeof(*onorm));

  if (wire)
    glColor4fv (t->tentacle_color);
  else
    {
      static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
      static const GLfloat bshiny    = 128.0;
      glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
      glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
    }

  glPushMatrix();
  glTranslatef (t->x, t->y, t->z);

  if (debug_p)
    {
      if (!wire) glDisable(GL_LIGHTING);
      glBegin(GL_LINE_LOOP);
      for (i = 0; i < arg_slices; i++)
        glVertex3f (arg_thickness / 2 * cos (M_PI * 2 * i / arg_slices),
                    arg_thickness / 2 * sin (M_PI * 2 * i / arg_slices),
                    0);
      glEnd();
      if (!wire) glEnable(GL_LIGHTING);
    }

  for (i = 0; i < t->nsegments; i++)
    {
      int j;
      XYZ p;

      for (j = 0; j < arg_slices; j++)
        {
          /* Construct a vertical disc at the origin, to use as the
             base of this segment.
           */
          double r = t->segments[i].thickness / 2;
          double a = M_PI * 2 * j / arg_slices;

          if (j <= indented_points/2 || j >= arg_slices-indented_points/2)
            r *= 0.75;

          ring[j].x = r * cos (a);
          ring[j].y = 0;
          ring[j].z = r * sin (a);

          /* Then rotate the points by the angle of the current segment. */
          ROT(ring[j]);

          /* Then move the ring to the base of this segment. */
          ring[j].x += ctr.x;
          ring[j].y += ctr.y;
          ring[j].z += ctr.z;
        }


      /* Compute the normals of the faces on this segment.  We do this
         first so that the normals of the vertexes can be the average
         of the normals of the faces.
         #### Uh, except I didn't actually implement that...
              but it would be a good idea.
       */
      if (i > 0)
        for (j = 0; j <= arg_slices; j++)
          {
            int j0 = j     % arg_slices;
            int j1 = (j+1) % arg_slices;
            norm[j0] = calc_normal (oring[j0], ring[j0], ring[j1]);
          }

      /* Draw!
       */

      if (i > 0)
        {
          int j;
          glFrontFace (GL_CCW);
          glBegin (wire ? GL_LINES : smooth_p ? GL_QUAD_STRIP : GL_QUADS);
          for (j = 0; j <= arg_slices; j++)
            {
              int j0 = j     % arg_slices;
              int j1 = (j+1) % arg_slices;

              if (j <= indented_points/2 || j >= arg_slices-indented_points/2)
                glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
                              t->stripe_color);
              else
                glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
                              t->tentacle_color);

              glNormal3f (onorm[j0].x, onorm[j0].y, onorm[j0].z);
              glVertex3f (oring[j0].x, oring[j0].y, oring[j0].z);
              glNormal3f ( norm[j0].x,  norm[j0].y,  norm[j0].z);
              glVertex3f ( ring[j0].x,  ring[j0].y,  ring[j0].z);
              if (!smooth_p)
                {
                  glVertex3f ( ring[j1].x,  ring[j1].y,  ring[j1].z);
                  glVertex3f (oring[j1].x, oring[j1].y, oring[j1].z);
                }
              t->mi->polygon_count++;
            }
          glEnd ();

          if (wire)
            {
              glBegin (GL_LINE_LOOP);
              for (j = 0; j < arg_slices; j++)
                glVertex3f (ring[j].x, ring[j].y, ring[j].z);
              glEnd();
            }

          /* Now draw the suckers!
           */
          {
            double seg_length = arg_length / t->nsegments;
            double sucker_size = arg_thickness / 8;
            double sucker_spacing = sucker_size * 1.5;
            int nsuckers = seg_length / sucker_spacing;
            double oth  = cth  - t->segments[i-1].th;
            double ophi = cphi - t->segments[i-1].phi;
            int k;

            if (nsuckers == 0)
              {
                int segs_per_sucker =
                  (int) ((sucker_spacing / seg_length) + 0.5);
                nsuckers = (i % segs_per_sucker) ? 0 : 1;
              }

            glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, t->sucker_color);

            for (k = 0; k < nsuckers; k++)
              {
                double scale;
                XYZ p0 = ring[0];
                XYZ p1 = oring[0];
                XYZ p;
                p.x = p0.x + (p1.x - p0.x) * (k + 0.5) / nsuckers;
                p.y = p0.y + (p1.y - p0.y) * (k + 0.5) / nsuckers;
                p.z = p0.z + (p1.z - p0.z) * (k + 0.5) / nsuckers;

                glPushMatrix();
                glTranslatef (p.x, p.y, p.z);
                glRotatef (ophi * 180 / M_PI, 0, 1, 0);
                glRotatef (-oth * 180 / M_PI, 1, 0, 0);
                glRotatef (90, 1, 0, 0);

                { /* Not quite right: this is the slope of the outer edge
                     if the next segment was not tilted at all...  If there
                     is any tilt, then the angle of this wall and the 
                     opposite wall are very different.
                   */
                  double slope = ((t->segments[i-1].thickness -
                                   t->segments[i].thickness) /
                                  t->segments[i].length);
                  glRotatef (-45 * slope, 1, 0, 0);
                }

                scale = t->segments[i].thickness / arg_thickness;
                scale *= 0.7;
                glTranslatef (0, 0, -0.15 * sucker_size);
                glScalef (scale, scale, scale*4);
                {
                  double off = sucker_size * 1.4;
                  glPushMatrix();
                  glTranslatef (off, 0, 0);
                  glCallList (tc->sucker_list);
                  t->mi->polygon_count += tc->sucker_polys;
                  glPopMatrix();

                  glPushMatrix();
                  glTranslatef (-off, 0, 0);
                  glCallList (tc->sucker_list);
                  t->mi->polygon_count += tc->sucker_polys;
                  glPopMatrix();
                }

                glPopMatrix();
              }
          }
        }

      /* Now draw the end caps.
       */
      if (i == 0 || i == t->nsegments-1)
        {
          int j;
          GLfloat ctrz = ctr.z + ((i == 0 ? -1 : 1) *
                                  t->segments[i].thickness / 4);
          glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, t->tentacle_color);
          glFrontFace (i == 0 ? GL_CCW : GL_CW);
          glBegin (wire ? GL_LINES : GL_TRIANGLE_FAN);
          glNormal3f (0, 0, (i == 0 ? -1 : 1));
          glVertex3f (ctr.x, ctr.y, ctrz);
          for (j = 0; j <= arg_slices; j++)
            {
              int jj = j % arg_slices;
              glNormal3f (norm[jj].x, norm[jj].y, norm[jj].z);
              glVertex3f (ring[jj].x, ring[jj].y, ring[jj].z);
              if (wire) glVertex3f (ctr.x, ctr.y, ctrz);
              t->mi->polygon_count++;
            }
          glEnd();
        }

      /* Now move to the end of this segment in preparation for the next.
       */
      if (i != t->nsegments-1)
        {
          p.x = 0;
          p.y = t->segments[i].length;
          p.z = 0;
          ROT (p);
          ctr.x += p.x;
          ctr.y += p.y;
          ctr.z += p.z;

          /* Accumulate the current angle and rotation, to keep track of the
             rotation of the upcoming segment.
           */
          cth  += t->segments[i].th;
          cphi += t->segments[i].phi;

          cth_sin  = sin (cth);
          cth_cos  = cos (cth);
          cphi_sin = sin (cphi);
          cphi_cos = cos (cphi);

          memcpy (oring, ring, arg_slices * sizeof(*ring));
          memcpy (onorm, norm, arg_slices * sizeof(*norm));
        }
    }

  glPopMatrix();

  free (ring);
  free (norm);
  free (oring);
  free (onorm);
}


static void
move_tentacle (tentacle *t)
{
  /* tentacles_configuration *tc = &tcs[MI_SCREEN(t->mi)]; */
  GLfloat len = 0;
  double pos = 0;
  int i, j;
  int skip = t->nsegments * (1 - (arg_wiggliness + 0.5));
  int tick = 0;
  int last = 0;

  for (i = 0; i < t->nsegments; i++)
    {
      if (++tick >= skip || i == t->nsegments-1)
        {
          /* randomize the motion of this segment... */
          double x, y, z;
          double phi_range = M_PI * 0.8 * arg_flexibility;
          double th_range  = M_PI * 0.9 * arg_flexibility;
          get_position (t->segments[i].rot, &x, &y, &z, True);
          t->segments[i].phi = phi_range * (0.5 - y);
          t->segments[i].th  = th_range  * (0.5 - z);
          t->segments[i].length = ((0.8 + ((0.5 - x) * 0.4)) * 
                                   (arg_length / t->nsegments));

          /* ...and make the previous N segments be interpolated
             between this one and the previous randomized one. */
          for (j = last+1; j <= i; j++)
            {
              t->segments[j].phi    = (t->segments[i].phi / (i - last));
              t->segments[j].th     = (t->segments[i].th  / (i - last));
              t->segments[j].length = (t->segments[i].length);
            }

          tick = 0;
          last = i;
        }
      len += t->segments[i].length;
    }

  /* thickness of segment is relative to current position on tentacle
     (not just the index of the segment). */
  for (i = 0; i < t->nsegments; i++)
    {
      if (i > 0)
        {
          double tt = (t->segments[0].thickness * (len - pos) / len);
          if (tt < 0.001) tt = 0.001;
          t->segments[i].thickness = tt;
        }
      pos += t->segments[i].length;
    }
}



ENTRYPOINT Bool
tentacles_handle_event (ModeInfo *mi, XEvent *event)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      tc->button_down_p = True;
      gltrackball_start (tc->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      tc->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (tc->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           tc->button_down_p)
    {
      gltrackball_track (tc->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
parse_color (ModeInfo *mi, const char *name, const char *s, GLfloat *a)
{
  XColor c;
  a[3] = 1.0;  /* alpha */

  if (! XParseColor (MI_DISPLAY(mi), MI_COLORMAP(mi), s, &c))
    {
      fprintf (stderr, "%s: can't parse %s color %s", progname, name, s);
      exit (1);
    }
  a[0] = c.red   / 65536.0;
  a[1] = c.green / 65536.0;
  a[2] = c.blue  / 65536.0;
}


ENTRYPOINT void 
init_tentacles (ModeInfo *mi)
{
  tentacles_configuration *tc;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!tcs) {
    tcs = (tentacles_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (tentacles_configuration));
    if (!tcs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    tc = &tcs[MI_SCREEN(mi)];
  }

  tc = &tcs[MI_SCREEN(mi)];

  tc->glx_context = init_GL(mi);

  reshape_tentacles (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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

  tc->trackball = gltrackball_init ();

  tc->left_p = !(random() % 5);

  if (arg_segments    < 2) arg_segments    = 2;
  if (arg_slices      < 3) arg_slices      = 3;
  if (arg_thickness < 0.1) arg_thickness   = 0.1;
  if (arg_wiggliness  < 0) arg_wiggliness  = 0;
  if (arg_wiggliness  > 1) arg_wiggliness  = 1;
  if (arg_flexibility < 0) arg_flexibility = 0;
  if (arg_flexibility > 1) arg_flexibility = 1;

  parse_color (mi, "tentacleColor", arg_color,  tc->tentacle_color);
  parse_color (mi, "stripeColor",   arg_stripe, tc->stripe_color);
  parse_color (mi, "suckerColor",   arg_sucker, tc->sucker_color);

  for (i = 0; i < MI_COUNT(mi); i++)
    move_tentacle (make_tentacle (mi, i, MI_COUNT(mi)));

  tc->sucker_list = glGenLists (1);
  glNewList (tc->sucker_list, GL_COMPILE);
  { GLfloat s = arg_thickness / 5; glScalef (s, s, s); }
  tc->sucker_polys = unit_torus (0.5, 
                                 MIN(8,  arg_slices/6),
                                 MIN(12, arg_slices/3), 
                                 MI_IS_WIREFRAME(mi));
  glEndList();
}


ENTRYPOINT void
draw_tentacles (ModeInfo *mi)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!tc->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(tc->glx_context));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();


# if 1
  glScalef (3, 3, 3);
# else
  glPushMatrix();
  { GLfloat s = 8.7/1600; glScalef(s,s,s); }
  glTranslatef(-800,-514,0);
  if (!wire) glDisable(GL_LIGHTING);
  glBegin(GL_LINE_LOOP);
  glVertex3f(0,0,0);
  glVertex3f(0,1028,0);
  glVertex3f(1600,1028,0);
  glVertex3f(1600,0,0);
  glEnd();
  if (!wire) glEnable(GL_LIGHTING);
  glPopMatrix();
# endif

  gltrackball_rotate (tc->trackball);

  mi->polygon_count = 0;

  if (debug_p)
    {
      if (!wire) glDisable(GL_LIGHTING);
      glBegin(GL_LINES);
      glVertex3f(-0.5, 0, 0); glVertex3f(0.5, 0, 0);
      glVertex3f(0, -0.5, 0); glVertex3f(0, 0.5, 0);
      glEnd();
      if (!wire) glEnable(GL_LIGHTING);
    }
  else if (tc->left_p)
    {
      glRotatef ( 45, 0, 1, 0);		/* upper left */
      glRotatef ( 45, 1, 0, 0);
      glRotatef (-70, 0, 0, 1);
      glTranslatef (0, -2, -4.5);
    }
  else
    {
      glRotatef (-45, 0, 1, 0);		/* upper right */
      glRotatef ( 45, 1, 0, 0);
      glRotatef ( 70, 0, 0, 1);
      glTranslatef (0, -2, -4.5);
    }

  if (!tc->button_down_p)
    for (i = 0; i < tc->ntentacles; i++)
      move_tentacle (tc->tentacles[i]);

  for (i = 0; i < tc->ntentacles; i++)
    draw_tentacle (tc->tentacles[i]);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("SkyTentacles", skytentacles, tentacles)

#endif /* USE_GL */
