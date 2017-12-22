/* cubenetic, Copyright (c) 2002-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*count:        5           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define free_cube 0
# define release_cube 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_TEXTURE     "True"

#define DEF_WAVES       "3"
#define DEF_WAVE_SPEED  "80"
#define DEF_WAVE_RADIUS "512"

typedef struct {
  int color;
  GLfloat x, y, z;
  GLfloat w, h, d;
  int frame;
  GLfloat dx, dy, dz;
  GLfloat dw, dh, dd;
} cube;

typedef struct {
  int x, y;
  double xth, yth;
} wave_src;

typedef struct {
  int nwaves;
  int radius;
  int speed;
  wave_src *srcs;
  int *heights;
} waves;


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint cube_list;
  GLuint texture_id;
  int cube_polys;
  int ncubes;
  cube *cubes;
  waves *waves;

  int texture_width, texture_height;
  unsigned char *texture;

  int ncolors;
  XColor *cube_colors;
  XColor *texture_colors;

} cube_configuration;

static cube_configuration *ccs = NULL;

static char *do_spin;
static Bool do_wander;
static Bool do_texture;

static int wave_count;
static int wave_speed;
static int wave_radius;
static int texture_size = 256;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  {"-texture", ".texture", XrmoptionNoArg, "true" },
  {"+texture", ".texture", XrmoptionNoArg, "false" },
  {"-waves",       ".waves",      XrmoptionSepArg, 0 },
  {"-wave-speed",  ".waveSpeed",  XrmoptionSepArg, 0 },
  {"-wave-radius", ".waveRadius", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
  {&wave_count, "waves",     "Waves",      DEF_WAVES, t_Int},
  {&wave_speed, "waveSpeed", "WaveSpeed",  DEF_WAVE_SPEED, t_Int},
  {&wave_radius,"waveRadius","WaveRadius", DEF_WAVE_RADIUS,t_Int},
};

ENTRYPOINT ModeSpecOpt cube_opts = {countof(opts), opts, countof(vars), vars, NULL};


static int
unit_cube (Bool wire)
{
  int polys = 0;
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* front */
  glNormal3f (0, 0, 1);
  glTexCoord2f(1, 0); glVertex3f ( 0.5, -0.5,  0.5);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5,  0.5,  0.5);
  glTexCoord2f(1, 1); glVertex3f (-0.5, -0.5,  0.5);
  polys++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* back */
  glNormal3f (0, 0, -1);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f ( 0.5, -0.5, -0.5);
  polys++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* left */
  glNormal3f (-1, 0, 0);
  glTexCoord2f(1, 1); glVertex3f (-0.5,  0.5,  0.5);
  glTexCoord2f(1, 0); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5, -0.5,  0.5);
  polys++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* right */
  glNormal3f (1, 0, 0);
  glTexCoord2f(1, 1); glVertex3f ( 0.5, -0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5, -0.5,  0.5);
  polys++;
  glEnd();

  if (wire) return polys;

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* top */
  glNormal3f (0, 1, 0);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f (-0.5,  0.5,  0.5);
  polys++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* bottom */
  glNormal3f (0, -1, 0);
  glTexCoord2f(1, 0); glVertex3f (-0.5, -0.5,  0.5);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5, -0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f ( 0.5, -0.5,  0.5);
  polys++;
  glEnd();
  return polys;
}



/* Window management, etc
 */
ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective (30.0, 1/h, 1.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}



/* Waves.
   Adapted from ../hacks/interference.c by Hannu Mallat.
 */

static void
init_wave (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  waves *ww;
  int i;
  cc->waves = ww = (waves *) calloc (sizeof(*cc->waves), 1);
  ww->nwaves = wave_count;
  ww->radius = wave_radius;
  ww->speed  = wave_speed;
  ww->heights = (int *) calloc (sizeof(*ww->heights), ww->radius);
  ww->srcs = (wave_src *) calloc (sizeof(*ww->srcs), ww->nwaves);

  for (i = 0; i < ww->radius; i++)
    {
      float max = (cc->ncolors * (ww->radius - i) / (float) ww->radius);
      ww->heights[i] = ((max + max * cos(i / 50.0)) / 2.0);
    }

  for (i = 0; i < ww->nwaves; i++)
    {
      ww->srcs[i].xth = frand(2.0) * M_PI;
      ww->srcs[i].yth = frand(2.0) * M_PI;
    }
}

