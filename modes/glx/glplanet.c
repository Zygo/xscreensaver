/* -*- Mode: C; tab-width: 4 -*- */
/* glplanet --- 3D rotating planet, e.g., Earth. */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)glplanet.c	5.01 01/04/13 xlockmore";

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
 *
 * 13-Apr-01: rolf@groppe.de Did this on the xlockmore version
 * (21-Mar-01: jwz@jwz.org   Broke sphere routine out into its own file.
 *                           Done on the xscreensaver version)
 *
 * 5-Apr-01:  rolf@groppe.de    Ported this mode from xscreensaver to xlock.
 *                              Made smoother roll & rotational movement
 *                                   configurable by -count and -cycles.
 *                              Renamed the parameter -image to -pimage
 *
 * 9-Oct-98:  dek@cgl.ucsf.edu  Added stars.
 *
 * 8-Oct-98:  jwz@jwz.org   Made the 512x512x1 xearth image be built in.
 *                          Made it possible to load XPM or XBM files.
 *                          Made the planet bounce and roll around.
 *
 * 8-Oct-98: Released initial version of "glplanet"
 * (David Konerding, dek@cgl.ucsf.edu)
 *
 * BUGS:
 * -bounce is broken
 * 
 *   For even more spectacular results, grab the images from the "SSysten"
 *   package (http://www.msu.edu/user/kamelkev/) and do this:
 *
 *     cd ssystem-1.4/hires/
 *     foreach f ( *.jpg )
 *       djpeg $f | ppmquant 254 | ppmtoxpm > /tmp/$f:r.xpm
 *     end
 *
 *     cd /tmp
 *     foreach f ( *.xpm )
 *       xlock -mode glplanet -pimage $f
 *     end
 */


/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#ifdef VMS
#include "vms_x_fix.h"
#endif
#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS						"Planet"
# define HACK_INIT						init_planet
# define HACK_DRAW						draw_glplanet
# define HACK_RESHAPE					reshape_planet
# define planet_opts					xlockmore_opts
#define DEFAULTS	"*delay:			15000   \n"	\
					"*showFPS:			False   \n" \
                    "*rotate:           True    \n" \
                    "*roll:             True    \n" \
                    "*bounce:           True    \n" \
					"*wireframe:		False	\n"	\
					"*light:			True	\n"	\
					"*texture:			True	\n" \
					"*stars:			True	\n" \
					"*pimage:			BUILTIN	\n" \
					"*imageForeground:	Green	\n" \
					"*imageBackground:	Blue	\n"

# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
# include "visgl.h"
#endif /* !STANDALONE */

#ifdef MODE_glplanet /* whole file */

#if defined( USE_XPM ) || defined( USE_XPMINC )
#if USE_XPMINC
#include <xpm.h>
#else
#include <X11/xpm.h>            /* Normal spot */
#endif /* USE_XPMINC */
# ifndef PIXEL_ALREADY_TYPEDEFED
#  define PIXEL_ALREADY_TYPEDEFED /* Sigh, Xmu/Drawing.h needs this... */
# endif
#endif

#ifdef USE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#include "sphere.h" 

#include <GL/glu.h>

#define DEF_ROTATE  "True"
#define DEF_ROLL    "True"
#define DEF_BOUNCE  "True"
#define DEF_TEXTURE "True"
#define DEF_STARS "True"
#define DEF_LIGHT   "True"
#define DEF_IMAGE   "BUILTIN"
#define MINROLL 1
#define MINROT 2

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int do_rotate;
static int do_roll;
static int do_bounce;
static int do_texture;
static int do_stars;
static int do_light;
static char *which_image;
static XrmOptionDescRec opts[] = {
  {(char *) "-rotate", (char *) ".glplanet.rotate",  XrmoptionNoArg, (caddr_t) "on" },
  {(char *) "+rotate", (char *) ".glplanet.rotate",  XrmoptionNoArg, (caddr_t) "off" },
  {(char *) "-roll", (char *) ".glplanet.roll",    XrmoptionNoArg, (caddr_t) "on" },
  {(char *) "+roll", (char *) ".glplanet.roll",    XrmoptionNoArg, (caddr_t) "off" },
  {(char *) "-bounce", (char *) ".glplanet.bounce",  XrmoptionNoArg, (caddr_t) "on" },
  {(char *) "+bounce", (char *) ".glplanet.bounce",  XrmoptionNoArg, (caddr_t) "off" },
  {(char *) "-texture", (char *) ".glplanet.texture", XrmoptionNoArg, (caddr_t) "on" },
  {(char *) "+texture", (char *) ".glplanet.texture", XrmoptionNoArg, (caddr_t) "off" },
  {(char *) "-stars", (char *) ".glplanet.stars",   XrmoptionNoArg, (caddr_t) "on" },
  {(char *) "+stars", (char *) ".glplanet.stars",   XrmoptionNoArg, (caddr_t) "off" },
  {(char *) "-light", (char *) ".glplanet.light",   XrmoptionNoArg, (caddr_t) "on" },
  {(char *) "+light", (char *) ".glplanet.light",   XrmoptionNoArg, (caddr_t) "off" },
  {(char *) "-pimage", (char *) ".glplanet.pimage",  XrmoptionSepArg, (caddr_t) 0 }
};

static argtype vars[] = {
  {(caddr_t *) &do_rotate,   (char *) "rotate",  (char *) "Rotate",  DEF_ROTATE,  t_Bool},
  {(caddr_t *) &do_roll,     (char *) "roll",    (char *) "Roll",    DEF_ROLL,    t_Bool},
  {(caddr_t *) &do_bounce,   (char *) "bounce",  (char *) "Bounce",  DEF_BOUNCE,  t_Bool},
  {(caddr_t *) &do_texture,  (char *) "texture", (char *) "Texture", DEF_TEXTURE, t_Bool},
  {(caddr_t *) &do_stars,  (char *) "stars", (char *) "Stars", DEF_STARS, t_Bool},
  {(caddr_t *) &do_light,    (char *) "light",   (char *) "Light",   DEF_LIGHT,   t_Bool},
  {(caddr_t *) &which_image, (char *) "pimage",   (char *) "Pimage",   DEF_IMAGE,   t_String},
};

static OptionStruct desc[] = {
  {(char *) "-/+rotate", (char *) "turn on/off rotatation"},
  {(char *) "-/+roll", (char *) "turn on/off rolling"},
  {(char *) "-/+bounce", (char *) "turn on/off bouncing"},
  {(char *) "-/+texture", (char *) "turn on/off texture mapping"},
  {(char *) "-/+stars", (char *) "turn on/off stars"},
  {(char *) "-/+light", (char *) "turn on/off light source"},
  {(char *) "-pimage", (char *) "set image"},
};

ModeSpecOpt glplanet_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   glplanet_description =
{"glplanet", "init_glplanet", "draw_glplanet", "release_glplanet",
 "draw_glplanet", "init_glplanet", NULL, &glplanet_opts,
 1000, 1, 2, 1, 64, 1.0, "",
 "Animates texture mapped sphere (planet)", 0, NULL};
#endif

#define FLOATRAND(a) (((double)LRAND() / (double)MAXRAND) * a)

#include "bitmaps/earth.xbm"
#include "xpm-ximage.h"


/*-
 * slices and stacks are used in the sphere parameterization routine.
 * more slices and stacks will increase the quality of the sphere,
 * at the expense of rendering speed
 */

#define NUM_STARS 1000
#define SLICES 32
#define STACKS 32

/* radius of the sphere- fairly arbitrary */
#define RADIUS 4

/* distance away from the sphere model */
#define DIST 40



/* structure for holding the planet data */
typedef struct {
  GLuint platelist;
  GLuint starlist;
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;

  XColor fg, bg;

  GLfloat tx, ty, tz;
  GLfloat dtx, dty, dtz;
  GLfloat xpos, ypos, zpos;
  GLfloat dx, dy, dz;
  GLfloat box_width, box_height, box_depth;
  int roll_div, rot_div;
  
} planetstruct;


static planetstruct *planets = (planetstruct *) NULL;

#if 0
static inline void
normalize(GLfloat v[3])
{
	GLfloat     d = (GLfloat) sqrt((double) (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));

	if (d != 0) {
		v[0] /= d;
		v[1] /= d;
		v[2] /= d;
	} else {
		v[0] = v[1] = v[2] = 0;
	}
}
#endif

/* Set up and enable texturing on our object */
static void
setup_xbm_texture (unsigned char *bits, int width, int height,
				   XColor *fgc, XColor *bgc)
{
  unsigned int fg = (((fgc->red  >> 8) << 16) |
					 ((fgc->green >> 8) << 8) |
					 ((fgc->blue >> 8)));
  unsigned int bg = (((bgc->red  >> 8) << 16) |
					 ((bgc->green >> 8) << 8) |
					 ((bgc->blue >> 8)));

  unsigned char *data = (unsigned char *)
	malloc ((width * height * 24) / 8);
  unsigned char *out = data;
  int x, y;

  for (y = 0; y < height; y++)
	for (x = 0; x < width; x++)
	  {
		unsigned char byte = bits [(y * (width / 8) + (x / 8))];
		unsigned char bit = (byte & (1 << (x % 8))) >> (x % 8);
		unsigned int word = (bit ? bg : fg); 
		*out++ = (word & 0xFF0000) >> 16;
		*out++ = (word & 0x00FF00) >> 8;
		*out++ = (word & 0x0000FF);
	  }

  clear_gl_error();
  glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0,
			   GL_RGB, GL_UNSIGNED_BYTE, data);
  check_gl_error("texture");

  /* setup parameters for texturing */
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (data != NULL) {
	(void) free((void *) data);
  }
}


