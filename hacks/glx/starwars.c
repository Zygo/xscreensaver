/* starwars, Copyright (c) 1998-2012 Jamie Zawinski <jwz@jwz.org> and
 * Claudio Matsuoka <claudio@helllabs.org>
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
 *           far away.
 *
 * Feb 2000 Claudio Matsuoka    First version.
 * Jan 2001 Jamie Zawinski      Rewrote large sections to add the ability to
 *                              run a subprocess, customization of the font
 *                              size and other parameters, etc.
 * Feb 2001 jepler@inetnebr.com Added anti-aliased lines, and fade-to-black.
 * Feb 2005 Jamie Zawinski      Added texture fonts.
 *
 *
 * For the fanboys:
 *
 *     starwars -program 'cat starwars.txt' -columns 25 -no-wrap
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <ctype.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


#include "starwars.h"
#define DEFAULTS "*delay:    40000     \n" \
		 "*showFPS:  False     \n" \
		 "*fpsTop:   True      \n" \
		 "*usePty:   False     \n" \
		 "*font:   " DEF_FONT "\n" \
		 "*textLiteral: " DEF_TEXT "\n"


# define refresh_sws 0
# define sws_handle_event 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "textclient.h"

#ifdef USE_GL /* whole file */

/* Should be in <GL/glext.h> */
# ifndef  GL_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
# endif
# ifndef  GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
# endif


#define DEF_PROGRAM    "xscreensaver-text --cols 0"  /* don't wrap */
#define DEF_LINES      "125"
#define DEF_STEPS      "35"
#define DEF_SPIN       "0.03"
#define DEF_SIZE       "-1"
#define DEF_COLUMNS    "-1"
#define DEF_LINE_WRAP  "True"
#define DEF_ALIGNMENT  "Center"
#define DEF_SMOOTH     "True"
#define DEF_THICK      "True"
#define DEF_FADE       "True"
#define DEF_TEXTURES   "True"
#define DEF_DEBUG      "False"

/* Utopia 800 needs 64 512x512 textures (4096x4096 bitmap).
   Utopia 720 needs 16 512x512 textures (2048x2048 bitmap).
   Utopia 480 needs 16 512x512 textures (2048x2048 bitmap).
   Utopia 400 needs  4 512x512 textures (1024x1024 bitmap).
   Utopia 180 needs  1 512x512 texture.
   Times  240 needs  1 512x512 texture.
 */
#define DEF_FONT       "-*-utopia-bold-r-normal-*-*-720-*-*-*-*-iso8859-1"

#define TAB_WIDTH        8

#define MAX_THICK_LINES   25
#define FONT_WEIGHT       14

#ifndef USE_IPHONE
# define KEEP_ASPECT	/* Letterboxing looks dumb on iPhone. */
#endif

#include "texfont.h"
#include "glutstroke.h"
#include "glut_roman.h"
#define GLUT_FONT (&glutStrokeRoman)

typedef struct {
  Display *dpy;
  GLXContext *glx_context;

  GLuint text_list, star_list;
  texture_font_data *texfont;
  int polygon_count;
  text_data *tc;

  char *buf;
  int buf_size;
  int buf_tail;

  char **lines;
  int total_lines;

  double star_theta;
  double char_width;
  double line_height;
  double font_scale;
  double intra_line_scroll;

  int line_pixel_width;   /* in font units (for wrapping text) */
  int line_pixel_height;  /* in screen units (for computing line thickness) */
  GLfloat line_thickness;

} sws_configuration;


static sws_configuration *scs = NULL;

static char *program;
static int max_lines;
static int scroll_steps;
static float star_spin;
static float font_size;
static int target_columns;
static int wrap_p;
static int smooth_p;
static int thick_p;
static int fade_p;
static int textures_p;
static int debug_p;
static char *alignment_str;
static int alignment;

