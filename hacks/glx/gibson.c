/* gibson, Copyright (c) 2020-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Hacking the Gibson, as per the 1995 classic film, HACKERS.
 *
 * In the movie, this was primarily a practical effect: the towers were
 * edge-lit etched perspex, each about four feet tall.
 */

#define TOWER_FONT "sans-serif bold 48"

#define DEFAULTS	"*delay:	20000       \n" \
			"*groundColor:  #8A2BE2"   "\n" \
			"*towerColor:   #4444FF"   "\n" \
			"*towerText:    #DDDDFF"   "\n" \
			"*towerText2:   #FF0000"   "\n" \
			"*towerFont:  " TOWER_FONT "\n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define release_gibson 0

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "texfont.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPEED	 "1.0"
#define DEF_TEXTURE      "True"
#define DEF_GRID_WIDTH	 "6"
#define DEF_GRID_HEIGHT	 "7"
#define DEF_GRID_DEPTH	 "6"
#define DEF_GRID_SPACING "2.0"
#define DEF_COLUMNS	 "5"

#define GROUND_QUAD_SIZE 30

typedef struct {
  GLfloat x, y, h;
  GLuint fg_dlists[5], bg_dlists[5];
  int fg_polys, bg_polys;
  unsigned int face_mode;   /* 5 bit field */
} tower;

typedef struct {
  GLXContext *glx_context;
  Bool button_down_p;
  rotator *rot, *rot2;
  GLfloat xscroll, yscroll;
  GLfloat oxscroll, oyscroll;

  GLuint ground_dlist;
  GLuint tower_dlist;
  int ground_polys, tower_polys;
  GLfloat ground_y;
  GLfloat billboard_y;
  const char *billboard_text;

  int ntowers;
  tower *towers;
  GLfloat tower_color[4];
  GLfloat tower_color2[4];
  GLfloat edge_color[4];
  GLfloat bg_color[4];
  Bool startup_p;

  struct {
    GLuint texid;
    XCharStruct metrics;
    int width, height;
    texture_font_data *font_data;
    int ascent, descent, em_width;
    char *text;
  } text[2];

} gibson_configuration;

static gibson_configuration *ccs = NULL;

static GLfloat speed;
static Bool do_tex;
static int grid_width;
static int grid_height;
static int grid_depth;
static GLfloat grid_spacing;
static int columns;

static XrmOptionDescRec opts[] = {
  { "-speed",        ".speed",       XrmoptionSepArg, 0 },
  { "-texture",      ".texture",     XrmoptionNoArg,  "True" },
  { "+texture",      ".texture",     XrmoptionNoArg,  "False" },
  { "-grid-width",   ".gridWidth",   XrmoptionSepArg, 0 },
  { "-grid-height",  ".gridHeight",  XrmoptionSepArg, 0 },
  { "-grid-depth",   ".gridDepth",   XrmoptionSepArg, 0 },
  { "-spacing",      ".gridSpacing", XrmoptionSepArg, 0 },
  { "-columns",      ".columns",     XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,        "speed",       "Speed",       DEF_SPEED,        t_Float},
  {&do_tex,       "texture",     "Texture",     DEF_TEXTURE,      t_Bool},
  {&grid_width,   "gridWidth",   "GridWidth",   DEF_GRID_WIDTH,   t_Int},
  {&grid_height,  "gridHeight",  "GridHeight",  DEF_GRID_HEIGHT,  t_Int},
  {&grid_depth,   "gridDepth",   "GridDepth",   DEF_GRID_DEPTH,   t_Int},
  {&grid_spacing, "gridSpacing", "GridSpacing", DEF_GRID_SPACING, t_Float},
  {&columns,      "columns",     "Columns",     DEF_COLUMNS,      t_Int},
};

ENTRYPOINT ModeSpecOpt gibson_opts = {
 countof(opts), opts, countof(vars), vars, NULL};


ENTRYPOINT void
reshape_gibson (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
# ifdef DEBUG
  gluPerspective (30, 1/h, 1, 100);
# else
  gluPerspective (100, 1/h/4,
                  1.0, 
                  20 * grid_depth * 1.5 * (1 + grid_spacing));
# endif

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 1,
             0, 0, 0,
             0, 1, 0);

  glClear(GL_COLOR_BUFFER_BIT);
}


/* Copied from gltrackball.c */
static void
adjust_for_device_rotation (double *x, double *y, double *w, double *h)
{
  int rot = (int) current_device_rotation();
  int swap;

  while (rot <= -180) rot += 360;
  while (rot >   180) rot -= 360;

  if (rot > 135 || rot < -135)		/* 180 */
    {
      *x = *w - *x;
      *y = *h - *y;
    }
  else if (rot > 45)			/* 90 */
    {
      swap = *x; *x = *y; *y = swap;
      swap = *w; *w = *h; *h = swap;
      *x = *w - *x;
    }
  else if (rot < -45)			/* 270 */
    {
      swap = *x; *x = *y; *y = swap;
      swap = *w; *w = *h; *h = swap;
      *y = *h - *y;
    }
}


