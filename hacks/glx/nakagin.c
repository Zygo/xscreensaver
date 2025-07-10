/* nakagin, Copyright © 2022 Jamie Zawinski <jwz@jwz.org>
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
			"*capsuleColor: #DDDDFF\n" \
			"*windowColor:  #8888AA\n" \
			"*doorColor:    #402010\n" \
			"*towerColor:   #873E23\n" \

# define release_nakagin 0

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "normals.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define STACK_HEIGHT	20
#define BASEMENT_DEPTH	5
#define CAPSULE_ASPECT	1.6
#define WINDOW_SIZE	0.528

#define DEF_SPEED	"1.0"
#define DEF_SPIN	"True"
#define DEF_WANDER	"False"
#define DEF_TILT	"True"

typedef enum { AVAIL, WAIT, UP, OVER, DOWN, DOCKED, OCCUPIED, EJECT, DEAD
} state_t;
typedef enum { XX, TT, N, S, E, W, NE, SE, NW, SW } orient_t;
typedef enum { CAPSULE, WINDOW, GLASS, DOOR } capsule_component_t;

typedef struct {
  state_t state;
  XYZ start_pos, end_pos, pos;
  GLfloat start_th, end_th, th;
  GLfloat ratio;
  GLfloat speed;
  double wait_until;
  enum { FRONT, LEFT, RIGHT } window_pos, door_pos;
  enum { DARK, SOLID, TV } light_state;
  GLfloat light_color[4];
} capsule;

typedef struct {
  state_t state;
  XYZ start_pos, end_pos, pos;
  GLfloat ratio;
  GLfloat speed;
} tower;

typedef struct {
  GLfloat y;
  struct {
    orient_t orient;
    GLfloat y;
    capsule c;
  } cell[4][8];
} floorplan;

typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  int ffwd;

  GLfloat capsule_color[4], window_color[4], door_color[4], tower_color[4];
  GLuint capsule_dlist,  window_dlist,  glass_dlist,  door_dlist, tower_dlist;
  int    capsule_npolys, window_npolys, glass_npolys, door_npolys,tower_npolys;

  floorplan floorplans[STACK_HEIGHT];
  tower towers[2];

} nakagin_configuration;

static nakagin_configuration *bps = NULL;

static GLfloat speed;
static Bool do_spin;
static Bool do_wander;
static Bool do_tilt;

static XrmOptionDescRec opts[] = {
  { "-speed",   ".speed",  XrmoptionSepArg, 0 },
  { "-spin",    ".spin",   XrmoptionNoArg, "True"  },
  { "+spin",    ".spin",   XrmoptionNoArg, "False" },
  { "-wander",  ".wander", XrmoptionNoArg, "True"  },
  { "+wander",  ".wander", XrmoptionNoArg, "False" },
  { "-tilt",    ".tilt",   XrmoptionNoArg, "True"  },
  { "+tilt",    ".tilt",   XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&do_tilt,   "tilt",   "Tilt",   DEF_TILT,   t_Bool},
};

ENTRYPOINT ModeSpecOpt nakagin_opts = {
  countof(opts), opts, countof(vars), vars, NULL };


static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
make_floorplan (floorplan *fp, floorplan *prev)
{
  /*  Possible floor plans look like this.  Any pair labeled NE can be either
      N or E, but they both need to be the same.  Same for NW, etc.

      The east and westmost columns are raised slightly, and the northmost two
      rows are raised further.

      We could also generate random, ahistoric floorplans, but for now let's
      stick with the original, modulo capsule orientations.

      In reality, the capsule layouts came in mirrored sets: doors were
      positioned towards the tower center, rather than always being on the
      left.

          -1     0     1      2      3       4     5       6     7      8
      .------.------.------.------.------.------.------.------.------.------.
      |      |                           |                           |      |
  -1  |      |       ###### ######       |       ###### ######       |      |
      |      |       #    # #    #       |       #    # #    #       |      |
      .------.----   #    # #    #  -----.-----  #    # #    #  -----.------.
      |              #    # #    #               #    # #    #              |
   0  |       ###### # NW # # N  # ###### ###### # N  # # NE # ######       |
      |       #    # ###### ###### #    # #    # ###### ###### #    #       |
      .-----  #    #               #    #.#    #               #    #  -----.
      |       #    #       |       #    # #    #       |       #    #       |
   1  |       # NW #       |       # N  # # N  #       |       # NE #       |
      |       ######       |       ###### ######       |       ######       |
      .-----          -----.-----                ------.------         -----.
      |       ######       |       ###### ######       |       ######       |
   2  |       # SW #       |       # S  # # S  #       |       # SE #       |
      |       #    #       |       #    # #    #       |       #    #       |
      .-----  #    #               #    # #    #               #    #  -----.
      |       #    # ###### ###### #    # #    # ###### ###### #    #       |
   3  |       ###### # SW # # S  # ###### ###### # S  # # SE # ######       |
      |              #    # #    #               #    # #    #              |
      .------.----   #    #.#    #  -----.-----  #    # #    #  -----.------.
      |      |       #    # #    #       |       #    # #    #       |      |
   4  |      |       ###### ######       |       ###### ######       |      |
      |      |                           |                           |      |
      .------.------.------.------.------.------.------.------.------.------.

      .------.------.------.------.------.------.------.------.------.------.
      |      |      |                    |                    |      |      |
  -1  |      |      |       ######       |       ######       |      |      |
      |      |              #    #       |       #    #              |      |
      .------.   ########## #    #  -----.-----  #    # ##########   .------.
      |      |   #        # #    #               #    # #        #   |      |
   0  |          #     NW # # N  # ###### ###### # N  # # NE     #          |
      |          ########## ###### #    # #    # ###### ##########          |
      .   ##########               #    # #    #               ##########   .
      |   #        #       |       #    # #    #       |       #        #   |
   1  |   #     NW #       |       # N  # # N  #       |       # NE     #   |
      |   ##########       |       ###### ######       |       ##########   |
      .               -----.----                   ----.----                .
      |   ##########       |       ###### ######       |       ##########   |
   2  |   #     SW #       |       # S  # # S  #       |       # SE     #   |
      |   #        #       |       #    # #    #       |       #        #   |
      .   ##########               #    # #    #               ##########   .
      |          ########## ###### #    # #    # ###### ##########          |
   3  |          #     SW # # S  # ###### ###### # S  # # SE     #          |
      |      |   #        # #    #               #    # #        #   |      |
      .------.   #########  #    #  -----.-----  #    # ##########   .------.
      |      |              #    #       |       #    #              |      |
   4  |      |      |       ######       |       ######       |      |      |
      |      |      |                    |                    |      |      |
      .------.------.------.------.------.------.------.------.------.------.
  */

  static const floorplan base_floorplan = { 0,
   {{{XX,0.4},{NW,0.2},{N, 0.2},{XX,0.2},{XX,0.2},{N, 0.2},{NE,0.2},{XX,0.4}},
    {{NW,0.4},{TT,0.0},{TT,0.0},{N, 0.2},{N, 0.2},{TT,0.0},{TT,0.0},{NE,0.4}},
    {{SW,0.2},{TT,0.0},{TT,0.0},{S, 0.0},{S, 0.0},{TT,0.0},{TT,0.0},{SE,0.2}},
    {{XX,0.2},{SW,0.0},{S, 0.0},{XX,0.0},{XX,0.0},{S, 0.0},{SE,0.0},{XX,0.2}}
   }};
  int x, y;
  int w = countof (*base_floorplan.cell);
  int h = countof ( base_floorplan.cell);

  orient_t nw = (random() & 1 ? N : W);
  orient_t ne = (random() & 1 ? N : E);
  orient_t sw = (random() & 1 ? S : W);
  orient_t se = (random() & 1 ? S : E);

  /* Occlusion map: how much does the lower floor intrude on this one? */
  GLfloat occ[countof( base_floorplan.cell)]
             [countof(*base_floorplan.cell)] = {{ 0, }, };

  if (w != 8) abort();
  if (h != 4) abort();
  if (countof (*occ) != w) abort();
  if (countof ( occ) != h) abort();

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      occ[y][x] = 0;

  /* Fill occlusion map */
  if (prev)
    {
      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          {
            orient_t o = prev->cell[y][x].orient;
            GLfloat yo = prev->cell[y][x].y;

            if (o != XX && o != TT)
              {
                int y2 = y;
                int x2 = x;
                switch (o) {
                case N: y2--; break;
                case S: y2++; break;
                case E: x2++; break;
                case W: x2--; break;
                default: abort(); break;
                }
                occ[y][x] = yo;
                if (x2 >= 0 && x2 < w &&
                    y2 >= 0 && y2 < h)
                  occ[y2][x2] = yo;
              }
          }
    }

  /* If either of the two cells occupied by this capsule (base and overhang)
     are occluded by something on the lower floor, there can't be a capsule
     here.
   */
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        orient_t o = base_floorplan.cell[y][x].orient;
        GLfloat yo = base_floorplan.cell[y][x].y;
        int x2 = x;
        int y2 = y;

        switch (o) {
        case NW: o = nw; break;
        case NE: o = ne; break;
        case SW: o = sw; break;
        case SE: o = se; break;
        default: break;
        }

        /* Find the coordinate of the capsule's overhang. */
        switch (o) {
        case N: y2--; break;
        case S: y2++; break;
        case E: x2++; break;
        case W: x2--; break;
        default: break;
        }

        if (occ[y][x] > yo ||
            (x2 >= 0 && x2 < w &&
             y2 >= 0 && y2 < h &&
             occ[y2][x2] > yo))
          o = XX;

        fp->cell[y][x].orient = o;
        fp->cell[y][x].y = yo;
        fp->cell[y][x].c.state = (o == XX || o == TT ? DEAD : AVAIL);
      }

  /* Decide which window positions are available.  Front always works.
     We can have a window on the left if there is no neighbor to the
     left, or if it has a different orientation than us.

     Actually, no, a neighbor with a different orientation clips about
     a quarter of our window.
   */
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        orient_t o = fp->cell[y][x].orient;
        if (o != XX && o != TT)
          {
            int x2 = x;
            int y2 = y;
            int x3 = x;
            int y3 = y;
            Bool left_p, right_p;

            /* Find the coordinate of our left and right neighbors. */
            switch (o) {
            case N: x2--; x3++; break;
            case S: x2++; x3--; break;
            case E: y2--; y3++; break;
            case W: y2++; y3--; break;
            default: abort(); break;
            }
            left_p = (x2 < 0 || x2 >= w ||
                      y2 < 0 || y2 >= h ||
                      fp->cell[y2][x2].orient == XX /* ||
                      (fp->cell[y2][x2].orient != TT &&
                       fp->cell[y2][x2].orient != o) */);
            right_p = (x3 < 0 || x3 >= w ||
                       y3 < 0 || y3 >= h ||
                       fp->cell[y3][x3].orient == XX /* ||
                       (fp->cell[y3][x3].orient != TT &&
                        fp->cell[y3][x3].orient != o) */);

# if 0
            fprintf (stderr,
                     "## %2d %2d %s,  %2d %2d %s,  %2d %2d %s,  %d %d\n",
                     x, y,
                     (o == N ? "N" :
                      o == S ? "S" :
                      o == E ? "E" :
                      o == W ? "W" :
                      o == TT ? "T" :
                      o == XX ? "X" : "?"),
                     x2, y2,
                     (fp->cell[y2][x2].orient == N ? "N" :
                      fp->cell[y2][x2].orient == S ? "S" :
                      fp->cell[y2][x2].orient == E ? "E" :
                      fp->cell[y2][x2].orient == W ? "W" :
                      fp->cell[y2][x2].orient == TT ? "T" :
                      fp->cell[y2][x2].orient == XX ? "X" : "?"),
                     x3, y3,
                     (fp->cell[y3][x3].orient == N ? "N" :
                      fp->cell[y3][x3].orient == S ? "S" :
                      fp->cell[y3][x3].orient == E ? "E" :
                      fp->cell[y3][x3].orient == W ? "W" :
                      fp->cell[y3][x3].orient == TT ? "T" :
                      fp->cell[y3][x3].orient == XX ? "X" : "?"),
                     left_p, right_p);
# endif /* 0 */

            if (left_p && right_p)
              {
                int n = random() % 4;
                fp->cell[y][x].c.window_pos =
                  (n <= 1 ? FRONT : n == 2 ? LEFT : RIGHT);
              }
            else if (left_p)
              {
                int n = random() % 3;
                fp->cell[y][x].c.window_pos = (n <= 1 ? FRONT : LEFT);
              }
            else if (right_p)
              {
                int n = random() % 3;
                fp->cell[y][x].c.window_pos = (n <= 1 ? FRONT : RIGHT);
              }
            else
              fp->cell[y][x].c.window_pos = FRONT;

            /* Figure out where the door must go.  There's only one option:
               it goes on the wall that touches the central tower.
               We already know our left and right neighbors, so if one of
               those is the tower, that's it; else back.
             */
            left_p = (x2 >= 0 && x2 < w &&
                      y2 >= 0 && y2 < h &&
                      fp->cell[y2][x2].orient == TT);
            right_p = (x3 >= 0 && x3 < w &&
                       y3 >= 0 && y3 < h &&
                       fp->cell[y3][x3].orient == TT);
            if (left_p && right_p)
              abort();
            else if (left_p)
              fp->cell[y][x].c.door_pos = LEFT;   /* When facing window */
            else if (right_p)
              fp->cell[y][x].c.door_pos = RIGHT;
            else
              fp->cell[y][x].c.door_pos = FRONT;  /* Meaning back */

            /* Decide on the interior lighting. */
            {
              capsule *c = &fp->cell[y][x].c;
              int n = random() % 100;
              if      (n < 8)  c->light_state = SOLID;
              else if (n < 10) c->light_state = TV;
              else             c->light_state = DARK;

              n = random() % 100;
              if (c->light_state == DARK)
                {
                  c->light_color[0] = 0;
                  c->light_color[1] = 0;
                  c->light_color[2] = 0;
                  c->light_color[3] = 1;
                }
              else if (n < 50)
                {
                  c->light_color[0] = 0.2 + frand(0.02);  /* Yellow-ish */
                  c->light_color[1] = 0.2 + frand(0.02);
                  c->light_color[2] = frand (0.005);
                  c->light_color[3] = 1;
                }
              else if (n < 85)
                {
                  c->light_color[0] = 0.2 + frand(0.1);  /* Red-ish */
                  c->light_color[1] = frand (0.01);
                  c->light_color[2] = frand (0.01);
                  c->light_color[3] = 1;
                }
              else
                {
                  c->light_color[0] = frand (0.01);	 /* Blue-ish */
                  c->light_color[1] = frand (0.01);
                  c->light_color[2] = 0.2 + frand(0.1);
                  c->light_color[3] = 1;
                }
            }
          }
      }

