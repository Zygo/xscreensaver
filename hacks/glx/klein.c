/* klein --- Shows a Klein bottle that rotates in 4d or on which you
   can walk */

#if 0
static const char sccsid[] = "@(#)klein.c  1.1 08/10/04 xlockmore";
#endif

/* Copyright (c) 2005-2013 Carsten Steger <carsten@mirsanmir.org>. */

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
 * REVISION HISTORY:
 * C. Steger - 08/10/04: Initial version
 * C. Steger - 09/08/03: Changes to the parameter handling
 * C. Steger - 13/12/25: Added the squeezed torus Klein bottle
 */

/*
 * This program shows three different Klein bottles in 4d: the figure-8 Klein
 * bottle, the squeezed torus Klein bottle, or the Lawson Klein bottle.  You
 * can walk on the Klein bottle, see it turn in 4d, or walk on it while it
 * turns in 4d.  The figure-8 Klein bottle is well known in its 3d form.  The
 * 4d form used in this program is an extension of the 3d form to 4d that
 * does not intersect itself in 4d (which can be seen in the depth colors
 * mode).  The squeezed torus Klein bottle also does not intersect itself in
 * 4d (which can be seen in the depth colors mode).  The Lawson Klein bottle,
 * on the other hand, does intersect itself in 4d.  Its primary use is that
 * it has a nice appearance for walking and for turning in 3d.  The Klein
 * bottle is a non-orientable surface.  To make this apparent, the two-sided
 * color mode can be used.  Alternatively, orientation markers (curling
 * arrows) can be drawn as a texture map on the surface of the Klein bottle.
 * While walking on the Klein bottle, you will notice that the orientation
 * of the curling arrows changes (which it must because the Klein bottle is
 * non-orientable).  The program projects the 4d Klein bottle to 3d using
 * either a perspective or an orthographic projection.  Which of the two
 * alternatives looks more appealing depends on the viewing mode and the
 * Klein bottle.  For example, the Lawson Klein bottle looks nicest when
 * projected perspectively.  The figure-8 Klein bottle, on the other
 * hand, looks nicer while walking when projected orthographically from 4d.
 * For the squeezed torus Klein bottle, both projection modes give equally
 * acceptable projections.  The projected Klein bottle can then be projected
 * to the screen either perspectively or orthographically.  When using the
 * walking modes, perspective projection to the screen should be used.  There
 * are three display modes for the Klein bottle: mesh (wireframe), solid, or
 * transparent.  Furthermore, the appearance of the Klein bottle can be as
 * a solid object or as a set of see-through bands.  Finally, the colors
 * with with the Klein bottle is drawn can be set to two-sided, rainbow, or
 * depth.  In the first case, the Klein bottle is drawn with red on one
 * "side" and green on the "other side".  Of course, the Klein bottle only
 * has one side, so the color jumps from red to green along a curve on the
 * surface of the Klein bottle.  This mode enables you to see that the Klein
 * bottle is non-orientable.  The second mode draws the Klein bottle with
 * fully saturated rainbow colors.  This gives a very nice effect when
 * combined with the see-through bands mode or with the orientation markers
 * drawn.  The third mode draws the Klein bottle with colors that are chosen
 * according to the 4d "depth" of the points.  This mode enables you to see
 * that the figure-8 and squeezed torus Klein bottles do not intersect
 * themselves in 4d, while the Lawson Klein bottle does intersect itself.
 * The rotation speed for each of the six planes around which the Klein
 * bottle rotates can be chosen.  For the walk-and-turn more, only the
 * rotation speeds around the true 4d planes are used (the xy, xz, and yz
 * planes).  Furthermore, in the walking modes the walking direction in the
 * 2d base square of the Klein bottle and the walking speed can be chosen.
 * This program is somewhat inspired by Thomas Banchoff's book "Beyond the
 * Third Dimension: Geometry, Computer Graphics, and Higher Dimensions",
 * Scientific American Library, 1990.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define KLEIN_BOTTLE_FIGURE_8       0
#define KLEIN_BOTTLE_SQUEEZED_TORUS 1
#define KLEIN_BOTTLE_LAWSON         2
#define NUM_KLEIN_BOTTLES           3

#define DISP_WIREFRAME              0
#define DISP_SURFACE                1
#define DISP_TRANSPARENT            2
#define NUM_DISPLAY_MODES           3

#define APPEARANCE_SOLID            0
#define APPEARANCE_BANDS            1
#define NUM_APPEARANCES             2

#define COLORS_TWOSIDED             0
#define COLORS_RAINBOW              1
#define COLORS_DEPTH                2
#define NUM_COLORS                  3

#define VIEW_WALK                   0
#define VIEW_TURN                   1
#define VIEW_WALKTURN               2
#define NUM_VIEW_MODES              3

#define DISP_3D_PERSPECTIVE         0
#define DISP_3D_ORTHOGRAPHIC        1
#define NUM_DISP_3D_MODES           2

#define DISP_4D_PERSPECTIVE         0
#define DISP_4D_ORTHOGRAPHIC        1
#define NUM_DISP_4D_MODES           2

#define DEF_KLEIN_BOTTLE            "random"
#define DEF_DISPLAY_MODE            "random"
#define DEF_APPEARANCE              "random"
#define DEF_COLORS                  "random"
#define DEF_VIEW_MODE               "random"
#define DEF_MARKS                   "False"
#define DEF_PROJECTION_3D           "random"
#define DEF_PROJECTION_4D           "random"
#define DEF_SPEEDWX                 "1.1"
#define DEF_SPEEDWY                 "1.3"
#define DEF_SPEEDWZ                 "1.5"
#define DEF_SPEEDXY                 "1.7"
#define DEF_SPEEDXZ                 "1.9"
#define DEF_SPEEDYZ                 "2.1"
#define DEF_WALK_DIRECTION          "7.0"
#define DEF_WALK_SPEED              "20.0"

#ifdef STANDALONE
# define DEFAULTS           "*delay:      10000 \n" \
                            "*showFPS:    False \n" \

# define refresh_klein 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifndef HAVE_COCOA
# include <X11/keysym.h>
#endif

#include "gltrackball.h"


#ifdef USE_MODULES
ModStruct   klein_description =
{"klein", "init_klein", "draw_klein", "release_klein",
 "draw_klein", "change_klein", NULL, &klein_opts,
 25000, 1, 1, 1, 1.0, 4, "",
 "Rotate a Klein bottle in 4d or walk on it", 0, NULL};

#endif


static char *klein_bottle;
static int bottle_type;
static char *mode;
static int display_mode;
static char *appear;
static int appearance;
static char *color_mode;
static int colors;
static char *view_mode;
static int view;
static Bool marks;
static char *proj_3d;
static int projection_3d;
static char *proj_4d;
static int projection_4d;
static float speed_wx;
static float speed_wy;
static float speed_wz;
static float speed_xy;
static float speed_xz;
static float speed_yz;
static float walk_direction;
static float walk_speed;


static XrmOptionDescRec opts[] =
{
  {"-klein-bottle",      ".kleinBottle",   XrmoptionSepArg, 0 },
  {"-figure-8",          ".kleinBottle",   XrmoptionNoArg,  "figure-8" },
  {"-squeezed-torus",    ".kleinBottle",   XrmoptionNoArg,  "squeezed-torus" },
  {"-lawson",            ".kleinBottle",   XrmoptionNoArg,  "lawson" },
  {"-mode",              ".displayMode",   XrmoptionSepArg, 0 },
  {"-wireframe",         ".displayMode",   XrmoptionNoArg,  "wireframe" },
  {"-surface",           ".displayMode",   XrmoptionNoArg,  "surface" },
  {"-transparent",       ".displayMode",   XrmoptionNoArg,  "transparent" },
  {"-appearance",        ".appearance",    XrmoptionSepArg, 0 },
  {"-solid",             ".appearance",    XrmoptionNoArg,  "solid" },
  {"-bands",             ".appearance",    XrmoptionNoArg,  "bands" },
  {"-colors",            ".colors",        XrmoptionSepArg, 0 },
  {"-twosided",          ".colors",        XrmoptionNoArg,  "two-sided" },
  {"-rainbow",           ".colors",        XrmoptionNoArg,  "rainbow" },
  {"-depth",             ".colors",        XrmoptionNoArg,  "depth" },
  {"-view-mode",         ".viewMode",      XrmoptionSepArg, 0 },
  {"-walk",              ".viewMode",      XrmoptionNoArg,  "walk" },
  {"-turn",              ".viewMode",      XrmoptionNoArg,  "turn" },
  {"-walk-turn",         ".viewMode",      XrmoptionNoArg,  "walk-turn" },
  {"-orientation-marks", ".marks",         XrmoptionNoArg, "on"},
  {"+orientation-marks", ".marks",         XrmoptionNoArg, "off"},
  {"-projection-3d",     ".projection3d",  XrmoptionSepArg, 0 },
  {"-perspective-3d",    ".projection3d",  XrmoptionNoArg,  "perspective" },
  {"-orthographic-3d",   ".projection3d",  XrmoptionNoArg,  "orthographic" },
  {"-projection-4d",     ".projection4d",  XrmoptionSepArg, 0 },
  {"-perspective-4d",    ".projection4d",  XrmoptionNoArg,  "perspective" },
  {"-orthographic-4d",   ".projection4d",  XrmoptionNoArg,  "orthographic" },
  {"-speed-wx",          ".speedwx",       XrmoptionSepArg, 0 },
  {"-speed-wy",          ".speedwy",       XrmoptionSepArg, 0 },
  {"-speed-wz",          ".speedwz",       XrmoptionSepArg, 0 },
  {"-speed-xy",          ".speedxy",       XrmoptionSepArg, 0 },
  {"-speed-xz",          ".speedxz",       XrmoptionSepArg, 0 },
  {"-speed-yz",          ".speedyz",       XrmoptionSepArg, 0 },
  {"-walk-direction",    ".walkDirection", XrmoptionSepArg, 0 },
  {"-walk-speed",        ".walkSpeed",     XrmoptionSepArg, 0 }
};

static argtype vars[] =
{
  { &klein_bottle,   "kleinBottle",   "KleinBottle",   DEF_KLEIN_BOTTLE,   t_String },
  { &mode,           "displayMode",   "DisplayMode",   DEF_DISPLAY_MODE,   t_String },
  { &appear,         "appearance",    "Appearance",    DEF_APPEARANCE,     t_String },
  { &color_mode,     "colors",        "Colors",        DEF_COLORS,         t_String },
  { &view_mode,      "viewMode",      "ViewMode",      DEF_VIEW_MODE,      t_String },
  { &marks,          "marks",         "Marks",         DEF_MARKS,          t_Bool },
  { &proj_3d,        "projection3d",  "Projection3d",  DEF_PROJECTION_3D,  t_String },
  { &proj_4d,        "projection4d",  "Projection4d",  DEF_PROJECTION_4D,  t_String },
  { &speed_wx,       "speedwx",       "Speedwx",       DEF_SPEEDWX,        t_Float},
  { &speed_wy,       "speedwy",       "Speedwy",       DEF_SPEEDWY,        t_Float},
  { &speed_wz,       "speedwz",       "Speedwz",       DEF_SPEEDWZ,        t_Float},
  { &speed_xy,       "speedxy",       "Speedxy",       DEF_SPEEDXY,        t_Float},
  { &speed_xz,       "speedxz",       "Speedxz",       DEF_SPEEDXZ,        t_Float},
  { &speed_yz,       "speedyz",       "Speedyz",       DEF_SPEEDYZ,        t_Float},
  { &walk_direction, "walkDirection", "WalkDirection", DEF_WALK_DIRECTION, t_Float},
  { &walk_speed,     "walkSpeed",     "WalkSpeed",     DEF_WALK_SPEED,     t_Float}
};

ENTRYPOINT ModeSpecOpt klein_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Radius of the figure-8 Klein bottle */
#define FIGURE_8_RADIUS 2.0

