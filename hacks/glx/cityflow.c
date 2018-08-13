/* cityflow, Copyright (c) 2014-2017 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*count:        800         \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define free_cube 0
# define release_cube 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SKEW        "12"

#define DEF_WAVES        "6"
#define DEF_WAVE_SPEED   "25"
#define DEF_WAVE_RADIUS  "256"
static int texture_size = 512;

typedef struct {
  GLfloat x, y, z;
  GLfloat w, h, d;
  GLfloat cth, sth;
} cube;

typedef struct {
  int x, y;
  double xth, yth;
} wave_src;

typedef struct {
  int nwaves;
  int radius;
  int speed;
  wave_src *srcs;
  int *heights;
} waves;


typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  GLuint cube_list;
  int cube_polys;
  int ncubes;
  cube *cubes;
  waves *waves;
  GLfloat min_x, max_x, min_y, max_y;
  int texture_width, texture_height;
  int ncolors;
  XColor *colors;

} cube_configuration;

static cube_configuration *ccs = NULL;

static int wave_count;
static int wave_speed;
static int wave_radius;
static int skew;

static XrmOptionDescRec opts[] = {
  {"-waves",       ".waves",      XrmoptionSepArg, 0 },
  {"-wave-speed",  ".waveSpeed",  XrmoptionSepArg, 0 },
  {"-wave-radius", ".waveRadius", XrmoptionSepArg, 0 },
  {"-skew",        ".skew",       XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&wave_count, "waves",     "Waves",      DEF_WAVES, t_Int},
  {&wave_speed, "waveSpeed", "WaveSpeed",  DEF_WAVE_SPEED, t_Int},
  {&wave_radius,"waveRadius","WaveRadius", DEF_WAVE_RADIUS,t_Int},
  {&skew,       "skew",      "Skew",       DEF_SKEW,t_Int},
};

ENTRYPOINT ModeSpecOpt cube_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 2) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  /* For this one it's really important to minimize the distance between
     near and far. */
  gluPerspective (30, 1/h, 10, 50);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
reset_colors (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  make_smooth_colormap (0, 0, 0,
                        cc->colors, &cc->ncolors, 
                        False, 0, False);
  if (! MI_IS_WIREFRAME(mi))
    glClearColor (cc->colors[0].red   / 65536.0,
                  cc->colors[0].green / 65536.0,
                  cc->colors[0].blue  / 65536.0,
                  1);
}


static void
tweak_cubes (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < cc->ncubes; i++)
    {
      cube *cube = &cc->cubes[i];
      cube->x += (frand(2)-1)*0.01;
      cube->y += (frand(2)-1)*0.01;
      cube->z += (frand(2)-1)*0.01;
    }
}


ENTRYPOINT Bool
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];

  /* Neutralize any vertical motion */
  GLfloat rot = current_device_rotation();
  Bool rotp = ((rot >  45 && rot <  135) ||
               (rot < -45 && rot > -135));
               
  if (event->xany.type == ButtonPress ||
      event->xany.type == ButtonRelease)
    {
      if (rotp)
        event->xbutton.x = MI_WIDTH(mi) / 2;
      else
        event->xbutton.y = MI_HEIGHT(mi) / 2;
    }
  else if (event->xany.type == MotionNotify)
    {
      if (rotp)
        event->xmotion.x = MI_WIDTH(mi) / 2;
      else
        event->xmotion.y = MI_HEIGHT(mi) / 2;
    }

  if (gltrackball_event_handler (event, cc->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &cc->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      reset_colors (mi);
      tweak_cubes (mi);
      gltrackball_reset (cc->trackball, 0, 0);
      return True;
    }

  return False;
}


/* Waves.
   Adapted from ../hacks/interference.c by Hannu Mallat.
 */

static void
init_wave (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  waves *ww;
  int i;
  cc->waves = ww = (waves *) calloc (sizeof(*cc->waves), 1);
  ww->nwaves = wave_count;
  ww->radius = wave_radius;
  ww->speed  = wave_speed;
  ww->heights = (int *) calloc (sizeof(*ww->heights), ww->radius);
  ww->srcs = (wave_src *) calloc (sizeof(*ww->srcs), ww->nwaves);

  for (i = 0; i < ww->radius; i++)
    {
      float max = (cc->ncolors * (ww->radius - i) / (float) ww->radius);
      ww->heights[i] = ((max + max * cos(i / 50.0)) / 2.0);
    }

  for (i = 0; i < ww->nwaves; i++)
    {
      ww->srcs[i].xth = frand(2.0) * M_PI;
      ww->srcs[i].yth = frand(2.0) * M_PI;
    }

  cc->texture_width  = texture_size;
  cc->texture_height = texture_size;
}


