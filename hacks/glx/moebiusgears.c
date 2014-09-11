/* moebiusgears, Copyright (c) 2007-2014 Jamie Zawinski <jwz@jwz.org>
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
			"*count:        17          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define refresh_mgears 0
# define release_mgears 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "involute.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_ROLL        "True"
#define DEF_SPEED       "1.0"
#define DEF_TEETH       "15"

typedef struct {

  gear g;
  GLfloat pos_th;  /* position on ring of gear system */
  GLfloat pos_thz; /* rotation out of plane of gear system */
} mogear;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int ngears;
  mogear *gears;
  GLfloat ring_r;  /* radius of gear system */
  GLfloat roll_th;

} mgears_configuration;

static mgears_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static Bool do_roll;
static int teeth_arg;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True"  },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0      },
  { "-wander", ".wander", XrmoptionNoArg, "True"  },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-roll",   ".roll",   XrmoptionNoArg, "True"  },
  { "+roll",   ".roll",   XrmoptionNoArg, "False" },
  { "-teeth",  ".teeth",  XrmoptionSepArg, 0      },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&do_roll,   "roll",   "Roll",   DEF_ROLL,   t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&teeth_arg, "teeth",  "Teeth",  DEF_TEETH,  t_Int},
};

ENTRYPOINT ModeSpecOpt mgears_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_mgears (ModeInfo *mi, int width, int height)
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


ENTRYPOINT void 
init_mgears (ModeInfo *mi)
{
  mgears_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bps) {
    bps = (mgears_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (mgears_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_mgears (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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
    }

  if (! bp->rot)
  {
    double spin_speed   = 0.5;
    double wander_speed = 0.01;
    double spin_accel   = 2.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False /* don't randomize */
                            );
    bp->trackball = gltrackball_init (True);
  }

  {
    int total_gears = MI_COUNT(mi);
    double gears_per_turn;
    double gear_r, tw, th, thick, slope;
    int nubs, size;

    if (! (total_gears & 1)) 
      total_gears++;		/* must be odd or gears intersect */

    /* Number of teeth must be odd if number of gears is odd, or teeth don't
       mesh when the loop closes.  And since number of gears must be odd...
     */
    if (! (teeth_arg & 1)) teeth_arg++;
    if (teeth_arg < 7) teeth_arg = 7;

    if (total_gears < 13)	/* gear mesh angle is too steep with less */
      total_gears = 13;

    thick = 0.2;
    nubs = (random() & 3) ? 0 : (random() % teeth_arg) / 2;

    slope = 0;

    /* Sloping gears are incompatible with "-roll" ... */
    /* slope= -M_PI * 2 / total_gears; */

    gears_per_turn = total_gears / 2.0;

    bp->ring_r = 3;
    gear_r = M_PI * bp->ring_r / gears_per_turn;
    tw = 0;
    th = gear_r * 2.5 / teeth_arg;

    /* If the gears are small, use a lower density mesh. */
    size = (gear_r > 0.32 ? INVOLUTE_LARGE  :
            gear_r > 0.13 ? INVOLUTE_MEDIUM :
            INVOLUTE_SMALL);

    /* If there are lots of teeth, use a lower density mesh. */
    if (teeth_arg > 77)
      size = INVOLUTE_SMALL;
    if (teeth_arg > 45 && size == INVOLUTE_LARGE)
      size = INVOLUTE_MEDIUM;

    if (bp->gears)
      {
        for (i = 0; i < bp->ngears; i++)
          glDeleteLists (bp->gears[i].g.dlist, 1);
        free (bp->gears);
      }

    bp->ngears = total_gears;

    bp->gears = (mogear *) calloc (bp->ngears, sizeof(*bp->gears));
    for (i = 0; i < bp->ngears; i++)
      {
        mogear *mg = &bp->gears[i];
        gear *g = &mg->g;

        g->r           = gear_r;
        g->size        = size;
        g->nteeth      = teeth_arg;
        g->tooth_w     = tw;
        g->tooth_h     = th;
        g->tooth_slope = slope;
        g->thickness   = g->r * thick;
        g->thickness2  = g->thickness * 0.1;
        g->thickness3  = g->thickness;
        g->inner_r     = g->r * 0.80;
        g->inner_r2    = g->r * 0.60;
        g->inner_r3    = g->r * 0.55;
        g->nubs        = nubs;
        mg->pos_th     = (M_PI * 2 / gears_per_turn) * i;
        mg->pos_thz    = (M_PI / 2 / gears_per_turn) * i;

        g->th = ((i & 1)
                 ? (M_PI * 2 / g->nteeth)
                 : 0);

        /* Colorize
         */
        g->color[0] = 0.7 + frand(0.3);
        g->color[1] = 0.7 + frand(0.3);
        g->color[2] = 0.7 + frand(0.3);
        g->color[3] = 1.0;

        g->color2[0] = g->color[0] * 0.85;
        g->color2[1] = g->color[1] * 0.85;
        g->color2[2] = g->color[2] * 0.85;
        g->color2[3] = g->color[3];

        /* Now render the gear into its display list.
         */
        g->dlist = glGenLists (1);
        if (! g->dlist)
          {
            check_gl_error ("glGenLists");
            abort();
          }

        glNewList (g->dlist, GL_COMPILE);
        g->polygons += draw_involute_gear (g, wire);
        glEndList ();
      }
  }
}


ENTRYPOINT Bool
mgears_handle_event (ModeInfo *mi, XEvent *event)
{
  mgears_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == '+' || c == '=' ||
          keysym == XK_Up || keysym == XK_Right || keysym == XK_Next)
        {
          MI_COUNT(mi) += 2;
          init_mgears (mi);
          return True;
        }
      else if (c == '-' || c == '_' ||
               keysym == XK_Down || keysym == XK_Left || keysym == XK_Prior)
        {
          if (MI_COUNT(mi) <= 13)
            return False;
          MI_COUNT(mi) -= 2;
          init_mgears (mi);
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        goto DEF;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
    DEF:
      MI_COUNT(mi) = 13 + (2 * (random() % 10));
      init_mgears (mi);
      return True;
    }

  return False;
}


