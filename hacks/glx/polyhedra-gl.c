/* polyhedra, Copyright (c) 2004 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"Polyhedra"
#define HACK_INIT	init_polyhedra
#define HACK_DRAW	draw_polyhedra
#define HACK_RESHAPE	reshape_polyhedra
#define HACK_HANDLE_EVENT polyhedra_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts	xlockmore_opts

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_TITLES      "True"
#define DEF_DURATION    "12"
#define DEF_WHICH       "-1"

#define DEFAULTS	"*delay:	30000         \n" \
			"*showFPS:      False         \n" \
			"*wireframe:    False         \n" \
			"*speed:      " DEF_SPEED    "\n" \
			"*spin:       " DEF_SPIN     "\n" \
			"*wander:     " DEF_WANDER   "\n" \
			"*duration:   " DEF_DURATION "\n" \
			"*which:      " DEF_WHICH    "\n" \
			"*titleFont:  -*-times-bold-r-normal-*-180-*\n" \
			"*titleFont2: -*-times-bold-r-normal-*-120-*\n" \
			"*titleFont3: -*-times-bold-r-normal-*-80-*\n"  \


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include <GL/glu.h>

#include "glxfonts.h"
#include "normals.h"
#include "polyhedra.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

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
  GLuint title_list;

  int mode;  /* 0 = normal, 1 = out, 2 = in */
  int mode_tick;

  int ncolors;
  XColor *colors;

  XFontStruct *xfont1, *xfont2, *xfont3;
  GLuint font1_dlist, font2_dlist, font3_dlist;

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

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Calculate the normals at each vertex of a face, and use the sum to
   decide which normal to assign to the entire face.  This also solves
   problems caused by nonconvex faces, in most (but not all) cases.
 */
static void
kludge_normal (int n, const int *indices, const point *points)
{
  XYZ normal = { 0, 0, 0 };
  XYZ p;
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
  load_font (mi->dpy, "titleFont",  &bp->xfont1, &bp->font1_dlist);
  load_font (mi->dpy, "titleFont2", &bp->xfont2, &bp->font2_dlist);
  load_font (mi->dpy, "titleFont3", &bp->xfont3, &bp->font3_dlist);
}



static void
startup_blurb (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  const char *s = "Computing polyhedra...";
  glColor3f (0.8, 0.8, 0);
  print_gl_string (mi->dpy, bp->xfont1, bp->font1_dlist,
                   mi->xgwa.width, mi->xgwa.height,
                   mi->xgwa.width - (string_width (bp->xfont1, s) + 40),
                   mi->xgwa.height - 10,
                   s);
  glFinish();
  glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}



/* Window management, etc
 */
static void new_label (ModeInfo *mi);

void
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

  /* need to re-render the text when the window size changes */
  new_label (mi);
}


