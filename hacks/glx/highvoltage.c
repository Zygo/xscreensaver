/* highvoltage, Copyright © 2024 Jamie Zawinski <jwz@jwz.org>
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
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*count:        7           \n" \
			".background:   #FFFFCC"   "\n" \
			".foreground:   #444422"   "\n" \

# define release_highvoltage 0

#define DEF_SPEED   "1.0"
#define DEF_SPACING "1.0"

#include "xlockmore.h"
#include "gltrackball.h"
#include "normals.h"
#include "tube.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gllist.h"

extern const struct gllist
  *highvoltage_model_tower_a_body,
  *highvoltage_model_tower_a_cables,
  *highvoltage_model_tower_a_cross,
  *highvoltage_model_tower_b_body,
  *highvoltage_model_tower_b_cables,
  *highvoltage_model_tower_b_cross,
  *highvoltage_model_tower_c_body,
  *highvoltage_model_tower_c_cables,
  *highvoltage_model_tower_c_cross,
 /*highvoltage_model_tower_d_body,*/
  *highvoltage_model_tower_d_cables,
  *highvoltage_model_tower_d_cross,
  *highvoltage_model_tower_e_body,
  *highvoltage_model_tower_e_cables,
  *highvoltage_model_tower_e_cross,
  *highvoltage_model_tower_f_body,
  *highvoltage_model_tower_f_cables,
  *highvoltage_model_tower_f_cross,
  *highvoltage_model_tower_g_body,
  *highvoltage_model_tower_g_cables,
  *highvoltage_model_tower_g_cross,
  *highvoltage_model_tower_h_body,
  *highvoltage_model_tower_h_cables,
  *highvoltage_model_tower_h_cross,
 /*highvoltage_model_tower_i_body,*/
  *highvoltage_model_tower_i_cross,
  *highvoltage_model_tower_i_cables,
  *highvoltage_model_tower_j_body,
 /*highvoltage_model_tower_i_cross,*/
  *highvoltage_model_tower_i_cables,
  *highvoltage_model_tower_j_body,
 /*highvoltage_model_tower_j_cross,*/
  *highvoltage_model_tower_j_cables,
  *highvoltage_model_tower_j_connections;

typedef struct {
  const struct gllist * const * const body;
  const struct gllist * const * const cross;
  const struct gllist * const * const cables;
  const struct gllist * const * const connections;
} tower_model;

static const tower_model all_tower_models[] = {
  { &highvoltage_model_tower_a_body,
    &highvoltage_model_tower_a_cross,
    &highvoltage_model_tower_a_cables, NULL },
  { &highvoltage_model_tower_b_body,
    &highvoltage_model_tower_b_cross,
    &highvoltage_model_tower_b_cables, NULL },
  { &highvoltage_model_tower_c_body,
    &highvoltage_model_tower_c_cross,
    &highvoltage_model_tower_c_cables, NULL },
  { NULL,
    &highvoltage_model_tower_d_cross,
    &highvoltage_model_tower_d_cables, NULL },
  { &highvoltage_model_tower_e_body,
    &highvoltage_model_tower_e_cross,
    &highvoltage_model_tower_e_cables, NULL },
  { &highvoltage_model_tower_f_body,
    &highvoltage_model_tower_f_cross,
    &highvoltage_model_tower_f_cables, NULL },
  { &highvoltage_model_tower_g_body,
    &highvoltage_model_tower_g_cross,
    &highvoltage_model_tower_g_cables, NULL },
  { &highvoltage_model_tower_h_body,
    &highvoltage_model_tower_h_cross,
    &highvoltage_model_tower_h_cables, NULL },
  { NULL,
    &highvoltage_model_tower_i_cross,
    &highvoltage_model_tower_i_cables, NULL },
  { &highvoltage_model_tower_j_body,
    NULL,
    &highvoltage_model_tower_j_cables,
    &highvoltage_model_tower_j_connections,
 },
};

#define NTOWERS countof(all_tower_models)
#define MAX_WIRES 20

typedef struct {
  const tower_model * model;
  GLuint list;
  struct { GLfloat x, y, z, w, h, d; } bbox;
  int nwires;
  XYZ wires[MAX_WIRES];
  int polys;
} tower;


