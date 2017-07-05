/* -*- Mode: C; tab-width: 4 -*- */
/* stairs --- Infinite Stairs, and Escher-like scene. */

#if 0
static const char sccsid[] = "@(#)stairs.c	4.07 97/11/24 xlockmore";
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
 * This mode shows some interesting scenes that are impossible OR very
 * weird to build in the real universe. Much of the scenes are inspirated
 * on Mauritz Cornelis stairs's works which derivated the mode's name.
 * M.C. Escher (1898-1972) was a dutch artist and many people prefer to
 * say he was a mathematician.
 *
 * Thanks goes to Brian Paul for making it possible and inexpensive to use 
 * OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistake.
 *
 * My e-mail address is
 * m-vianna@usa.net
 *
 * Marcelo F. Vianna (Jun-01-1997)
 *
 * Revision History:
 * 07-Jan-98: This would be a scene for the escher mode, but now escher mode
 *            was splitted in different modes for each scene. This is the
 *            initial release and is not working yet.
 *            Marcelo F. Vianna.
 *
 */

/*-
 * Texture mapping is only available on RGBA contexts, Mono and color index
 * visuals DO NOT support texture mapping in OpenGL.
 *
 * BUT Mesa do implements RGBA contexts in pseudo color visuals, so texture
 * mapping shuld work on PseudoColor, DirectColor, TrueColor using Mesa. Mono
 * is not officially supported for both OpenGL and Mesa, but seems to not crash
 * Mesa.
 *
 * In real OpenGL, PseudoColor DO NOT support texture map (as far as I know).
 */

#ifdef STANDALONE
# define DEFAULTS			"*delay:		20000   \n" \
							"*showFPS:      False   \n"

# define refresh_stairs 0
# define release_stairs 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */

#endif /* !STANDALONE */

#ifdef USE_GL

#if 0
#include "e_textures.h"
#else
#include "xpm-ximage.h"
#include "../images/wood.xpm"
#endif

#include "sphere.h"
#include "gltrackball.h"

ENTRYPOINT ModeSpecOpt stairs_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   stairs_description =
{"stairs", "init_stairs", "draw_stairs", NULL,
 "draw_stairs", "change_stairs", NULL, &stairs_opts,
 1000, 1, 1, 1, 4, 1.0, "",
 "Shows Infinite Stairs, an Escher-like scene", 0, NULL};

#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

/*************************************************************************/

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	int         rotating;
	int         AreObjectsDefined[1];
	int         sphere_position;
	int         sphere_tick;
	GLXContext *glx_context;
    trackball_state *trackball;
    Bool button_down_p;
    GLuint objects;
} stairsstruct;

static const float front_shininess[] = {60.0};
static const float front_specular[] = {0.7, 0.7, 0.7, 1.0};
static const float ambient[] = {0.0, 0.0, 0.0, 1.0};
static const float diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const float position0[] = {1.0, 1.0, 1.0, 0.0};
static const float position1[] = {-1.0, -1.0, 1.0, 0.0};
static const float lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
static const float lmodel_twoside[] = {GL_TRUE};

static const float MaterialYellow[] = {0.7, 0.7, 0.0, 1.0};
static const float MaterialWhite[] = {0.7, 0.7, 0.7, 1.0};

static const float ball_positions[] = {
  -3.0, 3.0, 1.0,
  -3.0, 2.8, 2.0,
  -3.0, 2.6, 3.0,

  -2.0, 2.4, 3.0,
  -1.0, 2.2, 3.0,
   0.0, 2.0, 3.0,
   1.0, 1.8, 3.0,
   2.0, 1.6, 3.0,

   2.0, 1.5, 2.0,
   2.0, 1.4, 1.0,
   2.0, 1.3, 0.0,
   2.0, 1.2, -1.0,
   2.0, 1.1, -2.0,

   1.0, 0.9, -2.0,
   0.0, 0.7, -2.0,
  -1.0, 0.5, -2.0,
};

#define NPOSITIONS ((sizeof ball_positions) / (sizeof ball_positions[0]) / 3)
#define SPHERE_TICKS 32

static stairsstruct *stairs = NULL;

