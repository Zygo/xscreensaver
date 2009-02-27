/* -*- Mode: C; tab-width: 4 -*- */
/* extrusion --- extrusion module for xscreensaver */
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

 * Revision History:
 * Tue Oct 19 22:24:47 PDT 1999    Initial creation by David Konerding
 *                                 <dek@cgl.ucsf.edu>
 *                                                                 
 * Notes:
 * This screensaver requires the GLE ("OpenGL Tubing and Extrusion Library")
 * which can be obtained from http://www.linas.org/gle/index.html
  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef STANDALONE
# define PROGCLASS						"Screensaver"
# define HACK_INIT						init_screensaver
# define HACK_DRAW						draw_screensaver
# define HACK_RESHAPE					reshape_screensaver
# define HACK_HANDLE_EVENT				screensaver_handle_event
# define EVENT_MASK						PointerMotionMask
# define screensaver_opts				xlockmore_opts
#define	DEFAULTS                        "*delay:			10000	\n" \
										"*showFPS:      	False	\n" \
										"*light:			True	\n" \
                                        "*wire:				False	\n" \
                                        "*texture:			False	\n" \
										"*image:			BUILTIN	\n" \
                                        "*name:             RANDOM  \n" \
                                        "*example:          0       \n"

# include "xlockmore.h"				/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#ifdef HAVE_GLE3
#include <GL/gle.h>
#else
#include <GL/tube.h>
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xpm-ximage.h"
#include "rotator.h"

#define checkImageWidth 64
#define checkImageHeight 64


extern void InitStuff_helix2(void);
extern void DrawStuff_helix2(void);
extern void InitStuff_helix3(void);
extern void DrawStuff_helix3(void);
extern void InitStuff_helix4(void);
extern void DrawStuff_helix4(void);
extern void InitStuff_joinoffset(void);
extern void DrawStuff_joinoffset(void);
extern void InitStuff_screw(void);
extern void DrawStuff_screw(void);
extern void InitStuff_taper(void);
extern void DrawStuff_taper(void);
extern void InitStuff_twistoid(void);
extern void DrawStuff_twistoid(void);



#define WIDTH 640
#define HEIGHT 480

#define DEF_LIGHT	  	"True"
#define DEF_WIRE   		"False"
#define DEF_TEXTURE		"False"
#define DEF_TEXTURE_QUALITY   "False"
#define DEF_MIPMAP   	"False"
#define DEF_NAME        "RANDOM"
#define DEF_IMAGE   	"BUILTIN"

static int do_light;
static int do_wire;
static int do_texture;
static int do_texture_quality;
static int do_mipmap;
static char *which_name;
static char *which_image;

static XrmOptionDescRec opts[] = {
  {"-light",   ".extrusion.light",     XrmoptionNoArg, "true" },
  {"+light",   ".extrusion.light",     XrmoptionNoArg, "false" },
  {"-wire",    ".extrusion.wire",      XrmoptionNoArg, "true" },
  {"+wire",    ".extrusion.wire",      XrmoptionNoArg, "false" },
  {"-texture", ".extrusion.texture",   XrmoptionNoArg, "true" },
  {"+texture", ".extrusion.texture",   XrmoptionNoArg, "false" },
  {"-texture", ".extrusion.texture",   XrmoptionNoArg, "true" },
  {"+texture_quality", ".extrusion.texture",   XrmoptionNoArg, "false" },
  {"-texture_quality", ".extrusion.texture",   XrmoptionNoArg, "true" },
  {"+mipmap", ".extrusion.mipmap",   XrmoptionNoArg, "false" },
  {"-mipmap", ".extrusion.mipmap",   XrmoptionNoArg, "true" },
  {"-name",   ".extrusion.name",  XrmoptionSepArg, 0 },
  {"-image",   ".extrusion.image",  XrmoptionSepArg, 0 },
};


