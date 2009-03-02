/* pinion, Copyright (c) 2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS       "Pinion"
#define HACK_INIT       init_pinion
#define HACK_DRAW       draw_pinion
#define HACK_RESHAPE    reshape_pinion
#define HACK_HANDLE_EVENT pinion_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts        xlockmore_opts

#define DEF_SPIN_SPEED   "1.0"
#define DEF_SCROLL_SPEED "1.0"
#define DEF_GEAR_SIZE    "1.0"
#define DEF_MAX_RPM      "900"

#define DEFAULTS        "*delay:        15000              \n" \
                        "*showFPS:      False              \n" \
                        "*wireframe:    False              \n" \
                        "*spinSpeed:  " DEF_SPIN_SPEED   " \n" \
                        "*scrollSpeed:" DEF_SCROLL_SPEED " \n" \
                        "*maxRPM:     " DEF_MAX_RPM      " \n" \
                        "*gearSize:   " DEF_GEAR_SIZE    " \n" \
                        "*titleFont:  -*-times-bold-r-normal-*-180-*\n" \
                        "*titleFont2: -*-times-bold-r-normal-*-120-*\n" \
                        "*titleFont3: -*-times-bold-r-normal-*-80-*\n"  \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#include "xlockmore.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

typedef struct {
  unsigned long id;          /* unique name */
  double x, y, z;            /* position */
  double r;                  /* radius of the gear, at middle of teeth */
  double th;                 /* rotation (degrees) */

  GLint nteeth;              /* how many teeth */
  double tooth_w, tooth_h;   /* size of teeth */

  double inner_r;            /* radius of the (larger) inside hole */
  double inner_r2;           /* radius of the (smaller) inside hole, if any */
  double inner_r3;           /* yet another */

  double thickness;          /* height of the edge */
  double thickness2;         /* height of the (smaller) inside disc if any */
  double thickness3;         /* yet another */
  int spokes;                /* how many spokes inside, if any */
  int nubs;                  /* how many little nubbly bits, if any */
  double spoke_thickness;    /* spoke versus hole */
  GLfloat wobble;            /* factory defect! */
  int motion_blur_p;	     /* whether it's spinning too fast to draw */
  int polygons;              /* how many polys in this gear */

  double ratio;              /* gearing ratio with previous gears */
  double rpm;                /* approximate revolutions per minute */

  Bool base_p;               /* whether this gear begins a new train */
  int coax_p;                /* whether this is one of a pair of bound gears.
                                1 for first, 2 for second. */
  double coax_thickness;     /* thickness of the other gear in the pair */
  GLfloat color[4];
  GLfloat color2[4];

  GLuint dlist;
} gear;


typedef struct {
  GLXContext *glx_context;
  GLfloat vp_left, vp_right, vp_top, vp_bottom;    /* default visible area */
  GLfloat vp_width, vp_height;
  GLfloat render_left, render_right;  /* area in which gears are displayed */
  GLfloat layout_left, layout_right;  /* layout region, on the right side  */

  int ngears;
  int gears_size;
  gear **gears;

  trackball_state *trackball;
  Bool button_down_p;
  unsigned long mouse_gear_id;

  XFontStruct *xfont1, *xfont2, *xfont3;
  GLuint font1_dlist, font2_dlist, font3_dlist;
  GLuint title_list;

} pinion_configuration;


static pinion_configuration *pps = NULL;
static GLfloat spin_speed, scroll_speed, max_rpm, gear_size;
static GLfloat plane_displacement = 0.1;  /* distance between coaxial gears */

static Bool verbose_p = False;            /* print progress on stderr */
static Bool debug_placement_p = False;    /* extreme verbosity on stderr */
static Bool debug_p = False;              /* render as flat schematic */
static Bool debug_one_gear_p = False;     /* draw one big stationary gear */
static Bool wire_all_p = False;           /* in wireframe, do not abbreviate */

static int debug_size_failures;           /* for debugging messages */
static int debug_position_failures;
static unsigned long current_length;      /* gear count in current train */
static unsigned long current_blur_length; /* how long have we been blurring? */


static XrmOptionDescRec opts[] = {
  { "-spin",   ".spinSpeed",   XrmoptionSepArg, 0 },
  { "-scroll", ".scrollSpeed", XrmoptionSepArg, 0 },
  { "-size",   ".gearSize",    XrmoptionSepArg, 0 },
  { "-max-rpm",".maxRPM",      XrmoptionSepArg, 0 },
  { "-debug",  ".debug",       XrmoptionNoArg, "True" },
  { "-verbose",".verbose",     XrmoptionNoArg, "True" },
};

static argtype vars[] = {
  {&spin_speed,   "spinSpeed",   "SpinSpeed",   DEF_SPIN_SPEED,   t_Float},
  {&scroll_speed, "scrollSpeed", "ScrollSpeed", DEF_SCROLL_SPEED, t_Float},
  {&gear_size,    "gearSize",    "GearSize",    DEF_GEAR_SIZE,    t_Float},
  {&max_rpm,      "maxRPM",      "MaxRPM",      DEF_MAX_RPM,      t_Float},
  {&debug_p,      "debug",       "Debug",       "False",          t_Bool},
  {&verbose_p,    "verbose",     "Verbose",     "False",          t_Bool},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Computing normal vectors
 */

typedef struct {
  double x,y,z;
} XYZ;

/* Calculate the unit normal at p given two other points p1,p2 on the
   surface. The normal points in the direction of p1 crossproduct p2
 */
static XYZ
calc_normal (XYZ p, XYZ p1, XYZ p2)
{
  XYZ n, pa, pb;
  pa.x = p1.x - p.x;
  pa.y = p1.y - p.y;
  pa.z = p1.z - p.z;
  pb.x = p2.x - p.x;
  pb.y = p2.y - p.y;
  pb.z = p2.z - p.z;
  n.x = pa.y * pb.z - pa.z * pb.y;
  n.y = pa.z * pb.x - pa.x * pb.z;
  n.z = pa.x * pb.y - pa.y * pb.x;
  return (n);
}

static void
do_normal(GLfloat x1, GLfloat y1, GLfloat z1,
          GLfloat x2, GLfloat y2, GLfloat z2,
          GLfloat x3, GLfloat y3, GLfloat z3)
{
  XYZ p1, p2, p3, p;
  p1.x = x1; p1.y = y1; p1.z = z1;
  p2.x = x2; p2.y = y2; p2.z = z2;
  p3.x = x3; p3.y = y3; p3.z = z3;
  p = calc_normal (p1, p2, p3);
  glNormal3f (p.x, p.y, p.z);
}


/* Font stuff
 */

static void
load_font (ModeInfo *mi, char *res, XFontStruct **fontP, GLuint *dlistP)
{
  const char *font = get_string_resource (res, "Font");
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-times-bold-r-normal-*-180-*";

  f = XLoadQueryFont(mi->dpy, font);
  if (!f) f = XLoadQueryFont(mi->dpy, "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  *dlistP = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");
  glXUseXFont(id, first, last-first+1, *dlistP + first);
  check_gl_error ("glXUseXFont");

  *fontP = f;
}


static void
load_fonts (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  load_font (mi, "titleFont",  &pp->xfont1, &pp->font1_dlist);
  load_font (mi, "titleFont2", &pp->xfont2, &pp->font2_dlist);
  load_font (mi, "titleFont3", &pp->xfont3, &pp->font3_dlist);
}


static int
string_width (XFontStruct *f, const char *c)
{
  int w = 0;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      w += (f->per_char
            ? f->per_char[cc-f->min_char_or_byte2].rbearing
            : f->min_bounds.rbearing);
      c++;
    }
  return w;
}

static void
print_title_string (ModeInfo *mi, const char *string,
                    GLfloat x, GLfloat y,
                    XFontStruct *font, int font_dlist)
{
  GLfloat line_height = font->ascent + font->descent;
  GLfloat sub_shift = (line_height * 0.3);
  int cw = string_width (font, "m");
  int tabs = cw * 7;

  y -= line_height;

  glPushAttrib (GL_TRANSFORM_BIT |  /* for matrix contents */
                GL_ENABLE_BIT);     /* for various glDisable calls */
  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
      glLoadIdentity();

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        int i;
        int x2 = x;
        Bool sub_p = False;
        glLoadIdentity();

        gluOrtho2D (0, mi->xgwa.width, 0, mi->xgwa.height);

        glColor3f (0.8, 0.8, 0);

        glRasterPos2f (x, y);
        for (i = 0; i < strlen(string); i++)
          {
            char c = string[i];
            if (c == '\n')
              {
                glRasterPos2f (x, (y -= line_height));
                x2 = x;
              }
            else if (c == '\t')
              {
                x2 -= x;
                x2 = ((x2 + tabs) / tabs) * tabs;  /* tab to tab stop */
                x2 += x;
                glRasterPos2f (x2, y);
              }
            else if (c == '[' && (isdigit (string[i+1])))
              {
                sub_p = True;
                glRasterPos2f (x2, (y -= sub_shift));
              }
            else if (c == ']' && sub_p)
              {
                sub_p = False;
                glRasterPos2f (x2, (y += sub_shift));
              }
            else
              {
                glCallList (font_dlist + (int)(c));
                x2 += (font->per_char
                       ? font->per_char[c - font->min_char_or_byte2].width
                       : font->min_bounds.width);
              }
          }
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
  }
  glPopAttrib();

  glMatrixMode(GL_MODELVIEW);
}

