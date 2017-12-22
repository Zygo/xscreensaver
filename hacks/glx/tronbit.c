/* tronbit, Copyright (c) 2011-2014 Jamie Zawinski <jwz@jwz.org>
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
			"*wireframe:    False       \n"

# define free_bit 0
# define release_bit 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "sphere.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gllist.h"

extern const struct gllist *tronbit_idle1, *tronbit_idle2,
  *tronbit_no, *tronbit_yes;
static const struct gllist * const *all_objs[] = {
  &tronbit_idle1, &tronbit_idle2, &tronbit_no, &tronbit_yes };


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"

#define HISTORY_LENGTH 512
typedef enum { BIT_IDLE1, BIT_IDLE2, BIT_NO, BIT_YES } bit_state;
#define MODELS 4


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  double frequency;
  double confidence;

  double last_time;
  unsigned char history   [HISTORY_LENGTH];
  unsigned char histogram [HISTORY_LENGTH];
  int history_fp, histogram_fp;

  GLuint dlists[MODELS], polys[MODELS];
  char kbd;

} bit_configuration;

static bit_configuration *bps = NULL;

static const GLfloat colors[][4] = {
  { 0.66, 0.85, 1.00, 1.00 },
  { 0.66, 0.85, 1.00, 1.00 },
  { 1.00, 0.12, 0.12, 1.00 },
  { 0.98, 0.85, 0.30, 1.00 }
};


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

ENTRYPOINT ModeSpecOpt bit_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static int
make_bit (ModeInfo *mi, bit_state which)
{
  static const GLfloat spec[4] = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat shiny   = 128.0;
  const GLfloat *color = colors[which];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  GLfloat s;
  const struct gllist *gll;

  glMaterialfv (GL_FRONT, GL_SPECULAR,            spec);
  glMateriali  (GL_FRONT, GL_SHININESS,           shiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
  glColor4f (color[0], color[1], color[2], color[3]);

  glPushMatrix();
  switch (which)
    {
    case BIT_IDLE1:
      glRotatef (-44, 0, 1, 0);   /* line up the models with each other */
      glRotatef (-11, 1, 0, 0);
      glRotatef (  8, 0, 0, 1);
      s = 1.0;
      break;
    case BIT_IDLE2:
      glRotatef ( 16.0, 0, 0, 1);
      glRotatef (-28.0, 1, 0, 0);
      s = 1.0;
      break;
    case BIT_NO:
      glRotatef ( 16.0, 0, 0, 1);
      glRotatef (-28.0, 1, 0, 0);
      s = 1.6;
      break;
    case BIT_YES:
      glRotatef (-44.0, 0, 1, 0);
      glRotatef (-32.0, 1, 0, 0);
      s = 1.53;
      break;
    default:
      abort();
      break;
    }
  glScalef (s, s, s);
  gll = *all_objs[which];
  renderList (gll, wire);
  polys += gll->points / 3;
  glPopMatrix();

  return polys;
}


static void
tick_bit (ModeInfo *mi, double now)
{
  bit_configuration *bp = &bps[MI_SCREEN(mi)];
  double freq = bp->frequency;
  int n = bp->history[bp->history_fp];
  int histogram_speed = 3 * speed;
  int i;

  if (histogram_speed < 1) histogram_speed = 1;

  if (n == BIT_YES || n == BIT_NO)
    freq *= 2;

  if (bp->button_down_p) return;

  for (i = 0; i < histogram_speed; i++)
    {
      int nn = (n == BIT_YES ? 240 : n == BIT_NO  ? 17 : 128);
      int on = bp->histogram[(bp->histogram_fp-1) % countof(bp->histogram)];

      /* smooth out the square wave a little bit */

      if (!(nn > 100 && nn < 200) !=
          !(on > 100 && on < 200))
        nn += (((random() % 48) - 32) *
               ((on > 100 && on < 200) ? 1 : -1));

      nn += (random() % 16) - 8;

      bp->histogram_fp++;
      if (bp->histogram_fp >= countof(bp->history))
        bp->histogram_fp = 0;
      bp->histogram [bp->histogram_fp] = nn;
    }


  if (bp->last_time + freq > now && !bp->kbd) return;

  bp->last_time = now;

  bp->history_fp++;
  if (bp->history_fp >= countof(bp->history))
    bp->history_fp = 0;

  if (bp->kbd)
    {
      n = (bp->kbd == '1' ? BIT_YES :
           bp->kbd == '0' ? BIT_NO :
           (random() & 1) ? BIT_YES : BIT_NO);
      bp->kbd = 0;
    }
  else if (n == BIT_YES || 
           n == BIT_NO ||
           frand(1.0) >= bp->confidence)
    n = (n == BIT_IDLE1 ? BIT_IDLE2 : BIT_IDLE1);
  else
    n = (random() & 1) ? BIT_YES : BIT_NO;

  bp->history [bp->history_fp] = n;
}


