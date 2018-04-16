/* xscreensaver, Copyright (c) 1998-2018 Jamie Zawinski <jwz@jwz.org>
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

     *  The gold leaf should appear to be raised up from the surface, but
        I think this isn't possible with OpenGL.  No bump maps.

     *  There should be strange lighting effects playing across the surface:
        electric sparks, or little glittery blobs of light.  Maybe like
        http://www.opengl.org/archives/resources/features/KilgardTechniques/
        LensFlare/

     *  Chains.

     *  Needs music.  ("Hellraiser Themes" by Coil: TORSO CD161; also
        duplicated on the "Unnatural History 2" compilation, WORLN M04699.)
 */

#define DEFAULTS	"*delay:	20000   \n"	\
			"*showFPS:      False   \n"     \
			"*wireframe:	False	\n"	\
			"*suppressRotationAnimation: True\n" \

# define free_lament 0
# define release_lament 0
#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#include "gllist.h"

/* #define DEBUG_MODE LAMENT_LEVIATHAN_COLLAPSE */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))
#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))

extern const struct gllist
  *lament_model_box,
  *lament_model_iso_base_a,
  *lament_model_iso_base_b,
  *lament_model_iso_den,
  *lament_model_iso_dse,
  *lament_model_iso_dwn,
  *lament_model_iso_swd,
  *lament_model_iso_une,
  *lament_model_iso_unw,
  *lament_model_iso_use,
  *lament_model_iso_usw,
  *lament_model_leviathan,
  *lament_model_lid_a,
  *lament_model_lid_b,
  *lament_model_lid_base,
  *lament_model_lid_c,
  *lament_model_lid_d,
  *lament_model_pillar_a,
  *lament_model_pillar_b,
  *lament_model_pillar_base,
  *lament_model_star_d,
  *lament_model_star_u,
  *lament_model_taser_a,
  *lament_model_taser_b,
  *lament_model_taser_base,
  *lament_model_tetra_base,
  *lament_model_tetra_dse,
  *lament_model_tetra_dwn,
  *lament_model_tetra_une,
  *lament_model_tetra_usw;

static const struct gllist * const *all_objs[] = {
  &lament_model_box,
  &lament_model_iso_base_a,
  &lament_model_iso_base_b,
  &lament_model_iso_den,
  &lament_model_iso_dse,
  &lament_model_iso_dwn,
  &lament_model_iso_swd,
  &lament_model_iso_une,
  &lament_model_iso_unw,
  &lament_model_iso_use,
  &lament_model_iso_usw,
  &lament_model_leviathan,
  &lament_model_lid_a,
  &lament_model_lid_b,
  &lament_model_lid_base,
  &lament_model_lid_c,
  &lament_model_lid_d,
  &lament_model_pillar_a,
  &lament_model_pillar_b,
  &lament_model_pillar_base,
  &lament_model_star_d,
  &lament_model_star_u,
  &lament_model_taser_a,
  &lament_model_taser_b,
  &lament_model_taser_base,
  &lament_model_tetra_base,
  &lament_model_tetra_dse,
  &lament_model_tetra_dwn,
  &lament_model_tetra_une,
  &lament_model_tetra_usw
};

typedef enum {	/* must be in the same order as in `all_objs'. */
  OBJ_BOX = 0,
  OBJ_ISO_BASE_A,
  OBJ_ISO_BASE_B,
  OBJ_ISO_DEN,
  OBJ_ISO_DSE,
  OBJ_ISO_DWN,
  OBJ_ISO_SWD,
  OBJ_ISO_UNE,
  OBJ_ISO_UNW,
  OBJ_ISO_USE,
  OBJ_ISO_USW,
  OBJ_LEVIATHAN,
  OBJ_LID_A,
  OBJ_LID_B,
  OBJ_LID_BASE,
  OBJ_LID_C,
  OBJ_LID_D,
  OBJ_PILLAR_A,
  OBJ_PILLAR_B,
  OBJ_PILLAR_BASE,
  OBJ_STAR_D,
  OBJ_STAR_U,
  OBJ_TASER_A,
  OBJ_TASER_B,
  OBJ_TASER_BASE,
  OBJ_TETRA_BASE,
  OBJ_TETRA_DSE,
  OBJ_TETRA_DWN,
  OBJ_TETRA_UNE,
  OBJ_TETRA_USW
} lament_obj_index;


#define DEF_TEXTURE "True"

static int do_texture;

static XrmOptionDescRec opts[] = {
  {"-texture", ".lament.texture", XrmoptionNoArg, "true" },
  {"+texture", ".lament.texture", XrmoptionNoArg, "false" },
};

static argtype vars[] = {
  {&do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
};

ENTRYPOINT ModeSpecOpt lament_opts = {countof(opts), opts, countof(vars), vars, NULL};

#include "ximage-loader.h"
#include "rotator.h"
#include "gltrackball.h"
#include "normals.h"

#include "images/gen/lament512_png.h"

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
  LAMENT_TASER_IN,

  LAMENT_PILLAR_OUT,
  LAMENT_PILLAR_SPIN,
  LAMENT_PILLAR_IN,

  LAMENT_SPHERE_OUT,
  LAMENT_SPHERE_IN,

  LAMENT_LEVIATHAN_SPIN,
  LAMENT_LEVIATHAN_FADE,
  LAMENT_LEVIATHAN_TWIST,
  LAMENT_LEVIATHAN_COLLAPSE,
  LAMENT_LEVIATHAN_EXPAND,
  LAMENT_LEVIATHAN_UNTWIST,
  LAMENT_LEVIATHAN_UNFADE,
  LAMENT_LEVIATHAN_UNSPIN,

} lament_type;

static const GLfloat exterior_color[] =
 { 0.33, 0.22, 0.03, 1.00,  /* ambient    */
   0.78, 0.57, 0.11, 1.00,  /* specular   */
   0.99, 0.91, 0.81, 1.00,  /* diffuse    */
   27.80                    /* shininess  */
 };
static const GLfloat interior_color[] =
 { 0.20, 0.20, 0.15, 1.00,  /* ambient    */
   0.40, 0.40, 0.32, 1.00,  /* specular   */
   0.99, 0.99, 0.81, 1.00,  /* diffuse    */
   50.80                    /* shininess  */
 };
static const GLfloat leviathan_color[] =
 { 0.30, 0.30, 0.30, 1.00,  /* ambient    */
   0.85, 0.85, 0.95, 1.00,  /* specular   */
   0.99, 0.99, 0.99, 1.00,  /* diffuse    */
   50.80                    /* shininess  */
 };