typedef struct obj obj;
struct obj {
  XYZ pos, rot;
  obj *next;
};

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  int which;
  tower towers[NTOWERS];
  obj *objs, *dead;
  obj from, to;
  GLfloat ratio;
  enum { FADE_IN, DRAW, FADE_OUT } state;
  Bool left_p;
  GLfloat tick;
  GLfloat fg[4], bg[4];
} highvoltage_configuration;

static highvoltage_configuration *bps = NULL;

static GLfloat speed_arg;
static GLfloat spacing_arg;

static XrmOptionDescRec opts[] = {
  { "-speed",    ".speed",  XrmoptionSepArg, 0 },
  { "-spacing", ".spacing", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed_arg,    "speed",  "Speed",   DEF_SPEED,   t_Float},
  {&spacing_arg, "spacing", "Spacing", DEF_SPACING, t_Float},
};

ENTRYPOINT ModeSpecOpt highvoltage_opts = {
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


ENTRYPOINT void
reshape_highvoltage (ModeInfo *mi, int width, int height)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
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

  {
    GLfloat fovy = 30.0;
    GLfloat aspect = 1/h;
    GLfloat near = 1.0;
    GLfloat far = 28 * spacing_arg * MI_COUNT(mi) * 15;
# if 0
    gluPerspective (fovy, aspect, near, far);
# else
    GLfloat fh = tan (fovy / 360 * M_PI) * near;
    GLfloat fw1 = -fh * aspect;
    GLfloat fw2 = -fw1;

    if (bp->left_p)		/* Adjust horizontal vanishing point */
      {
        fw1 *= 2;
        fw2 = 0;
      }
    else
      {
        fw1 = 0;
        fw2 *= 2;
      }

    glFrustum (fw1, fw2,
               -fh * 0.6, fh,  /* Move horizon closer to bottom of window */
               near, far);
# endif
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt (0, 0, 30,
             0, 3, 0,   /* Look up */
             0, 1, 0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
highvoltage_handle_event (ModeInfo *mi, XEvent *event)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
          bp->state = FADE_OUT;
          return True;
        }
      else if (c == '>' || c == '.' || c == '+' || c == '=' ||
               keysym == XK_Right || keysym == XK_Up || keysym == XK_Next)
        {
          speed_arg += 0.1;
          return True;
        }
      else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' ||
               keysym == XK_Left || keysym == XK_Down || keysym == XK_Prior)
        {
          speed_arg -= 0.1;
          if (speed_arg < 0.1) speed_arg = 0.1;
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        {
          bp->state = FADE_OUT;
          return True;
        }
    }

  return False;
}


/* Draw a gllist using square tubes.
 */
static int
render_lines (ModeInfo *mi, const struct gllist *list, int cable_type)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  if (wire)
    {
      renderList (list, True);
      polys += list->points / 2;
    }
  else if (list->primitive == GL_TRIANGLES)
    {
      renderList (list, wire);
      polys += list->points / 3;
    }
  else
    {
      GLfloat diam = 0.25 * (cable_type == 0 ? 1 :
                             cable_type == 1 ? 0.3 : 0.1);
      GLfloat cap = diam / 2;
      int faces = 4;
      const GLfloat *p = (GLfloat *) list->data;
      int i;
      if (list->primitive != GL_LINES) abort();
      if (list->format != GL_V3F) abort();
      for (i = 0; i < list->points * 3; i += 6)
        {
          if (cable_type == 1 &&
              p[i+2] == p[i+5])  /* Horizontal */
            continue;

          polys += tube (p[i+0], p[i+1], p[i+2],
                         p[i+3], p[i+4], p[i+5],
                         diam, cap, faces, False, False, wire);

          if (cable_type == 1)		/* Add insulator discs */
            {
              GLfloat disc_spacing = 0.5;
              GLfloat disc_height = 0.5;
              GLfloat disc_width = 10;
              GLfloat w = p[i+3] - p[i+0];
              GLfloat h = p[i+4] - p[i+1];
              GLfloat d = p[i+5] - p[i+2];
              GLfloat L = sqrt (w*w + h*h + d*d);
              int ndiscs = L / disc_spacing;
              int j;

              for (j = 0; j < ndiscs; j++)
                {
                  GLfloat r1 = (GLfloat) j / ndiscs;
                  GLfloat r2 = ((GLfloat) j + disc_height) / ndiscs;
                  GLfloat x1 = p[i+0] + w * r1;
                  GLfloat y1 = p[i+1] + h * r1;
                  GLfloat z1 = p[i+2] + d * r1;
                  GLfloat x2 = p[i+0] + w * r2;
                  GLfloat y2 = p[i+1] + h * r2;
                  GLfloat z2 = p[i+2] + d * r2;
                  polys += tube (x1, y1, z1, x2, y2, z2,
                                 diam * disc_width, 0, 6,
                                 True, True, wire);
                }
            }
        }
    }

  return polys;
}


