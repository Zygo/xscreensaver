/* -*- Mode: C; tab-width: 4 -*- */
/* gears --- 3D gear wheels */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)gears.c	4.07 97/11/24 xlockmore";

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
 * 09-Feb-01: "Planetary" gear system added by jwz@jwz.org.
 * 10-May-97: Compatible with xscreensaver
 * 22-Mar-97: Added support for -mono mode, and monochrome X servers.
 *              Ed Mackey, emackey@netaxs.com
 * 13-Mar-97: Memory leak fix by Tom Schmidt <tschmidt@micron.com>
 * 1996: "written" by Danny Sung <dannys@ucla.edu>
 *       Based on 3-D gear wheels by Brian Paul which is in the public domain.
 */

/*-
 * PURIFY 3.0a on SunOS4 reports an unitialized memory read on each of
 * the glCallList() functions below when using MesaGL 2.1.  This has
 * been fixed in MesaGL 2.2 and later releases.
 */

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS					"Gears"
# define HACK_INIT					init_gears
# define HACK_DRAW					draw_gears
# define HACK_RESHAPE				reshape_gears
# define HACK_HANDLE_EVENT			gears_handle_event
# define EVENT_MASK					PointerMotionMask
# define gears_opts					xlockmore_opts
# define DEFAULTS	"*count:		1       \n"			\
					"*cycles:		2       \n"			\
					"*delay:		20000   \n"			\
					"*planetary:	False   \n"			\
					"*showFPS:      False   \n"			\
					"*wireframe:	False	\n"
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "rotator.h"
#include "gltrackball.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEF_PLANETARY "False"

static int planetary;

static XrmOptionDescRec opts[] = {
  {"-planetary", ".gears.planetary", XrmoptionNoArg, (caddr_t) "true" },
  {"+planetary", ".gears.planetary", XrmoptionNoArg, (caddr_t) "false" },
};

static argtype vars[] = {
  {(caddr_t *) &planetary, "planetary", "Planetary", DEF_PLANETARY, t_Bool},
};

