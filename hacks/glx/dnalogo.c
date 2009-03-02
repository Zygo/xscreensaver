/* DNA Logo, Copyright (c) 2001, 2002, 2003 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	__extension__ \
			"*delay:	    25000   \n" \
			"*showFPS:	    False   \n" \
			"*wireframe:	    False   \n" \
			"*doGasket:	    True    \n" \
			"*doHelix:	    True    \n" \
			"*doLadder:	    True    \n" \
			"*wallFacets:	    360	    \n" \
			"*tubeFacets:	    90	    \n" \
			"*clockwise:	    False   \n" \
			"*turns:	    0.8	    \n" \
			"*turnSpacing:	    2.40    \n" \
			"*barSpacing:	    0.30    \n" \
			"*wallHeight:	    0.4	    \n" \
			"*wallThickness:    0.1	    \n" \
			"*tubeThickness:    0.075   \n" \
			"*wallTaper:	    1.047   \n" \
			"*gasketSize:	    2.15    \n" \
			"*gasketDepth:	    0.15    \n" \
			"*gasketThickness:  0.4	    \n" \
			"*speed:	    1.0	    \n" \
			".foreground:	    #00AA00 \n" \

# define refresh_logo 0
# define release_logo 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "normals.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"

#ifdef USE_GL /* whole file */

typedef struct {
  Bool spinning_p;
  GLfloat position;     /* 0.0 - 1.0 */
  GLfloat speed;        /* how far along the path (may be negative) */
  GLfloat probability;  /* relative likelyhood to start spinning */
} spinner;

typedef struct {
  GLXContext *glx_context;

  GLuint helix_list,  helix_list_wire,  helix_list_facetted;
  GLuint gasket_list, gasket_list_wire;

  int wall_facets;
  int tube_facets;
  Bool clockwise;
  GLfloat color[4];

  GLfloat turns;
  GLfloat turn_spacing;
  GLfloat bar_spacing;
  GLfloat wall_height;
  GLfloat wall_thickness;
  GLfloat tube_thickness;
  GLfloat wall_taper;

  GLfloat gasket_size;
  GLfloat gasket_depth;
  GLfloat gasket_thickness;

  GLfloat speed;

  spinner gasket_spinnerx, gasket_spinnery, gasket_spinnerz;
  spinner scene_spinnerx,  scene_spinnery;
  spinner helix_spinnerz;

  trackball_state *trackball;
  Bool button_down_p;

  int wire_overlay;  /* frame countdown */

} logo_configuration;

static logo_configuration *dcs = NULL;

static XrmOptionDescRec opts[] = {
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
};

ENTRYPOINT ModeSpecOpt logo_opts = {countof(opts), opts, 0, NULL, NULL};

#define PROBABILITY_SCALE 600



/* Calculate the angle (in degrees) between two vectors.
 */
static GLfloat
vector_angle (double ax, double ay, double az,
              double bx, double by, double bz)
{
  double La = sqrt (ax*ax + ay*ay + az*az);
  double Lb = sqrt (bx*bx + by*by + bz*bz);
  double cc, angle;

  if (La == 0 || Lb == 0) return 0;
  if (ax == bx && ay == by && az == bz) return 0;

  /* dot product of two vectors is defined as:
       La * Lb * cos(angle between vectors)
     and is also defined as:
       ax*bx + ay*by + az*bz
     so:
      La * Lb * cos(angle) = ax*bx + ay*by + az*bz
      cos(angle)  = (ax*bx + ay*by + az*bz) / (La * Lb)
      angle = acos ((ax*bx + ay*by + az*bz) / (La * Lb));
  */
  cc = (ax*bx + ay*by + az*bz) / (La * Lb);
  if (cc > 1) cc = 1;  /* avoid fp rounding error (1.000001 => sqrt error) */
  angle = acos (cc);

  return (angle * M_PI / 180);
}


/* Make the helix
 */