Bool
polyhedra_handle_event (ModeInfo *mi, XEvent *event)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (bp->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      bp->change_to = -1;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        bp->change_to = random() % bp->npolyhedra;
      else if (c == '>' || c == '.' || c == '+' || c == '=')
        bp->change_to = (bp->which + 1) % bp->npolyhedra;
      else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177')
        bp->change_to = (bp->which + bp->npolyhedra - 1) % bp->npolyhedra;

      if (bp->change_to != -1)
        return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      gltrackball_track (bp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
new_label (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  polyhedron *p = bp->which >= 0 ? bp->polyhedra[bp->which] : 0;

  glNewList (bp->title_list, GL_COMPILE);
  if (p && do_titles)
    {
      char label[1024];
      char name2[255];
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

      {
        XFontStruct *f;
        GLuint fl;
        if (MI_WIDTH(mi) >= 500 && MI_HEIGHT(mi) >= 375)
          f = bp->xfont1, fl = bp->font1_dlist;		       /* big font */
        else if (MI_WIDTH(mi) >= 350 && MI_HEIGHT(mi) >= 260)
          f = bp->xfont2, fl = bp->font2_dlist;		       /* small font */
        else
          f = bp->xfont3, fl = bp->font3_dlist;		       /* tiny font */

        glColor3f (0.8, 0.8, 0);
        print_gl_string (mi->dpy, f, fl,
                         mi->xgwa.width, mi->xgwa.height,
                         10, mi->xgwa.height - 10,
                         label);
      }
    }
  glEndList ();
}


static void
tess_error (GLenum errorCode)
{
  fprintf (stderr, "%s: tesselation error: %s\n",
           progname, gluErrorString(errorCode));
  exit (0);
}

static void
new_polyhedron (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  polyhedron *p;
  int wire = MI_IS_WIREFRAME(mi);
  static GLfloat bcolor[4] = {0.0, 0.0, 0.0, 1.0};
  int i;

  /* Use the GLU polygon tesselator so that nonconvex faces are displayed
     correctly (e.g., for the "pentagrammic concave deltohedron").
   */
  GLUtesselator *tobj = gluNewTess();
  gluTessCallback (tobj, GLU_TESS_BEGIN,  (void (*) (void)) &glBegin);
  gluTessCallback (tobj, GLU_TESS_END,    (void (*) (void)) &glEnd);
  gluTessCallback (tobj, GLU_TESS_VERTEX, (void (*) (void)) &glVertex3dv);
  gluTessCallback (tobj, GLU_TESS_ERROR,  (void (*) (void)) &tess_error);

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

  new_label (mi);

  glNewList (bp->object_list, GL_COMPILE);
  for (i = 0; i < p->nfaces; i++)
    {
      int j;
      face *f = &p->faces[i];

      if (f->color > 64 || f->color < 0) abort();
      if (wire)
        glColor3f (0, 1, 0);
      else
        {
          bcolor[0] = bp->colors[f->color].red   / 65536.0;
          bcolor[1] = bp->colors[f->color].green / 65536.0;
          bcolor[2] = bp->colors[f->color].blue  / 65536.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bcolor);
        }

      kludge_normal (f->npoints, f->points, p->points);
      
      gluTessBeginPolygon (tobj, 0);
      gluTessBeginContour (tobj);
      for (j = 0; j < f->npoints; j++)
        {
          point *pp = &p->points[f->points[j]];
          gluTessVertex (tobj, &pp->x, &pp->x);
        }
      gluTessEndContour (tobj);
      gluTessEndPolygon (tobj);
    }
  glEndList ();

  mi->polygon_count += p->nfaces;
  gluDeleteTess (tobj);
}


void 
init_polyhedra (ModeInfo *mi)
{
  polyhedra_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (polyhedra_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (polyhedra_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->which = -1;
  load_fonts (mi);
  startup_blurb (mi);

  reshape_polyhedra (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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
    bp->trackball = gltrackball_init ();
  }

  bp->npolyhedra = construct_polyhedra (&bp->polyhedra);

  bp->object_list = glGenLists (1);
  bp->title_list  = glGenLists (1);
  bp->change_to = -1;

  {
    int x;
    char c;
    do_which = -1;
    if (1 == sscanf (do_which_str, " %d %c", &x, &c))
      do_which = x;
    else if (*do_which_str)
      {
        char *s;
        for (s = do_which_str; *s; s++)
          if (*s == '-' || *s == '_') *s = ' ';

        for (x = 0; x < bp->npolyhedra; x++)
          if (!strcasecmp (do_which_str, bp->polyhedra[x]->name) ||
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
}


void
draw_polyhedra (ModeInfo *mi)
{
  polyhedra_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  static time_t last_time = 0;

  static GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static GLfloat bshiny    = 128.0;

  if (!bp->glx_context)
    return;

  if (bp->mode == 0 && do_which >= 0 && bp->change_to < 0)
    ;
  else if (bp->mode == 0)
    {
      static int tick = 0;

      if (bp->change_to >= 0)
        tick = 999, last_time = 1;
      if (tick++ > 10)
        {
          time_t now = time((time_t *) 0);
          if (last_time == 0) last_time = now;
          tick = 0;
          if (!bp->button_down_p && now - last_time >= duration)
            {
              bp->mode = 1;    /* go out */
              bp->mode_tick = 20 * speed;
              last_time = now;
            }
        }
    }
  else if (bp->mode == 1)   /* out */
    {
      if (--bp->mode_tick <= 0)
        {
          new_polyhedron (mi);
          bp->mode_tick = 20 * speed;
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
                   ? bp->mode_tick / (20 * speed)
                   : ((20 * speed) - bp->mode_tick + 1) / (20 * speed));
      glScalef (s, s, s);
    }

  glScalef (2, 2, 2);
  glCallList (bp->object_list);
  if (bp->mode == 0 && !bp->button_down_p)
    glCallList (bp->title_list);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