# if 0
  fprintf (stderr, "\n");
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
        {
          orient_t o = fp->cell[y][x].orient;
          fprintf (stderr, " %s%s",
                   (o == N ? "N" :
                    o == S ? "S" :
                    o == E ? "E" :
                    o == W ? "W" :
                    o == TT ? ":" :
                    o == XX ? "-" : "?"),
                   (o == XX ? "-" :
                    o == TT ? ":" :
                    fp->cell[y][x].c.window_pos == FRONT ? " " :
                    fp->cell[y][x].c.window_pos == LEFT  ? "L" : "R")
                   );
        }

      fprintf (stderr, "\t");
      for (x = 0; x < w; x++)
        fprintf (stderr, " %.0f", 10 * occ[y][x]);

      fprintf (stderr, "\n");
    }
# endif /* 0 */
}


static int
make_capsule (ModeInfo *mi, capsule_component_t which)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  GLfloat wthick = 0.10;
  GLfloat wdepth = 0.02;
  GLfloat z = CAPSULE_ASPECT - 0.5;
  GLfloat steps = (wire ? 12 : 60);
  int i;

  glPushMatrix();
  glFrontFace (GL_CCW);
  glRotatef (180, 0, 1, 0);
  glTranslatef (0, 0.5, 0);

  switch (which) {
  case CAPSULE:

    /* Back */
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (0, 0, 1);
    glVertex3f ( 0.5, -0.5,  0.5);
    glVertex3f ( 0.5,  0.5,  0.5);
    glVertex3f (-0.5,  0.5,  0.5);
    glVertex3f (-0.5, -0.5,  0.5);
    polys++;
    glEnd();

    /* Right */
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (1, 0, 0);
    glVertex3f ( 0.5,  0.5,  0.5);
    glVertex3f ( 0.5, -0.5,  0.5);
    glVertex3f ( 0.5, -0.5, -z);
    glVertex3f ( 0.5,  0.5, -z);
    polys++;
    glEnd();

    /* Front */
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (0, 0, -1);
    glVertex3f ( 0.5, -0.5,  -z);
    glVertex3f (-0.5, -0.5,  -z);
    glVertex3f (-0.5,  0.5,  -z);
    glVertex3f ( 0.5,  0.5,  -z);
    polys++;
    glEnd();

    /* Left */
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (-1, 0, 0);
    glVertex3f (-0.5, -0.5,  0.5);
    glVertex3f (-0.5,  0.5,  0.5);
    glVertex3f (-0.5,  0.5, -z);
    glVertex3f (-0.5, -0.5, -z);
    polys++;
    glEnd();

    if (wire) break;

    /* Top */
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (0, 1, 0);
    glVertex3f (-0.5, 0.5, 0.5);
    glVertex3f ( 0.5, 0.5, 0.5);
    glVertex3f ( 0.5, 0.5, -z);
    glVertex3f (-0.5, 0.5, -z);
    polys++;
    glEnd();

    /* Bottom */
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (0, -1, 0);
    glVertex3f ( 0.5, -0.5, 0.5);
    glVertex3f (-0.5, -0.5, 0.5);
    glVertex3f (-0.5, -0.5, -z);
    glVertex3f ( 0.5, -0.5, -z);
    polys++;
    glEnd();

    break;
  case WINDOW:

    /* Front face */
    glNormal3f (0, 0, -1);
    glBegin (wire ? GL_LINE_LOOP : GL_QUAD_STRIP);
    for (i = 0; i <= steps; i++)
      {
        GLfloat r = i / (GLfloat) steps;
        GLfloat x = WINDOW_SIZE * 0.5 * cos (M_PI * 2 * r);
        GLfloat y = WINDOW_SIZE * 0.5 * sin (M_PI * 2 * r);
        GLfloat r2 = 1 - wthick;
        glVertex3f (x,      y,      -z - wdepth);
        if (! wire)
          glVertex3f (x * r2, y * r2, -z - wdepth);
        polys++;
      }
    glEnd();

    if (wire) break;

    /* Outside rim */
    glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
    for (i = 0; i <= steps; i++)
      {
        GLfloat r = i / (GLfloat) steps;
        GLfloat x = WINDOW_SIZE * 0.5 * cos (M_PI * 2 * r);
        GLfloat y = WINDOW_SIZE * 0.5 * sin (M_PI * 2 * r);
        glNormal3f (x, y, 0);
        glVertex3f (x, y, -z);
        glVertex3f (x, y, -z - wdepth);
        polys++;
      }
    glEnd();

    /* Inside rim */
    glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
    for (i = 0; i <= steps; i++)
      {
        GLfloat r = i / (GLfloat) steps;
        GLfloat r2 = 1 - wthick;
        GLfloat x = r2 * WINDOW_SIZE * 0.5 * cos (M_PI * 2 * r);
        GLfloat y = r2 * WINDOW_SIZE * 0.5 * sin (M_PI * 2 * r);
        glNormal3f (-x, -y, 0);
        glVertex3f (x, y, -z - wdepth);
        glVertex3f (x, y, -z);
        polys++;
      }
    glEnd();

    break;

  case GLASS:

    if (wire) break;

    /* Front face */
    glNormal3f (0, 0, -1);
    glBegin (wire ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
    glVertex3f (0, 0, -z - wdepth/2);
    for (i = steps; i >= 0; i--)
      {
        GLfloat r = i / (GLfloat) steps;
        GLfloat r2 = 1 - wthick;
        GLfloat x = WINDOW_SIZE * 0.5 * cos (M_PI * 2 * r);
        GLfloat y = WINDOW_SIZE * 0.5 * sin (M_PI * 2 * r);
        glVertex3f (x * r2, y * r2, -z - wdepth/2);
        polys++;
      }
    glEnd();
    break;

  case DOOR:							/* Door */
    {
      GLfloat dw    = 0.39;
      GLfloat dh    = 0.91;
      GLfloat left  = dw / 3;
      GLfloat right = left + dw;
      GLfloat bot   = 0.01;
      GLfloat top   = dh + bot;
      GLfloat thick = 0.02;

      /* Outside */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, 0, 1);
      glVertex3f (left  - 0.5, bot - 0.5, 0.5 + thick);
      glVertex3f (right - 0.5, bot - 0.5, 0.5 + thick);
      glVertex3f (right - 0.5, top - 0.5, 0.5 + thick);
      glVertex3f (left  - 0.5, top - 0.5, 0.5 + thick);
      polys++;
      glEnd();

      if (wire) break;

      /* Inside */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, 0, -1);
      glVertex3f (left  - 0.5, top - 0.5, 0.5 - thick);
      glVertex3f (right - 0.5, top - 0.5, 0.5 - thick);
      glVertex3f (right - 0.5, bot - 0.5, 0.5 - thick);
      glVertex3f (left  - 0.5, bot - 0.5, 0.5 - thick);
      polys++;
      glEnd();

      /* Right */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (1, 0, 0);
      glVertex3f (right - 0.5, top - 0.5, 0.5);
      glVertex3f (right - 0.5, top - 0.5, 0.5 + thick);
      glVertex3f (right - 0.5, bot - 0.5, 0.5 + thick);
      glVertex3f (right - 0.5, bot - 0.5, 0.5);
      polys++;
      glEnd();

      /* Left */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (-1, 0, 0);
      glVertex3f (left - 0.5, bot - 0.5, 0.5);
      glVertex3f (left - 0.5, bot - 0.5, 0.5 + thick);
      glVertex3f (left - 0.5, top - 0.5, 0.5 + thick);
      glVertex3f (left - 0.5, top - 0.5, 0.5);
      polys++;
      glEnd();

      /* Top */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, 1, 0);
      glVertex3f (left  - 0.5, top - 0.5, 0.5);
      glVertex3f (left  - 0.5, top - 0.5, 0.5 + thick);
      glVertex3f (right - 0.5, top - 0.5, 0.5 + thick);
      glVertex3f (right - 0.5, top - 0.5, 0.5);
      polys++;
      glEnd();

      /* Bot */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, -1, 0);
      glVertex3f (right - 0.5, bot - 0.5, 0.5);
      glVertex3f (right - 0.5, bot - 0.5, 0.5 + thick);
      glVertex3f (left  - 0.5, bot - 0.5, 0.5 + thick);
      glVertex3f (left  - 0.5, bot - 0.5, 0.5);
      polys++;
      glEnd();

    }
    break;

  default:
    abort();
    break;

  }

  glPopMatrix();
  return polys;
}


