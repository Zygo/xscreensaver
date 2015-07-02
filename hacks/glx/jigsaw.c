/* xscreensaver, Copyright (c) 1997-2015 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Written as an Xlib program some time in 1997.
 * Rewritten as an OpenGL program 24-Aug-2008.
 */

/*
  Currently, we do this:

    - start pieces off screen and move them in.
    - when they land, they fill the puzzle grid with a shuffled
      puzzle (pieces are rotated, too).
    - swap random pairs of pieces until the puzzle is solved.
    - scatter the pieces off screen (resulting in black).
    - load new image and repeat.

  Another idea would be to show the puzzle being solved the way
  a person would do it:

    - start off with black screen.
    - assume knowledge of size of grid (position of corners).
    - find a corner piece, and place it.
    - while puzzle unsolved:
      - pick a random piece;
      - if it is the correct piece for any open edge, place it;
      - if it fits physically in any rotation at any open edge,
        place it, then toss it back (show the fake-out).
      - if it doesn't fit at all, don't animate it at all.

   This would take a long time to solve, I think...

   An even harder idea would involve building up completed "clumps"
   and sliding them around (a coral growth / accretion approach)
 */

#define DEF_SPEED        "1.0"
#define DEF_COMPLEXITY   "1.0"
#define DEF_RESOLUTION   "100"
#define DEF_THICKNESS    "0.06"
#define DEF_WOBBLE       "True"
#define DEF_DEBUG        "False"

#define DEF_FONT "-*-helvetica-bold-r-normal-*-*-240-*-*-*-*-*-*"
#define DEFAULTS  "*delay:		20000	\n" \
		  "*showFPS:		False	\n" \
		  "*font:	     " DEF_FONT"\n" \
		  "*wireframe:		False	\n" \
		  "*desktopGrabber:   xscreensaver-getimage -no-desktop %s\n" \
		  "*grabDesktopImages:	False	\n" \
		  "*chooseRandomImages:	True	\n"


# define refresh_jigsaw 0
# define release_jigsaw 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else
# include <X11/Xlib.h>
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#include "xlockmore.h"
#include "rotator.h"
#include "gltrackball.h"
#include "spline.h"
#include "normals.h"
#include "grab-ximage.h"
#include "texfont.h"

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#else /* !HAVE_JWZGLES */
# define HAVE_TESS
#endif /* !HAVE_JWZGLES */

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#ifdef USE_GL /* whole file */

#define TOP    0
#define RIGHT  1
#define BOTTOM 2
#define LEFT   3

#define IN    -1
#define FLAT   0
#define OUT    1

typedef struct jigsaw_configuration jigsaw_configuration;

typedef struct {
  double x,y,z,r;	/* position and Z rotation (in degrees) */
} XYZR;


typedef struct {
  jigsaw_configuration *jc;
  int edge[4];
  GLuint dlist;
  int polys;

  XYZR home;		/* correct position in puzzle */
  XYZR current;		/* where it is right now */
  XYZR from, to;	/* in transit from A to B */
  double tick;		/* 0-1.0, how far from A to B */
  double arc_height;	/* height of apex of curved path from A to B */
  double tilt, max_tilt;

} puzzle_piece;


struct jigsaw_configuration {
  GLXContext *glx_context;
  trackball_state *trackball;
  rotator *rot;
  Bool button_down_p;
  texture_font_data *texfont;
  GLuint loading_dlist;

  int puzzle_width;
  int puzzle_height;
  puzzle_piece *puzzle;

  enum { PUZZLE_LOADING_MSG,
         PUZZLE_LOADING,
         PUZZLE_UNSCATTER,
         PUZZLE_SOLVE, 
         PUZZLE_SCATTER } state;
  double pausing;
  double tick_speed;

  GLuint texid;
  GLfloat tex_x, tex_y, tex_width, tex_height, aspect;

  GLuint line_thickness;
};

static jigsaw_configuration *sps = NULL;

static GLfloat speed;
static GLfloat complexity_arg;
static int resolution_arg;
static GLfloat thickness_arg;
static Bool wobble_p;
static Bool debug_p;

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",       XrmoptionSepArg, 0 },
  { "-complexity", ".complexity",  XrmoptionSepArg, 0 },
  { "-resolution", ".resolution",  XrmoptionSepArg, 0 },
  { "-thickness",  ".thickness",   XrmoptionSepArg, 0 },
  { "-wobble",     ".wobble",      XrmoptionNoArg, "True" },
  { "+wobble",     ".wobble",      XrmoptionNoArg, "False" },
  { "-debug",      ".debug",       XrmoptionNoArg, "True" },
};

static argtype vars[] = {
  {&speed,          "speed",       "Speed",       DEF_SPEED,      t_Float},
  {&complexity_arg, "complexity",  "Complexity",  DEF_COMPLEXITY, t_Float},
  {&resolution_arg, "resolution",  "Resolution",  DEF_RESOLUTION, t_Int},
  {&thickness_arg,  "thickness",   "Thickness",   DEF_THICKNESS,  t_Float},
  {&wobble_p,       "wobble",      "Wobble",      DEF_WOBBLE,     t_Bool},
  {&debug_p,        "debug",       "Debug",       DEF_DEBUG,      t_Bool},
};

