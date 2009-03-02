/* gltext, Copyright (c) 2001-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	20000        \n" \
			"*showFPS:      False        \n" \
			"*wireframe:    False        \n" \

# define refresh_text 0
# define release_text 0
#define SMOOTH_TUBE       /* whether to have smooth or faceted tubes */

#ifdef SMOOTH_TUBE
# define TUBE_FACES  12   /* how densely to render tubes */
#else
# define TUBE_FACES  8
#endif


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"

#include <ctype.h>

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif /* HAVE_LOCALE_H */

#ifdef USE_GL /* whole file */

#ifdef HAVE_COCOA
# define DEF_TEXT       "%A%n%d %b %Y%n%r"
#else
# define DEF_TEXT        "(default)"
#endif

#define DEF_PROGRAM     "(default)"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_FRONT       "True"

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif /* HAVE_UNAME */

#include "glutstroke.h"
#include "glut_roman.h"
#define GLUT_FONT (&glutStrokeRoman)


typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  Bool spinx, spiny, spinz;

  GLuint text_list;

  int ncolors;
  XColor *colors;
  int ccolor;

  char *text;
  int reload;

  time_t last_update;

} text_configuration;

static text_configuration *tps = NULL;

static char *text_fmt;
static char *program_str;
static char *do_spin;
static Bool do_wander;
static Bool face_front_p;

static XrmOptionDescRec opts[] = {
  { "-text",    ".text",      XrmoptionSepArg, 0 },
  { "-program", ".program",   XrmoptionSepArg, 0 },
  { "-spin",    ".spin",      XrmoptionSepArg, 0 },
  { "+spin",    ".spin",      XrmoptionNoArg, "" },
  { "-wander",  ".wander",    XrmoptionNoArg, "True" },
  { "+wander",  ".wander",    XrmoptionNoArg, "False" },
  { "-front",   ".faceFront", XrmoptionNoArg, "True" },
  { "+front",   ".faceFront", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&text_fmt,     "text",      "Text",      DEF_TEXT,    t_String},
  {&program_str,  "program",   "Program",   DEF_PROGRAM, t_String},
  {&do_spin,      "spin",      "Spin",      DEF_SPIN,    t_String},
  {&do_wander,    "wander",    "Wander",    DEF_WANDER,  t_Bool},
  {&face_front_p, "faceFront", "FaceFront", DEF_FRONT,   t_Bool},
};

ENTRYPOINT ModeSpecOpt text_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_text (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
gl_init (ModeInfo *mi)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  static const GLfloat pos[4] = {5.0, 5.0, 10.0, 1.0};

  if (!wire)
    {
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glEnable(GL_CULL_FACE);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
    }

  tp->text_list = glGenLists (1);
  glNewList (tp->text_list, GL_COMPILE);
  glEndList ();
}


/* The GLUT font only has ASCII characters in them, so do what we can to
   convert Latin1 characters to the nearest ASCII equivalent... 
 */
static void
latin1_to_ascii (char *s)
{
  unsigned char *us = (unsigned char *) s;
  const unsigned char ascii[95] = {
    '!', 'C', '#', '#', 'Y', '|', 'S', '_', 'C', '?', '<', '=', '-', 'R', '_',
    '?', '?', '2', '3', '\'','u', 'P', '.', ',', '1', 'o', '>', '?', '?', '?',
    '?', 'A', 'A', 'A', 'A', 'A', 'A', 'E', 'C', 'E', 'E', 'E', 'E', 'I', 'I',
    'I', 'I', 'D', 'N', 'O', 'O', 'O', 'O', 'O', 'x', '0', 'U', 'U', 'U', 'U',
    'Y', 'p', 'S', 'a', 'a', 'a', 'a', 'a', 'a', 'e', 'c', 'e', 'e', 'e', 'e',
    'i', 'i', 'i', 'i', 'o', 'n', 'o', 'o', 'o', 'o', 'o', '/', 'o', 'u', 'u',
    'u', 'u', 'y', 'p', 'y' };
  while (*us)
    {
      if (*us >= 161)
        *us = ascii[*us - 161];
      else if (*us > 127)
        *us = '?';
      us++;
    }
}


