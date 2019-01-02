/* blocktube, Copyright (c) 2003 Lars Damerow <lars@oddment.org>
 *
 * Based on Jamie Zawinski's original dangerball code.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEBUG 1

#define DEFAULTS        "*delay:	40000           \n" \
                        "*wireframe:    False           \n" \
			"*showFPS:      False           \n" \
			"*suppressRotationAnimation: True\n" \

# define release_blocktube 0
# define blocktube_handle_event xlockmore_no_events
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include <math.h>
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_HOLDTIME    "1000"
#define DEF_CHANGETIME  "200"
#define MAX_ENTITIES     1000
#define DEF_TEXTURE     "True"
#define DEF_FOG         "True"

#if defined(USE_XPM) || defined(USE_XPMINC) || defined(STANDALONE)
/* USE_XPM & USE_XPMINC in xlock mode ; HAVE_XPM in xscreensaver mode */
#include "ximage-loader.h"
#define I_HAVE_XPM

#include "images/gen/blocktube_png.h"
#endif /* HAVE_XPM */

typedef struct {
    int id, r, g, b;
    GLfloat tVal;
    int age;
    int lifetime;
    GLfloat position[3];
    GLfloat angle;
    GLfloat angularVelocity;
} entity;

typedef struct {
  GLXContext *glx_context;
  GLuint  block_dlist;
  int nextID;

  entity entities[MAX_ENTITIES];
  float targetR, targetG, targetB,
    currentR, currentG, currentB,
    deltaR, deltaG, deltaB;
  int counter;
  int changing;
  GLfloat zoom;
  GLfloat tilt;
  GLuint envTexture;
  XImage *texti;

  GLfloat tunnelLength;
  GLfloat tunnelWidth;
  int polys;

} blocktube_configuration;

static blocktube_configuration *lps = NULL;

static GLint holdtime;
static GLint changetime;
static int do_texture;
static int do_fog;

static XrmOptionDescRec opts[] = {
    { "-holdtime",  ".holdtime",  XrmoptionSepArg, 0 },
    { "-changetime",  ".changetime",  XrmoptionSepArg, 0 },
    {"-texture",     ".texture",   XrmoptionNoArg, "True" },
    {"+texture",     ".texture",   XrmoptionNoArg, "False" },
    {"-fog",         ".fog",       XrmoptionNoArg, "True" },
    {"+fog",         ".fog",       XrmoptionNoArg, "False" },
};

static argtype vars[] = {
    {&holdtime, "holdtime",  "Hold Time",  DEF_HOLDTIME,  t_Int},
    {&changetime, "changetime",  "Change Time",  DEF_CHANGETIME, \
     t_Int},
    {&do_texture, "texture",    "Texture", DEF_TEXTURE,   t_Bool},
    {&do_fog,     "fog",        "Fog",     DEF_FOG,       t_Bool},
};

static OptionStruct desc[] = {
    {"-holdtime", "how long to stay on the same color"},
    {"-changetime", "how long it takes to fade to a new color"},
};

ENTRYPOINT ModeSpecOpt blocktube_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct blocktube_description =
    {"blocktube", "init_blocktube", "draw_blocktube", (char *)NULL,
     "draw_blocktube", "init_blocktube", "free_blocktube", &blocktube_opts,
     40000, 30, 1, 1, 64, 1.0, "",
     "A shifting tunnel of reflective blocks", 0, NULL};
#endif /* USE_MODULES */

#if defined( I_HAVE_XPM )
static Bool LoadGLTextures(ModeInfo *mi)
{
    blocktube_configuration *lp = &lps[MI_SCREEN(mi)];
    Bool status;

    status = True;
    glGenTextures(1, &lp->envTexture);
    glBindTexture(GL_TEXTURE_2D, lp->envTexture);
    lp->texti = image_data_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
                                     blocktube_png, sizeof(blocktube_png));
    if (!lp->texti) {
        status = False;
    } else {
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lp->texti->width, lp->texti->height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, lp->texti->data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
# ifndef HAVE_JWZGLES /* #### Sphere maps unimplemented */
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
        glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
# endif
    }
    return status;
}
#endif

static void newTargetColor(blocktube_configuration *lp)
{
    int luminance = 0;

    while (luminance <= 150) {
        lp->targetR = random() % 256;
        lp->targetG = random() % 256;
        lp->targetB = random() % 256;
        lp->deltaR = (lp->targetR - lp->currentR) / changetime;
        lp->deltaG = (lp->targetG - lp->currentG) / changetime;
        lp->deltaB = (lp->targetB - lp->currentB) / changetime;
        luminance = 0.3 * lp->targetR + 0.59 * lp->targetG + 0.11 * lp->targetB;
    }
}