static void
setup_file_texture (ModeInfo *mi, char *filename)
{
  Display *dpy = MI_DISPLAY(mi);
  Visual *visual = MI_VISUAL(mi);
  Colormap cmap = MI_COLORMAP(mi);

#if defined( USE_XPM ) || defined( USE_XPMINC )
  {
	char **xpm_data = 0;
	int result = XpmReadFileToData (filename, &xpm_data); 
	switch (result) {
	case XpmSuccess:
	  {
		XImage *image = xpm_to_ximage (dpy, visual, cmap, xpm_data);

        clear_gl_error();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
					 image->width, image->height, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, image->data);
        check_gl_error("texture");

		/* setup parameters for texturing */
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		return;
	  }
	  break;

	case XpmOpenFailed:
	  fprintf (stderr, "Glplanet: file %s doesn't exist.\n", filename);
	  exit (-1);
	  break;

	case XpmFileInvalid:
	  /* Fall through and try it as an XBM. */
	  break;

	case XpmNoMemory:
	  fprintf (stderr, "Glplanet: XPM: out of memory\n");
	  exit (-1);
	  break;

	default:
	  fprintf (stderr, "Glplanet: XPM: unknown error code %d\n", result);
	  exit (-1);
	  break;
	}
  }
#endif /* HAVE_XPM */

