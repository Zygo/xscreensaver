/* graphstat, Copyright (c) 2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Inspired by the After Dark saver of the same name by Harry Chesley and
 * Rob Vaterlaus in 1990-1993.
 */

#define DEFAULTS	"*delay:	30000        \n" \
			"*count:	5            \n" \
			"*showFPS:      False        \n" \
			"*titleFont:    sans-serif 24\n" \
			"*suppressRotationAnimation: True\n" \

# define release_graphstat 0

#include "xlockmore.h"
#include "texfont.h"
#include "hsv.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)
#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)
#undef MAX
#define MAX(A,B) ((A)>(B)?(A):(B))

#define DEF_SPEED       "1.0"
#define DEF_THICKNESS	"3"
#define DEF_DURATION	"5"

#define SUBTITLE_SCALE 0.7

/* ENABLE_NLS: define this blank so that we can use it to tag static strings.
   We call gettext manually. */
# undef _
# define _(S) S

static const unsigned char graphstat_words[] =
#include "graphstat.h"
;

typedef struct {
  struct { GLfloat x, y, z; } *points;
  int i, count;
  GLfloat color[4], color2[4];
} graph;

typedef struct {
  char **strings;
  int count;
} string_set;

typedef struct {
  GLXContext *glx_context;

  int count;
  graph *graph;
  GLfloat grid_w, grid_h, grid_logx, grid_logy, grid_cx, grid_cy;
  GLfloat color[4];

  enum { INIT, FADE_IN, DRAW, TEXT, FADE_OUT } state;
  GLfloat tick;

  texture_font_data *font_data;
  string_set strings[4];
  char *labels[2];
  GLfloat text_x, text_y, text_x2, text_y2;
  GLfloat text_alpha;

} graphstat_configuration;

static graphstat_configuration *bps = NULL;

static GLfloat speed_arg;
static GLfloat thickness;
static GLfloat duration;

static XrmOptionDescRec opts[] = {
  { "-speed",     ".speed",     XrmoptionSepArg, 0 },
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
  { "-duration",  ".duration",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed_arg, "speed",     "Speed",     DEF_SPEED,     t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
  {&duration,  "duration",  "Duration",  DEF_DURATION,  t_Float},
};

ENTRYPOINT ModeSpecOpt graphstat_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


static void reset_graphstat (ModeInfo *mi);

ENTRYPOINT void
reshape_graphstat (ModeInfo *mi, int width, int height)
{
  glViewport (0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, width, 0, height, 0, 1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClear(GL_COLOR_BUFFER_BIT);

  reset_graphstat (mi);
}


ENTRYPOINT Bool
graphstat_handle_event (ModeInfo *mi, XEvent *event)
{
  graphstat_configuration *bp = &bps[MI_SCREEN(mi)];

  if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      if (bp->state != FADE_OUT)
        {
          bp->state = FADE_OUT;
          bp->tick = 0;
        }
      return True;
    }

  return False;
}


static void
reset_graphstat (ModeInfo *mi)
{
  graphstat_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (bp->graph)
    {
      for (i = 0; i < MI_COUNT(mi); i++)
        free (bp->graph[i].points);
      free (bp->graph);
    }

  bp->graph = (graph *) calloc (MI_COUNT(mi), sizeof(*bp->graph));

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      int n = MI_WIDTH(mi);
      bp->graph[i].count = n;
      bp->graph[i].points = calloc (n, sizeof (*bp->graph[i].points));
    }

  bp->state = INIT;
  bp->tick = 0;
}


/* Things someone is going to complain about eventually,
   but about which I do not care:

   - There is no way to load an external file.
   - It blows up if there aren't four sections.
   - It expects the file to end with a newline.
 */
