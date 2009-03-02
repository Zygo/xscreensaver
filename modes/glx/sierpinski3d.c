/* -*- Mode: C; tab-width: 4 -*- */
/* Sierpinski3D --- 3D sierpinski gasket */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sierpinski3D.c	00.01 99/11/04 xlockmore";

#endif

/*-
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 *
 * 06-Apr-2001  ported from Xscreensaver by Rolf Groppe <rolf@groppe.de>
 *
 * 1999: written by Tim Robinson <the_luggage@bigfoot.com>
 *       a 3-D representation of the Sierpinski gasket fractal.
 *
 * 10-Dec-99  jwz   rewrote to draw a set of tetrahedrons instead of a
 *                  random scattering of points.
 */

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#ifdef VMS
#include "vms_x_fix.h"
#include <X11/Intrinsic.h>
#endif

#ifdef STANDALONE
# define MODE_sierpinski3d
# define PROGCLASS					"Sierpinski3D"
# define HACK_INIT					init_gasket
# define HACK_DRAW					draw_gasket
# define HACK_RESHAPE				refesh_gasket
# define gasket_opts				xlockmore_opts
# define DEFAULTS					"*count:		1       \n"			\
									"*cycles:		9999    \n"			\
									"*delay:		15000   \n"			\
									"*maxDepth:		5       \n"			\
									"*speed:		150     \n"			\
									"*showFPS:      False   \n"			\
									"*wireframe:	False	\n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#include "visgl.h"
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_sierpinski3d

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int max_depth;
static int speed;
static int intensity;
static float intens_factor;

static XrmOptionDescRec opts[] = {
  {(char *) "-maxdepth", (char *) ".sierpinski3d.maxdepth", XrmoptionSepArg, (caddr_t) 0 },
  {(char *) "-speed", (char *) ".sierpinski3d.speed",    XrmoptionSepArg, (caddr_t) 0 },
  {(char *) "-intensity",(char *) ".sierpinski3d.intensity", XrmoptionSepArg, (caddr_t) 0}
};

static argtype vars[] = {
  {(void *) &max_depth, (char *) "maxdepth", (char *) "MaxDepth",
     (char *) "5", t_Int},
  {(void *) &speed, (char *) "speed", (char *) "Speed",
     (char *) "150", t_Int},
  {(void *) &intensity, (char *) "intensity", (char *) "Intensity",
     (char *) "2185", t_Int}
};

static OptionStruct desc[] = {
  {(char *) "-maxdepth", (char *) "maximum depth"},
  {(char *) "-speed", (char *) "speed"},
  {(char *) "-intensity", (char *) "intensity"}
};

ModeSpecOpt gasket_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct sierpinski3d_description =
{"sierpinski3d", "init_gasket", "draw_gasket", "release_gasket",
 "draw_gasket", "init_gasket", (char *) NULL, &gasket_opts,
 15000, 1, 2, 1, 64, 1.0, "",
 "Shows GL's Sierpinski gasket", 0, NULL};

#endif

#define FLOATRAND(a) (((double)LRAND() / (double)MAXRAND) * a)

typedef struct{
  GLfloat x;
  GLfloat y;
  GLfloat z;
} GL_VECTOR;

typedef struct {
  GLfloat rotx, roty, rotz;	   /* current object rotation */
  GLfloat dx, dy, dz;		   /* current rotational velocity */
  GLfloat ddx, ddy, ddz;	   /* current rotational acceleration */
  GLfloat d_max;			   /* max velocity */

  GLfloat     angle;
  GLuint      gasket1;
  GLXContext *glx_context;
  Window      window;

  int current_depth;

  int ncolors;
  XColor *colors;
  int ccolor;

} gasketstruct;

static gasketstruct *gasket = (gasketstruct *) NULL;

#include <GL/glu.h>

/* static GLuint limit; */


/* Computing normal vectors (thanks to Nat Friedman <ndf@mit.edu>)
 */

