/* dymaxionmap --- Buckminster Fuller's unwrapped icosahedral globe.
 * Copyright (c) 2016-2018 Jamie Zawinski.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.	The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 */

#define LABEL_FONT "-*-helvetica-bold-r-normal-*-*-240-*-*-*-*-*-*"

#ifdef STANDALONE
#define DEFAULTS    "*delay:		20000	\n" \
		    "*showFPS:		False	\n" \
		    "*wireframe:	False	\n" \
		    "*labelFont:  " LABEL_FONT "\n"
# define release_planet 0
# include "xlockmore.h"		    /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"		    /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#include "sphere.h"
#include "normals.h"
#include "texfont.h"

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#define DEF_ROTATE  "True"
#define DEF_ROLL    "True"
#define DEF_WANDER  "True"
#define DEF_TEXTURE "True"
#define DEF_STARS   "True"
#define DEF_SPEED   "1.0"
#define DEF_IMAGE   "BUILTIN"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

static int do_roll;
static int do_wander;
static int do_texture;
static int do_stars;
static GLfloat speed;
static char *which_image;

static XrmOptionDescRec opts[] = {
  {"-speed",   ".dymaxionmap.speed",   XrmoptionSepArg, 0 },
  {"-roll",    ".dymaxionmap.roll",    XrmoptionNoArg, "true" },
  {"+roll",    ".dymaxionmap.roll",    XrmoptionNoArg, "false" },
  {"-wander",  ".dymaxionmap.wander",  XrmoptionNoArg, "true" },
  {"+wander",  ".dymaxionmap.wander",  XrmoptionNoArg, "false" },
  {"-texture", ".dymaxionmap.texture", XrmoptionNoArg, "true" },
  {"+texture", ".dymaxionmap.texture", XrmoptionNoArg, "false" },
  {"-stars",   ".dymaxionmap.stars",   XrmoptionNoArg, "true" },
  {"+stars",   ".dymaxionmap.stars",   XrmoptionNoArg, "false" },
  {"-image",   ".dymaxionmap.image",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,       "speed",   "Speed",   DEF_SPEED,   t_Float},
  {&do_roll,	 "roll",    "Roll",    DEF_ROLL,    t_Bool},
  {&do_wander,	 "wander",  "Wander",  DEF_WANDER,  t_Bool},
  {&do_texture,	 "texture", "Texture", DEF_TEXTURE, t_Bool},
  {&do_stars,	 "stars",   "Stars",   DEF_STARS,   t_Bool},
  {&which_image, "image",   "Image",   DEF_IMAGE,   t_String},
};

ENTRYPOINT ModeSpecOpt planet_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   planet_description =
{"planet", "init_planet", "draw_planet", NULL,
 "draw_planet", "init_planet", "free_planet", &planet_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Buckminster Fuller's unwrapped icosahedral globe", 0, NULL};
#endif

# ifdef __GNUC__
  __extension__	 /* don't warn about "string length is greater than the length
		    ISO C89 compilers are required to support" when including
		    the following XPM file... */
# endif
#include "images/gen/dymaxionmap_png.h"
#include "images/gen/ground_png.h"

#include "ximage-loader.h"
#include "rotator.h"
#include "gltrackball.h"


typedef struct {
  GLXContext *glx_context;
  GLuint starlist;
  int starcount;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  enum { STARTUP, FLAT, FOLD, 
         ICO, STEL_IN, AXIS, SPIN, STEL, STEL_OUT,
         ICO2, UNFOLD } state;
  GLfloat ratio;
  GLuint tex1, tex2;
  texture_font_data *font_data;
} planetstruct;


static planetstruct *planets = NULL;


/* Set up and enable texturing on our object */
static void
setup_png_texture (ModeInfo *mi, const unsigned char *png_data,
                   unsigned long data_size)
{
  XImage *image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                        png_data, data_size);
  char buf[1024];
  clear_gl_error();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  /* iOS invalid enum:
  glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
  */
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
	       image->width, image->height, 0,
	       GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "builtin texture (%dx%d)", image->width, image->height);
  check_gl_error(buf);
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
  glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
	       image->width, image->height, 0,
	       GL_RGBA, GL_UNSIGNED_BYTE, image->data);
  sprintf (buf, "texture: %.100s (%dx%d)",
	   filename, image->width, image->height);
  check_gl_error(buf);
  return True;
}


