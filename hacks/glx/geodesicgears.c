/* geodesicgears, Copyright (c) 2014-2015 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Inspired by http://bugman123.com/Gears/
 * and by http://kennethsnelson.net/PortraitOfAnAtom.pdf
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        4           \n" \
			"*wireframe:    False       \n" \
			"*showFPS:      False       \n" \
		        "*texFontCacheSize: 100     \n" \
			"*suppressRotationAnimation: True\n" \
		"*font:  -*-helvetica-medium-r-normal-*-*-160-*-*-*-*-*-*\n" \

# define refresh_geodesic 0
# define release_geodesic 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "involute.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>
#include "texfont.h"


#ifdef USE_GL /* whole file */

#include "gllist.h"

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_LABELS      "False"
#define DEF_NUMBERS     "False"
#define DEF_TIMEOUT     "20"

typedef struct { double a, o; } LL;	/* latitude + longitude */

/* 10:6 is a mismesh. */

static const struct {
  enum { PRISM, OCTO, DECA, G14, G18, G32, G92, G182 } type;
  const GLfloat args[5];
} gear_templates[] = {
  { PRISM },
  { OCTO },
  { DECA },
  { G14 },
  { G18 },
  { G32, { 15, 6,  0.4535 }},		/* teeth1, teeth2, radius1 */
  { G32, { 15, 12, 0.3560 }},
  { G32, { 20, 6,  0.4850 }},
  { G32, { 20, 12, 0.3995 }},		/* double of 10:6 */
  { G32, { 20, 18, 0.3375 }},
  { G32, { 25, 6,  0.5065 }},
  { G32, { 25, 12, 0.4300 }},
  { G32, { 25, 18, 0.3725 }},
  { G32, { 25, 24, 0.3270 }},
  { G32, { 30, 12, 0.4535 }},		/* double of 15:6 */
  { G32, { 30, 18, 0.3995 }},
  { G32, { 30, 24, 0.3560 }},		/* double of 15:12 */
  { G32, { 30, 30, 0.3205 }},
  { G32, { 35, 12, 0.4710 }},
  { G32, { 35, 18, 0.4208 }},
  { G32, { 35, 24, 0.3800 }},
  { G32, { 35, 30, 0.3450 }},
  { G32, { 35, 36, 0.3160 }},
  { G32, { 40, 12, 0.4850 }},		/* double of 20:6 */
  { G32, { 40, 24, 0.3995 }},		/* double of 10:6, 20:12 */
/*{ G32, { 40, 36, 0.3375 }},*/		/* double of 20:18 */
  { G32, { 50, 12, 0.5065 }},		/* double of 25:6 */
  { G32, { 50, 24, 0.4300 }},		/* double of 25:12 */

  /* These all have phase errors and don't always mesh properly.
     Maybe we should just omit them? */

  { G92, { 35, 36, 16, 0.2660, 0.366 }},	/* teeth1, 2, 3, r1, pitch3 */
  { G92, { 25, 36, 11, 0.2270, 0.315 }},
/*{ G92, { 15, 15,  8, 0.2650, 0.356 }},*/
/*{ G92, { 20, 21,  8, 0.2760, 0.355 }},*/
  { G92, { 25, 27, 16, 0.2320, 0.359 }},
  { G92, { 20, 36, 11, 0.1875, 0.283 }},
  { G92, { 30, 30, 16, 0.2585, 0.374 }}, 	/* double of 15:15:8 */
  { G92, { 20, 33, 11, 0.1970, 0.293 }},
/*{ G92, { 10, 12,  8, 0.2030, 0.345 }},*/
  { G92, { 30, 33, 16, 0.2455, 0.354 }},
/*{ G92, { 25, 24,  8, 0.3050, 0.375 }},*/
  { G92, { 20, 24, 16, 0.2030, 0.346 }},
};


typedef struct sphere_gear sphere_gear;
struct sphere_gear {
  int id;			/* name, for debugging */
  XYZ axis;			/* the vector on which this gear's axis lies */
  int direction;		/* rotation, +1 or -1 */
  GLfloat offset;		/* rotational degrees from parent gear */
  sphere_gear *parent;		/* gear driving this one, or 0 for root */
  sphere_gear **children;	/* gears driven by this one (no loops) */
  sphere_gear **neighbors;	/* gears touching this one (circular!) */
  int nchildren, children_size;
  int nneighbors, neighbors_size;
  const gear *g;		/* shape of this gear (shared) */
};


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  int ncolors;
  XColor *colors;
  GLfloat color1[4], color2[4];
  texture_font_data *font;

  int nshapes, shapes_size;	/* how many 'gear' objects there are */
  int ngears, gears_size;	/* how many 'sphere_gear' objects there are */
  gear *shapes;
  sphere_gear *gears;

  int which;
  int mode;  /* 0 = normal, 1 = out, 2 = in */
  int mode_tick;
  int next;  /* 0 = random, -1 = back, 1 = forward */
  time_t draw_time;
  int draw_tick;
  char *desc;

  GLfloat th;		/* rotation of the root sphere_gear in degrees. */

} geodesic_configuration;

static geodesic_configuration *bps = NULL;

static int timeout;
static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static Bool do_labels;
static Bool do_numbers;

static XrmOptionDescRec opts[] = {
  { "-spin",      ".spin",   XrmoptionNoArg, "True"  },
  { "+spin",      ".spin",   XrmoptionNoArg, "False" },
  { "-speed",     ".speed",  XrmoptionSepArg, 0      },
  { "-wander",    ".wander", XrmoptionNoArg, "True"  },
  { "+wander",    ".wander", XrmoptionNoArg, "False" },
  { "-labels",	  ".labels", XrmoptionNoArg, "True"  },
  { "+labels",	  ".labels", XrmoptionNoArg, "False" },
  { "-numbers",	  ".numbers",XrmoptionNoArg, "True"  },
  { "+numbers",	  ".numbers",XrmoptionNoArg, "False" },
  { "-timeout",	  ".timeout",XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&do_labels, "labels", "Labels", DEF_LABELS, t_Bool},
  {&do_numbers,"numbers","Numbers",DEF_NUMBERS,t_Bool},
  {&timeout,   "timeout","Seconds",DEF_TIMEOUT,t_Int},
};

ENTRYPOINT ModeSpecOpt geodesic_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)