static void rpm_string (double rpm, char *s);

static void
new_label (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  char label[1024];
  int i;
  gear *g = 0;

  if (pp->mouse_gear_id)
    for (i = 0; i < pp->ngears; i++)
      if (pp->gears[i]->id == pp->mouse_gear_id)
        {
          g = pp->gears[i];
          break;
        }

  if (!g)
    *label = 0;
  else
    {
      sprintf (label, "%d teeth\n", g->nteeth);
      rpm_string (g->rpm, label + strlen(label));
    }

  glNewList (pp->title_list, GL_COMPILE);
  if (*label)
    {
      XFontStruct *f;
      GLuint fl;
      if (MI_WIDTH(mi) >= 500 && MI_HEIGHT(mi) >= 375)
        f = pp->xfont1, fl = pp->font1_dlist;                  /* big font */
      else if (MI_WIDTH(mi) >= 350 && MI_HEIGHT(mi) >= 260)
        f = pp->xfont2, fl = pp->font2_dlist;                  /* small font */
      else
        f = pp->xfont3, fl = pp->font3_dlist;                  /* tiny font */

      print_title_string (mi, label,
                          10, mi->xgwa.height - 10,
                          f, fl);
    }
  glEndList ();
}


/* Some utilities
 */


/* Find the gear in the scene that is farthest to the right or left.
 */
static gear *
farthest_gear (ModeInfo *mi, Bool left_p)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int i;
  gear *rg = 0;
  double x = (left_p ? 999999 : -999999);
  for (i = 0; i < pp->ngears; i++)
    {
      gear *g = pp->gears[i];
      double gx = g->x + ((g->r + g->tooth_h) * (left_p ? -1 : 1));
      if (left_p ? (x > gx) : (x < gx))
        {
          rg = g;
          x = gx;
        }
    }
  return rg;
}


/* Compute the revolutions per minute of a gear.
 */
static void
compute_rpm (ModeInfo *mi, gear *g)
{
  double fps, rpf, rps;
  fps = (MI_PAUSE(mi) == 0 ? 999999 : 1000000.0 / MI_PAUSE(mi));

  if (fps > 150) fps = 150;  /* let's be reasonable... */
  if (fps < 10)  fps = 10;

  rpf    = (g->ratio * spin_speed) / 360.0;   /* rotations per frame */
  rps    = rpf * fps;                         /* rotations per second */
  g->rpm = rps * 60;
}

/* Prints the RPM into a string, doing fancy float formatting.
 */
static void
rpm_string (double rpm, char *s)
{
  char buf[30];
  int L;
  if (rpm >= 0.1)          sprintf (buf, "%.2f", rpm);
  else if (rpm >= 0.001)   sprintf (buf, "%.4f", rpm);
  else if (rpm >= 0.00001) sprintf (buf, "%.8f", rpm);
  else                     sprintf (buf, "%.16f",rpm);

  L = strlen(buf);
  while (buf[L-1] == '0') buf[--L] = 0;
  if (buf[L-1] == '.') buf[--L] = 0;
  strcpy (s, buf);
  strcat (s, " RPM");
}



/* Which of the gear's inside rings is the biggest? 
 */
static int
biggest_ring (gear *g, double *posP, double *sizeP, double *heightP)
{
  double r0 = (g->r - g->tooth_h/2);
  double r1 = g->inner_r;
  double r2 = g->inner_r2;
  double r3 = g->inner_r3;
  double w1 = (r1 ? r0 - r1 : r0);
  double w2 = (r2 ? r1 - r2 : 0);
  double w3 = (r3 ? r2 - r3 : 0);
  double h1 = g->thickness;
  double h2 = g->thickness2;
  double h3 = g->thickness3;

  if (g->spokes) w2 = 0;

  if (w1 > w2 && w1 > w3)
    {
      if (posP)    *posP = (r0+r1)/2;
      if (sizeP)   *sizeP = w1;
      if (heightP) *heightP = h1;
      return 0;
    }
  else if (w2 > w1 && w2 > w3)
    {
      if (posP)  *posP = (r1+r2)/2;
      if (sizeP) *sizeP = w2;
      if (heightP) *heightP = h2;
      return 1;
    }
  else
    {
      if (posP)  *posP = (r2+r3)/2;
      if (sizeP) *sizeP = w3;
      if (heightP) *heightP = h3;
      return 1;
    }
}


/* Layout and stuff.
 */


/* Create and return a new gear sized for placement next to or on top of
   the given parent gear (if any.)  Returns 0 if out of memory.
 */
static gear *
new_gear (ModeInfo *mi, gear *parent, Bool coaxial_p)
{
  /* pinion_configuration *pp = &pps[MI_SCREEN(mi)]; */
  gear *g = (gear *) calloc (1, sizeof (*g));
  int loop_count = 0;
  static unsigned long id = 0;

  if (!g) return 0;
  if (coaxial_p && !parent) abort();
  g->id = ++id;

  while (1)
    {
      loop_count++;
      if (loop_count > 1000)
        /* The only time we loop in here is when making a coaxial gear, and
           trying to pick a radius that is either significantly larger or
           smaller than its parent.  That shouldn't be hard, so something
           must be really wrong if we can't do that in only a few tries.
         */
        abort();

      /* Pick the size of the teeth.
       */
      if (parent && !coaxial_p) /* adjascent gears need matching teeth */
        {
          g->tooth_w = parent->tooth_w;
          g->tooth_h = parent->tooth_h;
          g->thickness  = parent->thickness;
          g->thickness2 = parent->thickness2;
          g->thickness3 = parent->thickness3;
        }
      else                 /* gears that begin trains get any size they want */
        {
          double scale = (1.0 + BELLRAND(4.0)) * gear_size;
          g->tooth_w = 0.007 * scale;
          g->tooth_h = 0.005 * scale;
          g->thickness  = g->tooth_h * (0.1 + BELLRAND(1.5));
          g->thickness2 = g->thickness / 4;
          g->thickness3 = g->thickness;
        }

      /* Pick the number of teeth, and thus, the radius.
       */
      {
        double c;

      AGAIN:
        g->nteeth = 3 + (random() % 97);    /* from 3 to 100 teeth */

        if (g->nteeth < 7 && (random() % 5) != 0)
          goto AGAIN;   /* Let's make very small tooth-counts more rare */

        c = g->nteeth * g->tooth_w * 2;     /* circumference = teeth + gaps */
        g->r = c / (M_PI * 2);              /* c = 2 pi r  */
      }


      /* Are we done now?
       */
      if (! coaxial_p) break;   /* yes */
      if (g->nteeth == parent->nteeth) continue; /* ugly */
      if (g->r  < parent->r * 0.6) break;  /* g much smaller than parent */
      if (parent->r < g->r  * 0.6) break;  /* g much larger than parent  */
    }

  /* Colorize
   */
  g->color[0] = 0.5 + frand(0.5);
  g->color[1] = 0.5 + frand(0.5);
  g->color[2] = 0.5 + frand(0.5);
  g->color[3] = 1.0;

  g->color2[0] = g->color[0] * 0.85;
  g->color2[1] = g->color[1] * 0.85;
  g->color2[2] = g->color[2] * 0.85;
  g->color2[3] = g->color[3];


  /* Decide on shape of gear interior:
     - just a ring with teeth;
     - that, plus a thinner in-set "plate" in the middle;
     - that, plus a thin raised "lip" on the inner plate;
     - or, a wide lip (really, a thicker third inner plate.)
   */
  if ((random() % 10) == 0)
    {
      /* inner_r can go all the way in; there's no inset disc. */
      g->inner_r = (g->r * 0.1) + frand((g->r - g->tooth_h/2) * 0.8);
      g->inner_r2 = 0;
      g->inner_r3 = 0;
    }
  else
    {
      /* inner_r doesn't go in very far; inner_r2 is an inset disc. */
      g->inner_r  = (g->r * 0.5)  + frand((g->r - g->tooth_h) * 0.4);
      g->inner_r2 = (g->r * 0.1) + frand(g->inner_r * 0.5);
      g->inner_r3 = 0;

      if (g->inner_r2 > (g->r * 0.2))
        {
          int nn = (random() % 10);
          if (nn <= 2)
            g->inner_r3 = (g->r * 0.1) + frand(g->inner_r2 * 0.2);
          else if (nn <= 7 && g->inner_r2 >= 0.1)
            g->inner_r3 = g->inner_r2 - 0.01;
        }
    }

  /* Coaxial gears need to have the same innermost hole size (for the axle.)
     Use whichever of the two is smaller.  (Modifies parent.)
   */
  if (coaxial_p)
    {
      double hole1 = (g->inner_r3 ? g->inner_r3 :
                      g->inner_r2 ? g->inner_r2 :
                      g->inner_r);
      double hole2 = (parent->inner_r3 ? parent->inner_r3 :
                      parent->inner_r2 ? parent->inner_r2 :
                      parent->inner_r);
      double hole = (hole1 < hole2 ? hole1 : hole2);
      if (hole <= 0) abort();

      if      (g->inner_r3) g->inner_r3 = hole;
      else if (g->inner_r2) g->inner_r2 = hole;
      else                  g->inner_r  = hole;

      if      (parent->inner_r3) parent->inner_r3 = hole;
      else if (parent->inner_r2) parent->inner_r2 = hole;
      else                       parent->inner_r  = hole;
    }

  /* If we have three discs, sometimes make the middle disc be spokes.
   */
  if (g->inner_r3 && ((random() % 5) == 0))
    {
      g->spokes = 2 + BELLRAND (5);
      g->spoke_thickness = 1 + frand(7.0);
      if (g->spokes == 2 && g->spoke_thickness < 2)
        g->spoke_thickness += 1;
    }

  /* Sometimes add little nubbly bits, if there is room.
   */
  if (g->nteeth > 5)
    {
      double size = 0;
      biggest_ring (g, 0, &size, 0);
      if (size > g->r * 0.2 && (random() % 5) == 0)
        {
          g->nubs = 1 + (random() % 16);
          if (g->nubs > 8) g->nubs = 1;
        }
    }

  if (g->inner_r3 > g->inner_r2) abort();
  if (g->inner_r2 > g->inner_r) abort();
  if (g->inner_r  > g->r) abort();

  g->base_p = !parent;

  return g;
}


