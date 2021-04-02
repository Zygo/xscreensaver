/* flipflop, Copyright (c) 2003 Kevin Ogden <kogden1@hotmail.com>
 *                     (c) 2006 Sergio Gutiérrez "Sergut" <sergut@gmail.com>
 *                     (c) 2008 Andrew Galante <a.drew7@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 *
 * 2003 Kevin Odgen                  First version
 * 2006 Sergio Gutiérrez "Sergut"    Made several parameters dynamic and selectable
 *                                   from the command line: size of the board, 
 *                                   rotation speed and number of free squares; also
 *                                   added the "sticks" mode.
 * 2008 Andrew Galante               Added -textured option: textures the board with
 *                                   an image which gets scrambled as the tiles move
 *
 */

#define DEF_MODE  "tiles" /* Default mode (options: "tiles", "sticks") */
#define DEF_SIZEX   "9"     /* Default width of the board */
#define DEF_SIZEY   "9"     /* Default length of the board */

#define DEF_BOARD_SIZE     "0"     /* "0" means "no value selected by user". It is changed */ 
#define DEF_NUMSQUARES     "0"     /* in function init_flipflop() to its correct value (that */ 
#define DEF_FREESQUARES    "0"     /* is a function of the size of the board and the mode)*/

#define DEF_SPIN           "0.1"   /* Default angular velocity: PI/10 rads/s    */

#define DEF_TEXTURED       "False" /* Default: do not grab an image for texturing */

#define DEF_STICK_THICK   54       /* Thickness for the sticks mode (over 100)  */
#define DEF_STICK_RATIO   80       /* Ratio of sticks/total squares (over 100)  */
#define DEF_TILE_THICK     4       /* Thickness for the tiles mode (over 100)   */
#define DEF_TILE_RATIO    95       /* Ratio of tiles/total squares (over 100)   */

#ifdef STANDALONE
#define DEFAULTS       "*delay:     20000     \n" \
                       "*showFPS:   False     \n" \
                       "*wireframe: False     \n"

# define release_flipflop 0
# include "xlockmore.h"

#else
# include "xlock.h"
#endif /* STANDALONE */

#ifdef USE_GL

#include "gltrackball.h"

static XrmOptionDescRec opts[] = {
    {"-sticks",         ".mode",            XrmoptionNoArg,  "sticks"},
    {"-tiles",          ".mode",            XrmoptionNoArg,  "tiles" },
    {"-mode",           ".mode",            XrmoptionSepArg, 0       },
    {"-size",           ".size",            XrmoptionSepArg, 0       },
    {"-size-x",         ".sizex",           XrmoptionSepArg, 0       },
    {"-size-y",         ".sizey",           XrmoptionSepArg, 0       },
    {"-count",          ".numsquares",      XrmoptionSepArg, 0       },
    {"-free",           ".freesquares",     XrmoptionSepArg, 0       },
    {"-spin",           ".spin",            XrmoptionSepArg, 0       },
    {"-texture",        ".textured",        XrmoptionNoArg,  "True"  },
    {"+texture",        ".textured",        XrmoptionNoArg,  "False" },
};

/* The code had been modifying these. That's not allowed. */
static int board_x_size_arg, board_y_size_arg, board_avg_size_arg;
static int numsquares_arg, freesquares_arg;
static float half_thick_arg;
static float spin_arg;
static char* flipflopmode_str_arg="tiles";
static int textured_arg;

static argtype vars[] = {
 { &flipflopmode_str_arg, "mode",        "Mode",     DEF_MODE,       t_String},
 { &board_avg_size_arg,   "size",        "Integer",  DEF_BOARD_SIZE, t_Int},
 { &board_x_size_arg,     "sizex",       "Integer",  DEF_SIZEX,      t_Int},
 { &board_y_size_arg,     "sizey",       "Integer",  DEF_SIZEY,      t_Int},
 { &numsquares_arg,       "numsquares",  "Integer",  DEF_NUMSQUARES, t_Int},
 { &freesquares_arg,      "freesquares", "Integer",  DEF_NUMSQUARES, t_Int},
 { &spin_arg,             "spin",        "Float",    DEF_SPIN,       t_Float},
 { &textured_arg,         "textured",    "Bool",     DEF_TEXTURED,   t_Bool},
};