ENTRYPOINT ModeSpecOpt jigsaw_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Returns a spline describing one edge of a puzzle piece of the given length.
 */
static spline *
make_puzzle_curve (int pixels)
{
  double x0 = 0.0000, y0 =  0.0000;
  double x1 = 0.3333, y1 =  0.1000;
  double x2 = 0.4333, y2 =  0.0333;
  double x3 = 0.4666, y3 = -0.0666;
  double x4 = 0.3333, y4 = -0.1666;
  double x5 = 0.3666, y5 = -0.2900;
  double x6 = 0.5000, y6 = -0.3333;

  spline *s = make_spline(20);
  s->n_controls = 0;

# define PT(x,y) \
    s->control_x[s->n_controls] = pixels * (x); \
    s->control_y[s->n_controls] = pixels * (y); \
    s->n_controls++
  PT (  x0, y0);
  PT (  x1, y1);
  PT (  x2, y2);
  PT (  x3, y3);
  PT (  x4, y4);
  PT (  x5, y5);
  PT (  x6, y6);
  PT (1-x5, y5);
  PT (1-x4, y4);
  PT (1-x3, y3);
  PT (1-x2, y2);
  PT (1-x1, y1);
  PT (1-x0, y0);
# undef PT

  compute_spline (s);
  return s;
}


#ifdef HAVE_TESS

static void
tess_error_cb (GLenum errorCode)
{
  fprintf (stderr, "%s: tesselation error: %s\n",
           progname, gluErrorString(errorCode));
  exit (0);
}


static void
tess_combine_cb (GLdouble coords[3], GLdouble *d[4], GLfloat w[4], 
                 GLdouble **dataOut)
{
  GLdouble *new = (GLdouble *) malloc (3 * sizeof(*new));
  new[0] = coords[0];
  new[1] = coords[1];
  new[2] = coords[2];
  *dataOut = new;
}


static void
tess_vertex_cb (void *vertex_data, void *closure)
{
  puzzle_piece *p = (puzzle_piece *) closure;
  GLdouble *v = (GLdouble *) vertex_data;
  GLdouble x = v[0];
  GLdouble y = v[1];
  GLdouble z = v[2];

  if (p)
    {
      GLfloat pw = p->jc->puzzle_width;
      GLfloat ph = p->jc->puzzle_height;

      GLfloat xx = x / (GLfloat) resolution_arg;  /* 0-1 from piece origin */
      GLfloat yy = y / (GLfloat) resolution_arg;
      GLdouble tx = (p->home.x + xx)      / pw;   /* 0-1 from puzzle origin */
      GLdouble ty = (ph - p->home.y - yy) / ph;

      tx = p->jc->tex_x + (tx * p->jc->tex_width);
      ty = p->jc->tex_y + (ty * p->jc->tex_height);

      glTexCoord2d (tx, ty);
    }

  glVertex3d (x, y, z);
}

#else  /* HAVE_TESS */

/* Writes triangles into the array of floats.
   Returns the number of floats written (triangles * 9).
 */
static int
make_piece_eighth (jigsaw_configuration *jc, const spline *s,
                   int resolution, int type, GLfloat *out,
                   Bool flip_x, Bool flip_y, Bool rotate_p)
{
  GLfloat *oout = out;
  int cx = resolution/2;
  int cy = resolution/2;
  int np = (s->n_points / 2) + 1;
  int last_x = -999999, last_y = -999999;
  Bool inflected = False;
  int i;

  if (type == FLAT)
    {
      *out++ = cx;
      *out++ = 0;
      *out++ = 0;

      *out++ = cx;
      *out++ = cy;
      *out++ = 0;

      *out++ = 0;
      *out++ = 0;
      *out++ = 0;

      goto END;
    }

  for (i = (type == IN ? np-1 : 0); 
       (type == IN ? i >= 0 : i < np);
       i += (type == IN ? -1 : 1))
    {
      int x = s->points[i].x;
      int y = s->points[i].y;

      if (type == IN)
        y = -y;

      if (last_x != -999999)
        {
          if (!inflected &&
              (type == IN
               ? x >= last_x 
               : x < last_x))
            {
              inflected = True;

              *out++ = cx;
              *out++ = cy;
              *out++ = 0;

              *out++ = last_x;
              *out++ = last_y;
              *out++ = 0;

              if (type == IN)
                {
                  cx = 0;
                  cy = 0;
                }
              else
                {
                  cy = y;
                }

              *out++ = cx;
              *out++ = cy;
              *out++ = 0;
            }

          *out++ = cx;
          *out++ = cy;
          *out++ = 0;

          *out++ = last_x;
          *out++ = last_y;
          *out++ = 0;

          *out++ = x;
          *out++ = y;
          *out++ = 0;
        }

      last_x = x;
      last_y = y;
    }
 END:

  {
    int count = out - oout;
    Bool cw_p;

    if (flip_x)
      for (i = 0; i < count; i += 3)
        oout[i] = resolution - oout[i];

    if (flip_y)
      for (i = 0; i < count; i += 3)
        oout[i+1] = resolution - oout[i+1];

    cw_p = (type == IN);
    if (flip_x) cw_p = !cw_p;
    if (flip_y) cw_p = !cw_p;

    if (cw_p)
      for (i = 0; i < count; i += 9)
        {
          GLfloat x1 = oout[i+0];
          GLfloat y1 = oout[i+1];
          GLfloat x2 = oout[i+3];
          GLfloat y2 = oout[i+4];
          GLfloat x3 = oout[i+6];
          GLfloat y3 = oout[i+7];
          oout[i+0] = x2;
          oout[i+1] = y2;
          oout[i+3] = x1;
          oout[i+4] = y1;
          oout[i+6] = x3;
          oout[i+7] = y3;
        }

    if (rotate_p)
      for (i = 0; i < count; i += 3)
        {
          GLfloat x = oout[i];
          GLfloat y = oout[i+1];
          oout[i]   = resolution - y;
          oout[i+1] = x;
        }

    return count;
  }
}

