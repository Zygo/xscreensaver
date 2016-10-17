/* menger, Copyright (c) 2001-2014 Jamie Zawinski <jwz@jwz.org>
 *         Copyright (c) 2002 Aurelien Jacobs <aurel@gnuage.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Generates a 3D Menger Sponge gasket:
 *
 *                                ___+._______
 *                           __-""   --     __"""----._____
 *                    __.--"" -- ___--+---_____.     __  .+'|
 *              _.-'""  __    +:"__   | ._..+"" __    .+'   F
 *            J"--.____         __ """""+"         .+'  .J  F
 *            J        """""---.___       --   .+'"     F'  F
 *             L                   """""--...+'    .J       F
 *             L   F"9      --.            |   .   F'      J
 *             L   -_J      L_J      F"9   | ;'J    .+J .J J
 *             |                     L_J   | F.'  .'| J F' J
 *             |        |"""--.__          | '   |""  J    J
 *             J   ._   J ;;; |  "L        |   . |-___J    |
 *             J   L J  J ;-' |   L        | .'J |_  .'  . |
 *             J    ""  J    .---_L  F"9   | F.' | .'   FJ |
 *              L       J .-'  __ |  L_J   | '   :'     ' .+
 *              L       '--.___   |        |       .J   .'
 *              |  F"9         """'        |   .   F' .'
 *              |  -_J      F"9            | .'J    .'
 *              +__         -_J      F"9   | F.'  .'
 *                 """--___          L_J   | '  .'
 *                         """---___       |  .'
 *                                  ""---._|.'
 *
 *  The straightforward way to generate this object creates way more polygons
 *  than are needed, since there end up being many buried, interior faces.
 *  So during the recursive building of the object we store which face of
 *  each unitary cube we need to draw. Doing this reduces the polygon count
 *  by 40% - 60%.
 *
 *  Another optimization we could do to reduce the polygon count would be to
 *  merge adjascent coplanar squares together into rectangles.  This would
 *  result in the outer faces being composed of 1xN strips.  It's tricky to
 *  to find these adjascent faces in non-exponential time, though.
 *
 *  We could actually simulate large depths with a texture map -- if the
 *  depth is such that the smallest holes are only a few pixels across,
 *  just draw them as spots on the surface!  It would look the same.
 */

#define DEFAULTS	"*delay:	 30000          \n" \
			"*showFPS:       False          \n" \
			"*wireframe:     False          \n" \
			"*suppressRotationAnimation: True\n" \


# define refresh_sponge 0
# define release_sponge 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "150"
#define DEF_MAX_DEPTH   "3"

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  GLuint sponge_list0;            /* we store X, Y, and Z-facing surfaces */
  GLuint sponge_list1;            /* in their own lists, to make it easy  */
  GLuint sponge_list2;            /* to color them differently.           */

  unsigned long squares_fp;

  int current_depth;

  int ncolors;
  XColor *colors;
  int ccolor0;
  int ccolor1;
  int ccolor2;

  int draw_tick;

} sponge_configuration;

static sponge_configuration *sps = NULL;

static Bool do_spin;
static Bool do_wander;
static int speed;
static int max_depth;

static XrmOptionDescRec opts[] = {
  { "-wander", ".wander",   XrmoptionNoArg, "True"  },
  { "+wander", ".wander",   XrmoptionNoArg, "False" },
  { "-spin",   ".spin",     XrmoptionSepArg, 0 },
  { "-speed",  ".speed",    XrmoptionSepArg, 0 },
  { "-depth",  ".maxDepth", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,     "spin",     "Spin",     DEF_SPIN,      t_Bool},
  {&do_wander,   "wander",   "Wander",   DEF_WANDER,    t_Bool},
  {&speed,       "speed",    "Speed",    DEF_SPEED,     t_Int},
  {&max_depth,   "maxDepth", "MaxDepth", DEF_MAX_DEPTH, t_Int},
};

