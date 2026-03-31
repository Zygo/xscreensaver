/* Roman monospaced simplex stroke font copyright (c) 1989, 1990, 1991
 * by Sun Microsystems, Inc. and the X Consortium.
 * Originally part of the GLUT library by Mark J. Kilgard.
 * I have changed names so that this can be a .h rather than a .c file without
 * causing name clashes with glut_roman.h
 */

#include "glutstroke.h"

/* char: 33 '!' */

static const CoordRec monoChar33_stroke0[] = {
    { 52.381, 100 },
    { 52.381, 33.3333 },
};

static const CoordRec monoChar33_stroke1[] = {
    { 52.381, 9.5238 },
    { 47.6191, 4.7619 },
    { 52.381, 0 },
    { 57.1429, 4.7619 },
    { 52.381, 9.5238 },
};

static const StrokeRec monoChar33[] = {
   { 2, monoChar33_stroke0 },
   { 5, monoChar33_stroke1 },
};

/* char: 34 '"' */

static const CoordRec monoChar34_stroke0[] = {
    { 33.3334, 100 },
    { 33.3334, 66.6667 },
};

static const CoordRec monoChar34_stroke1[] = {
    { 71.4286, 100 },
    { 71.4286, 66.6667 },
};

static const StrokeRec monoChar34[] = {
   { 2, monoChar34_stroke0 },
   { 2, monoChar34_stroke1 },
};

/* char: 35 '#' */

static const CoordRec monoChar35_stroke0[] = {
    { 54.7619, 119.048 },
    { 21.4286, -33.3333 },
};

static const CoordRec monoChar35_stroke1[] = {
    { 83.3334, 119.048 },
    { 50, -33.3333 },
};

static const CoordRec monoChar35_stroke2[] = {
    { 21.4286, 57.1429 },
    { 88.0952, 57.1429 },
};

static const CoordRec monoChar35_stroke3[] = {
    { 16.6667, 28.5714 },
    { 83.3334, 28.5714 },
};

static const StrokeRec monoChar35[] = {
   { 2, monoChar35_stroke0 },
   { 2, monoChar35_stroke1 },
   { 2, monoChar35_stroke2 },
   { 2, monoChar35_stroke3 },
};

/* char: 36 '$' */

static const CoordRec monoChar36_stroke0[] = {
    { 42.8571, 119.048 },
    { 42.8571, -19.0476 },
};

static const CoordRec monoChar36_stroke1[] = {
    { 61.9047, 119.048 },
    { 61.9047, -19.0476 },
};

static const CoordRec monoChar36_stroke2[] = {
    { 85.7143, 85.7143 },
    { 76.1905, 95.2381 },
    { 61.9047, 100 },
    { 42.8571, 100 },
    { 28.5714, 95.2381 },
    { 19.0476, 85.7143 },
    { 19.0476, 76.1905 },
    { 23.8095, 66.6667 },
    { 28.5714, 61.9048 },
    { 38.0952, 57.1429 },
    { 66.6666, 47.619 },
    { 76.1905, 42.8571 },
    { 80.9524, 38.0952 },
    { 85.7143, 28.5714 },
    { 85.7143, 14.2857 },
    { 76.1905, 4.7619 },
    { 61.9047, 0 },
    { 42.8571, 0 },
    { 28.5714, 4.7619 },
    { 19.0476, 14.2857 },
};

static const StrokeRec monoChar36[] = {
   { 2, monoChar36_stroke0 },
   { 2, monoChar36_stroke1 },
   { 20, monoChar36_stroke2 },
};

/* char: 37 '%' */

static const CoordRec monoChar37_stroke0[] = {
    { 95.2381, 100 },
    { 9.5238, 0 },
};

static const CoordRec monoChar37_stroke1[] = {
    { 33.3333, 100 },
    { 42.8571, 90.4762 },
    { 42.8571, 80.9524 },
    { 38.0952, 71.4286 },
    { 28.5714, 66.6667 },
    { 19.0476, 66.6667 },
    { 9.5238, 76.1905 },
    { 9.5238, 85.7143 },
    { 14.2857, 95.2381 },
    { 23.8095, 100 },
    { 33.3333, 100 },
    { 42.8571, 95.2381 },
    { 57.1428, 90.4762 },
    { 71.4286, 90.4762 },
    { 85.7143, 95.2381 },
    { 95.2381, 100 },
};

static const CoordRec monoChar37_stroke2[] = {
    { 76.1905, 33.3333 },
    { 66.6667, 28.5714 },
    { 61.9048, 19.0476 },
    { 61.9048, 9.5238 },
    { 71.4286, 0 },
    { 80.9524, 0 },
    { 90.4762, 4.7619 },
    { 95.2381, 14.2857 },
    { 95.2381, 23.8095 },
    { 85.7143, 33.3333 },
    { 76.1905, 33.3333 },
};

static const StrokeRec monoChar37[] = {
   { 2, monoChar37_stroke0 },
   { 16, monoChar37_stroke1 },
   { 11, monoChar37_stroke2 },
};

/* char: 38 '&' */

static const CoordRec monoChar38_stroke0[] = {
    { 100, 57.1429 },
    { 100, 61.9048 },
    { 95.2381, 66.6667 },
    { 90.4762, 66.6667 },
    { 85.7143, 61.9048 },
    { 80.9524, 52.381 },
    { 71.4286, 28.5714 },
    { 61.9048, 14.2857 },
    { 52.3809, 4.7619 },
    { 42.8571, 0 },
    { 23.8095, 0 },
    { 14.2857, 4.7619 },
    { 9.5238, 9.5238 },
    { 4.7619, 19.0476 },
    { 4.7619, 28.5714 },
    { 9.5238, 38.0952 },
    { 14.2857, 42.8571 },
    { 47.619, 61.9048 },
    { 52.3809, 66.6667 },
    { 57.1429, 76.1905 },
    { 57.1429, 85.7143 },
    { 52.3809, 95.2381 },
    { 42.8571, 100 },
    { 33.3333, 95.2381 },
    { 28.5714, 85.7143 },
    { 28.5714, 76.1905 },
    { 33.3333, 61.9048 },
    { 42.8571, 47.619 },
    { 66.6667, 14.2857 },
    { 76.1905, 4.7619 },
    { 85.7143, 0 },
    { 95.2381, 0 },
    { 100, 4.7619 },
    { 100, 9.5238 },
};

static const StrokeRec monoChar38[] = {
   { 34, monoChar38_stroke0 },
};

/* char: 39 ''' */

static const CoordRec monoChar39_stroke0[] = {
    { 52.381, 100 },
    { 52.381, 66.6667 },
};

static const StrokeRec monoChar39[] = {
   { 2, monoChar39_stroke0 },
};

/* char: 40 '(' */

static const CoordRec monoChar40_stroke0[] = {
    { 69.0476, 119.048 },
    { 59.5238, 109.524 },
    { 50, 95.2381 },
    { 40.4762, 76.1905 },
    { 35.7143, 52.381 },
    { 35.7143, 33.3333 },
    { 40.4762, 9.5238 },
    { 50, -9.5238 },
    { 59.5238, -23.8095 },
    { 69.0476, -33.3333 },
};

static const StrokeRec monoChar40[] = {
   { 10, monoChar40_stroke0 },
};

/* char: 41 ')' */

static const CoordRec monoChar41_stroke0[] = {
    { 35.7143, 119.048 },
    { 45.2381, 109.524 },
    { 54.7619, 95.2381 },
    { 64.2857, 76.1905 },
    { 69.0476, 52.381 },
    { 69.0476, 33.3333 },
    { 64.2857, 9.5238 },
    { 54.7619, -9.5238 },
    { 45.2381, -23.8095 },
    { 35.7143, -33.3333 },
};

static const StrokeRec monoChar41[] = {
   { 10, monoChar41_stroke0 },
};

/* char: 42 '*' */

static const CoordRec monoChar42_stroke0[] = {
    { 52.381, 71.4286 },
    { 52.381, 14.2857 },
};

static const CoordRec monoChar42_stroke1[] = {
    { 28.5715, 57.1429 },
    { 76.1905, 28.5714 },
};

static const CoordRec monoChar42_stroke2[] = {
    { 76.1905, 57.1429 },
    { 28.5715, 28.5714 },
};

static const StrokeRec monoChar42[] = {
   { 2, monoChar42_stroke0 },
   { 2, monoChar42_stroke1 },
   { 2, monoChar42_stroke2 },
};

/* char: 43 '+' */

static const CoordRec monoChar43_stroke0[] = {
    { 52.3809, 85.7143 },
    { 52.3809, 0 },
};

static const CoordRec monoChar43_stroke1[] = {
    { 9.5238, 42.8571 },
    { 95.2381, 42.8571 },
};

static const StrokeRec monoChar43[] = {
   { 2, monoChar43_stroke0 },
   { 2, monoChar43_stroke1 },
};

/* char: 44 ',' */

static const CoordRec monoChar44_stroke0[] = {
    { 57.1429, 4.7619 },
    { 52.381, 0 },
    { 47.6191, 4.7619 },
    { 52.381, 9.5238 },
    { 57.1429, 4.7619 },
    { 57.1429, -4.7619 },
    { 52.381, -14.2857 },
    { 47.6191, -19.0476 },
};

static const StrokeRec monoChar44[] = {
   { 8, monoChar44_stroke0 },
};

