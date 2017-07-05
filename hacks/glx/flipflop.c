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

# define refresh_flipflop 0
# define release_flipflop 0
# include "xlockmore.h"

#else
# include "xlock.h"
#endif /* STANDALONE */

#ifdef USE_GL

#include "gltrackball.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

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

static int wire, clearbits;
static int board_x_size, board_y_size, board_avg_size;
static int numsquares, freesquares;
static float half_thick;
static float spin;
static char* flipflopmode_str="tiles";
static int textured;

static argtype vars[] = {
    { &flipflopmode_str, "mode",        "Mode",     DEF_MODE,  t_String},
    { &board_avg_size,   "size",        "Integer",  DEF_BOARD_SIZE,     t_Int},
    { &board_x_size,     "sizex",       "Integer",  DEF_SIZEX,   t_Int},
    { &board_y_size,     "sizey",       "Integer",  DEF_SIZEY,   t_Int},
    { &numsquares,       "numsquares",  "Integer",  DEF_NUMSQUARES,     t_Int},
    { &freesquares,      "freesquares", "Integer",  DEF_NUMSQUARES,     t_Int},
    { &spin,             "spin",        "Float",    DEF_SPIN,           t_Float},
    { &textured,         "textured",    "Bool",     DEF_TEXTURED,       t_Bool},
};

ENTRYPOINT ModeSpecOpt flipflop_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   flipflop_description =
    {"flipflop", "init_flipflop", "draw_flipflop", NULL,
     "draw_flipflop", "init_flipflop", NULL, &flipflop_opts,
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

static void randsheet_create( randsheet *rs );
static void randsheet_initialize( randsheet *rs );
static void randsheet_free( randsheet *rs );
static int  randsheet_new_move( randsheet* rs );
static void randsheet_move( randsheet *rs, float rot );
static int randsheet_draw( randsheet *rs );
static void setup_lights(void);
static int drawBoard(Flipflopcreen *);
static int display(ModeInfo *mi);
static int draw_sheet(float *tex);


/* configure lighting */
static void
setup_lights(void)
{
    /*   GLfloat position0[] = { board_avg_size*0.5, board_avg_size*0.1, board_avg_size*0.5, 1.0 }; */

    /*   GLfloat position0[] = { -board_avg_size*0.5, 0.2*board_avg_size, -board_avg_size*0.5, 1.0 }; */
    GLfloat position0[4];
    position0[0] = 0;
    position0[1] = board_avg_size*0.3;
    position0[2] = 0;
    position0[3] = 1;

    if (wire) return;

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
      if (!textured || c->got_texture)
        {
          textured = 1;
          c->got_texture = False;
          get_texture (mi);
          return True;
        }
    }

    return False;
}

/* draw board */
static int
drawBoard(Flipflopcreen *c)
{
    int i;
    for( i=0; i < (c->energy) ; i++ ) {
        randsheet_new_move( c->sheet );
	}
    randsheet_move( c->sheet, c->flipspeed * 3.14159 );
    return randsheet_draw( c->sheet );
}


static int
display(ModeInfo *mi)
{
    Flipflopcreen *c = &qs[MI_SCREEN(mi)];
    GLfloat amb[] = { 0.8, 0.8, 0.8, 1.0 };
    int polys = 0;


    glClear(clearbits);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.2);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.15/board_avg_size );
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.15/board_avg_size );
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);


    glRotatef(current_device_rotation(), 0, 0, 1);

    /** setup perspectif */
    glTranslatef(0.0, 0.0, -c->reldist*board_avg_size);
    glRotatef(22.5, 1.0, 0.0, 0.0);  
    gltrackball_rotate (c->trackball);
    glRotatef(c->theta*100, 0.0, 1.0, 0.0);
    glTranslatef(-0.5*board_x_size, 0.0, -0.5*board_y_size); /* Center the board */

    /* set texture */
    if(textured)
      glBindTexture(GL_TEXTURE_2D, c->texid);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
    {
      GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
      int o = (int) current_device_rotation();
      if (o != 0 && o != 180 && o != -180)
        glScalef (1/h, 1/h, 1/h);
    }
# endif

    polys = drawBoard(c);

    if (!c->button_down_p) {
        c->theta += .01 * spin;
    }

    return polys;
}

