/*
 * fliptext, Copyright (c) 2005-2011 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

/* Utopia 800 needs 64 512x512 textures (4096x4096 bitmap).
   Utopia 720 needs 16 512x512 textures (2048x2048 bitmap).
   Utopia 480 needs 16 512x512 textures (2048x2048 bitmap).
   Utopia 400 needs  4 512x512 textures (1024x1024 bitmap).
   Utopia 180 needs  1 512x512 texture.
   Times  240 needs  1 512x512 texture.
 */
#define DEF_FONT       "-*-utopia-bold-r-normal-*-*-720-*-*-*-*-iso8859-1"
#define DEF_COLOR      "#00CCFF"

#define DEFAULTS "*delay:        10000      \n" \
		 "*showFPS:      False      \n" \
		 "*wireframe:    False      \n" \
		 "*usePty:       False      \n" \
		 "*font:       " DEF_FONT  "\n" \
		 ".foreground: " DEF_COLOR "\n" \

# define refresh_fliptext 0
# define fliptext_handle_event 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#include "xlockmore.h"
#include "texfont.h"
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
#define DEF_LINES      "8"
#define DEF_FONT_SIZE  "20"
#define DEF_COLUMNS    "80"
#define DEF_ALIGNMENT  "random"
#define DEF_SPEED       "1.0"
#define TAB_WIDTH        8

#define FONT_WEIGHT       14
#define KEEP_ASPECT

typedef enum { NEW, HESITATE, IN, LINGER, OUT, DEAD } line_state;
typedef enum { SCROLL_BOTTOM, SCROLL_TOP, SPIN } line_anim_type;

typedef struct { GLfloat x, y, z; } XYZ;

typedef struct {
  char *text;
  GLfloat width, height;	/* size */
  XYZ from, to, current;	/* start, end, and current position */
  GLfloat fth, tth, cth;	/* rotation around Z */

  int cluster_size;		/* how many lines in this cluster */
  int cluster_pos;		/* position of this line in the cluster */

  line_state state;		/* current motion model */
  int step, steps;		/* progress along this path */
  GLfloat color[4];

} line;


typedef struct {
  Display *dpy;
  GLXContext *glx_context;

  texture_font_data *texfont;
  text_data *tc;

  char *buf;
  int buf_size;
  int buf_tail;

  int char_width;         /* in font units */
  int line_height;	  /* in font units */
  double font_scale;	  /* convert font units to display units */

  int font_wrap_pixels;   /* in font units (for wrapping text) */

  int top_margin, bottom_margin;
  int left_margin, right_margin;

  int nlines;
  int lines_size;
  line **lines;

  line_anim_type anim_type;
  XYZ in, mid, out;
  XYZ rotation;
  GLfloat color[4];

} fliptext_configuration;


static fliptext_configuration *scs = NULL;

static char *program;
static int max_lines, min_lines;
static float font_size;
static int target_columns;
static char *alignment_str;
static int alignment, alignment_random_p;
static GLfloat speed;

static XrmOptionDescRec opts[] = {
  {"-program",     ".program",   XrmoptionSepArg, 0 },
  {"-lines",       ".lines",     XrmoptionSepArg, 0 },
  {"-size",	   ".fontSize",  XrmoptionSepArg, 0 },
  {"-columns",	   ".columns",   XrmoptionSepArg, 0 },
  {"-speed",       ".speed",     XrmoptionSepArg, 0 },
/*{"-font",        ".font",      XrmoptionSepArg, 0 },*/
  {"-alignment",   ".alignment", XrmoptionSepArg, 0 },
  {"-left",        ".alignment", XrmoptionNoArg,  "Left"   },
  {"-right",       ".alignment", XrmoptionNoArg,  "Right"  },
  {"-center",      ".alignment", XrmoptionNoArg,  "Center" },
};