/* Given a newly-created gear, place it next to its parent in the scene,
   with its teeth meshed and the proper velocity.  Returns False if it
   didn't work.  (Call this a bunch of times until either it works, or
   you decide it's probably not going to.)
 */
static Bool
place_gear (ModeInfo *mi, gear *g, gear *parent, Bool coaxial_p)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];

  /* If this gear takes up more than 1/3rd of the screen, it's no good.
   */
  if (((g->r + g->tooth_h) * (6 / gear_size)) >= pp->vp_width ||
      ((g->r + g->tooth_h) * (6 / gear_size)) >= pp->vp_height)
    {
      if (verbose_p && debug_placement_p && 0)
        fprintf (stderr,
                 "%s: placement: too big: %.2f @ %.2f vs %.2f x %.2f\n",
                 progname,
                 (g->r + g->tooth_h), gear_size,
                 pp->vp_width, pp->vp_height);
      debug_size_failures++;
      return False;
    }

  /* Compute this gear's velocity.
   */
  if (! parent)
    {
      g->ratio = 0.8 + BELLRAND(0.4);  /* 0.8-1.2 = 8-12rpm @ 60fps */
      g->th = frand (90) * ((random() & 1) ? 1.0 : -1.0);
    }
  else if (coaxial_p)
    {
      g->ratio = parent->ratio; /* bound gears have the same ratio */
      g->th = parent->th;
      g->rpm = parent->rpm;
      g->wobble = parent->wobble;
    }
  else
    {
      /* Gearing ratio is the ratio of the number of teeth to previous gear
         (which is also the ratio of the circumferences.)
       */
      g->ratio = (double) parent->nteeth / (double) g->nteeth;

      /* Set our initial rotation to match that of the previous gear,
         multiplied by the gearing ratio.  (This is finessed later,
         once we know the exact position of the gear relative to its
         parent.)
      */
      g->th = -(parent->th * g->ratio);

      if (g->nteeth & 1)    /* rotate 1/2 tooth-size if odd number of teeth */
        {
          double off = (180.0 / g->nteeth);
          if (g->th > 0)
            g->th += off;
          else
            g->th -= off;
        }

      /* ratios are cumulative for all gears in the train. */
      g->ratio *= parent->ratio;
    }


  /* Place the gear relative to the parent.
   */

  if (! parent)
    {
      gear *rg = farthest_gear (mi, False);
      double right = (rg ? rg->x + rg->r + rg->tooth_h : 0);
      if (right < pp->layout_left) /* place off screen */
        right = pp->layout_left;

      g->x = right + g->r + g->tooth_h + (0.01 / gear_size);
      g->y = 0;
      g->z = 0;

      if (debug_one_gear_p)
        g->x = 0;
    }
  else if (coaxial_p)
    {
      double off = plane_displacement;

      g->x = parent->x;
      g->y = parent->y;
      g->z = parent->z + (g->r > parent->r      /* small gear on top */
                          ? -off : off);

      if (parent->r > g->r)     /* mark which is top and which is bottom */
        {
          parent->coax_p = 1;
          g->coax_p      = 2;
          parent->wobble = 0;   /* looks bad when axle moves */
        }
      else
        {
          parent->coax_p = 2;
          g->coax_p      = 1;
          g->wobble      = 0;
        }

      g->coax_thickness      = parent->thickness;
      parent->coax_thickness = g->thickness;

      /* Don't let the train get too close to or far from the screen.
         If we're getting too close, give up on this gear.
         (But getting very far away is fine.)
       */
      if (g->z >=  off * 4 ||
          g->z <= -off * 4)
        {
          if (verbose_p && debug_placement_p)
            fprintf (stderr, "%s: placement: bad depth: %.2f\n",
                     progname, g->z);
          debug_position_failures++;
          return False;
        }
    }
  else                          /* position it somewhere next to the parent. */
    {
      double r_off = parent->r + g->r;
      int angle;

      if ((random() % 3) != 0)
        angle = (random() % 240) - 120;   /* mostly -120 to +120 degrees */
      else
        angle = (random() % 360) - 180;   /* sometimes -180 to +180 degrees */

      g->x = parent->x + (cos ((double) angle * (M_PI / 180)) * r_off);
      g->y = parent->y + (sin ((double) angle * (M_PI / 180)) * r_off);
      g->z = parent->z;

      /* If the angle we picked would have positioned this gear
         more than halfway off screen, that's no good. */
      if (g->y > pp->vp_top ||
          g->y < pp->vp_bottom)
        {
          if (verbose_p && debug_placement_p)
            fprintf (stderr, "%s: placement: out of bounds: %s\n",
                     progname, (g->y > pp->vp_top ? "top" : "bottom"));
          debug_position_failures++;
          return False;
        }

      /* avoid accidentally changing sign of "th" in the math below. */
      g->th += (g->th > 0 ? 360 : -360);

      /* Adjust the rotation of the gear so that its teeth line up with its
         parent, based on the position of the gear and the current rotation
         of the parent.
       */
      {
        double p_c = 2 * M_PI * parent->r;  /* circumference of parent */
        double g_c = 2 * M_PI * g->r;       /* circumference of g  */

        double p_t = p_c * (angle/360.0);   /* distance travelled along
                                               circumference of parent when
                                               moving "angle" degrees along
                                               parent. */
        double g_rat = p_t / g_c;           /* if travelling that distance
                                               along circumference of g,
                                               ratio of g's circumference
                                               travelled. */
        double g_th = 360.0 * g_rat;        /* that ratio in degrees */

        g->th += angle + g_th;
      }
    }

  if (debug_one_gear_p)
    {
      compute_rpm (mi, g);
      return True;
    }

  /* If the position we picked for this gear would cause it to already
     be visible on the screen, give up.  This can happen when the train
     is growing backwards, and we don't want to see gears flash into
     existence.
   */
  if (g->x - g->r - g->tooth_h < pp->render_right)
    {
      if (verbose_p && debug_placement_p)
        fprintf (stderr, "%s: placement: out of bounds: left\n", progname);
      debug_position_failures++;
      return False;
    }

  /* If the position we picked for this gear causes it to overlap
     with any earlier gear in the train, give up.
   */
  {
    int i;

    for (i = pp->ngears-1; i >= 0; i--)
      {
        gear *og = pp->gears[i];

        if (og == g) continue;
        if (og == parent) continue;
        if (g->z != og->z) continue;    /* Ignore unless on same layer */

        /* Collision detection without sqrt:
             d = sqrt(a^2 + b^2)   d^2 = a^2 + b^2
             d < r1 + r2           d^2 < (r1 + r2)^2
         */
        if (((g->x - og->x) * (g->x - og->x) +
             (g->y - og->y) * (g->y - og->y)) <
            ((g->r + g->tooth_h + og->r + og->tooth_h) *
             (g->r + g->tooth_h + og->r + og->tooth_h)))
          {
            if (verbose_p && debug_placement_p)
              fprintf (stderr, "%s: placement: collision with %lu\n",
                       progname, og->id);
            debug_position_failures++;
            return False;
          }
      }
  }

  compute_rpm (mi, g);


  /* Make deeper gears be darker.
   */
  {
    double depth = g->z / plane_displacement;
    double brightness = 1 + (depth / 6);
    double limit = 0.4;
    if (brightness < limit)   brightness = limit;
    if (brightness > 1/limit) brightness = 1/limit;
    g->color[0]  *= brightness;
    g->color[1]  *= brightness;
    g->color[2]  *= brightness;
    g->color2[0] *= brightness;
    g->color2[1] *= brightness;
    g->color2[2] *= brightness;
  }

  /* If a single frame of animation would cause the gear to rotate by
     more than 1/2 the size of a single tooth, then it won't look right:
     the gear will appear to be turning at some lower harmonic of its
     actual speed.
   */
  {
    double ratio = g->ratio * spin_speed;
    double blur_limit = 180.0 / g->nteeth;

    if (ratio > blur_limit)
      g->motion_blur_p = 1;

    if (!coaxial_p)
      {
        /* ride until the wheels fall off... */
        if (ratio > blur_limit * 0.7) g->wobble += (random() % 2);
        if (ratio > blur_limit * 0.9) g->wobble += (random() % 2);
        if (ratio > blur_limit * 1.1) g->wobble += (random() % 2);
        if (ratio > blur_limit * 1.3) g->wobble += (random() % 2);
        if (ratio > blur_limit * 1.5) g->wobble += (random() % 2);
        if (ratio > blur_limit * 1.7) g->wobble += (random() % 2);
      }
  }

  return True;
}

