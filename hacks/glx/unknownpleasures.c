/* unknownpleasures, Copyright (c) 2013-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Translated from Mathematica code by Archery:
 * http://intothecontinuum.tumblr.com/post/27443100682/in-july-1967-astronomers-at-the-cavendish
 *
 * Interestingly, the original image is copyright-free:
 * http://adamcap.com/2011/05/19/history-of-joy-division-unknown-pleasures-album-art/
 * https://en.wikipedia.org/wiki/Unknown_Pleasures
 *
 * TODO:
 *
 * - Performance is not great. Spending half our time in compute_line()
 *   and half our time in glEnd().  It's a vast number of cos/pow calls,
 *   and a vast number of polygons.  I'm not sure how much could be cached.
 *
 * - There's too low periodicity vertically on the screen.
 *
 * - There's too low periodicity in time.
 *
 * - Could take advantage of time periodicity for caching: just save every
 *   poly for an N second loop.  That would be a huge amount of RAM though.
 *
 * - At low resolutions, GL_POLYGON_OFFSET_FILL seems to work poorly
 *   and the lines get blocky.
 */


#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        80          \n" \
			"*resolution:   100         \n" \
			"*ortho:        True        \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*geometry:    =800x800"   "\n" \

# define refresh_unk 0
# define release_unk 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPEED  "1.0"


typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  Bool orthop;
  GLfloat resolution;
  int count;
  GLfloat t;
} unk_configuration;

static unk_configuration *bps = NULL;

static GLfloat speed;

static XrmOptionDescRec opts[] = {
  { "-speed",        ".speed",      XrmoptionSepArg, 0 },
  { "-resolution",   ".resolution", XrmoptionSepArg, 0 },
  { "-ortho",        ".ortho",      XrmoptionNoArg,  "True"  },
  { "-no-ortho",     ".ortho",      XrmoptionNoArg,  "False" },
};