static XYZ
cross_product (XYZ a, XYZ b)
{
  XYZ c;
  c.x = (a.y * b.z) - (a.z * b.y);
  c.y = (a.z * b.x) - (a.x * b.z);
  c.z = (a.x * b.y) - (a.y * b.x);
  return c;
}


static GLfloat
dot_product (XYZ a, XYZ b)
{
  return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}


static XYZ
normalize (XYZ v)
{
  GLfloat d = sqrt ((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
  if (d == 0)
    v.x = v.y = v.z = 0;
  else
    {
      v.x /= d;
      v.y /= d;
      v.z /= d;
    }
  return v;
}


static XYZ
polar_to_cartesian (LL v)
{
  XYZ p;
  p.x = cos (v.a) * cos (v.o);
  p.y = cos (v.a) * sin (v.o);
  p.z = sin (v.a);
  return p;
}




static gear *
add_gear_shape (ModeInfo *mi, GLfloat radius, int teeth)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  gear *g;
  int i;

  if (bp->nshapes >= bp->shapes_size - 1)
    {
      bp->shapes_size = bp->shapes_size * 1.2 + 4;
      bp->shapes = (gear *)
        realloc (bp->shapes, bp->shapes_size * sizeof(*bp->shapes));
    }
  g = &bp->shapes[bp->nshapes++];

  memset (g, 0, sizeof(*g));

  g->r = radius;
  g->nteeth = teeth;
  g->ratio = 1;

  g->tooth_h = g->r / (teeth * 0.4);

  if (g->tooth_h > 0.06)  /* stubbier teeth when small tooth count. */
    g->tooth_h *= 0.6;

  g->thickness  = 0.05 + BELLRAND(0.15);
  g->thickness2 = g->thickness / 4;
  g->thickness3 = g->thickness;
  g->size       = wire ? INVOLUTE_SMALL : INVOLUTE_LARGE;

  /* Move the disc's origin inward to make the edge of the disc be tangent
     to the unit sphere. */
  g->z = 1 - sqrt (1 - (g->r * g->r));

  /* #### This isn't quite right */
  g->tooth_slope = 1 + ((g->z * 2) / g->r);


  /* Decide on shape of gear interior:
     - just a ring with teeth;
     - that, plus a thinner in-set "plate" in the middle;
     - that, plus a thin raised "lip" on the inner plate;
     - or, a wide lip (really, a thicker third inner plate.)
   */
  if (wire)
    ;
  else if ((random() % 10) == 0)
    {
      /* inner_r can go all the way in; there's no inset disc. */
      g->inner_r = (g->r * 0.3) + frand((g->r - g->tooth_h/2) * 0.6);
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
  if (!wire && g->nteeth > 5)
    {
      double size = 0;
      involute_biggest_ring (g, 0, &size, 0);
      if (size > g->r * 0.2 && (random() % 5) == 0)
        {
          g->nubs = 1 + (random() % 16);
          if (g->nubs > 8) g->nubs = 1;
        }
    }

  /* Decide how complex the polygon model should be.
   */
  {
    double pix = g->tooth_h * MI_HEIGHT(mi); /* approx. tooth size in pixels */
    if (pix <= 4)        g->size = INVOLUTE_SMALL;
    else if (pix <= 8)   g->size = INVOLUTE_MEDIUM;
    else if (pix <= 30)  g->size = INVOLUTE_LARGE;
    else                 g->size = INVOLUTE_HUGE;
  }

  if (g->inner_r3 > g->inner_r2) abort();
  if (g->inner_r2 > g->inner_r) abort();
  if (g->inner_r  > g->r) abort();

  i = random() % bp->ncolors;
  g->color[0] = bp->colors[i].red    / 65536.0;
  g->color[1] = bp->colors[i].green  / 65536.0;
  g->color[2] = bp->colors[i].blue   / 65536.0;
  g->color[3] = 1;

  i = (i + bp->ncolors / 2) % bp->ncolors;
  g->color2[0] = bp->colors[i].red    / 65536.0;
  g->color2[1] = bp->colors[i].green  / 65536.0;
  g->color2[2] = bp->colors[i].blue   / 65536.0;
  g->color2[3] = 1;

  g->dlist = glGenLists (1);
  glNewList (g->dlist, GL_COMPILE);

#if 1
  {
    gear G, *g2 = &G;
    *g2 = *g;

    /* Move the gear inward so that its outer edge is on the disc, instead
       of its midpoint. */
    g2->z += g2->thickness/2;

    /* 'radius' is at the surface but 'g->r' is at the center, so we need
       to reverse the slope computation that involute.c does. */
    g2->r /= (1 + (g2->thickness * g2->tooth_slope / 2));

    glPushMatrix();
    glTranslatef(g2->x, g2->y, -g2->z);

    /* Line up the center of the point of tooth 0 with "up". */
    glRotatef (90, 0, 0, 1);
    glRotatef (180, 0, 1, 0);
    glRotatef (-360.0 / g2->nteeth / 4, 0, 0, 1);

    g->polygons = draw_involute_gear (g2, wire);
    glPopMatrix();
  }
# else  /* draw discs */
  {
    glPushMatrix();
    glTranslatef(g->x, g->y, -g->z);
    glLineWidth (2);
    glFrontFace (GL_CCW);
    glNormal3f(0, 0, 1);
    glColor3f(0, 0, 0);
    glDisable (GL_LIGHTING);

    glBegin(GL_LINES);
    glVertex3f (0, 0, 0);
    glVertex3f (0, radius, 0);
    glEnd();

    glColor3f(0.5, 0.5, 0.5);
    glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
    {
      GLfloat th;
      GLfloat step = M_PI * 2 / 128;
      /* radius *= 1.005; */
      glVertex3f (0, 0, 0);
      for (th = 0; th < M_PI * 2 + step; th += step)
        {
          GLfloat x = cos(th) * radius;
          GLfloat y = sin(th) * radius;
          glVertex3f (x, y, 0);
        }
    }
    glEnd();
    if (!wire) glEnable(GL_LIGHTING);
    glPopMatrix();
  }
# endif /* 0 */

  glEndList ();

  return g;
}


static void
add_sphere_gear (ModeInfo *mi, gear *g, XYZ axis)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  sphere_gear *gg;
  int i;

  axis = normalize (axis);

  /* If there's already a gear on this axis, don't duplicate it. */
  for (i = 0; i < bp->ngears; i++)
    {
      XYZ o = bp->gears[i].axis;
      if (o.x == axis.x && o.y == axis.y && o.z == axis.z)
        return;
    }

  if (bp->ngears >= bp->gears_size - 1)
    {
      bp->gears_size = bp->gears_size * 1.2 + 10;
      bp->gears = (sphere_gear *)
        realloc (bp->gears, bp->gears_size * sizeof(*bp->gears));
    }

  gg = &bp->gears[bp->ngears];
  memset (gg, 0, sizeof(*gg));
  gg->id = bp->ngears;
  gg->axis = axis;
  gg->direction = 0;
  gg->g = g;
  bp->ngears++;
}


static void
free_sphere_gears (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < bp->nshapes; i++)
    {
      if (bp->shapes[i].dlist)
        glDeleteLists (bp->shapes[i].dlist, 1);
    }
  free (bp->shapes);
  bp->nshapes = 0;
  bp->shapes_size = 0;
  bp->shapes = 0;

  for (i = 0; i < bp->ngears; i++)
    {
      sphere_gear *g = &bp->gears[i];
      if (g->children)
        free (g->children);
      if (g->neighbors)
        free (g->neighbors);
    }
  free (bp->gears);
  bp->ngears = 0;
  bp->gears_size = 0;
  bp->gears = 0;
}



