/*-
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Cube 21 - a Rubik-like puzzle.  It changes its shape and has more than
 * 200 configurations.  It is known better as Square-1, but it is called
 * Cube 21 in the Czech republic, where it was invented in 1992.
 * 
 * This file is derived from cage.c,
 * "cage --- the Impossible Cage, an Escher like scene",
 * by Marcelo F. Vienna,
 * parts from gltext.c by Jamie Zawinski
 *
 * Vaclav (Vasek) Potocek
 * vasek.potocek@post.cz
 */

/* TODO:
 *  some simple "solve mode"
 *  use rotator
 */

/*-
 * Texture mapping is only available on RGBA contexts, Mono and color index
 * visuals DO NOT support texture mapping in OpenGL.
 *
 * BUT Mesa do implements RGBA contexts in pseudo color visuals, so texture
 * mapping should work on PseudoColor, DirectColor, TrueColor using Mesa. Mono
 * is not officially supported for both OpenGL and Mesa, but seems to not crash
 * Mesa.
 *
 * In real OpenGL, PseudoColor DO NOT support texture map (as far as I know).
 */

#define DEFAULTS   "*delay:         20000         \n" \
                   "*showFPS:       False         \n" \
                   "*wireframe:     False         \n"

# define refresh_cube21 0
#include "xlockmore.h"

#include "gltrackball.h"

#ifdef USE_GL

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_TEXTURE     "True"
#define DEF_RANDOMIZE   "True"
#define DEF_SPINSPEED   "1.0"
#define DEF_ROTSPEED    "3.0"
#define DEF_WANDERSPEED "0.02"
#define DEF_WAIT        "40.0"
#define DEF_CUBESIZE    "0.7"
#define DEF_COLORMODE   "six"

#ifdef Pi
#undef Pi
#endif
#define Pi      M_PI

#define SHUFFLE 100

#define COS15   0.9659258263
#define SIN15   0.2588190451
#define COS30   0.8660254038
#define SIN30   0.5000000000

#define TEX_WIDTH  128
#define TEX_HEIGHT 128
#define TEX_GRAY   0.7, 0.7
#define BORDER     3
#define BORDER2    9

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define rnd01() (random()%2)
#define rndcolor() (frand(0.5)+0.3)

/*************************************************************************/

static Bool spin, wander, rndstart, tex;
static float spinspeed, tspeed, wspeed, twait, size;
static char *colmode_s;
static int colmode;

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
  { &colmode_s, "colormode",   "ColorMode",   DEF_COLORMODE,   t_String}
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
  { "-colormode",   ".colormode",   XrmoptionSepArg, 0 }
};

ENTRYPOINT ModeSpecOpt cube21_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   cube21_description =
{ "cube21", "init_cube21", "draw_cube21", "release_cube21",
  "draw_cube21", "change_cube21", NULL, &cube21_opts,
  25000, 1, 1, 1, 1.0, 4, "",
  "Shows randomly shuffling Cube 21", 0, NULL
};
#endif

typedef enum {
  CUBE21_STATE_BASIC,
  CUBE21_PAUSE1 = CUBE21_STATE_BASIC,
  CUBE21_ROT_BASE,
  CUBE21_ROT_TOP = CUBE21_ROT_BASE,
  CUBE21_ROT_BOTTOM,
  CUBE21_PAUSE2,
  CUBE21_HALF_BASE,
  CUBE21_HALF1 = CUBE21_HALF_BASE,
  CUBE21_HALF2
} cube21_state;

typedef enum {
  CUBE21_COLOR_WHITE,
  CUBE21_COLOR_RANDOM,
  CUBE21_COLOR_SILVER,
  CUBE21_COLOR_TWORND,
  CUBE21_COLOR_CLASSIC,
  CUBE21_COLOR_SIXRND
} cube21_cmode;

typedef int pieces_t[2][13];
typedef int cind_t[5][12];
typedef GLfloat col_t[6][3];

typedef struct {
  GLXContext    *glx_context;
  GLfloat       ratio;
  cube21_state  state;          /* type of "rotation" - shuffling */
  GLfloat       xrot, yrot;     /* "spin" - object rotation around axis */
  GLfloat       posarg;         /* position argument (for sine function) */
  GLfloat       t, tmax;        /* rotation clock */
  int           hf[2], fr[2];   /* half flipped / face rotated flags */
  int           rface, ramount; /* selected face and amount of rotation in multiplies of 30deg */
  int           pieces[2][13];  /* locations of "narrow" and "wide" pieces */
  int           cind[5][12];    /* color indices */
  GLfloat       colors[6][3];   /* color map */

  Bool wire, cmat;
  unsigned char texture[TEX_HEIGHT][TEX_WIDTH];

  GLfloat texp, texq, posc[6];
  GLfloat color_inner[4];

  Bool button_down_p;
  trackball_state *trackball;

} cube21_conf;

