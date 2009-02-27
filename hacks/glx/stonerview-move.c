/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>

#include "yarandom.h"
#include "stonerview-osc.h"
#include "stonerview-move.h"

/* The list of polygons. This is filled in by move_increment(), and rendered
   by perform_render(). */
elem_t elist[NUM_ELS];

/* The polygons are controlled by four parameters. Each is represented by
   an osc_t object, which is just something that returns a stream of numbers.
   (Originally the name stood for "oscillator", but it does ever so much more
   now... see osc.c.)
   Imagine a cylinder with a vertical axis (along the Z axis), stretching from
   Z=1 to Z=-1, and a radius of 1.
*/
static osc_t *theta = NULL; /* Angle around the axis. This is expressed in
   hundredths of a degree, so it's actually 0 to 36000. */
static osc_t *rad = NULL; /* Distance from the axis. This goes up to 1000,
   but we actually allow negative distances -- that just goes to the opposite
   side of the circle -- so the range is really -1000 to 1000. */
static osc_t *alti = NULL; /* Height (Z position). This goes from -1000 to 
   1000. */
static osc_t *color = NULL; /* Consider this to be an angle of a circle going 
   around the color wheel. It's in tenths of a degree (consistency is all I 
   ask) so it ranges from 0 to 3600. */
/* static GLint prevtime = 0; / * for timing */

int init_move()
{
  /*theta = new_osc_linear(
    new_osc_wrap(0, 36000, 25),
    new_osc_constant(2000));*/
    
  theta = new_osc_linear(
    new_osc_velowrap(0, 36000, new_osc_multiplex(
      new_osc_randphaser(300, 600),
      new_osc_constant(25),
      new_osc_constant(75),
      new_osc_constant(50),
      new_osc_constant(100))
    ),
        
    new_osc_multiplex(
      new_osc_buffer(new_osc_randphaser(300, 600)),
      new_osc_buffer(new_osc_wrap(0, 36000, 10)),
      new_osc_buffer(new_osc_wrap(0, 36000, -8)),
      new_osc_wrap(0, 36000, 4),
      new_osc_buffer(new_osc_bounce(-2000, 2000, 20))
      )
    );
        
  rad = new_osc_buffer(new_osc_multiplex(
    new_osc_randphaser(250, 500),
    new_osc_bounce(-1000, 1000, 10),
    new_osc_bounce(  200, 1000, -15),
    new_osc_bounce(  400, 1000, 10),
    new_osc_bounce(-1000, 1000, -20)));
  /*rad = new_osc_constant(1000);*/
    
  alti = new_osc_linear(
    new_osc_constant(-1000),
    new_osc_constant(2000 / NUM_ELS));

  /*alti = new_osc_multiplex(
    new_osc_buffer(new_osc_randphaser(60, 270)),
    new_osc_buffer(new_osc_bounce(-1000, 1000, 48)),
    new_osc_linear(
      new_osc_constant(-1000),
      new_osc_constant(2000 / NUM_ELS)),
    new_osc_buffer(new_osc_bounce(-1000, 1000, 27)),
      new_osc_linear(
      new_osc_constant(-1000),
      new_osc_constant(2000 / NUM_ELS))
    );*/
    
  /*color = new_osc_buffer(new_osc_randphaser(5, 35));*/
    
  /*color = new_osc_buffer(new_osc_multiplex(
    new_osc_randphaser(25, 70),
    new_osc_wrap(0, 3600, 20),
    new_osc_wrap(0, 3600, 30),
    new_osc_wrap(0, 3600, -20),
    new_osc_wrap(0, 3600, 10)));*/
  color = new_osc_multiplex(
    new_osc_buffer(new_osc_randphaser(150, 300)),
    new_osc_buffer(new_osc_wrap(0, 3600, 13)),
    new_osc_buffer(new_osc_wrap(0, 3600, 32)),
    new_osc_buffer(new_osc_wrap(0, 3600, 17)),
    new_osc_buffer(new_osc_wrap(0, 3600, 7)));

  move_increment();

  return 1;
}

void final_move()
{
}

/* Set up the list of polygon data for rendering. */
void move_increment()
{
  int ix, val;
/*  GLfloat fval; */
/*  GLfloat recipels = (1.0 / (GLfloat)NUM_ELS); */
  GLfloat pt[2];
  GLfloat ptrad, pttheta;
    
  for (ix=0; ix<NUM_ELS; ix++) {
    elem_t *el = &elist[ix];
        
    /* Grab r and theta... */
    val = osc_get(theta, ix);
    pttheta = val * (0.01 * M_PI / 180.0); 
    ptrad = (GLfloat)osc_get(rad, ix) * 0.001;
    /* And convert them to x,y coordinates. */
    pt[0] = ptrad * cos(pttheta);
    pt[1] = ptrad * sin(pttheta);
        
    /* Set x,y,z. */
    el->pos[0] = pt[0];
    el->pos[1] = pt[1];
    el->pos[2] = (GLfloat)osc_get(alti, ix) * 0.001;
        
    /* Set which way the square is rotated. This is fixed for now, although
       it would be trivial to make the squares spin as they revolve. */
    el->vervec[0] = 0.11;
    el->vervec[1] = 0.0;
        
    /* Grab the color, and convert it to RGB values. Technically, we're
       converting an HSV value to RGB, where S and V are always 1. */
    val = osc_get(color, ix);
    if (val < 1200) {
      el->col[0] = ((GLfloat)val / 1200.0);
      el->col[1] = 0;
      el->col[2] = (GLfloat)(1200 - val) / 1200.0;
    } 
    else if (val < 2400) {
      el->col[0] = (GLfloat)(2400 - val) / 1200.0;
      el->col[1] = ((GLfloat)(val - 1200) / 1200.0);
      el->col[2] = 0;
    }
    else {
      el->col[0] = 0;
      el->col[1] = (GLfloat)(3600 - val) / 1200.0;
      el->col[2] = ((GLfloat)(val - 2400) / 1200.0);
    }
    el->col[3] = 1.0;
  }
    
  osc_increment();
}

