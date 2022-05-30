/* glblur --- radial blur using GL textures
 * Copyright (c) 2002-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This program draws a box and a few line segments, and generates a flowing
 * radial blur outward from it -- this causes flowing field effects.
 * It does this by rendering the scene into a small texture, then repeatedly
 * rendering increasingly-enlarged and increasingly-transparent versions of
 * that texture onto the frame buffer.
 *
 * As such, it's quite graphics intensive -- don't bother trying to run this
 * if you don't have hardware-accelerated texture support.
 *
 * Inspired by Dario Corno's Radial Blur tutorial:
 *    http://nehe.gamedev.net/tutorials/lesson.asp?l=36
 */

#define DEFAULTS	"*delay:    10000 \n" \
			"*showFPS:  False \n" \
	               	"*fpsSolid: True  \n" \
			"*suppressRotationAnimation: True\n" \

# define release_glblur 0

#undef ABS
#define ABS(n) ((n)<0?-(n):(n))
#undef SIGNOF
#define SIGNOF(n) ((n)<0?-1:1)

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_BLUR_SIZE   "15"

typedef struct metaball metaball;


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint obj_dlist0;	/* east-west cube faces */
  GLuint obj_dlist1;	/* north-south cube faces */
  GLuint obj_dlist2;	/* up-down cube faces */
  GLuint obj_dlist3;	/* spikes coming out of the cube's corners */
  GLuint scene_dlist1;	/* the cube, rotated and translated */
  GLuint scene_dlist2;	/* the spikes, rotated and translated */
  int scene_polys1;	/* polygons in scene, not counting texture overlay */
  int scene_polys2;	/* polygons in scene, not counting texture overlay */

  GLuint texture;
  unsigned int *tex_data;
  int tex_w, tex_h;

  int ncolors;
  XColor *colors0;
  XColor *colors1;
  XColor *colors2;
  XColor *colors3;
  int ccolor;

  Bool show_cube_p;
  Bool show_spikes_p;

} glblur_configuration;

static glblur_configuration *bps = NULL;

static char *do_spin;
static Bool do_wander;
static int blursize;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-blursize", ".blurSize", XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&blursize,  "blurSize","BlurSize", DEF_BLUR_SIZE,  t_Int},
};

ENTRYPOINT ModeSpecOpt glblur_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_glblur (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 8.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}



/* Objects in the scene 
 */