/* Radius of the squeezed torus Klein bottle */
#define SQUEEZED_TORUS_RADIUS 2.0

/* Offset by which we walk above the Klein bottle */
#define DELTAY  0.02

/* Number of subdivisions of the Klein bottle */
#define NUMU 128
#define NUMV 128

/* Number of subdivisions per band */
#define NUMB 8


typedef struct {
  GLint      WindH, WindW;
  GLXContext *glx_context;
  /* 4D rotation angles */
  float alpha, beta, delta, zeta, eta, theta;
  /* Movement parameters */
  float umove, vmove, dumove, dvmove;
  int side;
  /* The viewing offset in 4d */
  float offset4d[4];
  /* The viewing offset in 3d */
  float offset3d[4];
  /* The 4d coordinates of the Klein bottle and their derivatives */
  float x[(NUMU+1)*(NUMV+1)][4];
  float xu[(NUMU+1)*(NUMV+1)][4];
  float xv[(NUMU+1)*(NUMV+1)][4];
  float pp[(NUMU+1)*(NUMV+1)][3];
  float pn[(NUMU+1)*(NUMV+1)][3];
  /* The precomputed colors of the Klein bottle */
  float col[(NUMU+1)*(NUMV+1)][4];
  /* The precomputed texture coordinates of the Klein bottle */
  float tex[(NUMU+1)*(NUMV+1)][2];
  /* The "curlicue" texture */
  GLuint tex_name;
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackballs[2];
  int current_trackball;
  Bool button_pressed;
  /* A random factor to modify the rotation speeds */
  float speed_scale;
} kleinstruct;

static kleinstruct *klein = (kleinstruct *) NULL;


/* A texture map containing a "curlicue" */
#define TEX_DIMENSION 64
static const unsigned char texture[TEX_DIMENSION*TEX_DIMENSION] = {
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 58, 43, 43, 43, 43, 45, 70, 70, 70,
   70, 70, 70, 70, 74, 98, 98, 98,100,194,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0, 30,186,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  1,111,244,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0, 43,198,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  5,123,248,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    0, 50,209,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,246,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
   74,252,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,138,  4,
   66,229,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    1,170,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,153,  0,  0,
    0, 53,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 18,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  6,188,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,213,  7,  0,  0,
    0,  0,226,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,  0,  0,  0,  0, 47,  0,  0,
    0,  0, 22,225,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,254, 54,  0,  0,  0,
    0, 81,254,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,  0,  0,  0, 56,247, 82,  0,
    0,  0,  0, 59,253,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,152,  0,  0,  0,  0,
   52,243,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,  0,  0,  8,215,255,250, 56,
    0,  0,  0,  0,142,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,241, 19,  0,  0,  0, 15,
  220,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,  0,  0,129,255,255,255,230,
   23,  0,  0,  0, 12,230,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,131,  0,  0,  0,  0,157,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,  0, 49,250,255,255,255,255,
  171,  0,  0,  0,  0,112,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,246, 19,  0,  0,  0, 54,253,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,  5,208,255,255,255,255,255,
  255, 77,  0,  0,  0,  9,231,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,163,  0,  0,  0,  0,186,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45,  0,121,255,255,255,255,255,255,
  255,211,  2,  0,  0,  0,134,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255, 69,  0,  0,  0, 50,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 45, 41,247,255,255,255,255,255,255,
  255,255, 73,  0,  0,  0, 38,254,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,237,  4,  0,  0,  0,145,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255, 52,201,255,255,255,255,255,255,255,
  255,255,169,  0,  0,  0,  0,216,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,181,  0,  0,  0,  0,229,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,186,255,255,255,255,255,255,255,255,
  255,255,247,  7,  0,  0,  0,150,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,130,  0,  0,  0, 42,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255, 67,  0,  0,  0, 91,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 79,  0,  0,  0, 95,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,120,  0,  0,  0, 56,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 55,  0,  0,  0,130,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,157,  0,  0,  0, 21,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 34,  0,  0,  0,161,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,179,  0,  0,  0,  2,250,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 27,  0,  0,  0,168,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,200,  0,  0,  0,  0,249,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 27,  0,  0,  0,168,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,200,  0,  0,  0,  0,249,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 27,  0,  0,  0,163,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,183,  0,  0,  0,  0,249,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 42,  0,  0,  0,135,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,161,  0,  0,  0, 17,254,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 76,  0,  0,  0,100,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,126,  0,  0,  0, 48,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,114,  0,  0,  0, 53,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255, 78,  0,  0,  0, 84,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,165,  0,  0,  0,  3,241,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,252, 16,  0,  0,  0,139,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,228,  0,  0,  0,  0,161,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,192,  0,  0,  0,  0,198,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255, 46,  0,  0,  0, 67,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255, 93,  0,  0,  0, 21,250,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,139,  0,  0,  0,  1,211,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,226,  7,  0,  0,  0,108,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,230,  6,  0,  0,  0, 79,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,106,  0,  0,  0,  1,206,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255, 97,  0,  0,  0,  0,183,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  202,  3,  0,  0,  0, 67,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,221,  8,  0,  0,  0, 27,
  235,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,243,
   40,  0,  0,  0,  0,198,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,126,  0,  0,  0,  0,
   71,252,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,253, 85,
    0,  0,  0,  0, 96,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,247, 44,  0,  0,  0,
    0, 91,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,116,  0,
    0,  0,  0, 25,233,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,216, 11,  0,  0,
    0,  0, 90,251,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,252,112,  0,  0,
    0,  0,  4,191,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,174,  4,  0,
    0,  0,  0, 72,235,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,242, 84,  0,  0,  0,
    0,  0,146,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,150,  1,
    0,  0,  0,  0, 27,181,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,194, 39,  0,  0,  0,  0,
    0,120,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,151,
    4,  0,  0,  0,  0,  0, 77,209,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,216, 92,  1,  0,  0,  0,  0,  0,
  125,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  175, 12,  0,  0,  0,  0,  0,  1, 70,164,241,255,255,255,255,255,
  255,255,255,255,255,242,171, 77,  2,  0,  0,  0,  0,  0,  4,150,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,214, 41,  0,  0,  0,  0,  0,  0,  0,  4, 48, 98,138,163,163,
  163,163,140,103, 55,  5,  0,  0,  0,  0,  0,  0,  0, 30,199,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,245,125,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,105,240,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,222,100,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  2, 83,210,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,228,136, 45,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0, 37,125,220,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,225,166,112, 74, 43, 32, 12,
    8, 32, 40, 71,105,162,218,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};


