/* unknownpleasures, Copyright (c) 2013-2018, 2019
 * -Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * A very particular waterfall chart.
 *
 * Interestingly, the original image is copyright-free:
 * http://adamcap.com/2011/05/19/history-of-joy-division-unknown-pleasures-album-art/
 * https://blogs.scientificamerican.com/sa-visual/pop-culture-pulsar-origin-story-of-joy-division-s-unknown-pleasures-album-cover-video/
 * https://en.wikipedia.org/wiki/Unknown_Pleasures
 *
 *   "Eighty successive periods of the first pulsar observed, CP1919
 *   (Cambridge pulsar at 19 hours 19 minutes right ascension), are stacked
 *   on top of one another using the average period of 1.33730 seconds in
 *   this computer-generated illustration produced at the Arecibo Radio
 *   Observatory in Puerto Rico. Athough the leading edges of the radio
 *   pulses occur within a few thousandths of a second of the predicted
 *   times, the shape of the pulses is quite irregular. Some of this
 *   irregularity in radio reception is caused by the effects of
 *   transmission through the interstellar medium. The average pulse width
 *   is less than 50 thousandths of a second."
 *
 * TODO:
 *
 * - Take a function generator program as a command line argument:
 *   read lines of N float values from it, interpolate to full width.
 */

#define DEF_ORTHO      "True"
#define DEF_SPEED      "1.0"
#define DEF_RESOLUTION "100"
#define DEF_AMPLITUDE  "0.13"
#define DEF_NOISE      "1.0"
#define DEF_ASPECT     "1.9"
#define DEF_BUZZ       "False"
#define DEF_MASK       "(none)"

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        80          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*geometry:    =800x800"   "\n" \

# define release_unk 0

#include "xlockmore.h"
#include "colors.h"
#include "gltrackball.h"
#include "ximage-loader.h"
#include "grab-ximage.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#undef DEBUG

Bool ortho_arg;
GLfloat speed_arg;
int resolution_arg;
GLfloat amplitude_arg;
GLfloat noise_arg;
GLfloat aspect_arg;
Bool buzz_arg;
char *mask_arg;


typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  Bool orthop;
  double speed, tick;
  int resolution;	/* X points */
  int count;		/* Y lines */
  GLfloat amplitude;	/* Z height */
  int frames;		/* Number of renders of each line, for buzzing */
  GLfloat noise;	/* Number of peaks in each line */
  GLfloat aspect;	/* Shape of the plot */
  GLuint base;		/* Display list for back */
  GLuint *lines;	/* Display lists for each edge * face * frame */
  GLfloat *heights;	/* Animated elevation / alpha of each line */
  GLfloat fg[4], bg[4];	/* Colors */
  XImage *mask;
  double mask_scale;
  int frame_count;
} unk_configuration;

static unk_configuration *bps = NULL;

static XrmOptionDescRec opts[] = {
  { "-speed",        ".speed",      XrmoptionSepArg, 0 },
  { "-resolution",   ".resolution", XrmoptionSepArg, 0 },
  { "-amplitude",    ".amplitude",  XrmoptionSepArg, 0 },
  { "-noise",        ".noise",      XrmoptionSepArg, 0 },
  { "-aspect",       ".aspect",     XrmoptionSepArg, 0 },
  { "-ortho",        ".ortho",      XrmoptionNoArg,  "True"  },
  { "-no-ortho",     ".ortho",      XrmoptionNoArg,  "False" },
  { "-buzz",         ".buzz",       XrmoptionNoArg,  "True"  },
  { "-no-buzz",      ".buzz",       XrmoptionNoArg,  "False" },
  { "-mask",         ".mask",       XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&ortho_arg,      "ortho",       "Ortho",       DEF_ORTHO,      t_Bool},
  {&speed_arg,      "speed",       "Speed",       DEF_SPEED,      t_Float},
  {&resolution_arg, "resolution",  "Resolution",  DEF_RESOLUTION, t_Int},
  {&amplitude_arg,  "amplitude",   "Amplitude",   DEF_AMPLITUDE,  t_Float},
  {&noise_arg,      "noise",       "Noise",       DEF_NOISE,      t_Float},
  {&aspect_arg,     "aspect",      "Aspect",      DEF_ASPECT,     t_Float},
  {&buzz_arg,       "buzz",        "Buzz",        DEF_BUZZ,       t_Bool},
  {&mask_arg,       "mask",        "Image",       DEF_MASK,       t_String},
};

