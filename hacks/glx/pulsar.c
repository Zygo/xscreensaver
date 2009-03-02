/* -*- Mode: C; tab-width: 4 -*- */
/* pulsar --- pulsar module for xscreensaver */
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
 * The pulsar screensaver draws a number of rotating, pulsing rectangles
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

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS						"Screensaver"
# define HACK_INIT						init_screensaver
# define HACK_DRAW						draw_screensaver
# define HACK_RESHAPE					reshape_screensaver
# define screensaver_opts				xlockmore_opts
#define	DEFAULTS                       	"*delay:			10000   \n" \
										"*showFPS:          False   \n" \
										"*light:			False	\n" \
                                        "*wire:				False	\n" \
                                        "*quads:			5   	\n" \
                                        "*blend:			True	\n" \
                                        "*fog:				False	\n" \
                                        "*antialias:		False	\n" \
                                        "*texture:			False	\n" \
                                        "*texture_quality:	False	\n" \
                                        "*mipmap:			False	\n" \
                                        "*doDepth:			False	\n" \
										"*image:			BUILTIN	\n"

# include "xlockmore.h"				/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#ifdef HAVE_XPM
# include <X11/xpm.h>
# ifndef PIXEL_ALREADY_TYPEDEFED
# define PIXEL_ALREADY_TYPEDEFED /* Sigh, Xmu/Drawing.h needs this... */
# endif
#endif

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif


#include <GL/gl.h>
#include <GL/glu.h>

#include <string.h>
#include <malloc.h>
#include <stdio.h>

/* Functions for loading and storing textures */

#define checkImageWidth 64
#define checkImageHeight 64

/* Functions for handling the frames per second timer */
#include "GL/glx.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define WIDTH 800
#define HEIGHT 600

#define NUM_QUADS 5
#define DEF_NUM_QUADS	"5"
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
  {"-quads",   ".pulsar.quads",   XrmoptionSepArg, (caddr_t) NULL },
  {"-light",   ".pulsar.light",   XrmoptionNoArg, (caddr_t) "true" },
  {"+light",   ".pulsar.light",   XrmoptionNoArg, (caddr_t) "false" },
  {"-wire",   ".pulsar.wire",   XrmoptionNoArg, (caddr_t) "true" },
  {"+wire",   ".pulsar.wire",   XrmoptionNoArg, (caddr_t) "false" },
  {"-blend",   ".pulsar.blend",   XrmoptionNoArg, (caddr_t) "true" },
  {"+blend",   ".pulsar.blend",   XrmoptionNoArg, (caddr_t) "false" },
  {"-fog",   ".pulsar.fog",   XrmoptionNoArg, (caddr_t) "true" },
  {"+fog",   ".pulsar.fog",   XrmoptionNoArg, (caddr_t) "false" },
  {"-antialias",   ".pulsar.antialias",   XrmoptionNoArg, (caddr_t) "true" },
  {"+antialias",   ".pulsar.antialias",   XrmoptionNoArg, (caddr_t) "false" },
  {"-texture",   ".pulsar.texture",   XrmoptionNoArg, (caddr_t) "true" },
  {"+texture",   ".pulsar.texture",   XrmoptionNoArg, (caddr_t) "false" },
  {"-texture_quality",   ".pulsar.texture_quality",   XrmoptionNoArg, (caddr_t) "true" },
  {"+texture_quality",   ".pulsar.texture_quality",   XrmoptionNoArg, (caddr_t) "false" },
  {"-mipmap",   ".pulsar.mipmap",   XrmoptionNoArg, (caddr_t) "true" },
  {"+mipmap",   ".pulsar.mipmap",   XrmoptionNoArg, (caddr_t) "false" },
  {"-do_depth",   ".pulsar.doDepth",   XrmoptionNoArg, (caddr_t) "true" },
  {"+do_depth",   ".pulsar.doDepth",   XrmoptionNoArg, (caddr_t) "false" },
  {"-image",   ".pulsar.image",  XrmoptionSepArg, (caddr_t) NULL },
};


static argtype vars[] = {
  {(caddr_t *) &num_quads, "quads", "Quads", DEF_NUM_QUADS, t_Int},
  {(caddr_t *) &do_light,    "light",   "Light",   DEF_LIGHT,   t_Bool},
  {(caddr_t *) &do_wire,    "wire",   "Wire",   DEF_WIRE,   t_Bool},
  {(caddr_t *) &do_blend,    "blend",   "Blend",   DEF_BLEND,   t_Bool},
  {(caddr_t *) &do_fog,    "fog",   "Fog",   DEF_FOG,   t_Bool},
  {(caddr_t *) &do_antialias,    "antialias",   "Antialias",   DEF_ANTIALIAS,   t_Bool},
  {(caddr_t *) &do_texture,    "texture",   "Texture",   DEF_TEXTURE,   t_Bool},
  {(caddr_t *) &do_texture_quality,    "texture_quality",   "Texture_quality",   DEF_TEXTURE_QUALITY,   t_Bool},
  {(caddr_t *) &do_mipmap,    "mipmap",   "Mipmap",   DEF_MIPMAP,   t_Bool},
  {(caddr_t *) &do_depth,    "doDepth",   "DoDepth",   DEF_DO_DEPTH,   t_Bool},
  {(caddr_t *) &which_image, "image",   "Image",   DEF_IMAGE,   t_String},
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
	{"-image <filename>", "texture image to load (PPM, PPM4, TIFF(?), XPM(?))"},
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
  Window window;
  XColor fg, bg;
} screensaverstruct;