typedef struct vector {
  GLfloat x, y, z;
} vector;

typedef struct plane {
  vector p1, p2, p3;
} plane;

static void
vector_set(vector *v, GLfloat x, GLfloat y, GLfloat z)
{
  v->x = x;
  v->y = y;
  v->z = z;
}

static void
vector_cross(vector v1, vector v2, vector *v3)
{
  v3->x = (v1.y * v2.z) - (v1.z * v2.y);
  v3->y = (v1.z * v2.x) - (v1.x * v2.z);
  v3->z = (v1.x * v2.y) - (v1.y * v2.x);
}

static void
vector_subtract(vector v1, vector v2, vector *res)
{
  res->x = v1.x - v2.x;
  res->y = v1.y - v2.y;
  res->z = v1.z - v2.z;
}

static void
plane_normal(plane p, vector *n)
{
  vector v1, v2;
  vector_subtract(p.p1, p.p2, &v1);
  vector_subtract(p.p1, p.p3, &v2);
  vector_cross(v2, v1, n);
}

static void
do_normal(GLfloat x1, GLfloat y1, GLfloat z1,
	  GLfloat x2, GLfloat y2, GLfloat z2,
	  GLfloat x3, GLfloat y3, GLfloat z3)
{
  plane plane;
  vector n;
  vector_set(&plane.p1, x1, y1, z1);
  vector_set(&plane.p2, x2, y2, z2);
  vector_set(&plane.p3, x3, y3, z3);
  plane_normal(plane, &n);
  n.x = -n.x; n.y = -n.y; n.z = -n.z;

  glNormal3f(n.x, n.y, n.z);

#ifdef DEBUG
  /* Draw a line in the direction of this face's normal. */
  {
    GLfloat ax = n.x > 0 ? n.x : -n.x;
    GLfloat ay = n.y > 0 ? n.y : -n.y;
    GLfloat az = n.z > 0 ? n.z : -n.z;
    GLfloat mx = (x1 + x2 + x3) / 3;
    GLfloat my = (y1 + y2 + y3) / 3;
    GLfloat mz = (z1 + z2 + z3) / 3;
    GLfloat xx, yy, zz;

    GLfloat max = ax > ay ? ax : ay;
    if (az > max) max = az;
    max *= 2;
    xx = n.x / max;
    yy = n.y / max;
    zz = n.z / max;

    glBegin(GL_LINE_LOOP);
    glVertex3f(mx, my, mz);
    glVertex3f(mx+xx, my+yy, mz+zz);
    glEnd();
  }
#endif /* DEBUG */
}



static void
triangle (GLfloat x1, GLfloat y1, GLfloat z1,
          GLfloat x2, GLfloat y2, GLfloat z2,
          GLfloat x3, GLfloat y3, GLfloat z3,
          Bool wireframe_p)
{
  if (wireframe_p)
    glBegin (GL_LINE_LOOP);
  else
    {
      do_normal (x1, y1, z1,  x2, y2, z2,  x3, y3, z3);
      glBegin (GL_TRIANGLES);
    }
  glVertex3f (x1, y1, z1);
  glVertex3f (x2, y2, z2);
  glVertex3f (x3, y3, z3);
  glEnd();
}

