/* sonar, Copyright (c) 1998-2018 Jamie Zawinski and Stephen Martin
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Created in Apr 1998 by Stephen Martin <smartin@vanderfleet-martin.net>
 * for the  RedHat Screensaver Contest
 * Heavily hacked by jwz ever since.
 * Rewritten in OpenGL by jwz, Aug 2008.
 *
 * This is an implementation of a general purpose reporting tool in the
 * format of a Sonar display. It is designed such that a sensor is read
 * on every movement of a sweep arm and the results of that sensor are
 * displayed on the screen. The location of the display points (targets) on the
 * screen are determined by the current localtion of the sweep and a distance
 * value associated with the target. 
 *
 * Currently the only two sensors that are implemented are the simulator
 * (the default) and the ping sensor. The simulator randomly creates a set
 * of bogies that move around on the scope while the ping sensor can be
 * used to display hosts on your network.
 *
 * The ping code is only compiled in if you define HAVE_ICMP or HAVE_ICMPHDR,
 * because, unfortunately, different systems have different ways of creating
 * these sorts of packets.
 *
 * In order to use the ping sensor on most systems, this program must be
 * installed as setuid root, so that it can create an ICMP RAW socket.  Root
 * privileges are disavowed shortly after startup (just after connecting to
 * the X server and reading the resource database) so this is *believed* to
 * be a safe thing to do, but it is usually recommended that you have as few
 * setuid programs around as possible, on general principles.
 *
 * It is not necessary to make it setuid on MacOS systems, because on those
 * systems, unprivileged programs can ping by using ICMP DGRAM sockets
 * instead of ICMP RAW.
 *
 * It should be easy to extend this code to support other sorts of sensors.
 * Some ideas:
 *
 *   - search the output of "netstat" for the list of hosts to ping;
 *   - plot the contents of /proc/interrupts;
 *   - plot the process table, by process size, cpu usage, or total time;
 *   - plot the logged on users by idle time or cpu usage.
 *   - plot IM contacts or Facebook friends and their last-activity times.
 */

#define DEF_FONT "-*-courier-bold-r-normal-*-*-480-*-*-*-*-iso8859-1"
#define DEF_SPEED        "1.0"
#define DEF_SWEEP_SIZE   "0.3"
#define DEF_FONT_SIZE    "12"
#define DEF_TEAM_A_NAME  "F18"
#define DEF_TEAM_B_NAME  "MIG"
#define DEF_TEAM_A_COUNT "4"
#define DEF_TEAM_B_COUNT "4"
#define DEF_PING         "default"
#define DEF_PING_TIMEOUT "3000"
#define DEF_RESOLVE      "True"
#define DEF_TIMES        "True"
#define DEF_WOBBLE       "True"
#define DEF_DEBUG        "False"

#include "thread_util.h"

#define DEFAULTS	"*delay:	30000       \n" \
			"*font:       " DEF_FONT   "\n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*texFontCacheSize: 300     \n" \
			THREAD_DEFAULTS_XLOCK


# define release_sonar 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef HAVE_UNISTD_H
# include <unistd.h>   /* for setuid() */
#endif

#include "xlockmore.h"
#include "sonar.h"
#include "gltrackball.h"
#include "rotator.h"
#include "texfont.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

/* #define TEST_ASYNC_NETDB 1 */

# if TEST_ASYNC_NETDB
#   include "async_netdb.h"

#   include <assert.h>
#   include <netinet/in.h>
#   include <stdio.h>
# endif /* TEST_ASYNC_NETDB */

typedef struct {
  double x,y,z;
} XYZ;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  rotator *rot;
  Bool button_down_p;

  double start_time;
  GLfloat sweep_offset;

  GLuint screen_list, grid_list, sweep_list, table_list;
  int screen_polys, grid_polys, sweep_polys, table_polys;
  GLfloat sweep_th;
  GLfloat line_thickness;

  texture_font_data *texfont;

  enum { MSG, RESOLVE, RUN } state;
  sonar_sensor_data *ssd;
  char *error;
  char *desc;

  sonar_bogie *displayed;	/* on screen and fading */
  sonar_bogie *pending;		/* returned by sensor, not yet on screen */

# if TEST_ASYNC_NETDB
  async_name_from_addr_t query0;
  async_addr_from_name_t query1;
# endif
} sonar_configuration;

static sonar_configuration *sps = NULL;

static GLfloat speed;
static GLfloat sweep_size;
static GLfloat font_size;
static Bool resolve_p;
static Bool times_p;
static Bool wobble_p;
static Bool debug_p;

