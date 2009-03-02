/* xscreensaver, Copyright (c) 1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Animates Lemarchand's Box, the Lament Configuration.  By jwz, 25-Jul-98.

   TODO:

     *  The "gold" color isn't quite right; it looks more like "yellow" than
        "gold" to me.

     *  For some reason, the interior surfaces are shinier than the exterior
        surfaces.  I don't understand why, but this should be remedied.

     *  Perhaps use a slightly-bumpy or oily texture for the interior surfaces?

     *  Some of the edges don't line up perfectly (since the images are not
        perfectly symetrical.)  Something should be done about this; either
        making the edges overlap slightly (instead of leaving gaps) or fixing
        the images so that the edges may be symmetrical.

     *  I want the gold leaf to seem to be raised up from the surface, but I
        think this isn't possible with OpenGL.  Supposedly, OpenGL only 
        supports Gouraud shading (interpolating edge normals from face normals,
        and shading smoothly) but bump-maps only work with Phong shading
        (computing a normal for each rendered pixel.)

     *  As far as I can tell, OpenGL doesn't do shadows.  As a result, the
        forward-facing interior walls are drawn bright, not dark.  If it was
        casting shadows properly, it wouldn't matter so much that the edges
        don't quite line up, because the lines would be black, and thus not
        visible.  But the edges don't match up, and so the bright interior
        faces show through, and that sucks.

	But apparently there are tricky ways around this:
	http://reality.sgi.com/opengl/tips/rts/
	I think these techniques require GLUT, however, which isn't 
	(currently) required by any other xscreensaver hacks.

     *  There should be strange lighting effects playing across the surface:
        electric sparks, or little glittery blobs of light.  
        http://reality.sgi.com/opengl/tips/lensflare/ might provide guidance.

     *  Need to add some more modes, to effect the transition from the cube
        shapes to the "spike" or "leviathan" shapes.  I have extensive notes
        on how these transformations occur, but unfortunately, due to camera
        trickery, the transitions require dematerializations which do not
        preserve object volume.  But I suppose that's allowed, in
        non-Euclidian or hyperdimensional spaces (since the extra mass could
        simply be rotated along the axis to which one cannot point.)

        The other hard thing about this is that the "leviathan" shapes contain
        a much larger number of facets, and I modelled this whole thing by 
        hand, since I don't have any 3d-object-editing tools that I know how
        to use (or that look like they would take any less than several months
        to become even marginally proficient with...)

     *  Perhaps there should be a table top, on which it casts a shadow?
        And then multiple light sources (for multiple shadows)?

     *  Needs music.  ("Hellraiser Themes" by Coil: TORSO CD161; also
        duplicated on the "Unnatural History 2" compilation, WORLN M04699.)
 */

#include <X11/Intrinsic.h>

#define PROGCLASS	"Lament"
#define HACK_INIT	init_lament
#define HACK_DRAW	draw_lament
#define HACK_RESHAPE	reshape_lament
#define HACK_HANDLE_EVENT lament_handle_event
#define EVENT_MASK	PointerMotionMask
#define lament_opts	xlockmore_opts
#define DEFAULTS	"*delay:	10000   \n"	\
			"*showFPS:      False   \n"     \
			"*wireframe:	False	\n"	\
			"*texture:	True	\n"
#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEF_TEXTURE "True"

static int do_texture;
static XrmOptionDescRec opts[] = {
  {"-texture", ".lament.texture", XrmoptionNoArg, (caddr_t) "true" },
  {"+texture", ".lament.texture", XrmoptionNoArg, (caddr_t) "false" },
};

static argtype vars[] = {
  {(caddr_t *) &do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
};

ModeSpecOpt lament_opts = {countof(opts), opts, countof(vars), vars, NULL};

#include "xpm-ximage.h"
#include "rotator.h"
#include "gltrackball.h"
#include "../images/lament.xpm"

#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

typedef enum {
  LAMENT_BOX,

  LAMENT_STAR_OUT,
  LAMENT_STAR_ROT,
  LAMENT_STAR_ROT_IN,
  LAMENT_STAR_ROT_OUT,
  LAMENT_STAR_UNROT,
  LAMENT_STAR_IN,

  LAMENT_TETRA_UNE,
  LAMENT_TETRA_USW,
  LAMENT_TETRA_DWN,
  LAMENT_TETRA_DSE,

  LAMENT_LID_OPEN,
  LAMENT_LID_CLOSE,
  LAMENT_LID_ZOOM,

  LAMENT_TASER_OUT,
  LAMENT_TASER_SLIDE,
  LAMENT_TASER_SLIDE_IN,
  LAMENT_TASER_IN

} lament_type;

static GLfloat exterior_color[] = { 0.33, 0.22, 0.03, 1.00,  /* ambient    */
                                    0.78, 0.57, 0.11, 1.00,  /* specular   */
                                    0.99, 0.91, 0.81, 1.00,  /* diffuse    */
                                   27.80                     /* shininess  */
                                  };
static GLfloat interior_color[] = { 0.20, 0.20, 0.15, 1.00,  /* ambient    */
                                    0.40, 0.40, 0.32, 1.00,  /* specular   */
                                    0.99, 0.99, 0.81, 1.00,  /* diffuse    */
                                   50.80                     /* shininess  */
                                  };


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  double rotx, roty, rotz;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint box;			   /* display list IDs */
  GLuint star1, star2;
  GLuint tetra_une, tetra_usw, tetra_dwn, tetra_dse, tetra_mid;
  GLuint lid_0, lid_1, lid_2, lid_3, lid_4;
  GLuint taser_base, taser_lifter, taser_slider;

  XImage *texture;		   /* image bits */
  GLuint texids[6];		   /* texture map IDs */
  lament_type type;		   /* which mode of the object is current */

  int anim_pause;		   /* countdown before animating again */
  GLfloat anim_r, anim_y, anim_z;  /* relative position during anims */

} lament_configuration;

static lament_configuration *lcs = NULL;

#define FACE_N 3
#define FACE_S 2
#define FACE_E 0
#define FACE_W 4
#define FACE_U 5
#define FACE_D 1

static void
parse_image_data(ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  lc->texture = xpm_to_ximage (mi->dpy,
			       mi->xgwa.visual,
			       mi->xgwa.colormap,
			       lament_faces);
}


typedef struct {
  double x,y,z;
} XYZ;

static void
normalize (XYZ *p)
{
  double length;
  length = sqrt (p->x * p->x +
                 p->y * p->y +
                 p->z * p->z);
  if (length != 0)
    {
      p->x /= length;
      p->y /= length;
      p->z /= length;
    }
  else
    {
      p->x = 0;
      p->y = 0;
      p->z = 0;
    }
}

/* Calculate the unit normal at p given two other points p1,p2 on the
   surface. The normal points in the direction of p1 crossproduct p2
 */
static XYZ
calc_normal (XYZ p, XYZ p1, XYZ p2)
{
  XYZ n, pa, pb;
  pa.x = p1.x - p.x;
  pa.y = p1.y - p.y;
  pa.z = p1.z - p.z;
  pb.x = p2.x - p.x;
  pb.y = p2.y - p.y;
  pb.z = p2.z - p.z;
  n.x = pa.y * pb.z - pa.z * pb.y;
  n.y = pa.z * pb.x - pa.x * pb.z;
  n.z = pa.x * pb.y - pa.y * pb.x;
  normalize (&n);
  return (n);
}


static void
do_normal(GLfloat x1, GLfloat y1, GLfloat z1,
	  GLfloat x2, GLfloat y2, GLfloat z2,
	  GLfloat x3, GLfloat y3, GLfloat z3)
{
  XYZ p1, p2, p3, p;
  p1.x = x1; p1.y = y1; p1.z = z1;
  p2.x = x2; p2.y = y2; p2.z = z2;
  p3.x = x3; p3.y = y3; p3.z = z3;

  p = calc_normal (p1, p2, p3);

  glNormal3f (p.x, p.y, p.z);

#ifdef DEBUG
  /* Draw a line in the direction of this face's normal. */
  {
    glPushMatrix();
    glTranslatef ((x1 + x2 + x3) / 3,
                  (y1 + y2 + y3) / 3,
                  (z1 + z2 + z3) / 3);
    glScalef (0.5, 0.5, 0.5);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0, 0, 0);
    glVertex3f(p.x, p.y, p.z);
    glEnd();
    glPopMatrix();
  }
#endif /* DEBUG */
}


/* Shorthand utilities for making faces, with proper normals.
 */

static void
set_colors (GLfloat *color)
{
  glMaterialfv(GL_FRONT, GL_AMBIENT, color+0);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, color+4);
  glMaterialfv(GL_FRONT, GL_SPECULAR, color+8);
  glMaterialfv(GL_FRONT, GL_SHININESS, color+12);
}

static void
face3(GLint texture, GLfloat *color, Bool wire,
      GLfloat s1, GLfloat t1, GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat s2, GLfloat t2, GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat s3, GLfloat t3, GLfloat x3, GLfloat y3, GLfloat z3)
{
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture(GL_TEXTURE_2D, texture);
#endif /* HAVE_GLBINDTEXTURE */
  set_colors(color);

  do_normal(x1, y1, z1,  x2, y2, z2,  x3, y3, z3);
  glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLES);
  glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
  glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
  glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
  glEnd();
}

