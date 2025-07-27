/* hextrail, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
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
            "*count:        20          \n" \
            "*suppressRotationAnimation: True\n" \

# define release_hextrail 0

#ifndef WEB_BUILD
#include "xlockmore.h"
#endif
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

// Rotator constants
static const double SPIN_SPEED = 0.002;
static const double WANDER_SPEED = 0.003;
static const double SPIN_ACCEL = 1.0;

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.15"

typedef enum { EMPTY, IN, WAIT, OUT, DONE } state_t;

typedef struct {
  state_t state;
  GLfloat ratio;
  GLfloat speed;
} arm;

typedef struct hexagon hexagon;
struct hexagon {
  XYZ pos;
  hexagon *neighbors[6];
  arm arms[6];
  int ccolor;
  state_t border_state;
  GLfloat border_ratio;
  int invis;
};

typedef struct {
  GLXContext *glx_context;
  GLdouble model[16], proj[16];
  GLint viewport[4];
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int grid_w, grid_h;
  hexagon *hexagons;
  int live_count, size;
  enum { FIRST, DRAW, FADE } state;
  GLfloat fade_ratio;

  int ncolors;
  XColor *colors;
} hextrail_configuration;

static hextrail_configuration *bps = NULL;

#ifdef WEB_BUILD
// Make these accessible to web wrapper
Bool do_spin, do_wander;
GLfloat speed, thickness;
#else
static Bool do_spin, do_wander;
static GLfloat speed, thickness;
#endif
static int draw_invis = 1;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
};

ENTRYPOINT ModeSpecOpt hextrail_opts = {countof(opts), opts, countof(vars), vars, NULL};

static void
make_plane (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;
  GLfloat size, w, h;
  hexagon *grid;

  bp->grid_w = MI_COUNT(mi) * 2;
  bp->grid_h = bp->grid_w;

  grid = (bp->hexagons
          ? bp->hexagons
          : (hexagon *) malloc (bp->grid_w * bp->grid_h * sizeof(*grid)));
  memset (grid, 0, bp->grid_w * bp->grid_h * sizeof(*grid));

  bp->ncolors = 8;
  if (! bp->colors)
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  size = 2.0 / bp->grid_w;
  w = size;
  h = size * sqrt(3) / 2;

  bp->hexagons = grid;

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++)
      {
        hexagon *h0 = &grid[y * bp->grid_w + x];
        h0->pos.x = (x - bp->grid_w/2) * w;
        h0->pos.y = (y - bp->grid_h/2) * h;
        h0->border_state = EMPTY;
        h0->border_ratio = 0;

        if (y & 1)
          h0->pos.x += w / 2;

        h0->ccolor = random() % bp->ncolors;
      }

  // Debug: Print the generated colors
  printf("hextrail: Generated %d colors. Start color=%d\n", bp->ncolors,
          grid[bp->grid_h * bp->grid_w / 2 + bp->grid_w / 2].ccolor);
  for (int i = 0; i < bp->ncolors; i++) {
    printf("  Color %d: RGB: %d,%d,%d\n",
           i, bp->colors[i].red >> 8, bp->colors[i].green >> 8, bp->colors[i].blue >> 8);
  }

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++)
      {
        hexagon *h0 = &grid[y * bp->grid_w + x];
# undef NEIGHBOR
# define NEIGHBOR(I,XE,XO,Y) do {                                        \
          int x1 = x + (y & 1 ? (XO) : (XE)); int y1 = y + (Y);          \
          if (x1 >= 0 && x1 < bp->grid_w && y1 >= 0 && y1 < bp->grid_h)  \
            h0->neighbors[(I)] = &grid [y1 * bp->grid_w + x1];           \
        } while (0)

        /*   0,0   1,0   2,0   3,0   4,0   5,0
                0,1   1,1   2,1   3,1   4,1   5,1
             0,2   1,2   2,2   3,2   4,2   5,2
                0,3   1,3   2,3   3,3   4,3   5,3
             0,4   1,4   2,4   3,4   4,4   5,4
                0,5   1,5   2,5   3,5   4,5   5,5
         */
        NEIGHBOR (0,  0,  1, -1);
        NEIGHBOR (1,  1,  1,  0);
        NEIGHBOR (2,  0,  1,  1);
        NEIGHBOR (3, -1,  0,  1);
        NEIGHBOR (4, -1, -1,  0);
        NEIGHBOR (5, -1,  0, -1);