static int
make_tower (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  GLfloat wthick = 0.15;
  GLfloat w = 2;
  GLfloat piling = BASEMENT_DEPTH + 4;
  GLfloat cap = 3;
  GLfloat h = STACK_HEIGHT + piling + cap;
  GLfloat h0 = h;
  GLfloat h1 = h - 2 * 3/4.0;
  GLfloat h2 = h - 2 * 6/4.0;
  GLfloat h3 = h - 2 * 1;
  GLfloat s;
  int i;

  glPushMatrix();
  glRotatef (-90, 1, 0, 0);

  glTranslatef (0, 0, -piling);

  for (i = 0; i <= 1; i++)
    {
      GLfloat si = i ? -1 : 1;

      if (wire && i) break;

      s = i ? 1 - wthick : 1;
      glFrontFace (i ? GL_CW : GL_CCW);

      /* North */
      glNormal3f (0, si * -1, 0);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (s *  w/2, s * -w/2,  0);
      glVertex3f (s *  w/2, s * -w/2,  h1);
      glVertex3f (s * -w/2, s * -w/2,  h0 - (i ? wthick*2 : 0));
      glVertex3f (s * -w/2, s * -w/2,  0);
      polys++;
      glEnd();

      /* East */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (si * 1, 0, 0);
      glVertex3f (s * w/2, s * -w/2,  h1);
      glVertex3f (s * w/2, s * -w/2,  0);
      glVertex3f (s * w/2, s *  w/2,  0);
      glVertex3f (s * w/2, s *  w/2,  h2 + (i ? wthick : 0));
      polys++;
      glEnd();

      /* South */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, si * 1, 0);
      glVertex3f (s *  w/2, s * w/2,  h2 + (i ? wthick : 0));
      glVertex3f (s *  w/2, s * w/2,  0);
      glVertex3f (s * -w/2, s * w/2,  0);
      glVertex3f (s * -w/2, s * w/2,  h3);
      polys++;
      glEnd();

      /* West */
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (si * -1, 0, 0);
      glVertex3f (s * -w/2, s *  w/2,  h3);
      glVertex3f (s * -w/2, s *  w/2,  0);
      glVertex3f (s * -w/2, s * -w/2,  0);
      glVertex3f (s * -w/2, s * -w/2,  h0 - (i ? wthick*2 : 0));
      polys++;
      glEnd();
    }

  if (!wire)
    {
      /* Top rim */
      glFrontFace (GL_CW);
      glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
      glNormal3f (0, 0, 1);  /* Not quite right */
      glVertex3f (    w/2,      -w/2, h1);
      glVertex3f (s * w/2, s *  -w/2, h1);
      glVertex3f (    w/2,       w/2, h2);
      glVertex3f (s * w/2, s *   w/2, h2 + wthick);
      glVertex3f (    -w/2,      w/2, h3);
      glVertex3f (s * -w/2, s *  w/2, h3);
      glVertex3f (    -w/2,     -w/2, h0);
      glVertex3f (s * -w/2, s * -w/2, h0 - wthick*2);
      glVertex3f (    w/2,      -w/2, h1);
      glVertex3f (s * w/2, s *  -w/2, h1);
      polys += 4;
      glEnd();

      /* Top floor */
      glFrontFace (GL_CW);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, 0, 1);
      glVertex3f (s * -w/2, s *   w/2, h2);
      glVertex3f (s *  w/2, s *   w/2, h2);
      glVertex3f (s *  w/2, s *  -w/2, h2);
      glVertex3f (s * -w/2, s *  -w/2, h2);
      polys++;
      glEnd();
    }

  glPopMatrix();
  return polys;
}