ENTRYPOINT Bool
gibson_handle_event (ModeInfo *mi, XEvent *event)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  double w = MI_WIDTH(mi);
  double h = MI_HEIGHT(mi);
  double xoff = 0, yoff = 0;

  if (event->xany.type == ButtonPress ||
      event->xany.type == ButtonRelease)
    {
      double x = event->xbutton.x;
      double y = event->xbutton.y;
      adjust_for_device_rotation (&x, &y, &w, &h);
      xoff = (x / w) - 0.5;
      yoff = (event->xbutton.y / h) - 0.5;
      bp->button_down_p = (event->xany.type == ButtonPress);
      bp->oxscroll = xoff;
      bp->oyscroll = yoff;

      return True;
    }
  else if (event->xany.type == MotionNotify)
    {
      double x = event->xmotion.x;
      double y = event->xmotion.y;
      adjust_for_device_rotation (&x, &y, &w, &h);
      xoff = (x / w) - 0.5;
      yoff = (y / h) - 0.5;
      if (bp->button_down_p)
        {
          bp->xscroll += xoff - bp->oxscroll;
          bp->yscroll += yoff - bp->oyscroll;
          bp->oxscroll = xoff;
          bp->oyscroll = yoff;
        }
      return True;
    }

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


static int
draw_ground (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int x, y;
  int cells = 20;
  GLfloat color[4];
  GLfloat color0[4];
  GLfloat cell_size = 1.0;
  GLfloat z = -0.005;

  parse_color (mi, "groundColor", color);
  parse_color (mi, "towerColor", color0);
  color0[0] *= 0.05;
  color0[1] *= 0.05;
  color0[2] *= 0.3;
  color0[3] = 1;

  if (!wire)
    {
      GLfloat fog_color[4] = { 0, 0, 0, 1 };

      glFogi (GL_FOG_MODE, GL_EXP2);
      glFogfv (GL_FOG_COLOR, fog_color);
      glFogf (GL_FOG_DENSITY, 0.015);
      glFogf (GL_FOG_START, -cells/2 * cell_size);
      glEnable (GL_FOG);
    }

  glPushMatrix();
  glScalef (1.0/cells, 1.0/cells, 1);
  glTranslatef (-cells/2.0, -cells/2.0, 0);
  glTranslatef (0.5, 0, 0);

  glBegin (GL_QUADS);	/* clipping quad */
  glColor4fv (color0);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color0);

  glVertex3f (0, 0, z);
  glVertex3f (cells * cell_size, 0, z);
  glVertex3f (cells * cell_size, cells * cell_size, z);
  glVertex3f (0, cells * cell_size, z);
  glEnd();
  polys++;

  glColor4fv (color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);

  for (y = 0; y < cells; y++)
    for (x = 0; x < cells; x++)
      {
        GLfloat a = 0;
        GLfloat b = 1.0/3;
        GLfloat c = 2.0/3;
        GLfloat d = 1.0;
        GLfloat w = 0.02;

        glPushMatrix();
        glTranslatef (x, y, 0);

        glNormal3f (0, 0, 1);

        switch (random() % 4) {
        case 0:
          glRotatef (90, 0, 0, 1);
          glTranslatef (0, -1, 0);
          break;
        case 1:
          glRotatef (-90, 0, 0, 1);
          glTranslatef (-1, 0, 0);
          break;
        case 2:
          glRotatef (180, 0, 0, 1);
          glTranslatef (-1, -1, 0);
          break;
        default: break;
        }

        switch (random() % 2) {
        case 0:
          glScalef (-1, -1, 1);
          glTranslatef (-1, -1, 0);
          break;
        default: break;
        }

        switch (random() % 2) {
        case 0:

          glBegin (wire ? GL_LINE_LOOP : GL_QUAD_STRIP);
          glVertex3f (a, b+w, 0);
          glVertex3f (a, b-w, 0); polys++;
          glVertex3f (b+w, a, 0); polys++;
          glVertex3f (b-w, a, 0); polys++;
          glEnd();

          glBegin (wire ? GL_LINE_LOOP : GL_QUAD_STRIP);
          glVertex3f (a,   c+w, 0);
          glVertex3f (a,   c-w, 0); polys++;
          glVertex3f (b+w, c+w, 0); polys++;
          glVertex3f (b,   c-w, 0); polys++;
          glVertex3f (c+w, b+w, 0); polys++;
          glVertex3f (c-w, b,   0); polys++;
          glVertex3f (c+w, a,   0); polys++;
          glVertex3f (c-w, a,   0); polys++;
          glEnd();

/*
          glBegin (wire ? GL_LINE_LOOP : GL_QUAD_STRIP);
          glVertex3f (c+w, d,   0);
          glVertex3f (c-w, d,   0); polys++;
          glVertex3f (d,   c+w, 0); polys++;
          glVertex3f (d,   c-w, 0); polys++;
          glEnd();
*/
          break;

        default:
          glBegin (wire ? GL_LINE_LOOP : GL_QUAD_STRIP);
          glVertex3f (a+w, d,   0);
          glVertex3f (a,   d,   0); polys++;
          glVertex3f (a+w, d,   0);
          glVertex3f (a,   d-w, 0); polys++;
          glVertex3f (b+w, c-w, 0); polys++;
          glVertex3f (b-w, c-w, 0); polys++;
          glVertex3f (b+w, a,   0); polys++;
          glVertex3f (b-w, a,   0); polys++;
          glEnd();

          glBegin (wire ? GL_LINE_LOOP : GL_QUAD_STRIP);
          glVertex3f (b+w, d,   0);
          glVertex3f (b-w, d,   0); polys++;
          glVertex3f (c+w, c-w, 0); polys++;
          glVertex3f (c-w, c-w, 0); polys++;
          glVertex3f (c+w, a,   0); polys++;
          glVertex3f (c-w, a,   0); polys++;
          glEnd();
          break;
        }

        glPopMatrix();
      }
  glPopMatrix();

  if (!wire)
    {
      glDisable (GL_BLEND);
      glDisable (GL_FOG);
    }

  return polys;
}


