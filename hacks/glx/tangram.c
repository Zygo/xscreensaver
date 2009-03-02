/* tangram, Copyright (c) 2005 Jeremy English <jhe@jeremyenglish.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */


#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	  "TANGRAM"
#define HACK_INIT	  init_tangram
#define HACK_DRAW	  draw_tangram
#define HACK_RESHAPE  reshape_tangram
#define sws_opts	  xlockmore_opts


#define DEFAULTS	"*delay:	30000		 \n" \
			"*wireframe:	False		 \n" \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#include <ctype.h>


#ifdef USE_GL                   /* whole file */


#include <GL/glu.h>
#include <time.h>
#include <math.h>
#include "tangram_shapes.h"

typedef enum {
    true = 1,
    false = 0
} bool;

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

} tangram_shape;

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

typedef enum {
    state_exploding,
    state_solving,
    state_solved
} tangram_state;

tangram_shape solved[][7] = {
    {
     {{-1.664000, -1.552000, 0}, 135, 0, 0},
     {{-1.696000, 0.944000, 0}, 315, 0, 0},
     {{0.064000, -2.128000, 0}, 135, 0, 0},
     {{-0.960000, -1.056000, 0}, 90, 0, 0},
     {{1.104000, 0.960000, 0}, 270, 0, 0},
     {{-1.376000, -0.800000, 0}, 180, 0, 0},
     {{1.152000, 0.736000, 0}, 360, 0, 0},
     },
    {
     {{-0.096000, 1.552000, 0}, 135, 180, 0},
     {{-0.064000, 2.336000, 0}, 315, 0, 0},
     {{-0.080000, -0.224000, 0}, 135, 180, 0},
     {{-2.096000, 1.584000, 0}, 90, 180, 0},
     {{1.920000, 1.584000, 0}, 270, 0, 0},
     {{0.416000, -0.192000, 0}, 180, 0, 0},
     {{-0.096000, -1.296000, 0}, 335, 0, 0},
     },
    {
     {{-0.144000, -0.720000, 0}, 225, 0, 0},
     {{0.608000, 0.032000, 0}, 135, 0, 0},
     {{-1.584000, 0.720000, 0}, 0, 0, 0},
     {{-0.112000, -0.720000, 0}, 315, 0, 0},
     {{-0.096000, -0.704000, 0}, 45, 0, 0},
     {{0.592000, 0.016000, 0}, 225, 0, 0},
     {{-0.880000, -0.032000, 0}, 315, 0, 0},
     },
    {
     {{1.472000, 2.176000, 0}, 225, 0, 0},
     {{1.248000, 3.488000, 0}, 270, 0, 0},
     {{-0.752000, -1.680000, 0}, 270, 0, 0},
     {{0.704000, -1.552000, 0}, 135, 0, 0},
     {{1.280000, -0.080000, 0}, 180, 0, 0},
     {{-0.016000, -0.896000, 0}, 225, 0, 0},
     {{-0.000000, -0.944000, 0}, 315, 0, 0},
     },
    {
     {{0.320000, 1.360000, 0}, 90, 0, 0},
     {{0.704000, 3.072000, 0}, 270, 0, 0},
     {{-1.200000, -3.392000, 0}, 135, 0, 0},
     {{0.688000, -1.184000, 0}, 135, 0, 0},
     {{-0.768000, 0.192000, 0}, 315, 0, 0},
     {{-1.168000, -2.304000, 0}, 180, 0, 0},
     {{1.312000, 1.296000, 0}, 270, 0, 0},
     },
    {
     {{-2.064000, 0.848000, 0}, 65, 0, 0},
     {{0.096000, 1.424000, 0}, 99, 180, 0},
     {{2.016000, -2.448000, 0}, 270, 180, 0},
     {{-2.016000, 0.368000, 0}, 315, 0, 0},
     {{-0.560000, -1.040000, 0}, 135, 0, 0},
     {{1.312000, -1.696000, 0}, 225, 0, 0},
     {{0.864000, 0.336000, 0}, 180, 180, 0},
     },
    {
     {{0.560000, -0.208000, 0}, 135, 0, 0},
     {{0.336000, -1.552000, 0}, 90, 180, 0},
     {{-2.336000, 1.248000, 0}, 90, 180, 0},
     {{1.296000, -1.504000, 0}, 180, 0, 0},
     {{-0.896000, 1.200000, 0}, 135, 180, 0},
     {{0.304000, -2.544000, 0}, 180, 0, 0},
     {{1.248000, 0.544000, 0}, 225, 0, 0},
     },
    {
     {{-0.480000, -2.832000, 0}, 45, 0, 0},
     {{-0.544000, -2.832000, 0}, 225, 180, 0},
     {{-0.064000, -0.880000, 0}, 225, 180, 0},
     {{-2.528000, 2.656000, 0}, 0, 0, 0},
     {{-2.512000, 0.640000, 0}, 45, 180, 0},
     {{0.192000, -2.096000, 0}, 225, 0, 0},
     {{-0.064000, -0.832000, 0}, 180, 0, 0},
     },
    {
     {{0.880000, -1.456000, 0}, 45, 0, 0},
     {{0.832000, 0.592000, 0}, 180, 180, 0},
     {{-2.016000, 1.648000, 0}, 135, 180, 0},
     {{0.448000, 2.064000, 0}, 315, 0, 0},
     {{-0.992000, 0.688000, 0}, 315, 180, 0},
     {{1.856000, -0.400000, 0}, 180, 0, 0},
     {{-0.128000, -1.424000, 0}, 90, 0, 0},
     },
    {
     {{2.208000, 2.000000, 0}, 180, 0, 0},
     {{-0.640000, 3.072000, 0}, 180, 180, 0},
     {{1.360000, -3.312000, 0}, 180, 0, 0},
     {{-0.592000, 2.256000, 0}, 360, 0, 0},
     {{-0.960000, -0.160000, 0}, 45, 180, 0},
     {{0.288000, -2.896000, 0}, 135, 0, 0},
     {{0.496000, -0.128000, 0}, 135, 0, 0},
     },
    {
     {{-0.480000, 0.864000, 0}, 58, 180, 0},
     {{0.624000, -0.752000, 0}, 90, 180, 0},
     {{0.576000, -0.560000, 0}, 180, 180, 0},
     {{-0.192000, -1.264000, 0}, 225, 0, 0},
     {{-0.176000, -1.280000, 0}, 135, 180, 0},
     {{-0.816000, -0.528000, 0}, 123, 0, 0},
     {{-0.416000, -0.528000, 0}, 90, 0, 0},
     },
    {
     {{0.688000, -0.144000, 0}, 45, 180, 0},
     {{-0.080000, 0.592000, 0}, 225, 0, 0},
     {{-0.048000, 0.592000, 0}, 315, 180, 0},
     {{-1.488000, -0.848000, 0}, 45, 0, 0},
     {{1.376000, -0.864000, 0}, 225, 180, 0},
     {{0.688000, -0.128000, 0}, 135, 0, 0},
     {{-1.504000, -0.832000, 0}, 135, 0, 0},
     },
    {
     {{0.624000, -1.776000, 0}, 225, 180, 0},
     {{-0.144000, 0.432000, 0}, 225, 0, 0},
     {{-0.800000, -0.272000, 0}, 45, 180, 0},
     {{-2.320000, -0.304000, 0}, 45, 0, 0},
     {{2.048000, -0.320000, 0}, 225, 180, 0},
     {{-0.112000, 0.480000, 0}, 135, 0, 0},
     {{-0.832000, -0.320000, 0}, 135, 180, 0},
     },
    {
     {{-1.744000, -0.400000, 0}, 45, 180, 0},
     {{0.416000, 1.776000, 0}, 315, 0, 0},
     {{-1.008000, 0.272000, 0}, 90, 180, 0},
     {{0.800000, 1.488000, 0}, 135, 0, 0},
     {{0.832000, 1.440000, 0}, 45, 180, 0},
     {{-1.744000, -1.872000, 0}, 135, 0, 0},
     {{-1.008000, 0.320000, 0}, 45, 180, 0},
     },
    {
     {{-0.720000, 3.440000, 0}, 180, 180, 0},
     {{0.912000, -1.072000, 0}, 225, 0, 0},
     {{0.736000, 3.440000, 0}, 180, 180, 0},
     {{0.720000, 1.984000, 0}, 225, 0, 0},
     {{-0.672000, 0.544000, 0}, 45, 180, 0},
     {{-0.192000, -3.840000, 0}, 135, 0, 0},
     {{-0.528000, -2.480000, 0}, 135, 0, 0},
     },
    {
     {{1.184000, 1.904000, 0}, 90, 180, 0},
     {{-2.256000, 0.960000, 0}, 90, 0, 0},
     {{-0.208000, -0.528000, 0}, 45, 180, 0},
     {{-0.256000, -0.512000, 0}, 135, 0, 0},
     {{0.144000, -0.944000, 0}, 135, 180, 0},
     {{-0.608000, -3.648000, 0}, 105, 0, 0},
     {{0.832000, -0.912000, 0}, 135, 0, 0},
     },
    {
     {{-1.056000, -2.272000, 0}, 90, 0, 0},
     {{0.960000, -1.264000, 0}, 180, 0, 0},
     {{-0.000000, -2.288000, 0}, 135, 0, 0},
     {{-1.088000, -2.256000, 0}, 180, 0, 0},
     {{0.992000, 0.736000, 0}, 0, 0, 0},
     {{0.960000, -0.256000, 0}, 180, 0, 0},
     {{-0.064000, 0.752000, 0}, 180, 180, 0},
     },
    {
     {{-1.360000, -0.224000, 0}, 0, 0, 0},
     {{-0.336000, -0.176000, 0}, 90, 0, 0},
     {{0.688000, -0.192000, 0}, 45, 0, 0},
     {{-1.424000, -1.168000, 0}, 180, 0, 0},
     {{1.744000, 0.816000, 0}, 0, 0, 0},
     {{-0.384000, -0.176000, 0}, 180, 0, 0},
     {{1.648000, -1.216000, 0}, 270, 180, 0},
     },
    {
     {{2.112000, 1.504000, 0}, 0, 0, 0},
     {{-1.040000, 1.472000, 0}, 180, 180, 0},
     {{0.032000, -1.600000, 0}, 135, 0, 0},
     {{1.056000, 1.504000, 0}, 270, 0, 0},
     {{-0.992000, -0.528000, 0}, 0, 180, 0},
     {{2.080000, 0.512000, 0}, 180, 0, 0},
     {{-1.104000, 0.480000, 0}, 270, 180, 0},
     },
    {
     {{0.608000, 1.184000, 0}, 135, 0, 0},
     {{-2.928000, -1.584000, 0}, 135, 0, 0},
     {{0.688000, 0.560000, 0}, 90, 180, 0},
     {{0.640000, -0.832000, 0}, 180, 0, 0},
     {{-0.752000, -2.304000, 0}, 315, 180, 0},
     {{-2.192000, -0.912000, 0}, 315, 0, 0},
     {{2.704000, 1.616000, 0}, 270, 0, 0},
     },
    {
     {{0.880000, 0.960000, 0}, 45, 0, 0},
     {{0.832000, -0.960000, 0}, 0, 0, 0},
     {{1.536000, 1.712000, 0}, 225, 180, 0},
     {{-0.992000, 2.096000, 0}, 315, 0, 0},
     {{0.480000, 2.704000, 0}, 180, 180, 0},
     {{0.816000, 0.912000, 0}, 315, 0, 0},
     {{0.784000, -1.952000, 0}, 315, 180, 0},
     },
    {
     {{0.176000, 3.584000, 0}, 270, 0, 0},
     {{2.016000, -1.424000, 0}, 90, 0, 0},
     {{2.496000, 0.608000, 0}, 180, 180, 0},
     {{-0.304000, 0.496000, 0}, 270, 0, 0},
     {{-0.256000, -0.144000, 0}, 0, 180, 0},
     {{-1.600000, -0.368000, 0}, 303, 0, 0},
     {{0.768000, 0.912000, 0}, 180, 0, 0},
     },
    {
     {{-2.096000, 1.696000, 0}, 315, 0, 0},
     {{-1.632000, -1.440000, 0}, 45, 0, 0},
     {{-1.232000, -0.064000, 0}, 315, 180, 0},
     {{0.304000, 2.416000, 0}, 315, 0, 0},
     {{1.776000, -0.496000, 0}, 315, 180, 0},
     {{1.168000, -0.240000, 0}, 332, 0, 0},
     {{-0.880000, -1.216000, 0}, 135, 0, 0},
     },
    {
     {{-0.432000, -0.496000, 0}, 259, 0, 0},
     {{0.176000, -1.488000, 0}, 105, 0, 0},
     {{1.184000, -1.168000, 0}, 300, 180, 0},
     {{1.824000, 0.096000, 0}, 195, 0, 0},
     {{-2.400000, -0.048000, 0}, 11, 180, 0},
     {{-0.240000, -1.200000, 0}, 315, 0, 0},
     {{-0.688000, -1.488000, 0}, 281, 180, 0},
     },
    {
     {{0.096000, 2.000000, 0}, 315, 0, 0},
     {{0.432000, 0.160000, 0}, 360, 0, 0},
     {{0.208000, -1.504000, 0}, 220, 180, 0},
     {{-1.104000, -0.336000, 0}, 50, 0, 0},
     {{-1.136000, -0.288000, 0}, 310, 180, 0},
     {{0.416000, 1.232000, 0}, 360, 0, 0},
     {{0.048000, 2.016000, 0}, 225, 180, 0},
     },
    {
     {{-2.128000, 2.112000, 0}, 45, 0, 0},
     {{0.128000, 1.856000, 0}, 360, 0, 0},
     {{2.128000, -0.720000, 0}, 180, 180, 0},
     {{-1.376000, 2.816000, 0}, 360, 0, 0},
     {{-1.360000, 0.768000, 0}, 45, 180, 0},
     {{0.128000, 0.336000, 0}, 360, 0, 0},
     {{0.432000, -2.944000, 0}, 149, 0, 0},
     },
    {
     {{1.952000, -0.800000, 0}, 225, 0, 0},
     {{2.064000, -0.816000, 0}, 45, 0, 0},
     {{0.928000, 0.688000, 0}, 225, 0, 0},
     {{-1.568000, 3.152000, 0}, 0, 0, 0},
     {{-1.520000, 1.104000, 0}, 45, 0, 0},
     {{2.720000, -0.064000, 0}, 225, 0, 0},
     {{1.968000, 0.672000, 0}, 270, 0, 0},
     },
    {
     {{2.480000, -0.912000, 0}, 225, 0, 0},
     {{2.592000, -0.928000, 0}, 45, 0, 0},
     {{0.352000, 1.280000, 0}, 315, 0, 0},
     {{-0.688000, 0.336000, 0}, 135, 0, 0},
     {{1.808000, -0.112000, 0}, 135, 0, 0},
     {{3.248000, -0.176000, 0}, 225, 0, 0},
     {{-1.472000, 1.024000, 0}, 225, 0, 0},
     },
    {
     {{-0.400000, -1.232000, 0}, 270, 0, 0},
     {{0.400000, -0.640000, 0}, 270, 0, 0},
     {{1.904000, -3.232000, 0}, 180, 0, 0},
     {{1.872000, -1.776000, 0}, 225, 0, 0},
     {{1.552000, 0.656000, 0}, 270, 0, 0},
     {{1.056000, 1.760000, 0}, 270, 0, 0},
     {{-0.320000, -1.024000, 0}, 135, 180, 0},
     },
    {
     {{0.896000, -0.480000, 0}, 0, 0, 0},
     {{0.128000, -0.720000, 0}, 45, 0, 0},
     {{0.960000, -1.728000, 0}, 270, 0, 0},
     {{-1.040000, -1.648000, 0}, 90, 0, 0},
     {{-0.848000, 2.304000, 0}, 0, 0, 0},
     {{-0.096000, 0.944000, 0}, 315, 0, 0},
     {{-1.568000, -1.728000, 0}, 90, 180, 0},
     },
    {
     {{0.416000, 3.648000, 0}, 270, 0, 0},
     {{-0.000000, -1.072000, 0}, 331, 0, 0},
     {{1.360000, 0.528000, 0}, 180, 0, 0},
     {{0.880000, 0.464000, 0}, 270, 0, 0},
     {{0.576000, -3.184000, 0}, 151, 0, 0},
     {{0.864000, 2.576000, 0}, 270, 0, 0},
     {{-1.120000, 0.528000, 0}, 90, 0, 0},
     },
    {
     {{-1.056000, -3.456000, 0}, 90, 0, 0},
     {{0.736000, 2.000000, 0}, 135, 0, 0},
     {{-1.488000, 1.760000, 0}, 45, 0, 0},
     {{-0.432000, 0.016000, 0}, 0, 180, 0},
     {{-0.432000, -0.064000, 0}, 0, 0, 0},
     {{0.560000, -2.576000, 0}, 225, 0, 0},
     {{0.032000, 2.656000, 0}, 0, 0, 0},
     },
    {
     {{-2.800000, -2.304000, 0}, 101, 0, 0},
     {{1.888000, 2.032000, 0}, 135, 180, 0},
     {{-1.856000, 2.016000, 0}, 315, 0, 0},
     {{0.352000, -0.144000, 0}, 315, 180, 0},
     {{-2.848000, 0.976000, 0}, 0, 0, 0},
     {{-1.424000, -1.104000, 0}, 236, 0, 0},
     {{-1.792000, 2.016000, 0}, 45, 0, 0},
     },
    {
     {{0.864000, 0.880000, 0}, 180, 0, 0},
     {{0.912000, -2.288000, 0}, 180, 180, 0},
     {{-1.136000, -0.144000, 0}, 45, 0, 0},
     {{-1.136000, -3.312000, 0}, 360, 180, 0},
     {{-1.168000, 1.920000, 0}, 0, 0, 0},
     {{-1.184000, -1.248000, 0}, 180, 0, 0},
     {{-1.136000, -0.224000, 0}, 0, 0, 0},
     },
    {
     {{0.592000, 0.704000, 0}, 90, 0, 0},
     {{0.576000, -2.496000, 0}, 90, 180, 0},
     {{2.624000, -1.440000, 0}, 225, 0, 0},
     {{2.640000, -3.504000, 0}, 270, 180, 0},
     {{2.656000, 1.712000, 0}, 270, 0, 0},
     {{0.544000, 0.704000, 0}, 180, 0, 0},
     {{1.648000, 0.640000, 0}, 0, 0, 0},
     },
    {
     {{0.448000, -3.344000, 0}, 90, 0, 0},
     {{-1.616000, 1.984000, 0}, 90, 180, 0},
     {{-1.584000, 0.928000, 0}, 45, 0, 0},
     {{-1.600000, -2.288000, 0}, 0, 180, 0},
     {{1.536000, -1.328000, 0}, 270, 0, 0},
     {{2.592000, -3.328000, 0}, 180, 0, 0},
     {{-1.600000, 0.832000, 0}, 0, 0, 0},
     },
    {
     {{0.608000, 0.880000, 0}, 180, 0, 0},
     {{0.576000, -2.304000, 0}, 180, 180, 0},
     {{-1.456000, -0.176000, 0}, 45, 0, 0},
     {{-1.472000, -3.344000, 0}, 0, 180, 0},
     {{-1.472000, 1.888000, 0}, 0, 0, 0},
     {{0.640000, -1.168000, 0}, 180, 0, 0},
     {{-1.456000, -0.256000, 0}, 0, 0, 0},
     },
    {
     {{-0.208000, -0.352000, 0}, 225, 0, 0},
     {{0.528000, 1.856000, 0}, 225, 180, 0},
     {{-0.176000, -3.904000, 0}, 135, 0, 0},
     {{-0.880000, 0.384000, 0}, 45, 180, 0},
     {{-0.192000, -0.384000, 0}, 315, 0, 0},
     {{0.304000, -2.864000, 0}, 180, 0, 0},
     {{-0.224000, 2.528000, 0}, 315, 0, 0},
     },
    {
     {{-0.032000, 0.704000, 0}, 315, 0, 0},
     {{2.208000, -1.504000, 0}, 225, 180, 0},
     {{-0.720000, -0.064000, 0}, 0, 0, 0},
     {{-0.720000, -1.536000, 0}, 45, 180, 0},
     {{-0.016000, 1.744000, 0}, 315, 180, 0},
     {{0.464000, 0.736000, 0}, 180, 0, 0},
     {{1.456000, -0.816000, 0}, 315, 0, 0},
     },
    {
     {{-0.944000, 1.040000, 0}, 360, 0, 0},
     {{1.120000, 0.000000, 0}, 180, 180, 0},
     {{0.080000, -0.048000, 0}, 315, 0, 0},
     {{0.080000, -1.104000, 0}, 135, 180, 0},
     {{0.080000, 1.120000, 0}, 315, 180, 0},
     {{1.120000, 0.048000, 0}, 180, 0, 0},
     {{0.064000, 0.992000, 0}, 180, 180, 0},
     },
    {
     {{-0.688000, 0.784000, 0}, 135, 0, 0},
     {{0.800000, 0.784000, 0}, 135, 0, 0},
     {{0.512000, -2.512000, 0}, 315, 0, 0},
     {{1.488000, 0.000000, 0}, 225, 0, 0},
     {{-1.392000, 0.000000, 0}, 45, 0, 0},
     {{0.496000, -2.432000, 0}, 180, 0, 0},
     {{0.480000, -2.496000, 0}, 270, 0, 0},
     },
    {
     {{-0.992000, -2.160000, 0}, 90, 0, 0},
     {{-1.040000, -1.152000, 0}, 270, 0, 0},
     {{0.064000, -2.144000, 0}, 135, 0, 0},
     {{0.080000, -1.088000, 0}, 90, 0, 0},
     {{0.032000, -1.072000, 0}, 180, 0, 0},
     {{0.544000, -3.216000, 0}, 180, 0, 0},
     {{2.160000, -1.136000, 0}, 270, 0, 0},
     },
    {
     {{-2.896000, -0.128000, 0}, 45, 0, 0},
     {{-0.800000, 0.992000, 0}, 135, 0, 0},
     {{-1.152000, -0.416000, 0}, 225, 0, 0},
     {{-0.016000, 0.656000, 0}, 315, 0, 0},
     {{1.456000, -0.736000, 0}, 135, 0, 0},
     {{2.864000, 0.736000, 0}, 180, 0, 0},
     {{-0.048000, 1.664000, 0}, 180, 180, 0},
     },
    {
     {{2.960000, -1.376000, 0}, 270, 0, 0},
     {{1.952000, -1.312000, 0}, 90, 0, 0},
     {{-0.096000, 0.720000, 0}, 315, 0, 0},
     {{-2.112000, -0.320000, 0}, 90, 180, 0},
     {{1.904000, -0.320000, 0}, 180, 180, 0},
     {{-0.096000, -1.776000, 0}, 135, 0, 0},
     {{-3.168000, -2.304000, 0}, 360, 180, 0},
     },
    {
     {{-1.600000, -1.232000, 0}, 270, 0, 0},
     {{-1.600000, -0.128000, 0}, 180, 0, 0},
     {{2.272000, -0.128000, 0}, 225, 0, 0},
     {{-0.160000, -3.648000, 0}, 315, 180, 0},
     {{-0.176000, 2.336000, 0}, 135, 180, 0},
     {{-2.608000, -1.184000, 0}, 90, 0, 0},
     {{1.280000, -2.208000, 0}, 360, 180, 0}
     }
};