ENTRYPOINT ModeSpecOpt flipflop_opts =
  {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   flipflop_description =
    {"flipflop", "init_flipflop", "draw_flipflop", NULL,
     "draw_flipflop", "init_flipflop", "free_flipflop", &flipflop_opts,
     1000, 1, 2, 1, 4, 1.0, "",
     "Flipflop", 0, NULL};

#endif /* USE_MODULES */

typedef struct {
  /* array specifying which squares are where (to avoid collisions) */
  /* -1 means empty otherwise integer represents square index 0 - n-1 */
  /* occupied[x*board_y_size+y] is the tile [x][y] (i.e. that starts at column x and row y)*/
  int *occupied; /* size: size_x * size_y */
  /* an array of xpositions of the squares */
  int *xpos; /* size: numsquares */
  /* array of y positions of the squares */
  int *ypos; /* size: numsquares */
  /* integer representing the direction of movement of a square */
  int *direction; /* 0 not, 1 x+, 2 y+, 3 x-, 4 y-*/ /* size: numsquares */
  /* angle of moving square (during a flip) */
  float *angle; /* size: numsquares */
  /* array of colors for a square (RGB) */
  /* eg. color[ 3*3 + 0 ] is the red   component of square 3 */
  /* eg. color[ 4*3 + 1 ] is the green component of square 4 */
  /* eg. color[ 5*3 + 2 ] is the blue  component of square 5 */
  /*            ^-- n is the number of square */
  float *color; /* size: numsquares * 3 */
  /* array of texcoords for each square */
  /* tex[ n*4 + 0 ] is x texture coordinate of square n's left side */
  /* tex[ n*4 + 1 ] is y texture coordinate of square n's top side */
  /* tex[ n*4 + 2 ] is x texture coordinate of square n's right side */
  /* tex[ n*4 + 3 ] is y texture coordinate of square n's bottom side */
  float *tex; /* size: numsquares * 4 */
} randsheet;

typedef struct {
  GLXContext *glx_context;
  Window window;
  trackball_state *trackball;
  Bool button_down_p;

  int clearbits;
  int board_x_size, board_y_size, board_avg_size;
  int numsquares, freesquares;
  float half_thick;
  float spin;
  const char *flipflopmode_str;
  int textured;

  randsheet *sheet;

  float theta;      /* angle of rotation of the board                */
  float flipspeed;  /* amount of flip;  1 is a entire flip           */
  float reldist;    /* relative distace of camera from center        */
  float energy;     /* likelyhood that a square will attempt to move */

  /* texture rectangle */
  float tex_x;
  float tex_y;
  float tex_width;
  float tex_height;

  /* id of texture in use */
  GLuint texid;

  Bool mipmap;
  Bool got_texture;

  GLfloat anisotropic;

} Flipflopcreen;

static Flipflopcreen *qs = NULL;

#include "grab-ximage.h"

static void randsheet_create(ModeInfo *mi, randsheet *rs);
static void randsheet_initialize(ModeInfo *mi, randsheet *rs);
static void randsheet_free (randsheet *rs);
static int  randsheet_new_move(ModeInfo *mi, randsheet* rs);
static void randsheet_move(ModeInfo *mi, randsheet *rs, float rot);
static int randsheet_draw(ModeInfo *mi, randsheet *rs);
static void setup_lights(ModeInfo *mi);
static int drawBoard(ModeInfo *mi, Flipflopcreen *);
static int display(ModeInfo *mi);
static int draw_sheet(ModeInfo *mi, float *tex);


/* configure lighting */
static void
setup_lights(ModeInfo *mi)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];

  /*   GLfloat position0[] = { c->board_avg_size*0.5, c->board_avg_size*0.1, c->board_avg_size*0.5, 1.0 }; */

  /*   GLfloat position0[] = { -c->board_avg_size*0.5, 0.2*c->board_avg_size, -c->board_avg_size*0.5, 1.0 }; */
  GLfloat position0[4];
  position0[0] = 0;
  position0[1] = c->board_avg_size*0.3;
  position0[2] = 0;
  position0[3] = 1;

  if (MI_IS_WIREFRAME(mi)) return;

  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glEnable(GL_LIGHT0);
}