static void
four_tetras (GL_VECTOR *outer, Bool wireframe_p, int countdown)
{
  if (countdown <= 0)
    {
      triangle (outer[0].x, outer[0].y, outer[0].z,
                outer[1].x, outer[1].y, outer[1].z,
                outer[2].x, outer[2].y, outer[2].z,
                wireframe_p);
      triangle (outer[0].x, outer[0].y, outer[0].z,
                outer[3].x, outer[3].y, outer[3].z,
                outer[1].x, outer[1].y, outer[1].z,
                wireframe_p);
      triangle (outer[0].x, outer[0].y, outer[0].z,
                outer[2].x, outer[2].y, outer[2].z,
                outer[3].x, outer[3].y, outer[3].z,
                wireframe_p);
      triangle (outer[1].x, outer[1].y, outer[1].z,
                outer[3].x, outer[3].y, outer[3].z,
                outer[2].x, outer[2].y, outer[2].z,
                wireframe_p);
    }
  else
    {
#     define M01 0
#     define M02 1
#     define M03 2
#     define M12 3
#     define M13 4
#     define M23 5
      GL_VECTOR inner[M23+1];
      GL_VECTOR corner[4];

      inner[M01].x = (outer[0].x + outer[1].x) / 2.0;
      inner[M01].y = (outer[0].y + outer[1].y) / 2.0;
      inner[M01].z = (outer[0].z + outer[1].z) / 2.0;

      inner[M02].x = (outer[0].x + outer[2].x) / 2.0;
      inner[M02].y = (outer[0].y + outer[2].y) / 2.0;
      inner[M02].z = (outer[0].z + outer[2].z) / 2.0;

      inner[M03].x = (outer[0].x + outer[3].x) / 2.0;
      inner[M03].y = (outer[0].y + outer[3].y) / 2.0;
      inner[M03].z = (outer[0].z + outer[3].z) / 2.0;

      inner[M12].x = (outer[1].x + outer[2].x) / 2.0;
      inner[M12].y = (outer[1].y + outer[2].y) / 2.0;
      inner[M12].z = (outer[1].z + outer[2].z) / 2.0;

      inner[M13].x = (outer[1].x + outer[3].x) / 2.0;
      inner[M13].y = (outer[1].y + outer[3].y) / 2.0;
      inner[M13].z = (outer[1].z + outer[3].z) / 2.0;

      inner[M23].x = (outer[2].x + outer[3].x) / 2.0;
      inner[M23].y = (outer[2].y + outer[3].y) / 2.0;
      inner[M23].z = (outer[2].z + outer[3].z) / 2.0;

      countdown--;

      corner[0] = outer[0];
      corner[1] = inner[M01];
      corner[2] = inner[M02];
      corner[3] = inner[M03];
      four_tetras (corner, wireframe_p, countdown);

      corner[0] = inner[M01];
      corner[1] = outer[1];
      corner[2] = inner[M12];
      corner[3] = inner[M13];
      four_tetras (corner, wireframe_p, countdown);

      corner[0] = inner[M02];
      corner[1] = inner[M12];
      corner[2] = outer[2];
      corner[3] = inner[M23];
      four_tetras (corner, wireframe_p, countdown);

      corner[0] = inner[M03];
      corner[1] = inner[M13];
      corner[2] = inner[M23];
      corner[3] = outer[3];
      four_tetras (corner, wireframe_p, countdown);
    }
}


static void
compile_gasket(ModeInfo *mi)
{
  Bool wireframe_p = MI_IS_WIREFRAME(mi);
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];

  GL_VECTOR   vertex[5];

  /* define verticies */
  vertex[0].x =  0.5;
  vertex[0].y = -(1.0/3.0)*sqrt((2.0/3.0));
  vertex[0].z = -sqrt(3.0)/6.0;

  vertex[1].x = -0.5;
  vertex[1].y = -(1.0/3.0)*sqrt((2.0/3.0));
  vertex[1].z = -sqrt(3.0)/6.0;

  vertex[2].x = 0.0;
  vertex[2].y = (2.0/3.0)*sqrt((2.0/3.0));
  vertex[2].z = -sqrt(3.0)/6.0;

  vertex[3].x = 0.0;
  vertex[3].y = 0.0;
  vertex[3].z = sqrt(3.0)/3.0;

  vertex[4].x = 0.0;
  vertex[4].y = 0.0;
  vertex[4].z = 0.0;

  four_tetras (vertex, wireframe_p,
               (gp->current_depth < 0
                ? -gp->current_depth : gp->current_depth));
}

