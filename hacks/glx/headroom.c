/* headroom, Copyright (c) 2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Well, it's supposed to be Max Headroom, but I have so far been unable to
 * find or commission a decent 3D model of Max.  So it's a skull instead.
 * This will have to do for now.
 *
 * Created 29-Nov-2020.
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*showFPS:      False       \n" \
			"*skullColor:   #777777"   "\n" \
			"*teethColor:   #FFFFFF"   "\n" \
			"*torsoColor:   #447744"   "\n" \
			"*torsoCapColor:#222222"   "\n" \
			"*gridColor1:   #AA0000"   "\n" \
			"*gridColor2:   #00FF00"   "\n" \
			"*gridColor3:   #6666FF"   "\n" \
			"*wireframe:    False       \n"

# define release_headroom 0

#define DEF_SPEED       "1.0"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "False"

#include "xlockmore.h"

#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gltrackball.h"
#include "rotator.h"
#include "gllist.h"

extern const struct gllist
  *headroom_model_skull_half, *headroom_model_jaw_half,
  *headroom_model_teeth_upper_half, *headroom_model_teeth_lower_half,
  *headroom_model_torso_half, *headroom_model_torso_cap_half;

static const struct gllist * const *all_objs[] = {
  &headroom_model_skull_half, &headroom_model_jaw_half,
  &headroom_model_teeth_upper_half, &headroom_model_teeth_lower_half,
  &headroom_model_torso_half, &headroom_model_torso_cap_half,
};

enum { SKULL_HALF, JAW_HALF, TEETH_UPPER_HALF, TEETH_LOWER_HALF, TORSO_HALF,
       TORSO_CAP_HALF };

typedef struct { GLfloat x, y, z; } XYZ;

typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2, *rot3;
  trackball_state *trackball;
  Bool button_down_p;
  Bool spinx, spiny, spinz;

  XYZ head_pos;
  GLfloat jaw_pos;
  GLfloat grid_colors[3][4];

  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];
} headroom_configuration;

static headroom_configuration *bps = NULL;

static GLfloat speed;
static char *do_spin;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-speed",   ".speed",     XrmoptionSepArg, 0 },
  { "-spin",    ".spin",      XrmoptionSepArg, 0 },
  { "+spin",    ".spin",      XrmoptionNoArg, "" },
  { "-wander",  ".wander",    XrmoptionNoArg, "True" },
  { "+wander",  ".wander",    XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&speed,       "speed",      "Speed",     DEF_SPEED,      t_Float},
  {&do_spin,      "spin",      "Spin",      DEF_SPIN,       t_String},
  {&do_wander,    "wander",    "Wander",    DEF_WANDER,     t_Bool},
};

ENTRYPOINT ModeSpecOpt headroom_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_headroom (ModeInfo *mi, int width, int height)
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
  gluPerspective (30, 1/h, 1, 500);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 30,
             0, 0, 0,
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
headroom_handle_event (ModeInfo *mi, XEvent *event)
{
  headroom_configuration *bp = &bps[MI_SCREEN(mi)];

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


ENTRYPOINT void 
init_headroom (ModeInfo *mi)
{
  headroom_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];
  bp->glx_context = init_GL(mi);
  reshape_headroom (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
/*      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};*/
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

      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }


  {
    double spin_speed   = 0.5;
    double wander_speed = 0.002 * speed;
    double tilt_speed   = 0.005 * speed;
    double spin_accel   = 0.5;

    double spin_speed_2 = 0.2 * speed;
    double spin_accel_2 = 0.2;
    double wander_speed_2 = 0.01 * speed;

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
    bp->rot2 = make_rotator (0, 0, 0, 0, tilt_speed, True);

    bp->rot3 = make_rotator (spin_speed_2, spin_speed_2, spin_speed_2,
                             spin_accel_2, wander_speed_2, True);

    bp->trackball = gltrackball_init (False);
  }

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
      case TORSO_HALF:
        key = "torsoColor";
        break;
      case TORSO_CAP_HALF:
        key = "torsoCapColor";
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

  parse_color (mi, "gridColor1", bp->grid_colors[0]);
  parse_color (mi, "gridColor2", bp->grid_colors[1]);
  parse_color (mi, "gridColor3", bp->grid_colors[2]);

  reshape_headroom (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


static int
draw_unit_panel (ModeInfo *mi, GLfloat *color1, GLfloat *color2)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int rows = 36;
  GLfloat spacing = 1.0 / rows;
  GLfloat thickness = spacing / 8;
  int i;
  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_QUADS);
  glNormal3f (0, 0, 1);
  for (i = 0; i < rows; i++)
    {
      GLfloat y = i / (GLfloat) rows + spacing/2;
      glColor4fv (color2);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color2);
      glVertex3f (1, y, 0);
      glColor4fv (color1);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color1);
      glVertex3f (0, y, 0);
      glVertex3f (0, y + thickness, 0);
      glColor4fv (color2);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color2);
      glVertex3f (1, y + thickness, 0);
      polys++;
    }
  glEnd();
  return polys;
}

