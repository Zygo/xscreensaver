/* menger, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Generates a 3D Menger Sponge gasket:
 *
 *                                ___+._______
 *                           __-""   --     __"""----._____
 *                    __.--"" -- ___--+---_____.     __  .+'|
 *              _.-'""  __    +:"__   | ._..+"" __    .+'   F
 *            J"--.____         __ """""+"         .+'  .J  F
 *            J        """""---.___       --   .+'"     F'  F
 *             L                   """""--...+'    .J       F
 *             L   F"9      --.            |   .   F'      J
 *             L   -_J      L_J      F"9   | ;'J    .+J .J J
 *             |                     L_J   | F.'  .'| J F' J
 *             |        |"""--.__          | '   |""  J    J
 *             J   ._   J ;;; |  "L        |   . |-___J    |
 *             J   L J  J ;-' |   L        | .'J |_  .'  . |
 *             J    ""  J    .---_L  F"9   | F.' | .'   FJ |
 *              L       J .-'  __ |  L_J   | '   :'     ' .+
 *              L       '--.___   |        |       .J   .'
 *              |  F"9         """'        |   .   F' .'
 *              |  -_J      F"9            | .'J    .'
 *              +__         -_J      F"9   | F.'  .'
 *                 """--___          L_J   | '  .'
 *                         """---___       |  .'
 *                                  ""---._|.'
 *
 *  The straightforward way to generate this object creates way more polygons
 *  than are needed, since there end up being many buried, interior faces.
 *  So first we go through and generate the list of all the squares; then we
 *  sort the list and delete any squares that have a duplicate: if there are
 *  two of them, then they will be interior and facing each other, so we
 *  don't need either.  Doing this reduces the polygon count by 20% - 33%.
 *
 *  Another optimization we could do to reduce the polygon count would be to
 *  merge adjascent coplanar squares together into rectangles.  This would
 *  result in the outer faces being composed of 1xN strips.  It's tricky to
 *  to find these adjascent faces in non-exponential time, though.
 *
 *  We could take advantage of the object's triple symmetry to reduce the
 *  size of the `squares' array, though that wouldn't actually reduce the
 *  number of polygons in the scene.
 *
 *  We could actually simulate large depths with a texture map -- if the
 *  depth is such that the smallest holes are only a few pixels across,
 *  just draw them as spots on the surface!  It would look the same.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"Menger"
#define HACK_INIT	init_sponge
#define HACK_DRAW	draw_sponge
#define HACK_RESHAPE	reshape_sponge
#define sws_opts	xlockmore_opts

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "150"
#define DEF_MAX_DEPTH   "3"
#define DEF_OPTIMIZE    "True"

#define DEFAULTS	"*delay:	 30000          \n" \
			"*showFPS:       False          \n" \
			"*wireframe:     False          \n" \
			"*maxDepth:    " DEF_MAX_DEPTH "\n" \
			"*speed:"        DEF_SPEED     "\n" \
			"*optimize:"     DEF_OPTIMIZE  "\n" \
			"*spin:        " DEF_SPIN      "\n" \
			"*wander:      " DEF_WANDER    "\n" \


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

typedef struct {
  unsigned int x0 : 8;     /* 8     bottom left */
  unsigned int y0 : 8;     /* 16                */
  unsigned int z0 : 8;     /* 24                */

  unsigned int x1 : 8;     /* 32    top right   */
  unsigned int y1 : 8;     /* 40                */
  unsigned int z1 : 8;     /* 48                */
  
  int          nx : 2;     /* 50    normal      */
  int          ny : 2;     /* 52                */
  int          nz : 2;     /* 54                */

                           /* 2 bits left over; 56 bits / 7 bytes total, */
                           /* which is surely rounded up to 8 bytes.     */
} square;