ModeSpecOpt gears_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   gears_description =
{"gears", "init_gears", "draw_gears", "release_gears",
 "draw_gears", "init_gears", NULL, &gears_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Shows GL's gears", 0, NULL};

#endif

#define SMOOTH_TUBE       /* whether to have smooth or faceted tubes */

#ifdef SMOOTH_TUBE
# define TUBE_FACES  20   /* how densely to render tubes */
#else
# define TUBE_FACES  6
#endif


typedef struct {
  GLuint      gear1, gear2, gear3;
  GLuint      gear_inner, gear_outer;
  GLuint      armature;
  GLfloat     angle;
  GLXContext *glx_context;
  Window      window;
  rotator    *rot;
  trackball_state *trackball;
  Bool		  button_down_p;
} gearsstruct;

static gearsstruct *gears = NULL;

/*-
 * Draw a gear wheel.  You'll probably want to call this function when
 * building a display list since we do a lot of trig here.
 *
 * Input:  inner_radius - radius of hole at center
 *         outer_radius - radius at center of teeth
 *         width - width of gear
 *         teeth - number of teeth
 *         tooth_depth - depth of tooth
 *         wire - true for wireframe mode
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth, Bool wire, Bool invert)
{
	GLint       i;
	GLfloat     r0, r1, r2;
	GLfloat     angle, da;
	GLfloat     u, v, len;

    if (!invert)
      {
        r0 = inner_radius;
        r1 = outer_radius - tooth_depth / 2.0;
        r2 = outer_radius + tooth_depth / 2.0;
        glFrontFace(GL_CCW);
      }
    else
      {
        r0 = outer_radius;
        r2 = inner_radius + tooth_depth / 2.0;
        r1 = outer_radius - tooth_depth / 2.0;
        glFrontFace(GL_CW);
      }

	da = 2.0 * M_PI / teeth / 4.0;

	glShadeModel(GL_FLAT);

	/* This subroutine got kind of messy when I added all the checks
	 * for wireframe mode.  A much cleaner solution that I sometimes
	 * use is to have a variable hold the value GL_LINE_LOOP when
	 * in wireframe mode, or hold the value GL_POLYGON otherwise.
	 * Then I just call glBegin(that_variable), give my polygon
	 * coordinates, and glEnd().  Pretty neat eh?  Too bad I couldn't
	 * integrate that trick here.
	 *                                  --Ed.
	 */

	if (!wire)
		glNormal3f(0.0, 0.0, 1.0);

	/* draw front face */
	if (!wire)
		glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		if (wire)
			glBegin(GL_LINES);
		angle = i * 2.0 * M_PI / teeth;
		glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		if (!wire) {
			glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
		} else {
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
			glVertex3f(r1 * cos(angle + 4 * da), r1 * sin(angle + 4 * da), width * 0.5);
			glEnd();
		}
	}
	if (!wire)
		glEnd();

	/* draw front sides of teeth */
	if (!wire)
		glBegin(GL_QUADS);
	da = 2.0 * M_PI / teeth / 4.0;
	for (i = 0; i < teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;

		if (wire)
			glBegin(GL_LINE_LOOP);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
		if (wire)
			glEnd();
	}
	if (!wire)
		glEnd();


	if (!wire)
		glNormal3f(0.0, 0.0, -1.0);

	/* draw back face */
	if (!wire)
		glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;
		if (wire)
			glBegin(GL_LINES);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		if (!wire) {
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
			glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		} else {
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
			glVertex3f(r1 * cos(angle + 4 * da), r1 * sin(angle + 4 * da), -width * 0.5);
			glEnd();
		}
	}
	if (!wire)
		glEnd();

	/* draw back sides of teeth */
	if (!wire)
		glBegin(GL_QUADS);
	da = 2.0 * M_PI / teeth / 4.0;
	for (i = 0; i < teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;

		if (wire)
			glBegin(GL_LINE_LOOP);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
		if (wire)
			glEnd();
	}
	if (!wire)
		glEnd();


	/* draw outward faces of teeth */
	if (!wire)
		glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;

        if(!invert) {
          u = r2 * cos(angle + da) - r1 * cos(angle);
          v = r2 * sin(angle + da) - r1 * sin(angle);
        } else {
          u = r2 * cos(angle + da + M_PI/2) - r1 * cos(angle + M_PI/2);
          v = r2 * sin(angle + da + M_PI/2) - r1 * sin(angle + M_PI/2);
        }

		len = sqrt(u * u + v * v);
		u /= len;
		v /= len;
		glNormal3f(v, -u, 0.0);

		if (wire)
			glBegin(GL_LINES);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);

		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);

        if(!invert)
          glNormal3f(cos(angle), sin(angle), 0.0);
        else
          glNormal3f(cos(angle + M_PI/2), sin(angle + M_PI/2), 0.0);

		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);

        if(!invert) {
          u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
          v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
        } else {
          u = r1 * cos(angle + 3 * da + M_PI/2) - r2 * cos(angle + 2 * da + M_PI/2);
          v = r1 * sin(angle + 3 * da + M_PI/2) - r2 * sin(angle + 2 * da + M_PI/2);
        }

		glNormal3f(v, -u, 0.0);

		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);

        if (!invert)
          glNormal3f(cos(angle), sin(angle), 0.0);
        else
          glNormal3f(cos(angle + M_PI/2), sin(angle + M_PI/2), 0.0);

		if (wire)
			glEnd();
	}

	if (!wire) {
		glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
		glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);
		glEnd();
	}
	if (!wire)
		glShadeModel(GL_SMOOTH);

	/* draw inside radius cylinder */
	if (!wire)
		glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;
		if (wire)
			glBegin(GL_LINES);

        if (!invert)
          glNormal3f(-cos(angle), -sin(angle), 0.0);
        else
          glNormal3f(cos(angle), sin(angle), 0.0);

		glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
		if (wire) {
			glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
			glVertex3f(r0 * cos(angle + 4 * da), r0 * sin(angle + 4 * da), -width * 0.5);
			glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
			glVertex3f(r0 * cos(angle + 4 * da), r0 * sin(angle + 4 * da), width * 0.5);
			glEnd();
		}
	}
	if (!wire)
		glEnd();

}


