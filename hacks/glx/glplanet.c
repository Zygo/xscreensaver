/* -*- Mode: C; tab-width: 4 -*- */
/* glplanet --- 3D rotating planet, e.g., Earth.
 *
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
 * 26-Feb-2022 jwz@jwz.org      Display timezones.
 * 04-Feb-2019 jwz@jwz.org      Mercator and Equirectangular modes.
 * 17-Apr-2018 jwz@jwz.org      Blended terminator on day/night images.
 * 10-Nov-2014 jwz@jwz.org      Day and night images. Better stars.
 * 16-Jan-2002 jwz@jwz.org      gdk_pixbuf support.
 * 21-Mar-2001 jwz@jwz.org      Broke sphere routine out into its own file.
 * 09-Oct-1998 dek@cgl.ucsf.edu Added stars.
 * 08-Oct-1998 jwz@jwz.org      Made the 512x512x1 xearth image be built in.
 *                              Made it possible to load XPM or XBM files.
 *                              Made the planet bounce and roll around.
 * 08-Oct-1998 dek@cgl.ucsf.edu Created.
 */

#ifdef STANDALONE
#define DEFAULTS	"*delay:			20000   \n"	\
					"*showFPS:			False   \n" \
					"*wireframe:		False	\n"	\
					"*imageForeground:	Green	\n" \
					"*imageBackground:	Blue	\n" \
					"*suppressRotationAnimation: True\n" \

# define release_planet 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#include "sphere.h"

#define DEF_ROTATE  "True"
#define DEF_ROLL    "True"
#define DEF_WANDER  "True"
#define DEF_SPIN    "1.0"
#define DEF_TEXTURE "True"
#define DEF_STARS   "True"
#define DEF_RESOLUTION "128"
#define DEF_IMAGE   "BUILTIN"
#define DEF_IMAGE2  "BUILTIN"
#define DEF_MODE    "RANDOM"

#define BLENDED_TERMINATOR

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static int do_rotate;
static int do_roll;
static int do_wander;
static int do_texture;
static int do_stars;
static char *which_image;
static char *which_image2;
static int resolution;
static GLfloat spin_arg;
static char *mode_arg;

static XrmOptionDescRec opts[] = {
  {"-rotate",  ".rotate",  XrmoptionNoArg, "true" },
  {"+rotate",  ".rotate",  XrmoptionNoArg, "false" },
  {"-roll",    ".roll",    XrmoptionNoArg, "true" },
  {"+roll",    ".roll",    XrmoptionNoArg, "false" },
  {"-wander",  ".wander",  XrmoptionNoArg, "true" },
  {"+wander",  ".wander",  XrmoptionNoArg, "false" },
  {"-texture", ".texture", XrmoptionNoArg, "true" },
  {"+texture", ".texture", XrmoptionNoArg, "false" },
  {"-stars",   ".stars",   XrmoptionNoArg, "true" },
  {"+stars",   ".stars",   XrmoptionNoArg, "false" },
  {"-spin",    ".spin",    XrmoptionSepArg, 0 },
  {"-no-spin", ".spin",    XrmoptionNoArg, "0" },
  {"-image",   ".image",   XrmoptionSepArg, 0 },
  {"-image2",  ".image2",  XrmoptionSepArg, 0 },
  {"-resolution", ".resolution", XrmoptionSepArg, 0 },
  {"-mode",    ".mode",    XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_rotate,   "rotate",  "Rotate",  DEF_ROTATE,  t_Bool},
  {&do_roll,     "roll",    "Roll",    DEF_ROLL,    t_Bool},
  {&do_wander,   "wander",  "Wander",  DEF_WANDER,  t_Bool},
  {&do_texture,  "texture", "Texture", DEF_TEXTURE, t_Bool},
  {&do_stars,    "stars",   "Stars",   DEF_STARS,   t_Bool},
  {&spin_arg,    "spin",    "Spin",    DEF_SPIN,    t_Float},
  {&which_image, "image",   "Image",   DEF_IMAGE,   t_String},
  {&which_image2,"image2",  "Image",   DEF_IMAGE2,  t_String},
  {&mode_arg,    "mode"     ,"Mode"  , DEF_MODE,    t_String},
  {&resolution,  "resolution","Resolution", DEF_RESOLUTION, t_Int},
};

