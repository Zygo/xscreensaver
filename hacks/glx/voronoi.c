/* voronoi, Copyright (c) 2007-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS        "*delay:        20000              \n" \
                        "*showFPS:      False              \n" \
			"*suppressRotationAnimation: True\n" \

# define release_voronoi 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)


#include "xlockmore.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_POINTS      "25"
#define DEF_POINT_SIZE  "9"
#define DEF_POINT_SPEED "1.0"
#define DEF_POINT_DELAY "0.05"
#define DEF_ZOOM_SPEED  "1.0"
#define DEF_ZOOM_DELAY  "15"

typedef struct node {
  GLfloat x, y;
  GLfloat dx, dy;
  GLfloat ddx, ddy;
  struct node *next;
  GLfloat color[4], color2[4];
  int rot;
} node;

typedef struct {
  GLXContext *glx_context;
  node *nodes;
  int nnodes;
  node *dragging;
  int ncolors;
  XColor *colors;
  int point_size;

  enum { MODE_WAITING, MODE_ADDING, MODE_ZOOMING } mode;
  int adding;
  double last_time;

  GLfloat zooming;         /* 1.0 starting zoom, 0.0 no longer zooming. */
  GLfloat zoom_toward[2];

} voronoi_configuration;

static voronoi_configuration *vps = NULL;

/* command line arguments */
static int npoints;
static GLfloat point_size, point_speed, point_delay;
static GLfloat zoom_speed, zoom_delay;

static XrmOptionDescRec opts[] = {
  { "-points",       ".points",      XrmoptionSepArg, 0 },
  { "-point-size",   ".pointSize",   XrmoptionSepArg, 0 },
  { "-point-speed",  ".pointSpeed",  XrmoptionSepArg, 0 },
  { "-point-delay",  ".pointDelay",  XrmoptionSepArg, 0 },
  { "-zoom-speed",   ".zoomSpeed",   XrmoptionSepArg, 0 },
  { "-zoom-delay",   ".zoomDelay",   XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&npoints,      "points",      "Points",      DEF_POINTS,       t_Int},
  {&point_size,   "pointSize",   "PointSize",   DEF_POINT_SIZE,   t_Float},
  {&point_speed,  "pointSpeed",  "PointSpeed",  DEF_POINT_SPEED,  t_Float},
  {&point_delay,  "pointDelay",  "PointDelay",  DEF_POINT_DELAY,  t_Float},
  {&zoom_speed,   "zoomSpeed",   "ZoomSpeed",   DEF_ZOOM_SPEED,   t_Float},
  {&zoom_delay,   "zoomDelay",   "ZoomDelay",   DEF_ZOOM_DELAY,   t_Float},
};

ENTRYPOINT ModeSpecOpt voronoi_opts =
  {countof(opts), opts, countof(vars), vars, NULL};


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static node *
add_node (voronoi_configuration *vp, GLfloat x, GLfloat y)
{
  node *nn = (node *) calloc (1, sizeof (*nn));
  int i;
  nn->x = x;
  nn->y = y;

  i = random() % vp->ncolors;
  nn->color[0] = vp->colors[i].red   / 65536.0;
  nn->color[1] = vp->colors[i].green / 65536.0;
  nn->color[2] = vp->colors[i].blue  / 65536.0;
  nn->color[3] = 1.0;

  nn->color2[0] = nn->color[0] * 0.7;
  nn->color2[1] = nn->color[1] * 0.7;
  nn->color2[2] = nn->color[2] * 0.7;
  nn->color2[3] = 1.0;

  nn->ddx = frand (0.000001 * point_speed) * (random() & 1 ? 1 : -1);
  nn->ddy = frand (0.000001 * point_speed) * (random() & 1 ? 1 : -1);

  nn->rot = (random() % 360) * (random() & 1 ? 1 : -1);

  nn->next = vp->nodes;
  vp->nodes = nn;
  vp->nnodes++;
  return nn;
}


static int
cone (void)
{
  int i;
  int faces = 64;
  GLfloat step = M_PI * 2 / faces;
  GLfloat th = 0;
  GLfloat x = 1;
  GLfloat y = 0;

  glBegin(GL_TRIANGLE_FAN);
  glVertex3f (0, 0, 1);
  for (i = 0; i < faces; i++)
    {
      glVertex3f (x, y, 0);
      th += step;
      x = cos (th);
      y = sin (th);
    }
  glVertex3f (1, 0, 0);
  glEnd();
  return faces;
}


static void
move_points (voronoi_configuration *vp)
{
  node *nn;
  for (nn = vp->nodes; nn; nn = nn->next)
    {
      if (nn == vp->dragging) continue;
      nn->x  += nn->dx;
      nn->y  += nn->dy;

      if (vp->mode == MODE_WAITING)
        {
          nn->dx += nn->ddx;
          nn->dy += nn->ddy;
        }
    }
}