ENTRYPOINT ModeSpecOpt sponge_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_sponge (ModeInfo *mi, int width, int height)
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


#define X0 0x01
#define X1 0x02
#define Y0 0x04
#define Y1 0x08
#define Z0 0x10
#define Z1 0x20

static int
cube (float x0, float x1, float y0, float y1, float z0, float z1,
      int faces, int wireframe)
{
  int n = 0;

  if (faces & X0)
    {
      glBegin (wireframe ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (-1.0, 0.0, 0.0);
      glVertex3f (x0, y1, z0);
      glVertex3f (x0, y0, z0);
      glVertex3f (x0, y0, z1);
      glVertex3f (x0, y1, z1);
      glEnd ();
      n++;
    }
  if (faces & X1)
    {
      glBegin (wireframe ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (1.0, 0.0, 0.0);
      glVertex3f (x1, y1, z1);
      glVertex3f (x1, y0, z1);
      glVertex3f (x1, y0, z0);
      glVertex3f (x1, y1, z0);
      glEnd ();
      n++;
    }
  if (faces & Y0)
    {
      glBegin (wireframe ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (0.0, -1.0, 0.0);
      glVertex3f (x0, y0, z0);
      glVertex3f (x0, y0, z1);
      glVertex3f (x1, y0, z1);
      glVertex3f (x1, y0, z0);
      glEnd ();
      n++;
    }
  if (faces & Y1)
    {
      glBegin (wireframe ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (0.0, 1.0, 0.0);
      glVertex3f (x0, y1, z0);
      glVertex3f (x0, y1, z1);
      glVertex3f (x1, y1, z1);
      glVertex3f (x1, y1, z0);
      glEnd ();
      n++;
    }
  if (faces & Z0)
    {
      glBegin (wireframe ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (0.0, 0.0, -1.0);
      glVertex3f (x1, y1, z0);
      glVertex3f (x1, y0, z0);
      glVertex3f (x0, y0, z0);
      glVertex3f (x0, y1, z0);
      glEnd ();
      n++;
    }
  if (faces & Z1)
    {
      glBegin (wireframe ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (0.0, 0.0, 1.0);
      glVertex3f (x0, y1, z1);
      glVertex3f (x0, y0, z1);
      glVertex3f (x1, y0, z1);
      glVertex3f (x1, y1, z1);
      glEnd ();
      n++;
    }

  return n;
}

static int
menger_recurs_1 (int level, float x0, float x1, float y0, float y1,
                 float z0, float z1, int faces, Bool wireframe, 
                 int orig, int forig)
{
  float xi, yi, zi;
  int f, x, y, z;
  int n = 0;

  if (orig)
    {
      if (wireframe)
        n += cube (x0, x1, y0, y1, z0, z1,
                   faces & (X0 | X1 | Y0 | Y1), wireframe);
    }

  if (level == 0)
    {
      if (!wireframe)
        n += cube (x0, x1, y0, y1, z0, z1, faces, wireframe);
    }
  else
    {
      xi = (x1 - x0) / 3;
      yi = (y1 - y0) / 3;
      zi = (z1 - z0) / 3;

      for (x = 0; x < 3; x++)
        for (y = 0; y < 3; y++)
          for (z = 0; z < 3; z++)
            {
              if ((x != 1 && y != 1)
                  || (y != 1 && z != 1)
                  || (x != 1 && z != 1))
                {
                  f = faces;

                  if (x == 1 || (x == 2 && (y != 1 && z != 1)))
                    f &= ~X0;
                  if (x == 1 || (x == 0 && (y != 1 && z != 1)))
                    f &= ~X1;
                  if (forig & X0 && x == 2 && (y == 1 || z == 1))
                    f |= X0;
                  if (forig & X1 && x == 0 && (y == 1 || z == 1))
                    f |= X1;

                  if (y == 1 || (y == 2 && (x != 1 && z != 1)))
                    f &= ~Y0;
                  if (y == 1 || (y == 0 && (x != 1 && z != 1)))
                    f &= ~Y1;
                  if (forig & Y0 && y == 2 && (x == 1 || z == 1))
                    f |= Y0;
                  if (forig & Y1 && y == 0 && (x == 1 || z == 1))
                    f |= Y1;

                  if (z == 1 || (z == 2 && (x != 1 && y != 1)))
                    f &= ~Z0;
                  if (z == 1 || (z == 0 && (x != 1 && y != 1)))
                    f &= ~Z1;
                  if (forig & Z0 && z == 2 && (x == 1 || y == 1))
                    f |= Z0;
                  if (forig & Z1 && z == 0 && (x == 1 || y == 1))
                    f |= Z1;

                  n += menger_recurs_1 (level-1,
                                        x0+x*xi, x0+(x+1)*xi,
                                        y0+y*yi, y0+(y+1)*yi,
                                        z0+z*zi, z0+(z+1)*zi, f, wireframe, 0,
                                        forig);
                }
              else if (wireframe && (x != 1 || y != 1 || z != 1))
                n += cube (x0+x*xi, x0+(x+1)*xi,
                           y0+y*yi, y0+(y+1)*yi,
                           z0+z*zi, z0+(z+1)*zi,
                           forig & (X0 | X1 | Y0 | Y1), wireframe);
            }
    }

  return n;
}

static int
menger_recurs (int level, float x0, float x1, float y0, float y1,
               float z0, float z1, int faces, Bool wireframe, 
               int orig)
{
  return menger_recurs_1 (level, x0, x1, y0, y1, z0, z1, faces, 
                          wireframe, orig, faces);
}


static void
build_sponge (sponge_configuration *sp, Bool wireframe, int level)
{
  glDeleteLists (sp->sponge_list0, 1);
  glNewList(sp->sponge_list0, GL_COMPILE);
  sp->squares_fp = menger_recurs (level, -1.5, 1.5, -1.5, 1.5, -1.5, 1.5,
                                  X0 | X1, wireframe,1);
  glEndList();

  glDeleteLists (sp->sponge_list1, 1);
  glNewList(sp->sponge_list1, GL_COMPILE);
  sp->squares_fp += menger_recurs (level, -1.5, 1.5, -1.5, 1.5, -1.5, 1.5,
                                   Y0 | Y1, wireframe,1);
  glEndList();

  glDeleteLists (sp->sponge_list2, 1);
  glNewList(sp->sponge_list2, GL_COMPILE);
  sp->squares_fp += menger_recurs (level, -1.5, 1.5, -1.5, 1.5, -1.5, 1.5,
                                   Z0 | Z1, wireframe,1);
  glEndList();
}


ENTRYPOINT Bool
sponge_handle_event (ModeInfo *mi, XEvent *event)
{
  sponge_configuration *sp = &sps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, sp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &sp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == '+' || c == '=' ||
          keysym == XK_Up || keysym == XK_Right || keysym == XK_Next)
        {
          sp->draw_tick = speed;
          sp->current_depth += (sp->current_depth > 0 ? 1 : -1);
          sp->current_depth--;
          return True;
        }
      else if (c == '-' || c == '_' ||
               keysym == XK_Down || keysym == XK_Left || keysym == XK_Prior)
        {
          sp->draw_tick = speed;
          sp->current_depth -= (sp->current_depth > 0 ? 1 : -1);
          sp->current_depth--;
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        goto DEF;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
    DEF:
      sp->draw_tick = speed;
      return True;
    }

  return False;
}



ENTRYPOINT void 
init_sponge (ModeInfo *mi)
{
  sponge_configuration *sp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!sps) {
    sps = (sponge_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (sponge_configuration));
    if (!sps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  sp = &sps[MI_SCREEN(mi)];

  if ((sp->glx_context = init_GL(mi)) != NULL) {
    reshape_sponge (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  if (!wire)
    {
      static const GLfloat pos0[4] = {-1.0, -1.0, 1.0, 0.1};
      static const GLfloat pos1[4] = { 1.0, -0.2, 0.2, 0.1};
      static const GLfloat dif0[4] = {1.0, 1.0, 1.0, 1.0};
      static const GLfloat dif1[4] = {1.0, 1.0, 1.0, 1.0};

      glLightfv(GL_LIGHT0, GL_POSITION, pos0);
      glLightfv(GL_LIGHT1, GL_POSITION, pos1);
      glLightfv(GL_LIGHT0, GL_DIFFUSE, dif0);
      glLightfv(GL_LIGHT1, GL_DIFFUSE, dif1);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHT1);

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_NORMALIZE);

      glShadeModel(GL_SMOOTH);
    }

  {
    double spin_speed   = 1.0;
    double wander_speed = 0.03;
    sp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            1.0,
                            do_wander ? wander_speed : 0,
                            True);
    sp->trackball = gltrackball_init (True);
  }

  sp->ncolors = 128;
  sp->colors = (XColor *) calloc(sp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        sp->colors, &sp->ncolors,
                        False, 0, False);
  sp->ccolor0 = 0;
  sp->ccolor1 = sp->ncolors / 3;
  sp->ccolor2 = sp->ccolor1 * 2;

  sp->sponge_list0 = glGenLists (1);
  sp->sponge_list1 = glGenLists (1);
  sp->sponge_list2 = glGenLists (1);

  sp->draw_tick = 9999999;
}


ENTRYPOINT void
draw_sponge (ModeInfo *mi)
{
  sponge_configuration *sp = &sps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  GLfloat color0[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color1[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color2[4] = {0.0, 0.0, 0.0, 1.0};

  if (!sp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (sp->rot, &x, &y, &z, !sp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 6,
                 (z - 0.5) * 15);

    gltrackball_rotate (sp->trackball);

    get_rotation (sp->rot, &x, &y, &z, !sp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  color0[0] = sp->colors[sp->ccolor0].red   / 65536.0;
  color0[1] = sp->colors[sp->ccolor0].green / 65536.0;
  color0[2] = sp->colors[sp->ccolor0].blue  / 65536.0;

  color1[0] = sp->colors[sp->ccolor1].red   / 65536.0;
  color1[1] = sp->colors[sp->ccolor1].green / 65536.0;
  color1[2] = sp->colors[sp->ccolor1].blue  / 65536.0;

  color2[0] = sp->colors[sp->ccolor2].red   / 65536.0;
  color2[1] = sp->colors[sp->ccolor2].green / 65536.0;
  color2[2] = sp->colors[sp->ccolor2].blue  / 65536.0;


  sp->ccolor0++;
  sp->ccolor1++;
  sp->ccolor2++;
  if (sp->ccolor0 >= sp->ncolors) sp->ccolor0 = 0;
  if (sp->ccolor1 >= sp->ncolors) sp->ccolor1 = 0;
  if (sp->ccolor2 >= sp->ncolors) sp->ccolor2 = 0;

  if (sp->draw_tick++ >= speed)
    {
      sp->draw_tick = 0;
      if (sp->current_depth >= max_depth)
        sp->current_depth = -max_depth;
      sp->current_depth++;
      build_sponge (sp,
                    MI_IS_WIREFRAME(mi),
                    (sp->current_depth < 0
                     ? -sp->current_depth : sp->current_depth));

      mi->polygon_count = sp->squares_fp;  /* for FPS display */
      mi->recursion_depth = (sp->current_depth < 0
                             ? -sp->current_depth : sp->current_depth);
    }

  glScalef (2.0, 2.0, 2.0);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color0);
  glCallList (sp->sponge_list0);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
  glCallList (sp->sponge_list1);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
  glCallList (sp->sponge_list2);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("Menger", menger, sponge)

#endif /* USE_GL */