/* qsort comparator for sorting towers by y position */
static int
cmp_towers (const void *aa, const void *bb)
{
  const tower *a = (tower *) aa;
  const tower *b = (tower *) bb;
  return ((int) (b->y * 10000) -
          (int) (a->y * 10000));
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


/* Draws the text quads on the face.
   First pass is for the small background text, second is for the big block.
 */
static int
draw_tower_face_text (ModeInfo *mi, GLfloat height, Bool which)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  Bool wire2 = False;   /* Debugging quads */
  Bool bg_p = (which == 1 && do_tex && !wire);

  /* The texture is a tex_width x tex_height rectangle, of which we 
     only use the rbearing+lbearing x ascent+descent sub-rectangle.
     Texture coordinates reference the tex_width x tex_height rectangle
     as a 0.0 - 1.0 coordinate.
   */
  int n = which ? 1 : 0;
  GLfloat twratio = ((bp->text[n].metrics.rbearing +
                      bp->text[n].metrics.lbearing) /
                     (GLfloat) bp->text[n].width);
  GLfloat thratio = ((bp->text[n].metrics.ascent +
                      bp->text[n].metrics.descent) /
                     (GLfloat) bp->text[n].height);
  GLfloat aspect = ((bp->text[n].ascent + bp->text[n].descent) /
                    (GLfloat) bp->text[n].em_width);

  GLfloat sx  = 1.0 / (which ? 1 : columns);
  GLfloat sy  = (which
                 ? height * 0.8
                 : sx * 4);  /* Tweaked to match gluPerspective */

  GLfloat lines_in_tex = ((bp->text[n].metrics.ascent +
                           bp->text[n].metrics.descent) /
                          (GLfloat)
                          (bp->text[n].ascent + bp->text[n].descent));
  GLfloat tex_lines = (which ? 3 : 8);  /* Put this many lines in each quad */

  GLfloat tsx = sx * twratio;
  GLfloat tsy = sy * thratio * tex_lines / lines_in_tex * aspect;
  GLfloat x1, tx1;
  GLfloat margin = 0.2;
  GLfloat m2 = margin/2 / (which ? 1 : columns);
  GLfloat m3 = m2 / (which ? 1 : height);
  GLfloat h2 = height * (which ? 1-margin : 1);

  glColor4fv (which ? bp->tower_color2 : bp->tower_color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, 
                which ? bp->tower_color2 : bp->tower_color);

  glBindTexture (GL_TEXTURE_2D, bp->text[n].texid);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  if (!wire && !wire2 && !bg_p) glBegin (GL_QUADS);
  for (x1 = 0, tx1 = 0; x1 < 1.0; x1 += sx, tx1 += tsx)
    {
      GLfloat x2  = x1 + sx;
      GLfloat tx2 = tx1 + tsx;
      GLfloat y2, ty2;
      GLfloat z = (which ? 0.05 : 0);

      tx1 = 0;
      tx2 = twratio;

      for (y2 = h2, ty2 = thratio;
           y2 > 0;
           y2 -= sy, ty2 -= tsy)
        {
          GLfloat y1  = y2 - sy * (1-margin);
          GLfloat ty1 = ty2 - tsy;
          GLfloat toff = frand ((bp->text[n].metrics.ascent + 
                                 bp->text[n].metrics.descent)
                                * 0.8);

          if (y1 < 0)  /* Clip the panel to the bottom of the tower face */
            {
              tsy = y2 / (y2 - y1);
              y1 = 0;
            }

          ty1 = toff;
          ty2 = ty1 + tsy;

          if (wire2 && which) glColor3f (1,0,0);
          if (wire || wire2 || bg_p)
            glBegin (!wire && (wire2 || bg_p) ? GL_QUADS : GL_LINE_LOOP);
          glTexCoord2f(tx1, ty2); glVertex3f (x1+m2, y1+m3, z);
          glTexCoord2f(tx2, ty2); glVertex3f (x2-m2, y1+m3, z);
          glTexCoord2f(tx2, ty1); glVertex3f (x2-m2, y2-m3, z);
          glTexCoord2f(tx1, ty1); glVertex3f (x1+m2, y2-m3, z);
          if (wire || wire2 || bg_p)
            glEnd();
          polys++;

          if (bg_p)
            {
              GLfloat bg[4] = { 1, 1, 1, 0.2 };
              z  -= 0.1;
              m2 -= 0.03;
              m3 -= 0.03;
              glColor4fv (bg);
              glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bg);
            }

          if ((wire2 || bg_p) && !wire)
            {
              if (do_tex) glDisable(GL_TEXTURE_2D);
              glBegin (bg_p ? GL_QUADS : GL_LINE_LOOP);
              glVertex3f (x1+m2, y1+m3, z);
              glVertex3f (x2-m2, y1+m3, z);
              glVertex3f (x2-m2, y2-m3, z);
              glVertex3f (x1+m2, y2-m3, z);
              glEnd();
              polys++;
              if (do_tex) glEnable(GL_TEXTURE_2D);
            }

          if (which) break;
        }
    }
  if (!wire && !wire2 && !bg_p) glEnd();

  return polys;
}