static void
make_helix (logo_configuration *dc, int facetted, int wire)
{
  int wall_facets = dc->wall_facets / (facetted ? 15 : 1);
  GLfloat th;
  GLfloat max_th = M_PI * 2 * dc->turns;
  GLfloat th_inc = M_PI * 2 / wall_facets;

  GLfloat x1=0,  y1=0,  x2=0,  y2=0;
  GLfloat x1b=0, y1b=0, x2b=0, y2b=0;
  GLfloat z1=0, z2=0;
  GLfloat h1=0, h2=0;
  GLfloat h1off=0, h2off=0;
  GLfloat z_inc = dc->turn_spacing / wall_facets;

  th  = 0;
  x1  = 1;
  y1  = 0;
  x1b = 1 - dc->wall_thickness;
  y1b = 0;

  z1 = -(dc->turn_spacing * dc->turns / 2);

  h1 = (dc->wall_taper > 0 ? 0 : dc->wall_height / 2);
  h1off = (dc->wall_taper > 0 ? -dc->wall_height / 2 : 0);

  if (!dc->clockwise)
    z1 = -z1, z_inc = -z_inc, h1off = -h1off;

  /* Leading end-cap
   */
  if (!wire && h1 > 0)
    {
      GLfloat nx, ny;
      glFrontFace(GL_CCW);
      glBegin(GL_QUADS);
      nx = cos (th + M_PI/2);
      ny = sin (th + M_PI/2);
      glNormal3f(nx, ny, 0);
      glVertex3f( x1, y1,  z1 - h1 + h1off);
      glVertex3f( x1, y1,  z1 + h1 + h1off);
      glVertex3f(x1b, y1b, z1 + h1 + h1off);
      glVertex3f(x1b, y1b, z1 - h1 + h1off);
      glEnd();
    }

  while (th + th_inc <= max_th)
    {
      th += th_inc;

      x2 = cos (th);
      y2 = sin (th);
      z2 = z1 + z_inc;
      x2b = x2 * (1 - dc->wall_thickness);
      y2b = y2 * (1 - dc->wall_thickness);

      h2 = h1;
      h2off = h1off;

      if (dc->wall_taper > 0)
        {
          h2off = 0;
          if (th < dc->wall_taper)
            {
              h2 = dc->wall_height/2 * cos (M_PI / 2
                                            * (1 - (th / dc->wall_taper)));
              if (dc->clockwise)
                h2off = h2 - dc->wall_height/2;
              else
                h2off = dc->wall_height/2 - h2;
            }
          else if (th >= max_th - dc->wall_taper)
            {
              if (th + th_inc > max_th) /* edge case: always come to a point */
                h2 = 0;
              else
                h2 = dc->wall_height/2 * cos (M_PI / 2
                                              * (1 - ((max_th - th)
                                                      / dc->wall_taper)));
              if (dc->clockwise)
                h2off = dc->wall_height/2 - h2;
              else
                h2off = h2 - dc->wall_height/2;
            }
        }

      /* outer face
       */
      glFrontFace(GL_CW);
      glBegin(wire ? GL_LINES : GL_QUADS);
      glNormal3f(x1, y1, 0);
      glVertex3f(x1, y1, z1 - h1 + h1off);
      glVertex3f(x1, y1, z1 + h1 + h1off);
      glNormal3f(x2, y2, 0);
      glVertex3f(x2, y2, z2 + h2 + h2off);
      glVertex3f(x2, y2, z2 - h2 + h2off);
      glEnd();

      /* inner face
       */
      glFrontFace(GL_CCW);
      glBegin(wire ? GL_LINES : GL_QUADS);
      glNormal3f(-x1b, -y1b, 0);
      glVertex3f( x1b,  y1b, z1 - h1 + h1off);
      glVertex3f( x1b,  y1b, z1 + h1 + h1off);
      glNormal3f(-x2b, -y2b, 0);
      glVertex3f( x2b,  y2b, z2 + h2 + h2off);
      glVertex3f( x2b,  y2b, z2 - h2 + h2off);
      glEnd();

      /* top face
       */
      glFrontFace(GL_CCW);
      /* glNormal3f(0, 0, 1);*/
      do_normal (x2,   y2,  z2 + h2 + h2off,
                 x2b,  y2b, z2 + h2 + h2off,
                 x1b,  y1b, z1 + h1 + h1off);
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f( x2,   y2,  z2 + h2 + h2off);
      glVertex3f( x2b,  y2b, z2 + h2 + h2off);
      glVertex3f( x1b,  y1b, z1 + h1 + h1off);
      glVertex3f( x1,   y1,  z1 + h1 + h1off);

      glEnd();

      /* bottom face
       */
      glFrontFace(GL_CCW);
      do_normal ( x1,   y1,  z1 - h1 + h1off,
                  x1b,  y1b, z1 - h1 + h1off,
                  x2b,  y2b, z2 - h2 + h2off);
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(0, 0, -1);
      glVertex3f( x1,   y1,  z1 - h1 + h1off);
      glVertex3f( x1b,  y1b, z1 - h1 + h1off);
      glVertex3f( x2b,  y2b, z2 - h2 + h2off);
      glVertex3f( x2,   y2,  z2 - h2 + h2off);
      glEnd();

      x1 = x2;
      y1 = y2;
      x1b = x2b;
      y1b = y2b;
      z1 += z_inc;
      h1 = h2;
      h1off = h2off;
    }

  /* Trailing end-cap
   */
  if (!wire && h2 > 0)
    {
      GLfloat nx, ny;
      glFrontFace(GL_CW);
      glBegin(GL_QUADS);
      nx = cos (th + M_PI/2);
      ny = sin (th + M_PI/2);
      glNormal3f(nx, ny, 0);
      glVertex3f(x2,  y2,  z1 - h2 + h2off);
      glVertex3f(x2,  y2,  z1 + h2 + h2off);
      glVertex3f(x2b, y2b, z1 + h2 + h2off);
      glVertex3f(x2b, y2b, z1 - h2 + h2off);
      glEnd();
    }
}


