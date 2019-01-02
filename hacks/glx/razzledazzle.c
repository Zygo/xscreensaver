/* razzledazzle, Copyright (c) 2018 Jamie Zawinski <jwz@jwz.org>
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
			"*ncolors:      2           \n" \
			"*suppressRotationAnimation: True\n" \

# define release_dazzle 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "gllist.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPEED       "1.0"
#define DEF_DENSITY     "5.0"
#define DEF_THICKNESS   "0.1"
#define DEF_MODE        "Random"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)
#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

extern const struct gllist
  *ships_ship1, *ships_ship2, *ships_ship3, *ships_ship4,
  *ships_ship5, *ships_ship6, *ships_ship7, *ships_ship8;

static const struct gllist * const *all_ships[] = {
  &ships_ship1, &ships_ship2, &ships_ship3, &ships_ship4,
  &ships_ship5, &ships_ship6, &ships_ship7, &ships_ship8,
};


typedef enum { LEFT, RIGHT, UP, DOWN } direction;

typedef struct node node;

struct node {
  long gx, gy;
  GLfloat x, y;
  GLfloat dx, dy;
  int nstripes;
  Bool horiz_p;
  Bool drawn_p;
  GLfloat color1[4], color2[4];
};

typedef struct {
  GLXContext *glx_context;
  Bool button_down_p;
  GLfloat xoff, yoff, dx, dy;
  node *nodes;
  node *dragging;
  int drag_x, drag_y;
  int ncolors;
  XColor *colors;
  GLuint *dlists;
  enum { SHIPS, FLAT, RANDOM } mode;
  int which_ship;
  long frames;
} dazzle_configuration;

static dazzle_configuration *bps = NULL;

static GLfloat speed, thickness, density;
static char *mode_arg;

static XrmOptionDescRec opts[] = {
  { "-speed",     ".speed",     XrmoptionSepArg, 0 },
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
  { "-density",   ".density",   XrmoptionSepArg, 0 },
  { "-mode",      ".mode",      XrmoptionSepArg,  0  },
  { "-ships",     ".mode",      XrmoptionNoArg,  "ships"  },
  { "-flat",      ".mode",      XrmoptionNoArg,  "flat"   },
};

static argtype vars[] = {
  {&speed,     "speed",     "Speed",     DEF_SPEED,     t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
  {&density,   "density",   "Density",   DEF_DENSITY,   t_Float},
  {&mode_arg,  "mode",      "Mode",      DEF_MODE,      t_String},
};

ENTRYPOINT ModeSpecOpt dazzle_opts = {countof(opts), opts, countof(vars), vars, NULL};



static void
draw_grid (ModeInfo *mi, int gx, int gy)
{
  dazzle_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  long x, y;
  long wh = density * 2;

  if (wire)
    glColor3f (1, 1, 1);

  if (!wire)
    glBegin (GL_QUADS);

  for (y = 0; y < wh; y++)
    for (x = 0; x < wh; x++)
      {
        node *n0 = &bp->nodes[(y     % wh) * wh + (x     % wh)];
        node *n1 = &bp->nodes[(y     % wh) * wh + ((x+1) % wh)];
        node *n2 = &bp->nodes[((y+1) % wh) * wh + ((x+1) % wh)];
        node *n3 = &bp->nodes[((y+1) % wh) * wh + (x     % wh)];
        int nstripes, i;
        Bool horiz_p, visible;
        GLfloat x0, y0, x1, y1, x2, y2, x3, y3;
        GLfloat xoff = (x < wh-1 ? 0 : wh);
        GLfloat yoff = (y < wh-1 ? 0 : wh);
        GLfloat bx = fmod ((double) bp->xoff, 2.0);
        GLfloat by = fmod ((double) bp->yoff, 2.0);

        bx += gx*2;
        by += gy*2;

        if (wire)
          {
            GLfloat a;
            glColor3f (0, 0, 1);
            glBegin (GL_LINE_LOOP);
            for (a = 0; a < 360; a += 10)
              glVertex3f ((n0->x / density-1) + 0.05 * cos(a * M_PI/180) + bx,
                          (n0->y / density-1) + 0.05 * sin(a * M_PI/180) + by,
                          0);
            glEnd();
          }

        x0 = n0->x        / density - 1 + bx;
        y0 = n0->y        / density - 1 + by;

        x1 = (n1->x+xoff) / density - 1 + bx;
        y1 = n1->y        / density - 1 + by;
        x2 = (n2->x+xoff) / density - 1 + bx;
        y2 = (n2->y+yoff) / density - 1 + by;
        x3 = n3->x        / density - 1 + bx;
        y3 = (n3->y+yoff) / density - 1 + by;

        if (wire)
          {
            if (gx == 0 && gy == 0)
              {
                glLineWidth (4);
                glColor3f(1, 0, 0);
              }
            else
              glColor3f(0.5, 0, 0.5);
            if (wire) glBegin (GL_LINE_LOOP);
            glVertex3f (x0, y0, 0);
            glVertex3f (x1, y1, 0);
            glVertex3f (x2, y2, 0);
            glVertex3f (x3, y3, 0);
            mi->polygon_count++;
            if (wire) glEnd();
            glLineWidth (1);
          }

        /* This isn't quite right: just because all corners are off screen
           doesn't mean the quad isn't visible. We need to intersect the
           edges with the screen rectangle.
         */
        {
          GLfloat max = 0.75;
          visible = ((x0 >= -max && y0 >= -max && x0 <= max && y0  <= max) ||
                     (x1 >= -max && y1 >= -max && x1 <= max && y1  <= max) ||
                     (x2 >= -max && y2 >= -max && x2 <= max && y2  <= max) ||
                     (x3 >= -max && y3 >= -max && x3 <= max && y3  <= max));
        }

        if (!visible) continue;

        if (visible)
          n0->drawn_p = True;

        nstripes = n0->nstripes;
        horiz_p  = n0->horiz_p;

        for (i = 0; i < nstripes; i++)
          {
            GLfloat ss  = (GLfloat) i     / nstripes;
            GLfloat ss1 = (GLfloat) (i+1) / nstripes;
            if (i & 1)
              glColor4fv (n0->color1);
            else if (wire)
              continue;
            else
              glColor4fv (n0->color2);

            if (horiz_p)
              {
                x0 = n0->x        + (n3->x        - n0->x)        * ss;
                y0 = n0->y        + ((n3->y+yoff) - n0->y)        * ss;
                x1 = (n1->x+xoff) + ((n2->x+xoff) - (n1->x+xoff)) * ss;
                y1 = n1->y        + ((n2->y+yoff) - n1->y)        * ss;

                x2 = (n1->x+xoff) + ((n2->x+xoff) - (n1->x+xoff)) * ss1;
                y2 = n1->y        + ((n2->y+yoff) - n1->y)        * ss1;
                x3 = n0->x        + (n3->x        - n0->x)        * ss1;
                y3 = n0->y        + ((n3->y+yoff) - n0->y)        * ss1;
              }
            else
              {
                x0 = n0->x        + ((n1->x+xoff) - n0->x)        * ss;
                y0 = n0->y        + (n1->y        - n0->y)        * ss;
                x1 = n3->x        + ((n2->x+xoff) - n3->x)        * ss;
                y1 = (n3->y+yoff) + ((n2->y+yoff) - (n3->y+yoff)) * ss;

                x2 = n3->x        + ((n2->x+xoff) - n3->x)        * ss1;
                y2 = (n3->y+yoff) + ((n2->y+yoff) - (n3->y+yoff)) * ss1;
                x3 = n0->x        + ((n1->x+xoff) - n0->x)        * ss1;
                y3 = n0->y        + (n1->y        - n0->y)        * ss1;
              }

            if (wire) glBegin (GL_LINES);
            glVertex3f (x0 / density - 1 + bx, y0 / density - 1 + by, 0);
            glVertex3f (x1 / density - 1 + bx, y1 / density - 1 + by, 0);
            glVertex3f (x2 / density - 1 + bx, y2 / density - 1 + by, 0);
            glVertex3f (x3 / density - 1 + bx, y3 / density - 1 + by, 0);
            mi->polygon_count++;
            if (wire) glEnd();
          }
      }

  if (!wire)
    glEnd();
}


