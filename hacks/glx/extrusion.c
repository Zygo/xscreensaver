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

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef STANDALONE
# define PROGCLASS						"Screensaver"
# define HACK_INIT						init_screensaver
# define HACK_DRAW						draw_screensaver
# define HACK_RESHAPE					reshape_screensaver
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
#include <malloc.h>
#include <GL/gl.h>
#include <GL/glu.h>
#ifdef HAVE_GLE3
#include <GL/gle.h>
#else
#include <GL/tube.h>
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


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
  {"-light",   ".extrusion.light",     XrmoptionNoArg, (caddr_t) "true" },
  {"+light",   ".extrusion.light",     XrmoptionNoArg, (caddr_t) "false" },
  {"-wire",    ".extrusion.wire",      XrmoptionNoArg, (caddr_t) "true" },
  {"+wire",    ".extrusion.wire",      XrmoptionNoArg, (caddr_t) "false" },
  {"-texture", ".extrusion.texture",   XrmoptionNoArg, (caddr_t) "true" },
  {"+texture", ".extrusion.texture",   XrmoptionNoArg, (caddr_t) "false" },
  {"-texture", ".extrusion.texture",   XrmoptionNoArg, (caddr_t) "true" },
  {"+texture_quality", ".extrusion.texture",   XrmoptionNoArg, (caddr_t) "false" },
  {"-texture_quality", ".extrusion.texture",   XrmoptionNoArg, (caddr_t) "true" },
  {"+mipmap", ".extrusion.mipmap",   XrmoptionNoArg, (caddr_t) "false" },
  {"-mipmap", ".extrusion.mipmap",   XrmoptionNoArg, (caddr_t) "true" },
  {"-name",   ".extrusion.name",  XrmoptionSepArg, (caddr_t) NULL },
  {"-image",   ".extrusion.image",  XrmoptionSepArg, (caddr_t) NULL },
};


static argtype vars[] = {
  {(caddr_t *) &do_light,    "light",   "Light",   DEF_LIGHT,   t_Bool},
  {(caddr_t *) &do_wire,    "wire",   "Wire",   DEF_WIRE,   t_Bool},
  {(caddr_t *) &do_texture,    "texture",   "Texture",   DEF_TEXTURE,   t_Bool},
  {(caddr_t *) &do_texture_quality,    "texture_quality",   "Texture_Quality",   DEF_TEXTURE_QUALITY,   t_Bool},
  {(caddr_t *) &do_mipmap,    "mipmap",   "Mipmap",   DEF_MIPMAP,   t_Bool},
  {(caddr_t *) &which_name, "name",   "Name",   DEF_NAME,   t_String},
  {(caddr_t *) &which_image, "image",   "Image",   DEF_IMAGE,   t_String},
};