static void
parse_text (ModeInfo *mi)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];

  if (tp->text) free (tp->text);

  if (program_str && *program_str && !!strcmp(program_str, "(default)"))
    {
      FILE *p;
      int i;
      char buf[1024];
      sprintf (buf, "( %.900s ) 2>&1", program_str);
      p = popen (buf, "r");
      if (! p)
        sprintf (buf, "error running '%.900s'", program_str);
      else
        {
          char *out = buf;
          char *end = out + sizeof(buf) - 1;
          int n;
          do {
            n = fread (out, 1, end - out, p);
            if (n > 0)
              out += n;
            *out = 0;
          } while (n > 0);
          fclose (p);
        }

      /* Truncate it to 10 lines */
      {
        char *s = buf;
        for (i = 0; i < 10; i++)
          if (s && (s = strchr (s, '\n')))
            s++;
        if (s) *s = 0;
      }

      tp->text = strdup (buf);
      tp->reload = 5;
    }
  else if (!text_fmt || !*text_fmt || !strcmp(text_fmt, "(default)"))
    {
# ifdef HAVE_UNAME
      struct utsname uts;

      if (uname (&uts) < 0)
        {
          tp->text = strdup("uname() failed");
        }
      else
        {
          char *s;
          if ((s = strchr(uts.nodename, '.')))
            *s = 0;
          tp->text = (char *) malloc(strlen(uts.nodename) +
                                     strlen(uts.sysname) +
                                     strlen(uts.version) +
                                     strlen(uts.release) + 10);
#  if defined(_AIX)
          sprintf(tp->text, "%s\n%s %s.%s",
                  uts.nodename, uts.sysname, uts.version, uts.release);
#  elif defined(__APPLE__)  /* MacOS X + XDarwin */
          sprintf(tp->text, "%s\n%s %s\n%s",
                  uts.nodename, uts.sysname, uts.release, uts.machine);
#  else
          sprintf(tp->text, "%s\n%s %s",
                  uts.nodename, uts.sysname, uts.release);
#  endif /* special system types */
        }
# else	/* !HAVE_UNAME */
#  ifdef VMS
      tp->text = strdup(getenv("SYS$NODE"));
#  else
      tp->text = strdup("*  *\n*  *  *\nxscreensaver\n*  *  *\n*  *");
#  endif
# endif	/* !HAVE_UNAME */
    }
  else if (!strchr (text_fmt, '%'))
    {
      tp->text = strdup (text_fmt);
    }
  else
    {
      time_t now = time ((time_t *) 0);
      struct tm *tm = localtime (&now);
      int L = strlen(text_fmt) + 100;
      tp->text = (char *) malloc (L);
      *tp->text = 0;
      strftime (tp->text, L-1, text_fmt, tm);
      if (!*tp->text)
        sprintf (tp->text, "strftime error:\n%s", text_fmt);
      tp->reload = 1;
    }

  latin1_to_ascii (tp->text);
}


