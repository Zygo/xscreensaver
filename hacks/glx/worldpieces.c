/* worldpieces, Copyright (c) 2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef STANDALONE
# define release_planet 0
# include "xlockmore.h"			/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#define TITLE_FONT "sans-serif bold 12"

#if defined(HAVE_MOBILE)
# define LABEL_FONT "sans-serif bold 50"
#elif defined(HAVE_JWXYZ)
# define LABEL_FONT "sans-serif bold 30"
#else
# define LABEL_FONT "sans-serif bold 64"
#endif

#define DEFAULTS   "*delay:	20000   \n"	\
				"*titleFont:  " TITLE_FONT "\n" \
				"*labelFont:  " LABEL_FONT "\n" \
				"*showFPS:	False   \n" \
				"*wireframe:	False	\n"	\
				"*suppressRotationAnimation: True\n" \

#include "texfont.h"
#include "sphere.h"
#include "easing.h"
#include "xftwrap.h"
#include "quaternion.h"
#include "utf8wc.h"
#include "triangle.h"

#define DEF_ROTATE  "True"
#define DEF_ROLL    "True"
#define DEF_WANDER  "True"
#define DEF_SPIN    "1.0"
#define DEF_TEXTURE "True"
#define DEF_STARS   "True"
#define DEF_FASTFORWARD "False"
#define DEF_UNDERGROUND "False"
#define DEF_SPEED   "1.0"
#define DEF_IMAGE   "BUILTIN"

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static int do_rotate;
static int do_roll;
static int do_wander;
static int do_texture;
static int do_stars;
static int do_underground;
static int do_fastforward;
static char *which_image;
static GLfloat spin_arg;
static GLfloat speed_arg;

static XrmOptionDescRec opts[] = {
  {"-rotate",  ".rotate",          XrmoptionNoArg, "true" },
  {"+rotate",  ".rotate",          XrmoptionNoArg, "false" },
  {"-roll",    ".roll",            XrmoptionNoArg, "true" },
  {"+roll",    ".roll",            XrmoptionNoArg, "false" },
  {"-wander",  ".wander",          XrmoptionNoArg, "true" },
  {"+wander",  ".wander",          XrmoptionNoArg, "false" },
  {"-texture", ".texture",         XrmoptionNoArg, "true" },
  {"+texture", ".texture",         XrmoptionNoArg, "false" },
  {"-stars",   ".stars",           XrmoptionNoArg, "true" },
  {"+stars",   ".stars",           XrmoptionNoArg, "false" },
  {"-underground", ".underground", XrmoptionNoArg, "true" },
  {"+underground", ".underground", XrmoptionNoArg, "false" },
  {"-fastforward", ".fastforward", XrmoptionNoArg, "true" },
  {"+fastforward", ".fastforward", XrmoptionNoArg, "false" },
  {"-ffwd",        ".fastforward", XrmoptionNoArg, "true" },
  {"+ffwd",        ".fastforward", XrmoptionNoArg, "false" },
  {"-spin",    ".spin",            XrmoptionSepArg, 0 },
  {"-no-spin", ".spin",            XrmoptionNoArg, "0" },
  {"-speed",   ".speed",           XrmoptionSepArg, 0 },
  {"-image",   ".image",           XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_rotate,      "rotate",      "Rotate",      DEF_ROTATE,      t_Bool},
  {&do_roll,        "roll",        "Roll",        DEF_ROLL,        t_Bool},
  {&do_wander,      "wander",      "Wander",      DEF_WANDER,      t_Bool},
  {&do_texture,     "texture",     "Texture",     DEF_TEXTURE,     t_Bool},
  {&do_stars,       "stars",       "Stars",       DEF_STARS,       t_Bool},
  {&do_underground, "underground", "Underground", DEF_UNDERGROUND, t_Bool},
  {&do_fastforward, "fastforward", "Fastforward", DEF_FASTFORWARD, t_Bool},
  {&spin_arg,       "spin",        "Spin",        DEF_SPIN,        t_Float},
  {&speed_arg,      "speed",       "Speed",       DEF_SPEED,       t_Float},
  {&which_image,    "image",       "Image",       DEF_IMAGE,       t_String},
};

ENTRYPOINT ModeSpecOpt planet_opts = {
  countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   planet_description =
{"planet", "init_planet", "draw_planet", NULL,
 "draw_planet", "init_planet", "free_planet", &planet_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "", 0, NULL};
#endif

#include "blurb.h"
#include "ximage-loader.h"
#include "rotator.h"
#include "gltrackball.h"
#include "gllist.h"
#include "countries.h"
#include "earth.h"
#include "images/gen/ground_png.h"

extern const struct gllist *timezones;

typedef struct { GLfloat x, y, z; } XYZ;
typedef struct { GLfloat a, o; } LL;	/* latitude + longitude */

typedef struct {
  const country_geom *geom;
  GLuint mesh_list, edge_list;
  int mesh_polygon_count, edge_polygon_count;
  LL center2;
  XYZ center3;
  GLfloat size;
  GLfloat area;
  enum { GROUND, RAISED, PLACED, DROPPED } state;
  GLfloat drop_ratio, drop_step;
  XYZ drop_spin;
} country_dlist;

typedef enum {
  INIT,
  FASTFORWARD,
  RAISE_COUNTRY,
  DESC_ON,
  POSITION_GLOBE,
  PLACE_COUNTRY,
  DESC_OFF,
  PULSE_COUNTRIES,
  DROP_COUNTRIES,
} anim_state;


typedef struct {
  GLuint platelist;
  int plate_polygon_count;
  GLuint latlonglist;
  GLuint tzlist;
  GLuint starlist;
  int starcount;
  int tzpoints;
  int screen_width, screen_height;
  GLXContext *glx_context;
  Window window;
  LL orient, last_roll, last_track, pos_start, pos_end;
  GLfloat tilt;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  GLuint tex1, tex2;
  int draw_axis;
  int draw_compass;
  Bool timezones_p;
  country_dlist *countries;
  texture_font_data *title_font, *label_font;
  int *country_order;
  int country_idx;
  int fastforward_count;
  char *label;
  XCharStruct label_geom;
  GLfloat tick;
  anim_state state;
} planetstruct;


static planetstruct *planets = NULL;


static void
setup_xpm_texture (ModeInfo *mi, const unsigned char *data, unsigned long size)
{
  XImage *image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                        data, size);
  char buf[1024];
  clear_gl_error();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
# if !defined(HAVE_IPHONE) && !defined(HAVE_ANDROID)
  glPixelStorei (GL_UNPACK_ROW_LENGTH, image->width);
# endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               image->width, image->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "builtin texture (%dx%d)", image->width, image->height);
  check_gl_error(buf);
  XDestroyImage (image);
}


static Bool
setup_file_texture (ModeInfo *mi, char *filename)
{
  Display *dpy = mi->dpy;
  Visual *visual = mi->xgwa.visual;
  char buf[1024];

  XImage *image = file_to_ximage (dpy, visual, filename);
  if (!image) return False;

  clear_gl_error();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
# if !defined(HAVE_IPHONE) && !defined(HAVE_ANDROID)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
# endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               image->width, image->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "texture: %.100s (%dx%d)",
           filename, image->width, image->height);
  check_gl_error(buf);
  return True;
}


static void
setup_texture (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  glGenTextures (1, &gp->tex1);
  glBindTexture (GL_TEXTURE_2D, gp->tex1);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (!which_image ||
      !*which_image ||
      !strcmp(which_image, "BUILTIN"))
    {
    BUILTIN1:
      setup_xpm_texture (mi, earth_png, earth_png_size);
    }
  else
    {
      if (! setup_file_texture (mi, which_image))
        goto BUILTIN1;
    }

  check_gl_error("texture 1 initialization");

  /* Need to flip the texture top for bottom for some reason. */
  glMatrixMode (GL_TEXTURE);
  glScalef (1, -1, 1);
  glMatrixMode (GL_MODELVIEW);

  glGenTextures (1, &gp->tex2);
  glBindTexture (GL_TEXTURE_2D, gp->tex2);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  setup_xpm_texture (mi, ground_png, sizeof(ground_png));
}


static void
init_stars (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i, j;
  int width  = MI_WIDTH(mi);
  int height = MI_HEIGHT(mi);
  int size = (width > height ? width : height);
  int nstars = size * size / 80;
  int max_size = 3;
  GLfloat inc = 0.5;
  int steps = max_size / inc;
  GLfloat scale = 1;

  if (MI_WIDTH(mi) > 2560) {  /* Retina displays */
    scale *= 2;
    nstars /= 2;
  }

  gp->starlist = glGenLists(1);
  glNewList(gp->starlist, GL_COMPILE);
  for (j = 1; j <= steps; j++)
    {
      glPointSize(inc * j * scale);
      glBegin (GL_POINTS);
      for (i = 0; i < nstars / steps; i++)
        {
          GLfloat d = 0.1;
          GLfloat r = 0.15 + frand(0.3);
          GLfloat g = r + frand(d) - d;
          GLfloat b = r + frand(d) - d;

          GLfloat x = frand(1)-0.5;
          GLfloat y = frand(1)-0.5;
          GLfloat z = ((random() & 1)
                       ? frand(1)-0.5
                       : (BELLRAND(1)-0.5)/12);   /* milky way */
          d = sqrt (x*x + y*y + z*z);
          x /= d;
          y /= d;
          z /= d;
          glColor3f (r, g, b);
          glVertex3f (x, y, z);
          gp->starcount++;
        }
      glEnd ();
    }
  glEndList ();

  check_gl_error("stars initialization");
}