ENTRYPOINT ModeSpecOpt unk_opts = {countof(opts), opts, countof(vars), vars, NULL};



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
parse_color (ModeInfo *mi, char *res, char *class, GLfloat *a)
{
  XColor c;
  char *s = get_string_resource(mi->dpy, res, class);
  if (! XParseColor (MI_DISPLAY(mi), MI_COLORMAP(mi), s, &c))
    {
      fprintf (stderr, "%s: can't parse %s color %s", progname, res, s);
      exit (1);
    }
  if (s) free (s);
  a[0] = c.red   / 65536.0;
  a[1] = c.green / 65536.0;
  a[2] = c.blue  / 65536.0;
  a[3] = 1.0;
}


ENTRYPOINT void
reshape_unk (ModeInfo *mi, int width, int height)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;
  int i;
  int new_count;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width*1.5;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  if (bp->orthop)
    {
      int magic = 700;
      int range = 30; /* Must be small or glPolygonOffset doesn't work. */
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluPerspective (1.0, 1/h, magic-range/2, magic+range/2);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      gluLookAt( 0, 0, magic,
                 0, 0, 0,
                 0, 1, 0);
      if (width < height)
        glScalef (1/h, 1/h, 1);
      glTranslatef (0, -0.5, 0);
    }
  else
    {
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluPerspective (30.0, 1/h, 1.0, 100.0);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      gluLookAt( 0.0, 0.0, 30.0,
                 0.0, 0.0, 0.0,
                 0.0, 1.0, 0.0);
      if (width < height)
        glScalef (1/h, 1/h, 1);
    }

  new_count = MI_COUNT(mi);

# if 0
  /* Lower the resolution to get a decent frame rate on iPhone 4s */
  if (mi->xgwa.width <= 640 || mi->xgwa.height <= 640)
    new_count /= 3;
# endif

  /* Lower it even further for iPhone 3 */
  if (mi->xgwa.width <= 480 || mi->xgwa.height <= 480)
    new_count /= 2;

  if (wire) new_count /= 2;

  if (new_count < 1) new_count = 1;

  if (bp->count != new_count || !bp->lines)
    {
      if (bp->lines)
        {
          for (i = 0; i < bp->count * bp->frames * 2; i++)
            glDeleteLists (bp->lines[i], 1);
          free (bp->lines);
          free (bp->heights);
        }

      bp->count = new_count;
      bp->lines = (GLuint *)
        malloc (bp->count * bp->frames * 2 * sizeof(*bp->lines));
      for (i = 0; i < bp->count * bp->frames * 2; i++)
        bp->lines[i] = glGenLists (1);
      bp->heights = (GLfloat *) calloc (bp->count, sizeof(*bp->heights));
    }

  {
    GLfloat lw = 1;
    GLfloat s = 1;

# ifdef HAVE_MOBILE
    lw = 4;
    s = 1.4;

# else  /* !HAVE_MOBILE */

    if (MI_WIDTH(mi) > 2560) lw = 4;  /* Retina displays */
#  ifdef HAVE_COCOA
    else if (MI_WIDTH(mi) > 1280) lw = 3;  /* WTF */
#  endif
    else if (MI_WIDTH(mi) > 1920) lw = 3;
    else if (mi->xgwa.width > 640 && mi->xgwa.height > 640) lw = 2;

    /* Make the image fill the screen a little more fully */
    if (mi->xgwa.width <= 640 || mi->xgwa.height <= 640)
      s = 1.2;

# endif /* !HAVE_MOBILE */

    s /= 1.9 / bp->aspect;

    glScalef (s, s, s);
    glLineWidth (lw);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}

