/* squirtorus, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
 * Eat stars, shit rainbows.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000  \n" \
			"*count:        16      \n" \
			"*groundColor:  #FFBE86\n" \
			"*holeColor:    #FF0000\n" \
			"*starColor:    #CCCC00\n" \
			"*showFPS:      False  \n" \
			"*wireframe:    False  \n"

# define release_sq 0

#define DEF_SPEED      "1.0"

#define SPHINCTER_SIZE   0.11
#define SPHINCTER_OPEN   0.45
#define SPHINCTER_FRAMES 100
#define MAX_EJECTA       60
#define EJECTA_SPEED     0.07
#define EJECTA_RATE      6
#define NSTARS           200

#include "xlockmore.h"
#include "gltrackball.h"
#include "colors.h"
#include "hsv.h"
#include "spline.h"
#include "normals.h"

#include <ctype.h>

#ifdef USE_GL /* whole file */

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef struct {
  GLfloat w, y, dy;
  GLfloat color[4];
  int countdown;
} ejecta;

typedef struct {
  enum { IDLE, OPENING, OPEN, CLOSING } state;
  double ratio;
  int nejecta, ejected, finished;
  ejecta ejecta[MAX_EJECTA];
  GLfloat size, x, y;
} sphincter;

typedef struct {
  GLfloat x, y, dx, dy, th, dth;
} star;
 

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  GLfloat dx, dy;
  int nsphincters;
  sphincter *sphincters;
  GLfloat ground_color[4], hole_color[4];
  int ncolors;
  XColor *colors;

  GLuint ground_dlist;
  int ground_npolys;

  GLuint star_dlist;
  int star_npolys;
  star stars[NSTARS];
  GLfloat star_color[4];

  GLuint sphincter_dlist0;
  int sphincter_npolys;

  GLuint ejecta_dlist0;
  int ejecta_npolys;
  int torus_polys;
  int torus_step;
  XYZ *torus_points;
  XYZ *torus_normals;

} sq_configuration;

static sq_configuration *bps = NULL;

static GLfloat speed;

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",      XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,      "speed",      "Speed",      DEF_SPEED,      t_Float},
};

ENTRYPOINT ModeSpecOpt sq_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


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


/* Honestly I don't know why I wrote this thing. It came to me in a dream.
   It's been a weird pandemic, ok? */
static void
new_sphincter (ModeInfo *mi, sphincter *s, Bool early_p)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  double ss;
  double depth = 0.5;
  int i;
  s->state = IDLE;
  s->ratio = 0;
  s->nejecta = BELLRAND (MAX_EJECTA);
  s->ejected = 0;
  s->finished = s->nejecta;
  s->size = SPHINCTER_SIZE * (0.8 + frand(0.4));
  ss = s->size * 1.8;

  memset (s->ejecta, 0, sizeof(*s->ejecta));
  for (i = 0; i < s->nejecta; i++)
    {
      ejecta *e = &s->ejecta[i];
      e->y = -1;  /* idle */
    }

  /* Place it randomly but try not to overlap an existing one. */
  for (i = 0; i < 1000; i++)
    {
      int j;
      Bool ok = True;
      s->x =  frand (1.0) - 0.5;
      s->y = (early_p && i == 0
              ? frand (0.5) + 0.25
              : -frand (depth));

      for (j = 0; j < bp->nsphincters; j++)
        {
          sphincter *s2 = &bp->sphincters[j];
          double dx = s2->x - s->x;
          double dy = s2->y - s->y;
          double d2 = dx*dx + dy*dy;
          if (s != s2 && d2 <= ss*ss)
            {
              ok = False;
              break;
            }
        }
      if (ok) break;
      depth += 0.1;  /* If we're having trouble placing, go farther back */
    }

}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.35;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


static void
compute_unit_torus (ModeInfo *mi, double ratio, int slices1, int slices2)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME (mi);
  int i, j, k, fp;

  if (wire) slices1 /= 2;
  if (wire) slices2 /= 4;
  if (slices1 < 3) slices1 = 3;
  if (slices2 < 3) slices2 = 3;

  bp->torus_polys = slices1 * (slices2+1) * 2;
  bp->torus_points  = (XYZ *) calloc (bp->torus_polys + 1,
                                      sizeof (*bp->torus_points));
  bp->torus_normals = (XYZ *) calloc (bp->torus_polys + 1,
                                      sizeof (*bp->torus_normals));
  bp->torus_step = 2 * (slices2+1);
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
          bp->torus_normals[fp] = p;

          p.x = (1 + ratio * cos(s*M_PI*2/slices1)) * cos(t*M_PI*2/slices2) / 2;
          p.y = (1 + ratio * cos(s*M_PI*2/slices1)) * sin(t*M_PI*2/slices2) / 2;
          p.z = ratio * sin(s*M_PI*2/slices1) / 2;
          bp->torus_points[fp] = p;
          fp++;
        }
  if (fp != bp->torus_polys) abort();
  bp->torus_polys = fp;
}


