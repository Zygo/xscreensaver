/* handsy, Copyright (c) 2018 Jamie Zawinski <jwz@jwz.org>
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
			"*count:        2           \n" \
			".foreground:   #8888CC"   "\n" \
			"*groundColor:  #0000FF"   "\n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n"

# define release_hands 0

#include "xlockmore.h"
#include "sphere.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include "gllist.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPEED       "1.0"
#define DEF_SPIN        "XY"
#define DEF_WANDER      "True"
#define DEF_FACE_FRONT  "True"
#define DEF_DEBUG       "False"

extern const struct gllist
 *handsy_model_finger_distal, *handsy_model_finger_intermediate,
  *handsy_model_finger_proximal, *handsy_model_finger_metacarpal,
  *handsy_model_thumb_distal, *handsy_model_thumb_proximal,
  *handsy_model_thumb_metacarpal, *handsy_model_palm;
static struct gllist *ground = 0;

static const struct gllist * const *all_objs[] = {
  &handsy_model_finger_distal, &handsy_model_finger_intermediate,
  &handsy_model_finger_proximal, &handsy_model_finger_metacarpal,
  &handsy_model_thumb_distal, &handsy_model_thumb_proximal,
  &handsy_model_thumb_metacarpal, &handsy_model_palm,
  (const struct gllist * const *) &ground
};

#define FINGER_DISTAL       0
#define FINGER_INTERMEDIATE 1
#define FINGER_PROXIMAL     2
#define FINGER_METACARPAL   3
#define THUMB_DISTAL        4
#define THUMB_PROXIMAL      5
#define THUMB_METACARPAL    6
#define PALM                7
#define GROUND              8


/* 'hand_geom' describes the position and extent of the various joints.
   'hand' describes the current flexion of the joints in that model.
   'hand_anim' is a list of positions and timings describing an animation.
   'hands_configuration' is the usual global state structure.
 */

typedef struct {
  double min, max;	/* +- pi */
} joint;

typedef struct {
  joint bones[4];
  joint base;
} finger;

typedef struct {
  finger fingers[5];
  joint palm;
  joint wrist1;
  joint wrist2;
} hand_geom;

static const hand_geom human_hand = {
 {{{{  0.0, 1.6 },	    /* thumb distal */
    {  0.0, 1.6 },	    /* thumb proximal */
    {  0.0, 1.6 },	    /* thumb metacarpal */
    {  0.0, 0.0 }},	    /* none */
   { -1.70, 0.00 }},
  {{{ -0.2, 1.6 },	    /* index distal */
    { -0.2, 1.6 },	    /* index intermediate */
    { -0.2, 1.6 },	    /* index proximal */
    {  0.0, 0.0 }},	    /* index metacarpal */
   { -0.25, 0.25 }},
  {{{ -0.2, 1.6 },	    /* middle distal */
    { -0.2, 1.6 },	    /* middle intermediate */
    { -0.2, 1.6 },	    /* middle proximal */
    {  0.0, 0.0 }},	    /* middle metacarpal */
   { -0.25, 0.25 }},
  {{{ -0.2, 1.6 },	    /* ring distal */
    { -0.2, 1.6 },	    /* ring intermediate */
    { -0.2, 1.6 },	    /* ring proximal */
    {  0.0, 0.0 }},	    /* ring metacarpal */
   { -0.25, 0.25 }},
  {{{ -0.2, 1.6 },	    /* pinky distal */
    { -0.2, 1.6 },	    /* pinky intermediate */
    { -0.2, 1.6 },	    /* pinky proximal */
    {  0.0, 0.0 }},	    /* pinky metacarpal */
   { -0.25, 0.25 }}},
 { -0.7, 1.5 },		    /* palm (wrist up/down) */
 { -M_PI, M_PI },	    /* wrist left/right */
 { -M_PI, M_PI },	    /* wrist rotate */
};

typedef struct {
  double joint[countof(human_hand.fingers)]		/* +- pi */	
              [countof(human_hand.fingers[0].bones)];
  double base[countof(human_hand.fingers)];
  double wrist[3];  /* up/down, left/right, rotate */
  double pos[3];    /* XYZ */
  Bool sinister;
  double alpha;
} hand;