static int
draw_capsule (ModeInfo *mi, capsule *c, GLfloat y)
{
  nakagin_configuration *bp = &bps[MI_SCREEN(mi)];
  int polys = 0;
  GLfloat ss = 0.95;
  /* int wire = MI_IS_WIREFRAME(mi); */

  static const GLfloat spec[4]  = {0.3, 0.3, 0.3, 1.0};
  static const GLfloat wspec[4] = {1.0, 1.0, 0.0, 1.0};
  static const GLfloat shiny    = 128.0;
  static const GLfloat wshiny   = 100.0;

# if 0
  if (wire)
    {
      glBegin (GL_LINES);
      glVertex3f (c->start_pos.x, y + c->start_pos.y, c->start_pos.z);
      glVertex3f (c->end_pos.x,   y + c->end_pos.y,   c->end_pos.z);
      glEnd();
    }
# endif

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);

  glPushMatrix();
  glTranslatef (c->pos.x, y + c->pos.y, c->pos.z);
  glRotatef (c->th, 0, 1, 0);

  glScalef (ss, ss, ss);

  glColor4fv (bp->capsule_color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->capsule_color);
  glCallList (bp->capsule_dlist);
  polys += bp->capsule_npolys;

  glPushMatrix();
  switch (c->door_pos) {
  case FRONT: break;
  case LEFT:
    glRotatef (-90, 0, 1, 0);
    break;
  case RIGHT:
    glRotatef ( 90, 0, 1, 0);
    break;
    break;
  default: abort();
  }

  glColor4fv (bp->door_color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->door_color);
  glCallList (bp->door_dlist);
  polys += bp->door_npolys;
  glPopMatrix();

  switch (c->window_pos) {
  case FRONT: break;
  case LEFT:
    glRotatef (90, 0, 1, 0);
    glTranslatef (1-CAPSULE_ASPECT, 0, 1-CAPSULE_ASPECT);
    break;
  case RIGHT:
    glRotatef (-90, 0, 1, 0);
    glTranslatef (-(1-CAPSULE_ASPECT), 0, 1-CAPSULE_ASPECT);
    break;
    break;
  default: abort();
  }

  glColor4fv (bp->window_color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->window_color);
  glCallList (bp->window_dlist);
  polys += bp->window_npolys;

  if (c->state == OCCUPIED || c->state == EJECT)
    {
      if (c->light_state == TV)
        {
          double now = double_time();
          if (c->wait_until < now)
            {
              c->light_color[0] = frand(0.3);
              c->light_color[1] = frand(0.3);
              c->light_color[2] = frand(0.3);
              c->light_color[3] = 1;
              c->wait_until = now + 0.05 + frand(0.2);
            }
        }

      glColor4fv (c->light_color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c->light_color);
    }
  else
    {
      GLfloat black[] = { 0, 0, 0, 1 };
      glColor4fv (black);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, black);
    }

  glMaterialfv (GL_FRONT, GL_SPECULAR,  wspec);
  glMateriali  (GL_FRONT, GL_SHININESS, wshiny);
  glCallList (bp->glass_dlist);
  polys += bp->glass_npolys;

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);

  glPopMatrix();

  return polys;
}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.5;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