static void
unit_tube (Bool wire)
{
  int i;
  int faces = TUBE_FACES;
  GLfloat step = M_PI * 2 / faces;
  GLfloat th;
  int z = 0;

  /* side walls
   */
  glFrontFace(GL_CCW);

# ifdef SMOOTH_TUBE
  glBegin(wire ? GL_LINES : GL_QUAD_STRIP);
# else
  glBegin(wire ? GL_LINES : GL_QUADS);
# endif

  for (i = 0, th = 0; i <= faces; i++)
    {
      GLfloat x = cos (th);
      GLfloat y = sin (th);
      glNormal3f(x, 0, y);
      glVertex3f(x, 0.0, y);
      glVertex3f(x, 1.0, y);
      th += step;

# ifndef SMOOTH_TUBE
      x = cos (th);
      y = sin (th);
      glVertex3f(x, 1.0, y);
      glVertex3f(x, 0.0, y);
# endif
    }
  glEnd();

  /* End caps
   */
  for (z = 0; z <= 1; z++)
    {
      glFrontFace(z == 0 ? GL_CCW : GL_CW);
      glNormal3f(0, (z == 0 ? -1 : 1), 0);
      glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
      if (! wire) glVertex3f(0, z, 0);
      for (i = 0, th = 0; i <= faces; i++)
        {
          GLfloat x = cos (th);
          GLfloat y = sin (th);
          glVertex3f(x, z, y);
          th += step;
        }
      glEnd();
    }
}


static void
tube (GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat diameter, GLfloat cap_size,
      Bool wire)
{
  GLfloat length, angle, a, b, c;

  if (diameter <= 0) abort();

  a = (x2 - x1);
  b = (y2 - y1);
  c = (z2 - z1);

  length = sqrt (a*a + b*b + c*c);
  angle = acos (a / length);

  glPushMatrix();
  glTranslatef(x1, y1, z1);
  glScalef (length, length, length);

  if (c == 0 && b == 0)
    glRotatef (angle / (M_PI / 180), 0, 1, 0);
  else
    glRotatef (angle / (M_PI / 180), 0, -c, b);

  glRotatef (-90, 0, 0, 1);
  glScalef (diameter/length, 1, diameter/length);

  /* extend the endpoints of the tube by the cap size in both directions */
  if (cap_size != 0)
    {
      GLfloat c = cap_size/length;
      glTranslatef (0, -c, 0);
      glScalef (1, 1+c+c, 1);
    }

  unit_tube (wire);
  glPopMatrix();
}


static void
ctube (GLfloat diameter, GLfloat width, Bool wire)
{
  tube (0, 0,  width/2,
        0, 0, -width/2,
        diameter, 0, wire);
}

static void
arm(GLfloat length,
    GLfloat width1, GLfloat height1,
    GLfloat width2, GLfloat height2,
    Bool wire)
{
  glShadeModel(GL_FLAT);

#if 0  /* don't need these - they're embedded in other objects */
  /* draw end 1 */
  glFrontFace(GL_CW);
  glNormal3f(-1, 0, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2,  height1/2);
  glVertex3f(-length/2, -width1/2,  height1/2);
  glEnd();

  /* draw end 2 */
  glFrontFace(GL_CCW);
  glNormal3f(1, 0, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(length/2, -width2/2, -height2/2);
  glVertex3f(length/2,  width2/2, -height2/2);
  glVertex3f(length/2,  width2/2,  height2/2);
  glVertex3f(length/2, -width2/2,  height2/2);
  glEnd();
#endif

  /* draw top */
  glFrontFace(GL_CCW);
  glNormal3f(0, 0, -1);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2, -height1/2);
  glVertex3f( length/2,  width2/2, -height2/2);
  glVertex3f( length/2, -width2/2, -height2/2);
  glEnd();

  /* draw bottom */
  glFrontFace(GL_CW);
  glNormal3f(0, 0, 1);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, height1/2);
  glVertex3f(-length/2,  width1/2, height1/2);
  glVertex3f( length/2,  width2/2, height2/2);
  glVertex3f( length/2, -width2/2, height2/2);
  glEnd();

  /* draw left */
  glFrontFace(GL_CW);
  glNormal3f(0, -1, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2, -width1/2, -height1/2);
  glVertex3f(-length/2, -width1/2,  height1/2);
  glVertex3f( length/2, -width2/2,  height2/2);
  glVertex3f( length/2, -width2/2, -height2/2);
  glEnd();

  /* draw right */
  glFrontFace(GL_CCW);
  glNormal3f(0, 1, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glVertex3f(-length/2,  width1/2, -height1/2);
  glVertex3f(-length/2,  width1/2,  height1/2);
  glVertex3f( length/2,  width2/2,  height2/2);
  glVertex3f( length/2,  width2/2, -height2/2);
  glEnd();

  glFrontFace(GL_CCW);
}


