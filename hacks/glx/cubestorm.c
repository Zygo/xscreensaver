/* cubestorm, Copyright (c) 2003-2018 Jamie Zawinski <jwz@jwz.org>
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
			"*count:      " DEF_COUNT   "\n" \
			"*showFPS:      False       \n" \
	               	"*fpsSolid:     True        \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \


# define release_cube 0

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.06"
#define DEF_COUNT       "4"
#define DEF_LENGTH      "200"

typedef struct {
  GLfloat px, py, pz;
  GLfloat rx, ry, rz;
  int ccolor;
} histcube;

typedef struct {
  rotator *rot;
  int ccolor;
} subcube;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  Bool clear_p;

  GLuint cube_list;

  int ncolors;
  XColor *colors;

  subcube *subcubes;

  int hist_size, hist_count;
  histcube *hist;

} cube_configuration;

static cube_configuration *bps = NULL;

static Bool do_spin;
static Bool do_wander;
static GLfloat speed;
static GLfloat thickness;
static int max_length;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-db",     ".doubleBuffer", XrmoptionNoArg, "True"},
  { "+db",     ".doubleBuffer", XrmoptionNoArg, "False"},
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
  { "-length", ".length", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness,  "thickness", "Thickness", DEF_THICKNESS, t_Float},
  {&max_length, "length",    "Length",    DEF_LENGTH,    t_Int},
};

ENTRYPOINT ModeSpecOpt cube_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void
draw_face (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);

  int i;
  GLfloat t = thickness / 2;
  GLfloat a = -0.5;
  GLfloat b =  0.5;

  if (t <= 0) t = 0.001;
  else if (t > 0.5) t = 0.5;

  glPushMatrix();
  glFrontFace(GL_CW);

  for (i = 0; i < 4; i++)
    {
      glNormal3f (0, 0, -1);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (a,   a,   a);
      glVertex3f (b,   a,   a);
      glVertex3f (b-t, a+t, a);
      glVertex3f (a+t, a+t, a);
      glEnd();

      glNormal3f (0, 1, 0);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (b-t, a+t, a);
      glVertex3f (b-t, a+t, a+t);
      glVertex3f (a+t, a+t, a+t);
      glVertex3f (a+t, a+t, a);
      glEnd();

      glRotatef(90, 0, 0, 1);
    }
  glPopMatrix();
}

static void
draw_faces (ModeInfo *mi)
{
  glPushMatrix();
  draw_face (mi);
  glRotatef (90,  0, 1, 0); draw_face (mi);
  glRotatef (90,  0, 1, 0); draw_face (mi);
  glRotatef (90,  0, 1, 0); draw_face (mi);
  glRotatef (90,  1, 0, 0); draw_face (mi);
  glRotatef (180, 1, 0, 0); draw_face (mi);
  glPopMatrix();
}


static void
new_cube_colors (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  bp->ncolors = 128;
  if (bp->colors) free (bp->colors);
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);
  for (i = 0; i < MI_COUNT(mi); i++)
    bp->subcubes[i].ccolor = random() % bp->ncolors;
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
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
  gluLookAt( 0.0, 0.0, 45.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT Bool
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ')
        {
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        goto DEF;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
    DEF:
      new_cube_colors (mi);
      return True;
    }
  return False;
}


ENTRYPOINT void 
init_cube (ModeInfo *mi)
{
  cube_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  if (MI_COUNT(mi) <= 0) MI_COUNT(mi) = 1;

  bp->trackball = gltrackball_init (True);
  bp->subcubes = (subcube *) calloc (MI_COUNT(mi), sizeof(subcube));

  bp->hist_count = 0;
  bp->hist_size = 100;
  bp->hist = (histcube *) malloc (bp->hist_size * sizeof(*bp->hist));

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      double wander_speed, spin_speed, spin_accel;

      if (i == 0)
        {
          wander_speed = 0.05 * speed;
          spin_speed   = 10.0 * speed;
          spin_accel   = 4.0  * speed;
        }
      else
        {
          wander_speed = 0;
          spin_speed   = 4.0 * speed;
          spin_accel   = 2.0 * speed;
        }

      bp->subcubes[i].rot = make_rotator (do_spin ? spin_speed : 0,
                                          do_spin ? spin_speed : 0,
                                          do_spin ? spin_speed : 0,
                                          spin_accel,
                                          do_wander ? wander_speed : 0,
                                          True);
    }

  bp->colors = 0;
  new_cube_colors (mi);

  reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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

  bp->cube_list = glGenLists (1);
  glNewList (bp->cube_list, GL_COMPILE);
  draw_faces (mi);
  glEndList ();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


/* Originally, this program achieved the "accumulating cubes" effect by
   simply not clearing the depth or color buffers between frames.  That
   doesn't work on modern systems, particularly mobile: you can no longer
   rely on your buffers being unmolested once you have yielded.  So now we
   must save and re-render every polygon.  Noof has the same problem and
   solves it by taking a screenshot of the frame buffer into a texture, but
   cubestorm needs to restore the depth buffer as well as the color buffer.
 */
