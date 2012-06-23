/* tangram, Copyright (c) 2005 Jeremy English <jhe@jeremyenglish.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.  No representations are made
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * Sun 10 July 2005 Changed the code that solves the puzzles.
 *                  Also, limited the palette and added names. 
 *
 * Wed 13 July 2005 Added option to turn off rotation.
 *                  Changed color and materials
 */


#define DEFAULTS	"*delay:	10000		 \n" \
			"*wireframe:    False         \n" \
			"*titleFont:  -*-helvetica-medium-r-normal-*-180-*\n" \
			"*titleFont2: -*-helvetica-medium-r-normal-*-120-*\n" \
			"*titleFont3: -*-helvetica-medium-r-normal-*-80-*\n"  \

# define refresh_tangram 0
# define release_tangram 0
# define tangram_handle_event 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#include <ctype.h>


#ifdef USE_GL                   /* whole file */

#include <time.h>
#include <math.h>
#include "tangram_shapes.h"
#include "glxfonts.h"

typedef struct {
    GLubyte r;
    GLubyte g;
    GLubyte b;
} color;

typedef struct {
    GLfloat x;
    GLfloat y;
    GLfloat z;
} coord;

typedef struct {
    coord crd;                  /* coordinates */
    GLint r;                    /* rotation */
    GLint fr;                   /* flip Rotate. Used to keep track during animation */
    GLint dl;                   /* display List */
    GLfloat dz;                 /* velocity */
    GLfloat ddz;                /* Acceleration */
    GLfloat solved;             /* shapes state */
    Bool up;                    /* Move up the z axis? */
} tangram_shape;

typedef struct {
    char *name;
    tangram_shape ts[7];
} puzzle;

typedef enum {
    no_shape = -1,
    small_triangle1 = 0,
    small_triangle2 = 1,
    medium_triangle = 2,
    large_triangle1 = 3,
    large_triangle2 = 4,
    square = 5,
    rhomboid = 6
} shape_type;

#define SPEED 0.03
enum {
    BOTTOM = 0,
    DEF_WAIT = 500,
    INIT_DZ = 2,
    NUM_SHAPES = 7
};

typedef struct {
    GLXContext *glx_context;
    tangram_shape tsm1, tsm2, tm, tlg1, tlg2, sq, rh;
    tangram_shape n_tsm1, n_tsm2, n_tm, n_tlg1, n_tlg2, n_sq, n_rh;
    char *puzzle_name;
    int csi;

    int ncolors;
    XColor *colors;
    int ccolor;

# ifdef HAVE_GLBITMAP
    XFontStruct *xfont1;
    XFontStruct *xfont2;
    XFontStruct *xfont3;
    GLuint font1_dlist, font2_dlist, font3_dlist;
# else
  texture_font_data *font1_data, *font2_data, *font3_data;
# endif

    GLuint name_list;

    GLfloat theta[3];
    Bool going_down[3];

    const char *pn;
    int display_counter;

} tangram_configuration;

static tangram_configuration *tps = NULL;

#define DEF_VIEWING_TIME "5"
#define DEF_ROTATE "True"
#define DEF_X_CAMERA_ROTATE "0.2"
#define DEF_Y_CAMERA_ROTATE "0.5"
#define DEF_Z_CAMERA_ROTATE "0"
#define DEF_LABELS "True"

static GLuint viewing_time;
static Bool do_rotate;
static Bool do_labels;
static GLfloat x_camera_rotate;
static GLfloat y_camera_rotate;
static GLfloat z_camera_rotate;
static int wire;