#endif /* !HAVE_TESS */



/* Draws a puzzle piece.  The top/right/bottom/left_type args
   indicate the direction the tabs point: 1 for out, -1 for in, 0 for flat.
 */
static int
draw_piece (jigsaw_configuration *jc, puzzle_piece *p,
            int resolution, GLfloat thickness,
            int top_type, int right_type,
            int bottom_type, int left_type,
            Bool wire)
{
  spline *s = make_puzzle_curve (resolution);
  GLdouble *pts = (GLdouble *) malloc (s->n_points * 4 * 3 * sizeof(*pts));
  int polys = 0;
  int i, o;
  GLdouble z = resolution * thickness;

  o = 0;
  if (top_type == 0) {
    pts[o++] = 0;
    pts[o++] = 0; 
    pts[o++] = z;

    pts[o++] = resolution;
    pts[o++] = 0;
    pts[o++] = z;
  } else {
    for (i = 0; i < s->n_points; i++) {
      pts[o++] = s->points[i].x;
      pts[o++] = s->points[i].y * top_type;
      pts[o++] = z;
    }
  }

  if (right_type == 0) {
    pts[o++] = resolution;
    pts[o++] = resolution;
    pts[o++] = z;
  } else {
    for (i = 1; i < s->n_points; i++) {
      pts[o++] = resolution + s->points[i].y * (-right_type);
      pts[o++] = s->points[i].x;
      pts[o++] = z;
    }
  }

  if (bottom_type == 0) {
    pts[o++] = 0;
    pts[o++] = resolution;
    pts[o++] = z;
  } else {
    for (i = 1; i < s->n_points; i++) {
      pts[o++] = s->points[s->n_points-i-1].x;
      pts[o++] = resolution + s->points[s->n_points-i-1].y * (-bottom_type);
      pts[o++] = z;
    }
  }

  if (left_type == 0) {
    pts[o++] = 0;
    pts[o++] = 0;
    pts[o++] = z;
  } else {
    for (i = 1; i < s->n_points; i++) {
      pts[o++] = s->points[s->n_points-i-1].y * left_type;
      pts[o++] = s->points[s->n_points-i-1].x;
      pts[o++] = z;
    }
  }

  { GLfloat ss = 1.0 / resolution; glScalef (ss, ss, ss); }

# ifndef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  glPolygonMode (GL_FRONT_AND_BACK, wire ? GL_LINE : GL_FILL);
# endif

  if (wire)
    {
      glDisable (GL_TEXTURE_2D);
      glDisable (GL_BLEND);
      glDisable (GL_LIGHTING);
    }
  else
    {
# ifdef HAVE_TESS

#  ifndef  _GLUfuncptr
#   define _GLUfuncptr void(*)(void)
#  endif
      GLUtesselator *tess = gluNewTess();
      gluTessCallback(tess, GLU_TESS_BEGIN,      (_GLUfuncptr)glBegin);
      gluTessCallback(tess, GLU_TESS_VERTEX_DATA,(_GLUfuncptr)tess_vertex_cb);
      gluTessCallback(tess, GLU_TESS_END,        (_GLUfuncptr)glEnd);
      gluTessCallback(tess, GLU_TESS_COMBINE,    (_GLUfuncptr)tess_combine_cb);
      gluTessCallback(tess, GLU_TESS_ERROR,      (_GLUfuncptr)tess_error_cb);

      /* front face */
      glEnable (GL_TEXTURE_2D);
      glEnable (GL_BLEND);
      glEnable (GL_LIGHTING);
      glBindTexture(GL_TEXTURE_2D, jc->texid);
      glFrontFace (GL_CCW);
      glNormal3f (0, 0, 1);
      gluTessBeginPolygon (tess, p);
      gluTessBeginContour (tess);
      for (i = 0; i < o; i += 3)
        {
          GLdouble *p = pts + i;
          gluTessVertex (tess, p, p);
          polys++;  /* not quite right but close */
        }
      gluTessEndContour(tess);
      gluTessEndPolygon(tess);

      /* back face */
      glDisable (GL_TEXTURE_2D);
      glFrontFace (GL_CW);
      glNormal3f (0, 0, -1);
      gluTessBeginPolygon (tess, 0);
      gluTessBeginContour (tess);
      for (i = 0; i < o; i += 3)
        {
          GLdouble *p = pts + i;
          p[2] = -p[2];
          gluTessVertex (tess, p, p);
          polys++;  /* not quite right but close */
        }
      gluTessEndContour(tess);
      gluTessEndPolygon(tess);
      gluDeleteTess(tess);

      /* Put it back */
      for (i = 0; i < o; i += 3)
        {
          GLdouble *p = pts + i;
          p[2] = -p[2];
        }

# else  /* !HAVE_TESS */

      GLfloat *tri = (GLfloat *)
        (GLfloat *) malloc (s->n_points * 4 * 3 * 3 * sizeof(*pts));
      GLfloat *otri = tri;
      int count;
      GLdouble zz;

      tri += make_piece_eighth (jc, s, resolution, top_type,    tri, 0, 0, 0);
      tri += make_piece_eighth (jc, s, resolution, top_type,    tri, 1, 0, 0);
      tri += make_piece_eighth (jc, s, resolution, left_type,   tri, 0, 1, 1);
      tri += make_piece_eighth (jc, s, resolution, left_type,   tri, 1, 1, 1);
      tri += make_piece_eighth (jc, s, resolution, bottom_type, tri, 0, 1, 0);
      tri += make_piece_eighth (jc, s, resolution, bottom_type, tri, 1, 1, 0);
      tri += make_piece_eighth (jc, s, resolution, right_type,  tri, 0, 0, 1);
      tri += make_piece_eighth (jc, s, resolution, right_type,  tri, 1, 0, 1);
      count = (tri - otri) / 9;

      if (! wire)
        {
          glEnable (GL_TEXTURE_2D);
          glEnable (GL_BLEND);
          glEnable (GL_LIGHTING);
          glBindTexture(GL_TEXTURE_2D, jc->texid);
        }

      for (zz = z; zz >= -z; zz -= 2*z)
        {
          int i;
          glFrontFace (zz > 0 ? GL_CCW : GL_CW);
          glNormal3f (0, 0, (zz > 0 ? 1 : -1));

          if (zz < 0)
            glDisable (GL_TEXTURE_2D);	/* back face */

          glPushMatrix();
          glTranslatef (0, 0, zz);

          tri = otri;
          if (wire)
            {
              for (i = 0; i < count; i++)
                {
                  glBegin (GL_LINE_LOOP);
                  glVertex3f (tri[0], tri[1], tri[2]); tri += 3;
                  glVertex3f (tri[0], tri[1], tri[2]); tri += 3;
                  glVertex3f (tri[0], tri[1], tri[2]); tri += 3;
                  glEnd();
                }
            }
          else
            {
              GLfloat pw = p->jc->puzzle_width;
              GLfloat ph = p->jc->puzzle_height;
              GLfloat r = resolution;

              glBegin (GL_TRIANGLES);
              for (i = 0; i < count * 3; i++)
                {
                  GLfloat x = *tri++;
                  GLfloat y = *tri++;
                  GLfloat z = *tri++;

                  /* 0-1 from piece origin */
                  GLfloat xx = x / r;
                  GLfloat yy = y / r;

                  /* 0-1 from puzzle origin */
                  GLfloat tx = (p->home.x + xx)      / pw;
                  GLfloat ty = (ph - p->home.y - yy) / ph;

                  tx = p->jc->tex_x + (tx * p->jc->tex_width);
                  ty = p->jc->tex_y + (ty * p->jc->tex_height);

                  glTexCoord2f (tx, ty);
                  glVertex3f (x, y, z);
                }
              glEnd();
            }

          polys += count;
          glPopMatrix();
        }

      free (otri);
# endif /* !HAVE_TESS */
    }

  /* side faces */

  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
  for (i = 0; i < o; i += 3)
    {
      int j = (i+o-3) % o;
      int k = (i+3)   % o;
      GLdouble *p  = pts + i;
      GLdouble *pj = pts + j;
      GLdouble *pk = pts + k;

      do_normal  (pj[0], pj[1],  pj[2],
                  pj[0], pj[1], -pj[2],
                  pk[0], pk[1],  pk[2]);

      glVertex3f (p[0], p[1],  p[2]);
      glVertex3f (p[0], p[1], -p[2]);
      polys++;
    }
  glEnd();

  if (! wire)
    glColor3f (0.3, 0.3, 0.3);

  /* outline the edges in gray */

  glDisable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);
  glLineWidth (jc->line_thickness);

  glBegin (GL_LINE_LOOP);
  for (i = 0; i < o; i += 3)
    glVertex3f (pts[i], pts[i+1], pts[i+2]);
  glEnd();
  polys += o/3;

  glBegin (GL_LINE_LOOP);
  for (i = 0; i < o; i += 3)
    glVertex3f (pts[i], pts[i+1], -pts[i+2]);
  glEnd();
  polys += o/3;

  free_spline (s);
  free (pts);

  return polys;
}