static void
load_image (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  XImage *image0;
  int x, y;
  double xs, ys;
  unsigned long max = 0;

  if (!mask_arg || !*mask_arg || !strcasecmp(mask_arg, "(none)"))
    return;

  image0 = file_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi), mask_arg);
  if (!image0) return;

  bp->mask = XCreateImage (MI_DISPLAY(mi), MI_VISUAL(mi), 32, ZPixmap, 0, 0,
                           bp->resolution,
                           bp->count * image0->height / image0->width *
                           (1.9 / bp->aspect) * 0.75,
                           32, 0);
  if (!bp->mask) abort();
  bp->mask->data = (char *)
    malloc (bp->mask->height * bp->mask->bytes_per_line);
  if (!bp->mask->data) abort();

  xs = image0->width  / (double) bp->mask->width;
  ys = image0->height / (double) bp->mask->height;

  /* Scale the image down to a 1-bit mask. */
  for (y = 0; y < bp->mask->height; y++)
    for (x = 0; x < bp->mask->width; x++)
      {
        int x2, y2, n = 0;
        double total = 0;
        unsigned long p;
        for (y2 = y * ys; y2 < (y+1) * ys; y2++)
          for (x2 = x * xs; x2 < (x+1) * xs; x2++)
            {
              unsigned long agbr = XGetPixel (image0, x2, y2);
              unsigned long a    = (agbr >> 24) & 0xFF;
              unsigned long gray = (a == 0
                                    ? 0
                                    : ((((agbr >> 16) & 0xFF) +
                                        ((agbr >>  8) & 0xFF) +
                                        ((agbr >>  0) & 0xFF))
                                       / 3));
# if 0
              if (gray < 96) gray /= 2;  /* a little more contrast */
# endif
              total += gray / 255.0;
              n++;
            }
        p = 255 * total / n;
        if (p > max) max = p;
        p = (0xFF << 24) | (p << 16) | (p << 8) | p;
        XPutPixel (bp->mask, x, bp->mask->height-y-1, p);
      }

  bp->mask_scale = 255.0 / max;
  XDestroyImage (image0);
}


# ifdef DEBUG
static GLfloat poly1 = 0, poly2 = 0;
# endif

ENTRYPOINT Bool
unk_handle_event (ModeInfo *mi, XEvent *event)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      (event->xbutton.button == Button4 ||
       event->xbutton.button == Button5 ||
       event->xbutton.button == Button6 ||
       event->xbutton.button == Button7))
    {
      int b = event->xbutton.button;
      int speed = 1;
      if (b == Button6 || b == Button7)
        speed *= 3;
      if (bp->orthop)
        switch (b) {				/* swap axes */
        case Button4: b = Button6; break;
        case Button5: b = Button7; break;
        case Button6: b = Button4; break;
        case Button7: b = Button5; break;
        }
      gltrackball_mousewheel (bp->trackball, b, speed, !!event->xbutton.state);
      return True;
    }

# ifdef DEBUG
  else if (event->type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      GLfloat i = 0.1;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      switch (keysym) {
      case XK_Up:    poly1 += i; break;
      case XK_Down:  poly1 -= i; break;
      case XK_Left:  poly2 += i; break;
      case XK_Right: poly2 -= i; break;
      default: break;
      }
      fprintf (stderr,"%.2f %.2f\n", poly1, poly2);
      return True;
    }
