/* -*- Mode: C; tab-width: 4 -*- */
/* glplanet --- 3D rotating planet, e.g., Earth. */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)plate.c	4.07 97/11/24 xlockmore";

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
 *       glplanet -image $f
 *     end
 */


/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS						"Planet"
# define HACK_INIT						init_planet
# define HACK_DRAW						draw_planet
# define planet_opts					xlockmore_opts
#define DEFAULTS	"*delay:			15000   \n"	\
                    "*rotate:           True    \n" \
                    "*roll:             True    \n" \
                    "*bounce:           True    \n" \
					"*wireframe:		False	\n"	\
					"*light:			True	\n"	\
					"*texture:			True	\n" \
					"*stars:			True	\n" \
					"*image:			BUILTIN	\n" \
					"*imageForeground:	Green	\n" \
					"*imageBackground:	Blue	\n"

# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#ifdef HAVE_XPM
# include <X11/xpm.h>
# ifndef PIXEL_ALREADY_TYPEDEFED
#  define PIXEL_ALREADY_TYPEDEFED /* Sigh, Xmu/Drawing.h needs this... */
# endif
#endif

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif


#include <GL/glu.h>

#define DEF_ROTATE  "True"
#define DEF_ROLL    "True"
#define DEF_BOUNCE  "True"
#define DEF_TEXTURE "True"
#define DEF_STARS "True"
#define DEF_LIGHT   "True"
#define DEF_IMAGE   "BUILTIN"

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
  {"-rotate",  ".glplanet.rotate",  XrmoptionNoArg, (caddr_t) "true" },
  {"+rotate",  ".glplanet.rotate",  XrmoptionNoArg, (caddr_t) "false" },
  {"-roll",    ".glplanet.roll",    XrmoptionNoArg, (caddr_t) "true" },
  {"+roll",    ".glplanet.roll",    XrmoptionNoArg, (caddr_t) "false" },
  {"-bounce",  ".glplanet.bounce",  XrmoptionNoArg, (caddr_t) "true" },
  {"+bounce",  ".glplanet.bounce",  XrmoptionNoArg, (caddr_t) "false" },
  {"-texture", ".glplanet.texture", XrmoptionNoArg, (caddr_t) "true" },
  {"+texture", ".glplanet.texture", XrmoptionNoArg, (caddr_t) "false" },
  {"-stars",   ".glplanet.stars",   XrmoptionNoArg, (caddr_t) "true" },
  {"+stars",   ".glplanet.stars",   XrmoptionNoArg, (caddr_t) "false" },
  {"-light",   ".glplanet.light",   XrmoptionNoArg, (caddr_t) "true" },
  {"+light",   ".glplanet.light",   XrmoptionNoArg, (caddr_t) "false" },
  {"-image",   ".glplanet.image",  XrmoptionSepArg, (caddr_t) 0 },
};

static argtype vars[] = {
  {(caddr_t *) &do_rotate,   "rotate",  "Rotate",  DEF_ROTATE,  t_Bool},
  {(caddr_t *) &do_roll,     "roll",    "Roll",    DEF_ROLL,    t_Bool},
  {(caddr_t *) &do_bounce,   "bounce",  "Bounce",  DEF_BOUNCE,  t_Bool},
  {(caddr_t *) &do_texture,  "texture", "Texture", DEF_TEXTURE, t_Bool},
  {(caddr_t *) &do_stars,  "stars", "Stars", DEF_STARS, t_Bool},
  {(caddr_t *) &do_light,    "light",   "Light",   DEF_LIGHT,   t_Bool},
  {(caddr_t *) &which_image, "image",   "Image",   DEF_IMAGE,   t_String},
};

ModeSpecOpt planet_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   planet_description =
{"planet", "init_planet", "draw_planet", "release_planet",
 "draw_planet", "init_planet", NULL, &planet_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Animates texture mapped sphere (planet)", 0, NULL};
#endif

#include "../images/earth.xbm"
#include "xpm-ximage.h"


/*-
 * slices and stacks are used in the sphere parameterization routine.
 * more slices and stacks will increase the quality of the sphere,
 * at the expense of rendering speed
 */

#define NUM_STARS 1000
#define SLICES 15
#define STACKS 15

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

} planetstruct;


static planetstruct *planets = NULL;


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