static void
draw(ModeInfo *mi)
{
  Bool wireframe_p = MI_IS_WIREFRAME(mi);
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  static int tick = 0;

  static GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
  static float white[]  = {1.0, 1.0, 1.0, 1.0};
  static float color[]  = {0.0, 0.0, 0.0, 1.0};

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!wireframe_p)
    {
      glColor4fv (white);

      glLightfv(GL_LIGHT0, GL_POSITION,  pos);

      color[0] = gp->colors[gp->ccolor].red  * intens_factor;
      color[1] = gp->colors[gp->ccolor].green * intens_factor;
      color[2] = gp->colors[gp->ccolor].blue * intens_factor;
      gp->ccolor++;
      if (gp->ccolor >= gp->ncolors) gp->ccolor = 0;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

      glShadeModel(GL_SMOOTH);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
    }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glPushMatrix();

  {
    static int frame = 0;
    GLfloat x, y, z;

#   define SINOID(SCALE,SIZE) \
      ((((1 + sin((frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)
    x = SINOID(0.0071, 8.0);
    y = SINOID(0.0053, 6.0);
    z = SINOID(0.0037, 15.0);
    frame++;
    glTranslatef(x, y, z);

    x = gp->rotx;
    y = gp->roty;
    z = gp->rotz;
    if (x < 0) x = 1 - (x + 1);
    if (y < 0) y = 1 - (y + 1);
    if (z < 0) z = 1 - (z + 1);
    glRotatef(x * 360, 1.0, 0.0, 0.0);
    glRotatef(y * 360, 0.0, 1.0, 0.0);
    glRotatef(z * 360, 0.0, 0.0, 1.0);
  }

  glScalef( 8.0, 8.0, 8.0 );
  glCallList(gp->gasket1);

  glPopMatrix();


  if (tick++ >= speed)
    {

      tick = 0;
      if (gp->current_depth >= max_depth)
        gp->current_depth = -max_depth;
      gp->current_depth++;

      glDeleteLists (gp->gasket1, 1);
      glNewList (gp->gasket1, GL_COMPILE);
      compile_gasket (mi);
      glEndList();

    }
}


/* new window size or exposure */
void
reshape_gasket(ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective( 30.0, 1/h, 1.0, 100.0 );
  gluLookAt( 0.0, 0.0, 15.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -15.0);

  glClear(GL_COLOR_BUFFER_BIT);
}

static void
pinit(ModeInfo *mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];

  /* draw the gasket */
  gp->gasket1 = glGenLists(1);
  gp->current_depth = 1;       /* start out at level 1, not 0 */
  glNewList(gp->gasket1, GL_COMPILE);
    compile_gasket(mi);
  glEndList();
}



/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

static void
rotate(GLfloat *pos, GLfloat *v, GLfloat *dv, GLfloat max_v, Bool verbose)
{
  double ppos = *pos;

  /* tick position */
  if (ppos < 0)
    ppos = -(ppos + *v);
  else
    ppos += *v;

  if (ppos > 1.0)
    ppos -= 1.0;
  else if (ppos < 0)
    ppos += 1.0;

  if ((ppos < 0.0) || (ppos > 1.0)) {
    if (verbose) {
      (void) fprintf(stderr, "Weirdness in rotate()\n");
      (void) fprintf(stderr, "ppos = %g\n", ppos);
    }
    return;
  }
  *pos = (*pos > 0 ? ppos : -ppos);

  /* accelerate */
  *v += *dv;

  /* clamp velocity */
  if (*v > max_v || *v < -max_v)
    {
      *dv = -*dv;
    }
  /* If it stops, start it going in the other direction. */
  else if (*v < 0)
    {
      if (random() % 4)
	{
	  *v = 0;

	  /* keep going in the same direction */
	  if (random() % 2)
	    *dv = 0;
	  else if (*dv < 0)
	    *dv = -*dv;
	}
      else
	{
	  /* reverse gears */
	  *v = -*v;
	  *dv = -*dv;
	  *pos = -*pos;
	}
    }

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    *dv = -*dv;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (*dv == 0)
	*dv = 0.00001;
      else if (random() & 1)
	*dv *= 1.2;
      else
	*dv *= 0.8;
    }
}


void
init_gasket(ModeInfo *mi)
{
  int           screen = MI_SCREEN(mi);
  gasketstruct *gp;

  if (gasket == NULL)
  {
    if ((gasket = (gasketstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (gasketstruct))) == NULL)
	return;
  }
  gp = &gasket[screen];

  gp->window = MI_WINDOW(mi);

  gp->rotx = FLOATRAND(1.0) * RANDSIGN();
  gp->roty = FLOATRAND(1.0) * RANDSIGN();
  gp->rotz = FLOATRAND(1.0) * RANDSIGN();

  /* bell curve from 0-1.5 degrees, avg 0.75 */
  gp->dx = (FLOATRAND(1) + FLOATRAND(1) + FLOATRAND(1)) / (360*2);
  gp->dy = (FLOATRAND(1) + FLOATRAND(1) + FLOATRAND(1)) / (360*2);
  gp->dz = (FLOATRAND(1) + FLOATRAND(1) + FLOATRAND(1)) / (360*2);

  gp->d_max = gp->dx * 2;

  gp->ddx = 0.00006 + FLOATRAND(0.00003);
  gp->ddy = 0.00006 + FLOATRAND(0.00003);
  gp->ddz = 0.00006 + FLOATRAND(0.00003);

  gp->ddx = 0.00001;
  gp->ddy = 0.00001;
  gp->ddz = 0.00001;

  intens_factor = intensity / 65536000.0;
  gp->ncolors = 255;
  gp->colors = (XColor *) calloc(gp->ncolors, sizeof(XColor));
  make_smooth_colormap (mi, None,
                        gp->colors, &gp->ncolors,
                        False, (Bool *) NULL);

  if ((gp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_gasket(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    pinit(mi);
  }
  else
  {
    MI_CLEARWINDOW(mi);
  }
}

void
draw_gasket(ModeInfo * mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  Display      *display = MI_DISPLAY(mi);
  Window        window = MI_WINDOW(mi);
  int           angle_incr = 1;

  if (!gp->glx_context) return;

  glDrawBuffer(GL_BACK);

  if (max_depth > 10)
    max_depth = 10;

  MI_IS_DRAWN(mi) = True;

  glXMakeCurrent(display, window, *(gp->glx_context));
  draw(mi);

  /* rotate */
  gp->angle = (int) (gp->angle + angle_incr) % 360;

  rotate(&gp->rotx, &gp->dx, &gp->ddx, gp->d_max, MI_IS_VERBOSE(mi));
  rotate(&gp->roty, &gp->dy, &gp->ddy, gp->d_max, MI_IS_VERBOSE(mi));
  rotate(&gp->rotz, &gp->dz, &gp->ddz, gp->d_max, MI_IS_VERBOSE(mi));

  /* if (mi->fps_p) do_fps (mi); */

  if (MI_IS_FPS(mi)) do_fps (mi);
  glFinish();
  glXSwapBuffers(display, window);
}

void
release_gasket(ModeInfo * mi)
{
  if (gasket != NULL)
  {
    int         screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
    {
      gasketstruct *gp = &gasket[screen];

      if (gp->colors != NULL) {
	XFree((caddr_t) gp->colors);
	gp->colors = (XColor *) NULL;
      }
      if (gp->glx_context)
      {
	/* Display lists MUST be freed while their glXContext is current. */
        glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

        if (glIsList(gp->gasket1)) glDeleteLists(gp->gasket1, 1);
      }
    }
    free(gasket);
    gasket = (gasketstruct *) NULL;
  }
  FreeAllGL(mi);
}


/*********************************************************/

#endif