typedef struct {
  const hand * const dest;
  double duration, pause;
  double pos[3], rot[3];    /* XYZ */
} hand_anim;

typedef struct {
  const hand_anim * const pair[2];	/* L/R */
  double delay;				/* Delay added to R */
} hand_anim_pair;

typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool spinx, spiny, spinz;
  Bool button_down_p;

  const hand_geom *geom;
  GLuint *dlists;
  int nhands;

  struct {
    const hand_anim *anim;
    int anim_hands; /* frames in animation */
    int anim_hand;  /* pos in anim, L/R */
    double anim_start, anim_time;
    double tick;
    double delay;
  } pair[2];

  struct { hand from, to, current; } *hands;
  GLfloat color[4];
  Bool ringp;

} hands_configuration;

#include "handsy_anim.h"


static hands_configuration *bps = NULL;

static GLfloat speed;
static char *do_spin;
static Bool do_wander;
static Bool face_front_p;
static Bool debug_p;

static XrmOptionDescRec opts[] = {
  { "-speed",   ".speed",     XrmoptionSepArg, 0 },
  { "-spin",    ".spin",      XrmoptionSepArg, 0 },
  { "+spin",    ".spin",      XrmoptionNoArg, "" },
  { "-wander",  ".wander",    XrmoptionNoArg, "True" },
  { "+wander",  ".wander",    XrmoptionNoArg, "False" },
  { "-front",   ".faceFront", XrmoptionNoArg, "True" },
  { "+front",   ".faceFront", XrmoptionNoArg, "False" },
  { "-debug",  ".debug",  XrmoptionNoArg, "True" },
  { "+debug",  ".debug",  XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_spin,      "spin",      "Spin",      DEF_SPIN,       t_String},
  {&do_wander,    "wander",    "Wander",    DEF_WANDER,     t_Bool},
  {&face_front_p, "faceFront", "FaceFront", DEF_FACE_FRONT, t_Bool},
  {&speed,        "speed",     "Speed",     DEF_SPEED,      t_Float},
  {&debug_p,      "debug",     "Debug",     DEF_DEBUG,      t_Bool},
};

ENTRYPOINT ModeSpecOpt hands_opts = {countof(opts), opts, countof(vars), vars, NULL};


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


static double
constrain_joint (double v, double min, double max)
{
  if (v < min) v = min;
  else if (v > max) v = max;
  return v;
}


