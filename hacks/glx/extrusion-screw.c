/* 
 * screw.c
 * 
 * FUNCTION:
 * Draws a screw shape.
 *
 * HISTORY:
 * -- created by Linas Vepstas October 1991
 * -- heavily modified to draw more texas shapes, Feb 1993, Linas
 * -- converted to use GLUT -- December 1995, Linas
 *
 */

/* required include files */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glut.h>
#ifdef HAVE_GLE3
#include <GL/gle.h>
#else
#include <GL/tube.h>
#endif

#ifndef NULL
#define NULL ((void *) 0x0)
#endif /* NULL */

/* =========================================================== */

#define SCALE 1.3
#define CONTOUR(x,y) {					\
   double ax, ay, alen;					\
   contour[i][0] = SCALE * (x);				\
   contour[i][1] = SCALE * (y);				\
   if (i!=0) {						\
      ax = contour[i][0] - contour[i-1][0];		\
      ay = contour[i][1] - contour[i-1][1];		\
      alen = 1.0 / sqrt (ax*ax + ay*ay);		\
      ax *= alen;   ay *= alen;				\
      norms [i-1][0] = ay;				\
      norms [i-1][1] = -ax;				\
   }							\
   i++;							\
}

#define NUM_PTS (25)

static double contour [NUM_PTS][2];
static double norms [NUM_PTS][2];

static void init_contour (void)
{
   int i;

   /* outline of extrusion */
   i=0;
   CONTOUR (1.0, 1.0);
   CONTOUR (1.0, 2.9);
   CONTOUR (0.9, 3.0);
   CONTOUR (-0.9, 3.0);
   CONTOUR (-1.0, 2.9);

   CONTOUR (-1.0, 1.0);
   CONTOUR (-2.9, 1.0);
   CONTOUR (-3.0, 0.9);
   CONTOUR (-3.0, -0.9);
   CONTOUR (-2.9, -1.0);

   CONTOUR (-1.0, -1.0);
   CONTOUR (-1.0, -2.9);
   CONTOUR (-0.9, -3.0);
   CONTOUR (0.9, -3.0);
   CONTOUR (1.0, -2.9);

   CONTOUR (1.0, -1.0);
   CONTOUR (2.9, -1.0);
   CONTOUR (3.0, -0.9);
   CONTOUR (3.0, 0.9);
   CONTOUR (2.9, 1.0);

   CONTOUR (1.0, 1.0);   /* repeat so that last normal is computed */
}
   
/* =========================================================== */

extern float lastx;
extern float lasty;
extern float rot_x;
extern float rot_y;
extern float rot_z;

void InitStuff_screw (void)
{
   int style;

   /* configure the pipeline */
   style = TUBE_JN_CAP;
   style |= TUBE_CONTOUR_CLOSED;
   style |= TUBE_NORM_FACET;
   style |= TUBE_JN_ANGLE;
   gleSetJoinStyle (style);

   init_contour();
}

/* =========================================================== */

void DrawStuff_screw (void) {

   glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glColor3f (0.5, 0.6, 0.6);

   /* set up some matrices so that the object spins with the mouse */
   glPushMatrix ();
   glTranslatef (0.0, 0.0, -80.0);
   glRotatef(rot_x, 1, 0, 0);
   glRotatef(rot_y, 0, 1, 0);
   glRotatef(rot_z, 0, 0, 1);
/*     glRotatef (130.0, 0.0, 1.0, 0.0); */
/*     glRotatef (65.0, 1.0, 0.0, 0.0); */

   /* draw the brand and the handle */
   gleScrew (20, contour, norms, 
                 NULL, -6.0, 9.0, lasty);

   glPopMatrix ();
}

/* ===================== END OF FILE ================== */
