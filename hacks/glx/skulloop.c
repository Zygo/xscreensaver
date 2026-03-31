/* skulloop, Copyright © 2023-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000            \n" \
			"*showFPS:      False            \n" \
			"*wireframe:    False            \n" \
			"*suppressRotationAnimation: True\n" \
		        "*font:         sans-serif 16    \n" \
			".background:   #666666"        "\n" \
			"*skullColor:   #777777"        "\n" \
			"*teethColor:   #FFFFFF"        "\n" \
/* 
			".background:   #AA99BB"        "\n" \
			"*skullColor:   #AA99BB"        "\n" \
			"*teethColor:   #CCCCAA"        "\n" \
*/

# define release_skulloop 0

#include "xlockmore.h"
#include "normals.h"
#include "gltrackball.h"
#include "gllist.h"
#include "texfont.h"
#include "easing.h"

#ifdef USE_GL /* whole file */

#define DEF_SPEED  "1.0"
#define DEF_LENGTH "5"

extern const struct gllist
  *skull_model_skull_half, *skull_model_jaw_half,
  *skull_model_teeth_upper_half, *skull_model_teeth_lower_half;

static const struct gllist * const *all_objs[] = {
  &skull_model_skull_half, &skull_model_jaw_half,
  &skull_model_teeth_upper_half, &skull_model_teeth_lower_half,
};

enum { SKULL_HALF, JAW_HALF, TEETH_UPPER_HALF, TEETH_LOWER_HALF };


typedef struct obj obj;
struct obj {
  int id;
  XYZ pos, rot;
  GLfloat scale;
  XYZ head_pos;
  GLfloat jaw_pos, chatter;
  obj *next;
};

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  obj *objs, *dead;
  obj from, to;
  GLfloat ratio;
  int last_id;

  texture_font_data *font;
  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];

} skulloop_configuration;

static skulloop_configuration *bps = NULL;

static GLfloat speed;
static GLfloat length_arg;

static XrmOptionDescRec opts[] = {
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-length", ".length", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&length_arg,"length", "Length", DEF_LENGTH, t_Float},
};

ENTRYPOINT ModeSpecOpt skulloop_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


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
draw_component (ModeInfo *mi, int i)
{
  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);

  glFrontFace (GL_CCW);
  glCallList (bp->dlists[i]);

  glPushMatrix();
  glScalef (-1, 1, 1);
  glTranslatef (-0.05, 0, 0);  /* The model seems to have a gap */
  glFrontFace (GL_CW);
  glCallList (bp->dlists[i]);
  glPopMatrix();

  return 2 * (*all_objs[i])->points / 3;
}


static void
draw_skull (ModeInfo *mi, obj *o)
{
  /* head x, nod: -50 to +30
     head y, turn: +- 70
     head z, tilt: +- 30
     jaw, open: 0 - 22
   */
  const XYZ head_base = { 0, 200, 0 };
  const XYZ jaw_base  = { 0, 270, 40 };

  glRotatef (o->head_pos.x, 1, 0, 0);
  glRotatef (o->head_pos.y, 0, 1, 0);
  glRotatef (o->head_pos.z, 0, 0, 1);
  glTranslatef (-head_base.x, -head_base.y, -head_base.z);

  mi->polygon_count += draw_component (mi, SKULL_HALF);

  if (o->jaw_pos >= -1)
    mi->polygon_count += draw_component (mi, TEETH_UPPER_HALF);

  if (o->jaw_pos >= 0)
    {
      glTranslatef (jaw_base.x, jaw_base.y, jaw_base.z);
      glRotatef (o->jaw_pos, 1, 0, 0);
      glTranslatef (-jaw_base.x, -jaw_base.y, -jaw_base.z);

      mi->polygon_count += draw_component (mi, JAW_HALF);
      mi->polygon_count += draw_component (mi, TEETH_LOWER_HALF);
    }
}


