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
 * 28-Jan-2006:  jon.dowdall@bigpond.com  Big clean up and bug fixes
 * 13-Apr-2009:  jon.dowdall@gmail.com    Fixed Mac version
 *
 * The mirrorblob screensaver draws a pulsing blob on the screen.  Options
 * include adding a background (via screen_to_texture), texturing the blob,
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
#define DEFAULTS "*delay:             " DEF_DELAY "\n"                      \
                 "*showFPS:           " DEF_FPS   "\n"                      \
                 "*useSHM:              True       \n"                      \
                 "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n" \
                 "*grabDesktopImages:   True  \n"                           \
                 "*chooseRandomImages:  True  \n"                           \
		 "*suppressRotationAnimation: True\n"                       \

# define refresh_mirrorblob 0
# include "xlockmore.h"
#else /* !STANDALONE */
# include "xlock.h"        /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */


#define DEF_DELAY            "10000"
#define DEF_FPS              "False"
#define DEF_WIRE             "False"
#define DEF_BLEND            "1.0"
#define DEF_FOG              "False"
#define DEF_ANTIALIAS        "False"
#define DEF_WALLS            "False"
#define DEF_COLOUR           "False"
#define DEF_ASYNC            "True"
#define DEF_TEXTURE          "True"
#define DEF_OFFSET_TEXTURE   "False"
#define DEF_PAINT_BACKGROUND "True"
#define DEF_RESOLUTION       "30"
#define DEF_BUMPS            "10"
#define DEF_MOTION_BLUR      "0.0"
#define DEF_INCREMENTAL      "0"
#define DEF_HOLD_TIME        "30.0"
#define DEF_FADE_TIME        "5.0"
#define DEF_ZOOM             "1.0"

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#include "gltrackball.h"
#include "grab-ximage.h"

#undef countof
#define countof(x) (sizeof((x)) / sizeof((*x)))

#define PI  3.1415926535897

/* Options from command line */
static GLfloat blend;
static Bool wireframe;
static Bool do_fog;
static Bool do_antialias;
static Bool do_walls;
static Bool do_texture;
static Bool do_paint_background;
static Bool do_colour;
static Bool offset_texture;
static int resolution;
static int bumps;
static float motion_blur;
static float fade_time;
static float hold_time;
static float zoom;

/* Internal parameters based on supplied options */
static Bool culling;
static Bool load_textures;

static XrmOptionDescRec opts[] = {
    {"-wire",             ".blob.wire",             XrmoptionNoArg, "true" },
    {"+wire",             ".blob.wire",             XrmoptionNoArg, "false" },
    {"-blend",            ".blob.blend",            XrmoptionSepArg, 0 },
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
    {"-offset-texture",   ".blob.offsetTexture",    XrmoptionNoArg, "true" },
    {"+offset-texture",   ".blob.offsetTexture",    XrmoptionNoArg, "false" },
    {"-paint-background", ".blob.paintBackground",  XrmoptionNoArg, "true" },
    {"+paint-background", ".blob.paintBackground",  XrmoptionNoArg, "false" },
    {"-resolution",       ".blob.resolution",       XrmoptionSepArg, NULL },
    {"-bumps",            ".blob.bumps",            XrmoptionSepArg, NULL },
    {"-motion-blur",      ".blob.motionBlur",       XrmoptionSepArg, 0 },
    {"-fade-time",        ".blob.fadeTime",         XrmoptionSepArg, 0 },
    {"-hold-time",        ".blob.holdTime",         XrmoptionSepArg, 0 },
    {"-zoom",             ".blob.zoom",             XrmoptionSepArg, 0 },
};

static argtype vars[] = {
    {&wireframe,    "wire",         "Wire",      DEF_WIRE,      t_Bool},
    {&blend,        "blend",        "Blend",     DEF_BLEND,     t_Float},
    {&do_fog,       "fog",          "Fog",       DEF_FOG,       t_Bool},
    {&do_antialias, "antialias",    "Antialias", DEF_ANTIALIAS, t_Bool},
    {&do_walls,     "walls",        "Walls",     DEF_WALLS,     t_Bool},
    {&do_texture,   "texture",      "Texture",   DEF_TEXTURE,   t_Bool},
    {&do_colour,    "colour",       "Colour",    DEF_COLOUR,   t_Bool},
    {&offset_texture, "offsetTexture","OffsetTexture", DEF_OFFSET_TEXTURE, t_Bool},
    {&do_paint_background,"paintBackground","PaintBackground", DEF_PAINT_BACKGROUND, t_Bool},
    {&resolution,   "resolution",   "Resolution",   DEF_RESOLUTION,   t_Int},
    {&bumps,        "bumps",        "Bump",         DEF_BUMPS, t_Int},
    {&motion_blur,  "motionBlur",   "MotionBlur",   DEF_MOTION_BLUR,  t_Float},
    {&fade_time,    "fadeTime",     "FadeTime",     DEF_FADE_TIME,    t_Float},
    {&hold_time,    "holdTime",     "HoldTime",     DEF_HOLD_TIME,    t_Float},
    {&zoom,         "zoom",         "Zoom",         DEF_ZOOM,         t_Float},
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
    {"-resolution", "Resolution of blob tesselation"},
    {"-bumps", "Number of bumps used to disturb blob"},
    {"-motion_blur", "Fade blob images (higher number = faster fade)"},
    {"-fade_time", "Number of frames to transistion to next image"},
    {"-hold_time", "Number of frames before next image"},
};

ENTRYPOINT ModeSpecOpt mirrorblob_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   mirrorblob_description =
{"mirrorblob", "init_mirrorblob", "draw_mirrorblob", "release_mirrorblob",
 "draw_mirrorblob", "init_mirrorblob", "handle_event", &mirrorblob_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "OpenGL mirrorblob", 0, NULL};
#endif

/*****************************************************************************
 * Types used in blob code
 *****************************************************************************/

typedef struct
{
  GLfloat x, y;
} Vector2D;

typedef struct
{
  GLfloat x, y, z;
} Vector3D;

typedef struct
{
  GLfloat w;
  GLfloat x;
  GLfloat y;
  GLfloat z;
} Quaternion;

typedef struct
{
  GLubyte red, green, blue, alpha;
} Colour;