static void
face4(GLint texture, GLfloat *color, Bool wire,
      GLfloat s1, GLfloat t1, GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat s2, GLfloat t2, GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat s3, GLfloat t3, GLfloat x3, GLfloat y3, GLfloat z3,
      GLfloat s4, GLfloat t4, GLfloat x4, GLfloat y4, GLfloat z4)
{
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture(GL_TEXTURE_2D, texture);
#endif /* HAVE_GLBINDTEXTURE */
  set_colors(color);
  do_normal(x1, y1, z1,  x2, y2, z2,  x3, y3, z3);
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
  glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
  glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
  glTexCoord2f(s4, t4); glVertex3f(x4, y4, z4);
  glEnd();
}

static void
face5(GLint texture, GLfloat *color, Bool wire,
      GLfloat s1, GLfloat t1, GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat s2, GLfloat t2, GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat s3, GLfloat t3, GLfloat x3, GLfloat y3, GLfloat z3,
      GLfloat s4, GLfloat t4, GLfloat x4, GLfloat y4, GLfloat z4,
      GLfloat s5, GLfloat t5, GLfloat x5, GLfloat y5, GLfloat z5)
{
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture(GL_TEXTURE_2D, texture);
#endif /* HAVE_GLBINDTEXTURE */
  set_colors(color);
  do_normal(x1, y1, z1,  x2, y2, z2,  x3, y3, z3);
  glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
  glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
  glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
  glTexCoord2f(s4, t4); glVertex3f(x4, y4, z4);
  glTexCoord2f(s5, t5); glVertex3f(x5, y5, z5);
  glEnd();
}



/* Creating object models
 */

static void
box(ModeInfo *mi, Bool wire)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];

  glNewList(lc->box, GL_COMPILE);
  glShadeModel(GL_SMOOTH);

  /* N */
  face4(lc->texids[FACE_N], exterior_color, wire,
	0.0, 0.0,	-0.5,  0.5,  0.5,
	1.0, 0.0,	 0.5,  0.5,  0.5,
	1.0, 1.0,	 0.5,  0.5, -0.5,
	0.0, 1.0,	-0.5,  0.5, -0.5);

  /* S */
  face4(lc->texids[FACE_S], exterior_color, wire,
	0.0, 0.0,	 -0.5, -0.5, -0.5,
	1.0, 0.0,	  0.5, -0.5, -0.5,
	1.0, 1.0,	  0.5, -0.5,  0.5,
	0.0, 1.0,	 -0.5, -0.5,  0.5);

  /* E */
  face4(lc->texids[FACE_E], exterior_color, wire,
	0.0, 0.0,	 0.5, -0.5, -0.5,
	1.0, 0.0,	 0.5,  0.5, -0.5,
	1.0, 1.0,	 0.5,  0.5,  0.5,
	0.0, 1.0,	 0.5, -0.5,  0.5);

  /* W */
  face4(lc->texids[FACE_W], exterior_color, wire,
	1.0, 1.0,	-0.5, -0.5,  0.5,
	0.0, 1.0,	-0.5,  0.5,  0.5,
	0.0, 0.0,	-0.5,  0.5, -0.5,
	1.0, 0.0,	-0.5, -0.5, -0.5);

  /* U */
  face4(lc->texids[FACE_U], exterior_color, wire,
	1.0, 0.0,	 0.5, -0.5,  0.5,
	1.0, 1.0,	 0.5,  0.5,  0.5,
	0.0, 1.0,	-0.5,  0.5,  0.5,
	0.0, 0.0,	-0.5, -0.5,  0.5);

  /* D */
  face4(lc->texids[FACE_D], exterior_color, wire,
	0.0, 1.0,	-0.5, -0.5, -0.5,
	0.0, 0.0,	-0.5,  0.5, -0.5,
	1.0, 0.0,	 0.5,  0.5, -0.5,
	1.0, 1.0,	 0.5, -0.5, -0.5);

  glEndList();
}


static void
star(ModeInfo *mi, Bool top, Bool wire)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  int i;

  int points[][2] = {
    {  77,  74 }, {  60,  98 }, {   0,  71 }, {   0,   0 },    /* L1 */
    {  60,  98 }, {  55, 127 }, {   0, 127 }, {   0,  71 },    /* L2 */
    {  55, 127 }, {  60, 154 }, {   0, 179 }, {   0, 127 },    /* L3 */
    {  60, 154 }, {  76, 176 }, {   0, 255 }, {   0, 179 },    /* L4 */
    {  76, 176 }, { 100, 193 }, {  74, 255 }, {   0, 255 },    /* B1 */
    { 100, 193 }, { 127, 198 }, { 127, 255 }, {  74, 255 },    /* B2 */
    { 127, 198 }, { 151, 193 }, { 180, 255 }, { 127, 255 },    /* B3 */
    { 151, 193 }, { 178, 177 }, { 255, 255 }, { 180, 255 },    /* B4 */
    { 178, 177 }, { 193, 155 }, { 255, 181 }, { 255, 255 },    /* R4 */
    { 193, 155 }, { 199, 127 }, { 255, 127 }, { 255, 181 },    /* R3 */
    { 199, 127 }, { 194,  99 }, { 255,  74 }, { 255, 127 },    /* R2 */
    { 194,  99 }, { 179,  76 }, { 255,   0 }, { 255,  74 },    /* R1 */
    { 179,  76 }, { 155,  60 }, { 180,   0 }, { 255,   0 },    /* T4 */
    { 155,  60 }, { 126,  55 }, { 126,   0 }, { 180,   0 },    /* T3 */
    { 126,  55 }, { 100,  60 }, {  75,   0 }, { 126,   0 },    /* T2 */
    { 100,  60 }, {  77,  74 }, {   0,   0 }, {  75,   0 },    /* T1 */
  };

  for (i = 0; i < countof(points); i++)
    points[i][1] = 255-points[i][1];

  if (top)
    glNewList(lc->star1, GL_COMPILE);
  else
    glNewList(lc->star2, GL_COMPILE);

  if (!top)
    glRotatef(-180.0, 1.0, 0.0, 0.0);

  for (i = 0; i < countof(points)/4; i += 2)
    {
      int j, k;

      /* Top face.
       */

      GLfloat s[4], t[4], x[4], y[4], z[4];
      for (j = 3, k = 0; j >= 0; j--, k++)
	{
	  GLfloat xx = points[(i*4)+j][0] / 255.0L;
	  GLfloat yy = points[(i*4)+j][1] / 255.0L;
	  s[k] = xx;
	  t[k] = yy;
	  x[k] = xx-0.5;
	  y[k] = yy-0.5;
	  z[k] = 0.5;
	}
      face4(lc->texids[top ? FACE_U : FACE_D], exterior_color, wire,
	    s[0], t[0],  x[0], y[0], z[0],
	    s[1], t[1],  x[1], y[1], z[1],
	    s[2], t[2],  x[2], y[2], z[2],
	    s[3], t[3],  x[3], y[3], z[3]);

      /* Bottom face.
       */
      for (j = 0, k = 0; j < 4; j++, k++)
	{
	  GLfloat xx = points[(i*4)+j][0] / 255.0L;
	  GLfloat yy = points[(i*4)+j][1] / 255.0L;
	  s[k] = xx;
	  t[k] = 1.0 - yy;
	  x[k] = xx-0.5;
	  y[k] = yy-0.5;
	  z[k] = -0.5;
	}
      face4(lc->texids[top ? FACE_U : FACE_D], exterior_color, wire,
	    s[0], t[0],  x[0], y[0], z[0],
	    s[1], t[1],  x[1], y[1], z[1],
	    s[2], t[2],  x[2], y[2], z[2],
	    s[3], t[3],  x[3], y[3], z[3]);

      /* Connecting faces.
       */
      for (j = 3; j >= 0; j--)
	{
	  int k = (j == 0 ? 3 : j-1);
	  Bool front_p = (j == 3);
	  GLfloat x1 = points[(i*4)+j][0] / 255.0L;
	  GLfloat y1 = points[(i*4)+j][1] / 255.0L;
	  GLfloat x2 = points[(i*4)+k][0] / 255.0L;
	  GLfloat y2 = points[(i*4)+k][1] / 255.0L;

	  GLfloat tx1=0.0, tx2=1.0, ty1=0.0, ty2=1.0;

	  int texture = 0;
	  int facing = i/4;
	  facing = (facing + j + 5) % 4;

	  switch (facing) {
	  case 0:
	    texture = FACE_W;
	    if (top) {
	      tx1 = 1.0 - y1;  tx2 = 1.0 - y2;
	      ty1 = 0.0;       ty2 = 1.0;
	    } else {
	      tx1 = y1;  tx2 = y2;
	      ty1 = 1.0; ty2 = 0.0;
	    }
	    break;
	  case 1:
	    texture = top ? FACE_S : FACE_N;
	    tx1 = x1;  tx2 = x2;
	    ty1 = 0.0; ty2 = 1.0;
	    break;
	  case 2:
	    texture = FACE_E;
	    if (top) {
	      tx1 = y1;  tx2 = y2;
	      ty1 = 0.0; ty2 = 1.0;
	    } else {
	      tx1 = 1.0 - y1;  tx2 = 1.0 - y2;
	      ty1 = 1.0;       ty2 = 0.0;
	    }
	    break;
	  case 3:
	    texture = top ? FACE_N : FACE_S;
	    tx1 = x1;  tx2 = x2;
	    ty1 = 1.0; ty2 = 0.0;
	    break;
	  }

	  x1 -= 0.5; x2 -= 0.5;
	  y1 -= 0.5; y2 -= 0.5;

	  face4(front_p ? lc->texids[texture] : 0,
		front_p ? exterior_color : interior_color,
		wire,
		tx1, ty2,  x1, y1,  0.5,
		tx1, ty1,  x1, y1, -0.5,
		tx2, ty1,  x2, y2, -0.5,
		tx2, ty2,  x2, y2,  0.5);
	}
    }


  /* Central core top cap.
   */
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture(GL_TEXTURE_2D, lc->texids[top ? FACE_U : FACE_D]);
#endif /* HAVE_GLBINDTEXTURE */
  set_colors(exterior_color);

  i = 1;
  do_normal(points[i+0][0], points[i+0][1], 0,
	    points[i+4][0], points[i+4][1], 0,
	    points[i+8][0], points[i+8][1], 0);
  glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  for (i = 1; i < countof(points); i += 4)
    {
      GLfloat x = points[i][0] / 255.0L;
      GLfloat y = points[i][1] / 255.0L;
      glTexCoord2f(x, y);
      glVertex3f(x-0.5, y-0.5, 0.5);
    }
  glEnd();


  /* Central core bottom cap.
   */
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture(GL_TEXTURE_2D, 0);
#endif /* HAVE_GLBINDTEXTURE */
  set_colors(interior_color);

  i = countof(points) - 9;
  do_normal(points[i+0][0], points[i+0][1], 0,
	    points[i+4][0], points[i+4][1], 0,
	    points[i+8][0], points[i+8][1], 0);

  glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  for (i = countof(points) - 3; i >= 0; i -= 4)
    {
      GLfloat x = points[i][0] / 255.0L;
      GLfloat y = points[i][1] / 255.0L;
      glVertex3f(x-0.5, y-0.5, 0);
    }
  glEnd();
  

  /* Central core walls.
   */
  for (i = 1; i < countof(points); i += 4)
    {

      GLfloat x1 = points[i-1][0] / 255.0L;
      GLfloat y1 = points[i-1][1] / 255.0L;
      GLfloat x2 = points[i][0] / 255.0L;
      GLfloat y2 = points[i][1] / 255.0L;
      face4(0, interior_color, wire,
	    0.0, 0.0,  x1-0.5, y1-0.5, 0.5,
	    0.0, 0.0,  x1-0.5, y1-0.5, 0.0,
	    0.0, 0.0,  x2-0.5, y2-0.5, 0.0,
	    0.0, 0.0,  x2-0.5, y2-0.5, 0.5);
    }

  glEndList();
}