static cube21_conf *cube21 = NULL;

static const GLfloat shininess = 20.0;
static const GLfloat ambient[] = {0.0, 0.0, 0.0, 1.0};
static const GLfloat diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const GLfloat position0[] = {1.0, 1.0, 1.0, 0.0};
static const GLfloat position1[] = {-1.0, -1.0, 1.0, 0.0};
static const GLfloat lmodel_ambient[] = {0.1, 0.1, 0.1, 1.0};
static const GLfloat material_ambient[] = {0.7, 0.7, 0.7, 1.0};
static const GLfloat material_diffuse[] = {0.7, 0.7, 0.7, 1.0};
static const GLfloat material_specular[] = {0.2, 0.2, 0.2, 1.0};
static const GLfloat zpos = -18.0;

/*************************************************************************/

static void find_matches(pieces_t pieces, int matches[12], int s) 
{
  int i, j = 1;
  for(i = 1; i<6; i++) {
    if(pieces[s][i] && pieces[s][i+6]) {
      matches[j++] = i;
    }
  }
  matches[0] = j;
  for(i = 1; i<matches[0]; i++) {
    matches[j++] = matches[i]-6;
  }
  matches[j++] = 6;
  matches[0] = j;
}

static void rot_face(pieces_t pieces, cind_t colors, int s, int o) 
{
  int i;
  int tmp[12], tmpc[2][12];
  int c0 = 2*s, c1 = c0+1;
  for(i = 0; i<12; i++) {
    tmp[i] = pieces[s][i];
    tmpc[0][i] = colors[c0][i];
    tmpc[1][i] = colors[c1][i];
  }
  if(o<0) o += 12;
  for(i = 0; i<12; i++, o++) {
    if(o==12) o = 0;
    pieces[s][i] = tmp[o];
    colors[c0][i] = tmpc[0][o];
    colors[c1][i] = tmpc[1][o];
  }
}

static void rot_halves(pieces_t pieces, cind_t colors, int hf[2], int s) 
{
  int ss = 6*s, i, j, k, t;
  for(i = 0; i<6; i++) {
    j = ss+i; k = ss+6-i;
    t = pieces[0][j];
    pieces[0][j] = pieces[1][k];
    pieces[1][k] = t;
    k--;
    t = colors[0][j];
    colors[0][j] = colors[2][k];
    colors[2][k] = t;
    t = colors[1][j];
    colors[1][j] = colors[3][k];
    colors[3][k] = t;
  }
  hf[s] ^= 1;
}

static void randomize(cube21_conf *cp) 
{
  int i, j, s;
  int matches[12];
  for(i = 0; i<SHUFFLE; i++) {
    s = rnd01();
    find_matches(cp->pieces, matches, s);
    j = matches[0]-1;
    j = random()%j;
    j = matches[j+1];
    rot_face(cp->pieces, cp->cind, s, j);
    s = rnd01();
    rot_halves(cp->pieces, cp->cind, cp->hf, s);
  }
}

static void finish(cube21_conf *cp) 
{
  int j, s;
  int matches[12];
  switch(cp->state) {
    case CUBE21_PAUSE1:
      s = rnd01();
      find_matches(cp->pieces, matches, s);
      j = matches[0]-1;
      j = random()%j;
      j = matches[j+1];
      if(j==6 && rnd01()) j = -6;
      cp->state = CUBE21_ROT_BASE+s;
      cp->tmax = 30.0*abs(j);
      cp->fr[0] = cp->fr[1] = 0;
      cp->rface = s;
      cp->ramount = j;
      break;
    case CUBE21_ROT_TOP:
    case CUBE21_ROT_BOTTOM:
      rot_face(cp->pieces, cp->cind, s = cp->rface, cp->ramount);
      cp->fr[s] = 1;
      s ^= 1;
      if(!cp->fr[s] && rnd01()) {
        find_matches(cp->pieces, matches, s);
        j = matches[0]-1;
        j = random()%j;
        j = matches[j+1];
        if(j==6 && rnd01()) j = -6;
        cp->state = CUBE21_ROT_BASE+s;
        cp->tmax = 30.0*abs(j);
        cp->rface = s;
        cp->ramount = j;        
        break;
      } else {
        cp->state = CUBE21_PAUSE2;
        cp->tmax = twait;
        break;
      }
    case CUBE21_PAUSE2:
      s = rnd01();
      cp->ramount = -rnd01();       /* 0 or -1, only sign is significant in this case */
      cp->state = CUBE21_HALF_BASE+s;
      cp->tmax = 180.0;
      cp->rface = s;
      break;
    case CUBE21_HALF1:
    case CUBE21_HALF2:
      rot_halves(cp->pieces, cp->cind, cp->hf, cp->rface);
      cp->state = CUBE21_PAUSE1;
      cp->tmax = twait;
      break;
  }
  cp->t = 0;
}