static void
interference (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  waves *ww = cc->waves;
  int x, y, i;

  /* Move the wave origins around
   */
  for (i = 0; i < ww->nwaves; i++)
    {
      ww->srcs[i].xth += (ww->speed / 1000.0);
      if (ww->srcs[i].xth > 2*M_PI)
        ww->srcs[i].xth -= 2*M_PI;
      ww->srcs[i].yth += (ww->speed / 1000.0);
      if (ww->srcs[i].yth > 2*M_PI)
        ww->srcs[i].yth -= 2*M_PI;

      ww->srcs[i].x = (cc->texture_width/2 +
                       (cos (ww->srcs[i].xth) *
                        cc->texture_width / 2));
      ww->srcs[i].y = (cc->texture_height/2 +
                       (cos (ww->srcs[i].yth) *
                        cc->texture_height / 2));
    }

  /* Compute the effect of the waves on each pixel,
     and generate the output map.
   */
  for (y = 0; y < cc->texture_height; y++)
    for (x = 0; x < cc->texture_width; x++)
      {
        int result = 0;
        unsigned char *o;
        for (i = 0; i < ww->nwaves; i++)
          {
            int dx = x - ww->srcs[i].x;
            int dy = y - ww->srcs[i].y;
            int dist = sqrt (dx*dx + dy*dy);
            result += (dist > ww->radius ? 0 : ww->heights[dist]);
          }
        result %= cc->ncolors;

        o = cc->texture + (((y * cc->texture_width) + x) << 2);
        o[0] = (cc->texture_colors[result].red   >> 8);
        o[1] = (cc->texture_colors[result].green >> 8);
        o[2] = (cc->texture_colors[result].blue  >> 8);
     /* o[3] = 0xFF; */
      }
}


/* Textures
 */

static void
init_texture (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  int i;

  glEnable(GL_TEXTURE_2D);

  clear_gl_error();
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glGenTextures (1, &cc->texture_id);
  glBindTexture (GL_TEXTURE_2D, cc->texture_id);
  check_gl_error("texture binding");

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  check_gl_error("texture initialization");

  cc->texture_width  = texture_size;
  cc->texture_height = texture_size;

  i = texture_size * texture_size * 4;
  cc->texture = (unsigned char *) malloc (i);
  memset (cc->texture, 0xFF, i);
}


static void
shuffle_texture (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  interference (mi);
  clear_gl_error();
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                cc->texture_width, cc->texture_height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE,
                cc->texture);
  check_gl_error("texture");
}


static void
reset_colors (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  double H[3], S[3], V[3];
  int shift = 60;
  H[0] = frand(360.0); 
  H[1] = ((H[0] + shift) < 360) ? (H[0]+shift) : (H[0] + shift - 360);
  H[2] = ((H[1] + shift) < 360) ? (H[1]+shift) : (H[1] + shift - 360);
  S[0] = S[1] = S[2] = 1.0;
  V[0] = V[1] = V[2] = 1.0;
  make_color_loop(0, 0, 0,
                  H[0], S[0], V[0], 
                  H[1], S[1], V[1], 
                  H[2], S[2], V[2], 
                  cc->texture_colors, &cc->ncolors,
                  False, False);

  make_smooth_colormap (0, 0, 0,
                        cc->cube_colors, &cc->ncolors, 
                        False, 0, False);
}


ENTRYPOINT Bool
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, cc->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &cc->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      reset_colors (mi);
      return True;
    }

  return False;
}