static void
move_grid (ModeInfo *mi)
{
  dazzle_configuration *bp = &bps[MI_SCREEN(mi)];
  long x, y;
  long wh = density * 2;
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat max = 1.0 / density * 3;

  if (bp->button_down_p)
    return;

  bp->xoff += bp->dx;
  bp->yoff += bp->dy;

  if (! (random() % 50))
    {
      bp->dx += frand(0.0002) * RANDSIGN() * speed;
      bp->dy += frand(0.0002) * RANDSIGN() * speed;
    }

  if (bp->dx > 0.003 * speed) bp->dx = 0.003 * speed;
  if (bp->dy > 0.003 * speed) bp->dy = 0.003 * speed;

  for (y = 0; y < wh; y++)
    for (x = 0; x < wh; x++)
      {
        node *n = &bp->nodes[y * wh + x];
        GLfloat x2 = n->x + n->dx;
        GLfloat y2 = n->y + n->dy;

        if (x2 < n->gx + max && x2 >= n->gx - max &&
            y2 < n->gy + max && y2 >= n->gy - max)
          {
            n->x = x2;
            n->y = y2;
          }

        if (! (random() % 50))
          {
            n->dx += frand(0.0005) * RANDSIGN() * speed;
            n->dy += frand(0.0005) * RANDSIGN() * speed;
          }

        /* If this quad was not drawn, it's ok to re-randomize stripes, */
        if (! n->drawn_p)
          {
            int i = random() % bp->ncolors;
            int j = (i + bp->ncolors / 2) % bp->ncolors;
            GLfloat cscale = 0.3;

            n->color1[0] = bp->colors[i].red   / 65536.0;
            n->color1[1] = bp->colors[i].green / 65536.0;
            n->color1[2] = bp->colors[i].blue  / 65536.0;
            n->color1[3] = 1.0;

            n->color2[0] = bp->colors[j].red   / 65536.0;
            n->color2[1] = bp->colors[j].green / 65536.0;
            n->color2[2] = bp->colors[j].blue  / 65536.0;
            n->color2[3] = 1.0;

            if (! wire)
              {
                n->color1[0] = cscale * n->color1[0] + 1 - cscale;
                n->color1[1] = cscale * n->color1[1] + 1 - cscale;
                n->color1[2] = cscale * n->color1[2] + 1 - cscale;
                n->color2[0] = cscale * n->color2[0];
                n->color2[1] = cscale * n->color2[1];
                n->color2[2] = cscale * n->color2[2];
              }

            n->horiz_p  = random() & 1;
            n->nstripes = 2 + (int) (BELLRAND(1.0 / thickness));
          }
        n->drawn_p = False;
      }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_dazzle (ModeInfo *mi, int width, int height)
{
  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, 1, 1, 0, -1, 1);

  if (width > height * 5) {   /* tiny window: show middle */
    GLfloat s = (GLfloat)height/width;
    glOrtho (0, 1, 0.5-s, 0.5+s, -1, 1);
  }

# ifdef USE_IPHONE	/* So much WTF */
  {
    int rot = current_device_rotation();

    glTranslatef (0.5, 0.5, 0);

    if (rot == 180 || rot == -180) {
      glTranslatef (1, 1, 0);
    } else if (rot == 90 || rot == -270) {
      glRotatef (180, 0, 0, 1);
      glTranslatef (0, 1, 0);
    } else if (rot == -90 || rot == 270) {
      glRotatef (180, 0, 0, 1);
      glTranslatef (1, 0, 0);
    }

    glTranslatef(-0.5, -0.5, 0);
  }
# endif

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
dazzle_randomize (ModeInfo *mi)
{
  dazzle_configuration *bp = &bps[MI_SCREEN(mi)];
  long x, y;
  long wh = density * 2;

  bp->ncolors = MI_NCOLORS(mi) - 1;
  if (bp->ncolors < 1) bp->ncolors = 1;
  if (bp->colors) free (bp->colors);
  bp->colors = (XColor *) calloc (bp->ncolors, sizeof(XColor));
  if (bp->ncolors < 3)
    make_random_colormap (0, 0, 0, bp->colors, &bp->ncolors,
                          True, False, 0, False);
  else
    make_smooth_colormap (0, 0, 0,
                          bp->colors, &bp->ncolors,
                          False, False, False);
  if (bp->ncolors < 1) abort();

  bp->dragging = 0;
  if (bp->nodes) free (bp->nodes);

  bp->nodes = (node *) calloc (wh * wh, sizeof (node));
  for (y = 0; y < wh; y++)
    for (x = 0; x < wh; x++)
      {
        node *n = &bp->nodes[wh * y + x];
        n->gx = n->x = x;
        n->gy = n->y = y;
      }

  bp->dx = bp->dy = 0;
  bp->xoff = bp->yoff = 0;
  for (x = 0; x < 1000; x++)
    move_grid (mi);

  bp->dx = frand(0.0005) * RANDSIGN() * speed;
  bp->dy = frand(0.0005) * RANDSIGN() * speed;

  if (bp->mode == SHIPS || bp->mode == RANDOM)
    {
      bp->which_ship = random() % countof(all_ships);
      if (bp->mode == RANDOM && !(random() % 3))
        bp->which_ship = -1;
    }

  if (bp->which_ship != -1)
    {
      bp->dx /= 10;
      bp->dy /= 10;
    }
}


ENTRYPOINT Bool
dazzle_handle_event (ModeInfo *mi, XEvent *event)
{
  dazzle_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat bx = fmod ((double) bp->xoff, 2.0);
  GLfloat by = fmod ((double) bp->yoff, 2.0);
  long wh = density * 2;

  if (event->xany.type == ButtonPress)
    {
      GLfloat x = (GLfloat) event->xbutton.x / MI_WIDTH (mi)  - 0.5;
      GLfloat y = (GLfloat) event->xbutton.y / MI_HEIGHT (mi) - 0.5;
      node *nn = 0;
      int xoff = 0, yoff = 0;
      GLfloat d2 = 999999;
      long x0, y0, x1, y1;

      if (wire) x /= 0.2, y /= 0.2;

      for (y0 = -1; y0 <= 1; y0++)
        for (x0 = -1; x0 <= 1; x0++)
          for (y1 = 0; y1 < wh; y1++)
            for (x1 = 0; x1 < wh; x1++)
              {
                node *n0 = &bp->nodes[(y1 % wh) * wh + (x1 % wh)];
                double dist2;
                GLfloat x2 = n0->x / density - 1 + bx + x0*2;
                GLfloat y2 = n0->y / density - 1 + by + y0*2;

                dist2 = (x - x2) * (x - x2) + (y - y2) * (y - y2);
                if (dist2 < d2)
                  {
                    d2 = dist2;
                    nn = n0;
                    xoff = x0;
                    yoff = y0;
                  }
              }

      bp->button_down_p = True;
      bp->dragging = nn;
      bp->drag_x = xoff;
      bp->drag_y = yoff;
      return True;
    }
  else if (event->xany.type == ButtonRelease)
    {
      bp->dragging = 0;
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify && bp->dragging)
    {
      GLfloat x = (GLfloat) event->xmotion.x / MI_WIDTH (mi)  - 0.5;
      GLfloat y = (GLfloat) event->xmotion.y / MI_HEIGHT (mi) - 0.5;
      if (wire) x /= 0.2, y /= 0.2;
      x -= bx;
      y -= by;
      x -= bp->drag_x * 2;
      y -= bp->drag_y * 2;
      bp->dragging->x = x * density + density;
      bp->dragging->y = y * density + density;
      bp->dragging->dx = bp->dragging->dy = 0;
      return True;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      dazzle_randomize (mi);
      return True;
    }

  return False;
}


ENTRYPOINT void 
init_dazzle (ModeInfo *mi)
{
  dazzle_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  if (!mode_arg || !*mode_arg || !strcasecmp(mode_arg, "random"))
    bp->mode = RANDOM;
  else if (!strcasecmp(mode_arg, "ship") || !strcasecmp(mode_arg, "ships"))
    bp->mode = SHIPS;
  else if (!strcasecmp(mode_arg, "flat"))
    bp->mode = FLAT;
  else
    {
      fprintf (stderr, "%s: mode must be ship, flat or random, not %s\n",
               progname, mode_arg);
      exit (1);
    }

  bp->which_ship = -1;
  if (bp->mode == SHIPS || bp->mode == RANDOM)
    {
      int i;
      bp->dlists = (GLuint *) calloc (countof(all_ships)+1, sizeof(GLuint));
      for (i = 0; i < countof(all_ships); i++)
        {
          const struct gllist *gll = *all_ships[i];

          bp->dlists[i] = glGenLists (1);
          glNewList (bp->dlists[i], GL_COMPILE);

          glMatrixMode(GL_MODELVIEW);
          glPushMatrix();
          glMatrixMode(GL_TEXTURE);
          glPushMatrix();
          glMatrixMode(GL_MODELVIEW);

          if (random() & 1)
            {
              glScalef (-1, 1, 1);
              glTranslatef (-1, 0, 0);
            }
          renderList (gll, MI_IS_WIREFRAME(mi));

          glMatrixMode(GL_TEXTURE);
          glPopMatrix();
          glMatrixMode(GL_MODELVIEW);
          glPopMatrix();
          glEndList ();
        }
    }

  dazzle_randomize (mi);
  reshape_dazzle (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


ENTRYPOINT void
draw_dazzle (ModeInfo *mi)
{
  dazzle_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int x, y;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  mi->polygon_count = 0;

  glTranslatef (0.5, 0.5, 0);

  if (wire)
    glScalef (0.2, 0.2, 1);

  move_grid (mi);

  for (y = -1; y <= 1; y++)
    for (x = -1; x <= 1; x++)
      draw_grid (mi, x, y);

  if (bp->which_ship != -1)
    {
# ifdef USE_IPHONE
      int rot = current_device_rotation();
# endif

      if (wire)
        glColor3f (1, 0, 0);
      else
        {
          glColor3f (0, 0, 0);

          /* Draw into the depth buffer but not the frame buffer */
          glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
          glClear (GL_DEPTH_BUFFER_BIT);
          glEnable (GL_DEPTH_TEST);
        }

# ifdef USE_IPHONE
      glRotatef (90, 0, 0, 1);
      if (rot == 90 || rot == -270)
        glRotatef (180, 0, 0, 1);
# endif

      glPushMatrix();
      glRotatef (90, 1, 0, 0);
      glScalef (0.9, 0.9, 0.9);
      glTranslatef (-0.5, 0, -0.2);

# ifdef USE_IPHONE
      if (rot == 0 || rot == 180 || rot == -180)
        glScalef (1, 1, (GLfloat) MI_HEIGHT(mi) / MI_WIDTH(mi));
      else
# endif
        glScalef (1, 1, (GLfloat) MI_WIDTH(mi) / MI_HEIGHT(mi));

      /* Wave boat horizontally and vertically */
      glTranslatef (cos ((double) bp->frames / 80 * M_PI * speed) / 200,
                    0,
                    cos ((double) bp->frames / 60 * M_PI * speed) / 300);

      glCallList (bp->dlists[bp->which_ship]);
      mi->polygon_count += (*all_ships[bp->which_ship])->points / 3;
      glPopMatrix();

      /* Wave horizon vertically */
      glTranslatef (0,
                    cos ((double) bp->frames / 120 * M_PI * speed) / 200,
                    0);

      if (! wire)
        {
          glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

          /* Black out everything that isn't a ship. */
# if 0
          glBegin (GL_QUADS);
          glVertex3f (-1, -1, 0);
          glVertex3f (-1,  1, 0);
          glVertex3f ( 1,  1, 0);
          glVertex3f ( 1, -1, 0);
          glEnd();
# else
          {
            GLfloat horizon = 0.15;

            glColor3f (0.7, 0.7, 1.0);
            glBegin (GL_QUADS);
            glVertex3f (-1, -1, 0);
            glVertex3f (-1,  horizon, 0);
            glVertex3f ( 1,  horizon, 0);
            glVertex3f ( 1, -1, 0);
            glEnd();

            glColor3f (0.0, 0.05, 0.2);
            glBegin (GL_QUADS);
            glVertex3f (-1, horizon, 0);
            glVertex3f (-1, 1, 0);
            glVertex3f ( 1, 1, 0);
            glVertex3f ( 1, horizon, 0);
            glEnd();
          }
# endif

          glDisable (GL_DEPTH_TEST);
        }
    }

  if (wire)
    {
      glColor3f(0,1,1);
      glLineWidth(4);
      glBegin(GL_LINE_LOOP);
      glVertex3f(-0.5, -0.5, 0);
      glVertex3f(-0.5,  0.5, 0);
      glVertex3f( 0.5,  0.5, 0);
      glVertex3f( 0.5, -0.5, 0);
      glEnd();
      glLineWidth(1);
    }

  glPopMatrix ();

  bp->frames++;
  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_dazzle (ModeInfo *mi)
{
  dazzle_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->nodes) free (bp->nodes);
  if (bp->colors) free (bp->colors);
  if (bp->dlists)
    {
      int i;
      for (i = 0; i < countof(all_ships); i++)
        glDeleteLists (bp->dlists[i], 1);
      free (bp->dlists);
    }
}

XSCREENSAVER_MODULE_2 ("RazzleDazzle", razzledazzle, dazzle)

#endif /* USE_GL */