# undef NEIGHBOR
      }
}

static int
add_arms (ModeInfo *mi, hexagon *h0)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  int added = 0;
  int target = 1 + (random() % 4);	/* Aim for 1-4 arms */

  int idx[6] = {0, 1, 2, 3, 4, 5};	/* Traverse in random order */
  for (i = 0; i < 6; i++)
    {
      int j = random() % 6;
      int swap = idx[j];
      idx[j] = idx[i];
      idx[i] = swap;
    }

  for (i = 0; i < 6; i++)
    {
      int j = idx[i];
      hexagon *h1 = h0->neighbors[j];
      arm *a0 = &h0->arms[j];
      arm *a1;
      if (!h1) continue;			/* No neighboring cell */
      if (h1->border_state != EMPTY) continue;	/* Occupado */
      if (a0->state != EMPTY) continue;		/* Incoming arm */

      a1 = &h1->arms[(j + 3) % 6];		/* Opposite arm */

      if (a1->state != EMPTY) {
        printf("DEBUG: a1->state is %d, expected EMPTY\n", a1->state);
        abort();
      }
      a0->state = OUT;
      a0->ratio = 0;
      a1->ratio = 0;
      a0->speed = 0.05 * speed * (0.8 + frand(1.0));
      a1->speed = a0->speed;

      h1->border_state = IN;

      /* Mostly keep the same color */
      h1->ccolor = h0->ccolor;
      if (! (random() % 5))
        h1->ccolor = (h0->ccolor + 1) % bp->ncolors;

      bp->live_count++;
      added++;
      if (added >= target)
        break;
    }
  return added;
}

# define H 0.8660254037844386   /* sqrt(3)/2 */

static const XYZ corners[] = {{  0, -1,   0 },       /*      0      */
                              {  H, -0.5, 0 },       /*  5       1  */
                              {  H,  0.5, 0 },       /*             */
                              {  0,  1,   0 },       /*  4       2  */
                              { -H,  0.5, 0 },       /*      3      */
                              { -H, -0.5, 0 }};

static XYZ scaled_corners[6][4];

static void scale_corners(ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat size = H * 2 / 3 / MI_COUNT(mi);
  GLfloat margin = thickness * 0.4;
  GLfloat size1 = size * (1 - margin * 2);
  GLfloat size2 = size * (1 - margin * 3);
  GLfloat thick2 = thickness * bp->fade_ratio;
  GLfloat size3 = size * thick2 * 0.8;
  GLfloat size4 = size3 * 2; // when total_arms == 1

  int i;
  for (i = 0; i < 6; i++) {
    scaled_corners[i][0].x = corners[i].x * size1;
    scaled_corners[i][0].y = corners[i].y * size1;
    scaled_corners[i][1].x = corners[i].x * size2;
    scaled_corners[i][1].y = corners[i].y * size2;
    scaled_corners[i][2].x = corners[i].x * size3;
    scaled_corners[i][2].y = corners[i].y * size3;
    scaled_corners[i][3].x = corners[i].x * size4;
    scaled_corners[i][3].y = corners[i].y * size4;
  }
}

static time_t now = 0;

static int hex_invis(hextrail_configuration *bp, XYZ pos, int i, GLfloat *rad) {
  GLdouble x, y, z;
  /* Project point to screen coordinates */
  gluProject(pos.x, pos.y, pos.z, bp->model, bp->proj, bp->viewport, &x, &y, &z);

  /*static time_t debug = 0;
  if (debug != now) {
      printf("%s: pos=(%f,%f,%f) x=%f, y=%f, z=%.1f vp=%d,%d i=%d\n", __func__,
              pos.x, pos.y, pos.z, x, y, z, bp->viewport[2], bp->viewport[3], i);
      debug = now;
  }*/

  if (z <= 0 || z >= 1) return 2;

  XYZ edge_posx = pos, edge_posy = pos;
  GLfloat wid = 2.0 / bp->size, hgt = wid * H;
  edge_posx.x += wid / 2;
  edge_posy.y += hgt / 2;
  GLdouble edge_xx, edge_xy, edge_yx, edge_yy, edge_z;
  gluProject(edge_posx.x, edge_posx.y, edge_posx.z, bp->model, bp->proj, bp->viewport, &edge_xx, &edge_xy, &edge_z);
  gluProject(edge_posy.x, edge_posy.y, edge_posy.z, bp->model, bp->proj, bp->viewport, &edge_yx, &edge_yy, &edge_z);
  GLfloat xx_diff = edge_xx - x, xy_diff = edge_xy - y;
  GLfloat yx_diff = edge_yx - x, yy_diff = edge_yy - y;
  GLdouble radiusx = sqrt(xx_diff * xx_diff + xy_diff * xy_diff);
  GLdouble radiusy = sqrt(yx_diff * yx_diff + yy_diff * yy_diff);
  GLdouble radius = radiusx > radiusy ? radiusx : radiusy;
  if (rad) *rad = radius;

  if (x + radius < bp->viewport[0] || x - radius > bp->viewport[0] + bp->viewport[2] ||
      y + radius < bp->viewport[1] || y - radius > bp->viewport[1] + bp->viewport[3])
    return 2; // Fully offscreen

  if (x < bp->viewport[0] || x > bp->viewport[0] + bp->viewport[2] ||
      y < bp->viewport[1] || y > bp->viewport[1] + bp->viewport[3])
    return 1; // Center is offscreen

  return 0; // Center is onscreen
}

