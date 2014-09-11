/* -*- Mode: C; tab-width: 4 -*- */
/* Sierpinski3D --- 3D sierpinski gasket */

#if 0
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

#ifdef STANDALONE
# define DEFAULTS					"*delay:		20000   \n"			\
									"*showFPS:      False   \n"			\
									"*wireframe:	False	\n"			\

# define refresh_gasket 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#define DEF_SPIN			        "True"
#define DEF_WANDER			        "True"
#define DEF_SPEED			        "150"
#define DEF_MAX_DEPTH		        "5"

#include "rotator.h"
#include "gltrackball.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int max_depth;
static int speed;
static Bool do_spin;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  {"-depth", ".sierpinski3d.maxDepth", XrmoptionSepArg, 0 },
  {"-speed", ".sierpinski3d.speed",    XrmoptionSepArg, 0 },
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_spin,   "spin",     "Spin",     DEF_SPIN,      t_Bool},
  {&do_wander, "wander",   "Wander",   DEF_WANDER,    t_Bool},
  {&speed,     "speed",    "Speed",    DEF_SPEED,     t_Int},
  {&max_depth, "maxDepth", "MaxDepth", DEF_MAX_DEPTH, t_Int},
};


ENTRYPOINT ModeSpecOpt gasket_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   gasket_description =
{"gasket", "init_gasket", "draw_gasket", "release_gasket",
 "draw_gasket", "init_gasket", NULL, &gasket_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Shows GL's Sierpinski gasket", 0, NULL};

#endif

typedef struct {
  double x,y,z;
} XYZ;

typedef struct {
  GLuint      gasket0, gasket1, gasket2, gasket3;
  GLXContext *glx_context;
  Window      window;
  rotator    *rot;
  trackball_state *trackball;
  Bool		  button_down_p;

  int current_depth;

  int ncolors;
  XColor *colors;
  int ccolor0;
  int ccolor1;
  int ccolor2;
  int ccolor3;

  int tick;

} gasketstruct;

static gasketstruct *gasket = NULL;


static void
triangle (GLfloat x1, GLfloat y1, GLfloat z1,
          GLfloat x2, GLfloat y2, GLfloat z2,
          GLfloat x3, GLfloat y3, GLfloat z3,
          Bool wireframe_p)
{
  if (wireframe_p)
    glBegin (GL_LINE_LOOP);
  else
    glBegin (GL_TRIANGLES);
  glVertex3f (x1, y1, z1);
  glVertex3f (x2, y2, z2);
  glVertex3f (x3, y3, z3);
  glEnd();
}

static void
four_tetras (gasketstruct *gp, 
             XYZ *outer, XYZ *normals,
             Bool wireframe_p, int countdown, int which,
             int *countP)
{
  if (countdown <= 0)
    {
      (*countP)++;
      if (which == 0)
        {
          glNormal3f (normals[0].x, normals[0].y, normals[0].z);
          triangle (outer[0].x, outer[0].y, outer[0].z,
                    outer[1].x, outer[1].y, outer[1].z,
                    outer[2].x, outer[2].y, outer[2].z,
                    wireframe_p);
        }
      else if (which == 1)
        {
          glNormal3f (normals[1].x, normals[1].y, normals[1].z);
          triangle (outer[0].x, outer[0].y, outer[0].z,
                    outer[3].x, outer[3].y, outer[3].z,
                    outer[1].x, outer[1].y, outer[1].z,
                    wireframe_p);
        }
      else if (which == 2)
        {
          glNormal3f (normals[2].x, normals[2].y, normals[2].z);
          triangle (outer[0].x, outer[0].y, outer[0].z,
                    outer[2].x, outer[2].y, outer[2].z,
                    outer[3].x, outer[3].y, outer[3].z,
                    wireframe_p);
        }
      else
        {
          glNormal3f (normals[3].x, normals[3].y, normals[3].z);
          triangle (outer[1].x, outer[1].y, outer[1].z,
                    outer[3].x, outer[3].y, outer[3].z,
                    outer[2].x, outer[2].y, outer[2].z,
                    wireframe_p);
        }
    }
  else
    {
#     define M01 0
#     define M02 1
#     define M03 2
#     define M12 3
#     define M13 4
#     define M23 5
      XYZ inner[M23+1];
      XYZ corner[4];

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
      four_tetras (gp, corner, normals, wireframe_p, countdown, which, countP);

      corner[0] = inner[M01];
      corner[1] = outer[1];
      corner[2] = inner[M12];
      corner[3] = inner[M13];
      four_tetras (gp, corner, normals, wireframe_p, countdown, which, countP);

      corner[0] = inner[M02];
      corner[1] = inner[M12];
      corner[2] = outer[2];
      corner[3] = inner[M23];
      four_tetras (gp, corner, normals, wireframe_p, countdown, which, countP);

      corner[0] = inner[M03];
      corner[1] = inner[M13];
      corner[2] = inner[M23];
      corner[3] = outer[3];
      four_tetras (gp, corner, normals, wireframe_p, countdown, which, countP);
    }
}


static void
compile_gasket(ModeInfo *mi, int which)
{
  Bool wireframe_p = MI_IS_WIREFRAME(mi);
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  int count = 0;
  XYZ vertex[5];
  XYZ normal[4];

  vertex[0].x = -1; vertex[0].y = -1; vertex[0].z = -1;
  vertex[1].x =  1; vertex[1].y =  1; vertex[1].z = -1;
  vertex[2].x =  1; vertex[2].y = -1; vertex[2].z =  1;
  vertex[3].x = -1; vertex[3].y =  1; vertex[3].z =  1;
  vertex[4].x =  0; vertex[4].y =  0; vertex[4].z =  0;  /* center */

  normal[0].x =  1; normal[0].y = -1; normal[0].z = -1;
  normal[1].x = -1; normal[1].y =  1; normal[1].z = -1;
  normal[2].x = -1; normal[2].y = -1; normal[2].z =  1;
  normal[3].x =  1; normal[3].y =  1; normal[3].z =  1;

  four_tetras (gp, vertex, normal, wireframe_p,
               (gp->current_depth < 0
                ? -gp->current_depth : gp->current_depth),
               which,
               &count);
  mi->polygon_count += count;
}

static void
draw(ModeInfo *mi)
{
  Bool wireframe_p = MI_IS_WIREFRAME(mi);
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  
  static const GLfloat pos[]    = {-4.0, 3.0, 10.0, 1.0};
  static const GLfloat white[]  = {1.0, 1.0, 1.0, 1.0};

  GLfloat color0[] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color1[] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color2[] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color3[] = {0.0, 0.0, 0.0, 1.0};

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!wireframe_p)
    {
      glColor4fv (white);

      glLightfv(GL_LIGHT0, GL_POSITION,  pos);

      color0[0] = gp->colors[gp->ccolor0].red   / 65536.0;
      color0[1] = gp->colors[gp->ccolor0].green / 65536.0;
      color0[2] = gp->colors[gp->ccolor0].blue  / 65536.0;

      color1[0] = gp->colors[gp->ccolor1].red   / 65536.0;
      color1[1] = gp->colors[gp->ccolor1].green / 65536.0;
      color1[2] = gp->colors[gp->ccolor1].blue  / 65536.0;

      color2[0] = gp->colors[gp->ccolor2].red   / 65536.0;
      color2[1] = gp->colors[gp->ccolor2].green / 65536.0;
      color2[2] = gp->colors[gp->ccolor2].blue  / 65536.0;

      color3[0] = gp->colors[gp->ccolor3].red   / 65536.0;
      color3[1] = gp->colors[gp->ccolor3].green / 65536.0;
      color3[2] = gp->colors[gp->ccolor3].blue  / 65536.0;

      gp->ccolor0++;
      gp->ccolor1++;
      gp->ccolor2++;
      gp->ccolor3++;
      if (gp->ccolor0 >= gp->ncolors) gp->ccolor0 = 0;
      if (gp->ccolor1 >= gp->ncolors) gp->ccolor1 = 0;
      if (gp->ccolor2 >= gp->ncolors) gp->ccolor2 = 0;
      if (gp->ccolor3 >= gp->ncolors) gp->ccolor3 = 0;

      glShadeModel(GL_SMOOTH);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
    }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glPushMatrix();

  {
    double x, y, z;
    get_position (gp->rot, &x, &y, &z, !gp->button_down_p);
    glTranslatef((x - 0.5) * 10,
                 (y - 0.5) * 10,
                 (z - 0.5) * 20);

    gltrackball_rotate (gp->trackball);

    get_rotation (gp->rot, &x, &y, &z, !gp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  glScalef (4, 4, 4);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color0);
  glCallList(gp->gasket0);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
  glCallList(gp->gasket1);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
  glCallList(gp->gasket2);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color3);
  glCallList(gp->gasket3);

  glPopMatrix();


  if (gp->tick++ >= speed)
    {
      gp->tick = 0;
      if (gp->current_depth >= max_depth)
        gp->current_depth = -max_depth;
      gp->current_depth++;

      /* We make four different lists so that each face of the tetrahedrons
         can have a different color (all triangles facing in the same
         direction have the same color, which is different from all
         triangles facing in other directions.)
       */
      glDeleteLists (gp->gasket0, 1);
      glDeleteLists (gp->gasket1, 1);
      glDeleteLists (gp->gasket2, 1);
      glDeleteLists (gp->gasket3, 1);

      mi->polygon_count = 0;
      glNewList (gp->gasket0, GL_COMPILE); compile_gasket (mi, 0); glEndList();
      glNewList (gp->gasket1, GL_COMPILE); compile_gasket (mi, 1); glEndList();
      glNewList (gp->gasket2, GL_COMPILE); compile_gasket (mi, 2); glEndList();
      glNewList (gp->gasket3, GL_COMPILE); compile_gasket (mi, 3); glEndList();

      mi->recursion_depth = (gp->current_depth > 0
                             ? gp->current_depth
                             : -gp->current_depth);
    }
}


