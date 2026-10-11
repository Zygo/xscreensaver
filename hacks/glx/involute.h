/* involute, Copyright (c) 2004-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Utilities for rendering OpenGL gears with involute teeth.
 */

#ifndef __GEAR_INVOLUTE_H__
#define __GEAR_INVOLUTE_H__

typedef struct {
  unsigned long id;          /* unique name */
  double x, y, z;            /* position */
  double r;                  /* radius of the gear, at middle of teeth */
  double th;                 /* rotation (degrees) */

  GLint nteeth;              /* how many teeth */
  double tooth_w, tooth_h;   /* size of teeth */

  double tooth_slope;	     /* 0 for normal right-angle gear; 1 = 45 deg. */
  double inner_r;            /* radius of the (larger) inside hole */
  double inner_r2;           /* radius of the (smaller) inside hole, if any */
  double inner_r3;           /* yet another */

  double thickness;          /* height of the edge */
  double thickness2;         /* height of the (smaller) inside disc if any */
  double thickness3;         /* yet another */
  int spokes;                /* how many spokes inside, if any */
  int nubs;                  /* how many little nubbly bits, if any */
  double spoke_thickness;    /* spoke versus hole */
  double wobble;             /* factory defect! */
  int motion_blur_p;	     /* whether it's spinning too fast to draw */
  int polygons;              /* how many polys in this gear */

  double ratio;              /* gearing ratio with previous gears */
  double rpm;                /* approximate revolutions per minute */

  Bool inverted_p;           /* whether this gear has teeth on the inside */
  Bool base_p;               /* whether this gear begins a new train */
  int coax_p;                /* whether this is one of a pair of bound gears.
                                1 for first, 2 for second. */
  double coax_displacement;  /* distance between gear planes */

  double coax_thickness;     /* thickness of the other gear in the pair */

  enum { INVOLUTE_SMALL, 
         INVOLUTE_MEDIUM, 
         INVOLUTE_LARGE, 
         INVOLUTE_HUGE } size;	/* Controls complexity of mesh. */
  GLfloat color[4];
  GLfloat color2[4];

  GLuint dlist;
} gear;

/* Render one gear, unrotated at 0,0.
   Returns the number of polygons.
 */
extern int draw_involute_gear (gear *g, Bool wire_p);

/* Draws a much simpler representation of a gear.
   Returns the number of polygons.
 */
extern int draw_involute_schematic (gear *g, Bool wire_p);

/* Which of the gear's inside rings is the biggest? 
 */
extern int involute_biggest_ring (gear *g,
                                  double *posP, double *sizeP,
                                  double *heightP);

#endif /* __GEAR_INVOLUTE_H__ */
