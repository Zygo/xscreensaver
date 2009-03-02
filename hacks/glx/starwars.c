/*
 * starwars, Copyright (c) 1998-2001 Jamie Zawinski <jwz@jwz.org> and
 * Claudio Matauoka <claudio@helllabs.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Star Wars -- Phosphor meets a well-known scroller from a galaxy far,
 *           far away. Hacked by Claudio Matsuoka. Includes portions of
 *           mjk's GLUT library, Copyright (c) 1994, 1995, 1996 by Mark J.
 *           Kilgard. Roman simplex stroke font Copyright (c) 1989, 1990,
 *           1991 by Sun Microsystems, Inc. and the X Consortium.
 *
 *      Notes:
 *         - I tried texturized fonts but the roman simplex stroke font
 *           was the most readable for the 80-column text from fortune.
 *         - The proportional font is bad for text from ps(1) or w(1).
 *         - Apparently the RIVA TNT cards for PCs don't like the stars to
 *           be drawn in orthogonal perspective, causing unnecessary system
 *           load.
 *
 *      History:
 *           20000221 claudio   First version
 *           20010124 jwz       Rewrote large sections to add the ability to
 *                              run a subprocess, customization of the font
 *                              size and other parameters, etc.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"StarWars"
#define HACK_INIT	init_sws
#define HACK_DRAW	draw_sws
#define HACK_RESHAPE	reshape_sws
#define sws_opts	xlockmore_opts

#define DEF_PROGRAM    ZIPPY_PROGRAM
#define DEF_LINES      "500"
#define DEF_STEPS      "35"
#define DEF_SPIN       "0.03"
#define DEF_FONT_SIZE  "-1"
#define DEF_COLUMNS    "-1"
#define DEF_WRAP       "True"
#define DEF_ALIGN      "Center"

#define TAB_WIDTH        8

#define BASE_FONT_SIZE    18 /* magic */
#define BASE_FONT_COLUMNS 80 /* magic */


#define DEFAULTS	"*delay:	40000 \n"		     \
			"*showFPS:      False \n"		     \
			"*fpsTop:       True \n"		     \
			"*program:	" DEF_PROGRAM		"\n" \
			"*lines:	" DEF_LINES		"\n" \
			"*spin:		" DEF_SPIN		"\n" \
			"*steps:	" DEF_STEPS		"\n" \
			"*starwars.fontSize: " DEF_FONT_SIZE	"\n" \
			"*starwars.columns:  " DEF_COLUMNS	"\n" \
			"*starwars.lineWrap: " DEF_WRAP		"\n" \
			"*starwars.alignment:" DEF_ALIGN	"\n"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#include <GL/glu.h>
#include "glutstroke.h"
#include "glut_roman.h"
#define GLUT_FONT (&glutStrokeRoman)


typedef struct {
  GLXContext *glx_context;

  GLuint text_list, star_list;

  FILE *pipe;
  XtInputId pipe_id;
  Time subproc_relaunch_delay;

  char buf [1024];
  int buf_tail;
  char **lines;
  int total_lines;
  int columns;

  double star_theta;
  double line_height;
  double font_scale;
  double intra_line_scroll;

} sws_configuration;


static sws_configuration *scs = NULL;

static char *program;
static int max_lines;
static int scroll_steps;
static float star_spin;
static float font_size;
static int target_columns;
static int wrap_p;
static char *alignment_str;
static int alignment;