static const GLfloat black_color[] =
 { 0.05, 0.05, 0.05, 1.00,  /* ambient    */
   0.05, 0.05, 0.05, 1.00,  /* specular   */
   0.05, 0.05, 0.05, 1.00,  /* diffuse    */
   80.00                    /* shininess  */
 };


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  double rotx, roty, rotz;
  trackball_state *trackball;
  Bool button_down_p;
  Bool ffwdp;

  GLuint dlists[countof(all_objs)];
  GLuint polys[countof(all_objs)];

  XImage *texture;		   /* image bits */
  GLuint texids[8];		   /* texture map IDs */
  lament_type type;		   /* which mode of the object is current */

  int anim_pause;		   /* countdown before animating again */
  GLfloat anim_r, anim_y, anim_z;  /* relative position during anims */
  Bool facing_p;

  int state, nstates;
  lament_type *states;

} lament_configuration;

static lament_configuration *lcs = NULL;


static Bool
facing_screen_p (ModeInfo *mi)
{
  Bool facing_p;
  GLdouble m[16], p[16], x, y, z;
  GLint v[4];
  glGetDoublev (GL_MODELVIEW_MATRIX, m);
  glGetDoublev (GL_PROJECTION_MATRIX, p);
  glGetIntegerv (GL_VIEWPORT, v);
	
  /* See if a coordinate 5 units in front of the door is near the
     center of the screen. */
  gluProject (0, -5, 0, m, p, v, &x, &y, &z);
  x = (x / MI_WIDTH(mi))  - 0.5;
  y = (y / MI_HEIGHT(mi)) - 0.5;

  facing_p = (z < 0.9 &&
              x > -0.15 && x < 0.15 &&
              y > -0.15 && y < 0.15);

# ifdef DEBUG_MODE
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable (GL_LIGHTING);
  glColor3f (1, (facing_p ? 1 : 0), 0);
  glBegin (GL_LINES);
  glVertex3f (0, 0, 0);
  glVertex3f (0, -5, 0);
  glEnd();
  if (!MI_IS_WIREFRAME(mi)) glEnable (GL_LIGHTING);
# endif /* DEBUG_MODE */

  return facing_p;
}


static void
scale_for_window (ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];

  GLfloat target_size = 1.4 * (lc->texture ? lc->texture->width : 512);
  GLfloat size = MI_WIDTH(mi) < MI_HEIGHT(mi) ? MI_WIDTH(mi) : MI_HEIGHT(mi);
  GLfloat scale;

  /* Make it take up roughly the full width of the window. */
  scale = 20;

  /* But if the window is wider than tall, make it only take up the
     height of the window instead.
   */
  if (MI_WIDTH(mi) > MI_HEIGHT(mi))
    scale /= MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);

  /* If the window is super wide, make it bigger. */
  if (scale < 8) scale = 8;

  /* Constrain it to roughly life-sized on the screen, not huge.
   */
# ifdef HAVE_MOBILE
  if (size > 768)  /* iPad retina / iPhone 6 */
    target_size *= 1.5;
  else
# endif
    {
      GLfloat max = 500;  /* 3" on my screen... */

      if (MI_WIDTH(mi) > 2560) {  /* Retina displays */
        target_size *= 2.5;
        max *= 2.5;
      }

      if (target_size > max)
        target_size = max;
    }

  /* But if that would make the image larger than target_size, scale it
     back down again.  The image-map bits we have are 512x512, so if the
     image is magnified a lot, it looks pretty blocky.  It's better to
     have a 512x512 animation on a 1920x1080 screen that looks good
     than a 1024x1024 animation that looks really pixelated.
   */
  if (size > target_size)
    scale *= target_size / size;

  glScalef (scale, scale, scale);
}


static void
set_colors (const GLfloat *color)
{
  glMaterialfv(GL_FRONT, GL_AMBIENT,   color + 0);
  glMaterialfv(GL_FRONT, GL_DIFFUSE,   color + 4);
  glMaterialfv(GL_FRONT, GL_SPECULAR,  color + 8);
  glMaterialfv(GL_FRONT, GL_SHININESS, color + 12);
}

static void
set_colors_alpha (const GLfloat *color, GLfloat a)
{
  GLfloat c[countof(leviathan_color)];
  memcpy (c, color, sizeof(c));
  c[3] = c[7] = c[11] = a;
  set_colors (c);
}


static void
which_face (ModeInfo *mi, const GLfloat *f, int *face, int *outerp)
{
  GLfloat size = 3;          /* 3" square */
  const GLfloat *n = f;      /* normal */
  const GLfloat *v = f + 3;  /* vertex */
  GLfloat slack = 0.01;

  /* First look at the normal to determine which direction this triangle
     is facing (or is most-closely facing).
     It's an outer surface if it is within epsilon of the cube wall that
     it is facing.  Otherwise, it's an inner surface.
   */
  if      (n[1] < -0.5)   *face = 1, *outerp = v[1] < slack;       /* S */
  else if (n[2] >  0.5)   *face = 2, *outerp = v[2] > size-slack;  /* U */
  else if (n[1] >  0.5)   *face = 3, *outerp = v[1] > size-slack;  /* N */
  else if (n[2] < -0.5)   *face = 4, *outerp = v[2] < slack;       /* D */
  else if (n[0] < -0.5)   *face = 5, *outerp = v[0] < slack;       /* W */
  else /* (n[0] >  0.5)*/ *face = 6, *outerp = v[0] > size-slack;  /* E */

  /* Faces that don't have normals parallel to the axes aren't external. */
  if (*outerp &&
      (n[0] > -0.95 && n[0] < 0.95 &&
       n[1] > -0.95 && n[1] < 0.95 &&
       n[2] > -0.95 && n[2] < 0.95))
    *outerp = 0;
}


static void
texturize_vert (ModeInfo *mi, int which, const GLfloat *v)
{
  GLfloat size = 3;          /* 3" square */
  GLfloat s = 0, q = 0;

  /* Texture coordinates are surface coordinates,
     on the plane of this cube wall. */
  switch (which) {
  case 0: break;
  case 1: s = v[0], q = v[2]; break;
  case 2: s = v[0], q = v[1]; break;
  case 3: s = v[0], q = v[2]; q = size - q; break;
  case 4: s = v[0], q = v[1]; q = size - q; break;
  case 5: s = v[1], q = v[2]; break;
  case 6: s = v[1], q = v[2]; break;
  default: abort(); break;
  }

  glTexCoord2f (s / size, q / size);
}


