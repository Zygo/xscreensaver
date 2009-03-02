/* -*- Mode: C; tab-width: 4 -*- */
/* atunnels --- OpenGL Advanced Tunnel Screensaver */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)atunnels.c	5.13 2004/07/19 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 2002-2004. */

/*
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
 * The original code for this mode was written by Roman Podobedov
 * Email: romka@ut.ee
 * WEB: http://romka.demonews.com
 *
 * Eric Lassauge  (March-16-2002) <lassauge AT users DOT sourceforge DOT net>
 * 				    http://lassauge.free.fr/linux.html
 *
 * REVISION HISTORY:
 *
 * E.Lassauge - 25-May-2004:
 *	- added more texture !
 * E.Lassauge - 16-Mar-2002:
 *	- created based on the Roman demo.
 *	- deleted all external file stuff to use xpm textures and
 *	  hardcoded path point values.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef STANDALONE		/* xscreensaver mode */
# define PROGCLASS		"Atunnels"
# define HACK_INIT		init_atunnels
# define HACK_DRAW		draw_atunnels
# define HACK_RESHAPE		reshape_atunnels
# define atunnels_opts		xlockmore_opts
#define	DEFAULTS                "*delay:	10000	\n" \
				"*showFPS:     	False	\n" \
                                "*light:	True	\n" \
                                "*wire:		False	\n" \
                                "*texture:	True	\n"

# include "xlockmore.h"		/* from the xscreensaver distribution */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
# include "visgl.h"
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#include "tunnel_draw.h"

#ifdef HAVE_XPM
#include "xpm-ximage.h"

#ifdef STANDALONE
#include "../images/tunnel0.xpm"
#include "../images/tunnel1.xpm"
#include "../images/tunnel2.xpm"
#include "../images/tunnel3.xpm"
#include "../images/tunnel4.xpm"
#include "../images/tunnel5.xpm"
#else /* !STANDALONE */
#include "pixmaps/tunnel0.xpm"
#include "pixmaps/tunnel1.xpm"
#include "pixmaps/tunnel2.xpm"
#include "pixmaps/tunnel3.xpm"
#include "pixmaps/tunnel4.xpm"
#include "pixmaps/tunnel5.xpm"
#endif /* !STANDALONE */
#endif /* HAVE_XPM */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEF_LIGHT	"True"
#define DEF_WIRE   	"False"
#define DEF_TEXTURE	"True"
#define MINSIZE         32      /* minimal viewport size */

static int do_light;
static int do_wire;
static int do_texture;

static XrmOptionDescRec opts[] = {
  {(char *)"-light",   (char *)".atunnels.light",      XrmoptionNoArg, (caddr_t) "true" },
  {(char *)"+light",   (char *)".atunnels.light",      XrmoptionNoArg, (caddr_t) "false" },
  {(char *)"-wire",    (char *)".atunnels.wire",       XrmoptionNoArg, (caddr_t) "true" },
  {(char *)"+wire",    (char *)".atunnels.wire",       XrmoptionNoArg, (caddr_t) "false" },
  {(char *)"-texture", (char *)".atunnels.texture",    XrmoptionNoArg, (caddr_t) "true" },
  {(char *)"+texture", (char *)".atunnels.texture",    XrmoptionNoArg, (caddr_t) "false" },
};

static argtype vars[] = {
  {(void *) &do_light,   (char *)"light",  (char *)"Light",   DEF_LIGHT,   t_Bool},
  {(void *) &do_wire,    (char *)"wire",   (char *)"Wire",    DEF_WIRE,    t_Bool},
  {(void *) &do_texture, (char *)"texture",(char *)"Texture", DEF_TEXTURE, t_Bool},
};

static OptionStruct desc[] =
{
  {(char *)"-/+ light",   (char *)"whether to do enable lighting (slower)"},
  {(char *)"-/+ wire",    (char *)"whether to do use wireframe instead of filled (faster)"},
  {(char *)"-/+ texture", (char *)"whether to apply a texture (slower)"},
};

ModeSpecOpt atunnels_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   atunnels_description =
{"atunnels", "init_atunnels", "draw_atunnels", "release_atunnels",
 "draw_atunnels", "init_atunnels", NULL, &atunnels_opts,
 25000, 1, 1, 1, 0, 1.0, "",
 "Shows an OpenGL advanced tunnel screensaver", 0, NULL};
#endif

/* structure for holding the screensaver data */
typedef struct {
  GLint WIDTH, HEIGHT;
  GLXContext *glx_context;
} atunnelsstruct;

static atunnelsstruct *Atunnels = NULL;

/* convenient access to the screen width */
static GLuint texture[MAX_TEXTURE];     /* Textures */

/*=================== Load Texture =========================================*/
static Bool LoadTexture(ModeInfo * mi, char **fn, int t_num)
{
#ifdef HAVE_XPM
	XImage *teximage;    /* Texture data */

        if ((teximage = xpm_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
			 MI_COLORMAP(mi), fn)) == None) {
	    (void) fprintf(stderr, "Error reading the texture.\n");
	    glDeleteTextures(1, &texture[t_num]);
	    return False;
	}

#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, texture[t_num]);
#endif /* HAVE_GLBINDTEXTURE */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, teximage->width, teximage->height,
			0, GL_RGBA, GL_UNSIGNED_BYTE, teximage->data);
	if (check_gl_error("texture"))
		return False;

	/* Texture parameters, LINEAR scaling for better texture quality */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	XDestroyImage(teximage);
	return True;