static OptionStruct desc[] =
{
  {"-name num", "example 'name' to draw (helix2, helix3, helix4, joinoffset, screw, taper, twistoid)"},
  {"-/+ light", "whether to do enable lighting (slower)"},
  {"-/+ wire", "whether to do use wireframe instead of filled (faster)"},
  {"-/+ texture", "whether to apply a texture (slower)"},
  {"-image <filename>", "texture image to load (PPM or PPM4)"},
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
static float dx=0, dy=0, dz=0;
static float ddx=0, ddy=0, ddz=0;
static float d_max = 0;
static int screensaver_number;

static float max_lastx=300, max_lasty=400;
static float min_lastx=-400, min_lasty=-400;
static float d_lastx=0, d_lasty=0;
static float dd_lastx=0, dd_lasty=0;
static float max_dlastx=0, max_dlasty=0;
float lastx=0, lasty=0;

static int errCode;
static GLubyte * errString;

struct functions {
  void (*InitStuff)(void);
  void (*DrawStuff)(void);
  char *name;
};

/* currently joinoffset and twistoid look funny-
   like we're looking at them from the back or something
*/

struct functions funcs_ptr[] = {
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


/* check for errors, bail if any.  useful for debugging */
int checkError(int line, char *file)
{
  if((errCode = glGetError()) != GL_NO_ERROR) {
    errString = (GLubyte *)gluErrorString(errCode);
    fprintf(stderr, "%s: OpenGL error: %s detected at line %d in file %s\n",
            progname, errString, line, file);
    exit(1);
  }
  return 0;
}


/* generate a checkered image for texturing */
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
      fprintf(stderr, "%s: unable to open file '%s'\n", progname, filename);
      return  Generate_Image(width, height, format);
    }

  if (!fgets(buff, sizeof(buff), fp))
    {
      perror("Unable to read header filename\n");
      return  Generate_Image(width, height, format);
    }

  if (buff[0] != '6' || buff[1] != 'P')
    {
      fprintf(stderr, "%s: Invalid image format (must be `6P')\n", progname);
      return  Generate_Image(width, height, format);
    }

  do
    {
      fgets(buff, sizeof(buff), fp);
    }
  while (buff[0] == '#');
    
  if (sscanf(buff, "%d %d", &sizeX, &sizeY) != 2)
    {
      fprintf(stderr, "%s: error loading image `%s'\n", progname, filename);
      return  Generate_Image(width, height, format);
    }

  if (fscanf(fp, "%d", &maxval) != 1)
    {
      fprintf(stderr, "%s: error loading image `%s'\n", progname, filename);
      return  Generate_Image(width, height, format);
    }

  while (fgetc(fp) != '\n')
    ;

  data = (GLubyte *)malloc(4 * sizeX * sizeY);
  if (data == NULL)
    {
      fprintf(stderr, "%s: unable to allocate memory\n", progname);
	  exit(1);
    }

  if (fread(data, 4 * sizeX, sizeY, fp) != sizeY)
    {
      fprintf(stderr, "%s: error loading image `%s'\n", progname, filename);
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
      fprintf(stderr, "%s: unable to open file '%s'\n", progname, filename);
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
      fprintf(stderr, "%s: invalid image format (must be `P6')\n", progname);
      return  Generate_Image(width, height, format);
    }

  do
    {
      fgets(buff, sizeof(buff), fp);
    }
  while (buff[0] == '#');
    
  if (sscanf(buff, "%d %d", &sizeX, &sizeY) != 2)
    {
      fprintf(stderr, "%s: error loading image `%s'\n", progname, filename);
      return  Generate_Image(width, height, format);
    }

  if (fscanf(fp, "%d", &maxval) != 1)
    {
      fprintf(stderr, "%s: error loading image `%s'\n", progname, filename);
      return  Generate_Image(width, height, format);
    }

  while (fgetc(fp) != '\n')
    ;

  data = (GLubyte *)malloc(3 * sizeX * sizeY);
  if (data == NULL)
    {
      fprintf(stderr, "%s: unable to allocate memory\n", progname);
	  exit(1);
    }

  if (fread(data, 3 * sizeX, sizeY, fp) != sizeY)
    {
      fprintf(stderr, "%s: error loading image `%s'\n", progname, filename);
      return  Generate_Image(width, height, format);
    }

  fclose(fp);

  *width = sizeX;
  *height = sizeY;
  *format = GL_RGB;
  return data;
}

/* create a texture to be applied to the surface
   this function loads a file using a loader depending on
   that extension of the file. there is very little error
   checking.  
*/

void Create_Texture(char *filename, int do_mipmap, int do_texture_quality)
{
  int height, width;
  GLubyte *image;
  int format;

  if ( !strncmp(filename, "BUILTIN", 7))
    image = Generate_Image(&width, &height, &format);
  else if ( !strncmp((filename+strlen(filename)-3), "ppm", 3))
    image = LoadPPM(filename, &width, &height, &format);
  else if ( !strncmp((filename+strlen(filename)-4), "ppm4", 4))
    image = LoadPPM4(filename, &width, &height, &format);
  else {
    fprintf(stderr, "%s: unknown file format extension: '%s'\n",
            progname, filename);
	exit(1);
  }

  /* GL_MODULATE or GL_DECAL depending on what you want */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  /* default is to do it quick and dirty */
  /* if you have mipmaps turned on, but not texture quality, nothing will happen! */
  if (do_texture_quality) {
    /* with texture_quality, the min and mag filters look *much* nice but are *much* slower */
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }

  if (do_mipmap) {
    gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, 
		      format, GL_UNSIGNED_BYTE, image);
  }
  else {
    clear_gl_error();
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
		 format, GL_UNSIGNED_BYTE, image);
    check_gl_error("texture");
  }
  free(image);
}


/* mostly lifted from lament.c */
static void
rotate (float *pos, float *v, float *dv, float max_v)
{
  double ppos = *pos;

  /* tick position */
  if (ppos < 0)
    ppos = -(ppos + *v);
  else
    ppos += *v;

  if (ppos > 360)
    ppos -= 360;
  else if (ppos < 0)
    ppos += 360;

  if (ppos < 0) abort();
  if (ppos > 360) abort();
  *pos = (*pos > 0 ? ppos : -ppos);

  /* accelerate */
  *v += *dv;

  /* clamp velocity */
  if (*v > max_v || *v < -max_v)
    {
      *dv = -*dv;
    }
  /* If it stops, start it going in the other direction. */
  else if (*v < 0)
    {
      if (random() % 4)
        {
          *v = 0;

          /* keep going in the same direction */
          if (random() % 2)
            *dv = 0;
          else if (*dv < 0)
            *dv = -*dv;
        }
      else
        {
          /* reverse gears */
          *v = -*v;
          *dv = -*dv;
          *pos = -*pos;
        }
    }

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    *dv = -*dv;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (*dv == 0)
        *dv = 0.00001;
      else if (random() & 1)
        *dv *= 1.2;
      else
        *dv *= 0.8;
    }
}