static argtype vars[] = {
  {&do_light,    "light",   "Light",   DEF_LIGHT,   t_Bool},
  {&do_wire,    "wire",   "Wire",   DEF_WIRE,   t_Bool},
  {&do_texture,    "texture",   "Texture",   DEF_TEXTURE,   t_Bool},
  {&do_texture_quality,    "texture_quality",   "Texture_Quality",   DEF_TEXTURE_QUALITY,   t_Bool},
  {&do_mipmap,    "mipmap",   "Mipmap",   DEF_MIPMAP,   t_Bool},
  {&which_name, "name",   "Name",   DEF_NAME,   t_String},
  {&which_image, "image",   "Image",   DEF_IMAGE,   t_String},
};


static OptionStruct desc[] =
{
  {"-name num", "example 'name' to draw (helix2, helix3, helix4, joinoffset, screw, taper, twistoid)"},
  {"-/+ light", "whether to do enable lighting (slower)"},
  {"-/+ wire", "whether to do use wireframe instead of filled (faster)"},
  {"-/+ texture", "whether to apply a texture (slower)"},
  {"-image <filename>", "texture image to load"},
  {"-/+ texture_quality", "whether to use texture smoothing (slower)"},
  {"-/+ mipmap", "whether to use texture mipmap (slower)"},
};

ModeSpecOpt screensaver_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   screensaver_description =
{"screensaver", "init_screensaver", "draw_screensaver", "release_screensaver",
 "draw_screensaver", "init_screensaver", NULL, &screensaver_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "OpenGL screensaver", 0, NULL};
#endif


/* structure for holding the screensaver data */
typedef struct {
  int screen_width, screen_height;
  GLXContext *glx_context;
  rotator *rot;
  Bool button_down_p;
  int mouse_x, mouse_y;
  Window window;
  XColor fg, bg;
} screensaverstruct;
static screensaverstruct *Screensaver = NULL;




/* convenient access to the screen width */
static int global_width=640, global_height=480;

/* set up a light */
static GLfloat lightOnePosition[] = {40.0, 40, 100.0, 0.0};
static GLfloat lightOneColor[] = {0.99, 0.99, 0.99, 1.0}; 

static GLfloat lightTwoPosition[] = {-40.0, 40, 100.0, 0.0};
static GLfloat lightTwoColor[] = {0.99, 0.99, 0.99, 1.0}; 

float rot_x=0, rot_y=0, rot_z=0;
float lastx=0, lasty=0;

static float max_lastx=300, max_lasty=400;
static float min_lastx=-400, min_lasty=-400;

static int screensaver_number;

struct functions {
  void (*InitStuff)(void);
  void (*DrawStuff)(void);
  char *name;
};

/* currently joinoffset and twistoid look funny-
   like we're looking at them from the back or something
*/

static struct functions funcs_ptr[] = {
  {InitStuff_helix2, DrawStuff_helix2, "helix2"},
  {InitStuff_helix3, DrawStuff_helix3, "helix3"},
  {InitStuff_helix4, DrawStuff_helix4, "helix4"},
  {InitStuff_joinoffset, DrawStuff_joinoffset, "joinoffset"},
  {InitStuff_screw, DrawStuff_screw, "screw"},
  {InitStuff_taper, DrawStuff_taper, "taper"},
  {InitStuff_twistoid, DrawStuff_twistoid, "twistoid"},
};

static int num_screensavers = countof(funcs_ptr);


/* BEGINNING OF FUNCTIONS */


GLubyte *
Generate_Image(int *width, int *height, int *format)
{
  GLubyte *result;
  int i, j, c;
  int counter=0;

  *width = checkImageWidth;
  *height = checkImageHeight;
  result = (GLubyte *)malloc(4 * (*width) * (*height));

  counter = 0;
  for (i = 0; i < checkImageWidth; i++) {
    for (j = 0; j < checkImageHeight; j++) {
      c = (((((i&0x8)==0))^(((j&0x8))==0)))*255;
      result[counter++] = (GLubyte) c;
      result[counter++] = (GLubyte) c;
      result[counter++] = (GLubyte) c;
      result[counter++] = (GLubyte) 255;
    }
  }

  *format = GL_RGBA;
  return result;
}