static int
draw_box (ModeInfo *mi)
{
  headroom_configuration *bp = &bps[MI_SCREEN(mi)];
  int polys = 0;
  GLfloat *c0 = bp->grid_colors[0];
  GLfloat *c1 = bp->grid_colors[1];
  GLfloat *c2 = bp->grid_colors[2];

  glPushMatrix();
  glTranslatef (-0.5, -0.5, 0.5);
  polys += draw_unit_panel (mi, c0, c1);
  glRotatef (-90, 0, 1, 0);
  glTranslatef (-1, 0, 0);
  polys += draw_unit_panel (mi, c1, c0);
  glRotatef (-90, 0, 1, 0);
  glTranslatef (-1, 0, 0);
  polys += draw_unit_panel (mi, c0, c1);
  glRotatef (-90, 0, 1, 0);
  glTranslatef (-1, 0, 0);
  polys += draw_unit_panel (mi, c1, c0);

  glRotatef (-90, 1, 0, 0);
  glTranslatef (0, 0, 1);
  polys += draw_unit_panel (mi, c2, c1);
  glRotatef (-180, 1, 0, 0);
  glTranslatef (0, -1, 1);
  polys += draw_unit_panel (mi, c1, c2);


  glPopMatrix();
  return polys;
}



static int
draw_component (ModeInfo *mi, int i)
{
  headroom_configuration *bp = &bps[MI_SCREEN(mi)];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);

  glFrontFace (GL_CCW);
  glCallList (bp->dlists[i]);

  glPushMatrix();
  glScalef (-1, 1, 1);
  glFrontFace (GL_CW);
  glCallList (bp->dlists[i]);
  glPopMatrix();

  return 2 * (*all_objs[i])->points / 3;
}


ENTRYPOINT void
draw_headroom (ModeInfo *mi)
{
  headroom_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat s;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);

  mi->polygon_count = 0;

  {
    double x, y, z;
    get_rotation (bp->rot3, &x, &y, &z, !bp->button_down_p);

    glPushMatrix();
    glRotatef (x * 360, 1, 0, 0);
    glRotatef (y * 360, 0, 1, 0);
    glRotatef (z * 360, 0, 0, 1);

    get_position (bp->rot3, &x, &y, &z, !bp->button_down_p);
    s = 3;
    glScalef (1+x*s, 1+y*s, 1+z*s);

    s = 60;
    glScalef (s, s, s);

    if (!wire) glDisable(GL_LIGHTING);
    mi->polygon_count += draw_box (mi);
    if (!wire) glEnable(GL_LIGHTING);
    glPopMatrix();
    glClear(GL_DEPTH_BUFFER_BIT);
  }

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 6,
                 (z - 0.5) * 10);

    gltrackball_rotate (bp->trackball);

    {
      double maxx = 40;
      double maxy = 20;
      double maxz = 2;
      get_position (bp->rot2, &x, &y, &z, !bp->button_down_p);
      if (bp->spinx) glRotatef (maxy/2 - x*maxy, 1, 0, 0);
      if (bp->spiny) glRotatef (maxx/2 - y*maxx, 0, 1, 0);
      if (bp->spinz) glRotatef (maxz/2 - z*maxz, 0, 0, 1);
    }
  }

  glTranslatef (0, -6, 0);
  s = 0.03;

  glScalef (s, s, s);

  {
    /* head x, nod: -50 to +30
       head y, turn: +- 70
       head z, tilt: +- 30
       jaw, open: 0 - 22
     */
    const XYZ head_base = { 0, 200, 0 };
    const XYZ jaw_base  = { 0, 270, 40 };

    mi->polygon_count += draw_component (mi, TORSO_HALF);
    mi->polygon_count += draw_component (mi, TORSO_CAP_HALF);

    glTranslatef (head_base.x, head_base.y, head_base.z);
    glRotatef (bp->head_pos.x, 1, 0, 0);
    glRotatef (bp->head_pos.y, 0, 1, 0);
    glRotatef (bp->head_pos.z, 0, 0, 1);
    glTranslatef (-head_base.x, -head_base.y, -head_base.z);

    mi->polygon_count += draw_component (mi, SKULL_HALF);
    mi->polygon_count += draw_component (mi, TEETH_UPPER_HALF);

    glTranslatef (jaw_base.x, jaw_base.y, jaw_base.z);
    glRotatef (bp->jaw_pos, 1, 0, 0);
    glTranslatef (-jaw_base.x, -jaw_base.y, -jaw_base.z);

    mi->polygon_count += draw_component (mi, JAW_HALF);
    mi->polygon_count += draw_component (mi, TEETH_LOWER_HALF);

    glFrontFace (GL_CCW);
  }

  if (! bp->button_down_p)
    {
      int twitch = 200 / speed;
      int untwitch = 50 / speed;
      if (twitch   < 10) twitch = 10;
      if (untwitch < 5)  untwitch = 5;

      if (! (random() % twitch))
        bp->head_pos.x = -20.0 + (random() % (20 + 30));
      if (! (random() % twitch))
        bp->head_pos.y = -50.0 + (random() % (50 * 2));
      if (! (random() % twitch))
        bp->head_pos.z = -30.0 + (random() % (30 * 2));
      if (! (random() % twitch))
        bp->jaw_pos = (random() % 22);

      if (! (random() % untwitch))
        {
          bp->head_pos.x = 0;
          bp->head_pos.y = 0;
          bp->head_pos.z = 0;
          bp->jaw_pos = 0;
        }
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_headroom (ModeInfo *mi)
{
  headroom_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->rot2) free_rotator (bp->rot2);
  if (bp->rot3) free_rotator (bp->rot3);
  if (bp->dlists) {
    for (i = 0; i < countof(all_objs); i++)
      if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
    free (bp->dlists);
  }
}

XSCREENSAVER_MODULE ("Headroom", headroom)

#endif /* USE_GL */