static char *team_a_name;
static char *team_b_name;
static int team_a_count;
static int team_b_count;
static int ping_timeout;
static char *ping_arg;

static XrmOptionDescRec opts[] = {
  { "-speed",        ".speed",       XrmoptionSepArg, 0 },
  { "-sweep-size",   ".sweepSize",   XrmoptionSepArg, 0 },
  { "-font-size",    ".fontSize",    XrmoptionSepArg, 0 },
  { "-team-a-name",  ".teamAName",   XrmoptionSepArg, 0 },
  { "-team-b-name",  ".teamBName",   XrmoptionSepArg, 0 },
  { "-team-a-count", ".teamACount",  XrmoptionSepArg, 0 },
  { "-team-b-count", ".teamBCount",  XrmoptionSepArg, 0 },
  { "-ping",         ".ping",        XrmoptionSepArg, 0 },
  { "-ping-timeout", ".pingTimeout", XrmoptionSepArg, 0 },
  { "-dns",          ".resolve",     XrmoptionNoArg, "True" },
  { "+dns",          ".resolve",     XrmoptionNoArg, "False" },
  { "-times",        ".times",       XrmoptionNoArg, "True" },
  { "+times",        ".times",       XrmoptionNoArg, "False" },
  { "-wobble",       ".wobble",      XrmoptionNoArg, "True" },
  { "+wobble",       ".wobble",      XrmoptionNoArg, "False" },
  THREAD_OPTIONS
  { "-debug",        ".debug",       XrmoptionNoArg, "True" },
};

static argtype vars[] = {
  {&speed,        "speed",       "Speed",       DEF_SPEED,        t_Float},
  {&sweep_size,   "sweepSize",   "SweepSize",   DEF_SWEEP_SIZE,   t_Float},
  {&font_size,    "fontSize",    "FontSize",    DEF_FONT_SIZE,    t_Float},
  {&team_a_name,  "teamAName",   "TeamName",    DEF_TEAM_A_NAME,  t_String},
  {&team_b_name,  "teamBName",   "TeamName",    DEF_TEAM_B_NAME,  t_String},
  {&team_a_count, "teamACount",  "TeamCount",   DEF_TEAM_A_COUNT, t_Int},
  {&team_b_count, "teamBCount",  "TeamCount",   DEF_TEAM_A_COUNT, t_Int},
  {&ping_arg,     "ping",        "Ping",        DEF_PING,         t_String},
  {&ping_timeout, "pingTimeout", "PingTimeout", DEF_PING_TIMEOUT, t_Int},
  {&resolve_p,    "resolve",     "Resolve",     DEF_RESOLVE,      t_Bool},
  {&times_p,      "times",       "Times",       DEF_TIMES,        t_Bool},
  {&wobble_p,     "wobble",      "Wobble",      DEF_WOBBLE,       t_Bool},
  {&debug_p,      "debug",       "Debug",       DEF_DEBUG,        t_Bool},
};

ENTRYPOINT ModeSpecOpt sonar_opts = {countof(opts), opts, countof(vars), vars, NULL};


