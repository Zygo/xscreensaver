/* lavalite --- 3D Simulation a Lava Lite, written by jwz.
 *
 * This software Copyright (c) 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * LAVA®, LAVA LITE®, LAVA WORLD INTERNATIONAL® and the configuration of the
 * LAVA® brand motion lamp are registered trademarks of Haggerty Enterprises,
 * Inc.  The configuration of the globe and base of the motion lamp are
 * registered trademarks of Haggerty Enterprises, Inc. in the U.S.A. and in
 * other countries around the world.
 *
 * Official Lava Lite web site: http://www.lavaworld.com/
 *
 * Implementation details:
 *
 * The blobs are generated using metaballs.  For an explanation of what
 * those are, see http://astronomy.swin.edu.au/~pbourke/modelling/implicitsurf/
 * or http://www.fifi.org/doc/povray-manual/pov30005.htm
 *
 * Basically, each bubble of lava is a few (4) overlapping spherical metaballs
 * of various sizes following similar but slightly different steep, slow
 * parabolic arcs from the bottom to the top and back.
 *
 * We then polygonize the surface of the lava using the marching squares
 * algorithm implemented in marching.c.
 *
 * Like real lavalites, this program is very slow.
 *
 * Surprisingly, it's loading the CPU and not the graphics engine: the speed
 * bottleneck is in *computing* the scene rather than *rendering* it.  We
 * actually don't use a huge number of polygons, but computing the mesh for
 * the surface takes a lot of cycles.
 *
 * I eliminated all the square roots, but there is still a lot of
 * floating-point multiplication in here.  I tried optimizing away the
 * fp divisions, but that didn't seem to make a difference.
 *
 *     -style            lamp shape: classic, giant, cone, rocket, or random.
 *     -speed            frequency at which new blobs launch.
 *     -resolution       density of the polygon mesh.
 *     -count            max number of blobs allowed at once.
 *     -wander, -spin    whether to roll the scene around the screen.
 *     -lava-color       color of the blobbies
 *     -fluid-color      color of the stuff the blobbies float in
 *     -base-color       color of the base of the lamp
 *     -table-color      color of the table under the lamp
 *     -impatient        at startup, skip forward in the animation so
 *                       that at least one blob is fully exposed.
 *
 * TODO:
 *
 *    - make the table look better, somehow.
 *    - should the lava be emissive?  should the one at the bottom be
 *      more brightly colored than the ones at the top?  light distance?
 *    - is there some way to put a specular reflection on the front glass
 *      itself?  Maybe render it twice with FRONT/BACK tweaked, or alpha
 *      with depth buffering turned off?
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"LavaLite"
#define HACK_INIT	init_lavalite
#define HACK_DRAW	draw_lavalite
#define HACK_RESHAPE	reshape_lavalite
#define HACK_HANDLE_EVENT lavalite_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts	xlockmore_opts

#define DEF_SPIN        "Z"
#define DEF_WANDER      "False"
#define DEF_SPEED       "0.003"
#define DEF_RESOLUTION  "40"
#define DEF_SMOOTH      "True"
#define DEF_COUNT       "3"
#define DEF_STYLE       "random"
#define DEF_IMPATIENT   "False"
#define DEF_LCOLOR	"#FF0000" /* lava */
#define DEF_FCOLOR	"#00AAFF" /* fluid */
#define DEF_BCOLOR	"#666666" /* base */
#define DEF_TCOLOR	"#000000" /*"#00FF00"*/ /* table */

#define DEF_FTEX	"(none)"
#define DEF_BTEX	"(none)"
#define DEF_TTEX	"(none)"

#define DEFAULTS	"*delay:	10000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*geometry:	640x640     \n" \
			"*count:      " DEF_COUNT " \n" \
			"*style:      " DEF_STYLE " \n" \
			"*speed:      " DEF_SPEED " \n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \
			"*resolution: " DEF_RESOLUTION "\n" \
			"*smooth: "     DEF_SMOOTH "\n" \
			"*impatient:  " DEF_IMPATIENT " \n" \
			"*geometry:	600x900\n" \
			"*lavaColor:  "	DEF_LCOLOR "\n" \
			"*fluidColor: "	DEF_FCOLOR "\n" \
			"*baseColor:  "	DEF_BCOLOR "\n" \
			"*tableColor: "	DEF_TCOLOR "\n" \
			"*fluidTexture: "	DEF_FTEX "\n" \
			"*baseTexture:  "	DEF_BTEX "\n" \
			"*tableTexture: "	DEF_TTEX "\n" \


#define BLOBS_PER_GROUP 4

#define GRAVITY         0.000013    /* odwnward acceleration */
#define CONVECTION      0.005       /* initial upward velocity (bell curve) */
#define TILT            0.00166666  /* horizontal velocity (bell curve) */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef ABS
#define ABS(n) ((n)<0?-(n):(n))
#undef SIGNOF
#define SIGNOF(n) ((n)<0?-1:1)

#include "xlockmore.h"
#include "marching.h"
#include "rotator.h"
#include "gltrackball.h"
#include "xpm-ximage.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>


typedef struct metaball metaball;

struct metaball {

  Bool alive_p;
  Bool static_p;

  double r;		/* hard radius */
  double R;		/* radius of field of influence */

  double z;		/* vertical position */
  double pos_r;		/* position on horizontal circle */
  double pos_th;	/* position on horizontal circle */
  double dr, dz;	/* current velocity */