/* Add a rotation around the wx-plane to the matrix m. */
static void rotatewx(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][1];
    v = m[i][2];
    m[i][1] = c*u+s*v;
    m[i][2] = -s*u+c*v;
  }
}


/* Add a rotation around the wy-plane to the matrix m. */
static void rotatewy(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][2];
    m[i][0] = c*u-s*v;
    m[i][2] = s*u+c*v;
  }
}


/* Add a rotation around the wz-plane to the matrix m. */
static void rotatewz(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][1];
    m[i][0] = c*u+s*v;
    m[i][1] = -s*u+c*v;
  }
}


/* Add a rotation around the xy-plane to the matrix m. */
static void rotatexy(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][2];
    v = m[i][3];
    m[i][2] = c*u+s*v;
    m[i][3] = -s*u+c*v;
  }
}


/* Add a rotation around the xz-plane to the matrix m. */
static void rotatexz(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][1];
    v = m[i][3];
    m[i][1] = c*u-s*v;
    m[i][3] = s*u+c*v;
  }
}


/* Add a rotation around the yz-plane to the matrix m. */
static void rotateyz(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][3];
    m[i][0] = c*u-s*v;
    m[i][3] = s*u+c*v;
  }
}


/* Compute the rotation matrix m from the rotation angles. */
static void rotateall(float al, float be, float de, float ze, float et,
                      float th, float m[4][4])
{
  int i, j;

  for (i=0; i<4; i++)
    for (j=0; j<4; j++)
      m[i][j] = (i==j);
  rotatewx(m,al);
  rotatewy(m,be);
  rotatewz(m,de);
  rotatexy(m,ze);
  rotatexz(m,et);
  rotateyz(m,th);
}


/* Compute the rotation matrix m from the 4d rotation angles. */
static void rotateall4d(float ze, float et, float th, float m[4][4])
{
  int i, j;

  for (i=0; i<4; i++)
    for (j=0; j<4; j++)
      m[i][j] = (i==j);
  rotatexy(m,ze);
  rotatexz(m,et);
  rotateyz(m,th);
}


/* Multiply two rotation matrices: o=m*n. */
static void mult_rotmat(float m[4][4], float n[4][4], float o[4][4])
{
  int i, j, k;

  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
    {
      o[i][j] = 0.0;
      for (k=0; k<4; k++)
        o[i][j] += m[i][k]*n[k][j];
    }
  }
}


/* Compute a 4D rotation matrix from two unit quaternions. */
static void quats_to_rotmat(float p[4], float q[4], float m[4][4])
{
  double al, be, de, ze, et, th;
  double r00, r01, r02, r12, r22;

  r00 = 1.0-2.0*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0-2.0*(p[1]*p[1]+p[0]*p[0]);

  al = atan2(-r12,r22)*180.0/M_PI;
  be = atan2(r02,sqrt(r00*r00+r01*r01))*180.0/M_PI;
  de = atan2(-r01,r00)*180.0/M_PI;

  r00 = 1.0-2.0*(q[1]*q[1]+q[2]*q[2]);
  r01 = 2.0*(q[0]*q[1]+q[2]*q[3]);
  r02 = 2.0*(q[2]*q[0]-q[1]*q[3]);
  r12 = 2.0*(q[1]*q[2]+q[0]*q[3]);
  r22 = 1.0-2.0*(q[1]*q[1]+q[0]*q[0]);

  et = atan2(-r12,r22)*180.0/M_PI;
  th = atan2(r02,sqrt(r00*r00+r01*r01))*180.0/M_PI;
  ze = atan2(-r01,r00)*180.0/M_PI;

  rotateall(al,be,de,ze,et,-th,m);
}


/* Compute a fully saturated and bright color based on an angle. */
static void color(double angle, float col[4])
{
  int s;
  double t;

  if (colors == COLORS_TWOSIDED)
    return;

  if (angle >= 0.0)
    angle = fmod(angle,2.0*M_PI);
  else
    angle = fmod(angle,-2.0*M_PI);
  s = floor(angle/(M_PI/3));
  t = angle/(M_PI/3)-s;
  if (s >= 6)
    s = 0;
  switch (s)
  {
    case 0:
      col[0] = 1.0;
      col[1] = t;
      col[2] = 0.0;
      break;
    case 1:
      col[0] = 1.0-t;
      col[1] = 1.0;
      col[2] = 0.0;
      break;
    case 2:
      col[0] = 0.0;
      col[1] = 1.0;
      col[2] = t;
      break;
    case 3:
      col[0] = 0.0;
      col[1] = 1.0-t;
      col[2] = 1.0;
      break;
    case 4:
      col[0] = t;
      col[1] = 0.0;
      col[2] = 1.0;
      break;
    case 5:
      col[0] = 1.0;
      col[1] = 0.0;
      col[2] = 1.0-t;
      break;
  }
  if (display_mode == DISP_TRANSPARENT)
    col[3] = 0.7;
  else
    col[3] = 1.0;
}


/* Set up the figure-8 Klein bottle coordinates, colors, and texture. */
static void setup_figure8(ModeInfo *mi, double umin, double umax, double vmin,
                          double vmax)
{
  int i, j, k, l;
  double u, v, ur, vr;
  double cu, su, cv, sv, cv2, sv2, c2u, s2u;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      k = i*(NUMV+1)+j;
      u = -ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      if (colors == COLORS_DEPTH)
        color((cos(u)+1.0)*M_PI*2.0/3.0,kb->col[k]);
      else
        color(v,kb->col[k]);
      kb->tex[k][0] = -32*u/(2.0*M_PI);
      kb->tex[k][1] = 32*v/(2.0*M_PI);
      cu = cos(u);
      su = sin(u);
      cv = cos(v);
      sv = sin(v);
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      c2u = cos(2.0*u);
      s2u = sin(2.0*u);
      kb->x[k][0] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv;
      kb->x[k][1] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv;
      kb->x[k][2] = su*sv2+s2u*cv2;
      kb->x[k][3] = cu;
      kb->xu[k][0] = (cu*cv2-2.0*c2u*sv2)*cv;
      kb->xu[k][1] = (cu*cv2-2.0*c2u*sv2)*sv;
      kb->xu[k][2] = cu*sv2+2.0*c2u*cv2;
      kb->xu[k][3] = -su;
      kb->xv[k][0] = ((-0.5*su*sv2-0.5*s2u*cv2)*cv-
                      (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv);
      kb->xv[k][1] = ((-0.5*su*sv2-0.5*s2u*cv2)*sv+
                      (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv);
      kb->xv[k][2] = 0.5*su*cv2-0.5*s2u*sv2;
      kb->xv[k][3] = 0.0;
      for (l=0; l<4; l++)
      {
        kb->x[k][l] /= FIGURE_8_RADIUS+1.25;
        kb->xu[k][l] /= FIGURE_8_RADIUS+1.25;
        kb->xv[k][l] /= FIGURE_8_RADIUS+1.25;
      }
    }
  }
}