/* char: 45 '-' */

static const CoordRec monoChar45_stroke0[] = {
    { 9.5238, 42.8571 },
    { 95.2381, 42.8571 },
};

static const StrokeRec monoChar45[] = {
   { 2, monoChar45_stroke0 },
};

/* char: 46 '.' */

static const CoordRec monoChar46_stroke0[] = {
    { 52.381, 9.5238 },
    { 47.6191, 4.7619 },
    { 52.381, 0 },
    { 57.1429, 4.7619 },
    { 52.381, 9.5238 },
};

static const StrokeRec monoChar46[] = {
   { 5, monoChar46_stroke0 },
};

/* char: 47 '/' */

static const CoordRec monoChar47_stroke0[] = {
    { 19.0476, -14.2857 },
    { 85.7143, 100 },
};

static const StrokeRec monoChar47[] = {
   { 2, monoChar47_stroke0 },
};

/* char: 48 '0' */

static const CoordRec monoChar48_stroke0[] = {
    { 47.619, 100 },
    { 33.3333, 95.2381 },
    { 23.8095, 80.9524 },
    { 19.0476, 57.1429 },
    { 19.0476, 42.8571 },
    { 23.8095, 19.0476 },
    { 33.3333, 4.7619 },
    { 47.619, 0 },
    { 57.1428, 0 },
    { 71.4286, 4.7619 },
    { 80.9524, 19.0476 },
    { 85.7143, 42.8571 },
    { 85.7143, 57.1429 },
    { 80.9524, 80.9524 },
    { 71.4286, 95.2381 },
    { 57.1428, 100 },
    { 47.619, 100 },
};

static const StrokeRec monoChar48[] = {
   { 17, monoChar48_stroke0 },
};

/* char: 49 '1' */

static const CoordRec monoChar49_stroke0[] = {
    { 40.4762, 80.9524 },
    { 50, 85.7143 },
    { 64.2857, 100 },
    { 64.2857, 0 },
};

static const StrokeRec monoChar49[] = {
   { 4, monoChar49_stroke0 },
};

/* char: 50 '2' */

static const CoordRec monoChar50_stroke0[] = {
    { 23.8095, 76.1905 },
    { 23.8095, 80.9524 },
    { 28.5714, 90.4762 },
    { 33.3333, 95.2381 },
    { 42.8571, 100 },
    { 61.9047, 100 },
    { 71.4286, 95.2381 },
    { 76.1905, 90.4762 },
    { 80.9524, 80.9524 },
    { 80.9524, 71.4286 },
    { 76.1905, 61.9048 },
    { 66.6666, 47.619 },
    { 19.0476, 0 },
    { 85.7143, 0 },
};

static const StrokeRec monoChar50[] = {
   { 14, monoChar50_stroke0 },
};

/* char: 51 '3' */

static const CoordRec monoChar51_stroke0[] = {
    { 28.5714, 100 },
    { 80.9524, 100 },
    { 52.3809, 61.9048 },
    { 66.6666, 61.9048 },
    { 76.1905, 57.1429 },
    { 80.9524, 52.381 },
    { 85.7143, 38.0952 },
    { 85.7143, 28.5714 },
    { 80.9524, 14.2857 },
    { 71.4286, 4.7619 },
    { 57.1428, 0 },
    { 42.8571, 0 },
    { 28.5714, 4.7619 },
    { 23.8095, 9.5238 },
    { 19.0476, 19.0476 },
};

static const StrokeRec monoChar51[] = {
   { 15, monoChar51_stroke0 },
};

/* char: 52 '4' */

static const CoordRec monoChar52_stroke0[] = {
    { 64.2857, 100 },
    { 16.6667, 33.3333 },
    { 88.0952, 33.3333 },
};

static const CoordRec monoChar52_stroke1[] = {
    { 64.2857, 100 },
    { 64.2857, 0 },
};

static const StrokeRec monoChar52[] = {
   { 3, monoChar52_stroke0 },
   { 2, monoChar52_stroke1 },
};

/* char: 53 '5' */

static const CoordRec monoChar53_stroke0[] = {
    { 76.1905, 100 },
    { 28.5714, 100 },
    { 23.8095, 57.1429 },
    { 28.5714, 61.9048 },
    { 42.8571, 66.6667 },
    { 57.1428, 66.6667 },
    { 71.4286, 61.9048 },
    { 80.9524, 52.381 },
    { 85.7143, 38.0952 },
    { 85.7143, 28.5714 },
    { 80.9524, 14.2857 },
    { 71.4286, 4.7619 },
    { 57.1428, 0 },
    { 42.8571, 0 },
    { 28.5714, 4.7619 },
    { 23.8095, 9.5238 },
    { 19.0476, 19.0476 },
};

static const StrokeRec monoChar53[] = {
   { 17, monoChar53_stroke0 },
};

/* char: 54 '6' */

static const CoordRec monoChar54_stroke0[] = {
    { 78.5714, 85.7143 },
    { 73.8096, 95.2381 },
    { 59.5238, 100 },
    { 50, 100 },
    { 35.7143, 95.2381 },
    { 26.1905, 80.9524 },
    { 21.4286, 57.1429 },
    { 21.4286, 33.3333 },
    { 26.1905, 14.2857 },
    { 35.7143, 4.7619 },
    { 50, 0 },
    { 54.7619, 0 },
    { 69.0476, 4.7619 },
    { 78.5714, 14.2857 },
    { 83.3334, 28.5714 },
    { 83.3334, 33.3333 },
    { 78.5714, 47.619 },
    { 69.0476, 57.1429 },
    { 54.7619, 61.9048 },
    { 50, 61.9048 },
    { 35.7143, 57.1429 },
    { 26.1905, 47.619 },
    { 21.4286, 33.3333 },
};

static const StrokeRec monoChar54[] = {
   { 23, monoChar54_stroke0 },
};

/* char: 55 '7' */

static const CoordRec monoChar55_stroke0[] = {
    { 85.7143, 100 },
    { 38.0952, 0 },
};

static const CoordRec monoChar55_stroke1[] = {
    { 19.0476, 100 },
    { 85.7143, 100 },
};

static const StrokeRec monoChar55[] = {
   { 2, monoChar55_stroke0 },
   { 2, monoChar55_stroke1 },
};

/* char: 56 '8' */

static const CoordRec monoChar56_stroke0[] = {
    { 42.8571, 100 },
    { 28.5714, 95.2381 },
    { 23.8095, 85.7143 },
    { 23.8095, 76.1905 },
    { 28.5714, 66.6667 },
    { 38.0952, 61.9048 },
    { 57.1428, 57.1429 },
    { 71.4286, 52.381 },
    { 80.9524, 42.8571 },
    { 85.7143, 33.3333 },
    { 85.7143, 19.0476 },
    { 80.9524, 9.5238 },
    { 76.1905, 4.7619 },
    { 61.9047, 0 },
    { 42.8571, 0 },
    { 28.5714, 4.7619 },
    { 23.8095, 9.5238 },
    { 19.0476, 19.0476 },
    { 19.0476, 33.3333 },
    { 23.8095, 42.8571 },
    { 33.3333, 52.381 },
    { 47.619, 57.1429 },
    { 66.6666, 61.9048 },
    { 76.1905, 66.6667 },
    { 80.9524, 76.1905 },
    { 80.9524, 85.7143 },
    { 76.1905, 95.2381 },
    { 61.9047, 100 },
    { 42.8571, 100 },
};

static const StrokeRec monoChar56[] = {
   { 29, monoChar56_stroke0 },
};

/* char: 57 '9' */

static const CoordRec monoChar57_stroke0[] = {
    { 83.3334, 66.6667 },
    { 78.5714, 52.381 },
    { 69.0476, 42.8571 },
    { 54.7619, 38.0952 },
    { 50, 38.0952 },
    { 35.7143, 42.8571 },
    { 26.1905, 52.381 },
    { 21.4286, 66.6667 },
    { 21.4286, 71.4286 },
    { 26.1905, 85.7143 },
    { 35.7143, 95.2381 },
    { 50, 100 },
    { 54.7619, 100 },
    { 69.0476, 95.2381 },
    { 78.5714, 85.7143 },
    { 83.3334, 66.6667 },
    { 83.3334, 42.8571 },
    { 78.5714, 19.0476 },
    { 69.0476, 4.7619 },
    { 54.7619, 0 },
    { 45.2381, 0 },
    { 30.9524, 4.7619 },
    { 26.1905, 14.2857 },
};

static const StrokeRec monoChar57[] = {
   { 23, monoChar57_stroke0 },
};

/* char: 58 ':' */

static const CoordRec monoChar58_stroke0[] = {
    { 52.381, 66.6667 },
    { 47.6191, 61.9048 },
    { 52.381, 57.1429 },
    { 57.1429, 61.9048 },
    { 52.381, 66.6667 },
};

static const CoordRec monoChar58_stroke1[] = {
    { 52.381, 9.5238 },
    { 47.6191, 4.7619 },
    { 52.381, 0 },
    { 57.1429, 4.7619 },
    { 52.381, 9.5238 },
};

static const StrokeRec monoChar58[] = {
   { 5, monoChar58_stroke0 },
   { 5, monoChar58_stroke1 },
};

/* char: 59 ';' */

