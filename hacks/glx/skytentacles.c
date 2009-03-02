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
			"*count:        9           \n" \
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

#include "xpm-ximage.h"
#include "../images/scales.xpm"

static char *grey_texture[] = {
  "16 1 3 1",
  "X c #808080",
  "x c #C0C0C0",
  ". c #FFFFFF",
  "XXXxxxxx........"
};

#ifdef USE_GL /* whole file */

#define DEF_SPEED       "1.0"
#define DEF_SMOOTH      "True"
#define DEF_TEXTURE     "True"
#define DEF_CEL         "False"
#define DEF_INTERSECT   "False"
#define DEF_SLICES      "16"
#define DEF_SEGMENTS    "24"
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
  GLfloat thickness;  /* radius of tentacle at the bottom of this segment */
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

  int torus_polys;
  int torus_step;
  XYZ *torus_points;
  XYZ *torus_normals;

  GLfloat line_thickness;
  GLfloat outline_color[4];
  XImage *texture;
  GLuint texid;

  Bool left_p;
  

} tentacles_configuration;

static tentacles_configuration *tcs = NULL;

static int debug_p;
static GLfloat arg_speed;
static int smooth_p;
static int texture_p;
static int cel_p;
static int intersect_p;
static int arg_slices;
static int arg_segments;
static GLfloat arg_thickness;
static GLfloat arg_length;
static GLfloat arg_wiggliness;
static GLfloat arg_flexibility;
static char *arg_color, *arg_stripe, *arg_sucker;

/* we can only have one light when doing cel shading */
static GLfloat light_pos[4] = {1.0, 1.0, 1.0, 0.0};


static XrmOptionDescRec opts[] = {
  { "-speed",        ".speed",         XrmoptionSepArg, 0 },
  { "-no-smooth",    ".smooth",        XrmoptionNoArg, "False" },
  { "-texture",      ".texture",       XrmoptionNoArg, "True" },
  { "-no-texture",   ".texture",       XrmoptionNoArg, "False" },
  { "-cel",          ".cel",           XrmoptionNoArg, "True" },
  { "-no-cel",       ".cel",           XrmoptionNoArg, "False" },
  { "-intersect",    ".intersect",     XrmoptionNoArg, "True" },
  { "-no-intersect", ".intersect",     XrmoptionNoArg, "False" },
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
  {&texture_p,       "texture",       "Texture",     DEF_TEXTURE,     t_Bool},
  {&cel_p,           "cel",           "Cel",         DEF_CEL,         t_Bool},
  {&intersect_p,     "intersect",     "Intersect",   DEF_INTERSECT,   t_Bool},
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
  tentacles_configuration *tc = &tcs[MI_SCREEN(mi)];
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

  tc->line_thickness = (MI_IS_WIREFRAME (mi) ? 1 : MAX (3, width / 200));
}


static void
normalize (GLfloat *x, GLfloat *y, GLfloat *z)
{
  GLfloat d = sqrt((*x)*(*x) + (*y)*(*y) + (*z)*(*z));
  *x /= d;
  *y /= d;
  *z /= d;
}

static GLfloat
dot (GLfloat x0, GLfloat y0, GLfloat z0,
     GLfloat x1, GLfloat y1, GLfloat z1)
{
  return x0*x1 + y0*y1 + z0*z1;
}