/* Project a point through a 4x4 matrix. */
static XYZ
transform_point (const XYZ in, double m[16])
{
  XYZ out;
  out.x = in.x * m[0] + in.y * m[4] + in.z * m[8]  + m[12];
  out.y = in.x * m[1] + in.y * m[5] + in.z * m[9]  + m[13];
  out.z = in.x * m[2] + in.y * m[6] + in.z * m[10] + m[14];
  return out;
}


static LL
latlon_normalize (const LL a)
{
  LL b = a;
  while (b.a >  M_PI*2) b.a =  M_PI*2 - b.a;	/* Stay between += 2pi. */
  while (b.a < -M_PI*2) b.a = -M_PI*2 - b.a;
  while (b.o >  M_PI)   b.o -= M_PI*2;		/* Stay between += pi. */
  while (b.o < -M_PI)   b.o += M_PI*2;
  return b;
}


/* Construct dlists of every country.
 */
static void
init_countries (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  int i;

  int mesh_size, mesh_out;
  int edge_size, edge_out;
  struct { XYZ p, n; LL t; } *mesh, *edge;

  mesh_size = edge_size = 100;
  mesh = (void *) calloc (mesh_size, sizeof(*mesh));
  edge = (void *) calloc (edge_size, sizeof(*edge));

# define REALLOC(A) do { \
    if (A##_size < A##_out + 3) { \
      A##_size += 100; \
      A = (void *) realloc (A, A##_size * sizeof(*A)); \
    }} while(0)

  gp->countries = (country_dlist *)
    calloc (all_countries.count, sizeof (*gp->countries));

  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_NORMAL_ARRAY);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);

  for (i = 0; i < all_countries.count; i++)
    {
      int j;
      country_dlist *c = &gp->countries[i];
      struct { LL min, max; GLfloat size; }
        bbox_cur, bbox_main = {{0, 0}, {0, 0}, 0};

      c->geom = &all_countries.geom[i];

      mesh_out = 0;
      edge_out = 0;

      for (j = 0; j < c->geom->count; j++)	/* MultiPolygon */
        {
          const country_polys *mp = &c->geom->polys[j];
          int k;
          int np = 0;
          struct triangulateio in, out;
          memset (&in,  0, sizeof(in));
          memset (&out, 0, sizeof(out));

          bbox_cur.min.o = bbox_cur.min.a =  999;
          bbox_cur.max.o = bbox_cur.max.a = -999;

          for (k = 0; k < mp->count; k++)
            {
              const country_path *p = &mp->paths[k];
              if (p->count & 1) abort();
              np += p->count;
            }

          if (!np) abort();
          in.numberofpoints   = 0;
          in.numberofsegments = 0;
          in.numberofholes    = 0;
          in.pointlist   = (REAL *) calloc (np, sizeof(*in.pointlist));
          in.segmentlist = (int  *) calloc (np, sizeof(*in.segmentlist));
          in.holelist    = (REAL *) calloc (np, sizeof(*in.holelist));

          for (k = 0; k < mp->count; k++)	/* Polygon */
            {
              int oedge_out = edge_out;
              const country_path *p = &mp->paths[k];
              int np2 = p->count / 2;
              int m;

              /* The coordinates in countries.h are lat +-90° and +-180° with
                 lon 0° at the Prime Meridian (Eastern edge of TZ GMT+0) and
                 lat 0° at the Equator. */

              /* Edges */

              for (m = 0; m < p->count; m += 2)
                {
                  LL ip;
                  XYZ op;
                  LL tx = { 0, 0 };
                  ip.o = -p->points[m]   * M_PI/180;
                  ip.a = -p->points[m+1] * M_PI/180;
                  op.x = cos (ip.a) * cos (ip.o);
                  op.y = cos (ip.a) * sin (ip.o);
                  op.z = sin (ip.a);

                  /* Since Antarctica's coordinates include 3 edges of a *box*
                     at the left, bottom and right of the equirectangular map,
                     we have to omit those points or else there's a weird line
                     going directly to the pole. */
                  if (!strcmp (c->geom->code, "AQ") &&
                      (ip.o >=  M_PI   * 0.999 ||
                       ip.o <= -M_PI   * 0.999 ||
                       ip.a >=  M_PI/2 * 0.999))
                    continue;

                  REALLOC(edge);
                  edge[edge_out].p = op;
                  edge[edge_out].n = op;	/* normal unused */
                  edge[edge_out].t = tx;	/* texture unused */
                  edge_out++;

                  if (m > 0)
                    {
                      edge[edge_out] = edge[edge_out-1];
                      edge_out++;
                    }

                  if (edge_out > edge_size) abort();
                  c->edge_polygon_count++;

                  if (ip.o < bbox_cur.min.o) bbox_cur.min.o = ip.o;
                  if (ip.a < bbox_cur.min.a) bbox_cur.min.a = ip.a;
                  if (ip.o > bbox_cur.max.o) bbox_cur.max.o = ip.o;
                  if (ip.a > bbox_cur.max.a) bbox_cur.max.a = ip.a;
                }

              REALLOC(edge);
              edge[edge_out++] = edge[oedge_out];


              /* Emit points, then edges that reference them. */

              for (m = 0; m < p->count; m++)
                in.pointlist[(in.numberofpoints * 2) + m] = p->points[m];

              for (m = 0; m < np2; m++)
                {
                  in.segmentlist[(in.numberofsegments + m) * 2] =
                    in.numberofpoints + m;
                  in.segmentlist[(in.numberofsegments + m) * 2 + 1] =
                    (m == np2-1
                     ? in.numberofpoints
                     : in.numberofpoints + m + 1);
                }
              in.numberofsegments += np2;
              in.numberofpoints   += np2;

              /* If a MultiPolygon contains more than one Polygon, it is
                 supposed to use a winding rule to determine which ones are
                 holes.

                 However, triangle.c doesn't work that way.  To indicate that
                 a closed path is to be a hole, we have to provide a point on
                 the inside of that path (and not on its edge).  So to find
                 that point, we just average every point on the path and hope
                 that the hole is close enough to convex that the point is
                 actually on the interior.

                 In the 110M data set, there is only one country with a hole,
                 South Africa, which entirely encloses Lesotho.  There exist
                 only two other enclosed countries (San Marino and Vatican
                 City) but those are too small to show up in the 110M data.
                 https://en.wikipedia.org/wiki/List_of_enclaves_and_exclaves

                 In the 50M data set, there are nine countries with holes,
                 most of those holes representing lakes.
               */
              if (k > 0 && !!strcmp (c->geom->code, "NULL"))
                {
                  LL h = { 0, 0 };
                  for (m = 0; m < p->count; m += 2)
                    {
                      h.o += p->points[m];
                      h.a += p->points[m+1];
                    }
                  h.o /= np2;
                  h.a /= np2;
                  in.holelist[in.numberofholes * 2]    = h.o;
                  in.holelist[in.numberofholes * 2 +1] = h.a;
                  in.numberofholes++;
                }
            }

          /* Compute the polar bounding box of this Polygon.
             If it is the largest in the MultiPolygon, save it. */
          {
            LL size;
            size.o = bbox_cur.max.o - bbox_cur.min.o;
            size.a = bbox_cur.max.a - bbox_cur.min.a;
            if (size.o > M_PI*2) size.o -= M_PI*2;
            if (size.a > M_PI*2) size.a -= M_PI*2;
            bbox_cur.size = (size.a > size.o ? size.a : size.o);

            if (j == 0 || bbox_cur.size > bbox_main.size)
              bbox_main = bbox_cur;
          }


# if 0	  /* Print a .poly file */
          fprintf (stderr, "\n# %s\n%d %d %d %d\n", c->geom->name,
                   in.numberofpoints, 2,
                   in.numberofpointattributes, 0);
          for (k = 0; k < in.numberofpoints; k++)
            fprintf (stderr, "%d %.20f %.20f\n", k,
                     in.pointlist[k*2], in.pointlist[k*2+1]);
          fprintf (stderr, "%d %d\n",
                   in.numberofsegments, 0);
          for (k = 0; k < in.numberofsegments; k++)
            fprintf (stderr, "%d %d %d\n", k,
                     in.segmentlist[k*2], in.segmentlist[k*2+1]);
          fprintf (stderr, "%d\n", in.numberofholes);
          for (k = 0; k < in.numberofholes; k++)
            fprintf (stderr, "%d %.20f %.20f\n", k,
                     in.holelist[k*2], in.holelist[k*2+1]);
          fprintf (stderr, "\n");
# endif

          triangulate ("p"	/* Input is planar straight line graph	 */
                       "q20" 	/* Quality Delaunay mesh, min degrees	 */
                       "a0.4"	/* Maximum constant area		 */
                       "j"	/* Omit duplicate points		 */
                       "S20"	/* Limit Steiner points or we might loop */
                       "B"	/* Don't compute boundary markers	 */
                       "z"	/* Start numbering from 0		 */
                       "Q",	/* Quiet				 */
                       &in, &out, NULL);

          for (k = 0; k < out.numberoftriangles; k++)
            {
              int omesh_out = mesh_out;
              int i0 = out.trianglelist[k * 3 + 0];
              int i1 = out.trianglelist[k * 3 + 1];
              int i2 = out.trianglelist[k * 3 + 2];
              LL  ip0, ip1, ip2;
              LL  tx0, tx1, tx2;
              XYZ op0, op1, op2;
              ip0.o = -out.pointlist[i0 * 2 + 0] * M_PI/180;
              ip0.a = -out.pointlist[i0 * 2 + 1] * M_PI/180;
              ip1.o = -out.pointlist[i1 * 2 + 0] * M_PI/180;
              ip1.a = -out.pointlist[i1 * 2 + 1] * M_PI/180;
              ip2.o = -out.pointlist[i2 * 2 + 0] * M_PI/180;
              ip2.a = -out.pointlist[i2 * 2 + 1] * M_PI/180;

              tx0.a =  out.pointlist[i0 * 2 + 0] / 360 + 0.5;
              tx0.o = -out.pointlist[i0 * 2 + 1] / 180 + 0.5;
              tx1.a =  out.pointlist[i1 * 2 + 0] / 360 + 0.5;
              tx1.o = -out.pointlist[i1 * 2 + 1] / 180 + 0.5;
              tx2.a =  out.pointlist[i2 * 2 + 0] / 360 + 0.5;
              tx2.o = -out.pointlist[i2 * 2 + 1] / 180 + 0.5;

              /* Polar to Cartesian */
              op0.x = cos (ip0.a) * cos (ip0.o);
              op0.y = cos (ip0.a) * sin (ip0.o);
              op0.z = sin (ip0.a);
              op1.x = cos (ip1.a) * cos (ip1.o);
              op1.y = cos (ip1.a) * sin (ip1.o);
              op1.z = sin (ip1.a);
              op2.x = cos (ip2.a) * cos (ip2.o);
              op2.y = cos (ip2.a) * sin (ip2.o);
              op2.z = sin (ip2.a);

              /* Compute area by adding sizes of every emitted triangle. */
              {
                double R = 6371;  /* Radius of Earth in KM */
                double A = ((op1.x * op0.y) - (op2.x * op0.y) -
                            (op0.x * op1.y) + (op2.x * op1.y) +
                            (op0.x * op2.y) - (op1.x * op2.y));
                double B = ((op1.x * op0.z) - (op2.x * op0.z) -
                            (op0.x * op1.z) + (op2.x * op1.z) +
                            (op0.x * op2.z) - (op1.x * op2.z));
                double C = ((op1.y * op0.z) - (op2.y * op0.z) -
                            (op0.y * op1.z) + (op2.y * op1.z) +
                            (op0.y * op2.z) - (op1.y * op2.z));
                c->area += R * R * 0.5 * sqrt (A*A + B*B + C*C);
              }

              REALLOC(mesh);
              mesh[mesh_out].p = op0;
              mesh[mesh_out].n = op0;
              mesh[mesh_out].t = tx0;
              mesh_out++;

              REALLOC(mesh);
              mesh[mesh_out].p = op1;
              mesh[mesh_out].n = op1;
              mesh[mesh_out].t = tx1;
              mesh_out++;

              if (wire)
                {
                  mesh[mesh_out] = mesh[mesh_out-1];
                  mesh_out++;
                }

              REALLOC(mesh);
              mesh[mesh_out].p = op2;
              mesh[mesh_out].n = op2;
              mesh[mesh_out].t = tx2;
              mesh_out++;

              if (wire)
                {
                  REALLOC(mesh);
                  mesh[mesh_out] = mesh[mesh_out-1];
                  mesh_out++;

                  REALLOC(mesh);
                  mesh[mesh_out] = mesh[omesh_out];
                  mesh_out++;
                }

              if (mesh_out > mesh_size) abort();

              c->mesh_polygon_count++;
            }

          free (in.pointlist);
          free (in.segmentlist);
          free (in.holelist);
          free (out.pointlist);
          free (out.pointattributelist);
          free (out.pointmarkerlist);
          free (out.trianglelist);
          free (out.triangleattributelist);
          free (out.trianglearealist);
          free (out.neighborlist);
          free (out.segmentlist);
       /* free (out.holelist);    -- same as input */
          free (out.segmentmarkerlist);
          free (out.edgelist);
          free (out.edgemarkerlist);
        }

      /* Decide on a scaling factor to use when presenting the country.
         We base that on the size of the bounding box of the largest
         Polygon, not on the overall bounding box of the country.
         This is because the map data for France includes Guiana,
         New Caledonia, etc., which makes it around 180° wide.
         So by picking the largest Polygon, we base scale and center
         point on Metropolitan France instead (the part in Europe).

         Perhaps we should use the overall bbox if the size is small,
         so that we don't overly zoom on the island nations, like
         The Bahamas and Indonesia.
       */
      {
        LL ctr;
        LL size;
        size.o = bbox_main.max.o - bbox_main.min.o;
        size.a = bbox_main.max.a - bbox_main.min.a;

        if (size.o > M_PI*2) size.o -= M_PI*2;	/* Fiji straddles date line */
        if (size.a > M_PI*2) size.a -= M_PI*2;

        ctr.o = bbox_main.min.o + size.o / 2;
        ctr.a = bbox_main.min.a + size.a / 2;

        if (!strcmp (c->geom->code, "AQ"))      /* Center Antarctica on pole */
          {
            ctr.o = 0;
            ctr.a = M_PI/2;
          }

        c->center2.o = -ctr.o;
        c->center2.a = -ctr.a;

        c->center3.x = cos (ctr.a) * cos (ctr.o);
        c->center3.y = cos (ctr.a) * sin (ctr.o);
        c->center3.z = sin (ctr.a);

        /* The bbox size is the rectangular polar size, which was a fine first
           approximation for comparing two Polygons to see which is bigger,
           but is not very accurate due to equirectangular distortion. */
        {
          XYZ a, b;
          a.x = cos (bbox_main.min.a) * cos (bbox_main.min.o);
          a.y = cos (bbox_main.min.a) * sin (bbox_main.min.o);
          a.z = sin (bbox_main.min.a);
          b.x = cos (bbox_main.max.a) * cos (bbox_main.max.o);
          b.y = cos (bbox_main.max.a) * sin (bbox_main.max.o);
          b.z = sin (bbox_main.max.a);
          b.x -= a.x;
          b.y -= a.y;
          b.z -= a.z;
          c->size = sqrt (b.x*b.x + b.y*b.y + b.z*b.z);

          if (!strcmp (c->geom->code, "AQ"))
            c->size *= 4;  /* Fuckin' Antarctica again... */

        }
      }

      /* Move the vertexes to 0,0 and de-rotate, so that the points are relative
         to the center point of the country rather than the center of the Earth.
       */
      for (j = 0; j < 2; j++)
        {
          int k;
          for (k = 0; k < (j ? mesh_out : edge_out); k++)
            {
              double m[16];		 /* α = 0 */
              double b  = -c->center2.a; /* β     */
              double g  =  c->center2.o; /* γ     */
              double cb = cos(b);
              double sb = sin(b);
              double cg = cos(g);
              double sg = sin(g);
              m[0] = cb*cg;        m[4] = -cb*sg; m[8]  = sb; m[12] = -1;
              m[1] = sg + cg*0*sb; m[5] = cg;     m[9]  = 0;  m[13] =  0;
              m[2] = -cg*sb;       m[6] = sb*sg;  m[10] = cb; m[14] =  0;
              m[3] = 0;            m[7] = 0;      m[11] = 0;  m[15] =  1;
              if (j)
                mesh[k].p = transform_point (mesh[k].p, m);
              else
                edge[k].p = transform_point (edge[k].p, m);
            }
        }

      c->mesh_list = glGenLists(1);
      glNewList (c->mesh_list, GL_COMPILE);
      glVertexPointer   (3, GL_FLOAT, sizeof(*mesh), &mesh[0].p);
      glNormalPointer   (   GL_FLOAT, sizeof(*mesh), &mesh[0].n);
      glTexCoordPointer (2, GL_FLOAT, sizeof(*mesh), &mesh[0].t);
      glDrawArrays ((wire ? GL_LINES : GL_TRIANGLES), 0, mesh_out);
      glEndList();

      c->edge_list = glGenLists(1);
      glNewList (c->edge_list, GL_COMPILE);
      glVertexPointer   (3, GL_FLOAT, sizeof(*edge), &edge[0].p);
      glNormalPointer   (   GL_FLOAT, sizeof(*edge), &edge[0].n);
      glTexCoordPointer (2, GL_FLOAT, sizeof(*edge), &edge[0].t);
      glDrawArrays (GL_LINES, 0, edge_out);
      glEndList();
    }

  free (mesh);
  free (edge);
}


