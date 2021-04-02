/* kaleidocycle, Copyright (c) 2013-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * A loop of rotating tetrahedra.  Created by jwz, July 2013.
 * Inspired by, and some math borrowed from:
 * http://www.kaleidocycles.de/pdf/kaleidocycles_theory.pdf
 * http://intothecontinuum.tumblr.com/post/50873970770/an-even-number-of-at-least-8-regular-tetrahedra
 */


#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        16          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define release_kaleidocycle 0

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>
#include <math.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "Z"
#define DEF_WANDER      "False"
#define DEF_SPEED       "1.0"

typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;

  int min_count, max_count;
  Bool startup_p;

  int ncolors;
  XColor *colors;
  int ccolor;

  GLfloat count;
  GLfloat th, dth;

  enum { STATIC, IN, OUT } mode, prev_mode;

} kaleidocycle_configuration;

static kaleidocycle_configuration *bps = NULL;

static char *do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt kaleidocycle_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Window management, etc
 */
ENTRYPOINT void
reshape_kaleidocycle (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
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

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
kaleidocycle_handle_event (ModeInfo *mi, XEvent *event)
{
  kaleidocycle_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == '+' || c == '.' || c == '>' || c == '=' || c == '+' ||
          keysym == XK_Right || keysym == XK_Up || keysym == XK_Next)
        {
          bp->mode = IN;
          return True;
        }
      else if ((c == '-' || c == ',' || c == '<' || c == '-' || c == '_' ||
               keysym == XK_Left || keysym == XK_Down || keysym == XK_Prior) &&
               bp->count > bp->min_count)
        {
          bp->mode = OUT;
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        goto DEF;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
    DEF:
      if (bp->count <= bp->min_count)
        bp->mode = IN;
      else
        bp->mode = (random() & 1) ? IN : OUT;
      return True;
    }

  return False;
}



