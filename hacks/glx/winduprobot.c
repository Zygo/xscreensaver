/* winduprobot, Copyright © 2014-2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws a robot wind-up toy.
 *
 * I've had this little robot since I was about six years old!  When the time
 * came for us to throw the Cocktail Robotics Grand Challenge at DNA Lounge, I
 * photographed this robot (holding a tiny martini glass) to make a flyer for
 * the event.  You can see that photo here:
 * https://www.dnalounge.com/flyers/2014/09/14.html
 *
 * Then I decided to try and make award statues for the contest by modeling
 * this robot and 3D-printing it (a robot on a post, with the DNA Lounge
 * grommet around it.)  So I learned Maya and built a model.
 *
 * Well, that 3D printing idea didn't work out, but since I had the model
 * already, I exported it and turned it into a screen saver.
 *
 * The DXF files that Maya exports aren't simple enough for my dxf2gl.pl
 * script to process, so the exporting process went:
 *
 *  - Save from Maya to OBJ;
 *  - Import OBJ into SketchUp using
 *    http://www.scriptspot.com/sketchup/scripts/obj-importer
 *  - Clean up the model a little bit more;
 *  - Export to DXF with "Millimeters", "Triangles", using
 *    http://www.guitar-list.com/download-software/convert-sketchup-skp-files-dxf-or-stl
 *
 * We did eventually end up with robotic award statues, but we constructed
 * them out of mass-produced wind-up robots, rather than 3D printing them:
 * https://www.dnalounge.com/gallery/2014/09-14/045.html
 * https://www.youtube.com/watch?v=EZF4ZAAy49g
 */

#define LABEL_FONT "sans-serif bold 24"

#define DEFAULTS	"*delay:	20000       \n" \
			"*count:        25          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*labelFont:  " LABEL_FONT "\n" \
			"*legColor:     #AA2222"   "\n" \
			"*armColor:     #AA2222"   "\n" \
			"*handColor:    #AA2222"   "\n" \
			"*crankColor:   #444444"   "\n" \
			"*bodyColor:    #7777AA"   "\n" \
			"*domeColor:    #7777AA"   "\n" \
			"*insideColor:  #DDDDDD"   "\n" \
			"*gearboxColor: #444488"   "\n" \
			"*gearColor:    #008877"   "\n" \
			"*wheelColor:   #007788"   "\n" \
			"*wireColor:    #006600"   "\n" \
			"*groundColor:  #0000FF"   "\n" \
                        "*textColor:       #FFFFFF""\n" \
                        "*textBackground:  #444444""\n" \
                        "*textBorderColor: #FFFF88""\n" \
                        "*textLines:    10          \n" \
			"*program:      xscreensaver-text\n" \
			"*usePty:       False\n"

#undef DEBUG
#define WORDBUBBLES

# define release_robot 0

#define DEF_SPEED       "1.0"
#define DEF_ROBOT_SIZE  "1.0"
#define DEF_TEXTURE     "True"
#define DEF_FADE        "True"
#define DEF_OPACITY     "1.0"
#define DEF_TALK        "0.2"

#include "xlockmore.h"
#include "gltrackball.h"
#include "ximage-loader.h"
#include "involute.h"
#include "sphere.h"

#ifdef WORDBUBBLES
# include "textclient.h"
# include "texfont.h"
#endif

#include <ctype.h>

#ifndef HAVE_JWZGLES /* No SPHERE_MAP on iPhone */
# define HAVE_TEXTURE
#endif

#ifdef HAVE_TEXTURE
# include "images/gen/chromesphere_png.h"
#endif

#ifdef USE_GL /* whole file */

#include "gllist.h"

extern const struct gllist
  *robot_arm_half, *robot_body_half_outside, *robot_body_half_inside,
  *robot_crank_full, *robot_gearbox_half, *robot_hand_half,
  *robot_leg_half, *robot_rotator_half, *robot_wireframe;

static struct gllist *robot_dome = 0, *robot_gear = 0, *ground = 0;

static const struct gllist * const *all_objs[] = {
  &robot_arm_half, &robot_body_half_outside, &robot_body_half_inside,
  &robot_crank_full, &robot_gearbox_half, &robot_hand_half,
  &robot_leg_half, &robot_rotator_half, &robot_wireframe,
  (const struct gllist * const *) &robot_dome,
  (const struct gllist * const *) &robot_gear,
  (const struct gllist * const *) &ground
};

#define ROBOT_ARM	0
#define ROBOT_BODY_1	1
#define ROBOT_BODY_2	2
#define ROBOT_CRANK	3
#define ROBOT_GEARBOX	4
#define ROBOT_HAND	5
#define ROBOT_LEG	6
#define ROBOT_ROTATOR	7
#define ROBOT_WIREFRAME	8
#define ROBOT_DOME	9
#define ROBOT_GEAR	10
#define GROUND		11

typedef struct {
  GLfloat x, y, z;		/* position */
  GLfloat facing;		/* direction of front of robot, degrees */
  GLfloat pitch;		/* front/back tilt angle, degrees */
  GLfloat roll;			/* left/right tilt angle, degrees */
  GLfloat speed;		/* some robots are faster */
  GLfloat crank_rot;		/* gear state, degrees */
  GLfloat hand_rot[2];		/* rotation of the hands, degrees */
  GLfloat hand_pos[2];		/* openness of the hands, ratio */
  GLfloat balance;		/* how off-true does it walk? degrees */
  GLfloat body_transparency;	/* ratio */
  int fading_p;			/* -1, 0, 1 */

} walker;

typedef struct {
  GLXContext *glx_context;
  trackball_state *user_trackball;
  Bool button_down_p;

  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];

  int nwalkers;
  walker *walkers;

  GLfloat looking_x, looking_y, looking_z;	/* Where camera is aimed */
  GLfloat olooking_x, olooking_y, olooking_z;	/* Where camera was aimed */
  Bool camera_tracking_p;			/* Whether camera in motion */
  GLfloat tracking_ratio;

# ifdef HAVE_TEXTURE
  GLuint chrome_texture;
# endif

# ifdef WORDBUBBLES
  texture_font_data *font_data;
  int bubble_tick;
  text_data *tc;
  char words[10240];
  int lines, max_lines;

  GLfloat text_color[4], text_bg[4], text_bd[4];

# endif /* WORDBUBBLES */


# ifdef DEBUG
  GLfloat debug_x, debug_y, debug_z;
# endif

} robot_configuration;

static robot_configuration *bps = NULL;

static GLfloat speed, size, opacity;
static int do_texture, do_fade;
#ifdef WORDBUBBLES
static GLfloat talk_chance;
#endif
#ifdef DEBUG
static int debug_p;
#endif

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",     XrmoptionSepArg, 0 },
  { "-size",       ".robotSize", XrmoptionSepArg, 0 },
  { "-opacity",    ".opacity",   XrmoptionSepArg, 0 },
  { "-talk",       ".talk",      XrmoptionSepArg, 0 },
  {"-texture",     ".texture",   XrmoptionNoArg, "True" },
  {"+texture",     ".texture",   XrmoptionNoArg, "False" },
  {"-fade",        ".fade",      XrmoptionNoArg, "True" },
  {"+fade",        ".fade",      XrmoptionNoArg, "False" },
#ifdef DEBUG
  {"-debug",       ".debug",     XrmoptionNoArg, "True" },
  {"+debug",       ".debug",     XrmoptionNoArg, "False" },
#endif
};

static argtype vars[] = {
  {&speed,       "speed",      "Speed",     DEF_SPEED,      t_Float},
  {&size,        "robotSize",  "RobotSize", DEF_ROBOT_SIZE, t_Float},
  {&opacity,     "opacity",    "Opacity",   DEF_OPACITY,    t_Float},
  {&do_texture,  "texture",    "Texture",   DEF_TEXTURE,    t_Bool},
  {&do_fade,     "fade",       "Fade",      DEF_FADE,       t_Bool},
#ifdef WORDBUBBLES
  {&talk_chance, "talk",       "Talk",      DEF_TALK,       t_Float},
#endif
#ifdef DEBUG
  {&debug_p,     "debug",      "Debug",     "False",        t_Bool},
#endif
};

ENTRYPOINT ModeSpecOpt robot_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_robot (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (40.0, 1/h, 1.0, 250);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 30,
             0, 0, 0,
             0, 1, 0);

  glClear(GL_COLOR_BUFFER_BIT);

# ifdef WORDBUBBLES
  {
    robot_configuration *bp = &bps[MI_SCREEN(mi)];
    int w = (width < 800 ? 25 : 40);
    int h = 1000;
    if (bp->tc)
      textclient_reshape (bp->tc, w, h, w, h,
                          /* Passing bp->max_lines isn't actually necessary */
                          0);
  }
# endif

}


ENTRYPOINT Bool
robot_handle_event (ModeInfo *mi, XEvent *event)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->user_trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
#ifdef DEBUG
  else if (event->xany.type == KeyPress && debug_p)
    {
      KeySym keysym;
      char c = 0;
      double n[3] = { 1.0, 0.1, 0.1 };
      int s = (event->xkey.state & ShiftMask ? 10 : 1);

      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if      (keysym == XK_Right)   bp->debug_x += n[0] * s;
      else if (keysym == XK_Left)    bp->debug_x -= n[0] * s;
      else if (keysym == XK_Up)      bp->debug_y += n[1] * s;
      else if (keysym == XK_Down)    bp->debug_y -= n[1] * s;
      else if (c == '=' || c == '+') bp->debug_z += n[2] * s;
      else if (c == '-' || c == '_') bp->debug_z -= n[2] * s;
      else if (c == ' ') bp->debug_x = bp->debug_y = bp->debug_z = 0;
      else if (c == '\n' || c == '\r')
        fprintf (stderr, "%.4f %.4f %.4f\n", 
                 bp->debug_x, bp->debug_y, bp->debug_z);
      else return False;
      return True;
    }