ENTRYPOINT void 
init_cube (ModeInfo *mi)
{
  int i;
  cube_configuration *cc;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, ccs);

  cc = &ccs[MI_SCREEN(mi)];

  if ((cc->glx_context = init_GL(mi)) != NULL) {
    reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  if (!wire)
    {
      static const GLfloat pos[4] = {1.0, 0.5, 1.0, 0.0};
      static const GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      static const GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
    }


  {
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 1.0;
    double wander_speed = 0.05;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else if (*s == '0') ;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    cc->rot = make_rotator (spinx ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            1.0,
                            do_wander ? wander_speed : 0,
                            (spinx && spiny && spinz));
    cc->trackball = gltrackball_init (True);
  }

  cc->ncolors = 256;
  cc->texture_colors = (XColor *) calloc(cc->ncolors, sizeof(XColor));
  cc->cube_colors    = (XColor *) calloc(cc->ncolors, sizeof(XColor));

  reset_colors (mi);

  cc->ncubes = MI_COUNT (mi);
  cc->cubes = (cube *) calloc (sizeof(cube), cc->ncubes);
  for (i = 0; i < cc->ncubes; i++)
    {
      cube *cube = &cc->cubes[i];
      cube->color = random() % cc->ncolors;
      cube->w = 1.0;
      cube->h = 1.0;
      cube->d = 1.0;
      cube->dx = frand(0.1);
      cube->dy = frand(0.1);
      cube->dz = frand(0.1);
      cube->dw = frand(0.1);
      cube->dh = frand(0.1);
      cube->dd = frand(0.1);
    }

  if (wire)
    do_texture = False;
    
  if (do_texture)
    {
      init_texture (mi);
      init_wave (mi);
      shuffle_texture (mi);
    }

  cc->cube_list = glGenLists (1);
  glNewList (cc->cube_list, GL_COMPILE);
  cc->cube_polys = unit_cube (wire);
  glEndList ();
}


static void
shuffle_cubes (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < cc->ncubes; i++)
    {
#     define SINOID(SCALE,FRAME,SIZE) \
        ((((1 + sin((FRAME * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

      cube *cube = &cc->cubes[i];
      cube->x = SINOID(cube->dx, cube->frame, 0.5);
      cube->y = SINOID(cube->dy, cube->frame, 0.5);
      cube->z = SINOID(cube->dz, cube->frame, 0.5);
      cube->w = SINOID(cube->dw, cube->frame, 0.9) + 1.0;
      cube->h = SINOID(cube->dh, cube->frame, 0.9) + 1.0;
      cube->d = SINOID(cube->dd, cube->frame, 0.9) + 1.0;
      cube->frame++;
#     undef SINOID
    }
}


ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!cc->glx_context)
    return;

  mi->polygon_count = 0;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cc->glx_context));

  glShadeModel(GL_FLAT);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (cc->rot, &x, &y, &z, !cc->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 6,
                 (z - 0.5) * 15);

    gltrackball_rotate (cc->trackball);

    get_rotation (cc->rot, &x, &y, &z, !cc->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  glScalef (2.5, 2.5, 2.5);

  for (i = 0; i < cc->ncubes; i++)
    {
      cube *cube = &cc->cubes[i];
      GLfloat color[4];
      color[0] = cc->cube_colors[cube->color].red   / 65536.0;
      color[1] = cc->cube_colors[cube->color].green / 65536.0;
      color[2] = cc->cube_colors[cube->color].blue  / 65536.0;
      color[3] = 1.0;
      cube->color++;
      if (cube->color >= cc->ncolors) cube->color = 0;

      glPushMatrix ();
      glTranslatef (cube->x, cube->y, cube->z);
      glScalef (cube->w, cube->h, cube->d);
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
      glCallList (cc->cube_list);
      mi->polygon_count += cc->cube_polys;
      glPopMatrix ();
    }

  shuffle_cubes (mi);
  if (do_texture)
    shuffle_texture (mi);

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("Cubenetic", cubenetic, cube)

#endif /* USE_GL */