static void
tetra(ModeInfo *mi, Bool wire)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];

  glNewList(lc->tetra_une, GL_COMPILE);
  {
    glShadeModel(GL_SMOOTH);

    /* Ua */
    face3(lc->texids[FACE_U], exterior_color, wire,
	  1.0, 0.0,	 0.5, -0.5,  0.5,
	  1.0, 1.0,	 0.5,  0.5,  0.5,
	  0.0, 1.0,	-0.5,  0.5,  0.5);

    /* Na */
    face3(lc->texids[FACE_N], exterior_color, wire,
	  0.0, 0.0,	-0.5,  0.5,  0.5,
	  1.0, 0.0,	 0.5,  0.5,  0.5,
	  1.0, 1.0,	 0.5,  0.5, -0.5);

    /* Eb */
    face3(lc->texids[FACE_E], exterior_color, wire,
	  1.0, 0.0,	 0.5,  0.5, -0.5,
	  1.0, 1.0,	 0.5,  0.5,  0.5,
	  0.0, 1.0,	 0.5, -0.5,  0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	 0.5, -0.5,  0.5,
	  0.0, 0.0,	-0.5,  0.5,  0.5);
  }
  glEndList();

  glNewList(lc->tetra_usw, GL_COMPILE);
  {
    /* Ub */
    face3(lc->texids[FACE_U], exterior_color, wire,
	  0.0, 1.0,	-0.5,  0.5,  0.5,
	  0.0, 0.0,	-0.5, -0.5,  0.5,
	  1.0, 0.0,	 0.5, -0.5,  0.5);

    /* Sb */
    face3(lc->texids[FACE_S], exterior_color, wire,
	  1.0, 1.0,	 0.5, -0.5,  0.5,
	  0.0, 1.0,	-0.5, -0.5,  0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5);

    /* Wb */
    face3(lc->texids[FACE_W], exterior_color, wire,
	  1.0, 0.0,	-0.5, -0.5, -0.5,
	  1.0, 1.0,	-0.5, -0.5,  0.5,
	  0.0, 1.0,	-0.5,  0.5,  0.5);

    face3(0, interior_color, wire,
	  0.0,0.0,	-0.5, -0.5, -0.5,
	  0.0,0.0,	-0.5,  0.5,  0.5,
	  0.0,0.0,	 0.5, -0.5,  0.5);
  }
  glEndList();

  glNewList(lc->tetra_dwn, GL_COMPILE);
  {
    /* Db */
    face3(lc->texids[FACE_D], exterior_color, wire,
	  0.0, 1.0,	-0.5, -0.5, -0.5,
	  0.0, 0.0,	-0.5,  0.5, -0.5,
	  1.0, 0.0,	 0.5,  0.5, -0.5);

    /* Wa */
    face3(lc->texids[FACE_W], exterior_color, wire,
	  0.0, 1.0,	-0.5,  0.5,  0.5,
	  0.0, 0.0,	-0.5,  0.5, -0.5,
	  1.0, 0.0,	-0.5, -0.5, -0.5);

    /* Nb */
    face3(lc->texids[FACE_N], exterior_color, wire,
	  1.0, 1.0,	 0.5,  0.5, -0.5,
	  0.0, 1.0,	-0.5,  0.5, -0.5,
	  0.0, 0.0,	-0.5,  0.5,  0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	-0.5,  0.5,  0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5);
  }
  glEndList();

  glNewList(lc->tetra_dse, GL_COMPILE);
  {
    /* Sa */
    face3(lc->texids[FACE_S], exterior_color, wire,
	  0.0, 0.0,	-0.5, -0.5, -0.5,
	  1.0, 0.0,	 0.5, -0.5, -0.5,
	  1.0, 1.0,	 0.5, -0.5,  0.5);

    /* Ea */
    face3(lc->texids[FACE_E], exterior_color, wire,
	  0.0, 1.0,	 0.5, -0.5,  0.5,
	  0.0, 0.0,	 0.5, -0.5, -0.5,
	  1.0, 0.0,	 0.5,  0.5, -0.5);

    /* Da */
    face3(lc->texids[FACE_D], exterior_color, wire,
	  1.0, 0.0,	 0.5,  0.5, -0.5,
	  1.0, 1.0,	 0.5, -0.5, -0.5,
	  0.0, 1.0,	-0.5, -0.5, -0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	 0.5, -0.5,  0.5,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5);
  }
  glEndList();

  glNewList(lc->tetra_mid, GL_COMPILE);
  {
    face3(0, interior_color, wire,
	  0.0, 0.0,	 0.5, -0.5,  0.5,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	-0.5,  0.5,  0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	-0.5,  0.5,  0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5,
	  0.0, 0.0,	 0.5, -0.5,  0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	-0.5,  0.5,  0.5,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	 0.5, -0.5,  0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5);

    face3(0, interior_color, wire,
	  0.0, 0.0,	 0.5, -0.5,  0.5,
	  0.0, 0.0,	 0.5,  0.5, -0.5,
	  0.0, 0.0,	-0.5, -0.5, -0.5);
  }
  glEndList();

}