static void
shuffle_list (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i;

  /* Fisher-Yates shuffle. */
  for (i = all_countries.count-1; i > 0; i--)
    {
      int j = random() % i;
      int s = gp->country_order[i];
      gp->country_order[i] = gp->country_order[j];
      gp->country_order[j] = s;
    }

  gp->country_idx = 0;

  {
    static const char * const targets[] = {
      /* For debugging, list countries here that should come first.
         (Does not work in conjunction with --fastforward.) */

      /* "AQ", "LV", "BR", "TO", "KP", "CY", "GL", "US-CA", "FJ", "ZA", */

      /* Country endonyms with both ASCII and non-ASCII characters.
         All of these work on macOS Cocoa, iOS, Android and Linux X11.
         All of these work on macOS X11 except "MH", "TO", "WS".

           "AL", "AO", "AR", "AT", "AZ", "BE", "BG", "BI", "BJ", "BR", "BY",
           "CD", "CF", "CG", "CI", "CL", "CM", "CO", "CR", "CU", "CV", "CW",
           "CZ", "DO", "EC", "ES", "FR", "GA", "GN", "GQ", "GR", "GT", "GW",
           "HN", "HU", "IE", "IS", "KG", "KZ", "LI", "LU", "MC", "MF", "MH",
           "MU", "MW", "MZ", "NI", "PA", "PE", "PT", "PY", "RO", "RS", "RU",
           "SC", "SK", "SN", "ST", "SV", "TD", "TG", "TL", "TM", "TO", "TR",
           "UA", "UY", "UZ", "VE", "VN", "WS", "XK",

         Country endonyms with only non-ASCII chars, left-to-right scripts.
         All of these work on macOS Cocoa, iOS and Android.
         None of these work on Linux X11 except "CY", "MK", "MN", "TJ".
         None of these work on macOS X11.

           "AM", "BD", "BT", "CN", "CY", "ER", "GE", "IN", "JP", "KH", "KP",
           "KR", "LA", "LK", "MK", "MM", "MN", "MO", "NP", "TH", "TJ", "TW",

         Country endonyms with only non-ASCII chars, right-to-left scripts.
         All of these work on macOS Cocoa, iOS and Android.
         None of these work on Linux X11 or macOS X11.

           "AE", "AF", "BH", "DJ", "DZ", "EG", "IL", "IQ", "IR", "JO", "KW",
           "LB", "LY", "MA", "MR", "MV", "OM", "PK", "PS", "QA", "SA", "SD",
           "SY", "TN", "YE",
       */

      0 };
    int j, k = 0;
    for (j = 0; targets[j]; j++)
      {
        Bool done = False;
        for (i = 0; i < all_countries.count; i++)
          {
            int idx = gp->country_order[i];
            if (!strcmp (gp->countries[idx].geom->code, targets[j]))
              {
                int s = gp->country_order[k];
                gp->country_order[k] = gp->country_order[i];
                gp->country_order[i] = s;
                done = True;
                k++;
              }
          }
        if (!done) abort();
      }
  }

# if 0  /* Show everything for debugging */
  for (i = 0; i < all_countries.count; i++)
    gp->countries[i].state = PLACED;
# endif
}


