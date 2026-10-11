/* floppy, Copyright © 2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Created by jwz: 1-Sep-2026
 */

#define DEFAULTS	"*delay:	      20000       \n" \
			"*showFPS:            False       \n" \
			"*wireframe:          False       \n" \
			"*caseColor:          #CCCC00"   "\n" \
			"*diskColor:          #222222"   "\n" \
			"*doorColor:          #BBBBEE"   "\n" \
			"*spindleColor:       #BBBBEE"   "\n" \
 			"*tabColor:           #888888"   "\n" \
			"*springColor:        #CCCCCC"   "\n" \

# define release_floppy 0

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include "gllist.h"
#include "easing.h"
#include <ctype.h>

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

extern const struct gllist
  *floppy_model_top,
  *floppy_model_bottom,
  *floppy_model_spindle,
  *floppy_model_spring,
  *floppy_model_tab,
  *floppy_model_disk,
  *floppy_model_door;

static const struct gllist * const *all_objs[] = {
  &floppy_model_top,
  &floppy_model_bottom,
  &floppy_model_spindle,
  &floppy_model_spring,
  &floppy_model_tab,
  &floppy_model_disk,
  &floppy_model_door,
};

enum { TOP, BOTTOM, SPINDLE, SPRING, TAB, DISK, DOOR };

typedef struct { GLfloat x, y, z; } XYZ;

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  GLfloat tick;
  enum { DROP_BOTTOM,
         DROP_TAB,
         DROP_SPINDLE,
         DROP_DISK,
         DROP_SPRING,
         DROP_TOP,
         DROP_DOOR,
         PAUSE_1,
         OPEN_DOOR,
         RUN,
         SPIN,
         CLOSE_DOOR,
         PAUSE_2,
         EXPLODE,
         PAUSE_3,
         IMPLODE,
         OUT } state;
  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];
  GLfloat component_ratio[countof(all_objs)];
  GLfloat door_ratio, disk_th;
} floppy_configuration;

static floppy_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed_arg;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",    ".spin",    XrmoptionNoArg, "True" },
  { "+spin",    ".spin",    XrmoptionNoArg, "False" },
  { "-speed",   ".speed",   XrmoptionSepArg, 0 },
  { "-wander",  ".wander",  XrmoptionNoArg, "True" },
  { "+wander",  ".wander",  XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_spin,     "spin",    "Spin",    DEF_SPIN,    t_Bool},
  {&do_wander,   "wander",  "Wander",  DEF_WANDER,  t_Bool},
  {&speed_arg,   "speed",   "Speed",   DEF_SPEED,   t_Float},
};

ENTRYPOINT ModeSpecOpt floppy_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


ENTRYPOINT void
reshape_floppy (ModeInfo *mi, int width, int height)
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
floppy_handle_event (ModeInfo *mi, XEvent *event)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];
  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "Color");
  if (!XParseColor (mi->dpy, mi->xgwa.colormap, string, &xcolor))
    {
      fprintf (stderr, "%s: unparsable color in %s: %s\n", progname,
               key, string);
      exit (1);
    }
  free (string);

  color[0] = xcolor.red   / 65536.0;
  color[1] = xcolor.green / 65536.0;
  color[2] = xcolor.blue  / 65536.0;
  color[3] = 1;
}


static void
reset_rotator (ModeInfo *mi)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];
  double spin_speed   = speed_arg * 0.2;
  double wander_speed = speed_arg * 0.004;
  double spin_accel   = 0.3;

  gltrackball_reset (bp->trackball, 0, 0);

  if (bp->rot) free_rotator (bp->rot);
  bp->rot = make_rotator (do_spin ? spin_speed : 0,
                          do_spin ? spin_speed : 0,
                          do_spin ? spin_speed : 0,
                          spin_accel,
                          do_wander ? wander_speed : 0,
                          False);
}


