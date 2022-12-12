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

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.15"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

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
};

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int grid_w, grid_h;
  hexagon *hexagons;
  int live_count;
  enum { FIRST, DRAW, FADE } state;
  GLfloat fade_ratio;

  int ncolors;
  XColor *colors;

} hextrail_configuration;

static hextrail_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static GLfloat thickness;

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

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++)
      {
        hexagon *h0 = &grid[y * bp->grid_w + x];
# undef NEIGHBOR
# define NEIGHBOR(I,XE,XO,Y) do {                                        \
          int x1 = x + (y & 1 ? (XO) : (XE));                            \
          int y1 = y + (Y);                                              \
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


static Bool
empty_hexagon_p (hexagon *h)
{
  int i;
  for (i = 0; i < 6; i++)
    if (h->arms[i].state != EMPTY)
      return False;
  return True;
}

static int
add_arms (ModeInfo *mi, hexagon *h0, Bool out_p)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  int added = 0;
  int target = 1 + (random() % 4);	/* Aim for 1-5 arms */

  int idx[6];				/* Traverse in random order */
  for (i = 0; i < 6; i++)
    idx[i] = i;
  for (i = 0; i < 6; i++)
    {
      int j = random() % 6;
      int swap = idx[j];
      idx[j] = idx[i];
      idx[i] = swap;
    }

  if (out_p) target--;

  for (i = 0; i < 6; i++)
    {
      int j = idx[i];
      hexagon *h1 = h0->neighbors[j];
      arm *a0 = &h0->arms[j];
      arm *a1;
      if (!h1) continue;			/* No neighboring cell */
      if (! empty_hexagon_p (h1)) continue;	/* Occupado */
      if (a0->state != EMPTY) continue;		/* Arm already exists */

      a1 = &h1->arms[(j + 3) % 6];		/* Opposite arm */

      if (a1->state != EMPTY) abort();
      a0->state = (out_p ? OUT : IN);
      a1->state = WAIT;
      a0->ratio = 0;
      a1->ratio = 0;
      a0->speed = 0.05 * speed * (0.8 + frand(1.0));
      a1->speed = a0->speed;

      if (h1->border_state == EMPTY)
        {
          h1->border_state = IN;

          /* Mostly keep the same color */
          h1->ccolor = h0->ccolor;
          if (! (random() % 5))
            h1->ccolor = (h0->ccolor + 1) % bp->ncolors;
        }

      bp->live_count++;
      added++;
      if (added >= target)
        break;
    }
  return added;
}


