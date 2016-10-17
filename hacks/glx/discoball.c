/* discoball, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        30          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define refresh_ball 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "False"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"

typedef struct tile tile;
struct tile {
  XYZ position, normal;
  GLfloat size, tilt;
  tile *next;
};


typedef struct {
  XYZ normal;
  GLfloat color[4];
} ray;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  GLfloat th;
  trackball_state *trackball;
  Bool button_down_p;
  tile *tiles;
  int nrays;
  ray *rays;
} ball_configuration;

static ball_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt ball_opts = {countof(opts), opts, countof(vars), vars, NULL};


static XYZ
normalize (XYZ p)
{
  GLfloat d = sqrt(p.x*p.x + p.y*p.y * p.z*p.z);
  if (d < 0.0000001)
    p.x = p.y = p.z = 0;
  else
    {
      p.x /= d;
      p.y /= d;
      p.z /= d;
    }
    
  return p;
}


static void
build_texture (ModeInfo *mi)
{
  int x, y;
  int size = 128;
  int bpl = size * 2;
  unsigned char *data = malloc (bpl * size);

  for (y = 0; y < size; y++)
    {
      for (x = 0; x < size; x++)
        {
          unsigned char *c = &data [y * bpl + x * 2];
          GLfloat X = (x / (GLfloat) (size-1)) - 0.5;
          GLfloat Y = (y / (GLfloat) (size-1)) - 0.5;
          X = cos (X * X * 6.2);
          Y = cos (Y * Y * 6.2);
          X = X < Y ? X : Y;
          X *= 0.4;
          c[0] = 0xFF;
          c[1] = 0xFF * X;
        }
    }

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  check_gl_error ("texture param");

  glTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, size, size, 0,
                GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);
  check_gl_error ("light texture");
  free (data);
}


static int
draw_rays (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int i;

  glEnable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);
  glEnable (GL_BLEND);
  glDisable (GL_DEPTH_TEST);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE);

  for (i = 0; i < bp->nrays; i++)
    {
      GLfloat x = bp->rays[i].normal.x;
      GLfloat y = bp->rays[i].normal.y;
      GLfloat z = bp->rays[i].normal.z;
      glPushMatrix();

      /* Orient to direction of ray. */
      glRotatef (-atan2 (x, y)               * (180 / M_PI), 0, 0, 1);
      glRotatef ( atan2 (z, sqrt(x*x + y*y)) * (180 / M_PI), 1, 0, 0);

      glScalef (5, 5, 10);
      glTranslatef(0, 0, 1.1);
      glColor4fv (bp->rays[i].color);
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glTexCoord2f (0, 0); glVertex3f (-0.5, 0, -1);
      glTexCoord2f (1, 0); glVertex3f ( 0.5, 0, -1);
      glTexCoord2f (1, 1); glVertex3f ( 0.5, 0,  1);
      glTexCoord2f (0, 1); glVertex3f (-0.5, 0,  1);
      glEnd();
      polys++;
      glPopMatrix();
    }

  glDisable (GL_TEXTURE_2D);
  glEnable (GL_LIGHTING);
  glDisable (GL_BLEND);
  glEnable (GL_DEPTH_TEST);
  glDisable (GL_FOG);

  return polys;
}


