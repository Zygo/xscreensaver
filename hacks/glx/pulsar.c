/* -*- Mode: C; tab-width: 4 -*- */
/* pulsar --- pulsar module for xpulsar */
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
 * 4-Apr-1999:  dek@cgl.ucsf.edu  Created module "pulsar"
 * 27-Apr-1999:  dek@cgl.ucsf.edu  Submitted module "pulsar"
 * 4-May-1999:  jwz@jwz.org  Added module "pulsar"
 * 4-May-1999:  dek@cgl.ucsf.edu  Submitted module "pulsar" updates
 *
 * Notes:
 * The pulsar pulsar draws a number of rotating, pulsing rectangles
 * on your screen.  Depending on the options you choose, you can set a number
 * of interesting OpenGL parameters, including alpha blending, depth testing, fog,
 * lighting, texturing, mipmapping, bilinear filtering, and line antialiasing.  
 * Additionally, there is a "frames per second" meter which gives an estimate of
 * the speed of your graphics card.  
 *
 * Example command-line switches:
 * Only draw a single quad, and don't fill it (boring but fast)
 * pulsar -wire -quads 1 -fps
 *
 * Only try this with hardware graphics acceleration (PPro 200 w/ a Voodoo 2 runs great)
 * pulsar -quads 10 -texture -mipmap -texture_quality -light -fog -fps
 *                                                                 
 */

#include <math.h> 
#include <stdio.h>
#include <stdlib.h>

#ifdef STANDALONE
#define	DEFAULTS                       	"*delay:			10000   \n" \
										"*showFPS:          False   \n" \

# define refresh_pulsar 0
# define pulsar_handle_event 0
# include "xlockmore.h"				/* from the xpulsar distribution */
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

#include "xpm-ximage.h"

/* Functions for loading and storing textures */

#define checkImageWidth 64
#define checkImageHeight 64

/* Functions for handling the frames per second timer */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define WIDTH 800
#define HEIGHT 600

#define NUM_QUADS 5
#define DEF_QUADS   	"5"
#define DEF_LIGHT	  	"False"
#define DEF_WIRE   		"False"
#define DEF_BLEND   	"True"
#define DEF_FOG   		"False"
#define DEF_ANTIALIAS   "False"
#define DEF_TEXTURE   	"False"
#define DEF_TEXTURE_QUALITY   "False"
#define DEF_MIPMAP   	"False"
#define DEF_DO_DEPTH	"False"
#define DEF_IMAGE   	"BUILTIN"

static int num_quads;
static int do_light;
static int do_wire;
static int do_blend;
static int do_fog;
static int do_antialias;
static int do_texture;
static int do_texture_quality;
static int do_mipmap;
static int do_depth;
static char *which_image;


static XrmOptionDescRec opts[] = {
  {"-quads",   ".pulsar.quads",   XrmoptionSepArg, 0 },
  {"-light",   ".pulsar.light",   XrmoptionNoArg, "true" },
  {"+light",   ".pulsar.light",   XrmoptionNoArg, "false" },
  {"-wire",   ".pulsar.wire",   XrmoptionNoArg, "true" },
  {"+wire",   ".pulsar.wire",   XrmoptionNoArg, "false" },
  {"-blend",   ".pulsar.blend",   XrmoptionNoArg, "true" },
  {"+blend",   ".pulsar.blend",   XrmoptionNoArg, "false" },
  {"-fog",   ".pulsar.fog",   XrmoptionNoArg, "true" },
  {"+fog",   ".pulsar.fog",   XrmoptionNoArg, "false" },
  {"-antialias",   ".pulsar.antialias",   XrmoptionNoArg, "true" },
  {"+antialias",   ".pulsar.antialias",   XrmoptionNoArg, "false" },
  {"-texture",   ".pulsar.texture",   XrmoptionNoArg, "true" },
  {"+texture",   ".pulsar.texture",   XrmoptionNoArg, "false" },
  {"-texture_quality",   ".pulsar.textureQuality",   XrmoptionNoArg, "true" },
  {"+texture_quality",   ".pulsar.textureQuality",   XrmoptionNoArg, "false" },
  {"-mipmap",   ".pulsar.mipmap",   XrmoptionNoArg, "true" },
  {"+mipmap",   ".pulsar.mipmap",   XrmoptionNoArg, "false" },
  {"-do_depth",   ".pulsar.doDepth",   XrmoptionNoArg, "true" },
  {"+do_depth",   ".pulsar.doDepth",   XrmoptionNoArg, "false" },
  {"-image",   ".pulsar.image",  XrmoptionSepArg, 0 },
};