static int
draw_screen (ModeInfo *mi, Bool mesh_p, Bool sweep_p)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int i;
  int th_steps, r_steps, r_skip, th_skip, th_skip2, outer_r;
  GLfloat curvature = M_PI * 0.4;
  GLfloat r0, r1, z0, z1, zoff;
  XYZ *ring;

  static const GLfloat glass[4]  = {0.0, 0.4, 0.0, 0.5};
  static const GLfloat lines[4]  = {0.0, 0.7, 0.0, 0.5};
  static const GLfloat sweepc[4] = {0.2, 1.0, 0.2, 0.5};
  static const GLfloat spec[4]   = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat shiny     = 20.0;

  if (wire && !(mesh_p || sweep_p)) return 0;

  glDisable (GL_TEXTURE_2D);

  glFrontFace (GL_CCW);
  th_steps = 36 * 4;    /* must be a multiple of th_skip2 divisor */
  r_steps = 40;
  r_skip = 1;
  th_skip = 1;
  th_skip2 = 1;
  outer_r = 0;

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mesh_p ? lines : glass);
  if (wire) glColor3fv (lines);

  if (mesh_p) 
    {
      th_skip  = th_steps / 12;
      th_skip2 = th_steps / 36;
      r_skip = r_steps / 3;
      outer_r = r_steps * 0.93;

      if (! wire)
        glLineWidth (sp->line_thickness);
    }

  ring = (XYZ *) calloc (th_steps, sizeof(*ring));

  for (i = 0; i < th_steps; i++)
    {
      double a = M_PI * 2 * i / th_steps;
      ring[i].x = cos(a);
      ring[i].y = sin(a);
    }

  /* place the bottom of the disc on the xy plane. */
  zoff = cos (curvature/2 * (M_PI/2)) / 2;

  for (i = r_steps; i > 0; i--)
    {
      int j0, j1;

      r0 = i     / (GLfloat) r_steps;
      r1 = (i+1) / (GLfloat) r_steps;

      if (r1 > 1) r1 = 1; /* avoid asin lossage */

      z0 = cos (curvature/2 * asin (r0)) / 2 - zoff;
      z1 = cos (curvature/2 * asin (r1)) / 2 - zoff;

      glBegin(wire || mesh_p ? GL_LINES : GL_QUAD_STRIP);
      for (j0 = 0; j0 <= th_steps; j0++)
        {
          if (mesh_p && 
              (i < outer_r
               ? (j0 % th_skip != 0)
               : (j0 % th_skip2 != 0)))
            continue;

          if (sweep_p)
            {
              GLfloat color[4];
              GLfloat r = 1 - (j0 / (GLfloat) (th_steps * sweep_size));
#if 0
              color[0] = glass[0] + (sweepc[0] - glass[0]) * r;
              color[1] = glass[1] + (sweepc[1] - glass[1]) * r;
              color[2] = glass[2] + (sweepc[2] - glass[2]) * r;
              color[3] = glass[3];
#else
              color[0] = sweepc[0];
              color[1] = sweepc[1];
              color[2] = sweepc[2];
              color[3] = r;
#endif
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
            }

          j1 = j0 % th_steps;
          glNormal3f (r0 * ring[j1].x, r0 * ring[j1].y, z0);
          glVertex3f (r0 * ring[j1].x, r0 * ring[j1].y, z0);
          glNormal3f (r1 * ring[j1].x, r1 * ring[j1].y, z1);
          glVertex3f (r1 * ring[j1].x, r1 * ring[j1].y, z1);
          polys++;

          if (sweep_p && j0 >= th_steps * sweep_size)
            break;
          if (sweep_p && wire)
            break;
        }
      glEnd();

      if (mesh_p &&
          (i == outer_r ||
           i == r_steps ||
           (i % r_skip == 0 &&
            i < r_steps - r_skip)))
        {
          glBegin(GL_LINE_LOOP);
          for (j0 = 0; j0 < th_steps; j0++)
            {
              glNormal3f (r0 * ring[j0].x, r0 * ring[j0].y, z0);
              glVertex3f (r0 * ring[j0].x, r0 * ring[j0].y, z0);
              polys++;
            }
          glEnd();
        }
    }

  /* one more polygon for the middle */
  if (!wire && !sweep_p)
    {
      glBegin(wire || mesh_p ? GL_LINE_LOOP : GL_POLYGON);
      glNormal3f (0, 0, 1);
      for (i = 0; i < th_steps; i++)
        {
          glNormal3f (r0 * ring[i].x, r0 * ring[i].y, z0);
          glVertex3f (r0 * ring[i].x, r0 * ring[i].y, z0);
        }
      polys++;
      glEnd();
    }

  free (ring);

  return polys;
}