static int
draw_block(GLfloat width, GLfloat height, GLfloat thickness)
{
    int polys = 0;
	glFrontFace(GL_CCW);
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(0, 0);
	glVertex3f(-width, -height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, -height, thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, height, thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, height, thickness);
    polys++;
	glNormal3f(0, 0, -1);
	glTexCoord2f(0, 0);
	glVertex3f(-width, height, -thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, -height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, -height, -thickness);
    polys++;
	glNormal3f(0, 1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-width, height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, height, thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, height, -thickness);
    polys++;
	glNormal3f(0, -1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-width, -height, -thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, -height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, -height, thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, -height, thickness);
    polys++;
	glNormal3f(1, 0, 0);
	glTexCoord2f(0, 0);
	glVertex3f(width, -height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, -height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(width, height, thickness);
    polys++;
	glNormal3f(-1, 0, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-width, height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(-width, height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(-width, -height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, -height, thickness);
    polys++;
	glEnd();
    return polys;
}

static void
draw_stairs_internal(ModeInfo * mi)
{
	GLfloat     X;

    mi->polygon_count = 0;

	glPushMatrix();
	glPushMatrix();
	glTranslatef(-3.0, 0.1, 2.0);
	for (X = 0; X < 2; X++) {
        mi->polygon_count += draw_block(0.5, 2.7 + 0.1 * X, 0.5);
		glTranslatef(0.0, 0.1, -1.0);
	}
	glPopMatrix();
	glTranslatef(-3.0, 0.0, 3.0);
	glPushMatrix();

	for (X = 0; X < 6; X++) {
		mi->polygon_count += draw_block(0.5, 2.6 - 0.1 * X, 0.5);
		glTranslatef(1.0, -0.1, 0.0);
	}
	glTranslatef(-1.0, -0.9, -1.0);
	for (X = 0; X < 5; X++) {
		mi->polygon_count += draw_block(0.5, 3.0 - 0.1 * X, 0.5);
		glTranslatef(0.0, 0.0, -1.0);
	}
	glTranslatef(-1.0, -1.1, 1.0);
	for (X = 0; X < 3; X++) {
		mi->polygon_count += draw_block(0.5, 3.5 - 0.1 * X, 0.5);
		glTranslatef(-1.0, -0.1, 0.0);
	}
	glPopMatrix();
	glPopMatrix();
}

/*#define DEBUG*/
/*#define DEBUG_PATH*/

static int
draw_sphere(int pos, int tick)
{
    int pos2 = (pos+1) % NPOSITIONS;
    GLfloat x1 = ball_positions[pos*3];
    GLfloat y1 = ball_positions[pos*3+1];
    GLfloat z1 = ball_positions[pos*3+2];
    GLfloat x2 = ball_positions[pos2*3];
    GLfloat y2 = ball_positions[pos2*3+1];
    GLfloat z2 = ball_positions[pos2*3+2];
    GLfloat frac = tick / (GLfloat) SPHERE_TICKS;
    GLfloat x = x1 + (x2 - x1) * frac;
    GLfloat y = y1 + (y2 - y1) * frac + (2 * sin (M_PI * frac));
    GLfloat z = z1 + (z2 - z1) * frac;
    int polys = 0;

	glPushMatrix();

# ifdef DEBUG_PATH
    glVertex3f(x, y, z);
    if (tick == 0) {
      glVertex3f(x, y-7.5, z);
      glVertex3f(x, y, z); glVertex3f(x, y, z-0.6);
      glVertex3f(x, y, z); glVertex3f(x, y, z+0.6);
      glVertex3f(x, y, z); glVertex3f(x+0.6, y, z);
      glVertex3f(x, y, z); glVertex3f(x-0.6, y, z);
      glVertex3f(x, y, z);
    }

# else /* !DEBUG_PATH */
    y += 0.5;
    glTranslatef(x, y, z);

    glScalef (0.5, 0.5, 0.5);

    /* make ball a little smaller on the gap to obscure distance */
	if (pos == NPOSITIONS-1)
      glScalef (0.95, 0.95, 0.95);

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialYellow);
    glDisable (GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glFrontFace(GL_CCW);
    polys += unit_sphere (32, 32, False);
	glShadeModel(GL_FLAT);
    glEnable (GL_TEXTURE_2D);
#endif /* !DEBUG_PATH */

	glPopMatrix();
    return polys;
}



ENTRYPOINT void
reshape_stairs (ModeInfo * mi, int width, int height)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];

	glViewport(0, 0, sp->WindW = (GLint) width, sp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);
	if (width >= 1024) {
		glLineWidth(3);
		glPointSize(3);
	} else if (width >= 512) {
		glLineWidth(2);
		glPointSize(2);
	} else {
		glLineWidth(1);
		glPointSize(1);
	}
}

ENTRYPOINT Bool
stairs_handle_event (ModeInfo *mi, XEvent *event)
{
  stairsstruct *sp = &stairs[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, sp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &sp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t')
        {
          gltrackball_reset (sp->trackball, 0, 0);
          return True;
        }
    }

  return False;
}


static void
pinit(ModeInfo *mi)
{
  /* int status; */
	glClearDepth(1.0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position0);
	glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, position1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_NORMALIZE);
	glCullFace(GL_BACK);

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
	glShadeModel(GL_FLAT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#if 0
    clear_gl_error();
    status = gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
                               WoodTextureWidth, WoodTextureHeight,
                               GL_RGB, GL_UNSIGNED_BYTE, WoodTextureData);
    if (status)
      {
        const char *s = (char *) gluErrorString (status);
        fprintf (stderr, "%s: error mipmapping %dx%d texture: %s\n",
                 progname, WoodTextureWidth, WoodTextureHeight,
                 (s ? s : "(unknown)"));
        exit (1);
      }
    check_gl_error("mipmapping");
#else
    {
      XImage *img = xpm_to_ximage (mi->dpy,
                                   mi->xgwa.visual,
                                   mi->xgwa.colormap,
                                   wood_texture);
	  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                    img->width, img->height, 0,
                    GL_RGBA,
                    /* GL_UNSIGNED_BYTE, */
                    GL_UNSIGNED_INT_8_8_8_8_REV,
                    img->data);
      check_gl_error("texture");
      XDestroyImage (img);
    }
