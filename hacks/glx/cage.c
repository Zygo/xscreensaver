/* -*- Mode: C; tab-width: 4 -*- */
/* cage --- the Impossible Cage, an Escher like scene. */

#if 0
static const char sccsid[] = "@(#)cage.c	5.01 2001/03/01 xlockmore";
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
 * The RotateAroundU() routine was adapted from the book
 *    "Computer Graphics Principles and Practice
 *     Foley - vanDam - Feiner - Hughes
 *     Second Edition" Pag. 227, exercise 5.15.
 *
 * This mode shows some interesting scenes that are impossible OR very
 * wierd to build in the real universe. Much of the scenes are inspirated
 * on Mauritz Cornelis Escher's works which derivated the mode's name.
 * M.C. Escher (1898-1972) was a dutch artist and many people prefer to
 * say he was a mathematician.
 *
 * Thanks goes to Brian Paul for making it possible and inexpensive to use
 * OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistakes.
 *
 * My e-mail address is
 * mfvianna@centroin.com.br
 *
 * Marcelo F. Vianna (Jun-01-1997)
 *
 * Revision History:
 * 05-Apr-2002: Removed all gllist uses (fix some bug with nvidia driver)
 * 01-Mar-2001: Added FPS stuff E.Lassauge <lassauge@mail.dotcom.fr>
 * 01-Nov-2000: Allocation checks
 * 01-Jan-1998: Mode separated from escher and renamed
 * 08-Jun-1997: New scene implemented: "Impossible Cage" based in a M.C.
 *              Escher's painting with the same name (quite similar). The
 *              first GL mode to use texture mapping.
 *              The "Impossible Cage" scene doesn't use DEPTH BUFFER, the
 *              wood planks are drawn consistently using GL_CULL_FACE, and
 *              the painter's algorithm is used to sort the planks.
 *              Marcelo F. Vianna.
 * 07-Jun-1997: Speed ups in Moebius Strip using GL_CULL_FACE.
 *              Marcelo F. Vianna.
 * 03-Jun-1997: Initial Release (Only one scene: "Moebius Strip")
 *              The Moebius Strip scene was inspirated in a M.C. Escher's
 *              painting named Moebius Strip II in wich ants walk across a
 *              Moebius Strip path, sometimes meeting each other and sometimes
 *              being in "opposite faces" (note that the moebius strip has
 *              only one face and one edge).
 *              Marcelo F. Vianna.
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
# define MODE_cage
# define DEFAULTS			"*delay:		25000   \n"			\
							"*showFPS:      False   \n"			\
							"*wireframe:	False	\n"			\
							"*suppressRotationAnimation: True\n" \

# define release_cage 0
# define cage_handle_event xlockmore_no_events
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef MODE_cage

#if 0
#include "e_textures.h"
#else
#include "ximage-loader.h"
#include "images/gen/wood_png.h"
#endif

ENTRYPOINT ModeSpecOpt cage_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   cage_description =
{"cage", "init_cage", "draw_cage", NULL,
 "draw_cage", "change_cage", (char *) NULL, &cage_opts,
 25000, 1, 1, 1, 1.0, 4, "",
 "Shows the Impossible Cage, an Escher-like GL scene", 0, NULL};

#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

#define ObjWoodPlank    0
#define MaxObj          1

/*************************************************************************/

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	GLXContext *glx_context;
} cagestruct;

static const float front_shininess[] = {60.0};
static const float front_specular[] = {0.7, 0.7, 0.7, 1.0};
static const float ambient[] = {0.0, 0.0, 0.0, 1.0};
static const float diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const float position0[] = {1.0, 1.0, 1.0, 0.0};
static const float position1[] = {-1.0, -1.0, 1.0, 0.0};
static const float lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
static const float lmodel_twoside[] = {GL_TRUE};

static const float MaterialWhite[] = {0.7, 0.7, 0.7, 1.0};

static cagestruct *cage = (cagestruct *) NULL;

