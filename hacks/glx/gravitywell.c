/* gravitywell, Copyright (c) 2019 Jamie Zawinski <jwz@jwz.org>
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
			"*count:        15     \n" \
			"*gridColor:    #00FF00\n" \
			"*gridColor2:   #FF0000\n" \
			"*showFPS:      False  \n" \
			"*wireframe:    False  \n"

# define release_gw 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEF_SPEED      "1.0"
#define DEF_RESOLUTION "1.0"
#define DEF_GRID_SIZE  "1.0"

#include "xlockmore.h"
#include "gltrackball.h"
#include "colors.h"
#include "hsv.h"

#include <ctype.h>

#define ASSERT(x)

#ifdef USE_GL /* whole file */

typedef struct {
  GLfloat mass;
  GLfloat ro2, rm2, ri2; /* outer/middle/inner */
  GLfloat ro, radius;
  GLfloat x, y, dx, dy;
  GLfloat surface_gravity, depth;
} star;

typedef struct {
  GLXContext *glx_context;
  trackball_state *user_trackball;
  Bool button_down_p;
  int nstars;
  star *stars;
  int grid_w, grid_h;
  GLfloat *grid;
  char *segs;
  GLfloat *vtx, *col;
  GLfloat color[4];
  int ncolors;
  XColor *colors;
} gw_configuration;

static gw_configuration *bps = NULL;

static GLfloat speed, resolution, grid_size;

#define RESOLUTION_BASE 512
#define GRID_SIZE_BASE  7
#define SPEED_BASE      2.5
#define MASS_EPSILON    0.03
#define SLOPE_EPSILON   0.06
#define GRID_SEG        16u /* Power-of-two here is faster. */
#define MAX_MASS_COLOR  120

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",      XrmoptionSepArg, 0 },
  { "-resolution", ".resolution", XrmoptionSepArg, 0 },
  { "-grid-size",  ".gridSize",   XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,      "speed",      "Speed",      DEF_SPEED,      t_Float},
  {&resolution, "resolution", "Resolution", DEF_RESOLUTION, t_Float},
  {&grid_size,  "gridSize",   "GridSize",   DEF_GRID_SIZE,  t_Float},
};

ENTRYPOINT ModeSpecOpt gw_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define WCLIP(x,hi) MIN(MAX((int)(x),0),(hi))

/* Window management, etc
 */