static void draw_narrow_piece(ModeInfo *mi, cube21_conf *cp, GLfloat s, int c1, int c2, col_t colors) 
{
  GLfloat s1 = cp->posc[0]*s;
  glBegin(GL_TRIANGLES);
  glNormal3f(0.0, 0.0, s);
  if(cp->cmat) glColor3fv(colors[c1]);
  glTexCoord2f(0.5, 0.5);  glVertex3f(0.0, 0.0, s);
  glTexCoord2f(cp->texq, 0.0); glVertex3f(cp->posc[1], 0.0, s);
  glTexCoord2f(cp->texp, 0.0); glVertex3f(cp->posc[2], cp->posc[3], s);
  mi->polygon_count++;
  glNormal3f(0.0, 0.0, -s);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(0.0, 0.0, s1);
  glVertex3f(cp->posc[1], 0.0, s1);
  glVertex3f(cp->posc[2], cp->posc[3], s1);
  mi->polygon_count++;
  glEnd();
  glBegin(GL_QUADS);
  glNormal3f(0.0, -1.0, 0.0);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(0.0, 0.0, s);
  glVertex3f(cp->posc[1], 0.0, s);
  glVertex3f(cp->posc[1], 0.0, s1);
  glVertex3f(0.0, 0.0, s1);
  mi->polygon_count++;
  glNormal3f(COS15, SIN15, 0.0);
  if(cp->cmat) glColor3fv(colors[c2]);
  glTexCoord2f(cp->texq, cp->texq); glVertex3f(cp->posc[1], 0.0, s);
  glTexCoord2f(cp->texq, cp->texp); glVertex3f(cp->posc[2], cp->posc[3], s);
  glTexCoord2f(1.0, cp->texp);  glVertex3f(cp->posc[2], cp->posc[3], s1);
  glTexCoord2f(1.0, cp->texq);  glVertex3f(cp->posc[1], 0.0, s1);
  mi->polygon_count++;
  glNormal3f(-SIN30, COS30, 0.0);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(0.0, 0.0, s);
  glVertex3f(cp->posc[2], cp->posc[3], s);
  glVertex3f(cp->posc[2], cp->posc[3], s1);
  glVertex3f(0.0, 0.0, s1);
  mi->polygon_count++;
  glEnd();
  glRotatef(30.0, 0.0, 0.0, 1.0);
}

