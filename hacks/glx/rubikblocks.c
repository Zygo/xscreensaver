/* rubikblocks, Copyright (c) 2009 Vasek Potocek <vasek.potocek@post.cz>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* RubikBlocks - a Rubik's Mirror Blocks puzzle introduced in 2008.
 * No mirrors in this version, though, hence the altered name.
 */

/* TODO:
 * add reflection to the faces
 */

#define DEFAULTS   "*delay:         20000         \n" \
                   "*showFPS:       False         \n" \
                   "*wireframe:     False         \n" \
		   "*suppressRotationAnimation: True\n" \

# define free_rubikblocks 0
# define release_rubikblocks 0
#include "xlockmore.h"
#include "rotator.h"
#include "gltrackball.h"

#ifdef USE_GL

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_TEXTURE     "True"
#define DEF_RANDOMIZE   "False"
#define DEF_SPINSPEED   "0.1"
#define DEF_ROTSPEED    "3.0"
#define DEF_WANDERSPEED "0.005"
#define DEF_WAIT        "40.0"
#define DEF_CUBESIZE    "1.0"

#define SHUFFLE 100

#define TEX_WIDTH  64
#define TEX_HEIGHT 64
#define BORDER     5
#define BORDER2    (BORDER*BORDER)

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define rnd01() ((int)(random()%2))

/*************************************************************************/

static Bool spin, wander, rndstart, tex;
static float spinspeed, tspeed, wspeed, twait, size;

static argtype vars[] = {
  { &spin,      "spin",        "Spin",        DEF_SPIN,        t_Bool},
  { &wander,    "wander",      "Wander",      DEF_WANDER,      t_Bool},
  { &rndstart,  "randomize",   "Randomize",   DEF_RANDOMIZE,   t_Bool},
  { &tex,       "texture",     "Texture",     DEF_TEXTURE,     t_Bool},
  { &spinspeed, "spinspeed",   "SpinSpeed",   DEF_SPINSPEED,   t_Float},
  { &tspeed,    "rotspeed",    "RotSpeed",    DEF_ROTSPEED,    t_Float},
  { &wspeed,    "wanderspeed", "WanderSpeed", DEF_WANDERSPEED, t_Float},
  { &twait,     "wait",        "Wait",        DEF_WAIT,        t_Float},
  { &size,      "cubesize",    "CubeSize",    DEF_CUBESIZE,    t_Float},
};

static XrmOptionDescRec opts[] = {
  { "-spin",        ".spin",        XrmoptionNoArg,  "True" },
  { "+spin",        ".spin",        XrmoptionNoArg,  "False" },
  { "-wander",      ".wander",      XrmoptionNoArg,  "True" },
  { "+wander",      ".wander",      XrmoptionNoArg,  "False" },
  { "-randomize",   ".randomize",   XrmoptionNoArg,  "True" },
  { "+randomize",   ".randomize",   XrmoptionNoArg,  "False" },
  { "-texture",     ".texture",     XrmoptionNoArg,  "True" },
  { "+texture",     ".texture",     XrmoptionNoArg,  "False" },
  { "-spinspeed",   ".spinspeed",   XrmoptionSepArg, 0 },
  { "-wanderspeed", ".wanderspeed", XrmoptionSepArg, 0 },
  { "-rotspeed",    ".rotspeed",    XrmoptionSepArg, 0 },
  { "-wait",        ".wait",        XrmoptionSepArg, 0 },
  { "-cubesize",    ".cubesize",    XrmoptionSepArg, 0 },
};

ENTRYPOINT ModeSpecOpt rubikblocks_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   rubikblocks_description =
{ "rubikblocks", "init_rubikblocks", "draw_rubikblocks", NULL,
  "draw_rubikblocks", "change_rubikblocks", NULL, &rubikblocks_opts,
  25000, 1, 1, 1, 1.0, 4, "",
  "Shows randomly shuffling Rubik's Mirror Blocks puzzle", 0, NULL
};
#endif

typedef struct {
  float         pos[3]; /* _original_ position */
  float         qr[4];  /* quaternion of rotation */
  Bool          act;    /* flag if it is undergoing the current rotation */
} piece_t;