static void
tick_hexagons (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;
  static unsigned int ticks = 0;
  now = time(NULL);

  /* Enlarge any still-growing arms.
   */
  ticks++;
  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    {
      hexagon *h0 = &bp->hexagons[i];
      if (! (ticks % 4)) h0->invis = hex_invis(bp, h0->pos, i, 0);

      for (j = 0; j < 6; j++)
        {
          arm *a0 = &h0->arms[j];
          switch (a0->state) {
          case OUT:
            if (a0->speed <= 0) {
              printf("DEBUG: OUT arm speed is %f, expected > 0\n", a0->speed);
              abort();
            }
            a0->ratio += a0->speed * speed;
            if (a0->ratio > 1)
              {
                /* Just finished growing from center to edge.
                   Pass the baton to this waiting neighbor. */
                hexagon *h1 = h0->neighbors[j];
                arm *a1 = &h1->arms[(j + 3) % 6];
                a0->state = DONE;
                a0->ratio = 1;
                a1->state = IN;
                a1->ratio = 0;
                a1->speed = a0->speed;
                /* bp->live_count unchanged */
              } else if (a0->ratio <= 0) {
                a0->state = EMPTY;
                a0->ratio = 0;
                bp->live_count--;
                if (bp->live_count < 0) abort();
              }
            break;
          case IN:
            if (a0->speed <= 0) {
              printf("DEBUG: IN arm speed is %f, expected > 0\n", a0->speed);
              abort();
            }
            a0->ratio += a0->speed * speed;
            if (a0->ratio > 1)
              {
                /* Just finished growing from edge to center.
                   Look for any available exits. */
                if (add_arms (mi, h0)) {
                  bp->live_count--;
                  if (bp->live_count < 0) abort();
                  a0->state = DONE;
                  a0->ratio = 1;
                } else { // nub grow
                  a0->state = WAIT;
                  a0->ratio = ((a0->ratio - 1) * 5) + 1; a0->speed *= 5;
                }
              }
            break;
          case WAIT:
            a0->ratio += a0->speed * speed * (2 - a0->ratio);
            if (a0->ratio >= 1.999) {
              a0->state = DONE;
              a0->ratio = 1;
              bp->live_count--;
              if (bp->live_count < 0) abort();
            }
          case EMPTY: case DONE:
            break;
          default:
            printf("DEBUG: Invalid arm state: %d\n", a0->state);
            abort(); break;
          }
        }

      switch (h0->border_state) {
      case IN:
        h0->border_ratio += 0.05 * speed;
        if (h0->border_ratio >= 1)
          {
            h0->border_ratio = 1;
            h0->border_state = WAIT;
          }
        break;
      case OUT:
        h0->border_ratio -= 0.05 * speed;
        if (h0->border_ratio <= 0)
          {
            h0->border_ratio = 0;
            h0->border_state = DONE;
          }
      case WAIT:
        if (! (random() % (int)(50.0/speed)))
          h0->border_state = OUT;
        break;
      case EMPTY:
      case DONE:
        break;
      default:
        printf("DEBUG: Invalid border_state: %d\n", h0->border_state);
        abort();
        break;
      }
    }

  /* Start a new cell growing.
   */
  if (bp->live_count <= 0)
    for (i = 0; i < (bp->grid_w * bp->grid_h) / 3; i++)
      {
        hexagon *h0;
        int x, y;
        if (bp->state == FIRST)
          {
            x = bp->grid_w / 2;
            y = bp->grid_h / 2;
            bp->state = DRAW;
            bp->fade_ratio = 1;
            scale_corners(mi);
          }
        else
          {
            x = random() % bp->grid_w;
            y = random() % bp->grid_h;
          }
        h0 = &bp->hexagons[y * bp->grid_w + x];
        if (h0->border_state == EMPTY &&
            add_arms (mi, h0)) {
          h0->border_state = DONE;
          break;
        }
      }

  if (bp->live_count <= 0 && bp->state != FADE)
    {
      bp->state = FADE;
      bp->fade_ratio = 1;

      for (i = 0; i < bp->grid_w * bp->grid_h; i++)
        {
          hexagon *h = &bp->hexagons[i];
          if (h->border_state == IN || h->border_state == WAIT)
            h->border_state = OUT;
        }
    }
  else if (bp->state == FADE)
    {
      bp->fade_ratio -= 0.01 * speed;
      scale_corners(mi);
      if (bp->fade_ratio <= 0)
        {
          make_plane (mi);
          bp->state = FIRST;
          bp->fade_ratio = 1;
        }
    }
}

