/* Surface --- Parametric 3d surfaces visualization */

/*
 * Revision History:
 * 2000: written by Andrey Mirtchovski <mirtchov@cpsc.ucalgary.ca>
 *       
 * 01-Mar-2003  mirtchov    Modified as a xscreensaver hack.
 * 01-jan-2009  steger      Renamed from klein.c to surfaces.c.
 *                          Removed the Klein bottle.
 *                          Added many new surfaces.
 *                          Added many command line options.
 *
 */

/* surfaces to draw */
#define SURFACE_RANDOM            -1
#define SURFACE_DINI              0
#define SURFACE_ENNEPER           1
#define SURFACE_KUEN              2
#define SURFACE_MOEBIUS           3
#define SURFACE_SEASHELL          4
#define SURFACE_SWALLOWTAIL       5
#define SURFACE_BOHEMIAN          6
#define SURFACE_WHITNEY           7
#define SURFACE_PLUECKER          8
#define SURFACE_HENNEBERG         9
#define SURFACE_CATALAN           10
#define SURFACE_CORKSCREW         11
#define NUM_SURFACES              12

/* primitives to draw with 
 * note that we skip the polygons and
 * triangle fans -- too slow
 *
 * also removed triangle_strip and quads -- 
 * just doesn't look good enough
 */
#define RENDER_RANDOM             -1
#define RENDER_POINTS             0
#define RENDER_LINES              1
#define RENDER_LINE_LOOP          2
#define NUM_RENDER                3

#ifdef STANDALONE
# define DEFAULTS                   "*delay:        20000   \n" \
                                    "*showFPS:      False   \n" \
				   "*suppressRotationAnimation: True\n" \

# define refresh_surface 0
# define release_surface 0
# include "xlockmore.h"     /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"         /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#define DEF_SURFACE      "random"
#define DEF_MODE         "random"
#define DEF_SPIN         "True"
#define DEF_WANDER       "False"
#define DEF_SPEED        "300"

#include "rotator.h"
#include "gltrackball.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


static char *surface_type;
static char *render_mode;
static int render;
static int speed;
static Bool do_spin;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-surface",        ".surface", XrmoptionSepArg, 0 },
  { "-random-surface", ".surface", XrmoptionNoArg,  "random" },
  { "-dini",           ".surface", XrmoptionNoArg,  "dini" },
  { "-enneper",        ".surface", XrmoptionNoArg,  "enneper" },
  { "-kuen",           ".surface", XrmoptionNoArg,  "kuen" },
  { "-moebius",        ".surface", XrmoptionNoArg,  "moebius" },
  { "-seashell",       ".surface", XrmoptionNoArg,  "seashell" },
  { "-swallowtail",    ".surface", XrmoptionNoArg,  "swallowtail" },
  { "-bohemian",       ".surface", XrmoptionNoArg,  "bohemian" },
  { "-whitney",        ".surface", XrmoptionNoArg,  "whitney" },
  { "-pluecker",       ".surface", XrmoptionNoArg,  "pluecker" },
  { "-henneberg",      ".surface", XrmoptionNoArg,  "henneberg" },
  { "-catalan",        ".surface", XrmoptionNoArg,  "catalan" },
  { "-corkscrew",      ".surface", XrmoptionNoArg,  "corkscrew" },
  { "-mode",           ".mode",    XrmoptionSepArg, 0 },
  { "-random-mode",    ".mode",    XrmoptionNoArg,  "random" },
  { "-points",         ".mode",    XrmoptionNoArg,  "points" },
  { "-lines",          ".mode",    XrmoptionNoArg,  "lines" },
  { "-line-loops",     ".mode",    XrmoptionNoArg,  "line-loops" },
  { "-speed",          ".speed",   XrmoptionSepArg, 0 },
  { "-spin",           ".spin",    XrmoptionNoArg, "True" },
  { "+spin",           ".spin",    XrmoptionNoArg, "False" },
  { "-wander",         ".wander",  XrmoptionNoArg, "True" },
  { "+wander",         ".wander",  XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&surface_type, "surface", "Surface", DEF_SURFACE, t_String },
  {&render_mode,  "mode",    "Mode",    DEF_MODE,    t_String },
  {&do_spin,      "spin",    "Spin",    DEF_SPIN,    t_Bool },
  {&do_wander,    "wander",  "Wander",  DEF_WANDER,  t_Bool },
  {&speed,        "speed",   "Speed",   DEF_SPEED,   t_Int },
};