#endif

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);
}

static void free_stairs (ModeInfo * mi);

ENTRYPOINT void
init_stairs (ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	stairsstruct *sp;

	MI_INIT (mi, stairs, free_stairs);
	sp = &stairs[screen];

	sp->step = 0.0;
	sp->rotating = 0;
	sp->sphere_position = NRAND(NPOSITIONS);
	sp->sphere_tick = 0;

	if ((sp->glx_context = init_GL(mi)) != NULL) {

		reshape_stairs(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		if (!glIsList(sp->objects))
			sp->objects = glGenLists(1);
		pinit(mi);
	} else {
		MI_CLEARWINDOW(mi);
	}

    sp->trackball = gltrackball_init (False);
}

ENTRYPOINT void
draw_stairs (ModeInfo * mi)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];
    GLfloat rot = current_device_rotation();

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	if (!sp->glx_context)
		return;

	glXMakeCurrent(display, window, *(sp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

    glRotatef(rot, 0, 0, 1);

	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_IS_ICONIC(mi)) {
		glScalef(Scale4Window * sp->WindH / sp->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * sp->WindH / sp->WindW, Scale4Iconic, Scale4Iconic);
	}

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    if (rot != 0 && rot != 180 && rot != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

    gltrackball_rotate (sp->trackball);

    glTranslatef(0, 0.5, 0);
	glRotatef(44.5, 1, 0, 0);
    glRotatef(50, 0, 1, 0);

    if (!sp->rotating) {
      if ((LRAND() % 500) == 0)
        sp->rotating = (LRAND() & 1) ? 1 : -1;
    }

    if (sp->rotating) {
      glRotatef(sp->rotating * sp->step, 0, 1, 0);
      if (sp->step >= 360) {
        sp->rotating = 0;
		sp->step = 0;
      }

# ifndef DEBUG
      if (!sp->button_down_p)
        sp->step += 2;
# endif /* DEBUG */
    }

	draw_stairs_internal(mi);


# ifdef DEBUG
    {
      int i, j;
#  ifdef DEBUG_PATH
      glDisable(GL_LIGHTING);
      glDisable(GL_TEXTURE_2D);
      glBegin (GL_LINE_LOOP);
#  endif /* DEBUG_PATH */
      for (i = 0; i < NPOSITIONS; i ++)
        for (j = 0; j < SPHERE_TICKS; j++)
          mi->polygon_count += draw_sphere(i, j);
#  ifdef DEBUG_PATH
      glEnd();
      glEnable(GL_LIGHTING);
      glEnable(GL_TEXTURE_2D);
#  endif /* DEBUG_PATH */
    }
#else  /* !DEBUG */
    mi->polygon_count += draw_sphere(sp->sphere_position, sp->sphere_tick);
#endif /* !DEBUG */

    if (sp->button_down_p)
      ;
    else if (++sp->sphere_tick >= SPHERE_TICKS)
      {
        sp->sphere_tick = 0;
        if (++sp->sphere_position >= NPOSITIONS)
          sp->sphere_position = 0;
      }

	glPopMatrix();

    if (mi->fps_p) do_fps (mi);
	glFlush();

	glXSwapBuffers(display, window);
}

#ifndef STANDALONE
ENTRYPOINT void
change_stairs (ModeInfo * mi)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];

	if (!sp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sp->glx_context));
	pinit();
}
#endif /* !STANDALONE */

static void
free_stairs (ModeInfo * mi)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];
	if (glIsList(sp->objects)) {
		glDeleteLists(sp->objects, 1);
	}
}

XSCREENSAVER_MODULE ("Stairs", stairs)

#endif