static void get_texture(ModeInfo *);


ENTRYPOINT Bool
flipflop_handle_event (ModeInfo *mi, XEvent *event)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, c->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &c->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      if (!c->textured || c->got_texture)
        {
          c->textured = 1;
          c->got_texture = False;
          get_texture (mi);
          return True;
        }
    }

  return False;
}

/* draw board */
static int
drawBoard(ModeInfo *mi, Flipflopcreen *c)
{
  int i;
  for (i=0; i < (c->energy) ; i++) {
    randsheet_new_move(mi, c->sheet);
  }
  randsheet_move(mi, c->sheet, c->flipspeed * M_PI);
  return randsheet_draw(mi, c->sheet);
}


static int
display(ModeInfo *mi)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  GLfloat amb[] = { 0.8, 0.8, 0.8, 1.0 };
  int polys = 0;


  glClear(c->clearbits);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.2);
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.15/c->board_avg_size);
  glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.15/c->board_avg_size);
  glLightfv(GL_LIGHT0, GL_AMBIENT, amb);


  glRotatef(current_device_rotation(), 0, 0, 1);

  /** setup perspectif */
  glTranslatef(0.0, 0.0, -c->reldist*c->board_avg_size);
  glRotatef(22.5, 1.0, 0.0, 0.0);  
  gltrackball_rotate (c->trackball);
  glRotatef(c->theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*c->board_x_size, 0.0, -0.5*c->board_y_size); /* Center the board */

  /* set texture */
  if(c->textured)
    glBindTexture(GL_TEXTURE_2D, c->texid);

  polys = drawBoard(mi, c);

  if (!c->button_down_p) {
    c->theta += .01 * c->spin;
  }

  return polys;
}

ENTRYPOINT void
reshape_flipflop(ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport(0,y, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 1.0, 300.0);
  glMatrixMode(GL_MODELVIEW);
}

static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  Flipflopcreen *c = (Flipflopcreen *)closure;
  int i, j;
  int index = 0;
  randsheet *rs = c->sheet;

  c->tex_x = (float)geometry->x / (float)texture_width;
  c->tex_y = (float)geometry->y / (float)texture_height;
  c->tex_width = (float)geometry->width / (float)texture_width; 
  c->tex_height = (float)geometry->height / (float)texture_height;

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  (c->mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  if(c->anisotropic >= 1)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, c->anisotropic);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for(i = 0; i < c->board_x_size && index < c->numsquares; i++)
    for(j = 0; j < c->board_y_size && index < c->numsquares; j++)
      {
        /* arrange squares to form loaded image */
        rs->tex[ index*4 + 0 ] = c->tex_x + c->tex_width  / c->board_x_size * (i + 0);
        rs->tex[ index*4 + 1 ] = c->tex_y + c->tex_height / c->board_y_size * (j + 1);
        rs->tex[ index*4 + 2 ] = c->tex_x + c->tex_width  / c->board_x_size * (i + 1);
        rs->tex[ index*4 + 3 ] = c->tex_y + c->tex_height / c->board_y_size * (j + 0);
        rs->color[ index*3 + 0 ] = 1;
        rs->color[ index*3 + 1 ] = 1;
        rs->color[ index*3 + 2 ] = 1;
        index++;
      }

  c->got_texture = True;
}

static void
get_texture(ModeInfo *modeinfo)
{
  Flipflopcreen *c = &qs[MI_SCREEN(modeinfo)];

  c->got_texture = False;
  c->mipmap = True;
  load_texture_async (modeinfo->xgwa.screen, modeinfo->window,
                      *c->glx_context, 0, 0, c->mipmap, c->texid,
                      image_loaded_cb, c);
}