static void
push_hist (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  double px, py, pz;
  double rx = 0, ry = 0, rz = 0;
  int i;

  if (bp->hist_count > max_length &&
      bp->hist_count > MI_COUNT(mi) &&
      !bp->button_down_p)
    {
      /* Drop history off of the end. */
      memmove (bp->hist,
               bp->hist + MI_COUNT(mi), 
               (bp->hist_count - MI_COUNT(mi)) * sizeof(*bp->hist));
      bp->hist_count -= MI_COUNT(mi);
    }

  if (bp->hist_count + MI_COUNT(mi) >= bp->hist_size)
    {
      bp->hist_size = bp->hist_count + MI_COUNT(mi) + 100;
      bp->hist = (histcube *)
        realloc (bp->hist, bp->hist_size * sizeof(*bp->hist));
    }
  
  get_position (bp->subcubes[0].rot, &px, &py, &pz, !bp->button_down_p);

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      subcube  *sc = &bp->subcubes[i];
      histcube *hc = &bp->hist[bp->hist_count];
      double rx2, ry2, rz2;

      get_rotation (sc->rot, &rx2, &ry2, &rz2, !bp->button_down_p);

      if (i == 0)  /* N+1 cubes rotate relative to cube 0 */
        rx = rx2, ry = ry2, rz = rz2;
      else
        rx2 += rx, ry2 += ry, rz2 += rz;

      hc->px = px;
      hc->py = py;
      hc->pz = pz;
      hc->rx = rx2;
      hc->ry = ry2;
      hc->rz = rz2;
      hc->ccolor = sc->ccolor;
      sc->ccolor++;
      if (sc->ccolor >= bp->ncolors)
        sc->ccolor = 0;
      bp->hist_count++;
    }
}


ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (bp->clear_p)   /* we're in "no vapor trails" mode */
    {
      bp->hist_count = 0;
      if (! (random() % (int) (25 / speed)))
        bp->clear_p = False;
    }
  else
    {
      if (! (random() % (int) (200 / speed)))
        {
          bp->clear_p = True;
          new_cube_colors (mi);
        }
    }

  push_hist (mi);
  mi->polygon_count = 0;
  for (i = 0; i < bp->hist_count; i++)
    {
      GLfloat bcolor[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
      GLfloat bshiny    = 128.0;

      histcube *hc = &bp->hist[i];

      glPushMatrix();
      glScalef (1.1, 1.1, 1.1);

      glTranslatef((hc->px - 0.5) * 15,
                   (hc->py - 0.5) * 15,
                   (hc->pz - 0.5) * 30);
      gltrackball_rotate (bp->trackball);

      glScalef (4.0, 4.0, 4.0);

      glRotatef (hc->rx * 360, 1.0, 0.0, 0.0);
      glRotatef (hc->ry * 360, 0.0, 1.0, 0.0);
      glRotatef (hc->rz * 360, 0.0, 0.0, 1.0);

      bcolor[0] = bp->colors[hc->ccolor].red   / 65536.0;
      bcolor[1] = bp->colors[hc->ccolor].green / 65536.0;
      bcolor[2] = bp->colors[hc->ccolor].blue  / 65536.0;

      if (wire)
        glColor3f (bcolor[0], bcolor[1], bcolor[2]);
      else
        {
          glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
          glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
          glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);
        }

      glCallList (bp->cube_list);
      mi->polygon_count += (4 * 2 * 6);

      glPopMatrix();
    }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->subcubes) {
    for (i = 0; i < MI_COUNT(mi); i++)
      free_rotator (bp->subcubes[i].rot);
    free (bp->subcubes);
  }
  if (bp->hist) free (bp->hist);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->colors) free (bp->colors);

}

XSCREENSAVER_MODULE_2 ("CubeStorm", cubestorm, cube)

#endif /* USE_GL */