static argtype vars[] = {
  {&speed, "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt unk_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Window management, etc
 */
ENTRYPOINT void
reshape_unk (ModeInfo *mi, int width, int height)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  if (bp->orthop)
    {
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluPerspective (1.0, 1/h, 690, 710);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      gluLookAt( 0, 0, 700,
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
      gluPerspective (30.0, 1/h, 20.0, 40.0);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      gluLookAt( 0.0, 0.0, 30.0,
                 0.0, 0.0, 0.0,
                 0.0, 1.0, 0.0);
      if (width < height)
        glScalef (1/h, 1/h, 1);
    }

  glClear(GL_COLOR_BUFFER_BIT);
}


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



ENTRYPOINT void 
init_unk (ModeInfo *mi)
{
  unk_configuration *bp;

  if (!bps) {
    bps = (unk_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (unk_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->orthop = get_boolean_resource (MI_DISPLAY (mi), "ortho", "Ortho");
  bp->resolution = get_float_resource (MI_DISPLAY (mi),
                                       "resolution", "Resolution");
  if (bp->resolution < 1) bp->resolution = 1;
  if (bp->resolution > 300) bp->resolution = 300;

  reshape_unk (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->count = MI_COUNT(mi);
  if (bp->count < 1) bp->count = 1;

  bp->trackball = gltrackball_init (False);

  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}



static double
R (double f)
{
  /* A simple, fast, deterministic PRNG.
     ya_rand_init() is too slow for this, and the stream of numbers here
     doesn't have to be high quality.
   */
#if 1
  int seed0 = 1613287;

# else
  /* Kluge to let me pick a good seed factor by trial and error... */
  static int seed0 = 0;
  static int count = 0;
  if (count++ > 150000000) seed0 = 0, count=0;
  if (! seed0)
    {
      seed0 = (random() & 0xFFFFF);
      fprintf(stderr, "seed0 = %8x %d\n", seed0, seed0);
    }
# endif

  long seed = seed0 * f;
  seed = 48271 * (seed % 44488) - 3399 * (seed / 44488);
  f = (double) seed / 0x7FFFFFFF;

  return f;
}


static void
compute_line (ModeInfo *mi,
              int xmin, int xmax, GLfloat xinc,
              GLfloat res_scale,
              int y,
              GLfloat *points)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];

  GLfloat fx;
  int lastx = -999999;

  /* Compute the points on the line */

  for (fx = xmin; fx < xmax; fx += xinc)
    {
      int x = fx;
      int n;
      GLfloat hsum = 0, vsum = 0;

      if (x == lastx) continue;
      lastx = x;

      for (n = 1; n <= 30; n++)
        {
          /* This sum affects crinkliness of the lines */
          hsum += (10 *
                   sin (2 * M_PI
                        * R (4 * y)
                        + bp->t
                        + R (2 * n * y)
                        * 2 * M_PI)
                   *  exp (-pow ((.3 * (x / res_scale) + 30
                                  - 1 * 100 * R (2 * n * y)),
                                 2)
                           / 20.0));
        }

      for (n = 1; n <= 4; n++)
        {
          /* This sum affects amplitude of the peaks */
          vsum += (3 * (1 + R (3 * n * y))
                   * fabs (sin (bp->t + R (n * y)
                                * 2 * M_PI))
                   * exp (-pow (((x / res_scale) - 1 * 100 * R (n * y)),
                                2)
                          / 20.0));
        }

      /* Scale of this affects amplitude of the flat parts */
      points[x - xmin] = (0.2 * sin (2 * M_PI * R (6 * y) + hsum) + vsum);
    }

}


ENTRYPOINT void
draw_unk (ModeInfo *mi)
{
  unk_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat aspect = 1.5;

  GLfloat res_scale = 4;
  int xmin = -50 * res_scale;
  int xmax = 150 * res_scale;
  GLfloat xinc = 100.0 / (bp->resolution / res_scale);
  int ymin = 1;
  int ytop = MI_COUNT(mi);
  int yinc = 1;
  int y;
  GLfloat *points;

  if (xinc < 0.25) xinc = 0.25;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;

  glShadeModel (GL_FLAT);
  glEnable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);

  glPushMatrix ();

  glRotatef(current_device_rotation(), 0, 0, 1);

  gltrackball_rotate (bp->trackball);
  glScalef (10, 10, 10);

  glRotatef (-45, 1, 0, 0);
  glTranslatef (-0.5, -0.5, 0);
  if (bp->orthop)
    glTranslatef (0, 0.05, 0);
  else
    glTranslatef (0, 0.15, 0);

  points = (GLfloat *) malloc (sizeof(*points) * (xmax - xmin));

  if (!bp->button_down_p)
    {
      double s = 6.3 / 19 / 3;
# if 1
      bp->t -= s * speed;
      if (bp->t <= 0)
        bp->t = s * 18 * 3;
# else
      bp->t += s;
# endif
    }

  glLineWidth (2);

  /* Lower the resolution to get a decent frame rate on iPhone 4s */
  if (mi->xgwa.width <= 640 || mi->xgwa.height <= 640)
    {
      ytop *= 0.6;
      xinc *= 3;
    }

# ifdef USE_IPHONE
  /* Lower it even further for iPhone 3 */
  if (mi->xgwa.width <= 480 || mi->xgwa.height <= 480)
    {
      ytop *= 0.8;
      xinc *= 1.2;
    }

  /* Performance just sucks on iPad 3, even with a very high xinc. WTF? */
/*
  if (mi->xgwa.width >= 2048 || mi->xgwa.height >= 2048)
    xinc *= 2;
*/

# endif /* USE_IPHONE */


  /* Make the image fill the screen a little more fully */
  if (mi->xgwa.width <= 640 || mi->xgwa.height <= 640)
    {
      glScalef (1.2, 1.2, 1.2);
      glTranslatef (-0.08, 0, 0);
    }

  if (mi->xgwa.width <= 480 || mi->xgwa.height <= 480)
    glLineWidth (1);


  if (wire)
    xinc *= 1.3;

  /* Draw back mask */
  {
    GLfloat s = 0.99;
    glDisable (GL_POLYGON_OFFSET_FILL);
    glColor3f (0, 0, 0);
    glPushMatrix();
    glTranslatef (0, (1-aspect)/2, -0.005);
    glScalef (1, aspect, 1);
    glTranslatef (0.5, 0.5, 0);
    glScalef (s, s, 1);
    glBegin (GL_QUADS);
    glVertex3f (-0.5, -0.5, 0);
    glVertex3f ( 0.5, -0.5, 0);
    glVertex3f ( 0.5,  0.5, 0);
    glVertex3f (-0.5,  0.5, 0);
    glEnd();
    glPopMatrix();
  }

  if (! wire)
    {
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);

      /* So the masking quads don't interfere with the lines */
      glEnable (GL_POLYGON_OFFSET_FILL);
      glPolygonOffset (1.0, 1.0);
    }

  for (y = ymin; y <= ytop; y += yinc)
    {
      /* Compute all the verts on the line */
      compute_line (mi, xmin, xmax, xinc, res_scale, y, points);

      /* Draw the line segments; then draw the black shielding quads. */
      {
        GLfloat yy = y / (GLfloat) ytop;
        int linesp;

        yy = (yy * aspect) - ((aspect - 1) / 2);

        for (linesp = 0; linesp <= 1; linesp++)
          {
            GLfloat fx;
            int lastx = -999999;

            GLfloat c = (linesp || wire ? 1 : 0);
            glColor3f (c, c, c);

            glBegin (linesp
                     ? GL_LINE_STRIP
                     : wire ? GL_LINES : GL_QUAD_STRIP);
            lastx = -999999;
            for (fx = xmin; fx < xmax; fx += xinc)
              {
                int x = fx;
                GLfloat xx = (x - xmin) / (GLfloat) (xmax - xmin);
                GLfloat zz = points [x - xmin];

                if (x == lastx) continue;
                lastx = x;

                zz /= 80;
                glVertex3f (xx, yy, zz);
                if (! linesp)
                  glVertex3f (xx, yy, 0);
                mi->polygon_count++;
              }
            glEnd ();
          }
      }
    }

  free (points);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("UnknownPleasures", unknownpleasures, unk)

#endif /* USE_GL */