# endif /* DEBUG */

  else if (gltrackball_event_handler (event, bp->trackball,
                                      MI_WIDTH (mi), MI_HEIGHT (mi),
                                      &bp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      bp->orthop = !bp->orthop;
      reshape_unk (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }

  return False;
}


/* Like cos but constrained to [-pi, +pi] */
static GLfloat
cos1 (GLfloat th)
{
  if (th < -M_PI) th = -M_PI;
  if (th >  M_PI) th =  M_PI;
  return cos (th);
}


/* Returns an array of floats for one new row, range [0, 1.0] */
static double *
generate_signal (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  double *points = (double *) calloc (sizeof(*points), bp->resolution);
  double *p;
  int i, j;
  int nspikes = (6 + frand(15)) * bp->noise;
  double step = 1.0 / bp->resolution;
  double r, max;

  for (j = 0; j < nspikes; j++)
    {
      double off = (bp->mask
                    ? frand (1.0) - 0.5    /* all the way to the edge */
                    : frand (0.8) - 0.4);  /* leave a margin */
      double amp = (0.1 + frand (0.9)) * nspikes;
      double freq = (7 + frand (11)) * bp->noise;
      for (i = 0, r = -0.5, p = points;
           i < bp->resolution;
           i++, r += step, p++)
        *p += amp/2 + amp/2 * cos1 ((r + off) * M_PI * 2 * freq);
    }

  /* Avoid clipping */
  max = nspikes;
  if (max <= 0) max = 1;
  for (i = 0, p = points; i < bp->resolution; i++, p++)
    if (max < *p) max = *p;

  /* Multiply by baseline clipping curve, add static. */
  for (i = 0, r = -0.5, p = points; i < bp->resolution; i++, r += step, p++)
    *p = ((*p / max)
          * (0.5 +
             0.5
             * (bp->mask ? 1 : cos1 (r * r * M_PI * 14))
             * (1 - frand(0.2))));

  return points;
}


static void
tick_unk (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  double *points = generate_signal (mi);
  int linesp, frame;

  /* Roll forward by one line (2 dlists per frame) */
  GLuint *cur = (GLuint *) malloc (bp->frames * 2 * sizeof(*cur));
  memcpy (cur, bp->lines, bp->frames * 2 * sizeof(*cur));
  memmove (bp->lines, bp->lines + 2 * bp->frames,
           sizeof(*bp->lines) * (bp->count-1) * bp->frames * 2);
  memcpy (bp->lines + (bp->count-1) * bp->frames * 2, cur,
          bp->frames * 2 * sizeof(*cur));

  memmove (bp->heights, bp->heights + 1,
           sizeof(*bp->heights) * (bp->count-1));
  bp->heights[bp->count-1] = 0;

  /* Regenerate the pair at the bottom. */

  for (frame = 0; frame < bp->frames; frame++)
    {
      mi->polygon_count = 0;
      for (linesp = 0; linesp <= 1; linesp++)
        {
          int i;

          glNewList (cur[frame * 2 + !linesp], GL_COMPILE);
          glBegin (linesp
                   ? GL_LINE_STRIP
                   : wire ? GL_LINES : GL_QUAD_STRIP);
          for (i = 0; i < bp->resolution; i++)
            {
              GLfloat x = i / (GLfloat) bp->resolution;
              GLfloat z = (points[i] + frand (0.05)) * bp->amplitude;

              if (bp->mask)
                {
                  int h = bp->mask->height;  /* leave a 10% gutter */
                  int y = bp->frame_count % (int) (h * 1.1);
                  unsigned long p = (y < h ? XGetPixel (bp->mask, i, y) : 0);
                  unsigned long gray = ((p >> 8) & 0xFF);
                  z *= gray * bp->mask_scale / 255.0;
                }

              if (z < 0) z = 0;
              if (z > bp->amplitude) z = bp->amplitude;
              glVertex3f (x, 0, z);
              if (! linesp)
                glVertex3f (x, 0, 0);
              mi->polygon_count++;
            }
          glEnd ();
          glEndList ();
        }
    }

  bp->frame_count++;
  mi->polygon_count *= bp->count;
  mi->polygon_count += 5;  /* base */

  free (cur);
  free (points);
}


ENTRYPOINT void 
init_unk (ModeInfo *mi)
{
  unk_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->orthop = ortho_arg;
  bp->resolution = resolution_arg;
  bp->amplitude = amplitude_arg;
  bp->noise = noise_arg;
  bp->aspect = aspect_arg;
  bp->speed = speed_arg;
  bp->frames = buzz_arg ? 15 : 1;
  if (bp->resolution < 1) bp->resolution = 1;
  if (bp->resolution > 300) bp->resolution = 300;
  if (bp->amplitude < 0.01) bp->amplitude = 0.01;
  if (bp->amplitude > 1) bp->amplitude = 1;
  if (bp->noise < 0.01) bp->noise = 0.01;
  if (bp->speed <= 0.001) bp->speed = 0.001;

  parse_color (mi, "foreground", "Foreground", bp->fg);
  parse_color (mi, "background", "Background", bp->bg);

  reshape_unk (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->trackball = gltrackball_init (False);

  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
  /* bp->count is set in reshape */

  load_image (mi);

  bp->base = glGenLists (1);
  glNewList (bp->base, GL_COMPILE);
  {
    GLfloat h1 = 0.01;
    GLfloat h2 = 0.02;
    GLfloat h3 = (h1 + h2) / 2;
    GLfloat s = 0.505;
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glVertex3f (-s, -s, -h1);
    glVertex3f ( s, -s, -h1);
    glVertex3f ( s,  s, -h1);
    glVertex3f (-s,  s, -h1);
    glEnd();
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glVertex3f (-s, -s, 0);
    glVertex3f (-s, -s, -h2);
    glVertex3f ( s, -s, -h2);
    glVertex3f ( s, -s, 0);
    glEnd();
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glVertex3f (-s, -s, 0);
    glVertex3f (-s, -s, -h2);
    glVertex3f (-s,  s, -h2);
    glVertex3f (-s,  s, 0);
    glEnd();
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glVertex3f ( s, -s, 0);
    glVertex3f ( s, -s, -h2);
    glVertex3f ( s,  s, -h2);
    glVertex3f ( s,  s, 0);
    glEnd();
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glVertex3f (-s,  s, 0);
    glVertex3f (-s,  s, -h2);
    glVertex3f ( s,  s, -h2);
    glVertex3f ( s,  s, 0);
    glEnd();

    glColor3fv (bp->fg);
    glBegin (GL_LINE_LOOP);
    s -= 0.01;
    glVertex3f (-s, -s, -h3);
    glVertex3f ( s, -s, -h3);
    glVertex3f ( s,  s, -h3);
    glVertex3f (-s,  s, -h3);
    glEnd();
  }
  glEndList ();

  if (! wire)
    {
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);
    }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  for (i = 0; i < bp->count; i++)
    tick_unk (mi);
}


static double
ease_fn (double r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static double
ease_ratio (double r)
{
  double ease = 0.5;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


ENTRYPOINT void
draw_unk (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat step = 1.0 / bp->count;
  double speed = (0.6 / bp->speed) * (80.0 / bp->count);
  double now = double_time();
  double ratio = (now - bp->tick) / speed;
  int frame;
  int i;

  ratio = (ratio < 0 ? 0 : ratio > 1 ? 1 : ratio);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glShadeModel (GL_FLAT);
  glEnable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);

  glPushMatrix ();

  glRotatef(current_device_rotation(), 0, 0, 1);

  gltrackball_rotate (bp->trackball);

  glRotatef (-45, 1, 0, 0);

  i = bp->orthop ? 15 : 17;
  glScalef (i / bp->aspect, i, i / bp->aspect);

  glDisable (GL_POLYGON_OFFSET_FILL);
  if (wire)
    glColor3f (0.5 * bp->fg[0], 0.5 * bp->fg[1], 0.5 * bp->fg[2]);
  else
    glColor4fv (bp->bg);
  glCallList (bp->base);

  /* So the masking quads don't interfere with the lines.
     These numbers are empirical black magic. */
  glEnable (GL_POLYGON_OFFSET_FILL);

# ifdef DEBUG
  glPolygonOffset (poly1, poly2);
# else
  glPolygonOffset (0.5, 0.5);
# endif

  glTranslatef (-0.5, 0.55 + step*ratio, 0);

  frame = random() % bp->frames;
  for (i = 0; i < bp->count; i++)
    {
      int j = i * bp->frames * 2 + frame * 2;
      GLfloat s  = ease_ratio (bp->heights[i]);
      GLfloat s2 = ease_ratio (bp->heights[i] * 1.5);

      glPushMatrix();
      glScalef (1, 1, s);
      glColor4f (bp->fg[0], bp->fg[1], bp->fg[2], s2);
      glCallList (bp->lines[j]);	/* curve */
      s = 1;
      if (wire)
        glColor4f (0.5 * bp->fg[0], 0.5 * bp->fg[1], 0.5 * bp->fg[2], s);
      else
        glColor4f (bp->bg[0], bp->bg[1], bp->bg[2], s);
      glCallList (bp->lines[j+1]);	/* shield */
      glPopMatrix();
      glTranslatef (0, -step, 0);
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  if (!bp->button_down_p)
    {
      /* Set height/fade based on distance from either edge. */
      GLfloat dist = bp->count * 0.05;
      int i;
      for (i = 0; i < bp->count; i++)
        {
          GLfloat i2 = i - ratio;
          GLfloat h = ((i2 < bp->count/2 ? i2 : (bp->count - 1 - i2))
                       / dist);
          bp->heights[i] = (h > 1 ? 1 : h < 0 ? 0 : h);
        }

      if (bp->tick + speed <= now)     /* Add a new row. */
        {
          tick_unk (mi);
          bp->tick = now;
        }
    }

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_unk (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  for (i = 0; i < bp->count * bp->frames * 2; i++)
    glDeleteLists (bp->lines[i], 1);
  free (bp->lines);
  free (bp->heights);
  if (bp->mask) XDestroyImage (bp->mask);
}

XSCREENSAVER_MODULE_2 ("UnknownPleasures", unknownpleasures, unk)

#endif /* USE_GL */