ENTRYPOINT void
init_flipflop(ModeInfo *mi)
{
  Flipflopcreen *c;
  MI_INIT(mi, qs);
  c = &qs[MI_SCREEN(mi)];

  c->board_x_size = board_x_size_arg;
  c->board_y_size = board_y_size_arg;
  c->board_avg_size = board_avg_size_arg;
  c->numsquares = numsquares_arg;
  c->freesquares = freesquares_arg;
  c->half_thick = half_thick_arg;
  c->spin = spin_arg;
  c->flipflopmode_str = flipflopmode_str_arg;
  c->textured = textured_arg;

  if (MI_IS_WIREFRAME(mi)) c->textured = 0;

  /* Set all constants to their correct values */
  if (c->board_avg_size != 0) {  /* general size specified by user */
    c->board_x_size = c->board_avg_size;
    c->board_y_size = c->board_avg_size;
  } else {
    c->board_avg_size = (c->board_x_size + c->board_y_size) / 2;
  }
  if ((c->numsquares == 0) && (c->freesquares != 0)) {
    c->numsquares = c->board_x_size * c->board_y_size - c->freesquares; 
  }
  if (strcmp(c->flipflopmode_str, "tiles")) {
    c->textured = 0;  /* textures look dumb in stick mode */
    c->half_thick = 1.0 * DEF_STICK_THICK / 100.0; 
    if (c->numsquares == 0) {  /* No value defined by user */
      c->numsquares = c->board_x_size * c->board_y_size * DEF_STICK_RATIO / 100;
    }
  } else {
    c->half_thick = 1.0 * DEF_TILE_THICK / 100.0; 
    if (c->numsquares == 0) {  /* No value defined by user */
      c->numsquares = c->board_x_size * c->board_y_size * DEF_TILE_RATIO/ 100;;
    }
  }
  if (c->board_avg_size < 2) {
    fprintf (stderr,"%s: the board must be at least 2x2.\n", progname);
    exit(1);
  }
  if ((c->board_x_size < 1) || (c->board_y_size < 1) ||	(c->numsquares < 1)) {
    fprintf (stderr,"%s: the number of elements ('-count') and the dimensions of the board ('-size-x', '-size-y') must be positive integers.\n", progname);
    exit(1);
  }
  if (c->board_x_size * c->board_y_size <= c->numsquares) {
    fprintf (stderr,"%s: the number of elements ('-count') that you specified is too big \n for the dimensions of the board ('-size-x', '-size-y'). Nothing will move.\n", progname);
  }

  c->window = MI_WINDOW(mi);
  c->trackball = gltrackball_init (False);

  c->flipspeed = 0.03;
  c->reldist = 1;
  c->energy = 40;

  if((c->glx_context = init_GL(mi)))
    reshape_flipflop(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  else
    MI_CLEARWINDOW(mi);

  /* At this point, all the constants have already been set, */
  /* so we can create the board */
  c->sheet = (randsheet*) malloc(sizeof(randsheet)); 
  randsheet_create(mi, c->sheet); 

  c->clearbits = GL_COLOR_BUFFER_BIT;

  glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  setup_lights(mi);

  glEnable(GL_DEPTH_TEST);
  c->clearbits |= GL_DEPTH_BUFFER_BIT;
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  randsheet_initialize(mi, c->sheet);
  if (c->textured){
    /* check for anisotropic filtering */
    if(strstr((char *)glGetString(GL_EXTENSIONS),
              "GL_EXT_texture_filter_anisotropic"))
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &c->anisotropic);
    else
      c->anisotropic = 0;

    /* allocate a new texture and get it */
    glGenTextures(1, &c->texid);
    get_texture(mi);
  }
}

ENTRYPOINT void
draw_flipflop(ModeInfo *mi)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  glXMakeCurrent(disp, w, *c->glx_context);

  if(!c->textured || c->got_texture)
    mi->polygon_count = display(mi);
  else
    glClear(GL_COLOR_BUFFER_BIT);
  
  if(mi->fps_p){
    do_fps(mi);
  }

  glFinish();
  glXSwapBuffers(disp, w);


}

ENTRYPOINT void
free_flipflop(ModeInfo *mi)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  if (!c->glx_context) return;
  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *c->glx_context);
  if(c->trackball) gltrackball_free(c->trackball);
  if (c->sheet) {
    randsheet_free(c->sheet);
    free (c->sheet);
  }
  if (c->texid) glDeleteTextures (1, &c->texid);
}

/*** ADDED RANDSHEET FUNCTIONS ***/

