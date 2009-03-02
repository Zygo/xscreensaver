/* -*- Mode: C; tab-width: 4 -*- */
/* atunnels --- OpenGL Advanced Tunnel Screensaver */

#if 0
static const char sccsid[] = "@(#)atunnels.c	5.07 2003/02/12 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 2003. */

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
 * Eric Lassauge  (March-16-2002) <lassauge@mail.dotcom.fr>
 * 				    http://lassauge.free.fr/linux.html
 *
 * REVISION HISTORY:
 *
 * E.Lassauge - 16-Mar-2002:
 *	- created based on the Roman demo.
 *	- deleted all external file stuff to use xpm textures and
 *	  hardcoded path point values.
 *
 */

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

#define MODE_atunnels
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else 				/* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif 				/* !STANDALONE */

#ifdef MODE_atunnels /* whole file */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "tunnel_draw.h"

#if defined( USE_XPM ) || defined( USE_XPMINC ) || defined(STANDALONE)
/* USE_XPM & USE_XPMINC in xlock mode ; STANDALONE in xscreensaver mode */
#include "xpm-ximage.h"
#define I_HAVE_XPM

#ifdef STANDALONE
#include "../images/tunnel0.xpm"
#include "../images/tunnel1.xpm"
#include "../images/tunnel2.xpm"
#else /* !STANDALONE */
#include "pixmaps/tunnel0.xpm"
#include "pixmaps/tunnel1.xpm"
#include "pixmaps/tunnel2.xpm"
#endif /* !STANDALONE */
#endif /* HAVE_XPM */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEF_LIGHT	"True"
#define DEF_WIRE   	"False"
#define DEF_TEXTURE	"True"

static Bool do_light;
static Bool do_wire;
static Bool do_texture;

static XrmOptionDescRec opts[] = {
  {(char *)"-light",   (char *)".atunnels.light",      XrmoptionNoArg, (caddr_t) "true" },
  {(char *)"+light",   (char *)".atunnels.light",      XrmoptionNoArg, (caddr_t) "false" },
  {(char *)"-wire",    (char *)".atunnels.wire",       XrmoptionNoArg, (caddr_t) "true" },
  {(char *)"+wire",    (char *)".atunnels.wire",       XrmoptionNoArg, (caddr_t) "false" },
  {(char *)"-texture", (char *)".atunnels.texture",    XrmoptionNoArg, (caddr_t) "true" },
  {(char *)"+texture", (char *)".atunnels.texture",    XrmoptionNoArg, (caddr_t) "false" },
};

static argtype vars[] = {
  {&do_light,   "light",   "Light",   DEF_LIGHT,   t_Bool},
  {&do_wire,    "wire",    "Wire",    DEF_WIRE,    t_Bool},
  {&do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
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
 1000, 1, 2, 1, 4, 1.0, "",
 "OpenGL advanced tunnel screensaver", 0, NULL};
#endif

/* structure for holding the screensaver data */
typedef struct {
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
} atunnelsstruct;

static atunnelsstruct *Atunnels = NULL;
static GLuint texture[3]; /* texture id: GL world */

/*=================== Load Texture =========================================*/
static void LoadTexture(ModeInfo * mi, char **fn, int t_num)
{
#if defined( I_HAVE_XPM )
	XImage *teximage;    /* Texture data */
 
        if ((teximage = xpm_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
			 MI_COLORMAP(mi), fn)) == None) {
	    (void) fprintf(stderr, "Error reading the texture.\n");
	    glDeleteTextures(1, &texture[t_num]);
            do_texture = False;
#ifdef STANDALONE
	    exit(0);
#else
	    return;
#endif
	}

#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, texture[t_num]);
#endif /* HAVE_GLBINDTEXTURE */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, teximage->width, teximage->height, 
			0, GL_RGBA, GL_UNSIGNED_BYTE, teximage->data);
	check_gl_error("texture");

	/* Texture parameters, LINEAR scaling for better texture quality */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 

	XDestroyImage(teximage);
#else /* !I_HAVE_XPM */
	do_texture = False;
#endif /* !I_HAVE_XPM */
} 

/*=================== Main Initialization ==================================*/
static void Init(ModeInfo * mi)
{
	GLfloat light_ambient[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_diffuse[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_specular[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_position[] = {0.0, 0.0, 1.0, 0.0};
	GLfloat fogColor[4] = {0.8, 0.8, 0.8, 1.0};

  	glClearColor(0, 0, 0, 0);
	if (do_texture)
	{
		glGenTextures(3, texture);
		LoadTexture(mi, texture0,0);
		LoadTexture(mi, texture1,1);
		LoadTexture(mi, texture2,2);
		glEnable(GL_TEXTURE_2D);
	}
	InitTunnel();
	
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
  	}
	else
	{
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
}


/* Standard reshape function */
void
reshape_atunnels(ModeInfo *mi, int width, int height)
{
	float a;

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	a = (float)width/(float)height;
	glFrustum(-0.1*a, 0.1*a, -0.1, 0.1, 0.1, 10);
	glMatrixMode(GL_MODELVIEW);
}

/* draw the screensaver once */
void draw_atunnels(ModeInfo * mi)
{
  	atunnelsstruct *sa = &Atunnels[MI_SCREEN(mi)];
  	Display    *display = MI_DISPLAY(mi);
  	Window      window = MI_WINDOW(mi);

  	if (!sa->glx_context)
		return;

  	glXMakeCurrent(display, window, *(sa->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();

	DrawTunnel(do_texture, do_light, texture);
	SplashScreen(do_wire, do_texture, do_light);

	glFlush();  
	/* manage framerate display */
    	if (MI_IS_FPS(mi)) do_fps (mi);
  	glXSwapBuffers(display, window);

}


/* xscreensaver initialization routine */
void init_atunnels(ModeInfo * mi)
{
  int screen = MI_SCREEN(mi);
  atunnelsstruct *sa;

  if (Atunnels == NULL) {
	if ((Atunnels = (atunnelsstruct *) calloc(MI_NUM_SCREENS(mi), sizeof (atunnelsstruct))) == NULL)
	  return;
  }
  sa = &Atunnels[screen];

  sa->window = MI_WINDOW(mi);
  if ((sa->glx_context = init_GL(mi)) != NULL) {
	reshape_atunnels(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	Init(mi);
  } else {
	MI_CLEARWINDOW(mi);
  }

}

/* all sorts of nice cleanup code should go here! */
void release_atunnels(ModeInfo * mi)
{
  int screen;
  if (Atunnels != NULL) {
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	  /*	  atunnelsstruct *sa = &Atunnels[screen];*/
	}
	(void) free((void *) Atunnels);
	Atunnels = NULL;
  }
  FreeAllGL(mi);
}
#endif