  double x, y;		/* h planar position - compused from the above */

  metaball *leader;	/* stay close to this other ball */
};


typedef enum { CLASSIC = 0, GIANT, CONE, ROCKET } lamp_style;
typedef enum { CAP = 100, BOTTLE, BASE } lamp_part;

typedef struct {
  lamp_part part;
  GLfloat elevation;
  GLfloat radius;
  GLfloat texture_elevation;
} lamp_geometry;

static lamp_geometry classic_lamp[] = {
  { CAP,    1.16, 0.089, 0.00 },
  { BOTTLE, 0.97, 0.120, 0.40 },
  { BOTTLE, 0.13, 0.300, 0.87 },
  { BOTTLE, 0.07, 0.300, 0.93 },
  { BASE,   0.00, 0.280, 0.00 },
  { BASE,  -0.40, 0.120, 0.50 },
  { BASE,  -0.80, 0.280, 1.00 },
  { 0, 0, 0, 0 },
};

static lamp_geometry giant_lamp[] = {
  { CAP,    1.12, 0.105, 0.00 },
  { BOTTLE, 0.97, 0.130, 0.30 },
  { BOTTLE, 0.20, 0.300, 0.87 },
  { BOTTLE, 0.15, 0.300, 0.93 },
  { BASE,   0.00, 0.230, 0.00 },
  { BASE,  -0.18, 0.140, 0.20 },
  { BASE,  -0.80, 0.280, 1.00 },
  { 0, 0, 0, 0 },
};

static lamp_geometry cone_lamp[] = {
  { CAP,    1.35, 0.001, 0.00 },
  { CAP,    1.35, 0.020, 0.00 },
  { CAP,    1.30, 0.055, 0.05 },
  { BOTTLE, 0.97, 0.120, 0.40 },
  { BOTTLE, 0.13, 0.300, 0.87 },
  { BASE,   0.00, 0.300, 0.00 },
  { BASE,  -0.04, 0.320, 0.04 },
  { BASE,  -0.60, 0.420, 0.50 },
  { 0, 0, 0, 0 },
};

static lamp_geometry rocket_lamp[] = {
  { CAP,    1.35, 0.001, 0.00 },
  { CAP,    1.34, 0.020, 0.00 },
  { CAP,    1.30, 0.055, 0.05 },
  { BOTTLE, 0.97, 0.120, 0.40 },
  { BOTTLE, 0.13, 0.300, 0.87 },
  { BOTTLE, 0.07, 0.300, 0.93 },
  { BASE,   0.00, 0.280, 0.00 },
  { BASE,  -0.50, 0.180, 0.50 },
  { BASE,  -0.75, 0.080, 0.75 },
  { BASE,  -0.80, 0.035, 0.80 },
  { BASE,  -0.90, 0.035, 1.00 },
  { 0, 0, 0, 0 },
};



typedef struct {
  GLXContext *glx_context;
  lamp_style style;
  lamp_geometry *model;
  rotator *rot;
  rotator *rot2;
  trackball_state *trackball;
  Bool button_down_p;

  GLfloat max_bottle_radius;	   /* radius of widest part of the bottle */

  GLfloat launch_chance;	   /* how often to percolate */
  int blobs_per_group;		   /* how many metaballs we launch at once */
  Bool just_started_p;		   /* so we launch some goo right away */

  int grid_size;		   /* resolution for marching-cubes */
  int nballs;
  metaball *balls;

  GLuint bottle_list;
  GLuint ball_list;

  int bottle_poly_count;	   /* polygons in the bottle only */

} lavalite_configuration;

static lavalite_configuration *bps = NULL;

static char *do_spin;
static char *do_style;
static GLfloat speed;
static Bool do_wander;
static int resolution;
static Bool do_smooth;
static Bool do_impatient;

static char *lava_color_str, *fluid_color_str, *base_color_str,
  *table_color_str;
static char *fluid_tex, *base_tex, *table_tex;

static GLfloat lava_color[4], fluid_color[4], base_color[4], table_color[4];
static GLfloat lava_spec[4] = {1.0, 1.0, 1.0, 1.0};
static GLfloat lava_shininess = 128.0;
static GLfloat foot_color[4] = {0.2, 0.2, 0.2, 1.0};

static GLfloat light0_pos[4] = {-0.6, 0.0, 1.0, 0.0};
static GLfloat light1_pos[4] = { 1.0, 0.0, 0.2, 0.0};
static GLfloat light2_pos[4] = { 0.6, 0.0, 1.0, 0.0};