static int
draw_torus (ModeInfo *mi)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME (mi);
  int i, j;
  int strips = bp->torus_polys / bp->torus_step;
  int points = 0;

  glFrontFace (GL_CW);
  for (i = 0; i < strips; i++)
    {
      int ii = i * bp->torus_step;

      glBegin (wire ? GL_LINE_STRIP : GL_QUAD_STRIP);
      for (j = 0; j < bp->torus_step; j++)
        {
          XYZ sp = bp->torus_points[ii+j];
          XYZ sn = bp->torus_normals[ii+j];
          glNormal3f(sn.x, sn.y, sn.z);
          glVertex3f(sp.x, sp.y, sp.z);
          points++;
        }
      glEnd();
    }
  return points;
}


/* Usul, we have wormsign the likes of which even God has never seen. */
static void
draw_ejecta (ModeInfo *mi, ejecta *e)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  double sc, r;
  int frame;

  if (e->y <= 0) return;

  r = 0.5 + e->dy * 8.0;	/* torus thickness */
  if (r > 1) r = 1;
  if (r < 0) r = 0;
  if (r <= 0) return;

  frame = r * (SPHINCTER_FRAMES - 1);

  sc = e->w * 2;		/* torus width */
  sc *= 1 - e->dy * 6;

  glPushMatrix();
  glTranslatef (0, 0, e->y);
  glScalef (sc, sc, sc);

  glColor4fv (e->color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, e->color);
  glCallList (bp->ejecta_dlist0 + frame);
  mi->polygon_count += bp->ejecta_npolys;

  glPopMatrix();
}


static spline *
make_sphincter_profile (int pixels)
{
  spline *s = make_spline (pixels);
  s->n_controls = 0;

# define PT(x,y) \
    s->control_x[s->n_controls] = pixels * (x); \
    s->control_y[s->n_controls] = pixels * (y); \
    s->n_controls++
  PT (  0.00, -2.00);
  PT (  0.03, -0.25);
  PT (  0.04,  0.00);
  PT (  0.05,  0.20);
  PT (  0.30,  0.20);
  PT (  0.50,  0.05);
  PT (  0.80,  0.00);
  PT (  1.00,  0.00);
# undef PT

  compute_spline (s);

# if 0
  {
    int i;
    fprintf (stderr, "open 'http://www.graphreader.com/plotter?x=");
    for (i = 0; i < s->n_points; i++)
      fprintf (stderr, "%s%d", (i == 0 ? "" : ","), s->points[i].x);
    fprintf (stderr, "&y=");
    for (i = 0; i < s->n_points; i++)
      fprintf (stderr, "%s%d", (i == 0 ? "" : ","), s->points[i].y);
    fprintf (stderr, "'\n");
    exit (1);
  }
# endif

  return s;
}