static void
lid(ModeInfo *mi, Bool wire)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  int i;

  int points[][2] = {
    { 128,  20 },{  21, 129 },{   0, 129 },{   0,   0 },{ 128,   0 }, /* L1 */
    {  21, 129 },{ 127, 234 },{ 127, 255 },{   0, 255 },{   0, 129 }, /* L2 */
    { 127, 234 },{ 233, 127 },{ 255, 127 },{ 255, 255 },{ 127, 255 }, /* R2 */
    { 233, 127 },{ 128,  20 },{ 128,   0 },{ 255,   0 },{ 255, 127 }, /* R1 */
  };

  for (i = 0; i < countof(points); i++)
    points[i][1] = 255-points[i][1];

  glNewList(lc->lid_0, GL_COMPILE);
  glShadeModel(GL_SMOOTH);

  /* N */
  face4(lc->texids[FACE_N], exterior_color, wire,
	0.0, 0.0,	-0.5,  0.5,  0.5,
	1.0, 0.0,	 0.5,  0.5,  0.5,
	1.0, 1.0,	 0.5,  0.5, -0.5,
	0.0, 1.0,	-0.5,  0.5, -0.5);

  /* S */
  face4(lc->texids[FACE_S], exterior_color, wire,
	0.0, 0.0,	-0.5, -0.5, -0.5,
	1.0, 0.0,	 0.5, -0.5, -0.5,
	1.0, 1.0,	 0.5, -0.5,  0.5,
	0.0, 1.0,	-0.5, -0.5,  0.5);

  /* E */
  face4(lc->texids[FACE_E], exterior_color, wire,
	0.0, 0.0,	 0.5, -0.5, -0.5,
	1.0, 0.0,	 0.5,  0.5, -0.5,
	1.0, 1.0,	 0.5,  0.5,  0.5,
	0.0, 1.0,	 0.5, -0.5,  0.5);

  /* U */
  face4(lc->texids[FACE_U], exterior_color, wire,
	1.0, 0.0,	 0.5, -0.5,  0.5,
	1.0, 1.0,	 0.5,  0.5,  0.5,
	0.0, 1.0,	-0.5,  0.5,  0.5,
	0.0, 0.0,	-0.5, -0.5,  0.5);

  /* D */
  face4(lc->texids[FACE_D], exterior_color, wire,
	0.0, 1.0,	-0.5, -0.5, -0.5,
	0.0, 0.0,	-0.5,  0.5, -0.5,
	1.0, 0.0,	 0.5,  0.5, -0.5,
	1.0, 1.0,	 0.5, -0.5, -0.5);

  /* W -- lid_0 */
  for (i = 0; i < countof(points)/5; i++)
    {
      int j;
      GLfloat s[5], t[5], x[5], y[5], z[5];
      for (j = 0; j < 5; j++)
	{
	  GLfloat xx = points[(i*5)+j][0] / 255.0L;
	  GLfloat yy = points[(i*5)+j][1] / 255.0L;
	  s[j] = 1.0-xx;
	  t[j] = yy;
	  x[j] = -0.5;
	  y[j] = xx-0.5;
	  z[j] = yy-0.5;
	}
      face5(lc->texids[FACE_W], exterior_color, wire,
	    s[0], t[0],  x[0], y[0], z[0],
	    s[1], t[1],  x[1], y[1], z[1],
	    s[2], t[2],  x[2], y[2], z[2],
	    s[3], t[3],  x[3], y[3], z[3],
	    s[4], t[4],  x[4], y[4], z[4]);
    }
  glEndList();


  /* W -- lid_1 through lid_4 */
  for (i = 0; i < 4; i++)
    {
      GLfloat x1, y1, x2, y2, x3, y3;

      glNewList(lc->lid_1 + i, GL_COMPILE);
      glShadeModel(GL_SMOOTH);

      x1 = points[(i*5)+1][0] / 255.0L;
      y1 = points[(i*5)+1][1] / 255.0L;
      x2 = points[(i*5)][0] / 255.0L;
      y2 = points[(i*5)][1] / 255.0L;
      x3 = 0.5;
      y3 = 0.5;

      /* Outer surface */
      face3(lc->texids[FACE_W], exterior_color, wire,
	    1.0-x1, y1, 	-0.5, x1-0.5, y1-0.5,
	    1.0-x2, y2, 	-0.5, x2-0.5, y2-0.5,
	    1.0-x3, y3, 	-0.5, x3-0.5, y3-0.5);

      /* Inner surface */
      face3(0, interior_color, wire,
	    0.0, 0.0,	-0.48, x2-0.5, y2-0.5,
	    0.0, 0.0,	-0.48, x1-0.5, y1-0.5,
	    0.0, 0.0,	-0.48, x3-0.5, y3-0.5);

      /* Lip 1 */
      face4(0, interior_color, wire,
	    0.0, 0.0,	-0.5,  x1-0.5, y1-0.5,
	    0.0, 0.0,	-0.5,  x3-0.5, y3-0.5,
	    0.0, 0.0,	-0.48, x3-0.5, y3-0.5,
	    0.0, 0.0,	-0.48, x1-0.5, y1-0.5);

      /* Lip 2 */
      face4(0, interior_color, wire,
	    0.0, 0.0,	-0.48, x2-0.5, y2-0.5,
	    0.0, 0.0,	-0.48, x3-0.5, y3-0.5,
	    0.0, 0.0,	-0.5,  x3-0.5, y3-0.5,
	    0.0, 0.0,	-0.5,  x2-0.5, y2-0.5);

      glEndList();
    }
}