static XrmOptionDescRec opts[] = {
  { "-style",  ".style",  XrmoptionSepArg, 0 },
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-resolution", ".resolution", XrmoptionSepArg, 0 },
  { "-smooth", ".smooth", XrmoptionNoArg, "True" },
  { "+smooth", ".smooth", XrmoptionNoArg, "False" },
  { "-impatient", ".impatient", XrmoptionNoArg, "True" },
  { "+impatient", ".impatient", XrmoptionNoArg, "False" },

  { "-lava-color",   ".lavaColor",   XrmoptionSepArg, 0 },
  { "-fluid-color",  ".fluidColor",  XrmoptionSepArg, 0 },
  { "-base-color",   ".baseColor",   XrmoptionSepArg, 0 },
  { "-table-color",  ".tableColor",  XrmoptionSepArg, 0 },

  { "-fluid-texture",".fluidTexture",  XrmoptionSepArg, 0 },
  { "-base-texture", ".baseTexture",   XrmoptionSepArg, 0 },
  { "-table-texture",".tableTexture",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_style,     "style",      "Style",      DEF_STYLE,      t_String},
  {&do_spin,      "spin",       "Spin",       DEF_SPIN,       t_String},
  {&do_wander,    "wander",     "Wander",     DEF_WANDER,     t_Bool},
  {&speed,        "speed",      "Speed",      DEF_SPEED,      t_Float},
  {&resolution,   "resolution", "Resolution", DEF_RESOLUTION, t_Int},
  {&do_smooth,    "smooth",     "Smooth",     DEF_SMOOTH,     t_Bool},
  {&do_impatient, "impatient",  "Impatient",  DEF_IMPATIENT,  t_Bool},

  {&lava_color_str,  "lavaColor",    "LavaColor",  DEF_LCOLOR, t_String},
  {&fluid_color_str, "fluidColor",   "FluidColor", DEF_FCOLOR, t_String},
  {&base_color_str,  "baseColor",    "BaseColor",  DEF_BCOLOR, t_String},
  {&table_color_str, "tableColor",   "TableColor", DEF_TCOLOR, t_String},

  {&fluid_tex,       "fluidTexture", "FluidTexture", DEF_FTEX, t_String},
  {&base_tex,        "baseTexture",  "BaseTexture",  DEF_BTEX, t_String},
  {&table_tex,       "tableTexture", "BaseTexture",  DEF_TTEX, t_String},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
void
reshape_lavalite (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}



/* Textures
 */

static Bool
load_texture (ModeInfo *mi, const char *filename)
{
  Display *dpy = mi->dpy;
  Visual *visual = mi->xgwa.visual;
  Colormap cmap = mi->xgwa.colormap;
  char buf[1024];
  XImage *image;

  if (!filename ||
      !*filename ||
      !strcasecmp (filename, "(none)"))
    {
      glDisable (GL_TEXTURE_2D);
      return False;
    }

  image = xpm_file_to_ximage (dpy, visual, cmap, filename);

  clear_gl_error();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               image->width, image->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "texture: %.100s (%dx%d)",
           filename, image->width, image->height);
  check_gl_error(buf);

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, image->width);
  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glEnable (GL_TEXTURE_2D);
  return True;
}



/* Generating the lamp's bottle, caps, and base.
 */

static int
draw_disc (GLfloat r, GLfloat z, int faces, Bool up_p, Bool wire)
{
  int j;
  GLfloat th;
  GLfloat step = M_PI * 2 / faces;
  int polys = 0;
  GLfloat x, y;

  glFrontFace (up_p ? GL_CW : GL_CCW);
  glNormal3f (0, (up_p ? 1 : -1), 0);
  glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);

  x = r;
  y = 0;

  for (j = 0, th = 0; j <= faces; j++)
    {
      glTexCoord2f (-j / (GLfloat) faces, 1);
      glVertex3f (0, z, 0);

      glTexCoord2f (-j / (GLfloat) faces, 0);
      glVertex3f (x, z, y);

      th += step;
      x  = r * cos (th);
      y  = r * sin (th);

      glTexCoord2f (-j / (GLfloat) faces, 0);
      glVertex3f (x, z, y);

      polys++;
    }
  glEnd();

  return polys;
}


static int
draw_tube (GLfloat r0, GLfloat r1,
           GLfloat z0, GLfloat z1,
           GLfloat t0, GLfloat t1,
           int faces, Bool inside_out_p, Bool smooth_p, Bool wire)
{
  int polys = 0;
  GLfloat th;
  GLfloat x, y, x0=0, y0=0;
  GLfloat step = M_PI * 2 / faces;
  GLfloat s2 = step/2;
  int i;

  glFrontFace (inside_out_p ? GL_CW : GL_CCW);
  glBegin (wire ? GL_LINES : (smooth_p ? GL_QUAD_STRIP : GL_QUADS));

  th = 0;
  x = 1;
  y = 0;

  if (!smooth_p)
    {
      x0 = cos (s2);
      y0 = sin (s2);
    }

  if (smooth_p) faces++;

  for (i = 0; i < faces; i++)
    {
      int nsign = (inside_out_p ? -1 : 1);

      if (smooth_p)
        glNormal3f (x  * nsign, z1, y  * nsign);
      else
        glNormal3f (x0 * nsign, z1, y0 * nsign);

      glTexCoord2f (nsign * -i / (GLfloat) faces, 1-t1);
      glVertex3f (x * r1, z1, y * r1);

      glTexCoord2f (nsign * -i / (GLfloat) faces, 1-t0);
      glVertex3f (x * r0, z0, y * r0);

      th += step;
      x  = cos (th);
      y  = sin (th);

      if (!smooth_p)
        {
          x0 = cos (th + s2);
          y0 = sin (th + s2);

          glTexCoord2f (nsign * -(i+1) / (double) faces, 1-t0);
          glVertex3f (x * r0, z0, y * r0);

          glTexCoord2f (nsign * -(i+1) / (double) faces, 1-t1);
          glVertex3f (x * r1, z1, y * r1);
        }

      polys++;
    }
  glEnd();

  return polys;
}