static argtype vars[] = {
  {&num_quads,    "quads",     "Quads",     DEF_QUADS,     t_Int},
  {&do_light,     "light",     "Light",     DEF_LIGHT,     t_Bool},
  {&do_wire,      "wire",      "Wire",      DEF_WIRE,      t_Bool},
  {&do_blend,     "blend",     "Blend",     DEF_BLEND,     t_Bool},
  {&do_fog,       "fog",       "Fog",       DEF_FOG,       t_Bool},
  {&do_antialias, "antialias", "Antialias", DEF_ANTIALIAS, t_Bool},
  {&do_texture,   "texture",   "Texture",   DEF_TEXTURE,   t_Bool},
  {&do_texture_quality, "textureQuality", "TextureQuality", DEF_TEXTURE_QUALITY,   t_Bool},
  {&do_mipmap,    "mipmap",    "Mipmap",    DEF_MIPMAP,    t_Bool},
  {&do_depth,    "doDepth",    "DoDepth",   DEF_DO_DEPTH,  t_Bool},
  {&which_image, "image",      "Image",     DEF_IMAGE,     t_String},
};


static OptionStruct desc[] =
{
	{"-quads num", "how many quads to draw"},
	{"-/+ light", "whether to do enable lighting (slower)"},
	{"-/+ wire", "whether to do use wireframe instead of filled (faster)"},
	{"-/+ blend", "whether to do enable blending (slower)"},
	{"-/+ fog", "whether to do enable fog (?)"},
	{"-/+ antialias", "whether to do enable antialiased lines (slower)"},
	{"-/+ texture", "whether to do enable texturing (much slower)"},
	{"-/+ texture_quality", "whether to do enable linear/mipmap filtering (much much slower)"},
	{"-/+ mipmap", "whether to do enable mipmaps (much slower)"},
	{"-/+ depth", "whether to do enable depth buffer checking (slower)"},
	{"-image <filename>", "texture image to load"},
};

ENTRYPOINT ModeSpecOpt pulsar_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   pulsar_description =
{"pulsar", "init_pulsar", "draw_pulsar", "release_pulsar",
 "draw_pulsar", "init_pulsar", NULL, &pulsar_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "OpenGL pulsar", 0, NULL};
#endif

struct quad
{
  GLfloat tx, ty, tz;
  GLfloat rx, ry, rz;

  GLfloat dtx, dty, dtz;
  GLfloat drx, dry, drz;

};


/* structure for holding the pulsar data */
typedef struct {
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
  XColor fg, bg;

  GLint quad_list;
  float scale_x, scale_y, scale_z;
  int frame;

  struct quad *quads;

} pulsarstruct;

static pulsarstruct *Pulsar = NULL;

static GLubyte *
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
static void Create_Texture(ModeInfo *mi, const char *filename)
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

static void resetProjection(void) 
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1, 1, -1, 1, 1, 100); 
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}


static void GenerateQuad(pulsarstruct *gp)
{
  int i;

  gp->quad_list = glGenLists(1);
  glNewList(gp->quad_list,GL_COMPILE);
#if 1
  glBegin(GL_QUADS);
  glColor4f(1,0,0,.4); glNormal3f(0,0,1);  glTexCoord2f(0,0); glVertex2f(-1, -1);
  glColor4f(0,1,0,.4); glNormal3f(0,0,1);  glTexCoord2f(0,1); glVertex2f(-1,  1);
  glColor4f(0,0,1,.4); glNormal3f(0,0,1);  glTexCoord2f(1,1); glVertex2f( 1,  1);
  glColor4f(1,1,1,1); glNormal3f(0,0,1);  glTexCoord2f(1,0); glVertex2f( 1,  -1);
#else
  glBegin(GL_TRIANGLE_STRIP);
  glColor4f(0,1,0,.4); glNormal3f(0,0,1);  glTexCoord2f(0,1); glVertex2f(-1,  1);
  glColor4f(1,0,0,.4); glNormal3f(0,0,1);  glTexCoord2f(0,0); glVertex2f(-1, -1);
  glColor4f(0,0,1,.4); glNormal3f(0,0,1);  glTexCoord2f(1,1); glVertex2f( 1,  1);
  glColor4f(1,1,1,.4); glNormal3f(0,0,1);  glTexCoord2f(1,0); glVertex2f( 1,  -1);
#endif
  glEnd();
  glEndList();

  gp->quads = (struct quad *) malloc(sizeof(struct quad) * num_quads);
  for (i=0; i < num_quads; i++)
    {
      gp->quads[i].rx = 0.;
      gp->quads[i].ry = 0.;
      gp->quads[i].rz = 0.;
      gp->quads[i].tx = 0.;
      gp->quads[i].ty = 0.;
      gp->quads[i].tz = -10;

      gp->quads[i].drx = frand(5.0);
      gp->quads[i].dry = frand(5.0);
      gp->quads[i].drz = 0;
    }
}