static void
draw(ModeInfo * mi)
{
	gearsstruct *gp = &gears[MI_SCREEN(mi)];
	int         wire = MI_IS_WIREFRAME(mi);

	if (!wire) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	} else {
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glPushMatrix();

    gltrackball_rotate (gp->trackball);

    {
      double x, y, z;
      get_rotation (gp->rot, &x, &y, &z, !gp->button_down_p);
      glRotatef (x * 360, 1.0, 0.0, 0.0);
      glRotatef (y * 360, 0.0, 1.0, 0.0);
      glRotatef (z * 360, 0.0, 0.0, 1.0);
    }

    if (!planetary) {
      glPushMatrix();
      glTranslatef(-3.0, -2.0, 0.0);
      glRotatef(gp->angle, 0.0, 0.0, 1.0);
/* PURIFY 4.0.1 reports an unitialized memory read on the next line when using
   * MesaGL 2.2 and -mono.  This has been fixed in MesaGL 2.3 and later. */
      glCallList(gp->gear1);
      glPopMatrix();

      glPushMatrix();
      glTranslatef(3.1, -2.0, 0.0);
      glRotatef(-2.0 * gp->angle - 9.0, 0.0, 0.0, 1.0);
      glCallList(gp->gear2);
      glPopMatrix();

      glPushMatrix();
      glTranslatef(-3.1, 4.2, 0.0);
      glRotatef(-2.0 * gp->angle - 25.0, 0.0, 0.0, 1.0);
      glCallList(gp->gear3);
      glPopMatrix();

    } else { /* planetary */

      glScalef(0.8, 0.8, 0.8);

      glPushMatrix();
      glTranslatef(0.0, 4.2, 0.0);
      glRotatef(gp->angle - 7.0, 0.0, 0.0, 1.0);
      glCallList(gp->gear1);
      glPopMatrix();

      glPushMatrix();
      glRotatef(120, 0.0, 0.0, 1.0);
      glTranslatef(0.0, 4.2, 0.0);
      glRotatef(gp->angle - 7.0, 0.0, 0.0, 1.0);
      glCallList(gp->gear2);
      glPopMatrix();

      glPushMatrix();
      glRotatef(240, 0.0, 0.0, 1.0);
      glTranslatef(0.0, 4.2, 0.0);
      glRotatef(gp->angle - 7.0, 0.0, 0.0, 1.0);
      glCallList(gp->gear3);
      glPopMatrix();

      glPushMatrix();
      glTranslatef(0.0, 0.0, 0.0);
      glRotatef(-gp->angle, 0.0, 0.0, 1.0);
      glCallList(gp->gear_inner);
      glPopMatrix();

      glPushMatrix();
      glTranslatef(0.0, 0.0, 0.0);
      glRotatef((gp->angle / 3.0) - 7.5, 0.0, 0.0, 1.0);
      glCallList(gp->gear_outer);
      glPopMatrix();

      glPushMatrix();
      glTranslatef(0.0, 0.0, 0.0);
      glCallList(gp->armature);
      glPopMatrix();
    }

	glPopMatrix();
}