/* Just spit on your hand and see what's gurgling around in there. */
static int
render_sphincter (ModeInfo *mi, spline *sp, int rez, int frame)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat step = (wire ? 0.1 : 0.05);
  int strips = (M_PI * 2) / step;
  int quads  = sp->n_points;
  XYZ *pts = (XYZ *) calloc (strips * quads, sizeof(*pts));
  XYZ *ns  = (XYZ *) calloc (strips * quads, sizeof(*ns));
  GLfloat colors[SPHINCTER_FRAMES+1][4];
  int polys = 0;
  int i, j;

  GLfloat sc = frame / (GLfloat) SPHINCTER_FRAMES;
  sc = ease_ratio (sc);
  sc *= SPHINCTER_OPEN;

  /* Compute each vertex */
  for (i = 0; i < strips; i++)
    {
      GLfloat th = (i / (GLfloat) strips) * M_PI * 2;
      GLfloat x0 = cos (th);
      GLfloat y0 = sin (th);
      GLfloat ripple = 1 + (0.07 * sin (th * 13));

      for (j = 0; j < quads; j++)
        {
          GLfloat r0 = (sp->points[j].x / (GLfloat) rez) * (1-sc) + sc;
          GLfloat z0 = (sp->points[j].y / (GLfloat) rez);
          XYZ *p = &pts[i * quads + j];
          p->x = x0 * r0;
          p->y = y0 * r0;
          p->z = z0 * (j < quads-4 ? ripple : 1);
        }
    }

  /* Compute each face normal */
  for (i = 0; i < strips; i++)
    {
      int i2 = (i + 1) % strips;
      for (j = 0; j < quads - 1; j++)
        {
          XYZ *p0 = &pts[i * quads + j];
          XYZ *p1 = &pts[i * quads + j + 1];
          XYZ *p2 = &pts[i2 * quads + j];
       /* XYZ *p3 = &pts[i2 * quads + j + 1]; */
          XYZ *n0 = &ns[i * quads + j];
          *n0 = calc_normal (*p0, *p1, *p2);
        }
      ns[i * quads + j].z = 1;
    }

  /* Colors of radii */
  for (i = 0; i < quads; i++)
    {
      GLfloat cr = i / (GLfloat) (quads-1);  /* Color ratio */
      GLfloat ir = cr * 1.5;                 /* Intensity ratio */
      cr += 0.3;
      if (cr > 1) cr = 1;
      if (ir > 1) ir = 1;
      ir *= ir;
# define CC(J) \
      colors[i][J] = ir * (bp->ground_color[J] + \
                           ((1 - cr) * \
                            (bp->hole_color[J] - bp->ground_color[J])))
      CC(0);
      CC(1);
      CC(2);
      colors[i][3] = 1;
# undef CC
    }

  /* Render quads */
  glPushMatrix();
  glFrontFace (GL_CCW);
  if (!wire) glBegin (GL_QUADS);
  for (i = 0; i < strips; i++)
    {
      int i2 = (i + 1) % strips;
      for (j = 0; j < quads - 1; j++)
        {
          XYZ *p0 = &pts[i  * quads + j];
          XYZ *p1 = &pts[i  * quads + j + 1];
          XYZ *p2 = &pts[i2 * quads + j];
          XYZ *p3 = &pts[i2 * quads + j + 1];

          /* Vertex normal is the normal of its following face.  It should
             really be the average of the neighboring face normals, but this
             is close enough. */
          XYZ *n0 = &ns[i  * quads + j];
          XYZ *n1 = &ns[i  * quads + j + 1];
          XYZ *n2 = &ns[i2 * quads + j];
          XYZ *n3 = &ns[i2 * quads + j + 1];

          if (j < 3) continue;  /* We can skip the deep hole quads */

          if (wire) glBegin (GL_LINE_LOOP);

          glColor4fv (colors[j]);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, colors[j]);
          glNormal3f (n0->x, n0->y, n0->z);
          glVertex3f (p0->x, p0->y, p0->z);

          glColor4fv (colors[j+1]);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, colors[j+1]);
          glNormal3f (n1->x, n1->y, n1->z);
          glVertex3f (p1->x, p1->y, p1->z);

          glColor4fv (colors[j+1]);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, colors[j+1]);
          glNormal3f (n3->x, n3->y, n3->z);
          glVertex3f (p3->x, p3->y, p3->z);

          glColor4fv (colors[j]);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, colors[j]);
          glNormal3f (n2->x, n2->y, n2->z);
          glVertex3f (p2->x, p2->y, p2->z);

          if (wire) glEnd();
          polys++;
        }
    }
  if (!wire) glEnd();

  /* Mask out the hole so that the ground doesn't cover it. */
  if (! wire)
    {
      /* Fill depth buffer only, don't render */
      glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glFrontFace (GL_CCW);
      glNormal3f (0, 0, -1);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
      glVertex3f (0, 0, 0);
      for (i = 0; i <= strips; i++)
        {
          GLfloat th = (i / (GLfloat) strips) * M_PI * 2;
          glVertex3f (cos(th), sin(th), 0);
          polys++;
        }
      glEnd();
      glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
  glPopMatrix();

  free (pts);
  free (ns);

  return polys;
}