#define PlankWidth      3.0
#define PlankHeight     0.35
#define PlankThickness  0.15

static Bool 
draw_woodplank(ModeInfo *mi, cagestruct * cp, int wire)
{
	glBegin(wire ? GL_LINES : GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(0, 0);
	glVertex3f(-PlankWidth, -PlankHeight, PlankThickness);
	glTexCoord2f(1, 0);
	glVertex3f(PlankWidth, -PlankHeight, PlankThickness);
	glTexCoord2f(1, 1);
	glVertex3f(PlankWidth, PlankHeight, PlankThickness);
	glTexCoord2f(0, 1);
	glVertex3f(-PlankWidth, PlankHeight, PlankThickness);
    mi->polygon_count++;
	glNormal3f(0, 0, -1);
	glTexCoord2f(0, 0);
	glVertex3f(-PlankWidth, PlankHeight, -PlankThickness);
	glTexCoord2f(1, 0);
	glVertex3f(PlankWidth, PlankHeight, -PlankThickness);
	glTexCoord2f(1, 1);
	glVertex3f(PlankWidth, -PlankHeight, -PlankThickness);
	glTexCoord2f(0, 1);
	glVertex3f(-PlankWidth, -PlankHeight, -PlankThickness);
    mi->polygon_count++;
	glNormal3f(0, 1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-PlankWidth, PlankHeight, PlankThickness);
	glTexCoord2f(1, 0);
	glVertex3f(PlankWidth, PlankHeight, PlankThickness);
	glTexCoord2f(1, 1);
	glVertex3f(PlankWidth, PlankHeight, -PlankThickness);
	glTexCoord2f(0, 1);
	glVertex3f(-PlankWidth, PlankHeight, -PlankThickness);
    mi->polygon_count++;
	glNormal3f(0, -1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-PlankWidth, -PlankHeight, -PlankThickness);
	glTexCoord2f(1, 0);
	glVertex3f(PlankWidth, -PlankHeight, -PlankThickness);
	glTexCoord2f(1, 1);
	glVertex3f(PlankWidth, -PlankHeight, PlankThickness);
	glTexCoord2f(0, 1);
	glVertex3f(-PlankWidth, -PlankHeight, PlankThickness);
    mi->polygon_count++;
	glNormal3f(1, 0, 0);
	glTexCoord2f(0, 0);
	glVertex3f(PlankWidth, -PlankHeight, PlankThickness);
	glTexCoord2f(1, 0);
	glVertex3f(PlankWidth, -PlankHeight, -PlankThickness);
	glTexCoord2f(1, 1);
	glVertex3f(PlankWidth, PlankHeight, -PlankThickness);
	glTexCoord2f(0, 1);
	glVertex3f(PlankWidth, PlankHeight, PlankThickness);
    mi->polygon_count++;
	glNormal3f(-1, 0, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-PlankWidth, PlankHeight, PlankThickness);
	glTexCoord2f(1, 0);
	glVertex3f(-PlankWidth, PlankHeight, -PlankThickness);
	glTexCoord2f(1, 1);
	glVertex3f(-PlankWidth, -PlankHeight, -PlankThickness);
	glTexCoord2f(0, 1);
	glVertex3f(-PlankWidth, -PlankHeight, PlankThickness);
    mi->polygon_count++;
	glEnd();

	return True;
}

static Bool
draw_impossiblecage(ModeInfo *mi, cagestruct * cp, int wire)
{
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankHeight - PlankWidth, -PlankThickness - PlankWidth);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth - PlankThickness);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankWidth - PlankHeight, -PlankThickness - PlankWidth);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankWidth - PlankHeight, 3 * PlankThickness - PlankWidth);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth - PlankThickness);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth - 3 * PlankThickness);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankHeight - PlankWidth, 3 * PlankThickness - PlankWidth);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankThickness - PlankWidth);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth - 3 * PlankThickness);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth + PlankThickness);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankThickness - PlankWidth);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth + PlankThickness);
	if (!draw_woodplank(mi, cp, wire))
		return False;
	glPopMatrix();
	return True;
}

