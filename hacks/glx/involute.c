/* involute, Copyright (c) 2004-2007 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Utilities for rendering OpenGL gears with involute teeth.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "screenhackI.h"

#ifndef HAVE_COCOA
# include <GL/glx.h>
# include <GL/glu.h>
#endif /* !HAVE_COCOA */

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "involute.h"
#include "normals.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* For debugging: if true then in wireframe, do not abbreviate. */
static Bool wire_all_p = False;
static Bool show_normals_p = False;


/* Draws an uncapped tube of the given radius extending from top to bottom,
   with faces pointing either in or out.
 */
static int
draw_ring (int segments,
           GLfloat r, GLfloat top, GLfloat bottom, 
           Bool in_p, Bool wire_p)
{
  int i;
  int polys = 0;
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
draw_disc (int segments,
           GLfloat ra, GLfloat rb, GLfloat z, 
           Bool up_p, Bool wire_p)
{
  int i;
  int polys = 0;
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
    }
  polys += segments;
  glEnd();
  return polys;
}


/* Draws N thick radial lines between the given radii,
   with faces pointing either up or down.
 */
static int
draw_spokes (int n, GLfloat thickness, int segments,
             GLfloat ra, GLfloat rb, GLfloat z1, GLfloat z2,
             Bool wire_p)
{
  int i;
  int polys = 0;
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
  return polys;
}


/* Draws some bumps (embedded cylinders) on the gear.
 */
static int
draw_gear_nubs (gear *g, Bool wire_p)
{
  int polys = 0;
  int i;
  int steps = (g->size != INVOLUTE_LARGE ? 5 : 20);
  double r, size, height;
  GLfloat *cc;
  int which;
  GLfloat width, off;

  if (! g->nubs) return 0;

  which = involute_biggest_ring (g, &r, &size, &height);
  size /= 5;
  height *= 0.7;

  cc = (which == 1 ? g->color : g->color2);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, cc);

  if (g->inverted_p)
    r = g->r + size + g->tooth_h;

  width = M_PI * 2 / g->nubs;
  off = M_PI / (g->nteeth * 2);  /* align first nub with a tooth */

  for (i = 0; i < g->nubs; i++)
    {
      GLfloat th = (i * width) + off;
      glPushMatrix();

      glRotatef (th * 180 / M_PI, 0, 0, 1);
      glTranslatef (r, 0, 0);

      if (g->inverted_p)	/* nubs go on the outside rim */
        {
          size = g->thickness / 3;
          height = (g->r - g->inner_r)/2;
          glTranslatef (height, 0, 0);
          glRotatef (90, 0, 1, 0);
        }

      if (wire_p && !wire_all_p)
        polys += draw_ring ((g->size == INVOLUTE_LARGE ? steps/2 : steps),
                            size, 0, 0, False, wire_p);
      else
        {
          polys += draw_disc (steps, 0, size, -height,      True,  wire_p);
          polys += draw_disc (steps, 0, size,  height,      False, wire_p);
          polys += draw_ring (steps, size, -height, height, False, wire_p);
        }
      glPopMatrix();
    }
  return polys;
}



/* Draws a much simpler representation of a gear.
   Returns the number of polygons.
 */