static int
interference_point (cube_configuration *cc, int x, int y)
{
  /* Compute the effect of the waves on a pixel. */

  waves *ww = cc->waves;
  int result = 0;
  int i;
  for (i = 0; i < ww->nwaves; i++)
    {
      int dx = x - ww->srcs[i].x;
      int dy = y - ww->srcs[i].y;
      int dist = sqrt (dx*dx + dy*dy);
      result += (dist >= ww->radius ? 0 : ww->heights[dist]);
    }
  result *= 0.4;
  if (result > 255) result = 255;
  return result;
}


static void
interference (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  waves *ww = cc->waves;
  int i;

  /* Move the wave origins around
   */
  for (i = 0; i < ww->nwaves; i++)
    {
      ww->srcs[i].xth += (ww->speed / 1000.0);
      if (ww->srcs[i].xth > 2*M_PI)
        ww->srcs[i].xth -= 2*M_PI;
      ww->srcs[i].yth += (ww->speed / 1000.0);
      if (ww->srcs[i].yth > 2*M_PI)
        ww->srcs[i].yth -= 2*M_PI;

      ww->srcs[i].x = (cc->texture_width/2 +
                       (cos (ww->srcs[i].xth) *
                        cc->texture_width / 2));
      ww->srcs[i].y = (cc->texture_height/2 +
                       (cos (ww->srcs[i].yth) *
                        cc->texture_height / 2));
    }
}


/* qsort comparator for sorting cubes by y position */
static int
cmp_cubes (const void *aa, const void *bb)
{
  const cube *a = (cube *) aa;
  const cube *b = (cube *) bb;
  return ((int) (b->y * 10000) -
          (int) (a->y * 10000));
}