/* Measure and render a tower into display lists, bboxes, etc.
 */
static void
render_tower (ModeInfo *mi, tower *t)
{
  /* highvoltage_configuration *bp = &bps[MI_SCREEN(mi)]; */
  XYZ min[4], max[4];
  int j, k;

  for (k = 0; k < 4; k++)
    {
      min[k].x =  99999;
      min[k].y =  99999;
      min[k].z =  99999;
      max[k].x = -99999;
      max[k].y = -99999;
      max[k].z = -99999;
    }

  /* Find the bounding box of each model. */
  for (k = 0; k < 3; k++)
    {
      const struct gllist * const * L = (k == 0 ? t->model->body :
                                         k == 1 ? t->model->cross :
                                         t->model->cables);
      /* Omit model->connections from the bbox. */ 
      const struct gllist * list = (L ? *L : 0);
      const GLfloat *p = (GLfloat *) list ? list->data : 0;
      int skip, stride;
      if (!p) continue;
      switch (list->format) {
      case GL_V3F: skip = 0; stride = 3; break;
      case GL_C3F_V3F: case GL_N3F_V3F: skip = 3; stride = 6; break;
      default: abort(); break; /* write me */
      }
      for (j = skip; j < skip + list->points * 3; j += stride)
        {
          if (p[j+0] < min[k].x) min[k].x = p[j+0];
          if (p[j+1] < min[k].y) min[k].y = p[j+1];
          if (p[j+2] < min[k].z) min[k].z = p[j+2];
          if (p[j+0] > max[k].x) max[k].x = p[j+0];
          if (p[j+1] > max[k].y) max[k].y = p[j+1];
          if (p[j+2] > max[k].z) max[k].z = p[j+2];
        }

      if (k > 0)	/* cross and cables are mirrored around body center */
        {
          GLfloat r = min[0].x + (max[0].x - min[0].x) / 2;
          for (j = 0; j < list->points * 3; j += 3)
            {
              GLfloat p2[3];
              p2[0] = r - p[j+0];
              p2[1] = p[j+1];
              p2[2] = p[j+2];
              if (p2[0] < min[k].x) min[k].x = p2[0];
              if (p2[1] < min[k].y) min[k].y = p2[1];
              if (p2[2] < min[k].z) min[k].z = p2[2];
              if (p2[0] > max[k].x) max[k].x = p2[0];
              if (p2[1] > max[k].y) max[k].y = p2[1];
              if (p2[2] > max[k].z) max[k].z = p2[2];
            }
        }
      if (min[k].x < min[3].x) min[3].x = min[k].x;
      if (min[k].y < min[3].y) min[3].y = min[k].y;
      if (min[k].z < min[3].z) min[3].z = min[k].z;
      if (max[k].x > max[3].x) max[3].x = max[k].x;
      if (max[k].y > max[3].y) max[3].y = max[k].y;
      if (max[k].z > max[3].z) max[3].z = max[k].z;
    }

  t->bbox.x = min[3].x;
  t->bbox.y = min[3].y;
  t->bbox.z = min[3].z;
  t->bbox.w = max[3].x - min[3].x;
  t->bbox.h = max[3].y - min[3].y;
  t->bbox.d = max[3].z - min[3].z;

  /* Find the attachment points of the wires: the horizontal lines. */
  if (t->model->cables)
    {
      const struct gllist *list = *t->model->cables;
      const GLfloat *p = (GLfloat *) list->data;
      GLfloat cx = t->bbox.x + t->bbox.w / 2;
      GLfloat cy = t->bbox.y + t->bbox.h / 2;

      for (j = 0; j < list->points * 3; j += 6)
        {
          if (p[j+2] == p[j+5])	/* Horizontal line */
            {
              t->wires[t->nwires].x = p[j+0];
              t->wires[t->nwires].y = cy;
              t->wires[t->nwires].z = p[j+2];
              t->nwires++;

              if (p[j+0] != 0)	/* Mirror all but the center ones */
                {
                  t->wires[t->nwires].x = cx - p[j+0];
                  t->wires[t->nwires].y = cy;
                  t->wires[t->nwires].z = p[j+2];
                  t->nwires++;
                }

              if (t->nwires >= MAX_WIRES) abort();
            }
        }
      if (! t->nwires) abort();
    }

  t->list = glGenLists (1);

  glNewList (t->list, GL_COMPILE);
  {
    GLfloat s = 1 / t->bbox.d;

    glPushMatrix();
    glScalef (s, s, s);
      

    glTranslatef (-(t->bbox.x + t->bbox.w / 2),
                  -(t->bbox.y + t->bbox.h / 2),
                  -(t->bbox.z + t->bbox.d / 2));

    if (t->model->body)
      t->polys += render_lines (mi, *t->model->body, 0);
    if (t->model->cross)
      t->polys += render_lines (mi, *t->model->cross, 0);
    if (t->model->cables)
      t->polys += render_lines (mi, *t->model->cables, 1);
    if (t->model->connections)
      t->polys += render_lines (mi, *t->model->connections, 2);

    glPushMatrix();
    glScalef (-1, 1, 1);
    glFrontFace (GL_CW);

    if (t->model->cross)
      t->polys += render_lines (mi, *t->model->cross, 0);
    if (t->model->cables)
      t->polys += render_lines (mi, *t->model->cables, 1);

# if 0	/* Draw the wire attachment points */
    for (j = 0; j < t->nwires; j++)
      {
        glColor3f (1, 1, 0);
        glBegin (GL_LINES);
        glVertex3f (t->wires[j].x - 1, t->wires[j].y, t->wires[j].z - 1);
        glVertex3f (t->wires[j].x + 1, t->wires[j].y, t->wires[j].z + 1);
        glVertex3f (t->wires[j].x + 1, t->wires[j].y, t->wires[j].z - 1);
        glVertex3f (t->wires[j].x - 1, t->wires[j].y, t->wires[j].z + 1);
        glEnd();
        glColor3f (0, 1, 0);
      }


# endif /* */


# if 0  /* Draw bounding box */
    glColor3f (1, 0, 0);
    glBegin (GL_LINE_LOOP);
    glVertex3f (t->bbox.x, t->bbox.y, t->bbox.z);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y, t->bbox.z);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y + t->bbox.h, t->bbox.z);
    glVertex3f (t->bbox.x, t->bbox.y + t->bbox.h, t->bbox.z);
    glEnd();

    glBegin (GL_LINE_LOOP);
    glVertex3f (t->bbox.x, t->bbox.y, t->bbox.z + t->bbox.d);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y, t->bbox.z + t->bbox.d);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y + t->bbox.h, t->bbox.z +
                t->bbox.d);
    glVertex3f (t->bbox.x, t->bbox.y + t->bbox.h, t->bbox.z + t->bbox.d);
    glEnd();

    glBegin (GL_LINES);
    glVertex3f (t->bbox.x, t->bbox.y, t->bbox.z);
    glVertex3f (t->bbox.x, t->bbox.y, t->bbox.z + t->bbox.d);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y, t->bbox.z);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y, t->bbox.z + t->bbox.d);
    glVertex3f (t->bbox.x, t->bbox.y + t->bbox.h, t->bbox.z);
    glVertex3f (t->bbox.x, t->bbox.y + t->bbox.h, t->bbox.z + t->bbox.d);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y + t->bbox.h, t->bbox.z);
    glVertex3f (t->bbox.x + t->bbox.w, t->bbox.y + t->bbox.h, t->bbox.z +
                t->bbox.d);
    glEnd();
    glColor3f (0, 0, 0);