/* Dream */
color palette[] = {
    {22, 26, 28},
    {122, 142, 139},
    {72, 82, 76},
    {94, 115, 108},
    {194, 202, 195},
    {165, 175, 171},
    {132, 117, 107},
    {49, 54, 48},
    {111, 84, 78},
    {155, 144, 137},
    {82, 99, 100},
    {87, 57, 52},
    {131, 132, 125},
    {110, 130, 125},
    {62, 70, 68},
    {156, 160, 153},
    {182, 190, 184},
    {112, 116, 111},
    {56, 37, 35},
    {226, 230, 226},
    {146, 158, 154},
    {111, 101, 91},
    {96, 99, 96},
    {158, 130, 115},
    {147, 117, 105},
    {90, 85, 83},
    {112, 124, 121},
    {140, 144, 136},
    {176, 162, 156},
    {69, 56, 52},
    {183, 177, 169},
    {34, 38, 33},
    {213, 219, 212},
    {96, 108, 107},
    {143, 132, 121},
    {159, 166, 156},
    {132, 124, 113},
    {141, 151, 149},
    {132, 100, 94},
    {75, 73, 68},
    {57, 62, 57},
    {78, 90, 83},
    {210, 205, 207},
    {155, 152, 143},
    {42, 47, 40},
    {94, 70, 64},
    {195, 193, 188},
    {124, 116, 106},
    {123, 124, 117},
    {173, 182, 172},
    {133, 138, 135},
    {183, 185, 178},
    {120, 130, 120},
    {96, 100, 110},
    {127, 88, 78},
    {166, 161, 154},
    {91, 92, 98},
    {132, 124, 136},
    {101, 122, 117},
    {112, 108, 99},
    {140, 144, 151},
    {98, 109, 116},
    {58, 58, 51},
    {84, 84, 82},
    {132, 132, 142},
    {112, 116, 127},
    {58, 48, 48},
    {66, 66, 58},
    {205, 210, 205},
    {70, 78, 80},
    {246, 246, 244},
    {167, 168, 159},
    {147, 152, 145},
    {166, 154, 148},
    {107, 73, 67},
    {175, 177, 168},
    {132, 117, 126},
    {86, 106, 101},
    {78, 78, 75},
    {123, 138, 135},
    {112, 92, 83},
    {143, 138, 127},
    {133, 109, 95},
    {38, 34, 44},
    {150, 134, 147},
    {147, 123, 115},
    {46, 48, 54},
    {195, 198, 195},
    {194, 186, 188},
    {122, 110, 100},
    {105, 116, 106},
    {42, 42, 42},
    {223, 221, 220},
    {68, 71, 65},
    {156, 160, 164},
    {114, 124, 132},
    {155, 152, 163},
    {103, 94, 89},
    {58, 63, 70},
    {132, 143, 137},
    {168, 143, 128},
    {190, 198, 190},
    {239, 237, 237},
    {103, 101, 94},
    {84, 90, 84},
    {211, 213, 210},
    {183, 185, 188},
    {94, 94, 108},
    {203, 204, 196},
    {123, 125, 135},
    {120, 132, 133},
    {111, 110, 125},
    {28, 32, 28},
    {53, 56, 62},
    {88, 63, 59},
    {104, 108, 103},
    {140, 124, 115},
    {147, 126, 137},
    {155, 144, 157},
    {87, 98, 92},
    {188, 192, 189},
    {162, 133, 115},
    {148, 144, 134},
    {175, 169, 162},
    {138, 131, 120},
    {118, 124, 118},
    {139, 125, 135},
    {148, 144, 154},
    {72, 47, 44},
    {100, 72, 66},
    {66, 42, 39},
    {105, 84, 73},
    {86, 78, 82},
    {234, 238, 236},
    {108, 78, 73},
    {130, 110, 121},
    {134, 150, 146},
    {254, 254, 252},
    {151, 138, 130},
    {125, 99, 88},
    {80, 56, 52},
    {98, 63, 60},
    {114, 138, 132},
    {61, 42, 37},
    {150, 166, 164},
    {127, 94, 87},
    {225, 226, 223},
    {64, 47, 44},
    {218, 226, 220},
    {242, 242, 242},
    {150, 138, 150},
    {166, 138, 122},
    {102, 78, 71},
    {124, 118, 131},
    {74, 65, 66},
    {49, 33, 28},
    {182, 170, 175},
    {139, 117, 107},
    {120, 84, 76},
    {232, 232, 231},
    {46, 50, 41},
    {102, 102, 115},
    {104, 108, 119},
    {140, 118, 123},
    {120, 92, 84},
    {139, 108, 97},
    {156, 125, 107},
    {218, 214, 215},
    {144, 133, 143},
    {168, 161, 172},
    {58, 58, 68},
    {167, 169, 173},
    {148, 152, 158},
    {175, 177, 180},
    {67, 73, 77},
    {134, 143, 148},
    {79, 50, 45},
    {74, 83, 86},
    {150, 134, 124},
    {90, 86, 95},
    {184, 176, 186},
    {38, 39, 45},
    {160, 168, 166},
    {78, 73, 78},
    {195, 193, 196},
    {177, 184, 181},
    {43, 41, 52},
    {32, 33, 39},
    {135, 138, 148},
    {203, 198, 201},
    {26, 26, 28},
    {98, 116, 113},
    {198, 202, 199},
    {170, 176, 172},
    {54, 57, 48},
    {162, 146, 140},
    {114, 131, 130},
    {162, 160, 150},
    {118, 116, 108},
    {150, 159, 156},
    {118, 99, 84},
    {74, 54, 51},
    {138, 102, 100},
    {62, 64, 55},
    {162, 154, 146},
    {190, 185, 179},
    {98, 94, 94},
    {106, 124, 121},
    {118, 110, 100},
    {118, 117, 131},
    {50, 50, 60},
    {162, 152, 164},
    {94, 63, 58},
    {62, 70, 77},
    {174, 162, 172},
    {78, 91, 92},
    {85, 85, 93},
    {54, 50, 61},
    {67, 66, 74},
    {166, 154, 167},
    {126, 109, 119},
    {104, 116, 118},
    {83, 92, 95},
    {203, 205, 204},
    {88, 100, 101},
    {176, 169, 179},
    {145, 138, 148},
    {116, 78, 68},
    {78, 78, 84},
    {126, 142, 137},
    {78, 83, 76},
    {94, 58, 52},
    {62, 38, 36},
    {98, 86, 76},
    {38, 42, 34},
    {218, 220, 217},
    {82, 70, 60},
    {202, 194, 188},
    {178, 183, 172},
    {126, 131, 120},
    {138, 132, 143},
    {250, 250, 250},
    {114, 74, 68},
    {90, 108, 106},
    {50, 38, 36},
    {162, 161, 168},
    {118, 124, 132},
    {62, 65, 71},
    {190, 185, 193},
    {98, 94, 109},
    {126, 132, 138},
    {34, 34, 28},
    {162, 146, 163},
    {78, 85, 88},
    {158, 134, 124},
    {190, 178, 188}
};

