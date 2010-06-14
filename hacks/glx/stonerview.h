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

#ifndef __STONERVIEW_H__
# define __STONERVIEW_H__

#ifdef HAVE_COCOA
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

typedef struct stonerview_state stonerview_state;

#include "stonerview-osc.h"
#include "stonerview-move.h"

struct stonerview_state {

  int wireframe;
  int transparent;

  /* The list of polygons. This is filled in by move_increment(), and rendered
     by perform_render(). */
  int num_els;
  elem_t *elist;

  /* A linked list of all osc_t objects created. New objects are added
     to the end of the list, not the beginning. */
  osc_t *oscroot;
  osc_t **osctail;

  /* The polygons are controlled by four parameters. Each is represented by
     an osc_t object, which is just something that returns a stream of numbers.
     (Originally the name stood for "oscillator", but it does ever so much more
     now... see osc.c.)
     Imagine a cylinder with a vertical axis (along the Z axis), stretching
     from Z=1 to Z=-1, and a radius of 1.
  */
  osc_t *theta; /* Angle around the axis. This is expressed in
                   hundredths of a degree, so it's actually 0 to 36000. */
  osc_t *rad;   /* Distance from the axis. This goes up to 1000,
                   but we actually allow negative distances -- that just 
                   goes to the opposite side of the circle -- so the range
                   is really -1000 to 1000. */
  osc_t *alti;  /* Height (Z position). This goes from -1000 to 1000. */
  osc_t *color; /* Consider this to be an angle of a circle going 
                   around the color wheel. It's in tenths of a degree
                   (consistency is all I ask) so it ranges from 0 to 3600. */
};

#endif /* __STONERVIEW_H__ */