static void draw_wide_piece(ModeInfo *mi, cube21_conf *cp, GLfloat s, int c1, int c2, int c3, col_t colors) 
{
  GLfloat s1 = cp->posc[0]*s;
  glBegin(GL_TRIANGLES);
  glNormal3f(0.0, 0.0, s);
  if(cp->cmat) glColor3fv(colors[c1]);
  glTexCoord2f(0.5, 0.5);  glVertex3f(0.0, 0.0, s);
  glTexCoord2f(cp->texp, 0.0); glVertex3f(cp->posc[1], 0.0, s);
  glTexCoord2f(0.0, 0.0);  glVertex3f(cp->posc[4], cp->posc[5], s);
  glTexCoord2f(0.0, 0.0);  glVertex3f(cp->posc[4], cp->posc[5], s);
  glTexCoord2f(0.0, cp->texp); glVertex3f(cp->posc[3], cp->posc[2], s);
  glTexCoord2f(0.5, 0.5);  glVertex3f(0.0, 0.0, s);
  glNormal3f(0.0, 0.0, -s);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(0.0, 0.0, s1);
  glVertex3f(cp->posc[1], 0.0, s1);
  glVertex3f(cp->posc[4], cp->posc[5], s1);
  glVertex3f(cp->posc[4], cp->posc[5], s1);
  glVertex3f(cp->posc[3], cp->posc[2], s1);
  glVertex3f(0.0, 0.0, s1);
  glEnd();
  glBegin(GL_QUADS);
  glNormal3f(0.0, -1.0, 0);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(0.0, 0.0, s);
  glVertex3f(cp->posc[1], 0.0, s);
  glVertex3f(cp->posc[1], 0.0, s1);
  glVertex3f(0.0, 0.0, s1);
  glNormal3f(COS15, -SIN15, 0.0);
  if(cp->cmat) glColor3fv(colors[c2]);
  glTexCoord2f(cp->texq, cp->texp); glVertex3f(cp->posc[1], 0.0, s);
  glTexCoord2f(cp->texq, 0.0);  glVertex3f(cp->posc[4], cp->posc[5], s);
  glTexCoord2f(1.0, 0.0);   glVertex3f(cp->posc[4], cp->posc[5], s1);
  glTexCoord2f(1.0, cp->texp);  glVertex3f(cp->posc[1], 0.0, s1);
  glNormal3f(SIN15, COS15, 0.0);
  if(cp->cmat) glColor3fv(colors[c3]);
  glTexCoord2f(cp->texq, cp->texp); glVertex3f(cp->posc[4], cp->posc[5], s);
  glTexCoord2f(cp->texq, 0.0);  glVertex3f(cp->posc[3], cp->posc[2], s);
  glTexCoord2f(1.0, 0.0);   glVertex3f(cp->posc[3], cp->posc[2], s1);
  glTexCoord2f(1.0, cp->texp);  glVertex3f(cp->posc[4], cp->posc[5], s1);
  glNormal3f(-COS30, SIN30, 0.0);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(0.0, 0.0, s);
  glVertex3f(cp->posc[3], cp->posc[2], s);
  glVertex3f(cp->posc[3], cp->posc[2], s1);
  glVertex3f(0.0, 0.0, s1);
  glEnd();
  glRotatef(60.0, 0.0, 0.0, 1.0);
}

static void draw_middle_piece(cube21_conf *cp, int s, cind_t cind, col_t colors) 
{
  s *= 6;
  glBegin(GL_QUADS);
  if(cp->cmat) glColor3fv(cp->color_inner);
  glNormal3f(0.0, 0.0, 1.0);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(cp->posc[1], 0.0, cp->posc[0]);
  glVertex3f(cp->posc[4], cp->posc[5], cp->posc[0]);
  glVertex3f(-cp->posc[5], cp->posc[4], cp->posc[0]);
  glVertex3f(-cp->posc[1], 0.0, cp->posc[0]);
  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(cp->posc[1], 0.0, -cp->posc[0]);
  glVertex3f(cp->posc[4], cp->posc[5], -cp->posc[0]);
  glVertex3f(-cp->posc[5], cp->posc[4], -cp->posc[0]);
  glVertex3f(-cp->posc[1], 0.0, -cp->posc[0]);
  glNormal3f(0.0, -1.0, 0.0);
  glTexCoord2f(TEX_GRAY);
  glVertex3f(-cp->posc[1], 0.0, cp->posc[0]);
  glVertex3f(cp->posc[1], 0.0, cp->posc[0]);
  glVertex3f(cp->posc[1], 0.0, -cp->posc[0]);
  glVertex3f(-cp->posc[1], 0.0, -cp->posc[0]);
  glNormal3f(COS15, -SIN15, 0.0);
  if(cp->cmat) glColor3fv(colors[cind[4][s]]);
  glTexCoord2f(cp->texq, cp->texp); glVertex3f(cp->posc[1], 0.0, cp->posc[0]);
  glTexCoord2f(1.0, cp->texp);  glVertex3f(cp->posc[4], cp->posc[5], cp->posc[0]);
  glTexCoord2f(1.0, cp->texq);  glVertex3f(cp->posc[4], cp->posc[5], -cp->posc[0]);
  glTexCoord2f(cp->texq, cp->texq); glVertex3f(cp->posc[1], 0.0, -cp->posc[0]);
  glNormal3f(SIN15, COS15, 0.0);
  if(cp->cmat) glColor3fv(colors[cind[4][s+1]]);
  glTexCoord2f(0.0, 0.5);   glVertex3f(cp->posc[4], cp->posc[5], cp->posc[0]);
  glTexCoord2f(cp->texq, 0.5); glVertex3f(-cp->posc[5], cp->posc[4], cp->posc[0]);
  glTexCoord2f(cp->texq, 0.75); glVertex3f(-cp->posc[5], cp->posc[4], -cp->posc[0]);
  glTexCoord2f(0.0, 0.75);   glVertex3f(cp->posc[4], cp->posc[5], -cp->posc[0]);
  glNormal3f(-COS15, SIN15, 0.0);
  if(cp->cmat) glColor3fv(colors[cind[4][s+4]]);
  glTexCoord2f(0.0, 0.75); glVertex3f(-cp->posc[5], cp->posc[4], cp->posc[0]);
  glTexCoord2f(1.0, 0.75); glVertex3f(-cp->posc[1], 0.0, cp->posc[0]);
  glTexCoord2f(1.0, 1.0);  glVertex3f(-cp->posc[1], 0.0, -cp->posc[0]);
  glTexCoord2f(0.0, 1.0);  glVertex3f(-cp->posc[5], cp->posc[4], -cp->posc[0]);
  glEnd();
}