static int
draw_text (ModeInfo *mi, const char *string, GLfloat r, GLfloat th, 
           GLfloat ttl, GLfloat size)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  GLfloat font_scale = 0.001 * (size > 0 ? size : font_size) / 14.0;
  int lines = 0, max_w = 0, lh = 0;
  char *string2 = strdup (string);
  char *token = string2;
  char *line;

  if (MI_WIDTH(mi) > 2560) font_scale /= 2;  /* Retina displays */

  if (size <= 0)   /* if size not specified, draw in yellow with alpha */
    {
      GLfloat color[4];
      color[0] = 1;
      color[1] = 1;
      color[2] = 0;
      color[3] = (ttl / (M_PI * 2)) * 1.2;
      if (color[3] > 1) color[3] = 1;

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
      if (wire)
        glColor3f (color[0]*color[3], color[1]*color[3], color[2]*color[3]);
    }

  while ((line = strtok (token, "\r\n")))
    {
      XCharStruct e;
      int w, ascent, descent;
      texture_string_metrics (sp->texfont, line, &e, &ascent, &descent);
      w = e.width;
      lh = ascent + descent;
      if (w > max_w) max_w = w;
      lines++;
      token = 0;
    }

  glPushMatrix();
  glTranslatef (r * cos (th), r * sin(th), 0);
  glScalef (font_scale, font_scale, font_scale);

  if (size <= 0)		/* Draw the dot */
    {
      GLfloat s = font_size * 1.7;
      glDisable (GL_TEXTURE_2D);
      glFrontFace (GL_CW);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (0, s, 0);
      glVertex3f (s, s, 0);
      glVertex3f (s, 0, 0);
      glVertex3f (0, 0, 0);
      glEnd();
      glTranslatef (-max_w/2, -lh, 0);
    }
  else
    glTranslatef (-max_w/2, -lh/2, 0);

  /* draw each line, centered */
  if (! wire) glEnable (GL_TEXTURE_2D);
  free (string2);
  string2 = strdup (string);
  token = string2;
  while ((line = strtok (token, "\r\n")))
    {
      XCharStruct e;
      int w;
      texture_string_metrics (sp->texfont, line, &e, 0, 0);
      w = e.width;
      glPushMatrix();
      glTranslatef ((max_w-w)/2, 0, polys * 4); /* 'polys' stops Z-fighting. */

      if (wire)
        {
          glBegin (GL_LINE_LOOP);
          glVertex3f (0, 0, 0);
          glVertex3f (w, 0, 0);
          glVertex3f (w, lh, 0);
          glVertex3f (0, lh, 0);
          glEnd();
        }
      else
        {
          glFrontFace (GL_CW);
          print_texture_string (sp->texfont, line);
        }
      glPopMatrix();
      glTranslatef (0, -lh, 0);
      polys++;
      token = 0;
    }
  glPopMatrix();

  free (string2);

  if (! wire) glEnable (GL_DEPTH_TEST);

  return polys;
}


/* There's a disc with a hole in it around the screen, to act as a mask
   preventing slightly off-screen bogies from showing up.  This clips 'em.
 */
static int
draw_table (ModeInfo *mi)
{
  /*sonar_configuration *sp = &sps[MI_SCREEN(mi)];*/
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  int i;
  int th_steps = 36 * 4;    /* same as in draw_screen */

  static const GLfloat color[4]  = {0.0, 0.0, 0.0, 1.0};
  static const GLfloat spec[4]   = {0.0, 0.0, 0.0, 1.0};
  static const GLfloat shiny     = 0.0;

  if (wire) return 0;

  glDisable (GL_TEXTURE_2D);

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMateriali  (GL_FRONT, GL_SHININESS, shiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

  glFrontFace (GL_CCW);
  glBegin(wire ? GL_LINES : GL_QUAD_STRIP);
  glNormal3f (0, 0, 1);
  for (i = 0; i <= th_steps; i++)
    {
      double a = M_PI * 2 * i / th_steps;
      double x = cos(a);
      double y = sin(a);
      glVertex3f (x, y, 0);
      glVertex3f (x*10, y*10, 0);
      polys++;
    }
  glEnd();

  return polys;
}


static int
draw_angles (ModeInfo *mi)
{
  int i;
  int polys = 0;

  static const GLfloat text[4]   = {0.15, 0.15, 0.15, 1.0};
  static const GLfloat spec[4]   = {0.0, 0.0, 0.0, 1.0};

  glMaterialfv (GL_FRONT, GL_SPECULAR,  spec);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, text);
  glTranslatef (0, 0, 0.01);
  for (i = 0; i < 360; i += 10)
    {
      char buf[10];
      GLfloat a = M_PI/2 - (i / 180.0 * M_PI);
      sprintf (buf, "%d", i);
      polys += draw_text (mi, buf, 1.07, a, 0, 10.0);
    }

  return polys;
}


static int
draw_bogies (ModeInfo *mi)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];
  int polys = 0;
  sonar_bogie *b;

  for (b = sp->displayed; b; b = b->next)
    {
      char *s = (char *) 
        malloc (strlen (b->name) + (b->desc ? strlen(b->desc) : 0) + 3);
      strcpy (s, b->name);
      if (b->desc)
        {
          strcat (s, "\n");
          strcat (s, b->desc);
        }
      polys += draw_text (mi, s, b->r, b->th, b->ttl, -1);
      free (s);

      /* Move *very slightly* forward so that the text is not all in the
         same plane: this prevents flickering with overlapping text as
         the textures fight for priority. */
      glTranslatef(0, 0, 0.00002);
    }

  return polys;
}