/* Is the gear a member of the list?
 */
static Bool
gear_list_member (sphere_gear *g, sphere_gear **list, int count)
{
  int i;
  for (i = 0; i < count; i++)
    if (list[i] == g) return True;
  return False;
}


/* Add the gear to the list, resizing it as needed.
 */
static void
gear_list_push (sphere_gear *g,
                sphere_gear ***listP,
                int *countP, int *sizeP)
{
  if (*countP >= (*sizeP) - 1)
    {
      *sizeP = (*sizeP) * 1.2 + 4;
      *listP = (sphere_gear **) realloc (*listP, (*sizeP) * sizeof(**listP));
    }
  (*listP)[*countP] = g;
  (*countP)++;
}


/* Mark child and parent as being mutual neighbors.
 */
static void
link_neighbors (sphere_gear *parent, sphere_gear *child)
{
  if (child == parent) abort();

  /* Add child to parent's list of neighbors */
  if (! gear_list_member (child, parent->neighbors, parent->nneighbors))
    {
      gear_list_push (child,
                      &parent->neighbors,
                      &parent->nneighbors,
                      &parent->neighbors_size);
      /* fprintf(stderr, "neighbor %2d -> %2d  (%d)\n", parent->id, child->id,
                parent->nneighbors); */
    }

  /* Add parent to child's list of neighbors */
  if (! gear_list_member (parent, child->neighbors, child->nneighbors))
    {
      gear_list_push (parent,
                      &child->neighbors,
                      &child->nneighbors,
                      &child->neighbors_size);
      /* fprintf(stderr, "neighbor %2d <- %2d\n", parent->id, child->id); */
    }
}

/* Mark child as having parent, and vice versa.
 */
static void
link_child (sphere_gear *parent, sphere_gear *child)
{
  if (child == parent) abort();
  if (child->parent) return;

  gear_list_push (child,
                  &parent->children,
                  &parent->nchildren,
                  &parent->children_size);
  child->parent = parent;
  /* fprintf(stderr, "child    %2d -> %2d  (%d)\n", parent->id, child->id,
            parent->nchildren); */
}



static void link_children (sphere_gear *);

static void
link_children (sphere_gear *parent)
{
  int i;
# if 1    /* depth first */
  for (i = 0; i < parent->nneighbors; i++)
    {
      sphere_gear *child = parent->neighbors[i];
      if (! child->parent)
        {
          link_child (parent, child);
          link_children (child);
        }
    }
# else   /* breadth first */
  for (i = 0; i < parent->nneighbors; i++)
    {
      sphere_gear *child = parent->neighbors[i];
      if (! child->parent)
        link_child (parent, child);
    }
  for (i = 0; i < parent->nchildren; i++)
    {
      sphere_gear *child = parent->children[i];
      link_children (child);
    }
# endif
}



/* Whether the two gears touch.
 */
static Bool
gears_touch_p (ModeInfo *mi, sphere_gear *a, sphere_gear *b)
{
  /* We need to know if the two discs on the surface overlap.

     Find the angle between the axis of each disc, and a point on its edge:
     the axis between the hypotenuse and adjacent of a right triangle between
     the disc's radius and the origin.

		      R
		    _____
		   |_|  /
		   |   /
		1  |  /
		   |t/    t = asin(R)
		   |/

     Find the angle between the axes of the two discs.

		  |
		  |    /    angle = acos (v1 dot v2)
		1 |   /     axis  = v1 cross v2
		  |  /  1
		  | /
		  |/

     If the sum of the first two angles is less than the third angle,
     they touch.
   */
  XYZ p1 = a->axis;
  XYZ p2 = b->axis;
  double t1 = asin (a->g->r);
  double t2 = asin (b->g->r);
  double th = acos (dot_product (p1, p2));

  return (t1 + t2 >= th);
}


/* Set the rotation direction for the gear and its kids.
 */
static void
orient_gears (ModeInfo *mi, sphere_gear *g)
{
  int i;
  if (g->parent)
    g->direction = -g->parent->direction;
  for (i = 0; i < g->nchildren; i++)
    orient_gears (mi, g->children[i]);
}


/* Returns the global model coordinates of the given tooth of a gear.
 */