static void
free_gear (gear *g)
{
  if (g->dlist)
    glDeleteLists (g->dlist, 1);
  free (g);
}


/* Make a new gear, place it next to its parent in the scene,
   with its teeth meshed and the proper velocity.  Returns the gear;
   or 0 if it didn't work.  (Call this a bunch of times until either
   it works, or you decide it's probably not going to.)
 */
static gear *
place_new_gear (ModeInfo *mi, gear *parent, Bool coaxial_p)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int loop_count = 0;
  gear *g = 0;

  while (1)
    {
      loop_count++;
      if (loop_count >= 100)
        {
          if (g)
            free_gear (g);
          g = 0;
          break;
        }

      g = new_gear (mi, parent, coaxial_p);
      if (!g) return 0;  /* out of memory? */

      if (place_gear (mi, g, parent, coaxial_p))
        break;
    }

  if (! g) return 0;

  /* We got a gear, and it is properly positioned.
     Insert it in the scene.
   */
  if (pp->ngears + 2 >= pp->gears_size)
    {
      pp->gears_size += 100;
      pp->gears = (gear **) realloc (pp->gears,
                                     pp->gears_size * sizeof (*pp->gears));
      if (! pp->gears)
        {
          fprintf (stderr, "%s: out of memory (%d gears)\n",
                   progname, pp->gears_size);
        }
    }

  pp->gears[pp->ngears++] = g;
  return g;
}


static void delete_gear (ModeInfo *mi, gear *g);

static void
push_gear (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  gear *g;
  gear *parent = (pp->ngears <= 0 ? 0 : pp->gears[pp->ngears-1]);

  Bool tried_coaxial_p = False;
  Bool coaxial_p = False;
  Bool last_ditch_coax_p = False;
  int loop_count = 0;

  debug_size_failures = 0;
  debug_position_failures = 0;

 AGAIN:
  loop_count++;
  if (loop_count > 100) abort();  /* we're doomed! */
  
  g = 0;

  /* If the gears are turning at LUDICROUS SPEED, unhook the train to
     reset things to a sane velocity.

     10,000 RPM at 30 FPS = 5.5 rotations per frame.
      1,000 RPM at 30 FPS = 0.5 rotations per frame.
        600 RPM at 30 FPS =  3 frames per rotation.
        200 RPM at 30 FPS =  9 frames per rotation.
        100 RPM at 30 FPS = 18 frames per rotation.
         50 RPM at 30 FPS = 36 frames per rotation.
         10 RPM at 30 FPS =  3 sec per rotation.
          1 RPM at 30 FPS = 30 sec per rotation.
         .5 RPM at 30 FPS =  1 min per rotation.
         .1 RPM at 30 FPS =  5 min per rotation.
   */
  if (parent && parent->rpm > max_rpm)
    {
      if (verbose_p)
        {
          char buf[100];
          rpm_string (parent->rpm, buf);
          fprintf (stderr, "%s: ludicrous speed!  %s\n\n", progname, buf);
        }
      parent = 0;
    }

  /* If the last N gears we've placed have all been motion-blurred, then
     it's a safe guess that we've wandered off into the woods and aren't
     coming back.  Bail on this train.
   */
  if (current_blur_length >= 10)
    {
      if (verbose_p)
        fprintf (stderr, "%s: it's a blurpocalypse!\n\n", progname);
      parent = 0;
    }



  /* Sometimes, try to make a coaxial gear.
   */
  if (parent && !parent->coax_p && (random() % 40) == 0)
    {
      tried_coaxial_p = True;
      coaxial_p = True;
      g = place_new_gear (mi, parent, coaxial_p);
    }

  /* Try to make a regular gear.
   */
  if (!g)
    {
      coaxial_p = False;
      g = place_new_gear (mi, parent, coaxial_p);
    }

  /* If we couldn't make a regular gear, then try to make a coxial gear
     (unless we already tried that.)
   */
  if (!g && !tried_coaxial_p && parent && !parent->coax_p)
    {
      tried_coaxial_p = True;
      coaxial_p = True;
      g = place_new_gear (mi, parent, coaxial_p);
      if (g)
        last_ditch_coax_p = True;
    }

  /* If we couldn't do that either, then the train has hit a dead end:
     start a new train.
   */
  if (!g)
    {
      coaxial_p = False;
      if (verbose_p)
        fprintf (stderr, "%s: dead end!\n\n", progname);
      parent = 0;
      g = place_new_gear (mi, parent, coaxial_p);
    }

  if (! g)
    {
      /* Unable to make/place any gears at all!
         This can happen if we've backed ourself into a corner very near
         the top-right or bottom-right corner of the growth zone.
         It's time to add a gear, but there's no room to add one!
         In that case, let's just wipe all the gears that are in the
         growth zone and try again.
       */
      int i;

      if (verbose_p && debug_placement_p)
        fprintf (stderr,
                 "%s: placement: resetting growth zone!  "
                 "failed: %d size, %d pos\n",
                 progname,
                 debug_size_failures, debug_position_failures);
      for (i = pp->ngears-1; i >= 0; i--)
        {
          gear *g = pp->gears[i];
          if (g->x - g->r - g->tooth_h < pp->render_left)
            delete_gear (mi, g);
        }
      goto AGAIN;
    }

  if (g->coax_p)
    {
      if (g->x != parent->x) abort();
      if (g->y != parent->y) abort();
      if (g->z == parent->z) abort();
      if (g->r == parent->r) abort();
      if (g->th != parent->th) abort();
      if (g->ratio != parent->ratio) abort();
      if (g->rpm != parent->rpm) abort();
    }

  if (verbose_p)
    {
      fprintf (stderr, "%s: %5lu ", progname, g->id);

      fputc ((g->motion_blur_p ? '*' : ' '), stderr);
      fputc (((g->coax_p && last_ditch_coax_p) ? '2' :
              g->coax_p ? '1' : ' '),
             stderr);

      fprintf (stderr, " %2d%%",
               (int) (g->r * 2 * 100 / pp->vp_height));
      fprintf (stderr, "  %2d teeth", g->nteeth);
      fprintf (stderr, " %3.0f rpm;", g->rpm);

      {
        char buf1[50], buf2[50], buf3[100];
        *buf1 = 0; *buf2 = 0; *buf3 = 0;
        if (debug_size_failures)
          sprintf (buf1, "%3d sz", debug_size_failures);
        if (debug_position_failures)
          sprintf (buf2, "%2d pos", debug_position_failures);
        if (*buf1 || *buf2)
          sprintf (buf3, " tries: %-7s%s", buf1, buf2);
        fprintf (stderr, "%-21s", buf3);
      }

      if (g->base_p) fprintf (stderr, " RESET %lu", current_length);
      fprintf (stderr, "\n");
    }

  if (g->base_p)
    current_length = 1;
  else
    current_length++;

  if (g->motion_blur_p)
    current_blur_length++;
  else
    current_blur_length = 0;
}



/* Remove the given gear from the scene and free it.
 */
static void
delete_gear (ModeInfo *mi, gear *g)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int i;

  for (i = 0; i < pp->ngears; i++)      /* find this gear in the list */
    if (pp->gears[i] == g) break;
  if (pp->gears[i] != g) abort();

  for (; i < pp->ngears-1; i++)         /* pull later entries forward */
    pp->gears[i] = pp->gears[i+1];
  pp->gears[i] = 0;
  pp->ngears--;
  free_gear (g);
}


/* Update the position of each gear in the scene.
 */