#else /* !HAVE_XPM */
	return False;
#endif /* !HAVE_XPM */
}

/*=================== Main Initialization ==================================*/
static Bool Init(ModeInfo * mi)
{
	GLfloat light_ambient[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_diffuse[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_specular[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_position[] = {0.0, 0.0, 1.0, 0.0};
	GLfloat fogColor[4] = {0.8, 0.8, 0.8, 1.0};

  	glClearColor(0, 0, 0, 0);
#ifdef HAVE_XPM
	if (do_texture)
	{
		glGenTextures(MAX_TEXTURE, texture);
		if (!LoadTexture(mi, texture0,0))
			do_texture = False;
		else if (!LoadTexture(mi, texture1,1))
			do_texture = False;
		else if (!LoadTexture(mi, texture2,2))
			do_texture = False;
		else if (!LoadTexture(mi, texture3,3))
			do_texture = False;
		else if (!LoadTexture(mi, texture4,4))
			do_texture = False;
		else if (!LoadTexture(mi, texture5,5))
			do_texture = False;
		else
			glEnable(GL_TEXTURE_2D);
	}
#endif
	if (!InitTunnel())
		return False;

	/* Set lighting parameters */
	if (do_light)
	{
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
		glLightfv(GL_LIGHT0, GL_POSITION, light_position);

		/* Enable light 0 */
		glEnable(GL_LIGHT0);
		glDepthFunc(GL_LESS);

		glEnable(GL_LIGHTING);
	}
  	if (do_wire) {
		glDisable(GL_NORMALIZE);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_TEXTURE_2D);
		glPolygonMode(GL_FRONT,GL_LINE);
  		glPolygonMode(GL_BACK,GL_LINE);
  	} else {
		glEnable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		/* Enable fog */
		glFogi(GL_FOG_MODE, GL_EXP);
		glFogfv(GL_FOG_COLOR, fogColor);
		glFogf(GL_FOG_DENSITY, 0.3);
		glEnable(GL_FOG);

		/* Cull face */
		glCullFace(GL_FRONT);
		glEnable(GL_CULL_FACE);
	}

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	return True;
}


/* Standard reshape function */
void
reshape_atunnels(ModeInfo *mi, int width, int height)
{
	float a;

	glViewport((MI_WIDTH(mi) - width) / 2, (MI_HEIGHT(mi) - height) / 2, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	a = (float)width/(float)height;
	glFrustum(-0.1*a, 0.1*a, -0.1, 0.1, 0.1, 10);
	glMatrixMode(GL_MODELVIEW);
}

/* draw the screensaver once */
void draw_atunnels(ModeInfo * mi)
{
  	atunnelsstruct *gp;
  	Display    *display = MI_DISPLAY(mi);
  	Window      window = MI_WINDOW(mi);

	if (Atunnels == NULL)
		return;
  	gp = &Atunnels[MI_SCREEN(mi)];
  	if (!gp->glx_context)
		return;

  	glXMakeCurrent(display, window, *(gp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();

	DrawTunnel(do_texture, do_light, texture);
	SplashScreen(do_wire, do_texture, do_light);

	glFlush();
  	if (MI_IS_FPS(mi)) do_fps (mi);
  	glXSwapBuffers(display, window);

}

void
refresh_atunnels(ModeInfo * mi)
{
}


/* xscreensaver initialization routine */
void init_atunnels(ModeInfo * mi)
{
  int screen = MI_SCREEN(mi);
  int size = MI_SIZE(mi);
  atunnelsstruct *gp;

  if (Atunnels == NULL) {
	if ((Atunnels = (atunnelsstruct *) calloc(MI_NUM_SCREENS(mi), sizeof (atunnelsstruct))) == NULL)
	  return;
  }
  gp = &Atunnels[screen];

  /* Viewport is specified size if size >= MINSIZE && size < screensize */
  if (size <= 1) {
      gp->WIDTH = MI_WIDTH(mi);
      gp->HEIGHT = MI_HEIGHT(mi);
  } else if (size < MINSIZE) {
      gp->WIDTH = MINSIZE;
      gp->HEIGHT = MINSIZE;
  } else {
      gp->WIDTH = (size > MI_WIDTH(mi)) ? MI_WIDTH(mi) : size;
      gp->HEIGHT = (size > MI_HEIGHT(mi)) ? MI_HEIGHT(mi) : size;
  }

  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape_atunnels(mi, gp->WIDTH, gp->HEIGHT);
	glDrawBuffer(GL_BACK);
	(void) Init(mi);
  } else {
	MI_CLEARWINDOW(mi);
  }

}

/* all sorts of nice cleanup code should go here! */
void release_atunnels(ModeInfo * mi)
{
  if (Atunnels != NULL) {
#if 0
  	int screen;
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	  atunnelsstruct *gp = &Atunnels[screen];
	   FreeTunnel(gp->path);
	}
#endif
	free(Atunnels);
	Atunnels = NULL;
  }
  FreeAllGL(mi);
}
#endif

void change_atunnels(ModeInfo * mi)
{
  	atunnelsstruct *gp;

	if (Atunnels == NULL)
		return;
  	gp = &Atunnels[MI_SCREEN(mi)];
	if (!gp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(gp->glx_context));
	(void) Init(mi);
}

