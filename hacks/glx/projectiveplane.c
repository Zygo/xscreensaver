/* projectiveplane --- Shows a 4d embedding of the real projective plane
   that rotates in 4d or on which you can walk */

#if 0
static const char sccsid[] = "@(#)projectiveplane.c  1.1 14/01/01 xlockmore";
#endif

/* Copyright (c) 2005-2014 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 14/01/03: Initial version
 */

/*
 * This program shows a 4d embedding of the real projective plane.
 * You can walk on the projective plane, see it turn in 4d, or walk on
 * it while it turns in 4d.  The fact that the surface is an embedding
 * of the real projective plane in 4d can be seen in the depth colors
 * mode: set all rotation speeds to 0 and the projection mode to 4d
 * orthographic projection.  In its default orientation, the embedding
 * of the real projective plane will then project to the Roman
 * surface, which has three lines of self-intersection.  However, at
 * the three lines of self-intersection the parts of the surface that
 * intersect have different colors, i.e., different 4d depths.
 *
 * The real projective plane is a non-orientable surface.  To make
 * this apparent, the two-sided color mode can be used.
 * Alternatively, orientation markers (curling arrows) can be drawn as
 * a texture map on the surface of the projective plane.  While
 * walking on the projective plane, you will notice that the
 * orientation of the curling arrows changes (which it must because
 * the projective plane is non-orientable).
 *
 * The real projective plane is a model for the projective geometry in
 * 2d space.  One point can be singled out as the origin.  A line can
 * be singled out as the line at infinity, i.e., a line that lies at
 * an infinite distance to the origin.  The line at infinity is
 * topologically a circle.  Points on the line at infinity are also
 * used to model directions in projective geometry.  The origin can be
 * visualized in different manners.  When using distance colors, the
 * origin is the point that is displayed as fully saturated red, which
 * is easier to see as the center of the reddish area on the
 * projective plane.  Alternatively, when using distance bands, the
 * origin is the center of the only band that projects to a disc.
 * When using direction bands, the origin is the point where all
 * direction bands collapse to a point.  Finally, when orientation
 * markers are being displayed, the origin the the point where all
 * orientation markers are compressed to a point.  The line at
 * infinity can also be visualized in different ways.  When using
 * distance colors, the line at infinity is the line that is displayed
 * as fully saturated magenta.  When two-sided colors are used, the
 * line at infinity lies at the points where the red and green "sides"
 * of the projective plane meet (of course, the real projective plane
 * only has one side, so this is a design choice of the
 * visualization).  Alternatively, when orientation markers are being
 * displayed, the line at infinity is the place where the orientation
 * markers change their orientation.
 *
 * Note that when the projective plane is displayed with bands, the
 * orientation markers are placed in the middle of the bands.  For
 * distance bands, the bands are chosen in such a way that the band at
 * the origin is only half as wide as the remaining bands, which
 * results in a disc being displayed at the origin that has the same
 * diameter as the remaining bands.  This choice, however, also
 * implies that the band at infinity is half as wide as the other
 * bands.  Since the projective plane is attached to itself (in a
 * complicated fashion) at the line at infinity, effectively the band
 * at infinity is again as wide as the remaining bands.  However,
 * since the orientation markers are displayed in the middle of the
 * bands, this means that only one half of the orientation markers
 * will be displayed twice at the line at infinity if distance bands
 * are used.  If direction bands are used or if the projective plane
 * is displayed as a solid surface, the orientation markers are
 * displayed fully at the respective sides of the line at infinity.
 *
 * The program projects the 4d projective plane to 3d using either a
 * perspective or an orthographic projection.  Which of the two
 * alternatives looks more appealing is up to you.  However, two
 * famous surfaces are obtained if orthographic 4d projection is used:
 * The Roman surface and the cross cap.  If the projective plane is
 * rotated in 4d, the result of the projection for certain rotations
 * is a Roman surface and for certain rotations it is a cross cap.
 * The easiest way to see this is to set all rotation speeds to 0 and
 * the rotation speed around the yz plane to a value different from 0.
 * However, for any 4d rotation speeds, the projections will generally
 * cycle between the Roman surface and the cross cap.  The difference
 * is where the origin and the line at infinity will lie with respect
 * to the self-intersections in the projections to 3d.
 *
 * The projected projective plane can then be projected to the screen
 * either perspectively or orthographically.  When using the walking
 * modes, perspective projection to the screen will be used.
 *
 * There are three display modes for the projective plane: mesh
 * (wireframe), solid, or transparent.  Furthermore, the appearance of
 * the projective plane can be as a solid object or as a set of
 * see-through bands.  The bands can be distance bands, i.e., bands
 * that lie at increasing distances from the origin, or direction
 * bands, i.e., bands that lie at increasing angles with respect to
 * the origin.
 *
 * When the projective plane is displayed with direction bands, you
 * will be able to see that each direction band (modulo the "pinching"
 * at the origin) is a Moebius strip, which also shows that the
 * projective plane is non-orientable.
 *
 * Finally, the colors with with the projective plane is drawn can be
 * set to two-sided, distance, direction, or depth.  In two-sided
 * mode, the projective plane is drawn with red on one "side" and
 * green on the "other side".  As described above, the projective
 * plane only has one side, so the color jumps from red to green along
 * the line at infinity.  This mode enables you to see that the
 * projective plane is non-orientable.  In distance mode, the
 * projective plane is displayed with fully saturated colors that
 * depend on the distance of the points on the projective plane to the
 * origin.  The origin is displayed in red, the line at infinity is
 * displayed in magenta.  If the projective plane is displayed as
 * distance bands, each band will be displayed with a different color.
 * In direction mode, the projective plane is displayed with fully
 * saturated colors that depend on the angle of the points on the
 * projective plane with respect to the origin.  Angles in opposite
 * directions to the origin (e.g., 15 and 205 degrees) are displayed
 * in the same color since they are projectively equivalent.  If the
 * projective plane is displayed as direction bands, each band will be
 * displayed with a different color.  Finally, in depth mode the
 * projective plane with colors chosen depending on the 4d "depth"
 * (i.e., the w coordinate) of the points on the projective plane at
 * its default orientation in 4d.  As discussed above, this mode
 * enables you to see that the projective plane does not intersect
 * itself in 4d.
 *
 * The rotation speed for each of the six planes around which the
 * projective plane rotates can be chosen.  For the walk-and-turn
 * more, only the rotation speeds around the true 4d planes are used
 * (the xy, xz, and yz planes).
 *
 * Furthermore, in the walking modes the walking direction in the 2d
 * base square of the projective plane and the walking speed can be
 * chosen.  The walking direction is measured as an angle in degrees
 * in the 2d square that forms the coordinate system of the surface of
 * the projective plane.  A value of 0 or 180 means that the walk is
 * along a circle at a randomly chosen distance from the origin
 * (parallel to a distance band).  A value of 90 or 270 means that the
 * walk is directly from the origin to the line at infinity and back
 * (analogous to a direction band).  Any other value results in a
 * curved path from the origin to the line at infinity and back.
 *
 * This program is somewhat inspired by Thomas Banchoff's book "Beyond
 * the Third Dimension: Geometry, Computer Graphics, and Higher
 * Dimensions", Scientific American Library, 1990.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DISP_WIREFRAME             0
#define DISP_SURFACE               1
#define DISP_TRANSPARENT           2
#define NUM_DISPLAY_MODES          3

#define APPEARANCE_SOLID           0
#define APPEARANCE_DISTANCE_BANDS  1
#define APPEARANCE_DIRECTION_BANDS 2
#define NUM_APPEARANCES            3

#define COLORS_TWOSIDED            0
#define COLORS_DISTANCE            1
#define COLORS_DIRECTION           2
#define COLORS_DEPTH               3
#define NUM_COLORS                 4

#define VIEW_WALK                  0
#define VIEW_TURN                  1
#define VIEW_WALKTURN              2
#define NUM_VIEW_MODES             3

#define DISP_3D_PERSPECTIVE        0
#define DISP_3D_ORTHOGRAPHIC       1
#define NUM_DISP_3D_MODES          2

#define DISP_4D_PERSPECTIVE        0
#define DISP_4D_ORTHOGRAPHIC       1
#define NUM_DISP_4D_MODES          2

#define DEF_DISPLAY_MODE           "random"
#define DEF_APPEARANCE             "random"
#define DEF_COLORS                 "random"
#define DEF_VIEW_MODE              "random"
#define DEF_MARKS                  "False"
#define DEF_PROJECTION_3D          "random"
#define DEF_PROJECTION_4D          "random"
#define DEF_SPEEDWX                "1.1"
#define DEF_SPEEDWY                "1.3"
#define DEF_SPEEDWZ                "1.5"
#define DEF_SPEEDXY                "1.7"
#define DEF_SPEEDXZ                "1.9"
#define DEF_SPEEDYZ                "2.1"
#define DEF_WALK_DIRECTION         "83.0"
#define DEF_WALK_SPEED             "20.0"

#ifdef STANDALONE
# define DEFAULTS           "*delay:      10000 \n" \
                            "*showFPS:    False \n" \

# define refresh_projectiveplane 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifndef HAVE_COCOA
# include <X11/keysym.h>
#endif

#include "gltrackball.h"

#include <float.h>


#ifdef USE_MODULES
ModStruct projectiveplane_description =
{"projectiveplane", "init_projectiveplane", "draw_projectiveplane",
 "release_projectiveplane", "draw_projectiveplane", "change_projectiveplane",
 NULL, &projectiveplane_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Rotate a 4d embedding of the real projective plane in 4d or walk on it",
 0, NULL};

#endif


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
  {"-mode",              ".displayMode",   XrmoptionSepArg, 0 },
  {"-wireframe",         ".displayMode",   XrmoptionNoArg,  "wireframe" },
  {"-surface",           ".displayMode",   XrmoptionNoArg,  "surface" },
  {"-transparent",       ".displayMode",   XrmoptionNoArg,  "transparent" },
  {"-appearance",        ".appearance",    XrmoptionSepArg, 0 },
  {"-solid",             ".appearance",    XrmoptionNoArg,  "solid" },
  {"-distance-bands",    ".appearance",    XrmoptionNoArg,  "distance-bands" },
  {"-direction-bands",   ".appearance",    XrmoptionNoArg,  "direction-bands" },
  {"-colors",            ".colors",        XrmoptionSepArg, 0 },
  {"-twosided-colors",   ".colors",        XrmoptionNoArg,  "two-sided" },
  {"-distance-colors",   ".colors",        XrmoptionNoArg,  "distance" },
  {"-direction-colors",  ".colors",        XrmoptionNoArg,  "direction" },
  {"-depth-colors",      ".colors",        XrmoptionNoArg,  "depth" },
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

ENTRYPOINT ModeSpecOpt projectiveplane_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Offset by which we walk above the projective plane */
#define DELTAY  0.01