typedef struct {
  GLXContext *glx_context;

  GLfloat rotx, roty, rotz;	   /* current object rotation */
  GLfloat dx, dy, dz;		   /* current rotational velocity */
  GLfloat ddx, ddy, ddz;	   /* current rotational acceleration */
  GLfloat d_max;		   /* max velocity */

  GLuint sponge_list0;            /* we store X, Y, and Z-facing surfaces */
  GLuint sponge_list1;            /* in their own lists, to make it easy  */
  GLuint sponge_list2;            /* to color them differently.           */

  square *squares;
  unsigned long squares_size;
  unsigned long squares_fp;

  int current_depth;

  int ncolors;
  XColor *colors;
  int ccolor0;
  int ccolor1;
  int ccolor2;

} sponge_configuration;

static sponge_configuration *sps = NULL;

static char *do_spin;
static Bool do_wander;
static int speed;
static Bool do_optimize;
static int max_depth;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-optimize", ".optimize", XrmoptionNoArg, "True" },
  { "+optimize", ".optimize", XrmoptionNoArg, "False" },
  {"-depth",   ".maxDepth", XrmoptionSepArg, (caddr_t) 0 },
};

static argtype vars[] = {
  {(caddr_t *) &do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {(caddr_t *) &do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {(caddr_t *) &speed,     "speed",  "Speed",  DEF_SPEED,  t_Int},
  {(caddr_t *) &do_optimize, "optimize", "Optimize", DEF_OPTIMIZE, t_Bool},
  {(caddr_t *) &max_depth, "maxDepth", "MaxDepth", DEF_MAX_DEPTH, t_Int},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
void
reshape_sponge (ModeInfo *mi, int width, int height)
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


static void
gl_init (ModeInfo *mi)
{
/*  sponge_configuration *sp = &sps[MI_SCREEN(mi)]; */
  int wire = MI_IS_WIREFRAME(mi);

  static GLfloat pos[4] = {-4.0, 3.0, 10.0, 1.0};

  if (!wire)
    {
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glEnable(GL_CULL_FACE);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
    }
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


/* Pushes a 1x1x1 cube at XYZ into the sp->squares list.
 */
static void
cube (sponge_configuration *sp, Bool wireframe_p,
      int x, int y, int z, int s)
{
  square *sq;

# define PP(NX, NY, NZ, X0, Y0, Z0, X1, Y1, Z1) \
         (sq = &sp->squares[sp->squares_fp++], \
          sq->nx = (NX), \
          sq->ny = (NY), \
          sq->nz = (NZ), \
          sq->x0 = x+((X0)*s), \
          sq->y0 = y+((Y0)*s), \
          sq->z0 = z+((Z0)*s), \
          sq->x1 = x+((X1)*s), \
          sq->y1 = y+((Y1)*s), \
          sq->z1 = z+((Z1)*s))

  PP (0,  0, -1,   0, 1, 0,   1, 0, 0);
  PP (0,  0,  1,   0, 0, 1,   1, 1, 1);
  PP (0, -1,  0,   0, 0, 0,   1, 0, 1);
  PP (0,  1,  0,   0, 1, 1,   1, 1, 0);

  if (wireframe_p) return;  /* don't need the rest */

  PP (-1, 0,  0,   0, 0, 1,   0, 1, 0);
  PP ( 1, 0,  0,   1, 0, 0,   1, 1, 1);
# undef PP

  if (sp->squares_fp >= sp->squares_size) abort();
}


static int
iexp (int i, int n)
{
  int ii = 1;
  while (n-- > 0) ii *= i;
  return ii;
}


static void
descend_cubes (sponge_configuration *sp, Bool wireframe_p, int countdown,
               int x, int y, int z)
{
  int descend_p = (countdown > 0);
  int facet_size = iexp (3, countdown);
  int s = facet_size;

  if (wireframe_p)
    {
      cube (sp, wireframe_p, x+1*s, y+1*s, z,     1);
      cube (sp, wireframe_p, x+1*s, y+1*s, z+2*s, 1);
      cube (sp, wireframe_p, x,     y+1*s, z+1*s, 1);
      cube (sp, wireframe_p, x+2*s, y+1*s, z+1*s, 1);
      cube (sp, wireframe_p, x+1*s, y,     z+1*s, 1);
      cube (sp, wireframe_p, x+1*s, y+2*s, z+1*s, 1);

      if (!descend_p) return;
    }

# define CUBE(xx,yy,zz) \
         (descend_p \
          ? descend_cubes (sp, wireframe_p, countdown-1, \
                           x+(xx)*s, y+(yy)*s, z+(zz)*s) \
          : cube (sp, wireframe_p, x+(xx)*s, y+(yy)*s, z+(zz)*s, 1))

  CUBE(0, 2, 0); CUBE(1, 2, 0); CUBE(2, 2, 0);   /* top front row */
  CUBE(0, 1, 0);                CUBE(2, 1, 0);   /* middle front row */
  CUBE(0, 0, 0); CUBE(1, 0, 0); CUBE(2, 0, 0);   /* bottom front row */
  CUBE(0, 2, 1);                CUBE(2, 2, 1);   /* top middle row */
  CUBE(0, 0, 1);                CUBE(2, 0, 1);   /* bottom middle row */
  CUBE(0, 2, 2); CUBE(1, 2, 2); CUBE(2, 2, 2);   /* top back row */
  CUBE(0, 1, 2);                CUBE(2, 1, 2);   /* middle back row */
  CUBE(0, 0, 2); CUBE(1, 0, 2); CUBE(2, 0, 2);   /* bottom back row */
# undef CUBE
}


static int cmp_squares (const void *a, const void *b);
static void delete_redundant_faces (sponge_configuration *);

static void
build_sponge (sponge_configuration *sp, Bool wireframe_p, int countdown)
{
  sp->squares_fp = 0;

  if (countdown <= 0)
    {
      cube (sp, wireframe_p, 0, 0, 0, 1);
    }
  else
    {
      if (wireframe_p)
        {
          int facet_size = iexp(3, countdown);
          cube (sp, wireframe_p, 0, 0, 0, facet_size);
        }
      descend_cubes (sp, wireframe_p, countdown - 1,
                     0, 0, 0);
    }

  if (!wireframe_p && do_optimize)
    {
      qsort (sp->squares, sp->squares_fp, sizeof(*sp->squares),
             cmp_squares);
      delete_redundant_faces (sp);
    }

  {
    int i, j;
    square *sq;
    GLfloat s = 1.0 / iexp (3, countdown);
    s *= 3;

    glDeleteLists (sp->sponge_list0, 1);
    glDeleteLists (sp->sponge_list1, 1);
    glDeleteLists (sp->sponge_list2, 1);

    for (j = 0; j < 3; j++)
      {
        sq = sp->squares;
        glNewList ((j == 0 ? sp->sponge_list0 :
                    j == 1 ? sp->sponge_list1 :
                    sp->sponge_list2),
                   GL_COMPILE);
        glPushMatrix();
        glTranslatef (-1.5, -1.5, -1.5);
        glScalef(s, s, s);

        for (i = 0; i < sp->squares_fp; i++)
          {
            if ((j == 0 && sq->nx != 0) ||
                (j == 1 && sq->ny != 0) ||
                (j == 2 && sq->nz != 0))
              {
                glBegin (wireframe_p ? GL_LINE_LOOP : GL_QUADS);
                glNormal3i (sq->nx, sq->ny, sq->nz);
                if (sq->nz)
                  {
                    glVertex3i (sq->x1, sq->y0, sq->z0);
                    glVertex3i (sq->x1, sq->y1, sq->z0);
                    glVertex3i (sq->x0, sq->y1, sq->z0);
                    glVertex3i (sq->x0, sq->y0, sq->z0);
                  }
                else if (sq->ny)
                  {
                    glVertex3i (sq->x1, sq->y0, sq->z0);
                    glVertex3i (sq->x1, sq->y0, sq->z1);
                    glVertex3i (sq->x0, sq->y0, sq->z1);
                    glVertex3i (sq->x0, sq->y0, sq->z0);
                  }
                else
                  {
                    glVertex3i (sq->x0, sq->y1, sq->z0);
                    glVertex3i (sq->x0, sq->y1, sq->z1);
                    glVertex3i (sq->x0, sq->y0, sq->z1);
                    glVertex3i (sq->x0, sq->y0, sq->z0);
                  }
                glEnd();
              }
            sq++;
          }
        glPopMatrix();
        glEndList();
      }
  }
}


static int
cmp_squares (const void *aa, const void *bb)
{
  square *a = (square *) aa;
  square *b = (square *) bb;
  int i, p0, p1;

  p0 = (a->x0 < a->x1 ? a->x0 : a->x1);
  p1 = (b->x0 < b->x1 ? b->x0 : b->x1);
  if ((i = (p0 - p1))) return i;

  p0 = (a->y0 < a->y1 ? a->y0 : a->y1);
  p1 = (b->y0 < b->y1 ? b->y0 : b->y1);
  if ((i = (p0 - p1))) return i;

  p0 = (a->z0 < a->z1 ? a->z0 : a->z1);
  p1 = (b->z0 < b->z1 ? b->z0 : b->z1);
  if ((i = (p0 - p1))) return i;


  p0 = (a->x0 > a->x1 ? a->x0 : a->x1);
  p1 = (b->x0 > b->x1 ? b->x0 : b->x1);
  if ((i = (p0 - p1))) return i;

  p0 = (a->y0 > a->y1 ? a->y0 : a->y1);
  p1 = (b->y0 > b->y1 ? b->y0 : b->y1);
  if ((i = (p0 - p1))) return i;

  p0 = (a->z0 > a->z1 ? a->z0 : a->z1);
  p1 = (b->z0 > b->z1 ? b->z0 : b->z1);
  if ((i = (p0 - p1))) return i;

  return 0;

}


static void
delete_redundant_faces (sponge_configuration *sp)
{
  square *sq = sp->squares;
  square *end = sq + sp->squares_fp;
  square *out = sq;
  int i = 0;

  while (sq < end)
    {
      if (cmp_squares (sq, sq+1)) /* they differ - keep this one */
        {
          if (sq != out)
            *out = *sq;
          out++;
          i++;
        }
      sq++;
    }

# if 0
  fprintf (stderr, "%s: optimized away %d polygons (%d%%)\n",
           progname,
           sp->squares_fp - i,
           100 - ((i * 100) / sp->squares_fp));
# endif

  sp->squares_fp = i;
}


void 
init_sponge (ModeInfo *mi)
{
  sponge_configuration *sp;

  if (!sps) {
    sps = (sponge_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (sponge_configuration));
    if (!sps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    sp = &sps[MI_SCREEN(mi)];
  }

  sp = &sps[MI_SCREEN(mi)];

  if ((sp->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    reshape_sponge (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  sp->rotx = frand(1.0) * RANDSIGN();
  sp->roty = frand(1.0) * RANDSIGN();
  sp->rotz = frand(1.0) * RANDSIGN();

  /* bell curve from 0-3 degrees, avg 1.5 */
  sp->dx = (frand(1) + frand(1) + frand(1)) / (360/2);
  sp->dy = (frand(1) + frand(1) + frand(1)) / (360/2);
  sp->dz = (frand(1) + frand(1) + frand(1)) / (360/2);

  sp->d_max = sp->dx * 2;

  sp->ddx = 0.00006 + frand(0.00003);
  sp->ddy = 0.00006 + frand(0.00003);
  sp->ddz = 0.00006 + frand(0.00003);

  sp->ncolors = 128;
  sp->colors = (XColor *) calloc(sp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        sp->colors, &sp->ncolors,
                        False, 0, False);
  sp->ccolor0 = 0;
  sp->ccolor1 = sp->ncolors / 3;
  sp->ccolor2 = sp->ccolor1 * 2;

  {
    int d = max_depth < 1 ? 1 : max_depth;
    unsigned long facet_size = iexp(3, d);
    unsigned long bytes;
    float be_miserly = 0.741;
    sp->squares_size = iexp(facet_size, 3) * 6 * be_miserly;
    bytes = sp->squares_size * sizeof (*sp->squares);

    sp->squares_fp = 0;
    sp->squares = calloc (1, bytes);

    if (!sp->squares)
      {
        fprintf (stderr, "%s: out of memory", progname);
        if ((sp->squares_size >> 20) > 1)
          fprintf (stderr, " (%luMB for %luM polygons)\n",
                   bytes >> 20, sp->squares_size >> 20);
        else
          fprintf (stderr, " (%luKB for %luK polygons)\n",
                   bytes >> 10, sp->squares_size >> 10);
        exit (1);
      }
  }

  sp->sponge_list0 = glGenLists (1);
  sp->sponge_list1 = glGenLists (1);
  sp->sponge_list2 = glGenLists (1);
}


void
draw_sponge (ModeInfo *mi)
{
  sponge_configuration *sp = &sps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  static GLfloat color0[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat color1[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat color2[4] = {0.0, 0.0, 0.0, 1.0};

  static int tick = 99999;

  if (!sp->glx_context)
    return;

  glShadeModel(GL_SMOOTH);

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

    if (do_spin)
      {
        x = sp->rotx;
        y = sp->roty;
        z = sp->rotz;
        if (x < 0) x = 1 - (x + 1);
        if (y < 0) y = 1 - (y + 1);
        if (z < 0) z = 1 - (z + 1);

        glRotatef(x * 360, 1.0, 0.0, 0.0);
        glRotatef(y * 360, 0.0, 1.0, 0.0);
        glRotatef(z * 360, 0.0, 0.0, 1.0);

        rotate(&sp->rotx, &sp->dx, &sp->ddx, sp->d_max);
        rotate(&sp->roty, &sp->dy, &sp->ddy, sp->d_max);
        rotate(&sp->rotz, &sp->dz, &sp->ddz, sp->d_max);
      }
  }

  color0[0] = sp->colors[sp->ccolor0].red   / 65536.0;
  color0[1] = sp->colors[sp->ccolor0].green / 65536.0;
  color0[2] = sp->colors[sp->ccolor0].blue  / 65536.0;

  color1[0] = sp->colors[sp->ccolor1].red   / 65536.0;
  color1[1] = sp->colors[sp->ccolor1].green / 65536.0;
  color1[2] = sp->colors[sp->ccolor1].blue  / 65536.0;

  color2[0] = sp->colors[sp->ccolor2].red   / 65536.0;
  color2[1] = sp->colors[sp->ccolor2].green / 65536.0;
  color2[2] = sp->colors[sp->ccolor2].blue  / 65536.0;


  sp->ccolor0++;
  sp->ccolor1++;
  sp->ccolor2++;
  if (sp->ccolor0 >= sp->ncolors) sp->ccolor0 = 0;
  if (sp->ccolor1 >= sp->ncolors) sp->ccolor1 = 0;
  if (sp->ccolor2 >= sp->ncolors) sp->ccolor2 = 0;

  if (tick++ >= speed)
    {
      tick = 0;
      if (sp->current_depth >= max_depth)
        sp->current_depth = -max_depth;
      sp->current_depth++;
      build_sponge (sp,
                    MI_IS_WIREFRAME(mi),
                    (sp->current_depth < 0
                     ? -sp->current_depth : sp->current_depth));
    }

  glScalef (2.0, 2.0, 2.0);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color0);
  glCallList (sp->sponge_list0);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
  glCallList (sp->sponge_list1);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
  glCallList (sp->sponge_list2);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