static const CoordRec monoChar59_stroke0[] = {
    { 52.381, 66.6667 },
    { 47.6191, 61.9048 },
    { 52.381, 57.1429 },
    { 57.1429, 61.9048 },
    { 52.381, 66.6667 },
};

static const CoordRec monoChar59_stroke1[] = {
    { 57.1429, 4.7619 },
    { 52.381, 0 },
    { 47.6191, 4.7619 },
    { 52.381, 9.5238 },
    { 57.1429, 4.7619 },
    { 57.1429, -4.7619 },
    { 52.381, -14.2857 },
    { 47.6191, -19.0476 },
};

static const StrokeRec monoChar59[] = {
   { 5, monoChar59_stroke0 },
   { 8, monoChar59_stroke1 },
};

/* char: 60 '<' */

static const CoordRec monoChar60_stroke0[] = {
    { 90.4762, 85.7143 },
    { 14.2857, 42.8571 },
    { 90.4762, 0 },
};

static const StrokeRec monoChar60[] = {
   { 3, monoChar60_stroke0 },
};

/* char: 61 '=' */

static const CoordRec monoChar61_stroke0[] = {
    { 9.5238, 57.1429 },
    { 95.2381, 57.1429 },
};

static const CoordRec monoChar61_stroke1[] = {
    { 9.5238, 28.5714 },
    { 95.2381, 28.5714 },
};

static const StrokeRec monoChar61[] = {
   { 2, monoChar61_stroke0 },
   { 2, monoChar61_stroke1 },
};

/* char: 62 '>' */

static const CoordRec monoChar62_stroke0[] = {
    { 14.2857, 85.7143 },
    { 90.4762, 42.8571 },
    { 14.2857, 0 },
};

static const StrokeRec monoChar62[] = {
   { 3, monoChar62_stroke0 },
};

/* char: 63 '?' */

static const CoordRec monoChar63_stroke0[] = {
    { 23.8095, 76.1905 },
    { 23.8095, 80.9524 },
    { 28.5714, 90.4762 },
    { 33.3333, 95.2381 },
    { 42.8571, 100 },
    { 61.9047, 100 },
    { 71.4285, 95.2381 },
    { 76.1905, 90.4762 },
    { 80.9524, 80.9524 },
    { 80.9524, 71.4286 },
    { 76.1905, 61.9048 },
    { 71.4285, 57.1429 },
    { 52.3809, 47.619 },
    { 52.3809, 33.3333 },
};

static const CoordRec monoChar63_stroke1[] = {
    { 52.3809, 9.5238 },
    { 47.619, 4.7619 },
    { 52.3809, 0 },
    { 57.1428, 4.7619 },
    { 52.3809, 9.5238 },
};

static const StrokeRec monoChar63[] = {
   { 14, monoChar63_stroke0 },
   { 5, monoChar63_stroke1 },
};

/* char: 64 '@' */

static const CoordRec monoChar64_stroke0[] = {
    { 64.2857, 52.381 },
    { 54.7619, 57.1429 },
    { 45.2381, 57.1429 },
    { 40.4762, 47.619 },
    { 40.4762, 42.8571 },
    { 45.2381, 33.3333 },
    { 54.7619, 33.3333 },
    { 64.2857, 38.0952 },
};

static const CoordRec monoChar64_stroke1[] = {
    { 64.2857, 57.1429 },
    { 64.2857, 38.0952 },
    { 69.0476, 33.3333 },
    { 78.5714, 33.3333 },
    { 83.3334, 42.8571 },
    { 83.3334, 47.619 },
    { 78.5714, 61.9048 },
    { 69.0476, 71.4286 },
    { 54.7619, 76.1905 },
    { 50, 76.1905 },
    { 35.7143, 71.4286 },
    { 26.1905, 61.9048 },
    { 21.4286, 47.619 },
    { 21.4286, 42.8571 },
    { 26.1905, 28.5714 },
    { 35.7143, 19.0476 },
    { 50, 14.2857 },
    { 54.7619, 14.2857 },
    { 69.0476, 19.0476 },
};

static const StrokeRec monoChar64[] = {
   { 8, monoChar64_stroke0 },
   { 19, monoChar64_stroke1 },
};

/* char: 65 'A' */

static const CoordRec monoChar65_stroke0[] = {
    { 52.3809, 100 },
    { 14.2857, 0 },
};

static const CoordRec monoChar65_stroke1[] = {
    { 52.3809, 100 },
    { 90.4762, 0 },
};

static const CoordRec monoChar65_stroke2[] = {
    { 28.5714, 33.3333 },
    { 76.1905, 33.3333 },
};

static const StrokeRec monoChar65[] = {
   { 2, monoChar65_stroke0 },
   { 2, monoChar65_stroke1 },
   { 2, monoChar65_stroke2 },
};

/* char: 66 'B' */

static const CoordRec monoChar66_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar66_stroke1[] = {
    { 19.0476, 100 },
    { 61.9047, 100 },
    { 76.1905, 95.2381 },
    { 80.9524, 90.4762 },
    { 85.7143, 80.9524 },
    { 85.7143, 71.4286 },
    { 80.9524, 61.9048 },
    { 76.1905, 57.1429 },
    { 61.9047, 52.381 },
};

static const CoordRec monoChar66_stroke2[] = {
    { 19.0476, 52.381 },
    { 61.9047, 52.381 },
    { 76.1905, 47.619 },
    { 80.9524, 42.8571 },
    { 85.7143, 33.3333 },
    { 85.7143, 19.0476 },
    { 80.9524, 9.5238 },
    { 76.1905, 4.7619 },
    { 61.9047, 0 },
    { 19.0476, 0 },
};

static const StrokeRec monoChar66[] = {
   { 2, monoChar66_stroke0 },
   { 9, monoChar66_stroke1 },
   { 10, monoChar66_stroke2 },
};

/* char: 67 'C' */

static const CoordRec monoChar67_stroke0[] = {
    { 88.0952, 76.1905 },
    { 83.3334, 85.7143 },
    { 73.8096, 95.2381 },
    { 64.2857, 100 },
    { 45.2381, 100 },
    { 35.7143, 95.2381 },
    { 26.1905, 85.7143 },
    { 21.4286, 76.1905 },
    { 16.6667, 61.9048 },
    { 16.6667, 38.0952 },
    { 21.4286, 23.8095 },
    { 26.1905, 14.2857 },
    { 35.7143, 4.7619 },
    { 45.2381, 0 },
    { 64.2857, 0 },
    { 73.8096, 4.7619 },
    { 83.3334, 14.2857 },
    { 88.0952, 23.8095 },
};

static const StrokeRec monoChar67[] = {
   { 18, monoChar67_stroke0 },
};

/* char: 68 'D' */

static const CoordRec monoChar68_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar68_stroke1[] = {
    { 19.0476, 100 },
    { 52.3809, 100 },
    { 66.6666, 95.2381 },
    { 76.1905, 85.7143 },
    { 80.9524, 76.1905 },
    { 85.7143, 61.9048 },
    { 85.7143, 38.0952 },
    { 80.9524, 23.8095 },
    { 76.1905, 14.2857 },
    { 66.6666, 4.7619 },
    { 52.3809, 0 },
    { 19.0476, 0 },
};

static const StrokeRec monoChar68[] = {
   { 2, monoChar68_stroke0 },
   { 12, monoChar68_stroke1 },
};

/* char: 69 'E' */

static const CoordRec monoChar69_stroke0[] = {
    { 21.4286, 100 },
    { 21.4286, 0 },
};

static const CoordRec monoChar69_stroke1[] = {
    { 21.4286, 100 },
    { 83.3334, 100 },
};

static const CoordRec monoChar69_stroke2[] = {
    { 21.4286, 52.381 },
    { 59.5238, 52.381 },
};

static const CoordRec monoChar69_stroke3[] = {
    { 21.4286, 0 },
    { 83.3334, 0 },
};

static const StrokeRec monoChar69[] = {
   { 2, monoChar69_stroke0 },
   { 2, monoChar69_stroke1 },
   { 2, monoChar69_stroke2 },
   { 2, monoChar69_stroke3 },
};

/* char: 70 'F' */

static const CoordRec monoChar70_stroke0[] = {
    { 21.4286, 100 },
    { 21.4286, 0 },
};

static const CoordRec monoChar70_stroke1[] = {
    { 21.4286, 100 },
    { 83.3334, 100 },
};

static const CoordRec monoChar70_stroke2[] = {
    { 21.4286, 52.381 },
    { 59.5238, 52.381 },
};

static const StrokeRec monoChar70[] = {
   { 2, monoChar70_stroke0 },
   { 2, monoChar70_stroke1 },
   { 2, monoChar70_stroke2 },
};

/* char: 71 'G' */

static const CoordRec monoChar71_stroke0[] = {
    { 88.0952, 76.1905 },
    { 83.3334, 85.7143 },
    { 73.8096, 95.2381 },
    { 64.2857, 100 },
    { 45.2381, 100 },
    { 35.7143, 95.2381 },
    { 26.1905, 85.7143 },
    { 21.4286, 76.1905 },
    { 16.6667, 61.9048 },
    { 16.6667, 38.0952 },
    { 21.4286, 23.8095 },
    { 26.1905, 14.2857 },
    { 35.7143, 4.7619 },
    { 45.2381, 0 },
    { 64.2857, 0 },
    { 73.8096, 4.7619 },
    { 83.3334, 14.2857 },
    { 88.0952, 23.8095 },
    { 88.0952, 38.0952 },
};