static XrmOptionDescRec opts[] = {
  {"-program",   ".starwars.program",  XrmoptionSepArg, (caddr_t) 0 },
  {"-lines",     ".starwars.lines",    XrmoptionSepArg, (caddr_t) 0 },
  {"-steps",     ".starwars.steps",    XrmoptionSepArg, (caddr_t) 0 },
  {"-spin",      ".starwars.spin",     XrmoptionSepArg, (caddr_t) 0 },
  {"-size",	 ".starwars.fontSize", XrmoptionSepArg, (caddr_t) 0 },
  {"-columns",	 ".starwars.columns",  XrmoptionSepArg, (caddr_t) 0 },
  {"-wrap",	 ".starwars.lineWrap", XrmoptionNoArg,  (caddr_t) "True" },
  {"-no-wrap",	 ".starwars.lineWrap", XrmoptionNoArg,  (caddr_t) "False" },
  {"-nowrap",	 ".starwars.lineWrap", XrmoptionNoArg,  (caddr_t) "False" },
  {"-left",      ".starwars.alignment",XrmoptionNoArg,  (caddr_t) "Left" },
  {"-right",     ".starwars.alignment",XrmoptionNoArg,  (caddr_t) "Right" },
  {"-center",    ".starwars.alignment",XrmoptionNoArg,  (caddr_t) "Center" },
};