static void
bounce (float *pos, float *v, float *dv, float max_v)
{
  *pos += *v;

  if (*pos > 1.0)
    *pos = 1.0, *v = -*v, *dv = -*dv;
  else if (*pos < 0)
    *pos = 0, *v = -*v, *dv = -*dv;

  if (*pos < 0.0) abort();
  if (*pos > 1.0) abort();

  /* accelerate */
  *v += *dv;

  /* clamp velocity */
  if (*v > max_v || *v < -max_v)
    {
      *dv = -*dv;
    }

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    *dv = -*dv;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (*dv == 0)
        *dv = 0.00001;
      else if (random() & 1)
        *dv *= 1.2;
      else
        *dv *= 0.8;
    }
}


static void
init_rotation (void)
{
  rot_x = (float) (random() % (360 * 2)) - 360;  /* -360 - 360 */
  rot_y = (float) (random() % (360 * 2)) - 360;
  rot_z = (float) (random() % (360 * 2)) - 360;

  /* bell curve from 0-1.5 degrees, avg 0.75 */
  dx = (frand(1) + frand(1) + frand(1)) / 2.0;
  dy = (frand(1) + frand(1) + frand(1)) / 2.0;
  dz = (frand(1) + frand(1) + frand(1)) / 2.0;

  d_max = dx * 2;

  ddx = 0.004;
  ddy = 0.004;
  ddz = 0.004;

  lastx = (random() % (int) (max_lastx - min_lastx)) + min_lastx;
  lasty = (random() % (int) (max_lasty - min_lasty)) + min_lasty;
  d_lastx = (frand(1) + frand(1) + frand(1));
  d_lasty = (frand(1) + frand(1) + frand(1));
  max_dlastx = d_lastx * 2;
  max_dlasty = d_lasty * 2;
  dd_lastx = 0.004;
  dd_lasty = 0.004;
}


/* draw the screensaver once */
void draw_screensaver(ModeInfo * mi)
{
  screensaverstruct *gp = &Screensaver[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);

  Window root, child;
  int rootx, rooty, winx, winy;
  unsigned int mask;
  XEvent event;

  if (!gp->glx_context)
	return;

  glXMakeCurrent(display, window, *(gp->glx_context));

  funcs_ptr[screensaver_number].DrawStuff();
	  
  rotate(&rot_x, &dx, &ddx, d_max);
  rotate(&rot_y, &dy, &ddy, d_max);
  rotate(&rot_z, &dz, &ddz, d_max);

  /* swallow any ButtonPress events */
  while (XCheckMaskEvent (MI_DISPLAY(mi), ButtonPressMask, &event))
    ;
  /* check the pointer position and button state. */
  XQueryPointer (MI_DISPLAY(mi), MI_WINDOW(mi),
                 &root, &child, &rootx, &rooty, &winx, &winy, &mask);

  /* track the mouse only if a button is down. */
  if (mask & (Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask))
    {
      lastx = winx;
      lasty = winy;
    }
  else
    {
      float scale = (max_lastx - min_lastx);
      lastx -= min_lastx;
      lasty -= min_lasty;
      lastx /= scale;
      lasty /= scale;
      d_lastx /= scale;
      d_lasty /= scale;
      dd_lastx /= scale;
      dd_lasty /= scale;
      bounce(&lastx, &d_lastx, &dd_lastx, max_dlastx);
      bounce(&lasty, &d_lasty, &dd_lasty, max_dlasty);
      lastx *= scale;
      lasty *= scale;
      lastx += min_lastx;
      lasty += min_lasty;
      d_lastx *= scale;
      d_lasty *= scale;
      dd_lastx *= scale;
      dd_lasty *= scale;
    }

  if (mi->fps_p) do_fps (mi);
  glXSwapBuffers(display, window);
}


/* set up lighting conditions */
void SetupLight(void)
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
void resetProjection(void) {
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
void chooseScreensaverExample(void) {
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
  init_rotation();
  funcs_ptr[screensaver_number].InitStuff();
}

/* main OpenGL initialization routine */
void
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
  glShadeModel(GL_SMOOTH);

  if (do_light)
	SetupLight();
  if (do_wire) {
	glPolygonMode(GL_FRONT,GL_LINE);
  	glPolygonMode(GL_BACK,GL_LINE);
  }
  if (do_texture) {
	Create_Texture(which_image, do_mipmap, do_texture_quality);
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
	chooseScreensaverExample();
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