typedef struct {
  GLXContext    *glx_context;
  rotator       *rot;
  trackball_state *trackball;
  GLfloat       ratio;
  Bool          button_down;

  Bool          pause;          /* pause between two rotations */
  float         qfram[4];       /* quaternion describing the rotation in one anim. frame */
  GLfloat       t, tmax;        /* rotation clock */
  piece_t       pieces[27];     /* type and tilt of all the pieces */

  unsigned char texture[TEX_HEIGHT][TEX_WIDTH];
  GLuint        list_base;
  Bool          wire;
} rubikblocks_conf;

static rubikblocks_conf *rubikblocks = NULL;

static const GLfloat shininess = 20.0;
static const GLfloat ambient[] = {0.0, 0.0, 0.0, 1.0};
static const GLfloat diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const GLfloat position0[] = {1.0, 1.0, 1.0, 0.0};
static const GLfloat position1[] = {-1.0, -1.0, 1.0, 0.0};
static const GLfloat lmodel_ambient[] = {0.1, 0.1, 0.1, 1.0};
static const GLfloat material_ambient[] = {0.7, 0.7, 0.7, 1.0};
static const GLfloat material_diffuse[] = {0.7, 0.7, 0.7, 1.0};
static const GLfloat material_specular[] = {0.2, 0.2, 0.2, 1.0};

/*************************************************************************/

/* Multiplies two quaternions, src*dest, and stores the result in dest. */
static void
mult_quat(float src[4], float dest[4])
{
  float r, i, j, k;
  r = src[0]*dest[0] - src[1]*dest[1] - src[2]*dest[2] - src[3]*dest[3];
  i = src[0]*dest[1] + src[1]*dest[0] + src[2]*dest[3] - src[3]*dest[2];
  j = src[0]*dest[2] + src[2]*dest[0] + src[3]*dest[1] - src[1]*dest[3];
  k = src[0]*dest[3] + src[3]*dest[0] + src[1]*dest[2] - src[2]*dest[1];
  dest[0] = r;
  dest[1] = i;
  dest[2] = j;
  dest[3] = k;
}

/* Sets the 'act' flag for pieces which will undergo the rotation. */
static void
flag_pieces(piece_t pieces[27], int axis, int side)
{
  int i, j;
  float q[4];
  for(i = 0; i < 27; i++)
  {
    q[0] = 0;
    q[1] = pieces[i].pos[0];
    q[2] = pieces[i].pos[1];
    q[3] = pieces[i].pos[2];
    mult_quat(pieces[i].qr, q);
    for(j = 1; j < 4; j++)
      q[j] = -q[j];
    mult_quat(pieces[i].qr, q);
    for(j = 1; j < 4; j++)
      q[j] = -q[j];
    if(fabs(q[axis] - side) < 0.1)
      pieces[i].act = True;
    else
      pieces[i].act = False;
  }
}

/* "Rounds" the value to the nearest from the set {0, +-1/2, +-1/sqrt(2), +-1}.
 * It is guaranteed to be pretty close to one when this function is called. */
static float 
settle_value(float v) 
{
  if(v > 0.9) return 1;
  else if(v < -0.9) return -1;
  else if(v > 0.6) return M_SQRT1_2;
  else if(v < -0.6) return -M_SQRT1_2;
  else if(v > 0.4) return 0.5;
  else if(v < -0.4) return -0.5;
  else return 0;
}

static void 
randomize(rubikblocks_conf *cp) 
{
  int axis, side;
  int i, j;
  for(i = 0; i < SHUFFLE; i++)
  {
    axis = (random()%3)+1;
    side = rnd01()*2-1;
    flag_pieces(cp->pieces, axis, side);
    for(j = 1; j < 4; j++)
      cp->qfram[j] = 0;
    cp->qfram[0] = M_SQRT1_2;
    cp->qfram[axis] = M_SQRT1_2;
    for(j = 0; j < 27; j++)
    {
      if(cp->pieces[j].act)
        mult_quat(cp->qfram, cp->pieces[j].qr);
    }
  }
}