static void
free_puzzle_grid (jigsaw_configuration *jc)
{
  int i;
  for (i = 0; i < jc->puzzle_width * jc->puzzle_height; i++)
    glDeleteLists (jc->puzzle[i].dlist,  1);
  free (jc->puzzle);
  jc->puzzle = 0;
  jc->puzzle_width = 0;
  jc->puzzle_height = 0;
}


static void
make_puzzle_grid (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int x, y;
  GLfloat size = (8 + (random() % 8)) * complexity_arg;

  if (jc->puzzle)
    free_puzzle_grid (jc);

  if (wire)
    jc->aspect = MI_WIDTH(mi) / (float) MI_HEIGHT(mi);

  if (jc->aspect >= 1.0)
    {
      jc->puzzle_width  = size;
      jc->puzzle_height = (size + 0.5) / jc->aspect;
    }
  else
    {
      jc->puzzle_width  = (size + 0.5) * jc->aspect;
      jc->puzzle_height = size;
    }

  if (jc->puzzle_width  < 1) jc->puzzle_width  = 1;
  if (jc->puzzle_height < 1) jc->puzzle_height = 1;

  if (debug_p)
    fprintf (stderr, "%s: grid  %4d x %-4d               (%.2f)\n", progname,
             jc->puzzle_width, jc->puzzle_height,
             ((float) jc->puzzle_width / jc->puzzle_height));

  jc->puzzle = (puzzle_piece *) 
    calloc (jc->puzzle_width * (jc->puzzle_height+1), sizeof(*jc->puzzle));

  /* Randomize the right and bottom edge of each piece.
     Match the left edge of the piece to the right to our right edge.
     Match the top edge of the piece to the bottom to our bottom edge.
   */
  for (y = 0; y < jc->puzzle_height; y++)
    for (x = 0; x < jc->puzzle_width; x++)
      {
        puzzle_piece *p = &jc->puzzle [y     * jc->puzzle_width + x];
        puzzle_piece *r = &jc->puzzle [y     * jc->puzzle_width + x+1];
        puzzle_piece *b = &jc->puzzle [(y+1) * jc->puzzle_width + x];
        p->edge[RIGHT]  = (random() & 1) ? IN : OUT;
        p->edge[BOTTOM] = (random() & 1) ? IN : OUT;
        r->edge[LEFT]   = p->edge[RIGHT]  == IN ? OUT : IN;
        b->edge[TOP]    = p->edge[BOTTOM] == IN ? OUT : IN;
      }

  /* tell each piece where it belongs. */
  for (y = 0; y < jc->puzzle_height; y++)
    for (x = 0; x < jc->puzzle_width; x++)
      {
        puzzle_piece *p = &jc->puzzle [y * jc->puzzle_width + x];
        p->jc = jc;
        p->home.x = x;
        p->home.y = y;
        p->current = p->home;

        /* make sure the outer border is flat */
        if (p->home.x == 0)  p->edge[LEFT] = FLAT;
        if (p->home.y == 0)  p->edge[TOP]  = FLAT;
        if (p->home.x == jc->puzzle_width-1)  p->edge[RIGHT]  = FLAT;
        if (p->home.y == jc->puzzle_height-1) p->edge[BOTTOM] = FLAT;

        /* generate the polygons */
        p->dlist = glGenLists (1);
        check_gl_error ("generating lists");
        if (p->dlist <= 0) abort();

        glNewList (p->dlist, GL_COMPILE);
        p->polys += draw_piece (jc, p,
                                resolution_arg, thickness_arg,
                                p->edge[TOP], p->edge[RIGHT],
                                p->edge[BOTTOM], p->edge[LEFT], 
                                wire);
        glEndList();
      }
}