static const CoordRec monoChar71_stroke1[] = {
    { 64.2857, 38.0952 },
    { 88.0952, 38.0952 },
};

static const StrokeRec monoChar71[] = {
   { 19, monoChar71_stroke0 },
   { 2, monoChar71_stroke1 },
};

/* char: 72 'H' */

static const CoordRec monoChar72_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar72_stroke1[] = {
    { 85.7143, 100 },
    { 85.7143, 0 },
};

static const CoordRec monoChar72_stroke2[] = {
    { 19.0476, 52.381 },
    { 85.7143, 52.381 },
};

static const StrokeRec monoChar72[] = {
   { 2, monoChar72_stroke0 },
   { 2, monoChar72_stroke1 },
   { 2, monoChar72_stroke2 },
};

/* char: 73 'I' */

static const CoordRec monoChar73_stroke0[] = {
    { 52.381, 100 },
    { 52.381, 0 },
};

static const StrokeRec monoChar73[] = {
   { 2, monoChar73_stroke0 },
};

/* char: 74 'J' */

static const CoordRec monoChar74_stroke0[] = {
    { 76.1905, 100 },
    { 76.1905, 23.8095 },
    { 71.4286, 9.5238 },
    { 66.6667, 4.7619 },
    { 57.1429, 0 },
    { 47.6191, 0 },
    { 38.0953, 4.7619 },
    { 33.3334, 9.5238 },
    { 28.5715, 23.8095 },
    { 28.5715, 33.3333 },
};

static const StrokeRec monoChar74[] = {
   { 10, monoChar74_stroke0 },
};

/* char: 75 'K' */

static const CoordRec monoChar75_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar75_stroke1[] = {
    { 85.7143, 100 },
    { 19.0476, 33.3333 },
};

static const CoordRec monoChar75_stroke2[] = {
    { 42.8571, 57.1429 },
    { 85.7143, 0 },
};

static const StrokeRec monoChar75[] = {
   { 2, monoChar75_stroke0 },
   { 2, monoChar75_stroke1 },
   { 2, monoChar75_stroke2 },
};

/* char: 76 'L' */

static const CoordRec monoChar76_stroke0[] = {
    { 23.8095, 100 },
    { 23.8095, 0 },
};

static const CoordRec monoChar76_stroke1[] = {
    { 23.8095, 0 },
    { 80.9524, 0 },
};

static const StrokeRec monoChar76[] = {
   { 2, monoChar76_stroke0 },
   { 2, monoChar76_stroke1 },
};

/* char: 77 'M' */

static const CoordRec monoChar77_stroke0[] = {
    { 14.2857, 100 },
    { 14.2857, 0 },
};

static const CoordRec monoChar77_stroke1[] = {
    { 14.2857, 100 },
    { 52.3809, 0 },
};

static const CoordRec monoChar77_stroke2[] = {
    { 90.4762, 100 },
    { 52.3809, 0 },
};

static const CoordRec monoChar77_stroke3[] = {
    { 90.4762, 100 },
    { 90.4762, 0 },
};

static const StrokeRec monoChar77[] = {
   { 2, monoChar77_stroke0 },
   { 2, monoChar77_stroke1 },
   { 2, monoChar77_stroke2 },
   { 2, monoChar77_stroke3 },
};

/* char: 78 'N' */

static const CoordRec monoChar78_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar78_stroke1[] = {
    { 19.0476, 100 },
    { 85.7143, 0 },
};

static const CoordRec monoChar78_stroke2[] = {
    { 85.7143, 100 },
    { 85.7143, 0 },
};

static const StrokeRec monoChar78[] = {
   { 2, monoChar78_stroke0 },
   { 2, monoChar78_stroke1 },
   { 2, monoChar78_stroke2 },
};

/* char: 79 'O' */

static const CoordRec monoChar79_stroke0[] = {
    { 42.8571, 100 },
    { 33.3333, 95.2381 },
    { 23.8095, 85.7143 },
    { 19.0476, 76.1905 },
    { 14.2857, 61.9048 },
    { 14.2857, 38.0952 },
    { 19.0476, 23.8095 },
    { 23.8095, 14.2857 },
    { 33.3333, 4.7619 },
    { 42.8571, 0 },
    { 61.9047, 0 },
    { 71.4286, 4.7619 },
    { 80.9524, 14.2857 },
    { 85.7143, 23.8095 },
    { 90.4762, 38.0952 },
    { 90.4762, 61.9048 },
    { 85.7143, 76.1905 },
    { 80.9524, 85.7143 },
    { 71.4286, 95.2381 },
    { 61.9047, 100 },
    { 42.8571, 100 },
};

static const StrokeRec monoChar79[] = {
   { 21, monoChar79_stroke0 },
};

/* char: 80 'P' */

static const CoordRec monoChar80_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar80_stroke1[] = {
    { 19.0476, 100 },
    { 61.9047, 100 },
    { 76.1905, 95.2381 },
    { 80.9524, 90.4762 },
    { 85.7143, 80.9524 },
    { 85.7143, 66.6667 },
    { 80.9524, 57.1429 },
    { 76.1905, 52.381 },
    { 61.9047, 47.619 },
    { 19.0476, 47.619 },
};

static const StrokeRec monoChar80[] = {
   { 2, monoChar80_stroke0 },
   { 10, monoChar80_stroke1 },
};

/* char: 81 'Q' */

static const CoordRec monoChar81_stroke0[] = {
    { 42.8571, 100 },
    { 33.3333, 95.2381 },
    { 23.8095, 85.7143 },
    { 19.0476, 76.1905 },
    { 14.2857, 61.9048 },
    { 14.2857, 38.0952 },
    { 19.0476, 23.8095 },
    { 23.8095, 14.2857 },
    { 33.3333, 4.7619 },
    { 42.8571, 0 },
    { 61.9047, 0 },
    { 71.4286, 4.7619 },
    { 80.9524, 14.2857 },
    { 85.7143, 23.8095 },
    { 90.4762, 38.0952 },
    { 90.4762, 61.9048 },
    { 85.7143, 76.1905 },
    { 80.9524, 85.7143 },
    { 71.4286, 95.2381 },
    { 61.9047, 100 },
    { 42.8571, 100 },
};

static const CoordRec monoChar81_stroke1[] = {
    { 57.1428, 19.0476 },
    { 85.7143, -9.5238 },
};

static const StrokeRec monoChar81[] = {
   { 21, monoChar81_stroke0 },
   { 2, monoChar81_stroke1 },
};

/* char: 82 'R' */

static const CoordRec monoChar82_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar82_stroke1[] = {
    { 19.0476, 100 },
    { 61.9047, 100 },
    { 76.1905, 95.2381 },
    { 80.9524, 90.4762 },
    { 85.7143, 80.9524 },
    { 85.7143, 71.4286 },
    { 80.9524, 61.9048 },
    { 76.1905, 57.1429 },
    { 61.9047, 52.381 },
    { 19.0476, 52.381 },
};

static const CoordRec monoChar82_stroke2[] = {
    { 52.3809, 52.381 },
    { 85.7143, 0 },
};

static const StrokeRec monoChar82[] = {
   { 2, monoChar82_stroke0 },
   { 10, monoChar82_stroke1 },
   { 2, monoChar82_stroke2 },
};

/* char: 83 'S' */

static const CoordRec monoChar83_stroke0[] = {
    { 85.7143, 85.7143 },
    { 76.1905, 95.2381 },
    { 61.9047, 100 },
    { 42.8571, 100 },
    { 28.5714, 95.2381 },
    { 19.0476, 85.7143 },
    { 19.0476, 76.1905 },
    { 23.8095, 66.6667 },
    { 28.5714, 61.9048 },
    { 38.0952, 57.1429 },
    { 66.6666, 47.619 },
    { 76.1905, 42.8571 },
    { 80.9524, 38.0952 },
    { 85.7143, 28.5714 },
    { 85.7143, 14.2857 },
    { 76.1905, 4.7619 },
    { 61.9047, 0 },
    { 42.8571, 0 },
    { 28.5714, 4.7619 },
    { 19.0476, 14.2857 },
};

static const StrokeRec monoChar83[] = {
   { 20, monoChar83_stroke0 },
};

/* char: 84 'T' */

static const CoordRec monoChar84_stroke0[] = {
    { 52.3809, 100 },
    { 52.3809, 0 },
};

static const CoordRec monoChar84_stroke1[] = {
    { 19.0476, 100 },
    { 85.7143, 100 },
};

static const StrokeRec monoChar84[] = {
   { 2, monoChar84_stroke0 },
   { 2, monoChar84_stroke1 },
};

/* char: 85 'U' */

static const CoordRec monoChar85_stroke0[] = {
    { 19.0476, 100 },
    { 19.0476, 28.5714 },
    { 23.8095, 14.2857 },
    { 33.3333, 4.7619 },
    { 47.619, 0 },
    { 57.1428, 0 },
    { 71.4286, 4.7619 },
    { 80.9524, 14.2857 },
    { 85.7143, 28.5714 },
    { 85.7143, 100 },
};

static const StrokeRec monoChar85[] = {
   { 10, monoChar85_stroke0 },
};

/* char: 86 'V' */

static const CoordRec monoChar86_stroke0[] = {
    { 14.2857, 100 },
    { 52.3809, 0 },
};