static void 
finish(rubikblocks_conf *cp) 
{
  static int axis = 1;
  int side, angle;
  int i, j;
  if(cp->pause)
  {
    switch(axis) 
    {
      case 1:
        axis = rnd01()+2;
        break;
      case 2:
        axis = 2*rnd01()+1;
        break;
      default:
        axis = rnd01()+1;
    }
    side = rnd01()*2-1;
    angle = rnd01()+1;
    flag_pieces(cp->pieces, axis, side);
    cp->pause = False;
    cp->tmax = 90.0*angle;
    for(i = 1; i < 4; i++)
      cp->qfram[i] = 0;
    cp->qfram[0] = cos(tspeed*M_PI/360);
    cp->qfram[axis] = sin((rnd01()*2-1)*tspeed*M_PI/360);
  }
  else
  {
    for(i = 0; i < 27; i++)
    {
      for(j = 0; j < 4; j++)
      {
        cp->pieces[i].qr[j] = settle_value(cp->pieces[i].qr[j]);
      }
    }
    cp->pause = True;
    cp->tmax = twait;
  }
  cp->t = 0;
}

static Bool 
draw_main(ModeInfo *mi, rubikblocks_conf *cp) 
{
  int i;
  double x, y, z;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  get_position(cp->rot, &x, &y, &z, !cp->button_down);
  glTranslatef((x-0.5)*6, (y-0.5)*6, -20);

  gltrackball_rotate(cp->trackball);

  get_rotation(cp->rot, &x, &y, &z, !cp->button_down);
  glRotatef(x*360, 1, 0, 0);
  glRotatef(y*360, 0, 1, 0);
  glRotatef(z*360, 0, 0, 1);
  glScalef(size, size, size);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  if(cp->wire) glColor3f(0.7, 0.7, 0.7);
  if(!cp->pause)
    for(i = 0; i < 27; i++)
      if(cp->pieces[i].act)
        mult_quat(cp->qfram, cp->pieces[i].qr);
  for(i = 0; i < 27; i++) 
  {
    glPushMatrix();
    if(fabs(cp->pieces[i].qr[0]) < 1)
      glRotatef(360/M_PI*acos(cp->pieces[i].qr[0]),
          cp->pieces[i].qr[1], cp->pieces[i].qr[2], cp->pieces[i].qr[3]);
    glCallList(cp->list_base + i);
    glPopMatrix();
  }
  if((cp->t += tspeed) > cp->tmax) finish(cp);
  return True;
}

static void 
draw_horz_line(rubikblocks_conf *cp, int x1, int x2, int y) 
{
  int x, y0 = y, w;
  if(y < BORDER) y = -y;
  else y = -BORDER;
  for(; y < BORDER; y++) {
    if(y0+y >= TEX_HEIGHT) break;
    w = y*y*255/BORDER2;
    for(x = x1; x <= x2; x++)
      if(cp->texture[y0+y][x]>w) cp->texture[y0+y][x] = w;
  }
}

static void 
draw_vert_line(rubikblocks_conf *cp, int x, int y1, int y2) 
{
  int x0 = x, y, w;
  if(x<BORDER) x = -x;
  else x = -BORDER;
  for(; x < BORDER; x++) {
    if(x0+x >= TEX_WIDTH) break;
    w = x*x*255/BORDER2;
    for(y = y1; y <= y2; y++)
      if(cp->texture[y][x0+x]>w) cp->texture[y][x0+x] = w;
  }
}

static void 
make_texture(rubikblocks_conf *cp) 
{
  int x, y;
  for(y = 0; y < TEX_HEIGHT; y++)
    for(x = 0; x < TEX_WIDTH; x++)
      cp->texture[y][x] = 255;
  draw_horz_line(cp, 0, TEX_WIDTH-1, 0);
  draw_horz_line(cp, 0, TEX_WIDTH-1, TEX_HEIGHT-1);
  draw_vert_line(cp, 0, 0, TEX_HEIGHT-1);
  draw_vert_line(cp, TEX_WIDTH-1, 0, TEX_HEIGHT-1);
}

/* These simple transforms make the actual shape of the pieces. The parameters
 * A, B and C affect the excentricity of the pieces in each direction. */