static void shuffle_grid (ModeInfo *mi);


static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];

  jc->tex_x      = geometry->x      / (float) texture_width;
  jc->tex_y      = geometry->y      / (float) texture_height;
  jc->tex_width  = geometry->width  / (float) texture_width; 
  jc->tex_height = geometry->height / (float) texture_height;
  jc->aspect     = geometry->width  / (float) geometry->height;

  if (debug_p)
    {
      fprintf (stderr, "%s: image %s\n", progname, 
               (filename ? filename : "(null)"));
      fprintf (stderr, "%s: image %4d x %-4d + %4d + %-4d (%.2f)\n", progname,
               geometry->width, geometry->height, geometry->x, geometry->y,
               (float) geometry->width / geometry->height);
      fprintf (stderr, "%s: tex   %4d x %-4d\n", progname,
               texture_width, texture_height);
      fprintf (stderr, "%s: tex   %4.2f x %4.2f + %4.2f + %4.2f (%.2f)\n",
               progname,
               jc->tex_width, jc->tex_height, jc->tex_x, jc->tex_y,
               (jc->tex_width / jc->tex_height) *
               (texture_width / texture_height));
    }

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  make_puzzle_grid (mi);
}


static void
load_image (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  load_texture_async (mi->xgwa.screen, mi->window,
                      *jc->glx_context, 0, 0, 
                      False, jc->texid,
                      image_loaded_cb, mi);
}


/* Whether the two pieces are the same shape, when the second piece
   is rotated by the given degrees.
 */
static Bool
same_shape (puzzle_piece *p0, puzzle_piece *p1, int rotated_by)
{
  switch (rotated_by) 
    {
    case 0:
      return (p0->edge[0] == p1->edge[0] &&
              p0->edge[1] == p1->edge[1] &&
              p0->edge[2] == p1->edge[2] &&
              p0->edge[3] == p1->edge[3]);
    case 90:
      return (p0->edge[0] == p1->edge[1] &&
              p0->edge[1] == p1->edge[2] &&
              p0->edge[2] == p1->edge[3] &&
              p0->edge[3] == p1->edge[0]);
    case 180:
      return (p0->edge[0] == p1->edge[2] &&
              p0->edge[1] == p1->edge[3] &&
              p0->edge[2] == p1->edge[0] &&
              p0->edge[3] == p1->edge[1]);
    case 270:
      return (p0->edge[0] == p1->edge[3] &&
              p0->edge[1] == p1->edge[0] &&
              p0->edge[2] == p1->edge[1] &&
              p0->edge[3] == p1->edge[2]);
    default:
      abort();
    }
}