static int
draw_table (GLfloat z, Bool wire)
{
  GLfloat faces = 6;
  GLfloat step = M_PI * 2 / faces;
  GLfloat s = 8;
  GLfloat th;
  int j;
  int polys = 0;

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, table_color);

  glFrontFace(GL_CW);
  glNormal3f(0, 1, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);

  if (! wire)
    {
      glTexCoord2f (-0.5, 0.5);
      glVertex3f(0, z, 0);
    }

  for (j = 0, th = 0; j <= faces; j++)
    {
      GLfloat x = cos (th);
      GLfloat y = sin (th);
      glTexCoord2f (-(x+1)/2.0, (y+1)/2.0);
      glVertex3f(x*s, z, y*s);
      th += step;
      polys++;
    }
  glEnd();
  return polys;
}


static int
draw_wing (GLfloat w, GLfloat h, GLfloat d, Bool wire)
{
  static int coords[2][8][2] = {
    { {  0,   0 },
      { 10,  10 },
      { 20,  23 },
      { 30,  41 },
      { 40,  64 },
      { 45,  81 },
      { 50, 103 },
      { 53, 134 } },
    { {  0,  54 },
      { 10,  57 },
      { 20,  64 },
      { 30,  75 },
      { 40,  92 },
      { 45, 104 },
      { 50, 127 },
      { 51, 134 } 
    }
  };

  int polys = 0;
  int maxx = coords[0][countof(coords[0])-1][0];
  int maxy = coords[0][countof(coords[0])-1][1];
  unsigned int x;

  for (x = 1; x < countof(coords[0]); x++)
    {
      GLfloat px0 = (GLfloat) coords[0][x-1][0] / maxx * w;
      GLfloat py0 = (GLfloat) coords[0][x-1][1] / maxy * h;
      GLfloat px1 = (GLfloat) coords[1][x-1][0] / maxx * w;
      GLfloat py1 = (GLfloat) coords[1][x-1][1] / maxy * h;
      GLfloat px2 = (GLfloat) coords[0][x  ][0] / maxx * w;
      GLfloat py2 = (GLfloat) coords[0][x  ][1] / maxy * h;
      GLfloat px3 = (GLfloat) coords[1][x  ][0] / maxx * w;
      GLfloat py3 = (GLfloat) coords[1][x  ][1] / maxy * h;
      GLfloat zz = d/2;

      /* Left side
       */
      glFrontFace (GL_CW);
      glNormal3f (0, 0, -1);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

      glTexCoord2f (px0, py0); glVertex3f (px0, -py0, -zz);
      glTexCoord2f (px1, py1); glVertex3f (px1, -py1, -zz);
      glTexCoord2f (px3, py3); glVertex3f (px3, -py3, -zz);
      glTexCoord2f (px2, py2); glVertex3f (px2, -py2, -zz);
      polys++;
      glEnd();

      /* Right side
       */
      glFrontFace (GL_CCW);
      glNormal3f (0, 0, -1);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glTexCoord2f(px0, py0); glVertex3f (px0, -py0, zz);
      glTexCoord2f(px1, py1); glVertex3f (px1, -py1, zz);
      glTexCoord2f(px3, py3); glVertex3f (px3, -py3, zz);
      glTexCoord2f(px2, py2); glVertex3f (px2, -py2, zz);
      polys++;
      glEnd();

      /* Top edge
       */
      glFrontFace (GL_CCW);
      glNormal3f (1, -1, 0); /* #### wrong */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glTexCoord2f(px0, py0); glVertex3f (px0, -py0, -zz);
      glTexCoord2f(px0, py0); glVertex3f (px0, -py0,  zz);
      glTexCoord2f(px2, py2); glVertex3f (px2, -py2,  zz);
      glTexCoord2f(px2, py2); glVertex3f (px2, -py2, -zz);
      polys++;
      glEnd();

      /* Bottom edge
       */
      glFrontFace (GL_CW);
      glNormal3f (-1, 1, 0); /* #### wrong */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glTexCoord2f(px1, py1); glVertex3f (px1, -py1, -zz);
      glTexCoord2f(px1, py1); glVertex3f (px1, -py1,  zz);
      glTexCoord2f(px3, py3); glVertex3f (px3, -py3,  zz);
      glTexCoord2f(px3, py3); glVertex3f (px3, -py3, -zz);
      polys++;
      glEnd();


    }

  return polys;

}