ENTRYPOINT void 
init_cube (ModeInfo *mi)
{
  int i;
  cube_configuration *cc;

  MI_INIT (mi, ccs);

  cc = &ccs[MI_SCREEN(mi)];

  if ((cc->glx_context = init_GL(mi)) != NULL) {
    reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  cc->trackball = gltrackball_init (False);

  cc->ncolors = 256;
  cc->colors = (XColor *) calloc(cc->ncolors, sizeof(XColor));

  reset_colors (mi);
  init_wave (mi);

  cc->ncubes = MI_COUNT (mi);

  if (cc->ncubes < 1) cc->ncubes = 1;

  cc->cubes = (cube *) calloc (sizeof(cube), cc->ncubes);
  for (i = 0; i < cc->ncubes; i++)
    {
      /* Set the size to roughly cover a 2x2 square on average. */
      GLfloat scale = 1.8 / sqrt (cc->ncubes);
      cube *cube = &cc->cubes[i];
      double th = -(skew ? frand(skew) : 0) * M_PI / 180;

      cube->x = (frand(1)-0.5);
      cube->y = (frand(1)-0.5);

      cube->z = frand(0.12);
      cube->cth = cos(th);
      cube->sth = sin(th);

      cube->w = scale * (frand(1) + 0.2);
      cube->d = scale * (frand(1) + 0.2);

      if (cube->x < cc->min_x) cc->min_x = cube->x;
      if (cube->y < cc->min_y) cc->min_y = cube->y;
      if (cube->x > cc->max_x) cc->max_x = cube->x;
      if (cube->y > cc->max_y) cc->max_y = cube->y;
    }

  /* Sorting by depth improves frame rate slightly. With 6000 polygons we get:
     3.9 FPS unsorted;
     3.1 FPS back to front;
     4.3 FPS front to back.
   */
  qsort (cc->cubes, cc->ncubes, sizeof(*cc->cubes), cmp_cubes);
}


static void
animate_cubes (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < cc->ncubes; i++)
    {
      cube *cube = &cc->cubes[i];
      GLfloat fx = (cube->x - cc->min_x) / (cc->max_x - cc->min_x);
      GLfloat fy = (cube->y - cc->min_y) / (cc->max_y - cc->min_y);
      int x = (int) (cc->texture_width  * fx) % cc->texture_width;
      int y = (int) (cc->texture_height * fy) % cc->texture_height;
      unsigned char v = interference_point (cc, x, y);
      cube->h = cube->z + (v / 256.0 / 2.5) + 0.1;
    }
}


ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!cc->glx_context)
    return;

  mi->polygon_count = 0;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cc->glx_context));

  interference (mi);
  animate_cubes (mi);

  glShadeModel(GL_FLAT);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);
  /* glEnable (GL_POLYGON_OFFSET_FILL); */

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glRotatef(current_device_rotation(), 0, 0, 1);
  gltrackball_rotate (cc->trackball);
  glRotatef (-180, 1, 0, 0);

  {
    GLfloat s = 15;
    glScalef (s, s, s);
  }
  glRotatef (-90, 1, 0, 0);

  glTranslatef (-0.18, 0, -0.18);
  glRotatef (37, 1, 0, 0);
  glRotatef (20, 0, 0, 1);

  glScalef (2.1, 2.1, 2.1);

  /* Position lights after device rotation. */
  if (!wire)
    {
      static const GLfloat pos[4] = {0.0, 0.25, -1.0, 0.0};
      static const GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      static const GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
    }

  glBegin (wire ? GL_LINES : GL_QUADS);

  for (i = 0; i < cc->ncubes; i++)
    {
      cube *cube = &cc->cubes[i];
      GLfloat cth = cube->cth;
      GLfloat sth = cube->sth;
      GLfloat x =  cth*cube->x + sth*cube->y;
      GLfloat y = -sth*cube->x + cth*cube->y;
      GLfloat w = cube->w/2;
      GLfloat h = cube->h/2;
      GLfloat d = cube->d/2;
      GLfloat bottom = 5;

      GLfloat xw =  cth*w, xd = sth*d;
      GLfloat yw = -sth*w, yd = cth*d;

      GLfloat color[4];
      int c = cube->h * cc->ncolors * 0.7;
      c %= cc->ncolors;

      color[0] = cc->colors[c].red   / 65536.0;
      color[1] = cc->colors[c].green / 65536.0;
      color[2] = cc->colors[c].blue  / 65536.0;
      color[3] = 1.0;
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

      /* Putting this in a display list makes no performance difference. */

      if (! wire)
        {
          glNormal3f (0, 0, -1);				/* top */
          glVertex3f (x+xw+xd, y+yw+yd, -h);
          glVertex3f (x+xw-xd, y+yw-yd, -h);
          glVertex3f (x-xw-xd, y-yw-yd, -h);
          glVertex3f (x-xw+xd, y-yw+yd, -h);
          mi->polygon_count++;

          glNormal3f (sth, cth, 0);				/* front */
          glVertex3f (x+xw+xd, y+yw+yd, bottom);
          glVertex3f (x+xw+xd, y+yw+yd, -h);
          glVertex3f (x-xw+xd, y-yw+yd, -h);
          glVertex3f (x-xw+xd, y-yw+yd, bottom);
          mi->polygon_count++;

          glNormal3f (cth, -sth, 0);				/* right */
          glVertex3f (x+xw-xd, y+yw-yd, -h);
          glVertex3f (x+xw+xd, y+yw+yd, -h);
          glVertex3f (x+xw+xd, y+yw+yd, bottom);
          glVertex3f (x+xw-xd, y+yw-yd, bottom);
          mi->polygon_count++;

# if 0    /* Omitting these makes no performance difference. */

          glNormal3f (-cth, sth, 0);			/* left */
          glVertex3f (x-xw+xd, y-yw+yd, -h);
          glVertex3f (x-xw-xd, y-yw-yd, -h);
          glVertex3f (x-xw-xd, y-yw-yd, bottom);
          glVertex3f (x-xw+xd, y-yw+yd, bottom);
          mi->polygon_count++;

          glNormal3f (-sth, -cth, 0);			/* back */
          glVertex3f (x-xw-xd, y-yw-yd, bottom);
          glVertex3f (x-xw-xd, y-yw-yd, -h);
          glVertex3f (x+xw-xd, y+yw-yd, -h);
          glVertex3f (x+xw-xd, y+yw-yd, bottom);
          mi->polygon_count++;
# endif
        }
      else
        {
          glNormal3f (0, 0, -1);				/* top */
          glVertex3f (x+xw+xd, y+yw+yd, -h);
          glVertex3f (x+xw-xd, y+yw-yd, -h);

          glVertex3f (x+xw-xd, y+yw-yd, -h);
          glVertex3f (x-xw-xd, y-yw-yd, -h);

          glVertex3f (x-xw-xd, y-yw-yd, -h);
          glVertex3f (x-xw+xd, y-yw+yd, -h);

          glVertex3f (x-xw+xd, y-yw+yd, -h);
          glVertex3f (x+xw+xd, y+yw+yd, -h);
          mi->polygon_count++;
        }
    }
  glEnd();

  glPolygonOffset (0, 0);

# if 0
  glDisable(GL_DEPTH_TEST);	/* Outline the playfield */
  glColor3f(1,1,1);
  glBegin(GL_LINE_LOOP);
  glVertex3f (-0.5, -0.5, 0);
  glVertex3f (-0.5,  0.5, 0);
  glVertex3f ( 0.5,  0.5, 0);
  glVertex3f ( 0.5, -0.5, 0);
  glEnd();
# endif

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


XSCREENSAVER_MODULE_2 ("Cityflow", cityflow, cube)

#endif /* USE_GL */