static GLfloat
max_stack_height (ModeInfo *mi)
{
  nakagin_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat yo = 0;
  int x, y, z;
  int w = countof (*bp->floorplans[0].cell);
  int h = countof ( bp->floorplans[0].cell);
  for (z = 0; z < countof(bp->floorplans); z++)
    for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
        {
          capsule *c = &bp->floorplans[z].cell[y][x].c;
          GLfloat y1 =  bp->floorplans[z].cell[y][x].y;
          GLfloat y2 = z + y1 + c->pos.y;
          if (c->state != AVAIL &&
              c->state != DEAD &&
              y2 > yo)
            yo = y2;
        }
  return yo;
}


static void
move_capsules (ModeInfo *mi)
{
  nakagin_configuration *bp = &bps[MI_SCREEN(mi)];
  int moving = 0;
  capsule *avail[16];
  int navail = 0;
  int x, y, z, i;
  GLfloat scroll_speed = speed * 0.002;
  GLfloat slide_speed  = speed * 0.03;
  double now = double_time();

  int w = countof (*bp->floorplans[z].cell);
  int h = countof ( bp->floorplans[z].cell);

  if (w != 8) abort();
  if (h != 4) abort();

  /* Scroll everything down */
  for (z = 0; z < countof (bp->floorplans); z++)
    bp->floorplans[z].y -= scroll_speed;
  for (z = 0; z < countof (bp->towers); z++)
    bp->towers[z].pos.y -= scroll_speed;

  /* If the bottommost floorplan has fallen off the bottom of the scene,
     move the others down one, and add a new blank one at the top.
   */
  if (bp->floorplans[0].y < -1)
    {
      /* Defer if anything on the bottom floor is still in motion. */
      Bool ok = True;
      floorplan *fp = &bp->floorplans[0];
      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          {
            capsule *c = &fp->cell[y][x].c;
            if (c->state != DOCKED &&
                c->state != OCCUPIED &&
                c->state != DEAD &&
                c->state != AVAIL)
              ok = False;  /* Still in motion */
          }

      if (ok)
        {
          for (z = 0; z < countof (bp->floorplans) - 1; z++)
            bp->floorplans[z] = bp->floorplans[z+1];
          make_floorplan (&bp->floorplans[z],
                          (z > 0 ? &bp->floorplans[z-1] : 0));
          bp->floorplans[z].y = bp->floorplans[z-1].y + 1;
        }
    }

  /* Run the capsule state machine.
   */
  for (z = 0; z < countof (bp->floorplans); z++)
    {
      floorplan *fp = &bp->floorplans[z];

      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          {
            capsule *c = &fp->cell[y][x].c;
            orient_t o =  fp->cell[y][x].orient;
            GLfloat yo = fp->y + c->pos.y;

            switch (c->state) {
            case OCCUPIED:
              if (yo < -1)			/* Scrolled off bottom */
                c->state = DEAD;
              else if (!bp->ffwd &&
                       !moving &&		/* Eviction */
                       !(random() % (int) (12000 * speed)))
                {
                  GLfloat d = 500;
                  c->state = EJECT;
                  c->speed = 0.1;
                  c->end_pos.y -= d/3;
                  moving++;
                  switch (o) {
                  case N: c->end_pos.x -= d; break;
                  case S: c->end_pos.x += d; break;
                  case E: c->end_pos.z -= d; break;
                  case W: c->end_pos.z += d; break;
                  default: abort(); break;
                  }
                }
              break;

            case DEAD:
              break;

            case AVAIL:
              if (fp->cell[y][x].orient != XX &&
                  fp->cell[y][x].orient != TT)
                if (navail < countof(avail))
                  avail[navail++] = c;
              break;

            case WAIT:
              moving++;
              if (bp->ffwd ||
                  c->wait_until < now)
                c->state = UP;
              break;

            case UP: case OVER: case DOWN: case DOCKED: case EJECT:
              {
                GLfloat r;
                if (c->state != DOCKED)
                  moving++;
                c->ratio += slide_speed * c->speed;
                if (bp->ffwd) c->ratio = 1;
                r = ease_ratio (c->ratio);
                c->pos.x = c->start_pos.x + r * (c->end_pos.x - c->start_pos.x);
                c->pos.y = c->start_pos.y + r * (c->end_pos.y - c->start_pos.y);
                c->pos.z = c->start_pos.z + r * (c->end_pos.z - c->start_pos.z);
                c->th = c->start_th + r * (c->end_th - c->start_th);

                if (c->ratio >= 1.0)
                  {
                    c->ratio = 0;
                    c->pos = c->start_pos = c->end_pos;
                    c->th  = c->start_th  = c->end_th;
                    switch (c->state) {
                    case UP:
                      c->state = OVER;
                      c->end_pos.x = y;
                      c->end_pos.y = c->pos.y;
                      c->end_pos.z = w - x;

                      switch (fp->cell[y][x].orient) {
                      case N: c->end_th = 270; break;
                      case W: c->end_th =   0; break;
                      case S: c->end_th =  90; break;
                      case E: c->end_th = 180; break;
                      default: abort(); break;
                      }

                      if (c->end_th - c->start_th > 180)
                        c->end_th -= 360;
                      else if (c->end_th - c->start_th < -180)
                        c->end_th += 360;

                      break;

                    case OVER:
                      c->state = DOWN;
                      c->end_pos.y = 0;  /* Relative to floorplan */
                      break;

                    case DOWN:
                      c->state = DOCKED;
                      if (bp->ffwd) bp->ffwd--;
                      break;

                    case DOCKED:
                      c->state = OCCUPIED;
                      break;

                    case EJECT:
                      c->state = DEAD;
                      break;

                    default: abort();
                    }
                  }
                }

              break;
            default:
              abort();
            }
          }
    }

  /* Shuffle the order of the 'avail' list.
   */
  for (i = 0; i < navail; i++)
    {
      int a = random() % navail;
      capsule *swap = avail[a];
      avail[a] = avail[i];
      avail[i] = swap;
    }

  /* Launch some new capsules.
   */
  for (i = 0; i < navail; i++)
    {
      GLfloat hh = max_stack_height(mi);
      capsule *c = avail[i];
      GLfloat d = 3;
      orient_t o = NW;  /* any invalid value */

      if (moving > countof(avail) * 0.33)
        break;

      if (random() % 3) continue;  /* only do 1/3 of them each time */

      /* Mark any capsules on lower levels in the same stack as unable to
         be added, as we are now blocking them in. */
      {
        Bool hit = False;
        for (y = 0; y < h; y++)
          {
            for (x = 0; x < w; x++)
              {
                for (z = countof (bp->floorplans)-1; z >= 0; z--)
                  {
                    floorplan *fp = &bp->floorplans[z];
                    capsule *c2 = &fp->cell[y][x].c;
                    if (c == c2)
                      {
                        hit = True;
                        o = fp->cell[y][x].orient;
                      }
                    else if (hit && c2->state == AVAIL)
                      c2->state = DEAD;
                  }
                if (hit) break;
              }
            if (hit) break;
          }
      }

      /* At this point, x and y are this capsule's grid coord; z is not. */

      c->state = WAIT;
      c->wait_until = now + (i * 0.3) / speed;
      c->speed = 1;

      c->start_pos.x = y;
      c->start_pos.y = -(hh + 2);
      c->start_pos.z = w - x;

      switch (o) {
      case N: c->start_pos.x -= d; c->start_pos.z += frand(1)-0.5; break;
      case S: c->start_pos.x += d; c->start_pos.z += frand(1)-0.5; break;
      case E: c->start_pos.z -= d; c->start_pos.x += frand(1)-0.5; break;
      case W: c->start_pos.z += d; c->start_pos.x += frand(1)-0.5; break;
      default: abort(); break;
      }

      c->pos = c->start_pos;
      c->start_th = c->th = c->end_th = frand (360);
      c->end_pos.x = c->start_pos.x;
      c->end_pos.y = 2 + frand(1.5);
      c->end_pos.z = c->start_pos.z;
      moving++;

    }

  /* Run the tower state machine.
   */
  for (z = 0; z < countof(bp->towers); z++)
    {
      tower *t = &bp->towers[z];
      switch (t->state) {
      case DOCKED:
        {
          GLfloat top = t->pos.y + STACK_HEIGHT;  /* East tower taller */
          GLfloat hh = max_stack_height(mi) - 1 - (z ? 2 : 0);
          if (top < hh)
            {
              t->state = UP;
              t->ratio = 0;
              t->start_pos = t->end_pos = t->pos;
              t->end_pos.y += 2 + frand(1.5);
              t->speed = 0.3 * (0.7 + frand(0.6));
            }
        }
        break;
      case UP:
        {
          GLfloat r;
          t->ratio += slide_speed * t->speed;
          if (bp->ffwd) t->ratio = 1;
          r = ease_ratio (t->ratio);
          t->pos.x = t->start_pos.x + r * (t->end_pos.x - t->start_pos.x);
          t->pos.y = t->start_pos.y + r * (t->end_pos.y - t->start_pos.y);
          t->pos.z = t->start_pos.z + r * (t->end_pos.z - t->start_pos.z);

          if (t->ratio >= 1.0)
            {
              t->ratio = 0;
              t->pos = t->start_pos = t->end_pos;
              t->state = DOCKED;
            }
        }
        break;
      default:
        abort();
      }
    }
}


