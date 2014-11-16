/* polyhedra, Copyright (c) 2004-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Renders 160 different 3D solids, and displays some information about each.
 *  A new solid is chosen every few seconds.
 *
 * This file contains the OpenGL side; computation of the polyhedra themselves
 * is in "polyhedra.c".
 */

#define DEFAULTS	"*delay:	30000         \n" \
			"*showFPS:      False         \n" \
			"*wireframe:    False         \n" \
	"*titleFont:  -*-helvetica-medium-r-normal-*-*-140-*-*-*-*-*-*\n" \
	"*titleFont2: -*-helvetica-medium-r-normal-*-*-100-*-*-*-*-*-*\n" \
	"*titleFont3: -*-helvetica-medium-r-normal-*-*-80-*-*-*-*-*-*\n"  \


# define refresh_polyhedra 0
# define release_polyhedra 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else
# include <X11/Xlib.h>
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_TITLES      "True"
#define DEF_DURATION    "12"
#define DEF_WHICH       "random"

#include "texfont.h"
#include "normals.h"
#include "polyhedra.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include "teapot.h"

#ifndef HAVE_COCOA
# define XK_MISCELLANY
# include <X11/keysymdef.h>
#endif

#ifndef HAVE_JWZGLES
# define HAVE_TESS
#endif


#ifdef USE_GL /* whole file */

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int npolyhedra;
  polyhedron **polyhedra;

  int which;
  int change_to;
  GLuint object_list;

  int mode;  /* 0 = normal, 1 = out, 2 = in */
  int mode_tick;

  int ncolors;
  XColor *colors;

  texture_font_data *font1_data, *font2_data, *font3_data;

  time_t last_change_time;
  int change_tick;

} polyhedra_configuration;

static polyhedra_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static Bool do_titles;
static int duration;
static int do_which;
static char *do_which_str;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-titles", ".titles", XrmoptionNoArg, "True" },
  { "+titles", ".titles", XrmoptionNoArg, "False" },
  { "-duration",".duration",XrmoptionSepArg, 0 },
  { "-which",  ".which",   XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,    t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER,  t_Bool},
  {&do_titles, "titles", "Titles", DEF_TITLES,  t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,   t_Float},
  {&duration,"duration","Duration",DEF_DURATION,t_Int},
  {&do_which_str,"which", "Which", DEF_WHICH,   t_String},
};

ENTRYPOINT ModeSpecOpt polyhedra_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Calculate the normals at each vertex of a face, and use the sum to
   decide which normal to assign to the entire face.  This also solves
   problems caused by nonconvex faces, in most (but not all) cases.
 */
static void
kludge_normal (int n, const int *indices, const point *points)
{
  XYZ normal = { 0, 0, 0 };
  XYZ p = { 0, 0, 0 };
  int i;

  for (i = 0; i < n; ++i) {
    int i1 = indices[i];
    int i2 = indices[(i + 1) % n];
    int i3 = indices[(i + 2) % n];
    XYZ p1, p2, p3;

    p1.x = points[i1].x; p1.y = points[i1].y; p1.z = points[i1].z;
    p2.x = points[i2].x; p2.y = points[i2].y; p2.z = points[i2].z;
    p3.x = points[i3].x; p3.y = points[i3].y; p3.z = points[i3].z;

    p = calc_normal (p1, p2, p3);
    normal.x += p.x;
    normal.y += p.y;
    normal.z += p.z;
  }

  /*normalize(&normal);*/
  if (normal.x == 0 && normal.y == 0 && normal.z == 0) {
    glNormal3f (p.x, p.y, p.z);
  } else {
    glNormal3f (normal.x, normal.y, normal.z);
  }
}


static void
load_fonts (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  bp->font1_data = load_texture_font (mi->dpy, "titleFont");
  bp->font2_data = load_texture_font (mi->dpy, "titleFont2");
  bp->font3_data = load_texture_font (mi->dpy, "titleFont3");
}



static void
startup_blurb (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  const char *s = "Computing polyhedra...";
  texture_font_data *f = bp->font1_data;

  glColor3f (0.8, 0.8, 0);
  print_texture_label (mi->dpy, f,
                       mi->xgwa.width, mi->xgwa.height,
                       0, s);
  glFinish();
  glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}



/* Window management, etc
 */
ENTRYPOINT void
reshape_polyhedra (ModeInfo *mi, int width, int height)
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
polyhedra_handle_event (ModeInfo *mi, XEvent *event)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      bp->change_to = -1;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        bp->change_to = random() % bp->npolyhedra;
      else if (c == '>' || c == '.' || c == '+' || c == '=' ||
               keysym == XK_Right || keysym == XK_Up || keysym == XK_Next)
        bp->change_to = (bp->which + 1) % bp->npolyhedra;
      else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' ||
               keysym == XK_Left || keysym == XK_Down || keysym == XK_Prior)
        bp->change_to = (bp->which + bp->npolyhedra - 1) % bp->npolyhedra;
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        goto DEF;

      if (bp->change_to != -1)
        return True;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
    DEF:
      bp->change_to = random() % bp->npolyhedra;
      return True;
    }

  return False;
}