ENTRYPOINT Bool
text_handle_event (ModeInfo *mi, XEvent *event)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      tp->button_down_p = True;
      gltrackball_start (tp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      tp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (tp->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           tp->button_down_p)
    {
      gltrackball_track (tp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


ENTRYPOINT void 
init_text (ModeInfo *mi)
{
  text_configuration *tp;
  int i;

  /* setlocale (LC_TIME, "") only refers to environment:
     not needed
   */
#if 0
# ifdef HAVE_SETLOCALE
  setlocale (LC_TIME, "");      /* for strftime() calls */
# endif
#endif

  if (!tps) {
    tps = (text_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (text_configuration));
    if (!tps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    tp = &tps[MI_SCREEN(mi)];
  }

  tp = &tps[MI_SCREEN(mi)];

  if ((tp->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    reshape_text (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  {
    double spin_speed   = 0.5;
    double wander_speed = 0.02;
    double tilt_speed   = 0.03;
    double spin_accel   = 0.5;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') tp->spinx = True;
        else if (*s == 'y' || *s == 'Y') tp->spiny = True;
        else if (*s == 'z' || *s == 'Z') tp->spinz = True;
        else if (*s == '0') ;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    tp->rot = make_rotator (tp->spinx ? spin_speed : 0,
                            tp->spiny ? spin_speed : 0,
                            tp->spinz ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    tp->rot2 = (face_front_p
                ? make_rotator (0, 0, 0, 0, tilt_speed, True)
                : 0);
    tp->trackball = gltrackball_init ();
  }

  tp->ncolors = 255;
  tp->colors = (XColor *) calloc(tp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        tp->colors, &tp->ncolors,
                        False, 0, False);

  /* brighter colors, please... */
  for (i = 0; i < tp->ncolors; i++)
    {
      tp->colors[i].red   = (tp->colors[i].red   / 2) + 32767;
      tp->colors[i].green = (tp->colors[i].green / 2) + 32767;
      tp->colors[i].blue  = (tp->colors[i].blue  / 2) + 32767;
    }

  parse_text (mi);

}


static int
fill_character (GLUTstrokeFont font, int c, Bool wire)
{
  GLfloat tube_width = 10;

  const StrokeCharRec *ch;
  const StrokeRec *stroke;
  const CoordRec *coord;
  StrokeFontPtr fontinfo;
  int i, j;

  fontinfo = (StrokeFontPtr) font;

  if (c < 0 || c >= fontinfo->num_chars)
    return 0;
  ch = &(fontinfo->ch[c]);
  if (ch)
    {
      GLfloat lx=0, ly=0;
      for (i = ch->num_strokes, stroke = ch->stroke;
           i > 0; i--, stroke++) {
        for (j = stroke->num_coords, coord = stroke->coord;
             j > 0; j--, coord++)
          {
# ifdef SMOOTH_TUBE
            int smooth = True;
# else
            int smooth = False;
# endif
            if (j != stroke->num_coords)
              tube (lx,       ly,       0,
                    coord->x, coord->y, 0,
                    tube_width,
                    tube_width * 0.15,
                    TUBE_FACES, smooth, True, wire);
            lx = coord->x;
            ly = coord->y;
          }
      }
      return (int) (ch->right + tube_width);
    }
  return 0;
}


static int
text_extents (const char *string, int *wP, int *hP)
{
  const char *s, *start;
  int line_height = GLUT_FONT->top - GLUT_FONT->bottom;
  int lines = 0;
  *wP = 0;
  *hP = 0;
  start = string;
  s = start;
  while (1)
    if (*s == '\n' || *s == 0)
      {
        int w = 0;
        while (start < s)
          {
            w += glutStrokeWidth(GLUT_FONT, *start);
            start++;
          }
        start = s+1;

        if (w > *wP) *wP = w;
        *hP += line_height;
        lines++;
        if (*s == 0) break;
        s++;
      }
    else
      s++;

  return lines;
}


static void
fill_string (const char *string, Bool wire)
{
  const char *s, *start;
  int line_height = GLUT_FONT->top - GLUT_FONT->bottom;
  int off;
  GLfloat x = 0, y = 0;
  int lines;

  int ow, oh;
  lines = text_extents (string, &ow, &oh);

  y = oh / 2 - line_height;

  start = string;
  s = start;
  while (1)
    if (*s == '\n' || *s == 0)
      {
        int line_w = 0;
        const char *s2;
        const char *lstart = start;
        const char *lend = s;

        /* strip off whitespace at beginning and end of line
           (since we're centering.) */
        while (lend > lstart && isspace(lend[-1]))
          lend--;
        while (lstart < lend && isspace(*lstart))
          lstart++;

        for (s2 = lstart; s2 < lend; s2++)
          line_w += glutStrokeWidth (GLUT_FONT, *s2);

        x = (-ow/2) + ((ow-line_w)/2);
        for (s2 = lstart; s2 < lend; s2++)
          {
            glPushMatrix();
            glTranslatef(x, y, 0);
            off = fill_character (GLUT_FONT, *s2, wire);
            x += off;
            glPopMatrix();
          }

        start = s+1;

        y -= line_height;
        if (*s == 0) break;
        s++;
      }
    else
      s++;
}


ENTRYPOINT void
draw_text (ModeInfo *mi)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat white[4] = {1.0, 1.0, 1.0, 1.0};
  GLfloat color[4] = {0.0, 0.0, 0.0, 1.0};

  if (!tp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(tp->glx_context));

  if (tp->reload)
    {
      if (time ((time_t *) 0) >= tp->last_update + tp->reload)
        {
          parse_text (mi);
          tp->last_update = time ((time_t *) 0);
        }
    }

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);


  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (tp->rot, &x, &y, &z, !tp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 8);

    gltrackball_rotate (tp->trackball);

    if (face_front_p)
      {
        double max = 90;
        get_position (tp->rot2, &x, &y, &z, !tp->button_down_p);
        if (tp->spinx) glRotatef (max/2 - x*max, 1, 0, 0);
        if (tp->spiny) glRotatef (max/2 - y*max, 0, 1, 0);
        if (tp->spinz) glRotatef (max/2 - z*max, 0, 0, 1);
      }
    else
      {
        get_rotation (tp->rot, &x, &y, &z, !tp->button_down_p);
        glRotatef (x * 360, 1, 0, 0);
        glRotatef (y * 360, 0, 1, 0);
        glRotatef (z * 360, 0, 0, 1);
      }
  }


  glColor4fv (white);

  color[0] = tp->colors[tp->ccolor].red   / 65536.0;
  color[1] = tp->colors[tp->ccolor].green / 65536.0;
  color[2] = tp->colors[tp->ccolor].blue  / 65536.0;
  tp->ccolor++;
  if (tp->ccolor >= tp->ncolors) tp->ccolor = 0;

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

  glScalef(0.01, 0.01, 0.01);

  fill_string(tp->text, wire);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("GLText", gltext, text)

#endif /* USE_GL */