static void
setup_texture(ModeInfo * mi)
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
    BUILTIN:
      setup_png_texture (mi, dymaxionmap_png, sizeof(dymaxionmap_png));
    }
  else
    {
      if (! setup_file_texture (mi, which_image))
        goto BUILTIN;
    }

  glGenTextures (1, &gp->tex2);
  glBindTexture (GL_TEXTURE_2D, gp->tex2);

  /* Must be after glBindTexture */
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  setup_png_texture (mi, ground_png, sizeof(ground_png));

  check_gl_error("texture initialization");

  /* Need to flip the texture top for bottom for some reason. */
  glMatrixMode (GL_TEXTURE);
  glScalef (1, -1, 1);
  glMatrixMode (GL_MODELVIEW);
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
		       : (BELLRAND(1)-0.5)/12);	  /* milky way */
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


ENTRYPOINT void
reshape_planet (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (h, h, h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


static void
do_normal2 (Bool frontp, XYZ a, XYZ b, XYZ c)
{
  if (frontp)
    do_normal (a.x, a.y, a.z,
               b.x, b.y, b.z,
               c.x, c.y, c.z);
  else
    do_normal (b.x, b.y, b.z,
               a.x, a.y, a.z,
               c.x, c.y, c.z);
}


static void
triangle0 (ModeInfo *mi, Bool frontp, GLfloat stel_ratio, int bitmask)
{
  /* Render a triangle as six sub-triangles:

                A
               / \
              / | \
             /  |  \
            / 0 | 1 \
        E  /_   |   _\  F
          /  \_ | _/  \
         / 5   \D/   2 \
        /    /  |  \    \
       /   / 4  | 3  \   \
      /  /      |       \ \
   B ----------------------- C
                G
   */

  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat h = sqrt(3) / 2;
  GLfloat h2 = sqrt(h*h - (h/2)*(h/2)) - 0.5;
  XYZ  A,  B,  C,  D,  E,  F,  G;
  XYZ tA, tB, tC, tD, tE, tF, tG;
  XYZ  a,  b,  c;
  XYZ ta, tb, tc;
  A.x =  0;   A.y = h;   A.z = 0;
  B.x = -0.5, B.y = 0;   B.z = 0;
  C.x =  0.5, C.y = 0;   C.z = 0;
  D.x =  0;   D.y = h/3; D.z = 0;
  E.x = -h2;  E.y = h/2; E.z = 0;
  F.x =  h2;  F.y = h/2; F.z = 0;
  G.x =  0;   G.y = 0;   G.z = 0;

  /* When tweaking object XY to stellate, don't change texture coordinates. */
  tA = A; tB = B; tC = C; tD = D; tE = E; tF = F; tG = G;

  /* Eyeballed this to find the depth of stellation that seems to most
     approximate a sphere.
   */
  D.z = 0.193 * stel_ratio;

  /* We want to raise E, F and G as well but we can't just shift Z:
     we need to keep them on the same vector from the center of the sphere,
     which means also changing F and G's X and Y.
  */
  E.z = F.z = G.z = 0.132 * stel_ratio;
  {
    double magic_x = 0.044;
    double magic_y = 0.028;
    /* G.x stays 0 */
    G.y -= sqrt (magic_x*magic_x + magic_y*magic_y) * stel_ratio;
    E.x -= magic_x * stel_ratio;
    E.y += magic_y * stel_ratio;
    F.x += magic_x * stel_ratio;
    F.y += magic_y * stel_ratio;
  }


  if (bitmask & 1<<0)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  E;  b =  D;  c =  A;
      ta = tE; tb = tD; tc = tA;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (bitmask & 1<<1)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  D;  b =  F;  c =  A;
      ta = tD; tb = tF; tc = tA;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (bitmask & 1<<2)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  D;  b =  C;  c =  F;
      ta = tD; tb = tC; tc = tF;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (bitmask & 1<<3)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  G;  b =  C;  c =  D;
      ta = tG; tb = tC; tc = tD;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (bitmask & 1<<4)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  B;  b =  G;  c =  D;
      ta = tB; tb = tG; tc = tD;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (bitmask & 1<<5)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  B;  b =  D;  c =  E;
      ta = tB; tb = tD; tc = tE;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
  if (bitmask & 1<<6)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      a  =  E;  b =  D;  c =  A;
      ta = tE; tb = tD; tc = tA;
      do_normal2 (frontp, a, b, c);
      glTexCoord2f (ta.x, ta.y); glVertex3f (a.x, a.y, a.z);
      glTexCoord2f (tb.x, tb.y); glVertex3f (b.x, b.y, b.z);
      glTexCoord2f (tc.x, tc.y); glVertex3f (c.x, c.y, c.z);
      glEnd();
      mi->polygon_count++;
    }
}


/* The segments, numbered arbitrarily from the top left:
             ________         _      ________
             \      /\      /\ \    |\      /
              \ 0  /  \    /  \3>   | \ 5  /
               \  / 1  \  / 2  \| ..|4 \  /-6-..
     ___________\/______\/______\/______\/______\
    |   /\      /\      /\      /\      /\   
    |7 /  \ 9  /  \ 11 /  \ 13 /  \ 15 /  \  
    | / 8  \  / 10 \  / 12 \  / 14 \  / 16 \ 
    |/______\/______\/______\/______\/______\
     \      /\      /       /\      /\
      \ 17 /  \ 18 /       /  \ 20 /  \
       \  /    \  /       / 19 \  / 21 \
        \/      \/       /______\/______\

   Each triangle can be connected to at most two other triangles.
   We start from the middle, #12, and work our way to the edges.
   Its centroid is 0,0.
 */
static void
triangle (ModeInfo *mi, int which, Bool frontp, 
          GLfloat fold_ratio, GLfloat stel_ratio)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  const GLfloat fg[3] = { 1, 1, 1 };
  const GLfloat bg[3] = { 0.3, 0.3, 0.3 };
  int a = -1, b = -1;
  GLfloat max = acos (sqrt(5)/3);
  GLfloat rot = -max * fold_ratio / (M_PI/180);
  Bool wire = MI_IS_WIREFRAME(mi);

  if (wire)
    glColor3fv (fg);

  switch (which) {
  case 3:				/* One third of the face. */
    triangle0 (mi, frontp, stel_ratio, 1<<3 | 1<<4);
    break;
  case 4:				/* Two thirds of the face: convex. */
    triangle0 (mi, frontp, stel_ratio, 1<<1 | 1<<2 | 1<<3 | 1<<4);
    break;
  case 6:				/* One half of the face. */
    triangle0 (mi, frontp, stel_ratio, 1<<1 | 1<<2 | 1<<3);
    break;
  case 7:				/* One half of the face. */
    triangle0 (mi, frontp, stel_ratio, 1<<2 | 1<<3 | 1<<4);
    break;
  default:				/* Full face. */
    triangle0 (mi, frontp, stel_ratio, 0x3F);
    break;
  }

  if (wire)
    {
      char tag[20];
      glColor3fv (bg);
      sprintf (tag, "%d", which);
      glPushMatrix();
      glTranslatef (-0.1, 0.2, 0);
      glScalef (0.005, 0.005, 0.005);
      print_texture_string (gp->font_data, tag);
      glPopMatrix();
      mi->polygon_count++;
    }


  /* The connection hierarchy of the faces starting at the middle, #12. */
  switch (which) {
  case  0: break;
  case  1: a =  0; b = -1; break;
  case  2: a = -1; b =  3; break;
  case  3: break;
  case  4: a = -1; b =  5; break;
  case  5: a = -1; b =  6; break;
  case  7: break;
  case  6: break;
  case  8: a = 17; b =  7; break;
  case  9: a =  8; b = -1; break;
  case 10: a = 18; b =  9; break;
  case 11: a = 10; b =  1; break;
  case 12: a = 11; b = 13; break;
  case 13: a =  2; b = 14; break;
  case 14: a = 15; b = 20; break;
  case 15: a =  4; b = 16; break;
  case 16: break;
  case 17: break;
  case 18: break;
  case 19: break;
  case 20: a = 21; b = 19; break;
  case 21: break;
  default: abort(); break;
  }

  if (a != -1)
    {
      glPushMatrix();
      glTranslatef (-0.5, 0, 0);	/* Move model matrix to upper left */
      glRotatef (60, 0, 0, 1);
      glTranslatef ( 0.5, 0, 0);

      glMatrixMode(GL_TEXTURE);
      /* glPushMatrix(); */
      glTranslatef (-0.5, 0, 0);	/* Move texture matrix the same way */
      glRotatef (60, 0, 0, 1);
      glTranslatef ( 0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);

      glRotatef (rot, 1, 0, 0);
      triangle (mi, a, frontp, fold_ratio, stel_ratio);

      /* This should just be a PopMatrix on the TEXTURE stack, but
         fucking iOS has GL_MAX_TEXTURE_STACK_DEPTH == 4!  WTF!
         So we have to undo our rotations and translations manually.
       */
      glMatrixMode(GL_TEXTURE);
      /* glPopMatrix(); */
      glTranslatef (-0.5, 0, 0);
      glRotatef (-60, 0, 0, 1);
      glTranslatef (0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
    }

  if (b != -1)
    {
      glPushMatrix();
      glTranslatef (0.5, 0, 0);		/* Move model matrix to upper right */
      glRotatef (-60, 0, 0, 1);
      glTranslatef (-0.5, 0, 0);

      glMatrixMode(GL_TEXTURE);
      /* glPushMatrix(); */
      glTranslatef (0.5, 0, 0);		/* Move texture matrix the same way */
      glRotatef (-60, 0, 0, 1);
      glTranslatef (-0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);

      glRotatef (rot, 1, 0, 0);
      triangle (mi, b, frontp, fold_ratio, stel_ratio);

      /* See above. Grr. */
      glMatrixMode(GL_TEXTURE);
      /* glPopMatrix(); */
      glTranslatef (0.5, 0, 0);
      glRotatef (60, 0, 0, 1);
      glTranslatef (-0.5, 0, 0);

      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
    }
}


static void
draw_triangles (ModeInfo *mi, GLfloat fold_ratio, GLfloat stel_ratio)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME(mi);
  GLfloat h = sqrt(3) / 2;
  GLfloat c = h / 3;

  glTranslatef (0, -h/3, 0);  /* Center on face 12 */

  /* When closed, center on midpoint of icosahedron. Eyeballed this. */
  glTranslatef (0, 0, fold_ratio * 0.754);

  glFrontFace (GL_CCW);

  /* Adjust the texture matrix so that it has the same coordinate space
     as the model. */

  glMatrixMode(GL_TEXTURE);
  glPushMatrix();
  {
    GLfloat texw = 5.5;
    GLfloat texh = 3 * h;
    GLfloat midx = 2.5;
    GLfloat midy = 3 * c;
    glScalef (1/texw, -1/texh, 1);
    glTranslatef (midx, midy, 0);
  }
  glMatrixMode(GL_MODELVIEW);



  /* Front faces */

  if (wire)
    glDisable (GL_TEXTURE_2D);
  else if (do_texture)
    {
      glEnable (GL_TEXTURE_2D);
      glBindTexture (GL_TEXTURE_2D, gp->tex1);
    }
  else
    glDisable (GL_TEXTURE_2D);

  triangle (mi, 12, True, fold_ratio, stel_ratio);

  /* Back faces */

  if (wire)
    glDisable (GL_TEXTURE_2D);
  else if (do_texture)
    {
      glEnable (GL_TEXTURE_2D);
      glBindTexture (GL_TEXTURE_2D, gp->tex2);
    }
  else
    glDisable (GL_TEXTURE_2D);

  glFrontFace (GL_CW);

  triangle (mi, 12, False, fold_ratio, 0);

  glMatrixMode(GL_TEXTURE);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}