/* Number of subdivisions of the projective plane */
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
  int side, dir;
  /* The viewing offset in 4d */
  float offset4d[4];
  /* The viewing offset in 3d */
  float offset3d[4];
  /* The 4d coordinates of the projective plane and their derivatives */
  float x[(NUMU+1)*(NUMV+1)][4];
  float xu[(NUMU+1)*(NUMV+1)][4];
  float xv[(NUMU+1)*(NUMV+1)][4];
  float pp[(NUMU+1)*(NUMV+1)][3];
  float pn[(NUMU+1)*(NUMV+1)][3];
  /* The precomputed colors of the projective plane */
  float col[(NUMU+1)*(NUMV+1)][4];
  /* The precomputed texture coordinates of the projective plane */
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
} projectiveplanestruct;

static projectiveplanestruct *projectiveplane = (projectiveplanestruct *) NULL;


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


/* Set up the projective plane coordinates, colors, and texture. */
static void setup_projective_plane(ModeInfo *mi, double umin, double umax,
                                   double vmin, double vmax)
{
  int i, j, k;
  double u, v, ur, vr;
  double cu, su, cv2, sv2, cv4, sv4, c2u, s2u;
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMV; i++)
  {
    for (j=0; j<=NUMU; j++)
    {
      k = i*(NUMU+1)+j;
      if (appearance != APPEARANCE_DIRECTION_BANDS)
        u = -ur*j/NUMU+umin;
      else
        u = ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      cu = cos(u);
      su = sin(u);
      c2u = cos(2.0*u);
      s2u = sin(2.0*u);
      sv2 = sin(0.5*v);
      cv4 = cos(0.25*v);
      sv4 = sin(0.25*v);
      if (colors == COLORS_DEPTH)
        color(((su*su*sv4*sv4-cv4*cv4)+1.0)*M_PI*2.0/3.0,pp->col[k]);
      else if (colors == COLORS_DIRECTION)
        color(2.0*M_PI+fmod(2.0*u,2.0*M_PI),pp->col[k]);
      else /* colors == COLORS_DISTANCE */
        color(v*(5.0/6.0),pp->col[k]);
      pp->tex[k][0] = -32*u/(2.0*M_PI);
      if (appearance != APPEARANCE_DISTANCE_BANDS)
        pp->tex[k][1] = 32*v/(2.0*M_PI);
      else
        pp->tex[k][1] = 32*v/(2.0*M_PI)-0.5;
      pp->x[k][0] = 0.5*s2u*sv4*sv4;
      pp->x[k][1] = 0.5*su*sv2;
      pp->x[k][2] = 0.5*cu*sv2;
      pp->x[k][3] = 0.5*(su*su*sv4*sv4-cv4*cv4);
      /* Avoid degenerate tangential plane basis vectors. */
      if (v < FLT_EPSILON)
        v = FLT_EPSILON;
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      sv4 = sin(0.25*v);
      pp->xu[k][0] = c2u*sv4*sv4;
      pp->xu[k][1] = 0.5*cu*sv2;
      pp->xu[k][2] = -0.5*su*sv2;
      pp->xu[k][3] = 0.5*s2u*sv4*sv4;
      pp->xv[k][0] = 0.125*s2u*sv2;
      pp->xv[k][1] = 0.25*su*cv2;
      pp->xv[k][2] = 0.25*cu*cv2;
      pp->xv[k][3] = 0.125*(su*su+1.0)*sv2;
    }
  }
}