static void
compute_unit_torus (ModeInfo *mi, double ratio, int slices1, int slices2)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME (mi);
  int i, j, k, fp;

  if (wire) slices1 /= 2;
  if (wire) slices2 /= 4;
  if (slices1 < 3) slices1 = 3;
  if (slices2 < 3) slices2 = 3;

  tc->torus_polys = slices1 * (slices2+1) * 2;
  tc->torus_points  = (XYZ *) calloc (tc->torus_polys + 1,
                                      sizeof (*tc->torus_points));
  tc->torus_normals = (XYZ *) calloc (tc->torus_polys + 1,
                                      sizeof (*tc->torus_normals));
  tc->torus_step = 2 * (slices2+1);
  fp = 0;
  for (i = 0; i < slices1; i++)
    for (j = 0; j <= slices2; j++)
      for (k = 0; k <= 1; k++)
        {
          double s = (i + k) % slices1 + 0.5;
          double t = j % slices2;
          XYZ p;
          p.x = cos(t*M_PI*2/slices2) * cos(s*M_PI*2/slices1);
          p.y = sin(t*M_PI*2/slices2) * cos(s*M_PI*2/slices1);
          p.z = sin(s*M_PI*2/slices1);
          tc->torus_normals[fp] = p;

          p.x = (1 + ratio * cos(s*M_PI*2/slices1)) * cos(t*M_PI*2/slices2) / 2;
          p.y = (1 + ratio * cos(s*M_PI*2/slices1)) * sin(t*M_PI*2/slices2) / 2;
          p.z = ratio * sin(s*M_PI*2/slices1) / 2;
          tc->torus_points[fp] = p;
          fp++;
        }
  if (fp != tc->torus_polys) abort();
  tc->torus_polys = fp;
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
    if (!intersect_p) cols = 1, xx = 0;
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
      tc->tentacles_size = (tc->tentacles_size * 1.2) + tc->ntentacles + 2;
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
draw_sucker (tentacle *t, Bool front_p)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(t->mi)];
  Bool wire = MI_IS_WIREFRAME (t->mi);
  int i, j;
  int strips = tc->torus_polys / tc->torus_step;
  int points = 0;

  glFrontFace (front_p ? GL_CW : GL_CCW);
  for (i = 0; i < strips; i++)
    {
      int ii = i * tc->torus_step;

      /* Leave off the polygons on the underside.  This reduces polygon
         count by about 10% with the default settings. */
      if (strips > 4 && i >= strips/2 && i < strips-1)
        continue;

      glBegin (wire ? GL_LINE_STRIP : GL_QUAD_STRIP);
      for (j = 0; j < tc->torus_step; j++)
        {
          XYZ sp = tc->torus_points[ii+j];
          XYZ sn = tc->torus_normals[ii+j];
          glNormal3f(sn.x, sn.y, sn.z);
          glVertex3f(sp.x, sp.y, sp.z);
          points++;
        }
      glEnd();
    }
  t->mi->polygon_count += points/2;
}


static void
draw_tentacle (tentacle *t, Bool front_p)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(t->mi)];
  int i;
  Bool wire = MI_IS_WIREFRAME (t->mi);
  XYZ ctr = { 0,0,0 };     /* current position of base of segment */
  double cth  = 0;         /* overall orientation of current segment */
  double cphi = 0;
  double cth_cos  = 1, cth_sin  = 0;
  double cphi_cos = 1, cphi_sin = 0;

  GLfloat light[3];	   /* vector to the light */

  GLfloat t0 = 0.0;        /* texture coordinate */

  XYZ *ring, *oring;       /* points around the edge (this, and previous) */
  XYZ *norm, *onorm;       /* their normals */
  XYZ *ucirc;		   /* unit circle, to save some trig */

  /* Which portion of the radius the indented/colored stripe takes up */
  int indented_points = arg_slices * 0.2;

  /* We do rotation this way to minimize the number of calls to sin/cos.
     We have to hack the transformations manually instead of using
     glRotate/glTranslate because those calls are not allowed *inside*
     of a glBegin/glEnd block...
   */