static void
make_ladder (logo_configuration *dc, int facetted, int wire)
{
  GLfloat th;
  GLfloat max_th = dc->turns * M_PI * 2;
  GLfloat max_z  = dc->turns * dc->turn_spacing;
  GLfloat z_inc  = dc->bar_spacing;
  GLfloat th_inc = M_PI * 2 * (dc->bar_spacing / dc->turn_spacing);
  GLfloat x, y, z;

  /* skip forward to center the bars in the helix... */
  int i;
  GLfloat usable_th = max_th - dc->wall_taper;
  GLfloat usable_z  = max_z / (max_th / usable_th);
  int nbars = usable_z / dc->bar_spacing;
  GLfloat used_z = (nbars - 1) * dc->bar_spacing;
  GLfloat pad_z = max_z - used_z;
  GLfloat pad_ratio = pad_z / max_z;

  th = (max_th * pad_ratio/2);
  z  = -(max_z / 2) + (max_z * pad_ratio/2);

  if (!dc->clockwise)
    z = -z, z_inc = -z_inc;

  for (i = 0; i < nbars; i++)
    {
      int facets = dc->tube_facets / (facetted ? 14 : 1);
      if (facets <= 3) facets = 3;
      x = cos (th) * (1 - dc->wall_thickness);
      y = sin (th) * (1 - dc->wall_thickness);
      tube ( x,  y, z,
            -x, -y, z,
             dc->tube_thickness, 0, facets,
             True, True, wire);
      z  += z_inc;
      th += th_inc;
    }
}



/* Make the gasket
 */