#ifdef USE_XMU
  {
	planetstruct *gp = &planets[MI_SCREEN(mi)];
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned char *data = 0;
	int xhot, yhot;
	int status = XmuReadBitmapDataFromFile (filename, &width, &height, &data,
											&xhot, &yhot);
	if (status != Success)
	  {
#if defined( USE_XPM ) || defined( USE_XPMINC )
		fprintf (stderr, "Glplanet: not an XPM file: %s\n", filename);
# endif
		fprintf (stderr, "Glplanet: not an XBM file: %s\n", filename);
		exit (1);
	  }

	setup_xbm_texture ((unsigned char *) data, width, height, &gp->fg, &gp->bg);
  }
#else  /* !XMU */

#if defined( USE_XPM ) || defined( USE_XPMINC )
  fprintf (stderr, "Glplanet: not an XPM file: %s\n", filename);
# endif
  fprintf (stderr, "Glplanet: your vendor doesn't ship the standard Xmu library.\n");
  fprintf (stderr, "Glplanet: we can't load XBM files without it.\n");
  exit (1);
#endif /* !XMU */
}


static void
setup_texture(ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  if (!which_image ||
	  !*which_image ||
	  !strcmp(which_image, "BUILTIN"))
	setup_xbm_texture (earth_bits, earth_width, earth_height,
					   &gp->fg, &gp->bg);

  else
	setup_file_texture (mi, which_image);
}


/* Set up and enable lighting */
static void
setup_light(void)
{
  /* set a number of parameters which make the scene look much nicer */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glShadeModel(GL_SMOOTH);
}


/* Set up and enable face culling so we don't see the inside of the sphere */
static void
setup_face(void)
{
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK); 
}

