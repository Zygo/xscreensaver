/* pinion, Copyright (c) 2004-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS        "*delay:        15000              \n" \
                        "*showFPS:      False              \n" \
                        "*wireframe:    False              \n" \
           "*titleFont:  -*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*\n" \
           "*titleFont2: -*-helvetica-medium-r-normal-*-*-120-*-*-*-*-*-*\n" \
           "*titleFont3: -*-helvetica-medium-r-normal-*-*-80-*-*-*-*-*-*\n"  \

# define release_pinion 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#include "xlockmore.h"
#include "normals.h"
#include "gltrackball.h"
#include "texfont.h"
#include "involute.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN_SPEED   "1.0"
#define DEF_SCROLL_SPEED "1.0"
#define DEF_GEAR_SIZE    "1.0"
#define DEF_MAX_RPM      "900"

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

  texture_font_data *font1, *font2, *font3;

  int draw_tick;

  GLfloat plane_displacement;        /* distance between coaxial gears */

  int debug_size_failures;           /* for debugging messages */
  int debug_position_failures;
  unsigned long current_length;      /* gear count in current train */
  unsigned long current_blur_length; /* how long have we been blurring? */

} pinion_configuration;


static pinion_configuration *pps = NULL;

/* command line arguments */
static GLfloat spin_speed, scroll_speed, gear_size, max_rpm;

static Bool verbose_p = False;            /* print progress on stderr */
static Bool debug_p = False;              /* render as flat schematic */

/* internal debugging variables */
static Bool debug_placement_p = False;    /* extreme verbosity on stderr */
static Bool debug_one_gear_p = False;     /* draw one big stationary gear */


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

ENTRYPOINT ModeSpecOpt pinion_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Font stuff
 */

static void
load_fonts (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  pp->font1 = load_texture_font (mi->dpy, "titleFont");
  pp->font2 = load_texture_font (mi->dpy, "titleFont2");
  pp->font3 = load_texture_font (mi->dpy, "titleFont3");
}



static void rpm_string (double rpm, char *s);

static void
draw_label (ModeInfo *mi)
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
      sprintf (label, "%d teeth\n", (int) g->nteeth);
      rpm_string (g->rpm, label + strlen(label));
      if (debug_p)
        sprintf (label + strlen (label), "\nPolys:  %d\nModel:  %s  (%.2f)\n",
                 g->polygons,
                 (g->size == INVOLUTE_SMALL ? "small" :
                  g->size == INVOLUTE_MEDIUM ? "medium"
                  : "large"),
                 g->tooth_h * MI_HEIGHT(mi));
    }

  if (*label)
    {
      texture_font_data *fd;
      if (MI_WIDTH(mi) >= 500 && MI_HEIGHT(mi) >= 375)
        fd = pp->font1;
      else if (MI_WIDTH(mi) >= 350 && MI_HEIGHT(mi) >= 260)
        fd = pp->font2;
      else
        fd = pp->font3;

      glColor3f (0.8, 0.8, 0);
      print_texture_label (mi->dpy, fd,
                           mi->xgwa.width, mi->xgwa.height,
                           1, label);
    }
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



/* Layout and stuff.
 */


/* Create and return a new gear sized for placement next to or on top of
   the given parent gear (if any.)  Returns 0 if out of memory.
 */