static void
make_gasket (logo_configuration *dc, int wire)
{
  int i;
  int points_size;
  int npoints = 0;
  int nctrls = 0;
  int res = 360/8;
  GLfloat d2r = M_PI / 180;

  GLfloat thick2 = (dc->gasket_thickness / dc->gasket_size) / 2;

  GLfloat *pointsx0, *pointsy0, *pointsx1, *pointsy1, *normals;

  GLfloat r0  = 0.780;  /* 395 */
  GLfloat r1a = 0.855;                /* top of wall below upper left hole */
  GLfloat r1b = 0.890;                /* center of upper left hole */
  GLfloat r1c = 0.922;                /* bottom of wall above hole */
  GLfloat r1  = 0.928;  /* 471 */
  GLfloat r2  = 0.966;  /* 490 */
  GLfloat r3  = 0.984;  /* 499 */
  GLfloat r4  = 1.000;  /* 507 */
  GLfloat r5  = 1.090;  /* 553 */

  GLfloat ctrl_r[100], ctrl_th[100];

  glPushMatrix();

# define POINT(r,th) \
    ctrl_r [nctrls] = r, \
    ctrl_th[nctrls] = (th * d2r), \
    nctrls++

  POINT (0.829, 0);      /* top indentation, right half */
  POINT (0.831, 0.85);
  POINT (0.835, 1.81);
  POINT (0.841, 2.65);
  POINT (0.851, 3.30);
  POINT (0.862, 3.81);
  POINT (0.872, 3.95);

  POINT (r4,    4.0);   /* moving clockwise... */
  POINT (r4,   48.2);
  POINT (r1,   48.2);
  POINT (r1,   54.2);
  POINT (r2,   55.8);
  POINT (r2,   73.2);
  POINT (r1,   74.8);
  POINT (r1,  101.2);
  POINT (r3,  103.4);
  POINT (r3,  132.0);
  POINT (r1,  133.4);

  POINT (r1,  180.7);
  POINT (r2,  183.6);
  POINT (r2,  209.8);
  POINT (r1,  211.0);
  POINT (r1,  221.8);
  POINT (r5,  221.8);
  POINT (r5,  223.2);
  POINT (r4,  223.2);

  POINT (r4,  316.8);      /* upper left indentation */
  POINT (0.990, 326.87);
  POINT (0.880, 327.21);
  POINT (0.872, 327.45);
  POINT (0.869, 327.80);
  POINT (0.867, 328.10);

  POINT (0.867, 328.85);
  POINT (0.869, 329.15);
  POINT (0.872, 329.50);
  POINT (0.880, 329.74);
  POINT (0.990, 330.08);

  POINT (r4,  339.0);
  if (! wire)
    {
      POINT (r1a, 339.0);      /* cut-out disc */
      POINT (r1a, 343.0);
    }
  POINT (r4,  343.0);
  POINT (r4,  356.0);

  POINT (0.872, 356.05);   /* top indentation, left half */
  POINT (0.862, 356.19);
  POINT (0.851, 356.70);
  POINT (0.841, 357.35);
  POINT (0.835, 358.19);
  POINT (0.831, 359.15);
  POINT (0.829, 360);
# undef POINT

  points_size = res + (nctrls * 2);
  pointsx0 = (GLfloat *) malloc (points_size * sizeof(GLfloat));
  pointsy0 = (GLfloat *) malloc (points_size * sizeof(GLfloat));
  pointsx1 = (GLfloat *) malloc (points_size * sizeof(GLfloat));
  pointsy1 = (GLfloat *) malloc (points_size * sizeof(GLfloat));
  normals  = (GLfloat *) malloc (points_size * sizeof(GLfloat) * 2);

  npoints = 0;
  for (i = 1; i < nctrls; i++)
    {
      GLfloat from_r  = ctrl_r [i-1];
      GLfloat from_th = ctrl_th[i-1];
      GLfloat to_r    = ctrl_r [i];
      GLfloat to_th   = ctrl_th[i];

      GLfloat step = 2*M_PI / res;
      int nsteps = 1 + ((to_th - from_th) / step);
      int j;

      for (j = 0; j < nsteps + (i == nctrls-1); j++)
        {
          GLfloat r  = from_r  + (j * (to_r  - from_r)  / nsteps);
          GLfloat th = from_th + (j * (to_th - from_th) / nsteps);

          GLfloat cth = cos(th) * dc->gasket_size;
          GLfloat sth = sin(th) * dc->gasket_size;

          pointsx0[npoints] = r0 * cth;  /* inner ring */
          pointsy0[npoints] = r0 * sth;
          pointsx1[npoints] = r  * cth;  /* outer ring */
          pointsy1[npoints] = r  * sth;
          npoints++;

          if (npoints >= points_size) abort();
        }
    }

  /* normals for the outer ring */
  for (i = 1; i < npoints; i++)
    {
      XYZ a, b, c, n;
      a.x = pointsx1[i-1];
      a.y = pointsy1[i-1];
      a.z = 0;
      b.x = pointsx1[i];
      b.y = pointsy1[i];
      b.z = 0;
      c = b;
      c.z = 1;
      n = calc_normal (a, b, c);
      normals[(i-1)*2  ] = n.x;
      normals[(i-1)*2+1] = n.y;
    }

  glRotatef(-90, 0, 1, 0);
  glRotatef(180, 0, 0, 1);

  if (wire)
    {
      GLfloat z;
      for (z = -thick2; z <= thick2; z += thick2*2)
        {
# if 1
          /* inside edge */
          glBegin (GL_LINE_LOOP);
          for (i = 0; i < npoints; i++)
            glVertex3f (pointsx0[i], pointsy0[i], z);
          glEnd();

          /* outside edge */
          glBegin (GL_LINE_LOOP);
          for (i = 0; i < npoints; i++)
            glVertex3f (pointsx1[i], pointsy1[i], z);
          glEnd();
# else
          for (i = 1; i < npoints; i++)
            {
              glBegin (GL_LINE_STRIP);
              glVertex3f (pointsx0[i-1], pointsy0[i-1], z);
              glVertex3f (pointsx0[i  ], pointsy0[i  ], z);
              glVertex3f (pointsx1[i  ], pointsy1[i  ], z);
              glVertex3f (pointsx1[i-1], pointsy1[i-1], z);
              glEnd();
            }
# endif
        }
#if 1
      glBegin (GL_LINES);
      for (i = 0; i < npoints; i++)
        {
          /* inside rim */
          glVertex3f (pointsx0[i], pointsy0[i], -thick2);
          glVertex3f (pointsx0[i], pointsy0[i],  thick2);
          /* outside rim */
          glVertex3f (pointsx1[i], pointsy1[i], -thick2);
          glVertex3f (pointsx1[i], pointsy1[i],  thick2);
        }
      glEnd();
#endif
    }
  else
    {
      /* top */
      glFrontFace(GL_CW);
      glNormal3f(0, 0, -1);
      glBegin (GL_QUAD_STRIP);
      for (i = 0; i < npoints; i++)
        {
          glVertex3f (pointsx0[i], pointsy0[i], -thick2);
          glVertex3f (pointsx1[i], pointsy1[i], -thick2);
        }
      glEnd();

      /* bottom */
      glFrontFace(GL_CCW);
      glNormal3f(0, 0, 1);
      glBegin (GL_QUAD_STRIP);
      for (i = 0; i < npoints; i++)
        {
          glVertex3f (pointsx0[i], pointsy0[i], thick2);
          glVertex3f (pointsx1[i], pointsy1[i], thick2);
        }
      glEnd();

      /* inside edge */
      glFrontFace(GL_CW);
      glBegin (GL_QUAD_STRIP);
      for (i = 0; i < npoints; i++)
        {
          glNormal3f (-pointsx0[i], -pointsy0[i],  0);
          glVertex3f ( pointsx0[i],  pointsy0[i],  thick2);
          glVertex3f ( pointsx0[i],  pointsy0[i], -thick2);
        }
      glEnd();

      /* outside edge */
      glFrontFace(GL_CCW);
      glBegin (GL_QUADS);
      {
        for (i = 0; i < npoints-1; i++)
          {
            int ia = (i == 0 ? npoints-2 : i-1);
            int iz = (i == npoints-2 ? 0 : i+1);
            GLfloat  x = pointsx1[i];
            GLfloat  y = pointsy1[i];
            GLfloat xz = pointsx1[iz];
            GLfloat yz = pointsy1[iz];

            GLfloat nxa = normals[ia*2];   /* normal of [i-1 - i] face */
            GLfloat nya = normals[ia*2+1];
            GLfloat nx  = normals[i*2];    /* normal of [i - i+1] face */
            GLfloat ny  = normals[i*2+1];
            GLfloat nxz = normals[iz*2];    /* normal of [i+1 - i+2] face */
            GLfloat nyz = normals[iz*2+1];

            GLfloat anglea = vector_angle (nx, ny, 0, nxa, nya, 0);
            GLfloat anglez = vector_angle (nx, ny, 0, nxz, nyz, 0);
            GLfloat pointy = 0.005;

            if (anglea > pointy)
              {
                glNormal3f (nx, ny, 0);
                glVertex3f (x,  y,   thick2);
                glVertex3f (x,  y,  -thick2);
              }
            else
              {
                glNormal3f ((nxa + nx) / 2, (nya + ny) / 2, 0);
                glVertex3f (x,  y,   thick2);
                glVertex3f (x,  y,  -thick2);
              }

            if (anglez > pointy)
              {
                glNormal3f (nx, ny, 0);
                glVertex3f (xz, yz, -thick2);
                glVertex3f (xz, yz,  thick2);
              }
            else
              {
                glNormal3f ((nx + nxz) / 2, (ny + nyz) / 2, 0);
                glVertex3f (xz, yz, -thick2);
                glVertex3f (xz, yz,  thick2);
              }
          }
      }
      glEnd();
    }

  /* Fill in the upper left hole...
   */
  {
    GLfloat th;
    npoints = 0;

    th = 339.0 * d2r;
    pointsx0[npoints] = r1c * cos(th) * dc->gasket_size;
    pointsy0[npoints] = r1c * sin(th) * dc->gasket_size;
    npoints++;
    pointsx0[npoints] = r4 * cos(th) * dc->gasket_size;
    pointsy0[npoints] = r4 * sin(th) * dc->gasket_size;
    npoints++;

    th = 343.0 * d2r;
    pointsx0[npoints] = r1c * cos(th) * dc->gasket_size;
    pointsy0[npoints] = r1c * sin(th) * dc->gasket_size;
    npoints++;
    pointsx0[npoints] = r4 * cos(th) * dc->gasket_size;
    pointsy0[npoints] = r4 * sin(th) * dc->gasket_size;
    npoints++;

    if (! wire)
      {
        /* front wall */
        glNormal3f (0, 0, -1);
        glFrontFace(GL_CW);
        glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
        glVertex3f (pointsx0[0], pointsy0[0], -thick2);
        glVertex3f (pointsx0[1], pointsy0[1], -thick2);
        glVertex3f (pointsx0[3], pointsy0[3], -thick2);
        glVertex3f (pointsx0[2], pointsy0[2], -thick2);
        glEnd();

        /* back wall */
        glNormal3f (0, 0, 1);
        glFrontFace(GL_CCW);
        glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
        glVertex3f (pointsx0[0], pointsy0[0],  thick2);
        glVertex3f (pointsx0[1], pointsy0[1],  thick2);
        glVertex3f (pointsx0[3], pointsy0[3],  thick2);
        glVertex3f (pointsx0[2], pointsy0[2],  thick2);
        glEnd();
      }

    /* top wall */
    glFrontFace(GL_CW);
    glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f (pointsx0[1], pointsy0[1], 0);
    glVertex3f (pointsx0[1], pointsy0[1],  thick2);
    glNormal3f (pointsx0[3], pointsy0[3], 0);
    glVertex3f (pointsx0[3], pointsy0[3],  thick2);
    glVertex3f (pointsx0[3], pointsy0[3], -thick2);
    glNormal3f (pointsx0[1], pointsy0[1], 0);
    glVertex3f (pointsx0[1], pointsy0[1], -thick2);
    glEnd();


    /* Now make a donut.
     */
    {
      int nsteps = 12;
      GLfloat r0 = 0.026;
      GLfloat r1 = 0.060;
      GLfloat th, cth, sth;

      glPushMatrix ();

      th = ((339.0 + 343.0) / 2) * d2r;
      
      glTranslatef (r1b * cos(th) * dc->gasket_size,
                    r1b * sin(th) * dc->gasket_size,
                    0);

      npoints = 0;
      for (i = 0; i < nsteps; i++)
        {
          th = 2 * M_PI * i / nsteps;
          cth = cos (th) * dc->gasket_size;
          sth = sin (th) * dc->gasket_size;
          pointsx0[npoints] = r0 * cth;
          pointsy0[npoints] = r0 * sth;
          pointsx1[npoints] = r1 * cth;
          pointsy1[npoints] = r1 * sth;
          npoints++;
        }

      pointsx0[npoints] = pointsx0[0];
      pointsy0[npoints] = pointsy0[0];
      pointsx1[npoints] = pointsx1[0];
      pointsy1[npoints] = pointsy1[0];
      npoints++;

      if (wire)
        {
          glBegin (GL_LINE_LOOP);
          for (i = 0; i < npoints; i++)
            glVertex3f (pointsx0[i], pointsy0[i], -thick2);
          glEnd();
          glBegin (GL_LINE_LOOP);
          for (i = 0; i < npoints; i++)
            glVertex3f (pointsx0[i], pointsy0[i],  thick2);
          glEnd();
# if 0
          glBegin (GL_LINE_LOOP);
          for (i = 0; i < npoints; i++)
            glVertex3f (pointsx1[i], pointsy1[i], -thick2);
          glEnd();
          glBegin (GL_LINE_LOOP);
          for (i = 0; i < npoints; i++)
            glVertex3f (pointsx1[i], pointsy1[i],  thick2);
          glEnd();
# endif
        }
      else
        {
          /* top */
          glFrontFace(GL_CW);
          glNormal3f(0, 0, -1);
          glBegin (GL_QUAD_STRIP);
          for (i = 0; i < npoints; i++)
            {
              glVertex3f (pointsx0[i], pointsy0[i], -thick2);
              glVertex3f (pointsx1[i], pointsy1[i], -thick2);
            }
          glEnd();

          /* bottom */
          glFrontFace(GL_CCW);
          glNormal3f(0, 0, 1);
          glBegin (GL_QUAD_STRIP);
          for (i = 0; i < npoints; i++)
            {
              glVertex3f (pointsx0[i], pointsy0[i],  thick2);
              glVertex3f (pointsx1[i], pointsy1[i],  thick2);
            }
          glEnd();
        }

      /* inside edge */
      glFrontFace(GL_CW);
      glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
      for (i = 0; i < npoints; i++)
        {
          glNormal3f (-pointsx0[i], -pointsy0[i],  0);
          glVertex3f ( pointsx0[i],  pointsy0[i],  thick2);
          glVertex3f ( pointsx0[i],  pointsy0[i], -thick2);
        }
      glEnd();

      glPopMatrix();
    }
  }


  /* Attach the bottom-right dingus...
   */
  {
    GLfloat w = 0.04;
    GLfloat h = 0.17;
    GLfloat th;

    glRotatef (50, 0, 0, 1);
    glScalef (dc->gasket_size, dc->gasket_size, 1);
    glTranslatef (0, (r0+r1)/2, 0);

    /* buried box */
    if (! wire)
      {
        glFrontFace(GL_CCW);
        glBegin (wire ? GL_LINE_STRIP : GL_QUADS);
        glNormal3f (0, 0, -1);
        glVertex3f (-w/2, -h/2, -thick2); glVertex3f (-w/2,  h/2, -thick2);
        glVertex3f ( w/2,  h/2, -thick2); glVertex3f ( w/2, -h/2, -thick2);
        glNormal3f (1, 0, 0);
        glVertex3f ( w/2, -h/2, -thick2); glVertex3f ( w/2,  h/2, -thick2);
        glVertex3f ( w/2,  h/2,  thick2); glVertex3f ( w/2, -h/2,  thick2);
        glNormal3f (0, 0, 1);
        glVertex3f ( w/2, -h/2,  thick2); glVertex3f ( w/2,  h/2,  thick2);
        glVertex3f (-w/2,  h/2,  thick2); glVertex3f (-w/2, -h/2,  thick2);
        glNormal3f (-1, 0, 0);
        glVertex3f (-w/2, -h/2,  thick2); glVertex3f (-w/2,  h/2,  thick2);
        glVertex3f (-w/2,  h/2, -thick2); glVertex3f (-w/2, -h/2, -thick2);
        glEnd();
      }

    npoints = 0;
    for (th = 0; th < M_PI; th += (M_PI / 6))
      {
        pointsx0[npoints] = w/2 * cos(th);
        pointsy0[npoints] = w/2 * sin(th);
        npoints++;
      }

    /* front inside curve */
    glNormal3f (0, 0, -1);
    glFrontFace(GL_CW);
    glBegin (wire ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
    if (! wire) glVertex3f (0, h/2, -thick2);
    for (i = 0; i < npoints; i++)
      glVertex3f (pointsx0[i], h/2 + pointsy0[i], -thick2);
    glEnd();

    /* front outside curve */
    glFrontFace(GL_CCW);
    glBegin (wire ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
    if (! wire) glVertex3f (0, -h/2, -thick2);
    for (i = 0; i < npoints; i++)
      glVertex3f (pointsx0[i], -h/2 - pointsy0[i], -thick2);
    glEnd();

    /* back inside curve */
    glNormal3f (0, 0, 1);
    glFrontFace(GL_CCW);
    glBegin (wire ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
    if (! wire) glVertex3f (0, h/2, thick2);
    for (i = 0; i < npoints; i++)
      glVertex3f (pointsx0[i], h/2 + pointsy0[i], thick2);
    glEnd();

    /* back outside curve */
    glFrontFace(GL_CW);
    glBegin (wire ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
    if (! wire) glVertex3f (0, -h/2, thick2);
    for (i = 0; i < npoints; i++)
      glVertex3f (pointsx0[i], -h/2 - pointsy0[i], thick2);
    glEnd();

    /* inside curve */
    glFrontFace(GL_CCW);
    glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
    for (i = 0; i < npoints; i++)
      {
        glNormal3f (pointsx0[i], pointsy0[i], 0);
        glVertex3f (pointsx0[i], h/2 + pointsy0[i],  thick2);
        glVertex3f (pointsx0[i], h/2 + pointsy0[i], -thick2);
      }
    glEnd();

    /* outside curve */
    glFrontFace(GL_CW);
    glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
    for (i = 0; i < npoints; i++)
      {
        glNormal3f (pointsx0[i], -pointsy0[i], 0);
        glVertex3f (pointsx0[i], -h/2 - pointsy0[i],  thick2);
        glVertex3f (pointsx0[i], -h/2 - pointsy0[i], -thick2);
      }
    glEnd();
  }

  free (pointsx0);
  free (pointsy0);
  free (pointsx1);
  free (pointsy1);
  free (normals);

  glPopMatrix();
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_logo (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
gl_init (ModeInfo *mi)
{
/*  logo_configuration *dc = &dcs[MI_SCREEN(mi)]; */
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat position[]  = {0, 0, 0, 0};
  GLfloat direction[] = {3, -1, -3};

  position[0] = -direction[0];
  position[1] = -direction[1];
  position[2] = -direction[2];

  if (!wire)
    {
      glLightfv(GL_LIGHT0, GL_POSITION, position);
      glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, direction);
      glShadeModel(GL_SMOOTH);
      glEnable(GL_NORMALIZE);
      glEnable(GL_CULL_FACE);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
    }
}


ENTRYPOINT void 
init_logo (ModeInfo *mi)
{
  logo_configuration *dc;
  int do_gasket = get_boolean_resource(mi->dpy, "doGasket", "Boolean");
  int do_helix = get_boolean_resource(mi->dpy, "doHelix", "Boolean");
  int do_ladder = do_helix && get_boolean_resource(mi->dpy, "doLadder", "Boolean");

  if (!do_gasket && !do_helix)
    {
      fprintf (stderr, "%s: no helix or gasket?\n", progname);
      exit (1);
    }

  if (!dcs) {
    dcs = (logo_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (logo_configuration));
    if (!dcs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  dc = &dcs[MI_SCREEN(mi)];

  if ((dc->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    reshape_logo (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  dc->wall_facets    = get_integer_resource(mi->dpy, "wallFacets",  "Integer");
  dc->tube_facets    = get_integer_resource(mi->dpy, "tubeFacets",  "Integer");
  dc->clockwise      = get_boolean_resource(mi->dpy, "clockwise",   "Boolean");
  dc->turns          = get_float_resource(mi->dpy, "turns",         "Float");
  dc->turn_spacing   = get_float_resource(mi->dpy, "turnSpacing",   "Float");
  dc->bar_spacing    = get_float_resource(mi->dpy, "barSpacing",    "Float");
  dc->wall_height    = get_float_resource(mi->dpy, "wallHeight",    "Float");
  dc->wall_thickness = get_float_resource(mi->dpy, "wallThickness", "Float");
  dc->tube_thickness = get_float_resource(mi->dpy, "tubeThickness", "Float");
  dc->wall_taper     = get_float_resource(mi->dpy, "wallTaper",     "Float");

  dc->gasket_size      = get_float_resource(mi->dpy, "gasketSize",      "Float");
  dc->gasket_depth     = get_float_resource(mi->dpy, "gasketDepth",     "Float");
  dc->gasket_thickness = get_float_resource(mi->dpy, "gasketThickness", "Float");

  dc->speed          = get_float_resource(mi->dpy, "speed",         "Float");

  {
    XColor xcolor;

    char *color_name = get_string_resource (mi->dpy, "foreground", "Foreground");
    char *s2;
    for (s2 = color_name + strlen(color_name) - 1; s2 > color_name; s2--)
      if (*s2 == ' ' || *s2 == '\t')
        *s2 = 0;
      else
        break;

    if (! XParseColor (MI_DISPLAY(mi), mi->xgwa.colormap, color_name, &xcolor))
      {
        fprintf (stderr, "%s: can't parse color %s\n", progname, color_name);
        exit (1);
      }

    dc->color[0] = xcolor.red   / 65535.0;
    dc->color[1] = xcolor.green / 65535.0;
    dc->color[2] = xcolor.blue  / 65535.0;
    dc->color[3] = 1.0;
  }

  dc->trackball = gltrackball_init ();

  dc->gasket_spinnery.probability = 0.1;
  dc->gasket_spinnerx.probability = 0.1;
  dc->gasket_spinnerz.probability = 1.0;
  dc->helix_spinnerz.probability  = 0.6;
  dc->scene_spinnerx.probability  = 0.1;
  dc->scene_spinnery.probability  = 0.0;

  if (dc->speed > 0)    /* start off with the gasket in motion */
    {
      dc->gasket_spinnerz.spinning_p = True;
      dc->gasket_spinnerz.speed = (0.002
                                   * ((random() & 1) ? 1 : -1)
                                   * dc->speed);
    }

  glPushMatrix();
  dc->helix_list = glGenLists (1);
  glNewList (dc->helix_list, GL_COMPILE);
  glRotatef(126, 0, 0, 1);
  if (do_ladder) make_ladder (dc, 0, 0);
  if (do_helix)  make_helix  (dc, 0, 0);
  glRotatef(180, 0, 0, 1);
  if (do_helix)  make_helix  (dc, 0, 0);
  glEndList ();
  glPopMatrix();

  glPushMatrix();
  dc->helix_list_wire = glGenLists (1);
  glNewList (dc->helix_list_wire, GL_COMPILE);
  /* glRotatef(126, 0, 0, 1); wtf? */
  if (do_ladder) make_ladder (dc, 1, 1);
  if (do_helix)  make_helix  (dc, 1, 1);
  glRotatef(180, 0, 0, 1);
  if (do_helix)  make_helix  (dc, 1, 1);
  glEndList ();
  glPopMatrix();

  glPushMatrix();
  dc->helix_list_facetted = glGenLists (1);
  glNewList (dc->helix_list_facetted, GL_COMPILE);
  glRotatef(126, 0, 0, 1);
  if (do_ladder) make_ladder (dc, 1, 0);
  if (do_helix)  make_helix  (dc, 1, 0);
  glRotatef(180, 0, 0, 1);
  if (do_helix)  make_helix  (dc, 1, 0);
  glEndList ();
  glPopMatrix();

  dc->gasket_list = glGenLists (1);
  glNewList (dc->gasket_list, GL_COMPILE);
  if (do_gasket) make_gasket (dc, 0);
  glEndList ();

  dc->gasket_list_wire = glGenLists (1);
  glNewList (dc->gasket_list_wire, GL_COMPILE);
  if (do_gasket) make_gasket (dc, 1);
  glEndList ();

  /* When drawing both solid and wireframe objects,
     make sure the wireframe actually shows up! */
  glEnable (GL_POLYGON_OFFSET_FILL);
  glPolygonOffset (1.0, 1.0);
}


ENTRYPOINT Bool
logo_handle_event (ModeInfo *mi, XEvent *event)
{
  logo_configuration *dc = &dcs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      dc->button_down_p = True;
      gltrackball_start (dc->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      dc->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (dc->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           dc->button_down_p)
    {
      gltrackball_track (dc->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
tick_spinner (ModeInfo *mi, spinner *s)
{
  logo_configuration *dc = &dcs[MI_SCREEN(mi)];

  if (dc->speed == 0) return;
  if (dc->button_down_p) return;

  if (s->spinning_p)
    {
      s->position += s->speed;
      if (s->position >=  1.0 || s->position <= -1.0)
          
        {
          s->position = 0;
          s->spinning_p = False;
        }
    }
  else if (s->probability &&
           (random() % (int) (PROBABILITY_SCALE / s->probability)) == 0)
    {
      GLfloat ss = 0.004;
      s->spinning_p = True;
      s->position = 0;
      do {
        s->speed = dc->speed * (frand(ss/3) + frand(ss/3) + frand(ss/3));
      } while (s->speed <= 0);
      if (random() & 1)
        s->speed = -s->speed;
    }
}


static void
link_spinners (ModeInfo *mi, spinner *s0, spinner *s1)
{
  if (s0->spinning_p && !s1->spinning_p)
    {
      GLfloat op = s1->probability;
      s1->probability = PROBABILITY_SCALE;
      tick_spinner (mi, s1);
      s1->probability = op;
    }
}


ENTRYPOINT void
draw_logo (ModeInfo *mi)
{
  logo_configuration *dc = &dcs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat gcolor[4];
  GLfloat specular[]  = {0.8, 0.8, 0.8, 1.0};
  GLfloat shininess   = 50.0;

  if (!dc->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(dc->glx_context));

  if (dc->wire_overlay == 0 &&
      (random() % (int) (PROBABILITY_SCALE / 0.2)) == 0)
    dc->wire_overlay = ((random() % 200) +
                        (random() % 200) +
                        (random() % 200));
      
  tick_spinner (mi, &dc->gasket_spinnerx);
  tick_spinner (mi, &dc->gasket_spinnery);
  tick_spinner (mi, &dc->gasket_spinnerz);
  tick_spinner (mi, &dc->helix_spinnerz);
  tick_spinner (mi, &dc->scene_spinnerx);
  tick_spinner (mi, &dc->scene_spinnery);
  link_spinners (mi, &dc->scene_spinnerx, &dc->scene_spinnery);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  {
    glScalef(3.3, 3.3, 3.3);

    gltrackball_rotate (dc->trackball);

    glRotatef(90, 1, 0, 0);
    glRotatef(90, 0, 0, 1);

    glColor3f(dc->color[0], dc->color[1], dc->color[2]);

    glRotatef (360 * sin (M_PI/2 * dc->scene_spinnerx.position), 0, 1, 0);
    glRotatef (360 * sin (M_PI/2 * dc->scene_spinnery.position), 0, 0, 1);

    glPushMatrix();
    {
      glRotatef (360 * sin (M_PI/2 * dc->gasket_spinnerx.position), 0, 1, 0);
      glRotatef (360 * sin (M_PI/2 * dc->gasket_spinnery.position), 0, 0, 1);
      glRotatef (360 * sin (M_PI/2 * dc->gasket_spinnerz.position), 1, 0, 0);

      memcpy (gcolor, dc->color, sizeof (dc->color));
      if (dc->wire_overlay != 0)
        {
          gcolor[0]   = gcolor[1]   = gcolor[2]   = 0;
          specular[0] = specular[1] = specular[2] = 0;
          shininess = 0;
        }
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, gcolor);
      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  specular);
      glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shininess);

      if (wire)
        glCallList (dc->gasket_list_wire);
      else if (dc->wire_overlay != 0)
        {
          glCallList (dc->gasket_list);
          glDisable (GL_LIGHTING);
          glCallList (dc->gasket_list_wire);
          if (!wire) glEnable (GL_LIGHTING);
        }
      else
        glCallList (dc->gasket_list);
    }
    glPopMatrix();

    glRotatef (360 * sin (M_PI/2 * dc->helix_spinnerz.position), 0, 0, 1);

    if (wire)
      glCallList (dc->helix_list_wire);
    else if (dc->wire_overlay != 0)
      {
        glCallList (dc->helix_list_facetted);
        glDisable (GL_LIGHTING);
        glCallList (dc->helix_list_wire);
        if (!wire) glEnable (GL_LIGHTING);
      }
    else
      glCallList (dc->helix_list);
  }
  glPopMatrix();

  if (dc->wire_overlay > 0)
    dc->wire_overlay--;

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("DNAlogo", dnalogo, logo)

#endif /* USE_GL */