typedef struct
{
  Vector3D initial_position;
  Vector3D position;
  Vector3D normal;
} Node_Data;

typedef struct
{
  int node1, node2, node3;
  Vector3D normal;
  double length1, length2, length3;
} Face_Data;

/* Structure to hold data about bumps used to distortion sphere */
typedef struct
{
  double cx, cy, cpower, csize;
  double ax, ay, power, size;
  double mx, my, mpower, msize;
  double vx, vy, vpower, vsize;
  Vector3D pos;
} Bump_Data;

/* Vertices of a tetrahedron */
#define sqrt_3 0.5773502692
/*#undef sqrt_3
#define sqrt_3 1.0*/
#define PPP {  sqrt_3,  sqrt_3,  sqrt_3 }       /* +X, +Y, +Z */
#define MMP { -sqrt_3, -sqrt_3,  sqrt_3 }       /* -X, -Y, +Z */
#define MPM { -sqrt_3,  sqrt_3, -sqrt_3 }       /* -X, +Y, -Z */
#define PMM {  sqrt_3, -sqrt_3, -sqrt_3 }       /* +X, -Y, -Z */

/* Structure describing a tetrahedron */
static Vector3D tetrahedron[4][3] = {
    {PPP, MMP, MPM},
    {PMM, MPM, MMP},
    {PPP, MPM, PMM},
    {PPP, PMM, MMP}
};

/*****************************************************************************
 * Static blob data
 *****************************************************************************/

static const Vector3D zero_vector = { 0.0, 0.0, 0.0 };

/* Use 2 textures to allow a gradual fade between images */
#define NUM_TEXTURES 2
#define BUMP_ARRAY_SIZE 1024

typedef enum
{
  INITIALISING,
  HOLDING,
  LOADING,
  TRANSITIONING
} Frame_State;