ENTRYPOINT ModeSpecOpt planet_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   planet_description =
{"planet", "init_planet", "draw_planet", NULL,
 "draw_planet", "init_planet", "free_planet", &planet_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Animates texture mapped sphere (planet)", 0, NULL};
#endif

#include "images/gen/earth_png.h"
#include "images/gen/earth_night_png.h"

#include "ximage-loader.h"
#include "rotator.h"
#include "gltrackball.h"
#include "gllist.h"

extern const struct gllist *timezones;

/*-
 * slices and stacks are used in the sphere parameterization routine.
 * more slices and stacks will increase the quality of the sphere,
 * at the expense of rendering speed
 */

/* structure for holding the planet data */
typedef struct {
  GLuint platelist;
  GLuint shadowlist;
  GLuint latlonglist;
  GLuint tzlist;
  GLuint starlist;
  int starcount;
  int tzpoints;
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
  GLfloat z;
  GLfloat tilt;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  GLuint tex1, tex2;
  int draw_axis;
  Bool timezones_p;
  enum { GLOBE, EQUIRECTANGULAR, MERCATOR } mode;
  Bool random_p;

} planetstruct;


static planetstruct *planets = NULL;


/* Set up and enable texturing on our object */
static void
setup_xpm_texture (ModeInfo *mi, const unsigned char *data, unsigned long size)
{
  XImage *image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                        data, size);
  char buf[1024];
  clear_gl_error();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  /* iOS invalid enum:
  glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
  */
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               image->width, image->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "builtin texture (%dx%d)", image->width, image->height);
  check_gl_error(buf);
  XDestroyImage (image);
}


static Bool
setup_file_texture (ModeInfo *mi, char *filename)
{
  Display *dpy = mi->dpy;
  Visual *visual = mi->xgwa.visual;
  char buf[1024];

  XImage *image = file_to_ximage (dpy, visual, filename);
  if (!image) return False;

  clear_gl_error();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               image->width, image->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "texture: %.100s (%dx%d)",
           filename, image->width, image->height);
  check_gl_error(buf);
  return True;
}


static void
setup_texture (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  glGenTextures (1, &gp->tex1);
  glBindTexture (GL_TEXTURE_2D, gp->tex1);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (!which_image ||
	  !*which_image ||
	  !strcmp(which_image, "BUILTIN"))
    {
    BUILTIN1:
      setup_xpm_texture (mi, earth_png, sizeof(earth_png));
    }
  else
    {
      if (! setup_file_texture (mi, which_image))
        goto BUILTIN1;
    }

  check_gl_error("texture 1 initialization");

  glGenTextures (1, &gp->tex2);
  glBindTexture (GL_TEXTURE_2D, gp->tex2);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (!which_image2 ||
	  !*which_image2 ||
	  !strcmp(which_image2, "BUILTIN"))
    {
    BUILTIN2:
      setup_xpm_texture (mi, earth_night_png, sizeof(earth_night_png));
    }
  else
    {
      if (! setup_file_texture (mi, which_image2))
        goto BUILTIN2;
    }

  check_gl_error("texture 2 initialization");

  /* Need to flip the texture top for bottom for some reason. */
  glMatrixMode (GL_TEXTURE);
  glScalef (1, -1, 1);
  glMatrixMode (GL_MODELVIEW);
}