static const CoordRec monoChar86_stroke1[] = {
    { 90.4762, 100 },
    { 52.3809, 0 },
};

static const StrokeRec monoChar86[] = {
   { 2, monoChar86_stroke0 },
   { 2, monoChar86_stroke1 },
};

/* char: 87 'W' */

static const CoordRec monoChar87_stroke0[] = {
    { 4.7619, 100 },
    { 28.5714, 0 },
};

static const CoordRec monoChar87_stroke1[] = {
    { 52.3809, 100 },
    { 28.5714, 0 },
};

static const CoordRec monoChar87_stroke2[] = {
    { 52.3809, 100 },
    { 76.1905, 0 },
};

static const CoordRec monoChar87_stroke3[] = {
    { 100, 100 },
    { 76.1905, 0 },
};

static const StrokeRec monoChar87[] = {
   { 2, monoChar87_stroke0 },
   { 2, monoChar87_stroke1 },
   { 2, monoChar87_stroke2 },
   { 2, monoChar87_stroke3 },
};

/* char: 88 'X' */

static const CoordRec monoChar88_stroke0[] = {
    { 19.0476, 100 },
    { 85.7143, 0 },
};

static const CoordRec monoChar88_stroke1[] = {
    { 85.7143, 100 },
    { 19.0476, 0 },
};

static const StrokeRec monoChar88[] = {
   { 2, monoChar88_stroke0 },
   { 2, monoChar88_stroke1 },
};

/* char: 89 'Y' */

static const CoordRec monoChar89_stroke0[] = {
    { 14.2857, 100 },
    { 52.3809, 52.381 },
    { 52.3809, 0 },
};

static const CoordRec monoChar89_stroke1[] = {
    { 90.4762, 100 },
    { 52.3809, 52.381 },
};

static const StrokeRec monoChar89[] = {
   { 3, monoChar89_stroke0 },
   { 2, monoChar89_stroke1 },
};

/* char: 90 'Z' */

static const CoordRec monoChar90_stroke0[] = {
    { 85.7143, 100 },
    { 19.0476, 0 },
};

static const CoordRec monoChar90_stroke1[] = {
    { 19.0476, 100 },
    { 85.7143, 100 },
};

static const CoordRec monoChar90_stroke2[] = {
    { 19.0476, 0 },
    { 85.7143, 0 },
};

static const StrokeRec monoChar90[] = {
   { 2, monoChar90_stroke0 },
   { 2, monoChar90_stroke1 },
   { 2, monoChar90_stroke2 },
};

/* char: 91 '[' */

static const CoordRec monoChar91_stroke0[] = {
    { 35.7143, 119.048 },
    { 35.7143, -33.3333 },
};

static const CoordRec monoChar91_stroke1[] = {
    { 40.4762, 119.048 },
    { 40.4762, -33.3333 },
};

static const CoordRec monoChar91_stroke2[] = {
    { 35.7143, 119.048 },
    { 69.0476, 119.048 },
};

static const CoordRec monoChar91_stroke3[] = {
    { 35.7143, -33.3333 },
    { 69.0476, -33.3333 },
};

static const StrokeRec monoChar91[] = {
   { 2, monoChar91_stroke0 },
   { 2, monoChar91_stroke1 },
   { 2, monoChar91_stroke2 },
   { 2, monoChar91_stroke3 },
};

/* char: 92 '\' */

static const CoordRec monoChar92_stroke0[] = {
    { 19.0476, 100 },
    { 85.7143, -14.2857 },
};

static const StrokeRec monoChar92[] = {
   { 2, monoChar92_stroke0 },
};

/* char: 93 ']' */

static const CoordRec monoChar93_stroke0[] = {
    { 64.2857, 119.048 },
    { 64.2857, -33.3333 },
};

static const CoordRec monoChar93_stroke1[] = {
    { 69.0476, 119.048 },
    { 69.0476, -33.3333 },
};

static const CoordRec monoChar93_stroke2[] = {
    { 35.7143, 119.048 },
    { 69.0476, 119.048 },
};

static const CoordRec monoChar93_stroke3[] = {
    { 35.7143, -33.3333 },
    { 69.0476, -33.3333 },
};

static const StrokeRec monoChar93[] = {
   { 2, monoChar93_stroke0 },
   { 2, monoChar93_stroke1 },
   { 2, monoChar93_stroke2 },
   { 2, monoChar93_stroke3 },
};

/* char: 94 '^' */

static const CoordRec monoChar94_stroke0[] = {
    { 52.3809, 109.524 },
    { 14.2857, 42.8571 },
};

static const CoordRec monoChar94_stroke1[] = {
    { 52.3809, 109.524 },
    { 90.4762, 42.8571 },
};

static const StrokeRec monoChar94[] = {
   { 2, monoChar94_stroke0 },
   { 2, monoChar94_stroke1 },
};

/* char: 95 '_' */

static const CoordRec monoChar95_stroke0[] = {
    { 0, -33.3333 },
    { 104.762, -33.3333 },
    { 104.762, -28.5714 },
    { 0, -28.5714 },
    { 0, -33.3333 },
};

static const StrokeRec monoChar95[] = {
   { 5, monoChar95_stroke0 },
};

/* char: 96 '`' */

static const CoordRec monoChar96_stroke0[] = {
    { 42.8572, 100 },
    { 66.6667, 71.4286 },
};

static const CoordRec monoChar96_stroke1[] = {
    { 42.8572, 100 },
    { 38.0953, 95.2381 },
    { 66.6667, 71.4286 },
};

static const StrokeRec monoChar96[] = {
   { 2, monoChar96_stroke0 },
   { 3, monoChar96_stroke1 },
};

/* char: 97 'a' */

static const CoordRec monoChar97_stroke0[] = {
    { 80.9524, 66.6667 },
    { 80.9524, 0 },
};

static const CoordRec monoChar97_stroke1[] = {
    { 80.9524, 52.381 },
    { 71.4285, 61.9048 },
    { 61.9047, 66.6667 },
    { 47.619, 66.6667 },
    { 38.0952, 61.9048 },
    { 28.5714, 52.381 },
    { 23.8095, 38.0952 },
    { 23.8095, 28.5714 },
    { 28.5714, 14.2857 },
    { 38.0952, 4.7619 },
    { 47.619, 0 },
    { 61.9047, 0 },
    { 71.4285, 4.7619 },
    { 80.9524, 14.2857 },
};

static const StrokeRec monoChar97[] = {
   { 2, monoChar97_stroke0 },
   { 14, monoChar97_stroke1 },
};

/* char: 98 'b' */

static const CoordRec monoChar98_stroke0[] = {
    { 23.8095, 100 },
    { 23.8095, 0 },
};

static const CoordRec monoChar98_stroke1[] = {
    { 23.8095, 52.381 },
    { 33.3333, 61.9048 },
    { 42.8571, 66.6667 },
    { 57.1428, 66.6667 },
    { 66.6666, 61.9048 },
    { 76.1905, 52.381 },
    { 80.9524, 38.0952 },
    { 80.9524, 28.5714 },
    { 76.1905, 14.2857 },
    { 66.6666, 4.7619 },
    { 57.1428, 0 },
    { 42.8571, 0 },
    { 33.3333, 4.7619 },
    { 23.8095, 14.2857 },
};

static const StrokeRec monoChar98[] = {
   { 2, monoChar98_stroke0 },
   { 14, monoChar98_stroke1 },
};

/* char: 99 'c' */

static const CoordRec monoChar99_stroke0[] = {
    { 80.9524, 52.381 },
    { 71.4285, 61.9048 },
    { 61.9047, 66.6667 },
    { 47.619, 66.6667 },
    { 38.0952, 61.9048 },
    { 28.5714, 52.381 },
    { 23.8095, 38.0952 },
    { 23.8095, 28.5714 },
    { 28.5714, 14.2857 },
    { 38.0952, 4.7619 },
    { 47.619, 0 },
    { 61.9047, 0 },
    { 71.4285, 4.7619 },
    { 80.9524, 14.2857 },
};

static const StrokeRec monoChar99[] = {
   { 14, monoChar99_stroke0 },
};

/* char: 100 'd' */

static const CoordRec monoChar100_stroke0[] = {
    { 80.9524, 100 },
    { 80.9524, 0 },
};

static const CoordRec monoChar100_stroke1[] = {
    { 80.9524, 52.381 },
    { 71.4285, 61.9048 },
    { 61.9047, 66.6667 },
    { 47.619, 66.6667 },
    { 38.0952, 61.9048 },
    { 28.5714, 52.381 },
    { 23.8095, 38.0952 },
    { 23.8095, 28.5714 },
    { 28.5714, 14.2857 },
    { 38.0952, 4.7619 },
    { 47.619, 0 },
    { 61.9047, 0 },
    { 71.4285, 4.7619 },
    { 80.9524, 14.2857 },
};

static const StrokeRec monoChar100[] = {
   { 2, monoChar100_stroke0 },
   { 14, monoChar100_stroke1 },
};

/* char: 101 'e' */