static void
draw_label (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  polyhedron *p = bp->which >= 0 ? bp->polyhedra[bp->which] : 0;
  char label[1024];
  char name2[255];
  GLfloat color[4] = { 0.8, 0.8, 0.8, 1 };
  texture_font_data *f;

  if (!p || !do_titles) return;

  strcpy (name2, p->name);
  if (*p->class)
    sprintf (name2 + strlen(name2), "  (%s)", p->class);

  sprintf (label,
           "Polyhedron %d:   \t%s\n\n"
           "Wythoff Symbol:\t%s\n"
           "Vertex Configuration:\t%s\n"
           "Symmetry Group:\t%s\n"
        /* "Dual of:              \t%s\n" */
           "\n"
           "Faces:\t  %d\n"
           "Edges:\t  %d\n"
           "Vertices:\t  %d\n"
           "Density:\t  %d\n"
           "Euler:\t%s%d\n",
           bp->which, name2, p->wythoff, p->config, p->group,
        /* p->dual, */
           p->logical_faces, p->nedges, p->logical_vertices,
           p->density, (p->chi < 0 ? "" : "  "), p->chi);

  if (MI_WIDTH(mi) >= 500 && MI_HEIGHT(mi) >= 375)
    f = bp->font1_data;
  else if (MI_WIDTH(mi) >= 350 && MI_HEIGHT(mi) >= 260)
    f = bp->font2_data;				       /* small font */
  else
    f = bp->font3_data;				       /* tiny font */

  glColor4fv (color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
  print_texture_label (mi->dpy, f,
                       mi->xgwa.width, mi->xgwa.height,
                       1, label);
}


#ifdef HAVE_TESS
static void
tess_error (GLenum errorCode)
{
  fprintf (stderr, "%s: tesselation error: %s\n",
           progname, gluErrorString(errorCode));
  abort();
}
#endif /* HAVE_TESS */


static void
new_polyhedron (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  polyhedron *p;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  /* Use the GLU polygon tesselator so that nonconvex faces are displayed
     correctly (e.g., for the "pentagrammic concave deltohedron").
   */
# ifdef HAVE_TESS
  GLUtesselator *tobj = gluNewTess();
  gluTessCallback (tobj, GLU_TESS_BEGIN,  (void (*) (void)) &glBegin);
  gluTessCallback (tobj, GLU_TESS_END,    (void (*) (void)) &glEnd);
  gluTessCallback (tobj, GLU_TESS_VERTEX, (void (*) (void)) &glVertex3dv);
  gluTessCallback (tobj, GLU_TESS_ERROR,  (void (*) (void)) &tess_error);
# endif /* HAVE_TESS */

  mi->polygon_count = 0;

  bp->ncolors = 128;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_random_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        True, False, 0, False);

  if (do_which >= bp->npolyhedra)
    do_which = -1;

  bp->which = (bp->change_to != -1 ? bp->change_to :
               do_which >= 0 ? do_which :
               (random() % bp->npolyhedra));
  bp->change_to = -1;
  p = bp->polyhedra[bp->which];

  if (wire)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glNewList (bp->object_list, GL_COMPILE);
  if (bp->which == bp->npolyhedra-1)
    {
      GLfloat bcolor[4];
      bcolor[0] = bp->colors[0].red   / 65536.0;
      bcolor[1] = bp->colors[0].green / 65536.0;
      bcolor[2] = bp->colors[0].blue  / 65536.0;
      bcolor[3] = 1.0;
      if (wire)
        glColor3f (0, 1, 0);
      else
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bcolor);

      glScalef (0.8, 0.8, 0.8);
      p->nfaces = unit_teapot (6, wire);
      p->nedges = p->nfaces * 3 / 2;
      p->npoints = p->nfaces * 3;
      p->logical_faces = p->nfaces;
      p->logical_vertices = p->npoints;
    }
  else
    {
      glFrontFace (GL_CCW);
      for (i = 0; i < p->nfaces; i++)
        {
          int j;
          face *f = &p->faces[i];

          if (f->color > 64 || f->color < 0) abort();
          if (wire)
            glColor3f (0, 1, 0);
          else
            {
              GLfloat bcolor[4];
              bcolor[0] = bp->colors[f->color].red   / 65536.0;
              bcolor[1] = bp->colors[f->color].green / 65536.0;
              bcolor[2] = bp->colors[f->color].blue  / 65536.0;
              bcolor[3] = 1.0;
              glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bcolor);
            }

          kludge_normal (f->npoints, f->points, p->points);
      
# ifdef HAVE_TESS
          gluTessBeginPolygon (tobj, 0);
          gluTessBeginContour (tobj);
          for (j = 0; j < f->npoints; j++)
            {
              point *pp = &p->points[f->points[j]];
              gluTessVertex (tobj, &pp->x, &pp->x);
            }
          gluTessEndContour (tobj);
          gluTessEndPolygon (tobj);
# else  /* !HAVE_TESS */
          glBegin (wire ? GL_LINE_LOOP :
                   f->npoints == 3 ? GL_TRIANGLES :
                   f->npoints == 4 ? GL_QUADS :
                   GL_POLYGON);
          for (j = 0; j < f->npoints; j++)
            {
              point *pp = &p->points[f->points[j]];
              glVertex3f (pp->x, pp->y, pp->z);
            }
          glEnd();
# endif /* !HAVE_TESS */
        }
    }
  glEndList ();

  mi->polygon_count += p->nfaces;
