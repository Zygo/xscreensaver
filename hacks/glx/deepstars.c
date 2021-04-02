/* xscreensaver, Copyright (c) 2019 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef STANDALONE
#define DEFAULTS	"*delay:			30000   \n"	\
			"*showFPS:			False   \n" \
			"*suppressRotationAnimation: True\n" \

# define release_deepstars 0
# include "xlockmore.h"			/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#include "sphere.h"
#include "gltrackball.h"

#define DEF_SPEED "1.0"
#define DEF_SMEAR "1.0"
#define SMEAR_BASE 400
#define SPEED_BASE 0.02

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static GLfloat speed_arg, smear_arg;

static XrmOptionDescRec opts[] = {
  {"-speed",  ".speed",  XrmoptionSepArg, 0 },
  {"-smear",  ".smear",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed_arg, "speed" ,"Speed", DEF_SPEED, t_Float},
  {&smear_arg, "smear" ,"Smear", DEF_SMEAR, t_Float},
};

ENTRYPOINT ModeSpecOpt deepstars_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   deepstars_description =
{"deepstars", "init_deepstars", "draw_deepstars", NULL,
 "draw_deepstars", "init_deepstars", "free_deepstars", &deepstars_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Animates texture mapped sphere (deepstars)", 0, NULL};
#endif

typedef struct {
  GLfloat *colors;
  GLuint starlist, groundlist;
  int ncolors, starcount, groundcount;
  GLXContext *glx_context;
  GLfloat z, latitude, facing;
  int smear, dsmear;
  trackball_state *trackball;
  Bool button_down_p;
} starstruct;

static starstruct *deepstarss = NULL;


ENTRYPOINT void
reshape_deepstars (ModeInfo *mi, int width, int height)
{
  starstruct *gp = &deepstarss[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *gp->glx_context);

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT Bool
deepstars_handle_event (ModeInfo *mi, XEvent *event)
{
  starstruct *gp = &deepstarss[MI_SCREEN(mi)];

  /* Neutralize any horizontal motion, and flip Y */
  GLfloat rot = current_device_rotation();
  Bool rotp = ((rot >  45 && rot <  135) ||
               (rot < -45 && rot > -135));

  if (event->xany.type == ButtonPress ||
      event->xany.type == ButtonRelease)
    {
      if (rotp)
        {
          event->xbutton.y = MI_HEIGHT(mi) / 2;
          event->xbutton.x = MI_WIDTH(mi) - event->xbutton.x;
        }
      else
        {
          event->xbutton.x = MI_WIDTH(mi) / 2;
          event->xbutton.y = MI_HEIGHT(mi) - event->xbutton.y;
        }
    }
  else if (event->xany.type == MotionNotify)
    {
      if (rotp)
        {
          event->xmotion.y = MI_HEIGHT(mi) / 2;
          event->xmotion.x = MI_WIDTH(mi) - event->xmotion.x;
        }
      else
        {
          event->xmotion.x = MI_WIDTH(mi) / 2;
          event->xmotion.y = MI_HEIGHT(mi) - event->xmotion.y;
        }
    }

  if (gltrackball_event_handler (event, gp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &gp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      if (gp->smear <= 1)
        gp->dsmear = 1;
      else
        gp->dsmear = gp->smear = 0;
      return True;
    }

  return False;
}