static const CoordRec monoChar101_stroke0[] = {
    { 23.8095, 38.0952 },
    { 80.9524, 38.0952 },
    { 80.9524, 47.619 },
    { 76.1905, 57.1429 },
    { 71.4285, 61.9048 },
    { 61.9047, 66.6667 },
    { 47.619, 66.6667 },
    { 38.0952, 61.9048 },
    { 28.5714, 52.381 },
    { 23.8095, 38.0952 },
    { 23.8095, 28.5714 },
    { 28.5714, 14.2857 },
    { 38.0952, 4.7619 },
    { 47.619, 0 },
    { 61.9047, 0 },
    { 71.4285, 4.7619 },
    { 80.9524, 14.2857 },
};

static const StrokeRec monoChar101[] = {
   { 17, monoChar101_stroke0 },
};

/* char: 102 'f' */

static const CoordRec monoChar102_stroke0[] = {
    { 71.4286, 100 },
    { 61.9048, 100 },
    { 52.381, 95.2381 },
    { 47.6191, 80.9524 },
    { 47.6191, 0 },
};

static const CoordRec monoChar102_stroke1[] = {
    { 33.3334, 66.6667 },
    { 66.6667, 66.6667 },
};

static const StrokeRec monoChar102[] = {
   { 5, monoChar102_stroke0 },
   { 2, monoChar102_stroke1 },
};

/* char: 103 'g' */

static const CoordRec monoChar103_stroke0[] = {
    { 80.9524, 66.6667 },
    { 80.9524, -9.5238 },
    { 76.1905, -23.8095 },
    { 71.4285, -28.5714 },
    { 61.9047, -33.3333 },
    { 47.619, -33.3333 },
    { 38.0952, -28.5714 },
};

static const CoordRec monoChar103_stroke1[] = {
    { 80.9524, 52.381 },
    { 71.4285, 61.9048 },
    { 61.9047, 66.6667 },
    { 47.619, 66.6667 },
    { 38.0952, 61.9048 },
    { 28.5714, 52.381 },
    { 23.8095, 38.0952 },
    { 23.8095, 28.5714 },
    { 28.5714, 14.2857 },
    { 38.0952, 4.7619 },
    { 47.619, 0 },
    { 61.9047, 0 },
    { 71.4285, 4.7619 },
    { 80.9524, 14.2857 },
};

static const StrokeRec monoChar103[] = {
   { 7, monoChar103_stroke0 },
   { 14, monoChar103_stroke1 },
};

/* char: 104 'h' */

static const CoordRec monoChar104_stroke0[] = {
    { 26.1905, 100 },
    { 26.1905, 0 },
};

static const CoordRec monoChar104_stroke1[] = {
    { 26.1905, 47.619 },
    { 40.4762, 61.9048 },
    { 50, 66.6667 },
    { 64.2857, 66.6667 },
    { 73.8095, 61.9048 },
    { 78.5715, 47.619 },
    { 78.5715, 0 },
};

static const StrokeRec monoChar104[] = {
   { 2, monoChar104_stroke0 },
   { 7, monoChar104_stroke1 },
};

/* char: 105 'i' */

static const CoordRec monoChar105_stroke0[] = {
    { 47.6191, 100 },
    { 52.381, 95.2381 },
    { 57.1429, 100 },
    { 52.381, 104.762 },
    { 47.6191, 100 },
};

static const CoordRec monoChar105_stroke1[] = {
    { 52.381, 66.6667 },
    { 52.381, 0 },
};

static const StrokeRec monoChar105[] = {
   { 5, monoChar105_stroke0 },
   { 2, monoChar105_stroke1 },
};

/* char: 106 'j' */

static const CoordRec monoChar106_stroke0[] = {
    { 57.1429, 100 },
    { 61.9048, 95.2381 },
    { 66.6667, 100 },
    { 61.9048, 104.762 },
    { 57.1429, 100 },
};

static const CoordRec monoChar106_stroke1[] = {
    { 61.9048, 66.6667 },
    { 61.9048, -14.2857 },
    { 57.1429, -28.5714 },
    { 47.6191, -33.3333 },
    { 38.0953, -33.3333 },
};

static const StrokeRec monoChar106[] = {
   { 5, monoChar106_stroke0 },
   { 5, monoChar106_stroke1 },
};

/* char: 107 'k' */

static const CoordRec monoChar107_stroke0[] = {
    { 26.1905, 100 },
    { 26.1905, 0 },
};

static const CoordRec monoChar107_stroke1[] = {
    { 73.8095, 66.6667 },
    { 26.1905, 19.0476 },
};

static const CoordRec monoChar107_stroke2[] = {
    { 45.2381, 38.0952 },
    { 78.5715, 0 },
};

static const StrokeRec monoChar107[] = {
   { 2, monoChar107_stroke0 },
   { 2, monoChar107_stroke1 },
   { 2, monoChar107_stroke2 },
};

/* char: 108 'l' */

static const CoordRec monoChar108_stroke0[] = {
    { 52.381, 100 },
    { 52.381, 0 },
};

static const StrokeRec monoChar108[] = {
   { 2, monoChar108_stroke0 },
};

/* char: 109 'm' */

static const CoordRec monoChar109_stroke0[] = {
    { 0, 66.6667 },
    { 0, 0 },
};

static const CoordRec monoChar109_stroke1[] = {
    { 0, 47.619 },
    { 14.2857, 61.9048 },
    { 23.8095, 66.6667 },
    { 38.0952, 66.6667 },
    { 47.619, 61.9048 },
    { 52.381, 47.619 },
    { 52.381, 0 },
};

static const CoordRec monoChar109_stroke2[] = {
    { 52.381, 47.619 },
    { 66.6667, 61.9048 },
    { 76.1905, 66.6667 },
    { 90.4762, 66.6667 },
    { 100, 61.9048 },
    { 104.762, 47.619 },
    { 104.762, 0 },
};

static const StrokeRec monoChar109[] = {
   { 2, monoChar109_stroke0 },
   { 7, monoChar109_stroke1 },
   { 7, monoChar109_stroke2 },
};

/* char: 110 'n' */

static const CoordRec monoChar110_stroke0[] = {
    { 26.1905, 66.6667 },
    { 26.1905, 0 },
};

static const CoordRec monoChar110_stroke1[] = {
    { 26.1905, 47.619 },
    { 40.4762, 61.9048 },
    { 50, 66.6667 },
    { 64.2857, 66.6667 },
    { 73.8095, 61.9048 },
    { 78.5715, 47.619 },
    { 78.5715, 0 },
};

static const StrokeRec monoChar110[] = {
   { 2, monoChar110_stroke0 },
   { 7, monoChar110_stroke1 },
};

/* char: 111 'o' */

static const CoordRec monoChar111_stroke0[] = {
    { 45.2381, 66.6667 },
    { 35.7143, 61.9048 },
    { 26.1905, 52.381 },
    { 21.4286, 38.0952 },
    { 21.4286, 28.5714 },
    { 26.1905, 14.2857 },
    { 35.7143, 4.7619 },
    { 45.2381, 0 },
    { 59.5238, 0 },
    { 69.0476, 4.7619 },
    { 78.5714, 14.2857 },
    { 83.3334, 28.5714 },
    { 83.3334, 38.0952 },
    { 78.5714, 52.381 },
    { 69.0476, 61.9048 },
    { 59.5238, 66.6667 },
    { 45.2381, 66.6667 },
};

static const StrokeRec monoChar111[] = {
   { 17, monoChar111_stroke0 },
};

/* char: 112 'p' */

static const CoordRec monoChar112_stroke0[] = {
    { 23.8095, 66.6667 },
    { 23.8095, -33.3333 },
};

static const CoordRec monoChar112_stroke1[] = {
    { 23.8095, 52.381 },
    { 33.3333, 61.9048 },
    { 42.8571, 66.6667 },
    { 57.1428, 66.6667 },
    { 66.6666, 61.9048 },
    { 76.1905, 52.381 },
    { 80.9524, 38.0952 },
    { 80.9524, 28.5714 },
    { 76.1905, 14.2857 },
    { 66.6666, 4.7619 },
    { 57.1428, 0 },
    { 42.8571, 0 },
    { 33.3333, 4.7619 },
    { 23.8095, 14.2857 },
};

static const StrokeRec monoChar112[] = {
   { 2, monoChar112_stroke0 },
   { 14, monoChar112_stroke1 },
};

/* char: 113 'q' */

static const CoordRec monoChar113_stroke0[] = {
    { 80.9524, 66.6667 },
    { 80.9524, -33.3333 },
};

static const CoordRec monoChar113_stroke1[] = {
    { 80.9524, 52.381 },
    { 71.4285, 61.9048 },
    { 61.9047, 66.6667 },
    { 47.619, 66.6667 },
    { 38.0952, 61.9048 },
    { 28.5714, 52.381 },
    { 23.8095, 38.0952 },
    { 23.8095, 28.5714 },
    { 28.5714, 14.2857 },
    { 38.0952, 4.7619 },
    { 47.619, 0 },
    { 61.9047, 0 },
    { 71.4285, 4.7619 },
    { 80.9524, 14.2857 },
};

static const StrokeRec monoChar113[] = {
   { 2, monoChar113_stroke0 },
   { 14, monoChar113_stroke1 },
};

/* char: 114 'r' */

static const CoordRec monoChar114_stroke0[] = {
    { 33.3334, 66.6667 },
    { 33.3334, 0 },
};

static const CoordRec monoChar114_stroke1[] = {
    { 33.3334, 38.0952 },
    { 38.0953, 52.381 },
    { 47.6191, 61.9048 },
    { 57.1429, 66.6667 },
    { 71.4286, 66.6667 },
};