static XrmOptionDescRec opts[] = {
    {"-viewing_time", ".viewingTime", XrmoptionSepArg, 0},
    {"-rotate", ".rotate", XrmoptionNoArg, "True"},
    {"+rotate", ".rotate", XrmoptionNoArg, "False"},
    {"-labels", ".labels", XrmoptionNoArg, "True"},
    {"+labels", ".labels", XrmoptionNoArg, "False"},
    {"-x_camera_rotate", ".xCameraRotate", XrmoptionSepArg, 0},
    {"-y_camera_rotate", ".yCameraRotate", XrmoptionSepArg, 0},
    {"-z_camera_rotate", ".zCameraRotate", XrmoptionSepArg, 0}
};

static argtype vars[] = {
    {&viewing_time, "viewingTime", "ViewingTime", DEF_VIEWING_TIME, t_Int},
    {&do_rotate, "rotate", "Rotate", DEF_ROTATE, t_Bool},
    {&do_labels, "labels", "Labels", DEF_LABELS, t_Bool},
    {&x_camera_rotate, "xCameraRotate", "XCameraRotate", DEF_X_CAMERA_ROTATE, t_Float},
    {&y_camera_rotate, "yCameraRotate", "YCameraRotate", DEF_Y_CAMERA_ROTATE, t_Float},
    {&z_camera_rotate, "zCameraRotate", "ZCameraRotate", DEF_Z_CAMERA_ROTATE, t_Float}
};

ENTRYPOINT ModeSpecOpt tangram_opts = { countof(opts), opts, countof(vars), vars, NULL };