/* Returns the proper rotation for the piece at the given position. 
 */
static int
proper_rotation (jigsaw_configuration *jc, puzzle_piece *p, 
                 double x, double y)
{
  puzzle_piece *p1;
  int cx = x;
  int cy = y;
  if (cx != x) abort();  /* must be in integral position! */
  if (cy != y) abort();
  p1 = &jc->puzzle [cy * jc->puzzle_width + cx];
  if (same_shape (p, p1, 0))   return 0;
  if (same_shape (p, p1, 90))  return 90;
  if (same_shape (p, p1, 180)) return 180;
  if (same_shape (p, p1, 270)) return 270;
  abort();		/* these two pieces don't match in any rotation! */
}


/* Returns the piece currently at the given position. 
 */
static puzzle_piece *
piece_at (jigsaw_configuration *jc, double x, double y)
{
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;
  int cx = x;
  int cy = y;
  if (cx != x) abort();  /* must be in integral position! */
  if (cy != y) abort();

  for (i = 0; i < npieces; i++)
    {
      puzzle_piece *p = &jc->puzzle [i];
      if (p->current.x == cx &&
          p->current.y == cy)
        return p;
    }
  abort();   /* no piece at that position? */
}


static void
shuffle_grid (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int max_tries = jc->puzzle_width * jc->puzzle_height;
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;

  for (i = 0; i < npieces; i++)
    {
      puzzle_piece *p0 = &jc->puzzle [i];
      puzzle_piece *p1 = 0;
      int k;

      for (k = 0; k < max_tries; k++)
        {
          p1 = &jc->puzzle [random() % npieces];
          if (same_shape (p0, p1, 0))   break;
          if (same_shape (p0, p1, 90))  break;
          if (same_shape (p0, p1, 180)) break;
          if (same_shape (p0, p1, 270)) break;
          p1 = 0;  /* mismatch */
        }
      if (p1 && p0 != p1)
        {
          XYZR s;
          s = p0->current; p0->current = p1->current; p1->current = s;
          p0->current.r = 
            proper_rotation (jc, p0, p0->current.x, p0->current.y);
          p1->current.r = 
            proper_rotation (jc, p1, p1->current.x, p1->current.y);
        }
    }
}


/* We tend to accumulate floating point errors, e.g., z being 0.000001
   after a move.  This makes sure float values that should be integral are.
 */
static void
smooth_grid (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;

  for (i = 0; i < npieces; i++)
    {
      puzzle_piece *p = &jc->puzzle [i];
# define SMOOTH(P) \
        P.x = (int) (P.x + 0.5);                  \
        P.y = (int) (P.y + 0.5);                  \
        P.z = (int) (P.z + 0.5);                  \
        P.r = (int) (P.r + 0.5)
      SMOOTH(p->home);
      SMOOTH(p->current);
      SMOOTH(p->from);
      SMOOTH(p->to);
      if (p->tick <= 0.0001) p->tick = 0.0;
      if (p->tick >= 0.9999) p->tick = 1.0;
    }
}


static void
begin_scatter (ModeInfo *mi, Bool unscatter_p)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;
  XYZR ctr = { 0, };
  ctr.x = jc->puzzle_width  / 2;
  ctr.y = jc->puzzle_height / 2;

  for (i = 0; i < npieces; i++)
    {
      puzzle_piece *p = &jc->puzzle [i];
      XYZ a;
      double d, r, th;
      p->tick = -frand(1.0);
      p->from = p->current;

      a.x = p->from.x - ctr.x;	/* position relative to center */
      a.y = p->from.y - ctr.y;

      r = sqrt (a.x*a.x + a.y*a.y);
      th = atan2 (a.x, a.y);

      d = MAX (jc->puzzle_width, jc->puzzle_height) * 2;
      r = r*r + d;

      p->to.x = ctr.x + (r * sin (th));
      p->to.y = ctr.y + (r * cos (th));
      p->to.z = p->from.z;
      p->to.r = ((int) p->from.r + (random() % 180)) % 360;
      p->arc_height = frand(10.0);

      if (unscatter_p)
        {
          XYZR s = p->to; p->to = p->from; p->from = s;
          p->current = p->from;
        }
    }
}


static Bool
solved_p (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;

  for (i = 0; i < npieces; i++)
    {
      puzzle_piece *p = &jc->puzzle [i];
      if (p->current.x != p->home.x ||
          p->current.y != p->home.y ||
          p->current.z != p->home.z)
        return False;
    }
  return True;
}