static int
draw_sheet(ModeInfo *mi, float *tex)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  int polys = 0;
  int wire = MI_IS_WIREFRAME(mi);
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

  glNormal3f (0, -1, 0);
  glTexCoord2f(tex[0], tex[3]);
  glVertex3f (c->half_thick,  -c->half_thick,  c->half_thick);
  glTexCoord2f(tex[2], tex[3]);
  glVertex3f (1-c->half_thick,   -c->half_thick, c->half_thick);
  glTexCoord2f(tex[2], tex[1]);
  glVertex3f (1-c->half_thick, -c->half_thick,  1-c->half_thick);
  glTexCoord2f(tex[0], tex[1]);
  glVertex3f (c->half_thick, -c->half_thick, 1-c->half_thick);
  polys++;

  if (wire) { glEnd(); glBegin (GL_LINE_LOOP); }

  /* back */
  glNormal3f (0, 1, 0);
  glTexCoord2f(tex[0], tex[1]);
  glVertex3f (c->half_thick, c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[2], tex[1]);
  glVertex3f (1-c->half_thick, c->half_thick,  1-c->half_thick);
  glTexCoord2f(tex[2], tex[3]);
  glVertex3f (1-c->half_thick,   c->half_thick, c->half_thick);
  glTexCoord2f(tex[0], tex[3]);
  glVertex3f (c->half_thick,  c->half_thick,  c->half_thick);
  polys++;

  if (wire) { glEnd(); return polys; }

  /* 4 edges!!! weee.... */
  glNormal3f (0, 0, -1);
  glTexCoord2f(tex[0], tex[3]);
  glVertex3f (c->half_thick, c->half_thick, c->half_thick);
  glTexCoord2f(tex[2], tex[3]);
  glVertex3f (1-c->half_thick, c->half_thick, c->half_thick);
  glTexCoord2f(tex[2], tex[3]);
  glVertex3f (1-c->half_thick, -c->half_thick, c->half_thick);
  glTexCoord2f(tex[0], tex[3]);
  glVertex3f (c->half_thick, -c->half_thick, c->half_thick);
  polys++;
  glNormal3f (0, 0, 1);
  glTexCoord2f(tex[0], tex[1]);
  glVertex3f (c->half_thick, c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[0], tex[1]);
  glVertex3f (c->half_thick, -c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[2], tex[1]);
  glVertex3f (1-c->half_thick, -c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[2], tex[1]);
  glVertex3f (1-c->half_thick, c->half_thick, 1-c->half_thick);
  polys++;
  glNormal3f (1, 0, 0);
  glTexCoord2f(tex[2], tex[1]);
  glVertex3f (1-c->half_thick, c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[2], tex[1]);
  glVertex3f (1-c->half_thick, -c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[2], tex[3]);
  glVertex3f (1-c->half_thick, -c->half_thick, c->half_thick);
  glTexCoord2f(tex[2], tex[3]);
  glVertex3f (1-c->half_thick, c->half_thick, c->half_thick);
  polys++;
  glNormal3f (-1, 0, 0);
  glTexCoord2f(tex[0], tex[1]);
  glVertex3f (c->half_thick, c->half_thick, 1-c->half_thick);
  glTexCoord2f(tex[0], tex[3]);
  glVertex3f (c->half_thick, c->half_thick, c->half_thick);
  glTexCoord2f(tex[0], tex[3]);
  glVertex3f (c->half_thick, -c->half_thick, c->half_thick);
  glTexCoord2f(tex[0], tex[1]);
  glVertex3f (c->half_thick, -c->half_thick, 1-c->half_thick);
  polys++;
  glEnd();

  return polys;
}

/* Reserve memory for the randsheet */
static void
randsheet_create(ModeInfo *mi, randsheet *rs)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  rs -> occupied  = (int*) malloc(c->board_x_size*c->board_y_size * sizeof(int));
  rs -> xpos      = (int*) malloc(c->numsquares * sizeof(int));
  rs -> ypos      = (int*) malloc(c->numsquares * sizeof(int));
  rs -> direction = (int*) malloc(c->numsquares * sizeof(int));
  rs -> angle     = (float*) malloc(c->numsquares * sizeof(float));
  rs -> color     = (float*) malloc(c->numsquares*3 * sizeof(float));
  rs -> tex       = (float*) malloc(c->numsquares*4 * sizeof(float));
}