static tangram_shape tsm1 = { {-0.144, -0.72, 0}, 225, false, 0 };
static tangram_shape tsm2 = { {0.608, 0.032, 0}, 135, false, 0 };
static tangram_shape tm = { {-1.584, 0.72, 0}, 0, false, 0 };
static tangram_shape tlg1 = { {-0.112, -0.72, 0}, 315, false, 0 };
static tangram_shape tlg2 = { {-0.096, -0.704, 0}, 45, false, 0 };
static tangram_shape sq = { {0.592, 0.016, 0}, 225, false, 0 };
static tangram_shape rh = { {-0.88, -0.032, 0}, 315, false, 0 };


#define DEF_VIEWING_TIME "5"
static GLuint viewing_time = 0;

static XrmOptionDescRec opts[] = {
    {"-viewing_time", ".viewing_time", XrmoptionSepArg, 0}
};

static argtype vars[] = {
    {&viewing_time, "viewing_time", "Viewing Time", DEF_VIEWING_TIME, t_Int}
};

ModeSpecOpt sws_opts = { countof(opts), opts, countof(vars), vars, NULL };

static int wire;
static int csi = -1;            /* Current Shape index */

static void get_solved_puzzle(tangram_shape * tsm1, tangram_shape * tsm2,
                              tangram_shape * tm, tangram_shape * tlg1,
                              tangram_shape * tlg2, tangram_shape * sq,
                              tangram_shape * rh)
{
    int sz = sizeof(solved) / sizeof(solved[0]);
    int r;
    GLuint t_dl;

    do {
        r = random() % sz;
    } while (r == csi);
    csi = r;

    t_dl = tsm1->dl;
    *tsm1 = solved[r][small_triangle1];
    tsm1->dl = t_dl;

    t_dl = tsm2->dl;
    *tsm2 = solved[r][small_triangle2];
    tsm2->dl = t_dl;

    t_dl = tm->dl;
    *tm = solved[r][medium_triangle];
    tm->dl = t_dl;

    t_dl = tlg1->dl;
    *tlg1 = solved[r][large_triangle1];
    tlg1->dl = t_dl;

    t_dl = tlg2->dl;
    *tlg2 = solved[r][large_triangle2];
    tlg2->dl = t_dl;

    t_dl = sq->dl;
    *sq = solved[r][square];
    sq->dl = t_dl;

    t_dl = rh->dl;
    *rh = solved[r][rhomboid];
    rh->dl = t_dl;

}