/* Create a texture in OpenGL.  First an image is loaded 
   and stored in a raster buffer, then it's  */
void Create_Texture(ModeInfo *mi, const char *filename)
{
  int height, width;
  GLubyte *image;
  int format;

  if ( !strncmp(filename, "BUILTIN", 7))
    image = Generate_Image(&width, &height, &format);
  else
    {
      XImage *ximage = xpm_file_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                           MI_COLORMAP (mi), filename);
      image  = (GLubyte *) ximage->data;
      width  = ximage->width;
      height = ximage->height;
      format = GL_RGBA;
    }

  /* GL_MODULATE or GL_DECAL depending on what you want */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  /* perhaps we can edge a bit more speed at the expense of quality */
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  if (do_texture_quality) {
	/* with texture_quality, the min and mag filters look *much* nice but are *much* slower */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }
  else {
	/* default is to do it quick and dirty */
	/* if you have mipmaps turned on, but not texture quality, nothing will happen! */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }

  /* mipmaps make the image look much nicer */
  if (do_mipmap)
    {
      int status;
      clear_gl_error();
      status = gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, format,
                                 GL_UNSIGNED_BYTE, image);
      if (status)
        {
          const char *s = (char *) gluErrorString (status);
          fprintf (stderr, "%s: error mipmapping %dx%d texture: %s\n",
                   progname, width, height,
                   (s ? s : "(unknown)"));
          exit (1);
        }
      check_gl_error("mipmapping");
    }
  else
    {
      clear_gl_error();
      glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                   format, GL_UNSIGNED_BYTE, image);
      check_gl_error("texture");
    }
}


static void
init_rotation (ModeInfo *mi)
{
  screensaverstruct *gp = &Screensaver[MI_SCREEN(mi)];
  double spin_speed = 1.0;
  gp->rot = make_rotator (spin_speed, spin_speed, spin_speed, 1.0, 0.0, True);

  lastx = (random() % (int) (max_lastx - min_lastx)) + min_lastx;
  lasty = (random() % (int) (max_lasty - min_lasty)) + min_lasty;
}


/* draw the screensaver once */
void draw_screensaver(ModeInfo * mi)
{
  screensaverstruct *gp = &Screensaver[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);

  if (!gp->glx_context)
	return;

  glXMakeCurrent(display, window, *(gp->glx_context));

  funcs_ptr[screensaver_number].DrawStuff();
	  
  /* track the mouse only if a button is down. */
  if (gp->button_down_p)
    {
      lastx = gp->mouse_x;
      lasty = gp->mouse_y;
    }
  else
    {
      float scale = (max_lastx - min_lastx);
      double x, y, z;
      get_rotation (gp->rot, &x, &y, &z, True);
      rot_x = x * 360;
      rot_y = y * 360;
      rot_z = z * 360;
      lastx = x * scale + min_lastx;
      lasty = y * scale + min_lasty;
    }

  if (mi->fps_p) do_fps (mi);
  glXSwapBuffers(display, window);
}


/* set up lighting conditions */
static void SetupLight(void)
{
  glLightfv (GL_LIGHT0, GL_POSITION, lightOnePosition);
  glLightfv (GL_LIGHT0, GL_DIFFUSE, lightOneColor);
  glLightfv (GL_LIGHT1, GL_POSITION, lightTwoPosition);
  glLightfv (GL_LIGHT1, GL_DIFFUSE, lightTwoColor);

  glEnable (GL_LIGHT0);
  glEnable (GL_LIGHT1);
  glEnable (GL_LIGHTING);

  glColorMaterial (GL_FRONT, GL_DIFFUSE);
  glColorMaterial (GL_BACK, GL_DIFFUSE);
  glEnable (GL_COLOR_MATERIAL);
}