static void initializeGL(ModeInfo *mi, GLsizei width, GLsizei height) 
{
  pulsarstruct *gp = &Pulsar[MI_SCREEN(mi)];
  GLfloat fogColor[4] = { 0.1, 0.1, 0.1, 0.1 };

  glViewport( 0, 0, width, height ); 
  resetProjection();

  if (do_depth)
	glEnable(GL_DEPTH_TEST);

  if (do_antialias) {
	do_blend = 1;
	glEnable(GL_LINE_SMOOTH);
  }

  if (do_blend) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }


  if (do_light) {
	glShadeModel(GL_SMOOTH);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  }

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  do_wire = 0;
# endif

  if (do_wire)
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else 
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  if (do_fog) {
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_DENSITY, 0.35);
/*    	glHint(GL_FOG_HINT, GL_FASTEST); */
	glFogf(GL_FOG_START, 50.0);
	glFogf(GL_FOG_END, 100.0);
  }
	

  if (do_texture)
	  Create_Texture(mi, which_image); 

  GenerateQuad(gp);
}

static void drawQuads(pulsarstruct *gp) 
{
  int i;
  for (i=0; i < num_quads; i++)
    {
      glPushMatrix();
      glTranslatef(gp->quads[i].tx,0,0);
      glTranslatef(0,gp->quads[i].ty,0);
      glTranslatef(0,0,gp->quads[i].tz);
      glRotatef(gp->quads[i].rx, 1,0,0);
      glRotatef(gp->quads[i].ry, 0,1,0);
      glRotatef(gp->quads[i].rz, 0,0,1);
      glCallList(gp->quad_list);
      glPopMatrix();

      gp->quads[i].rx += gp->quads[i].drx;
      gp->quads[i].ry += gp->quads[i].dry;
      gp->quads[i].rz += gp->quads[i].drz;

    }
}

static GLvoid drawScene(ModeInfo * mi) 
{
  pulsarstruct *gp = &Pulsar[MI_SCREEN(mi)];
/*  check_gl_error ("drawScene"); */
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* we have to do this here because the FPS meter turns these 3 features off!! */
  {
	if (do_light) {
	  glEnable(GL_LIGHTING);
	  glEnable(GL_LIGHT0);
	}
	
	if (do_texture) {
	  glEnable(GL_TEXTURE_2D);
	}
	
	if (do_blend) {
	  glEnable(GL_BLEND);
	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
  }

  resetProjection();

  /* use XYZ scaling factors to change the size of the pulsar */
  glScalef(gp->scale_x, gp->scale_y, gp->scale_z);
  drawQuads(gp);
  mi->polygon_count = num_quads;

  /* update the scaling factors- cyclic */
  gp->scale_x = cos(gp->frame/360.)*10.;
  gp->scale_y = sin(gp->frame/360.)*10.;
  gp->scale_z = 1;

  /* increment frame-counter */
  gp->frame++;

/*  check_gl_error ("drawScene"); */
}


ENTRYPOINT void draw_pulsar(ModeInfo * mi)
{
  pulsarstruct *gp = &Pulsar[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);

  if (!gp->glx_context)
	return;

  glXMakeCurrent(display, window, *(gp->glx_context));
  drawScene(mi);
  if (mi->fps_p) do_fps (mi);
  glXSwapBuffers(display, window);
}

/* Standard reshape function */
ENTRYPOINT void
reshape_pulsar(ModeInfo *mi, int width, int height)
{
  glViewport( 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi) );
  resetProjection();
}

ENTRYPOINT void
init_pulsar(ModeInfo * mi)
{
  int screen = MI_SCREEN(mi);

  pulsarstruct *gp;

  if (Pulsar == NULL) {
	if ((Pulsar = (pulsarstruct *) calloc(MI_NUM_SCREENS(mi), sizeof (pulsarstruct))) == NULL)
	  return;
  }
  gp = &Pulsar[screen];

  gp->window = MI_WINDOW(mi);

  gp->scale_x = gp->scale_y = gp->scale_z = 1;

  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape_pulsar(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	initializeGL(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
	MI_CLEARWINDOW(mi);
  }
}


/* all sorts of nice cleanup code should go here! */
ENTRYPOINT void release_pulsar(ModeInfo * mi)
{
  int screen;
  if (Pulsar != NULL) {
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	  pulsarstruct *gp = &Pulsar[screen];
      free(gp->quads);
	}
	(void) free((void *) Pulsar);
	Pulsar = NULL;
  }
  FreeAllGL(mi);
}
#endif

XSCREENSAVER_MODULE ("Pulsar", pulsar)