static void
unit_mercator (int stacks, int slices, int wire_p, Bool mercp)
{
  int i, j;
  GLfloat x, y, ty, xs, ys;
  GLfloat lastx = 0, lasty = 0, lastty = 0;
  GLfloat r, north, south;
  GLfloat eqoff = 0;

  if (wire_p && mercp) stacks += 4;

  stacks /= 2;
  xs = 1.0 / slices;
  ys = 1.0 / stacks;

  glPushMatrix();
  r = 1.8;
  glScalef (r, r, r);
  glFrontFace(GL_CW);

  r = 0.35;  /* Grids are roughly square at equator */

  if (mercp)
    {
      /* The poles go to infinity. The traditional Mercator projection
         omits the Northern and Southern latitudes asymmetrically to
         move Europe toward the center.  How Colonial! */
      north =  85 / 180.0;
      south = -66 / 180.0;
    }
  else
    {
      /* Empirically, this puts the parallels in the right place. */
      north = 73.5 / 180.0;
      south = -north;
    }

  if (wire_p && mercp)
    eqoff = (south + (north - south) / 2);  /* equator offset */

  for (j = 0, y = -0.5, ty = 0; j <= stacks; 
       lasty = y, lastty = ty, y += ys, j++)
    {
      GLfloat th;

      if (j == 1) y += eqoff;  /* don't adjust end caps for equator */
      else if (j == stacks) y -= eqoff;

      ty = (0.5 - y) * (south - north) - south;
      ty += 0.5;

      th = M_PI * (ty - 0.5); /* latitude in radians */

      if (mercp)
        {
          /* Obviously I have no idea what I'm doing here */
          ty = 2 * (atan (pow(M_E, th)) - M_PI/4);
          ty *= 0.41;
          ty += 0.5;
        }

      /* Draw one ring of quads.
       */
      if (j > 0)
        {
          if (!wire_p) glBegin (GL_QUADS);
          for (i = 0, x = 0, lastx = 0; i <= slices; lastx = x, x += xs, i++)
            {
              GLfloat xx = r * cos(M_PI * 2 * x);
              GLfloat yy = r * sin(M_PI * 2 * x);
              GLfloat lx = r * cos(M_PI * 2 * lastx);
              GLfloat ly = r * sin(M_PI * 2 * lastx);
              GLfloat y2  = y;
              GLfloat ly2 = lasty;

              if (i == 0) continue;
              if (wire_p) glBegin(GL_LINE_LOOP);
              glNormal3f (lx, 0, ly);
              glTexCoord2f (lastx, lastty); glVertex3f (lx, ly2, ly);
              glNormal3f (xx, 0, yy);
              glTexCoord2f (x,     lastty); glVertex3f (xx, ly2, yy);
              glNormal3f (xx, 0, yy);
              glTexCoord2f (x,     ty);     glVertex3f (xx, y2,   yy);
              glNormal3f (lx, 0, ly);
              glTexCoord2f (lastx, ty);     glVertex3f (lx, y2,   ly);
              if (wire_p) glEnd();
            }
          if (!wire_p) glEnd();
        }

      /* Draw the end caps
       */
      if (j == 0 || j == stacks)
        {
          GLfloat xx, yy, lxx, lyy;

          glNormal3f (0, (j == 0 ? -1 : 1), 0);
          glFrontFace(j == 0 ? GL_CCW : GL_CW);
          glBegin (wire_p ? GL_LINE_LOOP : GL_TRIANGLES);
          for (i = 0, x = 0, lastx = 0, lxx = 0;
               i <= slices;
               lastx = x, lxx = xx, lyy = yy, x += xs, i++)
            {
              xx = r * cos(M_PI * 2 * x);
              yy = r * sin(M_PI * 2 * x);
              if (i == 0) continue;

              glTexCoord2f (x, j == 0 ? 0 : 1);
              glVertex3f (0, y, 0);
              glTexCoord2f (lastx, ty); glVertex3f (lxx, y, lyy);
              glTexCoord2f (x, ty);     glVertex3f (xx,  y, yy);
            }
          glEnd();
          if (j == 0)
            glFrontFace (GL_CW);
        }
    }

  glPopMatrix();
}



