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

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS					"Sierpinski3D"
# define HACK_INIT					init_gasket
# define HACK_DRAW					draw_gasket
# define gasket_opts				xlockmore_opts
# define DEFAULTS	"*count:		1       \n"			\
			"*cycles:		9999    \n"			\
			"*delay:		20000   \n"			\
			"*maxDepth:		5       \n"			\
			"*speed:		150     \n"			\
			"*wireframe:	False	\n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int max_depth;
static int speed;
static XrmOptionDescRec opts[] = {
  {"-depth", ".sierpinski3d.maxDepth", XrmoptionSepArg, (caddr_t) 0 },
  {"-speed", ".sierpinski3d.speed",    XrmoptionSepArg, (caddr_t) 0 }
};

static argtype vars[] = {
  {(caddr_t *) &max_depth, "maxDepth", "MaxDepth", "5", t_Int},
  {(caddr_t *) &speed,     "speed",    "Speed",   "150", t_Int},
};


ModeSpecOpt gasket_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   gasket_description =
{"gasket", "init_gasket", "draw_gasket", "release_gasket",
 "draw_gasket", "init_gasket", NULL, &gasket_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Shows GL's Sierpinski gasket", 0, NULL};

#endif

typedef struct{
  GLfloat x;
  GLfloat y;
  GLfloat z;
} GL_VECTOR;

typedef struct {
  GLfloat     view_rotx, view_roty, view_rotz;
  GLfloat     light_colour[4];/* = {6.0, 6.0, 6.0, 1.0}; */
  GLfloat     pos[3];/* = {0.0, 0.0, 0.0}; */
  GLfloat     xinc,yinc,zinc;
  GLfloat     angle;
  GLuint      gasket1;
  GLXContext *glx_context;
  Window      window;

  int current_depth;

} gasketstruct;

static gasketstruct *gasket = NULL;

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
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  static int tick = 0;
  
  static float position0[] = {-0.5,  1.2, 0.5, 0.0};
  static float ambient0[]  = {0.4, 0.6, 0.4, 1.0};
  static float spec[]      = {0.7, 0.7, 0.7, 1.0};

  glLightfv(GL_LIGHT0, GL_POSITION,  position0);
  glLightfv(GL_LIGHT0, GL_AMBIENT,   ambient0);
  glLightfv(GL_LIGHT0, GL_SPECULAR,  spec);
  glLightfv(GL_LIGHT0, GL_DIFFUSE,   gp->light_colour);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glTranslatef( gp->pos[0], gp->pos[1], gp->pos[2] );  

  glPushMatrix();
  glRotatef(2*gp->angle, 1.0, 0.0, 0.0);
  glRotatef(3*gp->angle, 0.0, 1.0, 0.0);
  glRotatef(  gp->angle, 0.0, 0.0, 1.0);
  glScalef( 8.0, 8.0, 8.0 );
  glCallList(gp->gasket1);
  
  glPopMatrix();

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

      /* do the colour change */
      gp->light_colour[0] = 3.0*SINF(gp->angle/20.0) + 4.0;
      gp->light_colour[1] = 3.0*SINF(gp->angle/30.0) + 4.0;
      gp->light_colour[2] = 3.0*SINF(gp->angle/60.0) + 4.0;
    }
}


/* new window size or exposure */
static void
reshape(int width, int height)
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
  
  /* The depth buffer will be cleared, if needed, before the
  * next frame.  Right now we just want to black the screen.
  */
  glClear(GL_COLOR_BUFFER_BIT);
}

static void
pinit(ModeInfo *mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];

  gp->xinc = 0.1*(1.0*random()/RAND_MAX);
  gp->yinc = 0.1*(1.0*random()/RAND_MAX);
  gp->zinc = 0.1*(1.0*random()/RAND_MAX);
  gp->light_colour[0] = 6.0;
  gp->light_colour[1] = 6.0;
  gp->light_colour[2] = 6.0;
  gp->light_colour[3] = 1.0;
  gp->pos[0] = 0.0;     
  gp->pos[1] = 0.0;
  gp->pos[2] = 0.0;    
  /* draw the gasket */
  gp->gasket1 = glGenLists(1);
  gp->current_depth = 1;       /* start out at level 1, not 0 */
  glNewList(gp->gasket1, GL_COMPILE);
    compile_gasket(mi);
  glEndList();
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
  gp->view_rotx = NRAND(360);
  gp->view_roty = NRAND(360);
  gp->view_rotz = NRAND(360);
  gp->angle = NRAND(360)/90.0;

  if ((gp->glx_context = init_GL(mi)) != NULL)
  {
    reshape(MI_WIDTH(mi), MI_HEIGHT(mi));
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
  int           rot_incr = 1;/*MI_COUNT(mi) ? MI_COUNT(mi) : 1;*/

  if (!gp->glx_context) return;

  glDrawBuffer(GL_BACK);

  if (max_depth > 10)
    max_depth = 10;

  glXMakeCurrent(display, window, *(gp->glx_context));
  draw(mi);

  /* rotate */
  gp->angle = (int) (gp->angle + angle_incr) % 360;
  if ( FABSF( gp->pos[0] ) > 8.0 ) gp->xinc = -1.0 * gp->xinc;
  if ( FABSF( gp->pos[1] ) > 6.0 ) gp->yinc = -1.0 * gp->yinc;
  if ( FABSF( gp->pos[2] ) >15.0 ) gp->zinc = -1.0 * gp->zinc;
  gp->pos[0] += gp->xinc;
  gp->pos[1] += gp->yinc;
  gp->pos[2] += gp->zinc;    
  gp->view_rotx = (int) (gp->view_rotx + rot_incr) % 360;
  gp->view_roty = (int) (gp->view_roty +(rot_incr/2.0)) % 360;
  gp->view_rotz = (int) (gp->view_rotz +(rot_incr/3.0)) % 360;

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

      if (gp->glx_context)
      {
	/* Display lists MUST be freed while their glXContext is current. */
        glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

        if (glIsList(gp->gasket1)) glDeleteLists(gp->gasket1, 1);
      }
    }
    (void) free((void *) gasket);
    gasket = NULL;
  }
  FreeAllGL(mi);
}


/*********************************************************/

#endif