static argtype vars[] = {
  {&program,        "program",   "Program",    DEF_PROGRAM,   t_String},
  {&max_lines,      "lines",     "Integer",    DEF_LINES,     t_Int},
  {&font_size,      "fontSize",  "Float",      DEF_FONT_SIZE, t_Float},
  {&target_columns, "columns",   "Integer",    DEF_COLUMNS,   t_Int},
  {&alignment_str,  "alignment", "Alignment",  DEF_ALIGNMENT, t_String},
  {&speed,	    "speed",     "Speed",      DEF_SPEED,     t_Float},
};

ENTRYPOINT ModeSpecOpt fliptext_opts = {countof(opts), opts, countof(vars), vars, NULL};



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


static int
char_width (fliptext_configuration *sc, char c)
{
  char s[2];
  s[0] = c;
  s[1] = 0;
  return texture_string_width (sc->texfont, s, 0);
}


/* Returns a single line of text from the output buffer of the subprocess,
   taking into account wrapping, centering, etc.  Returns 0 if no complete
   line is currently available.
 */
static char *
get_one_line (fliptext_configuration *sc)
{
  char *result = 0;
  int wrap_pix = sc->font_wrap_pixels;
  int col = 0;
  int col_pix = 0;
  char *s = sc->buf;
  int target = sc->buf_size - sc->buf_tail - 2;

  /* Fill as much as we can into sc->buf, but stop at newline.
   */
  while (target > 0)
    {
      char c = textclient_getc (sc->tc);
      if (c <= 0)
        break;
      sc->buf[sc->buf_tail++] = c;
      sc->buf[sc->buf_tail] = 0;
      target--;
      if (c == '\r' || c == '\n')
        break;
    }

  while (!result)
    {
      int cw;

      if (s >= sc->buf + sc->buf_tail)
        /* Reached end of buffer before end of line.  Bail. */
        return 0;

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

          if (result) abort();
          result = (char *) malloc (L+1);
          memcpy (result, sc->buf, L);
          result[L] = 0;

          {
            char *t = result;
            char *ut = untabify (t);
            strip (ut, (alignment == 0), 1); /* if centering, strip
                                                leading whitespace too */
            result = ut;
            free (t);
          }

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

  return result;
}


static Bool
blank_p (const char *s)
{
  for (; *s; s++)
    if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
      return False;
  return True;
}

/* Reads some text from the subprocess, and creates and returns a `line'
   object.  Adds that object to the lines list.   Returns 0 if no text
   available yet.

   If skip_blanks_p, then keep trying for new lines of text until we
   get one that is not empty.
 */
static line *
make_line (fliptext_configuration *sc, Bool skip_blanks_p)
{
  line *ln;
  char *s;

 AGAIN:
  s = get_one_line (sc);
  if (s && skip_blanks_p && blank_p (s))
    {
      free (s);
      goto AGAIN;
    }

  if (!s) return 0;

  ln = (line *) calloc (1, sizeof(*ln));
  ln->text = s;
  ln->state = NEW;
  ln->width = sc->font_scale * texture_string_width (sc->texfont, s, 0);
  ln->height = sc->font_scale * sc->line_height;

  memcpy (ln->color, sc->color, sizeof(ln->color));

  sc->nlines++;
  if (sc->lines_size <= sc->nlines)
    {
      sc->lines_size = (sc->lines_size * 1.2) + sc->nlines;
      sc->lines = (line **)
        realloc (sc->lines, sc->lines_size * sizeof(*sc->lines));
      if (! sc->lines)
        {
          fprintf (stderr, "%s: out of memory (%d lines)\n",
                   progname, sc->lines_size);
          exit (1);
        }
    }

  sc->lines[sc->nlines-1] = ln;
  return ln;
}


/* frees the object and removes it from the list.
 */
static void
free_line (fliptext_configuration *sc, line *line)
{
  int i;
  for (i = 0; i < sc->nlines; i++)
    if (sc->lines[i] == line)
      break;
  if (i == sc->nlines) abort();
  for (; i < sc->nlines-1; i++)
    sc->lines[i] = sc->lines[i+1];
  sc->lines[i] = 0;
  sc->nlines--;

  free (line->text);
  free (line);
}


static void
draw_line (ModeInfo *mi, line *line)
{
  int wire = MI_IS_WIREFRAME(mi);
  fliptext_configuration *sc = &scs[MI_SCREEN(mi)];

  if (! line->text || !*line->text ||
      line->state == NEW || line->state == HESITATE || line->state == DEAD)
    return;

  glPushMatrix();
  glTranslatef (line->current.x, line->current.y, line->current.z);

  glRotatef (line->cth, 0, 1, 0);

  if (alignment == 1)
    glTranslatef (-line->width, 0, 0);
  else if (alignment == 0)
    glTranslatef (-line->width/2, 0, 0);

  glScalef (sc->font_scale, sc->font_scale, sc->font_scale);

  glColor4f (line->color[0], line->color[1], line->color[2], line->color[3]);

  if (!wire)
    print_texture_string (sc->texfont, line->text);
  else
    {
      int w, h;
      char *s = line->text;
      char c[2];
      c[1]=0;
      glDisable (GL_TEXTURE_2D);
      glColor3f (0.4, 0.4, 0.4);
      while (*s)
        {
          *c = *s++;
          w = texture_string_width (sc->texfont, c, &h);
          glBegin (GL_LINE_LOOP);
          glVertex3f (0, 0, 0);
          glVertex3f (w, 0, 0);
          glVertex3f (w, h, 0);
          glVertex3f (0, h, 0);
          glEnd();
          glTranslatef (w, 0, 0);
        }
    }

#if 0
  glDisable (GL_TEXTURE_2D);
  glColor3f (0.4, 0.4, 0.4);
  glBegin (GL_LINE_LOOP);
  glVertex3f (0, 0, 0);
  glVertex3f (line->width/sc->font_scale, 0, 0);
  glVertex3f (line->width/sc->font_scale, line->height/sc->font_scale, 0);
  glVertex3f (0, line->height/sc->font_scale, 0);
  glEnd();
  if (!wire) glEnable (GL_TEXTURE_2D);
#endif

  glPopMatrix();

  mi->polygon_count += strlen (line->text);
}

static void
tick_line (fliptext_configuration *sc, line *line)
{
  int stagger = 30;            /* frames of delay between line spin-outs */
  int slide   = 600;           /* frames in a slide in/out */
  int linger  = 0;             /* frames to pause with no motion */
  double i, ii;

  if (line->state >= DEAD) abort();
  if (++line->step >= line->steps)
    {
      line->state++;
      line->step = 0;

      if (linger == 0 && line->state == LINGER)
        line->state++;

      if (sc->anim_type != SPIN)
        stagger *= 2;

      switch (line->state)
        {
        case HESITATE:			/* entering state HESITATE */
          switch (sc->anim_type)
            {
            case SPIN:
              line->steps = (line->cluster_pos * stagger);
              break;
            case SCROLL_TOP:
              line->steps = stagger * (line->cluster_size - line->cluster_pos);
              break;
            case SCROLL_BOTTOM:
              line->steps = stagger * line->cluster_pos;
              break;
            default:
              abort();
            }
          break;

        case IN:
          line->color[3] = 0;
          switch (sc->anim_type)
            {
            case SCROLL_BOTTOM:		/* entering state BOTTOM IN */
              line->from = sc->in;
              line->to   = sc->mid;
              line->from.y = (sc->bottom_margin -
                              (line->height *
                               (line->cluster_pos + 1)));
              line->to.y += (line->height *
                             ((line->cluster_size/2.0) - line->cluster_pos));
              line->steps = slide;
              break;

            case SCROLL_TOP:		/* entering state TOP IN */
              line->from = sc->in;
              line->to   = sc->mid;
              line->from.y = (sc->top_margin +
                              (line->height *
                               (line->cluster_size - line->cluster_pos)));
              line->to.y += (line->height *
                             ((line->cluster_size/2.0) - line->cluster_pos));
              line->steps = slide;
              break;

            case SPIN:			/* entering state SPIN IN */
              line->from = sc->in;
              line->to   = sc->mid;
              line->to.y += (line->height *
                             ((line->cluster_size/2.0) - line->cluster_pos));
              line->from.y += (line->height *
                               ((line->cluster_size/2.0) - line->cluster_pos));

              line->fth = 270;
              line->tth = 0;
              line->steps = slide;
              break;

            default:
              abort();
            }
          break;

        case OUT:
          switch (sc->anim_type)
            {
            case SCROLL_BOTTOM:		/* entering state BOTTOM OUT */
              line->from = line->to;
              line->to   = sc->out;
              line->to.y = (sc->top_margin +
                            (line->height *
                             (line->cluster_size - line->cluster_pos)));
              line->steps = slide;
              break;

            case SCROLL_TOP:		/* entering state TOP OUT */
              line->from = line->to;
              line->to   = sc->out;
              line->to.y = (sc->bottom_margin -
                            (line->height *
                             (line->cluster_pos + 1)));
              line->steps = slide;
              break;

            case SPIN:			/* entering state SPIN OUT */
              line->from = line->to;
              line->to   = sc->out;
              line->to.y += (line->height *
                             ((line->cluster_size/2.0) - line->cluster_pos));

              line->fth = line->tth;
              line->tth = -270;
              line->steps = slide;
              break;

            default:
              abort();
            }
          break;

        case LINGER:
          line->from = line->to;
          line->steps = linger;
          break;

        default:
          break;
        }

      line->steps /= speed;
    }

  switch (line->state)
    {
    case IN:
    case OUT:
      i = (double) line->step / line->steps;

      /* Move along the path exponentially, slow side towards the middle. */
      if (line->state == OUT)
        ii = i * i;
      else
        ii = 1 - ((1-i) * (1-i));

      line->current.x = line->from.x + (ii * (line->to.x - line->from.x));
      line->current.y = line->from.y + (ii * (line->to.y - line->from.y));
      line->current.z = line->from.z + (ii * (line->to.z - line->from.z));
      line->cth = line->fth + (ii * (line->tth - line->fth));

      if (line->state == OUT) ii = 1-ii;
      line->color[3] = sc->color[3] * ii;
      break;

    case HESITATE:
    case LINGER:
    case DEAD:
      break;
    default:
      abort();
    }
}


/* Start a new cluster of lines going.
   Pick their anim type, and in, mid, and out positions.
 */
static void
reset_lines (ModeInfo *mi)
{
  fliptext_configuration *sc = &scs[MI_SCREEN(mi)];
  int i;
  line *prev = 0;
  GLfloat minx, maxx, miny, maxy, minz, maxz, maxw, maxh;

  sc->rotation.x = 5 - BELLRAND(10);
  sc->rotation.y = 5 - BELLRAND(10);
  sc->rotation.z = 5 - BELLRAND(10);

  switch (random() % 8)
    {
    case 0:  sc->anim_type = SCROLL_TOP;    break;
    case 1:  sc->anim_type = SCROLL_BOTTOM; break;
    default: sc->anim_type = SPIN;          break;
    }

  minx = sc->left_margin  * 0.9;
  maxx = sc->right_margin * 0.9;

  miny = sc->bottom_margin * 0.9;
  maxy = sc->top_margin    * 0.9;

  minz = sc->left_margin  * 5;
  maxz = sc->right_margin * 2;

  maxw = sc->font_wrap_pixels * sc->font_scale;
  maxh = max_lines * sc->line_height * sc->font_scale;
 
  if (maxw > maxx - minx)
    maxw = maxx - minx;
  if (maxh > maxy - miny)
    maxh = maxy - miny;
      
  if (alignment_random_p)
    alignment = (random() % 3) - 1;

  if      (alignment == -1) maxx -= maxw;
  else if (alignment ==  1) minx += maxw;
  else                      minx += maxw/2, maxx -= maxw/2;

  miny += maxh/2;
  maxy -= maxh/2;

  sc->mid.x = minx + frand (maxx - minx);
  if (sc->anim_type == SPIN)
    sc->mid.y = miny + BELLRAND (maxy - miny);
  else
    sc->mid.y = miny + frand (maxy - miny);

  sc->in.x  = BELLRAND(sc->right_margin * 2) - sc->right_margin;
  sc->out.x = BELLRAND(sc->right_margin * 2) - sc->right_margin;

  sc->in.y  = miny + frand(maxy - miny);
  sc->out.y = miny + frand(maxy - miny);

  sc->in.z  = minz + frand(maxz - minz);
  sc->out.z = minz + frand(maxz - minz);

  sc->mid.z = 0;

  if (sc->anim_type == SPIN && sc->in.z  > 0) sc->in.z  /= 4;
  if (sc->anim_type == SPIN && sc->out.z > 0) sc->out.z /= 4;

  for (i = 0; i < max_lines; i++)
    {
      line *line = make_line (sc, (i == 0));
      if (!line) break;			/* no text available */
      if (i >= min_lines &&
          (!line->text || !*line->text))	/* blank after min */
        break;
    }

  for (i = 0; i < sc->nlines; i++)
    {
      line *line = sc->lines[i];
      if (!prev)
        {
          line->from.y = sc->bottom_margin;
          line->to.y   = 0;
        }
      else
        {
          line->from.y = prev->from.y - prev->height;
          line->to.y   = prev->to.y   - prev->height;
        }
      line->cluster_pos = i;
      line->cluster_size = sc->nlines;
      prev = line;
    }
}


static void
parse_color (ModeInfo *mi, const char *name, const char *s, GLfloat *a)
{
  XColor c;
  if (! XParseColor (MI_DISPLAY(mi), MI_COLORMAP(mi), s, &c))
    {
      fprintf (stderr, "%s: can't parse %s color %s", progname, name, s);
      exit (1);
    }
  a[0] = c.red   / 65536.0;
  a[1] = c.green / 65536.0;
  a[2] = c.blue  / 65536.0;
  a[3] = 1.0;
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_fliptext (ModeInfo *mi, int width, int height)
{
  fliptext_configuration *sc = &scs[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (60.0, 1/h, 0.01, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 2.6,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);

  sc->right_margin = sc->top_margin / h;
  sc->left_margin = -sc->right_margin;
}


ENTRYPOINT void 
init_fliptext (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);

  fliptext_configuration *sc;

  if (!scs) {
    scs = (fliptext_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (fliptext_configuration));
    if (!scs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    sc = &scs[MI_SCREEN(mi)];
    sc->lines = (line **) calloc (max_lines+1, sizeof(char *));
  }

  sc = &scs[MI_SCREEN(mi)];
  sc->dpy = MI_DISPLAY(mi);

  if ((sc->glx_context = init_GL(mi)) != NULL) {
    reshape_fliptext (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */
  }

  program = get_string_resource (mi->dpy, "program", "Program");

  {
    int cw, lh;
    sc->texfont = load_texture_font (MI_DISPLAY(mi), "font");
    check_gl_error ("loading font");
    cw = texture_string_width (sc->texfont, "n", &lh);
    sc->char_width = cw;
    sc->line_height = lh;
  }

  if (!wire)
    {
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);
      glEnable (GL_ALPHA_TEST);
      glEnable (GL_TEXTURE_2D);

      /* "Anistropic filtering helps for quadrilateral-angled textures.
         A sharper image is accomplished by interpolating and filtering
         multiple samples from one or more mipmaps to better approximate
         very distorted textures.  This is the next level of filtering
         after trilinear filtering." */
      if (strstr ((char *) glGetString(GL_EXTENSIONS),
                  "GL_EXT_texture_filter_anisotropic"))
      {
        GLfloat anisotropic = 0.0;
        glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropic);
        if (anisotropic >= 1.0)
          glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
                           anisotropic);
      }
    }
  
  /* The default font is (by fiat) "18 points".
     Interpret the user's font size request relative to that.
   */
  sc->font_scale = 3 * (font_size / 18.0);

  if (target_columns <= 2) target_columns = 2;

  /* Figure out what the wrap column should be, in font-coordinate pixels.
     Compute it from the given -columns value, but don't let it be wider
     than the screen.
   */
  {
    GLfloat maxw = 110 * sc->line_height / sc->font_scale;  /* magic... */
    sc->font_wrap_pixels = target_columns * sc->char_width;
    if (sc->font_wrap_pixels > maxw ||
        sc->font_wrap_pixels <= 0)
      sc->font_wrap_pixels = maxw;
  }

  sc->buf_size = target_columns * max_lines;
  sc->buf = (char *) calloc (1, sc->buf_size);

  alignment_random_p = False;
  if (!alignment_str || !*alignment_str ||
      !strcasecmp(alignment_str, "left"))
    alignment = -1;
  else if (!strcasecmp(alignment_str, "center") ||
           !strcasecmp(alignment_str, "middle"))
    alignment = 0;
  else if (!strcasecmp(alignment_str, "right"))
    alignment = 1;
  else if (!strcasecmp(alignment_str, "random"))
    alignment = -1, alignment_random_p = True;

  else
    {
      fprintf (stderr,
               "%s: alignment must be left/center/right/random, not \"%s\"\n",
               progname, alignment_str);
      exit (1);
    }

  sc->tc = textclient_open (sc->dpy);

  if (max_lines < 1) max_lines = 1;
  min_lines = max_lines * 0.66;
  if (min_lines > max_lines - 3) min_lines = max_lines - 4;
  if (min_lines < 1) min_lines = 1;

  parse_color (mi, "foreground",
               get_string_resource(mi->dpy, "foreground", "Foreground"),
               sc->color);

  sc->top_margin = (sc->char_width * 100);
  sc->bottom_margin = -sc->top_margin;
  reshape_fliptext (mi, MI_WIDTH(mi), MI_HEIGHT(mi));  /* compute left/right */
}


ENTRYPOINT void
draw_fliptext (ModeInfo *mi)
{
  fliptext_configuration *sc = &scs[MI_SCREEN(mi)];
/*  XtAppContext app = XtDisplayToApplicationContext (sc->dpy);*/
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!sc->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sc->glx_context));