/* called from sonar-sim.c and sonar-icmp.c */
sonar_bogie *
sonar_copy_bogie (sonar_sensor_data *ssd, const sonar_bogie *b)
{
  sonar_bogie *b2 = (sonar_bogie *) calloc (1, sizeof(*b2));
  b2->name = strdup (b->name);
  b2->desc = b->desc ? strdup (b->desc) : 0;
  b2->r    = b->r;
  b2->th   = b->th;
  b2->ttl  = b->ttl;
  /* does not copy b->closure */

  /* Take this opportunity to normalize 'th' to the range [0-2pi). */
  while (b2->th < 0)       b2->th += M_PI*2;
  while (b2->th >= M_PI*2) b2->th -= M_PI*2;

  return b2;
}


/* called from sonar-icmp.c */
void
sonar_free_bogie (sonar_sensor_data *ssd, sonar_bogie *b)
{
  if (b->closure)
    ssd->free_bogie_cb (ssd, b->closure);
  free (b->name);
  if (b->desc) free (b->desc);
  free (b);
}

/* removes it from the list and frees it
 */
static void
delete_bogie (sonar_sensor_data *ssd, sonar_bogie *b,
              sonar_bogie **from_list)
{
  sonar_bogie *ob, *prev;
  for (prev = 0, ob = *from_list; ob; prev = ob, ob = ob->next)
    if (ob == b)
      {
        if (prev)
          prev->next = b->next;
        else
          (*from_list) = b->next;
        sonar_free_bogie (ssd, b);
        break;
      }
}


/* copies the bogie and adds it to the list.
   if there's another bogie there with the same name, frees that one.
 */
static void
copy_and_insert_bogie (sonar_sensor_data *ssd, sonar_bogie *b,
                       sonar_bogie **to_list)
{
  sonar_bogie *ob, *next;
  if (!b) abort();
  for (ob = *to_list, next = ob ? ob->next : 0; 
       ob; 
       ob = next, next = ob ? ob->next : 0)
    {
      if (ob == b) abort();   /* this will end badly */
      if (!strcmp (ob->name, b->name))  /* match! */
        {
          delete_bogie (ssd, ob, to_list);
          break;
        }
    }

  b = sonar_copy_bogie (ssd, b);
  b->next = *to_list;
  *to_list = b;
}


static void
update_sensor_data (sonar_configuration *sp)
{
  sonar_bogie *new_list = sp->ssd->scan_cb (sp->ssd);
  sonar_bogie *b2;

  /* If a bogie exists in 'new_list' but not 'pending', add it.
     If a bogie exists in both, update it in 'pending'.
   */
  for (b2 = new_list; b2; b2 = b2->next)
    {
      if (debug_p > 2)
        fprintf (stderr, "%s:   updated: %s (%5.2f %5.2f %5.2f)\n", 
                 progname, b2->name, b2->r, b2->th, b2->ttl);
      copy_and_insert_bogie (sp->ssd, b2, &sp->pending);
    }
  if (debug_p > 2) fprintf (stderr, "\n");
}


/* Returns whether the given angle lies between two other angles.
   When those angles cross 0, it assumes the wedge is the smaller one.
   That is: 5 lies between 10 and 350 degrees (a 20 degree wedge).
 */
static Bool
point_in_wedge (GLfloat th, GLfloat low, GLfloat high)
{
  if (low < high)
    return (th > low && th <= high);
  else
    return (th <= high || th > low);
}


/* Returns the current time in seconds as a double.
 */
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
sweep (sonar_configuration *sp)
{
  sonar_bogie *b;

  /* Move the sweep forward (clockwise).
   */
  GLfloat prev_sweep, this_sweep, tick;
  GLfloat cycle_secs = 30 / speed;  /* default to one cycle every N seconds */
  this_sweep = ((cycle_secs - fmod (double_time() - sp->start_time +
                                    sp->sweep_offset,
                                    cycle_secs))
                / cycle_secs
                * M_PI * 2);
  prev_sweep = sp->sweep_th;
  tick = prev_sweep - this_sweep;
  while (tick < 0) tick += M_PI*2;

  sp->sweep_th = this_sweep;

  if (this_sweep < 0 || this_sweep >= M_PI*2) abort();
  if (prev_sweep < 0)  /* skip first time */
    return;

  if (tick < 0 || tick >= M_PI*2) abort();


  /* Go through the 'pending' sensor data, find those bogies who are
     just now being swept, and move them from 'pending' to 'displayed'.
     (Leave bogies that have not yet been swept alone: we'll get to
     them when the sweep moves forward.)
   */
  b = sp->pending;
  while (b)
    {
      sonar_bogie *next = b->next;
      if (point_in_wedge (b->th, this_sweep, prev_sweep))
        {
          if (debug_p > 1) {
            time_t t = time((time_t *) 0);
            fprintf (stderr,
                     "%s: sweep hit: %02d:%02d: %s: (%5.2f %5.2f %5.2f;"
                     " th=[%.2f < %.2f <= %.2f])\n", 
                     progname,
                     (int) (t / 60) % 60, (int) t % 60,
                     b->name, b->r, b->th, b->ttl,
                     this_sweep, b->th, prev_sweep);
          }
          b->ttl = M_PI * 2.1;
          copy_and_insert_bogie (sp->ssd, b, &sp->displayed);
          delete_bogie (sp->ssd, b, &sp->pending);
        }
      b = next;
    }


  /* Update TTL on all currently-displayed bogies; delete the dead.

     Request sensor updates on the ones just now being swept.

     Any updates go into 'pending' and might not show up until
     the next time the sweep comes around.  This is to prevent
     already-drawn bogies from jumping to a new position without
     having faded out first.
  */
  b = sp->displayed;
  while (b)
    {
      sonar_bogie *next = b->next;
      b->ttl -= tick;

      if (b->ttl <= 0)
        {
          if (debug_p > 1)
            fprintf (stderr, "%s: TTL expired: %s (%5.2f %5.2f %5.2f)\n",
                     progname, b->name, b->r, b->th, b->ttl);
          delete_bogie (sp->ssd, b, &sp->displayed);
        }
      b = next;
    }

  update_sensor_data (sp);
}