ENTRYPOINT void
reshape_flipflop(ModeInfo *mi, int width, int height)
{
    GLfloat h = (GLfloat) height / (GLfloat) width;
    glViewport(0,0, width, height);
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

    for(i = 0; i < board_x_size && index < numsquares; i++)
        for(j = 0; j < board_y_size && index < numsquares; j++)
        {
            /* arrange squares to form loaded image */
            rs->tex[ index*4 + 0 ] = c->tex_x + c->tex_width  / board_x_size * (i + 0);
            rs->tex[ index*4 + 1 ] = c->tex_y + c->tex_height / board_y_size * (j + 1);
            rs->tex[ index*4 + 2 ] = c->tex_x + c->tex_width  / board_x_size * (i + 1);
            rs->tex[ index*4 + 3 ] = c->tex_y + c->tex_height / board_y_size * (j + 0);
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

static void free_flipflop(ModeInfo *mi);

ENTRYPOINT void
init_flipflop(ModeInfo *mi)
{
    int screen; 
    Flipflopcreen *c;

    if (MI_IS_WIREFRAME(mi)) textured = 0;

    /* Set all constants to their correct values */
    if (board_avg_size != 0) {  /* general size specified by user */
        board_x_size = board_avg_size;
        board_y_size = board_avg_size;
    } else {
        board_avg_size = (board_x_size + board_y_size) / 2;
    }
    if ((numsquares == 0) && (freesquares != 0)) {
        numsquares = board_x_size * board_y_size - freesquares; 
    }
    if (strcmp(flipflopmode_str, "tiles")) {
      textured = 0;  /* textures look dumb in stick mode */
        half_thick = 1.0 * DEF_STICK_THICK / 100.0; 
        if (numsquares == 0) {  /* No value defined by user */
            numsquares = board_x_size * board_y_size * DEF_STICK_RATIO / 100;
        }
    } else {
        half_thick = 1.0 * DEF_TILE_THICK / 100.0; 
        if (numsquares == 0) {  /* No value defined by user */
            numsquares = board_x_size * board_y_size * DEF_TILE_RATIO/ 100;;
        }
    }
    if (board_avg_size < 2) {
        fprintf (stderr,"%s: the board must be at least 2x2.\n", progname);
        exit(1);
    }
    if ((board_x_size < 1) || (board_y_size < 1) ||	(numsquares < 1)) {
        fprintf (stderr,"%s: the number of elements ('-count') and the dimensions of the board ('-size-x', '-size-y') must be positive integers.\n", progname);
        exit(1);
    }
    if (board_x_size * board_y_size <= numsquares) {
        fprintf (stderr,"%s: the number of elements ('-count') that you specified is too big \n for the dimensions of the board ('-size-x', '-size-y'). Nothing will move.\n", progname);
    }

    screen = MI_SCREEN(mi);
    wire = MI_IS_WIREFRAME(mi);

    MI_INIT(mi, qs, free_flipflop);

    c = &qs[screen];
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
    randsheet_create( c->sheet ); 

    clearbits = GL_COLOR_BUFFER_BIT;

    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    setup_lights();

    glEnable(GL_DEPTH_TEST);
    clearbits |= GL_DEPTH_BUFFER_BIT;
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    randsheet_initialize( c->sheet );
    if( textured ){
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

    if(!c->glx_context || (textured && !c->got_texture))
        return;

    glXMakeCurrent(disp, w, *(c->glx_context));

    mi->polygon_count = display(mi);

    if(mi->fps_p){
        do_fps(mi);
    }

    glFinish();
    glXSwapBuffers(disp, w);


}

static void
free_flipflop(ModeInfo *mi)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  if (c->sheet) {
    randsheet_free(c->sheet);
    free (c->sheet);
  }
}

/*** ADDED RANDSHEET FUNCTIONS ***/

static int
draw_sheet(float *tex)
{
    int polys = 0;
    glBegin( wire ? GL_LINE_LOOP : GL_QUADS );

    glNormal3f( 0, -1, 0 );
    glTexCoord2f(tex[0], tex[3]);
    glVertex3f( half_thick,  -half_thick,  half_thick );
    glTexCoord2f(tex[2], tex[3]);
    glVertex3f( 1-half_thick,   -half_thick, half_thick );
    glTexCoord2f(tex[2], tex[1]);
    glVertex3f( 1-half_thick, -half_thick,  1-half_thick);
    glTexCoord2f(tex[0], tex[1]);
    glVertex3f( half_thick, -half_thick, 1-half_thick );
    polys++;

    if (wire) { glEnd(); glBegin (GL_LINE_LOOP); }

    /* back */
    glNormal3f( 0, 1, 0 );
    glTexCoord2f(tex[0], tex[1]);
    glVertex3f( half_thick, half_thick, 1-half_thick );
    glTexCoord2f(tex[2], tex[1]);
    glVertex3f( 1-half_thick, half_thick,  1-half_thick);
    glTexCoord2f(tex[2], tex[3]);
    glVertex3f( 1-half_thick,   half_thick, half_thick );
    glTexCoord2f(tex[0], tex[3]);
    glVertex3f( half_thick,  half_thick,  half_thick );
    polys++;

    if (wire) { glEnd(); return polys; }

    /* 4 edges!!! weee.... */
    glNormal3f( 0, 0, -1 );
    glTexCoord2f(tex[0], tex[3]);
    glVertex3f( half_thick, half_thick, half_thick );
    glTexCoord2f(tex[2], tex[3]);
    glVertex3f( 1-half_thick, half_thick, half_thick );
    glTexCoord2f(tex[2], tex[3]);
    glVertex3f( 1-half_thick, -half_thick, half_thick );
    glTexCoord2f(tex[0], tex[3]);
    glVertex3f( half_thick, -half_thick, half_thick );
    polys++;
    glNormal3f( 0, 0, 1 );
    glTexCoord2f(tex[0], tex[1]);
    glVertex3f( half_thick, half_thick, 1-half_thick );
    glTexCoord2f(tex[0], tex[1]);
    glVertex3f( half_thick, -half_thick, 1-half_thick );
    glTexCoord2f(tex[2], tex[1]);
    glVertex3f( 1-half_thick, -half_thick, 1-half_thick );
    glTexCoord2f(tex[2], tex[1]);
    glVertex3f( 1-half_thick, half_thick, 1-half_thick );
    polys++;
    glNormal3f( 1, 0, 0 );
    glTexCoord2f(tex[2], tex[1]);
    glVertex3f( 1-half_thick, half_thick, 1-half_thick );
    glTexCoord2f(tex[2], tex[1]);
    glVertex3f( 1-half_thick, -half_thick, 1-half_thick );
    glTexCoord2f(tex[2], tex[3]);
    glVertex3f( 1-half_thick, -half_thick, half_thick );
    glTexCoord2f(tex[2], tex[3]);
    glVertex3f( 1-half_thick, half_thick, half_thick );
    polys++;
    glNormal3f( -1, 0, 0 );
    glTexCoord2f(tex[0], tex[1]);
    glVertex3f( half_thick, half_thick, 1-half_thick );
    glTexCoord2f(tex[0], tex[3]);
    glVertex3f( half_thick, half_thick, half_thick );
    glTexCoord2f(tex[0], tex[3]);
    glVertex3f( half_thick, -half_thick, half_thick );
    glTexCoord2f(tex[0], tex[1]);
    glVertex3f( half_thick, -half_thick, 1-half_thick );
    polys++;
    glEnd();

    return polys;
}

/* Reserve memory for the randsheet */
static void
randsheet_create( randsheet *rs )
{
	rs -> occupied  = (int*) malloc(board_x_size*board_y_size * sizeof(int));
	rs -> xpos      = (int*) malloc(numsquares * sizeof(int));
	rs -> ypos      = (int*) malloc(numsquares * sizeof(int));
	rs -> direction = (int*) malloc(numsquares * sizeof(int));
	rs -> angle     = (float*) malloc(numsquares * sizeof(float));
	rs -> color     = (float*) malloc(numsquares*3 * sizeof(float));
	rs -> tex       = (float*) malloc(numsquares*4 * sizeof(float));
}

/* Free reserved memory for the randsheet */
static void
randsheet_free( randsheet *rs )
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
randsheet_initialize( randsheet *rs )
{
    int i, j, index;
    index = 0;
    /* put the moving sheets on the board */
    for( i = 0; i < board_x_size; i++ )
        {
            for( j = 0; j < board_y_size; j++ )
                {
                    /* initially fill up a corner with the moving squares */
                    if( index < numsquares )
                        {
                            rs->occupied[ i * board_y_size + j ] = index;
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
                            rs->occupied[ i * board_y_size + j ] = -1;
                        }
                }
        }
    /* initially everything is at rest */
    for( i=0; i<numsquares; i++ )
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
randsheet_new_move( randsheet* rs )
{
    int i, j;
    int num, dir;
    /* pick a random square */
    num = random( ) % numsquares;
    i = rs->xpos[ num ];
    j = rs->ypos[ num ];
    /* pick a random direction */
    dir = ( random( )% 4 ) + 1;

    if( rs->direction[ num ] == 0 )
        {
            switch( dir )
                {
                case 1:
                    /* move up in x */
                    if( ( i + 1 ) < board_x_size )
                        {
                            if( rs->occupied[ (i + 1) * board_y_size + j ] == -1 )
                                {
                                    rs->direction[ num ] = dir;
                                    rs->occupied[ (i + 1) * board_y_size + j ] = num;
                                    rs->occupied[ i * board_y_size + j ] = -1;
                                    return 1;
                                }
                        }
                    return 0;
                    break;
                case 2:
                    /* move up in y */
                    if( ( j + 1 ) < board_y_size )
                        {
                            if( rs->occupied[ i * board_y_size + (j + 1) ] == -1 )
                                {
                                    rs->direction[ num ] = dir;
                                    rs->occupied[ i * board_y_size + (j + 1) ] = num;
                                    rs->occupied[ i * board_y_size + j ] = -1;
                                    return 1;
                                }
                        }
                    return 0;
                    break;
                case 3:
                    /* move down in x */
                    if( ( i - 1 ) >= 0 )
                        {
                            if( rs->occupied[ (i - 1) * board_y_size + j ] == -1 )
                                {
                                    rs->direction[ num ] = dir;
                                    rs->occupied[ (i - 1) * board_y_size + j ] = num;
                                    rs->occupied[ i * board_y_size + j ] = -1;
                                    return 1;
                                }
                        }
                    return 0;
                    break;
                case 4:
                    /* move down in y */
                    if( ( j - 1 ) >= 0 )
                        {
                            if( rs->occupied[ i * board_y_size + (j - 1) ] == -1 )
                                {
                                    rs->direction[ num ] = dir;
                                    rs->occupied[ i * board_y_size + (j - 1) ] = num;
                                    rs->occupied[ i * board_y_size + j ] = -1;
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
randsheet_move( randsheet *rs, float rot )
{
    int index;
    float tmp;
    for( index = 0 ; index < numsquares; index++ )
        {
            switch( rs->direction[ index ] )
                {
                case 0:
                    /* not moving */
                    break;
                case 1:
                    /* move up in x */
                    if( textured && rs->angle[ index ] == 0 )
                        {
                            tmp = rs->tex[ index * 4 + 0 ];
                            rs->tex[ index * 4 + 0 ] = rs->tex[ index * 4 + 2 ];
                            rs->tex[ index * 4 + 2 ] = tmp;
                        }
                    rs->angle[ index ] += rot;
                    /* check to see if we have finished moving */
                    if( rs->angle[ index ] >= M_PI  )
                        {
                            rs->xpos[ index ] += 1;
                            rs->direction[ index ] = 0;
                            rs->angle[ index ] = 0;
                        }
                    break;
                case 2:
                    /* move up in y */
                    if( textured && rs->angle[ index ] == 0 )
                        {
                            tmp = rs->tex[ index * 4 + 1 ];
                            rs->tex[ index * 4 + 1 ] = rs->tex[ index * 4 + 3 ];
                            rs->tex[ index * 4 + 3 ] = tmp;
                        }
                    rs->angle[ index ] += rot;
                    /* check to see if we have finished moving */
                    if( rs->angle[ index ] >= M_PI  )
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
                    if( rs->angle[ index ] >= M_PI  )
                        {
                            rs->xpos[ index ] -= 1;
                            rs->direction[ index ] = 0;
                            rs->angle[ index ] = 0;
                            if( textured )
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
                    if( rs->angle[ index ] >= M_PI  )
                        {
                            rs->ypos[ index ] -= 1;
                            rs->direction[ index ] = 0;
                            rs->angle[ index ] = 0;
                            if( textured )
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
randsheet_draw( randsheet *rs )
{
    int i, j, polys = 0;
    int index;

    /* for all moving squares ... */
    for( index = 0; index < numsquares; index++ )
        {
            /* set color */
            glColor3f( rs->color[ index*3 + 0 ],
                       rs->color[ index*3 + 1 ],
                       rs->color[ index*3 + 2 ] );
            /* find x and y position */
            i = rs->xpos[ index ];
            j = rs->ypos[ index ];
            glPushMatrix();
            switch( rs->direction[ index ] )
                {
                case 0:

                    /* not moving */
                    /* front */
                    glTranslatef( i, 0, j );
                    break;
                case 1:
                    glTranslatef( i+1, 0, j );
                    glRotatef( 180 - rs->angle[ index ]*180/M_PI, 0, 0, 1 );

                    break;
                case 2:
                    glTranslatef( i, 0, j+1 );
                    glRotatef( 180 - rs->angle[ index ]*180/M_PI, -1, 0, 0 );

                    break;
                case 3:
                    glTranslatef( i, 0, j );
                    glRotatef( rs->angle[ index ]*180/M_PI, 0, 0, 1 );
                    break;
                case 4:
                    glTranslatef( i, 0, j );
                    glRotatef( rs->angle[ index ]*180/M_PI, -1, 0, 0 );
                    break;
                default:
                    break;
                }
            polys += draw_sheet( rs->tex + index*4 );
            glPopMatrix();

        }
    return polys;
}

/**** END RANDSHEET_BAK FUNCTIONS ***/

XSCREENSAVER_MODULE ("FlipFlop", flipflop)

#endif /* USE_GL */
