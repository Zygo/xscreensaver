/* quasicrystal, Copyright (c) 2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Overlapping sine waves create interesting plane-tiling interference
 * patterns.  Created by jwz, Jul 2013.  Inspired by
 * http://mainisusuallyafunction.blogspot.com/2011/10/quasicrystals-as-sums-of-waves-in-plane.html
 */


#define DEFAULTS	"*delay:	30000       \n" \
			"*spin:         True        \n" \
			"*wander:       True        \n" \
			"*symmetric:    True        \n" \
			"*count:        17          \n" \
			"*contrast:     30          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define free_quasicrystal 0
# define release_quasicrystal 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPEED  "1.0"

typedef struct {
  rotator *rot, *rot2;
  GLuint texid;
} plane;

typedef struct {
  GLXContext *glx_context;
  Bool button_down_p;
  Bool symmetric_p;
  GLfloat contrast;
  int count;
  int ncolors, ccolor;
  XColor *colors;
  plane *planes;
  int mousex, mousey;

} quasicrystal_configuration;

static quasicrystal_configuration *bps = NULL;

static GLfloat speed;

static XrmOptionDescRec opts[] = {
  { "-spin",         ".spin",      XrmoptionNoArg, "True"  },
  { "+spin",         ".spin",      XrmoptionNoArg, "False" },
  { "-wander",       ".wander",    XrmoptionNoArg, "True"  },
  { "+wander",       ".wander",    XrmoptionNoArg, "False" },
  { "-symmetry",     ".symmetric", XrmoptionNoArg, "True"   },
  { "-no-symmetry",  ".symmetric", XrmoptionNoArg, "False"  },
  { "-speed",        ".speed",     XrmoptionSepArg, 0 },
  { "-contrast",     ".contrast",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt quasicrystal_opts = {countof(opts), opts, countof(vars), vars, NULL};



/* Window management, etc
 */
ENTRYPOINT void
reshape_quasicrystal (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, 1, 1, 0, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef (0.5, 0.5, 0);
  glScalef (h, 1, 1);
  if (width > height)
    glScalef (1/h, 1/h, 1);
  glTranslatef (-0.5, -0.5, 0);
  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
quasicrystal_handle_event (ModeInfo *mi, XEvent *event)
{
  quasicrystal_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      bp->button_down_p = True;
      bp->mousex = event->xbutton.x;
      bp->mousey = event->xbutton.y;
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&	/* wheel up or right */
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button7))
    {
    UP:
      if (bp->contrast <= 0) return False;
      bp->contrast--;
      return True;
    }
  else if (event->xany.type == ButtonPress &&	/* wheel down or left */
           (event->xbutton.button == Button5 ||
            event->xbutton.button == Button6))
    {
    DOWN:
      if (bp->contrast >= 100) return False;
      bp->contrast++;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      /* Dragging up and down tweaks contrast */

      int dx = event->xmotion.x - bp->mousex;
      int dy = event->xmotion.y - bp->mousey;

      if (abs(dy) > abs(dx))
        {
          bp->contrast += dy / 40.0;
          if (bp->contrast < 0)   bp->contrast = 0;
          if (bp->contrast > 100) bp->contrast = 100;
        }

      bp->mousex = event->xmotion.x;
      bp->mousey = event->xmotion.y;
      return True;
    }
  else
    {
      if (event->xany.type == KeyPress)
        {
          KeySym keysym;
          char c = 0;
          XLookupString (&event->xkey, &c, 1, &keysym, 0);
          if (c == '<' || c == ',' || c == '-' || c == '_' ||
              keysym == XK_Left || keysym == XK_Up || keysym == XK_Prior)
            goto UP;
          else if (c == '>' || c == '.' || c == '=' || c == '+' ||
                   keysym == XK_Right || keysym == XK_Down ||
                   keysym == XK_Next)
            goto DOWN;
        }

      return screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event);
    }

  return False;
}



