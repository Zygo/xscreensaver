/* bouncingcow, Copyright (c) 2003-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Boing, boing, boing.  Cow, cow, cow.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        1           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

/* #define DEBUG */


# define refresh_cow 0
# define release_cow 0
#define DEF_SPEED       "1.0"
#define DEF_TEXTURE     "(none)"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)
#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

#include "xlockmore.h"
#include "rotator.h"
#include "gltrackball.h"
#include "xpm-ximage.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gllist.h"

extern struct gllist
 *cow_face, *cow_hide, *cow_hoofs, *cow_horns, *cow_tail, *cow_udder;

static struct gllist **all_objs[] = {
 &cow_face, &cow_hide, &cow_hoofs, &cow_horns, &cow_tail, &cow_udder
};

#define FACE	0
#define HIDE	1
#define HOOFS	2
#define HORNS	3
#define TAIL	4
#define UDDER	5

typedef struct {
  GLfloat x, y, z;
  GLfloat ix, iy, iz;
  GLfloat dx, dy, dz;
  GLfloat ddx, ddy, ddz;
  rotator *rot;
  Bool spinner_p;
} floater;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint *dlists;
  GLuint texture;

  int nfloaters;
  floater *floaters;

} cow_configuration;

static cow_configuration *bps = NULL;

static GLfloat speed;
static const char *do_texture;

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",     XrmoptionSepArg, 0 },
  {"-texture",     ".texture",   XrmoptionSepArg, 0 },
  {"+texture",     ".texture",   XrmoptionNoArg, "(none)" },
};

static argtype vars[] = {
  {&speed,      "speed",      "Speed",   DEF_SPEED,     t_Float},
  {&do_texture, "texture",    "Texture", DEF_TEXTURE,   t_String},
};

ENTRYPOINT ModeSpecOpt cow_opts = {countof(opts), opts, countof(vars), vars, NULL};


#define BOTTOM 28.0

static void
reset_floater (ModeInfo *mi, floater *f)
{
  cow_configuration *bp = &bps[MI_SCREEN(mi)];

  f->y = -BOTTOM;
  f->x = f->ix;
  f->z = f->iz;

  /* Yes, I know I'm varying the force of gravity instead of varying the
     launch velocity.  That's intentional: empirical studies indicate
     that it's way, way funnier that way. */

  f->dy = 5.0;
  f->dx = 0;
  f->dz = 0;

  /* -0.18 max  -0.3 top -0.4 middle  -0.6 bottom */
  f->ddy = speed * (-0.6 + BELLRAND(0.45));
  f->ddx = 0;
  f->ddz = 0;

  f->spinner_p = !(random() % (12 * bp->nfloaters));

  if (! (random() % (30 * bp->nfloaters)))
    {
      f->dx = BELLRAND(1.8) * RANDSIGN();
      f->dz = BELLRAND(1.8) * RANDSIGN();
    }
}