ENTRYPOINT void
reshape_nakagin (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
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
nakagin_handle_event (ModeInfo *mi, XEvent *event)
{
  nakagin_configuration *bp = &bps[MI_SCREEN(mi)];

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
init_nakagin (ModeInfo *mi)
{
  nakagin_configuration *bp;
  int z;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_nakagin (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  parse_color (mi, "capsuleColor", bp->capsule_color);
  parse_color (mi, "windowColor",  bp->window_color);
  parse_color (mi, "doorColor",    bp->door_color);
  parse_color (mi, "towerColor",   bp->tower_color);

  if (MI_IS_WIREFRAME(mi))
    for (z = 0; z < 3; z++)
      {
        bp->capsule_color[z] *= 0.7;
        bp->tower_color[z] *= 2;
        bp->door_color[z] *= 2;
      }

  {
    double spin_speed   = 0.05;
    double wander_speed = 0.0025;
    double tilt_speed   = 0.001;
    double spin_accel   = 0.5;

    bp->rot = make_rotator (0, 0, do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->rot2 = make_rotator (0, 0, 0, 0,
                             (do_tilt ? tilt_speed : 0),
                             True);
    bp->trackball = gltrackball_init (False);
  }

  bp->capsule_dlist = glGenLists (1);
  glNewList (bp->capsule_dlist, GL_COMPILE);
  bp->capsule_npolys = make_capsule (mi, CAPSULE);
  glEndList ();

  bp->window_dlist = glGenLists (1);
  glNewList (bp->window_dlist, GL_COMPILE);
  bp->window_npolys = make_capsule (mi, WINDOW);
  glEndList ();

  bp->glass_dlist = glGenLists (1);
  glNewList (bp->glass_dlist, GL_COMPILE);
  bp->glass_npolys = make_capsule (mi, GLASS);
  glEndList ();

  bp->door_dlist = glGenLists (1);
  glNewList (bp->door_dlist, GL_COMPILE);
  bp->window_npolys = make_capsule (mi, DOOR);
  glEndList ();

  bp->tower_dlist = glGenLists (1);
  glNewList (bp->tower_dlist, GL_COMPILE);
  bp->tower_npolys = make_tower (mi);
  glEndList ();

  for (z = 0; z < countof(bp->floorplans); z++)
    {
      floorplan *fp = &bp->floorplans[z];
      make_floorplan (fp,
                      (z > 0 ? &bp->floorplans[z-1] : 0));
      fp->y = z;
    }

  for (z = 0; z < countof(bp->towers); z++)
    {
      tower *t = &bp->towers[z];
      t->speed = 1;
      t->ratio = 0;
      t->state = DOCKED;
      t->pos.x = 2;
      t->pos.z = (z == 0 ? 3 : 7);
      t->pos.y = (z == 0 ? 0 : -2) - STACK_HEIGHT - 5;
      t->start_pos = t->end_pos = t->pos;
    }

  /* How many capsules to pre-load before the first frame. */
  bp->ffwd = BASEMENT_DEPTH * 32;
}


ENTRYPOINT void
draw_nakagin (ModeInfo *mi)
{
  nakagin_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  int x, y, z;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glShadeModel (GL_SMOOTH);
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {4.0, 1.4, 1.1, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.5, 0.5, 0.5, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  if (bp->ffwd || !bp->button_down_p)
    do {
      move_capsules (mi);
    } while (bp->ffwd);

  mi->polygon_count = 0;

  glPushMatrix ();

  {
    double x, y, z;
    double maxz = 50;

    glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */

    gltrackball_rotate (bp->trackball);

    if (do_wander)
      {
        get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
        glTranslatef((x - 0.5) * 4,
                     (y - 0.5) * 0.2,
                     (z - 0.5) * 8);
      }

    if (do_tilt)
      {
        get_position (bp->rot2, &x, &y, &z, !bp->button_down_p);
        glRotatef (maxz / 2 - z * maxz, 1, 0, 0);
      }

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    if (do_spin)
      {
        /* int w = countof (*bp->floorplans[0].cell); */
        int h = countof ( bp->floorplans[0].cell);
        glTranslatef ( 0, 0, h / 2.0);
        glRotatef (z * 360, 0, 1, 0);
        glTranslatef (0, 0, -h / 2.0);

      }
  }

  glRotatef (-90, 0, 1, 0);
  glTranslatef (0, 0, -(GLfloat) (countof (*bp->floorplans[0].cell) / 2));

# if 0  /* debug one capsule */
  {
    floorplan *fp = &bp->floorplans[0];
    capsule *c = &fp->cell[0][1].c;
    glScalef (6,6,6);
    glTranslatef (0.5, -0.5, 0);
    c->pos.x = c->pos.y = c->pos.z = 0;
    c->door_pos = LEFT;
    mi->polygon_count += draw_capsule (mi, c, 0);
  }
# else

  glTranslatef (0, -(STACK_HEIGHT / 2 + BASEMENT_DEPTH), 0);
  glTranslatef (0, -2, 0);


  glColor4fv (bp->tower_color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bp->tower_color);
  for (z = 0; z < countof(bp->towers); z++)
    {
      tower *t = &bp->towers[z];
      glPushMatrix();
      glTranslatef (t->pos.x - 0.5, t->pos.y, t->pos.z - 0.5);
      glRotatef (-90, 0, 1, 0);
      glCallList (bp->tower_dlist);
      glPopMatrix();
      mi->polygon_count += bp->tower_npolys;
    }

  for (z = 0; z < countof(bp->floorplans); z++)
    {
      floorplan *fp = &bp->floorplans[z];
      int w = countof (*fp->cell);
      int h = countof ( fp->cell);

      if (w != 8) abort();
      if (h != 4) abort();

      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          {
            capsule *c = &fp->cell[y][x].c;
            GLfloat cy =  fp->cell[y][x].y;

            if (fp->cell[y][x].orient != XX &&
                fp->cell[y][x].orient != TT &&
                c->state != AVAIL &&
                c->state != WAIT)
              {
                GLfloat color[4];

                memcpy (color, bp->door_color, sizeof(color));
                switch (c->state) {
                case WAIT: color[3] = 0; break;
                case UP:   color[3] = c->ratio / 2; break;
                case OVER: color[3] = c->ratio / 2 + 0.5; break;
                case DOWN: case DOCKED: case OCCUPIED: color[3] = 1; break;
                case EJECT: color[3] = 1 - c->ratio; break;
                case DEAD: color[3] = 0; break;
                default: abort(); break;
                }

                glPushMatrix();
                glTranslatef (y, fp->y + cy, w-x);

                /* Floor schematic */
# if 0
                if (wire)
                  {
                    glColor3f (1, 0, 0);
                    glBegin (GL_LINE_LOOP);
                    glVertex3f (-0.25, 0, -0.25);
                    glVertex3f (-0.25, 0,  0.25);
                    glVertex3f ( 0.25, 0,  0.25);
                    glVertex3f ( 0.25, 0, -0.25);
                    glEnd();
                    mi->polygon_count += 4;
                  }
# endif

                /* Draw a door on the tower. */

                switch (fp->cell[y][x].orient) {
                case N: glRotatef (270, 0, 1, 0); break;
                case W: break;
                case S: glRotatef (90,  0, 1, 0); break;
                case E: glRotatef (180, 0, 1, 0); break;
                default: abort(); break;
                }

                switch (c->door_pos) {
                case FRONT: break;
                case LEFT:
                  glRotatef (-90, 0, 1, 0);
                  break;
                case RIGHT:
                  glRotatef ( 90, 0, 1, 0);
                  break;
                  break;
                default: abort();
                }

                glEnable (GL_BLEND);
                glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4fv (color);
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
                glCallList (bp->door_dlist);
                glDisable (GL_BLEND);

                mi->polygon_count += bp->door_npolys;
                glPopMatrix();
              }

            if (c->state != AVAIL && c->state != DEAD)
              mi->polygon_count += draw_capsule (mi, c, fp->y + cy);
          }
    }
# endif /* 0 */

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_nakagin (ModeInfo *mi)
{
  nakagin_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot)  free_rotator (bp->rot);
  if (bp->rot2) free_rotator (bp->rot2);

  if (glIsList(bp->capsule_dlist)) glDeleteLists(bp->capsule_dlist, 1);
  if (glIsList(bp->window_dlist))  glDeleteLists(bp->window_dlist,  1);
  if (glIsList(bp->glass_dlist))   glDeleteLists(bp->glass_dlist,   1);
  if (glIsList(bp->door_dlist))    glDeleteLists(bp->door_dlist,    1);
  if (glIsList(bp->tower_dlist))   glDeleteLists(bp->tower_dlist,   1);
}

XSCREENSAVER_MODULE ("Nakagin", nakagin)

#endif /* USE_GL */