/* new window size or exposure */
ENTRYPOINT void
reshape_gasket(ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  
  glClear(GL_COLOR_BUFFER_BIT);
}

static void
pinit(ModeInfo *mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];

  /* draw the gasket */
  gp->gasket0 = glGenLists(1);
  gp->gasket1 = glGenLists(1);
  gp->gasket2 = glGenLists(1);
  gp->gasket3 = glGenLists(1);
  gp->current_depth = 1;       /* start out at level 1, not 0 */
}


ENTRYPOINT Bool
gasket_handle_event (ModeInfo *mi, XEvent *event)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, gp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &gp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == '+' || c == '=' ||
          keysym == XK_Up || keysym == XK_Right || keysym == XK_Next)
        {
          gp->tick = speed;
          gp->current_depth += (gp->current_depth > 0 ? 1 : -1);
          gp->current_depth--;
          return True;
        }
      else if (c == '-' || c == '_' ||
               keysym == XK_Down || keysym == XK_Left || keysym == XK_Prior)
        {
          gp->tick = speed;
          gp->current_depth -= (gp->current_depth > 0 ? 1 : -1);
          gp->current_depth--;
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        goto DEF;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
    DEF:
      gp->tick = speed;
      return True;
    }

  return False;
}


ENTRYPOINT void
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

  {
    double spin_speed   = 1.0;
    double wander_speed = 0.03;
    gp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            1.0,
                            do_wander ? wander_speed : 0,
                            True);
    gp->trackball = gltrackball_init (True);
  }

  gp->ncolors = 255;
  gp->colors = (XColor *) calloc(gp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        gp->colors, &gp->ncolors,
                        False, 0, False);
  gp->ccolor0 = 0;
  gp->ccolor1 = gp->ncolors * 0.25;
  gp->ccolor2 = gp->ncolors * 0.5;
  gp->ccolor3 = gp->ncolors * 0.75;
  gp->tick = 999999;

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

