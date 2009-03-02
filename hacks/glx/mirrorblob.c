/* mirrorblob  Copyright (c) 2003 Jon Dowdall <jon.dowdall@bigpond.com> */
/*
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
 * Revision History:
 * 23-Sep-2003:  jon.dowdall@bigpond.com  Created module "blob"
 * 19-Oct-2003:  jon.dowdall@bigpond.com  Added texturing
 * 21-Oct-2003:                           Renamed to mirrorblob
 * 10-Feb-2004:  jon.dowdall@bigpond.com  Added motion blur
 *
 * The mirrorblob screensaver draws a pulsing blob on the screen.  Options
 * include adding a background (via load_texture_async), texturing the blob,
 * making the blob semi-transparent and varying the resolution of the blob
 * tessellation.
 *
 * The blob was inspired by a lavalamp is in no way a simulation.  The code is
 * just an attempt to generate some eye-candy.
 *
 * Much of xmirrorblob code framework is taken from the pulsar module by David
 * Konerding and the glslideshow by Mike Oliphant and Jamie Zawinski.
 *
 */

#include <math.h>

#ifdef STANDALONE
#define DEFAULTS \
    "*delay:             " DEF_DELAY "\n" \
    "*showFPS:           " DEF_FPS   "\n" \
    "*useSHM:              True      \n"

# define refresh_mirrorblob 0
# define mirrorblob_handle_event 0
# include "xlockmore.h"    /* from the xmirrorblob distribution */
#else /* !STANDALONE */
# include "xlock.h"        /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */


#define DEF_DELAY            "10000"
#define DEF_FPS              "False"
#define DEF_WIRE             "False"
#define DEF_BLEND            "False"
#define DEF_FOG              "False"
#define DEF_ANTIALIAS        "False"
#define DEF_WALLS            "True"
#define DEF_COLOUR           "False"
#define DEF_TEXTURE          "True"
#define DEF_OFFSET_TEXTURE   "False"
#define DEF_PAINT_BACKGROUND "True"
#define DEF_X_RES            "60"
#define DEF_Y_RES            "32"
#define DEF_FIELD_POINTS     "5"
#define DEF_MOTION_BLUR      "0"
#define DEF_INCREMENTAL      "0"
#define DEF_HOLD_TIME        "30"
#define DEF_FADE_TIME        "5"

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#include "grab-ximage.h"

#undef countof
#define countof(x) (sizeof((x)) / sizeof((*x)))

#define PI  3.1415926535897

/* */
static int do_wire;
static int do_blend;
static int do_fog;
static int do_antialias;
static int do_walls;
static int do_texture;
static int do_paint_background;
static int do_colour;
static int offset_texture;
static int x_resolution;
static int y_resolution;
static int field_points;
static int motion_blur;
static int incremental;
static int fade_time;
static int hold_time;

static XrmOptionDescRec opts[] = {
    {"-wireframe",        ".blob.wire",             XrmoptionNoArg, "true" },
    {"+wireframe",        ".blob.wire",             XrmoptionNoArg, "false" },
    {"-blend",            ".blob.blend",            XrmoptionNoArg, "true" },
    {"+blend",            ".blob.blend",            XrmoptionNoArg, "false" },
    {"-fog",              ".blob.fog",              XrmoptionNoArg, "true" },
    {"+fog",              ".blob.fog",              XrmoptionNoArg, "false" },
    {"-antialias",        ".blob.antialias",        XrmoptionNoArg, "true" },
    {"+antialias",        ".blob.antialias",        XrmoptionNoArg, "false" },
    {"-walls",            ".blob.walls",            XrmoptionNoArg, "true" },
    {"+walls",            ".blob.walls",            XrmoptionNoArg, "false" },
    {"-texture",          ".blob.texture",          XrmoptionNoArg, "true" },
    {"+texture",          ".blob.texture",          XrmoptionNoArg, "false" },
    {"-colour",           ".blob.colour",           XrmoptionNoArg, "true" },
    {"+colour",           ".blob.colour",           XrmoptionNoArg, "false" },
    {"-offset_texture",   ".blob.offset_texture",   XrmoptionNoArg, "true" },
    {"+offset_texture",   ".blob.offset_texture",   XrmoptionNoArg, "false" },
    {"-paint_background", ".blob.paint_background", XrmoptionNoArg, "true" },
    {"+paint_background", ".blob.paint_background", XrmoptionNoArg, "false" },
    {"-x_res",            ".blob.x_res",            XrmoptionSepArg, NULL },
    {"-y_res",            ".blob.y_res",            XrmoptionSepArg, NULL },
    {"-field_points",     ".blob.field_points",     XrmoptionSepArg, NULL },
    {"-motion_blur",      ".blob.motion_blur",      XrmoptionSepArg, NULL },
    {"-incremental",      ".blob.incremental",      XrmoptionSepArg, NULL },
    {"-fade_time",        ".blob.fade_time",        XrmoptionSepArg, NULL },
    {"-hold_time",        ".blob.hold_time",        XrmoptionSepArg, NULL },
};