static const puzzle solved[] = {
  {"Teapot", {
      {{-1.664000, -1.552000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.696000, 0.944000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0,  False},
      {{0.064000, -2.128000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0,  True},
      {{-0.960000, -1.056000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.104000, 0.960000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.376000, -0.800000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.152000, 0.736000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Candle", {
      {{-0.016000, 2.176001, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.016000, 2.960001, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.000000, 0.400000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.015998, 2.208001, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{2.000001, 2.208001, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.496000, 0.432000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.016000, -0.672000, 0}, 335, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Square", {
      {{-0.048000, -0.016000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.704000, 0.736000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.488000, 1.424001, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.016000, -0.016000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.000000, 0.000000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.688000, 0.720000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.784000, 0.672000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Crane", {
      {{1.248001, 1.759999, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.024000, 3.071999, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.975999, -2.096001, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.480000, -1.968001, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.056000, -0.496000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.239999, -1.312001, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.223999, -1.360001, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Crane", {
      {{0.320000, 1.360000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.704000, 3.072000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.200000, -3.392000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.688000, -1.184000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.768000, 0.192000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.168000, -2.304000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.312000, 1.296000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Duck", {
      {{-1.391999, 1.424000, 0}, 65, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.768000, 2.000000, 0}, 99, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{2.688001, -1.872000, 0}, 270, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.343999, 0.944000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.112000, -0.464000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.984001, -1.120000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.536001, 0.912000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Pelican", {
      {{1.088000, 0.064001, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.864000, -1.279999, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.807999, 1.520000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{1.824001, -1.231998, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.368000, 1.472000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.832000, -2.271998, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.776001, 0.816000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Cat", {
      {{0.416000, -2.432000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.352000, -2.432000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.832000, -0.480000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.632000, 3.056000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.616000, 1.040000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{1.088000, -1.696000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.832000, -0.432000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Coi", {
      {{1.264000, -1.232000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.216000, 0.816000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.631999, 1.872000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.832000, 2.287999, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.608000, 0.912000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{2.240001, -0.176000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.256000, -1.200000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Man Skipping", {
      {{1.727998, 2.303998, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.120000, 3.376001, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.879998, -3.008001, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.072000, 2.559999, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.440000, 0.144000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.192001, -2.592001, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.015999, 0.176000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Old Man", {
      {{-0.400000, 1.744000, 0}, 58, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.704000, 0.128000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.656000, 0.320000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.112000, -0.384000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.096000, -0.399999, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.736000, 0.352000, 0}, 123, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.336000, 0.352000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Spear Head", {
      {{0.688000, -0.144000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.080000, 0.592000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.048000, 0.592000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.488000, -0.848000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.376000, -0.864000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.688000, -0.128000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.504000, -0.832000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Diamond", {
      {{0.624000, -1.776000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.144000, 0.432000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.800000, -0.272000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-2.320000, -0.304000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{2.048000, -0.320000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.112000, 0.480000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.832000, -0.320000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Arrow", {
      {{-2.048001, -1.232000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.112000, 0.943999, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.312001, -0.560000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.496000, 0.656000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.528000, 0.608000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.048001, -2.704000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.312001, -0.512000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Lady", {
      {{-0.720000, 3.440000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.912000, -1.072000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.736000, 3.440000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.720000, 1.984000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.672000, 0.544000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.192000, -3.840000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.528000, -2.480000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Running Man", {
      {{1.136000, 2.720000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.304001, 1.776000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.256000, 0.288000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.304000, 0.304000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.096000, -0.128000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.656000, -2.832000, 0}, 105, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.784000, -0.096000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Parallelogram", {
      {{-1.104000, -1.455999, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.912000, -0.447999, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.048000, -1.471999, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.136000, -1.439999, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.944000, 1.552000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.912000, 0.560000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.112000, 1.568000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"N", {
      {{-1.615999, 0.064000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.592000, 0.112000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.432000, 0.096000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.679999, -0.880000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.488001, 1.103999, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.640000, 0.112000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.392001, -0.928000, 0}, 270, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Farm House", {
      {{2.112000, 1.504000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.040000, 1.472000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.032000, -1.600000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.056000, 1.504000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.992000, -0.528000, 0}, 0, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{2.080000, 0.512000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.104000, 0.480000, 0}, 270, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Vulture", {
      {{0.912000, 1.728000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.623998, -1.040000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.992000, 1.104000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.944000, -0.288000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.448000, -1.760000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.887998, -0.368000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{3.008002, 2.160000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Swan", {
      {{0.720000, 0.352000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.672000, -1.568000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.376000, 1.104000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.151999, 1.488000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.320000, 2.096000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.656000, 0.304000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.624000, -2.559999, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"High Karate", {
      {{-0.144000, 2.576000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.696001, -2.432000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{2.176001, -0.400000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.624000, -0.512000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.576000, -1.152000, 0}, 0, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.919999, -1.376000, 0}, 303, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.448000, -0.096001, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Lazy", {
      {{-2.416000, 1.120000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.952000, -2.016000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.552000, -0.640000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.016000, 1.840000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.456000, -1.072000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.848000, -0.816000, 0}, 332, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.200000, -1.792000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Bat", {
      {{-0.304000, -0.352000, 0}, 259, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.304000, -1.344000, 0}, 105, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.312000, -1.024000, 0}, 300, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{1.952000, 0.240000, 0}, 195, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-2.272000, 0.096000, 0}, 11, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.112000, -1.056000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.560000, -1.344000, 0}, 281, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Sail Boat", {
      {{0.544000, 2.000000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.880000, 0.160000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.656000, -1.503999, 0}, 220, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.656000, -0.336000, 0}, 50, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.688000, -0.288000, 0}, 310, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.864000, 1.232000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.496000, 2.016001, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Glenda", {
      {{-2.016000, 2.080000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.240001, 1.824000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{2.239999, -0.752000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.264000, 2.784000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.248000, 0.736000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{0.240001, 0.304000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.544000, -2.976001, 0}, 149, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Cat", {
      {{1.376000, -1.536001, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.488000, -1.552001, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.352000, -0.048000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-2.144000, 2.415999, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.096000, 0.368000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{2.144000, -0.800000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.392000, -0.064000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Lying Cat", {
      {{2.480000, -0.912000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{2.592000, -0.928000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.352000, 1.280000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.688000, 0.336000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.808000, -0.112000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{3.248000, -0.176000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.472000, 1.024000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Witch", {
      {{-0.943999, -0.304000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.144000, 0.288000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.360000, -2.304000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.328000, -0.848000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.008000, 1.584000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.512000, 2.688000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.863999, -0.096000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Laugh", {
      {{0.703999, -0.160000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.064000, -0.400000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.767999, -1.408000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.232000, -1.328000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.040000, 2.624000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.288000, 1.264000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.760001, -1.408000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Standing Man", {
      {{0.272000, 3.392000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.144000, -1.328000, 0}, 331, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.216001, 0.272000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.736000, 0.208000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.432000, -3.440000, 0}, 151, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.720000, 2.320000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.263998, 0.272000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Walking Man", {
      {{-1.056000, -3.456000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.736000, 2.000000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.488000, 1.760000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.432000, 0.016000, 0}, 0, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.432000, -0.064000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.560000, -2.576000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.032000, 2.656000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Repose", {
      {{-2.800000, -2.304000, 0}, 101, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.888000, 2.032000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.856000, 2.016000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.352000, -0.144000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.848000, 0.976000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.424000, -1.104000, 0}, 236, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.792000, 2.016000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Shape", {
      {{1.263999, 1.600001, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.311999, -1.568000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.736000, 0.576000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.736000, -2.591999, 0}, 360, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.768000, 2.640001, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.784000, -0.528000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.736000, 0.496000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Shape", {
      {{-0.816000, 1.392000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.832000, -1.807999, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{1.216000, -0.752000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.232000, -2.815999, 0}, 270, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{1.248000, 2.400000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.864000, 1.392000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.240000, 1.328000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Lightning", {
      {{0.176000, -2.448000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.888000, 2.880000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.856000, 1.824000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.872000, -1.392000, 0}, 0, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{1.264000, -0.432000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{2.320001, -2.432000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.872000, 1.728000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"E", {
      {{0.928000, 1.664000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.896000, -1.519998, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.136000, 0.608000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.152000, -2.559998, 0}, 0, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.152000, 2.672002, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.960000, -0.384000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.136000, 0.528000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Dagger", {
      {{-0.096000, 0.448000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.640000, 2.656000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.064000, -3.104000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.767999, 1.184000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.080000, 0.416000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.416000, -2.064000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.112000, 3.328001, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Knight", {
      {{-0.368000, 0.400000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.871998, -1.808000, 0}, 225, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.056000, -0.368000, 0}, 0, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.056000, -1.840000, 0}, 45, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.352000, 1.440000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{0.128000, 0.432000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.119999, -1.120000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Candy", {
      {{-1.039999, 1.136000, 0}, 360, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.024000, 0.096000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.016000, 0.048000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.016000, -1.008000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.016000, 1.216000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{1.024000, 0.144000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.032000, 1.088000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"King", {
      {{-0.688000, 1.904000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.800000, 1.904000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.512000, -1.392000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{1.488000, 1.120000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.392000, 1.120000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.496000, -1.312000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.480000, -1.376000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Top", {
      {{-1.055999, -0.800000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.103999, 0.208000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{0.000000, -0.784000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.016000, 0.272000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.032000, 0.288000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{0.480000, -1.855999, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{2.096001, 0.224000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Dog", {
      {{-2.896000, -0.128000, 0}, 45, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.800000, 0.992000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-1.152000, -0.416000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.016000, 0.656000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.456000, -0.736000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{2.864000, 0.736000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.048000, 1.664000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, True},
    },
  },
  {"Moose Head", {
      {{2.944000, -0.288000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.936000, -0.224000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.112000, 1.808000, 0}, 315, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{-2.128000, 0.768000, 0}, 90, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{1.888000, 0.768000, 0}, 180, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.112000, -0.688000, 0}, 135, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-3.184000, -1.216000, 0}, 360, 180, 0, INIT_DZ, -SPEED, 0, False},
    },
  },
  {"Negative Square", {
      {{-1.520000, -0.624000, 0}, 270, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-1.520000, 0.480000, 0}, 180, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{2.352000, 0.480000, 0}, 225, 0, 0, INIT_DZ, -SPEED, 0, True},
      {{-0.080000, -3.040000, 0}, 315, 180, 0, INIT_DZ, -SPEED, 0, False},
      {{-0.096000, 2.944000, 0}, 135, 180, 0, INIT_DZ, -SPEED, 0, True},
      {{-2.528000, -0.576000, 0}, 90, 0, 0, INIT_DZ, -SPEED, 0, False},
      {{1.360000, -1.600000, 0}, 360, 180, INIT_DZ, -SPEED, 0, True},
    }}
};


static void get_solved_puzzle(ModeInfo * mi,
                              tangram_shape * tsm1, tangram_shape * tsm2,
                              tangram_shape * tm, tangram_shape * tlg1,
                              tangram_shape * tlg2, tangram_shape * sq,
                              tangram_shape * rh)
{
    tangram_configuration *tp = &tps[MI_SCREEN(mi)];
    int sz = sizeof(solved) / sizeof(solved[0]);
    int r;

    /* we don't want to see the same puzzle twice */
    do {
        r = random() % sz;
    } while (r == tp->csi);
    tp->csi = r;

    *tsm1 = solved[r].ts[small_triangle1];
    *tsm2 = solved[r].ts[small_triangle2];
    *tm = solved[r].ts[medium_triangle];
    *tlg1 = solved[r].ts[large_triangle1];
    *tlg2 = solved[r].ts[large_triangle2];
    *sq = solved[r].ts[square];
    *rh = solved[r].ts[rhomboid];

    tp->puzzle_name = solved[r].name;
}

static int approach_number(int goal, int current, int step)
{

    int i = 0;

    if (goal > current) {
        while (i < step) {
            current++;
            if (goal <= current)
                break;
            i++;
        }
    } else if (goal < current) {
        while (i < step) {
            current--;
            if (goal >= current)
                break;
            i++;
        }
    }

    return current;
}

/* gt - floating point greater than comparison */
static Bool gt(GLfloat x1, GLfloat x2, GLfloat per)
{
    if ((x1 > x2) && (fabs(x1 - x2) > per))
        return True;
    else
        return False;
}

/* lt - floating point less than comparison */
static Bool lt(GLfloat x1, GLfloat x2, GLfloat per)
{
    if ((x1 < x2) && (fabs(x1 - x2) > per))
        return True;
    else
        return False;
}

static GLfloat approach_float(GLfloat goal, GLfloat current,
                              Bool * changed, GLfloat per)
{
    *changed = False;
    if (gt(goal, current, per)) {
        current += per;
        *changed = True;
    } else if (lt(goal, current, per)) {
        current -= per;
        *changed = True;
    }
    return current;
}

#if 0
static void print_shape(char *s, tangram_shape sh)
{
    fprintf(stderr, "%s\n", s);
    fprintf(stderr, "(%f, %f, %f)\n", sh.crd.x, sh.crd.y, sh.crd.z);
    fprintf(stderr, "%d\n", sh.r);
    fprintf(stderr, "%d\n", sh.fr);
    fprintf(stderr, "\n");
}
#endif


static void reset_shape(tangram_shape * ts)
{
    GLfloat r = random() % 10;
    GLfloat f = r / 10;
    ts->crd.z = BOTTOM;
    ts->dz = INIT_DZ + f;
    ts->ddz = -SPEED;
}

static void bounce(tangram_shape * ts)
{
    ts->crd.z *= -1;            /* ignore this */
    ts->dz += ts->ddz;
    ts->crd.z += ts->dz * SPEED;
    if (ts->crd.z < BOTTOM) {
        reset_shape(ts);
    }

    ts->crd.z *= -1;            /* ignore this */
}

static void draw_tangram_shape(tangram_shape ts)
{
    glPushMatrix();

    if (!do_rotate) {
        ts.up = True;
    }

    glTranslatef(ts.crd.x, ts.crd.y, ts.up ? ts.crd.z : -ts.crd.z);
    glRotated(90, 1, 0, 0);
    glRotated(ts.fr, 1, 0, 0);
    glRotated(ts.r, 0, 1, 0);
    glCallList(ts.dl);
    glPopMatrix();
}

static void load_fonts(ModeInfo * mi)
{
    tangram_configuration *tp = &tps[MI_SCREEN(mi)];
# ifdef HAVE_GLBITMAP
    load_font(mi->dpy, "titleFont", &tp->xfont1, &tp->font1_dlist);
    load_font(mi->dpy, "titleFont2", &tp->xfont2, &tp->font2_dlist);
    load_font(mi->dpy, "titleFont3", &tp->xfont3, &tp->font3_dlist);
# else /* !HAVE_GLBITMAP */
    tp->font1_data = load_texture_font (mi->dpy, "titleFont");
    tp->font2_data = load_texture_font (mi->dpy, "titleFont2");
    tp->font3_data = load_texture_font (mi->dpy, "titleFont3");
# endif /* !HAVE_GLBITMAP */
}

static void draw_shapes(ModeInfo * mi)
{
    tangram_configuration *tp = &tps[MI_SCREEN(mi)];

    draw_tangram_shape(tp->tsm1);

    draw_tangram_shape(tp->tsm2);
    draw_tangram_shape(tp->tm);
    draw_tangram_shape(tp->tlg1);
    draw_tangram_shape(tp->tlg2);
    draw_tangram_shape(tp->sq);
    draw_tangram_shape(tp->rh);
    if (do_labels) glCallList(tp->name_list);
}

static void set_perspective(void)
{
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, -1, 0.1, 50);
    gluLookAt(0, 5, -5, 0, 0, 0, 0, -1, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

}

ENTRYPOINT void reshape_tangram(ModeInfo * mi, int w, int h)
{
    glViewport(0, 0, w, h);
    set_perspective();
    glLoadIdentity();
}

static void set_camera(tangram_configuration *tp)
{
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, -1, 0.1, 50);


    gluLookAt(0, 5, -5, 0, 0, 0, 0, -1, 0);

    if (do_rotate) {
        glRotatef(tp->theta[0], 1, 0, 0);
        glRotatef(tp->theta[1], 0, 1, 0);
        glRotatef(tp->theta[2], 0, 0, 1);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();


    if (tp->going_down[0] && tp->theta[0] < 0) {

        tp->going_down[0] = False;
    } else if ((!tp->going_down[0]) && tp->theta[0] > 90) {

        tp->going_down[0] = True;
    }

    if (tp->theta[1] > 360.0)
        tp->theta[1] -= 360.0;

    if (tp->theta[2] > 360.0)
        tp->theta[2] -= 360.0;

    if (tp->going_down[0])
      tp->theta[0] -= x_camera_rotate;
    else
      tp->theta[0] += x_camera_rotate;

    tp->theta[1] += y_camera_rotate;
    tp->theta[2] += z_camera_rotate;
}

static void init_shapes(ModeInfo * mi)
{
    int wire = MI_IS_WIREFRAME(mi);
    tangram_configuration *tp = &tps[MI_SCREEN(mi)];
    get_solved_puzzle(mi, &tp->tsm1, &tp->tsm2, &tp->tm, &tp->tlg1,
                      &tp->tlg2, &tp->sq, &tp->rh);
    get_solved_puzzle(mi, &tp->n_tsm1, &tp->n_tsm2, &tp->n_tm, &tp->n_tlg1,
                      &tp->n_tlg2, &tp->n_sq, &tp->n_rh);
    tp->tsm1.dl = get_sm_tri_dl(wire);
    tp->tsm2.dl = get_sm_tri_dl(wire);
    tp->tm.dl = get_md_tri_dl(wire);
    tp->tlg1.dl = get_lg_tri_dl(wire);
    tp->tlg2.dl = get_lg_tri_dl(wire);
    tp->sq.dl = get_square_dl(wire);
    tp->rh.dl = get_rhomboid_dl(wire);
}

static void gl_init(ModeInfo * mi)
{

    int wire = MI_IS_WIREFRAME(mi);

    GLfloat y = do_rotate ? -10 : 3;
    GLfloat x = do_rotate ? 5 : 10;
    GLfloat pos[4] = { 0, 0, -5, 1.00 };
    GLfloat pos2[4] = { 0, 0, 5, 1.00 };
    GLfloat dif2[4] = { 1, 1, 1, 1 };

    pos[0] = -x;
    pos[1] = y;

    pos2[1] = x;
    pos2[1] = y;

    if (!wire) {
        glEnable(GL_LIGHTING);
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glEnable(GL_LIGHT0);
        if (do_rotate) {
            glLightfv(GL_LIGHT1, GL_POSITION, pos2);
            glLightfv(GL_LIGHT1, GL_DIFFUSE, dif2);
            glEnable(GL_LIGHT1);
        }
        glEnable(GL_DEPTH_TEST);
    }

}

ENTRYPOINT void init_tangram(ModeInfo * mi)
{
    tangram_configuration *tp;

    if (!tps) {
        tps = (tangram_configuration *)
            calloc(MI_NUM_SCREENS(mi), sizeof(tangram_configuration));
        if (!tps) {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
    }

    tp = &tps[MI_SCREEN(mi)];

    if ((tp->glx_context = init_GL(mi)) != NULL) {
        gl_init(mi);
    }

    wire = MI_IS_WIREFRAME(mi);

    tp->name_list = glGenLists(1);

    load_fonts(mi);
    init_shapes(mi);

    tp->theta[0] = tp->theta[1] = tp->theta[2] = 1;

}

static Bool all_solved(tangram_shape * ls[])
{
    int i;
    Bool b = True;
    for (i = 0; i < NUM_SHAPES; i++) {
        b = (b && ls[i]->solved);
    }
    return b;
}

static void solve(tangram_shape * new_s, tangram_shape * old_s)
{
    Bool moved_x, moved_y, moved_r, moved_fr, z_ok;

    old_s->fr = approach_number(new_s->fr, old_s->fr, 2);
    moved_fr = (old_s->fr != new_s->fr);

    old_s->r = approach_number(new_s->r, old_s->r, 2);
    moved_r = (old_s->r != new_s->r);

    old_s->crd.x =
        approach_float(new_s->crd.x, old_s->crd.x, &moved_x, 0.1);
    if (!moved_x)
        old_s->crd.x = new_s->crd.x;

    old_s->crd.y =
        approach_float(new_s->crd.y, old_s->crd.y, &moved_y, 0.1);
    if (!moved_y)
        old_s->crd.y = new_s->crd.y;

    z_ok = (-old_s->crd.z <= BOTTOM);

    old_s->solved = (moved_x == False && moved_y == False &&
                     moved_r == False && moved_fr == False &&
                     z_ok == True);
}

static void set_not_solved(tangram_shape * ls[])
{
    int i;
    for (i = 0; i < NUM_SHAPES; i++)
        ls[i]->solved = False;
}


ENTRYPOINT void draw_tangram(ModeInfo * mi)
{
    Display *dpy = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
    tangram_configuration *tp = &tps[MI_SCREEN(mi)];

    tangram_shape *ls[NUM_SHAPES];
    tangram_shape *nls[NUM_SHAPES];


    int i;
    int MAX_DISPLAY;

    GLfloat color[4] = { 0.0, 0.0, 0.0, 1.0 };
    GLfloat white[4] = { 1.0, 1.0, 1.0, 1.0 };
    MAX_DISPLAY = viewing_time * 100;

    if (! tp->glx_context)
      return;
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(tp->glx_context));

    ls[small_triangle1] = &tp->tsm1;
    ls[small_triangle2] = &tp->tsm2;
    ls[medium_triangle] = &tp->tm;
    ls[large_triangle1] = &tp->tlg1;
    ls[large_triangle2] = &tp->tlg2;
    ls[square] = &tp->sq;
    ls[rhomboid] = &tp->rh;

    nls[small_triangle1] = &tp->n_tsm1;
    nls[small_triangle2] = &tp->n_tsm2;
    nls[medium_triangle] = &tp->n_tm;
    nls[large_triangle1] = &tp->n_tlg1;
    nls[large_triangle2] = &tp->n_tlg2;
    nls[square] = &tp->n_sq;
    nls[rhomboid] = &tp->n_rh;

    set_camera(tp);

    if (tp->display_counter <= 0) {
        for (i = 0; i < NUM_SHAPES; i++) {
            if (ls[i]->solved) {
                if (all_solved(ls)) {
                    tp->display_counter = MAX_DISPLAY;
                    tp->pn = tp->puzzle_name;
                    get_solved_puzzle(mi, nls[small_triangle1],
                                      nls[small_triangle2],
                                      nls[medium_triangle],
                                      nls[large_triangle1],
                                      nls[large_triangle2], nls[square],
                                      nls[rhomboid]);
                    tp->ncolors = 128;
                    tp->colors =
                        (XColor *) calloc(tp->ncolors, sizeof(XColor));

                    make_random_colormap(0, 0, 0,
                                         tp->colors, &tp->ncolors,
                                         True, False, 0, False);


                    color[0] = tp->colors[0].red / 65536.0;
                    color[1] = tp->colors[1].green / 65536.0;
                    color[2] = tp->colors[2].blue / 65536.0;


                    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                                 color);

                    set_not_solved(ls);
                    break;
                }
            } else {
                tp->pn = "";
                bounce(ls[i]);
                solve(nls[i], ls[i]);
            }
        }
    } else {
        tp->display_counter--;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();

    glLoadIdentity();
    if (do_labels)
      {
# ifdef HAVE_GLBITMAP
        XFontStruct *f;
        GLuint fl;
# else /* !HAVE_GLBITMAP */
        texture_font_data *f;
# endif /* !HAVE_GLBITMAP */
        if (MI_WIDTH(mi) >= 500 && MI_HEIGHT(mi) >= 375)
# ifdef HAVE_GLBITMAP
            f = tp->xfont1, fl = tp->font1_dlist;
# else /* !HAVE_GLBITMAP */
            f = tp->font1_data;
# endif /* !HAVE_GLBITMAP */
        else if (MI_WIDTH(mi) >= 350 && MI_HEIGHT(mi) >= 260)
# ifdef HAVE_GLBITMAP
            f = tp->xfont2, fl = tp->font2_dlist;
# else /* !HAVE_GLBITMAP */
            f = tp->font2_data;
# endif /* !HAVE_GLBITMAP */
        else
# ifdef HAVE_GLBITMAP
            f = tp->xfont3, fl = tp->font3_dlist;
# else /* !HAVE_GLBITMAP */
            f = tp->font3_data;
# endif /* !HAVE_GLBITMAP */

        glNewList(tp->name_list, GL_COMPILE);
        glColor3f(0.8, 0.8, 0);
        print_gl_string(mi->dpy, f,
# ifdef HAVE_GLBITMAP
                        fl,
# endif /* !HAVE_GLBITMAP */
                        mi->xgwa.width, mi->xgwa.height,
                        10, mi->xgwa.height - 10, tp->pn, False);
        glEndList();
      }

    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 128);

    draw_shapes(mi);

    if (mi->fps_p) do_fps (mi);

    glFlush();
    glPopMatrix();
    glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE ("Tangram", tangram)

#endif                          /* USE_GL */