/* Set up the squeezed torus Klein bottle coordinates, colors, and texture. */
static void setup_squeezed_torus(ModeInfo *mi, double umin, double umax,
                                 double vmin, double vmax)
{
  int i, j, k, l;
  double u, v, ur, vr;
  double cu, su, cv, sv, cv2, sv2;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      k = i*(NUMV+1)+j;
      u = -ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      if (colors == COLORS_DEPTH)
        color((sin(u)*sin(0.5*v)+1.0)*M_PI*2.0/3.0,kb->col[k]);
      else
        color(v,kb->col[k]);
      kb->tex[k][0] = -32*u/(2.0*M_PI);
      kb->tex[k][1] = 32*v/(2.0*M_PI);
      cu = cos(u);
      su = sin(u);
      cv = cos(v);
      sv = sin(v);
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      kb->x[k][0] = (SQUEEZED_TORUS_RADIUS+cu)*cv;
      kb->x[k][1] = (SQUEEZED_TORUS_RADIUS+cu)*sv;
      kb->x[k][2] = su*cv2;
      kb->x[k][3] = su*sv2;
      kb->xu[k][0] = -su*cv;
      kb->xu[k][1] = -su*sv;
      kb->xu[k][2] = cu*cv2;
      kb->xu[k][3] = cu*sv2;
      kb->xv[k][0] = -(SQUEEZED_TORUS_RADIUS+cu)*sv;
      kb->xv[k][1] = (SQUEEZED_TORUS_RADIUS+cu)*cv;
      kb->xv[k][2] = -0.5*su*sv2;
      kb->xv[k][3] = 0.5*su*cv2;
      for (l=0; l<4; l++)
      {
        kb->x[k][l] /= SQUEEZED_TORUS_RADIUS+1.25;
        kb->xu[k][l] /= SQUEEZED_TORUS_RADIUS+1.25;
        kb->xv[k][l] /= SQUEEZED_TORUS_RADIUS+1.25;
      }
    }
  }
}


/* Set up the Lawson Klein bottle coordinates, colors, and texture. */
static void setup_lawson(ModeInfo *mi, double umin, double umax, double vmin,
                         double vmax)
{
  int i, j, k;
  double u, v, ur, vr;
  double cu, su, cv, sv, cv2, sv2;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMV; i++)
  {
    for (j=0; j<=NUMU; j++)
    {
      k = i*(NUMU+1)+j;
      u = -ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      if (colors == COLORS_DEPTH)
        color((sin(u)*cos(0.5*v)+1.0)*M_PI*2.0/3.0,kb->col[k]);
      else
        color(v,kb->col[k]);
      kb->tex[k][0] = -32*u/(2.0*M_PI);
      kb->tex[k][1] = 32*v/(2.0*M_PI);
      cu = cos(u);
      su = sin(u);
      cv = cos(v);
      sv = sin(v);
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      kb->x[k][0] = cu*cv;
      kb->x[k][1] = cu*sv;
      kb->x[k][2] = su*sv2;
      kb->x[k][3] = su*cv2;
      kb->xu[k][0] = -su*cv;
      kb->xu[k][1] = -su*sv;
      kb->xu[k][2] = cu*sv2;
      kb->xu[k][3] = cu*cv2;
      kb->xv[k][0] = -cu*sv;
      kb->xv[k][1] = cu*cv;
      kb->xv[k][2] = su*cv2*0.5;
      kb->xv[k][3] = -su*sv2*0.5;
    }
  }
}


/* Draw a figure-8 Klein bottle projected into 3D. */
static int figure8(ModeInfo *mi, double umin, double umax, double vmin,
                   double vmax)
{
  int polys = 0;
  static const GLfloat mat_diff_red[]         = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_green[]       = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_trans_red[]   = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_green[] = { 0.0, 1.0, 0.0, 0.7 };
  float p[3], pu[3], pv[3], pm[3], n[3], b[3], mat[4][4];
  int i, j, k, l, m, o;
  double u, v;
  double xx[4], xxu[4], xxv[4], y[4], yu[4], yv[4];
  double q, r, s, t;
  double cu, su, cv, sv, cv2, sv2, c2u, s2u;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    /* Compute the rotation that rotates the Klein bottle in 4D without the
       trackball rotations. */
    rotateall4d(kb->zeta,kb->eta,kb->theta,mat);

    u = kb->umove;
    v = kb->vmove;
    cu = cos(u);
    su = sin(u);
    cv = cos(v);
    sv = sin(v);
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    c2u = cos(2.0*u);
    s2u = sin(2.0*u);
    xx[0] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv;
    xx[1] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv;
    xx[2] = su*sv2+s2u*cv2;
    xx[3] = cu;
    xxu[0] = (cu*cv2-2.0*c2u*sv2)*cv;
    xxu[1] = (cu*cv2-2.0*c2u*sv2)*sv;
    xxu[2] = cu*sv2+2.0*c2u*cv2;
    xxu[3] = -su;
    xxv[0] = ((-0.5*su*sv2-0.5*s2u*cv2)*cv-
              (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv);
    xxv[1] = ((-0.5*su*sv2-0.5*s2u*cv2)*sv+
              (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv);
    xxv[2] = 0.5*su*cv2-0.5*s2u*sv2;
    xxv[3] = 0.0;
    for (l=0; l<4; l++)
    {
      xx[l] /= FIGURE_8_RADIUS+1.25;
      xxu[l] /= FIGURE_8_RADIUS+1.25;
      xxv[l] /= FIGURE_8_RADIUS+1.25;
    }
    for (l=0; l<4; l++)
    {
      y[l] = (mat[l][0]*xx[0]+mat[l][1]*xx[1]+
              mat[l][2]*xx[2]+mat[l][3]*xx[3]);
      yu[l] = (mat[l][0]*xxu[0]+mat[l][1]*xxu[1]+
               mat[l][2]*xxu[2]+mat[l][3]*xxu[3]);
      yv[l] = (mat[l][0]*xxv[0]+mat[l][1]*xxv[1]+
               mat[l][2]*xxv[2]+mat[l][3]*xxv[3]);
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (l=0; l<3; l++)
      {
        p[l] = y[l]+kb->offset4d[l];
        pu[l] = yu[l];
        pv[l] = yv[l];
      }
    }
    else
    {
      s = y[3]+kb->offset4d[3];
      q = 1.0/s;
      t = q*q;
      for (l=0; l<3; l++)
      {
        r = y[l]+kb->offset4d[l];
        p[l] = r*q;
        pu[l] = (yu[l]*s-r*yu[3])*t;
        pv[l] = (yv[l]*s-r*yv[3])*t;
      }
    }
    n[0] = pu[1]*pv[2]-pu[2]*pv[1];
    n[1] = pu[2]*pv[0]-pu[0]*pv[2];
    n[2] = pu[0]*pv[1]-pu[1]*pv[0];
    t = 1.0/(kb->side*4.0*sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
    n[0] *= t;
    n[1] *= t;
    n[2] *= t;
    pm[0] = pu[0]*kb->dumove+pv[0]*kb->dvmove;
    pm[1] = pu[1]*kb->dumove+pv[1]*kb->dvmove;
    pm[2] = pu[2]*kb->dumove+pv[2]*kb->dvmove;
    t = 1.0/(4.0*sqrt(pm[0]*pm[0]+pm[1]*pm[1]+pm[2]*pm[2]));
    pm[0] *= t;
    pm[1] *= t;
    pm[2] *= t;
    b[0] = n[1]*pm[2]-n[2]*pm[1];
    b[1] = n[2]*pm[0]-n[0]*pm[2];
    b[2] = n[0]*pm[1]-n[1]*pm[0];
    t = 1.0/(4.0*sqrt(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]));
    b[0] *= t;
    b[1] *= t;
    b[2] *= t;

    /* Compute alpha, beta, delta from the three basis vectors.
           |  -b[0]  -b[1]  -b[2] |
       m = |   n[0]   n[1]   n[2] |
           | -pm[0] -pm[1] -pm[2] |
    */
    kb->alpha = atan2(-n[2],-pm[2])*180/M_PI;
    kb->beta = atan2(-b[2],sqrt(b[0]*b[0]+b[1]*b[1]))*180/M_PI;
    kb->delta = atan2(b[1],-b[0])*180/M_PI;

    /* Compute the rotation that rotates the Klein bottle in 4D. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,mat);

    u = kb->umove;
    v = kb->vmove;
    cu = cos(u);
    su = sin(u);
    cv = cos(v);
    sv = sin(v);
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    /*c2u = cos(2.0*u);*/
    s2u = sin(2.0*u);
    xx[0] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv;
    xx[1] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv;
    xx[2] = su*sv2+s2u*cv2;
    xx[3] = cu;
    for (l=0; l<4; l++)
      xx[l] /= FIGURE_8_RADIUS+1.25;
    for (l=0; l<4; l++)
    {
      r = 0.0;
      for (m=0; m<4; m++)
        r += mat[l][m]*xx[m];
      y[l] = r;
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (l=0; l<3; l++)
        p[l] = y[l]+kb->offset4d[l];
    }
    else
    {
      s = y[3]+kb->offset4d[3];
      for (l=0; l<3; l++)
        p[l] = (y[l]+kb->offset4d[l])/s;
    }

    kb->offset3d[0] = -p[0];
    kb->offset3d[1] = -p[1]-DELTAY;
    kb->offset3d[2] = -p[2];
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  /* Project the points from 4D to 3D. */
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      o = i*(NUMV+1)+j;
      for (l=0; l<4; l++)
      {
        y[l] = (mat[l][0]*kb->x[o][0]+mat[l][1]*kb->x[o][1]+
                mat[l][2]*kb->x[o][2]+mat[l][3]*kb->x[o][3]);
        yu[l] = (mat[l][0]*kb->xu[o][0]+mat[l][1]*kb->xu[o][1]+
                 mat[l][2]*kb->xu[o][2]+mat[l][3]*kb->xu[o][3]);
        yv[l] = (mat[l][0]*kb->xv[o][0]+mat[l][1]*kb->xv[o][1]+
                 mat[l][2]*kb->xv[o][2]+mat[l][3]*kb->xv[o][3]);
      }
      if (projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          kb->pp[o][l] = (y[l]+kb->offset4d[l])+kb->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+kb->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+kb->offset4d[l];
          kb->pp[o][l] = r*q+kb->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      kb->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      kb->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      kb->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(kb->pn[o][0]*kb->pn[o][0]+kb->pn[o][1]*kb->pn[o][1]+
                   kb->pn[o][2]*kb->pn[o][2]);
      kb->pn[o][0] *= t;
      kb->pn[o][1] *= t;
      kb->pn[o][2] *= t;
    }
  }

  if (colors == COLORS_TWOSIDED)
  {
    glColor3fv(mat_diff_red);
    if (display_mode == DISP_TRANSPARENT)
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_red);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_green);
    }
    else
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_red);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_green);
    }
  }
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);

  for (i=0; i<NUMU; i++)
  {
    if (appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
      continue;
    if (display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=NUMV; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = (i+k);
        m = j;
        o = l*(NUMV+1)+m;
        glNormal3fv(kb->pn[o]);
        glTexCoord2fv(kb->tex[o]);
        if (colors != COLORS_TWOSIDED)
        {
          glColor3fv(kb->col[o]);
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,kb->col[o]);
        }
        glVertex3fv(kb->pp[o]);
        polys++;
      }
    }
    glEnd();
  }
  polys /= 2;
  return polys;
}