static void
shuffle_drops (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < all_countries.count; i++)
    {
      country_dlist *c = &gp->countries[i];
      c->drop_step   = (1 + frand(0.3)) * 0.01 * speed_arg;
      c->drop_ratio  = frand(1) * -2;
    }
}


static void
shuffle_spins (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < all_countries.count; i++)
    {
      country_dlist *c = &gp->countries[i];
      GLfloat max = M_PI;

      if (!(random() % 3))
        {
          max *= 2;
          if (!(random() % 3))
            {
              max *= 2;
              if (!(random() % 3))
                max *= 2;
            }
        }

      c->drop_spin.x = (random() % 3) ? 0 : (BELLRAND(1)-0.5) * max;
      c->drop_spin.y = (random() % 3) ? 0 : (BELLRAND(1)-0.5) * max;
      c->drop_spin.z = (random() % 3) ? 0 : (BELLRAND(1)-0.5) * max * 3;
    }
}


static void
print_number (char *out, const char *head, const char *tail, int i)
{
  strcat (out, head);
  out += strlen (out);

  if (i > 1000000000)
    sprintf (out, "%d,%03d,%03d,%03d", 
             i / 1000000000, (i / 1000000) % 1000,
             (i / 1000) % 1000, i % 1000);
  else if (i > 1000000)
    sprintf (out, "%d,%03d,%03d", i / 1000000, (i / 1000) % 1000, i % 1000);
  else if (i > 1000)
    sprintf (out, "%d,%03d", i / 1000, i % 1000);
  else
    sprintf (out, "%d", i);

  strcat (out, tail);
}


/* Whether the font is capable of rendering (most of) this string.
   If all or most of the characters are missing, we omit the endonym
   instead of drawing a row of boxes.
 */
static Bool
printable_p (texture_font_data *data, const char *string)
{
  long in_len = strlen(string);
  const unsigned char *in = (const unsigned char *) string;
  const unsigned char *in_end = in + in_len;
  int failures = 0;
  int nchars = 0;
  while (in < in_end)
    {
      char s[8];
      unsigned long uc = 0;
      long L = utf8_decode (in, in_end - in, &uc);
      memcpy (s, in, L);
      s[L] = 0;
      in += L;
      if (uc > 0xFF && blank_character_p (data, s))
        failures++;
      nchars++;
    }

  return (failures <= nchars / 3);
}