static void
move_one_piece (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;

  for (i = 0; i < npieces * 100; i++)  /* shouldn't take that long */
    {
      int i = random() % npieces;
      puzzle_piece *p0 = &jc->puzzle [i];
      puzzle_piece *p1;

      if (p0->current.x == p0->home.x &&
          p0->current.y == p0->home.y &&
          p0->current.z == p0->home.z)
        continue;		/* piece already solved - try again */

      /* swap with the piece occupying p0's home cell. */
      p1 = piece_at (jc, p0->home.x, p0->home.y);

      if (p0 == p1) abort();    /* should have caught this above */

      p0->tick = 0;
      p0->from = p0->current;
      p0->to   = p1->current;
      p0->to.r = proper_rotation (jc, p0, p0->to.x, p0->to.y);

      p1->tick = 0;
      p1->from = p1->current;
      p1->to   = p0->current;
      p1->to.r = proper_rotation (jc, p1, p1->to.x, p1->to.y);

      /* Try to avoid having them intersect each other in the air. */
      p0->arc_height = 0;
      p1->arc_height = 0;
      while (fabs (p0->arc_height - p1->arc_height) < 1.5)
        {
          p0->arc_height = 0.5 + frand(3.0);
          p1->arc_height = 1.0 + frand(3.0);
        }

# define RTILT(V) \
         V = 90 - BELLRAND(180);       \
         if (! (random() % 5)) V *= 2; \
         if (! (random() % 5)) V *= 2; \
         if (! (random() % 5)) V *= 2
      RTILT (p0->max_tilt);
      RTILT (p1->max_tilt);
# undef RTILT

      if (debug_p)
        fprintf (stderr, "%s: swapping %2d,%-2d with %2d,%d\n", progname,
                 (int) p0->from.x, (int) p0->from.y,
                 (int) p1->from.x, (int) p1->from.y);
      return;
    }

  abort();  /* infinite loop! */
}


static Bool
anim_tick (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int npieces = jc->puzzle_width * jc->puzzle_height;
  int i;
  Bool finished_p = True;

  if (jc->pausing > 0)
    {
      jc->pausing -= jc->tick_speed * speed;
      if (debug_p && jc->pausing <= 0)
        fprintf (stderr, "%s:   (done pausing)\n", progname);
      return False;
    }

  for (i = 0; i < npieces; i++)
    {
      puzzle_piece *p = &jc->puzzle [i];
      double tt;

      if (p->tick >= 1.0) continue;	/* this piece is done */
      finished_p = False;		/* not done */

      p->tick += jc->tick_speed * speed;
      if (p->tick > 1.0) p->tick = 1.0;

      if (p->tick < 0.0) continue;	/* not yet started */

      tt = 1 - sin (M_PI/2 - p->tick * M_PI/2);

      p->current.x = p->from.x + ((p->to.x - p->from.x) * tt);
      p->current.y = p->from.y + ((p->to.y - p->from.y) * tt);
      p->current.z = p->from.z + ((p->to.z - p->from.z) * tt);
      p->current.r = p->from.r + ((p->to.r - p->from.r) * tt);

      p->current.z += p->arc_height * sin (p->tick * M_PI);

      p->tilt = p->max_tilt * sin (p->tick * M_PI);

    }

  return finished_p;
}


static void
loading_msg (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  const char *text = "Loading...";
  XCharStruct e;
  int w, h;
  texture_string_metrics (jc->texfont, text, &e, 0, 0);
  w = e.width;
  h = e.ascent + e.descent;

  if (wire) return;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (! jc->loading_dlist)
    {
      GLfloat othick = jc->line_thickness;
      puzzle_piece P = { 0, };
      P.jc = jc;
      jc->loading_dlist = glGenLists (1);
      glNewList (jc->loading_dlist, GL_COMPILE);
      jc->line_thickness = 1;
      draw_piece (jc, &P,
                  resolution_arg, thickness_arg,
                  OUT, OUT, IN, OUT, True);
      jc->line_thickness = othick;
      glEndList();
    }

  glColor3f (0.2, 0.2, 0.4);

  glPushMatrix();
  {
    double x, y, z;
    get_position (jc->rot, &x, &y, &z, True);
    glRotatef (x * 360, 1, 0, 0);
    glRotatef (y * 360, 0, 1, 0);
    glRotatef (z * 360, 0, 0, 1);
    glScalef (5, 5, 5);
    glTranslatef (-0.5, -0.5, 0);
    glCallList (jc->loading_dlist);
  }
  glPopMatrix();

  glColor3f (0.7, 0.7, 1);


  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  {
    double rot = current_device_rotation();
    glRotatef(rot, 0, 0, 1);
    if ((rot >  45 && rot <  135) ||
        (rot < -45 && rot > -135))
      {
        GLfloat s = MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
        glScalef (s, 1/s, 1);
      }
  }

  glOrtho(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi), -1, 1);
  glTranslatef ((MI_WIDTH(mi)  - w) / 2,
                (MI_HEIGHT(mi) - h) / 2,
                0);
  glEnable (GL_TEXTURE_2D);
  glPolygonMode (GL_FRONT, GL_FILL);
  glDisable (GL_LIGHTING);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  print_texture_string (jc->texfont, text);
  glEnable (GL_DEPTH_TEST);
  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
}