static void
init_stars (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i, j;
  int width  = MI_WIDTH(mi);
  int height = MI_HEIGHT(mi);
  int size = (width > height ? width : height);
  int nstars = size * size / 80;
  int max_size = 3;
  GLfloat inc = 0.5;
  int steps = max_size / inc;
  GLfloat scale = 1;

  if (MI_WIDTH(mi) > 2560) {  /* Retina displays */
    scale *= 2;
    nstars /= 2;
  }

  gp->starlist = glGenLists(1);
  glNewList(gp->starlist, GL_COMPILE);
  for (j = 1; j <= steps; j++)
    {
      glPointSize(inc * j * scale);
      glBegin (GL_POINTS);
      for (i = 0; i < nstars / steps; i++)
        {
          GLfloat d = 0.1;
          GLfloat r = 0.15 + frand(0.3);
          GLfloat g = r + frand(d) - d;
          GLfloat b = r + frand(d) - d;

          GLfloat x = frand(1)-0.5;
          GLfloat y = frand(1)-0.5;
          GLfloat z = ((random() & 1)
                       ? frand(1)-0.5
                       : (BELLRAND(1)-0.5)/12);   /* milky way */
          d = sqrt (x*x + y*y + z*z);
          x /= d;
          y /= d;
          z /= d;
          glColor3f (r, g, b);
          glVertex3f (x, y, z);
          gp->starcount++;
        }
      glEnd ();
    }
  glEndList ();

  check_gl_error("stars initialization");
}


#ifdef BLENDED_TERMINATOR
static void
terminator_tube (ModeInfo *mi, int resolution)
{
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat th;
  GLfloat step = M_PI*2 / resolution;
  GLfloat thickness = 0.1;  /* Dusk is about an hour wide. */
  GLfloat c1[] = { 0, 0, 0, 1 };
  GLfloat c2[] = { 0, 0, 0, 0 };

  glPushMatrix();
  if (wire)
    {
      c1[0] = c1[1] = 0.5;
      c2[2] = 0.5;
      glLineWidth (4);
    }
  glRotatef (90, 1, 0, 0);
  glScalef (1.02, 1.02, 1.02);
  glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
  for (th = 0; th < M_PI*2 + step; th += step)
    {
      GLfloat x = cos(th);
      GLfloat y = sin(th);
      glColor4fv (c1);
      if (!do_texture)
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c1);
      glNormal3f (x, y, 0);
      glVertex3f (x, y,  thickness);
      glColor4fv (c2);
      if (!do_texture)
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c2);
      glVertex3f (x, y, -thickness);
    }
  glEnd();

  /* There's a bit of a spike in the shading where the tube overlaps
     the sphere, so extend the sphere a lot to try and avoid that. */
# if 0 /* Nope, that doesn't help. */
  glColor4fv (c1);
  if (!do_texture)
    glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c1);
  glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
  for (th = 0; th < M_PI*2 + step; th += step)
    {
      GLfloat x = cos(th);
      GLfloat y = sin(th);
      glNormal3f (x, y, 0);
      glVertex3f (x, y, thickness);
      glVertex3f (x, y, thickness + 10);
    }
  glEnd();

  glColor4fv (c2);
  if (!do_texture)
    glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c2);
  glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
  for (th = 0; th < M_PI*2 + step; th += step)
    {
      GLfloat x = cos(th);
      GLfloat y = sin(th);
      glNormal3f (x, y, 0);
      glVertex3f (x, y, -thickness);
      glVertex3f (x, y, -thickness - 10);
    }
  glEnd();
# endif /* 0 */

  glPopMatrix();
}
#endif /* BLENDED_TERMINATOR */