static void
generate_object (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool wire = MI_IS_WIREFRAME (mi);
  int s = 10;

  bp->scene_polys1 = 0;
  bp->scene_polys2 = 0;

  glNewList (bp->obj_dlist0, GL_COMPILE);
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* front */
  glNormal3f (0, 0, 1);
  glTexCoord2f(1, 0); glVertex3f ( 0.5, -0.5,  0.5);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5,  0.5,  0.5);
  glTexCoord2f(1, 1); glVertex3f (-0.5, -0.5,  0.5);
  bp->scene_polys1++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* back */
  glNormal3f (0, 0, -1);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f ( 0.5, -0.5, -0.5);
  bp->scene_polys1++;
  glEnd();
  glEndList();

  glNewList (bp->obj_dlist1, GL_COMPILE);
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* left */
  glNormal3f (-1, 0, 0);
  glTexCoord2f(1, 1); glVertex3f (-0.5,  0.5,  0.5);
  glTexCoord2f(1, 0); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f (-0.5, -0.5,  0.5);
  bp->scene_polys1++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* right */
  glNormal3f (1, 0, 0);
  glTexCoord2f(1, 1); glVertex3f ( 0.5, -0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5, -0.5,  0.5);
  bp->scene_polys1++;
  glEnd();
  glEndList();

  glNewList (bp->obj_dlist2, GL_COMPILE);
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* top */
  glNormal3f (0, 1, 0);
  glTexCoord2f(0, 0); glVertex3f ( 0.5,  0.5,  0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5,  0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f (-0.5,  0.5, -0.5);
  glTexCoord2f(1, 0); glVertex3f (-0.5,  0.5,  0.5);
  bp->scene_polys1++;
  glEnd();

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);	/* bottom */
  glNormal3f (0, -1, 0);
  glTexCoord2f(1, 0); glVertex3f (-0.5, -0.5,  0.5);
  glTexCoord2f(0, 0); glVertex3f (-0.5, -0.5, -0.5);
  glTexCoord2f(0, 1); glVertex3f ( 0.5, -0.5, -0.5);
  glTexCoord2f(1, 1); glVertex3f ( 0.5, -0.5,  0.5);
  bp->scene_polys1++;
  glEnd();
  glEndList();

  glNewList (bp->obj_dlist3, GL_COMPILE);
  glLineWidth (1);
  glBegin(GL_LINES);
  glVertex3f(-s, 0, 0); glVertex3f(s, 0, 0);	/* face spikes */
  glVertex3f(0, -s, 0); glVertex3f(0, s, 0);
  glVertex3f(0, 0, -s); glVertex3f(0, 0, s);
  bp->scene_polys2 += 3;
  glEnd();

  glLineWidth (8);
  glBegin(GL_LINES);
  glVertex3f(-s, -s, -s); glVertex3f( s,  s,  s);  /* corner spikes */
  glVertex3f(-s, -s,  s); glVertex3f( s,  s, -s);
  glVertex3f(-s,  s, -s); glVertex3f( s, -s,  s);
  glVertex3f( s, -s, -s); glVertex3f(-s,  s,  s);
  bp->scene_polys2 += 4;
  glEnd();
  glEndList ();

  check_gl_error ("object generation");
}


static void
init_texture (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];

  if (bp->tex_data) free (bp->tex_data);

  bp->tex_w = 128;
  bp->tex_h = 128;
  bp->tex_data = (unsigned int *)
    malloc (bp->tex_w * bp->tex_h * 4 * sizeof (unsigned int));

  glGenTextures (1, &bp->texture);
  glBindTexture (GL_TEXTURE_2D, bp->texture);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                bp->tex_w, bp->tex_h, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, bp->tex_data);
  check_gl_error ("texture generation");
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


static void
render_scene_to_texture (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];

  glViewport (0, 0, bp->tex_w, bp->tex_h);

  glCallList (bp->scene_dlist1);
  glCallList (bp->scene_dlist2);

  glBindTexture (GL_TEXTURE_2D, bp->texture);
  glCopyTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE, 0, 0,
                    bp->tex_w, bp->tex_h, 0);
  check_gl_error ("texture2D");

  glViewport (0, 0, MI_WIDTH(mi), MI_HEIGHT(mi));
}

static void
overlay_blur_texture (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];
  int w = MI_WIDTH (mi);
  int h = MI_HEIGHT (mi);
  int times = blursize;
  int i;
  GLfloat inc = 0.02 * (25.0 / times);

  GLfloat spost = 0;		    /* starting texture coordinate offset */
  GLfloat alpha_inc;		    /* transparency fade factor */
  GLfloat alpha = 0.2;		    /* initial transparency */

  glEnable (GL_TEXTURE_2D);
  glDisable (GL_DEPTH_TEST);
  glBlendFunc (GL_SRC_ALPHA,GL_ONE);
  glEnable (GL_BLEND);
  glBindTexture (GL_TEXTURE_2D, bp->texture);


  /* switch to orthographic projection, saving both previous matrixes
     on their respective stacks.
   */
  glMatrixMode (GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho (0, w, h, 0, -1, 1);
  glMatrixMode (GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();	


  alpha_inc = alpha / times;

  mi->polygon_count = bp->scene_polys1 + bp->scene_polys2;

  glBegin (GL_QUADS);
  for (i = 0; i < times; i++)
    {
      glColor4f (1, 1, 1, alpha);
      glTexCoord2f (0+spost, 1-spost); glVertex2f (0, 0);
      glTexCoord2f (0+spost, 0+spost); glVertex2f (0, h);
      glTexCoord2f (1-spost, 0+spost); glVertex2f (w, h);
      glTexCoord2f (1-spost, 1-spost); glVertex2f (w, 0);
      spost += inc;
      alpha -= alpha_inc;
      mi->polygon_count++;
    }
  glEnd();

  /* Switch back to perspective projection, restoring the saved matrixes
   */
  glMatrixMode (GL_PROJECTION);
  glPopMatrix();
  glMatrixMode (GL_MODELVIEW);
  glPopMatrix();		

  glEnable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);
  glDisable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, 0);
}