int
draw_involute_schematic (gear *g, Bool wire_p)
{
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
draw_gear_interior (gear *g, Bool wire_p)
{
  int polys = 0;

  int steps = g->nteeth * 2;
  if (steps < 10) steps = 10;
  if ((wire_p && !wire_all_p) || g->size != INVOLUTE_LARGE) steps /= 2;
  if (g->size != INVOLUTE_LARGE && steps > 16) steps = 16;

  /* ring 1 (facing in) is done in draw_gear_teeth */

  /* ring 2 (facing in) and disc 2
   */
  if (g->inner_r2)
    {
      GLfloat ra = g->inner_r * 1.04;  /* slightly larger than inner_r */
      GLfloat rb = g->inner_r2;        /*  since the points don't line up */
      GLfloat za = -g->thickness2/2;
      GLfloat zb =  g->thickness2/2;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color2);

      if ((g->coax_p != 1 && !g->inner_r3) ||
          (wire_p && wire_all_p))
        polys += 
          draw_ring (steps, rb, za, zb, True, wire_p);  /* ring facing in*/

      if (wire_p && wire_all_p)
        polys += 
          draw_ring (steps, ra, za, zb, True, wire_p);  /* ring facing in*/

      if (g->spokes)
        polys += draw_spokes (g->spokes, g->spoke_thickness,
                              steps, ra, rb, za, zb, wire_p);
      else if (!wire_p || wire_all_p)
        {
          polys += 
            draw_disc (steps, ra, rb, za, True, wire_p);  /* top plate */
          polys += 
            draw_disc (steps, ra, rb, zb, False, wire_p); /* bottom plate*/
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

      polys += 
        draw_ring (steps, ra, za, zb, False, wire_p);  /* ring facing out */

      if (g->coax_p != 1 || (wire_p && wire_all_p))
        polys +=
          draw_ring (steps, rb, za, zb, True, wire_p);  /* ring facing in */

      if (!wire_p || wire_all_p)
        {
          polys += 
            draw_disc (steps, ra, rb, za, True, wire_p);  /* top plate */
          polys += 
            draw_disc (steps, ra, rb, zb, False, wire_p); /* bottom plate */
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
      GLfloat zb = g->coax_thickness/2 + g->coax_displacement + cap_height;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);

      if (wire_p && !wire_all_p) steps /= 2;

      polys += 
        draw_ring (steps, ra, za, zb, False, wire_p);  /* ring facing out */

      if (!wire_p || wire_all_p)
        {
          polys += 
            draw_disc (steps, 0,  ra, za, True, wire_p);  /* top plate */
          polys
            += draw_disc (steps, 0,  ra, zb, False, wire_p); /* bottom plate */
        }
    }
  return polys;
}


/* gear_teeth_geometry computes the vertices and normals of the teeth
   of a gear.  This is the heavy lifting: there are a ton of polygons
   around the perimiter of a gear, and the normals are difficult (not
   radial or right angles.)

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

typedef struct {
  int npoints;
  XYZ *points;
  XYZ *fnormals;  /* face normals */
  XYZ *pnormals;  /* point normals */
} tooth_face;


static void
tooth_normals (tooth_face *f, GLfloat tooth_slope)
{
  int i;

  /* Compute the face normals for each facet. */
  for (i = 0; i < f->npoints; i++)
    {
      XYZ p1, p2, p3;
      int a = i;
      int b = (i == f->npoints-1 ? 0 : i+1);
      p1 = f->points[a];
      p2 = f->points[b];
      p3 = p1;
      p3.x -= (p3.x * tooth_slope);
      p3.y -= (p3.y * tooth_slope);
      p3.z++;
      f->fnormals[i] = calc_normal (p1, p2, p3);
    }

  /* From the face normals, compute the vertex normals
     (by averaging the normals of adjascent faces.)
   */
  for (i = 0; i < f->npoints; i++)
    {
      int a = (i == 0 ? f->npoints-1 : i-1);
      int b = i;
      XYZ n1 = f->fnormals[a];   /* normal of [i-1 - i] face */
      XYZ n2 = f->fnormals[b];   /* normal of [i - i+1] face */
      f->pnormals[i].x = (n1.x + n2.x) / 2;
      f->pnormals[i].y = (n1.y + n2.y) / 2;
      f->pnormals[i].z = (n1.z + n2.z) / 2;
    }
}