/* Draws the wall of the face, and the edges, then the text quads on it.
 */
static int
draw_tower_face (ModeInfo *mi, GLfloat height, int mode)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;

  switch (mode) {
  case 0:
    if (! wire)
      {
        GLfloat m =  0.015;
        GLfloat z = -0.0005;
        if (do_tex) glDisable (GL_TEXTURE_2D);

        glNormal3f (0, 0, 1);

        glColor4fv (bp->bg_color);
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                      bp->bg_color);

        glBegin (GL_QUADS);
        glVertex3f (0, 0, z*2);			/* background */
        glVertex3f (1, 0, z*2);
        glVertex3f (1, height, z*2);
        glVertex3f (0, height, z*2);
        polys++;
        glEnd();

        glColor4fv (bp->edge_color);
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                      bp->edge_color);

        glBegin (GL_QUADS);
        glVertex3f (0, 0, z);			/* left */
        glVertex3f (m, 0, z);
        glVertex3f (m, height, z);
        glVertex3f (0, height, z);
        polys++;

        glVertex3f (1-m, 0, 0);			/* right */
        glVertex3f (1,   0, 0);
        glVertex3f (1,   height, z);
        glVertex3f (1-m, height, z);
        polys++;

        glVertex3f (m,   0, 0);			/* bottom */
        glVertex3f (1-m, 0, 0);
        glVertex3f (1-m, m, 0);
        glVertex3f (m,   m, 0);
        polys++;

        glVertex3f (m,   height-m, z);		/* top */
        glVertex3f (1-m, height-m, z);
        glVertex3f (1-m, height,   z);
        glVertex3f (m,   height,   z);
        polys++;
        glEnd();

        if (do_tex) glEnable (GL_TEXTURE_2D);
      }
    break;
  case 1:
    polys += draw_tower_face_text (mi, height, 0);
    break;
  case 2:
    polys += draw_tower_face_text (mi, height * 0.7, 1);
    break;
  default:
    abort();
    break;
  }

  return polys;
}


/* Mode 0: draws 5 sides of the box
   Mode 1: just background text
   Mode 2: just foreground text
 */
static int
draw_tower (ModeInfo *mi, tower *t, int mode, int face)
{
  GLfloat height = grid_height;

  int polys = 0;

  glPushMatrix();
  glTranslatef (-0.5, 0.5, 0);

  if (face == 0 || face == -1)
    {
      glPushMatrix();				/* top */
      glTranslatef (0, 0, height);
      polys += draw_tower_face (mi, 1.0, mode);
      glPopMatrix();
    }

  if (face == 1 || face == -1)
    {
      glPushMatrix();				/* left */
      glRotatef ( 90, 1, 0, 0);
      glRotatef (-90, 0, 1, 0);
      glTranslatef (-1, 0, 0);
      polys += draw_tower_face (mi, height, mode);
      glPopMatrix();
    }

  if (face == 2 || face == -1)
    {
      glPushMatrix();				/* back */
      glRotatef ( 90, 1, 0, 0);
      glRotatef (180, 0, 1, 0);
      glTranslatef (-1, 0, 1);
      polys += draw_tower_face (mi, height, mode);
      glPopMatrix();
    }

  if (face == 3 || face == -1)
    {
      glPushMatrix();				/* right */
      glRotatef ( 90, 1, 0, 0);
      glRotatef ( 90, 0, 1, 0);
      glTranslatef (0, 0, 1);
      polys += draw_tower_face (mi, height, mode);
      glPopMatrix();
    }

  if (face == 4 || face == -1)
    {
      glPushMatrix();				/* front */
      glRotatef ( 90, 1, 0, 0);
      polys += draw_tower_face (mi, height, mode);
      glPopMatrix();
    }

  if (face < -1 || face > 4) abort();

  glPopMatrix();
  return polys;
}