static void
taser(ModeInfo *mi, Bool wire)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  int i;

  int slider_face_points[][2] = {
    {  86,  58 },{  38, 106 },{  70, 106 },{ 118,  58 },{  -1,  -1 }, /* a */
    { 136,  58 },{ 184, 106 },{ 216, 106 },{ 168,  58 },{  -1,  -1 }, /* b */
    {  38, 106 },{   0, 144 },{   0, 190 },{  60, 190 },{ 108, 106 }, /* c */
    { 144, 106 },{ 194, 190 },{ 254, 190 },{ 254, 144 },{ 216, 106 }, /* d */
    {  98, 124 },{  60, 190 },{  92, 190 },{ 126, 158 },{ 126, 124 }, /* e */
    { 126, 124 },{ 126, 158 },{ 160, 190 },{ 194, 190 },{ 154, 124 }, /* f */
    {  22, 190 },{  22, 254 },{  60, 254 },{  60, 190 },{  -1,  -1 }, /* g */
    { 194, 190 },{ 194, 254 },{ 230, 254 },{ 230, 190 },{  -1,  -1 }, /* h */
    {  60, 190 },{  60, 210 },{  92, 210 },{  92, 190 },{  -1,  -1 }, /* i */
    { 160, 190 },{ 160, 210 },{ 194, 210 },{ 194, 190 },{  -1,  -1 }, /* j */
    { 110, 172 },{  92, 190 },{ 110, 190 },{  -1,  -1 },{  -1,  -1 }, /* k */
    { 140, 172 },{ 140, 190 },{ 160, 190 },{  -1,  -1 },{  -1,  -1 }, /* l */
    { 110, 172 },{ 140, 172 },{ 126, 156 },{  -1,  -1 },{  -1,  -1 }, /* m */
  };

  int body_face_points[][2] = {
    {   0,   0 },{   0,  58 },{ 254,  58 },{ 254,   0 },{  -1,  -1 }, /* A */
    {   0,  58 },{   0, 144 },{  86,  58 },{  -1,  -1 },{  -1,  -1 }, /* B */
    { 168,  58 },{ 254, 144 },{ 254,  58 },{  -1,  -1 },{  -1,  -1 }, /* C */
    { 118,  58 },{  70, 106 },{ 184, 106 },{ 136,  58 },{  -1,  -1 }, /* F */
    { 108, 106 },{  98, 124 },{ 154, 124 },{ 144, 106 },{  -1,  -1 }, /* G */
  };

  int lifter_face_points[][2] = {
    {   0, 190 },{   0, 254 },{  22, 254 },{  22, 190 },{  -1,  -1 }, /* D */
    { 230, 190 },{ 230, 254 },{ 254, 254 },{ 254, 190 },{  -1,  -1 }, /* E */
    {  60, 210 },{  60, 254 },{ 194, 254 },{ 194, 210 },{  -1,  -1 }, /* H */
    {  92, 190 },{  92, 210 },{ 160, 210 },{ 160, 190 },{  -1,  -1 }, /* I */
    { 110, 172 },{ 110, 190 },{ 140, 190 },{ 140, 172 },{  -1,  -1 }, /* J */
  };

  int body_perimiter_points[][2] = {
    {   0, 144 },{  86,  59 },{ 119,  58 },{  71, 107 },
    { 108, 107 },{  98, 124 },{ 155, 124 },{ 144, 107 },
    { 185, 106 },{ 136,  59 },{ 169,  59 },{ 255, 145 },
    { 255,   0 },{   0,   0 },
  };

  int slider_perimiter_points[][2] = {
    {  86,  58 },{   0,  144 },{   0, 190 },{  22,  190 },{  22, 254 },
    {  60, 254 },{  60,  210 },{  92, 210 },{  92,  190 },{ 110, 190 },
    { 110, 172 },{  140, 172 },{ 140, 190 },{ 160,  190 },{ 160, 210 },
    { 194, 210 },{  194, 254 },{ 230, 254 },{ 230,  190 },{ 254, 190 },
    { 254, 144 },{  168,  58 },{ 136,  58 },{ 184,  106 },{ 144, 106 },
    { 154, 124 },{  98,  124 },{ 108, 106 },{  70,  106 },{ 118,  58 },
  };

  int lifter_perimiter_points_1[][2] = {
    {   0, 189 },{   0, 254 },{  22, 255 },{  23, 190 },
  };

  int lifter_perimiter_points_2[][2] = {
    { 230, 254 },{ 255, 255 },{ 254, 190 },{ 230, 190 },
  };

  int lifter_perimiter_points_3[][2] = {
    {  60, 254 },{ 194, 254 },{ 194, 211 },{ 160, 210 },
    { 160, 190 },{ 140, 191 },{ 141, 172 },{ 111, 172 },
    { 110, 190 },{ 93, 190 },{ 92, 210 },{ 60, 211 },
  };

  for (i = 0; i < countof(slider_face_points); i++)
    slider_face_points[i][1] = 255-slider_face_points[i][1];
  for (i = 0; i < countof(body_face_points); i++)
    body_face_points[i][1] = 255-body_face_points[i][1];
  for (i = 0; i < countof(lifter_face_points); i++)
    lifter_face_points[i][1] = 255-lifter_face_points[i][1];
  for (i = 0; i < countof(body_perimiter_points); i++)
    body_perimiter_points[i][1] = 255-body_perimiter_points[i][1];
  for (i = 0; i < countof(slider_perimiter_points); i++)
    slider_perimiter_points[i][1] = 255-slider_perimiter_points[i][1];
  for (i = 0; i < countof(lifter_perimiter_points_1); i++)
    lifter_perimiter_points_1[i][1] = 255-lifter_perimiter_points_1[i][1];
  for (i = 0; i < countof(lifter_perimiter_points_2); i++)
    lifter_perimiter_points_2[i][1] = 255-lifter_perimiter_points_2[i][1];
  for (i = 0; i < countof(lifter_perimiter_points_3); i++)
    lifter_perimiter_points_3[i][1] = 255-lifter_perimiter_points_3[i][1];

  /* -------------------------------------------------------------------- */

  glNewList(lc->taser_base, GL_COMPILE);
  glShadeModel(GL_SMOOTH);

  /* N */
  face4(lc->texids[FACE_N], exterior_color, wire,
	0.0, 0.0,	-0.5,  0.5,  0.5,
	0.75, 0.0,	 0.25, 0.5,  0.5,
	0.75, 0.75,	 0.25, 0.5, -0.25,
	0.0, 0.75,	-0.5,  0.5, -0.25);

  /* S */
  face4(lc->texids[FACE_S], exterior_color, wire,
	0.0,  0.25,	-0.5,  -0.5, -0.25,
	0.75, 0.25,	 0.25, -0.5, -0.25,
	0.75, 1.0,	 0.25, -0.5,  0.5,
	0.0,  1.0,	-0.5,  -0.5,  0.5);

  /* interior E */
  face4(0, interior_color, wire,
	0.0, 0.0,	 0.25, -0.5, -0.25,
	1.0, 0.0,	 0.25,  0.5, -0.25,
	1.0, 1.0,	 0.25,  0.5,  0.5,
	0.0, 1.0,	 0.25, -0.5,  0.5);

  /* W */
  face4(lc->texids[FACE_W], exterior_color, wire,
	1.0, 1.0,	-0.5, -0.5,  0.5,
	0.0, 1.0,	-0.5,  0.5,  0.5,
	0.0, 0.25,	-0.5,  0.5, -0.25,
	1.0, 0.25,	-0.5, -0.5, -0.25);

  /* U */
  face4(lc->texids[FACE_U], exterior_color, wire,
	0.75, 0.0,	 0.25, -0.5,  0.5,
	0.75, 1.0,	 0.25,  0.5,  0.5,
	0.0, 1.0,	-0.5,   0.5,  0.5,
	0.0, 0.0,	-0.5,  -0.5,  0.5);

  /* interior D */
  face4(0, interior_color, wire,
	0.0, 1.0,	-0.5,  -0.5, -0.25,
	0.0, 0.0,	-0.5,   0.5, -0.25,
	1.0, 0.0,	 0.25,  0.5, -0.25,
	1.0, 1.0,	 0.25, -0.5, -0.25);

  /* Top face */
  for (i = 0; i < countof(body_face_points)/5; i++)
    {
      int j;
#ifdef HAVE_GLBINDTEXTURE
      glBindTexture(GL_TEXTURE_2D, lc->texids[FACE_E]);
#endif /* HAVE_GLBINDTEXTURE */
      set_colors(exterior_color);

      do_normal(0, body_face_points[(i*5)+0][0], body_face_points[(i*5)+0][1],
		0, body_face_points[(i*5)+1][0], body_face_points[(i*5)+1][1],
		0, body_face_points[(i*5)+2][0], body_face_points[(i*5)+2][1]
		);
      glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
      for (j = 0; j < 5; j++)
	{
	  int ix = body_face_points[(i*5)+j][0];
	  int iy = body_face_points[(i*5)+j][1];
	  GLfloat x, y;
	  if (ix == -1)  /* these are padding: ignore them */
	    continue;
	  x = ix / 255.0L;
	  y = iy / 255.0L;
	  glTexCoord2f(x, y);
	  glVertex3f(0.5, x-0.5, y-0.5);
	}
      glEnd();
    }

  /* Side walls */
  for (i = 0; i < countof(body_perimiter_points); i++)
    {
      int j = (i+1 >= countof(body_perimiter_points) ? 0 : i+1);
      GLfloat x1 = body_perimiter_points[i][0] / 255.0;
      GLfloat y1 = body_perimiter_points[i][1] / 255.0;
      GLfloat x2 = body_perimiter_points[j][0] / 255.0;
      GLfloat y2 = body_perimiter_points[j][1] / 255.0;
      int texture = -1;
      GLfloat s1=0, t1=0, s2=0, t2=0, s3=0, t3=0, s4=0, t4=0;

      if (i == 11)
	{
	  texture = lc->texids[FACE_N];
	  s1 = 1.0;  t1 = 0.0;
	  s2 = 1.0;  t2 = 0.568;
	  s3 = 0.75, t3 = 0.568;
	  s4 = 0.75; t4 = 0.0;
	}
      else if (i == 12)
	{
	  texture = lc->texids[FACE_U];
	  s1 = 1.0;  t1 = 0.0;
	  s2 = 1.0;  t2 = 1.0;
	  s3 = 0.75, t3 = 1.0;
	  s4 = 0.75; t4 = 0.0;
	}
      else if (i == 13)
	{
	  texture = lc->texids[FACE_S];
	  s1 = 1.0;  t1 = 0.437;
	  s2 = 1.0;  t2 = 1.0;
	  s3 = 0.75; t3 = 1.0;
	  s4 = 0.75; t4 = 0.437;
	}

      face4((texture == -1 ? 0 : texture),
	    (texture == -1 ? interior_color : exterior_color),
	    wire,
	    s1, t1,  0.5,  x2-0.5, y2-0.5,
	    s2, t2,  0.5,  x1-0.5, y1-0.5,
	    s3, t3,  0.25, x1-0.5, y1-0.5,
	    s4, t4,  0.25, x2-0.5, y2-0.5);
    }

  glEndList();

  /* -------------------------------------------------------------------- */

  glNewList(lc->taser_lifter, GL_COMPILE);
  glShadeModel(GL_SMOOTH);

  /* N */
  face4(lc->texids[FACE_N], exterior_color, wire,
	0.0,  0.75,	-0.5,  0.5, -0.25,
	0.75, 0.75,	 0.25, 0.5, -0.25,
	0.75, 1.0,	 0.25, 0.5, -0.5,
	0.0,  1.0,	-0.5,  0.5, -0.5);

  /* S */
  face4(lc->texids[FACE_S], exterior_color, wire,
	0.0,  0.0,	-0.5,  -0.5, -0.5,
	0.75, 0.0,	 0.25, -0.5, -0.5,
	0.75, 0.25,	 0.25, -0.5, -0.25,
	0.0,  0.25,	-0.5,  -0.5, -0.25);

  /* interior E */
  face4(0, interior_color, wire,
	0.0, 1.0,	 0.25, -0.5, -0.5,
	1.0, 1.0,	 0.25,  0.5, -0.5,
	1.0, 0.0,	 0.25,  0.5, -0.25,
	0.0, 0.0,	 0.25, -0.5, -0.25);

  /* W */
  face4(lc->texids[FACE_W], exterior_color, wire,
	1.0, 0.25,	-0.5, -0.5, -0.25,
	0.0, 0.25,	-0.5,  0.5, -0.25,
	0.0, 0.0,	-0.5,  0.5, -0.5,
	1.0, 0.0,	-0.5, -0.5, -0.5);

  /* interior U */
  face4(0, interior_color, wire,
	1.0, 0.0,	 0.25, -0.5,  -0.25,
	1.0, 1.0,	 0.25,  0.5,  -0.25,
	0.0, 1.0,	-0.5,   0.5,  -0.25,
	0.0, 0.0,	-0.5,  -0.5,  -0.25);

  /* D */
  face4(lc->texids[FACE_D], exterior_color, wire,
	0.0, 1.0,	-0.5, -0.5, -0.5,
	0.0, 0.0,	-0.5,  0.5, -0.5,
	0.75, 0.0,	 0.25,  0.5, -0.5,
	0.75, 1.0,	 0.25, -0.5, -0.5);


  /* Top face */
  for (i = 0; i < countof(lifter_face_points)/5; i++)
    {
      int j;

#ifdef HAVE_GLBINDTEXTURE
      glBindTexture(GL_TEXTURE_2D, lc->texids[FACE_E]);
#endif /* HAVE_GLBINDTEXTURE */
      set_colors(exterior_color);

      do_normal(
         0, lifter_face_points[(i*5)+0][0], lifter_face_points[(i*5)+0][1],
	 0, lifter_face_points[(i*5)+1][0], lifter_face_points[(i*5)+1][1],
	 0, lifter_face_points[(i*5)+2][0], lifter_face_points[(i*5)+2][1]);

      glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
      for (j = 0; j < 5; j++)
	{
	  int ix = lifter_face_points[(i*5)+j][0];
	  int iy = lifter_face_points[(i*5)+j][1];
	  GLfloat x, y;
	  if (ix == -1)  /* these are padding: ignore them */
	    continue;
	  x = ix / 255.0L;
	  y = iy / 255.0L;
	  glTexCoord2f(x, y);
	  glVertex3f(0.5, x-0.5, y-0.5);
	}
      glEnd();
    }

  /* Side walls */
  for (i = 0; i < countof(lifter_perimiter_points_1); i++)
    {
      int j = (i+1 >= countof(lifter_perimiter_points_1) ? 0 : i+1);
      GLfloat x1 = lifter_perimiter_points_1[i][0] / 255.0;
      GLfloat y1 = lifter_perimiter_points_1[i][1] / 255.0;
      GLfloat x2 = lifter_perimiter_points_1[j][0] / 255.0;
      GLfloat y2 = lifter_perimiter_points_1[j][1] / 255.0;
      int texture = -1;
      GLfloat s1=0, t1=0, s2=0, t2=0, s3=0, t3=0, s4=0, t4=0;

      if (i == 0)
	{
	  texture = lc->texids[FACE_S];
	  s1 = 1.0;  t1 = 0.0;
	  s2 = 1.0;  t2 = 0.26;
	  s3 = 0.75, t3 = 0.26;
	  s4 = 0.75; t4 = 0.0;
	}
      else if (i == 1)
	{
	  texture = lc->texids[FACE_D];
	  s1 = 1.0;  t1 = 0.914;
	  s2 = 1.0;  t2 = 1.0;
	  s3 = 0.75; t3 = 1.0;
	  s4 = 0.75; t4 = 0.914;
	}

      face4((texture == -1 ? 0 : texture),
	    (texture == -1 ? interior_color : exterior_color),
	    wire,
	    s1, t1,  0.5,  x2-0.5, y2-0.5,
	    s2, t2,  0.5,  x1-0.5, y1-0.5,
	    s3, t3,  0.25, x1-0.5, y1-0.5,
	    s4, t4,  0.25, x2-0.5, y2-0.5);
    }

  for (i = 0; i < countof(lifter_perimiter_points_2); i++)
    {
      int j = (i+1 >= countof(lifter_perimiter_points_2) ? 0 : i+1);
      GLfloat x1 = lifter_perimiter_points_2[i][0] / 255.0;
      GLfloat y1 = lifter_perimiter_points_2[i][1] / 255.0;
      GLfloat x2 = lifter_perimiter_points_2[j][0] / 255.0;
      GLfloat y2 = lifter_perimiter_points_2[j][1] / 255.0;
      int texture = -1;
      GLfloat s1=0, t1=0, s2=0, t2=0, s3=0, t3=0, s4=0, t4=0;

      if (i == 0)
	{
	  texture = lc->texids[FACE_D];
	  s1 = 1.0;  t1 = 0.0;
	  s2 = 1.0;  t2 = 0.095;
	  s3 = 0.75; t3 = 0.095;
	  s4 = 0.75; t4 = 0.0;
	}
      else if (i == 1)
	{
	  texture = lc->texids[FACE_N];
	  s1 = 1.0;  t1 = 0.745;
	  s2 = 1.0;  t2 = 1.0;
	  s3 = 0.75; t3 = 1.0;
	  s4 = 0.75; t4 = 0.745;
	}

      face4((texture == -1 ? 0 : texture),
	    (texture == -1 ? interior_color : exterior_color),
	    wire,
	    s1, t1,  0.5,  x2-0.5, y2-0.5,
	    s2, t2,  0.5,  x1-0.5, y1-0.5,
	    s3, t3,  0.25, x1-0.5, y1-0.5,
	    s4, t4,  0.25, x2-0.5, y2-0.5);
    }

  for (i = 0; i < countof(lifter_perimiter_points_3); i++)
    {
      int j = (i+1 >= countof(lifter_perimiter_points_3) ? 0 : i+1);
      GLfloat x1 = lifter_perimiter_points_3[i][0] / 255.0;
      GLfloat y1 = lifter_perimiter_points_3[i][1] / 255.0;
      GLfloat x2 = lifter_perimiter_points_3[j][0] / 255.0;
      GLfloat y2 = lifter_perimiter_points_3[j][1] / 255.0;
      int texture = -1;
      GLfloat s1=0, t1=0, s2=0, t2=0, s3=0, t3=0, s4=0, t4=0;

      if (i == 0)
	{
	  texture = lc->texids[FACE_D];
	  s1 = 1.0;  t1 = 0.235;
	  s2 = 1.0;  t2 = 0.765;
	  s3 = 0.75; t3 = 0.765;
	  s4 = 0.75; t4 = 0.235;
	}

      face4((texture == -1 ? 0 : texture),
	    (texture == -1 ? interior_color : exterior_color),
	    wire,
	    s1, t1,  0.5,  x2-0.5, y2-0.5,
	    s2, t2,  0.5,  x1-0.5, y1-0.5,
	    s3, t3,  0.25, x1-0.5, y1-0.5,
	    s4, t4,  0.25, x2-0.5, y2-0.5);
    }

  glEndList();

  /* -------------------------------------------------------------------- */

  glNewList(lc->taser_slider, GL_COMPILE);
  glShadeModel(GL_SMOOTH);

  /* Top face */
  for (i = 0; i < countof(slider_face_points)/5; i++)
    {
      int j;
#ifdef HAVE_GLBINDTEXTURE
      glBindTexture(GL_TEXTURE_2D, lc->texids[FACE_E]);
#endif /* HAVE_GLBINDTEXTURE */
      set_colors(exterior_color);

      do_normal(
	   0, slider_face_points[(i*5)+0][0], slider_face_points[(i*5)+0][1],
	   0, slider_face_points[(i*5)+1][0], slider_face_points[(i*5)+1][1],
	   0, slider_face_points[(i*5)+2][0], slider_face_points[(i*5)+2][1]);
      glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
      for (j = 0; j < 5; j++)
	{
	  int ix = slider_face_points[(i*5)+j][0];
	  int iy = slider_face_points[(i*5)+j][1];
	  GLfloat x, y;
	  if (ix == -1)  /* these are padding: ignore them */
	    continue;
	  x = ix / 255.0L;
	  y = iy / 255.0L;
	  glTexCoord2f(x, y);
	  glVertex3f(0.5, x-0.5, y-0.5);
	}
      glEnd();
    }

  /* Bottom face */
  for (i = countof(slider_face_points)/5 - 1; i >= 0; i--)
    {
      int j;
#ifdef HAVE_GLBINDTEXTURE
      glBindTexture(GL_TEXTURE_2D, 0);
#endif /* HAVE_GLBINDTEXTURE */
      set_colors(interior_color);

      do_normal(
	   0, slider_face_points[(i*5)+2][0], slider_face_points[(i*5)+2][1],
	   0, slider_face_points[(i*5)+1][0], slider_face_points[(i*5)+1][1],
	   0, slider_face_points[(i*5)+0][0], slider_face_points[(i*5)+0][1]);
      glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
      for (j = 4; j >= 0; j--)
	{
	  int ix = slider_face_points[(i*5)+j][0];
	  int iy = slider_face_points[(i*5)+j][1];
	  GLfloat x, y;
	  if (ix == -1)  /* these are padding: ignore them */
	    continue;
	  x = ix / 255.0L;
	  y = iy / 255.0L;
	  glTexCoord2f(x, y);
	  glVertex3f(0.25, x-0.5, y-0.5);
	}
      glEnd();
    }

  /* Side walls */
  for (i = 0; i < countof(slider_perimiter_points); i++)
    {
      int j = (i+1 >= countof(slider_perimiter_points) ? 0 : i+1);
      GLfloat x1 = slider_perimiter_points[i][0] / 255.0;
      GLfloat y1 = slider_perimiter_points[i][1] / 255.0;
      GLfloat x2 = slider_perimiter_points[j][0] / 255.0;
      GLfloat y2 = slider_perimiter_points[j][1] / 255.0;
      int texture = -1;
      GLfloat s1=0, t1=0, s2=0, t2=0, s3=0, t3=0, s4=0, t4=0;

      if (i == 1)
	{
	  texture = lc->texids[FACE_S];
	  s1 = 1.0;  t1 = 0.255;
	  s2 = 1.0;  t2 = 0.435;
	  s3 = 0.75; t3 = 0.435;
	  s4 = 0.75; t4 = 0.255;
	}
      else if (i == 4)
	{
	  texture = lc->texids[FACE_D];
	  s1 = 1.0;  t1 = 0.758;
	  s2 = 1.0;  t2 = 0.915;
	  s3 = 0.75; t3 = 0.915;
	  s4 = 0.75; t4 = 0.758;
	}
      else if (i == 16)
	{
	  texture = lc->texids[FACE_D];
	  s1 = 1.0;  t1 = 0.095;
	  s2 = 1.0;  t2 = 0.24;
	  s3 = 0.75; t3 = 0.24;
	  s4 = 0.75; t4 = 0.095;
	}
      else if (i == 19)
	{
	  texture = lc->texids[FACE_N];
	  s1 = 1.0;  t1 = 0.568;
	  s2 = 1.0;  t2 = 0.742;
	  s3 = 0.75; t3 = 0.742;
	  s4 = 0.75; t4 = 0.568;
	}

      face4((texture == -1 ? 0 : texture),
	    (texture == -1 ? interior_color : exterior_color),
	    wire,
	    s1, t1,  0.5,  x2-0.5, y2-0.5,
	    s2, t2,  0.5,  x1-0.5, y1-0.5,
	    s3, t3,  0.25, x1-0.5, y1-0.5,
	    s4, t4,  0.25, x2-0.5, y2-0.5);
    }

  glEndList();
}