# endif /* 0 */

    glPopMatrix();
    glPopMatrix();
  }
  glEndList ();
}


/* Draw the connecting wires between towers.
 */
static int
draw_wires (ModeInfo *mi, obj *o, obj *p)
{
  int wire = MI_IS_WIREFRAME(mi);
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  tower *t = &bp->towers[bp->which];
  GLfloat cy = t->bbox.y + t->bbox.h / 2;
  GLfloat cz = t->bbox.z + t->bbox.d / 2;
  GLfloat diam = 0.002;
  GLfloat wire_droop = 0.15;	/* How heavy are the power lines */
  int polys = 0;
  int i;

  for (i = 0; i < t->nwires; i++)
    {
      int j, segments = 20;
      XYZ from, to, cur, prev;
      from.x = t->wires[i].x / t->bbox.d;
      from.y = (t->wires[i].y - cy) / t->bbox.d;
      from.z = (t->wires[i].z - cz) / t->bbox.d;
      to.x = from.x + p->pos.x;
      to.y = from.y + p->pos.y;
      to.z = from.z + p->pos.z;

      for (j = 0; j <= segments; j++)
        {
          GLfloat r = (GLfloat) j / segments;
          GLfloat off = sin (M_PI * r) * wire_droop;
          cur.x = from.x + (to.x - from.x) * r;
          cur.y = from.y + (to.y - from.y) * r;
          cur.z = from.z + (to.z - from.z) * r - off;

          if (j > 0)
            {
              if (wire)
                {
                  glBegin (GL_LINES);
                  glVertex3f (prev.x, prev.y, prev.z);
                  glVertex3f (cur.x, cur.y, cur.z);
                  glEnd();
                  polys++;
                }
              else
                {
                  polys += tube (prev.x, prev.y, prev.z,
                                 cur.x, cur.y, cur.z,
                                 diam, 0, 6,
                                 True, False, wire);
                }
            }

          prev = cur;
        }
    }

  return polys;
}