static float 
fx(float x)
{
  const float A = 0.5;
  if(x > 1.4) return 1.5 - A;
  else if(x < -1.4) return -1.5 - A;
  else return x;
}

static float 
fy(float y)
{
  const float B = 0.25;
  if(y > 1.4) return 1.5 - B;
  else if(y < -1.4) return -1.5 - B;
  else return y;
}

static float 
fz(float z)
{
  const float C = 0.0;
  if(z > 1.4) return 1.5 - C;
  else if(z < -1.4) return -1.5 - C;
  else return z;
}

static void 
init_lists(rubikblocks_conf *cp)
{
  GLuint base;
  int i;
  float x, y, z;
  base = cp->list_base = glGenLists(27);
  for(i = 0; i < 27; i++)
  {
    x = cp->pieces[i].pos[0];
    y = cp->pieces[i].pos[1];
    z = cp->pieces[i].pos[2];
    glNewList(base+i, GL_COMPILE);
    glBegin(GL_QUAD_STRIP);
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0);
    glVertex3f(fx(x+0.5), fy(y-0.5), fz(z-0.5));
    glTexCoord2f(0, 1);
    glVertex3f(fx(x+0.5), fy(y+0.5), fz(z-0.5));
    glTexCoord2f(1, 0);
    glVertex3f(fx(x+0.5), fy(y-0.5), fz(z+0.5));
    glTexCoord2f(1, 1);
    glVertex3f(fx(x+0.5), fy(y+0.5), fz(z+0.5));
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0);
    glVertex3f(fx(x-0.5), fy(y-0.5), fz(z+0.5));
    glTexCoord2f(0, 1);
    glVertex3f(fx(x-0.5), fy(y+0.5), fz(z+0.5));
    glNormal3f(-1, 0, 0);
    glTexCoord2f(1, 0);
    glVertex3f(fx(x-0.5), fy(y-0.5), fz(z-0.5));
    glTexCoord2f(1, 1);
    glVertex3f(fx(x-0.5), fy(y+0.5), fz(z-0.5));
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0);
    glVertex3f(fx(x+0.5), fy(y-0.5), fz(z-0.5));
    glTexCoord2f(0, 1);
    glVertex3f(fx(x+0.5), fy(y+0.5), fz(z-0.5));
    glEnd();
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);
    glVertex3f(fx(x+0.5), fy(y+0.5), fz(z+0.5));
    glTexCoord2f(0, 1);
    glVertex3f(fx(x+0.5), fy(y+0.5), fz(z-0.5));
    glTexCoord2f(1, 1);
    glVertex3f(fx(x-0.5), fy(y+0.5), fz(z-0.5));
    glTexCoord2f(1, 0);
    glVertex3f(fx(x-0.5), fy(y+0.5), fz(z+0.5));
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0);
    glVertex3f(fx(x+0.5), fy(y-0.5), fz(z-0.5));
    glTexCoord2f(0, 1);
    glVertex3f(fx(x+0.5), fy(y-0.5), fz(z+0.5));
    glTexCoord2f(1, 1);
    glVertex3f(fx(x-0.5), fy(y-0.5), fz(z+0.5));
    glTexCoord2f(1, 0);
    glVertex3f(fx(x-0.5), fy(y-0.5), fz(z-0.5));
    glEnd();
    glEndList();
  }
}

/* It looks terrible... FIXME: any other ideas, maybe some anisotropic filtering? */
/*#define MIPMAP*/

static void 
init_gl(ModeInfo *mi) 
{
  rubikblocks_conf *cp = &rubikblocks[MI_SCREEN(mi)];
#ifdef MIPMAP
  int status;
#endif
  cp->wire = MI_IS_WIREFRAME(mi);

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  cp->wire = 0;
# endif

  if(MI_IS_MONO(mi))
    tex = False;
  if(cp->wire) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    return;
  }

  glClearDepth(1.0);
  glDrawBuffer(GL_BACK);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glShadeModel(GL_FLAT);
  glDepthFunc(GL_LESS);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT1, GL_POSITION, position1);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  glEnable(GL_LIGHTING);
  glEnable(GL_NORMALIZE);
  glEnable(GL_COLOR_MATERIAL);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_specular);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
  if (!tex) return;
  glEnable(GL_TEXTURE_2D);
