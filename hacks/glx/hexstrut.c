/* hexstrut, Copyright (c) 2016-2017 Jamie Zawinski <jwz@jwz.org>
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
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*count:        20          \n" \
			"*suppressRotationAnimation: True\n" \

# define release_hexstrut 0

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.2"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef struct triangle triangle;
struct triangle {
  XYZ p[3];
  triangle *next;
  triangle *neighbors[6];
  GLfloat orot, rot;
  int delay, odelay;
  int ccolor;
};

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int count;
  triangle *triangles;

  int ncolors;
  XColor *colors;

} hexstrut_configuration;

static hexstrut_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static GLfloat thickness;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
};

ENTRYPOINT ModeSpecOpt hexstrut_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Add t1 to the neighbor list of t0. */
static void
link_neighbor (triangle *t0, triangle *t1)
{
  int k;
  if (t0 == t1)
    return;
  for (k = 0; k < countof(t0->neighbors); k++)
    {
      if (t0->neighbors[k] == t1 ||
          t0->neighbors[k] == 0)
        {
          t0->neighbors[k] = t1;
          return;
        }
    }
  fprintf (stderr, "too many neighbors\n");
  abort();
}


static void
make_plane (ModeInfo *mi)
{
  hexstrut_configuration *bp = &bps[MI_SCREEN(mi)];
  int n = MI_COUNT(mi) * 2;
  GLfloat size = 2.0 / n;
  int x, y;
  GLfloat w = size;
  GLfloat h = size * sqrt(3) / 2;
  triangle **grid = (triangle **) calloc (n * n, sizeof(*grid));

  for (y = 0; y < n; y++)
    for (x = 0; x < n; x++)
      {
        triangle *t;

        t = (triangle *) calloc (1, sizeof(*t));
        t->p[0].x = (x - n/2) * w;
        t->p[0].y = (y - n/2) * h;

        if (y & 1)
          t->p[0].x += w / 2;

        t->p[1].x = t->p[0].x - w/2;
        t->p[2].x = t->p[0].x + w/2;
        t->p[1].y = t->p[0].y + h;
        t->p[2].y = t->p[0].y + h;

        if (x > 0)
          {
            triangle *t2 = grid[y * n + (x-1)];
            link_neighbor (t, t2);
            link_neighbor (t2, t);
          }
        if (y > 0)
          {
            triangle *t2 = grid[(y-1) * n + x];
            link_neighbor (t, t2);
            link_neighbor (t2, t);

            if (x < n-1)
              {
                t2 = grid[(y-1) * n + (x+1)];
                link_neighbor (t, t2);
                link_neighbor (t2, t);
              }
          }

        t->next = bp->triangles;
        bp->triangles = t;
        grid[y * n + x] = t;
        bp->count++;
      }

  free (grid);
}


static void
tick_triangles (ModeInfo *mi)
{
  hexstrut_configuration *bp = &bps[MI_SCREEN(mi)];
  triangle *t;
  GLfloat step = 0.01 + (0.04 * speed);

  if (! (random() % 80))
    {
      int n = random() % bp->count;
      int i;
      for (i = 0, t = bp->triangles; t && i < n; t = t->next, i++)
        ;
      if (! t->rot)
        {
          t->rot += step * ((random() & 1) ? 1 : -1);
          t->odelay = t->delay = 4;
        }
    }

  for (t = bp->triangles; t; t = t->next)
    {
      /* If this triangle is rotating, continue until done. */
      if (t->rot)
        {
          t->rot += step * (t->rot > 0 ? 1 : -1);

          t->ccolor++;
          if (t->ccolor >= bp->ncolors)
            t->ccolor = 0;

          if (t->rot > 1 || t->rot < -1)
            {
              t->orot += (t->rot > 1 ? 1 : -1);
              t->rot = 0;
            }
        }

      /* If this triangle's propagation delay hasn't hit zero, decrement it.
         When it does, start its neighbors rotating.
       */
      if (t->delay)
        {
          int i;
          t->delay--;
          if (t->delay == 0)
            for (i = 0; i < countof(t->neighbors); i++)
              {
                if (t->neighbors[i] &&
                    t->neighbors[i]->rot == 0)
                  {
                    t->neighbors[i]->rot += step * (t->rot > 0 ? 1 : -1);
                    t->neighbors[i]->delay = 
                      t->neighbors[i]->odelay = t->odelay;
                  }
              }
        }
    }
}