static void
reshape_cage(ModeInfo * mi, int width, int height)
{
	cagestruct *cp = &cage[MI_SCREEN(mi)];
	int i;
    int y = 0;

    if (width > height * 5) {   /* tiny window: show middle */
      height = width * 9/16;
      y = -height/2;
    }

	glViewport(0, y, cp->WindW = (GLint) width, cp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);
	i = width / 512 + 1;
	glLineWidth(i);
	glPointSize(i);
}

static void
pinit(ModeInfo *mi)
{
  /* int status; */

    if (MI_IS_WIREFRAME(mi))
      return;

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
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	/* cage */
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
	glShadeModel(GL_FLAT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#if 0
	clear_gl_error();
	if (MI_IS_MONO(mi))
      status = 0;
    else
      status = gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
                                 WoodTextureWidth, WoodTextureHeight,
                                 GL_RGB, GL_UNSIGNED_BYTE, WoodTextureData);
	if (status)
	  {
		const char *s = (char *) gluErrorString (status);
		fprintf (stderr, "%s: error mipmapping texture: %s\n",
				 progname, (s ? s : "(unknown)"));
		exit (1);
	  }
	check_gl_error("mipmapping");
#else
    {
      XImage *img = image_data_to_ximage (mi->dpy, mi->xgwa.visual,
                                          wood_png, sizeof(wood_png));
	  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                    img->width, img->height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, img->data);
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

ENTRYPOINT void
init_cage (ModeInfo * mi)
{
	cagestruct *cp;

	MI_INIT (mi, cage);
	cp = &cage[MI_SCREEN(mi)];

	cp->step = NRAND(90);
	if ((cp->glx_context = init_GL(mi)) != NULL) {

		reshape_cage(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		pinit(mi);
	} else {
		MI_CLEARWINDOW(mi);
	}
}

ENTRYPOINT void
draw_cage (ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	cagestruct *cp;

	if (cage == NULL)
		return;
	cp = &cage[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;
	if (!cp->glx_context)
		return;

    mi->polygon_count = 0;
	glXMakeCurrent(display, window, *cp->glx_context);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_IS_ICONIC(mi)) {
		glScalef(Scale4Window * cp->WindH / cp->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * cp->WindH / cp->WindW, Scale4Iconic, Scale4Iconic);
	}

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    if (o != 0 && o != 180 && o != -180) {
      glScalef (1/h, h, 1/h);  /* #### not quite right */
      h = 1.7;
      glScalef (h, h, h);
    }
    h = 0.7;
    glScalef (h, h, h);
  }
# else
  {
    /* Don't understand why this clause doesn't work on mobile, but it 
       doesn't. */
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glRotatef (current_device_rotation(), 0, 0, 1);
    glScalef (s, s, s);
  }
# endif

	/* cage */
	glRotatef(cp->step * 100, 0, 0, 1);
	glRotatef(25 + cos(cp->step * 5) * 6, 1, 0, 0);
	glRotatef(204.5 - sin(cp->step * 5) * 8, 0, 1, 0);
	if (!draw_impossiblecage(mi, cp, MI_IS_WIREFRAME(mi))) {
		MI_ABORT(mi);
		return;
	}

	glPopMatrix();
	if (MI_IS_FPS(mi)) do_fps (mi);
	glFlush();

	glXSwapBuffers(display, window);

	cp->step += 0.025;
}

#ifndef STANDALONE
ENTRYPOINT void
change_cage (ModeInfo * mi)
{
	cagestruct *cp = &cage[MI_SCREEN(mi)];

	if (!cp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *cp->glx_context);
	pinit(mi);
}
#endif /* !STANDALONE */

ENTRYPOINT void
free_cage (ModeInfo * mi)
{
	/* nothing to do */
}

XSCREENSAVER_MODULE ("Cage", cage)

#endif