static int
draw_ball_1 (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  tile *t;
  GLfloat m[4][4];

  glGetFloatv (GL_MODELVIEW_MATRIX, &m[0][0]);

  glFrontFace (GL_CW);

#if 0
  /* Draw the back rays.
   */
  if (! wire)
    {
      glPushMatrix();
      glLoadIdentity();
      glMultMatrixf (&m[0][0]);
      glTranslatef(0, 0, -4.1);
      glRotatef (bp->th, 0, 0, 1);
      polys += draw_rays (mi);
      glPopMatrix();
    }
  glClear(GL_DEPTH_BUFFER_BIT);
#endif


  /* Instead of rendering polygons for the foam ball substrate, let's
     just billboard a quad down the middle to mask out the back-facing
     tiles. */
  {
    glPushMatrix();
    m[0][0] = 1; m[1][0] = 0; m[2][0] = 0;
    m[0][1] = 0; m[1][1] = 1; m[2][1] = 0;
    m[0][2] = 0; m[1][2] = 0; m[2][2] = 1;
    glLoadIdentity();
    glMultMatrixf (&m[0][0]);
    glScalef (40, 40, 40);
    glTranslatef (-0.5, -0.5, -0.01);
    if (! wire)
      glDisable (GL_LIGHTING);
    /* Draw into the depth buffer but not the frame buffer */
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glColor3f (0, 0, 0);
    glBegin (GL_QUADS);
    glVertex3f (0, 0, 0);
    glVertex3f (0, 1, 0);
    glVertex3f (1, 1, 0);
    glVertex3f (1, 0, 0);
    glEnd();
    polys++;
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (! wire)
      glEnable (GL_LIGHTING);
    glPopMatrix();
    glColor3f (1, 1, 1);
  }

  /* Draw all the tiles.
   */
  for (t = bp->tiles; t; t = t->next)
    {
      GLfloat x = t->normal.x;
      GLfloat y = t->normal.y;
      GLfloat z = t->normal.z;
      GLfloat s = t->size / 2;
      glPushMatrix();

      /* Move to location of tile. */
      glTranslatef (t->position.x, t->position.y, t->position.z);

      /* Orient to direction tile is facing. */
      glRotatef (-atan2 (x, y)               * (180 / M_PI), 0, 0, 1);
      glRotatef ( atan2 (z, sqrt(x*x + y*y)) * (180 / M_PI), 1, 0, 0);

      glRotatef (t->tilt, 0, 1, 0);

      glScalef (s, s, s);
      glNormal3f (0, 1, 0);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (-1, 0, -1);
      glVertex3f ( 1, 0, -1);
      glVertex3f ( 1, 0,  1);
      glVertex3f (-1, 0,  1);
      glEnd();
      polys++;

      if (! wire)
        {
          GLfloat d = 0.2;
          glNormal3f (0, 0, -1);
          glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
          glVertex3f (-1,  0, -1);
          glVertex3f (-1, -d, -1);
          glVertex3f ( 1, -d, -1);
          glVertex3f ( 1,  0, -1);
          glEnd();
          polys++;

          glNormal3f (0, 0, 1);
          glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
          glVertex3f ( 1,  0,  1);
          glVertex3f ( 1, -d,  1);
          glVertex3f (-1, -d,  1);
          glVertex3f (-1,  0,  1);
          glEnd();
          polys++;

          glNormal3f (1, 0, 0);
          glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
          glVertex3f ( 1,  0, -1);
          glVertex3f ( 1, -d, -1);
          glVertex3f ( 1, -d,  1);
          glVertex3f ( 1,  0,  1);
          glEnd();
          polys++;

          glNormal3f (-1, 0, 0);
          glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
          glVertex3f (-1,  0,  1);
          glVertex3f (-1, -d,  1);
          glVertex3f (-1, -d, -1);
          glVertex3f (-1,  0, -1);
          glEnd();
          polys++;
        }

      glPopMatrix();
    }

  /* Draw the front rays.
   */
  if (! wire)
    {
      glPushMatrix();
      glLoadIdentity();
      glMultMatrixf (&m[0][0]);
      glTranslatef(0, 0, 4.1);
      glRotatef (-bp->th, 0, 0, 1);
      polys += draw_rays (mi);
      glPopMatrix();
    }

  return polys;
}