static void draw_middle(cube21_conf *cp) 
{
  if(cp->hf[0]) glRotatef(180.0, 0.0, 1.0, 0.0);
  draw_middle_piece(cp, 0, cp->cind, cp->colors);
  if(cp->hf[0]) glRotatef(180.0, 0.0, 1.0, 0.0);
  glRotatef(180.0, 0.0, 0.0, 1.0);
  if(cp->hf[1]) glRotatef(180.0, 0.0, 1.0, 0.0);
  draw_middle_piece(cp, 1, cp->cind, cp->colors);
  if(cp->hf[1]) glRotatef(180.0, 0.0, 1.0, 0.0);
}

static void draw_half_face(ModeInfo *mi, cube21_conf *cp, int s, int o) 
{
  int i, s1 = 1-s*2, s2 = s*2;
  for(i = o; i<o+6; i++) {
    if(cp->pieces[s][i+1])
      draw_narrow_piece(mi, cp, s1, cp->cind[s2][i], cp->cind[s2+1][i], cp->colors);
    else {
      draw_wide_piece(mi, cp, s1, cp->cind[s2][i], cp->cind[s2+1][i], cp->cind[s2+1][i+1], cp->colors);
      i++;
    }
  }
}

static void draw_top_face(ModeInfo *mi, cube21_conf *cp) 
{
  draw_half_face(mi, cp, 0, 0);
  draw_half_face(mi, cp, 0, 6);
}

static void draw_bottom_face(ModeInfo *mi, cube21_conf *cp) 
{
  draw_half_face(mi, cp, 1, 0);
  draw_half_face(mi, cp, 1, 6);
}

static Bool draw_main(ModeInfo *mi, cube21_conf *cp) 
{
  GLfloat theta = cp->ramount<0?cp->t:-cp->t;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  if(wander)
    glTranslatef(3.0*cp->ratio*sin(13.0*cp->posarg), 3.0*sin(17.0*cp->posarg), zpos);
  else
    glTranslatef(0, 0, zpos);
  glScalef(size, size, size);

  /* Do it twice because we don't track the device's orientation. */
  glRotatef( current_device_rotation(), 0, 0, 1);
  gltrackball_rotate (cp->trackball);
  glRotatef(-current_device_rotation(), 0, 0, 1);

  glRotatef(cp->xrot, 1.0, 0.0, 0.0);
  glRotatef(cp->yrot, 0.0, 1.0, 0.0);
  if(cp->wire) glColor3f(0.7, 0.7, 0.7);
  switch(cp->state) {
    case CUBE21_PAUSE1:
    case CUBE21_PAUSE2:
      draw_top_face(mi, cp);
      draw_bottom_face(mi, cp);
      draw_middle(cp);
      break;
    case CUBE21_ROT_TOP:
      glRotatef(theta, 0.0, 0.0, 1.0);
      draw_top_face(mi, cp);
      glRotatef(-theta, 0.0, 0.0, 1.0);
      draw_bottom_face(mi, cp);
      draw_middle(cp);
      break;
    case CUBE21_ROT_BOTTOM:
      draw_top_face(mi, cp);
      glRotatef(theta, 0.0, 0.0, 1.0);
      draw_bottom_face(mi, cp);
      glRotatef(-theta, 0.0, 0.0, 1.0);
      draw_middle(cp);
      break;
    case CUBE21_HALF1:
      glRotatef(theta, 0.0, 1.0, 0.0);
    case CUBE21_HALF2:
      draw_half_face(mi, cp, 0, 0);
      glRotatef(-180.0, 0.0, 0.0, 1.0);
      draw_half_face(mi, cp, 1, 0);
      glRotatef(-180.0, 0.0, 0.0, 1.0);
      if(cp->hf[0]) glRotatef(180.0, 0.0, 1.0, 0.0);
      draw_middle_piece(cp, 0, cp->cind, cp->colors);
      if(cp->hf[0]) glRotatef(180.0, 0.0, 1.0, 0.0);
      if(cp->state==CUBE21_HALF1)
        glRotatef(-theta, 0.0, 1.0, 0.0);
      else
        glRotatef(theta, 0.0, 1.0, 0.0);
      glRotatef(180.0, 0.0, 0.0, 1.0);
      draw_half_face(mi, cp, 0, 6);
      glRotatef(-180.0, 0.0, 0.0, 1.0);
      draw_half_face(mi, cp, 1, 6);
      glRotatef(-180.0, 0.0, 0.0, 1.0);
      if(cp->hf[1]) glRotatef(180.0, 0.0, 1.0, 0.0);
      draw_middle_piece(cp, 1, cp->cind, cp->colors);
      break;
  }
  if(spin) {
    if((cp->xrot += spinspeed)>360.0) cp->xrot -= 360.0;
    if((cp->yrot += spinspeed)>360.0) cp->yrot -= 360.0;
  }
  if(wander)
    if((cp->posarg += wspeed/1000.0)>360.0) cp->posarg -= 360.0;
  if((cp->t += tspeed)>cp->tmax) finish(cp);
  return True;
}