static bool colors_match(color c1, color c2)
{
    bool res = false;
    if (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b)
        res = true;
    return res;
}

static color rand_palette(void)
{
    int l = sizeof(palette) / sizeof(color);
    int r = random() % l;
    return palette[r];
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
static bool gt(GLfloat x1, GLfloat x2, GLfloat per)
{
    if ((x1 > x2) && (fabs(x1 - x2) > per))
        return true;
    else
        return false;
}

/* lt - floating point less than comparison */
static bool lt(GLfloat x1, GLfloat x2, GLfloat per)
{
    if ((x1 < x2) && (fabs(x1 - x2) > per))
        return true;
    else
        return false;
}

/* eq - Check the equality of a pair of floating point numbers to a certain perscision */
static bool eq(GLfloat x1, GLfloat x2, GLfloat per)
{
    if (fabs(x1 - x2) < per)
        return true;
    else
        return false;
}


static GLfloat approach_float(GLfloat goal, GLfloat current,
                              bool * changed, GLfloat per)
{
    *changed = false;
    if (gt(goal, current, per)) {
        current += per;
        *changed = true;
    } else if (lt(goal, current, per)) {
        current -= per;
        *changed = true;
    }
    return current;
}

static bool coords_match(coord c1, coord c2, GLfloat per)
{
    if (eq(c1.x, c2.x, per) && eq(c1.y, c2.y, per) && eq(c1.z, c2.z, per))
        return true;
    else
        return false;
}

static bool xy_coords_match(coord c1, coord c2, GLfloat per)
{
    if (eq(c1.x, c2.x, per) && eq(c1.y, c2.y, per))
        return true;
    else
        return false;
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

/* approach_shape - bring use on step closer to the new shape */
static tangram_shape approach_shape(tangram_shape old, tangram_shape new,
                                    bool * b, char *name)
{

    GLuint dl;
    bool changed;
    *b = false;

    old.fr = approach_number(new.fr, old.fr, 2);
    if (old.fr != new.fr) {
        return old;
    }

    old.r = approach_number(new.r, old.r, 2);
    if (old.r != new.r) {
        return old;
    }

    old.crd.x = approach_float(new.crd.x, old.crd.x, &changed, 0.1);
    if (!changed)
        old.crd.x = approach_float(new.crd.x, old.crd.x, &changed, 0.01);
    if (!changed)
        old.crd.x = approach_float(new.crd.x, old.crd.x, &changed, 0.001);
    if (!changed)
        old.crd.x = approach_float(new.crd.x, old.crd.x, &changed, 0.0001);

    old.crd.y = approach_float(new.crd.y, old.crd.y, &changed, 0.1);
    if (!changed)
        old.crd.y = approach_float(new.crd.y, old.crd.y, &changed, 0.01);
    if (!changed)
        old.crd.y = approach_float(new.crd.y, old.crd.y, &changed, 0.001);
    if (!changed)
        old.crd.y = approach_float(new.crd.y, old.crd.y, &changed, 0.0001);

    if (xy_coords_match(new.crd, old.crd, 0.0001)) {
        old.crd.z = approach_float(new.crd.z, old.crd.z, &changed, 0.1);
        if (!changed)
            old.crd.z =
                approach_float(new.crd.z, old.crd.z, &changed, 0.01);
        if (!changed)
            old.crd.z =
                approach_float(new.crd.z, old.crd.z, &changed, 0.001);
        if (!changed)
            old.crd.z =
                approach_float(new.crd.z, old.crd.z, &changed, 0.0001);
    }


    if (coords_match(new.crd, old.crd, 0.0001)) {
        dl = old.dl;
#if 0
        fprintf(stderr, "%s\n", name);
        print_shape("old", old);
        print_shape("new", new);
#endif
        old = new;              /* pick up the rest of the settings; */
        old.dl = dl;
        *b = true;
    }

    return old;
}

static color get_color(void)
{
    static color new_color = { 100, 100, 100 };
    static color old_color = { 100, 100, 100 };

    if (colors_match(old_color, new_color)) {
        old_color = new_color;
        new_color = rand_palette();
    }

    if ((random() % 10) == 0) { /* slow down the cycle */
        old_color.r = approach_number(new_color.r, old_color.r, 1);
        old_color.g = approach_number(new_color.g, old_color.g, 1);
        old_color.b = approach_number(new_color.b, old_color.b, 1);
    }

    return old_color;
}

static void explode(tangram_shape * tsm1, tangram_shape * tsm2,
                    tangram_shape * tm, tangram_shape * tlg1,
                    tangram_shape * tlg2, tangram_shape * sq,
                    tangram_shape * rh)
{
    tsm1->crd.z = 1;
    tsm1->r = 0;
    tsm2->crd.z = -1;
    tsm2->r = 0;
    tm->crd.z = 2;
    tm->r = 0;
    tlg1->crd.z = -2;
    tlg1->r = 0;
    tlg2->crd.z = 3;
    tlg2->r = 0;
    sq->crd.z = -3;
    sq->r = 0;
    rh->crd.z = 4;
    rh->r = 0;
}


static void draw_tangram_shape(tangram_shape ts)
{
    glPushMatrix();

    glTranslatef(ts.crd.x, ts.crd.y, ts.crd.z);
    glRotated(90, 1, 0, 0);
    glRotated(ts.fr, 1, 0, 0);
    glRotated(ts.r, 0, 1, 0);
    glCallList(ts.dl);
    glPopMatrix();
}

static void draw_shapes(void)
{

    if (!wire) {
        color c = get_color();
        glColor3ub(c.r, c.g, c.b);
    }

    draw_tangram_shape(tsm1);

    draw_tangram_shape(tsm2);
    draw_tangram_shape(tm);
    draw_tangram_shape(tlg1);
    draw_tangram_shape(tlg2);
    draw_tangram_shape(sq);
    draw_tangram_shape(rh);


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

void reshape_tangram(ModeInfo * mi, int w, int h)
{
    glViewport(0, 0, w, h);
    set_perspective();
    glLoadIdentity();
}

static void rotate_camera(void)
{
    static GLfloat theta[3] = { 1, 1, 1 };
    static bool going_down[3] = { false, false, false };


    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, -1, 0.1, 50);


    gluLookAt(0, 5, -5, 0, 0, 0, 0, -1, 0);

    glRotatef(theta[0], 1, 0, 0);
    glRotatef(theta[1], 0, 1, 0);
    glRotatef(theta[2], 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();


    if (going_down[0] && theta[0] < 0) {

        going_down[0] = false;
    } else if ((!going_down[0]) && theta[0] > 90) {

        going_down[0] = true;
    }

    if (theta[1] > 360.0)
        theta[1] -= 360.0;

    if (going_down[0])
        theta[0] -= 0.5;
    else
        theta[0] += 0.5;
    theta[1] += 1;
}

static void init_shapes(void)
{
    get_solved_puzzle(&tsm1, &tsm2, &tm, &tlg1, &tlg2, &sq, &rh);
    tsm1.dl = get_sm_tri_dl(wire);
    tsm2.dl = get_sm_tri_dl(wire);
    tm.dl = get_md_tri_dl(wire);
    tlg1.dl = get_lg_tri_dl(wire);
    tlg2.dl = get_lg_tri_dl(wire);
    sq.dl = get_square_dl(wire);
    rh.dl = get_rhomboid_dl(wire);
}

void init_tangram(ModeInfo * mi)
{
    GLfloat pos[4] = { 1, 1, -5, 1.00 };
    GLfloat pos2[4] = { 1, 1, 5, 1.00 };
    GLfloat lKa[4] = { 0, 0, 0, 1 };
    GLfloat lKd[4] = { 1, 1, 1, 1 };
    GLfloat lKs[4] = { 1, 1, 1, 1 };

    wire = MI_IS_WIREFRAME(mi);
    init_GL(mi);

    if (!wire) {
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lKa);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lKd);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lKs);

        glLightfv(GL_LIGHT1, GL_POSITION, pos2);
        glLightfv(GL_LIGHT1, GL_AMBIENT, lKa);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, lKd);
        glLightfv(GL_LIGHT1, GL_SPECULAR, lKs);

        glEnable(GL_DEPTH_TEST);

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);

        glEnable(GL_COLOR_MATERIAL);
    }
    init_shapes();

}