static argtype vars[] = {
    {&do_wire,      "wire",         "Wire",      DEF_WIRE,      t_Bool},
    {&do_blend,     "blend",        "Blend",     DEF_BLEND,     t_Bool},
    {&do_fog,       "fog",          "Fog",       DEF_FOG,       t_Bool},
    {&do_antialias, "antialias",    "Antialias", DEF_ANTIALIAS, t_Bool},
    {&do_walls,     "walls",        "Walls",     DEF_WALLS,     t_Bool},
    {&do_texture,   "texture",      "Texture",   DEF_TEXTURE,   t_Bool},
    {&do_colour,    "colour",       "Colour",    DEF_COLOUR,   t_Bool},
    {&offset_texture, "offset_texture","Offset_Texture", DEF_OFFSET_TEXTURE, t_Bool},
    {&do_paint_background,"paint_background","Paint_Background", DEF_PAINT_BACKGROUND, t_Bool},
    {&x_resolution, "x_res",        "X_Res",        DEF_X_RES,        t_Int},
    {&y_resolution, "y_res",        "Y_Res",        DEF_Y_RES,        t_Int},
    {&field_points, "field_points", "Field_Points", DEF_FIELD_POINTS, t_Int},
    {&motion_blur,  "motion_blur",  "Motion_Blur",  DEF_MOTION_BLUR,  t_Int},
    {&incremental,  "incremental",  "Incremental",  DEF_INCREMENTAL,  t_Int},
    {&fade_time,    "fade_time",    "Fade_Time",    DEF_FADE_TIME,    t_Int},
    {&hold_time,    "hold_time",    "Hold_Time",    DEF_HOLD_TIME,    t_Int},
};


static OptionStruct desc[] =
{
    {"-/+ wire", "whether to do use wireframe instead of filled (faster)"},
    {"-/+ blend", "whether to do enable blending (slower)"},
    {"-/+ fog", "whether to do enable fog (slower)"},
    {"-/+ antialias", "whether to do enable antialiased lines (slower)"},
    {"-/+ walls", "whether to add walls to the blob space (slower)"},
    {"-/+ texture", "whether to add a texture to the blob (slower)"},
    {"-/+ colour", "whether to colour the blob"},
    {"-/+ offset_texture", "whether to offset texture co-ordinates"},
    {"-/+ paint_background", "whether to display a background texture (slower)"},
    {"-x_res", "Blob resolution in x direction"},
    {"-y_res", "Blob resolution in y direction"},
    {"-field_points", "Number of field points used to disturb blob"},
    {"-motion_blur", "Fade blob images (higher number = faster fade)"},
    {"-incremental", "Field summation method"},
    {"-fade_time", "Number of frames to transistion to next image"},
    {"-hold_time", "Number of frames before next image"},
};

ENTRYPOINT ModeSpecOpt mirrorblob_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   mirrorblob_description =
{"mirrorblob", "init_mirrorblob", "draw_mirrorblob", "release_mirrorblob",
 "draw_mirrorblob", "init_mirrorblob", NULL, &mirrorblob_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "OpenGL mirrorblob", 0, NULL};
#endif

#define NUM_TEXTURES 2

/*****************************************************************************
 * Types used in blob code
 *****************************************************************************/

typedef struct
{
    GLdouble x, y;
} Vector2D;

typedef struct
{
    GLdouble x, y, z;
} Vector3D;

typedef struct
{
    GLubyte red, green, blue, alpha;
} Colour;

/* Data used for sphere tessellation */
typedef struct
{
    double cosyd, sinyd;

    /* Number of x points at each row of the blob */
    int num_x_points;
} Row_Data;

/* Structure to hold sphere distortion data */
typedef struct
{
    double cx, cy, cpower;
    double mx, my, mpower;
    double ax, ay, apower;
    double vx, vy, vpower;
    Vector3D pos;
} Field_Data;

typedef enum
{
  HOLDING=0,
  TRANSITIONING
} Frame_State;


/* structure for holding the mirrorblob data */
typedef struct {
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
  XColor fg, bg;
  double freak, v_freak;

  Row_Data *row_data;

  /* Parameters controlling the position of the blob as a whole */
  Vector3D blob_center;
  Vector3D blob_anchor;
  Vector3D blob_velocity;
  Vector3D blob_force;

  /* Count of the total number of points */
  int num_points;

  Vector3D *dots;
  Vector3D *normals;
  Colour   *colours;
  Vector2D *tex_coords;

  /* Pointer to the field function results */
  double *field, *wall_field;

  Field_Data *field_data;

  /* Use 2 textures to allow a gradual fade between images */
  int current_texture;

  /* Ratio of used texture size to total texture size */
  GLfloat tex_width[NUM_TEXTURES], tex_height[NUM_TEXTURES];
  GLuint textures[NUM_TEXTURES];

  Frame_State state;
  double state_start_time;

  int colour_cycle;

  Bool mipmap_p;
  Bool waiting_for_image_p;
  Bool first_image_p;

} mirrorblobstruct;

static mirrorblobstruct *Mirrorblob = NULL;

/******************************************************************************
 *
 * Returns the current time in seconds as a double.  Shamelessly borrowed from
 * glslideshow.
 *
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}

/******************************************************************************
 *
 * Change to the projection matrix and set our viewing volume.
 *
 */
static void
reset_projection(int width, int height)
{
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    gluPerspective (60.0, 1.0, 1.0, 1024.0 );
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
}

/****************************************************************************
 *
 * Load a texture.
 */

static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  mirrorblobstruct *mp = (mirrorblobstruct *) closure;
  GLint texid = -1;
  int texture_index = -1;
  int i;

  glGetIntegerv (GL_TEXTURE_BINDING_2D, &texid);
  if (texid < 0) abort();

  for (i = 0; i < NUM_TEXTURES; i++) {
    if (mp->textures[i] == texid) {
      texture_index = i;
      break;
    }
  }
  if (texture_index < 0) abort();

  mp->tex_width [texture_index] =  (GLfloat) image_width  / texture_width;
  mp->tex_height[texture_index] = -(GLfloat) image_height / texture_height;

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   (mp->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  mp->waiting_for_image_p = False;
  mp->first_image_p = True;
}


static void
grab_texture(ModeInfo *mi, int texture_index)
{
  mirrorblobstruct *mp = &Mirrorblob[MI_SCREEN(mi)];

  mp->waiting_for_image_p = True;
  mp->mipmap_p = True;
  load_texture_async (mi->xgwa.screen, mi->window,
                      *mp->glx_context, 0, 0, mp->mipmap_p, 
                      mp->textures[texture_index],
                      image_loaded_cb, mp);
}