static void parse_colmode(void) 
{
  if(!colmode_s) {
    colmode = CUBE21_COLOR_WHITE;
    return;
  }
  if(strstr(colmode_s, "se") || strstr(colmode_s, "sil")) colmode = CUBE21_COLOR_SILVER;
  else if(strstr(colmode_s, "ce") || strstr(colmode_s, "cla")) colmode = CUBE21_COLOR_CLASSIC;
  else if(strstr(colmode_s, "2") || strstr(colmode_s, "two")) colmode = CUBE21_COLOR_TWORND;
  else if(strstr(colmode_s, "6") || strstr(colmode_s, "six")) colmode = CUBE21_COLOR_SIXRND;
  else if(strstr(colmode_s, "1") || strstr(colmode_s, "ran") || strstr(colmode_s, "rnd")) colmode = CUBE21_COLOR_RANDOM;
  else colmode = CUBE21_COLOR_WHITE;
}

static void init_posc(cube21_conf *cp) 
{
  cp->texp = (1.0-tan(Pi/12.0))/2.0;
  cp->texq = 1.0-cp->texp;
  /* Some significant non-trivial coordinates
   * of the object. We need them exactly at GLfloat precision
   * for the edges to line up perfectly. */
  cp->posc[0] = tan(Pi/12);            /* 0.268 */
  cp->posc[1] = 1.0/cos(Pi/12);        /* 1.035 */
  cp->posc[2] = cos(Pi/6)/cos(Pi/12);  /* 0.897 */
  cp->posc[3] = sin(Pi/6)/cos(Pi/12);  /* 0.518 */
  cp->posc[4] = sqrt(2)*cos(Pi/6);     /* 1.225 */
  cp->posc[5] = sqrt(2)*sin(Pi/6);     /* 0.707 = 1/sqrt(2) */
}

static void draw_horz_line(cube21_conf *cp, int x1, int x2, int y) 
{
  int x, y0 = y, w;
  if(y<BORDER) y = -y;
  else y = -BORDER;
  for(; y<BORDER; y++) {
    if(y0+y>=TEX_HEIGHT) break;
    w = y*y*255/BORDER2;
    for(x=x1; x<=x2; x++)
      if(cp->texture[y0+y][x]>w) cp->texture[y0+y][x] = w;
  }
}

static void draw_vert_line(cube21_conf *cp, int x, int y1, int y2) 
{
  int x0 = x, y, w;
  if(x<BORDER) x = -x;
  else x = -BORDER;
  for(; x<BORDER; x++) {
    if(x0+x>=TEX_WIDTH) break;
    w = x*x*255/BORDER2;
    for(y=y1; y<=y2; y++)
      if(cp->texture[y][x0+x]>w) cp->texture[y][x0+x] = w;
  }
}

static void draw_slanted_horz(cube21_conf *cp, int x1, int y1, int x2, int y2) 
{
  int x, y, dx = x2-x1, dy = y2-y1, y0, w;
  for(x=x1; x<=x2; x++) {
    y0 = y1+(y2-y1)*(x-x1)/(x2-x1);
    for(y=-1-BORDER; y<2+BORDER; y++) {
      w = dx*(y0+y-y1)-dy*(x-x1);
      w = w*w/(dx*dx+dy*dy);
      w = w*255/BORDER2;
      if(cp->texture[y0+y][x]>w) cp->texture[y0+y][x] = w;
    }
  }
}