#endif /* DEBUG */

  return False;
}


#ifdef HAVE_TEXTURE

static void
load_textures (ModeInfo *mi)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  XImage *xi;

  xi = image_data_to_ximage (mi->dpy, mi->xgwa.visual,
                             chromesphere_png, sizeof(chromesphere_png));
  clear_gl_error();

  glGenTextures (1, &bp->chrome_texture);
  glBindTexture (GL_TEXTURE_2D, bp->chrome_texture);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                xi->width, xi->height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, xi->data);
  check_gl_error("texture");
  XDestroyImage (xi);

  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_T);
  glEnable(GL_TEXTURE_2D);
}

#endif /* HAVE_TEXTURE */


static int unit_gear (ModeInfo *, GLfloat color[4]);
static int draw_ground (ModeInfo *, GLfloat color[4]);
static void init_walker (ModeInfo *, walker *);

static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "RobotColor");
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
init_robot (ModeInfo *mi)
{
  robot_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_robot (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

# ifdef HAVE_TEXTURE
  if (!wire && do_texture)
    load_textures (mi);
# endif

  bp->user_trackball = gltrackball_init (False);

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;
      GLfloat spec1[4] = {1.00, 1.00, 1.00, 1.0};
      GLfloat spec2[4] = {0.40, 0.40, 0.70, 1.0};
      GLfloat *spec = 0;
      GLfloat shiny = 20;

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);
      glRotatef (180, 0, 0, 1);
      glScalef (6, 6, 6);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case ROBOT_BODY_1:
        key = "bodyColor";
        spec = spec1;
        shiny = 128;
# ifdef HAVE_TEXTURE
        if (do_texture)
          {
            glBindTexture (GL_TEXTURE_2D, bp->chrome_texture);
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
          }
# endif
        break;
      case ROBOT_DOME:
        key = "domeColor";
        spec = spec1;
        shiny = 128;
# ifdef HAVE_TEXTURE
        if (do_texture)
          {
            glBindTexture (GL_TEXTURE_2D, bp->chrome_texture);
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
          }
# endif
        break;
      case ROBOT_BODY_2:
        key = "insideColor";
        spec = spec2;
        shiny = 128;
        break;
      case ROBOT_ARM:
        key = "armColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_HAND:
        key = "handColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_LEG:
        key = "legColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_CRANK:
        key = "crankColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_ROTATOR:
        key = "wheelColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_GEAR:
        key = "gearColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_GEARBOX:
        key = "gearboxColor";
        spec = spec2;
        shiny = 20;
        break;
      case ROBOT_WIREFRAME:
        key = "wireColor";
        spec = spec2;
        shiny = 20;
        break;
      case GROUND:
        key = "groundColor";
        spec = spec2;
        shiny = 20;
        break;
      default:
        abort();
      }

      parse_color (mi, key, bp->component_colors[i]);

      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
      glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);

      switch (i) {
      case ROBOT_DOME:
        if (! robot_dome)
          robot_dome = (struct gllist *) calloc (1, sizeof(*robot_dome));
        robot_dome->points = unit_dome (32, 32, MI_IS_WIREFRAME(mi));
        break;
      case ROBOT_GEAR:
        if (! robot_gear)
          robot_gear = (struct gllist *) calloc (1, sizeof(*robot_gear));
        robot_gear->points = unit_gear (mi, bp->component_colors[i]);
        break;
      case GROUND:
        if (! ground)
          ground = (struct gllist *) calloc (1, sizeof(*ground));
        ground->points = draw_ground (mi, bp->component_colors[i]);
        break;
      case ROBOT_WIREFRAME:
        glLineWidth (0.3);
        renderList (gll, True);
        break;
      default:
        renderList (gll, wire);
        /* glColor3f (1, 1, 1); renderListNormals (gll, 100, True); */
        /* glColor3f (1, 1, 0); renderListNormals (gll, 100, False); */
        break;
      }

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

# ifdef DEBUG
  if (debug_p) MI_COUNT(mi) = 1;
# endif

  bp->nwalkers = MI_COUNT(mi);
  bp->walkers = (walker *) calloc (bp->nwalkers, sizeof (walker));

  for (i = 0; i < bp->nwalkers; i++)
    init_walker (mi, &bp->walkers[i]);

  /* Since #0 is the one we track, make sure it doesn't walk too straight.
   */
  bp->walkers[0].balance *= 1.5;

# ifdef WORDBUBBLES
  bp->font_data = load_texture_font (mi->dpy, "labelFont");
  bp->max_lines = get_integer_resource (mi->dpy, "textLines", "TextLines");
  bp->tc = textclient_open (MI_DISPLAY (mi));

  parse_color (mi, "textColor", bp->text_color);
  parse_color (mi, "textBackground", bp->text_bg);
  parse_color (mi, "textBorderColor", bp->text_bd);

  reshape_robot (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

# endif /* WORDBUBBLES */

# ifdef DEBUG
  if (!debug_p)
# endif
    /* Let's tilt the floor a little. */
    gltrackball_reset (bp->user_trackball,
                       -0.6 + frand(1.2),
                       -0.6 + frand(0.2));
}


static int
draw_component (ModeInfo *mi, int i)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);
  glCallList (bp->dlists[i]);
  return (*all_objs[i])->points / 3;
}

static int
draw_transparent_component (ModeInfo *mi, int i, GLfloat alpha)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int count = 0;

  if (alpha < 0) return 0;
  if (alpha > 1) alpha = 1;
  bp->component_colors[i][3] = alpha;

  if (wire || alpha >= 1)
    return draw_component (mi, i);

  /* Draw into the depth buffer but not the frame buffer */
  glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  count += draw_component (mi, i);

  /* Now draw into the frame buffer only where there's already depth */
  glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthFunc (GL_EQUAL);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  count += draw_component (mi, i);
  glDepthFunc (GL_LESS);
  glDisable (GL_BLEND);
  return count;
}



static int
draw_arm_half (ModeInfo *mi, walker *f)
{
  int count = 0;
  glPushMatrix();
  count += draw_transparent_component (mi, ROBOT_ARM, f->body_transparency);
  glPopMatrix();
  return count;
}

static int
draw_hand_half (ModeInfo *mi, walker *f)
{
  int count = 0;
  glPushMatrix();
  count += draw_transparent_component (mi, ROBOT_HAND, f->body_transparency);
  glPopMatrix();
  return count;
}


/* rotation of arm: 0-360.
   openness of fingers: 0.0 - 1.0.
 */
static int
draw_arm (ModeInfo *mi, walker *f, GLfloat left_p, GLfloat rot, GLfloat open)
{
  int count = 0;
  GLfloat arm_x = 4766;	/* distance from origin to arm axis */
  GLfloat arm_y = 12212;

  open *= 5.5;   /* scale of finger range */

# ifdef DEBUG
  if (debug_p) rot = 0;
# endif

  glPushMatrix();

  if (! left_p)
    glTranslatef (0, 0, arm_x * 2);

  glTranslatef (0, arm_y, -arm_x);	/* move to origin */
  glRotatef (rot, 1, 0, 0);
  glTranslatef (0, -arm_y, arm_x);	/* move back */

  glFrontFace(GL_CCW);
  count += draw_arm_half (mi, f);

  glScalef (1, -1, 1);
  glTranslatef (0, -arm_y * 2, 0);
  glFrontFace(GL_CW);
  count += draw_arm_half (mi, f);

  glPushMatrix();
  glTranslatef (0, 0, -arm_x * 2);
  glScalef (1, 1, -1);
  glFrontFace(GL_CCW);
  count += draw_arm_half (mi, f);

  glScalef (1, -1, 1);
  glTranslatef (0, -arm_y * 2, 0);
  glFrontFace(GL_CW);
  count += draw_arm_half (mi, f);
  glPopMatrix();

  glTranslatef (0, 0, open);
  glFrontFace(GL_CW);
  count += draw_hand_half (mi, f);

  glTranslatef (0, 0, -open);
  glScalef (1, 1, -1);
  glTranslatef (0, 0, arm_x * 2);
  glFrontFace(GL_CCW);
  glTranslatef (0, 0, open);
  count += draw_hand_half (mi, f);

  glPopMatrix();
  return count;
}


static int
draw_body_half (ModeInfo *mi, walker *f, Bool inside_p)
{
  int count = 0;
  int which = (inside_p ? ROBOT_BODY_2 : ROBOT_BODY_1);
  glPushMatrix();
  count += draw_transparent_component (mi, which, f->body_transparency);
  glPopMatrix();
  return count;
}


static int
draw_body (ModeInfo *mi, walker *f, Bool inside_p)
{
  int count = 0;
  glPushMatrix();

  glFrontFace(GL_CCW);
  count += draw_body_half (mi, f, inside_p);

  glScalef (1, 1, -1);
  glFrontFace(GL_CW);
  count += draw_body_half (mi, f, inside_p);

  glPopMatrix();

  return count;
}


static int
draw_gearbox_half (ModeInfo *mi)
{
  int count = 0;
  glPushMatrix();
  count += draw_component (mi, ROBOT_GEARBOX);
  glPopMatrix();
  return count;
}


static int
draw_gearbox (ModeInfo *mi)
{
  int count = 0;
  glPushMatrix();

  glFrontFace(GL_CCW);
  count += draw_gearbox_half (mi);

  glScalef (1, 1, -1);
  glFrontFace(GL_CW);
  count += draw_gearbox_half (mi);

  glPopMatrix();
  return count;
}