/* Startup initialization
 */

ENTRYPOINT Bool
glblur_handle_event (ModeInfo *mi, XEvent *event)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void 
init_glblur (ModeInfo *mi)
{
  glblur_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_glblur (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

  if (!wire)
    {
      GLfloat gamb[4]= {0.2, 0.2,  0.2, 1.0};
      GLfloat pos[4] = {0.0, 5.0, 10.0, 1.0};
      GLfloat amb[4] = {0.2, 0.2,  0.2, 1.0};
      GLfloat dif[4] = {0.3, 0.3,  0.3, 1.0};
      GLfloat spc[4] = {0.8, 0.8,  0.8, 1.0};
      GLfloat shiny = 128;

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
      glEnable(GL_NORMALIZE);
      glShadeModel(GL_SMOOTH);

      glLightModelfv (GL_LIGHT_MODEL_AMBIENT, gamb);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);

      glMaterialf(GL_FRONT, GL_SHININESS, shiny);
    }

  {
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 0.9;
    double wander_speed = 0.06;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else if (*s == '0') ;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    bp->rot = make_rotator (spinx ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            1.0,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }

  if (blursize < 0) blursize = 0;
  if (blursize > 200) blursize = 200;

  bp->ncolors = 128;
  bp->colors0 = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  bp->colors1 = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  bp->colors2 = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  bp->colors3 = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0, bp->colors0, &bp->ncolors, False, 0, False);
  make_smooth_colormap (0, 0, 0, bp->colors1, &bp->ncolors, False, 0, False);
  make_smooth_colormap (0, 0, 0, bp->colors2, &bp->ncolors, False, 0, False);
  make_smooth_colormap (0, 0, 0, bp->colors3, &bp->ncolors, False, 0, False);
  bp->ccolor = 0;

  bp->obj_dlist0   = glGenLists (1);
  bp->obj_dlist1   = glGenLists (1);
  bp->obj_dlist2   = glGenLists (1);
  bp->obj_dlist3   = glGenLists (1);
  bp->scene_dlist1 = glGenLists (1);
  bp->scene_dlist2 = glGenLists (1);

  init_texture (mi);

  generate_object (mi);
}


/* Render one frame
 */