static screensaverstruct *Screensaver = NULL;

struct quad
{
  GLfloat tx, ty, tz;
  GLfloat rx, ry, rz;

  GLfloat dtx, dty, dtz;
  GLfloat drx, dry, drz;

};


GLint quad_list;

static float scale_x=1, scale_y=1, scale_z=1;
static int frame = 0;

struct quad *quads;

GLubyte *Generate_Image(int *width, int *height, int *format)
{
  GLubyte *result;
  int i, j, c;
  int counter=0;

  *width = checkImageWidth;
  *height = checkImageHeight;
  result = (GLubyte *)malloc(4 * *width * *height);

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



#ifdef TIFF
#include <tiffio>
/* Load a TIFF texture: requires libtiff */
uint32 *LoadTIFF(char *filename, int *width, int *height, int *format)
{
  TIFF *tif;
  char emsg[1024];
  uint32 *raster;
  TIFFRGBAImage img;
  tsize_t npixels;

  tif = TIFFOpen(filename, "r");
  if (tif == NULL) {
    fprintf(stderr, "Problem showing %s\n", filename);
    return Generate_Image(&width, &height, &format);
  }
  if (TIFFRGBAImageBegin(&img, tif, 0, emsg)) {
    npixels = (tsize_t) (img.width * img.height);
    raster = (uint32 *) _TIFFmalloc(npixels * (tsize_t) sizeof(uint32));
    if (raster != NULL) {
      if (TIFFRGBAImageGet(&img, raster, img.width, img.height) == 0) {
        TIFFError(filename, emsg);
	return Generate_Image(&width, &height, &format);
      }
    }
    TIFFRGBAImageEnd(&img);
  } else {
    TIFFError(filename, emsg);
    return Generate_Image(&width, &height, &format);
  }

  *width = img.width;
  *height = img.height;
  *format = GL_RGBA;

  TIFFClose(tif);
  return raster;
}
#endif

/* Load a modified version of PPM format with an extra byte for alpha */
GLubyte *LoadPPM4(const char *filename, int *width, int *height, int *format)
{
  char buff[1024];
  GLubyte *data;
  int sizeX, sizeY;
  FILE *fp;
  int maxval;

  fp = fopen(filename, "rb");
  if (!fp)
    {
      fprintf(stderr, "Unable to open file '%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  if (!fgets(buff, sizeof(buff), fp))
    {
      perror("Unable to read header filename\n");
      return  Generate_Image(width, height, format);
    }

  if (buff[0] != '6' || buff[1] != 'P')
    {
      fprintf(stderr, "Invalid image format (must be `6P')\n");
      return  Generate_Image(width, height, format);
    }

  do
    {
      fgets(buff, sizeof(buff), fp);
    }
  while (buff[0] == '#');
    
  if (sscanf(buff, "%d %d", &sizeX, &sizeY) != 2)
    {
      fprintf(stderr, "Error loading image `%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  if (fscanf(fp, "%d", &maxval) != 1)
    {
      fprintf(stderr, "Error loading image `%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  while (fgetc(fp) != '\n')
    ;

  data = (GLubyte *)malloc(4 * sizeX * sizeY);
  if (data == NULL)
    {
      fprintf(stderr, "Unable to allocate memory\n");
	  exit(1);
    }

  if (fread(data, 4 * sizeX, sizeY, fp) != sizeY)
    {
      fprintf(stderr, "Error loading image `%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  fclose(fp);

  *width = sizeX;
  *height = sizeY;
  *format = GL_RGBA;
  return data;
}

/* Load a plain PPM image */
GLubyte *LoadPPM(const char *filename, int *width, int *height, int *format)
{
  char buff[1024];
  GLubyte *data;
  GLint sizeX, sizeY;
  FILE *fp;
  int maxval;

  fp = fopen(filename, "rb");
  if (!fp)
    {
      fprintf(stderr, "Unable to open file '%s'\n", filename);
      return  Generate_Image(width, height, format);
      exit(1);
    }
  if (!fgets(buff, sizeof(buff), fp))
    {
      perror(filename);
      return  Generate_Image(width, height, format);
    }

  if (buff[0] != 'P' || buff[1] != '6')
    {
      fprintf(stderr, "Invalid image format (must be `P6')\n");
      return  Generate_Image(width, height, format);
    }

  do
    {
      fgets(buff, sizeof(buff), fp);
    }
  while (buff[0] == '#');
    
  if (sscanf(buff, "%d %d", &sizeX, &sizeY) != 2)
    {
      fprintf(stderr, "Error loading image `%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  if (fscanf(fp, "%d", &maxval) != 1)
    {
      fprintf(stderr, "Error loading image `%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  while (fgetc(fp) != '\n')
    ;

  data = (GLubyte *)malloc(3 * sizeX * sizeY);
  if (data == NULL)
    {
      fprintf(stderr, "Unable to allocate memory\n");
	  exit(1);
    }

  if (fread(data, 3 * sizeX, sizeY, fp) != sizeY)
    {
      fprintf(stderr, "Error loading image `%s'\n", filename);
      return  Generate_Image(width, height, format);
    }

  fclose(fp);

  *width = sizeX;
  *height = sizeY;
  *format = GL_RGB;
  return data;
}

/* Create a texture in OpenGL.  First an image is loaded 
   and stored in a raster buffer, then it's  */
void Create_Texture(char *filename)
{
  int height, width;
  GLubyte *image;
  GLint a;
  int format;

  if ( !strncmp(filename, "BUILTIN", 7))
    image = Generate_Image(&width, &height, &format);
  else if ( !strncmp((filename+strlen(filename)-3), "ppm", 3))
    image = LoadPPM(filename, &width, &height, &format);
  else if ( !strncmp((filename+strlen(filename)-4), "ppm4", 4))
    image = LoadPPM4(filename, &width, &height, &format);
#ifdef TIFF
  else if ( !strncmp((filename+strlen(filename)-4), "tiff", 4))
    image = (GLubyte *)LoadTIFF(filename, &width, &height, &format);
#endif
  else {
    fprintf(stderr, "Unknown file format extension: '%s'\n", filename);
    image = Generate_Image(&width, &height, &format);
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
	a=gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, format, GL_UNSIGNED_BYTE, image);
  else
    {
      clear_gl_error();
      glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                   format, GL_UNSIGNED_BYTE, image);
      check_gl_error("texture");
    }

  free(image);
}

void resetProjection(void) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1, 1, -1, 1, 1, 100); 
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}


void GenerateQuad(void)
{
  int i;

  quad_list = glGenLists(1);
  glNewList(quad_list,GL_COMPILE);
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

  quads = (struct quad *) malloc(sizeof(struct quad) * num_quads);
  for (i=0; i < num_quads; i++)
    {
      quads[i].rx = 0.;
      quads[i].ry = 0.;
      quads[i].rz = 0.;
      quads[i].tx = 0.;
      quads[i].ty = 0.;
      quads[i].tz = -10;

      quads[i].drx = frand(5.0);
      quads[i].dry = frand(5.0);
      quads[i].drz = 0;
    }
}

void initializeGL(GLsizei width, GLsizei height) 
{
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
	  Create_Texture(which_image); 

  GenerateQuad();
}
void drawQuads(void) {
  int i;
  for (i=0; i < num_quads; i++)
    {
      glPushMatrix();
      glTranslatef(quads[i].tx,0,0);
      glTranslatef(0,quads[i].ty,0);
      glTranslatef(0,0,quads[i].tz);
      glRotatef(quads[i].rx, 1,0,0);
      glRotatef(quads[i].ry, 0,1,0);
      glRotatef(quads[i].rz, 0,0,1);
      glCallList(quad_list);
      glPopMatrix();

      quads[i].rx += quads[i].drx;
      quads[i].ry += quads[i].dry;
      quads[i].rz += quads[i].drz;

    }
}

GLvoid drawScene(ModeInfo * mi) 
{
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
  glScalef(scale_x, scale_y, scale_z);
  drawQuads();

  /* update the scaling factors- cyclic */
  scale_x = cos(frame/360.)*10.;
  scale_y = sin(frame/360.)*10.;
  scale_z = 1;

  /* increment frame-counter */
  frame++;

/*  check_gl_error ("drawScene"); */
}


void draw_screensaver(ModeInfo * mi)
{
  screensaverstruct *gp = &Screensaver[MI_SCREEN(mi)];
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
void
reshape_screensaver(ModeInfo *mi, int width, int height)
{
  glViewport( 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi) );
  resetProjection();
}

void
init_screensaver(ModeInfo * mi)
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
	initializeGL(MI_WIDTH(mi), MI_HEIGHT(mi));
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
  free(quads);
  FreeAllGL(mi);
}
#endif