static XrmOptionDescRec opts[] = {
  {"-program",     ".program",   XrmoptionSepArg, 0 },
  {"-lines",       ".lines",     XrmoptionSepArg, 0 },
  {"-steps",       ".steps",     XrmoptionSepArg, 0 },
  {"-spin",        ".spin",      XrmoptionSepArg, 0 },
  {"-size",	   ".size",      XrmoptionSepArg, 0 },
  {"-columns",	   ".columns",   XrmoptionSepArg, 0 },
/*{"-font",        ".font",      XrmoptionSepArg, 0 },*/
  {"-fade",        ".fade",      XrmoptionNoArg,  "True"   },
  {"-no-fade",     ".fade",      XrmoptionNoArg,  "False"  },
  {"-textures",    ".textures",  XrmoptionNoArg,  "True"   },
  {"-smooth",      ".smooth",    XrmoptionNoArg,  "True"   },
  {"-no-smooth",   ".smooth",    XrmoptionNoArg,  "False"  },
  {"-thick",       ".thick",     XrmoptionNoArg,  "True"   },
  {"-no-thick",    ".thick",     XrmoptionNoArg,  "False"  },
  {"-no-textures", ".textures",  XrmoptionNoArg,  "False"  },
  {"-wrap",	   ".lineWrap",  XrmoptionNoArg,  "True"   },
  {"-no-wrap",	   ".lineWrap",  XrmoptionNoArg,  "False"  },
  {"-nowrap",	   ".lineWrap",  XrmoptionNoArg,  "False"  },
  {"-alignment",   ".alignment", XrmoptionSepArg, 0        },
  {"-left",        ".alignment", XrmoptionNoArg,  "Left"   },
  {"-right",       ".alignment", XrmoptionNoArg,  "Right"  },
  {"-center",      ".alignment", XrmoptionNoArg,  "Center" },
  {"-debug",       ".debug",     XrmoptionNoArg,  "True"   },
};

static argtype vars[] = {
  {&program,        "program",   "Program",    DEF_PROGRAM,   t_String},
  {&max_lines,      "lines",     "Integer",    DEF_LINES,     t_Int},
  {&scroll_steps,   "steps",     "Integer",    DEF_STEPS,     t_Int},
  {&star_spin,      "spin",      "Float",      DEF_SPIN,      t_Float},
  {&font_size,      "size",      "Float",      DEF_SIZE,      t_Float},
  {&target_columns, "columns",   "Integer",    DEF_COLUMNS,   t_Int},
  {&wrap_p,         "lineWrap",  "Boolean",    DEF_LINE_WRAP, t_Bool},
  {&alignment_str,  "alignment", "Alignment",  DEF_ALIGNMENT, t_String},
  {&smooth_p,       "smooth",    "Boolean",    DEF_SMOOTH,    t_Bool},
  {&thick_p,        "thick",     "Boolean",    DEF_THICK,     t_Bool},
  {&fade_p,         "fade",      "Boolean",    DEF_FADE,      t_Bool},
  {&textures_p,     "textures",  "Boolean",    DEF_TEXTURES,  t_Bool},
  {&debug_p,        "debug",     "Boolean",    DEF_DEBUG,     t_Bool},
};

ENTRYPOINT ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Tabs are bad, mmmkay? */