#if 0
  if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
    XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);
#endif

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;

  glPushMatrix();
  glRotatef(current_device_rotation(), 0, 0, 1);
  {
    GLfloat s = 3.0 / (sc->top_margin - sc->bottom_margin);
    glScalef(s, s, s);
  }

  glRotatef (sc->rotation.x, 1, 0, 0);
  glRotatef (sc->rotation.y, 0, 1, 0);
  glRotatef (sc->rotation.z, 0, 0, 1);

#if 0
  glDisable (GL_TEXTURE_2D);
  glColor3f (1,1,1);
  glBegin (GL_LINE_LOOP);
  glVertex3f (sc->left_margin,  sc->top_margin, 0);
  glVertex3f (sc->right_margin, sc->top_margin, 0);
  glVertex3f (sc->right_margin, sc->bottom_margin, 0);
  glVertex3f (sc->left_margin,  sc->bottom_margin, 0);
  glEnd();
  glBegin (GL_LINES);
  glVertex3f (sc->in.x,  sc->top_margin,    sc->in.z);
  glVertex3f (sc->in.x,  sc->bottom_margin, sc->in.z);
  glVertex3f (sc->mid.x, sc->top_margin,    sc->mid.z);
  glVertex3f (sc->mid.x, sc->bottom_margin, sc->mid.z);
  glVertex3f (sc->out.x, sc->top_margin,    sc->out.z);
  glVertex3f (sc->out.x, sc->bottom_margin, sc->out.z);
  glEnd();
  glEnable (GL_TEXTURE_2D);
#endif

  for (i = 0; i < sc->nlines; i++)
    {
      line *line = sc->lines[i];
      draw_line (mi, line);
      tick_line (sc, line);
    }

  for (i = sc->nlines-1; i >= 0; i--)
    {
      line *line = sc->lines[i];
      if (line->state == DEAD)
        free_line (sc, line);
    }

  if (sc->nlines == 0)
    reset_lines (mi);

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);
}

ENTRYPOINT void
release_fliptext (ModeInfo *mi)
{
  if (scs) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
      fliptext_configuration *sc = &scs[screen];
      if (sc->tc)
        textclient_close (sc->tc);

      /* #### there's more to free here */
    }
    free (scs);
    scs = 0;
  }
  FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("FlipText", fliptext)

#endif /* USE_GL */