/* lame way to generate some random stars */
void generate_stars(ModeInfo * mi, int width, int height)
{
  int i;
/*  GLfloat size_range[2], size;*/
  GLfloat x, y;

  planetstruct *gp = &planets[MI_SCREEN(mi)];
  
/*    glGetFloatv(GL_POINT_SIZE_RANGE, size_range); */
  
/*    printf("size range: %f\t%f\n", size_range[0], size_range[1]); */
  gp->starlist = glGenLists(1);
  glNewList(gp->starlist, GL_COMPILE);

  /* this hackery makes the viewport map one-to-one with Vertex arguments */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, width, 0, height);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /* disable depth testing for the stars, so they don't obscure the planet */
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  glBegin(GL_POINTS);
  for(i = 0 ; i < NUM_STARS ; i++)
	{
/*   	  size = ((random()%size_range[0])) * size_range[1]/2.; */
/*    glPointSize(size); */
	  x = random() % width;
	  y = random() % height;
	  glVertex2f(x,y);
	}
  glEnd();

  /* return to original PROJECT and MODELVIEW */
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);


  glEndList();

}

/* Initialization function for screen saver */
static void
pinit(ModeInfo * mi)
{
  Bool wire = MI_IS_WIREFRAME(mi);
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (wire) {
	glEnable(GL_LINE_SMOOTH);
	do_texture = False;
  }

  /* turn on various options we like */
  if (do_texture)
	setup_texture(mi);
  if (do_light)
	setup_light();

  setup_face();

  if (do_stars) {
	glEnable(GL_POINT_SMOOTH);
	generate_stars(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  gp->platelist=glGenLists(1);
  glNewList(gp->platelist, GL_COMPILE);
  glPushMatrix ();
  glScalef (RADIUS, RADIUS, RADIUS);
  unit_sphere (STACKS, SLICES, wire);
  glPopMatrix ();
  glEndList();
 }

static void
draw_sphere_glp(ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  glEnable(GL_DEPTH_TEST);

  /* turn on the various attributes for making the sphere look nice */
  if (do_texture)
	glEnable(GL_TEXTURE_2D);

  if (do_light)
	{ 
	  glEnable(GL_LIGHTING);
	  glEnable(GL_LIGHT0);
	  glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	  glEnable(GL_COLOR_MATERIAL);
	}

  glCallList(gp->platelist);

}


#define RANDSIGN() ((random() & 1) ? 1 : -1)

static void
pick_velocity (ModeInfo * mi)
{
  float roll1,roll2,rot1,rot2;
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  gp->roll_div = MI_COUNT(mi);
  if (gp->roll_div <= MINROLL) gp->roll_div = MINROLL * 10;
  gp->rot_div = MI_CYCLES(mi);
  if (gp->rot_div <= MINROT) gp->rot_div = MINROT * 50;
  
  roll1 = gp->roll_div / 100.0;
  roll2 = roll1 * 0.75;
  rot1  = gp->rot_div  / 100.0;
  rot2  = rot1;
  gp->box_width =  15.0;
  gp->box_height = 15.0;
  gp->box_depth =  5.0;

  gp->tx = 0.0;
  gp->ty = 0.0;
  gp->tz = FLOATRAND(360);

/* following values changed for smoother rotation */
  gp->dtx = (FLOATRAND(roll1) + FLOATRAND(roll2)) * RANDSIGN();
  gp->dty = (FLOATRAND(roll1) + FLOATRAND(roll2)) * RANDSIGN();
  gp->dtz = (FLOATRAND(rot1) + FLOATRAND(rot2));  /* the sun sets in the west */

/* following values are unused
  gp->dx = (FLOATRAND(0.2) + FLOATRAND(0.2)) * RANDSIGN();
  gp->dy = (FLOATRAND(0.2) + FLOATRAND(0.2)) * RANDSIGN();
  gp->dz = (FLOATRAND(0.2) + FLOATRAND(0.2)) * RANDSIGN(); */ 
}


static void
rotate_and_move (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (do_roll)
	{
	  gp->tx += gp->dtx;
	  while (gp->tx < 0)   gp->tx += 360;
	  while (gp->tx > 360) gp->tx -= 360;

	  gp->ty += gp->dty;
	  while (gp->ty < 0)   gp->ty += 360;
	  while (gp->ty > 360) gp->ty -= 360;
	}

  if (do_rotate)
	{
	  gp->tz += gp->dtz;
	  while (gp->tz < 0)   gp->tz += 360;
	  while (gp->tz > 360) gp->tz -= 360;
	}

  if (do_bounce)
	{
      static int frame = 0;
/*
#     define SINOID(SCALE,SIZE) \
        ((((1 + sin((frame * (SCALE)) / 16 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)
      gp->xpos = SINOID(0.031, gp->box_width);
      gp->ypos = SINOID(0.023, gp->box_height);
      gp->zpos = SINOID(0.017, gp->box_depth);
      frame++;
*/
/* Original function replaced for faster execution */
#     define SINOID(SCALE,SIZE) \
        (sin(frame * (SCALE)) / 2.0) * (SIZE) 
      gp->xpos = SINOID(0.006086836, gp->box_width);
      gp->ypos = SINOID(0.004516039, gp->box_height);
      gp->zpos = SINOID(0.003337942, gp->box_depth);
      frame++;

	}
}


/* Standard reshape function */
void
reshape_planet(ModeInfo *mi, int width, int height)
{
  GLfloat light[4];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  light[0] = -1;
  light[1] = (int) (((random() % 3) & 0xFF) - 1);
  light[2] = (int) (((random() % 3) & 0xFF) - 1);
  light[3] = 0;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -DIST);
  glLightfv(GL_LIGHT0, GL_POSITION, light);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}


void
init_glplanet(ModeInfo * mi)
{
  int         screen = MI_SCREEN(mi);

  planetstruct *gp;

  if (planets == NULL) {
	if ((planets = (planetstruct *) calloc(MI_NUM_SCREENS(mi),
		  sizeof (planetstruct))) == NULL)
	  return;
  }
  gp = &planets[screen];

  pick_velocity (mi);

   {
/*	char *f = get_string_resource("imageForeground", "Foreground");
	char *b = get_string_resource("imageBackground", "Background"); */
        char *f = strdup("Green");
	char *b = strdup("Blue");
	char *s;
	if (!f) f = strdup("white");
	if (!b) b = strdup("black");

	for (s = f + strlen(f)-1; s > f; s--)
	  if (*s == ' ' || *s == '\t')
		*s = 0;
	for (s = b + strlen(b)-1; s > b; s--)
	  if (*s == ' ' || *s == '\t')
		*s = 0;

    if (!XParseColor(MI_DISPLAY(mi), MI_COLORMAP(mi), f, &gp->fg))
      {
		fprintf(stderr, "Glplanet: unparsable color: \"%s\"\n", f);
		exit(1);
      }
    if (!XParseColor(MI_DISPLAY(mi), MI_COLORMAP(mi), b, &gp->bg))
      {
		fprintf(stderr, "Glplanet: unparsable color: \"%s\"\n", f);
		exit(1);
      }

	(void) free((void *) f);
	(void) free((void *) b);
  } 


  gp->window = MI_WINDOW(mi);
  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape_planet(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	pinit(mi);
  } else {
	MI_CLEARWINDOW(mi);
  }
}

void
draw_glplanet(ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);

  if (!gp->glx_context)
	return;

  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glXMakeCurrent(display, window, *(gp->glx_context));


  if (do_stars) {
	/* protect our modelview matrix and attributes */
	glPushMatrix();
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	{
	  glColor3f(1,1,1);
	  /* draw the star field. */
	  glCallList(gp->starlist);

	}
	glPopMatrix();
	glPopAttrib();
  }

  /* protect our modelview matrix and attributes */
  glPushMatrix();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  {
	/* this pair of rotations seem to be necessary to orient the earth correctly */
	glRotatef(90,0,0,1);
	glRotatef(90,0,1,0);

	glTranslatef(gp->xpos, gp->ypos, gp->zpos);
	glRotatef(gp->tx, 1, 0, 0);
	glRotatef(gp->ty, 0, 1, 0);
	glRotatef(gp->tz, 0, 0, 1);
	/* draw the sphere */
	draw_sphere_glp(mi);
  }
  glPopMatrix();
  glPopAttrib();



  if (MI_IS_FPS(mi)) do_fps (mi);
  glFinish();
  glXSwapBuffers(display, window);

  rotate_and_move (mi);
}

void
release_glplanet(ModeInfo * mi)
{
  if (planets != NULL) {
	int         screen;

	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	  planetstruct *gp = &planets[screen];

	  if (gp->glx_context) {
		/* Display lists MUST be freed while their glXContext is current. */
		glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

		if (glIsList(gp->platelist))
		  glDeleteLists(gp->platelist, 1);
		if (glIsList(gp->starlist))
		  glDeleteLists(gp->starlist, 1);
	  }
	}
	(void) free((void *) planets);
	planets = (planetstruct *) NULL;
  }
  FreeAllGL(mi);
}


#endif