static void
draw_hand (ModeInfo *mi, hand *h)
{
  hands_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int finger;
  int off = h->sinister ? -1 : 1;
  int nfingers = countof (bp->geom->fingers);
  int nbones   = countof (bp->geom->fingers[0].bones);

  glLineWidth (1);
  glPushMatrix();

  glTranslatef (off * h->pos[0], h->pos[1], h->pos[2]);
  glRotatef (h->wrist[1] * 180 / M_PI * -off, 0, 1, 0);
  glRotatef (h->wrist[2] * 180 / M_PI * -off, 0, 0, 1);
  glRotatef (h->wrist[0] * 180 / M_PI,        1, 0, 0);

  bp->color[3] = h->alpha;
  glColor4fv (bp->color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->color);

  if (!wire) glEnable (GL_BLEND);

  glPushMatrix();
  if (h->sinister)
    {
      glScalef (-1, 1, 1);
      glFrontFace (GL_CW);
    }
  else
    glFrontFace (GL_CCW);
  glCallList (bp->dlists[PALM]);
  glPopMatrix();

  for (finger = 0; finger < nfingers; finger++)
    {
      int bone = nbones - 2;
      glPushMatrix();
      if (finger == 0)   /* thumb */
        {
          glTranslatef (off * 0.113, -0.033, 0.093);
          glRotatef (off * 45, 0, 1, 0);

          if (h->sinister)
            glRotatef (180, 0, 0, 1);

          glRotatef (off * h->base[finger] * -180 / M_PI, 1, 0, 0);
          bone--;

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[THUMB_METACARPAL]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[THUMB_METACARPAL]);
          glPopMatrix();

          glTranslatef (0, 0, 0.1497);
          glRotatef (h->joint[finger][bone] * -180 / M_PI, 0, 1, 0);
          bone--;

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[THUMB_PROXIMAL]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[THUMB_PROXIMAL]);
          glPopMatrix();

          glTranslatef (0, 0, 0.1212);
          glRotatef (h->joint[finger][bone] * -180 / M_PI, 0, 1, 0);
          bone--;

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[THUMB_DISTAL]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[THUMB_DISTAL]);
          glPopMatrix();
        }
      else
        {
          switch (finger) {
          case 1: /* index */
            glTranslatef (off * 0.135, 0.004, 0.26835);
            glRotatef (off * 4, 0, 1, 0);
            break;
          case 2: /* middle */
            glTranslatef (off * 0.046, 0.004, 0.27152);
            glRotatef (off * 1, 0, 1, 0);
            break;
          case 3: /* ring */
            glTranslatef (off * -0.046, 0.004, 0.25577);
            glRotatef (off * -1, 0, 1, 0);
            break;
          case 4: /* pinky */
            glTranslatef (off * -0.135, 0.004, 0.22204);
            glRotatef (off * -4, 0, 1, 0);
            break;
          default: abort(); break;
          }

          glRotatef (90, 0, 0, 1);

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[FINGER_METACARPAL]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[FINGER_METACARPAL]);
          glPopMatrix();

          glTranslatef (0, 0, 0.1155);
          glRotatef (off * h->base[finger] * -180 / M_PI, 1, 0, 0);
          glRotatef (h->joint[finger][bone] * -180 / M_PI, 0, 1, 0);
          bone--;

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[FINGER_PROXIMAL]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[FINGER_PROXIMAL]);
          glPopMatrix();

          glTranslatef (0, 0, 0.1815);
          glRotatef (h->joint[finger][bone] * -180 / M_PI, 0, 1, 0);
          bone--;

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[FINGER_INTERMEDIATE]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[FINGER_INTERMEDIATE]);
          glPopMatrix();

          glTranslatef (0, 0, 0.1003);
          glRotatef (h->joint[finger][bone] * -180 / M_PI, 0, 1, 0);
          bone--;

          glFrontFace (GL_CCW);
          glCallList (bp->dlists[FINGER_DISTAL]);
          glPushMatrix();
          glScalef (1, -1, 1);
          glFrontFace (GL_CW);
          glCallList (bp->dlists[FINGER_DISTAL]);
          glPopMatrix();
        }
      glPopMatrix();
    }
  glPopMatrix();

  if (h->sinister && bp->ringp)
    {
      GLfloat color[] = { 1.0, 0.4, 0.4, 1 };
      GLfloat center = 0.4;
      GLfloat th;
      GLfloat r = center - h->pos[0] + 0.1;
      GLfloat min = 0.22;
      if (r < min) r = min;
      glPushMatrix();
      glTranslatef (-center, -0.28, 0.5);
      glRotatef (h->wrist[2] * 180 / M_PI * -off, 0, 0, 1);
      glRotatef (h->wrist[0] * 180 / M_PI,        1, 0, 0);

      glColor4fv (color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
      glDisable (GL_LIGHTING);
      glLineWidth (8);
      glBegin (GL_LINE_LOOP);
      for (th = 0; th < M_PI * 2; th += M_PI / 180)
        glVertex3f (r * cos(th), r * sin(th), 0);
      glEnd();
      if (! wire) glEnable (GL_LIGHTING);
      glPopMatrix();
    }

  glDisable (GL_BLEND);
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


static int
draw_ground (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat i, j, k;

  /* When using fog, iOS apparently doesn't like lines or quads that are
     really long, and extend very far outside of the scene. Maybe?  If the
     length of the line (cells * cell_size) is greater than 25 or so, lines
     that are oriented steeply away from the viewer tend to disappear
     (whether implemented as GL_LINES or as GL_QUADS).

     So we do a bunch of smaller grids instead of one big one.
  */
  int cells = 30;
  GLfloat cell_size = 0.8;
  int points = 0;
  int grids = 12;
  GLfloat color[4];

  if (wire) glLineWidth (1);

  parse_color (mi, "groundColor", color);

  glPushMatrix();

  glScalef (0.2, 0.2, 0.2);

  glRotatef (frand(90), 0, 0, 1);

  if (!wire)
    {
      GLfloat fog_color[4] = { 0, 0, 0, 1 };

      glLineWidth (4);
# ifndef GL_LINE_SMOOTH_BROKEN
      glEnable (GL_LINE_SMOOTH);
# endif
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);

      glFogi (GL_FOG_MODE, GL_EXP2);
      glFogfv (GL_FOG_COLOR, fog_color);
      glFogf (GL_FOG_DENSITY, 0.015);
      glFogf (GL_FOG_START, -cells/2 * cell_size * grids);
      glEnable (GL_FOG);
    }

  glColor4fv (color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);

  glTranslatef (-cells * grids * cell_size / 2,
                -cells * grids * cell_size / 2, 0);

  for (j = 0; j < grids; j++)
    {
      glPushMatrix();
      for (k = 0; k < grids; k++)
        {
          glBegin (GL_LINES);
          for (i = -cells/2; i < cells/2; i++)
            {
              GLfloat a = i * cell_size;
              GLfloat b = cells/2 * cell_size;
              glVertex3f (a, -b, 0); glVertex3f (a, b, 0); points++;
              glVertex3f (-b, a, 0); glVertex3f (b, a, 0); points++;
            }
          glEnd();
          glTranslatef (cells * cell_size, 0, 0);
        }
      glPopMatrix();
      glTranslatef (0, cells * cell_size, 0);
    }

  if (!wire)
    {
# ifndef GL_LINE_SMOOTH_BROKEN
      glDisable (GL_LINE_SMOOTH);
# endif
      glDisable (GL_BLEND);
      glDisable (GL_FOG);
    }

  glPopMatrix();

  return points;
}