static int
unit_gear (ModeInfo *mi, GLfloat color[4])
{
  int wire = MI_IS_WIREFRAME(mi);
  gear G = { 0, };
  gear *g = &G;

  g->r          = 0.5;
  g->nteeth     = 16;
  g->tooth_h    = 0.12;
  g->thickness  = 0.32;
  g->thickness2 = g->thickness * 0.5;
  g->thickness3 = g->thickness;
  g->inner_r    = g->r * 0.7;
  g->inner_r2   = g->r * 0.4;
  g->inner_r3   = g->r * 0.1;
  g->size       = INVOLUTE_LARGE;

  g->color[0] = g->color2[0] = color[0];
  g->color[1] = g->color2[1] = color[1];
  g->color[2] = g->color2[2] = color[2];
  g->color[3] = g->color2[3] = color[3];

  return draw_involute_gear (g, wire);
}


static int
draw_gear (ModeInfo *mi)
{
  int count = 0;
  GLfloat n = 350;
  glScalef (n, n, n);
  count += draw_component (mi, ROBOT_GEAR);
  return count;
}


static int
draw_crank (ModeInfo *mi, walker *f, GLfloat rot)
{
  int count = 0;
  GLfloat origin = 12210.0;

  rot = -rot;

  glPushMatrix();

  glTranslatef (0, origin, 0);   /* position at origin */
  glRotatef (rot, 0, 0, 1);

  glPushMatrix();
  glRotatef (90, 1, 0, 0);
  count += draw_gear (mi);
  glPopMatrix();

  glTranslatef (0, -origin, 0);  /* move back */

  glFrontFace(GL_CCW);
  count += draw_component (mi, ROBOT_CRANK);

  glPopMatrix();

  return count;
}


static int
draw_rotator_half (ModeInfo *mi)
{
  int count = 0;
  glPushMatrix();
  count += draw_component (mi, ROBOT_ROTATOR);
  glPopMatrix();
  return count;
}


static int
draw_rotator (ModeInfo *mi, walker *f, GLfloat rot)
{
  int count = 0;
  GLfloat origin = 10093.0;

  glPushMatrix();

  glTranslatef (0, origin, 0);   /* position at origin */
  glRotatef (rot, 0, 0, 1);

  glPushMatrix();
  glRotatef (90, 1, 0, 0);
  count += draw_gear (mi);
  glPopMatrix();

# ifdef DEBUG
  if (debug_p)
    {
      glDisable(GL_LIGHTING);
      glBegin(GL_LINES);
      glVertex3f(0, 0, -3000);
      glVertex3f(0, 0,  3000);
      glEnd();
      if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
    }
# endif

  glTranslatef (0, -origin, 0);   /* move back */

  glFrontFace(GL_CCW);
  count += draw_rotator_half (mi);

  glScalef (1, 1, -1);
  glFrontFace(GL_CW);
  glRotatef (180, 0, 0, 1);
  glTranslatef (0, -origin * 2, 0);   /* move back */
  count += draw_rotator_half (mi);

  glPopMatrix();
  return count;
}


static int
draw_leg_half (ModeInfo *mi)
{
  int count = 0;
  glPushMatrix();
  count += draw_component (mi, ROBOT_LEG);
  glPopMatrix();
  return count;
}


static int
draw_wireframe (ModeInfo *mi, walker *f)
{
  int wire = MI_IS_WIREFRAME(mi);
  int count = 0;
  GLfloat alpha = 0.6 - f->body_transparency;
  if (alpha < 0) return 0;
  alpha *= 0.3;
  if (!wire) glDisable (GL_LIGHTING);
  glPushMatrix();
  count += draw_transparent_component (mi, ROBOT_WIREFRAME, alpha);
  glPopMatrix();
  if (!wire) glEnable (GL_LIGHTING);
  return count;
}


static int
draw_leg (ModeInfo *mi, GLfloat rot, Bool left_p)
{
  int count = 0;
  GLfloat x, y;
  GLfloat leg_distance = 9401;   /* distance from ground to leg axis */
  GLfloat rot_distance = 10110;  /* distance from ground to rotator axis */
  GLfloat pin_distance = 14541;  /* distance from ground to stop pin */
  GLfloat orbit_r = rot_distance - leg_distance;  /* radius of rotation */

  /* Actually it's the bottom of the pin minus its diameter, or something. */
  pin_distance -= 590;

  glPushMatrix();

  if (left_p)
    glRotatef (180, 0, 1, 0);

  if (!left_p) rot = -(rot + 180);

  rot -= 90;

  x = orbit_r * cos (-rot * M_PI / 180);
  y = orbit_r * sin (-rot * M_PI / 180);

  {
    /* Rotate the leg by angle B of the right	              A
       triangle ABC, where:		                     /:
						            / :
	A = position of stop pin		           /  :
	D = position of rotator wheel's axis	        , - ~ ~ ~ - ,
	C = D + y			            , '  /    :       ' ,
	B = D + xy (leg's axis)		          ,     /     :           ,
					         ,     /      :            ,
      So:				        ,     /       :             ,
        H = dist(A,B)			        ,    /        D             ,
        O = dist(A,C)			        ,   /         :             ,
        sin(th) = O/H			         , /          :            ,
        th = asin(O/H)			          B ~ ~ ~ ~ ~ C           .
					            ,                  , '
					              ' - , _ _ _ ,  '
    */
    GLfloat Ay = pin_distance - leg_distance;
    GLfloat Cx = 0, Cy = y;
    GLfloat Bx = x;
    GLfloat dBC = Cx - Bx;
    GLfloat dAC = Cy - Ay;
    GLfloat dAB = sqrt (dBC*dBC + dAC*dAC);
    GLfloat th = asin (dAC / dAB);
    rot = th / (M_PI / 180.0);
    rot += 90;
    if (dBC > 0) rot = 360-rot;
  }

  glTranslatef (0, orbit_r, 0);        /* position on rotator axis */
  glTranslatef (x, y, 0);

  glTranslatef (0, leg_distance, 0);   /* position on leg axis */
  glRotatef(rot, 0, 0, 1);
  glTranslatef (0, -leg_distance, 0);  /* move back */

  glFrontFace(GL_CCW);
  count += draw_leg_half (mi);

  glScalef (-1, 1, 1);
  glFrontFace(GL_CW);
  count += draw_leg_half (mi);

  glPopMatrix();
  return count;
}


static int
draw_dome (ModeInfo *mi, walker *f)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int which = ROBOT_DOME;
  int count = 0;
  GLfloat n = 8.3;
  GLfloat trans = f->body_transparency;
  GLfloat max = 0.7;
  GLfloat dome_y = 15290;

  if (trans < 0) trans = 0;
  if (trans > max) trans = max;

  if (!wire) glEnable (GL_BLEND);

  glPushMatrix();
  glTranslatef (0, dome_y, 0);
  glScalef (100, 100, 100);
  glRotatef (90, 1, 0, 0);
  glTranslatef (0.35, 0, 0);
  glScalef (n, n, n);
  glFrontFace(GL_CCW);
  bp->component_colors[which][3] = trans;
  count += draw_component (mi, which);
  glPopMatrix();

  if (!wire) glDisable (GL_BLEND);

  return count;
}


/* Is this robot overlapping any other?
 */
static Bool
collision_p (ModeInfo *mi, walker *w, GLfloat extra_space)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (MI_COUNT(mi) <= 1) return False;

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      walker *w2 = &bp->walkers[i];
      GLfloat min = 0.75 + extra_space;
      GLfloat d, dx, dy;
      if (w == w2) continue;
      dx = w->x - w2->x;
      dy = w->y - w2->y;
      d = (dx*dx + dy*dy);
      if (d <= min*min) return True;
    }
  return False;
}


/* I couldn't figure out a straightforward trig solution to the
   forward/backward tilting that happens as weight passes from one
   foot to another, so I just built a tool to eyeball it manually.
   { vertical_translation, rotation, forward_translation }
 */