ENTRYPOINT ModeSpecOpt surface_opts =
{countof(opts), opts, countof(vars), vars, NULL};



typedef struct {
  GLfloat x;
  GLfloat y;
  GLfloat z;
} GL_VECTOR;

typedef struct {
  GLXContext *glx_context;
  Window      window;
  rotator    *rot;
  trackball_state *trackball;
  Bool        button_down_p;

  int  render;
  Bool random_render;
  int  surface;
  Bool random_surface;
  int  frame;

  float du, dv;
  float a, b, c;

  float draw_step;
} surfacestruct;

static surfacestruct *surface = NULL;


static void draw(ModeInfo *mi)
{
  surfacestruct *sp = &surface[MI_SCREEN(mi)];
  double u, v;
  float coord[3];
  int render;

  mi->polygon_count = 0;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glPushMatrix();

  {
    double x, y, z;
    get_position(sp->rot, &x, &y, &z, !sp->button_down_p);
    glTranslatef((x-0.5)*10, (y-0.5)*10, (z-0.5)*20);

    gltrackball_rotate(sp->trackball);

    get_rotation(sp->rot, &x, &y, &z, !sp->button_down_p);
    glRotatef(x*360, 1.0, 0.0, 0.0);
    glRotatef(y*360, 0.0, 1.0, 0.0);
    glRotatef(z*360, 0.0, 0.0, 1.0);
  }

  glScalef(4.0, 4.0, 4.0);

  switch(sp->surface)
  {
    case SURFACE_DINI:
      for (v=0.11; v<=2.0; v+=sp->dv)
      {
        glBegin(sp->render);
        for (u=0; u<=6.0*M_PI; u+=sp->du)
        {
          coord[0] = sp->a*cos(u)*sin(v);
          coord[1] = sp->a*sin(u)*sin(v);
          coord[2] = sp->a*(cos(v)+log(tan(0.5*v)))+0.2*sp->b*u;
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_ENNEPER:
      for (u=-M_PI; u<=M_PI; u+=sp->du)
      {
        glBegin(sp->render);
        for (v=-M_PI; v<M_PI; v+=sp->dv)
        {
          coord[0] = sp->a*(u-(1.0/3.0*u*u*u)+u*v*v);
          coord[1] = sp->b*(v-(1.0/3.0*v*v*v)+u*u*v);
          coord[2] = u*u-v*v;
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_KUEN:
      for (u=-4.48; u<=4.48; u+=sp->du)
      {
        glBegin(sp->render);
        for (v=M_PI/51; v<M_PI; v+=sp->dv)
        {
          coord[0] = 2*(cos(u)+u*sin(u))*sin(v)/(1+u*u*sin(v)*sin(v));
          coord[1] = 2*(sin(u)-u*cos(u))*sin(v)/(1+u*u*sin(v)*sin(v));
          coord[2] = log(tan(0.5*v))+2*cos(v)/(1+u*u*sin(v)*sin(v));
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_MOEBIUS:
      for (u=-M_PI; u<M_PI; u+=sp->du)
      {
        glBegin(sp->render);
        for (v=-0.735; v<0.74; v+=sp->dv)
        {
          coord[0] = cos(u)+v*cos(u/2)*cos(u);
          coord[1] = sin(u)+v*cos(u/2)*sin(u);
          coord[2] = v*sin(u/2);
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_SEASHELL:
      for (u=0; u<2*M_PI; u+=sp->du)
      {
        glBegin(sp->render);
        for (v=0; v<2*M_PI; v+=sp->dv)
        {
          coord[0] = sp->a*(1-v/(2*M_PI))*cos(2*v)*(1+cos(u))+sp->c*cos(2*v);
          coord[1] = sp->a*(1-v/(2*M_PI))*sin(2*v)*(1+cos(u))+sp->c*sin(2*v);
          coord[2] = 2*sp->b*v/(2*M_PI)+sp->a*(1-v/(2*M_PI))*sin(u);
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_SWALLOWTAIL:
      for (u=-2.5; u<2.0; u+=sp->du)
      {
        glBegin(sp->render);
        for (v=-1.085; v<1.09; v+=sp->dv)
        {
          coord[0] = u*v*v+3*v*v*v*v;
          coord[1] = -2*u*v-4*v*v*v;
          coord[2] = u;
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_BOHEMIAN:
      for (u=-M_PI; u<M_PI; u+=sp->du)
      {
        glBegin(sp->render);
        for (v=-M_PI; v<M_PI; v+=sp->dv)
        {
          coord[0] = sp->a*cos(u);
          coord[1] = sp->b*cos(v)+sp->a*sin(u);
          coord[2] = sin(v);
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_WHITNEY:
      for (v=-1.995; v<2.0; v+=sp->dv)
      {
        glBegin(sp->render);
        for (u=-1.995; u<2.0; u+=sp->du)
        {
          coord[0] = u*v;
          coord[1] = u;
          coord[2] = v*v-2;
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_PLUECKER:
      for (u=0; u<2.5; u+=sp->dv)
      {
        glBegin(sp->render);
        for (v=-M_PI; v<M_PI; v+=sp->du)
        {
          coord[0] = u*cos(v);
          coord[1] = u*sin(v);
          coord[2] = 2*cos(v)*sin(v);
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_HENNEBERG:
      for (u=0.9; u<2.55; u+=sp->dv)
      {
        glBegin(sp->render);
        for (v=-M_PI; v<M_PI; v+=sp->du)
        {
          coord[0] = sinh(1.0/3.0*u)*cos(v)-1.0/3.0*sinh(u)*cos(3.0*v);
          coord[1] = sinh(1.0/3.0*u)*sin(v)+1.0/3.0*sinh(u)*sin(3.0*v);
          coord[2] = cosh(2.0/3.0*u)*cos(2.0*v);
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_CATALAN:
      for (v=-2; v<2; v+=sp->du)
      {
        glBegin(sp->render);
        for (u=-2*M_PI; u<2*M_PI+0.05; u+=sp->dv)
        {
          coord[0] = 0.33*(u-sin(u)*cosh(v));
          coord[1] = 0.33*(1.0-cos(u)*cosh(v));
          coord[2] = 0.33*4.0*sin(0.5*u)*sinh(0.5*v);
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
    case SURFACE_CORKSCREW:
      for (v=-M_PI; v<M_PI; v+=sp->du)
      {
        glBegin(sp->render);
        for (u=-M_PI; u<M_PI; u+=sp->dv)
        {
          coord[0] = 0.5*(sp->a+2.0)*cos(u)*cos(v);
          coord[1] = 0.5*(sp->a+2.0)*sin(u)*cos(v);
          coord[2] = 0.5*(sp->a+2.0)*sin(v)+u;
          glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
          glVertex3fv(coord);
          mi->polygon_count++;
        }
        glEnd();
      }
      break;
  }
  glPopMatrix();

  if (sp->render == GL_LINES)
    mi->polygon_count /= 2;

  sp->a = sin(sp->draw_step+=0.01);
  sp->b = cos(sp->draw_step+=0.01);
  sp->c = sin(sp->draw_step+0.25*M_PI);

  if (sp->random_surface || sp->random_render)
  {
    sp->frame++;
    if (sp->frame >= speed)
    {
      sp->frame = 0;
      if (sp->random_surface)
        sp->surface = random() % NUM_SURFACES;
      if (sp->random_render)
      {
        render = random() % NUM_RENDER;
        switch (render)
        {
          case RENDER_POINTS:
            sp->render = GL_POINTS;
            break;
          case RENDER_LINES:
            sp->render = GL_LINES;
            break;
          case RENDER_LINE_LOOP:
            if (sp->surface == SURFACE_BOHEMIAN ||
                sp->surface == SURFACE_PLUECKER ||
                sp->surface == SURFACE_HENNEBERG)
              sp->render = GL_LINE_LOOP;
            else
              sp->render = GL_LINE_STRIP;
            break;
          default:
            sp->render = GL_LINE_LOOP;
            break;
        }
      }
    }
  }
}


/* new window size or exposure */
ENTRYPOINT void reshape_surface(ModeInfo *mi, int width, int height)
{
  surfacestruct *sp = &surface[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sp->glx_context));

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(0.0, 0.0, 30.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    
# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool surface_handle_event(ModeInfo *mi, XEvent *event)
{
  surfacestruct *sp = &surface[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, sp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &sp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void init_surface(ModeInfo *mi)
{
  int    screen = MI_SCREEN(mi);
  surfacestruct *sp;

  MI_INIT (mi, surface, NULL);
  sp = &surface[screen];

  sp->window = MI_WINDOW(mi);

  {
    double spin_speed    = 1.0;
    double wander_speed = 0.03;
    sp->rot = make_rotator(do_spin ? spin_speed : 0,
                           do_spin ? spin_speed : 0,
                           do_spin ? spin_speed : 0,
                           1.0,
                           do_wander ? wander_speed : 0,
                           True);
    sp->trackball = gltrackball_init (True);
  }

  if (!strcasecmp(surface_type,"random"))
  {
    sp->random_surface = True;
    sp->surface = random() % NUM_SURFACES;
  }
  else if (!strcasecmp(surface_type,"dini"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_DINI;
  }
  else if (!strcasecmp(surface_type,"enneper"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_ENNEPER;
  }
  else if (!strcasecmp(surface_type,"kuen"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_KUEN;
  }
  else if (!strcasecmp(surface_type,"moebius"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_MOEBIUS;
  }
  else if (!strcasecmp(surface_type,"seashell"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_SEASHELL;
  }
  else if (!strcasecmp(surface_type,"swallowtail"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_SWALLOWTAIL;
  }
  else if (!strcasecmp(surface_type,"bohemian"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_BOHEMIAN;
  }
  else if (!strcasecmp(surface_type,"whitney"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_WHITNEY;
  }
  else if (!strcasecmp(surface_type,"pluecker"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_PLUECKER;
  }
  else if (!strcasecmp(surface_type,"henneberg"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_HENNEBERG;
  }
  else if (!strcasecmp(surface_type,"catalan"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_CATALAN;
  }
  else if (!strcasecmp(surface_type,"corkscrew"))
  {
    sp->random_surface = False;
    sp->surface = SURFACE_CORKSCREW;
  }
  else
  {
    sp->random_surface = True;
    sp->surface = random() % NUM_SURFACES;
  }

  if (!strcasecmp(render_mode,"random"))
  {
    sp->random_render = True;
    render = random() % NUM_RENDER;
  }
  else if (!strcasecmp(render_mode,"points"))
  {
    sp->random_render = False;
    render = RENDER_POINTS;
  }
  else if (!strcasecmp(render_mode,"lines"))
  {
    sp->random_render = False;
    render = RENDER_LINES;
  }
  else if (!strcasecmp(render_mode,"line-loops"))
  {
    sp->random_render = False;
    render = RENDER_LINE_LOOP;
  }
  else
  {
    sp->random_render = True;
    render = random() % NUM_RENDER;
  }

  switch (render)
  {
    case RENDER_POINTS:
      sp->render = GL_POINTS;
      break;
    case RENDER_LINES:
      sp->render = GL_LINES;
      break;
    case RENDER_LINE_LOOP:
      if (sp->surface == SURFACE_BOHEMIAN ||
          sp->surface == SURFACE_PLUECKER ||
          sp->surface == SURFACE_HENNEBERG)
        sp->render = GL_LINE_LOOP;
      else
        sp->render = GL_LINE_STRIP;
      break;
    default:
      sp->render = GL_LINE_LOOP;
      break;
  }

  sp->frame = 0;

  sp->du = 0.07;
  sp->dv = 0.07;
  sp->a = sp->b = 1;
  sp->c = 0.1;

  if ((sp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_surface(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }
  else
  {
    MI_CLEARWINDOW(mi);
  }
}


ENTRYPOINT void draw_surface(ModeInfo * mi)
{
  surfacestruct *sp = &surface[MI_SCREEN(mi)];
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!sp->glx_context)
    return;

  glDrawBuffer(GL_BACK);

  glXMakeCurrent(display, window, *(sp->glx_context));
  draw(mi);
  if (mi->fps_p)
    do_fps(mi);
  glFinish();
  glXSwapBuffers(display, window);
}


XSCREENSAVER_MODULE_2("Surfaces", surfaces, surface)

#endif