ENTRYPOINT void
reshape_hands (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  glViewport (0, y, (GLint) width, (GLint) height);

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
hands_handle_event (ModeInfo *mi, XEvent *event)
{
  hands_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool ok = False;

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    ok = True;
  else if (debug_p && event->type == KeyPress)
    {
      int nfingers = countof (bp->geom->fingers);
      int nbones   = countof (bp->geom->fingers[0].bones);
      KeySym keysym;
      char c = 0;
      Bool tiltp = False;
      double delta = 0.02;
      double sign = 1;
      int i, j, k;

      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      for (i = 0; i < bp->nhands; i++)
        {
          hand *h = &bp->hands[i].current;
          Bool modp = !!event->xkey.state;

          switch (keysym) {
          case XK_Up:    ok = True; h->pos[(modp ? 1 : 2)] += delta; break;
          case XK_Down:  ok = True; h->pos[(modp ? 1 : 2)] -= delta; break;
          case XK_Right: ok = True; h->pos[0] += delta; break;
          case XK_Left:  ok = True; h->pos[0] -= delta; break;
          default: break;
          }

          switch (c) {
          case '?': case '/':
            ok = True;
            fprintf (stderr, "\n"
                     "	Open fingers:	12345 (1 = pinky, 5 = thumb)\n"
                     "	Close fingers:	QWERT\n"
                     "	Tilt left:	ASDFG\n"
                     "	Tilt right:	ZXCVB\n"
                     "	Bend wrist:	UJ (up/down)\n"
                     "	Bend wrist:	IK (left/right)\n"
                     "	Rotate wrist:	OL\n"
                     "	Move origin:	arrow keys: XZ plane; shift: XY plane\n"
                     "	Tab:		print current state to stdout\n"
                     "	0:		Reset\n"
                     "	?:		This\n\n");
            return ok;
            break;


          case '1': case '!': j = 4; sign = -1; goto FINGER;
          case '2': case '@': j = 3; sign = -1; goto FINGER;
          case '3': case '#': j = 2; sign = -1; goto FINGER;
          case '4': case '$': j = 1; sign = -1; goto FINGER;
          case '5': case '%': j = 0; sign = -1; goto FINGER;

          case 'q': case 'Q': j = 4; sign = 1; goto FINGER;
          case 'w': case 'W': j = 3; sign = 1; goto FINGER;
          case 'e': case 'E': j = 2; sign = 1; goto FINGER;
          case 'r': case 'R': j = 1; sign = 1; goto FINGER;
          case 't': case 'T': j = 0; sign = 1; goto FINGER;
            
          case 'a': case 'A': tiltp = True; j = 4; sign = 1; goto FINGER;
          case 's': case 'S': tiltp = True; j = 3; sign = 1; goto FINGER;
          case 'd': case 'D': tiltp = True; j = 2; sign = 1; goto FINGER;
          case 'f': case 'F': tiltp = True; j = 1; sign = 1; goto FINGER;
          case 'g': case 'G': tiltp = True; j = 0; sign = 1; goto FINGER;

          case 'z': case 'Z': tiltp = True; j = 4; sign = -1; goto FINGER;
          case 'x': case 'X': tiltp = True; j = 3; sign = -1; goto FINGER;
          case 'c': case 'C': tiltp = True; j = 2; sign = -1; goto FINGER;
          case 'v': case 'V': tiltp = True; j = 1; sign = -1; goto FINGER;
          case 'b': case 'B': tiltp = True; j = 0; sign = -1; goto FINGER;
          FINGER:
            ok = True;
            if (tiltp)
              h->base[j] = constrain_joint (h->base[j] + sign * delta,
                                            bp->geom->fingers[j].base.min,
                                            bp->geom->fingers[j].base.max);
            else
              for (k = 0; k < nbones; k++)
                h->joint[j][k] =
                  constrain_joint (h->joint[j][k] + sign * delta,
                                   bp->geom->fingers[j].bones[k].min,
                                   bp->geom->fingers[j].bones[k].max);
            break;

          case 'u': case 'U': ok = True; h->wrist[0] -= delta; break;
          case 'j': case 'J': ok = True; h->wrist[0] += delta; break;
          case 'i': case 'I': ok = True; h->wrist[1] += delta; break;
          case 'k': case 'K': ok = True; h->wrist[1] -= delta; break;
          case 'o': case 'O': ok = True; h->wrist[2] -= delta; break;
          case 'l': case 'L': ok = True; h->wrist[2] += delta; break;

          case '0': case ')':
            ok = True;
            for (j = 0; j < nfingers; j++)
              {
                h->base[j] = 0;
                for (k = 0; k < nbones; k++)
                  h->joint[j][k] = 0;
              }
            for (j = 0; j < 3; j++)
              h->wrist[j] = 0;
            for (j = 0; j < 3; j++)
              h->pos[j] = 0;
            break;

          case '\t':
            ok = True;
            fprintf (stdout, "\nstatic const hand H = {\n  {");
            for (i = 0; i < nfingers; i++)
              {
                if (i > 0) fprintf (stdout, "   ");
                fprintf (stdout, "{");
                for (j = 0; j < nbones; j++)
                  {
                    double v = h->joint[i][j];
                    if (i == 0 && j == 3) v = 0;  /* no thumb intermediate */
                    if (j == 0) fprintf (stdout, " ");
                    fprintf (stdout, "%.2f", v);
                    if (j < nbones-1) fprintf (stdout, ", ");
                  }
                fprintf (stdout, " }");
                if (i < nfingers-1) fprintf (stdout, ",\n");
              }
            fprintf (stdout, "},\n  { ");
            for (i = 0; i < nfingers; i++)
              {
                fprintf (stdout, "%.2f", h->base[i]);
                if (i < nfingers-1) fprintf (stdout, ", ");
              }
            fprintf (stdout, " },\n");
            fprintf (stdout, "  { %.2f, %.2f, %.2f },\n",
                     h->wrist[0], h->wrist[1], h->wrist[2]);
            fprintf (stdout, "  { %.2f, %.2f, %.2f },\n",
                     h->pos[0], h->pos[1], h->pos[2]);
            fprintf (stdout, "  True\n};\n\n");
            fflush (stdout);
            return ok;
            break;

          default: break;
          }
        }
    }
  return ok;
}


ENTRYPOINT void 
init_hands (ModeInfo *mi)
{
  hands_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  hand def[2];
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hands (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->pair[0].tick = bp->pair[1].tick = 1.0;
  bp->geom = &human_hand;
  bp->nhands = MI_COUNT(mi);
  if (bp->nhands <= 0) bp->nhands = 1;

  if (bp->nhands & 1) bp->nhands++;  /* Even number */

  if (debug_p)
    {
      bp->nhands = 1;
      do_spin = "";
      do_wander = False;
    }

  bp->hands = calloc (bp->nhands, sizeof(*bp->hands));

  {
    double spin_speed   = 0.5 * speed;
    double wander_speed = 0.005 * speed;
    double tilt_speed   = 0.001 * speed;
    double spin_accel   = 0.5;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') bp->spinx = True;
        else if (*s == 'y' || *s == 'Y') bp->spiny = True;
        else if (*s == 'z' || *s == 'Z') bp->spinz = True;
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

    bp->rot = make_rotator (bp->spinx ? spin_speed : 0,
                            bp->spiny ? spin_speed : 0,
                            bp->spinz ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->rot2 = (face_front_p
                ? make_rotator (0, 0, 0, 0, tilt_speed, True)
                : 0);
    bp->trackball = gltrackball_init (False);
  }

  /* Set default hand to the last hand in the animation list. */
  for (i = 0; i <= 1; i++)
    {
      int j;
      for (j = 0; ; j++)
        if (!all_hand_anims[j+1].pair[i])
          {
            if (! all_hand_anims[j].pair[i]) abort();
            def[i] = *all_hand_anims[j].pair[i]->dest;
            if (debug_p)
              def[i].alpha = 1;
            else
              {
                def[i].pos[1] = 5;  /* off screen */
                def[i].pos[2] = 5;
              }
            break;
          }
    }

  for (i = 0; i < bp->nhands; i++)
    {
      int sinister = (i & 1);
      bp->hands[i].to = def[sinister];
      bp->hands[i].to.sinister = sinister;
      bp->hands[i].from    = bp->hands[i].to;
      bp->hands[i].current = bp->hands[i].to;
    }

  glFrontFace(GL_CW);
  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      GLfloat s = 0.1;
      glNewList (bp->dlists[i], GL_COMPILE);
      switch (i) {
      case GROUND:
        if (! ground)
          ground = (struct gllist *) calloc (1, sizeof(*ground));
        ground->points = draw_ground (mi);
        break;
      default:
        glPushMatrix();
        glScalef (s, s, s);
        renderList (gll, wire);
        glPopMatrix();
        break;
      }
      glEndList ();
    }

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
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

  parse_color (mi, "foreground", bp->color);
}


static void
tick_hands (ModeInfo *mi)
{
  hands_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j, k, nfingers, nbones, sinister;
  double now = double_time();

  if (debug_p) return;

  if (!bp->pair[0].anim &&	/* Both hands finished. */
      !bp->pair[1].anim)	/* Pick a new animation. */
    {
      int nanims = 0;
      while (all_hand_anims[nanims].pair[0] ||
             all_hand_anims[nanims].pair[1])
        nanims++;
      i = random() % nanims;
      for (sinister = 0; sinister <= 1; sinister++)
        {
          bp->pair[sinister].anim = all_hand_anims[i].pair[sinister];
          bp->pair[sinister].anim_hand = 0;
          bp->pair[sinister].anim_hands = 0;
          while (bp->pair[sinister].anim[bp->pair[sinister].anim_hands].dest)
            bp->pair[sinister].anim_hands++;
          bp->pair[sinister].anim_start = now;
          bp->pair[sinister].tick = 0;

          if (sinister == 1)
            bp->pair[sinister].delay = all_hand_anims[i].delay;
        }

      bp->ringp = (all_hand_anims[i].pair[0] == goatse_anim);

      for (i = 0; i < bp->nhands; i++)
        {
          sinister = bp->hands[i].from.sinister;
          bp->hands[i].from = bp->hands[i].current;
          bp->hands[i].to = *bp->pair[sinister].anim->dest;
          bp->hands[i].to.sinister = sinister;

          j = bp->pair[sinister].anim_hand;

          bp->hands[i].to.alpha =
            (bp->pair[sinister].anim == hidden_anim ? 0 : 1);

          /* Anim keyframes can adjust position and rotation */
          for (k = 0; k < 3; k++)
            {
              bp->hands[i].to.wrist[k] +=
                bp->pair[sinister].anim[j].rot[k];
              bp->hands[i].to.pos[k] +=
                bp->pair[sinister].anim[j].pos[k];
            }
        }
    }

  for (sinister = 0; sinister <= 1; sinister++)
    {
      const hand_anim *h;
      double elapsed, duration, duration2;

      if (! bp->pair[sinister].anim)  /* Done with this hand, not the other. */
        continue;

      h = &bp->pair[sinister].anim[bp->pair[sinister].anim_hand];

      elapsed   = now - bp->pair[sinister].anim_start;
      duration  = h->duration / speed;
      duration2 = duration + (bp->pair[sinister].delay + h->pause) / speed;

      if (elapsed > duration2 &&  /* Done animating and pausing this hand. */
          bp->pair[sinister].tick >= 1)  /* ...and painted final frame. */
        {
          bp->pair[sinister].anim_hand++;
          bp->pair[sinister].tick = 1;
          if (bp->pair[sinister].anim_hand >= bp->pair[sinister].anim_hands)
            {
              /* Done with all steps of this hand's animation. */
              bp->pair[sinister].anim = 0;
              for (i = 0; i < bp->nhands; i++)
                if (bp->hands[i].from.sinister == sinister)
                  bp->hands[i].from = bp->hands[i].to = bp->hands[i].current;
            }
          else
            {
              /* Move to next step of animation. */
              for (i = 0; i < bp->nhands; i++)
                {
                  if (sinister != bp->hands[i].current.sinister)
                    continue;

                  j = bp->pair[sinister].anim_hand;
                  bp->hands[i].from = bp->hands[i].current;
                  bp->hands[i].to = *bp->pair[sinister].anim[j].dest;
                  bp->hands[i].to.alpha =
                    (bp->pair[sinister].anim == hidden_anim ? 0 : 1);

                  /* Anim keyframes can adjust position and rotation */
                  for (k = 0; k < 3; k++)
                    {
                      bp->hands[i].to.wrist[k] +=
                        bp->pair[sinister].anim[j].rot[k];
                      bp->hands[i].to.pos[k] +=
                        bp->pair[sinister].anim[j].pos[k];
                    }
                }
              bp->pair[sinister].anim_start = now;
              bp->pair[sinister].tick = 0;
              bp->pair[sinister].delay = 0;
            }
        }
      else if (elapsed > duration)	/* Done animating, still pausing. */
        bp->pair[sinister].tick = 1;
      else				/* Still animating. */
        bp->pair[sinister].tick = elapsed / duration;

      if (bp->pair[sinister].tick > 1)
        bp->pair[sinister].tick = 1;

      /* Move the joints into position:
         compute 'current' between 'from' and 'to' by ratio 'tick'. */

      nfingers = countof (bp->geom->fingers);
      nbones   = countof (bp->geom->fingers[0].bones);
      for (i = 0; i < bp->nhands; i++)
        {
          if (bp->hands[i].current.sinister != sinister)
            continue;
          for (j = 0; j < nfingers; j++)
            {
              for (k = 0; k < nbones; k++)
                bp->hands[i].current.joint[j][k] =
                  constrain_joint (bp->hands[i].from.joint[j][k] +
                                   bp->pair[sinister].tick
                                   * (bp->hands[i].to.joint[j][k] -
                                      bp->hands[i].from.joint[j][k]),
                                   bp->geom->fingers[j].bones[k].min,
                                   bp->geom->fingers[j].bones[k].max);
              bp->hands[i].current.base[j] =
                constrain_joint (bp->hands[i].from.base[j] +
                                 bp->pair[sinister].tick
                                 * (bp->hands[i].to.base[j] -
                                    bp->hands[i].from.base[j]),
                                 bp->geom->fingers[j].base.min,
                                 bp->geom->fingers[j].base.max);
            }
          j = 0;
          bp->hands[i].current.wrist[j] =
            constrain_joint (bp->hands[i].from.wrist[j] +
                             bp->pair[sinister].tick
                             * (bp->hands[i].to.wrist[j] -
                                bp->hands[i].from.wrist[j]),
                             bp->geom->palm.min,
                             bp->geom->palm.max);
          j = 1;
          bp->hands[i].current.wrist[j] =
            constrain_joint (bp->hands[i].from.wrist[j] +
                             bp->pair[sinister].tick
                             * (bp->hands[i].to.wrist[j] -
                                bp->hands[i].from.wrist[j]),
                             bp->geom->wrist1.min,
                             bp->geom->wrist1.max);
          j = 2;
          bp->hands[i].current.wrist[j] =
            constrain_joint (bp->hands[i].from.wrist[j] +
                             bp->pair[sinister].tick
                             * (bp->hands[i].to.wrist[j] -
                                bp->hands[i].from.wrist[j]),
                             bp->geom->wrist2.min,
                             bp->geom->wrist2.max);
          for (j = 0; j < 3; j++)
            bp->hands[i].current.pos[j] =
              constrain_joint (bp->hands[i].from.pos[j] +
                               bp->pair[sinister].tick
                               * (bp->hands[i].to.pos[j] -
                                  bp->hands[i].from.pos[j]),
                               -999, 999);
          bp->hands[i].current.alpha =
            bp->hands[i].from.alpha +
            bp->pair[sinister].tick *
            (bp->hands[i].to.alpha - bp->hands[i].from.alpha);
        }
    }
}


ENTRYPOINT void
draw_hands (ModeInfo *mi)
{
  hands_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat s;
  int i;

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat bshiny    = 128.0;
  GLfloat bcolor[4] = { 0.7, 0.7, 1.0, 1.0 };

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glRotatef(current_device_rotation(), 0, 0, 1);

  s = 10;
  glScalef (s, s, s);

  {
    double x, y, z;

    gltrackball_rotate (bp->trackball);

    if (face_front_p)
      {
        double maxx = 120 / 10.0;
        double maxy = 55 / 10.0;
        double maxz = 40 / 10.0;
        get_position (bp->rot2, &x, &y, &z, !bp->button_down_p);
        if (bp->spinx) glRotatef (maxx/2 - x*maxx, 0, 1, 0);
        if (bp->spiny) glRotatef (maxy/2 - y*maxy, 1, 0, 0);
        if (bp->spinz) glRotatef (maxz/2 - z*maxz, 0, 0, 1);
      }
    else
      {
        get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
        glRotatef (x * 360, 1, 0, 0);
        glRotatef (y * 360, 0, 1, 0);
        glRotatef (z * 360, 0, 0, 1);
      }

    glRotatef (-70, 1, 0, 0);

    glTranslatef (0, 0, -0.5);

    glPushMatrix();
    glRotatef ((bp->spiny ? y : bp->spinx ? x : z) * 90, 0, 0, 1);
    glCallList (bp->dlists[GROUND]);
    glPopMatrix();

    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    z += 1;  /* Origin of hands is 1.0 above floor. */
    glTranslatef((x - 0.5) * 0.8,
                 (y - 0.5) * 1.1,
                 (z - 0.5) * 0.2);
  }

  glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
  glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);

  {
    /* Lay the pairs out in a square-ish grid, keeping pairs together. */
    int rows = sqrt (bp->nhands / 2);
    int cols;
    int x, y;

    cols = ceil (bp->nhands / 2.0 / rows);

    if (bp->nhands <= 2)
      rows = cols = 1;

    if (MI_WIDTH(mi) < MI_HEIGHT(mi))  /* Portrait */
      {
        s = 0.5;
        glScalef (s, s, s);
      }

    if (bp->nhands == 1)
        glScalef (2, 2, 2);

    if (cols > 1)
      {
        s = 1.0 / rows;
        glScalef (s, s, s);
      }

    if (bp->nhands > 1)
      {
        s = 0.8;
        glTranslatef (-s * rows * 1.5, -s * cols, 0);
        glTranslatef (s, s, 0);
      }

    i = 0;
    for (y = 0; y < cols; y++)
      for (x = 0; x < rows; x++)
        {
          glPushMatrix();
          glTranslatef (x * s * 3, y * s * 2, y * s);
          if (i < bp->nhands)
            draw_hand (mi, &bp->hands[i++].current);
          glTranslatef (s, 0, 0);
          if (i < bp->nhands)
            draw_hand (mi, &bp->hands[i++].current);
          glPopMatrix();
        }
  }
  glPopMatrix();

  tick_hands (mi);

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hands (ModeInfo *mi)
{
  hands_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->trackball) gltrackball_free (bp->trackball);

  if (bp->dlists) {
    for (i = 0; i < countof(all_objs); i++)
      if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
    free (bp->dlists);
  }
}

XSCREENSAVER_MODULE_2 ("Handsy", handsy, hands)

#endif /* USE_GL */
