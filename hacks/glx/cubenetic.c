/* cubenetic, Copyright (c) 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"Cubenetic"
#define HACK_INIT	init_cube
#define HACK_DRAW	draw_cube
#define HACK_RESHAPE	reshape_cube
#define ccs_opts	xlockmore_opts

#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_TEXTURE     "True"

#define DEF_WAVE_COUNT  "3"
#define DEF_WAVE_SPEED  "80"
#define DEF_WAVE_RADIUS "512"

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        5           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \
			"*texture:    " DEF_TEXTURE"\n" \
			"*waves:      " DEF_WAVE_COUNT  "\n" \
			"*waveSpeed:  " DEF_WAVE_SPEED  "\n" \
			"*waveRadius: " DEF_WAVE_RADIUS "\n" \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

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

  GLfloat rotx, roty, rotz;	   /* current object rotation */
  GLfloat dx, dy, dz;		   /* current rotational velocity */
  GLfloat ddx, ddy, ddz;	   /* current rotational acceleration */
  GLfloat d_max;		   /* max velocity */
  Bool spin_x, spin_y, spin_z;

  GLuint cube_list;
  GLuint texture_id;
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
  {"-texture", ".texture", XrmoptionNoArg, (caddr_t) "true" },
  {"+texture", ".texture", XrmoptionNoArg, (caddr_t) "false" },
  {"-waves",       ".waves",      XrmoptionSepArg, 0 },
  {"-wave-speed",  ".waveSpeed",  XrmoptionSepArg, 0 },
  {"-wave-radius", ".waveRadius", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {(caddr_t *) &do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {(caddr_t *) &do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {(caddr_t *) &do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
  {(caddr_t *) &wave_count, "waves",     "Waves",      DEF_WAVE_COUNT, t_Int},
  {(caddr_t *) &wave_speed, "waveSpeed", "WaveSpeed",  DEF_WAVE_SPEED, t_Int},
  {(caddr_t *) &wave_radius,"waveRadius","WaveRadius", DEF_WAVE_RADIUS,t_Int},
};

ModeSpecOpt ccs_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void
unit_cube (Bool wire)
{
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* front */
  glNormal3f (0, 0, 1);
  glTexCoord2f(1, 0); glVertex3f ( 0.5, -0.5,  0.5);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5,  0.5,  0.5);
  glTexCoord2f(1, 1); glVertex3f (-0.5, -0.5,  0.5);
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* back */
  glNormal3f (0, 0, -1);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f ( 0.5, -0.5, -0.5);
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* left */
  glNormal3f (-1, 0, 0);
  glTexCoord2f(1, 1); glVertex3f (-0.5,  0.5,  0.5);
  glTexCoord2f(1, 0); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5, -0.5,  0.5);
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* right */
  glNormal3f (1, 0, 0);
  glTexCoord2f(1, 1); glVertex3f ( 0.5, -0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5, -0.5,  0.5);
  glEnd();

  if (wire) return;

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* top */
  glNormal3f (0, 1, 0);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f (-0.5,  0.5,  0.5);
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* bottom */
  glNormal3f (0, -1, 0);
  glTexCoord2f(1, 0); glVertex3f (-0.5, -0.5,  0.5);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5, -0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f ( 0.5, -0.5,  0.5);
  glEnd();
}



/* Window management, etc
 */
void
reshape_cube (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective( 30.0, 1/h, 1.0, 100.0 );
  gluLookAt( 0.0, 0.0, 15.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -15.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

static void
rotate(GLfloat *pos, GLfloat *v, GLfloat *dv, GLfloat max_v)
{
  double ppos = *pos;

  /* tick position */
  if (ppos < 0)
    ppos = -(ppos + *v);
  else
    ppos += *v;

  if (ppos > 1.0)
    ppos -= 1.0;
  else if (ppos < 0)
    ppos += 1.0;

  if (ppos < 0) abort();
  if (ppos > 1.0) abort();
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
  glTexGeni (GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  glTexGeni (GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  check_gl_error("texture initialization");

  cc->texture_width  = texture_size;
  cc->texture_height = texture_size;

  i = texture_size * texture_size * 4;
  cc->texture = (char *) malloc (i);
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


void 
init_cube (ModeInfo *mi)
{
  int i;
  cube_configuration *cc;
  int wire = MI_IS_WIREFRAME(mi);

  if (!ccs) {
    ccs = (cube_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (cube_configuration));
    if (!ccs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  cc = &ccs[MI_SCREEN(mi)];

  if ((cc->glx_context = init_GL(mi)) != NULL) {
    reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  if (!wire)
    {
      static GLfloat pos[4] = {1.0, 0.5, 1.0, 0.0};
      static GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      static GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
    }


  {
    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') cc->spin_x = 1;
        else if (*s == 'y' || *s == 'Y') cc->spin_y = 1;
        else if (*s == 'z' || *s == 'Z') cc->spin_z = 1;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }
  }

  cc->rotx = frand(1.0) * RANDSIGN();
  cc->roty = frand(1.0) * RANDSIGN();
  cc->rotz = frand(1.0) * RANDSIGN();

  /* bell curve from 0-6 degrees, avg 3 */
  cc->dx = (frand(0.7) + frand(0.7) + frand(0.7)) / (360/2);
  cc->dy = (frand(0.7) + frand(0.7) + frand(0.7)) / (360/2);
  cc->dz = (frand(0.7) + frand(0.7) + frand(0.7)) / (360/2);

  cc->d_max = cc->dx * 2;

  cc->ddx = 0.00006 + frand(0.00003);
  cc->ddy = 0.00006 + frand(0.00003);
  cc->ddz = 0.00006 + frand(0.00003);

  cc->ncolors = 256;
  cc->texture_colors = (XColor *) calloc(cc->ncolors, sizeof(XColor));
  cc->cube_colors    = (XColor *) calloc(cc->ncolors, sizeof(XColor));

  {
    double H[3], S[3], V[3];
    int shift = 60;
    H[0] = frand(360.0); 
    H[1] = ((H[0] + shift) < 360) ? (H[0]+shift) : (H[0] + shift - 360);
    H[2] = ((H[1] + shift) < 360) ? (H[1]+shift) : (H[1] + shift - 360);
    S[0] = S[1] = S[2] = 1.0;
    V[0] = V[1] = V[2] = 1.0;
    make_color_loop(0, 0,
		    H[0], S[0], V[0], 
		    H[1], S[1], V[1], 
		    H[2], S[2], V[2], 
		    cc->texture_colors, &cc->ncolors,
		    False, False);

    make_smooth_colormap (0, 0, 0,
                          cc->cube_colors, &cc->ncolors, 
                          False, 0, False);
  }

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
  unit_cube (wire);
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


void
draw_cube (ModeInfo *mi)
{
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!cc->glx_context)
    return;

  glShadeModel(GL_FLAT);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    GLfloat x, y, z;

    if (do_wander)
      {
        static int frame = 0;

#       define SINOID(SCALE,SIZE) \
        ((((1 + sin((frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

        x = SINOID(0.0071, 8.0);
        y = SINOID(0.0053, 6.0);
        z = SINOID(0.0037, 15.0);
        frame++;
        glTranslatef(x, y, z);
      }

    if (cc->spin_x || cc->spin_y || cc->spin_z)
      {
        x = cc->rotx;
        y = cc->roty;
        z = cc->rotz;
        if (x < 0) x = 1 - (x + 1);
        if (y < 0) y = 1 - (y + 1);
        if (z < 0) z = 1 - (z + 1);

        if (cc->spin_x) glRotatef(x * 360, 1.0, 0.0, 0.0);
        if (cc->spin_y) glRotatef(y * 360, 0.0, 1.0, 0.0);
        if (cc->spin_z) glRotatef(z * 360, 0.0, 0.0, 1.0);

        rotate(&cc->rotx, &cc->dx, &cc->ddx, cc->d_max);
        rotate(&cc->roty, &cc->dy, &cc->ddy, cc->d_max);
        rotate(&cc->rotz, &cc->dz, &cc->ddz, cc->d_max);
      }
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

#endif /* USE_GL */