static void
draw_obj (ModeInfo *mi, obj *o)
{
# ifndef DEBUG
  GLfloat s = 0.005;
  glPushMatrix();
  glTranslatef (0, -0.5, -0.2);
  glScalef (s, s, s);
  draw_skull (mi, o);
  glPopMatrix();

# else /* DEBUG */

  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];
  char buf[20];
  GLfloat s = 0.007;
  sprintf (buf, "%d", o->id);
  glDisable (GL_LIGHTING);
  glPushMatrix();
  glScalef (s, s, s);
  print_texture_string (bp->font, buf);
  glPopMatrix();

  glBegin (GL_LINE_LOOP);
  glVertex3f (-0.5, -0.5, -0.5);
  glVertex3f (-0.5,  0.5, -0.5);
  glVertex3f ( 0.5,  0.5, -0.5);
  glVertex3f ( 0.5, -0.5, -0.5);
  glEnd();
  glBegin (GL_LINE_LOOP);
  glVertex3f ( 0.5, -0.5,  0.5);
  glVertex3f ( 0.5,  0.5,  0.5);
  glVertex3f (-0.5,  0.5,  0.5);
  glVertex3f (-0.5, -0.5,  0.5);
  glEnd();
  glBegin (GL_LINES);
  glVertex3f (-0.5, -0.5, -0.5);
  glVertex3f (-0.5, -0.5,  0.5);
  glVertex3f (-0.5,  0.5, -0.5);
  glVertex3f (-0.5,  0.5,  0.5);
  glVertex3f ( 0.5, -0.5, -0.5);
  glVertex3f ( 0.5, -0.5,  0.5);
  glVertex3f ( 0.5,  0.5, -0.5);
  glVertex3f ( 0.5,  0.5,  0.5);
  glEnd();
# endif /* DEBUG */
}


static void
draw_objs (ModeInfo *mi)
{
  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];
  obj *o;
  obj cur;
  GLfloat s;
  int i;
  GLfloat r = ease (EASE_IN_OUT_SINE, bp->ratio);

  cur.id = -1;
# undef R
# define R(F) cur.F = bp->from.F + (bp->to.F - bp->from.F) * r
  R(pos.x);
  R(pos.y);
  R(pos.z);
  R(rot.x);
  R(rot.y);
  R(rot.z);
  R(scale);
# undef R

  if (MI_IS_WIREFRAME(mi))
    {
      glColor3f (0.5, 1, 0.5);
      glClearColor (0, 0, 0, 0);
    }

  s = 1 / cur.scale;
  glScalef (s, s, s);

  glRotatef (cur.rot.x * 360, 1, 0, 0);
  glRotatef (cur.rot.y * 360, 0, 1, 0);
  glRotatef (cur.rot.z * 360, 0, 0, 1);

  glTranslatef (cur.pos.x, cur.pos.y, cur.pos.z);

  if (bp->dead)
    {
      glPushMatrix();
      o = bp->dead;
      s = 1 / o->scale;

      glScalef (s, s, s);

      glRotatef (o->rot.x * 360, 1, 0, 0);
      glRotatef (o->rot.y * 360, 0, 1, 0);
      glRotatef (o->rot.z * 360, 0, 0, 1);

      /* Reverse order */
      glTranslatef (o->pos.x, o->pos.y, o->pos.z);

      draw_obj (mi, o);
      glPopMatrix();
    }

  for (o = bp->objs, i = 0; o; o = o->next, i++)
    {
      s = o->scale;
      draw_obj (mi, o);
      if (!o->next) break;

      /* Reverse order */
      glTranslatef (-o->pos.x, -o->pos.y, -o->pos.z);

      glRotatef (-o->rot.z * 360, 0, 0, 1);
      glRotatef (-o->rot.y * 360, 0, 1, 0);
      glRotatef (-o->rot.x * 360, 1, 0, 0);

      glScalef (s, s, s);
    }
}