static int
animate_bits (ModeInfo *mi, bit_state omodel, bit_state nmodel, GLfloat ratio)
{
  bit_configuration *bp = &bps[MI_SCREEN(mi)];
  int polys = 0;
  GLfloat scale = sin (ratio * M_PI / 2);
  GLfloat osize, nsize, small;
  int wire = MI_IS_WIREFRAME(mi);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      glEnable(GL_LIGHTING);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
    }

 if ((omodel == BIT_IDLE1 || omodel == BIT_IDLE2) &&
     (nmodel == BIT_IDLE1 || nmodel == BIT_IDLE2))
   small = 0.9;
 else
   small = 0.5;

  nsize = small + (1 - small) * scale;
  osize = small + (1 - small) * (1 - scale);

  glPushMatrix();
  glScalef (osize, osize, osize);
  glCallList (bp->dlists [omodel]);
  polys += bp->polys [omodel];
  glPopMatrix();

  glPushMatrix();
  glScalef (nsize, nsize, nsize);
  glCallList (bp->dlists [nmodel]);
  polys += bp->polys [nmodel];
  glPopMatrix();

  return polys;
}


static int
draw_histogram (ModeInfo *mi, GLfloat ratio)
{
  bit_configuration *bp = &bps[MI_SCREEN(mi)];
  int samples = countof (bp->histogram);
  GLfloat scalex = (GLfloat) mi->xgwa.width / samples;
  GLfloat scaley = mi->xgwa.height / 255.0 / 4;  /* about 1/4th of screen */
  int polys = 0;
  int overlays = 5;
  int k;
  
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);
  glDisable (GL_BLEND);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  {
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glLoadIdentity();
    /* glRotatef(current_device_rotation(), 0, 0, 1); */
    glOrtho (0, mi->xgwa.width, 0, mi->xgwa.height, -1, 1);

    for (k = 0; k < overlays; k++)
      {
        int i, j;
        GLfloat a = (GLfloat) k / overlays;

        glColor3f (0.3 * a, 0.7 * a, 1.0 * a);

        glBegin (GL_LINE_STRIP);

        j = bp->histogram_fp + 1;
        for (i = 0; i < samples; i++)
          {
            GLfloat x, y, z;
            if (j >= samples) j = 0;
            x = i;
            y = bp->histogram[j];
            z = 0;

            y += (int) ((random() % 16) - 8);
            y += 16;  /* margin at bottom of screen */

            x *= scalex;
            y *= scaley;

            glVertex3f (x, y, z);
            ++j;
            polys++;
          }
        glEnd();
      }

    glPopMatrix();
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);

  return polys;
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_bit (ModeInfo *mi, int width, int height)
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



ENTRYPOINT Bool
bit_handle_event (ModeInfo *mi, XEvent *event)
{
  bit_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if (keysym == XK_Up || keysym == XK_Left || keysym == XK_Prior)
        c = '1';
      else if (keysym == XK_Down || keysym == XK_Right || keysym == XK_Next)
        c = '0';

      if (c == ' ' || c == '\t' || c == '\n' || c == '1' || c == '0')
        {
          bp->kbd = c;
          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_bit (ModeInfo *mi)
{
  bit_configuration *bp;
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_bit (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    double spin_speed   = 3.0;
    double wander_speed = 0.03 * speed;
    double spin_accel   = 4.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (False);
  }

  for (i = 0; i < countof(bp->dlists); i++)
    {
      bp->dlists[i] = glGenLists (1);
      glNewList (bp->dlists[i], GL_COMPILE);
      bp->polys [i] = make_bit (mi, i);
      glEndList ();
    }

  bp->frequency  = 0.30 / speed;	/* parity around 3x/second */
  bp->confidence = 0.06;		/* provide answer 1/15 or so */

  for (i = 0; i < countof(bp->histogram); i++)
    bp->histogram[i] = 128 + (random() % 16) - 8;
}


ENTRYPOINT void
draw_bit (ModeInfo *mi)
{
  bit_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  glPushMatrix ();
  glScalef(1.1, 1.1, 1.1);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
    glRotatef(o, 0, 0, 1);
  }
# endif

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 11,
                 (y - 0.5) * 5,
                 (z - 0.5) * 3);
    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (6, 6, 6);

  {
    int nmodel = bp->history [bp->history_fp];
    int omodel = bp->history [bp->history_fp > 0
                              ? bp->history_fp-1
                              : countof(bp->history)-1];
    double now = double_time();
    double ratio = 1 - ((bp->last_time + bp->frequency) - now) / bp->frequency;
    if (ratio > 1) ratio = 1;
    mi->polygon_count += draw_histogram (mi, ratio);

    if (MI_WIDTH(mi) > MI_HEIGHT(mi) * 5) {   /* wide window: scale up */
      glScalef (8, 8, 8);
    }

    mi->polygon_count += animate_bits (mi, omodel, nmodel, ratio);
    tick_bit (mi, now);
  }
  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("TronBit", tronbit, bit)

#endif /* USE_GL */