static void
generate_bottle (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int faces = resolution * 1.5;
  Bool smooth = do_smooth;
  Bool have_texture = False;

  lamp_geometry *top_slice = bp->model;
  const char *current_texture = 0;
  lamp_part last_part = 0;

  if (faces < 3)  faces = 3;
  else if (wire && faces > 20) faces = 20;
  else if (faces > 60) faces = 60;

  bp->bottle_poly_count = 0;

  glNewList (bp->bottle_list, GL_COMPILE);
  glPushMatrix();

  glRotatef (90, 1, 0, 0);
  glTranslatef (0, -0.5, 0);

  /* All parts of the lamp use the same specularity and shininess. */
  glMaterialfv (GL_FRONT, GL_SPECULAR,  lava_spec);
  glMateriali  (GL_FRONT, GL_SHININESS, lava_shininess);

  while (1)
    {
      lamp_geometry *bot_slice = top_slice + 1;

      const char *texture = 0;
      GLfloat *color = 0;
      GLfloat t0, t1;

      glDisable (GL_LIGHT2);

      switch (top_slice->part)
        {
        case CAP:
        case BASE:
          texture = base_tex;
          color   = base_color;
          break;
        case BOTTLE:
          texture = fluid_tex;
          color   = fluid_color;
          if (!wire) glEnable (GL_LIGHT2);   /* light2 affects only fluid */
          break;
        default:
          abort();
          break;
        }

      have_texture = False;
      if (!wire && texture && texture != current_texture)
        {
          current_texture = texture;
          have_texture = load_texture (mi, current_texture);
        }
        
      /* Color the discs darker than the tube walls. */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, foot_color);

      /* Do a top disc if this is the first slice of the CAP or BASE.
       */
      if ((top_slice->part == CAP  && last_part == 0) ||
          (top_slice->part == BASE && last_part == BOTTLE))
        bp->bottle_poly_count +=
          draw_disc (top_slice->radius, top_slice->elevation, faces,
                     True, wire);

      /* Do a bottom disc if this is the last slice of the CAP or BASE.
       */
      if ((top_slice->part == CAP  && bot_slice->part == BOTTLE) ||
          (top_slice->part == BASE && bot_slice->part == 0))
        {
          lamp_geometry *sl = (bot_slice->part == 0 ? top_slice : bot_slice);
          bp->bottle_poly_count +=
            draw_disc (sl->radius, sl->elevation, faces, False, wire);
        }

      if (bot_slice->part == 0)    /* done! */
        break;

      /* Do a tube or cone
       */
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

      t0 = top_slice->texture_elevation;
      t1 = bot_slice->texture_elevation;

      /* Restart the texture coordinates for the glass.
       */
      if (top_slice->part == BOTTLE)
        {
          Bool first_p = (top_slice[-1].part != BOTTLE);
          Bool last_p  = (bot_slice->part    != BOTTLE);
          if (first_p) t0 = 0;
          if (last_p)  t1 = 1;
        }

      bp->bottle_poly_count +=
        draw_tube (top_slice->radius, bot_slice->radius,
                   top_slice->elevation, bot_slice->elevation,
                   t0, t1,
                   faces,
                   (top_slice->part == BOTTLE),
                   smooth, wire);

      last_part = top_slice->part;
      top_slice++;
    }

  if (bp->style == ROCKET)
    {
      int i;
      for (i = 0; i < 3; i++)
        {
          glPushMatrix();
          glRotatef (120 * i, 0, 1, 0);
          glTranslatef (0.14, -0.05, 0);
          bp->bottle_poly_count += draw_wing (0.4, 0.95, 0.02, wire);
          glPopMatrix();
        }
      glTranslatef (0, -0.1, 0);  /* move floor down a little */
    }


  have_texture = !wire && load_texture (mi, table_tex);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, table_color);
  bp->bottle_poly_count += draw_table (top_slice->elevation, wire);


  glPopMatrix ();
  glDisable (GL_TEXTURE_2D);   /* done with textured objects */
  glEndList ();
}




/* Generating blobbies
 */

static double
bellrand (double extent)    /* like frand(), but a bell curve. */
{
  return (((frand(extent) + frand(extent) + frand(extent)) / 3)
          - (extent/2));
}


static void move_ball (ModeInfo *mi, metaball *b);

/* Bring a ball into play, and re-randomize its values.
 */
static void
reset_ball (ModeInfo *mi, metaball *b)
{
/*  lavalite_configuration *bp = &bps[MI_SCREEN(mi)]; */

  b->r = 0.00001;
  b->R = 0.12 + bellrand(0.10);

  b->pos_r = bellrand (0.9);
  b->pos_th = frand(M_PI*2);
  b->z = 0;

  b->dr = bellrand(TILT);
  b->dz = CONVECTION;

  b->leader = 0;

  if (!b->alive_p)
    b->alive_p = True;

  move_ball (mi, b);
}


/* returns the first metaball that is not in use, or 0.
 */
static metaball *
get_ball (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < bp->nballs; i++)
    {
      metaball *b = &bp->balls[i];
      if (!b->alive_p)
        return b;
    }
  return 0;
}


/* Generate the blobs that don't move: the ones at teh top and bottom
   that are part of the scenery.
 */
static void
generate_static_blobs (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  metaball *b0, *b1;
  int i;

  b0 = get_ball (mi);
  if (!b0) abort();
  b0->static_p = True;
  b0->alive_p = True;
  b0->R = 0.6;
  b0->r = 0.3;

  /* the giant blob at the bottom of the bottle.
   */
  b0->pos_r = 0;
  b0->pos_th = 0;
  b0->dr = 0;
  b0->dz = 0;
  b0->x = 0;
  b0->y = 0;
  b0->z = -0.43;

  /* the small blob at the top of the bottle.
   */
  b1 = get_ball (mi);
  if (!b1) abort();

  *b1 = *b0;
  b1->R = 0.16;
  b1->r = 0.135;
  b1->z = 1.078;

  /* Some extra blobs at the bottom of the bottle, to jumble the surface.
   */
  for (i = 0; i < bp->blobs_per_group; i++)
    {
      b1 = get_ball (mi);
      if (!b1) abort();
      reset_ball (mi, b1);
      b1->static_p = True;
      b1->z = frand(0.04);
      b1->dr = 0;
      b1->dz = 0;
    }
}