static XYZ
tooth_coords (const sphere_gear *s, int tooth)
{
  const gear *g = s->g;
  GLfloat off = s->offset * (M_PI / 180) * g->ratio * s->direction;
  GLfloat th = (tooth * M_PI * 2 / g->nteeth) - off;
  XYZ axis;
  GLfloat angle;
  XYZ from = { 0, 1, 0 };
  XYZ to = s->axis;
  XYZ p0, p1, p2;
  GLfloat x, y, z, C, S, m[4][4];

  axis  = cross_product (from, to);
  angle = acos (dot_product (from, to));

  p0 = normalize (axis);
  x = p0.x;
  y = p0.y;
  z = p0.z;
  C = cos(angle);
  S = sin(angle);

  /* this is what glRotatef does */
  m[0][0] = x*x * (1 - C) + C;
  m[1][0] = x*y * (1 - C) - z*S;
  m[2][0] = x*z * (1 - C) + y*S;
  m[3][0] = 0;

  m[0][1] = y*x * (1 - C) + z*S;
  m[1][1] = y*y * (1 - C) + C;
  m[2][1] = y*z * (1 - C) - x*S;
  m[3][1] = 0;

  m[0][2] = x*z * (1 - C) - y*S;
  m[1][2] = y*z * (1 - C) + x*S;
  m[2][2] = z*z * (1 - C) + C;
  m[3][2] = 0;

  m[0][3] = 0;
  m[1][3] = 0;
  m[2][3] = 0;
  m[3][3] = 1;

  /* The point to transform */
  p1.x = g->r * sin (th);
  p1.z = g->r * cos (th);
  p1.y = 1 - g->z;
  p1 = normalize (p1);

  /* transformation result */
  p2.x = p1.x * m[0][0] + p1.y * m[1][0] + p1.z * m[2][0] + m[3][0];
  p2.y = p1.x * m[0][1] + p1.y * m[1][1] + p1.z * m[2][1] + m[3][1];
  p2.z = p1.x * m[0][2] + p1.y * m[1][2] + p1.z * m[2][2] + m[3][2];

  return p2;
}


/* Returns the number of the tooth of the first gear that is closest
   to any tooth of its parent.  Also the position of the parent tooth.
 */
static int
parent_tooth (const sphere_gear *s, XYZ *parent)
{
  const sphere_gear *s2 = s->parent;
  int i, j;
  GLfloat min_dist = 99999;
  int min_tooth = 0;
  XYZ min_parent = { 0, 0, 0 };

  if (s2)
    for (i = 0; i < s->g->nteeth; i++)
      {
        XYZ p1 = tooth_coords (s, i);
        for (j = 0; j < s2->g->nteeth; j++)
          {
            XYZ p2 = tooth_coords (s2, j);
            XYZ d;
            GLfloat dist;
            d.x = p1.x - p2.x;
            d.y = p1.y - p2.y;
            d.z = p1.z - p2.z;

            dist = sqrt (d.x*d.x + d.y*d.y + d.z*d.z);
            if (dist < min_dist)
              {
                min_dist = dist;
                min_parent = p2;
                min_tooth = i;
              }
          }
      }
  *parent = min_parent;
  return min_tooth;
}


/* Make all of the gear's children's teeth mesh properly.
 */
static void align_gear_teeth (sphere_gear *s);
static void
align_gear_teeth (sphere_gear *s)
{
  int i;
  XYZ pc;

  if (s->parent)
    {
      /* Iterate this gear's offset until we find a value for it that
         minimizes the distance between this gear's parent-pointing
         tooth, and the corresponding tooth on the parent.
      */
      int pt = parent_tooth (s, &pc);
      GLfloat range = 360 / s->g->nteeth;
      GLfloat steps = 64;
      GLfloat min_dist = 999999;
      GLfloat min_off = 0;
      GLfloat off;

      for (off = -range/2; off < range/2; off += range/steps)
        {
          XYZ tc, d;
          GLfloat dist;
          s->offset = off;
          tc = tooth_coords (s, pt);
          d.x = pc.x - tc.x;
          d.y = pc.y - tc.y;
          d.z = pc.z - tc.z;
          dist = sqrt (d.x*d.x + d.y*d.y + d.z*d.z);
          if (dist < min_dist)
            {
              min_dist = dist;
              min_off = off;
            }
        }

      s->offset = min_off;
    }

  /* Now do the children.  We have to do it in parent/child order because
     the offset we just computed for the parent affects everyone downstream.
   */
  for (i = 0; i < s->nchildren; i++)
    align_gear_teeth (s->children[i]);
}



static void
describe_gears (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  int gears_per_teeth[1000];
  int i;
  int lines = 0;
  memset (gears_per_teeth, 0, sizeof(gears_per_teeth));
  for (i = 0; i < bp->ngears; i++)
    gears_per_teeth[bp->gears[i].g->nteeth]++;
  if (bp->desc) free (bp->desc);
  bp->desc = (char *) malloc (80 * bp->ngears);
  *bp->desc = 0;
  for (i = 0; i < countof(gears_per_teeth); i++)
    if (gears_per_teeth[i])
      {
        sprintf (bp->desc + strlen(bp->desc),
                 "%s%d gears with %d teeth",
                 (lines > 0 ? ",\n" : ""),
                 gears_per_teeth[i], i);
        lines++;
      }
  if (lines > 1)
    sprintf (bp->desc + strlen(bp->desc), ",\n%d gears total", bp->ngears);
  strcat (bp->desc, ".");
}


/* Takes the gears and makes an arbitrary DAG of them in order to compute
   direction and gear ratios.
 */
static void
sort_gears (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  sphere_gear *root = 0;
  int i, j;

  /* For each gear, compare it against every other gear.
     If they touch, mark them as being each others' neighbors.
   */
  for (i = 0; i < bp->ngears; i++)
    {
      sphere_gear *a = &bp->gears[i];
      for (j = 0; j < bp->ngears; j++)
        {
          sphere_gear *b = &bp->gears[j];
          if (a == b) continue;
          if (gears_touch_p (mi, a, b))
            link_neighbors (a, b);
        }
    }

  bp->gears[0].parent = &bp->gears[0]; /* don't give this one a parent */
  link_children (&bp->gears[0]);
  bp->gears[0].parent = 0;


# if 0
  for (i = 0; i < bp->ngears; i++)
    {
      fprintf (stderr, "%2d: p = %2d; k(%d, %d) = ",
               i,
               bp->gears[i].parent ? bp->gears[i].parent->id : -1,
               bp->gears[i].nneighbors,
               bp->gears[i].nchildren);
      for (j = 0; j < bp->gears[i].nneighbors; j++)
        fprintf (stderr, "%2d ", (int) bp->gears[i].neighbors[j]->id);
      fprintf (stderr, "\t\t");
      if (j < 5) fprintf (stderr, "\t");
      for (j = 0; j < bp->gears[i].nchildren; j++)
        fprintf (stderr, "%2d ", (int) bp->gears[i].children[j]->id);
      fprintf (stderr,"\n");
    }
  fprintf (stderr,"\n");
# endif /* 0 */


  /* If there is more than one gear with no parent, we fucked up. */

  root = 0;
  for (i = 0; i < bp->ngears; i++)
    {
      sphere_gear *g = &bp->gears[i];
      if (!g->parent)
        root = g;
    }

  if (! root) abort();

  root->direction = 1;
  orient_gears (mi, root);

  /* If there are any gears with no direction, they aren't reachable. */
  for (i = 0; i < bp->ngears; i++)
    {
      sphere_gear *g = &bp->gears[i];
      if (g->direction == 0)
        fprintf(stderr,"INTERNAL ERROR: unreachable: %d\n", g->id);
    }

  align_gear_teeth (root);
  describe_gears (mi);
}