static const struct { GLfloat up, rot, fwd; } wobble_profile[360] = {
 {  0.0000,  0.00, 0.0000 },	/* 0 */
 {  0.0000, -0.01, 0.0025 },
 {  0.0000, -0.25, 0.0040 },
 {  0.0000, -0.41, 0.0060 },
 {  0.0000, -0.62, 0.0080 },
 {  0.0000, -0.80, 0.0095 },
 {  0.0000, -0.90, 0.0120 },
 {  0.0000, -1.10, 0.0135 },
 {  0.0000, -1.25, 0.0150 },
 {  0.0000, -1.40, 0.0175 },
 {  0.0000, -1.50, 0.0195 },	/* 10 */
 {  0.0000, -1.70, 0.0215 },
 {  0.0000, -1.80, 0.0230 },
 {  0.0000, -2.00, 0.0250 },
 {  0.0000, -2.10, 0.0270 },
 { -0.0005, -2.30, 0.0290 },
 { -0.0005, -2.50, 0.0305 },
 { -0.0005, -2.60, 0.0330 },
 { -0.0005, -2.70, 0.0330 },
 { -0.0005, -2.90, 0.0350 },
 { -0.0005, -3.00, 0.0365 },	/* 20 */
 { -0.0010, -3.20, 0.0380 },
 { -0.0005, -3.30, 0.0400 },
 { -0.0010, -3.50, 0.0420 },
 { -0.0010, -3.70, 0.0440 },
 { -0.0010, -3.80, 0.0460 },
 { -0.0010, -3.90, 0.0470 },
 { -0.0015, -4.10, 0.0500 },
 { -0.0015, -4.20, 0.0515 },
 { -0.0015, -4.40, 0.0535 },
 { -0.0015, -4.50, 0.0550 },	/* 30 */
 { -0.0015, -4.60, 0.0565 },
 { -0.0020, -4.80, 0.0585 },
 { -0.0020, -5.00, 0.0600 },
 { -0.0020, -5.10, 0.0620 },
 { -0.0025, -5.20, 0.0635 },
 { -0.0025, -5.40, 0.0655 },
 { -0.0025, -5.50, 0.0675 },
 { -0.0025, -5.60, 0.0690 },
 { -0.0030, -5.80, 0.0710 },
 { -0.0030, -5.90, 0.0720 },	/* 40 */
 { -0.0030, -6.00, 0.0740 },
 { -0.0035, -6.10, 0.0760 },
 { -0.0035, -6.30, 0.0790 },
 { -0.0040, -6.40, 0.0805 },
 { -0.0040, -6.50, 0.0820 },
 { -0.0040, -6.60, 0.0835 },
 { -0.0045, -6.80, 0.0855 },
 { -0.0045, -6.90, 0.0870 },
 { -0.0050, -7.00, 0.0885 },
 { -0.0050, -7.20, 0.0900 },	/* 50 */
 { -0.0050, -7.20, 0.0915 },
 { -0.0055, -7.40, 0.0930 },
 { -0.0055, -7.50, 0.0945 },
 { -0.0060, -7.60, 0.0960 },
 { -0.0060, -7.70, 0.0970 },
 { -0.0065, -7.80, 0.0985 },
 { -0.0060, -7.70, 0.0995 },
 { -0.0060, -7.60, 0.1010 },
 { -0.0060, -7.50, 0.1020 },
 { -0.0055, -7.30, 0.1030 },	/* 60 */
 { -0.0050, -7.10, 0.1040 },
 { -0.0050, -6.90, 0.1050 },
 { -0.0045, -6.80, 0.1065 },
 { -0.0045, -6.50, 0.1075 },
 { -0.0040, -6.40, 0.1085 },
 { -0.0040, -6.20, 0.1095 },
 { -0.0040, -6.00, 0.1105 },
 { -0.0035, -5.80, 0.1115 },
 { -0.0030, -5.50, 0.1125 },
 { -0.0030, -5.40, 0.1135 },	/* 70 */
 { -0.0030, -5.10, 0.1145 },
 { -0.0030, -4.90, 0.1150 },
 { -0.0025, -4.70, 0.1160 },
 { -0.0025, -4.40, 0.1165 },
 { -0.0025, -4.20, 0.1175 },
 { -0.0020, -3.90, 0.1180 },
 { -0.0020, -3.70, 0.1185 },
 { -0.0020, -3.40, 0.1190 },
 { -0.0020, -3.10, 0.1195 },
 { -0.0020, -2.90, 0.1200 },	/* 80 */
 { -0.0015, -2.60, 0.1200 },
 { -0.0015, -2.30, 0.1205 },
 { -0.0015, -2.00, 0.1210 },
 { -0.0015, -1.80, 0.1215 },
 { -0.0015, -1.50, 0.1215 },
 { -0.0015, -1.20, 0.1215 },
 { -0.0015, -0.90, 0.1215 },
 { -0.0015, -0.60, 0.1215 },
 { -0.0015, -0.30, 0.1215 },
 { -0.0010,  0.00, 0.1215 },	/* 90 */
 { -0.0010,  0.30, 0.1215 },
 { -0.0010,  0.60, 0.1215 },
 { -0.0010,  0.90, 0.1215 },
 { -0.0010,  1.20, 0.1215 },
 { -0.0015,  1.40, 0.1215 },
 { -0.0015,  1.70, 0.1215 },
 { -0.0015,  2.00, 0.1215 },
 { -0.0015,  2.30, 0.1215 },
 { -0.0015,  2.60, 0.1215 },
 { -0.0015,  2.80, 0.1220 },	/* 100 */
 { -0.0020,  3.10, 0.1225 },
 { -0.0020,  3.30, 0.1230 },
 { -0.0020,  3.60, 0.1235 },
 { -0.0020,  3.90, 0.1240 },
 { -0.0025,  4.10, 0.1245 },
 { -0.0025,  4.40, 0.1250 },
 { -0.0025,  4.60, 0.1260 },
 { -0.0025,  4.90, 0.1265 },
 { -0.0030,  5.10, 0.1275 },
 { -0.0030,  5.30, 0.1285 },	/* 110 */
 { -0.0035,  5.60, 0.1290 },
 { -0.0035,  5.80, 0.1300 },
 { -0.0035,  6.00, 0.1310 },
 { -0.0040,  6.20, 0.1325 },
 { -0.0040,  6.40, 0.1335 },
 { -0.0045,  6.60, 0.1345 },
 { -0.0045,  6.70, 0.1355 },
 { -0.0050,  6.90, 0.1365 },
 { -0.0050,  7.10, 0.1375 },
 { -0.0055,  7.30, 0.1390 },	/* 120 */
 { -0.0055,  7.40, 0.1400 },
 { -0.0060,  7.50, 0.1415 },
 { -0.0065,  8.00, 0.1425 },
 { -0.0065,  7.80, 0.1440 },
 { -0.0060,  7.80, 0.1455 },
 { -0.0060,  7.60, 0.1470 },
 { -0.0055,  7.50, 0.1485 },
 { -0.0055,  7.40, 0.1500 },
 { -0.0050,  7.30, 0.1515 },
 { -0.0050,  7.20, 0.1530 },	/* 130 */
 { -0.0050,  7.00, 0.1545 },
 { -0.0045,  6.90, 0.1560 },
 { -0.0045,  6.80, 0.1575 },
 { -0.0040,  6.70, 0.1590 },
 { -0.0040,  6.50, 0.1605 },
 { -0.0040,  6.40, 0.1625 },
 { -0.0035,  6.30, 0.1640 },
 { -0.0035,  6.10, 0.1655 },
 { -0.0030,  6.10, 0.1670 },
 { -0.0030,  5.90, 0.1690 },	/* 140 */
 { -0.0030,  5.70, 0.1705 },
 { -0.0025,  5.70, 0.1720 },
 { -0.0025,  5.50, 0.1740 },
 { -0.0025,  5.40, 0.1755 },
 { -0.0025,  5.20, 0.1775 },
 { -0.0020,  5.10, 0.1790 },
 { -0.0020,  5.00, 0.1810 },
 { -0.0020,  4.80, 0.1825 },
 { -0.0015,  4.70, 0.1840 },
 { -0.0015,  4.60, 0.1860 },	/* 150 */
 { -0.0015,  4.40, 0.1880 },
 { -0.0015,  4.20, 0.1900 },
 { -0.0015,  4.10, 0.1915 },
 { -0.0010,  4.00, 0.1935 },
 { -0.0010,  3.80, 0.1955 },
 { -0.0010,  3.70, 0.1970 },
 { -0.0010,  3.50, 0.1990 },
 { -0.0005,  3.40, 0.2010 },
 { -0.0010,  3.20, 0.2025 },
 { -0.0005,  3.10, 0.2045 },	/* 160 */
 { -0.0005,  2.90, 0.2065 },
 { -0.0005,  2.80, 0.2085 },
 { -0.0005,  2.60, 0.2105 },
 { -0.0005,  2.50, 0.2120 },
 { -0.0005,  2.30, 0.2140 },
 { -0.0005,  2.20, 0.2160 },
 { -0.0005,  2.00, 0.2180 },
 {  0.0000,  1.90, 0.2200 },
 {  0.0000,  1.70, 0.2220 },
 {  0.0000,  1.60, 0.2235 },	/* 170 */
 {  0.0000,  1.40, 0.2255 },
 {  0.0000,  1.30, 0.2275 },
 {  0.0000,  1.10, 0.2295 },
 {  0.0000,  0.90, 0.2315 },
 {  0.0000,  0.80, 0.2335 },
 {  0.0000,  0.60, 0.2355 },
 {  0.0000,  0.50, 0.2375 },
 {  0.0000,  0.30, 0.2395 },
 {  0.0000,  0.10, 0.2415 },
 {  0.0000,  0.00, 0.2430 },	/* 180 */
 {  0.0000, -0.10, 0.2450 },
 {  0.0000, -0.30, 0.2470 },
 {  0.0000, -0.40, 0.2490 },
 {  0.0000, -0.60, 0.2510 },
 {  0.0000, -0.80, 0.2530 },
 {  0.0000, -0.90, 0.2550 },
 {  0.0000, -1.10, 0.2570 },
 {  0.0000, -1.20, 0.2590 },
 {  0.0000, -1.40, 0.2610 },
 {  0.0000, -1.50, 0.2625 },	/* 190 */
 {  0.0000, -1.70, 0.2645 },
 {  0.0000, -1.80, 0.2665 },
 { -0.0005, -2.00, 0.2685 },
 { -0.0005, -2.10, 0.2705 },
 { -0.0005, -2.30, 0.2725 },
 { -0.0005, -2.40, 0.2740 },
 { -0.0005, -2.60, 0.2760 },
 { -0.0005, -2.80, 0.2780 },
 { -0.0005, -2.90, 0.2800 },
 { -0.0005, -3.00, 0.2820 },	/* 200 */
 { -0.0010, -3.20, 0.2835 },
 { -0.0005, -3.30, 0.2855 },
 { -0.0010, -3.50, 0.2875 },
 { -0.0010, -3.70, 0.2895 },
 { -0.0010, -3.80, 0.2910 },
 { -0.0010, -3.90, 0.2930 },
 { -0.0010, -4.00, 0.2950 },
 { -0.0015, -4.20, 0.2965 },
 { -0.0015, -4.40, 0.2985 },
 { -0.0015, -4.50, 0.3000 },	/* 210 */
 { -0.0015, -4.60, 0.3020 },
 { -0.0020, -4.80, 0.3040 },
 { -0.0020, -5.00, 0.3055 },
 { -0.0020, -5.00, 0.3075 },
 { -0.0025, -5.20, 0.3090 },
 { -0.0025, -5.30, 0.3110 },
 { -0.0025, -5.50, 0.3125 },
 { -0.0025, -5.60, 0.3140 },
 { -0.0030, -5.70, 0.3160 },
 { -0.0030, -5.90, 0.3175 },	/* 220 */
 { -0.0030, -6.00, 0.3190 },
 { -0.0035, -6.10, 0.3210 },
 { -0.0035, -6.30, 0.3225 },
 { -0.0040, -6.40, 0.3240 },
 { -0.0040, -6.50, 0.3255 },
 { -0.0040, -6.60, 0.3270 },
 { -0.0045, -6.80, 0.3290 },
 { -0.0045, -6.90, 0.3305 },
 { -0.0050, -7.00, 0.3320 },
 { -0.0050, -7.20, 0.3335 },	/* 230 */
 { -0.0050, -7.20, 0.3350 },
 { -0.0055, -7.40, 0.3365 },
 { -0.0055, -7.50, 0.3380 },
 { -0.0060, -7.60, 0.3390 },
 { -0.0060, -7.70, 0.3405 },
 { -0.0065, -7.80, 0.3420 },
 { -0.0060, -7.60, 0.3425 },
 { -0.0060, -7.50, 0.3440 },
 { -0.0055, -7.40, 0.3455 },
 { -0.0055, -7.20, 0.3470 },	/* 240 */
 { -0.0050, -7.10, 0.3480 },
 { -0.0050, -6.90, 0.3490 },
 { -0.0045, -6.80, 0.3500 },
 { -0.0045, -6.50, 0.3510 },
 { -0.0040, -6.40, 0.3520 },
 { -0.0040, -6.10, 0.3535 },
 { -0.0035, -6.00, 0.3545 },
 { -0.0035, -5.80, 0.3550 },
 { -0.0030, -5.50, 0.3560 },
 { -0.0030, -5.30, 0.3570 },	/* 250 */
 { -0.0030, -5.10, 0.3580 },
 { -0.0030, -4.90, 0.3585 },
 { -0.0025, -4.70, 0.3595 },
 { -0.0025, -4.40, 0.3600 },
 { -0.0020, -4.10, 0.3610 },
 { -0.0020, -3.90, 0.3615 },
 { -0.0020, -3.70, 0.3620 },
 { -0.0020, -3.30, 0.3625 },
 { -0.0020, -3.10, 0.3630 },
 { -0.0015, -2.80, 0.3635 },	/* 260 */
 { -0.0015, -2.60, 0.3640 },
 { -0.0015, -2.40, 0.3645 },
 { -0.0015, -2.00, 0.3645 },
 { -0.0015, -1.80, 0.3650 },
 { -0.0015, -1.40, 0.3650 },
 { -0.0015, -1.20, 0.3655 },
 { -0.0010, -0.90, 0.3655 },
 { -0.0010, -0.60, 0.3655 },
 { -0.0010, -0.30, 0.3655 },
 { -0.0010,  0.00, 0.3655 },	/* 270 */
 { -0.0010,  0.30, 0.3655 },
 { -0.0010,  0.60, 0.3655 },
 { -0.0010,  0.90, 0.3655 },
 { -0.0015,  1.10, 0.3655 },
 { -0.0015,  1.40, 0.3655 },
 { -0.0015,  1.70, 0.3655 },
 { -0.0015,  1.90, 0.3660 },
 { -0.0015,  2.20, 0.3660 },
 { -0.0015,  2.50, 0.3665 },
 { -0.0015,  2.80, 0.3670 },	/* 280 */
 { -0.0015,  3.10, 0.3675 },
 { -0.0020,  3.40, 0.3680 },
 { -0.0020,  3.70, 0.3685 },
 { -0.0020,  3.90, 0.3690 },
 { -0.0025,  4.10, 0.3695 },
 { -0.0025,  4.40, 0.3700 },
 { -0.0025,  4.60, 0.3710 },
 { -0.0025,  4.80, 0.3715 },
 { -0.0025,  5.00, 0.3730 },
 { -0.0030,  5.40, 0.3735 },	/* 290 */
 { -0.0035,  5.60, 0.3745 },
 { -0.0035,  5.80, 0.3755 },
 { -0.0035,  6.00, 0.3765 },
 { -0.0040,  6.20, 0.3775 },
 { -0.0045,  6.50, 0.3785 },
 { -0.0045,  6.60, 0.3795 },
 { -0.0045,  6.80, 0.3805 },
 { -0.0050,  7.00, 0.3815 },
 { -0.0050,  7.10, 0.3825 },
 { -0.0055,  7.20, 0.3840 },	/* 300 */
 { -0.0055,  7.40, 0.3850 },
 { -0.0060,  7.50, 0.3865 },
 { -0.0060,  7.70, 0.3875 },
 { -0.0065,  7.80, 0.3890 },
 { -0.0060,  7.80, 0.3900 },
 { -0.0060,  7.60, 0.3915 },
 { -0.0055,  7.60, 0.3930 },
 { -0.0055,  7.40, 0.3945 },
 { -0.0050,  7.30, 0.3960 },
 { -0.0050,  7.20, 0.3975 },	/* 310 */
 { -0.0050,  7.00, 0.3990 },
 { -0.0045,  6.90, 0.4005 },
 { -0.0045,  6.80, 0.4020 },
 { -0.0040,  6.70, 0.4035 },
 { -0.0040,  6.60, 0.4050 },
 { -0.0040,  6.40, 0.4065 },
 { -0.0035,  6.30, 0.4085 },
 { -0.0035,  6.20, 0.4100 },
 { -0.0030,  6.10, 0.4115 },
 { -0.0030,  5.90, 0.4130 },	/* 320 */
 { -0.0030,  5.80, 0.4150 },
 { -0.0025,  5.70, 0.4165 },
 { -0.0025,  5.50, 0.4180 },
 { -0.0025,  5.40, 0.4200 },
 { -0.0025,  5.20, 0.4215 },
 { -0.0020,  5.10, 0.4235 },
 { -0.0020,  5.00, 0.4250 },
 { -0.0020,  4.80, 0.4270 },
 { -0.0015,  4.70, 0.4285 },
 { -0.0015,  4.60, 0.4305 },	/* 330 */
 { -0.0015,  4.40, 0.4325 },
 { -0.0015,  4.20, 0.4340 },
 { -0.0015,  4.10, 0.4360 },
 { -0.0010,  4.00, 0.4375 },
 { -0.0010,  3.80, 0.4395 },
 { -0.0010,  3.70, 0.4415 },
 { -0.0010,  3.50, 0.4435 },
 { -0.0005,  3.40, 0.4450 },
 { -0.0010,  3.20, 0.4470 },
 { -0.0005,  3.10, 0.4490 },	/* 340 */
 { -0.0005,  2.90, 0.4510 },
 { -0.0005,  2.80, 0.4525 },
 { -0.0005,  2.60, 0.4545 },
 { -0.0005,  2.40, 0.4565 },
 { -0.0005,  2.30, 0.4585 },
 { -0.0005,  2.20, 0.4605 },
 { -0.0005,  2.00, 0.4620 },
 {  0.0000,  1.90, 0.4640 },
 {  0.0000,  1.70, 0.4660 },
 {  0.0000,  1.60, 0.4680 },	/* 350 */
 {  0.0000,  1.40, 0.4700 },
 {  0.0000,  1.20, 0.4720 },
 {  0.0000,  1.10, 0.4740 },
 {  0.0000,  0.90, 0.4760 },
 {  0.0000,  0.80, 0.4780 },
 {  0.0000,  0.60, 0.4795 },
 {  0.0000,  0.50, 0.4815 },
 {  0.0000,  0.30, 0.4835 },
 {  0.0000,  0.20, 0.4855 },	/* 359 */
};