static void
animate (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  double slow = 0.01;
  double fast = 0.04;

  if (jc->button_down_p && jc->state != PUZZLE_LOADING_MSG)
    return;

  switch (jc->state)
    {
    case PUZZLE_LOADING_MSG:
      if (! jc->puzzle)
        loading_msg (mi);
      /* fall through */

    case PUZZLE_LOADING:
      if (!jc->puzzle) break;	/* still loading */
      jc->tick_speed = slow;
      shuffle_grid (mi);
      smooth_grid (mi);
      begin_scatter (mi, True);
      jc->pausing = 0;
      jc->state = PUZZLE_UNSCATTER;
      if (debug_p) fprintf (stderr, "%s: unscattering\n", progname);
      break;

    case PUZZLE_UNSCATTER:
      jc->tick_speed = slow;
      if (anim_tick (mi))
        {
          smooth_grid (mi);
          jc->pausing = 1.0;
          jc->state = PUZZLE_SOLVE;
          if (debug_p) fprintf (stderr, "%s: solving\n", progname);
        }
      break;

    case PUZZLE_SOLVE:
      jc->tick_speed = fast;
      if (anim_tick (mi))
        {
          smooth_grid (mi);
          if (solved_p (mi))
            {
              if (debug_p) fprintf (stderr, "%s: solved!\n", progname);
              begin_scatter (mi, False);
              jc->state = PUZZLE_SCATTER;
              jc->pausing = 3.0;
              if (debug_p) fprintf (stderr, "%s: scattering\n", progname);
            }
          else
            {
              move_one_piece (mi);
              jc->pausing = 0.3;
            }
        }
      break;

    case PUZZLE_SCATTER:
      jc->tick_speed = slow;
      if (anim_tick (mi))
        {
          free_puzzle_grid (jc);
          load_image (mi);
          jc->state = PUZZLE_LOADING;
          jc->pausing = 1.0;
          if (debug_p) fprintf (stderr, "%s: loading\n", progname);
        }
      break;

    default:
      abort();
    }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_jigsaw (ModeInfo *mi, int width, int height)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
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

  jc->line_thickness = (MI_IS_WIREFRAME (mi) ? 1 : MAX (1, height / 300.0));
}


ENTRYPOINT Bool
jigsaw_handle_event (ModeInfo *mi, XEvent *event)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, jc->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &jc->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      begin_scatter (mi, False);
      jc->state = PUZZLE_SCATTER;
      return True;
    }

  return False;
}


ENTRYPOINT void 
init_jigsaw (ModeInfo *mi)
{
  jigsaw_configuration *jc;
  int wire = MI_IS_WIREFRAME(mi);

  if (!sps) {
    sps = (jigsaw_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (jigsaw_configuration));
    if (!sps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }
  jc = &sps[MI_SCREEN(mi)];
  jc->glx_context = init_GL(mi);

  reshape_jigsaw (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {0.05, 0.07, 1.00, 0.0};
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  jc->trackball = gltrackball_init (False);
  jc->rot = make_rotator (0, 0, 0, 0, speed * 0.002, True);
  jc->texfont = load_texture_font (MI_DISPLAY(mi), "font");

  jc->state = PUZZLE_LOADING_MSG;

  resolution_arg /= complexity_arg;

# ifndef HAVE_TESS
  /* If it's not even, we get crosses. */
  if (resolution_arg & 1)
    resolution_arg++;
# endif /* !HAVE_TESS */

  if (wire)
    make_puzzle_grid (mi);
  else
    load_image (mi);
}


ENTRYPOINT void
draw_jigsaw (ModeInfo *mi)
{
  jigsaw_configuration *jc = &sps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  if (!jc->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(jc->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;

  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);
  gltrackball_rotate (jc->trackball);

  animate (mi);

  if (wobble_p && jc->puzzle)
    {
      double x, y, z;
      double max = 60;
      get_position (jc->rot, &x, &y, &z, !jc->button_down_p);
      x = 1; /* always lean back */
      glRotatef (max/2 - x*max, 1, 0, 0);
      glRotatef (max/2 - z*max, 0, 1, 0);
    }

  if (jc->puzzle)
    {
      GLfloat s = 14.0 / jc->puzzle_height;
      int x, y;

      glEnable(GL_CULL_FACE);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_NORMALIZE);
      glEnable(GL_LINE_SMOOTH);

      glScalef (s, s, s);
      glTranslatef (-jc->puzzle_width / 2.0, -jc->puzzle_height / 2.0, 0);

      if (! wire)
        {
          glEnable (GL_TEXTURE_2D);
          glEnable (GL_BLEND);
          glEnable (GL_LIGHTING);
          glEnable (GL_LIGHT0);
          glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

      for (y = 0; y < jc->puzzle_height; y++)
        for (x = 0; x < jc->puzzle_width; x++)
          {
            puzzle_piece *p = &jc->puzzle [y * jc->puzzle_width + x];
            glPushMatrix();
            glTranslatef (p->current.x, p->current.y, p->current.z);
            glTranslatef (0.5, 0.5, 0);
            glRotatef (p->current.r, 0, 0, 1);
            glRotatef (p->tilt, 0, 1, 0);
            glTranslatef (-0.5, -0.5, 0);
            glCallList(p->dlist);
            mi->polygon_count += p->polys;
            glPopMatrix();
          }
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE ("Jigsaw", jigsaw)

#endif /* USE_GL */