static void draw_slanted_vert(cube21_conf *cp, int x1, int y1, int x2, int y2) 
{
  int x, y, dx = x2-x1, dy = y2-y1, x0, w;
  for(y=y1; y<=y2; y++) {
    x0 = x1+(x2-x1)*(y-y1)/(y2-y1);
    for(x=-1-BORDER; x<2+BORDER; x++) {
      w = dy*(x0+x-x1)-dx*(y-y1);
      w = w*w/(dy*dy+dx*dx);
      w = w*255/BORDER2;
      if(cp->texture[y][x0+x]>w) cp->texture[y][x0+x] = w;
    }
  }
}

static void make_texture(cube21_conf *cp) 
{
  int x, y, x0, y0;
  float grayp[2] = {TEX_GRAY};
  for(y=0; y<TEX_HEIGHT; y++)
    for(x=0; x<TEX_WIDTH; x++)
      cp->texture[y][x] = 255;
  draw_horz_line(cp, 0, TEX_WIDTH-1, 0);
  draw_horz_line(cp, cp->texq*TEX_WIDTH, TEX_WIDTH-1, cp->texp*TEX_HEIGHT);
  draw_horz_line(cp, cp->texq*TEX_WIDTH, TEX_WIDTH-1, cp->texq*TEX_HEIGHT);
  draw_horz_line(cp, 0, cp->texq*TEX_WIDTH, TEX_HEIGHT/2);
  draw_horz_line(cp, 0, TEX_WIDTH-1, TEX_HEIGHT*3/4);
  draw_horz_line(cp, 0, TEX_WIDTH-1, TEX_HEIGHT-1);
  draw_vert_line(cp, 0, 0, TEX_HEIGHT-1);
  draw_vert_line(cp, cp->texq*TEX_WIDTH, 0, TEX_HEIGHT*3/4);
  draw_vert_line(cp, TEX_WIDTH-1, 0, TEX_HEIGHT-1);
  draw_slanted_horz(cp, 0, cp->texp*TEX_HEIGHT, TEX_WIDTH/2, TEX_HEIGHT/2);
  draw_slanted_vert(cp, cp->texp*TEX_WIDTH, 0, TEX_WIDTH/2, TEX_HEIGHT/2);
  draw_slanted_vert(cp, cp->texq*TEX_WIDTH, 0, TEX_WIDTH/2, TEX_HEIGHT/2);
  x0 = grayp[0]*TEX_WIDTH;
  y0 = grayp[1]*TEX_HEIGHT;
  for(y=-1; y<=1; y++)
    for(x=-1; x<=1; x++)
      cp->texture[y0+y][x0+x] = 100;   
}

/* It doesn't look good */
/*#define MIPMAP*/

static void init_gl(ModeInfo *mi) 
{
  cube21_conf *cp = &cube21[MI_SCREEN(mi)];
#ifdef MIPMAP
  int status;
#endif
  parse_colmode();
  cp->wire = MI_IS_WIREFRAME(mi);
  cp->cmat = !cp->wire && (colmode != CUBE21_COLOR_WHITE);
  if(MI_IS_MONO(mi)) {
    tex = False;
    cp->cmat = False;
  }
  if(cp->wire) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    return;
  }
  if(!tex)
    cp->color_inner[0] = cp->color_inner[1] = cp->color_inner[2] = 0.4;
  else
    cp->color_inner[0] = cp->color_inner[1] = cp->color_inner[2] = 1.0;

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
  if(!tex) return;
  glEnable(GL_TEXTURE_2D);
