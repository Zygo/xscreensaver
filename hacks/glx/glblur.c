/* glblur --- radial blur using GL textures
 * Copyright (c) 2002 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"GLBlur"
#define HACK_INIT	init_glblur
#define HACK_DRAW	draw_glblur
#define HACK_RESHAPE	reshape_glblur
#define HACK_HANDLE_EVENT glblur_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts	xlockmore_opts

#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_BLURSIZE    "15"

#define DEFAULTS	"*delay:	10000       \n" \
			"*showFPS:      False       \n" \
	               	"*fpsSolid:     True	    \n" \
			"*wireframe:    False       \n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \
			"*blurSize:   " DEF_BLURSIZE "\n" \


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

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

#include <GL/glu.h>


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
  {(caddr_t *) &do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {(caddr_t *) &do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {(caddr_t *) &blursize,  "blurSize","BlurSize", DEF_BLURSIZE,  t_Int},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
void
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
  glTexImage2D (GL_TEXTURE_2D, 0, 4, 128, 128, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, bp->tex_data);
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
  check_gl_error ("texture");

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
  GLfloat alpha_inc = 0.9 / times;  /* transparency fade factor */
  GLfloat alpha = 0.2;		    /* initial transparency */

  glDisable (GL_TEXTURE_GEN_S);
  glDisable (GL_TEXTURE_GEN_T);

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

Bool
glblur_handle_event (ModeInfo *mi, XEvent *event)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      gltrackball_track (bp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


void 
init_glblur (ModeInfo *mi)
{
  glblur_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (glblur_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (glblur_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_glblur (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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

      glMateriali(GL_FRONT, GL_SHININESS, shiny);
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
    bp->trackball = gltrackball_init ();
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
void
draw_glblur (ModeInfo *mi)
{
  glblur_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  static GLfloat color0[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat color1[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat color2[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat color3[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat spec[4]   = {1.0, 1.0, 1.0, 1.0};

  double rx, ry, rz;
  double px, py, pz;
  int extra_polys = 0;

  if (!bp->glx_context)
    return;

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


#endif /* USE_GL */