/* new window size or exposure */
void
reshape_gears(ModeInfo *mi, int width, int height)
{
	GLfloat     h = (GLfloat) height / (GLfloat) width;

	glViewport(0, 0, (GLint) width, (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -40.0);

	/* The depth buffer will be cleared, if needed, before the
	 * next frame.  Right now we just want to black the screen.
	 */
	glClear(GL_COLOR_BUFFER_BIT);

}


static void
pinit(ModeInfo * mi)
{
	gearsstruct *gp = &gears[MI_SCREEN(mi)];
	static GLfloat pos[4] =
	{5.0, 5.0, 10.0, 1.0};
	static GLfloat red[4] =
	{0.8, 0.1, 0.0, 1.0};
	static GLfloat green[4] =
	{0.0, 0.8, 0.2, 1.0};
	static GLfloat blue[4] =
	{0.2, 0.2, 1.0, 1.0};
	static GLfloat gray[4] =
	{0.5, 0.5, 0.5, 1.0};
	static GLfloat white[4] =
	{1.0, 1.0, 1.0, 1.0};
	int         wire = MI_IS_WIREFRAME(mi);
	int         mono = MI_IS_MONO(mi);

	if (!wire) {
		glLightfv(GL_LIGHT0, GL_POSITION, pos);
		glEnable(GL_CULL_FACE);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_DEPTH_TEST);
	}
#if 0
/*-
 * Messes up on multiscreen Pseudocolor:0 StaticGray(monochrome):1
 * 2nd time mode is run it is Grayscale on PseudoColor.
 * The code below forces monochrome on TrueColor.
 */
	if (MI_IS_MONO(mi)) {
		red[0] = red[1] = red[2] = 1.0;
		green[0] = green[1] = green[2] = 1.0;
		blue[0] = blue[1] = blue[2] = 1.0;
	}
#endif

	/* make the gears */

    if (! planetary) {

      gp->gear1 = glGenLists(1);
      glNewList(gp->gear1, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(red);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
      }

      gear(1.0, 4.0, 1.0, 20, 0.7, wire, False);
      glEndList();

      gp->gear2 = glGenLists(1);
      glNewList(gp->gear2, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(green);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
      }
      gear(0.5, 2.0, 2.0, 10, 0.7, wire, False);
      glEndList();

      gp->gear3 = glGenLists(1);
      glNewList(gp->gear3, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(blue);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
      }
      gear(1.3, 2.0, 0.5, 10, 0.7, wire, False);
      glEndList();
      if (!wire)
		glEnable(GL_NORMALIZE);

    } else { /* planetary */

      gp->gear1 = glGenLists(1);
      glNewList(gp->gear1, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(red);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
      }
      gear(1.3, 2.0, 2.0, 12, 0.7, wire, False);
      glEndList();

      gp->gear2 = glGenLists(1);
      glNewList(gp->gear2, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(green);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
      }
      gear(1.3, 2.0, 2.0, 12, 0.7, wire, False);
      glEndList();

      gp->gear3 = glGenLists(1);
      glNewList(gp->gear3, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(blue);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
      }
      gear(1.3, 2.0, 2.0, 12, 0.7, wire, False);
      glEndList();
      if (!wire)
		glEnable(GL_NORMALIZE);


      gp->gear_inner = glGenLists(1);
      glNewList(gp->gear_inner, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(blue);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
      }
      gear(1.0, 2.0, 2.0, 12, 0.7, wire, False);
      glEndList();
      if (!wire)
		glEnable(GL_NORMALIZE);


      gp->gear_outer = glGenLists(1);
      glNewList(gp->gear_outer, GL_COMPILE);
      if (wire) {
		if (mono)
          glColor4fv(white);
		else
          glColor4fv(blue);
      } else {
		if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
		else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
      }
      gear(5.7, 7.0, 2.0, 36, 0.7, wire, True);

      /* put some nubs on the outer ring, so we can tell how it's moving */
      glPushMatrix();
      glTranslatef(7.0, 0, 0);
      glRotatef(90, 0, 1, 0);

      ctube(0.5, 0.5, wire);   /* nub 1 */
      glPopMatrix();

      glPushMatrix();
      glRotatef(120, 0, 0, 1);
      glTranslatef(7.0, 0, 0);
      glRotatef(90, 0, 1, 0);
      ctube(0.5, 0.5, wire);   /* nub 2 */
      glPopMatrix();

      glPushMatrix();
      glRotatef(240, 0, 0, 1);
      glTranslatef(7.0, 0, 0);
      glRotatef(90, 0, 1, 0);
      ctube(0.5, 0.5, wire);   /* nub 3 */
      glPopMatrix();


      glEndList();
      if (!wire)
		glEnable(GL_NORMALIZE);

      gp->armature = glGenLists(1);
      glNewList(gp->armature, GL_COMPILE);
      if (wire) {
        if (mono)
          glColor4fv(white);
        else
          glColor4fv(blue);
      } else {
        if (mono)
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
        else
          glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
      }

      glTranslatef(0, 0, 1.5);
      ctube(0.5, 10, wire);       /* center axle */

      glPushMatrix();
      glTranslatef(0.0, 4.2, -1);
      ctube(0.5, 3, wire);       /* axle 1 */
      glTranslatef(0, 0, 1.8);
      ctube(0.7, 0.7, wire);
      glPopMatrix();

      glPushMatrix();
      glRotatef(120, 0.0, 0.0, 1.0);
      glTranslatef(0.0, 4.2, -1);
      ctube(0.5, 3, wire);       /* axle 2 */
      glTranslatef(0, 0, 1.8);
      ctube(0.7, 0.7, wire);
      glPopMatrix();

      glPushMatrix();
      glRotatef(240, 0.0, 0.0, 1.0);
      glTranslatef(0.0, 4.2, -1);
      ctube(0.5, 3, wire);       /* axle 3 */
      glTranslatef(0, 0, 1.8);
      ctube(0.7, 0.7, wire);
      glPopMatrix();

      glTranslatef(0, 0, 1.5);      /* center disk */
      ctube(1.5, 2, wire);

      glPushMatrix();
      glRotatef(270, 0, 0, 1);
      glRotatef(-10, 0, 1, 0);
      glTranslatef(-2.2, 0, 0);
      arm(4.0, 1.0, 0.5, 2.0, 1.0, wire);		/* arm 1 */
      glPopMatrix();

      glPushMatrix();
      glRotatef(30, 0, 0, 1);
      glRotatef(-10, 0, 1, 0);
      glTranslatef(-2.2, 0, 0);
      arm(4.0, 1.0, 0.5, 2.0, 1.0, wire);		/* arm 2 */
      glPopMatrix();

      glPushMatrix();
      glRotatef(150, 0, 0, 1);
      glRotatef(-10, 0, 1, 0);
      glTranslatef(-2.2, 0, 0);
      arm(4.0, 1.0, 0.5, 2.0, 1.0, wire);		/* arm 3 */
      glPopMatrix();

      glEndList();
      if (!wire)
        glEnable(GL_NORMALIZE);
    }
}


