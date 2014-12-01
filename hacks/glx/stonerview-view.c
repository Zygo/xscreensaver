/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)
   http://www.eblong.com/zarf/stonerview.html
 
   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

/* Ported away from GLUT (so that it can do `-root' and work with xscreensaver)
   by Jamie Zawinski <jwz@jwz.org>, 22-Jan-2001.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_COCOA
# include "jwxyz.h"
#elif defined(HAVE_ANDROID)
# include <GLES/gl.h>
#else /* real Xlib */
# include <GL/glx.h>
# include <GL/glu.h>
#endif /* !HAVE_COCOA */

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include <stdlib.h>
#include "stonerview.h"

static GLfloat view_rotx = -45.0, view_roty = 0.0, view_rotz = 15.0;
static GLfloat view_scale = 4.0;


stonerview_state *
stonerview_init_view(int wireframe_p, int transparent_p)
{
  stonerview_state *st = (stonerview_state *) calloc (1, sizeof(*st));

  st->wireframe = wireframe_p;
  st->transparent = transparent_p;
  st->num_els = NUM_ELS;
  st->elist = (stonerview_elem_t *) calloc (st->num_els, sizeof(*st->elist));

  st->osctail = &st->oscroot;

  /* for trackball, two-sided lighting and no face culling */
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_NORMALIZE);

  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_BLEND);

  if (st->transparent)
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);

  return st;
}

/* callback: draw everything */
void
stonerview_win_draw(stonerview_state *st)
{
  int ix;
  static const GLfloat white[] = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat gray[]  = { 0.6, 0.6, 0.6, 1.0 };

  glDrawBuffer(GL_BACK);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glScalef(view_scale, view_scale, view_scale);
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);

  glShadeModel(GL_FLAT);

  for (ix=0; ix < st->num_els; ix++) {
    stonerview_elem_t *el = &st->elist[ix];

    glNormal3f(0.0, 0.0, 1.0);

    /* outline the square */
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (st->wireframe ? white : gray));
    glBegin(GL_LINE_LOOP);
    glVertex3f(el->pos[0]-el->vervec[0], el->pos[1]-el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[1], el->pos[1]-el->vervec[0], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[0], el->pos[1]+el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]-el->vervec[1], el->pos[1]+el->vervec[0], el->pos[2]);
    glEnd();

    if (st->wireframe) continue;

    /* fill the square */
    {
      GLfloat col[4];
      col[0] = el->col[0]; col[1] = el->col[1];
      col[2] = el->col[2]; col[3] = el->col[3];
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    }
    glBegin(GL_QUADS);
    glVertex3f(el->pos[0]-el->vervec[0], el->pos[1]-el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[1], el->pos[1]-el->vervec[0], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[0], el->pos[1]+el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]-el->vervec[1], el->pos[1]+el->vervec[0], el->pos[2]);
    glEnd();
  }

  glPopMatrix();
}

void
stonerview_win_release(stonerview_state *st)
{
  free (st->elist);
  /*free (st->oscroot);  -- #### how do we free this? */
  free (st);
}