/* Turn the crank by 1 degree, which moves the legs and displaces the robot.
 */
static void
tick_walker (ModeInfo *mi, walker *f)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int deg;
  GLfloat th, fwd;

  if (bp->button_down_p) return;

  f->crank_rot++;
  deg = ((int) (f->crank_rot + 0.5)) % 360;

# ifdef DEBUG
  if (debug_p)
    {
      f->crank_rot = bp->debug_x;
      f->pitch = wobble_profile[deg].rot;
      f->z     = wobble_profile[deg].up;
    }
  else
# endif /* DEBUG */

  if (deg == 0)
    {
      fwd      = wobble_profile[deg].fwd;
      f->pitch = wobble_profile[deg].rot;
      f->z     = wobble_profile[deg].up;
    }
  else
    {
      fwd       = (wobble_profile[deg].fwd - wobble_profile[deg-1].fwd);
      f->pitch += (wobble_profile[deg].rot - wobble_profile[deg-1].rot);
      f->z     += (wobble_profile[deg].up  - wobble_profile[deg-1].up);
    }

  /* Lean slightly toward the foot that is raised off the ground. */
  f->roll = -2.5 * sin ((deg - 90) * M_PI / 180);

  if (!(random() % 10))
    {
      GLfloat b = f->balance / 10.0;
      int s = (f->balance > 0 ? 1 : -1);
      if (s < 0) b = -b;
      f->facing += s * frand (b);
    }

# ifdef DEBUG
  if (debug_p) fwd = 0;