static GLfloat
vector_angle (XYZ a, XYZ b)
{
  double La = sqrt (a.x*a.x + a.y*a.y + a.z*a.z);
  double Lb = sqrt (b.x*b.x + b.y*b.y + b.z*b.z);
  double cc, angle;

  if (La == 0 || Lb == 0) return 0;
  if (a.x == b.x && a.y == b.y && a.z == b.z) return 0;

  /* dot product of two vectors is defined as:
       La * Lb * cos(angle between vectors)
     and is also defined as:
       ax*bx + ay*by + az*bz
     so:
      La * Lb * cos(angle) = ax*bx + ay*by + az*bz
      cos(angle)  = (ax*bx + ay*by + az*bz) / (La * Lb)
      angle = acos ((ax*bx + ay*by + az*bz) / (La * Lb));
  */
  cc = (a.x*b.x + a.y*b.y + a.z*b.z) / (La * Lb);
  if (cc > 1) cc = 1;  /* avoid fp rounding error (1.000001 => sqrt error) */
  angle = acos (cc);

  return (angle);
}


#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static void
build_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int rows = MI_COUNT (mi);

  GLfloat tile_size = M_PI / rows;
  GLfloat th0, th1;

  struct { XYZ position; GLfloat strength; } dents[5];
  int dent_count = random() % countof(dents);
  int i;
  for (i = 0; i < dent_count; i++)
    {
      GLfloat dist;
      dents[i].position.x = RANDSIGN() * (2 - BELLRAND(0.2));
      dents[i].position.y = RANDSIGN() * (2 - BELLRAND(0.2));
      dents[i].position.z = RANDSIGN() * (2 - BELLRAND(0.2));
      dist = sqrt (dents[i].position.x * dents[i].position.x +
                   dents[i].position.y * dents[i].position.y +
                   dents[i].position.z * dents[i].position.z);
      dents[i].strength = dist - (1 - BELLRAND(0.3));
      dents[i].strength = dist - (1 - BELLRAND(0.3));
    }


  for (th1 = M_PI/2; th1 > -(M_PI/2 + tile_size/2); th1 -= tile_size)
    {
      GLfloat x  = cos (th1);
      GLfloat y  = sin (th1);
      GLfloat x0 = cos (th1 - tile_size/2);
      GLfloat x1 = cos (th1 + tile_size/2);
      GLfloat circ0 = M_PI * x0 * 2;
      GLfloat circ1 = M_PI * x1 * 2;
      GLfloat circ  = (circ0 < circ1 ? circ0 : circ1);
      int row_tiles = floor ((circ < 0 ? 0 : circ) / tile_size);
      GLfloat spacing;
      GLfloat dropsy = 0.13 + frand(0.04);

      if (row_tiles <= 0) row_tiles = 1;
      spacing = M_PI*2 / row_tiles;

      for (th0 = 0; th0 < M_PI*2; th0 += spacing)
        {
          tile *t = (tile *) calloc (1, sizeof(*t));
          t->size = tile_size * 0.85;
          t->position.x = cos (th0) * x;
          t->position.y = sin (th0) * x;
          t->position.z = y;

          t->normal = t->position;

          /* Apply pressure on position from the dents. */
          for (i = 0; i < dent_count; i++)
            {
              GLfloat dist;
              XYZ direction;

              if (! (random() % 150))	/* Drop tiles randomly */
                {
                  free (t);
                  goto SKIP;
                }

              direction.x = t->position.x - dents[i].position.x;
              direction.y = t->position.y - dents[i].position.y;
              direction.z = t->position.z - dents[i].position.z;
              dist = sqrt (direction.x * direction.x +
                           direction.y * direction.y +
                           direction.z * direction.z);
              if (dist < dents[i].strength)
                {
                  GLfloat s = 1 - (dents[i].strength - dist) * 0.66;
                  XYZ n2 = t->normal;
                  GLfloat angle = vector_angle (t->position, dents[i].position);

                  /* Drop out the tiles near the apex of the dent. */
                  if (angle < dropsy)
                    {
                      free (t);
                      goto SKIP;
                    }

                  t->position.x *= s;
                  t->position.y *= s;
                  t->position.z *= s;

                  direction = normalize (direction);
                  n2.x -= direction.x;
                  n2.y -= direction.y;
                  n2.z -= direction.z;

                  t->normal.x = (t->normal.x + n2.x) / 2;
                  t->normal.y = (t->normal.y + n2.y) / 2;
                  t->normal.z = (t->normal.z + n2.z) / 2;
                }
            }

          /* Skew the direction the tile is facing slightly. */
          t->normal.x += 0.12 - frand(0.06);
          t->normal.y += 0.12 - frand(0.06);
          t->normal.z += 0.12 - frand(0.06);
          t->tilt = 4 - BELLRAND(8);

          t->next = bp->tiles;
          bp->tiles = t;
        SKIP: ;
        }
    }

  bp->nrays = 5 + BELLRAND(10);
  bp->rays = (ray *) calloc (bp->nrays, sizeof(*bp->rays));
  for (i = 0; i < bp->nrays; i++)
    {
      GLfloat th = frand(M_PI * 2);
      bp->rays[i].normal.x = cos (th);
      bp->rays[i].normal.y = sin (th);
      bp->rays[i].normal.z = 1;
      bp->rays[i].normal = normalize (bp->rays[i].normal);
      bp->rays[i].color[0] = 0.9 + frand(0.1);
      bp->rays[i].color[1] = 0.6 + frand(0.4);
      bp->rays[i].color[2] = 0.6 + frand(0.2);
      bp->rays[i].color[3] = 1;
    }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_ball (ModeInfo *mi, int width, int height)
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

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
ball_handle_event (ModeInfo *mi, XEvent *event)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void 
init_ball (ModeInfo *mi)
{
  ball_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (ball_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (ball_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  if (! wire)
    build_texture (mi);

  reshape_ball (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->th = 180 - frand(360);

  if (MI_COUNT(mi) < 10)
    MI_COUNT(mi) = 10;
  if (MI_COUNT(mi) > 200)
    MI_COUNT(mi) = 200;

  {
    double spin_speed   = 0.1;
    double wander_speed = 0.003;
    double spin_accel   = 1;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }

  build_ball (mi);

  if (!wire)
    {
      GLfloat color[4] = {0.5, 0.5, 0.5, 1};
      GLfloat cspec[4] = {1, 1, 1, 1};
      static const GLfloat shiny = 10;

      static GLfloat pos0[4] = { 0.5, -1, -0.5, 0};
      static GLfloat pos1[4] = {-0.75, -1, 0, 0};
      static GLfloat amb[4] = {0, 0, 0, 1};
      static GLfloat dif[4] = {1, 1, 1, 1};
      static GLfloat spc[4] = {1, 1, 1, 1};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHT1);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      color[0] += frand(0.2);
      color[1] += frand(0.2);
      color[2] += frand(0.2);

      cspec[0] -= frand(0.2);
      cspec[1] -= frand(0.2);
      cspec[2] -= frand(0.2);

      glLightfv(GL_LIGHT0, GL_POSITION, pos0);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glLightfv(GL_LIGHT1, GL_POSITION, pos1);
      glLightfv(GL_LIGHT1, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT1, GL_SPECULAR, spc);

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
      glMaterialfv (GL_FRONT, GL_SPECULAR,  cspec);
      glMateriali  (GL_FRONT, GL_SHININESS, shiny);
    }
}


ENTRYPOINT void
draw_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glRotatef(current_device_rotation(), 0, 0, 1);

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 2);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glRotatef (50, 1, 0, 0);

  glScalef (4, 4, 4);
  glRotatef (bp->th, 0, 0, 1);
  if (! bp->button_down_p)
    {
      bp->th += (bp->th > 0 ? speed : -speed);
      while (bp->th >  360) bp->th -= 360;
      while (bp->th < -360) bp->th += 360;
    }

  mi->polygon_count += draw_ball_1 (mi);
  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
release_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  while (bp->tiles)
    {
      tile *t = bp->tiles->next;
      free (bp->tiles);
      bp->tiles = t;
    }
  free (bp->rays);
}

XSCREENSAVER_MODULE_2 ("Discoball", discoball, ball)

#endif /* USE_GL */