static void
tick_planet (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  GLfloat ts;
  double fps = 27;
  int i;

  if (gp->button_down_p) return;

  /* Tick based on duration of current state
   */
  switch (gp->state) {
  case INIT:            ts = 2.0; break;
  case FASTFORWARD:     ts = 2.0; break;
  case RAISE_COUNTRY:   ts = 3.0; break;
  case DESC_ON:         ts = 0.5; break;
  case POSITION_GLOBE:  ts = 2.0; break;
  case PLACE_COUNTRY:   ts = 3.0; break;
  case DESC_OFF:        ts = 0.5; break;
  case PULSE_COUNTRIES: ts = 0.0; break;
  case DROP_COUNTRIES:  ts = 0.0; break;
  default:              abort();  break;
  }

  gp->tick += speed_arg * (ts == 0 ? 0 : (1 / (ts * fps)));

  switch (gp->state) {
  case DROP_COUNTRIES:
  case PULSE_COUNTRIES:
    {
      Bool dropping_p = False;
      for (i = 0; i < all_countries.count; i++)
        {
          country_dlist *c = &gp->countries[i];
          if (c->state == PLACED || c->state == DROPPED)
            {
              c->state = DROPPED;
              c->drop_ratio += c->drop_step;
              if (c->drop_ratio < 1)
                dropping_p = True;
            }
        }
      if (! dropping_p)
        gp->tick = 1;		/* Done dropping all of them */
    }
    break;
  case POSITION_GLOBE:
    {
      GLfloat r = ease (EASE_IN_OUT_SINE, gp->tick);
      LL dist;
      dist.a = gp->pos_end.a - gp->pos_start.a;
      dist.o = gp->pos_end.o - gp->pos_start.o;
      while (dist.o >  M_PI) dist.o -= M_PI*2;
      while (dist.o < -M_PI) dist.o += M_PI*2;
      while (dist.a >  M_PI) dist.a -= M_PI*2;
      while (dist.a < -M_PI) dist.a += M_PI*2;
      gp->orient.o = gp->pos_start.o + dist.o * r;
      gp->orient.a = gp->pos_start.a + dist.a * r;
    }
    break;
  default:
    break;
  }

  if (gp->tick < 1) return;

  gp->tick = 0;

  /* Advance state machine.
   */
  switch (gp->state) {
  case INIT:
    if (do_fastforward)
      gp->state = FASTFORWARD;
    else
      gp->state = RAISE_COUNTRY;
    break;
  case FASTFORWARD:
    for (i = 0; i < all_countries.count; i++)
      {
        country_dlist *c = &gp->countries[i];
        if (c->state == DROPPED)
          c->state = PLACED;
      }
    gp->state = RAISE_COUNTRY;
    break;
  case RAISE_COUNTRY:
  case DESC_ON:
  case POSITION_GLOBE:
  case PLACE_COUNTRY:
    gp->state++;
    break;
  case PULSE_COUNTRIES:
    for (i = 0; i < all_countries.count; i++)
      {
        country_dlist *c = &gp->countries[i];
        if (c->state == DROPPED)
          c->state = PLACED;
      }
    if (gp->country_idx >= all_countries.count-1)
      gp->state = DROP_COUNTRIES;
    else
      gp->state = RAISE_COUNTRY;
    break;
  case DESC_OFF:
    gp->country_idx++;
    if (gp->country_idx >= all_countries.count-1)
      gp->state = PULSE_COUNTRIES;
    else if (! (random() % 100))
      gp->state = PULSE_COUNTRIES;
    else
      gp->state = RAISE_COUNTRY;
    break;
  case DROP_COUNTRIES:
    gp->state = RAISE_COUNTRY;
    break;
  default:
    abort();
    break;
  }

  /* Just entered this state, or restarting this state.
   */
  switch (gp->state) {
  case FASTFORWARD:
    {
      /* Move any tiny nations to the front, and exclude them.
           12000: 26% - features are less than around 2 pixels.
           40000: 37%
          100000: 48%
          400000: 75%
          800000: 85%
         1500000: 92%
         9000000: 99%
       */
      int min_area = 12000;
      GLfloat r = 0.4 + frand(0.2);
      int j = 0;
    
      shuffle_list (mi);
      for (i = 0; i < all_countries.count; i++)
        {
          int idx = gp->country_order[i];
          country_dlist *c = &gp->countries[idx];
          if (c->area < min_area)
            {
              int s = gp->country_order[j];
              gp->country_order[j] = gp->country_order[i];
              gp->country_order[i] = s;
              j++;
            }
        }
      gp->fastforward_count = j;
  
      /* After placing the islands, advance to at least N% through the list. */
      j = all_countries.count * r;
      if (gp->fastforward_count < j)
        gp->fastforward_count = j;

      gp->country_idx = gp->fastforward_count;

      for (i = 0; i < gp->fastforward_count; i++)
        {
          int idx = gp->country_order[i];
          country_dlist *c = &gp->countries[idx];
          c->state = DROPPED;
        }
    }
    break;

  case RAISE_COUNTRY:
    if (gp->country_idx == 0)
      shuffle_list (mi);
   {
      /* Generate the label string for this country. */
      int idx = gp->country_order[gp->country_idx];
      country_dlist *cc = &gp->countries[idx];
      const char *name = cc->geom->name;
      const char *endonym = (printable_p (gp->label_font, cc->geom->endonym)
                             ? cc->geom->endonym :
                             printable_p (gp->label_font, cc->geom->endonym2)
                             ? cc->geom->endonym2 : "");
      const char *code = cc->geom->code;
      int pop = cc->geom->population;
      int area = cc->area;
      char *label;
      GLfloat planet_scale = 4;
      int cols = (MI_WIDTH(mi) < MI_HEIGHT(mi) ? 20 : 40);   /* Approx */
      int wrap_px = planet_scale * cols * 18 /* pt */;
      char *name2 = xft_word_wrap (MI_DISPLAY (mi),
                                   texfont_xft (gp->label_font), name,
                                   wrap_px);
      char *endonym2 = xft_word_wrap (MI_DISPLAY (mi),
                                      texfont_xft (gp->label_font), endonym,
                                      wrap_px);
      Bool double_space_p = (strchr (name2, ' ') &&
                             strchr (endonym2, ' '));
      const char *LF = double_space_p ? "\n\n" : "\n";

      label = (char *) malloc (strlen(name2) + strlen(endonym2) + 100);
      *label = 0;

      if (*endonym2)
        sprintf (label + strlen(label), "%s%s", endonym2, LF);
      if (*name2)
        sprintf (label + strlen(label), "%s%s", name2, LF);

      if (!strcmp (code, "NULL"))
        strcat (label, "Pop: NaN\n");
      else if (pop)
        print_number (label + strlen(label), "Pop:  ", "\n", pop);

# define SQ "\xC2\xB2"  /* ² */
      print_number (label + strlen(label), "Area: ", " km" SQ, area);

      texture_string_metrics (gp->label_font, label, &gp->label_geom, 0, 0);

      if (gp->label) free (gp->label);
      gp->label = label;
      free (name2);
    }
    break;
  case PULSE_COUNTRIES:
    shuffle_drops (mi);
    break;
  case DROP_COUNTRIES:
    shuffle_list (mi);
    shuffle_drops (mi);
    for (i = 0; i < all_countries.count; i++)
      {
        country_dlist *c = &gp->countries[i];
        c->state = PLACED;	/* Undo the drop from PULSE_COUNTRIES */
        c->drop_ratio -= 3;	/* Around 6 seconds */
      }
    break;
  case POSITION_GLOBE:
    {
      int idx = gp->country_order[gp->country_idx];
      country_dlist *cc = &gp->countries[idx];
      gp->pos_start = gp->orient;
      gp->pos_end.a = -cc->center2.a;
      gp->pos_end.o =  cc->center2.o;
      /* Stay ahead of the spin. */
      gp->pos_end.o -= M_PI / 8 * spin_arg * speed_arg;
    }
    break;
  default:
    break;
  }

  shuffle_spins (mi);

#if 0
  switch (gp->state) {
  case INIT:            fprintf(stderr, "%s: INIT\n", blurb()); break;
  case FASTFORWARD:     fprintf(stderr, "%s: FASTFORWARD %d\n", blurb(),
                                gp->fastforward_count); break;
  case RAISE_COUNTRY:   fprintf(stderr, "%s: RAISE_COUNTRY %d = %d %s\n",
                                blurb(), gp->country_idx,
                                gp->country_order[gp->country_idx],
                                gp->countries[
                                  gp->country_order[gp->country_idx]]
                                  .geom->name); break;
  case DESC_ON:         fprintf(stderr, "%s: DESC_ON\n", blurb()); break;
  case POSITION_GLOBE:  fprintf(stderr, "%s: POSITION_GLOBE\n", blurb());break;
  case PLACE_COUNTRY:   fprintf(stderr, "%s: PLACE_COUNTRY\n", blurb()); break;
  case DESC_OFF:        fprintf(stderr, "%s: DESC_OFF\n", blurb()); break;
  case PULSE_COUNTRIES: fprintf(stderr, "%s: PULSE_COUNTRIES\n",blurb());break;
  case DROP_COUNTRIES:  fprintf(stderr, "%s: DROP_COUNTRIES\n", blurb());break;
  default:              abort(); break;
  }
#endif
}


/* Instead of calling gltrackball_rotate, we extract the X and Y out
   of the trackball's quat and just use those to adjust the orientation
   of the globe rather than the whole scene.  This also means that 
   North will stay up.

   #### Except this doesn't work very well.  It works ok for the first
   few drags, but after a while it stops tracking the mouse properly.
  */
static void
trackball_orient (ModeInfo *mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  LL track;
  quat tq = gltrackball_get_quat (gp->trackball);
  vec3 e = quat_euler_angles (tq);
  track.a = e.x;
  track.o = e.y;

  gp->orient.a -= gp->last_track.a - track.a;
  gp->orient.o -= gp->last_track.o - track.o;
  gp->last_track = track;
}


ENTRYPOINT void
reshape_planet (ModeInfo *mi, int width, int height)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glXMakeCurrent(MI_DISPLAY(mi), gp->window, *gp->glx_context);

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT Bool
planet_handle_event (ModeInfo *mi, XEvent *event)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, gp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &gp->button_down_p))
    {
      if (gp->state == POSITION_GLOBE && !gp->button_down_p)
        {
          /* If the user dragged while we were repositioning the globe,
             start over. */
          trackball_orient (mi);
          gp->pos_start = gp->orient;
          gp->tick = 0;
        }

      return True;
    }

  return False;
}