static void
animate_towers (ModeInfo *mi)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  int ii;
  GLfloat min = -3;
  GLfloat max = grid_depth * (1 + grid_spacing) - grid_spacing - 1;
  GLfloat yspeed = speed * 0.05;

  for (ii = 0; ii < 20; ii++)
    {
      int jj, kk;

      /* randomly trade two towers' fg dlists */
      if (0 == (random() % 20))
        {
          int i = random() % bp->ntowers;
          int j = random() % bp->ntowers;
          int k = random() % countof(bp->towers[i].fg_dlists);
          GLuint d1 = bp->towers[i].bg_dlists[k];
          GLuint d2 = bp->towers[j].bg_dlists[k];
          bp->towers[i].bg_dlists[k] = d2;
          bp->towers[j].bg_dlists[k] = d1;
        }

      /* randomly trade two towers' bg dlists */
      if (1) /* (0 == (random() % 3)) */
        {
          int i = random() % bp->ntowers;
          int j = random() % bp->ntowers;
          int k = random() % countof(bp->towers[i].fg_dlists);
          GLuint d1 = bp->towers[i].fg_dlists[k];
          GLuint d2 = bp->towers[j].fg_dlists[k];
          bp->towers[i].fg_dlists[k] = d2;
          bp->towers[j].fg_dlists[k] = d1;
        }

      /* Randomize whether it's displaying fg text or bg text */
      for (jj = 0; jj < bp->ntowers; jj++)
        for (kk = 0; kk < countof(bp->towers[jj].fg_dlists); kk++)
          {
            /* Re-choose every N frames.  Display fg 1 in M. */
            int frames = 500;
            int fg_chance = (kk == 0 ? 100000 : 10);
            unsigned int o = !!(bp->towers[jj].face_mode & (1 << kk));
            unsigned int n = !!((random() % frames) ? o : 
                                (0 == (random() % fg_chance)));
            bp->towers[jj].face_mode =
              ((bp->towers[jj].face_mode & ~(1 << kk)) | (n << kk));
          }
    }

  for (ii = 0; ii < bp->ntowers; ii++)
    {
      tower *t = &bp->towers[ii];
      t->h += speed * 0.01;
      if (t->h > 1) t->h = 1;

      t->y -= yspeed;

      if (t->y < min)
        {
          t->h = 0;
          t->y = max;
        }
    }

  /* Sorting by depth improves frame rate slightly. */
  qsort (bp->towers, bp->ntowers, sizeof(*bp->towers), cmp_towers);

  bp->ground_y -= yspeed / GROUND_QUAD_SIZE;
  if (bp->ground_y < 1)
    bp->ground_y += 1;

  bp->billboard_y -= yspeed;
  if (bp->billboard_y < min || !bp->billboard_text)
    {
      const char *const ss[] = {
        "ACCESS GRANTED",
        "ACCESS GRANTED",
        "ACCESS DENIED",
        "ACCESS DENIED",
        "ACCESS DENIED",
        "ACCESS DENIED",
        "ACCESS DENIED",
        "PASSWORD ACCEPTED",
        " GIVE ME\nA COOKIE",
        "MESS WITH THE BEST\n  DIE LIKE THE REST",
      };

      bp->billboard_y = max * (1 + frand(8));
      bp->billboard_text = ss[random() % countof(ss)];
    }
}


static int
draw_billboard (ModeInfo *mi)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  int polys = 0;
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat w, h, s, margin, margin2;
  XCharStruct metrics;
  int ascent, descent;
  texture_font_data *font = bp->text[1].font_data;
  GLfloat color[4];
  GLfloat y = grid_height * 0.3;

  texture_string_metrics (font, bp->billboard_text,
                          &metrics, &ascent, &descent);
  w = metrics.lbearing + metrics.rbearing;
  h = metrics.ascent   + metrics.descent;
  s = 1.0 / w;
  s *= 0.95;

  margin = w * 0.1;
  margin2 = margin * 1.7;

  glPushMatrix();

  glTranslatef (-0.5, bp->billboard_y, y);
  glRotatef (90, 1, 0, 0);
  glScalef (s, s * 1.5, s);

  memcpy (color, bp->tower_color2, sizeof(color));
  color[3] = 0.6;
  glColor4fv (color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);

  if (do_tex && !wire)
    glDisable (GL_TEXTURE_2D);

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f (0, 0, 1);
  glVertex3f (-margin,  -margin2,  0);
  glVertex3f (-margin,  h+margin2, 0);
  glVertex3f (w+margin, h+margin2, 0);
  glVertex3f (w+margin, -margin2,  0);
  glEnd();
  polys++;

  if (do_tex && !wire)
    {
      color[3] = 1;
      glColor4fv (color);
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
      glEnable (GL_TEXTURE_2D);
      glTranslatef (-metrics.lbearing, metrics.descent, 0);
      print_texture_string (font, bp->billboard_text);
      polys++;
    }

  glPopMatrix();
  return polys;
}