static void
draw_obj (ModeInfo *mi, obj *o, obj *p)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  tower *t = &bp->towers[bp->which];
  glCallList (t->list);
  mi->polygon_count += t->polys;
  if (p)
    mi->polygon_count += draw_wires (mi, o, p);
}


static void
draw_objs (ModeInfo *mi)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  obj *o;
  obj cur;
  obj *prev;
  int i;
  GLfloat r = bp->ratio;

# undef R
# define R(F) cur.F = bp->from.F + (bp->to.F - bp->from.F) * r
  R(pos.x);
  R(pos.y);
  R(pos.z);
  R(rot.x);
  R(rot.y);
  R(rot.z);
# undef R

  glRotatef (cur.rot.x * 360, 1, 0, 0);
  glRotatef (cur.rot.y * 360, 0, 1, 0);
  glRotatef (cur.rot.z * 360, 0, 0, 1);

  glTranslatef (cur.pos.x, cur.pos.y, cur.pos.z);

  if (bp->dead)
    {
      glPushMatrix();
      o = bp->dead;

      glRotatef (o->rot.x * 360, 1, 0, 0);
      glRotatef (o->rot.y * 360, 0, 1, 0);
      glRotatef (o->rot.z * 360, 0, 0, 1);

      /* Reverse order */
      glTranslatef (o->pos.x, o->pos.y, o->pos.z);

      draw_obj (mi, o, NULL);
      glPopMatrix();
    }

  prev = NULL;
  for (o = bp->objs, i = 0; o; o = o->next, i++)
    {
      draw_obj (mi, o, prev);

      if (!o->next) break;

      /* Reverse order */
      glTranslatef (-o->pos.x, -o->pos.y, -o->pos.z);

      glRotatef (-o->rot.z * 360, 0, 0, 1);
      glRotatef (-o->rot.y * 360, 0, 1, 0);
      glRotatef (-o->rot.x * 360, 1, 0, 0);
      prev = o;
    }
}


static void
tick_objs (ModeInfo *mi)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  int L = 0;
  int i;
  obj *o, *last = NULL;

  for (o = bp->objs; o; o = o->next)
    {
      last = o;
      L++;
    }

  for (i = 0; i < MI_COUNT(mi) - L; i++)
    {
      obj *o = (obj *) calloc (sizeof(*o), 1);

      int sign = (random() % 10) ? -1 : 1;	/* Prefer towards inside */
      if (bp->left_p) sign = -sign;
      o->pos.x = sign * frand (1.5);

      o->pos.y = -20 * (1 + frand(0.2)) * spacing_arg;

      /* This makes the motion twitchy, and also the wires don't attach. */
      /* o->rot.z = 0.01 * (random() & 1 ? 1 : -1); */

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

  bp->ratio += 0.0015 * speed_arg;	/* Flight speed */

  if (bp->ratio > 1)
    {
      /* The first object on the list has moved to unit origin position. */
      bp->ratio = 0;
      o = bp->objs;
      bp->objs = o->next;
      if (bp->dead)
        free (bp->dead);
      bp->dead = o;
    }
  bp->to = *bp->objs;
}