static void
tick_floater (ModeInfo *mi, floater *f)
{
  cow_configuration *bp = &bps[MI_SCREEN(mi)];

  if (bp->button_down_p) return;

  f->dx += f->ddx;
  f->dy += f->ddy;
  f->dz += f->ddz;

  f->x += f->dx * speed;
  f->y += f->dy * speed;
  f->z += f->dz * speed;

  if (f->y < -BOTTOM ||
      f->x < -BOTTOM*8 || f->x > BOTTOM*8 ||
      f->z < -BOTTOM*8 || f->z > BOTTOM*8)
    reset_floater (mi, f);
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_cow (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
cow_handle_event (ModeInfo *mi, XEvent *event)
{
  cow_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (bp->trackball, event->xbutton.button, 10,
                              !event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      gltrackball_track (bp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


/* Textures
 */

static Bool
load_texture (ModeInfo *mi, const char *filename)
{
  Display *dpy = mi->dpy;
  Visual *visual = mi->xgwa.visual;
  Colormap cmap = mi->xgwa.colormap;
  char buf[1024];
  XImage *image;

  if (MI_IS_WIREFRAME(mi))
    return False;

  if (!filename ||
      !*filename ||
      !strcasecmp (filename, "(none)"))
    {
      glDisable (GL_TEXTURE_2D);
      return False;
    }

  image = xpm_file_to_ximage (dpy, visual, cmap, filename);

  clear_gl_error();
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                image->width, image->height, 0,
                GL_RGBA,
                /* GL_UNSIGNED_BYTE, */
                GL_UNSIGNED_INT_8_8_8_8_REV,
                image->data);
  sprintf (buf, "texture: %.100s (%dx%d)",
           filename, image->width, image->height);
  check_gl_error(buf);

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, image->width);
  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_T);
  glEnable(GL_TEXTURE_2D);
  return True;
}


ENTRYPOINT void
init_cow (ModeInfo *mi)
{
  cow_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bps) {
    bps = (cow_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (cow_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_cow (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
/*      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};*/
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  bp->trackball = gltrackball_init ();

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      GLfloat black[4] = {0, 0, 0, 1};
      const struct gllist *gll = *all_objs[i];

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glBindTexture (GL_TEXTURE_2D, 0);

      if (i == HIDE)
        {
          GLfloat color[4] = {0.63, 0.43, 0.36, 1.00};

          if (load_texture (mi, do_texture))
            {
              glBindTexture (GL_TEXTURE_2D, bp->texture);
              glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
              glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
              glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

              /* approximately line it up with ../images/earth.xpm */
              glMatrixMode(GL_TEXTURE);
              glTranslatef(0.45, 0.58, 0);
              glScalef(0.08, 0.16, 1);
              glRotatef(-5, 0, 0, 1);
              glMatrixMode(GL_MODELVIEW);

              /* if we have a texture, make the base color be white. */
              color[0] = color[1] = color[2] = 1.0;
            }

          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            black);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           128);
        }
      else if (i == TAIL)
        {
          GLfloat color[4] = {0.63, 0.43, 0.36, 1.00};
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            black);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           128);
        }
      else if (i == UDDER)
        {
          GLfloat color[4] = {1.00, 0.53, 0.53, 1.00};
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            black);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           128);
        }
      else if (i == HOOFS || i == HORNS)
        {
          GLfloat color[4] = {0.20, 0.20, 0.20, 1.00};
          GLfloat spec[4]  = {0.30, 0.30, 0.30, 1.00};
          GLfloat shiny    = 8.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else if (i == FACE)
        {
          GLfloat color[4] = {0.10, 0.10, 0.10, 1.00};
          GLfloat spec[4]  = {0.10, 0.10, 0.10, 1.00};
          GLfloat shiny    = 8.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }
      else
        {
          GLfloat color[4] = {1.00, 1.00, 1.00, 1.00};
          GLfloat spec[4]  = {1.00, 1.00, 1.00, 1.00};
          GLfloat shiny    = 128.0;
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,            spec);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS,           shiny);
        }

      renderList (gll, wire);

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

  bp->nfloaters = MI_COUNT (mi);
  bp->floaters = (floater *) calloc (bp->nfloaters, sizeof (floater));

  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f = &bp->floaters[i];
      f->rot = make_rotator (10.0, 0, 0,
                             4, 0.05 * speed,
                             True);
      if (bp->nfloaters == 2)
        {
          f->x = (i ? 6 : -6);
        }
      else if (i != 0)
        {
          double th = (i - 1) * M_PI*2 / (bp->nfloaters-1);
          double r = 10;
          f->x = r * cos(th);
          f->z = r * sin(th);
        }

      f->ix = f->x;
      f->iy = f->y;
      f->iz = f->z;
      reset_floater (mi, f);
    }
}


static void
draw_floater (ModeInfo *mi, floater *f)
{
  cow_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat n;
  double x, y, z;

  get_position (f->rot, &x, &y, &z, !bp->button_down_p);

  glPushMatrix();
  glTranslatef (f->x, f->y, f->z);

  glRotatef (y * 360, 0.0, 1.0, 0.0);
  if (f->spinner_p)
    {
      glRotatef (x * 360, 1.0, 0.0, 0.0);
      glRotatef (z * 360, 0.0, 0.0, 1.0);
    }

  n = 1.5;
  if      (bp->nfloaters > 99) n *= 0.05;
  else if (bp->nfloaters > 25) n *= 0.18;
  else if (bp->nfloaters > 9)  n *= 0.3;
  else if (bp->nfloaters > 1)  n *= 0.7;
  glScalef(n, n, n);

  glCallList (bp->dlists[FACE]);
  mi->polygon_count += (*all_objs[FACE])->points / 3;

  glCallList (bp->dlists[HIDE]);
  mi->polygon_count += (*all_objs[HIDE])->points / 3;

  glCallList (bp->dlists[HOOFS]);
  mi->polygon_count += (*all_objs[HOOFS])->points / 3;

  glCallList (bp->dlists[HORNS]);
  mi->polygon_count += (*all_objs[HORNS])->points / 3;

  glCallList (bp->dlists[TAIL]);
  mi->polygon_count += (*all_objs[TAIL])->points / 3;

  glCallList (bp->dlists[UDDER]);
  mi->polygon_count += (*all_objs[UDDER])->points / 3;

  glPopMatrix();
}



ENTRYPOINT void
draw_cow (ModeInfo *mi)
{
  cow_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  gltrackball_rotate (bp->trackball);

  glScalef (0.5, 0.5, 0.5);

  mi->polygon_count = 0;

# if 0
  {
    floater F;
    F.x = F.y = F.z = 0;
    F.dx = F.dy = F.dz = 0;
    F.ddx = F.ddy = F.ddz = 0;
    F.rot = make_rotator (0, 0, 0, 1, 0, False);
    glScalef(2,2,2);
    draw_floater (mi, &F);
  }
# else
  for (i = 0; i < bp->nfloaters; i++)
    {
      /* "Don't kid yourself, Jimmy.  If a cow ever got the chance,
         he'd eat you and everyone you care about!"
             -- Troy McClure in "Meat and You: Partners in Freedom"
       */
      floater *f = &bp->floaters[i];
      draw_floater (mi, f);
      tick_floater (mi, f);
    }
# endif

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("BouncingCow", bouncingcow, cow)

#endif /* USE_GL */