static void
scroll_gears (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int i;

  for (i = 0; i < pp->ngears; i++)
    pp->gears[i]->x -= (scroll_speed * 0.002);

  /* if the right edge of any gear is off screen to the left, delete it.
   */
  for (i = pp->ngears-1; i >= 0; i--)
    {
      gear *g = pp->gears[i];
      if (g->x + g->r + g->tooth_h < pp->render_left)
        delete_gear (mi, g);
    }

  /* if the right edge of the last-added gear is left of the right edge
     of the layout area, add another gear.
   */
  i = 0;
  while (1)
    {
      gear *g = (pp->ngears <= 0 ? 0 : pp->gears[pp->ngears-1]);
      if (!g || g->x + g->r + g->tooth_h < pp->layout_right)
        push_gear (mi);
      else
        break;
      i++;
    }

  /*
  if (i > 1 && verbose_p)
    fprintf (stderr, "%s: pushed %d gears\n", progname, i);
   */
}


/* Update the rotation of each gear in the scene.
 */
static void
spin_gears (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int i;

  for (i = 0; i < pp->ngears; i++)
    {
      gear *g = pp->gears[i];
      double off = (g->ratio * spin_speed);

      if (g->th > 0)
        g->th += off;
      else
        g->th -= off;
    }
}


/* Run the animation fast (without displaying anything) until the first
   gear is just about to come on screen.  This is to avoid a big delay
   with a blank screen when -scroll is low.
 */
static void
ffwd (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  while (1)
    {
      gear *g = farthest_gear (mi, True);
      if (g && g->x - g->r - g->tooth_h/2 <= pp->vp_right * 0.88)
        break;
      scroll_gears (mi);
    }
}



/* Rendering the 3D objects into the scene.
 */


/* Draws an uncapped tube of the given radius extending from top to bottom,
   with faces pointing either in or out.
 */
static int
draw_ring (ModeInfo *mi, int segments,
           GLfloat r, GLfloat top, GLfloat bottom, Bool in_p)
{
  int i;
  int polys = 0;
  Bool wire_p = MI_IS_WIREFRAME(mi);
  GLfloat width = M_PI * 2 / segments;

  if (top != bottom)
    {
      glFrontFace (in_p ? GL_CCW : GL_CW);
      glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
      for (i = 0; i < segments + (wire_p ? 0 : 1); i++)
        {
          GLfloat th = i * width;
          GLfloat cth = cos(th);
          GLfloat sth = sin(th);
          if (in_p)
            glNormal3f (-cth, -sth, 0);
          else
            glNormal3f (cth, sth, 0);
          glVertex3f (cth * r, sth * r, top);
          glVertex3f (cth * r, sth * r, bottom);
        }
      polys += segments;
      glEnd();
    }

  if (wire_p)
    {
      glBegin (GL_LINE_LOOP);
      for (i = 0; i < segments; i++)
        {
          GLfloat th = i * width;
          glVertex3f (cos(th) * r, sin(th) * r, top);
        }
      glEnd();
      glBegin (GL_LINE_LOOP);
      for (i = 0; i < segments; i++)
        {
          GLfloat th = i * width;
          glVertex3f (cos(th) * r, sin(th) * r, bottom);
        }
      glEnd();
    }

  return polys;
}


/* Draws a donut-shaped disc between the given radii,
   with faces pointing either up or down.
   The first radius may be 0, in which case, a filled disc is drawn.
 */
static int
draw_disc (ModeInfo *mi, int segments,
           GLfloat ra, GLfloat rb, GLfloat z, Bool up_p)
{
  int i;
  int polys = 0;
  Bool wire_p = MI_IS_WIREFRAME(mi);
  GLfloat width = M_PI * 2 / segments;

  if (ra <  0) abort();
  if (rb <= 0) abort();

  if (ra == 0)
    glFrontFace (up_p ? GL_CW : GL_CCW);
  else
    glFrontFace (up_p ? GL_CCW : GL_CW);

  if (ra == 0)
    glBegin (wire_p ? GL_LINES : GL_TRIANGLE_FAN);
  else
    glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);

  glNormal3f (0, 0, (up_p ? -1 : 1));

  if (ra == 0 && !wire_p)
    glVertex3f (0, 0, z);

  for (i = 0; i < segments + (wire_p ? 0 : 1); i++)
    {
      GLfloat th = i * width;
      GLfloat cth = cos(th);
      GLfloat sth = sin(th);
      if (wire_p || ra != 0)
        glVertex3f (cth * ra, sth * ra, z);
      glVertex3f (cth * rb, sth * rb, z);
      polys++;
    }
  glEnd();
  return polys;
}


/* Draws N thick radial lines between the given radii,
   with faces pointing either up or down.
 */
static int
draw_spokes (ModeInfo *mi, int n, GLfloat thickness, int segments,
             GLfloat ra, GLfloat rb, GLfloat z1, GLfloat z2)
{
  int i;
  int polys = 0;
  Bool wire_p = MI_IS_WIREFRAME(mi);
  GLfloat width;
  int segments2 = 0;
  int insegs, outsegs;
  int tick;
  int state;

  if (ra <= 0 || rb <= 0) abort();

  segments *= 3;

  while (segments2 < segments) /* need a multiple of N >= segments */
    segments2 += n;            /* (yes, this is a moronic way to find that) */

  insegs  = ((float) (segments2 / n) + 0.5) / thickness;
  outsegs = (segments2 / n) - insegs;
  if (insegs  <= 0) insegs = 1;
  if (outsegs <= 0) outsegs = 1;

  segments2 = (insegs + outsegs) * n;
  width = M_PI * 2 / segments2;

  tick = 0;
  state = 0;
  for (i = 0; i < segments2; i++, tick++)
    {
      GLfloat th1 = i * width;
      GLfloat th2 = th1 + width;
      GLfloat cth1 = cos(th1);
      GLfloat sth1 = sin(th1);
      GLfloat cth2 = cos(th2);
      GLfloat sth2 = sin(th2);
      GLfloat orb = rb;

      int changed = (i == 0);

      if (state == 0 && tick == insegs)
        tick = 0, state = 1, changed = 1;
      else if (state == 1 && tick == outsegs)
        tick = 0, state = 0, changed = 1;

      if ((state == 1 ||                /* in */
           (state == 0 && changed)) &&
          (!wire_p || wire_all_p))
        {
          /* top */
          glFrontFace (GL_CCW);
          glBegin (wire_p ? GL_LINES : GL_QUADS);
          glNormal3f (0, 0, -1);
          glVertex3f (cth1 * ra, sth1 * ra, z1);
          glVertex3f (cth1 * rb, sth1 * rb, z1);
          glVertex3f (cth2 * rb, sth2 * rb, z1);
          glVertex3f (cth2 * ra, sth2 * ra, z1);
          polys++;
          glEnd();

          /* bottom */
          glFrontFace (GL_CW);
          glBegin (wire_p ? GL_LINES : GL_QUADS);
          glNormal3f (0, 0, 1);
          glVertex3f (cth1 * ra, sth1 * ra, z2);
          glVertex3f (cth1 * rb, sth1 * rb, z2);
          glVertex3f (cth2 * rb, sth2 * rb, z2);
          glVertex3f (cth2 * ra, sth2 * ra, z2);
          polys++;
          glEnd();
        }

      if (state == 1 && changed)   /* entering "in" state */
        {
          /* left */
          glFrontFace (GL_CW);
          glBegin (wire_p ? GL_LINES : GL_QUADS);
          do_normal (cth1 * rb, sth1 * rb, z1,
                     cth1 * ra, sth1 * ra, z1,
                     cth1 * rb, sth1 * rb, z2);
          glVertex3f (cth1 * ra, sth1 * ra, z1);
          glVertex3f (cth1 * rb, sth1 * rb, z1);
          glVertex3f (cth1 * rb, sth1 * rb, z2);
          glVertex3f (cth1 * ra, sth1 * ra, z2);
          polys++;
          glEnd();
        }

      if (state == 0 && changed)   /* entering "out" state */
        {
          /* right */
          glFrontFace (GL_CCW);
          glBegin (wire_p ? GL_LINES : GL_QUADS);
          do_normal (cth2 * ra, sth2 * ra, z1,
                     cth2 * rb, sth2 * rb, z1,
                     cth2 * rb, sth2 * rb, z2);
          glVertex3f (cth2 * ra, sth2 * ra, z1);
          glVertex3f (cth2 * rb, sth2 * rb, z1);
          glVertex3f (cth2 * rb, sth2 * rb, z2);
          glVertex3f (cth2 * ra, sth2 * ra, z2);
          polys++;
          glEnd();
        }

      rb = orb;
    }
  glEnd();
  return polys;
}


/* Draws some bumps (embedded cylinders) on the gear.
 */