# endif

  {
    GLfloat ox = f->x;
    GLfloat oy = f->y;
    th = f->facing * M_PI / 180.0;
    f->x += fwd * cos (th);
    f->y += fwd * sin (th);

    /* If moving this robot would collide with another, undo the move,
       recoil, and randomly turn.
     */
    if (collision_p (mi, f, 0))
      {
        fwd *= -1.5;
        f->x = ox + fwd * cos (th);
        f->y = oy + fwd * sin (th);
        f->facing += frand(10) - 5;
        if (! random() % 30)
          f->facing += frand(90) - 45;
      }
  }


  if (!do_fade ||
      opacity > 0.5)   /* Don't bother fading if it's already transparent. */
    {
      GLfloat tick = 0.002;
      GLfloat linger = 3;

      /* If we're not fading, maybe start fading out. */
      if (f->fading_p == 0 && ! (random() % 40000))
        f->fading_p = -1;

# ifdef DEBUG
      if (debug_p) f->fading_p = 0;
# endif

      if (f->fading_p < 0)
        {
          f->body_transparency -= tick;
          if (f->body_transparency <= -linger)
            {
              f->body_transparency = -linger;
              f->fading_p = 1;
            }
        }
      else if (f->fading_p > 0)
        {
          f->body_transparency += tick;
          if (f->body_transparency >= opacity)
            {
              f->body_transparency = opacity;
              f->fading_p = 0;
            }
        }
    }
}


static void
init_walker (ModeInfo *mi, walker *f)
{
  int i, start_tick = random() % 360;

  f->crank_rot = 0;
  f->pitch = wobble_profile[0].rot;
  f->z     = wobble_profile[0].up;

  f->body_transparency = opacity;

  f->hand_rot[0] = frand(180);
  f->hand_pos[0] = 0.6 + frand(0.4);
  f->hand_rot[1] = 180 - f->hand_rot[0];
  f->hand_pos[1] = f->hand_pos[0];

  if (! (random() % 30)) f->hand_rot[1] += frand(10);
  if (! (random() % 30)) f->hand_pos[1] = 0.6 + frand(0.4);

  f->facing = frand(360);
  f->balance = frand(10) - 5;

  if (MI_COUNT(mi) == 1)
    f->speed = 1.0;
  else
    f->speed = 0.6 + frand(0.8);

# ifdef DEBUG
  if (debug_p)
    {
      start_tick = 0;
      f->facing = 0;
      f->balance = 0;
    }
# endif

  for (i = 0; i < start_tick; i++)
    tick_walker (mi, f);

  /* Place them randomly, but non-overlapping. */
  for (i = 0; i < 1000; i++)
    {
      GLfloat range = 10;
      if (MI_COUNT(mi) > 10) range += MI_COUNT(mi) / 10.0;
      f->x = frand(range) - range/2;
      f->y = frand(range) - range/2;
      if (! collision_p (mi, f, 1.5))
        break;
    }

# ifdef DEBUG
  if (debug_p) f->x = f->y = 0;
# endif

}


/* Draw a robot standing in the right place, 1 unit tall.
 */
static int
draw_walker (ModeInfo *mi, walker *f, const char *tag)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int count = 0;
  glPushMatrix();

  glTranslatef (f->y, f->z, f->x);

  {
    GLfloat n = 0.01;
    glScalef (n, n, n);
  }

  glRotatef (90, 0, 1, 0);
  glRotatef (f->facing, 0, 1, 0);
  glRotatef (f->pitch,  0, 0, 1);
  glRotatef (f->roll,   1, 0, 0);

  {
    GLfloat n = 0.00484;	   /* make it 1 unit tall */
    glScalef (n, n, n);
  }

  count += draw_gearbox (mi);
  count += draw_crank (mi, f, f->crank_rot);
  count += draw_rotator (mi, f, f->crank_rot);
  count += draw_leg (mi, f->crank_rot, False);
  count += draw_leg (mi, f->crank_rot, True);
  count += draw_wireframe (mi, f);

  /* Draw these last, and outer shell first, to make transparency work.
     The order in which things hit the depth buffer matters.
   */
  if (f->body_transparency >= 0.001)
    {
      count += draw_arm (mi, f, True,  f->hand_rot[0], f->hand_pos[0]);
      count += draw_arm (mi, f, False, f->hand_rot[1], f->hand_pos[1]);
      count += draw_body (mi, f, False);
      count += draw_body (mi, f, True);
      count += draw_dome (mi, f);
    }

  if (tag)  /* For debugging depth sorting: label each robot */
    {
      GLfloat m[4][4];
      if (! wire) glDisable (GL_DEPTH_TEST);
      glColor3f (1, 1, 1);
      glPushMatrix();

      /* Billboard rotation */
      glGetFloatv (GL_MODELVIEW_MATRIX, &m[0][0]);
      m[0][0] = 1; m[1][0] = 0; m[2][0] = 0;
      m[0][1] = 0; m[1][1] = 1; m[2][1] = 0;
      m[0][2] = 0; m[1][2] = 0; m[2][2] = 1;
      glLoadIdentity();
      glMultMatrixf (&m[0][0]);
      glScalef (0.04, 0.04, 0.04);

      print_texture_string (bp->font_data, tag);
      glPopMatrix();
      if (! wire) glEnable (GL_DEPTH_TEST);
    }

  glPopMatrix();
  return count;
}


static int
draw_ground (ModeInfo *mi, GLfloat color[4])
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

# ifdef DEBUG
  if (debug_p) return 0;
# endif

  glPushMatrix();

  glRotatef (frand(90), 0, 0, 1);

  if (!wire)
    {
      GLfloat fog_color[4] = { 0, 0, 0, 1 };

      glLineWidth (4);
      glEnable (GL_LINE_SMOOTH);
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
      glDisable (GL_LINE_SMOOTH);
      glDisable (GL_BLEND);
      glDisable (GL_FOG);
    }

  glPopMatrix();

  return points;
}


/* If the target robot (robot #0) has moved too far from the point at which
   the camera is aimed, then initiate an animation to move the observer to
   the new spot.

   Because of the jerky forward motion of the robots, just always focusing
   on the center of the robot looks terrible, so instead we let them walk
   a little out of the center of the frame, and then catch up.
 */
static void
look_at_center (ModeInfo *mi)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat target_x = bp->walkers[0].x;
  GLfloat target_y = bp->walkers[0].y;
  GLfloat target_z = 0.8;   /* Look a little bit above his head */
  GLfloat max_dist = 2.5 / size;

# ifdef DEBUG
  if (debug_p) return;
# endif

  if (max_dist < 1)  max_dist = 1;
  if (max_dist > 10) max_dist = 10;

  if (bp->camera_tracking_p)
    {
      GLfloat r = (1 - cos (bp->tracking_ratio * M_PI)) / 2;
      bp->looking_x = bp->olooking_x + r * (target_x - bp->olooking_x);
      bp->looking_y = bp->olooking_y + r * (target_y - bp->olooking_y);
      bp->looking_z = bp->olooking_z + r * (target_z - bp->olooking_z);

      bp->tracking_ratio += 0.02;
      if (bp->tracking_ratio >= 1)
        {
          bp->camera_tracking_p = False;
          bp->olooking_x = bp->looking_x;
          bp->olooking_y = bp->looking_y;
          bp->olooking_z = bp->looking_z;
        }
    }
          
  if (! bp->camera_tracking_p)
    {
      GLfloat dist =
        sqrt ((target_x - bp->looking_x) * (target_x - bp->looking_x) +
              (target_y - bp->looking_y) * (target_y - bp->looking_y) +
              (target_z - bp->looking_z) * (target_z - bp->looking_z));

      if (dist > max_dist)
        {
          bp->camera_tracking_p = True;
          bp->tracking_ratio = 0;
          bp->olooking_x = bp->looking_x;
          bp->olooking_y = bp->looking_y;
          bp->olooking_z = bp->looking_z;
        }
    }

  glTranslatef (-bp->looking_y, -bp->looking_z, -bp->looking_x);

# if 0 /* DEBUG */
  {
    GLfloat th;
    glPushMatrix();
    glColor3f(1, 0, 0);
    glTranslatef (target_y, target_z, target_x);
    glBegin(GL_LINES);
    glVertex3f(0, -target_z, 0);
    glVertex3f(0, 1, 0);
    glVertex3f(-0.1, 0, -0.1);
    glVertex3f( 0.1, 0,  0.1);
    glVertex3f(-0.1, 0,  0.1);
    glVertex3f( 0.1, 0, -0.1);
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glColor3f(0, 1, 0);
    glTranslatef (bp->looking_y, bp->looking_z, bp->looking_x);
    glRotatef (30, 0, 1, 0);
    glBegin(GL_LINES);
    glVertex3f(0, -bp->looking_z, 0);
    glVertex3f(0, 1, 0);
    glVertex3f(-0.1, 0, -0.1);
    glVertex3f( 0.1, 0,  0.1);
    glVertex3f(-0.1, 0,  0.1);
    glVertex3f( 0.1, 0, -0.1);
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glColor3f(0, 0, 1);
    glTranslatef (bp->olooking_y, bp->olooking_z, bp->olooking_x);
    glRotatef (60, 0, 1, 0);
    glBegin(GL_LINES);
    glVertex3f(0, -bp->olooking_z, 0);
    glVertex3f(0, 1, 0);
    glVertex3f(-0.1, 0, -0.1);
    glVertex3f( 0.1, 0,  0.1);
    glVertex3f(-0.1, 0,  0.1);
    glVertex3f( 0.1, 0, -0.1);
    glEnd();

    glTranslatef (0, -bp->olooking_z, 0);
    glBegin (GL_LINE_LOOP);
    for (th = 0; th < M_PI * 2; th += 0.1)
      glVertex3f (bp->olooking_y + max_dist * cos(th), 0,
                  bp->olooking_x + max_dist * sin(th));
    glEnd();
    glPopMatrix();
  }