static void
draw_startup_blurb (ModeInfo *mi)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];

  if (sp->error)
    {
      const char *msg = sp->error;
      static const GLfloat color[4] = {0, 1, 0, 1};

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
      glTranslatef (0, 0, 0.3);
      draw_text (mi, msg, 0, 0, 0, 30.0);

      /* only leave error message up for N seconds */
      if (sp->start_time + 6 < double_time())
        {
          free (sp->error);
          sp->error = 0;
        }
    }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_sonar (ModeInfo *mi, int width, int height)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];
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

  sp->line_thickness = (MI_IS_WIREFRAME (mi) ? 1 : MAX (1, height / 300.0));
}


ENTRYPOINT Bool
sonar_handle_event (ModeInfo *mi, XEvent *event)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, sp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &sp->button_down_p))
    return True;

  return False;
}

ENTRYPOINT void 
init_sonar (ModeInfo *mi)
{
  sonar_configuration *sp;

  MI_INIT (mi, sps);
  sp = &sps[MI_SCREEN(mi)];
  sp->glx_context = init_GL(mi);

  reshape_sonar (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

  sp->trackball = gltrackball_init (False);
  sp->rot = make_rotator (0, 0, 0, 0, speed * 0.003, True);

  sp->texfont = load_texture_font (MI_DISPLAY(mi), "font");
  check_gl_error ("loading font");

  sp->table_list = glGenLists (1);
  glNewList (sp->table_list, GL_COMPILE);
  sp->table_polys = draw_table (mi);
  glEndList ();

  sp->screen_list = glGenLists (1);
  glNewList (sp->screen_list, GL_COMPILE);
  sp->screen_polys = draw_screen (mi, False, False);
  glEndList ();

  sp->grid_list = glGenLists (1);
  glNewList (sp->grid_list, GL_COMPILE);
  sp->grid_polys = draw_screen (mi, True,  False);
  glEndList ();

  sp->sweep_list = glGenLists (1);
  glNewList (sp->sweep_list, GL_COMPILE);
  sp->sweep_polys = draw_screen (mi, False, True);
  glEndList ();

  sp->start_time = double_time ();
  sp->sweep_offset = random() % 60;
  sp->sweep_th = -1;
  sp->state = MSG;
}


# ifdef TEST_ASYNC_NETDB

#   include <arpa/inet.h>

static void _print_sockaddr (void *addr, socklen_t addrlen, FILE *stream)
{
  sa_family_t family = ((struct sockaddr *)addr)->sa_family;
  char buf[256];
  switch (family)
    {
    case AF_INET:
      fputs (inet_ntoa(((struct sockaddr_in *)addr)->sin_addr), stream);
      break;
    case AF_INET6:
      inet_ntop(family, &((struct sockaddr_in6 *)addr)->sin6_addr,
                buf, sizeof (buf));
      fputs (buf, stream);
      break;
    default:
      abort();
      break;
    }
}

static void _print_error (int gai_error, int errno_error, FILE *stream)
{
  fputs (gai_error == EAI_SYSTEM ? strerror(errno_error) : gai_strerror(gai_error), stream);
}

#   if ASYNC_NETDB_USE_GAI

static void _print_thread (pthread_t thread, FILE *stream)
{
#     ifdef __linux__
    fprintf (stream, "%#lx", thread);
#     elif defined __APPLE__ && defined __MACH__
    fprintf (stream, "%p", thread);
#     else
    putc ('?', stream);
#     endif
}

#   endif /* ASYNC_NETDB_USE_GAI */

# endif /* TEST_ASYNC_NETDB */


static void
init_sensor (ModeInfo *mi)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];

  if (sp->ssd) abort();

  if (!ping_arg || !*ping_arg ||
      !strcmp(ping_arg, "default") ||
      !!strcmp (ping_arg, "simulation"))
    /* sonar_init_ping() always disavows privs, even on failure. */
    sp->ssd = sonar_init_ping (MI_DISPLAY (mi), &sp->error, &sp->desc,
                               ping_arg, ping_timeout, resolve_p, times_p,
                               debug_p);
  else
    setuid(getuid()); /* Disavow privs if not pinging. */

  sp->start_time = double_time ();  /* for error message timing */

  if (!sp->ssd)
    sp->ssd = sonar_init_simulation (MI_DISPLAY (mi), &sp->error, &sp->desc,
                                     team_a_name, team_b_name,
                                     team_a_count, team_b_count,
                                     debug_p);
  if (!sp->ssd)
    abort();