static const StrokeRec monoChar114[] = {
   { 2, monoChar114_stroke0 },
   { 5, monoChar114_stroke1 },
};

/* char: 115 's' */

static const CoordRec monoChar115_stroke0[] = {
    { 78.5715, 52.381 },
    { 73.8095, 61.9048 },
    { 59.5238, 66.6667 },
    { 45.2381, 66.6667 },
    { 30.9524, 61.9048 },
    { 26.1905, 52.381 },
    { 30.9524, 42.8571 },
    { 40.4762, 38.0952 },
    { 64.2857, 33.3333 },
    { 73.8095, 28.5714 },
    { 78.5715, 19.0476 },
    { 78.5715, 14.2857 },
    { 73.8095, 4.7619 },
    { 59.5238, 0 },
    { 45.2381, 0 },
    { 30.9524, 4.7619 },
    { 26.1905, 14.2857 },
};

static const StrokeRec monoChar115[] = {
   { 17, monoChar115_stroke0 },
};

/* char: 116 't' */

static const CoordRec monoChar116_stroke0[] = {
    { 47.6191, 100 },
    { 47.6191, 19.0476 },
    { 52.381, 4.7619 },
    { 61.9048, 0 },
    { 71.4286, 0 },
};

static const CoordRec monoChar116_stroke1[] = {
    { 33.3334, 66.6667 },
    { 66.6667, 66.6667 },
};

static const StrokeRec monoChar116[] = {
   { 5, monoChar116_stroke0 },
   { 2, monoChar116_stroke1 },
};

/* char: 117 'u' */

static const CoordRec monoChar117_stroke0[] = {
    { 26.1905, 66.6667 },
    { 26.1905, 19.0476 },
    { 30.9524, 4.7619 },
    { 40.4762, 0 },
    { 54.7619, 0 },
    { 64.2857, 4.7619 },
    { 78.5715, 19.0476 },
};

static const CoordRec monoChar117_stroke1[] = {
    { 78.5715, 66.6667 },
    { 78.5715, 0 },
};

static const StrokeRec monoChar117[] = {
   { 7, monoChar117_stroke0 },
   { 2, monoChar117_stroke1 },
};

/* char: 118 'v' */

static const CoordRec monoChar118_stroke0[] = {
    { 23.8095, 66.6667 },
    { 52.3809, 0 },
};

static const CoordRec monoChar118_stroke1[] = {
    { 80.9524, 66.6667 },
    { 52.3809, 0 },
};

static const StrokeRec monoChar118[] = {
   { 2, monoChar118_stroke0 },
   { 2, monoChar118_stroke1 },
};

/* char: 119 'w' */

static const CoordRec monoChar119_stroke0[] = {
    { 14.2857, 66.6667 },
    { 33.3333, 0 },
};

static const CoordRec monoChar119_stroke1[] = {
    { 52.3809, 66.6667 },
    { 33.3333, 0 },
};

static const CoordRec monoChar119_stroke2[] = {
    { 52.3809, 66.6667 },
    { 71.4286, 0 },
};

static const CoordRec monoChar119_stroke3[] = {
    { 90.4762, 66.6667 },
    { 71.4286, 0 },
};

static const StrokeRec monoChar119[] = {
   { 2, monoChar119_stroke0 },
   { 2, monoChar119_stroke1 },
   { 2, monoChar119_stroke2 },
   { 2, monoChar119_stroke3 },
};

/* char: 120 'x' */

static const CoordRec monoChar120_stroke0[] = {
    { 26.1905, 66.6667 },
    { 78.5715, 0 },
};

static const CoordRec monoChar120_stroke1[] = {
    { 78.5715, 66.6667 },
    { 26.1905, 0 },
};

static const StrokeRec monoChar120[] = {
   { 2, monoChar120_stroke0 },
   { 2, monoChar120_stroke1 },
};

/* char: 121 'y' */

static const CoordRec monoChar121_stroke0[] = {
    { 26.1905, 66.6667 },
    { 54.7619, 0 },
};

static const CoordRec monoChar121_stroke1[] = {
    { 83.3334, 66.6667 },
    { 54.7619, 0 },
    { 45.2381, -19.0476 },
    { 35.7143, -28.5714 },
    { 26.1905, -33.3333 },
    { 21.4286, -33.3333 },
};

static const StrokeRec monoChar121[] = {
   { 2, monoChar121_stroke0 },
   { 6, monoChar121_stroke1 },
};

/* char: 122 'z' */

static const CoordRec monoChar122_stroke0[] = {
    { 78.5715, 66.6667 },
    { 26.1905, 0 },
};

static const CoordRec monoChar122_stroke1[] = {
    { 26.1905, 66.6667 },
    { 78.5715, 66.6667 },
};

static const CoordRec monoChar122_stroke2[] = {
    { 26.1905, 0 },
    { 78.5715, 0 },
};

static const StrokeRec monoChar122[] = {
   { 2, monoChar122_stroke0 },
   { 2, monoChar122_stroke1 },
   { 2, monoChar122_stroke2 },
};

/* char: 123 '{' */

static const CoordRec monoChar123_stroke0[] = {
    { 64.2857, 119.048 },
    { 54.7619, 114.286 },
    { 50, 109.524 },
    { 45.2381, 100 },
    { 45.2381, 90.4762 },
    { 50, 80.9524 },
    { 54.7619, 76.1905 },
    { 59.5238, 66.6667 },
    { 59.5238, 57.1429 },
    { 50, 47.619 },
};

static const CoordRec monoChar123_stroke1[] = {
    { 54.7619, 114.286 },
    { 50, 104.762 },
    { 50, 95.2381 },
    { 54.7619, 85.7143 },
    { 59.5238, 80.9524 },
    { 64.2857, 71.4286 },
    { 64.2857, 61.9048 },
    { 59.5238, 52.381 },
    { 40.4762, 42.8571 },
    { 59.5238, 33.3333 },
    { 64.2857, 23.8095 },
    { 64.2857, 14.2857 },
    { 59.5238, 4.7619 },
    { 54.7619, 0 },
    { 50, -9.5238 },
    { 50, -19.0476 },
    { 54.7619, -28.5714 },
};

static const CoordRec monoChar123_stroke2[] = {
    { 50, 38.0952 },
    { 59.5238, 28.5714 },
    { 59.5238, 19.0476 },
    { 54.7619, 9.5238 },
    { 50, 4.7619 },
    { 45.2381, -4.7619 },
    { 45.2381, -14.2857 },
    { 50, -23.8095 },
    { 54.7619, -28.5714 },
    { 64.2857, -33.3333 },
};

static const StrokeRec monoChar123[] = {
   { 10, monoChar123_stroke0 },
   { 17, monoChar123_stroke1 },
   { 10, monoChar123_stroke2 },
};

/* char: 124 '|' */

static const CoordRec monoChar124_stroke0[] = {
    { 52.381, 119.048 },
    { 52.381, -33.3333 },
};

static const StrokeRec monoChar124[] = {
   { 2, monoChar124_stroke0 },
};

/* char: 125 '}' */

static const CoordRec monoChar125_stroke0[] = {
    { 40.4762, 119.048 },
    { 50, 114.286 },
    { 54.7619, 109.524 },
    { 59.5238, 100 },
    { 59.5238, 90.4762 },
    { 54.7619, 80.9524 },
    { 50, 76.1905 },
    { 45.2381, 66.6667 },
    { 45.2381, 57.1429 },
    { 54.7619, 47.619 },
};

static const CoordRec monoChar125_stroke1[] = {
    { 50, 114.286 },
    { 54.7619, 104.762 },
    { 54.7619, 95.2381 },
    { 50, 85.7143 },
    { 45.2381, 80.9524 },
    { 40.4762, 71.4286 },
    { 40.4762, 61.9048 },
    { 45.2381, 52.381 },
    { 64.2857, 42.8571 },
    { 45.2381, 33.3333 },
    { 40.4762, 23.8095 },
    { 40.4762, 14.2857 },
    { 45.2381, 4.7619 },
    { 50, 0 },
    { 54.7619, -9.5238 },
    { 54.7619, -19.0476 },
    { 50, -28.5714 },
};

static const CoordRec monoChar125_stroke2[] = {
    { 54.7619, 38.0952 },
    { 45.2381, 28.5714 },
    { 45.2381, 19.0476 },
    { 50, 9.5238 },
    { 54.7619, 4.7619 },
    { 59.5238, -4.7619 },
    { 59.5238, -14.2857 },
    { 54.7619, -23.8095 },
    { 50, -28.5714 },
    { 40.4762, -33.3333 },
};

static const StrokeRec monoChar125[] = {
   { 10, monoChar125_stroke0 },
   { 17, monoChar125_stroke1 },
   { 10, monoChar125_stroke2 },
};

/* char: 126 '~' */

static const CoordRec monoChar126_stroke0[] = {
    { 9.5238, 28.5714 },
    { 9.5238, 38.0952 },
    { 14.2857, 52.381 },
    { 23.8095, 57.1429 },
    { 33.3333, 57.1429 },
    { 42.8571, 52.381 },
    { 61.9048, 38.0952 },
    { 71.4286, 33.3333 },
    { 80.9524, 33.3333 },
    { 90.4762, 38.0952 },
    { 95.2381, 47.619 },
};