# endif /* DEBUG */
}


#ifdef WORDBUBBLES

/* Draw a cartoony word bubble.
   W and H are the inside size, for text.
   Origin is at bottom left.
   The bubble frame and arrow are outside that.
 */
static void
draw_bubble_box (ModeInfo *mi,
                 GLfloat width, GLfloat height,
                 GLfloat corner_radius,
                 GLfloat arrow_h, GLfloat arrow_x,
                 GLfloat fg[4], GLfloat bg[4])
{

# define CORNER_POINTS 16
  GLfloat outline_points[ (CORNER_POINTS + 2) * 4 + 8 ][3];
  int i = 0;
  GLfloat th;
  GLfloat tick = M_PI / 2 / CORNER_POINTS;

  GLfloat arrow_w = arrow_h / 2;
  GLfloat arrow_x2 = MAX(0, MIN(width - arrow_w, arrow_x));

  GLfloat w2 = MAX(arrow_w, width  - corner_radius * 1.10);
  GLfloat h2 = MAX(0,       height - corner_radius * 1.28);
  GLfloat x2 = (width  - w2) / 2;
  GLfloat y2 = (height - h2) / 2;
					/*        A  B         C   D	*/
  GLfloat xa = x2 -corner_radius;	/*    E     _------------_	*/
  GLfloat xb = x2;			/*    D   /__|         |__\	*/
  GLfloat xc = xb + w2;			/*        |  |         |  |	*/
  GLfloat xd = xc + corner_radius;	/*    C   |__|   EF    |__|	*/
  GLfloat xe = xb + arrow_x2;		/*    B    \_|_________|_/	*/
  GLfloat xf = xe + arrow_w;		/*    A          \|		*/

  GLfloat ya = y2 - (corner_radius + arrow_h);
  GLfloat yb = y2 - corner_radius;
  GLfloat yc = y2;
  GLfloat yd = yc + h2;
  GLfloat ye = yd + corner_radius;

  GLfloat z = 0;

  /* Let the lines take precedence over the fills. */
  glEnable (GL_POLYGON_OFFSET_FILL);
  glPolygonOffset (1.0, 1.0);

  glColor4fv (bg);
  glFrontFace(GL_CW);

  /* top left corner */

  glBegin (GL_TRIANGLE_FAN);
  glVertex3f (xb, yd, 0);
  for (th = 0; th < M_PI/2 + tick; th += tick)
    {
      GLfloat x = xb - corner_radius * cos(th);
      GLfloat y = yd + corner_radius * sin(th);
      glVertex3f (x, y, z);
      outline_points[i][0] = x;
      outline_points[i][1] = y;
      outline_points[i][2] = z;
      i++;
    }
  glEnd();

  /* top edge */
  outline_points[i][0] = xc;
  outline_points[i][1] = ye;
  outline_points[i][2] = z;
  i++;

  /* top right corner */

  glBegin (GL_TRIANGLE_FAN);
  glVertex3f (xc, yd, 0);
  for (th = M_PI/2; th > -tick; th -= tick)
    {
      GLfloat x = xc + corner_radius * cos(th);
      GLfloat y = yd + corner_radius * sin(th);
      glVertex3f (x, y, z);
      outline_points[i][0] = x;
      outline_points[i][1] = y;
      outline_points[i][2] = z;
      i++;
    }
  glEnd();

  /* right edge */
  outline_points[i][0] = xd;
  outline_points[i][1] = yc;
  outline_points[i][2] = z;
  i++;

  /* bottom right corner */

  glBegin (GL_TRIANGLE_FAN);
  glVertex3f (xc, yc, 0);
  for (th = 0; th < M_PI/2 + tick; th += tick)
    {
      GLfloat x = xc + corner_radius * cos(th);
      GLfloat y = yc - corner_radius * sin(th);
      glVertex3f (x, y, z);
      outline_points[i][0] = x;
      outline_points[i][1] = y;
      outline_points[i][2] = z;
      i++;
    }
  glEnd();

  /* bottom right edge */
  outline_points[i][0] = xf;
  outline_points[i][1] = yb;
  outline_points[i][2] = z;
  i++;

  /* arrow triangle */
  glFrontFace(GL_CW);
  glBegin (GL_TRIANGLES);

  /* bottom arrow point */
  outline_points[i][0] = xf;
  outline_points[i][1] = yb;
  outline_points[i][2] = z;
  glVertex3f (outline_points[i][0],
              outline_points[i][1],
              outline_points[i][2]);
  i++;

  /* bottom right edge */
  outline_points[i][0] = xf;
  outline_points[i][1] = ya;
  outline_points[i][2] = z;
  glVertex3f (outline_points[i][0],
              outline_points[i][1],
              outline_points[i][2]);
  i++;

  outline_points[i][0] = xe;
  outline_points[i][1] = yb;
  outline_points[i][2] = z;
  glVertex3f (outline_points[i][0],
              outline_points[i][1],
              outline_points[i][2]);
  i++;
  glEnd();


  /* bottom left corner */

  glBegin (GL_TRIANGLE_FAN);
  glVertex3f (xb, yc, 0);
  for (th = M_PI/2; th > -tick; th -= tick)
    {
      GLfloat x = xb - corner_radius * cos(th);
      GLfloat y = yc - corner_radius * sin(th);
      glVertex3f (x, y, z);
      outline_points[i][0] = x;
      outline_points[i][1] = y;
      outline_points[i][2] = z;
      i++;
    }
  glEnd();

  glFrontFace(GL_CCW);

  /* left edge */
  outline_points[i][0] = xa;
  outline_points[i][1] = yd;
  outline_points[i][2] = z;
  i++;

  glFrontFace(GL_CW);
  glBegin (GL_QUADS);
  /* left box */
  glVertex3f (xa, yd, z);
  glVertex3f (xb, yd, z);
  glVertex3f (xb, yc, z);
  glVertex3f (xa, yc, z);

  /* center box */
  glVertex3f (xb, ye, z);
  glVertex3f (xc, ye, z);
  glVertex3f (xc, yb, z);
  glVertex3f (xb, yb, z);

  /* right box */
  glVertex3f (xc, yd, z);
  glVertex3f (xd, yd, z);
  glVertex3f (xd, yc, z);
  glVertex3f (xc, yc, z);

  glEnd();

  glLineWidth (2.8);
  glColor4fv (fg);

  glBegin (GL_LINE_LOOP);
  while (i > 0)
    glVertex3fv (outline_points[--i]);
  glEnd();

  glDisable (GL_POLYGON_OFFSET_FILL);
}


static void
draw_label (ModeInfo *mi, walker *f, GLfloat y_off, GLfloat scale,
            const char *label)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat m[4][4];

  if (scale == 0) return;

  if (!wire)
    glDisable (GL_LIGHTING);   /* don't light fonts */

  glPushMatrix();

  /* First, we translate the origin to the center of the robot.

     Then we retrieve the prevailing modelview matrix, which
     includes any rotation, wandering, and user-trackball-rolling
     of the scene.

     We set the top 3x3 cells of that matrix to be the identity
     matrix.  This removes all rotation from the matrix, while
     leaving the translation alone.  This has the effect of
     leaving the prevailing coordinate system perpendicular to
     the camera view: were we to draw a square face, it would
     be in the plane of the screen.

     Now we translate by `size' toward the viewer -- so that the
     origin is *just in front* of the ball.

     Then we draw the label text, allowing the depth buffer to
     do its work: that way, labels on atoms will be occluded
     properly when other atoms move in front of them.

     This technique (of neutralizing rotation relative to the
     observer, after both rotations and translations have been
     applied) is known as "billboarding".
   */

  if (f)
    glTranslatef(f->y, 0, f->x);                   /* get matrix */

  glTranslatef (0, y_off, 0);

  glGetFloatv (GL_MODELVIEW_MATRIX, &m[0][0]);  /* load rot. identity */
  m[0][0] = 1; m[1][0] = 0; m[2][0] = 0;
  m[0][1] = 0; m[1][1] = 1; m[2][1] = 0;
  m[0][2] = 0; m[1][2] = 0; m[2][2] = 1;
  glLoadIdentity();                             /* reset modelview */
  glMultMatrixf (&m[0][0]);                     /* replace with ours */

  glTranslatef (0, 0, 0.1);		        /* move toward camera */

  glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */

  {
    XCharStruct e;
    int cw, ch, w, h, ascent, descent;
    GLfloat s;
    GLfloat max = 24;   /* max point size to avoid pixellated text */

    /* Let the font be much larger on iPhone */
    if (mi->xgwa.height <= 640 || mi->xgwa.width <= 640)
      max *= 3;
    
    texture_string_metrics (bp->font_data, "X", &e, &ascent, &descent);
    cw = e.width;
    ch = ascent + descent;
    s = 1.0 / ch;
    if (ch > max) s *= max/ch;

    s *= scale;

    texture_string_metrics (bp->font_data, label, &e, 0, 0);
    w = e.width;
    h = e.ascent + e.descent;

    glScalef (s, s, 1);
    glTranslatef (-w/2, h*2/3 + (cw * 7), 0);

    glPushMatrix();
    glTranslatef (0, -h + (ch * 1.2), -0.1);
    draw_bubble_box (mi, w, h, 
                     ch * 2,		/* corner radius */
                     ch * 2.5,		/* arrow height */
                     w / 2 - cw * 8,	/* arrow x */
                     bp->text_bd, bp->text_bg);
    glPopMatrix();

    glColor4fv (bp->text_color);
    glTranslatef (0, ch/2, 0);
    print_texture_string (bp->font_data, label);
  }

  glPopMatrix();

  /* More efficient to always call glEnable() with correct values
     than to call glPushAttrib()/glPopAttrib(), since reading
     attributes from GL does a round-trip and  stalls the pipeline.
   */
  if (!wire)
    glEnable (GL_LIGHTING);
}