static GLfloat
max_bottle_radius (lavalite_configuration *bp)
{
  GLfloat r = 0;
  lamp_geometry *slice;
  for (slice = bp->model; slice->part != 0; slice++)
    {
      if (slice->part == BOTTLE && slice->radius > r)
        r = slice->radius;      /* top */
      if (slice[1].radius > r)
        r = slice[1].radius;    /* bottom */
    }
  return r;
}


static GLfloat
bottle_radius_at (lavalite_configuration *bp, GLfloat z)
{
  GLfloat topz = -999, botz = -999, topr = 0, botr = 0;
  lamp_geometry *slice;
  GLfloat ratio;

  for (slice = bp->model; slice->part != 0; slice++)
    if (z > slice->elevation)
      {
        slice--;
        topz = slice->elevation;
        topr = slice->radius;
        break;
      }
  if (topz == -999) return 0;

  for (; slice->part != 0; slice++)
    if (z > slice->elevation)
      {
        botz = slice->elevation;
        botr = slice->radius;
        break;
      }
  if (botz == -999) return 0;

  ratio = (z - botz) / (topz - botz);

  return (botr + ((topr - botr) * ratio));
}


static void
move_ball (ModeInfo *mi, metaball *b)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  double gravity = GRAVITY;
  double real_r;

  if (b->static_p) return;

  b->pos_r += b->dr;
  b->z     += b->dz;

  b->dz -= gravity;

  if (b->pos_r > 0.9)
    {
      b->pos_r = 0.9;
      b->dr = -b->dr;
    }
  else if (b->pos_r < 0)
    {
      b->pos_r = -b->pos_r;
      b->dr = -b->dr;
    }

  real_r = b->pos_r * bottle_radius_at (bp, b->z);

  b->x = cos (b->pos_th) * real_r;
  b->y = sin (b->pos_th) * real_r;

  if (b->z < -b->R)  /* dropped below bottom of glass - turn it off */
    b->alive_p = False;
}


/* This function makes sure that balls that are part of a group always stay
   relatively close to each other.
 */
static void
clamp_balls (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < bp->nballs; i++)
    {
      metaball *b = &bp->balls[i];
      if (b->alive_p && b->leader)
        {
          double zslack = 0.1;
          double minz = b->leader->z - zslack;
          double maxz = b->leader->z + zslack;

          /* Try to keep the Z values near those of the leader.
             Don't let it go out of range (above or below) and clamp it
             if it does.  If we've clamped it, make sure dz will be
             moving it in the right direction (back toward the leader.)

             We aren't currently clamping r, only z -- doesn't seem to
             be needed.

             This is kind of flaky, I think.  Sometimes you can see
             the blobbies "twitch".  That's no good.
           */

          if (b->z < minz)
            {
              if (b->dz < 0) b->dz = -b->dz;
              b->z = minz - b->dz;
            }

          if (b->z > maxz)
            {
              if (b->dz > 0) b->dz = -b->dz;
              b->z = maxz + b->dz;
            }
        }
    }
}


static void
move_balls (ModeInfo *mi)   /* for great justice */
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < bp->nballs; i++)
    {
      metaball *b = &bp->balls[i];
      if (b->alive_p)
        move_ball (mi, b);
    }

  clamp_balls (mi);
}



/* Rendering blobbies using marching cubes.
 */

static double
compute_metaball_influence (lavalite_configuration *bp,
                            double x, double y, double z,
                            int nballs, metaball *balls)
{
  double vv = 0;
  int i;

  for (i = 0; i < nballs; i++)
    {
      metaball *b = &balls[i];
      double dx, dy, dz;
      double d2, r, R, r2, R2;

      if (!b->alive_p) continue;

      dx = x - b->x;
      dy = y - b->y;
      dz = z - b->z;
      R = b->R;

      if (dx > R || dx < -R ||    /* quick check before multiplying */
          dy > R || dy < -R ||
          dz > R || dz < -R)
        continue;

      d2 = (dx*dx + dy*dy + dz*dz);
      r = b->r;

      r2 = r*r;
      R2 = R*R;

      if (d2 <= r2)             /* (d <= r)   inside the hard radius */
        vv += 1;
      else if (d2 > R2)         /* (d > R)   outside the radius of influence */
        ;
      else          /* somewhere in between: linear drop-off from r=1 to R=0 */
        {
          /* was: vv += 1 - ((d-r) / (R-r)); */
          vv += 1 - ((d2-r2) / (R2-r2));
        }
    }

  return vv;
}


/* callback for marching_cubes() */
static void *
obj_init (double grid_size, void *closure)
{
  lavalite_configuration *bp = (lavalite_configuration *) closure;
  bp->grid_size = grid_size;

  return closure;
}


/* Returns True if the given point is outside of the glass tube.
 */
static double
clipped_by_glass_p (double x, double y, double z,
                    lavalite_configuration *bp)
{
  double d2, or, or2, ir2;

  or = bp->max_bottle_radius;

  if (x > or || x < -or ||    /* quick check before multiplying */
      y > or || y < -or)
    return 0;

  d2 = (x*x + y*y);
  or = bottle_radius_at (bp, z);

  or2 = or*or;

  if (d2 > or2)   /* (sqrt(d) > or) */
    return 0;

  ir2 = or2 * 0.7;

  if (d2 > ir2)  /* (sqrt(d) > ir) */
    {
      double dr1 = or2;
      double dr2 = ir2;
      /* was: (1 - (d-ratio2) / (ratio1-ratio2)) */
      return (1 - (d2-dr2) / (dr1-dr2));
    }

  return 1;
}