/* Create 5 identical gears arranged on the faces of a uniform
   triangular prism.
 */
static void
make_prism (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  gear *g;
  XYZ a;
  int i;
  int teeth = 4 * (4 + (int) (BELLRAND(20)));
  if (teeth % 4) abort();  /* must be a multiple of 4 */

  g = add_gear_shape (mi, 0.7075, teeth);

  a.x = 0; a.y = 0; a.z = 1;
  add_sphere_gear (mi, g, a);
  a.z = -1;
  add_sphere_gear (mi, g, a);

  a.z = 0;
  for (i = 0; i < 3; i++)
    {
      GLfloat th = i * M_PI * 2 / 3;
      a.x = cos (th);
      a.y = sin (th);
      add_sphere_gear (mi, g, a);
    }

  if (bp->ngears != 5) abort();
}


/* Create 8 identical gears arranged on the faces of an octohedron
   (or alternately, arranged on the diagonals of a cube.)
 */
static void
make_octo (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  static const XYZ verts[] = {{ -1, -1, -1 },
                              { -1, -1,  1 },
                              { -1,  1,  1 },
                              { -1,  1, -1 },
                              {  1, -1,  1 },
                              {  1, -1, -1 },
                              {  1,  1, -1 },
                              {  1,  1,  1 }};
  gear *g;
  int i;
  int teeth = 4 * (4 + (int) (BELLRAND(20)));
  if (teeth % 4) abort();  /* must be a multiple of 4 */

  g = add_gear_shape (mi, 0.578, teeth);
  for (i = 0; i < countof(verts); i++)
    add_sphere_gear (mi, g, verts[i]);

  if (bp->ngears != 8) abort();
}


/* Create 10 identical gears arranged on the faces of ... something.
   I'm not sure what polyhedron is the basis of this.
 */
static void
make_deca (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  gear *g;
  XYZ a;
  int i, j;
  int teeth = 4 * (4 + (int) (BELLRAND(15)));
  if (teeth % 4) abort();  /* must be a multiple of 4 */

  g = add_gear_shape (mi, 0.5415, teeth);

  a.x = 0; a.y = 0; a.z = 1;
  add_sphere_gear (mi, g, a);
  a.z = -1;
  add_sphere_gear (mi, g, a);

  for (j = -1; j <= 1; j += 2)
    {
      GLfloat off = (j < 0 ? 0 : M_PI / 4);
      LL v;
      v.a = j * M_PI * 0.136;  /* #### Empirical. What is this? */
      for (i = 0; i < 4; i++)
        {
          v.o = i * M_PI / 2 + off;
          a = polar_to_cartesian (v);
          add_sphere_gear (mi, g, a);
        }
    }
  if (bp->ngears != 10) abort();
}


/* Create 14 identical gears arranged on the faces of ... something.
   I'm not sure what polyhedron is the basis of this.
 */
static void
make_14 (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  gear *g;
  XYZ a;
  int i;
  GLfloat r = 0.4610;
  int teeth = 6 * (2 + (int) (BELLRAND(4)));
  if (teeth % 6) abort();  /* must be a multiple of 6. I think? */
  /* mismeshes: 24 30 34 36 42 48 54 60 */

  /* North, south */
  g = add_gear_shape (mi, r, teeth);
  a.x = 0; a.y = 0; a.z = 1;
  add_sphere_gear (mi, g, a);
  a.z = -1;
  add_sphere_gear (mi, g, a);

  /* Equator */
  a.z = 0;
  for (i = 0; i < 4; i++)
    {
      GLfloat th = i * M_PI * 2 / 4 + (M_PI / 4);
      a.x = cos(th);
      a.y = sin(th);
      add_sphere_gear (mi, g, a);
    }

  /* The other 8 */
  g = add_gear_shape (mi, r, teeth);

  for (i = 0; i < 4; i++)
    {
      LL v;
      v.a = M_PI * 0.197;  /* #### Empirical.  Also, wrong.  What is this? */
      v.o = i * M_PI * 2 / 4;
      a = polar_to_cartesian (v);
      add_sphere_gear (mi, g, a);
      v.a = -v.a;
      a = polar_to_cartesian (v);
      add_sphere_gear (mi, g, a);
    }

  if (bp->ngears != 14) abort();
}


/* Create 18 identical gears arranged on the faces of ... something.
   I'm not sure what polyhedron is the basis of this.
 */
static void
make_18 (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  gear *g, *g2;
  XYZ a;
  int i;
  GLfloat r = 0.3830;
  int sizes[] = { 8, 12, 16, 20 };  /* 10, 14, 18, 26 and 34 don't work */
  int teeth = sizes[random() % countof(sizes)] * (1 + (random() % 4));

  /* North, south */
  g = add_gear_shape (mi, r, teeth);
  a.x = 0; a.y = 0; a.z = 1;
  add_sphere_gear (mi, g, a);
  a.z = -1;
  add_sphere_gear (mi, g, a);

  /* Equator */
  g2 = add_gear_shape (mi, r, teeth);
  a.z = 0;
  for (i = 0; i < 8; i++)
    {
      GLfloat th = i * M_PI * 2 / 8 + (M_PI / 4);
      a.x = cos(th);
      a.y = sin(th);
      add_sphere_gear (mi, (i & 1 ? g : g2), a);
    }

  /* The other 16 */
  g = add_gear_shape (mi, r, teeth);

  for (i = 0; i < 4; i++)
    {
      LL v;
      v.a = M_PI * 0.25;
      v.o = i * M_PI * 2 / 4;
      a = polar_to_cartesian (v);
      add_sphere_gear (mi, g, a);
      v.a = -v.a;
      a = polar_to_cartesian (v);
      add_sphere_gear (mi, g, a);
    }

  if (bp->ngears != 18) abort();
}