static void
prune_points (voronoi_configuration *vp)
{
  node *nn;
  node *prev = 0;
  int lim = 5;

  for (nn = vp->nodes; nn; prev = nn, nn = (nn ? nn->next : 0))
    if (nn->x < -lim || nn->x > lim ||
        nn->y < -lim || nn->y > lim)
      {
        if (prev)
          prev->next = nn->next;
        else
          vp->nodes = nn->next;
        free (nn);
        vp->nnodes--;
        nn = prev;
     }
}


static void
zoom_points (voronoi_configuration *vp)
{
  node *nn;

  GLfloat tick = sin (vp->zooming * M_PI);
  GLfloat scale = 1 + (tick * 0.02 * zoom_speed);

  vp->zooming -= (0.01 * zoom_speed);
  if (vp->zooming < 0) vp->zooming = 0;

  if (vp->zooming <= 0) return;

  if (scale < 1) scale = 1;

  for (nn = vp->nodes; nn; nn = nn->next)
    {
      GLfloat x = nn->x - vp->zoom_toward[0];
      GLfloat y = nn->y - vp->zoom_toward[1];
      x *= scale;
      y *= scale;
      nn->x = x + vp->zoom_toward[0];
      nn->y = y + vp->zoom_toward[1];
    }
}



static void
draw_cells (ModeInfo *mi)
{
  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];
  node *nn;
  int lim = 5;

  for (nn = vp->nodes; nn; nn = nn->next)
    {
      if (nn->x < -lim || nn->x > lim ||
          nn->y < -lim || nn->y > lim)
        continue;

      glPushMatrix();
      glTranslatef (nn->x, nn->y, 0);
      glScalef (lim*2, lim*2, 1);
      glColor4fv (nn->color);
      mi->polygon_count += cone ();
      glPopMatrix();
    }

  glClear (GL_DEPTH_BUFFER_BIT);

  if (vp->point_size <= 0)
    ;
  else if (vp->point_size < 3)
    {
      glPointSize (vp->point_size);
      for (nn = vp->nodes; nn; nn = nn->next)
        {
          glBegin (GL_POINTS);
          glColor4fv (nn->color2);
          glVertex2f (nn->x, nn->y);
          glEnd();
          mi->polygon_count++;
        }
    }
  else
    {
      for (nn = vp->nodes; nn; nn = nn->next)
        {
          int w = MI_WIDTH (mi);
          int h = MI_HEIGHT (mi);
          int s = vp->point_size;
          int i;

          glColor4fv (nn->color2);
          glPushMatrix();
          glTranslatef (nn->x, nn->y, 0);
          glScalef (1.0 / w * s, 1.0 / h * s, 1);

          glLineWidth (vp->point_size / 10);
          nn->rot += (nn->rot < 0 ? -1 : 1);
          glRotatef (nn->rot, 0, 0, 1);

          glRotatef (180, 0, 0, 1);
          for (i = 0; i < 5; i++)
            {
              glBegin (GL_TRIANGLES);
              glVertex2f (0, 1);
              glVertex2f (-0.2, 0);
              glVertex2f ( 0.2, 0);
              glEnd ();
              glRotatef (360.0/5, 0, 0, 1);
              mi->polygon_count++;
            }
          glPopMatrix();
        }
    }

#if 0
  glPushMatrix();
  glColor3f(1,1,1);
  glBegin(GL_LINE_LOOP);
  glVertex3f(0,0,0);
  glVertex3f(1,0,0);
  glVertex3f(1,1,0);
  glVertex3f(0,1,0);
  glEnd();
  glScalef(0.25, 0.25, 1);
  glBegin(GL_LINE_LOOP);
  glVertex3f(0,0,0);
  glVertex3f(1,0,0);
  glVertex3f(1,1,0);
  glVertex3f(0,1,0);
  glEnd();
  glPopMatrix();
#endif
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_voronoi (ModeInfo *mi, int width, int height)
{
/*  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];*/

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, 1, 1, 0, -1, 1);

# ifdef HAVE_MOBILE	/* So much WTF */
  {
    int rot = current_device_rotation();

    glTranslatef (0.5, 0.5, 0);
    //  glScalef(0.19, 0.19, 0.19);

    if (rot == 180 || rot == -180) {
      glTranslatef (1, 1, 0);
    } else if (rot == 90 || rot == -270) {
      glRotatef (180, 0, 0, 1);
      glTranslatef (0, 1, 0);
    } else if (rot == -90 || rot == 270) {
      glRotatef (180, 0, 0, 1);
      glTranslatef (1, 0, 0);
    }

    glTranslatef(-0.5, -0.5, 0);
  }