static void
leviathan (ModeInfo *mi, GLfloat ratio, GLfloat alpha, Bool top_p)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat r = 0.34;
  GLfloat z = 2 * ratio;
  XYZ p[3];
  int i;

  GLfloat th = acos (2 / sqrt (6));  /* Line up with cube's diagonal */

  glPushMatrix();

  glRotatef (-45, 0, 1, 0);
  glRotatef (-th * 180 / M_PI, 0, 0, 1);

  if (!top_p)
    glRotatef (180, 0, 0, 1);

  for (i = 0; i < countof(p); i++)
    {
      GLfloat th = i * M_PI * 2 / countof(p);
      p[i].x = cos(th) * r;
      p[i].y = sin(th) * r;
    }

  glFrontFace (GL_CCW);
  for (i = 0; i < countof(p); i++)
    {
      int j = (i + 1) % countof(p);
/*      if (top_p)*/
        do_normal (z, 0, 0,
                   0, p[i].x, p[i].y,
                   0, p[j].x, p[j].y);
/*
      else
        do_normal (z, 0, 0,
                   0, p[j].y, p[j].z,
                   0, p[i].y, p[i].z);
*/

      if (do_texture)  /* Leviathan is the final texture */
        glBindTexture (GL_TEXTURE_2D, lc->texids[countof(lc->texids) - 1]);

      set_colors (leviathan_color);

      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glTexCoord2f (0.5, 1);
      glVertex3f (z, 0, 0);

      glTexCoord2f (0, 0);
      glVertex3f (0, p[i].x, p[i].y);

      glTexCoord2f (1, 0);
      glVertex3f (0, p[j].x, p[j].y);
      glEnd();
      mi->polygon_count++;

      /* Shield for fading */
      if (alpha < 0.9 && !wire)
        {
          GLfloat a = 0.35;
          GLfloat b = 0.69;

          set_colors_alpha (black_color, 1-alpha);
          glBindTexture (GL_TEXTURE_2D, 0);
          if (!wire)
            {
              glEnable (GL_BLEND);
              glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

          glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

          glVertex3f (z*a, p[j].x * b, p[j].y * b);
          glVertex3f (z*a, p[i].x * b, p[i].y * b);
          glVertex3f (0, p[i].x * 1.01, p[i].y * 1.01);
          glVertex3f (0, p[j].x * 1.01, p[j].y * 1.01);
          glEnd();
          mi->polygon_count++;
          glDisable (GL_BLEND);
        }
    }

  glPopMatrix();
}


static void
folding_walls (ModeInfo *mi, GLfloat ratio, Bool top_p)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  const GLfloat pa[4][2] = {{ -0.5,      -0.215833 },
                            {  0,         0.5      },
                            {  0.5,       0        },
                            { -0.215833, -0.5      }};
  const int tex[6] = { 0, 5, 1,  4, 2, 3 };
  const GLfloat top = -pa[0][1];
  GLfloat end_angle = 30.85;
  GLfloat rr = sin (ratio / 2 * M_PI);
  GLfloat offa = 0.15 * rr;
  GLfloat offb = 0.06 * rr;
  GLfloat p[4][3];
  GLfloat t[4][2];
  int i;

  glPushMatrix();

  if (top_p)
    {
      glRotatef (60, 1, -1, 1);
      glRotatef (180, 0, 1, 0);
      glRotatef (90, 1, 0, 0);
    }
  else
    {
      glRotatef (180, 1, 0, 0);
    }

  /* Scale down the points near the axis */

  p[0][0] = pa[0][0];
  p[0][1] = 0.5;
  p[0][2] = pa[0][1];

  p[1][0] = pa[1][0] - offb;
  p[1][1] = 0.5;
  p[1][2] = pa[1][1] - offa;

  p[2][0] = pa[2][0] - offa;
  p[2][1] = 0.5;
  p[2][2] = pa[2][1] - offb;

  p[3][0] = pa[3][0];
  p[3][1] = 0.5;
  p[3][2] = pa[3][1];

  if (!wire)
    {
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

  for (i = 0; i < 3; i++)
    {
      glPushMatrix();

      if (i == 1)
        {
          glRotatef (-90, 1, 0, 0);
          glRotatef (180, 1, 1, 0);
        }
      else if (i == 2)
        {
          glRotatef (-90, 1, 0, 0);
          glRotatef (180, 0, 1, 0);
          glRotatef (90, 0, 1, 0);
        }

      glRotatef (-90, 0, 1, 0);

      glTranslatef (-(top/2 + 0.25), 0.5, -(top/2 + 0.25));
      glRotatef (-45, 0, 1, 0);
      glRotatef (ratio * -end_angle, 0, 0, 1);
      glRotatef (45, 0, 1, 0);
      glTranslatef (top/2 + 0.25, -0.5, top/2 + 0.25);

      /* Get the texture coordinates right.
         This is hairy and incomprehensible. */

      t[0][0] = pa[0][1] + 0.5; t[0][1] = pa[0][0] + 0.5;
      t[1][0] = pa[1][1] + 0.5; t[1][1] = pa[1][0] + 0.5;
      t[2][0] = pa[2][1] + 0.5; t[2][1] = pa[2][0] + 0.5;
      t[3][0] = pa[3][1] + 0.5; t[3][1] = pa[3][0] + 0.5;

      if (i == 0 && !top_p)
        {
# define SWAP(A,B) A = 1-A, B = 1-B
          SWAP(t[0][0], t[0][1]);
          SWAP(t[1][0], t[1][1]);
          SWAP(t[2][0], t[2][1]);
          SWAP(t[3][0], t[3][1]);
# undef SWAP
        }
      else if (i == 0 && top_p)
        {
          GLfloat ot[4][3];
          memcpy (ot, t, sizeof(t));
# define SWAP(A,B) A = 1-A, B = 1-B
          SWAP(t[0][0], ot[2][1]);
          SWAP(t[1][0], ot[3][1]);
          SWAP(t[2][0], ot[0][1]);
          SWAP(t[3][0], ot[1][1]);
# undef SWAP
        }
      else if (i == 1)
        {
          GLfloat f;
# define SWAP(A,B) f = A, A = B, B = -f
          SWAP(t[0][0], t[0][1]);
          SWAP(t[1][0], t[1][1]);
          SWAP(t[2][0], t[2][1]);
          SWAP(t[3][0], t[3][1]);
# undef SWAP
        }

      set_colors_alpha (exterior_color, 1-ratio);
      glBindTexture (GL_TEXTURE_2D, lc->texids[tex[i + (top_p ? 3 : 0)]]);

      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      do_normal (p[0][0], p[0][1], p[0][2],
                 p[1][0], p[1][1], p[1][2],
                 p[2][0], p[2][1], p[2][2]);
      glTexCoord2fv(t[0]); glVertex3fv(p[0]);
      glTexCoord2fv(t[1]); glVertex3fv(p[1]);
      glTexCoord2fv(t[2]); glVertex3fv(p[2]);
      glTexCoord2fv(t[3]); glVertex3fv(p[3]);
      glEnd();
      mi->polygon_count++;

      /* The triangles between the quads */
# if 0
      /* #### There is a fucking gap between the two black triangles 
         that I can't figure out!  So instead of drawing the triangles,
         we build a black shield around the middle bit in leviathan()
         and count on back-face culling to have roughly the same effect.
       */
      if (!wire)
        {
          GLfloat pp[4][3];
          memcpy (pp, p, sizeof(pp));
          memcpy (pp[2], pp[1], sizeof(pp[1]));
          pp[2][0] -= 0.5 * (1-ratio);
          pp[2][1] -= 0.5 * (1-ratio);

          glBindTexture (GL_TEXTURE_2D, 0);
          set_colors_alpha (black_color, 1-ratio);

          glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLES);
          do_normal (pp[0][0], pp[0][1], pp[0][2],
                     pp[2][0], pp[2][1], pp[2][2],
                     pp[1][0], pp[1][1], pp[1][2]);
          glVertex3fv(pp[0]);
          glVertex3fv(pp[2]);
          glVertex3fv(pp[1]);
          glEnd();
          mi->polygon_count++;
        }
# endif

      glPopMatrix();
    }

  if (! wire) glDisable (GL_BLEND);

  glPopMatrix();
}