/* Draw a squeezed torus Klein bottle projected into 3D. */
static int squeezed_torus(ModeInfo *mi, double umin, double umax, double vmin,
                          double vmax)
{
  int polys = 0;
  static const GLfloat mat_diff_red[]         = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_green[]       = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_trans_red[]   = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_green[] = { 0.0, 1.0, 0.0, 0.7 };
  float p[3], pu[3], pv[3], pm[3], n[3], b[3], mat[4][4];
  int i, j, k, l, m, o;
  double u, v;
  double xx[4], xxu[4], xxv[4], y[4], yu[4], yv[4];
  double q, r, s, t;
  double cu, su, cv, sv, cv2, sv2;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    /* Compute the rotation that rotates the Klein bottle in 4D without the
       trackball rotations. */
    rotateall4d(kb->zeta,kb->eta,kb->theta,mat);

    u = kb->umove;
    v = kb->vmove;
    cu = cos(u);
    su = sin(u);
    cv = cos(v);
    sv = sin(v);
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    xx[0] = (SQUEEZED_TORUS_RADIUS+cu)*cv;
    xx[1] = (SQUEEZED_TORUS_RADIUS+cu)*sv;
    xx[2] = su*cv2;
    xx[3] = su*sv2;
    xxu[0] = -su*cv;
    xxu[1] = -su*sv;
    xxu[2] = cu*cv2;
    xxu[3] = cu*sv2;
    xxv[0] = -(SQUEEZED_TORUS_RADIUS+cu)*sv;
    xxv[1] = (SQUEEZED_TORUS_RADIUS+cu)*cv;
    xxv[2] = -0.5*su*sv2;
    xxv[3] = 0.5*su*cv2;
    for (l=0; l<4; l++)
    {
      xx[l] /= SQUEEZED_TORUS_RADIUS+1.25;
      xxu[l] /= SQUEEZED_TORUS_RADIUS+1.25;
      xxv[l] /= SQUEEZED_TORUS_RADIUS+1.25;
    }
    for (l=0; l<4; l++)
    {
      y[l] = (mat[l][0]*xx[0]+mat[l][1]*xx[1]+
              mat[l][2]*xx[2]+mat[l][3]*xx[3]);
      yu[l] = (mat[l][0]*xxu[0]+mat[l][1]*xxu[1]+
               mat[l][2]*xxu[2]+mat[l][3]*xxu[3]);
      yv[l] = (mat[l][0]*xxv[0]+mat[l][1]*xxv[1]+
               mat[l][2]*xxv[2]+mat[l][3]*xxv[3]);
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (l=0; l<3; l++)
      {
        p[l] = y[l]+kb->offset4d[l];
        pu[l] = yu[l];
        pv[l] = yv[l];
      }
    }
    else
    {
      s = y[3]+kb->offset4d[3];
      q = 1.0/s;
      t = q*q;
      for (l=0; l<3; l++)
      {
        r = y[l]+kb->offset4d[l];
        p[l] = r*q;
        pu[l] = (yu[l]*s-r*yu[3])*t;
        pv[l] = (yv[l]*s-r*yv[3])*t;
      }
    }
    n[0] = pu[1]*pv[2]-pu[2]*pv[1];
    n[1] = pu[2]*pv[0]-pu[0]*pv[2];
    n[2] = pu[0]*pv[1]-pu[1]*pv[0];
    t = 1.0/(kb->side*4.0*sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
    n[0] *= t;
    n[1] *= t;
    n[2] *= t;
    pm[0] = pu[0]*kb->dumove+pv[0]*kb->dvmove;
    pm[1] = pu[1]*kb->dumove+pv[1]*kb->dvmove;
    pm[2] = pu[2]*kb->dumove+pv[2]*kb->dvmove;
    t = 1.0/(4.0*sqrt(pm[0]*pm[0]+pm[1]*pm[1]+pm[2]*pm[2]));
    pm[0] *= t;
    pm[1] *= t;
    pm[2] *= t;
    b[0] = n[1]*pm[2]-n[2]*pm[1];
    b[1] = n[2]*pm[0]-n[0]*pm[2];
    b[2] = n[0]*pm[1]-n[1]*pm[0];
    t = 1.0/(4.0*sqrt(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]));
    b[0] *= t;
    b[1] *= t;
    b[2] *= t;

    /* Compute alpha, beta, delta from the three basis vectors.
           |  -b[0]  -b[1]  -b[2] |
       m = |   n[0]   n[1]   n[2] |
           | -pm[0] -pm[1] -pm[2] |
    */
    kb->alpha = atan2(-n[2],-pm[2])*180/M_PI;
    kb->beta = atan2(-b[2],sqrt(b[0]*b[0]+b[1]*b[1]))*180/M_PI;
    kb->delta = atan2(b[1],-b[0])*180/M_PI;

    /* Compute the rotation that rotates the Klein bottle in 4D. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,mat);

    u = kb->umove;
    v = kb->vmove;
    cu = cos(u);
    su = sin(u);
    cv = cos(v);
    sv = sin(v);
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    xx[0] = (SQUEEZED_TORUS_RADIUS+cu)*cv;
    xx[1] = (SQUEEZED_TORUS_RADIUS+cu)*sv;
    xx[2] = su*cv2;
    xx[3] = su*sv2;
    for (l=0; l<4; l++)
      xx[l] /= SQUEEZED_TORUS_RADIUS+1.25;
    for (l=0; l<4; l++)
    {
      r = 0.0;
      for (m=0; m<4; m++)
        r += mat[l][m]*xx[m];
      y[l] = r;
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (l=0; l<3; l++)
        p[l] = y[l]+kb->offset4d[l];
    }
    else
    {
      s = y[3]+kb->offset4d[3];
      for (l=0; l<3; l++)
        p[l] = (y[l]+kb->offset4d[l])/s;
    }

    kb->offset3d[0] = -p[0];
    kb->offset3d[1] = -p[1]-DELTAY;
    kb->offset3d[2] = -p[2];
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  /* Project the points from 4D to 3D. */
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      o = i*(NUMV+1)+j;
      for (l=0; l<4; l++)
      {
        y[l] = (mat[l][0]*kb->x[o][0]+mat[l][1]*kb->x[o][1]+
                mat[l][2]*kb->x[o][2]+mat[l][3]*kb->x[o][3]);
        yu[l] = (mat[l][0]*kb->xu[o][0]+mat[l][1]*kb->xu[o][1]+
                 mat[l][2]*kb->xu[o][2]+mat[l][3]*kb->xu[o][3]);
        yv[l] = (mat[l][0]*kb->xv[o][0]+mat[l][1]*kb->xv[o][1]+
                 mat[l][2]*kb->xv[o][2]+mat[l][3]*kb->xv[o][3]);
      }
      if (projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          kb->pp[o][l] = (y[l]+kb->offset4d[l])+kb->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+kb->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+kb->offset4d[l];
          kb->pp[o][l] = r*q+kb->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      kb->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      kb->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      kb->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(kb->pn[o][0]*kb->pn[o][0]+kb->pn[o][1]*kb->pn[o][1]+
                   kb->pn[o][2]*kb->pn[o][2]);
      kb->pn[o][0] *= t;
      kb->pn[o][1] *= t;
      kb->pn[o][2] *= t;
    }
  }

  if (colors == COLORS_TWOSIDED)
  {
    glColor3fv(mat_diff_red);
    if (display_mode == DISP_TRANSPARENT)
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_red);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_green);
    }
    else
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_red);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_green);
    }
  }
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);

  for (i=0; i<NUMU; i++)
  {
    if (appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
      continue;
    if (display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=NUMV; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = (i+k);
        m = j;
        o = l*(NUMV+1)+m;
        glNormal3fv(kb->pn[o]);
        glTexCoord2fv(kb->tex[o]);
        if (colors != COLORS_TWOSIDED)
        {
          glColor3fv(kb->col[o]);
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,kb->col[o]);
        }
        glVertex3fv(kb->pp[o]);
        polys++;
      }
    }
    glEnd();
  }
  polys /= 2;
  return polys;
}