ENTRYPOINT void
draw_gasket(ModeInfo * mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  Display      *display = MI_DISPLAY(mi);
  Window        window = MI_WINDOW(mi);

  if (!gp->glx_context) return;

  glDrawBuffer(GL_BACK);

  /*  0 =              4 polygons
      1 =             16 polygons
      2 =             64 polygons
      3 =            256 polygons
      4 =          1,024 polygons
      5 =          4,096 polygons
      6 =         16,384 polygons
      7 =         65,536 polygons,  30 fps (3GHz Core 2 Duo, GeForce 8800 GS)
      8 =        262,144 polygons,  12 fps
      9 =      1,048,576 polygons,   4 fps
     10 =      4,194,304 polygons,   1 fps
     11 =     16,777,216 polygons, 0.3 fps
     12 =     67,108,864 polygons,    OOM!
     13 =    268,435,456 polygons
     14 =  1,073,741,824 polygons, 31 bits
     15 =  4,294,967,296 polygons, 33 bits
     16 = 17,179,869,184 polygons, 35 bits
   */
  if (max_depth > 10)
    max_depth = 10;

  glXMakeCurrent(display, window, *(gp->glx_context));
  draw(mi);
  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(display, window);
}

ENTRYPOINT void
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

        if (glIsList(gp->gasket0)) glDeleteLists(gp->gasket0, 1);
        if (glIsList(gp->gasket1)) glDeleteLists(gp->gasket1, 1);
        if (glIsList(gp->gasket2)) glDeleteLists(gp->gasket2, 1);
        if (glIsList(gp->gasket3)) glDeleteLists(gp->gasket3, 1);
      }
    }
    (void) free((void *) gasket);
    gasket = NULL;
  }
  FreeAllGL(mi);
}

XSCREENSAVER_MODULE_2 ("Sierpinski3D", sierpinski3d, gasket)

/*********************************************************/

#endif