ENTRYPOINT void
init_floppy (ModeInfo *mi)
{
  floppy_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_floppy (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 0.5, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  bp->trackball = gltrackball_init (True);
  reset_rotator (mi);

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  bp->state = DROP_BOTTOM;
  bp->tick = 0;
  bp->disk_th = frand (M_PI * 2);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case TOP:     key = "caseColor";    break;
      case BOTTOM:  key = "caseColor";    break;
      case SPINDLE: key = "spindleColor"; break;
      case SPRING:  key = "springColor";  break;
      case TAB:     key = "tabColor";     break;
      case DISK:    key = "diskColor";    break;
      case DOOR:    key = "doorColor";    break;
      default:
        abort();
      }

      parse_color (mi, key, bp->component_colors[i]);
      renderList (gll, wire);

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }
}


static int
draw_component (ModeInfo *mi, int i)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];

  static const GLfloat spec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat shiny    = 128.0;
  glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
  glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);

  glFrontFace (GL_CCW);
  glCallList (bp->dlists[i]);
  return (*all_objs[i])->points / 3;
}


static void
tick_floppy (ModeInfo *mi)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat ts;
  /* double fps = MI_DELAY(mi) ? 1000000.0 / MI_DELAY(mi) : 30; */
  double fps = 27;

  if (bp->button_down_p) return;

  switch (bp->state) {
  case DROP_BOTTOM:	ts = 4; break;
  case DROP_TAB:	ts = 2; break;
  case DROP_SPINDLE:	ts = 2; break;
  case DROP_DISK:	ts = 2; break;
  case DROP_SPRING:	ts = 2; break;
  case DROP_TOP:	ts = 2; break;
  case DROP_DOOR:	ts = 3; break;
  case PAUSE_1:         ts = 3; break;
  case OPEN_DOOR:	ts = 1; break;
  case RUN:		ts = 9; break;
  case SPIN:		ts = 8; break;
  case CLOSE_DOOR:	ts = 1; break;
  case PAUSE_2:         ts = 2; break;
  case EXPLODE:		ts = 1; break;
  case PAUSE_3:         ts = 9; break;
  case IMPLODE:		ts = 2; break;
  case OUT:		ts = 4; break;
  default: 		abort(); break;
  }

  if (bp->tick >= 1)
    {
      bp->tick = 0;
      bp->state = (bp->state + 1) % (OUT + 1);

      if (bp->state == SPIN+1 && (random() % 4))
        bp->state = RUN;  /* Stay in this state longer */

      if (bp->state == DROP_BOTTOM)
        {
          GLfloat c[4];
          reset_rotator (mi);

          c[0] = 0.3 + frand(0.7);
          c[1] = 0.3 + frand(0.7);
          c[2] = 0.3 + frand(0.7);
          c[3] = 1.0;

          if (! (random() % 5))
            c[3] = 0.7;  /* Translucent case */

          memcpy (bp->component_colors[TOP], c, sizeof(c));
          if (random() % 10)
            memcpy (bp->component_colors[BOTTOM], c, sizeof(c));
        }
    }

  bp->tick += speed_arg * (1 / (ts * fps));

  if (bp->tick > 1)
    bp->tick = 1;
}