static void
align_axis (ModeInfo *mi, int undo)
{
  /* Rotate so that an axis is lined up with the north and south poles
     on the map, which are not in the center of their faces, or any
     other easily computable spot. */

  GLfloat r1 = 20.5;
  GLfloat r2 = 28.5;

  if (undo)
    {
      glRotatef (-r2, 0, 1, 0);
      glRotatef ( r2, 1, 0, 0);
      glRotatef (-r1, 1, 0, 0);
    }
  else
    {
      glRotatef (r1, 1, 0, 0);
      glRotatef (-r2, 1, 0, 0);
      glRotatef ( r2, 0, 1, 0);
    }
}


static void
draw_axis (ModeInfo *mi)
{
  GLfloat s;
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);
  glPushMatrix();

  align_axis (mi, 0);
  glTranslatef (0.34, 0.39, -0.61);

  s = 0.96;
  glScalef (s, s, s);   /* tighten up the enclosing sphere */

  glColor3f (0.5, 0.5, 0);

  glRotatef (90,  1, 0, 0);    /* unit_sphere is off by 90 */
  glRotatef (9.5, 0, 1, 0);    /* line up the time zones */
  glFrontFace (GL_CCW);
  unit_sphere (12, 24, True);
  glBegin(GL_LINES);
  glVertex3f(0, -2, 0);
  glVertex3f(0,  2, 0);
  glEnd();

  glPopMatrix();
}