static argtype vars[] = {
  {(caddr_t *) &program,        "program", "Program", DEF_PROGRAM, t_String},
  {(caddr_t *) &max_lines,      "lines",   "Integer", DEF_LINES,   t_Int},
  {(caddr_t *) &scroll_steps,   "steps",   "Integer", DEF_STEPS,   t_Int},
  {(caddr_t *) &star_spin,      "spin",    "Float",   DEF_SPIN,    t_Float},
  {(caddr_t *) &font_size,      "fontSize","Float",   DEF_STEPS,   t_Float},
  {(caddr_t *) &target_columns, "columns", "Integer", DEF_COLUMNS, t_Int},
  {(caddr_t *) &wrap_p,         "lineWrap","Boolean", DEF_COLUMNS, t_Bool},
  {(caddr_t *) &alignment_str,  "alignment","Alignment",DEF_ALIGN, t_String},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Tabs are bad, mmmkay? */

static char *
untabify (const char *string)
{
  char *result = (char *) malloc ((strlen(string) * 8) + 1);
  char *out = result;
  int col = 0;
  while (*string)
    {
      if (*string == '\t')
        {
          do {
            col++;
            *out++ = ' ';
          } while (col % TAB_WIDTH);
          string++;
        }
      else if (*string == '\r' || *string == '\n')
        {
          *out++ = *string++;
          col = 0;
        }
      else
        {
          *out++ = *string++;
          col++;
        }
    }
  *out = 0;
  return result;
}



/* Subprocess.
   (This bit mostly cribbed from phosphor.c)
 */

static void drain_input (sws_configuration *sc);

static void
subproc_cb (XtPointer closure, int *source, XtInputId *id)
{
  sws_configuration *sc = (sws_configuration *) closure;
  drain_input (sc);
}


static void
launch_text_generator (sws_configuration *sc)
{
  char *oprogram = get_string_resource ("program", "Program");
  char *program = (char *) malloc (strlen (oprogram) + 10);

  strcpy (program, "( ");
  strcat (program, oprogram);
  strcat (program, " ) 2>&1");

  if ((sc->pipe = popen (program, "r")))
    {
      sc->pipe_id =
        XtAppAddInput (app, fileno (sc->pipe),
                       (XtPointer) (XtInputReadMask | XtInputExceptMask),
                       subproc_cb, (XtPointer) sc);
    }
  else
    {
      perror (program);
    }
}


static void
relaunch_generator_timer (XtPointer closure, XtIntervalId *id)
{
  sws_configuration *sc = (sws_configuration *) closure;
  launch_text_generator (sc);
}


/* When the subprocess has generated some output, this reads as much as it
   can into sc->buf at sc->buf_tail.
 */
static void
drain_input (sws_configuration *sc)
{
  if (sc->buf_tail < sizeof(sc->buf) - 2)
    {
      int target = sizeof(sc->buf) - sc->buf_tail - 2;
      int n = read (fileno (sc->pipe),
                    (void *) (sc->buf + sc->buf_tail),
                    target);
      if (n > 0)
        {
          sc->buf_tail += n;
          sc->buf[sc->buf_tail] = 0;
        }
      else
        {
          XtRemoveInput (sc->pipe_id);
          sc->pipe_id = 0;
          pclose (sc->pipe);
          sc->pipe = 0;

          /* If the process didn't print a terminating newline, add one. */
          if (sc->buf_tail > 1 &&
              sc->buf[sc->buf_tail-1] != '\n')
            {
              sc->buf[sc->buf_tail++] = '\n';
              sc->buf[sc->buf_tail] = 0;
            }

          /* Then add one more, just for giggles. */
          sc->buf[sc->buf_tail++] = '\n';
          sc->buf[sc->buf_tail] = 0;

          /* Set up a timer to re-launch the subproc in a bit. */
          XtAppAddTimeOut (app, sc->subproc_relaunch_delay,
                           relaunch_generator_timer,
                           (XtPointer) sc);
        }
    }
}


/* Populates the sc->lines list with as many lines as are currently in
   sc->buf (which was filled by drain_input().
 */
static void
get_more_lines (sws_configuration *sc)
{
  int col = 0;
  char *s = sc->buf;
  while (sc->total_lines < max_lines)
    {
      if (s >= sc->buf + sc->buf_tail)
        {
          /* Reached end of buffer before end of line.  Bail. */
          return;
        }

      if (*s == '\n' || col > sc->columns)
        {
          int L = s - sc->buf;

          if (*s == '\n')
            *s++ = 0;
          else
            {
              /* We wrapped -- try to back up to the previous word boundary. */
              char *s2 = s;
              int n = 0;
              while (s2 > sc->buf && *s2 != ' ' && *s2 != '\t')
                s2--, n++;
              if (s2 > sc->buf)
                {
                  s = s2;
                  *s++ = 0;
                  L = s - sc->buf;
                }
            }

          sc->lines[sc->total_lines] = (char *) malloc (L+1);
          memcpy (sc->lines[sc->total_lines], sc->buf, L);
          sc->lines[sc->total_lines][L] = 0;

          {
            char *t = sc->lines[sc->total_lines];
            char *ut = untabify (t);
            sc->lines[sc->total_lines] = ut;
            free (t);
          }

          sc->total_lines++;

          if (sc->buf_tail > (s - sc->buf))
            {
              int i = sc->buf_tail - (s - sc->buf);
              memcpy (sc->buf, s, i);
              sc->buf_tail = i;
              sc->buf[sc->buf_tail] = 0;
            }
          else
            {
              sc->buf_tail = 0;
            }

          sc->buf[sc->buf_tail] = 0;
          s = sc->buf;
          col = 0;
        }
      else
        {
          col++;
          if (*s == '\t')
            col = TAB_WIDTH * ((col / TAB_WIDTH) + 1);
          s++;
        }
    }
}


static void
draw_string (int x, int y, const char *s)
{
  if (!s || !*s) return;
  glPushMatrix ();
  glTranslatef (x, y, 0);
  while (*s)
    glutStrokeCharacter (GLUT_FONT, *s++);
  glPopMatrix ();
}


#if 0
static void
grid (double width, double height, double spacing, double z)
{
  double x, y;
  for (y = 0; y <= height/2; y += spacing)
    {
      glBegin(GL_LINE_LOOP);
      glVertex3f(-width/2,  y, z);
      glVertex3f( width/2,  y, z);
      glEnd();
      glBegin(GL_LINE_LOOP);
      glVertex3f(-width/2, -y, z);
      glVertex3f( width/2, -y, z);
      glEnd();
    }
  for (x = 0; x <= width/2; x += spacing)
    {
      glBegin(GL_LINE_LOOP);
      glVertex3f( x, -height/2, z);
      glVertex3f( x,  height/2, z);
      glEnd();
      glBegin(GL_LINE_LOOP);
      glVertex3f(-x, -height/2, z);
      glVertex3f(-x,  height/2, z);
      glEnd();
    }

  glBegin(GL_LINE_LOOP);
  glVertex3f(-width, 0, z);
  glVertex3f( width, 0, z);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(0, -height, z);
  glVertex3f(0,  height, z);
  glEnd();
}

static void
box (double width, double height, double depth)
{
  glBegin(GL_LINE_LOOP);
  glVertex3f(-width/2,  -height/2, -depth/2);
  glVertex3f(-width/2,   height/2, -depth/2);
  glVertex3f( width/2,   height/2, -depth/2);
  glVertex3f( width/2,  -height/2, -depth/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-width/2,  -height/2,  depth/2);
  glVertex3f(-width/2,   height/2,  depth/2);
  glVertex3f( width/2,   height/2,  depth/2);
  glVertex3f( width/2,  -height/2,  depth/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-width/2,  -height/2, -depth/2);
  glVertex3f(-width/2,  -height/2,  depth/2);
  glVertex3f(-width/2,   height/2,  depth/2);
  glVertex3f(-width/2,   height/2, -depth/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f( width/2,  -height/2, -depth/2);
  glVertex3f( width/2,  -height/2,  depth/2);
  glVertex3f( width/2,   height/2,  depth/2);
  glVertex3f( width/2,   height/2, -depth/2);
  glEnd();

  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-width/2,   height/2,  depth/2);
  glVertex3f(-width/2,  -height/2, -depth/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f( width/2,   height/2,  depth/2);
  glVertex3f( width/2,  -height/2, -depth/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(-width/2,  -height/2,  depth/2);
  glVertex3f(-width/2,   height/2, -depth/2);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f( width/2,  -height/2,  depth/2);
  glVertex3f( width/2,   height/2, -depth/2);
  glEnd();
}
#endif /* 0 */


/* Window management, etc
 */
void
reshape_sws (ModeInfo *mi, int width, int height)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];
  static Bool stars_done = False;

  glViewport (0, 0, (GLint) width, (GLint) height);
  if (!stars_done)
    {
      int i;
      int nstars = width * height / 320;
      glDeleteLists (sc->star_list, 1);
      sc->star_list = glGenLists (1);
      glNewList (sc->star_list, GL_COMPILE);
      glBegin (GL_POINTS);
      for (i = 0; i < nstars; i++)
        {
          GLfloat c = 0.6 + 0.3 * random() / RAND_MAX;
          glColor3f (c, c, c);
          glVertex3f (2 * width  * (0.5 - 1.0 * random() / RAND_MAX),
                      2 * height * (0.5 - 1.0 * random() / RAND_MAX),
                      0.0);
        }
      glEnd ();
      glEndList ();
      stars_done = True;
    }
}


static void
gl_init (ModeInfo *mi)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];

  program = get_string_resource ("program", "Program");

  glMatrixMode (GL_MODELVIEW);

  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);

  sc->text_list = glGenLists (1);
  glNewList (sc->text_list, GL_COMPILE);
  glEndList ();

  sc->star_list = glGenLists (1);
  glNewList (sc->star_list, GL_COMPILE);
  glEndList ();
}