/* Rendering and animating object models
 */

Bool
lament_handle_event (ModeInfo *mi, XEvent *event)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      lc->button_down_p = True;
      gltrackball_start (lc->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      lc->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           lc->button_down_p)
    {
      gltrackball_track (lc->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
draw(ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);

  if (!wire)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  else
    glClear(GL_COLOR_BUFFER_BIT);

  glPushMatrix();

  gltrackball_rotate (lc->trackball);

  /* Make into the screen be +Y right be +X, and up be +Z. */
  glRotatef(-90.0, 1.0, 0.0, 0.0);

  /* Scale it up. */
  glScalef(4.0, 4.0, 4.0);

#ifdef DEBUG
    glPushMatrix();
    {
      /* Shift to the upper left, and draw the vanilla box. */
      glTranslatef(-0.6, 0.0, 0.6);

      /* Apply rotation to the object. */
      glRotatef(x * 360, 1.0, 0.0, 0.0);
      glRotatef(y * 360, 0.0, 1.0, 0.0);
      glRotatef(z * 360, 0.0, 0.0, 1.0);

      /* Draw it. */
      glCallList(lc->box);
    }
    glPopMatrix();

    /* Shift to the lower right, and draw the animated object. */
    glTranslatef(0.6, 0.0, -0.6);
#endif /* DEBUG */


    glPushMatrix();
    {
      /* Apply rotation to the object. */
      if (lc->type != LAMENT_LID_ZOOM)
        get_rotation (lc->rot, &lc->rotx, &lc->roty, &lc->rotz,
                      !lc->button_down_p);
      glRotatef (lc->rotx * 360, 1.0, 0.0, 0.0);
      glRotatef (lc->roty * 360, 0.0, 1.0, 0.0);
      glRotatef (lc->rotz * 360, 0.0, 0.0, 1.0);

      switch (lc->type)
	{
	case LAMENT_BOX:
	  glCallList(lc->box);
	  break;

	case LAMENT_STAR_OUT:
	case LAMENT_STAR_ROT:
	case LAMENT_STAR_ROT_IN:
	case LAMENT_STAR_ROT_OUT:
	case LAMENT_STAR_UNROT:
	case LAMENT_STAR_IN:
	  glTranslatef(0.0, 0.0, lc->anim_z/2);
	  glRotatef(lc->anim_r/2, 0.0, 0.0, 1.0);
	  glCallList(lc->star1);

	  glTranslatef(0.0, 0.0, -lc->anim_z);
	  glRotatef(-lc->anim_r, 0.0, 0.0, 1.0);
	  glCallList(lc->star2);
	  break;

	case LAMENT_TETRA_UNE:
	case LAMENT_TETRA_USW:
	case LAMENT_TETRA_DWN:
	case LAMENT_TETRA_DSE:
	  {
	    int magic;
	    GLfloat x, y, z;
	    switch (lc->type) {
	    case LAMENT_TETRA_UNE: magic = lc->tetra_une;
	      x = 1.0; y = 1.0; z = 1.0; break;
	    case LAMENT_TETRA_USW: magic = lc->tetra_usw;
	      x = 1.0; y = 1.0; z = -1.0; break;
	    case LAMENT_TETRA_DWN: magic = lc->tetra_dwn;
	      x = 1.0; y = -1.0; z = 1.0; break;
	    case LAMENT_TETRA_DSE: magic = lc->tetra_dse;
	      x = -1.0; y = 1.0; z = 1.0; break;
	    default: abort(); break;
	    }
	    glCallList(lc->tetra_mid);
	    if (magic != lc->tetra_une) glCallList(lc->tetra_une);
	    if (magic != lc->tetra_usw) glCallList(lc->tetra_usw);
	    if (magic != lc->tetra_dwn) glCallList(lc->tetra_dwn);
	    if (magic != lc->tetra_dse) glCallList(lc->tetra_dse);
	    glRotatef(lc->anim_r, x, y, z);
	    glCallList(magic);
	  }
	  break;

	case LAMENT_LID_OPEN:
	case LAMENT_LID_CLOSE:
	case LAMENT_LID_ZOOM:
	  {
	    GLfloat d = 0.417;

	    glTranslatef(lc->anim_z, 0.0, 0.0);

	    glCallList(lc->lid_0);

	    glPushMatrix();
	      glTranslatef(-0.5, -d, 0.0);
	      glRotatef(-lc->anim_r, 0.0, -1.0, -1.0);
	      glTranslatef( 0.5,  d, 0.0);
	      glCallList(lc->lid_1);
	    glPopMatrix();
	    glPushMatrix();
	      glTranslatef(-0.5, -d, 0.0);
	      glRotatef( lc->anim_r, 0.0, -1.0,   1.0);
	      glTranslatef( 0.5,  d, 0.0);
	      glCallList(lc->lid_2);
	    glPopMatrix();
	    glPushMatrix();
	      glTranslatef(-0.5,  d, 0.0);
	      glRotatef( lc->anim_r, 0.0,  -1.0,  -1.0);
	      glTranslatef( 0.5, -d, 0.0);
	      glCallList(lc->lid_3);
	    glPopMatrix();
	    glPushMatrix();
	      glTranslatef(-0.5,  d, 0.0);
	      glRotatef(-lc->anim_r, 0.0,  -1.0,   1.0);
	      glTranslatef( 0.5, -d, 0.0);
	      glCallList(lc->lid_4);
	    glPopMatrix();
	  }
	  break;

	case LAMENT_TASER_OUT:
	case LAMENT_TASER_SLIDE:
	case LAMENT_TASER_SLIDE_IN:
	case LAMENT_TASER_IN:

	  glTranslatef(-lc->anim_z/2, 0.0, 0.0);
	  glCallList(lc->taser_base);

	  glTranslatef(lc->anim_z, 0.0, 0.0);
	  glCallList(lc->taser_lifter);

	  glTranslatef(0.0, 0.0, lc->anim_y);
	  glCallList(lc->taser_slider);
	  break;

	default:
	  abort();
	  break;
	}
    }
    glPopMatrix();

  glPopMatrix();
}


static void
animate(ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  int pause = 10;
/*  int pause2 = 60;*/
  int pause3 = 120;

  switch (lc->type)
    {
    case LAMENT_BOX:
      {
	/* Rather than just picking states randomly, pick an ordering randomly,
	   do it, and then re-randomize.  That way one can be assured of seeing
	   all states in a short time period, though not always in the same
	   order (it's frustrating to see it pick the same state 5x in a row.)
	 */
	static lament_type states[] = {
	  LAMENT_STAR_OUT, LAMENT_STAR_OUT,
	  LAMENT_TETRA_UNE, LAMENT_TETRA_USW,
	  LAMENT_TETRA_DWN, LAMENT_TETRA_DSE,
	  LAMENT_LID_OPEN, LAMENT_LID_OPEN, LAMENT_LID_OPEN,
	  LAMENT_TASER_OUT, LAMENT_TASER_OUT,
	  LAMENT_BOX, LAMENT_BOX, LAMENT_BOX, LAMENT_BOX, LAMENT_BOX,
	  LAMENT_BOX, LAMENT_BOX, LAMENT_BOX, LAMENT_BOX, LAMENT_BOX,
	};
	static int state = countof(states);

	if (state < countof(states))
	  {
	    lc->type = states[state++];
	  }
	else
	  {
	    int i;
	    state = 0;
	    for (i = 0; i < countof(states); i++)
	      {
		int a = random() % countof(states);
		lament_type swap = states[a];
		states[a] = states[i];
		states[i] = swap;
	      }
	  }

	if (lc->type == LAMENT_BOX)
	  lc->anim_pause = pause3;

	lc->anim_r = 0.0;
	lc->anim_y = 0.0;
	lc->anim_z = 0.0;
      }
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_STAR_OUT:
      lc->anim_z += 0.01;
      if (lc->anim_z >= 1.0)
	{
	  lc->anim_z = 1.0;
	  lc->type = LAMENT_STAR_ROT;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_STAR_ROT:
      lc->anim_r += 1.0;
      if (lc->anim_r >= 45.0)
	{
	  lc->anim_r = 45.0;
	  lc->type = LAMENT_STAR_ROT_IN;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_STAR_ROT_IN:
      lc->anim_z -= 0.01;
      if (lc->anim_z <= 0.0)
	{
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_STAR_ROT_OUT;
	  lc->anim_pause = pause3 * (1 + (random() % 4) + (random() % 4));
	}
      break;

    case LAMENT_STAR_ROT_OUT:
      lc->anim_z += 0.01;
      if (lc->anim_z >= 1.0)
	{
	  lc->anim_z = 1.0;
	  lc->type = LAMENT_STAR_UNROT;
	  lc->anim_pause = pause;
	}
      break;
      
    case LAMENT_STAR_UNROT:
      lc->anim_r -= 1.0;
      if (lc->anim_r <= 0.0)
	{
	  lc->anim_r = 0.0;
	  lc->type = LAMENT_STAR_IN;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_STAR_IN:
      lc->anim_z -= 0.01;
      if (lc->anim_z <= 0.0)
	{
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause3;
	}
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_TETRA_UNE:
    case LAMENT_TETRA_USW:
    case LAMENT_TETRA_DWN:
    case LAMENT_TETRA_DSE:

      lc->anim_r += 1.0;
      if (lc->anim_r >= 360.0)
	{
	  lc->anim_r = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause3;
	}
      else if (lc->anim_r > 119.0 && lc->anim_r <= 120.0)
	{
	  lc->anim_r = 120.0;
	  lc->anim_pause = pause;
	}
      else if (lc->anim_r > 239.0 && lc->anim_r <= 240.0)
	{
	  lc->anim_r = 240.0;
	  lc->anim_pause = pause;
	}
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_LID_OPEN:
      lc->anim_r += 1.0;

      if (lc->anim_r >= 112.0)
	{
	  GLfloat hysteresis = 0.05;

	  lc->anim_r = 112.0;
	  lc->anim_z = 0.0;
	  lc->anim_pause = pause3;

	  if (lc->rotx >= -hysteresis &&
	      lc->rotx <=  hysteresis &&
	      ((lc->rotz >=  (0.25 - hysteresis) &&
		lc->rotz <=  (0.25 + hysteresis)) ||
	       (lc->rotz >= (-0.25 - hysteresis) &&
		lc->rotz <= (-0.25 + hysteresis))))
	    {
	      lc->type = LAMENT_LID_ZOOM;
	      lc->rotx = 0.00;
	      lc->rotz = (lc->rotz < 0 ? -0.25 : 0.25);
	    }
	  else
	    {
	      lc->type = LAMENT_LID_CLOSE;
	    }
	}
      break;

    case LAMENT_LID_CLOSE:
      lc->anim_r -= 1.0;
      if (lc->anim_r <= 0.0)
	{
	  lc->anim_r = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause3;
	}
      break;

    case LAMENT_LID_ZOOM:
      lc->anim_z -= 0.1;
      if (lc->anim_z < -50.0)
	{
	  lc->anim_r = 0.0;
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_BOX;
	}
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_TASER_OUT:
      lc->anim_z += 0.0025;
      if (lc->anim_z >= 0.25)
	{
	  lc->anim_z = 0.25;
	  lc->type = LAMENT_TASER_SLIDE;
	  lc->anim_pause = pause * (1 + (random() % 5) + (random() % 5));
	}
      break;

    case LAMENT_TASER_SLIDE:
      lc->anim_y += 0.0025;
      if (lc->anim_y >= 0.23)
	{
	  lc->anim_y = 0.23;
	  lc->type = LAMENT_TASER_SLIDE_IN;
	  lc->anim_pause = pause3 * (1 + (random() % 5) + (random() % 5));
	}
      break;

    case LAMENT_TASER_SLIDE_IN:
      lc->anim_y -= 0.0025;
      if (lc->anim_y <= 0.0)
	{
	  lc->anim_y = 0.0;
	  lc->type = LAMENT_TASER_IN;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_TASER_IN:
      lc->anim_z -= 0.0025;
      if (lc->anim_z <= 0.0)
	{
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause3;
	}
      break;

    default:
      abort();
      break;
    }
}



/* Window management, etc
 */

void
reshape_lament(ModeInfo *mi, int width, int height)
{
  int target_size = 180;
  int win_size = (width > height ? height : width);
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);

/*  glViewport(-600, -600, 1800, 1800); */

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);

  /* This scale makes the box take up most of the window */
  glScalef(2.0, 2.0, 2.0);

  /* But if the window is more than a little larger than our target size,
     scale the object back down, so that the bits drawn on the screen end
     up rougly target_size across (actually it ends up a little larger.)
     Note that the image-map bits we have are 128x128.  Therefore, if the
     image is magnified a lot, it looks pretty blocky.  So it's better to
     have a 128x128 animation on a 1280x1024 screen that looks good, than
     a 1024x1024 animation that looks really pixelated.
   */
  if (win_size > target_size * 1.5)
    {
      GLfloat ratio = ((GLfloat) target_size / (GLfloat) win_size);
      ratio *= 2.0;
      glScalef(ratio, ratio, ratio);
    }

  /* The depth buffer will be cleared, if needed, before the
   * next frame.  Right now we just want to black the screen.
   */
  glClear(GL_COLOR_BUFFER_BIT);
}


static void
gl_init(ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);

  if (wire)
    do_texture = False;

  if (!wire)
    {
      static GLfloat pos0[]  = { -4.0,  2.0, 5.0, 1.0 };
      static GLfloat pos1[]  = {  6.0, -1.0, 3.0, 1.0 };

      static GLfloat amb0[]  = { 0.7, 0.7, 0.7, 1.0 };
/*    static GLfloat amb1[]  = { 0.7, 0.0, 0.0, 1.0 }; */
      static GLfloat dif0[]  = { 1.0, 1.0, 1.0, 1.0 };
      static GLfloat dif1[]  = { 0.3, 0.1, 0.1, 1.0 };

      glLightfv(GL_LIGHT0, GL_POSITION, pos0);
      glLightfv(GL_LIGHT1, GL_POSITION, pos1);

      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb0);
/*    glLightfv(GL_LIGHT1, GL_AMBIENT,  amb1); */
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif0);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,  dif1);
      set_colors(exterior_color);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
/*    glEnable(GL_LIGHT1); */

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_NORMALIZE);
      glEnable(GL_CULL_FACE);
    }

  if (do_texture)
    {
#ifdef HAVE_GLBINDTEXTURE
      int i;
      for (i = 0; i < 6; i++)
	glGenTextures(1, &lc->texids[i]);

      parse_image_data(mi);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, lc->texture->width);

      for (i = 0; i < 6; i++)
	{
	  int height = lc->texture->width;	/* assume square */
	  glBindTexture(GL_TEXTURE_2D, lc->texids[i]);
	  set_colors(exterior_color);

          clear_gl_error();
	  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		       lc->texture->width, height, 0,
		       GL_RGBA,
                       /* GL_UNSIGNED_BYTE, */
                       GL_UNSIGNED_INT_8_8_8_8_REV,
		       (lc->texture->data +
			(lc->texture->bytes_per_line * height * i)));
          check_gl_error("texture");

	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	  glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	  glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	}