static void
draw_hexagons (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat length = sqrt(3) / 3;
  GLfloat size = length / MI_COUNT(mi);
  GLfloat margin = thickness * 0.4;
  GLfloat size2 = size * (1 - margin * 3);
  GLfloat thick2 = thickness * bp->fade_ratio;
  int i;

  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_TRIANGLES);
  glNormal3f (0, 0, 1);

  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    {
      hexagon *h = &bp->hexagons[i];
      if (draw_invis < h->invis) continue;
      int total_arms = 0;
      GLfloat color[4];
      GLfloat nub_ratio = 0;
      int j;

      for (j = 0; j < 6; j++)
        {
          arm *a = &h->arms[j];
          if (a->state == OUT || a->state == DONE || a->state == WAIT) {
            total_arms++;
            if (a->state == WAIT)
              nub_ratio = a->ratio;
          }
        }


# define HEXAGON_COLOR(V,H) do { \
          (V)[0] = bp->colors[(H)->ccolor].red   / 65535.0 * bp->fade_ratio; \
          (V)[1] = bp->colors[(H)->ccolor].green / 65535.0 * bp->fade_ratio; \
          (V)[2] = bp->colors[(H)->ccolor].blue  / 65535.0 * bp->fade_ratio; \
          (V)[3] = 1; \
        } while (0)
      HEXAGON_COLOR (color, h);

      for (j = 0; j < 6; j++)
        {
          arm *a = &h->arms[j];
          int k = (j + 1) % 6;
          XYZ p[6];

          if (h->border_state != EMPTY && h->border_state != DONE)
            {
              GLfloat color1[4];
              memcpy (color1, color, sizeof(color1));
              color1[0] *= h->border_ratio;
              color1[1] *= h->border_ratio;
              color1[2] *= h->border_ratio;

              glColor4fv (color1);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);

              /* Outer edge of hexagon border */
              p[0].x = h->pos.x + scaled_corners[j][0].x;
              p[0].y = h->pos.y + scaled_corners[j][0].y;
              p[0].z = h->pos.z;

              p[1].x = h->pos.x + scaled_corners[k][0].x;
              p[1].y = h->pos.y + scaled_corners[k][0].y;
              p[1].z = h->pos.z;

              /* Inner edge of hexagon border */
              p[2].x = h->pos.x + scaled_corners[k][1].x;
              p[2].y = h->pos.y + scaled_corners[k][1].y;
              p[2].z = h->pos.z;
              p[3].x = h->pos.x + scaled_corners[j][1].x;
              p[3].y = h->pos.y + scaled_corners[j][1].y;
              p[3].z = h->pos.z;

              glVertex3f (p[0].x, p[0].y, p[0].z);
              glVertex3f (p[1].x, p[1].y, p[1].z);
              if (! wire)
                glVertex3f (p[2].x, p[2].y, p[2].z);
              mi->polygon_count++;

              glVertex3f (p[2].x, p[2].y, p[2].z);
              glVertex3f (p[3].x, p[3].y, p[3].z);
              if (! wire)
                glVertex3f (p[0].x, p[0].y, p[0].z);
              mi->polygon_count++;
            }

          /* Line from center to edge, or edge to center.
           */
          if (a->state == IN || a->state == OUT || a->state == DONE || a->state == WAIT)
            {
              GLfloat x   = (corners[j].x + corners[k].x) / 2;
              GLfloat y   = (corners[j].y + corners[k].y) / 2;
              GLfloat xoff = corners[k].x - corners[j].x;
              GLfloat yoff = corners[k].y - corners[j].y;
              GLfloat line_length = (a->state == WAIT) ? 1 : a->ratio;
              GLfloat start, end;
              GLfloat ncolor[4];
              GLfloat color1[4];
              GLfloat color2[4];

              /* Color of the outer point of the line is average color of
                 this and the neighbor. */
              HEXAGON_COLOR (ncolor, h->neighbors[j]);
              ncolor[0] = (ncolor[0] + color[0]) / 2;
              ncolor[1] = (ncolor[1] + color[1]) / 2;
              ncolor[2] = (ncolor[2] + color[2]) / 2;
              ncolor[3] = (ncolor[3] + color[3]) / 2;

              if (a->state == OUT)
                {
                  start = 0;
                  end = size * line_length;
                  memcpy (color1, color,  sizeof(color1));
                  memcpy (color2, ncolor, sizeof(color1));
                }
              else
                {
                  start = size;
                  end = size * (1 - line_length);
                  memcpy (color1, ncolor, sizeof(color1));
                  memcpy (color2, color,  sizeof(color1));
                }

              if (! h->neighbors[j]) abort();  /* arm/neighbor mismatch */

              /* Center */
              p[0].x = h->pos.x + xoff * size2 * thick2 + x * start;
              p[0].y = h->pos.y + yoff * size2 * thick2 + y * start;
              p[0].z = h->pos.z;
              p[1].x = h->pos.x - xoff * size2 * thick2 + x * start;
              p[1].y = h->pos.y - yoff * size2 * thick2 + y * start;
              p[1].z = h->pos.z;

              /* Edge */
              p[2].x = h->pos.x - xoff * size2 * thick2 + x * end;
              p[2].y = h->pos.y - yoff * size2 * thick2 + y * end;
              p[2].z = h->pos.z;
              p[3].x = h->pos.x + xoff * size2 * thick2 + x * end;
              p[3].y = h->pos.y + yoff * size2 * thick2 + y * end;
              p[3].z = h->pos.z;

              glColor4fv (color2);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
              glVertex3f (p[3].x, p[3].y, p[3].z);
              glColor4fv (color1);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
              glVertex3f (p[0].x, p[0].y, p[0].z);
              if (! wire)
                glVertex3f (p[1].x, p[1].y, p[1].z);
              mi->polygon_count++;

              glVertex3f (p[1].x, p[1].y, p[1].z);

              glColor4fv (color2);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
              glVertex3f (p[2].x, p[2].y, p[2].z);
              if (! wire)
                glVertex3f (p[3].x, p[3].y, p[3].z);
              mi->polygon_count++;
            }

          /* Hexagon (one triangle of) in center to hide line miter/bevels.
           */
          if (total_arms && a->state != DONE && a->state != OUT)
            {
              p[0] = h->pos; p[1].z = h->pos.z; p[2].z = h->pos.z;

              if (nub_ratio) {
                p[1].x = h->pos.x + scaled_corners[j][2].x * nub_ratio;
                p[1].y = h->pos.y + scaled_corners[j][2].y * nub_ratio;
                p[2].x = h->pos.x + scaled_corners[k][2].x * nub_ratio;
                p[2].y = h->pos.y + scaled_corners[k][2].y * nub_ratio;
              } else {
                int8_t s = (total_arms == 1) ? 3 : 2;
                p[1].x = h->pos.x + scaled_corners[j][s].x;
                p[1].y = h->pos.y + scaled_corners[j][s].y;
                /* Inner edge of hexagon border */
                p[2].x = h->pos.x + scaled_corners[k][s].x;
                p[2].y = h->pos.y + scaled_corners[k][s].y;
              }

              glColor4fv (color);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
              if (! wire)
                glVertex3f (p[0].x, p[0].y, p[0].z);
              glVertex3f (p[1].x, p[1].y, p[1].z);
              glVertex3f (p[2].x, p[2].y, p[2].z);
              mi->polygon_count++;
            }
        }
    }

  glEnd();
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_hextrail (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 3) {   /* tiny window: show middle */
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
hextrail_handle_event (ModeInfo *mi, XEvent *event)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

  // Handle web parameter change events (NULL event means parameters changed)
  if (event == NULL) {
    printf("DEBUG: hextrail_handle_event - NULL event (parameter change)\n");
    scale_corners(mi);  // Recalculate corners when thickness changes
    return True;
  }

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ;
      else if (c == '>' || c == '.' || c == '+' || c == '=' ||
               keysym == XK_Up || keysym == XK_Next) {
          printf("DEBUG: Increasing count from %ld to %ld\n", MI_COUNT(mi), MI_COUNT(mi) + 1);
          MI_COUNT(mi)++;
          bp->size = MI_COUNT(mi) * 2;
          scale_corners(mi);
      } else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' || keysym == XK_Down || keysym == XK_Prior) {
          printf("DEBUG: Decreasing count from %ld to %ld\n", MI_COUNT(mi), MI_COUNT(mi) - 1);
          MI_COUNT(mi)--;
          if (MI_COUNT(mi) < 1) {
            printf("DEBUG: Count was %ld, resetting to 1\n", MI_COUNT(mi));
            MI_COUNT(mi) = 1;
          }
          bp->size = MI_COUNT(mi) * 2;
          scale_corners(mi);
      } else if (keysym == XK_Right) {
          printf("DEBUG: Increasing speed from %f to %f\n", speed, speed * 2);
          speed *= 2;
          if (speed > 20) speed = 20;
      } else if (keysym == XK_Left) {
          printf("DEBUG: Decreasing speed from %f to %f\n", speed, speed / 2);
          speed /= 2;
          if (speed < 0.0001) speed = 0.0001;
      } else if (c == 'i') {
          draw_invis = (draw_invis - 1) % 4;
          printf("DEBUG: draw_invis = %d\n", draw_invis);
      } else if (c == 'I') {
          draw_invis = (draw_invis + 1) % 4;
          printf("DEBUG: draw_invis = %d\n", draw_invis);
      } else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event)) {
          printf("DEBUG: Event handled by screenhack_event_helper\n");
      } else {
          printf("DEBUG: Unhandled key character '%c' (code=%d)\n", c, (int)c);
          return False;
      }
    } // KeyPress
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    return True;

  return False;
}