/* reset the projection matrix */
static void resetProjection(void) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum (-9, 9, -9, 9, 50, 150.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

/* Standard reshape function */
void
reshape_screensaver(ModeInfo *mi, int width, int height)
{
  global_width=width;
  global_height=height;
  glViewport( 0, 0, global_width, global_height );
  resetProjection();
}


/* decide which screensaver example to run */
static void chooseScreensaverExample(ModeInfo *mi)
{
  int i;
  /* call the extrusion init routine */

  if (!strncmp(which_name, "RANDOM", strlen(which_name))) {
    screensaver_number = random() % num_screensavers;
  }
  else {
	screensaver_number=-1;
	for (i=0; i < num_screensavers; i++) {
	  if (!strncmp(which_name, funcs_ptr[i].name, strlen(which_name))) {
		screensaver_number = i;
	  }
	}	  
  }
	
  if (screensaver_number < 0 || screensaver_number >= num_screensavers) {
	fprintf(stderr, "%s: invalid screensaver example number!\n", progname);
	fprintf(stderr, "%s: known screensavers:\n", progname);
	for (i=0; i < num_screensavers; i++)
	  fprintf(stderr,"\t%s\n", funcs_ptr[i].name);
	exit(1);
  }
  init_rotation(mi);
  funcs_ptr[screensaver_number].InitStuff();
}

/* main OpenGL initialization routine */
static void
initializeGL(ModeInfo *mi, GLsizei width, GLsizei height) 
{
  int style;
  int mode;

  reshape_screensaver(mi, width, height);
  glViewport( 0, 0, width, height ); 

  glEnable(GL_DEPTH_TEST);
  glClearColor(0,0,0,0);
/*    glCullFace(GL_BACK); */
/*    glEnable(GL_CULL_FACE); */
  glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, True);
  glShadeModel(GL_SMOOTH);

  if (do_light)
	SetupLight();
  if (do_wire) {
	glPolygonMode(GL_FRONT,GL_LINE);
  	glPolygonMode(GL_BACK,GL_LINE);
  }
  if (do_texture) {
	Create_Texture(mi, which_image);
	glEnable(GL_TEXTURE_2D);

	/* configure the pipeline */
	style = TUBE_JN_CAP;
	style |= TUBE_CONTOUR_CLOSED;
	style |= TUBE_NORM_FACET;
	style |= TUBE_JN_ANGLE;
	gleSetJoinStyle (style);

	if (do_texture) {
	  mode = GLE_TEXTURE_ENABLE | GLE_TEXTURE_VERTEX_MODEL_FLAT;
	  glMatrixMode (GL_TEXTURE); glLoadIdentity ();
	  glScalef (0.25, 0.1, 1); glMatrixMode (GL_MODELVIEW);
	  gleTextureMode (mode);
	}
  }

}

Bool
screensaver_handle_event (ModeInfo *mi, XEvent *event)
{
  screensaverstruct *gp = &Screensaver[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      gp->button_down_p = True;
      gp->mouse_x = event->xbutton.x;
      gp->mouse_y = event->xbutton.y;
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      gp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify)
    {
      gp->mouse_x = event->xmotion.x;
      gp->mouse_y = event->xmotion.y;
      return True;
    }

  return False;
}



/* xscreensaver initialization routine */
void init_screensaver(ModeInfo * mi)
{
  int screen = MI_SCREEN(mi);
  screensaverstruct *gp;

  if (Screensaver == NULL) {
	if ((Screensaver = (screensaverstruct *) calloc(MI_NUM_SCREENS(mi), sizeof (screensaverstruct))) == NULL)
	  return;
  }
  gp = &Screensaver[screen];

  gp->window = MI_WINDOW(mi);
  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape_screensaver(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	initializeGL(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	chooseScreensaverExample(mi);
  } else {
	MI_CLEARWINDOW(mi);
  }

}

/* all sorts of nice cleanup code should go here! */
void release_screensaver(ModeInfo * mi)
{
  int screen;
  if (Screensaver != NULL) {
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	  /*	  screensaverstruct *gp = &Screensaver[screen];*/
	}
	(void) free((void *) Screensaver);
	Screensaver = NULL;
  }
  FreeAllGL(mi);
}
#endif