/* Set up and enable texturing on our object */
static void
setup_xbm_texture (char *bits, int width, int height,
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

  glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0,
			   GL_RGB, GL_UNSIGNED_BYTE, data);

  /* setup parameters for texturing */
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}


static void
setup_file_texture (ModeInfo *mi, char *filename)
{
  Display *dpy = mi->dpy;
  Visual *visual = mi->xgwa.visual;
  Colormap cmap = mi->xgwa.colormap;

#ifdef HAVE_XPM
  {
	char **xpm_data = 0;
	int result = XpmReadFileToData (filename, &xpm_data);
	switch (result) {
	case XpmSuccess:
	  {
		XImage *image = xpm_to_ximage (dpy, visual, cmap, xpm_data);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
					 image->width, image->height, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, image->data);

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
	  fprintf (stderr, "%s: file %s doesn't exist.\n", progname, filename);
	  exit (-1);
	  break;

	case XpmFileInvalid:
	  /* Fall through and try it as an XBM. */
	  break;

	case XpmNoMemory:
	  fprintf (stderr, "%s: XPM: out of memory\n", progname);
	  exit (-1);
	  break;

	default:
	  fprintf (stderr, "%s: XPM: unknown error code %d\n", progname, result);
	  exit (-1);
	  break;
	}
  }
#endif /* HAVE_XPM */

#ifdef HAVE_XMU
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
# ifdef HAVE_XPM
		fprintf (stderr, "%s: not an XPM file: %s\n", progname, filename);
# endif
		fprintf (stderr, "%s: not an XBM file: %s\n", progname, filename);
		exit (1);
	  }

	setup_xbm_texture ((char *) data, width, height, &gp->fg, &gp->bg);
  }
#else  /* !XMU */

# ifdef HAVE_XPM
  fprintf (stderr, "%s: not an XPM file: %s\n", progname, filename);
# endif
  fprintf (stderr, "%s: your vendor doesn't ship the standard Xmu library.\n",
		   progname);
  fprintf (stderr, "%s: we can't load XBM files without it.\n",progname);
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


/* Function for determining points on the surface of the sphere */
static void inline ParametricSphere(float theta, float rho, GLfloat *vector)
{
  vector[0] = -sin(theta) * sin(rho);
  vector[1] = cos(theta) * sin(rho);
  vector[2] = cos(rho);

#if DO_HELIX
  vector[0] = -(1- cos(theta)) * cos(rho); 
  vector[1] = -(1- cos(theta)) * sin(rho); 
  vector[2] = -(sin(theta) + rho); 
#endif /* DO_HELIX */

	return;
}


/* lame way to generate some random stars */
void generate_stars(int width, int height)
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
  int i, j;
  int stacks=STACKS, slices=SLICES;
  float radius=RADIUS;

  float drho, dtheta;
  float rho, theta;
  GLfloat vector[3];
  GLfloat ds, dt, t, s;;

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
	generate_stars(MI_WIDTH(mi), MI_HEIGHT(mi));
  }


  /*-
   * Generate a sphere with quadrilaterals.
   * Quad vertices are determined using a parametric sphere function.
   * For fun, you could generate practically any parameteric surface and
   * map an image onto it. 
   */

  drho = M_PI / stacks;
  dtheta = 2.0 * M_PI / slices;
  ds = 1.0 / slices;
  dt = 1.0 / stacks;
  

  gp->platelist=glGenLists(1);
  glNewList(gp->platelist, GL_COMPILE);

  glColor3f(1,1,1);
  glBegin( wire ? GL_LINE_LOOP : GL_QUADS );

  t = 0.0;
  for(i=0; i<stacks; i++) {
	rho = i * drho;
	s = 0.0;
	for(j=0; j<slices; j++) {
	  theta = j * dtheta;


	  glTexCoord2f(s,t);
	  ParametricSphere(theta, rho, vector);
	  normalize(vector);
	  glNormal3fv(vector);
	  ParametricSphere(theta, rho, vector);
	  glVertex3f( vector[0]*radius, vector[1]*radius, vector[2]*radius );

	  glTexCoord2f(s,t+dt);
	  ParametricSphere(theta, rho+drho, vector);
	  normalize(vector);
	  glNormal3fv(vector);
	  ParametricSphere(theta, rho+drho, vector);
	  glVertex3f( vector[0]*radius, vector[1]*radius, vector[2]*radius );

	  glTexCoord2f(s+ds,t+dt);
	  ParametricSphere(theta + dtheta, rho+drho, vector);
	  normalize(vector);
	  glNormal3fv(vector);
	  ParametricSphere(theta + dtheta, rho+drho, vector);
	  glVertex3f( vector[0]*radius, vector[1]*radius, vector[2]*radius );

	  glTexCoord2f(s+ds, t);
	  ParametricSphere(theta + dtheta, rho, vector);
	  normalize(vector);
	  glNormal3fv(vector);
	  ParametricSphere(theta + dtheta, rho, vector);
	  glVertex3f( vector[0]*radius, vector[1]*radius, vector[2]*radius );

	  s = s + ds;

	}
	t = t + dt;
  }
  glEnd();
  glEndList();


 }