static int
lament_sphere (ModeInfo *mi, GLfloat ratio)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat size = 3; /* 3" square */
  int polys = 0;
  int facets = 16;  /* NxN grid on each face */
  int face;
  static const GLfloat norms[6][3] = {{ 0, -1, 0 }, {  0, 0, 1 }, { 0, 1, 0 },
                                      { 0, 0, -1 }, { -1, 0, 0 }, { 1, 0, 0 }};
  GLfloat s = 1.0 / facets;

  /* The ratio used for the normals: linger on the square normals. */
  GLfloat ratio2 = 1 - sin ((1 - ratio) / 2 * M_PI);
  GLfloat r1 = 1 - ratio2 / 2;
  GLfloat r2 =     ratio2 / 2;

  glPushMatrix();
  glTranslatef (-0.5, -0.5, -0.5);
  glScalef (1/size, 1/size, 1/size);

  set_colors (exterior_color);

  for (face = 0; face < 6; face++)
    {
      GLfloat x0, y0;
      for (y0 = 0; y0 < 1; y0 += s)
        for (x0 = 0; x0 < 1; x0 += s)
        {
          int i;
          GLfloat x1 = x0 + s;
          GLfloat y1 = y0 + s;
          GLfloat pa[4][3];	/* verts of the cube */
          GLfloat pb[4][3];	/* verts of the transition to the sphere */
          Bool frontp;
          GLfloat norm[4][3];	/* normals of the transitional verts */

          if (norms[face][0])
            frontp = norms[face][0] < 0,
            pa[0][1] = x0, pa[0][2] = y0, pa[0][0] = (frontp ? 0 : 1),
            pa[1][1] = x1, pa[1][2] = y0, pa[1][0] = pa[0][0],
            pa[2][1] = x1, pa[2][2] = y1, pa[2][0] = pa[0][0],
            pa[3][1] = x0, pa[3][2] = y1, pa[3][0] = pa[0][0];
          else if (norms[face][1])
            frontp = norms[face][1] > 0,
            pa[0][0] = x0, pa[0][2] = y0, pa[0][1] = (frontp ? 1 : 0),
            pa[1][0] = x1, pa[1][2] = y0, pa[1][1] = pa[0][1],
            pa[2][0] = x1, pa[2][2] = y1, pa[2][1] = pa[0][1],
            pa[3][0] = x0, pa[3][2] = y1, pa[3][1] = pa[0][1];
          else /* (norms[face][2]) */
            frontp = norms[face][2] < 0,
            pa[0][0] = x0, pa[0][1] = y0, pa[0][2] = (frontp ? 0 : 1),
            pa[1][0] = x1, pa[1][1] = y0, pa[1][2] = pa[0][2],
            pa[2][0] = x1, pa[2][1] = y1, pa[2][2] = pa[0][2],
            pa[3][0] = x0, pa[3][1] = y1, pa[3][2] = pa[0][2];

          for (i = 0; i < countof(pa); i++)
            pa[i][0] *= size, pa[i][1] *= size, pa[i][2] *= size;

          /* Convert square to sphere by treating as a normalized vector */
          for (i = 0; i < countof(pa); i++)
            {
              GLfloat x = (pa[i][0] / size) - 0.5;
              GLfloat y = (pa[i][1] / size) - 0.5;
              GLfloat z = (pa[i][2] / size) - 0.5;
              GLfloat d = sqrt (x*x + y*y + z*z) / 2;
              x = x/d + size/2;
              y = y/d + size/2;
              z = z/d + size/2;

              pb[i][0] = pa[i][0] + ((x - pa[i][0]) * ratio);
              pb[i][1] = pa[i][1] + ((y - pa[i][1]) * ratio);
              pb[i][2] = pa[i][2] + ((z - pa[i][2]) * ratio);
            }

          /* The normals of an intermediate point are the weighted average
             of the cube's orthogonal normals, and the sphere's radial
             normals: early in the sequence, the edges are sharp, but they
             soften as it expands. */
          {
            XYZ na, pa0, pa1, pa2;
            pa0.x = pa[0][0]; pa0.y = pa[0][1]; pa0.z = pa[0][2];
            pa1.x = pa[1][0]; pa1.y = pa[1][1]; pa1.z = pa[1][2];
            pa2.x = pa[2][0]; pa2.y = pa[2][1]; pa2.z = pa[2][2];
            na = calc_normal (pa0, pa1, pa2);

            for (i = 0; i < countof(pb); i++)
              {
                GLfloat d;
                XYZ nb;

                nb.x = pb[i][0];
                nb.y = pb[i][1];
                nb.z = pb[i][2];
                d = sqrt (nb.x*nb.x + nb.y*nb.y + nb.z*nb.z);  /* normalize */
                nb.x /= d;
                nb.y /= d;
                nb.z /= d;

                norm[i][0] = (na.x * r1) + (nb.x * r2);  /* weighted */
                norm[i][1] = (na.y * r1) + (nb.y * r2);
                norm[i][2] = (na.z * r1) + (nb.z * r2);
              }
          }

	  if (! wire)
            glBindTexture (GL_TEXTURE_2D, lc->texids[face]);

          glFrontFace (frontp ? GL_CW : GL_CCW);
          glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

          texturize_vert (mi, face+1, pa[0]);
          glNormal3fv (norm[0]);
          glVertex3fv (pb[0]);

          texturize_vert (mi, face+1, pa[1]);
          glNormal3fv (norm[1]);
          glVertex3fv (pb[1]);

          texturize_vert (mi, face+1, pa[2]);
          glNormal3fv (norm[2]);
          glVertex3fv (pb[2]);

          texturize_vert (mi, face+1, pa[3]);
          glNormal3fv (norm[3]);
          glVertex3fv (pb[3]);

          glEnd();
          polys++;
        }
    }

  glPopMatrix();

  return polys;
}


static void
draw (ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);

  mi->polygon_count = 0;

  if (!wire)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  else
    glClear(GL_COLOR_BUFFER_BIT);

  glPushMatrix();

  gltrackball_rotate (lc->trackball);

  /* Make into the screen be +Y, right be +X, and up be +Z. */
  glRotatef (-90.0, 1.0, 0.0, 0.0);

  scale_for_window (mi);

  /* Apply rotation to the object. */
  if (lc->type != LAMENT_LID_ZOOM)
    get_rotation (lc->rot, &lc->rotx, &lc->roty, &lc->rotz,
                  !lc->button_down_p);
# ifdef DEBUG_MODE
  lc->rotx = 0.18;
  lc->roty = 0.22;
  lc->rotx = lc->roty = 0;
  lc->rotz = 0;