static void
init_text (ModeInfo *mi)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  int lines = 20;
  int i;
  char *s;

  const char *const ss[] = {
   "\n"
   "ACCESS TO THIS COMPUTER AND\n"
   "ITS DATA IS RESTRICTED TO\n"
   "AUTHORIZED PERSONNEL ONLY\n"
   "\n",
   "\n"
   "  PASSWORD ACCEPTED\n"
   "             GOD\n"
   "\n",
   "PERSONNEL   >>>\n",
   "SEA ROUTINGS   >>>\n",
   "GARBAGE   >>>\n",
   "COMP. SERVICING   >>>\n",
   "COMPANY BUDGETS   >>>\n",
   "SCIENTIFIC BUDGETS   >>>\n",
   "COMPANY POLICIES   >>>\n",
   "ANNUAL RETURNS   >>>\n",
   "MINE RESEARCH   >>>\n",
   "CENTRAL LIBRARY   >>>\n",
   "QUANTATIVE SPEC.   >>>\n",
   "PAYMENT LEVELS   >>>\n",
   "CENTRAL SERVER   >>>\n",
   "GARBAGE   >>>\n",
   "KNMTS. DVPNT.   >>>\n",
   "LICENSING   >>>\n",
   "RELATIONS   >>>\n",
   "TIME SHEET RECS.   >>>\n",
   "RD. PRT. ROUTINGS   >>>\n",
   "RECRUITMENT   >>>\n",
   "TNKR. EXPENDITURE   >>>\n",
   "MINE DEVELOPMENT   >>>\n",
   "GARBAGE   >>>\n",
   "ANNUAL BUDGETS   >>>\n",
   "OIL LOCATIONS   >>>\n",
   "TIME SHEET RECS.   >>>\n",
   "RD. PRT. ROUTINGS   >>>\n",
   "KINEMATICS   >>>\n",
   "TPS. REPORTS   >>>\n",
   "BLAST FRNC. STATUS   >>>\n",
   "ACCOUNTANTS   >>>\n",
   "SHIPPING FORCASTS   >>>\n",
   "INDST. REPORTS   >>>\n",
   "EXPLOR. DVLT.   >>>\n",
   "WRHSE. EXPEND.   >>>\n",
   "GARBAGE   >>>\n",
   "RELOCATIONS   >>>\n",
   "AIRFREIGHT STATUS   >>>\n",
   "TPGC. EXPEND.   >>>\n",
   "SEA-BOARD LAWS   >>>\n",
   "COMPOSITE PLANTS   >>>\n",
   "NUCLEAR RESEARCH   >>>\n",
   "BALLAST REPORTS   >>>\n",
   "\n"
   "CONFIDENTAL\n"
   "FILES\n"
   "DO NOT DELETE\n"
   "BEFORE FINAL\n"
   "BACKUP IS COMPLETED\n"
   "\n",
   "\n"
   "FILE 1\n"
   "WAITING FOR BACK-UP\n"
   "\n"
   "FILE 2\n"
   "WAITING FOR BACK-UP\n"
   "\n"
   "FILE 3\n"
   "WAITING FOR BACK-UP\n"
   "\n"
   "FILE 4\n"
   "WAITING FOR BACK-UP\n"
   "\n"
   };


  bp->text[1].text = s = calloc (countof(ss) * 2 * 40, 1);
  for (i = 0; i < countof(ss); i++)
    {
      int n = random() % countof(ss);
      strcat (s, ss[n]);
      s += strlen(s);
    }

  bp->text[0].text = s = calloc (lines * 40, 1);
  for (i = 0; i < lines; i++)
    {
      switch (random() % 11) {
      case 0: sprintf (s, "%X\n", random() % 0xFFFFFFFF); break;
      case 1: sprintf (s, "%X\n", random() % 0xFFFFFF); break;
      case 2: sprintf (s, "%X\n", random() % 0xFFFF); break;
      case 3: sprintf (s, "%d\n", random() % 0xFFFFFF); break;
      case 4: sprintf (s, "%d\n", random() % 0xFFFF); break;
      case 5: sprintf (s, "%d\n", random() % 0xFFF); break;
      case 6: strcat (s, "00000000\n"); break;
      case 7: sprintf (s, "{{{{{{{{\n"); break;
      case 8: sprintf (s, "[][][][][][]\n"); break;
      case 9: sprintf (s, "DEFAULT\n"); break;
      case 10: sprintf (s, "\n"); break;
      }
      s += strlen(s);
    }
}