ENTRYPOINT void 
init_kaleidocycle (ModeInfo *mi)
{
  kaleidocycle_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_kaleidocycle (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glLineWidth (4);

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

  {
    Bool spinx = False, spiny = False, spinz = False;
    double spin_speed   = 0.25;
    double wander_speed = 0.005;
    double spin_accel   = 0.2;
    double twist_speed  = 0.25;
    double twist_accel  = 1.0;

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

    bp->rot = make_rotator (spinx ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->rot2 = make_rotator (twist_speed, 0, 0, twist_accel, 0, True);

    bp->trackball = gltrackball_init (True);
  }

  if (MI_COUNT(mi) < 8) MI_COUNT(mi) = 8;
  if (MI_COUNT(mi) & 1) MI_COUNT(mi)++;

  bp->min_count = 8;
  bp->max_count = 12 + MI_COUNT(mi) * 1.3;
  if (bp->max_count & 1) bp->max_count++;
  bp->startup_p = True;

  bp->count = 0;
  bp->mode = IN;
  bp->prev_mode = IN;

/*
  bp->count = MI_COUNT(mi);
  bp->mode = STATIC;
*/

  bp->ncolors = 512;
  if (! bp->colors)
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_uniform_colormap (0, 0, 0,
                         bp->colors, &bp->ncolors,
                         False, 0, False);

  for (i = 0; i < bp->ncolors; i++)
    {
      /* make colors twice as bright */
      bp->colors[i].red   = (bp->colors[i].red   >> 2) + 0x7FFF;
      bp->colors[i].green = (bp->colors[i].green >> 2) + 0x7FFF;
      bp->colors[i].blue  = (bp->colors[i].blue  >> 2) + 0x7FFF;
    }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}



/* t = toroidal rotation, a = radial position
   colors = 4 colors, 4 channels each.
 */
static void
draw_tetra (ModeInfo *mi, double t, double a, Bool reflect_p,
            GLfloat *colors)
{
  int wire = MI_IS_WIREFRAME(mi);

  XYZ v1, v2, v3, P, Q;
  XYZ verts[4];
  int i;

  double scale;
  double sint = sin(t);
  double cost = cos(t);
  double tana = tan(a);
  double sint2 = sint * sint;
  double tana2 = tana * tana;

  v1.x = cost;
  v1.y = 0;
  v1.z = sint;

  scale = 1 / sqrt (1 + sint2 * tana2);
  v2.x = scale * -sint;
  v2.y = scale * -sint * tana;
  v2.z = scale * cost;

  v3.x = scale * -sint2 * tana;
  v3.y = scale;
  v3.z = scale * cost * sint * tana;

  P.x = v3.y / tana - v3.x;
  P.y = 0;
  P.z = -v3.z / 2;

  Q.x = v3.y / tana;
  Q.y = v3.y;
  Q.z = v3.z / 2;

  verts[0] = P;
  verts[1] = P;
  verts[2] = Q;
  verts[3] = Q;

  scale = sqrt(2) / 2;
  verts[0].x = P.x - scale * v1.x;
  verts[0].y = P.y - scale * v1.y;
  verts[0].z = P.z - scale * v1.z;

  verts[1].x = P.x + scale * v1.x;
  verts[1].y = P.y + scale * v1.y;
  verts[1].z = P.z + scale * v1.z;

  verts[2].x = Q.x - scale * v2.x;
  verts[2].y = Q.y - scale * v2.y;
  verts[2].z = Q.z - scale * v2.z;

  verts[3].x = Q.x + scale * v2.x;
  verts[3].y = Q.y + scale * v2.y;
  verts[3].z = Q.z + scale * v2.z;

  for (i = 0; i < 4; i++)
    {
      Bool reflect2_p = ((i + (reflect_p != 0)) & 1);
      XYZ a = verts[(i+1) % 4];
      XYZ b = verts[(i+2) % 4];
      XYZ c = verts[(i+3) % 4];
      XYZ n = ((i & 1)
               ? calc_normal (b, a, c)
               : calc_normal (a, b, c));
      if (wire)
        glColor4fv (colors + (i * 4));
      else
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colors + (i * 4));

      glFrontFace (reflect2_p ? GL_CW : GL_CCW);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glNormal3f (n.x, n.y, n.z);
      glVertex3f (a.x, a.y, a.z);
      glVertex3f (b.x, b.y, b.z);
      glVertex3f (c.x, c.y, c.z);
      glEnd();
    }
}


/* Reflect through the plane normal to the given vector.
 */
static void
reflect (double x, double y, double z)
{
  GLfloat m[4][4];

  m[0][0] = 1 - (2 * x * x);
  m[1][0] = -2 * x * y;
  m[2][0] = -2 * x * z;
  m[3][0] = 0;

  m[0][1] = -2 * x * y;
  m[1][1] = 1 - (2 * y * y);
  m[2][1] = -2 * y * z;
  m[3][1] = 0;

  m[0][2] = -2 * x * z;
  m[1][2] = -2 * y * z;
  m[2][2] = 1 - (2 * z * z);
  m[3][2] = 0;

  m[0][3] = 0;
  m[1][3] = 0;
  m[2][3] = 0;
  m[3][3] = 1;

  glMultMatrixf (&m[0][0]);
}


ENTRYPOINT void
draw_kaleidocycle (ModeInfo *mi)
{
  kaleidocycle_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat colors[4*4];
  GLfloat count;
  double t, a;
  int i;

  GLfloat bspec[4] = {1.0, 1.0, 1.0, 1.0};
  GLfloat bshiny   = 128.0;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 5,
                 (y - 0.5) * 5,
                 (z - 0.5) * 10);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1, 0, 0);
    glRotatef (y * 360, 0, 1, 0);
    glRotatef (z * 360, 0, 0, 1);

    get_rotation (bp->rot2, &x, &y, &z, !bp->button_down_p);
    bp->th = x * 360 * 10 * speed;

    /* Make sure the twist is always in motion.  Without this, the rotator
       sometimes stops, and for too long, and it's boring looking.
     */
    bp->th += speed * bp->dth++;
    while (bp->dth > 360) bp->dth -= 360;
    while (bp->th  > 360) bp->th  -= 360;
  }

  glMaterialfv (GL_FRONT, GL_SPECULAR,  bspec);
  glMateriali  (GL_FRONT, GL_SHININESS, bshiny);


  /* Evenly spread the colors of the faces, and cycle them together.
   */
  for (i = 0; i < 4; i++)
    {
      int o  = bp->ncolors / 4;
      int c = (bp->ccolor + (o*i)) % bp->ncolors;
      colors[i*4+0] = bp->colors[c].red   / 65536.0;
      colors[i*4+1] = bp->colors[c].green / 65536.0;
      colors[i*4+2] = bp->colors[c].blue  / 65536.0;
      colors[i*4+3] = 1;
    }
  bp->ccolor++;
  if (bp->ccolor >= bp->ncolors) bp->ccolor = 0;


  count = (int) floor (bp->count);
  while (count < 8) count++;
  if (((int) floor (count)) & 1) count++;

  a = 2 * M_PI / (bp->count < 8 ? 8 : bp->count);
  t = bp->th / (180 / M_PI);

  glScalef (3, 3, 3);
  glScalef (a, a, a);
  glRotatef (90, 0, 0, 1);