static const CoordRec monoChar126_stroke1[] = {
    { 9.5238, 38.0952 },
    { 14.2857, 47.619 },
    { 23.8095, 52.381 },
    { 33.3333, 52.381 },
    { 42.8571, 47.619 },
    { 61.9048, 33.3333 },
    { 71.4286, 28.5714 },
    { 80.9524, 28.5714 },
    { 90.4762, 33.3333 },
    { 95.2381, 47.619 },
    { 95.2381, 57.1429 },
};

static const StrokeRec monoChar126[] = {
   { 11, monoChar126_stroke0 },
   { 11, monoChar126_stroke1 },
};

/* char: 127 */

static const CoordRec monoChar127_stroke0[] = {
    { 71.4286, 100 },
    { 33.3333, -33.3333 },
};

static const CoordRec monoChar127_stroke1[] = {
    { 47.619, 66.6667 },
    { 33.3333, 61.9048 },
    { 23.8095, 52.381 },
    { 19.0476, 38.0952 },
    { 19.0476, 23.8095 },
    { 23.8095, 14.2857 },
    { 33.3333, 4.7619 },
    { 47.619, 0 },
    { 57.1428, 0 },
    { 71.4286, 4.7619 },
    { 80.9524, 14.2857 },
    { 85.7143, 28.5714 },
    { 85.7143, 42.8571 },
    { 80.9524, 52.381 },
    { 71.4286, 61.9048 },
    { 57.1428, 66.6667 },
    { 47.619, 66.6667 },
};

static const StrokeRec monoChar127[] = {
   { 2, monoChar127_stroke0 },
   { 17, monoChar127_stroke1 },
};

static const StrokeCharRec monoChars[] = {
    { 0, /* monoChar0 */ 0, 0, 0 },
    { 0, /* monoChar1 */ 0, 0, 0 },
    { 0, /* monoChar2 */ 0, 0, 0 },
    { 0, /* monoChar3 */ 0, 0, 0 },
    { 0, /* monoChar4 */ 0, 0, 0 },
    { 0, /* monoChar5 */ 0, 0, 0 },
    { 0, /* monoChar6 */ 0, 0, 0 },
    { 0, /* monoChar7 */ 0, 0, 0 },
    { 0, /* monoChar8 */ 0, 0, 0 },
    { 0, /* monoChar9 */ 0, 0, 0 },
    { 0, /* monoChar10 */ 0, 0, 0 },
    { 0, /* monoChar11 */ 0, 0, 0 },
    { 0, /* monoChar12 */ 0, 0, 0 },
    { 0, /* monoChar13 */ 0, 0, 0 },
    { 0, /* monoChar14 */ 0, 0, 0 },
    { 0, /* monoChar15 */ 0, 0, 0 },
    { 0, /* monoChar16 */ 0, 0, 0 },
    { 0, /* monoChar17 */ 0, 0, 0 },
    { 0, /* monoChar18 */ 0, 0, 0 },
    { 0, /* monoChar19 */ 0, 0, 0 },
    { 0, /* monoChar20 */ 0, 0, 0 },
    { 0, /* monoChar21 */ 0, 0, 0 },
    { 0, /* monoChar22 */ 0, 0, 0 },
    { 0, /* monoChar23 */ 0, 0, 0 },
    { 0, /* monoChar24 */ 0, 0, 0 },
    { 0, /* monoChar25 */ 0, 0, 0 },
    { 0, /* monoChar26 */ 0, 0, 0 },
    { 0, /* monoChar27 */ 0, 0, 0 },
    { 0, /* monoChar28 */ 0, 0, 0 },
    { 0, /* monoChar29 */ 0, 0, 0 },
    { 0, /* monoChar30 */ 0, 0, 0 },
    { 0, /* monoChar31 */ 0, 0, 0 },
    { 0, /* monoChar32 */ 0, 52.381, 104.762 },
    { 2, monoChar33, 52.381, 104.762 },
    { 2, monoChar34, 52.381, 104.762 },
    { 4, monoChar35, 52.381, 104.762 },
    { 3, monoChar36, 52.381, 104.762 },
    { 3, monoChar37, 52.381, 104.762 },
    { 1, monoChar38, 52.381, 104.762 },
    { 1, monoChar39, 52.381, 104.762 },
    { 1, monoChar40, 52.381, 104.762 },
    { 1, monoChar41, 52.381, 104.762 },
    { 3, monoChar42, 52.381, 104.762 },
    { 2, monoChar43, 52.381, 104.762 },
    { 1, monoChar44, 52.381, 104.762 },
    { 1, monoChar45, 52.381, 104.762 },
    { 1, monoChar46, 52.381, 104.762 },
    { 1, monoChar47, 52.381, 104.762 },
    { 1, monoChar48, 52.381, 104.762 },
    { 1, monoChar49, 52.381, 104.762 },
    { 1, monoChar50, 52.381, 104.762 },
    { 1, monoChar51, 52.381, 104.762 },
    { 2, monoChar52, 52.381, 104.762 },
    { 1, monoChar53, 52.381, 104.762 },
    { 1, monoChar54, 52.381, 104.762 },
    { 2, monoChar55, 52.381, 104.762 },
    { 1, monoChar56, 52.381, 104.762 },
    { 1, monoChar57, 52.381, 104.762 },
    { 2, monoChar58, 52.381, 104.762 },
    { 2, monoChar59, 52.381, 104.762 },
    { 1, monoChar60, 52.381, 104.762 },
    { 2, monoChar61, 52.381, 104.762 },
    { 1, monoChar62, 52.381, 104.762 },
    { 2, monoChar63, 52.381, 104.762 },
    { 2, monoChar64, 52.381, 104.762 },
    { 3, monoChar65, 52.381, 104.762 },
    { 3, monoChar66, 52.381, 104.762 },
    { 1, monoChar67, 52.381, 104.762 },
    { 2, monoChar68, 52.381, 104.762 },
    { 4, monoChar69, 52.381, 104.762 },
    { 3, monoChar70, 52.381, 104.762 },
    { 2, monoChar71, 52.381, 104.762 },
    { 3, monoChar72, 52.381, 104.762 },
    { 1, monoChar73, 52.381, 104.762 },
    { 1, monoChar74, 52.381, 104.762 },
    { 3, monoChar75, 52.381, 104.762 },
    { 2, monoChar76, 52.381, 104.762 },
    { 4, monoChar77, 52.381, 104.762 },
    { 3, monoChar78, 52.381, 104.762 },
    { 1, monoChar79, 52.381, 104.762 },
    { 2, monoChar80, 52.381, 104.762 },
    { 2, monoChar81, 52.381, 104.762 },
    { 3, monoChar82, 52.381, 104.762 },
    { 1, monoChar83, 52.381, 104.762 },
    { 2, monoChar84, 52.381, 104.762 },
    { 1, monoChar85, 52.381, 104.762 },
    { 2, monoChar86, 52.381, 104.762 },
    { 4, monoChar87, 52.381, 104.762 },
    { 2, monoChar88, 52.381, 104.762 },
    { 2, monoChar89, 52.381, 104.762 },
    { 3, monoChar90, 52.381, 104.762 },
    { 4, monoChar91, 52.381, 104.762 },
    { 1, monoChar92, 52.381, 104.762 },
    { 4, monoChar93, 52.381, 104.762 },
    { 2, monoChar94, 52.381, 104.762 },
    { 1, monoChar95, 52.381, 104.762 },
    { 2, monoChar96, 52.381, 104.762 },
    { 2, monoChar97, 52.381, 104.762 },
    { 2, monoChar98, 52.381, 104.762 },
    { 1, monoChar99, 52.381, 104.762 },
    { 2, monoChar100, 52.381, 104.762 },
    { 1, monoChar101, 52.381, 104.762 },
    { 2, monoChar102, 52.381, 104.762 },
    { 2, monoChar103, 52.381, 104.762 },
    { 2, monoChar104, 52.381, 104.762 },
    { 2, monoChar105, 52.381, 104.762 },
    { 2, monoChar106, 52.381, 104.762 },
    { 3, monoChar107, 52.381, 104.762 },
    { 1, monoChar108, 52.381, 104.762 },
    { 3, monoChar109, 52.381, 104.762 },
    { 2, monoChar110, 52.381, 104.762 },
    { 1, monoChar111, 52.381, 104.762 },
    { 2, monoChar112, 52.381, 104.762 },
    { 2, monoChar113, 52.381, 104.762 },
    { 2, monoChar114, 52.381, 104.762 },
    { 1, monoChar115, 52.381, 104.762 },
    { 2, monoChar116, 52.381, 104.762 },
    { 2, monoChar117, 52.381, 104.762 },
    { 2, monoChar118, 52.381, 104.762 },
    { 4, monoChar119, 52.381, 104.762 },
    { 2, monoChar120, 52.381, 104.762 },
    { 2, monoChar121, 52.381, 104.762 },
    { 3, monoChar122, 52.381, 104.762 },
    { 3, monoChar123, 52.381, 104.762 },
    { 1, monoChar124, 52.381, 104.762 },
    { 3, monoChar125, 52.381, 104.762 },
    { 2, monoChar126, 52.381, 104.762 },
    { 2, monoChar127, 52.381, 104.762 },
};

static
StrokeFontRec glutStrokeMonoRoman = { "Roman", 128, monoChars, 119.048, -33.3333 };