static void
init_textures (ModeInfo *mi)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < countof(bp->text); i++)
    {
      glGenTextures (1, &bp->text[i].texid);
      glBindTexture (GL_TEXTURE_2D, bp->text[i].texid);
      texture_string_metrics (bp->text[i].font_data, " ",
                              &bp->text[i].metrics,
                              &bp->text[i].ascent,
                              &bp->text[i].descent);
      bp->text[i].em_width = bp->text[i].metrics.width;
      string_to_texture (bp->text[i].font_data, bp->text[i].text,
                         &bp->text[i].metrics,
                         &bp->text[i].width,
                         &bp->text[i].height);
    }
  glBindTexture (GL_TEXTURE_2D, 0);
}


ENTRYPOINT void 
init_gibson (ModeInfo *mi)
{
  gibson_configuration *bp;

  MI_INIT (mi, ccs);

  bp = &ccs[MI_SCREEN(mi)];

  if ((bp->glx_context = init_GL(mi)) != NULL) {
    reshape_gibson (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  parse_color (mi, "towerText",  bp->tower_color);
  parse_color (mi, "towerText2", bp->tower_color2);
  parse_color (mi, "towerColor", bp->bg_color);
  memcpy (bp->edge_color, bp->bg_color, sizeof(bp->tower_color));
  bp->edge_color  [3] = 0.7;
  bp->bg_color    [3] = 1.0;
  bp->tower_color [3] = 1.0;
  bp->tower_color2[3] = 1.0;

  if (grid_spacing < 1) grid_spacing = 1;
  if (grid_width   < 1) grid_width   = 1;
  if (grid_height  < 1) grid_height  = 1;
  if (grid_depth   < 1) grid_depth   = 1;
  if (columns      < 1) columns      = 1;
  bp->ntowers = grid_width * grid_depth;
  bp->towers = (tower *) calloc (sizeof(tower), bp->ntowers);
  bp->startup_p = True;

  {
    double wander_speed = 0.007 * speed;
    double tilt_speed   = 0.01 * speed;
    bp->rot  = make_rotator (0, 0, 0, 0, wander_speed, True);
    bp->rot2 = make_rotator (0, 0, 0, 0, tilt_speed,   True);
  }

  bp->text[0].font_data = load_texture_font (mi->dpy, "towerFont");
  bp->text[1].font_data = load_texture_font (mi->dpy, "towerFont");
  init_text (mi);
  init_textures (mi);

  bp->ground_dlist = glGenLists (1);
  glNewList (bp->ground_dlist, GL_COMPILE);
  bp->ground_polys = draw_ground (mi);
  glEndList ();

  bp->tower_dlist = glGenLists (1);
  glNewList (bp->tower_dlist, GL_COMPILE);
  bp->tower_polys = draw_tower (mi, &bp->towers[0], 0, -1);
  glEndList ();

  {
    int x, y;
    GLfloat ww = grid_width * (1 + grid_spacing) - grid_spacing;
    GLfloat hh = grid_depth * (1 + grid_spacing) - grid_spacing;
    for (y = 0; y < grid_depth; y++)
      for (x = 0; x < grid_width; x++)
        {
          int i;
          tower *t = &bp->towers[y * grid_width + x];
          t->x = (x * ww / (grid_width - 1)) - ww/2;
          t->y = (y * hh / grid_depth) + 6;
          t->h = 0 - y / (GLfloat) grid_depth / 2;

          for (i = 0; i < countof(t->fg_dlists); i++)
            {
              t->bg_dlists[i] = glGenLists (1);
              glNewList (t->bg_dlists[i], GL_COMPILE);
              t->bg_polys = draw_tower (mi, t, 1, i);
              glEndList ();

              t->fg_dlists[i] = glGenLists (1);
              glNewList (t->fg_dlists[i], GL_COMPILE);
              t->fg_polys += draw_tower (mi, t, 2, i);
              glEndList ();
            }
        }
  }

  animate_towers (mi);
}


ENTRYPOINT void
draw_gibson (ModeInfo *mi)
{
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat s;
  int i;

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat bshiny    = 128.0;
  GLfloat bcolor[4] = { 0.7, 0.7, 1.0, 1.0 };

  if (!bp->glx_context)
    return;

  mi->polygon_count = 0;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel (GL_SMOOTH);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);
  glDisable (GL_TEXTURE_2D);
  glEnable (GL_DEPTH_TEST);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  glPushMatrix ();

  glRotatef(current_device_rotation(), 0, 0, 1);

# ifdef DEBUG
  s = 0.02;
# else
  s = 10;
# endif

  glScalef (s, s, s);

  glTranslatef (0, -1, 0);

# ifndef DEBUG
  glRotatef (-82, 1, 0, 0);

  {
    double maxx = 40;   /* up/down */
    double maxy = 1.5;  /* tilt */
    double maxz = 100;  /* left/right */

    double x, y, z;
    double minh = -(grid_height / 2.0);
    double maxh = -(grid_height / 20.0);

    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    x -= 0.5;
    z = minh + (z * (maxh - minh));
    glTranslatef(x * grid_spacing * 0.005, 0, z);

    get_position (bp->rot2, &x, &y, &z, !bp->button_down_p);

    z += (bp->xscroll / 2.0);
    x += (bp->yscroll / 2.0);

    glRotatef (maxx/2 - x*maxx, 1, 0, 0);
    glRotatef (maxy/2 - y*maxy, 0, 1, 0);
    glRotatef (maxz/2 - z*maxz, 0, 0, 1);
  }
# endif /* DEBUG */

  glPushMatrix();
  glScalef (GROUND_QUAD_SIZE, GROUND_QUAD_SIZE, 1);

  glTranslatef (0, bp->ground_y - 1.5, 0);
  glCallList (bp->ground_dlist);
  mi->polygon_count += bp->ground_polys;

  glTranslatef (0, 1, 0);
  glCallList (bp->ground_dlist);
  mi->polygon_count += bp->ground_polys;
  glPopMatrix();

  glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
  glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);

  glPushMatrix();

  if (grid_width & 1)  /* Stay between towers */
    glTranslatef ((grid_spacing + 1) / 2.0, 0, 0);


  if (!wire)
    {
      GLfloat fog_color[4] = { 0, 0, 0, 1 };
      glFogfv (GL_FOG_COLOR, fog_color);
      /* I so don't understand how to choose the fog parameters. */
      glFogi (GL_FOG_MODE, GL_LINEAR);
      glFogf (GL_FOG_START, 0);
      glFogf (GL_FOG_END, 100);
      glEnable (GL_FOG);
    }

  /* Clear the floor under the tower bases */

  {
    GLfloat color0[4] = { 0, 0, 0, 1 };
    GLfloat z = 0.01;

    if (do_tex && !wire) glDisable (GL_TEXTURE_2D);
    glColor4fv (color0);
    glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color0);
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);

    for (i = 0; i < bp->ntowers; i++)
      {
        tower *t = &bp->towers[i];
        glPushMatrix();
        glTranslatef (t->x, t->y, 0);

        glNormal3f (0, 0, 1);
        glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* clipping quad */
        glVertex3f (-0.5, -0.5, z);
        glVertex3f ( 0.5, -0.5, z);
        glVertex3f ( 0.5,  0.5, z);
        glVertex3f (-0.5,  0.5, z);
        glEnd();
        mi->polygon_count++;
        glPopMatrix();
      }
  }

  glEnable (GL_DEPTH_TEST);

  if (!wire)
    {
      if (do_tex)
        {
          glEnable (GL_TEXTURE_2D);
          enable_texture_string_parameters (bp->text[0].font_data);
        }
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE);
      glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
      glDisable (GL_CULL_FACE);
      if (bp->startup_p)
        glEnable (GL_DEPTH_TEST);
      else
        glDisable (GL_DEPTH_TEST);
    }

  /* Draw the towers */

  for (i = 0; i < bp->ntowers; i++)
    {
      tower *t = &bp->towers[i];
      glPushMatrix();
      glTranslatef (t->x, t->y-1, -grid_height * ease_ratio (1 - t->h));

      glCallList (bp->tower_dlist);
      mi->polygon_count += bp->tower_polys;

      if (wire || do_tex)
        {
          int j;
          for (j = 0; j < countof(t->fg_dlists); j++)
            {
              if (! (t->face_mode & (1 << j)))
                {
                  glCallList (t->bg_dlists[j]);
                  mi->polygon_count += t->bg_polys;
                }
              else
                {
                  glCallList (t->fg_dlists[j]);
                  mi->polygon_count += t->fg_polys;
                }
            }
        }
      glPopMatrix();
    }

  glPopMatrix();

  mi->polygon_count += draw_billboard (mi);
  glPopMatrix();

  if (!bp->button_down_p)
    animate_towers (mi);

  if (bp->startup_p && bp->towers[bp->ntowers-1].h >= 1)
    bp->startup_p = False;

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_gibson (ModeInfo *mi)
{
  int i, j;
  gibson_configuration *bp = &ccs[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->rot)  free_rotator (bp->rot);
  if (bp->rot2) free_rotator (bp->rot2);
  if (glIsList(bp->ground_dlist)) glDeleteLists(bp->ground_dlist, 1);
  if (glIsList(bp->tower_dlist)) glDeleteLists(bp->tower_dlist, 1);
  for (i = 0; i < countof(bp->text); i++)
    {
      if (bp->text[i].font_data) free_texture_font (bp->text[i].font_data);
      if (bp->text[i].text) free (bp->text[i].text);
    }
  if (bp->towers)
    for (i = 0; i < bp->ntowers; i++)
      {
        for (j = 0; j < countof(bp->towers[i].fg_dlists); j++)
          {
            if (glIsList(bp->towers[i].fg_dlists[j]))
              glDeleteLists(bp->towers[i].fg_dlists[j], 1);
            if (glIsList(bp->towers[i].bg_dlists[j]))
              glDeleteLists(bp->towers[i].bg_dlists[j], 1);
          }
      }
}


XSCREENSAVER_MODULE ("Gibson", gibson)
/* Greets to Crash Override, The Phantom Freak, and also Joey */

#endif /* USE_GL */