static int
draw_gear_nubs (ModeInfo *mi, gear *g)
{
  Bool wire_p = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int i;
  int steps = 20;
  double r, size, height;
  GLfloat *cc;
  int which;
  GLfloat width, off;

  if (! g->nubs) return 0;

  which = biggest_ring (g, &r, &size, &height);
  size /= 5;
  height *= 0.7;

  cc = (which == 1 ? g->color : g->color2);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, cc);

  width = M_PI * 2 / g->nubs;
  off = M_PI / (g->nteeth * 2);  /* align first nub with a tooth */

  for (i = 0; i < g->nubs; i++)
    {
      GLfloat th = (i * width) + off;
      glPushMatrix();
      glTranslatef (cos(th) * r, sin(th) * r, 0);

      if (wire_p && !wire_all_p)
        polys += draw_ring (mi, steps/2, size, 0, 0, False);
      else
        {
          polys += draw_disc (mi, steps, 0, size, -height, True);
          polys += draw_disc (mi, steps, 0, size,  height, False);
          polys += draw_ring (mi, steps, size, -height, height, False);
        }
      glPopMatrix();
    }
  return polys;
}



/* Draws a much simpler representation of a gear.
 */
static int
draw_gear_schematic (ModeInfo *mi, gear *g)
{
  Bool wire_p = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int i;
  GLfloat width = M_PI * 2 / g->nteeth;

  if (!wire_p) glDisable(GL_LIGHTING);
  glColor3f (g->color[0] * 0.6, g->color[1] * 0.6, g->color[2] * 0.6);

  glBegin (GL_LINES);
  for (i = 0; i < g->nteeth; i++)
    {
      GLfloat th = (i * width) + (width/4);
      glVertex3f (0, 0, -g->thickness/2);
      glVertex3f (cos(th) * g->r, sin(th) * g->r, -g->thickness/2);
    }
  polys += g->nteeth;
  glEnd();

  glBegin (GL_LINE_LOOP);
  for (i = 0; i < g->nteeth; i++)
    {
      GLfloat th = (i * width) + (width/4);
      glVertex3f (cos(th) * g->r, sin(th) * g->r, -g->thickness/2);
    }
  polys += g->nteeth;
  glEnd();

  if (!wire_p) glEnable(GL_LIGHTING);
  return polys;
}


/* Renders all the interior (non-toothy) parts of a gear:
   the discs, axles, etc.
 */
static int
draw_gear_interior (ModeInfo *mi, gear *g)
{
  Bool wire_p = MI_IS_WIREFRAME(mi);
  int polys = 0;

  int steps = g->nteeth * 2;
  if (steps < 10) steps = 10;
  if (wire_p && !wire_all_p) steps /= 2;

  /* ring 1 (facing in) is done in draw_gear_teeth */

  /* ring 2 (facing in) and disc 2
   */
  if (g->inner_r2)
    {
      GLfloat ra = g->inner_r;
      GLfloat rb = g->inner_r2;
      GLfloat za = -g->thickness2/2;
      GLfloat zb =  g->thickness2/2;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color2);

      if ((g->coax_p != 1 && !g->inner_r3) ||
          (wire_p && wire_all_p))
        polys += draw_ring (mi, steps, rb, za, zb, True);  /* ring facing in */

      if (wire_p && wire_all_p)
        polys += draw_ring (mi, steps, ra, za, zb, True);  /* ring facing in */

      if (g->spokes)
        polys += draw_spokes (mi, g->spokes, g->spoke_thickness,
                              steps, ra, rb, za, zb);
      else if (!wire_p || wire_all_p)
        {
          polys += draw_disc (mi, steps, ra, rb, za, True);  /* top plate */
          polys += draw_disc (mi, steps, ra, rb, zb, False); /* bottom plate */
        }
    }

  /* ring 3 (facing in and out) and disc 3
   */
  if (g->inner_r3)
    {
      GLfloat ra = g->inner_r2;
      GLfloat rb = g->inner_r3;
      GLfloat za = -g->thickness3/2;
      GLfloat zb =  g->thickness3/2;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);

      polys += draw_ring (mi, steps, ra, za, zb, False);  /* ring facing out */

      if (g->coax_p != 1 || (wire_p && wire_all_p))
        polys += draw_ring (mi, steps, rb, za, zb, True);  /* ring facing in */

      if (!wire_p || wire_all_p)
        {
          polys += draw_disc (mi, steps, ra, rb, za, True);  /* top plate */
          polys += draw_disc (mi, steps, ra, rb, zb, False); /* bottom plate */
        }
    }

  /* axle tube
   */
  if (g->coax_p == 1)
    {
      GLfloat cap_height = g->coax_thickness/3;

      GLfloat ra = (g->inner_r3 ? g->inner_r3 :
                    g->inner_r2 ? g->inner_r2 :
                    g->inner_r);
      GLfloat za = -(g->thickness/2 + cap_height);
      GLfloat zb = g->coax_thickness/2 + plane_displacement + cap_height;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);

      if (wire_p && !wire_all_p) steps /= 2;

      polys += draw_ring (mi, steps, ra, za, zb, False);  /* ring facing out */

      if (!wire_p || wire_all_p)
        {
          polys += draw_disc (mi, steps, 0,  ra, za, True);  /* top plate */
          polys += draw_disc (mi, steps, 0,  ra, zb, False); /* bottom plate */
        }
    }
  return polys;
}


/* Computes the vertices and normals of the teeth of a gear.
   This is the heavy lifting: there are a ton of polygons around the
   perimiter of a gear, and the normals are difficult (not radial
   or right angles.)

   It would be nice if we could cache this, but the numbers are
   different for essentially every gear:

      - Every gear has a different inner_r, so the vertices of the
        inner ring (and thus, the triangle fans on the top and bottom
        faces) are different in a non-scalable way.

      - If the ratio between tooth_w and tooth_h changes, the normals
        on the outside edges of the teeth are invalid (this can happen
        every time we start a new train.)

   So instead, we rely on OpenGL display lists to do the cacheing for
   us -- we only compute all these normals once per gear, instead of
   once per gear per frame.
 */
static void
gear_teeth_geometry (ModeInfo *mi, gear *g,
                     int *points_per_tooth_ret,
                     XYZ **points_ret,
                     XYZ **normals_ret)
{
  int i;

  int ppt = 15; /* points per tooth */

  GLfloat width = M_PI * 2 / g->nteeth;

  GLfloat rh = g->tooth_h;
  GLfloat tw = width;
  GLfloat fudge = (g->nteeth >= 5 ? 0 : 0.04);   /* reshape small ones a bit */

  XYZ *points   = (XYZ *) calloc (ppt * g->nteeth + 1, sizeof(*points));
  XYZ *fnormals = (XYZ *) calloc (ppt * g->nteeth + 1, sizeof(*points));
  XYZ *pnormals = (XYZ *) calloc (ppt * g->nteeth + 1, sizeof(*points));
  int npoints = 0;

  /* Approximate shape of an "involute" gear tooth.

                                 (TH)
                 th0 th1 th2 th3 th4 th5 th6 th7 th8   th9    th10
                   :  :  :   :    :    :   :  :  :      :      :
                   :  :  :   :    :    :   :  :  :      :      :
        r0 ........:..:..:...___________...:..:..:......:......:..
                   :  :  :  /:    :    :\  :  :  :      :      :
                   :  :  : / :    :    : \ :  :  :      :      :
                   :  :  :/  :    :    :  \:  :  :      :      :
        r1 ........:.....@...:....:....:...@..:..:......:......:..
                   :  : @:   :    :    :   :@ :  :      :      :
    (R) ...........:...@.:...:....:....:...:.@..........:......:......
                   :  :@ :   :    :    :   : @:  :      :      :
        r2 ........:..@..:...:....:....:...:..@:........:......:..
                   : /:  :   :    :    :   :  :\ :      :      :
                   :/ :  :   :    :    :   :  : \:      :      : /
        r3 ......__/..:..:...:....:....:...:..:..\______________/
                   :  :  :   :    :    :   :  :  :      :      :
                   |  :  :   :    :    :   :  :  |      :      :
                   :  :  :   :    :    :   :  :  :      :      :
                   |  :  :   :    :    :   :  :  |      :      :
        r4 ......__:_____________________________:________________
   */

  GLfloat R = g->r;

  GLfloat r[20];
  GLfloat th[20];

  r[0] = R + (rh * 0.5);
  r[1] = R + (rh * 0.25);
  r[2] = R - (rh * 0.25);
  r[3] = R - (rh * 0.5);
  r[4] = g->inner_r;

  th[0] = -tw * 0.45;
  th[1] = -tw * 0.30;
  th[2] = -tw *(0.16 - fudge);
  th[3] = -tw * 0.04;
  th[4] =  0;
  th[5] =  tw * 0.04;
  th[6] =  tw *(0.16 - fudge);
  th[7] =  tw * 0.30;
  th[8] =  tw * 0.45;
  th[9] =  width / 2;
  th[10]=  th[0] + width;

  if (!points || !fnormals || !pnormals)
    {
      fprintf (stderr, "%s: out of memory\n", progname);
      exit (1);
    }

  npoints = 0;

  /* First, compute the coordinates of every point used for every tooth.
   */
  for (i = 0; i < g->nteeth; i++)
    {
      GLfloat TH = (i * width) + (width/4);

#     undef PUSH
#     define PUSH(PR,PTH) \
        points[npoints].x = cos(TH+th[(PTH)]) * r[(PR)]; \
        points[npoints].y = sin(TH+th[(PTH)]) * r[(PR)]; \
        npoints++

      /* start1 = npoints; */

      PUSH(3, 0);       /* tooth left 1 */
      PUSH(2, 1);       /* tooth left 2 */
      PUSH(1, 2);       /* tooth left 3 */
      PUSH(0, 3);       /* tooth top 1 */
      PUSH(0, 5);       /* tooth top 2 */
      PUSH(1, 6);       /* tooth right 1 */
      PUSH(2, 7);       /* tooth right 2 */
      PUSH(3, 8);       /* tooth right 3 */
      PUSH(3, 10);      /* gap top */

      /* end1   = npoints; */

      PUSH(4, 8);       /* gap interior */

      /* start2 = npoints; */

      PUSH(4, 10);      /* tooth interior 1 */
      PUSH(4, 8);       /* tooth interior 2 */
      PUSH(4, 4);       /* tooth bottom 1 */
      PUSH(4, 0);       /* tooth bottom 2 */

      /* end2 = npoints; */

      PUSH(3, 4);       /* midpoint */

      /* mid = npoints-1; */

      if (i == 0 && npoints != ppt) abort();  /* go update "ppt"! */
#     undef PUSH
    }


  /* Now compute the face normals for each facet on the tooth rim.
   */
  for (i = 0; i < npoints; i++)
    {
      XYZ p1, p2, p3;
      p1 = points[i];
      p2 = points[i+1];
      p3 = p1;
      p3.z++;
      fnormals[i] = calc_normal (p1, p2, p3);
    }


  /* From the face normals, compute the vertex normals (by averaging
     the normals of adjascent faces.)
   */
  for (i = 0; i < npoints; i++)
    {
      int a = (i == 0 ? npoints-1 : i-1);
      int b = i;

      /* Kludge to fix the normal on the last top point: since the
         faces go all the way around, this normal pointed clockwise
         instead of radially out. */
      int start1 = (i / ppt) * ppt;
      int end1   = start1 + 9;
      XYZ n1, n2;

      if (b == end1-1)
        b = (start1 + ppt == npoints ? 0 : start1 + ppt);

      n1 = fnormals[a];   /* normal of [i-1 - i] face */
      n2 = fnormals[b];   /* normal of [i - i+1] face */
      pnormals[i].x = (n1.x + n2.x) / 2;
      pnormals[i].y = (n1.y + n2.y) / 2;
      pnormals[i].z = (n1.z + n2.z) / 2;
    }

  free (fnormals);

  if (points_ret)
    *points_ret = points;
  else
    free (points);

  if (normals_ret)
    *normals_ret = pnormals;
  else
    free (pnormals);

  if (points_per_tooth_ret)
    *points_per_tooth_ret = ppt;
}