static void
draw_triangles (ModeInfo *mi)
{
  hexstrut_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  triangle *t;
  GLfloat length = sqrt(3) / 3;
  GLfloat t2 = length * thickness / 2;
  GLfloat scale;

  {
    triangle *t = bp->triangles;
    GLfloat X = t->p[0].x - t->p[1].x;
    GLfloat Y = t->p[0].y - t->p[1].y;
    GLfloat Z = t->p[0].z - t->p[1].z;
    scale = sqrt(X*X + Y*Y + Z*Z);
  }

  glFrontFace (GL_CCW);

  glBegin (wire ? GL_LINES : GL_QUADS);

  glNormal3f (0, 0, 1);
  for (t = bp->triangles; t; t = t->next)
    {
      int i;
      XYZ c;
      GLfloat color[4];

      GLfloat angle = (M_PI * 2 / 3) * t->rot;
      GLfloat cr = cos(angle), sr = sin(angle);

      c.x = (t->p[0].x + t->p[1].x + t->p[2].x) / 3;
      c.y = (t->p[0].y + t->p[1].y + t->p[2].y) / 3;
      c.z = (t->p[0].z + t->p[1].z + t->p[2].z) / 3;

      /* Actually we don't need normals at all, since no lighting.
      do_normal (t->p[0].x, t->p[0].y, t->p[0].z,
                 t->p[1].x, t->p[1].y, t->p[1].z,
                 t->p[2].x, t->p[2].y, t->p[2].z);
      */

      color[0] = bp->colors[t->ccolor].red   / 65535.0;
      color[1] = bp->colors[t->ccolor].green / 65535.0;
      color[2] = bp->colors[t->ccolor].blue  / 65535.0;
      color[3] = 1;

      /* Brighter */
      color[0] = color[0] * 0.75 + 0.25;
      color[1] = color[1] * 0.75 + 0.25;
      color[2] = color[2] * 0.75 + 0.25;

      glColor4fv (color);
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

      for (i = 0; i < 3; i++)
        {
          /* Orient to direction of corner. */
          GLfloat x = t->p[i].x - c.x;
          GLfloat y = t->p[i].y - c.y;
          GLfloat z = t->p[i].z - c.z;

          GLfloat smc = sr * y - cr * x;
          GLfloat spc = cr * y + sr * x;

          GLfloat st2 = t2 * scale / sqrt(x*x + y*y);
          GLfloat slength = length * scale / sqrt(x*x + y*y + z*z);

          GLfloat xt2 = spc * st2;
          GLfloat yt2 = smc * st2;
          GLfloat xlength = c.x - slength * smc;
          GLfloat ylength = c.y + slength * spc;
          GLfloat zlength = c.z + slength * z;

          if (! wire)
            glVertex3f (c.x - xt2, c.y - yt2, c.z);

          glVertex3f (c.x + xt2, c.y + yt2, c.z);
          if (wire)
            glVertex3f (xlength + xt2, ylength + yt2, zlength);

          if (! wire)
            glVertex3f (xlength + xt2, ylength + yt2, zlength);

          glVertex3f (xlength - xt2, ylength - yt2, zlength);

          if (wire)
            glVertex3f (c.x - xt2, c.y - yt2, c.z);

          mi->polygon_count++;
        }
    }

  glEnd();
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_hexstrut (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 3) {   /* tiny window: show middle */
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
hexstrut_handle_event (ModeInfo *mi, XEvent *event)
{
  hexstrut_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t')
        {
          bp->ncolors = 64;
          make_smooth_colormap (0, 0, 0,
                                bp->colors, &bp->ncolors,
                                False, 0, False);
          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_hexstrut (ModeInfo *mi)
{
  hexstrut_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hexstrut (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    double spin_speed   = 0.002;
    double wander_speed = 0.003;
    double spin_accel   = 1.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }


  /* Let's tilt the scene a little. */
  gltrackball_reset (bp->trackball,
                     -0.4 + frand(0.8),
                     -0.4 + frand(0.8));

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 1.7) thickness = 1.7;
  if (speed > 2) speed = 2;

  bp->ncolors = 64;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  make_plane (mi);
}


ENTRYPOINT void
draw_hexstrut (ModeInfo *mi)
{
  hexstrut_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 12);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (30, 30, 30);

  if (! bp->button_down_p)
    tick_triangles (mi);
  draw_triangles (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hexstrut (ModeInfo *mi)
{
  hexstrut_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->colors) free (bp->colors);
  while (bp->triangles)
    {
      triangle *t = bp->triangles->next;
      free (bp->triangles);
      bp->triangles = t;
    }
}

XSCREENSAVER_MODULE ("Hexstrut", hexstrut)

#endif /* USE_GL */
