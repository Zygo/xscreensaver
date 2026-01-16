/* hopfanimations.h --- Definition of the animations used in the Hopf
   fibration. */
/* Copyright (c) 2025 Carsten Steger <carsten@mirsanmir.org>.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * REVISION HISTORY:
 * C. Steger - 26/02/06: Initial version
 */

#ifndef __HOPFANIMATIONS_H__
#define __HOPFANIMATIONS_H__

#ifdef USE_GL

#include <stdbool.h>

#define M_PI_F 3.1415926535898f

#define ANIM_SINGLE_POINT       0
#define ANIM_SINGLE_TORUS       1
#define ANIM_DOUBLE_TORUS       2
#define ANIM_TRIPLE_TORUS       3
#define ANIM_SINGLE_SEIFERT     4
#define ANIM_TRIPLE_SEIFERT     5
#define ANIM_SINGLE_HOPF_TORUS  6
#define ANIM_SINGLE_HOPF_SPIRAL 7
#define NUM_ANIM_STATES         (ANIM_SINGLE_HOPF_SPIRAL+1)

#define MAX_ANIM_GEOM 10

#define GEN_TORUS     0
#define GEN_SPIRAL    1

#define EASING_NONE  0
#define EASING_CUBIC 1
#define EASING_SIN   2
#define EASING_COS   3
#define EASING_LIN   4
#define EASING_ACCEL 5
#define EASING_DECEL 6


/* A data structure that holds the data for the animation state of a single
   object. */
typedef struct {
  int generator;
  float p;
  float q;
  float r;
  float offset;
  float sector;
  int n;
  int num;
  bool rotate;
  float quat_base[4];
} animation_geometry;

/* A data structure that holds the data for an animation using a single
   object. */
typedef struct {
  int generator;
  float p_start, p_end;
  int easing_function_p;
  float q_start, q_end;
  int easing_function_q;
  float r_start, r_end;
  int easing_function_r;
  float offset_start, offset_end;
  int easing_function_offset;
  float sector_start, sector_end;
  int easing_function_sector;
  int n;
  int num;
  float rot_axis_base[3];
  float angle_start, angle_end;
  int easing_function_rotate;
} animation_single_obj;

/* A data structure that holds the data for an animation using multiple
   objects that are animated at the same time. */
typedef struct {
  int num;
  animation_single_obj *anim_so;
  float rotate_prob;
  int easing_function_rot_rnd;
  float rot_axis_space[3];
  float angle_start, angle_end;
  int easing_function_rot_space;
  int num_steps;
} animation_multi_obj;

/* A data structure that holds the data for an animation that consists of
   multiple phases that are animated consecutively. */
typedef struct {
  int num_phases;
  animation_multi_obj **anim_mo;
} animation_phases;

/* A data structure that holds a set of related animations. */
typedef struct {
  int num_anim;
  animation_phases **anim;
} animations;


/* The set of all possible animations. */
extern animations *hopf_animations[NUM_ANIM_STATES][NUM_ANIM_STATES];


#endif /* USE_GL */

#endif /* __HOPFANIMATIONS_H__ */