#else  /* !HAVE_GLBINDTEXTURE */
      fprintf(stderr,
	      "%s: this version of GL doesn't support multiple texture maps.\n"
	      "\tGet OpenGL 1.1.\n",
	      progname);
      exit (1);
#endif /* !HAVE_GLBINDTEXTURE */
    }

  lc->box = glGenLists(16);
  lc->star1 = lc->box+1;
  lc->star2 = lc->box+2;
  lc->tetra_une = lc->box+3;
  lc->tetra_usw = lc->box+4;
  lc->tetra_dwn = lc->box+5;
  lc->tetra_dse = lc->box+6;
  lc->tetra_mid = lc->box+7;
  lc->lid_0 = lc->box+8;
  lc->lid_1 = lc->box+9;
  lc->lid_2 = lc->box+10;
  lc->lid_3 = lc->box+11;
  lc->lid_4 = lc->box+12;
  lc->taser_base = lc->box+13;
  lc->taser_lifter = lc->box+14;
  lc->taser_slider = lc->box+15;

  box(mi, wire);
  star(mi, True, wire);
  star(mi, False, wire);
  tetra(mi, wire);
  lid(mi, wire);
  taser(mi, wire);
}


# ifdef HAVE_MESA_GL

# include <signal.h>

static RETSIGTYPE
lament_signal_kludge (int sig)
{
  signal (sig, SIG_DFL);
  fprintf (stderr,
           "\n"
           "%s: dying with signal %d (%s).\n"
           "\n"
           "\tThis is almost certainly a bug in the Mesa GL library,\n"
           "\tespecially if the stack trace in the core file mentions\n"
           "\t`lambda_textured_triangle' or `render_quad'.\n"
           "\n"
           "\tFirst make sure that you have the latest version of Mesa.\n"
           "\tIf that doesn't fix it, then I encourage you to report this\n"
           "\tbug to the Mesa maintainers at <http://www.mesa3d.org/>.\n"
           "\n",
           progname,
           sig,
           (sig == SIGILL ? "SIGILL" :
            sig == SIGFPE ? "SIGFPE" :
            sig == SIGBUS ? "SIGBUS" :
            sig == SIGSEGV ? "SIGSEGV" : "???"));
  fflush (stderr);
  kill (getpid (), sig);
}