static void
free_objs (ModeInfo *mi)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  while (bp->objs)
    {
      obj *o = bp->objs;
      bp->objs = bp->objs->next;
      free (o);
    }
  if (bp->dead)
    free (bp->dead);
  bp->objs = 0;
  bp->dead = 0;
}


/* Pick a new tower
 */
static void
reset (ModeInfo *mi)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  int o = bp->which;
  while (bp->which == o)
    bp->which = random() % NTOWERS;
  bp->left_p = (o == -1 ? (random() & 1) : !bp->left_p);
  free_objs (mi);
  reshape_highvoltage (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  gltrackball_reset (bp->trackball, 0, 0);
}


ENTRYPOINT void
init_highvoltage (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  highvoltage_configuration *bp;
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->trackball = gltrackball_init (True);

  for (i = 0; i < NTOWERS; i++)
    {
      tower *t = &bp->towers[i];
      t->model = &all_tower_models[i];
      render_tower (mi, t);
    }

  parse_color (mi, "foreground", bp->fg);
  parse_color (mi, "background", bp->bg);
  glClearColor (bp->bg[0], bp->bg[1], bp->bg[2], bp->bg[3]);

  bp->state = FADE_IN;
  bp->tick = 0;

  glShadeModel (GL_FLAT);

  /* None of these matter; unclear if there is a performance impact. */
  /* glEnable (GL_DEPTH_TEST); */
  /* glEnable (GL_NORMALIZE); */
  /* glEnable (GL_CULL_FACE); */

  if (!wire)
    {
      GLfloat fog_color[4] = { 1, 1, 1, 1 };
      glFogi (GL_FOG_MODE, GL_EXP2);
      glFogfv (GL_FOG_COLOR, fog_color);
      glFogf (GL_FOG_DENSITY, 0.001);
      glEnable (GL_FOG);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);
    }

  bp->which = -1;
  reset (mi);
}


ENTRYPOINT void
draw_highvoltage (ModeInfo *mi)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!(random() % (30*60*10)))   /* Change it up every 10 minutes or so */
    bp->state = FADE_OUT;

  switch (bp->state) {
  case FADE_IN:
    bp->tick += 0.01 * speed_arg;
    if (bp->tick >= 1.0)
      {
        bp->tick = 1;
        bp->state = DRAW;
      }
    break;
  case DRAW:
    break;
  case FADE_OUT:
    bp->tick -= 0.05 * speed_arg;
    if (bp->tick <= 0)
      {
        bp->tick = 0;
        bp->state = FADE_IN;
        reset (mi);
      }
    break;
  default:
    abort();
  }

  glPushMatrix ();
  glRotatef (current_device_rotation(), 0, 0, 1);

  glScalef (15, 15, 15);
  gltrackball_rotate (bp->trackball);
  glRotatef (-90, 1, 0, 0);

  /* Place the tower feet a bit below the horizon plane. */
  glTranslatef (0, 0, 0.4);

  /* Zoom in to avoid wires dropping out. */
  glTranslatef (0, -4 * spacing_arg, 0);

  /* Move off of the center line. */
  glTranslatef (0.6 * (bp->left_p ? -1 : 1), 0, 0);

  if (bp->towers[bp->which].model->connections)  /* Street pole */
    {
      /* More oblique viewing angle. */
      glTranslatef (0.6 * (bp->left_p ? -1 : 1), 0, 0);

      /* Point the low voltage wires toward the closer window edge. */
      if (bp->left_p) glScalef (-1, 1, 1);
    }

  mi->polygon_count = 0;

  {
    GLfloat c[4];
    c[0] = bp->bg[0] * (1 - bp->tick) + bp->fg[0] * bp->tick;
    c[1] = bp->bg[1] * (1 - bp->tick) + bp->fg[1] * bp->tick;
    c[2] = bp->bg[2] * (1 - bp->tick) + bp->fg[2] * bp->tick;
    c[3] = bp->fg[3];
    glColor3fv (c);
    glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
  }

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
free_highvoltage (ModeInfo *mi)
{
  highvoltage_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);

  for (i = 0; i < NTOWERS; i++)
    {
      tower *t = &bp->towers[bp->which];
      if (glIsList(t->list)) glDeleteLists(t->list, 1);
    }
  free_objs (mi);
}

XSCREENSAVER_MODULE ("HighVoltage", highvoltage)

#endif /* USE_GL */