ENTRYPOINT void 
init_quasicrystal (ModeInfo *mi)
{
  quasicrystal_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  unsigned char *tex_data = 0;
  int tex_width;
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_quasicrystal (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glDisable (GL_DEPTH_TEST);
  glEnable (GL_CULL_FACE);

  bp->count = MI_COUNT(mi);
  if (bp->count < 1) bp->count = 1;

  if (! wire)
    {
      unsigned char *o;

      tex_width = 4096;
      glGetIntegerv (GL_MAX_TEXTURE_SIZE, &tex_width);
      if (tex_width > 4096) tex_width = 4096;

      tex_data = (unsigned char *) calloc (4, tex_width);
      o = tex_data;
      for (i = 0; i < tex_width; i++)
        {
          unsigned char y = 255 * (1 + sin (i * M_PI * 2 / tex_width)) / 2;
          *o++ = y;
          *o++ = y;
          *o++ = y;
          *o++ = 255;
        }
    }

  bp->symmetric_p =
    get_boolean_resource (MI_DISPLAY (mi), "symmetry", "Symmetry");

  bp->contrast = get_float_resource (MI_DISPLAY (mi), "contrast", "Contrast");
  if (bp->contrast < 0 || bp->contrast > 100) 
    {
      fprintf (stderr, "%s: contrast must be between 0 and 100%%.\n", progname);
      bp->contrast = 0;
    }

  {
    Bool spinp   = get_boolean_resource (MI_DISPLAY (mi), "spin", "Spin");
    Bool wanderp = get_boolean_resource (MI_DISPLAY (mi), "wander", "Wander");
    double spin_speed   = 0.01;
    double wander_speed = 0.0001;
    double spin_accel   = 10.0;
    double scale_speed  = 0.005;

    bp->planes = (plane *) calloc (sizeof (*bp->planes), bp->count);

    bp->ncolors = 256;  /* ncolors affects color-cycling speed */
    bp->colors = (XColor *) calloc (bp->ncolors, sizeof(XColor));
    make_smooth_colormap (0, 0, 0, bp->colors, &bp->ncolors,
                          False, 0, False);
    bp->ccolor = 0;

    for (i = 0;  i < bp->count; i++)
      {
        plane *p = &bp->planes[i];
        p->rot = make_rotator (0, 0,
                               spinp ? spin_speed : 0,
                               spin_accel,
                               wanderp ? wander_speed : 0,
                               True);
        p->rot2 = make_rotator (0, 0,
                                0, 0,
                               scale_speed,
                               True);
        if (! wire)
          {
            clear_gl_error();

            glGenTextures (1, &p->texid);
            glBindTexture (GL_TEXTURE_1D, p->texid);
            glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
            glTexImage1D (GL_TEXTURE_1D, 0, GL_RGBA,
                          tex_width, 0,
                          GL_RGBA,
                          /* GL_UNSIGNED_BYTE, */
                          GL_UNSIGNED_INT_8_8_8_8_REV,
                          tex_data);
            check_gl_error("texture");

            glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
          }
      }
  }

  if (tex_data) free (tex_data);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT void
draw_quasicrystal (ModeInfo *mi)
{
  quasicrystal_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  double r=0, ps=0;
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;

  glShadeModel(GL_FLAT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable (GL_LIGHTING);
  if (!wire)
    {
      glEnable (GL_TEXTURE_1D);
# ifdef HAVE_JWZGLES
      glEnable (GL_TEXTURE_2D);  /* jwzgles needs this, bleh. */
# endif
    }

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glPushMatrix ();
  glTranslatef (0.5, 0.5, 0);
  glScalef (3, 3, 3);

  if (wire) glScalef (0.2, 0.2, 0.2);

  for (i = 0;  i < bp->count; i++)
    {
      plane *p = &bp->planes[i];
      double x, y, z;
      double scale = (wire ? 10 : 700.0 / bp->count);
      double pscale;

      glPushMatrix();

      get_position (p->rot, &x, &y, &z, !bp->button_down_p);
      glTranslatef((x - 0.5) * 0.3333,
                   (y - 0.5) * 0.3333,
                   0);

      /* With -symmetry, keep the planes' scales in sync.
         Otherwise, they scale independently.
       */
      if (bp->symmetric_p && i > 0)
        pscale = ps;
      else
        {
          get_position (p->rot2, &x, &y, &z, !bp->button_down_p);
          pscale = 1 + (4 * z);
          ps = pscale;
        }

      scale *= pscale;


      /* With -symmetry, evenly distribute the planes' rotation.
         Otherwise, they rotate independently.
       */
      if (bp->symmetric_p && i > 0)
        z = r + (i * M_PI * 2 / bp->count);
      else
        {
          get_rotation (p->rot, &x, &y, &z, !bp->button_down_p);
          r = z;
        }


      glRotatef (z * 360, 0, 0, 1);
      glTranslatef (-0.5, -0.5, 0);

      glColor4f (1, 1, 1, (wire ? 0.5 : 1.0 / bp->count));

      if (!wire)
        glBindTexture (GL_TEXTURE_1D, p->texid);

      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f (0, 0, 1);
      glTexCoord2f (-scale/2,  scale/2); glVertex3f (0, 1, 0);
      glTexCoord2f ( scale/2,  scale/2); glVertex3f (1, 1, 0);
      glTexCoord2f ( scale/2, -scale/2); glVertex3f (1, 0, 0);
      glTexCoord2f (-scale/2, -scale/2); glVertex3f (0, 0, 0);
      glEnd();

      if (wire)
        {
          float j;
          glDisable (GL_TEXTURE_1D);
          glColor4f (1, 1, 1, 1.0 / bp->count);
          for (j = 0; j < 1; j += (1 / scale))
            {
              glBegin (GL_LINES);
              glVertex3f (j, 0, 0);
              glVertex3f (j, 1, 0);
              mi->polygon_count++;
              glEnd();
            }
        }

      glPopMatrix();

      mi->polygon_count++;
    }

  /* Colorize the grayscale image. */
  {
    GLfloat c[4];
    c[0] = bp->colors[bp->ccolor].red   / 65536.0;
    c[1] = bp->colors[bp->ccolor].green / 65536.0;
    c[2] = bp->colors[bp->ccolor].blue  / 65536.0;
    c[3] = 1;

    /* Brighten the colors. */
    c[0] = (0.6666 + c[0]/3);
    c[1] = (0.6666 + c[1]/3);
    c[2] = (0.6666 + c[2]/3);

    glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);
    glDisable (GL_TEXTURE_1D);
    glColor4fv (c);
    glTranslatef (-0.5, -0.5, 0);
    glBegin (GL_QUADS);
    glVertex3f (0, 1, 0);
    glVertex3f (1, 1, 0);
    glVertex3f (1, 0, 0);
    glVertex3f (0, 0, 0);
    glEnd();
    mi->polygon_count++;
  }

  /* Clip the colors to simulate contrast. */

  if (bp->contrast > 0)
    {
      /* If c > 0, map 0 - 100 to 0.5 - 1.0, and use (s & ~d) */
      GLfloat c = 1 - (bp->contrast / 2 / 100.0);
      glDisable (GL_TEXTURE_1D);
      glDisable (GL_BLEND);
      glEnable (GL_COLOR_LOGIC_OP);
      glLogicOp (GL_AND_REVERSE);
      glColor4f (c, c, c, 1);
      glBegin (GL_QUADS);
      glVertex3f (0, 1, 0);
      glVertex3f (1, 1, 0);
      glVertex3f (1, 0, 0);
      glVertex3f (0, 0, 0);
      glEnd();
      mi->polygon_count++;
      glDisable (GL_COLOR_LOGIC_OP);
    }

  /* Rotate colors. */
  bp->ccolor++;
  if (bp->ccolor >= bp->ncolors)
    bp->ccolor = 0;


  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE ("QuasiCrystal", quasicrystal)

#endif /* USE_GL */