/* Free reserved memory for the randsheet */
static void
randsheet_free (randsheet *rs)
{
  free(rs->occupied);
  free(rs->xpos);
  free(rs->ypos);
  free(rs->direction);
  free(rs->angle);
  free(rs->color);
  free(rs->tex);
}

static void
randsheet_initialize(ModeInfo *mi, randsheet *rs)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  int i, j, index;
  index = 0;
  /* put the moving sheets on the board */
  for (i = 0; i < c->board_x_size; i++)
    {
      for (j = 0; j < c->board_y_size; j++)
        {
          /* initially fill up a corner with the moving squares */
          if (index < c->numsquares)
            {
              rs->occupied[ i * c->board_y_size + j ] = index;
              rs->xpos[ index ] = i;
              rs->ypos[ index ] = j;
              /* have the square colors start out as a pattern */
              rs->color[ index*3 + 0 ] = ((i+j)%3 == 0)||((i+j+1)%3 == 0);
              rs->color[ index*3 + 1 ] = ((i+j+1)%3 == 0);
              rs->color[ index*3 + 2 ] = ((i+j+2)%3 == 0);
              index++;
            }
          /* leave everything else empty*/
          else
            {
              rs->occupied[ i * c->board_y_size + j ] = -1;
            }
        }
    }
  /* initially everything is at rest */
  for (i=0; i<c->numsquares; i++)
    {
      rs->direction[ i ] = 0;
      rs->angle[ i ] = 0;
    }
}

/* Pick and random square and direction and try to move it. */
/* May not actually move anything, just attempt a random move. */
/* Returns true if move was sucessful. */
/* This could probably be implemented faster in a dequeue */
/* to avoid trying to move a square which is already moving */
/* but speed is most likely bottlenecked by rendering anyway... */
static int
randsheet_new_move(ModeInfo *mi, randsheet* rs)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  int i, j;
  int num, dir;
  /* pick a random square */
  num = random() % c->numsquares;
  i = rs->xpos[ num ];
  j = rs->ypos[ num ];
  /* pick a random direction */
  dir =  (random()% 4) + 1;

  if (rs->direction[ num ] == 0)
    {
      switch (dir)
        {
        case 1:
          /* move up in x */
          if ((i + 1) < c->board_x_size)
            {
              if (rs->occupied[ (i + 1) * c->board_y_size + j ] == -1)
                {
                  rs->direction[ num ] = dir;
                  rs->occupied[ (i + 1) * c->board_y_size + j ] = num;
                  rs->occupied[ i * c->board_y_size + j ] = -1;
                  return 1;
                }
            }
          return 0;
          break;
        case 2:
          /* move up in y */
          if ((j + 1) < c->board_y_size)
            {
              if (rs->occupied[ i * c->board_y_size + (j + 1) ] == -1)
                {
                  rs->direction[ num ] = dir;
                  rs->occupied[ i * c->board_y_size + (j + 1) ] = num;
                  rs->occupied[ i * c->board_y_size + j ] = -1;
                  return 1;
                }
            }
          return 0;
          break;
        case 3:
          /* move down in x */
          if ((i - 1) >= 0)
            {
              if (rs->occupied[ (i - 1) * c->board_y_size + j ] == -1)
                {
                  rs->direction[ num ] = dir;
                  rs->occupied[ (i - 1) * c->board_y_size + j ] = num;
                  rs->occupied[ i * c->board_y_size + j ] = -1;
                  return 1;
                }
            }
          return 0;
          break;
        case 4:
          /* move down in y */
          if ((j - 1) >= 0)
            {
              if (rs->occupied[ i * c->board_y_size + (j - 1) ] == -1)
                {
                  rs->direction[ num ] = dir;
                  rs->occupied[ i * c->board_y_size + (j - 1) ] = num;
                  rs->occupied[ i * c->board_y_size + j ] = -1;
                  return 1;
                }
            }
          return 0;
          break;
        default:
          break;
        }
    }
  return 0;
}