ENTRYPOINT void
reshape_gw (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (40, 1/h, 10, 1000);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 30,
             0, 0, 0,
             0, 1, 0);

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
gw_handle_event (ModeInfo *mi, XEvent *event)
{
  gw_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->user_trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
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


static void
new_star (const gw_configuration *bp, star *s)
{
  int w = bp->grid_w * GRID_SEG;

  s->radius = 2 * (2 + frand(3) + frand(3) + frand(3));
  s->mass = s->radius * 150 * (2 + frand(3) + frand(3) + frand(3));

  s->ro2 = s->mass / MASS_EPSILON;
  s->ro = sqrt (s->ro2);
  s->rm2 = pow (s->mass * (2.0f / SLOPE_EPSILON), 2.0f / 3.0f);
  s->ri2 = s->radius * s->radius;
  if (s->rm2 < s->ri2)
    s->rm2 = s->ri2;
  if (s->ro2 < s->rm2)
    s->ro2 = s->rm2;

  s->x = w * (s == bp->stars ? 0.5 : (0.35 + frand(0.3)));
  s->dx = ((frand(1.0) - 0.5) * 0.1) / resolution;
  s->dy = (0.1 + frand(0.6)) / resolution;

  /* What the experienced gravitation would be at the surface of the
     star, were the mass actually held in a singularity at its center.
   */
  s->surface_gravity = s->mass / s->ri2;
  s->depth = s->surface_gravity;
}


static void
move_stars (ModeInfo *mi)
{
  gw_configuration *bp = &bps[MI_SCREEN(mi)];
  int w = bp->grid_w * GRID_SEG;
  int h = bp->grid_h * GRID_SEG;
  int i;

  for (i = 0; i < bp->nstars; i++)
    {
      star *s = &bp->stars[i];
      /* Move stars off screen until most of their influence fades */
      GLfloat off = speed * SPEED_BASE * resolution;
      s->x += s->dx * off;
      s->y += s->dy * off;

      if (s->x < -s->ro ||
          s->y < -s->ro ||
          s->x >= w + s->ro ||
          s->y >= h + s->ro)
        {
          new_star (bp, s);
          s->y = -s->ro;
        }
    }
}


static void
calc_o (gw_configuration *bp, GLfloat mass, GLfloat cx, GLfloat y02,
        unsigned from, unsigned to)
{
  GLfloat x0 = cx - from * GRID_SEG;
  GLfloat g0 = mass / (x0*x0 + y02);
  unsigned x;

  ASSERT (to <= bp->grid_w || to <= bp->grid_h);

  for (x = from; x < to; x++)
    {
      GLfloat *g = &bp->grid[x * GRID_SEG];
      GLfloat g1;

      x0 = cx - (x + 1) * GRID_SEG;
      g1 = mass / (x0*x0 + y02);

      g[0] += g0;
      if (bp->segs[x])
        {
          GLfloat d = (g1 - g0) / GRID_SEG;
          unsigned i;
          for(i = 1; i != GRID_SEG; i++)
            {
              g0 += d;
              g[i] += g0;
            }
        }
      g0 = g1;
    }
}


static void
make_hires (gw_configuration *bp, unsigned from, unsigned to, unsigned w)
{
  unsigned x;

  /* One bigger than from/to so that there's a good angle between the middle
     and inner zones.

     Don't make the last GRID_SEG high-res. This keeps the length consistent.
   */
  if (from)
    from--;
  from = MIN(from / GRID_SEG, w - 1);
  to = MIN(to / GRID_SEG + 1, w - 1);

  ASSERT (to <= bp->grid_w - 1 || to <= bp->grid_h - 1);

  for (x = from; x < to; x++)
    {
      if (! bp->segs[x])
        {
          GLfloat *g = &bp->grid[x * GRID_SEG];
          GLfloat g0 = g[0], g1 = g[GRID_SEG];
          GLfloat d = (g1 - g0) / GRID_SEG;
          unsigned i;
          for (i = 1; i != GRID_SEG; i++)
            {
              g0 += d;
              g[i] = g0;
            }
          bp->segs[x] = True;
        }
    }
}


static void
calc_m (gw_configuration *bp, GLfloat mass, GLfloat cx, GLfloat y02,
        unsigned from, unsigned to)
{
  GLfloat *gridp = bp->grid;
  unsigned x;

  ASSERT (to <= bp->grid_w * GRID_SEG + 1 || to <= bp->grid_h * GRID_SEG + 1);

  for (x = from; x < to; x++)
    {
      /* Inverse square of distance from mass as a point source */
      GLfloat x0 = cx - x;
      gridp[x] += mass / (x0*x0 + y02);
    }
}


#define EASE(r) (sin ((r) * M_PI_2))

static void
draw_row (ModeInfo *mi, int w, int y, Bool swap)
{
  gw_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  int x;
  int polys;
  int w2 = w * GRID_SEG;

  GLfloat *vtx_x;
  GLfloat *vtx_y;
  GLfloat *gridp = bp->grid;
  memset (gridp, 0, w2 * sizeof(*gridp));
  memset (bp->segs, 0, w);

  for (i = 0; i < bp->nstars; i++)
    {
      star *s = &bp->stars[i];
      GLfloat cx, cy;
      unsigned olo, ohi, mlo, mhi, ilo, ihi;
      GLfloat mass, max;
      /* Move stars off screen until most of their influence fades */
      GLfloat ro, rm, ri;

      GLfloat y0;
      GLfloat y02;

      if (swap)
        {
          cy = s->x;
          cx = s->y;
        }
      else
        {
          cx = s->x;
          cy = s->y;
        }
      mass = s->mass;
      max = s->surface_gravity;

      y0 = cy - y;
      y02 = y0 * y0;

      if (y02 > s->ro2) continue;

      ro = sqrtf (s->ro2 - y02);
      olo = WCLIP((cx - ro) / GRID_SEG + 1, w);  /* GLfloat -> int */
      ohi = WCLIP((cx + ro) / GRID_SEG + 1, w);

      rm = s->rm2 > y02 ? sqrtf (s->rm2 - y02) : 0;
      mlo = WCLIP((cx - rm) + 1, w2);
      mhi = WCLIP((cx + rm) + 1, w2);

      ASSERT (mlo <= mhi);

      if (mlo != mhi)
        {
          ri = s->ri2 > y02 ? sqrtf (s->ri2 - y02) : 0;
          ilo = WCLIP(cx - ri + 1, w2);
          ihi = WCLIP(cx + ri + 1, w2);

          mlo -= mlo % GRID_SEG;
          mhi += GRID_SEG - 1;
          mhi -= mhi % GRID_SEG;

          /* These go first. */
          make_hires (bp, mlo, ilo, w);
          make_hires (bp, ihi, mhi, w);

          calc_m (bp, mass, cx, y02, mlo, ilo);
          calc_m (bp, mass, cx, y02, ihi, mhi);

          /* This does a bit more work than it needs to. */
          for (x = ilo; x < ihi; x++)
            gridp[x] += max;
        }

      calc_o (bp, mass, cx, y02, olo, mlo / GRID_SEG);
      calc_o (bp, mass, cx, y02, mhi / GRID_SEG, ohi);
    }

  if (swap)
    {
      vtx_y = bp->vtx;
      vtx_x = bp->vtx + 1;
    }
  else
    {
      vtx_x = bp->vtx;
      vtx_y = bp->vtx + 1;
    }

# define COLOR_CODE 0

# if COLOR_CODE
  {
    unsigned grid_max = bp->grid_w > bp->grid_h ? bp->grid_w : bp->grid_h;
    GLfloat *color = malloc(sizeof(GLfloat) * 4 * (grid_max * GRID_SEG + 1));
    glEnableClientState (GL_COLOR_ARRAY);
    glColorPointer (4, GL_FLOAT, 0, color);
# endif

  ASSERT (! bp->segs[w - 1]);

  polys = 0;
  for (x = 0; x != w; x++)
    {
      if (! bp->segs[x])
        {
          int ci;
          size_t vp = polys * 3;
          size_t cp = polys * 4;
# if COLOR_CODE
          GLfloat slope = 0;
          if (x != 0)
            slope += fabs(gridp[x * GRID_SEG] - gridp[(x - 1) * GRID_SEG]);
          if (x != w - 1)
            slope += fabs(gridp[(x + 1) * GRID_SEG] - gridp[x * GRID_SEG]);
          slope = 1 - (slope / (SLOPE_EPSILON * 2));

          color[cp]     = slope;
          color[cp + 1] = slope;
          color[cp + 2] = 1;
          color[cp + 3] = 1;
# endif
          vtx_x[vp] = x * GRID_SEG;
          bp->vtx[vp + 2] = gridp[x * GRID_SEG];
          polys += 1;

          ci = EASE (bp->vtx[vp + 2] / MAX_MASS_COLOR) * bp->ncolors;
          if (ci < 0) ci = 0;
          if (ci >= bp->ncolors) ci = bp->ncolors - 1;
          bp->col[cp]   = bp->colors[ci].red   / 65536.0;
          bp->col[cp+1] = bp->colors[ci].green / 65536.0;
          bp->col[cp+2] = bp->colors[ci].blue  / 65536.0;
          bp->col[cp+3] = 1;
        }
      else
        {
          for(i = 0; i != GRID_SEG; i++)
            {
              int ci;
              size_t vp = (polys + i) * 3;
              size_t cp = (polys + i) * 4;
# if COLOR_CODE
              color[cp] = 1;
              color[cp + 1] = 0.75;
              color[cp + 2] = 0;
              color[cp + 3] = 1;
# endif
              vtx_x[vp] = x * GRID_SEG + i;
              bp->vtx[vp + 2] = gridp[x * GRID_SEG + i];

              ci = EASE (bp->vtx[vp + 2] / MAX_MASS_COLOR) * bp->ncolors;
              if (ci < 0) ci = 0;
              if (ci >= bp->ncolors) ci = bp->ncolors - 1;
              bp->col[cp]   = bp->colors[ci].red   / 65536.0;
              bp->col[cp+1] = bp->colors[ci].green / 65536.0;
              bp->col[cp+2] = bp->colors[ci].blue  / 65536.0;
              bp->col[cp+3] = 1;
            }
          polys += GRID_SEG;
        }
    }

  for (i = 0; i < polys; i++)
    vtx_y[i * 3] = y; /* + random() * (MASS_EPSILON / (MAXRAND)); */

  mi->polygon_count += polys;
  glDrawArrays (GL_LINE_STRIP, 0, polys);

# if COLOR_CODE
    glDisableClientState (GL_COLOR_ARRAY);
    free (color);
  }
# endif
}


ENTRYPOINT void 
init_gw (ModeInfo *mi)
{
  gw_configuration *bp;
  unsigned grid_max, vtx_max;
  int i;
  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_gw (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  {
    int h1, h2;
    double s1, v1, s2, v2;
    GLfloat color2[4];
    parse_color (mi, "gridColor", bp->color);
    parse_color (mi, "gridColor2", color2);
    rgb_to_hsv (bp->color[0] * 65536,
                bp->color[1] * 65536,
                bp->color[2] * 65536,
                &h1, &s1, &v1);
    rgb_to_hsv (color2[0] * 65536,
                color2[1] * 65536,
                color2[2] * 65536,
                &h2, &s2, &v2);
    bp->ncolors = 128;
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
    make_color_ramp (0, 0, 0,
                     h1, s1, v1, h2, s2, v2,
                     bp->colors, &bp->ncolors,
                     False, 0, False);
  }

  bp->user_trackball = gltrackball_init (False);

  bp->grid_w = (RESOLUTION_BASE * resolution) / GRID_SEG;
  if (bp->grid_w < 2) bp->grid_w = 2;
  bp->grid_h = bp->grid_w;

  grid_max = bp->grid_w > bp->grid_h ? bp->grid_w : bp->grid_h;
  vtx_max = grid_max * GRID_SEG;
  bp->grid = (GLfloat *) calloc (vtx_max, sizeof(*bp->grid));
  bp->vtx = (GLfloat *) calloc (vtx_max * 3, sizeof(*bp->vtx));
  bp->col = (GLfloat *) calloc (vtx_max * 4, sizeof(*bp->col));
  bp->segs = (char *) calloc (grid_max, sizeof(*bp->segs));
  if (! bp->grid || ! bp->vtx || ! bp->col || ! bp->segs) abort();

  bp->nstars = MI_COUNT(mi);
  bp->stars = (star *) calloc (bp->nstars, sizeof (star));

  for (i = 0; i < bp->nstars; i++)
    {
      star *s = &bp->stars[i];
      new_star (bp, s);
      s->y = frand(s->ro * 2 + bp->grid_h * GRID_SEG) - s->ro;
    }

  /* Let's tilt the floor a little. */
  gltrackball_reset (bp->user_trackball,
                     -0.4 + frand(0.8),
                     -0.3 + frand(0.2));
}


ENTRYPOINT void
draw_gw (ModeInfo *mi)
{
  gw_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int gridmod = grid_size * GRID_SIZE_BASE;
  int x, y, i;
  int sample_x, sample_y;
  GLfloat sample_z = -1;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

# ifdef HAVE_MOBILE
  glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */
# endif

  gltrackball_rotate (bp->user_trackball);

#if 0
  glScalef(0.05/resolution, 0.05/resolution, 0.05/resolution);
#endif

  glRotatef (90, 1, 0, 0);
  glTranslatef (-bp->grid_w * (GRID_SEG / 2.0f),
                -bp->grid_h * (GRID_SEG * 0.75f),
                3);

#if 0
  glColor3f(1,0,0);
  glPushMatrix();
  glTranslatef(0,0,0);
  glScalef (bp->grid_w * GRID_SEG,
            bp->grid_w * GRID_SEG,
            bp->grid_w * GRID_SEG);
  glDisable (GL_FOG);
  glBegin(GL_LINE_LOOP);
  glVertex3f(0, 0, 0);
  glVertex3f(1, 0, 0);
  glVertex3f(1, 1, 0);
  glVertex3f(.4, 1, 0);
  glVertex3f(.5, .5, 0);
  glVertex3f(.6, 1, 0);
  glVertex3f(0, 1, 0);
  glEnd();
  glPopMatrix();
  glColor3f(0,1,0);
  if (!wire) glEnable (GL_FOG);
#endif

  if (!wire)
    {
      GLfloat fog_color[4] = { 0, 0, 0, 1 };

      glLineWidth (2);
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);

      glFogi (GL_FOG_MODE, GL_EXP2);
      glFogfv (GL_FOG_COLOR, fog_color);
      glFogf (GL_FOG_DENSITY, 0.005);
      glEnable (GL_FOG);
    }

  glEnableClientState (GL_COLOR_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
  glColorPointer  (4, GL_FLOAT, 0, bp->col);
  glVertexPointer (3, GL_FLOAT, 0, bp->vtx);

  /* Somewhere near the midpoint of the view */
  sample_x = ((int) (bp->grid_w * GRID_SEG * 0.5)  / gridmod) * gridmod;
  sample_y = ((int) (bp->grid_h * GRID_SEG * 0.75) / GRID_SEG) * GRID_SEG;

  /* Find the cumulative gravitational effect at the midpoint of each star,
     for the depth of the foot-circle.  This duplicates some of the draw_row()
     logic. */
  for (i = 0; i < bp->nstars; i++)
    {
      star *s0 = &bp->stars[i];
      GLfloat x0 = s0->x;
      GLfloat y0 = s0->y;
      int j;
      s0->depth = s0->surface_gravity;
      for (j = 0; j < bp->nstars; j++)
        {
          star *s1;
          GLfloat x1, y1, d2;
          if (i == j) continue;
          s1 = &bp->stars[j];
          x1 = s1->x;
          y1 = s1->y;
          d2 = (x1-x0)*(x1-x0) + (y1-y0)*(y1-y0);
          s0->depth += s1->mass / d2;
        }
    }

  mi->polygon_count = 0;
  for (y = 0; y < (bp->grid_h - 1) * GRID_SEG; y += gridmod)
    draw_row (mi, bp->grid_w, y, False);
  for (x = 0; x < (bp->grid_w - 1) * GRID_SEG; x += gridmod)
    {
      draw_row (mi, bp->grid_h, x, True);
      if (x == sample_x)
        sample_z = bp->grid[sample_y];
    }

  if (mi->fps_p)
    {
      /* Mass of Sol is 2x10^30kg, or 332 kilo-Earths.
         But I'm not sure what the funniest number to put here is. */
      /* mi->recursion_depth = (int) sample_z/4; */
      mi->recursion_depth = (int) (sample_z * 30000);
      glColor4fv (bp->color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->color);
      glBegin(GL_LINES);
      glVertex3f (sample_x-0.15, sample_y-0.15, sample_z);
      glVertex3f (sample_x+0.15, sample_y+0.15, sample_z);
      glVertex3f (sample_x-0.15, sample_y+0.15, sample_z);
      glVertex3f (sample_x+0.15, sample_y-0.15, sample_z);
      glEnd();
    }

  /* Draw a circle around the "footprint" at the bottom of the gravity well.
   */
  for (i = 0; i < bp->nstars; i++)
    {
      int steps = 16;
      star *s = &bp->stars[i];
      GLfloat th, color[4];
      int ci;
      ci = EASE (s->depth / MAX_MASS_COLOR) * bp->ncolors;
      if (ci < 0) ci = 0;
      if (ci >= bp->ncolors) ci = bp->ncolors - 1;
      color[0] = bp->colors[ci].red   / 65536.0;
      color[1] = bp->colors[ci].green / 65536.0;
      color[2] = bp->colors[ci].blue  / 65536.0;
      color[3] = 1;
      glColor4fv (color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
      glPushMatrix();
      glTranslatef (s->x, s->y, 0);
      glBegin (GL_LINE_LOOP);
      for (th = 0; th < M_PI * 2; th += M_PI/steps)
        glVertex3f (s->radius * cos(th), s->radius * sin(th), s->depth);
      glEnd();
      glPopMatrix();
      mi->polygon_count += steps;
    }

#if 0
  {
    for (i = 0; i < bp->nstars; i++)
      {
        star *s = &bp->stars[i];
        GLfloat maxr = sqrt (s->mass / MASS_EPSILON);
        GLfloat th;
        glPushMatrix();
        glTranslatef (s->x, s->y, 0);
        glColor3f(0, 0, 1);
        glBegin (GL_LINE_LOOP);
        for (th = 0; th < M_PI * 2; th += M_PI/32)
          glVertex3f (s->radius * cos(th), s->radius * sin(th), 0);
        glEnd();
        glColor3f(0, 0, 0.5);
        glBegin (GL_LINE_LOOP);
        for (th = 0; th < M_PI * 2; th += M_PI/32)
          glVertex3f (maxr * cos(th), maxr * sin(th), 0);
        glEnd();
        glBegin (GL_LINES);
        glVertex3f ( 3000 * s->dx,  3000 * s->dy, 0);
        glVertex3f (-3000 * s->dx, -3000 * s->dy, 0);
        glEnd();
        glPopMatrix();
      }
  }
#endif

  glPopMatrix ();

  if (! bp->button_down_p)
    move_stars (mi);

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_gw (ModeInfo *mi)
{
  gw_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->user_trackball) gltrackball_free (bp->user_trackball);
  if (bp->stars) free (bp->stars);
  if (bp->grid) free (bp->grid);
  if (bp->vtx) free (bp->vtx);
  if (bp->col) free (bp->col);
  if (bp->segs) free (bp->segs);
  if (bp->colors) free (bp->colors);
}

XSCREENSAVER_MODULE_2 ("GravityWell", gravitywell, gw)

#endif /* USE_GL */