static void
tick_hexagons (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;

  /* Enlarge any still-growing arms.
   */
  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    {
      hexagon *h0 = &bp->hexagons[i];
      for (j = 0; j < 6; j++)
        {
          arm *a0 = &h0->arms[j];
          switch (a0->state) {
          case OUT:
            if (a0->speed <= 0) abort();
            a0->ratio += a0->speed;
            if (a0->ratio > 1)
              {
                /* Just finished growing from center to edge.
                   Pass the baton to this waiting neighbor. */
                hexagon *h1 = h0->neighbors[j];
                arm *a1 = &h1->arms[(j + 3) % 6];
                if (a1->state != WAIT) abort();
                a0->state = DONE;
                a0->ratio = 1;
                a1->state = IN;
                a1->ratio = 0;
                a1->speed = a0->speed;
                /* bp->live_count unchanged */
              }
            break;
          case IN:
            if (a0->speed <= 0) abort();
            a0->ratio += a0->speed;
            if (a0->ratio > 1)
              {
                /* Just finished growing from edge to center.
                   Look for any available exits. */
                a0->state = DONE;
                a0->ratio = 1;
                bp->live_count--;
                if (bp->live_count < 0) abort();
                add_arms (mi, h0, True);
              }
            break;
          case EMPTY: case WAIT: case DONE:
            break;
          default:
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
            h0->border_state = EMPTY;
          }
      case WAIT:
        if (! (random() % 50))
          h0->border_state = OUT;
        break;
      case EMPTY:
/*
        if (! (random() % 3000))
          h0->border_state = IN;
 */
        break;
      default:
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
          }
        else
          {
            x = random() % bp->grid_w;
            y = random() % bp->grid_h;
          }
        h0 = &bp->hexagons[y * bp->grid_w + x];
        if (empty_hexagon_p (h0) &&
            add_arms (mi, h0, True)) 
          break;
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
  GLfloat thick2 = thickness * bp->fade_ratio;
  int i;

# undef H
# define H 0.8660254037844386   /* sqrt(3)/2 */
  const XYZ corners[] = {{  0, -1,   0 },       /*      0      */
                         {  H, -0.5, 0 },       /*  5       1  */
                         {  H,  0.5, 0 },       /*             */
                         {  0,  1,   0 },       /*  4       2  */
                         { -H,  0.5, 0 },       /*      3      */
                         { -H, -0.5, 0 }};

  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_TRIANGLES);
  glNormal3f (0, 0, 1);

  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    {
      hexagon *h = &bp->hexagons[i];
      int total_arms = 0;
      int done_arms = 0;
      GLfloat color[4];
      int j;

      for (j = 0; j < 6; j++)
        {
          arm *a = &h->arms[j];
          if (a->state == OUT || a->state == DONE)
            total_arms++;
          if (a->state == DONE)
            done_arms++;
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
          GLfloat margin = thickness * 0.4;
          GLfloat size1 = size * (1 - margin * 2);
          GLfloat size2 = size * (1 - margin * 3);
          int k = (j + 1) % 6;
          XYZ p[6];

          if (h->border_state != EMPTY)
            {
              GLfloat color1[4];
              memcpy (color1, color, sizeof(color1));
              color1[0] *= h->border_ratio;
              color1[1] *= h->border_ratio;
              color1[2] *= h->border_ratio;

              glColor4fv (color1);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);

              /* Outer edge of hexagon border */
              p[0].x = h->pos.x + corners[j].x * size1;
              p[0].y = h->pos.y + corners[j].y * size1;
              p[0].z = h->pos.z;

              p[1].x = h->pos.x + corners[k].x * size1;
              p[1].y = h->pos.y + corners[k].y * size1;
              p[1].z = h->pos.z;

              /* Inner edge of hexagon border */
              p[2].x = h->pos.x + corners[k].x * size2;
              p[2].y = h->pos.y + corners[k].y * size2;
              p[2].z = h->pos.z;
              p[3].x = h->pos.x + corners[j].x * size2;
              p[3].y = h->pos.y + corners[j].y * size2;
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
          if (a->state == IN || a->state == OUT || a->state == DONE)
            {
              GLfloat x   = (corners[j].x + corners[k].x) / 2;
              GLfloat y   = (corners[j].y + corners[k].y) / 2;
              GLfloat xoff = corners[k].x - corners[j].x;
              GLfloat yoff = corners[k].y - corners[j].y;
              GLfloat line_length = h->arms[j].ratio;
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
          if (total_arms)
            {
              GLfloat size3 = size * thick2 * 0.8;
              if (total_arms == 1)
                size3 *= 2;

              p[0] = h->pos;

              p[1].x = h->pos.x + corners[j].x * size3;
              p[1].y = h->pos.y + corners[j].y * size3;
              p[1].z = h->pos.z;

              /* Inner edge of hexagon border */
              p[2].x = h->pos.x + corners[k].x * size3;
              p[2].y = h->pos.y + corners[k].y * size3;
              p[2].z = h->pos.z;

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
        ;
      else if (c == '>' || c == '.' || c == '+' || c == '=' ||
               keysym == XK_Right || keysym == XK_Up || keysym == XK_Next)
        MI_COUNT(mi)++;
      else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' ||
               keysym == XK_Left || keysym == XK_Down || keysym == XK_Prior)
        MI_COUNT(mi)--;
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        ;
      else
        return False;

    RESET:
      if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
      free (bp->hexagons);
      bp->hexagons = 0;
      bp->state = FIRST;
      bp->fade_ratio = 1;
      bp->live_count = 0;
      make_plane (mi);
      return True;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    goto RESET;

  return False;
}


ENTRYPOINT void 
init_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hextrail (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    double spin_speed   = 0.002;
    double wander_speed = 0.003;
    double spin_accel   = 1.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }


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
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

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

  if (! bp->button_down_p)
    tick_hexagons (mi);
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

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