# define ROT(P) do {                                                    \
    XYZ _p = P;                                                         \
    _p.y = ((P.y * cth_sin - P.x * cth_cos));                           \
    _p.x = ((P.y * cth_cos + P.x * cth_sin) * cphi_sin - (P.z * cphi_cos)); \
    _p.z = ((P.y * cth_cos + P.x * cth_sin) * cphi_cos + (P.z * cphi_sin)); \
    P = _p;                                                             \
  } while(0)

  ring  = (XYZ *) malloc (arg_slices * sizeof(*ring));
  norm  = (XYZ *) malloc (arg_slices * sizeof(*norm));
  oring = (XYZ *) malloc (arg_slices * sizeof(*oring));
  onorm = (XYZ *) malloc (arg_slices * sizeof(*onorm));
  ucirc = (XYZ *) malloc (arg_slices * sizeof(*ucirc));

  light[0] = light_pos[0];
  light[1] = light_pos[1];
  light[2] = light_pos[2];
  normalize (&light[0], &light[1], &light[2]);

  for (i = 0; i < arg_slices; i++)
    {
      double a = M_PI * 2 * i / arg_slices;
      ucirc[i].x = cos(a);
      ucirc[i].y = sin(a);
      ucirc[i].z = 0;
    }


  if (cel_p)
    glPolygonMode (GL_FRONT_AND_BACK, (front_p ? GL_FILL : GL_LINE));

  glPushMatrix();
  glTranslatef (t->x, t->y, t->z);

  if (debug_p)
    {
      glPushAttrib (GL_ENABLE_BIT);
      glDisable (GL_LIGHTING);
      glDisable (GL_TEXTURE_1D);
      glDisable (GL_TEXTURE_2D);
      glColor3f (1, 1, 1);
      glLineWidth (1);
      glBegin(GL_LINE_LOOP);
      for (i = 0; i < arg_slices; i++)
        glVertex3f (arg_thickness / 2 * cos (M_PI * 2 * i / arg_slices),
                    arg_thickness / 2 * sin (M_PI * 2 * i / arg_slices),
                    0);
      glEnd();
      glPopAttrib();
    }

  if (!front_p)
    glColor4fv (tc->outline_color);
  else if (wire)
    glColor4fv (t->tentacle_color);
  else
    {
      static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
      static const GLfloat bshiny    = 128.0;
      glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
      glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
    }

  for (i = 0; i < t->nsegments; i++)
    {
      int j;
      GLfloat t1 = t0 + i / (t->nsegments * M_PI * 2);

      for (j = 0; j < arg_slices; j++)
        {
          /* Construct a vertical disc at the origin, to use as the
             base of this segment.
          */
          double r = t->segments[i].thickness / 2;

          if (j <= indented_points/2 || j >= arg_slices-indented_points/2)
            r *= 0.75;  /* indent the stripe */

          ring[j].x = r * ucirc[j].x;
          ring[j].y = 0;
          ring[j].z = r * ucirc[j].y;

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
          glLineWidth (tc->line_thickness);
          glFrontFace (front_p ? GL_CCW : GL_CW);
          glBegin (wire ? GL_LINES : smooth_p ? GL_QUAD_STRIP : GL_QUADS);
          for (j = 0; j <= arg_slices; j++)
            {
              int j0 = j     % arg_slices;
              int j1 = (j+1) % arg_slices;

              GLfloat ts = j / (double) arg_slices;

              if (!front_p)
                glColor4fv (tc->outline_color);
              else if (j <= indented_points/2 || 
                       j >= arg_slices-indented_points/2)
                {
                  glColor4fv (t->stripe_color);
                  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
                                t->stripe_color);
                }
              else
                {
                  glColor4fv (t->tentacle_color);
                  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
                                t->tentacle_color);
                }

              /* For cel shading, the 1d texture coordinate (s) is the 
                 dot product of the lighting vector and the vertex normal.
              */
              if (cel_p)
                {
                  t0 = dot (light[0], light[1], light[2],
                            onorm[j0].x, onorm[j0].y, onorm[j0].z);
                  t1 = dot (light[0], light[1], light[2],
                            norm[j0].x, norm[j0].y, norm[j0].z);
                  if (t0 < 0) t0 = 0;
                  if (t1 < 0) t1 = 0;
                }

              glTexCoord2f (t0, ts);
              glNormal3f (onorm[j0].x, onorm[j0].y, onorm[j0].z);
              glVertex3f (oring[j0].x, oring[j0].y, oring[j0].z);

              glTexCoord2f (t1, ts);
              glNormal3f ( norm[j0].x,  norm[j0].y,  norm[j0].z);
              glVertex3f ( ring[j0].x,  ring[j0].y,  ring[j0].z);

              if (!smooth_p)
                {
                  ts = j1 / (double) arg_slices;
                  glTexCoord2f (t1, ts);
                  glVertex3f ( ring[j1].x,  ring[j1].y,  ring[j1].z);
                  glTexCoord2f (t0, ts);
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
            double sucker_size = arg_thickness / 5;
            double sucker_spacing = sucker_size * 1.3;
            int nsuckers = seg_length / sucker_spacing;
            double oth  = cth  - t->segments[i-1].th;
            double ophi = cphi - t->segments[i-1].phi;
            int k;

            if (!wire)
              glLineWidth (MAX (2, tc->line_thickness / 2.0));
            glDisable (GL_TEXTURE_2D);

            /* Sometimes we have N suckers on one segment; 
               sometimes we have one sucker every N segments. */
            if (nsuckers == 0)
              {
                int segs_per_sucker =
                  (int) ((sucker_spacing / seg_length) + 0.5);
                nsuckers = (i % segs_per_sucker) ? 0 : 1;
              }

            if (front_p)
              {
                glColor4fv (t->sucker_color);
                glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, 
                              t->sucker_color);
              }

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
                scale *= 0.7 * sucker_size;
                glScalef (scale, scale, scale * 4);

                glTranslatef (0, 0, -0.1);  /* embed */

                glTranslatef (1, 0, 0);     /* left */
                draw_sucker (t, front_p);

                glTranslatef (-2, 0, 0);    /* right */
                draw_sucker (t, front_p);

                glPopMatrix();
              }

            if (texture_p) glEnable (GL_TEXTURE_2D);
          }
        }

      /* Now draw the end caps.
       */
      glLineWidth (tc->line_thickness);
      if (i == 0 || i == t->nsegments-1)
        {
          int j;
          GLfloat ctrz = ctr.z + ((i == 0 ? -1 : 1) *
                                  t->segments[i].thickness / 4);
          if (front_p)
            {
              glColor4fv (t->tentacle_color);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, 
                            t->tentacle_color);
            }
          glFrontFace ((front_p ? i == 0 : i != 0) ? GL_CCW : GL_CW);
          glBegin (wire ? GL_LINES : GL_TRIANGLE_FAN);
          glNormal3f (0, 0, (i == 0 ? -1 : 1));
          glTexCoord2f (t0 - 0.25, 0.5);
          glVertex3f (ctr.x, ctr.y, ctrz);
          for (j = 0; j <= arg_slices; j++)
            {
              int jj = j % arg_slices;
              GLfloat ts = j / (double) arg_slices;
              glTexCoord2f (t0, ts);
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
          XYZ p;
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

      t0 = t1;
    }

  glPopMatrix();

  free (ring);
  free (norm);
  free (oring);
  free (onorm);
  free (ucirc);
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
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ')
        {
          gltrackball_reset (tc->trackball);
          return True;
        }
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
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};
      glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  if (!wire && !cel_p)
    {
      glEnable (GL_LIGHTING);
      glEnable (GL_LIGHT0);
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

  /* Black outlines for light colors, white outlines for dark colors. */
  if (tc->tentacle_color[0] + tc->tentacle_color[1] + tc->tentacle_color[2]
      < 0.4)
    tc->outline_color[0] = 1;
  tc->outline_color[1] = tc->outline_color[0];
  tc->outline_color[2] = tc->outline_color[0];
  tc->outline_color[3] = 1;

  for (i = 0; i < MI_COUNT(mi); i++)
    move_tentacle (make_tentacle (mi, i, MI_COUNT(mi)));

  if (wire) texture_p = cel_p = False;
  if (cel_p) texture_p = False;

  if (texture_p || cel_p) {
    glGenTextures(1, &tc->texid);
# ifdef HAVE_GLBINDTEXTURE
    glBindTexture ((cel_p ? GL_TEXTURE_1D : GL_TEXTURE_2D), tc->texid);
# endif

    tc->texture = xpm_to_ximage (MI_DISPLAY(mi), MI_VISUAL(mi), 
                                 MI_COLORMAP(mi), 
                                 (cel_p ? grey_texture : scales));
    if (!tc->texture) texture_p = cel_p = False;
  }

  if (texture_p) {
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    clear_gl_error();
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                  tc->texture->width, tc->texture->height, 0,
                  GL_RGBA,
                  /* GL_UNSIGNED_BYTE, */
                  GL_UNSIGNED_INT_8_8_8_8_REV,
                  tc->texture->data);
    check_gl_error("texture");

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_2D);
  } else if (cel_p) {
    clear_gl_error();
    glTexImage1D (GL_TEXTURE_1D, 0, GL_RGBA,
                  tc->texture->width, 0,
                  GL_RGBA,
                  /* GL_UNSIGNED_BYTE, */
                  GL_UNSIGNED_INT_8_8_8_8_REV,
                  tc->texture->data);
    check_gl_error("texture");

    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_1D);
    glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable (GL_LINE_SMOOTH);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    glEnable (GL_BLEND);

    /* Dark gray instead of black, so the outlines show up */
    glClearColor (0.08, 0.08, 0.08, 1.0);
  }

  compute_unit_torus (mi, 0.5, 
                      MAX(5, arg_slices/6),
                      MAX(9, arg_slices/3));
}