ENTRYPOINT void
init_deepstars (ModeInfo * mi)
{
  starstruct *gp;
  int screen = MI_SCREEN(mi);

  int i, j, k;
  int width  = MI_WIDTH(mi);
  int height = MI_HEIGHT(mi);
  int size = (width > height ? width : height);
  int nstars = size * size / 80;
  int max_size = 3;
  GLfloat inc = 0.5;
  int sizes = max_size / inc;
  GLfloat scale = 1;

  MI_INIT (mi, deepstarss);
  gp = &deepstarss[screen];

  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape_deepstars(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

# ifdef HAVE_MOBILE
  scale *= 3;
  nstars /= 3;
# else /* !HAVE_MOBILE */
  if (MI_WIDTH(mi) > 2560) {  /* Retina displays */
    scale *= 2;
    nstars /= 2;
  }
# endif /* !HAVE_MOBILE */


  gp->trackball = gltrackball_init (True);

  gp->latitude = 10 + frand(70);
  gp->facing = 10 * (frand(1.0)-0.5);

  /* Only need a small number of distinct star colors, and we have one
     display list for each color, so we can modify the alpha. 
   */
  gp->ncolors = 16;
  gp->colors = (GLfloat *) malloc (4 * gp->ncolors * sizeof(*gp->colors));

  for (i = 0; i < gp->ncolors; i++)
    {
      GLfloat d = 0.1;
      GLfloat r = 0.15 + frand(0.3);
      GLfloat g = r + frand(d) - d;
      GLfloat b = r + frand(d) - d;
      gp->colors[i*4+0] = r;
      gp->colors[i*4+1] = g;
      gp->colors[i*4+2] = b;
      gp->colors[i*4+3] = 1;
    }

  gp->starcount = nstars / gp->ncolors;
  gp->starlist = glGenLists(gp->ncolors);
  for (i = 0; i < gp->ncolors; i++)
    {
      glNewList (gp->starlist + i, GL_COMPILE);
      for (j = 1; j <= sizes; j++)
        {
          glPointSize (inc * j * scale);
          glBegin (GL_POINTS);
          for (k = 0; k < gp->starcount / sizes; k++)
            {
              GLfloat x = frand(1)-0.5;
              GLfloat y = frand(1)-0.5;
              GLfloat z = ((random() & 1)
                           ? frand(1)-0.5
                           : (BELLRAND(1)-0.5)/20);   /* milky way */
              GLfloat d = sqrt (x*x + y*y + z*z);
              x /= d;
              y /= d;
              z /= d;
              glVertex3f (x, y, z);
            }
          glEnd();
        }
      glEndList();
    }

  glDisable (GL_BLEND);
  gp->groundlist = glGenLists(1);
  glNewList(gp->groundlist, GL_COMPILE);
  {
    GLfloat inc = 0.5;
    glColor3f (0.02, 0.02, 0.05);
    glBegin (GL_QUAD_STRIP);
    gp->groundcount = 50;
    for (i = 0; i <= gp->groundcount; i++)
      {
        glVertex3f (i / (GLfloat) gp->groundcount, 0, 0);
        glVertex3f (i / (GLfloat) gp->groundcount, inc, 0);
        inc += 0.1 * (frand(1.0) - 0.5);
      }
    glEnd();
  }
  glEndList();
}


ENTRYPOINT void
draw_deepstars (ModeInfo * mi)
{
  starstruct *gp = &deepstarss[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int smear_change = 800;
  int sky_scale = 60;
  int i, j;

  if (!gp->glx_context)
	return;

  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glXMakeCurrent (dpy, window, *gp->glx_context);

  mi->polygon_count = 0;

  glEnable (GL_LINE_SMOOTH);
  glEnable (GL_POINT_SMOOTH);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);

  glPushMatrix();

  gltrackball_rotate (gp->trackball);

  /* At the equator, Polaris is on the horizon. In the Arctic, overhead. */
  glRotatef (180 - gp->latitude, 1, 0, 0);
  glRotatef (gp->facing, 0, 1, 0);

  if (gp->dsmear == 0 && !(random() % smear_change))
    gp->dsmear = 1;
  else if (gp->smear == SMEAR_BASE * smear_arg && !(random() % smear_change))
    gp->dsmear = -1;

  if (! gp->button_down_p)
    gp->smear += gp->dsmear;
  if (gp->smear < 1) gp->smear = 1;
  else if (gp->smear > SMEAR_BASE * smear_arg)
    gp->smear = SMEAR_BASE * smear_arg;

  if (!gp->button_down_p)
    gp->z -= SPEED_BASE * speed_arg;

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for (i = 0; i < gp->smear; i++)
    {
      GLfloat alpha = 1 - (i / (GLfloat) gp->smear);

      glPushMatrix();

      glRotatef (gp->z - (-i * SPEED_BASE * speed_arg), 0, 0, 1);

# if 0
      if (i == 0)
        {
          glBegin(GL_LINES);
          glVertex3f(0,0,0); glVertex3f(0,0,-3);
          glVertex3f(0,-1,0); glVertex3f(0,1,0);
          glVertex3f(-1,0,0); glVertex3f(1,0,0);
          glEnd();

          glPushMatrix();
          glRotatef (90, 1, 0, 0);
          glScalef (sky_scale, sky_scale, sky_scale);
          mi->polygon_count += unit_sphere (12, 24, 1);
          glPopMatrix();
        }
# endif

      glRotatef (50, 1, 0, 0);  /* Tilt milky way */
      glScalef (sky_scale, sky_scale, sky_scale);

      for (j = 0; j < gp->ncolors; j++)
        {
          gp->colors[j*4+3] = alpha;
          glColor4fv (&gp->colors[j*4]);
          glCallList (gp->starlist + j);
          mi->polygon_count += gp->starcount;
        }
      glPopMatrix();
    }

  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  {
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    {
      glLoadIdentity();
      glTranslatef (-1, -1, 0);
      glScalef (2, 0.7, 1);
      glCallList (gp->groundlist);
      mi->polygon_count += gp->groundcount;
    }
    glPopMatrix();
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_deepstars (ModeInfo * mi)
{
  starstruct *gp = &deepstarss[MI_SCREEN(mi)];

  if (!gp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *gp->glx_context);
  if (gp->colors) free (gp->colors);
  if (glIsList(gp->groundlist))  glDeleteLists(gp->groundlist, 1);
  if (glIsList(gp->starlist))    glDeleteLists(gp->starlist, gp->ncolors);
  if (gp->trackball) gltrackball_free (gp->trackball);
}


XSCREENSAVER_MODULE ("DeepStars", deepstars)

#endif
