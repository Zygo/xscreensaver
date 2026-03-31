/* -*- Mode: C; tab-width: 4 -*- */
/* atunnel --- OpenGL Advanced Tunnel Screensaver */

#if 0
static const char sccsid[] = "@(#)atunnel.c	5.13 2004/05/25 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 2003-2004. */

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
 * Eric Lassauge  (May-25-2004) <lassauge@users.sourceforge.net>
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

#ifdef STANDALONE		/* xscreensaver mode */
#define	DEFAULTS                "*delay:	10000	\n" \
                                "*showFPS:  False   \n" \
								"*suppressRotationAnimation: True\n" \

# define release_atunnel 0
# define atunnel_handle_event 0
#define MODE_atunnel
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else 				/* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif 				/* !STANDALONE */

#ifdef MODE_atunnel /* whole file */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "tunnel_draw.h"

#if defined( USE_XPM ) || defined( USE_XPMINC ) || defined(STANDALONE)
/* USE_XPM & USE_XPMINC in xlock mode ; STANDALONE in xscreensaver mode */
#include "ximage-loader.h"
#define I_HAVE_XPM

#include "images/gen/tunnel0_png.h"
#include "images/gen/tunnel1_png.h"
#include "images/gen/tunnel2_png.h"
#include "images/gen/tunnel3_png.h"
#include "images/gen/tunnel4_png.h"
#include "images/gen/tunnel5_png.h"
#endif /* HAVE_XPM */


#define DEF_LIGHT	"True"
#define DEF_WIRE    "False"
#define DEF_TEXTURE	"True"

static Bool do_light;
static Bool do_wire;
static Bool do_texture;

static XrmOptionDescRec opts[] = {
  {"-light",   ".atunnel.light",      XrmoptionNoArg, "true" },
  {"+light",   ".atunnel.light",      XrmoptionNoArg, "false" },
  {"-wireframe",".atunnel.wire",       XrmoptionNoArg, "true" },
  {"+wireframe",".atunnel.wire",       XrmoptionNoArg, "false" },
  {"-texture", ".atunnel.texture",    XrmoptionNoArg, "true" },
  {"+texture", ".atunnel.texture",    XrmoptionNoArg, "false" },
};

static argtype vars[] = {
  {&do_light,   "light",   "Light",   DEF_LIGHT,   t_Bool},
  {&do_wire,    "wire",    "Wire",    DEF_WIRE,    t_Bool},
  {&do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
};

static OptionStruct desc[] =
{
  {"-/+ light",   "whether to do enable lighting (slower)"},
  {"-/+ wire",    "whether to do use wireframe instead of filled (faster)"},
  {"-/+ texture", "whether to apply a texture (slower)"},
};

ENTRYPOINT ModeSpecOpt atunnel_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   atunnel_description =
{"atunnel", "init_atunnel", "draw_atunnel", NULL,
 "draw_atunnel", "init_atunnel", "free_atunnel", &atunnel_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "OpenGL advanced tunnel screensaver", 0, NULL};
#endif

/* structure for holding the screensaver data */
typedef struct {
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
  struct tunnel_state *ts;
  GLuint texture[MAX_TEXTURE]; /* texture id: GL world */
} atunnelstruct;

static atunnelstruct *Atunnel = NULL;

/*=================== Load Texture =========================================*/
static void LoadTexture(ModeInfo * mi,
                        const unsigned char *data, unsigned long size,
                        int t_num)
{
#if defined( I_HAVE_XPM )
  	atunnelstruct *sa = &Atunnel[MI_SCREEN(mi)];
	XImage *teximage;    /* Texture data */
 
        if ((teximage = image_data_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
                                             data, size))
            == None) {
	    (void) fprintf(stderr, "Error reading the texture.\n");
	    glDeleteTextures(1, &sa->texture[t_num]);
            do_texture = False;
#ifdef STANDALONE
	    exit(0);
#else
	    return;
#endif
	}

#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, sa->texture[t_num]);
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
  	atunnelstruct *sa = &Atunnel[MI_SCREEN(mi)];
	GLfloat light_ambient[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_diffuse[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_specular[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat light_position[] = {0.0, 0.0, 1.0, 0.0};
	GLfloat fogColor[4] = {0.8, 0.8, 0.8, 1.0};

	if (do_texture)
	{
		glGenTextures(MAX_TEXTURE, sa->texture);
		LoadTexture(mi, tunnel0_png, sizeof(tunnel0_png),0);
		LoadTexture(mi, tunnel1_png, sizeof(tunnel1_png),1);
		LoadTexture(mi, tunnel2_png, sizeof(tunnel2_png),2);
		LoadTexture(mi, tunnel3_png, sizeof(tunnel3_png),3);
		LoadTexture(mi, tunnel4_png, sizeof(tunnel4_png),4);
		LoadTexture(mi, tunnel5_png, sizeof(tunnel5_png),5);
		glEnable(GL_TEXTURE_2D);
	}
	sa->ts = atunnel_InitTunnel();
	
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

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
    do_wire = 0;
# endif

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
ENTRYPOINT void
reshape_atunnel(ModeInfo *mi, int width, int height)
{
  double h = (GLfloat) height / (GLfloat) width;  
  int y = 0;

  if (width > height * 2) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport(0, y, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-0.1*(1/h), 0.1*(1/h), -0.1, 0.1, 0.1, 10);
  glMatrixMode(GL_MODELVIEW);
}

/* draw the screensaver once */
ENTRYPOINT void draw_atunnel(ModeInfo * mi)
{
  	atunnelstruct *sa = &Atunnel[MI_SCREEN(mi)];
  	Display    *display = MI_DISPLAY(mi);
  	Window      window = MI_WINDOW(mi);

  	if (!sa->glx_context)
		return;

  	glXMakeCurrent(display, window, *sa->glx_context);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();

	atunnel_DrawTunnel(sa->ts, do_texture, do_light, sa->texture);
	atunnel_SplashScreen(sa->ts, do_wire, do_texture, do_light);

	glFlush();  
	/* manage framerate display */
    	if (MI_IS_FPS(mi)) do_fps (mi);
  	glXSwapBuffers(display, window);

}


/* xscreensaver initialization routine */
ENTRYPOINT void init_atunnel(ModeInfo * mi)
{
  int screen = MI_SCREEN(mi);
  atunnelstruct *sa;

  MI_INIT(mi, Atunnel);
  sa = &Atunnel[screen];

  sa->window = MI_WINDOW(mi);
  if ((sa->glx_context = init_GL(mi)) != NULL) {
	reshape_atunnel(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	Init(mi);
  } else {
	MI_CLEARWINDOW(mi);
  }

}

ENTRYPOINT void free_atunnel(ModeInfo * mi)
{
  atunnelstruct *sa = &Atunnel[MI_SCREEN(mi)];
  if (!sa->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *sa->glx_context);
  atunnel_FreeTunnel(sa->ts);
  glDeleteTextures (MAX_TEXTURE, sa->texture);
}

XSCREENSAVER_MODULE ("Atunnel", atunnel)

#endif