static void
move_sphincters (ModeInfo *mi)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;

  for (i = 0; i < bp->nsphincters; i++)
    {
      sphincter *s = &bp->sphincters[i];
      s->x += bp->dx;
      s->y += bp->dy;
      if (s->y > 1)
        new_sphincter (mi, s, False);

      switch (s->state) {
      case IDLE:
        if (s->y > 0 &&
            s->finished >= s->nejecta &&
            ! (random() % 2000))
          {
            s->state = OPENING;
            s->ratio = 0;
          }
        break;
      case OPENING:
        {
          double w = 0.9 - frand(0.1);
          s->ratio += 0.01 * speed;
          if (s->ratio > 1)
            {
              s->ratio = 0;
              s->state = OPEN;
              s->ejected = 0;
              s->finished = 0;
              for (j = 0; j < s->nejecta; j++)
                {
                  ejecta *e = &s->ejecta[j];
                  if (e->y < 0)  /* if it's idle, enqueue it */
                    {
                      int c = j * (double) bp->ncolors / s->nejecta;
                      e->w = w;
                      e->y = 0;
                      e->dy = EJECTA_SPEED;
                      e->countdown = (j + 1) * EJECTA_RATE;

                      e->color[0] = bp->colors[c].red   / 65536.0;
                      e->color[1] = bp->colors[c].green / 65536.0;
                      e->color[2] = bp->colors[c].blue  / 65536.0;
                      e->color[3] = 1;
                    }
                }
            }
          break;
        }
      case OPEN:
        {
          if (s->ejected >= s->nejecta)
            {
              s->ratio = 1;
              s->state = CLOSING;
            }
          break;
        }
      case CLOSING:
        {
          s->ratio -= 0.03 * speed;
          if (s->ratio < 0)
            {
              s->ratio = 0;
              s->state = IDLE;
            }
          break;
        }
      default: abort();
      }

      for (j = 0; j < s->nejecta; j++)
        {
          ejecta *e = &s->ejecta[j];
          if (e->y < 0) continue; /* idle */
          if (e->countdown > 0) e->countdown--;
          if (e->countdown == 0 && e->y == 0)
            {
              e->countdown = 0;
              e->y = 0.001;
              s->ejected++;
            }
          else if (e->countdown == 0)
            {
              e->y += e->dy;
              e->dy -= 0.0008;
              if (e->y < 0)
                s->finished++;
            }
        }
    }

  for (i = 0; i < NSTARS; i++)
    {
      star *s = &bp->stars[i];
      s->th += s->dth;
      s->x += s->dx;
      s->y += s->dy;
      if (s->dy != 0)
        s->dy -= 0.00001;
      if (s->dx == 0 &&
          s->dy == 0 &&
          ! (random() % 8000))
        {
          s->dx = frand(0.0004) - 0.0002;
          s->dy = -frand(0.0004);
        }
    }

}


ENTRYPOINT void
reshape_sq (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

# if 0
  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }
# endif

  glViewport (0, y, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (40, 1/h, 10, 10000);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 30,
             0, 0, 0,
             0, 1, 0);

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
sq_handle_event (ModeInfo *mi, XEvent *event)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  return False;
}


static void
new_colors (ModeInfo *mi)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat h1, h2, h3;
  if (bp->colors) free (bp->colors);
  bp->ncolors = 128;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
# if 0
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);
# else
  h1 = frand(360);
  h2 = h1 + 60 + frand(90);
  h3 = h2 + 60 + frand(90);
  make_color_loop(0, 0, 0,
                  h1, 0.5 + frand(0.5), 0.8 + frand(0.2),
                  h2, 0.5 + frand(0.5), 0.8 + frand(0.2),
                  h3, 0.5 + frand(0.5), 0.8 + frand(0.2),
                  bp->colors, &bp->ncolors,
                  False, False);
# endif
}