# endif

  glRotatef (lc->rotx * 360, 1, 0, 0);
  glRotatef (lc->roty * 360, 0, 1, 0);
  glRotatef (lc->rotz * 360, 0, 0, 1);

  glScalef (0.5, 0.5, 0.5);

  switch (lc->type)
    {
    case LAMENT_BOX:
      glCallList (lc->dlists[OBJ_BOX]);
      mi->polygon_count += lc->polys[OBJ_BOX];
      break;

    case LAMENT_STAR_OUT:
    case LAMENT_STAR_ROT:
    case LAMENT_STAR_ROT_IN:
    case LAMENT_STAR_ROT_OUT:
    case LAMENT_STAR_UNROT:
    case LAMENT_STAR_IN:
      glTranslatef (0.0, 0.0, lc->anim_z/2);
      glRotatef (lc->anim_r/2, 0.0, 0.0, 1.0);
      glCallList (lc->dlists[OBJ_STAR_U]);
      mi->polygon_count += lc->polys[OBJ_STAR_U];

      glTranslatef (0.0, 0.0, -lc->anim_z);
      glRotatef (-lc->anim_r, 0.0, 0.0, 1.0);
      glCallList (lc->dlists[OBJ_STAR_D]);
      mi->polygon_count += lc->polys[OBJ_STAR_D];
      break;

    case LAMENT_TETRA_UNE:
    case LAMENT_TETRA_USW:
    case LAMENT_TETRA_DWN:
    case LAMENT_TETRA_DSE:
      {
        int magic;
        GLfloat x, y, z;
        switch (lc->type) {
        case LAMENT_TETRA_UNE: magic = OBJ_TETRA_UNE; x= 1; y= 1; z= 1; break;
        case LAMENT_TETRA_USW: magic = OBJ_TETRA_USW; x= 1; y= 1; z=-1; break;
        case LAMENT_TETRA_DWN: magic = OBJ_TETRA_DWN; x= 1; y=-1; z= 1; break;
        case LAMENT_TETRA_DSE: magic = OBJ_TETRA_DSE; x=-1; y= 1; z= 1; break;
        default: abort(); break;
        }
        glCallList(lc->dlists[OBJ_TETRA_BASE]);
        mi->polygon_count += lc->polys[OBJ_TETRA_BASE];
        if (magic != OBJ_TETRA_UNE) glCallList (lc->dlists[OBJ_TETRA_UNE]);
        if (magic != OBJ_TETRA_USW) glCallList (lc->dlists[OBJ_TETRA_USW]);
        if (magic != OBJ_TETRA_DWN) glCallList (lc->dlists[OBJ_TETRA_DWN]);
        if (magic != OBJ_TETRA_DSE) glCallList (lc->dlists[OBJ_TETRA_DSE]);
        glRotatef (lc->anim_r, x, y, z);
        glCallList (lc->dlists[magic]);
        mi->polygon_count += lc->polys[magic] * 3;
      }
      break;

    case LAMENT_LID_OPEN:
    case LAMENT_LID_CLOSE:
    case LAMENT_LID_ZOOM:
      {
        GLfloat d = 0.21582;
        int i;
        const int lists[4] = { OBJ_LID_A, OBJ_LID_B, OBJ_LID_C, OBJ_LID_D };

        lc->facing_p = facing_screen_p (mi);

        if (lc->anim_z < 0.5)
          glTranslatef (0, -30 * lc->anim_z, 0);    /* zoom */
        else
          glTranslatef (8 * (0.5 - (lc->anim_z - 0.5)), 0, 0);

        glCallList (lc->dlists[OBJ_LID_BASE]);
        mi->polygon_count += lc->polys[OBJ_LID_BASE];
        for (i = 0; i < countof(lists); i++)
          {
            glPushMatrix();
            glRotatef (90 * i, 0, 1, 0);
            glTranslatef (-d, -0.5, d);
            glRotatef (-45, 0, 1, 0);
            glRotatef (-lc->anim_r, 1, 0, 0);
            glRotatef (45, 0, 1, 0);
            glTranslatef (d, 0.5, -d);
            glRotatef (-90 * i, 0, 1, 0);
            glCallList (lc->dlists[lists[i]]);
            mi->polygon_count += lc->polys[lists[i]];
            glPopMatrix();
          }

# ifdef DEBUG_MODE
        if (lc->facing_p)
          {
            glColor3f(1, 0, 0);
            glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
            glVertex3f (-0.49, 0.49, -0.49);
            glVertex3f ( 0.49, 0.49, -0.49);
            glVertex3f ( 0.49, 0.49,  0.49);
            glVertex3f (-0.49, 0.49,  0.49);
            glEnd();
            mi->polygon_count++;
          }
# endif /* DEBUG_MODE */
      }
      break;

    case LAMENT_TASER_OUT:
    case LAMENT_TASER_SLIDE:
    case LAMENT_TASER_SLIDE_IN:
    case LAMENT_TASER_IN:

      glTranslatef (0, -lc->anim_z/2, 0);
      glCallList (lc->dlists[OBJ_TASER_BASE]);
      mi->polygon_count += lc->polys[OBJ_TASER_BASE];

      glTranslatef (0, lc->anim_z, 0);
      glCallList (lc->dlists[OBJ_TASER_A]);
      mi->polygon_count += lc->polys[OBJ_TASER_A];

      glTranslatef (lc->anim_y, 0, 0);
      glCallList (lc->dlists[OBJ_TASER_B]);
      mi->polygon_count += lc->polys[OBJ_TASER_B];
      break;

    case LAMENT_PILLAR_OUT:
    case LAMENT_PILLAR_SPIN:
    case LAMENT_PILLAR_IN:

      glCallList (lc->dlists[OBJ_PILLAR_BASE]);
      mi->polygon_count += lc->polys[OBJ_PILLAR_BASE];

      glPushMatrix();
      if (lc->anim_z == 1 || lc->anim_z == 3)
        {
          glRotatef (lc->anim_r, 0, 0, 1);
          glTranslatef (0, 0, lc->anim_y);
        }
      glCallList (lc->dlists[OBJ_PILLAR_A]);
      mi->polygon_count += lc->polys[OBJ_PILLAR_A];
      glPopMatrix();

      glPushMatrix();
      if (lc->anim_z == 2 || lc->anim_z == 3)
        {
          glRotatef (lc->anim_r, 0, 0, 1);
          glTranslatef (0, 0, -lc->anim_y);
        }
      glCallList (lc->dlists[OBJ_PILLAR_B]);
      mi->polygon_count += lc->polys[OBJ_PILLAR_B];
      glPopMatrix();
      break;

    case LAMENT_SPHERE_OUT:
    case LAMENT_SPHERE_IN:
      mi->polygon_count += lament_sphere (mi, lc->anim_y);
      break;

    case LAMENT_LEVIATHAN_SPIN:
    case LAMENT_LEVIATHAN_UNSPIN:
    case LAMENT_LEVIATHAN_FADE:
    case LAMENT_LEVIATHAN_UNFADE:
    case LAMENT_LEVIATHAN_TWIST:
    case LAMENT_LEVIATHAN_UNTWIST:
      {
        /* These normals are hard to compute, so I pulled them from the
           model. */
        const GLfloat axes[6][4] =
          {{ OBJ_ISO_UNE,  0.633994, 0.442836,   0.633994 },
           { OBJ_ISO_USW,  0.442836,  0.633994, -0.633994 },
           { OBJ_ISO_DSE, -0.633994,  0.633994,  0.442836 },
           { OBJ_ISO_SWD, -0.633994, -0.442836, -0.633994 },
           { OBJ_ISO_DEN, -0.442836, -0.633994,  0.633994 },
           { OBJ_ISO_UNW,  0.633994, -0.633994, -0.442836 }};
        int i;

        GLfloat s = (1 - lc->anim_z);
        GLfloat s2 = MAX (0, 360 - lc->anim_r) / 360.0;
        Bool blendp = 0;

        switch (lc->type) {
        case LAMENT_LEVIATHAN_SPIN: break;
        case LAMENT_LEVIATHAN_UNSPIN: s2 = 1 - s2; break;
        default: s2 = 0; blendp = 1; break;
        }

        if (wire) blendp = 0;

        s  = (s * 0.6) + 0.4;

        leviathan (mi, 1 - s2, 1, True);
        glCallList (lc->dlists[OBJ_ISO_BASE_A]);
        mi->polygon_count += lc->polys[OBJ_ISO_BASE_A];

        glPushMatrix();
        glScalef (s2, s2, s2);
        glCallList (lc->dlists[OBJ_ISO_USE]);
        mi->polygon_count += lc->polys[OBJ_ISO_USE];
        glPopMatrix();

        glPushMatrix();
        glRotatef (lc->anim_y, 1, -1, 1);
        glCallList (lc->dlists[OBJ_ISO_BASE_B]);
        mi->polygon_count += lc->polys[OBJ_ISO_BASE_B];
        leviathan (mi, 1 - s2, 1, False);
        glPopMatrix();

        if (blendp)
          {
# ifndef HAVE_JWZGLES /* no glBlendColor */
            glEnable (GL_BLEND);
            glBlendFunc (GL_CONSTANT_ALPHA, GL_SRC_ALPHA);
            glBlendColor (1, 1, 1, MAX(0, 1-(lc->anim_z * 3)));
# endif
          }

        for (i = 0; i < countof(axes); i++)
          {
            glPushMatrix();
            glRotatef (lc->anim_r, axes[i][1], axes[i][2], axes[i][3]);
            glScalef (s, s, s);
            glCallList (lc->dlists[(int) axes[i][0]]);
            mi->polygon_count += lc->polys[(int) axes[i][0]];
            glPopMatrix();
            if (i == 2)
              glRotatef (lc->anim_y, 1, -1, 1);
          }

        if (blendp) glDisable (GL_BLEND);

        glPushMatrix();
        glScalef (s2, s2, s2);
        glCallList (lc->dlists[OBJ_ISO_DWN]);
        mi->polygon_count += lc->polys[OBJ_ISO_DWN];
        glPopMatrix();
      }
      break;

    case LAMENT_LEVIATHAN_COLLAPSE:
    case LAMENT_LEVIATHAN_EXPAND:
      {
        glPushMatrix();
        leviathan (mi, 1, lc->anim_y, True);
        glRotatef (180, 1, -1, 1);
        leviathan (mi, 1, lc->anim_y, False);
        glPopMatrix();
        folding_walls (mi, lc->anim_y, True);
        folding_walls (mi, lc->anim_y, False);
      }
      break;

    default:
      abort();
      break;
    }

  glPopMatrix();
}