/* Draw a 4d embedding of the projective plane projected into 3D. */
static int projective_plane(ModeInfo *mi, double umin, double umax,
                            double vmin, double vmax)
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
  double cu, su, cv2, sv2, cv4, sv4, c2u, s2u;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    /* Compute the rotation that rotates the projective plane in 4D without
       the trackball rotations. */
    rotateall4d(pp->zeta,pp->eta,pp->theta,mat);

    u = pp->umove;
    v = pp->vmove;
    cu = cos(u);
    su = sin(u);
    c2u = cos(2.0*u);
    s2u = sin(2.0*u);
    sv2 = sin(0.5*v);
    cv4 = cos(0.25*v);
    sv4 = sin(0.25*v);
    xx[0] = 0.5*s2u*sv4*sv4;
    xx[1] = 0.5*su*sv2;
    xx[2] = 0.5*cu*sv2;
    xx[3] = 0.5*(su*su*sv4*sv4-cv4*cv4);
    /* Avoid degenerate tangential plane basis vectors. */
    if (v < FLT_EPSILON)
      v = FLT_EPSILON;
    cv2 = cos(0.5*v);
    sv2 = sin(0.5*v);
    sv4 = sin(0.25*v);
    xxu[0] = c2u*sv4*sv4;
    xxu[1] = 0.5*cu*sv2;
    xxu[2] = -0.5*su*sv2;
    xxu[3] = 0.5*s2u*sv4*sv4;
    xxv[0] = 0.125*s2u*sv2;
    xxv[1] = 0.25*su*cv2;
    xxv[2] = 0.25*cu*cv2;
    xxv[3] = 0.125*(su*su+1.0)*sv2;
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
        p[l] = y[l]+pp->offset4d[l];
        pu[l] = yu[l];
        pv[l] = yv[l];
      }
    }
    else
    {
      s = y[3]+pp->offset4d[3];
      q = 1.0/s;
      t = q*q;
      for (l=0; l<3; l++)
      {
        r = y[l]+pp->offset4d[l];
        p[l] = r*q;
        pu[l] = (yu[l]*s-r*yu[3])*t;
        pv[l] = (yv[l]*s-r*yv[3])*t;
      }
    }
    n[0] = pu[1]*pv[2]-pu[2]*pv[1];
    n[1] = pu[2]*pv[0]-pu[0]*pv[2];
    n[2] = pu[0]*pv[1]-pu[1]*pv[0];
    t = 1.0/(pp->side*4.0*sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
    n[0] *= t;
    n[1] *= t;
    n[2] *= t;
    pm[0] = pu[0]*pp->dumove+pv[0]*pp->dvmove;
    pm[1] = pu[1]*pp->dumove+pv[1]*pp->dvmove;
    pm[2] = pu[2]*pp->dumove+pv[2]*pp->dvmove;
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
    pp->alpha = atan2(-n[2],-pm[2])*180/M_PI;
    pp->beta = atan2(-b[2],sqrt(b[0]*b[0]+b[1]*b[1]))*180/M_PI;
    pp->delta = atan2(b[1],-b[0])*180/M_PI;

    /* Compute the rotation that rotates the projective plane in 4D. */
    rotateall(pp->alpha,pp->beta,pp->delta,pp->zeta,pp->eta,pp->theta,mat);

    u = pp->umove;
    v = pp->vmove;
    cu = cos(u);
    su = sin(u);
    s2u = sin(2.0*u);
    sv2 = sin(0.5*v);
    cv4 = cos(0.25*v);
    sv4 = sin(0.25*v);
    xx[0] = 0.5*s2u*sv4*sv4;
    xx[1] = 0.5*su*sv2;
    xx[2] = 0.5*cu*sv2;
    xx[3] = 0.5*(su*su*sv4*sv4-cv4*cv4);
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
        p[l] = y[l]+pp->offset4d[l];
    }
    else
    {
      s = y[3]+pp->offset4d[3];
      for (l=0; l<3; l++)
        p[l] = (y[l]+pp->offset4d[l])/s;
    }

    pp->offset3d[0] = -p[0];
    pp->offset3d[1] = -p[1]-DELTAY;
    pp->offset3d[2] = -p[2];
  }
  else
  {
    /* Compute the rotation that rotates the projective plane in 4D,
       including the trackball rotations. */
    rotateall(pp->alpha,pp->beta,pp->delta,pp->zeta,pp->eta,pp->theta,r1);

    gltrackball_get_quaternion(pp->trackballs[0],q1);
    gltrackball_get_quaternion(pp->trackballs[1],q2);
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
        y[l] = (mat[l][0]*pp->x[o][0]+mat[l][1]*pp->x[o][1]+
                mat[l][2]*pp->x[o][2]+mat[l][3]*pp->x[o][3]);
        yu[l] = (mat[l][0]*pp->xu[o][0]+mat[l][1]*pp->xu[o][1]+
                 mat[l][2]*pp->xu[o][2]+mat[l][3]*pp->xu[o][3]);
        yv[l] = (mat[l][0]*pp->xv[o][0]+mat[l][1]*pp->xv[o][1]+
                 mat[l][2]*pp->xv[o][2]+mat[l][3]*pp->xv[o][3]);
      }
      if (projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          pp->pp[o][l] = (y[l]+pp->offset4d[l])+pp->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+pp->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+pp->offset4d[l];
          pp->pp[o][l] = r*q+pp->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      pp->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      pp->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      pp->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(pp->pn[o][0]*pp->pn[o][0]+pp->pn[o][1]*pp->pn[o][1]+
                   pp->pn[o][2]*pp->pn[o][2]);
      pp->pn[o][0] *= t;
      pp->pn[o][1] *= t;
      pp->pn[o][2] *= t;
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
  glBindTexture(GL_TEXTURE_2D,pp->tex_name);

  if (appearance != APPEARANCE_DIRECTION_BANDS)
  {
    for (i=0; i<NUMV; i++)
    {
      if (appearance == APPEARANCE_DISTANCE_BANDS &&
          ((i & (NUMB-1)) >= NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
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
          glNormal3fv(pp->pn[o]);
          glTexCoord2fv(pp->tex[o]);
          if (colors != COLORS_TWOSIDED)
          {
            glColor3fv(pp->col[o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,pp->col[o]);
          }
          glVertex3fv(pp->pp[o]);
          polys++;
        }
      }
      glEnd();
    }
  }
  else /* appearance == APPEARANCE_DIRECTION_BANDS */
  {
    for (j=0; j<NUMU; j++)
    {
      if ((j & (NUMB-1)) >= NUMB/2)
        continue;
      if (display_mode == DISP_WIREFRAME)
        glBegin(GL_QUAD_STRIP);
      else
        glBegin(GL_TRIANGLE_STRIP);
      for (i=0; i<=NUMV; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = (j+k);
          o = l*(NUMU+1)+m;
          glNormal3fv(pp->pn[o]);
          glTexCoord2fv(pp->tex[o]);
          if (colors != COLORS_TWOSIDED)
          {
            glColor3fv(pp->col[o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,pp->col[o]);
          }
          glVertex3fv(pp->pp[o]);
          polys++;
        }
      }
      glEnd();
    }
  }

  polys /= 2;
  return polys;
}


/* Generate a texture image that shows the orientation reversal. */
static void gen_texture(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  glGenTextures(1,&pp->tex_name);
  glBindTexture(GL_TEXTURE_2D,pp->tex_name);
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
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (walk_speed == 0.0)
    walk_speed = 20.0;

  if (view == VIEW_TURN)
  {
    pp->alpha = frand(360.0);
    pp->beta = frand(360.0);
    pp->delta = frand(360.0);
    pp->zeta = 0.0;
    pp->eta = 0.0;
    pp->theta = 0.0;
  }
  else
  {
    pp->alpha = 0.0;
    pp->beta = 0.0;
    pp->delta = 0.0;
    pp->zeta = 120.0;
    pp->eta = 180.0;
    pp->theta = 90.0;
  }
  pp->umove = frand(2.0*M_PI);
  pp->vmove = frand(2.0*M_PI);
  pp->dumove = 0.0;
  pp->dvmove = 0.0;
  pp->side = 1;
  if (sin(walk_direction*M_PI/180.0) >= 0.0)
    pp->dir = 1;
  else
    pp->dir = -1;

  pp->offset4d[0] = 0.0;
  pp->offset4d[1] = 0.0;
  pp->offset4d[2] = 0.0;
  pp->offset4d[3] = 1.2;
  pp->offset3d[0] = 0.0;
  pp->offset3d[1] = 0.0;
  pp->offset3d[2] = -1.2;
  pp->offset3d[3] = 0.0;

  gen_texture(mi);
  setup_projective_plane(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);

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
    glOrtho(-0.6,0.6,-0.6,0.6,0.1,10.0);
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
static void display_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (!pp->button_pressed)
  {
    if (view == VIEW_TURN)
    {
      pp->alpha += speed_wx * pp->speed_scale;
      if (pp->alpha >= 360.0)
        pp->alpha -= 360.0;
      pp->beta += speed_wy * pp->speed_scale;
      if (pp->beta >= 360.0)
        pp->beta -= 360.0;
      pp->delta += speed_wz * pp->speed_scale;
      if (pp->delta >= 360.0)
        pp->delta -= 360.0;
      pp->zeta += speed_xy * pp->speed_scale;
      if (pp->zeta >= 360.0)
        pp->zeta -= 360.0;
      pp->eta += speed_xz * pp->speed_scale;
      if (pp->eta >= 360.0)
        pp->eta -= 360.0;
      pp->theta += speed_yz * pp->speed_scale;
      if (pp->theta >= 360.0)
        pp->theta -= 360.0;
    }
    if (view == VIEW_WALKTURN)
    {
      pp->zeta += speed_xy * pp->speed_scale;
      if (pp->zeta >= 360.0)
        pp->zeta -= 360.0;
      pp->eta += speed_xz * pp->speed_scale;
      if (pp->eta >= 360.0)
        pp->eta -= 360.0;
      pp->theta += speed_yz * pp->speed_scale;
      if (pp->theta >= 360.0)
        pp->theta -= 360.0;
    }
    if (view == VIEW_WALK || view == VIEW_WALKTURN)
    {
      pp->dvmove = (pp->dir*sin(walk_direction*M_PI/180.0)*
                    walk_speed*M_PI/4096.0);
      pp->vmove += pp->dvmove;
      if (pp->vmove > 2.0*M_PI)
      {
        pp->vmove = 4.0*M_PI-pp->vmove;
        pp->umove = pp->umove-M_PI;
        if (pp->umove < 0.0)
          pp->umove += 2.0*M_PI;
        pp->side = -pp->side;
        pp->dir = -pp->dir;
        pp->dvmove = -pp->dvmove;
      }
      if (pp->vmove < 0.0)
      {
        pp->vmove = -pp->vmove;
        pp->umove = pp->umove-M_PI;
        if (pp->umove < 0.0)
          pp->umove += 2.0*M_PI;
        pp->dir = -pp->dir;
        pp->dvmove = -pp->dvmove;
      }
      pp->dumove = cos(walk_direction*M_PI/180.0)*walk_speed*M_PI/4096.0;
      pp->umove += pp->dumove;
      if (pp->umove >= 2.0*M_PI)
        pp->umove -= 2.0*M_PI;
      if (pp->umove < 0.0)
        pp->umove += 2.0*M_PI;
    }
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (projection_3d == DISP_3D_PERSPECTIVE ||
      view == VIEW_WALK || view == VIEW_WALKTURN)
  {
    if (view == VIEW_WALK || view == VIEW_WALKTURN)
      gluPerspective(60.0,pp->aspect,0.01,10.0);
    else
      gluPerspective(60.0,pp->aspect,0.1,10.0);
  }
  else
  {
    if (pp->aspect >= 1.0)
      glOrtho(-0.6*pp->aspect,0.6*pp->aspect,-0.6,0.6,0.1,10.0);
    else
      glOrtho(-0.6,0.6,-0.6/pp->aspect,0.6/pp->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  mi->polygon_count = projective_plane(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
}


ENTRYPOINT void reshape_projectiveplane(ModeInfo *mi, int width, int height)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  pp->WindW = (GLint)width;
  pp->WindH = (GLint)height;
  glViewport(0,0,width,height);
  pp->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool projectiveplane_handle_event(ModeInfo *mi, XEvent *event)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];
  KeySym  sym = 0;
  char c = 0;

  if (event->xany.type == KeyPress || event->xany.type == KeyRelease)
    XLookupString (&event->xkey, &c, 1, &sym, 0);

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
  {
    pp->button_pressed = True;
    gltrackball_start(pp->trackballs[pp->current_trackball],
                      event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    pp->button_pressed = False;
    return True;
  }
  else if (event->xany.type == KeyPress)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      pp->current_trackball = 1;
      if (pp->button_pressed)
        gltrackball_start(pp->trackballs[pp->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == KeyRelease)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      pp->current_trackball = 0;
      if (pp->button_pressed)
        gltrackball_start(pp->trackballs[pp->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == MotionNotify && pp->button_pressed)
  {
    gltrackball_track(pp->trackballs[pp->current_trackball],
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
 *    Initialize projectiveplane.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp;

  if (projectiveplane == NULL)
  {
    projectiveplane =
      (projectiveplanestruct *)calloc(MI_NUM_SCREENS(mi),
                                      sizeof(projectiveplanestruct));
    if (projectiveplane == NULL)
      return;
  }
  pp = &projectiveplane[MI_SCREEN(mi)];

  
  pp->trackballs[0] = gltrackball_init();
  pp->trackballs[1] = gltrackball_init();
  pp->current_trackball = 0;
  pp->button_pressed = False;

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
  else if (!strcasecmp(appear,"distance-bands"))
  {
    appearance = APPEARANCE_DISTANCE_BANDS;
  }
  else if (!strcasecmp(appear,"direction-bands"))
  {
    appearance = APPEARANCE_DIRECTION_BANDS;
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
  else if (!strcasecmp(color_mode,"distance"))
  {
    colors = COLORS_DISTANCE;
  }
  else if (!strcasecmp(color_mode,"direction"))
  {
    colors = COLORS_DIRECTION;
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
  pp->speed_scale = 0.9 + frand(0.3);

  if ((pp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_projectiveplane(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_projectiveplane(ModeInfo *mi)
{
  Display          *display = MI_DISPLAY(mi);
  Window           window = MI_WINDOW(mi);
  projectiveplanestruct *pp;

  if (projectiveplane == NULL)
    return;
  pp = &projectiveplane[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!pp->glx_context)
    return;

  glXMakeCurrent(display,window,*(pp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_projectiveplane(mi);

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

ENTRYPOINT void release_projectiveplane(ModeInfo *mi)
{
  if (projectiveplane != NULL)
  {
    int screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
    {
      projectiveplanestruct *pp = &projectiveplane[screen];

      if (pp->glx_context)
        pp->glx_context = (GLXContext *)NULL;
    }
    (void) free((void *)projectiveplane);
    projectiveplane = (projectiveplanestruct *)NULL;
  }
  FreeAllGL(mi);
}

#ifndef STANDALONE
ENTRYPOINT void change_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (!pp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*(pp->glx_context));
  init(mi);
}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE ("ProjectivePlane", projectiveplane)

#endif /* USE_GL */