ENTRYPOINT Bool
planet_handle_event (ModeInfo *mi, XEvent *event)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, gp->trackball,
				 MI_WIDTH (mi), MI_HEIGHT (mi),
				 &gp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
          switch (gp->state) {
          case FLAT: case ICO: case STEL: case AXIS: case ICO2:
            gp->ratio = 1;
            break;
          default:
            break;
          }
          return True;
        }
    }

  return False;
}


ENTRYPOINT void
init_planet (ModeInfo * mi)
{
  planetstruct *gp;
  int screen = MI_SCREEN(mi);
  Bool wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, planets);
  gp = &planets[screen];

  if ((gp->glx_context = init_GL(mi)) != NULL) {
    reshape_planet(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  gp->state = STARTUP;
  gp->ratio = 0;
  gp->font_data = load_texture_font (mi->dpy, "labelFont");

  {
    double spin_speed	= 0.1;
    double wander_speed = 0.002;
    gp->rot = make_rotator (do_roll ? spin_speed : 0,
			    do_roll ? spin_speed : 0,
			    0, 1,
			    do_wander ? wander_speed : 0,
			    False);
    gp->trackball = gltrackball_init (True);
  }

  if (wire)
    do_texture = False;

  if (do_texture)
    setup_texture (mi);

  if (do_stars)
    init_stars (mi);

  glEnable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
      GLfloat amb[4] = {0.4, 0.4, 0.4, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,	amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,	dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }
}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.35;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


ENTRYPOINT void
draw_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  double x, y, z;

  if (!gp->glx_context)
    return;

  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glXMakeCurrent (dpy, window, *(gp->glx_context));

  mi->polygon_count = 0;

  if (! gp->button_down_p)
    switch (gp->state) {
    case STARTUP:  gp->ratio += speed * 0.01;  break;
    case FLAT:     gp->ratio += speed * 0.005; break;
    case FOLD:     gp->ratio += speed * 0.01;  break;
    case ICO:      gp->ratio += speed * 0.01;  break;
    case STEL_IN:  gp->ratio += speed * 0.05;  break;
    case STEL:     gp->ratio += speed * 0.01;  break;
    case STEL_OUT: gp->ratio += speed * 0.07;  break;
    case ICO2:     gp->ratio += speed * 0.07;  break;
    case AXIS:     gp->ratio += speed * 0.02;  break;
    case SPIN:     gp->ratio += speed * 0.005; break;
    case UNFOLD:   gp->ratio += speed * 0.01;  break;
    default:       abort();
    }

  if (gp->ratio > 1.0)
    {
      gp->ratio = 0;
      switch (gp->state) {
      case STARTUP:  gp->state = FLAT;     break;
      case FLAT:     gp->state = FOLD;     break;
      case FOLD:     gp->state = ICO;      break;
      case ICO:      gp->state = STEL_IN;  break;
      case STEL_IN:  gp->state = STEL;     break;
      case STEL:
        {
          int i = (random() << 9) % 7;
          gp->state = (i < 3 ? STEL_OUT :
                       i < 6 ? SPIN : AXIS);
        }
        break;
      case AXIS:     gp->state = STEL_OUT; break;
      case SPIN:     gp->state = STEL_OUT; break;
      case STEL_OUT: gp->state = ICO2;     break;
      case ICO2:     gp->state = UNFOLD;   break;
      case UNFOLD:   gp->state = FLAT;     break;
      default:       abort();
      }
    }

  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK); 

  glPushMatrix();

  gltrackball_rotate (gp->trackball);
  glRotatef (current_device_rotation(), 0, 0, 1);

  if (gp->state != STARTUP)
    {
      get_position (gp->rot, &x, &y, &z, !gp->button_down_p);
      x = (x - 0.5) * 3;
      y = (y - 0.5) * 3;
      z = 0;
      glTranslatef(x, y, z);
    }

  if (do_roll && gp->state != STARTUP)
    {
      get_rotation (gp->rot, &x, &y, 0, !gp->button_down_p);
      glRotatef (x * 360, 1.0, 0.0, 0.0);
      glRotatef (y * 360, 0.0, 1.0, 0.0);
    }

  if (do_stars)
    {
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_LIGHTING);
      glPushMatrix();
      glScalef (60, 60, 60);
      glRotatef (90, 1, 0, 0);
      glRotatef (35, 1, 0, 0);
      glCallList (gp->starlist);
      mi->polygon_count += gp->starcount;
      glPopMatrix();
      glClear(GL_DEPTH_BUFFER_BIT);
    }

  if (! wire)
    glEnable (GL_LIGHTING);

  if (do_texture)
    glEnable(GL_TEXTURE_2D);

  glScalef (2.6, 2.6, 2.6);

  {
    GLfloat fold_ratio = 0;
    GLfloat stel_ratio = 0;
    switch (gp->state) {
    case FOLD:     fold_ratio =     gp->ratio; break;
    case UNFOLD:   fold_ratio = 1 - gp->ratio; break;
    case ICO: case ICO2: fold_ratio = 1; break;
    case STEL: case AXIS: case SPIN: fold_ratio = 1; stel_ratio = 1; break;
    case STEL_IN:  fold_ratio = 1; stel_ratio = gp->ratio; break;
    case STEL_OUT: fold_ratio = 1; stel_ratio = 1 - gp->ratio; break;
    case STARTUP:      /* Tilt in from flat */
      glRotatef (-90 * ease_ratio (1 - gp->ratio), 1, 0, 0);
      break;

    default: break;
    }

# ifdef HAVE_MOBILE  /* Enlarge the icosahedron a bit to make it more visible */
    {
      GLfloat s = 1 + 1.3 * ease_ratio (fold_ratio);
      glScalef (s, s, s);
    }
# endif

    if (gp->state == SPIN)
      {
        align_axis (mi, 0);
        glRotatef (ease_ratio (gp->ratio) * 360 * 3, 0, 0, 1);
        align_axis (mi, 1);
      }

    draw_triangles (mi, ease_ratio (fold_ratio), ease_ratio (stel_ratio));

    if (gp->state == AXIS)
      draw_axis(mi);
  }

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_planet (ModeInfo * mi)
{
  planetstruct *gp = &planets[MI_SCREEN(mi)];

  if (gp->glx_context) {
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(gp->glx_context));

    if (glIsList(gp->starlist))
      glDeleteLists(gp->starlist, 1);
  }
}


XSCREENSAVER_MODULE_2 ("DymaxionMap", dymaxionmap, planet)

#endif