static void
gear_teeth_geometry (gear *g,
                     tooth_face *orim,      /* outer rim (the teeth) */
                     tooth_face *irim)      /* inner rim (the hole)  */
{
  int i;
  int ppt = 9;   /* max points per tooth */
  GLfloat width = M_PI * 2 / g->nteeth;
  GLfloat rh = g->tooth_h;
  GLfloat tw = width;

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

  GLfloat r[20];
  GLfloat th[20];
  GLfloat R = g->r;

  r[0] = R + (rh * 0.5);
  r[1] = R + (rh * 0.25);
  r[2] = R - (r[1]-R);
  r[3] = R - (r[0]-R);
  r[4] = g->inner_r;

  th[0] = -tw * (g->size == INVOLUTE_SMALL ? 0.5 : 
                 g->size == INVOLUTE_MEDIUM ? 0.41 : 0.45);
  th[1] = -tw * 0.30;
  th[2] = -tw * (g->nteeth >= 5 ? 0.16 : 0.12);
  th[3] = -tw * (g->size == INVOLUTE_MEDIUM ? 0.1 : 0.04);
  th[4] =  0;
  th[5] =  -th[3];
  th[6] =  -th[2];
  th[7] =  -th[1];
  th[8] =  -th[0];
  th[9] =  width / 2;
  th[10]=  th[0] + width;

  if (g->inverted_p)   /* put the teeth on the inside */
    {
      for (i = 0; i < countof(th); i++)
        th[i] = -th[i];
      for (i = 0; i < countof(r); i++)
        r[i] = R - (r[i] - R);
    }

#if 0
  if (g->inverted_p)   /* put the teeth on the inside */
    {
      GLfloat swap[countof(th)];

      for (i = 0; i < maxr; i++)
        swap[maxr-i-1] = R - (r[i] - R);
      for (i = 0; i < maxr; i++)
        r[i] = swap[i];

      for (i = 0; i < maxth; i++)
        swap[maxth-i-1] = -th[i];
      for (i = 0; i < maxth; i++)
        th[i] = swap[i];
    }
#endif

  orim->npoints  = 0;
  orim->points   = (XYZ *) calloc(ppt * g->nteeth+1, sizeof(*orim->points));
  orim->fnormals = (XYZ *) calloc(ppt * g->nteeth+1, sizeof(*orim->fnormals));
  orim->pnormals = (XYZ *) calloc(ppt * g->nteeth+1, sizeof(*orim->pnormals));

  irim->npoints  = 0;
  irim->points   = (XYZ *) calloc(ppt * g->nteeth+1, sizeof(*irim->points));
  irim->fnormals = (XYZ *) calloc(ppt * g->nteeth+1, sizeof(*irim->fnormals));
  irim->pnormals = (XYZ *) calloc(ppt * g->nteeth+1, sizeof(*irim->pnormals));

  if (!orim->points || !orim->pnormals || !orim->fnormals ||
      !irim->points || !irim->pnormals || !irim->fnormals)
    {
      fprintf (stderr, "%s: out of memory\n", progname);
      exit (1);
    }

  /* First, compute the coordinates of every point used for every tooth.
   */
  for (i = 0; i < g->nteeth; i++)
    {
      GLfloat TH = (i * width) + (width/4);
      int oon = orim->npoints;
      int oin = irim->npoints;

#     undef PUSH
#     define PUSH(OPR,IPR,PTH) \
        orim->points[orim->npoints].x = cos(TH+th[(PTH)]) * r[(OPR)]; \
        orim->points[orim->npoints].y = sin(TH+th[(PTH)]) * r[(OPR)]; \
        orim->npoints++; \
        irim->points[irim->npoints].x = cos(TH+th[(PTH)]) * r[(IPR)]; \
        irim->points[irim->npoints].y = sin(TH+th[(PTH)]) * r[(IPR)]; \
        irim->npoints++

      if (g->size == INVOLUTE_SMALL)
        {
          PUSH(3, 4, 0);       /* tooth left 1 */
          PUSH(0, 4, 4);       /* tooth top middle */
        }
      else if (g->size == INVOLUTE_MEDIUM)
        {
          PUSH(3, 4, 0);       /* tooth left 1 */
          PUSH(0, 4, 3);       /* tooth top left */
          PUSH(0, 4, 5);       /* tooth top right */
          PUSH(3, 4, 8);       /* tooth right 3 */
        }
      else if (g->size == INVOLUTE_LARGE)
        {
          PUSH(3, 4, 0);       /* tooth left 1 */
          PUSH(2, 4, 1);       /* tooth left 2 */
          PUSH(1, 4, 2);       /* tooth left 3 */
          PUSH(0, 4, 3);       /* tooth top left */
          PUSH(0, 4, 5);       /* tooth top right */
          PUSH(1, 4, 6);       /* tooth right 1 */
          PUSH(2, 4, 7);       /* tooth right 2 */
          PUSH(3, 4, 8);       /* tooth right 3 */
          PUSH(3, 4, 9);       /* gap top */
        }
      else
        abort();
#     undef PUSH

      if (i == 0 && orim->npoints > ppt) abort();  /* go update "ppt"! */

      if (g->inverted_p)
        {
          int start, end, j;
          start = oon;
          end = orim->npoints;
          for (j = 0; j < (end-start)/2; j++)
            {
              XYZ swap = orim->points[end-j-1];
              orim->points[end-j-1] = orim->points[start+j];
              orim->points[start+j] = swap;
            }

          start = oin;
          end = irim->npoints;
          for (j = 0; j < (end-start)/2; j++)
            {
              XYZ swap = irim->points[end-j-1];
              irim->points[end-j-1] = irim->points[start+j];
              irim->points[start+j] = swap;
            }
        }
    }

  tooth_normals (orim, g->tooth_slope);
  tooth_normals (irim, 0);

  if (g->inverted_p)   /* flip the normals */
    {
      for (i = 0; i < orim->npoints; i++)
        {
          orim->fnormals[i].x = -orim->fnormals[i].x;
          orim->fnormals[i].y = -orim->fnormals[i].y;
          orim->fnormals[i].z = -orim->fnormals[i].z;

          orim->pnormals[i].x = -orim->pnormals[i].x;
          orim->pnormals[i].y = -orim->pnormals[i].y;
          orim->pnormals[i].z = -orim->pnormals[i].z;
        }

      for (i = 0; i < irim->npoints; i++)
        {
          irim->fnormals[i].x = -irim->fnormals[i].x;
          irim->fnormals[i].y = -irim->fnormals[i].y;
          irim->fnormals[i].z = -irim->fnormals[i].z;

          irim->pnormals[i].x = -irim->pnormals[i].x;
          irim->pnormals[i].y = -irim->pnormals[i].y;
          irim->pnormals[i].z = -irim->pnormals[i].z;
        }
    }
}