ENTRYPOINT void
reshape_planet (ModeInfo *mi, int width, int height)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glXMakeCurrent(MI_DISPLAY(mi), gp->window, *gp->glx_context);

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40);

  {
# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
    int o = (int) current_device_rotation();
# else
    int o = (width > height ? 0 : 90);
# endif
    if (o != 0 && o != 180 && o != -180)
      glScalef (h, h, h);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT Bool
planet_handle_event (ModeInfo *mi, XEvent *event)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, gp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &gp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void
init_planet (ModeInfo * mi)
{
  planetstruct *gp;
  int screen = MI_SCREEN(mi);
  Bool wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, planets);
  gp = &planets[screen];

  gp->window = MI_WINDOW(mi);

  if ((gp->glx_context = init_GL(mi)) != NULL) {
	reshape_planet(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  if (!mode_arg || !*mode_arg || !strcasecmp(mode_arg, "random"))
    {
      if (random() % 6)
        gp->mode = GLOBE;
      else
        gp->mode = MERCATOR, gp->random_p = True;
    }
  else if (!strcasecmp(mode_arg, "globe"))
    gp->mode = GLOBE;
  else if (!strcasecmp(mode_arg, "equirectangular") ||
           !strcasecmp(mode_arg, "rectangular") ||
           !strcasecmp(mode_arg, "rect") ||
           !strcasecmp(mode_arg, "equi") ||
           !strcasecmp(mode_arg, "eq"))
    gp->mode = EQUIRECTANGULAR;
  else if (!strcasecmp(mode_arg, "mercator") ||
           !strcasecmp(mode_arg, "merc"))
    gp->mode = MERCATOR;
  else
    {
      fprintf (stderr,
               "%s: mode must be 'globe', 'merecator' or 'equirectangular'," 
               " not '%s'\n", progname, mode_arg);
      exit (1);
    }

  {
	char *f = get_string_resource(mi->dpy, "imageForeground", "Foreground");
	char *b = get_string_resource(mi->dpy, "imageBackground", "Background");
	char *s;
	if (!f) f = strdup("white");
	if (!b) b = strdup("black");
	
	for (s = f + strlen(f)-1; s > f; s--)
	  if (*s == ' ' || *s == '\t')
		*s = 0;
	for (s = b + strlen(b)-1; s > b; s--)
	  if (*s == ' ' || *s == '\t')
		*s = 0;

	free (f);
	free (b);
  }

  {
    double spin_speed   = 0.1;
    double wander_speed = 0.005;
    gp->rot = make_rotator (do_roll ? spin_speed : 0,
                            do_roll ? spin_speed : 0,
                            0, 1,
                            (do_wander && !gp->random_p) ? wander_speed : 0,
                            True);
    gp->z = frand (1.0);
    gp->tilt = frand (23.4);
    gp->trackball = gltrackball_init (True);
  }

  if (!wire && !do_texture)
    {
      GLfloat pos[4] = {1, 1, 1, 0};
      GLfloat amb[4] = {0, 0, 0, 1};
      GLfloat dif[4] = {1, 1, 1, 1};
      GLfloat spc[4] = {0, 1, 1, 1};
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  if (wire)
    do_texture = False;

  if (do_texture)
    setup_texture (mi);

  if (do_stars)
    init_stars (mi);

  /* construct the polygons of the planet
   */
  gp->platelist = glGenLists(1);
  glNewList (gp->platelist, GL_COMPILE);
  glFrontFace(GL_CCW);
  glPushMatrix();
  glRotatef (90, 1, 0, 0);
  if (gp->mode == GLOBE)
    unit_sphere (resolution, resolution, wire);
  else
    unit_mercator (resolution, resolution, wire, (gp->mode == MERCATOR));
  glPopMatrix();
  glEndList();

  gp->shadowlist = glGenLists(1);
  glNewList (gp->shadowlist, GL_COMPILE);
  glFrontFace(GL_CCW);

  if (wire)
    glColor4f (0.5, 0.5, 0, 1);
# ifdef BLENDED_TERMINATOR
  else
    {
      GLfloat c[] = { 0, 0, 0, 1 };
      glColor4fv (c);
      if (!do_texture)
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
    }
# endif

  if (gp->mode == GLOBE)
    {
      glPushMatrix();
      glScalef (1.01, 1.01, 1.01);
      unit_dome (resolution, resolution, wire);

# ifdef BLENDED_TERMINATOR
      terminator_tube (mi, resolution);
      if (!wire)
        {
          /* We have to draw the transparent side of the mask too, 
             though I'm not sure why. */
          GLfloat c[] = { 0, 0, 0, 0 };
          glColor4fv (c);
          if (!do_texture)
            glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
          glRotatef (180, 1, 0, 0);
          unit_dome (resolution, resolution, wire);
        }
# endif
    }

  glPopMatrix();
  glEndList();

  /* construct the polygons of the latitude/longitude/axis lines.
   */
  gp->latlonglist = glGenLists(1);
  glNewList (gp->latlonglist, GL_COMPILE);
  glPushMatrix ();
  glRotatef (90, 1, 0, 0);  /* unit_sphere is off by 90 */
  glRotatef (8,  0, 1, 0);  /* line up the time zones */
  if (gp->mode == GLOBE)
    unit_sphere (12, 24, 1);
  else
    unit_mercator (20, 24, 1, (gp->mode == MERCATOR));
  glBegin(GL_LINES);
  glVertex3f(0, -2, 0);
  glVertex3f(0,  2, 0);
  glEnd();
  glPopMatrix ();
  glEndList();

  /* Construct the line segments of the timezone lines.  The list of line
     segments is in a flat equirectangular map, so we need to project that
     onto a sphere, and interpolate additional segments within each line to
     make it curve.  It would be possible to also project these to a cone for
     the Mercator and Equirectangular modes, but I haven't bothered.
   */
  gp->tzpoints = 0;
  gp->tzlist = glGenLists(1);
  glNewList (gp->tzlist, GL_COMPILE);
  glPushMatrix ();
  glRotatef (90, 1, 0, 0);
  glRotatef (180, 0, 0, 1);
  glRotatef (180,  0, 1, 0);  /* line up the time zones */
  {
    GLfloat minx = 99999, miny = 99999, maxx = -99999, maxy = -99999;
    const GLfloat *p = (GLfloat *) timezones->data;
    int lines = timezones->points / 2;
    GLfloat min_seg = 0.05;
    int i;
    if (timezones->primitive != GL_LINES) abort();
    if (timezones->format != GL_V3F) abort();

    /* Find the bounding box. */
    for (i = 0; i < timezones->points; i++)
      {
        GLfloat x = p[i * 3];
        GLfloat y = p[i * 3 + 1];
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
      }

    for (i = 0; i < lines; i++)
      {
        GLfloat x0 = minx + p[0] / (maxx-minx);
        GLfloat y0 = miny + p[1] / (maxy-miny);
        GLfloat x1 = minx + p[3] / (maxx-minx);
        GLfloat y1 = miny + p[4] / (maxy-miny);

        GLfloat d = sqrt ((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
        int steps = d / min_seg;
        int i;
        if (steps == 0) steps = 1;

        glBegin (GL_LINE_STRIP);
        for (i = 0; i <= steps; i++)
          {
            GLfloat r = i / (GLfloat) steps;
            GLfloat x2 = x0 + r * (x1 - x0);
            GLfloat y2 = y0 + r * (y1 - y0);
            GLfloat th1 =  y2 * M_PI - M_PI_2;  /* longitude radians */
            GLfloat th2 = -x2 * M_PI * 2;       /* latitude radians */
            GLfloat x3 = cos(th1) * cos(th2);
            GLfloat y3 = sin(th1);
            GLfloat z3 = cos(th1) * sin(th2);
            glVertex3f (x3, y3, z3);
            gp->tzpoints++;
          }
        glEnd();
        p += 6;
      }
  }
  glPopMatrix ();
  glEndList();
}


ENTRYPOINT void
draw_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  double x, y, z;

  if (!gp->glx_context)
	return;

  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glXMakeCurrent (dpy, window, *gp->glx_context);

  mi->polygon_count = 0;

  if (gp->button_down_p)
    {
      if (! gp->draw_axis)
        gp->timezones_p = (gp->mode == GLOBE && !(random() % 2));
      gp->draw_axis = 60;
    }
  else if (!gp->draw_axis && !(random() % 1000))
    {
      gp->draw_axis = 60 + (random() % 90);
      gp->timezones_p = (gp->mode == GLOBE && !(random() % 10));
    }

  if (do_rotate && !gp->button_down_p)
    {
      int wat = gp->mode == GLOBE ? 1 : -1;
      gp->z -= 0.001 * spin_arg * wat;     /* the sun sets in the west */
      if (gp->z < 0) gp->z += 1;
    }

  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK); 

  glPushMatrix();

  get_position (gp->rot, &x, &y, &z, !gp->button_down_p);
  x = (x - 0.5) * 6;
  y = (y - 0.5) * 6;
  z = (z - 0.5) * 3;
  glTranslatef(x, y, z);

  gltrackball_rotate (gp->trackball);

  if (do_roll)
    {
      get_rotation (gp->rot, &x, &y, 0, !gp->button_down_p);
      glRotatef (x * 360, 1.0, 0.0, 0.0);
      glRotatef (y * 360, 0.0, 1.0, 0.0);
    }
  else
    glRotatef (current_device_rotation(), 0, 0, 1);

  if (do_stars)
    {
      glDisable(GL_TEXTURE_2D);
      glPushMatrix();
      glScalef (60, 60, 60);
      glRotatef (90, 1, 0, 0);
      glRotatef (35, 1, 0, 0);
      glCallList (gp->starlist);
      mi->polygon_count += gp->starcount;
      glPopMatrix();
      glClear(GL_DEPTH_BUFFER_BIT);
    }

  glRotatef (90, 1, 0, 0);
  glRotatef (35, 1, 0, 0);
  glRotatef (10, 0, 1, 0);
  glRotatef (120, 0, 0, 1);

  glScalef (3, 3, 3);

# ifdef HAVE_MOBILE
  glScalef (2, 2, 2);
# endif

  if (wire)
    glColor3f (0, 0, 0.5);
  else if (do_texture)
    {
      glColor4f (1, 1, 1, 1);
      glEnable (GL_TEXTURE_2D);
      glBindTexture (GL_TEXTURE_2D, gp->tex1);
    }
  else
    {
      GLfloat c[] = { 0, 0.5, 0, 1 };
      glColor4fv (c);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
    }

  glPushMatrix();
  glRotatef (gp->z * 360, 0, 0, 1);
  glCallList (gp->platelist);
  mi->polygon_count += resolution*resolution;
  glPopMatrix();

  if (wire)
    {
      glPushMatrix();
      glRotatef (gp->tilt, 1, 0, 0);
      glColor3f(1, 0, 0);
      glLineWidth(4);
      glCallList (gp->shadowlist);
      glLineWidth(1);
      mi->polygon_count += resolution*(resolution/2);
      glPopMatrix();
    }

  else if (!do_texture || (gp->tex2 && gp->mode == GLOBE))
    {
      /* Originally we just used GL_LIGHT0 to produce the day/night sides of
         the planet, but that always looked crappy, even with a vast number of
         polygons, because the day/night terminator didn't exactly line up with
         the polygon edges.
       */

#ifndef BLENDED_TERMINATOR

      /* Method 1, use the depth buffer as a stencil.

         - Draw the full "day" sphere;
         - Clear the depth buffer;
         - Draw a rotated/tilted half-sphere into the depth buffer only,
           on the Eastern hemisphere, putting non-zero depth only on the
           sunlit side;
         - Draw the full "night" sphere, which will clip to dark parts only.

         That lets us divide the sphere into the two maps, and the dividing
         line can be at any angle, regardless of polygon layout.

         The half-sphere is scaled slightly larger to avoid polygon fighting,
         since those triangles won't exactly line up because of the rotation.

         The downside of this is that the day/night terminator is 100% sharp.
      */
      glClear (GL_DEPTH_BUFFER_BIT);
      glColorMask (0, 0, 0, 0);
      glDisable (GL_TEXTURE_2D);
      glPushMatrix();
      glRotatef (gp->tilt, 1, 0, 0);
      glScalef (1.01, 1.01, 1.01);
      glCallList (gp->shadowlist);		/* Fill in depth on sunlit side */
      mi->polygon_count += resolution*(resolution/2);
      glPopMatrix();
      glColorMask (1, 1, 1, 1);

      if (do_texture)
        {
          glEnable (GL_TEXTURE_2D);
          glBindTexture (GL_TEXTURE_2D, gp->tex2);
        }
      else
        {
          GLfloat c[] = { 0, 0, 0.5, 1 };
          glColor4fv (c);
          if (! do_texture)
            glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
        }

      glPushMatrix();
      glRotatef (gp->z * 360, 0, 0, 1);
      glCallList (gp->platelist);		/* Fill in color on night side */
      mi->polygon_count += resolution*resolution;
      glPopMatrix();

#else /* BLENDED_TERMINATOR */

      /* Method 2, use the alpha buffer as a stencil.
         - Draw the full "day" sphere;
         - Clear the depth buffer; 
         - Clear the alpha buffer;
         - Draw a rotated/tilted half-sphere into the alpha buffer only,
           on the Eastern hemisphere, putting non-zero alpha only on the
           sunlit side;
         - Also draw a fuzzy terminator ring into the alpha buffer;
         - Clear the depth buffer again; 
         - Draw the full "night" sphere, which will blend to dark parts only.
       */
      glColorMask (0, 0, 0, 1);
      glClear (GL_COLOR_BUFFER_BIT);
      glClear (GL_DEPTH_BUFFER_BIT);
      glDisable (GL_TEXTURE_2D);

      glPushMatrix();
      glRotatef (gp->tilt, 1, 0, 0);
      glScalef (1.01, 1.01, 1.01);
      glCallList (gp->shadowlist);		/* Fill in alpha on sunlit side */
      mi->polygon_count += resolution*(resolution/2);
      glPopMatrix();

      glClear (GL_DEPTH_BUFFER_BIT);

      glColorMask (1, 1, 1, 1);
      {
        GLfloat c[] = { 1, 1, 1, 1 };
        glColor4fv (c);
        if (! do_texture)
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
      }
      glEnable (GL_BLEND);
      glBlendFunc (GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);

      if (do_texture)
        {
          glEnable (GL_TEXTURE_2D);
          glBindTexture (GL_TEXTURE_2D, gp->tex2);
        }
      else
        {
          GLfloat c[] = { 0, 0, 0.5, 1 };
          glColor4fv (c);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
          glEnable (GL_LIGHTING);
        }

      glPushMatrix();
      glRotatef (gp->z * 360, 0, 0, 1);
      glCallList (gp->platelist);		/* Fill in color on night side */
      mi->polygon_count += resolution*resolution;
      glPopMatrix();
      glDisable (GL_BLEND);
      glBlendFunc (GL_ONE, GL_ZERO);

#endif /* BLENDED_TERMINATOR */
    }
  else if (gp->mode != GLOBE)
    {
      glDisable (GL_LIGHTING);
      glDisable (GL_BLEND);
    }

  if (gp->draw_axis && !gp->timezones_p)
    {
      glPushMatrix();
      glRotatef (gp->z * 360, 0.0, 0.0, 1.0);
      glScalef (1.02, 1.02, 1.02);
      glDisable (GL_TEXTURE_2D);
      glDisable (GL_LIGHTING);
      glDisable (GL_LINE_SMOOTH);
      glColor3f (0.1, 0.3, 0.1);
      glCallList (gp->latlonglist);
      mi->polygon_count += 24*24;
      glPopMatrix();
      if (!wire && !do_texture)
        glEnable (GL_LIGHTING);
      if (gp->draw_axis) gp->draw_axis--;
    }
  else if (gp->draw_axis && gp->timezones_p)
    {
      glPushMatrix();
      glRotatef (gp->z * 360, 0.0, 0.0, 1.0);
      glScalef (1.02, 1.02, 1.02);
      glDisable (GL_TEXTURE_2D);
      glDisable (GL_LIGHTING);
      glDisable (GL_LINE_SMOOTH);
      glColor3f (0.1, 0.3, 0.1);
      glCallList (gp->tzlist);
      mi->polygon_count += gp->tzpoints;
      glPopMatrix();
      if (!wire && !do_texture)
        glEnable (GL_LIGHTING);
      if (gp->draw_axis) gp->draw_axis--;
    }

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (!gp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), gp->window, *gp->glx_context);

  if (glIsList(gp->platelist))   glDeleteLists(gp->platelist, 1);
  if (glIsList(gp->shadowlist))  glDeleteLists(gp->shadowlist, 1);
  if (glIsList(gp->latlonglist)) glDeleteLists(gp->latlonglist, 1);
  if (glIsList(gp->tzlist))      glDeleteLists(gp->tzlist, 1);
  if (glIsList(gp->starlist))    glDeleteLists(gp->starlist, 1);
  glDeleteTextures(1, &gp->tex1);
  glDeleteTextures(1, &gp->tex2);

  if (gp->trackball) gltrackball_free (gp->trackball);
  if (gp->rot) free_rotator (gp->rot);
}


XSCREENSAVER_MODULE_2 ("GLPlanet", glplanet, planet)

#endif
