/* glxfonts, Copyright (c) 2001-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws 2D text over the GL scene, e.g., the FPS displays.
 * Also billboarding.
 * The lower-level character rendering code is in texfont.c.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
/*# include <AGL/agl.h>*/
#else
# include <GL/glx.h>
# include <GL/glu.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "resources.h"
#include "glxfonts.h"
#include "texfont.h"
#include "fps.h"


/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

/* screenhack.h */
extern char *progname;


#undef DEBUG  /* Defining this causes check_gl_error() to be called inside
                 time-critical sections, which could slow things down (since
                 it might result in a round-trip, and stall of the pipeline.)
               */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().

   If width and height are 0, then instead the text is placed
   into the 3D scene at the origin, billboarded to face the
   viewer.
 */
void
print_gl_string (Display *dpy,
                 texture_font_data *data,
                 int window_width, int window_height,
                 GLfloat x, GLfloat y,
                 const char *string,
                 Bool clear_background_p)
{
  Bool in_scene_p = (window_width == 0);

  int line_height = 0;
  int lines = 0;
  const char *c;
  GLfloat color[4];

  Bool tex_p   = glIsEnabled (GL_TEXTURE_2D);
  Bool texs_p  = glIsEnabled (GL_TEXTURE_GEN_S);
  Bool text_p  = glIsEnabled (GL_TEXTURE_GEN_T);
  Bool light_p = glIsEnabled (GL_LIGHTING);
  Bool blend_p = glIsEnabled (GL_BLEND);
  Bool depth_p = glIsEnabled (GL_DEPTH_TEST);
  Bool cull_p  = glIsEnabled (GL_CULL_FACE);
  Bool fog_p   = glIsEnabled (GL_FOG);
  GLint oblend;
  GLint ovp[4];

#  ifndef HAVE_JWZGLES
  GLint opoly[2];
  glGetIntegerv (GL_POLYGON_MODE, opoly);
#  endif

  if (!in_scene_p)
    glGetIntegerv (GL_VIEWPORT, ovp);

  glGetIntegerv (GL_BLEND_DST, &oblend);
  glGetFloatv (GL_CURRENT_COLOR, color);

  for (c = string; *c; c++)
    if (*c == '\n') lines++;

  texture_string_width (data, "m", &line_height);
  y -= line_height;


  glEnable (GL_TEXTURE_2D);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPolygonMode (GL_FRONT, GL_FILL);

  glDisable (GL_TEXTURE_GEN_S);
  glDisable (GL_TEXTURE_GEN_T);
  glDisable (GL_LIGHTING);
  glDisable (GL_CULL_FACE);
  glDisable (GL_FOG);

  if (!in_scene_p)
    glDisable (GL_DEPTH_TEST);

  /* Each matrix mode has its own stack, so we need to push/pop
     them separately.
   */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  {
# ifdef DEBUG
    check_gl_error ("glPushMatrix");
# endif
    if (!in_scene_p)
      glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    {
# ifdef DEBUG
      check_gl_error ("glPushMatrix");
# endif

      if (!in_scene_p)
        {
          int rot = (int) current_device_rotation();
          while (rot <= -180) rot += 360;
          while (rot >   180) rot -= 360;

          glLoadIdentity();
          glViewport (0, 0, window_width, window_height);
          glOrtho (0, window_width, 0, window_height, -1, 1);

          if (rot > 135 || rot < -135)		/* 180 */
            {
              glTranslatef (window_width, window_height, 0);
              glRotatef (180, 0, 0, 1);
            }
          else if (rot > 45)			/* 90 */
            {
              glTranslatef (window_width, 0, 0);
              glRotatef (90, 0, 0, 1);
            }
          else if (rot < -45)			/* 270 */
            {
              glTranslatef(0, window_height, 0);
              glRotatef (-90, 0, 0, 1);
            }
        }

# ifdef DEBUG
      check_gl_error ("glOrtho");
# endif

      /* Let's always dropshadow the FPS and Title text. */
      if (! in_scene_p)
        clear_background_p = True;


      /* draw the text */

      glTranslatef (x, y, 0);

      {
        const XPoint offsets[] = {{ -1, -1 },
                                  { -1,  1 },
                                  {  1,  1 },
                                  {  1, -1 },
                                  {  0,  0 }};
        int i;

        glColor3f (0, 0, 0);
        for (i = 0; i < countof(offsets); i++)
          {
            if (! clear_background_p)
              i = countof(offsets)-1;
            if (offsets[i].x == 0)
              glColor4fv (color);

            glPushMatrix();
            glTranslatef (offsets[i].x, offsets[i].y, 0);
            print_texture_string (data, string);
            glPopMatrix();
          }
      }

# ifdef DEBUG
      check_gl_error ("print_texture_string");
# endif
    }
    glPopMatrix();
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  if (tex_p)   glEnable (GL_TEXTURE_2D); else glDisable (GL_TEXTURE_2D);
  if (texs_p)  glEnable (GL_TEXTURE_GEN_S);/*else glDisable(GL_TEXTURE_GEN_S);*/
  if (text_p)  glEnable (GL_TEXTURE_GEN_T);/*else glDisable(GL_TEXTURE_GEN_T);*/
  if (blend_p) glEnable (GL_BLEND); else glDisable (GL_BLEND);
  if (light_p) glEnable (GL_LIGHTING); /*else glDisable (GL_LIGHTING);*/
  if (depth_p) glEnable (GL_DEPTH_TEST); else glDisable (GL_DEPTH_TEST);
  if (cull_p)  glEnable (GL_CULL_FACE); /*else glDisable (GL_CULL_FACE);*/
  if (fog_p)   glEnable (GL_FOG); /*else glDisable (GL_FOG);*/

  if (!in_scene_p)
    glViewport (ovp[0], ovp[1], ovp[2], ovp[3]);

  glBlendFunc (GL_SRC_ALPHA, oblend);

# ifndef HAVE_JWZGLES
  glPolygonMode (GL_FRONT, opoly[0]);
# endif

  glMatrixMode(GL_MODELVIEW);
}