ENTRYPOINT void
draw_glblur (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  GLfloat color0[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color1[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color2[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat color3[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat spec[4]   = {1.0, 1.0, 1.0, 1.0};

  double rx, ry, rz;
  double px, py, pz;
  int extra_polys = 0;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  /* Decide what we're drawing
   */
  if (0 == (random() % 30))
    {
      bp->show_cube_p   = (0 == (random() % 10));
      bp->show_spikes_p = (0 == (random() % 20));
    }

  /* Select new colors for the various objects
   */
  color0[0] = bp->colors0[bp->ccolor].red   / 65536.0;
  color0[1] = bp->colors0[bp->ccolor].green / 65536.0;
  color0[2] = bp->colors0[bp->ccolor].blue  / 65536.0;

  color1[0] = bp->colors1[bp->ccolor].red   / 65536.0;
  color1[1] = bp->colors1[bp->ccolor].green / 65536.0;
  color1[2] = bp->colors1[bp->ccolor].blue  / 65536.0;

  color2[0] = bp->colors2[bp->ccolor].red   / 65536.0;
  color2[1] = bp->colors2[bp->ccolor].green / 65536.0;
  color2[2] = bp->colors2[bp->ccolor].blue  / 65536.0;

  color3[0] = bp->colors3[bp->ccolor].red   / 65536.0;
  color3[1] = bp->colors3[bp->ccolor].green / 65536.0;
  color3[2] = bp->colors3[bp->ccolor].blue  / 65536.0;

  bp->ccolor++;
  if (bp->ccolor >= bp->ncolors) bp->ccolor = 0;


  get_position (bp->rot, &px, &py, &pz, !bp->button_down_p);
  get_rotation (bp->rot, &rx, &ry, &rz, !bp->button_down_p);

  px = (px - 0.5) * 2;
  py = (py - 0.5) * 2;
  pz = (pz - 0.5) * 8;
  rx *= 360;
  ry *= 360;
  rz *= 360;

  /* Generate scene_dlist1, which contains the box (not spikes),
     rotated into position.
   */
  glNewList (bp->scene_dlist1, GL_COMPILE);
  {
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glTranslatef (px, py, pz);
    gltrackball_rotate (bp->trackball);
    glRotatef (rx, 1.0, 0.0, 0.0);
    glRotatef (ry, 0.0, 1.0, 0.0);
    glRotatef (rz, 0.0, 0.0, 1.0);

    glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, spec);

    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color0);
    glCallList (bp->obj_dlist0);

    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
    glCallList (bp->obj_dlist1);

    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
    glCallList (bp->obj_dlist2);

    glMatrixMode (GL_MODELVIEW);
    glPopMatrix ();
  }
  glEndList ();


  /* Generate scene_dlist2, which contains the spikes (not box),
     rotated into position.
   */
  glNewList (bp->scene_dlist2, GL_COMPILE);
  {
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glTranslatef (px, py, pz);
    gltrackball_rotate (bp->trackball);
    glRotatef (rx, 1.0, 0.0, 0.0);
    glRotatef (ry, 0.0, 1.0, 0.0);
    glRotatef (rz, 0.0, 0.0, 1.0);

    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color3);
    glCallList (bp->obj_dlist3);

    glMatrixMode (GL_MODELVIEW);
    glPopMatrix ();
  }
  glEndList ();


  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  render_scene_to_texture (mi);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (bp->show_cube_p || bp->button_down_p)
    {
      glCallList (bp->scene_dlist1);
      extra_polys += bp->scene_polys1;
    }
  if (bp->show_spikes_p || bp->button_down_p)
    {
      glCallList (bp->scene_dlist2);
      extra_polys += bp->scene_polys2;
    }

  overlay_blur_texture (mi);
  mi->polygon_count += extra_polys;

  glFlush ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_glblur (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->tex_data) free (bp->tex_data);
  if (bp->colors0) free (bp->colors0);
  if (bp->colors1) free (bp->colors1);
  if (bp->colors2) free (bp->colors2);
  if (bp->colors3) free (bp->colors3);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (glIsList(bp->obj_dlist0)) glDeleteLists(bp->obj_dlist0, 1);
  if (glIsList(bp->obj_dlist1)) glDeleteLists(bp->obj_dlist1, 1);
  if (glIsList(bp->obj_dlist2)) glDeleteLists(bp->obj_dlist2, 1);
  if (glIsList(bp->obj_dlist3)) glDeleteLists(bp->obj_dlist3, 1);
  if (glIsList(bp->scene_dlist1)) glDeleteLists(bp->scene_dlist1, 1);
  if (glIsList(bp->scene_dlist2)) glDeleteLists(bp->scene_dlist2, 1);
  if (bp->texture) glDeleteTextures (1, &bp->texture);
}

XSCREENSAVER_MODULE ("GLBlur", glblur)

#endif /* USE_GL */