void 
init_sws (ModeInfo *mi)
{
  double font_height;

  sws_configuration *sc;

  if (!scs) {
    scs = (sws_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (sws_configuration));
    if (!scs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    sc = &scs[MI_SCREEN(mi)];
    sc->lines = (char **) calloc (max_lines+1, sizeof(char *));
  }

  sc = &scs[MI_SCREEN(mi)];

  if ((sc->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    reshape_sws (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }



  font_height = GLUT_FONT->top - GLUT_FONT->bottom;
  sc->font_scale = 1.0 / glutStrokeWidth (GLUT_FONT, 'z');   /* 'n' seems
                                                                too wide */
  if (target_columns > 0)
    {
      sc->columns = target_columns;
    }
  else
    {
      if (font_size <= 0)
        font_size = BASE_FONT_SIZE;
      sc->columns = BASE_FONT_COLUMNS * ((double) BASE_FONT_SIZE / font_size);
    }

  sc->font_scale /= sc->columns;
  sc->line_height = font_height * sc->font_scale;


  if (!wrap_p) sc->columns = 1000;  /* wrap anyway, if it's absurdly long. */

  sc->subproc_relaunch_delay = 2 * 1000;
  sc->total_lines = max_lines-1;
  launch_text_generator (sc);

  if (random() & 1)
    star_spin = -star_spin;

  if (!alignment_str || !*alignment_str ||
      !strcasecmp(alignment_str, "left"))
    alignment = -1;
  else if (!strcasecmp(alignment_str, "center") ||
           !strcasecmp(alignment_str, "middle"))
    alignment = 0;
  else if (!strcasecmp(alignment_str, "right"))
    alignment = 1;
  else
    {
      fprintf (stderr,
               "%s: alignment must be left, center, or right, not \"%s\"\n",
               progname, alignment_str);
      exit (1);
    }
}


void
draw_sws (ModeInfo *mi)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!sc->glx_context)
    return;

  if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
    XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);

  glDrawBuffer (GL_BACK);
  glXMakeCurrent (dpy, window, *(sc->glx_context));

  glClear (GL_COLOR_BUFFER_BIT);

  glPushMatrix ();

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (-0.5 * MI_WIDTH(mi),  0.5 * MI_WIDTH(mi),
           -0.5 * MI_HEIGHT(mi), 0.5 * MI_HEIGHT(mi),
           -100.0, 100.0);
  glRotatef (sc->star_theta, 0.0, 0.0, 1.0);
  glCallList (sc->star_list);

  glLoadIdentity ();
  gluPerspective (80.0, 4.0/3.0, 10, 500000);
  glMatrixMode (GL_MODELVIEW);
  gluLookAt (0.0, 0.0, 4600.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glRotatef (-60.0, 1.0, 0.0, 0.0);

  /* The above gives us an arena where the bottom edge of the screen is
     represented by the line (-2100,-3140,0) - ( 2100,-3140,0). */

  /* Now let's move the origin to the front of the screen. */
  glTranslatef (0.0, -3140, 0.0);

  /* And then let's scale so that the bottom of the screen is 1.0 wide. */
  glScalef (4200, 4200, 4200);

  /* Scroll to current position */
  glTranslatef (0.0, sc->intra_line_scroll, 0.0);

  glColor3f (1.0, 1.0, 0.4);
  glCallList (sc->text_list);

  sc->intra_line_scroll += sc->line_height / scroll_steps;

  if (sc->intra_line_scroll >= sc->line_height)
    {
      sc->intra_line_scroll = 0;

      /* Drop the oldest line off the end. */
      if (sc->lines[0])
        free (sc->lines[0]);

      /* Scroll the contents of the lines array toward 0. */
      if (sc->total_lines > 0)
        {
          for (i = 1; i < sc->total_lines; i++)
            sc->lines[i-1] = sc->lines[i];
          sc->lines[--sc->total_lines] = 0;
        }

      /* Bring in new lines at the end. */
      get_more_lines (sc);

      if (sc->total_lines < max_lines)
        /* Oops, we ran out of text... well, insert some blank lines
           here so that new text still pulls in from the bottom of
           the screen, isntead of just appearing. */
        sc->total_lines = max_lines;

      glDeleteLists (sc->text_list, 1);
      sc->text_list = glGenLists (1);
      glNewList (sc->text_list, GL_COMPILE);
      glPushMatrix ();
      glScalef (sc->font_scale, sc->font_scale, sc->font_scale);
      for (i = 0; i < sc->total_lines; i++)
        {
          int offscreen_lines = 3;

          double x = -0.5;
          double y =  ((sc->total_lines - (i + offscreen_lines) - 1)
                       * sc->line_height);
          double xoff = 0;
          char *line = sc->lines[i];
#if 0
          char n[20];
          sprintf(n, "%d:", i);
          draw_string (x / sc->font_scale, y / sc->font_scale, n);
#endif
          if (!line || !*line)
            continue;

          if (alignment >= 0)
            xoff = 1.0 - (glutStrokeLength(GLUT_FONT, line) * sc->font_scale);
          if (alignment == 0)
            xoff /= 2;

          draw_string ((x + xoff) / sc->font_scale, y / sc->font_scale, line);
        }
      glPopMatrix ();
      glEndList ();
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);

  sc->star_theta += star_spin;
}

#endif /* USE_GL */