static void
tick_objs (ModeInfo *mi)
{
  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];
  int L = 0;
  int i;
  obj *o, *last = NULL;

  for (o = bp->objs; o; o = o->next)
    {
      int max = 22;
      last = o;
      L++;

      if (o->jaw_pos >= 0)
        {
          o->jaw_pos += o->chatter;
          if (o->jaw_pos < 0)
            {
              o->jaw_pos = 0;
              o->chatter = -o->chatter;
            }
          else if (o->jaw_pos > max)
            {
              o->jaw_pos = max;
              o->chatter = -o->chatter;
            }
        }
    }

  for (i = 0; i < length_arg - L; i++)
    {
      obj *o = (obj *) calloc (sizeof(*o), 1);
      GLfloat n = frand(1.0);

      o->jaw_pos = frand(20);
      o->chatter = frand(1) * speed;
      if (! (random() % 3))
        o->chatter *= 5;

      if (bp->last_id < 2) n = 0.66;

      if (n < 0.50)
        {						/* Corinthian */
          o->scale = 0.15 * (1 + frand(0.2));
          o->pos.x = 0.22 * (random() & 1 ? 1 : -1);
          o->pos.y = -0.03;
          o->pos.z = -0.4;
          o->rot.z = 0.2 - frand(0.4);

          if (! (random() % 10))
            o->rot.z = frand(0.5) - 1;
        }
      else if (n < 0.65)
        {						/* Nose */
          o->scale = 0.15 * (1 + frand(0.2));
          o->pos.x = 0;
          o->pos.y = 0.12;
          o->pos.z = -0.55;
        }
      else if (n < 0.80)				/* Zeiram (1991) */
        {
          o->scale = 0.15 * (1 + frand(0.2));
          o->pos.x = 0;
          o->pos.y = -0.27;
          o->pos.z = -0.53;
          o->rot.x = 0.1;
        }
      else if (n < 0.85)				/* Malignant (2021) */
        {
          o->scale = 1;
          o->pos.x = 0;
          o->pos.y = 0;
          o->pos.z = 0.4;
          o->rot.y = 0.5;
          if (! (random() % 10))
            o->rot.z = -0.5;
        }
      else if (n < 0.97)				/* Grille */
        {
          o->scale = 0.067;
          o->pos.y = 0.40;
          if (random() & 1)				/* Incisor */
            {
              o->pos.x = 0.028 * (random() & 1 ? 1 : -1);
              o->pos.z = -0.565;
            }
          else						/* Lateral */
            {
              o->pos.x = 0.078 * (random() & 1 ? 1 : -1);
              o->rot.y = 0.09 * (o->pos.x > 0 ? 1 : -1);
              o->pos.z = -0.56;
            }
        }
      else
        {						/* Cymothoa exigua */
          o->scale = 0.28;
          o->pos.x = 0;
          o->pos.y = 0.42;
          o->pos.z = -0.28;
          o->jaw_pos = 22;
          o->chatter = 0;
        }

      if (! (random() % 20))
        {
          o->jaw_pos = -1;
          if (! (random() % 7))
            o->jaw_pos = -2;
        }

      o->id = ++bp->last_id;

      if (o->rot.x > 0.5) o->rot.x = 1 - o->rot.x;
      if (o->rot.y > 0.5) o->rot.y = 1 - o->rot.y;
      if (o->rot.z > 0.5) o->rot.z = 1 - o->rot.z;

      if (last)
        {
          last->next = o;
          last = o;
        }
      else
        {
          bp->objs = last = o;
        }
    }

  bp->ratio += 0.02 * speed;
  if (bp->ratio > 1)
    {
      /* The first object on the list has moved to unit origin position. */
      bp->ratio = 0;
      o = bp->objs;
      bp->objs = o->next;
      if (bp->dead)
        free (bp->dead);
      bp->dead = o;
      bp->from.scale = 1;
    }
  bp->to = *bp->objs;
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_skulloop (ModeInfo *mi, int width, int height)
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
skulloop_handle_event (ModeInfo *mi, XEvent *event)
{
  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void 
init_skulloop (ModeInfo *mi)
{
  skulloop_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_skulloop (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (length_arg < 2)
    length_arg = 2;

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.7, 0.4, 0.0};
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

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;
      GLfloat spec[4] = {0.4, 0.4, 0.4, 1.0};
      GLfloat shiny = 80; /* 0-128 */

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case SKULL_HALF:
      case JAW_HALF:
        key = "skullColor";
        break;
      case TEETH_UPPER_HALF:
      case TEETH_LOWER_HALF:
        key = "teethColor";
        break;
      default:
        abort();
      }

      parse_color (mi, key, bp->component_colors[i]);

      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
      glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);
      renderList (gll, wire);

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

  bp->from.scale = 100;  /* At startup, zoom in from a point */

  bp->font = load_texture_font (MI_DISPLAY(mi), "font");
}


ENTRYPOINT void
draw_skulloop (ModeInfo *mi)
{
  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  gltrackball_rotate (bp->trackball);

  glRotatef (-2, 0, 1, 0);    /* Hide the glitchy left/right model seam. */
  glScalef (16, 16, 16);
  glTranslatef (0, 0.08, 0);

  mi->polygon_count = 0;
  if (!bp->button_down_p)
    tick_objs (mi);
  draw_objs (mi);
  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_skulloop (ModeInfo *mi)
{
  skulloop_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->font) free_texture_font (bp->font);

  if (bp->dlists) {
    for (i = 0; i < countof(all_objs); i++)
      if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
    free (bp->dlists);
  }

  while (bp->objs)
    {
      obj *o = bp->objs;
      bp->objs = bp->objs->next;
      free (o);
    }
  if (bp->dead)
    free (bp->dead);
}

XSCREENSAVER_MODULE ("Skulloop", skulloop)

#endif /* USE_GL */