static void
handle_signals (void)
{
  signal (SIGILL,  lament_signal_kludge);
  signal (SIGFPE,  lament_signal_kludge);
  signal (SIGBUS,  lament_signal_kludge);
  signal (SIGSEGV, lament_signal_kludge);
}
# endif /* HAVE_MESA_GL */


void
init_lament(ModeInfo *mi)
{
  lament_configuration *lc;
  if (!lcs)
    {
      lcs = (lament_configuration *)
	calloc(MI_NUM_SCREENS(mi), sizeof (lament_configuration));
      if (!lcs)
	{
	  fprintf(stderr, "%s: out of memory\n", progname);
	  exit(1);
	}
    }

  lc = &lcs[MI_SCREEN(mi)];

  {
    double rot_speed = 0.5;
    lc->rot = make_rotator (rot_speed, rot_speed, rot_speed, 1, 0, True);
    lc->trackball = gltrackball_init ();
  }

  lc->type = LAMENT_BOX;
  lc->anim_pause = 300 + (random() % 100);

  if ((lc->glx_context = init_GL(mi)) != NULL)
    {
      reshape_lament(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      gl_init(mi);
    }

# ifdef HAVE_MESA_GL
  handle_signals ();
# endif /* HAVE_MESA_GL */
}


void
draw_lament(ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!lc->glx_context)
    return;

  glDrawBuffer(GL_BACK);

  glXMakeCurrent(dpy, window, *(lc->glx_context));
  draw(mi);
  if (mi->fps_p) do_fps (mi);

  glFinish();
  glXSwapBuffers(dpy, window);

  if (lc->anim_pause)
    lc->anim_pause--;
  else
    animate(mi);
}

#endif /* USE_GL */