static char *
untabify (const char *string)
{
  const char *ostring = string;
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
      else if (*string == '\010')    /* backspace */
        {
          if (string > ostring)
            out--, string++;
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

static void
strip (char *s, Bool leading, Bool trailing)
{
  int L = strlen(s);
  if (trailing)
    while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\t'))
      s[L--] = 0;
  if (leading)
    {
      char *s2 = s;
      while (*s2 == ' ' || *s2 == '\t')
        s2++;
      if (s == s2)
        return;
      while (*s2)
        *s++ = *s2++;
      *s = 0;
    }
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


static int
string_width (sws_configuration *sc, const char *s)
{
  if (textures_p)
    return texture_string_width (sc->texfont, s, 0);
  else
    return glutStrokeLength (GLUT_FONT, (unsigned char *) s);
}

static int
char_width (sws_configuration *sc, char c)
{
  char s[2];
  s[0] = c;
  s[1] = 0;
  return string_width (sc, s);
}


/* Populates the sc->lines list with as many lines as possible.
 */
static void
get_more_lines (sws_configuration *sc)
{
  /* wrap anyway, if it's absurdly long. */
  int wrap_pix = (wrap_p ? sc->line_pixel_width : 10000);
  
  int col = 0;
  int col_pix = 0;

  char *s = sc->buf;

  int target = sc->buf_size - sc->buf_tail - 2;

  /* Fill as much as we can into sc->buf.
   */
  while (target > 0)
    {
      char c = textclient_getc (sc->tc);
      if (c <= 0)
        break;
      sc->buf[sc->buf_tail++] = c;
      sc->buf[sc->buf_tail] = 0;
      target--;
    }

  while (sc->total_lines < max_lines)
    {
      int cw;

      if (s >= sc->buf + sc->buf_tail)
        /* Reached end of buffer before end of line.  Bail. */
        return;

      cw = char_width (sc, *s);

      if (*s == '\r' || *s == '\n' ||
          col_pix + cw >= wrap_pix)
        {
          int L = s - sc->buf;

          if (*s == '\r' || *s == '\n')
            {
              if (*s == '\r' && s[1] == '\n')  /* swallow CRLF too */
                *s++ = 0;

              *s++ = 0;
            }
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

          if (!textures_p)
            latin1_to_ascii (sc->lines[sc->total_lines]);

          {
            char *t = sc->lines[sc->total_lines];
            char *ut = untabify (t);
            strip (ut, (alignment == 0), 1); /* if centering, strip
                                                leading whitespace too */
            sc->lines[sc->total_lines] = ut;
            free (t);
          }

          sc->total_lines++;

          if (sc->buf_tail > (s - sc->buf))
            {
              int i = sc->buf_tail - (s - sc->buf);
              memmove (sc->buf, s, i);
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
          col_pix = 0;
        }
      else
        {
          col++;
          col_pix += cw;
          if (*s == '\t')
            {
              int tab_pix = TAB_WIDTH * sc->char_width;
              col     = TAB_WIDTH * ((col / TAB_WIDTH) + 1);
              col_pix = tab_pix   * ((col / tab_pix)   + 1);
            }
          s++;
        }
    }
}


static void
draw_string (sws_configuration *sc, GLfloat x, GLfloat y, const char *s)
{
  const char *os = s;
  if (!s || !*s) return;
  glPushMatrix ();
  glTranslatef (x, y, 0);

  if (textures_p)
    print_texture_string (sc->texfont, s);
  else
    while (*s)
      glutStrokeCharacter (GLUT_FONT, *s++);
  glPopMatrix ();

  if (debug_p)
    {
      GLfloat w;
      GLfloat h = sc->line_height / sc->font_scale;
      char c[2];
      c[1]=0;
      s = os;
      if (textures_p) glDisable (GL_TEXTURE_2D);
      glLineWidth (1);
      glColor3f (0.4, 0.4, 0.4);
      glPushMatrix ();
      glTranslatef (x, y, 0);
      while (*s)
        {
          *c = *s++;
          w = string_width (sc, c);
          glBegin (GL_LINE_LOOP);
          glVertex3f (0, 0, 0);
          glVertex3f (w, 0, 0);
          glVertex3f (w, h, 0);
          glVertex3f (0, h, 0);
          glEnd();
          glTranslatef (w, 0, 0);
        }
      glPopMatrix ();
      if (textures_p) glEnable (GL_TEXTURE_2D);
    }
}


static void
grid (double width, double height, double xspacing, double yspacing, double z)
{
  double x, y;
  for (y = 0; y <= height/2; y += yspacing)
    {
      glBegin(GL_LINES);
      glVertex3f(-width/2,  y, z);
      glVertex3f( width/2,  y, z);
      glVertex3f(-width/2, -y, z);
      glVertex3f( width/2, -y, z);
      glEnd();
    }
  for (x = 0; x <= width/2; x += xspacing)
    {
      glBegin(GL_LINES);
      glVertex3f( x, -height/2, z);
      glVertex3f( x,  height/2, z);
      glVertex3f(-x, -height/2, z);
      glVertex3f(-x,  height/2, z);
      glEnd();
    }

  glBegin(GL_LINES);
  glVertex3f(-width, 0, z);
  glVertex3f( width, 0, z);
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

  glBegin(GL_LINES);
  glVertex3f(-width/2,   height/2,  depth/2);
  glVertex3f(-width/2,  -height/2, -depth/2);

  glVertex3f( width/2,   height/2,  depth/2);
  glVertex3f( width/2,  -height/2, -depth/2);

  glVertex3f(-width/2,  -height/2,  depth/2);
  glVertex3f(-width/2,   height/2, -depth/2);

  glVertex3f( width/2,  -height/2,  depth/2);
  glVertex3f( width/2,   height/2, -depth/2);
  glEnd();
}


/* Construct stars (number of stars is dependent on size of screen) */
static void
init_stars (ModeInfo *mi, int width, int height)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];
  int i, j;
  int size = (width > height ? width : height);
  int nstars = size * size / 320;
  int max_size = 3;
  GLfloat inc = 0.5;
  int steps = max_size / inc;

  glDeleteLists (sc->star_list, 1);
  sc->star_list = glGenLists (1);
  glNewList (sc->star_list, GL_COMPILE);

  glEnable(GL_POINT_SMOOTH);

  for (j = 1; j <= steps; j++)
    {
      glPointSize(inc * j);
      glBegin (GL_POINTS);
      for (i = 0; i < nstars / steps; i++)
        {
          glColor3f (0.6 + frand(0.3),
                     0.6 + frand(0.3),
                     0.6 + frand(0.3));
          glVertex2f (2 * size * (0.5 - frand(1.0)),
                      2 * size * (0.5 - frand(1.0)));
        }
      glEnd ();
    }
  glEndList ();
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_sws (ModeInfo *mi, int width, int height)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];

  /* Set up matrices for perspective text display
   */
  {
    GLfloat desired_aspect = (GLfloat) 3/4;
    int w = mi->xgwa.width;
    int h = mi->xgwa.height;
    int yoff = 0;
    GLfloat rot = current_device_rotation();

#ifdef KEEP_ASPECT
    {
      int h2 = w * desired_aspect;
      yoff = (h - h2) / 2;      /* Wide window: letterbox at top and bottom. */
      if (yoff < 0) yoff = 0;   /* Tall window: clip off the top. */
      h = h2;
    }
#endif

    glMatrixMode (GL_PROJECTION);
    glViewport (0, yoff, w, h);

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    gluPerspective (80.0, 1/desired_aspect, 1000, 55000);
    gluLookAt (0.0, 0.0, 4600.0,
               0.0, 0.0, 0.0,
               0.0, 1.0, 0.0);

    glRotatef(rot, 0, 0, 1);

    /* Horrible kludge to prevent the text from materializing already
       on screen on iPhone in landscape mode.
     */
    if ((rot >  45 && rot <  135) ||
        (rot < -45 && rot > -135))
      {
        GLfloat s = 1.1;
        glScalef (s, s, s);
      }

    glRotatef (-60.0, 1.0, 0.0, 0.0);

#if 0
    glRotatef (60.0, 1.0, 0.0, 0.0);
    glTranslatef (260, 3200, 0);
    glScalef (1.85, 1.85, 1);
#endif

    /* The above gives us an arena where the bottom edge of the screen is
       represented by the line (-2100,-3140,0) - ( 2100,-3140,0). */

    /* Now let's move the origin to the front of the screen. */
    glTranslatef (0.0, -3140, 0.0);

    /* And then let's scale so that the bottom of the screen is 1.0 wide. */
    glScalef (4200, 4200, 4200);
  }


  /* Compute the height in pixels of the line at the bottom of the screen. */
  {
    GLdouble mm[17], pm[17];
    GLint vp[5];
    GLdouble x = 0.5, y1 = 0, z = 0;
    GLdouble y2 = sc->line_height;
    GLdouble wx=-1, wy1=-1, wy2=-1, wz=-1;

    glGetDoublev (GL_MODELVIEW_MATRIX, mm);
    glGetDoublev (GL_PROJECTION_MATRIX, pm);
    glGetIntegerv (GL_VIEWPORT, vp);
    gluProject (x, y1, z, mm, pm, vp, &wx, &wy1, &wz);
    gluProject (x, y2, z, mm, pm, vp, &wx, &wy2, &wz);
    sc->line_pixel_height = (wy2 - wy1);
    glLineWidth (1);
  }

  /* Compute the best looking line thickness for the bottom line.
   */
  if (!thick_p)
    sc->line_thickness = 1.0;
  else
    sc->line_thickness = (GLfloat) sc->line_pixel_height / FONT_WEIGHT;

  if (sc->line_thickness < 1.2)
    sc->line_thickness = 1.0;
}


static void
gl_init (ModeInfo *mi)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];

  program = get_string_resource (mi->dpy, "program", "Program");

  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);

  if (smooth_p)
    {
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);
    }

  sc->text_list = glGenLists (1);
  glNewList (sc->text_list, GL_COMPILE);
  glEndList ();

  sc->star_list = glGenLists (1);
  glNewList (sc->star_list, GL_COMPILE);
  glEndList ();

  sc->line_thickness = 1.0;
}