ENTRYPOINT void 
init_sq (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  sq_configuration *bp;
  spline *sp;
  int spline_rez = 250;
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];
  bp->glx_context = init_GL(mi);

  reshape_sq (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel (GL_SMOOTH);
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);

  bp->trackball = gltrackball_init (False);
  new_colors (mi);

  bp->dx = 0;
  bp->dy = 0.0003 * speed;
  bp->nsphincters = MI_COUNT(mi);
  if (bp->nsphincters <= 0) bp->nsphincters = 1;
  bp->sphincters = (sphincter *) calloc (bp->nsphincters, sizeof (sphincter));

  for (i = 0; i < bp->nsphincters; i++)
    {
      sphincter *s = &bp->sphincters[i];
      new_sphincter (mi, s, True);
    }

  parse_color (mi, "groundColor", bp->ground_color);
  parse_color (mi, "holeColor", bp->hole_color);
  parse_color (mi, "starColor", bp->star_color);

  bp->ejecta_dlist0 = glGenLists (SPHINCTER_FRAMES);
  for (i = 0; i < SPHINCTER_FRAMES; i++)
    {
      glNewList (bp->ejecta_dlist0 + i, GL_COMPILE);
      compute_unit_torus (mi, 0.1 * (i / (double) SPHINCTER_FRAMES),
                          20, 60);
      bp->ejecta_npolys = draw_torus (mi);
      glEndList ();
    }

  sp = make_sphincter_profile (spline_rez);
  bp->sphincter_dlist0 = glGenLists (SPHINCTER_FRAMES);
  for (i = 0; i < SPHINCTER_FRAMES; i++)
    {
      glNewList (bp->sphincter_dlist0 + i, GL_COMPILE);
      bp->sphincter_npolys = render_sphincter (mi, sp, spline_rez, i);
      glEndList ();
    }
  free_spline (sp);

  bp->ground_dlist = glGenLists (1);
  glNewList (bp->ground_dlist, GL_COMPILE);
  {
    GLfloat step = 0.01;
    GLfloat x, y;
    GLfloat z = 0.001;
    GLfloat inc = 0.02;
    GLfloat black[4] = { 0, 0, 0, 1 };

    glNormal3f (0, 0, -1);
    glFrontFace (GL_CCW);
    glBegin (wire ? GL_LINES : GL_QUADS);
    /* When using fog, iOS apparently doesn't like lines or quads that are
     really long, and extend very far outside of the scene. */
    for (y = -0.5; y < 0.5 + step; y += step)
      for (x = -0.5; x < 0.5; x += step)
        {
          glVertex3f (x,      y,      z);
          glVertex3f (x,      y+step, z);
          glVertex3f (x+step, y+step, z);
          glVertex3f (x+step, y,      z);
          bp->ground_npolys++;
        }
    glEnd();

    /* Mountains */
    step = 0.02;
    y = -0.5;
    glBegin (wire ? GL_LINE_STRIP : GL_QUAD_STRIP);
    glNormal3f (0, 1, 0);
    glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, black);
    for (x = -0.5; x < 0.5 + step; x += step)
      {
        glVertex3f (x, y, -inc);
        if (! wire)
          glVertex3f (x, y, z);
        inc += 0.015 * (frand(1.0) - 0.5);
        if (inc < z) inc = z;
        bp->ground_npolys++;
      }
    glEnd();
  }
  glEndList ();

  bp->star_dlist = glGenLists (1);
  glNewList (bp->star_dlist, GL_COMPILE);
  {
    GLfloat th;
    int i;
    GLfloat step = M_PI *2 / 10;
    glNormal3f (0, 0, -1);
    glFrontFace (GL_CCW);
    glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
    if (!wire) glVertex3f (0, 0, 0);
    for (th = 0, i = 0; th < M_PI*2 + step; th += step, i++)
      glVertex3f (cos (th) * (i & 1 ? 0.4 : 1),
                  sin (th) * (i & 1 ? 0.4 : 1),
                  0);
    bp->star_npolys += 10;
    glEnd();
  }
  glEndList ();

  for (i = 0; i < NSTARS; i++)
    {
      star *s = &bp->stars[i];
      s->x = frand (1.0) - 0.5;
      s->y = frand (0.35) + 0.15;
      s->th = frand (M_PI);
      s->dth = 0.05 * frand(0.05) * ((random() & 1) ? 1 : -1);
    }
}


ENTRYPOINT void
draw_sq (ModeInfo *mi)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i, which;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glShadeModel (GL_SMOOTH);
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);
  glDisable (GL_BLEND);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 0.2, 0.2, 1.0};
      GLfloat fog_color[4] = { 0, 0, 0, 1 };

      static const GLfloat bspec[4] = {1.0, 1.0, 1.0, 1.0};
      static const GLfloat bshiny    = 128.0;

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glMaterialfv (GL_FRONT, GL_SPECULAR,  bspec);
      glMateriali  (GL_FRONT, GL_SHININESS, bshiny);


      glFogi (GL_FOG_MODE, GL_LINEAR);
      glFogfv (GL_FOG_COLOR, fog_color);
      glFogf (GL_FOG_DENSITY, 0.0065);
      glFogf (GL_FOG_START, 130);
      glFogf (GL_FOG_END,   240);
      glEnable (GL_FOG);

      glClearColor (0.05, 0.05, 0.06, 1);
    }

  glPushMatrix ();

  glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */

  mi->polygon_count = 0;

  glScalef (200, 200, 200);
  glRotatef (90, 1, 0, 0);
  glTranslatef (0, -0.6, 0);
  glRotatef (20, 1, 0, 0);

  glScalef (-1, 1, 1);
  gltrackball_rotate (bp->trackball);
  glScalef (-1, 1, 1);