/* Renders all teeth of a gear.
 */
static int
draw_gear_teeth (ModeInfo *mi, gear *g)
{
  Bool wire_p = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int i;

  GLfloat z1 = -g->thickness/2;
  GLfloat z2 =  g->thickness/2;

  int ppt;
  XYZ *points, *pnormals;

  gear_teeth_geometry (mi, g, &ppt, &points, &pnormals);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);

  for (i = 0; i < g->nteeth; i++)
    {
      int j;
      GLfloat z;

      int start1 = i * ppt;
      int end1   = start1 + 9;
      int start2 = end1   + 1;
      int end2   = start2 + 4;
      int mid    = end2;

      /* Outside rim of the tooth
       */
      glFrontFace (GL_CW);
      glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
      for (j = start1; j < end1; j++)
        {
          glNormal3f (pnormals[j].x, pnormals[j].y, pnormals[j].z);
          glVertex3f (points[j].x, points[j].y, z1);
          glVertex3f (points[j].x, points[j].y, z2);
          polys++;

# if 0
          /* Show the face normal vectors */
          if (wire_p)
            {
              XYZ n = fnormals[j];
              GLfloat x = (points[j].x + points[j+1].x) / 2;
              GLfloat y = (points[j].y + points[j+1].y) / 2;
              GLfloat z  = (z1 + z2) / 2;
              glVertex3f (x, y, z);
              glVertex3f (x + n.x, y + n.y, z);
            }

          /* Show the vertex normal vectors */
          if (wire_p)
            {
              XYZ n = pnormals[j];
              GLfloat x = points[j].x;
              GLfloat y = points[j].y;
              GLfloat z  = (z1 + z2) / 2;
              glVertex3f (x, y, z);
              glVertex3f (x + n.x, y + n.y, z);
            }
# endif /* 0 */
        }
      glEnd();

      /* Some more lines for the outside rim of the tooth...
       */
      if (wire_p)
        {
          glBegin (GL_LINE_STRIP);
          for (j = start1; j < end1; j++)
            glVertex3f (points[j].x, points[j].y, z1);
          glEnd();
          glBegin (GL_LINE_STRIP);
          for (j = start1; j < end1; j++)
            glVertex3f (points[j].x, points[j].y, z2);
          glEnd();
        }

      /* Inside rim behind the tooth
       */
      glFrontFace (GL_CW);
      glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
      for (j = start2; j < end2; j++)
        {
          glNormal3f (-points[j].x, -points[j].y, 0);
          glVertex3f ( points[j].x,  points[j].y, z1);
          glVertex3f ( points[j].x,  points[j].y, z2);
          polys++;
        }
      glEnd();

      /* Some more lines for the inside rim...
       */
      if (wire_p)
        {
          glBegin (GL_LINE_STRIP);
          for (j = start2; j < end2; j++)
            glVertex3f (points[j].x, points[j].y, z1);
          glEnd();
          glBegin (GL_LINE_STRIP);
          for (j = start2; j < end2; j++)
            glVertex3f (points[j].x, points[j].y, z2);
          glEnd();
        }

      /* All top and bottom facets.  We can skip all of these in wire mode.
       */
      if (!wire_p || wire_all_p)
        for (z = z1; z <= z2; z += z2-z1)
          {
            /* Flat edge of the tooth
             */
            glFrontFace (z == z1 ? GL_CW : GL_CCW);
            glBegin (wire_p ? GL_LINES : GL_TRIANGLE_FAN);
            glNormal3f (0, 0, z);
            for (j = start1; j < end2; j++)
              {
                if (j == end1-1 || j == end1 || j == start2)
                  continue;  /* kludge... skip these... */

                if (wire_p || j == start1)
                  glVertex3f (points[mid].x, points[mid].y, z);
                glVertex3f (points[j].x, points[j].y, z);
                polys++;
              }
            glVertex3f (points[start1].x, points[start1].y, z);
            glEnd();

            /* Flat edge between teeth
             */
            glFrontFace (z == z1 ? GL_CW : GL_CCW);
            glBegin (wire_p ? GL_LINES : GL_QUADS);
            glNormal3f (0, 0, z);
            glVertex3f (points[end1-1  ].x, points[end1-1  ].y, z);
            glVertex3f (points[start2  ].x, points[start2  ].y, z);
            glVertex3f (points[start2+1].x, points[start2+1].y, z);
            glVertex3f (points[end1-2  ].x, points[end1-2  ].y, z);
            polys++;
            glEnd();
          }
    }

  free (points);
  free (pnormals);
  return polys;
}


/* Render one gear, unrotated at 0,0.
 */
static int
draw_gear_1 (ModeInfo *mi, gear *g)
{
  Bool wire_p = MI_IS_WIREFRAME(mi);
  int polys = 0;

  static GLfloat spec[4] = {1.0, 1.0, 1.0, 1.0};
  static GLfloat shiny   = 128.0;

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);
  glColor3f (g->color[0], g->color[1], g->color[2]);

  if (debug_p && wire_p)
    polys += draw_gear_schematic (mi, g);
  else
    {
      glPushMatrix();
      glRotatef (g->wobble, 1, 0, 0);
      polys += draw_gear_teeth (mi, g);
      polys += draw_gear_interior (mi, g);
      polys += draw_gear_nubs (mi, g);
      glPopMatrix();
    }
  return polys;
}


/* Render one gear in the proper position, creating the gear's
   display list first if necessary.
 */