Bool
gears_handle_event (ModeInfo *mi, XEvent *event)
{
  gearsstruct *gp = &gears[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      gp->button_down_p = True;
      gltrackball_start (gp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      gp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           gp->button_down_p)
    {
      gltrackball_track (gp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


void
init_gears(ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);

	/*Colormap    cmap; */
	/* Boolean     rgba, doublebuffer, cmap_installed; */
	gearsstruct *gp;

	if (gears == NULL) {
		if ((gears = (gearsstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (gearsstruct))) == NULL)
			return;
	}
	gp = &gears[screen];

	gp->window = MI_WINDOW(mi);

    gp->rot = make_rotator (1, 1, 1, 1, 0, True);
    gp->trackball = gltrackball_init ();

	if ((gp->glx_context = init_GL(mi)) != NULL) {
		reshape_gears(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		pinit(mi);
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_gears(ModeInfo * mi)
{
	gearsstruct *gp = &gears[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         angle_incr = MI_CYCLES(mi) ? MI_CYCLES(mi) : 2;

    if (planetary)
      angle_incr *= 3;

	if (!gp->glx_context)
		return;

	glDrawBuffer(GL_BACK);

	glXMakeCurrent(display, window, *(gp->glx_context));
	draw(mi);

	/* let's do something so we don't get bored */
	gp->angle = (int) (gp->angle + angle_incr) % 360;

    if (mi->fps_p) do_fps (mi);
	glFinish();
	glXSwapBuffers(display, window);
}

void
release_gears(ModeInfo * mi)
{
	if (gears != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			gearsstruct *gp = &gears[screen];

			if (gp->glx_context) {
				/* Display lists MUST be freed while their glXContext is current. */
				glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

				if (glIsList(gp->gear1))
					glDeleteLists(gp->gear1, 1);
				if (glIsList(gp->gear2))
					glDeleteLists(gp->gear2, 1);
				if (glIsList(gp->gear3))
					glDeleteLists(gp->gear3, 1);
				if (glIsList(gp->gear_inner))
					glDeleteLists(gp->gear_inner, 1);
				if (glIsList(gp->gear_outer))
					glDeleteLists(gp->gear_outer, 1);

			}
		}
		(void) free((void *) gears);
		gears = NULL;
	}
	FreeAllGL(mi);
}


/*********************************************************/

#endif