# ifdef HAVE_TESS
  gluDeleteTess (tobj);
# endif
}


static void
construct_teapot (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  int n = bp->npolyhedra-1;
  polyhedron *p = (polyhedron *) calloc (1, sizeof(*p));
  p->number = n;
  p->wythoff = strdup("X00398|1984");
  p->name = strdup("Teapot");
  p->dual = strdup("");
  p->config = strdup("Melitta");
  p->group = strdup("Teapotahedral (Newell[1975])");
  p->class = strdup("Utah Teapotahedron");
  bp->polyhedra[n] = p;
}


ENTRYPOINT void 
init_polyhedra (ModeInfo *mi)
{
  polyhedra_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  MI_IS_WIREFRAME(mi) = 0;
  wire = 0;
# endif

  if (!bps) {
    bps = (polyhedra_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (polyhedra_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->which = -1;
  load_fonts (mi);
  startup_blurb (mi);

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      /* glEnable(GL_CULL_FACE); */

      /* We need two-sided lighting for polyhedra where both sides of
         a face are simultaneously visible (e.g., the "X-hemi-Y-hedrons".)
       */
      glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  {
    double spin_speed   = 2.0;
    double wander_speed = 0.05;
    double spin_accel   = 0.2;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->trackball = gltrackball_init (True);
  }

  bp->npolyhedra = construct_polyhedra (&bp->polyhedra);
  construct_teapot (mi);

  bp->object_list = glGenLists (1);
  bp->change_to = -1;

  {
    int x;
    char c;
    do_which = -1;
    if (!strcasecmp (do_which_str, "random"))
      ;
    else if (1 == sscanf (do_which_str, " %d %c", &x, &c))
      {
        if (x >= 0 && x < bp->npolyhedra) 
          do_which = x;
        else
          fprintf (stderr, 
                   "%s: polyhedron %d does not exist: there are only %d.\n",
                   progname, x, bp->npolyhedra-1);
      }
    else if (*do_which_str)
      {
        char *s;
        for (s = do_which_str; *s; s++)
          if (*s == '-' || *s == '_') *s = ' ';

        for (x = 0; x < bp->npolyhedra; x++)
          if (!strcasecmp (do_which_str, bp->polyhedra[x]->name) ||
              !strcasecmp (do_which_str, bp->polyhedra[x]->class) ||
              !strcasecmp (do_which_str, bp->polyhedra[x]->wythoff) ||
              !strcasecmp (do_which_str, bp->polyhedra[x]->config))
            {
              do_which = x;
              break;
            }
        if (do_which < 0)
          {
            fprintf (stderr, "%s: no such polyhedron: \"%s\"\n",
                     progname, do_which_str);
            exit (1);
          }
      }
  }

  new_polyhedron (mi);
  reshape_polyhedra (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

}


ENTRYPOINT void
draw_polyhedra (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  GLfloat bshiny    = 128.0;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  if (bp->mode == 0 && do_which >= 0 && bp->change_to < 0)
    ;
  else if (bp->mode == 0)
    {
      if (bp->change_to >= 0)
        bp->change_tick = 999, bp->last_change_time = 1;
      if (bp->change_tick++ > 10)
        {
          time_t now = time((time_t *) 0);
          if (bp->last_change_time == 0) bp->last_change_time = now;
          bp->change_tick = 0;
          if (!bp->button_down_p && now - bp->last_change_time >= duration)
            {
              bp->mode = 1;    /* go out */
              bp->mode_tick = 20 / speed;
              bp->last_change_time = now;
            }
        }
    }
  else if (bp->mode == 1)   /* out */
    {
      if (--bp->mode_tick <= 0)
        {
          new_polyhedron (mi);
          bp->mode_tick = 20 / speed;
          bp->mode = 2;  /* go in */
        }
    }
  else if (bp->mode == 2)   /* in */
    {
      if (--bp->mode_tick <= 0)
        bp->mode = 0;  /* normal */
    }
  else
    abort();

  glShadeModel(GL_FLAT);
  glEnable(GL_NORMALIZE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  glScalef (2.0, 2.0, 2.0);

  glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  bspec);
  glMateriali  (GL_FRONT_AND_BACK, GL_SHININESS, bshiny);

  if (bp->mode != 0)
    {
      GLfloat s = (bp->mode == 1
                   ? bp->mode_tick / (20 / speed)
                   : ((20 / speed) - bp->mode_tick + 1) / (20 / speed));
      glScalef (s, s, s);
    }

  glScalef (2, 2, 2);
  glCallList (bp->object_list);
  if (bp->mode == 0 && !bp->button_down_p)
    draw_label (mi);    /* print_texture_font can't go inside a display list */

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE ("Polyhedra", polyhedra)

#endif /* USE_GL */