/* Rather than just picking states randomly, pick an ordering randomly, do it,
   and then re-randomize.  That way one can be assured of seeing all states in
   a short time period, though not always in the same order (it's frustrating
   to see it pick the same state 5x in a row.)  Though, that can still happen,
   since states are in the list multiple times as a way of giving them
   probabilities.
 */
static void
shuffle_states (lament_configuration *lc)
{
  int i;
  for (i = 0; i < lc->nstates; i++)
    {
      int a = random() % lc->nstates;
      lament_type swap = lc->states[a];
      lc->states[a] = lc->states[i];
      lc->states[i] = swap;
    }
}


static void
animate (ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  int pause = 10;
  int pause2 = 120;
  GLfloat speed = (lc->ffwdp ? 20 : 1);

  switch (lc->type)
    {
    case LAMENT_BOX:
      {
        lc->state++;
        if (lc->state >= lc->nstates)
          {
            shuffle_states (lc);
            lc->state = 0;
          }
        lc->type = lc->states[lc->state];

	if (lc->type == LAMENT_BOX)
	  lc->anim_pause = pause2;

	lc->anim_r = 0.0;
	lc->anim_y = 0.0;
	lc->anim_z = 0.0;
      }
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_STAR_OUT:
      lc->anim_z += 0.01 * speed;
      if (lc->anim_z >= 1.0)
	{
	  lc->anim_z = 1.0;
	  lc->type = LAMENT_STAR_ROT;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_STAR_ROT:
      lc->anim_r += 1.0 * speed;
      if (lc->anim_r >= 45.0)
	{
	  lc->anim_r = 45.0;
	  lc->type = LAMENT_STAR_ROT_IN;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_STAR_ROT_IN:
      lc->anim_z -= 0.01 * speed;
      if (lc->anim_z <= 0.0)
	{
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_STAR_ROT_OUT;
	  lc->anim_pause = pause2 * (1 + frand(2) + frand(2));
	}
      break;

    case LAMENT_STAR_ROT_OUT:
      lc->anim_z += 0.01 * speed;
      if (lc->anim_z >= 1.0)
	{
	  lc->anim_z = 1.0;
	  lc->type = LAMENT_STAR_UNROT;
	  lc->anim_pause = pause;
	}
      break;
      
    case LAMENT_STAR_UNROT:
      lc->anim_r -= 1.0 * speed;
      if (lc->anim_r <= 0.0)
	{
	  lc->anim_r = 0.0;
	  lc->type = LAMENT_STAR_IN;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_STAR_IN:
      lc->anim_z -= 0.01 * speed;
      if (lc->anim_z <= 0.0)
	{
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause2;
	}
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_TETRA_UNE:
    case LAMENT_TETRA_USW:
    case LAMENT_TETRA_DWN:
    case LAMENT_TETRA_DSE:

      lc->anim_r += 1.0 * speed;
      if (lc->anim_r >= 360.0)
	{
	  lc->anim_r = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause2;
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
      lc->anim_r += 1.0 * speed;

      if (lc->anim_r >= 112.0)
	{
	  lc->anim_r = 112.0;
	  lc->anim_z = 0.0;
	  lc->anim_pause = pause2;
          lc->type = (lc->facing_p ? LAMENT_LID_ZOOM : LAMENT_LID_CLOSE);
        }
      break;

    case LAMENT_LID_CLOSE:
      lc->anim_r -= 1.0 * speed;
      if (lc->anim_r <= 0.0)
	{
	  lc->anim_r = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause2;
	}
      break;

    case LAMENT_LID_ZOOM:
      lc->anim_z += 0.01 * speed;
      if (lc->anim_z > 1.0)
	{
	  lc->anim_r = 0.0;
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_BOX;
	}
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_TASER_OUT:
      lc->anim_z += 0.005 * speed;
      if (lc->anim_z >= 0.5)
	{
	  lc->anim_z = 0.5;
	  lc->type = LAMENT_TASER_SLIDE;
	  lc->anim_pause = pause * (1 + frand(5) + frand(5));
	}
      break;

    case LAMENT_TASER_SLIDE:
      lc->anim_y += 0.005 * speed;
      if (lc->anim_y >= 0.255)
	{
	  lc->anim_y = 0.255;
	  lc->type = LAMENT_TASER_SLIDE_IN;
	  lc->anim_pause = pause2 * (1 + frand(5) + frand(5));
	}
      break;

    case LAMENT_TASER_SLIDE_IN:
      lc->anim_y -= 0.0025 * speed;
      if (lc->anim_y <= 0.0)
	{
	  lc->anim_y = 0.0;
	  lc->type = LAMENT_TASER_IN;
	  lc->anim_pause = pause;
	}
      break;

    case LAMENT_TASER_IN:
      lc->anim_z -= 0.0025 * speed;
      if (lc->anim_z <= 0.0)
	{
	  lc->anim_z = 0.0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause2;
	}
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_PILLAR_OUT:

      if (lc->anim_y == 0)  /* mostly in */
        lc->anim_y += 0.005 * ((random() % 5) ? -1 : 1) * speed;
      else if (lc->anim_y > 0)
        lc->anim_y += 0.005 * speed;
      else
        lc->anim_y -= 0.001 * speed;

      if (lc->anim_z == 0)
        {
          int i = (random() % 7);   /* A, B or both */
          if      (i == 0) lc->anim_z = 3;
          else if (i < 5)  lc->anim_z = 2;
          else             lc->anim_z = 1;

          /* We can do quarter turns, because it's radially symmetrical. */
          lc->anim_r =  90.0 * (1 + frand(6)) * RANDSIGN();
        }
      if (lc->anim_y > 0.4)
        {
          lc->anim_y = 0.4;
          lc->type = LAMENT_PILLAR_SPIN;
          lc->anim_pause = pause;
        }
      else if (lc->anim_y < -0.03)
        {
          lc->anim_y = -0.03;
          lc->type = LAMENT_PILLAR_SPIN;
          lc->anim_pause = pause;
        }
      break;

    case LAMENT_PILLAR_SPIN:
      {
        Bool negp = (lc->anim_r < 0);
        lc->anim_r += (negp ? 1 : -1) * speed;
        if (negp ? lc->anim_r > 0 : lc->anim_r < 0)
          {
            lc->anim_r = 0;
            lc->type = LAMENT_PILLAR_IN;
          }
      }
      break;

    case LAMENT_PILLAR_IN:
      {
        Bool negp = (lc->anim_y < 0);
        lc->anim_y += (negp ? 1 : -1) * 0.005 * speed;
        if (negp ? lc->anim_y > 0 : lc->anim_y < 0)
          {
            lc->anim_y = 0;
            lc->anim_z = 0;
            lc->type = LAMENT_BOX;
            lc->anim_pause = pause;
          }
      }
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_SPHERE_OUT:
      {
        lc->anim_y += 0.01 * speed;
        if (lc->anim_y >= 1)
          {
            lc->anim_y = 1;
            lc->type = LAMENT_SPHERE_IN;
            lc->anim_pause = pause2 * (1 + frand(1) + frand(1));
          }
      }
      break;

    case LAMENT_SPHERE_IN:
      {
        lc->anim_y -= 0.01 * speed;
        if (lc->anim_y <= 0)
          {
            lc->anim_y = 0;
            lc->type = LAMENT_BOX;
            lc->anim_pause = pause;
          }
      }
      break;

      /* -------------------------------------------------------------- */

    case LAMENT_LEVIATHAN_SPIN:
      lc->anim_r += 3.5 * speed;
      if (lc->anim_r >= 360 * 3)
	{
	  lc->anim_r = 0;
	  lc->type = LAMENT_LEVIATHAN_FADE;
	  lc->anim_pause = 0;
	}
      break;

    case LAMENT_LEVIATHAN_FADE:
      lc->anim_z += 0.01 * speed;
      if (lc->anim_z >= 1)
	{
	  lc->anim_z = 1;
	  lc->type = LAMENT_LEVIATHAN_TWIST;
	  lc->anim_pause = 0;
	}
      break;

    case LAMENT_LEVIATHAN_TWIST:
      lc->anim_y += 2 * speed;
      lc->anim_z = 1;
      if (lc->anim_y >= 180)
	{
	  lc->anim_y = 0;
	  lc->type = LAMENT_LEVIATHAN_COLLAPSE;
	  lc->anim_pause = 0;
	}
      break;

    case LAMENT_LEVIATHAN_COLLAPSE:
      lc->anim_y += 0.01 * speed;
      if (lc->anim_y >= 1)
	{
	  lc->anim_y = 1.0;
	  lc->type = LAMENT_LEVIATHAN_EXPAND;
	  lc->anim_pause = pause2 * 4;
	}
      break;

    case LAMENT_LEVIATHAN_EXPAND:
      lc->anim_y -= 0.005 * speed;
      if (lc->anim_y <= 0)
	{
	  lc->anim_y = 180;
	  lc->type = LAMENT_LEVIATHAN_UNTWIST;
	}
      break;

    case LAMENT_LEVIATHAN_UNTWIST:
      lc->anim_y -= 2 * speed;
      lc->anim_z = 1;
      if (lc->anim_y <= 0)
	{
	  lc->anim_y = 0;
	  lc->type = LAMENT_LEVIATHAN_UNFADE;
	  lc->anim_pause = 0;
	}
      break;

    case LAMENT_LEVIATHAN_UNFADE:
      lc->anim_z -= 0.1 * speed;
      if (lc->anim_z <= 0)
	{
	  lc->anim_z = 0;
	  lc->type = LAMENT_LEVIATHAN_UNSPIN;
	  lc->anim_pause = 0;
	}
      break;

    case LAMENT_LEVIATHAN_UNSPIN:
      lc->anim_r += 3.5 * speed;
      if (lc->anim_r >= 360 * 2)
	{
	  lc->anim_r = 0;
	  lc->type = LAMENT_BOX;
	  lc->anim_pause = pause2;
	}
      break;

    default:
      abort();
      break;
    }

# ifdef DEBUG_MODE

  lc->anim_pause = 0;

  if (lc->type == LAMENT_BOX)
    lc->type = DEBUG_MODE;

  if (lc->ffwdp)
    {
      lc->ffwdp = 0;
      while (lc->type != DEBUG_MODE)
        animate (mi);
    }

# else /* !DEBUG_MODE */

  if (lc->ffwdp && lc->type == LAMENT_BOX)
    {
      lc->ffwdp = 0;
      while (lc->type == LAMENT_BOX)
        animate (mi);
      lc->anim_pause = 0;
    }

# endif /* !DEBUG_MODE */
}


static void
gl_init (ModeInfo *mi)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  int i;

  if (wire)
    do_texture = False;

  if (!wire)
    {
      static const GLfloat pos0[]  = { -4.0,  2.0, 5.0, 1.0 };
      static const GLfloat pos1[]  = {  6.0, -1.0, 3.0, 1.0 };

      static const GLfloat amb0[]  = { 0.7, 0.7, 0.7, 1.0 };
/*    static const GLfloat amb1[]  = { 0.7, 0.0, 0.0, 1.0 }; */
      static const GLfloat dif0[]  = { 1.0, 1.0, 1.0, 1.0 };
      static const GLfloat dif1[]  = { 0.3, 0.1, 0.1, 1.0 };

      glLightfv(GL_LIGHT0, GL_POSITION, pos0);
      glLightfv(GL_LIGHT1, GL_POSITION, pos1);

      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb0);
/*    glLightfv(GL_LIGHT1, GL_AMBIENT,  amb1); */
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif0);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,  dif1);

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
      int i;
      for (i = 0; i < countof(lc->texids); i++)
	glGenTextures(1, &lc->texids[i]);

      lc->texture = image_data_to_ximage (mi->dpy, mi->xgwa.visual,
                                          lament512_png, sizeof(lament512_png));

      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      /* messes up -fps */
      /* glPixelStorei(GL_UNPACK_ROW_LENGTH, lc->texture->width); */

      for (i = 0; i < countof(lc->texids); i++)
	{
	  int height = lc->texture->width;	/* assume square */
	  glBindTexture(GL_TEXTURE_2D, lc->texids[i]);

          clear_gl_error();
	  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		       lc->texture->width, height, 0,
		       GL_RGBA, GL_UNSIGNED_BYTE,
                       (lc->texture->data +
			(lc->texture->bytes_per_line * height * i)));
          check_gl_error("texture");

          glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
          glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
          glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          check_gl_error("texture");

          /* This makes scaled pixmaps tolerable to look at. */
# if !defined(GL_TEXTURE_LOD_BIAS) && defined(GL_TEXTURE_LOD_BIAS_EXT)
#   define GL_TEXTURE_LOD_BIAS GL_TEXTURE_LOD_BIAS_EXT
# endif
# ifdef GL_TEXTURE_LOD_BIAS
          glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.25);
# endif
          clear_gl_error();  /* invalid enum on iPad 3 */
	}
    }

  for (i = 0; i < countof(all_objs); i++)
    {
      GLfloat s = 1/3.0;  /* box is 3" square */
      const struct gllist *L = *all_objs[i];
      const GLfloat *f = (const GLfloat *) L->data;
      int j;

      lc->dlists[i] = glGenLists(1);
      lc->polys[i] = L->points / 3;
      glNewList(lc->dlists[i], GL_COMPILE);
      if (L->primitive != GL_TRIANGLES) abort();
      if (L->format != GL_N3F_V3F) abort();

      glPushMatrix();
      glTranslatef (-0.5, -0.5, -0.5);
      glScalef (s, s, s);
      
      for (j = 0; j < L->points; j += 3)
        {
          int face, outerp;
          Bool blackp = (i == OBJ_ISO_BASE_A || i == OBJ_ISO_BASE_B);
          which_face (mi, f, &face, &outerp); /* from norm of first vert */

          set_colors (outerp ? exterior_color :
                      blackp ? black_color : interior_color);
          glBindTexture (GL_TEXTURE_2D, 
                         (outerp ? lc->texids[face-1] :
                          blackp ? 0 : lc->texids[6]));

          glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
          if (face) texturize_vert (mi, face, f+3);
          glNormal3fv (f); f += 3; glVertex3fv (f); f += 3;
          if (face) texturize_vert (mi, face, f+3);
          glNormal3fv (f); f += 3; glVertex3fv (f); f += 3;
          if (face) texturize_vert (mi, face, f+3);
          glNormal3fv (f); f += 3; glVertex3fv (f); f += 3;
          glEnd();
        }
      glPopMatrix();

      glEndList();
    }
}