/******************************************************************************
 *
 * Initialise the data used to calculate the blob shape.
 */
static void
initialize_gl(ModeInfo *mi, GLsizei width, GLsizei height)
{
    mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN(mi)];

    GLfloat fogColor[4] = { 0.1, 0.1, 0.1, 0.1 };
    /* Lighting values */
    GLfloat lightPos0[] = {500.0f, 100.0f, 200.0f, 1.0f };
    GLfloat whiteLight0[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat sourceLight0[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat specularLight0[] = { 0.7f, 0.6f, 0.3f, 1.0f };

    GLfloat lightPos1[] = {0.0f, -500.0f, 500.0f, 1.0f };
    GLfloat whiteLight1[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat sourceLight1[] = { 1.0f, 0.3f, 0.3f, 1.0f };
    GLfloat specularLight1[] = { 0.7f, 0.6f, 0.3f, 1.0f };

    GLfloat specref[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    /* Setup our viewport. */
    glViewport (0, 0, width, height ); 

    glEnable(GL_DEPTH_TEST);

    if (do_antialias)
    {
        do_blend = 1;
        glEnable(GL_LINE_SMOOTH);
    }

    /* The blend function is used for trasitioning between two images even when
     * blend is not selected.
     */
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (do_wire)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (do_fog)
    {
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fogColor);
        glFogf(GL_FOG_DENSITY, 0.35);
        glFogf(GL_FOG_START, 2.0);
        glFogf(GL_FOG_END, 10.0);
    }

    /* Our shading model--Gouraud (smooth). */
    glShadeModel (GL_SMOOTH);

    /* Culling. */
    glCullFace (GL_BACK);
    glEnable (GL_CULL_FACE);
    glEnable (GL_DEPTH_TEST);
    glFrontFace (GL_CCW);

    /* Set the clear color. */
    glClearColor( 0, 0, 0, 0 );

    glViewport( 0, 0, width, height );

    glLightfv (GL_LIGHT0, GL_AMBIENT, whiteLight0);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, sourceLight0);
    glLightfv (GL_LIGHT0, GL_SPECULAR, specularLight0);
    glLightfv (GL_LIGHT0, GL_POSITION, lightPos0);
    glEnable (GL_LIGHT0);
    glLightfv (GL_LIGHT1, GL_AMBIENT, whiteLight1);
    glLightfv (GL_LIGHT1, GL_DIFFUSE, sourceLight1);
    glLightfv (GL_LIGHT1, GL_SPECULAR, specularLight1);
    glLightfv (GL_LIGHT1, GL_POSITION, lightPos1);
    glEnable (GL_LIGHT1);
    glEnable (GL_LIGHTING);

    /* Enable color tracking */
    glEnable (GL_COLOR_MATERIAL);

    /* Set Material properties to follow glColor values */
    glColorMaterial (GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    /* Set all materials to have specular reflectivity */
    glMaterialfv (GL_FRONT, GL_SPECULAR, specref);
    glMateriali (GL_FRONT, GL_SHININESS, 64);

    glEnable (GL_NORMALIZE);

    /* Enable Arrays */
    if (do_texture)
    {
        glLightModeli (GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

        glEnable (GL_TEXTURE_2D);
        gp->current_texture = 0;
        glGenTextures (NUM_TEXTURES, gp->textures);
        grab_texture (mi, gp->current_texture);

        glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }

    if (do_colour)
    {
        glEnableClientState (GL_COLOR_ARRAY);
    }
    glEnableClientState (GL_NORMAL_ARRAY);
    glEnableClientState (GL_VERTEX_ARRAY);

    /* Clear the buffer since this is not done during a draw with motion blur */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/******************************************************************************
 *
 * Calculate the normal vector for a plane given three points in the plane.
 */
static void
calculate_normal (Vector3D point1,
                  Vector3D point2,
                  Vector3D point3,
                  Vector3D *normal)
{
    Vector3D vector1, vector2;
    double magnitude;

    vector1.x = point2.x - point1.x;
    vector1.y = point2.y - point1.y;
    vector1.z = point2.z - point1.z;

    vector2.x = point3.x - point2.x;
    vector2.y = point3.y - point2.y;
    vector2.z = point3.z - point2.z;

    (*normal).x = vector1.y * vector2.z - vector1.z * vector2.y;
    (*normal).y = vector1.z * vector2.x - vector1.x * vector2.z;
    (*normal).z = vector1.x * vector2.y - vector1.y * vector2.x;

    /* Adjust the normal to unit magnitude */
    magnitude = sqrt ((*normal).x * (*normal).x
                      + (*normal).y * (*normal).y
                      + (*normal).z * (*normal).z);

    /* Watch out for divide by zero/underflow */
    if (magnitude > 1e-300)
    {
        (*normal).x /= magnitude;
        (*normal).y /= magnitude;
        (*normal).z /= magnitude;
    }
}

/******************************************************************************
 *
 * Initialise the data required to draw the blob allocating the memory as
 * necessary.
 *
 * Return 0 on success.
 */
static int
initialise_blob(mirrorblobstruct *gp,
                int width, int height,
                int field_array_size)
{
    /* Loop variables */
    int x, y, i;
    double xd;

    gp->colour_cycle = 0;

    gp->row_data = (Row_Data *) malloc (y_resolution * sizeof (Row_Data));
    if (!gp->row_data)
    {
        fprintf(stderr, "Couldn't allocate row data buffer\n");
        return -1;
    }

    gp->field_data = (Field_Data *) malloc (field_points * sizeof (Field_Data));
    if (!gp->field_data)
    {
        fprintf(stderr, "Couldn't allocate field data buffer\n");
        return -1;
    }

    gp->field = (double *)malloc(field_array_size * sizeof(double));
    if (!gp->field)
    {
        fprintf(stderr, "Couldn't allocate field buffer\n");
        return -1;
    }

    gp->wall_field = (double *)malloc(field_array_size * sizeof(double));
    if (!gp->wall_field)
    {
        fprintf(stderr, "Couldn't allocate wall field buffer\n");
        return -1;
    }

    gp->dots = (Vector3D *)malloc(x_resolution * y_resolution * sizeof(Vector3D));
    if (!gp->dots)
    {
        fprintf(stderr, "Couldn't allocate points buffer\n");
        return -1;
    }
    glVertexPointer (3, GL_DOUBLE, 0, (GLvoid *) gp->dots);

    gp->normals = (Vector3D *)malloc(x_resolution * y_resolution * sizeof(Vector3D));
    if (!gp->normals)
    {
        fprintf(stderr, "Couldn't allocate normals buffer\n");
        return -1;
    }
    glNormalPointer (GL_DOUBLE, 0, (GLvoid *) gp->normals);

    if (do_colour)
    {
        gp->colours = (Colour *)malloc(x_resolution * y_resolution * sizeof(Colour));
        if (!gp->colours)
        {
            fprintf(stderr, "Couldn't allocate colours buffer\n");
            return -1;
        }
        glColorPointer (4, GL_UNSIGNED_BYTE, 0, (GLvoid *) gp->colours);
    }

    if (do_texture)
    {
        gp->tex_coords = (Vector2D *)malloc(x_resolution * y_resolution
                                        * sizeof(Vector2D));
        if (!gp->tex_coords)
        {
            fprintf(stderr, "Couldn't allocate tex_coords buffer\n");
            return -1;
        }
        glTexCoordPointer (2, GL_DOUBLE, 0, (GLvoid *) gp->tex_coords);
    }

    gp->num_points = 0;
    /* Generate constant row data and count of total number of points */
    for (y = 0; y < y_resolution; y++)
    {
        gp->row_data[y].cosyd = cos(PI * (double)(y * (y_resolution + 1))
                                / (double)(y_resolution * y_resolution));
        gp->row_data[y].sinyd = sin(PI * (double)(y * (y_resolution + 1))
                                / (double)(y_resolution * y_resolution));
        gp->row_data[y].num_x_points = (int)(x_resolution * gp->row_data[y].sinyd + 1.0);
        gp->num_points += gp->row_data[y].num_x_points;
    }

    /* Initialise field data */
    for (i = 0; i < field_points; i++)
    {
        gp->field_data[i].ax = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
        gp->field_data[i].ay = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
        gp->field_data[i].apower = (((double)random() / (double)RAND_MAX) - 0.5);

        gp->field_data[i].pos.x = 1.5 * sin(PI * gp->field_data[i].ay)
            * cos(PI *  gp->field_data[i].ax);
        gp->field_data[i].pos.y = 1.5 * cos(PI * gp->field_data[i].ay);
        gp->field_data[i].pos.z = 1.5 * sin(PI * gp->field_data[i].ay)
            * sin(PI *  gp->field_data[i].ax);

        gp->field_data[i].cx = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
        gp->field_data[i].cy = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
        gp->field_data[i].cpower = (((double)random() / (double)RAND_MAX) - 0.5);

        gp->field_data[i].vx = 0.0;
        gp->field_data[i].vy = 0.0;
        gp->field_data[i].vpower = 0.0;

        gp->field_data[i].mx = 0.003 * ((double)random() / (double)RAND_MAX);
        gp->field_data[i].my = 0.003 * ((double)random() / (double)RAND_MAX);
        gp->field_data[i].mpower = 0.003 * ((double)random() / (double)RAND_MAX);
    }

    /* Initialise lookup table of field strength */
    for (i = 0; i < field_array_size; i++)
    {
        xd = 2.0 * (((double)i / (double)field_array_size));

        xd = 3.0 * xd * xd * xd * xd;
        gp->field[i] = 0.4 / (field_points * (xd + 0.1));

        xd = 10.0 * (((double)i / (double)field_array_size));
        gp->wall_field[i] = 0.4 / (xd * xd * xd * xd + 1.0);
    }

    for (y = 0; y < y_resolution; y++)
    {
        for (x = 0; x < gp->row_data[y].num_x_points; x++)
        {
            i = x + y * x_resolution;
            xd = 2.0 * (((double)x / (double)gp->row_data[y].num_x_points) - 0.5);

            gp->dots[i].x = gp->row_data[y].sinyd * cos(PI * xd);
            gp->dots[i].y = gp->row_data[y].cosyd;
            gp->dots[i].z = gp->row_data[y].sinyd * sin(PI * xd);
            gp->normals[i].x = gp->row_data[y].sinyd * cos(PI * xd);
            gp->normals[i].y = gp->row_data[y].cosyd;
            gp->normals[i].z = gp->row_data[y].sinyd * sin(PI * xd);
            if (do_texture)
            {
                gp->tex_coords[i].x = 2.0 - 2.0 * x / (float) gp->row_data[y].num_x_points;
                gp->tex_coords[i].y = 1.0 - y / (float) y_resolution;
            }
        }
    }
    return 0;
}


/******************************************************************************
 *
 * Calculate the blob shape.
 */
static void
calc_blob(mirrorblobstruct *gp,
          int width, int height,
          int field_array_size,
          float limit,
          double fade)
{
    /* Loop variables */
    int x, y, i, index, index1, index2, index3;
    /* position of a point */
    double xd, yd, zd, offset_x, offset_y, offset_z;
    double strength, radius;
    double xdist, ydist, zdist;
    int dist;

    /* Color components */

    gp->colour_cycle++;

    /* Update position and strength of points used to distort the blob */
    for (i = 0; i < field_points; i++)
    {
        gp->field_data[i].vx += gp->field_data[i].mx*(gp->field_data[i].cx - gp->field_data[i].ax);
        gp->field_data[i].vy += gp->field_data[i].my*(gp->field_data[i].cy - gp->field_data[i].ay);
        gp->field_data[i].vpower += gp->field_data[i].mpower
            * (gp->field_data[i].cpower - gp->field_data[i].apower);

        gp->field_data[i].ax += 0.1 * gp->field_data[i].vx;
        gp->field_data[i].ay += 0.1 * gp->field_data[i].vy;
        gp->field_data[i].apower += 0.1 * gp->field_data[i].vpower;

        gp->field_data[i].pos.x = 1.0 * sin(PI * gp->field_data[i].ay)
            * cos(PI * gp->field_data[i].ax);
        gp->field_data[i].pos.y = 1.0 * cos(PI * gp->field_data[i].ay);
        gp->field_data[i].pos.z = 1.0 * sin(PI * gp->field_data[i].ay)
            * sin(PI * gp->field_data[i].ax);
    }

    gp->blob_force.x = 0.0;
    gp->blob_force.y = 0.0;
    gp->blob_force.z = 0.0;
    for (y = 0; y < y_resolution; y++)
    {
        for (x = 0; x < gp->row_data[y].num_x_points; x++)
        {
            index = x + y * x_resolution;
            xd = 2.0 * PI * (((double)x / (double)gp->row_data[y].num_x_points) - 0.5);

            radius = 1.0 + 0.0 * sin (xd * 10);

            zd = radius * gp->row_data[y].sinyd * sin(xd);
            xd = radius * gp->row_data[y].sinyd * cos(xd);
            yd = radius * gp->row_data[y].cosyd;

            gp->normals[index].x = xd;
            gp->normals[index].y = yd;
            gp->normals[index].z = zd;

            offset_x = 0.0;
            offset_y = 0.0;
            offset_z = 0.0;
            strength = 0.0;
            for ( i = 0; i < field_points; i++)
            {
                xdist = gp->field_data[i].pos.x - xd;
                ydist = gp->field_data[i].pos.y - yd;
                zdist = gp->field_data[i].pos.z - zd;
                dist = field_array_size * (xdist * xdist + ydist * ydist
                                           + zdist * zdist) * 0.1;

                strength += PI * gp->field_data[i].apower;

                if (dist < field_array_size)
                {
                    offset_x += xd * gp->field_data[i].apower * gp->field[dist];
                    offset_y += yd * gp->field_data[i].apower * gp->field[dist];
                    offset_z += zd * gp->field_data[i].apower * gp->field[dist];

                    gp->blob_force.x += 1.0 * xd * gp->field_data[i].apower * gp->field[dist];
                    gp->blob_force.y += 1.0 * yd * gp->field_data[i].apower * gp->field[dist];
                    gp->blob_force.z += 1.0 * zd * gp->field_data[i].apower * gp->field[dist];

                    strength *= 2.0 * gp->field[dist];
                }

                if (incremental)
                {
                    xd += offset_x * gp->freak * gp->freak;
                    yd += offset_y * gp->freak * gp->freak;
                    zd += offset_z * gp->freak * gp->freak;
                }
                if (incremental == 1)
                {
                    offset_x = 0.0;
                    offset_y = 0.0;
                    offset_z = 0.0;
                }
            }

            if (incremental < 3)
            {
                xd += offset_x;
                yd += offset_y;
                zd += offset_z;
            }
            xd += gp->blob_center.x;
            yd += gp->blob_center.y;
            zd += gp->blob_center.z;

            if (do_colour)
            {
                gp->colours[index].red = 128 + (int)(sin(strength + gp->colour_cycle * 0.01 + 2.0 * PI * x / gp->row_data[y].num_x_points) * 127.0);
                gp->colours[index].green = 128 + (int)(cos(strength + gp->colour_cycle * 0.025) * 127.0);
                gp->colours[index].blue = 128 + (int)(sin(strength + gp->colour_cycle * 0.03 + 2.0 * PI * y / y_resolution) * 127.0);
                gp->colours[index].alpha = (int)(255.0 * fade);
            }

            /* Add walls */
            if (do_walls)
            {
                if (zd < -limit) zd = -limit;
                if (zd > limit) zd = limit;

                dist = field_array_size * (zd + limit) * (zd + limit) * 0.5;
                if (dist < field_array_size)
                {
                    xd += (xd - gp->blob_center.x) * gp->wall_field[dist];
                    yd += (yd - gp->blob_center.y) * gp->wall_field[dist];
                    gp->blob_force.z += (zd + limit);
                }
                else
                {
                    dist = field_array_size * (zd - limit) * (zd - limit) * 0.5;
                    if (dist < field_array_size)
                    {
                        xd += (xd - gp->blob_center.x) * gp->wall_field[dist];
                        yd += (yd - gp->blob_center.y) * gp->wall_field[dist];
                        gp->blob_force.z -= (zd - limit);
                    }

                    if (yd < -limit) yd = -limit;
                    if (yd > limit) yd = limit;

                    dist = field_array_size * (yd + limit) * (yd + limit) * 0.5;
                    if (dist < field_array_size)
                    {
                        xd += (xd - gp->blob_center.x) * gp->wall_field[dist];
                        zd += (zd - gp->blob_center.z) * gp->wall_field[dist];
                        gp->blob_force.y += (yd + limit);
                    }
                    else
                    {
                        dist = field_array_size * (yd - limit) * (yd - limit) * 0.5;
                        if (dist < field_array_size)
                        {
                            xd += (xd - gp->blob_center.x) * gp->wall_field[dist];
                            zd += (zd - gp->blob_center.z) * gp->wall_field[dist];
                            gp->blob_force.y -= (yd - limit);
                        }
                    }

                    if (xd < -limit) xd = -limit;
                    if (xd > limit) xd = limit;

                    dist = field_array_size * (xd + limit) * (xd + limit) * 0.5;
                    if (dist < field_array_size)
                    {
                        yd += (yd - gp->blob_center.y) * gp->wall_field[dist];
                        zd += (zd - gp->blob_center.z) * gp->wall_field[dist];
                        gp->blob_force.x += (xd + limit);
                    }
                    else
                    {
                        dist = field_array_size * (xd - limit) * (xd - limit) * 0.5;
                        if (dist < field_array_size)
                        {
                            yd += (yd - gp->blob_center.y) * gp->wall_field[dist];
                            zd += (zd - gp->blob_center.z) * gp->wall_field[dist];
                            gp->blob_force.x -= (xd - limit);
                        }
                    }

                    if (yd < -limit) yd = -limit;
                    if (yd > limit) yd = limit;
                }
            }

            gp->dots[index].x = xd;
            gp->dots[index].y = yd;
            gp->dots[index].z = zd;
        }
    }

    /* Calculate the normals for each vertex and the texture mapping if required.
     * Although the code actually calculates the normal for one of the triangles
     * attached to a vertex rather than the vertex itself the results are not too
     * bad for with a reasonable number of verticies.
     */

    /* The first point is treated as a special case since the loop expects to use
     * points in the previous row to form the triangle.
     */
    index1 = 0;
    index2 = y * x_resolution;
    index3 = 1 + y * x_resolution;
    calculate_normal (gp->dots[index1], gp->dots[index2], gp->dots[index3], &gp->normals[index1]);
    if (do_texture)
    {
        if (offset_texture)
        {
            gp->tex_coords[index1].x = gp->dots[index1].x * 0.125 + 0.5
                * (1.0 + 0.25 * asin(gp->normals[index1].x) / (0.5 * PI));
            gp->tex_coords[index1].y = gp->dots[index1].y * 0.125 + 0.5
                * (1.0 + 0.25 * asin(gp->normals[index1].y) / (0.5 * PI));
        }
        else
        {
            gp->tex_coords[index1].x = 0.5 * (1.0 + (asin(gp->normals[index1].x)
                                                 / (0.5 * PI)));
            gp->tex_coords[index1].y = 0.5 * (1.0 + (asin(gp->normals[index1].y)
                                                 / (0.5 * PI)));
        }
        gp->tex_coords[index1].x *= gp->tex_width[gp->current_texture];
        gp->tex_coords[index1].y *= gp->tex_height[gp->current_texture];
    }

    for (y = 1; y < y_resolution - 1; y++)
    {
        if (gp->row_data[y - 1].num_x_points)
        {
            for (x = 0; x < gp->row_data[y].num_x_points; x++)
            {
                if (x == gp->row_data[y].num_x_points - 1)
                {
                    index1 = y * x_resolution;
                }
                else
                {
                    index1 = x + 1 + y * x_resolution;
                }
                index2 = x + y * x_resolution;
                index3 = ((x + 0.5) * gp->row_data[y - 1].num_x_points
                          / gp->row_data[y].num_x_points) + (y - 1) * x_resolution;
                calculate_normal (gp->dots[index1], gp->dots[index2], gp->dots[index3],
                                  &gp->normals[index1]);
                if (do_texture)
                {
                    if (offset_texture)
                    {
                        gp->tex_coords[index1].x = gp->dots[index1].x * 0.125 + 0.5
                            * (1.0 + 0.25 * asin(gp->normals[index1].x) / (0.5 * PI));
                        gp->tex_coords[index1].y = gp->dots[index1].y * 0.125 + 0.5
                            * (1.0 + 0.25 * asin(gp->normals[index1].y) / (0.5 * PI));
                    }
                    else
                    {
                        gp->tex_coords[index1].x = 0.5 * (1.0 + (asin(gp->normals[index1].x)
                                                             / (0.5 * PI)));
                        gp->tex_coords[index1].y = 0.5 * (1.0 + (asin(gp->normals[index1].y)
                                                             / (0.5 * PI)));
                    }
                    gp->tex_coords[index1].x *= gp->tex_width[gp->current_texture];
                    gp->tex_coords[index1].y *= gp->tex_height[gp->current_texture];
                }
            }
        }
    }
    index1 = (y_resolution - 1) * x_resolution;
    index2 = (y_resolution - 2) * x_resolution;
    index3 = 1 + (y_resolution - 2) * x_resolution;
    calculate_normal (gp->dots[index1], gp->dots[index2], gp->dots[index3], &gp->normals[index1]);
    if (do_texture)
    {
        if (offset_texture)
        {
            gp->tex_coords[index1].x = gp->dots[index1].x * 0.125 + 0.5
                * (1.0 + 0.25 * asin(gp->normals[index1].x) / (0.5 * PI));
            gp->tex_coords[index1].y = gp->dots[index1].y * 0.125 + 0.5
                * (1.0 + 0.25 * asin(gp->normals[index1].y) / (0.5 * PI));
        }
        else
        {
            gp->tex_coords[index1].x = 0.5 * (1.0 + (asin(gp->normals[index1].x)
                                                 / (0.5 * PI)));
            gp->tex_coords[index1].y = 0.5 * (1.0 + (asin(gp->normals[index1].y)
                                                 / (0.5 * PI)));
        }
        gp->tex_coords[index1].x *= gp->tex_width[gp->current_texture];
        gp->tex_coords[index1].y *= gp->tex_height[gp->current_texture];
    }


    gp->freak += gp->v_freak;
    gp->v_freak += -gp->freak / 2000000.0;

    /* Update the center of the whole blob */
    gp->blob_velocity.x += (gp->blob_anchor.x - gp->blob_center.x) / 80.0
        + 0.01 * gp->blob_force.x / gp->num_points;
    gp->blob_velocity.y += (gp->blob_anchor.y - gp->blob_center.y) / 80.0
        + 0.01 * gp->blob_force.y / gp->num_points;
    gp->blob_velocity.z += (gp->blob_anchor.z - gp->blob_center.z) / 80.0
        + 0.01 * gp->blob_force.z / gp->num_points;

    gp->blob_center.x += gp->blob_velocity.x * 0.5;
    gp->blob_center.y += gp->blob_velocity.y * 0.5;
    gp->blob_center.z += gp->blob_velocity.z * 0.5;

    gp->blob_velocity.x *= 0.99;
    gp->blob_velocity.y *= 0.99;
    gp->blob_velocity.z *= 0.99;
}

/******************************************************************************
 *
 * Draw the blob shape.
 *
 * The horrendous indexing to calculate the verticies that form a particular
 * traiangle is the result of the conversion of my first non-openGL version of
 * blob to this openGL version.  This may be tidied up when I finally playing
 * with the more interesting bits of the code.
 */
static void
draw_blob (mirrorblobstruct *gp)
{
    int x, y, x2, x3;
    int index1, index2, index3;
    int lower, upper;

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    /* Move down the z-axis. */
    glTranslatef (0.0, 0.0, -5.0 );

    for (y = 1; y < y_resolution; y++)
    {
        if (gp->row_data[y - 1].num_x_points)
        {
            for (x = 0; x < gp->row_data[y].num_x_points; x++)
            {
                glBegin (GL_TRIANGLES);
                if (x == gp->row_data[y].num_x_points - 1)
                {
                    index1 = y * x_resolution;
                }
                else
                {
                    index1 = x + 1 + y * x_resolution;
                }
                index2 = x + y * x_resolution;
                index3 = ((x + 0.5) * gp->row_data[y - 1].num_x_points
                          / gp->row_data[y].num_x_points) + (y - 1) * x_resolution;
                glArrayElement(index1);
                glArrayElement(index2);
                glArrayElement(index3);
                glEnd();

                lower = ((x - 0.5) * gp->row_data[y - 1].num_x_points
                         / (float)gp->row_data[y].num_x_points);
                upper = ((x + 0.5) * gp->row_data[y - 1].num_x_points
                         / (float)gp->row_data[y].num_x_points);

                if (upper > lower)
                {
                    glBegin (GL_TRIANGLE_FAN);
                    index1 = x + y * x_resolution;

                    for (x2 = lower; x2 <= upper; x2++)
                    {
                        x3 = x2;
                        while (x3 < 0) x3 += gp->row_data[y - 1].num_x_points;
                        while (x3 >= gp->row_data[y - 1].num_x_points)
                            x3 -= gp->row_data[y - 1].num_x_points;
                        index2 = x3 + (y - 1) * x_resolution;

                        if (x2 < upper)
                        {
                            x3 = x2 + 1;
                            while (x3 < 0) x3 += gp->row_data[y - 1].num_x_points;
                            while (x3 >= gp->row_data[y - 1].num_x_points)
                                x3 -= gp->row_data[y - 1].num_x_points;
                            index3 = x3 + (y - 1) * x_resolution;
                            if (x2 == lower)
                            {
                                glArrayElement(index1);
                            }
                        }
                        glArrayElement(index2);
                    }
                    glEnd ();
                }
            }
        }
    }
}

/******************************************************************************
 *
 * Draw the background image simply map a texture onto a full screen quad.
 */
static void
draw_background (ModeInfo *mi)
{
    mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN(mi)];

    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glEnable (GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    /* Reset the projection matrix to make it easier to get the size of the quad
     * correct
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glOrtho(0.0, MI_WIDTH(mi), MI_HEIGHT(mi), 0.0, -1000.0, 1000.0);

    glBegin (GL_QUADS);
    
    glTexCoord2f (0.0, 0.0);
    glVertex2i (0, 0);
    
    glTexCoord2f (0.0, -gp->tex_height[gp->current_texture]);
    glVertex2i (0, MI_HEIGHT(mi));

    glTexCoord2f (gp->tex_width[gp->current_texture], -gp->tex_height[gp->current_texture]);
    glVertex2i (MI_WIDTH(mi), MI_HEIGHT(mi));

    glTexCoord2f (gp->tex_width[gp->current_texture], 0.0);
    glVertex2i (MI_WIDTH(mi), 0);
    glEnd();

    glPopMatrix ();
    glMatrixMode (GL_MODELVIEW);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/******************************************************************************
 *
 * Update the scene.
 */
static GLvoid
draw_scene(ModeInfo * mi)
{
    mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN(mi)];

    double fade = 0.0;
    double current_time;
    check_gl_error ("draw_scene");

    glColor4d(1.0, 1.0, 1.0, 1.0);

    current_time = double_time();
    switch (gp->state)
    {
    case TRANSITIONING:
        fade = (current_time - gp->state_start_time) / fade_time;
        break;

    case HOLDING:
        fade = 0.0;
        break;
    }


    /* Set the correct texture, when transitioning this ensures that the first draw
     * is the original texture (which has the new texture drawn over it with decreasing
     * transparency)
     */
    if (do_texture)
    {
      glBindTexture (GL_TEXTURE_2D, gp->textures[gp->current_texture]);
    }

    if (do_paint_background && !do_wire)
    {
        glClear(GL_DEPTH_BUFFER_BIT);
        if (motion_blur)
        {
            glEnable (GL_BLEND);
            glColor4ub (255, 255, 255, motion_blur);
        }
        draw_background (mi);

        /* When transitioning between two images paint the new image over the old
         * image with a varying alpha value to get a smooth fade.
         */
        if (gp->state == TRANSITIONING)
        {
            glDisable (GL_DEPTH_TEST);
            glEnable (GL_BLEND);
            /* Select the texture to transition to */
            glBindTexture (GL_TEXTURE_2D, gp->textures[1 - gp->current_texture]);
            glColor4d (1.0, 1.0, 1.0, fade);

            draw_background (mi);

            /* Select the original texture to draw the blob */
            glBindTexture (GL_TEXTURE_2D, gp->textures[gp->current_texture]);
            glEnable (GL_DEPTH_TEST);
        }
        /* Clear the depth buffer bit so the backgound is behind the blob */
        glClear(GL_DEPTH_BUFFER_BIT);
    }
    else if (motion_blur)
    {
        glDisable (GL_DEPTH_TEST);
        glEnable (GL_BLEND);
        glColor4ub (0, 0, 0, motion_blur);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glRectd (-10.0, -10.0, 10.0, 10.0);
        if (do_wire)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        glEnable (GL_DEPTH_TEST);
        glClear (GL_DEPTH_BUFFER_BIT);
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (do_blend)
    {
        fade = fade * 0.5;
    }

    calc_blob(gp, MI_WIDTH(mi), MI_HEIGHT(mi), 1024, 2.5, fade);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    if (do_blend)
    {
        glEnable (GL_BLEND);
        if (do_colour)
        {
            glBlendFunc (GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        }
        else
        {
            glColor4d (1.0, 1.0, 1.0, 0.5 - fade);
        }
    }
    else
    {
        glDisable (GL_BLEND);
        glColor4d (1.0, 1.0, 1.0, 1.0);
    }
    draw_blob(gp);

    if (do_blend && do_colour)
    {
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    /* While transitioning draw a second blob twice with a modified alpha channel.
     * The trasitioning state machine is very crude, it simply counts frames
     * rather than elapsed time but it works.
     */
    if (do_texture && (hold_time > 0))
    {
        switch (gp->state)
        {
        case TRANSITIONING:
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable (GL_BLEND);
            /* Select the texture to transition to */
            glBindTexture (GL_TEXTURE_2D, gp->textures[1 - gp->current_texture]);
            glColor4d (1.0, 1.0, 1.0, fade);
            draw_blob (gp);

            if ((current_time - gp->state_start_time) > fade_time)
            {
                gp->state = HOLDING;
                gp->state_start_time = current_time;
                gp->current_texture = 1 - gp->current_texture;
            }
            break;

        case HOLDING:
            if ((current_time - gp->state_start_time) > hold_time)
            {
                grab_texture (mi, 1 - gp->current_texture);
                gp->state = TRANSITIONING;
                /* Get the time again rather than using the current time so
                 * that the time taken by the grab_texture function is not part
                 * of the fade time
                 */
                gp->state_start_time = double_time();
            }
            break;
        }
    }
}

/******************************************************************************
 *
 * XMirrorblob screen update entry
 */
ENTRYPOINT void
draw_mirrorblob(ModeInfo * mi)
{
    mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN(mi)];
    Display    *display = MI_DISPLAY(mi);
    Window      window = MI_WINDOW(mi);

    if (!gp->glx_context)
        return;

    /* Wait for the first image; for subsequent images, load them in the
       background while animating. */
    if (gp->waiting_for_image_p && gp->first_image_p)
      return;

    glXMakeCurrent(display, window, *(gp->glx_context));
    draw_scene(mi);
    if (mi->fps_p) do_fps (mi);
    glXSwapBuffers(display, window);
}

/******************************************************************************
 *
 * XMirrorblob screen resize entry
 */
ENTRYPOINT void
reshape_mirrorblob(ModeInfo *mi, int width, int height)
{
    glViewport( 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi) );
    reset_projection(width, height);
}

/******************************************************************************
 *
 * XMirrorblob initialise entry
 */
ENTRYPOINT void
init_mirrorblob(ModeInfo * mi)
{
    int screen = MI_SCREEN(mi);

    mirrorblobstruct *gp;

    if (Mirrorblob == NULL)
    {
        if ((Mirrorblob = (mirrorblobstruct *)
             calloc(MI_NUM_SCREENS(mi), sizeof (mirrorblobstruct))) == NULL)
        {
            return;
        }
    }
    gp = &Mirrorblob[screen];

    gp->window = MI_WINDOW(mi);
    if ((gp->glx_context = init_GL(mi)) != NULL)
    {
        reshape_mirrorblob(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
        initialize_gl(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    }
    else
    {
        MI_CLEARWINDOW(mi);
    }

    initialise_blob(gp, MI_WIDTH(mi), MI_HEIGHT(mi), 1024);
    gp->state_start_time = double_time();

    gp->freak = 0.0;
    gp->v_freak = 0.0007;
    gp->first_image_p = True;
}

/******************************************************************************
 *
 * XMirrorblob cleanup entry
 */
ENTRYPOINT void
release_mirrorblob(ModeInfo * mi)
{
  if (Mirrorblob != NULL) {
    int i;
    for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
      mirrorblobstruct *gp = &Mirrorblob[i];
      if (gp->row_data) free(gp->row_data);
      if (gp->field_data) free(gp->field_data);
      if (gp->colours) free(gp->colours);
      if (gp->tex_coords) free(gp->tex_coords);
      if (gp->dots) free(gp->dots);
      if (gp->wall_field) free(gp->wall_field);
      if (gp->field) free(gp->field);
    }

    free(Mirrorblob);
    Mirrorblob = NULL;
  }
  FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("MirrorBlob", mirrorblob)

#endif