#ifdef MIPMAP
  clear_gl_error();
  status = gluBuild2DMipmaps(GL_TEXTURE_2D, 1, TEX_WIDTH, TEX_HEIGHT,
      GL_LUMINANCE, GL_UNSIGNED_BYTE, cp->texture);
  if (status) {
    const char *s = (char *)gluErrorString(status);
    fprintf (stderr, "%s: error mipmapping texture: %s\n", progname, (s?s:"(unknown)"));
    exit (1);
  }
  check_gl_error("mipmapping");
#else    
  glTexImage2D(GL_TEXTURE_2D, 0, 1, TEX_WIDTH, TEX_HEIGHT,
      0, GL_LUMINANCE, GL_UNSIGNED_BYTE, cp->texture);
#endif  
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef MIPMAP
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
}

static void 
init_cp(rubikblocks_conf *cp) 
{
  int i, j, k, m;

  cp->pause = True;
  cp->t = 0.0;
  cp->tmax = twait;

  for(i = -1, m = 0; i <= 1; i++)
    for(j = -1; j <= 1; j++)
      for(k = -1; k <= 1; k++)
      {
        cp->pieces[m].pos[0] = k;
        cp->pieces[m].pos[1] = j;
        cp->pieces[m].pos[2] = i;
        cp->pieces[m].qr[0] = 1;
        cp->pieces[m].qr[1] = 0;
        cp->pieces[m].qr[2] = 0;
        cp->pieces[m].qr[3] = 0;
        m++;
      }

  cp->rot = make_rotator(spin?spinspeed:0, spin?spinspeed:0, spin?spinspeed:0,
      0.1, wander?wspeed:0, True);
  cp->trackball = gltrackball_init(True);

  if(rndstart) randomize(cp);
}

/*************************************************************************/

ENTRYPOINT void 
reshape_rubikblocks(ModeInfo *mi, int width, int height) 
{
  rubikblocks_conf *cp = &rubikblocks[MI_SCREEN(mi)];
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
  }
  if(!height) height = 1;
  cp->ratio = (GLfloat)width/(GLfloat)height;
  glViewport(0, y, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0, cp->ratio, 1.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT void 
init_rubikblocks(ModeInfo *mi) 
{
  rubikblocks_conf *cp;
  MI_INIT(mi, rubikblocks);
  cp = &rubikblocks[MI_SCREEN(mi)];

  if(tex)
    make_texture(cp);

  if ((cp->glx_context = init_GL(mi)) != NULL) 
  {
    init_gl(mi);
    init_cp(cp);
    init_lists(cp);
    reshape_rubikblocks(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }
  else 
  {
    MI_CLEARWINDOW(mi);
  }
}

ENTRYPOINT void 
draw_rubikblocks(ModeInfo * mi) 
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  rubikblocks_conf *cp;
  if (!rubikblocks) return;
  cp = &rubikblocks[MI_SCREEN(mi)];
  MI_IS_DRAWN(mi) = True;
  if (!cp->glx_context) return;
  mi->polygon_count = 0;
  glXMakeCurrent(display, window, *(cp->glx_context));
  if (!draw_main(mi, cp)) 
  {
    MI_ABORT(mi);
    return;
  }
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  glXSwapBuffers(display, window);
}

#ifndef STANDALONE
ENTRYPOINT void 
change_rubikblocks(ModeInfo * mi) 
{
  rubikblocks_conf *cp = &rubikblocks[MI_SCREEN(mi)];
  if (!cp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cp->glx_context));
  init_gl(mi);
}
#endif /* !STANDALONE */

ENTRYPOINT Bool
rubikblocks_handle_event (ModeInfo *mi, XEvent *event)
{
  rubikblocks_conf *cp = &rubikblocks[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, cp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &cp->button_down))
    return True;

  return False;
}


XSCREENSAVER_MODULE ("RubikBlocks", rubikblocks)

#endif