# if TEST_ASYNC_NETDB
  /*
     For extremely mysterious reasons, setuid apparently causes
     pthread_join(3) to deadlock.
     A rough guess at the sequence of events:
     1. Worker thread is created.
     2. Worker thread exits.
     3. setuid(getuid()) is called.
     4. pthread_join is called slightly later.

     This may have something to do with glibc's use of SIGSETXID.
   */

  putc ('\n', stderr);

#   if !ASYNC_NETDB_USE_GAI
  fputs ("Warning: getaddrinfo() was not available at compile time.\n", stderr);
#   endif

  {
    static const unsigned long addresses[] =
      {
        INADDR_LOOPBACK,
        0x00010203,
        0x08080808
      };
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl (addresses[random () % 3]);

    sp->query0 = async_name_from_addr_start (MI_DISPLAY (mi), (void *)&addr,
                                             sizeof(addr));
    assert (sp->query0);
    if (sp->query0)
      {
        fputs ("Looking up hostname from address: ", stderr);
        _print_sockaddr (&addr, sizeof(addr), stderr);
#   if ASYNC_NETDB_USE_GAI
        fputs (" @ ", stderr);
        _print_thread (sp->query0->io.thread, stderr);
#   endif
        putc ('\n', stderr);
      }

    if (!(random () & 3))
      {
        fputs ("Aborted hostname lookup (early)\n", stderr);
        async_name_from_addr_cancel (sp->query0);
        sp->query0 = NULL;
      }
  }

  {
    static const char *const hosts[] =
      {
        "example.com",
        "invalid",
        "ip6-localhost"
      };
    const char *host = hosts[random () % 3];

    sp->query1 = async_addr_from_name_start (MI_DISPLAY(mi), host);

    assert (sp->query1);

    fprintf (stderr, "Looking up address from hostname: %s", host);
#   if ASYNC_NETDB_USE_GAI
    fputs (" @ ", stderr);
    _print_thread (sp->query1->io.thread, stderr);
#   endif
    putc ('\n', stderr);

    if (!(random () & 3))
      {
        fputs ("Aborted address lookup (early)\n", stderr);
        async_addr_from_name_cancel (sp->query1);
        sp->query1 = NULL;
      }
  }

  fflush (stderr);
# endif
}