// Function to create rotator with current spin/wander settings
static rotator* create_hextrail_rotator(void) {
#ifdef WEB_BUILD
    printf("DEBUG: Creating rotator - do_spin=%d, do_wander=%d\n", do_spin, do_wander);
#endif

    return make_rotator(do_spin ? SPIN_SPEED : 0,
                       do_spin ? SPIN_SPEED : 0,
                       do_spin ? SPIN_SPEED : 0,
                       SPIN_ACCEL,
                       do_wander ? WANDER_SPEED : 0,
                       False);
}

ENTRYPOINT void
init_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hextrail (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  /* Initialize speed from resource */
  speed = get_float_resource (MI_DISPLAY(mi), "speed", "Float");
  if (speed <= 0) speed = 1.0;

  bp->rot = create_hextrail_rotator();
  bp->trackball = gltrackball_init (True);

  /* Let's tilt the scene a little. */
  gltrackball_reset (bp->trackball,
                     -0.4 + frand(0.8),
                     -0.4 + frand(0.8));

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 0.5) thickness = 0.5;

  make_plane (mi);
  bp->state = FIRST;
  bp->fade_ratio = 1;
}


ENTRYPOINT void
draw_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 12);
    gltrackball_rotate (bp->trackball);
    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  {
    GLfloat s = 18;
    glScalef (s, s, s);
  }

  if (! bp->button_down_p){
    glGetDoublev(GL_MODELVIEW_MATRIX, bp->model);
    glGetDoublev(GL_PROJECTION_MATRIX, bp->proj);
    tick_hexagons (mi);
  }
  draw_hexagons (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->colors) free (bp->colors);
  free (bp->hexagons);
}

#ifdef WEB_BUILD
// Function to update rotator when spin/wander settings change
void update_hextrail_rotator(void) {
    extern hextrail_configuration *bps;
    extern ModeInfo web_mi;

    hextrail_configuration *bp = &bps[web_mi.screen];
    if (!bp->rot) return;

    // Free old rotator and create new one with updated settings
    if (bp->rot) {
        free_rotator(bp->rot);
    }

    bp->rot = create_hextrail_rotator();
}
#endif

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