static void
init_strings (ModeInfo *mi)
{
  graphstat_configuration *bp = &bps[MI_SCREEN(mi)];
  const char *head = (const char *) graphstat_words;
  const char *tail = head;
  int i = 0, j = 0;

  memset (bp->strings, 0, sizeof(bp->strings));

  /* Count them */
  while (*tail)
    {
      const char *eol = strchr (tail, '\n');
      bp->strings[i].count++;
      if (!eol) break;
      eol++;
      if (*eol == '\n')
        {
          i++;
          eol++;
          if (i >= countof(bp->strings))
            break;
        }
      tail = eol;
    }

  /* Allocate space */
  for (i = 0; i < countof(bp->strings); i++)
    bp->strings[i].strings = (char **)
      calloc (bp->strings[i].count + 1, sizeof(*bp->strings[i].strings));

  /* Fill arrays */
  tail = head;
  i = 0;
  j = 0;
  while (*tail)
    {
      const char *eol = strchr (tail, '\n');
      char *s, *s2;
      int L;
      Bool done = False;

      if (!eol) break;
      eol++;
      if (*eol == '\n')
        {
          eol++;
          done = True;
        }

      L = eol - tail;
      s = (char *) malloc (L);
      memcpy (s, tail, L);
      s[--L] = 0;
      while (s[L-1] == '\n')
        s [--L] = 0;
      s2 = strdup (gettext (s));
      free (s);
      bp->strings[i].strings[j] = s2;
      j++;
      tail = eol;

      if (done)
        {
          i++;
          j = 0;
          if (i >= countof(bp->strings))
            break;
        }
    }
}