/*  glRotatef (45, 0, 1, 0); */

  for (i = 0; i <= (int) floor (bp->count); i++)
    {
      Bool flip_p = (i & 1);
      glPushMatrix();
      glRotatef ((i/2) * 4 * 180 / bp->count, 0, 0, 1);
      if (flip_p) reflect (-sin(a), cos(a), 0);

      if (bp->mode != STATIC && i >= (int) floor (bp->count))
        {
          /* Fractional bp->count means the last piece is in transition */
          GLfloat scale = bp->count - (int) floor (bp->count);
          GLfloat tick = 0.07 * speed;
          GLfloat ocount = bp->count;
          GLfloat alpha;

          /* Fill in faster if we're starting up */
          if (bp->count < MI_COUNT(mi))
            tick *= 2;

          glScalef (scale, scale, scale);

          switch (bp->mode) {
          case IN: break;
          case OUT: tick = -tick; break;
          case STATIC: tick = 0; break;
          }

          bp->count += tick;

          if (bp->mode == IN
              ? floor (ocount) != floor (bp->count)
              : ceil  (ocount) != ceil  (bp->count))
            {
              if (bp->mode == IN)
                bp->count = floor (ocount) + 1;
              else
                bp->count = ceil  (ocount) - 1;

              if (((int) floor (bp->count)) & 1 ||
                  (bp->mode == IN && 
                   (bp->count < MI_COUNT(mi) &&
                    bp->startup_p)))
                {
                  /* keep going if it's odd, or less than 8. */
                  bp->count = round(bp->count);
                }
              else
                {
                  bp->mode = STATIC;
                  bp->startup_p = False;
                }
            }

          alpha = (scale * scale * scale * scale);
          if (alpha < 0.4) alpha = 0.4;
          colors[3] = colors[7] = colors[11] = colors[15] = alpha;
        }

      draw_tetra (mi, t, a, flip_p, colors);
      mi->polygon_count += 4;

      glPopMatrix();
    }

  if (bp->mode == STATIC && !(random() % 200)) {
    if (bp->count <= bp->min_count)
      bp->mode = IN;
    else if (bp->count >= bp->max_count)
      bp->mode = OUT;
    else
      bp->mode = bp->prev_mode;

    bp->prev_mode = bp->mode;
  }


  mi->recursion_depth = ceil (bp->count);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_kaleidocycle (ModeInfo *mi)
{
  kaleidocycle_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->rot2) free_rotator (bp->rot2);
  if (bp->colors) free (bp->colors);
}

XSCREENSAVER_MODULE ("Kaleidocycle", kaleidocycle)

#endif /* USE_GL */