static void
draw_floppy_1 (ModeInfo *mi)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int i, alpha_order;

  glPushMatrix();

  glEnable (GL_BLEND);
  glEnable (GL_NORMALIZE);
  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LESS);
  glDepthMask (GL_TRUE);

  if (!wire)
    {
      glEnable (GL_LIGHTING);
      glShadeModel (GL_SMOOTH);
    }

  glTranslatef (-0.5, -0.5, 0);

  switch (bp->state) {
  case DROP_BOTTOM:  bp->component_ratio[BOTTOM]  = bp->tick; break;
  case DROP_TAB:     bp->component_ratio[TAB]     = bp->tick; break;
  case DROP_SPINDLE: bp->component_ratio[SPINDLE] = bp->tick; break;
  case DROP_DISK:    bp->component_ratio[DISK]    = bp->tick; break;
  case DROP_SPRING:  bp->component_ratio[SPRING]  = bp->tick; break;
  case DROP_TOP:     bp->component_ratio[TOP]     = bp->tick; break;
  case DROP_DOOR:    bp->component_ratio[DOOR]    = bp->tick; break;
  case OPEN_DOOR:    bp->door_ratio               = bp->tick; break;
  case PAUSE_1:      break;
  case CLOSE_DOOR:   bp->door_ratio               = 1-bp->tick; break;
  case PAUSE_2:      break;
  case RUN:          bp->disk_th += speed_arg * 0.3; break;
  case SPIN:         bp->disk_th += speed_arg * 0.3; break;
  case EXPLODE:
    for (i = 0; i < countof(all_objs); i++)
      bp->component_ratio[i] = bp->tick;
    break;
  case PAUSE_3:     break;
  case IMPLODE:
    for (i = 0; i < countof(all_objs); i++)
      bp->component_ratio[i] = 1-bp->tick;
    break;
  case OUT:
    for (i = 0; i < countof(all_objs); i++)
      bp->component_ratio[i] = 1-bp->tick;
    break;

  default: abort(); break;
  }

  if (wire)
    glColor3f (1, 1, 1);

  /* Draw TOP and BOTTOM last, for alpha blending.
   */
  for (alpha_order = 0; alpha_order <= 1; alpha_order++)
    for (i = 0; i < countof(all_objs); i++)
      if (alpha_order == (i == TOP || i == BOTTOM))
        {
          Bool explodo = (bp->state == EXPLODE || bp->state == IMPLODE ||
                          bp->state == PAUSE_3);
          Bool outp  = bp->state == OUT;
          Bool spinp = bp->state == SPIN;
          easing_function fn = (explodo || outp ? EASE_IN_OUT_QUAD :
                                i == BOTTOM ? EASE_OUT_ELASTIC :
                                EASE_OUT_BOUNCE);
          GLfloat s = 3;
          GLfloat r = bp->component_ratio[i];
          GLfloat off;

          if (explodo)
            {
              r = 1-r;
              s = 0.5;

              switch (i) {
              case TOP:            break;
              case BOTTOM: s = -s; break;
              case DISK:   s = 0;  break;
              case DOOR:   s = 3;  break;
              default:     s /= 2; break;
              }
            }

          off = r;
          off = ease (fn, off);
          if (fn == EASE_OUT_BOUNCE)
            off = ease (EASE_OUT_SINE, off);  /* Tighten up the bounce */
          off = (1 - off) * s;

          glPushMatrix();

          if (spinp)
            {
              GLfloat x = 0.5;
              GLfloat y = 0.5;
              GLfloat z = 0;
              glTranslatef (x, y, z);
              glRotatef (ease (EASE_IN_OUT_QUAD, bp->tick) * 360 * 2,
                         0, 1, 0);
              glTranslatef (-x, -y, -z);
            }

          if (i == DOOR)
            {
              GLfloat dw = 0.128;   /* How wide the door opens */
              glTranslatef (ease (EASE_IN_OUT_QUAD, bp->door_ratio) * dw,
                            0, 0);
            }

          if (outp)
            glTranslatef (0, off * 10, 0);
          else if (i == DOOR)
            glTranslatef (0, -off * 2, 0);
          else
            glTranslatef (0, 0, off);

          if (i == DISK || i == SPINDLE)
            {
              GLfloat x = 0.957 / 2;	/* Disk axis */
              GLfloat y = 1.032 / 2;
              glTranslatef (x, y, 0);
              glRotatef (-bp->disk_th * 180/M_PI, 0, 0, 1);
              glTranslatef (-x, -y, 0);
            }

          mi->polygon_count += draw_component (mi, i);
          glPopMatrix();
        }

  glPopMatrix();
}


ENTRYPOINT void
draw_floppy (ModeInfo *mi)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat s;
  static const float fps_color[4] = {1, 1, 0, 1};

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 1,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  glRotatef (current_device_rotation(), 0, 0, 1);

  mi->polygon_count = 0;

  glRotatef (-90, 1, 0, 0);	/* Flat on table */
  glRotatef (30, 1, 0, 0);	/* Tilt forward */

  s = 8;
  glScalef (s, s, s);

  draw_floppy_1 (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps_color (mi, fps_color);
  glFinish();

  tick_floppy (mi);

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_floppy (ModeInfo *mi)
{
  floppy_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->trackball) gltrackball_free (bp->trackball);
  for (i = 0; i < countof(all_objs); i++)
    if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
}

XSCREENSAVER_MODULE ("Floppy", floppy)

#endif /* USE_GL */