static void randomize_entity (blocktube_configuration *lp, entity *ent)
{
    ent->id = lp->nextID++;
    ent->tVal = 1 - ((float)random() / RAND_MAX / 1.5);
    ent->age = 0;
    ent->lifetime = 100;
    ent->angle = random() % 360;
    ent->angularVelocity = 0.5-((float)(random()) / RAND_MAX);
    ent->position[0] = (float)(random()) / RAND_MAX + lp->tunnelWidth;
    ent->position[1] = (float)(random()) / RAND_MAX * 2;
    ent->position[2] = -(float)(random()) / RAND_MAX * lp->tunnelLength;
}

static void entityTick(blocktube_configuration *lp, entity *ent)
{
    ent->angle += ent->angularVelocity;
    ent->position[2] += 0.1;
    if (ent->position[2] > lp->zoom) {
        ent->position[2] = -lp->tunnelLength + ((float)(random()) / RAND_MAX) * 20;
    }
    ent->age += 0.1;
}

static void tick(blocktube_configuration *lp)
{
    lp->counter--;
    if (!lp->counter) {
        if (!lp->changing) {
            newTargetColor(lp);
            lp->counter = changetime;
        } else {
            lp->counter = holdtime;
        }
        lp->changing = (!lp->changing);
    } else {
        if (lp->changing) {
            lp->currentR += lp->deltaR;
            lp->currentG += lp->deltaG;
            lp->currentB += lp->deltaB;
        }
    }
}

static int cube_vertices(float x, float y, float z, int wire);

ENTRYPOINT void reshape_blocktube (ModeInfo *mi, int width, int height);