static void
draw_sphere(ModeInfo * mi)
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
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  gp->box_width =  15.0;
  gp->box_height = 15.0;
  gp->box_depth =  5.0;

  gp->tx = 0.0;
  gp->ty = 0.0;
  gp->tz = frand(360);

  gp->dtx = (frand(0.4) + frand(0.3)) * RANDSIGN();
  gp->dty = (frand(0.4) + frand(0.3)) * RANDSIGN();
  gp->dtz = (frand(5.0) + frand(5.0));  /* the sun sets in the west */

  gp->dx = (frand(0.2) + frand(0.2)) * RANDSIGN();
  gp->dy = (frand(0.2) + frand(0.2)) * RANDSIGN();
  gp->dz = (frand(0.2) + frand(0.2)) * RANDSIGN();
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
	  /* Move in the direction we had been moving in. */
	  gp->xpos += gp->dx;
	  gp->ypos += gp->dy;
	  gp->zpos += gp->dz;

	  /* Bounce. */
	  if (gp->xpos > gp->box_depth)
		gp->xpos = gp->box_depth, gp->dx = -gp->dx;
	  else if (gp->xpos < 0)
		gp->xpos = 0, gp->dx = -gp->dx;

	  if (gp->ypos > gp->box_width/2)
		gp->ypos = gp->box_width/2, gp->dy = -gp->dy;
	  else if (gp->ypos < -gp->box_width/2)
		gp->ypos = -gp->box_width/2, gp->dy = -gp->dy;

	  if (gp->zpos > gp->box_height/2)
		gp->zpos = gp->box_height/2, gp->dz = -gp->dz;
	  else if (gp->zpos < -gp->box_height/2)
		gp->zpos = -gp->box_height/2, gp->dz = -gp->dz;
	}
}


/* Standard reshape function */
static void
reshape(int width, int height)
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
init_planet(ModeInfo * mi)
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
	char *f = get_string_resource("imageForeground", "Foreground");
	char *b = get_string_resource("imageBackground", "Background");
	char *s;
	if (!f) f = strdup("white");
	if (!b) b = strdup("black");
	
	for (s = f + strlen(f)-1; s > f; s--)
	  if (*s == ' ' || *s == '\t')
		*s = 0;
	for (s = b + strlen(b)-1; s > b; s--)
	  if (*s == ' ' || *s == '\t')
		*s = 0;

    if (!XParseColor(mi->dpy, mi->xgwa.colormap, f, &gp->fg))
      {
		fprintf(stderr, "%s: unparsable color: \"%s\"\n", progname, f);
		exit(1);
      }
    if (!XParseColor(mi->dpy, mi->xgwa.colormap, b, &gp->bg))
      {
		fprintf(stderr, "%s: unparsable color: \"%s\"\n", progname, f);
		exit(1);
      }

	free (f);
	free (b);
  }


  gp->window = MI_WINDOW(mi);
  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape(MI_WIDTH(mi), MI_HEIGHT(mi));
	pinit(mi);
  } else {
	MI_CLEARWINDOW(mi);
  }
}

void
draw_planet(ModeInfo * mi)
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
	draw_sphere(mi);
  }
  glPopMatrix();
  glPopAttrib();



  glFinish();
  glXSwapBuffers(display, window);

  rotate_and_move (mi);
}

void
release_planet(ModeInfo * mi)
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
	planets = NULL;
  }
  FreeAllGL(mi);
}


#endif