static void
draw_gear (ModeInfo *mi, int which)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  gear *g = pp->gears[which];
  GLfloat th;

  Bool visible_p = (g->x + g->r + g->tooth_h >= pp->render_left &&
                    g->x - g->r - g->tooth_h <= pp->render_right);

  if (!visible_p && !debug_p)
    return;

  if (! g->dlist)
    {
      g->dlist = glGenLists (1);
      if (! g->dlist)
        {
          /* I don't know how many display lists a GL implementation
             is supposed to provide, but hopefully it's more than
             "a few hundred", or we'll be in trouble...
           */
          check_gl_error ("glGenLists");
          abort();
        }

      glNewList (g->dlist, GL_COMPILE);
      g->polygons = draw_gear_1 (mi, g);
      glEndList ();
    }

  glPushMatrix();

  glTranslatef (g->x, g->y, g->z);

  if (g->motion_blur_p && !pp->button_down_p)
    {
      /* If we're in motion-blur mode, then draw the gear so that each
         frame rotates it by exactly one half tooth-width, so that it
         looks flickery around the edges.  But, revert to the normal
         way when the mouse button is down lest the user see overlapping
         polygons.
       */
      th = g->motion_blur_p * 180.0 / g->nteeth * (g->th > 0 ? 1 : -1);
      g->motion_blur_p++;
    }
  else
    th = g->th;

  glRotatef (th, 0, 0, 1);

  glPushName (g->id);

  if (! visible_p)
    mi->polygon_count += draw_gear_schematic (mi, g);
  else
    {
      glCallList (g->dlist);
      mi->polygon_count += g->polygons;
    }

  glPopName();
  glPopMatrix();
}


/* Render all gears.
 */
static void
draw_gears (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  Bool wire_p = MI_IS_WIREFRAME(mi);
  int i;

  glColor4f (1, 1, 0.8, 1);

  glInitNames();

  for (i = 0; i < pp->ngears; i++)
    draw_gear (mi, i);

  /* draw a line connecting gears that are, uh, geared. */
  if (debug_p)
    {
      static GLfloat color[4] = {1.0, 0.0, 0.0, 1.0};
      GLfloat off = 0.1;
      GLfloat ox=0, oy=0, oz=0;

      if (!wire_p) glDisable(GL_LIGHTING);
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
      glColor3f (color[0], color[1], color[2]);

      for (i = 0; i < pp->ngears; i++)
        {
          gear *g = pp->gears[i];
          glBegin(GL_LINE_STRIP);
          glVertex3f (g->x, g->y, g->z - off);
          glVertex3f (g->x, g->y, g->z + off);
          if (i > 0 && !g->base_p)
            glVertex3f (ox, oy, oz + off);
          glEnd();
          ox = g->x;
          oy = g->y;
          oz = g->z;
        }
      if (!wire_p) glEnable(GL_LIGHTING);
    }
}


/* Mouse hit detection
 */
void
find_mouse_gear (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int screen_width = MI_WIDTH (mi);
  int screen_height = MI_HEIGHT (mi);
  GLfloat h = (GLfloat) screen_height / (GLfloat) screen_width;
  int x, y;
  int hits;

  pp->mouse_gear_id = 0;

  /* Poll mouse position */
  {
    Window r, c;
    int rx, ry;
    unsigned int m;
    XQueryPointer (MI_DISPLAY (mi), MI_WINDOW (mi),
                   &r, &c, &rx, &ry, &x, &y, &m);
  }

  if (x < 0 || y < 0 || x > screen_width || y > screen_height)
    return;  /* out of window */

  /* Run OpenGL hit detector */
  {
    GLint vp[4];
    GLuint selbuf[512];

    glSelectBuffer (sizeof(selbuf), selbuf);  /* set up "select" mode */
    glRenderMode (GL_SELECT);
    glMatrixMode (GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glGetIntegerv (GL_VIEWPORT, vp);         /* save old vp */
    gluPickMatrix (x, vp[3]-y, 5, 5, vp);
    gluPerspective (30.0, 1/h, 1.0, 100.0);  /* must match reshape_pinion() */
    glMatrixMode (GL_MODELVIEW);

    draw_gears (mi);                         /* render into "select" buffer */

    glMatrixMode (GL_PROJECTION);            /* restore old vp */
    glPopMatrix ();
    glMatrixMode (GL_MODELVIEW);
    glFlush();
    hits = glRenderMode (GL_RENDER);         /* done selecting */

    if (hits > 0)
      {
        int i;
        GLuint name_count = 0;
        GLuint *p = (GLuint *) selbuf;
        GLuint *pnames = 0;
        GLuint min_z = ~0;

        for (i = 0; i < hits; i++)
          {     
            int names = *p++;
            if (*p < min_z)                  /* find match closest to screen */
              {
                name_count = names;
                min_z = *p;
                pnames = p+2;
              }
            p += names+2;
          }

        if (name_count > 0)                  /* take first hit */
          pp->mouse_gear_id = pnames[0];
      }
  }
}


/* Window management, etc
 */
void
reshape_pinion (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];

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

  {
    GLfloat render_width, layout_width;
    pp->vp_height = 1.0;
    pp->vp_width  = 1/h;

    pp->vp_left   = -pp->vp_width/2;
    pp->vp_right  =  pp->vp_width/2;
    pp->vp_top    =  pp->vp_height/2;
    pp->vp_bottom = -pp->vp_height/2;

    render_width = pp->vp_width * 2;
    layout_width = pp->vp_width * 0.8 * gear_size;

    pp->render_left  = -render_width/2;
    pp->render_right =  render_width/2;

    pp->layout_left  = pp->render_right;
    pp->layout_right = pp->layout_left + layout_width;
  }
}


Bool
pinion_handle_event (ModeInfo *mi, XEvent *event)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      pp->button_down_p = True;
      gltrackball_start (pp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      pp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           pp->button_down_p)
    {
      gltrackball_track (pp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' && debug_one_gear_p && pp->ngears)
        {
          delete_gear (mi, pp->gears[0]);
          return True;
        }
    }

  return False;
}


void 
init_pinion (ModeInfo *mi)
{
  pinion_configuration *pp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!pps) {
    pps = (pinion_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (pinion_configuration));
    if (!pps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    pp = &pps[MI_SCREEN(mi)];
  }

  pp = &pps[MI_SCREEN(mi)];

  pp->glx_context = init_GL(mi);

  load_fonts (mi);
  reshape_pinion (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  pp->title_list  = glGenLists (1);

  pp->ngears = 0;
  pp->gears_size = 0;
  pp->gears = 0;

  plane_displacement *= gear_size;

  if (!wire)
    {
      GLfloat pos[4] = {-3.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = { 0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = { 1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = { 1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  pp->trackball = gltrackball_init ();

  ffwd (mi);
}


void
draw_pinion (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  Bool wire_p = MI_IS_WIREFRAME(mi);
  static int tick = 0;

  if (!pp->glx_context)
    return;

  if (!pp->button_down_p)
    {
      if (!debug_one_gear_p || pp->ngears == 0)
        scroll_gears (mi);
      spin_gears (mi);
    }

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  {
    gltrackball_rotate (pp->trackball);
    mi->polygon_count = 0;

    glScalef (16, 16, 16);   /* map vp_width/height to the screen */

    if (debug_one_gear_p)    /* zoom in */
      glScalef (3, 3, 3);
    else if (debug_p)        /* show the "visible" and "layout" areas */
      {
        GLfloat ow = pp->layout_right - pp->render_left;
        GLfloat rw = pp->render_right - pp->render_left;
        GLfloat s = (pp->vp_width / ow) * 0.85;
        glScalef (s, s, s);
        glTranslatef (-(ow - rw) / 2, 0, 0);
      }
    else
      {
        GLfloat s = 1.2;
        glScalef (s, s, s);           /* zoom in a little more */
        glRotatef (-35, 1, 0, 0);     /* tilt back */
        glRotatef (  8, 0, 1, 0);     /* tilt left */
        glTranslatef (0.02, 0.1, 0);  /* pan up */
      }

    draw_gears (mi);

    if (debug_p)
      {
        if (!wire_p) glDisable(GL_LIGHTING);
        glColor3f (0.6, 0, 0);
        glBegin(GL_LINE_LOOP);
        glVertex3f (pp->render_left,  pp->vp_top,    0);
        glVertex3f (pp->render_right, pp->vp_top,    0);
        glVertex3f (pp->render_right, pp->vp_bottom, 0);
        glVertex3f (pp->render_left,  pp->vp_bottom, 0);
        glEnd();
        glColor3f (0.4, 0, 0);
        glBegin(GL_LINES);
        glVertex3f (pp->vp_left,      pp->vp_top,    0);
        glVertex3f (pp->vp_left,      pp->vp_bottom, 0);
        glVertex3f (pp->vp_right,     pp->vp_top,    0);
        glVertex3f (pp->vp_right,     pp->vp_bottom, 0);
        glEnd();
        glColor3f (0, 0.4, 0);
        glBegin(GL_LINE_LOOP);
        glVertex3f (pp->layout_left,  pp->vp_top,    0);
        glVertex3f (pp->layout_right, pp->vp_top,    0);
        glVertex3f (pp->layout_right, pp->vp_bottom, 0);
        glVertex3f (pp->layout_left,  pp->vp_bottom, 0);
        glEnd();
        if (!wire_p) glEnable(GL_LIGHTING);
      }

    if (tick++ > 10)   /* only do this every N frames */
      {
        tick = 0;
        find_mouse_gear (mi);
        new_label (mi);
      }
  }
  glPopMatrix ();

  glCallList (pp->title_list);

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