static void
fill_words (ModeInfo *mi)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  char *p = bp->words + strlen(bp->words);
  char *c;
  int lines = 0;
  int max = bp->max_lines;

  /* Fewer lines on iPhone */
  if ((mi->xgwa.height <= 640 || mi->xgwa.width <= 640) &&
      max > 4)
    max = 4;

  for (c = bp->words; c < p; c++)
    if (*c == '\n')
      lines++;

  while (p < bp->words + sizeof(bp->words) - 1 &&
         lines < max)
    {
      int c = textclient_getc (bp->tc);
      if (c == '\n')
        lines++;
      if (c > 0)
        *p++ = (char) c;
      else
        break;
    }
  *p = 0;

  bp->lines = lines;
}


static void
bubble (ModeInfo *mi)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int duration = 200;
  GLfloat fade = 0.015;
  int chance = (talk_chance <= 0.0  ? 0 :
                talk_chance >= 0.99 ? 1 :
                (1-talk_chance) * 1000);
  GLfloat scale;
  char *s0 = strdup (bp->words);
  char *s = s0;
  int L;

  while (*s == '\n') s++;
  L = strlen(s);
  while (L > 0 && (s[L-1] == '\n' || s[L-1] == ' ' || s[L-1] == '\t'))
    s[--L] = 0;
  if (! *s) goto DONE;

# ifdef DEBUG
  if (debug_p) goto DONE;
# endif

  if (chance <= 0) goto DONE;

  if (bp->bubble_tick > 0)
    {
      bp->bubble_tick--;
      if (! bp->bubble_tick)
        *bp->words = 0;
    }

  if (! bp->bubble_tick)
    {
      if (!(random() % chance))
        bp->bubble_tick = duration;
      else
        goto DONE;
    }

  scale = (bp->bubble_tick < duration * fade
           ? bp->bubble_tick / (duration * fade)
           : (bp->bubble_tick > duration * (1 - fade)
              ? 1 - ((bp->bubble_tick - duration * (1 - fade))
                     / (duration * fade))
              : 1));

  draw_label (mi, &bp->walkers[0], 1.5, scale, s);

 DONE:
  free (s0);
}
#endif /* WORDBUBBLES */



typedef struct {
  int i;
  GLdouble d;
} depth_sorter;

static int
cmp_depth_sorter (const void *aa, const void *bb)
{
  const depth_sorter *a = (depth_sorter *) aa;
  const depth_sorter *b = (depth_sorter *) bb;
  return (a->d == b->d ? 0 : a->d < b->d ? -1 : 1);
}


ENTRYPOINT void
draw_robot (ModeInfo *mi)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat robot_size;
  depth_sorter *sorted;
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

  glPushMatrix ();

  glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */
  gltrackball_rotate (bp->user_trackball);

  glTranslatef (0, -20, 0);  /* Move the horizon down the screen */

  robot_size = size * 7;
  glScalef (robot_size, robot_size, robot_size);
  look_at_center (mi);

  glPushMatrix();
  glScalef (1/robot_size, 1/robot_size, 1/robot_size);
  glCallList (bp->dlists[GROUND]);
  glPopMatrix();

# ifdef WORDBUBBLES
  fill_words (mi);
  bubble (mi);
# endif

#ifdef DEBUG
  if (debug_p)
    {
      glTranslatef (0, -1.2, 0);
      glScalef (3, 3, 3);
      glRotatef (-43.5, 0, 0, 1);
      glRotatef (-90, 0, 1, 0);

      /* glTranslatef (bp->debug_z, bp->debug_y, 0); */

      glPushMatrix();
      if (!MI_IS_WIREFRAME(mi)) glDisable(GL_LIGHTING);
      if (!MI_IS_WIREFRAME(mi) && do_texture) glDisable(GL_TEXTURE_2D);

      glBegin(GL_LINES);
      glVertex3f(-10, 0, 0); glVertex3f(10, 0, 0);
      glVertex3f(0, -10, 0); glVertex3f(0, 10, 0);
      glVertex3f(0, 0, -10); glVertex3f(0, 0, 10);
      glEnd();

      glTranslatef (-0.5, 0, -0.5);

      glColor3f (1, 0, 0);
      glBegin (GL_LINE_LOOP);
      glVertex3f (0, 0, 0); glVertex3f (0, 0, 1);
      glVertex3f (0, 1, 1); glVertex3f (0, 1, 0);
      glEnd();

      glBegin (GL_LINE_LOOP);
      glVertex3f (1, 0, 0); glVertex3f (1, 0, 1);
      glVertex3f (1, 1, 1); glVertex3f (1, 1, 0);
      glEnd();

# if 1
      glColor3f (0.5, 0.5, 0.5);
      glFrontFace (GL_CCW);
      glBegin (GL_QUADS);
      /* glVertex3f (0, 1, 0); glVertex3f (0, 1, 1); */
      /* glVertex3f (1, 1, 1); glVertex3f (1, 1, 0); */
      glVertex3f (0, 0, 0); glVertex3f (0, 0, 1);
      glVertex3f (1, 0, 1); glVertex3f (1, 0, 0);
      glEnd();

      glFrontFace (GL_CW);
      glBegin (GL_QUADS);
      glVertex3f (0, 1, 0); glVertex3f (0, 1, 1);
      glVertex3f (1, 1, 1); glVertex3f (1, 1, 0);
      glVertex3f (0, 0, 0); glVertex3f (0, 0, 1);
      glVertex3f (1, 0, 1); glVertex3f (1, 0, 0);
      glEnd();
# endif

      glColor3f (1, 0, 0);
      glBegin (GL_LINES);
      glVertex3f (0, 0, 0); glVertex3f (1, 0, 0);
      glVertex3f (0, 0, 1); glVertex3f (1, 0, 1);
      glVertex3f (0, 1, 0); glVertex3f (1, 1, 0);
      glVertex3f (0, 1, 1); glVertex3f (1, 1, 1);

      glVertex3f (0, 0, 0); glVertex3f (1, 0, 1);
      glVertex3f (0, 0, 1); glVertex3f (1, 0, 0);
      glVertex3f (0, 1, 0); glVertex3f (1, 1, 1);
      glVertex3f (0, 1, 1); glVertex3f (1, 1, 0);
      glEnd();

      if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
      if (!MI_IS_WIREFRAME(mi) && do_texture) glEnable(GL_TEXTURE_2D);

      glPopMatrix();
    }

# endif /* DEBUG */

  /* For transparency to work right, we have to draw the robots from
     back to front, from the perspective of the observer.  So project
     the origin of the robot to screen coordinates, and sort that by Z.
   */
  sorted = (depth_sorter *) calloc (bp->nwalkers, sizeof (*sorted));
  {
    GLdouble m[16], p[16];
    GLint v[4];
    glGetDoublev (GL_MODELVIEW_MATRIX, m);
    glGetDoublev (GL_PROJECTION_MATRIX, p);
    glGetIntegerv (GL_VIEWPORT, v);

    for (i = 0; i < bp->nwalkers; i++)
      {
        GLdouble x, y, z;
        walker *f = &bp->walkers[i];
        gluProject (f->y, f->z, f->x, m, p, v, &x, &y, &z);
        sorted[i].i = i;
        sorted[i].d = -z;
      }
    qsort (sorted, i, sizeof(*sorted), cmp_depth_sorter);
  }


  mi->polygon_count = 0;
  for (i = 0; i < bp->nwalkers; i++)
    {
      int ii = sorted[i].i;
      walker *f = &bp->walkers[ii];
      int ticks = 22 * speed * f->speed;
      int max = 180;
      char tag[1024];
      *tag = 0;

      if (ticks < 1) ticks = 1;
      if (ticks > max) ticks = max;

# ifdef DEBUG
      if (debug_p)
        sprintf (tag, "%.4f, %.4f,  %.4f",
                 bp->debug_x, bp->debug_y, bp->debug_z);
      else
        {
#  if 1
          /* sprintf (tag + strlen(tag), "    %d\n", ii); */
          sprintf (tag + strlen(tag), "    %d\n", bp->nwalkers - i - 1);
          /* sprintf (tag + strlen(tag), "%.03f\n", sorted[i].d); */
#  endif
        }

# endif /* DEBUG */

      mi->polygon_count += draw_walker (mi, f, tag);

      for (ii = 0; ii < ticks; ii++)
        tick_walker (mi, f);
    }
  free (sorted);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

ENTRYPOINT void
free_robot (ModeInfo *mi)
{
  robot_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->user_trackball) gltrackball_free (bp->user_trackball);
  if (bp->walkers) free (bp->walkers);
  if (robot_dome) free (robot_dome);
  robot_dome = 0;
  if (robot_gear) free (robot_gear);
  robot_gear = 0;
  if (ground) free (ground);
  ground = 0;

# ifdef WORDBUBBLES
  if (bp->tc) textclient_close (bp->tc);
  if (bp->font_data) free_texture_font (bp->font_data);
# endif

# ifdef HAVE_TEXTURE
  if (bp->chrome_texture) glDeleteTextures(1, &bp->chrome_texture);
# endif
  if (bp->dlists) {
    for (i = 0; i < countof(all_objs); i++)
      if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
    free (bp->dlists);
  }
}

XSCREENSAVER_MODULE_2 ("WindupRobot", winduprobot, robot)

#endif /* USE_GL */