static tangram_shape explode_step(tangram_shape old, tangram_shape new,
                                  bool * b, GLfloat per)
{
    old.crd.z = approach_float(new.crd.z, old.crd.z, b, per);
    if (eq(new.crd.z, old.crd.z, per))
        *b = true;
    else
        *b = false;

    return old;
}

void draw_tangram(ModeInfo * mi)
{
    Display *dpy = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
    static tangram_state state = state_solved;
    static tangram_shape n_tsm1, n_tsm2, n_tm, n_tlg1, n_tlg2, n_sq, n_rh;
    static bool b_tsm1, b_tsm2, b_tm, b_tlg1, b_tlg2, b_sq, b_rh;
    static time_t s_tm = 0;
    static time_t c_tm;
    rotate_camera();

    switch (state) {
    case state_exploding:
        tsm1 = explode_step(tsm1, n_tsm1, &b_tsm1, 0.1);
        tsm2 = explode_step(tsm2, n_tsm2, &b_tsm2, 0.1);
        tm = explode_step(tm, n_tm, &b_tm, 0.1);
        tlg1 = explode_step(tlg1, n_tlg1, &b_tlg1, 0.1);
        tlg2 = explode_step(tlg2, n_tlg2, &b_tlg2, 0.1);
        sq = explode_step(sq, n_sq, &b_sq, 0.1);
        rh = explode_step(rh, n_rh, &b_rh, 0.1);
        if (b_tsm1 && b_tsm2 && b_tm && b_tlg1 && b_tlg2 && b_sq && b_rh) {
            get_solved_puzzle(&n_tsm1, &n_tsm2, &n_tm, &n_tlg1, &n_tlg2,
                              &n_sq, &n_rh);
            state = state_solving;
        }

        break;
    case state_solving:

        tsm1 = approach_shape(tsm1, n_tsm1, &b_tsm1, "small 1");
        tsm2 = approach_shape(tsm2, n_tsm2, &b_tsm2, "small 2");
        tm = approach_shape(tm, n_tm, &b_tm, "medium");
        tlg1 = approach_shape(tlg1, n_tlg1, &b_tlg1, "large 1");
        tlg2 = approach_shape(tlg2, n_tlg2, &b_tlg2, "large 2");
        sq = approach_shape(sq, n_sq, &b_sq, "square");
        rh = approach_shape(rh, n_rh, &b_rh, "rhomboid");
        if (b_tsm1 && b_tsm2 && b_tm && b_tlg1 && b_tlg2 && b_sq && b_rh) {
            state = state_solved;
            s_tm = time(0);
        }
        break;
    case state_solved:
        c_tm = time(0);
        if (floor(difftime(c_tm, s_tm)) >= viewing_time) {
            /*if ((random() % 100) == 0) { */
            explode(&n_tsm1, &n_tsm2, &n_tm, &n_tlg1, &n_tlg2, &n_sq,
                    &n_rh);

            state = state_exploding;

        }
        break;
    }
    glPushMatrix();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    draw_shapes();
    glFlush();
    glPopMatrix();
    glXSwapBuffers(dpy, window);
}

#endif                          /* USE_GL */