/* Create 32 gears arranged along a truncated icosahedron:
   One gear on each of the 20 faces, and one on each of the 12 vertices.
 */
static void
make_32 (ModeInfo *mi, const GLfloat *args)
{
  /* http://bugman123.com/Gears/32GearSpheres/ */
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat th0 = atan (0.5);  /* lat division: 26.57 deg */
  GLfloat s = M_PI / 5;	     /* lon division: 72 deg    */
  int i;

  int teeth1 = args[0];
  int teeth2 = args[1];
  GLfloat r1 = args[2];
  GLfloat ratio = teeth2 / (GLfloat) teeth1;
  GLfloat r2 = r1 * ratio;

  gear *gear1, *gear2;

  if (teeth1 % 5) abort();
  if (teeth2 % 6) abort();

  gear1 = add_gear_shape (mi, r1, teeth1);
  gear2 = add_gear_shape (mi, r2, teeth2);
  gear2->ratio = 1 / ratio;

  {
    XYZ a = { 0, 0, 1 };
    XYZ b = { 0, 0, -1 };
    add_sphere_gear (mi, gear1, a);
    add_sphere_gear (mi, gear1, b);
  }

  for (i = 0; i < 10; i++)
    {
      GLfloat th1 = s * i;
      GLfloat th2 = s * (i+1);
      GLfloat th3 = s * (i+2);
      LL v1, v2, v3, vc;
      XYZ p1, p2, p3, pc, pc2;
      v1.a = th0;    v1.o = th1;
      v2.a = th0;    v2.o = th3;
      v3.a = -th0;   v3.o = th2;
      vc.a = M_PI/2; vc.o = th2;

      if (! (i & 1)) /* southern hemisphere */
        {
          v1.a = -v1.a;
          v2.a = -v2.a;
          v3.a = -v3.a;
          vc.a = -vc.a;
        }

      p1 = polar_to_cartesian (v1);
      p2 = polar_to_cartesian (v2);
      p3 = polar_to_cartesian (v3);
      pc = polar_to_cartesian (vc);

      /* Two faces: 123 and 12c. */

      add_sphere_gear (mi, gear1, p1);	/* left shared point of 2 triangles */

      pc2.x = (p1.x + p2.x + p3.x) / 3;	/* center of bottom triangle */
      pc2.y = (p1.y + p2.y + p3.y) / 3;
      pc2.z = (p1.z + p2.z + p3.z) / 3;
      add_sphere_gear (mi, gear2, pc2);

      pc2.x = (p1.x + p2.x + pc.x) / 3;	/* center of top triangle */
      pc2.y = (p1.y + p2.y + pc.y) / 3;
      pc2.z = (p1.z + p2.z + pc.z) / 3;
      add_sphere_gear (mi, gear2, pc2);
    }

  if (bp->ngears != 32) abort();
}


/* Create 92 gears arranged along a geodesic sphere: 20 + 12 + 60.
   (frequency 3v, class-I geodesic tessellation of an icosahedron)
 */
static void
make_92 (ModeInfo *mi, const GLfloat *args)
{
  /* http://bugman123.com/Gears/92GearSpheres/ */
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat th0 = atan (0.5);  /* lat division: 26.57 deg */
  GLfloat s = M_PI / 5;	     /* lon division: 72 deg    */
  int i;

  int tscale = 2;	/* These don't mesh properly, so let's increase the
                           number of teeth so that it's not so obvious. */

  int teeth1 = args[0] * tscale;
  int teeth2 = args[1] * tscale;
  int teeth3 = args[2] * tscale;
  GLfloat r1 = args[3];
  GLfloat ratio2 = teeth2 / (GLfloat) teeth1;
  GLfloat ratio3 = teeth3 / (GLfloat) teeth2;
  GLfloat r2 = r1 * ratio2;
  GLfloat r3 = r2 * ratio3;

  GLfloat r4 = args[4];	/* #### Empirical. Not sure what its basis is. */
  GLfloat r5 = 1 - r4;

  gear *gear1, *gear2, *gear3;

  gear1 = add_gear_shape (mi, r1, teeth1);
  gear2 = add_gear_shape (mi, r2, teeth2);
  gear3 = add_gear_shape (mi, r3, teeth3);
  gear2->ratio = 1 / ratio2;
  gear3->ratio = 1 / ratio3;

  {
    XYZ a = { 0, 0, 1 };
    XYZ b = { 0, 0, -1 };
    add_sphere_gear (mi, gear1, a);
    add_sphere_gear (mi, gear1, b);
  }

  for (i = 0; i < 10; i++)
    {
      GLfloat th1 = s * i;
      GLfloat th2 = s * (i+1);
      GLfloat th3 = s * (i+2);
      LL v1, v2, v3, vc;
      XYZ p1, p2, p3, pc, pc2;
      v1.a = th0;    v1.o = th1;
      v2.a = th0;    v2.o = th3;
      v3.a = -th0;   v3.o = th2;
      vc.a = M_PI/2; vc.o = th2;

      if (! (i & 1)) /* southern hemisphere */
        {
          v1.a = -v1.a;
          v2.a = -v2.a;
          v3.a = -v3.a;
          vc.a = -vc.a;
        }

      p1 = polar_to_cartesian (v1);
      p2 = polar_to_cartesian (v2);
      p3 = polar_to_cartesian (v3);
      pc = polar_to_cartesian (vc);

      /* Two faces: 123 and 12c. */

      add_sphere_gear (mi, gear1, p1);	/* left shared point of 2 triangles */

      pc2.x = (p1.x + p2.x + p3.x) / 3;	/* center of bottom triangle */
      pc2.y = (p1.y + p2.y + p3.y) / 3;
      pc2.z = (p1.z + p2.z + p3.z) / 3;
      add_sphere_gear (mi, gear2, pc2);

      pc2.x = (p1.x + p2.x + pc.x) / 3;	/* center of top triangle */
      pc2.y = (p1.y + p2.y + pc.y) / 3;
      pc2.z = (p1.z + p2.z + pc.z) / 3;
      add_sphere_gear (mi, gear2, pc2);

      /* left edge of bottom triangle, 1/3 in */
      pc2.x = p1.x + (p3.x - p1.x) * r4;
      pc2.y = p1.y + (p3.y - p1.y) * r4;
      pc2.z = p1.z + (p3.z - p1.z) * r4;
      add_sphere_gear (mi, gear3, pc2);

      /* left edge of bottom triangle, 2/3 in */
      pc2.x = p1.x + (p3.x - p1.x) * r5;
      pc2.y = p1.y + (p3.y - p1.y) * r5;
      pc2.z = p1.z + (p3.z - p1.z) * r5;
      add_sphere_gear (mi, gear3, pc2);

      /* left edge of top triangle, 1/3 in */
      pc2.x = p1.x + (pc.x - p1.x) * r4;
      pc2.y = p1.y + (pc.y - p1.y) * r4;
      pc2.z = p1.z + (pc.z - p1.z) * r4;
      add_sphere_gear (mi, gear3, pc2);

      /* left edge of top triangle, 2/3 in */
      pc2.x = p1.x + (pc.x - p1.x) * r5;
      pc2.y = p1.y + (pc.y - p1.y) * r5;
      pc2.z = p1.z + (pc.z - p1.z) * r5;
      add_sphere_gear (mi, gear3, pc2);

      /* center of shared edge, 1/3 in */
      pc2.x = p1.x + (p2.x - p1.x) * r4;
      pc2.y = p1.y + (p2.y - p1.y) * r4;
      pc2.z = p1.z + (p2.z - p1.z) * r4;
      add_sphere_gear (mi, gear3, pc2);

      /* center of shared edge, 2/3 in */
      pc2.x = p1.x + (p2.x - p1.x) * r5;
      pc2.y = p1.y + (p2.y - p1.y) * r5;
      pc2.z = p1.z + (p2.z - p1.z) * r5;
      add_sphere_gear (mi, gear3, pc2);
    }

  if (bp->ngears != 92) abort();
}