/*   move a single frame.  */
/*   Pass in the angle in rads the square rotates in a frame. */
static void
randsheet_move(ModeInfo *mi, randsheet *rs, float rot)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  int index;
  float tmp;
  for (index = 0 ; index < c->numsquares; index++)
    {
      switch (rs->direction[ index ])
        {
        case 0:
          /* not moving */
          break;
        case 1:
          /* move up in x */
          if (c->textured && rs->angle[ index ] == 0)
            {
              tmp = rs->tex[ index * 4 + 0 ];
              rs->tex[ index * 4 + 0 ] = rs->tex[ index * 4 + 2 ];
              rs->tex[ index * 4 + 2 ] = tmp;
            }
          rs->angle[ index ] += rot;
          /* check to see if we have finished moving */
          if (rs->angle[ index ] >= M_PI)
            {
              rs->xpos[ index ] += 1;
              rs->direction[ index ] = 0;
              rs->angle[ index ] = 0;
            }
          break;
        case 2:
          /* move up in y */
          if (c->textured && rs->angle[ index ] == 0)
            {
              tmp = rs->tex[ index * 4 + 1 ];
              rs->tex[ index * 4 + 1 ] = rs->tex[ index * 4 + 3 ];
              rs->tex[ index * 4 + 3 ] = tmp;
            }
          rs->angle[ index ] += rot;
          /* check to see if we have finished moving */
          if (rs->angle[ index ] >= M_PI)
            {
              rs->ypos[ index ] += 1;
              rs->direction[ index ] = 0;
              rs->angle[ index ] = 0;
            }
          break;
        case 3:
          /* down in x */
          rs->angle[ index ] += rot;
          /* check to see if we have finished moving */
          if (rs->angle[ index ] >= M_PI)
            {
              rs->xpos[ index ] -= 1;
              rs->direction[ index ] = 0;
              rs->angle[ index ] = 0;
              if (c->textured)
                {
                  tmp = rs->tex[ index * 4 + 0 ];
                  rs->tex[ index * 4 + 0 ] = rs->tex[ index * 4 + 2 ];
                  rs->tex[ index * 4 + 2 ] = tmp;
                }
            }
          break;
        case 4:
          /* down in y */
          rs->angle[ index ] += rot;
          /* check to see if we have finished moving */
          if (rs->angle[ index ] >= M_PI)
            {
              rs->ypos[ index ] -= 1;
              rs->direction[ index ] = 0;
              rs->angle[ index ] = 0;
              if (c->textured)
                {
                  tmp = rs->tex[ index * 4 + 1 ];
                  rs->tex[ index * 4 + 1 ] = rs->tex[ index * 4 + 3 ];
                  rs->tex[ index * 4 + 3 ] = tmp;
                }
            }
          break;
        default:
          break;
        }
    }
}


/* draw all the moving squares  */
static int
randsheet_draw(ModeInfo *mi, randsheet *rs)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];

  int i, j, polys = 0;
  int index;

  /* for all moving squares ... */
  for (index = 0; index < c->numsquares; index++)
    {
      /* set color */
      glColor3f (rs->color[ index*3 + 0 ],
                 rs->color[ index*3 + 1 ],
                 rs->color[ index*3 + 2 ]);
      /* find x and y position */
      i = rs->xpos[ index ];
      j = rs->ypos[ index ];
      glPushMatrix();
      switch (rs->direction[ index ])
        {
        case 0:

          /* not moving */
          /* front */
          glTranslatef (i, 0, j);
          break;
        case 1:
          glTranslatef (i+1, 0, j);
          glRotatef (180 - rs->angle[ index ]*180/M_PI, 0, 0, 1);

          break;
        case 2:
          glTranslatef (i, 0, j+1);
          glRotatef (180 - rs->angle[ index ]*180/M_PI, -1, 0, 0);

          break;
        case 3:
          glTranslatef (i, 0, j);
          glRotatef (rs->angle[ index ]*180/M_PI, 0, 0, 1);
          break;
        case 4:
          glTranslatef (i, 0, j);
          glRotatef (rs->angle[ index ]*180/M_PI, -1, 0, 0);
          break;
        default:
          break;
        }
      polys += draw_sheet(mi, rs->tex + index*4);
      glPopMatrix();

    }
  return polys;
}

/**** END RANDSHEET_BAK FUNCTIONS ***/

XSCREENSAVER_MODULE ("FlipFlop", flipflop)

#endif /* USE_GL */
