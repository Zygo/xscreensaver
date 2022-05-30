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

#include <math.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "yarandom.h"
#include "stonerview.h"


void
stonerview_init_move(stonerview_state *st)
{
  /*st->theta = new_osc_linear(
    new_osc_wrap(0, 36000, 25),
    new_osc_constant(2000));*/
    
  st->theta = new_osc_linear(st,
    new_osc_velowrap(st, 0, 36000, new_osc_multiplex(st,
      new_osc_randphaser(st, 300, 600),
      new_osc_constant(st, 25),
      new_osc_constant(st, 75),
      new_osc_constant(st, 50),
      new_osc_constant(st, 100))
    ),
        
    new_osc_multiplex(st,
      new_osc_buffer(st, new_osc_randphaser(st, 300, 600)),
      new_osc_buffer(st, new_osc_wrap(st, 0, 36000, 10)),
      new_osc_buffer(st, new_osc_wrap(st, 0, 36000, -8)),
      new_osc_wrap(st, 0, 36000, 4),
      new_osc_buffer(st, new_osc_bounce(st, -2000, 2000, 20))
      )
    );
        
  st->rad = new_osc_buffer(st, new_osc_multiplex(st,
    new_osc_randphaser(st, 250, 500),
    new_osc_bounce(st, -1000, 1000, 10),
    new_osc_bounce(st,   200, 1000, -15),
    new_osc_bounce(st,   400, 1000, 10),
    new_osc_bounce(st, -1000, 1000, -20)));
  /*st->rad = new_osc_constant(st, 1000);*/
    
  st->alti = new_osc_linear(st,
    new_osc_constant(st, -1000),
    new_osc_constant(st, 2000 / st->num_els));

  /*st->alti = new_osc_multiplex(
    new_osc_buffer(st, new_osc_randphaser(60, 270)),
    new_osc_buffer(st, new_osc_bounce(-1000, 1000, 48)),
    new_osc_linear(
      new_osc_constant(-1000),
      new_osc_constant(2000 / st->num_els)),
    new_osc_buffer(st, new_osc_bounce(-1000, 1000, 27)),
      new_osc_linear(
      new_osc_constant(-1000),
      new_osc_constant(2000 / st->num_els))
    );*/
    
  /*st->color = new_osc_buffer(st, new_osc_randphaser(5, 35));*/
    
  /*st->color = new_osc_buffer(st, new_osc_multiplex(
    new_osc_randphaser(25, 70),
    new_osc_wrap(0, 3600, 20),
    new_osc_wrap(0, 3600, 30),
    new_osc_wrap(0, 3600, -20),
    new_osc_wrap(0, 3600, 10)));*/
  st->color = new_osc_multiplex(st,
    new_osc_buffer(st, new_osc_randphaser(st, 150, 300)),
    new_osc_buffer(st, new_osc_wrap(st, 0, 3600, 13)),
    new_osc_buffer(st, new_osc_wrap(st, 0, 3600, 32)),
    new_osc_buffer(st, new_osc_wrap(st, 0, 3600, 17)),
    new_osc_buffer(st, new_osc_wrap(st, 0, 3600, 7)));

  stonerview_move_increment(st);
}

/* Set up the list of polygon data for rendering. */
void
stonerview_move_increment(stonerview_state *st)
{
  int ix, val;
/*  double fval; */
/*  double recipels = (1.0 / (double)st->num_els); */
  double pt[2];
  double ptrad, pttheta;
    
  for (ix=0; ix<st->num_els; ix++) {
    stonerview_elem_t *el = &st->elist[ix];
        
    /* Grab r and theta... */
    val = osc_get(st, st->theta, ix);
    pttheta = val * (0.01 * M_PI / 180.0); 
    ptrad = (double)osc_get(st, st->rad, ix) * 0.001;
    /* And convert them to x,y coordinates. */
    pt[0] = ptrad * cos(pttheta);
    pt[1] = ptrad * sin(pttheta);
        
    /* Set x,y,z. */
    el->pos[0] = pt[0];
    el->pos[1] = pt[1];
    el->pos[2] = (double)osc_get(st, st->alti, ix) * 0.001;
        
    /* Set which way the square is rotated. This is fixed for now, although
       it would be trivial to make the squares spin as they revolve. */
    el->vervec[0] = 0.11;
    el->vervec[1] = 0.0;
        
    /* Grab the color, and convert it to RGB values. Technically, we're
       converting an HSV value to RGB, where S and V are always 1. */
    val = osc_get(st, st->color, ix);
    if (val < 1200) {
      el->col[0] = ((double)val / 1200.0);
      el->col[1] = 0;
      el->col[2] = (double)(1200 - val) / 1200.0;
    } 
    else if (val < 2400) {
      el->col[0] = (double)(2400 - val) / 1200.0;
      el->col[1] = ((double)(val - 1200) / 1200.0);
      el->col[2] = 0;
    }
    else {
      el->col[0] = 0;
      el->col[1] = (double)(3600 - val) / 1200.0;
      el->col[2] = ((double)(val - 2400) / 1200.0);
    }
    el->col[3] = 1.0;
  }
    
  osc_increment(st);
}