ENTRYPOINT void init_blocktube (ModeInfo *mi)
{
    int loop;
    GLfloat fogColor[4] = {0,0,0,1};
    blocktube_configuration *lp;
    int wire = MI_IS_WIREFRAME(mi);

    MI_INIT(mi, lps);

    lp = &lps[MI_SCREEN(mi)];
    lp->glx_context = init_GL(mi);

    lp->zoom = 30;
    lp->tilt = 4.5;
    lp->tunnelLength = 200;
    lp->tunnelWidth = 5;

    if (wire) {
      do_fog = False;
      do_texture = False;
      glLineWidth(2);
    }

    lp->block_dlist = glGenLists (1);
    glNewList (lp->block_dlist, GL_COMPILE);
    lp->polys = cube_vertices(0.15, 1.2, 5.25, wire);
    glEndList ();

#if defined( I_HAVE_XPM )
    if (do_texture) {
      if (!LoadGLTextures(mi)) {
        fprintf(stderr, "%s: can't load textures!\n", progname);
        exit(1);
      }
      glEnable(GL_TEXTURE_2D);
    }
#endif

    /* kick on the fog machine */
    if (do_fog) {
      glEnable(GL_FOG);
      glFogi(GL_FOG_MODE, GL_LINEAR);
      glHint(GL_FOG_HINT, GL_NICEST);
      glFogf(GL_FOG_START, 0);
      glFogf(GL_FOG_END, lp->tunnelLength/1.8);
      glFogfv(GL_FOG_COLOR, fogColor);
      glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    }
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearDepth(1.0f);

    if (!do_texture && !wire) {
      /* If there is no texture, the boxes don't show up without a light.
         Though I don't understand why all the blocks come out gray.
       */
      GLfloat pos[4] = {0.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
    }

    lp->counter = holdtime;
    lp->currentR = random() % 256;
    lp->currentG = random() % 256;
    lp->currentB = random() % 256;
    newTargetColor(lp);
    for (loop = 0; loop < MAX_ENTITIES; loop++)
    {
        randomize_entity(lp, &lp->entities[loop]);
    }
    reshape_blocktube(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glFlush();
}

ENTRYPOINT void free_blocktube (ModeInfo *mi)
{
  blocktube_configuration *lp = &lps[MI_SCREEN(mi)];
# if defined ( I_HAVE_XPM )
  if (!lp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *lp->glx_context);
  if (lp->envTexture) glDeleteTextures(1, &lp->envTexture);
  if (lp->texti) XDestroyImage(lp->texti);
  if (glIsList(lp->block_dlist)) glDeleteLists(lp->block_dlist, 1);
  if (lp->envTexture) glDeleteTextures (1, &lp->envTexture);

# endif
}

ENTRYPOINT void reshape_blocktube (ModeInfo *mi, int width, int height)
{
    blocktube_configuration *lp = &lps[MI_SCREEN(mi)];
    GLfloat h = (GLfloat) height / (GLfloat) width;
    int y = 0;

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *lp->glx_context);

    if (width > height * 5) {   /* tiny window: show middle */
      height = width;
      y = -height/2;
      h = height / (GLfloat) width;
    }

    glViewport(0, y, (GLint) width, (GLint) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 1/h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
    {
      int o = (int) current_device_rotation();
      if (o != 0 && o != 180 && o != -180)
        glScalef (1/h, 1/h, 1/h);
    }
# endif
}

static int cube_vertices(float x, float y, float z, int wire)
{
    int polygon_count = 0;
    float x2, y2, z2, nv = 0.7;
    x2 = x/2;
    y2 = y/2;
    z2 = z/2;

    glFrontFace(GL_CW);

    glNormal3f(0, 0, nv);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3f(-x2,  y2,  z2);
    glTexCoord2f(1.0, 0.0); glVertex3f( x2,  y2,  z2);
    glTexCoord2f(1.0, 1.0); glVertex3f( x2, -y2,  z2);
    glTexCoord2f(0.0, 1.0); glVertex3f(-x2, -y2,  z2);
    polygon_count++;
    glEnd();

    glNormal3f(0, 0, -nv);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glTexCoord2f(1.0, 0.0); glVertex3f(-x2, -y2, -z2);
    glTexCoord2f(1.0, 1.0); glVertex3f( x2, -y2, -z2);
    glTexCoord2f(0.0, 1.0); glVertex3f( x2,  y2, -z2);
    glTexCoord2f(0.0, 0.0); glVertex3f(-x2,  y2, -z2);
    polygon_count++;
    glEnd();

    glNormal3f(0, nv, 0);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex3f(-x2,  y2, -z2);
    glTexCoord2f(0.0, 0.0); glVertex3f( x2,  y2, -z2);
    glTexCoord2f(1.0, 0.0); glVertex3f( x2,  y2,  z2);
    glTexCoord2f(1.0, 1.0); glVertex3f(-x2,  y2,  z2);
    polygon_count++;
    glEnd();

    glNormal3f(0, -nv, 0);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glTexCoord2f(1.0, 1.0); glVertex3f(-x2, -y2, -z2);
    glTexCoord2f(0.0, 1.0); glVertex3f(-x2, -y2,  z2);
    glTexCoord2f(0.0, 0.0); glVertex3f( x2, -y2,  z2);
    glTexCoord2f(1.0, 0.0); glVertex3f( x2, -y2, -z2);
    polygon_count++;
    glEnd();

    if (wire) return polygon_count;

    glNormal3f(nv, 0, 0);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glTexCoord2f(1.0, 0.0); glVertex3f( x2, -y2, -z2);
    glTexCoord2f(1.0, 1.0); glVertex3f( x2, -y2,  z2);
    glTexCoord2f(0.0, 1.0); glVertex3f( x2,  y2,  z2);
    glTexCoord2f(0.0, 0.0); glVertex3f( x2,  y2, -z2);
    polygon_count++;
    glEnd();

    glNormal3f(-nv, 0, 0);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3f(-x2, -y2, -z2);
    glTexCoord2f(1.0, 0.0); glVertex3f(-x2,  y2, -z2);
    glTexCoord2f(1.0, 1.0); glVertex3f(-x2,  y2,  z2);
    glTexCoord2f(0.0, 1.0); glVertex3f(-x2, -y2,  z2);
    polygon_count++;
    glEnd();

    return polygon_count;
}

static void draw_block(ModeInfo *mi, entity *ent)
{
    blocktube_configuration *lp = &lps[MI_SCREEN(mi)];
    glCallList (lp->block_dlist);
    mi->polygon_count += lp->polys;
}

ENTRYPOINT void
draw_blocktube (ModeInfo *mi)
{
    blocktube_configuration *lp = &lps[MI_SCREEN(mi)];
    Display *dpy = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
    entity *cEnt = NULL;
    int loop = 0;

    if (!lp->glx_context)
      return;

    mi->polygon_count = 0;

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *lp->glx_context);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (do_texture) {
      glEnable(GL_TEXTURE_GEN_S);
      glEnable(GL_TEXTURE_GEN_T);
      glBindTexture(GL_TEXTURE_2D, lp->envTexture);
    }

    for (loop = 0; loop < MAX_ENTITIES; loop++) {
        cEnt = &lp->entities[loop];

        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, lp->zoom);
        glRotatef(lp->tilt, 1.0f, 0.0f, 0.0f);
        glRotatef(cEnt->angle, 0.0f, 0.0f, 1.0f);
        glTranslatef(cEnt->position[0], cEnt->position[1], cEnt->position[2]);
        glColor4ub((int)(lp->currentR * cEnt->tVal),
                   (int)(lp->currentG * cEnt->tVal),
                   (int)(lp->currentB * cEnt->tVal), 255);
        draw_block(mi, cEnt);
        entityTick(lp, cEnt);
    }
    tick(lp);

    if (mi->fps_p) do_fps (mi);
    glFinish();
    glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE ("BlockTube", blocktube)

#endif /* USE_GL */