ENTRYPOINT void
draw_sonar (ModeInfo *mi)
{
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  if (!sp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!wire)
    {
      GLfloat pos[4] = {0.05, 0.07, 1.00, 0.0};
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_TEXTURE_2D);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_CULL_FACE);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_NORMALIZE);
      glEnable(GL_LINE_SMOOTH);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
      glShadeModel(GL_SMOOTH);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);

  {
    GLfloat s = 7;
    if (MI_WIDTH(mi) < MI_HEIGHT(mi))
      s *= (MI_WIDTH(mi) / (float) MI_HEIGHT(mi));
    glScalef (s,s,s);
  }

  gltrackball_rotate (sp->trackball);

  if (wobble_p)
    {
      double x, y, z;
      double max = 40;
      get_position (sp->rot, &x, &y, &z, !sp->button_down_p);
      glRotatef (max/2 - x*max, 1, 0, 0);
      glRotatef (max/2 - z*max, 0, 1, 0);
    }

  mi->polygon_count = 0;

  glPushMatrix();					/* table */
  glCallList (sp->table_list);
  mi->polygon_count += sp->table_polys;
  glPopMatrix();

  glPushMatrix();					/* text */
  glTranslatef (0, 0, -0.01);
  mi->polygon_count += draw_bogies (mi);
  glPopMatrix();

  glCallList (sp->screen_list);				/* glass */
  mi->polygon_count += sp->screen_polys;

  glTranslatef (0, 0, 0.004);				/* sweep */
  glPushMatrix();
  glRotatef ((sp->sweep_th * 180 / M_PI), 0, 0, 1);
  if (sp->sweep_th >= 0)
    glCallList (sp->sweep_list);
  mi->polygon_count += sp->sweep_polys;
  glPopMatrix();

  glTranslatef (0, 0, 0.004);				/* grid */
  glCallList (sp->grid_list);
  mi->polygon_count += sp->screen_polys;

  glPushMatrix();
  mi->polygon_count += draw_angles (mi);		/* angles */
  glPopMatrix();

  if (sp->desc)						/* local subnet */
    {
      glPushMatrix();
      glTranslatef (0, 0, 0.00002);
      mi->polygon_count += draw_text (mi, sp->desc, 1.35, M_PI * 0.75, 0, 10);
      /* glRotatef (45, 0, 0, 1); */
      /* mi->polygon_count += draw_text (mi, sp->desc, 1.2, M_PI/2, 0, 10); */
      glPopMatrix();
    }

  if (sp->error)
    sp->state = MSG;

  switch (sp->state) {
  case MSG:			/* Frame 1: get "Resolving Hosts" on screen. */
    draw_startup_blurb(mi);
    sp->state++;
    break;
  case RESOLVE:			/* Frame 2: gethostbyaddr may take a while. */
    if (! sp->ssd)
      init_sensor (mi);
    sp->state++;
    break;
  case RUN:			/* Frame N: ping away */
    sweep (sp);
    break;
  }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);

# if TEST_ASYNC_NETDB
  if(sp->query0 && async_name_from_addr_is_done (sp->query0))
    {
      if (!(random () & 3))
        {
          fputs ("Aborted hostname lookup (late)\n", stderr);
          async_name_from_addr_cancel (sp->query0);
        }
      else
        {
          char *hostname = NULL;
          int errno_error;
          int gai_error = async_name_from_addr_finish (sp->query0, &hostname,
                                                       &errno_error);

          if(gai_error)
            {
              fputs ("Couldn't get hostname: ", stderr);
              _print_error (gai_error, errno_error, stderr);
              putc ('\n', stderr);
            }
          else
            {
              fprintf (stderr, "Got a hostname: %s\n", hostname);
              free (hostname);
            }
        }

      sp->query0 = NULL;
    }

  if(sp->query1 && async_addr_from_name_is_done (sp->query1))
    {
      if (!(random () & 3))
        {
          fputs ("Aborted address lookup (late)\n", stderr);
          async_addr_from_name_cancel (sp->query1);
        }
      else
        {
          async_netdb_sockaddr_storage_t addr;
          socklen_t addrlen;
          int errno_error;
          int gai_error = async_addr_from_name_finish (sp->query1, &addr,
                                                       &addrlen, &errno_error);

          if (gai_error)
            {
              fputs ("Couldn't get address: ", stderr);
              _print_error (gai_error, errno_error, stderr);
              putc ('\n', stderr);
            }
          else
            {
              fputs ("Got an address: ", stderr);
              _print_sockaddr (&addr, addrlen, stderr);
              putc ('\n', stderr);
            }
        }

      sp->query1 = NULL;
    }

  fflush (stderr);
# endif /* TEST_ASYNC_NETDB */
}

ENTRYPOINT void
free_sonar (ModeInfo *mi)
{
#if 0
  sonar_configuration *sp = &sps[MI_SCREEN(mi)];
  sonar_bogie *b = sp->displayed;
  while (b)
    {
      sonar_bogie *next = b->next;
      free_bogie (sp->ssd, b);
      b = next;
    }
  sp->displayed = 0;

  b = sp->pending;
  while (b)
    {
      sonar_bogie *next = b->next;
      free_bogie (sp->ssd, b);
      b = next;
    }
  sp->pending = 0;

  sp->ssd->free_data_cb (sp->ssd, sp->ssd->closure);
  free (sp->ssd);
  sp->ssd = 0;
#endif
}

XSCREENSAVER_MODULE ("Sonar", sonar)

#endif /* USE_GL */