static void
make_182 (ModeInfo *mi, const GLfloat *args)
{
  /* #### TODO: http://bugman123.com/Gears/182GearSpheres/ */
  abort();
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_geodesic (ModeInfo *mi, int width, int height)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
pick_shape (ModeInfo *mi, time_t last)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  int count = countof (gear_templates);

  if (bp->colors)
    free (bp->colors);

  bp->ncolors = 1024;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  free_sphere_gears (mi);

  if (last == 0)
    {
      bp->which = random() % count;
    }
  else if (bp->next < 0)
    {
      bp->which--;
      if (bp->which < 0) bp->which = count-1;
      bp->next = 0;
    }
  else if (bp->next > 0)
    {
      bp->which++;
      if (bp->which >= count) bp->which = 0;
      bp->next = 0;
    }
  else
    {
      int n = bp->which;
      while (n == bp->which)
        n = random() % count;
      bp->which = n;
    }

  switch (gear_templates[bp->which].type) {
  case PRISM: make_prism (mi); break;
  case OCTO:  make_octo (mi); break;
  case DECA:  make_deca (mi); break;
  case G14:   make_14 (mi); break;
  case G18:   make_18 (mi); break;
  case G32:   make_32 (mi, gear_templates[bp->which].args); break;
  case G92:   make_92 (mi, gear_templates[bp->which].args); break;
  case G182:  make_182(mi, gear_templates[bp->which].args); break;
  default: abort(); break;
  }

  sort_gears (mi);
}


static void free_geodesic (ModeInfo *mi);

ENTRYPOINT void 
init_geodesic (ModeInfo *mi)
{
  geodesic_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps, free_geodesic);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_geodesic (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    static GLfloat cspec[4] = {1.0, 1.0, 1.0, 1.0};
    static const GLfloat shiny = 128.0;

    static GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
    static GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
    static GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
    static GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

    glMaterialfv (GL_FRONT, GL_SPECULAR,  cspec);
    glMateriali  (GL_FRONT, GL_SHININESS, shiny);
  }

  if (! wire)
    {
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable (GL_LIGHTING);
      glEnable (GL_LIGHT0);
    }

  if (! bp->rot)
  {
    double spin_speed   = 0.25 * speed;
    double wander_speed = 0.01 * speed;
    double spin_accel   = 0.2;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->trackball = gltrackball_init (True);
  }

  bp->font = load_texture_font (MI_DISPLAY(mi), "font");

  pick_shape (mi, 0);
}


ENTRYPOINT Bool
geodesic_handle_event (ModeInfo *mi, XEvent *event)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else
    {
      if (event->xany.type == KeyPress)
        {
          KeySym keysym;
          char c = 0;
          XLookupString (&event->xkey, &c, 1, &keysym, 0);
          if (c == '<' || c == ',' || c == '-' || c == '_' ||
              keysym == XK_Left || keysym == XK_Up || keysym == XK_Prior)
            {
              bp->next = -1;
              goto SWITCH;
            }
          else if (c == '>' || c == '.' || c == '=' || c == '+' ||
                   keysym == XK_Right || keysym == XK_Down ||
                   keysym == XK_Next)
            {
              bp->next = 1;
              goto SWITCH;
            }
        }

      if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        {
        SWITCH:
          bp->mode = 1;
          bp->mode_tick = 4;
          return True;
        }
    }

  return False;
}