ENTRYPOINT void
draw_mgears (ModeInfo *mi)
{
  mgears_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef ((x - 0.5) * 4,
                  (y - 0.5) * 4,
                  (z - 0.5) * 7);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);

    /* add a little rotation for -no-spin mode */
    x -= 0.14;
    y -= 0.06;

    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (1.5, 1.5, 1.5);

/*#define DEBUG*/

#ifdef DEBUG
  glScalef (.5, .5, .5);
  glTranslatef (0, -bp->gears[0].g.r * bp->ngears, 0);
#endif

  for (i = 0; i < bp->ngears; i++)
    {
      mogear *mg = &bp->gears[i];
      gear *g = &mg->g;

      glPushMatrix();
#ifndef DEBUG
      glRotatef (mg->pos_th  * 180 / M_PI, 0, 0, 1);  /* rotation on ring */
      glTranslatef (bp->ring_r, 0, 0);                /* position on ring */
      glRotatef (mg->pos_thz * 180 / M_PI, 0, 1, 0);  /* twist a bit */

      if (do_roll)
        {
          glRotatef (bp->roll_th * 180 / M_PI, 0, 1, 0);
          bp->roll_th += speed * 0.0005;
        }
#else
      glTranslatef (0, i * 2 * g->r, 0);
#endif
      glRotatef (g->th * 180 / M_PI, 0, 0, 1);

      glCallList (g->dlist);
      mi->polygon_count += g->polygons;
      glPopMatrix ();
    }

  glPopMatrix ();

#ifndef DEBUG
  /* spin gears */
  for (i = 0; i < bp->ngears; i++)
    {
      mogear *mg = &bp->gears[i];
      mg->g.th += speed * (M_PI / 100) * (i & 1 ? 1 : -1);
    }
#endif

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("MoebiusGears", moebiusgears, mgears)

#endif /* USE_GL */