ENTRYPOINT void
init_planet (ModeInfo * mi)
{
  planetstruct *gp;
  int screen = MI_SCREEN(mi);
  Bool wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, planets);
  gp = &planets[screen];

  gp->window = MI_WINDOW(mi);

  if ((gp->glx_context = init_GL(mi)) != NULL) {
    reshape_planet(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  gp->title_font = load_texture_font (mi->dpy, "titleFont");
  gp->label_font = load_texture_font (mi->dpy, "labelFont");

  {
    double spin_speed   = speed_arg * 0.1;
    double wander_speed = speed_arg * 0.005;
    double tilt_speed   = speed_arg * 0.01;
    gp->rot = make_rotator (do_roll ? spin_speed : 0,
                            do_roll ? spin_speed : 0,
                            0, 1,
                            do_wander ? wander_speed : 0,
                            True);
    gp->rot2 = make_rotator (0, 0, 0, 0, tilt_speed, False);
    gp->trackball = gltrackball_init (False);
  }

  if (!wire)
    {
      GLfloat pos[4] = {1, 1, 1, 0};
      GLfloat amb[4] = {0, 0, 0, 1};
      GLfloat dif[4] = {1, 1, 1, 1};
      GLfloat spc[4] = {0, 1, 1, 1};
      glEnable (GL_LIGHT0);
      glLightfv (GL_LIGHT0, GL_POSITION, pos);
      glLightfv (GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv (GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv (GL_LIGHT0, GL_SPECULAR, spc);
    }

  if (wire)
    do_texture = False;

  init_countries (mi);

  if (do_texture)
    setup_texture (mi);

  if (do_stars)
    init_stars (mi);

  /* construct the polygons of the planet
   */
  gp->platelist = glGenLists(1);
  glNewList (gp->platelist, GL_COMPILE);
  {
    GLfloat th;
    int s = 128;
    glBegin (GL_TRIANGLE_FAN);
    glVertex3f (0, 0, 0);
    for (th = 0; th < M_PI*2; th += M_PI * 2 / s)
      glVertex3f (cos(th), sin(th), 0);
    glVertex3f (1, 0, 0);
    glEnd();
    gp->plate_polygon_count = s;
  }
  glEndList();

  /* construct the polygons of the latitude/longitude/axis lines.
   */
  gp->latlonglist = glGenLists(1);
  glNewList (gp->latlonglist, GL_COMPILE);
  glPushMatrix ();
  glRotatef (90, 1, 0, 0);  /* unit_sphere is off by 90 */
  glRotatef (8,  0, 1, 0);  /* line up the time zones */
  unit_sphere (12, 24, 1);
  glPopMatrix ();
  glEndList();

  /* Construct the line segments of the timezone lines.  The list of line
     segments is in a flat equirectangular map, so we need to project that
     onto a sphere, and interpolate additional segments within each line to
     make it curve.  It would be possible to also project these to a tube
     for the Mercator and Equirectangular modes, but I haven't bothered.
   */
  gp->tzpoints = 0;
  gp->tzlist = glGenLists(1);
  glNewList (gp->tzlist, GL_COMPILE);
  glPushMatrix ();
  glRotatef (90, 1, 0, 0);
  glRotatef (180, 0, 0, 1);
  {
    GLfloat minx = 99999, miny = 99999, maxx = -99999, maxy = -99999;
    const GLfloat *p = (GLfloat *) timezones->data;
    int lines = timezones->points / 2;
    GLfloat min_seg = 0.05;
    int i;
    if (timezones->primitive != GL_LINES) abort();
    if (timezones->format != GL_V3F) abort();

    /* Find the bounding box. */
    for (i = 0; i < timezones->points; i++)
      {
        GLfloat x = p[i * 3];
        GLfloat y = p[i * 3 + 1];
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
      }

    for (i = 0; i < lines; i++)
      {
        GLfloat x0 = minx + p[0] / (maxx-minx);
        GLfloat y0 = miny + p[1] / (maxy-miny);
        GLfloat x1 = minx + p[3] / (maxx-minx);
        GLfloat y1 = miny + p[4] / (maxy-miny);

        GLfloat d = sqrt ((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
        int steps = d / min_seg;
        int i;
        if (steps == 0) steps = 1;

        glBegin (GL_LINE_STRIP);
        for (i = 0; i <= steps; i++)
          {
            GLfloat r = i / (GLfloat) steps;
            GLfloat x2 = x0 + r * (x1 - x0);
            GLfloat y2 = y0 + r * (y1 - y0);
            GLfloat th1 =  y2 * M_PI - M_PI_2;  /* longitude radians */
            GLfloat th2 = -x2 * M_PI * 2;       /* latitude radians */
            GLfloat x3 = cos(th1) * cos(th2);
            GLfloat y3 = sin(th1);
            GLfloat z3 = cos(th1) * sin(th2);
            glVertex3f (x3, y3, z3);
            gp->tzpoints++;
          }
        glEnd();
        p += 6;
      }
  }
  glPopMatrix ();
  glEndList();


  gp->country_order = (int *)
    calloc (all_countries.count, sizeof(*gp->country_order));
  for (i = 0; i < all_countries.count; i++)
    gp->country_order[i] = i;

  /* Prefer the official area to the computed area. */
  for (i = 0; i < all_countries.count; i++)
    if (gp->countries[i].geom->area)
      gp->countries[i].area = gp->countries[i].geom->area;
}


ENTRYPOINT void
draw_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  XYZ wander;
  int i;
  GLfloat planet_scale = 4;

  if (!gp->glx_context)
    return;

  if (MI_WIDTH(mi) < MI_HEIGHT(mi))
    planet_scale *= 1.5;

  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glXMakeCurrent (dpy, window, *gp->glx_context);

  mi->polygon_count = 0;

  if (gp->button_down_p)
    {
      gp->draw_compass = 60;
      gp->draw_axis = 0;
    }
  else if (!gp->draw_axis && !gp->draw_compass && !(random() % 4000))
    {
      gp->draw_axis = 60 + (random() % 90);
      gp->timezones_p = !(random() % 10);
    }
  else if (!gp->draw_axis && !gp->draw_compass && !(random() % 4000) &&
           gp->country_idx > 20)
    gp->draw_compass = 120 + (random() % 90);

  if (do_rotate && !gp->button_down_p)
    gp->orient.o += 0.005 * spin_arg * speed_arg; /* Sun sets in the west */

  if (do_roll)
    {
      double x, y;
      LL max = { 40 * M_PI/180,
          55 * M_PI/180 };
      LL roll;
      get_position (gp->rot2, &x, &y, 0, !gp->button_down_p);
      roll.o = (x - 0.5) * max.o;
      roll.a = (y - 0.5) * max.a;
      gp->orient.o += (gp->last_roll.o - roll.o);
      gp->orient.a += (gp->last_roll.a - roll.a);
      gp->last_roll = roll;
    }

  glEnable (GL_LINE_SMOOTH);
  glEnable (GL_POINT_SMOOTH);
  glEnable (GL_DEPTH_TEST);

  glPushMatrix();


  /* Wander */
  {
    double x, y, z;
    GLfloat s1 = 6;	/* horiz/vert */
    GLfloat s2 = 8;	/* depth */

    GLfloat a = MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
    get_position (gp->rot, &x, &y, &z, !gp->button_down_p);
    wander.x = (x - 0.5) * s2;		/* depth */
    wander.y = (y - 0.5) * s1 * a;	/* horiz */
    wander.z = (z - 0.5) * s1 * 1/a;	/* vert */
  }

  trackball_orient (mi);

  if (do_stars)
    {
      glDisable(GL_TEXTURE_2D);
      glPushMatrix();
      glRotatef (-gp->orient.a * 180/M_PI, 1, 0, 0);
      glRotatef (-gp->orient.o * 180/M_PI, 0, 1, 0);
      glScalef (60, 60, 60);
      glRotatef (90, 1, 0, 0);
      glRotatef (35, 1, 0, 0);
      glCallList (gp->starlist);
      mi->polygon_count += gp->starcount;
      glPopMatrix();
      glClear(GL_DEPTH_BUFFER_BIT);
    }

  glRotatef (90, 1, 0, 0);	/* North up */
  glRotatef (90, 0, 0, 1);	/* Longitude 0 front */

  glPushMatrix();

  /* Rather than drawing a sphere as a substrate, we just use a
     flat disk to black out the background behind the globe. */
  if (gp->state == INIT || !do_underground)
    {
      GLfloat m[4][4];
      GLfloat s = planet_scale * 0.993;

      if (gp->state == INIT)
        s *= gp->tick;

      /* Billboard rotation */
      glGetFloatv (GL_MODELVIEW_MATRIX, &m[0][0]);
      check_gl_error ("read modelview");
      m[0][0] = 1; m[1][0] = 0; m[2][0] = 0;
      m[0][1] = 0; m[1][1] = 1; m[2][1] = 0;
      m[0][2] = 0; m[1][2] = 0; m[2][2] = 1;
      glPushMatrix();
      glLoadIdentity();
      glMultMatrixf (&m[0][0]);
      check_gl_error ("billboard modelview");
      glRotatef (current_device_rotation(), 0, 0, 1);
      glTranslatef (-wander.y, -wander.z, wander.x);
      glScalef (s, s, s);
      glDisable (GL_LIGHTING);
      glDisable (GL_TEXTURE_2D);
      glEnable (GL_BLEND);
      glColor4f ((gp->state == INIT ? 0.4 * (1 - gp->tick) : 0),
                 0, 0.05,
                 (do_underground ? 1-gp->tick : 1));
      glFrontFace (GL_CCW);
      glCallList (gp->platelist);
      mi->polygon_count += gp->plate_polygon_count;
      glPopMatrix();
      glDisable (GL_BLEND);
    }

  {
    int idx = gp->country_order[gp->country_idx];
    country_dlist *cc = &gp->countries[idx];
    GLfloat label_alpha = 0;
    GLfloat label_matrix[4][4];

    switch (gp->state) {
    case INIT: break;
    case FASTFORWARD: break;
    case RAISE_COUNTRY:
      cc->state = RAISED;
      break;
    case DESC_ON:
      label_alpha = gp->tick;
      break;
    case POSITION_GLOBE:
      label_alpha = 1;
      break;
    case PLACE_COUNTRY:
      cc->state = PLACED;
      label_alpha = 1;
      break;
    case DESC_OFF:
      label_alpha = 1 - gp->tick;
      break;
    case PULSE_COUNTRIES:
    case DROP_COUNTRIES:
      break;
    default:
      abort();
      break;
    }


    /* If we normalize during placement, then countries very near 180°
       have a discontinuity.  But if we don't normalize during placement,
       then countries will go all the way around the globe the long way,
       if orient.o ended up being > 180°.  I don't understand why that
       discontinuity happens, but let's paper over it by suppressing
       normalization for those countries.  This affects:
       "AS", "FJ", "FM", "MH", "NC", "NF", "NR", "NU", "NZ", "SB",
       "TO", "TV", "VU", "WF", "WS".
     */
    if (gp->state != PLACE_COUNTRY ||
        (cc->center2.o * 180/M_PI > -165 &&
         cc->center2.o * 180/M_PI <  150))
      gp->orient = latlon_normalize (gp->orient);

    for (i = 0; i < all_countries.count; i++)
      {
        country_dlist *c = &gp->countries[i];
        GLfloat front_ratio = 0;
        GLfloat drop_ratio  = 0;
        GLfloat pulse_ratio = 0;
        XYZ country_pos = c->center3;

        switch (c->state) {
        case GROUND:
          /* Startup: skip entirely */
          drop_ratio = 1;
          break;
        case RAISED:
          if (c == cc && gp->state == RAISE_COUNTRY)
            {
              /* Moving from ground to facing front */
              front_ratio = gp->tick;
              drop_ratio  = 1 - gp->tick;
            }
          else
            front_ratio = 1;
          break;
        case PLACED:
          if (c == cc && gp->state == PLACE_COUNTRY)
            /* Moving from facing front to the globe */
            front_ratio = 1 - gp->tick;
          break;
        case DROPPED:
          if (gp->state == DROP_COUNTRIES)
            /* Moving from the globe to the ground. */
            drop_ratio = (c->drop_ratio <= 0 ? 0 : c->drop_ratio);
          else if (gp->state == PULSE_COUNTRIES)
            /* Moving from the globe to the ground. */
            pulse_ratio = (c->drop_ratio <= 0 ? 0 : c->drop_ratio);
          else if (gp->state == FASTFORWARD && i < gp->fastforward_count)
            /* Moving from ground to the globe, in parallel. */
            drop_ratio = 1-gp->tick;
          else
            drop_ratio = 1;
          break;
        default:
          abort();
          break;
        }

        if (drop_ratio >= 1)    /* Invisible, skip it */
          continue;

        if (front_ratio)
          {
            GLfloat r = 1 - front_ratio;
# if 0
            /* Doing a linear interpolation works ok most of the time since
               since POSITION_GLOBE has made the difference in orientation
               relatively small.
             */
            XYZ p0 = { 1, 0, 0 };
            XYZ p1 = c->center3;
            country_pos.x = p0.x + (p1.x - p0.x) * r;
            country_pos.y = p0.y + (p1.y - p0.y) * r;
            country_pos.z = p0.z + (p1.z - p0.z) * r;
# else
            /* Slerping is more generally correct. Both versions sometimes
               introduce unnecessarily-sweeping arcs, and sometimes go more
               than 180° around during PLACE_COUNTRY and I can't figure out
               why.  (Test case: Set 'targets' to "LV", "BR".)
             */
            quat q1 = { 1, 0, 0, 1 };
            quat q2, q3;
            LL p2c;
            GLfloat L;

            p2c.a = -c->center2.a;
            p2c.o = -c->center2.o;
            q2.x = cos (p2c.a) * cos (p2c.o);
            q2.y = cos (p2c.a) * sin (p2c.o);
            q2.z = sin (p2c.a);
            q2.w = 1;

            q3 = quat_slerp (r, q1, q2);

            L = sqrt (q3.x * q3.x + q3.y * q3.y + q3.z * q3.z);
            country_pos.x = q3.x / L;
            country_pos.y = q3.y / L;
            country_pos.z = q3.z / L;
# endif
          }


        /* Start rendering
         */
        glPushMatrix();

        /* First, move to the proper position to draw the country.
           Then after all of that, rotate and scale.
           If we don't do it in this order, there are some highly
           exaggerated swoops.
         */

        {
          GLfloat fr = (drop_ratio
                        ? ((gp->state == DROP_COUNTRIES ||
                            gp->state == FASTFORWARD)
                            ? 0 : 1)
                        : front_ratio);
          GLfloat r1 = ease ((gp->state == PLACE_COUNTRY
                              ? EASE_IN_SINE /* EASE_IN_BOUNCE */
                              : EASE_OUT_SINE),
                             1 - fr);

          if (drop_ratio > 0)
            {
              /* Straight down */
              GLfloat r = ease (EASE_IN_CUBIC, drop_ratio);
              GLfloat dist = 5 * planet_scale;
              glTranslatef (0, 0, r * dist);
            }

          glRotatef (current_device_rotation(), 1, 0, 0);
          glTranslatef (wander.x * r1, wander.y * r1, wander.z * r1);

          glRotatef ( gp->orient.a * 180/M_PI * r1, 0, 1, 0);
          glRotatef ( gp->orient.o * 180/M_PI * r1, 0, 0, 1);
          glTranslatef (country_pos.x * planet_scale,
                        country_pos.y * planet_scale,
                        country_pos.z * planet_scale);
          glRotatef (-gp->orient.o * 180/M_PI * r1, 0, 0, 1);
          glRotatef (-gp->orient.a * 180/M_PI * r1, 0, 1, 0);

          if (front_ratio > 0)
            {
              GLfloat r2 = 1-r1;
              GLfloat off = r2 * (4 + wander.x);  /* Toward camera */

              /* This scales every country to the same size, meaning sometimes
                 they grow during placement rather than shrink. */
              GLfloat s = r1 + (1 / c->size) * 1.5 * r2;

              /* This makes them not shrink, meaning Brazil/Russia are huge. */
              /* GLfloat s = 1 + (1 / c->size) * 0.5 * r2; */

              glTranslatef (off, 0, 0);
              glScalef (s, s, s);
            }


          /* Positioning is finished; now orient the country to face the
             right way. */
              
          glRotatef (gp->orient.a * 180/M_PI * r1, 0, 1, 0);
          glRotatef (gp->orient.o * 180/M_PI * r1, 0, 0, 1);

          glRotatef (-c->center2.o * 180/M_PI * r1, 0, 0, 1);
          glRotatef ( c->center2.a * 180/M_PI * r1, 0, 1, 0);

          if ((gp->state == RAISE_COUNTRY ||
               gp->state == PLACE_COUNTRY) &&
              (drop_ratio || front_ratio || pulse_ratio))
            {
              GLfloat r = gp->tick;
              GLfloat p = 0.7;
              r = (r < p
                   ? r * (1/p)
                   : 1 - (r-p) / (1-p));
              r = ease (EASE_IN_OUT_SINE, r);
              glRotatef (c->drop_spin.z * 180/M_PI * r, 1, 0, 0);
              glRotatef (c->drop_spin.y * 180/M_PI * r, 0, 0, 1);
              glRotatef (c->drop_spin.x * 180/M_PI * r, 0, 1, 0);
            }

          if (drop_ratio > 0 &&
              (gp->state == DROP_COUNTRIES || gp->state == FASTFORWARD))
            {
              /* Away from surface */
              GLfloat r = ease (EASE_OUT_CUBIC, drop_ratio);
              GLfloat dist = 0.3 * planet_scale;
              if (gp->state == FASTFORWARD) dist *= 40;
              glTranslatef (r * dist, 0, 0);
            }
          else if (pulse_ratio > 0)
            {
              GLfloat dist = 0.5 * planet_scale;
              GLfloat r = (pulse_ratio < 0.5
                           ? ease (EASE_OUT_ELASTIC, 2 * pulse_ratio)
                           : pulse_ratio <= 1
                           ? ease (EASE_IN_BOUNCE, 2 * (1-pulse_ratio))
                           : 0);
              glTranslatef (r * dist, 0, 0);
            }

        }

        /* Draw the country interiors */

        glScalef (planet_scale, planet_scale, planet_scale);

        if (!wire)
          {
            GLfloat c0[] = { 1, 1, 1, 1 };
            GLfloat c1[] = { 0, 1, 0, 1 };
            GLfloat c2[] = { 0.25, 0.25, 0.25, 1 };

            if (do_texture)
              {
                glDisable (GL_LIGHTING);
                glEnable (GL_TEXTURE_2D);
                glColor4fv (c0);
                glBindTexture (GL_TEXTURE_2D, gp->tex1);
              }
            else
              {
                glEnable (GL_LIGHTING);
                glDisable (GL_TEXTURE_2D);
                glColor4fv (c1);
                glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c1);
              }

            glCullFace (GL_BACK); 
            glEnable (GL_CULL_FACE);
            glFrontFace (GL_CCW);

            glPushMatrix();
            glCallList (c->mesh_list);
            mi->polygon_count += c->mesh_polygon_count;
            glPopMatrix();

            /* Draw the back face only if it might be visible.
             */
            if (do_underground || c->state != PLACED || front_ratio != 0)
              {
                glColor4fv (c2);
                glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c2);
                if (do_texture)
                  glBindTexture (GL_TEXTURE_2D, gp->tex2);
                glFrontFace (GL_CW);
                glCallList (c->mesh_list);
                mi->polygon_count += c->mesh_polygon_count;
              }

            glBindTexture (GL_TEXTURE_2D, 0);
          }

        /* Draw the country outlines.
         */

        glDisable (GL_TEXTURE_2D);
        glDisable (GL_LIGHTING);
        glEnable (GL_LINE_SMOOTH);
        glColor3f (0.7, 0.7, 0.5);

        glCallList (c->edge_list);
        mi->polygon_count += c->edge_polygon_count;

        /* Save the orientation at which we should draw the country label. */
        if (c == cc && label_alpha > 0)
          glGetFloatv (GL_MODELVIEW_MATRIX, &label_matrix[0][0]);
        glPopMatrix();
      }

    /* Draw the label text.
       Do this after drawing the countries so that it's always on top.
     */
    if (label_alpha > 0)
      {
        GLfloat s   = 1.0 / 300;
        GLfloat min = s / 4;

        if (MI_WIDTH(mi) < MI_HEIGHT(mi))
          s *= 2;

        /* Billboard rotation */
        label_matrix[0][0] = 1; label_matrix[1][0] = 0; label_matrix[2][0] = 0;
        label_matrix[0][1] = 0; label_matrix[1][1] = 1; label_matrix[2][1] = 0;
        label_matrix[0][2] = 0; label_matrix[1][2] = 0; label_matrix[2][2] = 1;

        glPushMatrix();
        glLoadIdentity();
        glMultMatrixf (&label_matrix[0][0]);
        glRotatef (current_device_rotation(), 0, 0, 1);

        if (gp->state == PLACE_COUNTRY)
          s *= 1 - gp->tick;
        else if (gp->state == DESC_OFF)
          s = 0;
        if (s < min) s = min;
        glScalef (s, s, s);

        glTranslatef (-gp->label_geom.width / 2,	/* Center text */
                      (-gp->label_geom.ascent + gp->label_geom.descent) / 2,
                      0.25 / s);		/* Move text toward camera */

        /* Without this we get white hairlines over the black outline */
        glDisable (GL_DEPTH_TEST);

        /* Draw the text five times, to give it a border. */
        {
          const XYZ offsets[] = {{ -1, -1, 0 },
                                 { -1,  1, 0 },
                                 {  1,  1, 0 },
                                 {  1, -1, 0 },
                                 {  0,  0, 1 }};
          int i;
          glColor4f (0, 0, 0, label_alpha);
          s = 2;
          for (i = 0; i < countof(offsets); i++)
            {
              if (offsets[i].x == 0)
                glColor4f (1, 1, 0, label_alpha);
              glPushMatrix();
              glTranslatef (s*offsets[i].x, s*offsets[i].y, offsets[i].z);
              print_texture_string (gp->label_font, gp->label);
              glPopMatrix();
            }
          glEnable (GL_DEPTH_TEST);
        }
        glPopMatrix();
      }
  }
  glPopMatrix();


  /* Done with countries. Draw axes, timezones and compass.
   */

#if 0
  gp->draw_axis = 1;
  gp->draw_compass = 1;
#endif
  if (gp->state == DROP_COUNTRIES)
    ;
  else if (gp->draw_axis && !gp->timezones_p)
    {
      glPushMatrix();
      glRotatef (current_device_rotation(), 1, 0, 0);
      glTranslatef (wander.x, wander.y, wander.z);
      glScalef (planet_scale, planet_scale, planet_scale);
      glRotatef (gp->orient.a * 180/M_PI, 0, 1, 0);
      glRotatef (gp->orient.o * 180/M_PI, 0, 0, 1);
      glRotatef(360 / 48, 0, 0, 1);
      glScalef (1.02, 1.02, 1.02);
      glDisable (GL_TEXTURE_2D);
      glDisable (GL_LIGHTING);
      glDisable (GL_LINE_SMOOTH);
      glColor3f (0.1, 0.3, 0.1);
      glCallList (gp->latlonglist);
      mi->polygon_count += 24*24;
      glPopMatrix();
      if (!wire && !do_texture)
        glEnable (GL_LIGHTING);
      if (gp->draw_axis) gp->draw_axis--;
    }
  else if (gp->draw_axis && gp->timezones_p)
    {
      glPushMatrix();
      glRotatef (current_device_rotation(), 1, 0, 0);
      glTranslatef (wander.x, wander.y, wander.z);
      glScalef (planet_scale, planet_scale, planet_scale);
      glRotatef (gp->orient.a * 180/M_PI, 0, 1, 0);
      glRotatef (gp->orient.o * 180/M_PI, 0, 0, 1);
      glScalef (1.02, 1.02, 1.02);
      glDisable (GL_TEXTURE_2D);
      glDisable (GL_LIGHTING);
      glDisable (GL_LINE_SMOOTH);
      glColor3f (0.1, 0.3, 0.1);
      glCallList (gp->tzlist);
      mi->polygon_count += gp->tzpoints;
      glPopMatrix();
      if (!wire && !do_texture)
        glEnable (GL_LIGHTING);
      if (gp->draw_axis) gp->draw_axis--;
    }

# define DEG "\xC2\xB0"  /* ° */
  if (gp->draw_compass)
    {
      XCharStruct cs;

      glPushMatrix();
      glRotatef (current_device_rotation(), 1, 0, 0);
      glTranslatef (wander.x, wander.y, wander.z);
      glScalef (planet_scale, planet_scale, planet_scale);
      glRotatef (gp->orient.a * 180/M_PI, 0, 1, 0);
      glRotatef (gp->orient.o * 180/M_PI, 0, 0, 1);

      /* Draw longitude around the equator. */
      glColor3f (1, 1, 0);
      for (i = 0; i < 360; i += 15)
        {
          GLfloat s = 1.0 / 500;
          char buf[100];
          int j = 360 - i;
          if (j == 360) j = 0;
          if (j == 0 || j == 180)
            sprintf (buf, "%d" DEG, j);
          else if (j <= 180)
            sprintf (buf, "%d" DEG " W", j);
          else
            sprintf (buf, "%d" DEG " E", 360 - j);
    
          glPushMatrix();
          glRotatef (i, 0, 0, 1);
          glTranslatef (1.1, 0, 0);
          glRotatef (90, 0, 1, 0);
          glRotatef (-90, 0, 0, 1);
          glScalef (s, s, s);
          glEnable (GL_CULL_FACE);
          texture_string_metrics (gp->title_font, buf, &cs, 0, 0);
          glTranslatef (-cs.width / 2, -cs.ascent / 2, 0);
          print_texture_string (gp->title_font, buf);
          mi->polygon_count++;
          glPopMatrix();
        }
    
      /* Draw four latitude indicators. */
      for (i = 0; i < 4; i++)
        {
          int j;
          glPushMatrix();
          glRotatef (i * 90, 0, 0, 1);
          for (j = 0; j <= 180; j += 15)
            {
              GLfloat s = 1.0 / 400;
              char buf[100];
              int k = 90 - j;
        
              if (k == 0 ||
                  (i > 0 && (k == 90 || k == -90)))
                continue;
    
              if (k >= 0)
                sprintf (buf, "%d" DEG " N", k);
              else
                sprintf (buf, "%d" DEG " S", -k);
        
              glPushMatrix();
              glRotatef (-90, 0, 0, 1);
              glRotatef (90, 0, 1, 0);
              glRotatef (j, 0, 0, 1);
              glTranslatef (1.1, 0, 0);
              glRotatef (90, 0, 1, 0);
              glRotatef (180, 0, 0, 1);

              glScalef (s, s, s);
              glEnable (GL_CULL_FACE);
              texture_string_metrics (gp->title_font, buf, &cs, 0, 0);
              glTranslatef (-cs.width / 2, -cs.ascent / 2, 0);
              print_texture_string (gp->title_font, buf);
              mi->polygon_count++;
              glPopMatrix();
            }
          glPopMatrix();
        }
      glPopMatrix();
      if (gp->draw_compass) gp->draw_compass--;
    }

  /* Display the top left label of the part of the globe we are looking at.
   */
  {
    char buf[1000];
    GLfloat lat_angle = -gp->orient.a * 180/M_PI;
    GLfloat lon_angle =  gp->orient.o * 180/M_PI;
    GLfloat lat = lat_angle > 0 ? lat_angle : -lat_angle;
    GLfloat lon = lon_angle > 0 ? lon_angle : -lon_angle;

    sprintf (buf,      /* 37° N, 122° W */
             "%d" DEG " %c, "
             "%d" DEG " %c"
             "\n%d%%",
             (int) lat,
             (((int) (lat_angle * 180/M_PI)) >= 0 ? 'N' : 'S'),
             (int) lon,
             (((int) (lon_angle * 180/M_PI)) >= 0 ? 'W' : 'E'),
             100 * gp->country_idx / all_countries.count
             );

    glColor3f (1, 1, 0);
    print_texture_label (MI_DISPLAY(mi), gp->title_font,
                         MI_WIDTH(mi), MI_HEIGHT(mi),
                         1, buf);
  }

  glPopMatrix();

  glColor3f (1, 1, 1);
  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);

  tick_planet (mi);
}


ENTRYPOINT void
free_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int i;

  if (!gp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), gp->window, *gp->glx_context);

  if (glIsList(gp->platelist))   glDeleteLists(gp->platelist, 1);
  if (glIsList(gp->latlonglist)) glDeleteLists(gp->latlonglist, 1);
  if (glIsList(gp->tzlist))      glDeleteLists(gp->tzlist, 1);
  if (glIsList(gp->starlist))    glDeleteLists(gp->starlist, 1);
  glDeleteTextures(1, &gp->tex1);
  glDeleteTextures(1, &gp->tex2);

  for (i = 0; i < all_countries.count; i++)
    {
      country_dlist *c = &gp->countries[i];
      if (glIsList(c->mesh_list)) glDeleteLists (c->mesh_list, 1);
      if (glIsList(c->edge_list)) glDeleteLists (c->edge_list, 1);
    }
  free (gp->countries);

  if (gp->title_font) free_texture_font (gp->title_font);
  if (gp->label_font) free_texture_font (gp->label_font);
  if (gp->trackball) gltrackball_free (gp->trackball);
  if (gp->rot) free_rotator (gp->rot);
  if (gp->rot2) free_rotator (gp->rot2);
  if (gp->country_order) free (gp->country_order);
  if (gp->label) free (gp->label);
}


XSCREENSAVER_MODULE_2 ("WorldPieces", worldpieces, planet)

#endif