/* callback for marching_cubes() */
static double
obj_compute (double x, double y, double z, void *closure)
{
  lavalite_configuration *bp = (lavalite_configuration *) closure;
  double clip;

  x /= bp->grid_size;	/* convert from 0-N to 0-1. */
  y /= bp->grid_size;
  z /= bp->grid_size;

  x -= 0.5;	/* X and Y range from -.5 to +.5; z ranges from 0-1. */
  y -= 0.5;

  clip = clipped_by_glass_p (x, y, z, bp);
  if (clip == 0) return 0;

  return (clip *
          compute_metaball_influence (bp, x, y, z, bp->nballs, bp->balls));
}


/* callback for marching_cubes() */
static void
obj_free (void *closure)
{
}


/* Send a new blob travelling upward.
   This blob will actually be composed of N metaballs that are near
   each other.
 */
static void
launch_balls (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  metaball *b0 = get_ball (mi);
  int i;

  if (!b0) return;
  reset_ball (mi, b0);

  for (i = 0; i < bp->blobs_per_group; i++)
    {
      metaball *b1 = get_ball (mi);
      if (!b1) break;
      *b1 = *b0;

      reset_ball (mi, b1);
      b1->leader = b0;

# define FROB(FIELD,AMT) \
         b1->FIELD += (bellrand(AMT) * b0->FIELD)

   /* FROB (pos_r,  0.7); */
   /* FROB (pos_th, 0.7); */
      FROB (dr, 0.8);
      FROB (dz, 0.6);
# undef FROB
    }

}


static void
animate_lava (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Bool just_started_p = bp->just_started_p;

  double isolevel = 0.3;

  /* Maybe bubble a new blobby to the surface.
   */
  if (just_started_p ||
      frand(1.0) < bp->launch_chance)
    {
      bp->just_started_p = False;
      launch_balls (mi);

      if (do_impatient && just_started_p)
        while (1)
          {
            int i;
            move_balls (mi);
            for (i = 0; i < bp->nballs; i++)
              {
                metaball *b = &bp->balls[i];
                if (b->alive_p && !b->static_p && !b->leader &&
                    b->z > 0.5)
                  goto DONE;
              }
          }
      DONE: ;
    }

  move_balls (mi);

  glNewList (bp->ball_list, GL_COMPILE);
  glPushMatrix();

  glMaterialfv (GL_FRONT, GL_SPECULAR,            lava_spec);
  glMateriali  (GL_FRONT, GL_SHININESS,           lava_shininess);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, lava_color);

  /* For the blobbies, the origin is on the axis at the bottom of the
     glass bottle; and the top of the bottle is +1 on Z.
   */
  glTranslatef (0, 0, -0.5);

  mi->polygon_count = 0;
  {
    double s;
    if (bp->grid_size == 0) bp->grid_size = 1;  /* first time through */
    s = 1.0/bp->grid_size;

    glPushMatrix();
    glTranslatef (-0.5, -0.5, 0);
    glScalef (s, s, s);
    marching_cubes (resolution, isolevel, wire, do_smooth,
                    obj_init, obj_compute, obj_free, bp,
                    &mi->polygon_count);
    glPopMatrix();
  }

  mi->polygon_count += bp->bottle_poly_count;

  glPopMatrix();
  glEndList ();
}



/* Startup initialization
 */