ENTRYPOINT Bool
lament_handle_event (ModeInfo *mi, XEvent *event)
{
  lament_configuration *lc = &lcs[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, lc->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &lc->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t')
        {
          lc->ffwdp = True;
          return True;
        }
    }

  return False;
}


ENTRYPOINT void
reshape_lament (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT void
init_lament (ModeInfo *mi)
{
  lament_configuration *lc;
  int i;
  MI_INIT (mi, lcs);

  lc = &lcs[MI_SCREEN(mi)];

  {
    double rot_speed = 0.5;
    lc->rot = make_rotator (rot_speed, rot_speed, rot_speed, 1, 0, True);
    lc->trackball = gltrackball_init (True);
  }

  lc->type = LAMENT_BOX;
  lc->anim_pause = 300 + (random() % 100);

  if ((lc->glx_context = init_GL(mi)) != NULL)
    {
      reshape_lament(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      gl_init(mi);
    }

  lc->states = (lament_type *) calloc (200, sizeof (*lc->states));
  lc->nstates = 0;

# define PUSH(N,WHICH) \
    for (i = 0; i < N; i++) lc->states[lc->nstates++] = WHICH

  PUSH (4, LAMENT_TETRA_UNE);		/* most common */
  PUSH (4, LAMENT_TETRA_USW);
  PUSH (4, LAMENT_TETRA_DWN);
  PUSH (4, LAMENT_TETRA_DSE);

  PUSH (8, LAMENT_STAR_OUT);		/* pretty common */
  PUSH (8, LAMENT_TASER_OUT);
  PUSH (8, LAMENT_PILLAR_OUT);

  PUSH (4, LAMENT_LID_OPEN);		/* rare */
  PUSH (2, LAMENT_SPHERE_OUT);		/* rare */
  PUSH (1, LAMENT_LEVIATHAN_SPIN);	/* very rare */

  PUSH (35, LAMENT_BOX);		/* rest state */
# undef PUSH

  shuffle_states (lc);

# ifdef DEBUG_MODE
  lc->type = DEBUG_MODE;
  lc->anim_pause = 0;
# endif

}


ENTRYPOINT void
draw_lament (ModeInfo *mi)
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

  if (!lc->ffwdp && lc->anim_pause)
    lc->anim_pause--;
  else
    animate(mi);
}

XSCREENSAVER_MODULE ("Lament", lament)

#endif /* USE_GL */