ENTRYPOINT void
draw_tentacles (ModeInfo *mi)
{
  tentacles_configuration *tc = &tcs[MI_SCREEN(mi)];
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
  glPushAttrib (GL_ENABLE_BIT);
  glPushMatrix();
  { GLfloat s = 8.7/1600; glScalef(s,s,s); }
  glTranslatef(-800,-514,0);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_1D);
  glDisable(GL_TEXTURE_2D);
  glColor3f (1, 1, 1);
  glBegin(GL_LINE_LOOP);
  glVertex3f(0,0,0);
  glVertex3f(0,1028,0);
  glVertex3f(1600,1028,0);
  glVertex3f(1600,0,0);
  glEnd();
  glPopMatrix();
  glPopAttrib();
# endif

  gltrackball_rotate (tc->trackball);

  mi->polygon_count = 0;

  if (debug_p)
    {
      glPushAttrib (GL_ENABLE_BIT);
      glDisable (GL_LIGHTING);
      glDisable (GL_TEXTURE_1D);
      glDisable (GL_TEXTURE_2D);
      glColor3f (1, 1, 1);
      glLineWidth (1);
      glBegin(GL_LINES);
      glVertex3f(-0.5, 0, 0); glVertex3f(0.5, 0, 0);
      glVertex3f(0, -0.5, 0); glVertex3f(0, 0.5, 0);
      glEnd();
      glPopAttrib();
    }
  else
    {
      GLfloat rx =  45;
      GLfloat ry = -45;
      GLfloat rz =  70;
      if (tc->left_p)
        ry = -ry, rz = -rz;
      glRotatef (ry, 0, 1, 0);
      glRotatef (rx, 1, 0, 0);
      glRotatef (rz, 0, 0, 1);
      if (intersect_p)
        glTranslatef (0, -2.0, -4.5);
      else
        glTranslatef (0, -2.5, -5.0);
    }

  if (!tc->button_down_p)
    for (i = 0; i < tc->ntentacles; i++)
      move_tentacle (tc->tentacles[i]);

#if 1
  for (i = 0; i < tc->ntentacles; i++)
    {
      if (! intersect_p)
        glClear(GL_DEPTH_BUFFER_BIT);
      draw_tentacle (tc->tentacles[i], True);
      if (cel_p)
        draw_tentacle (tc->tentacles[i], False);
    }
#else
  glScalef (3, 3, 3);
  glScalef (1, 1, 4);
  glColor3f(1,1,1);
  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  draw_sucker (tc->tentacles[0], True);
  if (cel_p)
    {
      glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
      glLineWidth (tc->line_thickness);
      glColor4fv (tc->outline_color);
      draw_sucker (tc->tentacles[0], False);
    }
#endif

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("SkyTentacles", skytentacles, tentacles)

#endif /* USE_GL */