ENTRYPOINT void 
init_graphstat (ModeInfo *mi)
{
  graphstat_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  if (speed_arg < 0.01) speed_arg = 0.01;
  if (thickness < 0.1) thickness = 0.1;
  if (duration < 1) duration = 1;
  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;

  bp->font_data = load_texture_font (mi->dpy, "titleFont");

  init_strings (mi);
  reshape_graphstat (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


static void
hsv_to_gl (int h, GLfloat s, GLfloat v, GLfloat out[3])
{
  unsigned short r, g, b;
  if (v < 0) v = 0; else if (v > 1) v = 1;
  if (s < 0) s = 0; else if (s > 1) s = 1;
  hsv_to_rgb (h, s, v, &r, &g, &b);
  out[0] = r / 65536.0;
  out[1] = g / 65536.0;
  out[2] = b / 65536.0;
}


static void
gl_to_hsv (GLfloat color[4], int *h_ret, GLfloat *s_ret, GLfloat *v_ret)
{
  double s, v;
  rgb_to_hsv (color[0] * 65536,
              color[1] * 65536,
              color[2] * 65536,
              h_ret, &s, &v);
  *s_ret = s;
  *v_ret = s;
}


static void
tick_graphstat (ModeInfo *mi)
{
  graphstat_configuration *bp = &bps[MI_SCREEN(mi)];
  XCharStruct e, e2;
  int i, j, k;

  switch (bp->state) {
  case INIT:

    bp->count = (BELLRAND(MI_COUNT(mi) * 2) + 1) - MI_COUNT(mi) + 1;
    if (bp->count < 1)
      bp->count = 1;
    else if (bp->count > MI_COUNT(mi))
      bp->count = MI_COUNT(mi);

    /* Grid */

    {
      GLfloat r = (GLfloat) MI_HEIGHT(mi) / MI_WIDTH(mi);
      bp->grid_w = 5 + frand(10);
      bp->grid_h = 5 + frand(10) * r;
      bp->grid_cx = bp->grid_cy = 0;
      bp->grid_logx = bp->grid_logy = 1;

      if (! (random() % 3))		/* Log-esque grid */
        {
          bp->grid_logx = (1.1 + frand(0.4)) * RANDSIGN();
          bp->grid_logy = (1.1 + frand(0.4)) * RANDSIGN();
        }
    }

    if (! (random() % 10))		/* Polar grid */
      {
        bp->grid_cx = (MI_WIDTH(mi)  / 4) + random() % (MI_WIDTH(mi)  / 2);
        bp->grid_cy = (MI_HEIGHT(mi) / 4) + random() % (MI_HEIGHT(mi) / 2);
      }

    /* Dark colors for the grid */
    hsv_to_gl (random() % 360,
               frand(0.3),
               0.2 + frand(0.2),
               bp->color);

    /* Graphs */

    for (j = 0; j < bp->count; j++)
      {
        graph *c = &bp->graph[j];

        GLfloat W = MI_WIDTH(mi);
        GLfloat H = MI_HEIGHT(mi);
        GLfloat base = H * (0.3 + frand (0.4));
        GLfloat tilt = (frand(1.0) - 0.5) * 0.5;

        /* These sinusoids are nice, but the original had more variety.
           Should add a couple more graph shape algorithms. */

        struct { GLfloat amp, freq, phase; } comps[3];
        int k;

        for (k = 0; k < countof(comps); k++)
          {
            comps[k].amp   = H * (0.06 + frand (0.16));
            comps[k].freq  = (0.6 + frand(1.8)) * M_PI * 2 / W;
            comps[k].phase = frand (M_PI * 2);

            if (! (random() % 5))
              comps[k].freq *= 0.2;	/* Flatten it */
          }

        c->i = 0;
        for (i = 0; i < c->count; i++)
          {

            int k;
            c->points[i].x = i;
            c->points[i].y = base + tilt * (i - W / 2);
            for (k = 0; k < countof(comps); k++)
              c->points[i].y += (comps[k].amp *
                                 sin (i * comps[k].freq + comps[k].phase));

            c->points[i].z =
              ((random() % 400) ? 0 :
                5 + frand ((double) MI_WIDTH(mi) / 60));
          }

        /* Light colors for the lines */
        hsv_to_gl (random() % 360,
                   0.2 + frand(0.3),
                   0.4 + frand(0.6),
                   c->color);
        c->color[3] = 1;

        /* Complementary colors for the nodes */
        {
          int h;
          GLfloat s, v;
          gl_to_hsv (c->color, &h, &s, &v);
          h += 135 + frand(90);
          v += 0.4;
          hsv_to_gl (h, s, v, c->color2);
          c->color2[3] = 1;
        }
      }

    /* Labels */

    if (bp->labels[0])
      free (bp->labels[0]);

    i = random() % bp->strings[0].count;
    j = random() % bp->strings[1].count;
    k = random() % bp->strings[2].count;
    bp->labels[0] = malloc (strlen (bp->strings[0].strings[i]) + 1 +
                            strlen (bp->strings[1].strings[j]) + 1 +
                            strlen (bp->strings[2].strings[k]) + 1 + 1);
    *bp->labels[0] = 0;
    strcat (bp->labels[0], bp->strings[0].strings[i]);
    strcat (bp->labels[0], " ");
    strcat (bp->labels[0], bp->strings[1].strings[j]);
    strcat (bp->labels[0], " ");
    strcat (bp->labels[0], bp->strings[2].strings[k]);
                           
    k = random() % bp->strings[3].count;
    bp->labels[1] = strdup (bp->strings[3].strings[k]);

    texture_string_metrics (bp->font_data, bp->labels[0], &e, 0, 0);
    bp->text_x = ((MI_WIDTH(mi) * 0.1) +
                  random() % MAX (1, (int) ((MI_WIDTH(mi) * 0.8) - e.width)));
    bp->text_y = ((MI_HEIGHT(mi) * 0.1) +
                  random() % MAX (1, (int) ((MI_HEIGHT(mi) * 0.8) -
                                            (e.ascent + e.descent))));

    texture_string_metrics (bp->font_data, bp->labels[1], &e2, 0, 0);
    bp->text_x2 = bp->text_x + (e.width - e2.width * SUBTITLE_SCALE) / 2;
    bp->text_y2 = bp->text_y - (e.ascent + e.descent);

    if (random() % 4)
      *bp->labels[1] = 0;	/* Only include subtitle sometimes */

    bp->text_alpha = 0;
    bp->tick = 1;
    break;

  case FADE_IN:
    bp->color[3] = bp->tick;
    bp->tick += speed_arg * (1 / 30.0);
    break;

  case DRAW:
    {
      int total_points = 0;
      int target_point;
      for (j = 0; j < MI_COUNT(mi); j++)
        total_points += bp->graph[j].count;
      target_point = total_points * bp->tick;

      total_points = 0;
      for (j = 0; j < MI_COUNT(mi); j++)
        {
          graph *c = &bp->graph[j];
          if (total_points < target_point)
            {
              c->i = target_point - total_points;
              if (c->i > c->count-1)
                c->i = c->count-1;
            }
          total_points += c->count;
        }

      bp->tick += 1 / (duration * 60 / 2);
    }
    break;

  case TEXT:
    bp->tick += 1 / (duration * 60 / 2);
    break;

  case FADE_OUT:
    bp->color[3] = 1 - bp->tick;
    for (j = 0; j < bp->count; j++)
      {
        graph *c = &bp->graph[j];
        c->color [3] = 1 - bp->tick;
        c->color2[3] = 1 - bp->tick;
      }
    bp->tick += speed_arg * (1 / 30.0);
    break;

  default: abort(); break;
  }

  if (bp->tick >= 1)
    {
      bp->tick = 0;
      bp->state = (bp->state + 1) % (FADE_OUT + 1);
    }
}


static void
glColor4fv_noalpha (const GLfloat color[4])
{
  GLfloat c2[4];
  c2[0] = color[0] * color[3];
  c2[1] = color[1] * color[3];
  c2[2] = color[2] * color[3];
  c2[3] = 1;
  glColor4fv (c2);
}



/* We draw everything using quads because glLineWidth / GL_LINES
   looks like crap with thick lines. */

static int
disc (GLfloat x, GLfloat y, GLfloat r, GLfloat thick, int steps)
{
  int polys = 0;
  int i;
  GLfloat step = M_PI*2 / steps;
  thick /= 2;
  glBegin (GL_QUAD_STRIP);
  for (i = 0; i <= steps; i++)
    {
      GLfloat th = i * step;
      GLfloat x2 = cos (th);
      GLfloat y2 = sin (th);
      glVertex3f (x + x2 * (r + thick), y + y2 * (r + thick), 0);
      glVertex3f (x + x2 * (r - thick), y + y2 * (r - thick), 0);
      polys++;
    }
  glEnd();
  return polys;
}


/* Draw the text five times, to give it a border. */
static int
dropshadow (texture_font_data *font, const char *text, const GLfloat color[4])
{
  int polys = 0;
  const int offsets[][2] = {{-1, -1}, {-1, 1}, {1, 1}, {1, -1}, {0, 0}};
  int i;
  glColor4f (0, 0, 0, 1);
  for (i = 0; i < countof(offsets); i++)
    {
      if (offsets[i][0] == 0)
        glColor4fv (color);
      glPushMatrix();
      glTranslatef (offsets[i][0], offsets[i][1], 0);
      print_texture_string (font, text);
      glPopMatrix();
      polys++;
    }

  return polys;
}


ENTRYPOINT void
draw_graphstat (ModeInfo *mi)
{
  graphstat_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat x, y, xoff, yoff;
  int i, j;
  int grid_thickness = thickness / 2;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  tick_graphstat (mi);

  glShadeModel (GL_SMOOTH);
  glDisable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glDisable (GL_CULL_FACE);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  mi->polygon_count = 0;

  glColor4fv_noalpha (bp->color);

  yoff = (GLfloat) MI_HEIGHT(mi) / bp->grid_h;
  xoff = (GLfloat) MI_WIDTH(mi)  / bp->grid_w;

  if (bp->grid_logx < 0) xoff = -xoff;
  if (bp->grid_logy < 0) yoff = -yoff;

  if (bp->grid_cx)
    {
      int max = MAX (MI_WIDTH(mi), MI_HEIGHT(mi));
      GLfloat th;
      y = 0;
      while (y < max)
        {
          GLfloat y2 = bp->grid_logy < 0 ? max - y : y;
          mi->polygon_count += disc (bp->grid_cx, bp->grid_cy, y2,
                                     grid_thickness, 64);
          y += yoff;
          yoff *= fabs (bp->grid_logy);
          if (yoff < 3) yoff = 3;
        }

      for (th = 0; th < 180; th += 180 / 16.0)
        {
          GLfloat t = grid_thickness / 2.0;
          glPushMatrix();
          glTranslatef (bp->grid_cx, bp->grid_cy, 0);
          glRotatef (th, 0, 0, 1);
          glBegin (GL_QUADS);
          glVertex3f (-t, -max, 0);
          glVertex3f ( t, -max, 0);
          glVertex3f ( t,  max, 0);
          glVertex3f (-t,  max, 0);
          mi->polygon_count++;
          glEnd();
          glPopMatrix();
        }
    }
  else
    {
      GLfloat t = grid_thickness / 2.0;
      y = 0;
      glBegin (GL_QUADS);
      while (y < MI_HEIGHT(mi))
        {
          GLfloat y2 = bp->grid_logy < 0 ? MI_HEIGHT(mi) - y : y;
          glVertex3f (0, y2 - t, 0);
          glVertex3f (0, y2 + t, 0);
          glVertex3f (MI_WIDTH(mi), y2 + t, 0);
          glVertex3f (MI_WIDTH(mi), y2 - t, 0);
          mi->polygon_count++;
          y += yoff;
          yoff *= fabs (bp->grid_logy);
          if (yoff < grid_thickness*2) yoff = grid_thickness*2;
        }
  
      x = 0;
      while (x < MI_WIDTH(mi))
        {
          GLfloat x2 = bp->grid_logx < 0 ? MI_WIDTH(mi) - x : x;
          glVertex3f (x2 - t, 0, 0);
          glVertex3f (x2 + t, 0, 0);
          glVertex3f (x2 + t, MI_HEIGHT(mi), 0);
          glVertex3f (x2 - t, MI_HEIGHT(mi), 0);
          mi->polygon_count++;
          x += xoff;
          xoff *= fabs (bp->grid_logx);
          if (xoff < grid_thickness*2) xoff = grid_thickness*2;
        }
      glEnd();
    }


  for (j = 0; j < bp->count; j++)
    {
      graph *c = &bp->graph[j];
      GLfloat w = (thickness * MI_HEIGHT(mi) / 2000.0) * (j == 0 ? 3 : 1);
      if (w < 0.5) w = 0.5;

      glColor4fv_noalpha (c->color);
      glBegin (GL_QUADS);
      for (i = 0; i < c->i; i++)
        {
          glVertex3f (c->points[i].x,   c->points[i].y, 0);
          glVertex3f (c->points[i].x+w, c->points[i].y, 0);
          glVertex3f (c->points[i].x+w, c->points[i].y+w, 0);
          glVertex3f (c->points[i].x,   c->points[i].y+w, 0);
          mi->polygon_count++;
        }
      glEnd();

      glColor4fv_noalpha (c->color2);
      for (i = 0; i < c->i; i++)
        if (c->points[i].z)
          mi->polygon_count +=
            disc (c->points[i].x, c->points[i].y,
                  c->points[i].z + w * 2,
                  w, 36);
    }

  if (bp->state == TEXT ||
      bp->graph[0].i >= bp->graph[0].count - 1)
    {
      GLfloat color[4];
      memcpy (color, bp->graph[0].color2, sizeof (color));
      bp->text_alpha += speed_arg * (2 / 30.0);
      if (bp->text_alpha > 1) bp->text_alpha = 1;
      if (color[3] > bp->text_alpha)
        color[3] = bp->text_alpha;

      glTranslatef (bp->text_x, bp->text_y, 0);
      mi->polygon_count += dropshadow (bp->font_data, bp->labels[0], color);
      mi->polygon_count++;

      if (bp->labels[1] && *bp->labels[1])
        {
          glTranslatef (bp->text_x2 - bp->text_x, bp->text_y2 - bp->text_y, 0);
          glScalef (SUBTITLE_SCALE, SUBTITLE_SCALE, SUBTITLE_SCALE);
          mi->polygon_count += dropshadow (bp->font_data, bp->labels[1],
                                           color);
          mi->polygon_count++;
        }
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_graphstat (ModeInfo *mi)
{
  graphstat_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->graph)
    {
      for (i = 0; i < MI_COUNT(mi); i++)
        free (bp->graph[i].points);
      free (bp->graph);
    }

  for (i = 0; i < countof(bp->strings); i++)
    if (bp->strings[i].strings)
      {
        for (j = 0; j < bp->strings[i].count; j++)
          free (bp->strings[i].strings[j]);
        free (bp->strings[i].strings);
      }

  for (i = 0; i < countof(bp->labels); i++)
    if (bp->labels[i])
      free (bp->labels[i]);

  if (bp->font_data)
    free_texture_font (bp->font_data);
}

XSCREENSAVER_MODULE ("GraphStat", graphstat)

#endif /* USE_GL */