ENTRYPOINT void 
init_sws (ModeInfo *mi)
{
  double font_height;

  sws_configuration *sc = 0;

  if (!scs) {
    scs = (sws_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (sws_configuration));
    if (!scs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  sc = &scs[MI_SCREEN(mi)];

  sc->dpy = MI_DISPLAY(mi);
  sc = &scs[MI_SCREEN(mi)];
  sc->lines = (char **) calloc (max_lines+1, sizeof(char *));

  if ((sc->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    reshape_sws (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

    init_stars (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  if (textures_p)
    {
      int cw, lh;
      sc->texfont = load_texture_font (MI_DISPLAY(mi), "font");
      cw = texture_string_width (sc->texfont, "n", &lh);
      sc->char_width = cw;
      font_height = lh;
      glEnable(GL_ALPHA_TEST);
      glEnable (GL_TEXTURE_2D);

      check_gl_error ("loading font");

      /* "Anistropic filtering helps for quadrilateral-angled textures.
         A sharper image is accomplished by interpolating and filtering
         multiple samples from one or more mipmaps to better approximate
         very distorted textures.  This is the next level of filtering
         after trilinear filtering." */
      if (smooth_p && 
          strstr ((char *) glGetString(GL_EXTENSIONS),
                  "GL_EXT_texture_filter_anisotropic"))
      {
        GLfloat anisotropic = 0.0;
        glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropic);
        if (anisotropic >= 1.0)
          glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
                           anisotropic);
      }
    }
  else
    {
      font_height = GLUT_FONT->top - GLUT_FONT->bottom;
      sc->char_width = glutStrokeWidth (GLUT_FONT, 'z'); /* 'n' seems wide */
    }
  
  sc->font_scale = 1.0 / sc->char_width;


  /* We consider a font that consumes 80 columns to be "18 points".

     If neither -size nor -columns was specified, default to 60 columns
     (which is 24 points.)

     If both were specified, -columns has priority.
   */
  {
    int base_col  = 80;
    int base_size = 18;

    if (target_columns <= 0 && font_size <= 0)
      target_columns = 60;

    if (target_columns > 0)
      font_size = base_size * (base_col / (double) target_columns);
    else if (font_size > 0)
      target_columns = base_col * (base_size / (double) font_size);
  }

  sc->line_pixel_width = target_columns * sc->char_width;

  sc->font_scale /= target_columns;
  sc->line_height = font_height * sc->font_scale;


  /* Buffer only two lines of text.
     If the buffer is too big, there's a significant delay between
     when the program launches and when the text appears, which can be
     irritating for time-sensitive output (clock, current music, etc.)
   */
  sc->buf_size = target_columns * 2;
  if (sc->buf_size < 80) sc->buf_size = 80;
  sc->buf = (char *) calloc (1, sc->buf_size);

  sc->total_lines = max_lines-1;

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

  sc->tc = textclient_open (sc->dpy);

  /* one more reshape, after line_height has been computed */
  reshape_sws (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


static void
draw_stars (ModeInfo *mi)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];

  glMatrixMode (GL_PROJECTION);
  glPushMatrix ();
  {
    glLoadIdentity ();

    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    {
      glLoadIdentity ();
      glOrtho (-0.5 * MI_WIDTH(mi),  0.5 * MI_WIDTH(mi),
               -0.5 * MI_HEIGHT(mi), 0.5 * MI_HEIGHT(mi),
               -100.0, 100.0);
      glRotatef (sc->star_theta, 0.0, 0.0, 1.0);
      if (textures_p) glDisable (GL_TEXTURE_2D);

      /* Keep the stars pointing in the same direction after rotation */
      glRotatef(current_device_rotation(), 0, 0, 1);

      glCallList (sc->star_list);
      if (textures_p) glEnable (GL_TEXTURE_2D);
    }
    glPopMatrix ();
  }
  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();
}

ENTRYPOINT void
draw_sws (ModeInfo *mi)
{
  sws_configuration *sc = &scs[MI_SCREEN(mi)];
/*  XtAppContext app = XtDisplayToApplicationContext (sc->dpy);*/
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!sc->glx_context)
    return;

  glDrawBuffer (GL_BACK);
  glXMakeCurrent (dpy, window, *(sc->glx_context));

  glClear (GL_COLOR_BUFFER_BIT);

  draw_stars (mi);

  glMatrixMode (GL_MODELVIEW);
  glPushMatrix ();

# ifdef USE_IPHONE
  /* Need to do this every time to get device rotation right */
  reshape_sws (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
# endif

  if (debug_p)
    {
      int i;
      glPushMatrix ();
      if (textures_p) glDisable (GL_TEXTURE_2D);
      glLineWidth (1);
      glTranslatef (0,-1, 0);

      glColor3f(1, 0, 0);	/* Red line is where text appears */
      glPushMatrix();
      glTranslatef(0, -0.028, 0);
      glLineWidth (4);
      glBegin(GL_LINES);
      glVertex3f(-0.5,  1, 0);
      glVertex3f( 0.5,  1, 0);
      glVertex3f(-0.5, -1, 0);
      glVertex3f( 0.5, -1, 0);
      glEnd();
      glLineWidth (1);
      glPopMatrix();

      glColor3f (0.4, 0.4, 0.4);
      for (i = 0; i < 16; i++)
        {
          box (1, 1, 1);
          grid (1, 1, sc->char_width * sc->font_scale, sc->line_height, 0);
          glTranslatef(0, 1, 0);
        }
      if (textures_p) glEnable (GL_TEXTURE_2D);
      glPopMatrix ();
    }

  /* Scroll to current position */
  glTranslatef (0.0, sc->intra_line_scroll, 0.0);

  glColor3f (1.0, 1.0, 0.4);
  glCallList (sc->text_list);
  mi->polygon_count = sc->polygon_count;

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
      sc->polygon_count = 0;
      glPushMatrix ();
      glScalef (sc->font_scale, sc->font_scale, sc->font_scale);
      for (i = 0; i < sc->total_lines; i++)
        {
          double fade = (fade_p ? 1.0 * i / sc->total_lines : 1.0);
          int offscreen_lines = 2;

          double x = -0.5;
          double y =  ((sc->total_lines - (i + offscreen_lines) - 1)
                       * sc->line_height);
          double xoff = 0;
          char *line = sc->lines[i];

          if (debug_p)
            {
              double xx = x * 1.4;  /* a little more to the left */
              char n[20];
              sprintf(n, "%d:", i);
              draw_string (sc, xx / sc->font_scale, y / sc->font_scale, n);
            }

          if (!line || !*line)
            continue;

          if (sc->line_thickness != 1 && !textures_p)
            {
              int max_thick_lines = MAX_THICK_LINES;
              GLfloat thinnest_line = 1.0;
              GLfloat thickest_line = sc->line_thickness;
              GLfloat range = thickest_line - thinnest_line;
              GLfloat thickness;

              int j = sc->total_lines - i - 1;

              if (j > max_thick_lines)
                thickness = thinnest_line;
              else
                thickness = (thinnest_line +
                             (range * ((max_thick_lines - j) /
                                       (GLfloat) max_thick_lines)));

              glLineWidth (thickness);
            }

          if (alignment >= 0)
            {
              int n = string_width (sc, line);
              xoff = 1.0 - (n * sc->font_scale);
            }

          if (alignment == 0)
            xoff /= 2;

          glColor3f (fade, fade, 0.5 * fade);
          draw_string (sc, (x + xoff) / sc->font_scale, y / sc->font_scale,
                       line);
          if (textures_p)
            sc->polygon_count += strlen (line);
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

ENTRYPOINT void
release_sws (ModeInfo *mi)
{
  if (scs) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
      sws_configuration *sc = &scs[screen];
      if (sc->tc)
        textclient_close (sc->tc);

      /* #### there's more to free here */
    }
    free (scs);
    scs = 0;
  }
  FreeAllGL(mi);
}


XSCREENSAVER_MODULE_2 ("StarWars", starwars, sws)

#endif /* USE_GL */