#ifdef MIPMAP
  clear_gl_error();
  status = gluBuild2DMipmaps(GL_TEXTURE_2D, 1, TEX_WIDTH, TEX_HEIGHT,
      GL_LUMINANCE, GL_UNSIGNED_BYTE, texture);
  if (status) {
    const char *s = gluErrorString(status);
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

static void init_cp(cube21_conf *cp) 
{
  int i, j;
  GLfloat ce_colors[6][3] = {
    {1.0, 1.0, 1.0},
    {1.0, 0.5, 0.0},
    {0.0, 0.9, 0.0},
    {0.8, 0.0, 0.0},
    {0.1, 0.1, 1.0},
    {0.9, 0.9, 0.0}
  };
  cp->state = CUBE21_STATE_BASIC;
  cp->xrot = -65.0; cp->yrot = 185.0;
  cp->posarg = (wspeed?random()%360:0.0);
  cp->t = 0.0; cp->tmax = twait;
  cp->hf[0] = cp->hf[1] = 0;
  cp->fr[0] = cp->fr[1] = 0;
  for(i=0;i<13;i++)
    cp->pieces[0][i] = cp->pieces[1][i] = (i%3==1?0:1);
  switch(colmode) {
    case CUBE21_COLOR_RANDOM:
    case CUBE21_COLOR_TWORND:
    case CUBE21_COLOR_SIXRND:
      for(i=0; i<6; i++)
        for(j=0; j<3; j++)
          cp->colors[i][j] = rndcolor();
      break;
    case CUBE21_COLOR_SILVER:
      cp->colors[0][0] = 1.0;
      cp->colors[0][1] = 1.0;
      cp->colors[0][2] = 1.0;
      cp->colors[1][0] = rndcolor();
      cp->colors[1][1] = rndcolor();
      cp->colors[1][2] = rndcolor();
      break;
    case CUBE21_COLOR_CLASSIC:
      for(i=0; i<6; i++)
        for(j=0; j<3; j++)
          cp->colors[i][j] = 0.2+0.7*ce_colors[i][j];
      break;
  }
  switch(colmode) {
    case CUBE21_COLOR_SILVER:
    case CUBE21_COLOR_TWORND:
      for(i=0; i<5; i++)
        for(j=0; j<12; j++)
          if(i==0) cp->cind[i][j] = 0;
          else if(i==2) cp->cind[i][j] = 1;
          else cp->cind[i][j] = ((j+5)%12)>=6?1:0;
      break;
    case CUBE21_COLOR_CLASSIC:
    case CUBE21_COLOR_SIXRND:
      for(i=0; i<5; i++)
        for(j=0; j<12; j++)
          if(i==0) cp->cind[i][j] = 4;
          else if(i==2) cp->cind[i][j] = 5;
          else cp->cind[i][j] = ((j+5)%12)/3;
      break;
    case CUBE21_COLOR_RANDOM:
      for(i=0; i<5; i++)
        for(j=0; j<12; j++)
          cp->cind[i][j] = 0;
      break;
  }
  if(rndstart) randomize(cp);
}

/*************************************************************************/

ENTRYPOINT void reshape_cube21(ModeInfo *mi, int width, int height) 
{
  cube21_conf *cp = &cube21[MI_SCREEN(mi)];
  if(!height) height = 1;
  cp->ratio = (GLfloat)width/(GLfloat)height;
  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0, cp->ratio, 1.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT Bool
cube21_handle_event (ModeInfo *mi, XEvent *event)
{
  cube21_conf *cp = &cube21[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      cp->button_down_p = True;
      gltrackball_start (cp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      cp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (cp->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           cp->button_down_p)
    {
      gltrackball_track (cp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


ENTRYPOINT void release_cube21(ModeInfo *mi) 
{
  if (cube21 != NULL) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
      cube21_conf *cp = &cube21[screen];
      if (cp->glx_context) {
        cp->glx_context = NULL;
      }
    }
    free((void *)cube21);
    cube21 = NULL;
  }
  FreeAllGL(mi);
}

ENTRYPOINT void init_cube21(ModeInfo *mi) 
{
  cube21_conf *cp;
  if(!cube21) {
    cube21 = (cube21_conf *)calloc(MI_NUM_SCREENS(mi), sizeof(cube21_conf));
    if(!cube21) return;
  }
  cp = &cube21[MI_SCREEN(mi)];

  cp->trackball = gltrackball_init ();

  if(!cp->texp) {
    init_posc(cp);
    make_texture(cp);
  }

  if ((cp->glx_context = init_GL(mi)) != NULL) {
    init_gl(mi);
    init_cp(cp);
    reshape_cube21(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }
}

ENTRYPOINT void draw_cube21(ModeInfo * mi) 
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  cube21_conf *cp;
  if (!cube21) return;
  cp = &cube21[MI_SCREEN(mi)];
  MI_IS_DRAWN(mi) = True;
  if (!cp->glx_context) return;
  mi->polygon_count = 0;
  glXMakeCurrent(display, window, *(cp->glx_context));
  if (!draw_main(mi, cp)) {
    release_cube21(mi);
    return;
  }
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  glXSwapBuffers(display, window);
}

#ifndef STANDALONE
ENTRYPOINT void change_cube21(ModeInfo * mi) 
{
  cube21_conf *cp = &cube21[MI_SCREEN(mi)];
  if (!cp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cp->glx_context));
  init_gl(mi);
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE ("Cube21", cube21)

#endif