ENTRYPOINT void
draw_geodesic (ModeInfo *mi)
{
  time_t now = time ((time_t *) 0);
  int wire = MI_IS_WIREFRAME(mi);
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));


  if (bp->draw_time == 0)
    {
      pick_shape (mi, bp->draw_time);
      bp->draw_time = now;
    }
  else if (bp->mode == 0)
    {
      if (bp->draw_tick++ > 10)
        {
          if (bp->draw_time == 0) bp->draw_time = now;
          bp->draw_tick = 0;

          if (!bp->button_down_p &&
              bp->draw_time + timeout <= now)
            {
              /* randomize every -timeout seconds */
              bp->mode = 1;    /* go out */
              bp->mode_tick = 10 / speed;
              bp->draw_time = now;
            }
        }
    }
  else if (bp->mode == 1)   /* out */
    {
      if (--bp->mode_tick <= 0)
        {
          bp->mode_tick = 10 / speed;
          bp->mode = 2;  /* go in */
          pick_shape (mi, bp->draw_time);
          bp->draw_time = now;
        }
    }
  else if (bp->mode == 2)   /* in */
    {
      if (--bp->mode_tick <= 0)
        bp->mode = 0;  /* normal */
    }
  else
    abort();


  if (! wire)
    glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 17);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (6, 6, 6);

  if (bp->ngears < 14)
    glScalef (0.8, 0.8, 0.8);  /* make these a little easier to see */

  if (bp->mode != 0)
    {
      GLfloat s = (bp->mode == 1
                   ? bp->mode_tick / (10 / speed)
                   : ((10 / speed) - bp->mode_tick + 1) / (10 / speed));
      glScalef (s, s, s);
    }


  {
    int i;
    for (i = 0; i < bp->ngears; i++)
      {
        const sphere_gear *s = &bp->gears[i];
        const gear *g = s->g;

        XYZ axis;
        XYZ from = { 0, 1, 0 };
        XYZ to = s->axis;
        GLfloat angle;
        GLfloat off = s->offset;

        /* If an even number of teeth, offset by 1/2 tooth width. */
        if (s->direction > 0 && !(g->nteeth & 1))
          off += 360 / g->nteeth / 2;

        axis  = cross_product (from, to);
        angle = acos (dot_product (from, to));

        glPushMatrix();
        glTranslatef (to.x, to.y, to.z);
        glRotatef (angle / M_PI * 180, axis.x, axis.y, axis.z);
        glRotatef (-90, 1, 0, 0);
        glRotatef(180, 0, 0, 1);
        glRotatef ((bp->th - off) * g->ratio * s->direction,
                   0, 0, 1);

        glCallList (g->dlist);
        mi->polygon_count += g->polygons;
        glPopMatrix();

#if 0
        {				/* Draw tooth vectors */
          GLfloat r = 1 - g->z;
          XYZ pc;
          int pt = parent_tooth (s, &pc);
          int t;
          glDisable(GL_LIGHTING);
          glLineWidth (8);
          for (t = 0; t < g->nteeth; t++)
            {
              XYZ p = tooth_coords (s, t);
              XYZ p2;
              p2.x = (r * s->axis.x + p.x) / 2;
              p2.y = (r * s->axis.y + p.y) / 2;
              p2.z = (r * s->axis.z + p.z) / 2;

              if (t == pt)
                glColor3f(1,0,1);
              else
                glColor3f(0,1,1);
              glBegin(GL_LINES);
              glVertex3f (p.x, p.y, p.z);
              glVertex3f (p2.x, p2.y, p2.z);
              glEnd();
            }
          if (!wire) glEnable(GL_LIGHTING);
          glLineWidth (1);
        }
#endif

#if 0
        {				/* Draw the parent/child DAG */
          GLfloat s1 = 1.1;
          GLfloat s2 = s->parent ? s1 : 1.5;
          GLfloat s3 = 1.0;
          XYZ p1 = s->axis;
          XYZ p2 = s->parent ? s->parent->axis : p1;
          glDisable(GL_LIGHTING);
          glColor3f(0,0,1);
          glBegin(GL_LINES);
          glVertex3f (s1 * p1.x, s1 * p1.y, s1 * p1.z);
          glVertex3f (s2 * p2.x, s2 * p2.y, s2 * p2.z);
          glVertex3f (s1 * p1.x, s1 * p1.y, s1 * p1.z);
          glVertex3f (s3 * p1.x, s3 * p1.y, s3 * p1.z);
          glEnd();
          if (!wire) glEnable(GL_LIGHTING);
        }
#endif
      }

    /* We need to draw the fonts in a second pass in order to make
       transparency (as a result of anti-aliasing) work properly.
      */
    if (do_numbers && bp->mode == 0)
      for (i = 0; i < bp->ngears; i++)
        {
          const sphere_gear *s = &bp->gears[i];
          const gear *g = s->g;

          XYZ axis;
          XYZ from = { 0, 1, 0 };
          XYZ to = s->axis;
          GLfloat angle;
          GLfloat off = s->offset;

          int w, h, j;
          char buf[100];
          XCharStruct e;

          /* If an even number of teeth, offset by 1/2 tooth width. */
          if (s->direction > 0 /* && !(g->nteeth & 1) */)
            off += 360 / g->nteeth / 2;

          axis  = cross_product (from, to);
          angle = acos (dot_product (from, to));

          glPushMatrix();
          glTranslatef(to.x, to.y, to.z);
          glRotatef (angle / M_PI * 180, axis.x, axis.y, axis.z);
          glRotatef (-90, 1, 0, 0);
          glRotatef(180, 0, 0, 1);
          glRotatef ((bp->th - off) * g->ratio * s->direction,
                     0, 0, 1);

          glDisable (GL_LIGHTING);
          glColor3f(1, 1, 0);
          glPushMatrix();
          glScalef(0.005, 0.005, 0.005);
          sprintf (buf, "%d", i);
          texture_string_metrics (bp->font, buf, &e, 0, 0);
          w = e.width;
          h = e.ascent + e.descent;
          glTranslatef (-w/2, -h/2, 0);
          print_texture_string (bp->font, buf);
          glPopMatrix();

# if 1
          for (j = 0; j < g->nteeth; j++)          /* Number the teeth */
            {
              GLfloat ss = 0.08 * g->r / g->nteeth;
              GLfloat r = g->r * 0.88;
              GLfloat th = M_PI - (j * M_PI * 2 / g->nteeth + M_PI/2);


              glPushMatrix();
              glTranslatef (r * cos(th), r * sin(th), -g->z + 0.01);
              glScalef(ss, ss, ss);
              sprintf (buf, "%d", j + 1);
              texture_string_metrics (bp->font, buf, &e, 0, 0);
              w = e.width;
              h = e.ascent + e.descent;
              glTranslatef (-w/2, -h/2, 0);
              print_texture_string (bp->font, buf);
              glPopMatrix();
            }
# endif
          glPopMatrix();
          if (!wire) glEnable(GL_LIGHTING);
        }

    bp->th += 0.7 * speed;    /* Don't mod this to 360 - causes glitches. */
  }

  if (do_labels && bp->mode == 0)
    {
      glColor3f (1, 1, 0);
      print_texture_label (mi->dpy, bp->font,
                           mi->xgwa.width, mi->xgwa.height,
                           1, bp->desc);
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

static void
free_geodesic (ModeInfo *mi)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context)
    return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));
  free_texture_font (bp->font);
  free (bp->colors);
  free_sphere_gears (mi);
  if (bp->desc) free (bp->desc);
}


XSCREENSAVER_MODULE_2 ("GeodesicGears", geodesicgears, geodesic)

#endif /* USE_GL */