# endif

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClear(GL_COLOR_BUFFER_BIT);
}


static node *
find_node (ModeInfo *mi, GLfloat x, GLfloat y)
{
  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];
  int ps = (vp->point_size < 5 ? 5 : vp->point_size);
  GLfloat hysteresis = (1.0 / MI_WIDTH (mi)) * ps;
  node *nn;
  for (nn = vp->nodes; nn; nn = nn->next)
    if (nn->x > x - hysteresis && nn->x < x + hysteresis &&
        nn->y > y - hysteresis && nn->y < y + hysteresis)
      return nn;
  return 0;
}


ENTRYPOINT Bool
voronoi_handle_event (ModeInfo *mi, XEvent *event)
{
  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress)
    {
      GLfloat x = (GLfloat) event->xbutton.x / MI_WIDTH (mi);
      GLfloat y = (GLfloat) event->xbutton.y / MI_HEIGHT (mi);
      node *nn = find_node (mi, x, y);
      if (!nn)
        nn = add_node (vp, x, y);
      vp->dragging = nn;

      return True;
    }
  else if (event->xany.type == ButtonRelease && vp->dragging)
    {
      vp->dragging = 0;
      return True;
    }
  else if (event->xany.type == MotionNotify && vp->dragging)
    {
      vp->dragging->x = (GLfloat) event->xmotion.x / MI_WIDTH (mi);
      vp->dragging->y = (GLfloat) event->xmotion.y / MI_HEIGHT (mi);
      return True;
    }

  return False;
}

static void
state_change (ModeInfo *mi)
{
  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];
  double now = double_time();

  if (vp->dragging)
    {
      vp->last_time = now;
      vp->adding = 0;
      vp->zooming = 0;
      return;
    }

  switch (vp->mode)
    {
    case MODE_WAITING:
      if (vp->last_time + zoom_delay <= now)
        {
          node *tn = vp->nodes;
          vp->zoom_toward[0] = (tn ? tn->x : 0.5);
          vp->zoom_toward[1] = (tn ? tn->y : 0.5);

          vp->mode = MODE_ZOOMING;
          vp->zooming = 1;

          vp->last_time = now;
        }
      break;

    case MODE_ADDING:
      if (vp->last_time + point_delay <= now)
        {
          add_node (vp, 
                    BELLRAND(0.5) + 0.25, 
                    BELLRAND(0.5) + 0.25);
          vp->last_time = now;
          vp->adding--;
          if (vp->adding <= 0)
            {
              vp->adding = 0;
              vp->mode = MODE_WAITING;
              vp->last_time = now;
            }
        }
      break;

    case MODE_ZOOMING:
      {
        zoom_points (vp);
        if (vp->zooming <= 0)
          {
            vp->mode = MODE_ADDING;
            vp->adding = npoints;
            vp->last_time = now;
          }
      }
      break;

    default:
      abort();
    }
}


ENTRYPOINT void 
init_voronoi (ModeInfo *mi)
{
  voronoi_configuration *vp;

  MI_INIT (mi, vps);

  vp = &vps[MI_SCREEN(mi)];

  vp->glx_context = init_GL(mi);

  vp->point_size = point_size;
  if (vp->point_size < 0) vp->point_size = 10;

  if (MI_WIDTH(mi) > 2560) vp->point_size *= 2;  /* Retina displays */

  vp->ncolors = 128;
  vp->colors = (XColor *) calloc (vp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        vp->colors, &vp->ncolors,
                        False, False, False);

  reshape_voronoi (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  vp->mode = MODE_ADDING;
  vp->adding = npoints * 2;
  vp->last_time = 0;
}


ENTRYPOINT void
draw_voronoi (ModeInfo *mi)
{
  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!vp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *vp->glx_context);

  glShadeModel(GL_FLAT);
  glEnable(GL_POINT_SMOOTH);
/*  glEnable(GL_LINE_SMOOTH);*/
/*  glEnable(GL_POLYGON_SMOOTH);*/

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;
  draw_cells (mi);
  move_points (vp);
  prune_points (vp);
  state_change (mi);

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_voronoi (ModeInfo *mi)
{
  voronoi_configuration *vp = &vps[MI_SCREEN(mi)];
  node *n;
  if (!vp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *vp->glx_context);
  if (vp->colors) free (vp->colors);
  n = vp->nodes;
  while (n) {
    node *n2 = n->next;
    free (n);
    n = n2;
  }
}

XSCREENSAVER_MODULE ("Voronoi", voronoi)

#endif /* USE_GL */