static gear *
new_gear (ModeInfo *mi, gear *parent, Bool coaxial_p)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  gear *g = (gear *) calloc (1, sizeof (*g));
  int loop_count = 0;
  static unsigned long id = 0;  /* only used in debugging output */

  if (!g) return 0;
  if (coaxial_p && !parent) abort();
  g->id = ++id;

  g->coax_displacement = pp->plane_displacement;

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

  /* g->tooth_slope = (parent ? -parent->tooth_slope : 4); */

  if (debug_one_gear_p)
    g->tooth_slope = frand(20)-10;


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
      involute_biggest_ring (g, 0, &size, 0);
      if (size > g->r * 0.2 && (random() % 5) == 0)
        {
          g->nubs = 1 + (random() % 16);
          if (g->nubs > 8) g->nubs = 1;
        }
    }

  if (g->inner_r3 > g->inner_r2) abort();
  if (g->inner_r2 > g->inner_r) abort();
  if (g->inner_r  > g->r) abort();

  /* Decide how complex the polygon model should be.
   */
  {
    double pix = g->tooth_h * MI_HEIGHT(mi); /* approx. tooth size in pixels */
    if (pix <= 2.5)      g->size = INVOLUTE_SMALL;
    else if (pix <= 3.5) g->size = INVOLUTE_MEDIUM;
    else if (pix <= 25)  g->size = INVOLUTE_LARGE;
    else                 g->size = INVOLUTE_HUGE;
  }

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
      pp->debug_size_failures++;
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
      double off = pp->plane_displacement;

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
          pp->debug_position_failures++;
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
          pp->debug_position_failures++;
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
      pp->debug_position_failures++;
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
            pp->debug_position_failures++;
            return False;
          }
      }
  }

  compute_rpm (mi, g);


  /* Make deeper gears be darker.
   */
  {
    double depth = g->z / pp->plane_displacement;
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

  pp->debug_size_failures = 0;
  pp->debug_position_failures = 0;

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
  if (pp->current_blur_length >= 10)
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
                 pp->debug_size_failures, pp->debug_position_failures);
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
      if (!parent) abort();
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
      fprintf (stderr, "  %2d teeth", (int) g->nteeth);
      fprintf (stderr, " %3.0f rpm;", g->rpm);

      {
        char buf1[50], buf2[50], buf3[200];
        *buf1 = 0; *buf2 = 0; *buf3 = 0;
        if (pp->debug_size_failures)
          sprintf (buf1, "%3d sz", pp->debug_size_failures);
        if (pp->debug_position_failures)
          sprintf (buf2, "%2d pos", pp->debug_position_failures);
        if (*buf1 || *buf2)
          sprintf (buf3, " tries: %-7s%s", buf1, buf2);
        fprintf (stderr, "%-21s", buf3);
      }

      if (g->base_p) fprintf (stderr, " RESET %lu", pp->current_length);
      fprintf (stderr, "\n");
    }

  if (g->base_p)
    pp->current_length = 1;
  else
    pp->current_length++;

  if (g->motion_blur_p)
    pp->current_blur_length++;
  else
    pp->current_blur_length = 0;
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
      if (debug_one_gear_p) break;
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
  if (debug_one_gear_p) return;
  while (1)
    {
      gear *g = farthest_gear (mi, True);
      if (g && g->x - g->r - g->tooth_h/2 <= pp->vp_right * 0.88)
        break;
      scroll_gears (mi);
    }
}



/* Render one gear in the proper position, creating the gear's
   display list first if necessary.
 */
static void
draw_gear (ModeInfo *mi, int which)
{
  Bool wire_p = MI_IS_WIREFRAME(mi);
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
      g->polygons = draw_involute_gear (g, (wire_p && debug_p ? 2 : wire_p));
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
    mi->polygon_count += draw_involute_schematic (g, wire_p);
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
      static const GLfloat color[4] = {1.0, 0.0, 0.0, 1.0};
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
static void
find_mouse_gear (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];

# ifndef HAVE_JWZGLES

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

#else  /* HAVE_JWZGLES */
  /* #### not yet implemented */
  pp->mouse_gear_id = (pp->ngears > 1 ? pp->gears[1]->id : 0);
  return;
#endif /* HAVE_JWZGLES */


}


/* Window management, etc
 */
ENTRYPOINT void
reshape_pinion (ModeInfo *mi, int width, int height)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
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


ENTRYPOINT Bool
pinion_handle_event (ModeInfo *mi, XEvent *event)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, pp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &pp->button_down_p))
    return True;
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


ENTRYPOINT void 
init_pinion (ModeInfo *mi)
{
  pinion_configuration *pp;

  MI_INIT (mi, pps);

  pp = &pps[MI_SCREEN(mi)];

  pp->glx_context = init_GL(mi);

  load_fonts (mi);
  reshape_pinion (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

  pp->ngears = 0;
  pp->gears_size = 0;
  pp->gears = 0;

  pp->plane_displacement = gear_size * 0.1;

  pp->trackball = gltrackball_init (False);

  ffwd (mi);
}


ENTRYPOINT void
draw_pinion (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  Bool wire_p = MI_IS_WIREFRAME(mi);

  if (!pp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);

  glPushMatrix();
  glRotatef(current_device_rotation(), 0, 0, 1);

  if (!wire_p)
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

    if (pp->draw_tick++ > 10)   /* only do this every N frames */
      {
        pp->draw_tick = 0;
        find_mouse_gear (mi);
      }
  }
  glPopMatrix ();

  draw_label (mi);
  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_pinion (ModeInfo *mi)
{
  pinion_configuration *pp = &pps[MI_SCREEN(mi)];
  int i;

  if (!pp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);

  for (i = 0; i < pp->ngears; i++)
    if (pp->gears[i]) free_gear (pp->gears[i]);
  if (pp->gears) free (pp->gears);
  if (pp->trackball) gltrackball_free (pp->trackball);
  if (pp->font1) free_texture_font (pp->font1);
  if (pp->font2) free_texture_font (pp->font2);
  if (pp->font3) free_texture_font (pp->font3);
}

XSCREENSAVER_MODULE ("Pinion", pinion)

#endif /* USE_GL */
