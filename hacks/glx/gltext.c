/* gltext, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"GLText"
#define HACK_INIT	init_text
#define HACK_DRAW	draw_text
#define HACK_RESHAPE	reshape_text
#define sws_opts	xlockmore_opts

#define DEF_TEXT        "(default)"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"

#define DEFAULTS	"*delay:	10000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \
			"*text:       " DEF_TEXT   "\n"


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
#include <ctype.h>

#ifdef USE_GL /* whole file */

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif /* HAVE_UNAME */


#include <GL/glu.h>
#include "glutstroke.h"
#include "glut_roman.h"
#define GLUT_FONT (&glutStrokeRoman)


typedef struct {
  GLXContext *glx_context;

  GLfloat rotx, roty, rotz;	   /* current object rotation */
  GLfloat dx, dy, dz;		   /* current rotational velocity */
  GLfloat ddx, ddy, ddz;	   /* current rotational acceleration */
  GLfloat d_max;		   /* max velocity */

  GLuint text_list;

  int ncolors;
  XColor *colors;
  int ccolor;

  Bool spin_x, spin_y, spin_z;
  char *text;

} text_configuration;

static text_configuration *tps = NULL;

static char *text_fmt;
static char *do_spin;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-text",   ".text",   XrmoptionSepArg, 0 },
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {(caddr_t *) &text_fmt,  "text",   "Text",   DEF_TEXT,   t_String},
  {(caddr_t *) &do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {(caddr_t *) &do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
void
reshape_text (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective( 30.0, 1/h, 1.0, 100.0 );
  gluLookAt( 0.0, 0.0, 15.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -15.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
gl_init (ModeInfo *mi)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  static GLfloat pos[4] = {5.0, 5.0, 10.0, 1.0};

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


/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

static void
rotate(GLfloat *pos, GLfloat *v, GLfloat *dv, GLfloat max_v)
{
  double ppos = *pos;

  /* tick position */
  if (ppos < 0)
    ppos = -(ppos + *v);
  else
    ppos += *v;

  if (ppos > 1.0)
    ppos -= 1.0;
  else if (ppos < 0)
    ppos += 1.0;

  if (ppos < 0) abort();
  if (ppos > 1.0) abort();
  *pos = (*pos > 0 ? ppos : -ppos);

  /* accelerate */
  *v += *dv;

  /* clamp velocity */
  if (*v > max_v || *v < -max_v)
    {
      *dv = -*dv;
    }
  /* If it stops, start it going in the other direction. */
  else if (*v < 0)
    {
      if (random() % 4)
	{
	  *v = 0;

	  /* keep going in the same direction */
	  if (random() % 2)
	    *dv = 0;
	  else if (*dv < 0)
	    *dv = -*dv;
	}
      else
	{
	  /* reverse gears */
	  *v = -*v;
	  *dv = -*dv;
	  *pos = -*pos;
	}
    }

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    *dv = -*dv;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (*dv == 0)
	*dv = 0.00001;
      else if (random() & 1)
	*dv *= 1.2;
      else
	*dv *= 0.8;
    }
}


static void
parse_text (ModeInfo *mi)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];

  if (tp->text) free (tp->text);

  if (!text_fmt || !*text_fmt || !strcmp(text_fmt, "(default)"))
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
#  ifdef _AIX
          sprintf(tp->text, "%s\n%s %s.%s",
                  uts.nodename, uts.sysname, uts.version, uts.release);
#  else  /* !_AIX */
          sprintf(tp->text, "%s\n%s %s",
                  uts.nodename, uts.sysname, uts.release);
#  endif /* !_AIX */
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
    }
}


void 
init_text (ModeInfo *mi)
{
  text_configuration *tp;

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

  tp->rotx = frand(1.0) * RANDSIGN();
  tp->roty = frand(1.0) * RANDSIGN();
  tp->rotz = frand(1.0) * RANDSIGN();

  /* bell curve from 0-6 degrees, avg 3 */
  tp->dx = (frand(1) + frand(1) + frand(1)) / (360/2);
  tp->dy = (frand(1) + frand(1) + frand(1)) / (360/2);
  tp->dz = (frand(1) + frand(1) + frand(1)) / (360/2);

  tp->d_max = tp->dx * 2;

  tp->ddx = 0.00006 + frand(0.00003);
  tp->ddy = 0.00006 + frand(0.00003);
  tp->ddz = 0.00006 + frand(0.00003);


  tp->ncolors = 255;
  tp->colors = (XColor *) calloc(tp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        tp->colors, &tp->ncolors,
                        False, 0, False);

  parse_text (mi);

  {
    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') tp->spin_x = 1;
        else if (*s == 'y' || *s == 'Y') tp->spin_y = 1;
        else if (*s == 'z' || *s == 'Z') tp->spin_z = 1;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }
  }

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
      GLfloat lx, ly;
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
                    TUBE_FACES, smooth, wire);
            lx = coord->x;
            ly = coord->y;
          }
      }
      return (int) (ch->right + tube_width/2);
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
        s++;
        lines++;
        if (*s == 0) break;
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


void
draw_text (ModeInfo *mi)
{
  text_configuration *tp = &tps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  static GLfloat color[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat white[4] = {1.0, 1.0, 1.0, 1.0};

  if (!tp->glx_context)
    return;

  if (strchr (text_fmt, '%'))
    {
      static time_t last_update = -1;
      time_t now = time ((time_t *) 0);
      if (now != last_update) /* do it once a second */
        parse_text (mi);
      last_update = now;
    }

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    GLfloat x, y, z;

    if (do_wander)
      {
        static int frame = 0;

#       define SINOID(SCALE,SIZE) \
        ((((1 + sin((frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

        x = SINOID(0.031, 9.0);
        y = SINOID(0.023, 9.0);
        z = SINOID(0.017, 9.0);
        frame++;
        glTranslatef(x, y, z);
      }

    if (tp->spin_x || tp->spin_y || tp->spin_z)
      {
        x = tp->rotx;
        y = tp->roty;
        z = tp->rotz;
        if (x < 0) x = 1 - (x + 1);
        if (y < 0) y = 1 - (y + 1);
        if (z < 0) z = 1 - (z + 1);

        if (tp->spin_x) glRotatef(x * 360, 1.0, 0.0, 0.0);
        if (tp->spin_y) glRotatef(y * 360, 0.0, 1.0, 0.0);
        if (tp->spin_z) glRotatef(z * 360, 0.0, 0.0, 1.0);

        rotate(&tp->rotx, &tp->dx, &tp->ddx, tp->d_max);
        rotate(&tp->roty, &tp->dy, &tp->ddy, tp->d_max);
        rotate(&tp->rotz, &tp->dz, &tp->ddz, tp->d_max);
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

#endif /* USE_GL */