/* Draw a Lawson Klein bottle projected into 3D. */
static int lawson(ModeInfo *mi, double umin, double umax, double vmin,
                  double vmax)
{
  int polys = 0;
  static const GLfloat mat_diff_red[]         = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_green[]       = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_trans_red[]   = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_green[] = { 0.0, 1.0, 0.0, 0.7 };
  float p[3], pu[3], pv[3], pm[3], n[3], b[3], mat[4][4];
  int i, j, k, l, m, o;
  double u, v;
  double cu, su, cv, sv, cv2, sv2;
  double xx[4], xxu[4], xxv[4], y[4], yu[4], yv[4];
  double q, r, s, t;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    /* Compute the rotation that rotates the Klein bottle in 4D without the
       trackball rotations. */
    rotateall4d(kb->zeta,kb->eta,kb->theta,mat);

    u = kb->umove;
    v = kb->vmove;
    cu = cos(u);
    su = sin(u);
    cv = cos(v);
    sv = sin(v);
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    xx[0] = cu*cv;
    xx[1] = cu*sv;
    xx[2] = su*sv2;
    xx[3] = su*cv2;
    xxu[0] = -su*cv;
    xxu[1] = -su*sv;
    xxu[2] = cu*sv2;
    xxu[3] = cu*cv2;
    xxv[0] = -cu*sv;
    xxv[1] = cu*cv;
    xxv[2] = su*cv2*0.5;
    xxv[3] = -su*sv2*0.5;
    for (l=0; l<4; l++)
    {
      y[l] = (mat[l][0]*xx[0]+mat[l][1]*xx[1]+
              mat[l][2]*xx[2]+mat[l][3]*xx[3]);
      yu[l] = (mat[l][0]*xxu[0]+mat[l][1]*xxu[1]+
               mat[l][2]*xxu[2]+mat[l][3]*xxu[3]);
      yv[l] = (mat[l][0]*xxv[0]+mat[l][1]*xxv[1]+
               mat[l][2]*xxv[2]+mat[l][3]*xxv[3]);
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (l=0; l<3; l++)
      {
        p[l] = y[l]+kb->offset4d[l];
        pu[l] = yu[l];
        pv[l] = yv[l];
      }
    }
    else
    {
      s = y[3]+kb->offset4d[3];
      q = 1.0/s;
      t = q*q;
      for (l=0; l<3; l++)
      {
        r = y[l]+kb->offset4d[l];
        p[l] = r*q;
        pu[l] = (yu[l]*s-r*yu[3])*t;
        pv[l] = (yv[l]*s-r*yv[3])*t;
      }
    }
    n[0] = pu[1]*pv[2]-pu[2]*pv[1];
    n[1] = pu[2]*pv[0]-pu[0]*pv[2];
    n[2] = pu[0]*pv[1]-pu[1]*pv[0];
    t = 1.0/(kb->side*4.0*sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
    n[0] *= t;
    n[1] *= t;
    n[2] *= t;
    pm[0] = pu[0]*kb->dumove+pv[0]*kb->dvmove;
    pm[1] = pu[1]*kb->dumove+pv[1]*kb->dvmove;
    pm[2] = pu[2]*kb->dumove+pv[2]*kb->dvmove;
    t = 1.0/(4.0*sqrt(pm[0]*pm[0]+pm[1]*pm[1]+pm[2]*pm[2]));
    pm[0] *= t;
    pm[1] *= t;
    pm[2] *= t;
    b[0] = n[1]*pm[2]-n[2]*pm[1];
    b[1] = n[2]*pm[0]-n[0]*pm[2];
    b[2] = n[0]*pm[1]-n[1]*pm[0];
    t = 1.0/(4.0*sqrt(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]));
    b[0] *= t;
    b[1] *= t;
    b[2] *= t;

    /* Compute alpha, beta, delta from the three basis vectors.
           |  -b[0]  -b[1]  -b[2] |
       m = |   n[0]   n[1]   n[2] |
           | -pm[0] -pm[1] -pm[2] |
    */
    kb->alpha = atan2(-n[2],-pm[2])*180/M_PI;
    kb->beta = atan2(-b[2],sqrt(b[0]*b[0]+b[1]*b[1]))*180/M_PI;
    kb->delta = atan2(b[1],-b[0])*180/M_PI;

    /* Compute the rotation that rotates the Klein bottle in 4D. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,mat);

    u = kb->umove;
    v = kb->vmove;
    cu = cos(u);
    su = sin(u);
    cv = cos(v);
    sv = sin(v);
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    xx[0] = cu*cv;
    xx[1] = cu*sv;
    xx[2] = su*sv2;
    xx[3] = su*cv2;
    for (l=0; l<4; l++)
    {
      r = 0.0;
      for (m=0; m<4; m++)
        r += mat[l][m]*xx[m];
      y[l] = r;
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (l=0; l<3; l++)
        p[l] = y[l]+kb->offset4d[l];
    }
    else
    {
      s = y[3]+kb->offset4d[3];
      for (l=0; l<3; l++)
        p[l] = (y[l]+kb->offset4d[l])/s;
    }

    kb->offset3d[0] = -p[0];
    kb->offset3d[1] = -p[1]-DELTAY;
    kb->offset3d[2] = -p[2];
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  /* Project the points from 4D to 3D. */
  for (i=0; i<=NUMV; i++)
  {
    for (j=0; j<=NUMU; j++)
    {
      o = i*(NUMU+1)+j;
      for (l=0; l<4; l++)
      {
        y[l] = (mat[l][0]*kb->x[o][0]+mat[l][1]*kb->x[o][1]+
                mat[l][2]*kb->x[o][2]+mat[l][3]*kb->x[o][3]);
        yu[l] = (mat[l][0]*kb->xu[o][0]+mat[l][1]*kb->xu[o][1]+
                 mat[l][2]*kb->xu[o][2]+mat[l][3]*kb->xu[o][3]);
        yv[l] = (mat[l][0]*kb->xv[o][0]+mat[l][1]*kb->xv[o][1]+
                 mat[l][2]*kb->xv[o][2]+mat[l][3]*kb->xv[o][3]);
      }
      if (projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          kb->pp[o][l] = (y[l]+kb->offset4d[l])+kb->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+kb->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+kb->offset4d[l];
          kb->pp[o][l] = r*q+kb->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      kb->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      kb->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      kb->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(kb->pn[o][0]*kb->pn[o][0]+kb->pn[o][1]*kb->pn[o][1]+
                   kb->pn[o][2]*kb->pn[o][2]);
      kb->pn[o][0] *= t;
      kb->pn[o][1] *= t;
      kb->pn[o][2] *= t;
    }
  }

  if (colors == COLORS_TWOSIDED)
  {
    glColor3fv(mat_diff_red);
    if (display_mode == DISP_TRANSPARENT)
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_red);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_green);
    }
    else
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_red);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_green);
    }
  }
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);

  for (i=0; i<NUMV; i++)
  {
    if (appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
      continue;
    if (display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=NUMU; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = (i+k);
        m = j;
        o = l*(NUMU+1)+m;
        glNormal3fv(kb->pn[o]);
        glTexCoord2fv(kb->tex[o]);
        if (colors != COLORS_TWOSIDED)
        {
          glColor3fv(kb->col[o]);
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,kb->col[o]);
        }
        glVertex3fv(kb->pp[o]);
        polys++;
      }
    }
    glEnd();
  }
  polys /= 2;
  return polys;
}