#if 0
  glScalef(0.2, 0.2, 0.2);
#endif


  /* Draw the ejecta first so that they aren't masked by the ground. */
  for (which = 0; which <= 1; which++)
    for (i = 0; i < bp->nsphincters; i++)
      {
        sphincter *s = &bp->sphincters[i];

        if (s->y < 0) continue;

        glPushMatrix();
        glTranslatef (s->x, s->y - 0.5, 0);
        glRotatef (180, 0, 1, 0);
        glScalef (s->size, s->size, s->size);

        if (which == 1)		/* sphincter */
          {
            GLfloat sc = (s->state == IDLE    ? 0 :
                          s->state == OPENING ? s->ratio :
                          s->state == CLOSING ? s->ratio :
                          1);
            int frame = sc * (SPHINCTER_FRAMES - 1);

            if (s->state == OPEN &&
                !bp->button_down_p &&
                s->ejected < s->nejecta * 0.95)
              glScalef (1 + frand(0.03),  /* Brrrrrrrtttt */
                        1 + frand(0.03),
                        1 + frand(0.2));
            glCallList (bp->sphincter_dlist0 + frame);
          }
        else			/* ejecta */
          {
            int j;
            glScalef (0.7, 0.7, 0.7);  /* SPHINCTER_OPEN? */
            glTranslatef (0, 0, -0.3);
            for (j = 0; j < s->nejecta; j++)
              draw_ejecta (mi, &s->ejecta[j]);
          }

        glPopMatrix();
        mi->polygon_count += bp->sphincter_npolys;
      }

  /* Draw ground (must be after drawing the sphincters, for hole masking) */
  glColor4fv (bp->ground_color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->ground_color);
  glPushMatrix();
  glScalef (2, 1, 1);
  glCallList (bp->ground_dlist);
  glPopMatrix();

  glPopMatrix ();


  /* Stars */
  glDisable (GL_LIGHTING);
  glMatrixMode (GL_PROJECTION);
  glPushMatrix ();
  {
    glLoadIdentity ();

    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    {
      GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
      GLfloat sc = 0.004;
      glLoadIdentity ();
      glOrtho (-0.5,  0.5,
               -0.5, 0.5,
               -100.0, 100.0);
      glColor4fv (bp->star_color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->star_color);
      for (i = 0; i < NSTARS; i++)
        {
          star *s = &bp->stars[i];
          if (s->y < -0.2) continue;
          glPushMatrix();
          glTranslatef (s->x, s->y, -95);
          glScalef (sc, sc / h, sc);
          glRotatef (s->th / (M_PI / 180.0), 0, 0, 1);
          glCallList (bp->star_dlist);
          glPopMatrix();
        }
    }
    glPopMatrix ();
  }
  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();
  glMatrixMode (GL_MODELVIEW);

  if (! bp->button_down_p)
    move_sphincters (mi);

  if (! (random() % 300))
    new_colors (mi);

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_sq (ModeInfo *mi)
{
  sq_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (glIsList(bp->ground_dlist))
    glDeleteLists(bp->ground_dlist, 1);
  if (glIsList(bp->star_dlist))
    glDeleteLists(bp->star_dlist, 1);
  if (glIsList(bp->ejecta_dlist0))
    glDeleteLists(bp->ejecta_dlist0, SPHINCTER_FRAMES);
  if (glIsList(bp->sphincter_dlist0))
    glDeleteLists(bp->sphincter_dlist0, SPHINCTER_FRAMES);

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->sphincters) free (bp->sphincters);
  if (bp->colors) free (bp->colors);
  if (bp->torus_points) free (bp->torus_points);
  if (bp->torus_normals) free (bp->torus_normals);
}

XSCREENSAVER_MODULE_2 ("Squirtorus", squirtorus, sq)

#endif /* USE_GL */