Bool
lavalite_handle_event (ModeInfo *mi, XEvent *event)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      gltrackball_track (bp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
parse_color (ModeInfo *mi, const char *name, const char *s, GLfloat *a)
{
  XColor c;
  a[4] = 1.0;  /* alpha */

  if (! XParseColor (MI_DISPLAY(mi), MI_COLORMAP(mi), s, &c))
    {
      fprintf (stderr, "%s: can't parse %s color %s", progname, name, s);
      exit (1);
    }
  a[0] = c.red   / 65536.0;
  a[1] = c.green / 65536.0;
  a[2] = c.blue  / 65536.0;
}


void 
init_lavalite (ModeInfo *mi)
{
  lavalite_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (lavalite_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (lavalite_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_lavalite (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    char *s = do_style;
    if (!s || !*s || !strcasecmp (s, "classic")) bp->style = CLASSIC;
    else if (!strcasecmp (s, "giant"))  bp->style = GIANT;
    else if (!strcasecmp (s, "cone"))   bp->style = CONE;
    else if (!strcasecmp (s, "rocket")) bp->style = ROCKET;
    else if (!strcasecmp (s, "random"))
      {
        if (random() & 1) bp->style = CLASSIC;  /* half the time */
        else bp->style = (random() % ((int) ROCKET+1));
      }
    else
      {
        fprintf (stderr,
         "%s: style must be Classic, Giant, Cone, or Rocket (not \"%s\")\n",
                 progname, s);
        exit (1);
      }
  }

  parse_color (mi, "lava",  lava_color_str,  lava_color);
  parse_color (mi, "fluid", fluid_color_str, fluid_color);
  parse_color (mi, "base",  base_color_str,  base_color);
  parse_color (mi, "table", table_color_str, table_color);

  if (!wire)
    {
      GLfloat amb[4]  = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4]  = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc0[4] = {0.0, 1.0, 1.0, 1.0};
      GLfloat spc1[4] = {1.0, 0.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHT1);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
      glEnable(GL_NORMALIZE);
      glShadeModel(GL_SMOOTH);

      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc0);

      glLightfv(GL_LIGHT1, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT1, GL_SPECULAR, spc1);

      glLightfv(GL_LIGHT2, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT2, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT2, GL_SPECULAR, spc0);
    }

  {
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 0.4;
    double wander_speed = 0.03;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    bp->rot = make_rotator (spinx ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            1.0,
                            do_wander ? wander_speed : 0,
                            False);
    bp->rot2 = make_rotator (spin_speed, 0, 0,
                             1, 0.1,
                             False);
    bp->trackball = gltrackball_init ();

    /* move initial camera position up by around 15 degrees:
       in other words, tilt the scene toward the viewer. */
    gltrackball_start (bp->trackball, 50, 50, 100, 100);
    gltrackball_track (bp->trackball, 50,  5, 100, 100);

    /* Oh, but if it's the "Giant" model, tilt the scene away: make it
       look like we're looking up at it instead of odwn at it! */
    if (bp->style == GIANT)
      gltrackball_track (bp->trackball, 50, -12, 100, 100);
    else if (bp->style == ROCKET)  /* same for rocket, but not as much */
      gltrackball_track (bp->trackball, 50, -4, 100, 100);
  }

  switch (bp->style)
    {
    case CLASSIC: bp->model = classic_lamp; break;
    case GIANT:   bp->model = giant_lamp;   break;
    case CONE:    bp->model = cone_lamp;    break;
    case ROCKET:  bp->model = rocket_lamp;  break;
    default: abort(); break;
    }

  bp->max_bottle_radius = max_bottle_radius (bp);

  bp->launch_chance = speed;
  bp->blobs_per_group = BLOBS_PER_GROUP;
  bp->just_started_p = True;

  bp->nballs = (((MI_COUNT (mi) + 1) * bp->blobs_per_group)
                + 2);
  bp->balls = (metaball *) calloc (sizeof(*bp->balls), bp->nballs+1);

  bp->bottle_list = glGenLists (1);
  bp->ball_list = glGenLists (1);

  generate_bottle (mi);
  generate_static_blobs (mi);
}


/* Render one frame
 */

void
draw_lavalite (ModeInfo *mi)
{
  lavalite_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glMatrixMode (GL_MODELVIEW);
  glPushMatrix ();

  {
    double cx, cy, cz; /* camera position, 0-1. */
    double px, py, pz; /* object position, 0-1. */
    double rx, ry, rz; /* object rotation, 0-1. */

    get_position (bp->rot2, 0,   &cy, &cz, !bp->button_down_p);
    get_rotation (bp->rot2, &cx, 0,   0,   !bp->button_down_p);

    get_position (bp->rot,  &px, &py, &pz, !bp->button_down_p);
    get_rotation (bp->rot,  &rx, &ry, &rz, !bp->button_down_p);

#if 1
    cx = 0.5;
    cy = 0.5;
    cz = 1.0;

#else  /* #### this crud doesn't really work yet */


    /* We have c[xyz] parameters describing a camera position, but we don't
       want to just map those to points in space: the lamp doesn't look very
       good from the inside, or from underneath...

       Good observation points form a ring around the lamp: basically, a
       torus ringing the lamp, parallel to the lamp's floor.

       We interpret cz as distance from the origin.
       cy as elevation.
       cx is then used as position in the torus (theta).
     */

    {
      double cx2, cy2, cz2;
      double d;

      cx2 = 0.5;
      cy2 = 0.5;
      cz2 = 1.0;

      cy2 = (cy * 0.4);		/* cam elevation: 0.0 (table) - 0.4 up. */
      d = 0.9 + cz;		/* cam distance: 0.9 - 1.9. */

      cz2 = 0.5 + (d * cos (cx * M_PI * 2));
      cx2 = 0.5 + (d * sin (cx * M_PI * 2));


      cx = cx2;
      cy = cy2;
      cz = cz2;
    }
#endif  /* 0 */

    glLoadIdentity();

    gluLookAt ((cx - 0.5) * 8,		/* Position the camera */
               (cy - 0.5) * 8,
               (cz - 0.5) * 8,
               0, 0, 0,
               0, 1, 0);

    gltrackball_rotate (bp->trackball);	/* Apply mouse-based camera position */


    /* Place the lights relative to the object, before the object has
       been rotated or wandered within the scene. */
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT2, GL_POSITION, light2_pos);


    /* Position the lamp in the scene according to the "wander" settings */
    glTranslatef ((px - 0.5), (py - 0.5), (pz - 0.5));

    /* Rotate the object according to the "spin" settings */
    glRotatef (rx * 360, 1.0, 0.0, 0.0);
    glRotatef (ry * 360, 0.0, 1.0, 0.0);
    glRotatef (rz * 360, 0.0, 0.0, 1.0);

    /* Move the lamp up slightly: make 0,0 be at its vertical center. */
    switch (bp->style)
      {
      case CLASSIC: glTranslatef (0, 0, 0.33); break;
      case GIANT:   glTranslatef (0, 0, 0.33); break;
      case CONE:    glTranslatef (0, 0, 0.16); break;
      case ROCKET:  glTranslatef (0, 0, 0.30);
                    glScalef (0.85,0.85,0.85); break;
      default: abort(); break;
      }
  }

  animate_lava (mi);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glCallList (bp->bottle_list);
  glCallList (bp->ball_list);
  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