/* Which of the gear's inside rings is the biggest? 
 */
int
involute_biggest_ring (gear *g, double *posP, double *sizeP, double *heightP)
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


/* Renders all teeth of a gear.
 */
static int
draw_gear_teeth (gear *g, Bool wire_p)
{
  int polys = 0;
  int i;

  GLfloat z1 = -g->thickness/2;
  GLfloat z2 =  g->thickness/2;
  GLfloat s1 = 1 + (g->thickness * g->tooth_slope / 2);
  GLfloat s2 = 1 - (g->thickness * g->tooth_slope / 2);

  tooth_face orim, irim;
  gear_teeth_geometry (g, &orim, &irim);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);

  /* Draw the outer rim (the teeth)
     (In wire mode, this draws just the upright lines.)
   */
  glFrontFace (g->inverted_p ? GL_CCW : GL_CW);
  glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
  for (i = 0; i < orim.npoints; i++)
    {
      glNormal3f (orim.pnormals[i].x, orim.pnormals[i].y, orim.pnormals[i].z);
      glVertex3f (s1*orim.points[i].x, s1*orim.points[i].y, z1);
      glVertex3f (s2*orim.points[i].x, s2*orim.points[i].y, z2);

      /* Show the face normal vectors */
      if (wire_p && show_normals_p)
        {
          XYZ n = orim.fnormals[i];
          int a = i;
          int b = (i == orim.npoints-1 ? 0 : i+1);
          GLfloat x = (orim.points[a].x + orim.points[b].x) / 2;
          GLfloat y = (orim.points[a].y + orim.points[b].y) / 2;
          GLfloat z = (z1 + z2) / 2;
          glVertex3f (x, y, z);
          glVertex3f (x + n.x, y + n.y, z + n.z);
        }

      /* Show the vertex normal vectors */
      if (wire_p && show_normals_p)
        {
          XYZ n = orim.pnormals[i];
          GLfloat x = orim.points[i].x;
          GLfloat y = orim.points[i].y;
          GLfloat z = (z1 + z2) / 2;
          glVertex3f (x, y, z);
          glVertex3f (x + n.x, y + n.y, z + n.z);
        }
    }

  if (!wire_p)  /* close the quad loop */
    {
      glNormal3f (orim.pnormals[0].x, orim.pnormals[0].y, orim.pnormals[0].z);
      glVertex3f (s1*orim.points[0].x, s1*orim.points[0].y, z1);
      glVertex3f (s2*orim.points[0].x, s2*orim.points[0].y, z2);
    }
  polys += orim.npoints;
  glEnd();

  /* Draw the outer rim circles, in wire mode */
  if (wire_p)
    {
      glBegin (GL_LINE_LOOP);
      for (i = 0; i < orim.npoints; i++)
        glVertex3f (s1*orim.points[i].x, s1*orim.points[i].y, z1);
      glEnd();
      glBegin (GL_LINE_LOOP);
      for (i = 0; i < orim.npoints; i++)
        glVertex3f (s2*orim.points[i].x, s2*orim.points[i].y, z2);
      glEnd();
    }

  /* Draw the inner rim (the hole)
     (In wire mode, this draws just the upright lines.)
   */
  glFrontFace (g->inverted_p ? GL_CW : GL_CCW);
  glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
  for (i = 0; i < irim.npoints; i++)
    {
      glNormal3f(-irim.pnormals[i].x, -irim.pnormals[i].y,-irim.pnormals[i].z);
      glVertex3f (irim.points[i].x, irim.points[i].y, z1);
      glVertex3f (irim.points[i].x, irim.points[i].y, z2);

      /* Show the face normal vectors */
      if (wire_p && show_normals_p)
        {
          XYZ n = irim.fnormals[i];
          int a = i;
          int b = (i == irim.npoints-1 ? 0 : i+1);
          GLfloat x = (irim.points[a].x + irim.points[b].x) / 2;
          GLfloat y = (irim.points[a].y + irim.points[b].y) / 2;
          GLfloat z  = (z1 + z2) / 2;
          glVertex3f (x, y, z);
          glVertex3f (x - n.x, y - n.y, z);
        }

      /* Show the vertex normal vectors */
      if (wire_p && show_normals_p)
        {
          XYZ n = irim.pnormals[i];
          GLfloat x = irim.points[i].x;
          GLfloat y = irim.points[i].y;
          GLfloat z = (z1 + z2) / 2;
          glVertex3f (x, y, z);
          glVertex3f (x - n.x, y - n.y, z);
        }
    }

  if (!wire_p)  /* close the quad loop */
    {
      glNormal3f (-irim.pnormals[0].x,-irim.pnormals[0].y,-irim.pnormals[0].z);
      glVertex3f (irim.points[0].x, irim.points[0].y, z1);
      glVertex3f (irim.points[0].x, irim.points[0].y, z2);
    }
  polys += irim.npoints;
  glEnd();

  /* Draw the inner rim circles, in wire mode
   */
  if (wire_p)
    {
      glBegin (GL_LINE_LOOP);
      for (i = 0; i < irim.npoints; i++)
        glVertex3f (irim.points[i].x, irim.points[i].y, z1);
      glEnd();
      glBegin (GL_LINE_LOOP);
      for (i = 0; i < irim.npoints; i++)
        glVertex3f (irim.points[i].x, irim.points[i].y, z2);
      glEnd();
    }

  /* Draw the side (the flat bit)
   */
  if (!wire_p || wire_all_p)
    {
      GLfloat z;
      if (irim.npoints != orim.npoints) abort();
      for (z = z1; z <= z2; z += z2-z1)
        {
          GLfloat s = (z == z1 ? s1 : s2);
          glFrontFace (((z == z1) ^ g->inverted_p) ? GL_CCW : GL_CW);
          glNormal3f (0, 0, z);
          glBegin (wire_p ? GL_LINES : GL_QUAD_STRIP);
          for (i = 0; i < orim.npoints; i++)
            {
              glVertex3f (s*orim.points[i].x, s*orim.points[i].y, z);
              glVertex3f (  irim.points[i].x,   irim.points[i].y, z);
            }
          if (!wire_p)  /* close the quad loop */
            {
              glVertex3f (s*orim.points[0].x, s*orim.points[0].y, z);
              glVertex3f (  irim.points[0].x,   irim.points[0].y, z);
            }
          polys += orim.npoints;
          glEnd();
        }
    }

  free (irim.points);
  free (irim.fnormals);
  free (irim.pnormals);

  free (orim.points);
  free (orim.fnormals);
  free (orim.pnormals);

  return polys;
}


/* Render one gear, unrotated at 0,0.
   Returns the number of polygons.
 */
int
draw_involute_gear (gear *g, Bool wire_p)
{
  int polys = 0;

  static const GLfloat spec[4] = {1.0, 1.0, 1.0, 1.0};
  GLfloat shiny   = 128.0;

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, g->color);
  glColor3f (g->color[0], g->color[1], g->color[2]);

  if (wire_p > 1)
    polys += draw_involute_schematic (g, wire_p);
  else
    {
      glPushMatrix();
      glRotatef (g->wobble, 1, 0, 0);
      polys += draw_gear_teeth (g, wire_p);
      polys += draw_gear_interior (g, wire_p);
      polys += draw_gear_nubs (g, wire_p);
      glPopMatrix();
    }
  return polys;
}