/* structure for holding the mirrorblob data */
typedef struct {
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
  XColor fg, bg;

  /* Parameters controlling the position of the blob as a whole */
  Vector3D blob_center;
  Vector3D blob_anchor;
  Vector3D blob_velocity;
  Vector3D blob_force;

  /* Count of the total number of nodes and faces used to tesselate the blob */
  int num_nodes;
  int num_faces;

  Node_Data *nodes;
  Face_Data *faces;
  
  Vector3D *dots;
  Vector3D *normals;
  Colour   *colours;
  Vector2D *tex_coords;

  /* Pointer to the bump function results */
  double *bump_shape, *wall_shape;

  Bump_Data *bump_data;

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

  trackball_state *trackball;
  int button_down;

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

/******************************************************************************
 *
 * Calculate the dot product of two vectors u and v
 * Dot product u.v = |u||v|cos(theta)
 * Where theta = angle between u and v
 */
static inline double
dot (const Vector3D u, const Vector3D v)
{
  return (u.x * v.x) + (u.y * v.y) + (u.z * v.z);
}

/******************************************************************************
 *
 * Calculate the cross product of two vectors.
 * Gives a vector perpendicular to u and v with magnitude |u||v|sin(theta)
 * Where theta = angle between u and v
 */
static inline Vector3D
cross (const Vector3D u, const Vector3D v)
{
  Vector3D result;

  result.x = (u.y * v.z - u.z * v.y);
  result.y = (u.z * v.x - u.x * v.z);
  result.z = (u.x * v.y - u.y * v.x);

  return result;
}

/******************************************************************************
 *
 * Add vector v to vector u
 */
static inline void
add (Vector3D *u, const Vector3D v)
{
  u->x = u->x + v.x;
  u->y = u->y + v.y;
  u->z = u->z + v.z;
}

/******************************************************************************
 *
 * Subtract vector v from vector u
 */
static inline Vector3D
subtract (const Vector3D u, const Vector3D v)
{
  Vector3D result;

  result.x = u.x - v.x;
  result.y = u.y - v.y;
  result.z = u.z - v.z;

  return result;
}

/******************************************************************************
 *
 * multiply vector v by scalar s
 */
static inline Vector3D
scale (const Vector3D v, const double s)
{
  Vector3D result;
    
  result.x = v.x * s;
  result.y = v.y * s;
  result.z = v.z * s;
  return result;
}

/******************************************************************************
 *
 * normalise vector v
 */
static inline Vector3D
normalise (const Vector3D v)
{
  Vector3D result;
  double magnitude;

  magnitude = sqrt (dot(v, v));

  if (magnitude > 1e-300)
    {
      result = scale (v, 1.0 / magnitude);
    }
  else
    {
      printf("zero\n");
      result = zero_vector;
    }
  return result;
}

/******************************************************************************
 *
 * Calculate the transform matrix for the given quaternion
 */
static void
quaternion_transform (Quaternion q, GLfloat * transform)
{
  GLfloat x, y, z, w;
  x = q.x;
  y = q.y;
  z = q.z;
  w = q.w;

  transform[0] = (w * w) + (x * x) - (y * y) - (z * z);
  transform[1] = (2.0 * x * y) + (2.0 * w * z);
  transform[2] = (2.0 * x * z) - (2.0 * w * y);
  transform[3] = 0.0;

  transform[4] = (2.0 * x * y) - (2.0 * w * z);
  transform[5] = (w * w) - (x * x) + (y * y) - (z * z);
  transform[6] = (2.0 * y * z) + (2.0 * w * x);
  transform[7] = 0.0;

  transform[8] = (2.0 * x * z) + (2.0 * w * y);
  transform[9] = (2.0 * y * z) - (2.0 * w * x);
  transform[10] = (w * w) - (x * x) - (y * y) + (z * z);
  transform[11] = 0.0;

  transform[12] = 0.0;
  transform[13] = 0.0;
  transform[14] = 0.0;
  transform[15] = (w * w) + (x * x) + (y * y) + (z * z);
}

/******************************************************************************
 *
 * Apply a matrix transform to the given vector
 */
static inline Vector3D
vector_transform (Vector3D u, GLfloat * t)
{
  Vector3D result;

  result.x = (u.x * t[0] + u.y * t[4] + u.z * t[8] + 1.0 * t[12]);
  result.y = (u.x * t[1] + u.y * t[5] + u.z * t[9] + 1.0 * t[13]);
  result.z = (u.x * t[2] + u.y * t[6] + u.z * t[10] + 1.0 * t[14]);

  return result;
}

/******************************************************************************
 *
 * Return a node that is on an arc between node1 and node2, where distance
 * is the proportion of the distance from node1 to the total arc.
 */
static Vector3D
partial (Vector3D node1, Vector3D node2, double distance)
{
  Vector3D result;
  Vector3D rotation_axis;
  GLfloat transformation[16];
  double angle;
  Quaternion rotation;

  rotation_axis = normalise (cross (node1, node2));
  angle = acos (dot (node1, node2)) * distance;

  rotation.x = rotation_axis.x * sin (angle / 2.0);
  rotation.y = rotation_axis.y * sin (angle / 2.0);
  rotation.z = rotation_axis.z * sin (angle / 2.0);
  rotation.w = cos (angle / 2.0);

  quaternion_transform (rotation, transformation);

  result = vector_transform (node1, transformation);

  return result;
}

/****************************************************************************
 *
 * Callback indicating a texture has loaded
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

/* Load a new file into a texture
 */
static void
grab_texture(ModeInfo *mi, int texture_index)
{
  mirrorblobstruct *mp = &Mirrorblob[MI_SCREEN(mi)];
	
  {
    int w = (MI_WIDTH(mi)  / 2) - 1;
    int h = (MI_HEIGHT(mi) / 2) - 1;
    if (w <= 10) w = 10;
    if (h <= 10) h = 10;
	
    mp->waiting_for_image_p = True;
    mp->mipmap_p = True;
    load_texture_async (mi->xgwa.screen, mi->window,
                        *mp->glx_context, w, h, mp->mipmap_p, 
                        mp->textures[texture_index],
                        image_loaded_cb, mp);
  }
}

/******************************************************************************
 *
 * Generate internal parameters based on supplied options the parameters to
 * ensure they are consistant.
 */
static void
set_parameters(void)
{
# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  wireframe = 0;
# endif

  /* In wire frame mode do not draw a texture */
  if (wireframe)
    {
      do_texture = False;
      blend = 1.0;
    }
    
  /* Need to load textures if either the blob or the backgound has an image */
  if (do_texture || do_paint_background)
    {
      load_textures = True;
    }
  else
    {
      load_textures = False;
    }
    
  /* If theres no texture don't calculate co-ordinates. */
  if (!do_texture)
    {
      offset_texture = False;
    }
    
  culling = True;
}

/******************************************************************************
 *
 * Initialise the openGL state data.
 */
static void
initialize_gl(ModeInfo *mi, GLsizei width, GLsizei height)
{
  mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN(mi)];
    
  /* Lighting values */
  GLfloat ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f };

  GLfloat lightPos0[] = {500.0f, 100.0f, 200.0f, 1.0f };
  GLfloat whiteLight0[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  GLfloat sourceLight0[] = { 0.6f, 0.6f, 0.6f, 1.0f };
  GLfloat specularLight0[] = { 0.8f, 0.8f, 0.9f, 1.0f };

  GLfloat lightPos1[] = {-50.0f, -100.0f, 2500.0f, 1.0f };
  GLfloat whiteLight1[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  GLfloat sourceLight1[] = { 0.6f, 0.6f, 0.6f, 1.0f };
  GLfloat specularLight1[] = { 0.7f, 0.7f, 0.7f, 1.0f };

  GLfloat specref[] = { 1.0f, 1.0f, 1.0f, 1.0f };

  GLfloat fogColor[4] = { 0.4, 0.4, 0.5, 0.1 };

  /* Set the internal parameters based on the configuration settings */
  set_parameters();

  /* Set the viewport to the width and heigh of the window */
  glViewport (0, 0, width, height ); 

  if (do_antialias)
    {
      blend = 1.0;
      glEnable(GL_LINE_SMOOTH);
      glEnable(GL_POLYGON_SMOOTH);
    }

  /* The blend function is used for trasitioning between two images even when
   * blend is not selected.
   */
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 
  if (do_fog)
    {
      glEnable(GL_FOG);
      glFogfv(GL_FOG_COLOR, fogColor);
      glFogf(GL_FOG_DENSITY, 0.50);
      glFogf(GL_FOG_START, 15.0);
      glFogf(GL_FOG_END, 30.0);
    }

  /* Set the shading model to smooth (Gouraud shading). */
  glShadeModel (GL_SMOOTH);

  glLightModelfv (GL_LIGHT_MODEL_AMBIENT, ambientLight);
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
  glMateriali (GL_FRONT, GL_SHININESS, 32);

  /* Let GL implementation scale normal vectors. */
  glEnable (GL_NORMALIZE);

  /* Enable Arrays */
  if (load_textures)
    {
      glLightModeli (GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
      glEnable (GL_TEXTURE_2D);

      gp->current_texture = 0;
      glGenTextures(NUM_TEXTURES, gp->textures);
      grab_texture(mi, gp->current_texture);

      glMatrixMode (GL_TEXTURE);
      glRotated (180.0, 1.0, 0.0, 0.0);
      glMatrixMode (GL_MODELVIEW);
    }

  /* Clear the buffer since this is not done during a draw with motion blur */
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/******************************************************************************
 *
 * Initialise the openGL state data.
 */
static void
set_blob_gl_state(GLfloat alpha)
{
  if (do_antialias)
    {
      glEnable(GL_LINE_SMOOTH);
      glEnable(GL_POLYGON_SMOOTH);
    }

  if (wireframe)
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
  else
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

  /* The blend function is used for trasitioning between two images even when
   * blend is not selected.
   */
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 
  /* Culling. */
  if (culling)
    {
      glCullFace (GL_BACK);
      glEnable (GL_CULL_FACE);
      glFrontFace (GL_CCW);
    }
  else
    {
      glDisable (GL_CULL_FACE);
    }
    
  if (blend < 1.0)
    {
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      /* Set the default blob colour to off-white. */
      glColor4f (0.9, 0.9, 1.0, alpha);
    }
  else
    {
      glDisable(GL_BLEND);
      glColor4f (0.9, 0.9, 1.0, 1.0);
    }
    
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
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
                int width,
                int height,
                int bump_array_size)
{
  /* Loop variables */    
  int i, u, v, node, side, face, base, base2 = 0;
  int nodes_on_edge = resolution;
  Vector3D node1, node2, result;

  if (nodes_on_edge < 2)
    return -1;

  gp->num_nodes = 2 * nodes_on_edge * nodes_on_edge - 4 * nodes_on_edge + 4;
  gp->num_faces = 4 * (nodes_on_edge - 1) * (nodes_on_edge - 1);
 
  gp->nodes = (Node_Data *) malloc (gp->num_nodes * sizeof (Node_Data));
  if (!gp->nodes)
    {
      fprintf (stderr, "Couldn't allocate gp->nodes buffer\n");
      return -1;
    }

  gp->faces = (Face_Data *) malloc (gp->num_faces * sizeof (Face_Data));
  if (!gp->faces)
    {
      fprintf (stderr, "Couldn't allocate faces data buffer\n");
      return -1;
    }

  gp->bump_data = (Bump_Data *) malloc (bumps * sizeof (Bump_Data));
  if (!gp->bump_data)
    {
      fprintf(stderr, "Couldn't allocate bump data buffer\n");
      return -1;
    }

  gp->bump_shape = (double *)malloc(bump_array_size * sizeof(double));
  if (!gp->bump_shape)
    {
      fprintf(stderr, "Couldn't allocate bump buffer\n");
      return -1;
    }

  gp->wall_shape = (double *)malloc(bump_array_size * sizeof(double));
  if (!gp->wall_shape)
    {
      fprintf(stderr, "Couldn't allocate wall bump buffer\n");
      return -1;
    }

	
  gp->dots = (Vector3D *)malloc(gp->num_nodes * sizeof(Vector3D));
  if (!gp->dots)
    {
      fprintf(stderr, "Couldn't allocate nodes buffer\n");
      return -1;
    }

  gp->normals = (Vector3D *)malloc(gp->num_nodes * sizeof(Vector3D));
  if (!gp->normals)
    {
      fprintf(stderr, "Couldn't allocate normals buffer\n");
      return -1;
    }

  gp->colours = (Colour *)malloc(gp->num_nodes * sizeof(Colour));
  if (!gp->colours)
    {
      fprintf(stderr, "Couldn't allocate colours buffer\n");
      return -1;
    }

  gp->tex_coords = (Vector2D *)malloc(gp->num_nodes * sizeof(Vector2D));
  if (!gp->tex_coords)
    {
      fprintf(stderr, "Couldn't allocate gp->tex_coords buffer\n");
      return -1;
    }

	
  /* Initialise bump data */
  for (i = 0; i < bumps; i++)
    {
      gp->bump_data[i].ax = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
      gp->bump_data[i].ay = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
      gp->bump_data[i].power = (5.0 / pow(bumps, 0.75)) * (((double)random() / (double)RAND_MAX) - 0.5);
      gp->bump_data[i].size = 0.1 + 0.5 * (((double)random() / (double)RAND_MAX));

      gp->bump_data[i].pos.x = 1.5 * sin(PI * gp->bump_data[i].ay)
        * cos(PI *  gp->bump_data[i].ax);
      gp->bump_data[i].pos.y = 1.5 * cos(PI * gp->bump_data[i].ay);
      gp->bump_data[i].pos.z = 1.5 * sin(PI * gp->bump_data[i].ay)
        * sin(PI *  gp->bump_data[i].ax);

      gp->bump_data[i].cx = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
      gp->bump_data[i].cy = 2.0 * (((double)random() / (double)RAND_MAX) - 0.5);
      gp->bump_data[i].cpower = (5.0 / pow(bumps, 0.75)) * (((double)random() / (double)RAND_MAX) - 0.5);
      gp->bump_data[i].csize = 0.35; /*0.1 + 0.25 * (((double)random() / (double)RAND_MAX));*/

      gp->bump_data[i].vx = 0.0;
      gp->bump_data[i].vy = 0.0;
      gp->bump_data[i].vpower = 0.0;
      gp->bump_data[i].vsize = 0.0;

      gp->bump_data[i].mx = 0.003 * ((double)random() / (double)RAND_MAX);
      gp->bump_data[i].my = 0.003 * ((double)random() / (double)RAND_MAX);
      gp->bump_data[i].mpower = 0.003 * ((double)random() / (double)RAND_MAX);
      gp->bump_data[i].msize = 0.003 * ((double)random() / (double)RAND_MAX);
    }

  /* Initialise lookup table of bump strength */
  for (i = 0; i < bump_array_size; i++)
    {
      double xd, xd2;
      xd = i / (double)bump_array_size;

      xd2 = 48.0 * xd * xd;
      gp->bump_shape[i] = 0.1 / (xd2 + 0.1);

      xd2 = 40.0 * xd * xd * xd * xd;
      gp->wall_shape[i] = 0.4 / (xd2 + 0.1);
    }

  node = 0;
  face = 0;
  for (side = 0; side < 4; side++)
    {
      base = node;
      if (side == 2) 
        {
          base2 = node;
        }
      /*
       * The start and end of the for loops below are modified based on the 
       * side of the tetrahedron that is being calculated to avoid duplication
       * of the gp->nodes that are on the edges of the tetrahedron. 
       */
      for (u = (side > 1); u < (nodes_on_edge - (side > 0)); u++)
        {
          node1 = partial (normalise (tetrahedron[side][0]),
                           normalise (tetrahedron[side][1]),
                           u / (double) (nodes_on_edge - 1));
          node2 = partial (normalise (tetrahedron[side][0]),
                           normalise (tetrahedron[side][2]),
                           u / (double) (nodes_on_edge - 1));

          for (v = (side > 1); v <= (u - (side > 2)); v++)
            {
              if (u > 0)
                result = partial (node1, node2, v / (double) u);
              else
                result = node1;

              gp->nodes[node].position = normalise (result);
              gp->nodes[node].initial_position = gp->nodes[node].position;
              gp->nodes[node].normal = zero_vector;
              node++;
            }
        }
 
      /*
       * Determine which nodes make up each face.  The complexity is caused 
       * by having to determine the correct nodes for the edges of the
       * tetrahedron since the common nodes on the edges are only calculated
       * once (see above).
       */
      for (u = 0; u < (nodes_on_edge - 1); u++)
        {
          for (v = 0; v <= u; v++)
            {
              {
                if (side < 2)
                  {
                    gp->faces[face].node1 = base + ((u * (u + 1)) / 2) + v;
                    gp->faces[face].node2 =
                      base + ((u + 1) * (u + 2)) / 2 + v + 1;
                    gp->faces[face].node3 =
                      base + ((u + 1) * (u + 2)) / 2 + v;

                    if ((side == 1) && (u == (nodes_on_edge - 2)))
                      {
                        gp->faces[face].node3 =
                          ((u + 1) * (u + 2)) / 2 +
                          nodes_on_edge - v - 1;
                        gp->faces[face].node2 =
                          ((u + 1) * (u + 2)) / 2 +
                          nodes_on_edge - v - 2;
                      }
                  }
                else if (side < 3)
                  {
                    gp->faces[face].node1 =
                      base + (((u - 1) * u) / 2) + v - 1;
                    gp->faces[face].node2 = base + ((u) * (u + 1)) / 2 + v;
                    gp->faces[face].node3 =
                      base + ((u) * (u + 1)) / 2 + v - 1;

                    if (u == (nodes_on_edge - 2))
                      {
                        int n = nodes_on_edge - v - 1;
                        gp->faces[face].node2 =
                          ((nodes_on_edge *
                            (nodes_on_edge + 1)) / 2) +
                          ((n - 1) * (n + 0)) / 2;
                        gp->faces[face].node3 =
                          ((nodes_on_edge *
                            (nodes_on_edge + 1)) / 2) +
                          ((n + 0) * (n + 1)) / 2;
                      }
                    if (v == 0)
                      {
                        gp->faces[face].node1 = (((u + 1) * (u + 2)) / 2) - 1;
                        gp->faces[face].node3 = (((u + 2) * (u + 3)) / 2) - 1;
                      }
                  }
                else
                  {
                    gp->faces[face].node1 =
                      base + (((u - 2) * (u - 1)) / 2) + v - 1;
                    gp->faces[face].node2 = base + ((u - 1) * u) / 2 + v;
                    gp->faces[face].node3 = base + ((u - 1) * u) / 2 + v - 1;

                    if (v == 0)
                      {
                        gp->faces[face].node1 =
                          base2 + ((u * (u + 1)) / 2) - 1;
                        gp->faces[face].node3 =
                          base2 + ((u + 1) * (u + 2)) / 2 - 1;
                      }
                    if (u == (nodes_on_edge - 2))
                      {
                        gp->faces[face].node3 =
                          ((nodes_on_edge *
                            (nodes_on_edge + 1)) / 2) +
                          ((v + 1) * (v + 2)) / 2 - 1;
                        gp->faces[face].node2 =
                          ((nodes_on_edge *
                            (nodes_on_edge + 1)) / 2) +
                          ((v + 2) * (v + 3)) / 2 - 1;
                      }
                    if (v == u)
                      {
                        gp->faces[face].node1 = (u * (u + 1)) / 2;
                        gp->faces[face].node2 = ((u + 1) * (u + 2)) / 2;
                      }
                  }
                face++;
              }

              if (v < u)
                {
                  if (side < 2)
                    {
                      gp->faces[face].node1 = base + ((u * (u + 1)) / 2) + v;
                      gp->faces[face].node2 =
                        base + ((u * (u + 1)) / 2) + v + 1;
                      gp->faces[face].node3 =
                        base + (((u + 1) * (u + 2)) / 2) + v + 1;

                      if ((side == 1) && (u == (nodes_on_edge - 2)))
                        {
                          gp->faces[face].node3 =
                            ((u + 1) * (u + 2)) / 2 +
                            nodes_on_edge - v - 2;
                        }
                    }
                  else if (side < 3)
                    {
                      gp->faces[face].node1 =
                        base + ((u * (u - 1)) / 2) + v - 1;
                      gp->faces[face].node2 = base + ((u * (u - 1)) / 2) + v;
                      gp->faces[face].node3 = base + ((u * (u + 1)) / 2) + v;

                      if (u == (nodes_on_edge - 2))
                        {
                          int n = nodes_on_edge - v - 1;
                          gp->faces[face].node3 =
                            ((nodes_on_edge *
                              (nodes_on_edge + 1)) / 2) +
                            ((n + 0) * (n - 1)) / 2;
                        }
                      if (v == 0)
                        {
                          gp->faces[face].node1 = (((u + 1) * (u + 2)) / 2) - 1;
                        }
                    }
                  else
                    {
                      gp->faces[face].node1 =
                        base + (((u - 2) * (u - 1)) / 2) + v - 1;
                      gp->faces[face].node2 =
                        base + (((u - 2) * (u - 1)) / 2) + v;
                      gp->faces[face].node3 = base + (((u - 1) * u) / 2) + v;

                      if (v == 0)
                        {
                          gp->faces[face].node1 = base2 + (u * (u + 1)) / 2 - 1;
                        }
                      if (u == (nodes_on_edge - 2))
                        {
                          gp->faces[face].node3 =
                            ((nodes_on_edge * (nodes_on_edge + 1)) / 2) +
                            ((v + 2) * (v + 3)) / 2 - 1;
                        }
                      if (v == (u - 1))
                        {
                          gp->faces[face].node2 = (u * (u + 1)) / 2;
                        }
                    }
                  face++;
                }
            }
        }
    }

  return 0;
}

/******************************************************************************
 *
 * Return the magnitude of the given vector
 */
#if 0
static inline double
length (Vector3D u)
{
  return sqrt (u.x * u.x + u.y * u.y + u.z * u.z);
}
#endif

/******************************************************************************
 *
 * Calculate the blob shape.
 */
static void
calc_blob(mirrorblobstruct *gp,
          int width,
          int height,
          int bump_array_size,
          float limit,
          double fade)
{
  /* Loop variables */
  int i, index, face;
  /* position of a node */
  Vector3D node;
  Vector3D offset;
  Vector3D bump_vector;
  int dist;

  /* Update position and strength of bumps used to distort the blob */
  for (i = 0; i < bumps; i++)
    {
      gp->bump_data[i].vx += gp->bump_data[i].mx*(gp->bump_data[i].cx - gp->bump_data[i].ax);
      gp->bump_data[i].vy += gp->bump_data[i].my*(gp->bump_data[i].cy - gp->bump_data[i].ay);
      gp->bump_data[i].vpower += gp->bump_data[i].mpower
        * (gp->bump_data[i].cpower - gp->bump_data[i].power);
      gp->bump_data[i].vsize += gp->bump_data[i].msize
        * (gp->bump_data[i].csize - gp->bump_data[i].size);

      gp->bump_data[i].ax += 0.1 * gp->bump_data[i].vx;
      gp->bump_data[i].ay += 0.1 * gp->bump_data[i].vy;
      gp->bump_data[i].power += 0.1 * gp->bump_data[i].vpower;
      gp->bump_data[i].size += 0.1 * gp->bump_data[i].vsize;

      gp->bump_data[i].pos.x = 1.0 * sin(PI * gp->bump_data[i].ay)
        * cos(PI * gp->bump_data[i].ax);
      gp->bump_data[i].pos.y = 1.0 * cos(PI * gp->bump_data[i].ay);
      gp->bump_data[i].pos.z = 1.0 * sin(PI * gp->bump_data[i].ay)
        * sin(PI * gp->bump_data[i].ax);
    }

  /* Update calculate new position for each vertex based on an offset from
   * the initial position
   */
  gp->blob_force = zero_vector;
  for (index = 0; index < gp->num_nodes; ++index)
    {
      node = gp->nodes[index].initial_position;
      gp->nodes[index].normal = node;

      offset = zero_vector;
      for ( i = 0; i < bumps; i++)
        {
          bump_vector = subtract(gp->bump_data[i].pos, node);

          dist = bump_array_size * dot(bump_vector, bump_vector) * gp->bump_data[i].size;

          if (dist < bump_array_size)
            {
              add(&offset, scale(node, gp->bump_data[i].power * gp->bump_shape[dist]));
              add(&gp->blob_force, scale(node, gp->bump_data[i].power * gp->bump_shape[dist]));
            }
        }

      add(&node, offset);
      node = scale(node, zoom);
      add(&node, gp->blob_center);

      if (do_walls)
        {
          if (node.z < -limit) node.z = -limit;
          if (node.z > limit) node.z = limit;

          dist = bump_array_size * (node.z + limit) * (node.z + limit) * 0.5;
          if (dist < bump_array_size)
            {
              node.x += (node.x - gp->blob_center.x) * gp->wall_shape[dist];
              node.y += (node.y - gp->blob_center.y) * gp->wall_shape[dist];
              gp->blob_force.z += (node.z + limit);
            }
          else
            {
              dist = bump_array_size * (node.z - limit) * (node.z - limit) * 0.5;
              if (dist < bump_array_size)
                {
                  node.x += (node.x - gp->blob_center.x) * gp->wall_shape[dist];
                  node.y += (node.y - gp->blob_center.y) * gp->wall_shape[dist];
                  gp->blob_force.z -= (node.z - limit);
                }

              if (node.y < -limit) node.y = -limit;
              if (node.y > limit) node.y = limit;

              dist = bump_array_size * (node.y + limit) * (node.y + limit) * 0.5;
              if (dist < bump_array_size)
                {
                  node.x += (node.x - gp->blob_center.x) * gp->wall_shape[dist];
                  node.z += (node.z - gp->blob_center.z) * gp->wall_shape[dist];
                  gp->blob_force.y += (node.y + limit);
                }
              else
                {
                  dist = bump_array_size * (node.y - limit) * (node.y - limit) * 0.5;
                  if (dist < bump_array_size)
                    {
                      node.x += (node.x - gp->blob_center.x) * gp->wall_shape[dist];
                      node.z += (node.z - gp->blob_center.z) * gp->wall_shape[dist];
                      gp->blob_force.y -= (node.y - limit);
                    }
                }

              if (node.x < -limit) node.x = -limit;
              if (node.x > limit) node.x = limit;

              dist = bump_array_size * (node.x + limit) * (node.x + limit) * 0.5;
              if (dist < bump_array_size)
                {
                  node.y += (node.y - gp->blob_center.y) * gp->wall_shape[dist];
                  node.z += (node.z - gp->blob_center.z) * gp->wall_shape[dist];
                  gp->blob_force.x += (node.x + limit);
                }
              else
                {
                  dist = bump_array_size * (node.x - limit) * (node.x - limit) * 0.5;
                  if (dist < bump_array_size)
                    {
                      node.y += (node.y - gp->blob_center.y) * gp->wall_shape[dist];
                      node.z += (node.z - gp->blob_center.z) * gp->wall_shape[dist];
                      gp->blob_force.x -= (node.x - limit);
                    }
                }

              if (node.y < -limit) node.y = -limit;
              if (node.y > limit) node.y = limit;
            }
        }
      gp->dots[index] = node;
    }

  /* Determine the normal for each face */
  for (face = 0; face < gp->num_faces; face++)
    {
      /* Use pointers to indexed nodes to help readability */
      int index1 = gp->faces[face].node1;
      int index2 = gp->faces[face].node2;
      int index3 = gp->faces[face].node3;

      gp->faces[face].normal = cross(subtract(gp->dots[index2], gp->dots[index1]),
                                     subtract(gp->dots[index3], gp->dots[index1]));
            
      /* Add the normal for the face onto the normal for the verticies of
         the face */
      add(&gp->nodes[index1].normal, gp->faces[face].normal);
      add(&gp->nodes[index2].normal, gp->faces[face].normal);
      add(&gp->nodes[index3].normal, gp->faces[face].normal);
    }

  /* Use the normal to set the colour and texture */
  if (do_colour || do_texture)
    {
      for (index = 0; index < gp->num_nodes; ++index)
        {
          gp->normals[index] = normalise(gp->nodes[index].normal);
   
          if (do_colour)
            {
              gp->colours[index].red = (int)(255.0 * fabs(gp->normals[index].x));
              gp->colours[index].green = (int)(255.0 * fabs(gp->normals[index].y));
              gp->colours[index].blue = (int)(255.0 * fabs(gp->normals[index].z));
              gp->colours[index].alpha = (int)(255.0 * fade);
            }
          if (do_texture)
            {
              if (offset_texture)
                {
                  const float cube_size = 100.0;
                  Vector3D eye = {0.0, 0.0, 50.0};
                  Vector3D eye_r = normalise(subtract(gp->dots[index], eye));
                  Vector3D reference = subtract(eye_r, scale(gp->normals[index], 2.0 * dot(eye_r, gp->normals[index])));
                  double x = 0.0;
                  double y = 0.0;
                  double n, n_min = 10000.0, sign = 1.0;
                  if (fabs(reference.z) > 1e-9)
                    {
                      n = (cube_size - gp->dots[index].z) / reference.z;
                      if (n < 0.0)
                        {
                          n = (-cube_size - gp->dots[index].z) / reference.z;
                          sign = 3.0;
                        }
                      if (n > 0.0)
                        {
                          x = sign * (gp->dots[index].x + n * reference.x);
                          y = sign * (gp->dots[index].y + n * reference.y);
                          n_min = n;
                        }
                    }
                  if (fabs(reference.x) > 1e-9)
                    {
                      n = (cube_size - gp->dots[index].x) / reference.x;
                      sign = 1.0;
                      if (n < 0.0)
                        {
                          n = (-cube_size - gp->dots[index].x) / reference.x;
                          sign = -1.0;
                        }
                      if ((n > 0.0) && (n < n_min))
                        {
                          x = sign * (2.0 * cube_size - (gp->dots[index].z + n * reference.z));
                          y = sign * x * (gp->dots[index].y + n * reference.y) / cube_size;
                          n_min = n;
                        }
                    }
                  if (fabs(reference.y) > 1e-9)
                    {
                      n = (cube_size - gp->dots[index].y) / reference.y;
                      sign = 1.0;
                      if (n < 0.0)
                        {
                          n = (-cube_size - gp->dots[index].y) / reference.y;
                          sign = -1.0;
                        }
                      if ((n > 0.0) && (n < n_min))
                        {
                          y = sign * (2.0 * cube_size -( gp->dots[index].z + n * reference.z));
                          x = sign * y * (gp->dots[index].x + n * reference.x) / cube_size;
                        }
                    }
					
                  gp->tex_coords[index].x = 0.5 + x / (cube_size * 6.0);
                  gp->tex_coords[index].y = 0.5 - y / (cube_size * 6.0);
                }
              else
                {
                  gp->tex_coords[index].x = 0.5
                    * (1.0 + asin(gp->normals[index].x) / (0.5 * PI));
                  gp->tex_coords[index].y = -0.5
                    * (1.0 + asin(gp->normals[index].y) / (0.5 * PI));
                }
              /* Adjust the texture co-ordinates to from range 0..1 to
               * 0..width or 0..height as appropriate
               */
              gp->tex_coords[index].x *= gp->tex_width[gp->current_texture];
              gp->tex_coords[index].y *= gp->tex_height[gp->current_texture];
            }
        }
    }
    
  /* Update the center of the whole blob */
  add(&gp->blob_velocity, scale (subtract (gp->blob_anchor, gp->blob_center), 1.0 / 80.0));
  add(&gp->blob_velocity, scale (gp->blob_force, 0.01 / gp->num_nodes));

  add(&gp->blob_center, scale(gp->blob_velocity, 0.5));

  gp->blob_velocity = scale(gp->blob_velocity, 0.999);
}

static void
draw_vertex(mirrorblobstruct *gp, int index)
{
  if (do_colour)
    {
      glColor3ub(gp->colours[index].red,
                 gp->colours[index].green,
                 gp->colours[index].blue);
    }
  if (load_textures)
    {
      glTexCoord2fv(&gp->tex_coords[index].x);
    }
  glNormal3fv(&gp->normals[index].x);
  glVertex3fv(&gp->dots[index].x);
}

/******************************************************************************
 *
 * Draw the blob shape.
 *
 */
static void
draw_blob (mirrorblobstruct *gp)
{
  int face;

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
/*  glRotatef(current_device_rotation(), 0, 0, 1); */

  /* Move down the z-axis. */
  glTranslatef (0.0, 0.0, -4.0);

  gltrackball_rotate (gp->trackball);

  /* glColor4ub (255, 0, 0, 128); */
  glBegin(GL_TRIANGLES);
  for (face = 0; face < gp->num_faces; face++)
    {
      draw_vertex(gp, gp->faces[face].node1);
      draw_vertex(gp, gp->faces[face].node2);
      draw_vertex(gp, gp->faces[face].node3);
    }
  glEnd();

#if 0
  glBegin(GL_LINES);
  for (face = 0; face < gp->num_faces; face++)
    {
      if (gp->normals[gp->faces[face].node1].z > 0.0)
        {
          Vector3D end = gp->dots[gp->faces[face].node1];
          glVertex3dv(&end);
          add(&end, scale(gp->normals[gp->faces[face].node1], 0.25));
          glVertex3dv(&end);
        }
    }
  glEnd();
#endif
	
  glLoadIdentity();
}

/******************************************************************************
 *
 * Draw the background image simply map a texture onto a full screen quad.
 */
static void
draw_background (ModeInfo *mi)
{
  mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN(mi)];
  GLfloat rot = current_device_rotation();
    
  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
  glEnable (GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  /* Reset the projection matrix to make it easier to get the size of the quad
   * correct
   */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  glRotatef (-rot, 0, 0, 1);
/*
  if ((rot >  45 && rot <  135) ||
      (rot < -45 && rot > -135))
    {
      GLfloat s = MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
      glScalef (s, 1/s, 1);
    }
*/

  glOrtho(0.0, MI_WIDTH(mi), MI_HEIGHT(mi), 0.0, -1000.0, 1000.0);

  glBegin (GL_QUADS);
    
  glTexCoord2f (0.0, 0.0);
  glVertex2i (0, 0);
    
  glTexCoord2f (0.0, gp->tex_height[gp->current_texture]);
  glVertex2i (0, MI_HEIGHT(mi));

  glTexCoord2f (gp->tex_width[gp->current_texture], gp->tex_height[gp->current_texture]);
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

  mi->polygon_count = 0;
  glColor4f (1.0, 1.0, 1.0, 1.0);

  current_time = double_time();
  switch (gp->state)
    {
    case INITIALISING:
      glColor4f (0.0, 0.0, 0.0, 1.0);
      fade = 1.0;
      break;

    case TRANSITIONING:
      fade = 1.0 - (current_time - gp->state_start_time) / fade_time;
      break;

    case LOADING: /* FALL-THROUGH */
    case HOLDING:
      fade = 1.0;
      break;
    }

  /* Set the correct texture, when transitioning this ensures that the first draw
   * is the original texture (which has the new texture drawn over it with decreasing
   * transparency)
   */
  if (load_textures)
    {
      glBindTexture(GL_TEXTURE_2D, gp->textures[gp->current_texture]);
    }

  glDisable (GL_DEPTH_TEST);
  if (do_paint_background)
    {
      glEnable (GL_TEXTURE_2D);
      if (motion_blur > 0.0)
        {
          glClear(GL_DEPTH_BUFFER_BIT);
          glEnable (GL_BLEND);
          glColor4f (1.0, 1.0, 1.0, motion_blur);
        }
      else
        {
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
      draw_background (mi);
      mi->polygon_count++;

      /* When transitioning between two images paint the new image over the old
       * image with a varying alpha value to get a smooth fade.
       */
      if (gp->state == TRANSITIONING)
        {
          glEnable (GL_BLEND);
          /* Select the texture to transition to */
          glBindTexture (GL_TEXTURE_2D, gp->textures[1 - gp->current_texture]);
          glColor4f (1.0, 1.0, 1.0, 1.0 - fade);

          draw_background (mi);
          mi->polygon_count++;

          /* Select the original texture to draw the blob */
          glBindTexture (GL_TEXTURE_2D, gp->textures[gp->current_texture]);
        }
      /* Clear the depth buffer bit so the backgound is behind the blob */
      glClear(GL_DEPTH_BUFFER_BIT);
    }
  else if (motion_blur > 0.0)
    {
      glEnable (GL_BLEND);
      glColor4f (0.0, 0.0, 0.0, motion_blur);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glTranslatef (0.0, 0.0, -4.0);
      glRectd (-10.0, -10.0, 10.0, 10.0);
      if (wireframe)
        {
          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
      glClear(GL_DEPTH_BUFFER_BIT);
    }
  else
    {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

  if (!do_texture)
    {
      fade = 1.0;
      glDisable (GL_TEXTURE_2D);
    }

  calc_blob(gp, MI_WIDTH(mi), MI_HEIGHT(mi), BUMP_ARRAY_SIZE, 2.5, fade * blend);

  set_blob_gl_state(fade * blend);

  if (blend < 1.0)
    {
      /* Disable the colour chanels so that only the depth buffer is updated */
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      draw_blob(gp);
      mi->polygon_count += gp->num_faces;
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
	
  glDepthFunc(GL_LEQUAL);
  draw_blob(gp);
  mi->polygon_count += gp->num_faces;

  /* While transitioning between images draw a second blob with a modified
   * alpha value.
   */
  if (load_textures && (hold_time > 0))
    {
      switch (gp->state)
        {
        case INITIALISING:
          if (!gp->waiting_for_image_p)
            {
              gp->state = HOLDING;
            }
          break;
		
        case HOLDING:
          if ((current_time - gp->state_start_time) > hold_time)
            {
              grab_texture(mi, 1 - gp->current_texture);
              gp->state = LOADING;
            }
          break;

        case LOADING:
          /* Once the image has loaded move to the TRANSITIONING STATE */
          if (!gp->waiting_for_image_p)
            {
              gp->state = TRANSITIONING;
              /* Get the time again rather than using the current time so
               * that the time taken by the grab_texture function is not part
               * of the fade time
               */
              gp->state_start_time = double_time();
            }
          break;        

        case TRANSITIONING:

          /* If the blob is textured draw over existing blob to fade between
           * images
           */
          if (do_texture)
            {
              /* Select the texture to transition to */
              glBindTexture (GL_TEXTURE_2D, gp->textures[1 - gp->current_texture]);
              glEnable (GL_BLEND);
                
              /* If colour is enabled update the alpha data in the buffer and
               * use that in the blending since the alpha of the incomming
               * verticies will not be correct
               */
              if (do_colour)
                {
                  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
                  glClearColor(0.0, 0.0, 0.0, (1.0 - fade) * blend);
                  glClear(GL_COLOR_BUFFER_BIT);
                  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);                    
                  glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
                }
              else
                {
                  glColor4f (0.9, 0.9, 1.0, (1.0 - fade) * blend);
                }

              draw_blob (gp);
              mi->polygon_count += gp->num_faces;

              if (do_colour)
                {
                  /* Restore the 'standard' blend functions. */
                  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
            }
            
          if ((current_time - gp->state_start_time) > fade_time)
            {
              gp->state = HOLDING;
              gp->state_start_time = current_time;
              gp->current_texture = 1 - gp->current_texture;
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
  glFinish();
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

/****************************************************************************
 *
 * Handle Mouse events
 */
ENTRYPOINT Bool
mirrorblob_handle_event (ModeInfo * mi, XEvent * event)
{
  mirrorblobstruct *gp = &Mirrorblob[MI_SCREEN (mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button4)
    {
      zoom *= 1.1;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           event->xbutton.button == Button5)
    {

      zoom *= 0.9;
      return True;
    }
  else if (gltrackball_event_handler (event, gp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &gp->button_down))
    {
      return True;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      gp->state_start_time = 0;
      gp->state = HOLDING;
      return True;
    }

  return False;
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
  gp->trackball = gltrackball_init(False);
    
  initialise_blob(gp, MI_WIDTH(mi), MI_HEIGHT(mi), BUMP_ARRAY_SIZE);
  gp->state = INITIALISING;
  gp->state_start_time = double_time();

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
      if (gp->nodes) free(gp->nodes);
      if (gp->faces) free(gp->faces);
      if (gp->bump_data) free(gp->bump_data);
      if (gp->colours) free(gp->colours);
      if (gp->tex_coords) free(gp->tex_coords);
      if (gp->dots) free(gp->dots);
      if (gp->wall_shape) free(gp->wall_shape);
      if (gp->bump_shape) free(gp->bump_shape);
    }

    free(Mirrorblob);
    Mirrorblob = NULL;
  }
  FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("MirrorBlob", mirrorblob)

#endif