/* Generate a texture image that shows the orientation reversal. */
static void gen_texture(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  glGenTextures(1,&kb->tex_name);
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,TEX_DIMENSION,TEX_DIMENSION,0,
               GL_LUMINANCE,GL_UNSIGNED_BYTE,texture);
}


static void init(ModeInfo *mi)
{
  static const GLfloat light_ambient[]  = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_diffuse[]  = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 };
  static const GLfloat mat_specular[]   = { 1.0, 1.0, 1.0, 1.0 };
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (walk_speed == 0.0)
    walk_speed = 20.0;

  if (view == VIEW_TURN)
  {
    kb->alpha = frand(360.0);
    kb->beta = frand(360.0);
    kb->delta = frand(360.0);
  }
  else
  {
    kb->alpha = 0.0;
    kb->beta = 0.0;
    kb->delta = 0.0;
  }
  kb->zeta = 0.0;
  if (bottle_type == KLEIN_BOTTLE_FIGURE_8 ||
      bottle_type == KLEIN_BOTTLE_SQUEEZED_TORUS)
    kb->eta = 0.0;
  else
    kb->eta = 45.0;
  kb->theta = 0.0;
  kb->umove = frand(2.0*M_PI);
  kb->vmove = frand(2.0*M_PI);
  kb->dumove = 0.0;
  kb->dvmove = 0.0;
  kb->side = 1;

  if (bottle_type == KLEIN_BOTTLE_FIGURE_8)
  {
    kb->offset4d[0] = 0.0;
    kb->offset4d[1] = 0.0;
    kb->offset4d[2] = 0.0;
    kb->offset4d[3] = 1.5;
    kb->offset3d[0] = 0.0;
    kb->offset3d[1] = 0.0;
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
      kb->offset3d[2] = -2.1;
    else
      kb->offset3d[2] = -1.9;
    kb->offset3d[3] = 0.0;
  }
  else if (bottle_type == KLEIN_BOTTLE_SQUEEZED_TORUS)
  {
    kb->offset4d[0] = 0.0;
    kb->offset4d[1] = 0.0;
    kb->offset4d[2] = 0.0;
    kb->offset4d[3] = 1.4;
    kb->offset3d[0] = 0.0;
    kb->offset3d[1] = 0.0;
    kb->offset3d[2] = -2.0;
    kb->offset3d[3] = 0.0;
  }
  else /* bottle_type == KLEIN_BOTTLE_LAWSON */
  {
    kb->offset4d[0] = 0.0;
    kb->offset4d[1] = 0.0;
    kb->offset4d[2] = 0.0;
    if (projection_4d == DISP_4D_PERSPECTIVE &&
        projection_3d == DISP_3D_ORTHOGRAPHIC)
      kb->offset4d[3] = 1.5;
    else
      kb->offset4d[3] = 1.1;
    kb->offset3d[0] = 0.0;
    kb->offset3d[1] = 0.0;
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
      kb->offset3d[2] = -2.0;
    else
      kb->offset3d[2] = -5.0;
    kb->offset3d[3] = 0.0;
  }

  gen_texture(mi);
  if (bottle_type == KLEIN_BOTTLE_FIGURE_8)
    setup_figure8(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else if (bottle_type == KLEIN_BOTTLE_SQUEEZED_TORUS)
    setup_squeezed_torus(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else /* bottle_type == KLEIN_BOTTLE_LAWSON */
    setup_lawson(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);

  if (marks)
    glEnable(GL_TEXTURE_2D);
  else
    glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (projection_3d == DISP_3D_PERSPECTIVE ||
      view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    if (view == VIEW_WALK || view == VIEW_WALKTURN)
      gluPerspective(60.0,1.0,0.01,10.0);
    else
      gluPerspective(60.0,1.0,0.1,10.0);
  }
  else
  {
    glOrtho(-1.0,1.0,-1.0,1.0,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  if (display_mode == DISP_WIREFRAME)
    display_mode = DISP_SURFACE;
# endif

  if (display_mode == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glShadeModel(GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else if (display_mode == DISP_TRANSPARENT)
  {
    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }
  else  /* display_mode == DISP_WIREFRAME */
  {
    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_BLEND);
  }
}


/* Redisplay the Klein bottle. */
static void display_klein(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->button_pressed)
  {
    if (view == VIEW_TURN)
    {
      kb->alpha += speed_wx * kb->speed_scale;
      if (kb->alpha >= 360.0)
        kb->alpha -= 360.0;
      kb->beta += speed_wy * kb->speed_scale;
      if (kb->beta >= 360.0)
        kb->beta -= 360.0;
      kb->delta += speed_wz * kb->speed_scale;
      if (kb->delta >= 360.0)
        kb->delta -= 360.0;
      kb->zeta += speed_xy * kb->speed_scale;
      if (kb->zeta >= 360.0)
        kb->zeta -= 360.0;
      kb->eta += speed_xz * kb->speed_scale;
      if (kb->eta >= 360.0)
        kb->eta -= 360.0;
      kb->theta += speed_yz * kb->speed_scale;
      if (kb->theta >= 360.0)
        kb->theta -= 360.0;
    }
    if (view == VIEW_WALKTURN)
    {
      kb->zeta += speed_xy * kb->speed_scale;
      if (kb->zeta >= 360.0)
        kb->zeta -= 360.0;
      kb->eta += speed_xz * kb->speed_scale;
      if (kb->eta >= 360.0)
        kb->eta -= 360.0;
      kb->theta += speed_yz * kb->speed_scale;
      if (kb->theta >= 360.0)
        kb->theta -= 360.0;
    }
    if (view == VIEW_WALK || view == VIEW_WALKTURN)
    {
      kb->dvmove = cos(walk_direction*M_PI/180.0)*walk_speed*M_PI/4096.0;
      kb->vmove += kb->dvmove;
      if (kb->vmove >= 2.0*M_PI)
      {
        kb->vmove -= 2.0*M_PI;
        kb->umove = 2.0*M_PI-kb->umove;
        kb->side = -kb->side;
      }
      kb->dumove = (kb->side*sin(walk_direction*M_PI/180.0)*
                    walk_speed*M_PI/4096.0);
      kb->umove += kb->dumove;
      if (kb->umove >= 2.0*M_PI)
        kb->umove -= 2.0*M_PI;
      if (kb->umove < 0.0)
        kb->umove += 2.0*M_PI;
    }
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (projection_3d == DISP_3D_PERSPECTIVE ||
      view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    if (view == VIEW_WALK || view == VIEW_WALKTURN)
      gluPerspective(60.0,kb->aspect,0.01,10.0);
    else
      gluPerspective(60.0,kb->aspect,0.1,10.0);
  }
  else
  {
    if (kb->aspect >= 1.0)
      glOrtho(-kb->aspect,kb->aspect,-1.0,1.0,0.1,10.0);
    else
      glOrtho(-1.0,1.0,-1.0/kb->aspect,1.0/kb->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (bottle_type == KLEIN_BOTTLE_FIGURE_8)
    mi->polygon_count = figure8(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else if (bottle_type == KLEIN_BOTTLE_SQUEEZED_TORUS)
    mi->polygon_count = squeezed_torus(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else /* bottle_type == KLEIN_BOTTLE_LAWSON */
    mi->polygon_count = lawson(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
}


ENTRYPOINT void reshape_klein(ModeInfo *mi, int width, int height)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  kb->WindW = (GLint)width;
  kb->WindH = (GLint)height;
  glViewport(0,0,width,height);
  kb->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool klein_handle_event(ModeInfo *mi, XEvent *event)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];
  KeySym  sym = 0;
  char c = 0;

  if (event->xany.type == KeyPress || event->xany.type == KeyRelease)
    XLookupString (&event->xkey, &c, 1, &sym, 0);

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
  {
    kb->button_pressed = True;
    gltrackball_start(kb->trackballs[kb->current_trackball],
                      event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    kb->button_pressed = False;
    return True;
  }
  else if (event->xany.type == KeyPress)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      kb->current_trackball = 1;
      if (kb->button_pressed)
        gltrackball_start(kb->trackballs[kb->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == KeyRelease)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      kb->current_trackball = 0;
      if (kb->button_pressed)
        gltrackball_start(kb->trackballs[kb->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == MotionNotify && kb->button_pressed)
  {
    gltrackball_track(kb->trackballs[kb->current_trackball],
                      event->xmotion.x, event->xmotion.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }

  return False;
}


/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *    Initialize klein.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_klein(ModeInfo *mi)
{
  kleinstruct *kb;

  if (klein == NULL)
  {
    klein = (kleinstruct *)calloc(MI_NUM_SCREENS(mi),
                                              sizeof(kleinstruct));
    if (klein == NULL)
      return;
  }
  kb = &klein[MI_SCREEN(mi)];

  
  kb->trackballs[0] = gltrackball_init();
  kb->trackballs[1] = gltrackball_init();
  kb->current_trackball = 0;
  kb->button_pressed = False;

  /* Set the Klein bottle. */
  if (!strcasecmp(klein_bottle,"random"))
  {
    bottle_type = random() % NUM_KLEIN_BOTTLES;
  }
  else if (!strcasecmp(klein_bottle,"figure-8"))
  {
    bottle_type = KLEIN_BOTTLE_FIGURE_8;
  }
  else if (!strcasecmp(klein_bottle,"squeezed-torus"))
  {
    bottle_type = KLEIN_BOTTLE_SQUEEZED_TORUS;
  }
  else if (!strcasecmp(klein_bottle,"lawson"))
  {
    bottle_type = KLEIN_BOTTLE_LAWSON;
  }
  else
  {
    bottle_type = random() % NUM_KLEIN_BOTTLES;
  }

  /* Set the display mode. */
  if (!strcasecmp(mode,"random"))
  {
    display_mode = random() % NUM_DISPLAY_MODES;
  }
  else if (!strcasecmp(mode,"wireframe"))
  {
    display_mode = DISP_WIREFRAME;
  }
  else if (!strcasecmp(mode,"surface"))
  {
    display_mode = DISP_SURFACE;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    display_mode = DISP_TRANSPARENT;
  }
  else
  {
    display_mode = random() % NUM_DISPLAY_MODES;
  }

  /* Orientation marks don't make sense in wireframe mode. */
  if (display_mode == DISP_WIREFRAME)
    marks = False;

  /* Set the appearance. */
  if (!strcasecmp(appear,"random"))
  {
    appearance = random() % NUM_APPEARANCES;
  }
  else if (!strcasecmp(appear,"solid"))
  {
    appearance = APPEARANCE_SOLID;
  }
  else if (!strcasecmp(appear,"bands"))
  {
    appearance = APPEARANCE_BANDS;
  }
  else
  {
    appearance = random() % NUM_APPEARANCES;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"random"))
  {
    colors = random() % NUM_COLORS;
  }
  else if (!strcasecmp(color_mode,"two-sided"))
  {
    colors = COLORS_TWOSIDED;
  }
  else if (!strcasecmp(color_mode,"rainbow"))
  {
    colors = COLORS_RAINBOW;
  }
  else if (!strcasecmp(color_mode,"depth"))
  {
    colors = COLORS_DEPTH;
  }
  else
  {
    colors = random() % NUM_COLORS;
  }

  /* Set the view mode. */
  if (!strcasecmp(view_mode,"random"))
  {
    view = random() % NUM_VIEW_MODES;
  }
  else if (!strcasecmp(view_mode,"walk"))
  {
    view = VIEW_WALK;
  }
  else if (!strcasecmp(view_mode,"turn"))
  {
    view = VIEW_TURN;
  }
  else if (!strcasecmp(view_mode,"walk-turn"))
  {
    view = VIEW_WALKTURN;
  }
  else
  {
    view = random() % NUM_VIEW_MODES;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj_3d,"random"))
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (view == VIEW_TURN)
      projection_3d = random() % NUM_DISP_3D_MODES;
    else
      projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"perspective"))
  {
    projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"orthographic"))
  {
    projection_3d = DISP_3D_ORTHOGRAPHIC;
  }
  else
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (view == VIEW_TURN)
      projection_3d = random() % NUM_DISP_3D_MODES;
    else
      projection_3d = DISP_3D_PERSPECTIVE;
  }

  /* Set the 4d projection mode. */
  if (!strcasecmp(proj_4d,"random"))
  {
    projection_4d = random() % NUM_DISP_4D_MODES;
  }
  else if (!strcasecmp(proj_4d,"perspective"))
  {
    projection_4d = DISP_4D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_4d,"orthographic"))
  {
    projection_4d = DISP_4D_ORTHOGRAPHIC;
  }
  else
  {
    projection_4d = random() % NUM_DISP_4D_MODES;
  }

  /* Modify the speeds to a useful range in walk-and-turn mode. */
  if (view == VIEW_WALKTURN)
  {
    speed_wx *= 0.2;
    speed_wy *= 0.2;
    speed_wz *= 0.2;
    speed_xy *= 0.2;
    speed_xz *= 0.2;
    speed_yz *= 0.2;
  }

  /* make multiple screens rotate at slightly different rates. */
  kb->speed_scale = 0.9 + frand(0.3);

  if ((kb->glx_context = init_GL(mi)) != NULL)
  {
    reshape_klein(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    init(mi);
  }
  else
  {
    MI_CLEARWINDOW(mi);
  }
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
ENTRYPOINT void draw_klein(ModeInfo *mi)
{
  Display          *display = MI_DISPLAY(mi);
  Window           window = MI_WINDOW(mi);
  kleinstruct *kb;

  if (klein == NULL)
    return;
  kb = &klein[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!kb->glx_context)
    return;

  glXMakeCurrent(display,window,*(kb->glx_context));

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_klein(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed 
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void release_klein(ModeInfo *mi)
{
  if (klein != NULL)
  {
    int screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
    {
      kleinstruct *kb = &klein[screen];

      if (kb->glx_context)
        kb->glx_context = (GLXContext *)NULL;
    }
    (void) free((void *)klein);
    klein = (kleinstruct *)NULL;
  }
  FreeAllGL(mi);
}

#ifndef STANDALONE
ENTRYPOINT void change_klein(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*(kb->glx_context));
  init(mi);
}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE ("Klein", klein)

#endif /* USE_GL */
